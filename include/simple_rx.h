#define UIO_DEVICE "/dev/uio4"
#define RX_ENABLE_MASK 0x01

int init_rx(void);
void enable_rx(void);
void disable_rx(void);
void cleanup_rx(void);

