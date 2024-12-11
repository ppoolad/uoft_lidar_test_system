#include <string>
//configuration struct
typedef struct {
    std::string hpc1_chip_name;
    std::string led_chip_name;

    std::string output_file;
    std::string rx_dev_fifo;
    int nbits_rx;
    int runtime;
    float snr;
    int kernel_means[4];
    int kernel_std_devs[4];
    float kernel_weights[4];

} Config;

Config parse_config(std::string filename);