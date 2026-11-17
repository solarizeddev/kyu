# Failure texts for kyu-update and the card (src/client/failuretext.*).
add_library(kyuclient STATIC failuretext.cpp)
target_include_directories(kyuclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kyuclient PUBLIC Qt6::Core PRIVATE kyucore)
