project(libsbx_tests VERSION 0.1.0 LANGUAGES CXX)

message(STATUS "Configuring ${PROJECT_NAME}...")

add_executable(${PROJECT_NAME})

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/math/vector3_test.cpp"
    "${PROJECT_SOURCE_DIR}/math/matrix4x4_test.cpp"
    "${PROJECT_SOURCE_DIR}/math/quaternion_test.cpp"
    "${PROJECT_SOURCE_DIR}/math/angle_test.cpp"
    "${PROJECT_SOURCE_DIR}/math/volume_test.cpp"
    "${PROJECT_SOURCE_DIR}/math/ray_test.cpp"

    "${PROJECT_SOURCE_DIR}/containers/static_vector_test.cpp"
    "${PROJECT_SOURCE_DIR}/containers/ring_buffer_test.cpp"
    "${PROJECT_SOURCE_DIR}/containers/compressed_pair_test.cpp"
    "${PROJECT_SOURCE_DIR}/containers/dense_map_test.cpp"
    "${PROJECT_SOURCE_DIR}/containers/stable_vector_test.cpp"

    "${PROJECT_SOURCE_DIR}/ecs/entity_test.cpp"
    "${PROJECT_SOURCE_DIR}/ecs/registry_test.cpp"

    "${PROJECT_SOURCE_DIR}/utility/fast_mod_test.cpp"
    "${PROJECT_SOURCE_DIR}/utility/crc32_test.cpp"
    "${PROJECT_SOURCE_DIR}/utility/hashed_string_test.cpp"
    "${PROJECT_SOURCE_DIR}/utility/static_string_test.cpp"
    "${PROJECT_SOURCE_DIR}/utility/compression_test.cpp"

    "${PROJECT_SOURCE_DIR}/units/length_test.cpp"
    "${PROJECT_SOURCE_DIR}/units/time_test.cpp"
    "${PROJECT_SOURCE_DIR}/units/velocity_test.cpp"
)

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE
    libsbx::libsbx
    GTest::gtest_main
)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    POSITION_INDEPENDENT_CODE ON
)

target_compile_options(
  ${PROJECT_NAME}
  PRIVATE
    -Wall
    -Wextra
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wuseless-cast
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
