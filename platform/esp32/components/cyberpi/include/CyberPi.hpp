/* include/CyberPi.hpp */
#pragma once

#include "cyberpi_config.h" // <--- 引入配置
#include "drivers/aw9523b.h"
#include "drivers/st7735.h" 

// 类声明
class CyberPi {
public:
    static CyberPi& getInstance();
    
    void init();
    void render(const uint16_t* frameBuffer);
    bool isButtonPressed(gpio_num_t btn);

    AW9523B* getIoExpander() { return _io_expander; }

private:
    CyberPi();
    CyberPi(const CyberPi&) = delete;
    CyberPi& operator=(const CyberPi&) = delete;

    AW9523B* _io_expander;
    ST7735* _lcd;
    spi_device_handle_t _spi_handle;

    void initI2C();
    void initSPI();
};