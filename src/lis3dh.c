#include <stdio.h>
#include <esp_log.h>
#include "driver/i2c.h"

#define LED_PIN GPIO_NUM_5
#define HIGH 1
#define LOW 0

#define ACCEL 0x19 /* Address of LIS3DH Accelerometer */
#define WHO_AM_I 0x0f /* Address of id register */
#define WHO_AM_I_ID 0x33
#define CTRL_REG1 0x20 /* Address of enable/disable register */

/* Acceleration value registers */
#define REG_X 0x28
#define REG_Y 0x2A
#define REG_Z 0x2C

#define I2C_MASTER_SCL_IO  GPIO_NUM_21 /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO  GPIO_NUM_22 /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM  I2C_NUM_0      /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE  0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE  0   /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ  100000     /*!< I2C master clock frequency */

#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */

//Error handler for when things go wrong
#undef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x)   do { esp_err_t rc = (x); if (rc != ESP_OK) { ESP_LOGE("err", "esp_err_t = %d", rc); assert(0 && #x);} } while(0);

static const char* TAG = "lis3dh";

typedef struct accel_values {
    uint16_t x;
    uint16_t y;
    uint16_t z;
} accel_values;

void write_byte(uint8_t value)
{
   i2c_cmd_handle_t cmd = i2c_cmd_link_create();
   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, (ACCEL << 1) | WRITE_BIT, ACK_CHECK_EN);
   i2c_master_write_byte(cmd, value, ACK_CHECK_DIS);
   i2c_master_stop(cmd);
   ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS));
   i2c_cmd_link_delete(cmd);
}

/**
 * Write value to register
 */
void write_reg(uint8_t reg_addr, uint8_t value)
{
   i2c_cmd_handle_t cmd = i2c_cmd_link_create();
   i2c_master_start(cmd);
   i2c_master_write_byte(cmd, (ACCEL << 1) | WRITE_BIT, ACK_CHECK_EN);
   i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_DIS);
   i2c_master_write_byte(cmd, value, ACK_CHECK_DIS);
   i2c_master_stop(cmd);
   ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS));
   i2c_cmd_link_delete(cmd);
}

uint8_t read_byte()
{
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte (cmd, (ACCEL<<1) | I2C_MASTER_READ, ACK_CHECK_EN);
  uint8_t res = 0;
  i2c_master_read_byte (cmd, &res, NACK_VAL);
  i2c_master_stop(cmd);
  ESP_ERROR_CHECK(i2c_master_cmd_begin (I2C_MASTER_NUM, cmd, 100/ portTICK_PERIOD_MS));
  i2c_cmd_link_delete(cmd);
  return res;
}

/**
 * Read register
 */
uint8_t read_reg(uint8_t reg_addr)
{
  // Write the register we want to read to the bus
  write_byte(reg_addr);

  // Now read a byte from that register
  return read_byte();
}

/**
 * Read acceleration values. Reads the 6 bytes
 * at the given address and returns them as an
 * array of 3 uint16_ts
 */
accel_values read_acceleration() {

  // Write the register we want to read to the bus
  // Set the 8th bit (MSB) to 1 to enable address autoincrement
  // This allows us to read multiple values at once
  write_byte(REG_X | 0x80);

  uint8_t data[6];

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte (cmd, (ACCEL<<1) | I2C_MASTER_READ, ACK_CHECK_EN);
  int16_t res = 0;
  i2c_master_read(cmd, data, 5, ACK_VAL);
  i2c_master_read_byte(cmd, data+5, NACK_VAL);
  i2c_master_stop(cmd);
  ESP_ERROR_CHECK(i2c_master_cmd_begin (I2C_MASTER_NUM, cmd, 100/ portTICK_PERIOD_MS));
  i2c_cmd_link_delete(cmd);

  uint16_t x = (data[1] << 8) | data[0];
  uint16_t y = (data[3] << 8) | data[2];
  uint16_t z = (data[5] << 8) | data[4];

  accel_values ret = {x, y, z};
  return ret;
}

/**
 * Initalise the accelerometer as an I2C slave
 */
void init_i2c_device()
{
    i2c_port_t i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_MASTER_RX_BUF_DISABLE,
                       I2C_MASTER_TX_BUF_DISABLE, 0);


    uint8_t id = read_reg(WHO_AM_I); //Read the WHO_AM_I register
    if(id != WHO_AM_I_ID) {
        ESP_LOGE(TAG, "Accelerometer returned unexpected id: %d", id);
        return;
    }
}
