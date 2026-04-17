include(add_component_test)

enable_testing()

find_package(fmt CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

add_component_test(containers)
add_component_test(ecs)
add_component_test(math)
add_component_test(reflection)

add_executable(
  all-tests
    containers/tests.cpp
    ecs/tests.cpp
    math/tests.cpp
    reflection/tests.cpp
)

target_link_libraries(
  all-tests
  PRIVATE
    libsbx::libsbx
    GTest::gtest
    GTest::gtest_main
)

set_target_properties(
  all-tests
  PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME all-tests COMMAND all-tests)
