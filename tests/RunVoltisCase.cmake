if(NOT DEFINED CASE)
    message(FATAL_ERROR "CASE is required")
endif()
if(NOT DEFINED VOLTISC)
    message(FATAL_ERROR "VOLTISC is required")
endif()
if(NOT DEFINED CASE_DIR)
    message(FATAL_ERROR "CASE_DIR is required")
endif()
if(NOT DEFINED OUT_DIR)
    message(FATAL_ERROR "OUT_DIR is required")
endif()

function(assert_contains haystack needle context)
    string(FIND "${haystack}" "${needle}" match_pos)
    if(match_pos EQUAL -1)
        message(FATAL_ERROR
            "${context} missing expected text:\n${needle}\n---- actual output ----\n${haystack}\n---- end output ----")
    endif()
endfunction()

function(assert_not_contains haystack needle context)
    string(FIND "${haystack}" "${needle}" match_pos)
    if(NOT match_pos EQUAL -1)
        message(FATAL_ERROR
            "${context} unexpectedly contained text:\n${needle}\n---- actual output ----\n${haystack}\n---- end output ----")
    endif()
endfunction()

file(MAKE_DIRECTORY "${OUT_DIR}")

set(source_file "")
set(args)
set(expected_exit 0)
set(expected_stdout)
set(expected_stderr)
set(expected_file "")
set(expected_file_contains)
set(expected_file_not_contains)
set(expected_file_hex_contains)
set(expected_file_hex_not_contains)
set(run_emitted FALSE)
set(expected_run_stdout)
set(run_without_source FALSE)
set(expect_benchmark_history FALSE)
set(benchmark_iterations_override "")

if(CASE STREQUAL "valid-default")
    set(source_file "${CASE_DIR}/valid_simple.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_simple.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_simple")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
elseif(CASE STREQUAL "valid-default-branches")
    set(source_file "${CASE_DIR}/valid_branches.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_branches.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_branches")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
elseif(CASE STREQUAL "valid-default-float-to-string")
    set(source_file "${CASE_DIR}/valid_float_to_string.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_float_to_string.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_float_to_string")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "value: 12.75")
elseif(CASE STREQUAL "valid-default-int-to-floats")
    set(source_file "${CASE_DIR}/valid_int_to_floats.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_int_to_floats.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_int_to_floats")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout
        "f32: 7"
        "f64: 7")
elseif(CASE STREQUAL "valid-default-while-control-flow")
    set(source_file "${CASE_DIR}/valid_while_control_flow.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_while_control_flow.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_while_control_flow")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "total: 12")
elseif(CASE STREQUAL "valid-default-void-return")
    set(source_file "${CASE_DIR}/valid_void_return.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_void_return.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_void_return")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout
        "banner"
        "done")
elseif(CASE STREQUAL "valid-default-dll-import")
    set(source_file "${CASE_DIR}/valid_dll_import.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_dll_import.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_dll_import")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "pid: ")
elseif(CASE STREQUAL "valid-default-import-lib")
    set(source_file "${CASE_DIR}/valid_import_lib.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_import_lib.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_import_lib")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "pid: ")
elseif(CASE STREQUAL "valid-default-import-a")
    set(source_file "${CASE_DIR}/valid_import_a.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_import_a.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_import_a")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "pid: ")
elseif(CASE STREQUAL "valid-default-import-so")
    set(source_file "${CASE_DIR}/valid_import_so.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_import_so.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_import_so")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "pid: ")
elseif(CASE STREQUAL "valid-default-import-dylib")
    set(source_file "${CASE_DIR}/valid_import_dylib.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_import_dylib.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_import_dylib")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "pid: ")
elseif(CASE STREQUAL "valid-default-source-import")
    set(source_file "${CASE_DIR}/valid_source_import.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_source_import.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_source_import")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "sum: 5")
elseif(CASE STREQUAL "valid-default-source-import-transitive")
    set(source_file "${CASE_DIR}/valid_source_import_transitive.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_source_import_transitive.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_source_import_transitive")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "deep: 10")
elseif(CASE STREQUAL "valid-default-multi-root-inputs")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_multi_root_inputs.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_multi_root_inputs")
    endif()
    set(run_without_source TRUE)
    set(args
        "${CASE_DIR}/multi_root_main.vlt"
        "${CASE_DIR}/multi_root_helpers.vlt"
        -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "multi: 10")
elseif(CASE STREQUAL "valid-default-multi-root-linker-chain")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_multi_root_linker_chain.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_multi_root_linker_chain")
    endif()
    set(run_without_source TRUE)
    set(args
        "${CASE_DIR}/multi_root_linker_chain_main.vlt"
        "${CASE_DIR}/multi_root_linker_chain_helpers.vlt"
        -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "chain: 50")
elseif(CASE STREQUAL "valid-default-struct-type-surface")
    set(source_file "${CASE_DIR}/valid_struct_type_surface.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_struct_type_surface.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_struct_type_surface")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "ok")
elseif(CASE STREQUAL "valid-default-linker-relocation-stress")
    set(source_file "${CASE_DIR}/valid_linker_relocation_stress.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_linker_relocation_stress.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_linker_relocation_stress")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(run_emitted TRUE)
    set(expected_run_stdout "link: 45")
elseif(CASE STREQUAL "valid-default-pe-layout-metadata")
    set(source_file "${CASE_DIR}/valid_simple.vlt")
    if(WIN32)
        set(expected_file "${OUT_DIR}/valid_pe_layout_metadata.exe")
    else()
        set(expected_file "${OUT_DIR}/valid_pe_layout_metadata")
    endif()
    set(args -o "${expected_file}")
    set(expected_stdout
        "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction."
        "Built executable:")
    set(expected_file_hex_contains
        "4d5a"
        "50450000"
        "2e74657874000000"
        "2e72646174610000"
        "2e69646174610000"
        "6b65726e656c33322e646c6c00"
        "6d73766372742e646c6c00")
    set(expected_file_hex_not_contains
        "2e72656c6f630000")
elseif(CASE STREQUAL "benchmark-mode")
    set(run_without_source TRUE)
    set(args --benchmark)
    set(benchmark_iterations_override "50000")
    set(expected_stdout
        "*** Voltis Benchmark ***"
        "Speed Comparison - Leibniz pi,"
        "Compiled Successfully:"
        "Benchmarked Successfully:"
        "Total Instructions:"
        "Benchmark Time Improvement:"
        "Compile Time Improvement:"
        "Best Benchmark Time:"
        "Best Compile Time:")
    set(expect_benchmark_history TRUE)
elseif(CASE STREQUAL "valid-emit-vir")
    set(source_file "${CASE_DIR}/valid_simple.vlt")
    set(expected_file "${OUT_DIR}/valid_simple.vir")
    set(args --emit-vir -o "${expected_file}")
    set(expected_stdout "Emitted VIR:")
    set(expected_file_contains
        "module {"
        "fn main() -> int32 {"
        "[to_string]"
        "call print [builtin]")
elseif(CASE STREQUAL "valid-emit-vir-conversions")
    set(source_file "${CASE_DIR}/valid_conversions.vlt")
    set(expected_file "${OUT_DIR}/valid_conversions.vir")
    set(args --emit-vir -o "${expected_file}")
    set(expected_stdout "Emitted VIR:")
    set(expected_file_contains
        "[round]"
        "[to_int32_trunc]"
        "[to_bool]"
        "br_if")
elseif(CASE STREQUAL "valid-emit-vir-import-angle")
    set(source_file "${CASE_DIR}/valid_import_angle.vlt")
    set(expected_file "${OUT_DIR}/valid_import_angle.vir")
    set(args --emit-vir -o "${expected_file}")
    set(expected_stdout "Emitted VIR:")
    set(expected_file_contains
        "imports:"
        "extern fn GetCurrentProcessId() -> int32 from \"kernel32.dll\""
        "call GetCurrentProcessId [extern from \"kernel32.dll\"]")
elseif(CASE STREQUAL "valid-emit-vir-fold-const-if")
    set(source_file "${CASE_DIR}/valid_fold_const_if.vlt")
    set(expected_file "${OUT_DIR}/valid_fold_const_if.vir")
    set(args --emit-vir -o "${expected_file}")
    set(expected_stdout "Emitted VIR:")
    set(expected_file_contains
        "module {"
        "\"hot\"")
    set(expected_file_not_contains
        "\"cold\"")
elseif(CASE STREQUAL "valid-emit-vir-fold-const-while")
    set(source_file "${CASE_DIR}/valid_fold_const_while.vlt")
    set(expected_file "${OUT_DIR}/valid_fold_const_while.vir")
    set(args --emit-vir -o "${expected_file}")
    set(expected_stdout "Emitted VIR:")
    set(expected_file_contains
        "module {"
        "ret")
    set(expected_file_not_contains
        "\"never\"")
elseif(CASE STREQUAL "parser-missing-semicolon")
    set(source_file "${CASE_DIR}/parser_missing_semicolon.vlt")
    set(expected_exit 1)
    set(expected_stderr "voltisc error: Parse failed for")
    list(APPEND expected_stderr "Expected ';' at line")
elseif(CASE STREQUAL "sema-undefined-symbol")
    set(source_file "${CASE_DIR}/sema_undefined_symbol.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: undefined symbol 'missing'")
elseif(CASE STREQUAL "sema-undefined-function")
    set(source_file "${CASE_DIR}/sema_undefined_function.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: undefined function 'missingFn'")
elseif(CASE STREQUAL "sema-bad-arg-count")
    set(source_file "${CASE_DIR}/sema_bad_arg_count.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: function 'add' expects 2 argument(s), got 1")
elseif(CASE STREQUAL "sema-bad-arg-type")
    set(source_file "${CASE_DIR}/sema_bad_arg_type.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: argument 1 of 'takesInt' expects 'int32', got 'string'")
elseif(CASE STREQUAL "sema-bad-conversion")
    set(source_file "${CASE_DIR}/sema_bad_conversion.vlt")
    set(expected_exit 1)
    set(expected_stderr
        "error: cannot convert string literal to int32 using ToInt32(): expected a base-10 numeric string")
elseif(CASE STREQUAL "sema-duplicate-symbol")
    set(source_file "${CASE_DIR}/sema_duplicate_symbol.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: duplicate symbol 'x' in current scope")
elseif(CASE STREQUAL "sema-if-condition-not-bool")
    set(source_file "${CASE_DIR}/sema_if_condition_not_bool.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: if condition must be 'bool', got 'int32'")
elseif(CASE STREQUAL "sema-while-condition-not-bool")
    set(source_file "${CASE_DIR}/sema_while_condition_not_bool.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: while condition must be 'bool', got 'int32'")
elseif(CASE STREQUAL "sema-print-arity")
    set(source_file "${CASE_DIR}/sema_print_arity.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: print expects exactly 1 argument, got 2")
elseif(CASE STREQUAL "sema-bad-round-receiver")
    set(source_file "${CASE_DIR}/sema_bad_round_receiver.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: conversion member 'Round()' is not valid for receiver type 'int32'")
elseif(CASE STREQUAL "sema-var-infer-void")
    set(source_file "${CASE_DIR}/sema_var_infer_void.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: cannot infer type for 'value' from void expression")
elseif(CASE STREQUAL "sema-break-outside-loop")
    set(source_file "${CASE_DIR}/sema_break_outside_loop.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: 'break' is only valid inside a loop")
elseif(CASE STREQUAL "sema-continue-outside-loop")
    set(source_file "${CASE_DIR}/sema_continue_outside_loop.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: 'continue' is only valid inside a loop")
elseif(CASE STREQUAL "sema-nonvoid-return-without-value")
    set(source_file "${CASE_DIR}/sema_nonvoid_return_without_value.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: non-void function must return a value of type 'int32'")
elseif(CASE STREQUAL "sema-void-return-with-value")
    set(source_file "${CASE_DIR}/sema_void_return_with_value.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: void function cannot return a value")
elseif(CASE STREQUAL "sema-missing-return-path")
    set(source_file "${CASE_DIR}/sema_missing_return_path.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: function 'maybe' with return type 'int32' is missing a return statement on some paths")
elseif(CASE STREQUAL "sema-extern-missing-import")
    set(source_file "${CASE_DIR}/sema_extern_missing_import.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: extern function 'GetCurrentProcessId' references 'kernel32.dll', but no matching import declaration was found")
elseif(CASE STREQUAL "sema-duplicate-import")
    set(source_file "${CASE_DIR}/sema_duplicate_import.vlt")
    set(expected_exit 1)
    set(expected_stderr "error: duplicate import 'kernel32.dll'")
elseif(CASE STREQUAL "source-import-missing-file")
    set(source_file "${CASE_DIR}/source_import_missing_file.vlt")
    set(expected_exit 1)
    set(expected_stderr "voltisc error: Source module not found:")
elseif(CASE STREQUAL "source-import-cycle")
    set(source_file "${CASE_DIR}/source_import_cycle_a.vlt")
    set(expected_exit 1)
    set(expected_stderr "voltisc error: Source module import cycle detected:")
else()
    message(FATAL_ERROR "Unknown CASE '${CASE}'")
endif()

if(NOT run_without_source)
    if(NOT EXISTS "${source_file}")
        message(FATAL_ERROR "Source file not found: ${source_file}")
    endif()
endif()
if(NOT EXISTS "${VOLTISC}")
    message(FATAL_ERROR "Compiler executable not found: ${VOLTISC}")
endif()

if(NOT "${expected_file}" STREQUAL "")
    file(REMOVE "${expected_file}")
endif()

if(run_without_source)
    if(NOT "${benchmark_iterations_override}" STREQUAL "")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env "VOLTIS_BENCHMARK_ITERATIONS=${benchmark_iterations_override}" "${VOLTISC}" ${args}
            RESULT_VARIABLE actual_exit
            OUTPUT_VARIABLE actual_stdout
            ERROR_VARIABLE actual_stderr
        )
    else()
        execute_process(
            COMMAND "${VOLTISC}" ${args}
            RESULT_VARIABLE actual_exit
            OUTPUT_VARIABLE actual_stdout
            ERROR_VARIABLE actual_stderr
        )
    endif()
else()
    if(NOT "${benchmark_iterations_override}" STREQUAL "")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env "VOLTIS_BENCHMARK_ITERATIONS=${benchmark_iterations_override}" "${VOLTISC}" "${source_file}" ${args}
            RESULT_VARIABLE actual_exit
            OUTPUT_VARIABLE actual_stdout
            ERROR_VARIABLE actual_stderr
        )
    else()
        execute_process(
            COMMAND "${VOLTISC}" "${source_file}" ${args}
            RESULT_VARIABLE actual_exit
            OUTPUT_VARIABLE actual_stdout
            ERROR_VARIABLE actual_stderr
        )
    endif()
endif()

string(REPLACE "\r\n" "\n" actual_stdout "${actual_stdout}")
string(REPLACE "\r\n" "\n" actual_stderr "${actual_stderr}")

if(NOT "${actual_exit}" STREQUAL "${expected_exit}")
    message(FATAL_ERROR
        "CASE ${CASE}: expected exit ${expected_exit}, got ${actual_exit}\nstdout:\n${actual_stdout}\nstderr:\n${actual_stderr}")
endif()

foreach(expected IN LISTS expected_stdout)
    assert_contains("${actual_stdout}" "${expected}" "CASE ${CASE} stdout")
endforeach()

foreach(expected IN LISTS expected_stderr)
    assert_contains("${actual_stderr}" "${expected}" "CASE ${CASE} stderr")
endforeach()

if(NOT "${expected_file}" STREQUAL "")
    if(NOT EXISTS "${expected_file}")
        message(FATAL_ERROR "CASE ${CASE}: expected output file was not emitted: ${expected_file}")
    endif()
    file(READ "${expected_file}" emitted_file_content)
    string(REPLACE "\r\n" "\n" emitted_file_content "${emitted_file_content}")
    foreach(expected IN LISTS expected_file_contains)
        assert_contains("${emitted_file_content}" "${expected}" "CASE ${CASE} emitted file")
    endforeach()
    foreach(expected IN LISTS expected_file_not_contains)
        assert_not_contains("${emitted_file_content}" "${expected}" "CASE ${CASE} emitted file")
    endforeach()
    if(expected_file_hex_contains OR expected_file_hex_not_contains)
        file(READ "${expected_file}" emitted_file_hex HEX)
        string(TOLOWER "${emitted_file_hex}" emitted_file_hex)
        foreach(expected IN LISTS expected_file_hex_contains)
            string(TOLOWER "${expected}" expected_hex_lower)
            assert_contains("${emitted_file_hex}" "${expected_hex_lower}" "CASE ${CASE} emitted file hex")
        endforeach()
        foreach(expected IN LISTS expected_file_hex_not_contains)
            string(TOLOWER "${expected}" expected_hex_lower)
            assert_not_contains("${emitted_file_hex}" "${expected_hex_lower}" "CASE ${CASE} emitted file hex")
        endforeach()
    endif()

    if(run_emitted)
        execute_process(
            COMMAND "${expected_file}"
            RESULT_VARIABLE run_exit
            OUTPUT_VARIABLE run_stdout
            ERROR_VARIABLE run_stderr
        )
        string(REPLACE "\r\n" "\n" run_stdout "${run_stdout}")
        string(REPLACE "\r\n" "\n" run_stderr "${run_stderr}")
        if(NOT "${run_exit}" STREQUAL "0")
            message(FATAL_ERROR
                "CASE ${CASE}: expected emitted executable exit 0, got ${run_exit}\nstdout:\n${run_stdout}\nstderr:\n${run_stderr}")
        endif()
        foreach(expected IN LISTS expected_run_stdout)
            assert_contains("${run_stdout}" "${expected}" "CASE ${CASE} emitted runtime stdout")
        endforeach()
    endif()
endif()

if(expect_benchmark_history)
    if(WIN32)
        set(temp_root "$ENV{TEMP}")
        if("${temp_root}" STREQUAL "")
            set(temp_root "$ENV{TMP}")
        endif()
    else()
        set(temp_root "$ENV{TMPDIR}")
        if("${temp_root}" STREQUAL "")
            set(temp_root "/tmp")
        endif()
    endif()

    if("${temp_root}" STREQUAL "")
        message(FATAL_ERROR "CASE ${CASE}: unable to resolve temp directory for benchmark history CSV check")
    endif()

    file(TO_CMAKE_PATH "${temp_root}" temp_root_norm)
    set(benchmark_csv "${temp_root_norm}/voltis_benchmark_history.csv")
    if(NOT EXISTS "${benchmark_csv}")
        message(FATAL_ERROR "CASE ${CASE}: benchmark history CSV missing: ${benchmark_csv}")
    endif()
endif()
