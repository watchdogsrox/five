//--------------------------------------------------------------------------------------
// CAutoLock.h
//
// Simple automatic Critical Section lock class
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#if RSG_DURANGO
#include <winnt.h>
#include <winbase.h>

//--------------------------------------------------------------------------------------
// CAutoLock class.
//--------------------------------------------------------------------------------------
class CAutoLock
{
public:

	CAutoLock( CRITICAL_SECTION& cs ) : m_cs( cs )
	{
		EnterCriticalSection( &m_cs );
	}

	~CAutoLock()
	{
		LeaveCriticalSection( &m_cs );
	}

	CAutoLock& operator=( CAutoLock& rhs )
	{
		m_cs = rhs.m_cs;
		return *this;
	}

private:

	CRITICAL_SECTION& m_cs;
};
#endif	//	RSG_DURANGO