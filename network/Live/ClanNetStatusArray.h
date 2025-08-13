//
// ClanNetStatusArray.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CLAN_NET_STATUS_ARRAY
#define CLAN_NET_STATUS_ARRAY

// rage
#include "atl/array.h"
#include "rline/clan/rlclan.h"

//////////////////////////////////////////////////////////////////////////
/// Simple wrapper to contain a status and an array
template <class _Type, int _MaxSize >
class ClanNetStatusArray
{
public:
	netStatus						m_RequestStatus;
	atRangeArray<_Type, _MaxSize>	m_Data;
	u32								m_iResultSize;
	u32								m_iTotalResultSize;

	ClanNetStatusArray() { Clear(); }
	~ClanNetStatusArray() {	if( m_RequestStatus.Pending() ) rlClan::Cancel(&m_RequestStatus);}

	const _Type& operator[](int index) const { return m_Data[index]; }
	_Type& operator[](int index)       { return m_Data[index]; }


	int GetMaxSize() const { return m_Data.GetMaxCount(); }
	void ForceSucceeded() 
	{
		m_iTotalResultSize = m_iResultSize = 0;
		m_RequestStatus.ForceSucceeded();
	}
	void Clear()
	{ 
		if( m_RequestStatus.Pending() ) 
		{
			rlClan::Cancel(&m_RequestStatus);
		}

		m_iTotalResultSize = 0;
		m_iResultSize = 0; 
		m_Data.empty();
		m_RequestStatus.Reset();
	}
};

#endif // CLAN_NET_STATUS_ARRAY

//eof
