cmake_minimum_required(VERSION 3.21)
project(boost-fiber-test)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake"
                TLS_VERIFY ON)
endif()
include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_configure(
  REQUIRES boost/1.80.0
  GENERATORS cmake
)

if(NOT CMAKE_BUILD_TYPE)
  set(conan_build_type Debug)
else()
  set(conan_build_type ${CMAKE_BUILD_TYPE})
endif()

message("conan_build_type: ${conan_build_type}")
conan_cmake_autodetect(settings
  BUILD_TYPE ${conan_build_type}
)

conan_cmake_install(PATH_OR_REFERENCE .
  BUILD missing
  REMOTE conancenter
  SETTINGS ${settings}
)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(${PROJECT_NAME} src/main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_precompile_headers(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
target_link_libraries(${PROJECT_NAME} PRIVATE CONAN_PKG::boost)
set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  LINKER_LANGUAGE CXX
)