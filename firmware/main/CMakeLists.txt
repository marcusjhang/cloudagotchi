file(GLOB FACE_SRCS "faces/*.c")

idf_component_register(
    SRCS
        "main.c"
        "pet.c"
        "game.c"
        "persist.c"
        "journal.c"
        "pet_ui.c"
        "app_imu.c"
        ${FACE_SRCS}
    INCLUDE_DIRS "."
)
