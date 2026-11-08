#include "stdafx.h"

#if defined(__ANDROID__)

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
bool g_saveDisabled = false;
bool g_saveDeviceSelected[4] = {true, false, false, false};
std::wstring g_saveTitle = L"Minecraft";
std::string g_saveFilename = "world";
unsigned int g_saveSize = 0;
PVOID g_saveBuffer = nullptr;
SAVE_DETAILS g_emptySaveDetails = {0, nullptr};
XMARKETPLACE_CONTENTOFFER_INFO g_emptyOffer{};
XCONTENT_DATA g_emptyContent{};
}

C4JStorage StorageManager;

C4JStorage::C4JStorage() : m_pStringTable(nullptr) {}
void C4JStorage::Tick() {}
C4JStorage::EMessageResult C4JStorage::RequestMessageBox(
    UINT, UINT, UINT*, UINT, DWORD,
    int (*)(LPVOID, int, const C4JStorage::EMessageResult), LPVOID,
    C4JStringTable*, WCHAR*, DWORD)
{
    return EMessage_Cancelled;
}
C4JStorage::EMessageResult C4JStorage::GetMessageBoxResult() { return EMessage_Cancelled; }
bool C4JStorage::SetSaveDevice(int (*callback)(LPVOID, const bool), LPVOID parameter, bool)
{
    if (callback) callback(parameter, true);
    g_saveDeviceSelected[0] = true;
    return true;
}
void C4JStorage::Init(unsigned int, LPCWSTR defaultName, char*, int,
                      int (*)(LPVOID, const ESavingMessage, int), LPVOID, LPCSTR)
{
    if (defaultName) g_saveTitle = defaultName;
}
void C4JStorage::ResetSaveData()
{
    g_saveSize = 0;
    if (g_saveBuffer) { std::free(g_saveBuffer); g_saveBuffer = nullptr; }
}
void C4JStorage::SetDefaultSaveNameForKeyboardDisplay(LPCWSTR name)
{
    if (name) g_saveTitle = name;
}
void C4JStorage::SetSaveTitle(LPCWSTR title)
{
    if (title) g_saveTitle = title;
}
bool C4JStorage::GetSaveUniqueNumber(INT* value)
{
    if (value) *value = 0;
    return value != nullptr;
}
bool C4JStorage::GetSaveUniqueFilename(char* name)
{
    if (!name) return false;
    std::snprintf(name, MAX_SAVEFILENAME_LENGTH, "%s", g_saveFilename.c_str());
    return true;
}
void C4JStorage::SetSaveUniqueFilename(char* name)
{
    if (name) g_saveFilename = name;
}
void C4JStorage::SetState(ESaveGameControlState, int (*callback)(LPVOID, const bool), LPVOID parameter)
{
    if (callback) callback(parameter, true);
}
void C4JStorage::SetSaveDisabled(bool disabled) { g_saveDisabled = disabled; }
bool C4JStorage::GetSaveDisabled() { return g_saveDisabled; }
unsigned int C4JStorage::GetSaveSize() { return g_saveSize; }
void C4JStorage::GetSaveData(void* data, unsigned int* bytes)
{
    if (!bytes) return;
    const unsigned int count = std::min(*bytes, g_saveSize);
    if (data && g_saveBuffer && count) std::memcpy(data, g_saveBuffer, count);
    *bytes = count;
}
PVOID C4JStorage::AllocateSaveData(unsigned int bytes)
{
    if (g_saveBuffer) std::free(g_saveBuffer);
    g_saveBuffer = bytes ? std::calloc(1, bytes) : nullptr;
    g_saveSize = g_saveBuffer ? bytes : 0;
    return g_saveBuffer;
}
void C4JStorage::SetSaveImages(PBYTE, DWORD, PBYTE, DWORD, PBYTE, DWORD) {}
C4JStorage::ESaveGameState C4JStorage::SaveSaveData(int (*callback)(LPVOID, const bool), LPVOID parameter)
{
    if (callback) callback(parameter, true);
    return ESaveGame_Idle;
}
void C4JStorage::CopySaveDataToNewSave(PBYTE, DWORD, WCHAR*, int (*callback)(LPVOID, bool), LPVOID parameter)
{
    if (callback) callback(parameter, false);
}
void C4JStorage::SetSaveDeviceSelected(unsigned int pad, bool selected)
{
    if (pad < 4) g_saveDeviceSelected[pad] = selected;
}
bool C4JStorage::GetSaveDeviceSelected(unsigned int pad)
{
    return pad < 4 && g_saveDeviceSelected[pad];
}
C4JStorage::ESaveGameState C4JStorage::DoesSaveExist(bool* exists)
{
    if (exists) *exists = false;
    return ESaveGame_Idle;
}
bool C4JStorage::EnoughSpaceForAMinSaveGame() { return true; }
void C4JStorage::SetSaveMessageVPosition(float) {}
C4JStorage::ESaveGameState C4JStorage::GetSavesInfo(int, int (*callback)(LPVOID, SAVE_DETAILS*, const bool),
                                                    LPVOID parameter, char*)
{
    if (callback) callback(parameter, &g_emptySaveDetails, true);
    return ESaveGame_GetSavesInfo;
}
PSAVE_DETAILS C4JStorage::ReturnSavesInfo() { return &g_emptySaveDetails; }
void C4JStorage::ClearSavesInfo() {}
C4JStorage::ESaveGameState C4JStorage::LoadSaveDataThumbnail(
    PSAVE_INFO, int (*callback)(LPVOID, PBYTE, DWORD), LPVOID parameter)
{
    if (callback) callback(parameter, nullptr, 0);
    return ESaveGame_GetSaveThumbnail;
}
void C4JStorage::GetSaveCacheFileInfo(DWORD, XCONTENT_DATA& data) { std::memset(&data, 0, sizeof(data)); }
void C4JStorage::GetSaveCacheFileInfo(DWORD, PBYTE* data, DWORD* bytes)
{
    if (data) *data = nullptr;
    if (bytes) *bytes = 0;
}
C4JStorage::ESaveGameState C4JStorage::LoadSaveData(
    PSAVE_INFO, int (*callback)(LPVOID, const bool, const bool), LPVOID parameter)
{
    if (callback) callback(parameter, false, false);
    return ESaveGame_Idle;
}
C4JStorage::ESaveGameState C4JStorage::DeleteSaveData(
    PSAVE_INFO, int (*callback)(LPVOID, const bool), LPVOID parameter)
{
    if (callback) callback(parameter, false);
    return ESaveGame_Idle;
}
void C4JStorage::RegisterMarketplaceCountsCallback(int (*)(LPVOID, DLC_TMS_DETAILS*, int), LPVOID) {}
void C4JStorage::SetDLCPackageRoot(char*) {}
C4JStorage::EDLCStatus C4JStorage::GetDLCOffers(int, int (*)(LPVOID, int, DWORD, int), LPVOID, DWORD)
{
    return EDLC_NoOffers;
}
DWORD C4JStorage::CancelGetDLCOffers() { return 0; }
void C4JStorage::ClearDLCOffers() {}
XMARKETPLACE_CONTENTOFFER_INFO& C4JStorage::GetOffer(DWORD) { return g_emptyOffer; }
int C4JStorage::GetOfferCount() { return 0; }
DWORD C4JStorage::InstallOffer(int, __uint64*, int (*)(LPVOID, int, int), LPVOID, bool) { return 0; }
DWORD C4JStorage::GetAvailableDLCCount(int) { return 0; }
C4JStorage::EDLCStatus C4JStorage::GetInstalledDLC(int, int (*)(LPVOID, int, int), LPVOID)
{
    return EDLC_NoInstalledDLC;
}
XCONTENT_DATA& C4JStorage::GetDLC(DWORD) { return g_emptyContent; }
DWORD C4JStorage::MountInstalledDLC(int, DWORD, int (*)(LPVOID, int, DWORD, DWORD), LPVOID, LPCSTR) { return 0; }
DWORD C4JStorage::UnmountInstalledDLC(LPCSTR) { return 0; }
void C4JStorage::GetMountedDLCFileList(const char*, std::vector<std::string>& files) { files.clear(); }
std::string C4JStorage::GetMountedPath(std::string mount) { return mount; }
C4JStorage::ETMSStatus C4JStorage::ReadTMSFile(
    int, eGlobalStorage, eTMS_FileType, WCHAR*, BYTE** data, DWORD* size,
    int (*callback)(LPVOID, WCHAR*, int, bool, int), LPVOID parameter, int)
{
    if (data) *data = nullptr;
    if (size) *size = 0;
    if (callback) callback(parameter, nullptr, 0, false, 0);
    return ETMSStatus_Fail;
}
bool C4JStorage::WriteTMSFile(int, eGlobalStorage, WCHAR*, BYTE*, DWORD) { return false; }
bool C4JStorage::DeleteTMSFile(int, eGlobalStorage, WCHAR*) { return false; }
void C4JStorage::StoreTMSPathName(WCHAR*) {}
C4JStorage::ETMSStatus C4JStorage::TMSPP_ReadFile(
    int, eGlobalStorage, eTMS_FILETYPEVAL, LPCSTR,
    int (*callback)(LPVOID, int, int, PTMSPP_FILEDATA, LPCSTR), LPVOID parameter, int)
{
    if (callback) callback(parameter, 0, 0, nullptr, nullptr);
    return ETMSStatus_Fail;
}
unsigned int C4JStorage::CRC(unsigned char* data, int length)
{
    unsigned int crc = 0xffffffffu;
    if (!data || length <= 0) return crc ^ 0xffffffffu;
    for (int i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}

#endif
