# Live win probability: model and bench

Fitted by `tools/train_winprob.py` on every play of the 2024-25 season (623,853 plays after dropping ESPN's in-feed scoring corrections, binned to 35,221 (margin, 5 s) cells with win/loss counts) and evaluated on every play of 2025-26 (641,199 plays, 1,322 games). ESPN's own in-game win probability, recorded on the same plays, is the benchmark.

## Model

```
f = fraction of regulation remaining, s = sqrt(f + eps), margin = home - away
logit P(home wins) = a + b_ms * margin / s + b_s * s + b_m * margin
```

| a | b_ms | b_s | b_m | eps |
|---|---|---|---|---|
| -0.0137 | 0.0956 | 0.1844 | -0.0111 | 0.005 |

Four coefficients in `firmware/src/winprob_coef.h`; inference is `winProbHome()` in `firmware/src/winprob.h`, one `expf` per poll.

## The board trains itself

The binned table (`firmware/data/winprob_train.bin`, 247 KB) is embedded in the firmware image, and `winProbTrain()` in `firmware/src/winprob_train.h` is the same Newton-Raphson fit as the Python trainer, header-only C++ with no dependencies. On boot the ESP32 refits the model from the table and reports the play count and time on the splash screen; the native bench (`test/test_winprob_train`) runs that exact code on the host and checks it lands on the coefficients above. The Python script is only needed to refresh the data.

## Held-out season 2025-26

| | plays | log loss | Brier | accuracy |
|---|---|---|---|---|
| this model | 641,199 | 0.4764 | 0.1617 | 0.747 |
| ESPN | 640,773 | 0.4418 | 0.1474 | 0.778 |
| home-court prior only | 641,199 | 0.6873 | 0.2471 | 0.556 |

Where the gap to ESPN comes from, by quarter (Brier, lower is better):

| quarter | plays | this model | ESPN | gap |
|---|---|---|---|---|
| 1st | 156,069 | 0.2289 | 0.1973 | +0.0315 |
| 2nd | 162,919 | 0.1918 | 0.1740 | +0.0178 |
| 3rd | 158,810 | 0.1455 | 0.1380 | +0.0075 |
| 4th + OT | 162,975 | 0.0833 | 0.0823 | +0.0010 |

ESPN's model also knows team strength (pregame odds) and who has the ball, which is worth about three Brier points in the first quarter and nothing by the fourth, when the scoreboard decides. The obvious next feature is a team-strength prior from the win-loss records; the pregame response carries neither team's record, so it would cost one extra request per game to the opponent's team endpoint.

Calibration of this model (predicted home win probability vs. how often the home team actually won):

| bin | plays | mean predicted | observed |
|---|---|---|---|
| 0.0–0.1 | 66,154 | 0.032 | 0.030 |
| 0.1–0.2 | 35,027 | 0.150 | 0.151 |
| 0.2–0.3 | 41,196 | 0.252 | 0.267 |
| 0.3–0.4 | 53,077 | 0.352 | 0.380 |
| 0.4–0.5 | 73,068 | 0.454 | 0.470 |
| 0.5–0.6 | 90,592 | 0.550 | 0.575 |
| 0.6–0.7 | 79,067 | 0.647 | 0.655 |
| 0.7–0.8 | 58,745 | 0.748 | 0.760 |
| 0.8–0.9 | 52,327 | 0.850 | 0.855 |
| 0.9–1.0 | 91,946 | 0.969 | 0.981 |

## Accuracy, and why it is not the headline

Accuracy here means: at every play, call the winner as whichever side is above 50%. It is easy to read but it scores a 51% and a 99% the same, so it cannot tell an honest 65% from an overconfident one. The pill's actual claim is "in situations like this one, the Kings win about this often", and the calibration table above is the test of that claim. Accuracy is reported for completeness:

| phase | plays | this model | ESPN | always pick home |
|---|---|---|---|---|
| tip-off | 1,339 | 0.557 | 0.688 | 0.558 |
| 1st quarter | 154,850 | 0.620 | 0.694 | 0.557 |
| 2nd | 163,030 | 0.702 | 0.733 | 0.555 |
| 3rd | 158,913 | 0.784 | 0.797 | 0.555 |
| 4th + OT | 163,067 | 0.880 | 0.883 | 0.555 |
| last minute | 22,934 | 0.939 | 0.939 | 0.551 |
| all plays | 641,199 | 0.747 | 0.778 | 0.556 |
| within 3 points, under 5 minutes | 17,575 | 0.716 | 0.715 | |
| SAC games only | 38,877 | 0.766 | 0.815 | |

At tip-off the model is exactly the always-home coin flip, because it knows nothing about the teams; ESPN's edge there is pregame odds. By the fourth quarter, and in close late-game situations, the two are indistinguishable. The SAC gap is wider than average for the same reason: a team's record is a strong pregame prior that ESPN has and this model does not.

## SAC games: what the pill would have shown

The pill shows P(SAC wins), so this restricts the held-out season to 82 SAC games (38,877 plays), flips the home probability when SAC is away, and calibrates that number (Brier 0.1524, log loss 0.4526). In toss-up situations (predicted 30–70%) the pill would have overstated SAC's chances by 19 points on average. That is systematic, not noise: SAC went 22-60, and the model starts every game at the league-average home prior. Late in games (the 0.8+ bins) the scoreboard takes over and the pill is honest again. This is the case for the team-strength prior below.

| bin | plays | mean predicted | SAC actually won |
|---|---|---|---|
| 0.0–0.1 | 8,174 | 0.029 | 0.020 |
| 0.1–0.2 | 3,762 | 0.150 | 0.090 |
| 0.2–0.3 | 4,104 | 0.250 | 0.173 |
| 0.3–0.4 | 4,675 | 0.352 | 0.181 |
| 0.4–0.5 | 5,350 | 0.450 | 0.253 |
| 0.5–0.6 | 4,663 | 0.547 | 0.341 |
| 0.6–0.7 | 3,072 | 0.644 | 0.449 |
| 0.7–0.8 | 1,747 | 0.749 | 0.653 |
| 0.8–0.9 | 1,649 | 0.848 | 0.834 |
| 0.9–1.0 | 1,681 | 0.960 | 0.992 |

## Not yet measured

- **Jitter between polls.** The board refreshes every 20 seconds, not every play. A replay at 20-second sampling would show how much the pill moves between refreshes, which is a display question rather than a model question, and decides whether the number needs smoothing or rounding to 5%.
- **A team-strength prior.** The SAC-only calibration above and the first two rows of the accuracy table are the same gap seen twice: the model starts every game at the league-average home prior. Win-loss records would recover most of it, at the cost of one extra request per game for the opponent's record (the pregame response carries neither team's). This is the first thing to add once the pill is on screen.

## C++ bench

`firmware/test/test_winprob` runs on the host with `pio test -e native -f test_winprob` (also in CI). It checks:

- the ESP32 inference reproduces the Python probability on 300 sampled plays (parity),
- symmetry, monotonicity and end-of-game behaviour of the function,
- the C++ trainer (`test/test_winprob_train`) refits from the embedded table and agrees with Python on every prediction within 0.001, and recovers known coefficients from synthetic data,
- a replay of the whole held-out season (641,199 plays in 1,322 games, `test/fixtures/winprob_eval.bin`, 5 bytes per play) through the firmware function. The C++ code must reproduce the table above: within 0.02 Brier of ESPN over the game and within 0.005 in the fourth quarter, at least 0.05 better than the home-court prior, and calibrated within 4 points in every bin.

Regenerate everything with `python3 tools/train_winprob.py` after refreshing `data/` (see `data/README.md`).
