# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -O2

# Libraries to link against
LIBS = -lgpiod

# Target executable
TARGET = tdc_test

# Source files
SRCS = tdc_test.c asic_control.c conf.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

# Separate build for tdc_start and asic_control
tdc_start: tdc_start.o asic_control.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile tdc_start.c to tdc_start.o
tdc_start.o: tdc_start.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile asic_control.c to asic_control.o
asic_control.o: asic_control.c
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

.PHONY: all clean