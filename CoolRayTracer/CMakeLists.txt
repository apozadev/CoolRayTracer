# CMakeList.txt: proyecto de CMake para CoolRayTracer, incluya el origen y defina
# la lógica específica del proyecto aquí.
#

# Agregue un origen al ejecutable de este proyecto.
add_executable (CoolRayTracer WIN32 "CoolRayTracer.cpp" "../Platform/win32_main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET CoolRayTracer PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(CoolRayTracer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../Util")

# TODO: Agregue pruebas y destinos de instalación si es necesario.

# Add maximum warning levels for different compilers
if (MSVC)
    target_compile_options(CoolRayTracer PRIVATE /W4 /WX)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(CoolRayTracer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
