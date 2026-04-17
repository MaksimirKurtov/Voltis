#pragma once

#include "source_location.h"
#include <ostream>
#include <string>
#include <vector>

struct Diagnostic {
    std::string message;
    SourceLocation location;
};

class DiagnosticBag {
public:
    void error(const SourceLocation& location, std::string message);
    bool hasErrors() const;
    const std::vector<Diagnostic>& all() const;
    void print(std::ostream& out) const;

private:
    std::vector<Diagnostic> diagnostics_;
};
