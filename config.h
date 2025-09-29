#pragma once

#include <cctype>
#include <fstream>
#include <map>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "ini.h"

namespace cpplib {
    class ConfigSource {
    public:
        virtual ~ConfigSource() = default;
        virtual bool reload() = 0;
        virtual std::optional<Ini::Value> find(std::string_view section, std::string_view key) const = 0;
    };

    class IniConfigSource final : public ConfigSource {
    public:
        explicit IniConfigSource(std::string path) : path_(std::move(path)) {
            reload();
        }

        bool reload() override {
            Ini loaded;
            if (!loaded.load(path_)) {
                return false;
            }
            data_ = std::move(loaded);
            return true;
        }

        std::optional<Ini::Value> find(std::string_view section, std::string_view key) const override {
            return data_.try_get(std::string(section), std::string(key));
        }

        const Ini& data() const { return data_; }

    private:
        std::string path_;
        Ini data_;
    };

    class JsonConfigSource;

    class Config {
    public:
        void addSource(std::shared_ptr<ConfigSource> source) {
            sources_.push_back(std::move(source));
        }

        void clearSources() {
            sources_.clear();
        }

        bool reloadAll() {
            bool ok = true;
            for (auto& source : sources_) {
                ok &= source->reload();
            }
            return ok;
        }

        template <typename T>
        std::optional<T> get(std::string_view section, std::string_view key) const {
            for (auto it = sources_.rbegin(); it != sources_.rend(); ++it) {
                if (!(*it)) {
                    continue;
                }
                auto value = (*it)->find(section, key);
                if (!value) {
                    continue;
                }
                if (auto converted = convert<T>(*value)) {
                    return converted;
                }
            }
            return std::nullopt;
        }

        template <typename T>
        T getOr(std::string_view section, std::string_view key, T fallback) const {
            if (auto value = get<T>(section, key)) {
                return *value;
            }
            return fallback;
        }

    private:
        template <typename T>
        static std::optional<T> convert(const Ini::Value& value) {
            if (std::holds_alternative<T>(value)) {
                return std::get<T>(value);
            }

            if constexpr (std::is_same_v<T, std::string>) {
                return std::visit([](const auto& v) {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<V, bool>) {
                        return v ? std::string{"true"} : std::string{"false"};
                    } else if constexpr (std::is_same_v<V, std::string>) {
                        return v;
                    } else {
                        return std::to_string(v);
                    }
                }, value);
            } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
                if (const auto* str = std::get_if<std::string>(&value)) {
                    T converted{};
                    std::istringstream iss(*str);
                    iss >> converted;
                    if (!iss.fail()) {
                        return converted;
                    }
                }
                if (const auto* dbl = std::get_if<double>(&value)) {
                    return static_cast<T>(*dbl);
                }
                if (const auto* integer = std::get_if<int>(&value)) {
                    return static_cast<T>(*integer);
                }
            } else if constexpr (std::is_floating_point_v<T>) {
                if (const auto* str = std::get_if<std::string>(&value)) {
                    T converted{};
                    std::istringstream iss(*str);
                    iss >> converted;
                    if (!iss.fail()) {
                        return converted;
                    }
                }
                if (const auto* dbl = std::get_if<double>(&value)) {
                    return static_cast<T>(*dbl);
                }
                if (const auto* integer = std::get_if<int>(&value)) {
                    return static_cast<T>(*integer);
                }
            } else if constexpr (std::is_same_v<T, bool>) {
                if (const auto* str = std::get_if<std::string>(&value)) {
                    std::string lowered;
                    lowered.reserve(str->size());
                    for (char ch : *str) {
                        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                    }
                    if (lowered == "true") {
                        return true;
                    }
                    if (lowered == "false") {
                        return false;
                    }
                }
                if (const auto* integer = std::get_if<int>(&value)) {
                    return *integer != 0;
                }
            }

            return std::nullopt;
        }

        std::vector<std::shared_ptr<ConfigSource>> sources_;
    };

    namespace detail {
        class JsonParser {
        public:
            explicit JsonParser(std::string text) : text_(std::move(text)) {}

            std::map<std::string, std::map<std::string, Ini::Value>> parse();

        private:
            char peek() const;
            bool eof() const;
            char get();
            void skipWhitespace();
            bool consume(char expected);
            void expect(char expected);
            std::string parseString();
            Ini::Value parseNumber();
            Ini::Value parseLiteral();
            std::map<std::string, Ini::Value> parseSectionObject();

            std::string text_;
            std::size_t pos_ = 0;
        };
    }

    class JsonConfigSource final : public ConfigSource {
    public:
        explicit JsonConfigSource(std::string path) : path_(std::move(path)) {
            reload();
        }

        bool reload() override {
            std::ifstream file(path_);
            if (!file.is_open()) {
                return false;
            }

            std::ostringstream oss;
            oss << file.rdbuf();
            detail::JsonParser parser(oss.str());
            auto sections = parser.parse();

            Ini loaded;
            for (auto& [section, entries] : sections) {
                for (auto& [key, value] : entries) {
                    loaded.set(section, key, value);
                }
            }
            data_ = std::move(loaded);
            return true;
        }

        std::optional<Ini::Value> find(std::string_view section, std::string_view key) const override {
            return data_.try_get(std::string(section), std::string(key));
        }

        const Ini& data() const { return data_; }

    private:
        std::string path_;
        Ini data_;
    };

    inline std::map<std::string, std::map<std::string, Ini::Value>> detail::JsonParser::parse() {
        skipWhitespace();
        expect('{');
        skipWhitespace();

        std::map<std::string, std::map<std::string, Ini::Value>> result;
        bool first = true;
        while (!consume('}')) {
            if (!first) {
                expect(',');
                skipWhitespace();
            }
            first = false;
            skipWhitespace();
            auto section = parseString();
            skipWhitespace();
            expect(':');
            skipWhitespace();
            result.emplace(section, parseSectionObject());
            skipWhitespace();
        }
        return result;
    }

    inline char detail::JsonParser::peek() const {
        if (eof()) {
            return '\0';
        }
        return text_[pos_];
    }

    inline bool detail::JsonParser::eof() const {
        return pos_ >= text_.size();
    }

    inline char detail::JsonParser::get() {
        if (eof()) {
            throw std::runtime_error("Unexpected end of JSON input");
        }
        return text_[pos_++];
    }

    inline void detail::JsonParser::skipWhitespace() {
        while (!eof() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    inline bool detail::JsonParser::consume(char expected) {
        if (!eof() && text_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    inline void detail::JsonParser::expect(char expected) {
        if (!consume(expected)) {
            std::ostringstream oss;
            oss << "Expected '" << expected << "'";
            throw std::runtime_error(oss.str());
        }
    }

    inline std::string detail::JsonParser::parseString() {
        expect('"');
        std::string result;
        while (true) {
            if (eof()) {
                throw std::runtime_error("Unterminated string literal");
            }
            char ch = get();
            if (ch == '"') {
                break;
            }
            if (ch == '\\') {
                if (eof()) {
                    throw std::runtime_error("Invalid escape sequence");
                }
                char escaped = get();
                switch (escaped) {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    default:
                        throw std::runtime_error("Unsupported escape sequence");
                }
            } else {
                result.push_back(ch);
            }
        }
        return result;
    }

    inline Ini::Value detail::JsonParser::parseNumber() {
        std::size_t start = pos_;
        if (peek() == '-') {
            ++pos_;
        }
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            ++pos_;
        }

        bool is_float = false;
        if (peek() == '.') {
            is_float = true;
            ++pos_;
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                ++pos_;
            }
        }

        if (peek() == 'e' || peek() == 'E') {
            is_float = true;
            ++pos_;
            if (peek() == '+' || peek() == '-') {
                ++pos_;
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                ++pos_;
            }
        }

        auto number = text_.substr(start, pos_ - start);
        try {
            if (is_float) {
                return std::stod(number);
            }
            long long value = std::stoll(number, nullptr, 10);
            if (value > std::numeric_limits<int>::max()) {
                return static_cast<double>(value);
            }
            if (value < std::numeric_limits<int>::min()) {
                return static_cast<double>(value);
            }
            return static_cast<int>(value);
        } catch (...) {
            throw std::runtime_error("Invalid numeric literal");
        }
    }

    inline Ini::Value detail::JsonParser::parseLiteral() {
        if (text_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            return true;
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            return false;
        }
        if (text_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            return std::string{};
        }
        throw std::runtime_error("Invalid literal value");
    }

    inline std::map<std::string, Ini::Value> detail::JsonParser::parseSectionObject() {
        skipWhitespace();
        expect('{');
        skipWhitespace();

        std::map<std::string, Ini::Value> entries;
        bool first = true;
        while (!consume('}')) {
            if (!first) {
                expect(',');
                skipWhitespace();
            }
            first = false;
            auto key = parseString();
            skipWhitespace();
            expect(':');
            skipWhitespace();

            Ini::Value value;
            char ch = peek();
            if (ch == '"') {
                value = parseString();
            } else if (ch == '{') {
                throw std::runtime_error("Nested objects beyond two levels are not supported");
            } else if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' ) {
                value = parseNumber();
            } else {
                value = parseLiteral();
            }

            entries.emplace(std::move(key), std::move(value));
            skipWhitespace();
        }
        return entries;
    }
}
