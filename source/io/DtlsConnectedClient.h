#pragma once

#include "CommonMacros.h"
#include "DataChunk.h"
#include "DtlsVersion.h"
#include "Endpoint.h"
#include "EventLoop.h"
#include "Error.h"
#include "Export.h"
#include "Forward.h"
#include "Removable.h"

#include <memory>

namespace io {

class DtlsConnectedClient : protected Removable,
                            public UserDataHolder {
public:
    friend class DtlsServer;

    using UnderlyingClientType = UdpPeer;

    using NewConnectionCallback = std::function<void(DtlsConnectedClient&, const Error&)>;
    using DataReceiveCallback = std::function<void(DtlsConnectedClient&, const DataChunk&, const Error&)>;
    using CloseCallback = std::function<void(DtlsConnectedClient&, const Error&)>;
    using EndSendCallback = std::function<void(DtlsConnectedClient&, const Error&)>;

    IO_FORBID_COPY(DtlsConnectedClient);
    IO_FORBID_MOVE(DtlsConnectedClient);

    IO_DLL_PUBLIC void close();
    IO_DLL_PUBLIC bool is_open() const;

    IO_DLL_PUBLIC const Endpoint& endpoint() const;

    IO_DLL_PUBLIC void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback = nullptr);
    IO_DLL_PUBLIC void send_data(const std::string& message, EndSendCallback callback = nullptr);

    IO_DLL_PUBLIC DtlsVersion negotiated_dtls_version() const;

    IO_DLL_PUBLIC DtlsServer& server();
    IO_DLL_PUBLIC const DtlsServer& server() const;

protected:
    ~DtlsConnectedClient();

private:
    DtlsConnectedClient(EventLoop& loop,
                        DtlsServer& dtls_server,
                        NewConnectionCallback new_connection_callback,
                        CloseCallback close_callback,
                        UdpPeer& udp_peer,
                        void* context);
    void set_data_receive_callback(DataReceiveCallback callback);
    void on_data_receive(const char* buf, std::size_t size);
    Error init_ssl();

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace io


