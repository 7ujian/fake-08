#include "CyberPi.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include <cstring>

static const char* TAG = "CyberPi";

// AW9523B 寄存器地址
#define REG_INPUT_P0      0x00
#define REG_INPUT_P1      0x01
#define REG_OUTPUT_P0     0x02
#define REG_OUTPUT_P1     0x03
#define REG_CONFIG_P0     0x04 
#define REG_CONFIG_P1     0x05
#define REG_GCR           0x11 
#define REG_LED_MODE_P0   0x12 
#define REG_LED_MODE_P1   0x13
#define REG_SOFT_RST      0x7F

CyberPi::CyberPi() : 
    _io_expander(nullptr), 
    _lcd(nullptr), 
    _spi_handle(nullptr),
    _cached_input_state(0xFFFF) 
{}

CyberPi& CyberPi::getInstance() {
    static CyberPi instance;
    return instance;
}

void CyberPi::init() {
    ESP_LOGI(TAG, ">>> CyberPi Hardware Init (Sync Fix) <<<");

    initI2C();
    initSPI();

    // 创建对象 (内部 pinDataP1 初始为 0)
    _io_expander = new AW9523B(CYBERPI_I2C_PORT, AW_ADDR_IO_LCD);

    // ============================================================
    // 1. 底层寄存器配置 (确保模式正确)
    // ============================================================
    ESP_LOGI(TAG, "Configuring IO Chip (0x%02X)...", AW_ADDR_IO_LCD);
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW_ADDR_IO_LCD << 1) | I2C_MASTER_WRITE, true);

    // [A] 软件复位
    i2c_master_write_byte(cmd, REG_SOFT_RST, true);
    i2c_master_write_byte(cmd, 0x00, true);

    // [B] GPIO 模式 (关键：必须设为 FF，否则是 LED 模式)
    i2c_master_write_byte(cmd, REG_LED_MODE_P0, true);
    i2c_master_write_byte(cmd, 0xFF, true); 
    i2c_master_write_byte(cmd, 0xFF, true); 

    // [C] 推挽输出 (GCR = 0x10) - 增强驱动能力
    i2c_master_write_byte(cmd, REG_GCR, true);
    i2c_master_write_byte(cmd, 0x10, true); 

    // [D] 预置输出 High
    i2c_master_write_byte(cmd, REG_OUTPUT_P0, true);
    i2c_master_write_byte(cmd, 0xFF, true); 
    i2c_master_write_byte(cmd, 0xFF, true); 

    // [E] 配置方向 (P0=In, P1=Mixed)
    i2c_master_write_byte(cmd, REG_CONFIG_P0, true);
    i2c_master_write_byte(cmd, 0xFF, true); // P0 Input
    
    // P1 Config: 0x47 (Menu=Input, others Output)
    // Mask: 0100 0111
    i2c_master_write_byte(cmd, 0x47, true); 

    i2c_master_stop(cmd);
    i2c_master_cmd_begin(CYBERPI_I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    // ============================================================
    // 2. [关键修复] 同步驱动对象内部状态
    // ============================================================
    // ST7735 init 会调用 digitalWrite，如果驱动内部缓存是 0，
    // 它会把 MENU 引脚 (P1_0) 误写为 0。
    // 我们这里显式把 P1 所有引脚设为 HIGH，更新内部缓存 pinDataP1。
    
    if (_io_expander) {
        ESP_LOGI(TAG, "Syncing driver state to HIGH...");
        // Port 1 共有 8 个脚 (0-7)
        // AW_LCD_PORT 是 1
        for (int i = 0; i < 8; i++) {
            _io_expander->digitalWrite(AW_LCD_PORT, i, 1);
        }
        // 此时 pinDataP1 变为 0xFF，之后 ST7735 修改 RST 位时，不会影响 Menu 位
    }

    // 3. 初始化 LCD
    if (_spi_handle && _io_expander) {
        ESP_LOGI(TAG, "Initializing ST7735...");
        _lcd = new ST7735(_spi_handle, _io_expander);
        if (_lcd) {
            _lcd->init();
        }
    }
}

// updateInputState 和 isButtonPressed 保持不变 (逻辑正确)
void CyberPi::updateInputState() {
    if (!_io_expander) return;

    uint8_t data[2] = {0xFF, 0xFF}; 

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW_ADDR_IO_LCD << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, REG_INPUT_P0, true); 
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW_ADDR_IO_LCD << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK); 
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(CYBERPI_I2C_PORT, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        _cached_input_state = data[0] | (data[1] << 8);
    }
}

bool CyberPi::isButtonPressed(uint8_t pin_index) {
    return !((_cached_input_state >> pin_index) & 1);
}

void CyberPi::render(const uint16_t* frameBuffer) {
    if (_lcd) _lcd->drawBitmap(frameBuffer);
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
    ESP_LOGI(TAG, "Init SPI: CS=%d", CYBERPI_LCD_CS);
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = CYBERPI_SPI_MOSI;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = CYBERPI_SPI_CLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;

    ESP_ERROR_CHECK(spi_bus_initialize(CYBERPI_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 20 * 1000 * 1000;
    devcfg.mode = 0; 
    devcfg.spics_io_num = CYBERPI_LCD_CS;
    devcfg.queue_size = 7;
    devcfg.flags = SPI_DEVICE_HALFDUPLEX;
    ESP_ERROR_CHECK(spi_bus_add_device(CYBERPI_SPI_HOST, &devcfg, &_spi_handle));
}