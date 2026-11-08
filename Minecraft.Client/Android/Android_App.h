#pragma once

// The console application contract is retained, while Android-specific
// implementations replace console profile, storage, screenshot and DLC calls.
class CConsoleMinecraftApp : public CMinecraftApp
{
public:
    CConsoleMinecraftApp();

    void SetRichPresenceContext(int iPad, int contextId) override;
    void StoreLaunchData() override;
    void ExitGame() override;
    void FatalLoadError() override;

    void CaptureSaveThumbnail() override;
    void GetSaveThumbnail(PBYTE* data, DWORD* size) override;
    void ReleaseSaveThumbnail() override;
    void GetScreenshot(int iPad, PBYTE* data, DWORD* size) override;

    int LoadLocalTMSFile(WCHAR* filename) override;
    int LoadLocalTMSFile(WCHAR* filename, eFileExtensionType extension) override;
    void FreeLocalTMSFiles(eTMSFileType type) override;
    int GetLocalTMSFileIndex(
        WCHAR* filename,
        bool includesExtension,
        eFileExtensionType extension = eFileExtensionType_PNG
    ) override;

    void ReadBannedList(
        int iPad,
        eTMSAction action = (eTMSAction)0,
        bool callback = false
    ) override;

    C4JStringTable* GetStringTable() { return NULL; }
    void TemporaryCreateGameStart();
};

extern CConsoleMinecraftApp app;
