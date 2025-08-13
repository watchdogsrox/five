// 
// audio/radiostationhistory.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_RADIOSTATIONHISTORY_H
#define AUD_RADIOSTATIONHISTORY_H

#include "atl/array.h"
#include "system/memops.h"

class audRadioStationHistory
{
public:
	enum {kRadioHistoryLength = 15};

	audRadioStationHistory()
		: m_WriteIndex(0)
	{
		Reset();
	}

	void Reset()
	{
		sysMemSet(&m_History, 0xff, sizeof(m_History));
		m_WriteIndex = 0;
	}

	void AddToHistory(const u32 id)
	{
		if(m_History[(m_WriteIndex + kRadioHistoryLength - 1) % kRadioHistoryLength] != id)
		{
			m_History[m_WriteIndex] = id;
			m_WriteIndex = (m_WriteIndex + 1) % kRadioHistoryLength;
		}	
	}

	u32 GetPreviousEntry() const
	{
		s32 previousIndex = (m_WriteIndex + kRadioHistoryLength - 1) % kRadioHistoryLength;
		return m_History[previousIndex];
	}

	bool IsInHistory(const u32 id, const s32 historyLength) const
	{
		const s32 searchLength = Clamp<s32>(historyLength, 0, kRadioHistoryLength);
		for(s32 i = 0; i < searchLength; i++)
		{
			s32 historyIndex = (m_WriteIndex + kRadioHistoryLength - 1 - i) % kRadioHistoryLength;
			if(m_History[historyIndex] == id)
			{
				return true;
			}
		}
		return false;
	}

	s32 GetWriteIndex() const { return m_WriteIndex; }
	void SetWriteIndex(const s32 writeIndex) { m_WriteIndex = writeIndex % kRadioHistoryLength; }

	u32 &operator[](const s32 index)
	{
		return m_History[index];
	}

	const u32 &operator[](const s32 index) const
	{
		return m_History[index];
	}

private:
	atRangeArray<u32, kRadioHistoryLength> m_History;
	s32 m_WriteIndex;
};

#endif // AUD_RADIOSTATIONHISTORY_H
