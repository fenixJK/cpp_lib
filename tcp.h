#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "socket.h"
#include "threadpool.h"

namespace cpplib {
    class TcpClient {
    public:
        TcpClient() = default;

        bool connect(const std::string& host, int port) {
            return socket_.connect(host, port);
        }

        bool send(const void* data, std::size_t length) {
            return socket_.send(data, length);
        }

        bool send(const std::string& data) {
            return send(data.data(), data.size());
        }

        std::ptrdiff_t receive(void* buffer, std::size_t length) {
            return socket_.receive(buffer, length);
        }

        std::string receive(std::size_t length) {
            std::vector<char> buffer(length);
            const auto received = socket_.receive(buffer.data(), buffer.size());
            if (received <= 0) {
                return {};
            }
            return std::string(buffer.data(), static_cast<std::size_t>(received));
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
            listener_.shutdown();
            listener_.close();
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
