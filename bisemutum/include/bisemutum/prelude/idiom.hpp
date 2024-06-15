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
    PImpl(Args&&... args)
        : impl_(new typename T::Impl(std::forward<Args>(args)...))
        , impl_deletor_([](void* impl) { delete static_cast<typename T::Impl*>(impl); })
    {}

    ~PImpl() {
        if (impl_) {
            impl_deletor_(impl_);
            impl_ = nullptr;
        }
    }

    PImpl(PImpl const& rhs) noexcept = delete;
    PImpl(PImpl&& rhs) noexcept {
        impl_ = rhs.impl_;
        impl_deletor_ = rhs.impl_deletor_;
        rhs.impl_ = nullptr;
    }

    auto operator=(PImpl const& rhs) noexcept -> PImpl& = delete;
    auto operator=(PImpl&& rhs) noexcept -> PImpl& {
        if (impl_) { impl_deletor_(impl_); }
        impl_ = rhs.impl_;
        impl_deletor_ = rhs.impl_deletor_;
        rhs.impl_ = nullptr;
        return *this;
    }

    auto operator==(PImpl const& rhs) const noexcept -> bool = default;
    auto operator<=>(PImpl const& rhs) const noexcept = default;

protected:
    auto impl() { return static_cast<typename T::Impl*>(impl_); }
    auto impl() const { return static_cast<typename T::Impl const*>(impl_); }

private:
    void* impl_ = nullptr;
    using Impl_Deletor = auto (void*) -> void;
    Impl_Deletor* impl_deletor_ = nullptr;
};

}
