// File header
#include "Objects/Door.h"

// rage headers
#include "bank/bank.h"
#include "vector/geometry.h"
#include "phsolver/contactmgr.h"
#include "Physics/constrainthinge.h"
#include "Physics/constraintprismatic.h"

// game headers
#include "audio/ambience/ambientaudioentity.h"
#include "audio/northaudioengine.h"
#include "Control/garages.h"
#include "game/ModelIndices.h"
#include "Modelinfo/PedModelInfo.h"
#include "Network/NetworkInterface.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "Objects/DoorTuning.h"
#include "objects/DummyObject.h"
#include "Objects/object.h"
#include "Peds/ped.h"
#include "Pathserver/PathServer.h"
#include "Physics/debugcontacts.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "script/commands_interiors.h"
#include "script/commands_object.h"
#include "script/script.h"
#include "fwdebug/picker.h"
#include "modelinfo/MloModelInfo.h"
#include "control/replay/replay.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "Cutscene/CutSceneManagerNew.h"

SCENE_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()
AI_OPTIMISATIONS()
ENTITY_OPTIMISATIONS()

#define DOOR_HIT_BY_PLAYER_FORCE_OPEN (16)

#define MAX_DOORSYSTEM_MAP_SIZE (375)
#define IGNORE_OLD_OVERIDES 1 

PARAM(doorExcessiveLogging, "Extra logging for doors");

AUTOID_IMPL(CDoorExtension);

bank_u32 CDoor::ms_uMaxRagdollDoorContactTime = 1000;
bool CDoor::ms_bUpdatingDoorRatio = false;
dev_float CDoor::ms_fPlayerDoorCloseDelay = 4.0f;
dev_float CDoor::ms_fEntityDoorCloseDelay = 1.5f;
bank_float CDoor::ms_fStdDoorDamping = 1.5f;
bank_float CDoor::ms_fStdFragDoorDamping = 1.8f;
f32 CDoor::sm_fLatchResetThreshold = 0.25f;
bool CDoor::ms_openDoorsForRacing = false;
bool CDoor::ms_snapOpenDoorsForRacing = false;

dev_bool TEST_SLIDING_DOORS = false;
dev_bool TEST_GARAGE_DOORS = false;
dev_bool TEST_BARRIER_ARMS = false;

fwPtrListSingleLink CDoor::ms_doorsList;

CDoorSystem::DoorSystemMap CDoorSystem::ms_doorMap;
CDoorSystem::LockedMap     CDoorSystem::ms_lockedMap;

#if __BANK
bool CDoorSystem::ms_logDebugDraw = false;
bool CDoorSystem::ms_logDoorPhysics = false;

bool CDoor::sm_ForceAllAutoOpenToOpen = false;
bool CDoor::sm_ForceAllAutoOpenToClose = false;
bool CDoor::sm_ForceAllToBreak = false;
bool CDoor::sm_ForceAllTargetRatio = false;
float CDoor::sm_ForcedTargetRatio = 0.0f;
bool CDoor::sm_IngnoreScriptSetDoorStateCommands = false;
char CDoor::sm_AddDoorString[CDoor::DEBUG_DOOR_STRING_LENGTH];
bool CDoor::sm_SnapOpenBarriersForRaceToggle = false;

bool sm_ProcessPhysics_BreakOnSelectedDoor = false;
bool sm_ProcessControlLogic_BreakOnSelectedDoor = false;
bool sm_enableHackGarageShut = false;
bool sm_breakOnSelectedDoorSetState = false;
bool sm_breakOnSelectedDoorSetFixedPhysics = false;
bool sm_logStateChangesForSelectedDoor = false;
int  sm_debugDooorSystemEnumHash = -1;
char sm_debugDooorSystemEnumString[CDoor::DEBUG_DOOR_STRING_LENGTH];
char sm_debugDooorSystemEnumNumber[CDoor::DEBUG_DOOR_STRING_LENGTH];
void SetDoorDebugStringHash();

bool sm_DebugDoorNearLocation = false;
bool sm_DebugBreakOnInitOfDoor = false;
bool sm_DebugBreakOnAddPhysicsOfDoor = false;
Vector3 sm_DebugDoorPosition(0.0f, 0.0f, 0.0f);
bool sm_DebugAddDoorToPickerForAddPhysicsOnInit = false;
bool sm_DebugAddDoorToPickerForAddPhysics = false;
float sm_DebugDoorRadius = 1.0f;

#else
const bool sm_enableHackGarageShut = false;
#endif	// __BANK

float CDoor::sm_fMaxStuckDoorRatio = 0.5f;

void CDoorExtension::InitEntityExtensionFromDefinition(const fwExtensionDef* definition, fwEntity* entity)
{
	fwExtension::InitEntityExtensionFromDefinition( definition, entity );
	const CExtensionDefDoor&	doorDef = *smart_cast< const CExtensionDefDoor* >( definition );

	m_enableLimitAngle = doorDef.m_enableLimitAngle;
	m_startsLocked = doorDef.m_startsLocked;
	m_limitAngle = doorDef.m_limitAngle;
	m_doorTargetRatio = doorDef.m_doorTargetRatio;
	m_audioHash = doorDef.m_audioHash;
}

CDoor::CDoor(const eEntityOwnedBy ownedBy)
	: CObject(ownedBy)
	, m_ClonedArchetype(NULL)
	, m_bHoldingOpen(false)
	, m_bUnderCodeControl(false)
	, m_bOpening(false)
	, m_fDoorCloseDelay(0.0f)
	, m_bDoorCloseDelayActive(false)
	, m_bOnActivationUpdate(false)
	, m_bAttachedToARoom0Portal(false)
	, m_wantToBeAwake(false)
	, m_ePlayerDoorState( PDS_NONE )
#if __BANK
	,m_processedPhysicsThisFrame(0)
#endif
{
	Init();
}

CDoor::CDoor(const eEntityOwnedBy ownedBy, s32 nModelIndex, bool bCreateDrawable, bool bInitPhys)
	: CObject(ownedBy, nModelIndex, bCreateDrawable, bInitPhys)
	, m_ClonedArchetype(NULL)
	, m_bHoldingOpen(false)
	, m_bUnderCodeControl(false)
	, m_bOpening(false)
	, m_fDoorCloseDelay(0.0f)
	, m_bDoorCloseDelayActive(false)
	, m_bOnActivationUpdate(false)
	, m_bAttachedToARoom0Portal(false)
	, m_wantToBeAwake(false)
	, m_ePlayerDoorState( PDS_NONE )
#if __BANK
	,m_processedPhysicsThisFrame(0)
#endif
{
	Init();
}

CDoor::CDoor(const eEntityOwnedBy ownedBy, class CDummyObject* pDummy)
	: CObject(ownedBy, pDummy)
	, m_ClonedArchetype(NULL)
	, m_bHoldingOpen(false)
	, m_bUnderCodeControl(false)
	, m_bOpening(false)
	, m_fDoorCloseDelay(0.0f)
	, m_bDoorCloseDelayActive(false)
	, m_bOnActivationUpdate(false)
	, m_bAttachedToARoom0Portal(false)
	, m_wantToBeAwake(false)
	, m_ePlayerDoorState( PDS_NONE )
#if __BANK
	,m_processedPhysicsThisFrame(0)
#endif
{
	RemoveDoorFromList(pDummy);
	Init();
}


CDoor::~CDoor()
{
	naAssertf(!sysThreadType::IsUpdateThread() || !audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), "Deleted a door entity from the main thread while the audio update thread is running.");

	ms_doorsList.Remove(this);

	if (GetRelatedDummy())
	{
		ms_doorsList.Add(GetRelatedDummy());
	}

 	if (m_DoorSystemData /*&& !IsNetworkClone()*/)
	{
		m_DoorSystemData->SetBrokenFlags(0);
		m_DoorSystemData->SetDamagedFlags(0);
	}

	CDoorSystem::UnlinkDoorObject(*this);

	// If we cloned the archetype, we want to make sure that it gets properly
	// deleted at this point.
	if(m_ClonedArchetype)
	{
		// First, clear out the pointer in the phInst, if it points to this object,
		// so we can reduce the ref count from that first.
		phInst* pInst = GetCurrentPhysicsInst();
		if(pInst && pInst->GetArchetype() == m_ClonedArchetype)
		{
			pInst->SetArchetype(NULL);
		}

		// Release the archetype. We're meant to get 0 back, meaning that it got destroyed,
		// if we don't, something may have gone wrong and we may have a leak.
		// Deleting the phArchetype should also delete the phBound clone by releasing its
		// last reference.
		(void)Verifyf(m_ClonedArchetype->Release() == 0, "Door archetype ref count problem detected (%d references remaining).", m_ClonedArchetype->GetRefCount());
		m_ClonedArchetype = NULL;
	}
}


//
// Stuff that always gets called from the constructor (Init to defaults)
//
void CDoor::Init()
{
	// It's not expected that we create CDoor objects if owned by the fragment
	// cache, since then this is probably a piece that broke off from another door.
	Assert(GetOwnedBy() != ENTITY_OWNEDBY_FRAGMENT_CACHE);

	// To protect against possible leakage if Init() is unexpectedly called again, we
	// require the constructor to set the archetype pointer to NULL, so we can assert here.
	Assert(!m_ClonedArchetype);
	m_ClonedArchetype = NULL;

	// union remember - so don't try to initialise with anything other than 0
	m_fDoorTargetRatio = 0.0f;
	m_nDoorInstanceDataIdx = -1;
	m_nDoorShot = 0;
	m_fAutomaticDist = 0.0f;
	m_fAutomaticRate = 0.0f;

	m_alwaysPushVehicles = false;

	m_nObjectFlags.bIsDoor = true;

	m_nDoorType = DOOR_TYPE_NOT_A_DOOR;

	m_vecDoorStartPos.Zero();
	m_qDoorStartRot.Identity();

	m_uNextScheduledDetermineShouldAutoOpenTimeMS = 0;
	m_bCachedShouldAutoOpenStatus = false;

	m_WasShutForPortalPurposes = false;

	m_alwaysPush = false;

	m_Tuning = NULL;

	m_DoorAudioEntity.Init();

	m_DoorSystemData = NULL;
	m_garageZeroVelocityFrameCount = 0;

	ms_doorsList.Add(this);

	m_bCountsTowardOpenPortals = false;

#if __BANK
	if (sm_DebugDoorNearLocation )
	{
		Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
		if ((doorPos - sm_DebugDoorPosition).Mag() < sm_DebugDoorRadius)
		{		
			if (sm_DebugBreakOnInitOfDoor)
			{
				__debugbreak();
			}
			if (g_PickerManager.IsEnabled() && sm_DebugAddDoorToPickerForAddPhysicsOnInit)
			{
				bool alreadyAdded = false;
				for (int i = 0; i < g_PickerManager.GetNumberOfEntities(); i++)
				{
					if (g_PickerManager.GetEntity(i) == this)
					{
						alreadyAdded = true;
						break;

					}
				}

				if (!alreadyAdded)
				{
					g_PickerManager.AddEntityToPickerResults(this, false, true);
				}
			}
		}
	}
#endif // __BANK

#if GTA_REPLAY
	m_ReplayDoorOpenRatio = FLT_MAX;

	if(CReplayMgr::IsEditModeActive() == false)
	{
		// need to handle animating doors, not just on activate.
		CReplayMgr::RecordMapObject(this);
	}
#endif // GTA_REPLAY
}

#if GTA_REPLAY
void CDoor::InitReplay(const Matrix34& matOriginal)
{
	if(m_nDoorType != DOOR_TYPE_NOT_A_DOOR)
	{
		return;
	}

	CBaseModelInfo* pModelInfo = GetBaseModelInfo();
	m_nDoorType = DetermineDoorType(pModelInfo, pModelInfo->GetBoundingBoxMin(), pModelInfo->GetBoundingBoxMax());
	m_vecDoorStartPos = matOriginal.d;
	matOriginal.ToQuaternion(m_qDoorStartRot);
	if(!m_Tuning)
	{
		m_Tuning = &CDoorTuningManager::GetInstance().GetTuningForDoorModel(pModelInfo->GetModelNameHash(), m_nDoorType);
	}
	if(GetDoorAudioEntity())
	{
		GetDoorAudioEntity()->InitDoor(this);
	}

	if(GetCurrentPhysicsInst())
	{
		// We need the doors to be in the physics sim. for shape tests to work. Decals relay on the shape tests.
		UpdatePhysicsFromEntity(false);
		GetCurrentPhysicsInst()->SetMatrix(GetMatrix());
		GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE,true); 

		// Add the door if it`s not already in the level (some door objects create and add physics using the frag system through a call from CObject::CreateDrawable()).
		if(GetCurrentPhysicsInst()->IsInLevel() == false)
			CPhysics::GetSimulator()->AddInactiveObject(GetCurrentPhysicsInst());
	}
}
#endif  //GTA_REPLAY

#if __BANK
void CDoor::SetFixedPhysics(bool bFixed, bool bNetwork)
{
	if (sm_breakOnSelectedDoorSetFixedPhysics && this == g_PickerManager.GetSelectedEntity())
	{
		__debugbreak();
	}
	CPhysical::SetFixedPhysics(bFixed, bNetwork);
}
#endif

void CDoor::EnforceLockedObjectStatus(const bool useSmoothTransitionHash)
{
	// look up the door & set attributes accordingly

	CDoorSystemData *pStatus = useSmoothTransitionHash ? CDoorSystem::GetDoorData(m_SmoothTransitionPosHash) : CDoorSystem::GetDoorData(this);

	if (pStatus && pStatus->GetState() != DOORSTATE_INVALID && (GetBaseModelInfo()->GetHashKey() == (u32)pStatus->GetModelInfoHash()))
	{
		const bool bFixedStateChanged = (IsBaseFlagSet(fwEntity::IS_FIXED) != pStatus->GetLocked());
		SetFixedPhysics(pStatus->GetLocked(), false);
		if(pStatus->GetTargetRatio() != 0.0f)
			SetTargetDoorRatio(pStatus->GetTargetRatio(), true);
		SetDoorControlFlag(DOOR_CONTROL_FLAGS_UNSPRUNG, pStatus->GetUnsprung());

		// Probably not needed, but clear this flag just in case it had some other value.
		// It's assumed that when a door gets streamed in, we can just pop it to the
		// desired position, so no need for this smooth close flag.
		SetDoorControlFlag(DOOR_CONTROL_FLAGS_LOCK_WHEN_CLOSED, false);

		// If door is newly locked/unlocked, then update object representation in pathserver
		UpdateNavigation(bFixedStateChanged);

		if (pStatus->GetAutomaticRate() != 0.0f)
		{
			m_fAutomaticRate = pStatus->GetAutomaticRate();
		}

		if(pStatus->GetAutomaticDistance() != 0.0f)
		{
			m_fAutomaticDist = pStatus->GetAutomaticDistance();
		}
	}
}

void CDoor::CalcAndApplyTorqueSLD(float fDoorTargetRatio, float fDoorCurrentRatio, float fTimeStep)
{
	static float SLIDE_DOOR_FORCE_DELTA_LIMIT = 0.5f;
	static float SLIDE_DOOR_FORCE_DELTA_MULT = 5.0f;
	static float SLIDE_DOOR_FORCE_VEL_LIMIT = 5.0f;
	static float SLIDE_DOOR_FORCE_VEL_MULT = 0.9f;

	float absTarget = fabsf(fDoorCurrentRatio);
	bool forcePosition = false;
	float desiredRatio = 0.0f;

	if( GetDoorType() < DOOR_TYPE_STD_SC )
	{
		//Closing ... 
		if (!IsDoorOpening() && absTarget <= SMALL_FLOAT)
		{
			//is at zero ...
			forcePosition = true;
			desiredRatio = 0.0f;
		}		//Opening ... 
		else if (IsDoorOpening() && absTarget >= 1.0f-SMALL_FLOAT)
		{
			//is at 1 or -1
			forcePosition = true;
			if (fDoorTargetRatio > 0.0f)
				desiredRatio = 1.0f;
			else
				desiredRatio = -1.0f;

		}
	}

	if (forcePosition)
	{
		Vec3V slideAxisV;
		float fSlideLimit;
		GetSlidingDoorAxisAndLimit(slideAxisV, fSlideLimit);

		Vector3 vecDesiredPos = m_vecDoorStartPos + VEC3V_TO_VECTOR3(slideAxisV) * (desiredRatio * fSlideLimit);
		if (GetCurrentPhysicsInst())
		{
			GetCurrentPhysicsInst()->SetPosition(VECTOR3_TO_VEC3V(vecDesiredPos));
			UpdateEntityFromPhysics(GetCurrentPhysicsInst(), 1);
		}

		if (GetCollider())
		{
			Vector3 noVelocity(0.0f, 0.0f, 0.0f);
			GetCollider()->SetVelocity(noVelocity);
		}
 		m_DoorAudioEntity.ProcessAudio(0.0f);
		
	}
	else
	{
		Vector3 vecAxis;
		float fSlideLimit;
		GetSlidingDoorAxisAndLimit(RC_VEC3V(vecAxis), fSlideLimit);

		Vector3 vecDesiredPos = m_vecDoorStartPos + vecAxis * fDoorTargetRatio * fSlideLimit;
		/// Vector3 vecDesiredPosOld = m_vecDoorStartPos + vecAxis * fDoorTargetOld * fSlideLimit;
		float fDeltaPos = DotProduct(vecAxis, vecDesiredPos - VEC3V_TO_VECTOR3(GetTransform().GetPosition()));
		if(fDeltaPos > SLIDE_DOOR_FORCE_DELTA_LIMIT)
			fDeltaPos = SLIDE_DOOR_FORCE_DELTA_LIMIT;
		else if(fDeltaPos < -SLIDE_DOOR_FORCE_DELTA_LIMIT)
			fDeltaPos = -SLIDE_DOOR_FORCE_DELTA_LIMIT;
		float fApplyForce = fDeltaPos * SLIDE_DOOR_FORCE_DELTA_MULT;

		const Vector3 velocity = GetVelocity();
		float fCurrentVel = DotProduct(vecAxis, velocity);
		float fTargetVel = fSlideLimit*(fDoorTargetRatio - fDoorCurrentRatio) / rage::Max(0.03f, fwTimer::GetTimeStep());

		float audioVelocity = (vecDesiredPos - VEC3V_TO_VECTOR3(GetTransform().GetPosition())).Mag2()/fwTimer::GetTimeStep();
		m_DoorAudioEntity.ProcessAudio(audioVelocity);

		float fDeltaVel = fTargetVel - fCurrentVel;
		if(fDeltaVel > SLIDE_DOOR_FORCE_VEL_LIMIT)
			fDeltaVel = SLIDE_DOOR_FORCE_VEL_LIMIT;
		else if(fDeltaVel < -SLIDE_DOOR_FORCE_VEL_LIMIT)
			fDeltaVel = -SLIDE_DOOR_FORCE_VEL_LIMIT;

		fApplyForce += fDeltaVel * SLIDE_DOOR_FORCE_VEL_MULT;

		static dev_float fApplyThreshold = 0.01f;
		if( rage::Abs( fApplyForce ) > fApplyThreshold )
		{
			// To avoid being perpetually awake and never reaching the goal by some small torque
			//  we try to let the thing go to sleep from as close as we could get
			const float fRatioDiff = fabsf(fDoorTargetRatio - fDoorCurrentRatio);
			static dev_float fRatioReallyCloseEpsilon = 0.01f;
			const bool bIsVeryClose = fRatioDiff <= fRatioReallyCloseEpsilon;
			// Also, only wake up if currently inactive
			if( !bIsVeryClose )
			{
				ActivatePhysics();
				m_wantToBeAwake = true;
			}

			phCollider* pCollider = GetCollider();
			if( pCollider != NULL )
			{
				pCollider->ApplyForceCenterOfMass(fApplyForce * GetMass() * vecAxis, ScalarV(fTimeStep).GetIntrin128ConstRef());				
			}
		}
	}
}

void CDoor::CalcAndApplyTorqueGRG(float fDoorTargetRatio, float fDoorCurrentRatio, float fTimeStep)
{
	static float GARAGE_TORQUE_ANGLE_LIMIT = 0.5f;
	static float GARAGE_TORQUE_ANGLE_MULT = 2.0f;
	static float GARAGE_TORQUE_ANGVEL_LIMIT = 5.0f;

	// ok treat heading for pitch of the door as ATan(z/y) looking at the fwd (b) vector
	Vector3 vecForward(VEC3V_TO_VECTOR3(GetTransform().GetB()));
	float fCurrentHeading = rage::Atan2f(vecForward.z, vecForward.XYMag());
	float fLimitAngle = -GetLimitAngle();
	float fDesiredHeading = -fDoorTargetRatio * fLimitAngle;
	float fDesiredHeadingOld = -fDoorCurrentRatio * fLimitAngle;

	float fDeltaHeading = fDesiredHeading - fCurrentHeading;
	fDeltaHeading = fwAngle::LimitRadianAngleFast(fDeltaHeading);

	if(fDeltaHeading > GARAGE_TORQUE_ANGLE_LIMIT)	fDeltaHeading = GARAGE_TORQUE_ANGLE_LIMIT;
	else if (fDeltaHeading < -GARAGE_TORQUE_ANGLE_LIMIT)	fDeltaHeading = -GARAGE_TORQUE_ANGLE_LIMIT;

	float fApplyTorque = fDeltaHeading * GARAGE_TORQUE_ANGLE_MULT;

	// Since fDoorTarget/fDoorTargetOld only get updated once per frame, not once per physics
	// step, we need to use the full frame time when computing the target velocity.
	const float updateTimeStep = Max(SMALL_FLOAT, fwTimer::GetTimeStep());
	const float invUpdateTimeStep = 1.0f/updateTimeStep;

	float fCurrentAngVel = DotProduct(VEC3V_TO_VECTOR3(GetTransform().GetA()), GetAngVelocity());
	float fTargetAngVel = (fDesiredHeading - fDesiredHeadingOld)*invUpdateTimeStep;


	float fDeltaAngVel = fTargetAngVel - fCurrentAngVel;
	if( m_Tuning )
		fDeltaAngVel = Clamp( fDeltaAngVel, -m_Tuning->m_TorqueAngularVelocityLimit, m_Tuning->m_TorqueAngularVelocityLimit );
	else
		fDeltaAngVel = Clamp( fDeltaAngVel, -GARAGE_TORQUE_ANGVEL_LIMIT, GARAGE_TORQUE_ANGVEL_LIMIT );

	// Next, we will compute how much torque we need to apply to get the
	// angular velocity close to the target. We need to determine over how long
	// time we expect to apply the torque so we can scale it accordingly. First,
	// we get the rate at which this door would auto-open:
	const float autoOpenRate = GetEffectiveAutoOpenRate();
	Assert(autoOpenRate > SMALL_FLOAT);

	// In order to be able to open at close to the desired auto-open rate, we'll have to accelerate
	// to this velocity in a fraction of the auto-open time. A little bit of visible acceleration
	// is probably desirable, though.
	static float s_InvAccFraction = 1.0f/0.1f;	// Accelerate in 10% of the opening time

	// Compute how much we should scale the torque by. The upper limit we ever allow is
	// as much as would accelerate us to the target in one frame (not one physics step).
	const float invAccTime = Min(autoOpenRate*s_InvAccFraction, invUpdateTimeStep);

	// Add the torque for matching the velocity.
	fApplyTorque += fDeltaAngVel*invAccTime;

	m_DoorAudioEntity.ProcessAudio(fCurrentAngVel);

	if (sm_enableHackGarageShut && !m_DoorControlFlags.IsSet(DOOR_CONTROL_FLAGS_LOCK_WHEN_CLOSED))
	{
		// Hack to deal with garage doors not closing properly
		if (m_garageZeroVelocityFrameCount == -1)
		{
			return;
		}
		else
		{
			const float angularVelocityMin = 0.015f;
			const float deltaHeadingMin    = 0.03f;
			const float targetRatioMin     = 0.060f;

			if (m_fDoorTargetRatio <= targetRatioMin && fabsf(fCurrentAngVel) < angularVelocityMin && fDeltaHeading < deltaHeadingMin)
			{
				m_garageZeroVelocityFrameCount++;
			}
			else
			{
				m_garageZeroVelocityFrameCount = 0;
			}
		}
	}

	static dev_float fApplyThreshold = 0.01f;
	if( rage::Abs( fApplyTorque ) > fApplyThreshold )
	{
		// To avoid being perpetually awake and never reaching the goal by some small torque
		//  we try to let the thing go to sleep from as close as we could get
		const float fRatioDiff = fabsf(fDoorTargetRatio - fDoorCurrentRatio);

#if RSG_PC
		const float fDefaultTimestep			= (1.0f / 30.0f) ;				// 30fps
		const float fDefaultEpsilonRatio		= ( fDefaultTimestep / 0.005f );	// tuned for 30fps
		const float fRatioReallyCloseEpsilon	= fwTimer::GetTimeStep() / fDefaultEpsilonRatio; 
#else
		static dev_float fRatioReallyCloseEpsilon = 0.005f;
#endif
		const bool bIsVeryClose = fRatioDiff <= fRatioReallyCloseEpsilon;
		// Also, only wake up if currently inactive
		if( !bIsVeryClose )
		{
			ActivatePhysics();
			m_wantToBeAwake = true;
		}

		phCollider* pCollider = GetCollider();
		if( pCollider != NULL )
		{
			pCollider->ApplyTorque(fApplyTorque * GetAngInertia().x * VEC3V_TO_VECTOR3(GetTransform().GetA()), ScalarV(fTimeStep).GetIntrin128ConstRef());			
		}
	}
}

void CDoor::CalcAndApplyTorqueBAR(float fDoorTargetRatio, float fDoorCurrentRatio, float fTimeStep)
{
	static float ARM_TORQUE_ANGLE_LIMIT = 0.5f;
	static float ARM_TORQUE_ANGLE_MULT = 4.0f;
	static float ARM_TORQUE_ANGVEL_LIMIT = 5.0f;
	static float ARM_TORQUE_ANGVEL_MULT = 2.0f;

	// For fragInsts, the fragment system likes to recompute the center of gravity,
	// while the door code wants it to be at the local origin. Having it there helps
	// because we don't have to worry about gravity, etc, though we may be able to
	// get away without it. Until we can find a better way, this code resets the center
	// of gravity if the fragment system moves it.
	fragInst* pFragInst = GetFragInst();
	ResetCenterOfGravityForFragDoor(pFragInst);

	// ok treat heading for pitch of the door as ATan(z/y) looking at the across (a) vector
	Vector3 vecAcross(VEC3V_TO_VECTOR3(GetTransform().GetA()));
	float fCurrentHeading = -rage::Atan2f(vecAcross.z, vecAcross.XYMag());
	float fLimitAngle = GetLimitAngle();
	float fDesiredHeading = -fDoorTargetRatio * fLimitAngle;
	float fDesiredHeadingOld = -fDoorCurrentRatio * fLimitAngle;

	float fDeltaHeading = fDesiredHeading - fCurrentHeading;
	fDeltaHeading = fwAngle::LimitRadianAngleFast(fDeltaHeading);

	if(fDeltaHeading > ARM_TORQUE_ANGLE_LIMIT)	fDeltaHeading = ARM_TORQUE_ANGLE_LIMIT;
	else if (fDeltaHeading < -ARM_TORQUE_ANGLE_LIMIT)	fDeltaHeading = -ARM_TORQUE_ANGLE_LIMIT;

	float fApplyTorque = fDeltaHeading * ARM_TORQUE_ANGLE_MULT;

	// Since fDoorTarget/fDoorTargetOld only get updated once per frame, not once per physics
	// step, we need to use the full frame time when computing the target velocity.
	const float updateTimeStep = Max(SMALL_FLOAT, fwTimer::GetTimeStep());

	float fCurrentAngVel = DotProduct(VEC3V_TO_VECTOR3(GetTransform().GetB()), GetAngVelocity());
	float fTargetAngVel = (fDesiredHeading - fDesiredHeadingOld)/updateTimeStep;

	float fDeltaAngVel = fTargetAngVel - fCurrentAngVel;
	if( m_Tuning )
		fDeltaAngVel = Clamp( fDeltaAngVel, -m_Tuning->m_TorqueAngularVelocityLimit, m_Tuning->m_TorqueAngularVelocityLimit );
	else
		fDeltaAngVel = Clamp( fDeltaAngVel, -ARM_TORQUE_ANGVEL_LIMIT, ARM_TORQUE_ANGVEL_LIMIT );

	fApplyTorque += fDeltaAngVel * ARM_TORQUE_ANGVEL_MULT;

	m_DoorAudioEntity.ProcessAudio(fCurrentAngVel);

	// Typical conditions for waking up the arm are the absolute magnitude of the torque and current angular velocity of the arm
	static dev_float sfTorqueThreshold = 0.01f;
	if( rage::Abs( fApplyTorque ) >= sfTorqueThreshold )
	{
		// To avoid being perpetually awake and never reaching the goal by some small torque
		//  we try to let the thing go to sleep from as close as we could get
		const float fRatioDiff = fabsf(fDoorTargetRatio - fDoorCurrentRatio);
		static dev_float fRatioReallyCloseEpsilon = 0.02f;
		const bool bIsVeryClose = fRatioDiff <= fRatioReallyCloseEpsilon;
		// Also, only wake up if currently inactive
		if( !bIsVeryClose )
		{
			ActivatePhysics();
			m_wantToBeAwake = true;
		}

		phCollider* pCollider = GetCollider();
		if( pCollider != NULL )
		{
			pCollider->ApplyTorque(fApplyTorque * GetAngInertia().y * VEC3V_TO_VECTOR3(GetTransform().GetB()), ScalarV(fTimeStep).GetIntrin128ConstRef());
		}
	}
}

void CDoor::CalcAndApplyTorqueSTD(float fDoorTargetRatio, float fDoorCurrentRatio, float fTimeStep)
{	
	// If no frag inst, the below function should quit out immediately and have no effect
	fragInst* pFragInst = GetFragInst();
	ResetCenterOfGravityForFragDoor(pFragInst);
	
	// Skip torque application if we are fully open
	static dev_float sfRatioEpsilon = 0.02f;
	if( IsDoorFullyOpen( sfRatioEpsilon ) )
	{
#if __BANK
		if( CDoorSystem::ms_logDoorPhysics &&
			m_DoorSystemData )
		{
			Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
			physicsDisplayf( "CalcAndApplyTorqueSTD: Door already fully open (%f, %f, %f) %s\tTarget: %.2f\tCurRatio: %.2f", 
								doorPos.x, doorPos.y, doorPos.z, m_DoorSystemData->GetNetworkObject() ?  m_DoorSystemData->GetNetworkObject()->GetLogName() : "", 
								m_fDoorTargetRatio, fDoorCurrentRatio );
		}
#endif // #if __BANK
		return;
	}

	static float DOOR_TORQUE_ANGLE_LIMIT = 0.5f;
	static float DOOR_TORQUE_ANGLE_MULT = 2.0f;
	static float DOOR_TORQUE_ANGVEL_LIMIT = 5.0f;
	static float DOOR_TORQUE_ANGVEL_MULT = 0.9f;

	float fCurrentHeading = GetTransform().GetHeading();
	bool bDoorSpringActive = m_DoorControlFlags.IsClear(DOOR_CONTROL_FLAGS_UNSPRUNG);
	float fOriginalHeading = m_qDoorStartRot.TwistAngle(ZAXIS);
	float fLimitAngle = GetLimitAngle();
	float fDesiredHeading = fOriginalHeading + (fDoorTargetRatio * fLimitAngle);
	float fDesiredHeadingOld = fOriginalHeading + (fDoorCurrentRatio * fLimitAngle);

	float fDeltaHeading = bDoorSpringActive ? fDesiredHeading - fCurrentHeading : 0.0f;

	fDeltaHeading = fwAngle::LimitRadianAngleFast(fDeltaHeading);

	if(fDeltaHeading > DOOR_TORQUE_ANGLE_LIMIT)	
	{
		fDeltaHeading = DOOR_TORQUE_ANGLE_LIMIT;
	}
	else if (fDeltaHeading < -DOOR_TORQUE_ANGLE_LIMIT)	
	{
		fDeltaHeading = -DOOR_TORQUE_ANGLE_LIMIT;
	}

	float fApplyTorque		= 0.0f;
	float fCurrentAngVel	= GetAngVelocity().z;
	float fTargetAngVel		= (fDesiredHeading - fDesiredHeadingOld) / rage::Max(0.03f, fwTimer::GetTimeStep());
	float fDeltaAngVel		= fTargetAngVel - fCurrentAngVel;

	if( m_alwaysPush )
	{
		fApplyTorque = ( fDeltaAngVel / fwTimer::GetTimeStep() );
	}
	else
	{
		fApplyTorque = fDeltaHeading * DOOR_TORQUE_ANGLE_MULT;

		// Note: the time step for fTargetVel above should probably be fwTimer::GetTimeStep() (for a full
		// frame) instead, not the time step for each physics update.

		if( m_Tuning )
		{
			fDeltaAngVel = Clamp( fDeltaAngVel, -m_Tuning->m_TorqueAngularVelocityLimit, m_Tuning->m_TorqueAngularVelocityLimit );
		}
		else
		{
			fDeltaAngVel = Clamp( fDeltaAngVel, -DOOR_TORQUE_ANGVEL_LIMIT, DOOR_TORQUE_ANGVEL_LIMIT );
		}

		fApplyTorque += fDeltaAngVel * DOOR_TORQUE_ANGVEL_MULT;
	}

	float fHeadingOffset = fCurrentHeading - fOriginalHeading;
	fHeadingOffset = fwAngle::LimitRadianAngleFast(fHeadingOffset);
	float fPreviousHeadingOffset = m_DoorAudioEntity.GetPreviousHeading(); // Should be named GetPreviousHeadingOffset

	const float fHeadingOffsetDelta = fHeadingOffset -  fPreviousHeadingOffset;
	static float fAngVelEpsilon = 0.001f;

	// Reset door latched flag
	if( GetDoorControlFlag( DOOR_CONTROL_FLAGS_LATCHED_SHUT ) )
	{	
		if(fabs( fHeadingOffset ) > CDoor::sm_fLatchResetThreshold)
		{
			SetDoorControlFlag( DOOR_CONTROL_FLAGS_LATCHED_SHUT, false );
		}
	}
	else if( !GetDoorControlFlag( DOOR_CONTROL_FLAGS_STUCK ) && !m_alwaysPush )
	{
		if((Abs(fHeadingOffsetDelta) > 0.f && Abs(fCurrentAngVel) > fAngVelEpsilon))
		{
			if((fHeadingOffset > 0.f && fPreviousHeadingOffset < 0.f) || (fHeadingOffset < 0.f && fPreviousHeadingOffset > 0.f))
			{
				if( !GetDoorControlFlag( DOOR_CONTROL_FLAGS_LATCHED_SHUT ) && m_Tuning && m_Tuning->m_ShouldLatchShut )
				{
					LatchShut();
				}
			}
		}
	}

	m_DoorAudioEntity.ProcessAudio(fCurrentAngVel,fCurrentHeading,fOriginalHeading);

	if(!m_alwaysPush && 
		GetDoorType()==DOOR_TYPE_STD && 
		(m_nDoorShot > 0 || IsNetworkClone()))
	{
		fApplyTorque = 0.0f;

#if __BANK
		if( CDoorSystem::ms_logDoorPhysics &&
			m_DoorSystemData )
		{
			Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
			physicsDisplayf( "CDoor::CalcAndApplyTorqueSTD: Setting torque to 0 (%f, %f, %f) %s\tSpring Active: %d", 
								doorPos.x, doorPos.y, doorPos.z, m_DoorSystemData->GetNetworkObject() ?  m_DoorSystemData->GetNetworkObject()->GetLogName() : "", m_DoorControlFlags.IsClear(DOOR_CONTROL_FLAGS_UNSPRUNG) );
		}
#endif // #if __BANK
	}

	// Typical conditions for waking up the arm are the absolute magnitude of the torque and current angular velocity of the arm
	static dev_float sfTorqueThreshold = 0.01f;
	if( ( !GetDoorControlFlag(DOOR_CONTROL_FLAGS_LATCHED_SHUT) && rage::Abs( fApplyTorque ) >= sfTorqueThreshold ) ||
		m_alwaysPush)
	{
		// We should keep this door awake as long as we exceed a min torque
		static dev_float sfAwakeThreshold = 0.375f;
		const bool bKeepAwake = rage::Abs( fApplyTorque ) >= sfAwakeThreshold;
		if( bKeepAwake ||
			m_alwaysPush)
		{
			ActivatePhysics();
			m_wantToBeAwake = true;
		}

		phCollider* pCollider = GetCollider();
		if( pCollider != NULL )
		{
			pCollider->ApplyTorque(fApplyTorque * GetAngInertia().z * VEC3V_TO_VECTOR3(GetTransform().GetC()), ScalarV(fTimeStep).GetIntrin128ConstRef());

#if __BANK
			if( CDoorSystem::ms_logDoorPhysics &&
				m_DoorSystemData )
			{
				Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
				physicsDisplayf( "CDoor::CalcAndApplyTorqueSTD: Applying torque %f (%f, %f, %f) %s\tSpring Active: %d", 
									fApplyTorque, doorPos.x, doorPos.y, doorPos.z, m_DoorSystemData->GetNetworkObject() ?  m_DoorSystemData->GetNetworkObject()->GetLogName() : "", m_DoorControlFlags.IsClear(DOOR_CONTROL_FLAGS_UNSPRUNG) );
			}
#endif // #if __BANK
		}
	}
#if __BANK
	else
	{
		if( CDoorSystem::ms_logDoorPhysics &&
			m_DoorSystemData )
		{
			Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
			physicsDisplayf( "CDoor::CalcAndApplyTorqueSTD: Not applying torque: %.2f (%f, %f, %f) %s\tSpring Active: %d\tLatched Shut: %d", 
								fApplyTorque, doorPos.x, doorPos.y, doorPos.z, m_DoorSystemData->GetNetworkObject() ?  m_DoorSystemData->GetNetworkObject()->GetLogName() : "", m_DoorControlFlags.IsClear(DOOR_CONTROL_FLAGS_UNSPRUNG), GetDoorControlFlag(DOOR_CONTROL_FLAGS_LATCHED_SHUT) );
		}
	}
#endif // #if __BANK
}

void CDoor::AddPhysics()
{
#if GTA_REPLAY
	//we don't need the door physics in replay so we can skip this
	if( GetOwnedBy() == ENTITY_OWNEDBY_REPLAY )
	{
		// But we do need to call InitDoorPhys because it sets to door type then early's out for replay objects.
		InitDoorPhys(MAT34V_TO_MATRIX34(GetMatrix()));
		return;
	}
#endif

	m_nDoorType = DOOR_TYPE_NOT_A_DOOR;		// reset door type prior to AddPhysics (which will call back into CDoor::InitDoorPhys before it has been added to the level), door type will be calculated in the InitDoorPhys below...  (GTA5: B* 115495)

	CObject::AddPhysics();

	CBaseModelInfo* pModelInfo = GetBaseModelInfo();

	if(GetCurrentPhysicsInst() && pModelInfo->GetUsesDoorPhysics())
	{
#if __BANK
		if (sm_DebugDoorNearLocation)
		{
			Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
			if ((doorPos - sm_DebugDoorPosition).Mag() < sm_DebugDoorRadius)
			{		
				if (sm_DebugBreakOnAddPhysicsOfDoor)
				{
					__debugbreak();
				}
				if (g_PickerManager.IsEnabled() && sm_DebugAddDoorToPickerForAddPhysics)
				{
					bool alreadyAdded = false;
					for (int i = 0; i < g_PickerManager.GetNumberOfEntities(); i++)
					{
						if (g_PickerManager.GetEntity(i) == this)
						{
							alreadyAdded = true;
							break;

						}
					}

					if (!alreadyAdded)
					{
						g_PickerManager.AddEntityToPickerResults(this, false, true);
					}
				}
			}
		}
#endif // __BANK
		InitDoorPhys(MAT34V_TO_MATRIX34(GetMatrix()));

		//Only standard doors get this ... must be done after InitDoorPhys because the type is set in that function.
		if (GetDoorType() == DOOR_TYPE_STD || GetDoorType() == DOOR_TYPE_STD_SC) 
		{ 
			// Setup the damping of the fragType if possible (so cache entries will get teh correct values) 
			phInst* doorInst = GetCurrentPhysicsInst(); 
			fragInst* doorFragInst = IsFragInst(doorInst) ? static_cast<fragInst*>(doorInst) : NULL; 
			float fDoorDamping = ms_fStdDoorDamping;
			if(doorFragInst != NULL) 
			{ 
				fDoorDamping = ms_fStdFragDoorDamping;
				fragPhysicsLOD* physicsLOD = doorFragInst->GetTypePhysics(); 
				physicsLOD->SetDamping(phArchetypeDamp::ANGULAR_V2, Vector3(0.0f, 0.0f, fDoorDamping)); 
			} 

			// But always set up the current archetype as well 
			phArchetype* arch = doorInst->GetArchetype(); 
			if (arch->GetClassType() == phArchetype::ARCHETYPE_DAMP) 
			{ 
				phArchetypeDamp* archdamp = (phArchetypeDamp*)arch; 
				archdamp->ActivateDamping(phArchetypeDamp::ANGULAR_V2, Vector3(0.0f, 0.0f, fDoorDamping)); 
			} 
		}

		m_WasShutForPortalPurposes = true;

		// look up door & set attributes accordingly

		if (m_DoorSystemData && m_DoorSystemData->GetState() != DOORSTATE_INVALID && (pModelInfo->GetHashKey() == (u32)m_DoorSystemData->GetModelInfoHash()))
		{
			if(m_DoorSystemData->GetTargetRatio() != 0.0f)
			{
				const bool	bWasFixed = IsBaseFlagSet( fwEntity::IS_FIXED );
				
				ClearBaseFlag( fwEntity::IS_FIXED );
				SetDoorControlFlag( DOOR_CONTROL_FLAGS_SMOOTH_STATUS_TRANSITION, true );
				SetTargetDoorRatio(m_DoorSystemData->GetTargetRatio(), false );
				
				m_SmoothTransitionPhysicsFlags = CPhysics::GetLevel()->GetInstanceIncludeFlags( GetCurrentPhysicsInst()->GetLevelIndex() );
				CPhysics::GetLevel()->ClearInstanceIncludeFlags( GetCurrentPhysicsInst()->GetLevelIndex(), ~0u );

				m_SmoothTransitionPosHash = CDoorSystem::GenerateHash32( VEC3V_TO_VECTOR3( this->GetTransform().GetPosition() ) );
				
				if (m_DoorSystemData->GetAutomaticRate() != 0.0f)
				{
					m_fAutomaticRate = m_DoorSystemData->GetAutomaticRate();
				}
				if (m_DoorSystemData->GetAutomaticDistance() != 0.0f)
				{
					m_fAutomaticDist = m_DoorSystemData->GetAutomaticDistance();
				}

				UpdateNavigation(bWasFixed);
			}
			else
			{
				EnforceLockedObjectStatus();
			}
			
			if (!m_bUnderCodeControl)
			{
				m_bHoldingOpen = m_DoorSystemData->GetHoldOpen();
			}
		}
		else
		{
			CDoorExtension*		doorExt = fwEntity::GetExtension<CDoorExtension>();
			if ( doorExt )
			{
				if (doorExt->m_enableLimitAngle)
				{
					Assertf(doorExt->m_limitAngle != 0.0f, "Door %s at %.2f, %.2f, %.2f has a door extension with a Limit Angle of 0.0f, this is wrong. "
															"The Limit Angle will now default to DOOR_ROT_LIMIT_STD. This is a bug for Art : Default Interiors.", 
															GetModelName(), GetTransform().GetPosition().GetXf(), 
															GetTransform().GetPosition().GetYf(), 
															GetTransform().GetPosition().GetZf());
					if (doorExt->m_limitAngle == 0.0f)
					{
						const float	DOOR_ROT_LIMIT_STD = (0.9f*HALF_PI);
						doorExt->m_limitAngle = DOOR_ROT_LIMIT_STD;
					}
				}
				AssignBaseFlag( fwEntity::IS_FIXED, doorExt->m_startsLocked );

				// We also need to revert the phInst::FLAG_NEVER_ACTIVATE flag as it was potentially
				phInst* pDoorInst = GetCurrentPhysicsInst(); 
				if( pDoorInst )
				{
					pDoorInst->SetInstFlag( phInst::FLAG_NEVER_ACTIVATE, doorExt->m_startsLocked );
				}

				SetTargetDoorRatio( doorExt->m_doorTargetRatio, true );
			}

			if( GetDoorType() == DOOR_TYPE_GARAGE )
			{
				CGarages::AttachDoorToGarage(this);		// Check whether a garage can use this door.
			}

		}

		// Force an update of m_WasShutForPortalPurposes, and a call to ProcessPortalDisabling().
		UpdatePortalState(true);

		// link door to door system once we have its physics and door type
		CDoorSystem::LinkDoorObject(*this);

		// we must force an update now that door flags are set correctly (dynamic object will have been added in CObject::AddPhysics(), above)
		UpdateNavigation(true);

		if (ms_openDoorsForRacing)
		{
			CDoor::ProcessRaceOpenings(*this, ms_snapOpenDoorsForRacing);
		}

		
		ProcessDoorBehaviour();
		SetOpenIfObstructed();

		fwBaseEntityContainer* pOwner = fwEntity::GetOwnerEntityContainer();
		if (pOwner)
		{
			fwSceneGraphNode* pSceneNode = pOwner->GetOwnerSceneNode();
			if (pSceneNode && pSceneNode->IsTypePortal())
			{
				fwPortalSceneGraphNode *pPortalSceneNode = (fwPortalSceneGraphNode*)pSceneNode;
				fwSceneGraphNode *pPositiveNode = pPortalSceneNode->GetPositivePortalEnd();
				fwSceneGraphNode *pNegativeNode = pPortalSceneNode->GetPositivePortalEnd();
				if ((pPositiveNode && pPositiveNode->IsTypeExterior()) || 
					(pNegativeNode && pNegativeNode->IsTypeExterior()))
				{
					m_bAttachedToARoom0Portal = true;
				}
			}
		}
	}
}



// don't need this now that we're using constraints
// because the constraints can't drift like the constrained collider did
//
// e1 correction - doors were moving far enough when hit by a van that the door status
// was finding the wrong door status instance, so reimplimenting this to just stop the doors
// entity position from changing if the constraints can't hold it in place (no need to do anything for the physics)
void CDoor::UpdateEntityFromPhysics(phInst *pInst, int nLoop)
{
	if(m_nDoorType > DOOR_TYPE_NOT_A_DOOR)
	{
		Vector3 vecOriginalPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		CPhysical::UpdateEntityFromPhysics(pInst, nLoop);

		if(m_nDoorType!=DOOR_TYPE_SLIDING_HORIZONTAL && m_nDoorType!=DOOR_TYPE_SLIDING_HORIZONTAL_SC
				&& m_nDoorType!=DOOR_TYPE_SLIDING_VERTICAL && m_nDoorType!=DOOR_TYPE_SLIDING_VERTICAL_SC
				&& m_nDoorType!=DOOR_TYPE_GARAGE && m_nDoorType!=DOOR_TYPE_GARAGE_SC)
		{
			// deliberately want to call the CDynamicEntity version of SetPosition here, only want to reset the entity's m_pMat->d
			// and the movedSinceLastPrerender flag and nothing else!
			CDynamicEntity::SetPosition(vecOriginalPosition, true, true, true);

			// old version from when the physics needed to be reset as well
			//			Matrix34 matDoor = GetMatrix();
			//			matDoor.d = vecOriginalPosition;
			//
			//			// force position of the door back to where it should be
			//			// (ms_bUpdatingDoor is a hacky way of letting it know we don't want to reset the door limits)
			//			ms_bUpdatingDoorRatio = true;
			//			SetMatrix(matDoor);
			//			ms_bUpdatingDoorRatio = false;
			///////////////
		}

		// The physics system may have moved the door, consider notifying the portal system.
		UpdatePortalState();
	}
	else
	{
		CObject::UpdateEntityFromPhysics(pInst, nLoop);
	}
}

#if __BANK

namespace object_commands
{
	void CommandDoorSystemSetState(int doorEnumHash, int doorState);
	void CommandAddDoorToSystem(int doorEnumHash, int hashKey, const scrVector &position, bool useOldOverrides, bool network, bool permanent);
}

void CDoor::DebugLockSelectedDoor(bool lock, bool force)
{
	bool backupScriptOverride = sm_IngnoreScriptSetDoorStateCommands;

	DoorScriptState state;
	if (lock)
	{
		if (force)
		{
			state = DOORSTATE_FORCE_LOCKED_THIS_FRAME;
		}
		else
		{
			state = DOORSTATE_LOCKED;
		}
	}
	else
	{
		if (force)
		{
			state = DOORSTATE_FORCE_UNLOCKED_THIS_FRAME;
		}
		else
		{
			state = DOORSTATE_UNLOCKED;
		}
	}

	sm_IngnoreScriptSetDoorStateCommands = false;
	bool foundDoor = false;
	for (CDoorSystem::DoorSystemMap::Iterator doorIterator = CDoorSystem::GetDoorMap().CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
	{
		CDoorSystemData *pDoorData = doorIterator.GetData();

		fwEntity *pEntity = g_PickerManager.GetSelectedEntity();

		if (pEntity && pEntity == pDoorData->GetDoor())
		{
			object_commands::CommandDoorSystemSetState(pDoorData->GetEnumHash(), state, FALSE, TRUE);
			foundDoor = true;
			break;
		}
	}
	sm_IngnoreScriptSetDoorStateCommands = backupScriptOverride;

	if (!foundDoor)
	{
		Displayf("Selected entity was not found in Door System");
	}

}

void OpenAllBarriersForRaceWithSnapOption()
{
	CDoor::OpenAllBarriersForRace(CDoor::sm_SnapOpenBarriersForRaceToggle);
};

namespace object_commands
{
	void CommandDoorSystemSetOpenRatio(int doorEnumHash, float openRatio, bool network, bool flushState);
}

bool gFlushOpenRatio = false;
void CDoor::DebugSetOpenRatio(float ratio)
{
	bool backupScriptOverride = CDoor::sm_IngnoreScriptSetDoorStateCommands;

	sm_IngnoreScriptSetDoorStateCommands = false;
	bool foundDoor = false;
	for (CDoorSystem::DoorSystemMap::Iterator doorIterator = CDoorSystem::GetDoorMap().CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
	{
		CDoorSystemData *pDoorData = doorIterator.GetData();

		fwEntity *pEntity = g_PickerManager.GetSelectedEntity();

		if (pEntity && pEntity == pDoorData->GetDoor())
		{
			object_commands::CommandDoorSystemSetOpenRatio(pDoorData->GetEnumHash(), ratio, FALSE, gFlushOpenRatio);
			foundDoor = true;
			break;
		}
	}
	sm_IngnoreScriptSetDoorStateCommands = backupScriptOverride;

	if (!foundDoor)
	{
		Displayf("Selected entity was not found in Door System");
	}
}

void CDoor::SetOpenRatioCallBack() { DebugSetOpenRatio(CDoor::sm_ForcedTargetRatio); }

void CDoor::AddClassWidgets(bkBank& bank)
{
	bank.AddToggle("Log door physics", &CDoorSystem::ms_logDoorPhysics);

	bank.AddToggle("Force All Auto-open to Open", &sm_ForceAllAutoOpenToOpen);
	bank.AddToggle("Force All Auto-open to Close", &sm_ForceAllAutoOpenToClose);
	bank.AddToggle("Force All to Break", &sm_ForceAllToBreak);
	bank.AddToggle("Force All Doors' Target Ratio", &sm_ForceAllTargetRatio);
	bank.AddButton("Lock All Doors", datCallback(CFA(CDoor::LockAllDoors)));
	bank.AddButton("Unlock All Doors", datCallback(CFA(CDoor::UnlockAllDoors)));
	bank.AddSlider("Target Ratio", &sm_ForcedTargetRatio, -1.0f, 1.0f, 0.05f);
	bank.AddToggle("Visualize Auto Open Bounds", &CDebugScene::sm_VisualizeAutoOpenBounds);

	bank.AddButton("Lock Selected Door", datCallback(CFA(CDoor::LockSelectedDoor)));
	bank.AddButton("Unlock Selected Door", datCallback(CFA(CDoor::UnlockSelectedDoor)));

	bank.AddButton("Force Lock Selected Door", datCallback(CFA(CDoor::ForceLockSelectedDoor)));
	bank.AddButton("Force Unlock Selected Door", datCallback(CFA(CDoor::ForceUnlockSelectedDoor)));

	bank.AddButton("Set Open Ratio", datCallback(CFA(CDoor::SetOpenRatioCallBack)));
	bank.AddToggle("Flush Calls to Set Open Ratio", &gFlushOpenRatio);


	bank.AddToggle("Override Script calls to DOOR_SYSTEM_SET_DOOR_STATE", &sm_IngnoreScriptSetDoorStateCommands);
	bank.AddToggle("Break in ProcessControlLogic() for selected door", &sm_ProcessControlLogic_BreakOnSelectedDoor);
	bank.AddToggle("Break in ProcessPhysics() for selected door", &sm_ProcessPhysics_BreakOnSelectedDoor);
	bank.AddToggle("Break in SetState for selected door", &sm_breakOnSelectedDoorSetState);
	bank.AddToggle("Break in SetFixedPhysics for selected door", &sm_breakOnSelectedDoorSetFixedPhysics);

	bank.AddToggle("Enable Hack Garage Shut", &sm_enableHackGarageShut);

	bank.AddToggle("Use \"Snap Open\" for Open All Barriers For Race", &sm_SnapOpenBarriersForRaceToggle);

	bank.AddButton("Open All Barriers For Race", datCallback(CFA(OpenAllBarriersForRaceWithSnapOption)));
	bank.AddButton("Close All Barriers Opened For Race", datCallback(CFA(CDoor::CloseAllBarriersOpenedForRace)));

	bank.AddText("Unique Hashed Name for Door", sm_AddDoorString, sizeof(sm_AddDoorString));
	bank.AddButton("Add Selected to Door System", datCallback(CFA(CDoor::AddSelectedToDoorSystem)));

	bank.AddButton("Set Selected Door to Open for Races", datCallback(CFA(CDoor::SetSelectedDoorToOpenForRaces)));
	bank.AddButton("Set Selected Door to Not Open for Races", datCallback(CFA(CDoor::SetSelectedDoorToNotOpenForRaces)));

	bank.AddSlider("Std Door Damping", &ms_fStdDoorDamping, 0.0f, 3.0f, 0.00001f);
	bank.AddSlider("Std Frag Door Damping", &ms_fStdFragDoorDamping, 0.0f, 3.0f, 0.00001f);

	bank.AddSeparator();
	bank.AddToggle("Log door commands called for door hash", &sm_logStateChangesForSelectedDoor);
	bank.AddText("Door Hash String", sm_debugDooorSystemEnumString, sizeof(sm_debugDooorSystemEnumString), datCallback(CFA(SetDoorDebugStringHash)));
	bank.AddText("Door Hash", sm_debugDooorSystemEnumNumber, sizeof(sm_debugDooorSystemEnumNumber), true);
}


void CDoor::SetLockedStateOfAllDoors(bool locked)
{
	const int maxDoors = (int) CObject::GetPool()->GetSize();
	for(int i = 0; i < maxDoors; i++)
	{
		CObject* pObj = CObject::GetPool()->GetSlot(i);
		if(!pObj || !pObj->IsADoor())
		{
			continue;
		}
		CDoor* pDoor = static_cast<CDoor*>(pObj);

		pDoor->ScriptDoorControl(locked, pDoor->GetTargetDoorRatio());
	}
}

void CDoor::AddSelectedToDoorSystem()
{
	fwEntity *pEntity = g_PickerManager.GetSelectedEntity();
	if (pEntity)
	{
		u32 hash = atStringHash(sm_AddDoorString);
		if (pEntity && hash)
		{
			for (CDoorSystem::DoorSystemMap::Iterator doorIterator = CDoorSystem::GetDoorMap().CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
			{
				CDoorSystemData *pDoorData = doorIterator.GetData();
				if (pEntity && pEntity == pDoorData->GetDoor())
				{
					Displayf("Door already exists in system.");
					return;
				}
				if ((u32)pDoorData->GetEnumHash() == hash)
				{
					Displayf("Hash has already been used.");
					return;
				}
			}
		}

		object_commands::CommandAddDoorToSystem(hash, pEntity->GetArchetype()->GetModelNameHash(), pEntity->GetTransform().GetPosition(), false, true, false);
	}
}

void CDoor::SetSelectedDoorToOpenForRaces()
{
	bool foundDoor = false;
	for (CDoorSystem::DoorSystemMap::Iterator doorIterator = CDoorSystem::GetDoorMap().CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
	{
		CDoorSystemData *pDoorData = doorIterator.GetData();

		fwEntity *pEntity = g_PickerManager.GetSelectedEntity();

		if (pEntity && pEntity == pDoorData->GetDoor())
		{
			pDoorData->SetOpenForRaces(true);
			if (ms_openDoorsForRacing)
			{
				if (pDoorData->GetDoor())
				{
					pDoorData->GetDoor()->OpenDoor();
				}
			}
			foundDoor = true;
			break;
		}
	}

	if (!foundDoor)
	{
		Displayf("Selected entity was not found in Door System");
	}

}

void CDoor::SetSelectedDoorToNotOpenForRaces()
{
	bool foundDoor = false;
	for (CDoorSystem::DoorSystemMap::Iterator doorIterator = CDoorSystem::GetDoorMap().CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
	{
		CDoorSystemData *pDoorData = doorIterator.GetData();

		fwEntity *pEntity = g_PickerManager.GetSelectedEntity();

		if (pEntity && pEntity == pDoorData->GetDoor())
		{
			pDoorData->SetOpenForRaces(false);
			if (ms_openDoorsForRacing)
			{
				if (pDoorData->GetDoor())
				{
					pDoorData->GetDoor()->CloseDoor();
					if (ms_snapOpenDoorsForRacing)
					{
						pDoorData->GetDoor()->SetFixedPhysics(false, false);
					}
				}
			}
			foundDoor = true;
			break;
		}
	}

	if (!foundDoor)
	{
		Displayf("Selected entity was not found in Door System");
	}
}

#endif	// __BANK

float CDoor::GetLimitAngle() const
{
	const CDoorExtension*	doorExt = fwEntity::GetExtension<CDoorExtension>();
	if ( doorExt && doorExt->m_enableLimitAngle )
		return doorExt->m_limitAngle;

	//Tuning setting override
	if (m_Tuning && m_Tuning->m_RotationLimitAngle != 0.0f)
		return DtoR*m_Tuning->m_RotationLimitAngle;
	
	const float				DOOR_ROT_LIMIT_STD = (0.9f*HALF_PI);
	const float				DOOR_ROT_LIMIT_ARM = (0.999f*HALF_PI);
	const float				DOOR_ROT_LIMIT_GARAGE = (-0.99f*HALF_PI);

	if( GetDoorType() == DOOR_TYPE_STD || GetDoorType() == DOOR_TYPE_STD_SC )
		return DOOR_ROT_LIMIT_STD;
	else if( GetDoorType() == DOOR_TYPE_BARRIER_ARM || GetDoorType() == DOOR_TYPE_BARRIER_ARM_SC || 
		GetDoorType() == DOOR_TYPE_RAIL_CROSSING_BARRIER || GetDoorType() == DOOR_TYPE_RAIL_CROSSING_BARRIER_SC)
		return DOOR_ROT_LIMIT_ARM;
	else if( GetDoorType() == DOOR_TYPE_GARAGE || GetDoorType() == DOOR_TYPE_GARAGE_SC )
		return DOOR_ROT_LIMIT_GARAGE;

	return FLT_MAX;
}

bool CDoor::CalcOpenFullyOpen(Matrix34& matFullyOpen) const
{
	float fOriginalHeading = m_qDoorStartRot.TwistAngle(ZAXIS);
	float fCurrentHeading = fwAngle::LimitRadianAngleFast(GetTransform().GetHeading() - fOriginalHeading);
	float fLimitAngle = GetLimitAngle() * 0.97f;	// Not fully open :(
	float fDesiredHeading = 0.f;

	if (fCurrentHeading > 0.f)
	{
		fDesiredHeading = fOriginalHeading + fLimitAngle;
	}
	else if (fCurrentHeading <= 0.f)
	{
		fDesiredHeading = fOriginalHeading - fLimitAngle;
	}

	Mat33V rot;
	Mat33VFromZAxisAngle(rot, ScalarVFromF32(fDesiredHeading));

	Mat34V matrix = GetMatrix();
	matrix.Set3x3(rot);
	matFullyOpen = MAT34V_TO_MATRIX34(matrix);
	return true;
}

void CDoor::UpdatePhysicsFromEntity(bool bWarp)
{
	CObject::UpdatePhysicsFromEntity(bWarp);

	if (GetAnimDirector())
	{
		fwAnimDirectorComponentSyncedScene* pComponent = static_cast<fwAnimDirectorComponentSyncedScene*>(GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeSyncedScene));
		if (pComponent && pComponent->IsPlayingSyncedScene())
		{
			UpdatePortalState();
			return;
		}
	}

	if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
	{
		return;
	}
	// door physics needs to get re-initialised if the door gets moved/placed
	if( GetDoorType() > DOOR_TYPE_NOT_A_DOOR && !ms_bUpdatingDoorRatio )
	{
		InitDoorPhys(MAT34V_TO_MATRIX34(GetMatrix()));
	}
}


// PURPOSE:	Helper functions and data structures for detecting double doors.
namespace DoubleDoorHelperFuncs
{
	// PURPOSE:	Data used during the search for double doors.
	struct SearchDataStruct
	{
		Vec3V		m_CoordToCalculateDistanceFrom;
		float		m_ClosestDistanceSquared;
		CDoor*		m_ClosestDoor;
		CDoor*		m_SearchingDoor;
	};

	// PURPOSE:	Helper function to check if the door opens along the positive or negative
	//			X axis, based on its bounding box.
	bool IsDoorDirPositive(const CDoor& door)
	{
		return door.GetBoundingBoxMax().x > -door.GetBoundingBoxMin().x;
	}

	// PURPOSE:	Callback function used for double door detection.
	bool CheckClosestDoubleDoorCB(CEntity* pEntity, void* data)
	{
		Assert(pEntity);
		SearchDataStruct* pClosestObjectData = reinterpret_cast<SearchDataStruct*>(data);

		// First, make sure we haven't found ourselves.
		if(pEntity != pClosestObjectData->m_SearchingDoor)
		{
			// Check if we found anything else than a door.
			if(!pEntity->GetIsTypeObject() || !((CObject*)pEntity)->IsADoor())
			{
				return true;
			}

			// Check if this other door could be used as the other half of a double door.
			CDoor* pDoor = static_cast<CDoor*>(pEntity);
			if(!pClosestObjectData->m_SearchingDoor->CheckIfCanBeDoubleWithOtherDoor(*pDoor))
			{
				// No, can't use it.
				return true;
			}

			// Compute the squared distance between the doors.
			const Vec3V pos1 = pEntity->GetTransform().GetPosition();
			const Vec3V pos2 = pClosestObjectData->m_CoordToCalculateDistanceFrom;
			const ScalarV distSqV = DistSquared(pos1, pos2);

			// Check if this is closer than any other matching object we've found.
			const float distSq = distSqV.Getf();
			if(distSq < pClosestObjectData->m_ClosestDistanceSquared)
			{
				pClosestObjectData->m_ClosestDoor = pDoor;
				pClosestObjectData->m_ClosestDistanceSquared = distSq;
			}
		}

		return true;
	}

}	// namespace CDoubleDoorHelperFuncs


bool CDoor::CheckIfCanBeDoubleWithOtherDoor(const CDoor& otherDoor) const
{
	// Check if the door type matches - sliding doors have to match up with
	// sliding doors, etc.
	if(otherDoor.GetDoorType() != m_nDoorType)
	{
		return false;
	}

	// Check if the other door already has the double door pointer set, and pointing
	// to some other door. If so, we can't use it.
	if(otherDoor.m_OtherDoorIfDouble && otherDoor.m_OtherDoorIfDouble != this)
	{
		return false;
	}

	// Compute the cosine of the half angle between the door orientations.
	const float cosHalfAngle = m_qDoorStartRot.Dot(otherDoor.m_qDoorStartRot);

	// Check the angle.
	bool thisDoorIsPositive = DoubleDoorHelperFuncs::IsDoorDirPositive(*this);
	bool otherDoorIsPositive = DoubleDoorHelperFuncs::IsDoorDirPositive(otherDoor);
	if(thisDoorIsPositive == otherDoorIsPositive)
	{
		// Both door models have the same direction in terms of which
		// direction they open in relative to their matrix. For the doors to be a pair,
		// they need to be rotated 180 degrees, i.e. the cosine of the half angle needs to be close to 0.
		const float threshold = 0.0872f;	// MAGIC! This is cos(5 degrees).
		if(cosHalfAngle > threshold)
		{
			return false;		// Not the right direction for a double door pair.
		}
	}
	else
	{
		// In this case, the models themselves are opposite, so the matrices of the
		// placed doors need to be in the same direction. This means the angle is close to 0,
		// and the cosine of the half angle is close to 1.
		const float threshold = 0.996f;		// MAGIC! This is cos(85 degrees)
		if(cosHalfAngle < threshold)
		{
			return false;		// Not the right direction for a double door pair.
		}
	}

	// Note: if needed, we could add additional conditions here: for example, the distance
	// between the (original) door positions is expected to be the sum of the bounding box
	// widths, and the origin of each door should be on the other door's X axis.

	return true;
}


void CDoor::CalcAutoOpenInfo(Vec3V_Ref testCenterOut, float& testRadiusOut) const
{
	//Get the untransformed offset
	Vec3V vecBoundCentre;
	phInst* pInst = GetFragInst();
	if(pInst==NULL)
		pInst = GetCurrentPhysicsInst();

	if(pInst)
	{
		vecBoundCentre = pInst->GetArchetype()->GetBound()->GetCentroidOffset();
	}
	else
	{
		vecBoundCentre = VECTOR3_TO_VEC3V(GetBaseModelInfo()->GetBoundingSphereOffset());
	}

	if(m_Tuning)
	{
		vecBoundCentre = rage::Add(vecBoundCentre, m_Tuning->m_AutoOpenVolumeOffset);
	}

	// Get the original position and orientation of the door.
	const Vec3V origPosV = RCC_VEC3V(m_vecDoorStartPos);
	Mat33V orientationMtrx;
	Mat33VFromQuatV(orientationMtrx, QUATERNION_TO_QUATV(m_qDoorStartRot));

	testCenterOut = Multiply(orientationMtrx, vecBoundCentre) + origPosV;

	//////////////////////////////////////////////////////////////////////////
	float fBoundRadius;
	if(m_fAutomaticDist != 0.0f)
	{
		fBoundRadius = m_fAutomaticDist;
	}
	else
	{
		fBoundRadius = GetBoundingBoxMax().x - GetBoundingBoxMin().x;

		if(m_Tuning)
		{
			fBoundRadius *= m_Tuning->m_AutoOpenRadiusModifier;
		}
	}

	testRadiusOut = fBoundRadius;
}


void CDoor::CalcAutoOpenInfoBox(spdAABB& rLocalBBox, Mat34V_Ref posMatOut) const
{
	//Get the untransformed offset
	Vec3V vecBoundCentre;
	phInst* pInst = GetFragInst();
	if(pInst==NULL)
		pInst = GetCurrentPhysicsInst();

	if(pInst)
	{
		vecBoundCentre = pInst->GetArchetype()->GetBound()->GetCentroidOffset();
	}
	else
	{
		vecBoundCentre = VECTOR3_TO_VEC3V(GetBaseModelInfo()->GetBoundingSphereOffset());
	}

	if(m_Tuning)
	{
		vecBoundCentre = rage::Add(vecBoundCentre, m_Tuning->m_AutoOpenVolumeOffset);
	}

	// Get the original position and orientation of the door.
	const Vec3V origPosV = RCC_VEC3V(m_vecDoorStartPos);
	Mat33V orientationMtrx;
	Mat33VFromQuatV(orientationMtrx, QUATERNION_TO_QUATV(m_qDoorStartRot));
	posMatOut.Set3x3(orientationMtrx);
	posMatOut.SetCol3(Multiply(orientationMtrx, vecBoundCentre) + origPosV);

	const CDoorTuning* tuning = GetTuning();
	if (tuning && tuning->m_CustomTriggerBox)
	{
		rLocalBBox.SetMin( tuning->m_TriggerBoxMinMax.GetMin() );
		rLocalBBox.SetMax( tuning->m_TriggerBoxMinMax.GetMax() );
	}
	else
	{
		//////////////////////////////////////////////////////////////////////////
		//Find the largest side
		float dif = m_fAutomaticDist;
		float multiplier = 1.0f;

		if (m_fAutomaticDist == 0.0f)
		{
			dif = Max(GetBoundingBoxMax().x - GetBoundingBoxMin().x, GetBoundingBoxMax().y - GetBoundingBoxMin().y);
			dif = Max(dif, GetBoundingBoxMax().z - GetBoundingBoxMin().z);

			if (m_Tuning)
			{
				multiplier = m_Tuning->m_AutoOpenRadiusModifier;
			}
		}		

		Vec3V vBoxEntent( V_ZERO );
		vBoxEntent.SetX( GetBoundingBoxMin().x );
		vBoxEntent.SetY( -dif * multiplier );
		vBoxEntent.SetZ( GetBoundingBoxMin().x );
		rLocalBBox.SetMin( vBoxEntent );

		vBoxEntent.SetX( GetBoundingBoxMax().x );
		vBoxEntent.SetY( dif * multiplier );
		vBoxEntent.SetZ( GetBoundingBoxMax().x );
		rLocalBBox.SetMax( vBoxEntent );
	}
}

bool CDoor::EntityCausesAutoDoorOpen( const CEntity* entity, const fwInteriorLocation doorRoom1Loc, const fwInteriorLocation doorRoom2Loc, fwFlags32 doorFlags ) const
{
	bool shouldOpen = false;

	//checking to see if you have a entity alone is no longer enough as cars/bikes/etc are considered
	// entities ... what we truly care about is entities that are peds or entities that contain peds (ie cars)
	bool bHasPed = false;
	if (entity->GetIsTypeVehicle())
	{
		if( doorFlags.IsFlagSet(CDoorTuning::AutoOpensForAllVehicles) )
			return true;

		const CVehicle* vehicle = (const CVehicle*)entity;

		//if the vehicle has any peds in it 
		if (vehicle->GetDriver() || vehicle->GetNumberOfPassenger())
		{
			const CSeatManager& seatMan = *vehicle->GetSeatManager();
			const s32 seats = seatMan.GetMaxSeats();
			for (s32 s = 0; s < seats; s++)
			{
				const CPed* ped = seatMan.GetPedInSeat(s);
				if (ped)
				{
					if( PedTriggersAutoOpen(ped, doorFlags, true) )
					{
						bHasPed = true;
						break;
					}
				}
			}
		}
	}
	else if (entity->GetIsTypePed())
	{
		const CPed* ped = (const CPed*)entity;
		bHasPed = PedTriggersAutoOpen(ped, doorFlags, false);
	}

	if (bHasPed) 
	{
		const fwInteriorLocation entityIntLoc = entity->GetInteriorLocation();
		if ( !entityIntLoc.IsValid() || entityIntLoc.IsSameLocation( doorRoom1Loc ) || entityIntLoc.IsSameLocation( doorRoom2Loc ) )
		{
			shouldOpen = true;
		}

		// bug 4048536 : special case for underground base - portal attached doors in entity sets aren't correctly coming through export with the portal attached flag set
		CInteriorProxy* pProxy = CInteriorProxy::GetFromLocation(entityIntLoc);
		if (pProxy)
		{
			if ( (pProxy->GetNameHash() == ATSTRINGHASH("xm_x17dlc_int_02", 0x2cf063ac)) )
			{
				shouldOpen = true;
			}
		}
	}	
	
	// Special case for dynamic objects that are blocking garage door spaces
	static dev_float sfIntersectionMinRatio = 0.15f;
	if( !shouldOpen && entity->GetIsTypeVehicle() && GetDoorType() == DOOR_TYPE_GARAGE && Abs( GetDoorOpenRatio() ) > sfIntersectionMinRatio )
	{
		// Set up a matrix containing the original door position and orientation
		Mat34V matOriginalDoor;
		Mat34VFromQuatV( matOriginalDoor, QUATERNION_TO_QUATV( m_qDoorStartRot ) );
		matOriginalDoor.SetCol3( VECTOR3_TO_VEC3V( m_vecDoorStartPos ) );

		// Create an oriented world AABB from the original door info
		spdAABB tempBB;
		const spdAABB& localBB = GetLocalSpaceBoundBox( tempBB );
		spdOrientedBB originalWorldDoorBBox;
		originalWorldDoorBBox.Set( localBB, matOriginalDoor );

		// Compare the dynamic entity oriented world AABB vs the original door oriented world AABB
		const CDynamicEntity* pDynamic = static_cast<const CDynamicEntity*>( entity );
		spdOrientedBB orientedWorldEntityBBox( pDynamic->GetLocalSpaceBoundBox( tempBB ), pDynamic->GetMatrix() );
		if( originalWorldDoorBBox.IntersectsOrientedBB( orientedWorldEntityBBox ) )
		{
			shouldOpen = true;
		}
	}

	return shouldOpen;
}

bool CDoor::PedTriggersAutoOpen( const CPed* pPed, fwFlags32 doorFlags, bool bInVehicle ) const
{
	if( pPed->GetIgnoredByAutoOpenDoors() )
	{
		return false;
	}	

	if( doorFlags.IsFlagSet(CDoorTuning::AutoOpensForLawEnforcement) && pPed->IsLawEnforcementPed())
	{
		return true;
	}

	if( NetworkInterface::IsGameInProgress() )
	{
		if( doorFlags.IsFlagSet(CDoorTuning::AutoOpensForMPPlayerPedsOnly) && !pPed->IsPlayer()  )
		{
			return false;
		}
		

		if( doorFlags.IsFlagSet(CDoorTuning::AutoOpensForMPVehicleWithPedsOnly) && !bInVehicle )
		{
			return false;
		}
	}
	else
	{
		if( doorFlags.IsFlagSet(CDoorTuning::AutoOpensForSPPlayerPedsOnly) && !pPed->IsPlayer()  )
		{
			return false;
		}


		if( doorFlags.IsFlagSet(CDoorTuning::AutoOpensForSPVehicleWithPedsOnly) && !bInVehicle )
		{
			return false;
		}
	}

	return true;
}

void CDoor::SetAlwaysPush( bool alwaysPush ) 
{ 
	m_alwaysPush = alwaysPush; 

	if( alwaysPush )
	{
		// HACK - FIX GTAV b*2227094 - make sure the door isn't latched shut when we're force it open
		SetDoorControlFlag( DOOR_CONTROL_FLAGS_LATCHED_SHUT, false );
		SetShouldUseKinematicPhysics( true );
	}
	else
	{
		SetShouldUseKinematicPhysics( false );
	}
}

bool CDoor::DoorTypeAutoOpens(int doorType)
{
	return doorType == DOOR_TYPE_SLIDING_HORIZONTAL
			|| doorType == DOOR_TYPE_SLIDING_VERTICAL
			|| doorType == DOOR_TYPE_GARAGE
			|| doorType == DOOR_TYPE_BARRIER_ARM;
}

bool CDoor::DoorTypeAutoOpensForVehicles(int doorType)
{
	return doorType == DOOR_TYPE_SLIDING_HORIZONTAL
		|| doorType == DOOR_TYPE_SLIDING_VERTICAL
		|| doorType == DOOR_TYPE_GARAGE
		|| doorType == DOOR_TYPE_BARRIER_ARM
		|| doorType == DOOR_TYPE_SLIDING_HORIZONTAL_SC
		|| doorType == DOOR_TYPE_SLIDING_VERTICAL_SC
		|| doorType == DOOR_TYPE_GARAGE_SC
		|| doorType == DOOR_TYPE_BARRIER_ARM_SC
		|| doorType == DOOR_TYPE_STD
		|| doorType == DOOR_TYPE_STD_SC;
}

bool CDoor::DoorTypeOpensForPedsWhenNotLocked(int doorType)
{
	return doorType == DOOR_TYPE_STD ||
			doorType == DOOR_TYPE_SLIDING_HORIZONTAL ||
			doorType == DOOR_TYPE_SLIDING_VERTICAL ||
			doorType == DOOR_TYPE_GARAGE ||
			doorType == DOOR_TYPE_STD_SC ||
			doorType == DOOR_TYPE_SLIDING_HORIZONTAL_SC ||
			doorType == DOOR_TYPE_SLIDING_VERTICAL_SC ||
			doorType == DOOR_TYPE_GARAGE_SC;
}

void CDoor::CheckForDoubleDoor()
{
	// Set the flag that we have considered this door. It's up to the caller to
	// check it before calling this function, if desired.
	m_DoorControlFlags.Set(DOOR_CONTROL_FLAGS_CHECKED_FOR_DOUBLE, true);

	// For now, we only allow sliding doors to be detected as double doors.
	// Most of the code below would probably work as is for rotating doors
	// as well, but no need for that yet.
	if(m_nDoorType != DOOR_TYPE_SLIDING_HORIZONTAL)
	{
		ClearDoubleDoor();
		return;
	}

	// Get the original position and orientation of the door.
	const Vec3V origPosV = RCC_VEC3V(m_vecDoorStartPos);
	Mat34V orientationMtrx;
	Mat34VFromQuatV(orientationMtrx, QUATERNION_TO_QUATV(m_qDoorStartRot));

	// Get the direction of the local X axis.
	const Vec3V sideDirBeforeFlipV = orientationMtrx.GetCol0();

	// Get bounding box info.
	const ScalarV bndBoxNegMinXV = Negate(RCC_VEC3V(GetBoundingBoxMin()).GetX());
	const ScalarV bndBoxMaxXV = RCC_VEC3V(GetBoundingBoxMax()).GetX();

	// Compute the direction towards the other potential door, depending on
	// if this door opens in the positive or negative direction.
	Vec3V sideDirV;
	if(DoubleDoorHelperFuncs::IsDoorDirPositive(*this))
	{
		sideDirV = sideDirBeforeFlipV;
	}
	else
	{
		sideDirV = Negate(sideDirBeforeFlipV);
	}

	// Compute the width of the door, and the position where the other door
	// is expected to have its origin, if it exists.
	const ScalarV doorWidthV = Max(bndBoxNegMinXV, bndBoxMaxXV);
	const Vec3V centerPosV = AddScaled(origPosV, sideDirV, doorWidthV);
	const Vec3V otherPosV = AddScaled(centerPosV, sideDirV, doorWidthV);

	// Initialize search data.
	DoubleDoorHelperFuncs::SearchDataStruct closestObjectData;
	closestObjectData.m_CoordToCalculateDistanceFrom = otherPosV;
	closestObjectData.m_ClosestDistanceSquared = LARGE_FLOAT;
	closestObjectData.m_ClosestDoor = NULL;
	closestObjectData.m_SearchingDoor = this;

	// Do a search for the other door, in a sphere centered at its expected origin,
	// with a radius matching the width of the door.
	spdSphere testSphere(closestObjectData.m_CoordToCalculateDistanceFrom, doorWidthV);
	fwIsSphereIntersecting searchSphere(testSphere);
	CGameWorld::ForAllEntitiesIntersecting(&searchSphere, DoubleDoorHelperFuncs::CheckClosestDoubleDoorCB, (void*)&closestObjectData, 
			(ENTITY_TYPE_MASK_OBJECT), (SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS),
			SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_DEFAULT);

	// Check if we found something.
	CDoor* pOtherDoor = closestObjectData.m_ClosestDoor;
	if(pOtherDoor)
	{
		// Link the doors up in both directions, so that the other door doesn't also
		// have to perform tests.
		m_OtherDoorIfDouble = pOtherDoor;
		Assert(!pOtherDoor->m_OtherDoorIfDouble || pOtherDoor->m_OtherDoorIfDouble == this);
		pOtherDoor->m_OtherDoorIfDouble = this;
		pOtherDoor->SetDoorControlFlag(DOOR_CONTROL_FLAGS_CHECKED_FOR_DOUBLE, true);
	}
	else
	{
		// If we had any sort of reference to a double door before, make sure it gets cleared,
		// as apparently it's no longer relevant.
		ClearDoubleDoor();
	}
}


void CDoor::ClearDoubleDoor()
{
	CDoor* pOtherDoor = m_OtherDoorIfDouble;
	if(!pOtherDoor)
	{
		return;
	}

	if(Verifyf(pOtherDoor->m_OtherDoorIfDouble == this, "Expected linked double door."))
	{
		pOtherDoor->SetDoorControlFlag(DOOR_CONTROL_FLAGS_CHECKED_FOR_DOUBLE, false);
		pOtherDoor->m_OtherDoorIfDouble = NULL;
	}
	m_OtherDoorIfDouble = NULL;
}

void CDoor::LatchShut()
{
	SetDoorControlFlag( DOOR_CONTROL_FLAGS_LATCHED_SHUT, true );
	SetTargetDoorRatio(0.f,true);
	SetAngVelocity(Vector3(GetAngVelocity().x,GetAngVelocity().y,0.f));

#if __BANK
	if( m_DoorSystemData &&
		CDoorSystem::ms_logDoorPhysics )
	{
		Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
		physicsDisplayf( "CDoor::LatchShut: (%f, %f, %f) %s", 
							doorPos.x, doorPos.y, doorPos.z, m_DoorSystemData->GetNetworkObject() ?  m_DoorSystemData->GetNetworkObject()->GetLogName() : "" );
	}
#endif // #if __BANK
}

dev_float SLIDING_DOOR_MASS_INV_SCALE_AGAINST_PED = 0.5f;

void CDoor::ProcessPreComputeImpacts(phContactIterator impacts)
{
	CObject::ProcessPreComputeImpacts(impacts);

	//Process the impacts.
	impacts.ResetToFirstActiveContact();
	while(!impacts.AtEnd())
	{
		//Ensure the impact is not a constraint.
		if(impacts.IsConstraint() || impacts.IsDisabled())
		{
			impacts.NextActiveContact();
			continue;
		}

		phInst* pOtherInstance = impacts.GetOtherInstance();
		CEntity* pOtherEntity = CPhysics::GetEntityFromInst(pOtherInstance);

		if(pOtherEntity != NULL)
		{
			if( m_alwaysPush )
			{
				impacts.SetMassInvScales( 0.0f, 1.0f );
			}

			// B*1991845: Allow certain doors to be flagged as being able to push vehicles out of the way with ease. Should be used sparingly.
			if(!CPhysics::GetLevel()->IsFixed(pOtherInstance->GetLevelIndex()))
			{
				if(pOtherEntity->GetIsTypeVehicle())
				{
					if(m_alwaysPushVehicles)
					{
						impacts.SetMassInvScales(0.1f, 2.0f);
					}			
				}
			}

			if(pOtherEntity->GetIsTypePed() && !CPhysics::GetLevel()->IsFixed(pOtherInstance->GetLevelIndex()))
			{
				if(pOtherInstance->GetClassType() == PH_INST_FRAG_PED)
				{
					static_cast<CPed*>(pOtherEntity)->DoHitPed(impacts);
				}

				//B*1755318 Make the sliding door able to push the ped out of the path
				if(GetDoorType() == DOOR_TYPE_SLIDING_HORIZONTAL)
				{
					impacts.SetFriction(0.0f);
					impacts.SetMassInvScales(SLIDING_DOOR_MASS_INV_SCALE_AGAINST_PED, 1.0f);
				}
			}

			if(pOtherEntity->GetIsTypeVehicle())
			{
				// Make doors seem heavier when interacting with very heavy vehicles such as the tank/cargoplane
				// Doors aren't that light, but some vehicles are upwards of 50,000kg. Since doors happen to be constrained
				//   it causes serious physics issues. 
				CVehicle* vehicle = static_cast<CVehicle*>(pOtherEntity);

				float massRatio = vehicle->GetMass()*InvertSafe(GetMass(),0.0f);
				dev_float maximumMassRatio = 50.0f;
				if(massRatio > maximumMassRatio)
				{
					float desiredMassInvScale = maximumMassRatio/massRatio;
					dev_float minimumMassInvScale = 0.1f;
					impacts.SetMassInvScales(Max(desiredMassInvScale,minimumMassInvScale),1.0f);
				}

				// GTA 5 - B*1646249 - Set the friction to 0 in contacts between sliding doors
				// and vehicles to stop the contact firing the vehicle into the air
				int doorType = GetDoorType();
				if( doorType == DOOR_TYPE_SLIDING_HORIZONTAL )
				{
					impacts.SetFriction( 0.0f );

					const float maxImpulse		= 1.0f;
					const float maxImpulseSqrd	= maxImpulse * maxImpulse;
					Vector3 impulse;
					impacts.GetImpulse( impulse );
					float impulseMag = impulse.Mag2();

					if( impulseMag > maxImpulseSqrd )
					{
						ScalarV impulseScale( maxImpulse );

						impacts.GetContact().SetAccumImpulse( impulseScale );
						impacts.SetFriction( 0.0f );
					}
				}
				else if( doorType == DOOR_TYPE_STD &&
						 GetDoorOpenRatio() != 0.0f &&
						 vehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_IS_TANK ) )
				{
					// GTAV - B* 1720647 - the tank can be catapulted into the air by the large gates
					// in order to stop this happening check for a large impulse and just break the doors
					fwModelId modelId = GetModelId();

					if( modelId == MI_LRG_GATE_L ||
						modelId == MI_LRG_GATE_R )
					{
						const float largeImpulseSquared = 20000.0f * 20000.0f;

						Vector3 impulse;
						impacts.GetImpulse( impulse );
						float impulseMag = impulse.Mag2();

						if( impulseMag > largeImpulseSquared )
						{
							BreakOffDoor();
						}
					}
				}
			}
		}

		impacts.NextActiveContact();
	}
}

ePhysicsResult CDoor::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
#if __BANK
	if (sm_ProcessPhysics_BreakOnSelectedDoor)
	{
		if (this == g_PickerManager.GetSelectedEntity())
		{
			__debugbreak();
		}

	}
#endif // __BANK

	// Process just once each game frame
	if (nTimeSlice != 0)
	{
		return PHYSICS_DONE;
	}
	
	m_wantToBeAwake = false;
#if __BANK
	m_processedPhysicsThisFrame = 0;
#endif

	CObject::ProcessPhysics(fTimeStep, bCanPostpone, nTimeSlice);

	if(m_nDoorType > DOOR_TYPE_NOT_A_DOOR && !GetIsAnyFixedFlagSet()
		&& GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel())
	{
		const float curRatio = GetDoorOpenRatio();
		const float curtarRatioDif = fabsf(m_fDoorTargetRatio - curRatio);

#if RSG_PC
		const float fDefaultTimestep			= (1.0f / 30.0f) ;				// 30fps
		const float fDefaultEpsilonRatio		= ( fDefaultTimestep / 0.005f );	// tuned for 30fps
		const float epsilon	= fwTimer::GetTimeStep() / fDefaultEpsilonRatio; 
#else
		static dev_float epsilon = 0.005f;
#endif
		bool doorRatiosMatch = curtarRatioDif < epsilon;
		
		if (!(doorRatiosMatch) && m_fDoorTargetRatio < FLT_MAX)
		{
#if __BANK
			m_processedPhysicsThisFrame = int(!doorRatiosMatch);
#endif
			if(m_nDoorType==DOOR_TYPE_SLIDING_HORIZONTAL || m_nDoorType==DOOR_TYPE_SLIDING_HORIZONTAL_SC
				|| m_nDoorType==DOOR_TYPE_SLIDING_VERTICAL || m_nDoorType==DOOR_TYPE_SLIDING_VERTICAL_SC)
			{
				CalcAndApplyTorqueSLD(m_fDoorTargetRatio, curRatio, fTimeStep);
				
				// For vertical doors, gravity can be a problem, so we force it off here.
				// Perhaps it would be better to do it in ActivatePhysics(), but that's in CPhysical
				// and currently not virtual.
				if(m_nDoorType==DOOR_TYPE_SLIDING_VERTICAL || m_nDoorType==DOOR_TYPE_SLIDING_VERTICAL_SC)
				{
					phCollider* pCollider = GetCollider();
					if(pCollider)
					{
						pCollider->SetGravityFactor(0.0f);
					}
				}
			}
			else if(GetDoorType()==DOOR_TYPE_GARAGE || GetDoorType()==DOOR_TYPE_GARAGE_SC)
			{
				CalcAndApplyTorqueGRG(m_fDoorTargetRatio, curRatio, fTimeStep);
			}
			else if(GetDoorType()==DOOR_TYPE_BARRIER_ARM || GetDoorType()==DOOR_TYPE_BARRIER_ARM_SC || 
				GetDoorType() == DOOR_TYPE_RAIL_CROSSING_BARRIER || GetDoorType() == DOOR_TYPE_RAIL_CROSSING_BARRIER_SC)
			{
				CalcAndApplyTorqueBAR(m_fDoorTargetRatio, curRatio, fTimeStep);
			}
			else if(GetDoorType()==DOOR_TYPE_STD || GetDoorType()==DOOR_TYPE_STD_SC)
			{
#if __BANK
				if( CDoorSystem::ms_logDoorPhysics &&
					m_DoorSystemData )
				{
					Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
					physicsDisplayf( "Updating Door angle (%f, %f, %f) %s\tTarget: %.2f\tCurRatio: %.2f", 
										doorPos.x, doorPos.y, doorPos.z, m_DoorSystemData->GetNetworkObject() ?  m_DoorSystemData->GetNetworkObject()->GetLogName() : "", 
										m_fDoorTargetRatio, curRatio );
				}
#endif // #if __BANK

				CalcAndApplyTorqueSTD(m_fDoorTargetRatio, curRatio, fTimeStep);
			}
			else
			{
				physicsAssertf(0, "Unknown door type [%d] trying to process physics", GetDoorType());
			}

			static const float	fOpenThreshold = 0.02f;
			if ( m_DoorControlFlags.IsSet(DOOR_CONTROL_FLAGS_SMOOTH_STATUS_TRANSITION) &&
				curtarRatioDif < fOpenThreshold )
			{
				SetDoorControlFlag( DOOR_CONTROL_FLAGS_SMOOTH_STATUS_TRANSITION, false );
				CPhysics::GetLevel()->SetInstanceIncludeFlags( GetCurrentPhysicsInst()->GetLevelIndex(), m_SmoothTransitionPhysicsFlags );
				
				m_fAutomaticRate = 0.0f;
				m_fAutomaticDist = 0.0f;
				
				EnforceLockedObjectStatus(true);
			}
		}
	}
	else 
	{
#if __BANK
		if( CDoorSystem::ms_logDoorPhysics &&
			m_DoorSystemData &&
			GetDoorOpenRatio() != m_fDoorTargetRatio )
		{
			Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
			physicsDisplayf( "Failed to update Door angle (%f, %f, %f) %s\tType: %d\tAny Fixed Flags: %d\tIn level: %d", 
					doorPos.x, doorPos.y, doorPos.z, m_DoorSystemData->GetNetworkObject() ?  m_DoorSystemData->GetNetworkObject()->GetLogName() : "", 
					m_nDoorType, GetIsAnyFixedFlagSet(),
					GetCurrentPhysicsInst() ? GetCurrentPhysicsInst()->IsInLevel() : 0 );
		}
#endif // #if __BANK
		m_DoorAudioEntity.ProcessAudio();
	}

	phCollider* pCollider = GetCollider();
	if (pCollider)
	{
		if (m_wantToBeAwake)
		{
			//Check if the sleep is valid.
			phSleep* pSleep = pCollider->GetSleep();
			if(pSleep)
			{
				pSleep->Reset();
			}
		}
	}

	// reset damaged skeleton after pre-physics motion tree
	if(GetAnimDirector())
	{
		fragInst* pFragInst = GetFragInst();
		if (pFragInst)
		{
			fragCacheEntry* cacheEntry = pFragInst->GetCacheEntry();
			if(cacheEntry)
			{
				fragHierarchyInst* hierInst = cacheEntry->GetHierInst();
				pFragInst->ZeroSkeletonMatricesByGroup(pFragInst->GetSkeleton(), hierInst->damagedSkeleton);
			}
		}
	}

	return PHYSICS_DONE;
}

void CDoor::ProcessPostPhysics()
{
	m_bOnActivationUpdate = false;
}

void CDoor::OnActivate(phInst* pInst, phInst* pOtherInst)
{
	CObject::OnActivate(pInst,pOtherInst);	
	if(CDoor::DoorTypeOpensForPedsWhenNotLocked(GetDoorType()))
	{
		m_bOnActivationUpdate = true;
	}
}

void CDoor::ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const* hitInst, const Vector3& vMyHitPos, const Vector3& vOtherHitPos,
							 float fImpulseMag, const Vector3& vMyNormal, int iMyComponent, int iOtherComponent,
							 phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact)
{
	if(GetDoorType() > CDoor::DOOR_TYPE_NOT_A_DOOR)
	{
		ProcessDoorImpact(pHitEnt, RCC_VEC3V(vMyHitPos), vMyNormal);
		audDoorAudioEntity * audio = GetDoorAudioEntity();
		if(audio)
		{
			audio->EntityWantsToOpenDoor(vMyHitPos, fImpulseMag);
		}
	}
	CObject::ProcessCollision(myInst, pHitEnt, hitInst, vMyHitPos, vOtherHitPos, fImpulseMag, vMyNormal, iMyComponent, iOtherComponent, iOtherMaterial, bIsPositiveDepth, bIsNewContact);
}

bool CDoor::ProcessControl()
{
	bool result = CObject::ProcessControl();
	ProcessControlLogic();
	return result;
}


bool CDoor::RequiresProcessControl() const
{
	if(GetBaseModelInfo()->GetUsesDoorPhysics())
		return true;
	return false;
}


void CDoor::ProcessControlLogic()
{
#if __BANK
	if (sm_ProcessControlLogic_BreakOnSelectedDoor)
	{
		
		if (this == g_PickerManager.GetSelectedEntity())
		{
			__debugbreak();
		}

	}
#endif // __BANK

	if(m_nDoorType > DOOR_TYPE_NOT_A_DOOR)
	{
		ProcessDoorBehaviour();

		// We no longer do this here. There are calls to UpdatePortalState() elsewhere
		// instead, which call the more expensive ProcessPortalDisabling() only when
		// a change is detected.
		//	ProcessPortalDisabling();

		if(m_nDoorShot > (u16)fwTimer::GetTimeStepInMilliseconds())
			m_nDoorShot = m_nDoorShot - (u16)fwTimer::GetTimeStepInMilliseconds();
		else
		{
			if(m_nDoorShot)
			{
				// When m_nDoorShot reaches zero, we need to call UpdatePortalState() as the
				// return value of IsShutForPortalPurposes() may have changed.
				UpdatePortalState();
			}
			m_nDoorShot = 0;
		}
	}
}



bool CDoor::ObjectDamage(float fImpulse, const Vector3* pColPos, const Vector3* pColNormal, CEntity* pEntityResponsible, u32 WeaponUsedHash, phMaterialMgr::Id hitMaterial)
{
	if( CObject::ObjectDamage(fImpulse, pColPos, pColNormal, pEntityResponsible, WeaponUsedHash, hitMaterial) )
	{
		if( !IsNetworkClone() && GetDoorType() > CDoor::DOOR_TYPE_NOT_A_DOOR && WeaponUsedHash != 0 )
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponUsedHash);

			// if door gets shot, break the spring so it doesn't swing shut again for a while
			if( pWeaponInfo && pWeaponInfo->GetIsGun() && GetDoorType() == DOOR_TYPE_STD )
			{
				// only a u16 available here so don't set timer too high
				static s16 delayMS = 30000;
				m_nDoorShot = delayMS;

				//B* 558641 Hold doors open ... not ones connected to portals
				fwBaseEntityContainer* owner = fwEntity::GetOwnerEntityContainer();
				if( owner )
				{
					fwSceneGraphNode* sceneNode = owner->GetOwnerSceneNode();
					if( sceneNode && !sceneNode->IsTypePortal() )
					{
						SetHoldingOpen(true);
					}
				}

				// By calling UpdatePortalState() here, we make sure that the portal system
				// is immediately considering the portal open, even before the physics
				// system moves it (by looking at m_nDoorShot).
				UpdatePortalState();
			}

			// Explosive damage may break a door off its constraints
			bool damageIsExplosive = (WeaponUsedHash == WEAPONTYPE_EXPLOSION);
			if( pWeaponInfo && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE )
			{
				damageIsExplosive = true;
			}

			// TODO: Add tuning for explosions instead of piggybacking the vehicle flag
			static dev_float explosionForceScale = 3.0f;
			if( damageIsExplosive && (m_Tuning && m_Tuning->m_BreakableByVehicle) )
			{
				bool canStillBreakApart = false;
				if( fragInst* pFragInst = GetFragInst() )
				{
					if (fragCacheEntry* pEntry = pFragInst->GetCacheEntry() )
					{
						if( pEntry->IsFurtherBreakable() )
						{
							canStillBreakApart = true;
						}
					}
				}

				if( !canStillBreakApart )
				{
					if( (fImpulse * explosionForceScale) >= m_Tuning->m_BreakingImpulse )
					{
						BreakOffDoor();
						UpdatePortalState();
					}
				}
			}
		}

		return true;
	}
	return false;
}

audDoorAudioEntity* CDoor::GetDoorAudioEntity()
{
	return IsDoorAudioEntityValid() ? &m_DoorAudioEntity : NULL;
}

const audDoorAudioEntity* CDoor::GetDoorAudioEntity() const
{
	return IsDoorAudioEntityValid() ? &m_DoorAudioEntity : NULL;
}

bool CDoor::IsDoorAudioEntityValid() const
{
	bool isValid = true;
	if (m_nDoorType == DOOR_TYPE_NOT_A_DOOR) //it not a door ... CDoor::BreakOffDoor
	{
		isValid = false;
	}
	else if (m_DoorSystemData && m_DoorSystemData->GetBrokenFlags()) //If the door is broken then we dont want to allow any audio to be played ... 
	{
		isValid = false;
	}
	else if(GetFragInst()) 
	{
		//if the frag is broken then we dont want to play any audio
		const fragInst* pFragInst = GetFragInst();
		const fragCacheEntry* cache = pFragInst->GetCacheEntry();

		//To determine if a fragment is broken need to iterate over all its physics children
		// and ask if a bit is set in the groupbroken bitSet... if the value is set
		// then the object is broken ... 
		if (cache)
		{
			const fragHierarchyInst* hierInst = cache->GetHierInst();
			if (hierInst)
			{
				const atBitSet* groupBroken = hierInst->groupBroken;
				if (groupBroken)
				{
					isValid = !groupBroken->AreAnySet();
				}
			}
		}
	}

	return isValid;
}

//returns 0.0f if door shut, 1.0f above the thresolds supplied
float CDoor::GetDoorOcclusion(float transThreshold, float orienThreshold, float transThreshold1, float orienThreshold1) const
{
	Assert(transThreshold<transThreshold1);
	Assert(orienThreshold<orienThreshold1);

	const int doorType = GetDoorType();
	if(doorType==DOOR_TYPE_SLIDING_HORIZONTAL || doorType==DOOR_TYPE_SLIDING_HORIZONTAL_SC
			|| doorType==DOOR_TYPE_SLIDING_VERTICAL || doorType==DOOR_TYPE_SLIDING_VERTICAL_SC)
	{
		float sqdist=(m_vecDoorStartPos - VEC3V_TO_VECTOR3(GetTransform().GetPosition())).Mag2();

		if( sqdist < transThreshold)
			return 0.0f;
		else
			return rage::Clamp((rage::Sqrtf(sqdist)-rage::Sqrtf(transThreshold))/(rage::Sqrtf(transThreshold1)-rage::Sqrtf(transThreshold)), 0.0f, 1.0f);		

	}
	else 
	{
		float fCurrentAngle = 0.0f;
		float fClosedAngle = 0.0f;
		switch(GetDoorType())
		{
		case DOOR_TYPE_NOT_A_DOOR:
			return 0.0f;

		case DOOR_TYPE_GARAGE:
		case DOOR_TYPE_GARAGE_SC:
			{
				fCurrentAngle = GetTransform().GetPitch();

				Matrix34 matOriginal;
				matOriginal.FromQuaternion(m_qDoorStartRot);

				fClosedAngle = m_qDoorStartRot.TwistAngle(matOriginal.a);
			}
			break;
		case DOOR_TYPE_RAIL_CROSSING_BARRIER:
		case DOOR_TYPE_RAIL_CROSSING_BARRIER_SC:
		case DOOR_TYPE_BARRIER_ARM:
		case DOOR_TYPE_BARRIER_ARM_SC:
			{
				fCurrentAngle = GetTransform().GetRoll();

				Matrix34 matOriginal;
				matOriginal.FromQuaternion(m_qDoorStartRot);

				fClosedAngle = m_qDoorStartRot.TwistAngle(matOriginal.c);
			}
			break;
		case DOOR_TYPE_STD:
		case DOOR_TYPE_STD_SC:
			{
				fCurrentAngle = GetTransform().GetHeading();
				fClosedAngle = m_qDoorStartRot.TwistAngle(ZAXIS);
			}
			break;
		default:
			break;
		}
		float angle = fCurrentAngle - fClosedAngle;


		if(rage::Abs(angle) < orienThreshold)
			return 0.0f;
		else
			return rage::Clamp((rage::Abs(angle)-orienThreshold)/(orienThreshold1-orienThreshold), 0.0f, 1.0f);

	}
}

bool CDoor::IsDoorShut() const
{
	float orientThreshold = 0.07f;

	if(GetDoorType() == CDoor::DOOR_TYPE_GARAGE || GetDoorType() == CDoor::DOOR_TYPE_GARAGE_SC)
	{
		orientThreshold = 0.0525f;// Smaller tolerance for the generally larger garage doors.
	}

	return IsDoorShut(0.03f, orientThreshold);
}

bool CDoor::IsDoorShut(float transThreshold, float orienThreshold) const
{
	const int doorType = GetDoorType();

	if (doorType==DOOR_TYPE_NOT_A_DOOR)
	{
		if (m_ClonedArchetype || (GetCurrentPhysicsInst() && !GetCurrentPhysicsInst()->GetInstFlag(phInstGta::FLAG_IS_DOOR))) // door is broken
		{
			return false;
		}
		
		// Possible to get here if querying before the physics is initialized
		Assertf(GetCurrentPhysicsInst()==NULL, "CDoor::IsDoorShut - DOOR_TYPE_NOT_A_DOOR for door at %f %f %f", GetTransform().GetPosition().GetXf(),GetTransform().GetPosition().GetYf(),GetTransform().GetPosition().GetZf());
		// Assume the door is open (until we know for sure when the physics come in)
		return true;
	}

	if(doorType==DOOR_TYPE_SLIDING_HORIZONTAL || doorType==DOOR_TYPE_SLIDING_HORIZONTAL_SC
			|| doorType==DOOR_TYPE_SLIDING_VERTICAL || doorType==DOOR_TYPE_SLIDING_VERTICAL_SC)
	{
		if((m_vecDoorStartPos - VEC3V_TO_VECTOR3(GetTransform().GetPosition())).Mag2() < transThreshold)
			return true;
	}
	else if(doorType==DOOR_TYPE_GARAGE || doorType==DOOR_TYPE_GARAGE_SC ||
		doorType==DOOR_TYPE_STD || doorType==DOOR_TYPE_STD_SC ||
		doorType==DOOR_TYPE_BARRIER_ARM || doorType==DOOR_TYPE_BARRIER_ARM_SC || 
		doorType == DOOR_TYPE_RAIL_CROSSING_BARRIER || doorType == DOOR_TYPE_RAIL_CROSSING_BARRIER_SC)
	{
		float fCurrentAngle = 0.0f;
		float fClosedAngle = 0.0f;
		switch(doorType)
		{
		case DOOR_TYPE_GARAGE:
		case DOOR_TYPE_GARAGE_SC:
			{
				fCurrentAngle = GetTransform().GetPitch();

				Matrix34 matOriginal;
				matOriginal.FromQuaternion(m_qDoorStartRot);

				fClosedAngle = m_qDoorStartRot.TwistAngle(matOriginal.a);
				if (fClosedAngle > HALF_PI)
				{
					fClosedAngle = PI - fClosedAngle;
				}
			}
			break;
		case DOOR_TYPE_RAIL_CROSSING_BARRIER:
		case DOOR_TYPE_RAIL_CROSSING_BARRIER_SC:
		case DOOR_TYPE_BARRIER_ARM:
		case DOOR_TYPE_BARRIER_ARM_SC:
			{
				fCurrentAngle = GetTransform().GetRoll();

				Matrix34 matOriginal;
				matOriginal.FromQuaternion(m_qDoorStartRot);

				fClosedAngle = m_qDoorStartRot.TwistAngle(matOriginal.c);
			}
			break;
		case DOOR_TYPE_STD:
		case DOOR_TYPE_STD_SC:
			{
				fCurrentAngle = GetTransform().GetHeading();
				fClosedAngle = m_qDoorStartRot.TwistAngle(ZAXIS);
			}
			break;
		default:
			break;
		}

		const float PI2 = PI * 2.0f;
		float angle = fCurrentAngle - fClosedAngle;

		if (angle > PI)
		{
			angle -= PI2;
		}
		else if (angle <= -PI)
		{
			angle += PI2;
		}

		if(Abs(angle) < orienThreshold)
			return true;
	}

	return false;
}

bool CDoor::IsDoorOpening() const
{
	return m_bOpening;
}

bool CDoor::IsDoorFullyOpen( float fRatioEpsilon ) const
{
	static dev_float sfOpenThreshold = 0.99f;
	float fTargetDoorRatio = GetTargetDoorRatio();

	// Check to make sure we are none zero and close to 1.0/-1.0
	if( rage::Abs( fTargetDoorRatio ) < sfOpenThreshold ) 
		return false;

	//@@: location CDOOR_ISDOORFULLYOPEN
	const float fRatioDiff = rage::Abs( fTargetDoorRatio - GetDoorOpenRatio() );
	return fRatioDiff < fRatioEpsilon;
}

void CDoor::SetTargetDoorRatio(float fRatio, bool bForce, bool bWarp)
{
	if(GetDoorType()==DOOR_TYPE_NOT_A_DOOR)
	{
		Assertf(false, "Object %s is not a door but trying to set the target door ratio", GetModelName());
		return;
	}

#if __BANK
	if( CDoorSystem::ms_logDoorPhysics &&
		m_DoorSystemData )
	{
		Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
		physicsDisplayf( "CDoor::SetTargetDoorRatio: (%f, %f, %f) %s\tTarget: %.2f\nCurRatio: %.2f\tForce: %d\tWarp: %d", 
							doorPos.x, doorPos.y, doorPos.z, m_DoorSystemData->GetNetworkObject() ?  m_DoorSystemData->GetNetworkObject()->GetLogName() : "", 
							fRatio, GetDoorOpenRatio(), bForce, bWarp );
	}
#endif // #if __BANK

	SetDoorControlFlag( DOOR_CONTROL_FLAGS_STUCK, false );

	// set door state to identify this door as being controlled by the script
	if(!bForce)
		m_fDoorTargetRatio = fRatio;
	else
	{
		int nDoorType = GetDoorType();

		// Convert the door type to always reflect the code type
		if(nDoorType < DOOR_TYPE_STD_SC)
			nDoorType = (u8)(DOOR_TYPE_STD_SC + GetDoorType() - DOOR_TYPE_STD);

		m_fDoorTargetRatio = fRatio;

		// WARNING SETTING STATIC - DONT EXIT WITHOUT RESETTING THIS BOOLEAN
		ms_bUpdatingDoorRatio = true;
		///////////////////////

		if(nDoorType==DOOR_TYPE_SLIDING_HORIZONTAL_SC || nDoorType==DOOR_TYPE_SLIDING_VERTICAL_SC)
		{
			Vec3V slideAxisV;
			float fSlideLimit;
			GetSlidingDoorAxisAndLimit(slideAxisV, fSlideLimit);
			
			// Adding this multiplier as a temporary fix otherwise when the door is obstructed at creation it tries to open in the opposite direction
			float multiplier = 1;
			if(bWarp)
				multiplier = -1;

			Vector3 vecDesiredPos = m_vecDoorStartPos + (VEC3V_TO_VECTOR3(slideAxisV) * (fRatio * fSlideLimit)) * multiplier;
			SetPosition(vecDesiredPos);
		}
		else if(nDoorType==DOOR_TYPE_GARAGE_SC)
		{
			Matrix34 matUnrotated, matRotated;

			matUnrotated.FromQuaternion(m_qDoorStartRot);
			matUnrotated.d.Zero();

			float fDesiredHeading = (fRatio * GetLimitAngle());
			matRotated = matUnrotated;
			matRotated.RotateLocalX(fDesiredHeading);

			phBound* pBound = GetCurrentPhysicsInst()->GetArchetype()->GetBound();
			Vector3 offsetUnrotated = VEC3V_TO_VECTOR3(pBound->GetCGOffset());
			Vector3 offsetRotated = -offsetUnrotated;

			matUnrotated.Transform(offsetUnrotated);
			matRotated.Transform(offsetRotated);

			matRotated.d = offsetUnrotated + offsetRotated + m_vecDoorStartPos;

			SetMatrix(matRotated);
		}
		// all other door types rotate
		else if(nDoorType==DOOR_TYPE_BARRIER_ARM_SC || nDoorType == DOOR_TYPE_RAIL_CROSSING_BARRIER_SC)
		{
			Matrix34 matNew;

			matNew.FromQuaternion(m_qDoorStartRot);
			float fDesiredHeading = (fRatio * GetLimitAngle());
			matNew.RotateLocalY(-fDesiredHeading);
			matNew.d = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

			SetMatrix(matNew);
		}
		else if(nDoorType==DOOR_TYPE_STD_SC)
		{
			Matrix34 matOriginal;
			matOriginal.FromQuaternion(m_qDoorStartRot);
			float fOriginalHeading = rage::Atan2f(-matOriginal.b.x, matOriginal.b.y);
			float fDesiredHeading = fOriginalHeading + (fRatio * GetLimitAngle());

			SetHeading(fDesiredHeading);

			if( fabs( fRatio ) < CDoor::sm_fLatchResetThreshold )
				SetDoorControlFlag( DOOR_CONTROL_FLAGS_LATCHED_SHUT, true );
			else
				SetDoorControlFlag( DOOR_CONTROL_FLAGS_LATCHED_SHUT, false );
		}
		else
		{
			Assertf(false, "SetTargetDoorRatio - Unexpected door type '%d'", GetDoorType());
		}

		// Since we're not moving through the physics system in this case,
		// make sure to notify the portal system about the new state.
		UpdatePortalState();
	}

	// MUST RESET THIS BEFORE EXITING
	ms_bUpdatingDoorRatio = false;
}

//
// name:		LockDoor
// description:	Lock door and set it to its original position
void CDoor::ScriptDoorControl(bool bLock, float fOpenRatio, bool bForceOpenRatio, bool allowScriptControlConversion)
{
	const bool bChanged = (bLock != (bool)IsBaseFlagSet(fwEntity::IS_FIXED)) ||
						  (fOpenRatio != m_fDoorTargetRatio);

// 	CEntity* pSelectedEntity = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity());
// 	if( pSelectedEntity && pSelectedEntity->GetIsTypeObject() && static_cast<CObject*>(pSelectedEntity)->IsADoor() )
// 		Printf( "Door=%x Current=%f Set=%f Lock=%s Force=%s\n", this, GetTargetDoorRatio(), fOpenRatio, bLock ? "TRUE" : "FALSE", bForceOpenRatio ? "TRUE" : "FALSE" );

	// If we are currently in a smooth transition, just cancel it
	if ( m_DoorControlFlags.IsSet(DOOR_CONTROL_FLAGS_SMOOTH_STATUS_TRANSITION) )
	{
		SetDoorControlFlag( DOOR_CONTROL_FLAGS_SMOOTH_STATUS_TRANSITION, false );
		CPhysics::GetLevel()->SetInstanceIncludeFlags( GetCurrentPhysicsInst()->GetLevelIndex(), m_SmoothTransitionPhysicsFlags );
					
		m_fAutomaticRate = 0;
		m_fAutomaticDist = 0;
	}

	if(bLock)
	{
		SetTargetDoorRatio(fOpenRatio, true);
		SetFixedPhysics(true, false);
	}
	else
	{
		SetTargetDoorRatio(fOpenRatio, bForceOpenRatio);
		SetFixedPhysics(false, false);
	}

	if (allowScriptControlConversion)
	{
		// Logic to switch back and forth between a code and script driven door
		if( fOpenRatio != 0.0f )
		{
			// Switch to script driven door
			if(GetDoorType() < DOOR_TYPE_STD_SC)
				m_nDoorType = (u8)(DOOR_TYPE_STD_SC + GetDoorType() - DOOR_TYPE_STD);
		}
		else
		{
			// Switch back to code driven door	
			if(GetDoorType() >= DOOR_TYPE_STD_SC)
				m_nDoorType = (u8)(DOOR_TYPE_STD + GetDoorType() - DOOR_TYPE_STD_SC);
		}
	}	 		

	// If we had the lock-when-closed flag set, clear it, since something
	// else appears to be desired now.
	SetDoorControlFlag(DOOR_CONTROL_FLAGS_LOCK_WHEN_CLOSED, false);

	// Will need to update the pathserver's representation of this object.
	UpdateNavigation(bChanged);
}

void CDoor::UpdateNavigation(const bool bStateChanged)
{
	// Will need to update the pathserver's representation of this object.
	if(IsInPathServer() && bStateChanged)
	{
		CPathServerGta::UpdateDynamicObject(this, true);
	}
	// Attempt to add the object, if we have failed to before
	else if(WasUnableToAddToPathServer())
	{
		CPathServerGta::MaybeAddDynamicObject(this);
	}
}

CDoor::ChangeDoorStateResult CDoor::ChangeDoorStateForScript(u32 doorHash, const Vector3 &doorPos, CDoor* pDoor, bool addToLockedMap, bool lockState, float openRatio, bool unsprung, bool smoothLock, float automaticRate, float automaticDist, bool useOldOverrides, bool bForceOpenRatio)
{
	ChangeDoorStateResult result = CHANGE_DOOR_STATE_NO_MODEL;

	CEntity *doorObject = pDoor;

	bool bAddToDoorSystem = !pDoor && addToLockedMap;

	if (!doorObject)
	{
		CBaseModelInfo *modelInfo = CModelInfo::GetBaseModelInfoFromHashKey(doorHash, NULL);

		if(modelInfo)
		{
			if(!modelInfo->GetUsesDoorPhysics())
			{
				result = CHANGE_DOOR_STATE_NOT_DOOR;
			}
			else
			{
				Vector3 vecTestPos = doorPos;
				vecTestPos.x = rage::Floorf(vecTestPos.x + 0.5f);
				vecTestPos.y = rage::Floorf(vecTestPos.y + 0.5f);
				vecTestPos.z = rage::Floorf(vecTestPos.z + 0.5f);

				const float radius = CDoor::GetScriptSearchRadius(*modelInfo);
				doorObject = CDoor::FindClosestDoor(modelInfo, vecTestPos, radius);
	
				if (!doorObject)
				{
					doorObject = GetPointerToClosestMapObjectForScript(vecTestPos.x, vecTestPos.y, vecTestPos.z, radius, doorHash);
					// MAGDEMO 
					// Removed this assert for the magdemo as its really just a warning that I had made an assert.
					// Assertf(!doorObject, "CDoor::FindClosestDoor failed to find the door but GetPointerToClosestMapObjectForScript did add bug for David Ely");
				}

				if(doorObject == 0)
				{
					result = CHANGE_DOOR_STATE_NO_OBJECT;
				}
				else
				{
					if(!doorObject->GetIsTypeObject() && !doorObject->GetIsTypeDummyObject())
					{
						result = CHANGE_DOOR_STATE_INVALID_OBJECT_TYPE;
					}
					else if(doorObject->GetIsTypeBuilding())
					{
						result = CHANGE_DOOR_STATE_IS_BUILDING;
					}
					else if(doorObject->GetIsTypeObject() && !static_cast<const CObject*>(doorObject)->IsADoor())
					{
						// It seems to be possible to have CObjects for which CBaseModelInfo::GetUsesDoorPhysics() returns true,
						// but that are not CDoors. These are probably created by cutscenes, and we need to be careful to not
						// cast them into a CDoor. We now treat this condition the same as if CBaseModelInfo::GetUsesDoorPhysics()
						// had returned false, possibly triggering a script assert.
						result = CHANGE_DOOR_STATE_NOT_DOOR;
					}
					else
					{
						if (doorObject->GetIsTypeObject() && doorObject->GetCurrentPhysicsInst())
						{
							// For safety, we do a dynamic_cast here, in case the IsADoor() (which is based on CObjectFlags.bIsDoor)
							// that we do above doesn't catch all cases.
							Assertf(dynamic_cast<CDoor*>(doorObject), "Found an object (of type %s) for which IsADoor() returned true, but which is not a CDoor.", modelInfo->GetModelName());
							pDoor = static_cast<CDoor*>(doorObject);
						}
						else if (doorObject->GetIsTypeDummyObject())
						{
							// can't set heading on a dummy
							doorObject->AssignBaseFlag(fwEntity::IS_FIXED, lockState);
							result = CHANGE_DOOR_STATE_SUCCESS;
						}
					}
				}
			}
		}
	}

	CDummyObject *pDummyObject =NULL;

	if (pDoor)
	{
		// lock a physical door
		pDummyObject = pDoor->GetRelatedDummy();

		// If we're locking and the smoothLock parameter is set, we may have to use
		// the DOOR_CONTROL_FLAGS_LOCK_WHEN_CLOSED flag to close the door before it
		// locks.
		if(lockState && smoothLock)
		{
			// In this case, the door is always meant to become closed and locked, so
			// if the user passes in something else than 0 as the openRatio, there may
			// be some misunderstanding of how the parameters work.
			Assert(fabsf(openRatio) <= SMALL_FLOAT);

			// Check to see if we're already physically fixed.
			if(pDoor->IsBaseFlagSet(fwEntity::IS_FIXED))
			{
				// Door is already "locked". But is it actually locked in the right position (closed)?
				if(fabsf(pDoor->GetDoorOpenRatio()) <= SMALL_FLOAT)
				{
					// Yes, it's basically already closed. Force it to exactly 0
					// and leave it locked.
					pDoor->ScriptDoorControl(true, 0.0f);
					smoothLock = false;
				}
			}

			if(smoothLock)
			{
				// Door is either unlocked, or locked in the wrong position. Either way,
				// use ScriptDoorControl() to unlock it.
				pDoor->ScriptDoorControl(false, pDoor->m_fDoorTargetRatio);

				// Set the flag, so we can fix the door and complete the locking once it
				// has reached the fully closed position.
				pDoor->SetDoorControlFlag(DOOR_CONTROL_FLAGS_LOCK_WHEN_CLOSED, true);
			}
		}
		else
		{
			pDoor->ScriptDoorControl(lockState, openRatio, bForceOpenRatio);
		}

		pDoor->SetDoorControlFlag(DOOR_CONTROL_FLAGS_UNSPRUNG, unsprung);
		pDoor->m_fAutomaticRate = automaticRate;
		pDoor->m_fAutomaticDist = automaticDist;

		result = CHANGE_DOOR_STATE_SUCCESS;
	}

	if (bAddToDoorSystem && result == CHANGE_DOOR_STATE_SUCCESS)
	{
		Vector3 vecDoorEntityPos = VEC3V_TO_VECTOR3(doorObject->GetTransform().GetPosition());

		if(pDummyObject)
		{	//Use this if it is available since doorObject could be "settling" down and still 
			//changing position due to physics updates occuring in CDoor::UpdateEntityFromPhysics 
			vecDoorEntityPos = VEC3V_TO_VECTOR3(pDummyObject->GetTransform().GetPosition());
		}

		ASSERT_ONLY(CDoorSystemData *newStatus = )CDoorSystem::SetLocked(vecDoorEntityPos, lockState, openRatio, unsprung, doorHash, automaticRate, automaticDist, pDoor, useOldOverrides);
#if __ASSERT
		if(pDummyObject==NULL)
		{
			// if we already set the status on the actual object, check the LockedStatus generated by the two things (pos and entity) will match
			Assertf(newStatus==CDoorSystem::GetDoorData(doorObject), "Door that was locked didn't match the Locked Status generated by the position hash - the positions must be out");
		}
#endif
	}

	return result;
}


u8 CDoor::DetermineDoorType(const CBaseModelInfo* pModelInfo, const Vector3& bndBoxMin, const Vector3& bndBoxMax)
{
	u8 nDoorType = DOOR_TYPE_NOT_A_DOOR;
	if (pModelInfo)
	{
		if (pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_GARAGE_DOOR))
			nDoorType = DOOR_TYPE_GARAGE;
		else if (pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_SLIDING_DOOR))
			nDoorType = DOOR_TYPE_SLIDING_HORIZONTAL;
		else if (pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_NORMAL_DOOR))
			nDoorType = DOOR_TYPE_STD;
		else if (pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_BARRIER_DOOR))
			nDoorType = DOOR_TYPE_BARRIER_ARM;
		else if (pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_SLIDING_DOOR_VERTICAL))
			nDoorType = DOOR_TYPE_SLIDING_VERTICAL;
		else if (pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_RAIL_CROSSING_DOOR))
			nDoorType = DOOR_TYPE_RAIL_CROSSING_BARRIER;
	}

	// Attempt auto-detection of door type (old method - somewhat flakey)
	if (nDoorType == DOOR_TYPE_NOT_A_DOOR)
	{
		if(((bndBoxMax.z > -2.0f*bndBoxMin.z || TEST_SLIDING_DOORS) && (!TEST_GARAGE_DOORS && !TEST_BARRIER_ARMS)) || 
		   (pModelInfo && pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_SLIDING_DOOR)))
			// test for sliding doors is that pivot is at the bottom of the model.
			nDoorType = DOOR_TYPE_SLIDING_HORIZONTAL;
		else if(((bndBoxMax.x > 1.0f && bndBoxMin.x < -1.0f) || TEST_GARAGE_DOORS) && (!TEST_BARRIER_ARMS))
			// test for garage doors is that the model is > 2m wide and the pivot is (approx) in the middle.
			nDoorType = DOOR_TYPE_GARAGE;
		else if(((bndBoxMax.x-bndBoxMin.x) > (bndBoxMax.z-bndBoxMin.z) * 3.0f) || TEST_BARRIER_ARMS)
			// test for barriers is that the model is at least 3x longer than it is tall .  Weak, need to revisit all these tests
			nDoorType = DOOR_TYPE_BARRIER_ARM;
		else
			// Go with the standard door type
			nDoorType = DOOR_TYPE_STD;
	}

	return nDoorType;
}


float CDoor::GetScriptSearchRadius(const CBaseModelInfo& modelInfo)
{
	float radius = 1.0f;

	// Since sliding doors and garage doors have their actual position updated as they open,
	// it can't be expected of the scripters to pass in an accurate position to find them at.
	// We also can't easily search for the doors based on the original position, if we
	// want to keep using CGameWorld::ForAllEntitiesIntersecting(). We can determine what type
	// of door we're looking for, though, and increase the search radius a bit to make the
	// search more robust - we shouldn't end up finding the wrong door like this, because
	// we still pick the closest one to what the scripter passed in.
	// TODO: Would be really nice if we could rely on the flags of the object
	// so we didn't need to pass in the bounding box to DetermineDoorType(), but
	// it doesn't look like all doors are tagged with proper flags yet.
	const Vector3 &bndMin = modelInfo.GetBoundingBoxMin();
	const Vector3 &bndMax = modelInfo.GetBoundingBoxMax();
	u8 nDoorType = DetermineDoorType(&modelInfo, bndMin, bndMax);
	if(nDoorType == DOOR_TYPE_SLIDING_HORIZONTAL)
	{
		// Increase the radius by the width of the sliding door.
		radius += Max(-bndMin.x, bndMax.x);
	}
	if(nDoorType == DOOR_TYPE_SLIDING_VERTICAL)
	{
		// Increase the radius by the height of the sliding door.
		radius += bndMax.z;
	}
	else if(nDoorType == DOOR_TYPE_GARAGE)
	{
		// Increase the radius by half the width of the garage door, plus the
		// full height, so that we should find it regardless of how open it is
		// if given a position somewhere in the middle.
		radius += (bndMax.x - bndMin.x)*0.5f + bndMax.z - bndMin.z;
	}

	return radius;
}

void CDoor::ProcessDoorBehaviour()
{
#if __BANK
	if(sm_ForceAllToBreak && m_nDoorType != DOOR_TYPE_NOT_A_DOOR)
	{
		BreakOffDoor();
	}
#endif	// __BANK

	if(!GetIsAnyFixedFlagSet() && GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel())
	{
#if __BANK
		if(sm_ForceAllTargetRatio)
		{
			m_fDoorTargetRatio = sm_ForcedTargetRatio;
			return;
		}
#endif	// __BANK

		const bool bDoorIsStuck = m_DoorControlFlags.IsSet( DOOR_CONTROL_FLAGS_STUCK );

		//process door close delay ... 
		if (!IsNetworkClone() && !bDoorIsStuck && m_bDoorCloseDelayActive)
		{
			if (m_fDoorCloseDelay <= 0.0f)
			{
#if __BANK
				if( CDoorSystem::ms_logDoorPhysics &&
					m_DoorSystemData )
				{
					Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
					physicsDisplayf( "CDoor::ProcessDoorBehaviour: process door close delay ... (%f, %f, %f) %s\nTarget: %.2f\nCurRatio: %.2f", 
										doorPos.x, doorPos.y, doorPos.z, m_DoorSystemData->GetNetworkObject() ?  m_DoorSystemData->GetNetworkObject()->GetLogName() : "", 
										m_fDoorTargetRatio, GetDoorOpenRatio() );
				}
#endif // #if __BANK

				m_fDoorTargetRatio = 0.0f;//reset the door target ratio so the door closes
				m_bDoorCloseDelayActive = false;
			}
			m_fDoorCloseDelay -= fwTimer::GetTimeStep();
		}

		if( bDoorIsStuck )
		{
			// Check to see if we are at least half way open and if so mark the door as unstuck
			const float fOpenDoorRatio = GetDoorOpenRatio();
			if( Abs( fOpenDoorRatio ) > sm_fMaxStuckDoorRatio )
				m_DoorControlFlags.Set( DOOR_CONTROL_FLAGS_STUCK, false );
		}
		// Check if we're trying to close the door smoothly and lock it.
		else if(m_DoorControlFlags.IsSet(DOOR_CONTROL_FLAGS_LOCK_WHEN_CLOSED))
		{
			// As long as this flag is set, make sure we go towards 0, and use the same rate
			// as we would have used for auto-opening.
			const float fRate = GetEffectiveAutoOpenRate();
			const float fOpenRatio = GetDoorOpenRatio();
			m_fDoorTargetRatio = rage::Max( 0.0f, fOpenRatio - ( fRate * fwTimer::GetTimeStep() ) );

			// Check if the door has closed.
			if(IsDoorShut())
			{
				// Note: we could also check for velocity here, if it looks unnatural
				// that the motion stops suddenly once the door has reached the zero position.

				// Force the door exactly to the zero position, and make it fixed.
				SetTargetDoorRatio(0.0f, true);
				SetFixedPhysics(true, false);

				UpdateNavigation(true);

				// Clear the control flag.
				SetDoorControlFlag(DOOR_CONTROL_FLAGS_LOCK_WHEN_CLOSED, false);
				m_garageZeroVelocityFrameCount = 0;
			}
		}
		// check if we need to open door automatically (not for script controlled doors)
		//also check to see if we should be holding the door open from code
		else if( m_bUnderCodeControl || m_bHoldingOpen || DoorTypeAutoOpens( GetDoorType() ) )
		{
			m_bOpening = false;

			// Set the target ratio.
			float fTarget = 0.0f;

			bool bShouldAutoOpen = DetermineShouldAutoOpen();
			if( bShouldAutoOpen || m_bHoldingOpen )
			{
				m_bOpening = true;

				if(GetDoorType()==DOOR_TYPE_SLIDING_HORIZONTAL)
				{
					if(rage::Abs(GetBoundingBoxMin().x) > rage::Abs(GetBoundingBoxMax().x))
						fTarget = 1.0f;
					else
						fTarget = -1.0f;
				}
				else
					fTarget = 1.0f;

				if (!IsNetworkClone() && IsDoorShut())
				{
					ObjectHasBeenMoved(NULL);
				}
			}

			float fRate = GetEffectiveAutoOpenRate();
			float fCurrentRatio = GetDoorOpenRatio();

			//Only auto open doors should be using this taper... 
			if( bShouldAutoOpen && m_Tuning && m_Tuning->m_AutoOpenCloseRateTaper )
			{
				static dev_float fTaperRatioMin = 0.8f;
				static dev_float fTaperRatioMax = 0.99f;
				if( fCurrentRatio > fTaperRatioMin && fCurrentRatio < fTaperRatioMax )
				{
					Assertf( ( 1.0f - fTaperRatioMax ) > 0.0f, "Invalid TaperRatioMax!" );
					float fTaperRatio = rage::Max( ( fTaperRatioMax - fCurrentRatio ) / ( fTaperRatioMax - fTaperRatioMin ), 0.01f );
					fRate = fRate * fTaperRatio;
				}
			}

			if( !m_alwaysPush )
			{
				if( ( fTarget - fCurrentRatio ) > 0.0f )
				{
					m_fDoorTargetRatio = rage::Min( fCurrentRatio + ( fRate * fwTimer::GetTimeStep() ), fTarget );
				}
				else
				{
					m_fDoorTargetRatio = rage::Max( fCurrentRatio - ( fRate * fwTimer::GetTimeStep() ), fTarget );
				}
			}
		}

		// check if this door is asleep, and is at the target ratio
		if(!CPhysics::GetLevel()->IsActive(GetCurrentPhysicsInst()->GetLevelIndex()))
		{
			static float epsilon = 0.01f;
			const float curRatio = GetDoorOpenRatio();
			const float tarRatio = m_fDoorTargetRatio;   
			const float dif = fabsf(curRatio - tarRatio);
			
			if((dif < epsilon))
			{
				m_bUnderCodeControl = false;
			}
		}

		// Hack to deal with garage doors not closing properly
		if (sm_enableHackGarageShut && 
		    (GetDoorType() == DOOR_TYPE_GARAGE || GetDoorType() == DOOR_TYPE_GARAGE_SC) &&
			 !m_DoorControlFlags.IsSet(DOOR_CONTROL_FLAGS_LOCK_WHEN_CLOSED))
		{
			const int frameDelay = 8;
			if (m_garageZeroVelocityFrameCount > frameDelay)
			{
				m_garageZeroVelocityFrameCount = -1;
			}
			else if (m_garageZeroVelocityFrameCount == -1)
			{
				Vector3 vecForward(VEC3V_TO_VECTOR3(GetTransform().GetB()));
				float fCurrentHeading = rage::Atan2f(vecForward.z, vecForward.XYMag());

				const float headingInc = 0.005f;
				float fDesiredHeading;
				if (fCurrentHeading > 0.0f)
				{
					fDesiredHeading = rage::Max(fCurrentHeading - headingInc, 0.0f);				
				}
				else
				{
					fDesiredHeading = rage::Min(fCurrentHeading + headingInc, 0.0f);				
				}
				Matrix34 matUnrotated, matRotated;

				matUnrotated.FromQuaternion(m_qDoorStartRot);
				matUnrotated.d.Zero();

				matRotated = matUnrotated;
				matRotated.RotateLocalX(fDesiredHeading);

				phBound* pBound = GetCurrentPhysicsInst()->GetArchetype()->GetBound();
				Vector3 offsetUnrotated = VEC3V_TO_VECTOR3(pBound->GetCGOffset());
				Vector3 offsetRotated = -offsetUnrotated;

				matUnrotated.Transform(offsetUnrotated);
				matRotated.Transform(offsetRotated);

				matRotated.d = offsetUnrotated + offsetRotated + m_vecDoorStartPos;

				SetMatrix(matRotated, false, false, false);
				if (fDesiredHeading == 0.0f)
				{
 					ScriptDoorControl(true, 0.0f);
 					ScriptDoorControl(false, 0.0f);
					m_garageZeroVelocityFrameCount = 0;
				}
			}
		}
	}
}

void CDoor::SetOpenIfObstructed()
{
	// don't open locked doors!
	if (!(GetDoorSystemData() && GetDoorSystemData()->GetLocked()))
	{
		// Do a test to see if anything is obstructing the door
		WorldProbe::CShapeTestBoundingBoxDesc boxTest;
		WorldProbe::CShapeTestFixedResults<> boxTestResults;
		boxTest.SetResultsStructure(&boxTestResults);
		boxTest.SetBound(GetCurrentPhysicsInst()->GetArchetype()->GetBound());
		boxTest.SetTransform(&RCC_MATRIX34(GetCurrentPhysicsInst()->GetMatrix()));
		boxTest.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
		boxTest.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE);
		boxTest.SetExcludeEntity(this);
		bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(boxTest);

		if(hasHit)
		{
			// Build plane from pivot and forward vector of transform
			Vec3V pos3V  = GetTransform().GetPosition();
			Vec3V norm3V = Normalize(GetTransform().GetForward());
			ScalarV pushOut(0.35f);

			pos3V += norm3V * pushOut;

			// If there are hit points both in front and behind the plane the car is truly intersecting the door
			bool inFrontOfPlane = false;
			bool behindPlane    = false;

			WorldProbe::ResultIterator it;
			for(it = boxTestResults.begin(); it < boxTestResults.end(); ++it)
			{
				if (it->GetHitDetected())
				{
					bool bFreeDoor = false;

					if (it->GetHitEntity() && it->GetHitEntity()->GetIsTypeVehicle())
					{
						const phInst *pInst  = it->GetHitInst();
						const phBound *pBound = pInst->GetArchetype()->GetBound();

						Mat34V_ConstRef matrix = pInst->GetMatrix();

						Vec3V pos  = UnTransformOrtho(matrix, pos3V);
						Vec3V norm = UnTransform3x3Ortho(matrix, norm3V);

						Vector4 doorPlane = VEC4V_TO_VECTOR4(BuildPlane(pos, norm));

						const int pointCount = 8;
						const int max = 0;
						const int min = 4;
						Vector4 points[pointCount];

						// 						1------0
						// 					   /|     /|
						// 					  7-+----6 |
						// 					  | |    | |
						// 					  | 2----+-3
						// 					  |/     |/ 
						// 					  4------5  

						points[max] = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMax()); // Right Back Top
						points[max].SetW(1.0f);

						points[min] = VEC3V_TO_VECTOR3(pBound->GetBoundingBoxMin()); // Left Front Bottom 
						points[min].SetW(1.0f);

						points[1] = Vector4(points[min].x, points[max].y, points[max].z, 1.0f); // Left Back Top
						points[2] = Vector4(points[min].x, points[max].y, points[min].z, 1.0f); // Left Back Bottom
						points[3] = Vector4(points[max].x, points[max].y, points[min].z, 1.0f); // Right Back Bottom

						points[5] = Vector4(points[max].x, points[min].y, points[min].z, 1.0f); // Right Front Bottom
						points[6] = Vector4(points[max].x, points[min].y, points[max].z, 1.0f); // Right Front Top
						points[7] = Vector4(points[min].x, points[min].y, points[max].z, 1.0f); // Left Front Top

						for (int i = 0; i < pointCount; ++i)
						{
							if ( doorPlane.Dot(points[i]) < 0.0f)
							{
								behindPlane = true;
							}
							else
							{
								inFrontOfPlane = true;
							}
							if (behindPlane && inFrontOfPlane)
							{
								// If we hit anything, open the door now.
								bFreeDoor = true;
								break;
							}
						}
					}
					else
					{
						// If we hit anything, open the door now.
						bFreeDoor = true;
					}

					if (bFreeDoor)
					{
						if( m_nDoorType != DOOR_TYPE_STD || !NetworkUtils::IsNetworkClone(it->GetHitEntity()) )
						{
							SetTargetDoorRatio(1.0f, true, true);
						}
					}
				}
			}
		}
	}
}

bool CDoor::DetermineShouldAutoOpen()
{
	bool shouldAutoOpen = false;
	if(m_DoorControlFlags.GetAndClear(DOOR_CONTROL_FLAGS_DOUBLE_DOOR_SHOULD_OPEN))
	{
		// The other half of the double door told us we need to be open this frame.
		shouldAutoOpen = true;
	}
	else if (DoorTypeAutoOpens(GetDoorType()) && !m_bUnderCodeControl)
	{
		// Check if we are close to any player
		bool bCloseToPlayer = false;
		const float fPlayerCloseCheckRadius = 15.0f; // MAGIC!
		if (NetworkInterface::IsGameInProgress())
		{
			const netPlayer* pClosest = NULL;
			if( NetworkInterface::IsCloseToAnyPlayer(m_vecDoorStartPos, fPlayerCloseCheckRadius, pClosest) )
			{
				bCloseToPlayer = true;
			}
		}
		else // not a network game
		{
			CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if (pPlayer)
			{
				Vec3V vPlayer = pPlayer->GetTransform().GetPosition();
				if( IsLessThanAll(DistSquared(vPlayer, VECTOR3_TO_VEC3V(m_vecDoorStartPos)), ScalarV(fPlayerCloseCheckRadius * fPlayerCloseCheckRadius)) )
				{
					bCloseToPlayer = true;
				}
			}
		}

		// If we are close to any player
		if( bCloseToPlayer )
		{
			// reset time throttle variable
			m_uNextScheduledDetermineShouldAutoOpenTimeMS = 0;
		}
		else // not close to any player, safe to time throttle these checks
		{
			// get current time
			const u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();

			// respect a time delay on the update of the expensive physics checks in this method
			if( currentTimeMS < m_uNextScheduledDetermineShouldAutoOpenTimeMS )
			{
				// Just use the cached result from our most recent check
				return m_bCachedShouldAutoOpenStatus;
			}
			else // otherwise we need to update
			{
				// Schedule the next time to update
				static u32 performanceDelayPeriodMS = 150; // MAGIC!
				m_uNextScheduledDetermineShouldAutoOpenTimeMS = currentTimeMS + performanceDelayPeriodMS;
			}
		}

		bool useBox = false;
		if (m_Tuning && m_Tuning->m_UseAutoOpenTriggerBox)
		{
			useBox = true;
		}

#if __BANK
		if(sm_ForceAllAutoOpenToOpen)
		{
			shouldAutoOpen = true;
		}
		else if (!sm_ForceAllAutoOpenToClose)
#endif
		{
			// Get the the room info for my door 
			// A hit entity should make the door open only if it is outside or in the same room of the door.
			const fwInteriorLocation	doorIntLoc = this->GetInteriorLocation();
			fwInteriorLocation	room1Loc;
			fwInteriorLocation	room2Loc;

			u32	room1, room2;
			if (doorIntLoc.IsAttachedToPortal())
			{
				CInteriorInst*				interior = CInteriorInst::GetInteriorForLocation( doorIntLoc );
				CMloModelInfo* pMloModelInfo = interior->GetMloModelInfo();

				Assert(pMloModelInfo->GetIsStreamType());
				CPortalFlags dummy;
				pMloModelInfo->GetPortalData(doorIntLoc.GetPortalIndex(), room1, room2, dummy);

				if( room1 != 0 )
				{
					room1Loc = fwInteriorLocation( doorIntLoc.GetInteriorProxyIndex(), room1 );
				}

				if ( room2 != 0 )
				{
					room2Loc =   fwInteriorLocation( doorIntLoc.GetInteriorProxyIndex(), room2 );
				}
			}
			else if (doorIntLoc.IsValid()) // Door is in an interior
			{
				room1Loc = doorIntLoc;
			}

			// Set up the door flags
			fwFlags32 doorFlags;
			if( m_Tuning )
				doorFlags = m_Tuning->m_Flags;
			
			// test with desired types (peds and objects probably)
			int nTestTypes = ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE;

#if ENABLE_PHYSICS_LOCK
			phIterator iterator( phIterator::PHITERATORLOCKTYPE_READLOCK );
#else	// ENABLE_PHYSICS_LOCK
			phIterator iterator;
#endif	// ENABLE_PHYSICS_LOCK

			//////////////////////////////////////////////////////////////////////////
			//These are used for more accurate check testing.
			Mat34V matAutoDoor( V_IDENTITY );
			spdAABB localDoorBBox;
			spdOrientedBB orientedWorldDoorBBox;
			spdSphere worldDoorSphere;
			ScalarV angleBetweenCosineThreshold = (GetTuning()) ? ScalarV(GetTuning()->m_AutoOpenCosineAngleBetweenThreshold) : ScalarV(V_NEGONE);
			//////////////////////////////////////////////////////////////////////////			
			if (useBox)
			{
				CalcAutoOpenInfoBox( localDoorBBox, matAutoDoor );

				// Calculate the half width to setup the phys inst cull box
				Vec3V vAutoDoorHalfWidth = Scale( localDoorBBox.GetMax() - localDoorBBox.GetMin(), ScalarV( V_HALF ) );
				Vec3V vOffsetPosition =  Scale( localDoorBBox.GetMin() + localDoorBBox.GetMax(), ScalarV( V_HALF ) );
				matAutoDoor.SetCol3( Transform( matAutoDoor, vOffsetPosition ) );
				iterator.InitCull_Box( matAutoDoor, vAutoDoorHalfWidth );
			
				// Calculate the center position and transform that to world space
				Vec3V vCenterPosition = Scale( localDoorBBox.GetMax() + localDoorBBox.GetMin(), ScalarV( V_HALF ) );
				matAutoDoor.SetCol3( Transform( matAutoDoor, vCenterPosition ) );
				orientedWorldDoorBBox.Set( localDoorBBox, matAutoDoor );
			}
			else
			{
				Vec3V sphCenter;
				float sphRadius;
				CalcAutoOpenInfo(sphCenter, sphRadius);
				iterator.InitCull_Sphere( sphCenter, ScalarV(sphRadius) );
				worldDoorSphere.Set( Negate( sphCenter ), ScalarV(sphRadius));
			}
			
			spdOrientedBB orientedWorldEntityBBox;
			ScalarV cosineAngleBetween;
			Vec3V doorAngleCheckPos = CalcAngleThresholdCheckPos();
			iterator.SetIncludeFlags( nTestTypes );
			iterator.SetStateIncludeFlags( phLevelBase::STATE_FLAGS_ALL );
			u16 culledLevelIndex = CPhysics::GetLevel()->GetFirstCulledObject(iterator);
			while(culledLevelIndex != phInst::INVALID_INDEX)
			{
				phInst* pCulledInstance = CPhysics::GetLevel()->GetInstance(culledLevelIndex);
				aiAssert(pCulledInstance);

				CEntity* pCulledEntity = CPhysics::GetEntityFromInst(pCulledInstance);
				if( pCulledEntity && pCulledEntity != this) //make sure entity is valid and not me ... 
				{
					// Skip over peds in vehicles as the vehicle should take care of the auto open bounds
					bool bIsPed = pCulledEntity->GetIsTypePed();
					if( bIsPed && static_cast<const CPed*>(pCulledEntity)->GetIsInVehicle() )
					{
						culledLevelIndex = CPhysics::GetLevel()->GetNextCulledObject(iterator);
						continue;
					}

					bool bIsInVolume = false;
					if( pCulledEntity->GetIsDynamic() )
					{
						const CDynamicEntity* pDynamic = static_cast<const CDynamicEntity*>( pCulledEntity );
						orientedWorldEntityBBox.Set( pDynamic->GetLocalSpaceBoundBox( localDoorBBox ), pDynamic->GetMatrix() );
						if( useBox )
						{	
							if( orientedWorldDoorBBox.IntersectsOrientedBB( orientedWorldEntityBBox ) )
							{
								bIsInVolume = true;
							}
						}
						else
						{
							if( orientedWorldEntityBBox.IntersectsSphere( worldDoorSphere ) )
							{
								bIsInVolume = true;
							}
						}
					}

					bool bIsInSameDirection = true;
					if( !bIsPed && pCulledEntity->GetIsPhysical() ) 
					{
						const CPhysical* pPhysical = static_cast<const CPhysical*>( pCulledEntity );
						Vec3V vVelocity = RCC_VEC3V( pPhysical->GetVelocity() );

						// If we are moving at some minimum speed, use the entity velocity
						static dev_float sf_MovingTolerance = 5.5f;
						const float fVelocity = Mag( vVelocity ).Getf();
						if( fVelocity > 0.0f && fVelocity > sf_MovingTolerance )
							cosineAngleBetween = Dot( Normalize( doorAngleCheckPos - matAutoDoor.GetCol3() ), Normalize( vVelocity ) );
						// Otherwise use the entity forward
						else
							cosineAngleBetween = Dot( Normalize( doorAngleCheckPos - matAutoDoor.GetCol3() ), pCulledEntity->GetTransform().GetB() );

						//grcDebugDraw::Arrow( doorAngleCheckPos, matAutoDoor.GetCol3(), 0.5f, Color_green );
						bIsInSameDirection = IsGreaterThanOrEqualAll( cosineAngleBetween, angleBetweenCosineThreshold ) > 0;
						//Displayf("CHECK: CosAngBetween: %f CosAngBetweenThreshold: %f", cosineAngleBetween.Getf(), angleBetweenCosineThreshold.Getf());
					}
					 					
					if(bIsInVolume && bIsInSameDirection && EntityCausesAutoDoorOpen( pCulledEntity, room1Loc, room2Loc, doorFlags ) )
					{
						shouldAutoOpen = true;

						// We have detected that we need to open (or remain open). If we
						// haven't yet checked if we're a part of a double door, do so now.
						if(!m_DoorControlFlags.IsSet(DOOR_CONTROL_FLAGS_CHECKED_FOR_DOUBLE))
						{
							CheckForDoubleDoor();
						}

						// If this is a double door, signal that the other door should also
						// open (or remain open).
						CDoor* pOtherDoor = m_OtherDoorIfDouble;
						if(pOtherDoor)
						{
							// Note that this SHOULD BE passing shouldOpen but I'm leaving it temporarily as true 
							// until all door are attached to portals.  Others this will create many PTS.
							pOtherDoor->SetDoorControlFlag(DOOR_CONTROL_FLAGS_DOUBLE_DOOR_SHOULD_OPEN, true/*shouldOpen*/);
						}

						// Note: with regards to double doors, we could potentially change this code so that
						// if we have a double door, one of the doors gets designated as the master, and then
						// we only do one sphere test which applies to both doors. The way it is now, each
						// door does its own test, and just tells the other door to open if needed.

						break; //dont need to keep checking once we found one case were we want to auto open the door.
					}
				}

				culledLevelIndex = CPhysics::GetLevel()->GetNextCulledObject(iterator);
			}
		}
	}	

	// cache the result
	m_bCachedShouldAutoOpenStatus = shouldAutoOpen;

	return shouldAutoOpen;
}

Vec3V_Out CDoor::CalcAngleThresholdCheckPos() const
{
	// Center of the bounding box on the localX, transformed to the starting door pos/rot in world space
	Mat33V mat;
	Mat33VFromQuatV(mat, QUATERNION_TO_QUATV(m_qDoorStartRot));
	Vec3V vDoorStartPos = VECTOR3_TO_VEC3V(m_vecDoorStartPos);
	spdAABB localBB;
	const spdAABB &result = GetLocalSpaceBoundBox(localBB);
	ScalarV centerX = result.GetCenter().GetX();
	return vDoorStartPos + mat.GetCol0() * centerX;
}

void CDoor::ProcessPortalDisabling()
{
	if ( GetIsRetainedByInteriorProxy() )
		return;

#ifdef GTA_REPLAY
	//this can be null when switching from one replay to another
	if(fwEntity::GetOwnerEntityContainer() == NULL)
	{
		return;
	}
#endif // GTA_REPLAY

	fwSceneGraphNode*			sceneNode = fwEntity::GetOwnerEntityContainer()->GetOwnerSceneNode();
	if ( !sceneNode->IsTypePortal() )
		return;

	fwPortalSceneGraphNode*		portalSceneNode = reinterpret_cast< fwPortalSceneGraphNode* >( sceneNode );
	const fwInteriorLocation	intLoc = portalSceneNode->GetInteriorLocation();
	CInteriorInst*				intInst = CInteriorInst::GetInteriorForLocation( intLoc );
	bool						newState = !(intInst ? intInst->TestPortalState( true, false, intLoc.GetPortalIndex() ) : true);
	
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && GetOwnedBy() != ENTITY_OWNEDBY_REPLAY)
	{
		newState = true;
	}
#endif

	if ((portalSceneNode->GetPositivePortalEnd() && portalSceneNode->GetPositivePortalEnd()->IsTypeExterior()) ||
		(portalSceneNode->GetNegativePortalEnd() && portalSceneNode->GetNegativePortalEnd()->IsTypeExterior()))
	{
		if (m_bCountsTowardOpenPortals && newState == false)
		{
			Assertf(intInst, "Attempting to decrement open portal count but interior inst is NULL");
			intInst->DecrementmOpenPortalsCount((u16)intLoc.GetInteriorProxyIndex());
			m_bCountsTowardOpenPortals = false;
		}
		else if (!m_bCountsTowardOpenPortals && newState == true)
		{
			Assertf(intInst, "Attempting to increment open portal count but interior inst is NULL");
			intInst->IncrementmOpenPortalsCount((u16)intLoc.GetInteriorProxyIndex());
			m_bCountsTowardOpenPortals = true;
		}
	}

	portalSceneNode->Enable( newState );
}

bool CDoor::IsShutForPortalPurposes() const
{
	// The portal is considered closed only if the angle is small enough,
	// AND we haven't been touched by the player or shot recently.
	// Also avoid to shut it if cutscene has requested it.

	if (!IsBaseFlagSet(fwEntity::IS_VISIBLE)){
		return(false);
	} 
	else
	{
		const float stdDoorOrientThreshold = 0.025f;
		const float defaultOrientThreshold = 0.015f;
		float orientThreshold = (m_nDoorType == DOOR_TYPE_STD || m_nDoorType == DOOR_TYPE_STD_SC) ? stdDoorOrientThreshold : defaultOrientThreshold;
		return m_nDoorShot == 0 && !m_DoorControlFlags.IsSet(DOOR_CONTROL_FLAGS_CUTSCENE_DONT_SHUT) && IsDoorShut(0.03f, orientThreshold);
	}
	
}


void CDoor::UpdatePortalState(bool force)
{
	// Check if the door should be considered shut by the portal system.
	const bool isShut = IsShutForPortalPurposes();
	if(isShut == m_WasShutForPortalPurposes && !force)
	{
		// Nothing changed, don't bother touching the memory of the portal and
		// other doors connected to the portal.
		return;
	}

	// Remember the new state, and notify the portal system (which may or may not
	// actually change the visibility of the portal, depending on the state of
	// other doors using the same portal).
	m_WasShutForPortalPurposes = isShut;
	ProcessPortalDisabling();
}

void CDoor::InitDoorPhys(const Matrix34& matOriginal)
{
#if GTA_REPLAY
	if(GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)
	{
		InitReplay(matOriginal);
		return;
	}
#endif

	// check for upside-down doors
#if __ASSERT
	if (GetTransform().GetC().GetZf() <= 0.99f REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()) )
	{
		const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		Assertf(0, "%s Door isn't placed vertically Pos(%f,%f,%f)", GetModelName(), vThisPosition.x, vThisPosition.y, vThisPosition.z);
	}
#endif

	// add a constraint to deal with rotation about z (search to remove constraints we already created first)
	if( !GetCurrentPhysicsInst() || !GetCurrentPhysicsInst()->IsInLevel() )
	{
		Displayf( "CDoor::InitDoorPhys: Door is not in level" );
		return;
	}


	// search to remove constraints we already created first
	RemoveConstraints(GetCurrentPhysicsInst());

	m_vecDoorStartPos = matOriginal.d;
	matOriginal.ToQuaternion(m_qDoorStartRot);

	CBaseModelInfo* pModelInfo = GetBaseModelInfo();
	m_nDoorType = DetermineDoorType(pModelInfo, GetBoundingBoxMin(), GetBoundingBoxMax());

	// InitDoorPhys() can potentially be called again, through UpdatePhysicsFromEntity() and perhaps in other
	// ways. If we already have a pointer to the tuning data, it's probably best to not overwrite it, in case
	// it was changed through script (even though this isn't supported at the moment, it would be a reasonable feature).
	// Note: we could also potentially move both the call to DetermineDoorType() and GetTuningForDoorModel() out
	// of InitDoorPhys(), as they should both only depend on model info stuff which would be known early.
	if(!m_Tuning)
	{
		m_Tuning = &CDoorTuningManager::GetInstance().GetTuningForDoorModel(pModelInfo->GetModelNameHash(), m_nDoorType);
	}


	switch(m_nDoorType)
	{
		case DOOR_TYPE_SLIDING_HORIZONTAL:
		case DOOR_TYPE_SLIDING_VERTICAL:
		{
			phConstraintPrismatic::Params params;
										  params.instanceA = GetCurrentPhysicsInst();

			if(m_nDoorType == DOOR_TYPE_SLIDING_HORIZONTAL)
			{
				if(rage::Abs(GetBoundingBoxMin().x) > rage::Abs(GetBoundingBoxMax().x))
				{
					params.maxDisplacement = -GetBoundingBoxMin().x;
					params.slideAxisLocalB = RCC_VEC3V(matOriginal.a);
				}
				else
				{
					params.maxDisplacement = GetBoundingBoxMax().x;
					params.slideAxisLocalB = -RCC_VEC3V(matOriginal.a);
				}

				Assert( GetCurrentPhysicsInst()->GetArchetype() );
				phBound* pBound = GetCurrentPhysicsInst()->GetArchetype()->GetBound();
				if( pBound && pBound->GetType() == phBound::COMPOSITE )
				{
					((phBoundComposite*)pBound)->CalcCenterOfGravity();
				}				
			}
			else
			{
				params.maxDisplacement = GetBoundingBoxMax().z;
				params.slideAxisLocalB = RCC_VEC3V(matOriginal.c);
			}
			
			ASSERT_ONLY(phConstraintHandle slidingDoorHandle =) PHCONSTRAINT->Insert(params);
			physicsAssertf( slidingDoorHandle.IsValid(), "Door::InitDoorPhys unable to create a sliding door constraint" );

			break;
		}
		case DOOR_TYPE_GARAGE:
		{
			static float GARAGE_DOOR_PIVOT_OFFSET_FRACTION = 0.8f;
			//float fBoundHeight = (GetBoundingBoxMax().z-GetBoundingBoxMin().z);
			Vector3 vecPivotOffset;
			vecPivotOffset.Zero();
			vecPivotOffset.y = GARAGE_DOOR_PIVOT_OFFSET_FRACTION * GetBoundingBoxMax().z;
			vecPivotOffset.z = (1.0f - GARAGE_DOOR_PIVOT_OFFSET_FRACTION) * GetBoundingBoxMax().z;

			phBound* pBound = GetCurrentPhysicsInst()->GetArchetype()->GetBound();
			pBound->SetCGOffset(RCC_VEC3V(vecPivotOffset));

			// Get position of fake centre of mass (centre of rotation) in world space
			matOriginal.Transform(vecPivotOffset);

			// Constrained to rotate about x-axis
			phConstraintHinge::Params constraint;
			constraint.instanceA = GetCurrentPhysicsInst();
			constraint.worldAnchor = RCC_VEC3V(vecPivotOffset);
			constraint.worldAxis = RCC_VEC3V(matOriginal.a);
			constraint.minLimit = GetLimitAngle();
			constraint.maxLimit = 0.0f;

			ASSERT_ONLY(phConstraintHandle garageDoorHingeHandle =) PHCONSTRAINT->Insert(constraint);
			physicsAssertf( garageDoorHingeHandle.IsValid(), "Door::InitDoorPhys unable to insert a garage door's hinge constraint" );

			break;
		}
		case DOOR_TYPE_RAIL_CROSSING_BARRIER:
		case DOOR_TYPE_BARRIER_ARM:
		{
			Vector3 vecPivotOffset;
			vecPivotOffset.Zero();

			phBound* pBound = GetCurrentPhysicsInst()->GetArchetype()->GetBound();
			pBound->SetCGOffset(RCC_VEC3V(vecPivotOffset));

			// Get position of fake centre of mass (centre of rotation) in world space
			matOriginal.Transform(vecPivotOffset);

			// Constrained to rotate about y-axis
			phConstraintHinge::Params constraint;
			constraint.instanceA = GetCurrentPhysicsInst();
			constraint.worldAnchor = RCC_VEC3V(vecPivotOffset);
			constraint.worldAxis = RCC_VEC3V(matOriginal.b);
			constraint.minLimit = -GetLimitAngle();
			constraint.maxLimit = 0.0f;

			// Intentionally use a large minimum separation and a third constraint to improve stability when objects push the barrier horizontally. 
			constraint.minFixedPointSeparation = 2.0f;
			constraint.useMiddleFixedPoint = true;

			ASSERT_ONLY(phConstraintHandle barrierDoorHingeHandle =) PHCONSTRAINT->Insert(constraint);
			physicsAssertf( barrierDoorHingeHandle.IsValid(), "Door::InitDoorPhys unable to insert a barrier arm's hinge constraint" );

			break;
		}
		case DOOR_TYPE_STD:
		{
			// add a constraint to deal with rotation about z
			phConstraintHinge::Params constraint;
			constraint.instanceA = GetCurrentPhysicsInst();
			constraint.worldAnchor = GetTransform().GetPosition();
			constraint.worldAxis = Vec3V(V_Z_AXIS_WZERO);

			if (m_Tuning)
			{
				if (m_Tuning->m_StdDoorRotDir == StdDoorOpenNegDir)
				{
					constraint.minLimit = -GetLimitAngle();
					constraint.maxLimit = 0.0f;
				}
				else if (m_Tuning->m_StdDoorRotDir == StdDoorOpenPosDir)
				{
					constraint.minLimit = 0.0f;
					constraint.maxLimit = GetLimitAngle();
				}
				else
				{
					constraint.minLimit = -GetLimitAngle();
					constraint.maxLimit = GetLimitAngle();
				}
			}
			else
			{
				constraint.minLimit = -GetLimitAngle();
				constraint.maxLimit = GetLimitAngle();
			}
			
			ASSERT_ONLY(phConstraintHandle standardDoorHingeHandle =) PHCONSTRAINT->Insert(constraint);
			physicsAssertf( standardDoorHingeHandle.IsValid(), "Door::InitDoorPhys unable to insert a standard door's hinge constraint" );

			break;
		}
		default:
			Assertf(0, "Unhandled doortype");
			break;
	}

	// If the MinMoveForce parameter in the fragType is set to not allow the object to move,
	// we can't activate the object and it can't function as a door. However, rather than
	// making sure this parameter is set to allow movement, it's probably more convenient to
	// call SetBroken(true), which I believe basically has the same effect, even though it feels
	// a bit misnamed. It doesn't mean that the object really is broken, just that it's allowed
	// to move.
	// Note: during a review, Eugene mentioned that we may be better off forcing the
	// MinMoveForce parameter. TODO: Think more about how that can be done.
	fragInst* pFragInst = GetFragInst();
	if(pFragInst)
	{
		pFragInst->SetBroken(true);
	}

	XPARAM(nogameaudio);
	if (!PARAM_nogameaudio.Get())
	{
		//Init the audio.
		m_DoorAudioEntity.InitDoor(this);
	}
}

bool CDoor::GetDoorIsAtLimit(Vector3::Vector3Param vPos, Vector3::Vector3Param vDir, const float fHeadingThreshold)
{
	if( (m_nDoorType == DOOR_TYPE_STD || m_nDoorType == DOOR_TYPE_STD_SC) && 
		!GetIsAnyFixedFlagSet() && GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel())
	{
		Matrix34 matOriginal;
		matOriginal.FromQuaternion(m_qDoorStartRot);

		float fOriginalHeading = rage::Atan2f(-matOriginal.b.x, matOriginal.b.y);
		float fCurrentHeading = GetTransform().GetHeading() - fOriginalHeading;
		fCurrentHeading = fwAngle::LimitRadianAngleFast(fCurrentHeading);

		fCurrentHeading /= GetLimitAngle();

		Vector3 vecOffset = VEC3V_TO_VECTOR3(GetTransform().GetPosition()) - vPos;
		const Vector3 vecForward(VEC3V_TO_VECTOR3(GetTransform().GetB()));
		float fPosDot = DotProduct(vecForward, vecOffset);
		float fDirDot = DotProduct(vecForward, vDir);

		// deal with left handed doors (a door that's modeled along it's negative x-axis)
		// by checking the bbox and flipping the heading
		//if(GetBoundingBoxMax().x > rage::Abs(GetBoundingBoxMin().x))
		if(GetBoundingBoxMin().x < -PED_HUMAN_RADIUS)
			fCurrentHeading *= -1.0f;

		if(fPosDot > 0.0f && fDirDot > 0.0f && fCurrentHeading  > fHeadingThreshold)
			return true;
		else if(fPosDot < 0.0f && fDirDot < 0.0f && fCurrentHeading  < -fHeadingThreshold)
			return true;
	}

	return false;
}

float CDoor::GetDoorOpenRatio() const
{
#if GTA_REPLAY
	if(CReplayMgr::IsReplayInControlOfWorld() && m_ReplayDoorOpenRatio != FLT_MAX)
	{
		return m_ReplayDoorOpenRatio;
	}
#endif

	float fOpenRatio = 0.0f;
	switch(GetDoorType())
	{
	case DOOR_TYPE_GARAGE:
	case DOOR_TYPE_GARAGE_SC:
		{
			float fCurrentAngle = GetTransform().GetPitch();

			Matrix34 matOriginal;
			matOriginal.FromQuaternion(m_qDoorStartRot);

			float fClosedAngle = m_qDoorStartRot.TwistAngle(matOriginal.a);

			fOpenRatio = fwAngle::LimitRadianAngleForPitch(fCurrentAngle - fClosedAngle);
			fOpenRatio = Abs(fOpenRatio) / Abs(GetLimitAngle());	

			Assertf(FPIsFinite(fOpenRatio), "CDoor::GetDoorOpenRatio - Garage door at pos: %f %f %f ratio is not finite fLimitAngle: %f fCurrentAngle: %f fClosedAngle: %f",
				GetTransform().GetPosition().GetXf(), GetTransform().GetPosition().GetYf(), GetTransform().GetPosition().GetZf(), 
				GetLimitAngle(), fCurrentAngle, fClosedAngle);
		}
		break;
	case DOOR_TYPE_STD:
	case DOOR_TYPE_STD_SC:
		{
			float fCurrentAngle = GetTransform().GetHeading();
			float fClosedAngle = m_qDoorStartRot.TwistAngle(ZAXIS);

			fOpenRatio = fwAngle::LimitRadianAngle(fCurrentAngle - fClosedAngle);
			fOpenRatio = fOpenRatio / GetLimitAngle();

			Assertf(FPIsFinite(fOpenRatio), "CDoor::GetDoorOpenRatio - Standard door at pos: %f %f %f open ratio is not finite fLimitAngle: %f fCurrentAngle: %f fClosedAngle: %f", 
				GetTransform().GetPosition().GetXf(), GetTransform().GetPosition().GetYf(), GetTransform().GetPosition().GetZf(), 
				GetLimitAngle(), fCurrentAngle, fClosedAngle);
		}
		break;
	case DOOR_TYPE_RAIL_CROSSING_BARRIER:
	case DOOR_TYPE_RAIL_CROSSING_BARRIER_SC:
	case DOOR_TYPE_BARRIER_ARM:
	case DOOR_TYPE_BARRIER_ARM_SC:
		{
			float fCurrentAngle = GetTransform().GetRoll();

			Matrix34 matOriginal;
			matOriginal.FromQuaternion(m_qDoorStartRot);

			float fClosedAngle = m_qDoorStartRot.TwistAngle(matOriginal.b);

			fOpenRatio = fwAngle::LimitRadianAngleForPitch(fCurrentAngle - fClosedAngle);
			fOpenRatio = fOpenRatio / GetLimitAngle();

			Assertf(FPIsFinite(fOpenRatio), "CDoor::GetDoorOpenRatio - Barrier door at pos: %f %f %f open ratio is not finite fLimitAngle: %f fCurrentAngle: %f fClosedAngle: %f",
				GetTransform().GetPosition().GetXf(), GetTransform().GetPosition().GetYf(), GetTransform().GetPosition().GetZf(),
				GetLimitAngle(), fCurrentAngle, fClosedAngle);
		}
		break;
	case DOOR_TYPE_SLIDING_HORIZONTAL:
	case DOOR_TYPE_SLIDING_HORIZONTAL_SC:
	case DOOR_TYPE_SLIDING_VERTICAL:
	case DOOR_TYPE_SLIDING_VERTICAL_SC:
		{
			Vector3 vDiff = VEC3V_TO_VECTOR3(GetTransform().GetPosition()) - m_vecDoorStartPos;

			// Note: this code used to use the original matrix for the axis projection here, but
			// I want to be able to use the GetSlidingDoorAxisAndLimit() helper function now, and
			// it should be the same thing.
			//	Matrix34 matOriginal;
			//	matOriginal.FromQuaternion(m_qDoorStartRot);

			
			float fSlideLimit;
			Vec3V slideAxisV;
			GetSlidingDoorAxisAndLimit(slideAxisV, fSlideLimit);

			fOpenRatio = vDiff.Dot(VEC3V_TO_VECTOR3(slideAxisV));
			fOpenRatio = fOpenRatio / fSlideLimit;

			Assertf(FPIsFinite(fOpenRatio), "CDoor::GetDoorOpenRatio - Sliding door at pos: %f %f %f open ratio is not finite fSlideLimit: %f vSlideAxis: %f %f %f vDiff: %f %f %f", 
				GetTransform().GetPosition().GetXf(), GetTransform().GetPosition().GetYf(), GetTransform().GetPosition().GetZf(), 
				fSlideLimit, slideAxisV.GetXf(), slideAxisV.GetYf(), slideAxisV.GetZf(), vDiff.x, vDiff.y, vDiff.z);
		}
		break;
	default:
		break;
	}

	return fOpenRatio;
}


void CDoor::ProcessDoorImpact(CEntity* pOtherEntity, Vec3V_In vHitPos, const Vector3 & vNormal)
{
	if( !IsNetworkClone() && pOtherEntity )
	{
		int nDoorType = GetDoorType();
		if( nDoorType == DOOR_TYPE_GARAGE )
		{
			// bug 2021488 : don't allow doors to collide with dead peds - to stop the dead peds blocking garage doors
			if (pOtherEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pOtherEntity);
				if (pPed->GetDeathState() == DeathState_Dead)
				{
					const CPhysical* pCollider = pPed->GetRagdollOnCollisionIgnorePhysical();
					if (pCollider == NULL)
					{
						// this ped ignores this door
						pPed->SetRagdollOnCollisionIgnorePhysical(this);
						pCollider = this;
					} 

					// the ped ignoring this door ignores all weapons too
					if (pCollider == this)
					{
						phInst* pRagdollInst = pPed->GetRagdollInst();
						if(pRagdollInst)
						{
							int nLevelIndex = pRagdollInst->GetLevelIndex();
							if(CPhysics::GetLevel()->IsInLevel(nLevelIndex))
							{
								u32 includeFlags = CPhysics::GetLevel()->GetInstanceIncludeFlags(nLevelIndex);
								includeFlags &= ~(ArchetypeFlags::GTA_BASIC_ATTACHMENT_INCLUDE_TYPES);
								CPhysics::GetLevel()->SetInstanceIncludeFlags(nLevelIndex, includeFlags);
							}
						}
					}
				}
			}

			if( !m_Tuning || !m_Tuning->m_Flags.IsFlagSet( CDoorTuning::DontCloseWhenTouched ) )
			{
				//Only fully open if we have pushed the door x% open already.
				// B* 1078272
				dev_float bufferPercent = 0.15f;
				float ratio = GetDoorOpenRatio();
				float ratioAbs = Abs(ratio);
				if (ratioAbs >= bufferPercent)
				{
					float fSignRatio = Sign(ratio);
					//if we are touched yet we are on the other side of the door then make it open so it does not slam in our face... 
					// B* 934596 shows this behavior.
					// B* 1303566: if non player peds last contact was the door, m_fDoorTargetRatio was set to 1
					// This is wrong since the real target needs to be -1 if opening from the other side, vector also needs to be negated for the dot check
					Vector3 vecPosOffset = VEC3V_TO_VECTOR3(pOtherEntity->GetTransform().GetPosition() - GetTransform().GetPosition());
					Vector3 vDoorFwdTowardOpeningPed = VEC3V_TO_VECTOR3(GetTransform().GetB()) * fSignRatio;
					if(DotProduct(vecPosOffset, vDoorFwdTowardOpeningPed) > 0.0f)
					{
						m_fDoorTargetRatio = fSignRatio;
						// this will get overwritten for players below
						// the reason we need the delay is because non player peds 
						// can contact and force it open completely, and then 
						// it never gets its target set to 0.0 again
						SetCloseDelay(ms_fEntityDoorCloseDelay); 
					}
					else
					{
						m_fDoorTargetRatio = 0.0f;
					}
				}
			}
		}
		else if( nDoorType == DOOR_TYPE_STD && ( pOtherEntity->GetIsTypePed() || 
												 pOtherEntity->GetIsTypeVehicle() || 
												 pOtherEntity->GetIsTypeObject() ) )
		{
			CPhysical* pOtherPhysical = static_cast<CPhysical*>(pOtherEntity);
			// If the door is backwards in model space flip the forward orientation
			Vec3V vDoorForward = GetTransform().GetB();
			if( Abs( GetBoundingBoxMin().x ) >  Abs( GetBoundingBoxMax().x ) )
				vDoorForward = Negate( vDoorForward );

			// Determine if the hit was on the front or the backside
			Vec3V vToHitPosition = vHitPos - GetTransform().GetPosition();
			vToHitPosition = Normalize( vToHitPosition );
			bool bInFront = Dot( vDoorForward, vToHitPosition ).Getf() > 0.0f;
			float fOpenDoorRatio = GetDoorOpenRatio();

			bool bPassedDoorStateTest = false;

			// Is the player currently interacting with this door?
			PlayerDoorState ePlayerDoorState = GetPlayerDoorState();
			if( ePlayerDoorState != PDS_NONE )
			{
				bPassedDoorStateTest = (  bInFront && ePlayerDoorState == PDS_BACK  && fOpenDoorRatio <  sm_fMaxStuckDoorRatio ) || 
									   ( !bInFront && ePlayerDoorState == PDS_FRONT && fOpenDoorRatio > -sm_fMaxStuckDoorRatio );
			}
			// Otherwise just check the side
			else
			{
				bPassedDoorStateTest = (  bInFront && fOpenDoorRatio > 0.0f ) || 
									   ( !bInFront && fOpenDoorRatio < 0.0f );
			}

			// Check to see if the hit position is on the opposite side we are pushing
			if( bPassedDoorStateTest )
			{
				// Did we hit a ped?
				if( pOtherEntity->GetIsTypePed() )
				{
					CPed* pOtherPed = static_cast<CPed*>(pOtherEntity);
					if( !m_DoorControlFlags.IsSet( DOOR_CONTROL_FLAGS_STUCK ) && 
						( !m_Tuning || !m_Tuning->m_Flags.IsFlagSet( CDoorTuning::DontCloseWhenTouched ) ) )
					{
						m_fDoorTargetRatio = 0.0f;
						SetCloseDelay( 0.0f );

#if __BANK
						if( CDoorSystem::ms_logDoorPhysics &&
							m_DoorSystemData )
						{
							Vector3 doorPos = VEC3V_TO_VECTOR3(GetMatrix().GetCol3());
							physicsDisplayf( "CDoor::ProcessDoorImpact: hit ped (%f, %f, %f) %s\nTarget: %.2f\nCurRatio: %.2f", 
												doorPos.x, doorPos.y, doorPos.z, m_DoorSystemData->GetNetworkObject() ?  m_DoorSystemData->GetNetworkObject()->GetLogName() : "", 
												m_fDoorTargetRatio, GetDoorOpenRatio() );
						}
#endif // #if __BANK
					}

					// Otherwise this is an ambient ped and should be moved out of the way
					if( !pOtherPed->IsPlayer() && !pOtherPed->IsDead() )
					{
						pOtherPed->m_PedResetFlags.SetKnockedByDoor( 10 );
					}
				}
				// Only care about stuck cases when not already stuck and the player is using the door
				else if( !m_DoorControlFlags.IsSet( DOOR_CONTROL_FLAGS_STUCK ) && ePlayerDoorState != PDS_NONE )
				{
					m_fDoorTargetRatio = bInFront ? -1.0f : 1.0f;
					SetCloseDelay( ms_fPlayerDoorCloseDelay );
					m_DoorControlFlags.Set( DOOR_CONTROL_FLAGS_STUCK, true );
				}
			}

			// Handle applying a force to the other ped, to push them out of the way
			if( ePlayerDoorState != PDS_NONE )
			{
				TUNE_GROUP_FLOAT( BLOCKED_DOORS, fPushPed_DoorRatio, 0.8f, 0.0f, 1.0f, 0.01f );

				const bool bPassedPushStateTest =
					(  bInFront && ePlayerDoorState == PDS_BACK  && fOpenDoorRatio <  fPushPed_DoorRatio ) || 
					( !bInFront && ePlayerDoorState == PDS_FRONT && fOpenDoorRatio > -fPushPed_DoorRatio );

				if(bPassedPushStateTest)
				{
					bool bPushableEntity = false;
					float fAccelMag = 0.0f;
					float fMaxVelocity = 0.0f;
					if(pOtherEntity->GetIsTypePed())
					{
						TUNE_GROUP_FLOAT( BLOCKED_DOORS, fPedAccelMag, 1.0f, 0.0f, 100.0f, 0.01f);
						fAccelMag = fPedAccelMag;
						fMaxVelocity = FLT_MAX; // This should probably be lower but I don't want to change it if it's not a problem.

						CPed* pOtherPed = static_cast<CPed*>(pOtherEntity);

						// Otherwise this is an ambient ped and should be moved out of the way
						if(!pOtherPed->IsPlayer() && !pOtherPed->IsDead() )
						{
							bPushableEntity = true;
						}
					}
					else if(pOtherEntity->GetIsTypeVehicle())
					{
						TUNE_GROUP_FLOAT( BLOCKED_DOORS, fVehicleAccelMag, 25.0f , 0.0f, 100.0f, 0.01f);
						TUNE_GROUP_FLOAT( BLOCKED_DOORS, fVehicleMaxVelMag, 0.1f ,0.0f, 100.0f, 0.01f);
						fAccelMag = fVehicleAccelMag;
						fMaxVelocity = fVehicleMaxVelMag;
						const CVehicle* pOtherVehicle = static_cast<const CVehicle*>(pOtherEntity);
						if(pOtherVehicle->GetDriver() == NULL || !pOtherVehicle->GetDriver()->IsPlayer())
						{
							bPushableEntity = true;
						}
					}
					else if(pOtherEntity->GetIsTypeObject())
					{
						TUNE_GROUP_FLOAT( BLOCKED_DOORS, fObjectAccelMag, 25.0f , 0.0f, 100.0f, 0.01f);
						TUNE_GROUP_FLOAT( BLOCKED_DOORS, fObjectMaxVelMag, 0.1f ,0.0f, 100.0f, 0.01f);
						fAccelMag = fObjectAccelMag;
						fMaxVelocity = fObjectMaxVelMag;
						bPushableEntity = true;
					}

					if(bPushableEntity)
					{
						Vector3 accelDirection = -vNormal;
						if(pOtherPhysical->GetVelocity().Dot(accelDirection) < fMaxVelocity)
						{
							pOtherPhysical->ApplyForceCg( pOtherPhysical->GetMass() * accelDirection * fAccelMag );
						}
					}
				}
			}
		}

		if( pOtherEntity->GetIsTypeVehicle() )
		{
			if (m_Tuning && m_Tuning->m_BreakableByVehicle)
			{
				if(const phCollider* collider = pOtherEntity->GetCollider())
				{
					if(fragInst* pFragInst = GetFragInst())
					{
						if (fragCacheEntry* pEntry = pFragInst->GetCacheEntry())
						{
							if (pEntry->IsFurtherBreakable())
							{
								return;
							}
						}
					}
					// For now the impulse is just the momentum of the car. This won't break if the door slams into the car and
					//   can still break even if contact's relative velocity is zero. 
					// To get more accurate breaking here we would have to look at the local velocity and mass of both colliders
					//   as well as the impact normal. 
					float impulseSquared = MagSquared(collider->CalculateMomentum()).Getf();
					if(impulseSquared >= square(m_Tuning->m_BreakingImpulse))
					{
						// cars break non-scripted barriers
						BreakOffDoor();
					}
				}
			}
		}
	}
}


void CDoor::BreakOffDoor()
{
	// remove all constraints we created
	RemoveConstraints(GetCurrentPhysicsInst());

	bool needActivatePhysics = true;

	if(m_nDoorType != DOOR_TYPE_NOT_A_DOOR										// Don't do anything if we've already broken.
			&& Verifyf(!m_ClonedArchetype, "Door archetype already cloned."))	// Make sure we can never leak a phArchetype objects here.
	{
		m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen = false;

		phInst* pInst = GetCurrentPhysicsInst();

		phArchetype* pArchToModify = NULL;
		if(!m_nFlags.bIsFrag)
		{
			// Clone the archetype from the CBaseModelInfo.
			/*const*/ CBaseModelInfo* pModelInfo = GetBaseModelInfo();
			Assert(pModelInfo->HasPhysics());
			phArchetype* pNewArchBase = pModelInfo->GetPhysics()->Clone();
			Assert(pNewArchBase);

			// Make sure it's a phArchetypeDamp, as we expect.
			Assert(dynamic_cast<phArchetypeDamp*>(pNewArchBase));
			phArchetypeDamp* pNewArch = static_cast<phArchetypeDamp*>(pNewArchBase);

			// Store the pointer to it, and add to the reference count.
			Assert(pNewArch);
			pNewArch->AddRef();
			m_ClonedArchetype = pNewArch;

			phBound* pNewBound = pNewArch->GetBound();

			// If it's a phBoundComposite, we could clone the individual bound parts too.
			// This is probably not necessary, since the only thing in the phBound that
			// we will change is the array of per-part type/include flags.
			//	if(pNewBound->GetType() == phBound::COMPOSITE)
			//	{
			//		static_cast<phBoundComposite*>(pNewBound)->CloneParts();
			//	}

			// Since we're going to mess with things like the archetype pointer in
			// the phInst, locking the level may be necessary to be safe in case there
			// are probes going on in another thread.
#if ENABLE_PHYSICS_LOCK
			// NOTE: I'm not really sure that this is necessary here (I think it's not).  But it seems like PHLEVEL->UpdateObjectPositionAndRadius()
			//   should be getting called after these modifications are made.
			PHLOCK_SCOPEDWRITELOCK;
#endif	// ENABLE_PHYSICS_LOCK

			// Update the archetype pointer in the instance.
			pInst->SetArchetype(pNewArch);

			// Reset the angular velocity damping to zero, which is what it would have been if it had not been a door.
			pNewArch->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(0.0f,0.0f,0.0f));

			// The door code tends to move the center of gravity to a point on the pivot axis,
			// so we may need to move it back for the physics simulation to work naturally.
			// Note that we probably would have lost any center of gravity information from the .bnd
			// file at this point.
			if(pNewBound->GetType() == phBound::COMPOSITE)
			{
				// For composite bounds, recompute from the parts.
				static_cast<phBoundComposite*>(pNewBound)->CalcCenterOfGravity();
			}
			else
			{
				// For other bounds, use the centroid. Should be fine for common door
				// primitives, like boxes.
				pNewBound->SetCGOffset(pNewBound->GetCentroidOffset());
			}

			// Update the instance in the level.
			if(pInst->IsInLevel())
			{
				// If we're already active, we may need to fix up the collider too, as we may
				// have moved the center of mass and made other adjustments.
				phCollider* collider = PHSIM->GetCollider(pInst);
				if(collider)
				{
					collider->SetColliderMatrixFromInstance();

					// Load the mass and inertial from the archetype into the collider.
					collider->InitInertia();

					// Recompute momentum by setting velocity.
					collider->SetVelocity(collider->GetVelocity().GetIntrin128());
					collider->SetAngVelocity(collider->GetAngVelocity().GetIntrin128());
				}
			}

			// We have cloned the phArchetype, so now we have one we are allowed to change.
			pArchToModify = pInst->GetArchetype();
		}
		else
		{
			// For a fragInst, we need to call ActivatePhysics() first, so the object gets put in
			// the cache so we can modify the phArchetype.
			ActivatePhysics();
			needActivatePhysics = false;

			// Update pInst, in case the call to ActivatePhysics() can change it.
			pInst = GetCurrentPhysicsInst();
			Assert(pInst);

			// Get the phArchetype pointer if we got put in the cache.
			fragInst* pFragInst = GetFragInst();
			if(pFragInst)
			{
				fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry();
				if(pCacheEntry)
				{
					Assert(pInst == pFragInst);
					pArchToModify = pInst->GetArchetype();

					// fragCacheEntry::SetMass() does more than just setting the mass value,
					// which we make use of here. Specifically, it should restore the center
					// of gravity.
					pCacheEntry->SetMass(pArchToModify->GetMass());
				}
			}

			// Note: it's possible that we want to adapt more code here from the
			// non-fragInst case, for setting the centre of mass, etc.	
		}

		// If we have a phArchetype/phBound that we are allowed to modify, update the include flags.
		if(pArchToModify)
		{
			// Set the include flags on the cloned archetype to what we use for regular objects,
			// to make the broken off door collide with the world again. 
			pArchToModify->SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);

			// If this is a phBoundComposite, we may also have to fix up the include flags of
			// its parts in order for it to not fall through the ground.
			// Note: this may need some work if we need camera-only parts, etc.
			// Note 2: not 100% sure that this is needed in the fragInst case, but probably is.
			phBound* pBound = pArchToModify->GetBound();
			if(pBound->GetType() == phBound::COMPOSITE)
			{
				phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pBound);
				if(pBoundComp->GetTypeAndIncludeFlags())
				{
					for(int i = 0; i < pBoundComp->GetNumBounds(); ++i)
					{
						pBoundComp->SetIncludeFlags(i, ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);
					}
				}
			}
		}

		// Set the instance to not be a door.
		pInst->SetInstFlag(phInstGta::FLAG_IS_DOOR, false);

		// We may need to update the include flags in the level.
		if(pInst->IsInLevel())
		{
			PHLEVEL->SetInstanceIncludeFlags(pInst->GetLevelIndex(), pInst->GetArchetype()->GetIncludeFlags());

			// Reset the contact warm start since we're changing the collider properties so much, otherwise wonkyness ensues. 
			PHSIM->GetContactMgr()->ResetWarmStartAllContactsWithInstance(pInst);
		}
	}

	// If we haven't called ActivatePhysics() yet, do so now. Note that for non-fragInst doors,
	// it might be better to do this here like this rather than at the beginning, so we don't have to
	// first set up a collider, then modify the phArchetype and have to update the collider again.
	if(needActivatePhysics)
	{
		ActivatePhysics();
	}

	// Turn it into a 'non door' type
	m_nDoorType = DOOR_TYPE_NOT_A_DOOR;
}

float CDoor::GetEffectiveAutoOpenRate() const
{
	float fRate = 1.0f;
	if(m_fAutomaticRate != 0.0f)
	{
		fRate = m_fAutomaticRate;
	}
	else if(Verifyf(m_Tuning, "Expected door tuning data."))
	{
		fRate = m_Tuning->m_AutoOpenRate;
	}
	return fRate;
}


void CDoor::GetSlidingDoorAxisAndLimit(Vec3V_Ref axisOut, float &limitOut) const
{
	bool verticalDoor = (m_nDoorType == DOOR_TYPE_SLIDING_VERTICAL || m_nDoorType == DOOR_TYPE_SLIDING_VERTICAL_SC);
	if(verticalDoor)
	{
		axisOut = GetTransform().GetC();
		limitOut = GetBoundingBoxMax().z;
	}
	else
	{
		Assert(m_nDoorType == DOOR_TYPE_SLIDING_HORIZONTAL || m_nDoorType == DOOR_TYPE_SLIDING_HORIZONTAL_SC);

		axisOut = GetTransform().GetA();
		limitOut = Max(Abs(GetBoundingBoxMin().x),Abs(GetBoundingBoxMax().x));
	}
}

// Make these inline
void CDoor::RemoveDoorFromList(CEntity *pDoor)
{
	fwPtrNode *pPrev = NULL;
	for (fwPtrNode *pNode = ms_doorsList.GetHeadPtr(); pNode != NULL; pPrev = pNode, pNode = pNode->GetNextPtr())
	{
		if (pNode->GetPtr() == (void*)pDoor)
		{
			((fwPtrNodeSingleLink*)pNode)->Remove(ms_doorsList, (fwPtrNodeSingleLink*)pPrev);
			delete pNode;
			break;
		}
	}
}

void CDoor::AddDoorToList(CEntity *pDoor) 
{	
	for (fwPtrNode *pNode = ms_doorsList.GetHeadPtr(); pNode != NULL; pNode = pNode->GetNextPtr())
	{
		if (pNode->GetPtr() == (void*)pDoor)
		{
			return;
		}
		else if (pDoor->GetIsTypeDummyObject())
		{
			CEntity* pEntityDoor = reinterpret_cast<CEntity*>(pNode->GetPtr());			
			if (pEntityDoor->GetIsTypeObject())
			{
				CDoor* pListDoor = static_cast<CDoor*>(pEntityDoor);			
				if (pListDoor->GetRelatedDummy() && pListDoor->GetRelatedDummy() == pDoor)
				{
					return;
				}
			}

		}
	}

	if (pDoor->GetIsTypeDummyObject())
	{
		ms_doorsList.Add(pDoor); 
	}
	else
	{
		if (pDoor->GetIsTypeObject())
		{
			CObject *pDoorObject = static_cast<CObject*>(pDoor);
			if (pDoorObject->GetRelatedDummy())
			{
				RemoveDoorFromList(pDoorObject->GetRelatedDummy());
			}
		
			ms_doorsList.Add(pDoor);
		}
	}
}

CEntity* CDoor::FindClosestDoor(CBaseModelInfo *pModelInfo, const Vector3 &position, float radius)
{
	CEntity* pClosestDoor = NULL;
	float closestDist = 0.0f;

	float radiusSqrd = radius * radius;
	for (fwPtrNode *pNode = ms_doorsList.GetHeadPtr(); pNode != NULL; pNode = pNode->GetNextPtr())
	{
		CEntity *pDoor = (CEntity*)pNode->GetPtr();
		if (pDoor->GetBaseModelInfo() == pModelInfo)
		{
			Vector3 doorPos = VEC3V_TO_VECTOR3(pDoor->GetTransform().GetPosition());
			float distanceSqrd = (position - doorPos).Mag2();
			if (distanceSqrd < radiusSqrd)
			{
				if (!pClosestDoor || distanceSqrd < closestDist)
				{
					pClosestDoor = pDoor;
					closestDist = distanceSqrd;
				}
			}
		}
	}

	return pClosestDoor;
}

void CDoor::OpenAllBarriersForRace(bool snapOpen)
{
	if (!ms_openDoorsForRacing)
	{
		ms_openDoorsForRacing = true;
		ms_snapOpenDoorsForRacing = snapOpen;

		for (fwPtrNode *pNode = ms_doorsList.GetHeadPtr(); pNode != NULL; pNode = pNode->GetNextPtr())
		{
			CEntity *pEntityDoor = (CEntity*)pNode->GetPtr();
			if (pEntityDoor->GetIsTypeObject())
			{
				CDoor *pDoor = (CDoor*)pEntityDoor;
				CDoor::ProcessRaceOpenings(*pDoor, ms_snapOpenDoorsForRacing);
			}
		}
	}
#if __BANK
	else
	{
		Displayf("OpenAllDoorsForRace() was called but race state was not none");
	}
#endif
}

void CDoor::CloseAllBarriersOpenedForRace()
{
	if (ms_openDoorsForRacing)
	{
		ms_openDoorsForRacing = false;
		for (fwPtrNode *pNode = ms_doorsList.GetHeadPtr(); pNode != NULL; pNode = pNode->GetNextPtr())
		{
			CEntity *pEntityDoor = (CEntity*)pNode->GetPtr();
			if (pEntityDoor->GetIsTypeObject())
			{
				CDoor *pDoor = (CDoor*)pEntityDoor;
				u8 doorType = pDoor->m_nDoorType;
				bool openForRacetype = doorType == DOOR_TYPE_BARRIER_ARM	|| doorType == DOOR_TYPE_RAIL_CROSSING_BARRIER ||
									   doorType == DOOR_TYPE_BARRIER_ARM_SC || doorType == DOOR_TYPE_RAIL_CROSSING_BARRIER_SC;
				CDoorSystemData*pData = pDoor->GetDoorSystemData();
				if (openForRacetype || (pData && pData->GetOpenForRaces()))
				{
					if (ms_snapOpenDoorsForRacing)
					{
						pDoor->SetFixedPhysics(false, false);
					}
					pDoor->CloseDoor();
				}
			}
		}

		ms_snapOpenDoorsForRacing = false;
	}
#if __BANK
	else
	{
		Displayf("CloseAllDoorsForRace() was called but race state was not racing");
	}
#endif

}

void CDoor::ProcessRaceOpenings(CDoor& door, const bool snapOpen)
{
	int doorType = door.GetDoorType();

	const bool railBarrier = (doorType == DOOR_TYPE_RAIL_CROSSING_BARRIER || doorType == DOOR_TYPE_RAIL_CROSSING_BARRIER_SC);
	const bool normBarrier = (doorType == DOOR_TYPE_BARRIER_ARM || doorType == DOOR_TYPE_BARRIER_ARM_SC);
	const bool openForRacetype = (normBarrier || railBarrier);

	CDoorSystemData *pData = door.GetDoorSystemData();
	if (openForRacetype || (pData && pData->GetOpenForRaces()))
	{
		//Rail crossing barriers are open at ratio 0.0 so all we need to do is lock them down ... 
		if (railBarrier)
		{
			if (snapOpen)
			{
				float oldTarget = door.m_fDoorTargetRatio;
				door.SetTargetDoorRatio(0.0f, true);
				door.SetFixedPhysics(true, false);
				door.SetTargetDoorRatio(oldTarget, false);
			}
		}
		else
		{
			door.OpenDoor();
			if (snapOpen)
			{
				float oldTarget = door.m_fDoorTargetRatio;
				float fTarget;
				if(door.GetDoorType()==DOOR_TYPE_SLIDING_HORIZONTAL)
				{
					if(rage::Abs(door.GetBoundingBoxMin().x) > rage::Abs(door.GetBoundingBoxMax().x))
						fTarget = 1.0f;
					else
						fTarget = -1.0f;
				}
				else
				{
					fTarget = 1.0f;
				}

				door.SetTargetDoorRatio(fTarget, true);
				door.SetFixedPhysics(true, false);
				door.SetTargetDoorRatio(oldTarget, false);
			}
		}
	}
}

void CDoor::ForceOpenIntersectingDoors(CPhysical **objectsToCheck, unsigned numObjectsToCheck, bool preventScriptControl)
{
	for (fwPtrNode *pNode = ms_doorsList.GetHeadPtr(); pNode != NULL; pNode = pNode->GetNextPtr())
	{
		CEntity *pDoorEntity = (CEntity*)pNode->GetPtr();
		if (pDoorEntity->GetIsTypeObject())
		{
			CDoor *pDoor = SafeCast(CDoor, pDoorEntity);
			if (pDoor->GetDoorType()!= DOOR_TYPE_NOT_A_DOOR)
			{
				for(unsigned index = 0; index < numObjectsToCheck; index++)
				{
					CPhysical *physical = objectsToCheck[index];

					spdAABB tempBox1;
					spdAABB tempBox2;
					if(physical && physical->IsVisible() && physical->IsCollisionEnabled() && physical->GetBoundBox(tempBox1).IntersectsAABB(pDoorEntity->GetBoundBox(tempBox2)))
					{
						CDoorSystemData *pData = pDoor->GetDoorSystemData();

						bool inUseByScript = false;

						// If script has locked the door or it has an open ratio thats not 0 its in used by script
						if (pData && (pData->GetLocked() || pData->GetTargetRatio() != 0.0f))
						{
							inUseByScript = true;
						}

						if(!inUseByScript)
						{
							pDoor->ScriptDoorControl(false, 1.0f, true, preventScriptControl);
						}
					}
				}
			}
		}
	}
}

void CDoor::ResetCenterOfGravityForFragDoor( fragInst * pFragInst )
{
	// For fragInsts, the fragment system likes to recompute the center of gravity,
	// while the door code wants it to be at the local origin. Having it there helps
	// because we don't have to worry about gravity, etc, though we may be able to
	// get away without it. Until we can find a better way, this code resets the center
	// of gravity if the fragment system moves it.
	if(pFragInst && pFragInst->GetCached() && pFragInst == GetCurrentPhysicsInst())
	{
		phArchetype* pArch = pFragInst->GetArchetype();
		phBound* pBnd = pArch->GetBound();
		const Vec3V offsV = pBnd->GetCGOffset();
		const Vec3V thresholdV(V_FLT_SMALL_6);
		if(!IsLessThanAll(offsV, thresholdV))
		{
			pBnd->SetCGOffset(Vec3V(V_ZERO));

			// Update the instance in the level.
			if(pFragInst->IsInLevel())
			{
				// If we're already active, we may need to fix up the collider too, as we may
				// have moved the center of mass and made other adjustments.
				phCollider* collider = PHSIM->GetCollider(pFragInst);
				if(collider)
				{
					collider->SetColliderMatrixFromInstance();

					// Load the mass and inertial from the archetype into the collider.
					//	collider->InitInertia();

					// Recompute momentum by setting velocity.
					collider->SetVelocity(collider->GetVelocity().GetIntrin128());
					collider->SetAngVelocity(collider->GetAngVelocity().GetIntrin128());
				}
			}
		}
	}
}

#if __DEV

void CDoorSystemData::AllocDebugMemForName()
{
	sysMemAllocator* debugAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL);
	m_pScriptName = (char*)debugAllocator->Allocate(DOOR_SCRIPT_NAME_LENGTH, 4);
	memset(m_pScriptName, 0, DOOR_SCRIPT_NAME_LENGTH);
}
void CDoorSystemData::FreeDebugMemForName()
{
	sysMemAllocator* debugAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL);
	debugAllocator->Free(m_pScriptName);
}
#endif //__DEV

CDoorSystemData *CDoorSystem::CreateNewDoorSystemData(int doorEnumHash, int modelInfoHash, const Vector3 &position)
{
	CDoorSystemData* pDoorData = rage_new CDoorSystemData(doorEnumHash, modelInfoHash, position);

#if ENABLE_NETWORK_LOGGING
	if(NetworkInterface::IsGameInProgress())
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CREATING_DOOR_SYSTEM", "%u", doorEnumHash);
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Position", "x:%.2f, y:%.2f, z:%.2f", position.x, position.y, position.z);
	}
#endif // ENABLE_NETWORK_LOGGING

	ms_doorMap.Insert(doorEnumHash, pDoorData);	
	u32 posHash = CDoorSystem::GenerateHash32(position);
	ms_lockedMap.Insert(posHash, pDoorData);	
#if __ASSERT
	if (ms_doorMap.GetNumUsed() >= MAX_DOORSYSTEM_MAP_SIZE)
	{
		Displayf("CDoorSystem::CreateNewDoorSystemData - big map of doors is full - %d", MAX_DOORSYSTEM_MAP_SIZE);

		for (CDoorSystem::DoorSystemMap::Iterator doorIterator = ms_doorMap.CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
		{
			CDoorSystemData *pDoorData = doorIterator.GetData();

			fwModelId objectModelId;
			CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfoFromHashKey((u32) pDoorData->GetModelInfoHash(), &objectModelId);		//	ignores return value
			const Vector3 &position = pDoorData->GetPosition();
			
			Displayf("Door - Model Hash %u, Name %s, %.3f, %.3f, %.3f, Added by %s", pDoorData->GetModelInfoHash(), pModel ? pModel->GetModelName() : "Model Lookup failed", position.x, position.y, position.z, pDoorData->m_pScriptName);
		}

		Assertf(ms_doorMap.GetNumUsed() < MAX_DOORSYSTEM_MAP_SIZE, "CDoorSystem::CreateNewDoorSystemData - big map of doors is full");
	}
#endif
	// find the corresponding physical door if it exists
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(modelInfoHash, NULL);
	if (pModelInfo)
	{
		float radius = CDoor::GetScriptSearchRadius(*pModelInfo);

		// This hacky code is here to fix this bug: url:bugstar:7359033
		// The GetScriptSearchRadius returns 1.75 for this door which is way too large and causes the code to pickup the wrong door.
		// Proper fix would be to not add 1meter to the radius of DOOR_TYPE_SLIDING_HORIZONTAL type doors but that is risky at this point
		bool check_radius_override = NetworkInterface::IsGameInProgress() && Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("USE_DOORSYSTEM_RADIUS_OVERRIDE", 0x6B25D763), true);
		if(check_radius_override
		&& pModelInfo->GetModelNameHash() == ATSTRINGHASH("v_ilev_garageliftdoor", 0xB614B4EF))
		{
			const float MIN_SQR_DIST_TO_LIFT = 1.0f;
			const Vector3 LIFT_DOOR_POS(2519.433105, -260.631012, -40.126999);
			if(position.Dist2(LIFT_DOOR_POS) < MIN_SQR_DIST_TO_LIFT)
			{
				radius = 1.4f;
			}
		}

		CEntity* pDoor = CDoor::FindClosestDoor(pModelInfo, position, radius);

		if (pDoor && pDoor->GetType() == ENTITY_TYPE_OBJECT)
		{
			Assert(dynamic_cast<CDoor*>(pDoor));
			pDoorData->SetDoor(static_cast<CDoor*>(pDoor));
		}
	}	
#if __DEV
	const char *pScriptName = CTheScripts::GetCurrentScriptNameAndProgramCounter();
	if (pScriptName)
	{
		const int nullOffset = CDoorSystemData::DOOR_SCRIPT_NAME_LENGTH - 1;
		strncpy(pDoorData->m_pScriptName, pScriptName, nullOffset);
		pDoorData->m_pScriptName[nullOffset] = '\0';
	}
#endif
	return pDoorData;
}

CDoorSystemData *CDoorSystem::FindDoorSystemData(const Vector3 &position, int modelHash, float radius)
{
	CDoorSystemData *pClosestDoorData = NULL;
	float closestDist = 0.0f;

	float radiusSqrd = radius * radius;

	for (DoorSystemMap::Iterator doorIterator = ms_doorMap.CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
	{
		CDoorSystemData &doorData = *doorIterator.GetData();

		if (modelHash == doorData.GetModelInfoHash())
		{
			float distanceSqrd = (position - doorData.GetPosition()).Mag2();
			if (distanceSqrd < radiusSqrd)
			{
				if (!pClosestDoorData || distanceSqrd < closestDist)
				{
					pClosestDoorData = &doorData;
					closestDist = distanceSqrd;
				}
			}
		}
	}

	return pClosestDoorData;
}

CDoorSystemData* CDoorSystem::AddDoor(int doorEnumHash, int modelInfoHash, const Vector3& position)
{
	CDoorSystemData* pDoorData = NULL;
	CDoorSystemData **ppData = ms_doorMap.Access(doorEnumHash);	
	if (ppData)
	{
		pDoorData = *ppData;
		u32 oldHash = CDoorSystem::GenerateHash32(pDoorData->GetPosition());
		u32 newHash = CDoorSystem::GenerateHash32(position);

		if (oldHash != newHash)
		{
			ms_lockedMap.Delete(oldHash);

			pDoorData->SetModelInfoHash(modelInfoHash);
			pDoorData->SetPosition(position);

			ms_lockedMap.Insert(newHash);	
		}
	}
	else 
	{
		u32 posHash32 = CDoorSystem::GenerateHash32(position);
		CDoorSystemData **ppLockedData = ms_lockedMap.Access(posHash32);
		if (!ppLockedData)
		{
			CDoorSystemData *pData = FindDoorSystemData(position, modelInfoHash, 0.5f);
			if (pData)
			{
				if (NetworkInterface::IsGameInProgress())
				{
					Assertf(0, "CDoorSystem::AddDoor - door system data (%s) already exists at this position (%0.2f, %0.2f, %0.2f)", pData->GetNetworkObject() ? pData->GetNetworkObject()->GetLogName() : "?", position.x, position.y, position.z);
					return NULL;
				}
				else
				{
					Warningf("Position hash look up for Door failed, but a door within 20 cm of that position was found, Updating position");
					pDoorData = pData;
				}
			}
		}

		if (pDoorData)
		{
			u32 oldPosHash = CDoorSystem::GenerateHash32(pDoorData->GetPosition());
			Assert(pDoorData->GetEnumHash() == (int)oldPosHash);

#if ENABLE_NETWORK_LOGGING
			if(NetworkInterface::IsGameInProgress())
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "REPLACING_ON_ADD_DOOR_SYSTEM", "%u", pDoorData->GetEnumHash());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("New Hash", "%u", posHash32);
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Position", "x:%.2f, y:%.2f, z:%.2f", position.x, position.y, position.z);
			}
#endif // ENABLE_NETWORK_LOGGING

			ms_doorMap.Delete(pDoorData->GetEnumHash());
			ms_lockedMap.Delete(oldPosHash);
			
			pDoorData->SetPosition(position);
			pDoorData->SetEnumHash(doorEnumHash);
			pDoorData->SetModelInfoHash(modelInfoHash);
		
			ms_lockedMap.Insert(posHash32, pDoorData);
			ms_doorMap.Insert(doorEnumHash, pDoorData);
		}
		else
		{
			pDoorData = CreateNewDoorSystemData(doorEnumHash, modelInfoHash, position);
		}
	}

	return pDoorData;
}

CDoorSystemData* CDoorSystem::AddSavedDoor(int doorEnumHash, int modelInfoHash, DoorScriptState state, const Vector3& position)
{
	CDoorSystemData* pDoorData = NULL;
	CDoorSystemData **ppData = ms_doorMap.Access(doorEnumHash);	
	if (ppData)
	{
		pDoorData = *ppData;
		if (pDoorData->GetState() != state)
		{
			pDoorData->SetPendingState(state);
		}
	}
	else
	{
		pDoorData = CreateNewDoorSystemData(doorEnumHash, modelInfoHash, position);
		pDoorData->SetState(state);
	}

	return pDoorData;
}

CDoorSystemData* CDoorSystem::AddDoor(const Vector3 &pos, u32 modelHash)
{
	u32 posHash32 = CDoorSystem::GenerateHash32(pos);

	CDoorSystemData* pDoorData = NULL;
	CDoorSystemData **ppData = ms_lockedMap.Access(posHash32);

	if (!ppData)
	{
		CDoorSystemData *pData = FindDoorSystemData(pos, modelHash, 0.5f);
		if (pData)
		{
			if (NetworkInterface::IsGameInProgress())
			{
				Assertf(0, "CDoorSystem::AddDoor - door system data (%s) already exists at this position (%0.2f, %0.2f, %0.2f)", pData->GetNetworkObject() ? pData->GetNetworkObject()->GetLogName() : "?", pos.x, pos.y, pos.z);
				return NULL;
			}
			else
			{
				Warningf("Position hash look up for Door failed, but a door within 20 cm of that position was found, Updating position");
				pDoorData = pData;
			}
		}
	}

	if (!pDoorData)
	{
		pDoorData = CreateNewDoorSystemData(posHash32, modelHash, pos);
	}
	else
	{
		u32 oldHash = CDoorSystem::GenerateHash32(pDoorData->GetPosition());

		ms_lockedMap.Delete(oldHash);

		pDoorData->SetPosition(pos);
		ms_lockedMap.Insert(posHash32, pDoorData);

		// if we are using a generated enum hash
		if (oldHash == static_cast<u32>(pDoorData->GetEnumHash()))
		{
#if ENABLE_NETWORK_LOGGING
			if(NetworkInterface::IsGameInProgress())
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "REPLACING_DOOR_SYSTEM", "%u", pDoorData->GetEnumHash());
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("New Hash", "%u", modelHash);
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Position", "x:%.2f, y:%.2f, z:%.2f", pos.x, pos.y, pos.z);
			}
#endif // ENABLE_NETWORK_LOGGING
			ms_doorMap.Delete(pDoorData->GetEnumHash());
			pDoorData->SetEnumHash(posHash32);
			ms_doorMap.Insert(posHash32, pDoorData);
		}
	}

	Assertf(ms_doorMap.GetNumUsed() < MAX_DOORSYSTEM_MAP_SIZE, "CDoorSystem::AddSavedDoor - big map of saved doors is full");

	return pDoorData;
}

void CDoorSystem::RemoveDoor(int doorEnumHash)
{
	CDoorSystemData **ppData = ms_doorMap.Access(doorEnumHash);	

	Assertf(ppData, "CDoorSystem::RemoveDoor - Failed to find door");

	if (ppData)
	{
		RemoveDoor(*ppData);
	}
}

void CDoorSystem::RemoveDoor(CDoorSystemData *pDoorData)
{
	// unlock the door  
	CDoor* pDoor = pDoorData->GetDoor();

	if (pDoor && pDoor->GetDoorType() != CDoor::DOOR_TYPE_NOT_A_DOOR)
	{
		bool bLock = pDoor->GetAndClearDoorControlFlag(CDoor::DOOR_CONTROL_FLAGS_LOCK_WHEN_REMOVED);
		pDoor->ScriptDoorControl(bLock, pDoor->GetDoorOpenRatio());
		pDoor->ClearUnderCodeControl();
	}

	u32 lockedHash = CDoorSystem::GenerateHash32(pDoorData->GetPosition());
	ms_lockedMap.Delete(lockedHash);

	ms_doorMap.Delete(pDoorData->GetEnumHash());

	if (pDoorData->GetScriptHandler())
	{
		pDoorData->GetScriptHandler()->UnregisterScriptObject(*pDoorData);
	}

#if ENABLE_NETWORK_LOGGING
	if(NetworkInterface::IsGameInProgress())
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "REMOVING_DOOR_SYSTEM", "%u", pDoorData->GetEnumHash());
	}
#endif // ENABLE_NETWORK_LOGGING

	delete pDoorData;
}

CDoorSystemData *CDoorSystem::SetLocked(const Vector3 &position, bool bIsLocked, float targetRatio, bool bUnsprung, u32 modelHash, 
													float automaticRate, float automaticDist, CDoor* pDoor,  bool useOldOverrides)
{
	CDoorSystemData *pData = NULL;

	if (pDoor && pDoor->GetDoorSystemData())
	{
		pData = pDoor->GetDoorSystemData();
	}
	else
	{
		u32 posHash32 = CDoorSystem::GenerateHash32(position);
		CDoorSystemData **ppData = ms_lockedMap.Access(posHash32);
		if (!ppData)
		{
			AddDoor(position, modelHash);
			ppData = ms_lockedMap.Access(posHash32);
			Assertf(ppData, "Failed to create Lock Object Status Door");
		}

		if (ppData)
		{
			pData = *ppData;
		}
	}

	if (pData)
	{
		pData->SetTargetRatio(targetRatio);
		pData->SetUnsprung(bUnsprung);
		
		pData->SetAutomaticRate(automaticRate);
		pData->SetAutomaticDistance(automaticDist);

		pData->SetPendingState(DOORSTATE_INVALID);
		pData->SetState(bIsLocked ? DOORSTATE_LOCKED : DOORSTATE_UNLOCKED);
		pData->SetUseOldOverrides(useOldOverrides);
	}

	return pData;
}

void CDoorSystem::SwapModel(u32 oldModelHash, u32 newModelHash, Vector3& swapCentre, float swapRadiusSqr)
{
	for (DoorSystemMap::Iterator doorIterator = ms_doorMap.CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
	{
		CDoorSystemData &doorData = *doorIterator.GetData();

		if (doorData.GetModelInfoHash() == static_cast<int>(oldModelHash))
		{
			Vector3 diff = doorData.GetPosition() - swapCentre;

			if (diff.GetHorizontalMag2() < swapRadiusSqr)
			{
				doorData.SetModelInfoHash(newModelHash);
			}
		}
	}
}

CDoorSystemData* CDoorSystem::GetDoorData(int doorEnumHash) 
{ 
	CDoorSystemData **ppData = ms_doorMap.Access(doorEnumHash); 
	if (ppData)
	{
		return *ppData;
	}

	return NULL;
}

CDoorSystemData* CDoorSystem::GetDoorData(const Vector3 &pos)
{
	CDoorSystemData **ppData = ms_lockedMap.Access(CDoorSystem::GenerateHash32(pos));
	if (ppData)
	{
		return *ppData;
	}

	return NULL;
}

CDoorSystemData* CDoorSystem::GetDoorData(u32 posHash)
{
	CDoorSystemData **ppData = ms_lockedMap.Access(posHash);

	if (ppData)
	{
		return *ppData;
	}

	return NULL;
}

CDoorSystemData* CDoorSystem::GetDoorData(CEntity* pEntity) 
{
	Assert(pEntity);
	CDoorSystemData **ppData = ms_lockedMap.Access(CDoorSystem::GenerateHash32(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition())));

	if (ppData)
	{
		return *ppData;
	}

	return NULL;
}

void CDoorSystem::LinkDoorObject(CDoor& door)
{
	Vector3 doorPos = VEC3V_TO_VECTOR3(door.GetTransform().GetPosition());

	CDoorSystemData *pDoorData = door.GetDoorSystemData();
	if (!pDoorData)
	{
		float searchRadius = CDoor::GetScriptSearchRadius(*door.GetBaseModelInfo());
		pDoorData = FindDoorSystemData(doorPos, door.GetBaseModelInfo()->GetHashKey(), searchRadius);
	}

	if (pDoorData && AssertVerify(door.GetDoorType() != CDoor::DOOR_TYPE_NOT_A_DOOR))
	{
		if (pDoorData->GetDoor())
		{
			CDoor *pCurrentDoor = pDoorData->GetDoor();
			Vector3 currentDoorPosition = VEC3V_TO_VECTOR3(pCurrentDoor->GetTransform().GetPosition());
			Vector3 newDoorPosition = VEC3V_TO_VECTOR3(door.GetTransform().GetPosition());
			float distToCurrDoorSqud = (currentDoorPosition - pDoorData->GetPosition()).Mag2();
			float distToNewDoorSqud		 = (newDoorPosition		- pDoorData->GetPosition()).Mag2();
			if (distToCurrDoorSqud < distToNewDoorSqud)
			{
				return;
			}
		}

		pDoorData->SetDoor(&door);

		DoorScriptState doorState = pDoorData->GetPendingState() != DOORSTATE_INVALID ? pDoorData->GetPendingState() : pDoorData->GetState();

		// set the current state to pending so that it is applied to the new door
		if (doorState != DOORSTATE_UNLOCKED)
		{
			pDoorData->SetPendingState(doorState);
			pDoorData->SetState(DOORSTATE_INVALID);
			SetPendingStateOnDoor(*pDoorData);
		}

		// If the door does not contain a Frag Inst its either not breakable or has been reset to unbroken.
		if (!door.GetFragInst())
		{
			pDoorData->SetBrokenFlags(0);
		}
		else
		{
			// if the door is broken, break it now
			DoorBrokenFlags brokenFlags = pDoorData->GetBrokenFlags();
			if (brokenFlags != 0 )
			{
				BreakDoor(door, brokenFlags, true);
			}

			DoorDamagedFlags damagedFlags = pDoorData->GetDamagedFlags();
			if (damagedFlags != 0)
			{
				DamageDoor(door, damagedFlags);
			}
		}

		if (door.GetHoldingOpen() != pDoorData->GetHoldOpen())
		{
			if (Verifyf(!door.GetUnderCodeControl(), "Door is under code control holding open changes will be ignored (%.2f,%.2f,%.2f)", doorPos.GetX(), doorPos.GetY(), doorPos.GetZ()))
			{
				door.SetHoldingOpen(pDoorData->GetHoldOpen());
			}
		}
	}
}

void CDoorSystem::UnlinkDoorObject(CDoor& door)
{
	if (door.GetDoorSystemData())
	{
		door.GetDoorSystemData()->SetDoor(NULL);
	}
	else if (door.GetNetworkObject())
	{
		NetworkInterface::UnregisterObject(&door);
	}

#if __ASSERT
	for (DoorSystemMap::Iterator doorIterator = ms_doorMap.CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
	{
		CDoorSystemData &doorData = *doorIterator.GetData();

		if (doorData.GetDoor() == &door)
		{
			Assertf(0, "CDoorSystem::UnlinkDoorObject - door system entry pointing to a physical door which had no ptr back to the entry");
			doorData.SetDoor(NULL);
		}
	}
#endif //__ASSERT
}

void CDoorSystem::SetPendingStateOnDoor(CDoorSystemData &doorData)
{
	CDoor* pDoor = doorData.GetDoor();

	bool bSuccess = false;

	if (pDoor && pDoor->GetDoorType() != CDoor::DOOR_TYPE_NOT_A_DOOR)
	{
		if (IGNORE_OLD_OVERIDES || !doorData.GetUseOldOverrides())
		{	
			DoorScriptState pendingState = doorData.GetPendingState();
			bool locked = !(pendingState == DOORSTATE_UNLOCKED || 
							pendingState == DOORSTATE_FORCE_UNLOCKED_THIS_FRAME || 
							pendingState == DOORSTATE_FORCE_OPEN_THIS_FRAME);

			bool forceOpenRatio = pendingState == DOORSTATE_FORCE_LOCKED_THIS_FRAME || 
								  pendingState == DOORSTATE_FORCE_CLOSED_THIS_FRAME ||
								  pendingState == DOORSTATE_FORCE_UNLOCKED_THIS_FRAME || 
								  pendingState == DOORSTATE_FORCE_OPEN_THIS_FRAME;

			bool smoothLock = locked && pendingState != DOORSTATE_FORCE_LOCKED_THIS_FRAME;
			if(fabsf(doorData.GetTargetRatio()) > SMALL_FLOAT)
			{
				// In this case, the door is not actually requested to close. Make sure
				// to not activate the smooth close-and-lock feature, which could conflict
				// with the request to be open.
				smoothLock = false;
			}

			bool removeSpring = doorData.GetUnsprung();
			if (removeSpring && locked)
			{
				removeSpring = false;
				Assertf(0, "SCRIPT - Cannot lock a door with spring removed");
			}

			ASSERT_ONLY(CDoor::ChangeDoorStateResult result = ) CDoor::ChangeDoorStateForScript(doorData.GetModelInfoHash(),
																								doorData.GetPosition(), 
																								pDoor, 
																								true,
																								locked, 
																								doorData.GetTargetRatio(),
																								removeSpring, 
																								smoothLock,
																								doorData.GetAutomaticRate(),
																								doorData.GetAutomaticDistance(), 
																								false, 
																								forceOpenRatio);

			Assert(result == CDoor::CHANGE_DOOR_STATE_SUCCESS);
			doorData.SetState(doorData.GetPendingState());
			doorData.SetPendingState(DOORSTATE_INVALID);
			doorData.StateHasBeenFlushed();
			return;
		}
#if !IGNORE_OLD_OVERIDES
		switch (doorData.GetPendingState())
		{
		case DOORSTATE_INVALID:
			break;
		case DOORSTATE_LOCKED:
			{		
				bool bLockNow = false;

				if (NetworkInterface::IsGameInProgress())
				{
					// lock immediately in MP
					bLockNow = true;
				}
				else
				{
					CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

					const Vector3 &playerPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());

					float distanceSquared = (doorData.GetPosition() - playerPosition).Mag2();

					// Make sure the player is at least 3 meters away from the door
					if (distanceSquared > 9.0f)
					{
						// Only lock the door once it is fully closed
						if (fabsf(pDoor->GetDoorOpenRatio()) <= 0.005f)
						{
							// Make sure we don't lock the player inside
							int playerInteriorProxy = 0;
							fwInteriorLocation loc = pPlayerPed->GetInteriorLocation();
							if (loc.IsValid())
							{
								CInteriorProxy* pIntProxy = CInteriorProxy::GetFromLocation(loc);
								if (pIntProxy)
								{
									playerInteriorProxy = CInteriorProxy::GetPool()->GetJustIndex(pIntProxy);
								}
							}

							int doorInteriorProxy = interior_commands::GetProxyAtCoords(doorData.GetPosition(), 0);

							if (playerInteriorProxy != doorInteriorProxy || playerInteriorProxy == 0)
							{
								bLockNow = true;
							}
						}
					}
				}

				if (bLockNow)
				{
					ASSERT_ONLY(CDoor::ChangeDoorStateResult result = ) CDoor::ChangeDoorStateForScript(doorData.GetModelInfoHash(),doorData.GetPosition(), pDoor, true, 0.0f, false, true);
					Assert(result == CDoor::CHANGE_DOOR_STATE_SUCCESS);
					bSuccess = true;
				}		
				break;
			} 
		case DOORSTATE_FORCE_LOCKED_UNTIL_OUT_OF_AREA:
		case DOORSTATE_FORCE_LOCKED_THIS_FRAME:
			{
				ASSERT_ONLY(CDoor::ChangeDoorStateResult result = ) CDoor::ChangeDoorStateForScript(doorData.GetModelInfoHash(), doorData.GetPosition(), pDoor, true, 0.0f, false);
				Assert(result == CDoor::CHANGE_DOOR_STATE_SUCCESS);
				bSuccess = true;
				break;
			} 
		case DOORSTATE_UNLOCKED:
			{
				ASSERT_ONLY(CDoor::ChangeDoorStateResult result = ) CDoor::ChangeDoorStateForScript(doorData.GetModelInfoHash(), doorData.GetPosition(), pDoor, false, pDoor->GetDoorOpenRatio(), false, true);
				Assert(result == CDoor::CHANGE_DOOR_STATE_SUCCESS);
				bSuccess = true;
				break;
			}
		case DOORSTATE_FORCE_UNLOCKED_THIS_FRAME:
			{
				ASSERT_ONLY(CDoor::ChangeDoorStateResult result = ) CDoor::ChangeDoorStateForScript(doorData.GetModelInfoHash(), doorData.GetPosition(), pDoor, false, pDoor->GetDoorOpenRatio(), false);
				Assert(result == CDoor::CHANGE_DOOR_STATE_SUCCESS);
				bSuccess = true;
				break;
			}
		case DOORSTATE_FORCE_OPEN_THIS_FRAME:
			{
				ASSERT_ONLY(CDoor::ChangeDoorStateResult result = ) CDoor::ChangeDoorStateForScript(doorData.GetModelInfoHash(), doorData.GetPosition(), pDoor, true, 1.0f, false);
				Assert(result == CDoor::CHANGE_DOOR_STATE_SUCCESS);
				bSuccess = true;
				break;
			}
		case DOORSTATE_FORCE_CLOSED_THIS_FRAME:
			{
				ASSERT_ONLY(CDoor::ChangeDoorStateResult result = ) CDoor::ChangeDoorStateForScript(doorData.GetModelInfoHash(), doorData.GetPosition(), pDoor, true, 0.0f, false);
				Assert(result == CDoor::CHANGE_DOOR_STATE_SUCCESS);
				bSuccess = true;
				break;
			}
		}
#endif
	}

	if (bSuccess)
	{
		doorData.SetState(doorData.GetPendingState());
		doorData.SetPendingState(DOORSTATE_INVALID);
		doorData.StateHasBeenFlushed();
	}
}

void CDoorSystem::BreakDoor(CDoor& door, DoorBrokenFlags brokenFlags, bool bRemoveFragments)
{
	fragInst* pFragInst = door.GetFragInst();
	Assert(pFragInst);

	if (pFragInst && AssertVerify(pFragInst->GetTypePhysics()->GetNumChildGroups() > 1))
	{
		if(bRemoveFragments)
		{
			// When deleting groups start from the root so we delete fewer subtrees
			s32 numBits = (sizeof(DoorBrokenFlags)<<3)-1;
			for (s32 bit=0; bit < numBits; bit++)
			{
				if (brokenFlags & (u64(1) << u64(bit)))
				{
					if (!pFragInst->GetCacheEntry() || !pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsSet(bit))
					{
						pFragInst->DeleteAboveGroup(bit);
					}
				}
			}
		}
		else
		{
			//When applying the broken bit information start from the highest bit number
			//and decrement down to ensure the hierarchy breaks fully and completely
			for (s32 bit=(sizeof(DoorBrokenFlags)<<3)-1; bit >= 0; bit--)
			{
				if (brokenFlags & (u64(1) << u64(bit)))
				{
					if (!pFragInst->GetCacheEntry() || !pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsSet(bit))
					{
						pFragInst->BreakOffAboveGroup(bit);
					}
				}
			}
		}
	}
}

void CDoorSystem::DamageDoor(CDoor& door, DoorDamagedFlags damagedFlags)
{
	fragInst* pFragInst = door.GetFragInst();
	if( pFragInst )
	{
		u32 numBits = (sizeof(DoorDamagedFlags)<<3);
		u64 damagedMask = 1;
		for( u32 bit = 0; bit < numBits; bit++, damagedMask = damagedMask<<1 )
		{
			if( damagedFlags & damagedMask )
			{
				const u32 groupIndex = bit;
				fragCacheEntry* cacheEntry = pFragInst->GetCacheEntry();
				if( !cacheEntry || !cacheEntry->GetHierInst()->groupInsts[groupIndex].IsDamaged() )
				{
					// TODO: Consider the last four parameters - Primarily we need to decide on whether to trigger events or not.
					// -- Also, Is the actual damage important? How about position and velocity?
					pFragInst->ReduceGroupDamageHealth(groupIndex, -1.0f, Vec3V(V_ZERO), Vec3V(V_ZERO) FRAGMENT_EVENT_PARAM(true));
				}
			}
		}
	}
}

void CDoorSystem::Update()
{
	static unsigned const MAX_DOORS_TO_REMOVE = 50;
	CDoorSystemData* doorsToRemove[MAX_DOORS_TO_REMOVE];
	u32 numDoorsToRemove = 0;

 	for (DoorSystemMap::Iterator doorIterator = ms_doorMap.CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
 	{
		CDoorSystemData &doorData = *doorIterator.GetData();
		CDoor* pDoor = doorData.GetDoor();

		if (doorData.GetFlaggedForRemoval())
		{
			if (numDoorsToRemove < MAX_DOORS_TO_REMOVE)
			{
				doorsToRemove[numDoorsToRemove++] = &doorData;
			}
		}
		else if (pDoor)
		{
			if (doorData.GetPendingState() != DOORSTATE_INVALID)
			{
				SetPendingStateOnDoor(doorData);
			}

			// Disabled broken and damaged door flags
			if (0)//!pDoor->IsNetworkClone())
			{
				// if the door has broken grab the broken state
				fragInst* pFragInst = pDoor->GetFragInst();

				// If there is a cache entry than the door must be non-pristine in some way
				fragCacheEntry* cacheEntry = pFragInst ? pFragInst->GetCacheEntry() : NULL;
				if( cacheEntry )
				{
					// Looking for broken components
					if(	pFragInst->GetTypePhysics()->GetNumChildGroups() > 1 )
					{
						DoorBrokenFlags brokenFlags = 0;

						u32 numBrokenFlags = cacheEntry->GetHierInst()->groupBroken->GetNumBits();

						if (numBrokenFlags > (sizeof(DoorBrokenFlags)<<3))
						{
							Vector3 doorPos = doorData.GetPosition();
							Assertf(0, "Door %s at %f, %f, %f has too many broken flags (%d)", pDoor->GetNetworkObject() ? pDoor->GetNetworkObject()->GetLogName() : "", doorPos.x, doorPos.y, doorPos.z, numBrokenFlags);
						}

						for (s32 bit=0; bit<numBrokenFlags; bit++)
						{
							if (cacheEntry->GetHierInst()->groupBroken->IsSet(bit))
							{
								brokenFlags |= (u64(1) << u64(bit));
							}
						}

						doorData.SetBrokenFlags(brokenFlags);
					}

					// Looking for groups that have damaged out
					fragPhysicsLOD* pTypePhysics = pFragInst->GetTypePhysics();
					fragHierarchyInst* hierInst = cacheEntry->GetHierInst();
					if( pTypePhysics && hierInst )
					{
						DoorDamagedFlags damagedFlags = 0;
						
						u32 numGroups = pTypePhysics->GetNumChildGroups();
						Assertf(numGroups < (sizeof(DoorDamagedFlags)<<3), "Increase size of DoorDamagedFlags");
						for( u32 groupIndex = 0; groupIndex < numGroups; groupIndex++ )
						{
							if( hierInst->groupInsts[groupIndex].IsDamaged() )
							{
								damagedFlags |= (u64(1) << u64(groupIndex));
							}
						}

						doorData.SetDamagedFlags(damagedFlags);
					}
				}
			}
		}
 	}

	for (u32 i=0; i<numDoorsToRemove; i++)
	{
		RemoveDoor(doorsToRemove[i]);
	}
}

void CDoorSystem::Init()
{
	ms_doorMap.Create(MAX_DOORSYSTEM_MAP_SIZE);
	ms_lockedMap.Create(MAX_DOORSYSTEM_MAP_SIZE);
}

void CDoorSystem::Shutdown()
{
	for (CDoorSystem::DoorSystemMap::Iterator doorIterator = ms_doorMap.CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
	{
		CDoorSystemData *pDoorData = doorIterator.GetData();

		if (AssertVerify(pDoorData))
		{
			Assert(!pDoorData->GetNetObject());

			if (pDoorData->GetScriptHandler())
			{
				pDoorData->GetScriptHandler()->UnregisterScriptObject(*pDoorData);
			}

			delete pDoorData;
		}
	}

	ms_doorMap.Kill();
	ms_lockedMap.Kill();
}

void CDoorSystem::DebugDraw()
{
#if DEBUG_DRAW
	fwEntity *pEntity = g_PickerManager.GetSelectedEntity();
	if (!pEntity)
	{
		return;
	}
	Vector2 pos(40.0f, 40.0f);

	for (CDoorSystem::DoorSystemMap::Iterator doorIterator = ms_doorMap.CreateIterator(); !doorIterator.AtEnd(); doorIterator.Next())
	{
		CDoorSystemData *pDoorData = doorIterator.GetData();

		fwModelId objectModelId;
		CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfoFromHashKey((u32) pDoorData->GetModelInfoHash(), &objectModelId);		//	ignores return value

		const Vector3 &position = pDoorData->GetPosition();

		u32 posHash = CDoorSystem::GenerateHash32(position);
		
		const char *pOpenOrClosed = "";
		const char *pProcessPhysics = __BANK ? "Not Processed Physics" : "";
		CDoor *pDoor = pDoorData->GetDoor();
		if (pDoor != pEntity)
		{
			continue;
		}

		pOpenOrClosed = object_commands::CommandIsDoorClosed(pDoorData->GetEnumHash()) ? "Closed" : "Open";
#if __BANK
		if (pDoor)
		{
			u32 processedPhysicsThisFrame  = pDoor->m_processedPhysicsThisFrame;
			if ((processedPhysicsThisFrame & 2) && (processedPhysicsThisFrame & 1))
			{
				pProcessPhysics = "Active & Ratios Mismatched";
			}
			else if (processedPhysicsThisFrame & 2)
			{
				pProcessPhysics = "Active";
			}
			else if (processedPhysicsThisFrame & 1)
			{
				pProcessPhysics = "Ratios Mismatched";
			}
		}
#endif
		char buff[1024];
		formatf(buff, "%s, %.3f, %.3f, %.3f, %u, %s, %s, Ratio %.3f, AutoRate %.3f, AutoDist %.3f, Pitch %f, %s",
				pModel->GetModelName(), 
				position.x, position.y, position.z,
				u32(posHash), pDoorData->GetLocked() ? "locked" : "unlocked",
				pDoorData->GetUnsprung() ? "unsprung" : "sprung",
				pDoorData->GetTargetRatio(),
				pDoorData->GetAutomaticRate(),
				pDoorData->GetAutomaticDistance(),
				(pDoorData->GetDoor() ? pDoorData->GetDoor()->GetTransform().GetPitch() : 0.0f),
				pOpenOrClosed);


		grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, buff);
		pos.y += 12.0f;
#if __BANK
		if (*pProcessPhysics != '\0')
		{
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, pProcessPhysics);
		}
		if (ms_logDebugDraw)
		{
			Displayf("%s", buff);
		}
		pos.y += 12.0f;
		grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, pDoor->WantsToBeAwake() ? "Wants to be awake" : "Doesn't want to be awake");

#endif
#if __DEV
		pos.y += 12.0f;
		char scriptNamebuff[512];
		formatf(scriptNamebuff, "Door Create by %s", pDoorData->m_pScriptName);
		grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color_white, scriptNamebuff);
#endif // __DEV

	}

#if __BANK
	ms_logDebugDraw = false;
#endif
#endif	// DEBUG_DRAW
}

u32 CDoorSystem::GenerateHash32(const Vector3 &position)
{
	Vector3 pos = position;
	unsigned hash = 0;

	Assert(pos.z >= -200.0f);	// allow z within -200..823m range		(4m accuracy)
	Assert(pos.z <= 823.0f);
	Assert(pos.x >= WORLDLIMITS_REP_XMIN);	
	Assert(pos.x <= WORLDLIMITS_REP_XMAX);
	Assert(pos.y >= WORLDLIMITS_REP_YMIN);	
	Assert(pos.y <= WORLDLIMITS_REP_YMAX);

	pos.y += WORLDLIMITS_REP_YMAX;
	pos.x += WORLDLIMITS_REP_XMAX;

	unsigned	zPart = (((s32)(rage::Floorf(pos.z + 0.5f))) + 200) / 4;
	unsigned	yPart = ((s32)(rage::Floorf(pos.y + 0.5f)));
	unsigned	xPart = ((s32)(rage::Floorf(pos.x + 0.5f)));

	hash = zPart << 24;
	hash = hash | (yPart << 12);
	hash = hash | xPart;

	Assert(hash != 0);

	return hash;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CDoorScriptHandlerObject
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CDoorScriptHandlerObject::CreateScriptInfo(const scriptIdBase& scrId, const ScriptObjectId& objectId, const HostToken hostToken)
{
	m_ScriptInfo = CGameScriptObjInfo(scrId, objectId, hostToken);
	Assert(m_ScriptInfo.IsValid());
}

void CDoorScriptHandlerObject::SetScriptInfo(const scriptObjInfoBase& info)
{
	m_ScriptInfo = info;
}

scriptObjInfoBase*	CDoorScriptHandlerObject::GetScriptInfo()
{ 
	if (m_ScriptInfo.IsValid())
	{
		return static_cast<scriptObjInfoBase*>(&m_ScriptInfo); 
	}

	return NULL;
}

const scriptObjInfoBase*	CDoorScriptHandlerObject::GetScriptInfo() const 
{ 
	if (m_ScriptInfo.IsValid())
	{
		return static_cast<const scriptObjInfoBase*>(&m_ScriptInfo); 
	}

	return NULL;
}

void CDoorScriptHandlerObject::SetScriptHandler(scriptHandler* handler) 
{ 	
	Assert(!handler || handler->GetScriptId() == m_ScriptInfo.GetScriptId());
	m_pHandler = handler; 
}

void CDoorScriptHandlerObject::OnRegistration(bool newObject, bool hostObject)
{
	CDoorSystemData* pDoorSystemData = static_cast<CDoorSystemData*>(this);

	if (NetworkInterface::IsGameInProgress())
	{
		netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

		if(log)
		{
			char logName[30];
			char header[50];
			m_ScriptInfo.GetLogName(logName, 30);

			formatf(header, 50, "REGISTER_%s_%s_DOOR", newObject ? "NEW" : "EXISTING", hostObject ? "HOST" : "LOCAL");

			NetworkLogUtils::WriteLogEvent(*log, header, "%s      %s", m_ScriptInfo.GetScriptId().GetLogName(), logName);

			log->WriteDataValue("Script id", "%d", m_ScriptInfo.GetObjectId());

			netObject* netObj = GetNetObject();
			log->WriteDataValue("Net Object",	"%s", netObj ? netObj->GetLogName() : "-none-");

			log->WriteDataValue("Door system hash",	"%u", pDoorSystemData->GetEnumHash());

			Vector3 doorPos = pDoorSystemData->GetPosition();
			log->WriteDataValue("Position",	"%f, %f, %f", doorPos.x, doorPos.y, doorPos.z);

#if __DEV
			CBaseModelInfo *modelInfo = CModelInfo::GetBaseModelInfoFromHashKey(pDoorSystemData->GetModelInfoHash(), NULL);
			log->WriteDataValue("Model name", modelInfo ? modelInfo->GetModelName() : "??");
#endif // __DEV

			log->WriteDataValue("Persists w/o netobj",	"%s", pDoorSystemData->GetPersistsWithoutNetworkObject() ? "true" : "false");
		}
	}
}

void CDoorScriptHandlerObject::OnUnregistration()
{
	CDoorSystemData* pDoorSystemData = static_cast<CDoorSystemData*>(this);

	netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

	char logName[30];
	m_ScriptInfo.GetLogName(logName, 30);

	if (log)
	{
		NetworkLogUtils::WriteLogEvent(*log, "UNREGISTER_DOOR", "%s      %s", m_ScriptInfo.GetScriptId().GetLogName(), logName);

		log->WriteDataValue("Script id", "%d", m_ScriptInfo.GetObjectId());
		log->WriteDataValue("Net Object", "%s", pDoorSystemData->GetNetworkObject() ? pDoorSystemData->GetNetworkObject()->GetLogName() : "-none-");
	}
}

void CDoorScriptHandlerObject::Cleanup()
{
	CDoorSystemData* pDoorSystemData = static_cast<CDoorSystemData*>(this);

	netLoggingInterface* log = CTheScripts::GetScriptHandlerMgr().GetLog();

	if (log)
	{
		char logName[30];
		logName[0] = 0;

		if (gnetVerify(GetScriptInfo()))
		{
			GetScriptInfo()->GetLogName(logName, 30);
		}

		NetworkLogUtils::WriteLogEvent(*log, "CLEANUP_SCRIPT_DOOR", "%s      %s", GetScriptInfo()->GetScriptId().GetLogName(), logName);
		log->WriteDataValue("Script id", "%d", m_ScriptInfo.GetObjectId());
		log->WriteDataValue("Net Object", "%s", pDoorSystemData->GetNetworkObject() ? pDoorSystemData->GetNetworkObject()->GetLogName() : "-none-");
		
		Vector3 doorPos = pDoorSystemData->GetPosition();
		log->WriteDataValue("Position",	"%f, %f, %f", doorPos.x, doorPos.y, doorPos.z);

#if __DEV
		CBaseModelInfo *modelInfo = CModelInfo::GetBaseModelInfoFromHashKey(pDoorSystemData->GetModelInfoHash(), NULL);
		log->WriteDataValue("Model name", modelInfo ? modelInfo->GetModelName() : "??");
#endif // __DEV

		log->WriteDataValue("Persists w/o netobj",	"%s", pDoorSystemData->GetPersistsWithoutNetworkObject() ? "true" : "false");
	}

	if (pDoorSystemData->GetNetworkObject() && AssertVerify(pDoorSystemData->GetNetworkObject()->GetDoorSystemData() == pDoorSystemData))
	{
		if (pDoorSystemData->GetPersistsWithoutNetworkObject())
		{
			log->WriteDataValue("Network clone", "%s", pDoorSystemData->GetNetworkObject()->IsClone() ? "true" : "false");
			log->WriteDataValue("Pending owner change", "%s", pDoorSystemData->GetNetworkObject()->IsPendingOwnerChange() ? "true" : "false");

			if (!pDoorSystemData->GetNetworkObject()->IsClone() &&
				!pDoorSystemData->GetNetworkObject()->IsPendingOwnerChange())
			{
				log->WriteDataValue("Net obj unregistered", "true");
				NetworkInterface::UnregisterScriptDoor(*pDoorSystemData);
			}
			else
			{
				log->WriteDataValue("Net obj unregistered", "false");
				// leave the network object if owned by another machine
				static_cast<CNetObjDoor*>(pDoorSystemData->GetNetworkObject())->SetDoorSystemData(NULL);
				pDoorSystemData->SetNetworkObject(NULL);
			}
		}
		else 
		{
			log->WriteDataValue("Net obj unregistered", "true");
			NetworkInterface::UnregisterScriptDoor(*pDoorSystemData);
		}
	}
	
	m_ScriptInfo.Invalidate();

	pDoorSystemData->SetFlagForRemoval();
}	

bool CDoorScriptHandlerObject::HostObjectCanBeCreatedByClient() const
{
	const CDoorSystemData* pDoorSystemData = static_cast<const CDoorSystemData*>(this);

	return pDoorSystemData->GetPersistsWithoutNetworkObject();
}

#if __BANK
void CDoorScriptHandlerObject::SpewDebugInfo(const char* scriptName) const
{
	const CDoorSystemData* pDoorSystemData = static_cast<const CDoorSystemData*>(this);

	Vector3 doorPos = pDoorSystemData->GetPosition();

	if (scriptName)
	{
		Displayf("%s: Door (%f, %f, %f) %s\n", 
			scriptName, 
			doorPos.x, doorPos.y, doorPos.z, 
			pDoorSystemData->GetNetworkObject() ?  pDoorSystemData->GetNetworkObject()->GetLogName() : "");
	}
	else
	{
		Displayf("Door (%f, %f, %f) %s\n", 
			doorPos.x, doorPos.y, doorPos.z, 
			pDoorSystemData->GetNetworkObject() ?  pDoorSystemData->GetNetworkObject()->GetLogName() : "");
	}
}
#endif // __BANK

netObject* CDoorScriptHandlerObject::GetNetObject() const
{
	const CDoorSystemData* pDoorSystemData = static_cast<const CDoorSystemData*>(this);

	return pDoorSystemData->GetNetworkObject();
}

void CDoorScriptHandlerObject::SetScriptObjInfo(scriptObjInfoBase* info)
{
	if (info)
	{
		if (m_ScriptInfo.IsValid())
		{
			const CDoorSystemData* pDoorSystemData = static_cast<const CDoorSystemData*>(this);
			
			if (pDoorSystemData->GetNetworkObject())
			{
				gnetAssertf(*info == m_ScriptInfo, "Trying to register %s with script %s, which is already registered with script %s", pDoorSystemData->GetLogName(), info->GetScriptId().GetLogName(), m_ScriptInfo.GetScriptId().GetLogName());
			}
			else
			{
				gnetAssertf(*info == m_ScriptInfo, "Trying to register door at %f, %f, %f with script %s, which is already registered with script %s", pDoorSystemData->GetPosition().x, pDoorSystemData->GetPosition().y, pDoorSystemData->GetPosition().z, info->GetScriptId().GetLogName(), m_ScriptInfo.GetScriptId().GetLogName());
			}
		}
		else
		{
			m_ScriptInfo = *info;
		}
	}
	else if (m_ScriptInfo.IsValid())
	{
		m_ScriptInfo.Invalidate();
	}
}

void CDoorSystemData::SetState(DoorScriptState doorState)
{
#if ENABLE_NETWORK_LOGGING
	if(PARAM_doorExcessiveLogging.Get() && m_state != doorState)
	{
		gnetDebug1("Door 0x%p [%u] changing state from: %d to: %d", this, GetEnumHash(), m_state, doorState);
		sysStack::PrintStackTrace();
	}
#endif // ENABLE_NETWORK_LOGGING
	m_state = doorState;
}
void CDoorSystemData::SetPendingState(DoorScriptState pendingState)
{
#if ENABLE_NETWORK_LOGGING
	if(PARAM_doorExcessiveLogging.Get() && m_pendingState != pendingState)
	{
		gnetDebug1("Door 0x%p [%u] changing pending state from: %d to: %d", this, GetEnumHash(), m_pendingState, pendingState);
		sysStack::PrintStackTrace();
	}
#endif // ENABLE_NETWORK_LOGGING
	m_pendingState = pendingState;
}

void CDoorSystemData::SetHoldOpen(bool holdOpen, bool bNetCall) 
{ 
	m_holdOpen = holdOpen; 
	if (m_pDoor && m_pDoor->GetHoldingOpen() != holdOpen)
	{
		if (bNetCall || Verifyf(!m_pDoor->GetUnderCodeControl(), "Door is under code control holding open changes will be ignored"))
		{
			m_pDoor->SetHoldingOpen(holdOpen);
		}
	}
}

void CDoorSystemData::SetFlagForRemoval()
{
#if ENABLE_NETWORK_LOGGING
	if(NetworkInterface::IsGameInProgress())
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FLAGGING_FOR_REMOVAL_DOOR_SYSTEM", "%u", GetEnumHash());
	}
#endif // ENABLE_NETWORK_LOGGING
	m_flaggedForRemoval = true;
	Assertf(!m_pNetworkObject, "Flagging a door for removal which still has a network object (%s)", m_pNetworkObject->GetLogName());
}


bool CDoorSystemData::CanStopNetworking()
{
	bool bCanStopNetworking = false;

	DoorScriptState state = m_pendingState != -1 ? m_pendingState : m_state;

	// the door must have returned to its original state and the physical door must match the door system state before the door can stop networking itself
	if (m_persistsWithoutNetworkObject && state == m_preNetworkedState)
	{
		bCanStopNetworking = true;

		if (m_pDoor)
		{
			switch (m_state)
			{
			case DOORSTATE_INVALID:
				break;
			case DOORSTATE_UNLOCKED:
			case DOORSTATE_FORCE_UNLOCKED_THIS_FRAME:
				if (m_pDoor->GetHoldingOpen() && m_pDoor->GetDoorOpenRatio() < 0.95f)
				{
					bCanStopNetworking = false;
				}
				break;
			case DOORSTATE_LOCKED:
			case DOORSTATE_FORCE_LOCKED_UNTIL_OUT_OF_AREA:
			case DOORSTATE_FORCE_LOCKED_THIS_FRAME:
			case DOORSTATE_FORCE_CLOSED_THIS_FRAME:
				if (!m_pDoor->IsDoorShut())
				{
					bCanStopNetworking = false;
				}
				break;
			case DOORSTATE_FORCE_OPEN_THIS_FRAME:
				if (m_pDoor->GetDoorOpenRatio() < 0.95f || GetTargetRatio() < 0.95f)
				{
					bCanStopNetworking = false;
				}
				break;
			}
		}
	}

	return bCanStopNetworking;
}

const char* CDoorSystemData::GetDoorStateName(DoorScriptState state)
{
	static char stateName[50];

	switch (state)
	{
	case DOORSTATE_INVALID:
		formatf(stateName, 50, "INVALID");
		break;
	case DOORSTATE_UNLOCKED:
		formatf(stateName, 50, "UNLOCKED");
		break;
	case DOORSTATE_LOCKED:
		formatf(stateName, 50, "LOCKED");
		break;
	case DOORSTATE_FORCE_LOCKED_UNTIL_OUT_OF_AREA:
		formatf(stateName, 50, "FORCE_LOCKED_UNTIL_OUT_OF_AREA");
		break;
	case DOORSTATE_FORCE_UNLOCKED_THIS_FRAME:
		formatf(stateName, 50, "FORCE_UNLOCKED_THIS_FRAME");
		break;
	case DOORSTATE_FORCE_LOCKED_THIS_FRAME:
		formatf(stateName, 50, "FORCE_LOCKED_THIS_FRAME");
		break;
	case DOORSTATE_FORCE_OPEN_THIS_FRAME:
		formatf(stateName, 50, "FORCE_OPEN_THIS_FRAME");
		break;
	case DOORSTATE_FORCE_CLOSED_THIS_FRAME:
		formatf(stateName, 50, "FORCE_CLOSED_THIS_FRAME");
		break;
	default:
		Assertf(0, "CDoorSystem::GetDoorStateName - unhandled door state");
	}

	return stateName;
}
#if __BANK
void SetDoorDebugStringHash()
{
	sm_debugDooorSystemEnumHash = atStringHash(sm_debugDooorSystemEnumString);
	snprintf(sm_debugDooorSystemEnumNumber, CDoor::DEBUG_DOOR_STRING_LENGTH, "%d", sm_debugDooorSystemEnumHash);
}
#endif 
