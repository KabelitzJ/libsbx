function(add_component_test MODULE_NAME)
  set(_target "${MODULE_NAME}-tests")
  set(_test_dir "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}")

  add_executable(${_target} "${_test_dir}/tests.cpp")

  target_include_directories(${_target} PRIVATE ${_test_dir})

  target_link_libraries(
    ${_target}
    PRIVATE
      libsbx::libsbx
      GTest::gtest
      GTest::gtest_main
  )

  set_target_properties(
    ${_target}
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )

  add_test(NAME ${_target} COMMAND ${_target})
endfunction()
