#pragma once

#include "../utils/serde.hpp"

namespace bi::rt {

template <typename T>
concept TComponent = requires () {
    { T::type_name } -> std::same_as<std::string_view const&>;
    JsonSerdeable<T>;
};

}
