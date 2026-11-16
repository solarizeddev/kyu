# kyu-window: the update in a window of Kyu's own, with Konsole's terminal
# inside (docs/TERMINAL-FIRST.md). Built where KF6 Parts is available.
add_executable(kyu-window main.cpp)
target_link_libraries(kyu-window PRIVATE kyucore Qt6::Widgets KF6::Parts KF6::CoreAddons)
target_compile_definitions(kyu-window PRIVATE KYU_VERSION="${PROJECT_VERSION}" KYU_APP_ID="${KYU_APP_ID}")
install(TARGETS kyu-window DESTINATION ${CMAKE_INSTALL_BINDIR})
