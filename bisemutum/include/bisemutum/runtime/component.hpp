#pragma once

#include "../utils/srefl.hpp"

namespace bi::rt {

template <typename T>
concept TComponent = requires () {
    { T::component_type_name } -> std::same_as<std::string_view const&>;
    srefl::is_reflectable<T>;
};

}
