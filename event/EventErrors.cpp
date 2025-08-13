//
// scriptEventTypes.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

// --- Include Files ------------------------------------------------------------

#include "event/EventErrors.h"
#include "peds/ped.h"

#include "script/script.h"


CEventErrorUnknownError::CEventErrorUnknownError(void)
{
	m_EventData.m_scriptNameHash=0;
}

CEventErrorStackOverflow::CEventErrorStackOverflow(void)
{
	m_EventData.m_scriptNameHash=0;
}

CEventErrorArrayOverflow::CEventErrorArrayOverflow(void)
{
	m_EventData.m_scriptNameHash=0;
}

CEventErrorInstructionLimit::CEventErrorInstructionLimit(void)
{
	m_EventData.m_scriptNameHash=0;
}

