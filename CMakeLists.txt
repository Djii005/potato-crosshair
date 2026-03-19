cmake_minimum_required(VERSION 3.21)

project(PotatoCrosshair VERSION 0.2.0 LANGUAGES CXX RC)

add_executable(PotatoCrosshair WIN32
    resources/app.rc
    src/app_controller.cpp
    src/crosshair_renderer.cpp
    src/main.cpp
    src/main_window.cpp
    src/overlay_window.cpp
    src/settings_store.cpp
    src/tray_icon.cpp
)

target_compile_features(PotatoCrosshair PRIVATE cxx_std_20)
target_compile_definitions(PotatoCrosshair PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(PotatoCrosshair PRIVATE src)

if(MSVC)
    target_compile_options(PotatoCrosshair PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(PotatoCrosshair PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(PotatoCrosshair PRIVATE
    comctl32
    comdlg32
    gdiplus
    ole32
    shell32
)
