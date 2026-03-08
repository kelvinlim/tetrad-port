#pragma once

#include <ostream>

namespace tetrad {

// Configurable output stream for verbose logging.
// Default: std::cout (set lazily on first access).
// R bindings override to Rcpp::Rcout at package load time.
std::ostream& logStream();
void setLogStream(std::ostream& os);

} // namespace tetrad
