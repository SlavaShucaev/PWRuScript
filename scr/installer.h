#pragma once

#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

class UserCancelled : public std::runtime_error {
public:
    explicit UserCancelled(const std::string& what)
        : std::runtime_error(what) {}
};

class Installer {
public:
    Installer(
        const std::filesystem::path& contentDir,
        const std::filesystem::path& voicePak,
        const std::filesystem::path& subtitlePak
        );

    bool CheckExistingInstallation(bool needPause = true) const;

    void InstallVoice() const;
    void InstallSubtitles() const;

    // Суммы модов из релиза (PW_RU_mods.sha256). Нет файла сумм (релизы до 1.0.6) —
    // проверка скачанного пропускается с предупреждением.
    void LoadChecksums();

    bool DownloadAndInstallAll() const;
    bool DownloadAndInstallUE4SS() const;

    // Старые версии скрипта меняли exe игры (en-US -> ru-RU). Если exe отличается от
    // резервной копии ровно этой заменой — возвращаем оригинал. false — exe не тронут
    // из-за ошибки или потому что игру с тех пор обновляли.
    bool RestoreOriginalExe() const;

    // Параметр запуска для Proton WINEDLLOVERRIDES="dwmapi=n,b": проверяет его в
    // localconfig.vdf найденных Steam; при закрытом Steam предлагает вписать.
    // true — параметр уже стоит или вписан.
    bool SetupLaunchOptions(const std::vector<std::filesystem::path>& steamRoots) const;

    static void PrintLaunchOptionsHelp();

private:
    void InstallPak(
        const std::filesystem::path& sourcePak,
        const std::filesystem::path& destinationName
        ) const;

    // Скачивает pak из GitHub и кладёт в ~mods/ с перезаписью.
    bool DownloadPakToMods(const std::string& fileName,
                           const std::string& label) const;

    bool DownloadFile(const std::string& url,
                      const std::filesystem::path& destFile,
                      const std::string& label,
                      bool quiet = false) const;

    // Сверяет файл с суммой из релиза; при несовпадении удаляет его.
    bool VerifyChecksum(const std::filesystem::path& file,
                        const std::string& assetName) const;

    bool ExtractZip(const std::filesystem::path& zipPath,
                    const std::filesystem::path& destDir) const;

    // Копирует файл с overwrite и логирует ошибку.
    bool CopyFileOrLog(const std::filesystem::path& src,
                       const std::filesystem::path& dst,
                       const std::string& description) const;

    bool DeployUE4SSFiles(const std::filesystem::path& extractedRoot) const;

    std::filesystem::path contentDir_;
    std::filesystem::path voicePak_;
    std::filesystem::path subtitlePak_;
    std::map<std::string, std::string> sums_;   // имя ассета -> sha256
};