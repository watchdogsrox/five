#ifndef GUN_FLAGS_H
#define GUN_FLAGS_H

enum GunFlags
{
	// The task ignores any duration parameter
	GF_InfiniteDuration								= BIT0,

	// Forces the ped to always aim even when the aim button isn't held
	GF_AlwaysAiming									= BIT1,

	// Prevents the ped from aiming, if not firing the task terminates
	GF_DisableAiming								= BIT2,

	// Skip the idle to aim/fire clip at the start
	// Skip the aim/fire to idle clip at the end
	GF_SkipIdleTransitions							= BIT3,

	// Act as if blocked, for AI peds
	GF_ForceBlockedClips							= BIT4,

	// Disabled the aim/fire direction blocked clip
	GF_DisableBlockingClip							= BIT5,

	// Disable bullet reactions
	GF_DisableBulletReactions						= BIT6,

	// Don't aim or fire until the ped is facing toward the target
	GF_WaitForFacingAngleToFire						= BIT7,

	// Fire bullets along the gun barrel ignoring any target
	GF_FireBulletsInGunDirection					= BIT8,

	// Forces the task to fire at least once
	GF_FireAtLeastOnce								= BIT9,

	// This task is firing using the blind fire clips
	GF_BlindFiring									= BIT10,

	// When the Ik limits are reached, it fires at the limits
	GF_FireAtIkLimitsWhenReached					= BIT11,

	// Allow aiming with no ammo
	GF_AllowAimingWithNoAmmo						= BIT12,

	// Only exit the firing loop clip at the end
	GF_OnlyExitFireLoopAtEnd						= BIT13,

	// Can fire while still blending in
	// CS - PERHAPS NOT NEEDED?
	GF_AllowFiringWhileBlending						= BIT14,

	// Stops the gun task from auto switching weapons, terminating instead
	GF_PreventAutoWeaponSwitching					= BIT15,

	// Stops the gun task from swapping weapons
	GF_PreventWeaponSwapping						= BIT16,

	// Update the target
	GF_UpdateTarget									= BIT17,

	// Orient the lower body toward the target when aiming/firing
	GF_DontUpdateHeading							= BIT18,

	// Disable reloading sub task
	GF_DisableReload								= BIT19,

	// Turns of the automatic torso aiming IK
	GF_DisableTorsoIk								= BIT20,

    // never use the camera for aiming at a target, even for players
    GF_NeverUseCameraForAiming                      = BIT21,

	// Forces the gun task to reset the firing pattern to defaults
	GF_ForceFiringPatternReset						= BIT22,

	// Enters aim state even if player isn't aiming
	GF_ForceAimState								= BIT23,

	// Let the gun task know we are reloading from idle
	GF_ReloadFromIdle								= BIT24,

	// Skip any transitions and force the state to aim
	GF_InstantBlendToAim							= BIT25,

	GF_SkipOutro									= BIT26,

	GF_LowCover										= BIT27,

	GF_LeftCover									= BIT28,

	GF_CornerCover									= BIT29,

	// used to tell CTaskGun to play a bullet reaction if possible
	GF_PlayBulletReaction							= BIT30,

	GF_ForceAimOnce									= BIT31,
};

enum GunFireFlags {

	// used when we need to ensure we don't fire from camera
	GFF_FireFromMuzzle								= BIT(0),

	// use this to force enabling of damage to own vehicle
	GFF_EnableDamageToOwnVehicle					= BIT(1),

	// use this to fire from the gun muzzle in the gun direction
	GFF_EnableDumbFire								= BIT(2),

	// use this to fire from the camera if possible (must be a player)
	GFF_ForceFireFromCamera							= BIT(3),

	// use to smash glass that the weapon intersects coming out of weapon blocked state.
	GFF_SmashGlassFromBlockingState					= BIT(4),

	// Disabled being able to fire until fire button is released
	GFF_RequireFireReleaseBeforeFiring				= BIT(5),

	// Used to disable clearing of GFF_RequireFireReleaseBeforeFiring, instead of having a new flag.
	GF_NotAllowToClearFireReleaseFlag				= BIT(6),

	// use this to force enabling of damage to own vehicle
	GFF_PassThroughOwnVehicleBulletProofGlass		= BIT(7)
};

#endif // GUN_FLAGS_H
