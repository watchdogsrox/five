
#include "savegame_rtstructure_sp.h"

// Rage headers
#include "parser/psoparserbuilder.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"

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

// ****************************** CRTStructureDataToBeSaved_SinglePlayer ****************************************************

CRTStructureDataToBeSaved_SinglePlayer::CRTStructureDataToBeSaved_SinglePlayer()
	: CRTStructureDataToBeSaved()
{
	m_pSimpleVariablesToSave = NULL;
	m_pMiniMapDataToSave = NULL;
	m_pPlayerPedToSave = NULL;
	m_pExtraContentDataToSave = NULL;
	m_pStatsDataToSave = NULL;
	m_pStuntJumpDataToSave = NULL;
	m_pRadioStationDataToSave = NULL;
#if __PPU
	m_pPS3AchievementsToSave = NULL;
#endif
	m_pGarageDataToSave = NULL;
	m_pDoors = NULL;
	m_pCameraDataToSave = NULL;

#if SAVEGAME_USES_XML
	m_pRTSToSave = NULL;
#endif	//	SAVEGAME_USES_XML
}

CRTStructureDataToBeSaved_SinglePlayer::~CRTStructureDataToBeSaved_SinglePlayer()
{
	DeleteAllRTStructureData();
}

void CRTStructureDataToBeSaved_SinglePlayer::DeleteAllRTStructureData()
{
	if (m_pSimpleVariablesToSave)
	{
		delete m_pSimpleVariablesToSave;
		m_pSimpleVariablesToSave = NULL;
	}

	if (m_pMiniMapDataToSave)
	{
		delete m_pMiniMapDataToSave;
		m_pMiniMapDataToSave = NULL;
	}

	if (m_pPlayerPedToSave)
	{
		delete m_pPlayerPedToSave;
		m_pPlayerPedToSave = NULL;
	}

	if (m_pExtraContentDataToSave)
	{
		delete m_pExtraContentDataToSave;
		m_pExtraContentDataToSave = NULL;
	}

	if (m_pStatsDataToSave)
	{
		delete m_pStatsDataToSave;
		m_pStatsDataToSave = NULL;
	}

	if (m_pStuntJumpDataToSave)
	{
		delete m_pStuntJumpDataToSave;
		m_pStuntJumpDataToSave = NULL;
	}

	if (m_pRadioStationDataToSave)
	{
		delete m_pRadioStationDataToSave;
		m_pRadioStationDataToSave = NULL;
	}
#if __PPU
	if (m_pPS3AchievementsToSave)
	{
		delete m_pPS3AchievementsToSave;
		m_pPS3AchievementsToSave = NULL;
	}
#endif
	if (m_pGarageDataToSave)
	{
		delete m_pGarageDataToSave;
		m_pGarageDataToSave = NULL;
	}

	if (m_pDoors)
	{
		delete m_pDoors;
		m_pDoors = NULL;
	}

	if (m_pCameraDataToSave)
	{
		delete m_pCameraDataToSave;
		m_pCameraDataToSave = NULL;
	}

#if SAVEGAME_USES_XML
	if (m_pRTSToSave)
	{
		delete m_pRTSToSave;
		m_pRTSToSave = NULL;
	}
#endif	//	SAVEGAME_USES_XML
}

#if SAVEGAME_USES_XML
parRTStructure *CRTStructureDataToBeSaved_SinglePlayer::CreateRTStructure()
{
	//	Free up any data that might have been left behind from a previous save
	DeleteAllRTStructureData();

	m_pSimpleVariablesToSave = rage_new CSimpleVariablesSaveStructure;
	m_pMiniMapDataToSave = rage_new CMiniMapSaveStructure;
	m_pPlayerPedToSave = rage_new CPlayerPedSaveStructure;
	m_pExtraContentDataToSave = rage_new CExtraContentSaveStructure;
	m_pStatsDataToSave = rage_new CStatsSaveStructure;
	m_pStuntJumpDataToSave = rage_new CStuntJumpSaveStructure;
	m_pRadioStationDataToSave = rage_new CRadioStationSaveStructure;
	m_pGarageDataToSave = rage_new CSaveGarages;
	m_pDoors = rage_new CSaveDoors;
	m_pCameraDataToSave = rage_new CCameraSaveStructure;

	m_pRTSToSave = rage_new parRTStructure("SaveGameData");
	m_pRTSToSave->AddStructureInstance("SimpleVariables", *m_pSimpleVariablesToSave);
	m_pRTSToSave->AddStructureInstance("MiniMap", *m_pMiniMapDataToSave);
	m_pRTSToSave->AddStructureInstance("PlayerPed", *m_pPlayerPedToSave);
	m_pRTSToSave->AddStructureInstance("ExtraContent", *m_pExtraContentDataToSave);
	m_pRTSToSave->AddStructureInstance("Garages", *m_pGarageDataToSave);
	m_pRTSToSave->AddStructureInstance("Stats", *m_pStatsDataToSave);
	m_pRTSToSave->AddStructureInstance("StuntJumps", *m_pStuntJumpDataToSave);
	m_pRTSToSave->AddStructureInstance("RadioStations", *m_pRadioStationDataToSave);
#if __PPU
	m_pRTSToSave->AddStructureInstance("PS3Achievements", *m_pPS3AchievementsToSave);
#endif
	m_pRTSToSave->AddStructureInstance("Doors", *m_pDoors);
	m_pRTSToSave->AddStructureInstance("Camera", *m_pCameraDataToSave);

	return m_pRTSToSave;
}
#endif // SAVEGAME_USES_XML


#if SAVEGAME_USES_PSO
psoBuilderInstance* CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure(psoBuilder& builder)
{
	// Define a new pso structure schema that contains a bunch of pointers

	psoBuilderStructSchema& schema = builder.CreateStructSchema(atLiteralHashValue("SaveGameData"));

#if __BANK
	if (CGenericGameStorage::ms_bSaveSimpleVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("SimpleVariables"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveMiniMapVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("MiniMap"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSavePlayerPedVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("PlayerPed"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveExtraContentVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("ExtraContent"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveGarageVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("Garages"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveStatsVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("Stats"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveStuntJumpVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("StuntJumps"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveRadioStationVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("RadioStations"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}


#if __PPU
#if __BANK
	if (CGenericGameStorage::ms_bSavePS3AchievementVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("PS3Achievements"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}
#endif	//	__PPU

#if __BANK
	if (CGenericGameStorage::ms_bSaveScriptVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue(CSaveGameData::GetNameOfMainScriptTreeNode()), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
		if(CScriptSaveData::GetNumOfScriptStructs(SAVEGAME_SINGLE_PLAYER)>0)
			schema.AddMemberPointer(atLiteralHashValue(CSaveGameData::GetNameOfDLCScriptTreeNode()), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveDoorVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("Doors"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveCameraVariables)
#endif	//	__BANK
	{
		schema.AddMemberPointer(atLiteralHashValue("Camera"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

	schema.FinishBuilding();

	// Now create an instance of that schema and set all the pointers

	psoBuilderInstance& builderInst = builder.CreateStructInstances(schema);

#if !__NO_OUTPUT
	u32 beforeSize = 0;
	u32 afterSize = 0;
#endif	//	!__NO_OUTPUT

#if __BANK
	if (CGenericGameStorage::ms_bSaveSimpleVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pSimpleVariablesToSave, "SimpleVariables");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(SimpleVariables) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveMiniMapVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pMiniMapDataToSave, "MiniMap");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(MiniMap) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSavePlayerPedVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pPlayerPedToSave, "PlayerPed");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(PlayerPed) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveExtraContentVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pExtraContentDataToSave, "ExtraContent");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(ExtraContent) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveGarageVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pGarageDataToSave, "Garages");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(Garages) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveStatsVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pStatsDataToSave, "Stats");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(Stats) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveStuntJumpVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pStuntJumpDataToSave, "StuntJumps");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(StuntJumps) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveRadioStationVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pRadioStationDataToSave, "RadioStations");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(RadioStations) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}

#if __PPU

#if __BANK
	if (CGenericGameStorage::ms_bSavePS3AchievementVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pPS3AchievementsToSave, "PS3Achievements");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(PS3Achievements) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}
#endif	//	__PPU

#if __BANK
	if (CGenericGameStorage::ms_bSaveDoorVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pDoors, "Doors");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(Doors) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}

#if __BANK
	if (CGenericGameStorage::ms_bSaveCameraVariables)
#endif	//	__BANK
	{
		OUTPUT_ONLY( beforeSize = builder.ComputeSize(); )

			AddChildObject(builder, builderInst, m_pCameraDataToSave, "Camera");

		OUTPUT_ONLY( afterSize = builder.ComputeSize(); )
			psoSizeDebugf1("CRTStructureDataToBeSaved_SinglePlayer::CreatePsoStructure :: AddChildObject(Camera) sizeDifference=%u beforeSize=%u afterSize=%u", (afterSize-beforeSize), beforeSize, afterSize);
	}

	// Note that we don't set the ScriptSaveData yet - that comes later!
	builder.SetRootObject(builderInst);

	return &builderInst;
}
#endif // SAVEGAME_USES_PSO

