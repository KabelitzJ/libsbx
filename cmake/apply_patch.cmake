execute_process(
  COMMAND "${GIT}" apply --reverse --check --ignore-whitespace "${PATCH_FILE}"
  RESULT_VARIABLE REVERSED
  OUTPUT_QUIET
  ERROR_QUIET
)

if(REVERSED EQUAL 0)
  return()
endif()

execute_process(
  COMMAND "${GIT}" apply --ignore-whitespace "${PATCH_FILE}"
  RESULT_VARIABLE APPLIED
)

if(NOT APPLIED EQUAL 0)
  message(FATAL_ERROR "Failed to apply patch: ${PATCH_FILE}")
endif()
