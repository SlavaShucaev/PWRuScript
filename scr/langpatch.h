#pragma once

// Старые версии скрипта меняли в exe игры строку en-US на ru-RU (UTF-16LE) и клали рядом
// резервную копию ProjectWingman-Win64-Shipping.exe.bak-pwlang. Это больше не нужно:
// язык держит UE4SS (под Proton — с WINEDLLOVERRIDES="dwmapi=n,b"), а exe игры мы не меняем.
// Копию возвращаем, только если текущий exe отличается от неё ровно этой заменой —
// если игру с тех пор обновляли, exe не трогаем.

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>

namespace langpatch {

// Замена en-US -> ru-RU (UTF-16LE) во всём буфере; возвращает число замен.
inline std::size_t Apply(std::vector<char>& buffer) {
    static const char target[]      = {'e', 0, 'n', 0, '-', 0, 'U', 0, 'S', 0};
    static const char replacement[] = {'r', 0, 'u', 0, '-', 0, 'R', 0, 'U', 0};
    std::size_t count = 0;
    auto it = buffer.begin();
    while ((it = std::search(it, buffer.end(), std::begin(target), std::end(target))) != buffer.end()) {
        std::copy(std::begin(replacement), std::end(replacement), it);
        std::advance(it, static_cast<std::ptrdiff_t>(sizeof(replacement)));
        ++count;
    }
    return count;
}

// current — это original с заменой en-US -> ru-RU, и больше ничем не отличается.
inline bool IsPatchedCopyOf(const std::vector<char>& original, const std::vector<char>& current) {
    if (original.size() != current.size()) return false;
    std::vector<char> copy = original;
    if (Apply(copy) == 0) return false;
    return copy == current;
}

} // namespace langpatch
