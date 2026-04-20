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
add_voltis_case(voltis.valid.default_branches valid-default-branches)
add_voltis_case(voltis.valid.default_float_to_string valid-default-float-to-string)
add_voltis_case(voltis.valid.default_int_to_floats valid-default-int-to-floats)
add_voltis_case(voltis.valid.default_while_control_flow valid-default-while-control-flow)
add_voltis_case(voltis.valid.default_void_return valid-default-void-return)
add_voltis_case(voltis.valid.default_dll_import valid-default-dll-import)
add_voltis_case(voltis.valid.emit_vir valid-emit-vir)
add_voltis_case(voltis.valid.emit_vir_conversions valid-emit-vir-conversions)
add_voltis_case(voltis.valid.emit_vir_import_angle valid-emit-vir-import-angle)
add_voltis_case(voltis.valid.emit_vir_fold_const_if valid-emit-vir-fold-const-if)
add_voltis_case(voltis.valid.emit_vir_fold_const_while valid-emit-vir-fold-const-while)
add_voltis_case(voltis.failure.parser_missing_semicolon parser-missing-semicolon)
add_voltis_case(voltis.failure.undefined_symbol sema-undefined-symbol)
add_voltis_case(voltis.failure.undefined_function sema-undefined-function)
add_voltis_case(voltis.failure.bad_arg_count sema-bad-arg-count)
add_voltis_case(voltis.failure.bad_arg_type sema-bad-arg-type)
add_voltis_case(voltis.failure.bad_conversion sema-bad-conversion)
add_voltis_case(voltis.failure.duplicate_symbol sema-duplicate-symbol)
add_voltis_case(voltis.failure.if_condition_not_bool sema-if-condition-not-bool)
add_voltis_case(voltis.failure.while_condition_not_bool sema-while-condition-not-bool)
add_voltis_case(voltis.failure.print_arity sema-print-arity)
add_voltis_case(voltis.failure.bad_round_receiver sema-bad-round-receiver)
add_voltis_case(voltis.failure.var_infer_void sema-var-infer-void)
add_voltis_case(voltis.failure.break_outside_loop sema-break-outside-loop)
add_voltis_case(voltis.failure.continue_outside_loop sema-continue-outside-loop)
add_voltis_case(voltis.failure.nonvoid_return_without_value sema-nonvoid-return-without-value)
add_voltis_case(voltis.failure.void_return_with_value sema-void-return-with-value)
add_voltis_case(voltis.failure.missing_return_path sema-missing-return-path)
add_voltis_case(voltis.failure.extern_missing_import sema-extern-missing-import)
add_voltis_case(voltis.failure.duplicate_import sema-duplicate-import)
