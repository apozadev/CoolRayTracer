# CMakeList.txt: proyecto de CMake para CoolerRayTracer, incluya el origen y defina
# la lógica específica del proyecto aquí.
#

# Agregue un origen al ejecutable de este proyecto.
add_executable (CoolerRayTracer WIN32 "CoolerRayTracer.cpp" "../Platform/win32_main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET CoolerRayTracer PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(CoolerRayTracer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../Util")

# TODO: Agregue pruebas y destinos de instalación si es necesario.

# Add maximum warning levels for different compilers
if (MSVC)
    target_compile_options(CoolerRayTracer PRIVATE /W4 /WX)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(CoolerRayTracer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
