#include "../timer.h"
#include "../config.h"
#include "../ini.h"
#include "../tcp.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

namespace {
    int failures = 0;

    void expect(bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << "[FAIL] " << message << std::endl;
            ++failures;
        } else {
            std::cout << "[PASS] " << message << std::endl;
        }
    }

    void test_stopwatch_and_scoped_timer() {
        cpplib::Stopwatch watch;
        cpplib::hypersleep(std::chrono::microseconds(5000));
        auto elapsed = watch.elapsed<std::chrono::microseconds>();
        expect(elapsed.count() >= 5000, "Stopwatch captures elapsed time" + std::to_string(elapsed.count()));
        bool callback_triggered = false;
        {
            cpplib::ScopedTimer timer("scope", [&](std::string_view label, std::chrono::nanoseconds duration) {
                callback_triggered = true;
                expect(label == "scope", "ScopedTimer forwards label");
                expect(duration.count() > 0, "ScopedTimer duration positive");
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        expect(callback_triggered, "ScopedTimer callback triggered");
    }

    std::filesystem::path write_temp_file(const std::string& name, const std::string& contents) {
        auto tmp = std::filesystem::temp_directory_path() / name;
        std::ofstream out(tmp);
        out << contents;
        return tmp;
    }

    void test_ini_and_config_sources() {
        const auto ini_path = write_temp_file("cpplib_config.ini",
            "[network]\nport=8080\nsecure=true\n\n[logging]\nlevel=INFO\n");

        cpplib::Config config;
        config.addSource(std::make_shared<cpplib::IniConfigSource>(ini_path.string()));
        expect(config.reloadAll(), "Config reloads INI source");
        expect(config.get<int>("network", "port").value_or(0) == 8080, "INI int retrieval");
        expect(config.get<bool>("network", "secure").value_or(false), "INI bool retrieval");
        expect(config.get<std::string>("logging", "level").value_or("") == "INFO", "INI string retrieval");

        const auto json_path = write_temp_file("cpplib_config.json",
            "{\n  \"network\": {\n    \"host\": \"127.0.0.1\",\n    \"port\": 4040,\n    \"secure\": false,\n    \"ratio\": 0.5\n  },\n  \"feature\": {\n    \"enabled\": true\n  }\n}\n");

        config.addSource(std::make_shared<cpplib::JsonConfigSource>(json_path.string()));
        expect(config.reloadAll(), "Config reloads JSON source");
        expect(config.get<std::string>("network", "host").value_or("") == "127.0.0.1", "JSON string retrieval");
        expect(config.get<int>("network", "port").value_or(0) == 4040, "JSON int retrieval");
        expect(config.get<double>("network", "ratio").value_or(0.0) == 0.5, "JSON double retrieval");
        expect(config.get<bool>("feature", "enabled").value_or(false), "JSON bool retrieval");
    }

    void test_tcp_server_client_roundtrip() {
        cpplib::TcpServer server;
        expect(server.bind(0), "Server binds to ephemeral port");
        expect(server.listen(), "Server starts listening");

        auto received_promise = std::make_shared<std::promise<void>>();
        auto received_future = received_promise->get_future();

        server.start(2, [received_promise](std::shared_ptr<cpplib::Socket> client) {
            char buffer[16] = {0};
            auto bytes = client->receive(buffer, sizeof(buffer));
            if (bytes > 0) {
                const std::string data(buffer, static_cast<std::size_t>(bytes));
                if (data == "ping") {
                    const std::string response = "pong";
                    client->send(response.data(), response.size());
                    received_promise->set_value();
                }
            }
            client->shutdown();
            client->close();
        });

        cpplib::TcpClient client;
        expect(client.connect("127.0.0.1", server.port()), "Client connects to server");
        expect(client.send("ping"), "Client sends request");
        auto reply = client.receive(16);
        expect(reply == "pong", "Client receives response");
        client.close();

        expect(received_future.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
               "Server handler executed");

        server.stop();
        expect(!server.isRunning(), "Server stops cleanly");
    }
}

int main() {
    test_stopwatch_and_scoped_timer();
    test_ini_and_config_sources();
    test_tcp_server_client_roundtrip();

    if (failures) {
        std::cerr << failures << " test(s) failed" << std::endl;
        return 1;
    }
    std::cout << "All tests passed" << std::endl;
    return 0;
}
