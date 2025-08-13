#include "ai\debug\types\CombatDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

// game headers
#include "Peds\CombatBehaviour.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "task\Combat\Cover\TaskCover.h"
#include "task\Combat\TaskInvestigate.h"

EXTERN_PARSER_ENUM(BehaviourFlags);
extern const char* parser_CCombatData__BehaviourFlags_Strings[];
extern const char* parser_CCombatData__Movement_Strings[];
extern const char* parser_CCombatData__TargetLossResponse_Strings[];
extern const char* parser_CCombatData__Range_Strings[];

CCombatTextDebugInfo::CCombatTextDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CCombatTextDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("COMBAT TEXT");
		PushIndent();
			PushIndent();
				PushIndent();
					PrintCombatText();
}

bool CCombatTextDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	if (!m_Ped->GetPedIntelligence()->GetRelationshipGroup())
		return false;

	return true;
}

void CCombatTextDebugInfo::PrintCombatText()
{
	const char* szRelGroupName = m_Ped->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetCStr();
	if (szRelGroupName)
	{
		ColorPrintLn(Color32(64,255,64), szRelGroupName);
	}

	// Display how pinned down the ped is
	const float fAmountPinned = m_Ped->GetPedIntelligence()->GetAmountPinnedDown();
	u8 iColour = s8(fAmountPinned*2.55f);
	ColorPrintLn(Color32(iColour,64,64), "%s!(%.2f)", fAmountPinned > CTaskInCover::ms_Tunables.m_PinnedDownTakeCoverAmount ? "Pinned" : "Not Pinned", fAmountPinned);

	// Display Battle Awareness + Action Mode time.
	if (m_Ped->IsLocalPlayer())
	{
		if (m_Ped->GetSpecialAbility())
		{
			ColorPrintLn(Color32(iColour,64,64), "Special Ability Charge (%.3f) Active: %s", m_Ped->GetSpecialAbility()->GetRemainingNormalized(), m_Ped->GetSpecialAbility()->IsActive() ? "T" : "F");
			ColorPrintLn(Color32(iColour,64,64), "Special Ability TimeScale (%.3f)", m_Ped->GetSpecialAbility()->GetInternalTimeWarp());
		}

		const float fBattleAwarenessTime = m_Ped->GetPedIntelligence()->GetBattleAwarenessSecs();
		u8 iColour = s8( fBattleAwarenessTime*255.0f);

		const CPedModelInfo::PersonalityMovementModes* pActionModes = m_Ped->GetPersonalityMovementMode();
		float fHighEnergyTime = 0.0f;
		if (m_Ped->IsBattleEventBlockingActionModeIdle())
		{
			u32 nTotalTime = pActionModes->GetLastBattleEventHighEnergyStartTimeMS();

			u32 nLastBattleTime = m_Ped->GetPedIntelligence()->GetLastBattleEventTime();
			u32 nCurrentTime = Min(fwTimer::GetTimeInMilliseconds() - nLastBattleTime, nTotalTime);

			float fRange = 1.0f - ((float)nCurrentTime/(float)nTotalTime);
			fHighEnergyTime = (fRange * (float)nTotalTime) * 0.001f;

			fHighEnergyTime = -fHighEnergyTime;
		}
		else if (pActionModes && m_Ped->GetPedIntelligence()->GetLastBattleEventTime() > 0)
		{
			u32 nTotalTime = pActionModes->GetLastBattleEventHighEnergyEndTimeMS()-pActionModes->GetLastBattleEventHighEnergyStartTimeMS();

			u32 nLastBattleTime = m_Ped->GetPedIntelligence()->GetLastBattleEventTime();
			u32 nCurrentTime = Clamp(fwTimer::GetTimeInMilliseconds() - nLastBattleTime - pActionModes->GetLastBattleEventHighEnergyStartTimeMS(), (u32)0, nTotalTime);

			float fRange = 1.0f - ((float)nCurrentTime/(float)nTotalTime);
			fHighEnergyTime = (fRange * (float)nTotalTime) * 0.001f;
		}

		ColorPrintLn(Color32(iColour,64,64), "Battle Awareness(%.1f) High Energy (%f)", fBattleAwarenessTime, fHighEnergyTime);
		float fActionModeTimer = m_Ped->GetActionModeTimer();
		ColorPrintLn(Color32(iColour,64,64), "Action Mode Timer(%.1f)", fActionModeTimer < 0.0f ? fActionModeTimer : (fBattleAwarenessTime + fActionModeTimer));
		ColorPrintLn(Color32(iColour,64,64), "Movement mode %s Override (%s:%d)", m_Ped->GetPersonalityMovementMode() ? m_Ped->GetPersonalityMovementMode()->GetName() : "", atNonFinalHashString(m_Ped->GetMovementModeOverrideHash()).TryGetCStr(), m_Ped->GetMovementModeOverrideHash());
		
		bool bWantsToUse = m_Ped->WantsToUseActionMode();
		bool bIsUsing = m_Ped->WantsToUseActionMode();
		bool bIdleActive = m_Ped->GetActionModeIdleActive();

		if(bWantsToUse || bIsUsing || bIdleActive)
		{
			ColorPrintLn(Color32(iColour,64,64), "WantsToUse: %d IsUsing: %d IdleActive: %d", bWantsToUse, bIsUsing, bIdleActive);
		}

		for(int i = 0; i < CPed::AME_Max; ++i)
		{
			if(m_Ped->IsActionModeReasonActive((CPed::ActionModeEnabled)i))
			{
				ColorPrintLn(Color32(iColour,64,64), "Action Mode Reason %s Active: (%d)", m_Ped->GetActionModeReasonString((CPed::ActionModeEnabled)i), m_Ped->GetActionModeReasonTime((CPed::ActionModeEnabled)i));
			}
		}
	}

	if (m_Ped->GetPlayerInfo())
	{
		ColorPrintLn(Color32(iColour,64,64), "Network Player Jack Rate(%.2f)", m_Ped->GetPlayerInfo()->GetJackRate());
		ColorPrintLn(Color32(iColour,64,64), "Weapon Damage Modifier(%.2f)", m_Ped->GetPlayerInfo()->GetPlayerWeaponDamageModifier());
	}
	else
	{
		ColorPrintLn(Color32(iColour,64,64), "Weapon Damage Modifier(%.2f)", m_Ped->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponDamageModifier));
	}

	if (m_Ped->GetWeaponManager())
	{
		if (m_Ped->GetPedIntelligence()->GetFiringPattern().GetFiringPatternInfo())
		{
			ColorPrintLn(Color32(64,255,64), "Firing Pattern Info : (%s)", m_Ped->GetPedIntelligence()->GetFiringPattern().GetFiringPatternInfo()->GetName());

			TUNE_GROUP_BOOL(FIRING_PATTERN_INFO, DISPLAY_FULL_INFO, false);
			if (!DISPLAY_FULL_INFO)
			{
				ColorPrintLn(Color32(64,255,64), "Enable DISPLAY_FULL_INFO in _TUNE_->FIRING_PATTERN_INFO for more info");
			}
			else
			{
				ColorPrintLn(Color32(64,255,64), "--------------------------");
				ColorPrintLn(Color32(64,255,64), "Num Bursts (%d)", m_Ped->GetPedIntelligence()->GetFiringPattern().GetFiringPatternInfo()->GetNumberOfBursts());
				ColorPrintLn(Color32(64,255,64), "Num Shots Per Burst Min/Max (%d/%d)", m_Ped->GetPedIntelligence()->GetFiringPattern().GetFiringPatternInfo()->m_NumberOfShotsPerBurstMin, m_Ped->GetPedIntelligence()->GetFiringPattern().GetFiringPatternInfo()->m_NumberOfShotsPerBurstMax);

				ColorPrintLn(Color32(64,255,64), "Time Before Firing (%.2f)", m_Ped->GetPedIntelligence()->GetFiringPattern().GetFiringPatternInfo()->GetTimeBeforeFiring());
				ColorPrintLn(Color32(64,255,64), "Time Between Bursts Min/Max (%.2f/%.2f)", m_Ped->GetPedIntelligence()->GetFiringPattern().GetFiringPatternInfo()->m_TimeBetweenBurstsMin, m_Ped->GetPedIntelligence()->GetFiringPattern().GetFiringPatternInfo()->m_TimeBetweenBurstsMax);
				ColorPrintLn(Color32(64,255,64), "Time Between Shots Min/Max (%.2f/%.2f)", m_Ped->GetPedIntelligence()->GetFiringPattern().GetFiringPatternInfo()->m_TimeBetweenShotsMin, m_Ped->GetPedIntelligence()->GetFiringPattern().GetFiringPatternInfo()->m_TimeBetweenShotsMax);
				ColorPrintLn(Color32(64,255,64), "Firing Pattern Data");
				ColorPrintLn(Color32(64,255,64), "--------------------------");
				ColorPrintLn(Color32(64,255,64), "Num Bursts Left (%d)", m_Ped->GetPedIntelligence()->GetFiringPattern().m_iNumberOfBursts);
				ColorPrintLn(Color32(64,255,64), "Num Shots Per Burst Left(%d)", m_Ped->GetPedIntelligence()->GetFiringPattern().m_iNumberOfShotsPerBurst);
				ColorPrintLn(Color32(64,255,64), "Time To Delay Firing (%.2f)", m_Ped->GetPedIntelligence()->GetFiringPattern().m_fTimeToDelayFiring);
				ColorPrintLn(Color32(64,255,64), "Time Till Next Shot (%.2f)", m_Ped->GetPedIntelligence()->GetFiringPattern().m_fTimeTillNextShot);
				ColorPrintLn(Color32(64,255,64), "Time Till Next Shot Modifier (%.2f)", m_Ped->GetPedIntelligence()->GetFiringPattern().m_fTimeTillNextShotModifier);
				ColorPrintLn(Color32(64,255,64), "Time Till Next Burst (%.2f)", m_Ped->GetPedIntelligence()->GetFiringPattern().m_fTimeTillNextBurst);
				ColorPrintLn(Color32(64,255,64), "Time Till Next Burst Modifier (%.2f)", m_Ped->GetPedIntelligence()->GetFiringPattern().m_fTimeTillNextBurstModifier);
				ColorPrintLn(Color32(64,255,64), "Unset Burst Cooldown Time (%.2f)", m_Ped->GetPedIntelligence()->GetFiringPattern().m_fBurstCooldownOnUnsetTimer);
				ColorPrintLn(Color32(64,255,64), "Unset Burst Cooldown Hash (%u)", m_Ped->GetPedIntelligence()->GetFiringPattern().m_uBurstCooldownOnUnsetPatternHash);
				ColorPrintLn(Color32(64,255,64), "--------------------------");
			}
		}

		sWeaponAccuracy accuracy;
		if (m_Ped->GetWeaponManager())
		{
			const CWeaponInfo* pEquippedWeaponInfo = m_Ped->GetWeaponManager()->GetEquippedWeaponInfo();
			if (pEquippedWeaponInfo)
			{
				float fDistance = pEquippedWeaponInfo->GetDamageFallOffRangeMin();
				if (m_Ped->GetPedIntelligence()->GetTargetting(false))
				{
					const CEntity* pTargetEntity = m_Ped->GetPedIntelligence()->GetTargetting()->GetCurrentTarget();
					if (pTargetEntity)
					{
						Vec3V vPedPos = m_Ped->GetTransform().GetPosition();
						Vec3V vTargetPos = pTargetEntity->GetTransform().GetPosition();
						ScalarV vDistance = Dist( vPedPos, vTargetPos );
						fDistance = vDistance.Getf();
					}
				}

				m_Ped->GetWeaponManager()->GetAccuracy().CalculateAccuracyRange(*m_Ped, *pEquippedWeaponInfo, fDistance, accuracy);
			}
		}

		ColorPrintLn(Color32(64,255,64), "Shooting Accuracy(%.2f), Rng(%.2f,%.2f), Mod(%.2f)", m_Ped->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponAccuracy), accuracy.fAccuracyMin, accuracy.fAccuracyMax, m_Ped->GetWeaponManager()->GetAccuracy().GetResetVars().GetAccuracyModifier());
	}
	
	ColorPrintLn(Color32(64,255,64), "Shooting Rate Mod(%.2f), Time Between Bursts Mod(%.2f)", m_Ped->GetPedIntelligence()->GetCombatBehaviour().GetShootRateModifier(), m_Ped->GetPedIntelligence()->GetFiringPattern().GetTimeTillNextBurstModifier());
	ColorPrintLn(Color32(64,255,64), parser_CCombatData__Movement_Strings[m_Ped->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement()]);
	ColorPrintLn(Color32(64,255,64), parser_CCombatData__TargetLossResponse_Strings[m_Ped->GetPedIntelligence()->GetCombatBehaviour().GetTargetLossResponse()]);
	ColorPrintLn(Color32(64,255,64), parser_CCombatData__Range_Strings[m_Ped->GetPedIntelligence()->GetCombatBehaviour().GetAttackRange()]);
	ColorPrintLn(Color32(64,255,64), "Time seen dead ped %u / current %u", m_Ped->GetPedIntelligence()->GetTimeLastSeenDeadPed(), fwTimer::GetTimeInMilliseconds());
	
	const CCombatBehaviour& combatBehaviour = m_Ped->GetPedIntelligence()->GetCombatBehaviour();

	for (s32 i = 0; i < CCombatData::BehaviourFlags_NUM_ENUMS; i++)
	{
		if (combatBehaviour.IsFlagSet((const CCombatData::BehaviourFlags)i)) 
		{ 
			ColorPrintLn(Color32(64,255,64), "%s", parser_CCombatData__BehaviourFlags_Strings[i]);
		}
	}
	
	// TODO: Convert flee flags to parser generated file
	const fwFlags32& fleeFlags = m_Ped->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags();
	if (fleeFlags.IsFlagSet(CFleeBehaviour::BF_CanUseVehicles)) 
	{
		ColorPrintLn(Color32(255,255,0), "BF_CanUseVehicles");
	}

	if (m_Ped->GetPedConfigFlag( CPED_CONFIG_FLAG_HasDeadPedBeenReported ))
	{
		ColorPrintLn(Color32(0,255,0), "%s", "Ped has been reported as dead");
	}

	const CItemInfo* pInfo = m_Ped->GetWeaponManager() ? CWeaponInfoManager::GetInfo(m_Ped->GetWeaponManager()->GetEquippedWeaponHash()) : NULL;

	if (pInfo)
	{
		ColorPrintLn(Color32(0,255,0), "Ped Equipped Weapon %s", pInfo->GetName());
		const CWeapon* pWeapon = m_Ped->GetWeaponManager()->GetEquippedWeapon();
		if (pWeapon)
		{
			ColorPrintLn(Color32(0,255,0), "Ammo Total: %u, Ammo In Clip: %u", pWeapon->GetAmmoTotal(), pWeapon->GetAmmoInClip());
		}
	}
	else if (m_Ped->GetWeaponManager() && m_Ped->GetWeaponManager()->GetPedEquippedWeapon()->GetWeaponInfo())
	{
		ColorPrintLn(Color32(0,255,0), "Ped Equipped Vehicle Weapon %s", m_Ped->GetWeaponManager()->GetPedEquippedWeapon()->GetWeaponInfo()->GetName());
	}

	if(m_Ped->GetWeaponManager())
	{
		ColorPrintLn(Color32(0,255,0), "Blanks Chance: %.2f, %.2f", m_Ped->GetWeaponManager()->GetChanceToFireBlanksMin(), m_Ped->GetWeaponManager()->GetChanceToFireBlanksMax());
	}

	ColorPrintLn(Color32(0,255,255), "Seeing range(%.2f)", m_Ped->GetPedIntelligence()->GetPedPerception().GetSeeingRange());
	ColorPrintLn(Color32(0,255,255), "Identification range(%.2f)", m_Ped->GetPedIntelligence()->GetPedPerception().GetIdentificationRange());
	ColorPrintLn(Color32(0,255,255), "Hearing range(%.2f)", m_Ped->GetPedIntelligence()->GetPedPerception().GetHearingRange());
	ColorPrintLn(Color32(0,255,255), "Windy Clothing(%.2f)", m_Ped->GetWindyClothingScale());

	// TODO: Move to task (no text) debug
	const CTask* pTask = m_Ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INVESTIGATE);
	if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_INVESTIGATE)
	{
		ColorPrintLn(Color_green, "%s", CEventNames::GetEventName(static_cast<const eEventType>(static_cast<const CTaskInvestigate*>(pTask)->GetInvestigationEventType())));
	}

	const CWeaponInfo* pEquippedWeaponInfo = m_Ped->GetWeaponManager() ? m_Ped->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
	if (pEquippedWeaponInfo && pEquippedWeaponInfo->GetIsHoming())
	{
		TUNE_GROUP_BOOL(HOMING_ROCKETS, DISPLAY_FULL_INFO, false);
		if (!DISPLAY_FULL_INFO)
		{
			ColorPrintLn(Color32(64,255,64), "Homing Rocket Override Info: Enable DISPLAY_FULL_INFO in _TUNE_->HOMING_ROCKETS");
		}
		else
		{
			ColorPrintLn(Color32(64,255,64), "Homing Rocket Override Info:");
			ColorPrintLn(Color32(64,255,64), "kAttribFloatHomingRocketBreakLockAngle: %f", m_Ped->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketBreakLockAngle));
			ColorPrintLn(Color32(64,255,64), "kAttribFloatHomingRocketBreakLockAngleClose: %f", m_Ped->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketBreakLockAngleClose));
			ColorPrintLn(Color32(64,255,64), "kAttribFloatHomingRocketBreakLockCloseDistance: %f", m_Ped->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketBreakLockCloseDistance));
			ColorPrintLn(Color32(64,255,64), "kAttribFloatHomingRocketTurnRateModifier: %f", m_Ped->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketTurnRateModifier));
		}
	}
}

#endif // AI_DEBUG_OUTPUT_ENABLED