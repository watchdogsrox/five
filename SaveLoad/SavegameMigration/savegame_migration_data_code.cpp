
#include "savegame_migration_data_code.h"

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

// Rage headers
#include "parser/psoparserbuilder.h"

// Game headers
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_data.h"

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

#include "SaveLoad/SavegameScriptData/script_save_data.h"

void CSaveGameMigrationData_Code::Initialize()
{
	m_pSimpleVariablesSaveStructure = NULL;
	m_pMiniMapSaveStructure = NULL;
	m_pPlayerPedSaveStructure_Pso = NULL;
	m_pPlayerPedSaveStructure_Xml = NULL;
	m_pExtraContentSaveStructure = NULL;
	m_pStatsSaveStructure_Pso = NULL;
	m_pStatsSaveStructure_Xml = NULL;
	m_pStuntJumpSaveStructure = NULL;
	m_pRadioStationSaveStructure = NULL;
	m_pGaragesSaveStructure_Pso = NULL;
	m_pGaragesSaveStructure_Xml = NULL;
	m_pDoorsSaveStructure = NULL;
	m_pCameraSaveStructure_Pso = NULL;
	m_pCameraSaveStructure_Xml = NULL;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	m_pRTSToSave = NULL;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
}


void CSaveGameMigrationData_Code::Create()
{
	//	Free up any data that might have been left behind from a previous export
	Delete();

	m_pSimpleVariablesSaveStructure = rage_new CSimpleVariablesSaveStructure;
	m_pMiniMapSaveStructure = rage_new CMiniMapSaveStructure;

	m_pPlayerPedSaveStructure_Pso = rage_new CPlayerPedSaveStructure;
	m_pPlayerPedSaveStructure_Xml = rage_new CPlayerPedSaveStructure_Migration;

	m_pExtraContentSaveStructure = rage_new CExtraContentSaveStructure;

	m_pStatsSaveStructure_Pso = rage_new CStatsSaveStructure;
	m_pStatsSaveStructure_Xml = rage_new CStatsSaveStructure_Migration;

	m_pStuntJumpSaveStructure = rage_new CStuntJumpSaveStructure;
	m_pRadioStationSaveStructure = rage_new CRadioStationSaveStructure;

	m_pGaragesSaveStructure_Pso = rage_new CSaveGarages;
	m_pGaragesSaveStructure_Xml = rage_new CSaveGarages_Migration;

	m_pDoorsSaveStructure = rage_new CSaveDoors;

	m_pCameraSaveStructure_Pso = rage_new CCameraSaveStructure;
	m_pCameraSaveStructure_Xml = rage_new CCameraSaveStructure_Migration;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	m_pRTSToSave = rage_new parRTStructure("SaveGameData");
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
}

void CSaveGameMigrationData_Code::Delete()
{
	if (m_pSimpleVariablesSaveStructure)
	{
		delete m_pSimpleVariablesSaveStructure;
		m_pSimpleVariablesSaveStructure = NULL;
	}

	if (m_pMiniMapSaveStructure)
	{
		delete m_pMiniMapSaveStructure;
		m_pMiniMapSaveStructure = NULL;
	}

	if (m_pPlayerPedSaveStructure_Pso)
	{
		delete m_pPlayerPedSaveStructure_Pso;
		m_pPlayerPedSaveStructure_Pso = NULL;
	}

	if (m_pPlayerPedSaveStructure_Xml)
	{
		delete m_pPlayerPedSaveStructure_Xml;
		m_pPlayerPedSaveStructure_Xml = NULL;
	}

	if (m_pExtraContentSaveStructure)
	{
		delete m_pExtraContentSaveStructure;
		m_pExtraContentSaveStructure = NULL;
	}

	if (m_pStatsSaveStructure_Pso)
	{
		delete m_pStatsSaveStructure_Pso;
		m_pStatsSaveStructure_Pso = NULL;
	}

	if (m_pStatsSaveStructure_Xml)
	{
		delete m_pStatsSaveStructure_Xml;
		m_pStatsSaveStructure_Xml = NULL;
	}

	if (m_pStuntJumpSaveStructure)
	{
		delete m_pStuntJumpSaveStructure;
		m_pStuntJumpSaveStructure = NULL;
	}

	if (m_pRadioStationSaveStructure)
	{
		delete m_pRadioStationSaveStructure;
		m_pRadioStationSaveStructure = NULL;
	}

	if (m_pGaragesSaveStructure_Pso)
	{
		delete m_pGaragesSaveStructure_Pso;
		m_pGaragesSaveStructure_Pso = NULL;
	}

	if (m_pGaragesSaveStructure_Xml)
	{
		delete m_pGaragesSaveStructure_Xml;
		m_pGaragesSaveStructure_Xml = NULL;
	}

	if (m_pDoorsSaveStructure)
	{
		delete m_pDoorsSaveStructure;
		m_pDoorsSaveStructure = NULL;
	}

	if (m_pCameraSaveStructure_Pso)
	{
		delete m_pCameraSaveStructure_Pso;
		m_pCameraSaveStructure_Pso = NULL;
	}

	if (m_pCameraSaveStructure_Xml)
	{
		delete m_pCameraSaveStructure_Xml;
		m_pCameraSaveStructure_Xml = NULL;
	}

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	if (m_pRTSToSave)
	{
		delete m_pRTSToSave;
		m_pRTSToSave = NULL;
	}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
}


#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
void CSaveGameMigrationData_Code::ReadDataFromPsoFile(psoFile* psofile)
{
	Create();

	psoStruct root = psofile->GetRootInstance();
	LoadSingleObject<CSimpleVariablesSaveStructure>(m_pSimpleVariablesSaveStructure, "SimpleVariables", root);
	LoadSingleObject<CMiniMapSaveStructure>(m_pMiniMapSaveStructure, "MiniMap", root);
	LoadSingleObject<CPlayerPedSaveStructure>(m_pPlayerPedSaveStructure_Pso, "PlayerPed", root);
	LoadSingleObject<CExtraContentSaveStructure>(m_pExtraContentSaveStructure, "ExtraContent", root);
	LoadSingleObject<CSaveGarages>(m_pGaragesSaveStructure_Pso, "Garages", root);
	LoadSingleObject<CStatsSaveStructure>(m_pStatsSaveStructure_Pso, "Stats", root);
	LoadSingleObject<CStuntJumpSaveStructure>(m_pStuntJumpSaveStructure, "StuntJumps", root);
	LoadSingleObject<CRadioStationSaveStructure>(m_pRadioStationSaveStructure, "RadioStations", root);
	LoadSingleObject<CSaveDoors>(m_pDoorsSaveStructure, "Doors", root);
	LoadSingleObject<CCameraSaveStructure>(m_pCameraSaveStructure_Pso, "Camera", root);

	HandleTranslationsFromLoadedPsoToSavedXmlData();
}


void CSaveGameMigrationData_Code::HandleTranslationsFromLoadedPsoToSavedXmlData()
{
	m_pPlayerPedSaveStructure_Xml->Initialize(*m_pPlayerPedSaveStructure_Pso);
	m_pStatsSaveStructure_Xml->Initialize(*m_pStatsSaveStructure_Pso);
	m_pGaragesSaveStructure_Xml->Initialize(*m_pGaragesSaveStructure_Pso);
	m_pCameraSaveStructure_Xml->Initialize(*m_pCameraSaveStructure_Pso);
}


parRTStructure *CSaveGameMigrationData_Code::ConstructRTSToSave()
{
	if (m_pRTSToSave)
	{
		m_pRTSToSave->AddStructureInstance("SimpleVariables", *m_pSimpleVariablesSaveStructure);
		m_pRTSToSave->AddStructureInstance("MiniMap", *m_pMiniMapSaveStructure);
		m_pRTSToSave->AddStructureInstance("PlayerPed", *m_pPlayerPedSaveStructure_Xml);
		m_pRTSToSave->AddStructureInstance("ExtraContent", *m_pExtraContentSaveStructure);
		m_pRTSToSave->AddStructureInstance("Garages", *m_pGaragesSaveStructure_Xml);
		m_pRTSToSave->AddStructureInstance("Stats", *m_pStatsSaveStructure_Xml);
		m_pRTSToSave->AddStructureInstance("StuntJumps", *m_pStuntJumpSaveStructure);
		m_pRTSToSave->AddStructureInstance("RadioStations", *m_pRadioStationSaveStructure);
		m_pRTSToSave->AddStructureInstance("Doors", *m_pDoorsSaveStructure);
		m_pRTSToSave->AddStructureInstance("Camera", *m_pCameraSaveStructure_Xml);
		return m_pRTSToSave;
	}

	savegameErrorf("CSaveGameMigrationData_Code::ConstructRTSToSave - m_pRTSToSave is NULL");
	savegameAssertf(0, "CSaveGameMigrationData_Code::ConstructRTSToSave - m_pRTSToSave is NULL");
	return NULL;
}

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
bool CSaveGameMigrationData_Code::ReadDataFromParTree(parTree *pParTree)
{
	Create();

	parRTStructure rts("SaveGameData");
	rts.AddStructureInstance("SimpleVariables", *m_pSimpleVariablesSaveStructure);
	rts.AddStructureInstance("MiniMap", *m_pMiniMapSaveStructure);
	rts.AddStructureInstance("PlayerPed", *m_pPlayerPedSaveStructure_Xml);
	rts.AddStructureInstance("ExtraContent", *m_pExtraContentSaveStructure);
	rts.AddStructureInstance("Garages", *m_pGaragesSaveStructure_Xml);
	rts.AddStructureInstance("Stats", *m_pStatsSaveStructure_Xml);
	rts.AddStructureInstance("StuntJumps", *m_pStuntJumpSaveStructure);
	rts.AddStructureInstance("RadioStations", *m_pRadioStationSaveStructure);
	rts.AddStructureInstance("Doors", *m_pDoorsSaveStructure);
	rts.AddStructureInstance("Camera", *m_pCameraSaveStructure_Xml);
	bool result = PARSER.LoadFromStructure(pParTree->GetRoot(), rts, NULL, false);

	HandleTranslationsFromLoadedXmlToSavedPsoData();

	return result;
}


void CSaveGameMigrationData_Code::HandleTranslationsFromLoadedXmlToSavedPsoData()
{
	m_pPlayerPedSaveStructure_Pso->Initialize(*m_pPlayerPedSaveStructure_Xml);
	m_pStatsSaveStructure_Pso->Initialize(*m_pStatsSaveStructure_Xml);
	m_pGaragesSaveStructure_Pso->Initialize(*m_pGaragesSaveStructure_Xml);
	m_pCameraSaveStructure_Pso->Initialize(*m_pCameraSaveStructure_Xml);
}


// Based on CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure()
psoBuilderInstance *CSaveGameMigrationData_Code::ConstructPsoDataToSave(psoBuilder *pPsoBuilder)
{
// Define a new pso structure schema that contains a bunch of pointers
	psoBuilderStructSchema& schema = pPsoBuilder->CreateStructSchema(atLiteralHashValue("SaveGameData"));
	schema.AddMemberPointer(atLiteralHashValue("SimpleVariables"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("MiniMap"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("PlayerPed"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("ExtraContent"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("Stats"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("StuntJumps"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("RadioStations"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("Garages"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("Doors"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("Camera"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);

	schema.AddMemberPointer(atLiteralHashValue(CSaveGameData::GetNameOfMainScriptTreeNode()), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	if(CScriptSaveData::GetNumOfScriptStructs(SAVEGAME_SINGLE_PLAYER) > 0)
	{
		schema.AddMemberPointer(atLiteralHashValue(CSaveGameData::GetNameOfDLCScriptTreeNode()), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

	schema.FinishBuilding();

// Now create an instance of that schema and set all the pointers
	psoBuilderInstance& builderInst = pPsoBuilder->CreateStructInstances(schema);

	AddChildObject(*pPsoBuilder, builderInst, m_pSimpleVariablesSaveStructure, "SimpleVariables");
	AddChildObject(*pPsoBuilder, builderInst, m_pMiniMapSaveStructure, "MiniMap");
	AddChildObject(*pPsoBuilder, builderInst, m_pPlayerPedSaveStructure_Pso, "PlayerPed");
	AddChildObject(*pPsoBuilder, builderInst, m_pExtraContentSaveStructure, "ExtraContent");
	AddChildObject(*pPsoBuilder, builderInst, m_pStatsSaveStructure_Pso, "Stats");
	AddChildObject(*pPsoBuilder, builderInst, m_pStuntJumpSaveStructure, "StuntJumps");
	AddChildObject(*pPsoBuilder, builderInst, m_pRadioStationSaveStructure, "RadioStations");
	AddChildObject(*pPsoBuilder, builderInst, m_pGaragesSaveStructure_Pso, "Garages");
	AddChildObject(*pPsoBuilder, builderInst, m_pDoorsSaveStructure, "Doors");
	AddChildObject(*pPsoBuilder, builderInst, m_pCameraSaveStructure_Pso, "Camera");

	// Note that we don't set the ScriptSaveData yet - that comes later!
	pPsoBuilder->SetRootObject(builderInst);

	return &builderInst;
}

#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

s32 CSaveGameMigrationData_Code::GetHashOfNameOfLastMissionPassed()
{
	if (savegameVerifyf(m_pStatsSaveStructure_Xml, "CSaveGameMigrationData_Code::GetHashOfNameOfLastMissionPassed - m_pStatsSaveStructure_Xml is NULL"))
	{
		return m_pStatsSaveStructure_Xml->GetHashOfNameOfLastMissionPassed();
	}

	return 0;
}

float CSaveGameMigrationData_Code::GetSinglePlayerCompletionPercentage()
{
	if (savegameVerifyf(m_pStatsSaveStructure_Xml, "CSaveGameMigrationData_Code::GetSinglePlayerCompletionPercentage - m_pStatsSaveStructure_Xml is NULL"))
	{
		return m_pStatsSaveStructure_Xml->GetSinglePlayerCompletionPercentage();
	}

	return 0.0f;
}

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
