#include "CyberPi.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char* TAG = "CyberPi";

// ==========================================
//    构造函数 & 单例
// ==========================================
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

// ==========================================
//    初始化流程 (修复黑屏 + 输入)
// ==========================================
void CyberPi::init() {
    ESP_LOGI(TAG, ">>> CyberPi System Init (LCD Fix + Input) <<<");

    initI2C();
    initSPI();

    // 1. 初始化 AW9523B 对象
    // 我们先创建对象，让它建立基础连接
    _io_expander = new AW9523B(CYBERPI_I2C_PORT, AW9523B_ADDR_LCD);
    
    // 2. [关键修复] 手动配置混合模式
    // 必须确保背光 (BL) 和复位 (RST) 在初始化时处于高电平
    // --------------------------------------------------------
    ESP_LOGI(TAG, "Configuring AW9523B Registers...");
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523B_ADDR_LCD << 1) | I2C_MASTER_WRITE, true);

    // [A] 退出 LED 模式，进入 GPIO 模式 (Reg 0x12, 0x13)
    i2c_master_write_byte(cmd, 0x12, true);
    i2c_master_write_byte(cmd, 0x00, true); // P0 GPIO
    i2c_master_write_byte(cmd, 0x00, true); // P1 GPIO

    // [B] 预置输出电平 (Reg 0x02, 0x03) -> 必须先做这个！
    // 如果先设方向再设电平，可能会瞬间拉低 RST 导致白屏/黑屏
    // 我们将所有端口预置为 1 (High)。
    // 对于 Output: BL=On, RST=Inactive, DC=Data, AMP=On
    // 对于 Input: 开启弱上拉或无效
    i2c_master_write_byte(cmd, 0x02, true);
    i2c_master_write_byte(cmd, 0xFF, true); // Port 0: All High
    i2c_master_write_byte(cmd, 0xFF, true); // Port 1: All High (BL/RST/DC = 1)

    // [C] 配置输入输出方向 (Reg 0x04, 0x05)
    // 0 = Output, 1 = Input
    i2c_master_write_byte(cmd, 0x04, true); 
    
    // Port 0: 全是游戏按键 -> 0xFF (Input)
    i2c_master_write_byte(cmd, 0xFF, true); 
    
    // Port 1: 混合模式
    // Menu(0)=Input(1)
    // AMP(3), DC(4), RST(5), BL(7) = Output(0)
    // Unused(1,2,6) = Input(1)
    // Val: 0100 0111 -> 0x47
    i2c_master_write_byte(cmd, 0x47, true); 

    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(CYBERPI_I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AW9523B Config Failed! Screen may not light up.");
    } else {
        ESP_LOGI(TAG, "AW9523B Configured: BL On, Inputs Ready.");
    }

    // 3. 再次确认背光开启 (通过对象接口双重保险)
    if (_io_expander) {
        // AW_LCD_BL_PIN 是 7 (P1_7)
        // 注意：AW9523B 库可能需要知道我们是在操作 Port 1
        // 假设库的 pinMode/digitalWrite 处理了端口逻辑
        // 如果库比较简单，可能需要传递 8+7=15，或者直接调用
        
        // 既然我们上面手动写寄存器已经拉高了，这里主要为了同步 internal state
        // 暂时不调用库函数以免覆盖配置，ST7735 init 会接管 RST/DC
    }

    // 4. 初始化 ST7735 LCD
    if (_spi_handle && _io_expander) {
        ESP_LOGI(TAG, "Initializing ST7735...");
        _lcd = new ST7735(_spi_handle, _io_expander);
        if (_lcd) {
            _lcd->init();
            // init 内部会 toggle RST，如果方向配置对了，这里应该能正常复位
        }
    }
}

// ==========================================
//    输入处理逻辑 (保持不变，已验证逻辑正确)
// ==========================================
void CyberPi::updateInputState() {
    if (!_io_expander) return;

    uint8_t data[2] = {0xFF, 0xFF}; 

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523B_ADDR_LCD << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true); // Reg 0x00
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523B_ADDR_LCD << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(CYBERPI_I2C_PORT, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        // Low Byte=P0, High Byte=P1
        _cached_input_state = data[0] | (data[1] << 8);
    }
}

bool CyberPi::isButtonPressed(uint8_t pin_index) {
    // 0 = Pressed, 1 = Released
    return !((_cached_input_state >> pin_index) & 1);
}

// ... render, initI2C, initSPI 保持不变 ...
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
    buscfg.max_transfer_sz = 4092; 
    ESP_ERROR_CHECK(spi_bus_initialize(CYBERPI_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 20 * 1000 * 1000;
    devcfg.mode = 0; 
    devcfg.spics_io_num = CYBERPI_LCD_CS;
    devcfg.queue_size = 7;
    devcfg.flags = 0;
    ESP_ERROR_CHECK(spi_bus_add_device(CYBERPI_SPI_HOST, &devcfg, &_spi_handle));
}