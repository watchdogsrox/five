//
// name:        NetworkScratchpad.h
// description: Scratchpads to store script data in between game modes
// written by:  John Gurney
//

#ifndef NETWORK_SCRATCHPAD_H
#define NETWORK_SCRATCHPAD_H

// framework includes
#include "fwnet/nettypes.h"
#include "fwscript/scriptId.h"

class GtaThread;

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetworkScratchpad
////////////////////////////////////////////////////////////////////////////////////////////////////

template<int MaxSize>
class CNetworkScratchpad
{
public:

	CNetworkScratchpad()
	{
		Clear();
	}

	int  GetMaxSize()
	{
		return MaxSize;
	}

	int  GetCurrentSize()
	{
		return m_currentSize;
	}

	bool IsFree()
	{
		return (m_currentSize == 0);
	}

	void Clear()
	{
		m_currentSize = 0;
	}

	void SaveArray(u8 *Address, int size)
	{
		gnetAssert(size < MaxSize);

		m_currentSize = MIN(size, MaxSize);

		sysMemCpy(m_scratchPad, Address, m_currentSize);
	}

	void RestoreArray(u8 *Address, int size)
	{
		gnetAssert(size <= m_currentSize);

		m_currentSize = MIN(size, m_currentSize);

		sysMemCpy(Address, m_scratchPad, m_currentSize);
	}

protected:

	u8		m_scratchPad[MaxSize];
	int		m_currentSize;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetworkArrayScratchpad
////////////////////////////////////////////////////////////////////////////////////////////////////
class CNetworkArrayScratchpad
{
public:

	CNetworkArrayScratchpad();

	int  GetMaxSize();
	int  GetCurrentSize();

	int  GetScriptId();
	bool IsFree();
	void Clear();

	void SaveArray(u8 *Address, int Size, int scriptId);
	void RestoreArray(u8 *Address, int Size, int scriptId);

protected:

	static const int ARRAY_SCRATCHPAD_SIZE = 256;

	CNetworkScratchpad<	ARRAY_SCRATCHPAD_SIZE>	m_arrayScratchPad;
	int	m_scriptId; // an id assigned by the script using the scratchpad
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetworkPlayerScratchpad
////////////////////////////////////////////////////////////////////////////////////////////////////
class CNetworkPlayerScratchpad
{
public:

	CNetworkPlayerScratchpad();

	int  GetMaxSize();
	int  GetCurrentSize(ActivePlayerIndex player);

	int  GetScriptId();
	bool IsFree();
	void Clear();

	bool IsFreeForPlayer(ActivePlayerIndex player);
	void ClearForPlayer(ActivePlayerIndex player);

	void SaveArray(ActivePlayerIndex player, u8 *Address, int Size, int scriptId);
	void RestoreArray(ActivePlayerIndex player, u8 *Address, int Size, int scriptId);

protected:

	static const int PLAYER_SCRATCHPAD_SIZE = 32;

	CNetworkScratchpad<	PLAYER_SCRATCHPAD_SIZE>	m_playerScratchPad[MAX_NUM_ACTIVE_PLAYERS];
	int	m_scriptId; // an id assigned by the script using the scratchpad
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CNetworkScratchpads
////////////////////////////////////////////////////////////////////////////////////////////////////

class CNetworkScratchpads
{
public:

	CNetworkArrayScratchpad& GetArrayScratchpad(int num)
	{
		gnetAssert(num < NUM_SCRATCHPADS);
		return m_arrayScratchpads[num];
	}

	CNetworkPlayerScratchpad& GetPlayerScratchpad(int num)
	{
		gnetAssert(num < NUM_SCRATCHPADS);
		return m_playerScratchpads[num];
	}

	int GetNumScratchpads() { return NUM_SCRATCHPADS; }

protected:

	static const int NUM_SCRATCHPADS = 4;

	CNetworkArrayScratchpad m_arrayScratchpads[NUM_SCRATCHPADS];
	CNetworkPlayerScratchpad m_playerScratchpads[NUM_SCRATCHPADS];
};

#endif  // #NETWORK_SCRATCHPAD_H
