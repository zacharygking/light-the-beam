# Export STLs from the OpenSCAD sources.
#   brew install --cask openscad   (or download from openscad.org)
#   make
OPENSCAD ?= $(shell command -v openscad 2>/dev/null || echo /Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD)
OUT = exports

TARGETS = $(OUT)/plinth.stl $(OUT)/plinth_lid.stl $(OUT)/plinth_test.stl \
          $(OUT)/beam_tube.stl $(OUT)/beam_spine.stl $(OUT)/beam_socket.stl

all: $(TARGETS)

$(OUT):
	mkdir -p $(OUT)

$(OUT)/plinth.stl: plinth.scad | $(OUT)
	$(OPENSCAD) -o $@ -D 'part="body"' $<
$(OUT)/plinth_lid.stl: plinth.scad | $(OUT)
	$(OPENSCAD) -o $@ -D 'part="lid"' $<
$(OUT)/plinth_test.stl: plinth.scad | $(OUT)
	$(OPENSCAD) -o $@ -D 'part="test"' $<
$(OUT)/beam_tube.stl: beam_tube.scad | $(OUT)
	$(OPENSCAD) -o $@ $<
$(OUT)/beam_spine.stl: strip_holder.scad | $(OUT)
	$(OPENSCAD) -o $@ -D 'part="spine"' $<
$(OUT)/beam_socket.stl: strip_holder.scad | $(OUT)
	$(OPENSCAD) -o $@ -D 'part="socket"' $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
