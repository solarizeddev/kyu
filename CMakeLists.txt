cmake_minimum_required(VERSION 3.22)
project(kyu VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(GNUInstallDirs)
include(CTest)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets DBus Network)

# Flatpak updates are counted through libflatpak. Packagers can switch it off
# to drop the dependency; the checker then reports Flatpak as not checked.
option(KYU_FLATPAK "Count Flatpak updates through libflatpak" ON)
if(KYU_FLATPAK)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FLATPAK REQUIRED IMPORTED_TARGET flatpak)
endif()

# KStatusNotifierItem is the real tray backend. QSystemTrayIcon is only a
# fallback so the code builds where KF6 is not available.
find_package(KF6StatusNotifierItem QUIET)
option(KYU_REQUIRE_KSNI "Fail the build if KStatusNotifierItem is missing" OFF)
if(KF6StatusNotifierItem_FOUND)
    message(STATUS "Tray backend: KStatusNotifierItem")
elseif(KYU_REQUIRE_KSNI)
    message(FATAL_ERROR "KF6StatusNotifierItem not found")
else()
    message(WARNING "KF6StatusNotifierItem not found, falling back to QSystemTrayIcon")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
add_compile_definitions(QT_NO_KEYWORDS QT_NO_CAST_FROM_ASCII QT_NO_URL_CAST_FROM_STRING)

set(KYU_APP_ID "io.github.solarizeddev.kyu")
set(KYU_SYSTEMD_USER_UNIT_DIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/user"
    CACHE PATH "Where the systemd user units are installed")

# The privileged helper is Rust (see docs/HANDOFF.md), built through Corrosion
# so that `cmake --build` stays the only entry point.
option(KYU_HELPER "Build the privileged helper (needs cargo)" ON)
if(KYU_HELPER)
    find_package(Corrosion 0.5 QUIET)
    if(NOT Corrosion_FOUND)
        include(FetchContent)
        FetchContent_Declare(Corrosion
            GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
            GIT_TAG v0.6.1)
        FetchContent_MakeAvailable(Corrosion)
    endif()
    corrosion_import_crate(MANIFEST_PATH helper/Cargo.toml LOCKED)
    install(IMPORTED_RUNTIME_ARTIFACTS kyu-helper DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
endif()

add_subdirectory(src)
add_subdirectory(data)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
