#include "drivers/aw9523b.h"
#include "esp_log.h"

static const char* TAG = "AW9523B";

// ... 寄存器定义保持不变 ...
#define REG_OUTPUT0     0x02
#define REG_OUTPUT1     0x03
#define REG_CONFIG0     0x04
#define REG_CONFIG1     0x05
#define REG_SWRST       0x7F
#define REG_ID          0x10

AW9523B::AW9523B(i2c_port_t port, uint8_t addr) 
    : _i2c_port(port), _addr(addr) {
    _out_reg[0] = 0;
    _out_reg[1] = 0;
    _conf_reg[0] = 0xFF;
    _conf_reg[1] = 0xFF;
}

bool AW9523B::begin() {
    ESP_LOGI(TAG, "Resetting AW9523B...");
    writeReg(REG_SWRST, 0x00);
    
    // 检查 ID
    uint8_t id = readReg(REG_ID);
    ESP_LOGI(TAG, "Read Chip ID: 0x%02X (Expected 0x23)", id);
    
    if (id != 0x23) {
        ESP_LOGE(TAG, "ID check failed! Is the chip connected?");
        // 这里不要 return false，有些兼容片 ID 可能不同，
        // 或者因为上电时序问题没读到，强行往下走试试。
    }

    // 配置端口为推挽模式
    ESP_LOGI(TAG, "Configuring Push-Pull mode...");
    writeReg(0x11, 0x10); // P0
    writeReg(0x12, 0x10); // P1

    return true;
}

void AW9523B::pinMode(uint8_t port, uint8_t pin, uint8_t mode) {
    // ... 保持不变 ...
    uint8_t* reg_val = &_conf_reg[port];
    if (mode == 0) *reg_val &= ~(1 << pin);
    else *reg_val |= (1 << pin);
    writeReg((port == 0) ? REG_CONFIG0 : REG_CONFIG1, *reg_val);
}

void AW9523B::digitalWrite(uint8_t port, uint8_t pin, uint8_t level) {
    // [调试] 如果觉得有问题，可以在这里加 ESP_LOGD 打印引脚操作
    uint8_t* reg_val = &_out_reg[port];
    if (level) *reg_val |= (1 << pin);
    else *reg_val &= ~(1 << pin);
    writeReg((port == 0) ? REG_OUTPUT0 : REG_OUTPUT1, *reg_val);
}

// ... readReg, writeReg 等保持不变 ...
void AW9523B::writeReg(uint8_t reg, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    // 增加超时时间以防万一
    esp_err_t ret = i2c_master_cmd_begin(_i2c_port, cmd, pdMS_TO_TICKS(50));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C Write Failed: Reg 0x%02X, Data 0x%02X, Err %d", reg, data, ret);
    }
    i2c_cmd_link_delete(cmd);
}

uint8_t AW9523B::readReg(uint8_t reg) {
    uint8_t data = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(_i2c_port, cmd, pdMS_TO_TICKS(50));
     if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C Read Failed: Reg 0x%02X, Err %d", reg, ret);
    }
    i2c_cmd_link_delete(cmd);
    return data;
}