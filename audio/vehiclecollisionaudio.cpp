
#include "vehiclecollisionaudio.h"
#include "boataudioentity.h"
#include "frontendaudioentity.h"
#include "debugaudio.h"
#include "dooraudioentity.h"
#include "northaudioengine.h"
#include "audio/environment/environment.h"
#include "audio/vehiclereflectionsaudioentity.h"
#include "pedaudioentity.h"
#include "traileraudioentity.h"
#include "caraudioentity.h"
#include "policescanner.h"
#include "radioaudioentity.h"
#include "radiostation.h"
#include "radioslot.h"
#include "weatheraudioentity.h"
#include "trainaudioentity.h"

#include "animation/animbones.h"
#include "audioeffecttypes/waveshapereffect.h"
#include "audioengine/categorymanager.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audioengine/environment.h"
#include "audioengine/environment_game.h"
#include "audioengine/soundfactory.h"
#include "audioengine/widgets.h"
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "audiohardware/driverdefs.h"
#include "audiohardware/driverutil.h"
#include "audiosoundtypes/envelopesound.h"
#include "audiosoundtypes/simplesound.h"
#include "audiosoundtypes/sound.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audiosoundtypes/sounddefs.h"
#include "atl/staticpool.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/tracking/CinematicHeliChaseCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "vehicleAi/vehicleintelligence.h"
#include "control/record.h"
#include "control/replay/audio/CollisionAudioPacket.h"
#include "crskeleton/skeleton.h"
#include "debug/debugglobals.h"
#include "grcore/debugdraw.h"
#include "game/ModelIndices.h"
#include "game/weather.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "network/NetworkInterface.h"
#include "phbound/boundcomposite.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "profile/element.h"
#include "objects/Door.h"
#include "renderer/Water.h"
#include "fwsys/timer.h"
#include "vehicles/automobile.h"
#include "vehicles/boat.h"
#include "vehicles/door.h"
#include "vehicles/heli.h"
#include "vehicles/vehicle.h"
#include "vehicles/Trailer.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vfx/Systems/VfxVehicle.h"
#include "peds/pedeventscanner.h"
#include "peds/PopCycle.h"
#include "peds/PedIntelligence.h"
#include "peds/PlayerPed.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "vfx/systems/VfxWheel.h"
#include "renderer/water.h"

AUDIO_VEHICLES_OPTIMISATIONS()


extern float g_CarVelForPedImpact;
extern float g_BodyDamageFactorForDebris;
extern float g_BodyDamageFactorForHydraulicDebris;
extern float g_DamageImpactVel;
extern float g_TrailerBumpVel;

extern f32 g_MinCarRollImpulseMag;
extern f32 g_MinCarRollSpeed;
extern f32 g_MaxCarRollImpulseMag;
extern f32 g_MaxCarRollSpeed;
extern u32 g_HydraulicsJumpLandSuppressionTime;

extern fwPtrListSingleLink g_RecentlyCollidedMaterials;
extern u32 g_CollisionMaterialCountThreshold;
extern bool g_UseCollisionIntensityCulling;
extern bool g_PedsMakeSolidCollisions;

f32 g_BoatBottomContactDotThreshold = 0.4f;
u32 g_BlockingTimeForFakeImpacts = 300;
u32 g_BlockingTimeForVehicleCollisions = 100;
u32 g_StuntJumpLandingMinJumpTimeMs = 500;
u32 g_ToyCarMinAirTimeForBigLanding = 250;
u32 g_ToyCarMinBigJumpLandDuration = 500;
u32 g_ToyCarMinSmallJumpLandDuration = 150;
bool g_NoVehicleCollisions = false;
bool g_NoFakeVehicleCollisions = false;
bool g_AlwaysPlayFakeCollisions = false;
bool g_NoDeformationAudio = false;
bool g_NoCrashSweetener = false;
bool g_NoHeadlightSmashAudio = false;
bool g_NoVehicleScrapeAudio = false;
bool g_ApplyBoatBodyCollisionLimitingToAllBoats = false;

extern CVfxWheel g_vfxWheel;

namespace rage
{
#if __BANK
	EXT_PF_COUNTER(CollisonsProcessed);
#endif
}

extern u32 g_MinTimeBetweenCollisions;
float g_VehCollisionJumpLandMag = 0.f;
u32 g_jumpLandTimeFilter = 100;
u32 g_jumpLandScrapeTimeFilter = 250;
float g_DeformationImpactVel = 5.f;
float g_VehicleExplosionCollisionAudioVolumeBoost = 6.f;
float g_VehicleExplosionCollisionAudioRolloffBoost = 10.f;
u32 g_VehicleExplosionAudioBoostTime = 5000;
f32 g_VehicleSpeedSqForFoliage = 1.f;
u32 g_VehicleRollHoldTime = 500;
#if __BANK
bool audVehicleCollisionAudio::sm_UpdateVehicleCollisionSettings = false;
bool g_DebugVehicleCollisionAudio = false;
extern bool g_DebugHydraulicSounds;
#endif

const u32 g_DefaultVehRoll = ATSTRINGHASH("CAR_ROLL_SOUND_temp", 0x0ea4f5504);

audSoundSet audVehicleCollisionAudio::sm_SoundSet;


audCurve audVehicleCollisionAudio::sm_VehicleLandFactorCurve;
audCurve audVehicleCollisionAudio::sm_VehicleCollisionRolloffBoostCurve;
u32 audVehicleCollisionAudio::sm_TrainLoopTime = 100;
float audVehicleCollisionAudio::sm_MinSpeedForVehicleCollisions = 0.5f;
u32 audVehicleCollisionAudio::sm_TimeForBikePostMeleeImpacts = 3000;

atStaticPool<audVehicleCollisionContext, audVehicleCollisionContextList::k_MaxCollisionEvents> sm_ContextPool ;

#if !__FINAL
u32 audVehicleCollisionAudio::sm_debugEventCounter = 0;
bool audVehicleCollisionAudio::sm_debugEventListFull = false;
#endif 

audVehicleCollisionContextList::audVehicleCollisionContextList()
{
	m_ReuseList = NULL;
	m_List = NULL;
	m_FrameCount = 0;
}

void audVehicleCollisionContextList::Reset()
{
	audVehicleCollisionContext * listEvent = m_List, *nextEvent = NULL;
	m_List = NULL;  

	while(listEvent)
	{
		if(listEvent->otherEntity)
			listEvent->otherEntity->m_nFlags.bAlreadyInAudioList = 0;
		
		nextEvent = listEvent->next;
		sm_ContextPool.Delete(listEvent);

		listEvent = nextEvent;
	}

	// Always clear out the reuse list too in this case.
	ClearReuseList();
}


void audVehicleCollisionContextList::ResetToReuseList()
{
	// Special case: if the actual list is already empty, don't do anything, in
	// particular, don't clear the reuse list. This is handy if
	// ResetToReuseList() is called multiple times.
	if(!m_List)
	{
		return;
	}

	// Clear anything already on the reuse list from before, we only want it
	// to match what's currently in m_List.
	ClearReuseList();

	audVehicleCollisionContext * listEvent = m_List, *nextEvent = NULL;
	m_List = NULL;  

	// Clear bAlreadyInAudioList for everything in the main list, and push
	// on m_ReuseList. Note that we intentionally reverse the order here, so
	// that when we grab from m_ReuseList again, we will end up with the nodes
	// in the same order as they had in m_List before.
	while(listEvent)
	{
		if(listEvent->otherEntity)
			listEvent->otherEntity->m_nFlags.bAlreadyInAudioList = 0;

		nextEvent = listEvent->next;

		listEvent->next = m_ReuseList;
		m_ReuseList = listEvent;

		listEvent = nextEvent;
	}
}


void audVehicleCollisionContextList::ClearReuseList()
{
	audVehicleCollisionContext * listEvent = m_ReuseList, *nextEvent = NULL;
	m_ReuseList = NULL;  

	while(listEvent)
	{
		// When we return to the pool, we should probably clear out the registered references,
		// so we don't hold on to extra fwRefAwareBase objects that other users may have
		// to iterate through.
		listEvent->otherEntity = NULL;
#if !__FINAL
		listEvent->parent = NULL;
#endif

		nextEvent = listEvent->next;
		sm_ContextPool.Delete(listEvent);
		listEvent = nextEvent;
	}
}



audVehicleCollisionContext* audVehicleCollisionContextList::InitAndAddEventToList(CEntity* pOtherEntity, Vec3V_In velocityV, u32 typeFlags)
{
	//bAlreadyInAudioList 1 in list, may not have body collision flag, if the new context has the flag then search for it
	if(pOtherEntity && pOtherEntity->m_nFlags.bAlreadyInAudioList)
	{
		if(pOtherEntity->m_nFlags.bAlreadyInAudioList == 1 && (typeFlags & AUD_VEH_COLLISION_BODY))
		{
			audVehicleCollisionContext * listContext = m_List;
			while(listContext)
			{
				if(listContext->otherEntity == pOtherEntity) 
				{
					listContext->SetTypeFlag(AUD_VEH_COLLISION_BODY);
					return NULL;
				}

				listContext = listContext->next;
			}
		}
		else
		{
			//bAlreadyInAudioList 2 - Already in the list and no need to change flags so don't search or add
			return NULL;
		}
	}

	//bAlreadyInAudioList 0 - not in list, needs to be added

	// Try to use the reuse list first, if there is something there.
	audVehicleCollisionContext* newContext = m_ReuseList;
	if(newContext)
	{
		m_ReuseList = newContext->next;
		newContext->next = NULL;
	}
	else if(naVerifyf(!sm_ContextPool.IsFull(), "Vehicle audio collision context pool is full"))
	{
		newContext = sm_ContextPool.New();
	}
	else
	{
		// Pool full.
		return NULL;
	}

	newContext->Init(pOtherEntity, VEC3V_TO_VECTOR3(velocityV), typeFlags);
	newContext->collisionEvent.Init();

	// Link in with the list. Note that the Init() calls above clear the next
	// pointer, so this has to be done after.
	naAssert(!newContext->next);
	newContext->next = m_List;
	m_List = newContext;

	if(pOtherEntity)
	{
		pOtherEntity->m_nFlags.bAlreadyInAudioList = (typeFlags & AUD_VEH_COLLISION_BODY) ? 2 : 1;
	}

	return newContext;
}

void audVehicleCollisionContext::Init(audVehicleCollisionContext & inEvent)
{
	*this = inEvent;
}

audVehicleCollisionAudio::audVehicleCollisionAudio(audVehicleAudioEntity * parent)
{
	m_Parent = parent;
	m_ScrapeSound = NULL;
	m_SlowScrapeSound = NULL;
	m_GlassDebrisSound = NULL;
	m_PartsDebrisSound = NULL;
	m_ScrapeStopSoundMs = 0;
	m_LastGlassDebrisTime = 0;
	m_LastGlassDebrisVolLin = 0.f;
	m_LastPartsDebrisTime = 0;
	m_LastPartsDebrisVolLin = 0.f;
	m_ImpactDamage = 0.f;
	m_JumpLandMag = 0.f;
	m_LastJumpLandTime = 0;
	m_LastJumpLandScrapeTime = 0;
	m_LastCarRollTime = 0;
	m_LastCarRollVol = 0.0f;
	m_CarRollSound = NULL;
	m_VehicleCollisionSettings = NULL;
	m_CollisionMaterialSettings = NULL;
	m_FoliageLoop = NULL;
	m_TrainLoop = NULL;
	m_WreckedTime = 0;
	m_JumpStartTime = 0;
	m_PlayingFoliageThisFrame = false;
	m_ImpactThisFrame = false;
	m_WasHitByTrain = false;
	m_IsOnAnotherVehicle = false;
	m_LastTimeHitByTrain = 0;
	m_LastTrailerBumpTime = 0;
	m_LastFakeImpactTime = 0;
	m_LastBodyCollisionTime = 0;
	m_LastFakeImpactVol = -100.f;
	m_LastVehicleCollisionVolume = -100.f;
	m_LastVehicleCollisionTime = 0;
	m_LastMeleeTime = 0;
	m_FakeImpactMagThisFrame = 0.f;
	m_NeedsToPlayVehOnVehImpact = false;
}

void audVehicleCollisionAudio::InitClass()
{
	static u32 curveName = ATSTRINGHASH("VEH_LAND_FACTOR", 0x991bcad6);
	sm_VehicleLandFactorCurve.Init(curveName);
	static u32 rolloffCurveName = ATSTRINGHASH("VEH_COLLISION_ROLLOFF_BOOST", 0xB9455E41);
	sm_VehicleCollisionRolloffBoostCurve.Init(rolloffCurveName);
	StaticConditionalWarning(sm_SoundSet.Init(ATSTRINGHASH("VEH_COLLISIONS_SET", 0xAB3A5029)), "Failed to find VEH_COLLISIONS_SET sound set");

}

void audVehicleCollisionAudio::UpdateSound(audSound *UNUSED_PARAM(sound), audRequestedSettings *reqSets, u32 UNUSED_PARAM(timeInMs))
{
	u32 clientVariable;
	reqSets->GetClientVariable(clientVariable);
	audVehicleSounds soundId = (audVehicleSounds)clientVariable;

	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	switch(soundId)
	{
		case AUD_VEHICLE_SOUND_SCRAPE: 
			
			if(m_ScrapeSound)
			{
				m_ScrapeSound->SetRequestedPosition(VEC3V_TO_VECTOR3(GetVehicle()->GetTransform().GetPosition()));//m_Vehicle->TransformIntoWorldSpace(m_ClatterOffsetPos));
			}
			if(m_SlowScrapeSound)
			{
				m_SlowScrapeSound->SetRequestedPosition(VEC3V_TO_VECTOR3(GetVehicle()->GetTransform().GetPosition()));
			}
			if(now > m_ScrapeStopSoundMs)
			{
				if(m_ScrapeSound)
				{
					m_ScrapeSound->StopAndForget();
					m_ScrapeSound = NULL;
				}
				if(m_SlowScrapeSound)
				{
					m_SlowScrapeSound->StopAndForget();
					m_SlowScrapeSound = NULL;
				}
			}
			break;
		case AUD_VEHICLE_SOUND_FOLIAGE:
			if(!m_PlayingFoliageThisFrame && m_FoliageLoop)
			{
				m_FoliageLoop->StopAndForget();
			}
			m_PlayingFoliageThisFrame = false;
			break;
		default:
			break;
	}
}

void audVehicleCollisionAudio::TriggerDebrisSounds(const f32 impactVelocity)
{
	if(GetParent()->GetDrowningFactor() >= 1.f && !Water::IsCameraUnderwater())
	{
		return;
	}

	const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	VehicleCollisionSettings * settings = GetVehicleCollisionSettings();

	f32 speedVol = audCurveRepository::GetLinearInterpolatedValue(0.0f, 1.0f, settings->CollisionMin, settings->CollisionMax, impactVelocity);
	f32 angVol   = audCurveRepository::GetLinearInterpolatedValue(0.0f, 1.0f, g_MinCarRollSpeed, g_MaxCarRollSpeed, GetVehicle()->GetAngVelocity().Mag());

	audSoundInitParams initParams;
	initParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
	initParams.Tracker = GetVehicle()->GetPlaceableTracker();
	const f32 volLin = Max(angVol, speedVol);
	const f32 dbVol = audDriverUtil::ComputeDbVolumeFromLinear(volLin);

	if(!m_PartsDebrisSound)
	{
		if(timeInMs > m_LastPartsDebrisTime + 250)
		{
			f32 bodyDamageFactor = Max(m_Parent->GetScriptBodyDamageFactor(), 1.0f - (GetVehicle()->GetVehicleDamage()->GetBodyHealth() / CVehicleDamage::GetBodyHealthMax()));

			if(bodyDamageFactor > g_BodyDamageFactorForDebris)
			{
				GetParent()->CreateSound_PersistentReference(settings->PostImpactDebris, &m_PartsDebrisSound, &initParams);
				m_LastPartsDebrisTime = timeInMs;
				m_LastPartsDebrisVolLin = volLin; 
				if(m_PartsDebrisSound)
				{
					m_PartsDebrisSound->SetRequestedVolume(dbVol);
					m_PartsDebrisSound->PrepareAndPlay();
				}
			}
		}
	}
	else if(volLin > m_LastPartsDebrisVolLin)
	{
		m_LastPartsDebrisVolLin = volLin;
		m_PartsDebrisSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(dbVol));
	}
	
	if(!m_GlassDebrisSound) 
	{
		if(GetParent()->HasWindowBeenSmashed())
		{
			if(timeInMs > m_LastGlassDebrisVolLin + 250)
			{
				GetParent()->CreateSound_PersistentReference(settings->GlassDebris, &m_GlassDebrisSound, &initParams);
				m_LastGlassDebrisTime = timeInMs;
				m_LastGlassDebrisVolLin = volLin;
				if(m_GlassDebrisSound)
				{
					m_GlassDebrisSound->SetRequestedVolume(dbVol);
					m_GlassDebrisSound->PrepareAndPlay();
				}
			}
		}
	}
	else if(volLin > m_LastGlassDebrisVolLin)
	{
		m_LastGlassDebrisVolLin = volLin;
		m_GlassDebrisSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(dbVol));
	}
}

CVehicle * audVehicleCollisionAudio::GetVehicle()
{
	return GetParent()->GetVehicle();
}

void audVehicleCollisionAudio::TriggerUpsideDownRoll(const f32 impulseMag)
{
	const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	// turn the impulse and speed into a volume - don't retrigger too often, but up the volume if the impulse,speed increase
	if(impulseMag > g_MinCarRollImpulseMag && GetVehicle()->GetVelocity().Mag() > g_MinCarRollSpeed)
	{
		if(!m_CarRollSound)
		{
			if(timeInMs > m_LastCarRollTime + 250)
			{
				//				f32 angularVelocity = m_Vehicle->GetTurnSpeed().Mag();
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
				initParams.Tracker = GetVehicle()->GetPlaceableTracker();

				if(GetVehicleCollisionSettings())
				{
					GetParent()->CreateSound_PersistentReference(GetVehicleCollisionSettings()->RollSound, &m_CarRollSound, &initParams);
				}
				
				m_LastCarRollVol = 0.f;
				m_LastCarRollTime = timeInMs;
				if(m_CarRollSound)
				{
					m_CarRollSound->SetRequestedVolume(0.f);
					m_CarRollSound->PrepareAndPlay();
				}
				//				Warningf("Roll: imp: %.2f; sp: %.2f; av: %.2f", impulseMag, m_Vehicle->GetVelocity().Mag(), angularVelocity);
			}			
		}
		if(m_CarRollSound)
		{
			f32 impulseVol = audCurveRepository::GetLinearInterpolatedValue(0.0f, 1.0f, g_MinCarRollImpulseMag, g_MaxCarRollImpulseMag, impulseMag);
			f32 speedVol   = audCurveRepository::GetLinearInterpolatedValue(0.0f, 1.0f, g_MinCarRollSpeed, g_MaxCarRollSpeed, GetVehicle()->GetVelocity().Mag());
			const f32 vol = impulseVol * speedVol;
			if(vol>m_LastCarRollVol)
			{
				m_LastCarRollVol = vol;
				m_CarRollSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(vol));
			}
		}
	}
}

void audVehicleCollisionAudio::ProcessImpactBoat(phContactIterator& impacts)
{
	// Cache some static values out
	CVehicle * pVehicle = GetVehicle();
	const Vec3V velocityV = VECTOR3_TO_VEC3V(pVehicle->GetVelocity());
	// const Vec3V transformC = pVehicle->GetTransform().GetC();
	const Mat34V& mtrx = pVehicle->GetMatrixRef();

	const Vec3V transformAV = mtrx.GetCol0();
	const Vec3V transformBV = mtrx.GetCol1();
	const Vec3V transformCV = mtrx.GetCol2();

	const phContact & contact = impacts.GetContact();
	const Vec3V contactWorldNormal = contact.GetWorldNormal();

	Vec3V myNormalV;
	impacts.GetMyNormal(myNormalV);

	// Take these vectors
	//		myNormalV
	//		contactWorldNormal
	//		contactWorldNormal
	//		velocityV
	// and transpose them so that all their X components are in one Vec4V, all the Y
	// components in another, etc.
	const Vec4V temp0BV = MergeXY(Vec4V(myNormalV), Vec4V(contactWorldNormal));
	const Vec4V temp1BV = MergeXY(Vec4V(contactWorldNormal), Vec4V(velocityV));
	const Vec4V temp2BV = MergeZW(Vec4V(myNormalV), Vec4V(contactWorldNormal));
	const Vec4V temp3BV = MergeZW(Vec4V(contactWorldNormal), Vec4V(velocityV));
	const Vec4V normalMyContactContactVelXV = MergeXY(temp0BV, temp1BV);
	const Vec4V normalMyContactContactVelYV = MergeZW(temp0BV, temp1BV);
	const Vec4V normalMyContactContactVelZV = MergeXY(temp2BV, temp3BV);

	// Take these vectors
	//		transformAV
	//		transformBV
	//		transformCV
	//		transformCV
	// and transpose them so that all their X components are in one Vec4V, all the Y
	// components in another, etc.
	const Vec4V temp0AV = MergeXY(Vec4V(transformAV), Vec4V(transformCV));
	const Vec4V temp1AV = MergeXY(Vec4V(transformBV), Vec4V(transformCV));
	const Vec4V temp2AV = MergeZW(Vec4V(transformAV), Vec4V(transformCV));
	const Vec4V temp3AV = MergeZW(Vec4V(transformBV), Vec4V(transformCV));
	const Vec4V transformABCCXV = MergeXY(temp0AV, temp1AV);
	const Vec4V transformABCCYV = MergeZW(temp0AV, temp1AV);
	const Vec4V transformABCCZV = MergeXY(temp2AV, temp3AV);

	// Compute four dot products:
	//		myNormalV*transformAV
	//		contactWorldNormal*transformBV
	//		contactWorldNormal*transformCV
	//		velocityV*transformCV
	const Vec4V dotXV = Scale(transformABCCXV, normalMyContactContactVelXV);
	const Vec4V dotXYV = AddScaled(dotXV, transformABCCYV, normalMyContactContactVelYV);
	const Vec4V dotV = AddScaled(dotXYV, transformABCCZV, normalMyContactContactVelZV);

	// We need the absolute values of some of the dot products.
	const Vec4V absDotV = Abs(dotV);

	u32 impactType = AUD_VEH_COLLISION_BODY;

	const ScalarV contactDotThresholdV = LoadScalar32IntoScalarV(g_BoatBottomContactDotThreshold);


	const ScalarV absContactDotCV = absDotV.GetZ();
	if(IsGreaterThanAll(absContactDotCV, contactDotThresholdV))
	{
		impactType |= AUD_VEH_COLLISION_BOTTOM;
	}

	CEntity* pOtherEntity = CPhysics::GetEntityFromInst(impacts.GetOtherInstance());

	audVehicleCollisionContext* pImpactContext = m_CollisionEventsList.InitAndAddEventToList(pOtherEntity, velocityV, impactType);
	if(pImpactContext)
	{
#if !__FINAL
		pImpactContext->parent = pVehicle;
#endif
	}	


#if !__FINAL
	if(sm_ContextPool.GetFreeCount() < 20)
	{
		naDisplayf("Vehicle collision context pool getting full, only %d slots left, frame %d time %d", sm_ContextPool.GetFreeCount(), fwTimer::GetFrameCount(), audNorthAudioEngine::GetCurrentTimeInMs());
	}

	if(sm_ContextPool.IsFull() && !sm_debugEventListFull) //Extra debug info to try and hunt down a rare assert
	{
		sm_debugEventListFull = true;
		sm_debugEventCounter = 100;
		naDisplayf("Context pool is full, frame: %d, time %d", fwTimer::GetFrameCount(), audNorthAudioEngine::GetCurrentTimeInMs());
		for(int i=0; i < audVehicleCollisionContextList::k_MaxCollisionEvents; i++)
		{
			audVehicleCollisionContext & context = sm_ContextPool.GetElement(i);
			naDisplayf("Index %d, entity %p, model %s, other entity %p, other model %s", i, context.parent.Get(), context.parent->GetModelName(), context.otherEntity.Get(), context.otherEntity.Get() ? context.otherEntity->GetModelName() : "NONE");
		}
	}
	else if(!sm_ContextPool.IsFull() && sm_debugEventCounter > 0)
	{
		naDisplayf("Context pool is NOT full: free count (%d)/(%d), frame: %d, entity %p, model %s, other entity %p, other model %s", sm_ContextPool.GetFreeCount(), audVehicleCollisionContextList::k_MaxCollisionEvents, fwTimer::GetFrameCount(), pVehicle, pVehicle->GetModelName(), pOtherEntity, pOtherEntity ? pOtherEntity->GetModelName() : "NONE");
		sm_debugEventCounter--;
		sm_debugEventListFull = false;
	}
#endif

}

void audVehicleCollisionAudio::ProcessFoliageImpact(f32 drag)
{
	if(!GetVehicle()->GetDriver() || GetVehicle()->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle))
	{
		return;
	}
	m_Foliage.numCollisions++;
	m_Foliage.drag += drag;
}


bool audVehicleCollisionAudio::StartProcessImpacts()
{
	// Get the current frame from the event list early to avoid LHS stall
	u32 frameCount = fwTimer::GetFrameCount();
	u32 listFrameCount = m_CollisionEventsList.GetFrameCount();

	// adding the heli check allows burnt out heli bodies to make ground impact sounds
	if(GetParent()->IsDisabled())
	{
		return false;
	}

	if(frameCount > listFrameCount)
	{
		m_CollisionEventsList.ResetToReuseList();
		m_CollisionEventsList.SetFrameCount(frameCount);
	}

	return true;
}


void audVehicleCollisionAudio::EndProcessImpacts()
{
	m_CollisionEventsList.ClearReuseList();
}


// Defined in 'Vehicles/wheel.cpp':
extern Vec4V g_WheelImpactNormalSideLimitsV;

void audVehicleCollisionAudio::ProcessImpact(phCachedContactConstIterator& impacts)
{
	// Cache some static values out
	CVehicle * pVehicle = GetVehicle();
	const Vec3V velocityV = VECTOR3_TO_VEC3V(pVehicle->GetVelocity());
	const Mat34V& mtrx = pVehicle->GetMatrixRef();

	const Vec3V transformAV = mtrx.GetCol0();
	const Vec3V transformBV = mtrx.GetCol1();
	const Vec3V transformCV = mtrx.GetCol2();
	const int numWheels = pVehicle->GetNumWheels();
	const CWheel * const * ppWheels = pVehicle->GetWheels();

	CEntity* pOtherEntity = CPhysics::GetEntityFromInst(impacts.GetOtherInstance());

	const phContact & contact = impacts.GetContact();
	const Vec3V contactWorldNormal = contact.GetWorldNormal();

	Vec3V myNormalV;
	impacts.GetMyNormal(myNormalV);

	// Take these vectors
	//		myNormalV
	//		contactWorldNormal
	//		contactWorldNormal
	//		velocityV
	// and transpose them so that all their X components are in one Vec4V, all the Y
	// components in another, etc.
	const Vec4V temp0BV = MergeXY(Vec4V(myNormalV), Vec4V(contactWorldNormal));
	const Vec4V temp1BV = MergeXY(Vec4V(contactWorldNormal), Vec4V(velocityV));
	const Vec4V temp2BV = MergeZW(Vec4V(myNormalV), Vec4V(contactWorldNormal));
	const Vec4V temp3BV = MergeZW(Vec4V(contactWorldNormal), Vec4V(velocityV));
	const Vec4V normalMyContactContactVelXV = MergeXY(temp0BV, temp1BV);
	const Vec4V normalMyContactContactVelYV = MergeZW(temp0BV, temp1BV);
	const Vec4V normalMyContactContactVelZV = MergeXY(temp2BV, temp3BV);

	// Take these vectors
	//		transformAV
	//		transformBV
	//		transformCV
	//		transformCV
	// and transpose them so that all their X components are in one Vec4V, all the Y
	// components in another, etc.
	const Vec4V temp0AV = MergeXY(Vec4V(transformAV), Vec4V(transformCV));
	const Vec4V temp1AV = MergeXY(Vec4V(transformBV), Vec4V(transformCV));
	const Vec4V temp2AV = MergeZW(Vec4V(transformAV), Vec4V(transformCV));
	const Vec4V temp3AV = MergeZW(Vec4V(transformBV), Vec4V(transformCV));
	const Vec4V transformABCCXV = MergeXY(temp0AV, temp1AV);
	const Vec4V transformABCCYV = MergeZW(temp0AV, temp1AV);
	const Vec4V transformABCCZV = MergeXY(temp2AV, temp3AV);

	// Compute four dot products:
	//		myNormalV*transformAV
	//		contactWorldNormal*transformBV
	//		contactWorldNormal*transformCV
	//		velocityV*transformCV
	const Vec4V dotXV = Scale(transformABCCXV, normalMyContactContactVelXV);
	const Vec4V dotXYV = AddScaled(dotXV, transformABCCYV, normalMyContactContactVelYV);
	const Vec4V dotV = AddScaled(dotXYV, transformABCCZV, normalMyContactContactVelZV);

	// We need the absolute values of some of the dot products.
	const Vec4V absDotV = Abs(dotV);


	u32 impactType = AUD_VEH_COLLISION_BODY;

	const ScalarV contactDotThresholdV(0.6f);

	const ScalarV absContactDotCV = absDotV.GetZ();
	if(IsGreaterThanAll(absContactDotCV, contactDotThresholdV))
	{
		impactType |= AUD_VEH_COLLISION_BOTTOM;
	}
	else
	{
		const ScalarV absContactDotBV = absDotV.GetY();
		if(IsGreaterThanAll(absContactDotBV, contactDotThresholdV))
		{
			m_ImpactThisFrame = true;

			if(pOtherEntity && pOtherEntity->GetIsTypePed())
			{
				((CPed*)pOtherEntity)->GetPedAudioEntity()->NotifyVehicleContact(GetVehicle());
			}

			if(GetParent()->GetAudioVehicleType() == AUD_VEHICLE_TRAIN && ((CTrain*)pVehicle)->IsEngine() && pOtherEntity && pOtherEntity->GetIsTypeVehicle())
			{
				audVehicleCollisionAudio& otherAudio = ((CVehicle*)(pOtherEntity))->GetVehicleAudioEntity()->GetCollisionAudio();
				if(!otherAudio.m_WasHitByTrain && ! otherAudio.m_IsOnAnotherVehicle)
				{
					// What's the purpose of this stuff? Normalizing the vectors,
					// computing a dot product, and checking if it's less than exactly one
					// doesn't seem to make sense - seems like that just always
					// passes except for if they happen to be exactly aligned,
					// except that it will be pretty sensitive to numerical errors, in particular
					// since we use NormalizeFast(). /FF

					Vec3V thisNormal = NormalizeFast(velocityV);
					Vec3V otherNormal = NormalizeFast(VECTOR3_TO_VEC3V(otherAudio.GetVehicle()->GetVelocity()));
					if(Dot(thisNormal, otherNormal).Getf() < 1.f)
					{
						otherAudio.m_WasHitByTrain = true;
					}
				}
			}
		}
	}

	//Process wheel audio
	if(m_Parent->GetVehicleModelNameHash() != ATSTRINGHASH("DELUXO", 0x586765FB) || m_Parent->GetVehicle()->GetSpecialFlightModeRatio() == 0.f)
	{
		const int myComponent = impacts.GetMyComponent();
		for(int i=0; i < numWheels; i++)
		{
			const CWheel * wheel = ppWheels[i];
			if(myComponent == wheel->GetFragChild())
			{
				impactType &= ~AUD_VEH_COLLISION_BODY;

				// Adapted from the old CWheel::ImpactIsOnSide():

				int nOtherInstType = PH_INST_GTA;
				phInst* pOtherInst = impacts.GetOtherInstance();
				if(pOtherInst)
				{
					nOtherInstType = pOtherInst->GetClassType();
				}

				const Vec4V wheelImpactNormalSideLimitsV = g_WheelImpactNormalSideLimitsV;

				ScalarV sideNormalLimitV;
				if(wheel->GetConfigFlags().IsFlagSet(WCF_BIKE_FALLEN_COLLIDER))
				{
					sideNormalLimitV = wheelImpactNormalSideLimitsV.GetZ();
				}
				else if(nOtherInstType==PH_INST_VEH || nOtherInstType==PH_INST_FRAG_VEH)
				{
					sideNormalLimitV = wheelImpactNormalSideLimitsV.GetY();
				}
				else
				{
					sideNormalLimitV = wheelImpactNormalSideLimitsV.GetX();
				}

				const ScalarV sideNormalV = absDotV.GetX();
				if(IsLessThanOrEqualAll(sideNormalV, sideNormalLimitV))
				{

					if(impactType & AUD_VEH_COLLISION_BOTTOM)
					{
						const ScalarV absDotVelCV = absDotV.GetW();

						impactType |= AUD_VEH_COLLISION_WHEEL_BOTTOM;

						const ScalarV jumpLandMagOldV = LoadScalar32IntoScalarV(m_JumpLandMag);
						const ScalarV jumpLandMagNewV = Max(jumpLandMagOldV, absDotVelCV);
						StoreScalar32FromScalarV(m_JumpLandMag, jumpLandMagNewV);

						if(pOtherEntity && pOtherEntity->GetIsTypeVehicle())
						{
							audVehicleCollisionAudio& otherAudio = ((CVehicle*)(pOtherEntity))->GetVehicleAudioEntity()->GetCollisionAudio();
							otherAudio.m_WasHitByTrain = false;			
							otherAudio.m_IsOnAnotherVehicle = true;
						}
					}
				}
				else
				{
					if(nOtherInstType==PH_INST_FRAG_VEH && !((CVehicleModelInfo*)GetVehicle()->GetBaseModelInfo())->GetIsTrailer() && !(pOtherEntity && pOtherEntity->GetIsTypeVehicle() && (((CVehicle*)pOtherEntity)->GetVehicleAudioEntity()->IsToyCar() || ((CVehicleModelInfo*)((CVehicle*)pOtherEntity)->GetBaseModelInfo())->GetIsTrailer())))
					{
						Fakeimpact(pVehicle->GetVelocity().Mag(), 1.f, true);

#if __BANK
						if(g_DebugVehicleCollisionAudio)
						{
							grcDebugDraw::Sphere(GetVehicle()->GetTransform().GetPosition() - GetVehicle()->GetTransform().GetA() + Vec3V(ScalarV(0.0f), ScalarV(0.f), ScalarV(1.5f)), 0.5f, Color32(255, 0, 255), true, 5);
						}
#endif
					}

					impactType |= AUD_VEH_COLLISION_WHEEL_SIDE;
				}
				break;
			}
		}
	}

	audVehicleCollisionContext* pImpactContext = m_CollisionEventsList.InitAndAddEventToList(pOtherEntity, velocityV, impactType);
	if(pImpactContext)
	{
		audVehicleCollisionContext& impactContext = *pImpactContext;

#if !__FINAL
		impactContext.parent = pVehicle;
#endif
		//Ped manifold collisions don't trigger on bikes so we have to fake it
		if((pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE )
			&& pOtherEntity && pOtherEntity->GetIsTypePed() && pVehicle->GetDriver() && pOtherEntity != pVehicle->GetDriver())
		{
			impactContext.canPlayCollision = true;
			impactContext.impactVelocity = Mag(velocityV).Getf();
			if(!FPIsFinite(impactContext.impactVelocity))
			{
				impactContext.impactVelocity = impactContext.impactMag;
				if(!FPIsFinite(impactContext.impactVelocity))
				{
					impactContext.impactVelocity = 0.f;
				}
			}
			impactContext.impactMag = impactContext.impactVelocity;
			impactContext.impactVolume = 0.f;
			impactContext.collisionEvent.component = (u16)impacts.GetMyComponent();
			impactContext.collisionEvent.impactMag = impactContext.impactMag;
			impactContext.collisionEvent.normal = contactWorldNormal;
			impactContext.collisionEvent.otherComponent = (u16)impacts.GetOtherComponent();
			impactContext.collisionEvent.pos = impacts.GetMyPosition();
			impactContext.collisionEvent.otherPos = impacts.GetOtherPosition();
			impactContext.collisionEvent.otherMaterialId = impacts.GetOtherMaterialId();
		}
	}


#if !__FINAL
	if(sm_ContextPool.GetFreeCount() < 20)
	{
		naDisplayf("Vehicle collision context pool getting full, only %d slots left, frame %d time %d", sm_ContextPool.GetFreeCount(), fwTimer::GetFrameCount(), audNorthAudioEngine::GetCurrentTimeInMs());
	}

	if(sm_ContextPool.IsFull() && !sm_debugEventListFull) //Extra debug info to try and hunt down a rare assert
	{
		sm_debugEventListFull = true;
		sm_debugEventCounter = 100;
		naDisplayf("Context pool is full, frame: %d, time %d", fwTimer::GetFrameCount(), audNorthAudioEngine::GetCurrentTimeInMs());
		for(int i=0; i < audVehicleCollisionContextList::k_MaxCollisionEvents; i++)
		{
			audVehicleCollisionContext & context = sm_ContextPool.GetElement(i);
			naDisplayf("Index %d, entity %p, model %s, other entity %p, other model %s", i, context.parent.Get(), context.parent->GetModelName(), context.otherEntity.Get(), context.otherEntity.Get() ? context.otherEntity->GetModelName() : "NONE");
		}
	}
	else if(!sm_ContextPool.IsFull() && sm_debugEventCounter > 0)
	{
		naDisplayf("Context pool is NOT full: free count (%d)/(%d), frame: %d, entity %p, model %s, other entity %p, other model %s", sm_ContextPool.GetFreeCount(), audVehicleCollisionContextList::k_MaxCollisionEvents, fwTimer::GetFrameCount(), pVehicle, pVehicle->GetModelName(), pOtherEntity, pOtherEntity ? pOtherEntity->GetModelName() : "NONE");
		sm_debugEventCounter--;
		sm_debugEventListFull = false;
	}
#endif
}

void audVehicleCollisionAudio::ProcessContact(VfxCollisionInfo_s &UNUSED_PARAM(collisionInfo), phManifold & manifold, CEntity * otherEntity, const audVehicleCollisionEvent & collisionEvent)
{
	audVehicleCollisionContext * playContext = m_CollisionEventsList.GetEvents();

//#if __BANK
//	if(g_DebugVehicleCollisionAudio)
//	{
//		grcDebugDraw::Sphere(GetVehicle()->GetTransform().GetPosition() - GetVehicle()->GetTransform().GetA() + Vec3V(ScalarV(0.0f), ScalarV(0.f), ScalarV(1.5f)), 0.5f, Color32(255, 0, 255), true, 5);
//	}
//#endif


	if(GetVehicle()->GetVehicleType() == VEHICLE_TYPE_BICYCLE && g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) < (m_LastMeleeTime + sm_TimeForBikePostMeleeImpacts))
	{
		Fakeimpact(collisionEvent.impactMag, 1.f, true);
	}

	while(playContext)
	{
		if (playContext->otherEntity == otherEntity)
		{
			if(IsBike() && m_Parent->GetVehicle()->GetSpecialFlightModeRatio() == 0.0f && playContext->GetTypeFlag(AUD_VEH_COLLISION_BOTTOM))
			{
				break;
			}

			if(IsTrailer())
			{
				if(playContext->otherEntity && playContext->otherEntity->GetIsTypeVehicle())
				{
					CVehicle *vehicle = (CVehicle*)(playContext->otherEntity.Get());
					if(vehicle->GetAttachedTrailer() == GetVehicle())
					{
						break;
					}
				}
			}

			f32 scrapeMag = collisionEvent.scrapeMag * g_CollisionAudioEntity.GetAudioWeightScaling(g_CollisionAudioEntity.GetAudioWeight(otherEntity), AUDIO_WEIGHT_VH);

			if(!FPIsFinite(scrapeMag))
			{
				break;
			}
			
			if(otherEntity->GetIsTypePed())
			{
				scrapeMag = 0.f;
			}

			

			f32 vehicleSpeed = m_Parent->GetVehicle()->GetVelocity().Mag();

			if(otherEntity->GetIsTypeVehicle())
			{
				if(vehicleSpeed < sm_MinSpeedForVehicleCollisions)
				{
					CVehicle * otherVehicle = (CVehicle*)otherEntity;
					if(otherVehicle->GetVelocity().Mag() < sm_MinSpeedForVehicleCollisions)
					{
						break;
					}
				}
				playContext->SetTypeFlag(AUD_VEH_COLLISION_ON_VEHICLE);
			}

			if(IsBike() && (!otherEntity || otherEntity->GetIsTypeBuilding() || (!GetVehicle()->GetDriver() && otherEntity->GetIsTypePed())))
			{
				if(vehicleSpeed < collisionEvent.impactMag || (otherEntity->GetIsTypePed() && !((CPed*)otherEntity)->GetUsingRagdoll()))
				{
					break;
				}
			}

			if(otherEntity->GetIsTypeBuilding() &&
				(GetVehicle()->GetVehicleType() == VEHICLE_TYPE_CAR &&
				(GetVehicle()->IsUpsideDown())))
			{
				playContext->SetTypeFlag(AUD_VEH_COLLISION_UPSIDEDOWN);
				GetParent()->SetIsOnGround();
			}
			u32 doorIndex = ~0U;
			if(otherEntity->GetType() == ENTITY_TYPE_OBJECT && !otherEntity->IsBaseFlagSet(fwEntity::IS_FIXED) &&
				((CObject*)otherEntity)->IsADoor() &&
				(((CDoor*)otherEntity)->IsCreakingDoorType()))
			{
				doorIndex = 0;
			}

			if(doorIndex != ~0U)
			{
				// we have a ped/door collision:
				// these are now handled in the dooraudioentity
			}


			if((playContext->GetTypeFlag(AUD_VEH_COLLISION_BODY) || playContext->GetTypeFlag(AUD_VEH_COLLISION_WHEEL_SIDE)
				|| (playContext->otherEntity && playContext->otherEntity->GetIsTypePed()) ) 
				&& (!g_CollisionAudioEntity.IsScrapeFromMagnitudes(collisionEvent.impactMag, scrapeMag) || 
				vehicleSpeed > g_ScrapeImpactVel))
			{
				bool isOkayToPlayImpacts = g_CollisionAudioEntity.IsOkToPlayImpacts(manifold);
				isOkayToPlayImpacts &= IsOkToPlayImpacts(playContext);

				if(isOkayToPlayImpacts)
				{
					playContext->canPlayCollision = true;
				}
				if(playContext->canPlayCollision && collisionEvent.impactMag > playContext->impactMag)
				{
					playContext->collisionEvent.Init(collisionEvent); 
					playContext->impactMag = collisionEvent.impactMag;
					playContext->impactVelocity = vehicleSpeed;
					if(!FPIsFinite(playContext->impactVelocity))
					{
						playContext->impactVelocity = 0.f;
					}
				}
			}

			if(g_CollisionAudioEntity.IsScrapeFromMagnitudes(collisionEvent.impactMag, scrapeMag)) 
			{		
				ProcessScrape(playContext, manifold, collisionEvent);
			}
			break;
		}

		playContext = playContext->next;
	}
}

bool audVehicleCollisionAudio::IsOkToPlayImpacts(audVehicleCollisionContext* playContext)
{
	if (playContext && m_Parent && m_Parent->GetVehicle()->InheritsFromBoat())
	{
		// Prevent boats from spamming body collisions
		if (playContext->GetTypeFlag(AUD_VEH_COLLISION_BODY))
		{
			u32 lastCollision = m_LastBodyCollisionTime;
			u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
			m_LastBodyCollisionTime = currentTime;

			u32 timeDelta = currentTime - lastCollision;
			
			// url:bugstar:6760524 - Boat Audio - VEH_DYNAMIC_IMPACTS sounds are spamming when you beach boats
			if (timeDelta < g_MinTimeBetweenCollisions && (m_Parent->GetVehicle()->GetModelNameHash() == ATSTRINGHASH("PATROLBOAT", 0xEF813606) || g_ApplyBoatBodyCollisionLimitingToAllBoats))
			{
				return false;
			}

			const f32 velocity = playContext->velocity.Mag2();

			if (velocity < 0.25f)
			{
				return false;
			}
		}
	}

	return true;
}

VehicleCollisionSettings * audVehicleCollisionAudio::GetVehicleCollisionSettings() 
{
	if(BANK_ONLY(!sm_UpdateVehicleCollisionSettings &&) m_VehicleCollisionSettings)
	{
		return m_VehicleCollisionSettings;
	}
	m_VehicleCollisionSettings = m_Parent->GetVehicleCollisionSettings();

	if(naVerifyf(m_VehicleCollisionSettings, "Couldn't get vehicle collision settings for vehicle %s", GetParent()->GetVehicle()->GetModelName()))
	{
		m_SlowScrapeVolCurve.Init(m_VehicleCollisionSettings->SlowScrapeVolCurve);
		m_ScrapeImpactCurve.Init(m_VehicleCollisionSettings->ScrapeImpactVolCurve);
		m_SlowScrapeImpactCurve.Init(m_VehicleCollisionSettings->SlowScrapeImpactCurve);
		m_ScrapePitchCurve.Init(m_VehicleCollisionSettings->ScrapePitchCurve);
		m_ScrapeVolCurve.Init(m_VehicleCollisionSettings->ScrapeVolCurve);
	}
	else
	{
		m_VehicleCollisionSettings = audNorthAudioEngine::GetObject<VehicleCollisionSettings>(ATSTRINGHASH("VEHICLE_COLLISION_CAR", 0xC2FB47));
	}

	return m_VehicleCollisionSettings;
}

void audVehicleCollisionAudio::ProcessScrape(audVehicleCollisionContext * scrapeContext, phManifold& manifold, const audVehicleCollisionEvent & scrapeEvent)
{


	audCollisionEvent *collisionEvent = NULL;
	bool hasFoundEvent = false;

	collisionEvent = g_CollisionAudioEntity.GetCollisionEventFromHistory(manifold); 

	if(collisionEvent)
	{
		u16 eventIndex;
		collisionEvent = g_CollisionAudioEntity.GetRollCollisionEventFromHistory(GetVehicle(), scrapeContext->otherEntity, &eventIndex);
		if(collisionEvent)
		{
			g_CollisionAudioEntity.SetManifoldUserDataIndex(manifold, eventIndex);
		}
	}

	//Do we have this event in the history?
	if (collisionEvent)
	{
		const bool areEntities00Matched = (collisionEvent->entities[0] == scrapeContext->otherEntity && collisionEvent->components[0] == scrapeEvent.otherComponent);
		const bool areEntities11Matched = (collisionEvent->entities[1] == GetVehicle() && collisionEvent->components[1] == scrapeEvent.component);

		if(areEntities00Matched && areEntities11Matched)
		{
			//@@: location AUDVEHICLECOLLISIONAUDIO_PROCESSSCRAPE_FOUND_EVENT
			hasFoundEvent = true;
		}
	}

	if(!hasFoundEvent)
	{
		u16 eventIndex;
		//We don't already have an event for collisions between these entities, so create a new one.
		collisionEvent = g_CollisionAudioEntity.GetFreeCollisionEventFromHistory(&eventIndex);

		if(collisionEvent == NULL)
		{
			naWarningf("The audio collision event history is full!");
			return;
		}
		g_CollisionAudioEntity.SetManifoldUserDataIndex(manifold, eventIndex);
	}

	collisionEvent->entities[0] = scrapeContext->otherEntity;
	collisionEvent->entities[1] = GetVehicle();
	collisionEvent->positions[0] = VEC3V_TO_VECTOR3(scrapeEvent.otherPos);
	collisionEvent->components[0] = scrapeEvent.otherComponent;
	collisionEvent->components[1] = scrapeEvent.component;
	collisionEvent->materialIds[0] = scrapeEvent.otherMaterialId;
	collisionEvent->type = AUD_COLLISION_TYPE_SCRAPE;

	collisionEvent->scrapeMagnitudes[0] = scrapeEvent.scrapeMag;
	collisionEvent->scrapeMagnitudes[1] = 0.f;

	phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(scrapeEvent.otherMaterialId);
	
	if(unpackedMtlIdA >= PGTAMATERIALMGR->GetNumMaterials())
	{
		return; //Invalid material id bail out
	}

	collisionEvent->materialSettings[0] = g_CollisionAudioEntity.GetMaterialOverride(scrapeContext->otherEntity, g_audCollisionMaterials[(s32)unpackedMtlIdA], scrapeEvent.otherComponent);
	collisionEvent->materialSettings[1] = NULL;
	
	if(scrapeContext->GetTypeFlag(AUD_VEH_COLLISION_BODY) && ((GetVehicle() && GetVehicle()->GetModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70)) || (collisionEvent->materialSettings[0] && collisionEvent->materialSettings[0]->HardImpact != g_NullSoundHash)))
	{
		scrapeContext->SetTypeFlag(AUD_VEH_COLLISION_SCRAPE);  
		if(scrapeEvent.scrapeMag > scrapeContext->scrapeMag)
		{
			scrapeContext->scrapeMag = scrapeEvent.scrapeMag;
		}
		if(!scrapeContext->otherEntity->GetIsTypeVehicle())
		{            
            if(m_Parent->HydraulicsModifiedRecently())
            {
                if(!scrapeContext->otherEntity->GetIsTypeObject() && !scrapeContext->otherEntity->GetIsTypePed())
                {
                    scrapeContext->scrapeMag = 0.f;
                    return;
                }                    
            }

			scrapeContext->scrapeMag *= g_CollisionAudioEntity.GetAudioWeightScaling(AUDIO_WEIGHT_VH, g_CollisionAudioEntity.GetAudioWeight(scrapeContext->otherEntity));
		}
	}

	if(scrapeContext->otherEntity->GetIsTypeVehicle())
	{
		return;
	}

	g_CollisionAudioEntity.ProcessScrapeSounds(collisionEvent);

	collisionEvent->scrapeStopTimeMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + g_ScrapingCollisionHoldTimeMs;

}

void audVehicleCollisionAudio::ComputePitchAndVolumeFromScrapeSpeed(audCollisionEvent *collisionEvent, u32 index, s32 &pitch, f32 &volume)
{
	pitch = 0;
	volume = g_SilenceVolume;

	CollisionMaterialSettings *materialSettings = collisionEvent->materialSettings[index];
	CEntity * entity = collisionEvent->entities[index];
	VehicleCollisionSettings * vehicleSettings = GetVehicleCollisionSettings();

	if(materialSettings)
	{
		bool isMap = entity->GetCurrentPhysicsInst() && entity->GetCurrentPhysicsInst()->GetClassType() == PH_INST_MAPCOL;

		if(entity->GetIsTypePed())
		{
			return;
		}

		f32 minScrapeSpeed = materialSettings->MinScrapeSpeed;
		f32 maxScrapeSpeed = materialSettings->MaxScrapeSpeed;

		if(!naVerifyf(minScrapeSpeed != maxScrapeSpeed, "MinsScrapeSpeed and MaxScrapeSpeed are the same for CollisionMaterialSettings %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings->NameTableOffset)))
		{
			maxScrapeSpeed += 0.1f;
		}

		if(isMap)
		{
			minScrapeSpeed = vehicleSettings->CollisionMin;
			maxScrapeSpeed = vehicleSettings->CollisionMax;

			if(!naVerifyf(minScrapeSpeed != maxScrapeSpeed, "MinsScrapeSpeed and MaxScrapeSpeed are the same for VehicleCollisionSettings %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(vehicleSettings->NameTableOffset)))
			{
				maxScrapeSpeed += 0.1f;
			}
		}

		//Scale the scrape speed to a 0->1 range.
		f32 scaledScrapeSpeed = ClampRange((collisionEvent->scrapeMagnitudes[index]- minScrapeSpeed) / (maxScrapeSpeed-minScrapeSpeed), 0.f, 1.0f);

		if(!naVerifyf(FPIsFinite(scaledScrapeSpeed), "Got a NAN in ComputePitchAndVolumeFromScrapeSpeed"))
		{
			scaledScrapeSpeed = 0.f;
		}

		//Derive a pitch from a curve.
		if(isMap)
		{
			pitch = audDriverUtil::ConvertRatioToPitch(m_ScrapePitchCurve.CalculateValue(scaledScrapeSpeed));
		}
		else if (AUD_GET_TRISTATE_VALUE(collisionEvent->materialSettings[index]->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES) != AUD_TRISTATE_TRUE)
		{
			pitch = audDriverUtil::ConvertRatioToPitch(g_CollisionAudioEntity.GetScrapePitchCurve().CalculateValue(scaledScrapeSpeed));
		}
		else
		{
			audCurve curve;
			if(curve.Init(materialSettings->ScrapePitchCurve))
			{
				pitch = audDriverUtil::ConvertRatioToPitch(curve.CalculateValue(scaledScrapeSpeed));
			}
		}

		//Derive a volume from a curve.
		if(isMap)
		{
			f32 volumeLinear = m_ScrapeVolCurve.CalculateValue(scaledScrapeSpeed);
			volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);									
		}
		else if (AUD_GET_TRISTATE_VALUE(collisionEvent->materialSettings[index]->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES) != AUD_TRISTATE_TRUE)
		{
			f32 volumeLinear = g_CollisionAudioEntity.GetScrapeVolCurve().CalculateValue(scaledScrapeSpeed);
			volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);			
		}
		else
		{
			audCurve curve;
			if(curve.Init(materialSettings->ScrapeVolCurve))
			{
				f32 volumeLinear = curve.CalculateValue(scaledScrapeSpeed);
				volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);
			}
		}
	}
	else
	{
		pitch = 0;
		volume = -100.f;
	}
}


void audVehicleCollisionAudio::ProcessCollision(audVehicleCollisionContext * impactContext, const audVehicleCollisionEvent & collisionEvent)
{
	VehicleCollisionSettings * settings = GetVehicleCollisionSettings();

	CEntity * otherEntity = impactContext->otherEntity;

	Vector3 posB = VEC3V_TO_VECTOR3(collisionEvent.otherPos);

	AudioWeight weight, otherWeight;
	weight = AUDIO_WEIGHT_VH; //TODO have this in the settings

	f32 weightScaling, otherWeightScaling;

	if(!FPIsFinite(collisionEvent.impactMag)) 
	{
		return;
	}

	// The thruster rests on some spindly legs, so big collisions against the base sound wrong - just rely on suspension sounds
	if(m_Parent && m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("Thruster", 0x58CDAF30) && impactContext->GetTypeFlag(AUD_VEH_COLLISION_BOTTOM))
	{
		return;
	}

	f32 impactMag = collisionEvent.impactMag;
	f32 otherImpactMag = collisionEvent.impactMag;

	f32 impactVel = impactContext->impactVelocity;

	naAssert(impactContext->parent->GetIsTypeVehicle());

	bool otherIsVehicle = false;

	if( otherEntity && otherEntity->GetIsTypePed())
	{
		CPed * ped = (CPed *)otherEntity;
		if(GetVehicle() == ped->GetVehiclePedInside())
		{
			return;
		}
		if(impactVel > g_CarVelForPedImpact)
		{
			audPedAudioEntity * pedAudio = ((CPed*)otherEntity)->GetPedAudioEntity();

			pedAudio->PlayVehicleImpact(posB, GetVehicle(), impactContext);
			pedAudio->PlayUnderOverVehicleSounds(posB, GetVehicle(), impactContext);
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketVehiclePedCollisionPacket>(
				CPacketVehiclePedCollisionPacket(posB, impactContext), otherEntity, GetParent()->GetOwningEntity());
			}
#endif
			pedAudio->TriggerImpactSounds(posB, GetVehicleCollisionMaterialSettings(), GetVehicle(), impactVel, collisionEvent.otherComponent, collisionEvent.component, false);
		}
	}
	else
	{
		
		CollisionMaterialSettings * materialSettings =  GetVehicleCollisionMaterialSettings(collisionEvent.component);

		if(!materialSettings)
		{
			return;
		}

		CollisionMaterialSettings * otherMaterialsettings = NULL;

		bool otherMaterialIsMap = false;

		if(otherEntity && otherEntity->GetIsTypeVehicle() && !(((CVehicle*)impactContext->otherEntity.Get())->GetVehicleType() == VEHICLE_TYPE_BICYCLE))
		{
			otherIsVehicle = true;

			VehicleCollisionSettings * otherVehSettings = ((CVehicle*)impactContext->otherEntity.Get())->GetVehicleAudioEntity()->GetCollisionAudio().GetVehicleCollisionSettings();

			impactContext->impactMag *= otherVehSettings->VehicleSizeScale;
			impactMag = impactContext->impactMag;

			if(otherVehSettings == settings && settings->VehOnVehCrashSound != g_NullSoundHash)
			{
				f32 otherVel = ((CVehicle*)impactContext->otherEntity.Get())->GetVelocity().Mag();
				if(impactVel > otherVel || (impactVel == otherVel && GetVehicle() > otherEntity))
				{
					m_NeedsToPlayVehOnVehImpact = true;
				}
				else
				{
					return;
				}
			}

			otherWeight = AUDIO_WEIGHT_VH; //g_CollisionAudioEntity.GetAudioWeight(impactContext->otherEntity);
			otherWeightScaling = g_CollisionAudioEntity.GetAudioWeightScaling(otherWeight, weight);

			otherMaterialsettings = ((CVehicle*)impactContext->otherEntity.Get())->GetVehicleAudioEntity()->GetCollisionAudio().GetVehicleCollisionMaterialSettings(collisionEvent.otherComponent);
		}
		else
		{
			otherWeight = g_CollisionAudioEntity.GetAudioWeight(otherEntity);

			otherMaterialIsMap = (otherWeight == AUDIO_WEIGHT_VH);


			phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(collisionEvent.otherMaterialId);

			if(unpackedMtlIdA >= PGTAMATERIALMGR->GetNumMaterials())
			{
				return; //Invalid material id bail out
			}

			if(otherEntity && otherEntity->GetIsTypeObject() && otherEntity->GetBaseModelInfo()->GetModelType() == MI_TYPE_VEHICLE)
			{
				CVehicleModelInfo * vehicleInfo = (CVehicleModelInfo *)(otherEntity->GetBaseModelInfo());
				VehicleCollisionSettings * vehicleCollisions = NULL;
				u32 audioHash = vehicleInfo->GetAudioNameHash();
			
				if(!audioHash)
				{
					audioHash = vehicleInfo->GetModelNameHash();
				}
			
				if(vehicleInfo->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
				{
					BicycleAudioSettings * bicycleSettings = audNorthAudioEngine::GetObject<BicycleAudioSettings>(audioHash);
					if(bicycleSettings)
					{
						vehicleCollisions = audNorthAudioEngine::GetObject<VehicleCollisionSettings>(bicycleSettings->VehicleCollisions);
					}
				}
				else if(vehicleInfo->GetIsTrain())
				{
					TrainAudioSettings * trainSettings = audNorthAudioEngine::GetObject<TrainAudioSettings>(audioHash);
					if(trainSettings)
					{
						vehicleCollisions = audNorthAudioEngine::GetObject<VehicleCollisionSettings>(trainSettings->VehicleCollisions);
					}
				}
				else if(vehicleInfo->GetIsBoat())
				{
					BoatAudioSettings * boatSettings = audNorthAudioEngine::GetObject<BoatAudioSettings>(audioHash);
					if(boatSettings)
					{
						vehicleCollisions = audNorthAudioEngine::GetObject<VehicleCollisionSettings>(boatSettings->VehicleCollisions);
					}
				}
				else if(vehicleInfo->GetIsPlane())
				{
					PlaneAudioSettings * planeSettings = audNorthAudioEngine::GetObject<PlaneAudioSettings>(audioHash);
					if(planeSettings)
					{
						vehicleCollisions = audNorthAudioEngine::GetObject<VehicleCollisionSettings>(planeSettings->VehicleCollisions);
					}
				}
				else if(vehicleInfo->GetVehicleType() == VEHICLE_TYPE_HELI ||  vehicleInfo->GetVehicleType()==VEHICLE_TYPE_AUTOGYRO || vehicleInfo->GetVehicleType()==VEHICLE_TYPE_BLIMP)
				{
					HeliAudioSettings * heliSettings = audNorthAudioEngine::GetObject<HeliAudioSettings>(audioHash);
					if(heliSettings)
					{
						vehicleCollisions = audNorthAudioEngine::GetObject<VehicleCollisionSettings>(heliSettings->VehicleCollisions);
					}
				}
				else if(vehicleInfo->GetVehicleType() == VEHICLE_TYPE_TRAILER)
				{
					//Trailers don't have VehicleCollisionSettings and are set up via MACS
				}
				else if(vehicleInfo->GetIsAutomobile() || vehicleInfo->GetVehicleType() == VEHICLE_TYPE_BIKE)
				{
					CarAudioSettings * carSettings = audNorthAudioEngine::GetObject<CarAudioSettings>(audioHash);
					if(carSettings)
					{
						vehicleCollisions = audNorthAudioEngine::GetObject<VehicleCollisionSettings>(carSettings->VehicleCollisions);
					}
				}

				if(vehicleCollisions)
				{
					CObject * object = (CObject*)otherEntity;
					if(object->m_nObjectFlags.bCarWheel)
					{
						otherMaterialsettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(vehicleCollisions->WheelFragMaterial);
					}
					else
					{
						otherMaterialsettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(vehicleCollisions->FragMaterial);
					}
				}
			}

			if(!otherMaterialsettings)
			{
				otherMaterialsettings = g_CollisionAudioEntity.GetMaterialOverride(impactContext->otherEntity, g_audCollisionMaterials[(s32)unpackedMtlIdA], collisionEvent.otherComponent);
			}

#if __BANK
			if(g_DebugVehicleCollisionAudio && otherMaterialsettings)
			{
				naErrorf("Vehicle colliding with %s, raw impact mag %f (event %f)", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(otherMaterialsettings->NameTableOffset), impactContext->impactMag, collisionEvent.impactMag);
			}
#endif
		}

		if(GetVehicle()->ContainsLocalPlayer())
		{
			if( otherEntity && (otherEntity->GetIsTypeObject() || otherEntity->GetIsTypePed() || otherEntity->GetIsTypeVehicle()))
			{
				g_CollisionAudioEntity.HandlePlayerEvent(otherEntity);
			}
		}

		weightScaling = g_CollisionAudioEntity.GetAudioWeightScaling(weight, otherWeight);
		otherWeightScaling = g_CollisionAudioEntity.GetAudioWeightScaling(otherWeight, weight);

		f32 volume = g_SilenceVolume, otherVolume = g_SilenceVolume;
		u32 startOffset = 0, otherStartOffset = 0;

		f32 collisionMin = otherIsVehicle ? settings->VehicleCollisionMin : settings->CollisionMin;
		f32 collisionMax = otherIsVehicle ? settings->VehicleCollisionMax : settings->CollisionMax;

		if(impactContext->GetTypeFlag(AUD_VEH_COLLISION_UPSIDEDOWN))  
		{
			impactMag *= g_UpsideDownCarImpulseScalingFactor;
		}

		audCurve curve;
		if(otherIsVehicle ? curve.Init(settings->VehicleImpactStartOffsetCurve) : curve.Init(settings->ImpactStartOffsetCurve))
		{
			startOffset = (u32)floorf(curve.CalculateRescaledValue(0.f, 1.f, collisionMin, collisionMax, impactMag) * 100.0f);
		}

		f32 volumeLinear = 0;

		if(curve.Init(settings->VelocityImpactScalingCurve))
		{
			impactMag*=curve.CalculateValue(impactVel);
		}

		if(otherIsVehicle ? curve.Init(settings->VehicleImpactVolCurve) : curve.Init(settings->ImpactVolCurve))
		{
			volumeLinear = curve.CalculateRescaledValue(0.f, 1.f, collisionMin, collisionMax, impactMag);
			volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear*weightScaling);
		}

		otherImpactMag *= otherWeightScaling;

		if(otherEntity && otherEntity->GetCurrentPhysicsInst() && otherEntity->GetCurrentPhysicsInst()->GetClassType() == PH_INST_MAPCOL)
		{
			otherVolume = volume;
			otherStartOffset = startOffset;
		}
		else
		{
			g_CollisionAudioEntity.ComputeStartOffsetAndVolumeFromImpactMagnitude(otherImpactMag, otherMaterialsettings, otherStartOffset, otherVolume);

		}

		if(otherEntity && otherEntity->GetIsTypeObject())
		{
			if(((CObject*)otherEntity)->m_nObjectFlags.bHasBeenUprooted && (((((CObject*)otherEntity)->m_nEndOfLifeTimer) > 
				(fwTimer::GetTimeInMilliseconds() - 100))))
			{
				otherVolume = 0.f;
				otherStartOffset = 0;    
			}
		}

        // Suppress vehicle/ground collision sounds when adjusting the vehicle hydraulics
        if(GetParent()->HydraulicsModifiedRecently() && GetParent()->GetCachedVelocity().Mag2() < 2.5f)
        {
            if(otherEntity && !otherEntity->GetIsTypeObject() && !otherEntity->GetIsTypePed() && !otherEntity->GetIsTypeVehicle())
            {
                otherVolume = g_SilenceVolume;
                volume = g_SilenceVolume;
            }
        } 

		if((volume <= g_SilenceVolume || startOffset >= 100) && (otherVolume <= g_SilenceVolume || otherStartOffset >= 100))
		{
#if __BANK
			if(g_DebugVehicleCollisionAudio)
			{
				naErrorf("Bailing from too-quiet vehicle collision, volume %f, startoffset %u, time %u", volume, startOffset, fwTimer::GetTimeInMilliseconds());
			}
#endif
			return;
		}

		bool canPlay = true, otherCanPlay = true;
		if(!GetVehicle()->ContainsLocalPlayer() && materialSettings->CollisionCount >= g_CollisionMaterialCountThreshold) 
		{
			if(volume > materialSettings->VolumeThreshold)
			{
				materialSettings->VolumeThreshold = volume;
			}
			else
			{
				volume = -100.f;
				canPlay = false;
			}
		}

		if(volume > impactContext->impactVolume)
		{
			impactContext->impactVolume = volume;
		}


		if(volume > m_VehicleCollisionContext.impactVolume)
		{
			m_VehicleCollisionContext.Init(*impactContext);
		}

		if(otherMaterialsettings && otherMaterialsettings->CollisionCount >= g_CollisionMaterialCountThreshold)
		{
			if(volume > otherMaterialsettings->VolumeThreshold)
			{
				otherMaterialsettings->VolumeThreshold = volume;
			}
			else
			{
				volume = -100.f;
				otherCanPlay = false;
			}
		}

		GetParent()->TriggerDynamicImpactSounds(volume);

		u32 hardImpact = materialSettings->HardImpact; 
		u32 hardImpactSlowMo = materialSettings->SlowMoHardImpact; 
		u32 solidImpact = materialSettings->SolidImpact;

		u32 otherHardImpact = otherMaterialsettings ? otherMaterialsettings->HardImpact : 0;
		u32 otherHardImpactSlowMo = otherMaterialsettings ? otherMaterialsettings->SlowMoHardImpact : 0;
		u32 otherSoftImpact = otherMaterialsettings ? otherMaterialsettings->SoftImpact : 0;
		u32 otherSolidImpact = otherMaterialsettings? otherMaterialsettings->SolidImpact: 0;

		if(otherHardImpact == g_NullSoundHash)
		{
			otherHardImpact = 0;
			otherHardImpactSlowMo = g_NullSoundHash;
		}
		if(otherSolidImpact == g_NullSoundHash) 
		{
			otherSolidImpact = 0;
		}

		if (GetParent()->GetAudioVehicleType() == AUD_VEHICLE_HELI && GetParent()->GetVehicleModelNameHash() == ATSTRINGHASH("SEASPARROW", 0xD4AE63D9) && impactContext->GetTypeFlag(AUD_VEH_COLLISION_BOTTOM))
		{
			hardImpact = ATSTRINGHASH("seasparrow_skid_collision", 0x9E6FA734);
			hardImpactSlowMo = ATSTRINGHASH("seasparrow_skid_collision", 0x9E6FA734);
			otherHardImpact = 0;
			otherHardImpactSlowMo = g_NullSoundHash;
		}

		if(GetParent()->GetAudioVehicleType() == AUD_VEHICLE_BOAT && (!GetVehicle()->GetIsInWater() || impactContext->GetTypeFlag(AUD_VEH_COLLISION_BOTTOM))  && (!otherEntity || !otherEntity->GetIsTypeVehicle()))
		{
			BoatAudioSettings * boatSettings = ((audBoatAudioEntity*)GetParent())->GetBoatAudioSettings();
			hardImpact = otherHardImpact ? boatSettings->DryLandHardImpact : hardImpact;
			hardImpactSlowMo = otherHardImpact ? g_NullSoundHash : hardImpactSlowMo;
		}

		if(GetParent()->GetAudioVehicleType() == AUD_VEHICLE_CAR && GetParent()->GetVehicle()->InheritsFromAmphibiousQuadBike() && (((audCarAudioEntity*)GetParent())->IsInAmphibiousBoatMode() || ((CAmphibiousQuadBike*)GetParent()->GetVehicle())->IsWheelsFullyIn()) &&
		   (!GetVehicle()->GetIsInWater() || impactContext->GetTypeFlag(AUD_VEH_COLLISION_BOTTOM))  && (!otherEntity || !otherEntity->GetIsTypeVehicle()))
		{
			BoatAudioSettings * boatSettings = ((audCarAudioEntity*)GetParent())->GetAmphibiousBoatSettings();

			if(boatSettings)
			{
				hardImpact = otherHardImpact ? boatSettings->DryLandHardImpact : hardImpact;
				hardImpactSlowMo = otherHardImpact ? g_NullSoundHash : hardImpactSlowMo;
			}			
		}

		audSoundInitParams otherInitParams;

		otherInitParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
		otherInitParams.StartOffset = otherStartOffset;
		otherInitParams.IsStartOffsetPercentage = true;
		otherInitParams.Volume = otherVolume;
		otherInitParams.Position = posB;

		const bool isKosatkaCollision = ((GetParent()->GetVehicle() && GetParent()->GetVehicle()->GetModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70) && impactContext->otherEntity && impactContext->otherEntity->GetIsTypeVehicle()));

		if(otherHardImpact && hardImpact) //Only play hard impacts if both materials are hard 
		{
			if(isKosatkaCollision || !impactContext->otherEntity || !impactContext->otherEntity->GetIsTypeVehicle())
			{
#if __BANK
				if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && otherHardImpactSlowMo != g_NullSoundHash)
				{
					otherHardImpact = otherHardImpactSlowMo;
				}
#endif

				GetParent()->CreateAndPlaySound(otherHardImpact, &otherInitParams);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(otherHardImpact, &otherInitParams, GetParent()->GetOwningEntity(), NULL, eNoGlobalSoundEntity, otherHardImpactSlowMo));

				audNorthAudioEngine::GetGtaEnvironment()->SpikeResonanceObjects(collisionEvent.impactMag * naEnvironment::sm_VehicleCollisionResonanceImpulse, VEC3V_TO_VECTOR3(collisionEvent.pos), naEnvironment::sm_VehicleCollisionResonanceDistance);
			}
		}
		else if(solidImpact && otherSolidImpact)
		{
			if(!impactContext->otherEntity || !impactContext->otherEntity->GetIsTypeVehicle())
			{
				GetParent()->CreateAndPlaySound(otherSolidImpact, &otherInitParams);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(otherSolidImpact, &otherInitParams, GetParent()->GetOwningEntity()));
			}
		}
#if __BANK
		PF_INCREMENT(CollisonsProcessed);
#endif

		if(impactContext->otherEntity && !impactContext->otherEntity->GetIsTypeVehicle())
		{
			GetParent()->CreateAndPlaySound(otherSoftImpact, &otherInitParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(otherSoftImpact, &otherInitParams, GetParent()->GetOwningEntity()));
		}

		if(otherMaterialsettings && otherCanPlay && g_UseCollisionIntensityCulling)
		{
			if(!g_RecentlyCollidedMaterials.IsMemberOfList(otherMaterialsettings))
			{
				g_RecentlyCollidedMaterials.Add(otherMaterialsettings);
			}
			otherMaterialsettings->CollisionCount++;
		}
	}
}

void audVehicleCollisionAudio::ProcessVehicleCollision()
{
	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if( m_VehicleCollisionContext.impactVolume > -100.f )
	{
#if __BANK
		if(g_DebugVehicleCollisionAudio)
		{
			naWarningf("Processing vehicle collision, volume %f vehicle %p", m_VehicleCollisionContext.impactVolume, GetVehicle());
		}
#endif


		if(now - m_LastFakeImpactTime <  g_BlockingTimeForVehicleCollisions && m_LastFakeImpactVol > m_VehicleCollisionContext.impactVolume)
		{
#if __BANK
			if(g_DebugVehicleCollisionAudio)
			{
				naWarningf("Bailing from vehicle collision due to previous fake collision: volume %f, fake vol %f, last fake time %u, time %u", m_VehicleCollisionContext.impactVolume, m_LastFakeImpactVol, m_LastFakeImpactTime , fwTimer::GetTimeInMilliseconds());
			}
#endif
			return;
		}
		if(now - m_LastVehicleCollisionTime <  g_BlockingTimeForVehicleCollisions && m_LastVehicleCollisionVolume > m_VehicleCollisionContext.impactVolume)
		{
#if __BANK
			if(g_DebugVehicleCollisionAudio)
			{
				naWarningf("Bailing from vehicle collision due to previous vehicle collision: volume %f, prev vol %f, prev time %u, time %u", m_VehicleCollisionContext.impactVolume, m_LastVehicleCollisionVolume, m_LastVehicleCollisionTime , fwTimer::GetTimeInMilliseconds());
			}
#endif	
			return;
		}
		const audVehicleCollisionContext & impactContext = m_VehicleCollisionContext; 
		const audVehicleCollisionEvent & collisionEvent = m_VehicleCollisionContext.collisionEvent;

		VehicleCollisionSettings * settings =  GetVehicleCollisionSettings();

		CEntity * otherEntity = impactContext.otherEntity;

		Vector3 posA = VEC3V_TO_VECTOR3(collisionEvent.pos);
	
		CollisionMaterialSettings * materialSettings =  GetVehicleCollisionMaterialSettings(collisionEvent.component);

		CollisionMaterialSettings * otherMaterialsettings = NULL;

		if(otherEntity && otherEntity->GetIsTypeVehicle())
		{
			otherMaterialsettings = ((CVehicle*)impactContext.otherEntity.Get())->GetVehicleAudioEntity()->GetCollisionAudio().GetVehicleCollisionMaterialSettings(collisionEvent.otherComponent);
		}
		else
		{
			phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(collisionEvent.otherMaterialId);

			if(unpackedMtlIdA >= PGTAMATERIALMGR->GetNumMaterials())
			{
				return; //Invalid material id bail out
			}

			otherMaterialsettings = g_CollisionAudioEntity.GetMaterialOverride(impactContext.otherEntity, g_audCollisionMaterials[(s32)unpackedMtlIdA], collisionEvent.otherComponent);
		}


		f32 volume = impactContext.impactVolume;
		u32 startOffset = impactContext.impactOffset;

		u32 hardImpact = materialSettings->HardImpact; 
		u32 hardImpactSlowMo = materialSettings->SlowMoHardImpact;

		u32 softImpact = materialSettings->SoftImpact;
		u32 solidImpact = materialSettings->SolidImpact;

		u32 otherHardImpact = otherMaterialsettings ? otherMaterialsettings->HardImpact : 0;
		u32 otherHardImpactSlowMo = otherMaterialsettings ? otherMaterialsettings->SlowMoHardImpact : 0;
		u32 otherSolidImpact = otherMaterialsettings? otherMaterialsettings->SolidImpact: 0;

		if(otherHardImpact == g_NullSoundHash || (otherEntity && otherEntity->GetIsTypePed()))
		{
			otherHardImpact = 0;
			otherHardImpactSlowMo = 0;
		}
		if(otherSolidImpact == g_NullSoundHash && ((otherEntity && !otherEntity->GetIsTypePed()) || !g_PedsMakeSolidCollisions)) 
		{
			otherSolidImpact = 0;
		}

		if (GetParent()->GetAudioVehicleType() == AUD_VEHICLE_HELI && GetParent()->GetVehicleModelNameHash() == ATSTRINGHASH("SEASPARROW", 0xD4AE63D9) && impactContext.GetTypeFlag(AUD_VEH_COLLISION_BOTTOM))
		{
			hardImpact = ATSTRINGHASH("seasparrow_skid_collision", 0x9E6FA734);
			hardImpactSlowMo = ATSTRINGHASH("seasparrow_skid_collision", 0x9E6FA734);
			otherHardImpact = 0;
			otherHardImpactSlowMo = g_NullSoundHash;
		}

		if(GetParent()->GetAudioVehicleType() == AUD_VEHICLE_BOAT && (!GetVehicle()->GetIsInWater() || impactContext.GetTypeFlag(AUD_VEH_COLLISION_BOTTOM))  && (!otherEntity || !otherEntity->GetIsTypeVehicle()))
		{
			BoatAudioSettings * boatSettings = ((audBoatAudioEntity*)GetParent())->GetBoatAudioSettings();
			hardImpact = otherHardImpact ? boatSettings->DryLandHardImpact : hardImpact;
			hardImpactSlowMo = otherHardImpact ? g_NullSoundHash : hardImpactSlowMo;
		}
		if(GetParent()->GetAudioVehicleType() == AUD_VEHICLE_CAR && GetParent()->GetVehicle()->InheritsFromAmphibiousQuadBike() && (((audCarAudioEntity*)GetParent())->IsInAmphibiousBoatMode() || ((CAmphibiousQuadBike*)GetParent()->GetVehicle())->IsWheelsFullyIn()) && 
		   (!GetVehicle()->GetIsInWater() || impactContext.GetTypeFlag(AUD_VEH_COLLISION_BOTTOM))  && (!otherEntity || !otherEntity->GetIsTypeVehicle()))
		{
			BoatAudioSettings * boatSettings = ((audCarAudioEntity*)GetParent())->GetAmphibiousBoatSettings();

			if(boatSettings)
			{
				hardImpact = otherHardImpact ? boatSettings->DryLandHardImpact : hardImpact;
				hardImpactSlowMo = otherHardImpact ? g_NullSoundHash : hardImpactSlowMo;
			}			
		}
		else if(m_NeedsToPlayVehOnVehImpact && settings->VehOnVehCrashSound && settings->VehOnVehCrashSound != g_NullSoundHash)
		{
			hardImpact = settings->VehOnVehCrashSound;
			hardImpactSlowMo = g_NullSoundHash;
		}

		audSoundInitParams initParams;
		initParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
		initParams.StartOffset = startOffset;
		initParams.IsStartOffsetPercentage = true;
		initParams.Volume = volume;
		initParams.Position = posA;

		//Prevent planes from making too-big collisions under water.
		if(Water::IsCameraUnderwater())
		{
			if(GetVehicle()->GetVehicleType() == VEHICLE_TYPE_PLANE)
			{
				return;
			}
		}

		if (!m_Parent->IsToyCar() && otherEntity && otherEntity->GetIsTypeVehicle())
		{
			if (((CVehicle*)otherEntity)->GetVehicleAudioEntity()->IsToyCar())
			{
				return;
			}
		}

		if(otherHardImpact || !otherEntity || otherEntity->GetIsTypeBuilding()) //Only play hard impacts if both materials are hard or if hitting a building
		{
			if(hardImpact)
			{
				if(!g_NoVehicleCollisions && g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
				{
#if __BANK
					if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && hardImpactSlowMo != g_NullSoundHash)
					{
						hardImpact = hardImpactSlowMo;
					}
#endif

					GetParent()->CreateAndPlaySound(hardImpact, &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(hardImpact, &initParams, GetParent()->GetOwningEntity(), NULL, eNoGlobalSoundEntity, hardImpactSlowMo));
				}
				m_LastVehicleCollisionVolume = volume;
				m_LastVehicleCollisionTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
			}
			else if(solidImpact && g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
			{
				GetParent()->CreateAndPlaySound(solidImpact, &initParams);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(solidImpact, &initParams, GetParent()->GetOwningEntity()));
			}
		}
		else if(solidImpact && otherSolidImpact && g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
		{
			GetParent()->CreateAndPlaySound(solidImpact, &initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(solidImpact, &initParams, GetParent()->GetOwningEntity()));
		}
#if __BANK
		PF_INCREMENT(CollisonsProcessed);
#endif

#if __BANK
		if(g_DebugVehicleCollisionAudio)
		{
			naErrorf("Process Collision: impactMag %f, volume %f, time %u", impactContext.impactMag, volume, fwTimer::GetTimeInMilliseconds());
			naErrorf("Vehicle collision sounds: hardImpact %s, solidImpact %s, softImpact %s, vehicle %s", g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(hardImpact), g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(solidImpact), g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(softImpact), GetVehicle()->GetModelName());
			naErrorf("Other collision sounds: hardImpact %s, solidImpact %s entity %s", g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(otherHardImpact), g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(otherSolidImpact), otherEntity ? otherEntity->GetModelName() : NULL);
			grcDebugDraw::Sphere(GetVehicle()->GetTransform().GetPosition() + Vec3V(ScalarV(0.f), ScalarV(0.f), ScalarV(2.f)), 0.5f, Color32(0,255, 255, (int)(audDriverUtil::ComputeLinearVolumeFromDb(volume)*255)), true, 5);
		}
#endif
		if( g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
		{
			GetParent()->CreateAndPlaySound(softImpact, &initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(softImpact, &initParams, GetParent()->GetOwningEntity()));
		}

		f32 sweetenerTheshold = impactContext.GetTypeFlag(AUD_VEH_COLLISION_ON_VEHICLE) ? settings->VehicleSweetenerThreshold : settings->SweetenerImpactThreshold;

		// If the conductors are playing the stunt jump stuff, we don't want to play the big impact as we are using a custom version.
		// PlayStuntJumpCollision will return true when it's going to play the custom impact, so we have to stop playing the high impact sweetener as usual
		if(!NetworkInterface::IsGameInProgress() &&  !SUPERCONDUCTOR.GetVehicleConductor().PlayStuntJumpCollision(impactContext.impactMag))
		{
			if(impactContext.impactMag >= sweetenerTheshold && GetVehicle()->ContainsLocalPlayer() && !g_NoCrashSweetener && g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
			{
#if __BANK
				if(g_DebugVehicleCollisionAudio)
				{
					naErrorf("Vehicle impact sweetener, time %u", fwTimer::GetTimeInMilliseconds());
				}
#endif
				GetParent()->CreateAndPlaySound(GetVehicleCollisionSettings()->HighImpactSweetenerSound, &initParams);	
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(GetVehicleCollisionSettings()->HighImpactSweetenerSound, &initParams, GetParent()->GetOwningEntity()));
				if(otherMaterialsettings)
				{
					GetParent()->CreateAndPlaySound(otherMaterialsettings->BigVehicleImpactSound, &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(otherMaterialsettings->BigVehicleImpactSound, &initParams, GetParent()->GetOwningEntity()));
				}
				if(g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeFranklin)
				{
					GetParent()->CreateAndPlaySound(g_FrontendAudioEntity.GetSpecialAbilitySounds().Find(ATSTRINGHASH("Franklin_VehicleCrash", 0xE22DC8F9)), &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(g_FrontendAudioEntity.GetSpecialAbilitySounds().Find(ATSTRINGHASH("Franklin_VehicleCrash", 0xE22DC8F9)).Get(), &initParams, GetParent()->GetOwningEntity()));
				}
			}
		}

		if(materialSettings && g_UseCollisionIntensityCulling)
		{
			if(!g_RecentlyCollidedMaterials.IsMemberOfList(materialSettings))
			{
				g_RecentlyCollidedMaterials.Add(materialSettings);
			}
			materialSettings->CollisionCount++;
		}
	}
}

bool audVehicleCollisionAudio::IsTrailer()
{
	if(GetVehicle())
	{
		return ((CVehicleModelInfo*)GetVehicle()->GetBaseModelInfo())->GetIsTrailer();
	}

	return false;
}

bool audVehicleCollisionAudio::IsBike()
{
	if(GetVehicle())
	{
		return ((CVehicleModelInfo*)GetVehicle()->GetBaseModelInfo())->GetIsBike();
	}
	return false;
}

bool audVehicleCollisionAudio::IsBoat()
{
	if(GetVehicle())
	{
		return ((CVehicleModelInfo*)GetVehicle()->GetBaseModelInfo())->GetIsBoat();
	}

	return false;
}

CollisionMaterialSettings * audVehicleCollisionAudio::GetVehicleCollisionMaterialSettings() 
{
	if(BANK_ONLY(!sm_UpdateVehicleCollisionSettings &&) m_CollisionMaterialSettings)
	{
		return m_CollisionMaterialSettings;
	}

	VehicleCollisionSettings *settings = GetVehicleCollisionSettings();

	if(settings)
	{
			m_CollisionMaterialSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(settings->VehicleMaterialSettings);
	}
	else
	{
		m_CollisionMaterialSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_CAR_ENTITY", 0x22F79CB));
	}

	return m_CollisionMaterialSettings;
}

CollisionMaterialSettings * audVehicleCollisionAudio::GetVehicleCollisionMaterialSettings(u32 component) 
{
	if(IsTrailer())
	{
		CollisionMaterialSettings* collisionSettings = g_CollisionAudioEntity.GetMaterialOverride(GetVehicle(), NULL, component);
		if(collisionSettings)
		{
			return collisionSettings;
		}
	}

	if(BANK_ONLY(!sm_UpdateVehicleCollisionSettings &&) m_CollisionMaterialSettings)
	{
		return m_CollisionMaterialSettings;
	}

	VehicleCollisionSettings *settings = GetVehicleCollisionSettings();

	m_CollisionMaterialSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(settings->VehicleMaterialSettings);
	return m_CollisionMaterialSettings;
}

void audVehicleCollisionAudio::HeadLightSmash() 
{
	if(GetParent()->GetAudioVehicleType() == AUD_VEHICLE_BOAT)
	{
		return;
	}

	if(GetParent()->GetDrowningFactor() >= 1.f && !Water::IsCameraUnderwater())
	{
		return;
	}

	audCurve headlightCurve;
	
	headlightCurve.Init(ATSTRINGHASH("veh_headlight_vel_to_impactmag", 0x1ACFCF7C));

#if __BANK
	if(g_DebugVehicleCollisionAudio)
	{
		grcDebugDraw::Sphere(GetVehicle()->GetTransform().GetPosition() + GetVehicle()->GetTransform().GetB() + Vec3V(ScalarV(0.f), ScalarV(0.f), ScalarV(1.5f)), 0.5f, Color32(255, 0, 0), true, 5);
	}
#endif

	//@@: location AUDVEHICLECOLLISIONAUDO_HEADLIGHTSMASH_CALC_MAG
	float impactMag = headlightCurve.CalculateValue(GetParent()->GetVehicle()->GetVelocity().Mag());

	Fakeimpact(impactMag);
}

void audVehicleCollisionAudio::Fakeimpact(f32 impactMag, f32 UNUSED_PARAM(volumeCurveScale), bool forceImpact)
{
	VehicleCollisionSettings * settings = GetVehicleCollisionSettings();

#if __BANK
	if(g_DebugVehicleCollisionAudio)
	{
		naErrorf("Registering fake impact impactMag %f, time %u", impactMag, fwTimer::GetTimeInMilliseconds());
	}
#endif

	if(naVerifyf(settings, "Couldn't get vehicle settings for %s", m_Parent->GetVehicle()->GetDebugName()) && (m_ImpactThisFrame||forceImpact))
	{
		m_FakeImpactMagThisFrame = Max(impactMag, m_FakeImpactMagThisFrame);
	}
}

void audVehicleCollisionAudio::ProcessFakeImpact()
{
	VehicleCollisionSettings * settings = GetVehicleCollisionSettings();

	if(g_NoFakeVehicleCollisions)
	{
		return;
	}
	if(GetParent()->GetDrowningFactor() >= 1.f && !Water::IsCameraUnderwater())
	{
		return;
	}
	
	if(m_FakeImpactMagThisFrame > 0.f)
	{
		u32 startOffset = 0;
		float volume = -100;

		audCurve curve;

		f32 impactMag = m_FakeImpactMagThisFrame;
		
		if(curve.Init(settings->VelocityImpactScalingCurve))
		{
			impactMag*=curve.CalculateValue(GetVehicle()->GetVelocity().Mag());
		}

		if(curve.Init(settings->FakeImpactStartOffsetCurve))
		{
			startOffset = (u32)floorf(curve.CalculateRescaledValue(0.f, 1.f, settings->FakeImpactMin, settings->FakeImpactMax, impactMag) * 100.0f);
		}
	
		f32 volumeLinear = 0;
		if(curve.Init((settings->FakeImpactVolCurve)))
		{
			volumeLinear = curve.CalculateRescaledValue(0.f, 1.f, settings->FakeImpactMin, settings->FakeImpactMax, impactMag);
			volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);

			u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

			if(((volume <= (m_LastVehicleCollisionVolume + settings->FakeImpactTriggerDelta)) && 
				(now - m_LastVehicleCollisionTime < g_BlockingTimeForFakeImpacts))  && !g_AlwaysPlayFakeCollisions)
			{
#if __BANK
				if(g_DebugVehicleCollisionAudio)
				{
					naErrorf("Bailing from fake impact (collision), mag %f, volume %f, lastVolume %f, time %u, lasttime %u", impactMag, volume, m_LastFakeImpactVol, now, m_LastFakeImpactTime);
				}
#endif
				m_LastFakeImpactTime = now;
				return;
			}

			if(((volume <= m_LastFakeImpactVol) && 
				(now - m_LastFakeImpactTime < g_BlockingTimeForFakeImpacts))  && !g_AlwaysPlayFakeCollisions)
			{
#if __BANK
				if(g_DebugVehicleCollisionAudio)
				{
					naErrorf("Bailing from fake impact (fake impact), mag %f, volume %f, lastVolume %f, time %u, lasttime %u", impactMag, volume, m_LastFakeImpactVol, now, m_LastFakeImpactTime);
				}
#endif
				m_LastFakeImpactTime = now;
				return;
			}
			
			m_LastFakeImpactVol = volume;
			m_LastFakeImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

#if __BANK
			if(g_DebugVehicleCollisionAudio)
			{
				naErrorf("Fake impact: impactMag %f, Vol %f, time %u", impactMag, volume, fwTimer::GetTimeInMilliseconds());
				grcDebugDraw::Sphere(GetVehicle()->GetTransform().GetPosition() + GetVehicle()->GetTransform().GetA() + Vec3V(ScalarV(0.f), ScalarV(0.f), ScalarV(1.5f)), 0.5f, Color32(255,255,0, (int)(volumeLinear*255)), true, 5);
			}
#endif

			audSoundInitParams initParams;
			initParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
			initParams.StartOffset = startOffset;
			initParams.IsStartOffsetPercentage = true;
			initParams.Volume = volume;
			initParams.Tracker = GetVehicle()->GetPlaceableTracker();
			if(sm_VehicleCollisionRolloffBoostCurve.IsValid())
			{
				initParams.VolumeCurveScale *= sm_VehicleCollisionRolloffBoostCurve.CalculateValue(impactMag);
				if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - GetParent()->GetLastTimeExploded() < g_VehicleExplosionAudioBoostTime)
				{
					initParams.VolumeCurveScale += g_VehicleExplosionCollisionAudioRolloffBoost;
					initParams.Volume += g_VehicleExplosionCollisionAudioVolumeBoost;
				}
			}

			if(GetParent()->GetDrowningFactor() >= 1.f)
			{
				if(GetVehicle()->GetVehicleType() == VEHICLE_TYPE_PLANE)
				{
						return;
				}
			}


			// If the conductors are playing the stunt jump stuff, we don't want to play the big impact as we are using a custom version.
			// PlayStuntJumpCollision will return true when it's going to play the custom impact, so we have to stop playing the high impact sweetener as usual
			if(!SUPERCONDUCTOR.GetVehicleConductor().PlayStuntJumpCollision(impactMag))
			{
				if(impactMag >= settings->FakeImpactSweetenerThreshold && GetVehicle()->ContainsLocalPlayer() && ! g_NoCrashSweetener && g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
				{
#if __BANK
					if(g_DebugVehicleCollisionAudio)
					{
						naErrorf("Vehicle impact sweetener, time %u", fwTimer::GetTimeInMilliseconds());
						grcDebugDraw::Sphere(GetVehicle()->GetTransform().GetPosition() + GetVehicle()->GetTransform().GetA() + Vec3V(ScalarV(0.f), ScalarV(0.5f), ScalarV(1.5f)), 0.5f, Color32(255,255, 100, (int)(volumeLinear*255)), true, 5);
					}
#endif
					GetParent()->CreateAndPlaySound(GetVehicleCollisionSettings()->HighImpactSweetenerSound, &initParams);	
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(GetVehicleCollisionSettings()->HighImpactSweetenerSound, &initParams, GetParent()->GetOwningEntity(), GetVehicle()));
					if(g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeFranklin)
					{
						GetParent()->CreateAndPlaySound(g_FrontendAudioEntity.GetSpecialAbilitySounds().Find(ATSTRINGHASH("Franklin_VehicleCrash", 0xE22DC8F9)), &initParams);
					}
				}
			}
			if(g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
			{
				CollisionMaterialSettings* materialSettings = GetVehicleCollisionMaterialSettings();

				if(materialSettings)
				{
					u32 vehicleHardImpact = materialSettings->HardImpact;

#if __BANK || GTA_REPLAY
					u32 vehicleHardImpactSlowMo = materialSettings->SlowMoHardImpact;

#if __BANK
					if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && vehicleHardImpactSlowMo != g_NullSoundHash)
					{
						vehicleHardImpact = vehicleHardImpactSlowMo;
					}
#endif
#endif

					GetParent()->CreateAndPlaySound(vehicleHardImpact, &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(vehicleHardImpact, &initParams, GetParent()->GetOwningEntity(), GetVehicle(), eNoGlobalSoundEntity, vehicleHardImpactSlowMo));
				}				
			}
		}
	}


}

void audVehicleCollisionAudio::PostProcessImpacts()
{
	if(!GetVehicle()->IsDummy() && !GetParent()->GetIsWaitingToInit())
	{
		if(!GetParent()->GetVehicle()->InheritsFromSubmarine() && GetParent()->GetDrowningFactor() >= 1.f && !Water::IsCameraUnderwater())
		{
			return;
		}

		VehicleCollisionSettings *settings = GetVehicleCollisionSettings();

		CollisionMaterialSettings * materialSettings = GetVehicleCollisionMaterialSettings();

		if(!materialSettings)
		{
			return;
		}

		audVehicleCollisionContext * playContext = m_CollisionEventsList.GetEvents();
		f32 scrapeMag = 0.f, impactVolume = 0.f;
		f32 impactVelocity = 0.f, upsideDownMag = 0.f;
		bool didCollision = false;
		bool isBottomScrape = false;
		audVehicleCollisionContext * scrapeContext = NULL;

		while(playContext)
		{
			if(playContext->canPlayCollision)
			{
				ProcessCollision(playContext, playContext->collisionEvent);
				playContext->canPlayCollision = false;
				didCollision = true;
			}

			if(playContext->GetTypeFlag(AUD_VEH_COLLISION_SCRAPE))
			{
				if(playContext->scrapeMag > scrapeMag)
				{
					isBottomScrape = playContext->GetTypeFlag(AUD_VEH_COLLISION_BOTTOM);
					scrapeMag = playContext->scrapeMag;
					scrapeContext = playContext;
				}
			}

			f32 velocity = playContext->velocity.Mag();
			if(velocity > impactVelocity)
			{
				impactVelocity = velocity;
				impactVolume = playContext->impactVolume;
			}

			if(playContext->GetTypeFlag(AUD_VEH_COLLISION_UPSIDEDOWN) && playContext->impactMag > upsideDownMag)
			{
				upsideDownMag = playContext->impactMag;
			}

			playContext = playContext->next;
		}

		Vector3 pos = VEC3V_TO_VECTOR3(GetVehicle()->GetTransform().GetPosition());


		if(didCollision && m_WreckedTime + 1000 > g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) && g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
		{
			audSoundInitParams wreckedParams;
			wreckedParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
			wreckedParams.Position = pos;
			GetParent()->CreateAndPlaySound(sm_SoundSet.Find(ATSTRINGHASH("wrecked_crash", 0xB573616C)), &wreckedParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_SoundSet.Find(ATSTRINGHASH("wrecked_crash", 0xB573616C)).Get(), &wreckedParams, GetParent()->GetOwningEntity()));
		}

		ProcessFoliage();

		audSoundInitParams damageParams;
		damageParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
		damageParams.Volume = impactVolume;
		//damageParams.Position = &GetVehicle()->GetPosition();
		damageParams.Tracker = GetVehicle()->GetPlaceableTracker();

		f32 bodyDamageFactor = Max(m_Parent->GetScriptBodyDamageFactor(), 1.0f - (GetVehicle()->GetVehicleDamage()->GetBodyHealth() / CVehicleDamage::GetBodyHealthMax()));

		if(didCollision)
		{
			if(bodyDamageFactor > g_BodyDamageFactorForDebris)
			{
				if (impactVelocity > g_DamageImpactVel)
				{
					if(g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
					{
						GetParent()->CreateAndPlaySound(settings->ImpactDebris, &damageParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(settings->ImpactDebris, &damageParams, GetParent()->GetOwningEntity(), GetVehicle()));
					}
					TriggerDebrisSounds(impactVelocity);
				}
			}
		}

		if(upsideDownMag > 0.f)
		{
			TriggerUpsideDownRoll(upsideDownMag);
			if(bodyDamageFactor > g_BodyDamageFactorForDebris)
			{
				TriggerDebrisSounds(upsideDownMag);
			}
		}


		if(FPIsFinite(scrapeMag) && scrapeMag > materialSettings->MinScrapeSpeed && !g_NoVehicleScrapeAudio)
		{
			f32 volume = -100.f;
			int pitch = 0;

			naAssertf(FPIsFinite(scrapeMag) && 
				FPIsFinite(materialSettings->MinScrapeSpeed) &&
				FPIsFinite(materialSettings->MaxScrapeSpeed),
				"Hit a NaN when processing vehicle scrapes on %s with material %s, scrapeMag %f, MinScrapeSpeed %f, MaxScrapeSpeed %f",
				GetVehicle()->GetModelName(), audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings->NameTableOffset),
				scrapeMag, materialSettings->MinScrapeSpeed, materialSettings->MaxScrapeSpeed);

			if(m_ScrapeVolCurve.IsValid())
			{
				volume = audDriverUtil::ComputeDbVolumeFromLinear(m_ScrapeVolCurve.CalculateRescaledValue(0.f, 1.f, materialSettings->MinScrapeSpeed, materialSettings->MaxScrapeSpeed, scrapeMag));
			}
			else
			{
				naWarningf("Trying to play scrape for vehicle (%s) but volume curve is invalid", GetVehicle()->GetModelName());
				volume = -100;
			}

			if(m_ScrapePitchCurve.IsValid())
			{
				pitch = audDriverUtil::ConvertRatioToPitch(m_ScrapePitchCurve.CalculateRescaledValue(0.f, 1.f, materialSettings->MinScrapeSpeed, materialSettings->MaxScrapeSpeed, scrapeMag));
			}
			else
			{
				naWarningf("Trying to play scrape for vehicle (%s) but pitch curve is invalid", GetVehicle()->GetModelName());
				pitch = 0;
			}

			if(!m_ScrapeSound)
			{
				audSoundInitParams initParams;

				initParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();

				const Vector3 pos = VEC3V_TO_VECTOR3(GetVehicle()->GetTransform().GetPosition()); 
				initParams.Position = pos;

//#if __BANK
//				if(g_DebugVehicleCollisionAudio)
//				{
//					grcDebugDraw::Sphere(GetVehicle()->GetTransform().GetPosition() - GetVehicle()->GetTransform().GetA() + Vec3V(ScalarV(0.0f), ScalarV(0.f), ScalarV(1.5f)), 0.5f, Color32(255, 0, 255), true, 5);
//				}
//#endif
				u32 scrapeSound = materialSettings->ScrapeSound;
				u32 slowScrapeSound = m_VehicleCollisionSettings->SlowScrapeLoop;

				bool suppressScrapeImpacts = false;

				if (GetParent()->GetAudioVehicleType() == AUD_VEHICLE_HELI && GetParent()->GetVehicleModelNameHash() == ATSTRINGHASH("SEASPARROW", 0xD4AE63D9) && isBottomScrape)
				{
					suppressScrapeImpacts = true;
					scrapeSound = ATSTRINGHASH("seasparrow_skid_scrape", 0x323B87AF);
					slowScrapeSound = ATSTRINGHASH("seasparrow_slow_skid_scrape", 0x37B1B8BF);
				}

				if (!suppressScrapeImpacts)
				{
					TriggerScrapeImpact(initParams);
				}

				initParams.Volume = volume;
				initParams.Pitch = static_cast<s16>( pitch ); 
				initParams.u32ClientVar = AUD_VEHICLE_SOUND_SCRAPE;
				initParams.UpdateEntity = true;				

				if(GetParent()->GetAudioVehicleType() == AUD_VEHICLE_BOAT && !GetVehicle()->GetIsInWater())
				{

					bool hardMaterial = false;
					if(scrapeContext)
					{
						phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(scrapeContext->collisionEvent.otherMaterialId);
						if(unpackedMtlIdA < PGTAMATERIALMGR->GetNumMaterials())
						{
							CollisionMaterialSettings * scrapeMaterialSettings = g_CollisionAudioEntity.GetMaterialOverride(scrapeContext->otherEntity, g_audCollisionMaterials[(s32)unpackedMtlIdA]);
							if(scrapeMaterialSettings && scrapeMaterialSettings->HardImpact && scrapeMaterialSettings->HardImpact != g_NullSoundHash)
							{
								hardMaterial = true;
							}
						}
					}

					BoatAudioSettings * boatSettings = ((audBoatAudioEntity*)GetParent())->GetBoatAudioSettings();
					scrapeSound = hardMaterial ? boatSettings->DryLandHardScrape : boatSettings->DryLandScrape;
				}

				if(GetParent()->GetAudioVehicleType() == AUD_VEHICLE_CAR && GetParent()->GetVehicle()->InheritsFromAmphibiousQuadBike() && (((audCarAudioEntity*)GetParent())->IsInAmphibiousBoatMode() || ((CAmphibiousQuadBike*)GetParent()->GetVehicle())->IsWheelsFullyIn()))
				{					
					bool hardMaterial = false;
					if(scrapeContext)
					{
						phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(scrapeContext->collisionEvent.otherMaterialId);
						if(unpackedMtlIdA < PGTAMATERIALMGR->GetNumMaterials())
						{
							CollisionMaterialSettings * scrapeMaterialSettings = g_CollisionAudioEntity.GetMaterialOverride(scrapeContext->otherEntity, g_audCollisionMaterials[(s32)unpackedMtlIdA]);
							if(scrapeMaterialSettings && scrapeMaterialSettings->HardImpact && scrapeMaterialSettings->HardImpact != g_NullSoundHash)
							{
								hardMaterial = true;
							}
						}
					}

					BoatAudioSettings * boatSettings = ((audCarAudioEntity*)GetParent())->GetAmphibiousBoatSettings();

					if(boatSettings)
					{
						scrapeSound = hardMaterial ? boatSettings->DryLandHardScrape : boatSettings->DryLandScrape;
					}
				}

				// url:bugstar:6794909 - Kosatka - MULTI_SCRAPE is playing when grounding the sub
				if (scrapeSound != ATSTRINGHASH("MULTI_SCRAPE", 0x69751E17) || !GetVehicle() || GetVehicle()->GetModelNameHash() != ATSTRINGHASH("Kosatka", 0x4FAF0D70))
				{
					GetParent()->CreateSound_PersistentReference(scrapeSound, &m_ScrapeSound, &initParams);
				}				

				if(m_ScrapeSound)
				{
					REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(scrapeSound, &initParams, m_ScrapeSound, GetParent()->GetOwningEntity(), eNoGlobalSoundEntity);)
					m_ScrapeSound->PrepareAndPlay();
				}

				if(m_SlowScrapeSound)
				{
					m_SlowScrapeSound->StopAndForget();
				}

				if(m_SlowScrapeVolCurve.IsValid())
				{
					initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(m_SlowScrapeVolCurve.CalculateRescaledValue(0.f, 1.f, materialSettings->MinScrapeSpeed, materialSettings->MaxScrapeSpeed, scrapeMag));
				}
				else
				{
					naWarningf("Trying to play slow scrape for vehicle (%s) but volume curve is invalid", GetVehicle()->GetModelName());
					initParams.Volume = -100;
				}

				if(scrapeMag < 0.1f && m_Parent->GetCachedVelocity().Mag2() < 1.f)
				{
					if(m_Parent->AreHydraulicsActive())
					{
						initParams.Volume = -100;
					}					
				}

				GetParent()->AllocateVehicleVariableBlock();
				GetParent()->CreateSound_PersistentReference(slowScrapeSound, &m_SlowScrapeSound, &initParams);

				if(m_SlowScrapeSound)
				{
					REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(slowScrapeSound, &initParams, m_SlowScrapeSound, GetParent()->GetOwningEntity(), eNoGlobalSoundEntity);)

					m_SlowScrapeSound->PrepareAndPlay();
				}			
			}
			else
			{
				if(volume < (m_ScrapeSound->GetRequestedVolume() - 0.003f*(float)m_ScrapeSound->GetSimpleReleaseTime()))
				{
					volume = m_ScrapeSound->GetRequestedVolume() - 0.003f*(float)m_ScrapeSound->GetSimpleReleaseTime();
				}

				if(pitch < (m_ScrapeSound->GetRequestedPitch() - 0.003f*(float)m_ScrapeSound->GetSimpleReleaseTime()))
				{
					pitch = (s32)(m_ScrapeSound->GetRequestedPitch() - 0.003f*(float)m_ScrapeSound->GetSimpleReleaseTime());
				}

				m_ScrapeSound->SetRequestedVolume(volume);
				m_ScrapeSound->SetRequestedPitch(pitch);

				if(m_SlowScrapeVolCurve.IsValid())
				{
					volume = audDriverUtil::ComputeDbVolumeFromLinear(m_SlowScrapeVolCurve.CalculateRescaledValue(materialSettings->MinScrapeSpeed, materialSettings->MaxScrapeSpeed, 0.f, 1.f, scrapeMag));
				}
				else
				{
					naWarningf("Trying to play slow scrape for vehicle (%s) but volume curve is invalid", GetVehicle()->GetModelName());
					volume = -100;
				}

				if(m_SlowScrapeSound)
				{
					if(volume < (m_SlowScrapeSound->GetRequestedVolume() - 0.003f*(float)m_SlowScrapeSound->GetSimpleReleaseTime()))
					{
						volume = m_SlowScrapeSound->GetRequestedVolume() - 0.003f*(float)m_SlowScrapeSound->GetSimpleReleaseTime();
					}

					if(pitch < (m_SlowScrapeSound->GetRequestedPitch() - 0.003f*(float)m_SlowScrapeSound->GetSimpleReleaseTime()))
					{
						pitch = (s32)(m_SlowScrapeSound->GetRequestedPitch() - 0.003f*(float)m_SlowScrapeSound->GetSimpleReleaseTime());
					}

					m_SlowScrapeSound->SetRequestedVolume(volume);
					m_SlowScrapeSound->SetRequestedPitch(pitch);
				}
			}
			m_ScrapeStopSoundMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + g_ScrapingCollisionHoldTimeMs;
		}  


		//	Vector3 pos;
		//m_Vehicle->TransformIntoWorldSpace(pos, offset);

		f32 volume = 0.f, dbVol = g_SilenceVolume;

		if (!m_IsOnAnotherVehicle && m_ImpactDamage >= settings->DamageMin && FPIsFinite(m_ImpactDamage) && !g_NoDeformationAudio)
		{
			audCurve curve;
			if(curve.Init(settings->DamageVolCurve))
			{
				volume = curve.CalculateRescaledValue(0.f, 1.f, settings->DamageMin, settings->DamageMax, m_ImpactDamage);  
				dbVol = audDriverUtil::ComputeDbVolumeFromLinear(volume);
			}

			if(dbVol > g_SilenceVolume && g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
			{
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
				initParams.Volume = dbVol;
				//initParams.Position = GetVehicle()->GetPosition();
				initParams.Tracker = GetVehicle()->GetPlaceableTracker();
				GetParent()->CreateAndPlaySound(settings->DeformationSound, &initParams);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(settings->DeformationSound, &initParams, GetParent()->GetOwningEntity(), GetVehicle()));

#if __BANK
				if(g_DebugVehicleCollisionAudio)
				{
					grcDebugDraw::Sphere(GetVehicle()->GetTransform().GetPosition() - GetVehicle()->GetTransform().GetB() + Vec3V(ScalarV(0.f), ScalarV(0.f), ScalarV(1.5f)), 0.5f, Color32(0, 0, 255), true, 5);
				}
#endif

				if(impactVelocity > g_DeformationImpactVel)
				{
					u32 hardImpactSoundHash = materialSettings->HardImpact;

#if __BANK || GTA_REPLAY
					u32 hardImpactSlowMoSoundHash = materialSettings->SlowMoHardImpact;

#if __BANK
					if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && hardImpactSlowMoSoundHash != g_NullSoundHash)
					{
						hardImpactSoundHash = hardImpactSlowMoSoundHash;
					}
#endif
#endif

					GetParent()->CreateAndPlaySound(hardImpactSoundHash, &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(hardImpactSoundHash, &initParams, GetParent()->GetOwningEntity(), GetVehicle(), eNoGlobalSoundEntity, hardImpactSlowMoSoundHash));
				}
			}
		}

		f32 numWheelsOnGround = 0.f, numWheelsWereOnGround = 0.f; 


		for(int i=0; i< GetVehicle()->GetNumWheels(); i++)
		{
			if(GetVehicle()->GetWheel(i)->GetIsTouching())
			{
				numWheelsOnGround += 1.f;
			}
			if(GetVehicle()->GetWheel(i)->GetWasTouching())
			{
				numWheelsWereOnGround += 1.f;
			}
		}
		
		u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		u32 minJumpLandDuration = 0;
		bool useBigJump = true;

		// See url:bugstar:5492108 - RC Bandito has very spongy suspension and its easy to trigger a jump land sound by cornering fast and momentarily 
		// lifting one wheel off the ground. This code just dampens it down a bit so that we also need a minimum air time to register the impact.		
		if (m_Parent->IsToyCar() || m_Parent->IsGolfKart())
		{
			if (fwTimer::GetTimeInMilliseconds() - m_Parent->GetLastTimeInAir() < g_ToyCarMinAirTimeForBigLanding)
			{
				useBigJump = true;
				minJumpLandDuration = g_ToyCarMinBigJumpLandDuration;
			}
			else
			{
				useBigJump = false;
				minJumpLandDuration = g_ToyCarMinSmallJumpLandDuration;
			}
		}

		if(numWheelsOnGround - numWheelsWereOnGround > 0 && numWheelsWereOnGround < 5 && now - m_JumpStartTime > minJumpLandDuration)
		{
			// Don't play jump landing sounds from amphibious quadbikes while we're deploying/retracting the wheels, unless we're genuinely falling from a height
			if(!m_Parent->GetVehicle()->InheritsFromAmphibiousQuadBike() || ((CAmphibiousQuadBike*)m_Parent->GetVehicle())->IsWheelTransitionFinished() || m_Parent->GetCachedVelocity().GetZ() < -5)
			{
				if(m_Parent->GetVehicle()->GetSpecialFlightModeRatio() == 0.0f)
				{
					bool justFinishedWheelie = numWheelsOnGround > 2 && m_Parent->GetVehicle()->InheritsFromAutomobile() && ((CAutomobile*)m_Parent->GetVehicle())->m_nAutomobileFlags.bInWheelieModeWheelsUp;

					// Stunt jump races, we get erroneous collisions when driving on curved surfaces, so suppress them
					if(g_ReflectionsAudioEntity.IsStuntTunnelMaterial(GetParent()->GetMainWheelMaterial()) || m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("OPPRESSOR", 0x34B82784))
					{
						if(now - m_JumpStartTime > g_StuntJumpLandingMinJumpTimeMs)
						{
							TriggerJumpLand(m_JumpLandMag, justFinishedWheelie, useBigJump);
						}
					}
					else
					{
						TriggerJumpLand(m_JumpLandMag, justFinishedWheelie, useBigJump);
					}
				}				
			}			
		}
		else if(numWheelsWereOnGround - numWheelsOnGround > 0)
		{
			m_JumpStartTime = now;
		}

		if(impactVelocity > g_TrailerBumpVel && now > (m_LastTrailerBumpTime + 1000))
		{
			audTrailerAudioEntity* trailer = GetParent()->GetTrailer(); 

			if(trailer && g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
			{
				trailer->TriggerTrailerBump(damageParams.Volume);
				m_LastTrailerBumpTime = now;
			}
		}
	}

	m_ImpactDamage = 0.f;
	m_JumpLandMag = 0.f;

	m_CollisionEventsList.ResetToReuseList(); 
}

void audVehicleCollisionAudio::UpdateDeformation(const float damage, const Vector3 & UNUSED_PARAM(offset) , CEntity * inflictor)
{
	if(inflictor && inflictor->GetIsTypeVehicle())
	{
		//TODO entity specific checks e.g. for vehicles containing other vehicles
	}
	m_ImpactDamage += damage;
}

void audVehicleCollisionAudio::ProcessFoliage()
{	
	if(GetParent()->GetCachedVelocity().Mag2() < g_VehicleSpeedSqForFoliage)
	{
		m_Foliage.numCollisions = 0;
	}
	if(m_Foliage.numCollisions && (m_Parent->IsReal() || m_Parent->IsDummy()))
	{
		audSoundInitParams foliageParams;
		foliageParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
		foliageParams.TrackEntityPosition = true;
		foliageParams.u32ClientVar = AUD_VEHICLE_SOUND_FOLIAGE;
		foliageParams.UpdateEntity = true;
		
		bool playLoop = false;
		if(!m_FoliageLoop)
		{
			playLoop = true;
			GetParent()->CreateSound_PersistentReference(sm_SoundSet.Find(ATSTRINGHASH("foliage_loop", 0xB2EBF067)), &m_FoliageLoop, &foliageParams);
		}

		if(m_FoliageLoop)
		{
			if(playLoop)
			{
				m_FoliageLoop->PrepareAndPlay();
			}
			m_PlayingFoliageThisFrame = true;
		}		
	}
	else
	{
		if(m_FoliageLoop)
		{
			m_FoliageLoop->StopAndForget();
		}
	}

	m_Foliage.drag = 0.f;
	m_Foliage.numCollisions = 0;
}

void audVehicleCollisionAudio::TriggerScrapeImpact(audSoundInitParams & initParams)
{
	VehicleCollisionSettings *settings = GetVehicleCollisionSettings();

	if(Water::IsCameraUnderwater())
	{
		if(GetVehicle()->GetVehicleType() == VEHICLE_TYPE_PLANE)
		{
			return;
		}
	}
	if(g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
	{
		f32 magVel = GetVehicle()->GetVelocity().Mag();

		initParams.Volume = m_ScrapeImpactCurve.CalculateRescaledValue(0.f, 1.f, settings->ScrapeMin, settings->ScrapeMax, magVel);

		GetParent()->CreateAndPlaySound(settings->SmallScrapeImpact, &initParams);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(settings->SmallScrapeImpact, &initParams, GetParent()->GetOwningEntity()));

		initParams.Volume = m_SlowScrapeImpactCurve.CalculateRescaledValue(0.f, 1.f, settings->ScrapeMin, settings->ScrapeMax, magVel);

		GetParent()->CreateAndPlaySound(settings->SlowScrapeImpact, &initParams);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(settings->SlowScrapeImpact, &initParams, GetParent()->GetOwningEntity()));

	}
}

void audVehicleCollisionAudio::TriggerJumpLand(const f32 impulseMag, bool justFinishedWheelie, bool useBigJumpLandSound)
{
	const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0); 
	const f32 impulseMagScalar = (GetVehicle()->GetStatus() == STATUS_WRECKED ? 2.f : 1.f);

	if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - GetParent()->GetLastTimeExploded() < g_VehicleExplosionAudioBoostTime)
	{
		Fakeimpact(GetParent()->GetVehicle()->GetVelocity().Mag(), impulseMagScalar, true);
	}

	if(impulseMag*impulseMagScalar > g_VehCollisionJumpLandMag) 
	{
		if(timeInMs > m_LastJumpLandTime + g_jumpLandTimeFilter)
		{
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
			initParams.Tracker = GetVehicle()->GetPlaceableTracker();
			initParams.SetVariableValue(ATSTRINGHASH("impulse", 0xB05D6C92), impulseMag);
			f32 volume = g_SilenceVolume;

			audCurve curve;
			VehicleCollisionSettings * colSettings = GetVehicleCollisionSettings();
			if(curve.Init((colSettings->JumpLandVolCurve)))
			{
				f32 volumeLinear = curve.CalculateValue(impulseMag);					
				volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);
			}

			if(volume > g_SilenceVolume)
			{
				const f32 suspensionHealth = m_Parent->ComputeEffectiveSuspensionHealth();
				bool isInteriorView = audNorthAudioEngine::IsRenderingFirstPersonVehicleCam() && GetParent()->IsFocusVehicle();
				initParams.Volume = volume;
				initParams.SetVariableValue(ATSTRINGHASH("SUSPENSION_HEALTH", 0x66758432), suspensionHealth);

#if __BANK
				if(m_Parent->IsFocusVehicle() && g_DebugHydraulicSounds)
				{
					audDisplayf("Triggering jump landing with suspension health %.02f", suspensionHealth);
				}
#endif

				f32 bodyDamageFactor = Max(m_Parent->GetScriptBodyDamageFactor(), 1.0f - (GetVehicle()->GetVehicleDamage()->GetBodyHealth() / CVehicleDamage::GetBodyHealthMax()));
                
				if (m_Parent->IsToyCar() && !useBigJumpLandSound)
				{
					audSoundSet soundSet;
					
					if (soundSet.Init(ATSTRINGHASH("rcbandito_sounds", 0x7B9D9FE0)))
					{
						const u32 soundName = ATSTRINGHASH("small_jump_land", 0xFAB667EB);
						const audMetadataRef jumpLandingSound = soundSet.Find(soundName);

						if (jumpLandingSound != g_NullSoundRef)
						{
							GetParent()->CreateAndPlaySound(jumpLandingSound, &initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundSet.GetNameHash(), soundName, &initParams, GetParent()->GetOwningEntity(), GetVehicle()));
						}
					}
				}
				else if(m_Parent->GetVehicle()->InheritsFromAutomobile() && static_cast<CAutomobile*>(m_Parent->GetVehicle())->HasHydraulicSuspension())
				{
					audSoundSet hydraulicSuspensionSoundSet;
					hydraulicSuspensionSoundSet.Init(m_Parent->GetVehicleHydraulicsSoundSetName());

					if(hydraulicSuspensionSoundSet.IsInitialised())
					{
						u32 nameHash = g_NullSoundHash;
						u32 timeSinceHydraulicsModified = fwTimer::GetTimeInMilliseconds() - m_Parent->GetLastHydraulicEngageDisengageTime();

						// Suppress landing sounds for a short period after activating/deactivating the hydraulics						
						if(timeSinceHydraulicsModified > g_HydraulicsJumpLandSuppressionTime)
						{
							if(m_Parent->HydraulicsModifiedRecently())
							{
								nameHash = ATSTRINGHASH("Hydraulic_Suspension_Landing", 0x6E130876);
							}
							else
							{
								if(bodyDamageFactor > g_BodyDamageFactorForHydraulicDebris && g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
								{
									nameHash = ATSTRINGHASH("Hydraulic_Suspension_Landing_Loose", 0x11A52128);
								}
								else
								{
									nameHash = ATSTRINGHASH("Hydraulic_Suspension_Landing_Hard", 0xAFFDD401);
								}
							}
						}						

						const audMetadataRef jumpLandingSound = hydraulicSuspensionSoundSet.Find(nameHash);

						if(jumpLandingSound != g_NullSoundRef)
						{
							GetParent()->CreateAndPlaySound(jumpLandingSound, &initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSound(hydraulicSuspensionSoundSet.GetNameHash(), nameHash, &initParams, GetParent()->GetOwningEntity(), GetVehicle()));
						}
					}  
				}
				else
				{
					audSoundSet wheelieSoundSet;

					if (justFinishedWheelie && wheelieSoundSet.Init(ATSTRINGHASH("mod_vehicle_wheely_SS", 0x860473AA)))
					{
						const u32 soundName = ATSTRINGHASH("Wheely_Suspension_Land", 0xE7D5E94E);
						GetParent()->CreateAndPlaySound(wheelieSoundSet.Find(soundName), &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(wheelieSoundSet.GetNameHash(), soundName, &initParams, GetParent()->GetOwningEntity(), GetVehicle()));
					}
					else
					{
						if (g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
						{
							GetParent()->CreateAndPlaySound(GetParent()->GetJumpLandingSound(isInteriorView), &initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSound(GetParent()->GetJumpLandingSound(false), &initParams, GetParent()->GetOwningEntity(), GetVehicle()));
						}

						if (bodyDamageFactor > g_BodyDamageFactorForDebris && g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
						{
							GetParent()->CreateAndPlaySound(GetParent()->GetDamagedJumpLandingSound(isInteriorView), &initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSound(GetParent()->GetDamagedJumpLandingSound(false), &initParams, GetParent()->GetOwningEntity(), GetVehicle()));
						}

						if (m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("MINITANK", 0xB53C6C52) && m_Parent->IsWaitingOnActivatedJumpLand())
						{
							audSoundSet jumpLandSoundSet;

							if (jumpLandSoundSet.Init(m_Parent->IsFocusVehicle() ? ATSTRINGHASH("ch_vehicle_minitank_player_sounds", 0xCF2B0226) : ATSTRINGHASH("ch_vehicle_minitank_remote_sounds", 0x90AF5388)))
							{
								GetParent()->CreateAndPlaySound(jumpLandSoundSet.Find(ATSTRINGHASH("jump_land", 0x6B2460BA)), &initParams);
							}							
						}
					}
				}

				m_LastJumpLandTime = timeInMs;
			}
		}			
	}

	m_Parent->ResetWaitingOnActivatedJumpLand();
}

void audVehicleCollisionAudio::TriggerJumpLandScrape()
{
	const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0); 
	
	if(timeInMs > m_LastJumpLandScrapeTime + g_jumpLandScrapeTimeFilter)
	{
		VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(m_Parent->GetMainWheelMaterial());
		VfxWheelInfo_s* pVfxWheelInfo = g_vfxWheel.GetInfo(VFXTYRESTATE_BURST_DRY, vfxGroup);

		if(pVfxWheelInfo)
		{
			CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(m_Parent->GetVehicle());

			if(pVfxVehicleInfo)
			{
				atHashWithStringNotFinal ptFxHashName;

				if (pVfxVehicleInfo->GetWheelGenericPtFxSet()==2)
				{
					ptFxHashName = pVfxWheelInfo->ptFxWheelFric2HashName;
				}
				else
				{
					ptFxHashName = pVfxWheelInfo->ptFxWheelFric1HashName;
				}

				if(ptFxHashName.GetHash() == ATSTRINGHASH("rim_fric_hard", 0x9F6F204C))
				{
					audSoundInitParams initParams;
					f32 speedRatio = Clamp((m_Parent->GetCachedVelocity().Mag() - 10.0f)/30.0f, 0.0f, 1.0f);
					initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(speedRatio);

					if(initParams.Volume > g_SilenceVolume)
					{
						initParams.UpdateEntity = true;
						initParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
						initParams.Tracker = GetVehicle()->GetPlaceableTracker();
						m_LastJumpLandScrapeTime = timeInMs;

						GetParent()->CreateAndPlaySound(ATSTRINGHASH("JUMP_LAND_SCRAPE", 0xCCEC4586), &initParams);				
					}
				}
			}
		}
	}			
}

void audVehicleCollisionAudio::PostUpdate()
{
	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if(m_WasHitByTrain)
	{
		audSoundInitParams initParams;
		initParams.UpdateEntity = true;
		initParams.EnvironmentGroup = GetParent()->GetEnvironmentGroup();
		initParams.TrackEntityPosition = true;

		VehicleCollisionSettings * settings = GetVehicleCollisionSettings();

		if(settings)
		{
			if((g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - m_LastTimeHitByTrain) > sm_TrainLoopTime)
			{
				GetParent()->CreateAndPlaySound(settings->TrainImpact, &initParams);	
			}
			else
			{
				if(!m_TrainLoop)
				{
					GetParent()->CreateAndPlaySound_Persistent(settings->TrainImpactLoop, &m_TrainLoop, &initParams);
				}
			}
		}

		m_LastTimeHitByTrain = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		m_WasHitByTrain = false;
	}
	else
	{
		if(m_TrainLoop)
		{
			m_TrainLoop->StopAndForget();
		}
	}

	if(m_CarRollSound)
	{
		if(now > m_LastCarRollTime + g_VehicleRollHoldTime)
		{
			m_CarRollSound->StopAndForget();
		}
	}

	ProcessVehicleCollision();
	ProcessFakeImpact();

	m_FakeImpactMagThisFrame = 0.f;
	m_VehicleCollisionContext.Init();
	m_VehicleCollisionContext.collisionEvent.Init();
	m_NeedsToPlayVehOnVehImpact = false;
	m_ImpactThisFrame = false;
	m_IsOnAnotherVehicle = false;
}

void audVehicleCollisionAudio::WreckVehicle()
{
	m_WreckedTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

void audVehicleCollisionAudio::HandleMelee() 
{ 
	m_LastMeleeTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0); 
}


#if __BANK
void audVehicleCollisionAudio::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Collision Audio");
		bank.AddSlider("Land impact mag", &g_VehCollisionJumpLandMag, 0.f, 10.f, 0.1f);
		bank.AddSlider("Jump land time filter", &g_jumpLandTimeFilter, 0, 1000, 10);
		bank.AddSlider("Jump land scrape time filter", &g_jumpLandScrapeTimeFilter, 0, 1000, 10);
		bank.AddSlider("RC Car Min Time for Big Jump Landing", &g_ToyCarMinAirTimeForBigLanding, 0, 10000, 10);		
		bank.AddSlider("RC Car Min Big Jump Land Duration", &g_ToyCarMinBigJumpLandDuration, 0, 10000, 10);		
		bank.AddSlider("RC Car Min Small Jump Land Duration", &g_ToyCarMinSmallJumpLandDuration, 0, 10000, 10);		
		bank.AddToggle("Debug vehicle collision audio", &g_DebugVehicleCollisionAudio);
		bank.AddSlider("Explosion Collision Volume Boost", &g_VehicleExplosionCollisionAudioVolumeBoost, -100.f, 100.f, 1.f);
		bank.AddSlider("Explosion Collision Rolloff Boost", &g_VehicleExplosionCollisionAudioRolloffBoost, 0.f, 100.f, 0.1f);
		bank.AddSlider("Explosion boost time", &g_VehicleExplosionAudioBoostTime, 0, 100000, 10);
		bank.AddSlider("Boat bottom collision threshold", &g_BoatBottomContactDotThreshold, 0.f, 1.f, 0.01f);
		bank.AddToggle("Apply Boat Body Collision Limiting", &g_ApplyBoatBodyCollisionLimitingToAllBoats);
		bank.AddSlider("g_VehicleSpeedSqForFoliage", &g_VehicleSpeedSqForFoliage, 0.f, 10.f, 0.1f);
		bank.AddSlider("g_BlockingTimeForFakeImpacts", &g_BlockingTimeForFakeImpacts, 0, 1000, 10);
		bank.AddSlider("g_BlockingTimeForVehicleCollisions", &g_BlockingTimeForVehicleCollisions, 0, 1000, 10);
		bank.AddToggle("g_NoVehicleCollisions", &g_NoVehicleCollisions);
		bank.AddToggle("g_NoFakeVehicleCollisions", &g_NoFakeVehicleCollisions);
		bank.AddToggle("g_AlwaysPlayFakeCollisions", &g_AlwaysPlayFakeCollisions);
		bank.AddToggle("g_NoDeformationAudio", &g_NoDeformationAudio);
		bank.AddToggle("g_NoCrashSweetener", &g_NoCrashSweetener);
		bank.AddToggle("g_NoHeadlightSmashAudio", &g_NoHeadlightSmashAudio);
		bank.AddToggle("g_NoVehicleScrapeAudio", &g_NoVehicleScrapeAudio);
		bank.AddSlider("g_VehicleRollHoldTime", &g_VehicleRollHoldTime, 0, 10000, 100);
		bank.AddSlider("sm_MinSpeedForVehicleCollisions", &sm_MinSpeedForVehicleCollisions, 0.f, 10.f, 0.1f);
		bank.AddSlider("sm_TimeForBikePostMeleeImpacts", &sm_TimeForBikePostMeleeImpacts, 0, 10000, 100);
		bank.PopGroup();
}

void audVehicleCollisionAudio::UpdateCollisionSettings()
{
	sm_UpdateVehicleCollisionSettings = true;
}
#endif
