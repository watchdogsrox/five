#include "ai/debug/types/TargetingDebugInfo.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"

#if AI_DEBUG_OUTPUT_ENABLED

// game headers
#include "Peds/PedWeapons/PlayerPedTargeting.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "Peds/PedIntelligence.h"
#include "Weapons/Info/WeaponInfo.h"
#include "Peds/PedDebugVisualiser.h"
#include "Vehicles/VehicleGadgets.h"

#include "Peds/Ped.h"
#include "script/script_hud.h"

CTargetingDebugInfo::CTargetingDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CTargetingDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("TARGETING");
		PushIndent();
			PushIndent();
				PushIndent();
					PrintTargeting();
}

void CTargetingDebugInfo::Visualise()
{
	if (!ValidateInput())
		return;

	CPedDebugVisualiserMenu::CreateBank();

	CPedTargetEvaluator::EnableDebugScoring(true);
		if (m_Ped->IsLocalPlayer() && m_Ped->GetPlayerInfo())
		{
			bool bProcessRejectionStrings = CPedTargetEvaluator::PROCESS_REJECTION_STRING;
			bool bDisplayRejectionStringsPeds = CPedTargetEvaluator::DISPLAY_REJECTION_STRING_PEDS;
			bool bDisplayRejectionStringsObjects = CPedTargetEvaluator::DISPLAY_REJECTION_STRING_OBJECTS;
			bool bDisplayRejectionStringsVehs = CPedTargetEvaluator::DISPLAY_REJECTION_STRING_VEHICLES;
			CPedTargetEvaluator::PROCESS_REJECTION_STRING = true;
			CPedTargetEvaluator::DISPLAY_REJECTION_STRING_PEDS = false;
			CPedTargetEvaluator::DISPLAY_REJECTION_STRING_OBJECTS = false;
			CPlayerPedTargeting & rPlayerTargeting = m_Ped->GetPlayerInfo()->GetTargeting();
			if(m_Ped->GetVehiclePedInside() && m_Ped->GetWeaponManager()->GetEquippedVehicleWeapon())
			{
				CPedTargetEvaluator::DISPLAY_REJECTION_STRING_VEHICLES = true;

				CEntity* pCurrentTarget = rPlayerTargeting.GetLockOnTarget();

				int iTargetingFlags = CTaskVehicleMountedWeapon::GetVehicleTargetingFlags(m_Ped->GetVehiclePedInside(), m_Ped, m_Ped->GetWeaponManager()->GetEquippedVehicleWeapon(), pCurrentTarget);

				float fWeaponRange = 0.0f;
				float fHeadingLimit = 0.0f;
				float fPitchLimitMin = 0.0f;
				float fPitchLimitMax = 0.0f;
				CTaskVehicleMountedWeapon::FindTarget(m_Ped->GetVehiclePedInside(), m_Ped, m_Ped->GetWeaponManager()->GetEquippedVehicleWeapon(), pCurrentTarget, NULL, iTargetingFlags, fWeaponRange, fHeadingLimit, fPitchLimitMin, fPitchLimitMax);
			}
			else
			{
				CPedTargetEvaluator::DISPLAY_REJECTION_STRING_VEHICLES = false;
				rPlayerTargeting.FindLockOnTarget();
			}

			CPedTargetEvaluator::PROCESS_REJECTION_STRING = bProcessRejectionStrings;
			CPedTargetEvaluator::DISPLAY_REJECTION_STRING_PEDS = bDisplayRejectionStringsPeds;
			CPedTargetEvaluator::DISPLAY_REJECTION_STRING_OBJECTS = bDisplayRejectionStringsObjects;
			CPedTargetEvaluator::DISPLAY_REJECTION_STRING_VEHICLES = bDisplayRejectionStringsVehs;
			CTask::ms_debugDraw.Render();
		}
	CPedTargetEvaluator::EnableDebugScoring(false);
}

bool CTargetingDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	if (!m_Ped->GetPlayerInfo() && !m_Ped->GetPedIntelligence()->GetTargetting(false))
		return false;

	return true;
}


void CTargetingDebugInfo::PrintTargeting()
{
	if (m_Ped->IsLocalPlayer() && m_Ped->GetPlayerInfo())
	{
		const CPlayerPedTargeting & rPlayerTargeting = m_Ped->GetPlayerInfo()->GetTargeting();

		ColorPrintLn(rPlayerTargeting.GetLockOnTarget() ? Color_red : Color_green, "%s Lockon target 0x%p", rPlayerTargeting.GetLockOnTarget() ? "HAS" : "NO", rPlayerTargeting.GetLockOnTarget());
		ColorPrintLn(rPlayerTargeting.GetLockOnTarget() ? Color_red : Color_green, "%s Free aim target 0x%p", rPlayerTargeting.GetFreeAimTarget() ? "HAS" : "NO", rPlayerTargeting.GetFreeAimTarget());
		ColorPrintLn(rPlayerTargeting.GetLockOnTarget() ? Color_red : Color_green, "%s Free aim ragdoll target 0x%p", rPlayerTargeting.GetFreeAimTargetRagdoll() ? "HAS" : "NO", rPlayerTargeting.GetFreeAimTargetRagdoll());
			
		Vector3 vFreeAimPos = rPlayerTargeting.GetClosestFreeAimTargetPos();
		ColorPrintLn(Color_red, "Free Aim Position %.2f:%.2f:%.2f", vFreeAimPos.x, vFreeAimPos.y, vFreeAimPos.z );

		u32 nTimeSinceAiming = fwTimer::GetTimeInMilliseconds() - rPlayerTargeting.GetTimeSinceLastAiming(); 

		float fDistanceWeight = (float)nTimeSinceAiming/(float)CPedTargetEvaluator::ms_Tunables.m_TargetDistanceMaxWeightingAimTime;
		fDistanceWeight = Min(fDistanceWeight, 1.0f);
		float fMin = CPedTargetEvaluator::ms_Tunables.m_TargetDistanceWeightingMin;
		float fMax = CPedTargetEvaluator::ms_Tunables.m_TargetDistanceWeightingMax;

		fDistanceWeight = fMin + ((fMax - fMin) * fDistanceWeight);

		ColorPrintLn(fDistanceWeight <= 1.0f ? Color_red : Color_green, "Distance Weight: %.2f", fDistanceWeight);

		u32 nCurrentTargetingMode = rPlayerTargeting.GetCurrentTargetingMode();
		ColorPrintLn(Color_red, "Targeting Mode: %u", nCurrentTargetingMode);

		ColorPrintLn(Color_red, "Should Use Targeting: %s", rPlayerTargeting.ShouldUpdateTargeting() ? "true" : "false");

		ColorPrintLn(Color_red, "Targeting Disabled: %s Wheel %s", rPlayerTargeting.IsLockOnDisabled() ? "true" : "false", rPlayerTargeting.IsDisabledDueToWeaponWheel() ? "true" : "false");

		if(rPlayerTargeting.GetWeaponInfo())
		{
			ColorPrintLn(Color_red, "Weapon Hash: %u (%s)", rPlayerTargeting.GetWeaponInfo()->GetHash(), rPlayerTargeting.GetWeaponInfo()->GetName());
		}
		else
		{
			const CPedWeaponManager *pWeapMgr = m_Ped->GetWeaponManager();
			if (pWeapMgr)
			{
				ColorPrintLn(Color_red, "Equipped: %d/%s, Selected: %d/%s", 
					pWeapMgr->GetEquippedWeaponInfo() ? pWeapMgr->GetEquippedWeaponInfo()->GetHash() : 0, 
					pWeapMgr->GetEquippedWeaponInfo() ? pWeapMgr->GetEquippedWeaponInfo()->GetName() : "",
					pWeapMgr->GetSelectedWeaponInfo() ? pWeapMgr->GetSelectedWeaponInfo()->GetHash() : 0, 
					pWeapMgr->GetSelectedWeaponInfo() ? pWeapMgr->GetSelectedWeaponInfo()->GetName() : "");
			}
		}

		ColorPrintLn(Color_red, "No Targeting Mode: %s", rPlayerTargeting.GetNoTargetingMode() ? "true" : "false");

		ColorPrintLn(Color_orange, "Vehicle Homing Enabled: %s", rPlayerTargeting.GetVehicleHomingEnabled() ? "true" : "false");
		ColorPrintLn(Color_orange, "Vehicle Homing Force Disabled: %s", rPlayerTargeting.GetVehicleHomingForceDisabled() ? "true" : "false");
	}
	else
	{
		CPedTargetting* pPedTargetting = m_Ped->GetPedIntelligence()->GetTargetting(false);
		if(pPedTargetting)
		{
			Vector3 vLastSeenPosition = VEC3_ZERO;
			float fLastSeenTimer = 0.0f;
			bool bAreTargetsWhereaboutsKnown = pPedTargetting->AreTargetsWhereaboutsKnown(&vLastSeenPosition, pPedTargetting->GetCurrentTarget(), &fLastSeenTimer);

			ColorPrintLn(Color_blue, "Are target whereabouts known: %s", bAreTargetsWhereaboutsKnown ? "yes" : "no");
			ColorPrintLn(Color_blue, "Last seen position: %.2f, %.2f, %.2f", vLastSeenPosition.x, vLastSeenPosition.y, vLastSeenPosition.z);
			ColorPrintLn(Color_blue, "Last seen timer: %.2f", fLastSeenTimer);
			ColorPrintLn(Color_blue, "Targetable with No LOS: %s", m_Ped->GetPedConfigFlag( CPED_CONFIG_FLAG_TargettableWithNoLos) ? "true" : "false");

			if(!m_Ped->GetPlayerInfo())
			{
				ColorPrintLn(Color_blue, "Ped %s active targets:", m_Ped->GetLogName());
				const int numActiveTargets = pPedTargetting->GetNumActiveTargets();
				for(int i = 0; i < numActiveTargets; i++)
				{
					const CEntity* pTarget = pPedTargetting->GetTargetAtIndex(i);
					CSingleTargetInfo* pTargetInfo = pTarget ? pPedTargetting->FindTarget(pTarget) : NULL;
					if(pTargetInfo)
					{
						ColorPrintLn(Color_green, "-------");
						if(pTarget == pPedTargetting->GetCurrentTarget())
						{
							ColorPrintLn(Color_green, "-CURRENT TARGET-");
						}

						ColorPrintLn(Color_blue, "Target log name: %s", pTarget->GetLogName());

						switch(pTargetInfo->GetLosStatus())
						{
						case Los_blocked:
							{
								ColorPrintLn(Color_blue, "Los Status: Blocked");
								break;
							}
						case Los_blockedByFriendly:
							{
								ColorPrintLn(Color_blue, "Los Status: Blocked by friendly");
								break;
							}
						case Los_clear:
							{
								ColorPrintLn(Color_blue, "Los Status: Clear");
								break;
							}
						case Los_unchecked:
							{
								ColorPrintLn(Color_blue, "Los Status: Unchecked");
								break;
							}
						default:
							{
								ColorPrintLn(Color_blue, "Los Status: Unknown");
								break;
							}
						}
					}
				}
			}
		}

		if(m_Ped->GetLastPlayerTargetingRejectionString())
		{
			ColorPrintLn(Color_red, "Last Rejection: %s (%d)", m_Ped->GetLastPlayerTargetingRejectionString(), m_Ped->GetLastPlayerTargetingRejectedFrame());
		}
	}
}

#endif // AI_DEBUG_OUTPUT_ENABLED