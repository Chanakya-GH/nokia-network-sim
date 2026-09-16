cmake_minimum_required(VERSION 3.16)
project(netsim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# Sanitizers available via: cmake -DENABLE_SANITIZERS=ON ..
option(ENABLE_SANITIZERS "Build with ASan+UBSan (thread issues: use -DENABLE_TSAN=ON separately)" OFF)
option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)

if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
endif()

find_package(Threads REQUIRED)

# --- Core library (everything except main.cpp) ---
add_library(netsim_core
    src/packet.cpp
    src/node.cpp
    src/link_state.cpp
    src/queue_metrics.cpp
)
target_include_directories(netsim_core PUBLIC include)
target_link_libraries(netsim_core PUBLIC Threads::Threads)

# --- Main executable (the simulation driver) ---
add_executable(netsim src/main.cpp)
target_link_libraries(netsim PRIVATE netsim_core)

# --- Tests ---
enable_testing()
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
