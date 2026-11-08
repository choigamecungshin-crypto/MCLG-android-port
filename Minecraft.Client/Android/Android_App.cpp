#include "stdafx.h"
#include "Minecraft.h"
#include "Screen.h"
#include "MinecraftServer.h"
#include "C4JThread.h"
#include "Tesselator.h"

#include <android/log.h>

namespace {
bool g_androidGameStarted = false;
bool g_androidNetworkStarted = false;
bool g_androidWorldStarting = false;
}

extern "C" bool MclgAndroidGameInitialize()
{
    if (g_androidGameStarted)
        return Minecraft::GetInstance() != NULL;

    __android_log_print(ANDROID_LOG_INFO, "MCLG-Android", "Starting Legacy Edition client");
    try
    {
        // Match the original console bootstrap: install worker-local pools
        // and the offline network adapter before the Minecraft singleton starts.
        if (!g_androidNetworkStarted) {
            g_NetworkManager.Initialise();
            g_androidNetworkStarted = true;
        }
        Tesselator::CreateNewThreadStorage(1024 * 1024);
        AABB::CreateNewThreadStorage();
        Vec3::CreateNewThreadStorage();
        IntCache::CreateNewThreadStorage();
        Compression::CreateNewThreadStorage();
        OldChunkStorage::CreateNewThreadStorage();
        Level::enableLightingCache();
        Tile::CreateNewThreadStorage();

        // Minecraft::run() only performs init() on Android. GLSurfaceView owns
        // the frame cadence and calls run_middle() below.
        Minecraft::main();
        Minecraft* game = Minecraft::GetInstance();
        if (game == NULL)
        {
            __android_log_print(ANDROID_LOG_ERROR, "MCLG-Android", "Minecraft bootstrap returned without an instance");
            return false;
        }

        ProfileManager.SetDebugFullOverride(true);
        ProfileManager.SetPrimaryPad(0);
        ProfileManager.SetLockedProfile(0);
        app.InitGameSettings();
        app.InitialiseTips();
        if (game->user != NULL) game->user->name = L"Player";

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

extern "C" bool MclgAndroidStartWorld(const wchar_t* worldName, __int64 seed)
{
    if (!g_androidGameStarted || g_androidWorldStarting)
        return false;

    Minecraft* game = Minecraft::GetInstance();
    if (game == NULL || game->user == NULL)
        return false;

    g_androidWorldStarting = true;
    app.setLevelGenerationOptions(NULL);
    app.ReleaseSaveThumbnail();
    ProfileManager.SetLockedProfile(0);
    game->user->name = L"Player";
    app.ApplyGameSettingsChanged(0);

    MinecraftServer::resetFlags();
    app.SetTutorialMode(false);
    app.SetCorruptSaveDeleted(false);
    app.ClearTerrainFeaturePosition();

    const wchar_t* safeName = (worldName != NULL && worldName[0] != 0) ? worldName : L"World";
    StorageManager.ResetSaveData();
    StorageManager.SetSaveTitle(safeName);

    NetworkGameInitData* params = new NetworkGameInitData();
    params->seed = seed;

    app.SetGameHostOption(eGameHostOption_Difficulty, 1);
    app.SetGameHostOption(eGameHostOption_FriendsOfFriends, 0);
    app.SetGameHostOption(eGameHostOption_Gamertags, 1);
    app.SetGameHostOption(eGameHostOption_BedrockFog, 1);
    app.SetGameHostOption(eGameHostOption_GameType, GameType::SURVIVAL->getId());
    app.SetGameHostOption(eGameHostOption_LevelType, 0);
    app.SetGameHostOption(eGameHostOption_Structures, 1);
    app.SetGameHostOption(eGameHostOption_BonusChest, 0);
    app.SetGameHostOption(eGameHostOption_PvP, 1);
    app.SetGameHostOption(eGameHostOption_TrustPlayers, 1);
    app.SetGameHostOption(eGameHostOption_FireSpreads, 1);
    app.SetGameHostOption(eGameHostOption_TNT, 1);
    app.SetGameHostOption(eGameHostOption_HostCanFly, 0);
    app.SetGameHostOption(eGameHostOption_HostCanChangeHunger, 1);
    app.SetGameHostOption(eGameHostOption_HostCanBeInvisible, 0);
    params->settings = app.GetGameHostOption(eGameHostOption_All);

    // The local QNet adapter is offline, single-player, and reports one host.
    g_NetworkManager.HostGame(1, false, false);
    g_NetworkManager.FakeLocalPlayerJoined();

    C4JThread* gameThread = new C4JThread(
        &CGameNetworkManager::RunNetworkGameThreadProc,
        params,
        "AndroidLocalWorld",
        512 * 1024
    );
    gameThread->Run();
    __android_log_print(ANDROID_LOG_INFO, "MCLG-Android", "Started Legacy world thread: %ls", safeName);
    return true;
}

extern "C" void MclgAndroidInputTouch(float x, float y, int pressed)
{
    Minecraft* game = Minecraft::GetInstance();
    if (!g_androidGameStarted || game == NULL || game->screen == NULL ||
        game->width <= 0 || game->height <= 0)
        return;

    Screen* screen = game->screen;
    const int screenX = static_cast<int>(x * screen->width);
    const int screenY = screen->height - static_cast<int>(y * screen->height) - 1;
    if (pressed)
        screen->handleAndroidTouch(screenX, screenY, true);
    else
        screen->handleAndroidTouch(screenX, screenY, false);
}
