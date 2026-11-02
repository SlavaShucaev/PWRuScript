#include <filesystem>
#include <cstdlib>
#include <exception>
#include <iostream>

#include "locator.h"
#include "utils.h"

namespace fs = std::filesystem;

    fs::path Locator::LocateSteamPath(){
        //return homeDir_ / ".local" / "share" / "Steam";
        return homeDir_ / ".steam" / "steam";
    }

    fs::path Locator::GetGamePath(){
        fs::path result;

        try
        {
            fs::path steam_p = LocateSteamPath();
            result = steam_p / "steamapps" / "common" / "Project Wingman";
        }
        catch(const std::exception& e)
        {
            utils::Print(e.what(),
                         utils::Status::Error);
        }
        if(!Locator::IsValidDirecoty(result)){
            return fs::path();
        }
        return result;
    }

    bool Locator::IsValidDirecoty(const std::string& targetDir){
        return fs::exists(targetDir) && fs::is_directory(targetDir);
    }

    bool Locator::IsValidFile(const std::string& targetFile){
        return fs::exists(targetFile) && fs::is_regular_file(targetFile);
    }