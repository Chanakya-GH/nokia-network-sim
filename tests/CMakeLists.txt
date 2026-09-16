include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
# Windows compat setting; harmless on Linux
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(netsim_tests
    test_bounded_queue.cpp
    test_link_state.cpp
    test_packet.cpp
)
target_link_libraries(netsim_tests PRIVATE netsim_core GTest::gtest_main)
target_include_directories(netsim_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(netsim_tests)
