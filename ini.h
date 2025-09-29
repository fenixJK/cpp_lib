#pragma once

#include <cctype>
#include <fstream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

namespace cpplib {
    class Ini {
    public:
        using Value = std::variant<int, double, bool, std::string>;

        class Section {
        public:
            Value& operator[](const std::string& key) {
                return entries[key];
            }

            const Value& at(const std::string& key) const {
                auto it = entries.find(key);
                if (it == entries.end()) {
                    throw std::out_of_range("Key not found: " + key);
                }
                return it->second;
            }

            bool contains(const std::string& key) const {
                return entries.find(key) != entries.end();
            }

            auto begin() { return entries.begin(); }
            auto end() { return entries.end(); }
            auto begin() const { return entries.begin(); }
            auto end() const { return entries.end(); }

            auto find(const std::string& key) { return entries.find(key); }
            auto find(const std::string& key) const { return entries.find(key); }

        private:
            std::map<std::string, Value> entries;
        };

        Section& operator[](const std::string& section) {
            return sections[section];
        }

        const Section& at(const std::string& section) const {
            auto it = sections.find(section);
            if (it == sections.end()) {
                throw std::out_of_range("Section not found: " + section);
            }
            return it->second;
        }

        bool has_section(const std::string& section) const {
            return sections.find(section) != sections.end();
        }

        bool has_key(const std::string& section, const std::string& key) const {
            auto it = sections.find(section);
            return it != sections.end() && it->second.contains(key);
        }

        void set(const std::string& section, const std::string& key, const Value& value) {
            sections[section][key] = value;
        }

        template <typename T>
        T get_value(const std::string& section, const std::string& key) const {
            auto opt = try_get_value<T>(section, key);
            if (!opt) {
                throw std::out_of_range("Key not found or wrong type: " + section + ":" + key);
            }
            return *opt;
        }

        template <typename T>
        std::optional<T> try_get_value(const std::string& section, const std::string& key) const {
            auto sec_it = sections.find(section);
            if (sec_it == sections.end()) {
                return std::nullopt;
            }
            const auto& section_ref = sec_it->second;
            auto value_it = section_ref.find(key);
            if (value_it == section_ref.end()) {
                return std::nullopt;
            }
            if (std::holds_alternative<T>(value_it->second)) {
                return std::get<T>(value_it->second);
            }
            return std::nullopt;
        }

        bool load(const std::string& filename);
        bool save(const std::string& filename) const;
        bool save(const std::string& filename, const std::string& section) const;
        bool save(const std::string& filename, const std::string& section, const std::string& key) const;

        std::optional<Value> try_get(const std::string& section, const std::string& key) const;

        auto begin() { return sections.begin(); }
        auto end() { return sections.end(); }
        auto begin() const { return sections.begin(); }
        auto end() const { return sections.end(); }

    private:
        static Value parse_value(const std::string& raw);
        static std::string format_value(const Value& value);
        static void trim(std::string& value);

        std::map<std::string, Section> sections;
    };

    inline bool Ini::load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        std::string current_section;

        while (std::getline(file, line)) {
            trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue;
            }

            if (line.front() == '[' && line.back() == ']') {
                current_section = line.substr(1, line.size() - 2);
                trim(current_section);
                if (!current_section.empty()) {
                    sections.emplace(current_section, Section{});
                }
                continue;
            }

            const auto equals_pos = line.find('=');
            if (equals_pos == std::string::npos || current_section.empty()) {
                continue;
            }

            auto key = line.substr(0, equals_pos);
            auto value = line.substr(equals_pos + 1);
            trim(key);
            trim(value);
            if (!key.empty()) {
                sections[current_section][key] = parse_value(value);
            }
        }

        return true;
    }

    inline bool Ini::save(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        for (const auto& section : sections) {
            file << '[' << section.first << "]\n";
            for (const auto& entry : section.second) {
                file << entry.first << '=' << format_value(entry.second) << "\n";
            }
            file << "\n";
        }
        return true;
    }

    inline bool Ini::save(const std::string& filename, const std::string& section) const {
        auto it = sections.find(section);
        if (it == sections.end()) {
            return false;
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        file << '[' << section << "]\n";
        for (const auto& entry : it->second) {
            file << entry.first << '=' << format_value(entry.second) << "\n";
        }
        return true;
    }

    inline bool Ini::save(const std::string& filename, const std::string& section, const std::string& key) const {
        auto sec_it = sections.find(section);
        if (sec_it == sections.end()) {
            return false;
        }

        auto value_it = sec_it->second.find(key);
        if (value_it == sec_it->second.end()) {
            return false;
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        file << '[' << section << "]\n";
        file << key << '=' << format_value(value_it->second) << "\n";
        return true;
    }

    inline std::optional<Ini::Value> Ini::try_get(const std::string& section, const std::string& key) const {
        auto sec_it = sections.find(section);
        if (sec_it == sections.end()) {
            return std::nullopt;
        }
        auto value_it = sec_it->second.find(key);
        if (value_it == sec_it->second.end()) {
            return std::nullopt;
        }
        return value_it->second;
    }

    inline Ini::Value Ini::parse_value(const std::string& raw) {
        if (raw.empty()) {
            return std::string{};
        }

        std::string lowered;
        lowered.reserve(raw.size());
        for (char ch : raw) {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }

        if (lowered == "true" || lowered == "false") {
            return lowered == "true";
        }

        try {
            size_t consumed = 0;
            const int int_value = std::stoi(raw, &consumed, 0);
            if (consumed == raw.size()) {
                return int_value;
            }
        } catch (...) {
        }

        try {
            size_t consumed = 0;
            const double double_value = std::stod(raw, &consumed);
            if (consumed == raw.size()) {
                return double_value;
            }
        } catch (...) {
        }

        return raw;
    }

    inline std::string Ini::format_value(const Value& value) {
        return std::visit([](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, int>) {
                return std::to_string(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::to_string(v);
            } else {
                return v;
            }
        }, value);
    }

    inline void Ini::trim(std::string& value) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            value.clear();
            return;
        }
        const auto last = value.find_last_not_of(" \t\r\n");
        value = value.substr(first, last - first + 1);
    }
}

using Ini = cpplib::Ini;
