#include "stdafx.h"

#if defined(__ANDROID__)

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
struct AndroidSubfile {
    unsigned int regionIndex;
    std::vector<unsigned char> bytes;
};

bool g_saveDisabled = false;
bool g_saveDeviceSelected[4] = {true, false, false, false};
std::wstring g_saveTitle = L"Minecraft";
std::string g_saveFilename = "world";
unsigned int g_saveSize = 0;
PVOID g_saveBuffer = nullptr;
SAVE_DETAILS g_saveDetails = {0, nullptr};
std::vector<SAVE_INFO> g_saveInfoList;
std::vector<unsigned char> g_thumbnail;
std::vector<AndroidSubfile> g_subfiles;
XMARKETPLACE_CONTENTOFFER_INFO g_emptyOffer{};
XCONTENT_DATA g_emptyContent{};
std::mutex g_storageMutex;

std::string safeComponent(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    for (unsigned char ch : input) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
            result.push_back(static_cast<char>(ch));
        } else if (ch == ' ' || ch == '.') {
            result.push_back('_');
        }
    }
    if (result.empty()) result = "world";
    if (result.size() >= MAX_SAVEFILENAME_LENGTH)
        result.resize(MAX_SAVEFILENAME_LENGTH - 1);
    return result;
}

std::string wideToAscii(const wchar_t* value) {
    std::string result;
    if (!value) return "world";
    for (const wchar_t* p = value; *p; ++p) {
        const wchar_t ch = *p;
        result.push_back(ch >= 32 && ch < 127 ? static_cast<char>(ch) : '_');
    }
    return safeComponent(result);
}

std::string savePath(const std::string& filename) {
    return std::string("Saves/") + safeComponent(filename) + ".mcs";
}
std::string regionPath(const std::string& filename) {
    return std::string("Saves/") + safeComponent(filename) + ".regions";
}

bool ensureDirectory(const std::string& path) {
    if (::mkdir(path.c_str(), 0770) == 0 || errno == EEXIST) {
        struct stat status{};
        return ::stat(path.c_str(), &status) == 0 && S_ISDIR(status.st_mode);
    }
    return false;
}

bool writeBytes(const std::string& path, const void* data, std::size_t size) {
    const std::size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && !ensureDirectory(path.substr(0, slash)))
        return false;
    std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!output) return false;
    if (size != 0 && data != nullptr)
        output.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    output.flush();
    return output.good();
}

bool readBytes(const std::string& path, std::vector<unsigned char>& output) {
    std::ifstream input(path.c_str(), std::ios::binary | std::ios::ate);
    if (!input) return false;
    const std::streamoff end = input.tellg();
    if (end < 0 || static_cast<unsigned long long>(end) > 0xffffffffULL)
        return false;
    output.resize(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    if (!output.empty())
        input.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(output.size()));
    return input.good() || input.eof();
}

std::string filenameFromInfo(PSAVE_INFO info) {
    if (!info || info->UTF8SaveFilename[0] == '\0')
        return safeComponent(g_saveFilename);
    return safeComponent(info->UTF8SaveFilename);
}

void refreshSaveInfoPointers() {
    g_saveDetails.iSaveC = static_cast<int>(g_saveInfoList.size());
    g_saveDetails.SaveInfoA = g_saveInfoList.empty() ? nullptr : g_saveInfoList.data();
}

void loadRegionFiles(const std::string& filename) {
    g_subfiles.clear();
    const std::string directory = regionPath(filename);
    DIR* dir = ::opendir(directory.c_str());
    if (!dir) return;
    while (dirent* entry = ::readdir(dir)) {
        if (entry->d_name[0] == '.') continue;
        const std::string name(entry->d_name);
        if (name.size() < 5 || name.substr(name.size() - 4) != ".bin")
            continue;
        char* end = nullptr;
        const unsigned long region = std::strtoul(name.c_str(), &end, 10);
        if (end == name.c_str() || *end != '.' || region > 0xffffffffUL)
            continue;
        AndroidSubfile subfile{};
        subfile.regionIndex = static_cast<unsigned int>(region);
        if (readBytes(directory + "/" + name, subfile.bytes))
            g_subfiles.push_back(std::move(subfile));
    }
    ::closedir(dir);
}

bool persistRegionFiles() {
    const std::string directory = regionPath(g_saveFilename);
    if (!ensureDirectory("Saves") || !ensureDirectory(directory))
        return false;
    bool success = true;
    for (const AndroidSubfile& subfile : g_subfiles) {
        const std::string path = directory + "/" + std::to_string(subfile.regionIndex) + ".bin";
        if (!writeBytes(path, subfile.bytes.empty() ? nullptr : subfile.bytes.data(),
                        subfile.bytes.size()))
            success = false;
    }
    return success;
}

void enumerateSaves() {
    g_saveInfoList.clear();
    if (!ensureDirectory("Saves")) {
        refreshSaveInfoPointers();
        return;
    }
    DIR* dir = ::opendir("Saves");
    if (!dir) {
        refreshSaveInfoPointers();
        return;
    }
    while (dirent* entry = ::readdir(dir)) {
        const std::string name(entry->d_name);
        if (name.size() <= 4 || name.substr(name.size() - 4) != ".mcs")
            continue;
        const std::string base = name.substr(0, name.size() - 4);
        const std::string safe = safeComponent(base);
        struct stat status{};
        const std::string path = savePath(safe);
        if (::stat(path.c_str(), &status) != 0 || !S_ISREG(status.st_mode))
            continue;
        SAVE_INFO info{};
        std::snprintf(info.UTF8SaveFilename, sizeof(info.UTF8SaveFilename), "%s", safe.c_str());
        std::snprintf(info.UTF8SaveTitle, sizeof(info.UTF8SaveTitle), "%s", safe.c_str());
        info.metaData.modifiedTime = status.st_mtime;
        info.metaData.dataSize = static_cast<unsigned int>(
            std::min<off_t>(status.st_size, 0xffffffffu));
        info.metaData.thumbnailSize = static_cast<unsigned int>(g_thumbnail.size());
        info.thumbnailData = g_thumbnail.empty() ? nullptr : g_thumbnail.data();
        g_saveInfoList.push_back(info);
    }
    ::closedir(dir);
    std::sort(g_saveInfoList.begin(), g_saveInfoList.end(),
        [](const SAVE_INFO& a, const SAVE_INFO& b) {
            return a.metaData.modifiedTime > b.metaData.modifiedTime;
        });
    refreshSaveInfoPointers();
}
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
    if (defaultName) {
        g_saveTitle = defaultName;
        g_saveFilename = wideToAscii(defaultName);
    }
    ensureDirectory("Saves");
}
void C4JStorage::ResetSaveData()
{
    std::lock_guard<std::mutex> lock(g_storageMutex);
    g_saveSize = 0;
    if (g_saveBuffer) {
        std::free(g_saveBuffer);
        g_saveBuffer = nullptr;
    }
    g_subfiles.clear();
}
void C4JStorage::SetDefaultSaveNameForKeyboardDisplay(LPCWSTR name)
{
    if (name) g_saveTitle = name;
}
void C4JStorage::SetSaveTitle(LPCWSTR title)
{
    if (title) {
        g_saveTitle = title;
        g_saveFilename = wideToAscii(title);
    }
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
    if (name) g_saveFilename = safeComponent(name);
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
    std::lock_guard<std::mutex> lock(g_storageMutex);
    void* replacement = bytes ? std::calloc(1, bytes) : nullptr;
    if (bytes != 0 && replacement == nullptr) {
        g_saveSize = 0;
        return nullptr;
    }
    if (g_saveBuffer) std::free(g_saveBuffer);
    g_saveBuffer = replacement;
    g_saveSize = replacement ? bytes : 0;
    return g_saveBuffer;
}
void C4JStorage::SetSaveImages(PBYTE thumbnail, DWORD thumbnailBytes,
                              PBYTE, DWORD, PBYTE, DWORD)
{
    g_thumbnail.clear();
    if (thumbnail && thumbnailBytes)
        g_thumbnail.assign(thumbnail, thumbnail + thumbnailBytes);
}
C4JStorage::ESaveGameState C4JStorage::SaveSaveData(
    int (*callback)(LPVOID, const bool), LPVOID parameter)
{
    bool success = false;
    {
        std::lock_guard<std::mutex> lock(g_storageMutex);
        success = ensureDirectory("Saves") &&
            writeBytes(savePath(g_saveFilename), g_saveBuffer, g_saveSize);
    }
    if (callback) callback(parameter, success);
    return ESaveGame_Idle;
}
void C4JStorage::CopySaveDataToNewSave(PBYTE, DWORD, WCHAR* newName,
                                        int (*callback)(LPVOID, bool), LPVOID parameter)
{
    if (newName) {
        g_saveTitle = newName;
        g_saveFilename = wideToAscii(newName);
    }
    const bool success = ensureDirectory("Saves") &&
        writeBytes(savePath(g_saveFilename), g_saveBuffer, g_saveSize);
    if (callback) callback(parameter, success);
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
    struct stat status{};
    const bool found = ::stat(savePath(g_saveFilename).c_str(), &status) == 0 &&
                       S_ISREG(status.st_mode);
    if (exists) *exists = found;
    return ESaveGame_Idle;
}
bool C4JStorage::EnoughSpaceForAMinSaveGame() { return true; }
void C4JStorage::SetSaveMessageVPosition(float) {}
C4JStorage::ESaveGameState C4JStorage::GetSavesInfo(
    int, int (*callback)(LPVOID, SAVE_DETAILS*, const bool), LPVOID parameter, char*)
{
    enumerateSaves();
    if (callback) callback(parameter, &g_saveDetails, true);
    return ESaveGame_GetSavesInfo;
}
PSAVE_DETAILS C4JStorage::ReturnSavesInfo()
{
    enumerateSaves();
    return &g_saveDetails;
}
void C4JStorage::ClearSavesInfo()
{
    g_saveInfoList.clear();
    refreshSaveInfoPointers();
}
C4JStorage::ESaveGameState C4JStorage::LoadSaveDataThumbnail(
    PSAVE_INFO, int (*callback)(LPVOID, PBYTE, DWORD), LPVOID parameter)
{
    if (callback) callback(parameter,
        g_thumbnail.empty() ? nullptr : g_thumbnail.data(),
        static_cast<DWORD>(g_thumbnail.size()));
    return ESaveGame_GetSaveThumbnail;
}
void C4JStorage::GetSaveCacheFileInfo(DWORD, XCONTENT_DATA& data)
{
    std::memset(&data, 0, sizeof(data));
}
void C4JStorage::GetSaveCacheFileInfo(DWORD, PBYTE* data, DWORD* bytes)
{
    if (data) *data = g_thumbnail.empty() ? nullptr : g_thumbnail.data();
    if (bytes) *bytes = static_cast<DWORD>(g_thumbnail.size());
}
C4JStorage::ESaveGameState C4JStorage::LoadSaveData(
    PSAVE_INFO info, int (*callback)(LPVOID, const bool, const bool), LPVOID parameter)
{
    const std::string filename = filenameFromInfo(info);
    std::vector<unsigned char> bytes;
    const bool readOk = readBytes(savePath(filename), bytes);
    if (readOk) {
        g_saveFilename = filename;
        g_saveSize = static_cast<unsigned int>(bytes.size());
        void* replacement = bytes.empty() ? nullptr : std::malloc(bytes.size());
        if (!bytes.empty() && replacement == nullptr) {
            g_saveSize = 0;
            if (callback) callback(parameter, false, false);
            return ESaveGame_Idle;
        }
        if (g_saveBuffer) std::free(g_saveBuffer);
        g_saveBuffer = replacement;
        if (!bytes.empty()) std::memcpy(g_saveBuffer, bytes.data(), bytes.size());
        loadRegionFiles(filename);
    }
    if (callback) callback(parameter, readOk, false);
    return ESaveGame_Idle;
}
C4JStorage::ESaveGameState C4JStorage::DeleteSaveData(
    PSAVE_INFO info, int (*callback)(LPVOID, const bool), LPVOID parameter)
{
    const std::string filename = filenameFromInfo(info);
    const bool removed = (::unlink(savePath(filename).c_str()) == 0 || errno == ENOENT);
    const std::string directory = regionPath(filename);
    DIR* dir = ::opendir(directory.c_str());
    if (dir) {
        while (dirent* entry = ::readdir(dir)) {
            if (entry->d_name[0] == '.') continue;
            ::unlink((directory + "/" + entry->d_name).c_str());
        }
        ::closedir(dir);
        ::rmdir(directory.c_str());
    }
    if (callback) callback(parameter, removed);
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

int C4JStorage::AddSubfile(unsigned int regionIndex)
{
    std::lock_guard<std::mutex> lock(g_storageMutex);
    for (std::size_t i = 0; i < g_subfiles.size(); ++i)
        if (g_subfiles[i].regionIndex == regionIndex) return static_cast<int>(i);
    g_subfiles.push_back(AndroidSubfile{regionIndex, {}});
    return static_cast<int>(g_subfiles.size() - 1);
}
unsigned int C4JStorage::GetSubfileCount()
{
    return static_cast<unsigned int>(g_subfiles.size());
}
void C4JStorage::GetSubfileDetails(unsigned int index, unsigned int* regionIndex,
                                  unsigned char** data, unsigned int* dataSize)
{
    if (regionIndex) *regionIndex = 0;
    if (data) *data = nullptr;
    if (dataSize) *dataSize = 0;
    if (index >= g_subfiles.size()) return;
    const AndroidSubfile& item = g_subfiles[index];
    if (regionIndex) *regionIndex = item.regionIndex;
    if (dataSize) *dataSize = static_cast<unsigned int>(item.bytes.size());
    if (data && !item.bytes.empty()) {
        *data = static_cast<unsigned char*>(std::malloc(item.bytes.size()));
        if (*data) std::memcpy(*data, item.bytes.data(), item.bytes.size());
        else if (dataSize) *dataSize = 0;
    }
}
void C4JStorage::ResetSubfiles()
{
    std::lock_guard<std::mutex> lock(g_storageMutex);
    g_subfiles.clear();
}
void C4JStorage::UpdateSubfile(int index, unsigned char* data, unsigned int dataSize)
{
    if (index < 0) return;
    std::lock_guard<std::mutex> lock(g_storageMutex);
    if (static_cast<std::size_t>(index) >= g_subfiles.size()) return;
    AndroidSubfile& item = g_subfiles[static_cast<std::size_t>(index)];
    if (!data || dataSize == 0) item.bytes.clear();
    else item.bytes.assign(data, data + dataSize);
}
C4JStorage::ESaveGameState C4JStorage::GetSaveState()
{
    return ESaveGame_Idle;
}
C4JStorage::ESaveGameState C4JStorage::SaveSubfiles(
    int (*callback)(LPVOID, const bool), LPVOID parameter)
{
    bool success = false;
    {
        std::lock_guard<std::mutex> lock(g_storageMutex);
        success = persistRegionFiles();
    }
    if (callback) callback(parameter, success);
    return ESaveGame_Idle;
}

#endif
