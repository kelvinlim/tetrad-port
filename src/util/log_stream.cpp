#include "util/log_stream.h"

#ifndef RTETRAD_NO_COUT
#include <iostream>
#endif

namespace tetrad {

static std::ostream* g_log_stream = nullptr;

std::ostream& logStream() {
#ifndef RTETRAD_NO_COUT
    if (!g_log_stream) g_log_stream = &std::cout;
#endif
    return *g_log_stream;
}

void setLogStream(std::ostream& os) {
    g_log_stream = &os;
}

} // namespace tetrad
