#pragma once

#include "io/Export.h"
#include "io/Logger.h"

namespace io {
namespace global {

IO_DLL_PUBLIC
Logger::Callback logger_callback();

IO_DLL_PUBLIC
void set_logger_callback(Logger::Callback callback);

IO_DLL_PUBLIC
void set_ciphers_list(const std::string& ciphers);

IO_DLL_PUBLIC
const std::string& ciphers_list();

IO_DLL_PUBLIC
std::size_t min_receive_buffer_size();
IO_DLL_PUBLIC
std::size_t default_receive_buffer_size();
IO_DLL_PUBLIC
std::size_t max_receive_buffer_size();

IO_DLL_PUBLIC
std::size_t min_send_buffer_size();
IO_DLL_PUBLIC
std::size_t default_send_buffer_size();
IO_DLL_PUBLIC
std::size_t max_send_buffer_size();


} // namespace global
} // namespace io
