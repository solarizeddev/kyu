add_executable(kyu-check main.cpp)
target_link_libraries(kyu-check PRIVATE kyucore)
target_compile_definitions(kyu-check PRIVATE KYU_VERSION="${PROJECT_VERSION}")
install(TARGETS kyu-check DESTINATION ${CMAKE_INSTALL_BINDIR})
