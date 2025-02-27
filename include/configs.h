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
} DSP_config;

typedef struct {
    int channel_enables[6] = {0};
    int channel_offsets[6] = {0};
    int tdc_chain_num_words;
    int tdc_chain_num_bits;
    int tdc_chain_timeout;
    int tdc_serdes_nbits;
    int tdc_start_mode = 0;
    int tdc_coarse_mode = 0;
    int tdc_external_mode = 0;
    int scheduler_external_mode = 0;
} TDC_config;

typedef struct {
    IO_dev_config io_dev_config;
    DSP_config dsp_config;
    TDC_config tdc_config;
    std::string output_file;
    int runtime;
    int debug_log;

} Config;

Config parse_config(std::string filename);