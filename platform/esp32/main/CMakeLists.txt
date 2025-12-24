# platform/esp32/main/CMakeLists.txt

# 1. 定义 z8lua (Lua 引擎) 的源文件列表
# 这些文件原本是 .c，但我们需要用 C++ 编译器处理它们
set(Z8LUA_SOURCES
    "../../../libs/z8lua/eris.c"
    "../../../libs/z8lua/lapi.c"
    "../../../libs/z8lua/lauxlib.c"
    "../../../libs/z8lua/lbaselib.c"
    "../../../libs/z8lua/lbitlib.c"
    "../../../libs/z8lua/lcode.c"
    "../../../libs/z8lua/lcorolib.c"
    "../../../libs/z8lua/lctype.c"
    "../../../libs/z8lua/ldblib.c"
    "../../../libs/z8lua/ldebug.c"
    "../../../libs/z8lua/ldo.c"
    "../../../libs/z8lua/ldump.c"
    "../../../libs/z8lua/lfunc.c"
    "../../../libs/z8lua/lgc.c"
    "../../../libs/z8lua/linit.c"
    "../../../libs/z8lua/liolib.c"
    "../../../libs/z8lua/llex.c"
    "../../../libs/z8lua/lmem.c"
    "../../../libs/z8lua/loadlib.c"
    "../../../libs/z8lua/lobject.c"
    "../../../libs/z8lua/lopcodes.c"
    "../../../libs/z8lua/loslib.c"
    "../../../libs/z8lua/lparser.c"
    "../../../libs/z8lua/lpico8lib.c"
    "../../../libs/z8lua/lstate.c"
    "../../../libs/z8lua/lstring.c"
    "../../../libs/z8lua/lstrlib.c"
    "../../../libs/z8lua/ltable.c"
    "../../../libs/z8lua/ltablib.c"
    "../../../libs/z8lua/ltm.c"
    "../../../libs/z8lua/lundump.c"
    "../../../libs/z8lua/lvm.c"
    "../../../libs/z8lua/lzio.c"
)

# 2. 注册 ESP-IDF 组件
idf_component_register(
    SRCS
        ${Z8LUA_SOURCES}
        # FAKE-08 核心源码
        "../../../source/vm.cpp"
        "../../../source/graphics.cpp"
        "../../../source/Audio.cpp"
        "../../../source/Input.cpp"
        "../../../source/cart.cpp"
        "../../../source/hostCommonFunctions.cpp"
        "../../../source/filehelpers.cpp"
        "../../../source/mathhelpers.cpp"
        "../../../source/nibblehelpers.cpp"
        "../../../source/stringToDataHelpers.cpp"
        "../../../source/picoluaapi.cpp"
        "../../../source/fontdata.cpp"
        "../../../source/logger.cpp"
        "../../../source/synth.cpp"
        "../../../source/emojiconversion.cpp"
        "../../../source/printHelper.cpp"
        
        # 依赖库
        "../../../libs/lodepng/lodepng.cpp"
        "../../../libs/miniz/miniz.c"
        
        # 平台适配代码
        "main.cpp"
        "Host_Implementation.cpp"

    INCLUDE_DIRS
        "."
        "../../../source"
        "../../../libs"
        "../../../libs/z8lua"
        "../../../libs/lodepng"
        "../../../libs/miniz"
        "../../../libs/simpleini"

    REQUIRES
        driver
        esp_timer
        fatfs
        spiffs
        heap
        freertos
        # 如果使用 S3，可能需要显式引入 log 组件
        log 
        cyberpi
)

# 3. [核心操作] 强制将 z8lua 和 miniz 的 .c 文件视为 C++ 编译
# 这解决了 fix32 类在 Lua 核心代码中不可见的问题
set_source_files_properties(${Z8LUA_SOURCES} "../../../libs/miniz/miniz.c" PROPERTIES LANGUAGE CXX)

# 4. 全局宏定义
target_compile_definitions(${COMPONENT_LIB} PRIVATE 
    MINIZ_NO_TIME       # 移除 miniz 对系统时间的依赖
    __LIBRETRO__        # 启用 FAKE-08 的 RetroArch 兼容模式 (适配代码结构)
    LUA_USE_LONGJMP     # 强制 Lua 使用 setjmp/longjmp 而不是 C++ try/throw
)

# 5. 编译选项调整
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -std=gnu++17 
    -fno-rtti           # 禁用运行时类型信息，节省内存
    
    # 忽略非关键警告，防止 -Werror 中断编译
    -Wno-format 
    -Wno-unused-variable 
    -Wno-unused-function
    -Wno-sign-compare
    -Wno-missing-field-initializers
    -Wno-error=deprecated-declarations
    
    # [关键修复] 解决 ESP-IDF 编译器过于严格的 specific errors
    -Wno-implicit-fallthrough   # 忽略 switch-case 无 break 的警告 (llex.c)
    -Wno-error=return-type      # 忽略 noreturn 函数返回警告 (ldo.c)
    -Wno-type-limits            # 忽略 filehelpers.cpp 中 unsigned >= 0 的警告
)