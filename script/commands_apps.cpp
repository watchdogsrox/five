// Rage headers
#include "atl/map.h"
#include "atl/string.h"
#include "script/wrapper.h"
#include "system/nelem.h"

// Game headers
#include "apps/appdata.h"
#include "script/script_channel.h"

SCRIPT_OPTIMISATIONS()

struct FetchedData
{
	FetchedData();

	atString m_AppName;

	bool m_Valid;
};

FetchedData::FetchedData()
: m_Valid(false)
, m_AppName(atString(""))
{
}

static FetchedData g_FetchedData;

namespace apps_commands
{
	CAppDataBlock* GetCurrentBlock()
	{
		CAppData* appData = CAppDataMgr::GetAppData(g_FetchedData.m_AppName);

		return appData->GetBlock();
	}

	bool HasSetAppAndBlock()
	{
		scriptAssertf(g_FetchedData.m_AppName.GetLength() != 0, "App has not been set before script command %s was called", scrThread::GetCurrentCmdName());
		scriptAssertf(GetCurrentBlock(), "Block has not been set before script command %s was called", scrThread::GetCurrentCmdName());

		if(g_FetchedData.m_AppName.GetLength() == 0 || !GetCurrentBlock())
		{
			return false;
		}

		return true;
	}

	bool CommandDataValid()
	{
		if( !HasSetAppAndBlock() )
		{
			return false;
		}

		CAppData* appData = CAppDataMgr::GetAppData(g_FetchedData.m_AppName);

		if( appData == NULL || appData->IsModified() )
		{
			return false;
		}

		CAppDataBlock * block = GetCurrentBlock();

		if( block == NULL )
		{
			return false;
		}

		return true;
	}

	int CommandGetInt(const char * name)
	{
		if( !HasSetAppAndBlock() )
		{
			return 0;
		}

		CAppDataBlock * block = GetCurrentBlock();

		if( !block )
		{
			return 0;
		}

		int value = 0;		
		if( !block->GetInt(name, &value) )
		{
			return 0;
		}

		return value;
	}

	float CommandGetFloat(const char * name)
	{
		if( !HasSetAppAndBlock() )
		{
			return 0;
		}

		CAppDataBlock * block = GetCurrentBlock();

		if( !block )
		{
			return 0;
		}

		float value = 0.0f;		
		if( !block->GetFloat(name, &value) )
		{
			return 0.0f;
		}

		return value;
	}

	const char* CommandGetString(const char * name)
	{
		if( !HasSetAppAndBlock() )
		{
			return "";
		}

		CAppDataBlock * block = GetCurrentBlock();

		if( !block )
		{
			return "";
		}

		const char *value = "";		
		if( !block->GetString(name, &value) )
		{
			return "";
		}

		return value;
	}

	//HasSetAppAndBlock will deal with the asserts
	void CommandSetInt(const char * name, int value)
	{
		if( !HasSetAppAndBlock() )
		{
			return;
		}

		CAppDataBlock * block = GetCurrentBlock();

		if( !block )
		{
			return;
		}

		if( block->SetInt( atString(name), value ) )
		{
			CAppData* appData = CAppDataMgr::GetAppData(g_FetchedData.m_AppName);
			appData->SetModified(true);
		}
	}

	void CommandSetFloat(const char * name, float value)
	{
		if( !HasSetAppAndBlock() )
		{
			return;
		}

		CAppDataBlock * block = GetCurrentBlock();

		if( !block )
		{
			return;
		}

		if( block->SetFloat( atString(name), value ) )
		{
			CAppData* appData = CAppDataMgr::GetAppData(g_FetchedData.m_AppName);
			appData->SetModified(true);
		}
	}

	void CommandSetString(const char * name, const char* value )
	{		
		if( !HasSetAppAndBlock() )
		{
			return;
		}

		CAppDataBlock * block = GetCurrentBlock();

		if( !block )
		{
			return;
		}

		if( block->SetString( atString(name), atString(value) ) )
		{
			CAppData* appData = CAppDataMgr::GetAppData(g_FetchedData.m_AppName);
			appData->SetModified(true);
		}
	}

	void CommandSetApp(const char * name)
	{
		if( g_FetchedData.m_AppName.c_str() == name )
		{
			return;
		}

		scriptAssertf(g_FetchedData.m_AppName.GetLength() == 0, "App has already been set to %s", g_FetchedData.m_AppName.c_str());

		if(g_FetchedData.m_AppName.GetLength() != 0)
		{
			return;
		}

		g_FetchedData.m_AppName = atString(name);
	}

	void CommandCloseApp()
	{
		g_FetchedData.m_AppName.Clear();
	}

	void CommandCloseBlock()
	{
		if (!HasSetAppAndBlock())
		{
			return;
		}

		CAppData* appData = CAppDataMgr::GetAppData(g_FetchedData.m_AppName);

		appData->CloseBlock();
	}

	void CommandSetBlock(const char * blockName)
	{
		scriptAssertf(g_FetchedData.m_AppName.GetLength() != 0, "App has not been set before script command %s was called", scrThread::GetCurrentCmdName());

		if (scriptVerifyf(blockName != nullptr && blockName[0] != '\0', "Block name is empty"))
		{
			CAppDataMgr::GetAppData(g_FetchedData.m_AppName)->SetBlock(blockName);
		}
	}

	void CommandSaveApp()
	{
		scriptAssertf(g_FetchedData.m_AppName.GetLength() != 0, "App has not been set before script command %s was called", scrThread::GetCurrentCmdName());

		if( g_FetchedData.m_AppName.GetLength() == 0 )
		{
			return;
		}

		CAppDataMgr::PushFile(g_FetchedData.m_AppName);
	}

	appDataFileStatus CommandGetAppFileStatus()
	{
		scriptAssertf(g_FetchedData.m_AppName.GetLength() != 0, "App has not been set before script command %s was called", scrThread::GetCurrentCmdName());

		if( g_FetchedData.m_AppName.GetLength() == 0 )
		{
			return APP_FILE_STATUS_FAILED;
		}

		return APP_FILE_STATUS_NONE;
	}

	appDataFileStatus CommandGetDeletedFileStatus()
	{
		return CAppDataMgr::GetDeletedFileStatus();
	}

	bool CommandDeleteAppData(const char * appName)
	{
		return CAppDataMgr::DeleteCloudData(appName, "app");
	}

	void CommandClearBlock()
	{
		if( !HasSetAppAndBlock() )
		{
			return;
		}

		CAppDataBlock * block = GetCurrentBlock();

		if( block )
		{
			block->ClearMembers();
		}
	}

	bool CommandIsLinkedToSocialClub()
	{
		return CAppDataMgr::IsLinkedToSocialClub();
	}

	bool CommandHasSyncedAppData(const char * appName)
	{
		CAppData* appData = CAppDataMgr::GetAppData(appName);

		if( appData == NULL )
		{
			return false;
		}

		return appData->HasSyncedData();
	}

	/* -----------------------------------------------------------------------------------------------------------------------*/

	// Register commands
	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(APP_DATA_VALID,0x22c6b747bc6e361e, CommandDataValid);
		SCR_REGISTER_SECURE(APP_GET_INT,0x5038830517cd70f6, CommandGetInt);
		SCR_REGISTER_SECURE(APP_GET_FLOAT,0x4c7cf7912c241e6b, CommandGetFloat);
		SCR_REGISTER_SECURE(APP_GET_STRING,0x2d15550f7fcd5086, CommandGetString);
		//@@: location APP_COMMANDS_SETUPSCRIPTCOMMANDS
		SCR_REGISTER_SECURE(APP_SET_INT,0x1c02fd23c1547cf8, CommandSetInt);
		SCR_REGISTER_SECURE(APP_SET_FLOAT,0x268821bcc02b91e9, CommandSetFloat);
		SCR_REGISTER_SECURE(APP_SET_STRING,0xe1adb2de0b8174ae, CommandSetString);

		SCR_REGISTER_SECURE(APP_SET_APP,0x1da152b5eac0cc1f, CommandSetApp);
		SCR_REGISTER_SECURE(APP_SET_BLOCK,0xff1067af9210d60e, CommandSetBlock);
		SCR_REGISTER_SECURE(APP_CLEAR_BLOCK,0xbe6c25a9cd239f04, CommandClearBlock);
		SCR_REGISTER_SECURE(APP_CLOSE_APP,0x9daa3c517c0e9456, CommandCloseApp);
		SCR_REGISTER_SECURE(APP_CLOSE_BLOCK,0xeaf291b4eace0e17, CommandCloseBlock);
		SCR_REGISTER_SECURE(APP_HAS_LINKED_SOCIAL_CLUB_ACCOUNT,0x0fecf87f208d8eb4, CommandIsLinkedToSocialClub);

		SCR_REGISTER_SECURE(APP_HAS_SYNCED_DATA,0x4325115cea2ae76c, CommandHasSyncedAppData);
		SCR_REGISTER_SECURE(APP_SAVE_DATA,0x7fb1b1a8b6c93051, CommandSaveApp);
		SCR_REGISTER_SECURE(APP_GET_DELETED_FILE_STATUS,0x9b1f0a416059fb72, CommandGetDeletedFileStatus);
		SCR_REGISTER_UNUSED(APP_GET_APP_FILE_STATUS,0x5ae69b6f236b12c8, CommandGetAppFileStatus);
		SCR_REGISTER_SECURE(APP_DELETE_APP_DATA,0x0b75c472b38530fb, CommandDeleteAppData);
	}
} // end of namespace apps_commands
