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

file(MAKE_DIRECTORY "${OUT_DIR}")

set(source_file "")
set(args)
set(expected_exit 0)
set(expected_stdout)
set(expected_stderr)
set(expected_file "")
set(expected_file_contains)
set(run_emitted FALSE)
set(expected_run_stdout)

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
elseif(CASE STREQUAL "parser-missing-semicolon")
    set(source_file "${CASE_DIR}/parser_missing_semicolon.vlt")
    set(expected_exit 1)
    set(expected_stderr "voltisc error: Expected ';'")
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
else()
    message(FATAL_ERROR "Unknown CASE '${CASE}'")
endif()

if(NOT EXISTS "${source_file}")
    message(FATAL_ERROR "Source file not found: ${source_file}")
endif()
if(NOT EXISTS "${VOLTISC}")
    message(FATAL_ERROR "Compiler executable not found: ${VOLTISC}")
endif()

if(NOT "${expected_file}" STREQUAL "")
    file(REMOVE "${expected_file}")
endif()

execute_process(
    COMMAND "${VOLTISC}" "${source_file}" ${args}
    RESULT_VARIABLE actual_exit
    OUTPUT_VARIABLE actual_stdout
    ERROR_VARIABLE actual_stderr
)

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
