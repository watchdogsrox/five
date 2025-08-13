#include "ai\debug\types\MovementDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

// game headers
#include "Peds\PedEventScanner.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PlayerInfo.h"
#include "task\Motion\TaskMotionBase.h"

CMovementTextDebugInfo::CMovementTextDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CMovementTextDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("MOVEMENT");
		PushIndent();
			PushIndent();
				PushIndent();
					PrintMovementDebug();
}

void CMovementTextDebugInfo::Visualise()
{
#if DEBUG_DRAW
	float fStoppingDist = 0.0f;
	CTaskMotionBase* pCurrentMotionTask = m_Ped->GetCurrentMotionTask();
	if (pCurrentMotionTask)
	{
		fStoppingDist = pCurrentMotionTask->GetStoppingDistance(m_Ped->GetCurrentHeading());
	}

	// Visualise stopping distance
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
	const Vector3 vStopPos = vPedPos + (VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetB()) * fStoppingDist);
	grcDebugDraw::Line(vStopPos - Vector3(0.1f,0.0f,0.0f), vStopPos + Vector3(0.1f,0.0f,0.0f), Color_LightCyan4);
	grcDebugDraw::Line(vStopPos - Vector3(0.0f,0.1f,0.0f), vStopPos + Vector3(0.0f,0.1f,0.0f), Color_LightCyan4);
	grcDebugDraw::Line(vPedPos, vStopPos, Color_LightCyan);

	// Sprint
	if (m_Ped->GetPlayerInfo())
	{
		static Vector2 vAxisDir(0.0f,-1.0f);
		static Vector2 vValueAxisDir(0.0f,-1.0f);
		static Vector2 vMeterPos(0.8f,0.25f);
		static Vector2 vMeterValuePos(0.8f,0.25f);
		static float fScale = 0.2f;
		static float fEndWidth = 0.01f;

		s32 iSprintType = CPlayerInfo::SPRINT_ON_FOOT;

		if (m_Ped->GetIsSwimming())
		{
			iSprintType = CPlayerInfo::SPRINT_ON_WATER;
		}
		else if (m_Ped->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_ON_BICYCLE))
		{
			iSprintType = CPlayerInfo::SPRINT_ON_BICYCLE;
		}

		if (iSprintType < CPlayerInfo::ms_Tunables.m_SprintControlData.GetCount())
		{
			const CPlayerInfo::sSprintControlData& rSprintControlData = CPlayerInfo::ms_Tunables.m_SprintControlData[iSprintType];
			if (rSprintControlData.m_Threshhold  > 0.0f && rSprintControlData.m_MaxLimit > 0.0f)
			{
				const bool bPastThreshold = m_Ped->GetPlayerInfo()->GetPlayerDataSprintControlCounter() / rSprintControlData.m_Threshhold > 1.0f ? true : false;
				grcDebugDraw::Meter(vMeterPos, vAxisDir,fScale, fEndWidth, Color_black, "Sprint Control Meter");
				float fValue = m_Ped->GetPlayerInfo()->GetPlayerDataSprintControlCounter() / rSprintControlData.m_MaxLimit;
				grcDebugDraw::MeterValue(vMeterValuePos, vValueAxisDir, fScale, fValue, fEndWidth, bPastThreshold ? Color_green : Color_red, true);
				// Does not know why we need to map this value to -1 to 1 but not the above
				float fThreshHoldValue = rSprintControlData.m_Threshhold / rSprintControlData.m_MaxLimit;
				fThreshHoldValue *= 2.0f;
				fThreshHoldValue -= 1.0f;	
				grcDebugDraw::MeterValue(vMeterValuePos, vValueAxisDir, fScale, fThreshHoldValue, fEndWidth, Color_blue);	
			}
		}
	}

	Vector3 vDbgPos = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
	vDbgPos.z -= 0.5f;
	grcDebugDraw::Line(vDbgPos, vDbgPos + (VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetB()) * fStoppingDist), Color_LightCyan, Color_LightCyan4);

	if (m_Ped->IsOnGround())
	{
		Vector3 vGroundPos = m_Ped->GetGroundPos();
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
		grcDebugDraw::Sphere(vGroundPos, 0.1f, Color_orange);
		grcDebugDraw::Line(vPedPosition,vGroundPos, Color_green, Color_green);
	}

#endif // DEBUG_DRAW
}

bool CMovementTextDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	return true;
}

void CMovementTextDebugInfo::PrintMovementDebug()
{
	const fwDynamicEntityComponent *pedDynComp = m_Ped->GetDynamicComponent();

	Color32 colour(255,192,128);

	Vector3 pedAnimatedVelocity = (pedDynComp) ? pedDynComp->GetAnimatedVelocity() : VEC3_ZERO;
	Vector3 pedGroundVelocityInt = VEC3V_TO_VECTOR3(m_Ped->GetGroundVelocityIntegrated());
	if(m_Ped->GetVehiclePedInside())
	{
		pedGroundVelocityInt = m_Ped->GetVehiclePedInside()->GetVelocity();
	}

	float fStoppingDist = 0.0f;
	CTaskMotionBase* pBaseMotionTask = m_Ped->GetPrimaryMotionTask();
	CTaskMotionBase* pCurrentMotionTask = m_Ped->GetCurrentMotionTask();
	if (pCurrentMotionTask)
	{
		fStoppingDist = pCurrentMotionTask->GetStoppingDistance(m_Ped->GetCurrentHeading());
	}

	Vector2 vCurrentMoveBlendRatio, vDesiredMoveBlendRatio, vGaitReducedMoveBlendRatio;
	m_Ped->GetMotionData()->GetCurrentMoveBlendRatio(vCurrentMoveBlendRatio);
	m_Ped->GetMotionData()->GetDesiredMoveBlendRatio(vDesiredMoveBlendRatio);
	m_Ped->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio(vGaitReducedMoveBlendRatio);

	if (pBaseMotionTask)
	{
		ColorPrintLn(Color_green3, "%s", pBaseMotionTask->GetStateName(pBaseMotionTask->GetState()));
	}

	// Lung
	const CInWaterEventScanner& waterScanner = m_Ped->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner();
	ColorPrintLn(colour, "Time Submerged Underwater (%.2f) Max (%.2f) Submerge Level (%.2f)", waterScanner.GetTimeSubmerged(), CPlayerInfo::GetMaxTimeUnderWaterForPed(*m_Ped), m_Ped->GetWaterLevelOnPed());

	Vector3 vLastRiverVelocity(Vector3::ZeroType);
	Vector3 vRiverHitPosition;
	Vector3 vRiverHitNormal;
	if (m_Ped->m_Buoyancy.GetCachedRiverBoundProbeResult(vRiverHitPosition, vRiverHitNormal))
	{
		vLastRiverVelocity = m_Ped->m_Buoyancy.GetLastRiverVelocity();
	}
	ColorPrintLn(colour, "River flow velocity (%.3f %.3f %.3f) [%.3f]", vLastRiverVelocity.x, vLastRiverVelocity.y, vLastRiverVelocity.z, vLastRiverVelocity.Mag());
	ColorPrintLn(colour, "Buoyancy Status %s", m_Ped->m_Buoyancy.GetStatus() > NOT_IN_WATER ? (m_Ped->m_Buoyancy.GetStatus() == PARTIALLY_IN_WATER ? "PARTIALLY_IN_WATER" : "FULLY_IN_WATER") : "NOT_IN_WATER");

	const CBuoyancyInfo* pBuoyancyInfo = m_Ped->m_Buoyancy.GetBuoyancyInfo(m_Ped);
	if (pBuoyancyInfo)
	{
		ColorPrintLn(colour, "Num Water Samples : %i", pBuoyancyInfo->m_nNumWaterSamples);
		ColorPrintLn(colour, "Water Samples: %p", pBuoyancyInfo->m_WaterSamples);
	}

	ColorPrintLn(colour, "Num Components With Samples : %i", m_Ped->m_Buoyancy.m_nNumComponentsWithSamples);

	// Sprint
	if (m_Ped->GetPlayerInfo())
	{
		ColorPrintLn(colour, "Sprint Control Counter (%.2f)", m_Ped->GetPlayerInfo()->GetPlayerDataSprintControlCounter());

		s32 iSprintType = CPlayerInfo::SPRINT_ON_FOOT;

		if (m_Ped->GetIsSwimming())
		{
			iSprintType = CPlayerInfo::SPRINT_ON_WATER;
		}
		else if (m_Ped->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_ON_BICYCLE))
		{
			iSprintType = CPlayerInfo::SPRINT_ON_BICYCLE;
		}

		ColorPrintLn(colour, "Sprint Energy (%.2f) Max (%.2f)", m_Ped->GetPlayerInfo()->GetPlayerDataSprintEnergy(), m_Ped->GetPlayerInfo()->GetPlayerDataMaxSprintEnergy());

		float fStaminaStat = 0.0f;
		float fMinDuration = 0.0f;
		float fMaxDuration = 0.0f;
		CPlayerInfo::GetStatValuesForSprintType(*m_Ped, fStaminaStat, fMinDuration, fMaxDuration, (CPlayerInfo::eSprintType)iSprintType);
		float fCurrentMaxDuration = CPlayerInfo::ComputeDurationFromStat(fStaminaStat, fMinDuration, fMaxDuration);
		ColorPrintLn(colour, "Time Sprinting (%.2f) Current Max (%.2f) Min/Max (%.2f / %.2f) Stamina (%.2f)", m_Ped->GetPlayerInfo()->GetTimeBeenSprinting(), fCurrentMaxDuration, fMinDuration, fMaxDuration, fStaminaStat);
		ColorPrintLn(m_Ped->GetPlayerInfo()->GetPlayerDataSprintReplenishing() ? Color_red : colour, "Sprint Replenishing : %s", m_Ped->GetPlayerInfo()->GetPlayerDataSprintReplenishing() ? "True" : "False");	
	}

	ColorPrintLn(colour, "CurrentMBR(%.2f,%.3f)", vCurrentMoveBlendRatio.x, vCurrentMoveBlendRatio.y);
	ColorPrintLn(colour, "DesiredMBR(%.2f,%.3f), GaitReducedMBR(%.2f,%.3f)", vDesiredMoveBlendRatio.x, vDesiredMoveBlendRatio.y, vGaitReducedMoveBlendRatio.x, vGaitReducedMoveBlendRatio.y);
	ColorPrintLn(colour, "ScriptedMBR Min / Max(%.2f/%.3f)", m_Ped->GetMotionData()->GetScriptedMinMoveBlendRatio(), m_Ped->GetMotionData()->GetScriptedMaxMoveBlendRatio());
	
	// anim rate overrides
	if(m_Ped->GetPlayerInfo())
		ColorPrintLn(colour, "OverrideRate(%.2f), RunRate(%.2f), SwimRate(%.2f)", m_Ped->GetMotionData()->GetCurrentMoveRateOverride(), m_Ped->GetPlayerInfo()->GetRunSprintSpeedMultiplier(), m_Ped->GetPlayerInfo()->m_fSwimSpeedMultiplier);
	else
		ColorPrintLn(colour, "OverrideRate(%.2f)", m_Ped->GetMotionData()->GetCurrentMoveRateOverride());

	// desired direction (yaw)
	const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(m_Ped->GetCurrentHeading());
	const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(m_Ped->GetDesiredHeading());
	const float	fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
	ColorPrintLn(colour, "DesiredDirection(Rad:%.3f, Deg:%.3f Extra:%.3f)", fHeadingDelta, RtoD*fHeadingDelta, m_Ped->GetMotionData()->GetExtraHeadingChangeThisFrame());
	ColorPrintLn(colour, "Desired Heading(=%.2f) : Current Heading(=%.2f)", m_Ped->GetDesiredHeading(), m_Ped->GetCurrentHeading());
	Vector3 vMoveSpeed = m_Ped->GetVelocity();
	ColorPrintLn(colour, "Vel(%.2f,%.2f,%.2f)(=%.2f)", vMoveSpeed.x, vMoveSpeed.y, vMoveSpeed.z, vMoveSpeed.Mag());

	Vector3 vDesiredMoveSpeed = m_Ped->GetDesiredVelocity();
	ColorPrintLn(colour, "DesiredVel(%.2f,%.2f,%.2f)(=%.2f)", vDesiredMoveSpeed.x, vDesiredMoveSpeed.y, vDesiredMoveSpeed.z, vDesiredMoveSpeed.Mag());	
	ColorPrintLn(colour, "XVel(%.2f,%.2f,%.2f)(=%.2f)", pedAnimatedVelocity.x, pedAnimatedVelocity.y, pedAnimatedVelocity.z, pedAnimatedVelocity.Mag());
	ColorPrintLn(colour, "GroundVelInt(%.2f,%.2f,%.2f)(=%.2f)", pedGroundVelocityInt.x, pedGroundVelocityInt.y, pedGroundVelocityInt.z, pedGroundVelocityInt.Mag());
	ColorPrintLn(colour, "ExtractedAngVel(%.2f), ActualAngVelz(%.2f), ActualAngVelMag(%.2f), ExtraHeadingChange(%.4f)", m_Ped->GetAnimatedAngularVelocity(), m_Ped->GetAngVelocity().z, m_Ped->GetAngVelocity().Mag(), m_Ped->GetMotionData()->GetExtraHeadingChangeThisFrame());

	// Reference frame velocity
	if (m_Ped->GetCollider())
	{
		Vec3V vMoveSpeed = m_Ped->GetCollider()->GetReferenceFrameVelocity();
		ColorPrintLn(colour, "ReferenceFrameVel(%.2f,%.2f,%.2f)(=%.2f)", vMoveSpeed.GetXf(), vMoveSpeed.GetYf(), vMoveSpeed.GetZf(), Mag(vMoveSpeed).Getf());
	}

	Vector3 vLocalMoveSpeed = VEC3V_TO_VECTOR3(m_Ped->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vMoveSpeed)));
	ColorPrintLn(colour, "LocalVel(%.2f,%.2f,%.2f)(=%.2f)", vLocalMoveSpeed.x, vLocalMoveSpeed.y, vLocalMoveSpeed.z, vLocalMoveSpeed.Mag());
	ColorPrintLn(colour, "Strafe: %s Crouch: %s, Stealth: %s", m_Ped->IsStrafing() ? "yes" : "no", m_Ped->GetIsCrouching() ? "yes" : "no", m_Ped->GetMotionData()->GetUsingStealth() ? "yes" : "no");

	// Gravity switched off (aka can extract Z)
	ColorPrintLn(colour, "UseExtZVel : %s", m_Ped->GetUseExtractedZ() ? "on" : "off");
	ColorPrintLn(colour, "IsStanding : %s", m_Ped->GetIsStanding() ? "on" : "off");
	ColorPrintLn(colour, "IsOnGround : %s %s", m_Ped->IsOnGround() ? "on" : "off", m_Ped->GetGroundPhysical() ? "(P)" : "");

	Vector3 vGroundPos = m_Ped->GetGroundPos();
	ColorPrintLn(colour, "GroundPos : %.2f:%.2f:%.2f", vGroundPos.x, vGroundPos.y, vGroundPos.z);

	// bUsesCollision/bIsFixed
	ColorPrintLn(m_Ped->IsCollisionEnabled() ? colour : Color_red, "Collision: %s", m_Ped->IsCollisionEnabled() ? "on" : "off");

	// NoCollisionEntity
		ColorPrintLn(m_Ped->GetNoCollisionEntity() ? Color_red : colour, "No Collision Entity: (%s) - 0x%p", m_Ped->GetNoCollisionEntity() ? m_Ped->GetNoCollisionEntity()->GetModelName() : "NULL", m_Ped->GetNoCollisionEntity());

	// No collision flags
	const bool bNoCollisionHitShouldFindImpacts = (m_Ped->GetNoCollisionFlags() & NO_COLLISION_HIT_SHOULD_FIND_IMPACTS) ? true : false;
	const bool bNoCollisionNetworkObjects		= (m_Ped->GetNoCollisionFlags() & NO_COLLISION_NETWORK_OBJECTS) ? true : false;
	const bool bNoCollisionPermanent			= (m_Ped->GetNoCollisionFlags() & NO_COLLISION_PERMENANT) ? true : false;
	const bool bNoCollisionResetWhenNoBBox		= (m_Ped->GetNoCollisionFlags() & NO_COLLISION_RESET_WHEN_NO_BBOX) ? true : false;
	const bool bNoCollisionResetWhenNoImpacts	= (m_Ped->GetNoCollisionFlags() & NO_COLLISION_RESET_WHEN_NO_IMPACTS) ? true : false;
	const bool bNoCollisionNeedsReset			= (m_Ped->GetNoCollisionFlags() & NO_COLLISION_NEEDS_RESET) ? true : false;
	const bool bAtLeastOneFlagSet				= ( bNoCollisionHitShouldFindImpacts || bNoCollisionNetworkObjects || bNoCollisionPermanent || bNoCollisionResetWhenNoBBox || bNoCollisionResetWhenNoImpacts || bNoCollisionNeedsReset) ? true : false;
	
	ColorPrintLn(bAtLeastOneFlagSet ? Color_red : colour, "No Collision Flags: %s | %s | %s | %s | %s | %s", 
		bNoCollisionHitShouldFindImpacts	? "NO_COLLISION_HIT_SHOULD_FIND_IMPACTS" : "NULL",
		bNoCollisionNetworkObjects			? "NO_COLLISION_NETWORK_OBJECTS" : "NULL", 
		bNoCollisionPermanent				? "NO_COLLISION_PERMENANT" : "NULL", 
		bNoCollisionResetWhenNoBBox			? "NO_COLLISION_RESET_WHEN_NO_BBOX" : "NULL", 
		bNoCollisionResetWhenNoImpacts		? "NO_COLLISION_RESET_WHEN_NO_IMPACTS" : "NULL", 
		bNoCollisionNeedsReset				? "NO_COLLISION_NEEDS_RESET" : "NULL");

	// RagdollOnCollisionIgnoreEntity
	ColorPrintLn(m_Ped->GetRagdollOnCollisionIgnorePhysical() ? Color_red : colour, "Ragdoll On Collision Ignore Physical: (%s) - 0x%p", m_Ped->GetRagdollOnCollisionIgnorePhysical() ? m_Ped->GetRagdollOnCollisionIgnorePhysical()->GetModelName() : "NULL", m_Ped->GetRagdollOnCollisionIgnorePhysical());

	// Is visible
	char isVisibleInfo[128];
	m_Ped->GetIsVisibleInfo(isVisibleInfo, 128);
	ColorPrintLn(m_Ped->IsBaseFlagSet(fwEntity::IS_VISIBLE) ? colour : Color_red, "IsVisible: %s", isVisibleInfo);

	if (m_Ped->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION))
	{
		ColorPrintLn(Color_red, "Fixed: Fixed waiting for collision!");
	}
	else if (m_Ped->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK))
	{
		ColorPrintLn(Color_red, "Fixed: Fixed by network!");
	}
	else if (m_Ped->IsBaseFlagSet(fwEntity::IS_FIXED))
	{
		ColorPrintLn(Color_red, "Fixed: Fixed by code/script!");
	}
	else
	{
		ColorPrintLn(colour, "Fixed: Not fixed");
	}
	
	ColorPrintLn(colour, "Load collision: %s", m_Ped->ShouldLoadCollision() ? "yes" : "no");
	ColorPrintLn(colour, "phInst->IsInLevel() : %s", m_Ped->GetCurrentPhysicsInst()->IsInLevel() ? "true" : "false");
	ColorPrintLn(colour, "TakingSplineCorner : %s", m_Ped->GetPedResetFlag(CPED_RESET_FLAG_TakingRouteSplineCorner) ? "true" : "false");

}

#endif // AI_DEBUG_OUTPUT_ENABLED