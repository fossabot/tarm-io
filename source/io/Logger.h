#pragma once

#include "CommonMacros.h"
#include "Export.h"

#include <functional>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <type_traits>

#ifdef _MSC_VER
//#define __func__ __FUNCTION__
#endif

#define IO_LOG(LOGGER_PTR, SEVERITY, ...) (LOGGER_PTR)->log_with_compile_context(io::Logger::Severity::SEVERITY, ::io::detail::extract_file_name_from_path(__FILE__), __LINE__, __func__, __VA_ARGS__)

namespace io {

class Logger {
public:
    enum class Severity {
        TRACE,
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    using Callback = std::function<void(const std::string& message)>;

    IO_FORBID_COPY(Logger);
    IO_FORBID_MOVE(Logger);

    IO_DLL_PUBLIC Logger();
    IO_DLL_PUBLIC Logger(const std::string& prefix);

    IO_DLL_PUBLIC void enable_log(Callback callback);

    template<typename... T>
    void log(Severity severity, T... t);

    template<typename... T>
    void log_with_compile_context(Severity severity, const char* const file, std::size_t line, const char* const func, T... t);

private:
    template<typename M, typename... T>
    typename std::enable_if<!std::is_pointer<M>::value>::type
    log_impl(std::ostream& os, const M& message_chunk, T... t) {
        log_impl(os, message_chunk);
        log_impl(os, t...);
    }

    template<typename M>
    typename std::enable_if<!std::is_pointer<M>::value>::type
    log_impl(std::ostream& os, const M& message_chunk) {
        os << " " << message_chunk;
    }

    template<typename M, typename... T>
    void log_impl(std::ostream& os, const M* message_chunk, T... t) {
        log_impl(os, message_chunk);
        log_impl(os, t...);
    }

    template<typename M>
    void log_impl(std::ostream& os, const M* const message_chunk) {
        os << " ";
#ifdef _MSC_VER // Visual Studio does not add 0x prefix for pointers
        os << "0x";
#endif // _MSC_VER
        os << message_chunk;
    }

    template<typename... T>
    void log_impl(std::ostream& os, const char* message_chunk, T... t) {
        log_impl(os, message_chunk);
        log_impl(os, t...);
    }

    void log_impl(std::ostream& os, const char* message_chunk) {
        os << " " << message_chunk;
    }

    IO_DLL_PUBLIC void out_common_prefix(std::ostream& ss, Logger::Severity severity);

    Callback m_callback = nullptr;
    std::string m_prefix;
};

inline
std::ostream& operator<< (std::ostream& os, Logger::Severity severity) {
    switch (severity) {
        case Logger::Severity::TRACE:
            return os << "TRACE";
        case Logger::Severity::DEBUG:
            return os << "DEBUG";
        case Logger::Severity::INFO:
            return os << "INFO";
        case Logger::Severity::WARNING:
            return os << "WARNING";
        case Logger::Severity::ERROR:
            return os << "ERROR";
    }

    return os << "?????";
}

///////////////////// IMPLEMENTATION /////////////////////
template<typename... T>
void Logger::log(Severity severity, T... t) {
    if (m_callback == nullptr) {
        return;
    }

    std::stringstream ss;
    out_common_prefix(ss, severity);
    log_impl(ss, t...);

    m_callback(ss.str());
}

template<typename... T>
void Logger::log_with_compile_context(Severity severity, const char* const file, std::size_t line, const char* const func, T... t) {
    if (m_callback == nullptr) {
        return;
    }

    std::stringstream ss;
    out_common_prefix(ss, severity);
    ss << " [" << file << ":" << line << "]";
    if (func) {
        ss << " (" << func << ")";
    }

    log_impl(ss, t...);

    m_callback(ss.str());
}

} // namespace io
