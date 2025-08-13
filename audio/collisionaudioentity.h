//      
// audio/collisionaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_COLLISIONAUDIOENTITY_H
#define AUD_COLLISIONAUDIOENTITY_H

// rage headers
#include "atl/array.h"
#include "atl/bitset.h"
#include "atl/freelist.h"
#include "audioengine/curve.h"
#include "audioengine/widgets.h"
#include "audioengine/categorymanager.h"
#include "audioengine/soundset.h"
#include "bank/bkmgr.h"
#include "vector/vector3.h"

// game headers
#include "audioentity.h"

#include "control/replay/Replay.h"
#include "control/replay/Audio/SoundPacket.h"
#include "gameobjects.h"
#include "physics/contactiterator.h"
#include "physics/gtaMaterialManager.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/entity.h" 
#include "weapons/info/AmmoInfo.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "scene/RegdRefTypes.h"

class audOcclusionGroup;
class audScene;

enum audCollisionType
{
	AUD_COLLISION_TYPE_NULL = 0,
	AUD_COLLISION_TYPE_IMPACT,
	AUD_COLLISION_TYPE_SCRAPE,
	AUD_COLLISION_TYPE_ROLL

};

//A bit-field
enum audCollisionFlag
{
	AUD_COLLISION_FLAG_NULL = 0,
	AUD_COLLISION_FLAG_MELEE = BIT(0),
	AUD_COLLISION_FLAG_PUNCH = BIT(1), 
	AUD_COLLISION_FLAG_KICK = BIT(2),
	AUD_COLLISION_FLAG_PLAY_SCRAPE = BIT(3), //So we can play scrapes and rolls in the audio update
	AUD_COLLISION_FLAG_PLAY_ROLL = BIT(4),
	AUD_COLLISION_FLAG_BULLET = BIT(5), 
	AUD_COLLISION_FLAG_PLAYER = BIT(6),
	AUD_COLLISION_FLAG_FOOTSTEP = BIT(7)
};

struct audSwingEvent
{
	audSwingEvent()
	{
		swingSound = NULL;
		Init();
	}

	void Init()
	{
		if(swingSound)
		{
			swingSound->StopAndForget();
		}
		lastProcessTime = 0;
		lastImpactTime = 0;
		historyIndex = 0;
		volSmoother.Init(1.f);
	}

	audSound * swingSound;
	RegdEnt swingEnt;
	u32 lastImpactTime;
	u32 lastProcessTime;
	u16 historyIndex;
	audSimpleSmoother volSmoother;
};

struct audPlayerCollisionEvent
{
	audPlayerCollisionEvent()
	{
		Init();
	}
	void Init()
	{
		lastImpactTime = 0;
		historyIndex = 0;
		blipAmount = 0.f;
		blipMap = false;
	}
	Vec3V position;
	RegdEnt hitEntity;
	u32 lastImpactTime;
	f32 blipAmount;
	u16 historyIndex;
	bool blipMap;
};


struct audCollisionEvent
{
	audCollisionEvent()
	{
		for(int i=0; i<2; i++)
		{
			scrapeSounds[i] = NULL;
			rollSounds[i] = NULL;
		}
		Init();
	}

	void Init() //NB: Only call from main thread due to RegdEnt assignment
	{
		for(int i=0; i<2; i++)
		{
			positions[i] = Vector3(0.f, 0.f, 0.f);

			if(scrapeSounds[i])
			{
				scrapeSounds[i]->StopAndForget();
			}
			if(rollSounds[i])
			{
				rollSounds[i]->StopAndForget();
			}
			impulseMagnitudes[i] = 0.f;
			scrapeMagnitudes[i] = 0.f;
			rollMagnitudes[i] = 0.f;
			rollOffScale = 0.f;
			materialIds[i] = 0;
			materialSettings[i] = NULL;
			components[i] = 0;
			entities[i] = NULL;
		}

		historyIndex = 0;
		predelay = 0;
		type = 0;
		flags = 0;
		rollUpdatedThisFrame = false;
		scrapeUpdatedThisFrame = false;
		material0IsMap = false;
		material1IsMap = false;
		scrapeStopTimeMs = 0;
		rollStopTimeMs = 0;
		lastImpactTime = 0;
	}

	Vector3 positions[2];
	RegdEnt entities[2];
	audSound *scrapeSounds[2];
	audSound *rollSounds[2];
	f32 impulseMagnitudes[2];
	f32 scrapeMagnitudes[2];
	f32 rollMagnitudes[2];
	f32 rollOffScale;
	phMaterialMgr::Id materialIds[2];
	CollisionMaterialSettings *materialSettings[2];
	u32 scrapeStopTimeMs;
	u32 rollStopTimeMs;
	u32 lastImpactTime;
	u16 components[2];
	u16 historyIndex;
	u16 predelay;
	s8 type;
	u8 flags;
	u8 rollUpdatedThisFrame:1;
	u8 scrapeUpdatedThisFrame:1;
	u8 material0IsMap:1;
	u8 material1IsMap:1;


	void SetFlag(audCollisionFlag flag) { flags |= flag; }
	bool GetFlag(audCollisionFlag flag) const { return (flags & flag) != 0; }
	void ClearFlag(audCollisionFlag flag) { flags &= ~flag; }

};

//This class compresses an index into the history and the last collision time to fit
//in the user data on the phManifold

class	audManifoldData
{
	public:
	audManifoldData():
		historyIndex(0),
		lastCollisionTime(0)
	{
	};

	void SetHistoryIndex(u16 index)
	{
		historyIndex = index+1;
	}

	void GetHistoryIndex(u16 &index)
	{
		if(historyIndex)
			index = historyIndex-1;
		// return (historyIndex);
	}

	void SetNoHistory()
	{
		historyIndex = 0;
	}

	void SetLastCollisionTime(u16 time)
	{
		lastCollisionTime = time;
	}

	u16 GetLastCollisionTime()
	{
		return lastCollisionTime;
	}

	private:
	u16 historyIndex; //This is actually the index +1 as 0 is used as no index
	u16 lastCollisionTime;
};



struct audBulletImpact
{
	Vector3 pos;
	phMaterialMgr::Id matID;
	RegdEnt entity;
	audSoundSetFieldRef soundRef;	
	CAmmoInfo::SpecialType ammoType;
	f32 volume;
	f32 frequencyScaling;
	f32 rollOffScale;	
	u16 predelay;
	audSoundSetFieldRef slowMotionSoundRef;
};

struct audFragmentBreakEvent
{
	Vec3V pos;
	RegdEnt entity;
	u32 breakSound;

};

struct audUprootEvent
{
	Vec3V pos;
	RegdEnt entity;
	u32 uprootSound;
};

struct audManifoldUserData
{
	audManifoldUserData(u32 manifoldData) { data = manifoldData;} 
	union
	{
			struct
			{
					u16 index; //Index of the collision event in the history (will be checked against the entities to ensure correct matching)
					u16 time; //time of last successful collision on the manifold
			}components;
			u32 data;
	};
};
#if __BANK
struct MaterialAudName
{
	char name[64];
};
#endif
static const u32 g_MaxCollisionEventHistory = 300;
static const u32 g_MaxPlayerCollisionEventHistory = 32;
static const u32 g_MaxSwingEventHistory = 32;

const u32 g_MaxCachedBulletImpacts = 32;
const u32 g_MaxCachedMeleeImpacts = 32;
const u32 g_MaxCachedFragmentBreaks = 32;
const u32 g_MaxCachedUproots = 32;

BANK_ONLY(extern atArray<MaterialAudName> g_audCollisionMaterialNames);
extern atArray<CollisionMaterialSettings*> g_audCollisionMaterials;
extern f32 g_MinSpeedForCollision;
extern const audCategory *g_CollisionCategory;
extern const audCategory *g_UppedCollisionCategory;
extern f32 g_VehicleScrapeImpactVel;
extern f32 g_ScrapeImpactVel;
extern const u32 g_ScrapingCollisionHoldTimeMs;
extern f32 g_UpsideDownCarImpulseScalingFactor;




class audCollisionAudioEntity : public naAudioEntity
{
#if GTA_REPLAY
	friend class CPacketSoundCreate;
#endif
public:
	AUDIO_ENTITY_NAME(audCollisionAudioEntity);

	virtual void Init(void);
	virtual void Shutdown(void);

	virtual void PreUpdateService(u32 timeInMs);

	virtual void PostUpdate();

	static void InitClass();

	void UpdateManifold(VfxCollisionInfo_s & vfxColnInfo, phManifold & manifold,bool breakableCollisionA,bool breakableCollisionB);

	void ReportFragmentBreak(const phInst *instance, u32 componentId, bool isDestroyed = false);

	void ReportUproot(CEntity *entity);

	void ReportMeleeCollision(const WorldProbe::CShapeTestHitPoint* pResult, f32 damageDone, CEntity *hittingEntity = NULL, const bool isBullet = false, const bool isFootsep = false, const bool isPunch = false, const bool isKick = false);
	void ReportHeliBladeCollision(const WorldProbe::CShapeTestHitPoint* pResult, CVehicle* vehicle);

	void ReportBulletHit(u32 weaponAudioHash, const WorldProbe::CShapeTestHitPoint* pResult, const Vector3 &weaponPosition, WeaponSettings * weaponSettings, CAmmoInfo::SpecialType ammoType, bool playerShot = false,bool bulletHitPlayer = false,bool isAutomatic = false, bool isShotgun = false, f32 timeBetweenShots = 0.f);

	void ProcessMacsModelOverrides();
	void ProcessMacsModelOverridesForChunk(u32 chunkId, u32 modelListHash);
	void RemoveMacsModelOverridesForChunk(u32 chunkId, u32 modelListHash);


#if __BANK

	void PrintCollisionSoundDebug(const char * context, f32 impulse, f32 volume, u32 soundHash);

	void DrawDebug();
	static void AddWidgets(bkBank &bank);

	void ReconnectPropMacsForChunk(u32 chunkId);

	////
	//Code that enables point and click creation of model settings from within the game
	////
	
	//Populates a static structure with information about the selcted model needed to generate the Macs
	//model: the model with which to populate the settings creator struct
	//name: the name of the Macs object to be created (i.e. MOD_<model name>)
	//numComponents: number of fraggable components in hte model
	static void PopulateModelSettingsCreator(CBaseModelInfo * model, const char * name, u32 numComponents);
	
	//Will send a message to RAVE to create a Macs game object; will create fragment Macs for multi component models
	static void CreateModelAudioSettings();
	
	//Does what is says on the tin; will navigate RAVe to the Macs of the currently selected model (if it has one)
	static void OpenCurrentModelInRave();
	
	//Macs: Model Audio Collision Settings
	//xmlMsg: is a char array that will be filled with the formatted message to be sent to RAVE. This needs to be pretty big; 4096 seems to be enough at the moment;
	//settingsName: the name of the Macs object to be (usually MOD_<model name>)
	//numComponents: number of frag Macs childen to include
	//overrideMaterials: frag Macs children dont override materials by default so set this to false
	static void FormatMacsXml(char * xmlMsg, const char * settingsName, u32 numComponents = 0, bool overrideMaterials = true, bool emptyComponents = true);
	
	//Called by CreateModelAudioSettings for multi-component models
	static void CreateFragModelAudioSettings();

	//Used to add override materials to frag components
	static void CreateIndividualFragModelAudioSettings();
	
	//Returns true if the number of components, number of material overrides or materials being overridden on s_ModelSettingsCreator don't match the Macs passed in
	static bool AreMacsMaterialsOutOfDate(const ModelAudioCollisionSettings * macs);
	
	//Iterates through all models loaded in and repopulates their Macs if they are out of sync
	static void SynchronizeMacsAndModels();
	
	static void FindNewModels();
	static void FindDefaultMaterialModels(fiStream *stream);
	static void SpawnNewModelObject();
	static void SpawnSyncModelObject();
	static void FindLocalDefaultMaterialModels();

	//Used to simulate impacts with different properties using the point and click editor
	static void SimulateCollision();

	static void BatchCreateMacs();

	void HandleRaveSelectedMaterial(const ModelAudioCollisionSettings * settings);
	static void SpawnRaveSettings();


#endif

	static bool sm_EnableModelEditor;


	static void UpdateModelEditorSelector();


	bool IsScrapeFromMagnitudes(const f32 bangMag, const f32 scrapeMag);
	bool IsOkToPlayImpacts(phManifold & manifold);
	bool ShouldPlayRollImpact(const audCollisionEvent * colEvt, float bangMag);
	bool ShouldPlayScrapeImpact(const audCollisionEvent * colEvt, float bangMag);

	CollisionMaterialSettings * GetMaterialFromIntersection(phIntersection *intersection);
	CollisionMaterialSettings * GetMaterialFromShapeTestSafe(const WorldProbe::CShapeTestHitPoint *intersection);

	CollisionMaterialSettings * GetCollisionSettings(s32 materialId);

	//Will return the collision settings for the material. Includes a call to GetMaterialOverride, so no need to call that seperately
	//entity: the entity from which to get the override
	//matId: the id of the material we want settings for
	//component: the frag component index for composite models
	CollisionMaterialSettings *GetMaterialSettingsFromMatId(const CEntity * entity, phMaterialMgr::Id matId, u32 component);

	void TriggerDeferedImpacts();
	void TriggerDeferredFragmentBreaks();
	void TriggerDeferredUproots();

	//Will return the Macs override for base if it has one
	//entity: the entity from which to get the override
	//base: the material settings we want to override
	//component: the frag component index for composite models
	static CollisionMaterialSettings * GetMaterialOverride(const CEntity * entity, CollisionMaterialSettings * base, u32 component = 0);

	//Will get the frag Macs for the input settings: it can't be accessed directly since it sits under another variable array and must be fixed up
	//settings: the parent Macs for the entity
	//component: the component we want to get the frag Macs for 
	static const ModelAudioCollisionSettings * GetFragComponentMaterialSettings(const ModelAudioCollisionSettings * settings, u32 component, bool dontUseParentMacs = false);
	//Does the same but includes a chack that there are frag components and returns settings if there aren't
	static const ModelAudioCollisionSettings * GetFragComponentMaterialSettingsSafe(const ModelAudioCollisionSettings * settings, u32 component);

	static const ModelAudioCollisionSettings * GetFragComponentMaterialSettings(CEntity * entity, u32 component, bool dontUseParentMacs = false)
	{
		return GetFragComponentMaterialSettings(entity->GetBaseModelInfo()->GetAudioCollisionSettings(), component,dontUseParentMacs);
	}
	//Does the same but includes a chack that there are frag components and returns settings if there aren't
	static const ModelAudioCollisionSettings * GetFragComponentMaterialSettingsSafe(CEntity * entity, u32 component)
	{
		return GetFragComponentMaterialSettingsSafe(entity->GetBaseModelInfo()->GetAudioCollisionSettings(), component);
	}

	//does what it say on the tin: it can't be accessed directly since it sits under a variable array and must be fixed up
	static u32 GetNumFragComponentsFromModelSettings(const ModelAudioCollisionSettings * settings)
	{
		if(settings)
		{
			//Two variable sized arrays in object so need to do a bit of fixing up to get values for the second array
			return *((u32 *)(settings->MaterialOverride + settings->numMaterialOverrides));
		}
		return 0;
	}

	static bool EntityIsInWater(CEntity * entity); 

	void ComputeStartOffsetAndVolumeFromImpactMagnitude(f32 impactMag, CollisionMaterialSettings *materialSettings, u32 &startOffset, f32 &volume, bool isPed=false);
	f32 ComputeScaledImpactMagnitude(f32 impactMag, CollisionMaterialSettings * materialSettings);

	CollisionMaterialSettings * GetVehicleCollisionSettings();

	//returns one of 5 audio weights specified on the entity's Macs, frag Macs will return the weight of the heavies thing they are attached to
	AudioWeight GetAudioWeight(CEntity *entity);
	static f32 GetAudioWeightScaling(AudioWeight weight, AudioWeight otherWeight);

	static bool ShouldDialogueSuppressBI(){return sm_DialogueSuppressBI;};
private:
	void TriggerDeferredBulletImpact(const u32 hash, const Vector3 &pos, f32 volume, f32 frequencyScaling, bool playerShot, bool isRicochets, CEntity * entity, phMaterialMgr::Id matID, CAmmoInfo::SpecialType ammoType, bool isHeadShot = false, u32 slowMoSoundHash = g_NullSoundHash);
	void TriggerDeferredBulletImpact(const audSoundSetFieldRef soundRef, const Vector3 &pos, f32 volume, f32 frequencyScaling, bool playerShot, bool isRicochets, CEntity * entity, phMaterialMgr::Id matID, CAmmoInfo::SpecialType ammoType, bool isHeadShot = false, audSoundSetFieldRef slowMoSoundRef = g_NullSoundHash);

	void TriggerDeferredMeleeImpact(const WorldProbe::CShapeTestHitPoint* pResult, const f32 damageDone, f32 impulseMagnitudeDamping, const bool isBullet, bool playerShot, bool isFootsetp = false, CEntity * hittingEntity = NULL, const bool isPunch = false, const bool isKick = false);

	bool PopulateAudioCollisionEventFromImpact(audCollisionEvent *collisionEvent, const VfxCollisionInfo_s &collisioninfo,
		float bangMag, float scrapeMag, float rollMag, bool shouldFlipEntities);
	void PopulateAudioCollisionEventFromIntersection(audCollisionEvent *collisionEvent, const WorldProbe::CShapeTestHitPoint* pResult);

	bool ProcessRollSounds(audCollisionEvent *collisionEvent);

	void PlayImpactSoundsDeferred(audCollisionEvent *collisionEvent);
	bool PlayScrapeSounds(audCollisionEvent *collisionEvent);
	void UpdateScrapeSounds(audCollisionEvent *collisionEvent);
	bool PlayRollSounds(audCollisionEvent *collisionEvent);
	void UpdateRollSounds(audCollisionEvent *collisionEvent);
	void ComputeStartOffsetAndVolumeFromImpulseMagnitude(audCollisionEvent *collisionEvent, u32 eventIndex, u32 &startOffset, f32 &volume, bool isPed = false);
	f32 ComputeScrapeSpeed(audCollisionEvent *collisionEvent);
	void ComputePitchAndVolumeFromScrapeSpeed(audCollisionEvent *collisionEvent, u32 index, s32 &pitch, f32 &volume);
	f32 ComputeRollSpeed(audCollisionEvent *collisionEvent);
	void ComputePitchAndVolumeFromRollSpeed(audCollisionEvent *collisionEvent, u32 index, s32 &pitch, f32 &volume);
	f32 ComputeScaledImpulseMagnitude(audCollisionEvent *collisionEvent, u32 index);

#if __BANK
	static void ClearDebugHistory();
	static void InitWorldCollisionListGenerator();
	static void GenerateWorldCollisionLists();
	static void AbortWorldCollisionListGeneration();
	static void WriteCollisionListData();
	static void FindNewModels(fiStream *stream, bool findMovable);
	static void FindChangedModels(fiStream *stream);
#endif

public:

	u32 GetTimeSinceLastBulletHit() {return sm_TimeSinceLastBulletHit;}

	bool ProcessImpactSounds(audCollisionEvent *collisionEvent, bool forceSoftImpact = false);
	bool ProcessScrapeSounds(audCollisionEvent *collisionEvent);

	//Sets the index of this manifold's collision event in the collision event history (used for scraping and rolling sounds)
	void SetManifoldUserDataIndex(phManifold &manifold, u16 index)
	{
		audManifoldUserData & userData = (audManifoldUserData &)(manifold.GetUserDataRef());
		userData.components.index = index;
	}

	//Gets the index of this manifold's collision event in the collision event history (used for scraping and rolling sounds)
	u16 GetManifoldUserDataIndex(phManifold &manifold)
	{
		return audManifoldUserData(manifold.GetUserData()).components.index;
	}

	//Sets the last impact time of this manifold's collision event in the collision event history
	void SetManifoldUserDataTime(phManifold &manifold, u16 time)
	{
		audManifoldUserData & userData = (audManifoldUserData &)(manifold.GetUserDataRef());
		userData.components.time = time;
	}

	//Gets the last impact time of this manifold's collision event in the collision event history
	u16 GetManifoldUserDataTime(phManifold &manifold)
	{
		return audManifoldUserData(manifold.GetUserData()).components.time;
	}
	
	//Uses the m_UserData on the manifold to look for an exising collision event 
	audCollisionEvent* GetCollisionEventFromHistory(phManifold &manifold)
	{
		u16 index = GetManifoldUserDataIndex(manifold);
		if(index)
		{
			return &m_CollisionEventHistory[index-1]; //We added 1 when the index was set so we could use 0 as invalid
		}
		return NULL;
	}

	void HandleEntitySwinging(CEntity * entity);

	audSwingEvent * FindOrAllocateSwingEvent(CEntity * entity);
	audSwingEvent* GetFreeSwingEventFromHistory(u16 *index = NULL);
	audSwingEvent * GetSwingEventFromHistory(CEntity * entity);
	audPlayerCollisionEvent* GetPlayerCollisionEventFromHistory(CEntity * entity);
	audPlayerCollisionEvent* FindOrAllocatePlayerCollisionEvent(CEntity *entity);
	void HandlePlayerEvent(CEntity * entity);

	audCollisionEvent *GetFreeCollisionEventFromHistory(u16 *index = NULL);
	audPlayerCollisionEvent *GetFreePlayerCollisionEventFromHistory(u16 *index = NULL);
	audCollisionEvent *GetRollCollisionEventFromHistory(CEntity* entityA, CEntity * entityB, u16* eventIndex);

	bool CanPlayRollSounds(const audCollisionEvent * colEvent);

	//Used by the conductors
	static f32	sm_PlayerRicochetsRollOffScaleFactor;

	audCurve & GetPedImpactVolumeCurve() { return m_PedImpactVolCurve; }

	audCurve & GetScrapeVolCurve() { return m_ScrapeVolCurve; }
	audCurve & GetScrapePitchCurve() { return m_ScrapePitchCurve; }
	
	const phMaterialMgr::Id GetCarBodyMaterialId() {return m_MaterialIdCarBody;};

	naEnvironmentGroup* GetOcclusionGroup(const Vec3V& position, CEntity *entity, const phMaterialMgr::Id matID = phMaterialMgr::MATERIAL_NOT_FOUND);

private:

	naEnvironmentGroup* GetOcclusionGroup(const audCollisionEvent *collisionEvent);

	naEnvironmentGroup* CreateBulletImpactEnvironmentGroup(const Vec3V& position, CEntity* entity, const phMaterialMgr::Id matID = phMaterialMgr::MATERIAL_NOT_FOUND);
	atFixedBitSet<g_MaxCollisionEventHistory> m_CollisionEventHistoryAllocation;
	atRangeArray<audCollisionEvent, g_MaxCollisionEventHistory> m_CollisionEventHistory;
	atFreeList<s16,g_MaxCollisionEventHistory> m_CollisionEventFreeList;

	atFixedBitSet<g_MaxPlayerCollisionEventHistory> m_PlayerCollisionEventHistoryAllocation;
	atRangeArray<audPlayerCollisionEvent, g_MaxPlayerCollisionEventHistory> m_PlayerCollisionEventHistory;
	atFreeList<s16,g_MaxPlayerCollisionEventHistory> m_PlayerCollisionEventFreeList;

	atFixedBitSet<g_MaxSwingEventHistory> m_SwingEventHistoryAllocation;
	atRangeArray<audSwingEvent, g_MaxSwingEventHistory> m_SwingEventHistory;
	atFreeList<s16,g_MaxSwingEventHistory> m_SwingEventFreeList;

	audCurve m_MeleeDamageImpactScalingCurve;
	audCurve m_MeleeFootstepImpactScalingCurve;
	u32 m_UppedCollisionCategoryNameHash;
	phMaterialMgr::Id m_CustomMaterialIdFootLeft;
	phMaterialMgr::Id m_CustomMaterialIdFootRight;
	phMaterialMgr::Id m_MaterialIdCarBody;
	phMaterialMgr::Id m_MaterialIdCarTyre;

	atFixedArray<audBulletImpact, g_MaxCachedBulletImpacts> m_CachedBulletImpacts;
	atFixedArray<audCollisionEvent, g_MaxCachedMeleeImpacts> m_CachedMeleeImpacts;
	atFixedArray<audFragmentBreakEvent, g_MaxCachedFragmentBreaks> m_CachedFragmentBreaks;
	atFixedArray<audUprootEvent, g_MaxCachedUproots> m_CachedUprootEvents;

	audCurve m_ImpactVolCurve;
	audCurve m_PedImpactVolCurve;
	audCurve m_VehicleImpactVolCurve;
	audCurve m_ImpactStartOffsetCurve;
	audCurve m_PedImpactStartOffsetCurve;
	audCurve m_VehicleImpactStartOffsetCurve;
	audCurve m_ScrapeVolCurve;
	audCurve m_SwingVolCurve;
	audCurve m_PedScrapeVolCurve;
	audCurve m_PlayerScrapeVolCurve;
	audCurve m_VehicleScrapeCurve;
	audCurve m_VehicleScrapePitchCurve;
	audCurve m_ScrapePitchCurve;
	audCurve m_PedScrapePitchCurve;
	audCurve m_PlayerScrapePitchCurve;
	audCurve m_RollVolCurve;
	audCurve m_RollPitchCurve;
	audCurve m_RollResVolCurve;
	audCurve m_RollResPitchCurve;

	static atMap<u32,u32> sm_MacOverrideMap;

	static audCurve sm_TimeBetweenShotsToRicoProb;
	static audCurve sm_DistanceToBIPredelay;
	static audCurve sm_DistanceToHeadShotPredelay;
	static audCurve sm_DistanceToRicochetsPredelay;

	static f32	sm_PlayerBIRollOffScaleFactor;
	static f32 sm_PlayerHeadShotRollOffScaleFactor;

	static u32 sm_PlayerCollisionEventTimeout;
	static f32 sm_PlayerCollisionEventSpeedThreshold;
	static f32 sm_PlayerContextCollisionRolloff;
	static f32 sm_PlayerContextVolumeForBlip;
	static f32 sm_DistanceToTriggerAutomaticBI;
	static f32 sm_DistanceToTriggerRaygunBI;
	static f32 sm_DistanceToTriggerShotgunBI;

	static u32 sm_DtToTriggerShotgunBI;
	static u32 sm_DtToTriggerAutomaticBI;
	static u32 sm_TimeSinceLastBulletHit;
	static u32 sm_TimeSinceLastAutomaticBulletHit;
	static u32 sm_TimeSinceLastShotgunBulletHit;

	static bool sm_DialogueSuppressBI;
#if __BANK 
	static audScene *sm_IncreaseFootstepVolumeScene;
	
	static int sm_CurrentSectorX;
	static int sm_CurrentSectorY;
	static bool sm_IsWaitingToInit;
	static bool sm_ProcessSectors;
	static fiStream * sm_NewMoveableFile;
	static fiStream * sm_NewStaticFile;
	static fiStream * sm_ChangedMacsFile;
	static fiStream * sm_DefaultMatFile;
	static bool sm_AutomaticImpactOnlyOnVeh;
	static bool sm_ShotgunDisabledForPlayer;
	static bool sm_ShotgunImpactOnlyOnVeh;
#endif

};

extern audCollisionAudioEntity g_CollisionAudioEntity;

#if __BANK
extern bool g_EnableModelEditor;
#endif

#endif // AUD_COLLISIONAUDIOENTITY_H

