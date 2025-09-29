#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <arpa/inet.h>
#endif

#include "socket.h"
#include "threadpool.h"

namespace cpplib {
    class TcpClient {
    public:
        TcpClient() = default;

        bool connect(const std::string& host, int port, int timeout_ms = 3000) {
            if (!socket_.connect(host, port)) return false;
            // Optional low-latency defaults and timeouts
            socket_.set_timeouts(timeout_ms, timeout_ms);
            return true;
        }

        bool send(const void* data, std::size_t length) {
            auto n = socket_.send_all(data, length);
            return n == static_cast<std::ptrdiff_t>(length);
        }

        bool send(const std::string& data) {
            return send(data.data(), data.size());
        }

        std::ptrdiff_t receive(void* buffer, std::size_t length) {
            return socket_.receive(buffer, length);
        }

        std::string receive(std::size_t length) {
            if (length == 0) return {};
            std::string out;
            out.resize(length);
            auto got = socket_.recv_exact(out.data(), length);
            if (got <= 0) return {};
            out.resize(static_cast<std::size_t>(got));
            return out;
        }

        bool sendFrame(const void* data, std::uint32_t len) {
            std::uint32_t be = htonl(len);
            if (socket_.send_all(&be, 4) < 0) return false;
            if (len == 0) return true;
            return socket_.send_all(data, len) == static_cast<std::ptrdiff_t>(len);
        }

        bool sendFrame(const std::string& s) { return sendFrame(s.data(), static_cast<std::uint32_t>(s.size())); }

        bool recvFrame(std::vector<std::uint8_t>& out, int timeout_ms) {
            if (!socket_.wait_readable(timeout_ms)) return false;
            std::uint32_t be = 0;
            if (socket_.recv_exact(&be, 4) <= 0) return false;
            std::uint32_t need = ntohl(be);
            out.resize(need);
            if (need == 0) return true;
            return socket_.recv_exact(out.data(), need) == static_cast<std::ptrdiff_t>(need);
        }

        bool recvFrame(std::string& out, int timeout_ms) {
            std::vector<std::uint8_t> buf;
            if (!recvFrame(buf, timeout_ms)) return false;
            out.assign(reinterpret_cast<const char*>(buf.data()), buf.size());
            return true;
        }

        void close() {
            socket_.close();
        }

        bool connected() const { return socket_.valid(); }

    private:
        Socket socket_;
    };

    class TcpServer {
    public:
        using ClientHandler = std::function<void(std::shared_ptr<Socket>)>;

        TcpServer() : running_(false) {}
        ~TcpServer() { stop(); }

        bool bind(int port) {
            return listener_.bind(port);
        }

        bool listen(int backlog = 16) {
            return listener_.listen(backlog);
        }

        std::uint16_t port() const {
            return listener_.localPort();
        }

        void start(std::size_t workers, ClientHandler handler) {
            if (running_.exchange(true)) {
                return;
            }
            handler_ = std::move(handler);
            pool_ = std::make_unique<ThreadPool>(workers);
            accept_thread_ = std::thread([this] { acceptLoop(); });
        }

        void stop() {
            if (!running_.exchange(false)) {
                return;
            }
            listener_.close();  // closing unblocks accept()
            if (accept_thread_.joinable()) {
                accept_thread_.join();
            }
            pool_.reset();
        }

        bool isRunning() const noexcept { return running_.load(); }

    private:
        void acceptLoop() {
            while (running_.load()) {
                auto client = listener_.accept();
                if (!client) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }

                if (handler_ && pool_) {
                    pool_->enqueue(handler_, client);
                }
            }
        }

        Socket listener_;
        std::atomic<bool> running_;
        std::unique_ptr<ThreadPool> pool_;
        ClientHandler handler_;
        std::thread accept_thread_;
    };
}
