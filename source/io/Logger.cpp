#include "Logger.h"

namespace io {



Logger::Logger() {

}

Logger::Logger(const std::string& prefix) :
    m_prefix(prefix) {
}

void Logger::enable_log(Callback callback) {
    m_callback = callback;
}

void Logger::out_common_prefix(std::ostream& ss, Logger::Severity severity) {
    ss << "[" << severity << "]";

    if (!m_prefix.empty()) {
        ss << " <" << m_prefix << ">";
    }
}

} // namespace io
