/* platform/esp32/main/Host_Implementation.cpp */

#include "host.h"         
#include "CyberPi.hpp"    
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <math.h> 

// ==========================================
// 静态状态管理
// ==========================================

static int g_targetFps = 60;
static int64_t g_lastFrameTime = 0;
static int64_t g_frameDurationUs = 16666;

#define AUDIO_BUF_SIZE 1024
static int16_t g_audioBuffer[AUDIO_BUF_SIZE * 2]; 
//static uint16_t g_lineBuffer[128 * 128];
static uint16_t* g_lineBuffer = nullptr;

// ==========================================
// 平台特定实现 (Partial Implementation)
// ==========================================

Host::Host() : 
    // 初始化成员变量 (参考 host.h 中的默认值)
    currKDown(0), currKHeld(0), currKBDown(false), currKBKey(""),
    lDown(false), rDown(false), stretchKeyPressed(false),
    stretch(PixelPerfectStretch), kbmode(Emoji), resizekey(NoResize),
    menustyle(Fancy), bgcolor(Gray),
    scaleX(1.0), scaleY(1.0), mouseOffsetX(0), mouseOffsetY(0), quit(0)
{
    g_lastFrameTime = esp_timer_get_time();

    if (g_lineBuffer == nullptr) {
        g_lineBuffer = new uint16_t[128 * 128];
        // 简单检查一下内存是否分配成功
        if (g_lineBuffer == nullptr) {
            printf("CRITICAL ERROR: Failed to allocate video buffer!\n");
        } else {
            printf("Video buffer allocated successfully.\n");
        }
    }
}

// 即使 hostCommonFunctions.cpp 没实现析构，这里也要有
// 如果链接报错 multiple def，就注释掉
// 通常 host.h 里没有虚析构函数的实现，所以这里需要
// 除非 hostCommonFunctions.cpp 里有 (我看源码是没有的)
// Host::~Host() {} 
// 注意：如果编译报 undefined reference to vtable，说明必须提供析构函数体
// 但 host.h 里并没有把析构声明为 virtual，所以不需要 vtable

// 系统接口
const char* Host::logFilePrefix() { return ""; }
void Host::overrideLogFilePrefix(const char* prefix) {}
bool Host::shouldRunMainLoop() { return true; }
bool Host::shouldQuit() { return false; }

// 帧率控制
void Host::setTargetFps(int fps) {
    if (fps <= 0) fps = 30;
    g_targetFps = fps;
    g_frameDurationUs = 1000000 / fps;
}

void Host::waitForTargetFps() {
    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - g_lastFrameTime;
    int64_t time_to_wait = g_frameDurationUs - elapsed;

    if (time_to_wait > 0) {
        if (time_to_wait > 2000) {
            vTaskDelay(pdMS_TO_TICKS(time_to_wait / 1000));
        }
        while ((esp_timer_get_time() - g_lastFrameTime) < g_frameDurationUs) {
            asm volatile("nop");
        }
    }
    g_lastFrameTime = esp_timer_get_time();
}

double Host::deltaTMs() {
    return g_frameDurationUs / 1000.0;
}

// 屏幕
void Host::changeStretch() {}
void Host::forceStretch(StretchOption opt) {}

void Host::drawFrame(uint8_t* picoFb, uint8_t* screenPaletteMap, uint8_t drawMode) {
    if (!g_lineBuffer) return;
    
    // 简单颜色转换：P8 Index -> RGB565
    // 为了性能，这部分应该尽量优化，或者由 CyberPi::render 内部处理
    // 这里假设 CyberPi::render 接受 uint16_t buffer
    
    // 获取调色板 (hostCommonFunctions.cpp 实现了 GetPaletteColors)
    Color* palette = GetPaletteColors();
    
    for (int i = 0; i < 128 * 128; i++) {
        uint8_t idx = picoFb[i];
        // 应用屏幕映射 (drawMode 逻辑暂时忽略，或者参考 fake08 源码)
        uint8_t mappedIdx = screenPaletteMap[idx];
        
        Color c = palette[mappedIdx & 0x7F]; // 防止越界
        
        // RGB888 -> RGB565
        uint16_t color565 = ((c.Red & 0xF8) << 8) | ((c.Green & 0xFC) << 3) | (c.Blue >> 3);
        
        // 大端/小端交换 (ESP32 SPI通常需要交换)
        g_lineBuffer[i] = (color565 >> 8) | (color565 << 8); 
    }
    
    CyberPi::getInstance().render(g_lineBuffer); 
}

// 音频
bool Host::shouldFillAudioBuff() { return true; }
void* Host::getAudioBufferPointer() { return g_audioBuffer; }
size_t Host::getAudioBufferSize() { return sizeof(g_audioBuffer); }
void Host::playFilledAudioBuffer() { /* I2S Write */ }

// 初始化
void Host::oneTimeSetup(Audio* audio) {
    // 调用通用实现来初始化调色板数据
    setUpPaletteColors(); 
}
void Host::oneTimeCleanup() {}
void Host::setPlatformParams(int, int, uint32_t, uint32_t, uint32_t, std::string, std::string, std::string) {}

// 文件系统
std::string Host::getCartDirectory() { return ""; }
std::vector<std::string> Host::listcarts() { return {}; }

// 输入
InputState_t Host::scanInput() {
    InputState_t state = {};
    state.KDown = 0;
    state.KHeld = 0;
    
    if (CyberPi::getInstance().isButtonPressed(CYBERPI_BTN_A_GPIO)) {
        state.KDown |= (1 << 4);
        state.KHeld |= (1 << 4);
    }
    if (CyberPi::getInstance().isButtonPressed(CYBERPI_BTN_B_GPIO)) {
        state.KDown |= (1 << 5);
        state.KHeld |= (1 << 5);
    }
    return state;
}

// BIOS 注入
std::string Host::customBiosLua() {
     return R"(
        frame_count = 0
        function _init()
            printh("LUA: Host Implementation Active")
        end
        function _update()
            frame_count = frame_count + 1
            if (frame_count % 30 == 0) then
                printh("LUA: Ping " .. frame_count)
            end
        end
        function _draw()
            cls(12)
            print("native host impl", 30, 60, 7)
            circfill(64, 64 + sin(frame_count/30)*10, 5, 8)
        end
    )";
}