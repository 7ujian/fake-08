/* platform/esp32/main/Host_Implementation.cpp */

#include "host.h"         
#include "CyberPi.h"    
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

// 在 drawFrame 函数之前添加这个 helper，或者作为类的私有函数
// PICO-8 内存布局: 低4位是左像素(偶数X)，高4位是右像素(奇数X)
inline uint8_t getPixelNibble(int x, int y, const uint8_t* picoFb) {
    // 128 像素宽 = 64 字节宽
    // y * 64 找到行首，x / 2 找到列字节
    int index = (y * 64) + (x >> 1);
    uint8_t byte = picoFb[index];
    
    // 如果 x 是奇数 (x&1)，取高 4 位；如果是偶数，取低 4 位
    return (x & 1) ? (byte >> 4) : (byte & 0x0F);
}

void Host::drawFrame(uint8_t* picoFb, uint8_t* screenPaletteMap, uint8_t drawMode) {
    // 安全检查
    if (!g_lineBuffer) return;

    // 获取 fake08 内置的 RGB888 调色板
    Color* basePalette = GetPaletteColors(); 

    // [性能优化] 预计算当前帧的 16 色 RGB565 查找表
    // 这样我们只需要做 16 次 RGB转换，而不是 16384 次
    uint16_t paletteLut[16];

    for (int i = 0; i < 16; i++) {
        // 处理 PICO-8 的 pal() 指令映射 (screenPaletteMap)
        // SDL 代码里也是这样做的: _paletteColors[screenPaletteMap[c]]
        uint8_t mappedIdx = screenPaletteMap[i] & 0x0F; // 限制在 0-15
        
        Color c = basePalette[mappedIdx];

        // RGB888 (Color struct) -> RGB565 (uint16_t)
        // 转换公式: R(5) << 11 | G(6) << 5 | B(5)
        uint16_t color565 = ((c.Red & 0xF8) << 8) | ((c.Green & 0xFC) << 3) | (c.Blue >> 3);

        // [重要] 字节序交换 (Endian Swap)
        // ESP32 的 SPI 通常需要大端序发送 (High byte first)
        // 将 0xRRGG 变成 0xGGRR
        paletteLut[i] = (color565 >> 8) | (color565 << 8);
    }

    // 像素填充循环
    int pixelIdx = 0;
    
    // 注意：PICO-8 标准分辨率是 128x128
    // 你的 SDL 代码用了 PicoScreenHeight/Width，这里直接用 128 以简化
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            // [关键修正] 使用 getPixelNibble 解包 4-bit 像素
            uint8_t colorIdx = getPixelNibble(x, y, picoFb);
            
            // 查表并写入
            g_lineBuffer[pixelIdx++] = paletteLut[colorIdx];
        }
    }

    // 推送给 CyberPi 硬件
    CyberPi::getInstance().render(g_lineBuffer);
}
// 音频
bool Host::shouldFillAudioBuff() { return false; }
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
    
    // 1. 初始化清零
    state.KDown = 0;
    state.KHeld = 0;
    state.mouseX = 0;
    state.mouseY = 0;
    state.mouseBtnState = 0;
    state.KBdown = false;
    state.KBkey = "";

    // 2. 驱动层刷新：通过 I2C 读取 AW9523B 状态
    CyberPi& device = CyberPi::getInstance();
    device.updateInputState();

    // 3. 映射逻辑：物理按键 -> PICO-8 标准位掩码
    
    // --- 方向键 ---
    if (device.isButtonPressed(CYBERPI_KEY_LEFT)) {
        state.KDown |= P8_KEY_LEFT;
        state.KHeld |= P8_KEY_LEFT;
    }
    if (device.isButtonPressed(CYBERPI_KEY_RIGHT)) {
        state.KDown |= P8_KEY_RIGHT;
        state.KHeld |= P8_KEY_RIGHT;
    }
    if (device.isButtonPressed(CYBERPI_KEY_UP)) {
        state.KDown |= P8_KEY_UP;
        state.KHeld |= P8_KEY_UP;
    }
    if (device.isButtonPressed(CYBERPI_KEY_DOWN)) {
        state.KDown |= P8_KEY_DOWN;
        state.KHeld |= P8_KEY_DOWN;
    }

    // --- 动作键 ---
    // 物理 A -> PICO-8 O 键 (Button 4)
    if (device.isButtonPressed(CYBERPI_KEY_A)) {
        state.KDown |= P8_KEY_O;
        state.KHeld |= P8_KEY_O;
    }
    
    // 摇杆中键 -> 也可以映射为 O 键 (确认)
    if (device.isButtonPressed(CYBERPI_KEY_CENTER)) {
        state.KDown |= P8_KEY_O;
        state.KHeld |= P8_KEY_O;
    }

    // 物理 B -> PICO-8 X 键 (Button 5)
    if (device.isButtonPressed(CYBERPI_KEY_B)) {
        state.KDown |= P8_KEY_X;
        state.KHeld |= P8_KEY_X;
    }

    // --- 系统键 ---
    // 菜单键 -> PICO-8 暂停/菜单 (Button 6)
    if (device.isButtonPressed(CYBERPI_KEY_MENU)) {
        state.KDown |= P8_KEY_PAUSE;
        state.KHeld |= P8_KEY_PAUSE;
    }

    return state;
}

// BIOS 注入
std::string Host::customBiosLua() { return ""; }