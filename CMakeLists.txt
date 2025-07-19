cmake_minimum_required(VERSION 3.15)
set(YOGA_VERSION v3.2.1)
set(YOGACPP_VERSION 2.0.0)
project(yoga_cpp VERSION ${YOGACPP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(yoga
    GIT_REPOSITORY https://github.com/facebook/yoga.git
    GIT_TAG ${YOGA_VERSION}
)
FetchContent_MakeAvailable(yoga)

set_target_properties(yogatests PROPERTIES EXCLUDE_FROM_ALL ON)

add_library(yoga_cpp STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/yoga-cpp/yoga.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/yoga.cpp
)

target_link_libraries(yoga_cpp PUBLIC yogacore)
target_include_directories(yoga_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(YOGACPP_BUILD_EXAMPLES "Build example program(s)" OFF)
option(YOGACPP_BUILD_TESTS "Build tests" OFF)

if (YOGACPP_BUILD_EXAMPLES)
    add_executable(print_dimensions ${CMAKE_CURRENT_SOURCE_DIR}/examples/print_dimensions.cpp)
    target_link_libraries(print_dimensions PRIVATE yoga_cpp)

    add_executable(child_iteration ${CMAKE_CURRENT_SOURCE_DIR}/examples/child_iteration.cpp)
    target_link_libraries(child_iteration PRIVATE yoga_cpp)
endif()

if (YOGACPP_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(yoga_cpp_tests test/layout.cpp)

    if(NOT MSVC)
        target_compile_options(gtest PRIVATE "-frtti")
        target_compile_options(gtest_main PRIVATE "-frtti")
        target_compile_options(yoga_cpp_tests PRIVATE "-frtti")
    endif()

    target_link_libraries(yoga_cpp_tests GTest::gtest_main yoga_cpp yogacore)

    include(GoogleTest)
    gtest_discover_tests(yoga_cpp_tests)
endif()
