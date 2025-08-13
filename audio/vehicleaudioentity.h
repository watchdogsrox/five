//  
// audio/vehicleaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLEAUDIOENTITY_H
#define AUD_VEHICLEAUDIOENTITY_H

#include "audiodefines.h"
#include "radioemitter.h"
#include "audio_channel.h"
#include "waveslotmanager.h"
#include "renderer/HierarchyIds.h"
#include "fwsys/timer.h"
#include "vehiclecollisionaudio.h"
#include "vehiclefireaudio.h"
#include "audioengine/soundset.h"
#include "vehiclegadgetaudio.h"
#include "audioengine/environment_game.h"
#include "audioengine/engine.h"
#include "audioentity.h"
#include "environment/environment.h"
#include "Peds/PedFootstepHelper.h"
#include "system/poolallocator.h"

#define StaticConditionalWarning(x, msg) if(!x) { naWarningf("%s", msg); }
#define ConditionalWarning(x, msg) if(!x) { naWarningf("%s: %s", GetVehicleModelName(), msg); }
#define ConditionalWarningf(x, msg,...) if(!x) { naWarningf(msg, ##__VA_ARGS__); }

const u32 g_audMaxEngineSlots = 25;

struct VfxWaterSampleData_s;

class CVehicle;
class CTrailer;
class audRadioStation;
class audVehicleEngine;
class audVehicleKersSystem;
class audScene;
class audTrailerAudioEntity;

typedef enum
{
	AUD_VEHICLE_TRAIN,
	AUD_VEHICLE_PLANE,
	AUD_VEHICLE_HELI,
	AUD_VEHICLE_CAR,
	AUD_VEHICLE_BOAT,
	AUD_VEHICLE_BICYCLE,
	AUD_VEHICLE_TRAILER,
	AUD_VEHICLE_ANY,
	AUD_VEHICLE_NONE,
}audVehicleType;

typedef enum
{
	// (Definitions in car-terms, may be slightly different for other vehicle types)
	AUD_VEHICLE_LOD_REAL = 0, // Full engine, full road noise. Generally nearby vehicles with engines on.
	AUD_VEHICLE_LOD_DUMMY, // No engine (NPC road noise active instead), reduced road noise. Generally distant vehicles with engines on.
	AUD_VEHICLE_LOD_SUPER_DUMMY, // Doesn't actively play anything (except rain effects), but won't block one-shot effects (eg. tyre punctures) from playing. Generally vehicles that couldn't get a dummy slot, or else neaby ones with engines off.
	AUD_VEHICLE_LOD_DISABLED, // Won't play anything at all. Generally for distant vehicles with engines off, which we just don't care about.
	AUD_VEHICLE_LOD_UNKNOWN,
}audVehicleLOD;

typedef enum
{
	AUD_SIREN_OFF,
	AUD_SIREN_SLOW,
	AUD_SIREN_FAST,
	AUD_SIREN_WARNING,
}audSirenState;

typedef enum
{
	AUD_CAR_ALARM_TYPE_HORN = 0,
	AUD_CAR_ALARM_TYPE_1,
	AUD_CAR_ALARM_TYPE_2,
	AUD_CAR_ALARM_TYPE_3,
	AUD_CAR_ALARM_TYPE_4,
	AUD_NUM_CAR_ALARM_TYPES,
}audCarAlarmType;

typedef enum
{
	// NB. New values must go at the end of this list as we save these values in replay packets!
	AUD_VEHICLE_SOUND_UNKNOWN,
	AUD_VEHICLE_SOUND_WHEEL0, 
	AUD_VEHICLE_SOUND_WHEEL1,
	AUD_VEHICLE_SOUND_WHEEL2,
	AUD_VEHICLE_SOUND_WHEEL3,
	AUD_MAX_WHEEL_WATER_SOUNDS = AUD_VEHICLE_SOUND_WHEEL3,
	AUD_VEHICLE_SOUND_WHEEL4,
	AUD_VEHICLE_SOUND_WHEEL5,
	AUD_VEHICLE_SOUND_INTERIOR_OCCLUSION, // specifies sound should be occluded as if coming from vehicle interior
	AUD_VEHICLE_SOUND_ENGINE,
	AUD_VEHICLE_SOUND_EXHAUST,
	AUD_VEHICLE_SOUND_HEAVY_CLATTER,
	AUD_VEHICLE_SOUND_HORN,
	AUD_VEHICLE_SOUND_SCRAPE,
	AUD_VEHICLE_SOUND_ROLL,
	AUD_VEHICLE_SOUND_CAR_RECORDING,
	AUD_VEHICLE_SOUND_FOLIAGE,
	AUD_VEHICLE_SOUND_LIGHTCOVER,
	AUD_VEHICLE_SOUND_SPOILER_STRUTS,
	AUD_VEHICLE_SOUND_SPOILER,
	AUD_VEHICLE_SOUND_AIRBRAKE_L,
	AUD_VEHICLE_SOUND_AIRBRAKE_R,
	AUD_VEHICLE_SOUND_SUBMARINECAR_PROPELLOR,
	AUD_VEHICLE_SOUND_SUBMARINE_PROPELLOR,
	AUD_VEHICLE_SOUND_CUSTOM_BONE,
	AUD_VEHICLE_SOUND_WHEEL6,
	AUD_VEHICLE_SOUND_WHEEL7,
	AUD_VEHICLE_SOUND_WHEEL8,
	AUD_VEHICLE_SOUND_WHEEL9,
	AUD_VEHICLE_SOUND_WHEEL10,
	AUD_VEHICLE_SOUND_BLADE_0,
	AUD_VEHICLE_SOUND_BLADE_1,
	AUD_VEHICLE_SOUND_BLADE_2,
	AUD_VEHICLE_SOUND_BLADE_3,
}audVehicleSounds;

typedef enum
{
	AUD_VEHICLE_SCRIPT_PRIORITY_NONE,
	AUD_VEHICLE_SCRIPT_PRIORITY_MEDIUM,
	AUD_VEHICLE_SCRIPT_PRIORITY_HIGH,
	AUD_VEHICLE_SCRIPT_PRIORITY_MAX,
}audVehicleScriptPriority;


// ----------------------------------------------------------------
// Per-frame cache of wheel positions. Calculating the current wheel positions requires some matrix transformations, so avoid repeatedly
// re-calculating a position if another function has already requested the position this frame
// ----------------------------------------------------------------
struct audVehicleWheelPositionCache
{
	audVehicleWheelPositionCache()
	{
		sysMemSet((void*)cachedWheelPositionValid, 0, sizeof(bool)*NUM_VEH_CWHEELS_MAX);
	}

	Vector3 cachedWheelPositions[NUM_VEH_CWHEELS_MAX];
	bool cachedWheelPositionValid[NUM_VEH_CWHEELS_MAX];
};

// ----------------------------------------------------------------
// Helper struct for collection info about a vehicle
// ----------------------------------------------------------------
struct audVehicleVariables
{
	audVehicleWheelPositionCache* wheelPosCache;

	// Calculated for all vehicles
	u32 timeInMs;
	u32 numWheelsTouching;
	f32 numWheelsTouchingFactor;
	f32 numDriveWheelsInWaterFactor;
	f32 numDriveWheelsInShallowWaterFactor;
	f32 invDriveMaxVelocity;
	f32 fwdSpeed;
	f32 fwdSpeedAbs;
	f32 fwdSpeedRatio;
	f32 wheelSpeed;
	f32 wheelSpeedAbs;
	f32 wheelSpeedRatio;
	f32 movementSpeed;
	s32 roadNoisePitch;
	f32 wetRoadSkidModifier;
	u32 distFromListenerSq;
	u32 numNonBurstWheelsTouching;
	bool visibleBySniper;
	bool visibleInGameViewPort;

	// Currently calculated for cars only
	f32 skidLevel; // arbitrary measure of how much skid audio is active
	u32 gear;
	f32 rawRevs;
	f32 gasPedal;
	f32 brakePedal;
	f32 pitchRatio;
	f32 exhaustConeAtten;
	f32 engineConeAtten;
	f32 engineThrottleAtten;
	f32 exhaustThrottleAtten;
	f32 carVolOffset;
	f32 environmentalLoudness;
	f32 speedForBrakes;
	f32 enginePowerFactor;
	f32 clutchRatio;
	f32 enginePostSubmixVol;
	f32 exhaustPostSubmixVol;
	u32 playerVolCategory;
	f32 throttle;
	f32 throttleInput;
	f32 revs;
	f32 rollOffScale;
	f32 engineDamageFactor;
	f32 forcedThrottle;
	s32 granularEnginePitchOffset;
	bool onRevLimiter;
	bool isInFirstPersonCam;
	bool isInHoodMountedCam;
	bool hasMaterialChanged;
};

#if __BANK
struct audioRecordingEvent
{
	f32 time;
	u32 hashEvent;
	bool processed;
};
struct audioCarRecordingInfo
{
	atArray<audioRecordingEvent> eventList;
	atHashString nameHash;
	CarRecordingAudioSettings *settings;
	u32 modelId;
};

// ----------------------------------------------------------------
// Debug class to help us simulate a plane/heli flying past
// ----------------------------------------------------------------
class audDebugTracker : public audPlaceableTracker
{
public:
	virtual const Vector3 GetPosition() const { return m_Position; }
	void SetPosition(Vector3::Param position) { m_Position = position; }

protected:
	Vector3 m_Position;
};

#endif 
// ----------------------------------------------------------------
// Base class for handling generic vehicle behaviour and functionality
// (sirens, horns, doors, radio etc.)
// ----------------------------------------------------------------
class audVehicleAudioEntity : public naAudioEntity
{
#if GTA_REPLAY
	friend class CPacketVehicleUpdate;
	friend class CPacketSoundUpdateAndPlay;
#endif

// Public methods:
public:
	REGISTER_POOL_ALLOCATOR(audVehicleAudioEntity, false);

	AUDIO_ENTITY_NAME(audVehicleAudioEntity);

	audVehicleAudioEntity();
	virtual ~audVehicleAudioEntity
		();  

	virtual void Init(CVehicle *vehicle);
	virtual void Reset();
	virtual void ProcessAnimEvents();
	virtual Vec3V_Out GetPosition() const;
	virtual audCompressedQuat GetOrientation() const;
	virtual CEntity* GetOwningEntity(){return (CEntity*)m_Vehicle;};
	virtual bool HasAlarm(){return false;};
	virtual void PreUpdateService(u32 timeInMs);
	virtual void PostUpdate();
	virtual void Shutdown();

#if GTA_REPLAY
	void SetIsInWater(bool val) { m_ReplayIsInWater = val; }
	void SetVehiclePhysicsInWater(bool val) { m_ReplayVehiclePhysicsInWater = val; }	
	void SetSubAngularVelocityMag(f32 val) { m_SubAngularVelocityMag  = val; }
	f32  GetSubAngularVelocityMag() { return m_SubAngularVelocityMag; }
#endif

	void Init() { naAssertf(0, "audVehicleAudioEntity must be initialized with a valid CVehicle ptr"); }
	u32 QuerySoundNameFromObjectAndField(const u32 *objectRefs, u32 numRefs);	
	void PlayHeldDownHorn(f32 holdTimeInSeconds,bool updateTime = false);
	void PlayVehicleHorn(f32 holdTimeInSeconds = 0.96f,bool updateTime = false,bool isScriptTriggered = false);
	void SetIsInCarModShop(bool inCarMod);
	bool IsInCarModShop() const {return m_InCarModShop;}
	void SetOnStairs() { m_OnStairs = true; }
	void SetLandingGearRetracting(bool isRetracting) { if(m_IsLandingGearRetracting != isRetracting) { m_IsLandingGearRetracting = isRetracting; m_ShouldPlayLandingGearSound = true; } }
	void StopLandingGearSound();
	void SetHornType(u32 hornTypeHash){m_HornType = hornTypeHash;}
	virtual void StopHorn(bool checkForCarModShop = false, bool onlyStopIfPlayer = false);
	u32 GetHornType(){return m_HornType;}
  	bool IsHornOn();
	virtual bool DoesHornHaveTail() const { return false; }
	static audMetadataRef GetClatterSoundFromType(ClatterType clatterType);
	static audMetadataRef GetChassisStressSoundFromType(ClatterType clatterType);

	static bool PreloadScriptVehicleAudio(const u32 vehicleModelNameHash, s32 scriptID);
	static bool IsStuntRaceActive();
	static const char* GetVehicleTypeName(audVehicleType type);

	void TriggerStationarySuspensionDrop();
	void CancelStationarySuspensionDrop();
	void TriggerSpecialFlightModeTransform(bool flightModeEnabled);
	void TriggerSpecialFlightModeWingDeploy(bool deplayWings);
	void CancelSpecialFlightModeTransform();
	void TriggerBoostActivateFail();
	void TriggerEMPShutdownSound();
	void TriggerSideShunt();
	bool IsLockedToRadioStation();

	void UpdateWaterState(const VfxWaterSampleData_s* pFxSampleData);
	bool IsAnyWaterVFXActive() const;
	audVehicleFireAudio& GetVehicleFireAudio() { return(m_VehicleFireAudio); }
	const audVehicleFireAudio& GetVehicleFireAudio() const { return(m_VehicleFireAudio); }
	void OverrideHorn(bool override, const u32 overridenHornHash) {m_OverrideHorn = override; m_OverridenHornHash = overridenHornHash;} ;
	bool IsHornOverriden() { return m_OverrideHorn; }
	u32  GetOverridenHornHash() { return m_OverridenHornHash; }
	void SetVehicleIsBeingStopped() { m_IsVehicleBeingStopped = true; }
	bool ShouldPlayPlayerVehSiren() const { return m_ShouldPlayPlayerVehSiren; }
	void SmashSiren(bool force = false);
	void BlipSiren();
	void SirenStateChanged(bool on);
	void ForceUseGameObject(u32 gameObject);
	f32 ComputeEffectiveEngineHealth() const;
	f32 ComputeEffectiveBodyDamageFactor() const;
	f32 ComputeEffectiveSuspensionHealth() const;
	
	s16 GetHornSoundIdx() const			{return m_HornSoundIndex;};
	s16 GetFakeEngineHealth() const		{return m_FakeEngineHealth;};
	s16 GetFakeBodyHealth() const		{return m_FakeBodyHealth;};

	virtual bool IsInAmphibiousBoatMode() const				{ return false; }
	void SetHornSoundIdx(const s16 hornSoundIdx)			{ m_HornSoundIndex = hornSoundIdx;};
	void SetFakeEngineHealth(const s16 fakeEngineHealth)	{ m_FakeEngineHealth = fakeEngineHealth;}
	void SetFakeBodyHealth(const s16 fakeBodyHealth)		{ m_FakeBodyHealth = fakeBodyHealth;}

	void SetVehRainInWaterVolume(const f32 vol) { m_VehRainInWaterVol = vol;};

	void SetPlayerAlarmBehaviour(const bool enabled) { m_PlayerVehicleAlarmBehaviour = enabled; }

	void UseSireAsHorn(bool useSirenAsHorn) {m_UseSirenForHorn = useSirenAsHorn;} ;

	//Conductors purpose.
	void SmoothSirenVolume(f32 rate,f32 desireLinVol,bool stopSiren = false);
	void SetSirenControlledByConductors(bool controlled) {m_SirenControlledByConductor = controlled;}
	void SetMustPlaySiren(bool mustPlay) { m_MustPlaySiren = mustPlay; }

	void OnScriptVehicleDoorOpen() { m_LastScriptDoorModifyTime = fwTimer::GetTimeInMilliseconds(); }
	void OnScriptVehicleDoorShut() { m_LastScriptDoorModifyTime = fwTimer::GetTimeInMilliseconds(); }
	void NotifyWeaponBladeImpact(u32 bladeIndex, bool isPed) { m_IsWeaponBladeColliding[bladeIndex] = true; m_LastWeaponBladeCollisionIsPed[bladeIndex] |= isPed; }
	
	u32 GetSubmarineCarSoundset() const;
	u32 GetSubmarineCarTransformSoundSet() const;
	bool IsSubmarineCar() const;

	void TriggerLandingGearSound(const audSoundSet& soundset, const u32 soundFieldHash);
	void TriggerStuntRaceSpeedBoost();
	void TriggerStuntRaceSlowDown();
	void TriggerBoostInstantRecharge();
	void TriggerMissileBatteryReloadSound();
	void TriggerSubmarineCarWingFlapSound();
	void TriggerSubmarineCarRudderTurnStart();
	void RequestWeaponChargeSfx(s32 boneIndex, f32 chargeLevel);
	void OnSubmarineCarAnimationStateChanged(u32 newState, f32 phase);

	const audSound* GetSirenLoop(){return m_SirenLoop;};

	bool IsPedSatInExternalSeat(CPed* ped) const;
	bool IsSeaPlane() const;
	bool GetSirenControlledByConductors() {return m_SirenControlledByConductor;}
	bool GetMustPlaySiren() const {return m_MustPlaySiren;}
	bool IsSirenForced() const { return m_IsSirenForcedOn;}
	bool IsParkingBeepEnabled() {return m_ParkingBeepEnabled;}
	BANK_ONLY(f32 GetDesireSirenVolume() {return m_SirensDesireLinVol;})

#if __DEV 
		static void PoolFullCallback(void* pItem);
#endif // __DEV

#if __BANK
	virtual void DrawBoundingBox(const Color32 color);
#endif

	static void	HornOffHandler(u32 hash, void *context);

#if GTA_REPLAY
	audCarAlarmType	GetAlarmType() const				{	return m_AlarmType;	}
	void			SetAlarmType(audCarAlarmType type)	{	m_AlarmType = type;	}
#endif // GTA_REPLAY

	//Vehicle debris
	void UpdateBulletHits();
	const u32 GetBulletHitCount() const {return m_BulletHitCount;}
	// Virtual functions - implemented in child classes if alternative functionality is required
	inline virtual u32 GetJumpLandingSound(bool UNUSED_PARAM(interiorView)) const			{return g_NullSoundHash;}
	inline virtual u32 GetDamagedJumpLandingSound(bool UNUSED_PARAM(interiorView)) const	{return g_NullSoundHash;} 
	inline virtual u32 GetNPCRoadNoiseSound() const									{return g_NullSoundHash;}
	inline virtual u32 GetWindNoiseSound() const									{return g_NullSoundHash;}
	inline virtual u32 GetEngineSubmixSynth() const									{return g_NullSoundHash;}
	inline virtual u32 GetEngineSubmixSynthPreset() const							{return g_NullSoundHash;}
	inline virtual u32 GetExhaustSubmixSynth() const								{return g_NullSoundHash;}
	inline virtual u32 GetExhaustSubmixSynthPreset() const							{return g_NullSoundHash;}
	inline virtual u32 GetEngineSubmixVoice() const									{return g_NullSoundHash;}
	inline virtual u32 GetExhaustSubmixVoice() const								{return g_NullSoundHash;}
	inline virtual u32 GetEngineIgnitionLoopSound() const							{return g_NullSoundHash;}
	inline virtual u32 GetEngineStartupSound() const								{return g_NullSoundHash;}
	inline virtual u32 GetEngineShutdownSound() const								{return g_NullSoundHash;}
	inline virtual u32 GetVehicleShutdownSound() const								{return g_NullSoundHash;}
	inline virtual u32 GetReverseWarningSound() const								{return g_NullSoundHash;}
	inline virtual u32 GetCabinToneSound() const									{return g_NullSoundHash;}
	inline virtual u32 GetIdleHullSlapSound() const									{return g_NullSoundHash;}
	inline virtual u32 GetLeftWaterSound() const									{return g_NullSoundHash;}
	inline virtual u32 GetWaveHitSound() const										{return g_NullSoundHash;}
	inline virtual u32 GetWaveHitBigSound() const									{return g_NullSoundHash;}
	inline virtual u32 GetVehicleRainSound(bool UNUSED_PARAM(interiorView)) const	{return g_NullSoundHash;}
	inline virtual audMetadataRef GetClatterSound() const							{return g_NullSoundRef;}
	inline virtual audMetadataRef GetChassisStressSound() const						{return g_NullSoundRef;}
	inline virtual u32 GetConvertibleRoofSoundSet() const							{return 0;}
	inline virtual u32 GetConvertibleRoofInteriorSoundSet() const					{return 0;}
	inline virtual u32 GetTurretSoundSet() const									{return 0;}
	inline virtual u32 GetForkliftSoundSet() const									{return 0;}
	inline virtual u32 GetTowTruckSoundSet() const									{return 0;}
	inline virtual u32 GetDiggerSoundSet() const									{return 0;}
	inline virtual f32 GetWaveHitBigAirTime() const									{return 0.0f;}

	virtual AmbientRadioLeakage GetPositionalPlayerRadioLeakage() const { return LEAKAGE_BASSY_MEDIUM; }
	virtual f32 ComputeIgnitionHoldTime(bool UNUSED_PARAM(isStartingThisTime)) { m_IgnitionHoldTime = 0.0f; return m_IgnitionHoldTime; }
	virtual f32 GetDrowningSpeed() const { return 0.3f; }
	virtual void TriggerDoorCloseSound(eHierarchyId UNUSED_PARAM(doorIndex), const bool UNUSED_PARAM(isBroken)) {};
	virtual void TriggerDoorOpenSound(eHierarchyId UNUSED_PARAM(doorIndex)) {};
	virtual void TriggerDoorFullyOpenSound(eHierarchyId UNUSED_PARAM(doorIndex)) {};
	virtual void TriggerDoorStartOpenSound(eHierarchyId UNUSED_PARAM(doorIndex)) {};
	virtual void TriggerDoorStartCloseSound(eHierarchyId UNUSED_PARAM(doorIndex)) {};
	virtual void TriggerTrailerDetach();
	virtual void TriggerWindscreenSmashSound(Vec3V_In position);
	virtual void TriggerWindowSmashSound(Vec3V_In position);
	virtual void GetHornOffsetPos( Vector3& offset ) { offset = VEC3V_TO_VECTOR3(m_EngineOffsetPos); };
	virtual void UpdateParkingBeep(float UNUSED_PARAM(distance)) {};
	virtual void StopParkingBeepSound() {};
	virtual void SetBoostActive(bool UNUSED_PARAM(active)) {};
	virtual void OnModChanged(eVehicleModType UNUSED_PARAM(modSlot), u8 UNUSED_PARAM(newModIndex)) {};
	virtual void EngineStarted() {};
	virtual void OnFocusVehicleChanged() {}; 
	virtual void Fix();
	virtual void TriggerHydraulicBounce(bool UNUSED_PARAM(smallBounce)) {};
	virtual void UpdateSound(audSound *sound, audRequestedSettings *reqSets, u32 timeInMs);
	virtual VehicleCollisionSettings* GetVehicleCollisionSettings() const;
	virtual GranularEngineAudioSettings* GetGranularEngineAudioSettings() const { return NULL; }
	virtual audVehicleEngine* GetVehicleEngine() { return NULL; }
	virtual audVehicleKersSystem* GetVehicleKersSystem() { return NULL; }
	virtual audSimpleSmootherDiscrete* GetRevsSmoother() { return NULL; }
	virtual bool IsUsingGranularEngine() { return false; }
	virtual void VehicleExploded() {};
	virtual void BreakOffSection(int UNUSED_PARAM(section)) {};
	virtual void TriggerEngineFailedToStart() {};
	virtual f32 CalculateWaterSpeedFactor() const;

	static f32 GetFrequencySmoothingRate() { return sm_FrequencySmoothingRate; }

	void ProcessExplosion()
	{
		m_LastTimeExploded = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	}
	void ProcessVehicleExplosion()
	{
		m_LastTimeExplodedVehicle = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	}

	// PURPOSE
	//	Returns true when engine startup is playing out, which will delay radio start-up
	virtual bool IsPlayingStartupSequence() const { return false; };

	virtual RandomDamageClass GetRandomDamageClass() const		{ return RANDOM_DAMAGE_NONE; }
	virtual f32 GetExhaustProximityVolumeBoost() const			{ return 0.0f; }
	virtual f32 GetAutoShutOffTimer() const						{ return 0.0f; }
	virtual f32 GetControllerRumbleIntensity() const			{ return 0.0f; }
	virtual f32 GetEngineSwitchFadeTimeScalar() const			{ return 1.0f; }
	virtual f32 GetClatterSensitivityScalar() const				{ return 1.0f; }
	virtual f32 GetChassisStressSensitivityScalar() const		{ return 1.0f; }
	virtual f32 GetClatterVolumeBoost() const					{ return 0.0f; }
	virtual f32 GetChassisStressVolumeBoost() const				{ return 0.0f; }
	virtual bool HasAutoShutOffEngine() const					{ return false; }
	virtual bool IsDamageModel() const							{ return false; }
	virtual bool HasReverseWarning() const						{ return false; }
	virtual bool HasDoorOpenWarning() const 					{ return false; }
	virtual bool HasHeavyRoadNoise() const  					{ return false; }
	virtual bool AreTyreChirpsEnabled() const					{ return false; }
	virtual bool AreExhaustPopsEnabled() const					{ return false; }
	virtual bool IsToyCar() const								{ return false; }
	virtual bool IsGolfKart() const								{ return false; }
	virtual bool IsGoKart() const								{ return false; }
	virtual bool HasCBRadio() const								{ return false; }
	virtual bool CanCauseControllerRumble() const				{ return false; }
	virtual bool IsKickstarted() const							{ return false; }
	virtual bool IsStartupSoundPlaying() const					{ return false; }
	virtual bool IsRadioUpgraded() const						{ return false; }
	virtual audMetadataRef GetShallowWaterWheelSound() const;	
	virtual audMetadataRef GetShallowWaterEnterSound() const;

	inline bool IsReversing() const							{ return m_IsReversing; }
	inline audVehicleCollisionAudio& GetCollisionAudio()	{ return m_CollisionAudio; }
	inline f32 GetIgnitionHoldTime() const					{ return m_IgnitionHoldTime; }
	inline audSimpleSmoother* GetEngineEffectWetSmoother()	{ return &m_EngineEffectWetSmoother; }
	inline u32 GetCarCategorySoundHash() const				{ return m_VehicleCategorySoundHash; }
	inline u32 GetCarModelSoundHash() const					{ return m_VehicleModelSoundHash; }
	inline u32 GetCarMakeSoundHash() const					{ return m_VehicleMakeSoundHash; }
	inline u32 GetScannerVehicleSettingsHash() const		{ return m_ScannerVehicleSettingsHash; }
	inline u32 GetLastTimeOnGround() const					{ return m_LastTimeOnGround; }
	inline u32 GetLastTimeInAir() const						{ return m_LastTimeInAir; }
	inline Vec3V_Out GetEngineOffsetPos() const				{ return m_EngineOffsetPos; }
	inline Vec3V_Out GetExhaustOffsetPos() const			{ return m_ExhaustOffsetPos; }
	inline u32 GetTimeSinceLastRevLimiter() const			{ return m_LastRevLimiterTime; }
	inline bool HasValidEngineEffectRoute() const			{ return m_EngineSubmixIndex >= 0; }
	inline bool HasValidExhaustEffectRoute() const			{ return m_ExhaustSubmixIndex >= 0; }
	inline s32 GetEngineEffectRoute() const					{ return EFFECT_ROUTE_VEHICLE_ENGINE_MIN + m_EngineSubmixIndex; }
	inline s32 GetExhaustEffectRoute() const				{ return EFFECT_ROUTE_VEHICLE_EXHAUST_MIN + m_ExhaustSubmixIndex; }
	inline audMixerSubmix* GetEngineEffectSubmix() const	{ return g_AudioEngine.GetEnvironment().GetVehicleEngineSubmix(m_EngineSubmixIndex); }
	inline audMixerSubmix* GetExhaustEffectSubmix() const	{ return g_AudioEngine.GetEnvironment().GetVehicleExhaustSubmix(m_ExhaustSubmixIndex); }
	inline s32 GetEngineEffectSubmixIndex() const			{ return m_EngineSubmixIndex; }
	inline s32 GetExhaustEffectSubmixIndex() const			{ return m_ExhaustSubmixIndex; }	
	inline bool ShouldTriggerHydraulicBounce() const		{ return m_ShouldTriggerHydraulicBounce; }
	inline bool ShouldTriggerHydraulicActivation() const	{ return m_ShouldTriggerHydraulicActivate; }
	inline bool ShouldTriggerHydraulicDectivation() const	{ return m_ShouldTriggerHydraulicDeactivate; }

	inline bool HasWindowBeenSmashed() const		{ return m_HasSmashedWindow; }
	inline bool IsSirenOn() const					{ return m_SirenLoop != NULL; }
	inline bool IsPlayingSiren() const				{ return m_IsSirenOn; }
	inline f32 GetDrowningFactor() const			{ return m_DrowningFactor; }
	inline bool IsReal() const						{ return m_VehicleLOD == AUD_VEHICLE_LOD_REAL; }
	inline bool IsDummy() const						{ return m_VehicleLOD == AUD_VEHICLE_LOD_DUMMY; }
	inline bool IsSuperDummy() const				{ return m_VehicleLOD == AUD_VEHICLE_LOD_SUPER_DUMMY; }
	inline bool IsDisabled() const					{ return m_VehicleLOD == AUD_VEHICLE_LOD_DISABLED; }
	inline audVehicleLOD GetVehicleLOD() const		{ return m_VehicleLOD; }

	inline void SetSirenOn()					{ m_IsSirenOn = true; m_PlaySirenWithNoNetworkPlayerDriver = true; }
	inline void TriggerHeadlightSwitch()		{ m_HeadlightsSwitched = true; }
	inline void TriggerLightCoverStateChange()	{ m_HeadlightsCoverSwitched = true; }
	inline void SmashWindow()					{ m_HasSmashedWindow = true; }
	inline void FixWindows()					{ m_HasSmashedWindow = false; }

	inline void RequestHydraulicBounce()		{ m_ShouldTriggerHydraulicBounce = true; }	
	inline void RequestHydraulicDectivation()	{ m_ShouldTriggerHydraulicDeactivate = true; }

	void OnScriptSetHydraulicWheelState();
	void RequestHydraulicActivation();
	void SetLOD(audVehicleLOD lod);
	void StopAllRoofLoops();
	static u32 CalculateNumVehiclesUsingSubmixes(audVehicleType type, bool engineSubmix, bool granularOnly);

	struct audVehicleDSPSettings
	{
		audVehicleDSPSettings()
		{
			timeInMs = 0;
			proximityRatio = 0.0f;
			enginePostSubmixAttenuation = 0.0f;
			exhaustPostSubmixAttenuation = 0.0f;
			rolloffScale = 1.0f;
			environmentalLoudness = 1.f;
		}

		u32 timeInMs;
		f32 proximityRatio;
		f32 enginePostSubmixAttenuation;
		f32 exhaustPostSubmixAttenuation;
		f32 rolloffScale;
		f32 environmentalLoudness;

		enum audVehicleDSPParameterType
		{
			AUD_VEHICLE_DSP_PARAM_ENGINE,
			AUD_VEHICLE_DSP_PARAM_EXHAUST,
			AUD_VEHICLE_DSP_PARAM_BOTH,
		};

		struct DSPParameter
		{
			atHashString parameterName;
			f32 parameterValue;
			audVehicleDSPParameterType submixType;
		};

		atFixedArray<DSPParameter, 20> dspParameters;

		void AddDSPParameter(atHashString name, f32 value, audVehicleDSPParameterType type = AUD_VEHICLE_DSP_PARAM_BOTH)
		{
			DSPParameter parameter;
			parameter.parameterName = name;
			parameter.parameterValue = value;
			parameter.submixType = type;
			dspParameters.Push(parameter);
		}

		void ClearDSPParameters()
		{
			dspParameters.Reset();
		}
	};

	void InitDSPEffects();
	void UpdateDSPEffects(audVehicleDSPSettings& dspSettings, audVehicleVariables& vehicleVariables);
	void ReapplyDSPEffects();
	void FreeDSPEffects();
	bool ShouldUseDSPEffects() const;
	void AllocateVehicleVariableBlock();
	Vec3V_Out ComputeClosestPositionOnVehicleYAxis();

	void TriggerAlarm();
	void PlayVehicleAlarm();
	bool IsMonsterTruck() const;

	audTrailerAudioEntity* GetTrailer();
	f32 ComputeProximityEffectRatio();
	u32 CalculateVehicleSoundLPFCutoff();
	
	void TriggerTowingHookAttachSound();
	void TriggerTowingHookDetachSound();

	bool GetHasNormalRadio() const;
	bool GetHasEmergencyServicesRadio() const;
	bool IsAmbientRadioDisabled() const;

	f32 GetEngineUpgradeLevel() const;
	f32 GetExhaustUpgradeLevel() const;
	f32 GetTurboUpgradeLevel() const;
	f32 GetEngineBayUpgradeLevel(u32 slot) const;
	f32 GetTransmissionUpgradeLevel() const;
	f32 GetRadioUpgradeLevel() const;

	bool AlarmIndicatorActive() { return m_WasAlarmActive && m_HornSound;}
	bool IsHornActive() { return m_HornSound != NULL;}    
    void SetHydraulicSuspensionInputAxis(f32 hydraulicSuspensionX, f32 hydraulicSuspensionY);	
	void CalculateHydraulicSuspensionAngle();
	void OnJumpingVehicleJumpActivate();
	void OnJumpingVehicleJumpPrimed();
	void OnVehicleParachuteDeploy();
	void OnReducedSuspensionForceSet(bool isReduced);
	void OnVehicleParachuteStabilize();
	void OnVehicleParachuteDetach();
	void UpdateParachuteSound();
	void SetBombBayDoorsOpen(bool open);

	void TriggerSubmarineDiveSound(u32 soundHash);
	void ToggleHeliWings(bool deploy);
	void DeployOutriggers(bool deploy);
	void StartGliding();
	void StopGliding();
	void TriggerSimpleSoundFromSoundset(u32 soundSetHash, u32 soundFieldHash REPLAY_ONLY(, bool recordSound = true));

	void ScriptAppliedForce(const Vector3 &vecForce);
	void TriggerDoorSound(u32 hash, eHierarchyId hierarchyId, f32 volumeOffset = 0.0f);
	void TriggerDoorBreakOff(const Vector3 &pos, bool isBonnet = false, fragInst* doorFragInst = NULL);
	void TriggerDynamicImpactSounds(f32 volume);
	void TriggerDynamicExplosionSounds(bool isUnderwater);
	u32 GetLastTimeExploded() { return m_LastTimeExploded; }
	u32 GetLastTimeExplodedVehicle() { return m_LastTimeExplodedVehicle; }
	void TriggerSplash(const f32 downSpeed,bool outOfWater = false);
	void TriggerBoatEntrySplash(bool goneIntoWater, bool comeOutOfWater);
	void TriggerHeliRotorSplash();
	void TriggerHeadlightSmash(const u32 headlightIdx, const Vector3 &pos);
	void TriggerTyrePuncture(const eHierarchyId wheelId, bool slowPuncture = true);
	void TriggerIndicator(const bool isOn);
	void RevLimiterHit();
	void SetHitByRocket();
	audVehicleGadget* CreateVehicleGadget(audVehicleGadget::audVehicleGadgetType type);
	void ShutdownVehicleGadgets();
	const atArray<audVehicleGadget*>& GetVehicleGadgets() const { return m_VehicleGadgets; }

#if GTA_REPLAY
	void SetReplayRevs(f32 revs)				{ m_ReplayRevs = revs; }
	f32 GetReplayRevs() const					{ return m_ReplayRevs; }

	void SetReplayRevLimiter(bool onLimiter)	{ m_ReplayRevLimiter = onLimiter; }
	bool GetReplayRevLimiter() const			{ return m_ReplayRevLimiter; }
#endif

	void FixSiren();

	virtual u32 GetVehicleHornSoundHash(bool ignoreMods = false);
	void EnableParkingBeep(bool enable) {m_ParkingBeepEnabled = enable;}

	audWaveSlot* GetWaveSlot();
	audDynamicWaveSlot* GetDynamicWaveSlot() {return m_EngineWaveSlot;}
	void FreeWaveSlot(bool freeDSPEffects = true);

	void SetVehicleCaterpillarTrackSpeed(f32 speed) { m_VehicleCaterpillarTrackSpeed = speed; }
	void SetWheelRetractRate(f32 rate) { m_WheelRetractRate = rate; }
	bool SetScriptPriority(audVehicleScriptPriority priority, scrThreadId scriptThreadID);
	void CalculateEngineExhaustPositions();
	void GetWheelPosition(u32 wheelIndex, Vector3 &pos);
	void GetCachedWheelPosition(u32 wheelIndex, Vector3 &pos, audVehicleVariables& vehicleVariables);
#if NA_RADIO_ENABLED
	void SetLoudRadio(const bool loud);
	bool HasLoudRadio() const;
	void SetRadioStation(u8 radioStationIndex);
	void SetRadioStationFromNetwork(u8 radioStationIndex, bool bTriggeredByDriver);
	void ResetRadioStationFromNetwork();
	f32 GetFrontendRadioVolumeOffset();

	void SetRadioStation(const audRadioStation *station, const bool alsoRetuneFE = true, const bool isForcedStation = false);
#endif

	template <typename SoundRefType> void UpdateLoopWithLinearVolumeAndPitch(audSound **sound, SoundRefType soundRef, f32 linVolume, s32 pitch, audEnvironmentGroupInterface *occlusionGroup, audCategory *category, bool stopWhenSilent, bool smoothPitch, f32 environmentalLoudness = 1.0f, bool trackPosition = true, bool randomStartOffset = false)
	{
		if(linVolume > g_SilenceVolumeLin || !stopWhenSilent)
		{
			// volume warrants playing
			if(!*sound)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = occlusionGroup;
				initParams.Category = category;
				initParams.TrackEntityPosition = trackPosition;
				initParams.UpdateEntity = true;

				if(randomStartOffset)
				{
					initParams.StartOffset = audEngineUtil::GetRandomNumberInRange(0,99);
					initParams.IsStartOffsetPercentage = true;
				}

				CreateSound_PersistentReference(soundRef, sound, &initParams);
				if(!*sound)
				{
					return;
				}
				(*sound)->SetClientVariable((u32)AUD_VEHICLE_SOUND_UNKNOWN);
				(*sound)->PrepareAndPlay(GetWaveSlot(),false);
			}

			(*sound)->SetRequestedPitch(pitch);

			f32 volume = audDriverUtil::ComputeDbVolumeFromLinear(linVolume);
			ConditionalWarning((volume <= 24.0f), "Vehicle is attempting to update a loop with an invalid volume");
			(*sound)->SetRequestedVolume(Min(volume, 24.0f));
			(*sound)->SetRequestedEnvironmentalLoudnessFloat(environmentalLoudness);
			if(smoothPitch)
			{
				(*sound)->SetRequestedFrequencySmoothingRate(sm_FrequencySmoothingRate);
			}
		}
		else if(*sound)
		{
			(*sound)->StopAndForget();
		}
	}

	// Helper function to avoid pointlessly searching for the soundset sound ref when we're not going to actually use it
	template <typename SoundNameType> void UpdateSoundsetLoopWithLinearVolumeAndPitch(audSound **sound, SoundNameType soundName, audSoundSet* soundset, f32 linVolume, s32 pitch, audEnvironmentGroupInterface *occlusionGroup, audCategory *category, bool stopWhenSilent, bool smoothPitch, f32 environmentalLoudness = 1.0f, bool trackPosition = true, bool randomStartOffset = false)
	{
		audMetadataRef soundRef = g_NullSoundRef;

		if((!*sound) && (!stopWhenSilent || linVolume > g_SilenceVolumeLin))
		{
			soundRef = soundset->Find(soundName);
		}
			
		UpdateLoopWithLinearVolumeAndPitch(sound, soundRef, linVolume, pitch, occlusionGroup, category, stopWhenSilent, smoothPitch, environmentalLoudness, trackPosition, randomStartOffset);
	}

	void UpdateLoopWithLinearVolumeAndPitchRatio(audSound **sound, const u32 soundHash, f32 linVolume, f32 pitchRatio, audEnvironmentGroupInterface *occlusionGroup, audCategory *category, bool stopWhenSilence, bool smoothPitch, f32 environmentalLoudness = 1.0f, bool trackPosition = true);
	
    static bool IsVehicleInteriorSceneActive()                  { return sm_VehicleInteriorScene != NULL; }
	static bool IsVehicleFirstPersonTurretSceneActive()			{ return sm_VehicleFirstPersonTurretScene != NULL; }
    static bool IsBonnetCamSceneActive()                        { return sm_VehicleBonnetCamScene != NULL; }

	inline u32 GetLastHydraulicSuspensionStateChangeTime() const	{ return m_LastHydraulicSuspensionStateChangeTime; }
	inline u32 GetLastHydraulicEngageDisengageTime() const			{ return m_LastHydraulicEngageDisengageTime; }

	inline bool ShouldSuppressHorn() const						{ return m_SuppressHorn; }
	inline void SuppressHorn(bool suppress)						{ m_SuppressHorn = suppress; }
	inline bool GetIsInUnderCover()								{ return m_VehicleUnderCover;}
	inline bool IsPlayerSeatedInVehicle()						{ return m_IsPlayerSeatedInVehicle; }
	inline bool IsPlayerDrivingVehicle()						{ return m_IsPlayerDrivingVehicle; }
	inline bool IsPlayerVehicle()								{ return m_IsPlayerVehicle; }
	inline bool IsFocusVehicle()								{ return m_IsFocusVehicle; }
	inline void SetIsOnGround()									{ m_LastTimeOnGround = fwTimer::GetTimeInMilliseconds(); }
	inline bool GetIsInAirCached() const						{ return m_IsInAirCached; }		// Safe to call from the audio thread as this is cached in CVehicle::ProcessControl()
	inline void SetIsInAirCached(const bool isInAir)			{ m_IsInAirCached = isInAir; }	// Set from the game thread as it's unsafe to call IsInAir on car's from audio thread
	inline audVehicleType GetAudioVehicleType() const			{ return m_VehicleType; }
	inline void SetForceSiren(const bool force)					{ m_IsSirenForcedOn = force; }
	inline void SetBypassMPDriverCheck(const bool bypass)		{ m_SirenBypassMPDriverCheck = bypass; }
	inline void SetHornPermanentlyOn(bool on)					{ m_IsHornPermanentlyOn = on; } 
	inline void SetHornEnabled(bool enabled)					{ m_IsHornEnabled = enabled; }
	inline void SetHasMissileWarningSystem(bool hasSystem)		{ m_HasMissileLockWarningSystem = hasSystem; }
	inline u8 GetGPSType() const								{ return m_GpsType; }
	inline u8 GetGPSVoice() const								{ return m_GpsVoice; }
	inline void SetGPSType(u8 gpsType)							{ m_GpsType = gpsType; }
	inline void SetGPSVoice(u8 gpsVoice)						{ m_GpsVoice = (u8)Min((s32)gpsVoice, 4); }	
	inline RadioType GetRadioType() const						{ return m_RadioType; }
	inline void SetRadioType(const RadioType type)				{ m_RadioType = type; }
	inline const audRadioStation *GetLastRadioStation() const	{ return m_LastRadioStation; }
	inline RadioGenre GetRadioGenre() const						{ return m_RadioGenre; }
	inline CVehicle* GetVehicle()								{ return m_Vehicle; }
	inline void SetRadioEnabled(const bool enabled)				{ m_RadioDisabled = !enabled; }
	inline bool IsLastPlayerVeh()								{ return m_LastPlayerVehicle; }
	inline bool GetIsWaitingToInit() const						{ return m_IsWaitingToInit; }
	inline const audVehicleDSPSettings*	GetDSPSettings() const	{ return &m_DspSettings; }
	inline f32 GetPrevExhaustPostSubmixVol() const				{ return m_PrevExhaustPostSubmixVol; }
	inline f32 GetFwdSpeedLastFrame() const						{ return m_FwdSpeedLastFrame; }
	inline void SetWasScheduledCreation(bool wasScheduled)		{ m_WasScheduledCreation = wasScheduled; }
	inline phMaterialMgr::Id GetMainWheelMaterial()	const		{ return m_MainWheelMaterial; }
	inline bool IsForcedGameObjectResetRequired() const			{ return m_ForcedGameObjectResetRequired; }
	inline void SetForcedGameObjectResetRequired()				{ m_ForcedGameObjectResetRequired = true; }
	inline void SetNextClatterTime(u32 time)					{ m_NextClatterTime = time; }
	inline void SetScriptEngineDamageFactor(f32 damageFactor)	{ m_ScriptEngineDamageFactor = damageFactor; }
	inline f32 GetScriptEngineDamageFactor() const				{ return m_ScriptEngineDamageFactor; }
	inline void SetScriptBodyDamageFactor(f32 damageFactor)		{ m_ScriptBodyDamageFactor = damageFactor; }
	inline f32 GetScriptBodyDamageFactor() const				{ return m_ScriptBodyDamageFactor; }
	inline void SetReplaySpecialFlightModeAngVelocity(f32 mag)	{ m_SpecialFlightModeReplayAngularVelocity = mag; }
	inline const Vector3& GetCachedAngularVelocity() const		{ return m_CachedAngularVelocity; }
	inline const Vector3& GetCachedVelocity() const				{ return m_CachedVehicleVelocity; }
	inline const Vector3& GetCachedVelocityLastFrame() const	{ return m_CachedVehicleVelocityLastFrame; }
	inline void SetInAirRotationalForces(f32 forceX, f32 forceY){ m_InAirRotationalForceX = forceX; m_InAirRotationalForceY = forceY; }
	inline u32 GetTimeCreated()									{ return m_TimeCreated; }
	inline u32 GetDistFromListenerSq()							{ return m_DistanceFromListener2; }
	inline bool IsPersonalVehicle() const						{ return m_IsPersonalVehicle; }
	inline bool IsJumpSoundPlaying() const						{ return (m_JumpRecharge != NULL); }
	inline void SetRechargeSoundStartOffset(u32 startOffset)	{ m_RechargeStartOffset = startOffset; }
	inline bool IsWaitingOnActivatedJumpLand() const			{ return m_IsWaitingOnActivatedJumpLand;  }
	inline void ResetWaitingOnActivatedJumpLand()				{ m_IsWaitingOnActivatedJumpLand = false;  }

#if NA_RADIO_ENABLED
	inline audRadioEmitter* GetRadioEmitter()					{ return &m_RadioEmitter; }
	void SetLastRadioStation(const audRadioStation *station)	{ m_LastRadioStation = station; }
	inline void SetRadioOffState(bool isOff)					{ m_IsRadioOff = isOff; }
	inline bool IsRadioSwitchedOff() const						{ return m_IsRadioOff; }
	inline bool IsRadioSwitchedOn() const						{ return !m_IsRadioOff; }
	inline u32 GetRadioHPFCutoff() const						{ return m_RadioHPFCutoff; }
	inline u32 GetRadioLPFCutoff() const						{ return m_RadioLPFCutoff; }
	inline float GetRadioRolloff() const						{ return m_RadioRolloffFactor; }
	inline const audRadioStation *GetRadioStation() const		{ return m_RadioStation; }
#endif

	inline void SetHasWindowToSmashCache(const s32 i, const bool hasWindowToSmash)
	{ 
		if(naVerifyf(i < NumWindowHierarchyIds, "Invalid eHierarchy ID: %d", i))
		{ 
			hasWindowToSmash ? m_HasWindowToSmashCache.Set(i) : m_HasWindowToSmashCache.Clear(i);
		} 
	}

#if !__NO_OUTPUT
	const char *GetVehicleModelName() const;
#endif

	void TriggerSubmarineCrushSound();
	void TriggerSubmarineImplodeWarning(bool fullyImplode);
	void StopSubTransformSound() { m_ShouldStopSubTransformSound = true; }

	f32 GetBurnoutRatio() { return m_BurnoutSmoother.GetLastValue(); }
	f32 ComputeOutsideWorldOcclusion();
	f32 GetAmbientRadioVolume() const;
	f32 GetOpenness(bool cameraRelative = true);
	f32 GetConvertibleRoofOpenness() const;
    bool AreHydraulicsActive() const;
    bool HydraulicsModifiedRecently() const;
	virtual f32 GetMinOpenness() const {return 0.f;};
	void InitOcclusion();
	void SetEnvironmentGroupSettings(const bool useInteriorDistanceMetrics, const u8 maxPathDepth);
	bool IsRadioEnabled() const;
	bool IsRadioDisabledByScript() const { return m_RadioDisabled; }
	void SetScriptForcedRadio() { m_ScriptForcedRadio = true; }
	void SetScriptRequestedRadioStation() { m_ScriptRequestedRadioStation = true; }
	bool GetScriptRequestedRadioStation() const { return m_ScriptRequestedRadioStation; }
	virtual bool AircraftWarningVoiceIsMale() {return true;}

	audVehicleLOD GetPrevDesiredLOD() const { return m_PrevDesiredLOD; }
	f32 GetTimeAtDesiredDesiredLOD() const { return m_TimeAtDesiredLOD; }

	void SetPrevDesiredLOD(audVehicleLOD desiredLOD) { m_PrevDesiredLOD = desiredLOD; }	
	void SetTimeAtDesiredLOD(f32 time) { m_TimeAtDesiredLOD = time; }

#if __BANK
	static void UpdateCarRecordingTool();
	static void LoadCRAudioSettings();
	static void CancelCRAudioSettings();
	static void CreateAudioSettings();
	static void CreateCREvent();
	static void FormatCRAudioSettingsXml(char * xmlMsg, const size_t bufferSize, const char * settingsName);
	static void UpdateProgressBar();
	static void SkipTo();
	static void FwdCR();
	static void RewindCR();
	static void UpdateTimeBar();
	static void AddWidgets(bkBank &bank);
	static u32 GetPlayBackVehModelIndex() {return sm_DebugCarRecording.modelId;}
	static u32 GetDefaultPlayBackVehModelIndex() {return 7653;}// tailgater
	static bool AddCarRecordingWidgets(bkBank *bank);
	static bool ShouldRenderRecording(const s32 index);
#endif

	static void InitClass();
	static void UpdateClass();
	static void ClassFinishUpdate();
	static void UpdateActivationRanges();
	static void ShutdownClass();
	static bool IsPlayerOnFootOrOnBicycle();
	static void ProcessScriptVehicleBankLoad();
	static bool IsPlayerVehicleStarving()	{ return sm_FramesSincePlayerVehicleStarvation > 0; }
	static void SetPlayerVehicleStarving()	{ sm_FramesSincePlayerVehicleStarvation = 3; }
	static audSoundSet* GetExtrasSoundSet() { return &sm_ExtrasSoundSet; }

	virtual void OnPlayerOpenDoor() {}

	u32 GetVehicleModelNameHash() const;
	u32 GetVehicleHydraulicsSoundSetName() const;
	bool ShouldForceEngineOff() const;
	void SetTriggerFastSubcarTransformSound() { m_TriggerFastSubCarTransformSound = true; }
	void SetForceReverseWarning(bool forceWarning) { m_ForceReverseWarning = forceWarning; }

// Protected methods
protected:
	inline audSound* GetEngineEffectSubmixSound() const		{ return m_EngineSubmixIndex >= 0? sm_VehicleEngineSubmixes[m_EngineSubmixIndex].submixSound : NULL; }
	inline audSound* GetExhaustEffectSubmixSound() const	{ return m_ExhaustSubmixIndex >= 0? sm_VehicleExhaustSubmixes[m_ExhaustSubmixIndex].submixSound : NULL; }
	static u32 GetWheelSoundUpdateClientVar(u32 wheelIndex);
	audQuadrants GetVehicleQuadrant();
	u32 GetDistance2FromSniperAimVector() const;
	bool IsHiddenByNetwork() const;
	void UpdateDoors();
	void UpdateRoadNoiseSounds(audVehicleVariables& vehicleVariables);
	void UpdateClatter(audVehicleVariables& vehicleVariables);
	bool RequestWaveSlot(audWaveSlotManager* waveSlotManager, u32 soundID, bool dspEffectsRequired = true);
	bool RequestLowLatencyWaveSlot(u32 soundID);
	bool RequestSFXWaveSlot(u32 soundID, bool useHighQualitySlot = false);
	void GetInteriorSoundOcclusion(f32 &volAtten, u32 &cutoffFreq);
	void TriggerClatter(audMetadataRef clatterSoundRef, f32 volumeLin);
	void StopSirenIfBroken();
	void SetPlayerHornOn(bool val) { m_PlayerHornOn = val; }
	void StopRoadNoiseSounds();
	bool AreWarningSoundsValid();
	bool IsSafeToApplyDSPChanges();
	bool ShouldForceSnow();
	void StopSubmersibleSounds();
	void AddCommonDSPParameters(audVehicleDSPSettings& dspSettings);
	virtual bool RequiresWaveSlot() const;
	virtual AmbientRadioLeakage GetAmbientRadioLeakage() const;
	
	void UpdateSubmersible(audVehicleVariables& vehicleVariables, BoatAudioSettings* boatSettings);
	void InitSubmersible(BoatAudioSettings* boatSettings);
	void TriggerWaterDiveSound();	

	virtual bool CalculateIsInWater() const									{ return false; }
	virtual bool IsSubmersible() const										{ return false; }
	virtual bool IsWaterDiveSoundPlaying() const							{ return false; }
	virtual f32 GetRoadNoiseVolumeScale() const								{ return 1.0f; }
	virtual f32 GetSkidVolumeScale() const									{ return 1.0f; }
	virtual f32 GetIdleHullSlapVolumeLinear(f32 UNUSED_PARAM(speed)) const	{ return 1.0f; }
	virtual u32 GetNormalPattern() const									{ return 0; }
	virtual u32 GetAggressivePattern() const								{ return 0; }
	virtual u32 GetHeldDownPattern() const									{ return 0; }
	virtual u32 GetSoundFromObjectData(u32 UNUSED_PARAM(fieldHash)) const	{ return 0; }
	virtual f32 GetVehicleModelRadioVolume() const							{ return 0.0f; }
	virtual f32 GetInteriorViewEngineOpenness() const						{ return 0.0f; }
	virtual bool InitVehicleSpecific()										{ return true; }
	virtual VehicleVolumeCategory GetVehicleVolumeCategory()				{ return VEHICLE_VOLUME_NORMAL; }
	virtual s32 GetShockwaveID() const										{ return kInvalidShockwaveId; }
	virtual void SetShockwaveID(s32 UNUSED_PARAM(shockwave))				{}
	virtual void UpdateVehicleSpecific(audVehicleVariables& UNUSED_PARAM(vehicleVariables), audVehicleDSPSettings& UNUSED_PARAM(variables))	{}
	virtual const u32 GetWindClothSound() const									{return 0;};
	virtual f32 GetSeriousDamageThreshold()									{return 0.0f;}
	virtual f32 GetCriticalDamageThreshold()								{return 0.0f;}
	virtual f32 GetOverspeedValue()											{return 0.0f;}

// Private methods
private:	
	void UpdateWaveImpactSounds(audVehicleVariables& params);
	void UpdateClothSounds(audVehicleVariables& params);
	void CalculateWheelMaterials(audVehicleVariables& params);
	void UpdateWheelDetail(audVehicleVariables& params);
	void UpdateSkids(audVehicleVariables& params);
	float UpdateSkidsVolume(audVehicleVariables& params, bool tarmacSkids, s32 numWheelsTouching, float fwdSlipAngle, const float* slipAngleDeltas);
	void UpdateWater(audVehicleVariables& params);
	void UpdateWheelWater(audVehicleVariables& params);
	void UpdateWheelSpin(audVehicleVariables& params);
	void UpdateFreewayBumps(audVehicleVariables& params);
	void UpdateRidgedSurfaces(audVehicleVariables& params);
	void UpdateFastRoadNoise(audVehicleVariables& params);
	void UpdateNPCRoadNoise(audVehicleVariables& params);
	void UpdateSurfaceSettleSounds(audVehicleVariables& params);
	void PopulateVehicleVariables(audVehicleVariables& vehicleVariables);
	void PopulateVehicleLODVariables(audVehicleVariables& vehicleVariables);
	void UpdateWarningSounds(audVehicleVariables& vehicleVariables);
	void UpdateHorn();
	void UpdateLandingGear();
	void UpdateWeaponCharge();
	void UpdateSiren(audVehicleVariables& vehicleVariables);
	void UpdateWheelDirtyness(audVehicleVariables& params);
	void UpdateCaterpillarTracks(audVehicleVariables& params);
	void UpdateDrowning(audVehicleVariables& params);
	void UpdateRain(audVehicleVariables& params);
	void ShutdownPrivate();
	void UpdateMissileLock();
	void UpdateDamageWarning();
	void UpdateAircraftWarningSpeech();
	void UpdateCabinTone(audVehicleVariables& params);
	void UpdateCarRecordings();
	void UpdateSpecialFlightModeTransformSounds();
	void CheckForRecordingChange();
	void UpdateEnvironment();
	void UpdateRadio();
	void UpdatePedStatus(const u32 distFromListenerSquared);
	void UpdateHeadLights();
	void UpdateWeaponBlades();
	void UpdateVehicleJump();
	void PlayNormalHorn();
	void PlayAggressiveHorn();
	void UpdateDetachedBonnetSound(bool firstFrame = false);
	bool InitVehicle();
	const u32 GetAudioWheelIndex(const u32 wheelId) const;
	static bool IsPedOnMovingTrain(const CPed* pPed);	
	bool ShouldRequestRadio(const AmbientRadioLeakage leakage, bool &shouldStopRadio) const;	

	virtual void ConvertToDummy()			{};
	virtual void ConvertToDisabled()		{};
	virtual void ConvertFromDummy()			{};
	virtual void ConvertToSuperDummy()		{ConvertToDisabled();}
	virtual bool AcquireWaveSlot()			{ return true; }
	virtual audVehicleLOD GetDesiredLOD(f32 fwdSpeedRatio, u32 distFromListenerSq, bool visibleBySniper);
	void UpdateVehicleLOD(f32 fwdSpeedRatio, u32 distanceFromListenerSq, bool visibleBySniper);

#if __BANK
	void UpdateDebug();
#endif

	audMetadataRef GetHornLoopRef(const u32 hornPattern);

	static u8 FindSkidSurfacePriority(const u8 priorityType);

public:

	enum { NumWindowHierarchyIds = 8 };
	static const eHierarchyId sm_WindowHierarchyIds[NumWindowHierarchyIds];

// Protected attributes
protected:
	atRangeArray<phMaterialMgr::Id, NUM_VEH_CWHEELS_MAX> m_LastMaterialIds;

	bool m_QuadrantDirty : 1;
	bool m_TyreChirpAccelPlayed : 1;
	bool m_TyreChirpBrakePlayed : 1;
	bool m_IsWaitingToInit : 1;
	bool m_WasWrecked : 1 ;
	bool m_IsRadioOff : 1;
	bool m_HasLoudRadio : 1;
	bool m_HasSmashedWindow : 1;
	bool m_HeadlightsSwitched : 1;
	bool m_HeadlightsCoverSwitched : 1;
	bool m_IsSirenFucked : 1;
	bool m_IsSirenForcedOn : 1;
	bool m_SirenBypassMPDriverCheck : 1;
	bool m_IsHornPermanentlyOn : 1;
	bool m_WasHornPermanentlyOn : 1;
	bool m_ParkingBeepEnabled : 1;
	bool m_KeepSirenOn : 1;
	bool m_SirenControlledByConductor : 1;
	bool m_WasAlarmActive : 1;
	bool m_ForcedGameObjectResetRequired : 1;
	bool m_PreserveEnvironmentGroupOnShutdown : 1;
	bool m_LastPlayerVehicle : 1;
	bool m_IsPlayerSeatedInVehicle : 1;
	bool m_IsNetworkPlayerSeatedInVehicle : 1;
	bool m_IsPlayerVehicle : 1;
	bool m_IsFocusVehicle : 1;
	bool m_IsPlayerDrivingVehicle : 1;
	bool m_AmbientRadioDisabled : 1;
	bool m_RadioDisabled : 1;
	bool m_ScriptForcedRadio : 1;
	bool m_ScriptRequestedRadioStation : 1;
	bool m_HasIndicatorRequest : 1;
	bool m_IsIndicatorRequestedOn : 1;
	bool m_WasMissileApproaching : 1;
	bool m_WasBeingTargeted : 1;
	bool m_WasAcquiringTarget : 1;
	bool m_HadAcquiredTarget : 1;
	bool m_HasMissileLockWarningSystem : 1;
	bool m_PlayerVehicleAlarmBehaviour : 1;
	bool m_VehicleUnderCover : 1;
	bool m_IsPersonalVehicle : 1;
	bool m_ReapplyDSPEffects : 1;
	bool m_SuppressHorn : 1;
	bool m_HasTriggeredSubmarineDiveHorn : 1;
	bool m_TriggerDeferredSubmarineDiveHorn : 1;
	bool m_HasTriggeredSubmarineSurface : 1;
	bool m_TriggerDeferredSubmarineSurface : 1;
	bool m_RequiresSFXBank : 1;
	bool m_TriggerFastSubCarTransformSound : 1;
	bool m_ForceReverseWarning : 1;

	atFixedBitSet<25> m_WaterSamples; // BOAT_WATER_SAMPLES
	audSoundSet m_VehSirenSounds;
	f32 m_SubmarineDiveHoldTime;
	u32 m_LastTimeInAir;
	u32 m_LastTimeTwoWheeling;
	u32 m_LastTimeExploded;
	u32 m_LastTimeExplodedVehicle;
	f32 m_LastClatterUpVel;
	f32 m_MissileApproachTimer;
	f32 m_LastClatterAcceleration;
	f32 m_ScriptEngineDamageFactor;
	f32 m_ScriptBodyDamageFactor;
	f32 m_SpecialFlightModeReplayAngularVelocity;
	f32 m_CachedNonCameraRelativeOpenness;
	f32 m_CachedCameraRelativeOpenness;
	u32 m_NextClatterTime;
	f32 m_FwdSpeedLastFrame;
	u32 m_DistanceFromListener2LastFrame;
	u32 m_DistanceFromListener2;
	
	Vec3V m_ExhaustOffsetPos;
	Vec3V m_EngineOffsetPos;
	Vec3V m_HornOffsetPos;

	Vector3 m_CachedVehicleVelocityLastFrame;
	Vector3 m_CachedVehicleVelocity;
	Vector3 m_CachedAngularVelocity;
	Vector3 m_NearestNodeAddressPos;
	Vector3 m_ClatterOffsetPos;
	Vector3 m_LastClatterWorldPos;
    Vector2 m_HydraulicsInputAxis;
    Vector2 m_PrevHydraulicsInputAxis;
	Vector2 m_HydraulicsSuspensionAngle;
	Vector2 m_PrevHydraulicsSuspensionAngle;

	atRangeArray<audScene*,CarRecordingAudioSettings::MAX_PERSISTENTMIXERSCENES> m_CarRecordingPersistentScenes;
	atArray<audVehicleGadget*> m_VehicleGadgets;
	CarRecordingAudioSettings *m_CRSettings;
	audVehicleFireAudio m_VehicleFireAudio;
	audSimpleSmootherDiscrete m_WheelDirtinessSmoother;
	audSimpleSmootherDiscrete m_WheelGlassinessSmoother;
	audSimpleSmoother m_EngineEffectWetSmoother;
	audSimpleSmootherDiscrete m_DrowningFactorSmoother;
	audSimpleSmootherDiscrete m_WheelSpinSmoother;
	audSimpleSmootherDiscrete m_VisibilitySmoother;
	audSimpleSmootherDiscrete m_ChassisStressSmoother;
    audSimpleSmootherDiscrete m_HydraulicSuspensionInputSmoother;
	audSimpleSmootherDiscrete m_HydraulicSuspensionAngleSmoother;
	audVehicleDSPSettings m_DspSettings;

	audCurve m_SubTurningToSweetenerVolume;
	audCurve m_SubTurningToSweetenerPitch;
	audCurve m_SubTurningEnginePitchModifier;
	audCurve m_SubRevsToSweetenerVolume;
	audSimpleSmootherDiscrete m_SubAngularVelocityVolumeSmoother;
	audSimpleSmootherDiscrete m_SubDiveVelocitySmoother;

	CollisionMaterialSettings* m_CachedMaterialSettings;
	audDynamicWaveSlot* m_EngineWaveSlot;
	audDynamicWaveSlot* m_LowLatencyWaveSlot;
	audDynamicWaveSlot* m_SFXWaveSlot;
	audSound* m_SpecialFlightModeTransformSound;
	audSound* m_VehicleClatter;
	audSound* m_CaterpillarTrackLoop;
	audSound* m_LandingGearSound;
	audSound* m_JumpRecharge;
	audSound* m_ChassisStressLoop;
	audSound* m_SirenLoop;
	audSound* m_SirenBlip;
	audSound* m_IdleHullSlapSound;
	audSound* m_ParachuteLoop;
	audSound* m_DoorOpenWarning, *m_ReverseWarning;
	audSound* m_WheelDetailLoop;
	audSound* m_MainSkidLoop, *m_SideSkidLoop, *m_UndersteerSkidLoop, *m_WheelSpinLoop, *m_WetWheelSpinLoop;
	audSound* m_SurfaceSettleSound;
	audSound* m_HornSound;
	audSound* m_AlarmSound;
	audSound* m_BeingTargetedSound;
	audSound* m_MissileApproachingSound;
	audSound* m_MissileApproachingSoundImminent;
	audSound* m_AllClearSound;
	audSound* m_AcquiringTargetSound;
	audSound* m_RainLoop;
	audSound* m_DetachedBonnetSound;
	audSound* m_WaterLappingSound;
	audSound* m_VehicleCabinTone;
	audSound* m_SubTurningSweetener;
	audSound* m_SubPropSound;
	audSound* m_SubExtrasSound;
	audSound* m_SubmersibleCreakSound;
	audSound* m_WaterDiveSound;
	CVehicle* m_Vehicle;
	CPed*	  m_LastVehicleDriver;
	audSound *m_FranklinSpecialTraction;
	audSound *m_FranklinSpecialPassby;
	audSound *m_DamageWarningSound;
	audSound *m_WaterTurbulenceSound;

	static audSoundSet sm_TarmacSkidSounds;
	static audSoundSet sm_ClatterSoundSet;
	static audSoundSet sm_ChassisStressSoundSet;

	atRangeArray<float, NUM_VEH_CWHEELS_MAX> m_LastSlipAngle;
	audSound *m_ShallowWaterSounds[NUM_VEH_CWHEELS_MAX];
	audSound *m_ShallowWaterEnterSound;
	AmbientRadioLeakage m_RadioLeakage;
	RadioGenre m_RadioGenre;
	RadioType m_RadioType;
	audVehicleLOD m_VehicleLOD;
	audVehicleLOD m_PrevDesiredLOD;
	audVehicleType m_VehicleType;

	u32 m_LastCaterpillarTrackValidTime;
	u32 m_LastHydraulicSuspensionStateChangeTime;
	u32 m_LastHydraulicEngageDisengageTime;
	f32 m_IgnitionHoldTime;
	f32 m_DrowningFactor;
	u32 m_VehicleMakeSoundHash;
	u32 m_VehicleModelSoundHash;
	u32 m_VehicleCategorySoundHash;
	u32 m_ScannerVehicleSettingsHash;
	u32 m_LastRevLimiterTime;
	u32 m_LastHitByRocketTime;
	u32 m_PrevFastRoadNoiseHash;
	u32 m_PrevDetailRoadNoiseHash;
	u32 m_SpecialFlightModeTransformSoundToTrigger;
	REPLAY_ONLY(f32 m_ReplayRevs;)
	f32 m_InAirRotationalForceX;
	f32 m_InAirRotationalForceXLastFrame;
	f32 m_InAirRotationalForceY;
	f32 m_InAirRotationalForceYLastFrame;
	f32 m_TimeAtCurrentLOD;
	u32 m_ForcedGameObject;
	u32 m_LastSurfaceChangeClatterTime;
	f32 m_TimeAtDesiredLOD;
	f32 m_FakeAudioHealth;
	s32 m_WeaponChargeBone;
	f32 m_WeaponChargeLevel;
	s32 m_PlayerVehicleMixGroupIndex;
	u32 m_LastTimeTerrainProbeHit;
	u32 m_LastTimeTerrainProbeNotHit;
	u32 m_LastTimeGroundProbeNotHit;
	u32 m_TimeOfLastAircraftOverspeedWarning;
	u32 m_LastTimeAircraftDamageReported;
	u32 m_TimeOfLastAircraftLowFuelWarning;
	u32 m_TimeOfLastAircraftLowDamageWarning;
	u32 m_TimeOfLastAircraftHighDamageWarning;
	u32 m_TimeOfLastAircraftEngineDamageWarning;
	u32 m_TimeCreated;
	u16 m_DetachedBonnetLevelIndex;
	s16 m_EnginePitchOffset;
	s16 m_HornSoundIndex;
	s16 m_FakeEngineHealth;
	s16 m_FakeBodyHealth;
	u8 m_GpsType;
	u8 m_GpsVoice;
	u8 m_LastCREventProcessed;
	u32 m_RechargeStartOffset;

	bool m_WasScheduledCreation : 1;
	bool m_PlayerHornOn : 1;
	bool m_IsInAirCached :1;
	bool m_HasPlayedEngineOnFire1:1;
	bool m_HasPlayedEngineOnFire2:1;
	bool m_HasPlayedEngineOnFire3:1;
	bool m_HasPlayedEngineOnFire4:1;
	bool m_TriggeredDispatchEnteredFreeway:1;
	bool m_TriggeredDispatchExitedFreeway:1;
	REPLAY_ONLY(bool m_ReplayRevLimiter:1;)

	static audWaveSlotManager sm_StandardWaveSlotManager;
	static audWaveSlotManager sm_HighQualityWaveSlotManager;
	static audWaveSlotManager sm_LowLatencyWaveSlotManager;

	static audSoundSet sm_ExtrasSoundSet;
	static audSoundSet sm_LightsSoundSet;
	static audSoundSet sm_MissileLockSoundSet;
	static audSmoother sm_HighSpeedSceneApplySmoother;
	static audSmoother sm_OpennessSmoother;

	static f32 sm_CheapDistance;
	static f32 sm_DoorConeInnerAngleScaling;
	static f32 sm_DoorConeOuterAngle;
	static f32 sm_IgnitionMinHold, sm_IgnitionMaxHold;
	static f32 sm_StartingIgnitionMinHold, sm_StartingIgnitionMaxHold;
	static f32 sm_FrequencySmoothingRate;
	static u32 sm_LodSwitchFadeTime;
	static u32 sm_ScheduledVehicleSpawnFadeTime;

	static s32 sm_FramesSincePlayerVehicleStarvation;
	static f32 sm_GranularActivationRangeScale;
	static u32 sm_NumVehiclesInDummyRange[NumQuadrants];
	static u32 sm_NumDummyVehiclesCounter;
	static u32 sm_NumDummyVehiclesLastFrame;
	static f32 sm_ActivationRangeScale;
	static f32 sm_DummyRangeScale[NumQuadrants];
	static f32 sm_VehDeltaSpeedForCollision;
	static u32 sm_NumVehiclesInRainRadius[NumQuadrants];
	static f32 sm_RainRadiusScalar[NumQuadrants];
	static u32 sm_NumVehiclesInWaterSlapRadius[NumQuadrants];
	static f32 sm_WaterSlapRadiusScalar[NumQuadrants];
	static u32 sm_NumVehiclesInGranularActivationRangeLastFrame;
	static u32 sm_NumVehiclesInActivationRangeLastFrame;

// Private types
private:
	struct WheelMaterialUsage
	{
		CollisionMaterialSettings* materialSettings;
		u32 numWheels;
		u32 wheelIndex;
	};

// Private attributes
private:
#if NA_RADIO_ENABLED
	audEntityRadioEmitter m_RadioEmitter;
#endif
	
	atRangeArray<audSound*,AUD_MAX_WHEEL_WATER_SOUNDS> m_WaveImpactSounds;
	atFixedBitSet<NumWindowHierarchyIds> m_HasWindowToSmashCache;

	CNodeAddress m_NearestNodeAddress;
	Vector2 m_LastRoadPosFront;
	Vector2 m_LastRoadPosBack;
	f32 m_PrevDirtinessVolume;
	f32 m_PrevGlassinessVolume;
	u32 m_LastFreewayBumpFront;
	u32 m_LastFreewayBumpBack;
	f32 m_LastDriftAngle;
	f32 m_LastDriftAngleDelta;
	u32 m_LastDriftChirpTime;
	f32 m_DistanceBetweenWheels;
	f32 m_PrevEnginePostSubmixVol;
	f32 m_PrevExhaustPostSubmixVol;
	u32 m_LastTimeOnGround;
	u32 m_LastRidgedRoadNoise;
	u32 m_RadioLPFCutoff;
	u32 m_RadioHPFCutoff;

	audVolumeConeLite m_RoadNoiseLeftCone, m_RoadNoiseRightCone;
	audVehicleCollisionAudio m_CollisionAudio;
	audSimpleSmoother m_SirensVolumeSmoother;
	audSimpleSmoother m_RainLoopVolSmoother;
	audSimpleSmootherDiscrete m_DriftFactorSmoother;
	audSmoother m_MainSkidSmoother;
	audSmoother m_LateralSkidSmoother;
	audSimpleSmoother m_BurnoutSmoother;
	audSimpleSmoother m_SurfaceSettleSmoother;
	audSimpleSmoother m_NPCRoadNoiseSmoother;
	audSimpleSmoother m_SubAngularVelocitySmoother;	

	audSound *m_WeaponChargeSound;
	audSound *m_DrowningRadioSound;
	audSound *m_RoadNoiseNPC;
	audSound *m_RoadNoiseFast;
	audSound *m_RoadNoiseHeavy;
	audSound *m_RoadNoiseWet;
	audSound *m_WetSkidLoop;
	audSound *m_DriveWheelSlipLoop;
	audSound *m_RoadNoiseRidged;
	audSound *m_RoadNoiseRidgedPulse;
	audSound *m_ExhastProximityPulseSound;
	audSound *m_DirtyWheelLoop;
	audSound *m_GlassyWheelLoop;
	audSound *m_ClothSound;
	audSound *m_CinematicTireSweetener;	
	audSound *m_StuntTunnelSound;

	const audRadioStation *m_RadioStation;
	const audRadioStation *m_LastRadioStation;

	f32 m_VehRainInWaterVol;
	f32 m_VehicleCaterpillarTrackSpeed;
	f32 m_SteeringAngleLastFrame;
	f32 m_RadioRolloffFactor;
	f32 m_RadioOpennessVol;
	u32 m_LastSplashTime;
	u32 m_LastHeliRotorSplashTime;
	u32 m_HornType;
	u32 m_HornHeldDownTime;
	f32	m_SirensDesireLinVol;
	f32 m_TimeSinceUpdatedRoadNoise;
	f32 m_FinalParam;
	f32 m_WheelRetractRate;
	u32 m_LastSirenChangeTime;
	f32 m_BurnoutPitch;
	u32 m_TimeToPlaySiren;
	f32 m_PrevAverageDriveWheelSpeed;
	u32 m_LastTyreBumpTimes[NUM_VEH_CWHEELS_MAX];
	BANK_ONLY(f32 m_PrevNPCRoadNoiseAttenuation;)

	// Vehicle debris
	u32 m_BulletHitCount;
	u32 m_TimeSinceLastBulletHit;
	u32 m_OverridenHornHash;

	u32 m_LastTimeAlarmPlayed;
	u32 m_RandomAlarmInterval;

	u32 m_TimeEnteredFreeway;
	u32 m_TimeExitedFreeway;
	u32 m_LastScriptDoorModifyTime;

	enum { kMaxAudioWeaponBlades = 4 };	
	atRangeArray<audSound*, kMaxAudioWeaponBlades> m_BladeImpactSound;
	atRangeArray<u32, kMaxAudioWeaponBlades> m_LastWeaponBladeCollisionTime;
	atRangeArray<bool, kMaxAudioWeaponBlades> m_LastWeaponBladeCollisionIsPed;
	atRangeArray<bool, kMaxAudioWeaponBlades> m_IsWeaponBladeColliding;

	s16 m_FreewayBumpPitch;
	s8 m_EngineSubmixIndex;
	s8 m_ExhaustSubmixIndex;
	audCarAlarmType m_AlarmType;

	audSirenState m_SirenState;
	audQuadrants m_VehicleQuadrant;	

	u8 m_NetworkCachedRadioStationIndex;

protected:
	phMaterialMgr::Id m_MainWheelMaterial;
	scrThreadId m_ScriptPriortyVehicleThreadID;
	audVehicleScriptPriority m_ScriptPriority;
	f32 m_SubAngularVelocityMag;
	f32 m_TimeStationary;
	f32 m_EngineWaterDepth;
	f32 m_InWaterTimer;	
	f32 m_OutOfWaterTimer;
	f32 m_RidgedSurfaceDirectionMatch;
	s32 m_SkidPitchOffset;
	u32 m_LastWaveHitTime;
	u32 m_LastBigWaveHitTime;
	u32 m_LastWaterLeaveTime;
	u32 m_LastStuntRaceSpeedBoostTime;
	u32 m_LastStuntRaceRechargeTime;
	u32 m_LastStuntRaceSpeedBoostIntensityIncreaseTime;
	u32 m_LastRechargeIntensityIncreaseTime;
	u32 m_LastSubCarRudderTurnSoundTime;
	u32 m_SubmarineTransformSoundHash;
	u8 m_HydraulicActivationDelayFrames;
	u8 m_SpeedBoostIntensity;
	u8 m_RechargeIntensity;

	bool m_ShouldStopSubTransformSound : 1;
	bool m_MustPlaySiren : 1;
	bool m_ShouldStopSiren : 1;
	bool m_UseSirenForHorn : 1;
	bool m_OverrideHorn : 1;
	bool m_IsFastRoadNoiseDisabled : 1;
	bool m_WantsToPlaySiren : 1;
	bool m_ShouldPlayPlayerVehSiren : 1;
	bool m_IsSirenOn : 1;
	bool m_PlaySirenWithNoNetworkPlayerDriver : 1;
	bool m_HasToTriggerAlarm : 1;
	bool m_IsWaitingOnActivatedJumpLand : 1;
	bool m_WasInInteriorViewLastFrame : 1;

	bool m_IsVehicleBeingStopped : 1;
	bool m_IsHornEnabled : 1;
	bool m_IsToyCar : 1;
	bool m_IsReversing : 1;
	bool m_WasEngineOnLastFrame : 1;
	bool m_DummyLODValid : 1;
	bool m_ResetDSPEffects : 1;
	bool m_ReapplyDSPPresets : 1;
	bool m_JustCameOutOfWater : 1;
	bool m_InCarModShop : 1;
	bool m_WasInCarModShop : 1;
	bool m_PlayedDiveSound : 1;
	bool m_IsScriptTriggeredHorn : 1;
	bool m_IsBeingNetworkSpectated : 1;
	bool m_WasInWaterLastFrame : 1;
	bool m_WasOnFreeWayLastFrame : 1;
	bool m_OnStairs : 1;
	bool m_WasOnStairs : 1;
	bool m_IsPlayerPedInFullyOpenSeat : 1;
    bool m_ShouldPlayBonnetDetachSound : 1;
	bool m_ShouldTriggerHydraulicBounce : 1;
	bool m_ShouldTriggerHydraulicActivate : 1;
	bool m_ShouldTriggerHydraulicDeactivate : 1;
	bool m_IsLandingGearRetracting : 1;
	bool m_ShouldPlayLandingGearSound : 1;
	bool m_ShouldStopLandingGearSound : 1;
	bool m_WasSuspensionDropped : 1;
	bool m_SpecialFlightModeWingsDeployed : 1;
	bool m_WasReducedSuspensionForceSet : 1;

#if GTA_REPLAY
	bool m_ReplayIsInWater : 1;
	bool m_ReplayVehiclePhysicsInWater : 1;
#endif

	// Public attributes
public:
	bool m_HasScannerDescribedModel : 1;
	bool m_HasScannerDescribedManufacturer : 1;
	bool m_HasScannerDescribedColour : 1;
	bool m_HasScannerDescribedCategory : 1;
#if __BANK
	static bool sm_LoadExistingCR;
	static bool sm_VehCreated;
	static audioCarRecordingInfo sm_DebugCarRecording;
	static char sm_CRAudioSettingsCreator[128];
#endif

	// Protected static members
protected:
	static f32 sm_PlayerVehicleOpenness;

	// Private static members
private:

	static audCurve sm_InteriorOpenToCutoffCurve, sm_InteriorOpenToVolumeCurve;
	static audCurve sm_RoofToOpennessCurve;
	static audCurve sm_RoadNoiseDetailVolCurve, sm_RoadNoiseFastVolCurve, sm_RoadNoiseSteeringAngleToAttenuation, sm_RoadNoiseSteeringAngleCRToVolume, sm_RoadNoisePitchCurve, sm_RoadNoiseSingleVolCurve, sm_RoadNoiseBicycleVolCurve;
	static audCurve sm_SkidSpeedRatioToPitch, sm_SkidDriftAngleToVolume, sm_RecentSpeedToSettleVolume, sm_RecentSpeedToSettleReleaseTime;
	static audCurve sm_SlipToUndersteerVol;
	static audCurve sm_SkidFwdSlipToMainVol;
	static audCurve sm_NPCRoadNoiseRelativeSpeedToVol;

	static u32 sm_NumVehiclesInGranularActivationRange;
	static u32 sm_NumVehiclesInActivationRange;

	struct audVehicleSubmix
	{
		audSound* submixSound;
		audVehicleType vehicleType;
		bool isGranular;
	};

	static atBitSet sm_GranularSlots;
	static audVehicleSubmix sm_VehicleEngineSubmixes[EFFECT_ROUTE_VEHICLE_ENGINE_MAX - EFFECT_ROUTE_VEHICLE_ENGINE_MIN + 1];
	static audVehicleSubmix sm_VehicleExhaustSubmixes[EFFECT_ROUTE_VEHICLE_EXHAUST_MAX - EFFECT_ROUTE_VEHICLE_EXHAUST_MIN + 1];
	static f32 sm_TimeToResetBulletCount;

	static audSound *sm_InCarViewWind;

	static CVehicle* sm_PlayerPedTargetVehicle;
	static audScene* sm_VehicleGeneralScene;
	static audScene* sm_VehicleInteriorScene;
	static audScene* sm_VehicleBonnetCamScene;
	static audScene* sm_VehicleFirstPersonTurretScene;
	static audScene* sm_StuntTunnelScene;
	static audScene* sm_OnMovingTrainScene;
	static audScene* sm_ExhaustProximityScene;
	static audScene* sm_HighSpeedScene;
	static audScene* sm_StuntRaceSpeedUpScene;
	static audScene* sm_StuntRaceSlowDownScene;
	static audVehicleType sm_StuntRaceVehicleType;
	static f32 sm_PlayerVehicleProximityRatio;	

	static u32 sm_ScriptVehicleRequest;
	static s32 sm_ScriptVehicleRequestScriptID;
	static audDynamicWaveSlot* sm_ScriptVehicleWaveSlot;

	static f32 sm_HighSpeedReqVehicleVelocity;
	static u32 sm_HighSpeedMaxGearOffset;
	static f32 sm_HighSpeedApplyStart;
	static f32 sm_HighSpeedApplyEnd;
	static u32 sm_InvHighSpeedSmootherIncreaseRate;
	static u32 sm_InvHighSpeedSmootherDecreaseRate;
	
	static u32 sm_AlarmIntervalInMs;

	static f32 sm_DownSpeedThreshold;
	static f32 sm_FwdSpeedThreshold;
	static f32 sm_SizeThreshold;
	static f32 sm_BigSizeThreshold;
	static f32 sm_CombineSpeedThreshold;
	static f32 sm_BicycleCombineSpeedMediumThreshold;
	static f32 sm_BicycleCombineSpeedBigThreshold;

	static u32 sm_TimeTargetLockStarted;
	static u32 sm_TimeMissleFiredStarted;
	static u32 sm_TimeAllClearStarted;
	static u32 sm_TimeAcquiringTargetStarted;
	static u32 sm_TimeATargetAcquiredStarted;

	static audSoundSet sm_VehiclesInWaterSoundset;
	static bool sm_IsSpectatorModeActive;
	static bool sm_TriggerStuntRaceSpeedBoostScene;
	static bool sm_TriggerStuntRaceSlowDownScene;
	static bool sm_IsInCarMeet;

#if __BANK

	static char sm_RecordingName[64];
	static f32 	sm_CRCurrentTime;
	static f32 	sm_CRProgress;
	static f32 	sm_CRJumpTimeInMs;
	static u32 sm_RecordingIndex;
	static bool sm_UpdateCRTime;
	static bool sm_EnableCREditor;
	static bool sm_ManualTimeSet;
	static bool sm_DrawCREvents;
	static bool sm_DrawCRName;
	static bool sm_PauseGame;
	static rage::bkGroup*	sm_CRWidgetGroup; 
#endif
};

#endif // AUD_VEHICLEAUDIOENTITY_H


