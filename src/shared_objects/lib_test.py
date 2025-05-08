import ctypes

lib = ctypes.CDLL("./libdata_analysis.so")

lib.extern_main.argtypes = [
    ctypes.c_char_p,  # const char* file
    ctypes.c_int,     # int packet_number
    ctypes.c_char_p,  # const char* output_name
    ctypes.c_int      # int adjust_start
]
lib.extern_main.restype = ctypes.c_int

# Sample input values
filename = b"../txt.txt"        # File to read from
packet_number = 1                    # Which packet to read
output_name = b"test1"           # Base name for output files
adjust_start = 1                     # Optional logic flag

result = lib.extern_main(filename, packet_number, output_name, adjust_start)

print(f"extern_main() returned: {result}")
