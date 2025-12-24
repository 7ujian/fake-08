# platform/esp32/CMakeLists.txt

# 1. 设定 CMake 最低版本要求 (ESP-IDF 推荐 3.5 或 3.16)
cmake_minimum_required(VERSION 3.5)

# 2. 引入 ESP-IDF 的核心构建脚本
# 这个脚本会自动处理工具链、组件扫描和构建流程
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 3. 定义项目名称
# 这决定了最终生成的固件文件名 (例如 fake08_cyberpi.elf, fake08_cyberpi.bin)
project(fake08_cyberpi)