CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -Os -ffunction-sections -fdata-sections -MMD -MP
BINNAME = dscleaner

# Platform-specific linker flags:
#   - macOS (Apple ld64): -dead_strip + CoreServices (FSEvents) and
#     DiskArbitration (hardware mount/unmount events).
#   - Linux/others (GNU ld): --gc-sections.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	LDFLAGS = -s -Wl,-dead_strip -framework CoreServices -framework DiskArbitration
else
	LDFLAGS = -s -Wl,--gc-sections
endif

SRCDIR = src
OBJDIR = obj
BINDIR = bin

SOURCES = $(shell find $(SRCDIR) -name '*.cpp')
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))
DEPS = $(OBJECTS:.o=.d)
HEADERS = $(shell find include -name '*.hpp')
EXECUTABLE = $(BINDIR)/$(BINNAME)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $^
	@ln -sf $(BINNAME) $(BINDIR)/dc   # short alias: `dc`

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Apply the Google C++ style (.clang-format) in place.
format:
	clang-format -i $(SOURCES) $(HEADERS)

# Static analysis against the project's .clang-tidy baseline.
lint:
	clang-tidy $(SOURCES) -- $(CXXFLAGS)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Pull in auto-generated header dependencies so edits to .hpp trigger rebuilds.
-include $(DEPS)

.PHONY: all clean format lint
