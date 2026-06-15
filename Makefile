# Top-level convenience Makefile. The C++ core lives in core/; the macOS menu
# bar GUI (SwiftUI) lives in gui/ and is built with the Swift toolchain.
.PHONY: all clean format lint gui

# Build the C++ core (the dscleaner CLI + engine).
all:
	$(MAKE) -C core

clean:
	$(MAKE) -C core clean

format:
	$(MAKE) -C core format

lint:
	$(MAKE) -C core lint

# Build the macOS menu bar app (requires macOS 13+ and the Swift toolchain).
gui:
	cd gui && swift build -c release
