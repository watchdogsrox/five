//
// filename:    NetworkScratchpad.cpp
// description: Scratchpads to store script data in between game modes
// written by:  John Gurney
//

#include "NetworkScratchpad.h"

#include "script/gta_thread.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetworkArrayScratchpad
////////////////////////////////////////////////////////////////////////////////////////////////////
CNetworkArrayScratchpad::CNetworkArrayScratchpad()
{
	Clear();
}

int  CNetworkArrayScratchpad::GetMaxSize()
{
	return ARRAY_SCRATCHPAD_SIZE;
}

int  CNetworkArrayScratchpad::GetCurrentSize()
{
	return m_arrayScratchPad.GetCurrentSize();
}

int CNetworkArrayScratchpad::GetScriptId()
{
	return m_scriptId;
}

bool CNetworkArrayScratchpad::IsFree()
{
	return (m_scriptId == -1);
}

void CNetworkArrayScratchpad::Clear()
{
	m_arrayScratchPad.Clear();
	m_scriptId = -1;
}

void CNetworkArrayScratchpad::SaveArray(u8* address, int size, int scriptId)
{
	gnetAssert(IsFree() || m_scriptId == scriptId);
	m_arrayScratchPad.SaveArray(address, size);
	m_scriptId = scriptId;
}

void CNetworkArrayScratchpad::RestoreArray(u8* address, int size, int NET_ASSERTS_ONLY(scriptId))
{
	gnetAssert(scriptId == m_scriptId);
	m_arrayScratchPad.RestoreArray(address, size);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetworkPlayerScratchpad
////////////////////////////////////////////////////////////////////////////////////////////////////
CNetworkPlayerScratchpad::CNetworkPlayerScratchpad()
 : m_scriptId(-1)
{
}

int CNetworkPlayerScratchpad::GetMaxSize()
{
	return PLAYER_SCRATCHPAD_SIZE;
}

int CNetworkPlayerScratchpad::GetCurrentSize(ActivePlayerIndex player)
{
	if (AssertVerify(player < MAX_NUM_ACTIVE_PLAYERS))
	{
		return m_playerScratchPad[player].GetCurrentSize();
	}

	return 0;
}

int CNetworkPlayerScratchpad::GetScriptId()
{
	return m_scriptId;
}

bool CNetworkPlayerScratchpad::IsFree()
{
	return (m_scriptId == -1);
}

void CNetworkPlayerScratchpad::Clear()
{
	m_scriptId = -1;

	for (int i=0; i<MAX_NUM_ACTIVE_PLAYERS; i++)
	{
		m_playerScratchPad[i].Clear();
	}
}

bool CNetworkPlayerScratchpad::IsFreeForPlayer(ActivePlayerIndex player)
{
	if (gnetVerify(player < MAX_NUM_ACTIVE_PLAYERS))
	{
		return m_playerScratchPad[player].IsFree();
	}

	return false;
}

void CNetworkPlayerScratchpad::ClearForPlayer(ActivePlayerIndex player)
{
	if (gnetVerify(player < MAX_NUM_ACTIVE_PLAYERS))
	{
		m_playerScratchPad[player].Clear();
	}

	bool bAllClear = true;

	for (int i=0; i<MAX_NUM_ACTIVE_PLAYERS; i++)
	{
		if (!m_playerScratchPad[i].IsFree())
		{
			bAllClear = false;
			break;
		}
	}

	if (bAllClear)
	{
		m_scriptId = -1;
	}
}

void CNetworkPlayerScratchpad::SaveArray(ActivePlayerIndex player, u8* address, int size, int scriptId)
{
	if (gnetVerify(IsFree() || m_scriptId == scriptId))
	{
		if (gnetVerify(player < MAX_NUM_ACTIVE_PLAYERS))
		{
			m_playerScratchPad[player].SaveArray(address, size);
		}

		m_scriptId = scriptId;
	}
}

void CNetworkPlayerScratchpad::RestoreArray(ActivePlayerIndex player, u8* address, int size, int scriptId)
{
	if (gnetVerify(m_scriptId == scriptId))
	{
		if (gnetVerify(player < MAX_NUM_ACTIVE_PLAYERS))
		{
			m_playerScratchPad[player].RestoreArray(address, size);
		}
	}
}

