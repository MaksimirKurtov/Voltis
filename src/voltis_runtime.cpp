#include <cctype>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

char* vtDupCString(const char* input) {
    if (!input) {
        input = "";
    }
    const std::size_t len = std::strlen(input);
    char* out = static_cast<char*>(std::malloc(len + 1));
    if (!out) {
        std::fputs("voltis runtime: out of memory\n", stderr);
        std::abort();
    }
    std::memcpy(out, input, len + 1);
    return out;
}

bool equalsIgnoreCase(const char* value, const char* expected) {
    if (!value || !expected) {
        return false;
    }
    while (*value != '\0' && *expected != '\0') {
        const unsigned char lhs = static_cast<unsigned char>(*value);
        const unsigned char rhs = static_cast<unsigned char>(*expected);
        if (std::tolower(lhs) != std::tolower(rhs)) {
            return false;
        }
        ++value;
        ++expected;
    }
    return *value == '\0' && *expected == '\0';
}

[[noreturn]] void failConversion(const char* functionName, const char* detail) {
    std::fprintf(stderr, "voltis runtime: %s failed: %s\n", functionName, detail);
    std::abort();
}

} // namespace

extern "C" {

void vt_print_i32(std::int32_t value) {
    std::printf("%d\n", value);
}

void vt_print_f32(float value) {
    std::printf("%.9g\n", value);
}

void vt_print_f64(double value) {
    std::printf("%.17g\n", value);
}

void vt_print_bool(bool value) {
    std::puts(value ? "true" : "false");
}

void vt_print_str(const char* value) {
    std::puts(value ? value : "");
}

char* vt_to_string_i32(std::int32_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d", value);
    return vtDupCString(buffer);
}

char* vt_to_string_f32(float value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.9g", value);
    return vtDupCString(buffer);
}

char* vt_to_string_f64(double value) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    return vtDupCString(buffer);
}

char* vt_to_string_bool(bool value) {
    return vtDupCString(value ? "true" : "false");
}

std::int32_t vt_str_to_i32(const char* value) {
    if (!value) {
        failConversion("vt_str_to_i32", "null string");
    }
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (errno == ERANGE) {
        failConversion("vt_str_to_i32", "out of range");
    }
    if (!end || *end != '\0') {
        failConversion("vt_str_to_i32", "invalid numeric string");
    }
    if (parsed < std::numeric_limits<std::int32_t>::min() || parsed > std::numeric_limits<std::int32_t>::max()) {
        failConversion("vt_str_to_i32", "out of int32 range");
    }
    return static_cast<std::int32_t>(parsed);
}

float vt_str_to_f32(const char* value) {
    if (!value) {
        failConversion("vt_str_to_f32", "null string");
    }
    errno = 0;
    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    if (errno == ERANGE) {
        failConversion("vt_str_to_f32", "out of range");
    }
    if (!end || *end != '\0') {
        failConversion("vt_str_to_f32", "invalid numeric string");
    }
    return parsed;
}

double vt_str_to_f64(const char* value) {
    if (!value) {
        failConversion("vt_str_to_f64", "null string");
    }
    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(value, &end);
    if (errno == ERANGE) {
        failConversion("vt_str_to_f64", "out of range");
    }
    if (!end || *end != '\0') {
        failConversion("vt_str_to_f64", "invalid numeric string");
    }
    return parsed;
}

bool vt_str_to_bool(const char* value) {
    if (!value) {
        failConversion("vt_str_to_bool", "null string");
    }
    if (equalsIgnoreCase(value, "true") || std::strcmp(value, "1") == 0) {
        return true;
    }
    if (equalsIgnoreCase(value, "false") || std::strcmp(value, "0") == 0) {
        return false;
    }
    failConversion("vt_str_to_bool", "expected true/false");
}

char* vt_str_concat(const char* lhs, const char* rhs) {
    if (!lhs) {
        lhs = "";
    }
    if (!rhs) {
        rhs = "";
    }
    const std::size_t lhsLen = std::strlen(lhs);
    const std::size_t rhsLen = std::strlen(rhs);
    char* out = static_cast<char*>(std::malloc(lhsLen + rhsLen + 1));
    if (!out) {
        std::fputs("voltis runtime: out of memory\n", stderr);
        std::abort();
    }
    std::memcpy(out, lhs, lhsLen);
    std::memcpy(out + lhsLen, rhs, rhsLen);
    out[lhsLen + rhsLen] = '\0';
    return out;
}

bool vt_str_eq(const char* lhs, const char* rhs) {
    lhs = lhs ? lhs : "";
    rhs = rhs ? rhs : "";
    return std::strcmp(lhs, rhs) == 0;
}

}
