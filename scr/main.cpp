#include "installer.h"
#include "locator.h"
#include "utils.h"

#include <curl/curl.h>

#include <exception>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

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
            curl_global_cleanup();
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
        curl_global_cleanup();
        return 1;
    }
    utils::Print("Игра сейчас не запущена.", utils::Status::Success);

    Installer installer(gamePath / "ProjectWingman",
                        "PW_RU_Voice.pak",
                        "PW_RU_Subs.pak");

    try {
        installer.CheckExistingInstallation(/*needPause=*/true);
    } catch (const UserCancelled& e) {
        utils::Print(std::string("Установка отменена: ") + e.what(),
                     utils::Status::Info);
        curl_global_cleanup();
        return 1;
    } catch (const std::exception& e) {
        utils::Print(std::string("Ошибка проверки: ") + e.what(),
                     utils::Status::Error);
        curl_global_cleanup();
        return 1;
    }

    const bool ok = installer.DownloadAndInstallAll();

    curl_global_cleanup();

    if (!ok) {
        utils::Print("Установка не завершена из-за ошибок.",
                     utils::Status::Error);
        return 1;
    }

    return 0;
}