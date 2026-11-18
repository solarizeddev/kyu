# kyu-update: the update in the user's terminal (docs/TERMINAL-FIRST.md).
add_executable(kyu-update main.cpp console.cpp apply.cpp)
target_link_libraries(kyu-update PRIVATE kyucore kyuclient)
target_compile_definitions(kyu-update PRIVATE KYU_VERSION="${PROJECT_VERSION}")
install(TARGETS kyu-update DESTINATION ${CMAKE_INSTALL_BINDIR})
