// 
// task/motion/TaskMotionBase.h
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved. 
// 

// Rage headers
#include "math/angmath.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"
#include "fwmaths/Vector.h"

// Game headers
#include "ai/EntityScanner.h"
#include "Animation/MovePed.h"
#include "camera/CamInterface.h"
#include "camera/helpers/frame.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Task/Movement/Climbing/TaskLadderClimb.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Task/Movement/Climbing/Ladders/LadderMetadataManager.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/climbing/TaskGoToAndClimbLadder.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskNavBase.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Animation/TaskAnims.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedMoveBlend/PedMoveBlendManager.h"
#include "Peds/Ped.h"
#include "Physics/gtaInst.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Stats/StatsInterface.h"
#include "fwscene/search/SearchVolumes.h"
#include "debug/DebugScene.h"
#include "Task/Default/TaskPlayer.h"

ANIM_OPTIMISATIONS()
AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//----------------------------------------------------------------------
RAGE_DEFINE_CHANNEL(ladder)
//----------------------------------------------------------------------

//----------------------------------------------------------------------

CTaskFSMClone *CClonedClimbLadderInfo::CreateCloneFSMTask()
{
 	return rage_new CTaskClimbLadder();
}

void CClonedClimbLadderInfo::Serialise(CSyncDataBase& serialiser)
{
	static const u32 SIZEOF_STARTPOS_ELEMENTS = 24;

	CSerialisedFSMTaskInfo::Serialise(serialiser);

	SERIALISE_ANIMATION_CLIP(serialiser, m_ClipSetId, m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall,	"Animation");
	SERIALISE_UNSIGNED(serialiser, m_ladderPosHash, SIZEOF_LADDER_POS_HASH, "Ladder Position Hash");
	SERIALISE_INTEGER(serialiser, m_effectIndex, SIZEOF_EFFECT_INDEX, "Effect Index");

	Vector3 minExtent, maxExtent;
	netSerialiserUtils::GetAdjustedWorldExtents(minExtent, maxExtent);

	float fMaxExtent = MAX(MAX(maxExtent.x,maxExtent.y),maxExtent.z);
	SERIALISE_VECTOR(serialiser,m_startPos,fMaxExtent,SIZEOF_STARTPOS_ELEMENTS,"StartPos");
	
	SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_startState), SIZEOF_START_STATE, "Start State");
	SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_ladderType), SIZEOF_LADDER_TYPE, "Ladder Type");
	SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(m_inputState), SIZEOF_INPUT_STATE, "Input State");

	float heading = PackHeading();
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, heading, PI * 2.0f, SIZEOF_HEADING, "Heading");
	UnpackHeading(heading);

	SERIALISE_BOOL(serialiser, m_isPlayer, "Is Player");

	if((!m_isPlayer) || (serialiser.GetIsMaximumSizeSerialiser()))
	{
		float movementOffset = m_movementBlockedOffset;
		SERIALISE_PACKED_FLOAT(serialiser, movementOffset, 0.5f, SIZEOF_MOVEMENT_BLOCKED_OFFSET, "MovementBlockedOffset");
	}

	bool getOnBackwards = m_getOnBackwards;
	SERIALISE_BOOL(serialiser,  getOnBackwards, "Get On Ladder Backwards");
	m_getOnBackwards = getOnBackwards;

	bool speed = m_speed;
	SERIALISE_BOOL(serialiser, speed, "Speed");
	m_speed = speed;

	bool canSlideDown = m_bCanSlideDown;
	SERIALISE_BOOL(serialiser, canSlideDown, "CanSlideDown");
	m_bCanSlideDown = canSlideDown;	

	bool rungCalculated = m_rungCalculated;
	SERIALISE_BOOL(serialiser, rungCalculated, "RungCalculated");
	m_rungCalculated = rungCalculated;

	bool dismountRightHand = m_dismountRightHand;
	SERIALISE_BOOL(serialiser, dismountRightHand, "Dismount Right Hand");
	m_dismountRightHand = dismountRightHand;
}

//----------------------------------------------------------------------

bank_float CTaskClimbLadder::ms_fHeightBeforeTopToFinishT = 1.0f;
bank_float CTaskClimbLadder::ms_fHeightBeforeBottomToFinishT = 1.75f;

//flags
const fwMvFlagId CTaskClimbLadder::ms_MountForwards("MountForwards",0x950FD569);
const fwMvFlagId CTaskClimbLadder::ms_DismountRightHand("DismountRightHand",0x5133FAC6);
const fwMvFlagId CTaskClimbLadder::ms_DismountWater("DismountWater",0x90E08A59);
const fwMvFlagId CTaskClimbLadder::ms_SettleDownward("SettleDownward",0x3EA341CC);
const fwMvFlagId CTaskClimbLadder::ms_LightWeight("LightWeight",0xC44AB530);
const fwMvFlagId CTaskClimbLadder::ms_MountRun("MountRun",0x4B0007AC);
const fwMvFlagId CTaskClimbLadder::ms_MountWater("MountWater",0x21A7538);
const fwMvFlagId CTaskClimbLadder::ms_MountBottomBehind("MountBottomBehind",0xADEE95C6);
const fwMvFlagId CTaskClimbLadder::ms_MountBottomBehindLeft("MountBottomBehindLeft",0xEA6658E8);
const fwMvFlagId CTaskClimbLadder::ms_MountBottomBehindRight("MountBottomBehindRight",0x23585F14);

//requests
const fwMvRequestId CTaskClimbLadder::ms_MountTop("MountTop",0x936564A5);
const fwMvRequestId CTaskClimbLadder::ms_MountBottom("MountBottom",0xBC1DD9E);
const fwMvRequestId CTaskClimbLadder::ms_DismountTop("DismountTop",0x4015FBA1);
const fwMvRequestId CTaskClimbLadder::ms_DismountBottom("DismountBottom",0x2905F00E);
const fwMvRequestId CTaskClimbLadder::ms_StartSliding("StartSliding",0x4CDBC4CA); 
const fwMvRequestId CTaskClimbLadder::ms_SlideDismount("SlideDismount",0xAB0B3B72); 
const fwMvRequestId CTaskClimbLadder::ms_ClimbUp("ClimbUp",0xADD765B8); 
const fwMvRequestId CTaskClimbLadder::ms_ClimbDown("ClimbDown",0x5330B3EE); 
const fwMvRequestId CTaskClimbLadder::ms_Idle("Idle",0x71C21326);

// states
const fwMvStateId CTaskClimbLadder::ms_slideState("Slide",0x74E3D185);
const fwMvStateId CTaskClimbLadder::ms_climbUpState("ClimbUp",0xADD765B8);
const fwMvStateId CTaskClimbLadder::ms_climbDownState("ClimbDown",0x5330B3EE);
const fwMvStateId CTaskClimbLadder::ms_idleState("IdleStateMachine",0x35285478);
const fwMvStateId CTaskClimbLadder::ms_mountBottomState("Mount Bottom",0xC7588054);
const fwMvStateId CTaskClimbLadder::ms_mountBottomStateRun("Mount Bottom Run",0x6379A172);
const fwMvStateId CTaskClimbLadder::ms_mountTopState("MountTop",0x936564A5);
const fwMvStateId CTaskClimbLadder::ms_dismountTopState("Dismount Top",0x32E52286);
const fwMvStateId CTaskClimbLadder::ms_dismountBottomState("Dismount Bottom",0x54FB6687);
const fwMvStateId CTaskClimbLadder::ms_slideDismountState("Slide Dismount",0x796E980E);

//bools
const fwMvBooleanId CTaskClimbLadder::ms_MountEnded("MountEnded",0xAE1E0F53);
const fwMvBooleanId CTaskClimbLadder::ms_LoopEnded("EndOfLoop",0x65414724); 
const fwMvBooleanId CTaskClimbLadder::ms_ChangeToFast("ChangeToFast",0x560AFF87); 
const fwMvBooleanId CTaskClimbLadder::ms_IsLooped("IsLooped",0xF11DCA8C); 
const fwMvBooleanId CTaskClimbLadder::ms_IsInterrupt("Interrupt",0x6D6BC7B7); 
const fwMvBooleanId CTaskClimbLadder::ms_IsInterruptNPC("InterruptNPC",0xAB50AF7E);
const fwMvBooleanId CTaskClimbLadder::ms_EnableHeadIK("EnableHeadIK",0x245F1214);

const fwMvBooleanId CTaskClimbLadder::ms_OnEnterClimbUp("OnEnterClimbUp",0x4F27ECFF);
const fwMvBooleanId CTaskClimbLadder::ms_OnEnterClimbDown("OnEnterClimbDown",0x75E63B8C);
const fwMvBooleanId CTaskClimbLadder::ms_OnEnterClimbIdle("OnEnterClimbIdle",0x5644EB5F);
const fwMvBooleanId CTaskClimbLadder::ms_OnEnterDismountTop("OnEnterDismountTop",0x668DFF4E);
const fwMvBooleanId CTaskClimbLadder::ms_OnEnterDismountBottom("OnEnterDismountBottom",0xC231C843);
const fwMvBooleanId CTaskClimbLadder::ms_OnEnterSlide("OnEnterSlide",0x1ECC71AF);
const fwMvBooleanId CTaskClimbLadder::ms_OnEnterSlideDismount("OnEnterSlideDismount",0x4792FD3E);
const fwMvBooleanId CTaskClimbLadder::ms_OnEnterMountTop("OnEnterMountTop",0xE9A8097C);
const fwMvBooleanId CTaskClimbLadder::ms_OnEnterMountTopRun("OnEnterMountTopRun",0x59A98932);
const fwMvBooleanId CTaskClimbLadder::ms_OnEnterMountBottom("OnEnterMountBottom",0x4424368);
const fwMvBooleanId CTaskClimbLadder::ms_OnEnterMountBottomRun("OnEnterMountBottomRun",0xCD585919);

//floats
const fwMvFloatId CTaskClimbLadder::ms_Speed("Speed",0xF997622B);
const fwMvFloatId CTaskClimbLadder::ms_MountAngle("MountAngle",0x267F52F5); 
const fwMvFloatId CTaskClimbLadder::ms_PhaseForwardLow("PhaseForwardLow",0xD6FCC44E);
const fwMvFloatId CTaskClimbLadder::ms_PhaseForwardHigh("PhaseForwardHigh",0x899FBA4D);

const fwMvFloatId CTaskClimbLadder::ms_WeightGetOnBottomFrontHigh("WeightGetOnBottomFrontHigh",0x1BAF9EB3); 
const fwMvFloatId CTaskClimbLadder::ms_WeightGetOnBottomRightHigh("WeightGetOnBottomRightHigh",0xB007032F); 
const fwMvFloatId CTaskClimbLadder::ms_WeightGetOnBottomLeftHigh("WeightGetOnBottomLeftHigh",0x4875C07B); 
const fwMvFloatId CTaskClimbLadder::ms_WeightGetOnBottomFrontLow("WeightGetOnBottomFrontLow",0x79B52A44); 
const fwMvFloatId CTaskClimbLadder::ms_WeightGetOnBottomRightLow("WeightGetOnBottomRightLow",0x1A2306CE); 
const fwMvFloatId CTaskClimbLadder::ms_WeightGetOnBottomLeftLow("WeightGetOnBottomLeftLow",0xAD36BADA); 

const fwMvFloatId CTaskClimbLadder::ms_WeightGetOnTopFront("WeightGetOnTopFront",0xBADFE2F9); 
const fwMvFloatId CTaskClimbLadder::ms_WeightGetOnTopLeft("WeightGetOnTopLeft",0x4258C012); 
const fwMvFloatId CTaskClimbLadder::ms_WeightGetOnTopRight("WeightGetOnTopRight",0xD6E3140F); 
const fwMvFloatId CTaskClimbLadder::ms_WeightGetOnTopBack("WeightGetOnTopBack",0x515077D4); 

const fwMvFloatId CTaskClimbLadder::ms_Phase("Phase",0xA27F482B); 
const fwMvFloatId CTaskClimbLadder::ms_PhaseGetOnTop("PhaseGetOnTop",0xC452AE22); 
const fwMvFloatId CTaskClimbLadder::ms_PhaseGetOnBottom("PhaseGetOnBottom",0x1A09820); 
const fwMvFloatId CTaskClimbLadder::ms_PhaseLeft("PhaseLeft",0x1FEF0EFC); 
const fwMvFloatId CTaskClimbLadder::ms_PhaseRight("PhaseRight",0x871275DA); 
const fwMvFloatId CTaskClimbLadder::ms_MountTopRunStartPhase("MountTopRunStartPhase",0x501DC32B);
const fwMvFloatId CTaskClimbLadder::ms_InputPhase("InputPhase",0x8FA35917);
const fwMvFloatId CTaskClimbLadder::ms_InputRate("InputRate",0x175F0423);
const fwMvFloatId CTaskClimbLadder::ms_InputRateFast("InputRateFast",0x685EE2C7);

//clips
const fwMvClipId CTaskClimbLadder::ms_CurrentClip("CurrentClip",0xA56F2358);

const fwMvClipId CTaskClimbLadder::ms_ClipGetOnBottomFrontHigh("ClipGetOnBottomFrontHigh",0x2FF7CCF2);
const fwMvClipId CTaskClimbLadder::ms_ClipGetOnBottomRightHigh("ClipGetOnBottomRightHigh",0x6E4B8868); 
const fwMvClipId CTaskClimbLadder::ms_ClipGetOnBottomLeftHigh("ClipGetOnBottomLeftHigh",0x4CD8022C); 
const fwMvClipId CTaskClimbLadder::ms_ClipGetOnBottomFrontLow("ClipGetOnBottomFrontLow",0x85C6F193); 
const fwMvClipId CTaskClimbLadder::ms_ClipGetOnBottomRightLow("ClipGetOnBottomRightLow",0x8C4D467C); 
const fwMvClipId CTaskClimbLadder::ms_ClipGetOnBottomLeftLow("ClipGetOnBottomFrontHigh",0x2FF7CCF2); 

const fwMvClipId CTaskClimbLadder::ms_ClipGetOnTopFront("ClipGetOnTopFront",0xE997EB88);
const fwMvClipId CTaskClimbLadder::ms_ClipGetOnTopLeft("ClipGetOnTopLeft",0x7D0E4CB1); 
const fwMvClipId CTaskClimbLadder::ms_ClipGetOnTopRight("ClipGetOnTopRight",0x4D9EDFB3); 
const fwMvClipId CTaskClimbLadder::ms_ClipGetOnTopBack("ClipGetOnTopBack",0x97A10B3F); 


const fwMvClipSetId CTaskClimbLadder::ms_ClipSetIdWaterMount("ANIM_GROUP_LADDERS_WATER_MOUNTS",0xB0A30B50);
const fwMvClipSetId CTaskClimbLadder::ms_ClipSetIdBase("ANIM_GROUP_LADDERS_BASE",0xBA533B7E);
const fwMvClipSetId CTaskClimbLadder::ms_ClipSetIdFemaleBase("ANIM_GROUP_LADDERS_FEMALE_BASE",0xB4488253);

CLadderClipSetRequestHelper CLadderClipSetRequestHelperPool::ms_LadderClipSetHelpers[ms_iPoolSize];
bool CLadderClipSetRequestHelperPool::ms_LadderClipSetHelperSlotUsed[CLadderClipSetRequestHelperPool::ms_iPoolSize] = {};
CPed* CLadderClipSetRequestHelperPool::sm_AssociatedPeds[ms_iPoolSize] = { NULL };

static const float	s_fClimbRateFast = 1.35f;

#if FPS_MODE_SUPPORTED
const u32 CTaskClimbLadder::sm_uTimeToAllowFPSToBreakOut = 400;
#endif

CTaskClimbLadder::CTaskClimbLadder(CEntity* pLadder, s32 LadderIndex, const Vector3& vTop, const Vector3& vBottom, const Vector3 & vStartPos, float fHeading, bool bCanGetOffAtTop,ClimbingState eStartState,bool bGetOnBackwards, eLadderType LadderType, CTaskGoToAndClimbLadder::InputState inputState)
: m_pLadder(pLadder)
, m_iDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE)
, m_EffectIndex(LadderIndex)
, m_eStartState(eStartState)
, m_eAIClimbState(inputState)
, m_vTop(vTop)
, m_vBottom(vBottom)
, m_fHeading(fHeading)
, m_vPositionToEmbarkLadder(vStartPos)
, m_fGoFasterTimer(0.0f)
, m_bCanGetOffAtTop(bCanGetOffAtTop)
, m_bCalledStartLadderSlide(false)
, m_bGetOnBackwards(bGetOnBackwards)
, m_bGetOnBehind(false)
, m_bGetOnBehindLeft(false)
, m_pEntityBlockingMovement(NULL)
, m_LadderType(LadderType)
, m_bFoundAbsoluteTop(false)
, m_bFoundAbsoluteBottom(false)
, m_LadderNormal(VEC3_ZERO)
, m_bNetworkBlendedIn(false)
, m_AngleBlend(0.0f)
, m_bTestedValidity(false)
, m_bCloneAudioSlidingFlag(false)
, m_bIsWaterMounting(false)
, m_bIsWaterDismounting(false)
, m_bCanSlideDown(true)
, m_bIgnoreNextRungCalc(false)
, m_bOnlyBase(false)
, m_bPDMatInitialized(false)
, m_bDismountRightHand(false)
, m_fSpeed(0.0f)
, m_RungSpacing(0.25f)
, m_fClimbRate(1.f)
, m_bLooped(false)
, m_BlendDonePhase(0.f)
, m_WaterLevel(0.f)
, m_fCurrentRung(0.f)
, m_ladderCheckRestart(false)
, m_bRungCalculated(false)
, m_bOwnerDismountedAtTop(false)
, m_bOwnerDismountedAtBottom(false)
, m_bPreventFPSMode(false)
, m_minHeightToBlock(0.0f)
, m_maxHeightToBlock(0.0f)
, m_topOfLadderBlockIndex(-1)
, m_bottomOfLadderBlockIndex(-1)
, m_vBlockPos(VEC3_ZERO)
, m_fBlockHeading(0.0f)
, m_ladderHash(0)
, m_fRandomIsMovementBlockedOffsetMP(0.0f)
, m_bOwnerTaskEnded(false)
{
	LoadMetaDataFromLadderType(); 
	CTaskGoToAndClimbLadder::GetNormalOfLadderFrom2dFx(m_pLadder, m_LadderNormal, m_EffectIndex ); 

	// We cannot slide so we climb fast instead!
	if (m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToSlideDown && !m_bCanSlideDown)
		m_eAIClimbState = CTaskGoToAndClimbLadder::InputState_WantsToClimbDownFast;

	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnBotScalar, 0.275f, 0.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnTopScalar, -0.275f, -1.0f, 1.0f, 0.1f);

	if (m_eStartState == State_MountingAtBottom || m_eStartState == State_MountingAtBottomRun)
		m_vPositionToEmbarkLadder = m_vBottom + (m_LadderNormal * GetOnBotScalar);
	else
		m_vPositionToEmbarkLadder = m_vTop - (m_LadderNormal  * GetOnTopScalar);

	SetInternalTaskType(CTaskTypes::TASK_CLIMB_LADDER);
}

CTaskClimbLadder::CTaskClimbLadder()
: m_pLadder(NULL)
, m_iDynamicObjectIndex(DYNAMIC_OBJECT_INDEX_NONE)
, m_ClipSetId(CLIP_SET_ID_INVALID)
, m_FemaleClipSetId(CLIP_SET_ID_INVALID)
, m_vTop(VEC3_ZERO)
, m_vBottom(VEC3_ZERO)
, m_vPositionToEmbarkLadder(VEC3_ZERO)
, m_LadderNormal(VEC3_ZERO)
, m_LadderType(eMaxLadderType)
, m_pEntityBlockingMovement(NULL)
, m_eStartState(State_Finish)
, m_eAIClimbState(CTaskGoToAndClimbLadder::InputState_Nothing)
, m_fHeading(0.0f)
, m_fGoFasterTimer(0.0f)
, m_AngleBlend(0.0f) 
, m_fSpeed(0.0f)
, m_RungSpacing(0.25f)
, m_BlendDonePhase(0.f)
, m_WaterLevel(0.f)
, m_fCurrentRung(0.f)
, m_fClimbRate(1.f)
, m_bCanGetOffAtTop(false)				
, m_bCalledStartLadderSlide(false)		
, m_bGetOnBackwards(false)			
, m_bGetOnBehind(false)
, m_bGetOnBehindLeft(false)
, m_bFoundAbsoluteTop(false)
, m_bFoundAbsoluteBottom(false) 
, m_bNetworkBlendedIn(false) 
, m_bTestedValidity(false) 
, m_bLooped(false)
, m_bCloneAudioSlidingFlag(false)
, m_bIsWaterMounting(false)
, m_bIsWaterDismounting(false)
, m_bCanSlideDown(true)
, m_bIgnoreNextRungCalc(false)
, m_bOnlyBase(false)
, m_bPDMatInitialized(false)
, m_bDismountRightHand(false)
, m_ladderCheckRestart(false)
, m_bRungCalculated(false)
, m_bOwnerDismountedAtTop(false)
, m_bOwnerDismountedAtBottom(false)
, m_bPreventFPSMode(false)
, m_minHeightToBlock(0.0f)
, m_maxHeightToBlock(0.0f)
, m_topOfLadderBlockIndex(-1)
, m_bottomOfLadderBlockIndex(-1)
, m_vBlockPos(VEC3_ZERO)
, m_fBlockHeading(0.0f)
, m_ladderHash(0)
, m_fRandomIsMovementBlockedOffsetMP(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_CLIMB_LADDER);
}

CTaskClimbLadder::~CTaskClimbLadder()
{
	// make sure we've freed up the ladder...
	if(NetworkInterface::IsGameInProgress())
	{
		// force unlock both potential locks...
		UnblockLadderMP(true, true);
	}
}

void CTaskClimbLadder::UpdatePlayerInput(CPed* pPed)
{
	CControl* pControl = pPed->GetControlFromPlayer();
	if (pControl && pControl->GetPedSprintIsDown())
	{
		static dev_float GO_FASTER_RESET_TIME = 0.3f;
		m_fGoFasterTimer = GO_FASTER_RESET_TIME - fwTimer::GetTimeStep();
	}
	else
	{
		m_fGoFasterTimer -= fwTimer::GetTimeStep();
	}
}

void CTaskClimbLadder::CleanUp()
{
	CPed *pPed = GetPed();

	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsClimbingLadder, false );

	taskAssert(pPed->GetMotionData());
	pPed->SetUseExtractedZ(false);
	// If the ped ends up in the air, then this will be reset in due course
	pPed->SetIsStanding(true);
	pPed->SetWasStanding(false);
	pPed->SetIsNotBuoyant(false);

	pPed->GetMovePed().ClearTaskNetwork(m_NetworkHelper, REALLY_SLOW_BLEND_DURATION);

	// Enable collision
	pPed->EnableCollision();

	// make sure we've freed up the ladder...
	if(NetworkInterface::IsGameInProgress())
	{
		// force unlock both potential locks...
		UnblockLadderMP(true, true);
	}

	if(m_iDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_NONE && m_iDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD)
	{
		CPathServerGta::RemoveBlockingObject(m_iDynamicObjectIndex);
		m_iDynamicObjectIndex = DYNAMIC_OBJECT_INDEX_NONE;
	}
}


bool CTaskClimbLadder::IsAtTop( const CPed* pPed ) const
{
	return DistanceToTop(pPed) >= 0.f;
}

bool CTaskClimbLadder::IsAtBottom( const CPed* pPed ) const
{
	if (pPed->GetTransform().GetPosition().GetZf() <= GetBottomZ() + ms_fHeightBeforeBottomToFinishT)
		return true;

	return false;
}

float CTaskClimbLadder::DistanceToTop( const CPed* pPed ) const
{
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOfTopHeight, 1.1f, 0.0f, 1.5f, 0.05f);
	return (pPed->GetTransform().GetPosition().GetZf() + GetOfTopHeight) - m_vTop.z;
}

bool CTaskClimbLadder::IsMovementBlocked(const CPed* pPed, const Vector3& vMovementDirection, int iPreferredState, bool bIgnorePeds, bool bForceSPDistChecks, bool bCheckGeo, bool bOldLegacyTest, CEntity** ppHitEntity_Out, Vec3V_Ptr pvHitPosition_Out ) const
{
	// Calculate a point ahead of the ped in the direction they are moving

	// If you change these, make sure to change the fScale accordingly in StateSliding_OnUpdate() - RT
	TUNE_GROUP_FLOAT(LADDER_DEBUG, LOOK_AHEAD_DIST, 1.5f, 0.0f, 3.0f, 0.1f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, LOOK_AHEAD_DIST_MP, 2.5f, 0.0f, 3.0f, 0.1f);

	// Exclude the ped and our ladder from the checks
	const CEntity* pExceptions[2];
	pExceptions[0] = pPed;
	pExceptions[1] = m_pLadder;

	// Check for objects, vehicles and ped obstructions
	CEntity* pHitEntity = 0;
	int nFlags = ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE;
	if (!bIgnorePeds)
		nFlags |= ArchetypeFlags::GTA_PED_TYPE;
	if (bCheckGeo)
		nFlags |= ArchetypeFlags::GTA_MAP_TYPE_MOVER;

	bool bCapsuleTestHit;
	if (bOldLegacyTest)
	{
		//static dev_float LOOK_AHEAD_DIST = 0.5f;
		Matrix34 pedMat;
		pPed->GetMatrixCopy(pedMat);
		pedMat.d += vMovementDirection * ((NetworkInterface::IsGameInProgress() && !bForceSPDistChecks) ? LOOK_AHEAD_DIST_MP : LOOK_AHEAD_DIST);

		bCapsuleTestHit = (CPedGeometryAnalyser::TestPedCapsule(pPed, &pedMat, pExceptions, 2, EXCLUDE_ENTITY_OPTIONS_NONE, nFlags, TYPE_FLAGS_ALL, TYPE_FLAGS_NONE, &pHitEntity, NULL));
		
		// Set output params
		if (ppHitEntity_Out)   { *ppHitEntity_Out = pHitEntity; }
		if (pvHitPosition_Out) { *pvHitPosition_Out = pHitEntity ? pHitEntity->GetTransform().GetPosition() : Vec3V(V_ZERO); }
	}
	else 
	{
		Vector3 movementDirNorm = vMovementDirection;
		movementDirNorm.Normalize();

		float len		= ((NetworkInterface::IsGameInProgress() && !bForceSPDistChecks) ? LOOK_AHEAD_DIST_MP : LOOK_AHEAD_DIST);
		len +=			(pPed->GetCapsuleInfo()->GetBipedCapsuleInfo()->GetHeadHeight() - pPed->GetCapsuleInfo()->GetBipedCapsuleInfo()->GetRadius());

		if(NetworkInterface::IsGameInProgress() && !pPed->IsPlayer())
		{
			// stop the cops all climbing the ladder perfectly spaced apart...
			len += m_fRandomIsMovementBlockedOffsetMP;
		}

		Vector3 start	= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if (NetworkInterface::IsGameInProgress())
		{
			if (iPreferredState == State_ClimbingDown)
			{
				//use a position a little less than center mass so we can test from the ped and below - if the ped is currently intersection another ped on an ladder as can happen the start position might 
				//be causing problems.
				start.z -= 1.0f;
				len /= 2.f;
			}
			else if (iPreferredState == State_ClimbingUp)
			{
				len /= 2.f;
			}
		}

		Vector3 end		= start;
		end				+= vMovementDirection * len;
		float radius	= pPed->GetCurrentMainMoverCapsuleRadius();

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetIsDirected(false);
		capsuleDesc.SetDoInitialSphereCheck(false);
		capsuleDesc.SetCapsule(start, movementDirNorm * len, len, radius );
		capsuleDesc.SetIncludeFlags(nFlags);
		capsuleDesc.SetContext(WorldProbe::ENetwork);
		capsuleDesc.SetExcludeEntity(pPed);
		capsuleDesc.SetTypeFlags(TYPE_FLAGS_ALL);
		capsuleDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		capsuleDesc.SetExcludeEntities(pExceptions, 2, EXCLUDE_ENTITY_OPTIONS_NONE);

	//	grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(capsuleDesc.GetStart()), VECTOR3_TO_VEC3V(capsuleDesc.GetEnd()), capsuleDesc.GetRadius(), Color_red, false, 500);
		WorldProbe::CShapeTestFixedResults<> capsuleResult;
		capsuleDesc.SetResultsStructure(&capsuleResult);

		bCapsuleTestHit = (WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc));

		if(CPedGeometryAnalyser::TestIfCollisionTypeIsValid(pPed, nFlags, capsuleResult))
		{
			if (capsuleResult.GetResultsReady())
			{
				pHitEntity = CPhysics::GetEntityFromInst(capsuleResult[0].GetHitInst());

				// Set output params
				if (ppHitEntity_Out)   { *ppHitEntity_Out = pHitEntity; }
				if (pvHitPosition_Out) { *pvHitPosition_Out = capsuleResult[0].GetHitPositionV(); }
			}
		}
	}

	if (bCapsuleTestHit)
	{
		// So we check if another ped is blocking us, if so, if that one is already climbing 
		// in same direction as we want to we allow us to climb.
		if (!NetworkInterface::IsGameInProgress())
		{			
			if(pHitEntity && pHitEntity->GetIsTypePed() && (iPreferredState == State_ClimbingDown || iPreferredState == State_ClimbingUp))
			{
				const CPed* pOtherPed = (CPed*)pHitEntity;
				if (pOtherPed->IsPlayer())	// Don't climb up the players arse
					return true;

				bool bDistGoodForClimbUp = (pOtherPed->GetTransform().GetPosition().GetZf() - pPed->GetTransform().GetPosition().GetZf()) > 2.7f;
				const CTaskClimbLadder* pTask = pOtherPed->GetPedIntelligence()->GetTaskClimbLadder();
				if (pTask)
				{
					int iState = pTask->GetState();
					if (iState == iPreferredState)
						return false;

					static bank_float fStateTimeThreshold = 0.5f; // roughly what we need to wait for anims to not clip
					if (iState == State_DismountAtTop && iPreferredState == State_ClimbingUp && (pTask->GetTimeInState() > fStateTimeThreshold || bDistGoodForClimbUp))
						return false;
				}

				CTaskSynchronizedScene* pTaskSynchronizedScene = static_cast<CTaskSynchronizedScene*>(pOtherPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE));
				if (pTaskSynchronizedScene)
				{
					static bank_float fStateTimeThreshold = 0.7f; // roughly what we need to wait for anims to not clip but this depends...
					if (iPreferredState == State_ClimbingUp && (pTaskSynchronizedScene->GetTimeInState() > fStateTimeThreshold || bDistGoodForClimbUp))
						return false;
				}
			}
		}
		else
		{
			//Sometimes the hit entity can be a player that is spectating - but remains on the ladder - perpetually blocking the player.  If the hit entity isn't visible and is a spectating player - disregard it. lavalley
			if (pHitEntity && pHitEntity->GetIsTypePed() && !pHitEntity->GetIsVisible() && NetworkInterface::IsASpectatingPlayer(static_cast<CPed*>(pHitEntity)))
				return false;
		}
		
		return true;
	}

	return false;
}

// PURPOSE
// When getting on a ladder from the top, the ped will not be in the position range tested by the capsule test in 
// IsMovementBlocked so a ped climbing the ladder will think the way up is clear but it will shortly after 
// become blocked causing overlap. This performs an additional test to see if the way will be blocked in case the 
// capsule test fails.

bool CTaskClimbLadder::IsMovementUpBlockedByPedGettingOnTopOfLadderMP(void)
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	CPed* pPed = GetPed();

	if(!pPed || pPed->IsNetworkClone())
	{
		return false;
	}

	if(!m_pLadder)
	{
		return false;
	}
	
	// are we in the blocking zone 
	Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());	

	if(pedPos.z < m_maxHeightToBlock)
	{
		return false;
	}

	// check nearby peds...
	CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		CPed* pNearbyPed = static_cast<CPed*>(pEnt);
		if (pNearbyPed)
		{
			if( pNearbyPed != pPed )
			{
				Vector3 nearbyPedPos = VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition());	

				if((pedPos - nearbyPedPos).XYMag2() < (5.0f * 5.0f))
				{
					CTaskClimbLadder* ladderTask = static_cast<CTaskClimbLadder*>(pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER));
					if(ladderTask)
					{
						// Getting on the ladder and same part of ladder?
						if( (ladderTask->GetLadder() == GetLadder()) && (ladderTask->GetEffectIndex() == GetEffectIndex()) )
						{
							if((ladderTask->GetState() == State_MountingAtTop) || (ladderTask->GetState() == State_MountingAtTopRun))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool CTaskClimbLadder::IsControlledDown(CPed* pPed)
{
	if(pPed->IsNetworkClone())
	{
		return false;
	}

	CControl* pControl = NULL;
	if (pPed->IsLocalPlayer())
		pControl = pPed->GetControlFromPlayer();
	
//	if(IsPlayerUnderAIControl())
//	{
//		pControl = NULL;
//	}

	bool bLooped = false;
	if ((pControl && pControl->GetPedWalkUpDown().GetNorm() > 0.5f) || m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbDown || m_eAIClimbState==CTaskGoToAndClimbLadder::InputState_WantsToClimbDownFast)
	{
		float fSpeed;
		bool bIsFast = m_fGoFasterTimer > 0.0f || m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbDownFast;			
		if (bIsFast)
			fSpeed = 1.0f;
		else
			fSpeed = 0.0f;
		
		bLooped = true;
		m_NetworkHelper.SetFloat(ms_Speed, fSpeed);
		m_fSpeed = fSpeed;
	}

	m_bLooped = bLooped;
	return bLooped; 
}


bool CTaskClimbLadder::IsControlledUp(CPed* pPed, bool bIgnorePeds)
{
	if(pPed->IsNetworkClone())
	{
		return false;
	}

	CControl* pControl = NULL;
	if (pPed->IsLocalPlayer())
		pControl = pPed->GetControlFromPlayer();

//	if(IsPlayerUnderAIControl())
//	{
//		pControl = NULL;
//	}

	m_bLooped = false;
	bool bLooped = false;
	if ((pControl && pControl->GetPedWalkUpDown().GetNorm() < -0.5f) || m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUp || m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUpFast)
	{
		if (!IsMovementBlocked(pPed, ZAXIS, State_ClimbingUp, bIgnorePeds))
		{
			// Need to test if there is something blocking where we want to play dismount anim also
			Vector3 DestDir = ZAXIS * 1.3f - m_LadderNormal * 0.6f;	// Yes not normalized
			if (!IsAtTop(pPed) || !IsMovementBlocked(pPed, DestDir, State_ClimbingUp, bIgnorePeds, true, false, true))
			{
				float fSpeed;
				bool bIsFast = m_fGoFasterTimer > 0.0f || m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUpFast;
				if (bIsFast)
					fSpeed = 1.0f;
				else
					fSpeed = 0.0f;

				m_NetworkHelper.SetFloat(ms_Speed, fSpeed);
				m_fSpeed = fSpeed;
				bLooped = true;
			}
		}
	}
	
	m_bLooped = bLooped;
	return bLooped;
}

bool CTaskClimbLadder::ScanForNextLadderSection()
{
	if (!m_bFoundAbsoluteTop || !m_bFoundAbsoluteBottom)	
	{
		CEntityScannerIterator entityList = GetPed()->GetPedIntelligence()->GetNearbyObjects();
		for (CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
		{
			if (m_pLadder && pEnt)
			{	
				CBaseModelInfo * pModelInf = pEnt->GetBaseModelInfo();
				CBaseModelInfo * pCurrentModelInfo = m_pLadder->GetBaseModelInfo(); 

				if (pModelInf && pModelInf->GetIsLadder() && pCurrentModelInfo && pCurrentModelInfo->GetIsLadder() )
				{
					if (FindNext2DEffect(m_pLadder, pEnt))
						return true; 

				}
			}
		}
	
		int nLadders = CTaskGoToAndClimbLadder::ms_Ladders.GetCount();
		for (int i =0; i < nLadders; i++)
		{
			if (m_pLadder && CTaskGoToAndClimbLadder::ms_Ladders[i])
			{
				CBaseModelInfo * pCurrentModelInfo = m_pLadder->GetBaseModelInfo(); 
				
				if (pCurrentModelInfo && pCurrentModelInfo->GetIsLadder())
				{
					if (FindNext2DEffect(m_pLadder, CTaskGoToAndClimbLadder::ms_Ladders[i]))
						return true; 

				}
			}
		}
	}
	return false; 
}

bool CTaskClimbLadder::FindNext2DEffect(CEntity *CurrentLadder, CEntity* NextLadder)
{
	if (CurrentLadder && NextLadder)
	{
		if (NextLadder == CurrentLadder)
		{
			CBaseModelInfo* pBaseModel = NextLadder->GetBaseModelInfo();
		
			if (pBaseModel)
			{
				Assert(pBaseModel->GetIsLadder());
				const s32 iNumEffects = pBaseModel->GetNum2dEffectsNoLights();
				if (iNumEffects <= 1)
					return false; 

			}
			return false; 
		}
		
		Vector3 vTarget= VEC3_ZERO; 
		bool bCanGetOffAtTop; 

		if (!m_bFoundAbsoluteTop)
		{
			if (GetNextLadderSection(CurrentLadder, NextLadder, GetTopPositionOfNextSection, vTarget, bCanGetOffAtTop))
			{
				m_vTop = vTarget; 
				if (bCanGetOffAtTop)
					m_bFoundAbsoluteTop = true; 

				return true; 
			}
		}

		if (!m_bFoundAbsoluteBottom)
		{
			if (GetNextLadderSection(CurrentLadder, NextLadder, GetBottomPostionOfNextSection, vTarget, bCanGetOffAtTop))
			{
				m_vBottom = vTarget; 
				return true; 
			}
		}
	}
	return false; 
}

bool CTaskClimbLadder::GetNextLadderSection(CEntity *CurrentLadder, CEntity* NextLadder, SearchState iSearchState, Vector3& vNextSectionPos, bool &bCanGetOff  )
{
	const float RungSpacing = 0.3f; //will get this from the 2d effects once the data is in

	if (CurrentLadder && NextLadder)
	{
		// Go through the effects one by one
		CBaseModelInfo* pBaseModel = NextLadder->GetBaseModelInfo();
		taskAssertf(pBaseModel, "GetNextLadderSection: ladder has no base model info for the next ladder section was expecing one"); 

		if (!pBaseModel)
			return false; 
		
		Assert(pBaseModel->GetIsLadder());

		if (!pBaseModel->GetIsLadder())
			return false; 

		const s32 iNumEffects = pBaseModel->GetNum2dEffectsNoLights();
		
		//The current ladder section
		Vector3 CurrentTop, CurrentBottom, CurrentNormal; 
		bool bCurrentCanGetOffTop; 

		if (CTaskGoToAndClimbLadder::CalcLadderInfo(*CurrentLadder, m_EffectIndex, CurrentBottom, CurrentTop, CurrentNormal, bCurrentCanGetOffTop))
		{
			for (s32 i=0; i<iNumEffects; i++)
			{
				Vector3 vNextBottom, vNextTop, vNextNormal;
				Vector3 vTarget, vCurrent; 
				vNextSectionPos = VEC3_ZERO; 
				
				if (CTaskGoToAndClimbLadder::CalcLadderInfo(*NextLadder, i, vNextBottom, vNextTop, vNextNormal, bCanGetOff))
				{
					switch(iSearchState)
					{
					case GetBottomPostionOfNextSection:
						{
							vTarget = vNextTop; 
							vCurrent = CurrentBottom; 
							vNextSectionPos = vNextBottom; 
						}
					break;
					case GetTopPositionOfNextSection:
						{
							vTarget = vNextBottom;
							vCurrent = CurrentTop; 
							vNextSectionPos = vNextTop; 
						}
					break; 
					}
					
					//calculate the distance between the current 2d effect and the next one
					Vector3 vNext2dEffect; 
					vNext2dEffect = vCurrent - vTarget; 
					float DistToNext2dEffect = vNext2dEffect.Mag2(); 

					//Check the angle between the vector to the next node and angle of the next ladder.
					//Ideally should check the angle of the 2 ladder sections and the angle with respect to the next node
					Vector3 vLadderTarget = vNextTop - vNextBottom; 
					vLadderTarget.Normalize(); 
					vNext2dEffect.NormalizeSafe(Vector3(0.0f, 0.0f, 1.0f)); 
					float fLadderDotToTarget = vLadderTarget.Dot(vNext2dEffect); 
					
					//check that the vector to the new node is close to the angle of the ladder
					if ((DistToNext2dEffect < RungSpacing*RungSpacing) && (Abs(fLadderDotToTarget) > 0.98f))
					{
						m_pLadder = NextLadder; 
						m_EffectIndex = i; 
						return true; 
					}
				}
			}
		}
	}
	return false; 
}

bool CTaskClimbLadder::CheckForLadderSlide(CPed* pPed)
{
	if (!m_bCanSlideDown)
		return false;

	Assert(pPed);
	Assert(!pPed->IsNetworkClone());
	taskAssert(GetState() == State_ClimbingIdle || GetState() == State_ClimbingUp || GetState() == State_ClimbingDown);

	CControl* pControl = NULL;
	if (pPed->IsAPlayerPed())
		pControl = pPed->GetControlFromPlayer();

	if ((pControl && m_fGoFasterTimer > 0.0f && pControl->GetPedWalkUpDown().GetNorm() > 0.5f) || m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToLetGo || m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToSlideDown)
	{
		m_bCalledStartLadderSlide = true;
		return true;
	}
	return false;
}

bool CTaskClimbLadder::CheckForWaterDismount(CPed* pPed, float &fWaterLevel) const
{
	bool bIsWaterDismounting = false;

	// This one also check against rivers but is more expensive I guess
	Vec3V StartPos = Vec3V(m_vTop + m_LadderNormal * 0.5f);
	Vec3V EndPos;// = Vec3V(m_vBottom + m_LadderNormal * 0.5f);
	//Vec3V OutPos;// = Vec3V(VEC3_ZERO);

	//Check for water beneath the ped.
	float fWaterHeight = 0.0f;
	//if (CVfxHelper::TestLineAgainstWater(StartPos, EndPos, OutPos))
	if(CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(VEC3V_TO_VECTOR3(StartPos), &fWaterHeight, true, 999999.9f)) 
	{
		if(m_vBottom.z < fWaterHeight + 3.0f)
		{
			bIsWaterDismounting = true;

			const u32 nIncludeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER	| 
				ArchetypeFlags::GTA_GLASS_TYPE		| 
				ArchetypeFlags::GTA_VEHICLE_TYPE	| 
				ArchetypeFlags::GTA_OBJECT_TYPE;

			const CEntity* exclusionList[2];
			int nExclusions = 0;
			exclusionList[nExclusions++] = pPed;
			
			StartPos.SetZf(Max(m_vBottom.z, fWaterHeight));
			EndPos = StartPos;
			EndPos.SetZf(fWaterHeight - 2.f);

			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
			capsuleDesc.SetResultsStructure(&shapeTestResults);
			capsuleDesc.SetCapsule(VEC3V_TO_VECTOR3(StartPos), VEC3V_TO_VECTOR3(EndPos), 0.25f);
			capsuleDesc.SetIsDirected(true);
			capsuleDesc.SetDoInitialSphereCheck(true);
			capsuleDesc.SetIncludeFlags(nIncludeFlags);
			capsuleDesc.SetExcludeEntities(exclusionList, nExclusions);
			if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
			{
				float fMaxDepthZ = fWaterHeight - 2.f;
				if(shapeTestResults[0].GetHitPosition().z >= fMaxDepthZ) 
				{
					bIsWaterDismounting = false;
				}
				else if(shapeTestResults[0].GetHitEntity() && shapeTestResults[0].GetHitEntity()->GetIsPhysical())
				{
					CPhysical *pPhysical = static_cast<CPhysical*>(shapeTestResults[0].GetHitEntity());
					if(pPhysical->GetVelocity().z > 0.0f)
					{
						bIsWaterDismounting = false;
					}
				}
			}

			fWaterLevel = fWaterHeight;
		}
	}

	return bIsWaterDismounting;
}

CTask::FSM_Return CTaskClimbLadder::ProcessPreFSM()
{
	CPed *pPed = GetPed();
	taskAssertf(pPed, "No ped in CTaskClimbLadder::ProcessPreFSM()!");
	// Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

// Might need this - Traffe
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableCellphoneAnimations, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_TaskUseKinematicPhysics, true);

	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableGaitReduction, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true );

	TUNE_GROUP_FLOAT(LADDER_DEBUG, fPitchSpeedMul, 2.0f, 0.0f, 50.0f, 0.1f);
	const float fPitchDelta = /*pPed->GetDesiredPitch()*/0.f - pPed->GetCurrentPitch();
	pPed->GetMotionData()->SetExtraPitchChangeThisFrame(fPitchDelta * fwTimer::GetTimeStep() * fPitchSpeedMul);

	if (m_bOnlyBase)
	{
		fwClipSet* clipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
		if (clipSet && clipSet->Request_DEPRECATED())
		{
			m_NetworkHelper.SetClipSet(m_ClipSetId);
			m_NetworkHelper.SetFlag(false, ms_LightWeight);
			m_bOnlyBase = false;
		}
	}

	if( (GetState() != State_Initial) && (GetState() != State_Blocked) )
	{
		m_NetworkHelper.SetFloat(ms_InputRate, m_fClimbRate);
		m_NetworkHelper.SetFloat(ms_InputRateFast, m_fClimbRate * s_fClimbRateFast);
	}

/*#if FPS_MODE_SUPPORTED
	if( GetState() == State_DismountAtTop )
	{
		if(!m_bPreventFPSMode)
		{
			const camFirstPersonShooterCamera* pFpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
			if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pFpsCamera != NULL && pFpsCamera->ShouldGoIntoFallback() )
			{
				m_bPreventFPSMode = true;
				camInterface::GetGameplayDirector().DisableFirstPersonThisUpdate();
			}
		}
		else
		{
			camInterface::GetGameplayDirector().DisableFirstPersonThisUpdate();
		}
	}
#endif*/

	if (GetState() != State_ClimbingIdle)
	{
		// Don't lod whilst climbing ladder, it might cause peds to go nuts when it is used as a subtask
		pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}

	if (m_eAIClimbState != CTaskGoToAndClimbLadder::InputState_Nothing && !pPed->IsNetworkClone())
	{
		const bool bPlayerInControl = pPed->IsPlayer() && pPed->GetMotionData()->GetPlayerHadControlOfPedLastFrame(); /*&& (!NetworkInterface::IsGameInProgress() || !IsPlayerUnderAIControl());*/
		if (bPlayerInControl)
		{
			m_eAIClimbState = CTaskGoToAndClimbLadder::InputState_Nothing;
		}
	}

	if (!m_pLadder)
	{
		if((m_ladderHash != 0) && (m_EffectIndex != -1))
		{
			// Can't sync ladders as they aren't dynamic entities so we scan for the ladder here....
			// need to do this here as it's used to set things first time around on migration below 
			m_pLadder = CTaskGoToAndClimbLadder::FindLadderUsingPosHash(*pPed, m_ladderHash);
			
			if(m_pLadder)
			{
				Vector3 vNormal;
				if (m_pLadder && !CTaskGoToAndClimbLadder::FindTopAndBottomOfAllLadderSections(m_pLadder, m_EffectIndex, m_vTop, m_vBottom, vNormal, m_bCanGetOffAtTop))
					taskAssertf(false, "Failed FindTopAndBottomOfAllLadderSections()!");
			}
		}
		else if(!pPed->IsNetworkClone() && GetTimeInState() >= 2.0f)
		{
			if(pPed->GetPlayerInfo() && !pPed->GetPlayerInfo()->AreControlsDisabled())
			{
				//! We have lost our ladder and are now stuck. This is probably because our ladder was deleted whilst we were on it.
				taskAssertf(0, "A ladder has been deleted from ped %s, so we need to bail. Check logs for deletion callstack!", pPed->GetDebugName());
				return FSM_Quit;
			}
		}
	}

	// block the ladder to stop other players / peds getting on - taken care of elsewhere in SP code...
	if((NetworkInterface::IsGameInProgress()) && (m_pLadder))
	{
		if((GetState() != State_Initial) && (GetState() != State_Blocked))
		{
			// Stop other peds getting on / off if possible....
			ProcessLadderBlockingMP();

			// If two players are overlapping when getting on, make one quit.
			if(FSM_Continue != ProcessLadderCoordinationMP())
			{
				return FSM_Quit;
			}
		}
	}
	
	if (!pPed->IsNetworkClone() && pPed->IsLocalPlayer() &&
		(GetState() == State_ClimbingIdle || GetState() == State_ClimbingUp || GetState() == State_ClimbingDown))
	{
		Vector3 vToLadder = m_vPositionToEmbarkLadder - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vPosDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - pPed->GetPreviousPosition();
		if (vPosDir.XYMag2() > 0.01f &&				// We must have moved
			vToLadder.XYMag2() > 0.05f * 0.05f)		// And be far enough away from ladder
		{
			vPosDir.z = 0.f;
			vToLadder.z = 0.f;
			vPosDir.Normalize();
			vToLadder.Normalize();

			if (vPosDir.Dot(vToLadder) < 0.f)		// Moving away from ladder
			{
				taskWarningf("Quitting ladder task, ped not close enough to ladder at X: %.2f Y: %.2f Z: %.2f", 
					m_vPositionToEmbarkLadder.x, m_vPositionToEmbarkLadder.y, m_vPositionToEmbarkLadder.z);

				return FSM_Quit;
			}
		}
	}

	if (!pPed->IsNetworkClone() && pPed->GetUsingRagdoll())
		return FSM_Quit; 

	if (!pPed->IsNetworkClone() && (!m_bTestedValidity && !GetLadderInfo()))
		return FSM_Quit;

	ScanForNextLadderSection(); 

	if (pPed->IsAPlayerPed() && !pPed->IsNetworkClone())
		UpdatePlayerInput(pPed);

	// tell the ped we need to call ProcessPhysics for this task
	if (GetState() != State_DismountAtTop)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	}
	
	if (GetState() != State_DismountAtBottom && GetState() != State_SlideDismount )
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ApplyVelocityDirectly, true );
	}

	// if we're already climbing, we don't care if the move network isn't ready - we need to set this anyway to prevent
	// "standing idle" pose coming in mid air. But if we're not (we're getting on / off) we need to wait until the move network
	// is attached other wise we get bind pose pops....
	if (( m_NetworkHelper.IsNetworkAttached() && GetPed()->GetMovePed().IsTaskNetworkFullyBlended() )
		|| ( IsAlreadyClimbingOrSlidingFromMigration( GetPed() ) ) )
	{
		if (GetState() != State_DismountAtBottom && GetState() != State_SlideDismount)
		{
			// We don't want our ped to think it is actually moving
			pPed->GetMotionData()->SetCurrentMoveBlendRatio(0.0f, 0.0f);
			pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
		}
	}

	if (GetState() != State_DismountAtBottom && GetState() != State_SlideDismount)
	{
		pPed->SetIsStanding(false);
		pPed->SetUseExtractedZ(true);

		if ((m_bIsWaterMounting || pPed->GetIsSwimming()) && (GetState() == State_Initial || GetState() == State_MountingAtBottom || GetState() == State_MountingAtBottomRun))
		{
			
		}
		else
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_IsClimbing, true );
		}
	}
	
	//Ensure the ped will always collide with explosions, even when collision is disabled.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceExplosionCollisions, true);

	//zero out to stop the ped spinning
	GetPed()->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f); 

	if (pPed->IsLocalPlayer())
	{
		if (GetState() == State_Finish)
		{
			if (m_iDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_NONE && m_iDynamicObjectIndex != DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD)
			{
				CPathServerGta::RemoveBlockingObject(m_iDynamicObjectIndex);
				m_iDynamicObjectIndex = DYNAMIC_OBJECT_INDEX_NONE;
			}
		}
		else if ((m_iDynamicObjectIndex == DYNAMIC_OBJECT_INDEX_NONE || m_iDynamicObjectIndex == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD))
		{
			if (m_LadderNormal.Mag2() > 0.01f)
			{
				const Vector3 vSize(1.25f, 2.f, 1.f);
				m_iDynamicObjectIndex = CPathServerGta::AddBlockingObject(m_vTop, vSize, m_fHeading, TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES);
			}
		}
	}

	// Because we don't kill the task off when it completes on the owner if we're in a dismounting state,
	// add a failsafe that will kill the task off if we've become stuck for whatever reason....
	if(pPed && pPed->IsNetworkClone())
	{
		if ((GetState() == State_DismountAtTop)	|| (GetState() == State_DismountAtBottom) || (GetState() == State_SlideDismount))
		{
			if(m_bOwnerTaskEnded)
			{
				TUNE_GROUP_FLOAT(TASK_LADDER, DismountSafetyQuitTime, 5.0f, 0.0f, 10.0f, 0.01f);
				if(GetTimeInState() > DismountSafetyQuitTime)
				{
					return FSM_Quit;
				}
			}
		}
	}

	// Reset entity blocking movement. I will be set again if we make a new test in this tick
	m_pEntityBlockingMovement = NULL;

	return FSM_Continue;
}

CLadderInfo* CTaskClimbLadder::GetLadderInfo()
{
	m_bTestedValidity = true;

	// Go through the effects one by one
	if (m_pLadder)
	{
		CBaseModelInfo* pBaseModel = m_pLadder->GetBaseModelInfo();

		taskAssertf(pBaseModel,"IsLadderValid:: Cannnot find a valid Base model info for the given ladder");

		if (pBaseModel)
		{
			C2dEffect* pEffect = pBaseModel->Get2dEffect(m_EffectIndex);
			
			taskAssertf(pEffect,"IsLadderValid:: This ladder has no 2d effects");

			if (pEffect && pEffect->GetType() == ET_LADDER)
			{
				CLadderInfo* pLadderInfo = pEffect->GetLadder();
				
				taskAssertf(pLadderInfo,"IsLadderValid:: The 2d effect is inval;id for this ladder");
				return pLadderInfo;
			}
		}
	}

	return NULL; 
}

Matrix34 CTaskClimbLadder::CalculateGetOnMatrix()
{	
	CLadderInfo* pLadderInfo = GetLadderInfo();
	if (pLadderInfo)
	{
		Vector3 vPosMount;
		Vector3 vLadderNormal;
		
		if (m_eStartState == State_MountingAtBottom || m_eStartState == State_MountingAtBottomRun)
			pLadderInfo->vBottom.Get(vPosMount);
		else
			pLadderInfo->vTop.Get(vPosMount);
	
		pLadderInfo->vNormal.Get(vLadderNormal);
		Matrix34 mat = MAT34V_TO_MATRIX34(m_pLadder->GetMatrix());

		// Transform positions into worldspace and render
		mat.Transform(vPosMount);

		// Calculate the ladder normal in world space
		mat.Transform3x3(vLadderNormal);
		mat.d = vPosMount;

		camFrame::ComputeWorldMatrixFromFront(vLadderNormal, mat); 
		return mat; 
	}

	return M34_IDENTITY; 
}

bool CTaskClimbLadder::ShouldAbort( const AbortPriority iPriority, const aiEvent * pEvent)
{
	CPed *pPed = GetPed();

	if (pEvent && iPriority != ABORT_PRIORITY_IMMEDIATE)
	{
		if(((CEvent*)pEvent)->GetEventPriority() < E_PRIORITY_DAMAGE && pPed->GetHealth() > 0.0f)
			return false;
	}

	// Don't abort for entering water - continue climbing/descending
	if (pEvent && ((CEvent*)pEvent)->GetEventType()==EVENT_IN_WATER)
		return false;

	if (GetSubTask())
		return GetSubTask()->MakeAbortable(iPriority, pEvent);

	return true;
}

bool CTaskClimbLadder::ProcessMoveSignals()
{
#if FPS_MODE_SUPPORTED
	switch( GetState() )
	{
		case State_DismountAtTop:
		{
			if( m_NetworkHelper.GetBoolean(ms_IsInterrupt) && m_uTimeWhenAnimWasInterruptible == 0 )
				m_uTimeWhenAnimWasInterruptible = fwTimer::GetTimeInMilliseconds();

			break;
		}
	}

	return true;
#else
	return false;
#endif
}

// Clone task implementation
void CTaskClimbLadder::OnCloneTaskNoLongerRunningOnOwner()
{
	// cache that the owner has ended....
	m_bOwnerTaskEnded = true;

	// if we're getting off the ladder anyway, let it finish....
	if ((GetState() != State_DismountAtTop)
		&& (GetState() != State_DismountAtBottom)
		&& (GetState() != State_SlideDismount))
	{
		SetStateFromNetwork(State_Finish);
	}
}

void CTaskClimbLadder::TaskSetState(const s32 iState)
{
    SetTaskState(iState);
}

bool CTaskClimbLadder::ControlPassingAllowed(CPed* pPed, const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{ 
	int state = GetState();

	// check we're not getting on / off....
	bool canPass = ( ! ((state == State_MountingAtTop)	|| 
			(state == State_MountingAtTopRun)	|| 
			(state == State_MountingAtBottom)	|| 
			(state == State_MountingAtBottomRun)||
			(state == State_DismountAtTop)		||
			(state == State_DismountAtBottom)	||
			(state == State_SlideDismount)) );

	// check we're not in a position where we might pick the wrong state 
	// i.e. a ped can be 'climbing down' but still "at top" of the ladder
	// so the code may select "getting off at top" incorrectly).
	// Other way to fix would be to test anim phase's but this is neater.
	canPass &= 	( ( !IsAtBottom( pPed ) ) && ( !IsAtTop( pPed ) ) );
	return canPass;
}

CTaskInfo* CTaskClimbLadder::CreateQueriableState() const
{
	Assert(GetPed());
	Assert((m_fSpeed == 0.0f) || (m_fSpeed == 1.0f));
	Assert(m_eAIClimbState >= CTaskGoToAndClimbLadder::InputState_Nothing);
	Assert(m_eAIClimbState <= CTaskGoToAndClimbLadder::InputState_Max);
	Assert(GetState() != -1);

	u32 hash = 0;
	if(m_pLadder)
	{
		Vector3 ladderPos = VEC3V_TO_VECTOR3(m_pLadder->GetTransform().GetPosition());
		hash = CTaskGoToAndClimbLadder::GenerateHash(ladderPos,m_pLadder->GetBaseModelInfo()->GetModelNameHash());
	}

	CClonedClimbLadderInfo::InitParams initParams
	(
		GetPed()->IsPlayer(),
		GetState(),
		hash,
		m_ClipSetId,
		(s16)m_EffectIndex,
		m_vPositionToEmbarkLadder,
		m_fHeading,
		(m_fSpeed > 0.0f),
		m_eStartState,
		m_LadderType,
		m_eAIClimbState,
		m_bGetOnBackwards,
		m_bCanSlideDown,
		m_bRungCalculated,
		m_bDismountRightHand,
		m_fRandomIsMovementBlockedOffsetMP
	);

	return rage_new CClonedClimbLadderInfo(initParams);
}

void CTaskClimbLadder::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_CLIMB_LADDER );
    CClonedClimbLadderInfo *info = static_cast<CClonedClimbLadderInfo*>(pTaskInfo);

	m_ladderHash						=	info->GetLadderPosHash();
	m_ClipSetId							=	info->GetClipSetId();
	m_EffectIndex						=	info->GetEffectIndex();
	m_vPositionToEmbarkLadder			=	info->GetStartPos();
	m_fHeading							=	info->GetHeading();
	m_fSpeed							=	info->GetSpeed();
	m_eStartState						=	info->GetStartState();
	m_LadderType						=	info->GetLadderType();
	m_bGetOnBackwards					=	info->GetOnLadderBackwards();
	m_eAIClimbState						=	info->GetInputState();
	m_bCanSlideDown						=	info->GetCanSlideDown();
	m_bRungCalculated					=	info->GetRungCalculated();
	m_bDismountRightHand				=	info->GetDismountRightHand();

	if(GetEntity())
	{
		Assert(GetPed()->IsPlayer() == info->GetIsPlayer());	

		if(!GetPed()->IsPlayer())
		{
			// not used by players...
			m_fRandomIsMovementBlockedOffsetMP	=	info->GetRandomMovementBlockedOffset();
		}
	}

	m_bOwnerDismountedAtTop				=	info->GetState() == State_DismountAtTop;
	m_bOwnerDismountedAtBottom			=	info->GetState() == State_DismountAtBottom;

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

bool CTaskClimbLadder::StreamReqResourcesIn(CLadderClipSetRequestHelper* pClipSetRequestHelper, const CPed* pPed, int iNextState)
{
	//enum
	//{
	//	NEXT_STATE_MOUNT = BIT0,
	//	NEXT_STATE_WATERMOUNT = BIT1,
	//	NEXT_STATE_CLIMBING = BIT2,
	//	NEXT_STATE_DISMOUNT = BIT3,
	//	NEXT_STATE_WATERDISMOUNT = BIT4,
	//};
	taskAssertf(pClipSetRequestHelper, "Ehh no pClipSetRequestHelper for CTaskClimbLadder::StreamReqResourcesIn()");

	if (iNextState & NEXT_STATE_WATERMOUNT)
		pClipSetRequestHelper->AddClipSetRequest(ms_ClipSetIdWaterMount);
	if (iNextState & NEXT_STATE_CLIMBING)
		pClipSetRequestHelper->AddClipSetRequest(ShouldUseFemaleClipSets(pPed) ? ms_ClipSetIdFemaleBase : ms_ClipSetIdBase);
	if (iNextState & NEXT_STATE_CLIMBING)
	{
		const u32 uRequestedClipSet = ShouldUseFemaleClipSets(pPed) ? CLadderMetadataManager::GetLadderFemaleClipSet() : CLadderMetadataManager::GetLadderClipSet();
		pClipSetRequestHelper->AddClipSetRequest(fwMvClipSetId(uRequestedClipSet));
	}

	return pClipSetRequestHelper->RequestAllClipSets();
}

bool CTaskClimbLadder::IsReqResourcesLoaded(const CPed* pPed, int iNextState)
{

	if (iNextState & NEXT_STATE_WATERMOUNT)
	{
		fwClipSet* clipSet = fwClipSetManager::GetClipSet(ms_ClipSetIdWaterMount);
		if (!clipSet || !clipSet->IsStreamedIn_DEPRECATED())
			return false;
	}
	if (iNextState & NEXT_STATE_CLIMBING)
	{
		fwClipSet* clipSet = fwClipSetManager::GetClipSet(ShouldUseFemaleClipSets(pPed) ? ms_ClipSetIdFemaleBase : ms_ClipSetIdBase);
		if (!clipSet || !clipSet->IsStreamedIn_DEPRECATED())
			return false;
	}
	if (iNextState & NEXT_STATE_HIGHLOD)
	{
		const u32 uRequestedClipSet = ShouldUseFemaleClipSets(pPed) ? CLadderMetadataManager::GetLadderFemaleClipSet() : CLadderMetadataManager::GetLadderClipSet();
		fwClipSet* clipSet = fwClipSetManager::GetClipSet(fwMvClipSetId(uRequestedClipSet));
		if (!clipSet || !clipSet->IsStreamedIn_DEPRECATED())
			return false;
	}

	return true;
}

bool CTaskClimbLadder::OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) 
{
	if((GetState() != State_Blocked) && (GetState() != State_Initial) && (GetState() != State_WaitForNetworkAfterInitialising))
	{
		return true; 
	}
	
	return false; 
}

CTask::FSM_Return CTaskClimbLadder::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
#if !__FINAL
	// PrintDebugInfo();
#endif /* _FINAL */

	CPed *pPed = GetPed();
	if (iEvent == OnUpdate)
	{
		s32 state			 = GetState();
		s32 stateFromNetwork = GetStateFromNetwork();

		if (state != stateFromNetwork)
		{
			// task may not have started on owner yet
			if (stateFromNetwork != -1)
			{
				// Owner can't be in this state?!
				Assert((stateFromNetwork != State_WaitForNetworkAfterMounting) && (stateFromNetwork != State_WaitForNetworkAfterInitialising));

				// If we've been told to quit, go for it...
				if (stateFromNetwork == State_Finish)
				{
					TaskSetState(State_Finish);
					return FSM_Continue;
				}
				else if(state == State_Blocked)
				{
					TaskSetState(stateFromNetwork);
					return FSM_Continue;
				}
				//we've been told to wait until the owner has decided to either go to blocking or mounting...
				else if (state == State_WaitForNetworkAfterInitialising)
				{
					if((stateFromNetwork == State_Blocked)						||
						(stateFromNetwork == State_MountingAtTop)				||
						(stateFromNetwork == State_MountingAtTopRun)			||
						(stateFromNetwork == State_MountingAtBottom)			||
						(stateFromNetwork == State_MountingAtBottomRun))
					{
						TaskSetState(stateFromNetwork);
						return FSM_Continue;
					}
					// if the ped comes into scope half way up a ladder....
					else if ((stateFromNetwork == State_ClimbingIdle)	||
						(stateFromNetwork == State_ClimbingUp)			||
						(stateFromNetwork == State_ClimbingDown)		||
						(stateFromNetwork == State_SlidingDown))		
					{
						TaskSetState(stateFromNetwork);
						return FSM_Continue;
					}
				}
				// otherwise, if we're ready to be told what to do...
				else if (state == State_WaitForNetworkAfterMounting) 
				{
					// and the network is telling us to start climbing or sliding off (i.e not mounting on / off)...
					if ((stateFromNetwork == State_ClimbingIdle)	||
						(stateFromNetwork == State_ClimbingUp)		||
						(stateFromNetwork == State_ClimbingDown)	||
						(stateFromNetwork == State_SlidingDown)		||
						(stateFromNetwork == State_DismountAtBottom)||
						(stateFromNetwork == State_DismountAtTop))
					{
						// Move us onto the main state that controls climbing for the clone (based on position of owner)....
						TaskSetState(State_ClimbingIdle);
						return FSM_Continue;
					}
				}
				// else if we're climbing already and we've been told to slide...
				else if ((state == State_ClimbingIdle)	||
						(state == State_ClimbingUp)		||
						(state == State_ClimbingDown))
				{
					// Don't wait for the owner to get 0.5m above us, if we're there and ready, got for it....
					if((stateFromNetwork == State_DismountAtTop) && (m_NetworkHelper.IsInTargetState()) && IsAtTop(pPed))
					{
						TaskSetState(State_DismountAtTop);
						return FSM_Continue;
					}

					if ((stateFromNetwork == State_SlidingDown) && (m_NetworkHelper.IsInTargetState()))
					{
						TaskSetState(State_SlidingDown);
						return FSM_Continue;
					}
				}
			}
		}
	}

	// Clones should just be frozen if there is no ladder and they are too far away
	if(!m_pLadder)
	{
		if(iState != State_Finish)
		{
			return FSM_Continue;
		}
	}
	else
	{
		// we've went past StateInitial::OnEnter without calling it while we've been looking for a ladder....
		if((GetState() == State_Initial) && (iEvent == OnUpdate))
		{
			// So we restart this state so it will get called....
			if(!m_ladderCheckRestart)
			{
				m_ladderCheckRestart = true;
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}
		}
	}

	if (m_LadderNormal.Mag2() <= 0.01f)	// Must fetch ladder normal or peds might be facing backwards
		CTaskGoToAndClimbLadder::GetNormalOfLadderFrom2dFx(m_pLadder, m_LadderNormal, m_EffectIndex );

	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed); 
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);	

		FSM_State(State_WaitForNetworkAfterInitialising)
			FSM_OnUpdate
				return FSM_Continue;

		FSM_State(State_MountingAtTop)
			FSM_OnEnter
				return StateMounting_OnEnter(pPed); 
			FSM_OnUpdate
				return StateMounting_OnUpdate(pPed);

		FSM_State(State_MountingAtTopRun)
			FSM_OnEnter
				return StateMounting_OnEnter(pPed); 
			FSM_OnUpdate
				return StateMounting_OnUpdate(pPed);

		FSM_State(State_MountingAtBottom)
			FSM_OnEnter
				return StateMounting_OnEnter(pPed); 
			FSM_OnUpdate
				return StateMounting_OnUpdate(pPed);

		FSM_State(State_MountingAtBottomRun)
			FSM_OnEnter
				return StateMounting_OnEnter(pPed); 
			FSM_OnUpdate
				return StateMounting_OnUpdate(pPed);

		FSM_State(State_WaitForNetworkAfterMounting)
			FSM_OnUpdate
				return FSM_Continue;

		FSM_State(State_ClimbingIdle)
			FSM_OnEnter
				return StateClimbingIdle_OnEnter(pPed); 
			FSM_OnUpdate
				return StateClimbingIdle_OnUpdate_Clone(pPed);	
				
		FSM_State(State_ClimbingUp)
			FSM_OnEnter
				return StateClimbingUp_OnEnter(pPed); 
			FSM_OnUpdate
				return StateClimbingUp_OnUpdate(pPed);	

		FSM_State(State_ClimbingDown)
			FSM_OnEnter
				return StateClimbingDown_OnEnter(pPed); 
			FSM_OnUpdate
				return StateClimbingDown_OnUpdate(pPed);	

		FSM_State(State_DismountAtTop)
			FSM_OnEnter
				return StateAtTopDismount_OnEnter();
			FSM_OnUpdate
				return StateAtTopDismount_OnUpdate(pPed);		

		FSM_State(State_DismountAtBottom)
			FSM_OnEnter
				return StateAtBottomDismount_OnEnter();
			FSM_OnUpdate
				return StateAtBottomDismount_OnUpdate(pPed);
	
		FSM_State(State_SlidingDown)
			FSM_OnEnter
				return StateSliding_OnEnter(pPed);
			FSM_OnUpdate
				return StateSliding_OnUpdate(pPed);

		FSM_State(State_SlideDismount)
			FSM_OnEnter
				return StateSlideDismount_OnEnter(pPed); 
			FSM_OnUpdate
				return StateSlideDismount_OnUpdate(pPed);

		FSM_State(State_Blocked)
			FSM_OnEnter
				return StateBlocked_OnEnter(pPed);
			FSM_OnUpdate
				return StateBlocked_OnUpdate(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTaskFSMClone* CTaskClimbLadder::CreateTaskForClonePed(CPed* UNUSED_PARAM( pPed ) )
{
	CTaskClimbLadder *newTask = rage_new CTaskClimbLadder(m_pLadder, m_EffectIndex, m_vTop, m_vBottom, m_vPositionToEmbarkLadder, m_fHeading, m_bCanGetOffAtTop, m_eStartState, m_bGetOnBackwards, m_LadderType, m_eAIClimbState);
	Assert(GetState() != State_WaitForNetworkAfterMounting);

	return newTask;
}

CTaskFSMClone* CTaskClimbLadder::CreateTaskForLocalPed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskClimbLadder(m_pLadder, m_EffectIndex, m_vTop, m_vBottom, m_vPositionToEmbarkLadder, m_fHeading, m_bCanGetOffAtTop, m_eStartState, m_bGetOnBackwards, m_LadderType, m_eAIClimbState);
}

CTask::FSM_Return CTaskClimbLadder::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
#if !__FINAL
	//Debug();
	// PrintDebugInfo();
#endif /* _FINAL */

	if (!m_pLadder)
		return FSM_Continue;

	CPed *pPed = GetPed();

	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed); 
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);	

		FSM_State(State_MountingAtTop)
			FSM_OnEnter
				return StateMounting_OnEnter(pPed); 
			FSM_OnUpdate
				return StateMounting_OnUpdate(pPed);

		FSM_State(State_MountingAtTopRun)
			FSM_OnEnter
				return StateMounting_OnEnter(pPed); 
			FSM_OnUpdate
				return StateMounting_OnUpdate(pPed);

		FSM_State(State_MountingAtBottom)
			FSM_OnEnter
				return StateMounting_OnEnter(pPed); 
			FSM_OnUpdate
				return StateMounting_OnUpdate(pPed);

		FSM_State(State_MountingAtBottomRun)
			FSM_OnEnter
				return StateMounting_OnEnter(pPed); 
			FSM_OnUpdate
				return StateMounting_OnUpdate(pPed);

		FSM_State(State_ClimbingIdle)
			FSM_OnEnter
				return StateClimbingIdle_OnEnter(pPed); 
			FSM_OnUpdate
				return StateClimbingIdle_OnUpdate(pPed);	
	
		FSM_State(State_ClimbingUp)
			FSM_OnEnter
				return StateClimbingUp_OnEnter(pPed); 
			FSM_OnUpdate
				return StateClimbingUp_OnUpdate(pPed);	

		FSM_State(State_ClimbingDown)
			FSM_OnEnter
				return StateClimbingDown_OnEnter(pPed); 
			FSM_OnUpdate
				return StateClimbingDown_OnUpdate(pPed);	

		FSM_State(State_DismountAtTop)
			FSM_OnEnter
				return StateAtTopDismount_OnEnter();
			FSM_OnUpdate
				return StateAtTopDismount_OnUpdate(pPed);
		
		FSM_State(State_DismountAtBottom)
			FSM_OnEnter
				return StateAtBottomDismount_OnEnter();
			FSM_OnUpdate	
				return StateAtBottomDismount_OnUpdate(pPed); 
	
		FSM_State(State_SlidingDown)
			FSM_OnEnter
				return StateSliding_OnEnter(pPed);
			FSM_OnUpdate
				return StateSliding_OnUpdate(pPed);

		FSM_State(State_SlideDismount)
			FSM_OnEnter
				return StateSlideDismount_OnEnter(pPed); 
			FSM_OnUpdate
				return StateSlideDismount_OnUpdate(pPed); 

		FSM_State(State_Blocked)
			FSM_OnEnter
				return StateBlocked_OnEnter(pPed);
			FSM_OnUpdate
				return StateBlocked_OnUpdate(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskClimbLadder::ProcessPostFSM()
{
	// I don't know what I am doing yet or I've been deliberately told to wait...
	if((GetState() != State_Initial) && (GetState() != State_Blocked))
	{
		m_NetworkHelper.SetFloat(ms_InputRate, m_fClimbRate);
		m_NetworkHelper.SetFloat(ms_InputRateFast, m_fClimbRate * s_fClimbRateFast);
	}
// 	if(m_NetworkHelper.IsNetworkAttached() && GetPed()->GetMovePed().IsTaskNetworkFullyBlended())
// 	{
// 		GetPed()->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);
// 		GetPed()->StopAllMotion(true);
// 	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateInitial_OnEnter(CPed* pPed)
{
	// True while CTaskClimbLadder is running
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsClimbingLadder, true );

	// Check if we need to assign female clipset
	if (ShouldUseFemaleClipSets(pPed))
		m_ClipSetId = m_FemaleClipSetId;

	Assert(m_ClipSetId.IsNotNull());

	if (!pPed->GetIsSwimming())
		pPed->SetIsNotBuoyant(true);

	pPed->GetMotionData()->SetDesiredPitch(0.f);

	// request the move network
	m_NetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskClimbLadder);
	
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
	if (pClipSet)
		pClipSet->Request_DEPRECATED();

	m_bIsWaterDismounting	= CheckForWaterDismount(pPed, m_WaterLevel);

	m_fClimbRate			= GetBaseClimbRate();

	if(NetworkInterface::IsGameInProgress() && !pPed->IsPlayer() && !pPed->IsNetworkClone())
	{
		// stop the NPCs climbing the ladder perfectly spaced apart.
		m_fRandomIsMovementBlockedOffsetMP = fwRandom::GetRandomNumberInRange(-0.5f, 0.5f);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateInitial_OnUpdate(CPed* pPed)
{
	// We must start this task by either starting to climb up or starting to climb down
	taskAssert(m_eStartState == State_MountingAtBottom || m_eStartState == State_MountingAtBottomRun || m_eStartState == State_MountingAtTop || m_eStartState == State_MountingAtTopRun);
	
	const fwMvClipSetId clipSetIdBase = ShouldUseFemaleClipSets(pPed) ? ms_ClipSetIdFemaleBase : ms_ClipSetIdBase;
	fwClipSet* clipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
	fwClipSet* clipSetBase = fwClipSetManager::GetClipSet(clipSetIdBase);
	taskAssert(clipSet); 
	taskAssert(clipSetBase);

	if (pPed->GetIsSwimming())
	{
		fwClipSet* clipSetWater = fwClipSetManager::GetClipSet(ms_ClipSetIdWaterMount);
		taskAssert(clipSetWater);
		if (clipSetWater)
			clipSetWater->Request_DEPRECATED();
	}

	if (clipSetBase && clipSetBase->Request_DEPRECATED() && 
		m_NetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskClimbLadder))
	{
		m_NetworkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskClimbLadder);
		m_NetworkHelper.SetClipSet(clipSetIdBase, fwMvClipSetVarId("Base",0x44E21C90));
		if (clipSet && clipSet->Request_DEPRECATED())
			m_NetworkHelper.SetClipSet(m_ClipSetId);
		else
			m_bOnlyBase = true;

		m_NetworkHelper.SetFlag(m_bOnlyBase, ms_LightWeight);
		m_NetworkHelper.SetFlag(m_eStartState == State_MountingAtBottomRun || m_eStartState == State_MountingAtTopRun, ms_MountRun);

		// Attach it to an empty precedence slot in fwMove
		Assert(GetPed()->GetAnimDirector());
		CMovePed& move = GetPed()->GetMovePed();

		TUNE_GROUP_FLOAT(LADDER_DEBUG, BlendDuration, 0.25f, 0.0f, 1.0f, 0.1f);
		TUNE_GROUP_FLOAT(LADDER_DEBUG, BlendDurationBehind, 0.15f, 0.0f, 1.0f, 0.1f);
		TUNE_GROUP_FLOAT(LADDER_DEBUG, BlendDurationWater, 0.5f, 0.0f, 1.0f, 0.1f);
		TUNE_GROUP_FLOAT(LADDER_DEBUG, AngleLimitBehind, 0.175f, 0.0f, 1.0f, 0.1f);

		if (m_eStartState == State_MountingAtBottom)
		{
			const float fPosBehind = CalculateApproachFromBehind();
			if (fPosBehind > AngleLimitBehind && fPosBehind < 1.0f - AngleLimitBehind)
			{
				m_bGetOnBehind = true;

				if (fPosBehind < 0.5f)
					m_bGetOnBehindLeft = true;

				Vector3 vLadderLeft(m_LadderNormal.y, -m_LadderNormal.x, 0.f);

				const float fSideOffset = 0.65f;
				const float fCapsuleMultiplier = 1.2f;

				Matrix34 pedMat;
				pPed->GetMatrixCopy(pedMat);
				pedMat.d = Vector3(m_vBottom.x, m_vBottom.y, pedMat.d.z) + vLadderLeft * ((fPosBehind < 0.5f) ? -fSideOffset : fSideOffset) + ZAXIS * 0.25f;

				const CEntity* pExceptions[2];
				pExceptions[0] = pPed;
				pExceptions[1] = m_pLadder;

				pPed->SetPedResetFlag(CPED_RESET_FLAG_IsClimbing, false);

				int nFlags = ArchetypeFlags::GTA_ALL_TYPES_MOVER;
				if (CPedGeometryAnalyser::TestPedCapsule(pPed, &pedMat, pExceptions, 2, EXCLUDE_ENTITY_OPTIONS_NONE, nFlags, TYPE_FLAGS_ALL, TYPE_FLAGS_NONE, NULL, NULL, fCapsuleMultiplier))
				{
					m_bGetOnBehindLeft = !m_bGetOnBehindLeft;

					pedMat.d = Vector3(m_vBottom.x, m_vBottom.y, pedMat.d.z) + vLadderLeft * ((fPosBehind >= 0.5f) ? -fSideOffset : fSideOffset);
					if (CPedGeometryAnalyser::TestPedCapsule(pPed, &pedMat, pExceptions, 2, EXCLUDE_ENTITY_OPTIONS_NONE, nFlags, TYPE_FLAGS_ALL, TYPE_FLAGS_NONE, NULL, NULL, fCapsuleMultiplier))
					{
						m_bGetOnBehind = false;
					}
				}

				pPed->SetPedResetFlag(CPED_RESET_FLAG_IsClimbing, true);
				move.SetTaskNetwork(m_NetworkHelper, BlendDurationBehind);
			}
		}

		if (m_bGetOnBehind)
			move.SetTaskNetwork(m_NetworkHelper, BlendDurationBehind);
		else if(pPed->GetIsSwimming())
			move.SetTaskNetwork(m_NetworkHelper, BlendDurationWater);
		else
			move.SetTaskNetwork(m_NetworkHelper, BlendDuration);

		weaponAssert(pPed->GetWeaponManager());
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if (!pWeapon || !pWeapon->GetIsCooking()) //don't destroy cooking projectiles
			pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
		// disable stuck check when climbing
		pPed->GetPedIntelligence()->GetEventScanner()->GetStuckChecker().SkipStuckCheck();
	
		ClimbingState targetState = State_Finish;

		// am i migrating and already climbing?
		if (IsAlreadyClimbingOrSlidingFromMigration(pPed))
		{
			m_bFoundAbsoluteTop		= false; 
			m_bFoundAbsoluteBottom	= false; 

			ChooseMigrationState(pPed);
			return FSM_Continue;
		}
		else if (m_eStartState == State_MountingAtBottom)
		{
			targetState = State_MountingAtBottom;
			m_bFoundAbsoluteTop = false; 
			m_bFoundAbsoluteBottom = true; 
		}
		else if (m_eStartState == State_MountingAtBottomRun)
		{
			targetState = State_MountingAtBottomRun;
			m_bFoundAbsoluteTop = false; 
			m_bFoundAbsoluteBottom = true; 
		}
		else if(m_eStartState == State_MountingAtTop)
		{
			m_NetworkHelper.SetFlag(!m_bGetOnBackwards,ms_MountForwards);  
			targetState = State_MountingAtTop;

			m_bFoundAbsoluteTop = true; 
			m_bFoundAbsoluteBottom = false; 
		}
		else if(m_eStartState == State_MountingAtTopRun)
		{
			targetState = State_MountingAtTopRun;

			m_bFoundAbsoluteTop = true; 
			m_bFoundAbsoluteBottom = false; 
		}
		else
		{
			Assert(false);
		}

		if(NetworkInterface::IsGameInProgress())
		{
			if(!pPed->IsNetworkClone())
			{
				bool gettingOnAtTop = ((m_eStartState == State_MountingAtTop) || (m_eStartState == State_MountingAtTopRun));
				if(!gettingOnAtTop)
				{
					Assert((m_eStartState == State_MountingAtBottom) || (m_eStartState == State_MountingAtBottomRun));
				}

				if(gettingOnAtTop)
				{
					if(CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP(m_pLadder, m_EffectIndex, CTaskClimbLadderFully::BF_CLIMBING_DOWN))
					{
						targetState = State_Blocked;
					}
				}
			}
			else
			{
				targetState = State_WaitForNetworkAfterInitialising;
			}
		}

		TaskSetState(targetState);

		// Disable ped collision, so we don't get unwanted reactions to the geometry
		//pPed->DisableCollision();
	}
	else if ( pPed->IsPlayer() && GetTimeInState() > 1.f ) /*&& (!NetworkInterface::IsGameInProgress() || !IsPlayerUnderAIControl() ) )*/
	{
		CControl* pControl = pPed->GetControlFromPlayer();
		if (pControl && pControl->GetPedWalkUpDown().GetNorm() > 0.2f)
		{
			// So we break out if we are pushing the stick backwards abit if we are stuck streaming
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

void CTaskClimbLadder::UpdateRungInfo(float fFromZToTop)
{
	float TargetRung = fFromZToTop;
	int RungsToTop = int(((m_vTop.z - TargetRung) / m_RungSpacing) + 0.1f);

	RungsToTop -= (RungsToTop & 1);

	// This indicate which hand we should end with at the top
	// We cannot know this if several ladders are connected so we haveto also check this during climb
	// and will do that once we split the climb anim
	if (RungsToTop % 4)
	{
		m_bDismountRightHand = true;
		m_NetworkHelper.SetFlag(true, ms_DismountRightHand);
	}
	else
	{
		m_bDismountRightHand = false;
		m_NetworkHelper.SetFlag(false, ms_DismountRightHand);
	}
}

float CTaskClimbLadder::CalculateIdealEmbarkZ(bool bGetonTop)
{
	// Set this so the clone knows it can jump on to the same rung....
	m_bRungCalculated = true;

	if (bGetonTop)
	{
		m_bDismountRightHand = false;
		m_NetworkHelper.SetFlag(false, ms_DismountRightHand);
		return m_vTop.z - ((GetState() == State_MountingAtTopRun) ? 1.f : 1.f);
	}

	TUNE_GROUP_FLOAT(LADDER_DEBUG, LowAnimHeight, 0.5f, 0.0f, 3.0f, 0.1f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, HighAnimHeightRun, 1.0f, 0.0f, 3.0f, 0.1f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, AnimHeightBehindLeftExtra, 0.5f, 0.0f, 3.0f, 0.1f);

	const float PedOffsetToGround = GetPed()->GetCapsuleInfo()->GetGroundToRootOffset();

	Vector3 vGround = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()); 
	vGround.z -= PedOffsetToGround;

	float DistanceToRung = m_vBottom.z - vGround.z;
	float ExtraHeight = m_bGetOnBehindLeft ? AnimHeightBehindLeftExtra : 0.f;	// Bit hackish but the left anim climb an extra step

	float TestAnimHeight = LowAnimHeight;
	if (GetState() == State_MountingAtBottomRun)
		TestAnimHeight = HighAnimHeightRun;
	else
		ExtraHeight += LowAnimHeight;

	// We must find a rung higher than animation height
	while (DistanceToRung < TestAnimHeight)
	{
		DistanceToRung += m_RungSpacing;	// Target next rung instead
		ExtraHeight += m_RungSpacing;
	}

	// We basicly just want the first suitable rung to target that 
	float TargetRung = m_vBottom.z + ExtraHeight + TestAnimHeight;

	// Clones need to get onto the same rung as the owners so we just use whatever value has been synced...
	if(GetPed()->IsNetworkClone())
	{
		TargetRung = m_vPositionToEmbarkLadder.z;
	}

	// Do we have an uneven amount of rungs to the top? If so we must target next rung or we will mismatch in the last animation
	// It is better to target the next rather than the previous, otherwise we might clip through the ground
	int RungsToTop = int(((m_vTop.z - TargetRung) / m_RungSpacing) + 0.1f);
	
	// Clones need to get onto the same rung as the owners so we just use whatever value has been synced...
	if(!GetPed()->IsNetworkClone())
	{
		TargetRung += m_RungSpacing * float(RungsToTop & 1);
	}

	RungsToTop -= (RungsToTop & 1);

	// This indicate which hand we should end with at the top
	// We cannot know this if several ladders are connected so we haveto also check this during climb
	// and will do that once we split the climb anim
	if (RungsToTop % 4)
	{
		m_bDismountRightHand = true;
		m_NetworkHelper.SetFlag(true, ms_DismountRightHand);
	}
	else
	{
		m_bDismountRightHand = false;
		m_NetworkHelper.SetFlag(false, ms_DismountRightHand);
	}


	float TopMostRung = Min(TargetRung, m_vTop.z - m_RungSpacing * 4.f);
	if (fabs(TopMostRung - TargetRung) > 0.01f)		// In case it is a short ladder
	{
		TargetRung = TopMostRung;
		m_bDismountRightHand = false;
		m_NetworkHelper.SetFlag(false, ms_DismountRightHand);
	}

	return TargetRung;
}

float CTaskClimbLadder::CalculateApproachAngleBlend(bool bGettingOnAtTop)
{
	if (bGettingOnAtTop)
		return 1.f;

	Vector3 vPed = m_vPositionToEmbarkLadder - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()); 
	if (GetState() == State_MountingAtBottomRun)
		vPed += GetPed()->GetVelocity() * 0.125f; // Half BlendDur

	//Vector3 vPedForward = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetForward()); 
	vPed.z = 0.f;
	vPed.Normalize(); 
	Vector3 LadderNormal = -m_LadderNormal;

	float Angle = LadderNormal.AngleZ(vPed); 
	
	//now scale the input between 0 and 1
	Angle = (Angle + HALF_PI) / PI; 

	return Angle;
}

float CTaskClimbLadder::CalculateApproachFromBehind()
{
	Vector3 vPed = m_vPositionToEmbarkLadder - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()); 
	vPed.z = 0.f;
	vPed.Normalize(); 

	float fAngle = m_LadderNormal.AngleZ(vPed);
	fAngle = (fAngle + HALF_PI) / PI;		// Scale the input between 0 and 1

	return fAngle;
}

float CTaskClimbLadder::GetActivePhase()
{
	float phase = 0.0f; 

	switch (GetState())
	{
	case State_MountingAtBottom:
	case State_MountingAtBottomRun:
		phase = Clamp(m_NetworkHelper.GetFloat(ms_PhaseGetOnBottom), 0.0f, 1.0f);
		break; 

	case State_MountingAtTop:
	case State_MountingAtTopRun:
		phase = Clamp(m_NetworkHelper.GetFloat(ms_PhaseGetOnTop), 0.0f, 1.0f);
		break; 

	default:
		phase = Clamp(m_NetworkHelper.GetFloat(ms_Phase), 0.0f, 1.0f);
	}

	return phase; 
}

float CTaskClimbLadder::GetActivePhaseLeft()
{
	float phase = 0.0f; 

	switch (GetState())
	{
	case State_MountingAtBottom:
	case State_MountingAtBottomRun:
		phase = Clamp(m_NetworkHelper.GetFloat(ms_PhaseLeft), 0.0f, 1.0f);
		break; 
	}

	return phase; 
}


float CTaskClimbLadder::GetActivePhaseRight()
{
	float phase = 0.0f; 

	switch (GetState())
	{
	case State_MountingAtBottom:
	case State_MountingAtBottomRun:
		phase = Clamp(m_NetworkHelper.GetFloat(ms_PhaseRight), 0.0f, 1.0f);
		break; 
	}

	return phase; 
}

////////////////////////////////////////////////////////////////////////////////

CTaskClimbLadder::AnimInfo CTaskClimbLadder::GetAnimInfo(const fwMvFloatId FloatId, const fwMvClipId clip )
{
	AnimInfo Info; 
	
	//always blend on a front anim so just decide if its from the left or right 
	float blend = Clamp(m_NetworkHelper.GetFloat(FloatId), 0.0f, 1.0f);
	if (blend != 0.0f)
	{
		Info.Blend = blend; 
		Info.pClip = m_NetworkHelper.GetClip(clip);
	}
	return Info; 
}


Vector3 CTaskClimbLadder::GetAnimTranslationForBlend(const crClip* pClip, float StartPhase, float EndPhase, float Blend, const Matrix34& Mat)
{
	Vector3 vAnimTotalDist(Vector3::ZeroType);
	if (pClip)
		vAnimTotalDist = fwAnimHelpers::GetMoverTrackTranslationDiffRotated(*pClip, StartPhase, EndPhase, true); 

	Mat.Transform3x3(vAnimTotalDist);
	return vAnimTotalDist * Blend; 
}

float CTaskClimbLadder::GetAnimOrientationForBlend(const crClip* pClip, float StartPhase, float EndPhase, float Blend)
{
	Quaternion qOrient = fwAnimHelpers::GetMoverTrackRotationDelta(*pClip, StartPhase, EndPhase); 
	
	float Angle = qOrient.TwistAngle(ZAXIS);
	return Angle * Blend;
}

float CTaskClimbLadder::CalculateTotalAnimRotation(float StartPhase, float EndPhase, bool bUseLeftRight)
{
	float Heading = 0.0f; 
	
	switch(GetState())
	{
	case State_MountingAtBottom:
	case State_MountingAtBottomRun:
		{
			AnimInfo GetOnBottomFrontHigh = GetAnimInfo(ms_WeightGetOnBottomFrontHigh, ms_ClipGetOnBottomFrontHigh);  
			AnimInfo GetOnBottomLeftHigh = GetAnimInfo(ms_WeightGetOnBottomLeftHigh, ms_ClipGetOnBottomLeftHigh);  
			AnimInfo GetOnBottomRightHigh = GetAnimInfo(ms_WeightGetOnBottomRightHigh, ms_ClipGetOnBottomRightHigh); 

			if (GetOnBottomFrontHigh.pClip)
				Heading += GetAnimOrientationForBlend(GetOnBottomFrontHigh.pClip, StartPhase, EndPhase, GetOnBottomFrontHigh.Blend); 
			
			if (bUseLeftRight)
				EndPhase = GetActivePhaseLeft();

			if (GetOnBottomLeftHigh.pClip)
				Heading += GetAnimOrientationForBlend(GetOnBottomLeftHigh.pClip, StartPhase, EndPhase, GetOnBottomLeftHigh.Blend);
			
			if (bUseLeftRight)
				EndPhase = GetActivePhaseRight();

			if (GetOnBottomRightHigh.pClip)
				Heading += GetAnimOrientationForBlend(GetOnBottomRightHigh.pClip, StartPhase, EndPhase, GetOnBottomRightHigh.Blend);

		}
		break; 

	case State_MountingAtTop:
	case State_MountingAtTopRun:
		{
			AnimInfo GetOnTopFront = GetAnimInfo(ms_WeightGetOnTopFront, ms_ClipGetOnTopFront);   
			AnimInfo GetOnTopBack = GetAnimInfo(ms_WeightGetOnTopBack, ms_ClipGetOnTopBack); 
			AnimInfo GetOnTopLeft = GetAnimInfo(ms_WeightGetOnTopLeft, ms_ClipGetOnTopLeft); 
			AnimInfo GetOnTopRight = GetAnimInfo(ms_WeightGetOnTopRight, ms_ClipGetOnTopRight); 

			if (GetOnTopFront.pClip)
				Heading += GetAnimOrientationForBlend(GetOnTopFront.pClip, StartPhase, EndPhase, GetOnTopFront.Blend);

			if (GetOnTopBack.pClip)
				Heading += GetAnimOrientationForBlend(GetOnTopBack.pClip, StartPhase, EndPhase, GetOnTopBack.Blend);

			if (GetOnTopLeft.pClip)
				Heading += GetAnimOrientationForBlend(GetOnTopLeft.pClip, StartPhase, EndPhase, GetOnTopLeft.Blend);

			if (GetOnTopRight.pClip)
				Heading += GetAnimOrientationForBlend(GetOnTopRight.pClip, StartPhase, EndPhase, GetOnTopRight.Blend);

		}
		break;
	}

	return Heading; 
}

Vector3 CTaskClimbLadder::CalculateTotalAnimTranslation(float StartPhase, float EndPhase,  const Matrix34& mMat, bool bUseLeftRight)
{
	Vector3 vTotal = VEC3_ZERO; 

	switch(GetState())
	{
	case State_MountingAtBottom:
	case State_MountingAtBottomRun:
		{
			AnimInfo GetOnBottomFrontHigh = GetAnimInfo(ms_WeightGetOnBottomFrontHigh, ms_ClipGetOnBottomFrontHigh);  
			AnimInfo GetOnBottomLeftHigh = GetAnimInfo(ms_WeightGetOnBottomLeftHigh, ms_ClipGetOnBottomLeftHigh);  
			AnimInfo GetOnBottomRightHigh = GetAnimInfo(ms_WeightGetOnBottomRightHigh, ms_ClipGetOnBottomRightHigh); 

			Vector3 TotalAnimHighDist = VEC3_ZERO; 

			if (GetOnBottomFrontHigh.pClip)
				TotalAnimHighDist += GetAnimTranslationForBlend(GetOnBottomFrontHigh.pClip, StartPhase, EndPhase, GetOnBottomFrontHigh.Blend, mMat); 

			if (bUseLeftRight)
				EndPhase = GetActivePhaseLeft();

			if (GetOnBottomLeftHigh.pClip)
				TotalAnimHighDist += GetAnimTranslationForBlend(GetOnBottomLeftHigh.pClip, StartPhase, EndPhase, GetOnBottomLeftHigh.Blend, mMat);

			if (bUseLeftRight)
				EndPhase = GetActivePhaseRight();

			if (GetOnBottomRightHigh.pClip)
				TotalAnimHighDist += GetAnimTranslationForBlend(GetOnBottomRightHigh.pClip, StartPhase, EndPhase, GetOnBottomRightHigh.Blend, mMat);

			//calculate the total contribution from the animation 
			vTotal = TotalAnimHighDist;
		}
		break; 

	case State_MountingAtTop:
	case State_MountingAtTopRun:
		{
			AnimInfo GetOnTopFront = GetAnimInfo(ms_WeightGetOnTopFront, ms_ClipGetOnTopFront);  
			AnimInfo GetOnTopBack = GetAnimInfo(ms_WeightGetOnTopBack, ms_ClipGetOnTopBack); 
			AnimInfo GetOnTopLeft = GetAnimInfo(ms_WeightGetOnTopLeft, ms_ClipGetOnTopLeft); 
			AnimInfo GetOnTopRight = GetAnimInfo(ms_WeightGetOnTopRight, ms_ClipGetOnTopRight); 
			
			if (GetOnTopFront.pClip)
				vTotal += GetAnimTranslationForBlend(GetOnTopFront.pClip, StartPhase, EndPhase, GetOnTopFront.Blend, mMat);
			
			if (GetOnTopBack.pClip)
				vTotal += GetAnimTranslationForBlend(GetOnTopBack.pClip, StartPhase, EndPhase, GetOnTopBack.Blend, mMat);

			if (GetOnTopLeft.pClip)
				vTotal += GetAnimTranslationForBlend(GetOnTopLeft.pClip, StartPhase, EndPhase, GetOnTopLeft.Blend, mMat);

			if (GetOnTopRight.pClip)
				vTotal += GetAnimTranslationForBlend(GetOnTopRight.pClip, StartPhase, EndPhase, GetOnTopRight.Blend, mMat);

		}
		break; 
	}

	return vTotal; 
}

void CTaskClimbLadder::CalculatePerfectStart(float _StartPhase)
{
	const Matrix34& StartMat = MAT34V_TO_MATRIX34(GetPed()->GetTransform().GetMatrix());
	m_StartMat = StartMat;
	Vector3 EndPos = StartMat * CalculateTotalAnimTranslation(_StartPhase, 1.f, M34_IDENTITY, false);
	float Heading = CalculateTotalAnimRotation(_StartPhase, 1.f, false);

	Matrix34 DestMatInv = StartMat;
	DestMatInv.RotateZ(Heading);
	DestMatInv.d = EndPos;
//	grcDebugDraw::Axis(DestMatInv, 1.f, false, 500000);
	DestMatInv.Inverse();

	Matrix34 IdealEndMat = CalculateGetOnMatrix();
	IdealEndMat.RotateZ(PI);
	IdealEndMat.d = m_vPositionToEmbarkLadder;

	Matrix34 StartInEndSpace = StartMat;// * DestMatInv;
	StartInEndSpace.Dot(DestMatInv);

	m_PerfectStartMat = StartInEndSpace;// * IdealEndMat;
	m_PerfectStartMat.Dot(IdealEndMat);

	taskAssert(IsFiniteAll(MATRIX34_TO_MAT34V(m_PerfectStartMat)));

//	grcDebugDraw::Axis(m_StartMat, 1.f, true, 500000);
//	grcDebugDraw::Axis(m_PerfectStartMat, 1.f, true, 500000);
//	grcDebugDraw::Axis(IdealEndMat, 1.f, true, 500000);	
}

void CTaskClimbLadder::DriveTowardsPerfect(const Matrix34& _StartMat, const Matrix34& _PerfectStartMat, float _StartPhase, float _CurrentPhase, float _BlendDur, float fTimeStep)
{
	if (!m_bNetworkBlendedIn || _StartPhase == _CurrentPhase)	// Don't drive this frame!
		return;

	// Should be fully blended in after % of the anim
	// Might want to implement a time blend also, start and end phase sort of might not want to start to blend instantly
	const float FullyBlendedAt = (1.0f - _StartPhase) * _BlendDur;
	float LerpPhase = Clamp((_CurrentPhase - _StartPhase) / (FullyBlendedAt), 0.f, 1.f);

	Matrix34 LerpMat;
	LerpMat.Interpolate(_StartMat, _PerfectStartMat, LerpPhase);

	Vector3 TargetPos = LerpMat * CalculateTotalAnimTranslation(_StartPhase, _CurrentPhase, M34_IDENTITY, true);
	float TargetHeading = CalculateTotalAnimRotation(_StartPhase, _CurrentPhase, true);
	
	Matrix34 TargetMat = LerpMat;
	TargetMat.RotateZ(TargetHeading);
	TargetMat.d = TargetPos;

	//grcDebugDraw::Axis(TargetMat, 1.f, true, 50000);
	
	TargetHeading = rage::Atan2f(-TargetMat.b.x, TargetMat.b.y);	// .b is the forward vector

	CPed* pPed = GetPed();
	pPed->SetHeading(TargetHeading);
	pPed->SetDesiredHeading(TargetHeading);

	Vector3 OurPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 TargetVel = (((TargetMat.d - OurPos)) / fTimeStep);		// .d is the position

	// We don't want too much correction every frame, it should end up well anyway in the end
	TargetVel.ClampMag(0.f, DEFAULT_MAX_SPEED - 0.01f);

	pPed->SetVelocity(TargetVel);
	pPed->SetDesiredVelocity(TargetVel);
}

void CTaskClimbLadder::SteerTowardsPosition(const Vector3& _Position, float _Strength, float /*_TimeStep*/)
{
	CPed* pPed = GetPed();
	Vector3 OurPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 OurDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
	Vector3 ProjTargetPos = _Position;
	ProjTargetPos.z = OurPos.z;

	Vector3 TargetDir = ProjTargetPos - OurPos;
	TargetDir.Normalize();

	Vector3 LerpDir;
	LerpDir.Lerp(_Strength, OurDir, TargetDir);	// Slerp could improve this slightly

	pPed->SetHeading(rage::Atan2f(-LerpDir.x, LerpDir.y));
	pPed->SetDesiredHeading(pPed->GetCurrentHeading());
}

void CTaskClimbLadder::SteerTowardsDirection(const Vector3& _Direction, float _Strength, float /*_TimeStep*/)
{
	CPed* pPed = GetPed();
	Vector3 OurDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
	Vector3 TargetDir = _Direction;

	Vector3 LerpDir;
	LerpDir.Lerp(_Strength, OurDir, TargetDir);	// Slerp could improve this slightly

	pPed->SetHeading(rage::Atan2f(-LerpDir.x, LerpDir.y));
	pPed->SetDesiredHeading(pPed->GetCurrentHeading());
}

void CTaskClimbLadder::ScaleClimbToMatchRung(float fTimeStep)
{
	CPed* pPed = GetPed();
	const crClip* pClip = m_NetworkHelper.GetClip(ms_CurrentClip); 
	if (pClip && pPed)
	{	
		// Try to match position also
		Vector3 PedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 TargetXY = m_vPositionToEmbarkLadder;
		TargetXY.z = PedPos.z;
		TargetXY = TargetXY - PedPos;
		Vector3 DesiredVel = TargetXY / fTimeStep * 0.075f;

		const float ClimbAnimHeight = m_RungSpacing * 4.f * (GetState() == State_ClimbingDown ? -1.f : 1.f);
		float WantedPos = m_fCurrentRung + GetActivePhase() * ClimbAnimHeight;

		DesiredVel.z = (WantedPos - PedPos.z) / fTimeStep;
		DesiredVel.ClampMag(0.f, DEFAULT_IMPULSE_LIMIT - 1.f);
		pPed->SetDesiredVelocity(DesiredVel);

		float fDesiredHeadingChange =  SubtractAngleShorter(
			rage::Atan2f(m_LadderNormal.x, -m_LadderNormal.y), 
			fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetCurrentHeading())
			);

		Assertf(abs(fDesiredHeadingChange) < 0.6f || pPed->IsNetworkClone(), "Ped (0x%p) is climbing a ladder without facing it so well Normal: %.2f %.2f, CurrentHeading: %.2f, DesiredHeadingChange %.2f, PedPos: %.2f %.2f", 
			pPed, m_LadderNormal.x, m_LadderNormal.y, pPed->GetMotionData()->GetCurrentHeading(), fDesiredHeadingChange, PedPos.x, PedPos.y);

		float fAngVel = fDesiredHeadingChange / fTimeStep * 0.15f;
		pPed->SetDesiredAngularVelocity(Vector3(0.0f, 0.0f, fAngVel));
	}
}

float CTaskClimbLadder::CalcCurrentRung()
{
	CPed* pPed = GetPed();
	Vector3 OurPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	float CurrentRung = (float)(int)(((m_vTop.z - OurPos.z/* - m_vBottom.z*/) + m_RungSpacing * 0.5f) / m_RungSpacing);
	float CurrentRungHeight = (m_vTop.z - CurrentRung * m_RungSpacing);

	return CurrentRungHeight;
}

float CTaskClimbLadder::GetBaseClimbRate(void) const
{
	CPed const* pPed = GetPed();

	TUNE_GROUP_FLOAT(LADDER_DEBUG, ClimbMinStrSpeed, 1.0f, 1.0f, 3.0f, 0.1f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, ClimbMaxStrSpeed, 1.15f, 1.0f, 3.0f, 0.1f);

	float climbRate = ClimbMinStrSpeed;

	if(pPed && pPed->GetPedModelInfo())
	{
		float fStrengthValue = Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_STRENGTH.GetStatId())) / 100.0f, 0.0f, 1.0f);
		climbRate = ClimbMinStrSpeed + ((ClimbMaxStrSpeed - ClimbMinStrSpeed) * fStrengthValue);

		if (CAnimSpeedUps::ShouldUseMPAnimRates())
		{
			// B*1641078. TODO: UNDO this, probably should think about tuning patch support
			climbRate *= 1.333f; //CAnimSpeedUps::ms_Tunables.m_MultiplayerLadderRateModifier;
		}
	}

	return climbRate;
}

float CTaskClimbLadder::GetBottomZ() const
{
	if (m_bIsWaterDismounting)
		return m_WaterLevel;

	return m_vBottom.z;
}

bool CTaskClimbLadder::IsInterruptCondition()
{
	if (!m_NetworkHelper.GetBoolean(ms_IsInterrupt))
		return false;

	CPed* pPed = GetPed();
	if (!pPed->IsPlayer())
	{
		if (GetState() == State_DismountAtTop)
			return m_NetworkHelper.GetBoolean(ms_IsInterruptNPC);
		
		return true;
	}

	CControl* pControl = pPed->GetControlFromPlayer();
	if (!pControl)
		return true;

	//Displayf("Trying to interrupt!");
	if (pControl->GetPedEnter().IsPressed())
		return true;

	WeaponControllerState eWeaponState = CWeaponControllerManager::GetController(CWeaponController::WCT_Player)->GetState(pPed, false);
	s32 eFSMState = GetState();
	if ( (eWeaponState != WCS_None) || 
		 (abs(pControl->GetPedWalkUpDown().GetNorm()) > 0.5f) || 
		 (eFSMState == State_DismountAtBottom || eFSMState == State_SlideDismount || eFSMState == State_DismountAtTop) )
	{
#if FPS_MODE_SUPPORTED
		// B*1993769 Apparently breaking out early when in FPS mode causes the capsule to collide
		// and reposition the ped a little bit back which sometimes makes the ped fall down when
		// reaching the top of the ladder.
		// B*1965078 It also makes the ped get stuck when in tight spaces.
		// For now this hack should do.
		if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && GetState() == State_DismountAtTop )
		{
			const u32 uElapsedTime = fwTimer::GetTimeInMilliseconds() - m_uTimeWhenAnimWasInterruptible;
			if( uElapsedTime > sm_uTimeToAllowFPSToBreakOut )
				return true;
		}
		else
		{
#endif
			return true;
#if FPS_MODE_SUPPORTED
		}
#endif
	}

	//Displayf("No interrupt!");

	return false;
}

bool CTaskClimbLadder::ShouldDismountInsteadOfSlide()
{
	TUNE_GROUP_FLOAT(LADDER_DEBUG, MinSlideDismountHeight, 2.5f, 0.0f, 10.0f, 0.1f);

	CPed* pPed = GetPed();
	if (pPed->GetTransform().GetPosition().GetZf() <= (GetBottomZ() + MinSlideDismountHeight))
		return true;
	
	return false;
}

void CTaskClimbLadder::SetBlendsForMounting(bool bGettingOnTop)
{
	float fAngleBlend = CalculateApproachAngleBlend(bGettingOnTop); 
	m_NetworkHelper.SetFloat( ms_MountAngle, fAngleBlend); 
	m_AngleBlend = fAngleBlend;
}

CTask::FSM_Return CTaskClimbLadder::StateMounting_OnEnter(CPed* pPed)
{
	switch (GetState())
	{
	case State_MountingAtTop:
		m_NetworkHelper.SetFlag(true, ms_SettleDownward);
		m_NetworkHelper.SendRequest(ms_MountTop);
		m_NetworkHelper.WaitForTargetState(ms_OnEnterMountTop);
		m_bCanInterruptMount = true;
		break;

	case State_MountingAtTopRun:
		{
			m_NetworkHelper.SetFlag(true, ms_SettleDownward);
			m_NetworkHelper.SendRequest(ms_MountTop);
			m_NetworkHelper.WaitForTargetState(ms_OnEnterMountTop);

			TUNE_GROUP_FLOAT(LADDER_DEBUG, MountTopRunStartPhase, 0.0f, 0.0f, 1.0f, 0.01f);
			m_NetworkHelper.SetFloat(ms_MountTopRunStartPhase, MountTopRunStartPhase);
			m_bCanInterruptMount = true;
		}
		break;

	case State_MountingAtBottom:
		m_bIsWaterMounting = pPed->GetIsSwimming();
		if (pPed->GetIsSwimming())
		{
			fwClipSet* clipSetWater = fwClipSetManager::GetClipSet(ms_ClipSetIdWaterMount);
			if (clipSetWater && clipSetWater->Request_DEPRECATED())
			{
				m_NetworkHelper.SetClipSet(ms_ClipSetIdWaterMount, fwMvClipSetVarId("WaterMount",0x3C7D5869));
				m_NetworkHelper.SetFlag(true, ms_MountWater);
			}
		}
		else if (m_bGetOnBehind)
		{
			m_NetworkHelper.SetFlag(true, ms_MountBottomBehind);

			if (m_bGetOnBehindLeft)
				m_NetworkHelper.SetFlag(true, ms_MountBottomBehindLeft);
			else
				m_NetworkHelper.SetFlag(true, ms_MountBottomBehindRight);

		}
		
		m_NetworkHelper.SetFlag(false, ms_SettleDownward);
		m_NetworkHelper.SendRequest(ms_MountBottom);
		m_NetworkHelper.WaitForTargetState(ms_OnEnterMountBottom);
		m_bCanInterruptMount = true;
		break;

	case State_MountingAtBottomRun:
		m_NetworkHelper.SetFlag(false, ms_SettleDownward);
		m_NetworkHelper.SendRequest(ms_MountBottom);
		m_NetworkHelper.WaitForTargetState(ms_OnEnterMountBottom);
		m_bCanInterruptMount = false;	// !!
		break;
	};

	pPed->GetFootstepHelper().SetClimbingLadder(true);
	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateMounting_OnUpdate(CPed* pPed)
{
	Assert(pPed);

	ladderDebugf1("");
#if !__FINAL
	Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	ladderDebugf1("Frame %d : Task %p : %s : Ped %p %s : Pos %f %f %f : iState %s : iStateFromNetwork %s",
	fwTimer::GetFrameCount(),
	this,
	__FUNCTION__,
	GetEntity(),
	BANK_ONLY(GetEntity() ? GetPed()->GetDebugName() :) "NO PED",
	pedPos.x, pedPos.y, pedPos.z,
	GetStaticStateName(GetState()),
	GetPed()->IsNetworkClone() && (GetStateFromNetwork() != -1) ? GetStaticStateName(GetStateFromNetwork()) : "None / -1");
#endif	// !__FINAL

	ladderDebugf1("m_NetworkHelper.IsNetworkAttached() = %d", m_NetworkHelper.IsNetworkAttached());
	ladderDebugf1("m_NetworkHelper.GetBoolean(ms_MountEnded) = %d", m_NetworkHelper.GetBoolean(ms_MountEnded));
	ladderDebugf1("m_NetworkHelper.GetBoolean(ms_EnableHeadIK) = %d", m_NetworkHelper.GetBoolean(ms_EnableHeadIK));
	ladderDebugf1("DistanceToTop() = %f", DistanceToTop(pPed));
	ladderDebugf1("GetActivePhase() = %f", GetActivePhase());
	ladderDebugf1("m_bCanInterruptMount = %d", m_bCanInterruptMount);
	ladderDebugf1("m_bPDMatInitialized = %d", m_bPDMatInitialized);
	ladderDebugf1("m_bIsWaterDismounting = %d", m_bIsWaterDismounting);
	ladderDebugf1("m_BlendDonePhase = %f", m_BlendDonePhase);
	ladderDebugf1("m_bNetworkBlendedIn = %d", m_bNetworkBlendedIn);
	ladderDebugf1("m_bRungCalculated = %d", m_bRungCalculated);
	ladderDebugf1("m_WaterLevel = %f", m_WaterLevel);
	ladderDebugf1("m_vPositionToEmbarkLadder = %f %f %f", m_vPositionToEmbarkLadder.x, m_vPositionToEmbarkLadder.y, m_vPositionToEmbarkLadder.z);
	ladderDebugf1("pPed->GetMovePed().IsTaskNetworkFullyBlended() = %d", pPed->GetMovePed().IsTaskNetworkFullyBlended());
	ladderDebugf1("IsInterruptCondition = %d", IsInterruptCondition());

	bool bMountingAtTop = (GetState() == State_MountingAtTop || GetState() == State_MountingAtTopRun);
	if (!m_bNetworkBlendedIn && !bMountingAtTop)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);	// To prevent that foot sinking through the ground during blend
	}

	// Exit diving task and reset ped capsule befoer we are done with the mount to prevent pops
	if (m_bIsWaterMounting && GetActivePhase() > 0.25f)
	{
		pPed->SetIsNotBuoyant(true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_IsClimbing, true );

		// If we are in water
		if (pPed->m_Buoyancy.GetStatus() != NOT_IN_WATER)
		{
			// We are not fully in water after all!
			if (pPed->GetTransform().GetPosition().GetZf() + 0.25f > m_WaterLevel)
				pPed->m_Buoyancy.SetStatusHighNDry();
		}

		pPed->GetMotionData()->SetPedBoundPitch(0.f);
		pPed->GetMotionData()->SetDesiredPedBoundPitch(0.f);
	}

	if (!m_NetworkHelper.IsInTargetState())
	{
		SetBlendsForMounting(bMountingAtTop); 
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true );

		return FSM_Continue;
	}
	
	if (!m_bNetworkBlendedIn)
	{
		if (m_NetworkHelper.IsNetworkAttached() && pPed->GetMovePed().IsTaskNetworkFullyBlended())
		{
			if(pPed && pPed->IsNetworkClone())
			{
				// the owner hasn't calculated which rung he's getting on at so we need to hang tight....
				if(!m_bRungCalculated)
				{
					return FSM_Continue;
				}
			}

			// Only disable collision when the move network is fully blended in, so we don't penetrate walls due to extra animated velocity
			pPed->DisableCollision();

			m_bNetworkBlendedIn = true;
			m_BlendDonePhase = GetActivePhase();
			
			// clones receive the position to embark / dismount right hand across the network...
			if(!pPed->IsNetworkClone())
			{
				m_vPositionToEmbarkLadder.z = CalculateIdealEmbarkZ(bMountingAtTop);
			}
			else
			{
				m_NetworkHelper.SetFlag(m_bDismountRightHand, ms_DismountRightHand);
			}

			CalculatePerfectStart(GetActivePhase());

			if (pPed->IsPlayer() && pPed->GetPlayerInfo())
				pPed->GetPlayerInfo()->SetPlayerDataSprintControlCounter(0.0f);

		}
		else
		{
			CalculatePerfectStart(GetActivePhase());	// To calculate the target angle to steer towards whilst blending, expensive but meh...
			m_bPDMatInitialized = true;
		}
	}

	// To get a smooth head transition between mount climb
	if (m_NetworkHelper.GetBoolean(ms_EnableHeadIK))
	{
		if (GetState() == State_MountingAtTop || GetState() == State_MountingAtTopRun)
		{
			TUNE_GROUP_FLOAT(LADDER_DEBUG, HeadIKZMountTop, -0.5f, -2.0f, 0.0f, 0.1f);
			Vector3 HeadOffset(0.f,1.f,HeadIKZMountTop);
			pPed->GetIkManager().LookAt(0, pPed, 50, BONETAG_HEAD, &HeadOffset, LF_WIDE_PITCH_LIMIT | LF_WIDEST_PITCH_LIMIT, 500, 750);
		}

		if (GetState() == State_MountingAtBottom || GetState() == State_MountingAtBottomRun)
		{
			TUNE_GROUP_FLOAT(LADDER_DEBUG, HeadIKZMountBottom, 0.7f, 0.0f, 2.0f, 0.1f);
			Vector3 HeadOffset(0.f,1.f,HeadIKZMountBottom);
			pPed->GetIkManager().LookAt(0, pPed, 50, BONETAG_HEAD, &HeadOffset, 0, 500, 750);	
		}
	}

	// Since the interrupts don't match up entirely we want to make sure the pet is somewhat in position before allowing it
	bool bIsInterruptCondition = IsInterruptCondition();
	bool bCanInterruptMount = m_bCanInterruptMount;
	if (GetState() == State_MountingAtBottomRun && bIsInterruptCondition && !m_bCanInterruptMount)
	{
		TUNE_GROUP_FLOAT(LADDER_DEBUG, MountBottomRunPosTolerance, 0.1f, 0.0f, 1.0f, 0.1f);
		TUNE_GROUP_FLOAT(LADDER_DEBUG, MountBottomRunDotTolerance, 1.5f, 0.0f, 180.0f, 0.1f);

		Vector3 PedForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
		Vector3 PedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if (PedPosition.Dist2(m_vPositionToEmbarkLadder) < rage::square(MountBottomRunPosTolerance) && 
			PedForward.Dot(-m_LadderNormal) > rage::Cosf(MountBottomRunDotTolerance * DtoR))
		{
			bCanInterruptMount = true;	
		}
	}

	if ((GetState() == State_MountingAtTop || GetState() == State_MountingAtTopRun) && m_bIsWaterDismounting)
	{
		Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if (vPedPos.z < m_WaterLevel - 0.5f)
		{
			TaskSetState(State_ClimbingIdle);
			return FSM_Continue;
		}
	}

	// Don't interrupt to dismount top as that cause blend glitches
	if (DistanceToTop(pPed) > -0.25f && bIsInterruptCondition && IsControlledUp(pPed) && (GetState() != State_MountingAtBottomRun && GetState() != State_MountingAtBottom))
		bIsInterruptCondition = false;

	if ((m_NetworkHelper.GetBoolean(ms_MountEnded) && GetActivePhase() >= 1.f) || (bCanInterruptMount && bIsInterruptCondition))
	{
		pPed->SetAngVelocity(VEC3_ZERO); 
		pPed->SetDesiredAngularVelocity(VEC3_ZERO); 
		pPed->EnableCollision();

		if (!pPed->IsNetworkClone())
		{
			if (IsControlledDown(pPed) && !IsMovementBlocked(pPed, -ZAXIS, State_ClimbingDown) && !IsAtBottom(pPed) && GetState() != State_MountingAtBottomRun)
				TaskSetState(State_ClimbingDown);
			else if (IsControlledUp(pPed) && !IsAtTop(pPed))
				TaskSetState(State_ClimbingUp);
			else
			{
				pPed->SetVelocity(VEC3_ZERO); 
				pPed->SetDesiredVelocity(VEC3_ZERO);
				TaskSetState(State_ClimbingIdle);
			}
		}
		else
		{
			pPed->SetVelocity(VEC3_ZERO); 
			pPed->SetDesiredVelocity(VEC3_ZERO);
			TaskSetState(State_WaitForNetworkAfterMounting);
		}
	}

	m_bCanInterruptMount = bCanInterruptMount;

	return FSM_Continue;
}

bool CTaskClimbLadder::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnTopBlendDur, 0.33f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnTopBackwardBlendDur, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnTopRunBlendDur, 0.25f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnBottomBlendDur, 0.3f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnBottomBlendDurMP, 0.5f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnBottomRunBlendDur, 0.3f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnBottomWaterBlendDur, 0.25f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnBottomBehindBlendDur, 0.15f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnBlendDirectionFixup, 0.2f, 0.0f, 1.0f, 0.01f);

	if (m_bNetworkBlendedIn)
	{
		switch(GetState())
		{
		case State_MountingAtTop:
			if (m_bGetOnBackwards)
				DriveTowardsPerfect(m_StartMat, m_PerfectStartMat, m_BlendDonePhase, GetActivePhase(), GetOnTopBackwardBlendDur, fTimeStep);
			else
				DriveTowardsPerfect(m_StartMat, m_PerfectStartMat, m_BlendDonePhase, GetActivePhase(), GetOnTopBlendDur, fTimeStep);

			break;

		case State_MountingAtTopRun:
			DriveTowardsPerfect(m_StartMat, m_PerfectStartMat, m_BlendDonePhase, GetActivePhase(), GetOnTopRunBlendDur, fTimeStep);
			break;

		case State_MountingAtBottom:
			{
				float fBlendDur;

				if (m_bGetOnBehind)
					fBlendDur = GetOnBottomBehindBlendDur;
				else if (m_bIsWaterMounting)
					fBlendDur = GetOnBottomWaterBlendDur;
				else if (NetworkInterface::IsGameInProgress())
					fBlendDur = GetOnBottomBlendDurMP;
				else
					fBlendDur = GetOnBottomBlendDur;

				DriveTowardsPerfect(m_StartMat, m_PerfectStartMat, m_BlendDonePhase, GetActivePhase(), fBlendDur, fTimeStep);
			}
			break;

		case State_MountingAtBottomRun:
			DriveTowardsPerfect(m_StartMat, m_PerfectStartMat, m_BlendDonePhase, GetActivePhase(), GetOnBottomRunBlendDur, fTimeStep);
			break;

		case State_ClimbingUp:
		case State_ClimbingDown:
			ScaleClimbToMatchRung(fTimeStep);
			break;
		}
	}
	else if (m_NetworkHelper.IsInTargetState() && m_bPDMatInitialized)	// If we are not blended in we try to steer the ped towards a good point to have the perfectblend less sliding sideways
	{
		switch(GetState())
		{
		case State_MountingAtTop:
		case State_MountingAtTopRun:
		case State_MountingAtBottom:
		case State_MountingAtBottomRun:

			SteerTowardsDirection(m_PerfectStartMat.b, GetOnBlendDirectionFixup * m_fClimbRate, fTimeStep);
			break;
		}
	}

	return true;
}

CTask::FSM_Return CTaskClimbLadder::StateClimbingIdle_OnEnter(CPed* pPed)
{
	if((IsAlreadyClimbingOrSlidingFromMigration(pPed)) || (GetPreviousState() == State_WaitForNetworkAfterInitialising))
	{
		m_NetworkHelper.ForceStateChange(ms_idleState);
	}
	else
	{
		m_NetworkHelper.SendRequest(ms_Idle);
	}

	m_NetworkHelper.WaitForTargetState(ms_OnEnterClimbIdle);

	//Vector3 vPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()); 
	//Displayf("Pos: %f, %f, %f, Head: %f", vPos.x, vPos.y, vPos.z, GetPed()->GetCurrentHeading()); 
	pPed->SetDesiredHeading(GetPed()->GetCurrentHeading()); 

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateClimbingIdle_OnUpdate(CPed* pPed)
{
	if (!m_NetworkHelper.IsInTargetState())
	{
		m_NetworkHelper.SetFloat(ms_Speed, m_fSpeed);
		m_NetworkHelper.SetBoolean(ms_IsLooped, m_bLooped); 

		// force the ped to do a (expensive) reset after this AI update to make sure it doesnt' bind pose....
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true );

		return FSM_Continue;
	}

	if(IsControlledDown(pPed))
	{
		// If we're at the bottom, finish
		if(IsAtBottom(pPed))
		{
			TaskSetState(State_DismountAtBottom);
			return FSM_Continue;
		}

		const float fZScale = (NetworkInterface::IsGameInProgress() ? 1.0f : 1.5f); // 

		CEntity* pHitEntity(NULL);
		Vec3V vHitPosition(V_ZERO);
		if(IsMovementBlocked(pPed, -ZAXIS * fZScale, State_ClimbingDown, false, false, false, false, &pHitEntity, &vHitPosition))
		{
			if(IsMovementBlocked(pPed, -ZAXIS * fZScale, State_ClimbingDown, true))	// Don't dismount due to other peds
			{
				TaskSetState(State_DismountAtBottom);
				return FSM_Continue;	
			}

			m_pEntityBlockingMovement = pHitEntity;
		}
		else
		{
			TaskSetState(State_ClimbingDown); 
		}
	}
	else if(IsControlledUp(pPed) && !IsMovementUpBlockedByPedGettingOnTopOfLadderMP())
	{	
		if (IsAtTop(pPed))
		{ 
			TaskSetState(State_DismountAtTop);
			return FSM_Continue;
		}

		TaskSetState(State_ClimbingUp); 
	}
	else if (IsAtTop(pPed) && IsControlledUp(pPed, true))
	{	
		TaskSetState(State_DismountAtTop);
		return FSM_Continue;
	}

	//check for slide
	if (CheckForLadderSlide(pPed))
	{
		if (!ShouldDismountInsteadOfSlide())
		{
			if(pPed && pPed->GetPedAudioEntity())
			{
				pPed->GetPedAudioEntity()->GetFootStepAudio().StartLadderSlide();
			}
			TaskSetState(State_SlidingDown);
			return FSM_Continue;
		}
			
		TaskSetState(State_ClimbingDown);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateClimbingIdle_OnUpdate_Clone(CPed* pPed)
{
	Assert(pPed->IsNetworkClone());

	m_NetworkHelper.SetFloat(ms_Speed, m_fSpeed);
	m_NetworkHelper.SetBoolean(ms_IsLooped, false);

	if (!m_NetworkHelper.IsInTargetState())
		return FSM_Continue;

    const Vector3 vLastPosFromNetwork = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
    const Vector3 vPedPosition        = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// I'm an NPC Clone...
	if( ! pPed->IsPlayer() ) /*|| ( IsPlayerUnderAIControl() ) )*/
	{
		// if I've migrated I might be below or above the new owner
		if((m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUp)			||
			(m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUpFast))
		{
			// if I want to climb up and I'm above the owner...
			if(vPedPosition.z > vLastPosFromNetwork.z)
			{
				// sit tight unil the owner gets above me...
				return FSM_Continue;
			}
		}
		else if((m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbDown)	||
			(m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbDownFast))
		{
			// if I want to climb down and I'm below the owner already...
			if(vPedPosition.z < vLastPosFromNetwork.z)
			{
				return FSM_Continue;
			}
		}
		else
		{
			// just return for now as my state is about to be changed to sliding down....
			return FSM_Continue;
		}
	}

	eOwnerPosStatus status = IsOwnerZPosHigherOrLower(pPed);

	// if the owner has climbing far enough above or below the clone, change to the appropriate state...
	if(status == Status_OwnerIsHigher)
	{
		if(!IsAtTop(pPed))
		{
			TaskSetState(State_ClimbingUp);
			return FSM_Continue;
		}
		else if (m_bOwnerDismountedAtTop)
		{
			TaskSetState(State_DismountAtTop);
			return FSM_Continue;
		}
	}
	else if(status == Status_OwnerIsLower)
	{
		if(!IsAtBottom(pPed))
		{
			TaskSetState(State_ClimbingDown);
			return FSM_Continue;
		}
		else if (m_bOwnerDismountedAtBottom)
		{
			TaskSetState(State_DismountAtBottom);
			return FSM_Continue;
		}
	}
	
	return FSM_Continue;
}

CTaskClimbLadder::eOwnerPosStatus CTaskClimbLadder::IsOwnerZPosHigherOrLower(CPed* pPed) const
{
	if(!pPed || !pPed->IsNetworkClone())
	{
		return Status_OwnerInvalid;
	}

    const Vector3 vLastPosFromNetwork = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
    const Vector3 vPedPosition        = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	const float fZDifference = Abs(vPedPosition.z - vLastPosFromNetwork.z);

	// how long do we wait before the clone starts climbing after the owner.
	// too high and the clone will wait too long before climbing 
	// too low and the clone will flip flop (cllimb up, then climb down, repeat)
	// if the owner gets stuck instead of sitting and waiting for the owner to resume.
	const float climbThreshold = 0.5f;

	if((fZDifference > climbThreshold) && (vLastPosFromNetwork.z > vPedPosition.z))
	{
		return Status_OwnerIsHigher;
	}
	else if((fZDifference > climbThreshold) && (vLastPosFromNetwork.z < vPedPosition.z))
	{
		return Status_OwnerIsLower;
	}

	return Status_OwnerIsWithinThreshold;
}

CTask::FSM_Return CTaskClimbLadder::StateClimbingUp_OnEnter(CPed* pPed)
{
	if(pPed && pPed->IsNetworkClone())
	{
		UpdateCloneClimbRate();
	}

	m_NetworkHelper.SetFlag(false, ms_SettleDownward);

	if ((IsAlreadyClimbingOrSlidingFromMigration(pPed)) || (GetPreviousState() == State_WaitForNetworkAfterInitialising))
		m_NetworkHelper.ForceStateChange(ms_climbUpState);
	else
		m_NetworkHelper.SendRequest(ms_ClimbUp);

	m_NetworkHelper.WaitForTargetState(ms_OnEnterClimbUp);

	if (!m_bIgnoreNextRungCalc)
		m_fCurrentRung = CalcCurrentRung();
	else
		m_fCurrentRung -= m_RungSpacing * 4.f;

	m_bIgnoreNextRungCalc = false;

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateClimbingUp_OnUpdate(CPed* pPed)
{
	if (!m_NetworkHelper.IsInTargetState())
	{
		m_NetworkHelper.SetFloat(ms_Speed, m_fSpeed);
		m_NetworkHelper.SetBoolean(ms_IsLooped, m_bLooped); 

		// force the ped to do a (expensive) reset after this AI update to make sure it doesnt' bind pose....
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true );

		return FSM_Continue;
	}
	
	// If we are in water
	if (m_bIsWaterMounting && pPed->m_Buoyancy.GetStatus() != NOT_IN_WATER)
	{
		// We are not fully in water after all!
		if (pPed->GetTransform().GetPosition().GetZf() + 0.25f > m_WaterLevel)
			pPed->m_Buoyancy.SetStatusHighNDry();
	}

	TUNE_GROUP_FLOAT(LADDER_DEBUG, HeadIKZClimbUp, 0.7f, 0.0f, 2.0f, 0.1f);
	Vector3 HeadOffset(0.f,1.f,HeadIKZClimbUp);
	pPed->GetIkManager().LookAt(0, pPed, 50, BONETAG_HEAD, &HeadOffset, 0, 500, 750);

	// If we're at the top and can get off, start the get off anim
	if (DistanceToTop(pPed) > -0.5f)
	{
		m_bFoundAbsoluteTop = false;	//See if there is another ladder above this one
		ScanForNextLadderSection();
		if( (DistanceToTop(pPed) >= 0.05f && m_bDismountRightHand) && (!pPed->IsNetworkClone() || m_bOwnerDismountedAtTop) )
		{
			TaskSetState(State_DismountAtTop);
			return FSM_Continue;
		}

		// Ugly safeup in case the art is not properly setup
		if( DistanceToTop(pPed) >= 0.15f && (!pPed->IsNetworkClone() || m_bOwnerDismountedAtTop) )
		{
			TaskSetState(State_DismountAtTop);
			return FSM_Continue;
		}
	}

//	if(IsPlayerUnderAIControl())
//	{
//		Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
//		if(pedPos.z > m_minHeightToBlock)
//		{
//			SwitchFromAIToPlayerControl();
//		}
//	}

	if( ( ( IsControlledUp(pPed) && !IsMovementUpBlockedByPedGettingOnTopOfLadderMP() ) || ( IsOwnerZPosHigherOrLower(pPed) == Status_OwnerIsHigher ) ) && DistanceToTop(pPed) < -0.25f && GetActivePhase() > 0.9f )
	{
		m_NetworkHelper.SetBoolean(ms_IsLooped, true); 
		m_NetworkHelper.SetFloat(ms_Speed, m_fSpeed);
	}
	else
	{
		m_NetworkHelper.SetBoolean(ms_IsLooped, false);
		m_NetworkHelper.SetFloat(ms_Speed, m_fSpeed);
	}

	if (m_NetworkHelper.GetBoolean(ms_LoopEnded))
	{
		TaskSetState(State_ClimbingIdle); 
		return FSM_Continue;
	}

	if (!pPed->IsNetworkClone())
	{
		if (!ShouldDismountInsteadOfSlide() && CheckForLadderSlide(pPed))
		{
			if(pPed && pPed->GetPedAudioEntity())
			{
				pPed->GetPedAudioEntity()->GetFootStepAudio().StartLadderSlide();
			}
			TaskSetState(State_SlidingDown);
			return FSM_Continue;
		}
	}

	TUNE_GROUP_FLOAT(LADDER_DEBUG, BreakIntoDownPhase, 0.50f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, BreakIntoDownPhaseTolerance, 0.005f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, BreakIntoDownPhaseOffset, 0.0f, -1.0f, 1.0f, 0.01f);
	if (abs(GetActivePhase() - BreakIntoDownPhase + BreakIntoDownPhaseTolerance * 0.5f) < BreakIntoDownPhaseTolerance && IsControlledDown(pPed))
	{
		// If we're at the bottom, finish
		if (IsAtBottom(pPed) || IsMovementBlocked(pPed, -ZAXIS, State_ClimbingDown))
			return FSM_Continue;

		m_NetworkHelper.SetFloat(ms_InputPhase, 1.f - GetActivePhase() + BreakIntoDownPhaseOffset);
		m_bIgnoreNextRungCalc = true;
		TaskSetState(State_ClimbingDown); 
		return FSM_Continue;
	}

	if (m_NetworkHelper.GetBoolean(ms_ChangeToFast))
	{
		float fCurrentRung = CalcCurrentRung();
		UpdateRungInfo(fCurrentRung);
		m_fCurrentRung = fCurrentRung;
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskClimbLadder::StateClimbingDown_OnEnter(CPed* pPed)
{
	if(pPed && pPed->IsNetworkClone())
	{
		UpdateCloneClimbRate();
	}

	m_NetworkHelper.SetFlag(true, ms_SettleDownward);

	if ((IsAlreadyClimbingOrSlidingFromMigration(pPed)) || (GetPreviousState() == State_WaitForNetworkAfterInitialising))
		m_NetworkHelper.ForceStateChange(ms_climbDownState);
	else
		m_NetworkHelper.SendRequest(ms_ClimbDown);

	m_NetworkHelper.WaitForTargetState(ms_OnEnterClimbDown);

	if (!m_bIgnoreNextRungCalc)
		m_fCurrentRung = CalcCurrentRung();
	else
		m_fCurrentRung += m_RungSpacing * 4.f;

	m_bIgnoreNextRungCalc = false;

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateClimbingDown_OnUpdate(CPed* pPed)
{
	if (!m_NetworkHelper.IsInTargetState())
	{
		m_NetworkHelper.SetFloat(ms_Speed, m_fSpeed);
		m_NetworkHelper.SetBoolean(ms_IsLooped, m_bLooped); 

		// force the ped to do a (expensive) reset after this AI update to make sure it doesnt' bind pose....
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true );

		return FSM_Continue;
	}
	
	TUNE_GROUP_FLOAT(LADDER_DEBUG, HeadIKZClimbDown, -0.5f, -2.0f, 0.0f, 0.1f);
	Vector3 HeadOffset(0.f,1.f,HeadIKZClimbDown);
	pPed->GetIkManager().LookAt(0, pPed, 50, BONETAG_HEAD, &HeadOffset, LF_WIDE_PITCH_LIMIT | LF_WIDEST_PITCH_LIMIT, 500, 750);

	bool bIsAtBottom = IsAtBottom(pPed);
	if (bIsAtBottom)
	{
		m_bFoundAbsoluteBottom = false;
		ScanForNextLadderSection();
		bIsAtBottom = IsAtBottom(pPed);
	}


	if ((pPed->GetVelocity().z < 0.0f - SMALL_FLOAT))
	{
		if( (bIsAtBottom || IsMovementBlocked(pPed, -ZAXIS, State_ClimbingDown, true)) && (!pPed->IsNetworkClone() || m_bOwnerDismountedAtBottom) )
		{
			TaskSetState(State_DismountAtBottom);
			return FSM_Continue;
		}
	}

	if( ( IsControlledDown(pPed) ) || (IsOwnerZPosHigherOrLower(pPed) == Status_OwnerIsLower) )
	{
		if (bIsAtBottom)
		{
			TaskSetState(State_DismountAtBottom);
			return FSM_Continue;
		}

		const float fZScale = (NetworkInterface::IsGameInProgress() ? 1.0f : 1.5f); // 

		CEntity* pHitEntity(NULL);
		Vec3V vHitPosition(V_ZERO);
		if (IsMovementBlocked(pPed, -ZAXIS * fZScale, State_ClimbingDown, false, false, false, false, &pHitEntity, &vHitPosition))
		{
			if (IsMovementBlocked(pPed, -ZAXIS * fZScale, State_ClimbingDown, true) && (!pPed->IsNetworkClone() || m_bOwnerDismountedAtBottom))
			{
				TaskSetState(State_DismountAtBottom);
				return FSM_Continue;
			}

			m_pEntityBlockingMovement = pHitEntity;

			m_NetworkHelper.SetBoolean(ms_IsLooped, false);
			m_NetworkHelper.SetFloat(ms_Speed, m_fSpeed);
		}
		else
		{
			m_NetworkHelper.SetBoolean(ms_IsLooped, true); 
			m_NetworkHelper.SetFloat(ms_Speed, m_fSpeed);
		}
	}
	else
	{
		m_NetworkHelper.SetBoolean(ms_IsLooped, false);
		m_NetworkHelper.SetFloat(ms_Speed, m_fSpeed);
	}

	if (m_NetworkHelper.GetBoolean(ms_LoopEnded))
	{
		TaskSetState(State_ClimbingIdle); 
		return FSM_Continue;
	}

	if (!pPed->IsNetworkClone())
	{
		if(!ShouldDismountInsteadOfSlide() && CheckForLadderSlide(pPed))
		{
			if(pPed && pPed->GetPedAudioEntity())
			{
				pPed->GetPedAudioEntity()->GetFootStepAudio().StartLadderSlide();
			}
			TaskSetState(State_SlidingDown);
			return FSM_Continue;
		}
	}

	TUNE_GROUP_FLOAT(LADDER_DEBUG, BreakIntoUpPhase, 0.50f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, BreakIntoUpPhaseTolerance, 0.005f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, BreakIntoUpPhaseOffset, 0.0f, -1.0f, 1.0f, 0.01f);
	if (abs(GetActivePhase() - BreakIntoUpPhase + BreakIntoUpPhaseTolerance * 0.5f) < BreakIntoUpPhaseTolerance && IsControlledUp(pPed))
	{
		// If we're at the bottom, finish
		if (IsAtTop(pPed) || IsMovementBlocked(pPed, ZAXIS, State_ClimbingUp))
			return FSM_Continue;

		m_NetworkHelper.SetFloat(ms_InputPhase, 1.f - GetActivePhase() + BreakIntoUpPhaseOffset);
		m_bIgnoreNextRungCalc = true;
		TaskSetState(State_ClimbingUp); 
		return FSM_Continue;
	}

	if (m_NetworkHelper.GetBoolean(ms_ChangeToFast))
		m_fCurrentRung = CalcCurrentRung();
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateSliding_OnEnter(CPed* pPed)
{
	if(( IsAlreadyClimbingOrSlidingFromMigration(pPed) ) || (GetPreviousState() == State_WaitForNetworkAfterInitialising))
		m_NetworkHelper.ForceStateChange(ms_slideState);
	else
		m_NetworkHelper.SendRequest(ms_StartSliding);

	m_NetworkHelper.WaitForTargetState(ms_OnEnterSlide);
	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateSliding_OnUpdate(CPed* pPed)
{
	if (!m_NetworkHelper.IsInTargetState())
		return FSM_Continue;
	
	pPed->SetIsNotBuoyant(false);

	// the clones won't call CheckForLadderSlide so we call the audio here...
	if (pPed->IsNetworkClone())
	{
		if (!m_bCloneAudioSlidingFlag)
		{
			pPed->GetPedAudioEntity()->GetFootStepAudio().StartLadderSlide();
			m_bCloneAudioSlidingFlag = true;
		}
	}

	static const float fSlidDownFinishZOffset = 0.0f;		// This offset is to prevent ped's feet going through ground
	static const float fLetGoNotAllowedDistFromBase = 2.0f;	// Prevent peds from letting go too close to the base, as it causes issues

	const float fPedZ = pPed->GetTransform().GetPosition().GetZf();

	TUNE_GROUP_FLOAT(LADDER_DEBUG, SlideDismountHeight, 1.6f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, SlideDismountHeightWater, 3.25f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(LADDER_DEBUG, SlideDismountHeightScaleSP, 1.333f, 0.0f, 10.0f, 0.1f);

	float SlideDismountHeightCheck = (m_bIsWaterDismounting ? SlideDismountHeightWater : SlideDismountHeight);

	const float fZScale = (NetworkInterface::IsGameInProgress() ? 0.8f : SlideDismountHeightScaleSP); // 
	if (IsMovementBlocked(pPed, -ZAXIS * fZScale, State_ClimbingDown, false, false, true))
	{
		TaskSetState(State_SlideDismount);
		return FSM_Continue;
	}

	if (fPedZ <= (GetBottomZ() + SlideDismountHeightCheck + fSlidDownFinishZOffset))
	{
		m_bFoundAbsoluteBottom = false;
		ScanForNextLadderSection();
		if (fPedZ <= (GetBottomZ() + SlideDismountHeightCheck + fSlidDownFinishZOffset))
		{
			TaskSetState(State_SlideDismount);
			return FSM_Continue;
		}
	}

	if (!pPed->IsNetworkClone())
	{
		// If we're already sliding down the ladder & press Y again, then let go & exit the task
		CControl* pControl = NULL;
		if (pPed->IsLocalPlayer())
			pControl = pPed->GetControlFromPlayer();

		if (((pControl && pControl->GetPedEnter().IsPressed()) || m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToLetGo) &&
			fPedZ > (GetBottomZ() + ms_fHeightBeforeBottomToFinishT + fLetGoNotAllowedDistFromBase))
		{
			// Apply a slight nudge to push the ped away from the ladder
			static const float fImpulseMag = 3.0f;
			const Vector3 vImpulse = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()) * -fImpulseMag;
			pPed->ApplyImpulse(vImpulse * pPed->GetMass(), Vector3(0.0f, 0.0f, 0.0f));

			// Make sure ped gets in-air event straight away
			CEventInAir eventInAir(pPed);
			pPed->GetPedIntelligence()->AddEvent(eventInAir);

			TaskSetState(State_Finish);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateSlideDismount_OnEnter(CPed* pPed)
{
	m_NetworkHelper.SetFlag(CheckForWaterDismount(pPed, m_WaterLevel), ms_DismountWater);
	m_NetworkHelper.SendRequest(ms_SlideDismount); 
	m_NetworkHelper.WaitForTargetState(ms_OnEnterSlideDismount);

	// Not pretty but if we keep the velocity downward from the slidedown animation
	// we would slam into the ground so god damn fast it hurts
	TUNE_GROUP_FLOAT(LADDER_DEBUG, fSlideDismountVelocityRestitution, 0.5f, 0.0f, 1.0f, 0.1f);

	pPed->SetVelocity(pPed->GetVelocity() * fSlideDismountVelocityRestitution);
	pPed->SetDesiredVelocity(pPed->GetVelocity() * fSlideDismountVelocityRestitution);

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateSlideDismount_OnUpdate(CPed* pPed)
{
	if (!m_NetworkHelper.IsInTargetState())
		return FSM_Continue;

	// Make sure this flag is raised for the spring dampening on the
	// peds capsule to take effect
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsLanding, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);

	pPed->SetUseExtractedZ(false);	
	pPed->EnableCollision();

	if ((m_bCalledStartLadderSlide) || (m_bCloneAudioSlidingFlag))
		pPed->GetPedAudioEntity()->GetFootStepAudio().StopLadderSlide();

	if (m_NetworkHelper.GetBoolean(ms_MountEnded) || IsInterruptCondition()) //need new dismount anim to stop ped walking through ladder
	{
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);
		TaskSetState(State_Finish);
	}
	
	pPed->SetIsNotBuoyant(false);

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateAtTopDismount_OnEnter()
{
	m_NetworkHelper.SendRequest(ms_DismountTop); 
	m_NetworkHelper.WaitForTargetState(ms_OnEnterDismountTop);
	m_bNetworkBlendedIn = false;
	m_bPreventFPSMode = false;

#if FPS_MODE_SUPPORTED
	m_uTimeWhenAnimWasInterruptible = 0;
#endif

	GetPed()->DisableCollision();

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateAtTopDismount_OnUpdate(CPed* pPed)
{
	if( !m_NetworkHelper.IsInTargetState() )
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true );

		return FSM_Continue;
	}

	GetPed()->GetFootstepHelper().SetClimbingLadder(false);
	if( !m_bNetworkBlendedIn && m_NetworkHelper.IsNetworkAttached() && pPed->GetMovePed().IsTaskNetworkFullyBlended() )
		m_bNetworkBlendedIn = true;

	// To prevent peds walking through the wall in the end
	if( m_NetworkHelper.GetBoolean(ms_IsInterrupt) && !pPed->IsCollisionEnabled() )
	{
		pPed->SetWasStanding(false);
		pPed->EnableCollision();
	}

	if( pPed->IsCollisionEnabled() )
	{
		pPed->SetUseExtractedZ(false);
		pPed->SetIsStanding(true);
	}

	if( m_NetworkHelper.GetBoolean(ms_MountEnded) || IsInterruptCondition() )
	{
		pPed->SetDesiredHeading( pPed->GetCurrentHeading() ); 
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);
		TaskSetState(State_Finish);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateAtBottomDismount_OnEnter()
{
	m_NetworkHelper.SetFlag(CheckForWaterDismount(GetPed(), m_WaterLevel), ms_DismountWater);
	m_NetworkHelper.SendRequest(ms_DismountBottom); 
	m_NetworkHelper.WaitForTargetState(ms_OnEnterDismountBottom);

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateAtBottomDismount_OnUpdate(CPed* pPed) 
{
	if (!m_NetworkHelper.IsInTargetState())
		return FSM_Continue;
	
	pPed->SetIsNotBuoyant(false);

	pPed->SetUseExtractedZ(false);	

	pPed->EnableCollision();

	// Make sure this flag is raised for the spring dampening on the
	// peds capsule to take effect
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsLanding, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	pPed->GetFootstepHelper().SetClimbingLadder(false);

	if (m_NetworkHelper.GetBoolean(ms_MountEnded) || IsInterruptCondition())
	{
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);
		TaskSetState(State_Finish);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadder::StateBlocked_OnEnter(CPed* pPed)
{
	if(!pPed)
	{
		return FSM_Quit;
	}

	if(!pPed->IsNetworkClone())
	{
		m_vBlockPos		= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		m_fBlockHeading = pPed->GetTransform().GetHeading();
	}

	return FSM_Continue;		
}

CTask::FSM_Return CTaskClimbLadder::StateBlocked_OnUpdate(CPed* pPed)
{
	if(!pPed || !pPed->IsPlayer())
	{
		return FSM_Quit;
	}

	if(!m_pLadder)
	{
		return FSM_Continue;
	}

	// Ped has given up trying to get on ladder....
	if(!pPed->IsNetworkClone())
	{
		CControl* pControl = pPed->GetControlFromPlayer();

		if(pControl)
		{
			static dev_float DEADZONE = 0.33f;
			Vector2 vecStickInput;
			vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm(DEADZONE);
			vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm(DEADZONE);
			
			if(vecStickInput.Mag() > CTaskMovePlayer::ms_fStickInCentreMag)
			{
				float fCamOrient = camInterface::GetPlayerControlCamHeading(*pPed);

				float fStickAngle = rage::Atan2f(-vecStickInput.x, vecStickInput.y);
				fStickAngle = fStickAngle + fCamOrient;
				fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);

				if(fabs(fStickAngle - m_fBlockHeading) > 1.57f)
				{
					SetState(State_Finish);
				}
			}
		}
	}

	pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
	pPed->SetVelocity(VEC3_ZERO); 
	pPed->SetDesiredVelocity(VEC3_ZERO);

	if(!pPed->IsNetworkClone())
	{
		pPed->SetHeading(m_fBlockHeading); 
		pPed->SetPosition(m_vBlockPos, true, true);

		// ladder has become unblocked, we definitely get on....
		bool gettingOnAtTop = ((m_eStartState == State_MountingAtTop) || (m_eStartState == State_MountingAtTopRun));
		if(!gettingOnAtTop)
		{
			Assert((m_eStartState == State_MountingAtBottom) || (m_eStartState == State_MountingAtBottomRun));
		}

		if(!CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP(m_pLadder, m_EffectIndex, /*gettingOnAtTop ?*/ CTaskClimbLadderFully::BF_CLIMBING_DOWN /*: CTaskClimbLadderFully::BF_CLIMBING_UP*/))
		{
			if (m_eStartState == State_MountingAtBottom)
			{
				SetState(State_MountingAtBottom);
			}
			else if (m_eStartState == State_MountingAtBottomRun)
			{
				SetState(State_MountingAtBottomRun);
			}
			else if(m_eStartState == State_MountingAtTop)
			{
				SetState(State_MountingAtTop);
			}
			else if(m_eStartState == State_MountingAtTopRun)
			{
				SetState(State_MountingAtTopRun);
			}
		}
	}

	return FSM_Continue;
}

bool CTaskClimbLadder::LoadMetaDataCanMountBehind(CEntity* pLadderEntity, int iLadderIndex)
{
	if (pLadderEntity)
	{
		// Go through the effects one by one
		CBaseModelInfo* pBaseModel = pLadderEntity->GetBaseModelInfo();

		C2dEffect* pEffect = pBaseModel->Get2dEffect(iLadderIndex);

		if (pEffect->GetType() == ET_LADDER)
		{
			CLadderInfo* pLadderInfo = pEffect->GetLadder();

			atHashString DefaultLadder("DEFAULT",0xE4DF46D5); 
			if (pLadderInfo && pLadderInfo->ladderdef.IsNotNull())
				DefaultLadder = pLadderInfo->ladderdef;
			
			const CLadderMetadata* pMeta = CLadderMetadataManager::GetLadderDataFromHash(DefaultLadder.GetHash()); 
			taskAssertf(pMeta, "no meta found"); 
			if (pMeta)	
			{
				return pMeta->m_CanMountBehind;
			}
		}
	}

	return true;
}

void CTaskClimbLadder::LoadMetaDataFromLadderType()
{
	switch(m_LadderType)
	{
	case eSmallLadder:
	case eStandardLadder:
	case eBigLadder:
		{
			atHashString DefaultLadder("DEFAULT",0xE4DF46D5); 
			CLadderInfo* pLadderInfo = GetLadderInfo();
			if (pLadderInfo && pLadderInfo->ladderdef.IsNotNull())
				DefaultLadder = pLadderInfo->ladderdef;
			
			const CLadderMetadata* pMeta = CLadderMetadataManager::GetLadderDataFromHash(DefaultLadder.GetHash()); 
			taskAssertf(pMeta, "no meta found"); 
			if(pMeta)	
			{
				m_ClipSetId.SetHash(pMeta->m_ClipSet);
				m_FemaleClipSetId.SetHash(pMeta->m_FemaleClipSet);
				m_RungSpacing = pMeta->m_RungSpacing;
				m_bCanSlideDown = pMeta->m_CanSlide;
				//m_bCanMountBehind = pMeta->m_CanMountBehind;
			}
			
		}
		break; 
	default:
		{
			taskAssertf(0,"Invalid anim set for climbing task"); 
		}
	}
}

bool CTaskClimbLadder::IsAlreadyClimbingOrSlidingFromMigration(CPed* pPed)
{
	// players can't migrate...
	if (!pPed->IsPlayer())
	{
		// positional testing... can't just use AI state as AI can be 
		// set to ClimbUp when ped hasn't mounted ladder yet ...
		// if i'm at the top or the bottom (i.e. I'm getting on / off)
		if (( IsAtBottom( pPed ) ) || ( IsAtTop( pPed ) ))
			return false;
		
		if ((m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUp)		||
			(m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbDown)		||
			(m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUpFast)		||
			(m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbDownFast)	||
			(m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToSlideDown))
		{
			// AI is climbing up or down....
			return true;
		}
	}

	return false;
}

void CTaskClimbLadder::ChooseMigrationState(CPed* pPed)
{
	Assert(pPed);

	// decide on what we want to do with the sync AI state....
	if ((m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUp) || (m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUpFast))
	{
		// if the path is blocked, then we have to sit tight...
		bool pathBlocked = IsMovementBlocked(pPed, ZAXIS);

		// because there's a delay between the owner climbing and the clone climbing
		// when we migrate the clone may be ahead of the owner on the ladder, so we sit 
		// tight until the new owner is ahead of the clone...
		if((!pathBlocked) && (!pPed->IsNetworkClone()))
			TaskSetState(State_ClimbingUp);
		else
			TaskSetState(State_ClimbingIdle);

	}
	else if ((m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbDown) || (m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbDownFast))
	{
		bool pathBlocked = IsMovementBlocked(pPed, -ZAXIS);
		if ((!pathBlocked)&& (!pPed->IsNetworkClone()))
			TaskSetState(State_ClimbingDown);
		else
			TaskSetState(State_ClimbingIdle);

	}
	else if (m_eAIClimbState == CTaskGoToAndClimbLadder::InputState_WantsToSlideDown)
	{
		bool pathBlocked = IsMovementBlocked(pPed, -ZAXIS);
		if ((!pathBlocked) && (!pPed->IsNetworkClone()))
			TaskSetState(State_SlidingDown);
		else
			TaskSetState(State_ClimbingIdle);

	}
	else
	{
		Assert(false);
		TaskSetState(State_ClimbingIdle);
	}
}

//------------------------------------------------
// CTaskClimbLadder::ProcessLadderCoordinationMP
// Detect if two 
//------------------------------------------------

CTask::FSM_Return CTaskClimbLadder::ProcessLadderCoordinationMP(void)
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return FSM_Continue;
	}

	CPed* pLocalPed = GetPed();

	if(!pLocalPed || pLocalPed->IsNetworkClone() || !pLocalPed->GetPedIntelligence())
	{
		return FSM_Continue;
	}

	if(!m_pLadder)
	{
		return FSM_Continue;
	}

	u32 state = GetState();

	// are we in getting on the ladder?
	if( ! ( (state == State_MountingAtTop) || (state == State_MountingAtTopRun) || (state == State_MountingAtBottom) || (state == State_MountingAtBottomRun) ) )
	{
		return FSM_Continue;
	}
		
	Vector3 pedPos = VEC3V_TO_VECTOR3(pLocalPed->GetTransform().GetPosition());	

	// check nearby peds...
	CEntityScannerIterator entityList = pLocalPed->GetPedIntelligence()->GetNearbyPeds();
	for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		CPed* pNearbyPed = static_cast<CPed*>(pEnt);
		if( (pNearbyPed) && ( pNearbyPed != pLocalPed ) && (pNearbyPed->GetPedIntelligence()) )
		{
			Vector3 nearbyPedPos = VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition());	

			if((pedPos - nearbyPedPos).Mag2() < (5.0f * 5.0f))
			{
				CTaskClimbLadder* nearByLadderTask = static_cast<CTaskClimbLadder*>(pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER));
				if(nearByLadderTask)
				{
					// Getting on the ladder and same part of ladder?
					if( (nearByLadderTask->GetLadder() == GetLadder()) && (nearByLadderTask->GetEffectIndex() == GetEffectIndex()) )
					{
						// is other player getting on at same point?
						bool sameEntryPoint = false;
						if((state == State_MountingAtTop) || (state == State_MountingAtTopRun))
						{
							sameEntryPoint = (nearByLadderTask->GetState() == State_MountingAtTop) || (nearByLadderTask->GetState() == State_MountingAtTopRun);
						}
						else if((state == State_MountingAtBottom) || (state == State_MountingAtBottomRun))
						{
							sameEntryPoint = (nearByLadderTask->GetState() == State_MountingAtBottom) || (nearByLadderTask->GetState() == State_MountingAtBottomRun);
						}

						if(sameEntryPoint)
						{
							if(pNearbyPed->IsPlayer() && !pLocalPed->IsPlayer())
							{
								// always let the player get on first...
								return FSM_Quit;
							}
							else if(pNearbyPed->GetNetworkObject() && !pLocalPed->GetNetworkObject())
							{
								// Network ped vs non network ped, let the networkped get on first
								return FSM_Quit;
							}
							else if(pNearbyPed->GetNetworkObject() && pLocalPed->GetNetworkObject())
							{
								if(pNearbyPed->IsPlayer() && pLocalPed->IsPlayer())
								{
									if(pNearbyPed->GetNetworkObject()->GetPhysicalPlayerIndex() > pLocalPed->GetNetworkObject()->GetPhysicalPlayerIndex())
									{
										// Quit one of them out to stop us both getting on and overlapping...
										return FSM_Quit;
									}
								}
								else if(!pNearbyPed->IsPlayer() && !pLocalPed->IsPlayer())
								{
									// both just npcs, who has the highest index?
									if(pNearbyPed->GetNetworkObject()->GetObjectID() > pLocalPed->GetNetworkObject()->GetObjectID())
									{
										return FSM_Quit;
									}
								}								
							}
						}
					}
				}
			}
		}
	}
	
	return FSM_Continue;
}

void CTaskClimbLadder::UpdateCloneClimbRate(void)
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	if(!GetPed() || !GetPed()->IsNetworkClone())
	{
		return;
	}

	if((GetState() != State_ClimbingUp) && (GetState() != State_ClimbingDown))
	{
		return;
	}

	CPed const* pPed = GetPed();

	// Get the percentage distance of the entire ladder, the clone is behind the owner...
	Vector3 ownerPos = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
	Vector3 clonePos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// Abs - might be climbing down not up...
	float delta			= fabs(ownerPos.z - clonePos.z);

	float minDistance = 0.0f;
	float maxDistance = 3.0f;

	float minCloneClimbRateModifier = GetBaseClimbRate();
	float maxCloneClimbRateModifier = minCloneClimbRateModifier * 2.0f;

	float percent		= Clamp(delta / (maxDistance - minDistance), 0.0f, 1.0f);
	float newClimbRate	= minCloneClimbRateModifier + (percent * (maxCloneClimbRateModifier - minCloneClimbRateModifier));

//	grcDebugDraw::Line(ownerPos, clonePos, Color_red, Color_blue);
	
//	char buffer[32];
//	sprintf(buffer, "climb %2f", newClimbRate);
//	grcDebugDraw::Text(Lerp(0.5f, ownerPos, clonePos), Color_white, buffer, true);

	m_fClimbRate = newClimbRate;
}

void CTaskClimbLadder::ProcessLadderBlockingMP(void)
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	if((GetState() != State_Initial) && (GetState() != State_Blocked))
	{
		CPed const* pPed = GetPed();
		Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		if(pPed->IsNetworkClone())
		{
			// when climbing a ladder, clones lag slightly behind the owner so we use the network position to decide blocking.
			pedPos = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
		}

		// we should have a ladder by now...
		if(m_pLadder)
		{
			// if we're getting on or off....
			bool mountingAtTopState		= ((GetState() == State_MountingAtTop || GetState() == State_MountingAtTopRun));
			bool mountingAtBottomState	= ((GetState() == State_MountingAtBottom || GetState() == State_MountingAtBottomRun));
		
			// or we're near the top / or bottom where there's no room for another ped to get on or off (4.0f = four rungs - ped climbs four rungs per animation loop)...
			float numRungsPerAnimLoop = 4.0f;
			
			TUNE_GROUP_FLOAT(LADDER_DEBUG, numLoopsNeededForSpace,2.0f, 0.0f, 4.0f, 0.1f);

			float heightOffset	= ((m_RungSpacing * numRungsPerAnimLoop) * numLoopsNeededForSpace);
			m_minHeightToBlock	= m_vBottom.z	+ ms_fHeightBeforeBottomToFinishT + heightOffset + 0.2f;	// add 20cm just in case the player lines up with the rung in question
			m_maxHeightToBlock	= m_vTop.z		- ms_fHeightBeforeTopToFinishT - heightOffset - 0.2f;		// add 20cm just in case the player lines up with the rung in question

			// Sanity check for small ladders - will make the ladder always blocked if someone is on it...
			m_minHeightToBlock	= Min(m_vTop.z		- ms_fHeightBeforeTopToFinishT,		m_minHeightToBlock);
			m_maxHeightToBlock	= Max(m_vBottom.z	+ ms_fHeightBeforeBottomToFinishT,	m_maxHeightToBlock);

			// because the top and bottom locking areas can overlap, there's never a place on the ladder where we unblock so we do it here before re-blocking...
			UnblockLadderMP(true, true);

			// we're at the top or the bottom, we want to block a ped from getting on 
			BlockLadderMP((mountingAtTopState || pedPos.z > m_maxHeightToBlock), (mountingAtBottomState || pedPos.z < m_minHeightToBlock));
		}
	}
}

void CTaskClimbLadder::BlockLadderMP(bool const blockTop, bool const blockBottom)
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	if(blockTop)
	{
		if(m_topOfLadderBlockIndex == -1)
		{
			CTaskClimbLadderFully::BlockLadderMP(m_pLadder, m_EffectIndex, CTaskClimbLadderFully::BF_CLIMBING_DOWN, m_topOfLadderBlockIndex);
		}
	}

	if(blockBottom)
	{
		if(m_bottomOfLadderBlockIndex == -1)
		{
			CTaskClimbLadderFully::BlockLadderMP(m_pLadder, m_EffectIndex, CTaskClimbLadderFully::BF_CLIMBING_UP, m_bottomOfLadderBlockIndex);
		}
	}
}

void CTaskClimbLadder::UnblockLadderMP(bool const unblockTop, bool const unblockBottom)
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	if(m_topOfLadderBlockIndex != -1)
	{
		if(unblockTop)
		{
			CTaskClimbLadderFully::ReleaseLadderByIndexMP(m_topOfLadderBlockIndex);
			m_topOfLadderBlockIndex = -1;
		}
	}

	if(m_bottomOfLadderBlockIndex != -1)
	{
		if(unblockBottom)
		{
			CTaskClimbLadderFully::ReleaseLadderByIndexMP(m_bottomOfLadderBlockIndex);
			m_bottomOfLadderBlockIndex = -1;
		}
	}
}


CLadderClipSetRequestHelper* CLadderClipSetRequestHelperPool::GetNextFreeHelper(CPed* pPed)
{
	if (NetworkInterface::IsGameInProgress())
		return NULL;

	for (int i = 0; i < ms_iPoolSize; ++i)		
	{
		if (!ms_LadderClipSetHelperSlotUsed[i])
		{
			ms_LadderClipSetHelperSlotUsed[i] = true;
			sm_AssociatedPeds[i] = pPed;

			return &ms_LadderClipSetHelpers[i];
		}
	}

	return NULL;	
}

void CLadderClipSetRequestHelperPool::ReleaseClipSetRequestHelper(CLadderClipSetRequestHelper* pLadderClipSetRequestHelper)
{
	// Could use memory pos to calc index instead but meh...
	for (int i = 0; i < ms_iPoolSize; ++i)
	{
		if (&ms_LadderClipSetHelpers[i] == pLadderClipSetRequestHelper)
		{
			pLadderClipSetRequestHelper->ReleaseAllClipSets();	// Just to be sure
			ms_LadderClipSetHelperSlotUsed[i] = false;
			sm_AssociatedPeds[i] = NULL;
			break;
		}
	}
}

bool CTaskClimbLadder::ShouldUseFemaleClipSets(const CPed* pPed)
{
	if (pPed)
	{
		return !NetworkInterface::IsGameInProgress() && !pPed->IsMale();
	}
	return false;
}


#if !__FINAL
void CTaskClimbLadder::Debug() const
{
#if __BANK
	if (CClimbLadderDebug::ms_bRenderDebug)
	{
#if DEBUG_DRAW
		Vector3 vX(0.5f,0,0);
		Vector3 vY(0,0.5f,0);
		Color32 iCol(192,0,192,255);

		// Render the target position
		//grcDebugDraw::Line(m_vPedTargetPos-vX,m_vPedTargetPos+vX,iCol);
		//grcDebugDraw::Line(m_vPedTargetPos-vY,m_vPedTargetPos+vY,iCol);

		// Render the height of the ladder
		grcDebugDraw::Line(m_vBottom,m_vTop,iCol);

		// Render the position the ped should be at after getting off at the top
		grcDebugDraw::Sphere(m_vTop, 0.1f, Color_red, false);

		grcDebugDraw::Sphere(m_vBottom, 0.05f, Color_purple, true);

		// Render the ladder mid point and normal
		const Vector3 vLadderMid = (m_vTop + m_vBottom) * 0.5f;
		const Vector3 vLadderNormal(-rage::Sinf(m_fHeading), rage::Cosf(m_fHeading), 0.0f);

		char dbgTxt[64];
		sprintf(dbgTxt, "Ladder Top (%.1f, %.1f, %.1f)", m_vTop.x, m_vTop.y, m_vTop.z);
		grcDebugDraw::Text(vLadderMid, Color_yellow1, 0, 0, dbgTxt, false);
		sprintf(dbgTxt, "Ladder Base (%.1f, %.1f, %.1f)", m_vBottom.x, m_vBottom.y, m_vBottom.z);
		grcDebugDraw::Text(vLadderMid, Color_yellow1, 0, grcDebugDraw::GetScreenSpaceTextHeight(), dbgTxt, false);
		sprintf(dbgTxt, "Ladder Normal (%.1f, %.1f, %.1f)", vLadderNormal.x, vLadderNormal.y, vLadderNormal.z);
		grcDebugDraw::Text(vLadderMid, Color_yellow1, 0, grcDebugDraw::GetScreenSpaceTextHeight()*2, dbgTxt, false);
		sprintf(dbgTxt, "Get off at top? : %s", m_bCanGetOffAtTop ? "true" : "false");
		grcDebugDraw::Text(vLadderMid, Color_yellow1, 0, grcDebugDraw::GetScreenSpaceTextHeight()*3, dbgTxt, false);

#endif // DEBUG_DRAW
	}
#endif // __BANK
}
#endif /* !__FINAL */

#if DEBUG_DRAW
void CTaskClimbLadder::DebugCloneTaskInformation(void) 
{
	if (GetPed())
	{
		if (m_NetworkHelper.GetNetworkPlayer())
		{
			Vector3 pos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
			Vector3 top = m_vTop;
			Vector3 bottom = m_vBottom;

			if (IsAtTop(GetPed()) && GetActivePhase() > 0.1f)
				grcDebugDraw::Sphere(pos, 0.2f, Color_goldenrod, false);
			else if (IsAtBottom(GetPed()) && GetActivePhase() > 0.1f)
				grcDebugDraw::Sphere(pos, 0.2f, Color_goldenrod, false); 
			else
				grcDebugDraw::Sphere(pos, 0.2f, Color_purple, false);

			grcDebugDraw::Sphere(top, 0.1f, Color_green, false);
			top.z -= ms_fHeightBeforeTopToFinishT;
			grcDebugDraw::Sphere(top, 0.1f, Color_green4, false);


			grcDebugDraw::Sphere(bottom, 0.1f, Color_blue, false);
			bottom.z += ms_fHeightBeforeBottomToFinishT;
			grcDebugDraw::Sphere(bottom, 0.1f, Color_blue4, false);
	
			Vector3 minBlockPos=  m_vTop;
			minBlockPos.z = m_minHeightToBlock;
			grcDebugDraw::Sphere(minBlockPos, 0.15f, m_topOfLadderBlockIndex != -1 ? Color_yellow : Color_seashell, false);
			
			Vector3 maxBlockPos=  m_vTop;
			maxBlockPos.z = m_maxHeightToBlock;			
			grcDebugDraw::Sphere(maxBlockPos, 0.1f, m_bottomOfLadderBlockIndex != -1 ? Color_red : Color_DodgerBlue, false);
			
//			grcDebugDraw::Sphere(m_vTargetPosition, 0.1f, Color_orange, false);

			char buffer[1256];
			sprintf(buffer, "State %s\nClipSetId %s\nEffect Index %d\nStartPos %f %f %f\nHeading %f\nSpeed %f\nStart State %s\nLadderType %d\nInputState %d\nGetOnBackwards %d\nCanSlideDown %d\nClimbRate %f\n",
/*			IsPlayerUnderAIControl(),*/
			GetStaticStateName(GetState()), 
			m_ClipSetId.GetCStr(),
			(s16)m_EffectIndex,
			m_vPositionToEmbarkLadder.x, m_vPositionToEmbarkLadder.y, m_vPositionToEmbarkLadder.z,
			m_fHeading,
			m_fSpeed,
			GetStaticStateName(m_eStartState),
			m_LadderType,
			m_eAIClimbState,
			m_bGetOnBackwards,
			m_bCanSlideDown,
			m_fClimbRate
			);
			
			grcDebugDraw::Text(GetPed()->GetTransform().GetPosition(), Color_white, buffer);

			if(m_pLadder)
			{
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(m_pLadder->GetTransform().GetPosition()), Color_aquamarine, Color_orange);
			}
		}
	}
}
#endif /* DEBUG_DRAW */	

#if !__FINAL

const char * CTaskClimbLadder::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
	case State_Initial							: return "State_Initial";
	case State_WaitForNetworkAfterInitialising	: return "State_WaitForNetworkAfterInitialising";
	case State_WaitForNetworkAfterMounting		: return "State_WaitForNetworkAfterMounting";
	case State_MountingAtTop					: return "State_MountingAtTop"; 
	case State_MountingAtTopRun					: return "State_MountingAtTopRun";
	case State_MountingAtBottom					: return "State_MountingAtBottom";
	case State_MountingAtBottomRun				: return "State_MountingAtBottomRun";
	case State_ClimbingIdle						: return "State_ClimbingIdle";
	case State_ClimbingUp						: return "State_ClimbingUp";
	case State_ClimbingDown						: return "State_ClimbingDown";
	case State_DismountAtTop					: return "State_DismountAtTop";
	case State_DismountAtBottom					: return "State_DismountAtBottom"; 
	case State_SlidingDown						: return "State_SlidingDown";
	case State_SlideDismount					: return "State_SlideDismount";
	case State_Blocked							: return "State_Blocked";
	case State_Finish							: return "State_Finish";
	default : taskAssertf(0,"Unhandled state name");
	}

	return "State_Invalid";
}
#endif // !__FINAL

#if __BANK

void CTaskClimbLadder::VerifyNoPedsClimbingDeletedLadder(CEntity *pLadder)
{
	// Add peds to the list.
	CPed::Pool* pedPool = CPed::GetPool();
	s32 i = pedPool->GetSize();
	while(i--)
	{
		// Get the ped of interest.
		CPed* pPed = pedPool->GetSlot(i);
		if(pPed)
		{
			const CTaskClimbLadder* pClimbLadderTask  = (const CTaskClimbLadder*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER);
			if(pClimbLadderTask && (pClimbLadderTask->GetLadder() == pLadder) )
			{
				//! Don't assert - due to shutdown order etc, this may be valid. allows us to catch legitimate cases though.
				scrThread::PrePrintStackTrace();
				Warningf("Deleting a ladder that a ped (%s) is actively climbing on!", pPed->GetDebugName());
			}
		}
	}
}

#endif
