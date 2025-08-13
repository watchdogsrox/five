//
// audio/pedaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_PEDFOOTSTEPAUDIO_H
#define AUD_PEDFOOTSTEPAUDIO_H

#include "Peds/PedFootstepHelper.h"
#include "scene/RegdRefTypes.h"

#include "audioengine/soundset.h"

#include "dynamicmixer.h"
class CPed;
class audPedAudioEntity;

#define MAX_NUM_SLOPE_DEBRIS 8
#define MAX_EXTRA_CLOTHING 4

enum audFootWaterInfo
{
	audFootOutOfWater,
	audFootUnderWater,
	audLegsUnderWater,
	audBodyUnderWater,
};

struct audFootstepEventMinMax
{
	float m_MinDist;
	float m_MaxDist;

	PAR_SIMPLE_PARSABLE;
};

/*
PURPOSE
	Tuning data for AI/blip noise event ranges.
*/
struct audFootstepEventTuning
{
public:
	enum
	{
		kFootstepWalkCrouch,
		kFootstepWalkStand,
		kFootstepRunCrouch,
		kFootstepRunStand,
		kFootstepSprint,

		kNumFootstepMinMax
	};

#if __BANK
	static void AddWidgets(bkBank& bank, atRangeArray<audFootstepEventMinMax, kNumFootstepMinMax>& rangeArray);
#endif	// __BANK

	atRangeArray<audFootstepEventMinMax, kNumFootstepMinMax>	m_SinglePlayer;
	atRangeArray<audFootstepEventMinMax, kNumFootstepMinMax>	m_MultiPlayer;

	PAR_SIMPLE_PARSABLE;
};

struct downSlopEvent
{
	Vec3V downSlopeDirection;
	audSound *debrisSound;
	f32 slopeAngle;
	audFootstepEvent stepType;
	bool ready;
};

struct foliageEvent
{
	audFootstepEvent stepType;
	u32 typeNameHash;
};

class audPedFootStepAudio
{

#if GTA_REPLAY
	friend class CPacketPedUpdate;
	friend class CPacketSoundCreate;
	friend class CPacketFootStepSound;
	friend class CPacketBreathSound;
#endif

public:

	audPedFootStepAudio();
	~audPedFootStepAudio();

	void 						AddBushEvent(audFootstepEvent stepType, u32 typeNameHash = g_NullSoundHash);
	void 						AddClothEvent(audFootstepEvent stepType);
	void 						AddFootstepEvent(audFootstepEvent stepType);
	void 						AddPetrolCanEvent(audFootstepEvent stepType,bool replay = false);
	void						AddMolotovEvent(audFootstepEvent stepType,bool replay = false);
	void 						AddScriptSweetenerEvent(audFootstepEvent stepType);
	void 						AddSlopeDebrisEvent(audFootstepEvent stepType, Vec3V_In downSlopeDirection,f32 slopeAngle);
	void 						AddWaterEvent(audFootstepEvent stepType);
	void 						Init(audPedAudioEntity *pedAudioEntity);
	void 						InitModelSpecifics();
	void						DoFootstepStealth();
	void 						PlayCoverHitWallSounds(u32 hash);
	void 						PlayFootstepEvent(audFootstepEvent event, bool playWhenNotStanding = false);
	void 						PlayPedOnPedBumpSounds(CPed* otherPed, f32 moveBlendRatio);
	void 						PreUpdate();
	void 						ProcessEvents();
	void						ResetScriptSweetenerSoundSet();
	void 						ResetLastMoveBlendRatio(){m_LastMoveBlendRatio = 0.f;}
	void 						ResetWetFeet(bool forceReset = false);
	void						SetScriptSweetenerSoundSet(const u32 soundsetHash);
	void 						SetCurrentMaterialSettings(CollisionMaterialSettings * val);
	void						SetFootstepTuningMode(bool scriptOverrides,const u32 modeHash = 0);
	void						SetMoveBlendRatioWhenGoingIntoCover(f32 moveBlendRatio){m_MoveBlendRatioWhenGoingIntoCover = moveBlendRatio;}
	inline bool					HasStandingMaterialChanged(phMaterialMgr::Id materialId, CEntity *entity, u32 component) const;
	void 						SetStandingMaterial(phMaterialMgr::Id materialId, CEntity *entity, u32 component);
	void 						ChangeStandingMaterial(phMaterialMgr::Id materialId, CEntity *entity, u32 component);
	void 						SetStandingMaterial(CollisionMaterialSettings *material, CEntity *entity, u32 component);
	void						TriggerFallInWaterSplash(bool collisionEvent = false);
	void 						TriggerWaveImpact();
	void 						Update();
	void 						StartLadderSlide();
	void 						StopLadderSlide();
	void						CleanWaterFlags() {m_ShouldStopWaveImpact = true;};
	void						ShouldTriggerFallInWaterSplash() {m_ShouldTriggerSplash = true;};
	audSoundSet&				GetMolotovSounds() { return sm_MolotovSounds; }	

	void						SetFootstepEventsEnabled(bool enabled) { m_FootstepEventsEnabled = enabled; }
	void						SetClothEventsEnabled(bool enabled) { m_ClothEventsEnabled = enabled; }
	void						OverridePlayerMaterial(const u32 nameHash) { sm_OverriddenPlayerMaterialName = nameHash;};

	void						PlayWindClothSound();
	void						StopWindClothSound();
	// grab all variation data - only happens when normal peds are spawned, done every frame atm
	// for the player
	void 						UpdateVariationData(ePedVarComp pedVarComp);
	void 						UpdateWetFootInfo(eAnimBoneTag boneTag);

	const ClothAudioSettings*	GetCachedUpperBodyClothSounds()const {return m_UpperClothSettings;};
	const ClothAudioSettings*	GetCachedLowerBodyClothSounds()const {return m_LowerClothSettings;};
	const ClothAudioSettings*	GetCachedExtraClothSounds(const u8 idx) const ;

	phMaterialMgr::Id			GetCurrentPackedMaterialId(){return m_LastMaterial;}
	CollisionMaterialSettings*	GetCurrentMaterialSettings();
	ShoeAudioSettings*			GetCachedShoeSettings()const {return m_ShoeSettings;};
	ShoeTypes					GetShoeType();

	f32							GetDragScuffProbability();
	f32							GetPlayerClumsiness();

	bool						ShouldScriptAddSweetenersEvents()const {return m_ScriptFootstepSweetenersSoundSet.IsInitialised();};
	bool 						DoesPedHavePockets() const;
	bool						IsWearingKifflomSuit();
	bool						IsWearingClothes();
	bool						IsWearingScubaSuit();
	bool						IsWearingScubaMask();
	bool						IsWearingBikini();
	bool						IsWearingHeels();
	bool						IsWearingHighHeels();
	bool						IsWearingFlapsInWindClothes();
	bool						GetIsStandingOnVehicle();
	bool						GetFlapsInWind();
	bool						GetAreFeetInWater() {return m_FeetInWater;};
	void						SetForceWearingScubaMask(bool force) { m_ForceWearingScubaMask = force; }
	CEntity*					GetLastStandingEntity() {return m_LastStandingEntity;};

	// PURPOSE:	Bring variables most commonly used by SetStandingMaterial() into the data cache.
	void PrefetchLastMaterial()	{ PrefetchDC(&m_LastMaterial); }

	static void 				InitClass();
	static void 				UpdateClass();
	static void 				ShutdownClass();
	// Anim tag handlers
	static void 				LeftLadderFoot(u32 hash, void *context);
	static void 				LeftLadderFootUp(u32 hash, void *context);
	static void 				RightLadderFoot(u32 hash, void *context);
	static void 				RightLadderFootUp(u32 hash, void *context);
	static void 				LeftLadderHand(u32 hash, void *context);
	static void 				RightLadderHand(u32 hash, void *context);
	static void 				FootStepHandler(u32 hash, void *context);
	static void					WallClimbHandler(u32 hash, void *context);
	static void					BonnetSlideHandler(u32 hash, void *context);
	static void					PedRollHandler(u32 hash, void *context);
	static void					PedFwdRollHandler(u32 hash, void *context);
	static bool					GetFootStepEventFromHash(const u32 &hash,audFootstepEvent &event);
	ShoeAudioSettings*			GetShoeSettings()const;

#if __BANK
	static void					AddMaterialCollisionWidgets();
	static void					AddWidgets(bkBank &bank);

	static void					PlayDelayDebugFootstepEvent(const Vector3 &pos,phMaterialMgr::Id materialId,const u16 hitComponent,CEntity* hitEntity,CollisionMaterialSettings* material);
	static void					PlayDebugFootstepEvent(const Vector3 &pos,phMaterialMgr::Id materialId,const u16 hitComponent,const CEntity* hitEntity,const CollisionMaterialSettings* material);

	static bool 				sm_bMouseFootstep;
	static bool 				sm_bMouseFootstepBullet;
#endif

private:

	void 						SetStandingMaterial(CollisionMaterialSettings *material, phMaterialMgr::Id matId);
	void						GetShoeSoundRefsAndOffsets(audFootstepEvent event,u32 &soundRef,u32 &dirtySoundRef,u32 &creakySoundRef,u32 &glassySoundRef,u32 &wetSoundRef,audMetadataRef &underRainSoundRef,f32 &volOffset,f32 &volCurveScale);
	void						GetCustomMaterialSoundRefsAndOffsets(audFootstepEvent event,audMetadataRef &soundRef,audMetadataRef &wetSoundRef,audMetadataRef &deepSoundRef,f32 &npcRunRollOffScale,f32 &jumpLandVolOffset);
	void 						GetVolumeAndRelFreqForFootstep(f32 &vol, f32 &freq);
	void 						PlayAnimalFootStepsEvent(audFootstepEvent event, bool playWhenNotStanding = false);
	void 						PlayFoliageEvent(foliageEvent event,const f32 angularVelocity); 
	void 						PlayClothEvent(audFootstepEvent event,const f32 angularVelocity); 
	void 						PlayClothEvent(audFootstepEvent event,const ClothAudioSettings* settings,const f32 angularVelocity); 
	void 						PlayFootStepCustomImpactSound(audFootstepEvent event, const Vector3 &pos,u8 playToe,f32 surfaceDepthVol);
	void 						PlayFootStepMaterialImpact(const Vector3 &pos,audFootstepEvent event,f32 surfaceDepthScale);
	void 						PlayPetrolCanEvent(audFootstepEvent event,f32 petrolLevel); 
	void						PlayScriptSweetenerEvent(audFootstepEvent event);
	void						PlayMolotovEvent(audFootstepEvent event);
	void 						PlayShoeSound(audFootstepEvent event,const Vector3 &pos,u8 playToe,f32 surfaceDepth);
	void 						PlayWaterEvent(audFootstepEvent event); 
	void 						PlayDirtLayer(const u32 soundRef, f32 volume,f32 volCurveScale,u8 playToe,audFootstepEvent event,const Vector3 &pos);
	void 						PlayCreakLayer(const u32 soundRef, f32 volume,f32 volCurveScale,audFootstepEvent event,const Vector3 &pos);
	void 						PlayGlassLayer(const u32 soundRef, f32 volume,f32 volCurveScale,u8 playToe,audFootstepEvent event,const Vector3 &pos);
	void 						PlayWetLayer(const u32 soundRef,const audMetadataRef underRainSoundRef, f32 volume,f32 volCurveScale,u8 playToe,audFootstepEvent event,const Vector3 &pos);
	void 						ProcessBushEvents(const f32 angularVelocity);
	void 						ProcessClothEvents(const f32 angularVelocity);
	void 						ProcessFootsteps();
	void 						ProcessPetrolCanEvents(); 
	void						ProcessScriptSweetenerEvents();
	void						ProcessMolotovEvents();
	void 						ProcessSlopeDebrisEvents();
	void 						ProcessWaterEvents();
	void 						ResetEvents(bool isAnAnimal);
	void 						TriggerAudioAIEvent();
	void 						TriggerBigSplash();
	void 						TriggerSplash(const f32 speedEvo);
	void						UpdateFootstetVolOffsetOverTime();
	void						UpdatePedPockets();
	void 						UpdateWetness();

	AnimalFootstepSettings*		GetAnimalFootstepSettings(bool overrideAnimalRef = false, audMetadataRef overridenAnimalRef = g_NullSoundRef);
	const ClothAudioSettings*	GetBodyClothSounds(atHashString nameHash,bool extra = false)const;
	const ClothAudioSettings*	GetLowerBodyClothSounds()const;
	const ClothAudioSettings*	GetUpperBodyClothSounds()const;
	const ClothAudioSettings*	GetExtraClothSounds(const u8 idx) const;

	bool						ShouldForceSnow();
	f32 						ComputeSkidAmount();
	f32 						GetGlassiness(f32 glassLevelReduction = 0.0f);
	f32 						GetShoeDirtiness();
	f32 						CalculateShoeDirtiness();
	f32 						GetShoeCreakiness(audFootstepEvent event);
	f32 						GetWetness(audFootstepEvent event);

	u32 						GetAnimalFootstepSoundRef(const AnimalFootstepSettings *sounds, const audFootstepEvent event) const;
	u32 						GetShoeSound(u32 index, OldShoeSettings *sounds) const;
	u8							GetFootstepTuningValues(u32 modeHash = 0);

	static void 				LoadFootstepEventTuning();
#if __BANK
	static void 				AddLowerClothWidgets();
	static void 				AddExtraClothWidgets();
	static void 				AddShoeTypeWidgets();
	//static void 				AddStealthSceneModes();
	static void 				AddUpperClothWidgets();
	static void 				SaveFootstepEventTuning();

	static const char*			GetStepTypeName(audFootstepEvent stepType) ;

#endif

	atRangeArray<downSlopEvent, MAX_NUM_SLOPE_DEBRIS>	 	m_SlopeDebrisEvents;
	atRangeArray<ClothAudioSettings*,MAX_EXTRA_CLOTHING> 	m_ExtraClothingSettings;
	atRangeArray<atHashString,MAX_EXTRA_CLOTHING> 		 	m_ExtraClothing;
	atFixedArray<foliageEvent, MAX_NUM_FEET> 				m_FoliageEvents;
	atFixedArray<audFootstepEvent, BIPED_NUM_FEET> 			m_ClothsEvents;// If we want to play some cloth sound on animals set the size to MAX_NUM_FEET
	atFixedArray<audFootstepEvent, MAX_NUM_FEET> 			m_FootstepEvents;
	atFixedArray<audFootstepEvent, BIPED_NUM_FEET> 			m_PetrolCanEvents;
	atFixedArray<audFootstepEvent, BIPED_NUM_FEET> 			m_MolotovEvents;
	atFixedArray<audFootstepEvent, BIPED_NUM_FEET> 			m_ScriptSweetenerEvent;
	atFixedArray<audFootstepEvent, MAX_NUM_FEET>			m_WaterEvents;

	atRangeArray<audFootWaterInfo, MAX_NUM_FEET>			m_FootWaterInfo;	
	atRangeArray<f32, MAX_NUM_FEET>							m_FootWetness;	

	audPedAudioEntity*										m_Parent;
	RegdPed													m_ParentOwningEntity;

	ModelFootStepTuning*									m_FootstepsTuningValues;
	CollisionMaterialSettings*								m_CurrentMaterialSettings;
	ShoeAudioSettings*										m_ShoeSettings;
	ClothAudioSettings* 									m_LowerClothSettings;
	ClothAudioSettings* 									m_UpperClothSettings;
	//ClothAudioSettings* 									m_ExtraClothingSettings;

	audSound*												m_WaveImpactSound;
	audSound*												m_PetrolCanGlugSound;
	audSound*												m_PetrolCanPouringSound;
	audSound*												m_LadderSlideSound;
	audSound*												m_ClothWindSound;

	RegdEnt													m_LastStandingEntity;

	audSoundSet												m_ScriptFootstepSweetenersSoundSet;

	atHashString 											m_ShoeType;
	atHashString 											m_LowerClothing;
	atHashString 											m_UpperClothing;
	//atHashString 											m_ExtraClothing;

	f32														m_FootstepLoudness;
	f32														m_FootstepPitchRatio;
	f32														m_FootVolOffsetOverTime;
	f32 													m_FootstepImpsMagOffsetOverTime;
	f32 													m_MoveBlendRatioWhenGoingIntoCover;
	f32														m_LastMoveBlendRatio;
	f32														m_ShoeDirtiness;
	f32														m_PlayerStealth;
	f32														m_LastPedAngVel;

	s32														m_bLastMoveType; 

	u32														m_LastTimePlayedFPShuffle;
	audFootstepEvent										m_LastFPFootstepEvent;			

	// For performance reasons, keep these variables together - PrefetchLastMaterial()
	// tries to bring them into the data cache to make CPed::ProcessPreComputeImpacts() fast.
	phMaterialMgr::Id										m_LastMaterial;	
	u32														m_LastComponent;	

	u32														m_LastSplashTime;
	u32														m_ScriptTuningValuesHash;
	u32														m_LastCombatRollTime;
	u32														m_ModeHash;
	u8														m_Pockets;
	u8														m_ModeIndex;

	bool													m_FootstepEventsEnabled;
	bool													m_ClothEventsEnabled;
	bool													m_ForceWetFeetReset;
	bool													m_MaterialOverriddenThisFrame;
	bool													m_IsWearingElfHat;
	bool													m_IsWearingScubaMask;
	bool													m_ForceWearingScubaMask;
	bool													m_ShouldStopWaveImpact;
	bool													m_HasInitialisedPockets;
	bool 													m_HasToPlayPetrolCanStop;
	bool 													m_HasToPlayMolotovStop;
	bool 													m_WasInStealthMode;
	bool 													m_ScriptOverridesMode;
	bool 													m_ClumsyStep;
	bool													m_WasAiming;
	bool													m_FeetInWater; 
	bool													m_FeetInWaterUpdatedThisFrame; 
	bool													m_BonnetSlideRecently; 
	bool													m_ShouldTriggerSplash;
	bool													m_HasToTriggerSuperJumpLaunch;
	u8														m_GoodStealthIndex;
	u8														m_BadStealthIndex;

	static u32												sm_OverriddenPlayerMaterialName;

	static audScene*										sm_CrouchModeScene;
	static MovementShoeSettings*							sm_LongGrassFootsepSounds;

	static audSoundSet										sm_PetrolCanSounds;
	static audSoundSet										sm_MolotovSounds;
	static audSoundSet										sm_PocketsSoundSet;
	static audSoundSet										sm_PedCollisionSoundSet;
	static audSoundSet										sm_PedInsideWaterSoundSet;
	static audSoundSet										sm_FootstepsUnderRainSoundSet;

	static audCurve											sm_ScriptStealthToAudioStealth;
	static audCurve											sm_PlayerStealthToToeDelayOffset;
	static audCurve											sm_PlayerClumsinessToMatSweetenerProb;
	static audCurve											sm_PlayerClumsinessToMatSweetenerVol;
	static audCurve											sm_PlayerStealthToClumsiness;
	static audCurve 										sm_AngularAccToVolumeCurve;
	static audCurve											sm_ImpactVelocityToIntensity;
	static audCurve 										sm_LinearVelToCreakVolumeCurve;
	static audCurve 										sm_LinearVelToVolumeCurve;
	static audCurve											sm_ShoeWetnessToCreakiness;
	static audCurve 										sm_SlopeAngleToFallSpeed;
	static audCurve 										sm_SlopeAngleToVolume;
	static audCurve 										sm_EqualPowerFallCrossFade;
	static audCurve 										sm_EqualPowerRiseCrossFade;
	static audCurve 										sm_SurfaceDepthToVolDump;
	static audCurve 										sm_SurfaceDepthToInterp;

	static audSmoother										sm_SlopeDebrisSmoother;

	static audFootstepEventTuning							sm_FootstepEventTuning;

	static f32 												sm_PlayerFoleyIntensity;
	static f32 												sm_MoveBlendStopThreshold;
	static f32												sm_FootstepVolDecayOverTime;
	static f32												sm_MaxFootstepVolDecay;
	static f32												sm_FootstepImpMagDecayOverTime;
	static f32												sm_MinFootstepImpMagDecay;
	static f32												sm_MinImpulseMag;
	static f32												sm_MaxImpulseMag;
	static f32												sm_WalkFootstepVolOffset;
	static f32												sm_RunFootstepVolOffset;
	static f32												sm_NPCFootstepVolOffset;
	static f32												sm_CleanUpRatio;
	static f32												sm_MinCleanUpRatio;
	static f32												sm_DryOutRatio;
	static f32												sm_MinDryOutRatio;
	static f32												sm_DistanceThresholdToUpdateGlassiness;
	static f32												sm_FeetInsideWaterVolDump;
	static f32												sm_FeetInsideWaterImpactDump;
	static f32												sm_DeltaApplyPerStep;
	static f32												sm_ClumsyStepToeDelayOffset;
	static f32												sm_CIJumpLandVolumeOffset;
	static f32												sm_NPCRunFootstepRollOffScale;

	static f32												sm_WaterSplashThreshold;
	static f32												sm_WaterSplashSmallThreshold;
	static f32												sm_WaterSplashMediumThreshold;
	static f32												sm_StealthPedRollVolOffset;
	static f32												sm_BareFeetStealthVol;
	static f32												sm_JumpLandCustomImpactStealthVol;

	static u32												sm_JumpLandCustomImpactStealthLPFCutOff;
	static u32												sm_JumpLandCustomImpactStealthAttackTime;
	static u32												sm_BareFeetStealthLPFCutOff;
	static u32												sm_BareFeetStealthAttackTime;
	static u32												sm_StealthPedRollLPFCutoff;
	static u32												sm_StealthPedRollAttackTime;
	static u32												sm_FadeInCrouchScene;
	static u32												sm_FadeOutCrouchScene;

	static bool												sm_MaterialCustomTrack;
	static bool												sm_ShouldShowPlayerMaterial;

#if __BANK

	static Vector3											sm_Position;

	static rage::bkGroup*									sm_WidgetGroup; //Active patches
	static audScene*										sm_ScriptStealthModeScene;
	static CEntity*											sm_HitEntity;
	static CollisionMaterialSettings*						sm_Material;

	static audCurve											sm_PedVelocityToStealthSceneApply;

	static f32												sm_OverridedFootSpeed;
	static f32												sm_OverridedMaterialHardness;
	static f32												sm_OverridedShoeHardness;
	static f32												sm_OverridedShoeDirtiness;
	static f32												sm_OverridedShoeCreakiness;
	static f32												sm_OverridedShoeGlassiness;
	static f32												sm_OverridedShoeWetness;
	static f32												sm_OverridedPlayerStealth;
	//Debug footsteps delayed
	static s32												sm_TimeToPlayDebugStep;
	static phMaterialMgr::Id								sm_MaterialId;
	static u32												sm_LastTimeDebugStepWasPlayed;
	static u16												sm_HitComponent;

	static bool												sm_ShowExtraLayersInfo;
	static bool												sm_ScriptStealthMode;
	static bool												sm_ForceSoftSteps;
	static bool												sm_DontApplyScales;
	static bool												sm_DontUsePlayerVersion;
	static bool												sm_UsePlayerVersionOnFocusPed;
	static bool												sm_OverridePlayerStealth;
	static bool												sm_OverrideShoeGlassiness;
	static bool												sm_OverrideShoeCreakiness;
	static bool												sm_OverrideShoeDirtiness;
	static bool												sm_OverrideFootSpeed;
	static bool												sm_OverrideMaterialHardness;
	static bool												sm_OverrideShoeHardness;
	static bool												sm_OnlyPlayMaterial;
	static bool												sm_OnlyPlayMaterialCustom;
	static bool												sm_OnlyPlayMaterialImpact;
	static bool												sm_OnlyPlayShoe;
	static bool												sm_OverrideShoeWetness;
	static bool												sm_WetFeetInfo; 
#endif
};

inline bool audPedFootStepAudio::HasStandingMaterialChanged(phMaterialMgr::Id materialId, CEntity *entity, u32 component) const
{
	return m_LastMaterial != materialId || m_LastStandingEntity != entity || m_LastComponent != component;
}

#endif // AUD_PEDAUDIOENTITY_H
