#pragma once

#include <iostream>

namespace tetrad {

// Configurable output stream for verbose logging.
// Default: std::cout. R bindings set to Rcpp::Rcout at package load time.
std::ostream& logStream();
void setLogStream(std::ostream& os);

} // namespace tetrad
