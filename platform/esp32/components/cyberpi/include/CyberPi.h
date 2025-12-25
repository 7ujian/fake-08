#pragma once

#include "cyberpi_config.h"
#include "drivers/aw9523b.h"
#include "drivers/st7735.h" 
#include "driver/gpio.h"
#include "driver/spi_master.h"


class CyberPi {
public:
    // 单例访问
    static CyberPi& getInstance();
    
    // 初始化系统 (I2C, SPI, 扩展芯片配置, LCD)
    void init();

    // 绘制屏幕 (推送到 ST7735)
    void render(const uint16_t* frameBuffer);

    // [核心] 每帧调用一次，通过 I2C 读取并缓存按键状态
    void updateInputState();

    // [核心] 查询某个按键是否按下 (基于 updateInputState 的缓存结果)
    // pin_index: 0-15 (0-7 for Port0, 8-15 for Port1)
    bool isButtonPressed(uint8_t pin_index);

    // 获取扩展芯片指针 (如果其他模块需要直接操作 IO)
    AW9523B* getIoExpander() { return _io_expander; }

private:
    CyberPi();
    CyberPi(const CyberPi&) = delete;
    CyberPi& operator=(const CyberPi&) = delete;

    // 硬件实例
    AW9523B* _io_expander; // 地址 0x58
    ST7735* _lcd;
    spi_device_handle_t _spi_handle;

    // 输入状态缓存 (16位: Low Byte=Port0, High Byte=Port1)
    uint16_t _cached_input_state; 

    // 内部初始化函数
    void initI2C();
    void initSPI();
};