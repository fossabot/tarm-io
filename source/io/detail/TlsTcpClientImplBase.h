#pragma once

#include "io/DataChunk.h"
#include "io/EventLoop.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

//#include <memory>
//#include <assert.h>
#include <iostream>

namespace io {
namespace detail {

template<typename ParentType, typename ImplType>
class TlsTcpClientImplBase {
public:
    TlsTcpClientImplBase(EventLoop& loop, ParentType& parent);
    ~TlsTcpClientImplBase();

    bool schedule_removal();

    bool ssl_init();
    virtual const SSL_METHOD* ssl_method() = 0;
    virtual bool ssl_set_siphers() = 0;
    virtual bool ssl_init_certificate_and_key() = 0;
    virtual void ssl_set_state() = 0;

    void read_from_ssl();
    virtual void on_ssl_read(const DataChunk&) = 0;

    void do_handshake();
    void handshake_read_from_sll_and_send();
    virtual void on_handshake_complete() = 0;

    void send_data(std::shared_ptr<const char> buffer, std::uint32_t size, typename ParentType::EndSendCallback callback);
    void send_data(const std::string& message, typename ParentType::EndSendCallback callback);

    void on_data_receive(const char* buf, std::size_t size);

    bool is_open() const;

protected:
    ParentType* m_parent;
    EventLoop* m_loop;

    typename ParentType::UnderlyingClientType* m_client = nullptr;

    SSL_CTX* m_ssl_ctx = nullptr;
    BIO* m_ssl_read_bio = nullptr;
    BIO* m_ssl_write_bio = nullptr;
    SSL* m_ssl = nullptr;

    bool m_ssl_handshake_complete = false;

    // Removal is scheduled in 2 steps. First underlying connection is removed, then secure one.
    bool m_ready_schedule_removal = false;
};

///////////////////////////////////////// implementation ///////////////////////////////////////////

template<typename ParentType, typename ImplType>
TlsTcpClientImplBase<ParentType, ImplType>::TlsTcpClientImplBase(EventLoop& loop, ParentType& parent) :
    m_parent(&parent),
    m_loop(&loop) {
}

template<typename ParentType, typename ImplType>
TlsTcpClientImplBase<ParentType, ImplType>::~TlsTcpClientImplBase() {
    // TODO: smart pointers???
    if (m_ssl) {
        SSL_free(m_ssl);
    }

    if (m_ssl_ctx) {
        SSL_CTX_free(m_ssl_ctx);
    }
}

template<typename ParentType, typename ImplType>
bool TlsTcpClientImplBase<ParentType, ImplType>::is_open() const {
    return m_client && m_client->is_open();
}

template<typename ParentType, typename ImplType>
bool TlsTcpClientImplBase<ParentType, ImplType>::ssl_init() {
    // TOOD: looks like this could be done only once
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    int result = SSL_library_init();

    //SSL_load_error_strings();
    //OpenSSL_add_ssl_algorithms();
    if (!result) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to init OpenSSL");
        return false;
    }

    //OpenSSL_add_all_algorithms();
    //OpenSSL_add_all_ciphers();
    //OpenSSL_add_all_digests();

    // TODO: support different versions of TLS
    m_ssl_ctx = SSL_CTX_new(ssl_method()/*TLSv1_2_server_method()*/);
    if (m_ssl_ctx == nullptr) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to init SSL context");
        return false;
    }

    // TODO: remove ???
    //SSL_CTX_set_ecdh_auto(m_ssl_ctx, 1);

    // TODO: remove this
    SSL_CTX_set_verify(m_ssl_ctx, SSL_VERIFY_NONE, NULL);

    if (!ssl_set_siphers()) {
        return false;
    }

   if (!ssl_init_certificate_and_key()) {
       return false;
   }

    // TODO: most likely need to set also
    // SSL_CTX_set_verify
    // SSL_CTX_set_verify_depth

    m_ssl = SSL_new(m_ssl_ctx);
    if (m_ssl == nullptr) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to create SSL");
        return false;
    }

    m_ssl_read_bio = BIO_new(BIO_s_mem());
    if (m_ssl_read_bio == nullptr) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to create read BIO");
        return false;
    }

    m_ssl_write_bio = BIO_new(BIO_s_mem());
    if (m_ssl_write_bio == nullptr) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to create write BIO");
        return false;
    }

    SSL_set_bio(m_ssl, m_ssl_read_bio, m_ssl_write_bio);

    ssl_set_state();

    IO_LOG(m_loop, DEBUG, m_parent, "SSL inited");

    return true;
}

template<typename ParentType, typename ImplType>
void TlsTcpClientImplBase<ParentType, ImplType>::read_from_ssl() {
    // TODO: investigate if this buffer can be of less size
    // TODO: not create this buffer on every read
    const std::size_t SIZE = 16*1024; // https://www.openssl.org/docs/man1.0.2/man3/SSL_read.html
    std::shared_ptr<char> decrypted_buf(new char[SIZE], std::default_delete<char[]>());
    // TODO: TEST save buffer in callback and check if it is valid

    int decrypted_size = SSL_read(m_ssl, decrypted_buf.get(), SIZE);
    while (decrypted_size > 0) {
        IO_LOG(m_loop, TRACE, m_parent, "Decrypted message of size:", decrypted_size);
        on_ssl_read({decrypted_buf, static_cast<std::size_t>(decrypted_size)});
        decrypted_size = SSL_read(m_ssl, decrypted_buf.get(), SIZE);
    }

    if (decrypted_size < 0) {
        int code = SSL_get_error(m_ssl, decrypted_size);
        if (code != SSL_ERROR_WANT_READ) {
            IO_LOG(m_loop, ERROR, m_parent, "Failed to write buf of size", code);
            // TODO: handle error
            return;
        }
    }
}

template<typename ParentType, typename ImplType>
void TlsTcpClientImplBase<ParentType, ImplType>::handshake_read_from_sll_and_send() {
    const std::size_t BUF_SIZE = 4096;
    std::shared_ptr<char> buf(new char[BUF_SIZE], [](const char* p) { delete[] p; });
    const auto size = BIO_read(m_ssl_write_bio, buf.get(), BUF_SIZE);

    IO_LOG(m_loop, TRACE, m_parent, "Getting data from SSL and sending to server, size:", size);
    m_client->send_data(buf, size);
}

template<typename ParentType, typename ImplType>
void TlsTcpClientImplBase<ParentType, ImplType>::do_handshake() {
    IO_LOG(m_loop, DEBUG, m_parent, "Doing handshake");

    auto handshake_result = SSL_do_handshake(m_ssl);

    int write_pending = BIO_pending(m_ssl_write_bio);
    int read_pending = BIO_pending(m_ssl_read_bio);
    IO_LOG(m_loop, TRACE, m_parent, "write_pending:", write_pending);
    IO_LOG(m_loop, TRACE, m_parent, "read_pending:", read_pending);

    if (handshake_result < 0) {
        auto error = SSL_get_error(m_ssl, handshake_result);

        if (error == SSL_ERROR_WANT_READ) {
            IO_LOG(m_loop, TRACE, m_parent, "SSL_ERROR_WANT_READ");

            handshake_read_from_sll_and_send();
        } else if (error == SSL_ERROR_WANT_WRITE) {
            IO_LOG(m_loop, TRACE, m_parent, "SSL_ERROR_WANT_WRITE");
        } else {
            char msg[1024];
            ERR_error_string_n(ERR_get_error(), msg, sizeof(msg));
            printf("%s %s %s %s\n", msg, ERR_lib_error_string(0), ERR_func_error_string(0), ERR_reason_error_string(0));

            // TODO: error handling here
        }
    } else if (handshake_result == 1) {
        IO_LOG(m_loop, DEBUG, m_parent, "Connected!");

        if (write_pending) {
            handshake_read_from_sll_and_send();
        }

        m_ssl_handshake_complete = true;

        on_handshake_complete();

        if (read_pending) {
            read_from_ssl();
        }
    } else {
        IO_LOG(m_loop, ERROR, m_parent, "The TLS/SSL handshake was not successful but was shut down controlled and by the specifications of the TLS/SSL protocol.");
        // TODO: error handling
    }
}

template<typename ParentType, typename ImplType>
void TlsTcpClientImplBase<ParentType, ImplType>::send_data(std::shared_ptr<const char> buffer, std::uint32_t size, typename ParentType::EndSendCallback callback) {

    if (!is_open()) {
        callback(*m_parent, Error(StatusCode::SOCKET_IS_NOT_CONNECTED));
        return;
    }

    const auto write_result = SSL_write(m_ssl, buffer.get(), size);
    if (write_result <= 0) {
        IO_LOG(m_loop, ERROR, m_parent, "Failed to write buf of size", size);

        const auto openss_error_code = ERR_get_error();
        if (callback) {
            callback(*m_parent, Error(StatusCode::OPENSSL_ERROR, ERR_reason_error_string(openss_error_code)));
        }

        return;
    }

    // TODO: fixme
    const std::size_t SIZE = 1024 + size * 2; // TODO:
    std::shared_ptr<char> ptr(new char[SIZE], [](const char* p) { delete[] p;});

    const auto actual_size = BIO_read(m_ssl_write_bio, ptr.get(), SIZE);
    if (actual_size < 0) {
        IO_LOG(m_loop, ERROR, m_parent, "BIO_read failed code:", actual_size);
        return;
    }

    IO_LOG(m_loop, TRACE, m_parent, "Sending message to client. Original size:", size, "encrypted_size:", actual_size);
    m_client->send_data(ptr, actual_size, [callback, this](typename ParentType::UnderlyingClientType& tcp_client, const Error& error) {
        if (callback) {
            callback(*m_parent, error);
        }
    });
}

template<typename ParentType, typename ImplType>
void TlsTcpClientImplBase<ParentType, ImplType>::send_data(const std::string& message, typename ParentType::EndSendCallback callback) {
    std::shared_ptr<char> ptr(new char[message.size()], [](const char* p) { delete[] p;});
    std::copy(message.c_str(), message.c_str() + message.size(), ptr.get());
    send_data(ptr, static_cast<std::uint32_t>(message.size()), callback);
}

template<typename ParentType, typename ImplType>
void TlsTcpClientImplBase<ParentType, ImplType>::on_data_receive(const char* buf, std::size_t size) {
    IO_LOG(m_loop, TRACE, m_parent, "");

    if (m_ssl_handshake_complete) {
        const auto written_size = BIO_write(m_ssl_read_bio, buf, size);
        if (written_size < 0) {
            IO_LOG(m_loop, ERROR, m_parent, "BIO_write failed with code:", written_size);
            return;
        }

        read_from_ssl();
    } else {
        const auto write_size = BIO_write(m_ssl_read_bio, buf, size);
        // TODO: error handling
        do_handshake();
    }
}

template<typename ParentType, typename ImplType>
bool TlsTcpClientImplBase<ParentType, ImplType>::schedule_removal() {
    IO_LOG(m_loop, TRACE, "");

    if (m_client) {
        if (!m_ready_schedule_removal) {
            m_client->set_on_schedule_removal([this](const Removable&) {
                this->m_parent->schedule_removal();
            });
            m_ready_schedule_removal = true;
            m_client->schedule_removal();
            return false; // postpone removal
        }
    }

    return true;
}

} // namespace detail
} // namespace io
