//
// audio/pedaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_PEDAUDIOENTITY_H
#define AUD_PEDAUDIOENTITY_H

#include "hashactiontable.h"
#include "gameobjects.h"
#include "pedfootstepaudio.h"
#include "Peds/PedDefines.h"
#include "Peds/PedMotionData.h"
#include "Peds/PedMoveBlend/PedMoveBlendTypes.h"
#include "peds/pedtype.h"
#include "animation/animbones.h"
#include "atl/bitset.h"
#include "scene/RegdRefTypes.h"
#include "parser/macros.h"		// PAR_SIMPLE_PARSABLE

#include "weapons/inventory/InventoryListener.h"

#include "audioentity.h"

#include "audioengine/soundset.h"
#include "audiosoundtypes/speechsound.h"

class CPed;
class CActionResult;
class CScenarioInfo;

class audStreamSlot;
class naEnvironmentGroup;
class audPedScenario;
struct audVehicleCollisionContext;
class audWeaponAudioComponent;

#define AUD_NUM_BIRD_TAKE_OFFS_ARRAY_SIZE (32)

typedef enum {
	AUD_BDIR_N = 0,
	AUD_BDIR_NE,
	AUD_BDIR_E,
	AUD_BDIR_SE,
	AUD_BDIR_S,
	AUD_BDIR_SW,
	AUD_BDIR_W,
	AUD_BDIR_NW,
	AUD_NUM_BDIR 
} audBirdDirection;

struct audBirdTakeOffStruct
{
	audBirdTakeOffStruct() :
		Bird(NULL),
		Direction(AUD_BDIR_N),
		Time(~0U),
		IsPigeon(false)
		{}

	RegdPed Bird;
	u32 Time;
	audBirdDirection Direction;
	bool IsPigeon;
};

class audWeaponInventoryListener : public CInventoryListener
{

public:

	void Init(CPed * ped)
	{
		m_Ped = ped;
	}

	virtual void ItemRemoved(u32 weaponHash);

	virtual void AllItemsRemoved();

private:
	CPed * m_Ped;

};

class audPedAudioEntity : public naAudioEntity
{

#if GTA_REPLAY
	friend class CPacketPedUpdate;
	friend class CPacketSoundCreate;
	friend class CPacketFootStepSound;
	friend class CPacketBreathSound;
#endif

public:
	AUDIO_ENTITY_NAME(audPedAudioEntity);

	audPedAudioEntity();
	virtual ~audPedAudioEntity();

	void Init(CPed *ped);
	void Init()
	{
		naAssertf(0, "Need to init audPedAudioEntity with a CPed");
	}
	void Resurrect();
	void InitOcclusion(bool forceUpdate = false);
	void PlayArrestingLoop();
	void StopArrestingLoop();
	void PlayCuffingSounds();
	void PlayMeleeCombatBikeSwipe();
	void PlayMeleeCombatSwipe(u32 meleeCombatObjectNameHash);
	void PlayMeleeCombatHit(u32 meleeCombatObjectNameHash, const WorldProbe::CShapeTestHitPoint& refResult, const CActionResult * actionResult
#if __BANK
		, const char* soundName
#endif  
		);
	void PlayBikeMeleeCombatHit(const WorldProbe::CShapeTestHitPoint& refResult);
	void TriggerMeleeResponse(u32 meleeCombatObjectNameHash,const WorldProbe::CShapeTestHitPoint& refResult,f32 damageDone);

	void HandleWeaponFire( bool isSilenced = false);

	virtual void UpdateSound(audSound* sound, audRequestedSettings *reqSets, u32 timeInMs);

	virtual void PreUpdateService(u32 timeInMs);
	virtual void PostUpdate();

	virtual CEntity* GetOwningEntity(){return (CEntity*)(m_Ped.Get());};
	virtual naEnvironmentGroup *GetEnvironmentGroup(bool create = false);
	
	void ResetEnvironmentGroupLifeTime();

	CPed* GetOwningPed() const {return m_Ped.Get();};

	static void InitMPSpecificSounds();
	static void InitClass();
	static void UpdateClass();
	static void ShutdownClass();

	inline void SetRingtone(audMetadataRef ringtoneSoundRef) { m_RingtoneSoundRef = ringtoneSoundRef; }
	void StartAmbientAnim(const crClip *anim);
	void StopAmbientAnim();
	void SetPedHasReleaseParachute()  { m_PedHasReleasedParachute = true;}

	// PURPOSE
	// Called in main game-thread update loop, too allow the audEntity to update its occlusion group(s).
//	virtual void UpdateOcclusionGroups(u32 timeInMs);

	// PURPOSE
	// populates the current occlusion metrics for the zone and/or sound specified - this allows individual audEntities
	// to use a range of mechanisms for allocating and managing zones.
	// PARAMS
	//  occlusionMetric - the audOcclusionMetric struct to populate.
	//  occlusionGroupIndex - audEntity-specific enum
	//  sound - ptr to the sound that wants to know the occlusion factor (or NULL, if not sound specific)
//	virtual void GetOcclusionMetrics(audOcclusionMetric* occlusionMetric, s16 occlusionGroupIndex, audSound* sound, u32 timeInMs);

#if __BANK
	static void AddWidgets(bkBank &bank);
#endif

	/*audRadioEmitter *GetRadioEmitter(void)
	{
		return &m_RadioEmitter;
	}*/

	// PURPOSE
	//	Called for each hash code in the audio anim track
	void HandleAnimEventFlag(u32 hash);
	void HandleAnimEventFlag(const char *name)
	{
		HandleAnimEventFlag(atStringHash(name));
	}

	static audHashActionTable GetPedAnimActionTable()
	{
		return sm_PedAnimActionTable;
	}

	void Update();

	void UpdateBreathing();
	void TriggerBreathSound(const f32 breathRate);

	void StartHeartbeat(bool fromSprinting = true);
	void StopHeartbeat();
	void UpdateHeartbeat();

	void PlayRingTone(s32 timeLimit = -1, bool triggerAsHudSound = false);
	void StopRingTone();
	bool IsRingTonePlaying();
	void PlayGarbledTwoWay();
	void StopGarbledTwoWay();
	bool IsGarbledTwoWayPlaying();
	bool IsFreeForAMobileCall();
	void StartedMobileCall();

	void TriggerShellCasing(const audWeaponAudioComponent &weaponComponent);

	void OnScenarioStarted(s32 scenarioType, const CScenarioInfo* scenarioInfo);
	void OnScenarioStopped(s32 scenarioType);
	
	bool IsInitialised() const
	{
		return (m_Ped != NULL);
	}

	const CVehicle* GetLastVehicle() const {return m_LastVehicle;};
	static CVehicle* GetBJVehicle();
	void SetCachedBJVehicle(CVehicle* vehicle); 

	void TriggerSplash(const f32 speedEvo);
	void TriggerBigSplash();

	virtual void ProcessAnimEvents();

	bool TriggerBoneBreak(const RagdollComponent comp);

	void TriggerFallToDeath();
	void PedFellToDeath();
	void TriggerImpactSounds(const Vector3 &pos, CollisionMaterialSettings *material, CEntity *otherEntity, f32 impulseMag, u32 pedComponent, u32 otherComponent, bool isMeleeImpact);
	void PlayVehicleImpact(const Vector3 &pos, CVehicle *veh, const audVehicleCollisionContext * context);
	void PlayUnderOverVehicleSounds(const Vector3 &pos, CVehicle *veh, const audVehicleCollisionContext * context);

	void AllocateVariableBlock();

	void UpBodyfallForShooting();

	bool ShouldUpBodyfallSounds(); 
	bool ShouldUpBodyfallSoundsFromShooting();

	void SetIsStaggering();
	void SetIsStaggerFalling();

#if __DEV
	static const char *GetEventName(const u32 hash)
	{
		return sm_PedAnimActionTable.FindActionName(hash);
	}
#endif

#if __BANK
	void DrawBoundingBox(const Color32 color);
#endif

	void SetWeaponAnimEvent(u32 eventHash);
	void WeaponThrow();

	audPedFootStepAudio &GetFootStepAudio() {return m_FootStepAudio;}
	const audPedFootStepAudio &GetFootStepAudio() const {return m_FootStepAudio;}

	//Conductors purpose.
	void						SetGunfightVolumeOffset(f32 volOffset) {m_GunfightVolOffset = volOffset;};
	void						SetWasInCombat(bool wasInCombat){ m_WasInCombat = wasInCombat;}
	void						SetWasTarget(bool wasTarget){ m_WasTarget = wasTarget;}
	f32							GetGunfightVolumeOffset() {return m_GunfightVolOffset;};
	bool						GetWasTarget() {return m_WasTarget;}
	bool						GetWasInCombat() {return m_WasInCombat;}

	void						SetHasPlayedKillShotAudio(bool playedKillShotAudio) {m_PlayedKillShotAudio = playedKillShotAudio;}
	bool						GetHasPlayedKillShotAudio() {return m_PlayedKillShotAudio;}


	void SetWasFiringForReplay(bool wasFiring) { m_PedWasFiringForReplay = wasFiring; }
	bool GetWasFiringforReplay() { return m_PedWasFiringForReplay; }

	void SetTriggerAutoWeaponStopForReplay(bool trigger)	{ m_PedTriggerAutoFireStopForReplay = trigger;}
	bool GetTriggerAutoWeaponStopForReplay() { return m_PedTriggerAutoFireStopForReplay; }

	
	const CPedModelInfo*				GetPedModelInfo();
	ePedType GetPedType();
	bool GetIsInWater();
	bool GetIsUnderCover(){return m_PedUnderCover;};

	bool GetIsWhistling() { return m_PlayerWhistleSound != NULL; }

	void IsBeingElectrocuted(bool isElectocuted);

	void RegisterBirdTakingOff();
	void PlayBirdTakingOffSound(bool playPigeon);

	void OnIncendiaryAmmoFireStarted();

	//PURPOSE
	//Supports audDynamicEntitySound queries
	u32 QuerySoundNameFromObjectAndField(const u32 *objectRefs, u32 numRefs);

	virtual Vec3V_Out GetPosition() const;
	virtual audCompressedQuat GetOrientation() const;

	virtual void AddAnimEvent(const audAnimEvent & event);

	static audSoundSet * GetPedCollisionSoundset() { return &sm_PedCollisionSoundSet; }

	CollisionMaterialSettings* GetScrapeMaterialSettings();

	void NotifyVehicleContact(CVehicle *vehicle);

	void HandleCarWasJacked();

	void SetLastScrapeTime();
	u32 GetLastScrapeTime()
	{
		return m_LastScrapeTime;
	}

	void TriggerScrapeStartImpact(f32 volume);

	bool GetWasInWater()
	{
		return m_WasInWater;
	}

	void PlayScubaBreath(bool inhale);
	bool IsScubaSoundPlaying() { return m_ScubaSound != NULL; }
	void StopScubaSound() { if(m_ScubaSound) m_ScubaSound->StopAndForget(); }
	f32 GetPlayerImpactMagOverride(f32 impactMag, CEntity * otherEntity, CollisionMaterialSettings * otherMaterial);
	void HandlePreComputeContact(phContactIterator &impacts);

	void HandleJumpingImpactRagdoll();
	bool HasBeenOnTrainRecently();

	void TriggerRappelLand();

	void SetLastStickyBombTime(u32 time) { m_LastStickyBombTime = time; }
	u32 GetLastStickyBombTime() { return m_LastStickyBombTime; }

#if GTA_REPLAY
	void SetForceUpdateVariablesForReplay()	{	m_forceUpdateVariablesForReplay = true;	}
	void ReplaySetRingTone(u32 soundSetHash, u32 soundHash) { m_RingtoneSoundSetHash = soundSetHash; m_RingtoneSoundHash = soundHash; }
#endif // GTA_REPLAY

	static float GetPedWalkingImpactFactor() { return sm_PedWalkingImpactFactor; }
	static float GetPedRunningImpactFactor() { return sm_PedRunningImpactFactor; }
	static bool GetShouldDuckRadioForPlayerRingtone(); 

private:
	void HandleCarJackImpact(const Vector3 &pos);

	void UpdateRappel();
	void SelectRingTone();
	void PlayRingTone_Internal();
	void StopRingTone_Internal();

	void ProcessParachuteEvents();
#if GTA_REPLAY	
	bool ReplayIsParachuting();
	u8 ReplayGetParachutingState();

	bool m_forceUpdateVariablesForReplay;
	u32 m_RingtoneSoundSetHash;
	u32 m_RingtoneSoundHash;
	u32 m_RingToneSoundStartTime;
#endif //GTA_REPLAy

	void TriggerBreathSound();

	void PlaySlowMoHeartSound();
	void UpdateSlowMoHeartBeatSound();
	void StopSlowMoHeartSound();

	void UpdateEquippedMolotov();
	void UpdateMovingWindSounds();

	static u32 ValidateMeleeWeapon(const u32 hash, CPed* ped);

	static void DefaultPedActionHandler(u32 hash, void *context);

	static void CoverHitWallHandler(u32 hash, void *context);
	static void MeleeOverrideHandler(u32 hash, void *context);
	static void MeleeHandler(u32 hash, void *context);
	static void MeleeSwingHandler(u32 hash, void *context);

	static void PedToolImpactHandler(u32 hash, void *context);

	static void GunHeftHandler(u32 hash, void *context);
	static void PutDownGunHandler(u32 hash, void *context);

	static void HeadHitSteeringWheelHandler(u32 hash, void *context);

	static void BlipThrottleHandler(u32 hash, void *context);
	static void ConversationTriggerHandler(u32 hash, void *context);
	static void StopSpeechHandler(u32 hash, void *context);
	static void SpaceRangerSpeechHandler(u32 hash, void *context);
	static void WeaponFireHandler(u32 hash, void*context);
	static void WeaponThrowHandler(u32 hash, void*context);
	
	static void UpdateMichaelSpeacialAbilityForPeds();

	// Speech contexts triggered by anims - each has a custom handler
	static void SayFightWatchHandler(u32 hash, void *context);
	static void SaySteppedInSomethingHandler(u32 hash, void *context);
	static void SayWalkieTalkieHandler(u32 hash, void *context);
	static void SayStripClubHandler(u32 hash, void *context);
	static void SayDrinkHandler(u32 hash, void *context);
	static void SayGruntClimbEasyUpHandler(u32 hash, void *context);
	static void SayGruntClimbHardUpHandler(u32 hash, void *context);
	static void SayGruntClimbOverHandler(u32 hash, void *context);
	static void SayInhaleHandler(u32 hash, void *context);
	static void SayJackingHandler(u32 hash, void *context);
	static void SayDyingMoanHandler(u32 hash, void *context);

	// custom player whistle

	static void PlayerWhistleForCabHandler(u32 hash, void *context);
	static void PlayerWhistleHandler(u32 hash, void *context);
	// and pain ones
	static void PainLowHandler(u32 hash, void *context);
	static void PainHighHandler(u32 hash, void *context);
	static void PainMediumHandler(u32 hash, void *context);
	static void DeathHighShortHandler(u32 hash, void *context);
	static void DeathLowHandler(u32 hash, void *context);
	static void PedImpactHandler(u32 hash, void * context);
	static void PainShoveHandler(u32 hash, void *context);
	static void PainWheezeHandler(u32 hash, void *context);
	static void ScreamShockedHandler(u32 hash, void *context);
	static void DeathGargleHandler(u32 hash, void *context);
	static void DeathHighLongHandler(u32 hash, void *context);
	static void DeathHighMediumHandler(u32 hash, void *context);
	static void PainGargleHandler(u32 hash, void *context);
	static void ClimbLargeHandler(u32 hash, void *context);
	static void ClimbSmallHandler(u32 hash, void *context);
	static void JumpHandler(u32 hash, void *context);
	static void CowerHandler(u32 hash, void *context);
	static void WhimperHandler(u32 hash, void *context);
	static void HighFallHandler(u32 hash, void *context);
	static void VocalExerciseHandler(u32 hash, void *context);
	static void ExhaleCyclingHandler(u32 hash, void *context);
	static void CoughHandler(u32 hash, void *context);
	static void SneezeHandler(u32 hash, void *context);
	static void PainLowFrontendHandler(u32 hash, void *context);
	static void PainHighFrontendHandler(u32 hash, void *context);
	static void SayLaughHandler(u32 hash, void *context);
	static void SayCryHandler(u32 hash, void *context);
	static void MeleeSmallGrunt(u32 hash, void *context);
	static void MeleeLargeGrunt(u32 hash, void *context);

	static void CowMooHandler(u32 hash, void*context);
	static void CowMooRandomHandler(u32 hash, void*context);
	static void RetrieverBarkHandler(u32 hash, void*context);
	static void RetrieverBarkRandomHandler(u32 hash, void*context);
	static void RottweilerBarkHandler(u32 hash, void*context);
	static void RottweilerBarkRandomHandler(u32 hash, void*context);

	static void AnimPedBurgerEatHandler(u32 hash, void *context);

	static void TennisLiteHandler(u32 hash, void *context);
	static void TennisMedHandler(u32 hash, void *context);
	static void TennisStrongHandler(u32 hash, void *context);
	static void InitTennisVocalizationRaveSettings();

	static void UpdateBirdTakingOffSounds(u32 timeInMs);

	void PlayAnimTriggeredSound(u32 soundHash);
	void PlayAnimTriggeredPlayerWhistle(u32 soundHash);
	void StartElectrocutionSound();

	void UpdateStaggering();

	void UpdatePedVariables();
	void UpdateWeaponHum();

	Vector3 m_PreviousStrikeBonePosition;

#if GTA_REPLAY
	void SetReplayVelocity(const Vector3 &inVel) { m_ReplayVel = inVel; }
	const Vector3& GetReplayVelocity() { return m_ReplayVel; }
	Vector3 m_ReplayVel;
#endif

	audPedFootStepAudio m_FootStepAudio;

	Vec3V m_LeftHandCachedPos;
	Vec3V m_RightHandCachedPos;

	audSpeechSound* m_RingToneSound;
	audSound* m_GarbledTwoWaySound;
	audSound *m_MolotovFuseSound;
	audSound *m_TrouserSwishSound;
	audSound *m_ArrestingLoop;
	audSound *m_AmbientAnimLoop;
	audSound *m_ParachuteRainSound;
	audSound *m_ParachuteSound;
	audSound *m_ParachuteDeploySound;
	audSound *m_FreeFallSound;
	audSound *m_HeartBeatSound;
	audSound *m_MobileRingSound;
	audSound* m_PlayerWhistleSound;
	audSound* m_ElectrocutionSound;
	audSound* m_MovingWindSound;
	audSound* m_ScubaSound;
	audSound* m_LockedOnSound;
	audSound* m_AcquringLockSound;
	audSound* m_NotLockedOnSound;
	audSound* m_RailgunHumSound;
	audSound* m_RappelLoopSound;

	audPedScenario* m_ActiveScenario;

	RegdVeh m_LastVehicle;

	f32 m_GunfightVolOffset;
	f32	m_BreathRate;
	f32 m_LastImpactMag;
	u32 m_LastScrapeTime;
	u32 m_LastRappelStopTime;

	s32 m_PrevScenario;
	s32 m_RingtoneSlotID;
	s32 m_RingToneTimeLimit;
	s32 m_RingToneLoopLimit;
	s32 m_PlayerMixGroupIndex;

	eAnimBoneTag m_MeleeStrikeBoneTag;

	u32	m_LastTimeShellCasingPlayed;
	u32	m_LastWaterSplashTime;
	u32 m_ParachuteState;
	u32 m_TimeLastStartedMobileCall;
	audMetadataRef m_RingtoneSoundRef;
	u32 m_LastBigSplashTime;
	u32 m_BoneBreakState;
	u32 m_LastCarImpactTime; //uses sound timer 0
	u32 m_LastCarOverUnderTime; //uses sound timer 0
	u32 m_LastBulletImpactTime; //uses sound timer 0
	u32 m_LastImpactTime; //uses sound timer 0
	u32 m_LastHeadImpactTime; //uses sound timer 0
	u32 m_LastMeleeImpactTime; //uses sound timer 0
	u32 m_LastFallToDeathTime; //uses sound timer 0
	u32 m_LastBigdeathImpactTime; //uses sound timer 0
	u32 m_LastTimeInVehicle; //uses sound time 0
	u32 m_LastTimeOnGround; 
	
	u32 m_HeartbeatTimeSprinting;
	u32 m_HeartbeatTimeNotSprinting;
	u32 m_LastTimeHurtFromSprinting;
	u32 m_TimeOfLastPainBreath;
	u32 m_WeaponAnimEvent;
	u32 m_TimeElectrocutionLastSet;
	u32	m_LastTimeValidGroundWasTrain;
	u32 m_LastScrapeDebrisTime;
	u32 m_LastStickyBombTime;

	float m_EntityRandomSeed;

	audSimpleSmoother m_MolotovVelocitySmoother;

	audWeaponInventoryListener m_WeaponInventoryListener;

	RegdPed m_Ped;

	audScene * m_ParachuteScene, *m_SkyDiveScene;

	RegdVeh m_LastVehicleContact;

	RegdEnt m_LastImpactEntity;
	u32 m_LastEntityImpactTime;

	u32 m_LastVehicleContactTime;
	u32 m_LastCarJackTime;

	RegdVeh m_BJVehicle;

	s32 m_EnvironmentGroupLifeTime;

	u32 m_LastMeleeHitTime;
	u32 m_RailgunHumSoundHash;
	u32 m_LastMeleeMaterial; //Pointer value of the last material a melee impact occurred with.
	u32 m_LastMeleeMaterialImpactTime; //Last time we played a material impact sound

	bool m_LastMeleeCollisionWasPed;
	bool m_IsMeleeStrikeBoneALeg;
	bool m_ShouldPlayRingTone;
	bool m_TriggerRingtoneAsHUDSound;
	bool m_WaitingForRingToneSlot;
	bool m_IsDLCRingtone;	//may as well set this up...I'm guessing we'll use it
	bool m_WasInVehicle;
	bool m_WasInVehicleAndNowInAir;
	bool m_HasToTriggerBreathSound;
	bool m_WasInWater; 
	bool m_LastPainBreathWasInhale;
	bool m_WasInCombat:1;
	bool m_WasTarget:1;
	bool m_HasPlayedDeathImpact:1;
	bool m_HasPlayedRunOver:1;
	bool m_HasPlayedBigAnimalImpact:1;
	bool m_IsBeingElectrocuted:1;
	bool m_IsStaggering:1;
	bool m_IsStaggerFalling:1;
	bool m_HasPlayedInitialStaggerSounds:1;
	bool m_HasPlayedSecondStaggerSound:1;
	bool m_HasJustBeenCarJacked:1;
	bool m_PedHasReleasedParachute:1; //used to turn on falling wind sound
	bool m_NeedsToDoScrapeEnd:1;
	bool m_ShouldUpdateScubaBubblePtFX:1;
	bool m_PedUnderCover:1;
	bool m_PlayedKillShotAudio:1;
	bool m_PedWasFiringForReplay:1;
	bool m_PedTriggerAutoFireStopForReplay:1;

	REPLAY_ONLY(bool m_RailgunHumSoundRecorded;);

	static audWaveSlot*				sm_ScriptMissionSlot;

	static RegdPed					sm_LastTarget;

	static audSoundSet				sm_KnuckleOverrideSoundSet;
	static audSoundSet				sm_PedCollisionSoundSet;
	static audSoundSet				sm_PedVehicleCollisionSoundSet;

	static audSoundSet				sm_BikeMeleeHitPedSounds;
	static audSoundSet				sm_BikeMeleeHitSounds;
	static audSoundSet				sm_BikeMeleeSwipeSounds;

	static audCurve sm_LinearVelToCreakVolumeCurve;
	static audCurve sm_MeleeDamageToClothVol;
	static audCurve sm_MolotovFuseVolCurve, sm_MolotovFusePitchCurve;

	static f32 sm_PedHighLODDistanceThresholdSqd;
	static f32 sm_SmallAnimalThreshold;
	static f32 sm_SqdDistanceThresholdToTriggerShellCasing;

	static f32 sm_ImpactMagForShotFallBoneBreak;
	static u32 sm_AirTimeForLiveBigimpacts;
	static f32 sm_MaxMotorbikeCollisionVelocity;
	static f32 sm_MaxBicycleCollisionVelocity;
	static f32 sm_MinHeightForWindNoise;
	static f32 sm_HighVelForWindNoise;
	static f32	sm_InnerUpdateFrames, sm_OuterUpdateFrames, sm_InnerCheapUpdateFrames, sm_OuterCheapUpdateFrames;
	static f32  sm_CheapDistance;

	static audHashActionTable		sm_PedAnimActionTable;
	static u32						sm_MeleeOverrideSoundNameHash;
	static u32						sm_TimeThresholdToTriggerShellCasing;

	static u32						sm_LifeTimePedEnvGroup;

	static u16 sm_LodAnimLoopTimeout; 

	static bool						sm_ShouldShowPlayerMaterial;


	BANK_ONLY(static bool sm_ShowMeleeDebugInfo);

	static u32		sm_PedImpactTimeFilter;

	static u32 sm_BirdTakeOffWriteIndex;
	static u32 sm_TimeOfLastBirdTakeOff;
	static atRangeArray<audBirdTakeOffStruct, AUD_NUM_BIRD_TAKE_OFFS_ARRAY_SIZE> sm_BirdTakeOffArray;

	static float sm_PedWalkingImpactFactor;
	static float sm_PedRunningImpactFactor;
	static float sm_PedLightImpactFactor;
	static float sm_PedMediumImpactFactor;
	static float sm_PedHeavyImpactFactor;
	static u32 sm_EntityImpactTimeFilter;
	static u32 sm_LastTimePlayerRingtoneWasPlaying;
	static float sm_VehicleImpactForHeadphonePulse;
};

#endif // AUD_PEDAUDIOENTITY_H
