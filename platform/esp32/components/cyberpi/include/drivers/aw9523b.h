/* include/drivers/aw9523b.h */
#pragma once

#include "driver/i2c.h"
#include <stdint.h>

class AW9523B {
public:
    AW9523B(i2c_port_t port, uint8_t addr);
    
    // 初始化芯片
    bool begin();

    // 设置引脚模式 (port: 0=P0, 1=P1)
    // mode: 0=Output, 1=Input (默认为OpenDrain, LED模式需特殊配置)
    void pinMode(uint8_t port, uint8_t pin, uint8_t mode);

    // 写引脚电平
    void digitalWrite(uint8_t port, uint8_t pin, uint8_t level);

    // 读引脚电平
    uint8_t digitalRead(uint8_t port, uint8_t pin);

    // 批量写端口 (优化 LCD 控制)
    void writePort(uint8_t port, uint8_t value);

private:
    i2c_port_t _i2c_port;
    uint8_t _addr;
    
    // 本地缓存，避免读改写(RMW)导致的慢速 I2C 操作
    uint8_t _out_reg[2];  // 0x02, 0x03
    uint8_t _conf_reg[2]; // 0x04, 0x05

    void writeReg(uint8_t reg, uint8_t data);
    uint8_t readReg(uint8_t reg);
};