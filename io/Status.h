#pragma once

#include "StatusCode.h"

#include <string>

namespace io {

class Status {
public:
    Status(int libuv_code);

    StatusCode status_code() const;
    std::string as_string() const;

    bool ok() const;
    bool fail() const;

private:
    int m_libuv_code;
    StatusCode m_status_code;
};

} // namespace io
