/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    script_brains.h
// PURPOSE : all types of script brains
// AUTHOR :  Graeme
// CREATED : 15.3.06
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef _SCRIPT_BRAINS_H_
#define _SCRIPT_BRAINS_H_

#include "streaming/streamingmodule.h"
#include "script/gta_thread.h"
#include "scene/RegdRefTypes.h" // For RegdPed, etc.

/////////////////////////////////////////////////////////////////////////////////
// Class for giving scripts to random peds
/////////////////////////////////////////////////////////////////////////////////

#define MAX_DIFFERENT_SCRIPT_BRAINS	(150)

class CScriptsForBrains
{
	struct script_brain_struct
	{
		strStreamingObjectNameString m_ScriptFileName;
		float m_fBrainActivationRange;
		atHashWithStringNotFinal m_ScenarioName;	//	Used by Scenario Ped brains
		u32 m_ModelHashkey;	//	Used by Ped and Object brains
		u32 m_SetToWhichThisBrainBelongs;	//	Used to decide whether the brain can run in single player, freemode, CnC etc.
		s8 m_TypeOfBrain;
		s8 m_ObjectGroupingID;			//	This can be used to switch a number of object brains off/on with a single Command
		u8 m_PercentageChance;
		u8 m_bBrainActive:1;
	//	u8 bOnlyGiveBrainToScenarioPeds:1;

		void Initialise();
	};

public:
	enum {	PED_STREAMED, 
			OBJECT_STREAMED, 
			WORLD_POINT,
			SCENARIO_PED
		};


public:
	void Init(void);

	void AddScriptBrainsFromFile(const char* filePath);
	void RemoveScriptBrainsFromFile(const char* filePath);

	//	Used for object and ped brains
	void AddScriptBrainForEntity(const strStreamingObjectNameString pScriptName, u32 EntityModelHashkey, u8 Percentage, s8 BrainType, s8 object_grouping_id, float ObjectActivationRange, u32 SetToWhichThisBrainBelongs);

	void AddScriptBrainForScenarioPed(const strStreamingObjectNameString pScriptName, const atHashWithStringNotFinal ScenarioName, u8 Percentage, float fActivationRange, u32 SetToWhichThisBrainBelongs);

	void AddScriptBrainForWorldPoint(const strStreamingObjectNameString pScriptName, s8 BrainType, float ActivationRange, u32 SetToWhichThisBrainBelongs);


	void StartOrRequestNewStreamedScriptBrain(s32 ScriptBrainIndex, void *pEntityOrWorldPoint, s32 BrainType, bool bFirstTime);
	void ClearMissionRequiredFlagOnTheStreamedScript(s32 ScriptBrainIndex);

	void CheckAndApplyIfNewEntityNeedsScript(CEntity *pNewEntity, s8 BrainType);

	s32 GetBrainIndexForNewScenarioPed(u32 HashOfScenarioName);
	void GiveScenarioBrainToPed(CPed *pPed, s32 ScenarioBrainIndex);

	s32	GetIndexOfScriptBrainWithThisName(const strStreamingObjectName pScriptName, s8 BrainType);

	bool IsObjectWithinActivationRange(const CObject *pObject, const Vector3 &vecCentrePoint);
	bool IsWorldPointWithinActivationRange(C2dEffect *pEff, const Vector3 &vecCentrePoint);
	bool IsScenarioPedWithinActivationRange(const CPed *pPed, const Vector3 &vecCentrePoint, float fRangeExtension);

	bool CanBrainSafelyRun(s32 ScriptBrainIndex);

	const char *GetScriptFileName(s32 BrainIndex);
	s8 GetBrainType(s32 BrainIndex);

	void RemoveScenarioPedFromBrainDispatcherArray(CPed *pPed);

	void EnableBrainSet(u32 setToEnable);
	void DisableBrainSet(u32 setToDisable);

private:
	void StartNewStreamedScriptBrain(u32 ScriptBrainIndex, void *pEntityOrWorldPoint);

	s32 FindFreeSlot();
	void ResetSlot(s32 index);

	bool ArePointsWithinActivationRange(const Vector3 &vecCoord1, const Vector3 &vecCoord2, s32 ScriptBrainIndex, float fRangeExtension, bool bIgnoreZ);

//	void SetEnableBrainAllocationFlag(bool bEnableAllocation);
//	void SwitchAllObjectBrainsWithThisID(int object_grouping_id, bool bBrainOn);

private:
	script_brain_struct m_ScriptBrainArray[MAX_DIFFERENT_SCRIPT_BRAINS];

	u32 m_SetsEnabled;

//	bool bEnableBrainAllocation;
};


#define MAX_ENTITIES_WAITING_FOR_SCRIPT_BRAIN		(150)
class CScriptBrainDispatcher
{
public:
	void Init						(void);
	void Process					(void);

	void AddWaitingObject			(CObject*	pItem, s32 scriptBrainIndexToLoad);
	void AddWaitingPed				(CPed*		pItem, s32 scriptBrainIndexToLoad);
	void AddWaitingWorldPoint		(C2dEffect*	pItem, s32 scriptBrainIndexToLoad);

	void RemoveWaitingObject		(CObject*	pItem, s32 scriptBrainIndexToLoad);
	void RemoveWaitingPed			(CPed*		pItem, s32 scriptBrainIndexToLoad);
	void RemoveWaitingWorldPoint	(C2dEffect*	pItem, s32 scriptBrainIndexToLoad);

	//	Brenda asked for a command to help with debugging that will immediately set brains back to OBJECT_WAITING_TILL_IN_BRAIN_RANGE
	//	so that she doesn't have to warp away from an object and then back again to re-trigger the brain.
	void ReactivateAllObjectBrainsThatAreWaitingTillOutOfRange();

	void ReactivateNamedObjectBrainsWaitingTillOutOfRange(const char *pNameToSearchFor);

private:
	enum WaitingItemType
	{
		INVALID_WAITING = 0,
		PED_WAITING,
		OBJECT_WAITING,
		WORLD_POINT_WAITING
	};

	void AddWaitingItem		(void *pEntityOrPoint, s32 scriptBrainIndexToLoad, WaitingItemType itemType);
	void RemoveWaitingItem	(void *pEntityOrPoint, s32 scriptBrainIndexToLoad, WaitingItemType itemType);

	struct ItemWaitingForBrain
	{
		RegdObj			m_pWaitingObject;
		RegdPed			m_pWaitingPed;
		C2dEffect*		m_pWaitingWorldPoint;// These exist for the life of the game, so don't need to be regd (and aren't entities anyway).
		WaitingItemType	m_waitingItemsType;

		s32			m_scriptBrainIndex;

		void Clear();
	};
	ItemWaitingForBrain ItemsWaitingForBrains[MAX_ENTITIES_WAITING_FOR_SCRIPT_BRAIN];

//	void PurgeWaitingItems			(void);
//	bool AreAnyItemsWaiting			(void);
};

#endif // _SCRIPT_BRAINS_H_
