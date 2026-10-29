cmake_minimum_required(VERSION 3.16)
project(Atomica LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # lets clangd (your Neovim setup) find headers

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- physics core: no OpenGL needed, so it can be unit-tested headless ----
add_library(orbital STATIC src/orbital.cpp)
target_include_directories(orbital PUBLIC src)

# ---- the application ----
find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

add_executable(atomica
  src/main.cpp
  src/raster.cpp
  src/stages2d.cpp
  src/orbital_view.cpp
  src/glutil.cpp)
target_link_libraries(atomica PRIVATE orbital glfw OpenGL::GL)

# ---- tests (run with: ctest --test-dir build --output-on-failure) ----
enable_testing()
add_executable(orbital_test tests/orbital_test.cpp)
target_link_libraries(orbital_test PRIVATE orbital)
add_test(NAME orbital_test COMMAND orbital_test)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  foreach(t atomica orbital orbital_test)
    target_compile_options(${t} PRIVATE -Wall -Wextra)
  endforeach()
endif()
