/* include/drivers/st7735.h */
#pragma once

#include "driver/spi_master.h"
#include "drivers/aw9523b.h"

class ST7735 {
public:
    ST7735(spi_device_handle_t spi, AW9523B* io);

    void init();
    
    // 推送全屏画面 (128x128 RGB565)
    void drawBitmap(const uint16_t* buffer);

    // 基础命令
    void sendCmd(uint8_t cmd);
    void sendData(const uint8_t* data, int len);

private:
    spi_device_handle_t _spi;
    AW9523B* _io;
    uint8_t _colstart;
    uint8_t _rowstart;

    void reset();
};