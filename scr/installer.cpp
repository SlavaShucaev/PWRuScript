#include "installer.h"
#include "utils.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

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
    // contentDir_ — это корень папки ProjectWingman,
    // т.е. <...>/Project Wingman/ProjectWingman
    const fs::path paksMods = contentDir_ / "Content" / "Paks" / "~mods";
    const fs::path binWin64 = contentDir_ / "Binaries" / "Win64";

    bool found = false;

    // ---- Локализация ----
    found |= CheckFile(paksMods / "PW_RU_Voice.pak");
    found |= CheckFile(paksMods / "PW_RU_Subs.pak");

    // ---- UE4SS ----
    found |= CheckFile(binWin64 / "dwmapi.dll");
    found |= CheckFile(binWin64 / "UE4SS.dll");
    found |= CheckFile(binWin64 / "UE4SS-settings.ini");
    found |= CheckDirectory(binWin64 / "Mods");

    if (found) {
        utils::Print(
            "Обнаружены ранее установленные компоненты мода.\n"
            "Установка может перезаписать или конфликтовать с ними.\n"
            "Проверьте перечисленные выше пути.",
            utils::Status::Warning);

        if (needPause) {
            std::cout << "\nНажмите Enter, чтобы продолжить..." << std::flush;
            if (!std::cin.get()) {
                std::cin.clear();
            }
        }
    } else {
        utils::Print("Ранее установленных компонентов мода не обнаружено.",
                     utils::Status::Success);
    }

    return found;
}

// ---------------------------------------------------------------
// Установка
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