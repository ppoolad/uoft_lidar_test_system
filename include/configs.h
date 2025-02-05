#include <string>
//configuration struct
#define NUM_KERNELS 4
#define MAX_BUF_SIZE_BYTES 4096

typedef struct {
    std::string hpc0_chip_name;
    int num_hpc0;
    std::string hpc1_chip_name;
    int num_hpc1;
    std::string led_chip_name;
    int num_led;
    std::string pb_chip_name;
    int num_pb;

    std::string rx_dev_fifo;
    int nbits_rx;

    std::string uio_device;

} IO_dev_config;

typedef struct {
    float snr;
    int kernel_means[NUM_KERNELS];
    int kernel_std_devs[NUM_KERNELS];
    float kernel_weights[NUM_KERNELS];
    int debug_log;
} DSP_config;

typedef struct {
    IO_dev_config io_dev_config;
    DSP_config dsp_config;

    std::string output_file;
    int runtime;

} Config;

Config parse_config(std::string filename);