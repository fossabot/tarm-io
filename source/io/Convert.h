#pragma once

#include "Error.h"
#include "Export.h"

#include <array>
#include <cstdint>
#include <string>

namespace io {

// IPv4
IO_DLL_PUBLIC
std::string ip4_addr_to_string(std::uint32_t addr);

IO_DLL_PUBLIC
Error string_to_ip4_addr(const std::string& string_address, std::uint32_t& uint_address);

// IPv6
IO_DLL_PUBLIC
std::string ip6_addr_to_string(const std::uint8_t* address_bytes);

IO_DLL_PUBLIC
std::string ip6_addr_to_string(std::initializer_list<std::uint8_t> address_bytes);

IO_DLL_PUBLIC
Error string_to_ip6_addr(const std::string& string_address, std::array<std::uint8_t, 16>& bytes);

} // namespace io
