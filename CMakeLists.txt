cmake_minimum_required(VERSION 3.16)
project(deadswitch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(CURL REQUIRED)
find_package(nlohmann_json 3.9 REQUIRED)

add_executable(deadswitch src/deadswitch.cpp)
target_link_libraries(deadswitch PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)

if(MSVC)
  # /utf-8 is needed so the emoji in the Telegram messages are not damaged
  target_compile_options(deadswitch PRIVATE /W4 /utf-8)
else()
  target_compile_options(deadswitch PRIVATE -Wall -Wextra)
endif()
