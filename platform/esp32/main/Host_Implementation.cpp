/* platform/esp32/main/Host_Implementation.cpp */

#include "Host.h"
#include "CyberPi.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "cyberpi_config.h" 

// ==========================================
// 1. 静态全局资源
// ==========================================

static uint16_t* g_lineBuffer = nullptr;

// 帧率控制
static int64_t s_lastFrameTime = 0;
static int64_t s_frameDurationUs = 16666; 

// FPS 统计
static bool    s_showFps = DEFAULT_SHOW_FPS; 
static int     s_currentFps = 0;
static int     s_frameCount = 0;
static int64_t s_fpsTimer = 0;

// 输入状态缓存 (用于实现 KDown/btnp 的上升沿检测)
static uint8_t s_lastHeldState = 0; 

// 音频缓冲区
#define AUDIO_BUF_SIZE 1024
static int16_t g_audioBuffer[AUDIO_BUF_SIZE * 2]; 

// ==========================================
// 2. 静态辅助函数
// ==========================================

// 极简 3x5 数字字体
static const uint8_t MINI_DIGITS[10][3] = {
    {0x1F, 0x11, 0x1F}, // 0
    {0x00, 0x1F, 0x00}, // 1
    {0x1D, 0x15, 0x17}, // 2
    {0x15, 0x15, 0x1F}, // 3
    {0x07, 0x04, 0x1F}, // 4
    {0x17, 0x15, 0x1D}, // 5
    {0x1F, 0x15, 0x1D}, // 6
    {0x01, 0x01, 0x1F}, // 7
    {0x1F, 0x15, 0x1F}, // 8
    {0x17, 0x15, 0x1F}  // 9
};

// 在显存中绘制数字
static void drawFpsNumber(int x, int y, int num, uint16_t color, uint16_t* buffer) {
    // 1. 安全边界检查：防止负数导致数组越界 (Crash来源之一)
    if (num < 0) num = 0; 
    if (num > 99) num = 99;

    int digits[2] = {num / 10, num % 10};
    int currentX = x;

    for (int d = 0; d < 2; d++) {
        int digit = digits[d];
        // 去掉前导零 (如果是十位且是0，跳过绘制，只移动光标)
        if (d == 0 && digit == 0) { 
            currentX += 4; 
            continue; 
        } 

        for (int col = 0; col < 3; col++) {
            uint8_t colData = MINI_DIGITS[digit][col];
            for (int row = 0; row < 5; row++) {
                // 判断当前位是否为 1
                if ((colData >> row) & 0x01) {
                    int px = currentX + col;
                    
                    // [修正点] 之前是 y + (4 - row)，导致上下颠倒
                    // 现在改为 y + row，确保 Bit 0 画在最上面
                    int py = y + row; 
                    
                    if (px >= 0 && px < 128 && py >= 0 && py < 128) {
                        buffer[py * 128 + px] = color;
                    }
                }
            }
        }
        currentX += 4; // 数字间隔
    }
}

inline uint8_t getPixelNibble(int x, int y, const uint8_t* picoFb) {
    int index = (y * 64) + (x >> 1);
    uint8_t byte = picoFb[index];
    return (x & 1) ? (byte >> 4) : (byte & 0x0F);
}

// ==========================================
// 3. Host 类实现
// ==========================================

Host::Host() : 
    currKDown(0), currKHeld(0), currKBDown(false), currKBKey(""),
    lDown(false), rDown(false), stretchKeyPressed(false),
    stretch(PixelPerfectStretch), kbmode(Emoji), resizekey(NoResize),
    menustyle(Fancy), bgcolor(Gray),
    scaleX(1.0), scaleY(1.0), mouseOffsetX(0), mouseOffsetY(0), quit(0)
{
    setUpPaletteColors();
    s_lastFrameTime = esp_timer_get_time();
    s_fpsTimer = s_lastFrameTime;

    if (g_lineBuffer == nullptr) {
        size_t bufSize = 128 * 128 * sizeof(uint16_t);
        g_lineBuffer = (uint16_t*)heap_caps_malloc(bufSize, MALLOC_CAP_8BIT);
        if (g_lineBuffer) {
            memset(g_lineBuffer, 0, bufSize);
        }
    }
}

// 系统接口
const char* Host::logFilePrefix() { return ""; }
void Host::overrideLogFilePrefix(const char* prefix) {}
bool Host::shouldRunMainLoop() { return true; }
bool Host::shouldQuit() { return false; }

// 帧率控制
void Host::setTargetFps(int fps) {
    if (fps <= 0) fps = 30;
    s_frameDurationUs = 1000000 / fps;
}

void Host::waitForTargetFps() {
    int64_t now = esp_timer_get_time();
    
    // FPS 统计
    s_frameCount++;
    if (now - s_fpsTimer >= 1000000) {
        s_currentFps = s_frameCount;
        s_frameCount = 0;
        s_fpsTimer = now;
    }

    // // 延时逻辑
    // if (s_lastFrameTime == 0) {
    //     s_lastFrameTime = now;
    //     return;
    // }

    int64_t elapsed = now - s_lastFrameTime;
    int64_t time_to_wait = s_frameDurationUs - elapsed;

    if (time_to_wait > 0) {
        uint32_t ms = time_to_wait / 1000;
        if (ms == 0) ms = 1;
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        vTaskDelay(1);
    }
    
    s_lastFrameTime = now;
}

double Host::deltaTMs() {
    return s_frameDurationUs / 1000.0;
}

// 屏幕
void Host::changeStretch() {}
void Host::forceStretch(StretchOption opt) {}

void Host::drawFrame(uint8_t* picoFb, uint8_t* screenPaletteMap, uint8_t drawMode) {
    if (!g_lineBuffer) return;

    Color* basePalette = GetPaletteColors(); 
    uint16_t paletteLut[16];

    for (int i = 0; i < 16; i++) {
        uint8_t mappedIdx = screenPaletteMap[i] & 0x0F; 
        Color c = basePalette[mappedIdx];
        uint16_t color565 = ((c.Red & 0xF8) << 8) | ((c.Green & 0xFC) << 3) | (c.Blue >> 3);
        paletteLut[i] = (color565 >> 8) | (color565 << 8);
    }

    int pixelIdx = 0;
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            uint8_t colorIdx = getPixelNibble(x, y, picoFb);
            g_lineBuffer[pixelIdx++] = paletteLut[colorIdx];
        }
    }

    // [修改点 1] 绘制 FPS 到右上角
    if (s_showFps) {
        uint16_t color = 0xE007; 
        if (s_currentFps < 28) color = 0x00F8; 
        // x=119: 128 - (4*2) - 1 = 119 (留出2个数字的宽度)
        drawFpsNumber(119, 1, s_currentFps, color, g_lineBuffer);
    }

    CyberPi::getInstance().render(g_lineBuffer);
}

// 音频
bool Host::shouldFillAudioBuff() { return false; }
void* Host::getAudioBufferPointer() { return g_audioBuffer; }
size_t Host::getAudioBufferSize() { return sizeof(g_audioBuffer); }
void Host::playFilledAudioBuffer() { }

// 初始化
void Host::oneTimeSetup(Audio* audio) {
    setUpPaletteColors(); 
}
void Host::oneTimeCleanup() {}
void Host::setPlatformParams(int, int, uint32_t, uint32_t, uint32_t, std::string, std::string, std::string) {}
std::string Host::getCartDirectory() { return ""; }
std::vector<std::string> Host::listcarts() { return {}; }
std::string Host::customBiosLua() { return ""; }

// [修改点 2] 输入逻辑修正
InputState_t Host::scanInput() {
    InputState_t state = {};
    
    // 清空状态
    state.KDown = 0;
    state.KHeld = 0;
    // ... 鼠标状态省略，CyberPi暂无 ...

    // 1. 读取硬件 I2C
    CyberPi& device = CyberPi::getInstance();
    device.updateInputState();

    // 2. 构建当前帧的按键掩码 (Current Held Mask)
    uint8_t currentHeld = 0;

    if (device.isButtonPressed(CYBERPI_KEY_LEFT))   currentHeld |= P8_KEY_LEFT;
    if (device.isButtonPressed(CYBERPI_KEY_RIGHT))  currentHeld |= P8_KEY_RIGHT;
    if (device.isButtonPressed(CYBERPI_KEY_UP))     currentHeld |= P8_KEY_UP;
    if (device.isButtonPressed(CYBERPI_KEY_DOWN))   currentHeld |= P8_KEY_DOWN;
    
    // A 键和摇杆中键都映射为 O
    if (device.isButtonPressed(CYBERPI_KEY_A) || 
        device.isButtonPressed(CYBERPI_KEY_CENTER)) currentHeld |= P8_KEY_O;
        
    // B 键映射为 X
    if (device.isButtonPressed(CYBERPI_KEY_B))      currentHeld |= P8_KEY_X;
    
    // Menu 键映射为 Pause
    if (device.isButtonPressed(CYBERPI_KEY_MENU))   currentHeld |= P8_KEY_PAUSE;

    // 3. 计算 KHeld (btn) 和 KDown (btnp)
    // KHeld: 当前正按下的键
    state.KHeld = currentHeld;

    // KDown: 当前按下 且 上一帧未按下 (上升沿检测)
    // 公式: Current & (~Last)
    state.KDown = currentHeld & (~s_lastHeldState);

    // 4. 保存当前状态供下一帧对比
    s_lastHeldState = currentHeld;

    return state;
}