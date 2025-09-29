#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <cerrno>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace cpplib {
    class Socket {
    public:
        Socket();
        Socket(Socket&& other) noexcept;
        Socket& operator=(Socket&& other) noexcept;
        ~Socket();

        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        bool bind(int port);
        bool listen(int backlog = 5);
        std::shared_ptr<Socket> accept();
        bool connect(const std::string& address, int port);
        bool send(const void* buffer, std::size_t length);
        std::ptrdiff_t receive(void* buffer, std::size_t length);
        void close();
        void shutdown();
        bool valid() const noexcept;
        std::uint16_t localPort() const;

    private:
#if defined(_WIN32) || defined(_WIN64)
        using native_socket_t = SOCKET;
        static constexpr native_socket_t invalid_socket = INVALID_SOCKET;
#else
        using native_socket_t = int;
        static constexpr native_socket_t invalid_socket = -1;
#endif

        explicit Socket(native_socket_t handle);

#if defined(_WIN32) || defined(_WIN64)
        static void ensureWsaStartup();
#endif
        void ensureSocket();
        void closeSocket() noexcept;

        native_socket_t sockfd;
    };
}

namespace cpplib {
#if defined(_WIN32) || defined(_WIN64)
    inline void Socket::ensureWsaStartup() {
        static struct WsaInitializer {
            WsaInitializer() {
                WSADATA data{};
                const auto result = WSAStartup(MAKEWORD(2, 2), &data);
                if (result != 0) {
                    throw std::system_error(result, std::system_category(), "WSAStartup failed");
                }
            }

            ~WsaInitializer() {
                WSACleanup();
            }
        } initializer;
    }
#endif

    inline Socket::Socket() : sockfd(invalid_socket) {
#if defined(_WIN32) || defined(_WIN64)
        ensureWsaStartup();
#endif
    }

    inline Socket::Socket(native_socket_t handle) : sockfd(handle) {
#if defined(_WIN32) || defined(_WIN64)
        ensureWsaStartup();
#endif
    }

    inline Socket::Socket(Socket&& other) noexcept : sockfd(other.sockfd) {
        other.sockfd = invalid_socket;
    }

    inline Socket& Socket::operator=(Socket&& other) noexcept {
        if (this != &other) {
            closeSocket();
            sockfd = other.sockfd;
            other.sockfd = invalid_socket;
        }
        return *this;
    }

    inline Socket::~Socket() {
        closeSocket();
    }

    inline bool Socket::valid() const noexcept {
        return sockfd != invalid_socket;
    }

    inline std::uint16_t Socket::localPort() const {
        if (!valid()) {
            return 0;
        }

        sockaddr_in addr{};
#if defined(_WIN32) || defined(_WIN64)
        int len = sizeof(addr);
        if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&addr), &len) == SOCKET_ERROR) {
            return 0;
        }
#else
        socklen_t len = sizeof(addr);
        if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&addr), &len) < 0) {
            return 0;
        }
#endif
        return ntohs(addr.sin_port);
    }

    inline void Socket::ensureSocket() {
        if (valid()) {
            return;
        }

#if defined(_WIN32) || defined(_WIN64)
        sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == invalid_socket) {
            throw std::system_error(WSAGetLastError(), std::system_category(), "Failed to create socket");
        }
#else
        sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == invalid_socket) {
            throw std::system_error(errno, std::system_category(), "Failed to create socket");
        }
#endif

        int enable = 1;
        ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&enable), sizeof(enable));
    }

    inline bool Socket::bind(int port) {
        ensureSocket();

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

#if defined(_WIN32) || defined(_WIN64)
        const auto result = ::bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (result == SOCKET_ERROR) {
            return false;
        }
#else
        if (::bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            return false;
        }
#endif
        return true;
    }

    inline bool Socket::listen(int backlog) {
        ensureSocket();
#if defined(_WIN32) || defined(_WIN64)
        if (!valid()) {
            return false;
        }
        return ::listen(sockfd, backlog) != SOCKET_ERROR;
#else
        if (!valid()) {
            return false;
        }
        return ::listen(sockfd, backlog) == 0;
#endif
    }

    inline std::shared_ptr<Socket> Socket::accept() {
        if (!valid()) {
            return nullptr;
        }
        sockaddr_in addr{};
#if defined(_WIN32) || defined(_WIN64)
        int addr_len = sizeof(addr);
        native_socket_t client_fd = ::accept(sockfd, reinterpret_cast<sockaddr*>(&addr), &addr_len);
        if (client_fd == invalid_socket) {
            return nullptr;
        }
#else
        socklen_t addr_len = sizeof(addr);
        native_socket_t client_fd = ::accept(sockfd, reinterpret_cast<sockaddr*>(&addr), &addr_len);
        if (client_fd == invalid_socket) {
            return nullptr;
        }
#endif
        return std::shared_ptr<Socket>(new Socket(client_fd));
    }

    inline bool Socket::connect(const std::string& address, int port) {
        ensureSocket();

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));

#if defined(_WIN32) || defined(_WIN64)
        if (InetPton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
            return false;
        }
        if (::connect(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            return false;
        }
#else
        if (::inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
            return false;
        }
        if (::connect(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            return false;
        }
#endif
        return true;
    }

    inline bool Socket::send(const void* buffer, std::size_t length) {
        if (!valid()) {
            return false;
        }
        const char* data = static_cast<const char*>(buffer);
        std::size_t total_sent = 0;

        while (total_sent < length) {
#if defined(_WIN32) || defined(_WIN64)
            const auto sent = ::send(sockfd, data + total_sent,
                                     static_cast<int>(length - total_sent), 0);
            if (sent == SOCKET_ERROR) {
                return false;
            }
#else
            const auto sent = ::send(sockfd, data + total_sent, length - total_sent, 0);
            if (sent < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return false;
            }
#endif
            if (sent == 0) {
                break;
            }
            total_sent += static_cast<std::size_t>(sent);
        }

        return total_sent == length;
    }

    inline std::ptrdiff_t Socket::receive(void* buffer, std::size_t length) {
        if (!valid()) {
            return -1;
        }
        char* data = static_cast<char*>(buffer);

        while (true) {
#if defined(_WIN32) || defined(_WIN64)
            const auto received = ::recv(sockfd, data, static_cast<int>(length), 0);
            if (received == SOCKET_ERROR) {
                const auto error = WSAGetLastError();
                if (error == WSAEINTR) {
                    continue;
                }
                return -1;
            }
            return received;
#else
            const auto received = ::recv(sockfd, data, length, 0);
            if (received < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return -1;
            }
            return received;
#endif
        }
    }

    inline void Socket::close() {
        closeSocket();
    }

    inline void Socket::shutdown() {
        if (!valid()) {
            return;
        }
#if defined(_WIN32) || defined(_WIN64)
        ::shutdown(sockfd, SD_BOTH);
#else
        ::shutdown(sockfd, SHUT_RDWR);
#endif
    }

    inline void Socket::closeSocket() noexcept {
        if (!valid()) {
            return;
        }

#if defined(_WIN32) || defined(_WIN64)
        ::closesocket(sockfd);
#else
        ::close(sockfd);
#endif
        sockfd = invalid_socket;
    }
}
