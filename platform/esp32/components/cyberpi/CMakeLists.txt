idf_component_register(
    SRCS
        "src/CyberPi.cpp"
        "src/drivers/aw9523b.cpp"
        "src/drivers/st7735.cpp"
    
    INCLUDE_DIRS
        "include"

    REQUIRES
        driver      # 包含 GPIO, SPI, I2C 驱动
        log         # 包含 esp_log.h
        freertos    # 包含 FreeRTOS.h, task.h
)