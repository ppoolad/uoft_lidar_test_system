#include <string>
//configuration struct
typedef struct {
    std::string hpc1_chip_name;
    std::string led_chip_name;

    std::string output_file;
    std::string rx_dev_fifo;
    int nbits_rx;
    int runtime;
    
} Config;