// File header
#include "Task/Scenario/Info/ScenarioInfoConditions.h"

// Rage headers
#include "fwanimation/animmanager.h"
#include "fwanimation/clipsets.h"
#include "math/angmath.h"

// Game headers
#include "ai/ambient/AmbientModelSetManager.h"
#include "Animation/Move.h"
#include "Animation/MoveVehicle.h"
#include "Audio/GameObjects.h"
#include "Audio/speechmanager.h"
#include "Audio/VehicleAudioEntity.h"
#include "Game/Clock.h"
#include "Game/Weather.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Scene/World/GameWorld.h"
#include "Task/Default//AmbientAnimationManager.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Helpers/TaskConversationHelper.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Scenario/Types/TaskVehicleScenario.h"
#include "Task/System/TaskTypes.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "VehicleAi/Task/TaskVehicleAnimation.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

INSTANTIATE_RTTI_CLASS(CScenarioCondition,0x8F73CD12);
INSTANTIATE_RTTI_CLASS(CScenarioConditionSet,0x96995688);
INSTANTIATE_RTTI_CLASS(CScenarioConditionSetOr,0x72DE3A08);
INSTANTIATE_RTTI_CLASS(CScenarioConditionWorld,0x3F50C8F0);
INSTANTIATE_RTTI_CLASS(CScenarioConditionPopulation,0x2944F237);
INSTANTIATE_RTTI_CLASS(CScenarioConditionWorldSet,0xBB40C33E);
INSTANTIATE_RTTI_CLASS(CScenarioConditionPlayingAnim,0x41F9D2F0);
INSTANTIATE_RTTI_CLASS(CScenarioConditionModel,0x994DF669);
INSTANTIATE_RTTI_CLASS(CScenarioConditionSpeed,0xFBC8A654);
INSTANTIATE_RTTI_CLASS(CScenarioConditionCrouched,0xF01F9F53);
INSTANTIATE_RTTI_CLASS(CScenarioConditionInCover,0x3D0FD7A4);
INSTANTIATE_RTTI_CLASS(CScenarioConditionHealth,0x93B2D11);
INSTANTIATE_RTTI_CLASS(CScenarioConditionEquippedWeapon,0x41E2E975);
INSTANTIATE_RTTI_CLASS(CScenarioConditionWet,0xF812CAB0);
INSTANTIATE_RTTI_CLASS(CScenarioConditionOutOfBreath,0xA2E200B7);
INSTANTIATE_RTTI_CLASS(CScenarioConditionJustGotUp,0xa7513fcc);
INSTANTIATE_RTTI_CLASS(CScenarioConditionAlert,0x14DAE9DA);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsPlayer,0x114E52A0);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsPlayerInMultiplayerGame,0xB2E1973A);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsPlayerTired,0xA1848310);
INSTANTIATE_RTTI_CLASS(CScenarioConditionDistanceToPlayer,0x4C5A4848);
INSTANTIATE_RTTI_CLASS(CScenarioConditionRaining,0x498E22E2);
INSTANTIATE_RTTI_CLASS(CScenarioConditionSunny,0xECAC1683);
INSTANTIATE_RTTI_CLASS(CScenarioConditionSnowing,0x28366E49);
INSTANTIATE_RTTI_CLASS(CScenarioConditionWindy,0xB4E1B26B);
INSTANTIATE_RTTI_CLASS(CScenarioConditionTime,0xB3C295EA);
INSTANTIATE_RTTI_CLASS(CScenarioConditionInInterior,0xDCEEA6D7);
INSTANTIATE_RTTI_CLASS(CScenarioConditionHasProp,0x84A73CB3);
INSTANTIATE_RTTI_CLASS(CScenarioConditionHasNoProp,0x51E32BCD);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsMale,0xE074D914);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsPanicking,0x1B0B89ED);
INSTANTIATE_RTTI_CLASS(CVehicleConditionEntryPointHasOpenableDoor,0x857B3111);
INSTANTIATE_RTTI_CLASS(CVehicleConditionRoofState,0xD059113A);
INSTANTIATE_RTTI_CLASS(CScenarioConditionArePedConfigFlagsSet,0xB0168A7);
INSTANTIATE_RTTI_CLASS(CScenarioConditionArePedConfigFlagsSetOnOtherPed,0xA7BE179D);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsMultiplayerGame,0xCA0E99C);
INSTANTIATE_RTTI_CLASS(CScenarioConditionOnStraightPath,0x6646E760);
INSTANTIATE_RTTI_CLASS(CScenarioConditionHasParachute,0x3D7FF1F9);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsSwat,0x44C59AB4);
INSTANTIATE_RTTI_CLASS(CScenarioConditionAffluence,0xF5148459);
INSTANTIATE_RTTI_CLASS(CScenarioConditionTechSavvy,0x78FD5E31);
INSTANTIATE_RTTI_CLASS(CScenarioConditionAmbientEventTypeCheck,0x5E0CA0AA);
INSTANTIATE_RTTI_CLASS(CScenarioConditionAmbientEventDirection,0x12CA6CEF);
INSTANTIATE_RTTI_CLASS(CScenarioConditionRoleInSyncedScene,0xFFB06E81);
INSTANTIATE_RTTI_CLASS(CScenarioConditionInVehicleOfType,0x488D3D89);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsRadioPlaying,0x31183FA3);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsRadioPlayingMusic,0x673E3736);
INSTANTIATE_RTTI_CLASS(CScenarioConditionInVehicleSeat,0x2EAD7385);
INSTANTIATE_RTTI_CLASS(CScenarioConditionAttachedToPropOfType,0x94E87BE5);
INSTANTIATE_RTTI_CLASS(CScenarioConditionFullyInIdle,0x95ABCF06);
INSTANTIATE_RTTI_CLASS(CScenarioConditionPedHeading,0xC0C1A7F6);
INSTANTIATE_RTTI_CLASS(CScenarioConditionInStationaryVehicleScenario,0xBC0673DA);
INSTANTIATE_RTTI_CLASS(CScenarioConditionPhoneConversationAvailable,0xA6AD0729);
INSTANTIATE_RTTI_CLASS(CScenarioPhoneConversationInProgress,0xD4FD2DD);
INSTANTIATE_RTTI_CLASS(CScenarioConditionCanStartNewPhoneConversation,0x9E466278);
INSTANTIATE_RTTI_CLASS(CScenarioConditionPhoneConversationStarting,0x25926E0);
INSTANTIATE_RTTI_CLASS(CScenarioConditionMovementModeType,0xA6BD5CC8);
INSTANTIATE_RTTI_CLASS(CScenarioConditionHasComponentWithFlag,0xC346C277);
INSTANTIATE_RTTI_CLASS(CScenarioConditionPlayerHasSpaceForIdle,0xA05A4F89);
INSTANTIATE_RTTI_CLASS(CScenarioConditionCanPlayInCarIdle,0xF1B95D32);
INSTANTIATE_RTTI_CLASS(CScenarioConditionBraveryFlagSet,0x11585134);
INSTANTIATE_RTTI_CLASS(CScenarioConditionOnFootClipSet,0x5036257D);
INSTANTIATE_RTTI_CLASS(CScenarioConditionWorldPosWithinSphere, 0x4F7566AE);
INSTANTIATE_RTTI_CLASS(CScenarioConditionHasHighHeels, 0x69c7a2f8);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsReaction, 0x3f649f84);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsHeadbobbingToRadioMusicEnabled, 0xf8e7e7be);
INSTANTIATE_RTTI_CLASS(CScenarioConditionHeadbobMusicGenre, 0x11894842);
INSTANTIATE_RTTI_CLASS(CScenarioConditionIsTwoHandedWeaponEquipped, 0x2a35ae45);
INSTANTIATE_RTTI_CLASS(CScenarioConditionUsingLRAltClipset, 0x3efe67d4);
#if ENABLE_JETPACK
	INSTANTIATE_RTTI_CLASS(CScenarioConditionUsingJetpack,0xfb1b070b);
#endif

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CScenarioCondition::PreAddWidgets(bkBank& bank)
{
	bank.PushGroup(GetClassId().GetCStr());
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CScenarioCondition::PostAddWidgets(bkBank& bank)
{
	bank.PopGroup();
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

CPedModelInfo* CScenarioConditionPopulation::GetPedModelInfo(const sScenarioConditionData& conditionData) const
{
	if (conditionData.pPed)
	{
		return conditionData.pPed->GetPedModelInfo();
	}
	else if (conditionData.m_ModelId.IsValid())
	{
		return (CPedModelInfo*)CModelInfo::GetBaseModelInfo(conditionData.m_ModelId);
	}
	else
	{
		return NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionPlayingAnim::Result CScenarioConditionPlayingAnim::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionPlayingAnim should have a valid ped"))
	{
		strLocalIndex iDictIndex = strLocalIndex(fwClipSetManager::GetClipDictionaryIndex(static_cast<fwMvClipSetId>(m_ClipSet.GetHash())));

		// Check the dictionary is loaded
		if(iDictIndex != -1 && fwAnimManager::Get(iDictIndex))
		{
			const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex.Get(), m_Anim.GetHash());
			if(pClip)
			{
				CMove& move = static_cast<CMove&>(conditionData.pPed->GetAnimDirector()->GetMove());
				return move.FindClipHelper(iDictIndex.Get(), m_Anim.GetHash()) != NULL ? SCR_True : SCR_False;
			}
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionModel::Result CScenarioConditionModel::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPedModelInfo * pPedModelInfo = GetPedModelInfo(conditionData);
	if(aiVerifyf(pPedModelInfo, "CScenarioConditionModel should have a valid model id or ped!"))
	{
		if(aiVerifyf(m_ModelSet, "m_ModelSet is NULL"))
		{
			return (m_ModelSet->GetContainsModel(pPedModelInfo->GetModelNameHash())) ? SCR_True : SCR_False;
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionSpeed::Result CScenarioConditionSpeed::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionSpeed should have a valid ped"))
	{
		switch(m_Speed)
		{
		case Stand:
			return conditionData.pPed->GetMotionData()->GetIsStill() ? SCR_True : SCR_False;
		case Walk:
			return conditionData.pPed->GetMotionData()->GetIsWalking() ? SCR_True : SCR_False;
		case Run:
			return conditionData.pPed->GetMotionData()->GetIsRunning() ? SCR_True : SCR_False;
		case Sprint:
			return conditionData.pPed->GetMotionData()->GetIsSprinting() ? SCR_True : SCR_False;
		default:
			aiAssertf(0, "Unhandled speed");
			return SCR_False;
		};
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionCrouched::Result CScenarioConditionCrouched::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionCrouched should have a valid ped"))
	{
		return conditionData.pPed->GetIsCrouching() ? SCR_True : SCR_False;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionInCover::Result CScenarioConditionInCover::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionInCover should have a valid ped"))
	{
		return conditionData.pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER) ? SCR_True : SCR_False;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionHealth::Result CScenarioConditionHealth::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionHealth should have a valid ped"))
	{
		return Compare(m_Comparison, conditionData.pPed->GetHealth(), m_Health);
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionEquippedWeapon::Result CScenarioConditionEquippedWeapon::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionEquippedWeapon should have a valid ped"))
	{
		const CPedWeaponManager* pWeaponManager = conditionData.pPed->GetWeaponManager();
		if(pWeaponManager)
		{
			return pWeaponManager->GetEquippedWeaponHash() == m_Weapon ? SCR_True : SCR_False;
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionWet::Result CScenarioConditionWet::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionWet should have a valid ped"))
	{
		return conditionData.pPed->GetTimeSincePedInWater() <= CTaskAmbientClips::GetSecondsSinceInWaterThatCountsAsWet() ? SCR_True : SCR_False;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionOutOfBreath::Result CScenarioConditionOutOfBreath::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionOutOfBreath should have a valid ped"))
	{
		return (conditionData.pPed->GetPlayerInfo() && conditionData.pPed->GetPlayerInfo()->GetSprintEnergyRatio() <= 0.5f) ? SCR_True : SCR_False;
	}
	return SCR_False;
}


////////////////////////////////////////////////////////////////////////////////

CScenarioConditionJustGotUp::Result CScenarioConditionJustGotUp::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionJustGotUp should have a valid ped"))
	{
		CPedIntelligence* pIntelligence = conditionData.pPed->GetPedIntelligence();
		if (pIntelligence && pIntelligence->GetLastGetUpTime() > 0 && !conditionData.pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableDustOffAnims))
		{
			return (pIntelligence->GetLastGetUpTime() + CTaskAmbientClips::GetMaxTimeSinceGetUpForAmbientIdles()) > fwTimer::GetTimeInMilliseconds() ? SCR_True : SCR_False;			
		}		
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionAlert::Result CScenarioConditionAlert::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionAlert should have a valid ped"))
	{
		// Get the ped's current alertness state
		CPedIntelligence::AlertnessState alertState = conditionData.pPed->GetPedIntelligence()->GetAlertnessState();

		switch(alertState)
		{
		case CPedIntelligence::AS_Alert:
		case CPedIntelligence::AS_VeryAlert:
		case CPedIntelligence::AS_MustGoToCombat:
			return SCR_True;

		default:
			return SCR_False;
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionIsPlayer::Result CScenarioConditionIsPlayer::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionIsPlayer should have a valid ped"))
	{
		return conditionData.pPed->IsPlayer() ? SCR_True : SCR_False;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionIsPlayerInMultiplayerGame::Result CScenarioConditionIsPlayerInMultiplayerGame::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionIsPlayerInMultiplayerGame should have a valid ped"))
	{
		return (conditionData.pPed->IsPlayer() && NetworkInterface::IsGameInProgress()) ? SCR_True : SCR_False;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionIsMale::Result CScenarioConditionIsMale::CheckInternal(const sScenarioConditionData& conditionData) const
{
	CPedModelInfo* pModelInfo = GetPedModelInfo(conditionData);
	if (aiVerifyf(pModelInfo, "CScenarioConditionIsMale requires either a valid model index or a valid ped!"))
	{
		return pModelInfo->IsMale() ? SCR_True : SCR_False;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionIsPlayerTired::Result CScenarioConditionIsPlayerTired::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionIsPlayerTired should have a valid ped"))
	{
		if(conditionData.pPed->IsAPlayerPed())
		{
			CPlayerInfo* pPlayerInfo = conditionData.pPed->GetPlayerInfo();

			static dev_float TIRED_PLAYER_SPRINT_VALUE = 400.0f;
			if( pPlayerInfo->GetPlayerDataSprintEnergy() < TIRED_PLAYER_SPRINT_VALUE )
			{
				return SCR_True;
			}
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionDistanceToPlayer::Result CScenarioConditionDistanceToPlayer::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionDistanceToPlayer should have a valid ped"))
	{
		const CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if(pPlayerPed)
		{
			ScalarV fDistToPlayerSq = DistSquared(conditionData.pPed->GetTransform().GetPosition(), pPlayerPed->GetTransform().GetPosition());
			return Compare(m_Comparison, fDistToPlayerSq.Getf(), square(m_Range));
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionRaining::Result CScenarioConditionRaining::CheckInternal(const sScenarioConditionData& conditionData) const
{
	// Early out if we are ignoring the weather.
	if (conditionData.iScenarioFlags & SF_IgnoreWeather)
	{
		return m_Result;
	}

	CScenarioConditionRaining::Result checkResult = SCR_False;
	if (conditionData.pPed && ( conditionData.pPed->m_nFlags.bInMloRoom || (conditionData.pPed->IsLocalPlayer() && !conditionData.pPed->GetPlayerInfo()->GetIsPlayerShelteredFromRain())) )
	{
		checkResult = SCR_False;
	}
	else if( g_weather.IsRaining() )
	{
		checkResult = SCR_True;
	}
	
	conditionData.m_bOutFailedWeatherCondition = (checkResult != m_Result);
	return checkResult;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionSunny::Result CScenarioConditionSunny::CheckInternal(const sScenarioConditionData& conditionData) const
{
	// Early out if we are ignoring the weather.
	if (conditionData.iScenarioFlags & SF_IgnoreWeather)
	{
		return m_Result;
	}

	CScenarioConditionSunny::Result checkResult = SCR_False;
	if (conditionData.pPed && conditionData.pPed->m_nFlags.bInMloRoom)
	{
		checkResult = SCR_False;
	} 
	else if((g_weather.GetNextType().m_sun > 0.6f) && !CClock::IsTimeInRange(20, 6))
	{
		checkResult = SCR_True;
	}

	conditionData.m_bOutFailedWeatherCondition = (checkResult != m_Result);
	return checkResult;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionSnowing::Result CScenarioConditionSnowing::CheckInternal(const sScenarioConditionData& conditionData) const
{
	// Early out if we are ignoring the weather.
	if (conditionData.iScenarioFlags & SF_IgnoreWeather)
	{
		return m_Result;
	}

	CScenarioConditionSnowing::Result checkResult = SCR_False;
	if (conditionData.pPed && conditionData.pPed->m_nFlags.bInMloRoom)
	{
		checkResult = SCR_False;
	}
	else if(g_weather.IsSnowing())
	{
		checkResult = SCR_True;
	}

	conditionData.m_bOutFailedWeatherCondition = (checkResult != m_Result);
	return checkResult;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionWindy::Result CScenarioConditionWindy::CheckInternal(const sScenarioConditionData& conditionData) const
{
	// Early out if we are ignoring the weather.
	if (conditionData.iScenarioFlags & SF_IgnoreWeather)
	{
		return m_Result;
	}

	CScenarioConditionWindy::Result checkResult = SCR_False;
	if (conditionData.pPed && conditionData.pPed->m_nFlags.bInMloRoom)
	{
		checkResult = SCR_False;
	}
	else if(g_weather.IsWindy())
	{
		checkResult = SCR_True;
	}

	conditionData.m_bOutFailedWeatherCondition = (checkResult != m_Result);
	return checkResult;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionTime::CScenarioConditionTime()
#if __BANK
: m_iComboIndex(0)
#endif // __BANK
{
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionTime::Result CScenarioConditionTime::CheckInternal(const sScenarioConditionData& conditionData) const
{
	//Check if we are ignoring time.
	if(conditionData.iScenarioFlags & SF_IgnoreTime)
	{
		return m_Result;
	}

	int hour = CClock::GetHour();
	if(conditionData.iScenarioFlags & SF_AddPedRandomTimeOfDayDelay)
	{
		// Need a ped if this flag is set.
		taskAssert(conditionData.pPed);

		// In this case, we also need the minutes and seconds from CClock.
		const int minute = CClock::GetMinute();
		const int seconds = CClock::GetSecond();

		// Make sure the input hour is in the 0-23 range that we expect.
		Assert(hour >= 0 && hour <= 23);
		
		// Compute the time of day measured in hours, as a floating point value.
		float timeInHours = (float)hour + (1.0f/60.0f)*(float)minute + (1.0f/3600.0f)*(float)seconds;

		// Use a ped-specific random number generator to figure out how late this ped is willing to stay.
		mthRandom rnd(conditionData.pPed->GetRandomSeed() + 123 /* MAGIC! */);
		const float change = rnd.GetFloat()*CTaskUseScenario::GetTunables().m_TimeOfDayRandomnessHours;

		// The ped should stay for longer, so we need to effectively compare an earlier time against the limit.
		timeInHours -= change;

		// Wrap it if it went negative.
		timeInHours = Selectf(timeInHours, timeInHours, timeInHours + 24.0f);

		// Convert it back into an hour.
		hour = (int)timeInHours;
		if(hour == 24)
		{
			hour = 23;
		}
		Assertf(hour >= 0 && hour <= 23, "Hour is %d, timeInHours = %f, change = %f, clock is %d:%d:%d.", hour, timeInHours, change, CClock::GetHour(), minute, seconds);
	}

	return (m_Hours & (1<<hour)) ? SCR_True : SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CScenarioConditionTime::PreAddWidgets(bkBank& bank)
{
	superclass::PreAddWidgets(bank);

	static const char* comboText[TP_Max] =
	{
		"Unset",
		"Morning",
		"Afternoon",
		"Evening",
		"Night",
	};
	bank.AddCombo("Presets", &m_iComboIndex, TP_Max, comboText, datCallback(MFA(CScenarioConditionTime::PresetChangedCB), (datBase*)this));
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CScenarioConditionTime::PresetChangedCB()
{
	switch(m_iComboIndex)
	{
	case Unset:
		m_Hours = 0;
		break;
	case Morning:
		m_Hours = Hour_6 | Hour_7 | Hour_8 | Hour_9 | Hour_10 | Hour_11;
		break;
	case Afternoon:
		m_Hours = Hour_12 | Hour_13	| Hour_14 | Hour_15 | Hour_16;
		break;
	case Evening:
		m_Hours = Hour_17 | Hour_18	| Hour_19 | Hour_20 | Hour_21 | Hour_22;
		break;
	case Night:
		m_Hours = Hour_23 | Hour_0 | Hour_1 | Hour_2 | Hour_3 | Hour_4 | Hour_5;
		break;
	default:
		aiAssertf(0, "Unhandled case %d", m_iComboIndex);
		break;
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionInInterior::Result CScenarioConditionInInterior::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionInInterior should have a valid ped"))
	{
		if(conditionData.pPed->m_nFlags.bInMloRoom)
		{
			return SCR_True;
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

strLocalIndex CScenarioConditionHasProp::GetPropModelIndex(const CPed* pPed)
{
	strLocalIndex uProp = strLocalIndex(fwModelId::MI_INVALID);
	if(pPed)
	{
		const CObject* pObject = pPed->GetHeldObject(*pPed);
		if(pObject)
		{
			uProp = pObject->GetModelIndex();
		}
		else if(!pPed->IsNetworkClone())
		{
			uProp = pPed->GetLoadingObjectIndex(*pPed);
		}
	}
	return uProp;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionHasProp::Result CScenarioConditionHasProp::CheckInternal(const sScenarioConditionData& conditionData) const
{
	// We don't care about the prop, so ignore this check - it always return true
	if(conditionData.iScenarioFlags & SF_IgnoreHasPropCheck)
	{
		return m_Result;
	}

	if(aiVerifyf(conditionData.pPed, "CScenarioConditionHasProp should have a valid ped"))
	{
		strLocalIndex uHeldProp = GetPropModelIndex(conditionData.pPed);
		if(CModelInfo::IsValidModelInfo(uHeldProp.Get()))
		{
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(uHeldProp));
			aiAssert(pModelInfo);
			s32 iPropSetIndex = CAmbientModelSetManager::GetInstance().GetModelSetIndexFromHash(CAmbientModelSetManager::kPropModelSets, m_PropSet.GetHash());
			if(aiVerifyf(iPropSetIndex >= 0, "Propset %s doesn't have an entry, check propsets.meta", m_PropSet.GetCStr()))
			{
				const CAmbientPropModelSet* pPropSet = CAmbientModelSetManager::GetInstance().GetPropSet(iPropSetIndex);
				if(pPropSet)
				{
					if(pPropSet->GetContainsModel(pModelInfo->GetHashKey()))
					{
						return SCR_True;
					}
				}
			}		
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionHasNoProp::Result CScenarioConditionHasNoProp::CheckInternal(const sScenarioConditionData& conditionData) const
{
	u32 uHeldProp = CScenarioConditionHasProp::GetPropModelIndex(conditionData.pPed).Get();
	if(!CModelInfo::IsValidModelInfo(uHeldProp))
	{
		return SCR_True;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionIsPanicking::Result CScenarioConditionIsPanicking::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionInInterior should have a valid ped"))
	{
		if ( conditionData.pPed->GetIsPanicking() )
		{
			return SCR_True;
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CVehicleConditionEntryPointHasOpenableDoor::Result CVehicleConditionEntryPointHasOpenableDoor::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pTargetVehicle, "CVehicleConditionEntryPointHasOpenableDoor should have a valid target vehicle") && 
		aiVerifyf(conditionData.pTargetVehicle->IsEntryIndexValid(conditionData.iTargetEntryPoint), "Entry index %u is invalid", conditionData.iTargetEntryPoint))
	{
		const CEntryExitPoint* pEntryExitPoint = conditionData.pTargetVehicle->GetEntryExitPoint(conditionData.iTargetEntryPoint);
		if ( pEntryExitPoint )
		{
			const CCarDoor* pDoor = conditionData.pTargetVehicle->GetDoorFromBoneIndex(pEntryExitPoint->GetDoorBoneIndex());
			if (pDoor && pDoor->GetIsIntact(conditionData.pTargetVehicle) && pDoor->GetIsClosed())
			{
				return SCR_True;
			}
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CVehicleConditionRoofState::Result CVehicleConditionRoofState::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pTargetVehicle, "CVehicleConditionRoofState should have a valid target vehicle"))
	{
		bool bAllTestsPassed = true;

		const bool bVehicleHasRoof = conditionData.pTargetVehicle->CarHasRoof();
		const bool bRoofIsDown = conditionData.pTargetVehicle->DoesVehicleHaveAConvertibleRoofAnimation() && conditionData.pTargetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERED ? true : false;
		const bool bRoofIsUp = conditionData.pTargetVehicle->DoesVehicleHaveAConvertibleRoofAnimation() && conditionData.pTargetVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISED ? true : false;

		if ((m_RoofState & NoRoof) && bVehicleHasRoof)
		{
			bAllTestsPassed = false;
		}

		if ((m_RoofState & HasRoof) && !bVehicleHasRoof)
		{
			bAllTestsPassed = false;
		}

		if ((m_RoofState & RoofIsDown) && !bRoofIsDown)
		{
			bAllTestsPassed = false;
		}

		if ((m_RoofState & RoofIsUp) && !bRoofIsUp)
		{
			bAllTestsPassed = false;
		}

		if (bAllTestsPassed)
		{
			return SCR_True;
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionArePedConfigFlagsSet::Result CScenarioConditionArePedConfigFlagsSet::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionArePedConfigFlagsSet should have a valid ped"))
	{
		if (conditionData.pPed->AreConfigFlagsSet(m_ConfigFlags))
		{
			return SCR_True;
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionArePedConfigFlagsSetOnOtherPed::Result CScenarioConditionArePedConfigFlagsSetOnOtherPed::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(conditionData.pOtherPed && conditionData.pOtherPed->AreConfigFlagsSet(m_ConfigFlags))
	{
		return SCR_True;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionIsMultiplayerGame::Result CScenarioConditionIsMultiplayerGame::CheckInternal(const sScenarioConditionData& UNUSED_PARAM(conditionData)) const
{
	if(NetworkInterface::IsGameInProgress())
	{		
		return SCR_True;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionOnStraightPath::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;

	Result ret = SCR_False;

	if(aiVerifyf(pPed, "CScenarioConditionOnStraightPath should have a valid ped"))
	{
		// I think that if this flag is set, we are rounding a corner, so then we
		// probably shouldn't even try to look at the current move target.
		if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_TakingRouteSplineCorner))
		{
			// Get the current CTaskMoveFollowNavMesh task, if it exists.
			CTaskMoveFollowNavMesh* pNavmeshTask = static_cast<CTaskMoveFollowNavMesh*>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
			if(pNavmeshTask && pNavmeshTask->IsFollowingNavMeshRoute())
			{
				// Get the position of ourself, and the current target (path waypoint?).
				Vector3 tgtPosVector;
				pNavmeshTask->GetPositionAheadOnRoute(pPed, m_MinDist, tgtPosVector);

				const Vec3V tgtPosV(tgtPosVector);
				const Vec3V currentPosV = pPed->GetTransform().GetPosition();

				// Get the minimum distance we want to be from the next target.
				const ScalarV minDistV = LoadScalar32IntoScalarV(m_MinDist);
				const ScalarV minDistSquaredV = Scale(minDistV, minDistV);	// Could be precomputed.

				// Compute a delta vector and the squared distance to the target.
				const Vec3V toTgtV = Subtract(tgtPosV, currentPosV);
				const ScalarV distSquaredV = MagSquared(toTgtV);
	
				// Check if we are far enough away.
				if(IsGreaterThanAll(distSquaredV, minDistSquaredV))
				{
					const ScalarV zeroV(V_ZERO);
					const Vec3V fwdDirV = pPed->GetTransform().GetForward();

					// Compute a flattened target vector.
					Vec3V toTgtFlatV = toTgtV;
					toTgtFlatV.SetZ(zeroV);

					// Compute a flattened forward vector (may not be unit length, for animals on slopes for example).
					Vec3V fwdDirFlatV = fwdDirV;
					fwdDirFlatV.SetZ(zeroV);

					// Compute the dot product, and check if we are pointing in the right 180 degrees.
					const ScalarV dotV = Dot(toTgtFlatV, fwdDirFlatV);
					if(IsGreaterThanAll(dotV, zeroV))
					{
						// MAGIC! This is the max angle we are allowed to face, away from the target.
						// We use the square of the cosine.
						static const float s_CosAngleSquared = square(cosf(5.0f*DtoR));
						const ScalarV cosAngleSquaredV = LoadScalar32IntoScalarV(s_CosAngleSquared);

						// We want to check the angle, something like this:
						// (A*B)/(Mag(A)*Mag(B)) >= cos(Angle)
						// but multiply the magnitudes to the right side, and square everything (all numbers are positive):
						// (A*B)^2 >= cos(Angle)^2*Mag(A)^2*Mag(B)^2
						// This avoids division and square roots.

						const ScalarV dotSquaredV = Scale(dotV, dotV);
						const ScalarV fwdMagSquaredV = MagSquared(fwdDirFlatV);
						const ScalarV minDotSquaredV = Scale(cosAngleSquaredV, Scale(fwdMagSquaredV, distSquaredV));

						if(IsGreaterThanAll(dotSquaredV, minDotSquaredV))
						{
							// Condition fulfilled.
							ret = SCR_True;
						}
					}

				}
			}
		}
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionHasParachute::Result CScenarioConditionHasParachute::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(conditionData.pPed && conditionData.pPed->GetInventory() && conditionData.pPed->GetInventory()->GetWeapon(GADGETTYPE_PARACHUTE))
	{		
		return SCR_True;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

#if ENABLE_JETPACK

CScenarioConditionUsingJetpack::Result CScenarioConditionUsingJetpack::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(conditionData.pPed && conditionData.pPed->IsUsingJetpack())
	{		
		return SCR_True;
	}
	return SCR_False;
}

#endif

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionIsSwat::Result CScenarioConditionIsSwat::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(conditionData.pPed && (conditionData.pPed->GetPedType() == PEDTYPE_SWAT))
	{		
		return SCR_True;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioConditionAffluence::Result CScenarioConditionAffluence::CheckInternal(const sScenarioConditionData& conditionData) const
{
	CPedModelInfo* pModelInfo = GetPedModelInfo(conditionData);
	if (aiVerifyf(pModelInfo, "CScenarioConditionAffluence requires either a valid model index or a valid ped!"))
	{
		return pModelInfo->GetPersonalitySettings().GetAffluence() == m_Affluence ? SCR_True : SCR_False;
	}
	return SCR_False;
}

/////////////////////////////////////////////////////////////////////////////////////////

CScenarioConditionTechSavvy::Result CScenarioConditionTechSavvy::CheckInternal(const sScenarioConditionData& conditionData) const
{
	CPedModelInfo* pModelInfo = GetPedModelInfo(conditionData);
	if (aiVerifyf(pModelInfo, "CScenarioConditionTechSavvy requires either a valid model index or a valid ped!"))
	{
		return pModelInfo->GetPersonalitySettings().GetTechSavvy() == m_TechSavvy ? SCR_True : SCR_False;
	}
	return SCR_False;
}

/////////////////////////////////////////////////////////////////////////////////////////

CScenarioConditionAmbientEventTypeCheck::Result CScenarioConditionAmbientEventTypeCheck::CheckInternal(const sScenarioConditionData& conditionData) const
{
	return conditionData.eAmbientEventType == m_Type ? SCR_True : SCR_False;	
}

/////////////////////////////////////////////////////////////////////////////////////////

CScenarioConditionAmbientEventDirection::Result CScenarioConditionAmbientEventDirection::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionAmbientEventDirection should have a valid ped"))
	{
		//Grab the ped position.
		Vec3V vPedPosition = pPed->GetTransform().GetPosition();
		
		//Calculate a vector from the ped to the event position.
		Vec3V vPedToEvent = Subtract(conditionData.vAmbientEventPosition, vPedPosition);
		if(m_FlattenZ)
		{
			vPedToEvent.SetZ(ScalarV(V_ZERO));
		}
		Vec3V vPedToEventNormal = NormalizeFastSafe(vPedToEvent, Vec3V(V_X_AXIS_WZERO));
		
		//Transform the direction into world space.
		Vec3V vDirectionWorld = pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_Direction));
		if(m_FlattenZ)
		{
			vDirectionWorld.SetZ(ScalarV(V_ZERO));
		}
		Vec3V vDirectionWorldNormal = NormalizeFastSafe(vDirectionWorld, Vec3V(V_X_AXIS_WZERO));
		
		//Generate the dot product.
		ScalarV scDot = Dot(vPedToEventNormal, vDirectionWorldNormal);
		if(IsLessThanOrEqualAll(scDot, ScalarVFromF32(m_Threshold)))
		{
			return SCR_False;
		}
		else
		{
			return SCR_True;
		}
	}
	return SCR_False;
}

/////////////////////////////////////////////////////////////////////////////////////////

CScenarioConditionRoleInSyncedScene::Result CScenarioConditionRoleInSyncedScene::CheckInternal(const sScenarioConditionData& conditionData) const
{
	return (conditionData.m_RoleInSyncedScene == m_Role) ? SCR_True : SCR_False;
}

/////////////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionInVehicleOfType::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;

	// Verify the ped.
	if (aiVerifyf(pPed, "CScenarioConditionInVehicleOfType did not have a valid ped in the condition data!"))
	{
		if (pPed->GetIsInVehicle() && m_VehicleModelSet)
		{
			return m_VehicleModelSet->GetContainsModel(pPed->GetMyVehicle()->GetVehicleModelInfo()->GetHashKey()) ? SCR_True : SCR_False;
		}
	}
	return SCR_False;
}

/////////////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionIsRadioPlaying::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;

	// Verify the ped.
	if (aiVerifyf(pPed, "CScenarioConditionIsRadioPlaying did not have a valid ped in the condition data!"))
	{
#if NA_RADIO_ENABLED
		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if (pVehicle)
		{
			audVehicleAudioEntity* pVehicleAudioEntity = pVehicle->GetVehicleAudioEntity();

			if (pVehicleAudioEntity && pVehicleAudioEntity->IsRadioEnabled() && pVehicleAudioEntity->GetRadioGenre() != RADIO_GENRE_OFF)
			{
				return SCR_True;
			}
		}
#endif // NA_RADIO_ENABLED
	}
	return SCR_False;
}



/////////////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionIsRadioPlayingMusic::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;

	// Verify the ped.
	if (aiVerifyf(pPed, "CScenarioConditionIsRadioPlayingMusic did not have a valid ped in the condition data!"))
	{
#if NA_RADIO_ENABLED
		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if (pVehicle)
		{
			audVehicleAudioEntity* pVehicleAudioEntity = pVehicle->GetVehicleAudioEntity();

			if (pVehicleAudioEntity && pVehicleAudioEntity->IsRadioEnabled())
			{
				const audRadioStation* pCurrentRadioStation = pVehicleAudioEntity->GetRadioStation();
				if (pCurrentRadioStation)
				{
					if (pCurrentRadioStation->IsTalkStation())
					{
						return SCR_False;
					}
					else if(pCurrentRadioStation->GetCurrentTrack().IsInitialised() && (pCurrentRadioStation->GetCurrentTrack().GetCategory() == RADIO_TRACK_CAT_MUSIC || pCurrentRadioStation->GetCurrentTrack().GetCategory() == RADIO_TRACK_CAT_TAKEOVER_MUSIC))
					{
						return SCR_True;
					}
					else
					{
						return SCR_False;
					}
				}
				else 
				{
					// The radio isn't playing yet; see if the driver ped will play one. To be safe, disallow if any choice could be off 
					//  or talk radio. [5/20/2013 mdawe]
					s32 iFirstPotentialGenre  = -1;
					s32 iSecondPotentialGenre = -1;
					pPed->GetPedRadioCategory(iFirstPotentialGenre, iSecondPotentialGenre);
					if ( iFirstPotentialGenre != -1 && iSecondPotentialGenre != -1 &&
							(iFirstPotentialGenre != RADIO_GENRE_LEFT_WING_TALK || iFirstPotentialGenre != RADIO_GENRE_RIGHT_WING_TALK) &&
							(iSecondPotentialGenre != RADIO_GENRE_LEFT_WING_TALK || iSecondPotentialGenre != RADIO_GENRE_RIGHT_WING_TALK) )
					{
						return SCR_True;
					}
				}
			}
		}
#endif // NA_RADIO_ENABLED
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionInVehicleSeat::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;

	// Verify the ped.
	if (aiVerifyf(pPed, "CScenarioConditionInVehicleSeat did not have a valid ped in the condition data!"))
	{
		CVehicle* pVehicle = pPed->GetVehiclePedInside();

		if (pVehicle)
		{
			s32 iPedSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			if (iPedSeatIndex != -1 && iPedSeatIndex == m_SeatIndex)
			{
				return SCR_True;
			}
		}
	}
	return SCR_False;
}

/////////////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionAttachedToPropOfType::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;

	// Verify the ped.
	if (aiVerifyf(pPed, "CScenarioConditionAttachedToPropOfType did not have a valid ped in the condition data!"))
	{
		// Check if the ped is attached to anything.
		const CPhysical* pParent = (CPhysical*)pPed->GetAttachParent();
		if (pParent && pParent->GetIsTypeObject())
		{
			// Check if the object the ped is attached to is inside of the modelset specified in metadata.
			const CObject* pAttachObject = static_cast<const CObject*>(pParent);
			aiAssert(pAttachObject->GetBaseModelInfo());
			if (m_PropModelSet && m_PropModelSet->GetContainsModel(pAttachObject->GetBaseModelInfo()->GetHashKey()))
			{
				return SCR_True;
			}
		}
	}

	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionFullyInIdle::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CTaskMotionBase* pMotionTask = conditionData.pPed->GetCurrentMotionTask();
	if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION && static_cast<const CTaskHumanLocomotion*>(pMotionTask)->IsFullyInIdle())
		return SCR_True;
	else
		return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionPedHeading::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionPedHeading should have a valid ped"))
	{
		//Grab the ped heading.
		float fPedHeading = pPed->GetCurrentHeading();
		
		//Subtract the shorter angle between the ped heading and the scenario heading.
		float fShorter = SubtractAngleShorter(fPedHeading, conditionData.fPointHeading);

		//Compare the heading difference to the animation's heading change.
		//If the difference is lower than the threshold, return true.
		if (fabs(SubtractAngleShorter(fShorter, m_TurnAngleDegrees * DtoR)) * RtoD < m_ThresholdDegrees)
		{
			return SCR_True;
		}
	}
	return SCR_False;
}

/////////////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionInStationaryVehicleScenario::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	// Verify the ped.
	if (aiVerifyf(pPed, "CScenarioConditionInStationaryVehicleScenario did not have a valid ped in the condition data!"))
	{
		CTaskUseVehicleScenario* pTaskScenario = static_cast<CTaskUseVehicleScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_VEHICLE_SCENARIO));
		if (pTaskScenario && pTaskScenario->GetState() == CTaskUseVehicleScenario::State_UsingScenario)
		{
			// Considered stationary if the ped should never leave or should leave after threshold.
			float fBaseTimeToUse = pTaskScenario->GetBaseTimeToUse();
			if (fBaseTimeToUse < 0.0f || fBaseTimeToUse >= m_MinimumWaitTime)
			{
				return SCR_True;
			}
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionPhoneConversationAvailable::CheckInternal(const sScenarioConditionData& conditionData) const
{
	// If we're in the middle of a conversation, we can continue in it. [7/30/2012 mdawe]
	if (CScenarioPhoneConversationInProgress::CheckPhoneConversationInProgress(conditionData) == SCR_True) 
	{
		return SCR_True;
	}

	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionPhoneConversationAvailable did not have a valid ped in the condition data!"))
	{
		// Limit the number of conversations that can happen simultaneously. [7/30/2012 mdawe]
		int num_spaces = CTaskConversationHelper::GetPool()->GetNoOfFreeSpaces();
		if (num_spaces == 0)
		{
			return SCR_False;
		}

		// Check to see if we have the audio [7/30/2012 mdawe]
		const audSpeechAudioEntity* const pAudioEntity = pPed->GetSpeechAudioEntity();
		if (pAudioEntity && (pAudioEntity->DoesPhoneConversationExistForThisPed()))
		{
			// Make sure disc isn't too busy.
			if (!g_SpeechManager.IsStreamingBusyForContext(ATPARTIALSTRINGHASH("PHONE_CONV1_INTRO", 0x8AADB637))) 
			{
				return SCR_True;
			}
		}
	}
	return SCR_False;
}
////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioPhoneConversationInProgress::CheckInternal(const sScenarioConditionData& conditionData) const
{
	return CheckPhoneConversationInProgress(conditionData);
}

CScenarioCondition::Result CScenarioPhoneConversationInProgress::CheckPhoneConversationInProgress(const sScenarioConditionData& conditionData)
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CheckPhoneConversationInProgress did not have a valid ped in the condition data!"))
	{
		CTaskAmbientClips *pAmbientTask = static_cast<CTaskAmbientClips*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if (pAmbientTask) 
		{
			return (pAmbientTask->IsInConversation() ? SCR_True : SCR_False);
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionCanStartNewPhoneConversation::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionCanStartNewPhoneConversation did not have a valid ped in the condition data!"))
	{
		// Prevent the player characters from talking on the phone. They shouldn't do this as ambient characters, they don't have the audio contexts. [7/8/2013 mdawe]
		ePedType pedType = pPed->GetPedType();
		if (pedType == PEDTYPE_PLAYER_0 || pedType == PEDTYPE_PLAYER_1 || pedType == PEDTYPE_PLAYER_2)
		{
			return SCR_False;
		}

		return pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedHadPhoneConversation) ? SCR_False : SCR_True;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionPhoneConversationStarting::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionPhoneConversationStarting did not have a valid ped in the condition data!"))
	{
		CTaskAmbientClips *pAmbientTask = static_cast<CTaskAmbientClips*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if (pAmbientTask) 
		{
			return (pAmbientTask->ShouldPlayPhoneEnterAnim() ? SCR_True : SCR_False);
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionMovementModeType::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionActionModeType did not have a valid ped in the condition data!"))
	{
		CTaskMotionPed* pMotionPedTask = NULL;
		CTaskMotionBase* pPrimaryMotionTask = pPed->GetPrimaryMotionTask();
		if(pPrimaryMotionTask && pPrimaryMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
			pMotionPedTask = static_cast<CTaskMotionPed*>(pPrimaryMotionTask);

		switch(m_MovementModeType)
		{
		case MMT_Action:
			if(pMotionPedTask && pMotionPedTask->GetState() != CTaskMotionPed::State_ActionMode)
				return SCR_False;
			break;
		case MMT_Stealth:
			if(pMotionPedTask && pMotionPedTask->GetState() != CTaskMotionPed::State_Stealth)
				return SCR_False;
			break;
		case MMT_Any:
			if(pMotionPedTask && pMotionPedTask->GetState() != CTaskMotionPed::State_ActionMode && pMotionPedTask->GetState() != CTaskMotionPed::State_Stealth)
				return SCR_False;
			break;
		default:
			aiAssertf(0, "Unset movement mode type [%d]", m_MovementModeType);
			return SCR_False;
		}

		bool bInHighEnergyMode = pPed->IsHighEnergyMovementMode();

		if((m_HighEnergy && bInHighEnergyMode) || (!m_HighEnergy && !bInHighEnergyMode))
		{
			if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponInfo())
			{
				const CWeaponInfo *pInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();

				if(pInfo->GetMovementModeConditionalIdleHash() == m_MovementModeIdle.GetHash())
				{
					return SCR_True;
				}
			}
		}

		if(m_MovementModeIdle.GetHash() == 0)
		{
			return SCR_True;
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionHasComponentWithFlag::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if(aiVerifyf(pPed, "CScenarioConditionHasComponentWithFlag did not have a valid ped."))
	{
		const CPedVariationData& varData = pPed->GetPedDrawHandler().GetVarData();
		/*const*/ CPedModelInfo* pModelInfo = pPed->GetPedModelInfo();

		if(!pModelInfo)
		{
			return SCR_False;
		}

		/*const*/ CPedVariationInfoCollection* pVarInfoCollection = pModelInfo->GetVarInfo();
		if(!pVarInfoCollection)
		{
			return SCR_False;
		}

		const u32 flags = m_PedComponentFlag;
		for(int i = 0; i < PV_MAX_COMP; i++)
		{
			ePedVarComp comp = (ePedVarComp)i;
			const u32 drawblId = varData.GetPedCompIdx(comp);
			if(pVarInfoCollection->HasComponentFlagsSet(comp, drawblId, flags))
			{
				return SCR_True;
			}
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionPlayerHasSpaceForIdle::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionPlayerHasSpaceForIdle did not have a valid ped in the condition data!"))
	{
		if ( pPed->IsPlayer() )
		{
			if (aiVerifyf(pPed->GetPedIntelligence(), "Player didn't have a PedIntelligence inside CScenarioConditionPlayerHasSpaceForIdle"))
			{
				CTaskPlayerIdles* pPlayerIdlesTask = static_cast<CTaskPlayerIdles*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_IDLES));
				if (pPlayerIdlesTask)
				{
					return pPlayerIdlesTask->HasSpaceToPlayIdleAnims() ? SCR_True : SCR_False;
				}
			}
		}
	}

	// If this was called incorrectly, assume there's space. [11/28/2012 mdawe]
	return SCR_True;
}

////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionCanPlayInCarIdle::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionCanPlayInCarIdle did not have a valid ped in the condition data!"))
	{
		return pPed->CanPlayInCarIdles() ? SCR_True : SCR_False;
	}

	return SCR_True;
}

////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionBraveryFlagSet::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionBraveryFlagSet did not have a valid ped in the condition data!"))
	{
		return pPed->CheckBraveryFlags(BIT(m_Flag)) ? SCR_True : SCR_False;
	}

	return SCR_True;
}

////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionOnFootClipSet::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionOnFootClipSet did not have a valid ped in the condition data!"))
	{
		const CTaskMotionBase* pMotionTask = conditionData.pPed->GetPrimaryMotionTask();
		if(pMotionTask && pMotionTask->GetDefaultOnFootClipSet() == m_ClipSet)
		{		
			
			return SCR_True;
		}
	}

	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionWorldPosWithinSphere::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionWorldPosWithinSphere did not have a valid ped in the condition data!"))
	{
		return (m_Sphere.ContainsPoint(pPed->GetTransform().GetPosition())) ? SCR_True : SCR_False;
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionHasHighHeels::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionHasHighHeels did not have a valid ped in the condition data!"))
	{
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasHighHeels))
		{
			return SCR_True;
		}
	}
	return SCR_False;
}

/////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionIsReaction::CheckInternal(const sScenarioConditionData& conditionData) const
{
	return conditionData.m_bIsReaction ? SCR_True : SCR_False;
}

/////////////////////////////////////////////////////////////////////////////////
CScenarioCondition::Result CScenarioConditionIsHeadbobbingToRadioMusicEnabled::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;
	if (aiVerifyf(pPed, "CScenarioConditionIsHeadbobbingToRadioMusicEnabled did not have a valid ped in the condition data!"))
	{
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
		{
			return SCR_True;
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionHeadbobMusicGenre::CheckInternal(const sScenarioConditionData& conditionData) const
{
	const CPed* pPed = conditionData.pPed;

	// Verify the ped.
	if (aiVerifyf(pPed, "CScenarioConditionHeadbobMusicGenre did not have a valid ped in the condition data!"))
	{
		CVehicle* pVehicle = pPed->GetVehiclePedInside();

		if (pVehicle)
		{
			audVehicleAudioEntity* pVehAudEntity = pVehicle->GetVehicleAudioEntity();
			if (pVehAudEntity)
			{
				const audRadioStation *pAudRadioStation = pVehAudEntity->GetRadioStation();
				if (pAudRadioStation)
				{
					RadioGenre radioGenre = pAudRadioStation->GetGenre();
					RadioGenrePar currentRadioGenre = GENRE_GENERIC;
					switch (radioGenre)
					{
					case RADIO_GENRE_COUNTRY:
						currentRadioGenre = GENRE_COUNTRY;
						break;
					case RADIO_GENRE_DANCE:
						currentRadioGenre = GENRE_GENERIC;
						break;
					case RADIO_GENRE_CLASSIC_HIPHOP:
						currentRadioGenre = GENRE_HIPHOP;
						break;
					case RADIO_GENRE_MODERN_HIPHOP:
						currentRadioGenre = GENRE_HIPHOP;
						break;
					case RADIO_GENRE_JAZZ:
						currentRadioGenre = GENRE_MOTOWN;
						break;
					case RADIO_GENRE_MOTOWN:
						currentRadioGenre = GENRE_MOTOWN;
						break;
					case RADIO_GENRE_POP:
						currentRadioGenre = GENRE_GENERIC;
						break;
					case RADIO_GENRE_PUNK:
						currentRadioGenre = GENRE_PUNK;
						break;
					case RADIO_GENRE_REGGAE:
						currentRadioGenre = GENRE_MOTOWN;
						break;
					case RADIO_GENRE_CLASSIC_ROCK:
						currentRadioGenre = GENRE_GENERIC;
						break;
					case RADIO_GENRE_MODERN_ROCK:
						currentRadioGenre = GENRE_GENERIC;
						break;					
					case RADIO_GENRE_MEXICAN:
						currentRadioGenre = GENRE_MEXICAN;
						break;
					default:
						currentRadioGenre = GENRE_GENERIC;
						break;
					}

					if (currentRadioGenre == m_RadioGenre)
					{
						return SCR_True;
					}
				}
				
			}
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionIsTwoHandedWeaponEquipped::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionIsTwoHandedWeaponEquipped should have a valid ped"))
	{
		const CPedWeaponManager* pWeaponManager = conditionData.pPed->GetWeaponManager();
		if(pWeaponManager)
		{
			const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
			if(pWeaponInfo)
			{
				return pWeaponInfo->GetIsTwoHanded() ? SCR_True : SCR_False;
			}
		}
	}
	return SCR_False;
}


////////////////////////////////////////////////////////////////////////////////////////

CScenarioCondition::Result CScenarioConditionUsingLRAltClipset::CheckInternal(const sScenarioConditionData& conditionData) const
{
	if(aiVerifyf(conditionData.pPed, "CScenarioConditionUsingLRAltClipset should have a valid ped"))
	{
		if (conditionData.pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans))
		{
			return SCR_True;
		}
	}
	return SCR_False;
}

////////////////////////////////////////////////////////////////////////////////////////