#include "installer.h"
#include "locator.h"
#include "utils.h"

#include <exception>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main() {
    const std::string exe_file_name = "ProjectWingman-Win64-Shipping";

    Locator locator;
    fs::path gamePath = locator.GetGamePath();

    if (gamePath.empty()) {
        utils::Print("Не удалось автоматически найти Project Wingman.",
                     utils::Status::Warning);
        gamePath = locator.AskUserForGamePath();
        if (gamePath.empty()) {
            utils::Print("Путь не указан. Установка отменена.",
                         utils::Status::Error);
            return 1;
        }
    }

    utils::Print("Путь к Project Wingman:", utils::Status::Info);
    utils::Print(gamePath.string());

    if (utils::IsProcessRunning(exe_file_name)) {
        utils::Print(
            "Игра сейчас запущена!\n"
            "Установка локализации приостановлена.\n"
            "Пожалуйста, закройте игру.",
            utils::Status::Warning);
        return 1;
    }
    utils::Print("Игра сейчас не запущена.", utils::Status::Success);

    // Пути к исходным .pak — задаются вызывающей стороной.
    const fs::path voicePak    = "PW_RU_Voice.pak";
    const fs::path subtitlePak = "PW_RU_Subs.pak";

    Installer installer(gamePath / "ProjectWingman", voicePak, subtitlePak);

    // 1) Проверяем, что уже установлено. При находках — предупреждаем
    //    и ждём Enter.
    installer.CheckExistingInstallation(/*needPause=*/true);

    // 2) Установка. TODO
    // try {
    //     installer.InstallVoice();
    //     installer.InstallSubtitles();
    //     utils::Print("Установка завершена.", utils::Status::Success);
    // } catch (const std::exception& e) {
    //     utils::Print(e.what(), utils::Status::Error);
    //     return 1;
    // }

    return 0;
}