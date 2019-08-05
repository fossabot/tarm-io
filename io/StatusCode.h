#pragma once

#include <cstdint>

// TODO: move with underlying_type
#include <type_traits>
#include <ostream>

namespace io {

enum class StatusCode : uint32_t {
    OK = 0,
    UNDEFINED,
    FILE_NOT_OPEN,

    ARGUMENT_LIST_TOO_LONG,
    PERMISSION_DENIED,
    ADDRESS_ALREADY_IN_USE,
    ADDRESS_NOT_AVAILABLE,
    ADDRESS_FAMILY_NOT_SUPPORTED,
    RESOURCE_TEMPORARILY_UNAVAILABLE,
    TEMPORARY_FAILURE,
    BAD_AI_FLAGS_VALUE,
    INVALID_VALUE_FOR_HINTS,
    REQUEST_CANCELED,
    PERMANENT_FAILURE,
    AI_FAMILY_NOT_SUPPORTED,
    OUT_OF_MEMORY,
    NO_ADDRESS_AVAILABLE,
    UNKNOWN_NODE_OR_SERVICE,
    ARGUMENT_BUFFER_OVERFLOW,
    RESOLVED_PROTOCOL_IS_UNKNOWN,
    SERVICE_NOT_AVAILABLE_FOR_SOCKET_TYPE,
    SOCKET_TYPE_NOT_SUPPORTED,
    CONNECTION_ALREADY_IN_PROGRESS,
    BAD_FILE_DESCRIPTOR,
    RESOURCE_BUSY_OR_LOCKED,
    OPERATION_CANCELED,
    INVALID_UNICODE_CHARACTER,
    SOFTWARE_CAUSED_CONNECTION_ABORT,
    CONNECTION_REFUSED,
    CONNECTION_RESET_BY_PEER,
    DESTINATION_ADDRESS_REQUIRED,
    FILE_OR_DIR_ALREADY_EXISTS,
    BAD_ADDRESS_IN_SYSTEM_CALL_ARGUMENT,
    FILE_TOO_LARGE,
    HOST_IS_UNREACHABLE,
    INTERRUPTED_SYSTEM_CALL,
    INVALID_ARGUMENT,
    IO_ERROR,
    SOCKET_IS_ALREADY_CONNECTED,
    ILLEGAL_OPERATION_ON_A_DIRECTORY,
    TOO_MANY_SYMBOLIC_LINKS_ENCOUNTERED,
    TOO_MANY_OPEN_FILES,
    MESSAGE_TOO_LONG,
    NAME_TOO_LONG,
    NETWORK_IS_DOWN,
    NETWORK_IS_UNREACHABLE,
    FILE_TABLE_OVERFLOW,
    NO_BUFFER_SPACE_AVAILABLE,
    NO_SUCH_DEVICE,
    NO_SUCH_FILE_OR_DIRECTORY,
    NOT_ENOUGH_MEMORY,
    MACHINE_IS_NOT_ON_THE_NETWORK,
    PROTOCOL_NOT_AVAILABLE,
    NO_SPACE_LEFT_ON_DEVICE,
    FUNCTION_NOT_IMPLEMENTED,
    SOCKET_IS_NOT_CONNECTED,
    NOT_A_DIRECTORY,
    DIRECTORY_NOT_EMPTY,
    SOCKET_OPERATION_ON_NON_SOCKET,
    OPERATION_NOT_SUPPORTED_ON_SOCKET,
    OPERATION_NOT_PERMITTED,
    BROKEN_PIPE,
    PROTOCOL_ERROR,
    PROTOCOL_NOT_SUPPORTED,
    PROTOCOL_WRONG_TYPE_FOR_SOCKET,
    RESULT_TOO_LARGE,
    READ_ONLY_FILE_SYSTEM,
    CANNOT_SEND_AFTER_TRANSPORT_ENDPOINT_SHUTDOWN,
    INVALID_SEEK,
    NO_SUCH_PROCESS,
    CONNECTION_TIMED_OUT,
    TEXT_FILE_IS_BUSY,
    CROSS_DEVICE_LINK_NOT_PERMITTED,
    UNKNOWN_ERROR,
    END_OF_FILE,
    NO_SUCH_DEVICE_OR_ADDRESS,
    TOO_MANY_LINKS,
};

StatusCode convert_from_uv(int libuv_code);

// TODO: move
template<typename T>
typename std::underlying_type<T>::type as_integral(T t) {
    return static_cast<typename std::underlying_type<T>::type>(t);
}

// TODO: print as strings???????
/* because
    Expected equality of these values:
  io::StatusCode::ILLEGAL_OPERATION_ON_A_DIRECTORY
    Which is: 39
  status.code()
    Which is: 31
*/

inline
std::ostream& operator<<(std::ostream& out, io::StatusCode code) {
    return out << as_integral(code);
}

} // namespace io
