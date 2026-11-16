find_package(Qt6 REQUIRED COMPONENTS Test)

function(kyu_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE kyucore Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

kyu_test(test_pacmanparse)
kyu_test(test_updatestate)

kyu_test(test_kyucheck)
target_compile_definitions(test_kyucheck PRIVATE KYU_CHECK_BIN="$<TARGET_FILE:kyu-check>")
add_dependencies(test_kyucheck kyu-check)
