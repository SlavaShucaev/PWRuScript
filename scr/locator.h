#pragma once

#include <filesystem>
#include <string>
#include <vector>

class Locator {
public:
    Locator();

    std::filesystem::path GetGamePath();
    std::filesystem::path AskUserForGamePath();

    static bool IsValidDirectory(const std::string& targetDir);
    static bool IsValidFile(const std::string& targetFile);

private:
    std::vector<std::filesystem::path> CandidateSteamPaths() const;
    std::vector<std::filesystem::path> ReadSteamLibraries(
        const std::filesystem::path& steamRoot) const;

    std::filesystem::path homeDir_;
};