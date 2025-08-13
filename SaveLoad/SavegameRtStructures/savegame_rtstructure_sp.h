#ifndef SAVEGAME_RT_STRUCTURE_SP_H
#define SAVEGAME_RT_STRUCTURE_SP_H

// Game headers
#include "SaveLoad/SavegameRtStructures/savegame_rtstructure.h"


// Forward declarations
class CSimpleVariablesSaveStructure;
class CMiniMapSaveStructure;
class CPlayerPedSaveStructure;
class CExtraContentSaveStructure;
class CStatsSaveStructure;
class CStuntJumpSaveStructure;
class CRadioStationSaveStructure;
class CSaveGarages;
class CSaveDoors;
class CCameraSaveStructure;


class CRTStructureDataToBeSaved_SinglePlayer : public CRTStructureDataToBeSaved
{
	CSimpleVariablesSaveStructure *m_pSimpleVariablesToSave;
	CMiniMapSaveStructure *m_pMiniMapDataToSave;
	CPlayerPedSaveStructure *m_pPlayerPedToSave;
	CExtraContentSaveStructure *m_pExtraContentDataToSave;
	CStatsSaveStructure *m_pStatsDataToSave;
	CStuntJumpSaveStructure *m_pStuntJumpDataToSave;
	CRadioStationSaveStructure *m_pRadioStationDataToSave;
#if __PPU
	CPS3AchievementsSaveStructure *m_pPS3AchievementsToSave;
#endif
	CSaveGarages *m_pGarageDataToSave;

#if SAVEGAME_USES_XML
	parRTStructure *m_pRTSToSave;
#endif	//	SAVEGAME_USES_XML

	CSaveDoors *m_pDoors;
	CCameraSaveStructure *m_pCameraDataToSave;

public:
	CRTStructureDataToBeSaved_SinglePlayer();
	virtual ~CRTStructureDataToBeSaved_SinglePlayer();

	virtual void DeleteAllRTStructureData();

#if SAVEGAME_USES_XML
	virtual parRTStructure *CreateRTStructure();
#endif

#if SAVEGAME_USES_PSO
	virtual psoBuilderInstance* CreatePsoStructure(psoBuilder& builder);
#endif
};


#endif // SAVEGAME_RT_STRUCTURE_SP_H


