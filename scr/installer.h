#pragma once

#include <curl/curl.h>
#include <filesystem>

class Installer {
public:
    Installer(
        const std::filesystem::path& contentDir,
        const std::filesystem::path& voicePak,
        const std::filesystem::path& subtitlePak
    );

    void InstallVoice() const;
    void InstallSubtitles() const;

private:
    void InstallPak(
        const std::filesystem::path& sourcePak,
        const std::filesystem::path& destinationName
    ) const;

    std::filesystem::path contentDir_;
    std::filesystem::path voicePak_;
    std::filesystem::path subtitlePak_;
};
