#include "ai\debug\types\PedDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "task\Combat\Cover\TaskCover.h"
#include "Task/Movement/TaskParachute.h"

EXTERN_PARSER_ENUM(ePedConfigFlags);
EXTERN_PARSER_ENUM(ePedResetFlags);

CPedFlagsDebugInfo::CPedFlagsDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CPedFlagsDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("PED FLAGS");
		PushIndent();
			PushIndent();
				PushIndent();
					WriteHeader("CONFIG FLAGS");
						PushIndent();
							PrintConfigFlags();
						PopIndent();
					WriteHeader("RESET FLAGS");
						PushIndent();
							PrintResetFlags();
						PopIndent();

					PrintCoverFlags();	// TODO: Move to task (no text) debug
}

bool CPedFlagsDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	return true;
}

void CPedFlagsDebugInfo::PrintConfigFlags()
{
	const bool bStripPrefix = true;
	for (s32 i=0; i<ePedConfigFlags_NUM_ENUMS; i++)
	{
		if (m_Ped->GetPedConfigFlag((const ePedConfigFlags)i)) 
		{ 
			WritePedConfigFlag(*m_Ped, (ePedConfigFlags)i, bStripPrefix);
		}
	}
}

void CPedFlagsDebugInfo::PrintResetFlags()
{
	const bool bStripPrefix = true;
	for (s32 i=0; i<ePedResetFlags_NUM_ENUMS; i++)
	{
		if (m_Ped->GetPedResetFlag((const ePedResetFlags)i)) 
		{
			WritePedResetFlag(*m_Ped, (ePedResetFlags)i, bStripPrefix);
		}
	}
}

void CPedFlagsDebugInfo::PrintCoverFlags()
{
	const CTaskCover* pCoverTask = static_cast<const CTaskCover*>(m_Ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
	if (!pCoverTask)
		return;

	WriteHeader("COVER FLAGS");
		PushIndent();
			WriteBoolValueTrueIsBad("CTaskCover::CF_TooHighCoverPoint", pCoverTask->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint));
			WriteBoolValueTrueIsBad("CTaskCover::CF_FacingLeft", pCoverTask->IsCoverFlagSet(CTaskCover::CF_FacingLeft));
			WriteBoolValueTrueIsBad("CTaskCover::CF_DesiredFacingLeft", pCoverTask->IsCoverFlagSet(CTaskCover::CF_DesiredFacingLeft));
			WriteBoolValueTrueIsBad("CTaskCover::CF_EnterLeft", pCoverTask->IsCoverFlagSet(CTaskCover::CF_EnterLeft));
			WriteBoolValueTrueIsBad("CTaskCover::CF_SpecifyInitialHeading", pCoverTask->IsCoverFlagSet(CTaskCover::CF_SpecifyInitialHeading));
			WriteBoolValueTrueIsBad("CTaskCover::CF_SkipIdleCoverTransition", pCoverTask->IsCoverFlagSet(CTaskCover::CF_SkipIdleCoverTransition));
			WriteBoolValueTrueIsBad("CTaskCover::CF_StrafeToCover", pCoverTask->IsCoverFlagSet(CTaskCover::CF_StrafeToCover));
			WriteBoolValueTrueIsBad("CTaskCover::CF_AimDirectly", pCoverTask->IsCoverFlagSet(CTaskCover::CF_AimDirectly));
			WriteBoolValueTrueIsBad("CTaskCover::CF_AtCorner", pCoverTask->IsCoverFlagSet(CTaskCover::CF_AtCorner));
			WriteBoolValueTrueIsBad("CTaskCover::CF_CloseToPossibleCorner", pCoverTask->IsCoverFlagSet(CTaskCover::CF_CloseToPossibleCorner));

			CCoverPoint* pCoverPoint = NULL;
			const s32 iScriptedCoverIndex = pCoverTask->GetScriptedCoverIndex();
			if (iScriptedCoverIndex != INVALID_COVER_POINT_INDEX)
			{
				pCoverPoint = CCover::FindCoverPointWithIndex(iScriptedCoverIndex);
			}
			PrintLn("Scripted Cover Index : %i, Address : 0x%p", iScriptedCoverIndex, pCoverPoint);
		PopIndent();
}


CPedNamesDebugInfo::CPedNamesDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CPedNamesDebugInfo::Print()
{
	PedNamePrint();
	ResetIndent();

	PedModelPrint();
	ResetIndent();
}

void CPedNamesDebugInfo::PedModelPrint()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("MODEL INFORMATION");
	PushIndent(3);
	PrintModelInfo();
}

void CPedNamesDebugInfo::PedNamePrint()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("PED NAMES");
	PushIndent(3);
	PrintNames();
}

bool CPedNamesDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;



	return true;
}

void CPedNamesDebugInfo::PrintModelInfo()
{
	Color32 colour = CRGBA(255,192,96, m_PrintParams.Alpha);
	ColorPrintLn(colour, "%s", m_Ped->GetBaseModelInfo()->GetModelName());
}

void CPedNamesDebugInfo::PrintNames()
{
	Color32 colour = CRGBA(255,192,96, m_PrintParams.Alpha);
	const char* pDebugName = m_Ped->m_debugPedName;
	if ( pDebugName && StringLength(pDebugName) > 0 )
	{
		ColorPrintLn(colour,"Debug: %s",pDebugName);
	}

	extern bool bShowMissingLowLOD;	// in ped.cpp
	if (bShowMissingLowLOD)
	{
		ColorPrintLn(Color_white, "<No Low LOD>");
	}

	ColorPrintLn(colour, "Model: %s", m_Ped->GetBaseModelInfo()->GetModelName());	
	ColorPrintLn(colour, "Log: %s", m_Ped->GetDebugName());
}


CPedGroupTextDebugInfo::CPedGroupTextDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}


void CPedGroupTextDebugInfo::Print()
{
	GroupPrint();
}

void CPedGroupTextDebugInfo::GroupPrint()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("PED GROUP");
	PushIndent(3);
	PrintGroupText();
}

bool CPedGroupTextDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	return true;
}

void CPedGroupTextDebugInfo::PrintGroupText()
{
	const CPed* pPed = m_Ped;
	s32 iPedGroupIndex = pPed->GetPedGroupIndex();
	ColorPrintLn(Color_blue, "Ped Group Index: %i", iPedGroupIndex);

	CPedGroup* pGroup = pPed->GetPedsGroup();
	if (pGroup)
	{
		const CPed* pLeader = pGroup->GetGroupMembership()->GetLeader();
		if(pLeader)
		{
			ColorPrintLn(Color_blue, "Ped(0x%p): %s, %s", pPed, AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed));
			ColorPrintLn(pPed == pLeader ? Color_red : Color_blue, "Leader(0x%p): %s, %s", pLeader, AILogging::GetDynamicEntityIsCloneStringSafe(pLeader), AILogging::GetDynamicEntityNameSafe(pLeader));
		}
	
		s32 iNumMembers = pGroup->GetGroupMembership()->CountMembers();
		ColorPrintLn(Color_blue, "Number in group: %i", iNumMembers);
	
		if (pGroup->GetFormation())
		{
			ColorPrintLn(Color_blue, "Formation: %s",  pGroup->GetFormation()->GetFormationTypeName());
		}
	}
}


CStealthDebugInfo::CStealthDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_pPed(pPed)
{

}

void CStealthDebugInfo::Print()
{
	if (!m_pPed)
	{
		return;
	}

	m_PrintParams.Alpha = 255;
	WriteLogEvent("STEALTH");
	PushIndent(3);
	PrintDebug();
	PopIndent(3);
}

void CStealthDebugInfo::PrintDebug()
{
	const bool bIsAPlayerPed = m_pPed->IsPlayer();
	ColorPrintLn(Color_orange, "%s: %s", bIsAPlayerPed ? "PLAYER" : "PED", AILogging::GetEntityDescription(m_pPed).c_str());
	PushIndent(2);
	PrintPedPhysicsInfo();
	PrintPedMotionInfo();
	PrintPedStealthInfo();
	PrintPedPerceptionInfo();
	if (!bIsAPlayerPed)
	{
		PrintTargettingInfo();
	}
	PopIndent(2);
}

void CStealthDebugInfo::PrintPedPhysicsInfo()
{
	phMaterialMgr::Id groundId = m_pPed->GetPackedGroundMaterialId();

	const phMaterial& groundMat = PGTAMATERIALMGR->GetMaterial(groundId);
	ColorPrintLn(Color_WhiteSmoke, "Ground material: %s", groundMat.GetName());
	ColorPrintLn(Color_WhiteSmoke, "Ground noisiness %.2f", PGTAMATERIALMGR->GetMtlNoisiness(groundId));

	const bool bIsStanding = m_pPed->GetIsStanding();
	ColorPrintLn(bIsStanding ? Color_LimeGreen : Color_WhiteSmoke, "Standing (Physics): %s", bIsStanding ? "Y" : "N");
}

void CStealthDebugInfo::PrintPedMotionInfo()
{
	if (const CPedMotionData* pMotion = m_pPed->GetMotionData())
	{
		const bool bUsingStealth = pMotion->GetUsingStealth();
		ColorPrintLn(bUsingStealth ? Color_LimeGreen : Color_tomato, "Using Stealth: %s", bUsingStealth ? "Y" : "N");

		const bool bIsUsingStealthInFPS = pMotion->GetIsUsingStealthInFPS();
		ColorPrintLn(bIsUsingStealthInFPS ? Color_LimeGreen : Color_tomato, "Is Using Stealth in FPS: %s", bIsUsingStealthInFPS ? "Y" : "N");

		const bool bWasUsingStealthInFPS = pMotion->GetWasUsingStealthInFPS();
		ColorPrintLn(bWasUsingStealthInFPS ? Color_LimeGreen : Color_tomato, "Was Using Stealth in FPS: %s", bWasUsingStealthInFPS ? "Y" : "N");

		const bool bStealthed = pMotion->GetUsingStealth();
		ColorPrintLn(bStealthed ? Color_LimeGreen : Color_tomato, "Stealthed: %s", bStealthed ? "Y" : "N");

		const bool bUsingCrouchMode = pMotion->GetIsCrouching();
		ColorPrintLn(bUsingCrouchMode ? Color_LimeGreen : Color_tomato, "Is Using Crouch Mode: %s", bUsingCrouchMode ? "Y" : "N");
	}
}

void CStealthDebugInfo::PrintPedPerceptionInfo()
{
	ColorPrintLn(Color_WhiteSmoke, "PED HEARING:");
	PushIndent(2);
	if (CPedIntelligence* pIntelligence = m_pPed->GetPedIntelligence())
	{
		ColorPrintLn(Color_WhiteSmoke, "(Combat float)kAttribFloatMaxDistanceToHearEvents: %f", pIntelligence->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxDistanceToHearEvents));

		const float fHearingRange = pIntelligence->GetPedPerception().GetHearingRange();
		ColorPrintLn(Color_WhiteSmoke, "Hearing Range: %f", fHearingRange);
	}

	PopIndent(2);
}

void CStealthDebugInfo::PrintPedStealthInfo()
{
	const CPlayerInfo* rPlayerInfo = m_pPed->GetPlayerInfo();
	if(!rPlayerInfo)
	{
		return;
	}
	
	const float fStealthNoise = rPlayerInfo->GetStealthNoise();
	ColorPrintLn(fStealthNoise > 0.0f ? Color_tomato : Color_LimeGreen, "Stealth Noise: %.2f", fStealthNoise);
	const float fStealthMultiplier = rPlayerInfo->GetStealthMultiplier();
	ColorPrintLn(Color_WhiteSmoke, "Stealth multiplier: %.2f", fStealthMultiplier);

	PushIndent(2);
	ColorPrintLn(Color_WhiteSmoke, "Normal multiplier: %.2f", rPlayerInfo->GetNormalStealthMultiplier());
	ColorPrintLn(Color_WhiteSmoke, "Sneaking multiplier: %.2f", rPlayerInfo->GetSneakingStealthMultiplier());
	PopIndent(2);
	
	if (const CPedIntelligence* pIntelligence = m_pPed->GetPedIntelligence())
	{
		PrintPedStealthFlags(pIntelligence->GetPedStealth());
	}

	PrintOtherStealthFlags(*m_pPed);
}

void CStealthDebugInfo::PrintTargettingInfo()
{
	CPedTargetting* pPedTargetting = m_pPed->GetPedIntelligence()->GetTargetting(false);
	ColorPrintLn(pPedTargetting ? Color_tomato : Color_WhiteSmoke, "HAS TARGET: %s", pPedTargetting ? "Y" : "N");
	if (pPedTargetting)
	{
		PushIndent(2);
		Vector3 vLastSeenPosition(0.0f,0.0f,0.0f);
		float fLastSeenTimer = 0.0f;
		const CEntity* pCurrentTarget			= pPedTargetting->GetCurrentTarget();
		const bool bAreTargetsWhereaboutsKnown	= pPedTargetting->AreTargetsWhereaboutsKnown(&vLastSeenPosition, pCurrentTarget, &fLastSeenTimer);

		ColorPrintLn(Color_WhiteSmoke, "Current Target: %s", AILogging::GetEntityDescription(pCurrentTarget).c_str());
		ColorPrintLn(bAreTargetsWhereaboutsKnown ? Color_LimeGreen : Color_tomato, "Are target whereabouts known: %s", bAreTargetsWhereaboutsKnown ? "Y" : "N");
		ColorPrintLn(Color_WhiteSmoke, "Last seen position: %.2f, %.2f, %.2f", vLastSeenPosition.x, vLastSeenPosition.y, vLastSeenPosition.z);
		ColorPrintLn(Color_WhiteSmoke, "Last seen timer: %.2f", fLastSeenTimer);
		PopIndent(2);
	}
}

void CStealthDebugInfo::PrintPedStealthFlags(const CPedStealth& rPedStealth)
{
	const fwFlags32& flags = rPedStealth.GetFlags();

	ColorPrintLn(Color_WhiteSmoke, "PED STEALTH FLAGS:");
	PushIndent(2);
	ColorPrintLn(flags.IsFlagSet(CPedStealth::SF_PedGeneratesFootStepEvents) ? Color_LimeGreen : Color_tomato,
		"PedGeneratesFootStepEvents: %s",
		flags.IsFlagSet(CPedStealth::SF_PedGeneratesFootStepEvents) ? "Y" : "N");

	ColorPrintLn(flags.IsFlagSet(CPedStealth::SF_PedGeneratesFootStepEvents) ? Color_LimeGreen : Color_tomato,
		"TreatedAsActingSuspiciously: %s",
		flags.IsFlagSet(CPedStealth::SF_PedGeneratesFootStepEvents) ? "Y" : "N");
	PopIndent(2);
}

void CStealthDebugInfo::PrintOtherStealthFlags( const CPed& rPed )
{
	ColorPrintLn(Color_WhiteSmoke, "OTHER STEALTH FLAGS:");
	PushIndent(2);
	const bool bCheckLosForSoundEvents = rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_CheckLoSForSoundEvents);
	ColorPrintLn(bCheckLosForSoundEvents ? Color_LimeGreen : Color_tomato, "CheckLoSForSoundEvents: %s", bCheckLosForSoundEvents ? "Y" : "N");

	const bool bListensToSoundEvents = rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_ListensToSoundEvents);
	ColorPrintLn(bListensToSoundEvents ? Color_LimeGreen : Color_tomato, "ListensToSoundEvents: %s", bListensToSoundEvents ? "Y" : "N");

	const bool bKilledByStealth = rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth);
	ColorPrintLn(bKilledByStealth ? Color_LimeGreen : Color_tomato, "KilledByStealth: %s", bKilledByStealth ? "Y" : "N");

	
	const bool bNeverLoseTarget = rPed.GetPedIntelligence()->GetCombatBehaviour().GetTargetLossResponse() == CCombatData::TLR_NeverLoseTarget;
	ColorPrintLn(bNeverLoseTarget ? Color_LimeGreen : Color_tomato, "(COMBAT) NeverLoseTarget: %s", bNeverLoseTarget ? "Y" : "N");
	PopIndent(2);
}



void CMotivationDebugInfo::Print()
{
	MotivationPrint();
	ResetIndent();
}

void CMotivationDebugInfo::MotivationPrint()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("MOTIVATION");
	PushIndent(3);
	PrintMotivation();
	PopIndent(3);
}

bool CMotivationDebugInfo::ValidateInput()
{
	if (!m_Ped || !m_Ped->GetPedIntelligence())
		return false;

	return true;
}

void CMotivationDebugInfo::PrintMotivation()
{
	const CPedIntelligence* pPedIntelligence = m_Ped->GetPedIntelligence();

	ColorPrintLn(Color_orange, "Ped Motivation:");

	ColorPrintLn(Color_white, "Fear: %f Modify: %f",pPedIntelligence->GetPedMotivation().GetMotivation(CPedMotivation::FEAR_STATE),
		pPedIntelligence->GetPedMotivation().GetMotivationRateModifier(CPedMotivation::FEAR_STATE));

	ColorPrintLn(Color_white, "Anger: %f Modify: %f", pPedIntelligence->GetPedMotivation().GetMotivation(CPedMotivation::ANGRY_STATE),
		pPedIntelligence->GetPedMotivation().GetMotivationRateModifier(CPedMotivation::ANGRY_STATE));
}



CPedPersonalitiesDebugInfo::CPedPersonalitiesDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CPedPersonalitiesDebugInfo::Print()
{
	PersonalitiesPrint();
}

void CPedPersonalitiesDebugInfo::PersonalitiesPrint()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("PERSONALITIES");
	PushIndent();
	PushIndent();
	PushIndent();
	PrintPersonalities();
}

bool CPedPersonalitiesDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	return true;
}

void CPedPersonalitiesDebugInfo::PrintPersonalities()
{
	const CPedModelInfo* mi = m_Ped->GetPedModelInfo();
	Assert(mi);

	const CPedModelInfo::PersonalityData& pd = mi->GetPersonalitySettings();

	ColorPrintLn(Color_yellow, "Personality Data:");
	PushIndent();
	ColorPrintLn(Color_green, "Personality: %s", pd.GetPersonalityName());
	ColorPrintLn(Color_green, "Weapon Loadout: %s", pd.GetDefaultWeaponLoadoutName());
	ColorPrintLn(Color_green, "Witness personality: %s", pd.GetWitnessPersonality().GetCStr());
	ColorPrintLn(Color_green, "Attack strength: %.2f [%.2f, %.2f]", pd.GetAttackStrength(m_Ped->GetRandomSeed()), pd.GetAttackStrengthMin(), pd.GetAttackStrengthMax());
	ColorPrintLn(Color_green, "Stamina Efficiency: %.2f", pd.GetStaminaEfficiency());
	ColorPrintLn(Color_green, "Motivation: %d [%d, %d]", pd.GetMotivation(m_Ped->GetRandomSeed()), pd.GetMotivationMin(), pd.GetMotivationMax());
	ColorPrintLn(Color_green, "Age: %d", mi->GetAge());
	ColorPrintLn(Color_green, "Sexiness: %d (flags)", mi->GetSexinessFlags());
	ColorPrintLn(Color_green, "Bravery: (%s) %d (flags)", pd.GetBraveryName(), pd.GetBraveryFlags());
	ColorPrintLn(Color_green, "Agility: %d (flags)", pd.GetAgilityFlags());
	ColorPrintLn(Color_green, "Crime: %d (flags)", pd.GetCriminalityFlags());
	ColorPrintLn(Color_green, "Driving ability: %d [%d, %d]", pd.GetDrivingAbility(m_Ped->GetRandomSeed()), pd.GetDrivingAbilityMin(), pd.GetDrivingAbilityMax());
	ColorPrintLn(Color_green, "Driving aggressiveness: %d [%d, %d]", pd.GetDrivingAggressiveness(m_Ped->GetRandomSeed()), pd.GetDrivingAggressivenessMin(), pd.GetDrivingAggressivenessMax());
	ColorPrintLn(Color_green, "Default model gender: %s", pd.GetIsMale() ? "MALE" : "FEMALE");

	char debugText[128];

	sprintf(debugText, "Drives car types:");
	if (pd.DrivesVehicleType(PED_DRIVES_POOR_CAR))	
		safecat(debugText, " poor");
	if (pd.DrivesVehicleType(PED_DRIVES_AVERAGE_CAR))	
		safecat(debugText, " average");
	if (pd.DrivesVehicleType(PED_DRIVES_RICH_CAR))	
		safecat(debugText, " rich");
	if (pd.DrivesVehicleType(PED_DRIVES_BIG_CAR))	
		safecat(debugText, " big");
	if (pd.DrivesVehicleType(PED_DRIVES_MOTORCYCLE))	
		safecat(debugText, " motorcycles");
	if (pd.DrivesVehicleType(PED_DRIVES_BOAT))	
		safecat(debugText, " boats");
	ColorPrintLn(Color_green, "%s", debugText);

	Affluence a = pd.GetAffluence();
	sprintf(debugText, "Affluence:");
	switch(a) {
	case AFF_POOR:
		safecat(debugText, " poor");
		break;
	case AFF_AVERAGE:
		safecat(debugText, " average");
		break;
	case AFF_RICH:
		safecat(debugText, " rich");
		break;
	default:
		safecat(debugText, " NOT SET!");
		break;
	}
	ColorPrintLn(Color_green, "%s", debugText);

	TechSavvy t = pd.GetTechSavvy();
	sprintf(debugText, "TechSavvy:");
	switch(t) {
	case TS_LOW:
		safecat(debugText, " low");
		break;
	case TS_HIGH:
		safecat(debugText, " high");
		break;
	default:
		safecat(debugText, " NOT SET!");
		break;
	}
	ColorPrintLn(Color_green, "%s", debugText);

	PopIndent();
	ColorPrintLn(Color_yellow, "Model Info:");
	PushIndent();
	ColorPrintLn(Color_blue, "Weapon Loadout: %s", pd.GetDefaultWeaponLoadoutName());
	ColorPrintLn(Color_blue, "Sexiness: %d (flags)", mi->GetSexinessFlags());
	PopIndent();
	ColorPrintLn(Color_yellow, "Ped:");
	PushIndent();
	ColorPrintLn(Color_purple, "Ped gender: %s", pd.GetIsMale() ? "Male" : "Female");

}



CPedPopulationTypeDebugInfo::CPedPopulationTypeDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}


void CPedPopulationTypeDebugInfo::Print()
{
	TypePrint();
	ResetIndent();
}

void CPedPopulationTypeDebugInfo::TypePrint()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("POPULATION TYPE");
	PushIndent(3);
	PrintPopulationText();
}

bool CPedPopulationTypeDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	return true;
}

void CPedPopulationTypeDebugInfo::PrintPopulationText()
{
	const char* cPopInfoString;
	// Get a nice string representation of the ped's pop info.
	switch(m_Ped->PopTypeGet())
	{
	case POPTYPE_MISSION:
		{
			cPopInfoString = "Mission";
			break;
		}
	case POPTYPE_RANDOM_AMBIENT:
		{
			cPopInfoString = "Ambient";
			break;
		}
	case POPTYPE_RANDOM_SCENARIO:
		{
			cPopInfoString = "Scenario";
			break;
		}
	case POPTYPE_RANDOM_PERMANENT:
		{
			cPopInfoString = "Persistence";
			break;
		}
	case POPTYPE_PERMANENT:
		{
			cPopInfoString = "Player / Player Groups";
			break;
		}
	default:
		{
			cPopInfoString = "Unknown type";
			break;
		}
	}

	ColorPrintLn(Color_blue, "Population type: %s %s", cPopInfoString, m_Ped->GetIsInExterior() ? "Exterior" : "Interior");
}



CPedGestureDebugInfo::CPedGestureDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CPedGestureDebugInfo::Print()
{
	if(!ValidateInput())
	{
		return;
	}

	WriteLogEvent("GESTURE");
	PushIndent(3);
	PrintGestureText();
}

bool CPedGestureDebugInfo::ValidateInput()
{
	if (!m_Ped)
	{
		return false;
	}

	return true;
}

void CPedGestureDebugInfo::PrintGestureText()
{
	if (m_Ped->IsPlayingAGestureAnim())
	{
		atString clip; 
		clip = m_Ped->GetGestureClip()->GetName();
		clip.Replace("pack:/", "");
		clip.Replace(".clip", "");

		ColorPrintLn(Color_green, "Gesture:%s\\%s", m_Ped->GetGestureClipSet().TryGetCStr(), clip.c_str());
	}
	else
	{
		if(m_Ped->BlockGestures(BANK_ONLY(false)))
		{
			ColorPrintLn(Color_red, "Gesture:%s", m_Ped->GetGestureBlockReason());
		}
		else
		{
			ColorPrintLn(Color_green, "Gesture:ALLOWED");
		}
	}

}



CPedTaskHistoryDebugInfo::CPedTaskHistoryDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CPedTaskHistoryDebugInfo::Print()
{
	if(!ValidateInput())
	{
		return;
	}

	WriteLogEvent("TASK HISTORY");
	PushIndent(3);
	PrintTaskHistory();
}

bool CPedTaskHistoryDebugInfo::ValidateInput()
{
	if (!m_Ped)
	{
		return false;
	}

	return true;
}

void CPedTaskHistoryDebugInfo::PrintTaskHistory()
{
	for( s32 i = 0; i < CTaskTree::MAX_TASK_HISTORY; i++ )
	{
		if( m_Ped->GetPedIntelligence()->GetTaskManager()->GetHistory(PED_TASK_TREE_PRIMARY, i) != CTaskTypes::TASK_NONE )
		{
			ColorPrintLn(Color_green, TASKCLASSINFOMGR.GetTaskName(m_Ped->GetPedIntelligence()->GetTaskManager()->GetHistory(PED_TASK_TREE_PRIMARY, i)));
		}
	}
}



CPlayerDebugInfo::CPlayerDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CPlayerDebugInfo::Print()
{
	if(!ValidateInput())
	{
		return;
	}

	WriteLogEvent("PLAYER INFO");
	PushIndent(3);
	PrintPlayerInfo();
}

bool CPlayerDebugInfo::ValidateInput()
{
	if (!m_Ped)
	{
		return false;
	}

	return true;
}

void CPlayerDebugInfo::PrintPlayerInfo()
{
	if(m_Ped->IsLocalPlayer())
	{
		ColorPrintLn(Color_green, "Auto give parachute when enter plane: %s", m_Ped->GetPlayerInfo()->GetAutoGiveParachuteWhenEnterPlane() ? "yes" : "no");
		ColorPrintLn(Color_green, "Auto give scuba gear when exit vehicle: %s", m_Ped->GetPlayerInfo()->GetAutoGiveScubaGearWhenExitVehicle() ? "yes" : "no");
		ColorPrintLn(Color_green, "Disable agitation: %s", m_Ped->GetPlayerInfo()->GetDisableAgitation() ? "yes" : "no");
		ColorPrintLn(Color_green, "Recharge Script Multiplier: %.2f", m_Ped->GetPlayerInfo()->GetHealthRecharge().GetRechargeScriptMultiplier());
	}
}



CParachuteDebugInfo::CParachuteDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CParachuteDebugInfo::Print()
{
	if(!ValidateInput())
	{
		return;
	}

	WriteLogEvent("PARACHUTE INFO");
	PushIndent(3);
	PrintParachuteInfo();
}

bool CParachuteDebugInfo::ValidateInput()
{
	if (!m_Ped)
	{
		return false;
	}

	return true;
}

void CParachuteDebugInfo::PrintParachuteInfo()
{
	//Ensure the ped's inventory is valid.
	const CPedInventory* pPedInventory = m_Ped->GetInventory();
	if(!pPedInventory)
	{
		return;
	}

	ColorPrintLn(Color_blue, "Has Parachute?: %s", pPedInventory->GetWeapon(GADGETTYPE_PARACHUTE) ? "true" : "false");
	ColorPrintLn(Color_blue, "Has Reserve?: %s", m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute) ? "true" : "false");

	//! PACK.
	ColorPrintLn(Color_blue, "Pack:");
	PushIndent();
	ColorPrintLn(Color_blue, "Current Parachute Pack Tint: %d", CTaskParachute::GetTintIndexForParachutePack(*m_Ped));
	ColorPrintLn(Color_blue, "Parachute Pack Model ID: %s (%d)", CTaskParachute::GetModelForParachutePack(m_Ped).TryGetCStr() ? CTaskParachute::GetModelForParachutePack(m_Ped).TryGetCStr() : "none", CTaskParachute::GetModelForParachutePack(m_Ped).GetHash() );

	const CTaskParachute::PedVariation* pVariation = CTaskParachute::FindParachutePackVariationForPed(*m_Ped);
	if(pVariation)
	{
		ColorPrintLn(Color_blue, "Parachute Pack Variation. Comp: %d, Draw: %d, AltDraw: %d, Tex: %d", (int)pVariation->m_Component, pVariation->m_DrawableId, pVariation->m_DrawableAltId, pVariation->m_TexId);
	}

	if(m_Ped->GetPlayerInfo())
	{
		if(m_Ped->GetPlayerInfo()->GetPedParachuteVariationComponentOverride() != PV_COMP_INVALID)
		{
			ColorPrintLn(Color_blue, "Script Parachute Pack Variation. Comp: %d, Draw: %d, AltDraw: %d, Tex: %d", (int)m_Ped->GetPlayerInfo()->GetPedParachuteVariationComponentOverride(),
				m_Ped->GetPlayerInfo()->GetPedParachuteVariationDrawableOverride(), 
				m_Ped->GetPlayerInfo()->GetPedParachuteVariationAltDrawableOverride(), 
				m_Ped->GetPlayerInfo()->GetPedParachuteVariationTexIdOverride() );
		}

		if(m_Ped->GetPlayerInfo()->GetPreviousVariationComponent() != PV_COMP_INVALID)
		{
			u32				PreviousDrawableId;
			u32				PreviousDrawableAltId;
			u32				PreviousTexId;
			u32				PreviousVariationPaletteId;
			m_Ped->GetPlayerInfo()->GetPreviousVariationInfo(PreviousDrawableId, PreviousDrawableAltId, PreviousTexId, PreviousVariationPaletteId);

			ColorPrintLn(Color_blue, "Previous Ped Variation. Comp: %d, Draw: %d, AltDraw: %d, Tex: %d", (int)m_Ped->GetPlayerInfo()->GetPreviousVariationComponent(),
				PreviousDrawableId, 
				PreviousDrawableAltId, 
				PreviousTexId );
		}
	}

	ColorPrintLn(Color_blue, "Parachute Pack Variation Active: %s", CTaskParachute::IsParachutePackVariationActiveForPed(*m_Ped) ? "true" : "false"  );
	PopIndent();


	//! PARACHUTE.
	ColorPrintLn(Color_blue, "Parachute:");
	PushIndent();
	ColorPrintLn(Color_blue, "Current Parachute Tint: %d", CTaskParachute::GetTintIndexForParachute(*m_Ped) );
	ColorPrintLn(Color_blue, "Parachute Model ID: %s (%d)", CTaskParachute::GetModelForParachute(m_Ped).TryGetCStr() ? CTaskParachute::GetModelForParachute(m_Ped).TryGetCStr() : "none", CTaskParachute::GetModelForParachute(m_Ped).GetHash()  );
	PopIndent();

	//! RAW - ped intelligence info overrides.
	if(m_Ped->GetPedIntelligence())
	{
		ColorPrintLn(Color_blue, "Raw Parachute Pack Tint Override: %d", m_Ped->GetPedIntelligence()->GetTintIndexForParachutePack() );
		ColorPrintLn(Color_blue, "Raw Parachute Tint Override: %d", m_Ped->GetPedIntelligence()->GetTintIndexForParachute() );
		ColorPrintLn(Color_blue, "Raw Reserve Parachute Tint Override: %d", m_Ped->GetPedIntelligence()->GetTintIndexForReserveParachute() );
	}
}

#endif // AI_DEBUG_OUTPUT_ENABLED