#ifndef SCENARIO_DEBUG_H
#define SCENARIO_DEBUG_H

#include "task/Scenario/scdebug.h" // for SCENARIO_DEBUG

#if SCENARIO_DEBUG

#include "scene/RegdRefTypes.h"

// Rage headers
#include "atl/array.h"
#include "atl/queue.h"

// Game headers
#include "task/Scenario/ScenarioEditor.h"
#include "Task/Scenario/ScenarioPoint.h"

namespace rage
{
	class bkBank;
	class bkButton;
}

class CConditionalAnimsGroup;
class CPopZone;
class CScenarioEntityOverride;

//////////////////////////////////////////////////////////////////////////
// CScenarioDebug
//////////////////////////////////////////////////////////////////////////

// PURPOSE:	Base class for filters for GetClosestSpawnPoint().
class CScenarioDebugClosestFilter : public datBase
{
public:
	virtual bool Match(const CScenarioPoint& scenarioPt) const = 0;
};

class CScenarioDebug
{
public:

	// Setup the RAG widgets 
	static void InitBank();
	static void CreateBank();
	static void CreateEditorBank();
	static void CreateConditionalAnimsBank();
	static void CreateDebugBank();
	static void CreateModelSetsBank();
	static void CreateOtherBank();
	static void CreateScenarioInfoBank();
	static int CompareModelInfos(const void* pVoidA, const void* pVoidB);
	static void CreateScenarioReactsDebugBank();
	static void SpawnProp();
	static void SpawnScenario();
	static void CleanUpScenarioReactInfo();
	static CPed* SpawnPed(const Vector3& vSpawnPosition, float fHeading = 0.0f, bool bScenarioPed = true);
	static void SpawnScenarioPed();
	static void SpawnReactionPed(const Vector3& vSpawnPosition);
	static void TriggerReacts();
	static void TriggerAllReacts();
	static void ShutdownBank();
	static void UpdateScenarioGroupCombo();

	static void DebugUpdate();
	static void DebugRender();

	static CScenarioEditor ms_Editor;
	
	static bank_float ms_fRenderDistance;
	static bank_bool  ms_bRenderScenarioPoints;
	static bank_bool  ms_bRenderStationaryReactionsPointsOnly;
	static bank_bool  ms_bRenderOnlyChainedPoints;
	static bank_bool  ms_bRenderChainPointUsage;
	static bank_bool  ms_bRenderScenarioPointsDisabled;
	static bank_bool  ms_bRenderScenarioPointsOnlyInPopZone;
	static bank_bool  ms_bDisableScenariosWithNoClipData;
	static bank_bool  ms_bRenderScenarioPointRegionBounds;
	static bank_bool  ms_bRenderScenarioPointsOnlyInTimePeriod;
	static bank_bool  ms_bRenderScenarioPointsOnlyInTimePeriodCurrent;
	static bank_u32   ms_iRenderScenarioPointsOnlyInTimePeriodStart;
	static bank_u32   ms_iRenderScenarioPointsOnlyInTimePeriodEnd;
	static bank_float ms_fPointHandleLength;			// direction/rotation handle length when debug drawing
	static bank_float ms_fPointHandleRadius;
	static bank_float ms_fPointDrawRadius;
	static bank_bool  ms_bRenderIntoVectorMap;
	static bank_bool  ms_bRenderSpawnHistory;
	static bank_bool  ms_bRenderSpawnHistoryText;
	static bank_bool  ms_bRenderSpawnHistoryTextPed;
	static bank_bool  ms_bRenderSpawnHistoryTextVeh;
	static bank_bool  ms_bDisplayScenarioModelSetsOnVectorMap;
	static bank_bool  ms_bAnimDebugDisableSPRender;
	static bank_bool  ms_bRenderScenarioSpawnCheckCache;
	static bank_bool  ms_bRenderPointText;
	static bank_bool  ms_bRenderPointRadius;
	static bank_bool  ms_bRenderPointAddress;
	static bank_bool  ms_bRenderClusterInfo;
	static bank_bool  ms_bRenderClusterReport;
	static bank_bool  ms_bRenderNonRegionSpatialArray;
	static bank_bool  ms_bRenderSummaryInfo;
	static bank_bool  ms_bRenderPopularTypesInSummary;
	static bank_bool  ms_bRenderSelectedTypeOnly;
	static bank_bool  ms_bRenderSelectedScenarioGroup;

	// Allow restriction on what scenarios are permitted to attract vehicles.
	static bank_bool ms_bRestrictVehicleAttractionToSelectedPoint;

private:
	static void RenderModelSetsOnVectorMap();
	static void RenderPoint(const CScenarioPoint& point, int regionIndex, bool editorActive, CPopZone* pZone, const CScenarioEntityOverride* pEntityOverride = NULL, int index = 0);
	static bool IsTimeInRange(u32 currHour, u32 startHour, u32 endHour);
	static bool CanRenderPoint(const CScenarioPoint& point, CPopZone* pZone);
	static bool CanRenderPointType(unsigned pointTypeVirtualOrReal);

	static void RenderChainEdge(const CScenarioChainingGraph& graph, const CScenarioChainingEdge& edge, bool editorActive, CPopZone* pZone);
	static const char* GetRegionDisplayName(int regionIndex);
	static const char* GetRegionSource(int regionIndex);

	// PURPOSE:	Filter that ignores any point that is in use
	class CScenarioDebugClosestFilterScenarioInUse : public CScenarioDebugClosestFilter
	{
	public:
		explicit CScenarioDebugClosestFilterScenarioInUse();
		virtual bool Match(const CScenarioPoint& scenarioPt) const;
		bool		m_IgnoreInUse;
	};

	// PURPOSE:	Filter that only accepts a specific type of scenarios.
	class CScenarioDebugClosestFilterScenarioType : public CScenarioDebugClosestFilterScenarioInUse
	{
	public:
		explicit CScenarioDebugClosestFilterScenarioType(int scenarioType);
		virtual bool Match(const CScenarioPoint& scenarioPt) const;
		unsigned	m_ScenarioType;
	};

	// PURPOSE:	Filter that only accepts scenarios that are not in CScenarioDebug's
	//			memory, and are of a given type.
	class CScenarioDebugClosestFilterNotVisited : public CScenarioDebugClosestFilterScenarioType
	{
	public:
		explicit CScenarioDebugClosestFilterNotVisited(int scenarioType);
		virtual bool Match(const CScenarioPoint& scenarioPt) const;
	};

	// PURPOSE:	Filter that only accepts one specific scenario point.
	class CScenarioDebugClosestFilterSpecific : public CScenarioDebugClosestFilterScenarioType
	{
	public:
		explicit CScenarioDebugClosestFilterSpecific(int scenarioType, CScenarioPoint* pScenarioPoint);
		virtual bool Match(const CScenarioPoint& scenarioPt) const;
		CScenarioPoint* m_pScenarioPoint;
	};

	static CPed* GetFocusPed();
	static void WarpToNextNearestScenarioCB();
	static void WarpToPreviousScenarioCB();
	static bool WarpTo(CPed& focusPed, const CScenarioPoint& scenarioPt);
	static void CreateAndActivateScenarioOnSelectedPedCB();
	static void DeActivateScenarioOnSelectedPedCB();
	static void DestroyScenarioProp();
	static void LoadPropOffsetsCB();
	static void ForceSpawnInAreaCB();
	static void ClearSpawnHistoryCB();

	static void ReloadAmbientData();
	static void GiveRandomProp();
	static void GivePedAmbientTaskCB();
	static u32 GetAmbientFlags();

	// PURPOSE:	Get the scenario point closest to a given position, which pass the conditions of a filter object.
	static CScenarioPoint* GetClosestScenarioPoint(Vec3V_In fromPos, const CScenarioDebugClosestFilter& filter);

	//
	// Members
	//

	static bkBank* ms_pBank;
	static bkButton* ms_pCreateEditorButton;
	static bkButton* ms_pCreateConditionalAnimsButton;
	static bkButton* ms_pCreateDebugButton;
	static bkButton* ms_pCreateModelSetsButton;
	static bkButton* ms_pCreateOtherButton;
	static bkButton* ms_pCreateScenarioInfoButton;
	static bkButton* ms_pCreateScenarioReactsButton;
	static bkButton* ms_pTriggerReactsButton;
	static bkButton* ms_pTriggerAllReactsButton;
	static bkButton* ms_pSpawnScenarioButton;
	static bkButton* ms_pCleanUpScenarioButton;
	static bkButton* ms_pSpawnPropButton;
	static bkCombo* ms_scenarioGroupCombo;

	static atArray<const char*> ms_szScenarioNames;
	static s32 ms_iScenarioComboIndex;
	static s32 ms_iScenarioGroupIndex;
	static s32 ms_iAvailabilityComboIndex;

	struct ConditionalAnimData
	{
		const CConditionalAnimsGroup*	m_Group;
		int								m_AnimIndexWithinGroup;
	};
	static atArray<ConditionalAnimData> ms_ConditionalAnims;
	static atArray<const char*> ms_ConditionalAnimNames;
	static s32 ms_iAmbientContextComboIndex;

	// PURPOSE:	Entries for the ms_WarpToNearestAlreadyVisited queue.
	// NOTE:	These pointers aren't necessarily safe to dereference. They are used
	//			in pointer comparisons against objects that are known to be valid.
	struct ScenarioPointMemory
	{
		RegdScenarioPnt m_pScenarioPoint;
	};

	// PURPOSE:	Memory of scenario points that have recently been warped to, used
	//			to implement the "warp to next closest" feature.
	static atQueue<ScenarioPointMemory, 32> ms_WarpToNearestAlreadyVisited;

	// PURPOSE:	Keeps track of which prop should be generated next, when creating
	//			props that a scenario being debugged is attached to.
	static int ms_NextScenarioPropIndex;

	// PURPOSE:	The scenario type the entries in ms_WarpToNearestAlreadyVisited are for,
	//			so we can clear ms_WarpToNearestAlreadyVisited when a new scenario
	//			type is chosen.
	static int ms_WarpToNearestScenarioType;

	static RegdObj ms_DebugScenarioProp;

	static bool ms_UseFixedPhysicsForProps;
	static bool ms_UseScenarioCreateProp;
	static bool ms_UseScenarioFindExisting;
	static bool ms_UseScenarioWarp;
	static bool ms_StopTargetPedImmediate;
	
	// Ambient Task Flags
	static bool ms_bPlayBaseClip;
	static bool ms_bInstantlyBlendInBaseClip;
	static bool ms_bPlayIdleClips;
	static bool ms_bPickPermanentProp;
	static bool ms_bForcePropCreation;
	static bool ms_bOverrideInitialIdleTime;

	// Scenario Reacts Debug members
	static atArray<const char*> ms_szScenarioTypeNames;
	static atArray<const char*> ms_szPropSetNames;
	static atArray<const char*> ms_szPedModelNames;
	static atArray<const char*> ms_szReactionTypeNames;
	static atArray<CPedModelInfo*> ms_PedModelInfoArray;
	static RegdPed ms_pScenarioPed;
	static RegdObj ms_pScenarioProp;
	static RegdPed ms_pReactionPed;
	static s32* ms_pedSortArray;
	static int ms_iScenarioTypeIndex;
	static int ms_iPropSetIndex;
	static int ms_iPedModelIndex;
	static int ms_iReactPedModelIndex;
	static int ms_iReactionTypeIndex;
	static float ms_fReactionHeading;
	static float ms_fReactionDistance;
	static float ms_fDistanceFromPlayer;
	static float ms_fTimeToLeave;
	static float ms_fTimeBetweenReactions;
	static bool ms_bSpawnPedToUsePropScenario;
	static bool ms_bTriggerOnFocusPed;
	static bool ms_bPlayIntro;
	static bool ms_bDontSpawnReactionPed;
};
#endif // SCENARIO_DEBUG

#endif // SCENARIO_DEBUG_H
