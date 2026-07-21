include(FetchContent)

# fetch_dependencies(<dependencies_file> <install_dir> [<export_set>])
#
# <export_set> is optional. If given, header-only targets created for
# dependencies declared with "build": "none" are installed into that export
# set, so consumers exporting their own targets do not fail with
# "requires target X that is not in any export set".
function(fetch_dependencies DEPENDENCIES_FILE INSTALL_DIR)
  set(EXPORT_SET "")

  if(ARGC GREATER 2)
    set(EXPORT_SET "${ARGV2}")
  endif()

  set(FETCHCONTENT_BASE_DIR "${INSTALL_DIR}")

  set(PATCH_HELPER "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/apply_patch.cmake")

  # Pointing SOURCE_SUBDIR at a path that contains no CMakeLists.txt makes
  # FetchContent populate the sources and skip add_subdirectory. This is the
  # documented way to consume a dependency that ships no CMake at all. The
  # path must not exist in any dependency.
  set(NO_CMAKE_SUBDIR "_fetch_no_cmake")

  set(NO_CMAKE_DEPENDENCIES)

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

    # Build mode
    #
    #   cmake (default) The dependency configures itself via add_subdirectory.
    #                   Use source.subdir when its CMakeLists.txt is not at the
    #                   repository root (lz4, zstd: build/cmake).
    #
    #   none            The dependency ships no CMake. Only the sources are
    #                   downloaded; a header-only target is created below.
    #                   Set include_dir when the headers live in a
    #                   subdirectory; it defaults to the repository root.

    unset(ERR)

    string(JSON BUILD_MODE ERROR_VARIABLE ERR GET "${JSON}" "${NAME}" build)

    if(ERR)
      set(BUILD_MODE "cmake")
    endif()

    unset(ERR)

    string(JSON SUBDIR ERROR_VARIABLE ERR GET "${JSON}" "${NAME}" source subdir)

    if(ERR)
      set(SUBDIR "")
    endif()

    if(BUILD_MODE STREQUAL "none")
      if(SUBDIR)
        message(FATAL_ERROR "'${NAME}' sets build=none and source.subdir; these are mutually exclusive")
      endif()

      list(APPEND DECLARE_ARGS SOURCE_SUBDIR "${NO_CMAKE_SUBDIR}")

      list(APPEND NO_CMAKE_DEPENDENCIES "${NAME}")

      message(STATUS "    build: none (header-only)")
    elseif(BUILD_MODE STREQUAL "cmake")
      if(SUBDIR)
        list(APPEND DECLARE_ARGS SOURCE_SUBDIR "${SUBDIR}")

        message(STATUS "    subdir: ${SUBDIR}")
      endif()
    else()
      message(FATAL_ERROR "Unknown build mode '${BUILD_MODE}' for dependency '${NAME}'")
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

        install(
          TARGETS ${ADOPT_TARGET} 
          EXPORT ${EXPORT_SET}
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
          INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )

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

    # Header-only target
    #
    # Reached only for build=none. A build=cmake dependency that produced no
    # target named after its key and declared no 'target' is a configuration
    # error, not something to paper over with an empty INTERFACE library.

    if(NOT "${NAME}" IN_LIST NO_CMAKE_DEPENDENCIES)
      message(FATAL_ERROR "'${NAME}' builds with CMake but defines no target '${NAME}' and declares no 'target' key")
    endif()

    unset(ERR)

    string(JSON INCLUDE_DIR ERROR_VARIABLE ERR GET "${JSON}" "${NAME}" include_dir)

    if(ERR)
      set(INCLUDE_DIR "")
    endif()

    if(INCLUDE_DIR)
      set(INCLUDE_ROOT "${${NAME_LOWER}_SOURCE_DIR}/${INCLUDE_DIR}")
    else()
      set(INCLUDE_ROOT "${${NAME_LOWER}_SOURCE_DIR}")
    endif()

    if(NOT IS_DIRECTORY "${INCLUDE_ROOT}")
      message(FATAL_ERROR "Include directory for '${NAME}' does not exist: ${INCLUDE_ROOT}")
    endif()

    add_library(${NAME} INTERFACE)

    target_include_directories(
      ${NAME} 
      INTERFACE 
        $<BUILD_INTERFACE:${INCLUDE_ROOT}> 
        $<INSTALL_INTERFACE:include>
    )

    message(STATUS "  ${NAME}: header-only target from ${INCLUDE_ROOT}")

    if(EXPORT_SET)
      install(
        TARGETS ${NAME} 
        EXPORT ${EXPORT_SET}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      )

      install(
        DIRECTORY "${INCLUDE_ROOT}/" 
        DESTINATION include 
        FILES_MATCHING 
          PATTERN "*.h" 
          PATTERN "*.hpp" 
          PATTERN "*.inl"
      )
    endif()
  endforeach()

  message(STATUS "Dependencies resolved")

endfunction()
