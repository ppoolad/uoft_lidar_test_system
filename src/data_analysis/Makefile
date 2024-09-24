# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -Wextra -std=c++11

# Source file
SRC = data_analysis.cpp

# Output binaries
DEBUG_BIN = data_analysis_debug
RELEASE_BIN = data_analysis_release

# Build all
all: debug release
# Build debug version
debug: CXXFLAGS += -g
debug: $(SRC)
	$(CXX) $(CXXFLAGS) -o $(DEBUG_BIN) $(SRC)

# Build release version
release: CXXFLAGS += -O2
release: $(SRC)
	$(CXX) $(CXXFLAGS) -o $(RELEASE_BIN) $(SRC)

# Clean up build artifacts
clean:
	rm -f $(DEBUG_BIN) $(RELEASE_BIN)



.PHONY: debug release clean