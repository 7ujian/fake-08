#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
// 假设 Cart 类定义在 vm.h 或类似的地方，请根据实际情况 include
#include "vm.h" 

// =========================================================
// [配置区] 在这里添加你的卡带
// 规则：填写 CMake 生成的符号中间名 (即文件名将 . 替换为 _ )
// 例如: "game.p8" -> game_p8
// =========================================================
#define CART_LIST \
    X(boldtexttest_p8) \
    X(bunnysurvivor_p8) \
    X(cpu_test_p8) \
    X(rom_test_p8)

// =========================================================
// [自动生成区] 以下代码不需要修改，会自动根据上面的列表展开
// =========================================================

// 1. 自动生成 extern 声明
#define X(name) \
    extern const uint8_t _binary_##name##_start[]; \
    extern const uint8_t _binary_##name##_end[];
CART_LIST
#undef X

// 2. 定义卡带 ID 枚举 (方便调用)
enum class CartID {
    #define X(name) name,
    CART_LIST
    #undef X
    COUNT // 计数用
};

// 3. 辅助结构体
struct EmbeddedCartInfo {
    const char* name;
    const uint8_t* start;
    const uint8_t* end;
};

// 4. 核心加载函数
// 输入: CartID::xxx
// 输出: new Cart(...) 对象指针
static Cart* LoadEmbeddedCart(CartID id) {
    // 定义数据表
    static const EmbeddedCartInfo carts[] = {
        #define X(name) { #name, _binary_##name##_start, _binary_##name##_end },
        CART_LIST
        #undef X
    };

    int index = (int)id;
    if (index < 0 || index >= (int)CartID::COUNT) {
        printf("[Embed] Error: Invalid Cart ID\n");
        return nullptr;
    }

    const EmbeddedCartInfo* info = &carts[index];
    size_t size = info->end - info->start;

    printf("[Embed] Loading: %s | Size: %d bytes\n", info->name, (int)size);

    // 调用 Cart 构造函数 (根据你提供的 main.c 调用方式)
    // 注意这里进行了 const 强转，适配 fake08 的接口
    return new Cart(info->start, size);
}