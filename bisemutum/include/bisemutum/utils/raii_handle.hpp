#pragma once

#include <functional>

#include "../prelude/idiom.hpp"

namespace bi {

template <typename Manager, typename Handle>
struct RaiiHandle final : MoveOnly {
    RaiiHandle() noexcept = default;
    ~RaiiHandle() noexcept { reset(); }

    RaiiHandle(RaiiHandle&& rhs) noexcept {
        handle_ = rhs.handle_;
        unregister_ = std::move(rhs.unregister_);
        rhs.unregister_ = {};
    }
    auto operator=(RaiiHandle&& rhs) noexcept -> RaiiHandle& {
        reset();
        handle_ = rhs.handle_;
        unregister_ = std::move(rhs.unregister_);
        rhs.unregister_ = {};
        return *this;
    }

    auto reset() noexcept -> void {
        if (unregister_) {
            unregister_(handle_);
            unregister_ = {};
        }
    }

private:
    friend Manager;
    RaiiHandle(Handle handle, std::function<auto(Handle) -> void> unregister)
        : handle_(handle), unregister_(std::move(unregister)) {}
    Handle handle_;
    std::function<auto(Handle) -> void> unregister_;
};

}
