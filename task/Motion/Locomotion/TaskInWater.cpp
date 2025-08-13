#include "TaskInWater.h"

#include "Animation/AnimManager.h"
#include "animation/Move.h"
#include "camera/base/BaseCamera.h"
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/Aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "control\replay\replay.h"
#include "debug\DebugScene.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "Peds/PedIntelligence.h"
#include "Peds/rendering/PedProps.h"
#include "Peds/rendering/PedVariationPack.h"
#include "renderer\Water.h"
#include "task\Default\TaskPlayer.h"
#include "task\Motion\Locomotion\TaskHumanLocomotion.h"
#include "Task/Movement/TaskIdle.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMRiverRapids.h"
#include "task/motion/Locomotion/TaskMotionPed.h"
#include "task/System/MotionTaskData.h"

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

dev_float CTaskMotionSwimming::ms_fMaxPitch = HALF_PI * 0.95f;
dev_float CTaskMotionSwimming::ms_fFPSClipHeightOffset = -0.0625f;

CTaskMotionSwimming::Tunables CTaskMotionSwimming::sm_Tunables;
IMPLEMENT_MOTION_TASK_TUNABLES(CTaskMotionSwimming, 0x2bdbfecf);

dev_float CTaskMotionDiving::ms_fMaxPitch = HALF_PI * 0.95f;

dev_float CTaskMotionDiving::ms_fDiveUnderPitch = 0;//HALF_PI * 0.95f;

//////////////////////////////////////////////////////////////////////////
//	Swimming motion task
//////////////////////////////////////////////////////////////////////////
const u32 g_InWaterCameraHash = ATSTRINGHASH("FOLLOW_PED_IN_WATER_CAMERA", 0x0b2c08bfd);


dev_float MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER		= 0.525f; //0.5->0.6 B* 1190776 0.6->525 B* 1337761
dev_float MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER_SURFACING = 0.6f;
dev_float MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER_NOWAVES = 0.475f; //0.5->0.475 B* 1337761
dev_float MBIW_SWIM_UNDER_BOAT_HEIGHT_BELOW_WATER	= 1.0f;
// Velocity change limits
dev_float MBIW_SWIM_BUOYANCY_ACCEL_LIMIT			= 4.0f;
dev_float MBIW_SWIM_BUOYANCY_DECEL_LIMIT			= 4.0f;
dev_float MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT			= 30.0f;
dev_float MBIW_SWIM_UWATER_SURFACING_DAMPING_DIST	= 0.1f; //7.5->0.1 B* 1149877
dev_float MBIW_SWIM_UWATER_SURFACING_MIN_VEL		= 0.05f;

dev_float MBIW_FRICTION_VEL_CHANGE_LIMIT			= 30.0f;
dev_float MBIW_SWIM_EXTRA_YAW_SCALE = 0.5f;				

const float CTaskMotionSwimming::s_fPreviousVelocityChange = 0.0f;
const float CTaskMotionSwimming::s_fHeightBelowWater = 0.75f;
const float CTaskMotionSwimming::s_fHeightBelowWaterGoal = 0.75f;

const fwMvBooleanId CTaskMotionSwimming::ms_OnEnterIdleSwimmingId("OnEnter_Idle",0x1941AC09);
const fwMvBooleanId CTaskMotionSwimming::ms_OnEnterRapidsIdleSwimmingId("OnEnter_RapidsIdle",0x11620242);
const fwMvBooleanId CTaskMotionSwimming::ms_OnExitStruggleSwimmingId("OnExit_Struggle",0xE378577F);
const fwMvBooleanId CTaskMotionSwimming::ms_OnEnterStopSwimmingId("OnEnter_StopSwimming",0xC676C3C2);
const fwMvBooleanId CTaskMotionSwimming::ms_OnEnterSwimmingId("OnEnter_Swimming",0x912B6EC2);
const fwMvBooleanId CTaskMotionSwimming::ms_OnEnterSwimStartId("OnEnter_SwimStart",0x7D735D50);
const fwMvBooleanId CTaskMotionSwimming::ms_BreathCompleteId("Breath_Complete",0xB2FE5DC7);
const fwMvFlagId CTaskMotionSwimming::ms_AnimsLoadedId("AnimsLoaded",0x20D334DE);
const fwMvFlagId CTaskMotionSwimming::ms_InRapidsId("InRapids",0x8C431D90);
const fwMvFloatId CTaskMotionSwimming::ms_DesiredSpeedId("desiredspeed",0xCF7BA842);
const fwMvFloatId CTaskMotionSwimming::ms_HeadingId("heading",0x43F5CF44);
const fwMvFloatId CTaskMotionSwimming::ms_SpeedId("speed",0xF997622B);
const fwMvFloatId CTaskMotionSwimming::ms_TargetHeading("TargetHeading",0x2B1FFB24);
const fwMvFloatId CTaskMotionSwimming::ms_TargetHeadingDelta("TargetHeadingDelta",0x68F5225B);
const fwMvFloatId CTaskMotionSwimming::ms_MoveRateOverrideId("MoveRateOverride",0x9968532B);
const fwMvFloatId CTaskMotionSwimming::ms_SwimPhaseId("SwimPhase",0x2ef9896b);
const fwMvRequestId CTaskMotionSwimming::ms_StruggleId("struggle",0x68673BCA);
const fwMvRequestId CTaskMotionSwimming::ms_BreathId("Breath",0x642D40E8);
const fwMvFlagId CTaskMotionSwimming::ms_FPSSwimming("FPSSwimming", 0xf83afe2f);
const fwMvClipId CTaskMotionSwimming::ms_WeaponHoldingClipId("WeaponHoldingClip", 0xca01efb6);

CTaskMotionSwimming::~CTaskMotionSwimming()
{

}

void CTaskMotionSwimming::ClearScubaGearVariationForPed(CPed& rPed)
{
	//Clear the scuba mask prop.
	ClearScubaMaskPropForPed(rPed);

	//Ensure the scuba gear variation is active.
	if(!IsScubaGearVariationActiveForPed(rPed))
	{
		return;
	}

	//Ensure the scuba gear variation is valid.
	bool bLightOn = IsScubaGearLightOnForPed(rPed);
	const PedVariation* pVariation = FindScubaGearVariationForPed(rPed, bLightOn);
	if(!pVariation)
	{
		return;
	}

	// Grab the previous variation information if it was for this component
	u32 uVariationDrawableId = 0;
	u32 uVariationDrawableAltId = 0;
	u32 uVariationTexId = 0;
	u32 uVariationPaletteId = 0;

	CPlayerInfo* pPlayerInfo = rPed.GetPlayerInfo();
	if(pPlayerInfo)
	{
		if(pPlayerInfo->GetPreviousVariationComponent() == pVariation->m_Component)
		{
			pPlayerInfo->GetPreviousVariationInfo(uVariationDrawableId, uVariationDrawableAltId, uVariationTexId, uVariationPaletteId);
		}

		pPlayerInfo->ClearPreviousVariationData();
	}

	//Clear the scuba gear variation.
	rPed.SetVariation(pVariation->m_Component, uVariationDrawableId, uVariationDrawableAltId, uVariationTexId, uVariationPaletteId, 0, false);

	//Clear the override light bone for MP players
	if(bLightOn && NetworkInterface::IsGameInProgress() && rPed.IsPlayer())
	{
		rPed.EnableMpLight(false);
	}
}

void CTaskMotionSwimming::ClearScubaMaskPropForPed(CPed& rPed)
{
	//Ensure the prop is valid.
	const Tunables::ScubaMaskProp* pProp = FindScubaMaskPropForPed(rPed);
	if(!pProp)
	{
		return;
	}

	//Ensure the prop is active.
	if(CPedPropsMgr::GetPedPropIdx(&rPed, ANCHOR_EYES) != pProp->m_Index)
	{
		return;
	}

	//Clear the prop.
	CPedPropsMgr::ClearPedProp(&rPed, ANCHOR_EYES);
}

const CTaskMotionSwimming::PedVariation * CTaskMotionSwimming::FindScubaGearVariationForPed(const CPed& rPed, bool bLightOn)
{
	//Grab the ped's model name hash.
	u32 uModelNameHash = rPed.GetPedModelInfo()->GetModelNameHash();

	//Ensure the variations are valid.
	const ScubaGearVariations* pVariations = FindScubaGearVariationsForModel(uModelNameHash);
	if(!pVariations)
	{
		return NULL;
	}

	//Check scuba pack variations to see if one already matches current variation
	for(int i = 0; i < pVariations->m_Variations.GetCount(); ++i)
	{
		const PedVariation& rPedVariation = bLightOn ? pVariations->m_Variations[i].m_ScubaGearWithLightsOn :
			pVariations->m_Variations[i].m_ScubaGearWithLightsOff;

		u32 uDrawableId = rPed.GetPedDrawHandler().GetVarData().GetPedCompIdx(rPedVariation.m_Component);
		if(uDrawableId == rPedVariation.m_DrawableId)
		{
			u8 uDrawableAltId = rPed.GetPedDrawHandler().GetVarData().GetPedCompAltIdx(rPedVariation.m_Component);
			if(uDrawableAltId == rPedVariation.m_DrawableAltId)
			{
				return &rPedVariation;
			}
		}
	}

	//Check which scuba pack is valid.
	for(int i = 0; i < pVariations->m_Variations.GetCount(); ++i)
	{
		const ScubaGearVariation& rScubaGearVariation = pVariations->m_Variations[i];
		const PedVariation& rPedVariation = bLightOn ? rScubaGearVariation.m_ScubaGearWithLightsOn :
			rScubaGearVariation.m_ScubaGearWithLightsOff;

		if(rScubaGearVariation.m_Wearing.GetCount() == 0)
		{
			return &rPedVariation;
		}
		else
		{
			for(int j = 0; j < rScubaGearVariation.m_Wearing.GetCount(); ++j)
			{
				const PedVariationSet& rPedVariationSet = rScubaGearVariation.m_Wearing[j];

				u32 uDrawableId = rPed.GetPedDrawHandler().GetVarData().GetPedCompIdx(rPedVariationSet.m_Component);

				for(int k = 0; k < rPedVariationSet.m_DrawableIds.GetCount(); ++k)
				{
					if(uDrawableId == rPedVariationSet.m_DrawableIds[k])
					{
						return &rPedVariation;
					}
				}	
			}
		}
	}

	return NULL;
}

const CTaskMotionSwimming::ScubaGearVariations* CTaskMotionSwimming::FindScubaGearVariationsForModel(u32 uModelName)
{
	//Iterate over the scuba gear variations.
	for(int i = 0; i < sm_Tunables.m_ScubaGearVariations.GetCount(); ++i)
	{
		//Check if the model name matches.
		const ScubaGearVariations& rVariations = sm_Tunables.m_ScubaGearVariations[i];
		if(uModelName == rVariations.m_ModelName)
		{
			return &rVariations;
		}
	}

	return NULL;
}

const CTaskMotionSwimming::Tunables::ScubaMaskProp* CTaskMotionSwimming::FindScubaMaskPropForPed(const CPed& rPed)
{
	//Grab the ped's model name hash.
	u32 uModelNameHash = rPed.GetPedModelInfo()->GetModelNameHash();

	//Search through the scuba mask props.
	for(s32 i = 0; i < sm_Tunables.m_ScubaMaskProps.GetCount(); ++i)
	{
		//Check if the model name hash matches.
		const Tunables::ScubaMaskProp& rProp = sm_Tunables.m_ScubaMaskProps[i];
		if(uModelNameHash == rProp.m_ModelName.GetHash())
		{
			return &rProp;
		}
	}

	return NULL;
}

bool CTaskMotionSwimming::GetScubaGearVariationForPed(const CPed& rPed, ePedVarComp& nComponent, u8& uDrawableId, u8& uDrawableAltId, u8& uTexId)
{
	//Assert that the scuba gear variation is active.
	taskAssert(IsScubaGearVariationActiveForPed(rPed));

	//Ensure the scuba gear variation is valid.
	bool bLightOn = IsScubaGearLightOnForPed(rPed);
	const PedVariation* pVariation = FindScubaGearVariationForPed(rPed, bLightOn);
	if(!pVariation)
	{
		return false;
	}

	//Set the values.
	nComponent	= pVariation->m_Component;
	uDrawableId	= (u8)pVariation->m_DrawableId;
	uDrawableAltId = (u8)pVariation->m_DrawableAltId;

	//This doesn't exist in metadata, so look up from ped (assuming script set it properly)
	uTexId = rPed.GetPedDrawHandler().GetVarData().GetPedTexIdx(pVariation->m_Component);

	return true;
}

u32 CTaskMotionSwimming::GetTintIndexFromScubaGearVariationForPed(const CPed& rPed)
{
	//Assert that the scuba gear variation is active.
	taskAssert(IsScubaGearVariationActiveForPed(rPed));

	//Ensure the scuba gear variation is valid.
	bool bLightOn = IsScubaGearLightOnForPed(rPed);
	const PedVariation* pVariation = FindScubaGearVariationForPed(rPed, bLightOn);
	if(!pVariation)
	{
		return UINT_MAX;
	}

	return CPedVariationPack::GetPaletteVar(&rPed, pVariation->m_Component);
}

bool CTaskMotionSwimming::IsScubaGearLightOnForPed(const CPed& rPed)
{
	//Assert that the scuba gear variation is active.
	taskAssert(IsScubaGearVariationActiveForPed(rPed));

	//MP players use a bool on the ped, rather than a specific variation
	if(NetworkInterface::IsGameInProgress() && rPed.IsPlayer())
	{
		return rPed.GetMpLightEnabled();
	}
	else
	{
		return (IsScubaGearVariationActiveForPed(rPed, true));
	}
}

bool CTaskMotionSwimming::IsScubaGearVariationActiveForPed(const CPed& rPed)
{
	return (IsScubaGearVariationActiveForPed(rPed, false) || IsScubaGearVariationActiveForPed(rPed, true));
}

bool CTaskMotionSwimming::IsScubaGearVariationActiveForPed(const CPed& rPed, bool bLightOn)
{
	//Ensure the scuba gear variation is valid.
	const PedVariation* pVariation = FindScubaGearVariationForPed(rPed, bLightOn);
	if(!pVariation)
	{
		return false;
	}

	//Ensure the scuba gear variation is active.
	u32 uDrawableId = rPed.GetPedDrawHandler().GetVarData().GetPedCompIdx(pVariation->m_Component);
	if(uDrawableId != pVariation->m_DrawableId)
	{
		return false;
	}
	u8 uDrawableAltId = rPed.GetPedDrawHandler().GetVarData().GetPedCompAltIdx(pVariation->m_Component);
	if(uDrawableAltId != pVariation->m_DrawableAltId)
	{
		return false;
	}

	return true;
}

void CTaskMotionSwimming::OnPedDiving(CPed& rPed)
{
	//Update the scuba gear variation.
	UpdateScubaGearVariationForPed(rPed, true);
}

void CTaskMotionSwimming::OnPedNoLongerDiving(CPed& rPed, bool bForceSet)
{
	//Update the scuba gear variation.
	UpdateScubaGearVariationForPed(rPed, false, bForceSet);
}

void CTaskMotionSwimming::SetScubaGearVariationForPed(CPed& rPed)
{
	//Set the scuba gear variation.
	SetScubaGearVariationForPed(rPed, false);

	//Set the scuba mask prop.
	SetScubaMaskPropForPed(rPed);
}

void CTaskMotionSwimming::SetScubaGearVariationForPed(CPed& rPed, bool bLightOn)
{
	//Ensure the scuba gear variation is valid.
	const PedVariation* pVariation = FindScubaGearVariationForPed(rPed, bLightOn);
	if(!pVariation)
	{
		return;
	}

	bool bScubaGearVariationLightOnAlreadyActive = IsScubaGearVariationActiveForPed(rPed, true);
	bool bScubaGearVariationLightOffAlreadyActive = IsScubaGearVariationActiveForPed(rPed, false);

	// Cache off the current variation information for this component if not already wearing a scuba variation
	bool bScubaGearVariationAlreadyActive = bScubaGearVariationLightOnAlreadyActive || bScubaGearVariationLightOffAlreadyActive;
	CPlayerInfo* pPlayerInfo = rPed.GetPlayerInfo();
	if(!bScubaGearVariationAlreadyActive && pPlayerInfo)
	{
		pPlayerInfo->SetPreviousVariationData(pVariation->m_Component, rPed.GetPedDrawHandler().GetVarData().GetPedCompIdx(pVariation->m_Component), 
																	   rPed.GetPedDrawHandler().GetVarData().GetPedCompAltIdx(pVariation->m_Component),
																	   rPed.GetPedDrawHandler().GetVarData().GetPedTexIdx(pVariation->m_Component),
																	   rPed.GetPedDrawHandler().GetVarData().GetPedPaletteIdx(pVariation->m_Component));
	}

	//Set the scuba gear variation if it's different (to stop us wiping texture/palette data)
	bool bScubaGearVariationSameForBothLightStates = bScubaGearVariationLightOnAlreadyActive && bScubaGearVariationLightOffAlreadyActive;
	if (!bScubaGearVariationSameForBothLightStates)
	{
		rPed.SetVariation(pVariation->m_Component, pVariation->m_DrawableId, pVariation->m_DrawableAltId, 0, 0, 0, false);
	}

	//Set override light bone for MP players, as the main light bone is missing from skeleton
	if(NetworkInterface::IsGameInProgress() && rPed.IsPlayer())
	{
		rPed.EnableMpLight(bLightOn);
	}
}

void CTaskMotionSwimming::SetScubaMaskPropForPed(CPed& rPed)
{
	//Ensure the prop is valid.
	const Tunables::ScubaMaskProp* pProp = FindScubaMaskPropForPed(rPed);
	if(!pProp)
	{
		return;
	}

	//If we're already wearing a eye prop (and it isn't the scuba mask), remove it
	if (CPedPropsMgr::GetPedPropIdx(&rPed, ANCHOR_EYES) != -1 && CPedPropsMgr::GetPedPropIdx(&rPed, ANCHOR_EYES) != pProp->m_Index)
	{
		CPedPropsMgr::ClearPedProp(&rPed, ANCHOR_EYES);
	}

	//If we're wearing a head prop, remove it
	if (CPedPropsMgr::GetPedPropIdx(&rPed, ANCHOR_HEAD) != -1)
	{
		CPedPropsMgr::ClearPedProp(&rPed, ANCHOR_HEAD);
	}

	//Set the prop.
	CPedPropsMgr::SetPedProp(&rPed, ANCHOR_EYES, pProp->m_Index);
}

void CTaskMotionSwimming::UpdateScubaGearVariationForPed(CPed& rPed, bool bLightOn, bool bForceSet)
{
	//Ensure the scuba gear variation is active.
	if(!bForceSet && !IsScubaGearVariationActiveForPed(rPed))
	{
		return;
	}

	//Set the scuba gear variation.
	SetScubaGearVariationForPed(rPed, bLightOn);
}

atHashWithStringNotFinal CTaskMotionSwimming::GetModelForScubaGear(CPed& rPed)
{
	if(NetworkInterface::IsGameInProgress() && rPed.IsPlayer())
	{
		static atHashWithStringNotFinal s_hPropMP("xm_prop_x17_scuba_tank",0xC7530EE8);
		return s_hPropMP;
	}

	static atHashWithStringNotFinal s_hPropSP("P_Michael_Scuba_tank_s",0x5EFF0BC9);
	return s_hPropSP;
}

//////////////////////////////////////////////////////////////////////////
// FSM methods

CTask::FSM_Return CTaskMotionSwimming::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	if (pPed->GetIsSwimming()) 
	{
		ProcessMovement();
		PitchCapsuleBound();
		CTaskMotionDiving::UpdateDiveGaitReduction(pPed, m_bSwimGaitReduced, m_fGaitReductionHeading, true);
	} else
		pPed->SetDesiredBoundPitch(0.0f);
	
	if (pPed)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset, true);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning, true );
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true);	
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true);
#if FPS_MODE_SUPPORTED
		if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterTurnSettings, true );
		}
#endif	//FPS_MODE_SUPPORTED
	}
	return FSM_Continue;
}

void CTaskMotionSwimming::ProcessMovement()
{

	CPed* pPed = GetPed();
	//************************************
	// Smooth forward movement request
	//

	const float fMoveAccel = 4.0f;
	const float fMoveDecel = 2.0f;

	ApproachCurrentMBRTowardsDesired(fMoveAccel, fMoveDecel, MOVEBLENDRATIO_SPRINT);


	//***********************
	// apply extra yaw
	//
	float fDirectionVar= CalcDesiredDirection();
	m_TurnPhaseGoal = (fDirectionVar/PI + 1.0f)/2.0f;
	float fYawScale = MBIW_SWIM_EXTRA_YAW_SCALE;
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings) || pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettingsForScript))
		fYawScale = 1.5f;

#if FPS_MODE_SUPPORTED
	// Turn faster when FPS swimming
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		static dev_float fFPSExtraYawScale = 10.0f;
		fYawScale = fFPSExtraYawScale;
	}
#endif	//FPS_MODE_SUPPORTED

	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fDirectionVar * fYawScale * fwTimer::GetTimeStep());	

	//***********************
	// Smooth pitch request
	//

	float fDesiredPitch = pPed->GetDesiredPitch();
	if (GetState() == State_Idle)
	{
		//wave tilt		
		Vector3 vWaveDirection(pPed->m_Buoyancy.GetLastWaterNormal()); 
		float fWaveIntensity = vWaveDirection.Dot(ZAXIS);
		vWaveDirection.z=0.0f; vWaveDirection.Normalize();

#if __DEV
		Vector3 debugStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()); debugStart.z+=1.0f;
		Vector3 debugEnd = debugStart + (vWaveDirection * 0.3f);
		CTask::ms_debugDraw.AddArrow(VECTOR3_TO_VEC3V(debugStart), VECTOR3_TO_VEC3V(debugEnd), 0.1f, Color_green, 1);		
		debugEnd = debugStart + (pPed->m_Buoyancy.GetLastWaterNormal() * 0.3f);
		CTask::ms_debugDraw.AddArrow(VECTOR3_TO_VEC3V(debugStart), VECTOR3_TO_VEC3V(debugEnd), 0.1f, Color_blue, 1);	
#endif
		static dev_float sfWaveIntensityScale = 0.02f;
		float t = Min(1.0f, (1.0f-fWaveIntensity)/sfWaveIntensityScale);
		//Displayf("Wave intensity: %f, scale = %f", fWaveIntensity, t);
		fDesiredPitch = -VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()).Dot(vWaveDirection) * t;		
	}
	
	fDesiredPitch = Clamp( fDesiredPitch, -HALF_PI, HALF_PI );
	pPed->SetDesiredPitch( fwAngle::LimitRadianAngleForPitch(fDesiredPitch) );

	Assert(pPed->GetDesiredPitch() >= -HALF_PI && pPed->GetDesiredPitch() <= HALF_PI);

	//**********************************
	// Calculate desired pitch change
	//
	static float fPitchSpeedMul = 1.0f;
	const float fPitchDelta = pPed->GetDesiredPitch() - pPed->GetCurrentPitch();
	GetMotionData().SetExtraPitchChangeThisFrame(GetMotionData().GetExtraPitchChangeThisFrame() + fPitchDelta * fwTimer::GetTimeStep() * fPitchSpeedMul);
}

void CTaskMotionSwimming::PitchCapsuleBound()
{
	CPed * pPed = GetPed();
	if (pPed && !pPed->GetUsingRagdoll())
	{
		if (pPed) {
			// B*2005653: FPS Swim/Dive: rotate capsule to always cover peds head to fix camera pop issues on collisions
			if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				CTaskMotionDiving::PitchCapsuleRelativeToPed(pPed);
			}
			else
			{
				Matrix34 mat;
				pPed->GetBoneMatrix(mat, BONETAG_ROOT);			
				float fPitch = mat.c.Angle(ZAXIS); 								
				InterpBoundPitch(pPed, -fabs(fPitch));
			}
		}
	}
}

void CTaskMotionSwimming::InterpBoundPitch(CPed* pPed, float fTarget)
{	
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fPitchCapsuleRate, 2.0f,0.0f,20.0f,0.001f);
	
	if(pPed)
	{
		float currentPitch = pPed->GetBoundPitch(); 
		InterpValue(currentPitch, fTarget, ms_fPitchCapsuleRate, true); 
		pPed->SetDesiredBoundPitch(currentPitch);
	}

}

void CTaskMotionSwimming::HideGun()
{
	CPed *pPed = GetPed();
	CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	if (pWeapon)
	{
		// Don't make gun invisible if running swap weapon or reload task
		if (pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsGun() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading))
		{
			pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, true);
		}
	}
}

CTask::FSM_Return	CTaskMotionSwimming::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iEvent == OnUpdate)
	{
		CPed* pPed = GetPed();	

		if(iState != State_Start && CheckForWaterfall(pPed))
		{
			return FSM_Continue;
		}

		if(SyncToMoveState())
		{
			return FSM_Continue;
		}
	}

	FSM_Begin
		// Entry point
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
			FSM_OnExit
				Start_OnExit();

		FSM_State(State_Idle)			
			FSM_OnUpdate
				return Idle_OnUpdate();

		FSM_State(State_RapidsIdle)
			FSM_OnEnter
				RapidsIdle_OnEnter();
			FSM_OnUpdate
				return RapidsIdle_OnUpdate();
			FSM_OnExit
				RapidsIdle_OnExit();

		FSM_State(State_SwimStart)
			FSM_OnEnter
				SwimStart_OnEnter();
			FSM_OnUpdate
				return SwimStart_OnUpdate();
			FSM_OnExit
				SwimStart_OnExit();

		FSM_State(State_Swimming)
			FSM_OnEnter
				Swimming_OnEnter();
			FSM_OnUpdate
				return Swimming_OnUpdate();
			FSM_OnExit
				Swimming_OnExit();

		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

bool CTaskMotionSwimming::ProcessMoveSignals()
{
	SendParams();
	return true;	
}

bool CTaskMotionSwimming::CanDiveFromPosition(CPed& rPed, Vec3V_In vPosition, bool bCanParachute, Vec3V_Ptr pOverridePedPosition, bool bCollideWithIgnoreEntity)
{
	//Ensure the ped is not injured.
	if(rPed.IsInjured())
	{
		return false;
	}

	//Ensure the ped is not in custody.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody))
	{
		return false;
	}

	//Ensure the ped is not cuffed.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return false;
	}

	if (rPed.GetIsInWater())
	{
		return false;
	}

	if (rPed.GetHasJetpackEquipped())
	{
		return false;
	}

	//Calculate the start position.
	Vector3 vStart = VEC3V_TO_VECTOR3(vPosition);

	//Calculate the end position.
	Vector3 vEnd = vStart;

	//Check for water beneath the ped.
	float fWaterHeight = 0.0f;
	if(!CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(vStart, &fWaterHeight, false, 999999.9f)) 
	{
		return false;
	}

	float fHeightAboveWater = vEnd.z-fWaterHeight;

	static dev_float sf_MAXIMUM_DIVE_DIST = 9999.0f; // make infinite. dive skill will ragdoll if ped isn't good enough to perform.
	static dev_float sf_MINIMUM_DIVE_DIST = 1.0f;

	if (fHeightAboveWater > sf_MAXIMUM_DIVE_DIST || fHeightAboveWater < sf_MINIMUM_DIVE_DIST)
		return false;

	bool bCanDive = CPlayerInfo::GetDiveHeightForPed(rPed) > (rPed.GetTransform().GetPosition().GetZf() - fWaterHeight);

	//If we don't meet dive height & we can parachute, then prefer that.
	if(!bCanDive && bCanParachute)
	{
		return false;
	}

	//If ped has a parachute, prefer at a certain height.
	static dev_float sf_SKYDIVE_INSTEAD_DIST = 50.0f;
	if (bCanParachute && (fHeightAboveWater > sf_SKYDIVE_INSTEAD_DIST) )
	{
		return false;
	}

	//push test out depending on high/far I'm jumping
	static dev_float sf_MAX_DIVE_JUMP_DISTANCE = 1.0f;
	static dev_float sf_MIN_WATERLEVEL_START_SCALE = 2.0f;
	static dev_float sf_MAX_WATERLEVEL_START_SCALE = 20.0f;
	static dev_float sf_MIN_TEST_HEIGHT_OFFSET = 0.5f;
	static dev_float sf_MAX_TEST_HEIGHT_OFFSET = 2.0f;
	float t = Clamp((fHeightAboveWater-sf_MIN_WATERLEVEL_START_SCALE)/sf_MAX_WATERLEVEL_START_SCALE, 0.0f, 1.0f);	
	vStart.AddScaled(VEC3V_TO_VECTOR3(rPed.GetTransform().GetForward()), 1.0f + t*sf_MAX_DIVE_JUMP_DISTANCE);
	vStart.z -= sf_MIN_TEST_HEIGHT_OFFSET + t*(sf_MAX_TEST_HEIGHT_OFFSET-sf_MIN_TEST_HEIGHT_OFFSET);
	vEnd = vStart;

	//Check for solid objects beneath the ped.
	static dev_float depth_clearance_min = 1.5f;
	static dev_float depth_clearance_max = 2.5f;
	static dev_float depth_for_test = 5.0f;

	//Scale depth based on height.
	static dev_float depth_clearance_height_for_min = 5.0f;
	static dev_float depth_clearance_height_for_max = 25.0f;

	float fClampedHeight = Clamp(fHeightAboveWater, depth_clearance_height_for_min, depth_clearance_height_for_max);
	float fHeightWeight = (fClampedHeight - depth_clearance_height_for_min) / (depth_clearance_height_for_max - depth_clearance_height_for_min);

	float fMaxDepth = depth_clearance_min + ((depth_clearance_max-depth_clearance_min) * fHeightWeight);
	float fMaxDepthZ = fWaterHeight - fMaxDepth;

	vEnd.z = fWaterHeight - depth_for_test;

#if __DEV
	CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), 0.25f, Color_green, 2500, 0, false);	
#endif

	const u32 nIncludeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER	| 
		ArchetypeFlags::GTA_GLASS_TYPE		| 
		ArchetypeFlags::GTA_VEHICLE_TYPE	| 
		ArchetypeFlags::GTA_OBJECT_TYPE		|
		ArchetypeFlags::GTA_PED_TYPE;

	const CEntity* exclusionList[2];
	int nExclusions = 0;
	exclusionList[nExclusions++] = &rPed;
	exclusionList[nExclusions++] = bCollideWithIgnoreEntity ? NULL : rPed.GetRagdollOnCollisionIgnorePhysical();

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
	capsuleDesc.SetResultsStructure(&shapeTestResults);
	capsuleDesc.SetCapsule(vStart, vEnd, 0.25f);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetDoInitialSphereCheck(true);
	capsuleDesc.SetIncludeFlags(nIncludeFlags);
	capsuleDesc.SetExcludeEntities(exclusionList, nExclusions);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
	{
		//! If hit position is greater than max depth z, fail.
		if(shapeTestResults[0].GetHitPosition().z >= fMaxDepthZ) 
		{
#if __DEV
			CTask::ms_debugDraw.AddLine(shapeTestResults[0].GetPosition(), shapeTestResults[0].GetPosition() + shapeTestResults[0].GetIntersectedPolyNormal() * ScalarVFromF32(0.1f), Color_green, 2500);	
#endif
			return false;
		}

		//! if we have hit an entity that is moving upwards (i.e. a sub or somesuch), fail.
		else if(shapeTestResults[0].GetHitEntity() && shapeTestResults[0].GetHitEntity()->GetIsPhysical())
		{
			CPhysical *pPhysical = static_cast<CPhysical*>(shapeTestResults[0].GetHitEntity());
			if(pPhysical->GetVelocity().z > 0.0f)
			{
#if __DEV
				CTask::ms_debugDraw.AddLine(shapeTestResults[0].GetPosition(), shapeTestResults[0].GetPosition() + shapeTestResults[0].GetIntersectedPolyNormal() * ScalarVFromF32(0.1f), Color_green, 2500);	
#endif
				return false;
			}
		}
	}

	//Check if the position must be reachable.
	//Grab the ped position.
	static dev_float sf_FORWARD_TEST_OFFSET = 0.5f;
	static dev_float sf_FORWARD_TEST_Z_OFFSET = -0.2f;
	
	Vector3 vPedPosition;

	if(pOverridePedPosition)
	{
		vPedPosition = VEC3V_TO_VECTOR3(*pOverridePedPosition);
	}
	else
	{
		vPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
		vPedPosition.AddScaled(VEC3V_TO_VECTOR3(rPed.GetTransform().GetForward()), sf_FORWARD_TEST_OFFSET);
		vPedPosition.z += sf_FORWARD_TEST_Z_OFFSET;
	}

#if __DEV
	CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vPedPosition), VECTOR3_TO_VEC3V(vStart), 0.25f, Color_green, 2500, 0, false);	
#endif
		
	//Check for solid objects between the ped and the position.
	capsuleDesc.SetCapsule(vPedPosition, vStart, 0.25f);
	capsuleDesc.SetIncludeFlags(nIncludeFlags);
	capsuleDesc.SetExcludeEntity(&rPed);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
	{
#if __DEV
		CTask::ms_debugDraw.AddLine(shapeTestResults[0].GetPosition(), shapeTestResults[0].GetPosition() + shapeTestResults[0].GetIntersectedPolyNormal() * ScalarVFromF32(0.1f), Color_green, 2500);	
#endif
		return false;
	}

	return true;
}

void CTaskMotionSwimming::CleanUp()
{
	CPed * pPed = GetPed();
	pPed->SetDesiredBoundPitch(0.0f);
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning, false );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false );

	//Make weapon visible again
	CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	if (pWeapon && !pWeapon->GetIsVisible())
	{
		pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
	}

	// Clear any old filter
	if(m_pWeaponFilter)
	{
		m_pWeaponFilter->Release();
		m_pWeaponFilter = NULL;
	}
}

#if !__FINAL

const char * CTaskMotionSwimming::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Idle",
		"State_RapidsIdle",
		"State_SwimStart",
		"State_Swimming",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

void CTaskMotionSwimming::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	settings.m_CameraHash = g_InWaterCameraHash; 
}

//////////////////////////////////////////////////////////////////////////
// MoVE interface helper methods
//////////////////////////////////////////////////////////////////////////

void	CTaskMotionSwimming::SendParams()
{
	
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.SetFlag(m_bAnimationsReady, ms_AnimsLoadedId);

		CPed * pPed = GetPed();
		Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
		Vector2 vDesiredMBR = pPed->GetMotionData()->GetDesiredMoveBlendRatio();

		//if running into something, consider slowing down, cheap gait reduction B* 502395s
        if(!pPed->IsNetworkClone())
        {
		    Vector3 riverVel = pPed->m_Buoyancy.GetLastRiverVelocity();
		    float speed = (pPed->GetVelocity()-riverVel).Mag();
		    float clipSpeed = pPed->GetPreviousAnimatedVelocity().Mag();
		    float speedRatio = (clipSpeed !=0) ? speed/clipSpeed : 1.0f;		
		    static dev_float sfMinRunRatio = 0.7f;
		    if (vCurrentMBR.y >= MOVEBLENDRATIO_RUN && speedRatio< sfMinRunRatio) 
		    {
			    if (m_bPossibleGaitReduction) //avoids small fluxuations in speed ratios
			    {
				    vDesiredMBR.y = MOVEBLENDRATIO_WALK;
				    vCurrentMBR.y = MOVEBLENDRATIO_WALK;
			    }
			    else
				    m_bPossibleGaitReduction = true;
		    } else
			    m_bPossibleGaitReduction = false;
        }
		
		float fDirectionVar= CalcDesiredDirection();
		// Use the desired direction in the MoVE network to trigger transitions, etc (more responsive than using the smoothed one)
		TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fSwimHeadingLerpRate, 4.00f,0.0f,20.0f,0.001f);
		if (Sign(m_fPrevDirection) != Sign(fDirectionVar)) //direction flip
		{				
			m_fSwimHeadingLerpRate = 0;
			m_fPrevDirection = fDirectionVar;
		}				
		TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fSwimApproachApproachRate, 10.0f,0.0f,100.0f,0.1f);
		TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fTurnIntensity, 2.00f,0.0f,20.0f,0.001f);
		Approach(m_fSwimHeadingLerpRate, ms_fSwimHeadingLerpRate, ms_fSwimApproachApproachRate,  fwTimer::GetTimeStep());		
		fDirectionVar = Clamp(ms_fTurnIntensity * fDirectionVar, -PI,PI);
		InterpValue( m_fSmoothedDirection, fDirectionVar, m_fSwimHeadingLerpRate);
		
		m_MoveNetworkHelper.SetFloat( ms_HeadingId, ConvertRadianToSignal(m_fSmoothedDirection));	 	
		//Displayf("Swim heading: %f/%f Smoothed: %f/%f", fDirectionVar, ConvertRadianToSignal(fDirectionVar), m_fSmoothedDirection, ConvertRadianToSignal(m_fSmoothedDirection));

		// Set the overridden rate param for swim
		// ...Use the scripted override if set.
		if (pPed->GetMotionData()->GetDesiredMoveRateInWaterOverride() != 1.0f || pPed->GetMotionData()->GetCurrentMoveRateInWaterOverride() != 1.0f)
		{
			// Speed override lerp (same as in CTaskHumanLocomotion::ProcessMovement)
			TUNE_GROUP_FLOAT(PED_MOVEMENT, fTendToDesiredMoveRateOverrideMultiplierSwimming, 2.0, 1.0f, 20.0f, 0.5f);
			float fDeltaOverride = pPed->GetMotionData()->GetDesiredMoveRateInWaterOverride() - pPed->GetMotionData()->GetCurrentMoveRateInWaterOverride();
			fDeltaOverride *= (fTendToDesiredMoveRateOverrideMultiplierSwimming * fwTimer::GetTimeStep());
			fDeltaOverride += pPed->GetMotionData()->GetCurrentMoveRateInWaterOverride();
			pPed->GetMotionData()->SetCurrentMoveRateInWaterOverride(fDeltaOverride);
			m_MoveNetworkHelper.SetFloat(ms_MoveRateOverrideId, fDeltaOverride * m_fSwimRateScalar);
		}
		else
		{
			const CMotionTaskDataSet* pMotionTaskDataSet = CMotionTaskDataManager::GetDataSet(pPed->GetPedModelInfo()->GetMotionTaskDataSetHash().GetHash());
			taskAssert(pMotionTaskDataSet);		
			if (pPed->GetPlayerInfo())
				m_MoveNetworkHelper.SetFloat(ms_MoveRateOverrideId, pPed->GetPlayerInfo()->m_fSwimSpeedMultiplier * m_fSwimRateScalar * pMotionTaskDataSet->GetInWaterData()->m_MotionAnimRateScale);
			else
				m_MoveNetworkHelper.SetFloat(ms_MoveRateOverrideId, m_fSwimRateScalar * pMotionTaskDataSet->GetInWaterData()->m_MotionAnimRateScale);			
		}

		float fGoalHeading = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading()); 

		static dev_float s_fHeadingApproachRate = 3.0f;
		if (GetState()==State_Idle)
			m_fTargetHeading = fGoalHeading;
		else
			Approach(m_fTargetHeading, fGoalHeading, s_fHeadingApproachRate, fwTimer::GetTimeStep());

		m_MoveNetworkHelper.SetFloat(ms_TargetHeading, ConvertRadianToSignal(m_fTargetHeading)); 

		if (m_bSwimGaitReduced)
		{
			vDesiredMBR.y = MOVEBLENDRATIO_STILL;
			vCurrentMBR.y = MOVEBLENDRATIO_WALK;
		}

		// Some animals swimming can not just idle in place, they keep on moving, we use this moveblendratio because it is the first one that gets affected by the ms_MoveRateOverrideId parameter
		// TODO[AMPHIBIAN]: Rethink this once we have real swimming animations for animals
		if ( !m_bAllowIdle && vDesiredMBR.y < MOVEBLENDRATIO_RUN )
		{
			vCurrentMBR.y = MOVEBLENDRATIO_RUN;
			vDesiredMBR.y = MOVEBLENDRATIO_RUN;
		}

		// Smooth the direction parameter we'll be using to set blend weights in the network
		float fSpeed = vCurrentMBR.y / MOVEBLENDRATIO_SPRINT; // normalize the speed values between 0.0f and 1.0f		
		m_MoveNetworkHelper.SetFloat( ms_SpeedId, fSpeed);			
		m_MoveNetworkHelper.SetFloat( ms_DesiredSpeedId, vDesiredMBR.y / MOVEBLENDRATIO_SPRINT);
	
		m_MoveNetworkHelper.SetFlag(CheckForRapids(pPed), ms_InRapidsId);

		m_MoveNetworkHelper.SetFlag(pPed->GetIsFPSSwimming(), ms_FPSSwimming);
	}	

}

//////////////////////////////////////////////////////////////////////////
bool CTaskMotionSwimming::CheckForRapids(CPed* pPed)
{
	Vector3 vRiverHitPos;
	Vector3 vRiverHitNormal;
	bool bInRapids = false;
	if (pPed->m_Buoyancy.GetCachedRiverBoundProbeResult(vRiverHitPos, vRiverHitNormal))
	{
		Vector3 vRiverFlow = pPed->m_Buoyancy.GetLastRiverVelocity();	
		float fFlowStrength = vRiverFlow.Mag();

		//Check for rapids
		if (fFlowStrength > CTaskNMRiverRapids::sm_Tunables.m_fMinRiverFlowForRapids)
		{
			float fGroundClearance = -1.0f;

			float fWaterZ = 0.0f;
			WaterTestResultType eTestResult = CVfxHelper::GetWaterZ(pPed->GetTransform().GetPosition(), fWaterZ, pPed);
			if (eTestResult > WATERTEST_TYPE_NONE)
			{
				// Test from the water height level down 10 meters...
				// Add the ground clearance as padding to the probe - we then subtract it out again from the final value
				fGroundClearance = CTaskMotionDiving::TestGroundClearance(pPed, 10.0f, fWaterZ - pPed->GetTransform().GetPosition().GetZf() + CTaskNMRiverRapids::sm_Tunables.m_fMinRiverGroundClearanceForRapids, false);
				if (fGroundClearance > 0.0f)
				{
					// If we found a valid ground clearance (non-negative) then we never want to make it invalid... so clamp to 0.0f
					fGroundClearance = Max(fGroundClearance - CTaskNMRiverRapids::sm_Tunables.m_fMinRiverGroundClearanceForRapids, 0.0f);
				}
			}

			//Ensure there isn't ground just below us...
			if (fGroundClearance < 0.0f || fGroundClearance > CTaskNMRiverRapids::sm_Tunables.m_fMinRiverGroundClearanceForRapids)
			{
				bInRapids = true;
			}
		}
	}
	return bInRapids;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskMotionSwimming::CheckForWaterfall(CPed* pPed)
{
	Vector3 vRiverHitPos, vRiverHitNormal;
	bool bInRiver = pPed->m_Buoyancy.GetCachedRiverBoundProbeResult(vRiverHitPos, vRiverHitNormal);
	CEvent* pEvent = NULL;
	if (bInRiver && vRiverHitNormal.z < 0.7f) //waterfall
	{
		//if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
		//{
		//	if (!pPed->GetUsingRagdoll())
		//	{
		//		CTask* pNmTask = rage_new CTaskNMHighFall(500);
		//		CEventSwitch2NM event(5000, pNmTask, true, 500);
		//		pEvent = pPed->SwitchToRagdoll(event);			
		//	}
		//	else
		//	{
		//		CTaskNMBehaviour* pTask = pPed->GetPedIntelligence()->GetLowestLevelNMTask(pPed);
		//		// If we don't have a ragdoll task or the river is steep enough and we're only running an underwater or buoyancy NM task...
		//		if (vRiverHitNormal.z < 0.5f && (pTask == NULL || pTask->GetTaskType() == CTaskTypes::TASK_NM_RIVER_RAPIDS || pTask->GetTaskType() == CTaskTypes::TASK_NM_BUOYANCY))
		//		{
		//			CTask* pNmTask = rage_new CTaskNMHighFall(500);
		//			CEventSwitch2NM event(5000, pNmTask, true, 500);
		//			pEvent = pPed->GetPedIntelligence()->AddEvent(event);
		//		}
		//	}
		//}

		CEventInAir event(pPed);
		// Cannot add an in-air event while swimming...
		pPed->SetIsSwimming(false);
		pEvent = pPed->GetPedIntelligence()->AddEvent(event);
	}
	return pEvent != NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskMotionSwimming::GetHorseGaitPhaseAndSpeed(Vector4& phase, float& speed, float &turnPhase, float &slopePhase) 
{
	// Ensure the task has made it past the initialization state.
	// If not, the network helper is invalid.
	if(GetState() == State_Start )
	{
		return false;
	}

	// TODO[AMPHIBIAN]: Redo this once we have real swimming animations for animals.
	// movement phase
	m_MoveNetworkHelper.GetFloat( ms_SwimPhaseId, phase.x );;	// walk : get this one from the swimming phase from now
	phase.y = 0.0f;	// trot
	phase.z = 0.0f;	// canter
	phase.w = 0.0f;	// gallop
	// speed : set it to 0.25 by default for now. 
	speed = 0.25f;
	// turn phase
	turnPhase = m_TurnPhaseGoal;
	// slope phase
	slopePhase = 0.5f;

	return true;
}

bool	CTaskMotionSwimming::SyncToMoveState()
{

	if(m_bAnimationsReady && m_MoveNetworkHelper.IsNetworkActive())
	{
		s32 iState = -1;
		if (m_MoveNetworkHelper.GetBoolean( ms_OnEnterIdleSwimmingId))
		{
			iState = State_Idle; 
			m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterIdleSwimmingId);
		}
		else if (m_MoveNetworkHelper.GetBoolean( ms_OnEnterRapidsIdleSwimmingId))
		{
			iState = State_RapidsIdle; 
			m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterRapidsIdleSwimmingId);
		}
		else if(m_MoveNetworkHelper.GetBoolean( ms_OnEnterSwimStartId))
		{
			iState = State_SwimStart;		
			m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterSwimStartId);
		}
		else if(m_MoveNetworkHelper.GetBoolean( ms_OnEnterSwimmingId))
		{
			iState = State_Swimming;		
			m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterSwimmingId);
		}		

		if(iState != -1 && iState != GetState())
		{
			SendParams();
			SetState(iState);
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//Task motion base methods

void CTaskMotionSwimming::GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)
{
	fwMvClipSetId clipSetId = GetSwimmingClipSetId();
	taskAssert(clipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	fwClipSet *clipSet = fwClipSetManager::GetClipSet(clipSetId);
	taskAssert(clipSet);

	static const fwMvClipId s_walkClip("walk",0x83504C9C);
	static const fwMvClipId s_runClip("run",0x1109B569);
	static const fwMvClipId s_sprintClip("run",0x1109B569);

	const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };

	RetrieveMoveSpeeds(*clipSet, speeds, clipNames);

	return;
}

Vec3V_Out CTaskMotionSwimming::CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrixIn, float fTimestep)
{
	const Matrix34& updatedPedMatrix = RCC_MATRIX34(updatedPedMatrixIn);

	CPed* pPed = GetPed();
	if (!pPed->GetIsSwimming())
		return CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrixIn, fTimestep);

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnCalcDesiredVelocityOfFocusEntity(), pPed );

	Vector3	velocity(VEC3_ZERO);
	Vector3 extractedVel(pPed->GetAnimatedVelocity());

	velocity += extractedVel.y * updatedPedMatrix.b;
	velocity += extractedVel.x * updatedPedMatrix.a;
	if(pPed->GetUseExtractedZ())
	{
		velocity += extractedVel.z * updatedPedMatrix.c;
	}

	// Add in swimming resistance
	Vector3 vSwimmingResistanceVelChange = ProcessSwimmingResistance(fTimestep);
	velocity += vSwimmingResistanceVelChange;

	// Clamp the velocity change to avoid large changes
	Vector3 vDesiredVelChange = velocity - pPed->GetVelocity();
	vDesiredVelChange+=pPed->m_Buoyancy.GetLastRiverVelocity();

    // don't clamp the XY velocity change for network clones, the network blending code prevents
    // large velocity changes in a frame but needs to know the target velocity for the ped. No blending is
    // done to the ped's Z position when it is water, so this still needs to be clamped here
    if(!pPed->IsNetworkClone())
    {
		bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
		bFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif	//FPS_MODE_SUPPORTED
		if (bFPSMode)
		{
			float fVelocityZBeforeClamp = vDesiredVelChange.GetZ();
			ClampVelocityChange(vDesiredVelChange,fTimestep);
			vDesiredVelChange.z = fVelocityZBeforeClamp;
		}
		else
		{
			ClampVelocityChange(vDesiredVelChange,fTimestep);
		}
    }
    else
    {
        Vector3 velBeforeClamp = vDesiredVelChange;
        ClampVelocityChange(vDesiredVelChange,fTimestep);
        vDesiredVelChange.x = velBeforeClamp.x;
        vDesiredVelChange.y = velBeforeClamp.y;
    }

	velocity = pPed->GetVelocity() + vDesiredVelChange;

#if FPS_MODE_SUPPORTED
	// FPS Swim: Store velocity so we can transition to strafing smoothly
	// (Don't need to constantly check for input here like in CTaskMotionAiming as we will always have a velocity while in the Swimming task)
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
	{
		CTaskMotionPed* pTaskMotionPed = static_cast<CTaskMotionPed*>(GetParent());
		if (pTaskMotionPed)
		{
			pTaskMotionPed->SetCachedSwimVelocity(velocity);
		}
	}
#endif	//FPS_MODE_SUPPORTED

	Assertf(IsFiniteAll(VECTOR3_TO_VEC3V(velocity)), "CTaskMotionSwimming::CalcDesiredVelocity: desired velocity is invalid. PedVelocity: %f,%f,%f, vDesiredVelChange: %f,%f,%f, DesiredVelocity: %f,%f,%f", pPed->GetVelocity().GetX(), pPed->GetVelocity().GetY(), pPed->GetVelocity().GetZ(), vDesiredVelChange.GetX(), vDesiredVelChange.GetY(), vDesiredVelChange.GetZ(), velocity.GetX(), velocity.GetY(), velocity.GetZ());

    NetworkInterface::OnDesiredVelocityCalculated(*pPed);

	return VECTOR3_TO_VEC3V(velocity);
}

void CTaskMotionSwimming::ClampVelocityChange(Vector3& vChangeInAndOut, const float timeStep)
{
	// Limit the rate of change so we don't apply a crazy force to the ped.
	if(vChangeInAndOut.Mag2() > MBIW_FRICTION_VEL_CHANGE_LIMIT * MBIW_FRICTION_VEL_CHANGE_LIMIT * timeStep * timeStep)
	{
		vChangeInAndOut *= MBIW_FRICTION_VEL_CHANGE_LIMIT * timeStep / vChangeInAndOut.Mag();
	}
}

float CTaskMotionSwimming::GetHighestWaterLevelAroundPos(const CPed *pPed, const Vector3 vPos, const float fWaterLevelAtPos, Vector3 &vHighestWaterLevelPosition, const float fOffset)
{
	Vector3 vOffsetLeft = Vector3(-fOffset, 0.0f, 0.0f);
	Vector3 vOffsetRight = Vector3(fOffset, 0.0f, 0.0f);
	Vector3 vOffsetFront = Vector3(0.0f, fOffset, 0.0f);
	Vector3 vOffsetBack = Vector3(0.0f, -fOffset, 0.0f);

	// Test around FPS camera if in FPS mode, else offset around the peds position.
	camFirstPersonShooterCamera* pFpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
	if (pFpsCamera && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		Mat34V mFpsCamera = MATRIX34_TO_MAT34V(pFpsCamera->GetFrame().GetWorldMatrix());
		vOffsetLeft = vPos + VEC3V_TO_VECTOR3(Transform3x3(mFpsCamera, VECTOR3_TO_VEC3V(vOffsetLeft)));
		vOffsetRight = vPos + VEC3V_TO_VECTOR3(Transform3x3(mFpsCamera, VECTOR3_TO_VEC3V(vOffsetRight)));
		vOffsetFront = vPos + VEC3V_TO_VECTOR3(Transform3x3(mFpsCamera, VECTOR3_TO_VEC3V(vOffsetFront)));
		vOffsetBack = vPos + VEC3V_TO_VECTOR3(Transform3x3(mFpsCamera, VECTOR3_TO_VEC3V(vOffsetBack)));
	}
	else
	{
		vOffsetLeft = pPed->TransformIntoWorldSpace(vOffsetLeft);
		vOffsetRight = pPed->TransformIntoWorldSpace(vOffsetRight);
		vOffsetFront = pPed->TransformIntoWorldSpace(vOffsetFront);
		vOffsetBack = pPed->TransformIntoWorldSpace(vOffsetBack);
	}

	const int fArraySize = 4;
	Vector3 *vOffsetArray = rage_new Vector3[fArraySize];
	vOffsetArray[0] = vOffsetLeft;
	vOffsetArray[1] = vOffsetRight;
	vOffsetArray[2] = vOffsetFront;
	vOffsetArray[3] = vOffsetBack;

	float fWaterLevel = fWaterLevelAtPos;

	for (int i = 0; i < fArraySize; i++)
	{
		Vector3 vOffset = vOffsetArray[i];
		float fWaterLevelOverride = 0.0f;
		if (pPed->m_Buoyancy.GetWaterLevelIncludingRivers(vOffset, &fWaterLevelOverride, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pPed, pPed->GetCanUseHighDetailWaterLevel()))
		{
			// Override the water level if the detected level is higher.
			if (fWaterLevelOverride > fWaterLevel)
			{
				fWaterLevel = fWaterLevelOverride;
				vHighestWaterLevelPosition = vOffset;
			}

#if __DEV
			static dev_bool bRenderOffset = false;
			if (bRenderOffset)
			{
				vOffset.z = fWaterLevelOverride;
				CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(vOffset), 0.05f, Color_green, 1, 0, false);	
			}
#endif
		}
	}

	return fWaterLevel;
}

void CTaskMotionSwimming::ProcessFlowOrientation()
{
	CPed* pPed = GetPed();

	Vector3 vRiverFlow = pPed->m_Buoyancy.GetLastRiverVelocity();	
	float fFlowStrength = vRiverFlow.Mag();

	static dev_float MIN_RIVERFLOW_VELOCITY = 0.5f;
	if (fFlowStrength > MIN_RIVERFLOW_VELOCITY)
	{
		vRiverFlow.z=0; vRiverFlow.Normalize();
#if __DEV
		Vector3 debugStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()); debugStart.z+=1.0f;
		Vector3 debugEnd = debugStart + vRiverFlow;
		CTask::ms_debugDraw.AddArrow(VECTOR3_TO_VEC3V(debugStart), VECTOR3_TO_VEC3V(debugEnd), 0.1f, Color_red, 10);		
#endif
		vRiverFlow.Negate();
		Vector3 pedNormal(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()));
		float fHeadingDelta = -vRiverFlow.AngleZ(pedNormal);

		pedNormal.z=0; pedNormal.Normalize();
		float scale = 1.0f - vRiverFlow.Dot(pedNormal);

		static dev_float maxHeadingChangeRate = PI/3.0f;		
		static dev_float minHeadingChangeRate = 0.2f;
		float headingChangeRate = Lerp(Min(1.0f, scale),  minHeadingChangeRate, maxHeadingChangeRate);

		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(Sign(fHeadingDelta) * Min(Abs(fHeadingDelta), headingChangeRate * fwTimer::GetTimeStep()));
	}
}

Vector3 CTaskMotionSwimming::ProcessSwimmingResistance(float fTimestep)
{
	Vector3 desiredVel(VEC3_ZERO);
	Vector3 vExtractedVel;

	CPed* pPed = GetPed();

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		bFPSMode = true;
	}
#endif	//FPS_MODE_SUPPORTED

	vExtractedVel = pPed->GetAnimatedVelocity();

	static dev_float sfWaterSurfaceSwimVelScale = 1.0f;

	vExtractedVel *= sfWaterSurfaceSwimVelScale;

	const Matrix34  mPedMat = MAT34V_TO_MATRIX34(pPed->GetMatrix());
	desiredVel += mPedMat.a * vExtractedVel.x;
	desiredVel += mPedMat.b * vExtractedVel.y;

	Vector3 desiredVelChange = desiredVel - pPed->GetVelocity();

	// Clamp the velocity change before we apply the water surface correction	
	if(desiredVelChange.Mag2() > MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT * MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT * fTimestep * fTimestep)
	{
		desiredVelChange *=  MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT * fTimestep / desiredVelChange.Mag();
	}

	Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// B*2257378: FPS: Get water level at camera X/Y position as it's slightly in front of the ped position (particularly when idle).
	if (bFPSMode)
	{
		const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
		if(pDominantRenderedCamera && pDominantRenderedCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
		{
			Vector3 vFPSCamPos = camInterface::GetPos();
			vPedPosition.x = vFPSCamPos.x;
			vPedPosition.y = vFPSCamPos.y;
		}
	}

	Vector3 newPos = vPedPosition + ((pPed->GetVelocity() + desiredVelChange) * fTimestep);

	float fWaterLevel = 0.0f;
	m_fLastWaveDelta = 0.0f;
	if(pPed->m_Buoyancy.GetWaterLevelIncludingRivers(newPos, &fWaterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pPed, pPed->GetCanUseHighDetailWaterLevel()))
	{
#if __DEV
		Vector3 wavePos = vPedPosition; wavePos.z = fWaterLevel;
		CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(wavePos), 0.05f, Color_yellow, 1, 0, false);	
#endif

		static dev_float WATERHEIGHT_APPROACH_RATE = 0.125f; 	
		static dev_float MAX_WAVE_HEIGHT = 1.0f; 	
		Vector3 vRiverHitPos, vRiverHitNormal;
		bool bInRiver = pPed->m_Buoyancy.GetCachedRiverBoundProbeResult(vRiverHitPos, vRiverHitNormal) && (vRiverHitPos.z+REJECTIONABOVEWATER) > newPos.z;

		// B*2257378: FPS: Test water level at several points around ped position to check for large incoming waves. Helps ensure we always keep our head above water.
		Vector3 vHighestWaterLevelPosition = vPedPosition;
		if (bFPSMode)
		{
			fWaterLevel = CTaskMotionSwimming::GetHighestWaterLevelAroundPos(pPed, vPedPosition, fWaterLevel, vHighestWaterLevelPosition);
		}

		float waterHeight = MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER_NOWAVES;
		if (GetState() == State_Start && GetPed()->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_STILL)
			waterHeight = MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER_SURFACING;
		else if (!bInRiver) //scale based on wave height (not in FPS mode, we want to keep at a consistent height above water).
		{
			Water::GetWaterLevel(vHighestWaterLevelPosition, &m_fLastWaveDelta, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL, NULL, pPed->GetCanUseHighDetailWaterLevel());
			float fNoWaves = m_fLastWaveDelta;
			Water::GetWaterLevelNoWaves(vHighestWaterLevelPosition, &fNoWaves, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
			m_fLastWaveDelta -= fNoWaves;

			if (!bFPSMode)
			{
				float t = Min(1.0f, m_fLastWaveDelta/MAX_WAVE_HEIGHT);
				waterHeight = MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER_NOWAVES + (t*(MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER-MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER_NOWAVES));
			}
		}
		
		static dev_float FEMALE_CLIP_HEIGHT_OFFSET = -0.075f;
		if (!pPed->IsMale())
			waterHeight+=FEMALE_CLIP_HEIGHT_OFFSET;

		if (bFPSMode)
		{
			waterHeight += CTaskMotionSwimming::ms_fFPSClipHeightOffset;
		}

		// Lerp to the water height goal in FPS mode (this may be passed in from the swimming task and we don't want to instantly pop).
		if (bFPSMode && m_fHeightBelowWaterGoal != waterHeight && m_fHeightBelowWaterGoal != CTaskMotionSwimming::s_fHeightBelowWaterGoal)
		{		
			static dev_float WATERHEIGHT_GOAL_APPROACH_RATE_FPS = 0.33f;
			Approach(m_fHeightBelowWaterGoal, waterHeight, WATERHEIGHT_GOAL_APPROACH_RATE_FPS, fTimestep);
		}
		else
		{
			m_fHeightBelowWaterGoal = waterHeight; // rage::Max(waterHeight, fWaterLevel - vPedPosition.z);		
		}

		// Check a little bit further than we will swim under boats to avoid instabilities
		CVehicle* vehicle = GetVehAbovePed(pPed, MBIW_SWIM_UNDER_BOAT_HEIGHT_BELOW_WATER + 0.75f);
		if(vehicle && (vehicle->GetVehicleType() == VEHICLE_TYPE_BOAT))
		{
			m_fHeightBelowWaterGoal += MBIW_SWIM_UNDER_BOAT_HEIGHT_BELOW_WATER;	// Drop the player way below the water's surface so they can escape the boat
		}

		if (bFPSMode)
		{
			m_fHeightBelowWater = m_fHeightBelowWaterGoal;
		}
		else
		{
			Approach(m_fHeightBelowWater, m_fHeightBelowWaterGoal, WATERHEIGHT_APPROACH_RATE, fTimestep);
		}

#if __DEV
		wavePos.z = m_fHeightBelowWater + vPedPosition.z;
		CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(wavePos), 0.05f, Color_green, 1, 0, false);	
		wavePos.z = m_fHeightBelowWaterGoal + vPedPosition.z;
		CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(wavePos), 0.05f, Color_red, 1, 0, false);	

#endif
	
		// AS we are floating on waters surface, track waves
		// Try to match peds vertical velocity with the water's
		// Desired displacement = water height - Height below water - currentPosition.z
		//Displayf("m_fHeightBelowWaterGoal = %f, Actual = %f, PedPos = %f,  fWaterLevel = %f",m_fHeightBelowWaterGoal, m_fHeightBelowWater, vPedPosition.z, fWaterLevel);
		fWaterLevel -= m_fHeightBelowWater + vPedPosition.z;	


		float reqZSpeed = fWaterLevel / fTimestep;

		// Smooth Z speed if changes are small.
		if (bFPSMode)
		{
			float fCurrentZSpeed = pPed->GetVelocity().z;
			static dev_float fZSpeedDeltaThreshold = 0.1f;
			if (Abs(reqZSpeed - fCurrentZSpeed) < fZSpeedDeltaThreshold)
			{
				static dev_float fRate = 0.6f;
				Approach(fCurrentZSpeed, reqZSpeed, fRate, fTimestep);
				reqZSpeed = fCurrentZSpeed;
			}
		}

		desiredVelChange.z = reqZSpeed;// - pPed->GetVelocity().z;

		// Clamp this vel change. Note this is done separately to the MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT so that wave intensity wont affect the XY swim speed
		if (bFPSMode)
		{
			static dev_float FPS_ACCEL_DECEL_LIMIT = 8.0f;
			desiredVelChange.z = Clamp(desiredVelChange.z, -FPS_ACCEL_DECEL_LIMIT, FPS_ACCEL_DECEL_LIMIT);
		}
		else
		{
			desiredVelChange.z = Clamp(desiredVelChange.z, -MBIW_SWIM_BUOYANCY_DECEL_LIMIT, MBIW_SWIM_BUOYANCY_ACCEL_LIMIT);
		}

		float storedVel = desiredVelChange.z;
		desiredVelChange.z = m_PreviousVelocityChange;		
		static float sfRiverVelApproachRate = 2.0f;
		

		if (bInRiver)
			Approach(desiredVelChange.z, storedVel, sfRiverVelApproachRate, fTimestep);
		else 
			desiredVelChange.z = storedVel;
		m_PreviousVelocityChange = desiredVelChange.z;
		//Displayf("desiredVelChange = %f, Actual = %f, fWaterLevel = %f",storedVel, desiredVelChange.z, fWaterLevel);
	}	// End in-water check
	
	pPed->m_Buoyancy.SetShouldStickToWaterSurface(true);

	// In case pPed is in rag doll (shouldn't really happen)
	if(!pPed->GetUsingRagdoll() && pPed->GetCurrentPhysicsInst()->IsInLevel())
	{
		Assertf(IsFiniteAll(VECTOR3_TO_VEC3V(desiredVelChange)), "CTaskMotionSwimming::ProcessSwimmingResistance: desired velocity change is invalid. desiredVelChange: %f,%f,%f. Water Level: %f. Using high detail water level: %d", desiredVelChange.GetX(), desiredVelChange.GetY(), desiredVelChange.GetZ(), fWaterLevel, pPed->GetCanUseHighDetailWaterLevel());
		// Apply new speed to character by adding force to centre of gravity
		return desiredVelChange;
	}

	return VEC3_ZERO;
}

CVehicle *CTaskMotionSwimming::GetVehAbovePed(CPed *ped, float testDistance)
{
	const int numIntersections = 2;
	WorldProbe::CShapeTestHitPoint probeIsects[numIntersections];
	WorldProbe::CShapeTestResults probeResults(probeIsects, numIntersections);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResults);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
	probeDesc.SetStartAndEnd(vPedPosition, vPedPosition+Vector3(0.0f,0.0f, testDistance));
	probeDesc.SetExcludeEntity(ped);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

	// See if we found any vehicles
	//for(int i = 0; i < numResults; i++)
	WorldProbe::ResultIterator it;
	for(it = probeResults.begin(); it < probeResults.last_result(); ++it)
	{
		CEntity* entity = static_cast<CEntity*>(it->GetHitInst()->GetUserData());
		if(entity && entity->GetIsTypeVehicle())
		{
			return static_cast<CVehicle*>(entity);
		}
	}

	return NULL;	
}

CTask * CTaskMotionSwimming::CreatePlayerControlTask()
{
	CTask *playerTask = rage_new CTaskPlayerOnFoot();
	return playerTask;
}

bool CTaskMotionSwimming::IsInMotion(const CPed * UNUSED_PARAM(pPed)) const
{
	bool isMoving = (GetMotionData().GetCurrentMbrX() != 0.0f) || (GetMotionData().GetCurrentMbrY() != 0.0f);
	return isMoving;
}

fwMvClipSetId CTaskMotionSwimming::GetSwimmingClipSetId()
{
	if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsScuba))
	{
		static fwMvClipSetId s_defaultSwimmingSet("move_ped_Swimming_scuba",0xA1B871B0);
		return s_defaultSwimmingSet;
	}

#if FPS_MODE_SUPPORTED
	// Use FPS swimming clipset if in FPS mode
	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return CLIP_SET_FPS_SWIMMING;
	}
#endif	//FPS_MODE_SUPPORTED

	fwMvClipSetId swimmingClipSetId;
	CTaskMotionBase* pedTask = GetPed()->GetPrimaryMotionTask();
	if (pedTask)
	{
		swimmingClipSetId = pedTask->GetDefaultSwimmingClipSet();
	}

	return swimmingClipSetId;
}

bool  CTaskMotionSwimming::StartMoveNetwork()
{
	CPed *pPed = GetPed(); 

	if (!m_MoveNetworkHelper.IsNetworkAttached() || !m_bAnimationsReady) {
		if (!m_MoveNetworkHelper.IsNetworkActive())
		{		
			m_MoveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkMotionSwimming);

			if (m_MoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkMotionSwimming))
			{
				m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkMotionSwimming);

				//Setup base animset 
				static const fwMvClipSetVarId baseAnimSet("Base",0x44E21C90);	
				static fwMvClipSetId s_swimmingBaseClipSetId("move_ped_swimming_base",0xB2143FC);
				m_MoveNetworkHelper.SetClipSet(s_swimmingBaseClipSetId, baseAnimSet);
			}
		}

		//Setup swimming set
		fwMvClipSetId clipSetId = GetSwimmingClipSetId();
		fwClipSet *clipSet = fwClipSetManager::GetClipSet(clipSetId);
		taskAssert(clipSet);

		if (clipSet->Request_DEPRECATED() && m_MoveNetworkHelper.IsNetworkActive())
		{
			m_bAnimationsReady = true;
			m_MoveNetworkHelper.SetClipSet( clipSetId);

			if (m_initialState > State_Start)
			{
				static const fwMvStateId s_moveStateSignals[State_Exit] = 
				{		
					fwMvStateId(),
					fwMvStateId("idle",0x71C21326),
					fwMvStateId("SwimStart",0xEC4259D5),
					fwMvStateId("swimming",0x5DA2A411)
				};

				if (s_moveStateSignals[m_initialState].GetHash()!=0)
				{					
					m_MoveNetworkHelper.ForceStateChange( s_moveStateSignals[m_initialState] );
				}		
			}
		}
		if (!m_MoveNetworkHelper.IsNetworkAttached()) 
		{
			m_MoveNetworkHelper.DeferredInsert();
			SendParams();
		}
	}
	else
	{
		SetState(m_initialState);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// State implementations

//////////////////////////////////////////////////////////////////////////
//start state

CTask::FSM_Return					CTaskMotionSwimming::Start_OnUpdate()
{
	StartMoveNetwork();	

	// If ped has a water-enabled gun equipped, hide it so we can still swim
	HideGun();

	return FSM_Continue;
}

void CTaskMotionSwimming::Start_OnExit()
{
	PitchCapsuleBound();
	RequestProcessMoveSignalCalls();
}


//////////////////////////////////////////////////////////////////////////
//start state

CTask::FSM_Return				CTaskMotionSwimming::Idle_OnUpdate()
{
	// If ped has a water-enabled gun equipped, hide it so we can still swim
	HideGun();

	//river re-orientation
	static dev_float TIME_BEFORE_FLOW_ORIENTAION = 0.5f;
	if (GetTimeInState() >= TIME_BEFORE_FLOW_ORIENTAION)
	{
		ProcessFlowOrientation();
	}

	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	return FSM_Continue; 
}

//////////////////////////////////////////////////////////////////////////
// Start rapids behaviour

void							CTaskMotionSwimming::RapidsIdle_OnEnter()
{
	if(GetPed() && !GetPed()->IsNetworkClone())
	{
		GetPed()->GetPedIntelligence()->AddTaskMovementResponse(rage_new CTaskMoveStandStill(CTaskMoveStandStill::ms_iStandStillTime, true));
	}
}

CTask::FSM_Return				CTaskMotionSwimming::RapidsIdle_OnUpdate()
{
	// If ped has a water-enabled gun equipped, hide it so we can still swim
	HideGun();

	//river re-orientation
	ProcessFlowOrientation();

	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if(m_uNextStruggleTime == 0)
	{
		m_uNextStruggleTime = fwTimer::GetGameTimer().GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinStruggleTime, sm_Tunables.m_MaxStruggleTime);
	}
	else if(fwTimer::GetGameTimer().GetTimeInMilliseconds() >= m_uNextStruggleTime)
	{
		m_MoveNetworkHelper.SendRequest(ms_StruggleId);
		m_MoveNetworkHelper.WaitForTargetState(ms_OnExitStruggleSwimmingId);
		m_uNextStruggleTime = 0;
	}

	return FSM_Continue; 
}

void						CTaskMotionSwimming::RapidsIdle_OnExit()
{
	if(GetPed() && !GetPed()->IsNetworkClone())
	{
		GetPed()->GetPedIntelligence()->AddTaskMovementResponse(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////
// Start swim behaviour

void						CTaskMotionSwimming::SwimStart_OnEnter()
{
	CPed* pPed = GetPed(); 

	//there's no turn in run versions of these clips, so we'll need to turn the ped directly here in the task
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fSwimStartHeadingLerpRate, 1.5f,0.0f,20.0f,0.001f);
	m_fSwimStartHeadingLerpRate = pPed->GetMotionData()->GetDesiredMbrY() >= MOVEBLENDRATIO_RUN ? ms_fSwimStartHeadingLerpRate : 0.0f;	
	m_fTargetHeading = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading()); //pop heading
}

CTask::FSM_Return					CTaskMotionSwimming::SwimStart_OnUpdate()
{
	// If ped has a water-enabled gun equipped, hide it so we can still swim
	HideGun();

	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	float CurrentHeading  = pPed->GetCurrentHeading();

	float HeadingDelta = InterpValue(CurrentHeading, pPed->GetDesiredHeading(), m_fSwimStartHeadingLerpRate, true); 

	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(HeadingDelta);
	
	float targetdelta = abs(SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading())); 
	
	m_MoveNetworkHelper.SetFloat( ms_TargetHeadingDelta, targetdelta);

	return FSM_Continue;
}
void						CTaskMotionSwimming::SwimStart_OnExit()
{

}


//////////////////////////////////////////////////////////////////////////
// swimming behaviour

void						CTaskMotionSwimming::Swimming_OnEnter()
{
	static dev_float STARTING_SWIM_RATE = 0.7f;
	m_fSwimRateScalar = STARTING_SWIM_RATE;
	m_uBreathTimer = fwTimer::GetGameTimer().GetTimeInMilliseconds();
}
CTask::FSM_Return					CTaskMotionSwimming::Swimming_OnUpdate()
{
	// If ped has a water-enabled gun equipped, hide it so we can still swim
	HideGun();

	// Wait for Move network to be in sync
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	static dev_u32 s_uIntervalBetweenBreaths = 3000;
	if ((m_uBreathTimer+s_uIntervalBetweenBreaths) < fwTimer::GetGameTimer().GetTimeInMilliseconds())
	{
		if (m_MoveNetworkHelper.GetBoolean(ms_BreathCompleteId))
		{
			m_uBreathTimer = fwTimer::GetGameTimer().GetTimeInMilliseconds();
		}
		else
		{
			m_MoveNetworkHelper.SendRequest(ms_BreathId);
		}
	}

	static dev_float SWIM_RATE_APPROACH_SPEED = 0.25f;
	Approach(m_fSwimRateScalar, 1.0f, SWIM_RATE_APPROACH_SPEED, fwTimer::GetTimeStep());
	return FSM_Continue;
}
void						CTaskMotionSwimming::Swimming_OnExit()
{

}


void CTaskMotionSwimming::SetWeaponClipSet(const fwMvClipSetId& clipSetId, const fwMvFilterId& filterId, bool bUpperBodyShadowExpression)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

	if(m_WeaponClipSetId != clipSetId)
	{

		// Store the new clip set
		m_WeaponClipSetId = clipSetId;

		bool bClipSetStreamed = false;
		if(m_WeaponClipSetId == CLIP_SET_ID_INVALID)
		{
			// Clip set doesn't need to be streamed
			bClipSetStreamed = true;
			m_WeaponClipSet = NULL;
		}
		else
		{
			m_WeaponClipSet = fwClipSetManager::GetClipSet(m_WeaponClipSetId);
			if(taskVerifyf(m_WeaponClipSet, "Weapon clip set [%s] doesn't exist", m_WeaponClipSetId.GetCStr()))
			{
				if(m_WeaponClipSet->Request_DEPRECATED())
				{
					// Clip set streamed
					bClipSetStreamed = true;
				}
			}
		}

		if(m_MoveNetworkHelper.IsNetworkActive() && bClipSetStreamed)
		{
			// Send immediately
			SendWeaponClipSet();
		}
		else
		{
			// Flag the change
			m_Flags.SetFlag(Flag_WeaponClipSetChanged);
		}
	}

	if(m_WeaponFilterId != filterId)
	{
		// Clear any old filter
		if(m_pWeaponFilter)
		{
			m_pWeaponFilter->Release();
			m_pWeaponFilter = NULL;
		}

		// Store the new filter
		m_WeaponFilterId = filterId;

		if(m_WeaponFilterId != FILTER_ID_INVALID)
		{
			if(GetEntity())
			{
				// Store now
				StoreWeaponFilter();
			}
			else
			{
				// Flag the change
				m_Flags.SetFlag(Flag_WeaponFilterChanged);
			}
		}
		else
		{
			// Clear any pending flags, as the filter is now set to invalid
			m_Flags.ClearFlag(Flag_WeaponFilterChanged);
		}
	}

	//Set weapon holding filter so we don't get stuck in place when reloading whilst swimming
	if(m_pWeaponFilter)
	{
		m_MoveNetworkHelper.SetFilter(m_pWeaponFilter, CTaskHumanLocomotion::ms_WeaponHoldingFilterId);
	}

	if(bUpperBodyShadowExpression)
	{
		m_Flags.SetFlag(Flag_UpperBodyShadowExpression);
	}
	else
	{
		m_Flags.ClearFlag(Flag_UpperBodyShadowExpression);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionSwimming::SendWeaponClipSet()
{
	m_MoveNetworkHelper.SetClipSet(m_WeaponClipSetId, CTaskHumanLocomotion::ms_WeaponHoldingClipSetVarId);

	bool bUseWeaponHolding = m_WeaponClipSetId != CLIP_SET_ID_INVALID;

	m_MoveNetworkHelper.SetFlag(bUseWeaponHolding, CTaskHumanLocomotion::ms_UseWeaponHoldingId);

	// Set weapon holding clip (default: "IDLE").
	if (bUseWeaponHolding)
	{
		static const fwMvClipId idleClipId("IDLE",0x71c21326);
		crClip *pClip = fwClipSetManager::GetClip(m_WeaponClipSetId, idleClipId);
		if (pClip)
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_WeaponHoldingClipId);
		}

		/*
		// Removed this as swimming with knuckle duster grip looks a bit weird!! 
		if (GetPed() && GetPed()->GetEquippedWeaponInfo() && GetPed()->GetEquippedWeaponInfo()->GetIsMeleeFist())
		{
			static const fwMvClipId meleeFistIdleClipID("GRIP_IDLE",0x3ec63b58);
			pClip = fwClipSetManager::GetClip(m_WeaponClipSetId, meleeFistIdleClipID);
			if (pClip)
			{
				m_MoveNetworkHelper.SetClip(pClip, ms_WeaponHoldingClipId);
			}
		}
		*/
		m_MoveNetworkHelper.SendRequest(CTaskHumanLocomotion::ms_RestartWeaponHoldingId);
	}

	// Don't do this again
	m_Flags.ClearFlag(Flag_WeaponClipSetChanged);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionSwimming::StoreWeaponFilter()
{
	if(taskVerifyf(m_WeaponFilterId != FILTER_ID_INVALID, "Weapon filter [%s] is invalid", m_WeaponFilterId.GetCStr()))
	{
		m_pWeaponFilter = CGtaAnimManager::FindFrameFilter(m_WeaponFilterId.GetHash(), GetPed());
		if(taskVerifyf(m_pWeaponFilter, "Failed to get filter [%s]", m_WeaponFilterId.GetCStr()))	
		{
			m_pWeaponFilter->AddRef();
			m_Flags.ClearFlag(Flag_WeaponFilterChanged);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Ped diving motion task
//////////////////////////////////////////////////////////////////////////

const fwMvBooleanId CTaskMotionDiving::ms_OnEnterGlideId("OnEnter_Glide",0xA89FD883);
const fwMvBooleanId CTaskMotionDiving::ms_OnEnterIdleId("OnEnter_Idle",0x1941AC09);
const fwMvBooleanId CTaskMotionDiving::ms_OnEnterStartSwimmingId("OnEnter_StartSwimming",0x509972A8);
const fwMvBooleanId CTaskMotionDiving::ms_OnEnterSwimmingId("OnEnter_Swimming",0x912B6EC2);
const fwMvFlagId CTaskMotionDiving::ms_AnimsLoadedId("AnimsLoaded",0x20D334DE);
const fwMvFlagId CTaskMotionDiving::ms_AnimsLoadedToIdleId("AnimsLoadedToIdle",0x91cdc66a);
const fwMvFlagId CTaskMotionDiving::ms_UseAlternateRunFlagId("UseAlternateRun",0x250022C6);
const fwMvClipId CTaskMotionDiving::ms_AlternateRunClipId("AlternateRun_Clip",0x8A2EF7F);
const fwMvFloatId CTaskMotionDiving::ms_DesiredSpeedId("desiredspeed",0xCF7BA842);
const fwMvFloatId CTaskMotionDiving::ms_HeadingId("heading",0x43F5CF44);
const fwMvFloatId CTaskMotionDiving::ms_SpeedId("speed",0xF997622B);
const fwMvFloatId CTaskMotionDiving::ms_PitchPhaseId("PitchPhase",0xDA36894E);
const fwMvFloatId CTaskMotionDiving::ms_MoveRateOverrideId("MoveRateOverride",0x9968532B);
const fwMvRequestId CTaskMotionDiving::ms_RestartState("RestartState", 0xb582bfd6);
const fwMvBooleanId CTaskMotionDiving::ms_IsFPSMode("InFPSMode", 0xf6892a5e);
const fwMvFloatId CTaskMotionDiving::ms_StrafeDirectionId("StrafeDirection", 0xcf6aa9c6);


const fwMvClipId CTaskMotionDiving::ms_IdleWeaponHoldingClipId("IdleWeaponHoldingClipId", 0xa28d8e18);
const fwMvClipId CTaskMotionDiving::ms_GlideWeaponHoldingClipId("GlideWeaponHoldingClipId", 0x2474729c);
const fwMvClipId CTaskMotionDiving::ms_SwimRunWeaponHoldingClipId("SwimRunWeaponHoldingClipId", 0xcb94d4c9);
const fwMvClipId CTaskMotionDiving::ms_SwimSprintWeaponHoldingClipId("SwimSprintWeaponHoldingClipId", 0xfc6d9ab0);

CTaskMotionDiving::~CTaskMotionDiving()
{

}


//////////////////////////////////////////////////////////////////////////
// FSM methods

CTask::FSM_Return CTaskMotionDiving::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	m_fGroundClearance = TestGroundClearance(pPed, 1.5f);
	ProcessMovement();
	UpdateDiveGaitReduction(pPed, m_bDiveGaitReduced, m_fGaitReductionHeading);
	PitchCapsuleBound();

	if (pPed)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset, true);
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning, true );		
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true);
#if FPS_MODE_SUPPORTED
		if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterTurnSettings, true );
		}
#endif	//FPS_MODE_SUPPORTED
	}

	if (GetState()==State_DivingDown)
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);

	return FSM_Continue;
}

CTaskMotionDiving::FSM_Return CTaskMotionDiving::ProcessPostFSM()
{
	// Send signals valid for all states
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		if(m_Flags.IsFlagSet(Flag_WeaponClipSetChanged))
		{
			if(m_WeaponClipSetId == CLIP_SET_ID_INVALID)
			{
				SendWeaponClipSet();
			}
			else
			{
				if(m_WeaponClipSet)
				{
					if(m_WeaponClipSet->Request_DEPRECATED())
					{
						SendWeaponClipSet();
					}
				}
			}
		}

		if(m_Flags.IsFlagSet(Flag_WeaponFilterChanged))
		{
			StoreWeaponFilter();
		}

		if(m_pWeaponFilter)
		{
			m_MoveNetworkHelper.SetFilter(m_pWeaponFilter, CTaskHumanLocomotion::ms_WeaponHoldingFilterId);
		}
	}

	return FSM_Continue;
}

bool CTaskMotionDiving::ProcessPostMovement()
{
	//! If motion task is out of sync with primary tree, we can be processing this when attached. If that is the case, just bail
	//! as it'll assert.
	if(GetPed()->IsNetworkClone() && GetPed()->GetIsAttached())
	{
		return false;
	}

	if (GetState()==State_DivingDown && m_fDivingDownCorrectionCurrent > 0 && !m_bDivingDownBottomedOut)
	{
		float fDiveUnderPhase = GetParent()->GetMoveNetworkHelper()->GetFloat(CTaskMotionPed::ms_DiveUnderPhaseId);		
		static dev_float s_fStartDiveUnderCorrectionPhase = 0.2f;
		static dev_float s_fEndDiveUnderCorrectionPhase = 0.8f;
		if (fDiveUnderPhase >= s_fStartDiveUnderCorrectionPhase)
		{
			float correction = 0;
			float t = (fDiveUnderPhase - s_fStartDiveUnderCorrectionPhase) / (s_fEndDiveUnderCorrectionPhase - s_fStartDiveUnderCorrectionPhase);
			if (t >= 1.0f)
			{
				correction = m_fDivingDownCorrectionCurrent;
				m_fDivingDownCorrectionCurrent = 0;
			}
			else
			{
				float ratio = 1.0f - m_fDivingDownCorrectionCurrent/m_fDivingDownCorrectionGoal;
				float delta = t-ratio;
				correction = delta * m_fDivingDownCorrectionGoal;
				m_fDivingDownCorrectionCurrent -= correction;
			}
			Vector3 vCurrentPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
			vCurrentPos.z -= correction;
			GetPed()->SetPosition(vCurrentPos, true, true, true);
#if GTA_REPLAY
			// We need to reset this flag as 'SetPosition' above is saying 'Warp==true'...but it's not actually moving a large distance...
			// This results in the ped not interpolating when diving under the water. (url:bugstar:2286751)
			CReplayMgr::CancelRecordingWarpedOnEntity(GetPed());
#endif
			return true;
		}		
	}
	return false;
}


void CTaskMotionDiving::ProcessMovement()
{

	CPed* pPed = GetPed();
	//************************************
	// Smooth forward movement request
	//

	const float fMoveAccel = 4.0f;
	const float fMoveDecel = 2.0f;

	ApproachCurrentMBRTowardsDesired(fMoveAccel, fMoveDecel, MOVEBLENDRATIO_SPRINT);

	// TODO - move this down to the base somewhere?
	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f); // need to reset this or we'll just keep turning

	//***********************
	// Smooth pitch request
	//

	float fDesiredPitch = Clamp( pPed->GetDesiredPitch(), -HALF_PI, HALF_PI );

	//straighten out if near the bottom
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fGroundLevelClearance, 0.5f,0.0f,3.0f,0.01f);
	static dev_float sfGroundLevelLerpRange = 0.5f;

	if (m_fGroundClearance > 0.0f) {
		float t = Min(1.0f, Max(0.0f, (m_fGroundClearance - ms_fGroundLevelClearance))/sfGroundLevelLerpRange);
		float minPitch = Lerp(t, 0.0f, -HALF_PI); 
		fDesiredPitch = Max(fDesiredPitch, minPitch);
	}

	pPed->SetDesiredPitch( fwAngle::LimitRadianAngleForPitch(fDesiredPitch) );

	Assert(pPed->GetDesiredPitch() >= -HALF_PI && pPed->GetDesiredPitch() <= HALF_PI);

	//**********************************
	// Calculate desired pitch change
	//

	const float fPitchDelta = pPed->GetDesiredPitch() - pPed->GetCurrentPitch();
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingInterppitch, 1.0f,0.0f,20.0f,0.001f);
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fMaxDivingInterppitch, 1.500f,0.0f,20.0f,0.001f);
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fPitchApproachRate, 0.5f,0.0f,20.0f,0.001f);	
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fGlidingPitchScale, 0.5f,0.0f,20.0f,0.001f);	

	//Check for reset 
	bool gliding = (GetState() == State_Glide);
	bool idling = (GetState() == State_Idle);
	if (fPitchDelta > 0 && !m_bPositivePitchChange)
	{
		m_bPositivePitchChange = true;
		m_fPitchRate = 0; 
	} 
	else if (fPitchDelta < 0 && m_bPositivePitchChange)
	{
		m_bPositivePitchChange = false;
		m_fPitchRate = 0; 
	} else
	{
		Approach(m_fPitchRate, idling ? 0.0f : (gliding ? ms_fMaxDivingInterppitch * ms_fGlidingPitchScale : ms_fMaxDivingInterppitch), ms_fPitchApproachRate, fwTimer::GetTimeStep());
	}	
	//Displayf("Pitch rate: %f Positive?: %s", m_fPitchRate, m_bPositivePitchChange ? "true" : "false");
		
	GetMotionData().SetExtraPitchChangeThisFrame(GetMotionData().GetExtraPitchChangeThisFrame() + fPitchDelta * fwTimer::GetTimeStep() * m_fPitchRate);

	//Update forward drift
	static dev_float sf_MaxForwardDrift = 2.0f;
	static dev_float sf_MaxForwardDriftApproachRate = 2.0f;
	Approach(m_fForwardDriftScale, m_bApplyForwardDrift ? sf_MaxForwardDrift : 0.0f, sf_MaxForwardDriftApproachRate, fwTimer::GetTimeStep());	
	m_bApplyForwardDrift = false;

#if FPS_MODE_SUPPORTED
	// Process strafing direction if in FPS mode and swimming
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && GetState() == State_Swimming)
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		if (pControl)
		{
			float fLeftStickX = pControl->GetPedWalkLeftRight().GetNorm();
			fLeftStickX = ConvertRadianToSignal(fLeftStickX);
			static dev_float fLerpRate = 0.1f;
			m_fFPSStrafeDirection = rage::Lerp(fLerpRate, m_fFPSStrafeDirection, fLeftStickX);
		}
	}
#endif	//FPS_MODE_SUPPORTED
}

void CTaskMotionDiving::PitchCapsuleBound()
{
	CPed * pPed = GetPed();
	// B*2005653: FPS Swim/Dive: rotate capsule to always cover peds head to fix camera pop issues on collisions
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		CTaskMotionDiving::PitchCapsuleRelativeToPed(pPed);
	}
	else
	{
		pPed->SetDesiredBoundPitch(-HALF_PI);
	}
}

void CTaskMotionDiving::PitchCapsuleRelativeToPed(CPed *pPed)
{
	// Grab the bone positions.
	Vector3 vHead = Vector3::ZeroType;
	Vector3 vSpine1 = Vector3::ZeroType;
	pPed->GetBonePosition(vHead, BONETAG_HEAD);
	pPed->GetBonePosition(vSpine1, BONETAG_ROOT);

	// Generate a vector from the spine to the head.
	Vector3 vSpineToHead = vHead - vSpine1;
	Vector3 vSpineToHeadDirection = vSpineToHead;
	vSpineToHeadDirection.NormalizeSafe(VEC3_ZERO);

	// Grab the angle of rotation around the X axis.
	float fAngle = vSpineToHeadDirection.FastAngle(ZAXIS);

	// Add peds current pitch
	float fCurrentPitch = pPed->GetCurrentPitch();
	fAngle += fCurrentPitch;

	// Set the bound pitch.
	pPed->SetDesiredBoundPitch(-fAngle);

	// Increase capsule radius from 0.25 to 0.33 for extra protection
	//TUNE_GROUP_FLOAT(FPS_SWIMMING, fDiveCapsuleRadius, 0.33f, 0.0f, 5.0f, 0.01f);
	//pPed->SetTaskCapsuleRadius(fDiveCapsuleRadius);

#if __DEV
	TUNE_GROUP_BOOL(FPS_SWIMMING, bEnableDebugCapsuleRender, false);
	if (bEnableDebugCapsuleRender)
	{
		CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vHead), VECTOR3_TO_VEC3V(vSpine1), 0.15f, Color_red, false);
		CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(vSpine1), 0.1f, Color_blue, 16);
		CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(vHead), 0.1f, Color_yellow, 16);
		CTask::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vSpine1), VECTOR3_TO_VEC3V(vHead), Color_red, 16);
	}
#endif
}

CTask::FSM_Return	CTaskMotionDiving::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{

	if(iEvent == OnUpdate)
	{
		if(SyncToMoveState())
			return FSM_Continue;
	}

	FSM_Begin
		// Entry point
		FSM_State(State_DivingDown)
			FSM_OnEnter
				DivingDown_OnEnter();
			FSM_OnUpdate
				return DivingDown_OnUpdate();
	
		FSM_State(State_Idle)
			FSM_OnEnter
				Idle_OnEnter();
			FSM_OnUpdate
				return Idle_OnUpdate();
			FSM_OnExit
				Idle_OnExit();

		FSM_State(State_StartSwimming)
			FSM_OnEnter
				StartSwimming_OnEnter();
			FSM_OnUpdate
				return StartSwimming_OnUpdate();
			FSM_OnExit
				StartSwimming_OnExit();

		FSM_State(State_Swimming)
				FSM_OnEnter
				Swimming_OnEnter();
			FSM_OnUpdate
				return Swimming_OnUpdate();
			FSM_OnExit
				Swimming_OnExit();

		FSM_State(State_Glide)
				FSM_OnEnter
				Glide_OnEnter();
			FSM_OnUpdate
				return Glide_OnUpdate();

		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End


}

void CTaskMotionDiving::CleanUp()
{
	CPed * pPed = GetPed();
	pPed->SetUseExtractedZ(false);
	pPed->SetDesiredBoundPitch(0.0f);
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning, false );		
	pPed->m_Buoyancy.m_fForceMult = 1.0f;
	// Clear any old filter
	if(m_pWeaponFilter)
	{
		m_pWeaponFilter->Release();
		m_pWeaponFilter = NULL;
	}

	CTaskMotionSwimming::OnPedNoLongerDiving(*pPed, m_bHasEnabledLight);
	m_bHasEnabledLight = false;
}

#if !__FINAL

const char * CTaskMotionDiving::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_DivingDown&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_DivingDown",
		"State_Idle",
		"State_StartSwimming",
		"State_Swimming",
		"State_Glide",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

void CTaskMotionDiving::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	settings.m_CameraHash = g_InWaterCameraHash; 
}

//////////////////////////////////////////////////////////////////////////
// MoVE interface helper methods
//////////////////////////////////////////////////////////////////////////

void	CTaskMotionDiving::SendParams()
{
	
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		if (m_initialState == State_Idle)
		{
			m_MoveNetworkHelper.SetFlag(true, ms_AnimsLoadedToIdleId);
		}
		else 
		{
			m_MoveNetworkHelper.SetFlag(m_bAnimationsReady, ms_AnimsLoadedId);
		}

		CPed * pPed = GetPed();

		bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
		bFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif	//FPS_MODE_SUPPORTED

		float fDirectionVar = CalcDesiredDirection();

		TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingSwimHeadingMaxLerpRate, 3.00f,0.0f,20.0f,0.001f);
		TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingSwimHeadingMinLerpRate, 1.00f,0.0f,20.0f,0.001f);
		TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fScubaDivingSwimHeadingMaxLerpRate, 0.6f,0.0f,20.0f,0.001f);
		TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fScubaDivingSwimHeadingMinLerpRate, 0.6f,0.0f,20.0f,0.001f);
		bool bIsScuba = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsScuba);

		bool bSwimmingStraight = bFPSMode ? true : (fabs(fDirectionVar)< 0.2f);

		if (Sign(m_fPrevDirection) != Sign(fDirectionVar) && !bFPSMode) //direction flip
		{				
			m_fDivingSwimHeadingLerpRate = 0;
		}
		static dev_float sfSwimApproachApproachRate = 2.0f;
		Approach(m_fDivingSwimHeadingLerpRate, bSwimmingStraight ? (bIsScuba ? ms_fScubaDivingSwimHeadingMinLerpRate : ms_fDivingSwimHeadingMinLerpRate) : (bIsScuba ? ms_fScubaDivingSwimHeadingMaxLerpRate : ms_fDivingSwimHeadingMaxLerpRate), bSwimmingStraight ? 3.0f : sfSwimApproachApproachRate,  fwTimer::GetTimeStep());		
		//Displayf( "m_fDivingSwimHeadingLerpRate:%f fDirectionVar: %f", m_fDivingSwimHeadingLerpRate, fDirectionVar);

		// Use the desired direction in the MoVE network to trigger transitions, etc (more responsive than using the smoothed one)		
		if (GetState()!=State_Idle) //idle heading has it's own rules
		{
			TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingTurnIntensity, 2.00f,0.0f,20.0f,0.001f);
			float fPitchScale = bFPSMode ? 1.0f : 1.0f-fabs(pPed->GetCurrentPitch()/HALF_PI);
			fDirectionVar = Clamp(ms_fDivingTurnIntensity * fPitchScale * fDirectionVar, -PI,PI);			
			InterpValue( m_smoothedDirection , fDirectionVar, m_fDivingSwimHeadingLerpRate);		
			//Displayf("PitchScale: %f, DirVar: %f, Actual: %f", fPitchScale, fDirectionVar, m_smoothedDirection);
			m_MoveNetworkHelper.SetFloat( ms_HeadingId, ConvertRadianToSignal(m_smoothedDirection));
		}

		// Set the overridden rate param for swim
		// ...Use the scripted override if set.
		if (pPed->GetMotionData()->GetDesiredMoveRateInWaterOverride() != 1.0f || pPed->GetMotionData()->GetCurrentMoveRateInWaterOverride() != 1.0f)
		{
			// Speed override lerp (same as in CTaskHumanLocomotion::ProcessMovement)
			TUNE_GROUP_FLOAT(PED_MOVEMENT, fTendToDesiredMoveRateOverrideMultiplierDiving, 2.0, 1.0f, 20.0f, 0.5f);
			float fDeltaOverride = pPed->GetMotionData()->GetDesiredMoveRateInWaterOverride() - pPed->GetMotionData()->GetCurrentMoveRateInWaterOverride();
			fDeltaOverride *= (fTendToDesiredMoveRateOverrideMultiplierDiving * fwTimer::GetTimeStep());
			fDeltaOverride += pPed->GetMotionData()->GetCurrentMoveRateInWaterOverride();
			pPed->GetMotionData()->SetCurrentMoveRateInWaterOverride(fDeltaOverride);
			m_MoveNetworkHelper.SetFloat(ms_MoveRateOverrideId, fDeltaOverride);
		}
		else
		{
			const CMotionTaskDataSet* pMotionTaskDataSet = CMotionTaskDataManager::GetDataSet(pPed->GetPedModelInfo()->GetMotionTaskDataSetHash().GetHash());
			taskAssert(pMotionTaskDataSet);				
			if (pPed->GetPlayerInfo())
				m_MoveNetworkHelper.SetFloat(ms_MoveRateOverrideId, pMotionTaskDataSet->GetInWaterData()->m_MotionAnimRateScale * pPed->GetPlayerInfo()->m_fSwimSpeedMultiplier);
			else
				m_MoveNetworkHelper.SetFloat( ms_MoveRateOverrideId, pMotionTaskDataSet->GetInWaterData()->m_MotionAnimRateScale );	
		}

		// Smooth the direction parameter we'll be using to set blend weights in the network
		Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
		Vector2 vDesiredMBR = pPed->GetMotionData()->GetDesiredMoveBlendRatio();

		if (m_bDiveGaitReduced)
		{
			vDesiredMBR.y = MOVEBLENDRATIO_STILL;
			vCurrentMBR.y = MOVEBLENDRATIO_WALK;
		}

		float fSpeed = vCurrentMBR.y / MOVEBLENDRATIO_SPRINT; // normalize the speed values between 0.0f and 1.0f
		m_MoveNetworkHelper.SetFloat( ms_SpeedId, fSpeed);
		m_MoveNetworkHelper.SetFloat( ms_DesiredSpeedId, vDesiredMBR.y / MOVEBLENDRATIO_SPRINT);

		//Update pitch parameter
		float fPitchDelta = pPed->GetDesiredPitch() - pPed->GetCurrentPitch();	
		fPitchDelta = Clamp(fPitchDelta, -1.0f, 1.0f);
		TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fPitchApproachRate, 1.0f,0.0f,20.0f,0.001f);
		Approach(m_fPitchBlend, fPitchDelta, ms_fPitchApproachRate, fwTimer::GetTimeStep());	
		m_MoveNetworkHelper.SetFloat( ms_PitchPhaseId, (m_fPitchBlend+1.0f)/2.0f);
		//Displayf("Pitch blend: %f Positive?: %s, pitch Delta: %f", m_fPitchBlend, m_bPositivePitchChange ? "true" : "false", fPitchDelta);

#if FPS_MODE_SUPPORTED
		// Set strafing value in FPS mode
		if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && GetState() == State_Swimming)
		{
			m_MoveNetworkHelper.SetFloat(ms_StrafeDirectionId, m_fFPSStrafeDirection);
		}
#endif	//FPS_MODE_SUPPORTED
	}	
}

void CTaskMotionDiving::UpdateDiveGaitReduction(CPed* pPed, bool &bGaitReduced, float &fGaitReductionHeading, bool bFromSwimmingTask)
{
	if (bGaitReduced)
	{
		static float totalHeadingDelta = PI/8.0f;
		if (fabs(pPed->GetMotionData()->GetCurrentHeading() - fGaitReductionHeading) > totalHeadingDelta)
		{
			bGaitReduced = false;
			return;
		}

		WorldProbe::CShapeTestFixedResults<1> capsuleResults;		
		WorldProbe::CShapeTestCapsuleDesc capsuleTest;

		Vector3 vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());				
		Vector3 vEnd(vStart);
		Vector3 vTestDirection = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
		
		vEnd += vTestDirection * 1.35f;
		vStart += vTestDirection * 0.2f; //pull it back behind the ped a little for swept sphere
		static dev_float sfCapsuleRadius = 0.2f;
		// Test flags
		static const int iBoundFlags   = ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE; 		
		capsuleTest.SetCapsule(vStart, vEnd, sfCapsuleRadius);
		capsuleTest.SetResultsStructure(&capsuleResults);
		capsuleTest.SetIsDirected(true);
		capsuleTest.SetIncludeFlags(iBoundFlags);		
		capsuleTest.SetExcludeEntity(pPed);
		// Do initial sphere check when swimming (useful for when swimming against a vertical obstacle, ie edge of a swimming pool)
		if (bFromSwimmingTask)
		{
			capsuleTest.SetDoInitialSphereCheck(true);
		}
#if __DEV
		CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), 0.2f, Color_green, 1, 0, false);	
#endif
		if (!WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest))
		{	
			bGaitReduced = false;
		}
	} 
	else
	{	
		Vector3 riverVel = pPed->m_Buoyancy.GetLastRiverVelocity();
		float speed = (pPed->GetVelocity()-riverVel).Mag();
		float clipSpeed = pPed->GetPreviousAnimatedVelocity().Mag();
		float speedRatio = (clipSpeed !=0) ? speed/clipSpeed : 1.0f;
		//Displayf("SPEED ratio: %f", speedRatio);
		static dev_float sfMinRunRatio = 0.3f;
		Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();			

		TUNE_GROUP_FLOAT(FPS_SWIMMING, fSwimMBRThreshold, 1.5f, 0.0f, 3.0f, 0.01f);
		float fMBRThreshold = bFromSwimmingTask ? fSwimMBRThreshold: MOVEBLENDRATIO_RUN;

		if (vCurrentMBR.y >= fMBRThreshold && speedRatio< sfMinRunRatio) 
		{
			bGaitReduced = true;
			fGaitReductionHeading = pPed->GetMotionData()->GetCurrentHeading();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool	CTaskMotionDiving::SyncToMoveState()
{

	if(m_bAnimationsReady && m_MoveNetworkHelper.IsNetworkActive())
	{
		s32 iState = -1;
		if(m_MoveNetworkHelper.GetBoolean( ms_OnEnterIdleId))
			iState = State_Idle;
		else if(m_MoveNetworkHelper.GetBoolean( ms_OnEnterStartSwimmingId))
			iState = State_StartSwimming;
		else if(m_MoveNetworkHelper.GetBoolean( ms_OnEnterSwimmingId))
			iState = State_Swimming;
		else if(m_MoveNetworkHelper.GetBoolean( ms_OnEnterGlideId))
			iState = State_Glide;

		SendParams();

		if(iState != -1 && iState != GetState())
		{
			SetState(iState);
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
//Task motion base methods

void CTaskMotionDiving::GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)
{
	fwMvClipSetId clipSetId = GetDivingClipSetId();
	taskAssert(clipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	fwClipSet *clipSet = fwClipSetManager::GetClipSet(clipSetId);
	taskAssert(clipSet);

	static const fwMvClipId s_walkClip("dive_walk",0x85B00C52);
	static const fwMvClipId s_runClip("dive_run",0xD37B22B6);
	static const fwMvClipId s_sprintClip("dive_sprint",0x7386E13A);

	const fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };

	RetrieveMoveSpeeds(*clipSet, speeds, clipNames);

	return;
}

Vec3V_Out CTaskMotionDiving::CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrixIn, float fTimestep)
{
	const Matrix34& updatedPedMatrix = RCC_MATRIX34(updatedPedMatrixIn);

	CPed* pPed = GetPed();

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnCalcDesiredVelocityOfFocusEntity(), pPed );

	Vector3	velocity(VEC3_ZERO);
	Vector3 extractedVel(pPed->GetAnimatedVelocity());

	if (m_bForceScubaAnims && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsScuba))
	{
		//Scale velocity to normal swim speed
		//maxSwimSpeed(5.499)/maxScubaSpeed(7.499) = 0.73
		//minSwimSpeed(3.854)/minScubaSpeed(5.016) = 0.77
		TUNE_GROUP_FLOAT(SCUBA_SPEED_DIVING, SCUBA_MODIFIER, 0.75f, 0.1f, 1.0f, 0.01f);
		extractedVel *= SCUBA_MODIFIER;
	}

	velocity += extractedVel.y * updatedPedMatrix.b;
	velocity += extractedVel.x * updatedPedMatrix.a;
	if(pPed->GetUseExtractedZ())
	{
		velocity += extractedVel.z * updatedPedMatrix.c;
	}	

	// Add in swimming resistance
	Vector3 vSwimmingResistanceVelChange = ProcessSwimmingResistance(fTimestep);
	velocity += vSwimmingResistanceVelChange;

	// Control surfacing at a manageable speed
	static bool bControlSurfacing = true;
	if(bControlSurfacing)
	{
		if(GetState()!=State_DivingDown)
		{
			if(m_fHeightBelowWater < MBIW_SWIM_UWATER_SURFACING_DAMPING_DIST && velocity.z > 0.0f)
			{
				const float fScale = Max(m_fHeightBelowWater / MBIW_SWIM_UWATER_SURFACING_DAMPING_DIST, MBIW_SWIM_UWATER_SURFACING_MIN_VEL);
				velocity.z *= fScale;
			}
		}
	}
	
	// Clamp the velocity change to avoid large changes
	Vector3 vDesiredVelChange = velocity - pPed->GetVelocity();
	vDesiredVelChange+=pPed->m_Buoyancy.GetLastRiverVelocity();


	//Add forward drift
	vDesiredVelChange += VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()) * m_fForwardDriftScale;	

    // don't clamp the velocity change for network clones, the network blending code prevents
    // large velocity changes in a frame but needs to know the target velocity for the ped
    if(!pPed->IsNetworkClone())
    {
	    ClampVelocityChange(vDesiredVelChange,fTimestep);
    }

	velocity = pPed->GetVelocity() + vDesiredVelChange;

	//Scale speed based on pitch B* 961320
	static dev_float sf_PitchVelocityScale = 0.8f;
	float fPitchScale = Lerp( fabs(pPed->GetCurrentPitch()/HALF_PI), 1.0f, sf_PitchVelocityScale);
	//Displayf("PitchScale: %f", fPitchScale);
	velocity *= fPitchScale;

#if FPS_MODE_SUPPORTED
	// FPS Swim: Store velocity so we can transition to strafing smoothly
	// (Don't need to constantly check for input here like in CTaskMotionAiming as we will always have a velocity while in the Diving task)
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
	{
		CTaskMotionPed* pTaskMotionPed = static_cast<CTaskMotionPed*>(GetParent());
		if (pTaskMotionPed)
		{
			pTaskMotionPed->SetCachedSwimVelocity(velocity);
		}
	}
#endif	//FPS_MODE_SUPPORTED

    NetworkInterface::OnDesiredVelocityCalculated(*pPed);

	return VECTOR3_TO_VEC3V(velocity);
}

void CTaskMotionDiving::ClampVelocityChange(Vector3& vChangeInAndOut, const float timeStep)
{
	// MEGA-HACK: B*2148207: Something funky going on with the velocity calculation code; velocity was being scaled back hugely on PC with high framerates.
	// Not entirely sure what the actual problem is here, bit scary to be changing how we calculate the velocities since we shipped with this on current and next gen.
	// ("ProcessSwimmingResistance" seems to return different values when called multiple times in the same frame - this should probably remain consistent).
	// Locking the timestep to 1/60 here seems to fix it, keeping PC consistent with PS4/Xbone.
	const float fMinTimeStep = 1.0f / 60.0f;
	float fTimeStep = Clamp(timeStep, fMinTimeStep, 1.0f);

	// Limit the rate of change so we don't apply a crazy force to the ped.
	if(vChangeInAndOut.Mag2() > MBIW_FRICTION_VEL_CHANGE_LIMIT * MBIW_FRICTION_VEL_CHANGE_LIMIT * fTimeStep * fTimeStep)
	{
		vChangeInAndOut *= MBIW_FRICTION_VEL_CHANGE_LIMIT * fTimeStep / vChangeInAndOut.Mag();
	}
}

Vector3 CTaskMotionDiving::ProcessSwimmingResistance(float fTimestep)
{

	CPed * pPed = GetPed();

	Vector3 desiredVel(VEC3_ZERO);
	Vector3 vExtractedVel;

	if(GetState()==State_Glide)
	{
		const bool bWalkInInterior =
			pPed->GetPortalTracker()->IsInsideInterior() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting) &&
			!pPed->GetPortalTracker()->IsAllowedToRunInInterior();

		if(bWalkInInterior)
		{
			static dev_float fInteriorGlideScale = 0.1f;	// 0.0f
			vExtractedVel = Vector3(0.0f, m_fGlideForwardsVel*fInteriorGlideScale, 0.0f);
		}
		else
		{
			vExtractedVel = Vector3(0.0f, m_fGlideForwardsVel, 0.0f);
		}
	}
	else
	{
		vExtractedVel = pPed->GetAnimatedVelocity();

		static dev_float sfUnderwaterInteriorSwimVelScale = 0.0125f;
		static dev_float sfUnderwaterSwimVelScale = 0.25f;

		if(pPed->GetPortalTracker()->IsInsideInterior() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting) &&
			!pPed->GetPortalTracker()->IsAllowedToRunInInterior())
		{
			vExtractedVel *= sfUnderwaterInteriorSwimVelScale;
		}
		else
		{
			vExtractedVel *= sfUnderwaterSwimVelScale;
		}
	}

	if( GetState() == State_DivingDown )
	{
		if(vExtractedVel.z > 0.0f)
			vExtractedVel.z = -vExtractedVel.z;
	}

	const Matrix34  mPedMat = MAT34V_TO_MATRIX34(pPed->GetMatrix());
	desiredVel += mPedMat.a * vExtractedVel.x;
	desiredVel += mPedMat.b * vExtractedVel.y;

	Vector3 desiredVelChange = desiredVel - pPed->GetVelocity();

	// Clamp the velocity change before we apply the water surface correction	
	if(desiredVelChange.Mag2() > MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT * MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT * fTimestep * fTimestep)
	{
		desiredVelChange *=  MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT * fTimestep / desiredVelChange.Mag();
	}

	Vector3 newPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + ((pPed->GetVelocity() + desiredVelChange) * fTimestep);

	float fWaterLevel = 0.0f;
	if(pPed->m_Buoyancy.GetWaterLevelIncludingRivers(newPos, &fWaterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pPed, pPed->GetCanUseHighDetailWaterLevel()))
	{
		m_fHeightBelowWater = rage::Max(MBIW_SWIM_CLIP_HEIGHT_BELOW_WATER, fWaterLevel - newPos.z);

		// Check a little bit further than we will swim under boats to avoid instabilities
		CVehicle* vehicle = GetVehAbovePed(pPed, MBIW_SWIM_UNDER_BOAT_HEIGHT_BELOW_WATER + 0.75f);
		if(vehicle && (vehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)/* && (vehicle->GetC().z < -0.33f)*/)
		{
			m_fHeightBelowWater += MBIW_SWIM_UNDER_BOAT_HEIGHT_BELOW_WATER;	// Drop the player way below the water's surface so they can escape the boat
		}

		// Clamp this vel change. Note this is done separately to the MBIW_SWIM_CLIP_VEL_CHANGE_LIMIT so that wave intensity wont affect the XY swim speed
		desiredVelChange.z = Clamp(desiredVelChange.z, -MBIW_SWIM_BUOYANCY_DECEL_LIMIT, MBIW_SWIM_BUOYANCY_ACCEL_LIMIT);

	}	// End in-water check
	else
	{
		m_fHeightBelowWater = 0.0f;
		return VEC3_ZERO;
	}

	pPed->m_Buoyancy.SetShouldStickToWaterSurface(false);

	pPed->SetUseExtractedZ( (GetState()==State_DivingDown  && !m_bDivingDownBottomedOut));

	// In case pPed is in rag doll (shouldn't really happen)
	if(!pPed->GetUsingRagdoll() && pPed->GetCurrentPhysicsInst()->IsInLevel())
	{ 
		// If diving in rapids force to the surface!
		static dev_float sf_PlayerForceMult = 1.0f; 
		static dev_float sf_AIForceMult = .7625f; 
		if(CTaskMotionSwimming::CheckForRapids(pPed))
		{
			static dev_float sfUnderwaterRapidsBuoyancyMult = 3.0f;
			pPed->m_Buoyancy.m_fForceMult = sfUnderwaterRapidsBuoyancyMult;
		}
		else
		{
			pPed->m_Buoyancy.m_fForceMult = pPed->IsPlayer() ? sf_PlayerForceMult : sf_AIForceMult;
		}

		// Apply new speed to character by adding force to centre of gravity
		return desiredVelChange;
	}

	return VEC3_ZERO;
}

CVehicle *CTaskMotionDiving::GetVehAbovePed(CPed *ped, float testDistance)
{
	const int numIntersections = 2;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<numIntersections> probeResults;
	probeDesc.SetResultsStructure(&probeResults);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
	probeDesc.SetStartAndEnd(vPedPosition, vPedPosition+Vector3(0.0f,0.0f, testDistance));
	probeDesc.SetExcludeEntity(ped);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

	// See if we found any vehicles
	//for(int i = 0; i < numResults; i++)
	WorldProbe::ResultIterator it;
	for(it = probeResults.begin(); it < probeResults.last_result(); ++it)
	{
		CEntity* entity = static_cast<CEntity*>(it->GetHitInst()->GetUserData());
		if(entity && entity->GetIsTypeVehicle())
		{
			return static_cast<CVehicle*>(entity);
		}
	}

	return NULL;	
}

CTask * CTaskMotionDiving::CreatePlayerControlTask()
{
	CTask *playerTask = rage_new CTaskPlayerOnFoot();
	return playerTask;
}

bool CTaskMotionDiving::IsInMotion(const CPed * UNUSED_PARAM(pPed)) const
{
	bool isMoving = (GetMotionData().GetCurrentMbrX() != 0.0f) || (GetMotionData().GetCurrentMbrY() != 0.0f);
	return isMoving;
}

fwMvClipSetId CTaskMotionDiving::GetDivingClipSetId()
{
	TUNE_GROUP_BOOL(FPS_SWIMMING, bForceDivingScubaAnims, false);
	if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsScuba) || m_bForceScubaAnims || bForceDivingScubaAnims)
	{
#if FPS_MODE_SUPPORTED
		// Use FPS scuba clipset if in FPS mode
		if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			return CLIP_SET_FPS_DIVING_SCUBA;
		}
#endif	//FPS_MODE_SUPPORTED
		static fwMvClipSetId s_defaultDivingSet("move_ped_diving_scuba",0x7AE26B56);
		return s_defaultDivingSet;
	}

#if FPS_MODE_SUPPORTED
	// Use FPS diving clipset if in FPS mode
	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return CLIP_SET_FPS_DIVING;
	}
#endif	//FPS_MODE_SUPPORTED

	fwMvClipSetId swimmingClipSetId;
	CTaskMotionBase* pedTask = GetPed()->GetPrimaryMotionTask();
	if (pedTask)
	{
		swimmingClipSetId = pedTask->GetDefaultDivingClipSet();
	}

	return swimmingClipSetId;
}


void CTaskMotionDiving::ResetDivingClipSet()
{
	fwMvClipSetId clipSetId = GetDivingClipSetId();
	aiAssert(GetParent() && GetParent()->GetTaskType()==CTaskTypes::TASK_MOTION_PED);

	fwClipSet *clipSet = fwClipSetManager::GetClipSet(clipSetId);
	taskAssert(clipSet);

	if (clipSet->Request_DEPRECATED() && m_MoveNetworkHelper.IsNetworkActive())
	{
		//Set new clip set
		m_MoveNetworkHelper.SetClipSet(clipSetId);	

		//Reset the current state
		m_MoveNetworkHelper.SendRequest(ms_RestartState);	
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionDiving::StartAlternateClip(AlternateClipType type, sAlternateClip& data, bool bLooping)
{
	if (data.IsValid())
	{
		m_alternateClips[type] = data;
		SetAlternateClipLooping(type, bLooping);

		if (m_MoveNetworkHelper.IsNetworkActive())
		{
			if (type == ACT_Run)
			{
				SendAlternateParams();
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////

void CTaskMotionDiving::EndAlternateClip(AlternateClipType type, float fBlendDuration)
{
	StopAlternateClip(type, fBlendDuration);

	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		if (type == ACT_Run)
		{
			SendAlternateParams();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionDiving::SendAlternateParams()
{
	if (m_alternateClips[ACT_Run].IsValid())
	{
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_alternateClips[ACT_Run].iDictionaryIndex, m_alternateClips[ACT_Run].uClipHash.GetHash());		
		m_MoveNetworkHelper.SetClip(pClip, ms_AlternateRunClipId);
		//m_MoveNetworkHelper.SetBoolean(ms_LoopedId, m_alternateClips[ACT_Run].bLooped == 1);
		//m_MoveNetworkHelper.SetFloat(ms_DurationId, m_alternateClips[ACT_Run].fBlendDuration);
		m_MoveNetworkHelper.SetFlag(true, ms_UseAlternateRunFlagId);
	}
	else
	{
		m_MoveNetworkHelper.SetClip(NULL, ms_AlternateRunClipId);
		m_MoveNetworkHelper.SetFlag(false, ms_UseAlternateRunFlagId);
	}

}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionDiving::HolsterWeapon(bool bHolster)
{
	CPed *pPed = GetPed();
	if (pPed)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, bHolster);
		if (bHolster)
		{
			pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionDiving::SetWeaponHoldingClip(bool bIsGun)
{
	taskAssert(m_MoveNetworkHelper.IsNetworkActive());
	CPed *pPed = GetPed();
	if (pPed)
	{
	//Only set the weapon holding clip if we have a visible weapon
	CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
	if (pPed->GetEquippedWeaponInfo() && !pPed->GetEquippedWeaponInfo()->GetIsUnarmed() && pWeapon && pWeapon->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY) && m_WeaponClipSetId != CLIP_SET_ID_INVALID)
	{
		//Harpoon gun check
		if (bIsGun)
		{
			static const fwMvClipId idleClipID("IDLE",0x71c21326);
			static const fwMvClipId glideClipID("GLIDE",0xf9209d38);
			static const fwMvClipId runClipID("RUN",0x1109b569);
			static const fwMvClipId sprintClipID("SPRINT",0x0bc29e48);
			
			const crClip* pClip = NULL;
			pClip = fwClipSetManager::GetClip(m_WeaponClipSetId, idleClipID);	
			if (pClip)
			{
				m_MoveNetworkHelper.SetClip(pClip, ms_IdleWeaponHoldingClipId);
			}
			pClip = fwClipSetManager::GetClip(m_WeaponClipSetId, glideClipID);	
			if (pClip)
			{
				m_MoveNetworkHelper.SetClip(pClip, ms_GlideWeaponHoldingClipId);
			}
			pClip = fwClipSetManager::GetClip(m_WeaponClipSetId, runClipID);	
			if (pClip)
			{
				m_MoveNetworkHelper.SetClip(pClip, ms_SwimRunWeaponHoldingClipId);
			}
			pClip = fwClipSetManager::GetClip(m_WeaponClipSetId, sprintClipID);	
			if (pClip)
			{
				m_MoveNetworkHelper.SetClip(pClip, ms_SwimSprintWeaponHoldingClipId);
			}
		}
		else
		{
			//Else use the idle clip (as previously set in the MoVE network)
			const crClip* pClip = NULL;
			if (pPed->GetEquippedWeaponInfo()->GetIsMeleeFist())
			{
				static const fwMvClipId meleeFistIdleClipID("GRIP_IDLE",0x3ec63b58);
				pClip = fwClipSetManager::GetClip(m_WeaponClipSetId, meleeFistIdleClipID);
			}
			else
			{
				static const fwMvClipId idleClipID("IDLE",0x71c21326);
				pClip = fwClipSetManager::GetClip(m_WeaponClipSetId, idleClipID);
			}
			
			if (pClip)
			{
				m_MoveNetworkHelper.SetClip(pClip, ms_IdleWeaponHoldingClipId);
				m_MoveNetworkHelper.SetClip(pClip, ms_GlideWeaponHoldingClipId);
				m_MoveNetworkHelper.SetClip(pClip, ms_SwimRunWeaponHoldingClipId);
				m_MoveNetworkHelper.SetClip(pClip, ms_SwimSprintWeaponHoldingClipId);
			}
		}
	}
	}
}

bool  CTaskMotionDiving::StartMoveNetwork()
{
	CPed * pPed = GetPed();

	if (!m_MoveNetworkHelper.IsNetworkAttached() || !m_bAnimationsReady)
	{

		if (!m_MoveNetworkHelper.IsNetworkActive())
		{		
			m_MoveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkMotionDiving);

			if (m_MoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkMotionDiving))
			{		
				m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkMotionDiving);

				//Setup base animset 
				static const fwMvClipSetVarId baseAnimSet("Base",0x44E21C90);	
				static fwMvClipSetId s_swimmingBaseClipSetId("move_ped_swimming_base",0xB2143FC);
				m_MoveNetworkHelper.SetClipSet(s_swimmingBaseClipSetId, baseAnimSet);
			}
		}

		//Initialize m_bForceScubaAnims on starting network
		const CWeaponInfo *pWeaponInfo = pPed->GetEquippedWeaponInfo();
		m_bForceScubaAnims = pWeaponInfo && pWeaponInfo->GetIsGun();

		fwMvClipSetId clipSetId = GetDivingClipSetId();
		aiAssert(GetParent() && GetParent()->GetTaskType()==CTaskTypes::TASK_MOTION_PED);

		fwClipSet *clipSet = fwClipSetManager::GetClipSet(clipSetId);
		taskAssert(clipSet);

		if (clipSet->Request_DEPRECATED() && m_MoveNetworkHelper.IsNetworkActive())
		{
			//Setup diving set
			m_MoveNetworkHelper.SetClipSet( clipSetId);		
			m_bAnimationsReady = true;

			if (m_initialState > State_DivingDown)
			{
				static const fwMvStateId s_moveStateSignals[State_Exit] = 
				{		
					fwMvStateId(),
					fwMvStateId("idle",0x71C21326),
					fwMvStateId("startswimming",0xAFA9935E),
					fwMvStateId("swimming",0x5DA2A411),
					fwMvStateId("glide",0xF9209D38)
				};

				if (s_moveStateSignals[m_initialState].GetHash()!=0)
				{
					m_MoveNetworkHelper.ForceStateChange( s_moveStateSignals[m_initialState] );
				}
			}
		}

		if (!m_MoveNetworkHelper.IsNetworkAttached())
			m_MoveNetworkHelper.DeferredInsert();
	}
	else
	{
		SetState(m_initialState);
		return true;
	}

	return false;

}

//////////////////////////////////////////////////////////////////////////
// State implementations

//////////////////////////////////////////////////////////////////////////
void					CTaskMotionDiving::DivingDown_OnEnter()
{
	float fWaveDelta = 0;
	Vector3 vPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	Water::GetWaterLevel(vPos, &fWaveDelta, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
	float fNoWaves = fWaveDelta;
	Water::GetWaterLevelNoWaves(vPos, &fNoWaves, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
	fWaveDelta -= fNoWaves;
	static dev_float sfWaveDeltaScale = 0.5f;
	fWaveDelta = sfWaveDeltaScale * Max(fWaveDelta, 0.0f);
	//Displayf("Wave height = %f", fWaveDelta);
	m_fDivingDownCorrectionGoal = fWaveDelta;
	m_fDivingDownCorrectionCurrent = m_fDivingDownCorrectionGoal;
	m_bDivingDownBottomedOut = false;

	//Holster weapon when diving down (unless our initial state is set to idle, as we are coming directly from aiming so don't holster the gun)
	if (m_initialState != State_Idle)
	{
		HolsterWeapon();
	}

	if(CTaskMotionSwimming::IsScubaGearVariationActiveForPed(*GetPed()))
	{
		m_bHasEnabledLight = true;
	}
	CTaskMotionSwimming::OnPedDiving(*GetPed());
}

CTask::FSM_Return		CTaskMotionDiving::DivingDown_OnUpdate()
{
	StartMoveNetwork();
	CPed* pPed = GetPed();

	if ( m_fGroundClearance > 0.0f && m_fGroundClearance < 1.0f )
	{
		m_bDivingDownBottomedOut = true;
	}

	bool cancelAnim = false;
	if (!pPed->GetIsInWater())
	{
		cancelAnim = true;
	}

	bool bChangingHeading = false;
	if (pPed->IsLocalPlayer())
	{
		CControl * pControl = pPed->GetControlFromPlayer();
		float stickOffsetX = pControl->GetPedWalkLeftRight().GetNorm();
		if (fabs(stickOffsetX) > 0.25f)
		{
			bChangingHeading = true;
		}
	}
	else if ( fabs(CalcDesiredDirection()) > 0.01f)
	{
		bChangingHeading = true;
	}	

	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fMinDivingDowninterruptPhase, 0.3f,0.0f,1.0f,0.01f);
	if (pPed->GetMotionData()->GetDesiredMbrY()>0.0f || bChangingHeading)
	{
		float fDiveUnderPhase = GetParent()->GetMoveNetworkHelper()->GetFloat(CTaskMotionPed::ms_DiveUnderPhaseId);		
		if (fDiveUnderPhase > ms_fMinDivingDowninterruptPhase)
			cancelAnim = true;		
	}
	if (cancelAnim)
	{
		taskAssert(GetParent()->GetTaskType() == CTaskTypes::TASK_MOTION_PED);
		GetParent()->GetMoveNetworkHelper()->SetBoolean(CTaskMotionPed::ms_TransitionClipFinishedId, true);
	}

	if (CTaskMotionSwimming::CheckForWaterfall(pPed))
		return FSM_Continue;	

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////
// Idle behaviour
void						CTaskMotionDiving::Idle_OnEnter()
{
	m_fIdleHeadingApproachRate = 3.25f;
	m_fHeadingApproachRate = 0;
	m_fPrevDirection = 0;

	if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired))
	{
		HolsterWeapon(false);
	}

		//Set up weapon holding clips
	if (GetPed()->GetEquippedWeaponInfo() && GetPed()->GetEquippedWeaponInfo()->GetIsGun())
	{
		SetWeaponHoldingClip(true);
	}
	else
	{
		SetWeaponHoldingClip(false);
	}

}

CTask::FSM_Return					CTaskMotionDiving::Idle_OnUpdate()
{
	CPed* pPed = GetPed();

	const CWeaponInfo *pWeaponInfo = pPed->GetEquippedWeaponInfo();
	m_bForceScubaAnims = pWeaponInfo && pWeaponInfo->GetIsGun();
	// If m_bForceScubaAnims is out of sync with bForceScubaAnims then reset anims
	if (m_bForceScubaAnims != m_bResetAnims)
	{
		m_bResetAnims = m_bForceScubaAnims;
		ResetDivingClipSet();
	}

	if (pWeaponInfo && pWeaponInfo->GetIsGun())
	{
		SetWeaponHoldingClip(true);
	}
	else
	{
		SetWeaponHoldingClip(false);
	}

	float fDirectionVar=0;
	static dev_float sfIdleApproachApproachRate = 2.0f;
	float fHeadingDelta = 0;
	if (pPed->IsLocalPlayer())
	{
		if (CTaskMotionSwimming::CheckForWaterfall(pPed))
			return FSM_Continue;	

		CControl * pControl = pPed->GetControlFromPlayer();
		taskAssert(pControl!=NULL);
		static dev_s32 iControlResetTimer = 250;

		// This used to use GetAxisValue() that return in range -127...+127 but looking at the code I think they meant to use GetAxisValuef()
		// which uses range -1...+1 so I have converted back to old range.
		TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingIdleHeadingLerpRate, 1.400f,0.0f,20.0f,0.001f);	
		
		float stickOffsetX = pControl->GetPedWalkLeftRight().GetNorm();
		bool bInFPSMode = false;
#if FPS_MODE_SUPPORTED
		bInFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif
		// Allow heading update in FPS mode regardless of left stick input (as we want to rotate based on camera heading).
		if ((fabs(stickOffsetX) > 0.25f) || bInFPSMode)
		{					
			float fBoundPitch = GetPed()->GetBoundPitch(); 
			float fPitchDelta = GetPed()->GetCurrentPitch(); 
			float fCurrentPitch = fPitchDelta;

			TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingIdlePitchLerpRate, 1.000f,0.0f,20.0f,0.001f);
			
			float fBoundPitchAbs = abs(fBoundPitch); 
			float fPitchDeltaAbs = abs(fPitchDelta); 
			

			if (fBoundPitchAbs>SMALL_FLOAT && fPitchDeltaAbs > SMALL_FLOAT)
			{
				// B*1946245: Don't instantly set pitch lerp to 1 as we get a minor pop. Lerp it up to ms_fDivingIdlePitchLerpRate. 
				TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingIdlePitchLerpRateLerpRate, 0.250f,0.0f,20.0f,0.001f);
				InterpValue(m_fDivingIdlePitchLerpRateLerpRate, ms_fDivingIdlePitchLerpRate, ms_fDivingIdlePitchLerpRateLerpRate, true);

				InterpValue(fPitchDelta, 0.0f, m_fDivingIdlePitchLerpRateLerpRate, true);
				InterpValue(fBoundPitch, -HALF_PI, m_fDivingIdlePitchLerpRateLerpRate, true);
				
			}
			else
			{
				fBoundPitch = -HALF_PI; 
				fPitchDelta = 0.0f; 
				m_bCanRotateCamRel = true; 
			}
			pPed->SetDesiredBoundPitch(fBoundPitch);
			pPed->SetDesiredPitch(fPitchDelta);
			
			if(!pPed->GetUsingRagdoll())
			{
				GetMotionData().SetExtraPitchChangeThisFrame(- (fCurrentPitch - fPitchDelta));
			}				

			//there's no turn in these clips, so we'll need to turn the ped directly here in the task 
			fHeadingDelta = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());
			
			if (Sign(m_fPrevDirection) != Sign(fHeadingDelta)) //direction flip
			{					
				m_fPrevDirection = fHeadingDelta;
				m_fHeadingApproachRate = 0;
			}

			//Idle heading processing
			InterpValue(m_fHeadingApproachRate, ms_fDivingIdleHeadingLerpRate, sfIdleApproachApproachRate);			
			InterpValue(m_fIdleHeadingApproachRate, 1.0f, 1.0f);			
			//Displayf ("m_fIdleHeadingApproachRate: %f", m_fIdleHeadingApproachRate);
		}
		else
		{
			InterpValue(m_fHeadingApproachRate, 0.0f, sfIdleApproachApproachRate);	
			m_fDivingIdlePitchLerpRateLerpRate = 0.0f;
		}
		TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingIdleHeadingMaxLerpRate, 2.5f,0.0f,20.0f,0.001f);				
		if (pControl->InputHowLongAgo() > iControlResetTimer) {
			m_fIdleHeadingApproachRate = ms_fDivingIdleHeadingMaxLerpRate; //reset				
		}
	}
	else
	{
		m_fHeadingApproachRate = sfIdleApproachApproachRate;	
		fHeadingDelta = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());
	}
	UpdateRiverReorientation(pPed);
	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(pPed->GetMotionData()->GetExtraHeadingChangeThisFrame() + fHeadingDelta*fwTimer::GetTimeStep()*m_fHeadingApproachRate);
	fDirectionVar = CalcDesiredDirection();			

	InterpValue( m_smoothedDirection , fDirectionVar, m_fIdleHeadingApproachRate);				
	m_MoveNetworkHelper.SetFloat( ms_HeadingId, ConvertRadianToSignal(m_smoothedDirection));
	return FSM_Continue;
}
void						CTaskMotionDiving::Idle_OnExit()
{
	m_bCanRotateCamRel = false; 
}

//////////////////////////////////////////////////////////////////////////
// swim start behaviour

void						CTaskMotionDiving::StartSwimming_OnEnter()
{
	if (GetPed()->GetEquippedWeaponInfo() && GetPed()->GetEquippedWeaponInfo()->GetIsGun())
	{
		SetWeaponHoldingClip(true);
	}
	else
	{
		SetWeaponHoldingClip(false);
	}
}
CTask::FSM_Return					CTaskMotionDiving::StartSwimming_OnUpdate()
{
	const CWeaponInfo *pWeaponInfo = GetPed()->GetEquippedWeaponInfo();
	m_bForceScubaAnims = pWeaponInfo && pWeaponInfo->GetIsGun();
	// If m_bForceScubaAnims is out of sync with bForceScubaAnims then reset anims
	if (m_bForceScubaAnims != m_bResetAnims)
	{
		m_bResetAnims = m_bForceScubaAnims;
		ResetDivingClipSet();
	}
	if (GetPed()->GetEquippedWeaponInfo() && GetPed()->GetEquippedWeaponInfo()->GetIsGun())
	{
		SetWeaponHoldingClip(true);
	}
	else
	{
		SetWeaponHoldingClip(false);
	}
	return FSM_Continue;
}
void						CTaskMotionDiving::StartSwimming_OnExit()
{

}

//////////////////////////////////////////////////////////////////////////
// swimming behaviour

void						CTaskMotionDiving::Swimming_OnEnter()
{
	m_fPrevDirection = 0;

	//Set up weapon holding clips
	if (GetPed()->GetEquippedWeaponInfo() && GetPed()->GetEquippedWeaponInfo()->GetIsGun())
	{
		SetWeaponHoldingClip(true);
	}
	else
	{
		SetWeaponHoldingClip(false);
	}

#if FPS_MODE_SUPPORTED
	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		m_MoveNetworkHelper.SetBoolean(ms_IsFPSMode, true);
	}
#endif	//FPS_MODE_SUPPORTED
}
CTask::FSM_Return					CTaskMotionDiving::Swimming_OnUpdate()
{
	CPed* pPed = GetPed();
	if (CTaskMotionSwimming::CheckForWaterfall(pPed))
		return FSM_Continue;	

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		bFPSMode = true;
		m_MoveNetworkHelper.SetBoolean(ms_IsFPSMode, true);
	}
#endif	//FPS_MODE_SUPPORTED

	const CWeaponInfo *pWeaponInfo = pPed->GetEquippedWeaponInfo();
	m_bForceScubaAnims = pWeaponInfo && pWeaponInfo->GetIsGun();
	// If m_bForceScubaAnims is out of sync with bForceScubaAnims then reset anims
	if (m_bForceScubaAnims != m_bResetAnims)
	{
		m_bResetAnims = m_bForceScubaAnims;
		ResetDivingClipSet();
	}

	if (pWeaponInfo && pWeaponInfo->GetIsGun())
	{
		SetWeaponHoldingClip(true);
	}
	else
	{
		SetWeaponHoldingClip(false);
	}

	bool bUsingAiMotion = (!GetPed()->GetMotionData()->GetPlayerHasControlOfPedThisFrame() && !GetPed()->IsNetworkPlayer());

	//Adding some turn to the ped directly for responsiveness, B* 852656
	const float fHeadingDelta = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());
	if (!bUsingAiMotion && Sign(m_fPrevDirection) != Sign(fHeadingDelta)) //direction flip
	{					
		m_fPrevDirection = fHeadingDelta;
		if (!bFPSMode)
		{
			m_fHeadingApproachRate = 0;
		}
	}

	// Regular settings
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingSwimHeadingLerpRate, 0.75f,0.0f,20.0f,0.001f);	
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fScubaDivingSwimHeadingLerpRate, 0.8f,0.0f,20.0f,0.001f);	
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fSwimApproachApproachRate, 2.0f,0.0f,20.0f,0.001f);	
	// Tighter settings
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingSwimHeadingLerpRateTighterTurn, 10.0f,0.0f,20.0f,0.001f);	
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fScubaDivingSwimHeadingLerpRateTighterTurn, 10.0f,0.0f,20.0f,0.001f);	
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fSwimApproachApproachRateTighterTurn, 5.0f,0.0f,20.0f,0.001f);	

	float fDivingSwimHeadingLerpRate, fScubaDivingSwimHeadingLerpRate, fSwimApproachApproachRate;

	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings) || pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettingsForScript))
	{
		fDivingSwimHeadingLerpRate = ms_fDivingSwimHeadingLerpRateTighterTurn;
		fScubaDivingSwimHeadingLerpRate = ms_fScubaDivingSwimHeadingLerpRateTighterTurn;
		fSwimApproachApproachRate = ms_fSwimApproachApproachRateTighterTurn;
	}
	else
	{
		fDivingSwimHeadingLerpRate = ms_fDivingSwimHeadingLerpRate;
		fScubaDivingSwimHeadingLerpRate = ms_fScubaDivingSwimHeadingLerpRate;
		fSwimApproachApproachRate = ms_fSwimApproachApproachRate;
	}

	
	InterpValue(m_fHeadingApproachRate, pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsScuba) ? fScubaDivingSwimHeadingLerpRate : fDivingSwimHeadingLerpRate, fSwimApproachApproachRate);
	float fPitchScale = bFPSMode ? 1.0f : Clamp(1.0f-fabs(pPed->GetCurrentPitch()/HALF_PI), 0.0f, 1.0f);
	
	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(pPed->GetMotionData()->GetExtraHeadingChangeThisFrame() + fHeadingDelta*fwTimer::GetTimeStep()*m_fHeadingApproachRate*fPitchScale);

	return FSM_Continue;
}
void						CTaskMotionDiving::Swimming_OnExit()
{

}

//////////////////////////////////////////////////////////////////////////
// Swim stop behaviour
void						CTaskMotionDiving::Glide_OnEnter()
{
	m_fGlideForwardsVel = GetPed()->GetVelocity().Mag();
	m_fHeadingApproachRate = 0;
	m_fPrevDirection = 0;
	
	if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired))
	{
		HolsterWeapon(false);
	}

	//Set up weapon holding clips
	if (GetPed()->GetEquippedWeaponInfo() && GetPed()->GetEquippedWeaponInfo()->GetIsGun())
	{
		SetWeaponHoldingClip(true);
	}
	else
	{
		SetWeaponHoldingClip(false);
	}
}

CTask::FSM_Return					CTaskMotionDiving::Glide_OnUpdate()
{
	CPed* pPed = GetPed();
	if (CTaskMotionSwimming::CheckForWaterfall(pPed))
		return FSM_Continue;	

	const CWeaponInfo *pWeaponInfo = pPed->GetEquippedWeaponInfo();
	m_bForceScubaAnims = pWeaponInfo && pWeaponInfo->GetIsGun();
	// If m_bForceScubaAnims is out of sync with bForceScubaAnims then reset anims
	if (m_bForceScubaAnims != m_bResetAnims)
	{
		m_bResetAnims = m_bForceScubaAnims;
		ResetDivingClipSet();
	}

	if (pWeaponInfo && pWeaponInfo->GetIsGun())
	{
		SetWeaponHoldingClip(true);
	}
	else
	{
		SetWeaponHoldingClip(false);
	}

	static dev_float fGlideDecaySpd = 1.5f;
	m_fGlideForwardsVel = Max(0.0f, m_fGlideForwardsVel - (fwTimer::GetTimeStep() / fGlideDecaySpd));

	static dev_float fGlideEndSpeed = 0.01f;

	if (m_fGlideForwardsVel < fGlideEndSpeed && !m_bDiveGaitReduced)
	{
		static const fwMvRequestId stopGlidingId("stop_gliding",0xC97B1F9C);
		m_MoveNetworkHelper.SendRequest(stopGlidingId);
	}

	UpdateRiverReorientation(pPed);
	//Heading
	TUNE_GROUP_FLOAT (PED_SWIMMING, ms_fDivingGlideHeadingLerpRate, 0.25f,0.0f,20.0f,0.001f);	
	if (pPed->IsLocalPlayer())
	{
		CControl * pControl = pPed->GetControlFromPlayer();
		taskAssert(pControl!=NULL);	
		static dev_float sfIdleApproachApproachRate = 2.0f;
		float stickOffsetX = pControl->GetPedWalkLeftRight().GetNorm();
		bool bInFPSMode = false;
#if FPS_MODE_SUPPORTED
		bInFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif
		// Allow heading update in FPS mode regardless of left stick input (as we want to rotate based on camera heading).
		if ((fabs(stickOffsetX) > 0.25f) || bInFPSMode)
		{											
			//there's no turn in these clips, so we'll need to turn the ped directly here in the task 
			const float fHeadingDelta = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());

			if (Sign(m_fPrevDirection) != Sign(fHeadingDelta)) //direction flip
			{					
				m_fPrevDirection = fHeadingDelta;
				m_fHeadingApproachRate = 0;
			}
			InterpValue(m_fHeadingApproachRate, ms_fDivingGlideHeadingLerpRate, sfIdleApproachApproachRate);			
			
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(pPed->GetMotionData()->GetExtraHeadingChangeThisFrame() + fHeadingDelta*fwTimer::GetTimeStep()*m_fHeadingApproachRate);
	
		}
	}
	else
	{
		//there's no turn in these clips, so we'll need to turn the ped directly here in the task 
		const float fHeadingDelta = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());	
		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(pPed->GetMotionData()->GetExtraHeadingChangeThisFrame() + fHeadingDelta*fwTimer::GetTimeStep()*ms_fDivingGlideHeadingLerpRate);
	}

	return FSM_Continue;
}

void CTaskMotionDiving::UpdateRiverReorientation(CPed *pPed) 
{
	//river re-orientation
	Vector3 vRiverFlow = pPed->m_Buoyancy.GetLastRiverVelocity();		
	static dev_float MIN_RIVERFLOW_VELOCITY = 0.5f;
	if (vRiverFlow.Mag() > MIN_RIVERFLOW_VELOCITY)
	{
		vRiverFlow.z=0; vRiverFlow.Normalize();
#if __DEV
		Vector3 debugStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()); 
		Vector3 debugEnd = debugStart + vRiverFlow;
		CTask::ms_debugDraw.AddArrow(VECTOR3_TO_VEC3V(debugStart), VECTOR3_TO_VEC3V(debugEnd), 0.1f, Color_red, 10);	
#endif
		Vector3 pedNormal(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()));
		float fHeadingDelta = -vRiverFlow.AngleZ(pedNormal);

		pedNormal.z=0; pedNormal.Normalize();
		float scale = 1.0f - vRiverFlow.Dot(pedNormal);

		static dev_float maxHeadingChangeRate = PI/3.0f;		
		static dev_float minHeadingChangeRate = 0.2f;
		float headingChangeRate = Lerp(Min(1.0f, scale),  minHeadingChangeRate, maxHeadingChangeRate);
		pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(Sign(fHeadingDelta) * Min(Abs(fHeadingDelta), headingChangeRate * fwTimer::GetTimeStep()));
	}
}


float CTaskMotionDiving::TestGroundClearance(CPed *pPed, float testDepth, float heightOffset, bool scaleStart) 
{
	WorldProbe::CShapeTestFixedResults<> capsuleResult;		
	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	float fGroundClearance = -1.0f;

	Vector3 vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());		

	//check for the bottom of the world
	static dev_float s_fMinWorldZ = -190.0f; //see LOWESTPEDZ
	static dev_float s_fMinWorldZBuffer = 1.5f;
	if (vStart.z <= (s_fMinWorldZ + s_fMinWorldZBuffer))
		return Max(0.0f, (vStart.z - s_fMinWorldZ));

	if (scaleStart)
	{
		Matrix34 mtx;
		if (!pPed->GetBoneMatrix(mtx, BONETAG_ROOT))
			return fGroundClearance;

		vStart.AddScaled(mtx.c, 0.5f);
	}
	Vector3 vEnd(vStart);
	vStart.z += heightOffset;
	vEnd.z-=testDepth;
	static dev_float sfCapsuleRadius = 0.15f;
	// Test flags
	static const int iBoundFlags   = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE; 
	capsuleTest.SetCapsule(vStart, vEnd, sfCapsuleRadius);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetResultsStructure(&capsuleResult);
	capsuleTest.SetIncludeFlags(iBoundFlags);		


	static const s32 iNumExceptions = 1;
	const CEntity* ppExceptions[iNumExceptions] = { pPed };	
	capsuleTest.SetExcludeEntities(ppExceptions,iNumExceptions);

	if (WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest))
	{
		for (int i = 0; i < capsuleResult.GetNumHits(); i++)
		{
			static dev_float sfGroundRejection = 0.8f;
			// If any of the hits pass the ground rejection test then use them...
			if (capsuleResult[i].GetHitNormal().z > sfGroundRejection)
			{
				Vector3 hitPos = capsuleResult[i].GetHitPosition();
				hitPos.Subtract(vStart);
				fGroundClearance = hitPos.Mag();
				break;
			}
			else 
				fGroundClearance = 0; //hit something 

		}
	} 
	return fGroundClearance;
}

void CTaskMotionDiving::SetWeaponClipSet(const fwMvClipSetId& clipSetId, const fwMvFilterId& filterId, bool bUpperBodyShadowExpression)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

	if(m_WeaponClipSetId != clipSetId)
	{

		// Store the new clip set
		m_WeaponClipSetId = clipSetId;

		bool bClipSetStreamed = false;
		if(m_WeaponClipSetId == CLIP_SET_ID_INVALID)
		{
			// Clip set doesn't need to be streamed
			bClipSetStreamed = true;
			m_WeaponClipSet = NULL;
		}
		else
		{
			m_WeaponClipSet = fwClipSetManager::GetClipSet(m_WeaponClipSetId);
			if(taskVerifyf(m_WeaponClipSet, "Weapon clip set [%s] doesn't exist", m_WeaponClipSetId.GetCStr()))
			{
				if(m_WeaponClipSet->Request_DEPRECATED())
				{
					// Clip set streamed
					bClipSetStreamed = true;
				}
			}
		}

		if(m_MoveNetworkHelper.IsNetworkActive() && bClipSetStreamed)
		{
			// Send immediately
			SendWeaponClipSet();
		}
		else
		{
			// Flag the change
			m_Flags.SetFlag(Flag_WeaponClipSetChanged);
		}
	}

	if(m_WeaponFilterId != filterId)
	{
		// Clear any old filter
		if(m_pWeaponFilter)
		{
			m_pWeaponFilter->Release();
			m_pWeaponFilter = NULL;
		}

		// Store the new filter
		m_WeaponFilterId = filterId;

		if(m_WeaponFilterId != FILTER_ID_INVALID)
		{
			if(GetEntity())
			{
				// Store now
				StoreWeaponFilter();
			}
			else
			{
				// Flag the change
				m_Flags.SetFlag(Flag_WeaponFilterChanged);
			}
		}
		else
		{
			// Clear any pending flags, as the filter is now set to invalid
			m_Flags.ClearFlag(Flag_WeaponFilterChanged);
		}
	}

	if(bUpperBodyShadowExpression)
	{
		m_Flags.SetFlag(Flag_UpperBodyShadowExpression);
	}
	else
	{
		m_Flags.ClearFlag(Flag_UpperBodyShadowExpression);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionDiving::SendWeaponClipSet()
{
	m_MoveNetworkHelper.SetClipSet(m_WeaponClipSetId, CTaskHumanLocomotion::ms_WeaponHoldingClipSetVarId);

	bool bUseWeaponHolding = m_WeaponClipSetId != CLIP_SET_ID_INVALID;

	//Set up weapon holding clips
	CPed* pPed = GetPed();
	taskAssert(pPed);
	if (pPed && pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsGun())
	{
		SetWeaponHoldingClip(true);
	}
	else
	{
		SetWeaponHoldingClip(false);
	}

	m_MoveNetworkHelper.SetFlag(bUseWeaponHolding, CTaskHumanLocomotion::ms_UseWeaponHoldingId);
	if (bUseWeaponHolding)
		m_MoveNetworkHelper.SendRequest(CTaskHumanLocomotion::ms_RestartWeaponHoldingId);

	// Don't do this again
	m_Flags.ClearFlag(Flag_WeaponClipSetChanged);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionDiving::StoreWeaponFilter()
{
	if(taskVerifyf(m_WeaponFilterId != FILTER_ID_INVALID, "Weapon filter [%s] is invalid", m_WeaponFilterId.GetCStr()))
	{
		m_pWeaponFilter = CGtaAnimManager::FindFrameFilter(m_WeaponFilterId.GetHash(), GetPed());
		if(taskVerifyf(m_pWeaponFilter, "Failed to get filter [%s]", m_WeaponFilterId.GetCStr()))	
		{
			m_pWeaponFilter->AddRef();
			m_Flags.ClearFlag(Flag_WeaponFilterChanged);
		}
	}
}
