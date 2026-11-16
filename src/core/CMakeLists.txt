add_library(kyucore STATIC
    paths.cpp
    pacmanparse.cpp
    pacmancheck.cpp
    flatpakcheck.cpp
    statustext.cpp
    updatestate.cpp
)
target_include_directories(kyucore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kyucore PUBLIC Qt6::Core)
if(KYU_FLATPAK)
    target_compile_definitions(kyucore PRIVATE KYU_HAVE_FLATPAK)
    target_link_libraries(kyucore PRIVATE PkgConfig::FLATPAK)
endif()
