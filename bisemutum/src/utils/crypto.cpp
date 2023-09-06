#include <bisemutum/utils/crypto.hpp>

extern "C" {
#include <md5.h>
#include <sha256.h>
}

namespace bi::crypto {

namespace {

static constexpr char lower_hex_map[17] = "0123456789abcdef";

auto bytes_as_hex_string(CSpan<std::byte> data) -> std::string {
    std::string res(data.size() * 2, '\0');
    for (size_t i = 0; i < data.size(); i++) {
        res[i * 2] = lower_hex_map[static_cast<size_t>(data[i]) >> 4];
        res[i * 2 + 1] = lower_hex_map[static_cast<size_t>(data[i]) & 0xf];
    }
    return res;
}

}

auto MD5::as_string() const -> std::string {
    return bytes_as_hex_string({data, size});
}

auto md5(CSpan<std::byte> in_data) -> MD5 {
    MD5_CTX ctx{};
    md5_init(&ctx);
    md5_update(&ctx, reinterpret_cast<BYTE const*>(in_data.data()), in_data.size());
    MD5 res{};
    md5_final(&ctx, reinterpret_cast<BYTE*>(res.data));
    return res;
}

auto SHA256::as_string() const -> std::string {
    return bytes_as_hex_string({data, size});
}

auto sha256(CSpan<std::byte> in_data) -> SHA256 {
    SHA256_CTX ctx{};
    sha256_init_one(&ctx);
    sha256_update(&ctx, reinterpret_cast<BYTE const*>(in_data.data()), in_data.size());
    SHA256 res{};
    sha256_final(&ctx, reinterpret_cast<BYTE*>(res.data));
    return res;
}

}
