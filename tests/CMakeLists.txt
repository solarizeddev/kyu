find_package(Qt6 REQUIRED COMPONENTS Test)

function(kyu_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE kyucore Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

kyu_test(test_pacmanparse)
kyu_test(test_updatestate)
kyu_test(test_news)
kyu_test(test_aftercare)
kyu_test(test_traystate)

# kyu-update end to end, with fake sudo/pacman/flatpak and util-linux script.
find_program(SCRIPT_TOOL script)
if(SCRIPT_TOOL)
    add_test(NAME kyu_update COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/kyu_update_test.sh ${CMAKE_BINARY_DIR}/bin)
    set_tests_properties(kyu_update PROPERTIES TIMEOUT 120)
else()
    message(STATUS "script (util-linux) not found: kyu_update test disabled")
endif()

kyu_test(test_kyucheck)
target_compile_definitions(test_kyucheck PRIVATE KYU_CHECK_BIN="$<TARGET_FILE:kyu-check>")
add_dependencies(test_kyucheck kyu-check)

