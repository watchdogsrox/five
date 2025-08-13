
#include "savegame_rtstructure_mp.h"

// Rage headers
#include "parser/psoparserbuilder.h"

// Game headers
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_data.h"

//	#include "SaveLoad/SavegameData/savegame_data_mp_fog_of_war.h"
#include "SaveLoad/SavegameData/savegame_data_mp_timestamp.h"
#include "SaveLoad/SavegameData/savegame_data_stats.h"

#include "SaveLoad/SavegameScriptData/script_save_data.h"

XPARAM(dont_save_script_data_to_mp_stats);

// ****************************** CRTStructureDataToBeSaved_MultiplayerCharacter *************************************************

CRTStructureDataToBeSaved_MultiplayerCharacter::CRTStructureDataToBeSaved_MultiplayerCharacter()
	: CRTStructureDataToBeSaved()
{
	m_pStatsDataToSave = NULL;
	m_pPosixTimeStamp = NULL;
#if SAVE_MULTIPLAYER_FOG_OF_WAR
	m_pFogOfWarDataToSave = NULL;
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

#if SAVEGAME_USES_XML
	m_pRTSToSave = NULL;
#endif	//	SAVEGAME_USES_XML
}

CRTStructureDataToBeSaved_MultiplayerCharacter::~CRTStructureDataToBeSaved_MultiplayerCharacter()
{
	DeleteAllRTStructureData();
}

void CRTStructureDataToBeSaved_MultiplayerCharacter::DeleteAllRTStructureData()
{
	if (m_pStatsDataToSave)
	{
		delete m_pStatsDataToSave;
		m_pStatsDataToSave = NULL;
	}

	if (m_pPosixTimeStamp)
	{
		delete m_pPosixTimeStamp;
		m_pPosixTimeStamp = NULL;
	}

#if SAVE_MULTIPLAYER_FOG_OF_WAR
	if (m_pFogOfWarDataToSave)
	{
		delete m_pFogOfWarDataToSave;
		m_pFogOfWarDataToSave = NULL;
	}
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

#if SAVEGAME_USES_XML
	if (m_pRTSToSave)
	{
		delete m_pRTSToSave;
		m_pRTSToSave = NULL;
	}
#endif	//	SAVEGAME_USES_XML
}

#if SAVEGAME_USES_XML
parRTStructure *CRTStructureDataToBeSaved_MultiplayerCharacter::CreateRTStructure()
{
	//	Free up any data that might have been left behind from a previous save
	DeleteAllRTStructureData();

	m_pStatsDataToSave = rage_new CMultiplayerStatsSaveStructure;
	m_pPosixTimeStamp = rage_new CPosixTimeStampForMultiplayerSaves;
#if SAVE_MULTIPLAYER_FOG_OF_WAR
	m_pFogOfWarDataToSave = rage_new CMultiplayerFogOfWarSaveStructure;
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

	m_pRTSToSave = rage_new parRTStructure("SaveGameData");
	m_pRTSToSave->AddStructureInstance("Stats", *m_pStatsDataToSave);
	m_pRTSToSave->AddStructureInstance("Timestamp", *m_pPosixTimeStamp);
#if SAVE_MULTIPLAYER_FOG_OF_WAR
	m_pRTSToSave->AddStructureInstance("FogOfWar", *m_pFogOfWarDataToSave);
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

	return m_pRTSToSave;
}
#endif	//	SAVEGAME_USES_XML

#if SAVEGAME_USES_PSO
psoBuilderInstance* CRTStructureDataToBeSaved_MultiplayerCharacter::CreatePsoStructure(psoBuilder& builder)
{
	// Define a new pso structure schema that contains a bunch of pointers

	OUTPUT_ONLY( u32 size = builder.ComputeSize(); )
		psoSizeDebugf1("MultiplayerCharacter::CreatePsoStructure :: Start size is '%d'", size);

	psoBuilderStructSchema& schema = builder.CreateStructSchema(atLiteralHashValue("SaveGameData"));
	psoSizeDebugf1("Size Of Member 'SaveGameData' is '%d'", builder.ComputeSize() - size);
	OUTPUT_ONLY( size = builder.ComputeSize(); )

		schema.AddMemberPointer(atLiteralHashValue("Stats"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("Timestamp"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
#if SAVE_MULTIPLAYER_FOG_OF_WAR
	schema.AddMemberPointer(atLiteralHashValue("FogOfWar"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

#if !__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON

#if !__FINAL
	if (PARAM_dont_save_script_data_to_mp_stats.Get() == false)
#endif	//	!__FINAL
	{
		if(CScriptSaveData::GetMainScriptSaveData().HasMultiplayerData())
			schema.AddMemberPointer(atLiteralHashValue(CSaveGameData::GetNameOfMainScriptTreeNode()), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
		if(CScriptSaveData::GetNumOfScriptStructs(SAVEGAME_MULTIPLAYER_CHARACTER)>0)
			schema.AddMemberPointer(atLiteralHashValue(CSaveGameData::GetNameOfDLCScriptTreeNode()), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#endif	//	!__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON


	schema.FinishBuilding();

	// Now create an instance of that schema and set all the pointers

	psoBuilderInstance& builderInst = builder.CreateStructInstances(schema);
	psoSizeDebugf1("Size Of Member 'CreateStructInstances' is '%d'", builder.ComputeSize() - size);
	OUTPUT_ONLY( size = builder.ComputeSize(); )

		AddChildObject(builder, builderInst, m_pStatsDataToSave, "Stats");
	psoSizeDebugf1("Size Of Member 'Stats' is '%d'", builder.ComputeSize() - size);
	OUTPUT_ONLY( size = builder.ComputeSize(); )

		AddChildObject(builder, builderInst, m_pPosixTimeStamp, "Timestamp");
	psoSizeDebugf1("Size Of Member 'Timestamp' is '%d'", builder.ComputeSize() - size);
	OUTPUT_ONLY( size = builder.ComputeSize(); )

#if SAVE_MULTIPLAYER_FOG_OF_WAR
		AddChildObject(builder, builderInst, m_pFogOfWarDataToSave, "FogOfWar");
	psoSizeDebugf1("Size Of Member 'FogOfWar' is '%d'", builder.ComputeSize() - size);
	OUTPUT_ONLY( size = builder.ComputeSize(); )
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

		// If we do have any ScriptSaveData, it doesn't get set here - that comes later!

		builder.SetRootObject(builderInst);

	psoSizeDebugf1("MultiplayerCharacter::CreatePsoStructure :: End Size Of All Members is '%d'", size);

	return &builderInst;
}
#endif	//	SAVEGAME_USES_PSO


// ****************************** CRTStructureDataToBeSaved_MultiplayerCommon ****************************************************


CRTStructureDataToBeSaved_MultiplayerCommon::CRTStructureDataToBeSaved_MultiplayerCommon()
	: CRTStructureDataToBeSaved()
{
	m_pStatsDataToSave = NULL;
	m_pPosixTimeStamp = NULL;

#if SAVEGAME_USES_XML
	m_pRTSToSave = NULL;
#endif	//	SAVEGAME_USES_XML
}

CRTStructureDataToBeSaved_MultiplayerCommon::~CRTStructureDataToBeSaved_MultiplayerCommon()
{
	DeleteAllRTStructureData();
}

void CRTStructureDataToBeSaved_MultiplayerCommon::DeleteAllRTStructureData()
{
	if (m_pStatsDataToSave)
	{
		delete m_pStatsDataToSave;
		m_pStatsDataToSave = NULL;
	}

	if (m_pPosixTimeStamp)
	{
		delete m_pPosixTimeStamp;
		m_pPosixTimeStamp = NULL;
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
parRTStructure *CRTStructureDataToBeSaved_MultiplayerCommon::CreateRTStructure()
{
	//	Free up any data that might have been left behind from a previous save
	DeleteAllRTStructureData();

	m_pStatsDataToSave = rage_new CMultiplayerStatsSaveStructure;
	m_pPosixTimeStamp = rage_new CPosixTimeStampForMultiplayerSaves;

	m_pRTSToSave = rage_new parRTStructure("SaveGameData");
	m_pRTSToSave->AddStructureInstance("Stats", *m_pStatsDataToSave);
	m_pRTSToSave->AddStructureInstance("Timestamp", *m_pPosixTimeStamp);

	return m_pRTSToSave;
}
#endif	//	SAVEGAME_USES_XML

#if SAVEGAME_USES_PSO
psoBuilderInstance* CRTStructureDataToBeSaved_MultiplayerCommon::CreatePsoStructure(psoBuilder& builder)
{
	// Define a new pso structure schema that contains a bunch of pointers

	OUTPUT_ONLY( u32 size = builder.ComputeSize(); )
		psoSizeDebugf1("MultiplayerCommon::CreatePsoStructure :: Start size is '%d'", size);

	psoBuilderStructSchema& schema = builder.CreateStructSchema(atLiteralHashValue("SaveGameData"));
	psoSizeDebugf1("Size Of Member 'SaveGameData' is '%d'", builder.ComputeSize() - size);
	OUTPUT_ONLY( size = builder.ComputeSize(); )

		schema.AddMemberPointer(atLiteralHashValue("Stats"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	schema.AddMemberPointer(atLiteralHashValue("Timestamp"), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);

#if __MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON

#if !__FINAL
	if (PARAM_dont_save_script_data_to_mp_stats.Get() == false)
#endif	//	!__FINAL
	{
		if(CScriptSaveData::GetMainScriptSaveData().HasMultiplayerData())
			schema.AddMemberPointer(atLiteralHashValue(CSaveGameData::GetNameOfMainScriptTreeNode()), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
		if(CScriptSaveData::GetNumOfScriptStructs(SAVEGAME_MULTIPLAYER_COMMON)>0)
			schema.AddMemberPointer(atLiteralHashValue(CSaveGameData::GetNameOfDLCScriptTreeNode()), parMemberStructSubType::SUBTYPE_SIMPLE_POINTER);
	}

#endif	//	__MP_SCRIPT_DATA_IS_SAVED_IN_SAVEGAME_MULTIPLAYER_COMMON

	schema.FinishBuilding();

	// Now create an instance of that schema and set all the pointers

	psoBuilderInstance& builderInst = builder.CreateStructInstances(schema);
	psoSizeDebugf1("Size Of Member 'CreateStructInstances' is '%d'", builder.ComputeSize() - size);
	OUTPUT_ONLY( size = builder.ComputeSize(); )

		AddChildObject(builder, builderInst, m_pStatsDataToSave, "Stats");
	psoSizeDebugf1("Size Of Member 'Stats' is '%d'", builder.ComputeSize() - size);
	OUTPUT_ONLY( size = builder.ComputeSize(); )

		AddChildObject(builder, builderInst, m_pPosixTimeStamp, "Timestamp");
	psoSizeDebugf1("Size Of Member 'Timestamp' is '%d'", builder.ComputeSize() - size);
	OUTPUT_ONLY( size = builder.ComputeSize(); )

		// If we do have any ScriptSaveData, it doesn't get set here - that comes later!

		builder.SetRootObject(builderInst);

	psoSizeDebugf1("MultiplayerCommon::CreatePsoStructure :: End Size Of All Members is '%d'", size);

	return &builderInst;
}
#endif	//	SAVEGAME_USES_PSO

