cmake_minimum_required(VERSION 3.15)
project(life-calendar
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# ---------- Dependencies via FetchContent ----------
include(FetchContent)

# FTXUI - Terminal UI library
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui.git
  GIT_TAG        v6.1.9
  GIT_SHALLOW    TRUE
)

# FetchContent_Declare(
#   tomlplusplus
#   GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
#   GIT_TAG        v3.4.0
#   GIT_SHALLOW    TRUE
# )

FetchContent_MakeAvailable(ftxui)

# ---------- Main executable ----------
add_executable(life-calendar
  src/main.cpp
  src/config.cpp
  src/calendar.cpp
  src/diary.cpp
)

target_include_directories(life-calendar PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(life-calendar
  PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)

target_compile_features(life-calendar PRIVATE cxx_std_26)
target_compile_options(life-calendar PRIVATE
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-O3>
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2>
)

# ---------- Install ----------
install(TARGETS life-calendar DESTINATION bin)
