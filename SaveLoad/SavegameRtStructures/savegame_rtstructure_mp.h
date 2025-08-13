#ifndef SAVEGAME_RT_STRUCTURE_MP_H
#define SAVEGAME_RT_STRUCTURE_MP_H


// Game headers
#include "SaveLoad/SavegameData/savegame_data_mp_fog_of_war.h"
#include "SaveLoad/SavegameRtStructures/savegame_rtstructure.h"



// Forward declarations
class CMultiplayerStatsSaveStructure;
class CPosixTimeStampForMultiplayerSaves;


class CRTStructureDataToBeSaved_MultiplayerCharacter : public CRTStructureDataToBeSaved
{
	CMultiplayerStatsSaveStructure *m_pStatsDataToSave;
	CPosixTimeStampForMultiplayerSaves *m_pPosixTimeStamp;
#if SAVE_MULTIPLAYER_FOG_OF_WAR
	CMultiplayerFogOfWarSaveStructure *m_pFogOfWarDataToSave;
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR

#if SAVEGAME_USES_XML
	parRTStructure *m_pRTSToSave;
#endif	//	SAVEGAME_USES_XML

public:
	CRTStructureDataToBeSaved_MultiplayerCharacter();
	virtual ~CRTStructureDataToBeSaved_MultiplayerCharacter();

	virtual void DeleteAllRTStructureData();

#if SAVEGAME_USES_XML
	virtual parRTStructure *CreateRTStructure();
#endif

#if SAVEGAME_USES_PSO
	virtual psoBuilderInstance* CreatePsoStructure(psoBuilder& builder);
#endif
};


class CRTStructureDataToBeSaved_MultiplayerCommon : public CRTStructureDataToBeSaved
{
	CMultiplayerStatsSaveStructure *m_pStatsDataToSave;
	CPosixTimeStampForMultiplayerSaves *m_pPosixTimeStamp;

#if SAVEGAME_USES_XML
	parRTStructure *m_pRTSToSave;
#endif	//	SAVEGAME_USES_XML

public:
	CRTStructureDataToBeSaved_MultiplayerCommon();
	virtual ~CRTStructureDataToBeSaved_MultiplayerCommon();

	virtual void DeleteAllRTStructureData();

#if SAVEGAME_USES_XML
	virtual parRTStructure *CreateRTStructure();
#endif

#if SAVEGAME_USES_PSO
	virtual psoBuilderInstance* CreatePsoStructure(psoBuilder& builder);
#endif
};


#endif // SAVEGAME_RT_STRUCTURE_MP_H


