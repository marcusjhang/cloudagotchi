file(GLOB FACE_SRCS "faces/*.c")

idf_component_register(
    SRCS
        "main.c"
        "pet.c"
        "game.c"
        "persist.c"
        "journal.c"
        "rtc.c"
        "battery.c"
        "power.c"
        "pet_ui.c"
        "app_imu.c"
        ${FACE_SRCS}
    INCLUDE_DIRS "."
)

# Game-time multiplier: idf.py -DFAST_FORWARD=60 build makes a day pass in 24 min.
set(FAST_FORWARD "1" CACHE STRING "Game-time multiplier (60 = a day in 24 minutes)")
target_compile_definitions(${COMPONENT_LIB} PRIVATE FAST_FORWARD=${FAST_FORWARD})
