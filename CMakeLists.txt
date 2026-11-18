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

# Notifications ("News", "The update is waiting for you"), each switchable
# in System Settings → Notifications through kyu.notifyrc.
find_package(KF6Notifications QUIET)
option(KYU_REQUIRE_NOTIFICATIONS "Fail the build if KF6 Notifications is missing" OFF)
if(KF6Notifications_FOUND)
    message(STATUS "Notifications: KNotification")
elseif(KYU_REQUIRE_NOTIFICATIONS)
    message(FATAL_ERROR "KF6Notifications not found")
else()
    message(WARNING "KF6Notifications not found: no notifications (the tray and the taskbar still show the state)")
endif()

# kyu-window embeds Konsole's terminal part (KF6 Parts; Konsole at run time).
find_package(KF6Parts QUIET)
find_package(KF6CoreAddons QUIET)
option(KYU_REQUIRE_WINDOW "Fail the build if KF6 Parts (for kyu-window) is missing" OFF)
if(KF6Parts_FOUND AND KF6CoreAddons_FOUND)
    set(KYU_WINDOW ON)
    message(STATUS "kyu-window: with Konsole's terminal part")
elseif(KYU_REQUIRE_WINDOW)
    message(FATAL_ERROR "KF6 Parts not found (needed for kyu-window)")
else()
    message(WARNING "KF6 Parts not found: kyu-window is not built (kyu-update opens in Konsole instead)")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
add_compile_definitions(QT_NO_KEYWORDS QT_NO_CAST_FROM_ASCII QT_NO_URL_CAST_FROM_STRING)

set(KYU_APP_ID "io.github.solarizeddev.kyu")
set(KYU_SYSTEMD_USER_UNIT_DIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/user"
    CACHE PATH "Where the systemd user units are installed")

# Translations (translations/kyu_*.ts): Kyu's own words. lrelease builds the
# .qm files when Qt's Linguist tools are there; "kyu-lupdate" refreshes the
# .ts files from the sources.
find_package(Qt6 QUIET COMPONENTS LinguistTools)
file(GLOB KYU_TS_FILES ${CMAKE_SOURCE_DIR}/translations/kyu_*.ts)
set(KYU_QM_DIR ${CMAKE_BINARY_DIR}/translations)
set(KYU_TRANSLATIONS_DIR ${CMAKE_INSTALL_FULL_DATADIR}/kyu/translations)

add_subdirectory(src)
add_subdirectory(data)
if(Qt6LinguistTools_FOUND AND KYU_TS_FILES)
    # Before qt_add_lrelease, which reads it: the .qm files in build/translations.
    set_source_files_properties(${KYU_TS_FILES} PROPERTIES OUTPUT_LOCATION ${KYU_QM_DIR})
    qt_add_lrelease(kyucore TS_FILES ${KYU_TS_FILES} QM_FILES_OUTPUT_VARIABLE KYU_QM_FILES OPTIONS -silent)
    install(FILES ${KYU_QM_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/kyu/translations)
    add_custom_target(kyu-lupdate
        COMMAND Qt6::lupdate -tr-function-alias tr+=numbered -locations relative -no-obsolete
                ${CMAKE_SOURCE_DIR}/src -ts ${KYU_TS_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Refreshing translations/kyu_*.ts from the sources")
elseif(KYU_TS_FILES)
    message(WARNING "Qt6 LinguistTools not found: translations are not built (Kyu stays English)")
endif()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
