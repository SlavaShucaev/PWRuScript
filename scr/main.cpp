#include "installer.h"
#include "locator.h"
#include "utils.h"

#include <curl/curl.h>

#include <chrono>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

const std::string exe_file_name = "ProjectWingman-Win64-Shipping";

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

fs::path LocateGame(){
    Locator locator;
    fs::path gamePath = locator.GetGamePath();

    if (gamePath.empty()) {
        utils::Print("Не удалось автоматически найти Project Wingman.",
                     utils::Status::Warning);
        gamePath = locator.AskUserForGamePath();
        if (gamePath.empty()) {
            utils::Print("Путь не указан. Установка отменена.",
                         utils::Status::Error);
            return {};
        }
    }

    utils::Print("Путь к Project Wingman:", utils::Status::Info);
    utils::Print(gamePath.string());
    return gamePath;
}

// Путь к запущенному исполняемому файлу (для удаления самого скрипта).
// На Linux /proc/self/exe всегда указывает на реальный бинарник, даже если
// он запущен по относительному пути или через симлинк.
static fs::path SelfPath() {
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::path{} : exe;
}

// Удаление самого скрипта. Запущенный файл на Linux удалить нельзя — отдаём
// удаление отдельному /bin/sh, который сначала дождётся завершения текущего
// процесса, а потом снесёт файл. Если не удалось — не критично.
static void RemoveSelf(const fs::path& self) {
    if (self.empty()) {
        utils::Print("Не удалось определить путь к скрипту — удалите его вручную.",
                     utils::Status::Warning);
        return;
    }

    utils::Print("Скрипт будет удалён после завершения: " + self.string(),
                 utils::Status::Info);

    // sh -c 'while kill -0 $1 2>/dev/null; do sleep 0.2; done; rm -f -- "$1"' sh <pid> <path>
    // Ждём, пока наш PID исчезнет, потом удаляем файл.
    const std::string pid = std::to_string(static_cast<long>(::getpid()));
    const std::string path = self.string();

    const pid_t child = ::fork();
    if (child < 0) {
        utils::Print("Не удалось запустить удаление — удалите скрипт вручную: " + path,
                     utils::Status::Warning);
        return;
    }
    if (child == 0) {
        // Дочерний процесс: отсоединяемся от терминала и ждём родителя.
        ::setsid();
        ::execl("/bin/sh", "sh", "-c",
                "while kill -0 \"$1\" 2>/dev/null; do sleep 0.2; done; rm -f -- \"$2\"",
                "sh", pid.c_str(), path.c_str(),
                static_cast<char*>(nullptr));
        ::_exit(127);
    }
}

int InstallMod (){
    utils::PrintSideBySide(kAsciiArt, kIntroText);

    // Пауза, чтобы пользователь успел прочитать сообщение.
    std::this_thread::sleep_for(std::chrono::seconds(3));

    curl_global_init(CURL_GLOBAL_DEFAULT);

    // RAII-очистка curl при любом выходе из main.
    struct CurlGuard {
        ~CurlGuard() { curl_global_cleanup(); }
    } curlGuard;

    fs::path gamePath = LocateGame();
    if (gamePath.empty()) return 1;

    if (utils::IsProcessRunning(exe_file_name)) {
        utils::Print(
            "Игра сейчас запущена!\n"
            "Установка локализации приостановлена.\n"
            "Пожалуйста, закройте игру.",
            utils::Status::Warning);
        return 1;
    }
    utils::Print("Игра сейчас не запущена.", utils::Status::Success);

    try {
        Installer installer(gamePath / "ProjectWingman",
                            "PW_RU_Voice.pak",
                            "PW_RU_Subs.pak");

        installer.CheckExistingInstallation(/*needPause=*/true);
        installer.LoadChecksums();

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

        // Старые версии скрипта меняли exe игры (en-US -> ru-RU); это больше не нужно —
        // язык держит UE4SS. Вернуть оригинал не критично: при неудаче только подсказка.
        installer.RestoreOriginalExe();

        // Под Proton UE4SS грузится только с WINEDLLOVERRIDES="dwmapi=n,b".
        Locator locator;
        const bool launchOptionsOk = installer.SetupLaunchOptions(locator.SteamRoots());

        utils::Print("Все компоненты успешно установлены.",
                     utils::Status::Success);
        if (!launchOptionsOk) {
            utils::Print("Осталось вписать параметр запуска в Steam (см. выше) —\n"
                         "без него язык будет сбрасываться на английский.",
                         utils::Status::Warning);
        }
    } catch (const UserCancelled& e) {
        utils::Print(std::string("Установка отменена: ") + e.what(),
                     utils::Status::Info);
        return 1;
    } catch (const std::exception& e) {
        utils::Print(std::string("Ошибка установки: ") + e.what(),
                     utils::Status::Error);
        return 1;
    }

    return 0;
}

int UninstallAll() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    struct CurlGuard {
        ~CurlGuard() { curl_global_cleanup(); }
    } curlGuard;

    fs::path gamePath = LocateGame();
    if (gamePath.empty()) return 1;

    if (utils::IsProcessRunning(exe_file_name)) {
        utils::Print(
            "Игра сейчас запущена!\n"
            "Удаление локализации приостановлено.\n"
            "Пожалуйста, закройте игру.",
            utils::Status::Warning);
        return 1;
    }
    utils::Print("Игра сейчас не запущена.", utils::Status::Success);

    bool allOk = true;
    try {
        Installer installer(gamePath / "ProjectWingman",
                            "PW_RU_Voice.pak",
                            "PW_RU_Subs.pak");

        allOk = installer.RemoveModFiles();

        Locator locator;
        allOk = installer.RemoveLaunchOptions(locator.SteamRoots()) && allOk;
    } catch (const std::exception& e) {
        utils::Print(std::string("Ошибка удаления: ") + e.what(),
                     utils::Status::Error);
        return 1;
    }

    if (!allOk) {
        utils::Print(
            "Удаление завершено не полностью: часть файлов или настроек "
            "не удалось изменить. См. сообщения выше.\n"
            "Скрипт оставлен на месте — запустите --uninstall ещё раз после "
            "исправления ошибок.",
            utils::Status::Warning);
        return 1;
    }

    utils::Print("Компоненты мода удалены. Удаляю скрипт...",
                 utils::Status::Success);
    RemoveSelf(SelfPath());
    return 0;
}

int main(int argc, char* argv[]) {
    // Возращаем код ошибки в main
    int result = 0;

    // Без аргументов — установка мода. С аргументом — одна команда.
    if (argc == 1) {
        result = InstallMod();
    } else if (argc == 2) {
        std::string arg = argv[1];

        if (arg == "--uninstall" || arg == "-u") {
            result = UninstallAll();
        } else if (arg == "--help" || arg =="-h") {
            utils::Print("Доступные команды:\n"
                         "Скрипт может принять только один аргумент\n"
                         "  без аргументов — установка локализации\n"
                         "  --uninstall или -u   — удаление локализации, её настроек и самого скрипта\n"
                         "  --help или -h         — эта справка");
        } else {
            utils::Print("Неизвестный аргумент: " + arg + "\n"
                                                          "Введите ./pwru_script --help \n"
                                                          "чтобы получить список доступных комманд.",
                         utils::Status::Warning);
            result = 1;
        }
    } else {
        utils::Print("Слишком много аргументов. Скрипт принимает не более одного.\n"
                     "Введите ./pwru_script --help \n"
                     "чтобы получить список доступных комманд.",
                     utils::Status::Warning);
        result = 1;
    }

    return result;
}