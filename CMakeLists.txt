cmake_minimum_required(VERSION 3.20)

# Название проекта и стандарт C++
project(Checkers LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==== ИСХОДНИКИ ====
# Явно указываем файл main.cpp в корне.
# Если у тебя есть другие .cpp в подпапках — добавь их сюда списком.
add_executable(Checkers
  main.cpp
)

# ==== ПОИСК ЗАВИСИМОСТЕЙ ЧЕРЕЗ vcpkg ====
# Не забудь в VS Code указать toolchain:
# "cmake.configureSettings": {
#   "CMAKE_TOOLCHAIN_FILE": "/Users/annala/vcpkg/scripts/buildsystems/vcpkg.cmake",
#   "VCPKG_TARGET_TRIPLET": "arm64-osx"
# }

# SDL2 (основная и SDL2main), SDL2_image и nlohmann_json
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Базовые библиотеки линкуем сразу
target_link_libraries(Checkers PRIVATE
  SDL2::SDL2
  SDL2::SDL2main
  nlohmann_json::nlohmann_json
)

# SDL2_image: на macOS/arm64 через vcpkg обычно есть только статическая цель
if (TARGET SDL2_image::SDL2_image)
  target_link_libraries(Checkers PRIVATE SDL2_image::SDL2_image)
elseif (TARGET SDL2_image::SDL2_image-static)
  target_link_libraries(Checkers PRIVATE SDL2_image::SDL2_image-static)
else()
  message(FATAL_ERROR "SDL2_image target not found (neither SDL2_image::SDL2_image nor SDL2_image::SDL2_image-static).")
endif()

# ==== ПОЛЕЗНЫЕ НАСТРОЙКИ (не обязательно, но удобно) ====

# Более информативные сообщения компилятора
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
  target_compile_options(Checkers PRIVATE -Wall -Wextra -Wpedantic)
endif()

# На macOS удобно собирать в Debug/Release из статус-бара CMake Tools
# Ничего дополнительного делать не требуется — CMake Tools сам подставит флаги.

# Если позже захочешь сделать приложение без консоли (GUI bundle) на macOS:
# set_target_properties(Checkers PROPERTIES MACOSX_BUNDLE TRUE)
# и при необходимости перенести ресурсы внутрь .app
