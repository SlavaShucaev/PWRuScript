#include "installer.h"
#include "locator.h"
#include "utils.h"

#include <exception>
#include <fstream>
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
        // Здесь можно опционально дождаться завершения процесса.
    } else {
        utils::Print("Игра сейчас не запущена.", utils::Status::Success);
    }

    return 0;
}