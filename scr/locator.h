#pragma once

#include <string>

namespace fs = std::filesystem;

class Locator{
    public:
    Locator(){
        const char* env_p = std::getenv("HOME");
        homeDir_ = (env_p);
    }

    fs::path GetGamePath();
    bool IsValidDirecoty(const std::string& targetDir);
    bool IsValidFile(const std::string& targetFile);

    private:
    fs::path LocateSteamPath();
    fs::path homeDir_;

};