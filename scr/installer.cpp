#include "installer.h"
#include "langpatch.h"
#include "proton.h"
#include "sha256.h"
#include "utils.h"

#include <archive.h>
#include <archive_entry.h>
#include <curl/curl.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------- вспомогательные элементы для libcurl ----------
namespace {

// Фиксированная ширина строки прогресс-бара, чтобы \r-перерисовка не оставляла хвостов.
constexpr std::size_t kProgressLineWidth = 100;

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
    if (s.size() < kProgressLineWidth) s.append(kProgressLineWidth - s.size(), ' ');

    std::cerr << s << std::flush;
}

void ClearProgress() {
    std::cerr << '\r' << std::string(kProgressLineWidth, ' ') << '\r' << std::flush;
}

int ProgressCallback(void* clientp,
                     curl_off_t dltotal, curl_off_t dlnow,
                     curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    const std::string* label = static_cast<const std::string*>(clientp);
    DrawProgress(label ? *label : std::string("Загрузка"), dltotal, dlnow);
    return 0;
}

constexpr const char* kGithubReleaseBase =
    "https://github.com/RikudoSmpai/PWRuLauncher/releases/latest/download/";

// Суммы модов в релизе (формат sha256sum), есть начиная с 1.0.6.
constexpr const char* kSumsAsset = "PW_RU_mods.sha256";

constexpr const char* kShippingExe = "ProjectWingman-Win64-Shipping.exe";

std::vector<char> ReadAllBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::vector<char>((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
}

std::string ReadAllText(const fs::path& path) {
    const std::vector<char> bytes = ReadAllBytes(path);
    return std::string(bytes.begin(), bytes.end());
}

// Запись через временный файл и переименование: при сбое старый файл остаётся целым.
bool WriteAtomically(const fs::path& path, const char* data, std::size_t size) {
    const fs::path tmp = path.string() + ".pwru-tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(data, static_cast<std::streamsize>(size));
        if (!out) {
            out.close();
            utils::RemoveQuietly(tmp);
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        utils::RemoveQuietly(tmp);
        return false;
    }
    return true;
}

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

bool Installer::CheckExistingInstallation(bool needPause) const {
    const fs::path paksMods = contentDir_ / "Content" / "Paks" / "~mods";
    const fs::path binWin64 = contentDir_ / "Binaries" / "Win64";

    bool found = false;
    found |= utils::CheckPath(paksMods / "PW_RU_Voice.pak",   false, "файл");
    found |= utils::CheckPath(paksMods / "PW_RU_Subs.pak",    false, "файл");
    found |= utils::CheckPath(binWin64 / "dwmapi.dll",        false, "файл");
    found |= utils::CheckPath(binWin64 / "UE4SS.dll",         false, "файл");
    found |= utils::CheckPath(binWin64 / "UE4SS-settings.ini", false, "файл");
    found |= utils::CheckPath(binWin64 / "Mods",              true,  "папка");

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

    if (needPause &&
        !utils::Confirm("Продолжить и перезаписать существующие файлы?")) {
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
            "Исходный файл не найден: " + sourcePak.string());
    }
    if (sourcePak.extension() != ".pak") {
        throw std::runtime_error(
            "Ожидался файл .pak: " + sourcePak.string());
    }

    const fs::path paksDir = contentDir_ / "Content" / "Paks";
    if (!fs::is_directory(paksDir)) {
        throw std::runtime_error(
            "Не найдена папка Paks: " + paksDir.string());
    }

    const fs::path modsDir     = paksDir / "~mods";
    const fs::path destination = modsDir / destinationName;

    if (fs::exists(destination)) {
        throw std::runtime_error(
            "Файл уже существует, перезапись отменена: "
            + destination.string());
    }

    fs::create_directories(modsDir);
    fs::copy_file(sourcePak, destination);
}

// ---------------------------------------------------------------
// Скачивание с GitHub (с прогресс-баром)
// ---------------------------------------------------------------

bool Installer::DownloadFile(const std::string& url,
                             const fs::path& destFile,
                             const std::string& label,
                             bool quiet) const {
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
    // Без общего лимита времени: на медленном канале озвучка качается дольше 10 минут.
    // Обрываем, только если минуту подряд скорость ниже 1 КБ/с.
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PWRuLauncher-installer/1.0");
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, quiet ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &label);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    std::fclose(fp);
    if (!quiet) ClearProgress();

    if (res != CURLE_OK) {
        const std::string reason =
            (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST)
                ? "Не удалось подключиться к GitHub.\nПроверьте интернет-соединение."
                : std::string("Ошибка загрузки: ") + curl_easy_strerror(res);
        if (!quiet) utils::Print(reason + "\nURL: " + url, utils::Status::Error);
        utils::RemoveQuietly(destFile);
        return false;
    }

    if (httpCode >= 400) {
        if (!quiet) {
            utils::Print(
                "Сервер GitHub вернул HTTP " + std::to_string(httpCode) +
                    ".\nФайл не найден или недоступен: " + url,
                utils::Status::Error);
        }
        utils::RemoveQuietly(destFile);
        return false;
    }

    std::error_code ec;
    auto sz = fs::file_size(destFile, ec);
    if (ec || sz == 0) {
        if (!quiet) {
            utils::Print("Скачанный файл пуст: " + destFile.string(),
                         utils::Status::Error);
        }
        utils::RemoveQuietly(destFile);
        return false;
    }

    if (!quiet) {
        utils::Print("Скачано [" + label + "]: " + destFile.filename().string() +
                         " (" + HumanSize((curl_off_t)sz) + ")",
                     utils::Status::Success);
    }
    return true;
}

// ---------------------------------------------------------------
// Контрольные суммы
// ---------------------------------------------------------------

void Installer::LoadChecksums() {
    sums_.clear();
    std::error_code ec;
    fs::create_directories(contentDir_, ec);
    const fs::path tmp = contentDir_ / ".pwru_sums.tmp";
    if (DownloadFile(std::string(kGithubReleaseBase) + kSumsAsset, tmp, "суммы", /*quiet=*/true)) {
        sums_ = sha256::ParseSums(ReadAllText(tmp));
    }
    utils::RemoveQuietly(tmp);

    if (sums_.empty()) {
        utils::Print(
            "В релизе нет файла контрольных сумм (" + std::string(kSumsAsset) + ").\n"
                                                                                "Проверка скачанных файлов будет пропущена.",
            utils::Status::Warning);
    } else {
        utils::Print("Контрольные суммы релиза получены: файлов " +
                         std::to_string(sums_.size()) + ".",
                     utils::Status::Success);
    }
}

bool Installer::VerifyChecksum(const fs::path& file,
                               const std::string& assetName) const {
    if (sums_.empty()) return true;                 // предупреждение уже было в LoadChecksums

    const auto it = sums_.find(assetName);
    if (it == sums_.end()) {
        utils::Print("Для " + assetName + " нет суммы в " + kSumsAsset +
                         " — файл не проверен.",
                     utils::Status::Warning);
        return true;
    }

    const std::string actual = sha256::OfFile(file);
    if (actual == it->second) {
        utils::Print("Контрольная сумма совпала: " + assetName, utils::Status::Success);
        return true;
    }

    utils::Print(
        "Контрольная сумма не совпала: " + assetName + "\n"
                                                       "ожидалась: " + it->second + "\n"
                           "получена:  " + (actual.empty() ? std::string("(файл не прочитан)") : actual) + "\n"
                                                                              "Файл повреждён при загрузке — запустите установку ещё раз.",
        utils::Status::Error);
    utils::RemoveQuietly(file);
    return false;
}

bool Installer::DownloadPakToMods(const std::string& fileName,
                                  const std::string& label) const {
    const fs::path paksMods = contentDir_ / "Content" / "Paks" / "~mods";
    std::error_code ec;
    fs::create_directories(paksMods, ec);
    if (ec) {
        utils::Print("Не удалось создать папку: " + paksMods.string(),
                     utils::Status::Error);
        return false;
    }

    const fs::path tmpFile   = paksMods / (fileName + ".download");
    const fs::path finalFile = paksMods / fileName;

    if (!DownloadFile(std::string(kGithubReleaseBase) + fileName,
                      tmpFile, label)) {
        return false;
    }

    if (!VerifyChecksum(tmpFile, fileName)) {
        return false;
    }

    if (fs::exists(finalFile)) {
        utils::Print("Перезапись существующего файла: " + finalFile.string(),
                     utils::Status::Warning);
        fs::remove(finalFile);
    }

    try {
        fs::rename(tmpFile, finalFile);
    } catch (const std::exception& e) {
        utils::Print(std::string("Не удалось установить файл: ") + e.what(),
                     utils::Status::Error);
        utils::RemoveQuietly(tmpFile);
        return false;
    }
    return true;
}

bool Installer::DownloadAndInstallAll() const {
    utils::Print("Скачивание файлов локализации с GitHub...",
                 utils::Status::Info);

    if (!DownloadPakToMods("PW_RU_Voice.pak", "Озвучка")) {
        utils::Print("Не удалось скачать файл озвучки.",
                     utils::Status::Error);
        return false;
    }

    if (!DownloadPakToMods("PW_RU_Subs.pak", "Субтитры")) {
        utils::Print("Не удалось скачать файл субтитров.",
                     utils::Status::Error);
        // Откатываем уже скачанную озвучку.
        utils::RemoveQuietly(contentDir_ / "Content" / "Paks" / "~mods"
                             / "PW_RU_Voice.pak");
        return false;
    }

    utils::Print("Озвучка и субтитры успешно установлены.",
                 utils::Status::Success);
    return true;
}

// ---------------------------------------------------------------
// UE4SS: распаковка zip
// ---------------------------------------------------------------

bool Installer::ExtractZip(const fs::path& zipPath,
                           const fs::path& destDir) const {
    struct archive* a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    if (archive_read_open_filename(a, zipPath.string().c_str(),
                                   10240) != ARCHIVE_OK) {
        utils::Print(
            "Не удалось открыть архив: " + zipPath.string() +
                "\n" + archive_error_string(a),
            utils::Status::Error);
        archive_read_free(a);
        return false;
    }

    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec) {
        utils::Print("Не удалось создать папку распаковки: " +
                         destDir.string(), utils::Status::Error);
        archive_read_free(a);
        return false;
    }

    const fs::path rootNorm = fs::weakly_canonical(destDir, ec);
    if (ec) {
        utils::Print("Не удалось нормализовать путь распаковки: " +
                         ec.message(), utils::Status::Error);
        archive_read_free(a);
        return false;
    }

    // Проверка, что target остаётся внутри rootNorm (защита от path traversal).
    // Сравниваем с разделителем: иначе «/tmp/root2» сошёл бы за «/tmp/root».
    const std::string rootStr = rootNorm.string();
    const std::string rootPrefix =
        rootStr + static_cast<char>(fs::path::preferred_separator);
    auto isInsideRoot = [&rootStr, &rootPrefix](const std::string& p) {
        return p == rootStr || p.rfind(rootPrefix, 0) == 0;
    };

    struct archive_entry* entry = nullptr;
    bool ok = true;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char* entryName = archive_entry_pathname(entry);
        if (!entryName) { ok = false; break; }

        const fs::path target = rootNorm / entryName;
        std::error_code pec;
        fs::path parentNorm = fs::weakly_canonical(target.parent_path(), pec);
        if (pec) {
            fs::create_directories(target.parent_path(), pec);
            parentNorm = fs::weakly_canonical(target.parent_path(), pec);
        }

        if (!isInsideRoot(parentNorm.string())) {
            utils::Print("Небезопасный путь в архиве: " +
                             std::string(entryName),
                         utils::Status::Error);
            ok = false;
            break;
        }

        if (archive_entry_filetype(entry) == AE_IFDIR) {
            fs::create_directories(target, ec);
            if (ec) { ok = false; break; }
            continue;
        }

        fs::create_directories(target.parent_path(), ec);
        if (ec) { ok = false; break; }

        std::ofstream out(target, std::ios::binary);
        if (!out) {
            utils::Print("Не удалось записать файл: " + target.string(),
                         utils::Status::Error);
            ok = false;
            break;
        }

        const void* buff = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;
        while (true) {
            int r = archive_read_data_block(a, &buff, &size, &offset);
            if (r == ARCHIVE_EOF) break;
            if (r != ARCHIVE_OK) {
                utils::Print(
                    "Ошибка чтения архива: " +
                        std::string(archive_error_string(a)),
                    utils::Status::Error);
                ok = false;
                break;
            }
            out.write(static_cast<const char*>(buff),
                      static_cast<std::streamsize>(size));
            if (!out) { ok = false; break; }
        }
        if (!ok) break;
    }

    archive_read_close(a);
    archive_read_free(a);

    if (!ok) {
        utils::Print("Распаковка архива завершилась с ошибкой.",
                     utils::Status::Error);
    }
    return ok;
}

// ---------------------------------------------------------------
// UE4SS: раскладка файлов по целям
// ---------------------------------------------------------------

bool Installer::CopyFileOrLog(const fs::path& src,
                              const fs::path& dst,
                              const std::string& description) const {
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        utils::Print("Не удалось установить " + description + ": " +
                         ec.message(),
                     utils::Status::Error);
        return false;
    }
    utils::Print("Установлен " + description, utils::Status::Success);
    return true;
}

bool Installer::DeployUE4SSFiles(const fs::path& extractedRoot) const {
    std::error_code ec;

    const fs::path moviesDst = contentDir_ / "Content" / "Movies";
    const fs::path win64Dst  = contentDir_ / "Binaries" / "Win64";

    // 1) Интро: сохраняем оригинал один раз, ставим новое.
    const fs::path introSrc  = extractedRoot / "Movies" / "HumbleIntro_PW.mp4";
    const fs::path introDst  = moviesDst / "HumbleIntro_PW.mp4";
    const fs::path introOrig = moviesDst / "HumbleIntro_PW.mp4.pwru-orig";

    if (!fs::is_regular_file(introSrc, ec)) {
        utils::Print("В архиве отсутствует Movies/HumbleIntro_PW.mp4",
                     utils::Status::Error);
        return false;
    }
    if (!fs::is_directory(moviesDst, ec)) {
        utils::Print("Не найдена папка Movies: " + moviesDst.string(),
                     utils::Status::Error);
        return false;
    }

    if (fs::exists(introDst, ec) && !fs::exists(introOrig, ec)) {
        fs::rename(introDst, introOrig, ec);
        if (ec) {
            utils::Print("Не удалось переименовать оригинальное интро: " +
                             ec.message(), utils::Status::Error);
            return false;
        }
        utils::Print("Оригинальное интро сохранено как " +
                         introOrig.filename().string(),
                     utils::Status::Info);
    }

    if (!CopyFileOrLog(introSrc, introDst, "интро")) return false;

    // 2) Win64: три обязательных файла.
    const fs::path win64Src = extractedRoot / "Win64";
    if (!fs::is_directory(win64Src, ec)) {
        utils::Print("В архиве отсутствует папка Win64",
                     utils::Status::Error);
        return false;
    }

    for (const char* name : {"dwmapi.dll", "UE4SS.dll", "UE4SS-settings.ini"}) {
        if (!CopyFileOrLog(win64Src / name, win64Dst / name, name)) {
            return false;
        }
    }

    // 3) Mods/: копируем целиком.
    const fs::path modsSrc = win64Src / "Mods";
    const fs::path modsDst = win64Dst / "Mods";
    if (!fs::is_directory(modsSrc, ec)) {
        utils::Print("В архиве отсутствует папка Win64/Mods",
                     utils::Status::Error);
        return false;
    }

    fs::create_directories(modsDst, ec);
    if (ec) {
        utils::Print("Не удалось создать Mods/: " + ec.message(),
                     utils::Status::Error);
        return false;
    }

    fs::copy(modsSrc, modsDst,
             fs::copy_options::recursive |
                 fs::copy_options::overwrite_existing, ec);
    if (ec) {
        utils::Print("Не удалось скопировать Mods/: " + ec.message(),
                     utils::Status::Error);
        return false;
    }
    utils::Print("Установлена папка Mods/", utils::Status::Success);

    return true;
}

bool Installer::DownloadAndInstallUE4SS() const {
    utils::Print("Скачивание UE4SS с GitHub...", utils::Status::Info);

    const fs::path tmpRoot = contentDir_ / ".pwru_tmp";
    const fs::path zipPath = tmpRoot / "PW_RU_UE4SS.zip";
    const fs::path unzDir  = tmpRoot / "ue4ss_extracted";

    std::error_code ec;
    fs::remove_all(tmpRoot, ec);
    fs::create_directories(tmpRoot, ec);
    if (ec) {
        utils::Print("Не удалось создать временную папку: " +
                         tmpRoot.string(), utils::Status::Error);
        return false;
    }

    // RAII-очистка временной папки при любом выходе.
    struct TmpCleanup {
        fs::path path;
        ~TmpCleanup() { std::error_code ignore; fs::remove_all(path, ignore); }
    } cleanup{tmpRoot};

    if (!DownloadFile(std::string(kGithubReleaseBase) + "PW_RU_UE4SS.zip",
                      zipPath, "UE4SS")) {
        utils::Print("Не удалось скачать архив UE4SS.",
                     utils::Status::Error);
        return false;
    }

    if (!VerifyChecksum(zipPath, "PW_RU_UE4SS.zip")) {
        return false;
    }

    if (!ExtractZip(zipPath, unzDir)) {
        utils::Print("Не удалось распаковать архив UE4SS.",
                     utils::Status::Error);
        return false;
    }

    if (!DeployUE4SSFiles(unzDir)) {
        utils::Print("Не удалось установить компоненты UE4SS.",
                     utils::Status::Error);
        return false;
    }

    utils::Print("UE4SS и интро успешно установлены.",
                 utils::Status::Success);
    return true;
}

// ---------------------------------------------------------------
// Оригинальный exe игры (правка языка в exe больше не нужна)
// ---------------------------------------------------------------

bool Installer::RestoreOriginalExe() const {
    const fs::path exePath = contentDir_ / "Binaries" / "Win64" / kShippingExe;
    const fs::path backupPath = exePath.string() + ".bak-pwlang";

    std::error_code ec;
    if (!fs::exists(backupPath, ec)) {
        return true;                                 // exe этим скриптом не менялся
    }

    utils::Print(
        "Найдена копия exe от прошлой версии скрипта (" + backupPath.filename().string() + ").\n"
                                                                                           "Править exe игры больше не нужно: русский язык держит UE4SS.",
        utils::Status::Info);

    const std::vector<char> original = ReadAllBytes(backupPath);
    const std::vector<char> current = ReadAllBytes(exePath);
    if (original.empty() || current.empty()) {
        utils::Print("Не удалось прочитать exe игры или его копию — exe не тронут.",
                     utils::Status::Error);
        return false;
    }

    if (original == current) {
        utils::RemoveQuietly(backupPath);
        utils::Print("exe игры и так оригинальный — лишняя копия удалена.",
                     utils::Status::Success);
        return true;
    }

    if (!langpatch::IsPatchedCopyOf(original, current)) {
        utils::Print(
            "exe игры отличается от копии не только правкой языка (игру обновляли?), поэтому его не трогаю.\n"
            "Чтобы вернуть оригинал: Steam → Project Wingman → Свойства → Установленные файлы →\n"
            "Проверить целостность файлов игры. После этого копию можно удалить:\n" +
                backupPath.string(),
            utils::Status::Warning);
        return false;
    }

    if (!WriteAtomically(exePath, original.data(), original.size())) {
        utils::Print("Не удалось записать оригинальный exe — он остался с правкой языка.\n"
                     "Проверьте целостность файлов игры в Steam.",
                     utils::Status::Error);
        return false;
    }
    utils::RemoveQuietly(backupPath);
    utils::Print("Оригинальный exe игры восстановлен.", utils::Status::Success);
    return true;
}

// ---------------------------------------------------------------
// Параметр запуска для Proton
// ---------------------------------------------------------------

void Installer::PrintLaunchOptionsHelp() {
    utils::Print(
        "Под Linux игра идёт через Proton, и UE4SS загрузится только с параметром запуска.\n"
        "Без него не будет русского языка при каждом запуске, перевода текста мимо субтитров\n"
        "и титров локализации. Впишите один раз:\n"
        "\n"
        "    " + std::string(proton::kLaunchLine) + "\n"
                                                 "\n"
                                                 "Steam: Библиотека → Project Wingman → Свойства → Общие → Параметры запуска.\n"
                                                 "Steam Deck (игровой режим): Project Wingman → ⚙ → Свойства → Параметры запуска.",
        utils::Status::Warning);
}

bool Installer::SetupLaunchOptions(const std::vector<fs::path>& steamRoots) const {
    // localconfig.vdf всех учёток Steam: <Steam>/userdata/<id>/config/localconfig.vdf
    std::vector<fs::path> configs;
    std::vector<std::string> seen;
    for (const auto& root : steamRoots) {
        std::error_code ec;
        const fs::path userdata = root / "userdata";
        if (!fs::is_directory(userdata, ec)) continue;
        for (const auto& entry : fs::directory_iterator(userdata, ec)) {
            const fs::path cfg = entry.path() / "config" / "localconfig.vdf";
            std::error_code fec;
            if (!fs::is_regular_file(cfg, fec)) continue;
            const std::string key = fs::weakly_canonical(cfg, fec).string();
            if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
            seen.push_back(key);
            configs.push_back(cfg);
        }
    }

    bool alreadySet = false;
    std::vector<fs::path> needFix;
    for (const auto& cfg : configs) {
        const proton::AppOptions app = proton::FindApp(ReadAllText(cfg));
        if (!app.appFound) continue;
        if (app.hasOptions && proton::HasOverride(app.options)) {
            alreadySet = true;
        } else {
            needFix.push_back(cfg);
        }
    }

    if (needFix.empty()) {
        if (alreadySet) {
            utils::Print("Параметр запуска для Proton уже стоит.", utils::Status::Success);
            return true;
        }
        // Настроек игры в Steam нет (не нашли Steam или игру ещё не запускали).
        PrintLaunchOptionsHelp();
        return false;
    }

    if (utils::IsProcessRunning("steam")) {
        utils::Print(
            "Steam сейчас запущен: вписать параметр запуска сам не могу —\n"
            "Steam перезапишет настройки при выходе.",
            utils::Status::Info);
        PrintLaunchOptionsHelp();
        return false;
    }

    if (!utils::Confirm(std::string("Вписать в настройки Steam параметр запуска ") +
                        proton::kLaunchLine + " ?")) {
        PrintLaunchOptionsHelp();
        return false;
    }

    bool allOk = true;
    for (const auto& cfg : needFix) {
        const std::string text = ReadAllText(cfg);
        const auto updated = proton::WithOverride(text);
        if (!updated) continue;

        const fs::path backup = cfg.string() + ".bak-pwru";
        std::error_code ec;
        if (!fs::exists(backup, ec)) {
            fs::copy_file(cfg, backup, ec);
            if (ec) {
                utils::Print("Не удалось сохранить копию " + cfg.string() + " — файл не тронут.",
                             utils::Status::Error);
                allOk = false;
                continue;
            }
        }
        if (!WriteAtomically(cfg, updated->data(), updated->size())) {
            utils::Print("Не удалось записать " + cfg.string(), utils::Status::Error);
            allOk = false;
            continue;
        }

        const proton::AppOptions check = proton::FindApp(ReadAllText(cfg));
        if (check.hasOptions && proton::HasOverride(check.options)) {
            utils::Print("Параметр запуска вписан: " + cfg.string() +
                             "\n(копия прежних настроек: " + backup.filename().string() + ")",
                         utils::Status::Success);
        } else {
            utils::Print("После записи параметр не найден: " + cfg.string(), utils::Status::Error);
            allOk = false;
        }
    }

    if (!allOk) PrintLaunchOptionsHelp();
    return allOk;
}

// ---------------------------------------------------------------
// Удаление компонентов мода
// ---------------------------------------------------------------

bool Installer::RemoveFileIfExists(const fs::path& file,
                                   const std::string& description) const {
    std::error_code ec;
    if (!fs::exists(file, ec)) {
        utils::Print("Не найден " + description + " — пропускаю.",
                     utils::Status::Info);
        return true;
    }
    fs::remove(file, ec);
    if (ec) {
        utils::Print("Не удалось удалить " + description + ": " +
                         ec.message(),
                     utils::Status::Error);
        return false;
    }
    utils::Print("Удалён " + description, utils::Status::Success);
    return true;
}

bool Installer::RemoveDirIfExists(const fs::path& dir,
                                  const std::string& description) const {
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        utils::Print("Не найдена " + description + " — пропускаю.",
                     utils::Status::Info);
        return true;
    }
    fs::remove_all(dir, ec);
    if (ec) {
        utils::Print("Не удалось удалить " + description + ": " +
                         ec.message(),
                     utils::Status::Error);
        return false;
    }
    utils::Print("Удалена " + description, utils::Status::Success);
    return true;
}

bool Installer::RemoveModFiles() const {
    bool ok = true;

    // 1) Модовые pak-файлы. Остальное в ~mods/ не трогаем.
    const fs::path paksMods = contentDir_ / "Content" / "Paks" / "~mods";
    ok &= RemoveFileIfExists(paksMods / "PW_RU_Voice.pak", "файл озвучки");
    ok &= RemoveFileIfExists(paksMods / "PW_RU_Subs.pak", "файл субтитров");

    // 2) Файлы UE4SS. UE4SS.dll не упомянут в задании — не трогаем.
    const fs::path binWin64 = contentDir_ / "Binaries" / "Win64";
    ok &= RemoveFileIfExists(binWin64 / "dwmapi.dll", "dwmapi.dll");
    ok &= RemoveFileIfExists(binWin64 / "UE4SS-settings.ini", "UE4SS-settings.ini");
    ok &= RemoveDirIfExists(binWin64 / "Mods", "папка Mods");
    ok &= RemoveFileIfExists(binWin64 / "UE4SS.log", "UE4SS.log");
    ok &= RemoveFileIfExists(binWin64 / "UE4SS.prev.log", "UE4SS.prev.log");

    // 3) Оригинальное интро: удаляем модовое и возвращаем сохранённый оригинал.
    const fs::path moviesDir = contentDir_ / "Content" / "Movies";
    const fs::path introDst  = moviesDir / "HumbleIntro_PW.mp4";
    const fs::path introOrig = moviesDir / "HumbleIntro_PW.mp4.pwru-orig";

    std::error_code ec;
    if (fs::exists(introOrig, ec)) {
        if (!RemoveFileIfExists(introDst, "модовое интро")) {
            ok = false;
        } else {
            fs::rename(introOrig, introDst, ec);
            if (ec) {
                utils::Print("Не удалось вернуть оригинальное интро: " +
                                 ec.message(),
                             utils::Status::Error);
                ok = false;
            } else {
                utils::Print("Оригинальное интро восстановлено.",
                             utils::Status::Success);
            }
        }
    } else {
        utils::Print("Резервная копия интро не найдена — интро не трогаю.",
                     utils::Status::Info);
    }

    return ok;
}

// ---------------------------------------------------------------
// Удаление параметра запуска для Proton
// ---------------------------------------------------------------

bool Installer::RemoveLaunchOptions(const std::vector<fs::path>& steamRoots) const {
    // localconfig.vdf всех учёток Steam: <Steam>/userdata/<id>/config/localconfig.vdf
    std::vector<fs::path> configs;
    std::vector<std::string> seen;
    for (const auto& root : steamRoots) {
        std::error_code ec;
        const fs::path userdata = root / "userdata";
        if (!fs::is_directory(userdata, ec)) continue;
        for (const auto& entry : fs::directory_iterator(userdata, ec)) {
            const fs::path cfg = entry.path() / "config" / "localconfig.vdf";
            std::error_code fec;
            if (!fs::is_regular_file(cfg, fec)) continue;
            const std::string key = fs::weakly_canonical(cfg, fec).string();
            if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
            seen.push_back(key);
            configs.push_back(cfg);
        }
    }

    if (configs.empty()) {
        utils::Print("Настройки Steam не найдены — параметр запуска не изменён.",
                     utils::Status::Warning);
        return false;
    }

    if (utils::IsProcessRunning("steam")) {
        utils::Print(
            "Steam сейчас запущен: убрать параметр запуска сам не могу —\n"
            "Steam перезапишет настройки при выходе.\n"
            "Удалите вручную: Свойства → Общие → Параметры запуска:\n"
            "    " + std::string(proton::kLaunchLine),
            utils::Status::Warning);
        return false;
    }

    bool changed = false;
    bool allOk = true;
    for (const auto& cfg : configs) {
        const std::string text = ReadAllText(cfg);
        const auto updated = proton::WithoutOverride(text);
        if (!updated) continue;
        if (*updated == text) continue;              // нечего менять

        const fs::path backup = cfg.string() + ".bak-pwru";
        std::error_code ec;
        if (!fs::exists(backup, ec)) {
            fs::copy_file(cfg, backup, ec);
            if (ec) {
                utils::Print("Не удалось сохранить копию " + cfg.string() + " — файл не тронут.",
                             utils::Status::Error);
                allOk = false;
                continue;
            }
        }
        if (!WriteAtomically(cfg, updated->data(), updated->size())) {
            utils::Print("Не удалось записать " + cfg.string(), utils::Status::Error);
            allOk = false;
            continue;
        }

        const proton::AppOptions check = proton::FindApp(ReadAllText(cfg));
        const bool clean = !check.hasOptions || !proton::HasOverride(check.options);
        if (clean) {
            utils::Print("Параметр запуска убран: " + cfg.string() +
                             "\n(копия прежних настроек: " + backup.filename().string() + ")",
                         utils::Status::Success);
            changed = true;
        } else {
            utils::Print("После записи параметр всё ещё найден: " + cfg.string(),
                         utils::Status::Error);
            allOk = false;
        }
    }

    if (!changed && allOk) {
        utils::Print("Параметр запуска dwmapi=n,b не найден — чистить нечего.",
                     utils::Status::Info);
    }
    return allOk;
}