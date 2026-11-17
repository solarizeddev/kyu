# kyu-window: the update in a window of Kyu's own, with Konsole's terminal
# inside (docs/TERMINAL-FIRST.md). Built where KF6 Parts is available.
add_executable(kyu-window main.cpp activator.cpp waitingnotice.cpp)
target_link_libraries(kyu-window PRIVATE kyucore Qt6::Widgets Qt6::DBus KF6::Parts KF6::CoreAddons)
target_compile_definitions(kyu-window PRIVATE KYU_VERSION="${PROJECT_VERSION}" KYU_APP_ID="${KYU_APP_ID}")
install(TARGETS kyu-window DESTINATION ${CMAKE_INSTALL_BINDIR})

# Optional: lets the card's "Show window" bring this window forward on Wayland.
find_package(KF6WindowSystem QUIET)
if(KF6WindowSystem_FOUND)
    target_link_libraries(kyu-window PRIVATE KF6::WindowSystem)
    target_compile_definitions(kyu-window PRIVATE KYU_HAVE_KWINDOWSYSTEM)
endif()

# Optional: "The update is waiting for you" (kyu.notifyrc); without it, only
# the taskbar entry asks for attention.
if(KF6Notifications_FOUND)
    target_link_libraries(kyu-window PRIVATE KF6::Notifications)
    target_compile_definitions(kyu-window PRIVATE KYU_HAVE_KNOTIFICATIONS)
endif()
