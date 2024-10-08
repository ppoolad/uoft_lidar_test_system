# Memory Word Extractor and Histogram Plotter

## Overview

This project consists of a C++ program that extracts memory words from a text file and a Python script that reads the extracted memory words and plots a histogram.

## Project Structure
The project has the following structure:
```
gpio_app/
├── src
|   ├── data_analysis
|   |   ├── data_analysis.cpp
|   |   └── DrawHist.py
|   ├── data_collector
|   |   ├── asic_control.c/h
|   |   ├── tdc_start.c
|   |   └── tdc_test.c
|   └── asic.h
└── README.md
```
- `data_analysis`: C++/python program to extract memory words from a file and draw histogram of codes and calculated INL/DNL.
- `data_collector`: tdc_test program starts tdc, collects data in a fifo, reset and repeat for 10mins. tdc_start, just starts the tdc for manual inspection
- `asic.h`: pin map of the asic interface to the FPGA

## C++ Program: data_analysis

### Description

The C++ program is invoked in terminal by data_analysis_release <ascii_outputs_from_data_collector.txt> <number of packet to extract> and isolates readout from one of the channels. Then, DrawHist.py reads "extracted_memory_word.txt" and draws histograms.

## C Program: data_collector

### Description
The C Program is used to collect data from the ASIC's serialzier via FPGA LVDS inputs (Refer to simple_rx repo). and save them in a text file. use `xxd -c 4 -g 4` to format them into ascii. 

## Compilation and Execution

each folder has a Makefile inside. just use "make"

### Requirements

- gpiod library must be installed on linux
- axi_fifo driver [(repo)](https://github.com/jacobfeder/axisfifo/tree/master)