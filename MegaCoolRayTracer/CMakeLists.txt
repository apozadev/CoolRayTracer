# CMakeList.txt: proyecto de CMake para MegaCoolRayTracer, incluya el origen y defina
# la lógica específica del proyecto aquí.
#

# Agregue un origen al ejecutable de este proyecto.
add_executable (MegaCoolRayTracer WIN32 "MegaCoolRayTracer.cpp" "../Platform/win32_main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MegaCoolRayTracer PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(MegaCoolRayTracer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../Util")

# TODO: Agregue pruebas y destinos de instalación si es necesario.

# Add maximum warning levels for different compilers
if (MSVC)
    target_compile_options(MegaCoolRayTracer PRIVATE /W4 /WX)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(MegaCoolRayTracer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Lets the compiler vectorize the sphere loops 8 floats wide, like SuperCoolRayTracer's intrinsics.
# With /arch:AVX, MSVC only vectorizes them 4 wide; /arch:AVX2 gets the full 256-bit width. Under the
# default /fp:precise MSVC doesn't fuse multiplies and adds, so results stay identical to the other
# tracers; clang does by default, so it's told not to. /Qvec-report:1 lists the loops MSVC
# vectorized in the build output.
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(MegaCoolRayTracer PRIVATE /arch:AVX2 /Qvec-report:1)
elseif (MSVC)
    target_compile_options(MegaCoolRayTracer PRIVATE /arch:AVX2 /clang:-ffp-contract=off)
else()
    target_compile_options(MegaCoolRayTracer PRIVATE -mavx2 -ffp-contract=off)
endif()
