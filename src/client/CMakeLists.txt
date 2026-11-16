add_library(kyuclient STATIC helperclient.cpp failuretext.cpp)
target_include_directories(kyuclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kyuclient PUBLIC Qt6::Core Qt6::DBus)

add_executable(kyu-update main.cpp)
target_link_libraries(kyu-update PRIVATE kyuclient)
target_compile_definitions(kyu-update PRIVATE KYU_VERSION="${PROJECT_VERSION}")
install(TARGETS kyu-update DESTINATION ${CMAKE_INSTALL_BINDIR})
