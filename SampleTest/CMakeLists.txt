# CMakeList.txt: proyecto de CMake para CoolRayTracer, incluya el origen y defina
# la lógica específica del proyecto aquí.
#

# Agregue un origen al ejecutable de este proyecto.
add_executable (SampleTest WIN32 "DiskSampleTest.cpp" "../CoolRayTracer/win32_main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SampleTest PROPERTY CXX_STANDARD 20)
endif()

# TODO: Agregue pruebas y destinos de instalación si es necesario.

# Add maximum warning levels for different compilers
if (MSVC)
    target_compile_options(SampleTest PRIVATE /W4 /WX)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(SampleTest PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
