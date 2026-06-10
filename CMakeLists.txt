cmake_minimum_required(VERSION 3.15)
project(spikehat C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

add_library(spikehat SHARED
    src/serial.c
    src/protocol.c
    src/spikehat.c
    src/motor.c
    src/sensor.c
)
target_include_directories(spikehat
    PUBLIC  include
    PRIVATE src
)
target_link_libraries(spikehat pthread m)
set_target_properties(spikehat PROPERTIES
    VERSION   1.0.0
    SOVERSION 1
)

add_executable(test_motor   examples/test_motor.c)
add_executable(test_sensor  examples/test_sensor.c)
foreach(t test_motor test_sensor)
    target_include_directories(${t} PRIVATE include)
    target_link_libraries(${t} spikehat)
endforeach()
