#pragma once

#include "UdpImplBase.h"

namespace io {
namespace detail {

// TODO: do we need ImplType??? Looks like can replace with 'using ImplType = ParentType::Impl'
template<typename ParentType, typename ImplType>
class UdpClientImplBase : public UdpImplBase<ParentType, ImplType> {
public:
    UdpClientImplBase(EventLoop& loop, ParentType& parent);
    UdpClientImplBase(EventLoop& loop, ParentType& parent, uv_udp_t* udp_handle);

    void send_data(const std::string& message, typename ParentType::EndSendCallback  callback);
    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, typename ParentType::EndSendCallback  callback);

protected:
    struct SendRequest : public uv_udp_send_t {
        uv_buf_t uv_buf;
        std::shared_ptr<const char> buf;
        typename ParentType::EndSendCallback end_send_callback = nullptr;
    };

    // statics
    static void on_send(uv_udp_send_t* req, int status);
};

template<typename ParentType, typename ImplType>
UdpClientImplBase<ParentType, ImplType>::UdpClientImplBase(EventLoop& loop, ParentType& parent) :
    UdpImplBase<ParentType, ImplType>(loop, parent) {
}

template<typename ParentType, typename ImplType>
UdpClientImplBase<ParentType, ImplType>::UdpClientImplBase(EventLoop& loop, ParentType& parent, uv_udp_t* udp_handle) :
    UdpImplBase<ParentType, ImplType>(loop, parent, udp_handle) {
}

template<typename ParentType, typename ImplType>
void UdpClientImplBase<ParentType, ImplType>::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, typename ParentType::EndSendCallback callback) {
    auto req = new SendRequest;
    req->end_send_callback = callback;
    req->buf = buffer;
    // const_cast is a workaround for lack of constness support in uv_buf_t
    req->uv_buf = uv_buf_init(const_cast<char*>(buffer.get()), size);

    int uv_status = uv_udp_send(req,
                                UdpImplBase<ParentType, ImplType>::m_udp_handle.get(),
                                &req->uv_buf,
                                1,
                                reinterpret_cast<const sockaddr*>(&(UdpImplBase<ParentType, ImplType>::m_raw_unix_addr)),
                                on_send);
    if (uv_status < 0) {

    }
}

template<typename ParentType, typename ImplType>
void UdpClientImplBase<ParentType, ImplType>::send_data(const std::string& message, typename ParentType::EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::memcpy(ptr.get(), message.c_str(), message.size());
    send_data(ptr, static_cast<std::uint32_t>(message.size()), callback);
}

template<typename ParentType, typename ImplType>
void UdpClientImplBase<ParentType, ImplType>::on_send(uv_udp_send_t* req, int uv_status) {
    auto& request = *reinterpret_cast<SendRequest*>(req);
    auto& this_ = *reinterpret_cast<ImplType*>(req->handle->data);
    auto& parent = *this_.m_parent;

    Error error(uv_status);
    if (request.end_send_callback) {
        request.end_send_callback(parent, error);
    }

    delete &request;
}

} // namespace detail
} // namespace io
