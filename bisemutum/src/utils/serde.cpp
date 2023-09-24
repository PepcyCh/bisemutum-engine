#include <bisemutum/utils/serde.hpp>

#include <sstream>

#include <bisemutum/prelude/misc.hpp>
#include <nlohmann/json.hpp>
#include <toml++/toml.h>

namespace bi::serde {

auto Value::operator[](size_t index) const -> Value const& {
    return std::get<Array>(value_)[index];
}
auto Value::operator[](std::string const& key) const -> Value const& {
    return std::get<Table>(value_).find(key)->second;
}
auto Value::operator[](std::string_view key) const -> Value const& {
    return std::get<Table>(value_).find(key)->second;
}
auto Value::operator[](char const* key) const -> Value const& {
    return std::get<Table>(value_).find(key)->second;
}

auto Value::operator[](size_t index) -> Value& {
    return std::get<Array>(value_)[index];
}
auto Value::operator[](std::string const& key) -> Value& {
    if (value_.index() == 0) { value_ = Table{}; }
    return std::get<Table>(value_).try_emplace(key).first->second;
}
auto Value::operator[](std::string_view key) -> Value& {
    if (value_.index() == 0) { value_ = Table{}; }
    return std::get<Table>(value_).try_emplace(std::string{key}).first->second;
}
auto Value::operator[](char const* key) -> Value& {
    if (value_.index() == 0) { value_ = Table{}; }
    return std::get<Table>(value_).try_emplace(key).first->second;
}

auto Value::at(size_t index) const -> Value const& {
    return std::get<Array>(value_)[index];
}
auto Value::at(std::string const& key) const -> Value const& {
    return std::get<Table>(value_).find(key)->second;
}
auto Value::at(std::string_view key) const -> Value const& {
    return std::get<Table>(value_).find(key)->second;
}
auto Value::at(char const* key) const -> Value const& {
    return std::get<Table>(value_).find(key)->second;
}

auto Value::try_at(size_t index) const -> Value const* {
    if (auto arr = std::get_if<Array>(&value_); arr) {
        return index < arr->size() ? &(*arr)[index] : nullptr;
    } else {
        return nullptr;
    }
}
auto Value::try_at(std::string const& key) const -> Value const* {
    if (auto table = std::get_if<Table>(&value_); table) {
        if (auto it = table->find(key); it != table->end()) {
            return &it->second;
        }
    }
    return nullptr;
}
auto Value::try_at(std::string_view key) const -> Value const* {
    if (auto table = std::get_if<Table>(&value_); table) {
        if (auto it = table->find(key); it != table->end()) {
            return &it->second;
        }
    }
    return nullptr;
}
auto Value::try_at(char const* key) const -> Value const* {
    if (auto table = std::get_if<Table>(&value_); table) {
        if (auto it = table->find(key); it != table->end()) {
            return &it->second;
        }
    }
    return nullptr;
}

auto Value::size() const -> size_t {
    if (auto arr = std::get_if<Array>(&value_); arr) {
        return arr->size();
    } else if (auto table = std::get_if<Table>(&value_); table) {
        return table->size();
    } else {
        return 1;
    }
}

auto Value::contains(std::string const& key) const -> bool {
    if (auto table = std::get_if<Table>(&value_); table) {
        return table->contains(key);
    } else {
        return false;
    }
}
auto Value::contains(std::string_view key) const -> bool {
    if (auto table = std::get_if<Table>(&value_); table) {
        return table->contains(key);
    } else {
        return false;
    }
}
auto Value::contains(char const* key) const -> bool {
    if (auto table = std::get_if<Table>(&value_); table) {
        return table->contains(key);
    } else {
        return false;
    }
}

namespace {

auto value_to_json(Value const&v) -> nlohmann::json {
    nlohmann::json j{};
    std::visit(
        FunctorsHelper{
            [](std::monostate) {},
            [&j](Value::Bool v) { j = v; },
            [&j](Value::Float v) { j = v; },
            [&j](Value::Integer v) { j = v; },
            [&j](Value::String const& v) { j = v; },
            [&j](Value::Array const& v) {
                for (auto& elem : v) {
                    j.push_back(value_to_json(elem));
                }
            },
            [&j](Value::Table const& v) {
                for (auto& [key, elem] : v) {
                    j[key] = value_to_json(elem);
                }
            },
        },
        v.variant()
    );
    return j;
}

auto value_to_toml_node(Value const&v) -> std::unique_ptr<toml::node> {
    std::unique_ptr<toml::node> t{};
    std::visit(
        FunctorsHelper{
            [](std::monostate) {},
            [&t](Value::Bool v) { t = std::make_unique<toml::value<bool>>(v); },
            [&t](Value::Float v) { t = std::make_unique<toml::value<double>>(v); },
            [&t](Value::Integer v) { t = std::make_unique<toml::value<int64_t>>(v); },
            [&t](Value::String const& v) { t = std::make_unique<toml::value<std::string>>(v); },
            [&t](Value::Array const& v) {
                auto arr = std::make_unique<toml::array>();
                for (auto& elem : v) {
                    arr->insert(arr->end(), toml::node_view<toml::node>(value_to_toml_node(elem).get()));
                }
                t = std::move(arr);
            },
            [&t](Value::Table const& v) {
                auto table = std::make_unique<toml::table>();
                for (auto& [key, elem] : v) {
                    table->insert(key, toml::node_view<toml::node>(value_to_toml_node(elem).get()));
                }
                t = std::move(table);
            },
        },
        v.variant()
    );
    return t;
}
auto value_to_toml(Value const&v) -> toml::table {
    auto b = toml::is_node_view<std::unique_ptr<toml::node>>;
    toml::table t{};
    for (auto& [key, elem] : v.get<Value::Table>()) {
        t.insert(key, toml::node_view<toml::node>(value_to_toml_node(elem).get()));
    }
    return t;
}

auto value_from_toml(toml::node& t) -> Value {
    Value v{};
    switch (t.type()) {
        case toml::node_type::boolean:
            v = t.as_boolean()->get();
            break;
        case toml::node_type::integer:
            v = t.as_integer()->get();
            break;
        case toml::node_type::floating_point:
            v = t.as_floating_point()->get();
            break;
        case toml::node_type::string:
            v = t.as_string()->get();
            break;
        case toml::node_type::array:
            for (auto& elem : *t.as_array()) {
                v.push_back(value_from_toml(elem));
            }
            break;
        case toml::node_type::table:
            for (auto& [key, elem] : *t.as_table()) {
                v[key] = value_from_toml(elem);
            }
            break;
        default: break;
    }
    return v;
}

} // namespace

auto Value::to_json(int indent) const -> std::string {
    auto json = value_to_json(*this);
    return json.dump(indent);
}
auto Value::to_toml() const -> std::string {
    auto toml = value_to_toml(*this);
    std::string toml_str{};
    std::ostringstream sout{toml_str};
    sout << toml;
    return toml_str;
}

auto Value::from_json(std::string_view json_str) -> Value {
    auto j = nlohmann::json::parse(json_str);
    Value v{};
    switch (j.type()) {
        case nlohmann::json::value_t::boolean:
            v = j.get<Value::Bool>();
            break;
        case nlohmann::json::value_t::number_integer:
        case nlohmann::json::value_t::number_unsigned:
            v = j.get<Value::Integer>();
            break;
        case nlohmann::json::value_t::number_float:
            v = j.get<Value::Float>();
            break;
        case nlohmann::json::value_t::string:
            v = j.get<Value::String>();
            break;
        case nlohmann::json::value_t::array:
            for (auto& elem : j) {
                v.push_back(from_json(elem));
            }
            break;
        case nlohmann::json::value_t::object:
            for (auto& [key, elem] : j.items()) {
                v[key] = from_json(elem);
            }
            break;
        default: break;
    }
    return v;
}
auto Value::from_toml(std::string_view toml_str) -> Value {
    auto t = toml::parse(toml_str);
    Value v{};
    for (auto& [key, node] : t) {
        v[key] = value_from_toml(node);
    }
    return v;
}

}
