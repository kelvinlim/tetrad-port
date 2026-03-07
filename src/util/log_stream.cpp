#include "util/log_stream.h"

namespace tetrad {

static std::ostream* g_log_stream = &std::cout;

std::ostream& logStream() {
    return *g_log_stream;
}

void setLogStream(std::ostream& os) {
    g_log_stream = &os;
}

} // namespace tetrad
