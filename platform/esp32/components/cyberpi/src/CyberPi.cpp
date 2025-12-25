#include "CyberPi.h"
#include "esp_log.h"
#include "driver/i2c.h"

static const char* TAG = "CyberPi";

// AW9523B 寄存器地址
#define REG_INPUT_P0        0x00
#define REG_INPUT_P1        0x01
#define REG_OUTPUT_P0       0x02
#define REG_OUTPUT_P1       0x03
#define REG_CONFIG_P0       0x04 // 0=Output, 1=Input
#define REG_CONFIG_P1       0x05
#define REG_CTRL            0x11 // GCR
#define REG_WORK_MODE_P0    0x12 // 0=GPIO, 1=LED
#define REG_WORK_MODE_P1    0x13
#define REG_SWRST           0x7F

CyberPi::CyberPi() : 
    _io_expander(nullptr), 
    _lcd(nullptr), 
    _spi_handle(nullptr),
    _cached_input_state(0xFFFF) // 默认未按下 (High)
{}

CyberPi& CyberPi::getInstance() {
    static CyberPi instance;
    return instance;
}

void CyberPi::init() {
    ESP_LOGI(TAG, ">>> CyberPi Init (Config Driven) <<<");

    initI2C();
    initSPI();

    // 创建 AW9523B 对象 (仅用于封装，底层配置下文手动完成)
    _io_expander = new AW9523B(CYBERPI_I2C_PORT, AW9523B_ADDR_LCD);

    // ============================================================
    // AW9523B 手动初始化序列 (解决黑屏和按键问题)
    // ============================================================
    ESP_LOGI(TAG, "Configuring AW9523B (0x%02X)...", AW9523B_ADDR_LCD);
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523B_ADDR_LCD << 1) | I2C_MASTER_WRITE, true);

    // 1. 软件复位 (Soft Reset)
    i2c_master_write_byte(cmd, REG_SWRST, true);
    i2c_master_write_byte(cmd, 0x00, true);

    // 2. 设置推挽输出 (Push-Pull) - 增强 LCD 信号驱动能力
    i2c_master_write_byte(cmd, REG_CTRL, true);
    i2c_master_write_byte(cmd, 0x10, true); 

    // 3. 强制进入 GPIO 模式 (关闭 LED 扩展模式)
    i2c_master_write_byte(cmd, REG_WORK_MODE_P0, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write_byte(cmd, 0x00, true);

    // 4. [关键] 预置输出电平为 HIGH
    // 必须在设置 Direction 之前执行，防止 Output 变为 Low 拉低 LCD 控制线
    i2c_master_write_byte(cmd, REG_OUTPUT_P0, true);
    i2c_master_write_byte(cmd, 0xFF, true); // Port 0: 释放 Input 线 (Open Drain safe)
    i2c_master_write_byte(cmd, 0xFF, true); // Port 1: BL=1(亮), RST=1(不复位), DC=1

    // 5. 配置 IO 方向 (0=Output, 1=Input)
    i2c_master_write_byte(cmd, REG_CONFIG_P0, true);
    
    // --- Port 0 配置 ---
    // 所有游戏按键 (Left, Up, Right, Center, Down, A, B) 都在 Port 0
    // 全部设为 Input (0xFF)
    i2c_master_write_byte(cmd, 0xFF, true); 
    
    // --- Port 1 配置 (混合模式) ---
    // Output: AMP(3), DC(4), RST(5), BL(7) -> bit=0
    // Input:  Menu(0) -> bit=1
    // Unused: 1, 2, 6 -> 设为 1 (Input) 安全
    
    uint8_t p1_config = 0xFF;
    p1_config &= ~(1 << AW_AMP_EN_PIN); // Output
    p1_config &= ~(1 << AW_LCD_DC_PIN); // Output
    p1_config &= ~(1 << AW_LCD_RST_PIN);// Output
    p1_config &= ~(1 << AW_LCD_BL_PIN); // Output
    // 此时 Menu(Bit 0) 依然是 1 (Input)
    
    i2c_master_write_byte(cmd, p1_config, true); // 写入配置 (约为 0x47)

    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(CYBERPI_I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AW9523B Init Failed! I2C Error: %d", ret);
    } else {
        ESP_LOGI(TAG, "AW9523B Configured. P1 Config Mask: 0x%02X", p1_config);
    }

    // 4. 初始化 LCD
    if (_spi_handle && _io_expander) {
        ESP_LOGI(TAG, "Initializing ST7735...");
        _lcd = new ST7735(_spi_handle, _io_expander);
        if (_lcd) {
            _lcd->init();
        }
    }
}

// ==========================================
// 输入读取逻辑 (符合 Active Low)
// ==========================================
void CyberPi::updateInputState() {
    if (!_io_expander) return;

    uint8_t data[2] = {0xFF, 0xFF}; 

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523B_ADDR_LCD << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, REG_INPUT_P0, true); // 从 P0 Input 开始读
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AW9523B_ADDR_LCD << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK); // 连续读 P0, P1
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(CYBERPI_I2C_PORT, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        // data[0] 是 Port 0, data[1] 是 Port 1
        _cached_input_state = data[0] | (data[1] << 8);
    }
}

bool CyberPi::isButtonPressed(uint8_t pin_index) {
    // 硬件逻辑: 按下 = 低电平(0), 松开 = 高电平(1)
    bool val = (_cached_input_state >> pin_index) & 1;
    return !val; // 如果读到 0，返回 true (Pressed)
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