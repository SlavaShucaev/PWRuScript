#include "installer.h"
#include "utils.h"

#include <curl/curl.h>

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

// ---------- вспомогательные элементы для libcurl ----------
namespace {

size_t WriteFileCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    FILE* fp = static_cast<FILE*>(userdata);
    return std::fwrite(ptr, size, nmemb, fp);
}

std::string HumanSize(curl_off_t bytes) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1);
    if (bytes >= 1024LL * 1024 * 1024) {
        os << (double)bytes / (1024.0 * 1024.0 * 1024.0) << " GiB";
    } else if (bytes >= 1024LL * 1024) {
        os << (double)bytes / (1024.0 * 1024.0) << " MiB";
    } else if (bytes >= 1024LL) {
        os << (double)bytes / 1024.0 << " KiB";
    } else {
        os << (long long)bytes << " B";
    }
    return os.str();
}

void DrawProgress(const std::string& label,
                  curl_off_t dltotal, curl_off_t dlnow) {
    std::ostringstream os;
    os << "\r  [" << label << "] ";

    if (dltotal > 0) {
        const int width = 30;
        double ratio = (double)dlnow / (double)dltotal;
        if (ratio > 1.0) ratio = 1.0;

        int filled = (int)(ratio * width);
        os << '[';
        for (int i = 0; i < width; ++i) {
            os << (i < filled ? '#' : '-');
        }
        os << "] "
           << std::setw(3) << (int)(ratio * 100.0) << "%  "
           << HumanSize(dlnow) << " / " << HumanSize(dltotal);
    } else {
        os << HumanSize(dlnow) << " (размер неизвестен)";
    }

    std::string s = os.str();
    if (s.size() < 100) s.append(100 - s.size(), ' ');

    std::cerr << s << std::flush;
}

void ClearProgress() {
    std::cerr << '\r' << std::string(100, ' ') << '\r' << std::flush;
}

int ProgressCallback(void* clientp,
                     curl_off_t dltotal,
                     curl_off_t dlnow,
                     curl_off_t /*ultotal*/,
                     curl_off_t /*ulnow*/) {
    const std::string* label = static_cast<const std::string*>(clientp);
    DrawProgress(label ? *label : std::string("Загрузка"), dltotal, dlnow);
    return 0;
}

constexpr const char* kGithubReleaseBase =
    "https://github.com/RikudoSmpai/PWRuLauncher/releases/latest/download/";

} // namespace

// ---------------------------------------------------------------
// Конструктор
// ---------------------------------------------------------------
Installer::Installer(
    const fs::path& contentDir,
    const fs::path& voicePak,
    const fs::path& subtitlePak
    )
    : contentDir_(contentDir),
    voicePak_(voicePak),
    subtitlePak_(subtitlePak)
{
}

// ---------------------------------------------------------------
// Проверки существующих файлов
// ---------------------------------------------------------------

bool Installer::CheckFile(const fs::path& p) const {
    std::error_code ec;
    if (!fs::exists(p, ec) || ec) return false;
    if (!fs::is_regular_file(p, ec) || ec) return false;

    utils::Print("Найден уже установленный файл:\n" + p.string(),
                 utils::Status::Warning);
    return true;
}

bool Installer::CheckDirectory(const fs::path& p) const {
    std::error_code ec;
    if (!fs::exists(p, ec) || ec) return false;
    if (!fs::is_directory(p, ec) || ec) return false;

    utils::Print("Найдена уже установленная папка:\n" + p.string(),
                 utils::Status::Warning);
    return true;
}

bool Installer::CheckExistingInstallation(bool needPause) const {
    const fs::path paksMods = contentDir_ / "Content" / "Paks" / "~mods";
    const fs::path binWin64 = contentDir_ / "Binaries" / "Win64";

    bool found = false;

    found |= CheckFile(paksMods / "PW_RU_Voice.pak");
    found |= CheckFile(paksMods / "PW_RU_Subs.pak");
    found |= CheckFile(binWin64 / "dwmapi.dll");
    found |= CheckFile(binWin64 / "UE4SS.dll");
    found |= CheckFile(binWin64 / "UE4SS-settings.ini");
    found |= CheckDirectory(binWin64 / "Mods");

    if (!found) {
        utils::Print("Ранее установленных компонентов мода не обнаружено.",
                     utils::Status::Success);
        return true;
    }

    utils::Print(
        "Обнаружены ранее установленные компоненты мода.\n"
        "Установка перезапишет или сконфликтует с ними.\n"
        "Проверьте перечисленные выше пути.",
        utils::Status::Warning);

    if (!needPause) {
        return true;
    }

    const bool allow = utils::Confirm(
        "Продолжить и перезаписать существующие файлы?"
        );

    if (!allow) {
        throw UserCancelled("Пользователь отказался от перезаписи.");
    }

    return true;
}

// ---------------------------------------------------------------
// Установка из локального источника
// ---------------------------------------------------------------

void Installer::InstallVoice() const {
    InstallPak(voicePak_, "PW_RU_Voice.pak");
}

void Installer::InstallSubtitles() const {
    InstallPak(subtitlePak_, "PW_RU_Subs.pak");
}

void Installer::InstallPak(
    const fs::path& sourcePak,
    const fs::path& destinationName
    ) const {
    if (!fs::is_regular_file(sourcePak)) {
        throw std::runtime_error(
            "Исходный файл не найден: " + sourcePak.string()
            );
    }

    if (sourcePak.extension() != ".pak") {
        throw std::runtime_error(
            "Ожидался файл .pak: " + sourcePak.string()
            );
    }

    const fs::path paksDir = contentDir_ / "Content" / "Paks";
    if (!fs::is_directory(paksDir)) {
        throw std::runtime_error(
            "Не найдена папка Paks: " + paksDir.string()
            );
    }

    const fs::path modsDir     = paksDir / "~mods";
    const fs::path destination = modsDir / destinationName;

    if (fs::exists(destination)) {
        throw std::runtime_error(
            "Файл уже существует, перезапись отменена: "
            + destination.string()
            );
    }

    fs::create_directories(modsDir);
    fs::copy_file(sourcePak, destination);
}

// ---------------------------------------------------------------
// Скачивание с GitHub (с прогресс-баром)
// ---------------------------------------------------------------

bool Installer::DownloadFile(const std::string& url,
                             const fs::path& destFile,
                             const std::string& label) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        utils::Print("Не удалось инициализировать libcurl.",
                     utils::Status::Error);
        return false;
    }

    FILE* fp = std::fopen(destFile.string().c_str(), "wb");
    if (!fp) {
        utils::Print("Не удалось открыть файл для записи: " + destFile.string(),
                     utils::Status::Error);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PWRuLauncher-installer/1.0");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &label);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_easy_cleanup(curl);
    std::fclose(fp);

    ClearProgress();

    if (res != CURLE_OK) {
        if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST) {
            utils::Print(
                "Не удалось подключиться к GitHub.\n"
                "Проверьте интернет-соединение.\n"
                "URL: " + url,
                utils::Status::Error);
        } else {
            utils::Print(
                std::string("Ошибка загрузки: ") + curl_easy_strerror(res) +
                    "\nURL: " + url,
                utils::Status::Error);
        }
        std::error_code ignore;
        fs::remove(destFile, ignore);
        return false;
    }

    if (httpCode >= 400) {
        utils::Print(
            "Сервер GitHub вернул HTTP " + std::to_string(httpCode) +
                ".\nФайл не найден или недоступен: " + url,
            utils::Status::Error);
        std::error_code ignore;
        fs::remove(destFile, ignore);
        return false;
    }

    std::error_code ec;
    auto sz = fs::file_size(destFile, ec);
    if (ec || sz == 0) {
        utils::Print("Скачанный файл пуст: " + destFile.string(),
                     utils::Status::Error);
        std::error_code ignore;
        fs::remove(destFile, ignore);
        return false;
    }

    utils::Print("Скачано [" + label + "]: " + destFile.filename().string() +
                     " (" + HumanSize((curl_off_t)sz) + ")",
                 utils::Status::Success);
    return true;
}

bool Installer::DownloadAndInstallAll() const {
    utils::Print("Скачивание файлов локализации с GitHub...",
                 utils::Status::Info);

    const fs::path paksMods = contentDir_ / "Content" / "Paks" / "~mods";
    std::error_code ec;
    fs::create_directories(paksMods, ec);
    if (ec) {
        utils::Print("Не удалось создать папку: " + paksMods.string(),
                     utils::Status::Error);
        return false;
    }

    const fs::path tmpVoice = paksMods / "PW_RU_Voice.pak.download";
    const fs::path tmpSubs  = paksMods / "PW_RU_Subs.pak.download";

    const std::string voiceUrl =
        std::string(kGithubReleaseBase) + "PW_RU_Voice.pak";
    if (!DownloadFile(voiceUrl, tmpVoice, "Озвучка")) {
        utils::Print("Не удалось скачать файл озвучки.",
                     utils::Status::Error);
        return false;
    }

    const std::string subsUrl =
        std::string(kGithubReleaseBase) + "PW_RU_Subs.pak";
    if (!DownloadFile(subsUrl, tmpSubs, "Субтитры")) {
        utils::Print("Не удалось скачать файл субтитров.",
                     utils::Status::Error);
        std::error_code ignore;
        fs::remove(tmpVoice, ignore);
        return false;
    }

    const fs::path finalVoice = paksMods / "PW_RU_Voice.pak";
    const fs::path finalSubs  = paksMods / "PW_RU_Subs.pak";

    try {
        if (fs::exists(finalVoice)) {
            utils::Print("Перезапись существующего файла: " +
                             finalVoice.string(),
                         utils::Status::Warning);
            fs::remove(finalVoice);
        }
        fs::rename(tmpVoice, finalVoice);

        if (fs::exists(finalSubs)) {
            utils::Print("Перезапись существующего файла: " +
                             finalSubs.string(),
                         utils::Status::Warning);
            fs::remove(finalSubs);
        }
        fs::rename(tmpSubs, finalSubs);
    } catch (const std::exception& e) {
        utils::Print(std::string("Не удалось установить файл: ") + e.what(),
                     utils::Status::Error);
        std::error_code ignore;
        fs::remove(tmpVoice, ignore);
        fs::remove(tmpSubs, ignore);
        return false;
    }

    utils::Print("Озвучка и субтитры успешно установлены.",
                 utils::Status::Success);
    return true;
}