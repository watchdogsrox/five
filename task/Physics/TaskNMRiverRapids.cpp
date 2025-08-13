// Filename   :	TaskNMRiverRapids.cpp
// Description:	GTA-side of the NM underwater behavior for river rapids

//---- Include Files ---------------------------------------------------------------------------------------------------------------------------------
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragmentnm\messageparams.h"

// Framework headers
#include "grcore/debugdraw.h"

// Game headers
#include "Peds/ped.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMRiverRapids.h"
#include "Task/Physics/NmDebug.h"
#include "Renderer/Water.h"
#include "system/controlMgr.h"

//----------------------------------------------------------------------------------------------------------------------------------------------------

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMRiverRapids::Tunables CTaskNMRiverRapids::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMRiverRapids, 0xd74b499c);

#if __BANK
extern const char* parser_RagdollComponent_Strings[];
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMRiverRapids //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CTaskNMRiverRapids::Tunables::PreAddWidgets(bkBank& bank)
{
	bank.PushGroup("Ragdoll Component Buoyancy Multiplier", false);
	for (int i = 0; i < RAGDOLL_NUM_COMPONENTS; i++)
	{
		bank.AddSlider(parser_RagdollComponent_Strings[i], &m_fRagdollComponentBuoyancy[i], 0.0f, 10.0f, 0.1f);
	}
	bank.PopGroup();
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMRiverRapids::CTaskNMRiverRapids(u32 nMinTime, u32 nMaxTime)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: CTaskNMBehaviour(nMinTime, nMaxTime),
m_fTimeSubmerged(0.0f),
m_pBuoyancyOverride(NULL)
{
#if !__FINAL
	m_strTaskName = "NMRiverRapids";
#endif // !__FINAL
	SetInternalTaskType(CTaskTypes::TASK_NM_RIVER_RAPIDS);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMRiverRapids::~CTaskNMRiverRapids()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMRiverRapids::CleanUp()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed* pPed = GetPed();

	if (pPed != NULL)
	{
		if (pPed->IsPlayer())
		{
			// Reset the buoyancy force multiplier and sprint control counter when leaving the task
			pPed->m_Buoyancy.m_fForceMult = 1.0f;
			if (pPed->GetPlayerInfo())
			{
				pPed->GetPlayerInfo()->SetPlayerDataSprintControlCounter(0.0f);
			}
		}

		//may as well have this here as well, as there are rare cases where this is apparently not shutting off properly.
		if(pPed->GetSpeechAudioEntity())
		{
			pPed->GetSpeechAudioEntity()->SetIsInRapids(false);
		}

		// Restore buoyancy info!
		if(m_pBuoyancyOverride)
		{
			delete m_pBuoyancyOverride;
			m_pBuoyancyOverride = NULL;
			pPed->m_Buoyancy.SetBuoyancyInfoOverride(NULL);
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMRiverRapids::StartBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CBuoyancyInfo* pBuoyancyInfo = pPed->m_Buoyancy.GetBuoyancyInfo(pPed);
	if (pBuoyancyInfo != NULL)
	{
		// Copy the buoyancy info prior to changing it.
		m_pBuoyancyOverride = pBuoyancyInfo->Clone();
		Assert(pPed->m_Buoyancy.GetBuoyancyInfoOverride()==NULL);
		pPed->m_Buoyancy.SetBuoyancyInfoOverride(m_pBuoyancyOverride);
		pBuoyancyInfo = pPed->m_Buoyancy.GetBuoyancyInfo(pPed);

		for (int i = WS_PED_RAGDOLL_SAMPLE_START; i < pBuoyancyInfo->m_nNumWaterSamples; i++)
		{
			CWaterSample& sample = pBuoyancyInfo->m_WaterSamples[i];

			if (sample.m_nComponent < RAGDOLL_NUM_COMPONENTS)
			{
				sample.m_fBuoyancyMult = sm_Tunables.m_fRagdollComponentBuoyancy[sample.m_nComponent];
			}
		}
	}

	sm_Tunables.m_Start.Post(*pPed, this);

	if (pPed != NULL && pPed->GetSpeechAudioEntity())
	{
		pPed->GetSpeechAudioEntity()->SetIsInRapids(true);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMRiverRapids::ControlBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	sm_Tunables.m_Update.Post(*pPed, this);

	HandleBuoyancy(pPed);

	CTaskNMBehaviour::ControlBehaviour(pPed);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMRiverRapids::FinishConditions(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (!CTaskMotionSwimming::CheckForRapids(pPed) || pPed->ShouldBeDead() || pPed->GetIsDeadOrDying())
	{
		if (pPed->GetWaterLevelOnPed() > 0.6f)
		{
			m_nSuggestedNextTask = CTaskTypes::TASK_NM_BUOYANCY;
		}
		else
		{
			CTaskNMHighFall* pNewTask = rage_new CTaskNMHighFall(200, NULL, CTaskNMHighFall::HIGHFALL_IN_AIR);
			CTaskNMControl* pControlTask = GetControlTask();
			if (pNewTask && pControlTask)
			{
				pControlTask->ForceNewSubTask(pNewTask);
			}
		}

		nmDebugf3("[%u]Finish %s on %s - not in river or river flow not fast enough. Next task: %s", fwTimer::GetFrameCount(), GetTaskName(), pPed->GetModelName(), TASKCLASSINFOMGR.GetTaskName(m_nSuggestedNextTask));
		
		if(pPed && pPed->GetSpeechAudioEntity())
			pPed->GetSpeechAudioEntity()->SetIsInRapids(false);

		return true;
	}

	bool finished = ProcessFinishConditionsBase(pPed, MONITOR_FALL, FLAG_SKIP_DEAD_CHECK | FLAG_SKIP_WATER_CHECK);
	if(finished && pPed && pPed->GetSpeechAudioEntity())
		pPed->GetSpeechAudioEntity()->SetIsInRapids(false);

	return finished;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMRiverRapids::HandleBuoyancy(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Control how much the player thrashes about in the water!
	float fSprintResult = 1.0f;
	if (pPed->IsPlayer() && pPed->GetControlFromPlayer() != NULL && sm_Tunables.m_BodyWrithe.m_bControlledByPlayerSprintInput)
	{
		fSprintResult = pPed->GetPlayerInfo()->ProcessSprintControl(*pPed->GetControlFromPlayer(), *pPed, CPlayerInfo::SPRINT_UNDER_WATER, true);

		const CPlayerInfo::sSprintControlData& rSprintControlData = CPlayerInfo::ms_Tunables.m_SprintControlData[CPlayerInfo::SPRINT_UNDER_WATER];
		fSprintResult *= (rSprintControlData.m_Threshhold / rSprintControlData.m_MaxLimit);
	}

	ART::MessageParams msg;
	msg.addFloat(NMSTR_PARAM(NM_WRITHE_ARM_AMPLITUDE), Lerp(fSprintResult, sm_Tunables.m_BodyWrithe.m_fMinArmAmplitude, sm_Tunables.m_BodyWrithe.m_fMaxArmAmplitude));
	msg.addFloat(NMSTR_PARAM(NM_WRITHE_ARM_PERIOD), Lerp(fSprintResult, sm_Tunables.m_BodyWrithe.m_fMinArmPeriod, sm_Tunables.m_BodyWrithe.m_fMaxArmPeriod));
	msg.addFloat(NMSTR_PARAM(NM_WRITHE_ARM_STIFFNESS), Lerp(fSprintResult, sm_Tunables.m_BodyWrithe.m_fMinArmStiffness, sm_Tunables.m_BodyWrithe.m_fMaxArmStiffness));
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_WRITHE_MSG), &msg);

	msg.reset();
	msg.addBool(NMSTR_PARAM(NM_SET_CHARACTER_UNDERWATER_IS_UNDERWATER), true);
	msg.addFloat(NMSTR_PARAM(NM_SET_CHARACTER_UNDERWATER_GRAVITY_FACTOR), 1.0f);
	msg.addFloat(NMSTR_PARAM(NM_SET_CHARACTER_UNDERWATER_VISCOSITY), -1.0f);
	msg.addBool(NMSTR_PARAM(NM_SET_CHARACTER_UNDERWATER_LINEAR_STROKE), false);
	msg.addFloat(NMSTR_PARAM(NM_SET_CHARACTER_UNDERWATER_STROKE), Lerp(fSprintResult, sm_Tunables.m_BodyWrithe.m_fMinStroke, sm_Tunables.m_BodyWrithe.m_fMaxStroke));
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_SET_CHARACTER_UNDERWATER_MSG), &msg);

	pPed->m_Buoyancy.m_fForceMult = Lerp(fSprintResult, sm_Tunables.m_BodyWrithe.m_fMinBuoyancy, sm_Tunables.m_BodyWrithe.m_fMaxBuoyancy);
}

bool CTaskNMRiverRapids::ProcessPhysics(float fTimeStep, int iTimeSlice)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	phCollider* pCollider = pPed->GetCollider();

	if (!nmVerifyf(pCollider != NULL && pCollider->IsArticulated(), "CTaskNMRiverRapids::ProcessPhysics: Should have a valid ragdoll inst at this point!"))
	{
		return CTaskNMBehaviour::ProcessPhysics(fTimeStep, iTimeSlice);
	}

	// Track how long we've been in the water.  Clamp to 0-1 scaler. character should try
	// to right himself more the longer he's in the water.
	float fSubmergedLevel = 0.0f;
	int iNumSamples = 0;
	CBuoyancyInfo* pBuoyancyInfo = pPed->m_Buoyancy.GetBuoyancyInfo(pPed);
	if (pBuoyancyInfo != NULL)
	{
		for (int iSample = 0; iSample < pBuoyancyInfo->m_nNumWaterSamples; iSample++)
		{
			if (pBuoyancyInfo->m_WaterSamples[iSample].m_nComponent == RAGDOLL_SPINE3)
			{
				fSubmergedLevel += pPed->m_Buoyancy.GetWaterLevelOnSample(iSample) / pBuoyancyInfo->m_WaterSamples[iSample].m_fSize;
				iNumSamples++;
			}
		}
	}

	if (iNumSamples > 0 && (fSubmergedLevel / iNumSamples) > 0.0f)
	{
		m_fTimeSubmerged += fTimeStep;

		int iLinkIndex = static_cast<phArticulatedCollider*>(pCollider)->GetLinkFromComponent(RAGDOLL_SPINE3);
		Mat34V xSpineTransform;
		Transpose3x3(xSpineTransform, static_cast<phArticulatedCollider*>(pCollider)->GetBody()->GetLink(iLinkIndex).GetMatrix());
		Translate(xSpineTransform, xSpineTransform, pCollider->GetMatrix().d());

		// Grab surface point and normal
		Vec3V vSurfacePoint;
		Vec3V vSurfaceNormal;
		// If we are in a river, used the cached river bound probe result
		if (!pPed->m_Buoyancy.GetCachedRiverBoundProbeResult(RC_VECTOR3(vSurfacePoint), RC_VECTOR3(vSurfaceNormal)))
		{
			float fDepth, fWaterHeight;
			const Vec3V vPedPosition = pPed->GetTransform().GetPosition();
			Water::GetWaterDepth(RCC_VECTOR3(vPedPosition), &fDepth, &fWaterHeight);
			vSurfacePoint = vPedPosition;
			vSurfacePoint.SetZf(fWaterHeight);
			vSurfaceNormal = Vec3V(V_UP_AXIS_WZERO);
		}

		const ScalarV fDeadzone(V_HALF);
		ScalarV rumbleScale(V_ZERO);
		if (sm_Tunables.m_bHorizontalRighting)
		{
			// Measure horizontalness. Clamp into 0-1 scaler.
			// Character should try to right himself more when he's horizontal.
			ScalarV fHorizontalness = Subtract(ScalarV(V_ONE), Abs(Dot(vSurfaceNormal, xSpineTransform.a())));
			// Compress somewhat to create a deadzone.
			fHorizontalness *= Add(ScalarV(V_ONE), fDeadzone);
			fHorizontalness -= fDeadzone;
			fHorizontalness = Clamp(fHorizontalness, ScalarV(V_ZERO), ScalarV(V_ONE));

			vSurfaceNormal = UnTransform3x3Ortho(xSpineTransform, vSurfaceNormal);
			vSurfaceNormal.SetX(ScalarV(V_ZERO));
			vSurfaceNormal = Normalize(Transform3x3(xSpineTransform, vSurfaceNormal));

			ScalarV fAngle = AngleNormInput(vSurfaceNormal, Negate(xSpineTransform.c()));
			if(IsLessThanAll(Dot(vSurfaceNormal, xSpineTransform.b()), ScalarV(V_ZERO)) != 0.0f)
			{
				fAngle = Negate(fAngle);
			}

			float fTimeSubmerged = Clamp(m_fTimeSubmerged, 0.0f, sm_Tunables.m_fHorizontalRightingTime);
			fTimeSubmerged /= sm_Tunables.m_fHorizontalRightingTime;
			
			Vec3V vAngImpulse = xSpineTransform.a() * fAngle * ScalarV(sm_Tunables.m_fHorizontalRightingStrength) * fHorizontalness * ScalarV(fTimeSubmerged);
			static_cast<phArticulatedCollider*>(pCollider)->GetBody()->ApplyAngImpulse(iLinkIndex, vAngImpulse.GetIntrin128());
			rumbleScale = MagSquared(vAngImpulse);
		}

		if (sm_Tunables.m_bVerticalRighting)
		{
			// Character should try to right himself more when up-side down, somewhat less when he's horizontal
			// and not at all when right-side up
			ScalarV fDotProduct = Dot(vSurfaceNormal, xSpineTransform.a());
			ScalarV fVerticalness = Scale(fDotProduct, Negate(fDeadzone));
			fVerticalness = Add(fVerticalness, fDeadzone);
			fVerticalness = Clamp(fVerticalness, ScalarV(V_ZERO), ScalarV(V_ONE));

			ScalarV fAngle = Arccos(fDotProduct);
			Vec3V vTorqueAxis;
			vTorqueAxis = Normalize(Cross(xSpineTransform.a(), vSurfaceNormal));

			float fTimeSubmerged = Clamp(m_fTimeSubmerged, 0.0f, sm_Tunables.m_fVerticalRightingTime);
			fTimeSubmerged /= sm_Tunables.m_fVerticalRightingTime;

			Vec3V vAngImpulse = vTorqueAxis * fAngle * ScalarV(sm_Tunables.m_fVerticalRightingStrength) * fVerticalness * ScalarV(fTimeSubmerged);
			static_cast<phArticulatedCollider*>(pCollider)->GetBody()->ApplyAngImpulse(iLinkIndex, vAngImpulse.GetIntrin128());
			rumbleScale = MagSquared(vAngImpulse);
		}

		// Give the control pad a shake as the ragdoll is thrown around in the rapids.
		if(pPed->IsLocalPlayer() && IsGreaterThanAll(rumbleScale, ScalarV(V_ZERO)))
		{
			// Scale the rumble intensity based on the angular impulse applied above.
			const u32 knRumbleDuration = 50;
			const float kfMaxRumbleIntensity = 0.3f;
			const float kfMinAngImpSq = 1.0f;
			const float kfMaxAngImpSq = 22.0f;

			float fRange = kfMaxAngImpSq - kfMinAngImpSq;

			float fRumbleScale = rumbleScale.Getf();
			fRumbleScale -= kfMinAngImpSq;
			fRumbleScale /= fRange;
			fRumbleScale = Clamp(fRumbleScale, 0.0f, 1.0f);

			CControlMgr::StartPlayerPadShakeByIntensity(knRumbleDuration, fRumbleScale*kfMaxRumbleIntensity);
		}
	}
	else
	{
		m_fTimeSubmerged = 0.0f;
	}
	
	return CTaskNMBehaviour::ProcessPhysics(fTimeStep, iTimeSlice);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskNMRiverRapids::CreateQueriableState() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new CClonedNMRiverRapidsInfo(); 
}

CTaskFSMClone *CClonedNMRiverRapidsInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMRiverRapids(4000, 30000);
}
