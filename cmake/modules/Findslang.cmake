# Findslang.cmake
# Cross-platform find module for Slang (as bundled with the Vulkan SDK or standalone)
#
# Provides:
#   slang::slang  - imported target with include + library paths
#   slang_FOUND   - true if found
#   slang_INCLUDE_DIR
#   slang_LIBRARY
#   slang_DLL (Windows only)

# Typical layout:
#   Windows: $ENV{VULKAN_SDK}/Include/slang, /Lib/slang.lib, /Bin/slang.dll
#   Linux:   $ENV{VULKAN_SDK}/include/slang, /lib/libslang.so

# --- Search paths ----------------------------------------------------------

include(FindPackageHandleStandardArgs)

set(_SLANG_INCLUDE_HINTS
  "$ENV{VULKAN_SDK}/Include"
  "$ENV{VULKAN_SDK}/include"
  "/usr/include"
  "/usr/local/include"
  "/opt/homebrew/include"
  "/opt/local/include"
)

set(_SLANG_LIB_HINTS
  "$ENV{VULKAN_SDK}/Lib"
  "$ENV{VULKAN_SDK}/lib"
  "/usr/lib"
  "/usr/local/lib"
  "/opt/homebrew/lib"
  "/opt/local/lib"
)

find_path(slang_INCLUDE_DIR
  NAMES slang/slang.h
  HINTS ${_SLANG_INCLUDE_HINTS}
)

# Library names differ across platforms
if(WIN32)
  set(_SLANG_LIB_NAMES slang.lib slang)
elseif(APPLE)
  set(_SLANG_LIB_NAMES slang libslang.dylib libslang)
else()
  set(_SLANG_LIB_NAMES slang libslang.so libslang)
endif()

find_library(slang_LIBRARY
  NAMES ${_SLANG_LIB_NAMES}
  HINTS ${_SLANG_LIB_HINTS}
)

# Optional runtime for Windows
if(WIN32)
  find_file(slang_DLL
    NAMES slang.dll
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
  )
endif()

find_package_handle_standard_args(
  slang
  REQUIRED_VARS slang_INCLUDE_DIR slang_LIBRARY
)

# --- Diagnostics (optional) ---
message(STATUS "slang_FOUND: ${slang_FOUND}")
message(STATUS "slang_INCLUDE_DIR: ${slang_INCLUDE_DIR}")
message(STATUS "slang_LIBRARY: ${slang_LIBRARY}")
message(STATUS "slang_DLL: ${slang_DLL}")

# --- Imported target ---
if(slang_FOUND AND NOT TARGET slang::slang)
  if(WIN32)
    if(slang_DLL AND EXISTS "${slang_DLL}")
      # Shared (DLL + import lib)
      add_library(slang::slang SHARED IMPORTED GLOBAL)
      set_target_properties(slang::slang PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${slang_INCLUDE_DIR}"
        IMPORTED_IMPLIB "${slang_LIBRARY}"
        IMPORTED_LOCATION "${slang_DLL}"
        IMPORTED_IMPLIB_DEBUG "${slang_LIBRARY}"
        IMPORTED_LOCATION_DEBUG "${slang_DLL}"
        IMPORTED_IMPLIB_RELEASE "${slang_LIBRARY}"
        IMPORTED_LOCATION_RELEASE "${slang_DLL}"
      )
    else()
      # Static (.lib only)
      add_library(slang::slang STATIC IMPORTED GLOBAL)
      set_target_properties(slang::slang PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${slang_INCLUDE_DIR}"
        IMPORTED_LOCATION "${slang_LIBRARY}"
        IMPORTED_LOCATION_DEBUG "${slang_LIBRARY}"
        IMPORTED_LOCATION_RELEASE "${slang_LIBRARY}"
      )
    endif()
  else()
    # Unix-like (Linux/macOS): .so/.dylib path is the location
    add_library(slang::slang SHARED IMPORTED GLOBAL)
    set_target_properties(slang::slang PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${slang_INCLUDE_DIR}"
      IMPORTED_LOCATION "${slang_LIBRARY}"
      IMPORTED_LOCATION_DEBUG "${slang_LIBRARY}"
      IMPORTED_LOCATION_RELEASE "${slang_LIBRARY}"
    )
  endif()
endif()

mark_as_advanced(slang_INCLUDE_DIR slang_LIBRARY slang_DLL)
