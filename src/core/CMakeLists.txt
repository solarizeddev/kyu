add_library(kyucore STATIC
    paths.cpp
    pacmanparse.cpp
    pacmancheck.cpp
    flatpakcheck.cpp
    statustext.cpp
    updatestate.cpp
    statewatcher.cpp
    checkscheduler.cpp
    flatpakwarnings.cpp
    news.cpp
    aftercare.cpp
    failure.cpp
    keyrings.cpp
    aur.cpp
    translations.cpp
    traystate.cpp
    waiting.cpp
    notices.cpp
    launch.cpp
    housekeeping.cpp
    i18n.cpp
)
target_include_directories(kyucore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kyucore PUBLIC Qt6::Core Qt6::Network)
target_compile_definitions(kyucore PRIVATE KYU_CHECK_BINARY="${CMAKE_INSTALL_FULL_BINDIR}/kyu-check"
    KYU_BUILD_TRANSLATIONS="${KYU_QM_DIR}" KYU_INSTALLED_TRANSLATIONS="${KYU_TRANSLATIONS_DIR}")
if(KYU_FLATPAK)
    target_compile_definitions(kyucore PRIVATE KYU_HAVE_FLATPAK)
    target_link_libraries(kyucore PRIVATE PkgConfig::FLATPAK)
endif()
