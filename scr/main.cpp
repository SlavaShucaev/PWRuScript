#include "installer.h"
#include "locator.h"
#include "utils.h"

#include <curl/curl.h>

#include <chrono>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

static const std::vector<std::string> kAsciiArt = {
R"(                  @                    )",
R"(                @@#                    )",
R"(               @#**  =                 )",
R"(              @****=@                  )",
R"(            @#****@==     @            )",
R"(           @****@@===  @@*@            )",
R"(         @%**##@=====@#***@            )",
R"(        @**#*@@=====#*****@=           )",
R"(       @**=%@=======*****#==           )",
R"(        @*=@========**=@====           )",
R"(        @*+@=======*=@======           )",
R"(    =    @#@=======#========****#@     )",
R"(      ===  %===============#****@      )",
R"(       =====  =============*+===@=     )",
R"(        -==== =================@       )",
R"(           ==== =============+%        )",
R"(              === ==========@#         )",
R"(                ===   @@@@@%           )",
R"(                   ==                  )",
R"(                      ==               )",
R"(                         -             )",
};

static const std::vector<std::string> kIntroText = {
    "Скрипт утановщик русской локализации Project Wingman",
    "Локализацией занималсь:              VGS Studio",
    "Project Wingman и его артом владеют: Sector D2",
    "Автор скрипта:                       Slava Shucaev",
    "----------------------------------------------------",
    "Скоро начнётся установка ...",
    "----------------------------------------------------",
    "«The missile knows where it is at all times.»",
};

int main() {
    utils::PrintSideBySide(kAsciiArt, kIntroText);

    // Пауза, чтобы пользователь успел прочитать сообщение.
    std::this_thread::sleep_for(std::chrono::seconds(3));

    curl_global_init(CURL_GLOBAL_DEFAULT);

    // RAII-очистка curl при любом выходе из main.
    struct CurlGuard {
        ~CurlGuard() { curl_global_cleanup(); }
    } curlGuard;

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

    Installer installer(gamePath / "ProjectWingman",
                        "PW_RU_Voice.pak",
                        "PW_RU_Subs.pak");

    try {
        installer.CheckExistingInstallation(/*needPause=*/true);
    } catch (const UserCancelled& e) {
        utils::Print(std::string("Установка отменена: ") + e.what(),
                     utils::Status::Info);
        return 1;
    } catch (const std::exception& e) {
        utils::Print(std::string("Ошибка проверки: ") + e.what(),
                     utils::Status::Error);
        return 1;
    }

    if (!installer.DownloadAndInstallAll()) {
        utils::Print("Установка не завершена из-за ошибок.",
                     utils::Status::Error);
        return 1;
    }

    if (!installer.DownloadAndInstallUE4SS()) {
        utils::Print("Установка UE4SS не завершена из-за ошибок.",
                     utils::Status::Error);
        return 1;
    }

    // Фикс языка не критичен: сообщаем, но установку не прерываем.
    if (!installer.ApplyLanguageFix()) {
        utils::Print("Не удалось применить исправление языка.",
                     utils::Status::Error);
    }

    utils::Print("Все компоненты успешно установлены.",
                 utils::Status::Success);
    return 0;
}