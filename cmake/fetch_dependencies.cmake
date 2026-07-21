include(FetchContent)

# fetch_dependencies(<dependencies_file> <install_dir> [<export_set>])
#
# <export_set> is optional. If given, fallback INTERFACE targets created for
# dependencies that expose no CMake target of their own are installed into that
# export set, so consumers exporting their own targets do not fail with
# "requires target X that is not in any export set".
function(fetch_dependencies DEPENDENCIES_FILE INSTALL_DIR)
  set(EXPORT_SET "")

  if(ARGC GREATER 2)
    set(EXPORT_SET "${ARGV2}")
  endif()

  set(FETCHCONTENT_BASE_DIR "${INSTALL_DIR}")

  set(PATCH_HELPER "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/apply_patch.cmake")

  get_filename_component(DEPS_DIR "${DEPENDENCIES_FILE}" DIRECTORY)

  if(NOT EXISTS "${DEPENDENCIES_FILE}")
    message(FATAL_ERROR "Dependencies file not found: ${DEPENDENCIES_FILE}")
  endif()

  message(STATUS "Resolving dependencies from ${DEPENDENCIES_FILE}")

  file(READ "${DEPENDENCIES_FILE}" JSON)

  string(JSON DEP_COUNT LENGTH "${JSON}")

  if(DEP_COUNT EQUAL 0)
    message(STATUS "No dependencies declared")

    return()
  endif()

  math(EXPR DEP_LAST "${DEP_COUNT} - 1")

  set(DEPENDENCIES_TO_FETCH)

  foreach(I RANGE ${DEP_LAST})
    string(JSON NAME MEMBER "${JSON}" ${I})

    # Condition

    unset(ERR)

    string(JSON CONDITION ERROR_VARIABLE ERR GET "${JSON}" "${NAME}" condition)

    if(NOT ERR)
      if(NOT DEFINED ${CONDITION} OR NOT "${${CONDITION}}")
        message(STATUS "  ${NAME}: skipped (condition '${CONDITION}' not met)")

        continue()
      endif()
    endif()

    # Source

    string(JSON TYPE GET "${JSON}" "${NAME}" source type)

    set(DECLARE_ARGS)

    if(TYPE STREQUAL "git")
      string(JSON URI GET "${JSON}" "${NAME}" source uri)
      string(JSON TAG GET "${JSON}" "${NAME}" source tag)

      set(DECLARE_ARGS GIT_REPOSITORY ${URI} GIT_TAG ${TAG})

      message(STATUS "  ${NAME}: git ${URI}@${TAG}")
    elseif(TYPE STREQUAL "archive")
      string(JSON URI GET "${JSON}" "${NAME}" source uri)
      string(JSON SHA GET "${JSON}" "${NAME}" source sha256)

      set(DECLARE_ARGS URL ${URI} URL_HASH SHA256=${SHA})

      message(STATUS "  ${NAME}: archive ${URI}")
    else()
      message(FATAL_ERROR "Unknown source type '${TYPE}' for dependency '${NAME}'")
    endif()

    # Source subdirectory
    #
    # Two uses:
    #   1. The CMakeLists.txt is not at the repository root (lz4: build/cmake).
    #   2. The dependency has no CMake at all. Point this at a directory that
    #      contains no CMakeLists.txt; FetchContent then only downloads and
    #      skips add_subdirectory, and the fallback below creates the target.

    unset(ERR)

    string(JSON SUBDIR ERROR_VARIABLE ERR GET "${JSON}" "${NAME}" source subdir)

    if(NOT ERR)
      list(APPEND DECLARE_ARGS SOURCE_SUBDIR "${SUBDIR}")

      message(STATUS "    subdir: ${SUBDIR}")
    endif()

    # Patches

    unset(ERR)

    string(JSON PATCH_COUNT ERROR_VARIABLE ERR LENGTH "${JSON}" "${NAME}" patches)

    if(NOT ERR AND PATCH_COUNT GREATER 0)
      if(NOT EXISTS "${PATCH_HELPER}")
        message(FATAL_ERROR "Patch helper missing: ${PATCH_HELPER}")
      endif()

      find_package(Git REQUIRED)

      math(EXPR PATCH_LAST "${PATCH_COUNT} - 1")

      set(PATCH_COMMAND_ARGS)

      foreach(K RANGE ${PATCH_LAST})
        string(JSON PATCH_REL GET "${JSON}" "${NAME}" patches ${K})

        if(IS_ABSOLUTE "${PATCH_REL}")
          set(PATCH_ABS "${PATCH_REL}")
        else()
          set(PATCH_ABS "${DEPS_DIR}/${PATCH_REL}")
        endif()

        if(NOT EXISTS "${PATCH_ABS}")
          message(FATAL_ERROR "Patch not found for '${NAME}': ${PATCH_ABS}")
        endif()

        if(PATCH_COMMAND_ARGS)
          list(APPEND PATCH_COMMAND_ARGS COMMAND)
        endif()

        list(APPEND PATCH_COMMAND_ARGS "${CMAKE_COMMAND}" "-DGIT=${GIT_EXECUTABLE}" "-DPATCH_FILE=${PATCH_ABS}" -P "${PATCH_HELPER}")

        message(STATUS "    patch: ${PATCH_REL}")
      endforeach()

      list(APPEND DECLARE_ARGS PATCH_COMMAND ${PATCH_COMMAND_ARGS})
    endif()

    list(APPEND DECLARE_ARGS SYSTEM)

    FetchContent_Declare(${NAME} ${DECLARE_ARGS})

    # Options

    unset(ERR)

    string(JSON OPT_COUNT ERROR_VARIABLE ERR LENGTH "${JSON}" "${NAME}" options)

    if(NOT ERR AND OPT_COUNT GREATER 0)
      math(EXPR OPT_LAST "${OPT_COUNT} - 1")

      foreach(J RANGE ${OPT_LAST})
        string(JSON OPT_NAME MEMBER "${JSON}" "${NAME}" options ${J})
        string(JSON OPT_VALUE GET "${JSON}" "${NAME}" options "${OPT_NAME}")

        if(OPT_VALUE STREQUAL "true")
          set(OPT_VALUE ON)
        elseif(OPT_VALUE STREQUAL "false")
          set(OPT_VALUE OFF)
        endif()

        if(OPT_VALUE MATCHES "^(ON|OFF)$")
          set(${OPT_NAME} "${OPT_VALUE}" CACHE BOOL "" FORCE)
        else()
          set(${OPT_NAME} "${OPT_VALUE}" CACHE STRING "" FORCE)
        endif()

        message(VERBOSE "    option: ${OPT_NAME}=${OPT_VALUE}")
      endforeach()
    endif()

    list(APPEND DEPENDENCIES_TO_FETCH ${NAME})

  endforeach()

  if(NOT DEPENDENCIES_TO_FETCH)
    message(STATUS "No dependencies to fetch")

    return()
  endif()

  list(LENGTH DEPENDENCIES_TO_FETCH FETCH_TOTAL)

  message(STATUS "Fetching ${FETCH_TOTAL} dependencies: ${DEPENDENCIES_TO_FETCH}")

  FetchContent_MakeAvailable(${DEPENDENCIES_TO_FETCH})

  # Target resolution

  foreach(NAME IN LISTS DEPENDENCIES_TO_FETCH)
    unset(ERR)

    string(JSON ADOPT_COUNT ERROR_VARIABLE ERR LENGTH "${JSON}" "${NAME}" export)

    if(NOT ERR AND ADOPT_COUNT GREATER 0)
      if(NOT EXPORT_SET)
        message(FATAL_ERROR "'${NAME}' requests export but no export set was passed to fetch_dependencies()")
      endif()

      math(EXPR ADOPT_LAST "${ADOPT_COUNT} - 1")

      foreach(E RANGE ${ADOPT_LAST})
        string(JSON ADOPT_TARGET GET "${JSON}" "${NAME}" export ${E})

        if(NOT TARGET ${ADOPT_TARGET})
          message(FATAL_ERROR "Export target '${ADOPT_TARGET}' for '${NAME}' does not exist")
        endif()

        install(TARGETS ${ADOPT_TARGET} EXPORT ${EXPORT_SET}
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
          INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

        message(STATUS "  ${NAME}: adopted ${ADOPT_TARGET} into ${EXPORT_SET}")
      endforeach()
    endif()

    if(TARGET ${NAME})
      continue()
    endif()

    string(TOLOWER "${NAME}" NAME_LOWER)

    unset(ERR)

    string(JSON REAL_TARGET ERROR_VARIABLE ERR GET "${JSON}" "${NAME}" target)

    if(NOT ERR)
      if(NOT TARGET ${REAL_TARGET})
        message(FATAL_ERROR "Target '${REAL_TARGET}' declared for '${NAME}' does not exist")
      endif()

      add_library(${NAME} ALIAS ${REAL_TARGET})

      message(STATUS "  ${NAME}: aliased to ${REAL_TARGET}")

      continue()
    endif()

    unset(ERR)

    string(JSON INCLUDE_DIR ERROR_VARIABLE ERR GET "${JSON}" "${NAME}" include_dir)

    if(ERR)
      set(INCLUDE_DIR "")
    endif()

    message(WARNING "  ${NAME}: no CMake target found, creating INTERFACE fallback")

    add_library(${NAME} INTERFACE)

    target_include_directories(${NAME} INTERFACE
      $<BUILD_INTERFACE:${${NAME_LOWER}_SOURCE_DIR}/${INCLUDE_DIR}>
      $<INSTALL_INTERFACE:include>)

    if(EXPORT_SET)
      install(TARGETS ${NAME} EXPORT ${EXPORT_SET})
      install(DIRECTORY "${${NAME_LOWER}_SOURCE_DIR}/${INCLUDE_DIR}/" DESTINATION include)
    endif()
  endforeach()

  message(STATUS "Dependencies resolved")

endfunction()
