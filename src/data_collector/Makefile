# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -O2

# Libraries to link against
LIBS = -lgpiod

# Target executable
TARGET = tdc_test

# Source files
SRCS = tdc_test.c asic_control.c

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

.PHONY: all clean