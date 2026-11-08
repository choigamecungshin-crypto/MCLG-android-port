#include "stdafx.h"

CConsoleMinecraftApp app;

CConsoleMinecraftApp::CConsoleMinecraftApp() : CMinecraftApp()
{
}

void CConsoleMinecraftApp::SetRichPresenceContext(int, int) {}
void CConsoleMinecraftApp::StoreLaunchData() {}
void CConsoleMinecraftApp::ExitGame() {}
void CConsoleMinecraftApp::FatalLoadError() {}

void CConsoleMinecraftApp::CaptureSaveThumbnail() {}
void CConsoleMinecraftApp::GetSaveThumbnail(PBYTE* data, DWORD* size)
{
    if (data) *data = NULL;
    if (size) *size = 0;
}
void CConsoleMinecraftApp::ReleaseSaveThumbnail() {}
void CConsoleMinecraftApp::GetScreenshot(int, PBYTE* data, DWORD* size)
{
    if (data) *data = NULL;
    if (size) *size = 0;
}

int CConsoleMinecraftApp::LoadLocalTMSFile(WCHAR*) { return -1; }
int CConsoleMinecraftApp::LoadLocalTMSFile(WCHAR*, eFileExtensionType) { return -1; }
void CConsoleMinecraftApp::FreeLocalTMSFiles(eTMSFileType) {}
int CConsoleMinecraftApp::GetLocalTMSFileIndex(WCHAR*, bool, eFileExtensionType) { return -1; }
void CConsoleMinecraftApp::ReadBannedList(int, eTMSAction, bool) {}

// World creation is routed through the Android world-selection UI. This hook
// remains the console app's game-start entry point for when that flow is wired.
void CConsoleMinecraftApp::TemporaryCreateGameStart()
{
    app.SetTutorialMode(false);
}
