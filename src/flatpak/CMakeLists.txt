add_executable(kyu-flatpak main.cpp flatpakupdater.cpp)
target_link_libraries(kyu-flatpak PRIVATE kyucore kyuclient PkgConfig::FLATPAK)
target_compile_definitions(kyu-flatpak PRIVATE KYU_VERSION="${PROJECT_VERSION}")
install(TARGETS kyu-flatpak DESTINATION ${CMAKE_INSTALL_BINDIR})
