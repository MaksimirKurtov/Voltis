#include "diagnostics.h"
#include <utility>

void DiagnosticBag::error(const SourceLocation& location, std::string message) {
    diagnostics_.push_back(Diagnostic{std::move(message), location});
}

bool DiagnosticBag::hasErrors() const {
    return !diagnostics_.empty();
}

const std::vector<Diagnostic>& DiagnosticBag::all() const {
    return diagnostics_;
}

void DiagnosticBag::print(std::ostream& out) const {
    for (const auto& diagnostic : diagnostics_) {
        out << "error: " << diagnostic.message;
        if (diagnostic.location.line > 0 && diagnostic.location.column > 0) {
            out << " (" << diagnostic.location.line << ":" << diagnostic.location.column << ")";
        }
        out << "\n";
    }
}
