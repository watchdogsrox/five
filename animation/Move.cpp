#include "animation/move.h"

#include "crclip/clipplayer.h"
#include "crmotiontree/nodeblend.h"
#include "crmotiontree/nodeblendn.h"
#include "crmotiontree/nodeclip.h"

//game includes
#include "animation/AnimManager.h"
#if __DEV
#include "animation/debug/animviewer.h"
#endif //__DEV
#include "fwanimation/animdirector.h"
#include "data/resource.h"
#include "peds/ped.h"
#include "scene/DynamicEntity.h"
#include "streaming/streaming.h"
#include "streaming/streamingrequest.h"
#include "task/System/TaskHelpers.h"

#define DEFAULT_INPUT_BUFFER_SIZE (8)

#define REPLAY_VALUE_INVALID (-1)

ANIM_OPTIMISATIONS()

fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkAnimatedBuilding("AnimatedBuilding",0x2E968919);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskCowerScenario("TaskCowerScenario",0x27A42979);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkGenericClip("genericclip",0xDBDEE723);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkHelicopterRappel("HelicopterRappel",0x8DC014E6);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskRappel("TaskRappel",0x93F9598D);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkMotionDiving("MotionDiving",0x32AEC7EC);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkMotionPed("PedMotion",0x6CF8D7EB);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkMotionPedLowLod("PedMotionLowLod",0x4BDFA3D);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkMotionAnimal("AnimalMotion", 0x8C5CD983);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkMotionSwimming("MotionSwimming",0x76A32779);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkObject("Object",0x39958261);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkWeapon("Weapon",0xE68A4A98);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkPed("Ped",0x34D90761);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkPm("",0x0);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkSyncedScene("absolutemover",0x9CE5D8B7);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTask("generictask",0x8D0E389D);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectile("TaskAimAndThrow",0xA48C2B2B);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectileCover("TaskAimAndThrowCover",0x153D5103);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectileThrow("TaskAimAndThrowThrow",0x64AF1584);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAimGunBlindFire("TaskAimGunBlindFire",0x7FFBDFCF);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAimGunDriveBy("TaskAimGunDriveBy",0x49AB75E);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAimGunDriveByHeli("TaskAimGunDriveByHeli",0x828B9F2E);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAimGunOnFoot("TaskAimGunOnFoot",0x7467C0D);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAimGunScripted("TaskAimGunScripted",0x9A367645);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskBlendFromNM("TaskBlendFromNM",0xCB5D9028);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskCover("TaskCover",0xC73B662E);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskUseCover("TaskUseCover",0x6F49D9CC);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMotionInCover("TaskMotionInCover",0x63AE6AD1);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskDismountAnimal("TaskDismountAnimal",0x3EC90E1C);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskDyingDead("TaskDyingDead",0x6C89A852);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAlign("TaskAlign",0x728A1143);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskEnterVehicle("TaskEnterVehicle",0x41A81FFC);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskExitVehicle("TaskExitVehicle",0x8FF31972);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkVehicleBicycle("VehicleBicycle",0x146DD413);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMotionOnBicycle("TaskMotionOnBicycle",0xEE774E1F);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskInVehicle("TaskInVehicle",0xC7C8A738);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskInVehicleSeatShuffle("TaskInVehicleSeatShuffle",0x72E06091);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMountAnimal("TaskMountAnimal",0x5D6C313F);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMountedThrow("TaskMountAnimalThrow",0xFE1E33EB);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMountedDriveBy("TaskMountedDriveBy",0xC15108EF);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFoot("OnFootLocomotion",0xE2F1211C);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootHuman("OnFootHuman",0x22CD5DB);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootHumanLowLod("OnFootHumanLowLod",0xC1350ECC);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootBird("Bird",0xA0133787);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootQuad("OnFootQuad",0x1A1ACB80); 
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskFish("Fish",0x612920B7); 
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootFlightlessBird("FlightlessBird",0x4471CE27);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootHorse("OnFootHorse",0x2BE1741F);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskJumpMounted("TaskJumpMounted",0x3AC2AE0A);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootOpenDoor("OnFootOpenDoor",0xABBB1CFD);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootStrafing("OnFootStrafingTagSync",0xBDF615A3);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootAiming("OnFootAiming",0xF33D894A);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMotionTennis("TaskMotionTennis",0xE4B5241F);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskParachute("TaskParachute",0x9E0CDAE6);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskParachuteObject("TaskParachuteObject",0x4D3D3DF);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskReactAimWeapon("TaskReactAimWeapon",0xCD01671A);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskReactInDirection("TaskReactInDirection",0x838B2952);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskRideHorse("TaskRideHorse",0xFA420BC9);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskScriptedAnimation("TaskScriptedAnimation",0x51F366F1);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskVault("TaskVault",0x9890324D);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskDropDown("TaskDropDown",0x696F4EE4);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskFall("TaskFall",0x44FB3635);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskJetpack("TaskJetpack",0x8c1f7d54);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskFallHorse("TaskFallHorse",0x57f34ac0);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskClimbLadder("TaskClimbLadder",0xFA87E59D);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskReloadGun("TaskReloadGun",0x82202090); 
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskCutScene("TaskCutScene",0x72D80C63); 
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMobilePhone("TaskMobilePhone",0xD6DDF971);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskArrest("TaskArrestPaired",0x259178cb);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskCuffed("TaskCuffedPaired",0x9710bb9c);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskWrithe("TaskWrithe",0xEC50FD8C);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskRepositionMove("RepositionMove",0x45D8361B);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMotionAimingToOnFootTransition("OnFootAimingToOnFootTransition",0x6228AFAF);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMotionOnFootToAimingTransition("OnFootToOnFootAimingTransition",0xA599B930);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskCombatRoll("TaskCombatRoll",0x3FA32B26);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_TaskIncapacitatedNetwork("TaskIncapacitated", 0xFE76DF47);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_TaskIncapacitatedInVehicleNetwork("TaskIncapacitatedInVehicle", 0xA1A6E786);

fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkBeginScriptedMiniGames("",0x0);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkMgBenchPress("Minigame_BenchPress",0x3CE203EE);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkMgSquats("Minigame_Squats",0x9C23EAB5);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkEndScriptedMiniGames("",0x0);

fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootDuckAndCover("DuckAndCover",0x7C93CD39);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskOnFootSlopeScramble("SlopeScramble",0x91BBC7BA);

fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskGeneralSweep("TaskGeneralSweep",0x5E7AC36B);

fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskGunBulletReactions("TaskGunBulletReactions",0x97E1D31A);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskReactAndFlee("TaskReactAndFlee",0x598E451);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskShellShocked("TaskShellShocked",0x73B78C1C);

fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskShockingEventReact("TaskShockingEventReact",0xD2E3D615);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskShockingEventBackAway("TaskShockingEventBackAway",0xFE35CF0B);

fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkUpperBodyAimPitch("UpperBodyAimPitch",0x6BEC8CA2);

fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMeleeUpperbodyAnims("TaskMeleeUpperbodyAnims",0xA56A86A4);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskMeleeActionResult("TaskMeleeActionResult",0x5B8E8321);

fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskSwapWeapon("TaskSwapWeapon",0x8EC81D4A);
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkProjectileFPS("ProjectileFPS", 0xEE40D40E);

fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAmbientLowriderArm("TaskAmbientLowriderArm", 0x288fa590);

#if __BANK
fwMvNetworkDefId CClipNetworkMoveInfo::ms_NetworkTaskAttachedVehicleAnimDebug("TaskAttachedVehicleAnimDebug",0x2FB2872C);
#endif // __BANK

//////////////////////////////////////////////////////////////////////////

void CMove::PostUpdate()
{
	// nothing to see here
}

//////////////////////////////////////////////////////////////////////////

void CMove::FinishUpdate()
{
	// nothing to see here
}

//////////////////////////////////////////////////////////////////////////

void CMove::PostFinishUpdate()
{
	// nothing to see here
}

//////////////////////////////////////////////////////////////////////////
// MoveAnimatedBuilding
////////////////////////////////////////////////////////////////////////////////

CMoveAnimatedBuilding::CMoveAnimatedBuilding(CDynamicEntity& dynamicEntity)
: CMove(dynamicEntity)
, m_dictionaryIndex(-1)
REPLAY_ONLY(, m_replayPhase(REPLAY_VALUE_INVALID))
REPLAY_ONLY(, m_replayRate(REPLAY_VALUE_INVALID))
{
	//nothing more to do here
}

////////////////////////////////////////////////////////////////////////////////

CMoveAnimatedBuilding::~CMoveAnimatedBuilding()
{
	// Remember to remove the ref to the dictionary index here
	SetDictionaryIndex(-1);
}


// PURPOSE: Sets the anim to play back on this animated building
void CMoveAnimatedBuilding::SetPlaybackAnim(s16 dictionaryIndex, u32 animHash, float startTime /*= 0.0f*/)
{
	static const fwMvClipId clipId("PlaybackClip",0x23B0EE24);

	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(dictionaryIndex, animHash);

	//this will addref for us
	SetDictionaryIndex(dictionaryIndex);

	float startPhase = 0.0f;
	float rate = 1.0f;

	if (pClip)
	{
		startPhase = pClip->ConvertTimeToPhase(startTime);
	}

	SetClip(clipId, pClip);
	SetPhase(startPhase);
	SetRate(rate);
	REPLAY_ONLY(m_replayPhase = REPLAY_VALUE_INVALID;)
	REPLAY_ONLY(m_replayRate = REPLAY_VALUE_INVALID;)
}

// PURPOSE: Set the current phase of the anim
void CMoveAnimatedBuilding::SetPhase( float phase )
{
	static const fwMvFloatId phaseFloatId("PlaybackPhase",0xD6E85451);

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && m_replayPhase != REPLAY_VALUE_INVALID)
	{
		phase = m_replayPhase;
	}
#endif //GTA_REPLAY

	SetFloat( phaseFloatId, phase );
}

// PURPOSE: Get the current phase of the anim
float CMoveAnimatedBuilding::GetPhase()
{
	static const fwMvFloatId phaseFloatId("PlaybackPhaseOut",0x7CEE93A4);
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && m_replayPhase != REPLAY_VALUE_INVALID)
	{
		return m_replayPhase;
	}
#endif //GTA_REPLAY

	return GetFloat( phaseFloatId );
}

// PURPOSE: Set the playback rate of the anim
void CMoveAnimatedBuilding::SetRate( float rate )
{
	static const fwMvFloatId rateFloatId("PlaybackRate",0xAD2B14CE);

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && m_replayRate != REPLAY_VALUE_INVALID)
	{
		rate = m_replayRate;
	}
#endif //GTA_REPLAY

	SetFloat( rateFloatId, rate );
}

// PURPOSE: Get the playback rate of the anim
float CMoveAnimatedBuilding::GetRate()
{
	static const fwMvFloatId rateFloatId("PlaybackRateOut",0x83EF6397);
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && m_replayRate != REPLAY_VALUE_INVALID)
	{
		return m_replayRate;
	}
#endif //GTA_REPLAY

	return GetFloat( rateFloatId );
}

// PURPOSE: Set whether or not the anim should loop
void CMoveAnimatedBuilding::SetLooped( bool looped )
{
	static const fwMvBooleanId loopedId("PlaybackLooped",0xC7C4DDF0);

	SetBoolean( loopedId, looped );
}

////////////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CMoveAnimatedBuildingPooledObject, CONFIGURED_FROM_FILE, 0.54f, atHashString("CMoveAnimatedBuilding",0x1b7bbebe));
