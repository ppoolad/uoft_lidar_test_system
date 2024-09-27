# Variables
CC = gcc
CXX = g++
LDFLAGS = -lgpiod

# Directories
SRC_DIR = src
DATA_COLLECTOR_DIR = $(SRC_DIR)/data_collector
DATA_ANALYSIS_DIR = $(SRC_DIR)/data_analysis
INCLUDE_DIR = include

CFLAGS = -I$(INCLUDE_DIR)
CXXFLAGS = -I$(INCLUDE_DIR)

# Output directories
BUILD_DIR = build
DEBUG_DIR = $(BUILD_DIR)/debug
RELEASE_DIR = $(BUILD_DIR)/release

# Source files
DATA_COLLECTOR_SRCS = $(DATA_COLLECTOR_DIR)/tdc_test.c $(DATA_COLLECTOR_DIR)/asic_control.c
TDC_START_SRC = $(DATA_COLLECTOR_DIR)/tdc_start.c $(DATA_COLLECTOR_DIR)/asic_control.c
DATA_ANALYSIS_SRCS = $(wildcard $(DATA_ANALYSIS_DIR)/*.cpp)

# Object files
DATA_COLLECTOR_OBJS = $(patsubst $(DATA_COLLECTOR_DIR)/%.c, $(DEBUG_DIR)/%.o, $(DATA_COLLECTOR_SRCS))
TDC_START_OBJ = $(patsubst $(DATA_COLLECTOR_DIR)/%.c, $(DEBUG_DIR)/%.o, $(TDC_START_SRC))
DATA_ANALYSIS_OBJS = $(patsubst $(DATA_ANALYSIS_DIR)/%.cpp, $(DEBUG_DIR)/%.o, $(DATA_ANALYSIS_SRCS))

# Targets
TARGETS = $(DEBUG_DIR)/tdc_test $(DEBUG_DIR)/tdc_start $(DEBUG_DIR)/data_analysis

# Rules
all: debug release

debug: CFLAGS += -g
debug: CXXFLAGS += -g
debug: $(TARGETS)

release: CFLAGS += -O2
release: CXXFLAGS += -O2
release: $(patsubst $(DEBUG_DIR)/%, $(RELEASE_DIR)/%, $(TARGETS))

$(DEBUG_DIR)/%.o: $(DATA_COLLECTOR_DIR)/%.c
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(DEBUG_DIR)/%.o: $(DATA_ANALYSIS_DIR)/%.cpp
	@mkdir -p $(DEBUG_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(DEBUG_DIR)/%.o: $(TDC_START_OBJ)/%.c
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(DEBUG_DIR)/tdc_test: $(DATA_COLLECTOR_OBJS)
	$(CC) $(CFLAGS) $(DATA_COLLECTOR_OBJS) -o $@ $(LDFLAGS)

$(DEBUG_DIR)/tdc_start: $(TDC_START_OBJ)
	$(CC) $(CFLAGS) $(TDC_START_OBJ) -o $@ $(LDFLAGS)

$(DEBUG_DIR)/data_analysis: $(DATA_ANALYSIS_OBJS)
	$(CXX) $(CXXFLAGS) $(DATA_ANALYSIS_OBJS) -o $@ $(LDFLAGS)

$(RELEASE_DIR)/%: $(DEBUG_DIR)/%
	@mkdir -p $(RELEASE_DIR)
	cp $< $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all debug release clean