cmake_minimum_required(VERSION 3.16)
project(skc_e195 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2ALL REQUIRED sdl2 SDL2_ttf)

add_executable(skc_e195
  src/main.cpp
  src/save.cpp
  src/mundo/world.cpp
  src/animais/fauna.cpp
  src/pensantes/creature.cpp
  src/deus/god.cpp
  src/ui/hud.cpp
)

target_include_directories(skc_e195 PRIVATE
  src
  src/mundo
  src/animais
  src/pensantes
  src/deus
  src/ui
  ${SDL2ALL_INCLUDE_DIRS}
)
target_link_directories(skc_e195 PRIVATE ${SDL2ALL_LIBRARY_DIRS})
target_link_libraries(skc_e195 PRIVATE ${SDL2ALL_LIBRARIES})
target_compile_options(skc_e195 PRIVATE ${SDL2ALL_CFLAGS_OTHER} -Wall -Wextra)
