#include <cstdint>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace vt {
    inline void Print(const std::string& value) { std::cout << value << std::endl; }
    inline void Print(const char* value) { std::cout << value << std::endl; }
    inline void Print(std::int32_t value) { std::cout << value << std::endl; }
    inline void Print(float value) { std::cout << value << std::endl; }
    inline void Print(double value) { std::cout << value << std::endl; }
    inline void Print(bool value) { std::cout << (value ? "true" : "false") << std::endl; }
    inline std::string ToString(const std::string& value) { return value; }
    inline std::string ToString(const char* value) { return std::string(value); }
    inline std::string ToString(std::int32_t value) { return std::to_string(value); }
    inline std::string ToString(float value) { return std::to_string(value); }
    inline std::string ToString(double value) { return std::to_string(value); }
    inline std::string ToString(bool value) { return value ? "true" : "false"; }
    inline std::int32_t ToInt32(const std::string& value) { return std::stoi(value); }
    inline std::int32_t ToInt32(float value) { return static_cast<std::int32_t>(value); }
    inline std::int32_t ToInt32(double value) { return static_cast<std::int32_t>(value); }
    inline std::int32_t ToInt32(bool value) { return value ? 1 : 0; }
    inline float ToFloat32(const std::string& value) { return std::stof(value); }
    inline float ToFloat32(std::int32_t value) { return static_cast<float>(value); }
    inline float ToFloat32(double value) { return static_cast<float>(value); }
    inline double ToFloat64(const std::string& value) { return std::stod(value); }
    inline double ToFloat64(std::int32_t value) { return static_cast<double>(value); }
    inline double ToFloat64(float value) { return static_cast<double>(value); }
    inline bool ToBool(const std::string& value) { if (value == "true") return true; if (value == "false") return false; throw std::runtime_error("Invalid bool conversion"); }
    inline bool ToBool(std::int32_t value) { return value != 0; }
    inline bool ToBool(float value) { return value != 0.0f; }
    inline bool ToBool(double value) { return value != 0.0; }
    inline double Round(double value) { return std::round(value); }
    inline double Floor(double value) { return std::floor(value); }
    inline double Ceil(double value) { return std::ceil(value); }
    inline float Round(float value) { return std::round(value); }
    inline float Floor(float value) { return std::floor(value); }
    inline float Ceil(float value) { return std::ceil(value); }
}

std::int32_t main()
{
    vt::Print(std::string("Hello World"));
    system(std::string("pause"));
    return 0;
}

