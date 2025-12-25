#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

// ==========================================
// 1. ESP32 物理引脚映射
// ==========================================

// --- I2C 总线 ---
#define CYBERPI_I2C_PORT    I2C_NUM_1
#define CYBERPI_I2C_SDA     GPIO_NUM_19
#define CYBERPI_I2C_SCL     GPIO_NUM_18
#define CYBERPI_I2C_FREQ    400000

// --- SPI 总线 ---
#define CYBERPI_SPI_HOST    SPI2_HOST
#define CYBERPI_SPI_MOSI    GPIO_NUM_2
#define CYBERPI_SPI_MISO    GPIO_NUM_26
#define CYBERPI_SPI_CLK     GPIO_NUM_4
#define CYBERPI_LCD_CS      GPIO_NUM_27 
#define CYBERPI_FONT_CS     GPIO_NUM_12 

// ==========================================
// 2. AW9523B 扩展芯片配置
// ==========================================
// 两个芯片地址
#define AW9523B_ADDR_LCD    0x58 
#define AW9523B_ADDR_INPUT  0x5B

// ==========================================
// 3. 屏幕参数 (ST7735 128x128)
// ==========================================
#define LCD_WIDTH           128
#define LCD_HEIGHT          128
#define LCD_OFFSET_X        3
#define LCD_OFFSET_Y        3

// LCD 控制线在 AW9523B 的 Port 1 (寄存器位索引 0-7)
#define AW_LCD_PORT         1  
#define AW_LCD_DC_PIN       4  // P1_4
#define AW_LCD_RST_PIN      5  // P1_5
#define AW_LCD_BL_PIN       7  // P1_7
#define AW_AMP_EN_PIN       3  // P1_3 (功放)

// ==========================================
// 4. 输入按键映射 (基于 AW9523B Port 0/1)
// ==========================================
// Port 0 Pins (0-7)
#define CYBERPI_KEY_LEFT    0   // AW_P0_0
#define CYBERPI_KEY_UP      1   // AW_P0_1
#define CYBERPI_KEY_RIGHT   2   // AW_P0_2
#define CYBERPI_KEY_CENTER  3   // AW_P0_3
#define CYBERPI_KEY_DOWN    4   // AW_P0_4
#define CYBERPI_KEY_B       5   // AW_P0_5
#define CYBERPI_KEY_A       6   // AW_P0_6

// Port 1 Pins (实际 Pin号，代码逻辑里通常处理为 8 + Pin)
#define CYBERPI_KEY_MENU_PIN 0  // AW_P1_0
// 为了统一 isButtonPressed 接口 (0-15)，定义其索引 ID
#define CYBERPI_KEY_MENU     8  // 8 + 0