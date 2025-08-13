#ifndef PED_DEBUG_VISUALISER_H
#define PED_DEBUG_VISUALISER_H

#include "grcore/debugdraw.h"

#define PEDDEBUGVISUALISER DEBUG_DRAW
#if PEDDEBUGVISUALISER
#define PEDDEBUGVISUALISER_ONLY(x)	x
#else
#define PEDDEBUGVISUALISER_ONLY(x)
#endif

#if PEDDEBUGVISUALISER

// rage headers
#include "atl/array.h"
#include "atl/string.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "crmotiontree/nodefactory.h"
#include "vector/vector3.h"

// game headers
#include "animation/debug/AnimDebug.h"
#include "Streaming/streamingrequest.h"
#include "Task/Physics/TaskNM.h"

namespace rage {
	class fwPtrList;
}

class CPed;
#include <string>
class CEntity;
class CObject;
class CTask;
class CDynamicEntity;
class CVehicle;

class CClipDebugIterator : public crmtIterator
{	
public:
	CClipDebugIterator( const Vector3& worldCoords, crmtNode* referenceNode = NULL)
	{ 
		(void)referenceNode;
		m_worldCoords = worldCoords;
		m_noOfTexts = 0;
	};

	~CClipDebugIterator() {};

	// PURPOSE: Iterator visit override
	virtual void Visit(crmtNode&);
	static atString GetClipDebugInfo(const crClip* pClip, float Time, float weight, bool& addedContent);
	s32 GetNoOfLines() const { return m_noOfTexts; }

protected:

	float CalcVisibleWeight(crmtNode& node);

	Vector3 m_worldCoords;
	int m_noOfTexts;
};

#if !__FINAL
class CPedDebugVisualiser
{
	friend class CPedDebugVisualiserMenu;
	friend class CDamageDebugInfo;

public:

	enum ePedDebugVisMode
	{
		eOff,
		eTasksFullDebugging,
		eTasksNoText,
		eVehicleEntryDebug,
		ePedNames,
		eModelInformation,
		eAnimationDebugging,
		eFacialDebugging,
		eVisibilityDebugging,
		eGestureDebugging,
		eIKDebugging,
		eCombatDebugging,
		eRelationshipDebugging,
		ePedGroupDebugging,
		eMovementDebugging,
		eRagdollDebugging,
		eRagdollTypeDebugging,
		eQueriableStateDebugging,
		ePedAmbientAnimations,
		ePedDragging,
		ePedLookAboutScoring,
		eScanners,
		eAvoidance,
		eScriptHistory,
		eAudioContexts,
		eTaskHistory,
		eCurrentEvents,
		eEventHistory,
		eMissionStatus,
		eNavigation,
		eStealth,
		eAssistedMovement,
		eAttachment,
		eLOD,
		eMotivation,
		eWanted,
		eOrder,
		ePersonalities,
		ePedFlags,
		eDamage,
		eCombatDirector,
		eTargeting,
		eFocusAiPedTargeting,
		ePopulationInfo,
		eArrestInfo,
		eVehLockInfo,
		ePlayerCoverSearchInfo,
		ePlayerInfo,
		eParachuteInfo,
		eMelee,

		eSimpleCombat, // please keep this last in the list

		eNumModes
	};

	enum ePedDebugNMDebugMode
	{
		eNMOff,
		eNMInteractiveShooter,
		eNMInteractiveGrabber,

		eNumNMDebugModes
	};

	static ePedDebugVisMode			eDisplayDebugInfo;
	static ePedDebugNMDebugMode		eDebugNMMode;

	static float ms_fDefaultVisualiseRange; // initialised to DEFAULT_VISUALISE_RANGE
	static float ms_fVisualiseRange;	// initialised to ms_fDefaultVisualiseRange each frame

    CPedDebugVisualiser() { }
    ~CPedDebugVisualiser() { }
           
#if !__FINAL
	void VisualiseAll();
#endif
    void VisualisePedsNearPlayer();
    void VisualisePlayer();
	void VisualiseDamage(const CPed& ped, const bool bOnlyDamageRecords = false) const; 

	static ePedDebugVisMode GetDebugDisplay() { return eDisplayDebugInfo; }
	static void SetDebugDisplay(ePedDebugVisMode mode) { eDisplayDebugInfo = mode; }
	static void SetDetailedDamageDisplay(const bool Val) { ms_bDetailedDamageDisplay = Val; }
	static bool GetDetailedDamageDisplay() { return ms_bDetailedDamageDisplay; }
	static void DisplayPedDamageRecords(const bool bDisplay, const bool bDetailed = false);
private:
	
	void VisualiseStateHistoryForTask(const CTask& task, const Vector3& WorldCoors, s32 iAlpha, s32& iNoOfTexts, char* tmp) const;
    void VisualiseText(const CPed& ped) const;
    void VisualiseBoundingVolumes(CPed& ped) const;
    void VisualiseHitSidesToPlayer(CPed& ped) const;
    void VisualiseTasks(const CPed& ped) const;
	void VisualiseVehicleEntryDebug(const CPed& ped) const;
    void VisualisePosition(const CPed& ped) const;
    void VisualiseFOV(const CPed& ped) const;
	bool VisualiseCombatText(const CPed& ped) const;
	bool VisualiseSimpleCombatText(const CPed& ped) const;
	bool VisualiseRelationshipText(const CPed& ped) const;
	bool VisualisePedGroupText(const CPed& ped) const;
	bool VisualiseAnimation(const CDynamicEntity& dynamicEntity) const;
	bool VisualiseFacial(const CDynamicEntity& dynamicEntity) const;
	bool VisualiseGesture(const CPed& ped) const;
	bool VisualiseVisibility(const CDynamicEntity& dynamicEntity) const;
	bool VisualiseIK(const CPed& ped) const;
	bool VisualiseQueriableState(const CPed& ped) const;
	bool VisualiseMovement(const CPed& ped) const;
	bool VisualiseRagdoll(const CPed& ped) const;
	bool VisualiseRagdollType(const CPed& ped) const;
	bool VisualiseAmbientAnimations(const CPed& ped) const;
	bool VisualiseModelInformation(const CPed& ped) const;
	bool VisualisePedNames(const CPed& ped) const;

	// Visualise the player ped's scoring, unless there is a focus ped then visualise that ped's scoring.
	bool VisualiseLookAbout(const CEntity& entity) const;
	void VisualiseScanners(const CPed& ped) const;
	void VisualiseAvoidance(const CPed& ped) const;
	void VisualiseNavigation(const CPed& ped) const;
	void VisualiseScriptHistory(const CPed& ped) const;
	void VisualiseAudioContexts(const CPed& ped) const;
	void VisualiseTaskHistory(const CPed& ped) const;
	void VisualiseCurrentEvents(const CPed& ped) const;
	void VisualiseEventHistory(const CPed& ped) const;
	void VisualiseMissionStatus(const CPed& ped) const;
	void VisualiseStealth(const CPed& ped) const;
	void VisualiseAssistedMovement(const CPed& ped) const;
	void VisualiseAttachments(const CPed& ped) const;
	void VisualiseLOD(const CPed& ped) const;
	void VisualiseMotivation(const CPed& ped)const; 
	void VisualiseWanted(const CPed& ped)const; 
	void VisualiseOrder(const CPed& ped)const; 
	void VisualisePersonalities(const CPed& ped)const; 
	void VisualisePedFlags(const CPed& ped)const; 
	void VisualiseCombatDirector(const CPed& ped)const; 
	void VisualiseTargeting(const CPed& ped)const;
	void VisualiseFocusAiPedTargeting(const CPed& ped)const;
	void VisualisePopulationInfo(const CPed& ped) const;
	void VisualiseArrestInfo(const CPed& ped) const;
	void VisualiseVehicleLockInfo(const CPed& ped) const;
	void VisualisePlayerCoverSearchInfo(const CPed& ped) const;
	void VisualisePlayerInfo(const CPed& ped) const;
	void VisualiseParachuteInfo(const CPed& ped) const;
	void VisualiseMelee(const CPed& ped) const;

    void VisualisePed(const CPed& ped) const;
	void VisualiseVehicle(const CVehicle& vehicle) const;
	void VisualiseObject(const CObject& object) const;
 
	void VisualiseNearbyVehicles(const CPed& ped) const;
	void VisualiseOneVehicle(const CPed& ped, const CVehicle& vehicle) const;
    
	void VisualiseTextViaMenu(const CPed& ped) const;
	void VisualiseRelationships(const CPed& ped) const;

	void VisualiseEntityName(const CEntity& entity, const char* strName) const;

	void DebugJacking();
  
    class CTextDisplayFlags
    {
    public:
    	bool		m_bDisplayAnimList;
		bool		m_bDisplayTasks;
		bool		m_bDisplayPersonality;
   	
    	CTextDisplayFlags();
    	~CTextDisplayFlags() {}
    };
    
    static CTextDisplayFlags m_textDisplayFlags;
	static bool ms_bDetailedDamageDisplay;
};
#endif // !__FINAL

#if !__FINAL

struct TNavMeshRouteDebugToggles
{
public:

	TNavMeshRouteDebugToggles()
	{
		m_bUseFreeSpaceRefVal = false;
		m_bFavourFreeSpaceGreaterThanRefVal = false;
		m_bUseDirectionalCover = false;
		m_iFreeSpaceRefVal = 0;
		m_vThreatOrigin = Vector2(0.0f, 0.0f);
		m_bGoToPointAnyMeans = false;
	}
	bool m_bUseFreeSpaceRefVal;
	bool m_bFavourFreeSpaceGreaterThanRefVal;
	int m_iFreeSpaceRefVal;
	bool m_bUseDirectionalCover;
	Vector2 m_vThreatOrigin;
	bool m_bGoToPointAnyMeans;
};

class CPedDebugVisualiserMenu
{
private:
	enum eSearchType
	{
		CLOSEST,
		LEFT,
		RIGHT
	};
	
	class CMenuFlags
	{
	public:
		bool		m_bDisplayAddresses;
		bool		m_bDisplayTasks;
		bool		m_bDisplayTaskHierarchy;
		bool		m_bDisplayEvents;
		bool		m_bDisplayHitSides;
		bool		m_bDisplayBoundingVolumes;
		bool		m_bDisplayGroupInfo;
		bool		m_bDisplayAnimatedCol;

		bool		m_bDisplayCombatDirectorAnimStreaming;
		bool		m_bDisplayPrimaryTaskStateHistory;
		bool		m_bDisplayMovementTaskStateHistory;
		bool		m_bDisplayMotionTaskStateHistory;
		bool		m_bDisplaySecondaryTaskStateHistory;
		bool		m_bVisualisePedsInVehicles;
		bool		m_bOnlyDisplayForFocus;
		bool		m_bFocusPedDisplayIn2D;
		bool		m_bOnlyDisplayForPlayers;
		bool		m_bDontDisplayForMounts;
		bool		m_bDontDisplayForDeadPeds;
		bool		m_bDontDisplayForPlayer;
		bool		m_bDontDisplayForRiders;
		bool		m_bDisplayDebugShooting;
		bool		m_bVisualiseRelationships;
		bool		m_bVisualiseDefensiveAreas;
		bool		m_bDisableVisualiseDefensiveAreasForDeadPeds;
		bool		m_bVisualiseSecondaryDefensiveAreas;
		bool		m_bVisualiseAttackWindowForFocusPed;
		bool		m_bVisualiseNearbyLadders;
		bool		m_bDisplayPedBravery;
		bool		m_bDisplayCoverSearch;
		bool		m_bDisplayCoverLineTests;
		bool		m_bDisplayPedGroups;
		bool		m_bDisableEvasiveDives;
		bool		m_bDisableFlinchReactions;
		bool		m_bLogPedSpeech;
		bool		m_bDisplayDebugAccuracy;
		bool		m_bDisplayScriptTaskHistoryCodeTasks;
		bool		m_bDisplayPedSpatialArray;
		bool		m_bDisplayVehicleSpatialArray;
		bool		m_bLogAIEvents;
		bool		m_bDisplayDamageRecords;
		bool		m_bRenderInitialSpawnPoint;
		CMenuFlags();
		~CMenuFlags() {}
	};


private:
	static CPedDebugVisualiser 	m_debugVis;

public:
	static void Initialise();
	static void Shutdown();

#if __BANK
	static void InitBank();
	static void CreateBank();
	static void ShutdownBank();

	// Pointer to the main RAG bank
	static bkBank*		ms_pBank;
	static bkButton*	ms_pCreateButton;
#endif

	static void Process();
	static void Render();

	// Registered with CDebugScene, called any time the focus entity is changed
	static void OnChangeFocusPedCB(CEntity * pEntity);

	static void InitMovementGroupWidget(bkBank & bank);

	static bool IsDisplayingOnlyForFocus() { return (ms_menuFlags.m_bOnlyDisplayForFocus); }
	static bool IsFocusPedDisplayIn2D() { return (ms_menuFlags.m_bFocusPedDisplayIn2D); }
	static bool IsDisplayingOnlyForPlayers() { return (ms_menuFlags.m_bOnlyDisplayForPlayers); }
	static void SetDisplayingOnlyForPlayers(bool b) { ms_menuFlags.m_bOnlyDisplayForPlayers = b; }
	static bool IsDisplayingForPlayer() { return (!ms_menuFlags.m_bDontDisplayForPlayer); }
	static bool GetDisplayDebugShooting() { return (ms_menuFlags.m_bDisplayDebugShooting); }
	static bool IsVisualisingRelationships() { return (ms_menuFlags.m_bVisualiseRelationships); }
	static bool IsVisualisingDefensiveAreas() { return (ms_menuFlags.m_bVisualiseDefensiveAreas); }
	static bool DisableVisualisingDefensiveAreasForDeadPeds() { return (ms_menuFlags.m_bDisableVisualiseDefensiveAreasForDeadPeds); }
	static bool IsVisualisingSecondaryDefensiveAreas() { return (ms_menuFlags.m_bVisualiseSecondaryDefensiveAreas); }
	static bool IsVisualisingAttackWindowForFocusPed() { return (ms_menuFlags.m_bVisualiseAttackWindowForFocusPed); }


	static bool GetDisplayDebugAccuracy() {return (ms_menuFlags.m_bDisplayDebugAccuracy);}
	static bool IsDebugPedDraggingActivated(void);
	static void UpdatePedDragging(void);
	static bool IsNMInteractiveShooterActivated(void);
	static void UpdateNMInteractiveShooter(void);
	static bool IsNMInteractiveGrabberActivated(void); 
	static void UpdateNMInteractiveGrabber(void);
	static bool IsDebugDefensiveAreaDraggingActivated(void);
	static bool IsDebugDefensiveAreaSecondary();
	static void UpdateDefensiveAreaDragging(void);
	static float GetCopSearchRadius(void);
	static bool GetForceAllCops(void);
	static void SetForceAllCops(bool bForce);
	static bool ShouldDisableEvasiveDives() { return (ms_menuFlags.m_bDisableEvasiveDives); }
	static bool ShouldDisableFlinchReactions() { return (ms_menuFlags.m_bDisableFlinchReactions); }
	static bool ShouldLogPedSpeech() { return (ms_menuFlags.m_bLogPedSpeech); }
	static void PrintTestHarnessParamsCB();
	static void TestHarnessCB();
	static void WarpToStartCB();
	static void StopThreatResponse();
	static void ChangeFocusToPlayer();

	static void SetSelectedConfigFlagsOnFocusPed();
	static void ClearSelectedConfigFlagsOnFocusPed();
	static void SetSelectedPostPhysicsResetFlagsOnFocusPed();
	static void ClearSelectedPostPhysicsResetFlagsOnFocusPed();

	static void SpewRelationShipGroupsToTTY();
	static void AddRelationShipGroup();
	static void RemoveRelationShipGroup();
	static void SetRelationShipGroup();
	static void SetBlipPedsInRelationshipGroup();
	static void SetRelationshipBetweenGroups();
	static void GetRelationshipBetweenGroups();
	static void ToggleFriendlyFire();

#if __BANK
	static s32 GetModelOverrideScenario();
#endif
	
	static u32 ms_FlushLogTime;
	static CMenuFlags ms_menuFlags;
	static int ms_nRagdollDebugDrawBone;
	static float ms_fRagdollApplyForceFactor;
	static float ms_fNMInteractiveShooterShotForce;
	static float ms_fNMInteractiveGrabLeanAmount;
	static bool ms_bUseHandCuffs;
	static bool ms_bUseAnkleCuffs;
	static bool ms_bFreezeRelativeWristPositions;
	static bool ms_bAnimPoseArms;
	static float ms_bHandCuffLength;
	static bool ms_bUseLeftArmIK;
	static bool ms_bUseRightArmIK;
	static Vector3 ms_vTargetLeftArmIK;
	static Vector3 ms_vTargetRightArmIK;
	static float ms_fArmIKBlendInTime;
	static float ms_fArmIKBlendOutTime;
	static float ms_fArmIKBlendInRange;
	static float ms_fArmIKBlendOutRange;
	static bool ms_bUseOriginalPosAsTarget;	
	static bool ms_bUseBoneAndOffset;
	static bool ms_bTargetInWorldSpace;
	static bool ms_bTargetWRTHand;
	static bool ms_bTargetWRTPointHelper;
	static bool ms_bTargetWRTIKHelper;
	static bool ms_bUseAnimAllowTags;
	static bool ms_bUseAnimBlockTags;

	static bool ms_bHeadIKEnable;
	static Vector3 ms_vHeadIKTarget;
	static bool ms_bHeadIKUseWorldSpace;
	static float ms_fHeadIKBlendInTime;
	static float ms_fHeadIKBlendOutTime;
	static float ms_fHeadIKTime;
	static bool ms_bSlowTurnRate;
	static bool ms_bFastTurnRate;
	static bool ms_bWideYawLimit;
	static bool ms_bWidestYawLimit;
	static bool ms_bWidePitchLimit;
	static bool ms_bWidestPitchLimit;
	static bool ms_bNarrowYawLimit;
	static bool ms_bNarrowestYawLimit;
	static bool ms_bNarrowPitchLimit;
	static bool ms_bNarrowestPitchLimit;
	static bool ms_bWhileNotInFOV;
	static bool ms_bUseEyesOnly;

	static Vector3 ms_vLookIKTarget;
	static bool ms_bLookIKEnable;
	static bool ms_bLookIKUseWorldSpace;
	static bool ms_bLookIKUseCamPos;
	static bool ms_bLookIkUsePlayerPos;
	static bool ms_bLookIKUpdateTarget;
	static bool ms_bLookIkUseTags;
	static bool ms_bLookIkUseTagsFromAnimViewer;
	static bool ms_bLookIkUseAnimAllowTags;
	static bool ms_bLookIkUseAnimBlockTags;
	static u8 ms_uRefDirTorso;
	static u8 ms_uRefDirNeck;
	static u8 ms_uRefDirHead;
	static u8 ms_uRefDirEye;
	static u8 ms_uRotLimTorso[2];
	static u8 ms_uRotLimNeck[2];
	static u8 ms_uRotLimHead[2];
	static u8 ms_uTurnRate;
	static u8 ms_auBlendRate[2];
	static u8 ms_uHeadAttitude;
	static u8 ms_auArmComp[2];

	static bool ms_bRouteRoundCarTest;
	static ePedConfigFlagsBitSet ms_DebugPedConfigFlagsBitSet;
	static ePedResetFlagsBitSet ms_DebugPedPostPhysicsResetFlagsBitSet;
	static bool ms_bDebugPostPhysicsResetFlagActive;

	static TNavMeshRouteDebugToggles ms_NavmeshRouteDebugToggles;

	enum eFocusPedTasks
	{
		eNoTask=0,
		eFollowEntityOffset,
		eStandStill,
		eFacePlayer,
		eAddToPlayerGroup,
		eRemoveFromPlayerGroup,
		eWander,
		eCarDriveWander,
		eUnalerted,
		eFlyingWander,
		eSwimmingWander,
		eGoToPointToCameraPos,
		eFlyToPointToCameraPos,
		eFollowNavmeshToMeasuringToolStopExactly,
		eFollowNavMeshToCameraPos,
		eGenericMoveToCameraPos,
		eFollowNavMeshToCameraPosWithSlide,
		eFollowNavMeshAndSlideInSequence,
		eSlideToCoordToCameraPos,
		eSetMoveTaskTargetToCameraPos,
		eSetDesiredHeadingToFaceCamera,
		eWalkAlongsideClosestPed,
		eTaskDie,
		eTestSequencePlayer,
		eTestMelee,
		eSeekCover,
		eUseMobilePhone,
		eHandsUp,
		eEvasiveStep,
		eAvoidanceTest,
		eGetOffBoat,
		eDefendCurrentPosition,
		eEnterNearestCar,
		eShuffleBetweenSeats,
		eExitNearestCar,
		eClearCharTasks,
		eFlushCharTasks,
		eDriveToLocator,
		eGoDirectlyIntoCover,
		eMobileChat,
		eMobileChat2Way,
		eShootAtPoint,
		eGoToPointShooting,
		eGoToPointShootingPlayer,
		eUseNearestScenario,
		eUseNearestScenarioWarp,
		eFallAndGetup,
		eClimbUp,
		eClimbVault,
		eReactToCarCollision,
		eNavMeshAndTurnToFacePedInSequence,
		eNMRelax,
		eNMPose,
#if __DEV
		eNMBindPose,
#endif	//	__DEV
		eNMShootingWhileAttached,
		eNMBrace,
		eNMBuoyancy,
		eNMShot,
		eNMMeleeHit,
		eNMHighFall,
		eNMBalance,
		eNMBalanceGrab,
		eNMDrunk,
		//eNMStumble,
		eNMExplosion,
		eNMOnFire,
		eNMFlinch,
		eNMRollUp,
		eNMFallOverWall,
		eNMRiverRapids,

		eTreatAccident,
		eDropOverEdge,
		
		eDragInjuredToSafety,
		eVariedAimPose,
		eCombat,
		eCowerCrouched,
		eCowerStanding,
		eShellShocked,
		eReachArm,
		eMoveFaceTarget,

		eShockingBackAway,
		eShockingHurryAway,
		eShockingReact,

		eSharkAttack,
		eSharkCircleForever,

		eCallPolice,
		eBeastJump,

		eNumFocusPedTasks
	};
	
	enum eFocusPedEvents
	{
		eNoEvent = 0,
		eCarUndriveable,
		eClimbNavMeshOnRoute,
		eGunshot,
		ePotentialGetRunOver,
		eRanOverPed,
		eScriptCommand,
		eShockingCarCrash,
		eShockingBicycleCrash,
		eShockingDrivingOnPavement,
		eShockingDeadBody,
		eShockingRunningPed,
		eShockingGunShotFired,
		eShockingWeaponThreat,
		eShockingCarStolen,
		eShockingInjuredPed,
		eShockingGunfight1,
		eShockingGunfight2,
		eShockingSeenPedKilled,
		eNumFocusPedEvents
	};

#if __BANK
	static bkCombo * m_pRelationshipsCombo;
	static bkCombo * m_pTasksCombo;
	static bkCombo * m_pEventsCombo;
	static bkCombo * m_pFormationCombo;
#endif
	static s32 m_iRelationshipsComboIndex;
	static atArray<const char*> m_scenarioNames;
	static atArray<s32> m_ScenarioComboTypes;
	static s32 m_iScenarioComboIndex;
	static atArray<eFocusPedTasks> m_TaskComboTypes;
	static s32 m_iTaskComboIndex;
	static atArray<eFocusPedEvents> m_EventComboTypes;
	static s32 m_iEventComboIndex;
	static atArray<u32> m_MovementGroupComboIDs;
	static s32 m_iMovementGroupComboIndex;
	static atArray<u32> m_FormationComboIDs;
	static s32 m_iFormationComboIndex;
	static float m_fFormationSpacing;
	static float m_fFormationAccelDist;
	static float m_fFormationDecelDist;
	static u32 m_iFormationSpacingScriptId;
	static bool ms_bDebugSearchPositions;
	static bool ms_bRagdollContinuousTestCode;
	static bool ms_bRenderRagdollTypeText;
	static bool ms_bRenderRagdollTypeSpheres;

#if __DEV
	static bkToggle * m_pToggleAssertIfFocusPedFailsToFindNavmeshRoute;
	static bool m_bAssertIfFocusPedFailsToFindNavmeshRoute;
	static void OnAssertIfFocusPedFailsToFindNavmeshRoute();
#endif

#if __BANK
	static bool ms_bUseNewLocomotionTask;
	static char ms_onFootClipSetText[60];

	static CDebugClipSelector	m_alternateAnimSelector;
	static float				m_alternateAnimBlendDuration;
	static bool					m_alternateAnimLooped;

	static bool					m_actionModeRun;

	// Start params
	static Matrix34		m_StartMat;
	static bool			m_bUseStartPos;

	// Vehicle debug options
	// 
	static s32			m_Mission;
	static RegdEnt		m_TargetEnt;
	static Vector3		m_TargetPos;
	static s32			m_DrivingFlags;
	static float		m_StraightLineDistance;
	static float		m_TargetReachedDistance;
	static float		m_CruiseSpeed;
	static float		m_fHeliOrientation;
	static s32			m_iFlightHeight;
	static s32			m_iMinHeightAboveTerrain;
	static bool			m_abDrivingFlags[32];

	//Plane testing
	static bool			ms_bTestPlaneAttackPlayer;
	static bool			ms_bTestPlanePlayerControl;
	static s32			ms_iTestPlaneNumToSpawn;
	static Vector3		m_goToTarget;
	static s32			ms_iTestPlaneType;

	// Relationship groups
	static bool			ms_BlipPedsInRelGroup;
	static char			ms_addRemoveSetRelGroupText[60];
	static char			ms_RelGroup1Text[60];
	static char			ms_RelGroup2Text[60];

	// Event History options
	static bool			ms_bShowRemovedEvents;
	static bool			ms_bHideFocusedPedsEventHistoryText;
	static bool			ms_bShowFocusedPedsEventPositions;
	static bool			ms_bShowFocusedPedsEventPositionsStateInfo;


#endif //__BANK
private:

#if __BANK

	static void StartAlternateWalkAnimSelectedPed();
	static void EndAlternateWalkAnimSelectedPed();

	static void ToggleActionMode();
	static void ToggleMovementModeOverride();

#endif // __BANK

	static void ToggleDisplayAddresses(void);
	static void ToggleVisualisePedsInVehicles(void);
	static void ToggleDisplayGroupInfo(void);

	static void ToggleDisplayAnims(void);
	static void ToggleDisplayTaskHeirarchies(void);
	static void ToggleDisplayEvents(void);
	static void ToggleDisplayPersonality(void);
	static void ToggleDisplayAttractors(void);
	static void ToggleDisplayAnimatedCol(void);
	
	static void TogglePlayerInvincibility();
	static void TogglePlayerInvincibilityRestoreHealth();

	static void ChangeFormationSpacing();

	static void ToggleDisplayCoverPoints(void);

	static void DisplayNearbyPedIDs();
	static void DisplayGroupInfo();
	static void DisplayPedInfo(const CPed& ped);
	static void DisplayPedID(const CPed& ped);
	static void DisplayDamageRecords();
	static void RenderInitialSpawnPointOfSelectedPed();
	// Displays the relationships by drawing lines between the focus peds and all other related peds

	static void ChangeFocusToClosestToPlayer();
	static void ChangeFocusForwards();
	static void ChangeFocusBack();
	static void ChangeFocus(int iDir);

	// IK 
	static void UpdateHeadBoneLimits();

	static void IKHeadTest1();
	static void IKHeadTest2();
	static void IKHeadTest3();
	static void IKHeadTest4();
	static void IKHeadTest5();
	static void IKHeadTest6();
	static void IKHeadTest7();
	static void ProcessHeadIK();
	static void ProcessLookIK();

	static void ProcessArmIK();

	static void IKArmTest1();
	static void IKArmTest2();
	static void IKArmTest3();
	static void IKArmTest4();
	static void IKArmTest5();

	static void IKLegTest1();
	static void IKLegTest2();
	static void IKLegTest3();
	static void IKLegTest4();
	static void IKLegTest5();

	static void ProcessRouteRoundCarTest();

	static void AddTaskStandStill();
	static void AddOrRemovePedFromGroup(CPed * pLeaderPed, CPed * pFollower, bool bAdd);
	static void AddTaskGotoPlayer();
	static void MoveFollowEntityOffset();
	static void AttackPlayersCar();
	static void DropPedAtCameraPos();
public:
	static void IncreaseWantedLevel();
	static void DecreaseWantedLevel();
	static void TriggerArrest();

	static void ForceSpawnInArea();

	static void MakePedFollowNavMeshToCoord(CPed * pPed, const Vector3 & vPos);
	static void AssistedMovement_DesertTest(int iTestArea);

	static void ToggleAllowLegInterpenetration(void);
	static void SpewRagdollTaskInfo(void);
	static void PrintRagdollUsageData(void);
	static void ToggleNaturalMotionAnimPose(void);
	static void ToggleNaturalMotionHandCuffs(void);
	static void ToggleNaturalMotionAnkleCuffs(void);
	static void ToggleNaturalMotionFlagDebug(void);
	static void ToggleNaturalMotionAPILog(void);
	static void ToggleDistributedTasksEnabled(void);
	static void ReloadTaskParameters(void);
	static void ReloadBlendFromNmSets(void);
	static void UpdateBlendFromNmSetsWidgets(void);
	static void ReloadWeaponTargetSequences(void);
	static void SwitchPedToNMBehaviour(CTask* TaskNM);
	static void SwitchPedToRagdollCB(void);
	static void SwitchPedToNMBalanceCB(void) {SwitchPedToNMBehaviour(NULL);}
	static void SwitchPedToAnimatedCB(void);
	static void SwitchPedToDrivenRagdollCB(void);
	static void ForcePedToAnimatedCB(void);
	static void SwitchToStaticFrameCB(void);
	static void ClearAllBehavioursCB(void);
	static void SetRelaxMessageCB(void);
	static void UnlockPedRagdollCB(void);
	static void FixPedPhysicsCB(void);
	static void TriggerRagdollTestCodeCB(void);
	static void RagdollContinuousDebugCode(void);
	static CPed* GetFocusPed(void);
	static void CreatePolicePursuitCB(void);
	static void CreateVehiclePolicePursuitCB(void);
	static void CreateJetTest(void);
	static void CreateHeliTest(void);
	static void SendNMFallOverInstructionToFocusEntity(void);

	static void ApplyOnFootClipSetToSelectedPed(void);
private:
	static void WantedLevelAlwaysZero();
    static void SwitchPlayerToFocusPed();

	// Force the game to run special logic dealing with the player trying to leave the map.
	static void TriggerWorldExtentsProtection();

	// Test shark spawning.
	static void TriggerSharkSpawnToCamera();

	static void KillFocusPed(void);
	static void ReviveFocusPed(void);
	static void JamesDebugTest(void);
	

	static void OnSelectTaskCombo(void);
	static void OnSelectEventCombo(void);
	static void OnSelectFormationCombo();

#if __BANK
	static void SetVehicleMissionTargetEntity(void);
	static void ClearVehicleMissionTargetEntity(void);
	static void SetStartPosToMeasuringTool(void);
	static void SetStartPosToDebugCam(void);
	static void SetStartPosToFocusPos(void);
	static void SetVehicleMissionTargetPosToMeasuringTool(void);
	static void SetVehicleMissionTargetPosToDebugCam(void);
	static void SetVehicleMissionTargetPosToFocusPos(void);
	static void SetVehicleMissionTargetPosToRandomRoadPos(void);
	static void SetVehicleDrivingFlags(void);
#endif
	static void DropWeaponCB(void);

	static CPed* FindPed(const CPed* pPed, const Vector3& vCurrentDirectoin, eSearchType searchType);
	static void MakeFocusPedMoveScenarioToCamera();
};
#endif // !__FINAL

#endif // PEDDEBUGVISUALISER

#endif	// PED_DEBUG_VISUALISER_H
