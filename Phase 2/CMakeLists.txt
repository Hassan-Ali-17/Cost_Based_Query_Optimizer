cmake_minimum_required(VERSION 3.10)
project(qopt CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Minimal nlohmann/json integration
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

include_directories(src/include)

# Find all source files
file(GLOB SOURCES "src/*.cpp")

# Executable
add_executable(qopt ${SOURCES})
target_link_libraries(qopt PRIVATE nlohmann_json::nlohmann_json)
