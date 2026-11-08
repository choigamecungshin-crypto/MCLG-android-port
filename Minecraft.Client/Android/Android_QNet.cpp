#include "stdafx.h"

#if defined(__ANDROID__)

namespace {
QNET_STATE g_androidQnetState = QNET_STATE_IDLE;
bool g_androidQnetHasPlayer = false;
constexpr wchar_t kAndroidPlayerName[] = L"Player";

int playerIndex(const IQNetPlayer* player)
{
    for (int i = 0; i < 4; ++i)
        if (player == &IQNet::m_player[i]) return i;
    return -1;
}
}

IQNetPlayer IQNet::m_player[4];

BYTE IQNetPlayer::GetSmallId() { return static_cast<BYTE>(playerIndex(this) < 0 ? 0 : playerIndex(this)); }
void IQNetPlayer::SendData(IQNetPlayer*, const void*, DWORD, DWORD) {}
bool IQNetPlayer::IsSameSystem(IQNetPlayer* player) { return player == &IQNet::m_player[0]; }
DWORD IQNetPlayer::GetSendQueueSize(IQNetPlayer*, DWORD) { return 0; }
DWORD IQNetPlayer::GetCurrentRtt() { return 0; }
bool IQNetPlayer::IsHost() { return playerIndex(this) == 0; }
bool IQNetPlayer::IsGuest() { return false; }
bool IQNetPlayer::IsLocal() { return playerIndex(this) >= 0; }
PlayerUID IQNetPlayer::GetXuid() { return static_cast<PlayerUID>(playerIndex(this) + 1); }
LPCWSTR IQNetPlayer::GetGamertag() { return kAndroidPlayerName; }
int IQNetPlayer::GetSessionIndex() { return 0; }
bool IQNetPlayer::IsTalking() { return false; }
bool IQNetPlayer::IsMutedByLocalUser(DWORD) { return false; }
bool IQNetPlayer::HasVoice() { return false; }
bool IQNetPlayer::HasCamera() { return false; }
int IQNetPlayer::GetUserIndex() { return playerIndex(this); }
void IQNetPlayer::SetCustomDataValue(ULONG_PTR value) { m_customData = value; }
ULONG_PTR IQNetPlayer::GetCustomDataValue() { return m_customData; }

HRESULT IQNet::AddLocalPlayerByUserIndex(DWORD userIndex)
{
    if (userIndex != 0) return static_cast<HRESULT>(-1);
    g_androidQnetHasPlayer = true;
    return 0;
}
IQNetPlayer* IQNet::GetHostPlayer()
{
    return g_androidQnetHasPlayer ? &m_player[0] : nullptr;
}
IQNetPlayer* IQNet::GetLocalPlayerByUserIndex(DWORD userIndex)
{
    return userIndex == 0 && g_androidQnetHasPlayer ? &m_player[0] : nullptr;
}
IQNetPlayer* IQNet::GetPlayerByIndex(DWORD index)
{
    return index == 0 && g_androidQnetHasPlayer ? &m_player[0] : nullptr;
}
IQNetPlayer* IQNet::GetPlayerBySmallId(BYTE smallId)
{
    return smallId == 0 && g_androidQnetHasPlayer ? &m_player[0] : nullptr;
}
IQNetPlayer* IQNet::GetPlayerByXuid(PlayerUID xuid)
{
    return g_androidQnetHasPlayer && xuid == static_cast<PlayerUID>(1) ? &m_player[0] : nullptr;
}
DWORD IQNet::GetPlayerCount()
{
    return g_androidQnetHasPlayer && g_androidQnetState != QNET_STATE_IDLE ? 1u : 0u;
}
QNET_STATE IQNet::GetState() { return g_androidQnetState; }
bool IQNet::IsHost() { return g_androidQnetHasPlayer && g_androidQnetState != QNET_STATE_IDLE; }
HRESULT IQNet::JoinGameFromInviteInfo(DWORD, DWORD, const INVITE_INFO*) { return static_cast<HRESULT>(-1); }
void IQNet::HostGame()
{
    g_androidQnetHasPlayer = true;
    g_androidQnetState = QNET_STATE_GAME_PLAY;
}
void IQNet::EndGame()
{
    g_androidQnetState = QNET_STATE_IDLE;
    g_androidQnetHasPlayer = false;
}

#endif
