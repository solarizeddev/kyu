add_library(kyuclient STATIC helperclient.cpp failuretext.cpp)
target_include_directories(kyuclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kyuclient PUBLIC Qt6::Core Qt6::DBus)

# The old root helper's command-line client, for the helper's tests until the
# helper is removed (docs/TERMINAL-FIRST.md). Not installed.
add_executable(kyu-helper-cli main.cpp)
target_link_libraries(kyu-helper-cli PRIVATE kyuclient)
target_compile_definitions(kyu-helper-cli PRIVATE KYU_VERSION="${PROJECT_VERSION}")
