#pragma once

#include <iostream>
#include <string>
#include <memory>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
#elif defined(__linux__) || defined(__APPLE__)
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#else
    #error "Unsupported platform"
#endif

namespace cpplib {
    class Socket {
    public:
        Socket();
        ~Socket();

        bool bind(int port);
        bool listen(int backlog = 5);
        std::shared_ptr<Socket> accept(); // Accept a connection
        bool connect(const std::string& address, int port);
        bool send(const void* buffer, size_t length);
        ssize_t receive(void* buffer, size_t length);
        void close(); // Close the socket

    private:
        int sockfd;

#if defined(_WIN32) || defined(_WIN64)
        WSADATA wsaData;
#endif

        void closeSocket();
    };
}

namespace cpplib {
    #if defined(_WIN32) || defined(_WIN64)
    // Windows-specific implementation

    Socket::Socket() {
        WSAStartup(MAKEWORD(2, 2), &wsaData); // Initialize Winsock
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == INVALID_SOCKET) {
            return;
        }
    }

    Socket::~Socket() {
        closeSocket();
        WSACleanup(); // Clean up Winsock
    }

    bool Socket::bind(int port) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            return false;
        }
        return true;
    }

    bool Socket::listen(int backlog) {
        if (::listen(sockfd, backlog) < 0) {
            return false;
        }
        return true;
    }

    std::shared_ptr<Socket> Socket::accept() {
        int client_sockfd = ::accept(sockfd, nullptr, nullptr);
        if (client_sockfd == INVALID_SOCKET) {
            return nullptr;
        }
        auto client = std::make_shared<Socket>();
        client->sockfd = client_sockfd;
        return client;
    }

    bool Socket::connect(const std::string& address, int port) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(address.c_str());

        if (::connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            return false;
        }
        return true;
    }

    bool Socket::send(const void* buffer, size_t length) {
        if (::send(sockfd, static_cast<const char*>(buffer), length, 0) < 0) {
            return false;
        }
        return true;
    }

    ssize_t Socket::receive(void* buffer, size_t length) {
        return ::recv(sockfd, static_cast<char*>(buffer), length, 0);
    }

    void Socket::closeSocket() {
        if (sockfd != INVALID_SOCKET) {
            closesocket(sockfd); // Close socket on Windows
            sockfd = INVALID_SOCKET;
        }
    }

    #elif defined(__linux__) || defined(__APPLE__)
    // Linux/macOS-specific implementation

    Socket::Socket() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0); // Linux/macOS socket creation
        if (sockfd < 0) {
            return;
        }
    }

    Socket::~Socket() {
        closeSocket();
    }

    bool Socket::bind(int port) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            return false;
        }
        return true;
    }

    bool Socket::listen(int backlog) {
        if (::listen(sockfd, backlog) < 0) {
            return false;
        }
        return true;
    }

    std::shared_ptr<Socket> Socket::accept() {
        int client_sockfd = ::accept(sockfd, nullptr, nullptr);
        if (client_sockfd == -1) {
            return nullptr;
        }
        auto client = std::make_shared<Socket>();
        client->sockfd = client_sockfd;
        return client;
    }

    bool Socket::connect(const std::string& address, int port) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(address.c_str());

        if (::connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            return false;
        }
        return true;
    }

    bool Socket::send(const void* buffer, size_t length) {
        if (::send(sockfd, static_cast<const char*>(buffer), length, 0) < 0) {
            return false;
        }
        return true;
    }

    ssize_t Socket::receive(void* buffer, size_t length) {
        return ::recv(sockfd, static_cast<char*>(buffer), length, 0);
    }

    void Socket::closeSocket() {
        if (sockfd != -1) {
            ::close(sockfd); // Close socket on Linux/macOS
            sockfd = -1;
        }
    }

    #endif
}