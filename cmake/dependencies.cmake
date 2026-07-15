include(FetchContent)

function(fetch_dependencies DEPENDENCIES_FILE INSTALL_DIR)
  set(FETCHCONTENT_BASE_DIR "${INSTALL_DIR}")

  file(READ "${DEPENDENCIES_FILE}" JSON)

  string(JSON DEP_COUNT LENGTH "${JSON}")

  if(DEP_COUNT EQUAL 0)
    return()
  endif()

  math(EXPR DEP_LAST "${DEP_COUNT} - 1")

  set(DEPENDENCIES_TO_FETCH)

  foreach(I RANGE ${DEP_LAST})
    string(JSON NAME MEMBER "${JSON}" ${I})

    unset(ERR)

    string(JSON CONDITION ERROR_VARIABLE ERR GET "${JSON}" "${NAME}" condition)

    if(NOT ERR)
      if(NOT DEFINED ${CONDITION} OR NOT ${${CONDITION}})
        continue()
      endif()
    endif()

    string(JSON TYPE GET "${JSON}" "${NAME}" source type)

    if(TYPE STREQUAL "git")
        string(JSON URI GET "${JSON}" "${NAME}" source uri)
        string(JSON TAG GET "${JSON}" "${NAME}" source tag)

        FetchContent_Declare(
          ${NAME} 
          GIT_REPOSITORY ${URI} 
          GIT_TAG ${TAG}
          # FIND_PACKAGE_ARGS CONFIG
        )
    elseif(TYPE STREQUAL "archive")
        string(JSON URI GET "${JSON}" "${NAME}" source uri)
        string(JSON SHA GET "${JSON}" "${NAME}" source sha256)

        FetchContent_Declare(
          ${NAME} 
          URL ${URI} 
          URL_HASH SHA256=${SHA}
          # FIND_PACKAGE_ARGS CONFIG
        )
    else()
        message(FATAL_ERROR "Unknown source type '${TYPE}' for dependency '${NAME}'")
    endif()

    unset(ERR)

    string(JSON OPT_COUNT ERROR_VARIABLE ERR LENGTH "${JSON}" "${NAME}" options)

    if(NOT ERR)
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
        endforeach()

    endif()

    list(APPEND DEPENDENCIES_TO_FETCH ${NAME})

  endforeach()

  if(DEPENDENCIES_TO_FETCH)
    FetchContent_MakeAvailable(${DEPENDENCIES_TO_FETCH})
  endif()

endfunction()
