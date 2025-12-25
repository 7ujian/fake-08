#pragma once

#include "cyberpi_config.h"
#include "drivers/aw9523b.h"
#include "drivers/st7735.h" 
#include "driver/gpio.h"
#include "driver/spi_master.h"

class CyberPi {
public:
    // 单例模式获取实例
    static CyberPi& getInstance();
    
    // 系统初始化：配置 I2C/SPI, AW9523B 混合模式, 初始化 LCD
    void init();

    // 绘制屏幕缓冲区 (RGB565)
    void render(const uint16_t* frameBuffer);

    // 每帧调用，通过 I2C 读取 AW9523B 状态并缓存
    void updateInputState();
    
    // 查询按键状态 (基于 updateInputState 的缓存结果)
    // pin_index: 0-7 (Port 0), 8-15 (Port 1)
    // 推荐使用 config 中的 CYBERPI_KEY_* 宏
    bool isButtonPressed(uint8_t pin_index);

    // 获取扩展芯片对象指针
    AW9523B* getIoExpander() { return _io_expander; }

private:
    CyberPi();
    CyberPi(const CyberPi&) = delete;
    CyberPi& operator=(const CyberPi&) = delete;

    AW9523B* _io_expander; 
    ST7735* _lcd;
    spi_device_handle_t _spi_handle;

    // 输入状态缓存 (16位: 低8位=Port0, 高8位=Port1)
    uint16_t _cached_input_state; 

    void initI2C();
    void initSPI();
};