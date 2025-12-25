#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

// ==========================================
// 1. ESP32 物理引脚映射 (基于 CyberPi_Config.h 修正)
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

// --- 片选信号 (关键修正点!) ---
// [修正] 原先认为是12，对比文件后确认为 27
#define CYBERPI_LCD_CS      GPIO_NUM_27 
// 字库芯片片选 (推测为 12，与 LCD 互换)
#define CYBERPI_FONT_CS     GPIO_NUM_12 

// --- 板载按键 ---
#define CYBERPI_BTN_A_GPIO  GPIO_NUM_13 
#define CYBERPI_BTN_B_GPIO  GPIO_NUM_14 

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

// [修正] 基于 CyberPi_Config.h 的 Offset 设置
// X=3, Y=2 是 CyberPi 定制屏幕的特征
#define LCD_OFFSET_X        3
#define LCD_OFFSET_Y        3

// LCD 控制线在 AW9523B 的 Port 1
#define AW_LCD_PORT         1  // Port 1
#define AW_LCD_DC_PIN       4  // P1_4 -> Data/Command
#define AW_LCD_RST_PIN      5  // P1_5 -> Reset
#define AW_LCD_BL_PIN       7  // P1_7 -> Backlight

// ==========================================
// 4. 输入按键映射 (基于提供的硬件定义)
// ==========================================
// 我们将 Port 0 映射为 bit 0-7, Port 1 映射为 bit 8-15

// Port 0 Pins
#define CYBERPI_KEY_LEFT    0   // AW_P0_0
#define CYBERPI_KEY_UP      1   // AW_P0_1
#define CYBERPI_KEY_RIGHT   2   // AW_P0_2
#define CYBERPI_KEY_CENTER  3   // AW_P0_3 (Joystick Press)
#define CYBERPI_KEY_DOWN    4   // AW_P0_4
#define CYBERPI_KEY_B       5   // AW_P0_5
#define CYBERPI_KEY_A       6   // AW_P0_6

// Port 1 Pins (索引 = 8 + Pin号)
#define CYBERPI_KEY_MENU    8   // AW_P1_0 -> 8 + 0 = 8