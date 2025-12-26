#pragma once

#include "cyberpi_config.h"
#include "drivers/aw9523b.h"
#include "drivers/st7735.h" 
#include "driver/gpio.h"
#include "driver/spi_master.h"

class CyberPi {
public:
    static CyberPi& getInstance();
    
    // 初始化系统
    void init();

    // 绘制屏幕
    void render(const uint16_t* frameBuffer);

    // [输入] 每帧调用：通过 I2C 读取 AW9523B (0x58) 状态并缓存
    void updateInputState();

    // [输入] 查询按键
    // pin_index 使用 cyberpi_config.h 中的定义 (0-15)
    bool isButtonPressed(uint8_t pin_index);

    // 获取扩展芯片对象 (供 ST7735 驱动使用)
    AW9523B* getIoExpander() { return _io_expander; }

private:
    CyberPi();
    CyberPi(const CyberPi&) = delete;
    CyberPi& operator=(const CyberPi&) = delete;

    AW9523B* _io_expander; 
    ST7735* _lcd;
    spi_device_handle_t _spi_handle;

    // 输入状态缓存 (16位)
    // Low Byte: Port 0 State
    // High Byte: Port 1 State
    uint16_t _cached_input_state; 

    void initI2C();
    void initSPI();
};