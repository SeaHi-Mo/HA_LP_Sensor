cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

file(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/components/*.c)
sdk_add_include_directories(.)
sdk_add_include_directories(main systemConfig)
sdk_add_include_directories(components/wifi)

aux_source_directory(../Ai-mqtt-lib/driver SRC_FILES)

aux_source_directory(../Ai-mqtt-lib/protocol SRC_FILES)
aux_source_directory(../Ai-mqtt-lib/system SRC_FILES)
aux_source_directory(../Ai-mqtt-lib/utils SRC_FILES)

aux_source_directory(../Ai-mqtt-lib/application SRC_FILES)
aux_source_directory(../Ai-mqtt-lib/3rdparty/http-parser/src SRC_FILES)
aux_source_directory(../Ai-mqtt-lib/3rdparty/aiio/tcp_transport SRC_FILES)
aux_source_directory(../Ai-mqtt-lib/3rdparty/aiio/tcp_transport SRC_FILES)
aux_source_directory(../Ai-mqtt-lib/3rdparty/aiio/aiio_tls SRC_FILES)
aux_source_directory(../Ai-mqtt-lib/3rdparty/aiio/aiio_tls/aiio-tls-crypto SRC_FILES)
aux_source_directory(../Ai-mqtt-lib/3rdparty/aiio/mqtt/aiio_mqtt SRC_FILES)
aux_source_directory(../Ai-mqtt-lib/3rdparty/aiio/mqtt/aiio_mqtt/lib SRC_FILES)

# aux_source_directory(../Ai-mqtt-lib/3rdparty/blufi SRC_FILES)

# 添加头文件的引用路径（Add .h include directories）
sdk_add_include_directories(../Ai-mqtt-lib/driver)
sdk_add_include_directories(../Ai-mqtt-lib/protocol)
sdk_add_include_directories(../Ai-mqtt-lib/system)
sdk_add_include_directories(../Ai-mqtt-lib/utils)

sdk_add_include_directories(../Ai-mqtt-lib/application)
sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/http-parser/include)
sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio/tcp_transport/include)
sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio/tcp_transport/private_include)
sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio/aiio_tls)
sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio/aiio_tls/aiio-tls-crypto)

sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio)

sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio/aiio_http_client/include)
sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio/aiio_http_client/lib/include)
sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio/aiio_http_server/include)
sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio/aiio_http_server/src/port)
sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio/mqtt/aiio_mqtt/lib/include)
sdk_add_include_directories(../Ai-mqtt-lib/3rdparty/aiio/mqtt/aiio_mqtt/include)

# sdk_add_include_directories(./../Ai-mqtt-lib/3rdparty/blufi)

# 添加HomeAssistant 库
file(GLOB_RECURSE HA ${CMAKE_CURRENT_SOURCE_DIR}/../HomeAssistant-lib/HomeAssistant-C/*.c)
sdk_add_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../HomeAssistant-lib/HomeAssistant-C)

list(REMOVE_ITEM SRC_FILES "./main/main.c")

target_sources(app PRIVATE ${SRC_FILES} ${sources} ${HA})

sdk_set_main_file(main/main.c)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME})
