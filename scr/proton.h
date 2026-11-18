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
    std::size_t keyBegin = 0;     // позиция ключа "LaunchOptions" (открывающая кавычка)
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

// Разбор значения WINEDLLOVERRIDES: "dll1,dll2=mode;dll3=mode".
struct OverrideEntry {
    std::string dlls;      // как есть, между ';'
    std::string mode;      // часть после '=' (может быть пустой, если '=' не было)
    bool hasEquals = false;
};

inline std::vector<OverrideEntry> ParseOverrideEntries(const std::string& value) {
    std::vector<OverrideEntry> entries;
    std::size_t start = 0;
    while (start <= value.size()) {
        std::size_t semi = value.find(';', start);
        if (semi == std::string::npos) semi = value.size();
        const std::string entry = value.substr(start, semi - start);
        OverrideEntry e;
        const std::size_t eq = entry.find('=');
        if (eq != std::string::npos) {
            e.dlls = entry.substr(0, eq);
            e.mode = entry.substr(eq + 1);
            e.hasEquals = true;
        } else {
            e.dlls = entry;
        }
        if (!e.dlls.empty() || e.hasEquals) entries.push_back(e);
        start = semi + 1;
    }
    return entries;
}

// Есть ли в списке dlls (через запятую) ровно dwmapi/dwmapi.dll.
inline bool DllsContainDwmapi(const std::string& dlls) {
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
    return false;
}

// Убирает dwmapi из списка dlls. Возвращает false, если в списке остались только
// пробелы/запятые, — тогда вызывающий удаляет всю запись.
inline bool RemoveDwmapiFromDlls(std::string& dlls) {
    std::string out;
    std::size_t ds = 0;
    while (ds <= dlls.size()) {
        std::size_t comma = dlls.find(',', ds);
        if (comma == std::string::npos) comma = dlls.size();
        std::string dll = dlls.substr(ds, comma - ds);
        std::string trimmed = dll;
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) trimmed.pop_back();
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(trimmed.begin());
        if (trimmed != "dwmapi" && trimmed != "dwmapi.dll") {
            if (!out.empty()) out.push_back(',');
            out += dll;
        }
        ds = comma + 1;
    }
    dlls = out;
    return !dlls.empty();
}

// Находит значение WINEDLLOVERRIDES=<value> в строке параметров. Возвращает позицию
// начала value и его длину (без кавычек, если они были), либо npos.
struct OverrideSpan {
    std::size_t begin = std::string::npos;  // позиция value (после кавычки, если была)
    std::size_t end = std::string::npos;    // конец value (до кавычки, если была)
    bool quoted = false;
    char quote = '\0';
    std::size_t prefixBegin = std::string::npos; // позиция "WINEDLLOVERRIDES="
};

inline OverrideSpan FindOverrideSpan(const std::string& options) {
    OverrideSpan span;
    const std::string lower = Lower(options);
    std::size_t pos = 0;
    while ((pos = lower.find("winedlloverrides=", pos)) != std::string::npos) {
        std::size_t i = pos + 17;
        span.prefixBegin = pos;
        if (i < options.size() && (options[i] == '"' || options[i] == '\'')) {
            const char quote = options[i];
            const std::size_t end = options.find(quote, i + 1);
            if (end != std::string::npos) {
                span.begin = i + 1;
                span.end = end;
                span.quoted = true;
                span.quote = quote;
                return span;
            }
        } else {
            std::size_t end = i;
            while (end < options.size() && !std::isspace(static_cast<unsigned char>(options[end]))) ++end;
            span.begin = i;
            span.end = end;
            span.quoted = false;
            return span;
        }
        pos += 17;
    }
    return span;
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
    std::size_t keyBegin = 0;

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
                keyBegin = t.begin;
                expectKey = false;
            } else {
                if (atTarget() && detail::IEquals(key, "LaunchOptions") && !res.hasOptions) {
                    res.hasOptions = true;
                    res.options = Unescape(t.text);
                    res.valueBegin = t.begin;
                    res.valueEnd = t.end;
                    res.keyBegin = keyBegin;
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
    const detail::OverrideSpan span = detail::FindOverrideSpan(options);
    if (span.begin == std::string::npos) return false;
    const std::string value = options.substr(span.begin, span.end - span.begin);
    for (const auto& e : detail::ParseOverrideEntries(value)) {
        if (!e.hasEquals || e.mode.empty() || e.mode[0] != 'n') continue;
        if (detail::DllsContainDwmapi(e.dlls)) return true;
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

// Убирает dwmapi=n,b из строки параметров запуска. Всё, что не относилось к dwmapi,
// сохраняется. Если WINEDLLOVERRIDES становится пустым — убирается целиком; если в
// итоге не остаётся ничего, кроме %command%, — возвращается пустая строка.
inline std::string RemoveOverride(const std::string& current) {
    std::string cur = current;
    const detail::OverrideSpan span = detail::FindOverrideSpan(cur);
    if (span.begin == std::string::npos) {
        // dwmapi нет; но если остался только %command% — чистим и его.
        std::string trimmed = cur;
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) trimmed.pop_back();
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(trimmed.begin());
        return trimmed == "%command%" ? std::string() : cur;
    }

    const std::string value = cur.substr(span.begin, span.end - span.begin);
    std::vector<detail::OverrideEntry> entries = detail::ParseOverrideEntries(value);
    std::vector<std::string> rebuilt;
    for (auto& e : entries) {
        if (!e.hasEquals || e.mode.empty() || e.mode[0] != 'n' ||
            !detail::DllsContainDwmapi(e.dlls)) {
            rebuilt.push_back(e.hasEquals ? e.dlls + "=" + e.mode : e.dlls);
            continue;
        }
        std::string dlls = e.dlls;
        if (detail::RemoveDwmapiFromDlls(dlls)) {
            rebuilt.push_back(dlls + "=" + e.mode);
        }
        // иначе запись состояла только из dwmapi — выбрасываем её целиком
    }

    std::string newValue;
    for (std::size_t i = 0; i < rebuilt.size(); ++i) {
        if (i) newValue.push_back(';');
        newValue += rebuilt[i];
    }

    // Заменяем участок от "WINEDLLOVERRIDES=" до конца значения.
    std::string replacement;
    if (!newValue.empty()) {
        replacement = "WINEDLLOVERRIDES=";
        if (span.quoted) replacement += std::string(1, span.quote) + newValue + std::string(1, span.quote);
        else replacement = "WINEDLLOVERRIDES=" + newValue;
    }

    std::size_t valueEndIncludingQuotes = span.end;
    if (span.quoted) ++valueEndIncludingQuotes;   // закрывающая кавычка
    std::string result = cur.substr(0, span.prefixBegin) + replacement + cur.substr(valueEndIncludingQuotes);

    // Подчищаем двойные пробелы на стыке.
    std::string cleaned;
    cleaned.reserve(result.size());
    bool prevSpace = false;
    for (char ch : result) {
        const bool isSpace = std::isspace(static_cast<unsigned char>(ch));
        if (isSpace) {
            if (!prevSpace) cleaned.push_back(ch);
        } else {
            cleaned.push_back(ch);
        }
        prevSpace = isSpace;
    }
    while (!cleaned.empty() && std::isspace(static_cast<unsigned char>(cleaned.back()))) cleaned.pop_back();
    while (!cleaned.empty() && std::isspace(static_cast<unsigned char>(cleaned.front()))) cleaned.erase(cleaned.begin());

    return cleaned == "%command%" ? std::string() : cleaned;
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

// Текст localconfig.vdf без dwmapi в параметрах запуска приложения.
// nullopt — блока приложения нет. Если после очистки параметров не осталось —
// строка LaunchOptions удаляется целиком, а не остаётся пустой.
inline std::optional<std::string> WithoutOverride(const std::string& vdf,
                                                  const std::string& appId = kAppId) {
    const AppOptions app = FindApp(vdf, appId);
    if (!app.appFound) return std::nullopt;
    if (!app.hasOptions) return vdf;                 // параметров запуска нет — нечего чистить

    const std::string cleaned = RemoveOverride(app.options);
    if (cleaned == app.options) return vdf;          // dwmapi и так не было

    if (cleaned.empty()) {
        // Строка LaunchOptions целиком удаляется вместе с переводом строки и отступом.
        std::size_t lineStart = vdf.rfind('\n', app.keyBegin);
        lineStart = lineStart == std::string::npos ? 0 : lineStart + 1;

        // Конец строки значения: valueEnd указывает на закрывающую кавычку, ищем перевод строки.
        std::size_t lineEnd = vdf.find('\n', app.valueEnd);
        if (lineEnd == std::string::npos) lineEnd = vdf.size();
        else ++lineEnd;                              // вместе с '\n'

        return vdf.substr(0, lineStart) + vdf.substr(lineEnd);
    }

    const std::string value = "\"" + Escape(cleaned) + "\"";
    return vdf.substr(0, app.valueBegin) + value + vdf.substr(app.valueEnd);
}

} // namespace proton