#include <string>
//configuration struct
#define NUM_KERNELS 4
#define MAX_BUF_SIZE_BYTES 4096
typedef struct {
    std::string hpc1_chip_name;
    std::string led_chip_name;

    std::string output_file;
    std::string rx_dev_fifo;
    int nbits_rx;
    int runtime;
    float snr;
    int kernel_means[NUM_KERNELS];
    int kernel_std_devs[NUM_KERNELS];
    float kernel_weights[NUM_KERNELS];

} Config;

Config parse_config(std::string filename);