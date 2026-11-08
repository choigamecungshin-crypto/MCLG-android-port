#include "stdafx.h"
#include "AABB.h"
#include "IntBuffer.h"
#include "IntCache.h"
#include "Level.h"
#include "LevelSettings.h"
#include "OldChunkStorage.h"
#include "Tile.h"
#include "User.h"
#include "Minecraft.h"
#include "Screen.h"
#include "MinecraftServer.h"
#include "C4JThread.h"
#include "Tesselator.h"

#include <android/log.h>
#include <deque>
#include <mutex>

namespace {
bool g_androidGameStarted = false;
bool g_androidNetworkStarted = false;
bool g_androidWorldStarting = false;
__int64 g_androidWorldStartTime = 0;

enum AndroidInputEventType { ANDROID_INPUT_TOUCH, ANDROID_INPUT_MOUSE, ANDROID_INPUT_KEY };
struct AndroidInputEvent {
    AndroidInputEventType type;
    float x;
    float y;
    int keyCode;
    int unicode;
    int button;
    int pressed;
};
std::mutex g_androidTouchMutex;
std::deque<AndroidInputEvent> g_androidTouchQueue;

unsigned int androidButtonMask(int keyCode)
{
    switch (keyCode)
    {
        case 62:  // Space
        case 96:  // Gamepad A
            return 0x00000001;
        case 45:  // Q
        case 97:  // Gamepad B
            return 0x00000002;
        case 99:  // Gamepad X
            return 0x00000004;
        case 33:  // E
        case 100: // Gamepad Y
            return 0x00000008;
        case 4:   // Android Back
        case 111: // Escape
        case 108: // Gamepad Start
            return 0x00000010;
        case 109: // Gamepad Select
            return 0x00000020;
        case 103: // R1
            return 0x00000040;
        case 102: // L1
            return 0x00000080;
        case 59:  // Left Shift
        case 60:  // Right Shift
        case 107: // Right stick
            return 0x00000100;
        case 135: // F5
        case 106: // Left stick
            return 0x00000200;
        case 19: return 0x00000400; // D-pad up
        case 20: return 0x00000800; // D-pad down
        case 21: return 0x00001000; // D-pad left
        case 22: return 0x00002000; // D-pad right
        case 105: return 0x00400000; // R2
        case 104: return 0x00800000; // L2
        default: return 0;
    }
}
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
        // Mark the build as the full, offline single-player edition before
        // Minecraft::main() initializes Options, title screens and world lists.
        ProfileManager.SetDebugFullOverride(true);
        ProfileManager.SetPrimaryPad(0);
        ProfileManager.SetLockedProfile(0);

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
        if (game->screen == NULL || game->user == NULL)
        {
            __android_log_print(ANDROID_LOG_ERROR, "MCLG-Android",
                                "Legacy client initialization did not create its title screen and local user");
            return false;
        }

        app.InitGameSettings();
        app.InitialiseTips();
        if (game->user != NULL) game->user->name = L"Player";

        g_androidGameStarted = true;
        __android_log_print(ANDROID_LOG_INFO, "MCLG-Android", "Legacy client initialized (%d x %d; title screen ready)", game->width, game->height);
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
    if (!g_androidGameStarted || game == NULL || !game->running)
        return;

    // All UI events are consumed on the GL thread so screen state and game state
    // never race the Android input thread.
    std::deque<AndroidInputEvent> pendingEvents;
    {
        std::lock_guard<std::mutex> lock(g_androidTouchMutex);
        pendingEvents.swap(g_androidTouchQueue);
    }
    for (const AndroidInputEvent& event : pendingEvents) {
        Screen* screen = game->screen;
        if (event.type == ANDROID_INPUT_TOUCH) {
            if (screen == NULL || screen->width <= 0 || screen->height <= 0) continue;
            const int x = static_cast<int>(event.x * screen->width);
            const int y = static_cast<int>(event.y * screen->height);
            screen->handleAndroidTouch(x, y, event.pressed != 0);
        } else if (event.type == ANDROID_INPUT_MOUSE) {
            if (screen != NULL && screen->width > 0 && screen->height > 0) {
                const int x = static_cast<int>(event.x * screen->width);
                const int y = static_cast<int>(event.y * screen->height);
                screen->handleAndroidMouse(x, y, event.button, event.pressed != 0);
            } else {
                const unsigned int mask = event.button == 0 ? 0x00400000 :
                                          event.button == 1 ? 0x00800000 :
                                          event.button == 2 ? 0x00000020 : 0;
                if (mask != 0) MclgAndroidInputButton(mask, event.pressed);
            }
        } else if (event.type == ANDROID_INPUT_KEY) {
            if (screen != NULL) {
                screen->handleAndroidKey(event.keyCode, event.unicode, event.pressed != 0);
            } else {
                const unsigned int mask = androidButtonMask(event.keyCode);
                if (mask != 0) MclgAndroidInputButton(mask, event.pressed);
            }
        }
    }

    if (g_androidWorldStarting && game->level != NULL)
        g_androidWorldStarting = false;
    else if (g_androidWorldStarting && !g_NetworkManager.IsNetworkThreadRunning() &&
             System::currentTimeMillis() - g_androidWorldStartTime > 10000)
        g_androidWorldStarting = false;

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

// The original Legacy UI calls this hook after the player confirms the
// Create World screen. Route that existing action into the Android offline
// integrated-server path instead of leaving the callback as a no-op.
extern "C" bool MclgAndroidStartWorld(const wchar_t* worldName, __int64 seed);

void CConsoleMinecraftApp::TemporaryCreateGameStart()
{
    app.SetTutorialMode(false);
    const __int64 seed = System::currentTimeMillis();
    if (!MclgAndroidStartWorld(L"World", seed)) {
        __android_log_print(ANDROID_LOG_ERROR, "MCLG-Android",
                            "The Create World action could not start the local world");
    }
}

static void configureAndroidLocalGame()
{
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
}

static bool launchAndroidLocalWorld(NetworkGameInitData* params, const wchar_t* worldName)
{
    if (!g_androidGameStarted || g_androidWorldStarting || params == NULL)
        return false;
    Minecraft* game = Minecraft::GetInstance();
    if (game == NULL || game->user == NULL)
        return false;

    g_androidWorldStarting = true;
    g_androidWorldStartTime = System::currentTimeMillis();
    app.setLevelGenerationOptions(NULL);
    app.ReleaseSaveThumbnail();
    ProfileManager.SetLockedProfile(0);
    ProfileManager.SetPrimaryPad(0);
    game->user->name = L"Player";
    app.ApplyGameSettingsChanged(0);
    MinecraftServer::resetFlags();
    app.SetTutorialMode(false);
    app.SetCorruptSaveDeleted(false);
    app.ClearTerrainFeaturePosition();

    configureAndroidLocalGame();
    params->settings = app.GetGameHostOption(eGameHostOption_All);

    // Run the original Legacy integrated server through the offline network adapter.
    g_NetworkManager.HostGame(1, false, false);
    g_NetworkManager.FakeLocalPlayerJoined();

    C4JThread* gameThread = new C4JThread(
        &CGameNetworkManager::RunNetworkGameThreadProc,
        params,
        "AndroidLocalWorld",
        512 * 1024
    );
    gameThread->Run();
    __android_log_print(ANDROID_LOG_INFO, "MCLG-Android",
                        "Started Legacy world thread: %ls", worldName ? worldName : L"World");
    return true;
}

extern "C" bool MclgAndroidStartWorld(const wchar_t* worldName, __int64 seed)
{
    if (!g_androidGameStarted || g_androidWorldStarting)
        return false;

    const wchar_t* safeName = (worldName != NULL && worldName[0] != 0) ? worldName : L"World";
    StorageManager.ResetSaveData();
    StorageManager.SetSaveTitle(safeName);

    NetworkGameInitData* params = new NetworkGameInitData();
    params->seed = seed;
    params->saveData = NULL;
    return launchAndroidLocalWorld(params, safeName);
}

struct AndroidWorldLoadCallback
{
    bool called;
    bool success;
};

static int androidWorldLoadComplete(LPVOID parameter, const bool success, const bool)
{
    AndroidWorldLoadCallback* result = static_cast<AndroidWorldLoadCallback*>(parameter);
    if (result != NULL) {
        result->called = true;
        result->success = success;
    }
    return 0;
}

extern "C" bool MclgAndroidLoadWorld(const wchar_t* worldName)
{
    if (!g_androidGameStarted || g_androidWorldStarting || worldName == NULL || worldName[0] == 0)
        return false;

    SAVE_DETAILS* details = StorageManager.ReturnSavesInfo();
    if (details == NULL || details->SaveInfoA == NULL)
        return false;

    SAVE_INFO* selected = NULL;
    for (int i = 0; i < details->iSaveC; ++i) {
        wstring candidate;
        for (const char* p = details->SaveInfoA[i].UTF8SaveFilename; *p; ++p)
            candidate.push_back(static_cast<unsigned char>(*p));
        if (candidate == worldName) {
            selected = &details->SaveInfoA[i];
            break;
        }
    }
    if (selected == NULL)
        return false;

    AndroidWorldLoadCallback callback = {false, false};
    StorageManager.LoadSaveData(selected, androidWorldLoadComplete, &callback);
    if (!callback.called || !callback.success)
        return false;

    DWORD bytes = StorageManager.GetSaveSize();
    if (bytes == 0)
        return false;

    BYTE* saveBuffer = new (std::nothrow) BYTE[bytes];
    if (saveBuffer == NULL)
        return false;
    DWORD copied = bytes;
    StorageManager.GetSaveData(saveBuffer, &copied);
    if (copied == 0) {
        delete[] saveBuffer;
        return false;
    }

    NetworkGameInitData* params = new NetworkGameInitData();
    params->seed = 0;
    params->saveData = new LoadSaveDataThreadParam(saveBuffer, copied, wstring(worldName));
    return launchAndroidLocalWorld(params, worldName);
}

extern "C" void MclgAndroidInputTouch(float x, float y, int pressed)
{
    if (!g_androidGameStarted) return;
    std::lock_guard<std::mutex> lock(g_androidTouchMutex);
    g_androidTouchQueue.push_back(AndroidInputEvent{ANDROID_INPUT_TOUCH, x, y, 0, 0, 0, pressed});
}

extern "C" void MclgAndroidInputMouse(float x, float y, int button, int pressed)
{
    if (!g_androidGameStarted) return;
    std::lock_guard<std::mutex> lock(g_androidTouchMutex);
    g_androidTouchQueue.push_back(AndroidInputEvent{ANDROID_INPUT_MOUSE, x, y, 0, 0, button, pressed});
}

extern "C" void MclgAndroidInputKey(int keyCode, int unicode, int pressed)
{
    if (!g_androidGameStarted) return;
    std::lock_guard<std::mutex> lock(g_androidTouchMutex);
    g_androidTouchQueue.push_back(AndroidInputEvent{ANDROID_INPUT_KEY, 0.0f, 0.0f, keyCode, unicode, 0, pressed});
}
