#pragma once

#include <cstdint>
#include <tuple>

namespace io {

// List all known versions of TLS. Some of them may be not available for some setups.
enum class TlsVersion : std::uint8_t {
    UNKNOWN = 0,
    V1_0,
    V1_1,
    V1_2,
    V1_3,

    MIN = V1_0,
    MAX = V1_3
};

static_assert(static_cast<int>(TlsVersion::V1_0) == 1, "Enum values are used in code logic");

// Min and max version
using TlsVersionRange = std::tuple<TlsVersion, TlsVersion>;

const TlsVersionRange DEFAULT_TLS_VERSION_RANGE{TlsVersion::MIN, TlsVersion::MAX};

} // namespace io
