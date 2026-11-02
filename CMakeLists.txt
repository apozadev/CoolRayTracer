# CMakeList.txt: archivo del proyecto de CMake de nivel superior, establezca la configuración global
# e incluya los subproyectos aquí.
#
cmake_minimum_required (VERSION 3.8)

# Habilite Recarga activa para los compiladores de MSVC si se admiten.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Make Release mode emit PDB for compiled artifacts
add_compile_options("$<$<CONFIG:RELEASE>:/Zi>")
add_link_options("$<$<CONFIG:RELEASE>:/DEBUG>") 

project ("OkayRayTracer")

project ("CoolRayTracer")

project ("SuperCoolRayTracer")

project ("MegaCoolRayTracer")

project ("CoolerRayTracer")

project ("SampleTest")

# Incluya los subproyectos.
add_subdirectory ("OkayRayTracer")
add_subdirectory ("CoolRayTracer")
add_subdirectory ("CoolerRayTracer")
add_subdirectory ("SuperCoolRayTracer")
add_subdirectory ("MegaCoolRayTracer")
add_subdirectory ("SampleTest")
