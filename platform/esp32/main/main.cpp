#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// 包含原始头文件
#include "host.h"
#include "vm.h"
#include "logger.h"
#include "CyberPi.hpp" // 用于硬件初始化

extern "C" void app_main(void)
{
    Logger_Initialize(NULL);
    Logger_Write("Starting (Native Host Implementation)...\n");

    // 1. 初始化硬件
    CyberPi::getInstance().init();

    // 2. 创建 Host (这里的 Host 就是我们在 Host_Implementation.cpp 里实现的那个)
    Host* host = new Host();

    // 3. 创建 VM
    // 我们不需要传 Audio，传 nullptr 让 VM 内部自己 new
    // 或者 PICO_RAM 也让 VM 自己 new
    Vm* vm = new Vm(host, nullptr, nullptr, nullptr, nullptr);

    // 4. 初始化设置
    host->oneTimeSetup(nullptr);

    // 5. 加载 BIOS
    Logger_Write("Loading BIOS...\n");
    vm->LoadBiosCart();
    
    // 6. 手动注入 Lua (作为双重保险)
    std::string lua = host->customBiosLua();
    // if (!lua.empty()) {
    //     vm->ExecuteLua(lua, "");
    //     vm->ExecuteLua("_init()", "");
    // }

    // 7. 把控制权交给 VM
    // VM 会在内部死循环调用 host->waitForTargetFps() 和 host->drawFrame()
    Logger_Write("Entering GameLoop...\n");
    vm->GameLoop();
    
    Logger_Exit();
}