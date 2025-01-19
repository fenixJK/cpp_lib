#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <variant>
#include <stdexcept>

class Ini {
private:
    using IniValue = std::variant<int, float, bool, std::string>;

    class Encapsulation {
    public:
        IniValue& operator[](const std::string& key) {
            return data[key];
        }

        bool has_key(const std::string& key) {
            return data.find(key) != data.end();
        }

        const IniValue& operator[](const std::string& key) const {
            return data.at(key);
        }

        bool has_key(const std::string& key) const {
            return data.find(key) != data.end();
        }

        auto begin() {
            return data.begin();
        }

        auto end() {
            return data.end();
        }

        auto begin() const {
            return data.begin();
        }

        auto end() const {
            return data.end();
        }

        IniValue at(const std::string& key) {
            if (!has_key(key)) {
                throw std::out_of_range("Key not found: " + key);
            }
            return data.at(key);
        }

        IniValue at(const std::string& key) const {
            if (!has_key(key)) {
                throw std::out_of_range("Key not found: " + key);
            }
            return data.at(key);  // This is the const version
        }

        auto find(const std::string& key) {
            return data.find(key);
        }

        auto find(const std::string& key) const {
            return data.find(key);
        }

    private:
        std::map<std::string, IniValue> data;
    };

    std::map<std::string, Encapsulation> data;

    IniValue parse_value(const std::string& value) {
        if (value == "true" || value == "false") {
            return value == "true";
        }
        try {
            size_t pos;
            int int_value = std::stoi(value, &pos);
            if (pos == value.size()) {
                return int_value;
            }
        } catch (...) {}

        try {
            size_t pos;
            float float_value = std::stof(value, &pos);
            if (pos == value.size()) {
                return float_value;
            }
        } catch (...) {}

        return value;
    }

    std::string format_value(const IniValue& value) const {
        if (std::holds_alternative<int>(value)) {
            return std::to_string(std::get<int>(value));
        } else if (std::holds_alternative<float>(value)) {
            return std::to_string(std::get<float>(value));
        } else if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? "true" : "false";
        } else if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        }
        return "";
    }

public:
    Encapsulation& operator[](const std::string& section) {
        return data[section];
    }

    bool load(const std::string& filename) {
        std::ifstream file(filename);
        std::string line;
        std::string current_section;

        while (std::getline(file, line)) {
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);

            if (line.empty() || line.front() == ';') {
                continue;
            }

            if (line.front() == '[' && line.back() == ']') {
                current_section = line.substr(1, line.size() - 2);
            } else {
                size_t equals_pos = line.find('=');
                if (equals_pos != std::string::npos) {
                    std::string key = line.substr(0, equals_pos);
                    std::string value = line.substr(equals_pos + 1);
                    data[current_section][key] = parse_value(value);
                }
            }
        }

        return true;
    }

    bool save(const std::string& filename) {
        std::ofstream file(filename);
        for (const auto& section : data) {
            file << "[" << section.first << "]\n";
            for (const auto& pair : section.second) {
                file << pair.first << "=" << format_value(pair.second) << "\n";
            }
        }
        return true;
    }

    bool save(const std::string& filename, const std::string& section) {
        std::ofstream file(filename);
        auto section_it = data.find(section);
        if (section_it != data.end()) {
            file << "[" << section << "]\n";
            for (const auto& pair : section_it->second) {
                file << pair.first << "=" << format_value(pair.second) << "\n";
            }
            return true;
        }
        return false;
    }

    bool save(const std::string& filename, const std::string& section, const std::string& key) {
        std::ofstream file(filename);
        auto section_it = data.find(section);
        if (section_it != data.end()) {
            auto key_it = section_it->second.find(key);
            if (key_it != section_it->second.end()) {
                file << "[" << section << "]\n";
                file << key << "=" << format_value(key_it->second) << "\n";
                return true;
            }
        }
        return false;
    }

    void set(const std::string& section, const std::string& key, const IniValue& value) {
        data[section][key] = value;
    }

    bool has_section(const std::string& section) const {
        return data.find(section) != data.end();
    }

    bool has_key(const std::string& section, const std::string& key) const {
        return has_section(section) && data.at(section).has_key(key);
    }

    template<typename T>
    T get_value(const std::string& section, const std::string& key) const {
        auto& enc = data.at(section);
        if (enc.has_key(key)) {
            const auto& value = enc[key];
            if (std::holds_alternative<T>(value)) {
                return std::get<T>(value);
            }
            throw std::bad_variant_access();
        }
        throw std::out_of_range("Key not found: " + key);
    }

    Encapsulation& at(const std::string& section) {
        if (!has_section(section)) {
            throw std::out_of_range("Section not found: " + section);
        }
        return data.at(section);
    }

    auto find(const std::string& section) {
        return data.find(section);
    }

    auto find(const std::string& section) const {
        return data.find(section);
    }

    auto begin() {
        return data.begin();
    }
    auto end() {
        return data.end();
    }
    auto begin() const {
        return data.begin();
    }
    auto end() const {
        return data.end();
    }
};