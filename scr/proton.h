#pragma once

// Параметр запуска Steam для Proton. UE4SS (русский язык при каждом запуске, перевод
// текста мимо субтитров, титры локализации) грузится через dwmapi.dll рядом с exe игры,
// а Proton по умолчанию берёт свою встроенную dwmapi — нужен
//     WINEDLLOVERRIDES="dwmapi=n,b" %command%
// Здесь только логика над текстом localconfig.vdf (без файлов), чтобы её проверяли тесты.

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace proton {

constexpr const char* kAppId = "895870";              // Project Wingman в Steam
constexpr const char* kLaunchLine = "WINEDLLOVERRIDES=\"dwmapi=n,b\" %command%";

struct AppOptions {
    bool appFound = false;        // в apps есть блок приложения
    bool hasOptions = false;      // у блока есть LaunchOptions
    std::string options;          // значение LaunchOptions (раскодированное)
    std::size_t blockOpen = 0;    // позиция '{' блока приложения
    std::size_t valueBegin = 0;   // значение LaunchOptions вместе с кавычками: [valueBegin, valueEnd)
    std::size_t valueEnd = 0;
};

namespace detail {

struct Token {
    enum Kind { Str, Open, Close } kind;
    std::string text;
    std::size_t begin;
    std::size_t end;
};

// Токены KeyValues (VDF): строки в кавычках (с \" и \\), '{', '}', слова без кавычек;
// комментарии // и условия [$...] пропускаются.
inline std::vector<Token> Tokenize(const std::string& s) {
    std::vector<Token> out;
    std::size_t i = 0;
    while (i < s.size()) {
        const char c = s[i];
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n') ++i;
            continue;
        }
        if (c == '{') { out.push_back({Token::Open, "{", i, i + 1}); ++i; continue; }
        if (c == '}') { out.push_back({Token::Close, "}", i, i + 1}); ++i; continue; }
        if (c == '[') {                       // условие вида [$WIN32] — не ключ и не значение
            while (i < s.size() && s[i] != ']' && s[i] != '\n') ++i;
            if (i < s.size() && s[i] == ']') ++i;
            continue;
        }
        if (c == '"') {
            const std::size_t begin = i++;
            std::string raw;
            while (i < s.size() && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < s.size()) { raw.push_back(s[i]); ++i; }
                raw.push_back(s[i]);
                ++i;
            }
            if (i < s.size()) ++i;            // закрывающая кавычка
            out.push_back({Token::Str, raw, begin, i});
            continue;
        }
        const std::size_t begin = i;
        while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i])) &&
               s[i] != '{' && s[i] != '}' && s[i] != '"') {
            ++i;
        }
        out.push_back({Token::Str, s.substr(begin, i - begin), begin, i});
    }
    return out;
}

inline bool IEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline std::string Lower(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

} // namespace detail

// \" -> ", \\ -> \, \n и \t — как в KeyValues.
inline std::string Unescape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            const char n = raw[++i];
            out.push_back(n == 'n' ? '\n' : n == 't' ? '\t' : n);
        } else {
            out.push_back(raw[i]);
        }
    }
    return out;
}

inline std::string Escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char ch : value) {
        if (ch == '"' || ch == '\\') out.push_back('\\');
        out.push_back(ch);
    }
    return out;
}

// Блок UserLocalConfigStore/Software/Valve/Steam/apps/<appId> и его LaunchOptions.
inline AppOptions FindApp(const std::string& vdf, const std::string& appId = kAppId) {
    const std::vector<std::string> target = {
        "UserLocalConfigStore", "Software", "Valve", "Steam", "apps", appId};
    AppOptions res;
    std::vector<std::string> stack;
    bool expectKey = true;
    std::string key;

    auto atTarget = [&]() {
        if (stack.size() != target.size()) return false;
        for (std::size_t i = 0; i < target.size(); ++i) {
            if (!detail::IEquals(stack[i], target[i])) return false;
        }
        return true;
    };

    for (const auto& t : detail::Tokenize(vdf)) {
        if (t.kind == detail::Token::Str) {
            if (expectKey) {
                key = t.text;
                expectKey = false;
            } else {
                if (atTarget() && detail::IEquals(key, "LaunchOptions") && !res.hasOptions) {
                    res.hasOptions = true;
                    res.options = Unescape(t.text);
                    res.valueBegin = t.begin;
                    res.valueEnd = t.end;
                }
                expectKey = true;
            }
        } else if (t.kind == detail::Token::Open) {
            stack.push_back(expectKey ? std::string() : key);
            expectKey = true;
            if (atTarget() && !res.appFound) {
                res.appFound = true;
                res.blockOpen = t.begin;
            }
        } else {
            if (!stack.empty()) stack.pop_back();
            expectKey = true;
        }
    }
    return res;
}

// Есть ли в параметрах запуска WINEDLLOVERRIDES с dwmapi в режиме native (n, n,b, native).
inline bool HasOverride(const std::string& options) {
    const std::string lower = detail::Lower(options);
    std::size_t pos = 0;
    while ((pos = lower.find("winedlloverrides=", pos)) != std::string::npos) {
        std::size_t i = pos + 17;
        std::string value;
        if (i < lower.size() && (lower[i] == '"' || lower[i] == '\'')) {
            const char quote = lower[i++];
            const std::size_t end = lower.find(quote, i);
            value = lower.substr(i, end == std::string::npos ? std::string::npos : end - i);
        } else {
            std::size_t end = i;
            while (end < lower.size() && !std::isspace(static_cast<unsigned char>(lower[end]))) ++end;
            value = lower.substr(i, end - i);
        }
        // value: "dll1,dll2=mode;dll3=mode"
        std::size_t start = 0;
        while (start <= value.size()) {
            std::size_t semi = value.find(';', start);
            if (semi == std::string::npos) semi = value.size();
            const std::string entry = value.substr(start, semi - start);
            const std::size_t eq = entry.find('=');
            if (eq != std::string::npos && eq + 1 < entry.size() && entry[eq + 1] == 'n') {
                const std::string dlls = entry.substr(0, eq);
                std::size_t ds = 0;
                while (ds <= dlls.size()) {
                    std::size_t comma = dlls.find(',', ds);
                    if (comma == std::string::npos) comma = dlls.size();
                    std::string dll = dlls.substr(ds, comma - ds);
                    while (!dll.empty() && std::isspace(static_cast<unsigned char>(dll.back()))) dll.pop_back();
                    while (!dll.empty() && std::isspace(static_cast<unsigned char>(dll.front()))) dll.erase(dll.begin());
                    if (dll == "dwmapi" || dll == "dwmapi.dll") return true;
                    ds = comma + 1;
                }
            }
            start = semi + 1;
        }
        pos += 17;
    }
    return false;
}

// Новое значение LaunchOptions: dwmapi=n,b добавляется, то, что игрок уже вписал, сохраняется.
inline std::string MergeOptions(const std::string& current) {
    std::string cur = current;
    while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.back()))) cur.pop_back();
    while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.front()))) cur.erase(cur.begin());

    if (cur.empty()) return kLaunchLine;
    if (HasOverride(cur)) return cur;

    const std::string lower = detail::Lower(cur);
    const std::size_t pos = lower.find("winedlloverrides=");
    if (pos != std::string::npos) {           // свои оверрайды уже есть — дописываем к ним
        std::size_t i = pos + 17;
        if (i < cur.size() && (cur[i] == '"' || cur[i] == '\'')) {
            const char quote = cur[i];
            const std::size_t end = cur.find(quote, i + 1);
            if (end != std::string::npos) {
                const bool empty = end == i + 1;
                return cur.substr(0, end) + (empty ? "dwmapi=n,b" : ";dwmapi=n,b") + cur.substr(end);
            }
        } else {
            std::size_t end = i;
            while (end < cur.size() && !std::isspace(static_cast<unsigned char>(cur[end]))) ++end;
            const bool empty = end == i;
            return cur.substr(0, end) + (empty ? "dwmapi=n,b" : ";dwmapi=n,b") + cur.substr(end);
        }
    }
    if (lower.find("%command%") != std::string::npos) {
        return std::string("WINEDLLOVERRIDES=\"dwmapi=n,b\" ") + cur;
    }
    // Без %command% Steam дописывает параметры к команде игры — оставляем их после %command%.
    return std::string(kLaunchLine) + " " + cur;
}

// Текст localconfig.vdf с параметром для приложения; nullopt — блока приложения нет
// (игру с этой учётки не запускали — тогда только подсказка).
inline std::optional<std::string> WithOverride(const std::string& vdf,
                                               const std::string& appId = kAppId) {
    const AppOptions app = FindApp(vdf, appId);
    if (!app.appFound) return std::nullopt;

    const std::string value = "\"" + Escape(MergeOptions(app.hasOptions ? app.options : "")) + "\"";
    if (app.hasOptions) {
        return vdf.substr(0, app.valueBegin) + value + vdf.substr(app.valueEnd);
    }

    // Новая строка сразу после '{' с отступом строки блока плюс таб, как пишет сам Steam.
    std::size_t lineStart = vdf.rfind('\n', app.blockOpen);
    lineStart = lineStart == std::string::npos ? 0 : lineStart + 1;
    std::string indent;
    for (std::size_t i = lineStart; i < app.blockOpen && (vdf[i] == '\t' || vdf[i] == ' '); ++i) {
        indent.push_back(vdf[i]);
    }
    const std::string eol = (app.blockOpen + 1 < vdf.size() && vdf[app.blockOpen + 1] == '\r') ? "\r\n" : "\n";
    const std::string line = eol + indent + "\t\"LaunchOptions\"\t\t" + value;
    return vdf.substr(0, app.blockOpen + 1) + line + vdf.substr(app.blockOpen + 1);
}

} // namespace proton
