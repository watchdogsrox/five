//
// Task/Weapons/Gun/Metadata/ScriptedGunTaskInfoMetadataMgr.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_SCRIPTED_GUN_TASK_INFO_METADATA_MGR_H
#define INC_SCRIPTED_GUN_TASK_INFO_METADATA_MGR_H

////////////////////////////////////////////////////////////////////////////////

// Rage Headers
#include "atl/array.h"
#include "bank/bank.h"
#include "bank/button.h"
#include "bank/combo.h"
#include "parser/macros.h"

////////////////////////////////////////////////////////////////////////////////

// Forward Declarations
class CScriptedGunTaskInfo;

/////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Scripted gun task data driven info
////////////////////////////////////////////////////////////////////////////////

class CScriptedGunTaskMetadataMgr
{
public:

	// PURPOSE: Initialise scripted gun task info pool and load file
	static void Init(unsigned initMode);

	// PURPOSE: Clear scripted gun task infos and shutdown pool
	static void Shutdown(unsigned shutdownMode);

	// PURPOSE: Get a scripted gun task info by hash
	static const CScriptedGunTaskInfo* GetScriptedGunTaskInfoByHash(u32 uHash);

	// PURPOSE: Get a scripted gun task info by index
	static const CScriptedGunTaskInfo* GetScriptedGunTaskInfoByIndex(u32 uIndex);

	// PURPOSE: For determining the scripted gun task info hash
	static u32 GetScriptedGunTaskInfoHashFromIndex(s32 iIndex);

	// PURPOSE: Get the instance of the metadata manager
	static CScriptedGunTaskMetadataMgr& GetInstance() { return ms_Instance; }

	// PURPOSE: Get the number of scripted gun task infos
	static u32 GetNumberOfScriptedGunTaskInfos() { return ms_Instance.m_ScriptedGunTaskInfos.GetCount(); }

	// PURPOSE: Constructor
	CScriptedGunTaskMetadataMgr();

	// PURPOSE: Destructor
	~CScriptedGunTaskMetadataMgr();

	bool GetIsInitialised() const { return m_bIsInitialised; }

private:

	// PURPOSE: Clear all infos in memory
	static void Reset();

	// PURPOSE: Load all infos from file
	static void Load();

	// PURPOSE: Static instance of metatdata manager
	static CScriptedGunTaskMetadataMgr ms_Instance;

	// PURPOSE: Scripted gun task info array
	atArray<CScriptedGunTaskInfo*> m_ScriptedGunTaskInfos;

	// PURPOSE: Store whether manager has been initialised
	bool m_bIsInitialised;

	PAR_SIMPLE_PARSABLE

#if __BANK
public:

	// PURPOSE: Save metadata to file
	static void Save();

	// PURPOSE: Add widgets to RAG
	static void AddWidgets(bkBank& bank);

#endif
};

/////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Scripted gun task debug
////////////////////////////////////////////////////////////////////////////////

#if __BANK
class CScriptedGunTaskDebug
{
public:

	// PURPOSE: Create the bank in RAG
	static void InitBank();

	// PURPOSE: Shutdown the bank
	static void	ShutdownBank(); 

	// PURPOSE: Add widgets to RAG
	static void	AddWidgets();

	// PURPOSE: Gives a ped a scripted gun task
	static void GivePedTaskCB();

private:

	static bkBank* ms_pBank;
	static bkButton* ms_pCreateButton;
	static bkCombo*	ms_pTasksCombo;

	static s32 ms_iSelectedGunTask;

	static atArray<const char*> ms_ScriptedGunTaskNames;		
};
#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////

#endif // INC_SCRIPTED_GUN_TASK_INFO_METADATA_MGR_H
