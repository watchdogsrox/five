// 
// audio/hashactiontable.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "hashactiontable.h"

AUDIO_OPTIMISATIONS()

audHashActionTable::audHashActionTable()
{
	m_DefaultHandler = NULL;
}

audHashActionTable::~audHashActionTable()
{
	m_ActionMap.Kill();
}

void audHashActionTable::AddActionHandler(const char *name, const ActionHandler handler)
{
#if __DEV
	m_EventNameList.Grow() = StringDuplicate(name);
#endif
	AddActionHandler(atStringHash(name), handler);
}

void audHashActionTable::AddActionHandler(const u32 hash, const ActionHandler handler)
{
	m_ActionMap.Insert(hash, handler);
}

void audHashActionTable::InvokeActionHandler(const char *name, void *context)
{
	InvokeActionHandler(atStringHash(name), context);
}

void audHashActionTable::InvokeActionHandler(u32 hash, void *context)
{
	ActionHandler *handler = m_ActionMap.Access(hash);
	if(handler)
	{
		ActionHandler f = *handler;
		if(f)
		{
			f(hash,context);
		}
	}
	else if(m_DefaultHandler)
	{
		m_DefaultHandler(hash,context);
	}
}

void audHashActionTable::SetDefaultActionHandler(ActionHandler handler)
{
	m_DefaultHandler = handler;
}

#if __DEV
const char *audHashActionTable::FindActionName(const u32 hash)
{
	for(s32 i = 0; i < m_EventNameList.GetCount(); i++)
	{
		if(atStringHash(m_EventNameList[i])==hash)
		{
			return m_EventNameList[i];
		}
	}
	return NULL;
}
#endif

