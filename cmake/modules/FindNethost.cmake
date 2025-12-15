# FindNethost.cmake
# Usage:
#   find_package(Nethost CONFIG REQUIRED)
# Provides:
#   Nethost::nethost  (IMPORTED target)
#   NETHOST_INCLUDE_DIR
#   NETHOST_LIBRARY
# Options/Hints:
#   -DNETHOST_ROOT=/path/to/dotnet        # overrides root
#   -DDOTNET_ROOT=/path/to/dotnet         # typical root (…/dotnet)
#   -DNETHOST_RID=linux-x64               # override RID if needed

include_guard(GLOBAL)

# ----- helpers ---------------------------------------------------------------
function(_nh_append_if_exists _outvar)
  foreach(_p IN LISTS ARGN)
    if(EXISTS "${_p}")
      list(APPEND ${_outvar} "${_p}")
    endif()
  endforeach()
  set(${_outvar} "${${_outvar}}" PARENT_SCOPE)
endfunction()

# ----- derive DOTNET_ROOT ----------------------------------------------------
# Priority:
#  1) NETHOST_ROOT (explicit)
#  2) DOTNET_ROOT (cmake var)
#  3) $ENV{DOTNET_ROOT}
#  4) dotnet --list-sdks (strip /sdk)
#  5) common defaults per OS
set(_NH_DOTNET_ROOT "")

if(NETHOST_ROOT)
  set(_NH_DOTNET_ROOT "${NETHOST_ROOT}")
elseif(DOTNET_ROOT)
  set(_NH_DOTNET_ROOT "${DOTNET_ROOT}")
elseif(DEFINED ENV{DOTNET_ROOT})
  set(_NH_DOTNET_ROOT "$ENV{DOTNET_ROOT}")
else()
  # Try to discover via dotnet CLI
  find_program(_NH_DOTNET dotnet)
  if(_NH_DOTNET)
    execute_process(COMMAND "${_NH_DOTNET}" --list-sdks
                    OUTPUT_VARIABLE _NH_SDKS
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
    if(_NH_SDKS)
      # Take the first [path] occurrence, e.g. "9.0.110 [/usr/lib/dotnet/sdk]"
      string(REGEX MATCH "\\[([^\\]]+)\\]" _NH_MATCH "${_NH_SDKS}")
      if(_NH_MATCH)
        string(REGEX REPLACE "^\\[|\\]$" "" _NH_SDK_DIR "${_NH_MATCH}") # /usr/lib/dotnet/sdk
        get_filename_component(_NH_DOTNET_ROOT "${_NH_SDK_DIR}" DIRECTORY) # -> /usr/lib/dotnet
      endif()
    endif()
  endif()
endif()

# Fallbacks if still empty
if(NOT _NH_DOTNET_ROOT)
  if(WIN32)
    set(_NH_DOTNET_ROOT "C:/Program Files/dotnet")
  elseif(APPLE)
    set(_NH_DOTNET_ROOT "/usr/local/share/dotnet")
  else()
    # Linux
    # Try the two most common distro locations
    foreach(_cand "/usr/share/dotnet" "/usr/lib/dotnet" "/opt/dotnet")
      if(EXISTS "${_cand}")
        set(_NH_DOTNET_ROOT "${_cand}")
        break()
      endif()
    endforeach()
  endif()
endif()

# Make it cache-visible for user tweaks
set(DOTNET_ROOT "${_NH_DOTNET_ROOT}" CACHE PATH "Root directory of the .NET installation")

# ----- compute default RID (override with -DNETHOST_RID=...) -----------------
if(NOT NETHOST_RID)
  set(_nh_os "")
  if(WIN32)
    set(_nh_os "win")
  elseif(APPLE)
    set(_nh_os "osx")
  else()
    set(_nh_os "linux")
  endif()

  # Normalize processor to lowercase
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _nh_proc)

  set(_nh_arch "")
  if(_nh_proc MATCHES "^(x86_64|amd64)$")
    set(_nh_arch "x64")
  elseif(_nh_proc MATCHES "^(aarch64|arm64)$")
    set(_nh_arch "arm64")
  elseif(_nh_proc MATCHES "^armv7" OR _nh_proc STREQUAL "arm")
    set(_nh_arch "arm")
  elseif(_nh_proc MATCHES "^i[3-6]86$")
    set(_nh_arch "x86")
  endif()

  if(_nh_os AND _nh_arch)
    set(NETHOST_RID "${_nh_os}-${_nh_arch}")
  endif()
endif()
set(NETHOST_RID "${NETHOST_RID}" CACHE STRING "Runtime Identifier (RID) for the host pack, e.g., linux-x64")

# ----- collect candidate native dirs ----------------------------------------
set(_NH_HINT_DIRS "")

if(DOTNET_ROOT AND EXISTS "${DOTNET_ROOT}/packs")
  set(_packs "${DOTNET_ROOT}/packs")

  # Prefer exact RID path if known
  if(NETHOST_RID)
    file(GLOB _rid_native_dirs
      "${_packs}/Microsoft.NETCore.App.Host.${NETHOST_RID}/*/runtimes/${NETHOST_RID}/native")
    list(APPEND _NH_HINT_DIRS ${_rid_native_dirs})
  endif()

  # Fallback: search any RID
  file(GLOB _any_native_dirs
    "${_packs}/Microsoft.NETCore.App.Host.*/*/runtimes/*/native")
  list(APPEND _NH_HINT_DIRS ${_any_native_dirs})
endif()

# Also consider user overrides
if(NETHOST_ROOT AND EXISTS "${NETHOST_ROOT}")
  list(APPEND _NH_HINT_DIRS "${NETHOST_ROOT}")
endif()
if(DEFINED ENV{NETHOST_ROOT} AND EXISTS "$ENV{NETHOST_ROOT}")
  list(APPEND _NH_HINT_DIRS "$ENV{NETHOST_ROOT}")
endif()

# Common hardcoded natives (just in case someone points directly)
_nh_append_if_exists(_NH_HINT_DIRS
  "${DOTNET_ROOT}/host/fxr"     # not where nethost lives, but kept as edge hint
)

# De-duplicate
list(REMOVE_DUPLICATES _NH_HINT_DIRS)

# ----- find header and library ----------------------------------------------
# Header file is next to the library inside the native dir
find_path(NETHOST_INCLUDE_DIR
  NAMES nethost.h
  HINTS ${_NH_HINT_DIRS}
  NO_DEFAULT_PATH
)

# Library names differ by platform/toolchain
set(_NH_LIB_NAMES)
if(WIN32)
  list(APPEND _NH_LIB_NAMES nethost.lib libnethost.lib)
else()
  list(APPEND _NH_LIB_NAMES libnethost.a nethost.a)
endif()

find_library(NETHOST_LIBRARY
  NAMES ${_NH_LIB_NAMES}
  HINTS ${_NH_HINT_DIRS}
  NO_DEFAULT_PATH
)

# If not found yet, allow default search paths as a last resort
if(NOT NETHOST_INCLUDE_DIR)
  find_path(NETHOST_INCLUDE_DIR nethost.h)
endif()
if(NOT NETHOST_LIBRARY)
  find_library(NETHOST_LIBRARY NAMES ${_NH_LIB_NAMES})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Nethost
  REQUIRED_VARS NETHOST_INCLUDE_DIR NETHOST_LIBRARY
  FAIL_MESSAGE "nethost not found. Set DOTNET_ROOT or NETHOST_ROOT to your dotnet directory (…/dotnet)."
)

if(Nethost_FOUND AND NOT TARGET Nethost::nethost)
  add_library(Nethost::nethost UNKNOWN IMPORTED)
  set_target_properties(Nethost::nethost PROPERTIES
    IMPORTED_LOCATION "${NETHOST_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${NETHOST_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(NETHOST_INCLUDE_DIR NETHOST_LIBRARY DOTNET_ROOT NETHOST_RID)
