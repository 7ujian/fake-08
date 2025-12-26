# CyberPi / Fake08 架构重构设计文档

## 1. 设计目标

1.  **消除内存浪费**：解决当前 `Cart` 类中存在的 ROM 数据三份拷贝（File String -> Member String -> Rom Data）的问题。
2.  **生命周期分离**：区分**持久化数据**（资源 ROM）与**瞬时数据**（Lua 源代码），针对 ESP32 内存特性进行优化。
3.  **支持编程生成**：不仅支持从文件加载，也支持通过代码直接构建 `GameCart` 对象，为单元测试和未来的 **PicoEditor** 编辑器功能铺路。
4.  **清晰的职责划分**：解耦 Loader（加载器）、Storage（存储）和 Runtime（运行时）。

---

## 2. 核心架构：主机模式 (The Console Architecture)

我们将系统重新划分为三个核心角色，类比真实的游戏主机：

| 角色 | 新类名 | 类比硬件 | 职责 | 内存位置 (ESP32) |
| :--- | :--- | :--- | :--- | :--- |
| **存储容器** | `GameCart` | **卡带** (Cartridge) | 只读数据的备份源，用于 `reload()` 和持久化。不包含 Lua 源码。 | **PSRAM** (外部 RAM) |
| **工作内存** | `PicoRam` | **内存条** (RAM) | CPU/PPU 直接读写的高速内存。 | **Internal SRAM** (内部 RAM) |
| **加载器** | `CartLoader` | **烧录器/光驱** | 读取文件或数据流，生产 `GameCart` 和 Lua 代码，用完即销毁。 | 栈 / 临时堆 |
| **虚拟机** | `Vm` | **主机** (Console) | 协调者。持有 `PicoRam` 和当前插着的 `GameCart`。 | 堆 (Heap) |

---

## 3. 详细类定义

### 3.1 数据结构：`GameCart` (The Storage)

这是一个纯粹的数据容器（DTO）。它不仅仅代表一个文件，它代表一个**游戏资源包**。

**关键变更**：
* 移除了所有解析逻辑（`loadFromPng` 等）。
* 移除了 Lua 源代码成员变量（代码在加载时分离）。
* 提供了 API 以支持编辑器模式下的修改。

```cpp
// GameCart.h
#include "CartRomData.h" // 定义具体的 ROM 结构

class GameCart {
public:
    // 1. 核心数据：卡带 ROM (Graphics, Map, Sfx, Music)
    // 建议在构造时分配到 PSRAM
    CartRomData* rom; 

    // 2. 元数据
    std::string filename;
    
    // 构造函数：分配空白 ROM (便于编辑器新建空卡带)
    GameCart(); 
    
    // 析构函数：释放 rom 内存
    ~GameCart();

    // ==========================================
    // 编辑器 API (Programmatic Generation)
    // ==========================================
    // 允许代码直接修改 ROM，而不需要经过 PNG 解析
    // 例如：editor->DrawPixel -> cart->SetSpritePixel(...)
    void setSpriteData(const uint8_t* data, size_t size);
    void setMapData(const uint8_t* data, size_t size);
    // ... 其他 setter
};
```

### 3.2 传输对象：`LoadedCartResult`

这是一个临时结构体，用于在 `Loader` 和 `VM` 之间传递“拆包”后的数据。

```cpp
struct LoadedCartResult {
    GameCart* cart;       // 准备好的资源对象 (持久)
    std::string luaCode;  // 提取出的 Lua 代码 (瞬时)
    bool success;
    std::string errorMsg;
};
```

### 3.3 构建者：`CartLoader` (The Builder)

这是一个静态工具类，负责“脏活累活”。它处理文件 IO、PNG 解码、隐写术提取和文本解析。

```cpp
// CartLoader.h
class CartLoader {
public:
    // 方式 A: 从文件系统加载
    static LoadedCartResult LoadFromFile(const std::string& path);

    // 方式 B: 从内存中的文件数据加载 (如网络下载的 buffer)
    static LoadedCartResult LoadFromBuffer(const uint8_t* data, size_t size);

private:
    // 内部实现流式解析，不再缓存大字符串
    static void ParseP8Text(std::istream& stream, GameCart* targetCart, std::string& outLua);
    static void ParsePng(const std::vector<uint8_t>& image, GameCart* targetCart, std::string& outLua);
};
```

### 3.4 运行时：`Vm` (The Console)

VM 负责将 `GameCart` "插入" 到系统，并初始化 `PicoRam`。

```cpp
// Vm.cpp
void Vm::LoadGame(const LoadedCartResult& result) {
    if (!result.success) {
        // Handle error...
        return;
    }

    // 1. 保存卡带引用 (用于 reload API)
    // 如果之前有卡带，先卸载
    if (_currentCart) delete _currentCart; 
    _currentCart = result.cart;

    // 2. 硬件复位 & 数据拷贝
    // 将 GameCart(PSRAM) 的数据 复制到 PicoRam(SRAM)
    Reset(); 

    // 3. 编译 Lua 代码
    // 这是 luaCode 生命周期的终点。
    // 执行完这一步，Result 对象析构，luaCode 字符串释放。
    if (!result.luaCode.empty()) {
        _lua->Execute(result.luaCode);
    }
    
    // 4. 启动
    _lua->Call("_init");
}

void Vm::Reset() {
    _ram->clear();
    // 这里的 memcpy 是实现 reload() 的关键
    if (_currentCart && _currentCart->rom) {
        memcpy(_ram->spriteSheetData, _currentCart->rom->SpriteSheetData, sizeof(...));
        // ... 拷贝其他段 ...
    }
}
```

---

## 4. 场景演练

### 场景 A：从 SD 卡加载游戏 (Player Mode)

这是最标准的流程。

1.  用户选择 `celeste.p8`。
2.  调用 `CartLoader::LoadFromFile("celeste.p8")`。
3.  Loader 逐行解析：
    * 遇到 `__gfx__`：直接写入 `GameCart->rom`。
    * 遇到 `__lua__`：追加到临时的 `std::string luaCode`。
4.  Loader 返回 `LoadedCartResult`。
5.  VM 调用 `LoadGame(result)`：
    * `GameCart` 被挂载。
    * Lua 代码被编译进 Lua VM。
6.  `result` 变量离开作用域，`luaCode` 字符串被销毁，释放 RAM。

### 场景 B：通过代码生成游戏 (Editor Mode / Unit Test)

这是为了满足**PicoEditor**和**开发便利性**的需求。

```cpp
// main.cpp 测试代码

// 1. 手动创建一个空卡带
GameCart* manualCart = new GameCart();

// 2. 编程式注入资源 (Editor 操作)
// 模拟编辑器画了一个红点
manualCart->rom->SpriteSheetData[0] = 8; 

// 3. 准备 Lua 代码 (Editor 文本框内容)
std::string dynamicCode = R"(
    function _init()
        print("Generated Cart!")
    end
    function _draw()
        spr(0, 64, 64)
    end
)";

// 4. 包装成 Result
LoadedCartResult result;
result.cart = manualCart;
result.luaCode = dynamicCode;
result.success = true;

// 5. 运行
vm->LoadGame(result);
```

---

## 5. 内存优化对比 (ESP32 视角)

| 阶段 | 旧架构 (Memory Usage) | 新架构 (Console Architecture) |
| :--- | :--- | :--- |
| **加载前** | 0 | 0 |
| **解析中** | 全文String + 分段String + RomData | 1行Buffer + 瞬时LuaCode + RomData(PSRAM) |
| **运行时** | **LuaCode(Heap)** + RomData(Heap) + PicoRam(SRAM) | **0 (已释放)** + RomData(PSRAM) + PicoRam(SRAM) |
| **Reload时**| 依赖 Cart 对象中的 String | 依赖 GameCart 中的 RomData |

**核心收益**：

1.  **SRAM 节省**：运行时 Lua 源代码字符串被完全移除，只保留编译后的字节码。
2.  **PSRAM 利用**：`GameCart` 作为备份数据，完全可以放在慢速的外部 RAM 中，不占用宝贵的内部 RAM。

## 6. 实施路线图

1.  **Refactor Cart Data**: 创建 `GameCart` 类，仅包含 `CartRomData` 和基本 Metadata。确保其构造函数申请内存。
2.  **Create Loader**: 创建 `CartLoader` 类，迁移原有的 PNG/Text 解析逻辑。将解析逻辑改为直接写入 `GameCart` 而非中间 String。
3.  **Update VM**: 修改 `Vm::LoadCart` 接口，使其接受 `LoadedCartResult`。
4.  **Update Main**: 在 `main.cpp` 中使用新的构建方式进行测试。