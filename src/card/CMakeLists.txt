# kyu: the card the tray opens (docs/WHAT-KYU-SHOULD-BE.md). Widgets, so
# Plasma draws it in the user's style.
add_executable(kyu main.cpp cardwindow.cpp singleinstance.cpp)
target_link_libraries(kyu PRIVATE kyucore kyuclient Qt6::Widgets Qt6::DBus)
target_compile_definitions(kyu PRIVATE KYU_VERSION="${PROJECT_VERSION}" KYU_APP_ID="${KYU_APP_ID}")
install(TARGETS kyu DESTINATION ${CMAKE_INSTALL_BINDIR})

# Optional: lets a tray click focus an already open card on Wayland.
find_package(KF6WindowSystem QUIET)
if(KF6WindowSystem_FOUND)
    target_link_libraries(kyu PRIVATE KF6::WindowSystem)
    target_compile_definitions(kyu PRIVATE KYU_HAVE_KWINDOWSYSTEM)
endif()

# Optional: KDE's icon engine everywhere (Breeze's monochrome icons follow the
# colour scheme even outside a Plasma session).
find_package(KF6IconThemes QUIET)
if(KF6IconThemes_FOUND)
    target_link_libraries(kyu PRIVATE KF6::IconThemes)
    target_compile_definitions(kyu PRIVATE KYU_HAVE_KICONTHEMES)
endif()
