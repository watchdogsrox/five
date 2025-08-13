
#include "savegame_buffer_sp.h"


// Rage headers
#include "parser/psofile.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/SavegameRtStructures/savegame_rtstructure_sp.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"

#include "SaveLoad/SavegameData/savegame_data_camera.h"
#include "SaveLoad/SavegameData/savegame_data_doors.h"
#include "SaveLoad/SavegameData/savegame_data_extra_content.h"
#include "SaveLoad/SavegameData/savegame_data_garages.h"
#include "SaveLoad/SavegameData/savegame_data_mini_map.h"
#include "SaveLoad/SavegameData/savegame_data_player_ped.h"
#include "SaveLoad/SavegameData/savegame_data_radio_stations.h"
#include "SaveLoad/SavegameData/savegame_data_simple_variables.h"
#include "SaveLoad/SavegameData/savegame_data_stats.h"
#include "SaveLoad/SavegameData/savegame_data_stunt_jumps.h"


// ****************************** CSaveGameBuffer_SinglePlayer ****************************************************

CSaveGameBuffer_SinglePlayer::CSaveGameBuffer_SinglePlayer(u32 MaxBufferSize)
	: CSaveGameBuffer(SAVEGAME_SINGLE_PLAYER, MaxBufferSize, true)
{
	m_pRTStructureDataToBeSaved = rage_new CRTStructureDataToBeSaved_SinglePlayer;
}

CSaveGameBuffer_SinglePlayer::~CSaveGameBuffer_SinglePlayer()
{
	delete m_pRTStructureDataToBeSaved;
	m_pRTStructureDataToBeSaved = NULL;
}

void CSaveGameBuffer_SinglePlayer::Init(unsigned initMode)
{
	CSaveGameBuffer::Init(initMode);

	if (initMode == INIT_AFTER_MAP_LOADED)
	{
		//	The buffer is freed here rather than in CSaveGameBuffer_SinglePlayer::Shutdown(SHUTDOWN_SESSION) to prevent 
		//		m_BufferContainingContentsOfSavegameFile of CGenericGameStorage::ms_SaveGameBuffer 
		//		being wiped before it is read during a load.
		FreeBufferContainingContentsOfSavegameFile();
		FreeAllDataToBeSaved();
		m_BufferSize = 0;
	}
}

void CSaveGameBuffer_SinglePlayer::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{
		//	Don't want to call FreeAllData here as that would lead to m_BufferContainingContentsOfSavegameFile of 
		//	CGenericGameStorage::ms_SaveGameBuffer being wiped before it is read during a load.
		//	The buffer is freed in CSaveGameBuffer_SinglePlayer::Init(INIT_AFTER_MAP_LOADED) instead.
		FreeAllDataToBeSaved();
	}

	CSaveGameBuffer::Shutdown(shutdownMode);
}

void CSaveGameBuffer_SinglePlayer::FreeAllDataToBeSaved(bool bAssertIfDataHasntAlreadyBeenFreed)
{
	CSaveGameBuffer::FreeAllDataToBeSaved(bAssertIfDataHasntAlreadyBeenFreed);
#if SAVEGAME_USES_XML
	CScriptSaveData::DeleteAllRTStructures(SAVEGAME_SINGLE_PLAYER);
#endif // SAVEGAME_USES_XML
}




#if SAVEGAME_USES_PSO
void CSaveGameBuffer_SinglePlayer::ReadDataFromPsoFile(psoFile* psofile)
{
	psoStruct root = psofile->GetRootInstance();
	LoadSingleObject<CSimpleVariablesSaveStructure>("SimpleVariables", root);
	LoadSingleObject<CMiniMapSaveStructure>("MiniMap", root);
	LoadSingleObject<CPlayerPedSaveStructure>("PlayerPed", root);
	LoadSingleObject<CExtraContentSaveStructure>("ExtraContent", root);
	LoadSingleObject<CSaveGarages>("Garages", root);
	LoadSingleObject<CStatsSaveStructure>("Stats", root);
	LoadSingleObject<CStuntJumpSaveStructure>("StuntJumps", root);
	LoadSingleObject<CRadioStationSaveStructure>("RadioStations", root);
	LoadSingleObject<CSaveDoors>("Doors", root);
	LoadSingleObject<CCameraSaveStructure>("Camera", root);

	if (CScriptSaveData::ReadScriptTreesFromLoadedPso(SAVEGAME_SINGLE_PLAYER, *psofile, GetBuffer()))
	{
		ReleaseBuffer(); // make sure we don't delete the PSO data just yet
	}
	else
	{
		savegameAssertf(0, "CSaveGameBuffer_SinglePlayer::ReadDataFromPsoFile - expected the savegame to contain script data");
		// no structs got added, so delete the file right away
		delete psofile;
	}
}
#endif // SAVEGAME_USES_PSO


#if SAVEGAME_USES_XML
bool CSaveGameBuffer_SinglePlayer::ReadDataFromXmlFile(fiStream* pReadStream)
{
	CSimpleVariablesSaveStructure simpleVariablesToLoad;	//	simpleVarsCurrent, 
	CMiniMapSaveStructure MiniMapDataToLoad;
	CPlayerPedSaveStructure playerPedToLoad;
	CExtraContentSaveStructure ExtraContentDataToLoad;
	CStatsSaveStructure StatsDataToLoad;
	CStuntJumpSaveStructure StuntJumpDataToLoad;
	CRadioStationSaveStructure RadioStationDataToLoad;
	CSaveGarages GarageDataToLoad;
	CSaveDoors DoorsToLoad;
	CCameraSaveStructure CameraDataToLoad;

	parRTStructure rts("SaveGameData");
	rts.AddStructureInstance("SimpleVariables", simpleVariablesToLoad);
	rts.AddStructureInstance("MiniMap", MiniMapDataToLoad);
	rts.AddStructureInstance("PlayerPed", playerPedToLoad);
	rts.AddStructureInstance("ExtraContent", ExtraContentDataToLoad);
	rts.AddStructureInstance("Garages", GarageDataToLoad);
	rts.AddStructureInstance("Stats", StatsDataToLoad);
	rts.AddStructureInstance("StuntJumps", StuntJumpDataToLoad);
	rts.AddStructureInstance("RadioStations", RadioStationDataToLoad);
#if __PPU
	rts.AddStructureInstance("PS3Achievements", PS3AchievementsToLoad);
#endif
	rts.AddStructureInstance("Doors", DoorsToLoad);
	rts.AddStructureInstance("Camera", CameraDataToLoad);
	bool result = LoadXmlTreeFromStream(pReadStream, rts);

	savegameDebugf2("CSaveGameBuffer_SinglePlayer::ReadDataFromXmlFile - Done loading from stream '%s'", pReadStream->GetName());
	return result;
}

void CSaveGameBuffer_SinglePlayer::AfterReadingFromXmlFile()
{
	//	parManager::AutoUseTempMemory temp;
	//	if (m_bBufferContainsScriptData)
	//	{
	DeleteRootNodeOfTree();	//	delete the root node but keep the tree in memory so that the nametable still exists
	// 	}
	// 	else
	// 	{
	// 		delete m_pLoadedParTree;	//	tree doesn't contain script data so we don't need to keep the nametable around. Delete the entire tree.
	// 		m_pLoadedParTree = NULL;
	// 	}
}
#endif	//	SAVEGAME_USES_XML

