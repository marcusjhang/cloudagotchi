idf_component_register(
    SRCS
        "main.c"
        "app_wifi.c"
        "app_mqtt.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES
        "certs/AmazonRootCA1.pem"
        "certs/device-cert.pem"
        "certs/device-key.pem"
)
