// 
// audio/hashactiontable.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_HASHACTIONTABLE_H
#define AUD_HASHACTIONTABLE_H

#include "string/stringhash.h"
#include "atl/array.h"
#include "atl/map.h"

// PURPOSE
//	Action handler function pointer
// PARAMS
//	u32 hash - hashed action name
//	void *context - context specified to InvokeActionHandler()
typedef void (*ActionHandler)(u32,void *);

class audHashActionTable
{
public:
	audHashActionTable();
	~audHashActionTable();

	void AddActionHandler(const char *name, const ActionHandler handler);
	void AddActionHandler(const u32 hash, const ActionHandler handler);

	void SetDefaultActionHandler(ActionHandler handler);
	
	void InvokeActionHandler(const char *name, void *context);
	void InvokeActionHandler(u32 hash, void *context);

#if __DEV
	const char *FindActionName(const u32 hash);
#endif

private:

	ActionHandler m_DefaultHandler;
	atMap<u32, ActionHandler> m_ActionMap;
#if __DEV
public: atArray<const char *> m_EventNameList;
#endif
};

#endif // AUD_HASHACTIONTABLE_H
