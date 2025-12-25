#include "drivers/st7735.h"
#include "cyberpi_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cstring>
#include <algorithm>
#include "rom/ets_sys.h"

static const char* TAG = "ST7735";

// 保留延时以确保稳定性
#define LCD_DC_LOW()  if(_io) { _io->digitalWrite(AW_LCD_PORT, AW_LCD_DC_PIN, 0); ets_delay_us(50); }
#define LCD_DC_HIGH() if(_io) { _io->digitalWrite(AW_LCD_PORT, AW_LCD_DC_PIN, 1); ets_delay_us(50); }
#define LCD_RST_LOW() if(_io) { _io->digitalWrite(AW_LCD_PORT, AW_LCD_RST_PIN, 0); vTaskDelay(pdMS_TO_TICKS(10)); }
#define LCD_RST_HIGH() if(_io) { _io->digitalWrite(AW_LCD_PORT, AW_LCD_RST_PIN, 1); vTaskDelay(pdMS_TO_TICKS(120)); }
#define LCD_BL_ON()   if(_io) _io->digitalWrite(AW_LCD_PORT, AW_LCD_BL_PIN, 1)

ST7735::ST7735(spi_device_handle_t spi, AW9523B* io) 
    : _spi(spi), _io(io) {
    // 构造函数先赋默认值，具体旋转逻辑在 init 中可能需要调整
    _colstart = LCD_OFFSET_X; 
    _rowstart = LCD_OFFSET_Y; 
}

void ST7735::reset() {
    LCD_RST_HIGH();
    LCD_RST_LOW();
    LCD_RST_HIGH();
}

void ST7735::sendCmd(uint8_t cmd) {
    LCD_DC_LOW();
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_polling_transmit(_spi, &t);
    LCD_DC_HIGH();
}

void ST7735::sendData(const uint8_t* data, int len) {
    if (len <= 0) return;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_polling_transmit(_spi, &t);
}

void ST7735::init() {
    ESP_LOGI(TAG, "Initializing ST7735 (Offset X:%d Y:%d)...", _colstart, _rowstart);
    reset();
    
    sendCmd(0x01); vTaskDelay(pdMS_TO_TICKS(150)); // SWRESET
    sendCmd(0x11); vTaskDelay(pdMS_TO_TICKS(255)); // SLPOUT

    // 标准 ST7735 配置
    uint8_t data1[] = {0x01, 0x2C, 0x2D};
    sendCmd(0xB1); sendData(data1, 3);
    sendCmd(0xB2); sendData(data1, 3);
    uint8_t data2[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
    sendCmd(0xB3); sendData(data2, 6);
    
    sendCmd(0xB4); sendData((uint8_t[]){0x07}, 1); // Inversion

    // Power Settings
    sendCmd(0xC0); sendData((uint8_t[]){0xA2, 0x02, 0x84}, 3);
    sendCmd(0xC1); sendData((uint8_t[]){0xC5}, 1);
    sendCmd(0xC2); sendData((uint8_t[]){0x0A, 0x00}, 2);
    sendCmd(0xC3); sendData((uint8_t[]){0x8A, 0x2A}, 2);
    sendCmd(0xC4); sendData((uint8_t[]){0x8A, 0xEE}, 2);
    sendCmd(0xC5); sendData((uint8_t[]){0x0E}, 1);

    // =================================================================
    // [修改] 旋转设置 (MADCTL 0x36)
    // 原值: 0xC0 (MX=1, MY=1, MV=0) -> Portrait
    // 目标: Landscape (90度顺时针)
    // 常用值: 
    //   0xA0 (MY=1, MV=1, MX=0, RGB=0) -> Landscape
    //   0x60 (MV=1, MX=1, MY=0, RGB=0) -> Landscape (180度翻转)
    //   0x70 (MV=1, MX=1, MY=1, RGB=0) -> 也就是 X/Y 交换且翻转
    //
    // 这里我们使用 0xA0 作为标准的 Landscape。
    // 如果发现颜色反了(红变蓝)，把 0xA0 改为 0xA8 (RGB bit)。
    // 如果发现方向是反的(270度)，尝试改为 0x60。
    // =================================================================
    sendCmd(0x36); sendData((uint8_t[]){0xA0}, 1); 

    // [重要] 旋转后，行列偏移量通常需要交换
    // 如果画面没有居中（比如上方或左边有杂色/黑边），请尝试交换下面的赋值，
    // 或者直接手动调整这里的数值。
    _colstart = LCD_OFFSET_Y; 
    _rowstart = LCD_OFFSET_X;
    
    // COLMOD: 16-bit
    sendCmd(0x3A); sendData((uint8_t[]){0x05}, 1);

    // Gamma
    uint8_t gm_p[] = {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10};
    sendCmd(0xE0); sendData(gm_p, 16);
    uint8_t gm_n[] = {0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10};
    sendCmd(0xE1); sendData(gm_n, 16);

    sendCmd(0x29); // DISPON
    vTaskDelay(pdMS_TO_TICKS(100));
    
    LCD_BL_ON();

    // 清屏全黑 (验证 SPI 是否工作)
    ESP_LOGI(TAG, "Clearing Screen...");
    static uint16_t blackBuf[128]; // 256 bytes, safe for DMA
    memset(blackBuf, 0, sizeof(blackBuf));
    
    // 注意：旋转后 LCD_WIDTH 和 LCD_HEIGHT 的含义在硬件上交换了，
    // 但我们的宏定义 (128x128) 没变，所以清屏逻辑依然适用。
    sendCmd(0x2A); 
    uint8_t x[] = {0, 0, 0, 127}; sendData(x, 4);
    sendCmd(0x2B); 
    uint8_t y[] = {0, 0, 0, 127}; sendData(y, 4);
    sendCmd(0x2C);
    
    for(int i=0; i<128; i++) {
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = 128 * 16;
        t.tx_buffer = blackBuf;
        // 使用 polling (queue 需要配合 pre_cb/post_cb 处理 CS，polling 更简单)
        spi_device_polling_transmit(_spi, &t);
    }
}

void ST7735::drawBitmap(const uint16_t* buffer) {
    // 设置窗口
    uint16_t x_start = _colstart;
    uint16_t x_end = _colstart + LCD_WIDTH - 1;
    uint16_t y_start = _rowstart;
    uint16_t y_end = _rowstart + LCD_HEIGHT - 1;

    sendCmd(0x2A);
    uint8_t data_x[] = {0, (uint8_t)x_start, 0, (uint8_t)x_end};
    sendData(data_x, 4);

    sendCmd(0x2B);
    uint8_t data_y[] = {0, (uint8_t)y_start, 0, (uint8_t)y_end};
    sendData(data_y, 4);

    sendCmd(0x2C);

    // [关键] 分块传输，每块 < 4092 字节
    // 每次传 2行 (128*2*2 = 512 bytes)，非常安全
    const int CHUNK_PIXELS = 128 * 8; // 1024 pixels = 2048 bytes
    const int TOTAL_PIXELS = LCD_WIDTH * LCD_HEIGHT;
    int pixels_sent = 0;

    while (pixels_sent < TOTAL_PIXELS) {
        int now = std::min(CHUNK_PIXELS, TOTAL_PIXELS - pixels_sent);
        
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = now * 16;
        t.tx_buffer = buffer + pixels_sent; // PSRAM 指针
        
        // 即使 buffer 在 PSRAM，由于启用了 DMA 且 size < 4092，
        // 驱动会自动申请内部 RAM 并 copy，不会崩。
        spi_device_polling_transmit(_spi, &t);
        
        pixels_sent += now;
    }
}