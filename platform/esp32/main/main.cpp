#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include <new>
#include "esp_log.h"
#include "esp_timer.h"

// Fake08 核心头文件
#include "CyberPi.h"      // 包含 updateInputState, isButtonPressed
#include "cyberpi_config.h" // 包含 CYBERPI_KEY_* 定义
#include "Vm.h"
#include "Host.h"
#include "Graphics.h"
#include "Input.h"
#include "Audio.h"
#include "PicoRam.h"
#include "Cart.h"

#include "embedded_carts.h"

static const char* TAG = "Main";

#define CHECK_ALLOC(ptr, name) \
    if (!ptr) { \
        printf("CRITICAL ERROR: Failed to allocate %s\n", name); \
        return; \
    }

    #define NEW_IN_SRAM(TYPE, VAR, ...) \
    void* raw_##VAR = heap_caps_malloc(sizeof(TYPE), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); \
    TYPE* VAR = nullptr; \
    if (raw_##VAR) { \
        printf("[%s] Allocating " #TYPE " in SRAM (%d bytes) at %p\n", TAG, sizeof(TYPE), raw_##VAR); \
        VAR = new (raw_##VAR) TYPE(__VA_ARGS__); \
    } else { \
        printf("[%s] SRAM Full! Fallback " #TYPE " to default heap\n", TAG); \
        VAR = new TYPE(__VA_ARGS__); \
    } \
    if (!VAR) { printf("[%s] Critical: Failed to allocate " #TYPE "\n", TAG); abort(); }

// =============================================================
// 硬件自检函数 (更新版)
// =============================================================
void run_hardware_selftest() {
    printf("\n\n--- [SELF-TEST] STARTING HARDWARE DIAGNOSTICS ---\n");
    
    CyberPi& device = CyberPi::getInstance();

    // 1. 屏幕测试 (Red/Green/Blue)
    // ---------------------------------------------------------
    printf("[SelfTest] Testing Display Output...\n");
    size_t buf_count = 128 * 128;
    uint16_t* test_buffer = (uint16_t*)heap_caps_malloc(buf_count * sizeof(uint16_t), MALLOC_CAP_8BIT);

    if (test_buffer) {
        // Red
        for (int i = 0; i < buf_count; i++) test_buffer[i] = 0xF800;
        device.render(test_buffer);
        vTaskDelay(pdMS_TO_TICKS(500));

        // Green
        for (int i = 0; i < buf_count; i++) test_buffer[i] = 0x07E0;
        device.render(test_buffer);
        vTaskDelay(pdMS_TO_TICKS(500));

        // Blue
        for (int i = 0; i < buf_count; i++) test_buffer[i] = 0x001F;
        device.render(test_buffer);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        free(test_buffer);
    } else {
        printf("[SelfTest] Failed to allocate display buffer.\n");
    }

    // 2. 输入测试 (Input Test) - 适配 I2C AW9523B
    // ---------------------------------------------------------
    printf("\n[SelfTest] Testing Input Buttons (5 seconds)...\n");
    printf("  -> Press buttons now. If nothing logs, check I2C/AW9523B config.\n");
    
    int64_t start_time = esp_timer_get_time();
    const int64_t duration = 5000000; // 5秒
    
    while (esp_timer_get_time() - start_time < duration) {
        // [关键] 必须先刷新 I2C 状态
        device.updateInputState();
        
        // 使用 cyberpi_config.h 中的宏 (0-15)
        if (device.isButtonPressed(CYBERPI_KEY_UP))     printf("  [KEY] UP\n");
        if (device.isButtonPressed(CYBERPI_KEY_DOWN))   printf("  [KEY] DOWN\n");
        if (device.isButtonPressed(CYBERPI_KEY_LEFT))   printf("  [KEY] LEFT\n");
        if (device.isButtonPressed(CYBERPI_KEY_RIGHT))  printf("  [KEY] RIGHT\n");
        if (device.isButtonPressed(CYBERPI_KEY_CENTER)) printf("  [KEY] CENTER (JOY)\n");
        if (device.isButtonPressed(CYBERPI_KEY_A))      printf("  [KEY] A\n");
        if (device.isButtonPressed(CYBERPI_KEY_B))      printf("  [KEY] B\n");
        if (device.isButtonPressed(CYBERPI_KEY_MENU))   printf("  [KEY] MENU\n");
        
        vTaskDelay(pdMS_TO_TICKS(100)); // 避免刷屏太快
    }
    printf("[SelfTest] Input Test Complete.\n");
    printf("--- [SELF-TEST] FINISHED ---\n\n");
}

extern "C" void app_main(void)
{
    printf("\n\n=== CyberPi PICO-8 Emulator Booting ===\n");
    
    // 1. 底层硬件初始化
    // 确保你的 CyberPi::init() 里已经修复了 AW9523B 的输入方向配置 (Write 0xFF to Reg 0x04/0x05)
    CyberPi::getInstance().init();
    
   // 1. 创建 PicoRam (最关键的数据，64KB)
    // 注意：PicoRam 的无参构造函数
    NEW_IN_SRAM(PicoRam, memory);
    memset(raw_memory, 0, sizeof(PicoRam)); // 确保清零

    // 2. 创建 Host
    // 假设 Host 无参构造
    NEW_IN_SRAM(Host, host);

    // 3. 创建 Audio
    // Audio 构造需要 memory 指针
    NEW_IN_SRAM(Audio, audio, memory);

    // 4. 创建 VM
    // Vm 构造函数签名: Vm(Host* host, PicoRam* ram, uint8_t* bios, char* custom_gpu, Audio* audio)
    printf("[%s] Creating Virtual Machine...\n", TAG);
    NEW_IN_SRAM(Vm, vm, host, memory, nullptr, nullptr, audio);

    // 7. 平台设置
    host->oneTimeSetup(nullptr);

    // const unsigned char* cart_start = (const unsigned char*) bunnysurvivor_p8_start;
    // const unsigned char* cart_end = (const unsigned char*) bunnysurvivor_p8_end;

    // size_t cart_size = cart_end - cart_start;
    // printf("Loading embedded cart, size: %d bytes\n", cart_size);

    Cart* testCart = LoadEmbeddedCart(CartID::cpu_test_p8);

    // // 8. 生成测试卡带
    // printf("[%s] Generating Test Cart...\n", TAG);
    // Cart* testCart = new Cart();
    
    // // Lua 测试脚本: 移动红点，变色背景
    // testCart->LuaString = R"(
    //     x = 60
    //     y = 60
    //     col = 1
        
    //     function _init()
    //         cls(1)
    //         print("INPUT TEST READY")
    //     end

    //     function _update()
    //         -- Input Test
    //         if (btn(0)) then x = x - 1 end -- Left
    //         if (btn(1)) then x = x + 1 end -- Right
    //         if (btn(2)) then y = y - 1 end -- Up
    //         if (btn(3)) then y = y + 1 end -- Down
            
    //         -- Button O (A) -> Red Background
    //         if (btn(4)) then col = 8 end
    //         -- Button X (B) -> Blue Background
    //         if (btn(5)) then col = 12 end
    //     end

    //     function _draw()
    //         cls(col)
    //         rectfill(x, y, x+8, y+8, 7) -- White box
    //         print("X:"..x.." Y:"..y, 0, 0, 7)
            
    //         -- Visual feedback for buttons
    //         if (btn(0)) print("LEFT", 0, 10, 6)
    //         if (btn(1)) print("RIGHT", 40, 10, 6)
    //         if (btn(2)) print("UP", 80, 10, 6)
    //         if (btn(3)) print("DOWN", 0, 20, 6)
    //         if (btn(4)) print("BTN O", 40, 20, 6)
    //         if (btn(5)) print("BTN X", 80, 20, 6)
    //     end
    // )";

    // 9. 加载卡带
    if (vm->loadCart(testCart)) {
        printf("[%s] Cart Loaded.\n", TAG);
    } else {
        printf("[%s] Cart Load Failed.\n", TAG);
    }

    // 10. 启动主循环
    printf("[%s] Entering GameLoop...\n", TAG);
    vm->GameLoop();
}