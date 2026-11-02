#include "installer.h"

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
    // Проверяем исходный файл.
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

    // Проверяем папку игры до создания новых каталогов.
    const fs::path paksDir = contentDir_ / "Paks";

    if (!fs::is_directory(paksDir)) {
        throw std::runtime_error(
            "Не найдена папка Paks: " + paksDir.string()
        );
    }

    const fs::path modsDir = paksDir / "~mods";
    const fs::path destination = modsDir / destinationName;

    // Пока не заменяем уже установленные файлы.
    if (fs::exists(destination)) {
        throw std::runtime_error(
            "Файл уже существует, перезапись отменена: "
            + destination.string()
        );
    }

    fs::create_directories(modsDir);

    // Без overwrite_existing: существующий файл не перезаписываем.
    fs::copy_file(sourcePak, destination);
}
