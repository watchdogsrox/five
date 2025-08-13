// 
// audio/weaponaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_WEAPONAUDIOENTITY_H
#define AUD_WEAPONAUDIOENTITY_H

#include "audio_channel.h"
#include "gameobjects.h"
#include "superconductor.h"
#include "waveslotmanager.h"

#include "audioengine/tracker.h"

#include "audioentity.h"

#include "audioengine/tracker.h"
#include "audioengine/soundfactory.h"

#include "scene/RegdRefTypes.h"

#include "atl/array.h"

#include "Objects/object.h"

#include "system/criticalsection.h"

const u8 g_MaxWeaponFireEventsPerFrame = 20;
const u32 g_AudMaxPlayerWeaponSlots = 3;

#define WEAPON_AUDENTITY_NUM_ANIM_EVENTS 32

class CEntity;
class CObject;
namespace rage
{
	class bkBank;
	class audSound;
}

class audWeaponAudioEntity;
class audWeaponAudioComponent;

struct audProjectileData
{
	RegdObj object;
	audSound ** soundPtr;
	u32 soundKeyHash;
	u32 soundSetHash;
	bool isUnderwater;
};

const u32 g_MaxAudProjectileEvents = 64;
const u32 g_MaxAudWeaponSpinEvents = 32;
const u32 g_MaxAudCookProjectileEvents = 32;

struct audPlayerWeaponSlot
{
	audPlayerWeaponSlot()
	{ Init();}

	void Init()
	{
		waveSlot = NULL;
		contextHash = 0;
		audioHash = 0;
		lastUsedTime = 0; 
	}

	audDynamicWaveSlot * waveSlot;
	u32 contextHash;
	u32 audioHash;
	u32 lastUsedTime;

};

struct audWeaponInteriorMetrics  
{
	audWeaponInteriorMetrics()
		:Lpf(0)
		,Wetness(0.f)
		,OutsideVisability(0.f)
		,Predelay(0)
		,Hold(0.f)
		,LastTimeForCalc(0)
	{
	}

	u32 Lpf;
	f32 Wetness;
	f32 OutsideVisability;
	u32 Predelay;
	f32 Hold;
	u32 LastTimeForCalc;
};

struct audWeaponFireInfo
{
	struct PresuckInfo
	{
		u32 presuckSound;
		u32 presuckScene;
		s32 presuckTime;

		PresuckInfo() :
			  presuckSound(g_NullSoundHash)
			, presuckScene(0u)
			, presuckTime(0u)
		{
		}
	};

	audWeaponFireInfo()
		:parentEntity(NULL)
		,isSuppressed(false)
		,forceSingleShot(false)
		,lastPlayerHitTime(0)
		,settingsHash(0)
		,isKillShot(false)
		,ammo(-1)
		,autoFireSound(0u)
	{
		explicitPosition.Zero();
	}

	void Init(const audWeaponFireInfo &info)
	{
		explicitPosition = info.explicitPosition;
		audioSettingsRef = info.audioSettingsRef;
		parentEntity = info.parentEntity;
		isSuppressed = info.isSuppressed;
		lastPlayerHitTime = info.lastPlayerHitTime;
		forceSingleShot = info.forceSingleShot;
		hitPlayerSmoother.Reset();
		settingsHash = info.settingsHash;
		isKillShot = info.isKillShot;
		ammo = info.ammo;
		autoFireSound = info.autoFireSound;
	}

	u32 GetWeaponFireSound(const WeaponSettings * settings, const bool isLocalPlayer, PresuckInfo* presuckInfo = NULL, bool isFinalKillSHot = false) const;
	u32 GetWeaponEchoSound(const WeaponSettings * settings) const;
	const WeaponSettings * FindWeaponSettings(bool warnOnBackup = true);
	void PopulatePresuckInfo(const WeaponSettings *settings, audWeaponFireInfo::PresuckInfo &presuckInfo);

	Vector3 explicitPosition;
	audSimpleSmoother hitPlayerSmoother;
	audMetadataRef audioSettingsRef;
	RegdEnt parentEntity;
	u32 lastPlayerHitTime;
	u32 settingsHash;
	u32 autoFireSound;
	bool isSuppressed;
	bool forceSingleShot;
	bool isKillShot;
	s32 ammo; // ammo in players clip, used for replay
};

struct audWeaponFireEvent
{
	audWeaponFireEvent() 
	{
		weapon = NULL;
	}

	audWeaponFireInfo fireInfo;
	RegdWeapon weapon;
};

struct audWeaponAutoFireEvent
{
	audWeaponAutoFireEvent() 
	{
		m_AutoFireTimeOut = ~0U;
		m_AutoFireLoop = NULL;
		m_InteriorAutoLoop = NULL;
		m_PulseHeadsetSound = NULL;
	}

	audWeaponFireInfo fireInfo;
	u32 m_AutoFireTimeOut;
	audSound ** m_AutoFireLoop;
	audSound ** m_InteriorAutoLoop;
	audSound ** m_PulseHeadsetSound;
};

struct audPlayerWeaponLoadEvent
{
	audPlayerWeaponLoadEvent()
	{
		waitingForLoad = false;
		weaponHash = 0;
		audioHash = 0;
	}

	void HandlePlayerWeaponLoad();
	void AddPlayerWeaponLoadEvent(u32 inWeaponHash, u32 inAudioHash);

	u32 weaponHash;
	u32 audioHash;
	bool waitingForLoad;
};

class audWeaponAudioComponent 
{
	friend class audWeaponAudioEntity;
public:
	audWeaponAudioComponent();
	~audWeaponAudioComponent();

	void Init(u32 audioHash, CWeapon * weapon, bool isScriptWeapon = false);
	void Init()
	{
		naAssertf(0, "audWeaponAudioComponent must be initialised with a u32 audioHash");
	}

	WeaponSettings *GetWeaponSettings(const CEntity * parentEntity = NULL) const;

	void PlayWeaponFire(CEntity *parentEntity = NULL, bool isSilenced = false, bool forceSingleShot = false, bool isKillShot = false);


	u32 GetSettingsHash() const { return m_SettingsHash; }

	void SpinUpWeapon(const CEntity * parentEntity, bool forceNPCVersion, s32 boneIndex = -1);
	void SpinDownWeapon();

	void UpdateSpin();

	void CookProjectile(const CEntity * parentEntity);
	void StopCookProjectile();
	void PlayCookProjectileSound(const CEntity * entity);

	void TriggerEmptyShot(CPed * ped);
	void TriggerEmptyShot(CVehicle* parent, const CVehicleWeapon* vehicleWeapon);

	void RemoveFromAutoFiringList();

	void SetAnimIsAutoFiring(bool isFiring) { m_AnimIsAutoFiring = isFiring; }
	bool GetAnimIsAutoFiring() { return m_AnimIsAutoFiring; }

	void SetLastImpactTime();

	void HandleHitPlayer();

	u32 GetLastPlayerHitTime()
	{
		return m_LastHitPlayerTime;
	}

	u32 GetLastImpactTime()
	{
		return m_LastImpactTime;
	}

	void SetWasDropped(bool wasDropped)
	{
		m_WasDropped = wasDropped;
	}

	bool GetWasDropped() const { return m_WasDropped; }



	u32 GetLastAnimTime() { return m_LastAnimTime; }
	void SetLastAnimTime(u32 time) { m_LastAnimTime = time; }
	u32 GetLastAnimHash() { return m_LastAnimHash; }
	void SetLastAnimHash(u32 hash) { m_LastAnimHash = hash; }

private:

	RegdWeapon m_Weapon;
	RegdConstEnt m_SpinOwner;
	audMetadataRef m_ItemSettings;
	audSound * m_AutoFireLoop;
	audSound * m_InteriorAutoLoop;
	audSound * m_PulseHeadsetSound;
	audSound * m_SpinSound;

	u32 m_SettingsHash;
	u32 m_FrameLastEmptyShotFired;
	u32 m_TimeLastEmptyShotFired;
	u32 m_LastImpactTime;
	u32 m_LastHitPlayerTime;
	u32 m_LastAnimTime;
	u32 m_LastAnimHash;
	s32 m_SpinBoneIndex;
	s32 m_SpecialAbilityMode;
	bool m_ForceNPCVersion;

	bool m_IsScriptWeapon;
	bool m_AnimIsAutoFiring;
	bool m_WasDropped;
};

struct audWeaponSpinEvent
{
	audWeaponSpinEvent()
	{
		isActive = false;
	}
	void HandleWeaponSpin();

	RegdConstEnt entity;
	RegdWeapon weapon;
	s32 boneIndex;
	bool isActive;
};

struct audProjectileCookEvent
{
	audProjectileCookEvent()
	{
		isActive = false;
	}
	void HandleCookProjectile();

	RegdConstEnt entity;
	bool isActive;
};

struct weaponAnimEvent
{
	u32 hash;
	RegdObj weapon;// Store as the object that contains the CWeapon* in orther to get the trakcer. 
};

struct waterCannonHitInfo
{
	audSmoother smoother;
	audSound*	sound;
	f32			volume;
	bool		hitByWaterCannon;
};



class audWeaponAudioEntity : public naAudioEntity
{
	friend class audWeaponAudioComponent;

public:
	AUDIO_ENTITY_NAME(audWeaponAudioEntity);
	audWeaponAudioEntity();

	void Init();

	void PlayBulletBy(u32 weaponAudioHash, const Vector3 &vecStart, const Vector3 &vecEnd, const s32 predelay = -1);
	void PlayExplosionBulletBy(const Vector3 &vecStart);

	// Conductors
	void SetEmphasizeFactor(f32 factor);

	void PlayWeaponFireEvent(audWeaponFireEvent *fireEvent);
	void HandleFire(audWeaponFireInfo &fireInfo, const eWeaponEffectGroup effectGroup);
	void StartAutomaticFire(audWeaponFireInfo &fireInfo, audSound ** autoFireSound, audSound ** interiorAutoSound, const eWeaponEffectGroup effectGroup, audSound ** pulseHeadsetSound);
	void StopAutomaticFire(audWeaponFireInfo & fireInfo, audSound ** autoFireLoop, audSound ** interiorAutoLoop, audSound ** pulseHeadsetSound);

	void ProcessCachedWeaponFireEvents(void);
	void PlayTwoEchos(u32 echoSound, u32 existingPredelay, Vector3& sourcePosition, f32 existingAttenuation, u32 echoDepth, u32 timeAutoSoundPlayed = 0, EnvironmentRule* envRule = NULL);
	void PlayWeaponReport(audWeaponFireInfo * fireInfo, const audWeaponInteriorMetrics * interior, const WeaponSettings * weaponSettings, float fDist, bool dontPlayEcho = false);
	void PlayWaterCannonHit(Vec3V_In hitPosition,const CEntity *hitEntity,phMaterialMgr::Id matId,u16 component);
	void PlayWaterCannonHitVehicle(CVehicle *vehicle, Vec3V_In hitPosition);
	void PlayWaterCannonHitPed(CPed *ped, Vec3V_In hitPosition);
	void CleanWaterCannonMapFlags();
	void CleanWaterCannonFlags();
	void GetReportMetrics(const audWeaponFireInfo * fireInfo, const WeaponSettings * settings, Vector3* vPos, f32* fVolume, s32* sPredelay, u32* bestIndex, u32* secondBestIndex);
	void SetPlayerDoubleActionWeaponPhase(const CWeapon* weapon, f32 prevPhase, f32 currentPhase);
	void TriggerWeaponReloadConfirmation(Vector3& position, const CVehicle* vehicle);
	void TriggerNotReadyToFireSound(Vector3& position, const CVehicle* vehicle);
	void TriggerVehicleWeaponSound(Vector3& position, const CVehicle* vehicle, const u32 soundNameHash REPLAY_ONLY(, bool replayRecord = true));

	static bool IsRayGun(u32 weaponAudioHash);
	static bool IsRubberBulletGun(u32 weaponAudioHash);
	static bool IsRiotShotgun(u32 weaponAudioHash);

	static void AddWeaponAnimEvent(const audAnimEvent &event,CObject *weapon);
	static void SetReportGoodness(u8 index, f32 echoSuitability);

	virtual void ProcessAnimEvents();
	void ProcessWeaponAnimEvents();
	virtual void PreUpdateService(u32 timeInMs);
	virtual void PostUpdate();

	static audMetadataRef FindSound(char * name) {return FindSound(atStringHash(name));}
	static audMetadataRef FindSound(u32 name) { return sm_CodeSounds.Find(name);}
	
#if __DEV
	void DebugDraw();
	//Check that none of our sounds are using banks that we won't load in
	void ValidateWeaponSettings(WeaponSettings * settings);
#endif

	
	void AttachProjectileLoop(const char * soundName, CObject *obj, audSound **sound, bool underwater = false);
	void AttachProjectileLoop(const u32 soundHash, CObject *obj, audSound **sound, const bool underwater);


	static void GetInteriorMetrics(audWeaponInteriorMetrics & interior, CEntity * parent);

#if USE_CONDUCTORS
	f32 GetIntensityFactor()
	{
		return m_IntensityFactor;
	}
#endif

	void CookProjectile(const CEntity * parentEntity);
	void SpinUpWeapon(const CEntity * parentEntity, CWeapon * weapon, s32 boneIndex);
	static void PlaySpinUpSound(const CEntity * parentEntity, audSound ** spinSound, bool forceNPCVersion, bool skipSpinUp, s32 boneIndex);
	void TriggerStickyBombAttatch(Vec3V_In position, CEntity* stuckEntity, u32 stuckComponent, phMaterialMgr::Id matId, float impulse, CEntity * owner, audMetadataRef stickSoundRef);
	void TriggerFlashlight(const CWeapon* weapon,bool active);
	void ToggleThermalVision(bool turnOn, bool forcePlay = false);
	void UpdateThermalVision();



	static const audWeaponFireEvent &GetLastWeaponFireEventPlayed()
	{
		return sm_LastWeaponFireEventPlayed;
	}
	static const audWeaponFireEvent &GetLastWeaponFireEventPlayedAgainstPlayer()
	{
		return sm_LastWeaponFireEventPlayedAgainstPlayer;
	}

	static f32 PlayerGunFired();

	static f32 GetGunfireFactor()
	{
		return sm_GunfireFactor;
	}

	static u32 GetLastTimePlayerGunFired()
	{
		return sm_LastTimePlayerGunFired;
	}
	static u32 GetLastTimeSomeoneNearbyFiredAGun()
	{
		return sm_LastTimeSomeoneNearbyFiredAGun;
	}
	static u32 GetLastTimeEnemyNearbyFiredAGun()
	{
		return sm_LastTimeEnemyNearbyFiredAGun;
	}
	static u32 GetLastTimePlayerNotFiredGunForAWhile()
	{
		return sm_LastTimePlayerNotFiredGunForAWhile;
	}

	void AddToAutoFiringList(const audWeaponFireInfo * fireInfo, audSound ** autoFireSound, audSound ** interiorAutoSound, audSound ** pulseHeadsetSound);

	static bool IsWaitingOnEquippedWeaponLoad();

	bool AddScriptWeapon(u32 itemHash);
	void ReleaseScriptWeapon();
	bool HandlePlayerEquippedWeapon(u32 weaponHash, u32 audioHash, bool isEquippingNow = false);
	void AddPlayerEquippedWeapon(u32 weaponHash, u32 audioHash);
	void AddPlayerPreviousWeapon(u32 weaponHash, u32 audioHash);
	void AddPlayerInventoryWeapon();
	void AddNextInventoryWeapon(CPed* player, u32 weaponHash, u32 audioHash);
	bool LoadDynamicWeaponAudio(audPlayerWeaponSlot * weaponSlot, u32 audioHash);
	void RemovePlayerInventoryWeapon(u32 weaponHash);
	void RemoveAllPlayerInventoryWeapons();
	void AddAllInventoryWeaponsOnPed(CPed * ped);
	u32 GetNextWeapon(CPed * ped, u32 * audioHash = NULL);
	void ProcessEquippedWeapon();
	void ProcessNextWeapon();
	void SmoothWaterCannonOnMap(f32 rate,f32 desireLinVol,MaterialType matType);
	void SmoothWaterCannonOnVeh(f32 rate,f32 desireLinVol);
	void SmoothWaterCannonOnPed(f32 rate,f32 desireLinVol);

	void HandleAnimFireEvent(u32 hash, CWeapon *weapon, CPed *ped);
	bool SuppressorIsVisible(const CWeapon *weapon);
	bool EquippedWeaponIsVisible(const CPed *ped);

	static bool ShouldUseBackupWeapons() { return sm_UseBackupWeapons; }

	u32 GetPlayerBankIdFromWeaponSettings(WeaponSettings * settings);


	u32 GetUnderWaterFireSound() { return sm_CodeSounds.Find(ATSTRINGHASH("UNDERWATER_WEAPON_FIRE", 0xE9FB6E48)).Get();}
	
	static WeaponSettings * GetWeaponSettings(u32 audioHash, u32 contextHash = NULL_HASH, bool forceContext = false);
	static WeaponSettings * GetWeaponSettings(audMetadataRef audioRef, u32 contextHash = NULL_HASH);

	static void SetSkipMinigunSpinUp(bool skip)
	{
		sm_SkipMinigunSpinUp = skip;
	}

	static void SetBlockSpinUpFrame(u32 time) { sm_BlockWeaponSpinFrame = time; }

	static audWeaponFireEvent sm_WeaponFireEventList[g_MaxWeaponFireEventsPerFrame];
	static audWeaponAutoFireEvent sm_AutoWeaponsFiringList[g_MaxWeaponFireEventsPerFrame];
	static u8 sm_NumWeaponFireEventsThisFrame;
	static u32 sm_WeaponAnimTimeFilter;

#if __BANK
	static void AddWidgets(bkBank &bank);
#endif // __BANK

	u32 GetEquippedWeaponAudioHash() { return sm_PlayerEquippedWeaponSlot.audioHash; }

private:
	u32 GetCustomEchoSoundForWeapon(audSoundSet echoSet, eWeaponWheelSlot wheelSlot);
	void TriggerDeferredProjectileEvents();
	f32 GetWeaponResonanceImpulse(eWeaponEffectGroup effectGroup);


	audPlayerWeaponLoadEvent m_PlayerWeaponLoad;

	//u32 m_WeaponHash;
	u32 m_NextBulletByTimeMs;
	
	f32 m_WaterCannonPedHitDistSqd;

	f32 m_IntensityFactor;

	u32 m_LastDoubleActionWeaponSoundTime;

	sysCriticalSectionToken m_ProjectileLock;


	static waterCannonHitInfo sm_WaterCannonOnPedsInfo;
	static waterCannonHitInfo sm_WaterCannonOnVehInfo;

	static audSoundSet sm_WaterCannonSounds;
	static audSoundSet sm_BulletByUnderwaterSounds;
	static audSoundSet sm_BulletByGeneralSounds;

	static bool sm_Smooth;
	static f32 sm_TimeToMuteWaterCannon;
	//Eventually, these will live in the environment code (actually...scan positions already do, but I'd like to keep this all close
	//	together for now)
	static atRangeArray<waterCannonHitInfo, NUM_MATERIALTYPE> sm_WaterCannonHitMapInfo;
	static atRangeArray<f32, 8> sm_ReportGoodness;
	static atRangeArray<Vector3, 24> sm_RelativeScanPositions;

	static atRangeArray<audWeaponSpinEvent, g_MaxAudWeaponSpinEvents> sm_WeaponSpinEvents;

	static atRangeArray<audProjectileCookEvent, g_MaxAudCookProjectileEvents> sm_ProjectileCookEvents;

	static atFixedArray<audProjectileData, g_MaxAudProjectileEvents> sm_ProjectileEvents;

	static audWeaponFireEvent sm_LastWeaponFireEventPlayed;
	static audWeaponFireEvent sm_LastWeaponFireEventPlayedAgainstPlayer;
	static bool sm_bIsReportStuffInited;
	static audCurve sm_ReportGoodnessToAttenuationCurve;
	static audCurve sm_ReportGoodnessToPredelayCurve;
	
	static audCurve sm_InteriorWetShotVolumeCurve;
	static audCurve sm_InteriorDryShotVolumeCurve;
	static audCurve sm_OutsideVisibilityReportVolumeCurve;
	static audCurve sm_StickyBombImpulseVolumeCurve;

	static atRangeArray<weaponAnimEvent, WEAPON_AUDENTITY_NUM_ANIM_EVENTS> sm_WeaponAnimEvents;

	static audCurve sm_ConductorEmphasizeFactorToVol;

	static audWaveSlotManager sm_PlayerWeaponSlotManager;
	static audPlayerWeaponSlot sm_PlayerEquippedWeaponSlot;
	static audPlayerWeaponSlot sm_ScriptWeaponSlot;
	static audPlayerWeaponSlot sm_PlayerNextWeaponSlot;

	static u32 sm_EmptyShotFilter;

	static bool sm_PlayerGunDampingOn;
	static u32 sm_LastTimePlayerGunFired;
	static u32 sm_LastTimePlayerNotFiredGunForAWhile; // this is the last time the player had gone g_NotFiredForAWhileTime without shooting.
	static f32 sm_PlayerGunDuckingAttenuation;

	static audSound* sm_ThermalGoggleSound;
	static audSound* sm_ThermalGoggleOffSound;

	static bool sm_ThermalGogglesOn;
	static u32 sm_PlayThermalGogglesSound;  // 0 = nosound, soundhash = play the sound


	static u32 sm_LastTimeSomeoneNearbyFiredAGun;
	static u32 sm_LastTimeEnemyNearbyFiredAGun;
	static f32 sm_GunfireFactor;

	static f32 sm_InteriorSilencedWeaponWetness;


	static bool sm_IsWaitingOnEquippedWeaponLoad;
	static bool sm_UseBackupWeapons;
	static bool sm_IsWaitingOnNextWeaponLoad;
	static bool sm_WantsToLoadPlayerEquippedWeapon;

	static audSoundSet sm_CodeSounds;

	static bool sm_UsingPlayerDistanceCam;

	static f32 sm_NpcReportIntensity;
	static f32 sm_NpcReportIntensityDecreaseRate;
	static f32 sm_NpcReportCullingDistanceSq;
	static f32 sm_IntensityForNpcReportDistanceCulling;
	static f32 sm_IntensityForNpcReportFullCulling;

	static u32 sm_PlayerHitBoostHoldTime;
	static f32 sm_PlayerhitBoostSmoothTime;
	static float sm_PlayerHitBoostVol;
	static float sm_PlayerHitBoostRolloff;

	static u32 sm_AutoEmptyShotTimeFilter;

	static bool sm_SkipMinigunSpinUp;

	static u32 sm_BlockWeaponSpinFrame;
};

extern audWeaponAudioEntity g_WeaponAudioEntity;
#endif // AUD_WEAPONAUDIOENTITY_H
