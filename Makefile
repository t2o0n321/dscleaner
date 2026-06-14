CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -Os -ffunction-sections -fdata-sections
BINNAME = dscleaner

# Platform-specific linker flags:
#   - macOS (Apple ld64): -dead_strip + CoreServices for FSEvents.
#   - Linux/others (GNU ld): --gc-sections.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	LDFLAGS = -s -Wl,-dead_strip -framework CoreServices
else
	LDFLAGS = -s -Wl,--gc-sections
endif

SRCDIR = src
OBJDIR = obj
BINDIR = bin

SOURCES = $(shell find $(SRCDIR) -name '*.cpp')
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))
EXECUTABLE = $(BINDIR)/$(BINNAME)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean
