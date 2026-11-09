idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2s
    EMBED_FILES "audio/sample1.wav"
)