cmake_minimum_required(VERSION 3.10)
project(browning-safari C)

set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

file(GLOB SOURCES src/platform/*.c src/engine/*.c src/game/*.c)
add_executable(browning-safari ${SOURCES})
target_include_directories(browning-safari PRIVATE include src/engine)
target_link_libraries(browning-safari PRIVATE PkgConfig::SDL2 m)
target_compile_options(browning-safari PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-format-truncation)
# the original was built for x87; keep float rounding predictable
target_compile_options(browning-safari PRIVATE -ffp-contract=off)

include(GNUInstallDirs)
install(TARGETS browning-safari RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/browning-safari.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications OPTIONAL)
install(FILES packaging/browning-safari.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps OPTIONAL)
install(FILES packaging/browning-safari.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo OPTIONAL)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR} OPTIONAL)
