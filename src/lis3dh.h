#ifndef lis3dh_H_
#define lis3dh_H_

#include <stdint.h>

typedef struct accel_values {
    uint16_t x;
    uint16_t y;
    uint16_t z;
} accel_values;

void write_byte(uint8_t value);

/**
 * Write value to register
 */
void write_reg(uint8_t reg_addr, uint8_t value);

/**
 * Read register
 */
uint8_t read_reg(uint8_t reg_addr);

/**
 * Read acceleration values. Reads the 6 bytes
 * at the given address and returns them as an
 * array of 3 uint16_ts
 */
accel_values read_acceleration();

/**
 * Initalise the accelerometer as an I2C slave
 */
void init_i2c_device();

#endif
