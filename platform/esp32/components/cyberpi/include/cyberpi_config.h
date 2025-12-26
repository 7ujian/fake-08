#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

// ==========================================
// 1. ESP32 物理引脚定义
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

// --- 板载直连按键 (保留定义) ---
#define CYBERPI_BTN_A_GPIO  GPIO_NUM_13 
#define CYBERPI_BTN_B_GPIO  GPIO_NUM_14 

// ==========================================
// 2. AW9523B 扩展芯片地址
// ==========================================
// 芯片 A: 负责 LCD 控制和 游戏按键输入 (AD0=0, AD1=0)
#define AW_ADDR_IO_LCD      0x58 

// 芯片 B: 负责 RGB LED (AD0=1, AD1=1) - 本次暂不操作
#define AW_ADDR_LEDS        0x5B

// ==========================================
// 3. 屏幕参数 (ST7735)
// ==========================================
#define LCD_WIDTH           128
#define LCD_HEIGHT          128
#define LCD_OFFSET_X        3
#define LCD_OFFSET_Y        3

// ==========================================
// 4. AW9523B 引脚映射 (严格保留你的定义)
// ==========================================

// --- LCD 控制线 (Port 1) ---
#define AW_LCD_PORT         1  // Port 1
#define AW_LCD_DC_PIN       4  // P1_4 -> Data/Command
#define AW_LCD_RST_PIN      5  // P1_5 -> Reset
#define AW_LCD_BL_PIN       7  // P1_7 -> Backlight
// 补充: 功放使能通常在 P1_3，防止没声音
#define AW_AMP_EN_PIN       3  

// --- 输入按键映射 (Port 0) ---
#define CYBERPI_KEY_LEFT    0   // AW_P0_0
#define CYBERPI_KEY_UP      1   // AW_P0_1
#define CYBERPI_KEY_RIGHT   2   // AW_P0_2
#define CYBERPI_KEY_CENTER  3   // AW_P0_3 (Joystick Press)
#define CYBERPI_KEY_DOWN    4   // AW_P0_4
#define CYBERPI_KEY_B       5   // AW_P0_5
#define CYBERPI_KEY_A       6   // AW_P0_6

// --- 输入按键映射 (Port 1) ---
// 索引 = 8 + Pin号 (P1_0)
#define CYBERPI_KEY_MENU    8   // AW_P1_0 -> 8 + 0 = 8

// ==========================================
// 5. 系统设置
// ==========================================
// 是否默认显示 FPS (1=开启, 0=关闭)
#define DEFAULT_SHOW_FPS    1