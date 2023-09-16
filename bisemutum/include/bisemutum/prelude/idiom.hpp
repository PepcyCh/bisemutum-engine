#pragma once

#include <type_traits>
#include <compare>

namespace bi {

struct MoveOnly {
    MoveOnly() noexcept = default;

    MoveOnly(MoveOnly&& rhs) noexcept = default;
    MoveOnly(MoveOnly const& rhs) noexcept = delete;
    auto operator=(MoveOnly&& rhs) noexcept -> MoveOnly& = default;
    auto operator=(MoveOnly const& rhs) noexcept -> MoveOnly& = delete;

    auto operator==(MoveOnly const& rhs) const noexcept -> bool = default;
    auto operator<=>(MoveOnly const& rhs) const noexcept = default;
};

template <typename T>
struct PImpl {
    template <typename... Args>
    PImpl(Args&&... args) : impl_(new typename T::Impl(std::forward<Args>(args)...)) {}

    ~PImpl() {
        if (impl_) {
            delete impl();
            impl_ = nullptr;
        }
    }

    PImpl(PImpl const& rhs) noexcept = delete;
    PImpl(PImpl&& rhs) noexcept {
        impl_ = rhs.impl_;
        rhs.impl_ = nullptr;
    }

    auto operator=(PImpl const& rhs) noexcept -> PImpl& = delete;
    auto operator=(PImpl&& rhs) noexcept -> PImpl& {
        if (impl_) { delete impl(); }
        impl_ = rhs.impl_;
        rhs.impl_ = nullptr;
        return *this;
    }

    auto operator==(PImpl const& rhs) const noexcept -> bool = default;
    auto operator<=>(PImpl const& rhs) const noexcept = default;

protected:
    auto impl() { return reinterpret_cast<typename T::Impl*>(impl_); }
    auto impl() const { return reinterpret_cast<typename T::Impl const*>(impl_); }

private:
    void* impl_ = nullptr;
};

}
