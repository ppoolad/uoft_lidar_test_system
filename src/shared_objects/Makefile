# Compiler
CC = g++
CFLAGS = -fPIC -std=c++11
LDFLAGS = -shared -lpthread

# Sources and output
SRCDIR = ../data_analysis
SRCS    = $(SRCDIR)/data_analysis.cpp \
		  ./data_analysis_wrapper.cpp
OUT = libdata_analysis.so

all: $(OUT)

$(OUT): $(SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(OUT) $(SRCS)

clean:
	rm -f $(OUT)