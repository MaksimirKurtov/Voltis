set(VOLTIS_CASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cases")
set(VOLTIS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
file(MAKE_DIRECTORY "${VOLTIS_OUTPUT_DIR}")

function(add_voltis_case test_name case_name)
    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_COMMAND}
            "-DCASE=${case_name}"
            "-DVOLTISC=$<TARGET_FILE:voltisc>"
            "-DCASE_DIR=${VOLTIS_CASE_DIR}"
            "-DOUT_DIR=${VOLTIS_OUTPUT_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/RunVoltisCase.cmake"
    )
endfunction()

add_voltis_case(voltis.valid.default_compile valid-default)
add_voltis_case(voltis.valid.emit_vir valid-emit-vir)
add_voltis_case(voltis.failure.parser_missing_semicolon parser-missing-semicolon)
add_voltis_case(voltis.failure.undefined_symbol sema-undefined-symbol)
add_voltis_case(voltis.failure.bad_arg_count sema-bad-arg-count)
add_voltis_case(voltis.failure.bad_arg_type sema-bad-arg-type)
add_voltis_case(voltis.failure.bad_conversion sema-bad-conversion)
