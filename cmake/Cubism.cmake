set(CUBISM_CORE_PATH "${BONGO_CAT_CUBISM_SDK}/Core")
set(CUBISM_FRAMEWORK_PATH "${BONGO_CAT_CUBISM_SDK}/Framework")
set(CUBISM_GLEW_PATH "${BONGO_CAT_CUBISM_SDK}/Samples/OpenGL/thirdParty/glew")

# GLEW 2.2.0 still declares compatibility with pre-3.5 CMake. CMake 4 removes
# that compatibility unless the parent project supplies an explicit floor.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

foreach(PATH IN ITEMS CUBISM_FRAMEWORK_PATH CUBISM_GLEW_PATH)
  if(NOT EXISTS "${${PATH}}")
    message(FATAL_ERROR "Incomplete Cubism SDK: ${${PATH}} is missing")
  endif()
endforeach()

add_library(Live2DCubismCore STATIC IMPORTED GLOBAL)
if(WIN32)
  if(MSVC_VERSION LESS 1930 OR MSVC_VERSION GREATER_EQUAL 1950)
    message(FATAL_ERROR "Cubism Windows build requires Visual Studio 2022")
  endif()
  set(CUBISM_CORE_LIBRARY
    "${CUBISM_CORE_PATH}/lib/windows/x86_64/143/Live2DCubismCore_MT.lib")
elseif(APPLE)
  if(CMAKE_OSX_ARCHITECTURES)
    list(GET CMAKE_OSX_ARCHITECTURES 0 CUBISM_ARCH)
  else()
    set(CUBISM_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  set(CUBISM_CORE_LIBRARY
    "${CUBISM_CORE_PATH}/lib/macos/${CUBISM_ARCH}/libLive2DCubismCore.a")
else()
  set(CUBISM_CORE_LIBRARY
    "${CUBISM_CORE_PATH}/lib/linux/x86_64/libLive2DCubismCore.a")
endif()
if(NOT EXISTS "${CUBISM_CORE_LIBRARY}")
  message(FATAL_ERROR "Cubism Core library missing: ${CUBISM_CORE_LIBRARY}")
endif()
set_target_properties(Live2DCubismCore PROPERTIES
  IMPORTED_LOCATION "${CUBISM_CORE_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${CUBISM_CORE_PATH}/include")

set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CUBISM_GLEW_PATH}/build/cmake" "${CMAKE_BINARY_DIR}/cubism-glew")
set(FRAMEWORK_SOURCE OpenGL)
add_subdirectory("${CUBISM_FRAMEWORK_PATH}" "${CMAKE_BINARY_DIR}/cubism-framework")
include(cmake/CubismShaderOptimize.cmake)
bongo_cat_optimize_cubism_shaders(Framework)

if(WIN32)
  target_compile_definitions(Framework PUBLIC CSM_TARGET_WIN_GL)
elseif(APPLE)
  target_compile_definitions(Framework PUBLIC CSM_TARGET_MAC_GL)
else()
  target_compile_definitions(Framework PUBLIC CSM_TARGET_LINUX_GL)
endif()
target_include_directories(Framework PUBLIC "${CUBISM_GLEW_PATH}/include")
target_link_libraries(Framework PUBLIC Live2DCubismCore glew_s)
