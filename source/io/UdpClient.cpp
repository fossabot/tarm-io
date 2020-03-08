#include "UdpClient.h"

#include "BacklogWithTimeout.h"
#include "ByteSwap.h"
#include "detail/UdpClientImplBase.h"

#include "detail/Common.h"

#include <cstring>
#include <cstddef>
#include <assert.h>

namespace io {

class UdpClient::Impl : public detail::UdpClientImplBase<UdpClient, UdpClient::Impl> {
public:
    Impl(EventLoop& loop, UdpClient& parent);
    Impl(EventLoop& loop, const Endpoint& endpoint, UdpClient& parent);
    ~Impl();

    Error start_receive(DataReceivedCallback receive_callback);
    Error start_receive(DataReceivedCallback receive_callback, std::size_t timeout_ms, TimeoutCallback timeout_callback);

    using CloseHandler = void (*)(uv_handle_t* handle);
    bool close_on_timeout();
    bool close_with_removal();
    bool close(CloseHandler handler);

    void set_destination(const Endpoint& endpoint);

    std::uint32_t ipv4_addr() const;
    std::uint16_t port() const;

protected:
    Error start_receive_impl();

    // statics
    static void on_data_received(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* uv_buf, const struct sockaddr* addr, unsigned flags);

    static void on_close_on_timeout(uv_handle_t* handle);

private:
    DataReceivedCallback m_receive_callback = nullptr;

    TimeoutCallback m_timeout_callback = nullptr;

    // Here is a bit unusual usage of backlog which consists of one single element to track expiration
    std::unique_ptr<BacklogWithTimeout<UdpClient::Impl*>> m_timeout_handler;
    std::function<void(BacklogWithTimeout<UdpClient::Impl*>&, UdpClient::Impl* const& )> m_on_item_expired = nullptr;
};

UdpClient::Impl::Impl(EventLoop& loop, UdpClient& parent) :
    UdpClientImplBase(loop, parent) {

    m_on_item_expired = [this](BacklogWithTimeout<UdpClient::Impl*>&, UdpClient::Impl* const & item) {
        this->close_on_timeout();
    };
}

UdpClient::Impl::Impl(EventLoop& loop, const Endpoint& endpoint, UdpClient& parent) :
    Impl(loop, parent) {
    set_destination(endpoint);
}

Error UdpClient::Impl::start_receive(DataReceivedCallback receive_callback) {
    m_receive_callback = receive_callback;
    return start_receive_impl();
}

Error UdpClient::Impl::start_receive(DataReceivedCallback receive_callback,
                                     std::size_t timeout_ms,
                                     TimeoutCallback timeout_callback) {
    m_receive_callback = receive_callback;
    m_timeout_callback = timeout_callback;
    m_timeout_handler.reset(new BacklogWithTimeout<UdpClient::Impl*>(*m_loop, timeout_ms, m_on_item_expired, std::bind(&UdpClient::Impl::last_packet_time, this), &::uv_hrtime));
    m_timeout_handler->add_item(this);
    return start_receive_impl();
}

UdpClient::Impl::~Impl() {
    IO_LOG(m_loop, TRACE, m_parent, "Deleted UdpClient");
}

void UdpClient::Impl::set_destination(const Endpoint& endpoint) {
    // TODO: invalid endpoint handling

    if (m_udp_handle.get()->io_watcher.fd == -1) {
        ::sockaddr_storage storage{0};
        storage.ss_family = endpoint.type() == Endpoint::IP_V4 ? AF_INET : AF_INET6;
        Error bind_error = uv_udp_bind(m_udp_handle.get(), reinterpret_cast<const ::sockaddr*>(&storage), UV_UDP_REUSEADDR);
        // TODO: error handling
    }

    m_destination_endpoint = endpoint;
}

bool UdpClient::Impl::close_with_removal() {
    return close(&on_close_with_removal);
}

bool UdpClient::Impl::close_on_timeout() {
    return close(&on_close_on_timeout);
}

bool UdpClient::Impl::close(CloseHandler handler) {
    if (is_open()) {
        if (m_receive_callback) {
            uv_udp_recv_stop(m_udp_handle.get());
        }

        uv_close(reinterpret_cast<uv_handle_t*>(m_udp_handle.get()), handler);
        return false; // not ready to remove
    }

    return true;
}

Error UdpClient::Impl::start_receive_impl() {
    Error recv_start_error = uv_udp_recv_start(m_udp_handle.get(), detail::default_alloc_buffer, on_data_received);
    if (recv_start_error) {
        return recv_start_error;
    }

    return Error(0);
}

// TODO: ipv6
std::uint32_t UdpClient::Impl::ipv4_addr() const {
    const auto& unix_addr = *reinterpret_cast<const sockaddr_in*>(m_destination_endpoint.raw_endpoint());
    return network_to_host(unix_addr.sin_addr.s_addr);
}

std::uint16_t UdpClient::Impl::port() const {
    const auto& unix_addr = *reinterpret_cast<const sockaddr_in*>(m_destination_endpoint.raw_endpoint());
    return network_to_host(unix_addr.sin_port);
}

///////////////////////////////////////////  static  ////////////////////////////////////////////

void UdpClient::Impl::on_data_received(uv_udp_t* handle,
                                       ssize_t nread,
                                       const uv_buf_t* uv_buf,
                                       const struct sockaddr* addr,
                                       unsigned flags) {
    assert(handle);
    auto& this_ = *reinterpret_cast<UdpClient::Impl*>(handle->data);
    auto& parent = *this_.m_parent;

    this_.set_last_packet_time(::uv_hrtime());

    // TODO: need some mechanism to reuse memory
    std::shared_ptr<const char> buf(uv_buf->base, std::default_delete<char[]>());

    if (!this_.m_receive_callback) {
        return;
    }

    Error error(nread);

    if (!error) {
        if (addr && nread) {
            const auto& address_in_from = *reinterpret_cast<const struct sockaddr_in*>(addr);
            const auto& address_in_expect = *reinterpret_cast<sockaddr_in*>(this_.m_destination_endpoint.raw_endpoint());

            if (address_in_from.sin_addr.s_addr == address_in_expect.sin_addr.s_addr &&
                address_in_from.sin_port == address_in_expect.sin_port) {

                DataChunk data_chunk(buf, std::size_t(nread));
                this_.m_receive_callback(parent, data_chunk, error);
            }
        }
    } else {
        DataChunk data(nullptr, 0);
        this_.m_receive_callback(parent, data, error);
    }
}

void UdpClient::Impl::on_close_on_timeout(uv_handle_t* handle) {
    auto& this_ = *reinterpret_cast<UdpClient::Impl*>(handle->data);
    auto& parent = *this_.m_parent;

    handle->data = nullptr;

    if (this_.m_timeout_callback) {
        this_.m_timeout_callback(parent, Error(0));
    }
}

/////////////////////////////////////////// interface ///////////////////////////////////////////

UdpClient::UdpClient(EventLoop& loop) :
    Removable(loop),
    m_impl(new UdpClient::Impl(loop, *this)) {
}

UdpClient::UdpClient(EventLoop& loop, const Endpoint& endpoint) :
    Removable(loop),
    m_impl(new UdpClient::Impl(loop, endpoint, *this)) {
}

UdpClient::~UdpClient() {
}

Error UdpClient::start_receive(DataReceivedCallback receive_callback) {
    return m_impl->start_receive(receive_callback);
}

Error UdpClient::start_receive(DataReceivedCallback receive_callback,
                               std::size_t timeout_ms,
                               TimeoutCallback timeout_callback) {
    return m_impl->start_receive(receive_callback, timeout_ms, timeout_callback);
}

void UdpClient::set_destination(const Endpoint& endpoint) {
    return m_impl->set_destination(endpoint);
}

void UdpClient::send_data(const std::string& message, EndSendCallback callback) {
    return m_impl->send_data(message, callback);
}

void UdpClient::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, EndSendCallback callback) {
    return m_impl->send_data(buffer, size, callback);
}

void UdpClient::schedule_removal() {
    const bool ready_to_remove = m_impl->close_with_removal();
    if (ready_to_remove) {
        Removable::schedule_removal();
    }
}

std::uint16_t UdpClient::bound_port() const {
    return m_impl->bound_port();
}

std::uint32_t UdpClient::ipv4_addr() const {
    return m_impl->ipv4_addr();
}

std::uint16_t UdpClient::port() const {
    return m_impl->port();
}

bool UdpClient::is_open() const {
    return m_impl->is_open();
}

BufferSizeResult UdpClient::receive_buffer_size() const {
    return m_impl->receive_buffer_size();
}

BufferSizeResult UdpClient::send_buffer_size() const {
    return m_impl->send_buffer_size();
}

Error UdpClient::set_receive_buffer_size(std::size_t size) {
    return m_impl->set_receive_buffer_size(size);
}

Error UdpClient::set_send_buffer_size(std::size_t size) {
    return m_impl->set_send_buffer_size(size);
}

} // namespace io
