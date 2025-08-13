//  
// audio/vehiclecollisionaudio.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLECOLLISIONAUDIO_H
#define AUD_VEHICLECOLLISIONAUDIO_H

#include "audiodefines.h"
#include "audioengine/widgets.h"
#include "audiohardware/waveslot.h"
#include "audioengine/curverepository.h"
#include "audioengine/environment.h"
#include "audioengine/soundset.h"
#include "bank/bkmgr.h"
#include "renderer/HierarchyIds.h"
#include "vehicles/VehicleDefines.h"
#include "audio_channel.h"
#include "gameobjects.h"
#include "collisionaudioentity.h"
#include "Vfx/Systems/VfxMaterial.h"

#include "audioentity.h"

#include "audio/environment/environmentgroup.h"

class audVehicleAudioEntity;
class CVehicle;

enum audVehicleCollisionType
{
	AUD_VEH_COLLISION_NULL = 0,
	AUD_VEH_COLLISION_WHEEL_SIDE = BIT(1),
	AUD_VEH_COLLISION_WHEEL_BOTTOM = BIT(2), 
	AUD_VEH_COLLISION_BODY = BIT(3),
	AUD_VEH_COLLISION_PED = BIT(4),
	AUD_VEH_COLLISION_SCRAPE = BIT(5),
	AUD_VEH_COLLISION_BOTTOM = BIT(6), 
	AUD_VEH_COLLISION_UPSIDEDOWN = BIT(7), 
	AUD_VEH_COLLISION_LEFTSIDE = BIT(8), //Currently unused
	AUD_VEH_COLLISION_RIGHTSIDE = BIT(9), //Currently unused
	AUD_VEH_COLLISION_ON_VEHICLE = BIT(10)
};

struct audVehicleCollisionEvent
{
	audVehicleCollisionEvent(VfxCollisionInfo_s & collisionInfo, bool vehicleIsA)
	{
		if(vehicleIsA)
		{
			pos = collisionInfo.vWorldPosA;
			otherPos = collisionInfo.vWorldPosB;
			normal = collisionInfo.vWorldNormalA;
			component = (u16)collisionInfo.componentIdA; 
			otherComponent = (u16)collisionInfo.componentIdB;
			otherMaterialId = collisionInfo.mtlIdB;
			impactMag = collisionInfo.bangMagA;
			scrapeMag = collisionInfo.scrapeMagA;
		}
		else
		{
			pos = collisionInfo.vWorldPosB;
			otherPos = collisionInfo.vWorldPosA;
			normal = collisionInfo.vWorldNormalA;
			component = (u16)collisionInfo.componentIdB; 
			otherComponent = (u16)collisionInfo.componentIdA;
			otherMaterialId = collisionInfo.mtlIdA;
			impactMag = collisionInfo.bangMagA;
			scrapeMag = collisionInfo.scrapeMagA;
		}
	}

	audVehicleCollisionEvent()
	{

	}

	void Init()
	{
		pos.ZeroComponents();
		otherPos.ZeroComponents();
		normal.ZeroComponents();
		component = 0;
		otherComponent = 0;
		otherMaterialId = 0;
		impactMag = 0.f;
		scrapeMag = 0.f;
	}

	void Init(const audVehicleCollisionEvent & colEvent)
	{
		pos = colEvent.pos;
		otherPos = colEvent.otherPos;
		component = colEvent.component;
		normal = colEvent.normal;
		otherComponent = colEvent.otherComponent;
		otherMaterialId = colEvent.otherMaterialId;
		impactMag = colEvent.impactMag;
		scrapeMag = colEvent.scrapeMag;
	}

	Vec3V pos;
	Vec3V otherPos;
	Vec3V normal;
	u16 component;
	u16 otherComponent;
	phMaterialMgr::Id otherMaterialId;
	f32 impactMag;
	f32 scrapeMag;
};

struct audVehicleFoliageCollisions
{
	audVehicleFoliageCollisions()
	{
		drag = 0.f;
		numCollisions = 0;
	}

	u32 numCollisions;
	f32 drag;
};


struct audVehicleCollisionContext
{
	audVehicleCollisionContext()
	{
		Init();
	}

	void Init()
	{
		next = NULL;
		otherEntity = NULL;
		type = AUD_VEH_COLLISION_NULL;
		scrapeMag = 0.f;
		impactVolume = -100.f;		
		impactOffset = 0;
		impactMag = 0.f;
		canPlayCollision = false;
		impactVelocity = 0.f;
	}

	void Init(CEntity * pOtherEntity, Vector3::Vector3Param vVelocity, u32 typeFlags)
	{
		otherEntity = pOtherEntity;
		type = typeFlags;
		velocity = vVelocity;

		next = NULL;
		scrapeMag = 0.f;
		impactVolume = -100.f;		
		impactOffset = 0;
		impactMag = 0.f;
		canPlayCollision = false;
		impactVelocity = 0.f;
	}

	void Init(audVehicleCollisionContext & inEvent);

	audVehicleCollisionEvent collisionEvent;

	Vector3 velocity;

	audVehicleCollisionContext * next;
	RegdEnt otherEntity;

#if! __FINAL
	RegdVeh  parent;
#endif

	f32 impactMag;
	f32 impactVolume;
	u32 impactOffset;
	f32 impactVelocity;
	f32 scrapeMag;

	u32 type;

	bool canPlayCollision;

	void SetTypeFlag(audVehicleCollisionType flag) { type |= (u32)flag; }
	bool GetTypeFlag(audVehicleCollisionType flag) const { return (type & (u32)flag) != 0; }
	void ClearTypeFlag(audVehicleCollisionType flag) { type &= ~(u32)flag; }
};

class audVehicleCollisionContextList
{
public:

	audVehicleCollisionContextList();
	~audVehicleCollisionContextList()
	{
		Reset();
	}

	// PURPOSE:	Check if an impact should be added, and update existing impacts if needed.
	//			Initialize a new entry from the pool or reuse list, and return it, if a new one
	//			needs to be added.
	audVehicleCollisionContext* InitAndAddEventToList(CEntity* pOtherEntity, Vec3V_In velocityV, u32 typeFlags);

	audVehicleCollisionContext * GetEvents()  
	{
		return m_List;
	}

	u32 GetFrameCount()
	{
		return m_FrameCount;
	}

	void SetFrameCount(u32 frameCount)
	{
		m_FrameCount = frameCount;
	}

	void Reset();

	// PURPOSE:	Like Reset(), but don't return to the pool, move to m_ReuseList instead.
	void ResetToReuseList();

	// PURPOSE:	Clear any entries in m_ReuseList, returning them to the pool.
	void ClearReuseList();

	static const u32 k_MaxCollisionEvents = 128;

private:

	audVehicleCollisionContext * m_List;

	// PURPOSE:	Temporary linked list for nodes that were removed from m_List, but are likely to be
	//			reused again in the same order, for performance.
	audVehicleCollisionContext * m_ReuseList;

	u32 m_FrameCount;
};

class audVehicleCollisionAudio
{

public:

	audVehicleCollisionAudio(audVehicleAudioEntity * parent);

	static void InitClass();

	void ProcessImpactBoat(phContactIterator& impacts);

	// PURPOSE:	Start a sequence of calls to ProcessImpact().
	// RETURNS:	True if we should call ProcessImpact()+EndProcessImpacts(), false
	//			if this vehicle is not interested.
	bool StartProcessImpacts();

	// PURPOSE:	End a sequence of calls to ProcessImpact().
	void EndProcessImpacts();

	void ProcessImpact(phCachedContactConstIterator& impacts);
	void ProcessContact(VfxCollisionInfo_s& collisionInfo, phManifold & manifold, CEntity * otherEntity, const audVehicleCollisionEvent & collisionEvent);
	void TriggerDebrisSounds(const f32 impactVelocity);
	void TriggerJumpLand(const f32 impulseMag, bool justFinishedWheelie = false, bool useBigJumpLandSound = true);
	void TriggerJumpLandScrape();
	void TriggerUpsideDownRoll(const f32 impulseMag);
	void TriggerScrapeImpact(audSoundInitParams & initParams);

	void ProcessFoliageImpact(f32 drag);
	void ProcessFoliage();

	void ProcessCollision(audVehicleCollisionContext * impactContext, const audVehicleCollisionEvent & collisionEvent);
	void ProcessVehicleCollision();

	VehicleCollisionSettings * GetVehicleCollisionSettings(); 

	bool IsOkToPlayImpacts(audVehicleCollisionContext* playContext);
	void ProcessScrape(audVehicleCollisionContext * scrapeContext, phManifold& manifold, const audVehicleCollisionEvent & collisionEvent);
	void ComputePitchAndVolumeFromScrapeSpeed(audCollisionEvent *collisionEvent, u32 index, s32 &pitch, f32 &volume);

	audVehicleAudioEntity * GetParent()
	{
		return m_Parent;
	}

	void WreckVehicle();

	CVehicle * GetVehicle();

	void PostProcessImpacts();

	void UpdateDeformation(const float damage, const Vector3 & offset, CEntity * inflictor);

	CollisionMaterialSettings *GetVehicleCollisionMaterialSettings();
	CollisionMaterialSettings *GetVehicleCollisionMaterialSettings(u32 component); 

	bool IsTrailer();
	bool IsBoat();
	bool IsBike();

	void UpdateSound(audSound *sound, audRequestedSettings *reqSets, u32 timeInMs);

	audVehicleCollisionContextList & GetCollisionContextList() { return m_CollisionEventsList; }

	void HeadLightSmash();

	void Fakeimpact(f32 impactMag, f32 volumeCurveScale = -1.f, bool forceImpact = false);
	void ProcessFakeImpact();

	void HandleMelee();

	void PostUpdate();

#if __BANK
	static void AddWidgets(bkBank &bank);

	static void UpdateCollisionSettings();
#endif

	static audCurve sm_VehicleCollisionRolloffBoostCurve;

private:

	audVehicleCollisionContextList m_CollisionEventsList;	

	audCurve m_SlowScrapeVolCurve, m_ScrapeImpactCurve, m_SlowScrapeImpactCurve, m_ScrapePitchCurve, m_ScrapeVolCurve;

	static audSoundSet sm_SoundSet;

	audVehicleFoliageCollisions m_Foliage;

	audVehicleAudioEntity * m_Parent;

	VehicleCollisionSettings * m_VehicleCollisionSettings;
	CollisionMaterialSettings * m_CollisionMaterialSettings;

	audVehicleCollisionContext  m_VehicleCollisionContext;

	RegdVeh m_OtherVehicleOnTopOf;

	audSound* m_CarRollSound;
	u32 m_LastJumpLandTime;
	u32 m_LastJumpLandScrapeTime;

	f32 m_ImpactDamage;
	f32 m_JumpLandMag; 

	audSound * m_ScrapeSound, *m_SlowScrapeSound, *m_GlassDebrisSound, *m_PartsDebrisSound, *m_FoliageLoop, *m_TrainLoop;
	u32 m_LastCarRollTime;
	f32 m_LastCarRollVol;
	u32 m_ScrapeStopSoundMs;
	u32 m_LastPartsDebrisTime;
	f32 m_LastPartsDebrisVolLin;
	u32 m_LastGlassDebrisTime;
	f32 m_LastGlassDebrisVolLin;
	u32 m_JumpStartTime;
	u32 m_WreckedTime;
	u32 m_LastTimeHitByTrain;
	u32 m_LastTrailerBumpTime;
	u32 m_LastBodyCollisionTime;

	u32 m_LastFakeImpactTime;
	f32 m_LastFakeImpactVol;
	f32 m_FakeImpactMagThisFrame;
	f32 m_LastVehicleCollisionVolume;
	u32 m_LastVehicleCollisionTime;
	u32 m_LastMeleeTime;
	bool m_PlayingFoliageThisFrame : 1; 
	bool m_ImpactThisFrame : 1;
	bool m_WasHitByTrain : 1;
	bool m_IsOnAnotherVehicle : 1;
	bool m_NeedsToPlayVehOnVehImpact : 1;

#if !__FINAL
	static u32 sm_debugEventCounter;
	static bool sm_debugEventListFull;
#endif

#if __BANK
	static bool sm_UpdateVehicleCollisionSettings;
#endif

	static audCurve sm_VehicleLandFactorCurve;

	static u32 sm_TrainLoopTime;

	static float sm_MinSpeedForVehicleCollisions;
	static u32 sm_TimeForBikePostMeleeImpacts;


};

#endif
