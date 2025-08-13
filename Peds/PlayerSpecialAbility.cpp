// FILE		: PlayerSpecialAbility.cpp
// PURPOSE	: Load in special ability information and manage the abilities
// CREATED	: 23-03-2011

// class header
#include "PlayerSpecialAbility.h"

// Framework headers
#include "fwsys/timer.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/system/CameraMetadata.h"
#include "control/replay/replay.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "game/ModelIndices.h"
#include "scene/datafilemgr.h"
#include "modelinfo/PedModelInfo.h"
#include "Ped.h"
#include "Peds/PedIntelligence.h"
#include "PlayerInfo.h"
#include "scene/EntityIterator.h"
#include "scene/ExtraContent.h"
#if __BANK
#include "scene/world/gameworld.h"
#endif // __BANK
#include "Stats/StatsInterface.h"
#include "Network/NetworkInterface.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/water.h"
#include "system/control.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Movement/TaskFall.h"
#include "task/System/TaskHelpers.h"
#include "vehicles/Vehicle.h"
#include "vehicles/Wheel.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "Frontend/MobilePhone.h"

// Parser headers
#include "PlayerSpecialAbility_parser.h"

// rage headers
#include "bank/bank.h"
#include "fwsys/fileExts.h"
#include "parser/manager.h"
#include "string/stringhash.h"


AI_OPTIMISATIONS()

u32 s_specialEditionHash = ATSTRINGHASH("dlc_specialedition", 0x2040A77);
u32 s_collectorsEditionHash = ATSTRINGHASH("dlc_collectorsedition", 0x4f98c2a7);

//////////////////////////////////////////////////////////////////////////
// CPlayerSpecialAbility
//////////////////////////////////////////////////////////////////////////

static const float MIN_DAMAGE_FOR_CHARGE = 10.f;
static const float MIN_RAMMED_BY_CAR_IMPACT = 5000.f;
static const float MIN_RAMMED_INTO_CAR_IMPACT = 5000.f;
static const float MIN_CRASHED_CAR_IMPACT = 15000.f;
static const u32 MIN_NEAR_VEHICLE_MISS_TIME = 800;
static const float MIN_NEAR_MISS_VELOCITY_DIFF_SQR = 200.f; // difference in vehicle velocities for the near miss to apply
static const u32 TIME_SPAN_FOR_SUCCESSFUL_LANDING = 300; // after a vehicle jump how long to wait for all wheels to touch ground to count as a 4 wheel landing
static const u32 TIME_AIRBORN_BEFORE_REWARD = 500; // time needed to be airborn before we give the reward to avoid small bounces
static const s32 MIN_REMAINING_CHARGE_FOR_ACTIVATION = 3; // minimum charge (in absolute time) required for ability activation to be possible

float CPlayerSpecialAbility::timeToAddCharge = 0.5f; // in seconds

#if __BANK
static bool sigmoidCurve = false;
static bool halfSigmoidCurve = true;
static bool linearCurve = false;

u32 CPlayerSpecialAbilityManager::unlockedCapacityPercentage = 30;

const char* abilityNames[] = 
{
    "Car slowdown",			// SAT_CAR_SLOWDOWN
    "Rage",					// SAT_RAGE
    "Bullet time",			// SAT_BULLET_TIME
    "Snapshot",				// SAT_SNAPSHOT
    "Insult",				// SAT_INSULT
	"Enraged",				// SAT_ENRAGED
	"Ghost",				// SAT_GHOST
	"Sprint Speed Boost",	// SAT_SPRINT_SPEED_BOOST
	"Nitro (Cop)",			// SAT_NITRO_COP
	"Nitro (Crook)",		// SAT_NITRO_CROOK
	"EMP",					// SAT_EMP
	"Spike Mine",			// SAT_SPIKE_MINE
	"Full Throttle",		// SAT_FULL_THROTTLE
	"Pumped Up",			// SAT_PUMPED_UP
	"Back Up",				// SAT_BACKUP
	"Oil Slick",			// SAT_OIL_SLICK
	"Kinetic Ram"			// SAT_KINETIC_RAM
};
static_assert(sizeof(abilityNames) / sizeof(char*) == SAT_MAX, "Mismatched number of Special Ability names");
#endif // __BANK



CPlayerSpecialAbility::CPlayerSpecialAbility(const CSpecialAbilityData* data) : 
data(data),
isEnabled(true),
isActive(false), 
addingContinuous(false), 
addContinuousMultiplier(0.f),
lastNearVehicleMissTime(0), 
nearMissChargeAmount(0.f),
appliedLowHealthCharge(false),
canToggle(true),
chargeToAdd(0.f),
currentRemaining(0.f),
sliceToAdd(0.f),
toggleTime(0),
lastTimeWarpFrame(0),
lastExplosionKillTime(0),
lastPedBumpedInto(NULL),
lastPedBumpTime(0),
lastPedRanOverTime(0),
accumulatedCharge(0.f),
unlockedCapacity(10.f),
vehJumpStartTime(0),
vehJumpTouchTime(0),
isVehJumpInProgress(false),
gaveKillCharge(false),
isFadingOut(false),
kineticRamActivationTime(0),
kineticRamExplosionCount(0)
{
	m_statAbility = StatsInterface::GetStatsModelHashId("SPECIAL_ABILITY");
	Assertf(StatsInterface::IsKeyValid(m_statAbility), "Player model %s should have SPECIAL_ABILITY stat since this model has ability type=%d.", FindPlayerPed() && FindPlayerPed()->GetPedModelInfo() ? FindPlayerPed()->GetPedModelInfo()->GetModelName() : "", data ? data->type : -1);
		
	if(!NetworkInterface::IsGameInProgress())
	{
		m_statUnlockedAbility = StatsInterface::GetStatsModelHashId("SPECIAL_ABILITY_UNLOCKED");
		Assertf(StatsInterface::IsKeyValid(m_statUnlockedAbility), "Player model %s should have SPECIAL_ABILITY_UNLOCKED stat since this model has ability type=%d.", FindPlayerPed() && FindPlayerPed()->GetPedModelInfo() ? FindPlayerPed()->GetPedModelInfo()->GetModelName() : "", data ? data->type : -1);
    
		unlockedCapacity = (float)data->initialUnlockedCap;
		if (StatsInterface::IsKeyValid(m_statUnlockedAbility))
		{
			s32 capacityStatValue = (s32)floor(100.f * (unlockedCapacity / data->duration));
			if (StatsInterface::GetIntStat(m_statUnlockedAbility) <= capacityStatValue)
				StatsInterface::SetStatData(m_statUnlockedAbility, capacityStatValue, STATUPDATEFLAG_ASSERTONLINESTATS); // the stat variable wants a percentage [0, 100] not absolute duration
			else
				unlockedCapacity = StatsInterface::GetIntStat(m_statUnlockedAbility) * 0.01f * data->duration;
		}

		UpdateFromStat();
	}
	else if (NetworkInterface::IsInCopsAndCrooks())
	{	
		unlockedCapacity = (float)data->initialUnlockedCap;
		currentRemaining = unlockedCapacity;
	}
	
}

void CPlayerSpecialAbility::UpdateFromStat()
{
	if (!Verifyf(StatsInterface::IsKeyValid(m_statAbility), "Player model %s should have SPECIAL_ABILITY stat since this model has ability type=%d.", FindPlayerPed() && FindPlayerPed()->GetPedModelInfo() ? FindPlayerPed()->GetPedModelInfo()->GetModelName() : "", data ? data->type : -1))
		return;

	currentRemaining = (float)StatsInterface::GetIntStat(m_statAbility);

	// fix special ability value in case we had a bad stored one
	if (currentRemaining >= unlockedCapacity)
		currentRemaining = unlockedCapacity;
	else if (currentRemaining <= 0.f)
		currentRemaining = 0.f;

	if (StatsInterface::IsKeyValid(m_statAbility))
	{
		StatsInterface::SetStatData(m_statAbility, (s32)floor(currentRemaining), STATUPDATEFLAG_ASSERTONLINESTATS);
	}
}

CPlayerSpecialAbility::~CPlayerSpecialAbility()
{
	m_ClipsetRequestHelper.Release();
}


s32 CPlayerSpecialAbility::GetTunedOrDefaultDuration(s32 abilityDuration) const
{
	// cloud tunable durations are in ms, so need to be converted to seconds if returned
	return abilityDuration >= 0 ? (s32)(abilityDuration * 0.001f) : data->duration;
}

s32 CPlayerSpecialAbility::GetDuration() const
{
	switch (data->type)
	{
	case SAT_ENRAGED:
		return GetTunedOrDefaultDuration(CPlayerSpecialAbilityManager::ms_CNCAbilityEnragedDuration);
	case SAT_GHOST:
		return GetTunedOrDefaultDuration(CPlayerSpecialAbilityManager::ms_CNCAbilityGhostDuration);
	case SAT_SPRINT_SPEED_BOOST:		
		return GetTunedOrDefaultDuration(CPlayerSpecialAbilityManager::ms_CNCAbilitySprintBoostDuration);		
	case SAT_BACKUP:
		return GetTunedOrDefaultDuration(CPlayerSpecialAbilityManager::ms_CNCAbilityCallDuration);		
	case SAT_NITRO_COP:		
	case SAT_NITRO_CROOK:
		return GetTunedOrDefaultDuration(CPlayerSpecialAbilityManager::ms_CNCAbilityNitroDuration);
	case SAT_OIL_SLICK:
		return GetTunedOrDefaultDuration(CPlayerSpecialAbilityManager::ms_CNCAbilityOilSlickDuration);
	case SAT_SPIKE_MINE:
		return GetTunedOrDefaultDuration(CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineDurationLifeTime);		
	default:
		break;
	}
	
	return data->duration;
}

float CPlayerSpecialAbility::GetDefenseMultiplier() const
{
	switch (data->type)
	{
		case SAT_ENRAGED:
			if (CPlayerSpecialAbilityManager::ms_CNCAbilityEnragedHealthReduction >= 0)
				return CPlayerSpecialAbilityManager::ms_CNCAbilityEnragedHealthReduction;
			break;
		default:
			break;
	}

	return data->defenseMultiplier;
}

float CPlayerSpecialAbility::GetStaminaMultiplier() const
{
	switch (data->type)
	{
		case SAT_ENRAGED:
			if (CPlayerSpecialAbilityManager::ms_CNCAbilityEnragedStaminaReduction >= 0)
				return CPlayerSpecialAbilityManager::ms_CNCAbilityEnragedStaminaReduction;
			break;
		default:
			break;
	}

	return data->staminaMultiplier;
}

float CPlayerSpecialAbility::GetSprintMultiplier() const
{
	switch (data->type)
	{
		case SAT_SPRINT_SPEED_BOOST:
			if (CPlayerSpecialAbilityManager::ms_CNCAbilitySprintBoostIncrease >= 0)
				return CPlayerSpecialAbilityManager::ms_CNCAbilitySprintBoostIncrease;
			break;
		default:
			break;
	}

	return data->sprintMultiplier;
}

void CPlayerSpecialAbility::Process(CPed *pPed)
{
	gaveKillCharge = false;

	if (!IsAbilityUnlocked())
	{
		currentRemaining = 0.f;
		chargeToAdd = 0.f;
		sliceToAdd = 0.f;
		return;
	}

	HandleVehicleJump();

	HandleKineticRam();

	float deltaTime = fwTimer::GetSystemTimeStep();

	// add small part of charge based on time delta
	if (chargeToAdd > 0.f)
	{
		if (currentRemaining < unlockedCapacity)
		{
			float addThisFrame = rage::Min(chargeToAdd, deltaTime * sliceToAdd);
			currentRemaining += addThisFrame;

			// the rage ability gets charges from taking damage, which happens a lot. we need to slow down ability unlock a bit
			if (GetType() == SAT_RAGE)
				addThisFrame *= 0.5f;

			accumulatedCharge += addThisFrame;
		}

		chargeToAdd = rage::Max(0.f, chargeToAdd - deltaTime * sliceToAdd);
	}
	else if (chargeToAdd < 0.f)
	{
		if (currentRemaining > 0.f)
		{
			float addThisFrame = rage::Max(chargeToAdd, deltaTime * sliceToAdd);
			currentRemaining += addThisFrame;
		}

		chargeToAdd = rage::Min(0.f, chargeToAdd - deltaTime * sliceToAdd);
	}
	else
	{
		sliceToAdd = 0.f;
	}

	if (addingContinuous)
	{
		if (currentRemaining < unlockedCapacity)
		{
			float chargeByTime = deltaTime * CPlayerSpecialAbilityManager::GetContinuousCharge() * data->chargeMultiplier * CPlayerSpecialAbilityManager::GetChargeMultiplier() * addContinuousMultiplier;
			currentRemaining += chargeByTime;
			accumulatedCharge += chargeByTime;
		}

		addContinuousMultiplier = 0.f;
		addingContinuous = false;
	}

	if (currentRemaining >= unlockedCapacity)
	{
		currentRemaining = unlockedCapacity;
		if (chargeToAdd > 0.f)
			chargeToAdd = 0.f;
	}
	else if (currentRemaining <= 0.f)
	{
		currentRemaining = 0.f;
		if (chargeToAdd < 0.f)
			chargeToAdd = 0.f;
	}

	if (StatsInterface::IsKeyValid(m_statAbility))
	{
		StatsInterface::SetStatData(m_statAbility, (s32)floor(currentRemaining), STATUPDATEFLAG_ASSERTONLINESTATS);
	}

	const u32 curTime = fwTimer::GetTimeInMilliseconds();
	const float dt = ((float)curTime) - toggleTime;

	bool chargeUnder50Percent = false;

	if (IsActive())
	{
		SetInternalTimeWarp(data->timeWarpScale);

		const fwMvClipSetId clipSetId(GetActiveAnimSetID());
		if (clipSetId != CLIP_SET_ID_INVALID)
			m_ClipsetRequestHelper.Request(clipSetId);

		// deactivate during cutscenes
		if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
		{
			ANIMPOSTFXMGR.Stop(data->fxName, AnimPostFXManager::kSpecialAbility);
			ANIMPOSTFXMGR.Stop(data->outFxName, AnimPostFXManager::kSpecialAbility);
			Deactivate();
			return;
		}

		if (Water::IsCameraUnderwater())
		{
			Deactivate();
			return;
		}

		if (data->type == SAT_CAR_SLOWDOWN && !FindPlayerPed()->GetIsInVehicle())
		{
			ANIMPOSTFXMGR.Stop(data->fxName, AnimPostFXManager::kSpecialAbility);
			ANIMPOSTFXMGR.Start(data->outFxName, 0U, false, false, false, 0U, AnimPostFXManager::kSpecialAbility);
			Deactivate();
			return;
		}

		float depleteByTime = data->depletionMultiplier * deltaTime;
		currentRemaining -= depleteByTime;

		if ((s32)floor(currentRemaining) <= 0)
		{
			currentRemaining = 0.f;
			if (StatsInterface::IsKeyValid(m_statAbility))
			{
				StatsInterface::SetStatData(m_statAbility, 0, STATUPDATEFLAG_ASSERTONLINESTATS);
			}
			ANIMPOSTFXMGR.Stop(data->fxName, AnimPostFXManager::kSpecialAbility);
			ANIMPOSTFXMGR.Start(data->outFxName, 0U, false, false, false, 0U, AnimPostFXManager::kSpecialAbility);
			Deactivate();
		}
		isFadingOut = false;
	}
	else if (dt <= (CPlayerSpecialAbilityManager::GetFadeOutTime() * 1000.f))
	{
		isFadingOut = true;

		if (pPed->IsLocalPlayer() && GetInternalTimeWarp() < 1.0f)
		{
			SetInternalTimeWarp(rage::Lerp(1.0f - GetFxStrength(), data->timeWarpScale, 1.f));
		}

		lastTimeWarpFrame = fwTimer::GetFrameCount();
	}
	else
	{
		if (lastTimeWarpFrame == fwTimer::GetFrameCount() - 1)
		{
			isFadingOut = false;
		}

		if (pPed->IsLocalPlayer())
		{
			SetInternalTimeWarp(1.f);
		}

		chargeUnder50Percent = true;
	}

	// only apply nearmiss, or the under 50% charge, when inactive
	if (chargeUnder50Percent)
	{
		if (lastNearVehicleMissTime > 0 && lastNearVehicleMissTime + MIN_NEAR_VEHICLE_MISS_TIME <= fwTimer::GetTimeInMilliseconds())
		{
			AddToMeterNormalized(nearMissChargeAmount, true);
			nearMissChargeAmount = 0.f;
			lastNearVehicleMissTime = 0;
		}
		else
		{
			// ability not active, slowly charge the meter to 50% (as displayed)
			float perc = (currentRemaining - MIN_REMAINING_CHARGE_FOR_ACTIVATION) / (unlockedCapacity - MIN_REMAINING_CHARGE_FOR_ACTIVATION);
			if (perc < 0.5f)
			{
				const float step = 0.5f / 120.f; // 50% over 2 minutes
				float chargeByTime = unlockedCapacity * step * deltaTime;
				currentRemaining = Min(currentRemaining + chargeByTime, 0.5f * (unlockedCapacity + MIN_REMAINING_CHARGE_FOR_ACTIVATION));
			}
		}
	}

	if (StatsInterface::IsKeyValid(m_statAbility))
	{
		StatsInterface::SetStatData(m_statAbility, (s32)floor(currentRemaining), STATUPDATEFLAG_ASSERTONLINESTATS);
	}

	// if we've accumulated 60% of the entire unlocked bar we increase the current capacity by 10% untill it's fully unlocked
	if (accumulatedCharge >= data->duration * 1.2f)
	{
		accumulatedCharge = 0.f;
		unlockedCapacity = rage::Min((float)data->duration, unlockedCapacity + (data->duration * 0.1f));

		// update SP stat data
		if (!NetworkInterface::IsGameInProgress())
		{
			if (StatsInterface::IsKeyValid(m_statUnlockedAbility))
			{
				// the stat variable wants a percentage [0, 100] not absolute duration
				StatsInterface::SetStatData(m_statUnlockedAbility, (s32)floor(100.f * (unlockedCapacity / data->duration)), STATUPDATEFLAG_ASSERTONLINESTATS);
			}
		}
	}

#if __BANK
	if (!NetworkInterface::IsGameInProgress())
	{
		if (StatsInterface::IsKeyValid(m_statUnlockedAbility))
		{
			CPlayerSpecialAbilityManager::unlockedCapacityPercentage = StatsInterface::GetIntStat(m_statUnlockedAbility);
		}
	}
 
#endif // __BANK
}

void CPlayerSpecialAbility::SetInternalTimeWarp(float fTimeWarp)
{
	fwTimer::SetTimeWarpSpecialAbility(fTimeWarp);
}

float CPlayerSpecialAbility::GetInternalTimeWarp() const
{
	return fwTimer::GetTimeWarpSpecialAbility();
}

CNetGamePlayer* GetNetGamePlayer(CPlayerSpecialAbility* playerSpecialAbility)
{
	if (!playerSpecialAbility) return NULL;
	
	CPed* pPlayerPed = FindPlayerPed();
	if (pPlayerPed)
	{
		CNetObjGame* pNetObject = pPlayerPed->GetNetworkObject();
		if (pNetObject)
		{
			CNetGamePlayer* pNetPlayer = pNetObject->GetPlayerOwner();
			if (pNetPlayer) return pNetPlayer;
		}
	}

	return NULL;

}

bool CPlayerSpecialAbility::CanBeActivatedInArcadeMode(CPed* pPed) const
{
	Assertf(pPed, "Attempting to validate ability use for arcade mode but ped is not valid.");

	// apply arcade mode specific exclusions
	if (NetworkInterface::IsInCopsAndCrooks())
	{
		if (pPed->GetIsOnFoot())
		{
			return data->allowsActivationOnFoot;
		}
		else if (data->allowsActivationInVehicle)
		{
			// in C&C, secondary slot is used for vehicle abilities		
			if (data->type == pPed->GetSpecialAbilityType(PSAS_SECONDARY))
			{				
				if (pPed->GetPlayerInfo() && pPed->GetIsDrivingVehicle())
				{
					// player may only use vehicle ability as the driver of their active vehicle
					if (pPed->GetVehiclePedInside() == pPed->GetPlayerInfo()->GetArcadeInformation().GetActiveVehicle())
					{
						return true;
					}
				}
			}
			else return true;
		}				
	}
	return false;
}

bool CPlayerSpecialAbility::CanBeSelectedInArcadeMode() const
{
	CPed* ped = FindPlayerPed();
	if (!ped)
	{
		return false;
	}

	return CanBeActivatedInArcadeMode(ped);
}

bool CPlayerSpecialAbility::CanBeActivated() const
{
	if ((NetworkInterface::IsGameInProgress() && (!isEnabled || isActive)) || (!NetworkInterface::IsGameInProgress() && (isActive || !IsAbilityUnlocked() || !isEnabled || (currentRemaining <= 0.f))))
	{
		return false;
	}

	if (Water::IsCameraUnderwater())
	{
		return false;
	}

	CPed* ped = FindPlayerPed();
	if (!ped)
	{
		return false;
	}

	// can't use if we have not accumulated the minimum charge required for activation
	// (get remaining is rounded to nearest otherwise it may never reach the minimum charge required for activation)
	if (GetRemaining(false) < GetMinimumChargeForActivation())
	{
		return false;
	}
		
	// check for arcade mode
	if (NetworkInterface::IsInArcadeMode() && !CanBeActivatedInArcadeMode(ped))
	{
		return false;
	}

	if (!NetworkInterface::IsGameInProgress()) // Fall checks for SP only
	{
		// turn off if we are going to fall further than our max fall height.
		const CTask* pTask = ped->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_FALL);
		if (pTask)
		{
			const CTaskFall* pFallTask = static_cast<const CTaskFall*>(pTask);
			if ((pFallTask->GetMaxFallDistance() > pFallTask->GetMaxPedFallHeight(ped)) || (pFallTask->GetState() == CTaskFall::State_Parachute) || (pFallTask->GetState() == CTaskFall::State_HighFall))
			{
				return false;
			}
		}

		if (ped->GetPedIntelligence()->GetQueriableInterface() && ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_HIGH_FALL))
		{
			return false;
		}
	}

	if (ped->GetArrestState() == ArrestState_Arrested)
	{
		return false;
	}

	if (!NetworkInterface::IsGameInProgress() && (GetRemaining() < MIN_REMAINING_CHARGE_FOR_ACTIVATION))
	{
		return false;
	}

	// apply ability specific exclusions
	switch (data->type)
	{
	case SAT_NONE:
		return false;
	case SAT_CAR_SLOWDOWN:
	{
		if (!ped->GetIsInVehicle())
			return false;

		CVehicle* veh = ped->GetMyVehicle();
		if (!veh || (veh->GetVehicleType() != VEHICLE_TYPE_CAR && veh->GetVehicleType() != VEHICLE_TYPE_BIKE && veh->GetVehicleType() != VEHICLE_TYPE_QUADBIKE && veh->GetVehicleType() != VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE))
			return false;
	}
	break;
	default:
		break;
	}

	return true;
}

bool CPlayerSpecialAbility::Activate()
{
	if (!CanBeActivated())
	{		
		if (NetworkInterface::IsGameInProgress())
		{
			if (CNetGamePlayer* pNetGamePlayer = GetNetGamePlayer(this))
			{
				GetEventScriptNetworkGroup()->Add(CEventNetworkPlayerSpecialAbilityFailedActivation(*pNetGamePlayer, data->type));
			}
		}
		return false;
	}		

	// validity of ped is checked in CanBeActivated
	CPed* ped = FindPlayerPed();

	switch (data->type)
	{
	case SAT_RAGE:
		{
			// Force stealth mode off if we are using rage
			if( ped->GetMotionData()->GetUsingStealth() )
				ped->GetMotionData()->SetUsingStealth( false );

			if(ped->GetSpeechAudioEntity())
				ped->GetSpeechAudioEntity()->PlayTrevorSpecialAbilitySpeech();
		}
		break;
	case SAT_BULLET_TIME:
		{
		}
		break;
	case SAT_SNAPSHOT:
		{
		}
		break;
	case SAT_INSULT:
		{
			//Say the combat taunt.
			audSpeechAudioEntity::MakeTrevorRage();

			//Grab the ped position.
			Vec3V vPedPosition = ped->GetTransform().GetPosition();

			//Iterate over the nearby peds.
			static float s_fMaxDistance = 50.0f;
			CEntityIterator pedIterator(IteratePeds, ped, &vPedPosition, s_fMaxDistance);
			for(CEntity* pNearbyEntity = pedIterator.GetNext(); pNearbyEntity; pNearbyEntity = pedIterator.GetNext())
			{
				//Grab the nearby ped.
				CPed* pNearbyPed = static_cast<CPed *>(pNearbyEntity);

				//Post the event.
				CEventCombatTaunt event(ped);
				pNearbyPed->GetPedIntelligence()->AddEvent(event);
			}
		}
		break;
	case SAT_ENRAGED:
		{
			// enraged is currently used in Arcade mode only
			if (NetworkInterface::IsInArcadeMode())
			{ 
				CPlayerInfo* playerInfo = ped->GetPlayerInfo();
				if (playerInfo)
				{
					// set values of the networked defense modifiers from ability data
					float abilityDefenseMultiplier = GetDefenseMultiplier();
					pedDebugf1("CPlayerSpecialAbility::Activate() [SAT_ENRAGED]: Applying defence multiplier (%f)", abilityDefenseMultiplier);
					playerInfo->SetPlayerWeaponDefenseModifier(abilityDefenseMultiplier);
					playerInfo->SetPlayerMeleeWeaponDefenseModifier(abilityDefenseMultiplier);
				}
			}
		}
		break;
	case SAT_KINETIC_RAM:
		{
			if (ped->GetVehiclePedInside())
			{
				// Everything else processed in HandleKineticRam
				StartKineticRam();
			}
		}
		break;
	default:
		break;
	}

	SetInternalTimeWarp(data->timeWarpScale);
	isActive = true;
	toggleTime = fwTimer::GetTimeInMilliseconds();
	
	ANIMPOSTFXMGR.Start(data->fxName, 0U, true, true, true, 0U, AnimPostFXManager::kSpecialAbility);

    chargeToAdd = 0.f;
    sliceToAdd = 0.f;

	if (NetworkInterface::IsGameInProgress())
	{
		if (CNetGamePlayer* pNetGamePlayer = GetNetGamePlayer(this))
		{
			GetEventScriptNetworkGroup()->Add(CEventNetworkPlayerActivatedSpecialAbility(*pNetGamePlayer, data->type));
		}
	}
	else
	{
		// Don't increment stats when the ability is activated in MP
		StatsInterface::IncrementStat(STAT_SPECIAL_ABILITY_ACTIVE_NUM.GetStatId(), 1.0f);
	}

	CPlayerSpecialAbilityManager::UpdateDlcMultipliers();

	return true;
}

void CPlayerSpecialAbility::Deactivate()
{
	if ((!NetworkInterface::IsGameInProgress() && (!isActive || !IsAbilityUnlocked() || !isEnabled)) || (NetworkInterface::IsInArcadeMode() && !isActive))
	{
		return;
	}

	m_ClipsetRequestHelper.Release();

	switch (data->type)
	{
	case SAT_NONE:
		return;
	case SAT_CAR_SLOWDOWN:
	{
	}
	break;
	case SAT_RAGE:
	{
	}
	break;
	case SAT_BULLET_TIME:
	{
	}
	break;
	case SAT_SNAPSHOT:
	{
	}
	break;
	case SAT_INSULT:
	{
	}
	break;
	case SAT_ENRAGED:
	{
		// enraged is currently used in Arcade mode only
		CPed* ped = FindPlayerPed();
		if (ped && NetworkInterface::IsInArcadeMode())
		{
			CPlayerInfo* playerInfo = ped->GetPlayerInfo();
			if (playerInfo)
			{
				// restore default
				pedDebugf1("CPlayerSpecialAbility::Deactivate() [SAT_ENRAGED]: Restoring defence multiplier to 1.0");
				playerInfo->SetPlayerWeaponDefenseModifier(1.0f);
				playerInfo->SetPlayerMeleeWeaponDefenseModifier(1.0f);
			}
		}
	}
	break;	
	default:
		break;
	}
	
	if (NetworkInterface::IsGameInProgress())
	{
		if (CNetGamePlayer* pNetGamePlayer = GetNetGamePlayer(this))
		{
			GetEventScriptNetworkGroup()->Add(CEventNetworkPlayerDeactivatedSpecialAbility(*pNetGamePlayer, data->type));
		}
	}
	else if (fwTimer::GetTimeInMilliseconds() > toggleTime)
	{
		StatsInterface::IncrementStat(STAT_SPECIAL_ABILITY_ACTIVE_TIME.GetStatId(), (float)(fwTimer::GetTimeInMilliseconds() - toggleTime));
	}
	
	isActive = false;
	toggleTime = fwTimer::GetTimeInMilliseconds();
	
}

void CPlayerSpecialAbility::DeactivateNoFadeOut()
{
	if (!NetworkInterface::IsGameInProgress() && (!isActive || !IsAbilityUnlocked() || !isEnabled))
	{
        // do these resets here too, in case this is called after the regular Deactivate and we need to kill the fade
        SetInternalTimeWarp(1.f);

        u32 curTime = fwTimer::GetTimeInMilliseconds();
        toggleTime = curTime - (u32)(CPlayerSpecialAbilityManager::GetFadeOutTime() * 1000.f);

		return;
	}

	Deactivate();
	SetInternalTimeWarp(1.f);

	//Set the toggle time as though the fade out has finished
	u32 curTime = fwTimer::GetTimeInMilliseconds();
	toggleTime = curTime - (u32)(CPlayerSpecialAbilityManager::GetFadeOutTime() * 1000.f);
}

void CPlayerSpecialAbility::Toggle(CPed* ped)
{
	if (!IsAbilityUnlocked() || !canToggle || !isEnabled)
	{
		return;
	}

	if (isActive)
	{
		ANIMPOSTFXMGR.Stop(data->fxName, AnimPostFXManager::kSpecialAbility);
		ANIMPOSTFXMGR.Start(data->outFxName, 0U, false, false, false, 0U, AnimPostFXManager::kSpecialAbility);
		Deactivate();
	}
	else
	{
		// don't allow activation under certain circumstances
		if (ped->IsDead())
		{
			return;
		}

		Activate();
	}

	canToggle = false;
}

void CPlayerSpecialAbility::Reset()
{
	Deactivate();
	currentRemaining = 0.f;
	chargeToAdd = 0.f;
	sliceToAdd = 0.f;
    accumulatedCharge = 0.f;

	if (StatsInterface::IsKeyValid(m_statAbility))
	{
		StatsInterface::SetStatData(m_statAbility, 0, STATUPDATEFLAG_ASSERTONLINESTATS);
	}
}

bool CPlayerSpecialAbility::IsMeterFull() const
{
	s32 remaining = 0;

	if (StatsInterface::IsKeyValid(m_statAbility))
	{
		remaining = StatsInterface::GetIntStat(m_statAbility);
	}

    return remaining == (s32)floor(unlockedCapacity);
}

void CPlayerSpecialAbility::FillMeter(bool ignoreActive)
{
	if (!IsAbilityUnlocked() || (!ignoreActive && isActive))
	{
		return;
	}

	chargeToAdd = unlockedCapacity;
	sliceToAdd = chargeToAdd / timeToAddCharge;
}

void CPlayerSpecialAbility::DepleteMeter(bool ignoreActive)
{
	if (!IsAbilityUnlocked() || (!ignoreActive && isActive))
	{
		return;
	}

	s32 remaining = 0;
	
	if (StatsInterface::IsKeyValid(m_statAbility))
	{
		remaining = StatsInterface::GetIntStat(m_statAbility);
	}

	chargeToAdd = -(float)remaining;
	sliceToAdd = chargeToAdd / timeToAddCharge;
}

void CPlayerSpecialAbility::AddToMeter(s32 value, bool ignoreActive)
{
	if (!IsAbilityUnlocked() || (!ignoreActive && isActive))
	{
		return;
	}

    if (value > 0)
        value = (s32)(value * data->chargeMultiplier * CPlayerSpecialAbilityManager::GetChargeMultiplier());

	chargeToAdd += value;
	chargeToAdd = chargeToAdd > -currentRemaining ? chargeToAdd : -currentRemaining;
	sliceToAdd = chargeToAdd / timeToAddCharge;
}

void CPlayerSpecialAbility::AddToMeterNormalized(float value, bool ignoreActive, bool applyLocalMultiplier)
{
	if (!IsAbilityUnlocked() || (!ignoreActive && isActive))
	{
		return;
	}

    if (value > 0.f)
    {
		if (applyLocalMultiplier)
			value *= data->chargeMultiplier;

        value *= CPlayerSpecialAbilityManager::GetChargeMultiplier();
        value = rage::Min(1.f, value);
        value = rage::Max(0.f, value);
    }

	chargeToAdd += value * unlockedCapacity;
	chargeToAdd = chargeToAdd > -currentRemaining ? chargeToAdd : -currentRemaining;
	sliceToAdd = chargeToAdd / timeToAddCharge;
}

void CPlayerSpecialAbility::AddToMeterContinuous(bool ignoreActive, float multiplier)
{
	if (!IsAbilityUnlocked() || (!ignoreActive && isActive))
	{
		return;
	}

	addingContinuous = true;
	addContinuousMultiplier = rage::Max(addContinuousMultiplier, multiplier);
}

s32 CPlayerSpecialAbility::GetRemaining(bool roundDown) const
{
	if (StatsInterface::IsKeyValid(m_statAbility))
	{
		Assertf(currentRemaining <= unlockedCapacity, "Current special ability charge is larger than what unlocked bar allows! (%f / %f)", currentRemaining, unlockedCapacity);

		if (roundDown)
		{
			return (s32)floor(currentRemaining);
		}
		else
		{
			return (s32)floor(currentRemaining + 0.5f);
		}
        
	}

	return 0;
}

float CPlayerSpecialAbility::GetRemainingNormalized() const
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return replayRemainingNormalized;
	}
#endif

    return currentRemaining / unlockedCapacity;
}

float CPlayerSpecialAbility::GetRemainingPercentageForDisplay() const
{
	// Any charge below MIN_REMAINING_CHARGE_FOR_ACTIVATION is displayed as no charge through the HUD 

	if (GetRemaining() < MIN_REMAINING_CHARGE_FOR_ACTIVATION)
	{
		return 0.0f;
	}

	float fRemainingNormalized = (currentRemaining - MIN_REMAINING_CHARGE_FOR_ACTIVATION) / (unlockedCapacity - MIN_REMAINING_CHARGE_FOR_ACTIVATION);
	fRemainingNormalized = rage::Max(fRemainingNormalized, 0.01f);

	return (fRemainingNormalized * GetCapacityPercentageForDisplay());
}

float CPlayerSpecialAbility::GetCapacity() const
{
	return unlockedCapacity / data->duration;
}

float CPlayerSpecialAbility::GetCapacityPercentageForDisplay() const
{
	// Adjusted to account for charge below MIN_REMAINING_CHARGE_FOR_ACTIVATION being displayed as no charge.

	return ((unlockedCapacity - MIN_REMAINING_CHARGE_FOR_ACTIVATION) / (data->duration - MIN_REMAINING_CHARGE_FOR_ACTIVATION)) * 100.0f;
}

void CPlayerSpecialAbility::GetSpecialAbilityFx(atHashString &fxmod) const
{
	fxmod = data->fxName;
}

float CPlayerSpecialAbility::GetFxStrength() const
{
	u32 curTime = fwTimer::GetTimeInMilliseconds();
	float dt = ((float)curTime) - toggleTime;

    float timeRange = IsActive() ? CPlayerSpecialAbilityManager::GetFadeInTime() : CPlayerSpecialAbilityManager::GetFadeOutTime();
	float fHalfSigmoid = IsActive() ? CPlayerSpecialAbilityManager::GetHalfSigmoidConstant() : -1.0f - CPlayerSpecialAbilityManager::GetHalfSigmoidConstant();

	float strength = 0.f;
    float x = dt / (timeRange * 1000.f);

    switch (CPlayerSpecialAbilityManager::GetFadeCurveType())
    {
        case FCT_LINEAR:
            strength = x;
            break;
        case FCT_HALF_SIGMOID:
            x = rage::Min(x, 1.f);
            strength = (fHalfSigmoid * x) / (fHalfSigmoid - x + 1.f);
        break;
        case FCT_SIGMOID:
            x *= 2.f;
            strength = x / (Sqrtf(CPlayerSpecialAbilityManager::GetSigmoidConstant() + x*x));
        break;
        case FCT_NONE:
        default:
            strength = 1.f;
    }

    strength = rage::Min(strength, 1.f);
    strength = rage::Max(strength, 0.f);

	if (!IsActive())
		strength = 1.f - strength;

	return strength;
}

bool CPlayerSpecialAbility::ShouldApplyFx() const
{
	if( CNetwork::IsGameInProgress() )
	{
		return false;
	}

	u32 curTime = fwTimer::GetTimeInMilliseconds();
	float dt = ((float)curTime) - toggleTime;

    if (CPlayerSpecialAbilityManager::GetFadeCurveType() != FCT_NONE)
    {
        if (!IsActive() && dt > (CPlayerSpecialAbilityManager::GetFadeOutTime() * 1000.f))
		{
			ANIMPOSTFXMGR.Stop(data->fxName, AnimPostFXManager::kSpecialAbility);
			ANIMPOSTFXMGR.Stop(data->outFxName, AnimPostFXManager::kSpecialAbility);
            return false;
		}
        return true;
    }

    if (!IsActive())
	{
		ANIMPOSTFXMGR.Stop(data->fxName, AnimPostFXManager::kSpecialAbility);
		ANIMPOSTFXMGR.Stop(data->outFxName, AnimPostFXManager::kSpecialAbility);
		return false;
	}

    return true;
}

bool CPlayerSpecialAbility::IsPlayingFx() const
{
	return ANIMPOSTFXMGR.IsRunning(data->fxName) || ANIMPOSTFXMGR.IsRunning(data->outFxName);
}

void CPlayerSpecialAbility::ApplyFx() const
{
	if (ShouldApplyFx())
	{
		float strength = GetFxStrength();
		ANIMPOSTFXMGR.SetUserFadeLevel(data->fxName, strength);
	}
}

void CPlayerSpecialAbility::StartFx() const
{
	ANIMPOSTFXMGR.Start(data->fxName, 0U, true, true, true, 0U, AnimPostFXManager::kSpecialAbility);
}

void CPlayerSpecialAbility::StopFx() const
{
	ANIMPOSTFXMGR.Stop(data->fxName, AnimPostFXManager::kSpecialAbility);
	ANIMPOSTFXMGR.Stop(data->outFxName, AnimPostFXManager::kSpecialAbility);
}


void CPlayerSpecialAbility::TriggerNearVehicleMiss()
{
	if (!IsAbilityUnlocked())
	{
		return;
	}

	// when a near miss is triggered, the time is recorded and a charge is awarded only after
	// a predefined amount of time has passed. This can be aborted if a crash happens in the mean time,
	// i.e. the near miss was a bit too near.
	// 0 works as a reset value for the time variable
    // moreover, the charge accumulates to give a larger charge if many near misses took place before the next issue
	if (lastNearVehicleMissTime == 0)
	{
		lastNearVehicleMissTime = fwTimer::GetTimeInMilliseconds();
	}
	nearMissChargeAmount += 0.0325f;
}

void CPlayerSpecialAbility::AbortNearVehicleMiss()
{
	lastNearVehicleMissTime = 0;
    nearMissChargeAmount = 0.f;
}

void CPlayerSpecialAbility::TriggerLowHealth()
{
	if (!IsAbilityUnlocked())
	{
		return;
	}

	if (!appliedLowHealthCharge)
	{
		AddToMeterNormalized(0.065f, true);
		appliedLowHealthCharge = true;
	}
}

void CPlayerSpecialAbility::AbortLowHealth()
{
	appliedLowHealthCharge = false;
}

void CPlayerSpecialAbility::TriggerExplosionKill()
{
	// allow only an explosion kill charge per 3 seconds (to not get one for each kill)
	if (fwTimer::GetTimeInMilliseconds() > lastExplosionKillTime + 3000)
	{
		AddToMeterNormalized(0.04875f, false);
		lastExplosionKillTime = fwTimer::GetTimeInMilliseconds();
	}
}

void CPlayerSpecialAbility::TriggerBumpedIntoPed(void* pedEa)
{
	// restrict ped bumps to one every 3 seconds, or one for each ped but not more often than 1 per second,
	// the bump is triggered by collision so it's continuous if you touch a ped, can't have it trigger
	// all the time like that or the meter gets filled real quick
	u32 currentTime = fwTimer::GetTimeInMilliseconds();
	if ((currentTime > lastPedBumpTime + 3000) || (pedEa != lastPedBumpedInto && currentTime > lastPedBumpTime + 1000))
	{
		AddToMeterNormalized(0.0325f, false);
		lastPedBumpedInto = pedEa;
		lastPedBumpTime = currentTime;
	}
}

void CPlayerSpecialAbility::TriggerRanOverPed(float damage)
{
    if (damage > 0.f)
    {
        // can only run over peds once per second, or we might end up filling a whole bar when the player keeps
        // driving into a ped
        u32 currentTime = fwTimer::GetTimeInMilliseconds();
        if (currentTime > lastPedRanOverTime + 1000)
        {
            AddToMeterNormalized(0.00325f, false);
            lastPedRanOverTime = currentTime;
        }
    }
}

void CPlayerSpecialAbility::TriggerVehicleJump(CVehicle* pVeh)
{
	if (!pVeh || isVehJumpInProgress || isActive)
		return;

	vehJumpStartTime = fwTimer::GetTimeInMilliseconds();
	vehJumpTouchTime = 0;

	isVehJumpInProgress = true;
	jumpVeh = pVeh;
}

void CPlayerSpecialAbility::HandleVehicleJump()
{
    if (isVehJumpInProgress && jumpVeh)
    {
        u32 duration = fwTimer::GetTimeInMilliseconds() - vehJumpStartTime;

        // abort jump reward if xy velocity is lower than z velocity, vehicle is just falling down
        const Vector3& vel = jumpVeh->GetVelocity();
        if ((vel.x * vel.x + vel.y * vel.y) < vel.z * vel.z)
        {
            isVehJumpInProgress = false;
        }
        else
        {
            // after a certain period we start giving continuous rewards while airborn and before any wheel has touched the ground
            if (vehJumpTouchTime == 0 && duration > TIME_AIRBORN_BEFORE_REWARD)
                AddToMeterContinuous(false, 0.05f);

            bool allWheelsTouching = true;
            s32 numWheels = jumpVeh->GetNumWheels();
            for (s32 i = 0; i < numWheels; ++i)
            {
                CWheel* wheel = jumpVeh->GetWheel(i);
                if (wheel)
                {
                    if (wheel->GetIsTouching())
                    {
                        // if a wheel touches the ground too early we abort the reward
                        if (duration < TIME_AIRBORN_BEFORE_REWARD)
                        {
                            allWheelsTouching = false;
                            isVehJumpInProgress = false;
                            break;
                        }

                        // record the time of the first wheel touching the ground
                        if (vehJumpTouchTime == 0)
                            vehJumpTouchTime = fwTimer::GetTimeInMilliseconds();
                    }
                    else
                    {
                        allWheelsTouching = false;
                    }
                }
            }

            if (allWheelsTouching)
            {
                // we give the reward if all wheels touch the ground after we've been airborn for a certain amount of time
                // and all wheels touched the ground within a certain period
                if (duration > TIME_AIRBORN_BEFORE_REWARD && vehJumpTouchTime + TIME_SPAN_FOR_SUCCESSFUL_LANDING >= fwTimer::GetTimeInMilliseconds())
                {
                    AddToMeterNormalized(0.0325f, false);
                }

                isVehJumpInProgress = false;
            }
            else
            {
                // if all wheels haven't touched the ground within the time span set, we fail the landing
                if (vehJumpTouchTime && vehJumpTouchTime + TIME_SPAN_FOR_SUCCESSFUL_LANDING < fwTimer::GetTimeInMilliseconds())
                {
                    isVehJumpInProgress = false;
                }
            }
        }
    }
    else if (isVehJumpInProgress && !jumpVeh)
    {
        isVehJumpInProgress = false;
    }
}

bool CPlayerSpecialAbility::IsAbilityUnlocked() const
{
	return CPlayerSpecialAbilityManager::IsAbilityUnlocked(FindPlayerPed()->GetPedModelInfo()->GetModelNameHash());
}

void CPlayerSpecialAbility::ProcessVfx()
{
	if (IsActive())
	{
		if (data->type == SAT_CAR_SLOWDOWN)
		{
			CPed* pPlayerPed = FindPlayerPed();
			if (pPlayerPed && pPlayerPed->GetIsInVehicle())
			{
				CVehicle* pPlayerVehicle = pPlayerPed->GetMyVehicle();
				if (pPlayerVehicle)
				{
					CVehicleModelInfo* pPlayerVehicleModelInfo = pPlayerVehicle->GetVehicleModelInfo();
					if (pPlayerVehicleModelInfo)
					{
						s32	boneIdx = pPlayerVehicleModelInfo->GetBoneIndex(VEH_TAILLIGHT_L);
						g_vfxVehicle.UpdatePtFxLightTrail(pPlayerVehicle, boneIdx, 0);

						boneIdx = pPlayerVehicleModelInfo->GetBoneIndex(VEH_TAILLIGHT_R);
						g_vfxVehicle.UpdatePtFxLightTrail(pPlayerVehicle, boneIdx, 1);
					}
				}
			}
		}
	}
}

void CPlayerSpecialAbility::StartKineticRam()
{
	kineticRamActivationTime = fwTimer::GetTimeInMilliseconds();
	kineticRamExplosionCount = 0;
}

void CPlayerSpecialAbility::StopKineticRam()
{
	kineticRamActivationTime = 0;
	kineticRamExplosionCount = 0;
} 

void CPlayerSpecialAbility::HandleKineticRam()
{
	if (kineticRamActivationTime != 0)
	{
		static const eExplosionTag EXPLOSION_HASH = EXP_TAG_CNC_KINETICRAM;
		TUNE_GROUP_INT(KINETIC_RAM_TUNE, EXPLOSION_COUNT, 4, 1, 5, 1);
		TUNE_GROUP_FLOAT(KINETIC_RAM_TUNE, EXPLOSION_SPACING_DISTANCE, 2.0f, 0.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(KINETIC_RAM_TUNE, EXPLOSION_SPACING_TIMING, 0.075f, 0.0f, 10.0f, 0.01f);

		CPed* pPed = FindPlayerPed();
		if (pPed && pPed->GetVehiclePedInside())
		{
			CVehicle* pVehicle = pPed->GetVehiclePedInside();

			const u32 timeSinceTrigger = fwTimer::GetTimeInMilliseconds() - kineticRamActivationTime;
			if (timeSinceTrigger > (kineticRamExplosionCount * (u32)(EXPLOSION_SPACING_TIMING * 1000.0f)))
			{
				Vec3V vLocalExplosionPos = Vec3V(V_ZERO);

				// Adjust position to directly between the front two wheels
				const int wheelLFBoneIdx = pVehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_WHEEL_LF);
				const int wheelRFBoneIdx = pVehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_WHEEL_RF);
				if (wheelLFBoneIdx != VEH_INVALID_ID && wheelRFBoneIdx != VEH_INVALID_ID)
				{
					const Vec3V vWheelLFPos = pVehicle->GetSkeletonData().GetDefaultTransform(wheelLFBoneIdx).d();
					const Vec3V vWheelRFPos = pVehicle->GetSkeletonData().GetDefaultTransform(wheelRFBoneIdx).d();
					vLocalExplosionPos = (vWheelLFPos+vWheelRFPos)/ScalarV(2.0f);
				}
				else
				{
					// Backup for stuff like motorbikes
					const int chassisBoneIdx = pVehicle->GetVehicleModelInfo()->GetBoneIndex(VEH_CHASSIS);
					if (chassisBoneIdx != VEH_INVALID_ID)
					{
						vLocalExplosionPos = pVehicle->GetSkeletonData().GetDefaultTransform(chassisBoneIdx).d();
					}
				}
				
				// Offset forward based on explosion count
				const float fForwardOffset = kineticRamExplosionCount * EXPLOSION_SPACING_DISTANCE;
				vLocalExplosionPos.SetYf(vLocalExplosionPos.GetYf() + fForwardOffset);

				// Convert to world space
				Vec3V vWorldExplosionPos = pVehicle->TransformIntoWorldSpace(vLocalExplosionPos);

#if __BANK
				TUNE_GROUP_BOOL(KINETIC_RAM_TUNE, EXPLOSION_ORGINAL_POSITION_RENDER, false);
				if (EXPLOSION_ORGINAL_POSITION_RENDER)
				{
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(vWorldExplosionPos), 0.1f, Color_red, true, 30);
				}
#endif // __BANK

				// Adjust to ground via probe (within reasonable limit), cancel if we didn't find anything
				TUNE_GROUP_FLOAT(KINETIC_RAM_TUNE, EXPLOSION_GROUND_OFFSET_LIMIT, 1.0f, 0.0f, 5.0f, 0.01f);
				TUNE_GROUP_FLOAT(KINETIC_RAM_TUNE, EXPLOSION_GROUND_OFFSET_PROBE_HEIGHT, 20.0f, 0.0f, 5.0f, 0.01f);
				bool bFoundGround = false;
				const float fGroundZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, vWorldExplosionPos.GetXf(), vWorldExplosionPos.GetYf(), vWorldExplosionPos.GetZf() + EXPLOSION_GROUND_OFFSET_PROBE_HEIGHT, &bFoundGround);
				if (bFoundGround)
				{
					const float fAdjustmentDiff = Abs(vWorldExplosionPos.GetZf() - fGroundZ);
					if (fAdjustmentDiff < EXPLOSION_GROUND_OFFSET_LIMIT)
					{
						vWorldExplosionPos.SetZf(fGroundZ);
					}
					else
					{
						// Significant Z change? Cancel the effect
						StopKineticRam();
						return;
					}
				}
				else
				{
					// No ground? Cancel the effect
					StopKineticRam();
					return;
				}
#if __BANK
				TUNE_GROUP_BOOL(KINETIC_RAM_TUNE, EXPLOSION_ADJUSTED_POSITION_RENDER, false);
				if (EXPLOSION_ADJUSTED_POSITION_RENDER)
				{
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(vWorldExplosionPos), 0.1f, Color_green, true, 30);
				}
#endif // __BANK

				// Shockwave VFX on the first explosion only
				const bool bNoVfx = kineticRamExplosionCount != 0;

				// Grab the radius of the last explosion, scale earlier ones down based on that (we can't scale up due to clamps...)
				const CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(EXPLOSION_HASH);
				TUNE_GROUP_FLOAT(KINETIC_RAM_TUNE, EXPLOSION_START_RADIUS, 1.5f, 0.0f, 50.0f, 0.01f);
				const float fScaleRatio = EXPLOSION_START_RADIUS / expTagData.endRadius;
				const float t = kineticRamExplosionCount / (float)(EXPLOSION_COUNT - 1);
				const float fExpScale = Lerp(t, fScaleRatio, 1.0f);

				// Create explosion
				CExplosionManager::CExplosionArgs explosionArgs(EXPLOSION_HASH, VEC3V_TO_VECTOR3(vWorldExplosionPos));				
				explosionArgs.m_pEntExplosionOwner = pPed;
				explosionArgs.m_pEntIgnoreDamage = pVehicle;
				explosionArgs.m_bInAir = pVehicle->IsInAir();
				explosionArgs.m_pAttachEntity = pVehicle;
				explosionArgs.m_bAttachedToVehicle = true;
				explosionArgs.m_bNoFx = bNoVfx;
				explosionArgs.m_sizeScale = fExpScale;
				CExplosionManager::AddExplosion(explosionArgs);

				// TODO - Can we force attach VFX to the car as well so this looks better with fast motion?

				// Increment and check if we're all done
				kineticRamExplosionCount++;
				if (kineticRamExplosionCount == (u32)EXPLOSION_COUNT)
				{
					StopKineticRam();
				}
			}
		}
		else
		{
			// Something went wrong? Cancel the effect
			StopKineticRam();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CPlayerSpecialAbilityManager
//////////////////////////////////////////////////////////////////////////

// Static Manager object
CPlayerSpecialAbilityManager CPlayerSpecialAbilityManager::instance;
bool CPlayerSpecialAbilityManager::abilityUnlocked[5]; // michael, franklin, trevor, arcadeplayer, techtest

float CPlayerSpecialAbilityManager::specialChargeMultiplier = 1.f;
u32 CPlayerSpecialAbilityManager::pendingActivationTime1 = 0;
u32 CPlayerSpecialAbilityManager::pendingActivationTime2 = 0;
u32 CPlayerSpecialAbilityManager::timeForActivation = 0;
u32 CPlayerSpecialAbilityManager::activationTime = 100;
u32 CPlayerSpecialAbilityManager::activationTime_CNC = 150;
u32 CPlayerSpecialAbilityManager::audioActivationTime = 50;
u32 CPlayerSpecialAbilityManager::canTriggerAbilityTime = 0;
bool CPlayerSpecialAbilityManager::pendingActivation = false;
bool CPlayerSpecialAbilityManager::needsResetForActivation = false;
bool CPlayerSpecialAbilityManager::canTriggerAbility = false;
bool CPlayerSpecialAbilityManager::canUnTriggerAbility = false;
bool CPlayerSpecialAbilityManager::blockingInputs = false;
bool CPlayerSpecialAbilityManager::specialAbilityDisabledSprint = false;
bool CPlayerSpecialAbilityManager::activateSprintToggle = false;

// cloud tunables
s32 CPlayerSpecialAbilityManager::ms_CNCAbilityEnragedDuration = -1;
float CPlayerSpecialAbilityManager::ms_CNCAbilityEnragedHealthReduction = -1.f;
float CPlayerSpecialAbilityManager::ms_CNCAbilityEnragedStaminaReduction = -1.f;
s32 CPlayerSpecialAbilityManager::ms_CNCAbilityGhostDuration = -1;
s32 CPlayerSpecialAbilityManager::ms_CNCAbilitySprintBoostDuration = -1;
float CPlayerSpecialAbilityManager::ms_CNCAbilitySprintBoostIncrease = -1.f;
s32 CPlayerSpecialAbilityManager::ms_CNCAbilityCallDuration = -1;
s32 CPlayerSpecialAbilityManager::ms_CNCAbilityNitroDuration = -1;
s32 CPlayerSpecialAbilityManager::ms_CNCAbilityOilSlickDuration = -1;

s32 CPlayerSpecialAbilityManager::ms_CNCAbilityEMPVehicleDuration = -1;
s32 CPlayerSpecialAbilityManager::ms_CNCAbilityOilSlickVehicleDuration = -1;
float CPlayerSpecialAbilityManager::ms_CNCAbilityOilSlickFrictionReduction = -1.f;
float CPlayerSpecialAbilityManager::ms_CNCAbilityBullbarsAttackRadius = -1.f;
float CPlayerSpecialAbilityManager::ms_CNCAbilityBullbarsImpulseAmount = -1.f;
float CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineRadius = -1.0f;
s32 CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineDurationArming = -1;
s32 CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineDurationLifeTime = -1;

void CPlayerSpecialAbilityManager::Init(const char* fileName)
{
	abilityUnlocked[0] = false;
	abilityUnlocked[1] = false;
	abilityUnlocked[2] = false;
	abilityUnlocked[3] = false;
	abilityUnlocked[4] = false;


	pendingActivationTime1 = 0;
	pendingActivationTime2 = 0;
	canTriggerAbilityTime = 0;
	timeForActivation = 0;

	instance.chargeMultiplier = 1.f;

	Load(fileName);
}

void CPlayerSpecialAbilityManager::Shutdown()
{
	// Delete
	Reset();
}

CPlayerSpecialAbility* CPlayerSpecialAbilityManager::Create(SpecialAbilityType type)
{
	const CSpecialAbilityData* pData = GetSpecialAbilityData(type);
	if(pData)
	{
		return rage_new CPlayerSpecialAbility(pData);
	}
	return NULL;
}

void CPlayerSpecialAbilityManager::Destroy(CPlayerSpecialAbility* ability)
{
	ability->StopFx();
	delete ability;
}

s32 CPlayerSpecialAbilityManager::GetSmallCharge()
{
	return instance.smallCharge;
}

s32 CPlayerSpecialAbilityManager::GetMediumCharge()
{
	return instance.mediumCharge;
}

s32 CPlayerSpecialAbilityManager::GetLargeCharge()
{
	return instance.largeCharge;
}

s32 CPlayerSpecialAbilityManager::GetContinuousCharge()
{
	return instance.continuousCharge;
}

eFadeCurveType CPlayerSpecialAbilityManager::GetFadeCurveType()
{
    return instance.fadeCurveType;
}

float CPlayerSpecialAbilityManager::GetHalfSigmoidConstant()
{
    return instance.halfSigmoidConstant;
}

float CPlayerSpecialAbilityManager::GetSigmoidConstant()
{
    return instance.sigmoidConstant;
}

float CPlayerSpecialAbilityManager::GetFadeInTime()
{
    return instance.fadeInTime;
}

float CPlayerSpecialAbilityManager::GetFadeOutTime()
{
    return instance.fadeOutTime;
}

void CPlayerSpecialAbilityManager::UpdateDlcMultipliers()
{
	// update charge SE/CE charge multipliers, in case any content has been mounted/unmounted
	if (EXTRACONTENT.IsContentPresent(s_specialEditionHash) || EXTRACONTENT.IsContentPresent(s_collectorsEditionHash))
		CPlayerSpecialAbilityManager::SetSpecialChargeMultiplier(1.25f);
}

void CPlayerSpecialAbilityManager::ChargeEvent(AbilityChargeEventType eventType, CPed* eventPed, float fData, u32 iData, void* pData)
{
	if (!eventPed || !eventPed->GetSpecialAbility() || !IsAbilityUnlocked(eventPed->GetPedModelInfo()->GetModelNameHash()) || eventPed != FindPlayerPed())
	{
		return;
	}

	CPlayerSpecialAbility* ability = eventPed->GetSpecialAbility();
	SpecialAbilityType type = ability->GetType();

	switch (type)
	{
	case SAT_CAR_SLOWDOWN:
		{
			CPed* ped = FindPlayerPed();
			if (!ped)
				break;

			bool isInVehicle = ped->GetIsInVehicle();
			CVehicle* veh = ped->GetMyVehicle();
			if (veh && veh->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
				isInVehicle =  false;

			switch (eventType)
			{
			case ACET_NEAR_VEHICLE_MISS:
				if (!isInVehicle)
					break;
				if (iData + 2000 < fwTimer::GetTimeInMilliseconds() && fData > MIN_NEAR_MISS_VELOCITY_DIFF_SQR)
				{
				    ability->TriggerNearVehicleMiss();
				}
				break;
			case ACET_DRIFTING:
				if (!isInVehicle)
					break;
				ability->AddToMeterContinuous(false, 0.325f);
				break;
			case ACET_DRIVING_TOWARDS_ONCOMING:
			case ACET_DRIVING_IN_CINEMATIC:
			case ACET_TOP_SPEED:
				if (!isInVehicle)
					break;
				ability->AddToMeterContinuous(false, 0.325f);
                break;
			//case ACET_CRASHED_VEHICLE:
				//if (!isInVehicle)
					//break;
				//if (fData >= MIN_CRASHED_CAR_IMPACT)
				//{
					//ability->AddToMeterNormalized(-0.0325f, false);
				//}
				//ability->AbortNearVehicleMiss();
				//break;
			case ACET_VEHICLE_JUMP:
				if (!isInVehicle)
					break;
				ability->TriggerVehicleJump((CVehicle*)pData);
				break;
            case ACET_PICKUP:
                ability->AddToMeterNormalized(0.25f, true);
                break;
			case ACET_HEADSHOT:
				if (!ability->HasGivenKillCharge())
				{
					ability->AddToMeterNormalized(0.125f, false);
					ability->GiveKillCharge();
				}
				break;
			case ACET_KILL:
				if (!ability->HasGivenKillCharge())
				{
					ability->AddToMeterNormalized(0.04166f, false);
					ability->GiveKillCharge();
				}
			default:
				break;
			}
		}
		break;
	case SAT_BULLET_TIME:
	case SAT_SNAPSHOT:
		switch (eventType)
		{
		case ACET_RESET_LOW_HEALTH:
			ability->AbortLowHealth();
			break;
		case ACET_HEADSHOT:
			if (!ability->HasGivenKillCharge())
			{
				ability->AddToMeterNormalized(0.125f, false);
				ability->GiveKillCharge();
			}
			break;
		case ACET_CHANGED_HEALTH:
			// if health is regained, remove flag so we can get the charge when health is lost again
			if (fData > (eventPed->GetMaxHealth() * 0.25f))
			{
				ability->AbortLowHealth();
			}
			else
			{
				ability->TriggerLowHealth();
			}
			break;
		case ACET_STEALTH_KILL:
		case ACET_KNOCKOUT:
			if (!ability->HasGivenKillCharge())
			{
				ability->AddToMeterNormalized(0.125f, false);
				ability->GiveKillCharge();
			}
			break;
		case ACET_KILL:
			if (!ability->HasGivenKillCharge())
			{
				ability->AddToMeterNormalized(0.0625f, false);
				ability->GiveKillCharge();
			}
		case ACET_NEAR_VEHICLE_MISS:
			if (iData + 2000 < fwTimer::GetTimeInMilliseconds() && fData > MIN_NEAR_MISS_VELOCITY_DIFF_SQR)
			{
				ability->TriggerNearVehicleMiss();
			}
			break;
		case ACET_TOP_SPEED:
			ability->AddToMeterContinuous(false, 0.24f);
			break;
        case ACET_PICKUP:
            ability->AddToMeterNormalized(0.25f, true);
            break;
		default:
			break;
		}
		break;
	case SAT_RAGE:
	case SAT_INSULT:
		switch (eventType)
		{
		case ACET_RESET_LOW_HEALTH:
			ability->AbortLowHealth();
			break;
		case ACET_CHANGED_HEALTH:
			// if health is regained, remove flag so we can get the charge when health is lost again
			if (fData > (eventPed->GetMaxHealth() * 0.25f))
			{
				ability->AbortLowHealth();
			}
			else
			{
				ability->TriggerLowHealth();
			}
			break;
		case ACET_PUNCHED:
			ability->AddToMeterNormalized(0.065f, false);
			break;
		case ACET_WEAPON_TAKEDOWN:
			if (!ability->HasGivenKillCharge())
			{
				ability->AddToMeterNormalized(0.125f, true);
				ability->GiveKillCharge();
			}
			break;
		case ACET_HEADSHOT:
			if (!ability->HasGivenKillCharge())
			{
				ability->AddToMeterNormalized(0.125f, false);
				ability->GiveKillCharge();
			}
			break;
		case ACET_NEAR_VEHICLE_MISS:
			if (iData + 2000 < fwTimer::GetTimeInMilliseconds() && fData > MIN_NEAR_MISS_VELOCITY_DIFF_SQR)
			{
				ability->TriggerNearVehicleMiss();
			}
			break;
		case ACET_TOP_SPEED:
			ability->AddToMeterContinuous(false, 0.24f);
			break;
		case ACET_MISSION_FAILED:
			ability->AddToMeterNormalized(0.065f, true);
			break;
		case ACET_GOT_PUNCHED:
			ability->AddToMeterNormalized(0.065f, false);
			break;
		case ACET_EXPLOSION_KILL:
			if (!ability->HasGivenKillCharge())
			{
				ability->TriggerExplosionKill();
				ability->GiveKillCharge();
			}
			break;
		case ACET_KILL:
			if (!ability->HasGivenKillCharge())
			{
				ability->AddToMeterNormalized(0.0625f, false);
				ability->GiveKillCharge();
			}
			break;
        case ACET_PICKUP:
            ability->AddToMeterNormalized(0.25f, true);
            break;
		case ACET_DAMAGE:
            if (fData > 0.f)
            {
                ability->AddToMeterNormalized(fData * 0.002f, false, false); // 0.2% charge for each 1% of damage received
            }
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

bool CPlayerSpecialAbilityManager::IsAbilityUnlocked(u32 modelNameHash)
{
	if( modelNameHash == MI_PLAYERPED_PLAYER_ZERO.GetName().GetHash() ) // Michael
	{
		return abilityUnlocked[0];
	}
	else if( modelNameHash == MI_PLAYERPED_PLAYER_ONE.GetName().GetHash() ) // Franklin
	{
		return abilityUnlocked[1];
	}
	else if (modelNameHash == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash()) // Trevor
	{
		return abilityUnlocked[2];

	}
	else if (NetworkInterface::IsGameInProgress())
	{
		return abilityUnlocked[4];
	}

	return false;
}

void CPlayerSpecialAbilityManager::SetAbilityStatus(u32 modelNameHash, bool unlocked, bool isArcadePlayer)
{
	if( modelNameHash == MI_PLAYERPED_PLAYER_ZERO.GetName().GetHash() ) // Michael
	{
		abilityUnlocked[0] = unlocked;
	}
	else if( modelNameHash == MI_PLAYERPED_PLAYER_ONE.GetName().GetHash() ) // Franklin
	{
		abilityUnlocked[1] = unlocked;
	}
	else if( modelNameHash == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash() ) // Trevor
	{
		abilityUnlocked[2] = unlocked;
	}
	else if (NetworkInterface::IsGameInProgress() && isArcadePlayer)
	{
		abilityUnlocked[4] = unlocked;
	}
}

void CPlayerSpecialAbilityManager::ProcessControl(CPed* ped)
{
	if (!ped)
		return;

	CControl *pControl = ped->GetControlFromPlayer();
	if (!pControl)
		return;

	// only process control for the ability in the selected slot
	CPlayerSpecialAbility* ability = ped->GetSpecialAbility(ped->GetSelectedAbilitySlot());
	if (!ability)
		return;	

	if (ability->GetType() == SAT_CAR_SLOWDOWN || ability->GetType() == SAT_BULLET_TIME || ability->GetType() == SAT_RAGE)
	{
		// turn ability off during these tasks
		if(ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) ||
			ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) ||
			ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE))
		{
			ability->Deactivate();
			return;
		}

		if (!NetworkInterface::IsGameInProgress()) // Fall checks for SP only
		{
			// turn off if we are going to fall further than our max fall height.
			const CTask* pTask = ped->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_FALL);
			if(pTask)
			{
				const CTaskFall* pFallTask = static_cast<const CTaskFall*>(pTask);
				if( (pFallTask->GetMaxFallDistance() > pFallTask->GetMaxPedFallHeight(ped)) || (pFallTask->GetState() == CTaskFall::State_Parachute) ||  (pFallTask->GetState() == CTaskFall::State_HighFall))
				{
					ability->Deactivate();
					return;
				}
			}

			if(ped->GetPedIntelligence()->GetQueriableInterface() && ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_HIGH_FALL))
			{
				ability->Deactivate();
				return;
			}
		}

		if(ped->IsDead())
		{
			ability->Deactivate();
			return;
		}
    }

    bool canUseAbility = true;

    // car slowdown can only be used in cars
	if (ability->GetType() == SAT_CAR_SLOWDOWN && !ped->GetIsDrivingVehicle())
		canUseAbility = false;
	
    // bullet time and rage can't be used in vehicles
    if ((ability->GetType() == SAT_BULLET_TIME || ability->GetType() == SAT_RAGE) && ped->GetIsInVehicle() && !ped->GetPedResetFlag(CPED_RESET_FLAG_AllowSpecialAbilityInVehicle))
		canUseAbility = false;

	// rage can't be used if you have been killed by melee
	if (ability->GetType() == SAT_RAGE && ped->IsDeadByMelee())
		canUseAbility = false;

	// can't use if camera is in selfie mode
	if(CPhoneMgr::CamGetSelfieModeState())
	{
		canUseAbility = false;
		ability->Deactivate();
	}

    if (canUseAbility)
	{	
		if (ability->GetAllowsDeactivationByInput())
		{
			controlDebugf1("This ability allows deactivation by input");

			if (CanTriggerSpecialPedAbility())
			{
				ability->Toggle(ped);
			}
			else if (CanUnTriggerSpecialPedAbility())
			{
				ability->UnlockToggle();
			}
		}
		else if (CanTriggerSpecialPedAbility() && ability->IsAbilityUnlocked() && ability->IsAbilityEnabled() && !ability->IsActive() && !ped->IsDead())
		{
			controlDebugf1("This ability doesn't allow deactivation by input");

			ability->Activate();
		}
	}

	// In C&C we want to disable the currently active Vehicle ability if the player leaves their Personal Vehicle for any reason
	if (NetworkInterface::IsInArcadeMode() && NetworkInterface::IsInCopsAndCrooks())
	{
		// The Secondary slot is used for vehicle abilities
		CPlayerSpecialAbility* specialAbility = ped->GetSpecialAbility(PSAS_SECONDARY);
		if (specialAbility && specialAbility->IsActive())
		{
			// We can't activate the ability without being in our personal vehicle so all we care about here is we're still driving
			if (!ped->GetIsDrivingVehicle())
			{
				specialAbility->Deactivate();
				return;
			}
		}
	}
}

bool CPlayerSpecialAbilityManager::UpdateSpecialAbilityTrigger(CPed *ped)
{
	if (!ped /*|| !ped->GetSpecialAbility()*/)
		return false;

#if GTA_REPLAY
	// Do not process the player special ability when we are replaying a clip (url:bugstar:2036086).
	if(CReplayMgr::IsReplayInControlOfWorld())
	{
		return false;
	}
#endif // GTA_REPLAY

	CControl *pControl = ped->GetControlFromPlayer();
	if (!pControl)
		return false;

	u32 uActivationTime = NetworkInterface::IsInCopsAndCrooks() ? activationTime_CNC : activationTime;

	u32 potentialActivationTime = fwTimer::GetSystemTimeInMilliseconds() + uActivationTime;

	//! If time is rewound, reset timers.
	if( (pendingActivationTime1 > potentialActivationTime) ||
		(pendingActivationTime2 > potentialActivationTime) ||
		(canTriggerAbilityTime > potentialActivationTime) ||
		(timeForActivation > potentialActivationTime) )
	{
		pendingActivationTime1 = 0;
		pendingActivationTime2 = 0;
		canTriggerAbilityTime = 0;
		timeForActivation = 0;
		needsResetForActivation = false;
	}

	canTriggerAbility = false;
	canUnTriggerAbility = false;

	if(ped->GetIsDrivingVehicle())
	{
		CVehicle* veh = ped->GetMyVehicle();
		if(veh && (veh->GetIsAircraft() || veh->GetIsHeli()))
		{
			const CWeaponInfo *pCurrentWeaponInfo = ped->GetEquippedWeaponInfo();
			if(pCurrentWeaponInfo && pCurrentWeaponInfo->GetIsVehicleWeapon())
			{
				return false;
			}
		}
		if( veh && veh->HasJump() && CVehicle::sm_bDoubleClickForJump )
		{
			return false;
		}
	}

	u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();

	if(pControl->GetSpecialAbilityPC().IsDown())
	{
		canTriggerAbility = true;
	}
	else
	{
		if(pControl->GetPedSpecialAbility().IsDown() && !pControl->GetPedSpecialAbility().WasDown())
		{
			if(!pendingActivation && !needsResetForActivation)
			{
				timeForActivation = currentTime + uActivationTime;
			}

			pendingActivationTime1 = currentTime;
		}

		if (pControl->GetPedSpecialAbilitySecondary().IsDown() && !pControl->GetPedSpecialAbilitySecondary().WasDown())
		{
			if(!pendingActivation && !needsResetForActivation)
			{
				timeForActivation = currentTime + uActivationTime;
			}

			pendingActivationTime2 = currentTime;
		}		

		if( (currentTime < timeForActivation) )
		{
			pendingActivation = true;
			needsResetForActivation = true;
		}
		else
		{
			pendingActivation = false;
		}

		if( pendingActivation && 
			(abs((s32)pendingActivationTime1 - (s32)pendingActivationTime2) < uActivationTime) )
		{
			canTriggerAbility = true;
			canTriggerAbilityTime = currentTime;
		}
	}

	if(pControl->GetSpecialAbilityPC().WasDown())
	{
		canUnTriggerAbility = true;
	}
	else if ( timeForActivation > 0 && 
		(currentTime > timeForActivation ) &&
		(!pControl->GetPedSpecialAbility().IsDown() && !pControl->GetPedSpecialAbilitySecondary().IsDown()))
	{
		canUnTriggerAbility = true;
		needsResetForActivation = false;
	}

	//! Keep disabling controls once activated for a short time. This prevents other controls mapped to the same inputs from 
	//! triggering when re-activated.
	static dev_u32 nDisableTime = 1000;
	bool bHasActivated = currentTime < (canTriggerAbilityTime + nDisableTime);

	if(bHasActivated)
	{
		activateSprintToggle = false;
	}

	// Keep the horn enabled for Trevor and Michael when they are in vehicle.
	bool keepHornInputEnabled = false;
	bool bHornInputEnabled = pControl->GetVehicleHorn().IsEnabled();
	if(ped->GetPedModelInfo())
	{
		keepHornInputEnabled = keepHornInputEnabled || ((ped->GetPedModelInfo()->GetHashKey() == ATSTRINGHASH("Player_Zero", 0xD7114C9)) && ped->GetIsDrivingVehicle());
		keepHornInputEnabled = keepHornInputEnabled || ((ped->GetPedModelInfo()->GetHashKey() == ATSTRINGHASH("Player_Two", 0x9B810FA2)) && ped->GetIsDrivingVehicle());
	}

	// B*1868999: Keep horn input enabled in MP (as we don't have a special ability).
	if (NetworkInterface::IsGameInProgress())
	{
		keepHornInputEnabled = true;
	}

	if(pendingActivation || canTriggerAbility || bHasActivated )
	{
		// we disable input updates when unless the special ability has been activated. In this case it should act normal.
		ioValue::DisableOptions options = ioValue::DEFAULT_DISABLE_OPTIONS;
		options.SetFlags(ioValue::DisableOptions::F_DISABLE_UPDATE_WHEN_DISABLED, pendingActivation);

		bool bSprintPressed = pControl->GetPedSprint().IsPressed();
		bool bWasSprintDisabled = pControl->IsInputDisabled(INPUT_SPRINT);
	
		pControl->SetInputExclusive(INPUT_SPECIAL_ABILITY,  options);
		pControl->SetInputExclusive(INPUT_SPECIAL_ABILITY_SECONDARY, options);
		pControl->EnableFrontendInputs();

		if(!bWasSprintDisabled && bSprintPressed)
		{
			specialAbilityDisabledSprint = true;
		}

		//! Don't disable sprint if it is going to cause us to stop. Also, ensure we toggle sprint if we don't activate the special ability.
		if(specialAbilityDisabledSprint && pControl->IsToggleRunOn())
		{
			pControl->EnableInput(INPUT_SPRINT);
			if(!bHasActivated)
			{
				activateSprintToggle = true;
			}
		}

		if((keepHornInputEnabled || (!canTriggerAbility && !bHasActivated )) && bHornInputEnabled)
		{
			if(keepHornInputEnabled || ((currentTime - Max(pendingActivationTime1,pendingActivationTime2)) > audioActivationTime ))
			{
				pControl->EnableInput(INPUT_VEH_HORN);
			}
		}
		blockingInputs = true;
	}
	else
	{
		//! As soon as we stop pressing sprint, don't suppress sprint toggling anymore. Note: we need to wait for this as otherwise we may
		//! end up toggling sprint twice in quick succession.
		if(!pControl->GetPedSprint().IsDown())
		{
			specialAbilityDisabledSprint = false;
		}

		if(blockingInputs) 
		{
			blockingInputs = false;
		}
	}

	return true;
}

bool CPlayerSpecialAbilityManager::CanTriggerSpecialPedAbility()
{
	return canTriggerAbility;
}

bool CPlayerSpecialAbilityManager::CanUnTriggerSpecialPedAbility()
{
	return canUnTriggerAbility;
}

bool CPlayerSpecialAbilityManager::IsSpecialAbilityBlockingInputs()
{
	return blockingInputs;
}

bool CPlayerSpecialAbilityManager::HasSpecialAbilityDisabledSprint()
{
	return specialAbilityDisabledSprint;
}

bool CPlayerSpecialAbilityManager::CanActivateSprintToggle()
{
	return activateSprintToggle;
}

void CPlayerSpecialAbilityManager::Load(const char* fileName)
{
	if(Verifyf(!instance.specialAbilities.GetCount(), "Special abilities have already been loaded"))
	{
		// Reset/Delete the existing data before loading
		Reset();

		// Load
		fwPsoStoreLoader loader = fwPsoStoreLoader::MakeSimpleFileLoader<CPlayerSpecialAbilityManager>();
		fwPsoStoreLoadInstance inst(&instance);
		loader.GetFlagsRef().Set(fwPsoStoreLoader::RUN_POSTLOAD_CALLBACKS);
		char fullName[RAGE_MAX_PATH];
		fullName[0] = '\0';
		ASSET.AddExtension(fullName, RAGE_MAX_PATH, fileName, META_FILE_EXT);
		loader.Load(fullName, inst);

	}

	UpdateDlcMultipliers();
}

void CPlayerSpecialAbilityManager::Reset()
{
	instance.specialAbilities.Reset();

	pendingActivationTime1 = 0;
	pendingActivationTime2 = 0;
	canTriggerAbilityTime = 0;
	timeForActivation = 0;
}

#if __BANK
static s32 addAmount = 0;
static float addPercentage = 0.f;
static bool ignoreActive = false;
void ActivateCallback()
{
	CPed* playerPed = FindPlayerPed();
	CPlayerSpecialAbility* ability = playerPed->GetSpecialAbility();
	if (ability)
	{
		ability->Activate();
	}
}

void DeactivateCallback()
{
	CPed* playerPed = FindPlayerPed();
	CPlayerSpecialAbility* ability = playerPed->GetSpecialAbility();
	if (ability)
	{
		ability->Deactivate();
	}
};

void AddAmountCallback()
{
	CPed* playerPed = FindPlayerPed();
	CPlayerSpecialAbility* ability = playerPed->GetSpecialAbility();
	if (ability)
	{
		ability->AddToMeter(addAmount, ignoreActive);
	}
}

void AddNormalizedCallback()
{
	CPed* playerPed = FindPlayerPed();
	CPlayerSpecialAbility* ability = playerPed->GetSpecialAbility();
	if (ability)
	{
		ability->AddToMeterNormalized(addPercentage, ignoreActive);
	}
}

void FillMeterCallback()
{
	CPed* playerPed = FindPlayerPed();
	CPlayerSpecialAbility* ability = playerPed->GetSpecialAbility();
	if (ability)
	{
		ability->FillMeter(ignoreActive);
	}
}

void ResetAbilityCallback()
{
	CPed* playerPed = FindPlayerPed();
	CPlayerSpecialAbility* ability = playerPed->GetSpecialAbility();
	if (ability)
	{
		ability->Reset();
	}
}

void ResetAbilityStatCallback()
{
	CPed* playerPed = FindPlayerPed();
	CPlayerSpecialAbility* ability = playerPed->GetSpecialAbility();
	if (ability)
	{
		ability->ResetStat();
	}
}


void CPlayerSpecialAbilityManager::LoadCallback()
{
	CPlayerSpecialAbilityManager::Reset();
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_SPECIAL_ABILITIES_FILE);
	CPlayerSpecialAbilityManager::Load(pData->m_filename);
}

void CPlayerSpecialAbilityManager::SaveCallback()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_SPECIAL_ABILITIES_FILE);
	weaponVerifyf(PARSER.SaveObject(pData->m_filename, "meta", &instance, parManager::XML), "Failed to save special abilities file:%s", pData->m_filename);
}

void CPlayerSpecialAbilityManager::LinearCurveSelect()
{
    if (linearCurve)
    {
        sigmoidCurve = false;
        halfSigmoidCurve = false;

        instance.fadeCurveType = FCT_LINEAR;
    }
}

void CPlayerSpecialAbilityManager::SigmoidCurveSelect()
{
    if (sigmoidCurve)
    {
        linearCurve = false;
        halfSigmoidCurve = false;

        instance.fadeCurveType = FCT_SIGMOID;
    }
}

void CPlayerSpecialAbilityManager::HalfSigmoidCurveSelect()
{
    if (halfSigmoidCurve)
    {
        sigmoidCurve = false;
        linearCurve= false;

        instance.fadeCurveType = FCT_HALF_SIGMOID;
    }
}

void CPlayerSpecialAbilityManager::UnlockedCapacityCallback()
{
#if __BANK
	StatId capacityStat = StatsInterface::GetStatsModelHashId("SPECIAL_ABILITY_UNLOCKED");
	if (StatsInterface::IsKeyValid(capacityStat))
	{
        StatsInterface::SetStatData(capacityStat, (s32)unlockedCapacityPercentage, STATUPDATEFLAG_ASSERTONLINESTATS);

		if (CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->GetSpecialAbility())
			CGameWorld::FindLocalPlayer()->GetSpecialAbility()->unlockedCapacity = StatsInterface::GetIntStat(capacityStat) * 0.01f * CGameWorld::FindLocalPlayer()->GetSpecialAbility()->data->duration;
    }
#endif // __BANK
}

void CPlayerSpecialAbilityManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Player Special Abilities");
	
	bank.AddButton("Activate", datCallback(CFA(ActivateCallback)));
	bank.AddButton("Deactivate", datCallback(CFA(DeactivateCallback)));

	bank.AddText("Add amount", &addAmount);
	bank.AddButton("Add amount", datCallback(CFA(AddAmountCallback)));

	bank.AddText("Add normalized", &addPercentage);
	bank.AddButton("Add normalized", datCallback(CFA(AddNormalizedCallback)));

	bank.AddButton("Fill meter", datCallback(CFA(FillMeterCallback)));
	bank.AddButton("Reset ability", datCallback(CFA(ResetAbilityCallback)));
	bank.AddButton("Reset stat", datCallback(CFA(ResetAbilityStatCallback)));

	bank.AddToggle("Ignore active", &ignoreActive);

	bank.AddButton("Save", SaveCallback);
	bank.AddButton("Load", LoadCallback);

	bank.PushGroup("Special Abilities");
        for (s32 i = 0; i < SAT_MAX; ++i)
        {
            bank.PushGroup(abilityNames[i]);
                PARSER.AddWidgets(bank, &instance.specialAbilities[i]);
            bank.PopGroup();
        }
	bank.PopGroup();
	
	bank.AddToggle("Unlock Michael's ability", &abilityUnlocked[0]);
	bank.AddToggle("Unlock Franklin's ability", &abilityUnlocked[1]);
	bank.AddToggle("Unlock Trevor's ability", &abilityUnlocked[2]);
	bank.AddToggle("Unlock TechTest's ability", &abilityUnlocked[3]);
	bank.AddToggle("Unlock Arcae Player's ability", &abilityUnlocked[4]);

	bank.AddSlider("Unlocked capacity %", &CPlayerSpecialAbilityManager::unlockedCapacityPercentage, 0, 100, 1, datCallback(CFA(UnlockedCapacityCallback)));
	bank.AddSlider("Time to animate charge", &CPlayerSpecialAbility::timeToAddCharge, 0.f, 2.f, 0.01f);

	bank.AddSlider("Fade in time", &instance.fadeInTime, 0.f, 5.f, 0.01f);
	bank.AddSlider("Fade out time", &instance.fadeOutTime, 0.f, 5.f, 0.01f);

	bank.AddSlider("Activation Time", &activationTime, 0, 5000, 1);
	bank.AddSlider("Activation Time (CNC)", &activationTime_CNC, 0, 5000, 1);
	bank.AddSlider("Audio activation Time", &audioActivationTime, 0, 5000, 1);

    bank.AddToggle("Linear curve", &linearCurve, datCallback(CFA(LinearCurveSelect)));
    bank.AddToggle("Half-sigmoid curve", &halfSigmoidCurve, datCallback(CFA(HalfSigmoidCurveSelect)));
    bank.AddToggle("Sigmoid curve", &sigmoidCurve, datCallback(CFA(SigmoidCurveSelect)));
    bank.AddSlider("Half-sigmoid constant", &instance.halfSigmoidConstant, 0.1f, 10.f, 0.1f);
    bank.AddSlider("Sigmoid constant", &instance.sigmoidConstant, 0.1f, 5.f, 0.1f);

    bank.AddSlider("Charge multiplier", &instance.chargeMultiplier, -5.f, 5.f, 0.1f);
	bank.AddSlider("Special charge multiplier", &specialChargeMultiplier, 0.f, 10.f, 0.1f);
	bank.PopGroup(); 
}

void CPlayerSpecialAbility::ResetStat()
{
	Reset();

	unlockedCapacity = (float)data->initialUnlockedCap;
	
	// update SP stats
	if (!NetworkInterface::IsGameInProgress())
	{
		if (StatsInterface::IsKeyValid(m_statUnlockedAbility))
		{
			s32 capacityStatValue = (s32)floor(100.f * (unlockedCapacity / data->duration));
			if (StatsInterface::GetIntStat(m_statUnlockedAbility) <= capacityStatValue)
				StatsInterface::SetStatData(m_statUnlockedAbility, capacityStatValue, STATUPDATEFLAG_ASSERTONLINESTATS); // the stat variable wants a percentage [0, 100] not absolute duration
			else
				unlockedCapacity = StatsInterface::GetIntStat(m_statUnlockedAbility) * 0.01f * data->duration;
		}
	}
}

#endif // __BANK

void CPlayerSpecialAbilityManager::InitTunables()
{
	ms_CNCAbilityEnragedDuration = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_ENRAGED_DURATION", 0xC71F4CAA), -1);
	ms_CNCAbilityEnragedHealthReduction = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_ENRAGED_HEALTH_REDUCTION", 0xFAC3916D), -1.f);
	ms_CNCAbilityEnragedStaminaReduction = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_ENRAGED_STAMINA_REDUCTION", 0xB6A0B8A9), -1.f);
	ms_CNCAbilityGhostDuration = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_GHOST_DURATION", 0xE533F8FA), -1);
	ms_CNCAbilitySprintBoostDuration = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_SPRINT_BOOST_DURATION", 0x323E955C), -1);
	ms_CNCAbilitySprintBoostIncrease = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_SPRINT_BOOST_INCREASE", 0x180A6362), -1.f);
	ms_CNCAbilityCallDuration = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_CALL_DURATION", 0x58D72D98), -1);
	ms_CNCAbilityNitroDuration = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_NITRO_DURATION", 0x4D3A1F30), -1);
	ms_CNCAbilityOilSlickDuration = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_OIL_SLICK_DURATION", 0x483C8007), -1);

	ms_CNCAbilityEMPVehicleDuration = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_EMP_VEHICLE_DURATION", 0x711FD0B2), 5000);
	ms_CNCAbilityOilSlickVehicleDuration = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_OIL_SLICK_VEHICLE_DURATION", 0x40A30677), 5000);
	ms_CNCAbilityOilSlickFrictionReduction = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_OIL_SLICK_FRICTION_REDUCTION", 0xAD3FE512), -1.f);
	ms_CNCAbilityBullbarsAttackRadius = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_BULLBARS_ATTACK_RADIUS", 0xC06EF8FA), -1.f);
	ms_CNCAbilityBullbarsImpulseAmount = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_BULLBARS_IMPULSE_AMOUNT", 0x36D757DC), -1.f);
	ms_CNCAbilitySpikeMineRadius = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_SPIKE_MINE_RADIUS", 0xECFA7C17), -1.f);
	ms_CNCAbilitySpikeMineDurationArming = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_SPIKE_MINE_DURATION_ARMING", 0x89EBCEFC), -1);
	ms_CNCAbilitySpikeMineDurationLifeTime = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ABILITY_SPIKE_MINE_DURATION_LIFETIME", 0x41DAD91D), -1);
}

const CSpecialAbilityData* CPlayerSpecialAbilityManager::GetSpecialAbilityData(SpecialAbilityType type)
{
	if (type == SAT_NONE)
	{
		return NULL;
	}

	if((u32)type < instance.specialAbilities.GetCount())
	{
		return &instance.specialAbilities[(u32)type];
	}	

	Assertf(false, "Special ability type enum (%d) is larger than array count (%d)! Check pedspecialabilities.meta file.", type, instance.specialAbilities.GetCount());
	return NULL;
}

void CPlayerSpecialAbilityManager::PostLoad()
{
	for (u32 i = 0; i < instance.specialAbilities.GetCount(); ++i)
	{
		instance.specialAbilities[i].type = (SpecialAbilityType)i;
	}
}
