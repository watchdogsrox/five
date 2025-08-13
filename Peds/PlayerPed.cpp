// Title	:	PlayerPed.cpp
// Author	:	Gordon Speirs
// Started	:	10/12/99

#if 0

// File header
#include "Peds/Ped.h"

// Rage headers
#include "crSkeleton/SkeletonData.h"
#include "System/TimeMgr.h"
#include "system/param.h"
#include "System/timer.h"

// Framework headers
#include "fwdebug/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwtl/pool.h"

// Game headers
#include "Animation/AnimBlender.h"
#include "Animation/AnimBones.h"
#include "Animation/AnimDefines.h"
#include "Animation/AnimDirector.h"
#include "animation/AnimGroupIds.h"
#include "animation/AnimIds.h"
#include "Animation/AnimManager.h"
#include "Animation/AnimPlayer.h"
#include "Audio/ScriptAudioEntity.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/Viewport.h"
#include "vehicleAi/Task/TaskVehicleTempAction.h"
#include "vehicleAi/vehicleintelligence.h"
#include "Control/Garages.h"
#include "debug/debugscene.h"
#include "Event/EventDamage.h"
#include "Event/EventWeapon.h"
#include "Frontend/Radar.h"
#include "Game/cheat.h"
#include "Game/localisation.h"
#include "Game/modelindices.h"
#include "Stats/StatsMgr.h"
#include "ModelInfo/vehiclemodelinfo.h"
#include "ModelInfo/weaponmodelinfo.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/NetworkInterface.h"
#include "Network/players/NetworkPlayerMgr.h"
#include "Objects/object.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Gangs.h"
#include "Peds/Ped.h"
#include "Peds/PedFactory.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedMoveBlend/PedMoveBlendInWater.h"
#include "Peds/PedType.h"
#include "Peds/rendering/pedVariationStream.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Physics/GtaArchetype.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "Physics/WorldProbe.h"
#include "scene/portals/portalTracker.h"
#include "scene/world/GameWorld.h"
#include "script/commands_ped.h"
#include "Stats\StatsInterface.h"
#include "System/controlmgr.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/Ped/TaskMotionPed.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Weapons/Gun/TaskAimGunStanding.h"
#include "Vehicles/VehicleGadgets.h"
#include "vfx/misc/Fire.h"
#include "vfx/Particles/PtFxManager.h"
#include "vehicleAi/vehicleintelligence.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "weapons/Weapon.h"


#if __GTANY
#include "../../gta/source/frontend/MobilePhone.h"
#endif

AI_OPTIMISATIONS()

//mjc #define MIN_SPRINT_ENERGY	(-150.0f)

//Game doesnt create a player.
//mjc PARAM(noPlayer, "[PlayerPed] Game never creates the player.");


// Timer to avoid player getting stuck permanently hit by water cannon
//mjc dev_float PLAYER_MAX_TIME_HIT_BY_WATER_CANNON	= 20.0f;
//mjc dev_float PLAYER_WATER_CANNON_COOLDOWN_RATE	= 0.1f;		// Hit by timer decreases by rate * t every frame if > 0.0f
//mjc dev_float PLAYER_WATER_CANNON_IMMUNE_TIME		= 10.0f;

//mjc bool CPlayerPed::bDebugPlayerInfo = false;
//mjc bool CPlayerInfo::ms_bDebugPlayerInvincible = false;
//mjc bool CPlayerInfo::ms_bDebugPlayerInvisible = false;
//mjc bool CPlayerInfo::ms_bDebugPlayerInfiniteStamina = false;
//mjc const float CPlayerPed::ms_fInteriorShockingEventFreq = 2.0f;
//mjc bool CPlayerInfo::ms_bHasDisplayedPlayerQuitEnterCarHelpText=false;
#if __ASSERT
//mjc bool CPlayerPed::m_ChangingPlayerModel = false;
#endif // __ASSERT
// Name			:	Constructor
// Purpose		:	Default constructor for CPlayerPed class
// Parameters	:	None
// Returns		:	Nothing

#if 0 // mjc
CPlayerPed::CPlayerPed(const eEntityOwnedBy ownedBy, const CControlledByInfo& pedControlInfo, s32 modelIndex)
: CPed(ownedBy, pedControlInfo, modelIndex)
//mjc , m_nPhoneModelIndex(MI_CELLPHONE)
//mjc , m_vPhoneAttachPosOffset(PED_PHONE_ATTACH_POS_OFFSET)
//mjc , m_vPhoneAttachRotOffset(PED_PHONE_ATTACH_ROT_OFFSET)
//mjc , m_pExternallyDrivenDOFFrame(NULL)
{
	Assert(CPedFactory::IsCurrentlyWithinFactory());

	Assert(GetPortalTracker());
	GetPortalTracker()->SetLoadsCollisions(true);	//force loading of interior collisions
	CPortalTracker::AddToActivatingTrackerList(GetPortalTracker());		// causes interiors to activate & stay active

	// the player can have a cop model in the multiplayer game, which gives him a PEDTYPE_COP - this confuses the dispatch stuff
	SetPedType(PEDTYPE_CIVMALE);

	m_PedConfigFlags.bUpperBodyDamageAnimsOnly = true;
	
	// Have to update the max health here so that it is correct for the 2nd player in 2-player games.
    m_nMaxHealth = CPlayerInfo::PLAYER_DEFAULT_MAX_HEALTH; //CStatsMgr::GetFatAndMuscleModifier(STAT_MODIFIER_MAX_HEALTH);
	SetHealth(m_nMaxHealth);

	ClearDamage();

	// these used to be set in CGameWorld just after the ped was created:
	//mjc-moved m_targeting.SetOwner(this);
	SetHeading(0.0f);
	weaponAssert(GetWeaponManager());
	GetWeaponManager()->GetAccuracy().SetPedAccuracy(1.0f);

#if __WIN32PC && 0
	m_pMouseLockOnRecruitPed=0;
	m_iMouseLockOnRecruitTimer=0;
#endif

	//mjc m_bWasHeldDown					= false; //mjc unused

	GetPedIntelligence()->SetInformRespectedFriends(30.0f, 2);

	//mjc m_bCoverGeneratedByDynamicEntity = false;
	//mjc m_bCanMoveLeft = false;
	//mjc m_bCanMoveRight = false;

	//mjc m_timeGunLastFiredDuringRagdoll = 0;	//mjc unused

	//mjc m_fHitByWaterCannonTimer = 0.0f;
	//mjc m_fHitByWaterCannonImmuneTimer = 0.0f;

	//mjc m_fTimeBeenSprinting = 0.0f;
	//mjc m_fSprintStaminaUpdatePeriod = 0.0f;
	//mjc m_fSprintStaminaDurationMultiplier = 1.0f;
	//mjc m_fTimeBeenSwimming = 0.0f;
	//mjc m_fSwimLungCapacityUpdatePeriod = 0.0f;

	//mjc m_fTimeToNextCreateInteriorShockingEvents = ms_fInteriorShockingEventFreq;

	m_PedConfigFlags.nPedLegIkMode = LEG_IK_FULL;

	// The player ped is always treated as suspicious
	GetPedIntelligence()->GetPedStealth().GetFlags().SetFlag(CPedStealth::SF_TreatedAsActingSuspiciously);
	// Player's footsteps always generate events
	GetPedIntelligence()->GetPedStealth().GetFlags().SetFlag(CPedStealth::SF_PedGeneratesFootStepEvents);
}
#endif //mjc

#if 0 //mjc
void CPlayerPed::SetModelIndex(int model)
{
	const CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(CModelInfo::GetModelInfo(model));
	Assert(pModelInfo);

	CPed::SetModelIndex(model);

	SetupDrivenDOF();

	if (!pModelInfo->GetIsPlayerModel())
	{
		return;
	}

	SetInitialState();

	// load in the initial data for the player
	CPedVariationStream::RequestStreamPedFiles(this, &GetVarData(), (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD));	

	CPedVariationStream::ProcessStreamRequests();
}
#endif //mjc

// Name			:	Destructor
// Purpose		:	Default destructor for CPlayerPed class
// Parameters	:	None
// Returns		:	Nothing

CPlayerPed::~CPlayerPed()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), this->GetPosition() );

#if __DEV
	Assertf(CPedFactory::IsCurrentlyInDestroyPlayerPedFunction(), "Deleting a player ped from outside of the factory destroy method, this will cause problems for networked player peds");
#endif

	// moved from CGameWorld::DeletePlayer(): If this player was driving a car we have to tidy up
	if (GetMyVehicle() && GetMyVehicle()->GetDriver() == this)
	{
		if (GetMyVehicle()->GetStatus() != STATUS_WRECKED)
		{
			GetMyVehicle()->SetStatus(STATUS_PHYSICS);
		}

		if (!GetMyVehicle()->IsNetworkClone())
		{
			GetMyVehicle()->SetThrottle(0.0f);
			GetMyVehicle()->SetBrake(HIGHEST_BRAKING_WITHOUT_LIGHT);
		}
	}

	if (GetPlayerData())
	{
		if(GetPlayerData()->m_MeleeWeaponAnimReferenced!=ANIM_GROUP_STD_PED)
		{
			s32 blockIndex = CAnimManager::FindSlot(CAnimGroupManager::GetAnimDictName(GetPlayerData()->m_MeleeWeaponAnimReferenced));
			Assert(blockIndex!=-1);
			CAnimManager::RemoveRefWithoutDelete(blockIndex);
			GetPlayerData()->m_MeleeWeaponAnimReferenced = ANIM_GROUP_STD_PED;
		}
		// de-reference extra melee combo if no longer required
		if(GetPlayerData()->m_MeleeWeaponAnimReferencedExtra!=ANIM_GROUP_STD_PED)
		{
			s32 blockIndex = CAnimManager::FindSlot(CAnimGroupManager::GetAnimDictName(GetPlayerData()->m_MeleeWeaponAnimReferencedExtra));
			Assert(blockIndex!=-1);
			CAnimManager::RemoveRefWithoutDelete(blockIndex);
			GetPlayerData()->m_MeleeWeaponAnimReferencedExtra = ANIM_GROUP_STD_PED;
		}

		GetPlayerData()->m_Wanted.SetPed (NULL);

		if (GetPlayerData()->m_PlayerGroup >= 0)
		{
			CPedGroups::ms_groups[GetPlayerData()->m_PlayerGroup].Flush(false, false);
		}
	}

	//mjc moved DeleteDrivenDOF();
} // end - CPlayerPed::~CPlayerPed

#if 0 //mjc
void CPlayerPed::SetupPlayerGroup()
{
	GetPlayerData()->m_PlayerGroup = CPedGroups::GetPlayersGroup(this);
	CPedGroups::AddGroupAtIndex(POPTYPE_PERMANENT, GetPlayerData()->m_PlayerGroup);
	CPedGroups::ms_groups[GetPlayerData()->m_PlayerGroup].GetGroupMembership()->SetLeader(this);
	CPedGroups::ms_groups[GetPlayerData()->m_PlayerGroup].Process();
}
#endif

#if 0 //mjc
bool CPlayerPed::IsNetworkBot() const
{
	if(GetNetworkObject() && GetNetworkObject()->GetPlayerOwner())
	{
		if(GetNetworkObject()->GetPlayerOwner()->GetPlayerType() == CNetGamePlayer::NETWORK_BOT)
		{
			return true;
		}
	}

	return false;
}
#endif //mjc

#if 0 //mjc
// Name			:	SetInitialState
// Purpose		:	Processes control for player ped object
// Parameters	:	None
// Returns		:	Nothing
void CPlayerPed::SetInitialState()
{
	fwTimer::SetTimeWarp(1.0f);

	SetBaseFlag(fwEntity::USE_COLLISION);
	SetArrestState(ArrestState_None);
	SetDeathState(DeathState_Alive);
	SetMoveState(PEDMOVE_STILL);
	SetIsCrouching(false);
	SetIsInStealthMode(false);
#if !USE_NEW_MOTION_TASKS
	SetTempOverrideAnimGroup(ANIM_GROUP_ID_INVALID);
#endif //!USE_NEW_MOTION_TASKS
	if(GetIsAttached())
		DetachFromParent(0);

	SetBaseFlag(fwEntity::USE_COLLISION);

  	if(GetPlayerData())
  		GetPlayerData()->SetInitialState();
}
#endif //mjc

#if 0 //mjc
//
// name:		GetPadFromPlayer
// description:	Returns a pointer to the pad class that is used by this player
CControl* CPlayerPed::GetControlFromPlayer()
{
	if (IsControlledByLocalPlayer())
	{
		return &CControlMgr::GetMainPlayerControl();
	}
	else
	{
		return NULL; // Don't take controls into acount if this player is controlled from another machine
	}
}

const CControl* CPlayerPed::GetControlFromPlayer() const
{
	if (IsControlledByLocalPlayer())
	{
		return &CControlMgr::GetMainPlayerControl();
	}
	else
	{
		return NULL;		// Don't take controls into acount if this player is controlled from another machine
	}
}
#endif //mjc

#if 0 //mjc - moved to CPlayerInfo
bool CPlayerPed::CanPlayerStartMission()
{
//RAGE	if (CGameLogic::GameState != CGameLogic::GAMESTATE_PLAYING || CGameLogic::IsCoopGameGoingOn()) return false;	// Obbe: put in to fix the problem with ending a 2 player game on a mission trigger.

	if (IsPedInControl() == false && !GetIsDrivingVehicle() )
	{
		return false;
	}
	
	return true;
}
#endif //mjc

// Name			:	ProcessControl
// Purpose		:	Processes control for player ped object
// Parameters	:	None
// Returns		:	Nothing

#if 0 //mjc - moved to CPlayerInfo

bool CPlayerPed::ProcessControl()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessControlOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( (CDebugScene::ShouldDebugBreakOnProximityOfProcessControlCallingEntity() || CDebugScene::ShouldDebugBreakOnProximityOfProcessControlCallingPed()), this->GetPosition() );

	if (IsControlledByNetworkPlayer())
	{
		// we have control of another network player, just do a simple ped process 
		CPed::ProcessControl();
		return true;
	}

	Assertf((PopTypeGet() == POPTYPE_PERMANENT), "CPlayerPed::ProcessControl - Player Ped should always be a permanent character");

	Assertf(m_nRagdollState != RAGDOLL_STATE_ANIM_LOCKED || (GetPlayerInfo() && GetPlayerInfo()->AreControlsDisabled()),"CPlayerPed: Player is in control of locked ragdoll. Has script called UNLOCK_RAGDOLL on playerped?");

	// Update the max in water timers if needed
	if(GetIsInWater())
	{
		if(GetPosition().Mag2() > PLAYER_SWIMMING_RANGE*PLAYER_SWIMMING_RANGE)
			m_fMaxTimeInWater = PLAYER_MAX_TIME_IN_WATER_LONG_RANGE;
		else
			m_fMaxTimeInWater = PLAYER_MAX_TIME_IN_WATER;
	}

	// Decrement hit by water cannon timer. Timer gets increased whenever player gets hit by water cannon
	// This ensures player can't get trapped permanently by water cannon
	if(m_fHitByWaterCannonImmuneTimer > 0.0f)
		m_fHitByWaterCannonImmuneTimer -= fwTimer::GetTimeStep();

	if(m_fHitByWaterCannonTimer > 0.0f)
		m_fHitByWaterCannonTimer -= fwTimer::GetTimeStep()*PLAYER_WATER_CANNON_COOLDOWN_RATE;

	CPed::ProcessControl();

	// try forcing this extra test on (for the player only)
	m_PedResetFlags.bCheckColAboveHead = true;

	CControl* pControl = GetControlFromPlayer();
	

	// If a player ped stands on a police car he will get a wanted level
	if (m_pGroundPhysical && m_pGroundPhysical->GetIsTypeVehicle())
	{
		CVehicle *pVeh = (CVehicle *)m_pGroundPhysical.Get();

		if (pVeh->GetVehicleType() == VEHICLE_TYPE_CAR && pVeh->IsLawEnforcementCar() && pVeh->GetDriver() && pVeh->GetDriver()->GetPedType() == PEDTYPE_COP)
		{
			if (!(fwTimer::GetSystemFrameCount()&63))
			{
//				GetPlayerData()->m_Wanted.RegisterCrime(CRIME_STAND_ON_POLICE_CAR, GetPosition(), (u32) pVeh);
				CCrime::ReportCrime(CRIME_STAND_ON_POLICE_CAR, pVeh, this);
			}
		}
	}

	GetPlayerWanted()->Update(GetPosition(), GetVehiclePedInside() );
	
	// Any expired references need to go
#if REGREF_VALIDATION
	RemoveAllInvalidKnownRefs();
#endif // REGREF_VALIDATION
	
	//mjc-moved GetTargeting().Process();

	// let the player fire his gun when ragdolling
	weaponAssert(GetWeaponManager());
	if(pControl && GetUsingRagdoll())
	{
		CObject* pWeaponObject = GetWeaponManager()->GetEquippedWeaponObject();
		if(pWeaponObject && pWeaponObject->GetWeapon() && pWeaponObject->GetWeapon()->GetWeaponInfo()->GetIsGun())
		{
			if(pControl->GetPedAttack().IsDown())
			{
				bool bCanFire = true;
				CTask * pTaskDrive = GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_DRIVE);
				CTask * pTaskJumpRollFromVehicle = GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE);
				if(pTaskDrive || pTaskJumpRollFromVehicle)
					bCanFire = false;

				if(bCanFire)
				{
					Vector3 vecStart, vecEnd;
					Matrix34 m = MAT34V_TO_MATRIX34(pWeaponObject->GetMatrix());
					pWeaponObject->GetWeapon()->CalcFireVecFromGunOrientation(pWeaponObject, m, vecStart, vecEnd);
					pWeaponObject->GetWeapon()->Fire(this, m);
				}
			}
		}
	}

	// RECHARGE SPRINT ENERGY
	if(m_eMoveState!=PEDMOVE_NONE)
	{
		// Restore sprint energy if not running or sprinting
		if(m_eMoveState != PEDMOVE_RUN && m_eMoveState != PEDMOVE_SPRINT)
			HandleSprintEnergy(false, 1.0f);
		// Restore sprint energy at a lower rate if running but not sprinting
		else if(m_eMoveState == PEDMOVE_RUN)
			HandleSprintEnergy(false, 0.3f);
	}
	else if(m_PedAiFlags.bInVehicle && m_pMyVehicle /*RAGE && m_pMyVehicle->GetVehicleType()!=VEHICLE_TYPE_BMX*/)
		HandleSprintEnergy(false, 1.0f);

	if(GetIsDeadOrDying())
	{
		//mjc - IMPORTANT - PUT THIS BACK IN ONCE FUNCTION HAS BEEN MOVED	GetTargeting().ClearLockOnTarget();
		return true;
	}
	
	if(GetWeaponManager()->GetEquippedWeapon() && GetWeaponManager()->GetEquippedWeapon()->GetState() == CWeapon::STATE_FIRING)
	{
		GetPlayerData()->m_LastTimeFiring = fwTimer::GetTimeInMilliseconds();
	}
	
	// Every so often if we are inside an interior & standing on an object, we create a shocking event.
	// This is intended to make peds look if we're standing on tables, etc. (however it might not work
	// quite as planned as many of the interior furniture is placed as part of the building)
	if(GetPortalTracker()->IsInsideInterior() && m_pGroundPhysical && m_pGroundPhysical->GetIsTypeObject())
	{
		m_fTimeToNextCreateInteriorShockingEvents -= fwTimer::GetTimeStep();
		if(m_fTimeToNextCreateInteriorShockingEvents <= 0.0f)
		{
			CShockingEventsManager::Add(ERunningPed, GetPosition(), this);
			m_fTimeToNextCreateInteriorShockingEvents = ms_fInteriorShockingEventFreq;
		}
	}

	// Store whether we were firing this frame
	m_PedConfigFlags.bWasFiring = IsFiring();

	return true;
} // end - CPlayerPed::ProcessControl

#endif //mjc


#if 0 //mjc
bool CPlayerPed::IsHidden() const
{
	#define HIDDEN_THRESHOLD (0.05f)
	return false;//(!m_PedAiFlags.bInVehicle && GetLightingTotal() <= HIDDEN_THRESHOLD);
}
#endif //mjc

#if 0 //mjc
#if !__FINAL
// Name			:	PutPlayerAtCoords
// Purpose		:	Places player at passed coords (used in varconsole)
void CPlayerPed::PutPlayerAtCoords(float x, float y, float z)
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	pPlayer->Teleport(Vector3(x,y,z), pPlayer->GetCurrentHeading());
}
#endif  // !__FINAL
#endif //mjc

#if 0 //mjc - not used
//
float CPlayerPed::DoWeaponSmoothSpray()
{
	return -1.0f;	
}
#endif //mjc

#if 0 //mjc - not used
//
// name:		DoesPlayerWantNewWeapon
// description:	Does the player want this weapon
bool CPlayerPed::DoesPlayerWantNewWeapon(u32 UNUSED_PARAM(newWeaponHash), bool UNUSED_PARAM(bInCar))
{
#if 0 // CS
	s32 slot = CWeaponInfoManager::GetItemInfo<CWeaponInfo>(newWeapon)->GetWeaponSlot();
	u32 playersWeaponHash = GetWeaponMgr()->GetWeaponTypeInSlot((eWeaponSlot)slot);
	
	// if already have the same weapon or don't have a weapon in that slot can pick it up
	if(playersWeapon == newWeapon || playersWeapon == WEAPONTYPE_UNARMED)
		return true;
	
	// don't give weapon if in car
	if(bInCar)
		return false;
#endif // 0
	return true;
}
#endif //mjc

#if 0 //mjc
// Name			:	ResurrectPlayer
// Purpose		:	Resurrects the player at the given coords
// Parameters	:	NewCoors - the coords the player has to be resurrected at, on the ground.
//					fNewHeading - the player's heading after resurrection 
// Returns		:	Nothing

void CPlayerPed::Resurrect(const Vector3& NewCoors, float fNewHeading)
{
	// Only reactivate the players physics if currently inactive
	if( !GetPhysicsInst()->IsInLevel() )
	{
		AddPhysics();
	}

	Assert(GetPhysicsInst()->IsInLevel());

	//
	// DETACH PLAYER FROM VEHICLE, ETC:
	//
	SetPedOutOfVehicle(CPed::PVF_Warp);

	if(GetIsAttached())
	{
		DetachFromParent(0);
	}

	Assert(m_PedAiFlags.bInVehicle == false);
	Assert(!GetIsAttached());

	//
	// CLEANUP ANY RENDERING / VISUAL EFFECTS:
	//
	ClearDamage();
	GetPropData().SetSkipRender(false);
	m_footWetnessInfo[0].Init();
	m_footWetnessInfo[1].Init();
	m_nPhysicalFlags.bRenderScorched = false;

	g_ptFxManager.RemovePtFxFromEntity(this);
	g_fireMan.ExtinguishEntityFires(this);

	//
	// CLEANUP ANIMATION:
	//
	// Remove any higher priority animation
	GetAnimBlender()->RemovePlayersByPriority(AP_MEDIUMLOW);
	GetAnimBlender()->RemovePlayersByPriority(AP_MEDIUM);
	GetAnimBlender()->RemovePlayersByPriority(AP_HIGH);

	// get rid of ragdoll anim frame
	GetAnimBlender()->GetRagdollFrame()->SetBlend(0.0f);

	// The player may have been posed in a different animation state, set posed to false so the current animation state is forced after the camera update
	GetAnimDirector()->SetPosed(false);

	RestoreHeadingChangeRate();

	//
	// TELEPORT
	//

	// we have to add on ped's root offset because the spawn point is on the ground
	Teleport(NewCoors+Vector3(0.0f, 0.0f, PED_HUMAN_GROUNDTOROOTOFFSET), fNewHeading);

	// If the player doesn't actually exist in the world (because he died in a car) we'd have to add him in again.
	CGameWorld::Remove(this);
	CGameWorld::Add(this, CGameWorld::OUTSIDE );
	
	// prevent the player sinking into the ground for one frame:
	SetIsStanding(true);
	SetWasStanding(true);
	SetGroundOffsetForPhysics( -PED_HUMAN_GROUNDTOROOTOFFSET );
	
	m_fTimeBeenSprinting = 0.0f;
	m_fSprintStaminaUpdatePeriod = 0.0f;
	m_fSprintStaminaDurationMultiplier = 1.0f;
	m_fTimeBeenSwimming = 0.0f;
	m_fSwimLungCapacityUpdatePeriod = 0.0f;

	//
	// RESET PED STATE
	//
	if (!IsNetworkClone()) // a clone always has its state updated via sync messages 
	{
#if __DEV
		m_nDEflags.bCheckedForDead = true;
#endif
		SetIsVisible( true );
		SetTimeOfDeath(0);
		SetHealth(GetPlayerInfo()->MaxHealth);
		SetArmour(0.0f);
		SetWeaponDamageEntity(NULL);
		GetPlayerWanted()->Reset();
		ResetSprintEnergy();
		DisableBikeHelmet();

		ClearBaseFlag(fwEntity::REMOVE_FROM_WORLD);
		m_PedConfigFlags.bBlockWeaponSwitching = false;
		GetTargeting().ClearLockOnTarget();
		SetInitialState();
		SetVelocity(0.0f, 0.0f, 0.0f);

		// Clear all gadgets
		weaponAssert(GetWeaponManager());
		GetWeaponManager()->DestroyEquippedGadgets();

		if (GetNetworkObject())
		{
			static_cast<CNetObjPlayer *>(GetNetworkObject())->Resurrect(NewCoors);
		}
	}

	//
	// RESET PLAYER INFO STATE:
	//
	GetPlayerInfo()->ResurrectPlayer();
}

#endif //mjc


#if 0 //mjc
// Name			:	Busted
// Purpose		:	Puts the player into 'arrested' state
// Parameters	:	None
// Returns		:	Nothing

void CPlayerPed::Busted()
{
	GetPlayerWanted()->m_nWantedLevel = 0;
} // end - CPlayerPed::Busted

void CPlayerPed::SetWantedLevel(s32 NewLevel, s32 delay)
{
	Assert(GetPlayerWanted());
	GetPlayerWanted()->SetWantedLevel(GetPosition(), NewLevel, delay);
}

void CPlayerPed::SetWantedLevelNoDrop(s32 NewLevel, s32 delay)
{
	Assert(GetPlayerWanted());
	GetPlayerWanted()->SetWantedLevelNoDrop(GetPosition(), NewLevel, delay);
}

void CPlayerPed::CheatWantedLevel(s32 NewLevel)
{
	Assert(GetPlayerWanted());
	GetPlayerWanted()->CheatWantedLevel(NewLevel);
}

void CPlayerPed::SetOneStarParoleNoDrop()
{
	Assert(GetPlayerWanted());
	GetPlayerWanted()->SetOneStarParoleNoDrop();
}
#endif //mjc

#if 0 //mjc
// Name			:	KeepAreaAroundPlayerClear
// Purpose		:	When a player is in a cutscene, tries to keep the area around him clear of peds and cars
// Parameters	:	
// Returns		:	

void CPlayerPed::KeepAreaAroundPlayerClear()
{
	// check peds
	int i;
/*RAGE	
	CEntity** ppNearbyPeds=GetPedIntelligence()->GetNearbyPeds();
	const int N=GetPedIntelligence()->GetMaxNumPedsInRange();
	for(i=0;i<N;i++)
	{
		CEntity* pNearbyEntity=ppNearbyPeds[i];
		if(pNearbyEntity)
		{
			Assert(pNearbyEntity->GetType()==ENTITY_TYPE_PED);
			CPed* pNearbyPed=(CPed*)pNearbyEntity;
			
			if(pNearbyPed->PopTypeIsRandom() && 
			   !pNearbyPed->m_PedAiFlags.bInVehicle &&
			   !pNearbyPed->IsDead() &&
			   !CPedGroups::ms_groups[0].GetGroupMembership()->IsMember(pNearbyPed))
			{
				if(pNearbyPed->GetIsOnScreen() && !pNearbyPed->m_PedConfigFlags.bKeepTasksAfterCleanUp)
				{							
					//Get the nearby ped to flee the player.
					//Work out first if the ped is already fleeing the player or has already been told to flee the player.
					bool bHasAddedEvent=false;
					if(!bHasAddedEvent)
					{
						CTask* pTask=pNearbyPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_SMART_FLEE);
						if(pTask)
						{
							CTaskComplexSmartFleeEntity* pTaskFlee=(CTaskComplexSmartFleeEntity*)pTask;
							if(pTaskFlee->GetFleeEntity()==this)
							{
								bHasAddedEvent=true;
							}
						}
					}
					if(!bHasAddedEvent)
					{
						CEvent* pEvent=pNearbyPed->GetPedIntelligence()->GetEventOfType(CEventTypes::EVENT_SCRIPT_COMMAND);
					   	if(pEvent)
					   	{				   	
						   	CEventScriptCommand* pEventScriptCommand=(CEventScriptCommand*)pEvent;
							if(pEventScriptCommand->GetTask() && pEventScriptCommand->GetTask()->GetTaskType()==CTaskTypes::TASK_SMART_FLEE)
							{
								bHasAddedEvent=true;
							}
						}
					}
						
					if(!bHasAddedEvent)
					{						   
					   //Make the ped get out of the way by fleeing the player.
						const float fSafeDistance=1000.0f;
						const int iFleeTime=100000;
						CTaskComplexSmartFleeEntity* pTask=rage_new CTaskComplexSmartFleeEntity(this,false,fSafeDistance,iFleeTime);
						pTask->SetMoveState(PEDMOVE_WALK);
						CEventScriptCommand event(PED_TASK_PRIORITY_PRIMARY,pTask);
						pNearbyPed->GetPedIntelligence()->AddEvent(event);
					}
				}
				else
				{
					pNearbyPed->FlagToDestroyWhenNextProcessed();
				}
			}
		}
	}*/
	
	CEntity		*ppResults[8]; // whatever.. temp list
	CVehicle	*pVehicle;
	s32		Num;
	Vector3		vecDistance, vecPosition;

	
	if(m_PedAiFlags.bInVehicle && m_pMyVehicle)
		vecPosition = m_pMyVehicle->GetPosition();
	else
		vecPosition = GetPosition();
	
	// check for threat peds in cars
	CGameWorld::FindObjectsInRange(Vector3(GetPosition()), 15.0f, true, &Num, 6, ppResults, 0, 1, 0, 0, 0);	// Just Cars we're interested in
	// cars in range stored in ppResults

	for(i=0; i<Num; i++) // find cars close by
	{
		pVehicle = (CVehicle *)ppResults[i];

		if(!pVehicle->PopTypeIsMission() && 
		pVehicle->GetStatus() != STATUS_PLAYER &&
		pVehicle->GetStatus() != STATUS_PLAYER_DISABLED)
		{
			vecDistance = pVehicle->GetPosition() - vecPosition;		
			
			// cars are far enough away, stop them moving
			if(vecDistance.Mag2() > 5.0f*5.0f)
			{
				// Tell cars to stop for 5 secs so that they will automatically start driving again after a while
				aiTask *pTask = rage_new CTaskVehicleWait(fwTimer::GetTimeInMilliseconds() + 5000);
				pVehicle->GetIntelligence()->GetTaskManager()->SetTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
			}
			else
			{
				// Depending on whether the car is facing away or towards the player we will reverse or go foward a bit
				if ( (pVehicle->GetB().x * (vecPosition.x - pVehicle->GetPosition().x)) +
					 (pVehicle->GetB().y * (vecPosition.y - pVehicle->GetPosition().y)) > 0.0f)
				{	// reverse away
					aiTask *pTask = rage_new CTaskVehicleReverse(fwTimer::GetTimeInMilliseconds() + 2000, CTaskVehicleReverse::Reverse_Opposite_Direction);
					pVehicle->GetIntelligence()->GetTaskManager()->SetTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
				}
				else
				{	// go forward
					aiTask *pTask = rage_new CTaskVehicleGoForward(fwTimer::GetTimeInMilliseconds() + 2000);
					pVehicle->GetIntelligence()->GetTaskManager()->SetTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
				}

			}

		}
		
	}
}
#endif //mjc

#if 0 //mjc
void  CPlayerPed::CleanupMissionState(const GtaThread* pThread)
{
	SetAutoConversationLookAts(false);

	m_PedAiFlags.bDontUseVehicleSpecificAnims = false;
	m_PedConfigFlags.bAllowLockonToFriendlyPlayers = false;
	m_PedConfigFlags.bAllowLockonToRandomPeds = false;
	m_PedConfigFlags.bBlockWeaponSwitching = false;
	m_PedConfigFlags.nPedLegIkMode = LEG_IK_FULL;
	m_PedConfigFlags.bWillFlyThroughWindscreen = true;

	if (m_PedConfigFlags.nCantBeKnockedOffBike != KNOCKOFFBIKE_DEFAULT)
	{
		ped_commands::SetPedCanBeKnockedOffBike(this, KNOCKOFFBIKE_DEFAULT);
	}

	SetIsInStealthMode(false);

	// Set the player's group back to its default values
	int PlayersGroup = GetPlayerData()->m_PlayerGroup;
	CPedGroups::ms_groups[PlayersGroup].GetGroupMembership()->SetMaxSeparation(CPedGroupMembership::ms_fPlayerGroupMaxSeparation);

	g_ScriptAudioEntity.StopPedSpeaking(this, false);
	GetSpeechAudioEntity()->SetPedIsBlindRaging(false);

	if (AssertVerify(m_pPlayerInfo))
	{
		m_pPlayerInfo->m_fForceAirDragMult = 0.0f;
		m_pPlayerInfo->bCanDoDriveBy = true;
		m_pPlayerInfo->bCanUseCover = true;
		m_pPlayerInfo->bEnableControlOnDeath = true;

		m_pPlayerInfo->PlayerPedData.m_Wanted.m_DontDispatchCopsForThisPlayer = false;
		m_pPlayerInfo->PlayerPedData.m_Wanted.m_fMultiplier = 1.0f;

		if (AssertVerify(pThread) && CTheScripts::GetNumberOfMiniGamesInProgress() == 0 && pThread->bSetPlayerControlOnInMissionCleanup)
		{
			m_pPlayerInfo->PlayerPedData.m_Wanted.m_AllRandomsFlee = FALSE;
			m_pPlayerInfo->PlayerPedData.m_Wanted.m_PoliceBackOff = FALSE;
			m_pPlayerInfo->PlayerPedData.m_Wanted.m_EverybodyBackOff = FALSE;
			m_pPlayerInfo->PlayerPedData.m_Wanted.m_IgnoreLowPriorityShockingEvents = FALSE;
			m_pPlayerInfo->SetPlayerControl(false, false);
		}
	}

	// If the player is using a mobile phone then end the phonecall now
	CTask *pTaskPrimary = GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE);

	if(pTaskPrimary && pTaskPrimary->GetTaskType()==CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE)
	{
		CTaskComplexUseMobilePhone *pTaskPhone=(CTaskComplexUseMobilePhone*)pTaskPrimary;
		pTaskPhone->Stop(this);
	}
}
#endif //mjc

#if 0 //mjc
bool CPlayerPed::IsPlayerPressingHorn() const
{
    if(IsNetworkClone())
    {
        CNetObjPlayer *netObjPlayer = static_cast<CNetObjPlayer *>(GetNetworkObject());
        return netObjPlayer->IsPlayerPressingHorn();
    }
    else
    {
        return GetIsDrivingVehicle() && CControlMgr::GetMainPlayerControl().GetVehicleHorn().GetValue();
    }
}
#endif

#if 0
bool CPlayerPed::AffectedByWaterCannon()
{
	if(m_fHitByWaterCannonImmuneTimer > 0.0f)
		return false;
	else
		return true;
}

void CPlayerPed::RegisterHitByWaterCannon()
{
	// Add on cool down rate to cancel out constant decrement in ProcessControl
	m_fHitByWaterCannonTimer += fwTimer::GetTimeStep()*(1.0f + PLAYER_WATER_CANNON_COOLDOWN_RATE);

	if(m_fHitByWaterCannonTimer > PLAYER_MAX_TIME_HIT_BY_WATER_CANNON)
	{
		m_fHitByWaterCannonImmuneTimer = PLAYER_WATER_CANNON_IMMUNE_TIME;
		m_fHitByWaterCannonTimer = 0.0f;
	}
}
#endif

#if 0 //mjc
void CPlayerPed::StartAnimUpdate(float fTimeStep)
{
	CDynamicEntity::StartAnimUpdate(fTimeStep);
	
	// TODO JA - This all needs to be data driven. Need to support multiple sliders 
	TUNE_GROUP_FLOAT(FAT_SLIDER, fFatSlider, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_BOOL(FAT_SLIDER, bFatSliderOveride, false);

	if (m_pExternallyDrivenDOFFrame)
	{
		if( false == bFatSliderOveride )
		{ // Query Fat Stats
			fFatSlider = 0.0f;
			int hash = ((CPedModelInfo*)CModelInfo::GetModelInfo(GetModelIndex()))->GetStatsHashId("FAT");
			if( hash != -1 )
			{
				if( StatsInterface::IsKeyValid(hash) )
				{
					Assertf(StatsInterface::GetBaseTypeIsInt(hash) == true,"wrong type for player FAT Stats. a float was expected (%d,%d)",hash,StatsInterface::GetStatType(hash));
					fFatSlider = ((float)StatsInterface::GetIntStat(hash)/100.0f);
				}
			}
		}
		// The values below identify <id>Fat_Slider</id> 
		// See GTA5\assets\anim\expressions\test\fat_test.xml
		static u8 track = (u8)kTrackFacialTranslation;
		static u16 id = (u16)3021;
		Vector3 vFatSlider(fFatSlider, fFatSlider, fFatSlider);
		m_pExternallyDrivenDOFFrame->SetVector3(track, id, RCC_VEC3V(vFatSlider)); 
	}
}
#endif //mjc

#if 0 //mjc
void CPlayerPed::SetupDrivenDOF()
{
	// TODO JA - This all needs to be data driven. Need to support multiple sliders 
	Assertf(m_pExternallyDrivenDOFFrame==NULL, "ExternallyDrivenDOFFrame already exists");
	m_pExternallyDrivenDOFFrame = rage_new crFrame(); 
	Assertf(m_pExternallyDrivenDOFFrame, "Failed to create ExternallyDrivenDOFFrame");

	// The values below identify <id>Fat_Slider</id> 
	// See GTA5\assets\anim\expressions\test\fat_test.xml
	static u8 track = (u8)kTrackFacialTranslation;
	static u16 id = (u16)3021;
	static u8 type = 0;
	m_pExternallyDrivenDOFFrame->InitCreateDofs(1, &track, &id, &type, false, NULL);
	m_pExternallyDrivenDOFFrame->SetVector3(track, id, Vec3V(V_ONE)); 

	static CMovePed::Id s_FrameId = CMovePed::CalcId("ExternallyDrivenDOFFrame");
	m_pAnimDirector->GetMove().SetFrame(s_FrameId, m_pExternallyDrivenDOFFrame);
}
#endif //mjc

#if 0 //mjc
void CPlayerPed::DeleteDrivenDOF()
{
	if (m_pExternallyDrivenDOFFrame)
	{
		delete m_pExternallyDrivenDOFFrame;
	}
	m_pExternallyDrivenDOFFrame = NULL;
}
#endif //mjc

//mjc u8 CPlayerPed::PLAYER_SOFT_AIM_BOUNDARY = 238;
//mjc bool CPlayerPed::REVERSE_AIM_BOUNDARYS = false;
//mjc bool CPlayerPed::ALLOW_RUN_AND_GUN = false;
//mjc u8 CPlayerPed::PLAYER_SOFT_TARGET_SWITCH_BOUNDARY = 125;

#if 0 //mjc
//-------------------------------------------------------------------------
// Returns true if the player is not holding the aim button fully
//-------------------------------------------------------------------------
bool CPlayerInfo::IsFiring()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) 
	{ 
		return false; 
	}

	CControl* pControl = pPlayer->GetControlFromPlayer();

	u8 iTestValue = PLAYER_SOFT_AIM_BOUNDARY;

	s32 iAttackValue = 0;

	bool bInVehicle = false;

	if( pPlayer->m_PedAiFlags.bInVehicle && pPlayer->GetMyVehicle())
	{
		taskAssert(pPlayer->GetMyVehicle()->GetLayoutInfo());
		const bool bVehicleHasDriver = pPlayer->GetMyVehicle()->GetLayoutInfo()->GetHasDriver();

		if (bVehicleHasDriver)
		{
			bInVehicle = true;

			if( pPlayer->GetIsDrivingVehicle() )
			{
				if (pControl->GetVehicleAttack().IsDown())
				{
					iAttackValue = MAX(pControl->GetVehicleAttack().GetValue(), pControl->GetVehicleAttack2().GetValue());
				}
			}
			else
			{
				// Normal On Foot controls for passengers
				bInVehicle = false;
			}
		}
	}
	
	if (!bInVehicle)
	{
		iAttackValue = MAX(pControl->GetPedAttack2().GetValue(), pControl->GetPedAttack().GetValue());
	}

	return iAttackValue >= iTestValue;
}

bool CPlayerInfo::IsSoftFiring()
{
	const CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

	const CControl* pControl = pPlayer->GetControlFromPlayer();

	const u8 iFireTestHighValue	= PLAYER_SOFT_AIM_BOUNDARY - 1;
	const u8 iFireTestLowValue	= 10;

	const s32 iAttackValue = MAX(pControl->GetPedAttack2().GetValue(), pControl->GetPedAttack().GetValue());

	return (iAttackValue > iFireTestLowValue && iAttackValue <= iFireTestHighValue);
}

//-------------------------------------------------------------------------
// Returns true if the player is not in a vehicle or isn't driving a vehicle
//-------------------------------------------------------------------------
bool CPlayerInfo::IsNotDrivingVehicle()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) 
	{ 
		return false; 
	}

	if( pPlayer->m_PedAiFlags.bInVehicle && pPlayer->GetMyVehicle())
	{
		Assert(pPlayer->GetMyVehicle()->GetLayoutInfo());
		const bool bVehicleHasDriver = pPlayer->GetMyVehicle()->GetLayoutInfo()->GetHasDriver();

		if (bVehicleHasDriver && pPlayer->GetIsDrivingVehicle())
		{
			return false;
		}
	}

	return true;
}

//-------------------------------------------------------------------------
// Returns true if the player is aiming at all
//-------------------------------------------------------------------------
bool CPlayerInfo::IsAiming(const bool bConsiderAttackTriggerAiming)
{
	return IsSoftAiming(bConsiderAttackTriggerAiming) || IsHardAiming() || IsForcedAiming();
}

//-------------------------------------------------------------------------
// Returns true if the player is not holding the aim button fully
//-------------------------------------------------------------------------
bool CPlayerInfo::IsSoftAiming(const bool bConsiderAttackTriggerAiming)
{
	const CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

	const CControl* pControl = pPlayer->GetControlFromPlayer();

	u8 iAimTestHighValue	= PLAYER_SOFT_AIM_BOUNDARY - 1;
	u8 iAimTestLowValue		= 10;

	if( REVERSE_AIM_BOUNDARYS )
	{
		iAimTestLowValue	= iAimTestHighValue;
		iAimTestHighValue	= 255;
	}

	const s32 iTargetValue = pControl->GetPedTarget().GetValue();

	return (iTargetValue > iAimTestLowValue && iTargetValue <= iAimTestHighValue) || (bConsiderAttackTriggerAiming && IsSoftFiring());
}

//-------------------------------------------------------------------------
// Returns true if the player is  holding the aim button fully
//-------------------------------------------------------------------------
bool CPlayerInfo::IsHardAiming()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }
	CControl* pControl = pPlayer->GetControlFromPlayer();

	u8 iAimTestHighValue	= 255;
	u8 iAimTestLowValue		= PLAYER_SOFT_AIM_BOUNDARY;
	if( REVERSE_AIM_BOUNDARYS )
	{
		iAimTestHighValue	= iAimTestLowValue;
		iAimTestLowValue	= 1;
	}

	return pControl->GetPedTarget().GetValue() >= iAimTestLowValue &&
		pControl->GetPedTarget().GetValue() <= iAimTestHighValue;
}

bool CPlayerInfo::IsForcedAiming()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

	//CDE - The camera system now provides a view mode that specifies permanent soft-aim.
	bool shouldForceAimingForCameraSystem = camInterface::GetGameplayDirector().ShouldForceAiming(*pPlayer);

	return pPlayer->m_PedAiFlags.bForcedAim || shouldForceAimingForCameraSystem;
}

//-------------------------------------------------------------------------
// Returns true if the player is  holding the aim button fully
//-------------------------------------------------------------------------
bool CPlayerInfo::IsJustHardAiming()
{
	if( !IsHardAiming() )
	{
		return false;
	}

	// IF the player is hard aiming now, return true if they weren't last frame
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }
	CControl* pControl = pPlayer->GetControlFromPlayer();

	u8 iAimTestHighValue	= 255;
	u8 iAimTestLowValue		= PLAYER_SOFT_AIM_BOUNDARY;
	if( REVERSE_AIM_BOUNDARYS )
	{
		iAimTestHighValue	= iAimTestLowValue;
		iAimTestLowValue	= 1;
	}

	return pControl->GetPedTarget().HistoryPressed(fwTimer::GetTimeStepInMilliseconds()*2, NULL, iAimTestLowValue, iAimTestHighValue);
}

bool CPlayerInfo::IsReloading()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }
	CControl* pControl = pPlayer->GetControlFromPlayer();
	return pControl->GetPedReload().IsDown();
}

//-------------------------------------------------------------------------
// Returns true if the player is  trying to select the next target to the left
//-------------------------------------------------------------------------
bool CPlayerInfo::IsSelectingLeftTarget(bool bCheckAimButton)
{
	// IF the player is hard aiming now, return true if they weren't last frame
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }
	CControl* pControl = pPlayer->GetControlFromPlayer();

	bool bAssistedAiming = pPlayer->GetPlayerData()->m_bAssistedAiming;

	if( (!bCheckAimButton || IsHardAiming() || bAssistedAiming) &&
		CTaskPlayerOnFoot::ms_bAnalogueLockonAimControl )
	{
		if( pControl->GetAxisValue(pControl->GetPedAimWeaponLeftRight()) < -PLAYER_SOFT_TARGET_SWITCH_BOUNDARY )
		{
			if( pControl->GetPedAimWeaponLeftRight().HistoryPressed(fwTimer::GetTimeStepInMilliseconds(), NULL, 0, 128-PLAYER_SOFT_TARGET_SWITCH_BOUNDARY) )
// 				pControl->GetPedAimWeaponLeftRight().HistoryPressed(fwTimer::GetTimeStepInMilliseconds()*TARGET_SWITCH_FRAME_COUNT, NULL, 0, 100) )
			{
				return true;
			}
		}
	}

	return false;
}

//-------------------------------------------------------------------------
// Returns true if the player is  trying to select the next target to the right
//-------------------------------------------------------------------------
bool CPlayerInfo::IsSelectingRightTarget(bool bCheckAimButton)
{
	// IF the player is hard aiming now, return true if they weren't last frame
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }
	CControl* pControl = pPlayer->GetControlFromPlayer();

	bool bAssistedAiming = pPlayer->GetPlayerData()->m_bAssistedAiming;

	if( (!bCheckAimButton || IsHardAiming() || bAssistedAiming) &&
		CTaskPlayerOnFoot::ms_bAnalogueLockonAimControl )
	{
		if( (float)pControl->GetAxisValue(pControl->GetPedAimWeaponLeftRight()) > PLAYER_SOFT_TARGET_SWITCH_BOUNDARY )
		{
			if( pControl->GetPedAimWeaponLeftRight().HistoryPressed(fwTimer::GetTimeStepInMilliseconds(), NULL, 255-(128-PLAYER_SOFT_TARGET_SWITCH_BOUNDARY), 255) )
// 				pControl->GetPedAimWeaponLeftRight().HistoryPressed(fwTimer::GetTimeStepInMilliseconds()*TARGET_SWITCH_FRAME_COUNT, NULL, 150, 255) )
			{
				return true;
			}
		}
	}

	return false;
}

#endif	//mjc

#if 0
//-------------------------------------------------------------------------
// Creates the game player, if the player already exists sets the player health.
//-------------------------------------------------------------------------
int 
CGameWorld::CreatePlayer(Vector3& pos, const char* name, const float health)
{
	if (PARAM_noPlayer.Get())
		return 0;

	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	if (!pPlayerPed && AssertVerify(name))
	{
		s32 index = CModelInfo::GetModelIndex(name);
		if (!CStreaming::HasObjectLoaded(index, CModelInfo::GetStreamingModuleId()))
		{
			CStreaming::RequestObject(index, CModelInfo::GetStreamingModuleId(), STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		// need to fix for network stuff:
			Matrix34 tempMat;
			tempMat.Identity();

			const CControlledByInfo localPlayerControl(false, true);
			pPlayerPed = CPedFactory::GetFactory()->CreatePlayerPed(localPlayerControl, index, &tempMat);
			Assert(pPlayerPed);

		pPlayerPed->PopTypeSet(POPTYPE_PERMANENT);
		pPlayerPed->SetDefaultDecisionMaker();
		pPlayerPed->SetCharParamsBasedOnManagerType();

		// Correct Zeta position to match the ground floor
		if (pos.z <= INVALID_MINIMUM_WORLD_Z)
		{
			pos.z = CWorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, pos.x, pos.y);
		}
		pos.z += pPlayerPed->GetDistanceFromCentreOfMassToBaseOfModel();

		CGameWorld::AddPlayerToWorld(pPlayerPed, pos);

		// CStreaming::SetMissionDoesntRequireObject(index, CModelInfo::GetStreamingModuleId());
		CStreaming::SetObjectIsDeletable(index, CModelInfo::GetStreamingModuleId());

#if USE_NEW_MOTION_TASKS
		pPlayerPed->GetPedIntelligence()->AddTaskDefault(pPlayerPed->GetCurrentMotionTask()->CreatePlayerControlTask());
#else // USE_NEW_MOTION_TASKS
		pPlayerPed->GetPedIntelligence()->AddTaskDefault(pPlayerPed->GetMoveBlender()->CreatePlayerControlTask());
#endif // USE_NEW_MOTION_TASKS
	}

	// DEBUG-PH 
	// rage: the player spawns with 0 health as default
	// temporarily increase this to 100.0f
	pPlayerPed->SetHealth(health);
	// END DEBUG

#if __DEV
	// This flag is used and checked on certain script commands.
	pPlayerPed->m_nDEflags.bCheckedForDead = true;
#endif

	if (pPlayerPed->GetPlayerInfo())
	{
		pPlayerPed->GetPlayerInfo()->SetPlayerControl(false, false);
	}

	// local player always created in slot 0 now
	return (0);
}
#endif //0 mjc


#endif //0 //mjc
