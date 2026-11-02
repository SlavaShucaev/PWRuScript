#include "installer.h"
#include "locator.h"
#include "utils.h"

#include <exception>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

namespace fs = std::filesystem;

int main() {
    const std::string exe_file_name_ = "ProjectWingman-Win64-Shipping";

    Locator locator = Locator();
    fs::path path = locator.GetGamePath();
    if(path.empty()){
        utils::Print("Неудалось автоматически найти Project Wingman",
                     utils::Status::Warning);
    }else{
    utils::Print("Путь к Project Wingman :",
                 utils::Status::Info);
    utils::Print(path);
    }
    //Проверка, запушен ли игра
    //Если игра запущена, то ждем когда она закроется
    //Либо закрывем скрипт, по усмотрению пользователя
    if(utils::IsProcessRunning(exe_file_name_)){
        utils::Print("Игра сейчас запущена!\nУстановка локализации приостановлена\nПожайлуста закройте игру",
                     utils::Status::Warning);
    }else{
        utils::Print("Игра сейчас не запущена.",
                     utils::Status::Success);
    }

    return 0;
}
