#include "debugaudio.h"
#include "grcore/debugdraw.h"

#include "ai/EntityScanner.h"
#include "camera/CamInterface.h"
#include "debug/DebugScene.h"
#include "gunfightconductor.h"
#include "northaudioengine.h"
#include "Network/NetworkInterface.h"
#include "Peds/PedIntelligence.h"
#include "profile/profiler.h"
#include "fwsys/timer.h"
#include "superconductor.h"
#include "scene/world/GameWorld.h"
#include "task/Weapons/Gun/TaskGun.h"
#include "task/Combat/TaskNewCombat.h"

AUDIO_OPTIMISATIONS()

//OPTIMISATIONS_OFF()
//-------------------------------------------------------------------------------------------------------------------
//PF_PAGE(GunFightConductorTimers,"GunFightsConductor");
//PF_GROUP(GunFightsConductor); 
//PF_LINK(GunFightConductorTimers, GunFightsConductor);
//PF_TIMER(CalculateIntensityMap,GunFightsConductor);
//PF_TIMER(CalculateThreatZones,GunFightsConductor);
//PF_TIMER(CalculatePedsInCombat,GunFightsConductor);
//PF_TIMER(ComputeFitness,GunFightsConductor);
//PF_TIMER(ComputeGunFightTargets,GunFightsConductor); 
//-------------------------------------------------------------------------------------------------------------------
//														INIT
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
bool audGunFightConductor::sm_ShowInfo = false;
bool audGunFightConductor::sm_ShowTargetPeds = false;
bool audGunFightConductor::sm_ShowPedsInCombat = false;
bool audGunFightConductor::sm_ShowGunfightVolOffset = false;
bool audGunFightConductor::sm_DebugMode = false;
bool audGunFightConductor::sm_ForceLowIntensityScene = false;
bool audGunFightConductor::sm_ForceMediumIntensityScene = false;
bool audGunFightConductor::sm_ForceHighIntensityScene = false;
CPed* audGunFightConductor::sm_LastFocusedEntity = NULL;
#endif
f32	audGunFightConductor::sm_DistanceThreshold = 30.f;
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::Init()
{	 
	m_NumberOfPedsInCombat = 0;
	for ( u32 i = 0; i <AUD_MAX_NUM_PEDS_IN_COMBAT; i++ )
	{
		m_PedsInCombat[i].pPed = NULL;
		m_PedsInCombat[i].fitness = 1.f;
	}
	m_NumberOfTargets = 0;
	for ( s32 i = 0; i < MaxNumConductorsTargets; i++ )
	{
		m_CombatTargetPeds[i].fitness = -1.f;
		m_CombatTargetPeds[i].pPed = NULL;
	}
	m_CombatMovementToFitness.Init(ATSTRINGHASH("MOVEMENT_TO_FITNESS", 0xFC308D12));
	m_CombatAbilityToFitness.Init(ATSTRINGHASH("ABILITY_TO_FITNESS", 0xBF21955A));
	m_CombatRangeToFitness.Init(ATSTRINGHASH("RANGE_TO_FITNESS", 0xF5F3F99F));
	m_PositionToFitness.Init(ATSTRINGHASH("POSITION_TO_FITNESS", 0xDF57B9AE));
	m_DistanceToFitness.Init(ATSTRINGHASH("DISTANCE_TO_FITNESS", 0x81B995A5));
	m_DamageTypeToFitness.Init(ATSTRINGHASH("DAMAGE_TYPE_TO_FITNESS", 0x10B32E3F));
	m_EffectGroupToFitness.Init(ATSTRINGHASH("EFFECT_GROUP_TO_FITNESS", 0xBB2E7758));
	m_FireTypeToFitness.Init(ATSTRINGHASH("FIRE_TYPE_TO_FITNESS", 0xF4E42354));
	m_DistanceAffectsWeaponType.Init(ATSTRINGHASH("DISTANCE_TO_WEAPON_TYPE", 0x98757E4A));
	m_NumberOfPedsToConfidence.Init(ATSTRINGHASH("NUMBER_OF_PEDS_TO_CONFIDENCE", 0xD8489104));
	
	m_GunFightConductorDM.Init();
	m_GunFightConductorFS.Init();
	m_NeedToRecalculate = true;
	m_HasToTriggerPlayerIntoCover = false;
	m_HasToTriggerPlayerExitCover = false;

	m_UseQSSoftVersion = false;

	m_LastOrder = Order_DoNothing;

	SetState(ConductorState_Idle);
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::Reset()
{
	m_GunFightConductorDM.Reset();
	m_NumberOfPedsInCombat = 0;
	for ( u32 i = 0; i <AUD_MAX_NUM_PEDS_IN_COMBAT; i++ )
	{
		if(m_PedsInCombat[i].pPed)
		{
			m_PedsInCombat[i].pPed->GetPedAudioEntity()->SetWasInCombat(false);
			m_PedsInCombat[i].pPed->GetPedAudioEntity()->SetWasTarget(false);
		}
		m_PedsInCombat[i].pPed = NULL;
		m_PedsInCombat[i].fitness = 1.f;
	}
	m_NumberOfTargets = 0;
	for ( s32 i = 0; i < MaxNumConductorsTargets; i++ )
	{
		m_CombatTargetPeds[i].fitness = -1.f;
		m_CombatTargetPeds[i].pPed = NULL;
	}
	m_NeedToRecalculate = true;
	m_LastOrder = Order_DoNothing;
	SetState(ConductorState_Idle);
}
//-------------------------------------------------------------------------------------------------------------------
//													DECISION MAKING
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::ProcessUpdate()
{
	BANK_ONLY(UpdateGunFightTargetDebug();)
	Update();
	m_GunFightConductorDM.ProcessUpdate();
	m_GunFightConductorFS.ProcessUpdate();
	BANK_ONLY(GunfightConductorDebugInfo();)
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductor::UpdateState(const s32 state, const s32, const fwFsm::Event event)
{
	fwFsmBegin
		fwFsmState(ConductorState_Idle)
		fwFsmOnUpdate
		return Idle();	
	fwFsmState(ConductorState_EmphasizeLowIntensity)
		fwFsmOnUpdate
		return EmphasizeLowIntensity();			
	fwFsmState(ConductorState_EmphasizeMediumIntensity)
		fwFsmOnUpdate
		return EmphasizeMediumIntensity();			
	fwFsmState(ConductorState_EmphasizeHighIntensity)
		fwFsmOnUpdate
		return EmphasizeHighIntensity();			
	fwFsmEnd
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductor::Idle()
{
	m_UseQSSoftVersion = false;
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductor::EmphasizeLowIntensity()
{
	// Send message to the superconductor to stop the QUITE_SCENE
	m_UseQSSoftVersion = NetworkInterface::IsGameInProgress();
	if(!m_UseQSSoftVersion && audSuperConductor::sm_StopQSOnGunfightConductor)
	{
		ConductorMessageData message;
		message.conductorName = SuperConductor;
		message.message = StopQuietScene;
		message.bExtraInfo = false;
		message.vExtraInfo = Vector3((f32)audSuperConductor::sm_GunfightConductorQSFadeOutTime
									,(f32)audSuperConductor::sm_GunfightConductorQSDelayTime
									,(f32)audSuperConductor::sm_GunfightConductorQSFadeInTime);
		SUPERCONDUCTOR.SendConductorMessage(message);
	}
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductor::EmphasizeMediumIntensity()
{

	UpdateTargets();
	if(m_GunFightConductorFS.IsReady())
	{
		if(m_HasToTriggerPlayerIntoCover)
		{
			m_GunFightConductorDM.EmphasizeBulletImpacts();
			m_GunFightConductorFS.EmphasizePlayerIntoCover();
			m_HasToTriggerPlayerIntoCover = false;
		}
		else if(m_HasToTriggerPlayerExitCover)
		{
			m_GunFightConductorDM.DeemphasizeBulletImpacts();
			m_GunFightConductorFS.HandlePlayerExitCover();
			m_HasToTriggerPlayerExitCover = false;
		}
	}
	// Send message to the superconductor to stop the QUITE_SCENE
	m_UseQSSoftVersion = NetworkInterface::IsGameInProgress();
	if(!m_UseQSSoftVersion && audSuperConductor::sm_StopQSOnGunfightConductor)
	{
		ConductorMessageData message;
		message.conductorName = SuperConductor;
		message.message = StopQuietScene;
		message.bExtraInfo = false;
		message.vExtraInfo = Vector3((f32)audSuperConductor::sm_GunfightConductorQSFadeOutTime
									,(f32)audSuperConductor::sm_GunfightConductorQSDelayTime
									,(f32)audSuperConductor::sm_GunfightConductorQSFadeInTime);
		SUPERCONDUCTOR.SendConductorMessage(message);
	}
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductor::EmphasizeHighIntensity()
{
	UpdateTargets();
	if(m_GunFightConductorFS.IsReady())
	{
		if(m_HasToTriggerPlayerIntoCover)
		{
			m_GunFightConductorDM.EmphasizeBulletImpacts();
			m_GunFightConductorFS.EmphasizePlayerIntoCover();
			m_HasToTriggerPlayerIntoCover = false;
		}
		else if(m_HasToTriggerPlayerExitCover)
		{
			m_GunFightConductorDM.DeemphasizeBulletImpacts();
			m_GunFightConductorFS.HandlePlayerExitCover();
			m_HasToTriggerPlayerExitCover = false;
		}
	}
	// Send message to the superconductor to stop the QUITE_SCENE
	m_UseQSSoftVersion = NetworkInterface::IsGameInProgress();
	if(!m_UseQSSoftVersion && audSuperConductor::sm_StopQSOnGunfightConductor)
	{
		ConductorMessageData message;
		message.conductorName = SuperConductor;
		message.message = StopQuietScene;
		message.bExtraInfo = false;
		message.vExtraInfo = Vector3((f32)audSuperConductor::sm_GunfightConductorQSFadeOutTime
									,(f32)audSuperConductor::sm_GunfightConductorQSDelayTime
									,(f32)audSuperConductor::sm_GunfightConductorQSFadeInTime);
		SUPERCONDUCTOR.SendConductorMessage(message);
	}
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
//														HELPERS	
//-------------------------------------------------------------------------------------------------------------------
EmphasizeIntensity audGunFightConductor::GetIntensity()
{
	EmphasizeIntensity intensity = Intensity_Invalid;
	switch(GetState())
	{
	case ConductorState_EmphasizeLowIntensity:
		intensity = Intensity_Low;
		break;
	case ConductorState_EmphasizeMediumIntensity:
		intensity = Intensity_Medium;
		break;
	case ConductorState_EmphasizeHighIntensity:
		intensity = Intensity_High;
		break;
	default:
		break;
	}
	return intensity;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::ProcessSuperConductorOrder(ConductorsOrders order)
{
	naAssertf((order >= Order_Invalid ) && (order <= Order_HighSpecialIntensity), "Wrong gunfightconductor order.");
	switch(order)
	{
	case Order_LowIntensity:
		if(m_LastOrder == Order_DoNothing)
		{
			m_NeedToRecalculate = true;
		}
		m_GunFightConductorDM.SetState(ConductorState_EmphasizeLowIntensity);
		SetState(ConductorState_EmphasizeLowIntensity);
		m_LastOrder = Order_LowIntensity;
		break;
	case Order_MediumSpecialIntensity:
		if(m_LastOrder == Order_DoNothing)
		{
			m_NeedToRecalculate = true;
		}
		m_GunFightConductorDM.SetState(ConductorState_EmphasizeMediumIntensity);
		SetState(ConductorState_EmphasizeMediumIntensity);
		m_LastOrder = Order_MediumIntensity;
		break;  
	case Order_HighSpecialIntensity:
		if(m_LastOrder == Order_DoNothing)
		{
			m_NeedToRecalculate = true;
		}
		m_GunFightConductorDM.SetState(ConductorState_EmphasizeHighIntensity);
		SetState(ConductorState_EmphasizeHighIntensity);
		m_LastOrder = Order_HighIntensity;
		break;
	case Order_DoNothing:
		if(m_LastOrder > Order_DoNothing)
		{
			Reset();
			m_LastOrder = Order_DoNothing;
		}
		break;
	default:
		break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::UpdateTargets()
{
#if __BANK
	if(sm_DebugMode)
	{
		ComputeGunFightTargetsDebug();
	}
	else
	{
		ComputeGunFightTargets();
	}
#else
	ComputeGunFightTargets();
#endif
}
//-------------------------------------------------------------------------------------------------------------------
ConductorsInfo audGunFightConductor::UpdateInfo()
{
	ConductorsInfo info;
	info.info = Info_NothingToDo;
	info.confidence = 0.f; 
#if __BANK
	bool debugActive = false;
	if(sm_ForceLowIntensityScene)
	{
		info.emphasizeIntensity = Intensity_Low;
		info.info = Info_DoBasicJob;
		//audGunFightConductorFakeScenes::sm_ForceOpenSpaceScene = true;
		//audGunFightConductorFakeScenes::sm_FakeBulletImpacts = true;
		debugActive = true;
	}
	else if (sm_ForceMediumIntensityScene)
	{
		info.emphasizeIntensity = Intensity_Medium;
		info.info = Info_DoSpecialStuffs;
		debugActive = true;
		//audGunFightConductorFakeScenes::sm_ForceOpenSpaceScene = true;
		//audGunFightConductorFakeScenes::sm_FakeBulletImpacts = true;
	}
	else if( sm_ForceHighIntensityScene)
	{
		info.emphasizeIntensity = Intensity_High;
		info.info = Info_DoSpecialStuffs;
		debugActive = true;
		//audGunFightConductorFakeScenes::sm_ForceOpenSpaceScene = true;
		//audGunFightConductorFakeScenes::sm_FakeBulletImpacts = true;
	}
	else 
	{
		audGunFightConductorFakeScenes::sm_ForceOpenSpaceScene = false;
		audGunFightConductorFakeScenes::sm_FakeBulletImpacts = false;
	}
	if (!debugActive)
#endif
	{
		if(ComputeAndValidatePeds()) 
		{
			info.confidence = m_NumberOfPedsToConfidence.CalculateValue((f32)m_NumberOfPedsInCombat); 
			if(info.confidence >= 0.8f)
			{
				info.emphasizeIntensity = Intensity_High;
				info.info = Info_DoSpecialStuffs;
			}
			else if(info.confidence >= 0.6f)
			{
				info.emphasizeIntensity = Intensity_Medium;
				info.info = Info_DoSpecialStuffs;
			}
			else if(info.confidence >= 0.4f)
			{
				info.emphasizeIntensity = Intensity_Low;
				info.info = Info_DoBasicJob;
			}
			else 
			{
				info.emphasizeIntensity = Intensity_Invalid;
				info.info = Info_NothingToDo;
			}
			// Clean out the number of peds in combat before start recalculating them over again.
			if(m_NeedToRecalculate)
			{
				m_NumberOfPedsInCombat = 0;
				for ( u32 i = 0; i <AUD_MAX_NUM_PEDS_IN_COMBAT; i++ )
				{
					if(m_PedsInCombat[i].pPed)
					{
						m_PedsInCombat[i].pPed->GetPedAudioEntity()->SetWasInCombat(false);
						m_PedsInCombat[i].pPed->GetPedAudioEntity()->SetWasTarget(false);
					}
					m_PedsInCombat[i].pPed = NULL;
					m_PedsInCombat[i].fitness = 1.f;
				}
				m_NeedToRecalculate = false;
			}
			return info;
		}
	}

	return info;
}
//-------------------------------------------------------------------------------------------------------------------
bool audGunFightConductor::ComputeAndValidatePeds()
{
	UpdatePedsInCombat();
	return m_NumberOfPedsInCombat > 0;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::UpdatePedsInCombat()
{ 
	//PF_FUNC(CalculatePedsInCombat);
	for(s32 i = 0; i < m_NumberOfPedsInCombat; i++)
	{
		if(m_PedsInCombat[i].pPed)
		{
			m_PedsInCombat[i].pPed->GetPedAudioEntity()->SetWasInCombat(true);
			m_PedsInCombat[i].pPed->GetPedAudioEntity()->SetWasTarget(false);
		}
		m_PedsInCombat[i].pPed = NULL;
		m_PedsInCombat[i].fitness = 1.f;
	}
	m_NumberOfPedsInCombat = 0;
	//Recalculate list of peds in combat.
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{	
		// Get the list of the near enemies.
		CEntityScannerIterator entityList = pPlayer->GetPedIntelligence()->GetNearbyPeds();
		for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
		{
			CPed* pNearbyPed = static_cast<CPed*>(pEnt);
			if (pNearbyPed && (pNearbyPed != pPlayer))
			{
				if( pNearbyPed->GetWeaponManager() && pNearbyPed->GetWeaponManager()->GetEquippedWeaponInfo() && pNearbyPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetDamageType() > DAMAGE_TYPE_MELEE)
				{
					if (pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == pPlayer
						|| pNearbyPed->GetPedResetFlag( CPED_RESET_FLAG_IsInCombat ))
					{
						// Store the ped
						if(naVerifyf((m_NumberOfPedsInCombat < AUD_MAX_NUM_PEDS_IN_COMBAT),"No more slots to add a new ped in combat.")) 
						{
							m_PedsInCombat[m_NumberOfPedsInCombat].pPed = pNearbyPed;
							f32 fitness = ComputeActionFitness(pNearbyPed);
							m_PedsInCombat[m_NumberOfPedsInCombat].fitness = fitness * ComputeFitness(pPlayer,pNearbyPed);
							m_NumberOfPedsInCombat ++;
						}
					}else if (pNearbyPed->GetPedAudioEntity()->GetWasInCombat())
					{
						pNearbyPed->GetPedAudioEntity()->SetGunfightVolumeOffset(0.f);
						pNearbyPed->GetPedAudioEntity()->SetWasInCombat(false);
					}
				}//if (pNearbyPed)
			}//for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
		} //if (naVerifyf(pPlayer,"Can't find local player."))
	}
}
//-------------------------------------------------------------------------------------------------------------------
f32  audGunFightConductor::ComputeActionFitness(CPed *pPed)
{
	//PF_FUNC(ComputeFitness);
	f32 fitness = 1.f;
	if (naVerifyf(pPed,"Null ped reference when computing his fitness."))
	{	
		//1. How dangerous is the enemy. ( At the moment is the same for all enemies).
		CPedIntelligence *pPedIntelligence = pPed->GetPedIntelligence();
		if(pPedIntelligence)
		{
			CTask* pTask = pPedIntelligence->GetTaskActive();
			while( pTask )
			{
				switch(pTask->GetTaskType())
				{
				case CTaskTypes::TASK_COMBAT: 
					if(pTask->GetState() == CTaskCombat::State_InCover)
					{
						fitness *= 0.8f;
					}
					break;
				default:
					fitness *= 0.5f;
					break;
				}
				pTask = pTask->GetSubTask();
			}
		} // if(pPedIntelligence)
	}//if (naVerifyf(pPlayer,"Can't find local player."))
	return fitness;
}
//-------------------------------------------------------------------------------------------------------------------
f32  audGunFightConductor::ComputeFitness(CPed *pPlayer,CPed *pPed)
{
	//PF_FUNC(ComputeFitness);
	f32 fitness = 1.f;
	if (naVerifyf(pPlayer,"Can't find local player."))
	{	
		//1. How dangerous is the enemy. ( At the moment is the same for all enemies).
		CPedIntelligence *pPedIntelligence = pPed->GetPedIntelligence();
		if(pPedIntelligence)
		{
			fitness *= m_CombatMovementToFitness.CalculateValue((f32)pPedIntelligence->GetCombatBehaviour().GetCombatMovement());
			fitness *= m_CombatAbilityToFitness.CalculateValue((f32)pPedIntelligence->GetCombatBehaviour().GetCombatAbility());
			fitness *= m_CombatRangeToFitness.CalculateValue((f32)pPedIntelligence->GetCombatBehaviour().GetAttackRange());

			//2. See if the enemy is visible, where is it (in front of the player, behind it...) and use it with the distance (3.)
			// so an enemy is gonna be more dangerous for example if is behind us and close to us.
			spdSphere boundSphere;
			pPed->GetBoundSphere(boundSphere);
			bool visible = camInterface::IsSphereVisibleInGameViewport(boundSphere.GetV4());
			Vector3 cameraFront = camInterface::GetFront(); 

			cameraFront.SetZ(0.f);
			cameraFront.NormalizeSafe();
			Vector3 dirToPed = VEC3V_TO_VECTOR3(  pPed->GetTransform().GetPosition() - pPlayer->GetTransform().GetPosition() );
			//f32 distanceToPed = dirToPed.Mag();
			dirToPed.SetZ(0.f); 
			dirToPed.NormalizeSafe();
			f32 dotFAngle = cameraFront.Dot(dirToPed);
			fitness *= m_PositionToFitness.CalculateValue(dotFAngle);
			if(!visible) 
			{
				// the ped becomes more dangerous if he's no visible for the player ( i.e. hidden behind a column, car, etc..)
				fitness *= 2.f;
				fitness = Clamp<float>(fitness, 0.0f, 1.0f);
			}
			else
			{	
				fitness *= 0.5f; 
			}
			//3. The weapon the ped is carrying also affect the fitness.( At the mo only care about the damage, see weaponEnums because maybe
			// we want to to create valueTables as for the ped combat ability
			const CPedWeaponManager* weaponManager = pPed->GetWeaponManager();
			if (weaponManager) 
			{
				const CWeapon* weapon = weaponManager->GetEquippedWeapon();

				if (weapon) 
				{
					const CWeaponInfo* weaponInfo = weapon->GetWeaponInfo();
					if (weaponInfo) 
					{
						fitness *= m_DamageTypeToFitness.CalculateValue((f32)weaponInfo->GetDamageType());
						fitness *= m_EffectGroupToFitness.CalculateValue((f32)weaponInfo->GetEffectGroup());	
						//fitness *= m_FireTypeToFitness.CalculateValue((f32)weaponInfo->GetFireType());	
					}//	if (weaponInfo) 
				}else
				{
					fitness *= 0.1f; 
				}//		if (weapon) 
			}//	if (weaponManager) 
		} // if(pPedIntelligence)
	}//if (naVerifyf(pPlayer,"Can't find local player."))
	return fitness;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::ComputeGunFightTargets()
{
	//PF_FUNC(ComputeGunFightTargets);
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{	
		s32	  nearPrimaryIndex = -1;
		s32	  nearSecondaryIndex = -1;
		f32	  distanceToPrimaryTarget = -1.f;
		// Clean the targets.
		for ( s32 i = 0; i < m_NumberOfTargets; i++ )
		{
			if(m_CombatTargetPeds[i].pPed)
			{
				m_CombatTargetPeds[i].pPed->GetPedAudioEntity()->SetWasTarget(true);
			}
			m_CombatTargetPeds[i].fitness = -1.f;
			m_CombatTargetPeds[i].pPed = NULL;
		}
		m_NumberOfTargets = 0;
		//For all peds in combat
		for(s32 i = 0; i < m_NumberOfPedsInCombat; i++)
		{
			if(m_PedsInCombat[i].pPed)
			{
				CTaskGun* task = (CTaskGun *)m_PedsInCombat[i].pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN);
				if (task && task->GetIsFiring())
				{
					ComputeNearTargets(i,nearPrimaryIndex,nearSecondaryIndex,distanceToPrimaryTarget);
				}
			}
		}
		//For all peds in combat
		for(s32 i = 0; i < m_NumberOfPedsInCombat; i++)
		{
			if(m_PedsInCombat[i].pPed)
			{
				CTaskGun* task = (CTaskGun *)m_PedsInCombat[i].pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN);
				if (task && task->GetIsFiring())
				{
					ComputeFarTargets(i,nearPrimaryIndex,nearSecondaryIndex,distanceToPrimaryTarget);
				}
			}
		}

	}//if (naVerifyf(pPlayer,"Can't find local player."))
}
//-------------------------------------------------------------------------------------------------------------------
CPed* audGunFightConductor::GetTarget(ConductorsTargets target) const
{
	naAssertf(target >= Near_Primary && target <= Far_Secondary, "Bad target index.");
	return m_CombatTargetPeds[target].pPed;
}
//-------------------------------------------------------------------------------------------------------------------
CPed* audGunFightConductor::GetPedInCombat(s32 pedIndex) const
{
	naAssertf(pedIndex >= 0 && pedIndex <= AUD_MAX_NUM_PEDS_IN_COMBAT, "Bad ped index.");
	return m_PedsInCombat[pedIndex].pPed;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::ProcessMessage(const ConductorMessageData &messageData)
{
	CPed* pPed = NULL;
	switch(messageData.message)
	{
	//case FiringWhileHeadingToCover:
	//case GotIntoCover:
	//	ForceTarget(pPed);
	//	break;
	case PlayerGotIntoCover:
		if(GetState() >= ConductorState_EmphasizeMediumIntensity && m_GunFightConductorFS.IsReady())
		{
			m_GunFightConductorDM.EmphasizeBulletImpacts();
			m_GunFightConductorFS.EmphasizePlayerIntoCover();
		}
		else if(GetState() >= ConductorState_EmphasizeMediumIntensity)
		{
			m_HasToTriggerPlayerIntoCover = true;
		}
		break;
	case PlayerExitCover:
		if(GetState() >= ConductorState_EmphasizeMediumIntensity && m_GunFightConductorFS.IsReady())
		{
			m_GunFightConductorDM.DeemphasizeBulletImpacts();
			m_GunFightConductorFS.HandlePlayerExitCover();
		}
		else if(GetState() >= ConductorState_EmphasizeMediumIntensity)
		{
			m_HasToTriggerPlayerIntoCover = false;
			m_HasToTriggerPlayerExitCover = true;
		}
		break;
	case PedDying:
		if(messageData.entity)
		{
			pPed = (CPed*)messageData.entity;
		}
 		naAssertf(pPed,"Wrong ped pointer when sending message to the conductor.");
		// Make sure our changes in CPed are reset.
		pPed->GetPedAudioEntity()->SetWasTarget(false);
		pPed->GetPedAudioEntity()->SetWasInCombat(false);
		break;
	case FakeRicochet:
		//if(GetState() >= ConductorState_EmphasizeMediumIntensity)
		//{
			// in extraInfo we get the impact position.
			m_GunFightConductorFS.PlayFakeRicochets(messageData.entity,messageData.vExtraInfo);
		//}
		break;
	case VehicleDebris:
		//if(GetState() >= ConductorState_EmphasizeMediumIntensity)
		//{
		// in extraInfo we get the impact position.
		m_GunFightConductorFS.PlayPostShootOutVehDebris(messageData.entity,messageData.vExtraInfo);
		//}
		break;
	case BuildingDebris:
		//if(GetState() >= ConductorState_EmphasizeMediumIntensity)
		//{
		// in extraInfo we get the impact position.
		m_GunFightConductorFS.PlayPostShootOutBuildingDebris(messageData.entity,messageData.vExtraInfo);
		//}
		break;
	default: break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::ForceTarget(const CPed *pPed)
{
	if(!IsTarget(pPed))
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (naVerifyf(pPlayer,"Can't find local player."))
		{	
			m_CombatTargetPeds[Near_Secondary].pPed = m_CombatTargetPeds[Near_Primary].pPed;
			m_CombatTargetPeds[Near_Secondary].fitness = m_CombatTargetPeds[Near_Primary].fitness;
			m_CombatTargetPeds[Near_Primary].pPed = const_cast<CPed*>(pPed);
			// Setting the fitness to 1 because we force the target. 
			m_CombatTargetPeds[Near_Primary].fitness = 1.f;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::ComputeNearTargets(const s32 pedIndex,s32 &nearPrimaryIndex,s32 &nearSecondaryIndex,f32 &distanceToPrimaryTarget)
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{	
		if (m_PedsInCombat[pedIndex].pPed && !m_PedsInCombat[pedIndex].pPed->GetIsDeadOrDying())
		{
			//Get the targets.
			Vector3 dirToPlayer = VEC3V_TO_VECTOR3(  m_PedsInCombat[pedIndex].pPed->GetTransform().GetPosition() - pPlayer->GetTransform().GetPosition() );
			f32 distanceToPlayer = dirToPlayer.Mag();
			// To know if this ped belongs to the near or far group.
			if(m_PedsInCombat[pedIndex].fitness >= m_CombatTargetPeds[Near_Primary].fitness)
			{
				m_CombatTargetPeds[Near_Primary].pPed = m_PedsInCombat[pedIndex].pPed; 
				//m_CombatTargetPeds[Near_Primary].sector = m_PedsInCombat[pedIndex].sector; 
				m_CombatTargetPeds[Near_Primary].fitness = m_PedsInCombat[pedIndex].fitness; 
				distanceToPrimaryTarget = distanceToPlayer;
				nearPrimaryIndex = pedIndex;
				naAssertf(m_NumberOfTargets <= MaxNumConductorsTargets, "Bad number of targets");
				if(m_NumberOfTargets < (Near_Primary + 1))
				{
					m_NumberOfTargets ++;
				}
			}
			else if (( nearPrimaryIndex != -1 ) && ( nearPrimaryIndex != pedIndex ))
			{
				f32 fitness = m_PedsInCombat[pedIndex].fitness ;
				// Add new metrics to the fitness : 
				// 1. how far is from the primary target. 
				// NOTE: has to be in the limits of near targets
				Vector3 dirToPed = VEC3V_TO_VECTOR3(  m_PedsInCombat[pedIndex].pPed->GetTransform().GetPosition()
					- m_PedsInCombat[nearPrimaryIndex].pPed->GetTransform().GetPosition() );
				f32 distanceToPed = dirToPed.Mag();
				// The max distance between peds is the diameter (2.f * sm_DistanceThreshold)
				f32 distanceToFitness = m_DistanceToFitness.CalculateRescaledValue(0.1f,1.f,0.f,2.f*sm_DistanceThreshold,distanceToPed); 
				fitness *= distanceToFitness;
				// 2. as close to the primary target as the ped is more important is to have a different weapon.
				const CPedWeaponManager* weaponManager = m_PedsInCombat[pedIndex].pPed->GetWeaponManager();
				const CPedWeaponManager* targetWeaponManager = m_PedsInCombat[nearPrimaryIndex].pPed->GetWeaponManager();
				if (weaponManager && targetWeaponManager) 
				{
					const CWeapon* weapon = weaponManager->GetEquippedWeapon();
					const CWeapon* targetWeapon = targetWeaponManager->GetEquippedWeapon();
					if (weapon && targetWeapon) 
					{
						f32 amountWeaponTypeAffects = m_DistanceAffectsWeaponType.CalculateValue(distanceToFitness);
						if(weapon->GetWeaponHash() ==  targetWeapon->GetWeaponHash())
						{
							fitness = amountWeaponTypeAffects * 0.5f * fitness + (1.f - amountWeaponTypeAffects) * fitness;
						}
						else
						{
							fitness = amountWeaponTypeAffects * 2.f * fitness + (1.f - amountWeaponTypeAffects) * fitness;
						}
						fitness = Clamp<float>(fitness, 0.0f, 1.0f);
					}//		if (weapon) 

				}//if (weaponManager) 
				else
				{
					fitness *= 0.1f; 
				}
				if(fitness >= m_CombatTargetPeds[Near_Secondary].fitness)
				{
					m_CombatTargetPeds[Near_Secondary].pPed = m_PedsInCombat[pedIndex].pPed; 
					//m_CombatTargetPeds[Near_Secondary].sector = m_PedsInCombat[pedIndex].sector; 
					m_CombatTargetPeds[Near_Secondary].fitness = fitness; 
					nearSecondaryIndex = pedIndex;
					naAssertf(m_NumberOfTargets <= MaxNumConductorsTargets, "Bad number of targets");
					if(m_NumberOfTargets < (Near_Secondary +1))
					{
						m_NumberOfTargets ++;
					}
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::ComputeFarTargets(const s32 pedIndex,s32 nearPrimaryIndex,s32 nearSecondaryIndex, f32 distanceToPrimaryTarget)
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{	
		if (m_PedsInCombat[pedIndex].pPed && !m_PedsInCombat[pedIndex].pPed->GetIsDeadOrDying())
		{
			s32	  farPrimaryIndex = -1;
			//s32	  farSecondaryIndex = -1;
			//Get the targets.
			Vector3 dirToPlayer = VEC3V_TO_VECTOR3(  m_PedsInCombat[pedIndex].pPed->GetTransform().GetPosition() - pPlayer->GetTransform().GetPosition() );
			f32 distanceToPlayer = dirToPlayer.Mag();
			// To know if this ped belongs to the near or far group.
			if(distanceToPlayer > (distanceToPrimaryTarget + sm_DistanceThreshold))
			{
				if((m_PedsInCombat[pedIndex].fitness >= m_CombatTargetPeds[Far_Primary].fitness) && ( pedIndex != nearPrimaryIndex ) && (pedIndex != nearSecondaryIndex ) )
				{
					m_CombatTargetPeds[Far_Primary].pPed = m_PedsInCombat[pedIndex].pPed; 
					//m_CombatTargetPeds[Far_Primary].sector = m_PedsInCombat[pedIndex].sector; 
					m_CombatTargetPeds[Far_Primary].fitness = m_PedsInCombat[pedIndex].fitness; 
					farPrimaryIndex = pedIndex;
					naAssertf(m_NumberOfTargets <= MaxNumConductorsTargets, "Bad number of targets");
					if(m_NumberOfTargets < (Far_Primary + 1))
					{
						m_NumberOfTargets ++;
					}
				}
				else if ((farPrimaryIndex != -1)  && (farPrimaryIndex != pedIndex)  && ( pedIndex != nearPrimaryIndex ) && (pedIndex != nearSecondaryIndex ) )
				{
					f32 fitness = m_PedsInCombat[pedIndex].fitness ;
					// Add new metrics to the fitness : 
					// 1. how far is from the primary target. 
					// NOTE: has to be in the limits of near targets
					Vector3 dirToPed = VEC3V_TO_VECTOR3(  m_PedsInCombat[pedIndex].pPed->GetTransform().GetPosition()
						- m_PedsInCombat[nearPrimaryIndex].pPed->GetTransform().GetPosition() );
					f32 distanceToPed = dirToPed.Mag();
					// The max distance between peds is the diameter (2.f * sm_DistanceThreshold)
					f32 distanceToFitness = m_DistanceToFitness.CalculateRescaledValue(0.1f,1.f,0.f,2.f*sm_DistanceThreshold,distanceToPed); 
					fitness *= distanceToFitness;
					// 2. as close to the primary target as the ped is more important is to have a different weapon.
					const CPedWeaponManager* weaponManager = m_PedsInCombat[pedIndex].pPed->GetWeaponManager();
					const CPedWeaponManager* targetWeaponManager = m_PedsInCombat[nearPrimaryIndex].pPed->GetWeaponManager();
					if (weaponManager && targetWeaponManager) 
					{
						const CWeapon* weapon = weaponManager->GetEquippedWeapon();
						const CWeapon* targetWeapon = targetWeaponManager->GetEquippedWeapon();
						if (weapon && targetWeapon) 
						{
							f32 amountWeaponTypeAffects = m_DistanceAffectsWeaponType.CalculateValue(distanceToFitness);
							if(weapon->GetWeaponHash() ==  targetWeapon->GetWeaponHash())
							{
								fitness = amountWeaponTypeAffects * 0.5f * fitness + (1.f - amountWeaponTypeAffects) * fitness;
							}
							else
							{
								fitness = amountWeaponTypeAffects * 2.f * fitness + (1.f - amountWeaponTypeAffects) * fitness;
							}
							fitness = Clamp<float>(fitness, 0.0f, 1.0f);
						}//		if (weapon)

					}							
					else
					{
						fitness *= 0.1f; 
					} //if (weaponManager) 
					if(m_PedsInCombat[pedIndex].fitness >= m_CombatTargetPeds[Far_Secondary].fitness)
					{
						m_CombatTargetPeds[Far_Secondary].pPed = m_PedsInCombat[pedIndex].pPed; 
						//m_CombatTargetPeds[AUD_FAR_SECONDARY].sector = m_PedsInCombat[pedIndex].sector; 
						m_CombatTargetPeds[Far_Secondary].fitness = fitness; 
						naAssertf(m_NumberOfTargets <= MaxNumConductorsTargets, "Bad number of targets");
						if(m_NumberOfTargets < (Far_Secondary + 1))
						{
							m_NumberOfTargets ++;
						}
					}
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool audGunFightConductor::IsTarget(const CPed *pPed)
{
	for (s32 i = 0; i < m_NumberOfTargets; i++)
	{
		if(m_CombatTargetPeds[i].pPed == pPed)
		{
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
void audGunFightConductor::ComputeGunFightTargetsDebug()
{
	if (CDebugScene::FocusEntities_Get(0))
	{
		CPed* pPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));
		ForceTarget(pPed);
		//SetState(ConductorState_ProcessSuperConductorSuggestion);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::UpdateGunFightTargetDebug()
{
	if(sm_DebugMode)
	{
		if(CDebugScene::FocusEntities_Get(0))
		{
			if (CDebugScene::FocusEntities_Get(0) != sm_LastFocusedEntity)
			{
				sm_LastFocusedEntity = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));;
				ComputeGunFightTargetsDebug();
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
const char* audGunFightConductor::GetStateName(s32 iState) const
{
	taskAssert(iState >= ConductorState_Idle && iState <= ConductorState_EmphasizeHighIntensity);
	switch (iState)
	{
	case ConductorState_Idle:						return "ConductorState_Idle";
	case ConductorState_EmphasizeLowIntensity:		return "ConductorState_EmphasizeLowIntensity";
	case ConductorState_EmphasizeMediumIntensity:	return "ConductorState_EmphasizeMediumIntensity";
	case ConductorState_EmphasizeHighIntensity:		return "ConductorState_EmphasizeHighIntensity";
	default: taskAssert(0); 	
	}
	return "ConductorState_Invalid";
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::GunfightConductorDebugInfo()
{
	if(sm_ShowInfo)
	{
		char text[64];
		formatf(text,sizeof(text),"GunfightConductor state -> %s", GetStateName(GetState()));
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,sizeof(text),"Time in state -> %f", GetTimeInState());
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,sizeof(text),"Number of peds in combat-> %d", m_NumberOfPedsInCombat);
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,sizeof(text),"Number of targets-> %d", m_NumberOfTargets);
		grcDebugDraw::AddDebugOutput(text);
	}
	if(sm_ShowTargetPeds)
	{
		if(!sm_DebugMode)
		{
			for(u32 i = 0 ; i < m_NumberOfTargets ; i++)
			{
				if(m_CombatTargetPeds[i].pPed)
				{
					Vector3 vecHeadPos(0.0f,0.0f,0.0f);
					m_CombatTargetPeds[i].pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);	
					//char text[64];
					/*CPed* pPlayer = CGameWorld::FindLocalPlayer();
					if (naVerifyf(pPlayer,"Can't find local player."))
					{
						Vector3 dirToPlayer = VEC3V_TO_VECTOR3(  m_CombatTargetPeds[i].pPed->GetTransform().GetPosition() - pPlayer->GetTransform().GetPosition() );
						f32 distanceToPlayer = dirToPlayer.Mag();
						formatf(text,sizeof(text),"Target : %d, fitness %f, distance : %f",i,m_CombatTargetPeds[i].fitness, distanceToPlayer); 
					}
					grcDebugDraw::Text(vecHeadPos + Vector3(0.0f,0.0f,0.3f), Color32(255, 255, 255, 255),text,true,1);*/
					grcDebugDraw::Sphere(vecHeadPos, 0.2f,Color32(255, 0, 0, 128));
				}

			}
		}else
		{
			if (CDebugScene::FocusEntities_Get(0))
			{
				CPed* pPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));
				char text[64];
				Vector3 vecHeadPos(0.0f,0.0f,0.0f);
				pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);	
				formatf(text,sizeof(text),"Target"); 
				grcDebugDraw::Text(vecHeadPos + Vector3(0.0f,0.0f,0.3f), Color32(255, 255, 255, 255),text,true,1);
			}
		}
	}
	if(sm_ShowPedsInCombat)
	{
		for(u32 i = 0 ; i < m_NumberOfPedsInCombat ; i++)
		{
			if(m_PedsInCombat[i].pPed)
			{
				char text[64];
				Vector3 vecHeadPos(0.0f,0.0f,0.0f);
				m_PedsInCombat[i].pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);	
				CPed* pPlayer = CGameWorld::FindLocalPlayer();
				if (naVerifyf(pPlayer,"Can't find local player."))
				{
					//Vector3 dirToPlayer = VEC3V_TO_VECTOR3(  m_PedsInCombat[i].pPed->GetTransform().GetPosition() - pPlayer->GetTransform().GetPosition() );
					//f32 distanceToPlayer = dirToPlayer.Mag2();
					//f32 angle = RtoD * atan2( dirToPlayer.x, dirToPlayer.z );

					//formatf(text,sizeof(text),"fitness %f, distance : %f",m_PedsInCombat[i].fitness,angle, distanceToPlayer); 
					formatf(text,sizeof(text),"wasInCombat %s, wasTarget %s, applyValue : %d , time : %f ",(m_PedsInCombat[i].pPed->GetPedAudioEntity()->GetWasInCombat())?"true":"",(m_PedsInCombat[i].pPed->GetPedAudioEntity()->GetWasTarget())?"true":"" ,((naEnvironmentGroup *)m_PedsInCombat[i].pPed->GetAudioEnvironmentGroup())->GetApplyValue(),((naEnvironmentGroup *)m_PedsInCombat[i].pPed->GetAudioEnvironmentGroup())->GetMixGroupFadeTime() ); 
					//formatf(text,sizeof(text),"applyValue : %d , time : %f "
					//	,((naEnvironmentGroup *)m_PedsInCombat[i].pPed->GetAudioEnvironmentGroup())->GetApplyValue()
					//	,((naEnvironmentGroup *)m_PedsInCombat[i].pPed->GetAudioEnvironmentGroup())->GetMixGroupFadeTime()); 
				}
				grcDebugDraw::Text(vecHeadPos + Vector3(0.0f,0.0f,0.3f), Color32(255, 255, 255, 255),text,true,1);
			}

		}
	}
	if(sm_ShowGunfightVolOffset)
	{
		for(u32 i = 0 ; i < m_NumberOfPedsInCombat ; i++)
		{
			if(m_PedsInCombat[i].pPed)
			{
				char text[64];
				Vector3 vecHeadPos(0.0f,0.0f,0.0f);
				m_PedsInCombat[i].pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);	
				formatf(text,sizeof(text),"%f",m_PedsInCombat[i].pPed->GetPedAudioEntity()->GetGunfightVolumeOffset()); 
				grcDebugDraw::Text(vecHeadPos + Vector3(0.0f,0.0f,0.3f), Color32(255, 255, 255, 255),text,true,1);
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductor::AddWidgets(bkBank &bank){
	bank.PushGroup("GunFight Conductor",false);
		bank.AddToggle("Show info", &sm_ShowInfo);
		bank.AddToggle("Show Targets", &sm_ShowTargetPeds);
		bank.AddToggle("Show Peds in combat", &sm_ShowPedsInCombat);
		bank.AddToggle("Show gunfight vol offset", &sm_ShowGunfightVolOffset);
		bank.AddToggle("Debug mode", &sm_DebugMode);
		bank.AddSlider("Distance threshold to separate near and far targets", &sm_DistanceThreshold, 0.0f, 100.0f, 1.f);
		bank.AddSeparator();
		bank.AddTitle("Here you can force the gunfight conductor intensity scenes.");
		bank.AddToggle("Force low intensity scene", &sm_ForceLowIntensityScene);
		bank.AddToggle("Force medium intensity scene", &sm_ForceMediumIntensityScene);
		bank.AddToggle("Force high intensity scene", &sm_ForceHighIntensityScene);
		audGunFightConductorDynamicMixing::AddWidgets(bank);
		audGunFightConductorFakeScenes::AddWidgets(bank);
	bank.PopGroup();

}
#endif
