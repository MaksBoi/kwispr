cmake_minimum_required(VERSION 3.20)
project(KdeWhisper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(CTest)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)
find_package(KF6 REQUIRED COMPONENTS CoreAddons)

add_library(kde_whisper_core STATIC
    src/AppMetadata.cpp
    src/config/EnvFile.cpp
    src/config/KwisprSettings.cpp
)
target_include_directories(kde_whisper_core PUBLIC src)
target_link_libraries(kde_whisper_core PUBLIC Qt6::Core)

add_executable(kde-whisper
    src/main.cpp
)
target_link_libraries(kde-whisper PRIVATE kde_whisper_core Qt6::Widgets KF6::CoreAddons)

if(BUILD_TESTING)
    add_executable(SettingsModelTest tests/settings_model_test.cpp)
    target_link_libraries(SettingsModelTest PRIVATE kde_whisper_core Qt6::Test)
    add_test(NAME SettingsModelTest COMMAND SettingsModelTest)

    add_executable(EnvFileTest tests/env_file_test.cpp)
    target_link_libraries(EnvFileTest PRIVATE kde_whisper_core Qt6::Test)
    add_test(NAME EnvFileTest COMMAND EnvFileTest)

    add_executable(ScaffoldTest tests/scaffold_test.cpp)
    target_link_libraries(ScaffoldTest PRIVATE kde_whisper_core Qt6::Test)
    add_test(NAME ScaffoldTest COMMAND ScaffoldTest)
endif()
