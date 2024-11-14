# Variables
CC = gcc
CXX = g++
LDFLAGS = -lgpiod -lm -lpthread

# Directories
SRC_DIR = src
DATA_COLLECTOR_DIR = $(SRC_DIR)/data_collector
DATA_ANALYSIS_DIR = $(SRC_DIR)/data_analysis
DSP_DIR = $(SRC_DIR)/dsp_test
ASIC_CONTROL_DIR = $(SRC_DIR)/asic_control
SIMPLE_RX_DIR = $(SRC_DIR)/simple_rx
CONF_DIR = $(SRC_DIR)/conf
INCLUDE_DIR = include

CFLAGS = -I$(INCLUDE_DIR)
CXXFLAGS = -I$(INCLUDE_DIR)

# Output directories
BUILD_DIR = build
DEBUG_DIR = $(BUILD_DIR)/debug
RELEASE_DIR = $(BUILD_DIR)/release
SHARED_DIR = $(BUILD_DIR)/shared

# Source files
ASIC_CONTROL_SRCS = $(wildcard $(ASIC_CONTROL_DIR)/*.c)
SIMPLE_RX_SRCS = $(SIMPLE_RX_DIR)/simple_rx.c
CONF_SRCS = $(CONF_DIR)/conf.c
DATA_COLLECTOR_SRCS = $(DATA_COLLECTOR_DIR)/tdc_test.c
TDC_START_SRC = $(DATA_COLLECTOR_DIR)/tdc_start.c
DSP_TEST_SRCS = $(DSP_DIR)/dsp.c
DATA_ANALYSIS_SRCS = $(wildcard $(DATA_ANALYSIS_DIR)/*.cpp)

# Object files
ASIC_CONTROL_OBJS = $(patsubst $(ASIC_CONTROL_DIR)/%.c, $(DEBUG_DIR)/%.o, $(ASIC_CONTROL_SRCS))
SIMPLE_RX_OBJ = $(patsubst $(SIMPLE_RX_DIR)/%.c, $(DEBUG_DIR)/%.o, $(SIMPLE_RX_SRCS))
CONF_OBJ = $(patsubst $(CONF_DIR)/%.c, $(DEBUG_DIR)/%.o, $(CONF_SRCS))
DATA_COLLECTOR_OBJS = $(patsubst $(DATA_COLLECTOR_DIR)/%.c, $(DEBUG_DIR)/%.o, $(DATA_COLLECTOR_SRCS)) $(ASIC_CONTROL_OBJS) $(SIMPLE_RX_OBJ) $(CONF_OBJ)
TDC_START_OBJ = $(patsubst $(DATA_COLLECTOR_DIR)/%.c, $(DEBUG_DIR)/%.o, $(TDC_START_SRC)) $(ASIC_CONTROL_OBJS) $(SIMPLE_RX_OBJ) $(CONF_OBJ)
DSP_TEST_OBJS = $(patsubst %.c, $(DEBUG_DIR)/%.o, $(notdir $(DSP_TEST_SRCS))) $(ASIC_CONTROL_OBJS) $(SIMPLE_RX_OBJ) $(CONF_OBJ)
DATA_ANALYSIS_OBJS = $(patsubst $(DATA_ANALYSIS_DIR)/%.cpp, $(DEBUG_DIR)/%.o, $(DATA_ANALYSIS_SRCS))

# Shared Object files
ASIC_CONTROL_SOBJS = $(patsubst $(ASIC_CONTROL_DIR)/%.c, $(DEBUG_DIR)/%.so, $(ASIC_CONTROL_SRCS))
SIMPLE_RX_SOBJ = $(patsubst $(SIMPLE_RX_DIR)/%.c, $(DEBUG_DIR)/%.so, $(SIMPLE_RX_SRCS))
CONF_SOBJ = $(patsubst $(CONF_DIR)/%.c, $(DEBUG_DIR)/%.so, $(CONF_SRCS))
DATA_COLLECTOR_SOBJS = $(patsubst $(DATA_COLLECTOR_DIR)/%.c, $(DEBUG_DIR)/%.so, $(DATA_COLLECTOR_SRCS)) $(ASIC_CONTROL_SOBJS) $(SIMPLE_RX_SOBJ) $(CONF_SOBJ)
TDC_START_SOBJ = $(patsubst $(DATA_COLLECTOR_DIR)/%.c, $(DEBUG_DIR)/%.so, $(TDC_START_SRC)) $(ASIC_CONTROL_SOBJS) $(SIMPLE_RX_SOBJ) $(CONF_SOBJ)
DATA_ANALYSIS_SOBJS = $(patsubst $(DATA_ANALYSIS_DIR)/%.cpp, $(DEBUG_DIR)/%.so, $(DATA_ANALYSIS_SRCS))

# Targets
TARGETS = $(DEBUG_DIR)/tdc_test $(DEBUG_DIR)/tdc_start $(DEBUG_DIR)/data_analysis $(DEBUG_DIR)/dsp

# Rules
all: debug release shared

debug: CFLAGS += -g
debug: CXXFLAGS += -g
debug: $(TARGETS)

release: CFLAGS += -O2
release: CXXFLAGS += -O2
release: $(patsubst $(DEBUG_DIR)/%, $(RELEASE_DIR)/%, $(TARGETS))

shared: $(SHARED_DIR)/tdc_test.so $(SHARED_DIR)/tdc_start.so $(SHARED_DIR)/data_analysis.so $(SHARED_DIR)/simple_rx.so $(SHARED_DIR)/asic_control.so $(SHARED_DIR)/conf.so $(SHARED_DIR)/dsp.so

pp: $(DEBUG_DIR)/data_analysis

$(DEBUG_DIR)/%.o: $(ASIC_CONTROL_DIR)/%.c
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(DEBUG_DIR)/%.o: $(SIMPLE_RX_DIR)/%.c
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(DEBUG_DIR)/%.o: $(CONF_DIR)/%.c
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(DEBUG_DIR)/%.o: $(DATA_COLLECTOR_DIR)/%.c
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(DEBUG_DIR)/%.o: $(DATA_ANALYSIS_DIR)/%.cpp
	@mkdir -p $(DEBUG_DIR)
	$(CXX) $(CXXFLAGS) -fPIC  -c $< -o $@

$(DEBUG_DIR)/%.o: $(TDC_START_OBJ)/%.c
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -fPIC  -c $< -o $@

$(DEBUG_DIR)/%.o: $(DSP_DIR)/%.c
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -fPIC  -c $< -o $@

# create also so files
$(SHARED_DIR)/%.so: $(DEBUG_DIR)/%.o
	@mkdir -p $(SHARED_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $<


$(DEBUG_DIR)/tdc_test: $(DATA_COLLECTOR_OBJS)
	$(CC) $(CFLAGS) $(DATA_COLLECTOR_OBJS) -o $@ $(LDFLAGS)

$(DEBUG_DIR)/tdc_start: $(TDC_START_OBJ)
	$(CC) $(CFLAGS) $(TDC_START_OBJ) -o $@ $(LDFLAGS)

$(DEBUG_DIR)/data_analysis: $(DATA_ANALYSIS_OBJS)
	$(CXX) $(CXXFLAGS) $(DATA_ANALYSIS_OBJS) -o $@ $(LDFLAGS)

$(DEBUG_DIR)/dsp: $(DSP_TEST_OBJS)
	$(CC) $(CFLAGS) $(DSP_TEST_OBJS) -o $@ $(LDFLAGS)

$(RELEASE_DIR)/%: $(DEBUG_DIR)/%
	@mkdir -p $(RELEASE_DIR)
	cp $< $@
	cp $< ./
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all debug release clean
