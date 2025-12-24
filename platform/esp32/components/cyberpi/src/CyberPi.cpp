#include "CyberPi.hpp"
#include "esp_log.h"
#include "driver/i2c.h"

static const char* TAG = "CyberPi";

CyberPi::CyberPi() : _io_expander(nullptr), _lcd(nullptr), _spi_handle(nullptr) {}

CyberPi& CyberPi::getInstance() {
    static CyberPi instance;
    return instance;
}

void CyberPi::init() {
    ESP_LOGW(TAG, ">>> CyberPi Init (Corrected Pin 27 + DMA 4KB) <<<");

    initI2C();
    initSPI();

    // 1. 初始化扩展芯片
    ESP_LOGI(TAG, "Connecting to AW9523B (0x58)...");
    _io_expander = new AW9523B(CYBERPI_I2C_PORT, AW9523B_ADDR_LCD);
    
    if (_io_expander == nullptr || !_io_expander->begin()) {
        ESP_LOGE(TAG, "AW9523B Init Failed! Check I2C.");
    } else {
        ESP_LOGI(TAG, "AW9523B Connected. Resetting LCD Pins...");
        _io_expander->pinMode(AW_LCD_PORT, AW_LCD_DC_PIN, 0);
        _io_expander->pinMode(AW_LCD_PORT, AW_LCD_RST_PIN, 0);
        _io_expander->pinMode(AW_LCD_PORT, AW_LCD_BL_PIN, 0);
        
        ESP_LOGI(TAG, "Turning on Backlight...");
        _io_expander->digitalWrite(AW_LCD_PORT, AW_LCD_BL_PIN, 1);
    }

    // 2. 初始化 LCD
    if (_spi_handle && _io_expander) {
        ESP_LOGI(TAG, "Creating ST7735...");
        _lcd = new ST7735(_spi_handle, _io_expander);
        if (_lcd) {
            _lcd->init();
            // 此时屏幕应该变黑（初始化成功），不再是噪点
        }
    }

    // 3. GPIO
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pin_bit_mask = (1ULL << CYBERPI_BTN_A_GPIO) | (1ULL << CYBERPI_BTN_B_GPIO);
    gpio_config(&io_conf);

    ESP_LOGW(TAG, ">>> Init Complete <<<");
}

void CyberPi::render(const uint16_t* frameBuffer) {
    if (_lcd) {
        _lcd->drawBitmap(frameBuffer);
    }
}

bool CyberPi::isButtonPressed(gpio_num_t btn) {
    return gpio_get_level(btn) == 0;
}

void CyberPi::initI2C() {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = CYBERPI_I2C_SDA;
    conf.scl_io_num = CYBERPI_I2C_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = CYBERPI_I2C_FREQ;
    conf.clk_flags = 0; 
    ESP_ERROR_CHECK(i2c_driver_install(CYBERPI_I2C_PORT, conf.mode, 0, 0, 0));
    ESP_ERROR_CHECK(i2c_param_config(CYBERPI_I2C_PORT, &conf));
}

void CyberPi::initSPI() {
    ESP_LOGI(TAG, "Init SPI: CS=%d (Corrected)", CYBERPI_LCD_CS);
    
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = CYBERPI_SPI_MOSI;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = CYBERPI_SPI_CLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    
    // [关键策略] 启用 DMA，但限制最大传输为 4092 字节 (4KB)
    // 这样驱动程序只需要在内部 RAM 申请 4KB 的中转区，非常安全，不会崩。
    // 同时 DMA 允许传输 > 64字节的数据，解决了 SPI 报错。
    buscfg.max_transfer_sz = 4092; 

    // 启用 DMA (Channel Auto)
    ESP_ERROR_CHECK(spi_bus_initialize(CYBERPI_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 20 * 1000 * 1000; // 恢复 20MHz 高速
    devcfg.mode = 0; 
    devcfg.spics_io_num = CYBERPI_LCD_CS;
    devcfg.queue_size = 7;
    devcfg.flags = 0;
    
    ESP_ERROR_CHECK(spi_bus_add_device(CYBERPI_SPI_HOST, &devcfg, &_spi_handle));
}