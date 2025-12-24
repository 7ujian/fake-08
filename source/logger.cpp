/* platform/esp32/components/fake08/logger.cpp */
#include <strings.h>
#include <stdarg.h>
#include <string>
#include <stdio.h>

#include "logger.h"

// ---------------------------------------------------------
// [修改 1] 强制开启日志宏
// ---------------------------------------------------------
#define LOGGER_ENABLED true 
#define PRINT_TO_CONSOLE true

// 引入 ESP 日志头文件
#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif

FILE * m_file = nullptr;
bool m_enabled = false;

void Logger_Initialize(const char* pathPrefix)
{
    #if LOGGER_ENABLED
    
    #ifdef ESP_PLATFORM
        // [修改 2] ESP32 初始化
        // 不需要打开文件，直接标记为可用
        m_enabled = true;
        // 使用 ESP_LOGI 打印一条醒目的启动日志
        ESP_LOGI("LOGGER", "Logger System Initialized (UART Direct Mode)");
    #else
        // [保留] PC端原有逻辑
        std::string buf(pathPrefix ? pathPrefix : "");
        buf.append("pico.log");
        m_file = freopen(buf.c_str(), "w", stderr);
        m_enabled = true;
    #endif

    #endif
}

void Logger_LogOutput(const char * func, size_t line, const char * format, ...)
{
    #if LOGGER_ENABLED
    // 如果没有初始化，直接返回 (除了 ESP32 调试期间可能想强制输出)
    if (!m_enabled) return;

    #ifndef ESP_PLATFORM
    if (!m_file) return;
    #endif

    va_list args;
    va_start(args, format);

    #ifdef ESP_PLATFORM
        // [修改 3] ESP32 输出格式: [Func:Line] Msg
        printf("[%s:%zu] ", func, line);
        vprintf(format, args);
        printf("\n"); // 补换行
    #else
        fprintf(m_file, "%s:%zu:\n", func, line);
        vfprintf(m_file, format, args);
        fprintf(m_file, "\n\n");
        fflush(m_file);

        #if PRINT_TO_CONSOLE
        vfprintf(stdout, format, args);
        #endif
    #endif

    va_end(args);
    #endif
}

void Logger_Write(const char * format, ...)
{
    #if LOGGER_ENABLED
    if (!m_enabled) return;

    #ifndef ESP_PLATFORM
    if (!m_file) return;
    #endif
        
    va_list args;
    va_start(args, format);

    #ifdef ESP_PLATFORM
        // [修改 4] ESP32 直接透传
        vprintf(format, args);
    #else
        vfprintf(m_file, format, args);
        fflush(m_file);

        #if PRINT_TO_CONSOLE
        vfprintf(stdout, format, args);
        #endif
    #endif

    va_end(args);
    #endif
}

void Logger_WriteUnformatted(const char * message)
{
    #if LOGGER_ENABLED
    if (!m_enabled) return;
        
    #ifdef ESP_PLATFORM
        printf("%s", message);
    #else
        if (m_file) {
            fprintf(m_file, "%s", message);
            fflush(m_file);
        }
        #if PRINT_TO_CONSOLE
        fprintf(stdout, "%s", message);
        #endif
    #endif
    #endif
}

void Logger_Exit()
{
    #if LOGGER_ENABLED
    #ifdef ESP_PLATFORM
        m_enabled = false;
        printf("Logger Exited.\n");
    #else
        if (!m_enabled || !m_file) return;
        fclose(m_file);
        m_enabled = false;
    #endif
    #endif
}