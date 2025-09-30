#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <mutex>
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
        using ClientId       = std::uint64_t;
        using OnConnect      = std::function<void(ClientId, std::shared_ptr<Socket>)>;
        using MessageHandler = std::function<void(ClientId, std::shared_ptr<Socket>, const char*, std::size_t)>;
        using OnDisconnect   = std::function<void(ClientId)>;

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

        void start(std::size_t workers, MessageHandler on_message) {
            start(workers, OnConnect{}, std::move(on_message), OnDisconnect{});
        }

        void start(std::size_t workers, OnConnect on_connect, MessageHandler on_message, OnDisconnect on_disconnect) {
            if (running_.exchange(true)) {
                return;
            }
            on_connect_    = std::move(on_connect);
            on_message_    = std::move(on_message);
            on_disconnect_ = std::move(on_disconnect);
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
            {
                std::lock_guard<std::mutex> lk(clients_mtx_);
                for (auto &kv : clients_) if (kv.second) kv.second->close();
                clients_.clear();
            }
            pool_.reset();
        }

        bool isRunning() const noexcept { return running_.load(); }

        bool sendTo(ClientId id, const void* data, std::size_t len) {
            std::shared_ptr<Socket> s;
            {
                std::lock_guard<std::mutex> lk(clients_mtx_);
                auto it = clients_.find(id);
                if (it == clients_.end()) return false;
                s = it->second;
            }
            return s && (s->send_all(data, len) == static_cast<std::ptrdiff_t>(len));
        }

        bool sendTextTo(ClientId id, const std::string& text) {
            return sendTo(id, text.data(), text.size());
        }

        std::size_t broadcast(const void* data, std::size_t len) {
            std::vector<std::shared_ptr<Socket>> copy;
            {
                std::lock_guard<std::mutex> lk(clients_mtx_);
                copy.reserve(clients_.size());
                for (auto &kv : clients_) copy.push_back(kv.second);
            }
            std::size_t ok = 0;
            for (auto &s : copy) if (s && s->send_all(data, len) == static_cast<std::ptrdiff_t>(len)) ++ok;
            return ok;
        }

        std::size_t broadcastText(const std::string& text) {
            return broadcast(text.data(), text.size());
        }

        std::vector<ClientId> clientIds() const {
            std::vector<ClientId> ids;
            std::lock_guard<std::mutex> lk(clients_mtx_);
            ids.reserve(clients_.size());
            for (auto &kv : clients_) ids.push_back(kv.first);
            return ids;
        }

        bool closeClient(ClientId id) {
            std::shared_ptr<Socket> s;
            {
                std::lock_guard<std::mutex> lk(clients_mtx_);
                auto it = clients_.find(id);
                if (it == clients_.end()) return false;
                s = it->second;
                clients_.erase(it);
            }
            if (s) s->close();
            return true;
        }

        std::size_t numClients() const {
            std::lock_guard<std::mutex> lk(clients_mtx_);
            return clients_.size();
        }

    private:
        std::atomic<ClientId> next_id_{1};
        mutable std::mutex clients_mtx_;
        std::unordered_map<ClientId, std::shared_ptr<Socket>> clients_;
        OnConnect on_connect_;
        MessageHandler on_message_;
        OnDisconnect on_disconnect_;

        Socket listener_;
        std::atomic<bool> running_;
        std::unique_ptr<ThreadPool> pool_;
        std::thread accept_thread_;

        void handleClient(ClientId id, std::shared_ptr<Socket> client) {
            // Raw chunked reads; your on_message can parse frames if you use them.
            char buf[4096];
            for (;;) {
                auto r = client->receive(buf, sizeof(buf));
                if (r <= 0) break; // disconnect or error
                if (on_message_) on_message_(id, client, buf, static_cast<std::size_t>(r));
            }
            if (on_disconnect_) on_disconnect_(id);
            {
                std::lock_guard<std::mutex> lk(clients_mtx_);
                clients_.erase(id);
            }
        }

        void acceptLoop() {
            while (running_.load()) {
                auto client = listener_.accept();
                if (!client) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }

                // Assign ID and store
                ClientId id = next_id_.fetch_add(1, std::memory_order_relaxed);
                {
                    std::lock_guard<std::mutex> lk(clients_mtx_);
                    clients_.emplace(id, client);
                }

                if (on_connect_) on_connect_(id, client);

                if (pool_) {
                    pool_->enqueue([this, id, client]{ handleClient(id, client); });
                } else {
                    // Fallback: handle inline (not recommended for production)
                    handleClient(id, client);
                }
            }
        }
    };
}
