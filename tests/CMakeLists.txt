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

if(KYU_HELPER)
    find_program(DBUS_RUN_SESSION dbus-run-session)
    find_program(GDBUS gdbus)
    if(DBUS_RUN_SESSION AND GDBUS)
        add_test(NAME helper_session
                 COMMAND ${DBUS_RUN_SESSION} -- sh ${CMAKE_CURRENT_SOURCE_DIR}/helper_session_test.sh
                         ${CMAKE_BINARY_DIR}/bin)
        set_tests_properties(helper_session PROPERTIES TIMEOUT 180)
    else()
        message(STATUS "dbus-run-session or gdbus not found: helper_session test disabled")
    endif()
    find_program(FLATPAK_TOOL flatpak)
    if(KYU_FLATPAK AND DBUS_RUN_SESSION AND GDBUS AND FLATPAK_TOOL)
        add_test(NAME flatpak_update
                 COMMAND ${DBUS_RUN_SESSION} -- sh ${CMAKE_CURRENT_SOURCE_DIR}/flatpak_test.sh ${CMAKE_BINARY_DIR}/bin)
        set_tests_properties(flatpak_update PROPERTIES TIMEOUT 300)
    else()
        message(STATUS "flatpak, dbus-run-session or gdbus not found: flatpak_update test disabled")
    endif()
    add_test(NAME helper_unit COMMAND cargo test --locked --manifest-path ${CMAKE_SOURCE_DIR}/helper/Cargo.toml
             --target-dir ${CMAKE_BINARY_DIR}/cargo-test)
endif()

add_executable(test_helperclient test_helperclient.cpp)
target_link_libraries(test_helperclient PRIVATE kyuclient Qt6::Test)
add_test(NAME test_helperclient COMMAND test_helperclient)
