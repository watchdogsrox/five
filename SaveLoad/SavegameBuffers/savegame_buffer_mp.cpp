

#include "savegame_buffer_mp.h"


// Rage headers
#include "parser/psofile.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/SavegameRtStructures/savegame_rtstructure_mp.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"

#include "SaveLoad/SavegameData/savegame_data_stats.h"


XPARAM(dont_load_script_data_from_mp_stats);


// ****************************** CSaveGameBuffer_MultiplayerCharacter ****************************************************

CSaveGameBuffer_MultiplayerCharacter::CSaveGameBuffer_MultiplayerCharacter(u32 MaxBufferSize)
	: CSaveGameBuffer(SAVEGAME_MULTIPLAYER_CHARACTER, MaxBufferSize, 
#if __MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
	false)
#else
	true)
#endif	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
{
	m_pRTStructureDataToBeSaved = rage_new CRTStructureDataToBeSaved_MultiplayerCharacter;
}

CSaveGameBuffer_MultiplayerCharacter::~CSaveGameBuffer_MultiplayerCharacter()
{
	delete m_pRTStructureDataToBeSaved;
	m_pRTStructureDataToBeSaved = NULL;
}

void CSaveGameBuffer_MultiplayerCharacter::Init(unsigned initMode)
{
	CSaveGameBuffer::Init(initMode);
}

void CSaveGameBuffer_MultiplayerCharacter::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{	//	I'll try this here for MP saves. They don't have the same restriction as SinglePlayer saves concerning
		//	the loaded buffer having to persist across the shutdown_session
		FreeBufferContainingContentsOfSavegameFile();
		FreeAllDataToBeSaved();
		m_BufferSize = 0;
	}

	CSaveGameBuffer::Shutdown(shutdownMode);
}

void CSaveGameBuffer_MultiplayerCharacter::FreeAllDataToBeSaved(bool bAssertIfDataHasntAlreadyBeenFreed)
{
	CSaveGameBuffer::FreeAllDataToBeSaved(bAssertIfDataHasntAlreadyBeenFreed);

#if !__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
#if SAVEGAME_USES_XML
	CScriptSaveData::DeleteAllRTStructures(SAVEGAME_MULTIPLAYER_CHARACTER);
#endif // SAVEGAME_USES_XML
#endif	//	!__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
}


#if SAVEGAME_USES_PSO
void CSaveGameBuffer_MultiplayerCharacter::ReadDataFromPsoFile(psoFile* psofile)
{
	psoStruct root = psofile->GetRootInstance();

#if __ALLOW_LOCAL_MP_STATS_SAVES
	if (CGenericGameStorage::GetOnlyReadTimestampFromMPSave())
	{
		LoadSingleObject<CPosixTimeStampForMultiplayerSaves>("Timestamp", root);
		delete psofile;
	}
	else
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
	{
		LoadSingleObject<CMultiplayerStatsSaveStructure>("Stats", root);
#if SAVE_MULTIPLAYER_FOG_OF_WAR
		LoadSingleObject<CMultiplayerFogOfWarSaveStructure>("FogOfWar", root);
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

#if __MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
		// no script data so delete the file right away
		delete psofile;
#else	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON

#if !__FINAL
		if (PARAM_dont_load_script_data_from_mp_stats.Get())
		{
			//	we're not going to attempt to read the script data so delete the file right away
			delete psofile;
		}
		else
#endif	//	!__FINAL
		{

#if __BANK
			bool bOriginalPrintValues = CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads;
			CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads = CGenericGameStorage::ms_bPrintScriptDataFromLoadedMpSaveFile;

			savegameDisplayf("CSaveGameBuffer_MultiplayerCharacter::ReadDataFromPsoFile - about to read script variables from MP savegame");
#endif	//	__BANK

			//		CScriptSaveData::ReadScriptTreesFromLoadedPso(SAVEGAME_MULTIPLAYER_CHARACTER, *psofile, GetBuffer());
			//		CScriptSaveData::DeserializeSavedData(SAVEGAME_MULTIPLAYER_CHARACTER);
			CScriptSaveData::ReadAndDeserializeAllScriptTreesFromLoadedPso(SAVEGAME_MULTIPLAYER_CHARACTER, *psofile);

#if __BANK
			savegameDisplayf("CSaveGameBuffer_MultiplayerCharacter::ReadDataFromPsoFile - finished reading script variables from MP savegame");

			CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads = bOriginalPrintValues;
#endif	//	__BANK

			//		if (savegameVerifyf(CScriptSaveData::HasAllScriptDataBeenDeserializedFromPsoFile(SAVEGAME_MULTIPLAYER_CHARACTER), "CSaveGameBuffer_MultiplayerCharacter::ReadDataFromPsoFile - expected all MP character script data to have been deserialized as soon as the load completed. Since that's not the case, we can't free the loaded buffer. This will only be freed when any remaining script data is deserialized"))
			{
				//		savegameAssertf(!m_bBufferContainsScriptData, "CSaveGameBuffer_MultiplayerCharacter::ReadDataFromPsoFile - expected the savegame to contain script data");
				// all script data has been read, so delete the file right away
				delete psofile;
			}
			// 		else
			// 		{
			// 			//		savegameAssertf(m_bBufferContainsScriptData, "CSaveGameBuffer_MultiplayerCharacter::ReadDataFromPsoFile - didn't expect the savegame to contain script data");
			// 			ReleaseBuffer(); // make sure we don't delete the PSO data just yet
			// 		}
		}

#endif	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
	}
}
#endif // SAVEGAME_USES_PSO


#if SAVEGAME_USES_XML
bool CSaveGameBuffer_MultiplayerCharacter::ReadDataFromXmlFile(fiStream* pReadStream)
{
	CMultiplayerStatsSaveStructure StatsDataToLoad;	
#if SAVE_MULTIPLAYER_FOG_OF_WAR
	CMultiplayerFogOfWarSaveStructure FogOfWarDataToLoad;
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

	parRTStructure rts("SaveGameData");
	rts.AddStructureInstance("Stats", StatsDataToLoad);
#if SAVE_MULTIPLAYER_FOG_OF_WAR
	rts.AddStructureInstance("FogOfWar", FogOfWarDataToLoad);
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

	bool result = LoadXmlTreeFromStream(pReadStream, rts);

	savegameDebugf2("CSaveGameBuffer_MultiplayerCharacter::ReadDataFromXmlFile - Done loading from stream '%s'", pReadStream->GetName());

	return result;
}

void CSaveGameBuffer_MultiplayerCharacter::AfterReadingFromXmlFile()
{
#if __MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON

	//	parManager::AutoUseTempMemory temp;

	DeleteEntireTree();	//	tree doesn't contain script data so we don't need to keep the nametable around. Delete the entire tree.

#else	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON

#if !__FINAL
	if (PARAM_dont_load_script_data_from_mp_stats.Get())
	{
		savegameAssertf(CScriptSaveData::HasAllScriptDataBeenDeserializedFromXmlFile(SAVEGAME_MULTIPLAYER_CHARACTER), "CSaveGameBuffer_MultiplayerCharacter::AfterReadingFromXmlFile - the game is running with -dont_load_script_data_from_mp_stats so expected the array of loaded trees to always be empty");

		DeleteEntireTree();	//	we're not going to read any script data so we don't need to keep the nametable around. Delete the entire tree.
	}
	else
#endif	//	!__FINAL
	{
		CScriptSaveData::DeserializeSavedData(SAVEGAME_MULTIPLAYER_CHARACTER);

		//	parManager::AutoUseTempMemory temp;

		if (savegameVerifyf(CScriptSaveData::HasAllScriptDataBeenDeserializedFromXmlFile(SAVEGAME_MULTIPLAYER_CHARACTER), "CSaveGameBuffer_MultiplayerCharacter::AfterReadingFromXmlFile - expected all MP character script data to have been deserialized as soon as the load completed. Since that's not the case, we can't delete the entire tree. This will only be deleted when any remaining script data is deserialized"))
		{
			DeleteEntireTree();	//	DeserializeSavedData() should have read all the script data that we're interested in so we don't need to keep the nametable around. Delete the entire tree.
		}
		else
		{
			DeleteRootNodeOfTree();	//	delete the root node but keep the tree in memory so that the nametable still exists
		}
	}

#endif	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
}
#endif	//	SAVEGAME_USES_XML


// ****************************** CSaveGameBuffer_MultiplayerCommon ****************************************************

CSaveGameBuffer_MultiplayerCommon::CSaveGameBuffer_MultiplayerCommon(u32 MaxBufferSize)
	: CSaveGameBuffer(SAVEGAME_MULTIPLAYER_COMMON, MaxBufferSize,
#if __MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
	true)
#else
	false)
#endif	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
{
	m_pRTStructureDataToBeSaved = rage_new CRTStructureDataToBeSaved_MultiplayerCommon;
}

CSaveGameBuffer_MultiplayerCommon::~CSaveGameBuffer_MultiplayerCommon()
{
	delete m_pRTStructureDataToBeSaved;
	m_pRTStructureDataToBeSaved = NULL;
}

void CSaveGameBuffer_MultiplayerCommon::Init(unsigned initMode)
{
	CSaveGameBuffer::Init(initMode);
}

void CSaveGameBuffer_MultiplayerCommon::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{	//	I'll try this here for MP saves. They don't have the same restriction as SinglePlayer saves concerning
		//	the loaded buffer having to persist across the shutdown_session
		FreeBufferContainingContentsOfSavegameFile();
		FreeAllDataToBeSaved();
		m_BufferSize = 0;
	}

	CSaveGameBuffer::Shutdown(shutdownMode);
}

void CSaveGameBuffer_MultiplayerCommon::FreeAllDataToBeSaved(bool bAssertIfDataHasntAlreadyBeenFreed)
{
	CSaveGameBuffer::FreeAllDataToBeSaved(bAssertIfDataHasntAlreadyBeenFreed);

#if __MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
#if SAVEGAME_USES_XML
	CScriptSaveData::DeleteAllRTStructures(SAVEGAME_MULTIPLAYER_COMMON);
#endif // SAVEGAME_USES_XML
#endif	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
}


#if SAVEGAME_USES_PSO
void CSaveGameBuffer_MultiplayerCommon::ReadDataFromPsoFile(psoFile* psofile)
{
	psoStruct root = psofile->GetRootInstance();

#if __ALLOW_LOCAL_MP_STATS_SAVES
	if (CGenericGameStorage::GetOnlyReadTimestampFromMPSave())
	{
		LoadSingleObject<CPosixTimeStampForMultiplayerSaves>("Timestamp", root);
		delete psofile;
	}
	else
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES
	{
		LoadSingleObject<CMultiplayerStatsSaveStructure>("Stats", root);

#if __MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON

#if !__FINAL
		if (PARAM_dont_load_script_data_from_mp_stats.Get())
		{
			//	we're not going to attempt to read the script data so delete the file right away
			delete psofile;
		}
		else
#endif	//	!__FINAL
		{

#if __BANK
			bool bOriginalPrintValues = CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads;
			CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads = CGenericGameStorage::ms_bPrintScriptDataFromLoadedMpSaveFile;

			savegameDisplayf("CSaveGameBuffer_MultiplayerCommon::ReadDataFromPsoFile - about to read script variables from MP savegame");
#endif	//	__BANK

			//		CScriptSaveData::ReadScriptTreesFromLoadedPso(SAVEGAME_MULTIPLAYER_COMMON, *psofile, GetBuffer());
			//		CScriptSaveData::DeserializeSavedData(SAVEGAME_MULTIPLAYER_COMMON);
			CScriptSaveData::ReadAndDeserializeAllScriptTreesFromLoadedPso(SAVEGAME_MULTIPLAYER_COMMON, *psofile);

#if __BANK
			savegameDisplayf("CSaveGameBuffer_MultiplayerCommon::ReadDataFromPsoFile - finished reading script variables from MP savegame");

			CGenericGameStorage::ms_bPrintValuesAsPsoFileLoads = bOriginalPrintValues;
#endif	//	__BANK

			//		if (savegameVerifyf(CScriptSaveData::HasAllScriptDataBeenDeserializedFromPsoFile(SAVEGAME_MULTIPLAYER_COMMON), "CSaveGameBuffer_MultiplayerCommon::ReadDataFromPsoFile - expected all MP script data to have been deserialized as soon as the load completed. Since that's not the case, we can't free the loaded buffer. This will only be freed when any remaining script data is deserialized"))
			{
				//		savegameAssertf(!m_bBufferContainsScriptData, "CSaveGameBuffer_MultiplayerCommon::ReadDataFromPsoFile - expected the savegame to contain script data");
				// all script data has been read, so delete the file right away
				delete psofile;
			}
			// 		else
			// 		{
			// 			//		savegameAssertf(m_bBufferContainsScriptData, "CSaveGameBuffer_MultiplayerCommon::ReadDataFromPsoFile - didn't expect the savegame to contain script data");
			// 			ReleaseBuffer(); // make sure we don't delete the PSO data just yet
			// 		}
		}

#else	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
		// no script data so delete the file right away
		delete psofile;
#endif	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
	}
}
#endif // SAVEGAME_USES_PSO


#if SAVEGAME_USES_XML
bool CSaveGameBuffer_MultiplayerCommon::ReadDataFromXmlFile(fiStream* pReadStream)
{
	CMultiplayerStatsSaveStructure StatsDataToLoad;	

	parRTStructure rts("SaveGameData");
	rts.AddStructureInstance("Stats", StatsDataToLoad);

	bool result = LoadXmlTreeFromStream(pReadStream, rts);

	savegameDebugf2("CSaveGameBuffer_MultiplayerCommon::ReadDataFromXmlFile - Done loading from stream '%s'", pReadStream->GetName());
	return result;
}

void CSaveGameBuffer_MultiplayerCommon::AfterReadingFromXmlFile()
{
#if __MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON

#if !__FINAL
	if (PARAM_dont_load_script_data_from_mp_stats.Get())
	{
		savegameAssertf(CScriptSaveData::HasAllScriptDataBeenDeserializedFromXmlFile(SAVEGAME_MULTIPLAYER_COMMON), "CSaveGameBuffer_MultiplayerCommon::AfterReadingFromXmlFile - the game is running with -dont_load_script_data_from_mp_stats so expected the array of loaded trees to always be empty");

		DeleteEntireTree();	//	we're not going to read any script data so we don't need to keep the nametable around. Delete the entire tree.
	}
	else
#endif	//	!__FINAL
	{
		CScriptSaveData::DeserializeSavedData(SAVEGAME_MULTIPLAYER_COMMON);

		//	parManager::AutoUseTempMemory temp;

		if (savegameVerifyf(CScriptSaveData::HasAllScriptDataBeenDeserializedFromXmlFile(SAVEGAME_MULTIPLAYER_COMMON), "CSaveGameBuffer_MultiplayerCommon::AfterReadingFromXmlFile - expected all MP script data to have been deserialized as soon as the load completed. Since that's not the case, we can't delete the entire tree. This will only be deleted when any remaining script data is deserialized"))
		{
			DeleteEntireTree();	//	DeserializeSavedData() should have read all the script data that we're interested in so we don't need to keep the nametable around. Delete the entire tree.
		}
		else
		{
			DeleteRootNodeOfTree();	//	delete the root node but keep the tree in memory so that the nametable still exists
		}
	}


#else	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
	//	parManager::AutoUseTempMemory temp;

	DeleteEntireTree();	//	tree doesn't contain script data so we don't need to keep the nametable around. Delete the entire tree.
#endif	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON
}
#endif	//	SAVEGAME_USES_XML


