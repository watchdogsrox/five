// Filename   :	TaskNMExplosion.cpp
// Description:	Natural Motion explosion class (FSM version)

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated/articulatedcollider.h"
#include "phbound/boundcomposite.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths\Angle.h"

// Game headers
#include "camera/CamInterface.h"
#include "Event\EventDamage.h"
#include "Network\NetworkInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedPlacement.h"
#include "PedGroup\PedGroup.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Task\Combat\TaskAnimatedHitByExplosion.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Physics/TaskNMExplosion.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMExplosion::Tunables CTaskNMExplosion::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMExplosion, 0xd587e2a8);
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CClonedNMExplosionInfo
//////////////////////////////////////////////////////////////////////////

CClonedNMExposionInfo::CClonedNMExposionInfo(const Vector3& vecExplosionPos) 
: m_explosionPos(vecExplosionPos) 
{
}

CClonedNMExposionInfo::CClonedNMExposionInfo()
: m_explosionPos(Vector3(0.0f, 0.0f, 0.0f)) 
{
}

CTaskFSMClone *CClonedNMExposionInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMExplosion(10000, 10000, m_explosionPos);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMExplosion::CTaskNMExplosion(u32 nMinTime, u32 nMaxTime, const Vector3& vecExplosionPos)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
:	CTaskNMBehaviour(nMinTime, nMaxTime),
m_vExplosionOrigin(vecExplosionPos),
m_nMaxPedalTimeInRollUp(0),
m_nStartCatchfallTime(0),
m_bUseCatchFall(false),
m_bPostApex(false),
m_bRollUp(false),
m_bWrithe(false),
m_bStunned(false),
m_eState(WINDMILL_AND_PEDAL)
{
#if !__FINAL
	m_strTaskName = "NMExplosion";
#endif // !__FINAL

	Assertf(m_vExplosionOrigin.x == m_vExplosionOrigin.x, "CTaskNMExplosion::CTaskNMExplosion - given a non-finite explosion position");
	if (m_vExplosionOrigin.x != m_vExplosionOrigin.x)
		m_vExplosionOrigin = Vector3(Vector3::ZeroType);

	ClearFlag(CTaskNMBehaviour::DO_CLIP_POSE);
	SetInternalTaskType(CTaskTypes::TASK_NM_EXPLOSION);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMExplosion::~CTaskNMExplosion()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}

aiTask* CTaskNMExplosion::Copy() const { 
	CTaskNMExplosion* pNewTask = rage_new CTaskNMExplosion(m_nMinTime, m_nMaxTime, m_vExplosionOrigin);
	
	pNewTask->m_eState = m_eState;
	pNewTask->m_bRollUp = m_bRollUp;
	pNewTask->m_bWrithe = m_bWrithe;
	pNewTask->m_bStunned = m_bStunned;
	
	pNewTask->m_nInitialStateTime = m_nInitialStateTime;
	pNewTask->m_nWritheTime = m_nWritheTime;
	pNewTask->m_nStartWritheTime = m_nStartWritheTime;
	pNewTask->m_nStartCatchfallTime = m_nStartCatchfallTime;
	pNewTask->m_bPostApex = m_bPostApex;

	return pNewTask;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMExplosion::StartBehaviour(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If a lifeless ragdoll is required (i.e. a realistic explosion response), we can quit early.
	if(sm_Tunables.m_UseRelaxBehaviour)
		return;

#if !__FINAL
	// On restarting the task, reset the task name display for debugging the state.
	m_strTaskName = "NMExplosion";
#endif // !__FINAL

	StartWindmillAndPedal(pPed);
	m_eState = WINDMILL_AND_PEDAL;

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PreferInjuredGetup, true);

	// Compute a random time to execute the initial behaviour decided on above before testing for exit / state switch conditions.
	m_nInitialStateTime = (u32)fwRandom::GetRandomNumberInRange((float)sm_Tunables.m_MinTimeForInitialState, (float)sm_Tunables.m_MaxTimeForInitialState);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMExplosion::ControlBehaviour(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If a lifeless ragdoll is required (i.e. a realistic explosion response), we can quit early.
	if(sm_Tunables.m_UseRelaxBehaviour)
		return;

	const Vector3 vecRootSpeed = pPed->GetLocalSpeed(Vector3(0.0f,0.0f,0.0f), false, 0);
	const Vector3 vProbePos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// Has the ped reached the apex of its trajectory?
	if(vecRootSpeed.z <= 0.0f)
	{
		m_bPostApex = true;
	}

	WorldProbe::CShapeTestFixedResults<> probeResults;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
	probeDesc.SetExcludeEntity(pPed);

	
	// Explosion state machine:
	switch(m_eState)
	{
	/////////////////////////
	case WINDMILL_AND_PEDAL:
	/////////////////////////
		{
#if !__FINAL
		m_strTaskName = "NMExplosion:WINDMILL_AND_PEDAL";
#endif // !__FINAL

		// After running this behaviour for a period of time, or if we are close enough to the ground, switch to a different behavior.
		if(fwTimer::GetTimeInMilliseconds() > m_nInitialStateTime + m_nStartTime && m_bPostApex)
		{
			probeDesc.SetStartAndEnd(vProbePos, Vector3(vProbePos.x, vProbePos.y, vProbePos.z - sm_Tunables.m_CatchFallHeightThresholdWindmill));
			if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				m_eState = ROLL_DOWN_STAIRS;
			}
		}
		break;
		}

	//////////////
	case ROLL_DOWN_STAIRS:
	//////////////
		{
#if !__FINAL
		m_strTaskName = "NMExplosion:ROLL_DOWN_STAIRS";
#endif // !__FINAL
		// Set up the custom roll direction
		Vec3V vecImpactDir = pPed->GetTransform().GetPosition() - VECTOR3_TO_VEC3V(GetExplosionOrigin());
		vecImpactDir = Normalize(vecImpactDir);
		CNmMessageList list;
		AddTuningSet(&sm_Tunables.m_StartRollDownStairs);
		ApplyTuningSetsToList(list);

		ART::MessageParams& msgRollDownStairs = list.GetMessage(NMSTR_MSG(NM_ROLLDOWN_STAIRS_MSG));
		msgRollDownStairs.addVector3(NMSTR_PARAM(NM_ROLLDOWN_STAIRS_CUSTOM_ROLLDIR_VEC3), VEC3V_ARGS(vecImpactDir));

		list.Post(pPed->GetRagdollInst());

		if(pPed->IsDead())
		{
			m_eState = WAIT;
		}
		else
		{
			m_nStartCatchfallTime = fwTimer::GetTimeInMilliseconds();
			m_eState = CATCH_FALL;
		}

		break;
		}

	/////////////////
	case CATCH_FALL:
	/////////////////
		{
#if !__FINAL
		m_strTaskName = "NMExplosion:CATCH_FALL";
#endif // !__FINAL

		//  if the ped isn't dead yet, extend the time on the ground.
		if (!m_bStunned && !pPed->ShouldBeDead() && !pPed->m_nFlags.bIsOnFire && (sm_Tunables.m_AllowPlayerStunned || !pPed->IsPlayer()))
		{
			s32 stunnedTime = (s32)(fwRandom::GetRandomNumberInRange((float)sm_Tunables.m_MinStunnedTime, (float)sm_Tunables.m_MaxStunnedTime));

			s32 timePassed = (s32) (fwTimer::GetTimeInMilliseconds() - m_nStartTime);
			s32 minTime = (s32)m_nMinTime;
			m_nMinTime = (u16)Max(timePassed + stunnedTime, minTime - timePassed);
			m_nMaxTime = Max(m_nMinTime, m_nMaxTime);
			m_bStunned = true;
		}

		u32 timeToStartCatchfall = (pPed->IsPlayer() || pPed->m_nFlags.bIsOnFire) ? sm_Tunables.m_TimeToStartCatchFallPlayer : sm_Tunables.m_TimeToStartCatchFall;
		if(fwTimer::GetTimeInMilliseconds() > timeToStartCatchfall + m_nStartCatchfallTime)
		{
			AddTuningSet(&sm_Tunables.m_StartCatchFall);
			CNmMessageList list;
			ApplyTuningSetsToList(list);
			list.Post(pPed->GetRagdollInst());

			m_eState = WAIT;
		}
		break;
		}

	////////////
	case WAIT:
	////////////
		{
#if !__FINAL
		m_strTaskName = "NMExplosion:WAIT";
#endif // !__FINAL
		break;
		}
	/////////
	default:
	/////////
		{
			taskAssertf(false, "Unknown state in CTaskNMExplosion. State:%d", m_eState);
		break;
	}
}

	CNmMessageList list;
	AddTuningSet(&sm_Tunables.m_Update);
	ApplyTuningSetsToList(list);
	list.Post(pPed->GetRagdollInst());
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMExplosion::StartWindmillAndPedal(CPed *pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Determine whether this is a front or back shot
	Vec3V vecImpactDir;
	vecImpactDir = pPed->GetTransform().GetPosition() - VECTOR3_TO_VEC3V(GetExplosionOrigin());
	if (IsZeroAll(vecImpactDir))
		vecImpactDir = Vec3V(V_X_AXIS_WZERO);
	else 
		vecImpactDir = Normalize(vecImpactDir);
	Assertf(vecImpactDir.GetXf() == vecImpactDir.GetXf(), "CTaskNMExplosion::StartWindmillAndPedal - bad vecImpactDir. Ped position is %f, %f, %f.  Explosion origin is %f, %f, %f",
		pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf(), pPed->GetTransform().GetPosition().GetZf(),
		GetExplosionOrigin().x, GetExplosionOrigin().y, GetExplosionOrigin().z);
	if (vecImpactDir.GetXf() != vecImpactDir.GetXf())
		vecImpactDir = Vec3V(V_X_AXIS_WZERO);

	phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
	Matrix34 pelvisMat;
	pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(0)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); // The pelvis
	Vec3V dirFacing = -(RCC_MAT34V(pelvisMat).GetCol2());  
	dirFacing.SetZ(ScalarV(V_ZERO));
	dirFacing = NormalizeSafe(dirFacing, Vec3V(V_X_AXIS_WZERO));
	bool bFront = Dot(dirFacing, vecImpactDir).Getf() <= 0.0f;

	CNmMessageList list;
	AddTuningSet(&sm_Tunables.m_StartWindmill);
	ApplyTuningSetsToList(list);

	ART::MessageParams& msgShotFromBehind = list.GetMessage(NMSTR_MSG(NM_SHOTFROMBEHIND_MSG));
	msgShotFromBehind.addBool(NMSTR_PARAM(NM_START), !bFront);

	ART::MessageParams& msgShotNewBullet = list.GetMessage(NMSTR_MSG(NM_SHOTNEWBULLET_MSG));
	msgShotNewBullet.addInt(NMSTR_PARAM(NM_SHOTNEWBULLET_BODYPART), 10);
	msgShotNewBullet.addBool(NMSTR_PARAM(NM_SHOTNEWBULLET_LOCAL_HIT_POINT_INFO), false);
	pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(10)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix()));
	msgShotNewBullet.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_POS_VEC3), pelvisMat.d.x, pelvisMat.d.y, pelvisMat.d.z);
	msgShotNewBullet.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_NORMAL_VEC3), vecImpactDir.GetXf(), vecImpactDir.GetYf(), vecImpactDir.GetZf());
	static float mag = 1000.0f;
	msgShotNewBullet.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_VEL_VEC3), vecImpactDir.GetXf() * mag, vecImpactDir.GetYf() * mag, vecImpactDir.GetZf() * mag);

	list.Post(pPed->GetRagdollInst());
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMExplosion::FinishConditions(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Try using success and failure messages to end task. 
	int nFlags = FLAG_SKIP_DEAD_CHECK | FLAG_QUIT_IN_WATER | FLAG_VEL_CHECK;
	if (!pPed->IsDead())
		nFlags |= FLAG_RELAX_AP_LOW_HEALTH;

	// Some explosions don't kill, but set peds on fire - Finish the NM task and push an on fire event when we land
	if (m_eState == WAIT && !pPed->IsDead() && pPed->m_nFlags.bIsOnFire)
	{
		CEventOnFire event(0);
		pPed->GetPedIntelligence()->AddEvent(event);
		return true;
	}

	return ProcessFinishConditionsBase(pPed, MONITOR_FALL, nFlags);
}

void CTaskNMExplosion::StartAnimatedFallback(CPed* pPed)
{
	float fStartPhase = 0.0f;
	fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
	fwMvClipId clipId = CLIP_ID_INVALID;

	Vector3 vecHeadPos(0.0f,0.0f,0.0f);
	pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);
	// If the ped is already prone then use an on-ground damage reaction
	if (vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf())
	{
		const EstimatedPose pose = pPed->EstimatePose();
		if (pose == POSE_ON_BACK)
		{
			clipId = CLIP_DAM_FLOOR_BACK;
		}
		else
		{
			clipId = CLIP_DAM_FLOOR_FRONT;
		}

		Matrix34 rootMatrix;
		if (pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT))
		{
			float fAngle = AngleZ(pPed->GetTransform().GetForward(), RCC_VEC3V(rootMatrix.c)).Getf();
			if (fAngle < -QUARTER_PI)
			{
				clipId = CLIP_DAM_FLOOR_LEFT;
			}
			else if (fAngle > QUARTER_PI)
			{
				clipId = CLIP_DAM_FLOOR_RIGHT;
			}
		}

		clipSetId = pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();
	}
	else
	{
		// If this task is starting as the result of a migration then use the suggested clip and start phase
		CTaskNMControl* pControlTask = GetControlTask();
		if (pControlTask != NULL && (pControlTask->GetFlags() & CTaskNMControl::ALREADY_RUNNING) != 0 && 
			m_suggestedClipSetId != CLIP_SET_ID_INVALID && m_suggestedClipId != CLIP_SET_ID_INVALID)
		{
			clipSetId = m_suggestedClipSetId;
			clipId = m_suggestedClipId;
			fStartPhase = m_suggestedClipPhase;
		}
		else
		{
			Vector3 vTempDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - GetExplosionOrigin();
			vTempDir.NormalizeSafe();
			float fHitDir = rage::Atan2f(-vTempDir.x, vTempDir.y);
			fHitDir -= pPed->GetTransform().GetHeading();
			fHitDir = fwAngle::LimitRadianAngle(fHitDir);
			fHitDir += QUARTER_PI;
			if(fHitDir < 0.0f)
			{
				fHitDir += TWO_PI;
			}
			int nHitDirn = int(fHitDir / HALF_PI);

			CTaskFallOver::eFallDirection dir = CTaskFallOver::kDirFront;
			switch(nHitDirn)
			{
			case CEntityBoundAI::FRONT:
				dir = CTaskFallOver::kDirFront;
				break;
			case CEntityBoundAI::LEFT:
				dir = CTaskFallOver::kDirLeft;
				break;
			case CEntityBoundAI::REAR:
				dir = CTaskFallOver::kDirBack;
				break;
			case CEntityBoundAI::RIGHT:
				dir = CTaskFallOver::kDirRight;
				break;
			}

			CTaskFallOver::eFallContext context = CTaskFallOver::kContextExplosion;
			CTaskFallOver::PickFallAnimation(pPed, context, dir, clipSetId, clipId);
		}
	}

	SetNewTask(rage_new CTaskAnimatedHitByExplosion(clipSetId, clipId, fStartPhase));
}

bool CTaskNMExplosion::ControlAnimatedFallback(CPed* pPed)
{
	nmDebugf3("LOCAL: Controlling animated fallback task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");

	// disable ped capsule control so the blender can set the velocity directly on the ped
	if (pPed->IsNetworkClone())
	{
		NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
	}

	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_FINISHED;
		return false;
	}

	return true;
}
