idf_component_register(SRCS "xl9555.c" "button.c" "main.c"
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(html ../html FLASH_IN_PROJECT)