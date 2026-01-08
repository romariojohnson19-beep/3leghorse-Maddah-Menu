cmake_minimum_required(VERSION 3.20)
project(MenuLauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(menu_launcher main.cpp)

target_compile_definitions(menu_launcher PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(menu_launcher PRIVATE
    kernel32
    user32
)

install(TARGETS menu_launcher RUNTIME DESTINATION .)
