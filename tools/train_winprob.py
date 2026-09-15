#!/usr/bin/env python3
"""Fit the live win-probability model and emit everything the C++ test bench needs.

Model (home-team win probability, logistic):
    f  = fraction of regulation time remaining (0..1; overtime counts as the OT clock / 48 min)
    s  = sqrt(f + eps)
    logit p = a + b_ms * (margin / s) + b_s * s + b_m * margin        (margin = home - away)
The margin/sqrt(time) term is the classic Brownian-motion view of a basketball game
(Stern 1994); the sqrt(time) term carries home-court drift; the raw margin term is a small
correction. Four floats, nothing the ESP32 cannot do in one expf.

Training plays are binned to (margin, 5-second bucket) cells with win/loss counts, which is the
same weighted fit as the raw rows to within the 5 s quantisation and shrinks 624k plays to ~35k
cells (211 KB). The ESP32 embeds that table and refits the model itself on boot with the C++
trainer in firmware/src/winprob_train.h; the native test bench checks the two trainers agree.

Train on one season, evaluate on the next (time split), and write:
    firmware/data/winprob_train.bin          binned training table (embedded in the firmware image)
    firmware/src/winprob_coef.h              coefficients for the firmware
    firmware/test/fixtures/winprob_cases.h   ~300 sampled plays with the Python probability (parity test)
    firmware/test/fixtures/winprob_eval.bin  every play of the held-out season, 5 bytes each (C++ replay bench)
    docs/winprob.md                          metrics and calibration on the whole held-out season
Pure Python (no numpy on the build Mac); the fit takes a couple of minutes.

    python3 tools/train_winprob.py --train 2025 --test 2026
"""
import argparse
import gzip
import json
import math
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REG_SECS = 2880.0
EPS_GRID = [0.002, 0.005, 0.01, 0.02]
BUCKET_SECS = 5


def load(season):
    """Rows of (margin, frac_left, home_won, espn_wp, game) with ESPN's in-feed score corrections dropped."""
    rows, maxes = [], {}
    with gzip.open(ROOT / "data" / "pbp" / f"{season}.jsonl.gz", "rt") as f:
        for line in f:
            r = json.loads(line)
            mh, ma = maxes.get(r["game"], (0, 0))
            if r["home_score"] < mh or r["away_score"] < ma:
                continue  # scoring correction in progress; skip the corrected-away rows
            maxes[r["game"]] = (max(mh, r["home_score"]), max(ma, r["away_score"]))
            if r["period"] <= 4:
                frac = r["secs_left_reg"] / REG_SECS
            else:
                frac = r["secs_left"] / REG_SECS
            rows.append((r["home_score"] - r["away_score"], frac, 1.0 if r["home_won"] else 0.0, r["espn_wp"], r["game"]))
    return rows


def features(margin, frac, eps):
    s = math.sqrt(frac + eps)
    return (1.0, margin / s, s, float(margin))


def predict(coef, margin, frac, eps):
    z = sum(c * x for c, x in zip(coef, features(margin, frac, eps)))
    z = max(-30.0, min(30.0, z))
    return 1.0 / (1.0 + math.exp(-z))


def solve4(A, b):
    """Gaussian elimination with partial pivoting for the 4x4 Newton step."""
    n = len(b)
    M = [row[:] + [b[i]] for i, row in enumerate(A)]
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(M[r][c]))
        M[c], M[p] = M[p], M[c]
        for r in range(c + 1, n):
            k = M[r][c] / M[c][c]
            for j in range(c, n + 1):
                M[r][j] -= k * M[c][j]
    x = [0.0] * n
    for r in range(n - 1, -1, -1):
        x[r] = (M[r][n] - sum(M[r][j] * x[j] for j in range(r + 1, n))) / M[r][r]
    return x


def bin_rows(rows):
    """(margin, bucket) -> [wins, losses]; margin clamped to int8, time to BUCKET_SECS."""
    cells = {}
    for m, f, y, _, _ in rows:
        key = (max(-127, min(127, m)), int(round(f * REG_SECS / BUCKET_SECS)))
        c = cells.setdefault(key, [0, 0])
        c[0 if y else 1] += 1
    return cells


def write_table(cells, path):
    """4-byte header 'WP', bucket seconds, version 1; then 7-byte little-endian records
    int8 margin, uint16 bucket, uint16 wins, uint16 losses. Mirrors winprob_train.h."""
    import struct
    with open(path, "wb") as f:
        f.write(struct.pack("<2sBB", b"WP", BUCKET_SECS, 1))
        for (m, b), (w, l) in sorted(cells.items()):
            assert w < 65536 and l < 65536
            f.write(struct.pack("<bHHH", m, b, w, l))


def fit(cells, eps, iters=12, ridge=1e-6):
    """Weighted logistic regression by Newton-Raphson (IRLS) on the binned cells.
    Same arithmetic as winProbTrain() in firmware/src/winprob_train.h."""
    X = [(features(m, b * BUCKET_SECS / REG_SECS, eps), w, w + l) for (m, b), (w, l) in sorted(cells.items())]
    coef = [0.0, 0.0, 0.0, 0.0]
    for _ in range(iters):
        g = [0.0] * 4
        H = [[0.0] * 4 for _ in range(4)]
        for x, wins, n in X:
            z = coef[0] * x[0] + coef[1] * x[1] + coef[2] * x[2] + coef[3] * x[3]
            z = max(-30.0, min(30.0, z))
            p = 1.0 / (1.0 + math.exp(-z))
            w = n * p * (1.0 - p)
            d = wins - n * p
            for i in range(4):
                g[i] += x[i] * d
                Hi = H[i]
                xi_w = x[i] * w
                for j in range(4):
                    Hi[j] += xi_w * x[j]
        for i in range(4):
            H[i][i] += ridge
            g[i] -= ridge * coef[i]
        step = solve4(H, g)
        coef = [c + s for c, s in zip(coef, step)]
        if max(abs(s) for s in step) < 1e-7:
            break
    return coef


def metrics(pairs):
    """pairs of (p, y) -> dict of log loss, Brier, accuracy."""
    n = len(pairs)
    ll = -sum(y * math.log(max(p, 1e-12)) + (1 - y) * math.log(max(1 - p, 1e-12)) for p, y in pairs) / n
    brier = sum((p - y) ** 2 for p, y in pairs) / n
    acc = sum((p >= 0.5) == (y == 1.0) for p, y in pairs) / n
    return {"n": n, "logloss": ll, "brier": brier, "accuracy": acc}


def calibration(pairs, bins=10):
    out = []
    for b in range(bins):
        lo, hi = b / bins, (b + 1) / bins
        sel = [(p, y) for p, y in pairs if lo <= p < hi or (b == bins - 1 and p == 1.0)]
        if sel:
            out.append((lo, hi, len(sel), sum(p for p, _ in sel) / len(sel), sum(y for _, y in sel) / len(sel)))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--train", type=int, default=2025)
    ap.add_argument("--test", type=int, default=2026)
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()
    rng = random.Random(args.seed)

    train = load(args.train)
    test = load(args.test)
    print(f"train {args.train}: {len(train)} plays; test {args.test}: {len(test)} plays")

    cells = bin_rows(train)
    table = ROOT / "firmware" / "data" / "winprob_train.bin"
    table.parent.mkdir(exist_ok=True)
    write_table(cells, table)
    print(f"binned to {len(cells)} cells of {BUCKET_SECS} s -> {table.relative_to(ROOT)} ({table.stat().st_size / 1e3:.0f} KB)")

    # pick eps by training log loss (on the raw plays), then fit at that eps
    best = None
    for eps in EPS_GRID:
        c = fit(cells, eps)
        m = metrics([(predict(c, mg, fr, eps), y) for mg, fr, y, _, _ in train])
        print(f"  eps={eps}: train logloss {m['logloss']:.5f}")
        if best is None or m["logloss"] < best[0]:
            best = (m["logloss"], eps, c)
    _, eps, coef = best
    print(f"eps={eps} coef a={coef[0]:.6f} b_ms={coef[1]:.6f} b_s={coef[2]:.6f} b_m={coef[3]:.6f}")

    ours = [(predict(coef, mg, fr, eps), y) for mg, fr, y, _, _ in test]
    espn = [(wp, y) for _, _, y, wp, _ in test if wp is not None]
    m_ours, m_espn = metrics(ours), metrics(espn)
    print("held-out ours:", {k: round(v, 4) for k, v in m_ours.items()})
    print("held-out espn:", {k: round(v, 4) for k, v in m_espn.items()})

    # --- firmware coefficients
    (ROOT / "firmware" / "src" / "winprob_coef.h").write_text(
        "// Generated by tools/train_winprob.py; do not edit.\n"
        f"// Trained on the {args.train - 1}-{args.train % 100:02d} season, held out {args.test - 1}-{args.test % 100:02d}: "
        f"Brier {m_ours['brier']:.4f} (ESPN {m_espn['brier']:.4f}), log loss {m_ours['logloss']:.4f} (ESPN {m_espn['logloss']:.4f}).\n"
        "#pragma once\n"
        "#define WINPROB_A     " + f"{coef[0]:.7f}f\n"
        "#define WINPROB_B_MS  " + f"{coef[1]:.7f}f\n"
        "#define WINPROB_B_S   " + f"{coef[2]:.7f}f\n"
        "#define WINPROB_B_M   " + f"{coef[3]:.7f}f\n"
        "#define WINPROB_EPS   " + f"{eps:.7f}f\n"
        f"#define WINPROB_TRAIN_CELLS {len(cells)}\n"
        f"#define WINPROB_TRAIN_PLAYS {len(train)}\n"
    )

    # --- parity cases: sampled held-out plays with the Python probability, as period/clock the firmware sees
    fx = ROOT / "firmware" / "test" / "fixtures"
    cases = rng.sample(range(len(test)), 300)
    lines = ["// Generated by tools/train_winprob.py; do not edit.",
             "// {home_margin, period, secs_left_in_period, python_home_win_probability}",
             "struct WinProbCase { int margin; int period; float secsLeft; float p; };",
             "static const WinProbCase WINPROB_CASES[] = {"]
    for i in cases:
        mg, fr, y, _, _ = test[i]
        secs = fr * REG_SECS                      # express as a regulation period + clock
        period = max(1, min(4, 4 - int(secs // 720)))
        secs_in_period = secs - (4 - period) * 720
        lines.append(f"  {{{mg}, {period}, {secs_in_period:.1f}f, {predict(coef, mg, fr, eps):.6f}f}},")
    lines += ["};", ""]
    (fx / "winprob_cases.h").write_text("\n".join(lines))

    # --- replay fixture: every play of the held-out season, little-endian, 5 bytes per row:
    #   int8  home margin
    #   uint16 frac_left * 10000, bit 15 set when the home team won
    #   uint16 espn_wp * 1000 (0xFFFF when missing), bit 14 set on the first play of a game
    games = sorted({r[4] for r in test})
    import struct
    last = None
    with open(fx / "winprob_eval.bin", "wb") as f:
        for mg, fr, y, wp, g in test:
            fbits = int(round(fr * 10000)) | (0x8000 if y else 0)
            ebits = 0xFFFF if wp is None else int(round(wp * 1000)) | (0x4000 if g != last else 0)
            f.write(struct.pack("<bHH", max(-127, min(127, mg)), fbits, ebits))
            last = g

    # --- docs
    cal = calibration(ours)
    quarters = []
    for lo, hi, name in [(0.75, 1.01, "1st"), (0.5, 0.75, "2nd"), (0.25, 0.5, "3rd"), (0.0, 0.25, "4th + OT")]:
        sel = [(mg, fr, y, wp) for mg, fr, y, wp, _ in test if lo <= fr < hi and wp is not None]
        o = metrics([(predict(coef, mg, fr, eps), y) for mg, fr, y, _ in sel])
        e = metrics([(wp, y) for _, _, y, wp in sel])
        quarters.append((name, o["n"], o["brier"], e["brier"]))
    prior = metrics([(predict(coef, 0, 1.0, eps), y) for _, _, y, _, _ in test])
    doc = [f"# Live win probability: model and bench",
           "",
           f"Fitted by `tools/train_winprob.py` on every play of the {args.train - 1}-{args.train % 100:02d} season "
           f"({len(train):,} plays after dropping ESPN's in-feed scoring corrections, binned to {len(cells):,} "
           f"(margin, {BUCKET_SECS} s) cells with win/loss counts) and evaluated on every play of "
           f"{args.test - 1}-{args.test % 100:02d} ({len(test):,} plays, {len(games):,} games). "
           "ESPN's own in-game win probability, recorded on the same plays, is the benchmark.",
           "",
           "## Model",
           "",
           "```",
           "f = fraction of regulation remaining, s = sqrt(f + eps), margin = home - away",
           "logit P(home wins) = a + b_ms * margin / s + b_s * s + b_m * margin",
           "```",
           "",
           "| a | b_ms | b_s | b_m | eps |",
           "|---|---|---|---|---|",
           f"| {coef[0]:.4f} | {coef[1]:.4f} | {coef[2]:.4f} | {coef[3]:.4f} | {eps} |",
           "",
           f"Four coefficients in `firmware/src/winprob_coef.h`; inference is `winProbHome()` in "
           "`firmware/src/winprob.h`, one `expf` per poll.",
           "",
           "## The board trains itself",
           "",
           f"The binned table (`firmware/data/winprob_train.bin`, {table.stat().st_size / 1e3:.0f} KB) is embedded in the "
           "firmware image, and `winProbTrain()` in `firmware/src/winprob_train.h` is the same Newton-Raphson fit "
           "as the Python trainer, header-only C++ with no dependencies. On boot the ESP32 refits the model from the "
           "table and reports the play count and time on the splash screen; the native bench "
           "(`test/test_winprob_train`) runs that exact code on the host and checks it lands on the coefficients "
           "above. The Python script is only needed to refresh the data.",
           "",
           f"## Held-out season {args.test - 1}-{args.test % 100:02d}",
           "",
           "| | plays | log loss | Brier | accuracy |",
           "|---|---|---|---|---|",
           f"| this model | {m_ours['n']:,} | {m_ours['logloss']:.4f} | {m_ours['brier']:.4f} | {m_ours['accuracy']:.3f} |",
           f"| ESPN | {m_espn['n']:,} | {m_espn['logloss']:.4f} | {m_espn['brier']:.4f} | {m_espn['accuracy']:.3f} |",
           f"| home-court prior only | {prior['n']:,} | {prior['logloss']:.4f} | {prior['brier']:.4f} | {prior['accuracy']:.3f} |",
           "",
           "Where the gap to ESPN comes from, by quarter (Brier, lower is better):",
           "",
           "| quarter | plays | this model | ESPN | gap |",
           "|---|---|---|---|---|"]
    doc += [f"| {name} | {n:,} | {ob:.4f} | {eb:.4f} | {ob - eb:+.4f} |" for name, n, ob, eb in quarters]
    doc += ["",
           "ESPN's model also knows team strength (pregame odds) and who has the ball, which is worth about "
           "three Brier points in the first quarter and nothing by the fourth, when the scoreboard decides. "
           "The obvious next feature is a team-strength prior from the win-loss records; the pregame response "
           "carries neither team's record, so it would cost one extra request per game to the opponent's team endpoint.",
           "",
           "Calibration of this model (predicted home win probability vs. how often the home team actually won):",
           "",
           "| bin | plays | mean predicted | observed |",
           "|---|---|---|---|"]
    doc += [f"| {lo:.1f}–{hi:.1f} | {n:,} | {mp:.3f} | {ob:.3f} |" for lo, hi, n, mp, ob in cal]
    doc += ["",
            "## C++ bench",
            "",
            "`firmware/test/test_winprob` runs on the host with `pio test -e native -f test_winprob` (also in CI). It checks:",
            "",
            "- the ESP32 inference reproduces the Python probability on 300 sampled plays (parity),",
            "- symmetry, monotonicity and end-of-game behaviour of the function,",
            "- the C++ trainer (`test/test_winprob_train`) refits from the embedded table and agrees with Python "
            "on every prediction within 0.001, and recovers known coefficients from synthetic data,",
            f"- a replay of the whole held-out season ({len(test):,} plays in {len(games):,} games, "
            "`test/fixtures/winprob_eval.bin`, 5 bytes per play) through the firmware function. The C++ code must "
            "reproduce the table above: within 0.02 Brier of ESPN over the game and within 0.005 in the fourth quarter, "
            "at least 0.05 better than the home-court prior, and calibrated within 4 points in every bin.",
            "",
            "Regenerate everything with `python3 tools/train_winprob.py` after refreshing `data/` "
            "(see `data/README.md`).",
            ""]
    (ROOT / "docs" / "winprob.md").write_text("\n".join(doc))
    print("wrote firmware/src/winprob_coef.h, test fixtures, docs/winprob.md")


if __name__ == "__main__":
    main()
