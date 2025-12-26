# 测试目录概览

本目录使用 `doctest` 作为单元测试框架，`testroot.cpp` 提供测试入口。以下按文件整理测试内容与用途，便于快速理解与定位。

## 测试入口与公共代码
- `testroot.cpp`：定义 `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`，作为测试可执行入口。
- `doctest.h`：测试框架头文件。
- `testHelpers.h` / `testHelpers.cpp`：测试辅助工具（颜色比较、屏幕断言等）。
- `stubhost.h` / `stubhost.cpp`：`Host` 的桩实现，用于 VM 与图形/输入/音频的独立测试。
- `nooplogger.cpp`：空实现的日志接口，避免测试时产生输出或依赖。
- `Makefile`：构建测试用的目标与编译配置。

## 功能与模块测试
- `audiotests.cpp`
  - TEST_CASE: 音频类行为验证
  - 覆盖点：
    - 自定义乐器音符推进与取样
    - `api_sfx` 的通道选择/-2停止逻辑
    - `api_music` 的模式设定、主通道选择、通道映射

- `graphicstests.cpp`
  - TEST_CASE: 图形类行为验证
  - 覆盖点：
    - 构造默认裁剪/颜色/调色板与透明度
    - `cls` 清屏与游标/裁剪重置
    - `pset/pget` 设置与读取像素
    - `color` 设置/恢复/返回上一个颜色
    - `line` 状态机与绘制（多分支）

- `nibblehelperstests.cpp`
  - TEST_CASE: 像素索引与有效性计算
  - 覆盖点：`getCombinedIdx`、`isValidSprIdx` 的边界与合法性。

- `filehelperstests.cpp`
  - TEST_CASE: 路径/文件工具集
  - 覆盖点：
    - `isAbsolutePath`、`getDirectory`、`getFileExtension`
    - `isHiddenFile`、`isCartFile`
    - `get_first_four_chars`（文件头判断）
    - `isCPostFile`（特定命名约定）

- `fontdatatests.cpp`
  - TEST_CASE: 字体数据存在性
  - 说明：包含将字体表转换为二进制的示例代码（已注释），当前仅校验默认数据存在。

- `picoluaapitests.cpp`
  - TEST_CASE: Lua API — `stat` 与 `print`
  - 覆盖点：
    - `stat` 查询音频相关指标（sfx 通道与 music 状态）
    - `print` 的游标推进、包含空字节、渲染位置/颜色影响

- `printHelperTests.cpp`
  - TEST_CASE: 文本输出辅助
  - 覆盖点：
    - `print` 在透明度、当前颜色、游标、相机与裁剪下的行为
    - 制表符/退格/回车等 P8SCII 控制字符的游标影响

- `vmtests.cpp`
  - TEST_CASE: VM 内存与 API
  - 覆盖点：
    - `PicoRam` 初始化与尺寸校验
    - `vm_peek/poke/peek2/peek4/poke2/poke4`
    - 贴图/地图/精灵标记/音乐/SFX/通用 RAM 的读写
    - 画面调色板/裁剪/颜色/文本游标/相机/绘制模式/暂停状态等状态位
    - `cartdata` 区域的 `dget/dset` 与直接读写

- `carttests.cpp`
  - TEST_CASE: 卡带解析（p8 与 png）
  - 覆盖点：
    - BIOS 与简易 p8/png 的头部与分区解析
    - Sprite/Flags/Map/Sfx/Music 的字符串与二进制数据校验
    - 旧版 png 卡带格式的基本解析

- `endtoendtests.cpp`
  - TEST_CASE: 端到端卡带加载与运行
  - 覆盖点：
    - 加载简单卡带、逐帧更新计数
    - 提供截图比对工具（当前批量截图用例被注释，保留单用例）

- `cartloadingtest.cpp`
  - TEST_CASE: 批量卡带加载与筛选
  - 覆盖点：
    - 批量加载逻辑与忽略列表（大规模数据集的稳定性用例骨架）

## 资源与数据
- `carts/`：测试卡带与对比截图资源（`*.p8`、`*.p8.png`、`screenshots/*.png`）。

## 结构关系速览
- 依赖核心模块：`Audio`、`Graphics`、`Input`、`Vm`、`PicoRam`、工具库 `lodepng` 与 Lua（通过 Pico API）。
- 测试分层：
  - 基础工具/数据（nibble、file、font）
  - 核心组件（graphics、audio、vm）
  - Lua API（picoluaapi、printHelper）
  - 集成流与回归（cart 解析、end-to-end、批量加载）

