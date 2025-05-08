import matplotlib.pyplot as plt
import plotly.graph_objects as go
import streamlit as st
import numpy as np

import os, ctypes
import threading
import os
import time

from streamlit_autorefresh import st_autorefresh

SHARED_LIB_PATH = "../shared_objects"
STREAM_FILE_PATH = b"../txt.txt"
PACKET_NUMBER = 1
OUTPUT_FILE_NAME = b"test1"
ADJUST_START = 1
NBINS = 31 * 2

st_autorefresh(interval=1000, limit=None, key="autorefresh")

def read_memory_words(filename, max_value = 2**6):
    #n=0
    def is_valid_number(line):
        number = int(line)
        return 0 <= number <= max_value
    with open(filename, 'r') as file:
        lines = file.readlines()
        n = len(lines)
        #lines_stripped = [int(line.strip('\n').strip()) for line in lines if line.strip()]
        #lines_filtered = [number for number in lines_stripped 
    return [[int(line.strip('\n').strip()) & 0xfffffffe for line in lines if is_valid_number(line.strip())], n]

def draw_histogram(data, n, nbins=31*2):
    # create histogram bin edges and counts
    hist, bin_edges = np.histogram(data, bins=nbins)
    expected = len(data) / nbins

    fig = go.Figure()

    fig.add_trace(go.Bar(
        x=bin_edges[:-1],
        y=hist,
        marker_color='#B583F2',
        name='TDC Count',
        width=np.diff(bin_edges)
    ))

    # add expected line - red
    fig.add_shape(
        type="line",
        x0=bin_edges[0],
        x1=bin_edges[-1],
        y0=expected,
        y1=expected,
        line=dict(color="red", dash="dash"),
        name="Expected"
    )

    fig.update_layout(
        title="Histogram of The TDC Channel 1 Codes",
        xaxis_title="TDC Code",
        yaxis_title="# of Occurrences",
        bargap=0.01,
        showlegend=False
    )

    return fig

def run_data_analysis(file_path, packet_number, output_name, adjust_start, nbins):
    # if 'output' folder does not exist, create it
    if not os.path.exists("./outputs"):
        os.makedirs("./outputs")

    data_analysis = ctypes.CDLL(f"{SHARED_LIB_PATH}/libdata_analysis.so")

    data_analysis.extern_main.argtypes = [
        ctypes.c_char_p,  # const char* file
        ctypes.c_int,     # int packet_number
        ctypes.c_char_p,  # const char* output_name
        ctypes.c_int,     # int adjust_start
        ctypes.c_int,     # int nbins
    ]
    data_analysis.extern_main.restype = ctypes.c_int

    result = data_analysis.extern_main(
        file_path,
        packet_number,
        output_name,
        adjust_start,
        nbins
    )

    print(f"extern_main() returned: {result}")

    return result

st.title("📡 Data Analysis")

# SIDEBAR
## TEST SETTINGS
st.sidebar.title("Settings")
st.sidebar.write("Adjust the parameters for data analysis:")

st.sidebar.write("### Data Analysis Settings")

stream_file = st.sidebar.text_input("Stream File Path (Path on FPGA)", value=STREAM_FILE_PATH.decode())

packet_number = st.sidebar.number_input("Packet Number", min_value=1, value=PACKET_NUMBER)

# dropdown menu for output name with a custom entry option
existing_names = [
    name.replace("_extracted_memory_words.txt", "") for name in os.listdir("./outputs") if name.endswith(".txt")
]
default_names = ["Custom", OUTPUT_FILE_NAME.decode()] + existing_names
selected_name = st.sidebar.selectbox("Output Name", default_names)
if selected_name == "Custom":
    output_name = st.sidebar.text_input("Enter output name", value="")
else:
    output_name = selected_name

adjust_start = st.sidebar.checkbox("Adjust Start", value=ADJUST_START)

## HISTOGRAM SETTINGS
st.sidebar.write("### Histogram Settings")
nbins = st.sidebar.slider("Number of Bins", min_value=1, max_value=128, value=NBINS)

if st.sidebar.button("Run Analysis"):
    run_data_analysis(
        stream_file.encode(),
        packet_number,
        output_name.encode(),
        int(adjust_start),
        nbins
    )

# FIGURE
st.write("### 📊 Histogram")

filename = os.path.join("./outputs", f"{output_name}_extracted_memory_words.txt")

if os.path.exists(filename):
    data, n = read_memory_words(filename)
    fig = draw_histogram(data, n, nbins=nbins)
    st.plotly_chart(fig, use_container_width=True)
else:
    st.write("No data available. Please run the analysis first.")