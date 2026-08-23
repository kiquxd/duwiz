cmake_minimum_required(VERSION 3.24)
project(duwiz LANGUAGES C CXX)

include(CTest)
include(GNUInstallDirs)
find_package(Threads REQUIRED)

set(PREVIEW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/preview_lib" CACHE PATH
    "Path to the preview library source tree")
set(FTXUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps/ftxui" CACHE PATH
    "Path to the pinned FTXUI checkout")

if(NOT EXISTS "${FTXUI_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR
    "FTXUI is not prepared. Run scripts/bootstrap_dependencies.sh first.")
endif()
if(NOT EXISTS "${PREVIEW_ROOT}/CMakeLists.txt" OR
   NOT EXISTS "${PREVIEW_ROOT}/include/preview/preview.hpp")
  message(FATAL_ERROR
    "preview_lib source tree was not found at ${PREVIEW_ROOT}. "
    "Run git submodule update --init --recursive, then bootstrap dependencies.")
endif()

set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${FTXUI_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/ftxui")

set(PREVIEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PREVIEW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PREVIEW_ENABLE_SANITIZERS OFF CACHE BOOL "" FORCE)
add_subdirectory("${PREVIEW_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/preview")

add_executable(duwiz
  src/bin/main.cpp
  src/lib/argparser.cpp
  src/lib/fs.cpp
  src/lib/preview_service.cpp
  src/lib/system_opener.cpp
  src/lib/thread_pool.cpp
  src/tui/preview_view.cpp
  src/tui/utils.cpp)
target_compile_features(duwiz PRIVATE cxx_std_20)
target_compile_options(duwiz PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(duwiz PRIVATE src)
target_link_libraries(duwiz PRIVATE
  preview::preview
  ftxui::component
  ftxui::dom
  ftxui::screen
  Threads::Threads)
if(APPLE)
  set(DUWIZ_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
else()
  set(DUWIZ_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()
set_target_properties(duwiz PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  INSTALL_RPATH "${DUWIZ_INSTALL_RPATH}")

install(TARGETS duwiz
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(BUILD_TESTING)
  add_executable(argparser_test
    tests/argparser_test.cpp
    src/lib/argparser.cpp)
  target_compile_features(argparser_test PRIVATE cxx_std_20)
  target_compile_options(argparser_test PRIVATE -Wall -Wextra -Wpedantic)
  target_include_directories(argparser_test PRIVATE src)
  add_test(NAME argparser_defaults COMMAND argparser_test)
  add_test(NAME argparser_explicit_options
    COMMAND argparser_test -p /tmp/duwiz-argparser-test -j 3)

  add_executable(preview_integration_test
    tests/preview_integration_test.cpp
    src/lib/preview_service.cpp
    src/tui/preview_view.cpp)
  target_compile_features(preview_integration_test PRIVATE cxx_std_20)
  target_compile_options(preview_integration_test PRIVATE
    -Wall -Wextra -Wpedantic)
  target_compile_definitions(preview_integration_test PRIVATE
    PREVIEW_TEST_CORPUS="${PREVIEW_ROOT}/tests/corpus")
  target_include_directories(preview_integration_test PRIVATE src)
  target_link_libraries(preview_integration_test PRIVATE
    preview::preview
    ftxui::dom
    ftxui::screen
    Threads::Threads)
  add_test(NAME preview_integration_test COMMAND preview_integration_test)
endif()
