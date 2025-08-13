// Title	:	PlayerPed.h
// Author	:	Gordon Speirs
// Started	:	10/12/99

#if 0 //ndef _PLAYERPED_H_
#define _PLAYERPED_H_

#include "Game\wanted.h"
#include "Network\objects\entities\NetObjPlayer.h"
#include "Peds\Ped.h"
//mjc-moved #include "Peds\PedWeapons\PlayerPedTargeting.h"
#include "System\ControlMgr.h"
 
//mjc unused #define DEBUGPLAYERPEDWEAPON(X)
//mjc unused #define DEBUGPLAYERPEDCOORDS(X)			X
//mjc unused #define DEBUGPLAYERPEDVISIBLEPEDS(X)

//#define MAX_SPRINT_ENERGY	(150.0f)
//mjc unused #define NUM_ATTACK_POINTS  (6)
//mjc unused #define ATTACK_POINT_ANGLE (TWO_PI/NUM_ATTACK_POINTS)
//mjc unused #define MELEE_ATTACK_POINT_ANGLE (TWO_PI/NUM_MELEE_ATTACK_POINTS)

//mjc unused #define WEAPON_RADIUS_MIN (0.20f)
//mjc unused #define WEAPON_RADIUS_MAX (1.00f)

// class prototypes
class CEventDamage;
class CWeapon;
class CPlayerInfo;
class CPedStreamRenderGfx;
class CCustomShaderEffectPed;
class CWeaponInfo;

namespace rage
{
	class rlGamerInfo;
}

/*mjc-unused
enum eWeaponHoldingState
{
	WEAPONHOLDINGSTATE_UNKNOWN = 0,
	WEAPONHOLDINGSTATE_NOWEAPON,
	WEAPONHOLDINGSTATE_HOLDING,
	WEAPONHOLDINGSTATE_HOLSTERING,
	WEAPONHOLDINGSTATE_HOLSTERED,
	WEAPONHOLDINGSTATE_UNHOLSTERING,
	WEAPONHOLDINGSTATE_SWAPPING_IN,
	WEAPONHOLDINGSTATE_SWAPPING_OUT,
	WEAPONHOLDINGSTATE_SWAPPING_WAITING
};*/

class CPlayerPed : public CPed
{
	friend struct CPlayerPedOffsetInfo;
protected:
	// This is PROTECTED as only CPEDFACTORY is allowed to call this - make sure you use CPedFactory!
	// DONT make this public!
	CPlayerPed(const eEntityOwnedBy ownedBy, const CControlledByInfo& pedControlInfo, s32 modelIndex);
	friend class CPedFactory;

public:
	~CPlayerPed();

	//mjc-unused static bool	bDebugPlayerInfo;
	//mjc static bool bDebugPlayerInvincible;
	//mjc static bool bDebugPlayerInvisible;
	//mjc static bool bDebugPlayerInfiniteStamina;
	//mjc static bool bHasDisplayedPlayerQuitEnterCarHelpText;
	//mjc static const float ms_fInteriorShockingEventFreq;

	//mjc bool m_bWasHeldDown;	//mjc - unused
	
#if !__FINAL
	//mjc-just use teleport static void PutPlayerAtCoords(float x, float y, float z);
#endif  // !__FINAL

	//mjc virtual void SetModelIndex(int model);

	//mjc void SetInitialState();
	//mjc virtual bool ProcessControl();

	//mjc void SetupPlayerGroup();

	//mjc bool IsNetworkBot() const;
	//mjc virtual bool IsAPlayerPed() const { return true; }

	//mjc void ResetSprintEnergy();// { m_pPlayerData->m_fSprintEnergy = m_pPlayerData->m_fMaxSprintEnergy; }
	//mjc bool HandleSprintEnergy(bool bSprint, float fRate=1.0f);
	//mjc float ControlButtonSprint(eSprintType nSprintType, bool bUseButtonBashing=true);
	//mjc float GetButtonSprintResults(eSprintType nSprintType);

	//mjc void HandleSwimmingLungCapacity();
	//mjc float GetSwimmingLungCapacityUpdatePeriod() const;

    //mjc bool IsPlayerPressingHorn() const;

	//mjc unused bool DoesPlayerWantNewWeapon(u32 NewWeaponHash, bool bInCar = false);

	//mjc unused float DoWeaponSmoothSpray();

	//mjc void Resurrect(const Vector3& NewCoors, float fNewHeading);
	//mjc void Busted();
	//mjc void SetWantedLevel(s32 NewLevel, s32 delay);
	//mjc void CheatWantedLevel(s32 NewLevel);
	//mjc void SetWantedLevelNoDrop(s32 NewLevel, s32 delay);
	//mjc s32 GetWantedLevel(void) { return (GetPlayerWanted() ? (s32)GetPlayerWanted()->GetWantedLevel() : 0); }
	//mjc s32 GetWantedLevel(void) const { return (GetPlayerWanted() ? (s32)GetPlayerWanted()->GetWantedLevel() : 0); }
	//mjc bool CopsAreSearching() { return ( GetPlayerWanted()->CopsAreSearching() ); }
	//mjc void SetOneStarParoleNoDrop();

	//mjc-not used void DebugMarkVisiblePeds();
	
	//mjc void KeepAreaAroundPlayerClear();
	
	//mjc void CleanupMissionState(const class GtaThread* pThread = NULL);

	//mjc-moved void SetPlayerCanBeDamaged(bool DmgeFlag) { GetPlayerData()->bCanBeDamaged = DmgeFlag; }

    //mjc CControl* GetControlFromPlayer();
	//mjc const CControl* GetControlFromPlayer() const;
    //mjc-moved bool CanPlayerStartMission();
    //mjc-moved void SetDisablePlayerSprint(bool bSprintFlag) {GetPlayerInfo()->SetPlayerDataPlayerSprintDisabled( bSprintFlag );}
	
	// get the ped's phone model index
	//mjc-moved virtual s32 GetPhoneModelIndex() const { return m_nPhoneModelIndex; }

	// offsets for the phone attachment
	//mjc-moved virtual const Vector3& GetPhoneAttachPosOffset() const { return m_vPhoneAttachPosOffset; }
	//mjc-moved virtual const Vector3& GetPhoneAttachRotOffset() const { return m_vPhoneAttachRotOffset; }

	// set the ped's phone model index
	//mjc-unused  virtual void SetPhoneModelIndex(s32 nPhoneModelIndex) { m_nPhoneModelIndex = nPhoneModelIndex; }

	// set the offsets for the phone attachment
	//mjc-unused  virtual void SetPhoneAttachPosOffset(const Vector3& vPosOffset) { m_vPhoneAttachPosOffset = vPosOffset; }
	//mjc-unused  virtual void SetPhoneAttachRotOffset(const Vector3& vRotOffset) { m_vPhoneAttachRotOffset = vRotOffset; }

	//mjc void RegisterHitByWaterCannon();
	//mjc bool AffectedByWaterCannon();

	//mjc bool 	IsHidden() const;

	// Dynamic cover point, moved around as the player shifts dynamically around cover.
	//mjc-moved CCoverPoint* GetDynamicCoverPoint() { return &m_dynamicCoverPoint; }
	//mjc-moved CCoverPoint			m_dynamicCoverPoint;	
	//mjc-moved bool				m_bCanMoveLeft;
	//mjc-moved bool				m_bCanMoveRight;
	//mjc-moved bool				m_bCoverGeneratedByDynamicEntity;

	// DOF
	//mjc virtual void StartAnimUpdate(float fTimeStep);
	//mjc void SetupDrivenDOF();
	//mjc void DeleteDrivenDOF();
	
	//mjc-moved float GetSprintStaminaUpdatePeriod() const;
	//mjc-moved float GetSprintStaminaMultiplier() const;

	// 	
	// Targeting accessors
	//mjc-moved const CPlayerPedTargeting& GetTargeting() const { return m_targeting; }
	//mjc-moved CPlayerPedTargeting& GetTargeting() { return m_targeting; }

	//mjc-moved static bool IsNotDrivingVehicle();
	//mjc-moved static bool IsFiring();
	//mjc-moved static bool IsSoftFiring();
	//mjc-moved static bool IsAiming(const bool bConsiderAttackTriggerAiming = true);
	//mjc-moved static bool IsSoftAiming(const bool bConsiderAttackTriggerAiming = true);
	//mjc-moved static bool IsHardAiming();
	//mjc-moved static bool IsForcedAiming();
	//mjc-moved static bool IsJustHardAiming();
	//mjc-moved static bool IsReloading();
	//mjc-moved static bool IsSelectingLeftTarget(bool bCheckAimButton = true);
	//mjc-moved static bool IsSelectingRightTarget(bool bCheckAimButton = true);

	// PURPOSE
	//    Creates the game player, if the player already exists sets the player health.
	//    Player Zeta position is corrected for the right height to the floor.
	// RETURN
	//    The slot number were the player was created.
	//mjc-moved static int  CreatePlayer(Vector3& pos, const char* name = "player_zero", const float health = 200.0f);

	//mjc static u8 PLAYER_SOFT_AIM_BOUNDARY;
	//mjc static bool REVERSE_AIM_BOUNDARYS;
	//mjc static u8 PLAYER_SOFT_TARGET_SWITCH_BOUNDARY;
	//mjc static bool ALLOW_RUN_AND_GUN;

#if __ASSERT
    //mjc static void SetChangingPlayerModel(bool changingPlayerModel) { m_ChangingPlayerModel = changingPlayerModel; };
    //mjc static bool IsChangingPlayerModel()                          { return m_ChangingPlayerModel; };
#endif // __ASSERT

	// ============================= NETWORK CODE ========================================

	NETWORK_OBJECT_TYPE_DECL( CNetObjPlayer, NET_OBJ_TYPE_PLAYER );

private:
	//mjc u32 m_timeGunLastFiredDuringRagdoll;	//mjc unused

//// MJC - START PHONE
	// The current phone model
	//mjc s32 m_nPhoneModelIndex;

	// Offsets for the phone attachment
	//mjc Vector3 m_vPhoneAttachPosOffset;
	//mjc Vector3 m_vPhoneAttachRotOffset;
//// MJC - END PHONE

	// Timers to avoid player getting stuck permanently hit by water cannon
	//mjc float m_fHitByWaterCannonTimer;			// Total time hit by water cannon (constantly decreases by a cooldown rate)
	//mjc float m_fHitByWaterCannonImmuneTimer;	// Total time of water cannon immunity left

	//mjc float m_fTimeToNextCreateInteriorShockingEvents;

	//mjc float m_fTimeBeenSprinting;
	//mjc float m_fSprintStaminaUpdatePeriod;
	//mjc float m_fSprintStaminaDurationMultiplier;
	
	//mjc float m_fTimeBeenSwimming;
	//mjc float m_fSwimLungCapacityUpdatePeriod;

	// External DOF
	//mjc crFrame* m_pExternallyDrivenDOFFrame;

	// Targeting object
	//mjc-moved CPlayerPedTargeting m_targeting;

#if __ASSERT
    //mjc static bool m_ChangingPlayerModel;
#endif // __ASSERT
};

#endif
