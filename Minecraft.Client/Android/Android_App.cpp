#include "stdafx.h"
#include "Minecraft.h"

#include <android/log.h>

namespace {
bool g_androidGameStarted = false;
}

extern "C" bool MclgAndroidGameInitialize()
{
    if (g_androidGameStarted)
        return Minecraft::GetInstance() != NULL;

    __android_log_print(ANDROID_LOG_INFO, "MCLG-Android", "Starting Legacy Edition client");
    try
    {
        // This is the original client bootstrap: static registries, render
        // dispatchers, app rules, User, and the native title-screen startup.
        // On Android Minecraft::run() performs init() only; the GLSurfaceView
        // drives run_middle() once per frame below.
        Minecraft::main();
        Minecraft* game = Minecraft::GetInstance();
        if (game == NULL)
        {
            __android_log_print(ANDROID_LOG_ERROR, "MCLG-Android", "Minecraft bootstrap returned without an instance");
            return false;
        }

        g_androidGameStarted = true;
        __android_log_print(ANDROID_LOG_INFO, "MCLG-Android", "Legacy client initialized (%d x %d)", game->width, game->height);
        return true;
    }
    catch (...)
    {
        __android_log_print(ANDROID_LOG_ERROR, "MCLG-Android", "Unhandled C++ exception during Minecraft bootstrap");
        return false;
    }
}

extern "C" void MclgAndroidGameFrame()
{
    Minecraft* game = Minecraft::GetInstance();
    if (g_androidGameStarted && game != NULL && game->running)
        game->run_middle();
}

extern "C" void MclgAndroidGameResize(int width, int height)
{
    Minecraft* game = Minecraft::GetInstance();
    if (game == NULL || width <= 0 || height <= 0)
        return;

    game->width_phys = width;
    game->height_phys = height;
    game->width = width;
    game->height = height;
}

extern "C" void MclgAndroidGameShutdown()
{
    Minecraft* game = Minecraft::GetInstance();
    if (game != NULL && g_androidGameStarted)
        game->run_end();
    g_androidGameStarted = false;
}


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
