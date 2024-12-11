#ifndef ASIC_H
#define ASIC_H

#define HPC1_CHIP_NAME "gpiochip3"
#define HPC1_SYS_CHIP_NAME "gpiochip427"
#define HPC1_NUM_GPIO 40

#define TDC_ARESETN 0            // TDC
#define GND 1                    // and all missing numbers below
#define TDC_START_MODE 2         // TDC
#define TOF_TEST_MODE 4          // TDC
#define SERIAL_TOF_EN 6          // DSP 
#define DSP_ARESETN 8            // DSP
#define PIXEL3 10                // DSP
#define PIXEL2 12                // DSP
#define PIXEL0 14                // DSP
#define RO_RST 15                // RO
#define PIXEL1 16                // DSP
#define TDC_DEBUG_RD_EN 17       // TDC
#define TDC_SERIAL_CONF 18       // TDC
#define TDC_STOP_DEBUG_MODE 20   // TDC
#define SERIAL_CONFIG_EN 21      // DSP
#define TOF_TEST_VALID 22        // DSP
#define SCHED_EN 23              // SCHED
#define SCHED_ARESETN 24         // SCHED
#define EXTERNAL_EN 25           // SCHED
#define READ_EN 27               // DSP
#define DIGITAL_EN 29            // DSP
#define COARSE_EN 30             // TDC
#define TDC_EN 36                // TDC
#define START_SIGNAL 37          // TDC


#define HPC0_CHIP_NAME "gpiochip2"
#define HPC0_SYS_CHIP_NAME "gpiochip467"
#define HPC0_NUM_GPIO 32

#define LED_CHIP_NAME "gpiochip0"
#define LED_SYS_CHIP_NAME "gpiochip504"
#define LED_NUM_GPIO 8

#define PB_CHIP_NAME "gpiochip1"
#define PB_SYS_CHIP_NAME "gpiochip499"
#define PB_NUM_GPIO 5




#endif /* ASIC_H */
