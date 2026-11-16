#pragma once

// SHA-256 (FIPS 180-4) без внешних библиотек: проверка скачанных файлов
// по суммам из релиза (PW_RU_mods.sha256, формат sha256sum).

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace sha256 {

class Hasher {
public:
    Hasher() { Reset(); }

    void Reset() {
        state_ = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                  0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
        bitLen_ = 0;
        bufLen_ = 0;
    }

    void Update(const void* data, std::size_t len) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        bitLen_ += static_cast<std::uint64_t>(len) * 8u;
        while (len > 0) {
            std::size_t n = sizeof(buf_) - bufLen_;
            if (n > len) n = len;
            std::memcpy(buf_ + bufLen_, p, n);
            bufLen_ += n;
            p += n;
            len -= n;
            if (bufLen_ == sizeof(buf_)) {
                Transform(buf_);
                bufLen_ = 0;
            }
        }
    }

    std::array<std::uint8_t, 32> Digest() {
        const std::uint64_t bits = bitLen_;
        buf_[bufLen_++] = 0x80;
        if (bufLen_ > 56) {
            while (bufLen_ < 64) buf_[bufLen_++] = 0;
            Transform(buf_);
            bufLen_ = 0;
        }
        while (bufLen_ < 56) buf_[bufLen_++] = 0;
        for (int i = 7; i >= 0; --i) {
            buf_[bufLen_++] = static_cast<std::uint8_t>(bits >> (i * 8));
        }
        Transform(buf_);

        std::array<std::uint8_t, 32> out{};
        for (int i = 0; i < 8; ++i) {
            out[i * 4]     = static_cast<std::uint8_t>(state_[i] >> 24);
            out[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
            out[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
            out[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
        }
        Reset();
        return out;
    }

private:
    static std::uint32_t Rotr(std::uint32_t x, int n) {
        return (x >> n) | (x << (32 - n));
    }

    void Transform(const std::uint8_t* block) {
        static const std::uint32_t k[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                   (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                   static_cast<std::uint32_t>(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = h + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{};
    std::uint64_t bitLen_ = 0;
    std::uint8_t buf_[64] = {};
    std::size_t bufLen_ = 0;
};

inline std::string ToHex(const std::array<std::uint8_t, 32>& digest) {
    static const char* kHex = "0123456789abcdef";
    std::string s;
    s.reserve(64);
    for (std::uint8_t byte : digest) {
        s.push_back(kHex[byte >> 4]);
        s.push_back(kHex[byte & 15]);
    }
    return s;
}

inline std::string OfString(const std::string& data) {
    Hasher h;
    h.Update(data.data(), data.size());
    return ToHex(h.Digest());
}

// Сумма файла; пустая строка — файл не прочитан.
inline std::string OfFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    Hasher h;
    std::vector<char> buf(1 << 20);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const std::streamsize n = in.gcount();
        if (n > 0) h.Update(buf.data(), static_cast<std::size_t>(n));
    }
    if (in.bad()) return {};
    return ToHex(h.Digest());
}

// Файл в формате sha256sum: «<64 hex>  <имя>» или «<64 hex> *<имя>», по строке на файл.
// Возвращает имя -> сумма (hex в нижнем регистре); строки другого вида пропускаются.
inline std::map<std::string, std::string> ParseSums(const std::string& text) {
    std::map<std::string, std::string> sums;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;

        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line.size() < 66 || line[0] == '#') continue;

        bool hex = true;
        std::string sum = line.substr(0, 64);
        for (char& ch : sum) {
            if (!std::isxdigit(static_cast<unsigned char>(ch))) { hex = false; break; }
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (!hex || (line[64] != ' ' && line[64] != '\t')) continue;

        std::size_t nameStart = 65;
        if (nameStart < line.size() && (line[nameStart] == ' ' || line[nameStart] == '*')) ++nameStart;
        const std::string name = line.substr(nameStart);
        if (!name.empty()) sums[name] = sum;
    }
    return sums;
}

} // namespace sha256
