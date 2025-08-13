//  
// audio/collisionaudioentity.cpp
//  
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
// 
 
#include "collisionaudioentity.h"
#include "frontendaudioentity.h"
#include "northaudioengine.h"
#include "weaponaudioentity.h"

#include "glassaudioentity.h"
#include "scriptaudioentity.h"
#include "vehiclecollisionaudio.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audiohardware/driverdefs.h"
#include "audiohardware/driverutil.h"
#include "audiosoundtypes/multitracksound.h"
#include "audiosoundtypes/randomizedsound.h"
#include "audiosoundtypes/soundcontrol.h"
#include "animation/animbones.h"
#include "boataudioentity.h"
#include "camera/CamInterface.h"
#include "camera/Viewports/ViewportManager.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/frame.h"
#include "control/replay/Replay.h"
#include "control/replay/audio/weaponaudiopacket.h"
#include "control/replay/audio/CollisionAudioPacket.h"
#include "debug/debugscene.h"
#include "dynamicmixer.h"
#include "fragment/type.h"
#include "fragment/typegroup.h"
#include "fragment/typechild.h"
#include "grcore/debugdraw.h"
#include "input/mouse.h"
#include "ModelInfo/PedModelInfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "objects/door.h"
#include "pedfootstepaudio.h"
#include "peds/ped.h"
#include "peds/pedintelligence.h"
#include "physics/gtaMaterialManager.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "phbound/support.h"
#include "renderer/Water.h"
#include "scene/entities/compEntity.h"
#include "scene/AnimatedBuilding.h"
#include "scene/WarpManager.h"
#include "streaming/streaming.h"				// For CStreaming::HasObjectLoaded(), maybe other things.
#include "vehicles/boat.h"
#include "audio/heliaudioentity.h"
#include "game/weather.h"

#include "debugaudio.h"
#include "phbound/boundcomposite.h"
#include "atl/string.h"
#include "physics/inst.h"
#include "physics/gtaInst.h"


AUDIO_OPTIMISATIONS()

audCollisionAudioEntity g_CollisionAudioEntity;

extern RegdEnt g_AudioDebugEntity;

const u32 g_ScrapingCollisionHoldTimeMs = 100;
static const u32 g_RollingCollisionHoldTimeMs = 100;
f32 g_MinSpeedForCollision = 0.05f;
static f32 g_MinRollSpeed = 0.0f;
BANK_ONLY(static f32 g_CollisionTimeFilterSpeed = 3.f);
u32 g_MinTimeBetweenCollisions = 200;
BANK_ONLY(static s32 g_CollisionFilterLifetime = 80);
BANK_ONLY(static u32 g_FragCollisionStartTime = 200);
u32 g_CollisionMaterialCountThreshold = 3;
static u32 g_CollisionCountDecreaseRate = 10;
bool g_PedsMakeSolidCollisions = false;
bool g_DoPedScrapeStartImpacts = false;
bool g_PedsOverrideImpactMag = true; 
f32 g_ScrapeImpactVel = 0.01f;
f32 g_VehicleScrapeImpactVel = 1.5f;
BANK_ONLY(static f32 g_RollImpactVel = 0.01f);
static f32 g_RollScalar = 0.1f; //10.f;
static f32 g_WreckedVehicleImpactScale = 1.f;
static f32 g_WreckedVehicleRolloffScale = 1.f;
static f32 g_AnimalImpactScalar = 0.5f;
static f32 g_BodyFallBoost = 6.f;
static f32 g_MaterialScrapeAgainstPedVolumeScaling = 1.f;
static f32 s_ImpactMagScalar = 0.01f;
BANK_ONLY(bool g_DebugSwingingProps = false;)
f32 g_SwingVolSmoothRate = 0.001f;
u32 g_WeaponImpactFilterTime = 1000;
u32 g_PedScrapeStartImpactTime = 1000;
float g_PedWetSurfaceScrapeAtten = 0.5f;
bank_u32 g_LFPCuttoffInWater = 1000;
bank_float g_VolumeReductionInWater = 6.0f;
float g_PlayerMeleeCollisionVolumeBoost = 0.f;
float g_PlayerVehicleMeleeCollisionVolumeScale = -9.f;

f32 g_VelocityThesholdForPropSwing = 0.001f;

u32 audCollisionAudioEntity::sm_TimeSinceLastBulletHit = 0;
u32 audCollisionAudioEntity::sm_TimeSinceLastAutomaticBulletHit = 0;
u32 audCollisionAudioEntity::sm_TimeSinceLastShotgunBulletHit = 0;

bool g_UseCollisionIntensityCulling = true; //Limit the number of collision sounds when lots are playing

static const f32 g_MaxMeleeDamageDoneForCollision = 25.0f;
static const f32 g_MaxImpulseMagnitudeForMeleeDamageDoneVehicle = 7.0f;
static f32 g_MaxImpulseMagnitudeForMeleeDamageDoneGeneral = 6.0f;
static const f32 g_BulletRicochetPanDistance = 5.f;
//static f32 s_MeleeImpactScaleFactor = 25.f; 
f32	audCollisionAudioEntity::sm_PlayerRicochetsRollOffScaleFactor = 1.5f;
f32	audCollisionAudioEntity::sm_PlayerBIRollOffScaleFactor = 3.f;
f32	audCollisionAudioEntity::sm_PlayerHeadShotRollOffScaleFactor = 3.f;
u32 audCollisionAudioEntity::sm_PlayerCollisionEventTimeout = 2000;
f32 audCollisionAudioEntity::sm_PlayerCollisionEventSpeedThreshold = 1.f;
f32 audCollisionAudioEntity::sm_PlayerContextCollisionRolloff = 1.f;
f32 audCollisionAudioEntity::sm_PlayerContextVolumeForBlip = -20.f;

f32 audCollisionAudioEntity::sm_DistanceToTriggerRaygunBI = 50.f;
f32 audCollisionAudioEntity::sm_DistanceToTriggerAutomaticBI = 50.f;
f32 audCollisionAudioEntity::sm_DistanceToTriggerShotgunBI = 50.f;
u32 audCollisionAudioEntity::sm_DtToTriggerAutomaticBI = 500;
u32 audCollisionAudioEntity::sm_DtToTriggerShotgunBI = 500;

f32 g_SolidRollAttenuation = -8.f;
f32 g_SoftRollAttenuation = -12.f;

bool g_MapUsesPropRollSpeedSettings = true;
bool audCollisionAudioEntity::sm_DialogueSuppressBI = false;

#if __BANK
bank_float g_VehicleOnVehicleImpulseScalingFactor = 1.5f;
bank_float g_BoatImpulseScalingFactor = 3.0f;

bool g_PreserveMACSSettingsOnExport = true;
bool g_DebugFocusEntity = false;
audScene* audCollisionAudioEntity::sm_IncreaseFootstepVolumeScene = NULL;
int audCollisionAudioEntity::sm_CurrentSectorX = 0;
int audCollisionAudioEntity::sm_CurrentSectorY = 0;
bool audCollisionAudioEntity::sm_IsWaitingToInit = true;
bool audCollisionAudioEntity::sm_ProcessSectors = false;
fiStream * audCollisionAudioEntity::sm_NewMoveableFile = NULL;
fiStream * audCollisionAudioEntity::sm_NewStaticFile = NULL;
fiStream * audCollisionAudioEntity::sm_ChangedMacsFile = NULL;
fiStream * audCollisionAudioEntity::sm_DefaultMatFile = NULL;
bool audCollisionAudioEntity::sm_AutomaticImpactOnlyOnVeh = false;
bool audCollisionAudioEntity::sm_ShotgunDisabledForPlayer = false;
bool audCollisionAudioEntity::sm_ShotgunImpactOnlyOnVeh = false;

bool g_DisplayPedScrapeMaterial = false;
#endif // __BANK

bank_float g_PedBuildingImpulseScalingFactor = 4.f;
f32 g_UpsideDownCarImpulseScalingFactor = 3.0f;
//static u32 g_AudioWeights[NUM_AUDIOWEIGHT] = {1.f, 1.f, 1.f, 1.f, 1.f};
static f32 g_AudWeightDifferenceScaling[5] =  {1.f, 0.7f, 0.5f, 0.2f, 0.f};



bank_float g_MinBulletImpactDampingDistance = 50.0f;
bank_float g_MaxBulletImpactDampingDistance = 100.0f;
bank_float g_MaxBulletImpactCloserAttenuation = 0.0f;
bank_float g_MaxBulletImpactCloserImpulseDamping = 1.0f;

bool g_HardImpactsExcludeSolid = false;

//bank_float g_GrenadeImpulseMagnitudeScaling = 5000.f;

bank_u32 g_ImpactRetriggerTime = 61;

phMaterialMgr::Id g_PedMaterialId = ~0U;

const audCategory *g_UppedCollisionCategory		= NULL;
const audCategory *g_PedCollisionCategory		= NULL;
const audCategory *g_DoorsCategory				= NULL;

const char *g_MaterialNameCarBody				= "CAR_METAL";
const char *g_MaterialNameCarTyre				= "RUBBER";

const char *g_CustomMaterialNameFootLeft		= "FOOT_LEFT";
const char *g_CustomMaterialNameFootRight		= "FOOT_RIGHT";

CollisionMaterialSettings *g_VehiclePedMeleeMaterial = NULL;

const u32 g_NullMaterialHash = ATSTRINGHASH("AM_BASE_DEFAULT", 0xF2C345CD);
audMetadataRef g_NullMaterialRef;

atMap<u32,u32> audCollisionAudioEntity::sm_MacOverrideMap;

audCurve audCollisionAudioEntity::sm_TimeBetweenShotsToRicoProb;
audCurve audCollisionAudioEntity::sm_DistanceToBIPredelay;
audCurve audCollisionAudioEntity::sm_DistanceToHeadShotPredelay;
audCurve audCollisionAudioEntity::sm_DistanceToRicochetsPredelay;
EXT_PF_GROUP(TriggerSoundsGroup);
PF_COUNTER(audCollisionAudioEntity_Impacts,TriggerSoundsGroup);
PF_COUNTER(audCollisionAudioEntity_Scrapes,TriggerSoundsGroup);
PF_COUNTER(audCollisionAudioEntity_Rolls,TriggerSoundsGroup);
PF_COUNTER(audCollisionAudioEntity_Breaks,TriggerSoundsGroup);
PF_COUNTER(audCollisionAudioEntity_Bullets,TriggerSoundsGroup);

PF_PAGE(CollisionDebugPage,"audCollisionAudioEntity");
PF_GROUP(CollisionAudioEntity);
PF_LINK(CollisionDebugPage, CollisionAudioEntity);

PF_VALUE_FLOAT(DebugImpulseMag0, CollisionAudioEntity);
PF_VALUE_FLOAT(DebugImpulseMag1, CollisionAudioEntity);
PF_VALUE_FLOAT(DebugNumCollisions, CollisionAudioEntity);
PF_VALUE_INT(CollisionHistory, CollisionAudioEntity);

#if __BANK
XPARAM(audiotag);

namespace rage
{
	XPARAM(audiowidgets);

	PF_PAGE(AudioCollisionPage, "Audio Collisions"); 
	PF_GROUP(AudioCollisions);
	PF_LINK(AudioCollisionPage, AudioCollisions);



	PF_COUNTER(CollisonsProcessed, AudioCollisions);
}

bool g_PrintBulletImpactMaterial = false;
bool g_PrintMeleeImpactMaterial = false;
bool g_IncreaseFootstepsVolume = false;
bool g_DontPlayRicochets = false;
static char g_CollisionMaterialOverrideName[64] = "\0";
bool g_DisableCollisionAudio = false;

bool g_DebugAudioImpacts = false;
bool g_DebugAudioRolls = false; 
bool g_DebugAudioScrapes = false;
bool g_DebugPlayerVehImpacts= false;
f32 g_DebugImpactThresh = 0.01f;
bool g_MeleeOnlyBulletImpacts = false;

static const ModelAudioCollisionSettings * s_RaveSelectedMaterialSettings = NULL;
static char s_RaveSelectedMaterialSettingsName[256] = {"No object selected in Rave"};
static char s_PropSpawnName[256] = {"No prop to spawn"};
static bool s_AutoSpawnRaveObjects = false; 
static bool s_UpdateCreatedSettings = false;


//f32 g_RicochetProb90 = 0.0f;
//f32 g_RicochetProb0  = 0.9f;

u32 g_CollisionDebugIndex = 0;
const u32 g_NumDebugCollisionInfos = 64;
struct CollisionDebugInfo
{
	Vector3 position;
	u32 time;
	s32 component1;
	s32 component2;
	f32 impulseMag;
	char *mat1, *mat2;
	u32 entType1, entType2;
	bool hidden;
	audCollisionType type;
}g_CollisionDebugInfo[g_NumDebugCollisionInfos];

void DebugAudioImpact(const Vector3 &pos, s32 component1, s32 component2, f32 impulseMag, audCollisionType type)
{
	if((g_DebugAudioImpacts || g_DebugAudioRolls || g_DebugAudioScrapes) && impulseMag >= g_DebugImpactThresh)
	{
		g_CollisionDebugInfo[g_CollisionDebugIndex].time = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		g_CollisionDebugInfo[g_CollisionDebugIndex].position = pos;
		g_CollisionDebugInfo[g_CollisionDebugIndex].component1 = component1;
		g_CollisionDebugInfo[g_CollisionDebugIndex].component2 = component2;
		g_CollisionDebugInfo[g_CollisionDebugIndex].impulseMag = impulseMag;
		g_CollisionDebugInfo[g_CollisionDebugIndex].type = type;
		g_CollisionDebugInfo[g_CollisionDebugIndex].hidden = false;

		g_CollisionDebugIndex = (g_CollisionDebugIndex+1)%g_NumDebugCollisionInfos;
	}
}

//Used for rag-rave point and click Macs creation
struct ModelAudioSettingsSetup
{
	u32 numComponents;
	u32	 numMaterials;
	u32 model;
	char audioSettingsName[64];
	char materialNames[64][64];
};



static ModelAudioSettingsSetup s_ModelSettingsCreator;
atArray<MaterialAudName> g_audCollisionMaterialNames;
static char s_PointAndClickMaterial[64] = {0};
static char s_PointAndClickMaterialOverride[64] = {0};
static char s_PointAndClickComponent[32] = "No frag selected";
static s32 s_PointAndClickComponentId = 0;
static Vector3 s_PointAndClickPos;
static float s_SimulateCollisionVel = 0.f;
static s32 s_SimulateCollisionWeight1 = AUDIO_WEIGHT_M;
static s32 s_SimulateCollisionWeight2 = AUDIO_WEIGHT_M;

static CEntity * s_PointAndClickEntity = NULL;
static bool s_SimulateCollisionSoftImpact = false;
static char s_AudioWeightNames[NUM_AUDIOWEIGHT][32];

//#define MODEL_NAME_
//static char** g_NewModelDisplayObjectList;
//char gCurrentDisplayObject[STORE_NAME_LENGTH];
//static char** g_SyncModelDisplayObjectList;
//static char** g_NewFragModelDisplayObjectList;

static RegdEnt s_SpawnedCollisionProp;
static bool s_SpawnRavePropThisFrame;
#endif // __BANK

bool audCollisionAudioEntity::sm_EnableModelEditor = false;

bool g_DebugAudioCollisions = false;
f32 g_MaxRollSpeed = 3.f;
f32 g_RollVolumeSmoothUp = 0.05f;
f32 g_RollVolumeSmoothDown = 0.05f;

atArray<CollisionMaterialSettings*> g_audCollisionMaterials;
fwPtrListSingleLink g_RecentlyCollidedMaterials;

CollisionMaterialSettings* g_audBikeMetalMaterial;
CollisionMaterialSettings *g_audGrenadeMaterial;
ModelAudioCollisionSettings *g_MacsDefault = NULL;

void audCollisionAudioEntity::Init()
{ 
	naAudioEntity::Init();
	audEntity::SetName("audCollisionAudioEntity");

	m_CollisionEventHistoryAllocation.Reset();
	m_CollisionEventFreeList.MakeAllFree();

	m_CachedBulletImpacts.Reset();
	m_CachedMeleeImpacts.Reset();
	m_CachedFragmentBreaks.Reset();
	m_CachedUprootEvents.Reset();

	// made these local scope since they shouldn't be used outwith this function
	const u32 maxMaterialSettingsNameLength		= 64;
	const char *baseMaterialSettingsNamePrefix	= "AM_BASE_";

	for(u32 eventIndex=0; eventIndex<g_MaxCollisionEventHistory; eventIndex++)
	{
		m_CollisionEventHistory[eventIndex].Init();
	}

	m_MeleeDamageImpactScalingCurve.Init(ATSTRINGHASH("MELEE_DAMAGE_TO_VOLUME", 0x7C5035A6));
	m_MeleeFootstepImpactScalingCurve.Init(ATSTRINGHASH("MELEE_FOOTSTEP_TO_COLLISION_IMPACT", 0x9992D936));
	m_UppedCollisionCategoryNameHash =ATSTRINGHASH("PEDS_COLLISIONS_LOUD_DYNAMIC", 0xE8C16DEA);

	m_CustomMaterialIdFootLeft = PGTAMATERIALMGR->GetMaterialId(PGTAMATERIALMGR->FindMaterial(g_CustomMaterialNameFootLeft));
	m_CustomMaterialIdFootRight = PGTAMATERIALMGR->GetMaterialId(PGTAMATERIALMGR->FindMaterial(g_CustomMaterialNameFootRight));
	m_MaterialIdCarBody = PGTAMATERIALMGR->GetMaterialId(PGTAMATERIALMGR->FindMaterial(g_MaterialNameCarBody));
	m_MaterialIdCarTyre = PGTAMATERIALMGR->GetMaterialId(PGTAMATERIALMGR->FindMaterial(g_MaterialNameCarTyre));

	g_audCollisionMaterials.Resize(PGTAMATERIALMGR->GetNumMaterials());

	BANK_ONLY(g_audCollisionMaterialNames.Resize(PGTAMATERIALMGR->GetNumMaterials()));

	char materialSettingsName[maxMaterialSettingsNameLength];
	for(u32 i = 0; i < static_cast<u32>(PGTAMATERIALMGR->GetNumMaterials()); i++)
	{
		const phMaterial &mat = PGTAMATERIALMGR->GetMaterial(i);
		//Generate material settings game object name from material name.
		formatf(materialSettingsName, "%s%s", baseMaterialSettingsNamePrefix, mat.GetName());
		
		g_audCollisionMaterials[i] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(materialSettingsName);
		if(!g_audCollisionMaterials[i])
		{
			naDisplayf("Missing audio material: %u %s", i, materialSettingsName);
			g_audCollisionMaterials[i] = g_audCollisionMaterials[0];
			naAssert(g_audCollisionMaterials[i]);

			BANK_ONLY(strncpy(g_audCollisionMaterialNames[i].name, g_audCollisionMaterialNames[0].name, 64));
		}
		else
		{
			g_audCollisionMaterials[i]->MaterialID = i;

#if __BANK
			strncpy(g_audCollisionMaterialNames[i].name, materialSettingsName, 64);
#endif
		}


		if(g_PedMaterialId == ~0U && PGTAMATERIALMGR->GetIsPed(i))
		{
			g_PedMaterialId = i;
		}
	}

	// if a ped material hasn't been found then initialise it to zero
	if (g_PedMaterialId >= (u32)g_audCollisionMaterials.GetCount())
	{
		g_PedMaterialId = 0;
	}

	g_audBikeMetalMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_BIKE_METAL", 0x61ABD259)); 

	m_ImpactVolCurve.Init(ATSTRINGHASH("COLLISION_IMPACT_VOLUME", 0x4724E47F));
	m_PedImpactVolCurve.Init(ATSTRINGHASH("COLLISION_PED_IMPACT_VOLUME", 0x104C3F26));
	m_VehicleImpactVolCurve.Init(ATSTRINGHASH("COLLISION_VEHICLE_IMPACT_VOLUME", 0xF9E0D66));
	m_VehicleImpactStartOffsetCurve.Init(ATSTRINGHASH("COLLISION_VEHICLE_IMPACT_START_OFFSET", 0x478DF76B));
	m_ImpactStartOffsetCurve.Init(ATSTRINGHASH("COLLISION_IMPACT_START_OFFSET", 0xD081341));
	m_PedImpactStartOffsetCurve.Init(ATSTRINGHASH("COLLISION_PED_IMPACT_START_OFFSET", 0x44E5E0B3));
	m_ScrapeVolCurve.Init(ATSTRINGHASH("COLLISION_SCRAPE_VOLUME", 0x77D65D51));
	m_SwingVolCurve.Init(ATSTRINGHASH("COLLISION_SWING_VOLUME", 0x65D4BFA2));
	m_PedScrapeVolCurve.Init(ATSTRINGHASH("COLLISION_PED_SCRAPE_VOLUME", 0xC84D1FCE));
	m_PlayerScrapeVolCurve.Init(ATSTRINGHASH("COLLISION_PLAYER_SCRAPE_VOLUME", 0x9FA4B3A0));
	m_ScrapePitchCurve.Init(ATSTRINGHASH("COLLISION_SCRAPE_PITCH", 0x8682356F));
	m_PedScrapePitchCurve.Init(ATSTRINGHASH("COLLISION_PED_SCRAPE_PITCH", 0xC29DDFED));
	m_PlayerScrapePitchCurve.Init(ATSTRINGHASH("COLLISION_PLAYER_SCRAPE_PITCH", 0xDCEAFC5F));
	m_VehicleScrapeCurve.Init(ATSTRINGHASH("COLLISION_VEHICLE_SCRAPE_VOLUME", 0x6B53544B));
	m_VehicleScrapePitchCurve.Init(ATSTRINGHASH("COLLISION_VEHICLE_SCRAPE_PITCH", 0x4BB53B02));
	m_RollVolCurve.Init(ATSTRINGHASH("COLLISION_ROLL_VOLUME", 0x9EC036B5));
	m_RollPitchCurve.Init(ATSTRINGHASH("COLLISION_ROLL_PITCH", 0xDA4E293D));
	m_RollResVolCurve.Init(ATSTRINGHASH("COLLISION_ROLL_RES_VOLUME", 0x14DB8CFE));
	m_RollResPitchCurve.Init(ATSTRINGHASH("COLLISION_ROLL_RES_PITCH", 0xF710144F));


	g_UppedCollisionCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(m_UppedCollisionCategoryNameHash);
	g_PedCollisionCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("PEDS_COLLISIONS", 0xACBC84EA));
	g_DoorsCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("COLLISIONS", 0x9644F744)); //the is no door specific category at the moment so we'll just use the collision cat

	g_VehiclePedMeleeMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_VEHICLE_PED_MELEE_MATERIAL", 0xF9FD6ECA));
	g_audGrenadeMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_WEAPONS_GRENADE", 0x15F77876));
	g_MacsDefault = audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(ATSTRINGHASH("MOD_DEFAULT", 0x5F03E8C2));

	g_NullMaterialRef = audNorthAudioEngine::GetMetadataManager().GetObjectMetadataRefFromHash(g_NullMaterialHash);

#if __BANK
	sysMemSet(&s_ModelSettingsCreator, 0, sizeof(s_ModelSettingsCreator));
	for(s32 i = 0; i < NUM_AUDIOWEIGHT; ++i)
	{
		strcpy(s_AudioWeightNames[i], AudioWeight_ToString((AudioWeight)i));
	}
	if(PARAM_audiowidgets.Get())
	{
		audPedFootStepAudio::AddMaterialCollisionWidgets();
	}
#endif
}

void audCollisionAudioEntity::InitClass()
{
	sm_TimeBetweenShotsToRicoProb.Init(ATSTRINGHASH("TIME_BETWEEN_SHOTS_TO_RICCO_PROB", 0x222126F5));
	sm_DistanceToBIPredelay.Init(ATSTRINGHASH("SQD_DISTANCE_TO_BI_PREDELAY", 0x1B11CC02));
	sm_DistanceToHeadShotPredelay.Init(ATSTRINGHASH("SQD_DISTANCE_TO_PLAYER_HEADSHOT_PREDELAY", 0xE99A364));
	sm_DistanceToRicochetsPredelay.Init(ATSTRINGHASH("SQD_DISTANCE_TO_RICOCHETS_PREDELAY", 0x7C0C3C8E));
}

void audCollisionAudioEntity::Shutdown()
{
	audEntity::Shutdown();
}

void audCollisionAudioEntity::UpdateModelEditorSelector()
{
#if __BANK

	//Code that enables point and click generation of Macs objects and also prints out useful
	//information about materials, models, frags etc
	if(sm_EnableModelEditor)
	{
		s32 physicsTypeFlags =  ArchetypeFlags::GTA_ALL_TYPES_MOVER|ArchetypeFlags::GTA_RAGDOLL_TYPE|ArchetypeFlags::GTA_PROJECTILE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE;

		s32 component = -1;
		bool checkMouseClickOnSelectEntity = (ioMouse::GetPressedButtons()&ioMouse::MOUSE_LEFT);
		if(checkMouseClickOnSelectEntity)
		{
			s_PointAndClickEntity = NULL;

			//Find the material clicked on
			Vector3 mouseStart, mouseEnd;
			CDebugScene::GetMousePointing(mouseStart, mouseEnd, false);
			Vector3	StartPos;

			WorldProbe::CShapeTestProbeDesc probeDesc;
			WorldProbe::CShapeTestFixedResults<> probeResults;
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetStartAndEnd(mouseStart, mouseEnd);
			probeDesc.SetIncludeFlags(physicsTypeFlags);

			formatf(s_PointAndClickMaterial, "");
			formatf(s_PointAndClickMaterialOverride, "");
			//Reports the name of the material clicked on
			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				phMaterialMgr::Id id = probeResults[0].GetHitMaterialId();
				const phMaterial &mat = PGTAMATERIALMGR->GetMaterial(id);
				strncpy(s_PointAndClickMaterial, mat.GetName(), 64);
				s_PointAndClickPos = probeResults[0].GetHitPosition();
			}

			//If we've clicked on an entity display some interesting stuff and use it to set up the model creation settings
			//It's not particularly pretty as it gradually accumulates bits and bobs, but it's only used in dev for model creation
			//so doesn't need to be super slick

			Vector3 offset = Vector3(0.f, 0.f, 0.f);
			CEntity* pEntity = CDebugScene::GetEntityUnderMouse(&offset, &component, physicsTypeFlags);
			if(pEntity)
			{
				s_PointAndClickEntity = pEntity;
				//				naDisplayf("");
				char tmpstr[16] = {0};
				s_PointAndClickComponentId = component;
				sprintf(tmpstr, "%i", component);
				strncpy(s_PointAndClickComponent, tmpstr, 32);

				if(pEntity->GetFragInst()) //Do some display stuff and setup for frag props
				{
					fragInst *frag = pEntity->GetFragInst();
					naDisplayf("Entity ptr : %p, Component : %i, numComponents: %i", pEntity, component, frag->GetTypePhysics()->GetNumChildren());

					s_ModelSettingsCreator.numComponents = frag->GetTypePhysics()->GetNumChildren();

					//				fragTypeChild **components = pFragType->GetAllChildren();
					naDisplayf("Attached components ===========");
					for(s32 i=0; i<frag->GetTypePhysics()->GetNumChildren(); i++)
					{
						if(!frag->GetChildBroken(i))
						{
							naDisplayf("	Component %i", i);
						}
					}
					naDisplayf("=====================");
					phBoundComposite* compositeBound = static_cast<phBoundComposite*>(frag->GetArchetype()->GetBound());
					naAssert(compositeBound && frag->GetArchetype()->GetBound()->GetType()==phBound::COMPOSITE);
					naDisplayf("model has %i active bounds", compositeBound->GetNumActiveBounds());
					naDisplayf("model has %i bounds", compositeBound->GetNumBounds());
				}
				else //Not a frag prop;
				{
					naDisplayf("Entity ptr : %p", pEntity);
				}

				if(pEntity->IsArchetypeSet())
				{
					CBaseModelInfo * model = pEntity->GetBaseModelInfo();
					if(model) //If we can find a model for the entity we clicked on, populate the settings creator
					{
						naDisplayf("Selected model: %s", model->GetModelName());

						const char *audioSettingsNamePrefix	= "MOD_";
						char audioSettingsName[64] = "";
						strncpy(audioSettingsName, audioSettingsNamePrefix, 64);
						strncat(audioSettingsName, model->GetModelName(), 64);
						naDisplayf("Model settings ID: %s", audioSettingsName);

						PopulateModelSettingsCreator(model, audioSettingsName, s_ModelSettingsCreator.numComponents);
					}
					else
					{
						naDisplayf("Couldn't get model from entity of type %i, model index: %d", pEntity->GetType(), pEntity->GetModelId().GetModelIndex());
						strncpy(s_ModelSettingsCreator.audioSettingsName, "", 64);
					}
				}
				else
				{
					naDisplayf("Couldn't get model from entity of type %i, model %s", pEntity->GetType(), pEntity->GetModelName());
					strncpy(s_ModelSettingsCreator.audioSettingsName, "", 64);
				}
			}
			else
			{
				strncpy(s_ModelSettingsCreator.audioSettingsName, "", 64);
			}
		}
	}

#endif
}

void audCollisionAudioEntity::PreUpdateService(u32 timeInMs)
{
	//Use sound manager timer to keep it in line with the rest of the collision code
	timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

#if __DEV

	/*
	if (g_AudioDebugEntity)
	{
	f32 maxImpulseMag0 = 0.0f;
	f32 maxImpulseMag1 = 0.0f;
	f32 numCollisions = 0.0f;
	// look for collisions involving that entity
	for(u32 eventIndex=0; eventIndex<g_MaxCollisionEventHistory; eventIndex++)
	{
	if(m_CollisionEventHistory[eventIndex].entities[0] == g_AudioDebugEntity || m_CollisionEventHistory[eventIndex].entities[1] == g_AudioDebugEntity)
	{
	maxImpulseMag0 = Max(m_CollisionEventHistory[eventIndex].impulseMagnitudes[0], maxImpulseMag0);
	maxImpulseMag1 = Max(m_CollisionEventHistory[eventIndex].impulseMagnitudes[1], maxImpulseMag1);
	numCollisions += 1.0f;
	}
	}
	PF_SET(DebugNumCollisions, numCollisions);
	PF_SET(DebugImpulseMag0, maxImpulseMag0);
	PF_SET(DebugImpulseMag1, maxImpulseMag1);
	}
	else
	*/
	{
		PF_SET(DebugNumCollisions, 0.0f);
		PF_SET(DebugImpulseMag0, 0.0f);
		PF_SET(DebugImpulseMag1, 0.0f);
	}

	PF_SET(CollisionHistory, m_CollisionEventHistoryAllocation.CountOnBits());
#endif

	sm_TimeSinceLastBulletHit += fwTimer::GetTimeStepInMilliseconds();
	sm_TimeSinceLastAutomaticBulletHit += fwTimer::GetTimeStepInMilliseconds();
	sm_TimeSinceLastShotgunBulletHit += fwTimer::GetTimeStepInMilliseconds();
	for(u32 eventIndex=0; eventIndex<g_MaxCollisionEventHistory; eventIndex++)
	{
		const bool isAllocated = m_CollisionEventHistoryAllocation.IsSet(eventIndex);
		if(isAllocated)
		{
			audCollisionEvent & collisionEvent =  m_CollisionEventHistory[eventIndex];

			//We can't start a roll or scrape without both entities so don't process if we've lost one
			if(collisionEvent.entities[0] && collisionEvent.entities[1])
			{		
				if(collisionEvent.type == AUD_COLLISION_TYPE_SCRAPE)
				{
					if(collisionEvent.GetFlag(AUD_COLLISION_FLAG_PLAY_SCRAPE))
					{
						if(!PlayScrapeSounds(&collisionEvent))
						{
							collisionEvent.scrapeStopTimeMs = 0;
						}
						collisionEvent.ClearFlag(AUD_COLLISION_FLAG_PLAY_SCRAPE);
					}
					UpdateScrapeSounds(&m_CollisionEventHistory[eventIndex]);
				}

				if(collisionEvent.type == AUD_COLLISION_TYPE_ROLL && collisionEvent.rollUpdatedThisFrame != 0)
				{
					if(collisionEvent.GetFlag(AUD_COLLISION_FLAG_PLAY_ROLL))
					{
						if(!PlayRollSounds(&collisionEvent))
						{
							collisionEvent.rollStopTimeMs = 0;
						}
						collisionEvent.ClearFlag(AUD_COLLISION_FLAG_PLAY_ROLL);
					}
					UpdateRollSounds(&collisionEvent);
				}
			}

			if(timeInMs >= collisionEvent.scrapeStopTimeMs)
			{
				for(u8 i=0; i<2; i++)
				{
					if(collisionEvent.scrapeSounds[i] != NULL)
					{
						collisionEvent.scrapeSounds[i]->StopAndForget();
					}
				}
			}


			for(u8 i=0; i<2; i++)
			{
				if(collisionEvent.rollSounds[i] != NULL)
				{
					// See if it's been updated since last time, and if not, start ramping down it's volume. Then, either way, smooth its volume.
					if (collisionEvent.rollUpdatedThisFrame == 0)
					{
						// Not been updated, so ramp down its volume
						collisionEvent.rollSounds[i]->SetClientVariable(0.0f);
						// If it's silent and we've been waiting a while, kill it
						f32 playbackVolume = collisionEvent.rollSounds[i]->GetRequestedVolume();
						if (playbackVolume<=g_SilenceVolume && timeInMs >= collisionEvent.rollStopTimeMs)
						{
							collisionEvent.rollSounds[i]->StopAndForget();
						}
					}
					// Need to check we still have the sound, as we might have stopped it

					if(collisionEvent.rollSounds[i] != NULL)
					{
						f32 volume = g_SilenceVolume;
						collisionEvent.rollSounds[i]->GetClientVariable(volume);
						f32 currentVolume = audDriverUtil::ComputeLinearVolumeFromDb(collisionEvent.rollSounds[i]->GetRequestedVolume());
						f32 newVolume = 0.0f;
						if (volume>currentVolume)
						{
							newVolume = Min(volume, (currentVolume + g_RollVolumeSmoothUp));
						}
						else
						{
							newVolume = Max(volume, (currentVolume - g_RollVolumeSmoothDown));
						}
						newVolume = Clamp(newVolume, 0.0f, 1.0f);

						f32 newVolumedB = audDriverUtil::ComputeDbVolumeFromLinear(newVolume);
						collisionEvent.rollSounds[i]->SetRequestedVolume(newVolumedB);
					}
				}

				f32 rollVolume = 0.f, scrapeVolume = 0.f, blipVol = 0.f;
				if(collisionEvent.rollSounds[i])
				{
					rollVolume = collisionEvent.rollSounds[i]->GetRequestedVolume();
				}
				if(collisionEvent.scrapeSounds[i])
				{
					scrapeVolume = collisionEvent.scrapeSounds[i]->GetRequestedVolume();
				}

				blipVol = Max(rollVolume, scrapeVolume);
				
				if(blipVol > sm_PlayerContextVolumeForBlip)
				{
					audPlayerCollisionEvent * playerEvent = GetPlayerCollisionEventFromHistory(collisionEvent.entities[i]);
					if(playerEvent)
					{
						playerEvent->lastImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
						playerEvent->blipMap = true;
						playerEvent->position = VECTOR3_TO_VEC3V(collisionEvent.positions[i]);
						playerEvent->blipAmount = audDriverUtil::ComputeLinearVolumeFromDb(blipVol);
					}
				}
			}

			// Mark it as not updated, so we know for next frame 
			collisionEvent.rollUpdatedThisFrame = 0;

		}// if allocated
	}// for each event

	fwPtrNode * link = g_RecentlyCollidedMaterials.GetHeadPtr();
	static u32 lastTime = 0;

	f32 decrement = f32(g_CollisionCountDecreaseRate ) * (timeInMs-lastTime) * 0.001f;

	if(decrement >= 1.f)
	{
		lastTime = timeInMs;

		while(link)
		{
			CollisionMaterialSettings * material = (CollisionMaterialSettings*)link->GetPtr();
			if(material->CollisionCount < decrement)
			{
				material->CollisionCount = 0;
				material->VolumeThreshold = -100.f;
				fwPtrNode * next = link->GetNextPtr();
				link = next;
				g_RecentlyCollidedMaterials.Remove(material);
			}
			else
			{
				material->CollisionCount -=  (u32)(decrement);
				material->VolumeThreshold -= decrement*10.f;
				link = link->GetNextPtr();
			}
		}
	}
#if __BANK
	if( g_IncreaseFootstepsVolume)
	{
		if( !sm_IncreaseFootstepVolumeScene )
		{
			DYNAMICMIXER.StartScene(ATSTRINGHASH("PEDS_FOOTSTEP_DEBUG_SCENE", 0x6BD3FD2C),&sm_IncreaseFootstepVolumeScene);
		}
	}
	else
	{
		if( sm_IncreaseFootstepVolumeScene )
		{
			sm_IncreaseFootstepVolumeScene->Stop();
			sm_IncreaseFootstepVolumeScene = NULL;
		}
	}
#endif
}

void audCollisionAudioEntity::ProcessMacsModelOverrides()
{
	for(u32 chunkId = 0; chunkId < audNorthAudioEngine::GetMetadataManager().GetNumChunks(); chunkId++)
	{
		if(audNorthAudioEngine::GetMetadataManager().GetChunk(chunkId).enabled)
		{
#if __BANK
			if(PARAM_audiotag.Get())
			{
				g_CollisionAudioEntity.ReconnectPropMacsForChunk(chunkId);
			}
#endif
			g_CollisionAudioEntity.ProcessMacsModelOverridesForChunk(chunkId, ATSTRINGHASH("MACS_MODELS_OVERRIDES", 0x53F9AB71));
			g_CollisionAudioEntity.ProcessMacsModelOverridesForChunk(chunkId, ATSTRINGHASH("MACS_MODEL_OVERRIDE_LIST_GEN9", 0x149D39FA));
		}
	}
}

void audCollisionAudioEntity::ProcessMacsModelOverridesForChunk(u32 chunkId, u32 modelListHash)
{
	audMetadataObjectInfo info;
	audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(modelListHash, chunkId, info);
	if (info.GetObjectUntyped() && info.GetType() == MacsModelOverrideList::TYPE_ID)
	{
		const MacsModelOverrideList* overrides = info.GetObject<MacsModelOverrideList>();
		for (int index = 0; index < overrides->numEntrys; index++)
		{
			CBaseModelInfo *base = CModelInfo::GetBaseModelInfoFromHashKey(overrides->Entry[index].Model, NULL);
			//We only want to override models that don't have custom so that we can cleanly remove them later
			if (base && (!base->GetAudioCollisionSettings() || base->GetAudioCollisionSettings() == g_MacsDefault))

			{
				auto mapOverride = sm_MacOverrideMap.Access(overrides->Entry[index].Model);
				if (!mapOverride)
				{
					mapOverride = &sm_MacOverrideMap.Insert(overrides->Entry[index].Model).data;
				}
				(*mapOverride) = overrides->Entry[index].Macs;

				BANK_ONLY(audDisplayf("Added MACS model override for model %s (MACS %s) from chunk %d", base->GetModelName(), audNorthAudioEngine::GetMetadataManager().GetObjectName(overrides->Entry[index].Macs), chunkId);)
			}
			else
			{
				BANK_ONLY(audWarningf("Failed to override model %u with MACS %s", overrides->Entry[index].Model, audNorthAudioEngine::GetMetadataManager().GetObjectName(overrides->Entry[index].Macs));)
			}
		}
	}
}

void audCollisionAudioEntity::RemoveMacsModelOverridesForChunk(u32 chunkId, u32 modelListHash)
{
	audMetadataObjectInfo info;
	audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(modelListHash, chunkId, info);
	if(info.GetObjectUntyped() && info.GetType() == MacsModelOverrideList::TYPE_ID)
	{
		const MacsModelOverrideList* overrides = info.GetObject<MacsModelOverrideList>();
		for(int index=0; index < overrides->numEntrys; index++)
		{
			CBaseModelInfo *base = CModelInfo::GetBaseModelInfoFromHashKey(overrides->Entry[index].Model, NULL);
			if(base)
			{
				base->SetAudioCollisionSettings(audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(ATSTRINGHASH("MOD_DEFAULT", 0x05f03e8c2)), 0);
			}
			auto mapOverride = sm_MacOverrideMap.Access(overrides->Entry[index].Model);
			if(mapOverride)
			{
				(*mapOverride) = ATSTRINGHASH("MOD_DEFAULT", 0x05f03e8c2);
			}
		}
	}
}

#if __BANK
void audCollisionAudioEntity::ReconnectPropMacsForChunk(u32 chunkId)
{
	for(int i=0; i < audNorthAudioEngine::GetMetadataManager().ComputeNumberOfObjects(); i++)
	{
		audMetadataObjectInfo info;
		audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromObjectIndex(i, info);
		if(info.GetType() == ModelAudioCollisionSettings::TYPE_ID && info.GetChunkId() == chunkId)
		{
			CBaseModelInfo *base = CModelInfo::GetBaseModelInfoFromHashKey(atStringHash(&(info.GetName_Debug()[4])), NULL);
			if(base)
			{
				base->SetAudioCollisionSettings(info.GetObject<ModelAudioCollisionSettings>(), atStringHash(info.GetName_Debug()));
			}
		}
	}
}
#endif


void audCollisionAudioEntity::PostUpdate()
{
#if __BANK
	if(s_UpdateCreatedSettings)
	{
		if(audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(s_ModelSettingsCreator.audioSettingsName))
		{
			CModelInfo::GetBaseModelInfoFromHashKey(s_ModelSettingsCreator.model, NULL)->SetAudioCollisionSettings(audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(s_ModelSettingsCreator.audioSettingsName), atStringHash(s_ModelSettingsCreator.audioSettingsName));
			s_UpdateCreatedSettings = false;
		}
	}

	if(s_SpawnRavePropThisFrame)
	{
		s_SpawnRavePropThisFrame = false;
		SpawnRaveSettings();
	}

	GenerateWorldCollisionLists();
#endif

	for(u32 eventIndex=0; eventIndex<g_MaxCollisionEventHistory; eventIndex++)
	{
		const bool isAllocated = m_CollisionEventHistoryAllocation.IsSet(eventIndex);
		if(isAllocated)
		{
			audCollisionEvent & collisionEvent =  m_CollisionEventHistory[eventIndex];
			if(	collisionEvent.rollSounds[0] == NULL &&
				collisionEvent.rollSounds[1] == NULL &&
				collisionEvent.scrapeSounds[0] == NULL &&
				collisionEvent.scrapeSounds[1] == NULL
				)
			{
				//naErrorf("Clearing collision time %d event materials %s, %s, entities %p, %p", audNorthAudioEngine::GetCurrentTimeInMs(), collisionEvent.materialSettings[0] ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(collisionEvent.materialSettings[0]->NameTableOffset) : "", collisionEvent.materialSettings[1]?  audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(collisionEvent.materialSettings[1]->NameTableOffset): "", collisionEvent.entities[0], collisionEvent.entities[1]); 
				
				//We no longer have any active collision sounds, so NULL this event.
				collisionEvent.Init();
				m_CollisionEventHistoryAllocation.Clear(eventIndex);
				m_CollisionEventFreeList.Free(eventIndex);
			}
		}
	}

	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	for(int i=0; i < g_MaxSwingEventHistory; i++)
	{
		if(m_SwingEventHistoryAllocation.IsSet(i))
		{
			bool shouldRemoveEvent = true;
			audSwingEvent &swingEvent = m_SwingEventHistory[i];
			if(swingEvent.swingEnt)
			{
				const ModelAudioCollisionSettings * swingMacs = swingEvent.swingEnt->GetBaseModelInfo()->GetAudioCollisionSettings();
				f32 swingSpeed = ((CPhysical*)swingEvent.swingEnt.Get())->GetVelocity().Mag()*1000.f;

				f32 swingVolume = m_SwingVolCurve.CalculateRescaledValue(0.f, 1.f, swingMacs->MinSwingVel, swingMacs->MaxswingVel, swingSpeed);
				swingEvent.volSmoother.SetRate(g_SwingVolSmoothRate);
				f32 smoothedVol = swingEvent.volSmoother.CalculateValue(swingVolume, now-swingEvent.lastProcessTime);
				f32 swingVolumeDb = audDriverUtil::ComputeDbVolumeFromLinear(smoothedVol);

#if __BANK
				if(g_DebugSwingingProps)
				{
					naDisplayf("Swing speed %f, volumeDb %f on prop %s (%p)", swingSpeed, swingVolumeDb, swingEvent.swingEnt->GetModelName(), swingEvent.swingEnt.Get());
				}
#endif

				if(swingVolumeDb > -60.f) 
				{

					shouldRemoveEvent = false;
					bool needsToPlay = false;
					if(!swingEvent.swingSound)
					{
						audSoundInitParams initParams;
						initParams.EnvironmentGroup = swingEvent.swingEnt->GetAudioEnvironmentGroup();
						CreateSound_PersistentReference(swingMacs->SwingSound, &swingEvent.swingSound, &initParams);
						needsToPlay = true;
					}

					if(swingEvent.swingSound)
					{
						swingEvent.swingSound->SetRequestedVolume(swingVolumeDb);
						swingEvent.swingSound->SetRequestedPosition(swingEvent.swingEnt->GetTransform().GetPosition());
						swingEvent.swingSound->FindAndSetVariableValue(ATSTRINGHASH("LastImpactTime", 0x601C8B48),(f32)(now - swingEvent.lastImpactTime));
						if(needsToPlay)
						{
							swingEvent.swingSound->PrepareAndPlay();
						}
					}
				}
			}

			if(shouldRemoveEvent)
			{
				swingEvent.Init();
				m_SwingEventHistoryAllocation.Clear(i);
				m_SwingEventFreeList.Free(i);
			}
		}
	}

	for(int i=0; i < g_MaxPlayerCollisionEventHistory; i++)
	{
		if(m_PlayerCollisionEventHistoryAllocation.IsSet(i))
		{
			audPlayerCollisionEvent & playerEvent = m_PlayerCollisionEventHistory[i];
			bool shouldRemoveEvent = false;

			if(playerEvent.hitEntity && playerEvent.hitEntity->GetIsPhysical())
			{
				CPhysical * physical = (CPhysical*)playerEvent.hitEntity.Get();
				f32 speedSqr = physical->GetVelocity().Mag2();

				if(speedSqr > sm_PlayerCollisionEventSpeedThreshold)
				{
					if(playerEvent.blipMap)
					{
						CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
						if (pPlayerPed)
						{ 
							playerEvent.position = pPlayerPed->GetTransform().GetPosition(); 
							CMiniMap::CreateSonarBlipAndReportStealthNoise(pPlayerPed, playerEvent.position, CMiniMap::sm_Tunables.Sonar.fSoundRange_ObjectCollision * playerEvent.blipAmount, HUD_COLOUR_BLUEDARK, true, false);
						}
					}
				
					playerEvent.lastImpactTime = now;
				}
			}
			else
			{
				shouldRemoveEvent = true;
			}

			if(now - playerEvent.lastImpactTime > sm_PlayerCollisionEventTimeout)
			{
				shouldRemoveEvent = true;
			}

			playerEvent.blipMap = false;

			if(shouldRemoveEvent)
			{
				playerEvent.Init();
				m_PlayerCollisionEventHistoryAllocation.Clear(i);
				m_PlayerCollisionEventFreeList.Free(i);
			}
		}
	}
}

#if __BANK
void SpawnModel(CBaseModelInfo * pModel, fwModelId  modelId)
{
	//Spawn in an instance of the object so we can set up the MACS using the model editor
	
	if(s_SpawnedCollisionProp)
	{
		if(CGameWorld::IsDeleteAllowed())
		{
			CGameWorld::Remove(s_SpawnedCollisionProp);

			 if(s_SpawnedCollisionProp->GetIsTypeObject())
			{
				CObjectPopulation::DestroyObject((CObject*)s_SpawnedCollisionProp.Get());
			}
			else
			{
				delete s_SpawnedCollisionProp;
			}
		}

		s_SpawnedCollisionProp = NULL;
	}

	Matrix34 testMat;
	testMat.Identity();

	Vector3 vecCreateOffset = camInterface::GetFront();
	vecCreateOffset.z = 0.0f;
	vecCreateOffset *= 2.f + pModel->GetBoundingSphereRadius();

	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	testMat.d = debugDirector.IsFreeCamActive() ? camInterface::GetPos() : CGameWorld::FindLocalPlayerCoors();
	testMat.d += vecCreateOffset;

	if(debugDirector.IsFreeCamActive())
	{
		testMat.Set3x3(camInterface::GetMat());
	}
	else if(CGameWorld::FindLocalPlayer())
	{
		testMat.Set3x3(MAT34V_TO_MATRIX34(CGameWorld::FindLocalPlayer()->GetMatrix()));
	}

	if(pModel)
	{
		bool bForceLoad = false;
		if(!CModelInfo::HaveAssetsLoaded(modelId))
		{
			CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			bForceLoad = true;
		}

		if(bForceLoad)
		{
			CStreaming::LoadAllRequestedObjects(true);
		}

		if(pModel->GetIsTypeObject())
		{
			s_SpawnedCollisionProp = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_RANDOM, true);
		} 
		else
		{
			if (pModel->GetModelType() == MI_TYPE_COMPOSITE)
			{
				s_SpawnedCollisionProp = rage_new CCompEntity( ENTITY_OWNEDBY_AUDIO );
			}
			else if(pModel->GetClipDictionaryIndex() != -1 && pModel->GetHasAnimation())
			{
				s_SpawnedCollisionProp = rage_new CAnimatedBuilding( ENTITY_OWNEDBY_AUDIO );
			}
			else
			{
				s_SpawnedCollisionProp = rage_new CBuilding( ENTITY_OWNEDBY_DEBUG );
			}
			s_SpawnedCollisionProp->SetModelId(modelId);
		}
	}

	if(s_SpawnedCollisionProp)
	{
		if (!s_SpawnedCollisionProp->GetTransform().IsMatrix())
		{
#if ENABLE_MATRIX_MEMBER
			Mat34V m(V_IDENTITY);			
			s_SpawnedCollisionProp->SetTransform(m);
			s_SpawnedCollisionProp->SetTransformScale(1.0f, 1.0f);
			
#else
			fwTransform *trans = rage_new fwMatrixTransform();
			s_SpawnedCollisionProp->SetTransform(trans);
#endif
		}

		s_SpawnedCollisionProp->SetMatrix(testMat, true);

		if (fragInst* pInst = s_SpawnedCollisionProp->GetFragInst())
		{
			pInst->SetResetMatrix(testMat);
		}

		CGameWorld::Add(s_SpawnedCollisionProp, CGameWorld::OUTSIDE );
	}

	// create physics for static xrefs
	if (s_SpawnedCollisionProp && s_SpawnedCollisionProp->GetIsTypeBuilding()
		&& s_SpawnedCollisionProp->GetCurrentPhysicsInst()==NULL && s_SpawnedCollisionProp->IsBaseFlagSet(fwEntity::HAS_PHYSICS_DICT))
	{
		// create physics
		s_SpawnedCollisionProp->InitPhys();
		if (s_SpawnedCollisionProp->GetCurrentPhysicsInst())
		{
			s_SpawnedCollisionProp->AddPhysics();
		}
	}

	//If the entity being is an env cloth then we need to transform the cloth
	//verts to the entity frame.
	if(s_SpawnedCollisionProp && s_SpawnedCollisionProp->GetCurrentPhysicsInst() && s_SpawnedCollisionProp->GetCurrentPhysicsInst()->GetArchetype())
	{
		// TODO: Should this really be checking if it has the GTA_ENVCLOTH_OBJECT_TYPE flag, not that it has *only* the GTA_ENVCLOTH_OBJECT_TYPE flag?
		if(PHLEVEL->GetInstanceTypeFlags(s_SpawnedCollisionProp->GetCurrentPhysicsInst()->GetLevelIndex()) == ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE)
		{
			//Now transform the cloth verts to the frame of the cloth entity.
			phInst* pEntityInst=s_SpawnedCollisionProp->GetCurrentPhysicsInst();
			Assertf(dynamic_cast<fragInst*>(pEntityInst), "Physical with cloth must be a frag");
			fragInst* pEntityFragInst=static_cast<fragInst*>(pEntityInst);
			Assertf(pEntityFragInst->GetCached(), "Cloth entity has no cache entry");
			if(pEntityFragInst->GetCached())
			{
				fragCacheEntry* pEntityCacheEntry=pEntityFragInst->GetCacheEntry();
				Assertf(pEntityCacheEntry, "Cloth entity has a null cache entry");
				fragHierarchyInst* pEntityHierInst=pEntityCacheEntry->GetHierInst();
				Assertf(pEntityHierInst, "Cloth entity has a null hier inst");
				environmentCloth* pEnvCloth=pEntityHierInst->envCloth;
				Matrix34 clothMatrix=MAT34V_TO_MATRIX34(s_SpawnedCollisionProp->GetMatrix());
				
				pEnvCloth->SetInitialPosition( VECTOR3_TO_VEC3V(clothMatrix.d) );
			}
		}
	}//	if(s_SpawnedCollisionProp && s_SpawnedCollisionProp->GetCurrentPhysicsInst() && s_SpawnedCollisionProp->GetCurrentPhysicsInst()->GetArchetype())...

	if (s_SpawnedCollisionProp && s_SpawnedCollisionProp->GetCurrentPhysicsInst())
	{
		PHSIM->ActivateObject(s_SpawnedCollisionProp->GetCurrentPhysicsInst());
	}

	u32 index = modelId.GetModelIndex();
	Displayf("[NorthAudio] MACS editor: Spawned %s (index %d)", fwArchetypeManager::GetArchetypeName(index), index);

	if(s_SpawnedCollisionProp && s_SpawnedCollisionProp->GetIsTypeObject())
	{
		(static_cast<CObject*>(s_SpawnedCollisionProp.Get()))->PlaceOnGroundProperly();
	}

}

void SpawnPropFromPropName()
{
	fwModelId modelId;
	
	CBaseModelInfo * pModel = CModelInfo::GetBaseModelInfoFromName(s_PropSpawnName, &modelId);	
	if(!pModel)
	{
		return; 
	}

	//Check if the model is using MOD_DEFAULT, has physics and is not a weapon or a ped or a vehicle
	//if(!strcmp(pModel->GetModelName(), s_PropSpawnName))
	{
		SpawnModel(pModel, modelId);
		return;
	}
}

void audCollisionAudioEntity::SpawnRaveSettings()
{
	if(!s_RaveSelectedMaterialSettings || !s_RaveSelectedMaterialSettingsName[0])
	{
		naDisplayf("NO RAVE MODEL SETTINGS SELECTED");
		return;
	}

#if 0 //useful for debug
	const u32 maxAudioSettingsNameLength		= 128;
	const char *audioSettingsNamePrefix	= "MOD_";
	char audioSettingsName[maxAudioSettingsNameLength];
#endif

	//Iterate through all loaded models and fix any that have gotten out of synch
	//const u32 maxModelInfos = fwArchetypeManager::GetMaxArchetypeIndex();
	fwModelId modelId;
	CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfoFromName(&s_RaveSelectedMaterialSettingsName[4], &modelId);

	if(!pModel)
	{
		naErrorf("Couldn't get model info for %s when spawning rave settings; perhaps it doesn't exist?", &s_RaveSelectedMaterialSettingsName[4]);
		return;
	}

	SpawnModel(pModel, modelId);
	return;
}


void audCollisionAudioEntity::HandleRaveSelectedMaterial(const ModelAudioCollisionSettings * settings)
{
	if(settings)
	{
		s_RaveSelectedMaterialSettings = settings;
		formatf(s_RaveSelectedMaterialSettingsName,"%s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(settings->NameTableOffset));

		if(s_AutoSpawnRaveObjects && sm_EnableModelEditor)
		{
			s_SpawnRavePropThisFrame = true;
		}
	}
}

void audCollisionAudioEntity::SimulateCollision()
{
	audCollisionEvent collisionEvent;
	collisionEvent.Init();

	collisionEvent.impulseMagnitudes[0] = s_SimulateCollisionVel * GetAudioWeightScaling((AudioWeight)s_SimulateCollisionWeight1, (AudioWeight)s_SimulateCollisionWeight2);
	collisionEvent.impulseMagnitudes[1] = 0.f;
	collisionEvent.ClearFlag(AUD_COLLISION_FLAG_MELEE);
	collisionEvent.entities[0] = s_PointAndClickEntity;
	collisionEvent.positions[0] = s_PointAndClickPos;
	if(strlen(s_PointAndClickMaterial) > 0)
	{
		s32 materialIndex = -1;
		for(s32 i=0;  i<g_audCollisionMaterialNames.GetCount(); ++i)
		{
			if(strstr(g_audCollisionMaterialNames[i].name, s_PointAndClickMaterial))
			{	
				materialIndex = i;
				break;
			}
		}
		if(naVerifyf(materialIndex >=0, "Couldn't find the selected material in our audio materials list - something is out of sync"))
		{
			collisionEvent.materialSettings[0] = GetMaterialOverride(s_PointAndClickEntity, g_audCollisionMaterials[materialIndex], s_PointAndClickComponentId);
			collisionEvent.materialSettings[1] = NULL;
			g_CollisionAudioEntity.ProcessImpactSounds(&collisionEvent, s_SimulateCollisionSoftImpact);
		}
	}
}

void audCollisionAudioEntity::ClearDebugHistory()
{
	for(u32 i = 0; i < g_NumDebugCollisionInfos; i++)
	{
		g_CollisionDebugInfo[i].hidden = true;
	}
}

void audCollisionAudioEntity::PopulateModelSettingsCreator(CBaseModelInfo * model, const char * name, u32 numComponents)
{
	//Make sure we're starting from a clean slate
	sysMemSet(&s_ModelSettingsCreator, 0, sizeof(s_ModelSettingsCreator));
	strncpy(s_ModelSettingsCreator.audioSettingsName, name, 64);
	s_ModelSettingsCreator.numComponents = numComponents;
	s_ModelSettingsCreator.model = model->GetHashKey();

	phArchetype * phtype = model->GetPhysics();

	if(strlen(s_PointAndClickMaterial) > 0)
	{
		CollisionMaterialSettings *materialSettings = NULL;
		s32 materialIndex = -1;
		for(s32 i=0;  i<g_audCollisionMaterialNames.GetCount(); ++i)
		{
			if(strstr(g_audCollisionMaterialNames[i].name, s_PointAndClickMaterial))
			{	
				materialIndex = i;
			}
		}
		if(naVerifyf(materialIndex >=0, "Couldn't find the selected material in our audio materials list - something is out of sync"))
		{
			materialSettings = GetMaterialOverride(s_PointAndClickEntity, g_audCollisionMaterials[materialIndex], s_PointAndClickComponentId);
			if(materialSettings)
			{
				strncpy(s_PointAndClickMaterialOverride, audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings->NameTableOffset), 64);
			}
		}
	}


	if(phtype) //Simple, non-frag models go through this route
	{
		phBound * bound = phtype->GetBound();
		if(bound USE_GRIDS_ONLY(&& bound->GetType() != phBound::GRID) && bound->GetType() != phBound::COMPOSITE)
		{
			s32 numMats = bound->GetNumMaterials();
			CollisionMaterialSettings * usedMaterials[64] = {NULL}; //an array to keep track of what materials have been set in the model
			naDisplayf("Num materials is %i", numMats);

			for(s32 i=0; i< numMats; i++)
			{
				naDisplayf("Material id is %" I64FMT "u", bound->GetMaterialId(i));
				
				//Check to see this material hasn't already been overridden
				s32 matid = static_cast<s32>(PGTAMATERIALMGR->UnpackMtlId(bound->GetMaterialId(i)));
				s32 j = 0;
				bool bailout = false;
				while(usedMaterials[j] && j<numMats)
				{
					if(usedMaterials[j] == g_audCollisionMaterials[matid])
					{
						bailout = true;
						break;
					}
					j++;
				}

				if(bailout)
				{
					//material has already been overridden...try the next one
					continue;
				}

				if(naVerifyf(s_ModelSettingsCreator.numMaterials<ModelAudioCollisionSettings::MAX_MATERIALOVERRIDES, "Model %s has more materials than the max material overrideson ModelAudioCollisonSettings: get a coder to increase this", model->GetModelName()))
				{
					usedMaterials[s_ModelSettingsCreator.numMaterials] = g_audCollisionMaterials[matid]; 
					strncpy(s_ModelSettingsCreator.materialNames[s_ModelSettingsCreator.numMaterials], g_audCollisionMaterialNames[matid].name, 64);
					s_ModelSettingsCreator.numMaterials++; 
				}
			}
		}
		else if(bound && (bound->GetType() == phBound::COMPOSITE))
		{
			phBoundComposite * composite = (phBoundComposite *)phtype->GetBound();
			if(composite)
			{
				naDisplayf("Model has composite bounds");
				CollisionMaterialSettings * usedMaterials[64] = {NULL}; //an array to keep track of what materials have been set in the model
				s_ModelSettingsCreator.numComponents = composite->GetNumBounds();
				for(s32 k=0; k<composite->GetNumBounds(); k++)
				{	
					phBound * bound = composite->GetBound(k);
					if(bound) //Since we got the composite bound from the model, not the entity, it should be the full unbroken bound...but we'll check anyhow
					{
						s32 numMats = bound->GetNumMaterials();
						naDisplayf("Component/bound: %i", k);

						for(s32 i=0; i< numMats; i++)
						{
							//Check to see this material hasn't already been overridden
							s32 matid = static_cast<s32>(PGTAMATERIALMGR->UnpackMtlId(bound->GetMaterialId(i)));
							bool bailout = false;
							s32 j = 0;
							while(usedMaterials[j] && j<64)
							{
								if(usedMaterials[j] == g_audCollisionMaterials[matid])
								{
									bailout = true;
									break;
								}
								j++;  
							}

							if(bailout)
							{
								//material has already been overridden...try the next one
								continue;
							}

							if(naVerifyf(s_ModelSettingsCreator.numMaterials<ModelAudioCollisionSettings::MAX_MATERIALOVERRIDES, "Model %s has more materials than the max material overrideson ModelAudioCollisonSettings: get a coder to increase this", model->GetModelName()))
							{
								usedMaterials[s_ModelSettingsCreator.numMaterials] = g_audCollisionMaterials[matid]; 
								strncpy(s_ModelSettingsCreator.materialNames[s_ModelSettingsCreator.numMaterials], g_audCollisionMaterialNames[matid].name, 64);
								naDisplayf("Material name: %s", s_ModelSettingsCreator.materialNames[s_ModelSettingsCreator.numMaterials]);
								s_ModelSettingsCreator.numMaterials++; 
							}
						}
					}
				}
			}
		}
		else
		{
			naWarningf("Selected model for %s doesn't have a bound so we can't find it's materials", name);
			sysMemSet(&s_ModelSettingsCreator, 0, sizeof(s_ModelSettingsCreator));
		} 
	}
	else if(model->GetFragType() && model->GetFragType()->GetPhysics(0)) //Not a simple model but could be a frag
	{		
		phtype = (phArchetype *) model->GetFragType()->GetPhysics(0)->GetArchetype();
		if(phtype) 
		{ 
			naAssert(phtype->GetBound() && phtype->GetBound()->GetType()==phBound::COMPOSITE); //Just making sure we're actually looking at a composite bound
			phBoundComposite * composite = (phBoundComposite *)phtype->GetBound();
			if(composite)
			{
				naDisplayf("Model has composite bounds");
				CollisionMaterialSettings * usedMaterials[64] = {NULL}; //an array to keep track of what materials have been set in the model
				for(s32 k=0; k<composite->GetNumBounds(); k++)
				{	
					phBound * bound = composite->GetBound(k);
					if(bound) //Since we got the composite bound from the model, not the entity, it should be the full unbroken bound...but we'll check anyhow
					{
						s32 numMats = bound->GetNumMaterials();
						naDisplayf("Component/bound: %i", k);

						for(s32 i=0; i< numMats; i++)
						{
							//Check to see this material hasn't already been overridden
							s32 matid = static_cast<s32>(PGTAMATERIALMGR->UnpackMtlId(bound->GetMaterialId(i)));
							bool bailout = false;
							s32 j = 0;
							while(usedMaterials[j] && j<64)
							{
								if(usedMaterials[j] == g_audCollisionMaterials[matid])
								{
									bailout = true;
									break;
								}
								j++;
							}

							if(bailout)
							{
								//material has already been overridden...try the next one
								continue;
							}

							if(naVerifyf(s_ModelSettingsCreator.numMaterials<ModelAudioCollisionSettings::MAX_MATERIALOVERRIDES, "Model %s has more materials than the max material overrideson ModelAudioCollisonSettings: get a coder to increase this", model->GetModelName()))
							{
								usedMaterials[s_ModelSettingsCreator.numMaterials] = g_audCollisionMaterials[matid]; 
								strncpy(s_ModelSettingsCreator.materialNames[s_ModelSettingsCreator.numMaterials], g_audCollisionMaterialNames[matid].name, 64);
								naDisplayf("Material name: %s", s_ModelSettingsCreator.materialNames[s_ModelSettingsCreator.numMaterials]);
								s_ModelSettingsCreator.numMaterials++; 
							}
						}
					}
				}
			}
			else
			{
				naWarningf("Selected model for %s doesn't have a bound so we can't find it's materials", name);
				sysMemSet(&s_ModelSettingsCreator, 0, sizeof(s_ModelSettingsCreator));
			}

		}
		else
		{
			naAssertf(phtype, "Selected model for %s doesn't have a phArchetype so we can't find it's materials", name);
			sysMemSet(&s_ModelSettingsCreator, 0, sizeof(s_ModelSettingsCreator));
		}
	}
}

void audCollisionAudioEntity::CreateModelAudioSettings()
{

	if(strlen(s_ModelSettingsCreator.audioSettingsName) == 0)
	{
		return; //No model selected, nothing to create
	}

	naDisplayf("Model creator name is: %s", s_ModelSettingsCreator.audioSettingsName);

	char xmlMsg[16384];

	FormatMacsXml(xmlMsg, s_ModelSettingsCreator.audioSettingsName, s_ModelSettingsCreator.numComponents); 

	audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
	bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
	naAssertf(success, "Failed to send xml message to rave");
	naDisplayf("%s", (success)? "Success":"Failed");

	//The intention here is to allow auditioning of newly assigned MACS without requiring a restart but for reasons unknown, instead it kills all sound...
	if(success)
	{ 
		s_UpdateCreatedSettings = true;
		//CModelInfo::GetBaseModelInfoFromHashKey(s_ModelSettingsCreator.model, NULL)->SetAudioCollisionSettings(audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(s_ModelSettingsCreator.audioSettingsName));
	}

} 

//Macs: Model Audio Collision Settings
//the array xmlMsg points to should be pretty big to take the xml message
void audCollisionAudioEntity::FormatMacsXml(char * xmlMsg, const char * settingsName, u32 numComponents, bool overrideMaterials, bool emptyComponents)
{
	const audMetadataManager &metadataManager = SOUNDFACTORY.GetMetadataManager();
	ModelAudioCollisionSettings * settings =  audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(settingsName);		

	atString upperSettings(settingsName);
	upperSettings.Uppercase();

	atArray<atString> upperMaterials;
	upperMaterials.Resize(s_ModelSettingsCreator.numMaterials);

	for(int i=0; i<s_ModelSettingsCreator.numMaterials; i++)
	{
		upperMaterials[i] = s_ModelSettingsCreator.materialNames[i];
		upperMaterials[i].Uppercase();
	}

	sprintf(xmlMsg, "<RAVEMessage>\n");
	naDisplayf("%s", xmlMsg);
	char tmpBuf[256] = {0};
	sprintf(tmpBuf, "	<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", ATSTRINGHASH("BASE", 0x44E21C90));
	naDisplayf("%s", tmpBuf);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "		<ModelAudioCollisionSettings name=\"%s\">\n", &upperSettings[0]);
	naDisplayf("%s", tmpBuf);
	strcat(xmlMsg, tmpBuf);

	//If we already have an instance of this Macs, we preserve the non-material information
	if (settings && g_PreserveMACSSettingsOnExport)
	{
		char breakSound[64] = "NULL_SOUND";
		char destroySound[64] = "NULL_SOUND";
		char uprootSound[64] = "NULL_SOUND";
		char swingSound[64] = "NULL_SOUND";
		char rainLoop[64] = "NULL_SOUND";
		char shockwaveSound[64] = "NULL_SOUND";
		char randomAmbient[64] = "NULL_SOUND";
		char entityResonance[64] = "NULL_SOUND";
		
		safecpy(breakSound, metadataManager.GetObjectName(settings->BreakSound));
		safecpy(destroySound, metadataManager.GetObjectName(settings->DestroySound));
		safecpy(uprootSound, metadataManager.GetObjectName(settings->UprootSound));
		safecpy(swingSound, metadataManager.GetObjectName(settings->SwingSound));
		safecpy(rainLoop, metadataManager.GetObjectName(settings->RainLoop));
		safecpy(shockwaveSound, metadataManager.GetObjectName(settings->ShockwaveSound));
		safecpy(randomAmbient, metadataManager.GetObjectName(settings->RandomAmbient));
		safecpy(entityResonance, metadataManager.GetObjectName(settings->EntityResonance));
		

		if (settings->MediumIntensity != 0u)
		{
			sprintf(tmpBuf, "			<MediumIntensity>%s</MediumIntensity>\n", metadataManager.GetObjectName(settings->MediumIntensity)); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		}
		
		if (settings->HighIntensity != 0u)
		{
			sprintf(tmpBuf, "			<HighIntensity>%s</HighIntensity>\n", metadataManager.GetObjectName(settings->HighIntensity)); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		}
		
		sprintf(tmpBuf, "			<BreakSound>%s</BreakSound>\n", breakSound); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<DestroySound>%s</DestroySound>\n", destroySound); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<UprootSound>%s</UprootSound>\n", uprootSound); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);

		if (settings->WindSounds != 0u)
		{
			sprintf(tmpBuf, "			<WindSounds>%s</WindSounds>\n", metadataManager.GetObjectName(settings->WindSounds)); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		}
		
		sprintf(tmpBuf, "			<SwingSound>%s</SwingSound>\n", swingSound); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<MinSwingVel>%.02f</MinSwingVel>\n", settings->MinSwingVel); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<MaxswingVel>%.02f</MaxswingVel>\n", settings->MaxswingVel); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<RainLoop>%s</RainLoop>\n", rainLoop); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<ShockwaveSound>%s</ShockwaveSound>\n", shockwaveSound); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<RandomAmbient>%s</RandomAmbient>\n", randomAmbient); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<EntityResonance>%s</EntityResonance>\n", entityResonance); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<Weight>%s</Weight>\n", AudioWeight_ToString((AudioWeight)settings->Weight)); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<IsResonant>%s</IsResonant>\n", AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_ISRESONANT) == AUD_TRISTATE_TRUE ? "yes" : "no"); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<TurnOffRainVolOnProps>%s</TurnOffRainVolOnProps>\n", AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_TURNOFFRAINVOLONPROPS) == AUD_TRISTATE_TRUE ? "yes" : "no"); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<SwingingProp>%s</SwingingProp>\n", AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_SWINGINGPROP) == AUD_TRISTATE_TRUE ? "yes" : "no"); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<RainLoopTracksListener>%s</RainLoopTracksListener>\n", AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_RAINLOOPTRACKSLISTENER) == AUD_TRISTATE_TRUE ? "yes" : "no"); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<PlaceRainLoopOnTopOfBounds>%s</PlaceRainLoopOnTopOfBounds>\n", AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_PLACERAINLOOPONTOPOFBOUNDS) == AUD_TRISTATE_TRUE ? "yes" : "no"); naDisplayf("%s", tmpBuf); strcat(xmlMsg, tmpBuf);		
	}
	
	if(overrideMaterials)
	{
		for(u32 i=0; i<s_ModelSettingsCreator.numMaterials; i++)
		{
			sprintf(tmpBuf, "			<MaterialOverride>\n");
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg, tmpBuf);
			sprintf(tmpBuf, "				<Material>%s</Material>\n", &upperMaterials[i][0]);
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg, tmpBuf);
			sprintf(tmpBuf, "				<Override>%s</Override>\n", &upperMaterials[i][0]);
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg, tmpBuf);
			sprintf(tmpBuf, "			</MaterialOverride>\n");
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg, tmpBuf);
		}
	}
	if(emptyComponents)
	{
		for(u32 i=0; i < numComponents; i++)
		{
			sprintf(tmpBuf, "			<FragComponentSetting>\n");
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg, tmpBuf);
			sprintf(tmpBuf, "			</FragComponentSetting>\n");
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg,  tmpBuf);
		}
	}
	else
	{
		for(u32 i=0; i < numComponents; i++)
		{
			sprintf(tmpBuf, "			<FragComponentSetting>\n");
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg, tmpBuf);
			sprintf(tmpBuf, "				<ModelSettings>%s_%i</ModelSettings>\n", &upperSettings[0], i);
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg, tmpBuf);
			sprintf(tmpBuf, "			</FragComponentSetting>\n");
			naDisplayf("%s", tmpBuf);
			strcat(xmlMsg,  tmpBuf);
		}
	}
	sprintf(tmpBuf, "		</ModelAudioCollisionSettings>\n");
	naDisplayf("%s", tmpBuf);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "	</EditObjects>\n");
	naDisplayf("%s", tmpBuf);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "</RAVEMessage>\n");
	naDisplayf("%s", tmpBuf);
	strcat(xmlMsg, tmpBuf);

	naDisplayf("XML message is %d characters long \n", istrlen(xmlMsg));


}

void audCollisionAudioEntity::CreateIndividualFragModelAudioSettings()
{	
	if((strlen(s_ModelSettingsCreator.audioSettingsName) == 0) || !s_ModelSettingsCreator.numComponents) //only fraggable models should be set up here
	{
		naDisplayf("Non frag model in CreateFragModelAudioSettings: bailing out");
		naDisplayf("Num components: %i", s_ModelSettingsCreator.numComponents);
		return;
	}

	naDisplayf("Model creator name is: %s", s_ModelSettingsCreator.audioSettingsName);


	char xmlMsg[4096];
	char componentName[128];
	char suffix[8];


	sprintf(componentName, "%s", s_ModelSettingsCreator.audioSettingsName);
	sprintf(suffix, "_%i", s_PointAndClickComponentId);
	strcat(componentName, suffix);
	naDisplayf("Model component name is: %s", componentName);
	FormatMacsXml(xmlMsg, componentName, 0);

	audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
	bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
	naAssertf(success, "Failed to send xml message to rave");
	naDisplayf("%s", (success)? "Success":"Failed");
	
}

void audCollisionAudioEntity::CreateFragModelAudioSettings()
{
	if((strlen(s_ModelSettingsCreator.audioSettingsName) == 0) || s_ModelSettingsCreator.numComponents <= 1) //only fraggable models should be set up here
	{
		naDisplayf("Non frag model in CreateFragModelAudioSettings: bailing out");
		naDisplayf("Num components: %i", s_ModelSettingsCreator.numComponents);
		return;
	}

	naDisplayf("Model creator name is: %s", s_ModelSettingsCreator.audioSettingsName);

	u32 numComponents = s_ModelSettingsCreator.numComponents; 

	char xmlMsg[4096];
	char componentName[128];
	char suffix[8];


	//Format the frag component settings
	for(u32 i = 0; i < numComponents; i++)
	{
		sprintf(componentName, "%s", s_ModelSettingsCreator.audioSettingsName);
		sprintf(suffix, "_%i", i);
		strcat(componentName, suffix);
		naDisplayf("Model component name is: %s", componentName);
		FormatMacsXml(xmlMsg, componentName, 0, false);

		audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
		bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
		naAssertf(success, "Failed to send xml message to rave");
		naDisplayf("%s", (success)? "Success":"Failed");


		//Okay this is a bit of a hack...RAVE doesn't like having messages sent too frequently and
		//can lose some randomly while still claiming success if they are, so we'll spend a while 
		//sitting round twiddling our thumbs after each one. If Macs seem to be randomly missing components
		//for frag models it could be worth checking that it's waiting long enough between messages.

		u32 time = 100000;
		naDisplayf("Waiting for RAVE... Component %i", i);
		while(time)
		{
			time--;
			//Do some pointless stuff to take up time
			u32 fudge = time*time;
			(void)fudge;
			sprintf(xmlMsg, "Waiting for rave %i", fudge);
		}	
		naDisplayf("Done waiting :)");
	}

	//Format the base settings object
	FormatMacsXml(xmlMsg, s_ModelSettingsCreator.audioSettingsName, numComponents, true, false);

	audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
	bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
	naAssertf(success, "Failed to send xml message to rave");
	naDisplayf("%s", (success)? "Success":"Failed");

	if(success)
	{
		CModelInfo::GetBaseModelInfoFromHashKey(s_ModelSettingsCreator.model, NULL)->SetAudioCollisionSettings(audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(s_ModelSettingsCreator.audioSettingsName), atStringHash(s_ModelSettingsCreator.audioSettingsName));
	}

}

void audCollisionAudioEntity::SynchronizeMacsAndModels()
{
	const u32 maxAudioSettingsNameLength		= 128;
	const char *audioSettingsNamePrefix	= "MOD_";
	char audioSettingsName[maxAudioSettingsNameLength];

	fiStream *stream = fiStream::Create("MACS_SynchronizeList.txt");

	if(!stream)
	{
		naErrorf("Couldn't open x:gta5/tools/bin/MACS_SynchronizeList.txt");
		return;
	}

	naDisplayf("Writing sync list to: x:gta5/tools/bin/MACS_SynchronizeList.txt");


	//Iterate through all loaded models and fix any that have gotten out of synch
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for(u32 i=0; i< maxModelInfos; i++)
	{
		CBaseModelInfo * pModel = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));	
		if(!pModel)
		{
			continue; 
		}

		fwModelId modelId((strLocalIndex(i)));

		//Try get a Macs for the current model
		strcpy(audioSettingsName, audioSettingsNamePrefix);
		strcat(audioSettingsName, CModelInfo::GetBaseModelInfoName(modelId));
		ModelAudioCollisionSettings * moda = audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(audioSettingsName);

		if(moda && moda != g_MacsDefault && pModel->GetPhysics() && AreMacsMaterialsOutOfDate(moda))
		{
			const fragType* pFragType = pModel->GetFragType();
			u32 numChildren = 0;
			if(pFragType)
			{
				numChildren = pFragType->GetPhysics(0)->GetNumChildren();
			}

			PopulateModelSettingsCreator(pModel, audioSettingsName, numChildren);

			if(numChildren)
			{
				Displayf("[North Audio] ======= MACS Frag materials out of date: %s", audioSettingsName);
				fprintf(stream, "[North Audio] ======= MACS Frag materials out of date: %s\n", CModelInfo::GetBaseModelInfoName(modelId));
				//CreateFragModelAudioSettings();
			}
			else
			{
				Displayf("[North Audio] ======= MACS materials out of date: %s", audioSettingsName);
				fprintf(stream, "[North Audio] ======= MACS materials out of date: %s\n", CModelInfo::GetBaseModelInfoName(modelId));
				//CreateModelAudioSettings();
			}
		}
	}

	stream->Close();
}


static s32 newModelSpawnIndex = 0;
static s32 syncModelSpawnIndex = 0;
static s32 batchModelCreateStartIndex = 0;
static s32 batchModelCreateStopIndex = 0;

void audCollisionAudioEntity::BatchCreateMacs()
{
	const u32 maxAudioSettingsNameLength		= 128;
	const char *audioSettingsNamePrefix	= "MOD_";
	char audioSettingsName[maxAudioSettingsNameLength];

	u32 currentIndex = 0;
	//Iterate through all loaded models and fix any that have gotten out of synch
	const s32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for(u32 i=0; i< maxModelInfos; i++)
	{
		CBaseModelInfo * pModel = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));	
		if(!pModel)
		{
			continue;
		}

		fwModelId modelId((strLocalIndex(i)));

		//Try get a Macs for the current model
		strcpy(audioSettingsName, audioSettingsNamePrefix);
		strcat(audioSettingsName, CModelInfo::GetBaseModelInfoName(modelId));

		//Check if the model is using MOD_DEFAULT, has physics and is not a weapon or a ped or a vehicle
		if(pModel->GetAudioCollisionSettings() == g_MacsDefault && pModel->GetPhysics() && pModel->GetModelType() != MI_TYPE_WEAPON && pModel->GetModelType() != MI_TYPE_PED && pModel->GetModelType() != MI_TYPE_VEHICLE)
		{
			if(batchModelCreateStartIndex <= currentIndex )
			{
				const fragType* pFragType = pModel->GetFragType();
				u32 numChildren = 0;
				if(pFragType)
				{
					numChildren = pFragType->GetPhysics(0)->GetNumChildren(); 
				}
				PopulateModelSettingsCreator(pModel, audioSettingsName, numChildren);
				CreateModelAudioSettings();
			}
			currentIndex++;
			if(batchModelCreateStopIndex < currentIndex)
			{  
				break;
			}
		} 
	}

	FindNewModels();

}

void audCollisionAudioEntity::AbortWorldCollisionListGeneration()
{
	sm_ProcessSectors = false; 
	//naEnvironment::sm_DrawAudioWorldSectors = false; 
	sm_IsWaitingToInit = true;
	CWarpManager::WaitUntilTheSceneIsLoaded(false);
	CWarpManager::FadeCamera(true);
	fiStream *rescueFile = fiStream::Open("C:/AudioCollisionListRescue",false);
	if(rescueFile)
	{
		sm_CurrentSectorX = 0;
		sm_CurrentSectorY = 0;
		rescueFile->WriteInt(&sm_CurrentSectorX,1);
		rescueFile->WriteInt(&sm_CurrentSectorY,1);
		rescueFile->Close();
	}
	if(sm_NewMoveableFile)
	{
		sm_NewMoveableFile->Close();
		sm_NewMoveableFile = NULL;
	}
	if(sm_NewStaticFile)
	{
		sm_NewStaticFile->Close();
		sm_NewStaticFile = NULL;
	}
	if(sm_DefaultMatFile)
	{
		sm_DefaultMatFile->Close();
		sm_DefaultMatFile = NULL;
	}
	if(sm_ChangedMacsFile)
	{
		sm_ChangedMacsFile->Close();
		sm_ChangedMacsFile = NULL;
	}
}

void audCollisionAudioEntity::GenerateWorldCollisionLists()
{
	if(sm_ProcessSectors && sm_IsWaitingToInit)
	{
		InitWorldCollisionListGenerator();
	}
	else if(sm_ProcessSectors)
	{
		if(CWarpManager::IsActive())
		{
			// Waiting for the scene to be loaded.
			return;
		}
		else
		{
			WriteCollisionListData();
			sm_CurrentSectorX ++;
			if(sm_CurrentSectorX == g_audNumWorldSectorsX*3 )
			{
				sm_CurrentSectorX = 0;
				sm_CurrentSectorY ++;
			}
			if(sm_CurrentSectorY == g_audNumWorldSectorsY*3 )
			{
				naDisplayf("All sectors have been processed, terminating...");
				fiStream *rescueFile = fiStream::Open("C:/AudioCollisionListRescue");
				if(rescueFile)
				{
					sm_CurrentSectorX = 0;
					sm_CurrentSectorY = 0;
					rescueFile->WriteInt(&sm_CurrentSectorX,1);
					rescueFile->WriteInt(&sm_CurrentSectorY,1);
					rescueFile->Close();
				}
				sm_ProcessSectors =  false;
				CWarpManager::WaitUntilTheSceneIsLoaded(false);
				CWarpManager::FadeCamera(true);
				sm_IsWaitingToInit = true;
				if(sm_NewMoveableFile)
				{
					sm_NewMoveableFile->Close();
					sm_NewMoveableFile = NULL;
				}
				if(sm_NewStaticFile)
				{
					sm_NewStaticFile->Close();
					sm_NewStaticFile = NULL;
				}
				if(sm_DefaultMatFile)
				{
					sm_DefaultMatFile->Close();
					sm_DefaultMatFile = NULL;
				}
				if(sm_ChangedMacsFile)
				{
					sm_ChangedMacsFile->Close();
					sm_ChangedMacsFile = NULL;
				}
				naDisplayf("Done");
				return;
			}
		}
		Vector3 centreFirstSector; 
		centreFirstSector.SetX(g_audWorldSectorsMinX + 0.5f * g_audWidthOfSector/3);
		centreFirstSector.SetY(g_audWorldSectorsMinY + 0.5f * g_audDepthOfSector/3);
		naDisplayf("processing sector %d", (sm_CurrentSectorY * g_audNumWorldSectorsX*3) + sm_CurrentSectorX);
		Vector3 sectorCentre = centreFirstSector;
		sectorCentre.SetX(sectorCentre.GetX() + sm_CurrentSectorX*g_audWidthOfSector/3);
		sectorCentre.SetY(sectorCentre.GetY() + sm_CurrentSectorY*g_audDepthOfSector/3);
		naDisplayf("Wrapping player to the center sector");
		naDisplayf("Loading scene....");
		CWarpManager::WaitUntilTheSceneIsLoaded(true);
		CWarpManager::FadeCamera(false);
		CWarpManager::SetWarp(sectorCentre, sectorCentre, 0.f, true, true, 600.f);
		
	}
}

void audCollisionAudioEntity::InitWorldCollisionListGenerator()
{
	sm_IsWaitingToInit = false;
	naDisplayf("Initializing process"); 
	//naEnvironment::sm_DrawAudioWorldSectors = true; 
	naDisplayf("Making player invincible");
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Couldn't find local player, aborting process"))
	{
		pPlayer->m_nPhysicalFlags.bNotDamagedByAnything = true;
	}
	else
	{
		AbortWorldCollisionListGeneration();
	}
	naDisplayf("Checking if we have to resume the process or start a new one.");
	sm_CurrentSectorX = 0; 
	sm_CurrentSectorY = 0;

	bool abort = false;

	if(sm_NewMoveableFile)
	{
		sm_NewMoveableFile->Close();
		sm_NewMoveableFile = NULL;
	}
	if(sm_NewStaticFile)
	{
		sm_NewStaticFile->Close();
		sm_NewStaticFile = NULL;
	}
	if(sm_DefaultMatFile)
	{
		sm_DefaultMatFile->Close();
		sm_DefaultMatFile = NULL;
	}
	if(sm_ChangedMacsFile)
	{
		sm_ChangedMacsFile->Close();
		sm_ChangedMacsFile = NULL;
	}

	 sm_NewMoveableFile = fiStream::Open("C:/AudioCollisionNewMoveable.txt",false);
	if(!sm_NewMoveableFile)
	{
		sm_NewMoveableFile = fiStream::Create("C:/AudioCollisionNewMoveable.txt");

		if(!naVerifyf(sm_NewMoveableFile,"Fail opening/creating the  movable file, aborting the process."))
		{
			AbortWorldCollisionListGeneration();
			abort = true;
		}
	}
	sm_NewStaticFile = fiStream::Open("C:/AudioCollisionNewStatic.txt",false);
	if(!sm_NewStaticFile)
	{
		sm_NewStaticFile = fiStream::Create("C:/AudioCollisionNewStatic.txt");

		if(!naVerifyf(sm_NewStaticFile,"Fail opening/creating the  static file, aborting the process."))
		{
			AbortWorldCollisionListGeneration();
			abort = true;
		}
	}
	sm_DefaultMatFile = fiStream::Open("C:/AudioCollisionDefault.txt",false);
	if(!sm_DefaultMatFile)
	{
		sm_DefaultMatFile = fiStream::Create("C:/AudioCollisionDefault.txt");

		if(!naVerifyf(sm_DefaultMatFile,"Fail opening/creating the default file, aborting the process."))
		{
			AbortWorldCollisionListGeneration();
			abort = true;
		}
	}
	sm_ChangedMacsFile = fiStream::Open("C:/AudioCollisionChanged.txt",false);
	if(!sm_ChangedMacsFile)
	{
		sm_ChangedMacsFile = fiStream::Create("C:/AudioCollisionChanged.txt");

		if(!naVerifyf(sm_ChangedMacsFile,"Fail opening/creating the default file, aborting the process."))
		{
			AbortWorldCollisionListGeneration();
			abort = true;
		}
	}
	

	if(!abort)
	{
		//Check if we are resuming a process.
		fiStream *rescueFile = fiStream::Open("C:/AudioCollisionListRescue");
		if(rescueFile)
		{
			rescueFile->ReadInt(&sm_CurrentSectorX,1);
			rescueFile->ReadInt(&sm_CurrentSectorY,1);
			rescueFile->Close();
		}		
	}
	else
	{
		if(sm_NewMoveableFile)
		{
			sm_NewMoveableFile->Close();
		}
		if(sm_NewStaticFile)
		{
			sm_NewStaticFile->Close();
		}
		if(sm_DefaultMatFile)
		{
			sm_DefaultMatFile->Close();
		}
		if(sm_ChangedMacsFile)
		{
			sm_ChangedMacsFile->Close();
		}
	}
}

void audCollisionAudioEntity::WriteCollisionListData()
{
	// The players has been warpped and the scene is being completely loaded, write the file
	naDisplayf("Scene loaded : Writing rescue file");
	fiStream *rescueFile = fiStream::Open("C:/AudioCollisionListRescue",false);
	if(!rescueFile)
	{
		rescueFile = fiStream::Create("C:/AudioCollisionListRescue");
	}
	if(naVerifyf(rescueFile,"Fail opening/creating the file, aborting the process."))
	{
		rescueFile->WriteInt(&sm_CurrentSectorX,1);
		rescueFile->WriteInt(&sm_CurrentSectorY,1);
		rescueFile->Close();
	}
	else
	{
		AbortWorldCollisionListGeneration();
	}
	
	naDisplayf("Scene loaded : Writing data files");
	if(naVerifyf(sm_NewMoveableFile,"Fail opening/creating the file, aborting the process."))
	{
		FindNewModels(sm_NewMoveableFile, true);
	}
	else
	{
		AbortWorldCollisionListGeneration();
	}
	if(naVerifyf(sm_NewStaticFile,"Fail opening/creating the file, aborting the process."))
	{
		FindNewModels(sm_NewStaticFile, false);
	}
	else
	{
		AbortWorldCollisionListGeneration();
	}
	if(naVerifyf(sm_DefaultMatFile,"Fail opening/creating the file, aborting the process."))
	{
		FindDefaultMaterialModels(sm_DefaultMatFile);
	}
	else
	{
		AbortWorldCollisionListGeneration();
	}
	if(naVerifyf(sm_ChangedMacsFile,"Fail opening/creating the file, aborting the process."))
	{
		FindChangedModels(sm_ChangedMacsFile);
	}
	else
	{
		AbortWorldCollisionListGeneration();
	}
}

void audCollisionAudioEntity::FindNewModels(fiStream *stream, bool findMovable)
{
	const u32 maxAudioSettingsNameLength		= 128;
	const char *audioSettingsNamePrefix	= "MOD_";
	char audioSettingsName[maxAudioSettingsNameLength];

	s32 spawnCount = 0;

	//Iterate through all loaded models and fix any that have gotten out of synch
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for(u32 i=0; i< maxModelInfos; i++)
	{
		CBaseModelInfo * pModel = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));	
		if(!pModel || !pModel->GetIsTypeObject() || !(pModel->GetPhysics() ||(pModel->GetFragType() && pModel->GetFragType()->GetPhysics(0))))
		{
			continue; 
		}

		if((findMovable && pModel->GetIsFixed()) || (!findMovable && !pModel->GetIsFixed()) || pModel->GetAudioCollisionSettings() != g_MacsDefault)
		{
			continue;
		}

		fwModelId modelId((strLocalIndex(i)));

		//Try get a Macs for the current model
		strcpy(audioSettingsName, audioSettingsNamePrefix);
		strcat(audioSettingsName, CModelInfo::GetBaseModelInfoName(modelId));

		fprintf(stream, "%s\r\n", CModelInfo::GetBaseModelInfoName(modelId));

		++spawnCount;
	}
}

void audCollisionAudioEntity::FindLocalDefaultMaterialModels()
{
	fiStream * defaultMatFile = fiStream::Open("C:/AudioCollisionLocalDefault.txt",false);
	if(!defaultMatFile)
	{
		defaultMatFile = fiStream::Create("C:/AudioCollisionLocalDefault.txt");

		if(!naVerifyf(defaultMatFile,"Fail opening/creating the default file, aborting the process."))
		{
			return;
		}
	}

	FindDefaultMaterialModels(defaultMatFile);

	defaultMatFile->Close();
}

void audCollisionAudioEntity::FindDefaultMaterialModels(fiStream *stream)
{
	const u32 maxAudioSettingsNameLength		= 128;
	const char *audioSettingsNamePrefix	= "MOD_";
	char audioSettingsName[maxAudioSettingsNameLength];

	s32 spawnCount = 0;

	//Iterate through all loaded models and fix any that have gotten out of synch
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for(u32 i=0; i< maxModelInfos; i++)
	{
		CBaseModelInfo * pModel = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));	
		if(!pModel || !pModel->GetIsTypeObject() || !(pModel->GetPhysics() ||(pModel->GetFragType() && pModel->GetFragType()->GetPhysics(0))))
		{
			continue; 
		}

		const fragType* pFragType = pModel->GetFragType();
		u32 numChildren = 0;
		if(pFragType)
		{
			numChildren = pFragType->GetPhysics(0)->GetNumChildren();
		}

		PopulateModelSettingsCreator(pModel, audioSettingsName, numChildren);

		bool foundDefaultMaterial = false;
		for(int j=0; j<s_ModelSettingsCreator.numMaterials; j++)
		{
			if(strstr(s_ModelSettingsCreator.materialNames[j], "default"))
			{
				foundDefaultMaterial = true;
				break;
			}
		}
		if(!foundDefaultMaterial)
		{
			continue;
		}

		fwModelId modelId((strLocalIndex(i)));

		//Try get a Macs for the current model
		strcpy(audioSettingsName, audioSettingsNamePrefix);
		strcat(audioSettingsName, CModelInfo::GetBaseModelInfoName(modelId));

		fprintf(stream, "%s\r\n", CModelInfo::GetBaseModelInfoName(modelId));

		++spawnCount;
	}
}


void audCollisionAudioEntity::FindChangedModels(fiStream *stream)
{
	const u32 maxAudioSettingsNameLength		= 128;
	const char *audioSettingsNamePrefix	= "MOD_";
	char audioSettingsName[maxAudioSettingsNameLength];

	s32 spawnCount = 0;

	//Iterate through all loaded models and fix any that have gotten out of synch
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for(u32 i=0; i< maxModelInfos; i++)
	{
		CBaseModelInfo * pModel = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));	
		if(!pModel || !pModel->GetIsTypeObject() || !(pModel->GetPhysics() ||(pModel->GetFragType() && pModel->GetFragType()->GetPhysics(0))))
		{
			continue; 
		}

		const fragType* pFragType = pModel->GetFragType();
		u32 numChildren = 0;
		if(pFragType)
		{
			numChildren = pFragType->GetPhysics(0)->GetNumChildren();
		}

		PopulateModelSettingsCreator(pModel, audioSettingsName, numChildren);

		if(pModel->GetAudioCollisionSettings() == g_MacsDefault || !AreMacsMaterialsOutOfDate(pModel->GetAudioCollisionSettings()))
		{
			continue;
		}

		fwModelId modelId((strLocalIndex(i)));

		//Try get a Macs for the current model
		strcpy(audioSettingsName, audioSettingsNamePrefix);
		strcat(audioSettingsName, CModelInfo::GetBaseModelInfoName(modelId));

		fprintf(stream, "%s\r\n", CModelInfo::GetBaseModelInfoName(modelId));

		++spawnCount;
	}
}

//Looks for models that are still using MOD_DEFAULT and provides a way to spawn them so that they can be properly set up
void audCollisionAudioEntity::FindNewModels()
{
	const u32 maxAudioSettingsNameLength		= 128;
	const char *audioSettingsNamePrefix	= "MOD_";
	char audioSettingsName[maxAudioSettingsNameLength];

	s32 spawnCount = 0;

	fiStream *stream = fiStream::Create("MACS_NewModelsList.txt");

	if(!stream)
	{
		naErrorf("Couldn't open Desktop/MACS_NewModelsList.txt"); 
		return;
	}

	naDisplayf("Writing sync list to: Desktop/MACS_NewModelsList.txt");


	//Iterate through all loaded models and fix any that have gotten out of synch
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for(u32 i=0; i< maxModelInfos; i++)
	{
		CBaseModelInfo * pModel = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));	
		if(!pModel)
		{
			continue; 
		}

		fwModelId modelId((strLocalIndex(i)));

		//Try get a Macs for the current model
		strcpy(audioSettingsName, audioSettingsNamePrefix);
		strcat(audioSettingsName, CModelInfo::GetBaseModelInfoName(modelId));
 
		//Check if the model is using MOD_DEFAULT, has physics and is not a weapon or a ped or a vehicle
		if(pModel->GetAudioCollisionSettings() == g_MacsDefault && (pModel->GetFragType() || pModel->GetFragType()->GetPhysics(0)) && pModel->GetIsTypeObject()) 
		{
			const fragType* pFragType = pModel->GetFragType();
			u32 numChildren = 0;
			if(pFragType)
			{
				numChildren = pFragType->GetPhysics(0)->GetNumChildren(); 
			}

			if(numChildren)
			{
				fprintf(stream, "%i: MACS Frag materials absent: %s\r\n", spawnCount, CModelInfo::GetBaseModelInfoName(modelId));
			}
			else
			{
				fprintf(stream, "%i: MACS materials absent: %s\r\n", spawnCount, CModelInfo::GetBaseModelInfoName(modelId));
			}

			++spawnCount;
		} 
	}

	stream->Close();
}

void audCollisionAudioEntity::SpawnSyncModelObject()
{
		const u32 maxAudioSettingsNameLength		= 128;
	const char *audioSettingsNamePrefix	= "MOD_";
	char audioSettingsName[maxAudioSettingsNameLength];

	s32 spawnCount = 0;
	bool spawnModel = true;

	fiStream *stream = fiStream::Create("MACS_SynchronizeList.txt");

	if(!stream)
	{
		naErrorf("Couldn't open MACS_SynchronizeList.txt");
		return;
	}

	//Iterate through all loaded models and fix any that have gotten out of synch
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for(u32 i=0; i< maxModelInfos; i++)
	{
		CBaseModelInfo * pModel = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));	
		if(!pModel)
		{
			continue; 
		}

		fwModelId modelId((strLocalIndex(i)));

		//Try get a Macs for the current model
		strcpy(audioSettingsName, audioSettingsNamePrefix);
		strcat(audioSettingsName, CModelInfo::GetBaseModelInfoName(modelId));
 
		//Check if the model has collision settings, has physics and is not a weapon or a ped or a vehicle
		if(pModel->GetAudioCollisionSettings() && AreMacsMaterialsOutOfDate(pModel->GetAudioCollisionSettings()) && pModel->GetPhysics() && pModel->GetModelType() != MI_TYPE_WEAPON && pModel->GetModelType() != MI_TYPE_PED && pModel->GetModelType() != MI_TYPE_VEHICLE)
		{
//			Displayf("Model type is %d", pModel->GetModelType());

			if(spawnCount == syncModelSpawnIndex && spawnModel)
			{
				//Spawn in an instance of the object so we can set up the MACS using the model editor
				spawnModel = false;
				RegdEnt displayObject(NULL);
				Matrix34 testMat;
				testMat.Identity();

				Vector3 vecCreateOffset = camInterface::GetFront();
				vecCreateOffset.z = 0.0f;
				vecCreateOffset *= 2.f + pModel->GetBoundingSphereRadius();

				camDebugDirector & debugDirector = camInterface::GetDebugDirector();
				testMat.d = debugDirector.IsFreeCamActive() ? camInterface::GetPos() : CGameWorld::FindLocalPlayerCoors();
				testMat.d += vecCreateOffset;

				if(debugDirector.IsFreeCamActive())
				{
					testMat.Set3x3(camInterface::GetMat());
				}
				else if(CGameWorld::FindLocalPlayer())
				{
					testMat.Set3x3(MAT34V_TO_MATRIX34(CGameWorld::FindLocalPlayer()->GetMatrix()));
				}

				//if(pModel->GetIsTypeObject())
				{

					bool bForceLoad = false;
					if(!CModelInfo::HaveAssetsLoaded(modelId))
					{
						CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
						bForceLoad = true;
					}

					if(bForceLoad)
					{
						CStreaming::LoadAllRequestedObjects(true);
					}
				}

				if(pModel->GetIsTypeObject())
				{
					fwModelId modelId((strLocalIndex(i)));
					displayObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_RANDOM, true);
				} 
				else
				{
					displayObject = rage_new CBuilding( ENTITY_OWNEDBY_AUDIO );
					displayObject->SetModelId(fwModelId(strLocalIndex(i)));
				}

				if(displayObject)
				{
					displayObject->SetMatrix(testMat);
					CGameWorld::Add(displayObject, CGameWorld::OUTSIDE );

					syncModelSpawnIndex++;

					Displayf("[NorthAudio] MACS editor: Spawned %s (index %d)", CModelInfo::GetBaseModelInfoName(modelId), i);

					if(displayObject->GetIsTypeObject())
					{
						(static_cast<CObject*>(displayObject.Get()))->PlaceOnGroundProperly();
					}
				}
			}

			++spawnCount;
		} 
	}

	syncModelSpawnIndex = syncModelSpawnIndex < spawnCount ? syncModelSpawnIndex : 0;

	stream->Close();

}

void audCollisionAudioEntity::SpawnNewModelObject()
{
		const u32 maxAudioSettingsNameLength		= 128;
	const char *audioSettingsNamePrefix	= "MOD_";
	char audioSettingsName[maxAudioSettingsNameLength];

	s32 spawnCount = 0;
	bool spawnModel = true;

	fiStream *stream = fiStream::Create("MACS_NewModelsList.txt");

	if(!stream)
	{
		naErrorf("Couldn't open MACS_NewModelsList.txt");
		return;
	}

	//Iterate through all loaded models and fix any that have gotten out of synch
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for(u32 i=0; i< maxModelInfos; i++)
	{
		CBaseModelInfo * pModel = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));	
		if(!pModel)
		{
			continue; 
		}

		fwModelId modelId((strLocalIndex(i)));

		//Try get a Macs for the current model
		strcpy(audioSettingsName, audioSettingsNamePrefix);
		strcat(audioSettingsName, CModelInfo::GetBaseModelInfoName(modelId));
 
		//Check if the model is using MOD_DEFAULT, has physics and is not a weapon or a ped or a vehicle
		if(pModel->GetAudioCollisionSettings() == g_MacsDefault && pModel->GetPhysics() && pModel->GetModelType() != MI_TYPE_WEAPON && pModel->GetModelType() != MI_TYPE_PED && pModel->GetModelType() != MI_TYPE_VEHICLE)
		{
//			Displayf("Model type is %d", pModel->GetModelType());

			if(spawnCount == newModelSpawnIndex && spawnModel)
			{
				//Spawn in an instance of the object so we can set up the MACS using the model editor
				spawnModel = false;
				RegdEnt displayObject(NULL);
				Matrix34 testMat;
				testMat.Identity();

				Vector3 vecCreateOffset = camInterface::GetFront();
				vecCreateOffset.z = 0.0f;
				vecCreateOffset *= 2.f + pModel->GetBoundingSphereRadius();

				camDebugDirector & debugDirector = camInterface::GetDebugDirector();
				testMat.d = debugDirector.IsFreeCamActive() ? camInterface::GetPos() : CGameWorld::FindLocalPlayerCoors();
				testMat.d += vecCreateOffset;

				if(debugDirector.IsFreeCamActive())
				{
					testMat.Set3x3(camInterface::GetMat());
				}
				else if(CGameWorld::FindLocalPlayer())
				{
					testMat.Set3x3(MAT34V_TO_MATRIX34(CGameWorld::FindLocalPlayer()->GetMatrix()));
				}

				//if(pModel->GetIsTypeObject())
				{

					bool bForceLoad = false;
					if(!CModelInfo::HaveAssetsLoaded(modelId))
					{
						CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
						bForceLoad = true;
					}

					if(bForceLoad)
					{
						CStreaming::LoadAllRequestedObjects(true);
					}
				}

				if(pModel->GetIsTypeObject())
				{
					fwModelId modelId((strLocalIndex(i)));
					displayObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_RANDOM, true);
				} 
				else
				{
					displayObject = rage_new CBuilding( ENTITY_OWNEDBY_AUDIO );
					displayObject->SetModelId(fwModelId(strLocalIndex(i)));
				}

				if(displayObject)
				{
					displayObject->SetMatrix(testMat);
					CGameWorld::Add(displayObject, CGameWorld::OUTSIDE );

					newModelSpawnIndex++;

					Displayf("[NorthAudio] MACS editor: Spawned %s (index %d)", CModelInfo::GetBaseModelInfoName(modelId), i);

					if(displayObject->GetIsTypeObject())
					{
						(static_cast<CObject*>(displayObject.Get()))->PlaceOnGroundProperly();
					}
				}
			}

			++spawnCount;
		} 
	}

	newModelSpawnIndex = newModelSpawnIndex < spawnCount ? newModelSpawnIndex : 0;

	stream->Close();

}

bool audCollisionAudioEntity::AreMacsMaterialsOutOfDate(const ModelAudioCollisionSettings * macs)
{
	//Check we've got the right number of materials
	if(macs->numMaterialOverrides > 1 && macs->numMaterialOverrides != s_ModelSettingsCreator.numMaterials)
	{
		return true;
	}

	if(macs->numMaterialOverrides > 1)
	{
		//Check none of the materials have changed
		for(s32 i=0; i < macs->numMaterialOverrides; i++)
		{
			bool foundMaterial = false;
			for(int j=0; j < s_ModelSettingsCreator.numMaterials; j++)
			{
				if(audNorthAudioEngine::GetObject<CollisionMaterialSettings>(macs->MaterialOverride[i].Material) == 
					audNorthAudioEngine::GetObject<CollisionMaterialSettings>(s_ModelSettingsCreator.materialNames[j]) )
				{
					foundMaterial = true;
				}	
			}
			if(!foundMaterial)
			{
				return true;
			}
		}
	}

	int numFrags = GetNumFragComponentsFromModelSettings(macs);

	for(int i=0; i< numFrags; i++)
	{
		const ModelAudioCollisionSettings* fragMacs = GetFragComponentMaterialSettings(macs, i, true);
		if(fragMacs && AreMacsMaterialsOutOfDate(fragMacs))
		{
			return true;
		}
	}

	return false;
}

void audCollisionAudioEntity::OpenCurrentModelInRave()
{
	atString upSettingName(s_ModelSettingsCreator.audioSettingsName);
	upSettingName.Uppercase();

	if((strlen(s_ModelSettingsCreator.audioSettingsName) > 0) && audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(s_ModelSettingsCreator.audioSettingsName))
	{
		char xmlMsg[512] = {"<RAVEMessage>\n"};
		char tmpBuf[256] = {0};
		sprintf(tmpBuf, "	<ViewObject name=\"%s\" metadataType=\"GameObjects\" chunkNameHash=\"%u\"/>\n", &upSettingName[0], ATSTRINGHASH("BASE", 0x44E21C90));
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "</RAVEMessage>\n");
		strcat(xmlMsg, tmpBuf);

		naDisplayf("XML message is %d characters long \n", istrlen(xmlMsg));
		naDisplayf("%s", xmlMsg);

		audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
		bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
		naAssertf(success, "Failed to send xml message to rave");
		naDisplayf("%s", (success)? "Success":"Failed");
	}
	else
	{
		naDisplayf("===========================");
		naDisplayf("Model doesn't have a ModelAudioCollisionSettings game object yet");
		naDisplayf("===========================");
	}
}


# endif //__BANK

void audCollisionAudioEntity::ReportUproot(CEntity *entity)
{
#if __BANK
	if(g_DisableCollisionAudio)
	{
		return;
	}
#endif
	if(m_CachedUprootEvents.IsFull())
	{
		naErrorf("Cached uproot event array is full");
		return;
	}

	if(entity && entity->GetBaseModelInfo())
	{
		const ModelAudioCollisionSettings *settings = entity->GetBaseModelInfo()->GetAudioCollisionSettings();
				
		if(settings && settings->UprootSound && settings->UprootSound != g_NullSoundHash)
		{
			PF_INCREMENT(audCollisionAudioEntity_Breaks);		

			audUprootEvent & uproot = m_CachedUprootEvents.Append();
			audSoundInitParams initParams;
			// Need to work out now what occlusion group to use
			uproot.entity =  entity;
			uproot.pos = entity->GetTransform().GetPosition();
			uproot.uprootSound = settings->UprootSound;
		}
	}
}

void audCollisionAudioEntity::TriggerDeferredUproots()
{
	for(int i=0; i<m_CachedUprootEvents.GetCount(); i++)
	{
		audSoundInitParams initParams;
		// Need to work out now what occlusion group to use
		if(m_CachedUprootEvents[i].entity)
		{
			initParams.EnvironmentGroup = GetOcclusionGroup(m_CachedUprootEvents[i].pos, m_CachedUprootEvents[i].entity);
		}
		initParams.Position = VEC3V_TO_VECTOR3(m_CachedUprootEvents[i].pos);
		if(g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
		{
			CreateAndPlaySound(m_CachedUprootEvents[i].uprootSound, &initParams);
		}
	}

	m_CachedUprootEvents.Reset();
}

void audCollisionAudioEntity::ReportFragmentBreak(const phInst *instance, u32 componentId, bool isDestroyed)
{
#if __BANK
	if(g_DisableCollisionAudio)
	{
		return;
	}
#endif

	if(instance == NULL)
	{
		return;
	}

	if(m_CachedFragmentBreaks.IsFull())
	{
		naErrorf("Cached fragment break array is full");
		return;
	}

	CollisionMaterialSettings *materialSettings = NULL;
	CEntity *entity = CPhysics::GetEntityFromInst((phInst *)instance); 
	if(entity && entity->GetBaseModelInfo())
	{
		//<const_cast> to let us poke the last frag time
		ModelAudioCollisionSettings *settings = const_cast<ModelAudioCollisionSettings*>(entity->GetBaseModelInfo()->GetAudioCollisionSettings());
		if(!settings)
		{
			Errorf("Base model %s doesn't have a collision settings", entity->GetModelName());
			return;
		}

		if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - settings->LastFragTime < 1000)
		{
			return;
		}

		settings->LastFragTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

		audSoundInitParams initParams;

		u32 breakSound;

		if(GetNumFragComponentsFromModelSettings(settings))
		{
			const ModelAudioCollisionSettings * fragMod = GetFragComponentMaterialSettings(settings, componentId);

			//naErrorf("Frag settings %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(fragMod->NameTableOffset)); 
		
			//First try to get a break sound from the frag component Macs
			if(fragMod)
			{
				breakSound = isDestroyed && fragMod->DestroySound != g_NullSoundHash ? fragMod->DestroySound : fragMod->BreakSound;

				if(fragMod->BreakSound && fragMod->BreakSound != g_NullSoundHash)
				{
					audFragmentBreakEvent & fragBreak = m_CachedFragmentBreaks.Append();
					fragBreak.entity = entity;
					fragBreak.pos = entity->GetTransform().GetPosition();
					fragBreak.breakSound = breakSound;
					return;
				} 
			}
		}

		breakSound = isDestroyed && settings->DestroySound != g_NullSoundHash ? settings->DestroySound : settings->BreakSound;
		
		//If that fails look to the parent Macs
		if(breakSound && breakSound != g_NullSoundHash)
		{
			audFragmentBreakEvent & fragBreak = m_CachedFragmentBreaks.Append();
			fragBreak.entity = entity;
			fragBreak.pos = entity->GetTransform().GetPosition();
			fragBreak.breakSound = breakSound;
			return;
		}

		s32 matid = 0;
		CBaseModelInfo * modelInfo = entity->GetBaseModelInfo();

		if (modelInfo->GetFragType() && modelInfo->GetFragType()->GetPhysics(0))
		{
			//Finally try get a material settings directly from the bound
			phArchetype *phtype = (phArchetype *) modelInfo->GetFragType()->GetPhysics(0)->GetArchetype();
			if(phtype) 
			{
				naAssert(phtype->GetBound() && phtype->GetBound()->GetType()==phBound::COMPOSITE); //Just making sure we're actually looking at a composite bound
				phBoundComposite * composite = (phBoundComposite *)phtype->GetBound();
				if(composite)
				{
					//Get the bound/fragment that has broken off
					phBound * bound = composite->GetBound(componentId);
					if(bound) //Since we got the composite bound from the model, not the entity, it should be the full unbroken bound...but we'll check anyhow
					{
						//since there is no specifies break sound for the component, we'll take a guess at the material to use
						matid = static_cast<s32>(PGTAMATERIALMGR->UnpackMtlId(bound->GetMaterialId(0)));
					}
				}
			}
		}

		materialSettings = GetMaterialOverride(entity, g_audCollisionMaterials[matid], componentId);

		if(materialSettings != NULL)
		{
			breakSound = isDestroyed && materialSettings->DestroySound != g_NullSoundHash ? materialSettings->DestroySound : materialSettings->BreakSound;
			if(breakSound && breakSound != g_NullSoundHash)
			{
				audFragmentBreakEvent & fragBreak = m_CachedFragmentBreaks.Append();
				fragBreak.entity = entity;
				fragBreak.pos = entity->GetTransform().GetPosition();
				fragBreak.breakSound = breakSound;
			}
		}
	}
}

void audCollisionAudioEntity::TriggerDeferredFragmentBreaks()
{
	for(int i=0; i<m_CachedFragmentBreaks.GetCount(); i++)
	{
		audSoundInitParams initParams;
		if(m_CachedFragmentBreaks[i].entity)
		{
			initParams.EnvironmentGroup = GetOcclusionGroup(m_CachedFragmentBreaks[i].pos, m_CachedFragmentBreaks[i].entity);
		}
		initParams.Position = VEC3V_TO_VECTOR3(m_CachedFragmentBreaks[i].pos);
		if(g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
		{
			CreateAndPlaySound(m_CachedFragmentBreaks[i].breakSound, &initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_CachedFragmentBreaks[i].breakSound, &initParams, NULL, NULL, eCollisionAudioEntity));
		}
	}

	m_CachedFragmentBreaks.Reset();
}

BANK_ONLY(bool g_PedsHitLikeGirls = false);

void audCollisionAudioEntity::ReportMeleeCollision(const WorldProbe::CShapeTestHitPoint* pResult, f32 damageDone, CEntity *hittingEntity, const bool isBullet, const bool isFootstep, const bool isPunch, const bool isKick)
{
#if __BANK
	if(g_DisableCollisionAudio)
	{
		return;
	}
#endif

	bool isPlayerHit = false;
	if(hittingEntity && hittingEntity->GetIsTypePed() && ((CPed*)hittingEntity)->IsLocalPlayer())
	{
		isPlayerHit = true;
	}

	TriggerDeferredMeleeImpact(pResult, damageDone, 1.f, isBullet, isPlayerHit, isFootstep, hittingEntity, isPunch, isKick);

}


void audCollisionAudioEntity::ReportHeliBladeCollision(const WorldProbe::CShapeTestHitPoint* pResult, CVehicle* vehicle)
{
#if __BANK
	if(g_DisableCollisionAudio)
	{
		return;
	}
#endif
	if(!pResult)
		return;

	//Extract the entity involved in this collision.
	CEntity *entity = NULL;
	const phInst *instance = pResult->GetHitInst();
	if(instance)
	{
		entity = CPhysics::GetEntityFromInst((phInst *)instance);
		if(entity == NULL)
		{
			return;
		}
	}

	if(m_CachedMeleeImpacts.IsFull())
	{
		naErrorf("Cached melee impacts array is full");
		return;
	}

	audCollisionEvent *collisionEvent = &m_CachedMeleeImpacts.Append();
	if(collisionEvent == NULL)
	{
		//		Assertf(0, "The audio collision event history is full!");
		return;
	}

	collisionEvent->impulseMagnitudes[0] = 3.f;

	collisionEvent->type = AUD_COLLISION_TYPE_IMPACT;

	PopulateAudioCollisionEventFromIntersection(collisionEvent, pResult);

	if(vehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
	{
		static_cast<audHeliAudioEntity*>(vehicle->GetVehicleAudioEntity())->TriggerRotorImpactSounds();
	}
}

void audCollisionAudioEntity::PopulateAudioCollisionEventFromIntersection(audCollisionEvent *collisionEvent, const WorldProbe::CShapeTestHitPoint* pResult)
{
	phMaterialMgr::Id materialId = pResult->GetHitMaterialId();
	collisionEvent->positions[0] = pResult->GetHitPosition();
	u16 componentId = pResult->GetHitComponent();
	const phInst *instance = pResult->GetHitInst();
	if(instance)
	{
		collisionEvent->entities[0] = CPhysics::GetEntityFromInst((phInst *)instance);
	}
	else
	{
		collisionEvent->entities[0] = NULL;
	}

	collisionEvent->components[0] = componentId;
	collisionEvent->components[1] = 0;

	collisionEvent->materialSettings[0] = NULL;	
	collisionEvent->materialSettings[1] = NULL;

	collisionEvent->materialIds[0] = materialId;
	if(collisionEvent->entities[0] && collisionEvent->entities[0]->GetIsTypeVehicle()) 
	{
		CVehicle * vehicle = (CVehicle*)collisionEvent->entities[0].Get();
		vehicle->GetVehicleAudioEntity()->GetCollisionAudio().HandleMelee();

		VehicleCollisionSettings * vehicleCollisionSettings = vehicle->GetVehicleAudioEntity()->GetVehicleCollisionSettings();
		if(vehicleCollisionSettings)
		{
			CollisionMaterialSettings * settings =  collisionEvent->materialSettings[0], *overrideSettings = NULL;
			for(s32 i=0; i<vehicleCollisionSettings->numMeleeMaterialOverrides; i++)
			{
				if(vehicleCollisionSettings->MeleeMaterialOverride[0].Material.IsValid() && vehicleCollisionSettings->MeleeMaterialOverride[i].Override.IsValid() && settings == audNorthAudioEngine::GetObject<CollisionMaterialSettings>(vehicleCollisionSettings->MeleeMaterialOverride[i].Material))
				{
					 overrideSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(vehicleCollisionSettings->MeleeMaterialOverride[i].Override); 
				}
			}

			if(collisionEvent->GetFlag(AUD_COLLISION_FLAG_BULLET) && vehicle->GetArchetype()->GetModelNameHash() == 0x153E1B0A) // Ortega's trailer, prevents bullets from making the big collision boom
			{
				overrideSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_CAR_METAL", 0xA0231769)); 
			}
			if(overrideSettings)
			{
				collisionEvent->materialSettings[0] = overrideSettings;
			}
			else
			{
				collisionEvent->materialSettings[0] = vehicle->GetVehicleAudioEntity()->GetCollisionAudio().GetVehicleCollisionMaterialSettings();
			}
		}
	}
	
	if (!collisionEvent->materialSettings[0])
	{
		s32 mati = 0;
		if(naVerifyf(PGTAMATERIALMGR->UnpackMtlId(pResult->GetHitMaterialId())<(phMaterialMgr::Id)g_audCollisionMaterials.GetCount(), "Unpacked mtl id too large"))
		{
			if(naVerifyf(PGTAMATERIALMGR->UnpackMtlId(pResult->GetHitMaterialId())<(phMaterialMgr::Id)0xffff, "Unpacked mtl id too big for s32 cast"))
			{ 
				mati = static_cast<s32>(PGTAMATERIALMGR->UnpackMtlId(materialId));
			}
		}
#if __BANK
		if(g_PrintMeleeImpactMaterial)
		{
			char matname[128] = {0};
			PGTAMATERIALMGR->GetMaterialName(pResult->GetMaterialId(), matname, 128);
			audDisplayf("Melee phys material: %s", matname);
		}
#endif
		collisionEvent->materialSettings[0] = GetMaterialOverride(collisionEvent->entities[0],g_audCollisionMaterials[mati], collisionEvent->components[0]);

#if __BANK
		if(g_PrintMeleeImpactMaterial && collisionEvent->materialSettings[0])
		{
			audDisplayf("Melee material collision settings: %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(collisionEvent->materialSettings[0]->NameTableOffset));
		}
#endif
	}

	CPed *ped = NULL;

	if(collisionEvent->entities[0]->GetIsTypePed())
	{
		ped = ((CPed *)(collisionEvent->entities[0].Get()));
	}

	if(ped && ped->GetPedType() == PEDTYPE_ANIMAL)
	{
		collisionEvent->impulseMagnitudes[0] *= g_AnimalImpactScalar;

		if(ped->GetCapsuleInfo()->IsBird())
		{
			collisionEvent->materialSettings[0] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_BIRD", 0xBDC5E9E2));
		}
		else if(ped->GetCapsuleInfo()->IsFish())
		{
			collisionEvent->materialSettings[0] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_FISH", 0x51392531));
		}
		else if(ped->GetCapsuleInfo()->IsQuadruped())
		{
			switch(ped->GetArchetype()->GetModelNameHash())
			{
			case 0x97207223: //ATSTRINGHASH("A_C_Horse", 0x97207223)
			case 0xFCFA9E1E: //ATSTRINGHASH("A_C_Cow", 0xFCFA9E1E)
			case 0xD86B5A95: //ATSTRINGHASH("a_c_deer", 0xD86B5A95) 
				collisionEvent->materialSettings[0] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_QUADPED_LARGE", 0x628B6797));
				break;
			case 0x14EC17EA: //ATSTRINGHASH("a_c_chop", 0x14EC17EA)
			case 0xCE5FF074: //ATSTRINGHASH("a_c_boar", 0xCE5FF074)
			case 0x644AC75E: //ATSTRINGHASH("a_c_coyote", 0x644AC75E)
			case 0x1250D7BA: //ATSTRINGHASH("a_c_mtlion", 0x1250D7BA)
			case 0xB11BAB56: //ATSTRINGHASH("a_c_pig", 0xB11BAB56)
			case 0x349F33E1: //ATSTRINGHASH("a_c_retriever", 0x349F33E1)
			case 0xC2D06F53: //ATSTRINGHASH("a_c_rhesus", 0xC2D06F53)
			case 0x9563221D: //ATSTRINGHASH("a_c_rottweiler", 0x9563221D)
			case 0xA8683715: //ATSTRINGHASH("a_c_chimp", 0xA8683715)
			case 0x573201B8: //ATSTRINGHASH("a_c_cat_01", 0x573201B8)
			case 0xFC4E657D: //ATSTRINGHASH("A_C_POODLE ", 0xFC4E657D)
			case 0x6D362854: //ATSTRINGHASH("A_C_PUG", 0x6D362854)
			case 0xAD7844BB: //ATSTRINGHASH("A_C_WESTY", 0xAD7844BB)
				collisionEvent->materialSettings[0] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_QUADPED_MED", 0xEDA0B3E4));
				break;
			case 0xC3B52966: //ATSTRINGHASH("a_c_rat", 0xC3B52966)
			case 0xDFB55C81: //ATSTRINGHASH("a_c_rabbit_01", 0xDFB55C81)
				collisionEvent->materialSettings[0] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_QUADPED_SMALL", 0xB5D17E88));
				break;
			default:
				char animalMaterial[32];
				formatf(animalMaterial, "AM_ANIMAL_%08X", ped->GetArchetype()->GetModelNameHash());
				CollisionMaterialSettings * animalSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(animalMaterial);
				if(animalSettings)
				{
					collisionEvent->materialSettings[0] = animalSettings;
				}
				else
				{
					collisionEvent->materialSettings[0] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_QUADPED_MED", 0xEDA0B3E4));
				}
			}
		}
	}

	if(collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE))
	{
		CollisionMaterialSettings * settings =  audNorthAudioEngine::GetObject<CollisionMaterialSettings>(collisionEvent->materialSettings[0]->MeleeOverideMaterial);
		if(settings)
		{
			collisionEvent->materialSettings[0] = settings;
		}
	}

#if __BANK
	if(g_PrintMeleeImpactMaterial)
	{
		audDisplayf("Melee impact material: %s", collisionEvent->materialSettings[0]? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(collisionEvent->materialSettings[0]->NameTableOffset): "none");
	}
#endif

	collisionEvent->lastImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

CollisionMaterialSettings * audCollisionAudioEntity::GetMaterialFromIntersection(phIntersection *intersection)
{
	const CEntity *entity = CPhysics::GetEntityFromInst((phInst *)intersection->GetInstance());
	return GetMaterialSettingsFromMatId(entity, intersection->GetMaterialId(), intersection->GetComponent());
}

CollisionMaterialSettings * audCollisionAudioEntity::GetMaterialFromShapeTestSafe(const WorldProbe::CShapeTestHitPoint *intersection)
{
	const CEntity *entity = intersection->GetHitEntity();
	return GetMaterialSettingsFromMatId(entity, intersection->GetMaterialId(), intersection->GetComponent());
}	

bool audCollisionAudioEntity::PlayScrapeSounds(audCollisionEvent *collisionEvent)
{
	u8 numSoundsPlayed = 0;
	//	f32 scrapeSpeed = ComputeScrapeSpeed(collisionEvent);

	if(collisionEvent->scrapeMagnitudes[0] > 0.f || collisionEvent->scrapeMagnitudes[1] > 0.f)

	{
		s32 pitch;
		f32 volume;
		//f32 scaledScrapeSpeed;
		
		bool isWet = g_weather.GetWetness() > 0.f;
		if(collisionEvent->entities[0] && collisionEvent->entities[0]->GetIsTypePed() && isWet)
		{
			CPed * ped = ((CPed *)collisionEvent->entities[0].Get());
			if(ped->GetIsInInterior() || !ped->GetIsInExterior())
			{
				isWet = false;
			}
		}
		if(collisionEvent->entities[1] && collisionEvent->entities[1]->GetIsTypePed() && isWet)
		{
			CPed * ped = ((CPed *)collisionEvent->entities[1].Get());
			if(ped->GetIsInInterior() || !ped->GetIsInExterior())
			{
				isWet = false;
			}
		}

		for(u8 i=0; i<2; i++)
		{


			bool isPedCollision = collisionEvent->entities[i] && collisionEvent->entities[i]->GetIsTypePed();
			bool otherIsPedCollision = collisionEvent->entities[(i + 1) & 1] && collisionEvent->entities[(i + 1) & 1]->GetIsTypePed();
			

			CollisionMaterialSettings *materialSettings = collisionEvent->materialSettings[i];
			CollisionMaterialSettings *otherMaterialSettings = collisionEvent->materialSettings[(i + 1) & 1];

			if(isWet && isPedCollision)
			{
				materialSettings = otherMaterialSettings;
			}

			if(materialSettings == NULL || materialSettings->ScrapeSound == g_NullSoundHash || collisionEvent->scrapeSounds[i])
			{
				continue;
			}

			u32 scrapeSound = materialSettings->ScrapeSound;
			audMetadataRef wetScrapeLayer;

			if(otherMaterialSettings)
			{
				//We don't want to play a scrape if the other material is a lower level of hardness
				if(!isPedCollision && otherIsPedCollision)
				{
					if(isWet)
					{
						CollisionMaterialSettings * wetMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(materialSettings->WetMaterialReference);
						if(wetMaterial)
						{
							materialSettings = wetMaterial;
						}
					}
					if(materialSettings->PedScrapeSound != g_NullSoundHash)
					{
						scrapeSound = materialSettings->PedScrapeSound;
					}
					else if(materialSettings->HardImpact != g_NullSoundHash && 
						!(AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_LOOSESURFACE) == AUD_TRISTATE_TRUE)  
						&& !(AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_SCRAPESFORPEDS) == AUD_TRISTATE_TRUE))
					{
						continue;
					}
				}
				else if(isPedCollision && isWet)
				{
					wetScrapeLayer = ((CPed*)collisionEvent->entities[i].Get())->GetPedAudioEntity()->GetPedCollisionSoundset()->Find(ATSTRINGHASH("WetScrapeLayer", 0x901A79D0));
				}
				else 
				{
					if(otherMaterialSettings->HardImpact == g_NullSoundHash)
					{
						if( (materialSettings->HardImpact != g_NullSoundHash) || 
							(materialSettings->SolidImpact != g_NullSoundHash && otherMaterialSettings->SolidImpact == g_NullSoundHash))
						{
							continue;
						}
					}

					if((otherMaterialSettings && AUD_GET_TRISTATE_VALUE(otherMaterialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_ISSOFTFORLIGHTPROPS) == AUD_TRISTATE_TRUE && 
						GetAudioWeight(collisionEvent->entities[i]) <= AUDIO_WEIGHT_L) && (materialSettings->HardImpact || materialSettings->SolidImpact))
					{
						continue;
					}

					//If the other material is a loose surface then we don't play our scrape; this assumes only map materials are loose and never scrape
					if(!isPedCollision && AUD_GET_TRISTATE_VALUE(otherMaterialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_LOOSESURFACE) == AUD_TRISTATE_TRUE)
					{
						continue;
					}
				}
			}


			//Ensure we don't play the same scrape sound twice for a single collision event.
			if((numSoundsPlayed == 0) || (materialSettings != otherMaterialSettings) || (isWet && isPedCollision))
			{
				//Take the hardness of the other surface into account.
				//TODO: if we decide we want this, probably move it into the lower level
				//scaledScrapeSpeed = collisionEvent->scrapeMagnitudes[i];//scrapeSpeed; // * GetHardnessOfMaterial(otherMaterialSettings);

				ComputePitchAndVolumeFromScrapeSpeed(collisionEvent, i, pitch, volume);

				if(volume <= g_SilenceVolume)
				{
					continue;
				}

				if(materialSettings->CollisionCount >= g_CollisionMaterialCountThreshold)
				{
					if(volume > materialSettings->VolumeThreshold)
					{
						materialSettings->VolumeThreshold = volume;
					}
					else
					{
						continue;
					}
				}

				audSoundInitParams initParams;


				// Need to work out now what occlusion group to use
				initParams.EnvironmentGroup = GetOcclusionGroup(collisionEvent);
				
				PF_INCREMENT(audCollisionAudioEntity_Scrapes);
				if(isPedCollision && isWet)
				{
					CreateSound_PersistentReference(wetScrapeLayer, &(collisionEvent->scrapeSounds[i]),
						&initParams);
				}
				else
				{
					CreateSound_PersistentReference(scrapeSound, &(collisionEvent->scrapeSounds[i]),
						&initParams);
				}

				if(collisionEvent->scrapeSounds[i])
				{
					collisionEvent->scrapeSounds[i]->SetRequestedPitch(pitch);
					collisionEvent->scrapeSounds[i]->SetRequestedVolume(volume);
					collisionEvent->scrapeSounds[i]->SetRequestedPosition(collisionEvent->positions[i]);

					collisionEvent->scrapeSounds[i]->PrepareAndPlay();

					//naErrorf("Playing scrape sounds material %s, entity %p, index %d, time %d", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings->NameTableOffset), collisionEvent->entities[i].Get(), i, fwTimer::GetTimeInMilliseconds());

#if __BANK
					DebugAudioImpact(collisionEvent->positions[0], collisionEvent->components[0], collisionEvent->components[1], collisionEvent->impulseMagnitudes[0], (audCollisionType)collisionEvent->type);
#endif // #if __BANK

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						// We only need the entity to figure out the occlusion group so following what the 'GetOcclusionGroup' function does
						// we need entity[0] if it is valid...and entity[1] if not.
						CEntity* pEntity = NULL;
						CEntity* pEntity2 = NULL;
						naEnvironmentGroup* pGroup = NULL;

						if(collisionEvent->entities[0])
						{
							pEntity = collisionEvent->entities[0];
							pEntity2 = collisionEvent->entities[1];
							pGroup = (naEnvironmentGroup*)pEntity->GetAudioEnvironmentGroup();
						}
						if(pGroup == NULL && collisionEvent->entities[1])
						{
							pEntity = collisionEvent->entities[0];
							pEntity2 = collisionEvent->entities[1];
							pGroup = (naEnvironmentGroup*)pEntity->GetAudioEnvironmentGroup();
						}

						if(pGroup)
						{
							CReplayMgr::RecordPersistantFx<CPacketCollisionPlayPacket>(
								CPacketCollisionPlayPacket(materialSettings->ScrapeSound, 
								pitch,
								volume,
								collisionEvent->positions,
								i),
								CTrackedEventInfo<tTrackedSoundType>(collisionEvent->scrapeSounds[i]), 
								pEntity,
								true);
						}
						else
						{
							CReplayMgr::RecordPersistantFx<CPacketCollisionPlayPacket>(
								CPacketCollisionPlayPacket(materialSettings->ScrapeSound, 
								pitch,
								volume,
								collisionEvent->positions,
								collisionEvent->materialIds,
								i),
								CTrackedEventInfo<tTrackedSoundType>(collisionEvent->scrapeSounds[i]), 
								pEntity,
								pEntity2,
								true);
						}
					}
#endif // GTA_REPLAY
					numSoundsPlayed++;

					if(collisionEvent->materialSettings[i] && g_UseCollisionIntensityCulling)
					{
						if(!g_RecentlyCollidedMaterials.IsMemberOfList(collisionEvent->materialSettings[i]))
						{
							g_RecentlyCollidedMaterials.Add(collisionEvent->materialSettings[i]);
						}
						collisionEvent->materialSettings[i]->CollisionCount++;
					}

#if __BANK
					PF_INCREMENT(CollisonsProcessed);
#endif
				}
			}
		}
	}

	return (numSoundsPlayed > 0);
}

void audCollisionAudioEntity::UpdateScrapeSounds(audCollisionEvent *collisionEvent)
{
	//f32 scrapeSpeed = ComputeScrapeSpeed(collisionEvent);
	s32 pitch;
	f32 volume; 
	for(u8 i=0; i<2; i++)
	{
		if(collisionEvent->scrapeSounds[i]  && collisionEvent->materialSettings[i])
		{
			if(collisionEvent->scrapeMagnitudes[i] && collisionEvent->scrapeMagnitudes[i]  > collisionEvent->materialSettings[i]->MinScrapeSpeed)
			{
				ComputePitchAndVolumeFromScrapeSpeed(collisionEvent, i, pitch, volume);
				
				if(volume < (collisionEvent->scrapeSounds[i]->GetRequestedVolume() - 0.003f*(float)collisionEvent->scrapeSounds[i]->GetSimpleReleaseTime()))
				{
					volume = collisionEvent->scrapeSounds[i]->GetRequestedVolume() - 0.003f*(float)collisionEvent->scrapeSounds[i]->GetSimpleReleaseTime();
				}

				if(pitch < (collisionEvent->scrapeSounds[i]->GetRequestedPitch() - 0.003f*(float)collisionEvent->scrapeSounds[i]->GetSimpleReleaseTime()))
				{
					pitch = (s32)(collisionEvent->scrapeSounds[i]->GetRequestedPitch() - 0.003f*(float)collisionEvent->scrapeSounds[i]->GetSimpleReleaseTime());
				}

				collisionEvent->scrapeSounds[i]->SetRequestedPitch(pitch);
				collisionEvent->scrapeSounds[i]->SetRequestedVolume(volume);
				collisionEvent->scrapeSounds[i]->SetRequestedPosition(collisionEvent->positions[i]);
			}
			else
			{
				collisionEvent->scrapeSounds[i]->StopAndForget(); 

				//naErrorf("Stopping scrape sound on material %s time %u", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(collisionEvent->materialSettings[i]->NameTableOffset), fwTimer::GetTimeInMilliseconds()); 
			}
		}
	}
}

bool audCollisionAudioEntity::PlayRollSounds(audCollisionEvent *collisionEvent)
{
	u8 numSoundsPlayed = 0;
	//	f32 rollSpeed = ComputeRollSpeed(collisionEvent);

	if(collisionEvent->rollMagnitudes[0] > 0.0f || collisionEvent->rollMagnitudes[1] > 0.f)
	{
		for(u8 i=0; i<2; i++)
		{
			CollisionMaterialSettings *materialSettings = collisionEvent->materialSettings[i];
			CollisionMaterialSettings *otherMaterialSettings = collisionEvent->materialSettings[(i+1)&1];

			if(materialSettings == NULL || collisionEvent->rollSounds[i])
			{
				continue;
			}

			//Check to see if we're a map material or if we're on a material that allows roll sounds
			u32 rollSound = materialSettings->RollSound;
			
						
			if(rollSound == g_NullSoundHash)
			{
				continue;
			}

			s32 pitch;
			f32 volume;

			ComputePitchAndVolumeFromRollSpeed(collisionEvent, i, pitch, volume);

			CollisionMaterialSettings * otherSettings = collisionEvent->materialSettings[(i+1)&1];

			if(otherSettings)
			{
				if(otherSettings->HardImpact && otherSettings->HardImpact != g_NullSoundHash)
				{
					//roll volume stays the same
				}
				else if(otherSettings->SolidImpact && otherSettings->SolidImpact != g_NullSoundHash)
				{
					volume+= g_SolidRollAttenuation;
				}
				else
				{
					volume += g_SoftRollAttenuation;
				}
			}

			if(collisionEvent->materialSettings[i]->CollisionCount >= g_CollisionMaterialCountThreshold)
			{
				if(volume > collisionEvent->materialSettings[i]->VolumeThreshold)
				{
					collisionEvent->materialSettings[i]->VolumeThreshold = volume;
				}
				else
				{
					continue;
				}
			}

			//Ensure we don't play the same scrape sound twice for a single collision event.
			if((numSoundsPlayed == 0) || (materialSettings != otherMaterialSettings))
			{
				audSoundInitParams initParams;
				// Need to work out now what occlusion group to use
				initParams.EnvironmentGroup = GetOcclusionGroup(collisionEvent);
				
				PF_INCREMENT(audCollisionAudioEntity_Rolls);
				CreateSound_PersistentReference(rollSound, &(collisionEvent->rollSounds[i]),
					&initParams);
				if(collisionEvent->rollSounds[i])
				{
					collisionEvent->rollSounds[i]->SetClientVariable(0.0f);
					collisionEvent->rollSounds[i]->SetRequestedVolume(g_SilenceVolume);
					collisionEvent->rollSounds[i]->SetRequestedPosition(collisionEvent->positions[i]);

					collisionEvent->rollSounds[i]->PrepareAndPlay();
					numSoundsPlayed++;

					//naErrorf("Playing roll sounds material %s, entity %p", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings->NameTableOffset), collisionEvent->entities[i]); 
#if __BANK
					DebugAudioImpact(collisionEvent->positions[0], collisionEvent->components[0], collisionEvent->components[1], collisionEvent->impulseMagnitudes[0], (audCollisionType)collisionEvent->type);
#endif // #if __BANK

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CEntity* pEntity = NULL;
						CEntity* pEntity2 = NULL;
						naEnvironmentGroup* pGroup = NULL;

						if(collisionEvent->entities[0])
						{
							pEntity = collisionEvent->entities[0];
							pEntity2 = collisionEvent->entities[1];
							pGroup = (naEnvironmentGroup*)pEntity->GetAudioEnvironmentGroup();
						}
						if(pGroup == NULL && collisionEvent->entities[1])
						{
							pEntity = collisionEvent->entities[0];
							pEntity2 = collisionEvent->entities[1];
							pGroup = (naEnvironmentGroup*)pEntity->GetAudioEnvironmentGroup();
						}

						if(pGroup)
						{
							CReplayMgr::RecordPersistantFx<CPacketCollisionPlayPacket>(
								CPacketCollisionPlayPacket(rollSound, 
								g_SilenceVolume,
								collisionEvent->positions,
								i),
								CTrackedEventInfo<tTrackedSoundType>(collisionEvent->rollSounds[i]), 
								pEntity, 
								true);
						}
						else
						{
							CReplayMgr::RecordPersistantFx<CPacketCollisionPlayPacket>(
								CPacketCollisionPlayPacket(rollSound, 
								g_SilenceVolume,
								collisionEvent->positions,
								collisionEvent->materialIds,
								i),
								CTrackedEventInfo<tTrackedSoundType>(collisionEvent->rollSounds[i]), 
								pEntity,
								pEntity2,
								true);
						}
					}
#endif // GTA_REPLAY
				}
			}
		}
	}

	return (numSoundsPlayed > 0);
}


void audCollisionAudioEntity::UpdateRollSounds(audCollisionEvent *collisionEvent)
{
	//	f32 rollSpeed = ComputeRollSpeed(collisionEvent);
	s32 pitch;
	f32 volume;

	bool isMap[2] = {collisionEvent->material0IsMap, collisionEvent->material1IsMap};

	for(u8 i=0; i<2; i++)
	{
		if(collisionEvent->rollSounds[i] && collisionEvent->materialSettings[i])
		{
			f32 minRollSpeed = (isMap[i] && g_MapUsesPropRollSpeedSettings) || !collisionEvent->materialSettings[(i+1)&1] ? 
				collisionEvent->materialSettings[i]->MinRollSpeed : collisionEvent->materialSettings[(i+1)&1]->MinRollSpeed;

			if(collisionEvent->rollMagnitudes[i] > minRollSpeed)
			{
				ComputePitchAndVolumeFromRollSpeed(collisionEvent, i, pitch, volume);

				CollisionMaterialSettings * otherSettings = collisionEvent->materialSettings[(i+1)&1];

				if(otherSettings)
				{
					if(otherSettings->HardImpact && otherSettings->HardImpact != g_NullSoundHash)
					{
						//roll volume stays the same
					}
					else if(otherSettings->SolidImpact && otherSettings->SolidImpact != g_NullSoundHash)
					{
						volume+= g_SolidRollAttenuation;
					}
					else
					{
						volume += g_SoftRollAttenuation;
					}
				}

				//FIXME: Pitch seems to be coming out with nan's at the moment
				//			collisionEvent->rollSounds[i]->SetRequestedPitch(pitch);
				collisionEvent->rollSounds[i]->SetClientVariable(volume);
				collisionEvent->rollSounds[i]->SetRequestedVolume(volume);
				collisionEvent->rollSounds[i]->SetRequestedPosition(collisionEvent->positions[i]);
			}
			else
			{
				collisionEvent->rollSounds[i]->StopAndForget();
			}
		}
	}
}

f32 audCollisionAudioEntity::ComputeScaledImpulseMagnitude(audCollisionEvent *collisionEvent, u32 index)
{
	CollisionMaterialSettings *materialSettings = collisionEvent->materialSettings[index];
	if(!naVerifyf(materialSettings, "Invalid CollisionMaterialSettings passed into ComputeScaledaImpulseMagnitude"))
	{
		return 0.f;
	}


	f32 maxImpulse = materialSettings->MaxImpulseMag;
	f32 minImpulse = materialSettings->MinImpulseMag;
	f32 impulseMagScalar = materialSettings->ImpulseMagScalar * s_ImpactMagScalar;

	// impulse mag scalar is fixed point
	const f32 scaledMaxImpulseMag = (f32)maxImpulse; 
	const f32 scaledMinImpulseMag = (f32)minImpulse; 

	f32 scaledImpulseMag = Max(collisionEvent->impulseMagnitudes[index]*impulseMagScalar - scaledMinImpulseMag, 0.0f);
	scaledImpulseMag = Min(scaledImpulseMag / scaledMaxImpulseMag, 1.0f);

	return scaledImpulseMag;
}

f32 audCollisionAudioEntity::ComputeScaledImpactMagnitude(f32 impactMag, CollisionMaterialSettings * materialSettings)
{
	if(!naVerifyf(materialSettings, "Invalid CollisionMaterialSettings passed into ComputeScaledaImpulseMagnitude"))
	{
		return 0.f;
	}


	f32 maxImpulse = materialSettings->MaxImpulseMag;
	f32 minImpulse = materialSettings->MinImpulseMag;
	f32 impulseMagScalar = materialSettings->ImpulseMagScalar * s_ImpactMagScalar;

	// impulse mag scalar is fixed point
	const f32 scaledMaxImpulseMag = (f32)maxImpulse; 
	const f32 scaledMinImpulseMag = (f32)minImpulse; 

	f32 scaledImpulseMag = Max(impactMag*impulseMagScalar - scaledMinImpulseMag, 0.0f);
	scaledImpulseMag = Min(scaledImpulseMag / scaledMaxImpulseMag, 1.0f);

	return scaledImpulseMag;
}

void audCollisionAudioEntity::ComputeStartOffsetAndVolumeFromImpulseMagnitude(audCollisionEvent *collisionEvent, u32 index, u32 &startOffset, f32 &volume, bool isPed)
{
	startOffset = 0;
	volume = g_SilenceVolume;

	CollisionMaterialSettings *materialSettings = collisionEvent->materialSettings[index];

	if(materialSettings)
	{
		//Scale the impulse magnitude to a 0->1 range.
		f32 scaledImpulseMagnitude = ComputeScaledImpulseMagnitude(collisionEvent, index);
		if(scaledImpulseMagnitude > 0.f)
		{
			//			naDisplayf("Scaled impulse magnitude: %f", scaledImpulseMagnitude);
			//Derive a start offset percentage from a curve.
			if (AUD_GET_TRISTATE_VALUE(collisionEvent->materialSettings[index]->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES) != AUD_TRISTATE_TRUE)
			{
				startOffset = (u32)floorf((isPed?m_ImpactStartOffsetCurve.CalculateValue(scaledImpulseMagnitude):m_ImpactStartOffsetCurve.CalculateValue(scaledImpulseMagnitude)) * 100.0f);
			}
			else
			{
				audCurve curve;
				if(curve.Init(materialSettings->ImpactStartOffsetCurve))
				{
					startOffset = (u32)floorf(curve.CalculateValue(scaledImpulseMagnitude) * 100.0f);
				}
			}

			//Derive a volume from a curve.
			if (AUD_GET_TRISTATE_VALUE(collisionEvent->materialSettings[index]->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES) != AUD_TRISTATE_TRUE)
			{
				f32 volumeLinear = isPed ? m_PedImpactVolCurve.CalculateValue(scaledImpulseMagnitude):m_ImpactVolCurve.CalculateValue(scaledImpulseMagnitude);
				volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);
			}
			else
			{
				audCurve curve;
				if(curve.Init(materialSettings->ImpactVolCurve))
				{
					f32 volumeLinear = curve.CalculateValue(scaledImpulseMagnitude);
					volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);
				}
			}
		}
	}
}

void audCollisionAudioEntity::ComputeStartOffsetAndVolumeFromImpactMagnitude(f32 impactMag, CollisionMaterialSettings *materialSettings, u32 &startOffset, f32 &volume, bool isPed)
{
	startOffset = 0;
	volume = g_SilenceVolume;

	if(materialSettings)
	{
		//Scale the impulse magnitude to a 0->1 range.
		f32 scaledImpulseMagnitude = ComputeScaledImpactMagnitude(impactMag, materialSettings);
		if(scaledImpulseMagnitude > 0.f)
		{
			//			naDisplayf("Scaled impulse magnitude: %f", scaledImpulseMagnitude);
			//Derive a start offset percentage from a curve.
			if (AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES) != AUD_TRISTATE_TRUE)
			{
				startOffset = (u32)floorf(( isPed?m_PedImpactStartOffsetCurve.CalculateValue(scaledImpulseMagnitude):m_ImpactStartOffsetCurve.CalculateValue(scaledImpulseMagnitude)) * 100.0f);
			}
			else
			{
				audCurve curve;
				if(curve.Init(materialSettings->ImpactStartOffsetCurve))
				{
					startOffset = (u32)floorf(curve.CalculateValue(scaledImpulseMagnitude) * 100.0f);
				}
			}

			//Derive a volume from a curve.
			if (AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES) != AUD_TRISTATE_TRUE)
			{
				f32 volumeLinear = isPed? m_PedImpactVolCurve.CalculateValue(scaledImpulseMagnitude):m_ImpactVolCurve.CalculateValue(scaledImpulseMagnitude);
				volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);
			}
			else
			{
				audCurve curve;
				if(curve.Init(materialSettings->ImpactVolCurve))
				{
					f32 volumeLinear = curve.CalculateValue(scaledImpulseMagnitude);
					volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);
				}
			}
		}
	}
}

f32 audCollisionAudioEntity::ComputeScrapeSpeed(audCollisionEvent *collisionEvent)
{
	CEntity *entity = collisionEvent->entities[0];
	Vector3 moveSpeed(0.0f, 0.0f, 0.0f);
	Vector3 turnSpeed(0.0f, 0.0f, 0.0f);
	if(entity && entity->GetIsPhysical())
	{
		moveSpeed = ((CPhysical *)entity)->GetVelocity();
		turnSpeed = ((CPhysical *)entity)->GetAngVelocity();
	}

	CEntity *otherEntity = collisionEvent->entities[1];
	Vector3 otherMoveSpeed(0.0f, 0.0f, 0.0f);
	Vector3 otherTurnSpeed(0.0f, 0.0f, 0.0f);
	if(otherEntity && otherEntity->GetIsPhysical())
	{
		otherMoveSpeed = ((CPhysical *)otherEntity)->GetVelocity();
		otherTurnSpeed = ((CPhysical *)otherEntity)->GetAngVelocity();
	}

	f32 relativeMoveSpeed = (moveSpeed - otherMoveSpeed).Mag();
	f32 relativeTurnSpeed = (turnSpeed - otherTurnSpeed).Mag();
	f32 scrapeSpeed = Max(relativeMoveSpeed, relativeTurnSpeed);

	return scrapeSpeed;
}

void audCollisionAudioEntity::ComputePitchAndVolumeFromRollSpeed(audCollisionEvent *collisionEvent, u32 index, s32 &pitch, f32 &volume)
{
	pitch = 0;
	volume = g_SilenceVolume;

	CollisionMaterialSettings *materialSettings = collisionEvent->materialSettings[index];

	bool isMap[2] = {collisionEvent->material0IsMap, collisionEvent->material1IsMap};

	if(materialSettings)
	{
		f32 minRollSpeed = materialSettings->MinRollSpeed;
		f32 maxRollSpeed = materialSettings->MaxRollSpeed;

		if(collisionEvent->materialSettings[(index+1)&1] && isMap[index] && g_MapUsesPropRollSpeedSettings)
		{
			minRollSpeed = collisionEvent->materialSettings[(index+1)&1]->MinRollSpeed;
			maxRollSpeed = collisionEvent->materialSettings[(index+1)&1]->MaxRollSpeed;
			naAssertf(maxRollSpeed > 0.f, "Sound Designers: looks like material %s has an invalid MaxRollSpeed (%f); it's probably not doing what you want it to.", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(collisionEvent->materialSettings[(index+1)&1]->NameTableOffset), maxRollSpeed);
		}

		f32 scaledRollSpeed = maxRollSpeed > 0.f ? Min((collisionEvent->rollMagnitudes[index]-minRollSpeed) / maxRollSpeed, 1.0f) : 0.f;

		//Derive a pitch from a curve. 
		if (AUD_GET_TRISTATE_VALUE(collisionEvent->materialSettings[index]->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES) != AUD_TRISTATE_TRUE)
		{
			pitch = audDriverUtil::ConvertRatioToPitch(m_RollPitchCurve.CalculateValue(scaledRollSpeed));
		}
		else
		{
			audCurve curve;
			if(curve.Init(materialSettings->ScrapePitchCurve))
			{
				pitch = audDriverUtil::ConvertRatioToPitch(curve.CalculateValue(scaledRollSpeed));
			}
		}

		//Derive a volume from a curve.
		if (AUD_GET_TRISTATE_VALUE(collisionEvent->materialSettings[index]->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES) != AUD_TRISTATE_TRUE)
		{
			f32 volumeLinear = m_RollVolCurve.CalculateValue(scaledRollSpeed);
			volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);
		}
		else
		{
			audCurve curve;
			if(curve.Init(materialSettings->ScrapeVolCurve))
			{
				f32 volumeLinear = curve.CalculateValue(scaledRollSpeed);
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

f32 audCollisionAudioEntity::ComputeRollSpeed(audCollisionEvent *collisionEvent)
{
	CEntity *entity = collisionEvent->entities[0];
	Vector3 moveSpeed(0.0f, 0.0f, 0.0f);
	Vector3 turnSpeed(0.0f, 0.0f, 0.0f);
	if(entity && entity->GetIsPhysical())
	{
		moveSpeed = ((CPhysical *)entity)->GetVelocity();
		turnSpeed = ((CPhysical *)entity)->GetAngVelocity();
	}

	CEntity *otherEntity = collisionEvent->entities[1];
	Vector3 otherMoveSpeed(0.0f, 0.0f, 0.0f);
	Vector3 otherTurnSpeed(0.0f, 0.0f, 0.0f);
	if(otherEntity && otherEntity->GetIsPhysical())
	{
		otherMoveSpeed = ((CPhysical *)otherEntity)->GetVelocity();
		otherTurnSpeed = ((CPhysical *)otherEntity)->GetAngVelocity();
	}

	f32 rollSpeed = Max(turnSpeed.Mag(), otherTurnSpeed.Mag());

	return rollSpeed;
}

bool audCollisionAudioEntity::EntityIsInWater(CEntity * entity)
{
	if(entity && entity->GetIsPhysical() && ((CPhysical*)entity)->GetIsInWater())
	{
		return true;
	}
	return false;
}

void audCollisionAudioEntity::ComputePitchAndVolumeFromScrapeSpeed(audCollisionEvent *collisionEvent, u32 index, s32 &pitch, f32 &volume)
{
	pitch = 0;
	volume = g_SilenceVolume;
	int otherIndex = (index+1)&1;

	f32 wetness = g_weather.GetWetness();

	CollisionMaterialSettings *materialSettings = collisionEvent->materialSettings[index];

	if(collisionEvent->entities[otherIndex]->GetType() == ENTITY_TYPE_VEHICLE)
	{
		((CVehicle *)collisionEvent->entities[otherIndex].Get())->GetVehicleAudioEntity()->GetCollisionAudio().ComputePitchAndVolumeFromScrapeSpeed(collisionEvent, index, pitch, volume);
		return;
	}

	if(EntityIsInWater(collisionEvent->entities[index]) || EntityIsInWater(collisionEvent->entities[otherIndex]))
	{
		return;
	}

	// this allows only very heavy objects to scrape on vehicles(crates inside titan plane in exile1)
	bool secondMaterialisOk = true;
	if(!collisionEvent->materialSettings[otherIndex])
	{
		if((collisionEvent->entities[index] && collisionEvent->entities[otherIndex]) &&
			(collisionEvent->entities[otherIndex]->GetType() != ENTITY_TYPE_VEHICLE || 
			GetAudioWeight(collisionEvent->entities[index]) != AUDIO_WEIGHT_VH ||
			collisionEvent->entities[index]->GetType() != ENTITY_TYPE_OBJECT))
		{
				secondMaterialisOk = false;
		}
	}

	if(collisionEvent->materialSettings[index] && secondMaterialisOk)
	{
		bool isMap[2] = {collisionEvent->material0IsMap, collisionEvent->material1IsMap};

		bool isPed = collisionEvent->entities[index] && collisionEvent->entities[index]->GetIsTypePed();
		bool isPlayer = false;
		if(isPed)
		{
			if(((CPed *)collisionEvent->entities[index].Get())->IsLocalPlayer())
			{
				isPlayer = true;
			}
		}
		bool otherIsPed = collisionEvent->entities[otherIndex] && collisionEvent->entities[otherIndex]->GetIsTypePed();
		bool otherIsPlayer = false;
		if(otherIsPlayer)
		{
			if(((CPed *)collisionEvent->entities[otherIndex].Get())->IsLocalPlayer())
			{
				otherIndex = true;
			}
		}

		f32 minScrapeSpeed = materialSettings->MinScrapeSpeed;
		f32 maxScrapeSpeed = materialSettings->MaxScrapeSpeed;

		if((otherIsPed && !isPed) || (collisionEvent->materialSettings[otherIndex] && isMap[index] && g_MapUsesPropRollSpeedSettings))
		{
			minScrapeSpeed = collisionEvent->materialSettings[otherIndex]->MinScrapeSpeed;
			maxScrapeSpeed = collisionEvent->materialSettings[otherIndex]->MaxScrapeSpeed;

		}

		//Scale the scrape speed to a 0->1 range.
		f32 scaledScrapeSpeed = ClampRange((collisionEvent->scrapeMagnitudes[index]- minScrapeSpeed) / (maxScrapeSpeed-minScrapeSpeed), 0.f, 1.0f);

		if(!FPIsFinite(scaledScrapeSpeed))
		{
			naDebugf1("Got a NAN in ComputePitchAndVolumeFromScrapeSpeed");
			scaledScrapeSpeed = 0.f;
		}

		//Derive a pitch from a curve.
		if(isPed || otherIsPed)
		{
#if __BANK 
			if(g_DisplayPedScrapeMaterial)
			{
				naDisplayf("Ped scrape material: %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(collisionEvent->materialSettings[index]->NameTableOffset));
			}
#endif
			u32 pedIndex = isPed ? index : otherIndex;

			AnimalParams * animalParams = audNorthAudioEngine::GetObject<AnimalParams>(((CPed *)collisionEvent->entities[pedIndex].Get())->GetPedModelInfo()->GetAnimalAudioObject());
			audCurve animalCurve;
			if(animalParams)
			{
				animalCurve.Init(animalParams->ScrapePitchCurve);
			}

			if(isPlayer || otherIsPlayer)
			{
				pitch = audDriverUtil::ConvertRatioToPitch(m_PlayerScrapePitchCurve.CalculateValue(scaledScrapeSpeed));
			}
			else if(animalParams && animalCurve.IsValid()) 
			{
				pitch = audDriverUtil::ConvertRatioToPitch(m_PedScrapePitchCurve.CalculateValue(scaledScrapeSpeed));
			}
			else
			{
				pitch = audDriverUtil::ConvertRatioToPitch(m_PedScrapePitchCurve.CalculateValue(scaledScrapeSpeed));
			}
		}
		else if (AUD_GET_TRISTATE_VALUE(collisionEvent->materialSettings[index]->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES) != AUD_TRISTATE_TRUE)
		{
			pitch = audDriverUtil::ConvertRatioToPitch(m_ScrapePitchCurve.CalculateValue(scaledScrapeSpeed));
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
		if(isPed || otherIsPed)
		{
			u32 pedIndex = isPed ? index : otherIndex;

			AnimalParams * animalParams = audNorthAudioEngine::GetObject<AnimalParams>(((CPed *)collisionEvent->entities[pedIndex].Get())->GetPedModelInfo()->GetAnimalAudioObject());
			audCurve animalCurve;
			if(animalParams)
			{
				animalCurve.Init(animalParams->ScrapeVolCurve);
			}

			f32 volumeLinear = 0.f;
			if(isPlayer || otherIsPlayer)
			{
				volumeLinear = m_PlayerScrapeVolCurve.CalculateValue(scaledScrapeSpeed);
			}
			else if(animalParams && animalCurve.IsValid()) //ANIMAL
			{
				volumeLinear = animalCurve.CalculateValue(scaledScrapeSpeed);
			}
			else
			{
				volumeLinear = m_PedScrapeVolCurve.CalculateValue(scaledScrapeSpeed);
			}
			if(!isPed && otherIsPed)
			{
				volumeLinear *= g_MaterialScrapeAgainstPedVolumeScaling;
			}
			if(wetness > 0.f && !collisionEvent->entities[pedIndex]->GetIsInInterior() && collisionEvent->entities[pedIndex]->GetIsInExterior())
			{
				if(isPed)
				{
					volumeLinear *= wetness;
				}
				else
				{
					volumeLinear *= g_PedWetSurfaceScrapeAtten;
				}
			}
			volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeLinear);
		}
		else if (AUD_GET_TRISTATE_VALUE(collisionEvent->materialSettings[index]->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES) != AUD_TRISTATE_TRUE)
		{
			f32 volumeLinear = m_ScrapeVolCurve.CalculateValue(scaledScrapeSpeed);
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

audCollisionEvent *audCollisionAudioEntity::GetRollCollisionEventFromHistory(CEntity* entityA, CEntity * entityB, u16 * outIndex)
{
	for(u32 eventIndex=0; eventIndex<g_MaxCollisionEventHistory; eventIndex++)
	{
		const bool isAllocated = m_CollisionEventHistoryAllocation.IsSet(eventIndex);
		if(isAllocated)
		{
			audCollisionEvent & collisionEvent =  m_CollisionEventHistory[eventIndex];

			bool foundEntityA = false, foundEntityB = false;

			if(!entityA || (collisionEvent.entities[0]== entityA || collisionEvent.entities[1] == entityA))
			{
				foundEntityA = true;
			}
			if(!entityB || (collisionEvent.entities[0]== entityB || collisionEvent.entities[1] == entityB))
			{
				foundEntityB = true;
			}

			if(foundEntityA && foundEntityB)
			{
				*outIndex = (u16)eventIndex;
				return &collisionEvent;
			}
		}
	}
	return NULL;
}

audSwingEvent * audCollisionAudioEntity::GetSwingEventFromHistory(CEntity * entity)
{
	for(int i=0; i<g_MaxPlayerCollisionEventHistory; i++)
	{
		const bool isAllocated = m_SwingEventHistoryAllocation.IsSet(i);
		if(isAllocated)
		{
			audSwingEvent & colEvent = m_SwingEventHistory[i];
			if(entity == colEvent.swingEnt.Get())
			{
				return &colEvent;
			}
		}
	}

	return NULL;
}

audPlayerCollisionEvent * audCollisionAudioEntity::GetPlayerCollisionEventFromHistory(CEntity * entity)
{
	for(int i=0; i<g_MaxPlayerCollisionEventHistory; i++)
	{
		const bool isAllocated = m_PlayerCollisionEventHistoryAllocation.IsSet(i);
		if(isAllocated)
		{
			audPlayerCollisionEvent & colEvent = m_PlayerCollisionEventHistory[i];
			if(entity == colEvent.hitEntity.Get())
			{
				return &colEvent;
			}
		}
	}

	return NULL;
}

void audCollisionAudioEntity::HandleEntitySwinging(CEntity * entity)
{
	if(entity && entity->GetBaseModelInfo() && entity->GetBaseModelInfo()->GetAudioCollisionSettings() && AUD_GET_TRISTATE_VALUE(entity->GetBaseModelInfo()->GetAudioCollisionSettings()->Flags, FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_SWINGINGPROP))
	{
		audSwingEvent * swingEvent = FindOrAllocateSwingEvent(entity);
		if(swingEvent)
		{
			swingEvent->lastImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
			swingEvent->swingEnt = entity;
		}
	}
}

audSwingEvent * audCollisionAudioEntity::FindOrAllocateSwingEvent(CEntity * entity)
{
	audSwingEvent * playerEvent = GetSwingEventFromHistory(entity);

	if(playerEvent)
	{
		return playerEvent;
	}

	return GetFreeSwingEventFromHistory();

}

audPlayerCollisionEvent * audCollisionAudioEntity::FindOrAllocatePlayerCollisionEvent(CEntity * entity)
{
	audPlayerCollisionEvent * playerEvent = GetPlayerCollisionEventFromHistory(entity);

	if(playerEvent)
	{
		return playerEvent;
	}

	return GetFreePlayerCollisionEventFromHistory();

}

void audCollisionAudioEntity::HandlePlayerEvent(CEntity * entity)
{
	audPlayerCollisionEvent* playerEvent = FindOrAllocatePlayerCollisionEvent(entity);
	if(playerEvent)
	{
		playerEvent->lastImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		playerEvent->hitEntity = entity;
	}
}


audCollisionEvent *audCollisionAudioEntity::GetFreeCollisionEventFromHistory(u16 *index)
{
	audCollisionEvent *event = NULL;

	const s32 historyIndex = m_CollisionEventFreeList.Allocate();
	if(historyIndex == -1)
	{
		return NULL;
	}

	audAssertf(m_CollisionEventHistoryAllocation.IsClear(historyIndex), "Allocation bitset disagrees with freelist, index %p", index);
	const u16 eventIndex = (u16)historyIndex;
	
	event = &(m_CollisionEventHistory[eventIndex]);
	m_CollisionEventHistoryAllocation.Set(eventIndex);
	event->historyIndex = eventIndex;
	if(index)
	{
		naAssertf(eventIndex < 0xffff, "Collision history index is out of bounds: reduce g_MaxCollisionEventHistory (!!!!)");
		*index = (u16)eventIndex;
	}
	
	return event;
}

audSwingEvent *audCollisionAudioEntity::GetFreeSwingEventFromHistory(u16 *index)
{
	audSwingEvent *event = NULL;

	const s32 historyIndex = m_SwingEventFreeList.Allocate();
	if(historyIndex == -1)
	{
		return NULL;
	}

	audAssertf(m_SwingEventHistoryAllocation.IsClear(historyIndex), "Allocation bitset disagrees with freelist, index %p", index);
	const u16 eventIndex = (u16)historyIndex;

	event = &(m_SwingEventHistory[eventIndex]);
	event->Init();
	m_SwingEventHistoryAllocation.Set(eventIndex);
	event->historyIndex = eventIndex;
	if(index)
	{
		naAssertf(eventIndex < 0xffff, "Collision history index is out of bounds: reduce g_MaxPlayerCollisionEventHistory (!!!!)");
		*index = (u16)eventIndex;
	}

	return event;
}

audPlayerCollisionEvent *audCollisionAudioEntity::GetFreePlayerCollisionEventFromHistory(u16 *index)
{
	audPlayerCollisionEvent *event = NULL;

	const s32 historyIndex = m_PlayerCollisionEventFreeList.Allocate();
	if(historyIndex == -1)
	{
		return NULL;
	}

	audAssertf(m_PlayerCollisionEventHistoryAllocation.IsClear(historyIndex), "Allocation bitset disagrees with freelist, index %p", index);
	const u16 eventIndex = (u16)historyIndex;

	event = &(m_PlayerCollisionEventHistory[eventIndex]);
	m_PlayerCollisionEventHistoryAllocation.Set(eventIndex);
	event->historyIndex = eventIndex;
	if(index)
	{
		naAssertf(eventIndex < 0xffff, "Collision history index is out of bounds: reduce g_MaxPlayerCollisionEventHistory (!!!!)");
		*index = (u16)eventIndex;
	}

	return event;
}

void audCollisionAudioEntity::TriggerDeferredBulletImpact(const u32 reqHash, const Vector3 &pos, f32 volume, f32 frequencyScaling,bool playerShot,bool isRicochets, CEntity * entity, const phMaterialMgr::Id matID, CAmmoInfo::SpecialType ammoType, bool isHeadShot, u32 slowMoSoundHash)
{
	u32 hash = reqHash;
	if(hash == ATSTRINGHASH("VEHICLE_BULLET_IMPACT_MULTI", 0x95862DA3))
	{
		if(playerShot && g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeFranklin)
		{
			hash = ATSTRINGHASH("FRANKLIN_BULLET_VEHICLE_MASTER", 0x961AC398);
		}
		else if(ammoType == CAmmoInfo::FMJ)
		{			
			audSoundSet specialImpactSoundset;

			if(specialImpactSoundset.Init(ATSTRINGHASH("SPECIAL_AMMO_TYPE_BULLET_IMPACTS", 0x5F8529E3)))
			{
				const audSoundSetFieldRef soundRef = audSoundSetFieldRef(specialImpactSoundset, ATSTRINGHASH("VEHICLE_IMPACT_FMJ", 0x884237D8));
				const audSoundSetFieldRef slowMoSoundRef = audSoundSetFieldRef(specialImpactSoundset, ATSTRINGHASH("VEHICLE_IMPACT_FMJ_SLOW_MO", 0x5D0000DD));
				TriggerDeferredBulletImpact(soundRef, pos, volume, frequencyScaling, playerShot, isRicochets, entity, matID, ammoType, isHeadShot, slowMoSoundRef);
			}
		}
	}
	else if(hash == ATSTRINGHASH("GLASS_BULLET_STRIKE_MASTER", 0xAA251F8E))
	{
		if(ammoType == CAmmoInfo::FMJ)
		{
			audSoundSet specialImpactSoundset;

			if(specialImpactSoundset.Init(ATSTRINGHASH("SPECIAL_AMMO_TYPE_BULLET_IMPACTS", 0x5F8529E3)))
			{
				const audSoundSetFieldRef soundRef = audSoundSetFieldRef(specialImpactSoundset, ATSTRINGHASH("VEHICLE_GLASS_IMPACT_FMJ", 0x9DF7FD80));
				const audSoundSetFieldRef slowMoSoundRef = audSoundSetFieldRef(specialImpactSoundset, ATSTRINGHASH("VEHICLE_GLASS_IMPACT_FMJ_SLOW_MO", 0x2CFE212A));
				TriggerDeferredBulletImpact(soundRef, pos, volume, frequencyScaling, playerShot, isRicochets, entity, matID, ammoType, isHeadShot, slowMoSoundRef);
			}
		}
	}

	const audSoundSetFieldRef soundRef = hash;
	const audSoundSetFieldRef slowMoSoundRef = slowMoSoundHash;
	TriggerDeferredBulletImpact(soundRef, pos, volume, frequencyScaling, playerShot, isRicochets, entity, matID, ammoType, isHeadShot, slowMoSoundRef);
}

void audCollisionAudioEntity::TriggerDeferredBulletImpact(const audSoundSetFieldRef soundRef, const Vector3 &pos, f32 volume, f32 UNUSED_PARAM(frequencyScaling),
															bool playerShot,bool isRicochets, CEntity * entity, const phMaterialMgr::Id matID, CAmmoInfo::SpecialType ammoType, bool isHeadShot, audSoundSetFieldRef slowMoSoundRef)
{
	if(!m_CachedBulletImpacts.IsFull())
	{
		// B*5073502 - Ignore bullet impacts when we're in an online cutscene
		if (NetworkInterface::IsGameInProgress() && audNorthAudioEngine::IsCutsceneActive())
		{
			return;
		}

		audBulletImpact &impact = m_CachedBulletImpacts.Append();
		impact.soundRef = soundRef;
		impact.slowMotionSoundRef = slowMoSoundRef;
		impact.pos = pos;
		impact.volume = volume;
		impact.entity = entity;
		impact.matID = matID;
		impact.ammoType = ammoType;			

		f32 distanceToListener = g_AudioEngine.GetEnvironment().ComputeSqdDistanceToPanningListener(VECTOR3_TO_VEC3V(pos));

		if(playerShot && isRicochets)
		{
			impact.rollOffScale = sm_PlayerRicochetsRollOffScaleFactor;
			impact.predelay = (u16)sm_DistanceToRicochetsPredelay.CalculateValue(distanceToListener);
		}
		else
		{
			impact.predelay = (u16)sm_DistanceToBIPredelay.CalculateValue(distanceToListener);
			if(playerShot)
			{
				impact.rollOffScale = sm_PlayerBIRollOffScaleFactor;
				if(isHeadShot)
				{
					impact.predelay = (u16)sm_DistanceToHeadShotPredelay.CalculateValue(distanceToListener);
					impact.rollOffScale = sm_PlayerHeadShotRollOffScaleFactor;
				}
			}
			else
			{
				impact.rollOffScale = 1.f;
			}
		}		
	}
}

void audCollisionAudioEntity::TriggerDeferredMeleeImpact(const WorldProbe::CShapeTestHitPoint* pResult, const f32 damageDone, f32 impulseMagnitudeDamping, const bool isBullet,bool playerShot, bool isFootstep, CEntity * hittingEntity, bool isPunch, bool isKick)
{
	if(m_CachedMeleeImpacts.IsFull())
	{
		naErrorf("Cached melee impacts array is full");
		return;
	}

	CEntity *entity = NULL;
	const phInst *instance = pResult->GetHitInst();
	if(instance)
	{
		entity = CPhysics::GetEntityFromInst((phInst *)instance);
		if(entity == NULL)
		{
			return;
		}
	}
	else
	{
		return;
	}

	HandleEntitySwinging(entity);

	audCollisionEvent &collisionEvent = m_CachedMeleeImpacts.Append();

	collisionEvent.SetFlag(AUD_COLLISION_FLAG_MELEE);
	if(isFootstep)
	{
		collisionEvent.SetFlag(AUD_COLLISION_FLAG_FOOTSTEP);
	}
	if(isBullet)
	{
		collisionEvent.SetFlag(AUD_COLLISION_FLAG_BULLET);
	}
	if(isPunch)
	{
		collisionEvent.SetFlag(AUD_COLLISION_FLAG_PUNCH);
	}
	else if(isKick)
	{
		collisionEvent.SetFlag(AUD_COLLISION_FLAG_KICK);
	}

	collisionEvent.entities[0] = entity;
	collisionEvent.entities[1] = hittingEntity;

	if(playerShot)
	{
		collisionEvent.rollOffScale = sm_PlayerBIRollOffScaleFactor;
	}
	if(isBullet)
	{
		f32 distanceToListener = g_AudioEngine.GetEnvironment().ComputeSqdDistanceToPanningListener(VECTOR3_TO_VEC3V(pResult->GetHitPosition()));
		collisionEvent.predelay = (u16)sm_DistanceToBIPredelay.CalculateValue(distanceToListener);
	}

	collisionEvent.type = AUD_COLLISION_TYPE_IMPACT;
	
	PopulateAudioCollisionEventFromIntersection(&collisionEvent, pResult);

	//Apply a bit of a hack to ensure that we hit vehicles hard enough to be consistent with other entity types.
	f32 maxImpulseMag = 100.f;
	Assert(entity);
	if(!isBullet)
	{
		if(entity->GetType() == ENTITY_TYPE_VEHICLE)
		{
			maxImpulseMag = g_MaxImpulseMagnitudeForMeleeDamageDoneVehicle;
		}
		else
		{
			maxImpulseMag = g_MaxImpulseMagnitudeForMeleeDamageDoneGeneral;
		}
	}

	if(isFootstep)
	{
		collisionEvent.impulseMagnitudes[0] = maxImpulseMag * m_MeleeFootstepImpactScalingCurve.CalculateValue(
			damageDone / g_MaxMeleeDamageDoneForCollision);
	}
	else
	{
		collisionEvent.impulseMagnitudes[0] = impulseMagnitudeDamping * maxImpulseMag * m_MeleeDamageImpactScalingCurve.CalculateValue(
			damageDone / g_MaxMeleeDamageDoneForCollision);
	}
	
}

void audCollisionAudioEntity::TriggerDeferedImpacts()
{
	if(g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
	{
		for(s32 i = 0; i < m_CachedBulletImpacts.GetCount(); i++)
		{
			audSoundInitParams initParams;

			initParams.Predelay = (u32)m_CachedBulletImpacts[i].predelay;
			naAssert(m_CachedBulletImpacts[i].rollOffScale != 0.f);
			initParams.VolumeCurveScale = m_CachedBulletImpacts[i].rollOffScale;
			initParams.Position = m_CachedBulletImpacts[i].pos;
			initParams.EnvironmentGroup = CreateBulletImpactEnvironmentGroup(VECTOR3_TO_VEC3V(m_CachedBulletImpacts[i].pos), m_CachedBulletImpacts[i].entity, m_CachedBulletImpacts[i].matID); 

			f32 fFatalShotVolumeBoost = 0.0f;
			if(m_CachedBulletImpacts[i].entity && m_CachedBulletImpacts[i].entity->GetIsTypePed())
			{
				CEntity* entity = m_CachedBulletImpacts[i].entity.Get();
				CPed* ped =  static_cast<CPed*>(entity);
				if(ped->IsFatallyInjured() && ped->IsPlayer())
				{
					fFatalShotVolumeBoost = 20.0f;
					initParams.Predelay = 0;
					CreateAndPlaySound(m_CachedBulletImpacts[i].slowMotionSoundRef.GetMetadataRef(), &initParams);
				}
			}

			initParams.Volume = m_CachedBulletImpacts[i].volume + fFatalShotVolumeBoost;

			CreateAndPlaySound(m_CachedBulletImpacts[i].soundRef.GetMetadataRef(), &initParams);

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				audSoundSetFieldRef resolvedSlowMoSoundRef = m_CachedBulletImpacts[i].slowMotionSoundRef != g_NullMaterialRef? m_CachedBulletImpacts[i].slowMotionSoundRef : g_NullSoundHash;
				CReplayMgr::RecordFx<CPacketBulletImpactNew>(CPacketBulletImpactNew(m_CachedBulletImpacts[i].soundRef, resolvedSlowMoSoundRef, initParams.Position, initParams.Volume));
			}
#endif
		}
	}

	m_CachedBulletImpacts.Reset();

	for(s32 i = 0; i < m_CachedMeleeImpacts.GetCount(); i++)
	{
		ProcessImpactSounds(&m_CachedMeleeImpacts[i]);
		m_CachedMeleeImpacts[i].Init();
	}
	m_CachedMeleeImpacts.Reset();

	TriggerDeferredFragmentBreaks();
	TriggerDeferredUproots();
}

void audCollisionAudioEntity::ReportBulletHit(u32 weaponAudioHash, const WorldProbe::CShapeTestHitPoint* pResult, const Vector3 &vWeaponPosition, WeaponSettings * weaponSettings, CAmmoInfo::SpecialType ammoType, bool playerShot, bool bulletHitPlayer, bool isAutomatic, bool isShotgun, f32 timeBetweenShots)
{
#if __BANK 
	if(g_DisableCollisionAudio)
		return;
#endif
	if(!pResult)
	{
		return;
	}
	if(weaponSettings)
	{
		if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) < weaponSettings->LastBulletImpactTime + weaponSettings->BulletImpactTimeFilter)
		{
			return;
		}
	}

	bool dontPlayBI = g_ScriptAudioEntity.ShouldDuckForScriptedConversation();
	dontPlayBI = dontPlayBI && !playerShot && !bulletHitPlayer;
	
	dontPlayBI = dontPlayBI && sm_DialogueSuppressBI;

	if(dontPlayBI)
	{
		return;
	}

	if(weaponSettings)
	{
		weaponSettings->LastBulletImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	}

	CollisionMaterialSettings *materialSettings = NULL;

	Vector3 vBulletPath = pResult->GetHitPosition() - vWeaponPosition;
	Vector3 vCameraPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
	f32 cameraDistanceFromGun = (vCameraPosition-vWeaponPosition).Mag();
	f32 cameraDistanceFromImpact = (vCameraPosition- pResult->GetHitPosition()).Mag();
	f32 impactCloserDistance = cameraDistanceFromGun - cameraDistanceFromImpact;
	impactCloserDistance = Clamp(impactCloserDistance, g_MinBulletImpactDampingDistance, g_MaxBulletImpactDampingDistance);
	f32 distanceRange = Max(1.0f, g_MaxBulletImpactDampingDistance-g_MinBulletImpactDampingDistance);
	f32 impactCloserRatio = (impactCloserDistance - g_MinBulletImpactDampingDistance) / distanceRange;
	f32 impactCloserAttenuation = g_MaxBulletImpactCloserAttenuation * impactCloserRatio;
	impactCloserRatio = Clamp(impactCloserRatio, 0.0f, 1.0f);
	f32 impactCloserImpulseDamping = 1.0f-impactCloserRatio + g_MaxBulletImpactCloserImpulseDamping * impactCloserRatio;

	CEntity* pHitEntity = CPhysics::GetEntityFromInst(const_cast<phInst *>(pResult->GetHitInst()));

	bool doesMeleeImpacts = weaponSettings && AUD_GET_TRISTATE_VALUE(weaponSettings->Flags, FLAG_ID_WEAPONSETTINGS_DOESMELEEIMPACTS) == AUD_TRISTATE_TRUE;

	if(weaponSettings == audNorthAudioEngine::GetObject<WeaponSettings>(ATSTRINGHASH("W_SPL_SNOWBALL_PLAYER", 0xC622EAD0)))
	{
		return;
	}
	
	if(pHitEntity && pHitEntity->GetType() == ENTITY_TYPE_PED && !doesMeleeImpacts)
	{
		bool isHeadshot = false;
		CPed *pHitPed = (CPed*)pHitEntity;
		naAssertf(pHitPed, "Casting a CEntity to a CPed has somehow made it null!");
		if(pHitPed->GetRagdollInst() && pResult->GetHitComponent() < pHitPed->GetRagdollInst()->GetTypePhysics()->GetNumChildren())
		{
			s32 boneIndex = pHitPed->GetRagdollInst()->GetType()->GetBoneIndexFromID(
				pHitPed->GetRagdollInst()->GetTypePhysics()->GetAllChildren()[pResult->GetHitComponent()]->GetBoneID());
			s32 headBoneIndex = pHitPed->GetBoneIndexFromBoneTag(BONETAG_HEAD);
			if( headBoneIndex != BONETAG_INVALID )
			{
				isHeadshot = (boneIndex == headBoneIndex);
				// only play the headshot sound if the ped is alive
				isHeadshot = (isHeadshot && !pHitPed->IsDead());
			}
		}

		audSoundSetFieldRef soundRef;
		audSoundSetFieldRef slowMoSoundRef;

		// Overrides for special ammo types
		if(ammoType != CAmmoInfo::SpecialType::None)
		{
			audSoundSet specialImpactSoundset;

			if(specialImpactSoundset.Init(ATSTRINGHASH("SPECIAL_AMMO_TYPE_BULLET_IMPACTS", 0x5F8529E3)))
			{
				switch(ammoType)
				{
				case CAmmoInfo::ArmorPiercing:
					soundRef.Set(specialImpactSoundset, ATSTRINGHASH("PED_IMPACT_ARMOUR_PIERCING", 0xE7C5BED6));
					slowMoSoundRef.Set(specialImpactSoundset, ATSTRINGHASH("PED_IMPACT_ARMOUR_PIERCING_SLOW_MO", 0x2B62268D));
					break;

				case CAmmoInfo::HollowPoint:
					soundRef.Set(specialImpactSoundset, ATSTRINGHASH("PED_IMPACT_HOLLOW_POINT", 0x89F23D95));
					slowMoSoundRef.Set(specialImpactSoundset, ATSTRINGHASH("PED_IMPACT_HOLLOW_POINT_SLOW_MO", 0xC3AC5A50));
					break;

				default:
					break;
				}
			}
		}		

		if(!soundRef.IsValid())
		{
			u32 hitComponent = pResult->GetHitComponent();
			if(hitComponent >= RAGDOLL_SPINE0)
			{
				audSoundSet bulletSounds;
				audPedAudioEntity * audioEnt = pHitPed->GetPedAudioEntity();
				if(audioEnt)
				{
					const ClothAudioSettings * clothSettings = audioEnt->GetFootStepAudio().GetCachedUpperBodyClothSounds();
					if(clothSettings)
					{
						bulletSounds.Init(clothSettings->BulletImpacts);
						if(isHeadshot)
						{
							soundRef.Set(bulletSounds, ATSTRINGHASH("head", 0x84533F51));
							slowMoSoundRef.Set(bulletSounds, ATSTRINGHASH("headSlowMo", 0x19DAA68C));
						}
						else if(hitComponent >= RAGDOLL_UPPER_ARM_LEFT && hitComponent <= RAGDOLL_HAND_RIGHT)
						{
							soundRef.Set(bulletSounds, ATSTRINGHASH("arms", 0x903C14B));
							slowMoSoundRef.Set(bulletSounds, ATSTRINGHASH("armsSlowMo", 0x67DED9B5));
						}
						else
						{
							soundRef.Set(bulletSounds, ATSTRINGHASH("torso", 0x7EA8C1D8));
							slowMoSoundRef.Set(bulletSounds, ATSTRINGHASH("torsoSlowMo", 0x436BF408));
						}
					}
				}
			}
			else
			{
				audSoundSet bulletSounds;
				audPedAudioEntity * audioEnt = pHitPed->GetPedAudioEntity();
				if(audioEnt)
				{
					const ClothAudioSettings * clothSettings = audioEnt->GetFootStepAudio().GetCachedLowerBodyClothSounds();
					if(clothSettings)
					{
						bulletSounds.Init(clothSettings->BulletImpacts);
						soundRef.Set(bulletSounds, ATSTRINGHASH("legs", 0x8650F673));
						slowMoSoundRef.Set(bulletSounds, ATSTRINGHASH("legsSlowMo", 0x91D3A75E));
					}
				}
			}
		}

		if(weaponSettings && weaponSettings->HitPedSound != g_NullSoundHash)
		{
			audSoundInitParams initParams;
			initParams.Position = pResult->GetHitPosition();
			if(g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
			{
				CreateDeferredSound(weaponSettings->HitPedSound, pHitEntity, &initParams, true);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(weaponSettings->HitPedSound, &initParams, pHitEntity, NULL, eCollisionAudioEntity));
			}
		}

#if __BANK
		if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && slowMoSoundRef != g_NullSoundRef)
		{
			soundRef = slowMoSoundRef;
		}
#endif

		pHitPed->GetPedAudioEntity()->UpBodyfallForShooting();
		PF_INCREMENT(audCollisionAudioEntity_Bullets);
		TriggerDeferredBulletImpact(soundRef, pResult->GetHitPosition(), impactCloserAttenuation, 1.f, playerShot, false, pHitEntity, pResult->GetHitMaterialId(), ammoType, isHeadshot REPLAY_ONLY(,slowMoSoundRef));

		if(audNorthAudioEngine::ShouldTriggerPulseHeadset() && pHitPed->IsLocalPlayer())
		{
			Vec3V gunToBulletNrm = Normalize(RCC_VEC3V(vBulletPath));
			const Mat34V &listener = g_AudioEngine.GetEnvironment().GetPanningListenerMatrix(0);
			float bulletListenerDotProd = Dot(gunToBulletNrm, listener.GetCol1()).Getf();
			const float angle = AcosfSafe(bulletListenerDotProd);
			const float degrees = RtoD*angle;
			float actualDegrees;
			if(IsLessThanOrEqual(Dot(gunToBulletNrm, listener.GetCol0()), ScalarV(V_ZERO)).Getb())
			{
				actualDegrees = 360.0f - degrees;
			}
			else
			{
				actualDegrees = degrees;
			}

			audSoundInitParams initParams;
			initParams.Pan = (s16)actualDegrees;
			audDisplayf("Triggering pulse bullet impact: %f", actualDegrees);
			g_FrontendAudioEntity.CreateAndPlaySound(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("BulletImpact", 0xDD9382DE)), &initParams);
		}

#if GTA_REPLAY
		s32 mati = 0;
		if(naVerifyf(PGTAMATERIALMGR->UnpackMtlId(pResult->GetHitMaterialId())<(phMaterialMgr::Id)g_audCollisionMaterials.GetCount(), "Unpacked mtl id too large"))
		{
			if(naVerifyf(PGTAMATERIALMGR->UnpackMtlId(pResult->GetHitMaterialId())<(phMaterialMgr::Id)0xffff, "Unpacked mtl id too big for s32 cast"))
			{ 
				mati = static_cast<s32>(PGTAMATERIALMGR->UnpackMtlId(pResult->GetHitMaterialId()));
			}
		}

		materialSettings = g_audCollisionMaterials[mati];

		if(materialSettings && pHitEntity)
		{
			u32 slowMoPresuckSound = materialSettings->SlowMoBulletImpactPreSuck;
			u32 slowMoPresuckTime = materialSettings->SlowMoBulletImpactPreSuckTime;


			if(slowMoPresuckSound != g_NullSoundHash)
			{
				CReplayMgr::ReplayPresuckSound(slowMoPresuckSound, slowMoPresuckTime, 0u, pHitEntity);
			}	
		}
#endif

	}
	else
	{

		HandleEntitySwinging(pHitEntity);
		
		//Find the material settings from base material/prop overrides
		s32 mati = 0;
		if(naVerifyf(PGTAMATERIALMGR->UnpackMtlId(pResult->GetHitMaterialId())<(phMaterialMgr::Id)g_audCollisionMaterials.GetCount(), "Unpacked mtl id too large"))
		{
			if(naVerifyf(PGTAMATERIALMGR->UnpackMtlId(pResult->GetHitMaterialId())<(phMaterialMgr::Id)0xffff, "Unpacked mtl id too big for s32 cast"))
			{ 
				mati = static_cast<s32>(PGTAMATERIALMGR->UnpackMtlId(pResult->GetHitMaterialId()));
			}
		}
		if(pHitEntity && pHitEntity->GetType() == ENTITY_TYPE_VEHICLE)
		{
			if(PGTAMATERIALMGR->GetIsGlass(PGTAMATERIALMGR->GetMtlVfxGroup(pResult->GetHitMaterialId())) && ammoType != CAmmoInfo::FMJ)
			{
				return;
			}
			if(mati == m_MaterialIdCarBody)
			{
				materialSettings =  ((CVehicle *)pHitEntity)->GetVehicleAudioEntity()->GetCollisionAudio().GetVehicleCollisionMaterialSettings();
			}
		}
		if (!materialSettings)
		{
			
			materialSettings = g_audCollisionMaterials[mati];
			materialSettings = GetMaterialOverride(pHitEntity, materialSettings, pResult->GetHitComponent());
		}
#if __BANK
		if(g_PrintBulletImpactMaterial && materialSettings)
		{
			naDisplayf("map material: %s, component: %u", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings->NameTableOffset), pResult->GetHitComponent());
		}
		else if(g_PrintBulletImpactMaterial)
		{
			naDisplayf("Bullet impact material missing audio material settings: %s", PGTAMATERIALMGR->GetMaterial(pResult->GetHitMaterialId()).GetName());
		}
#endif

		if(materialSettings)
		{
#if __BANK 
			if( audPedFootStepAudio::sm_bMouseFootstepBullet)
			{
				audPedFootStepAudio::PlayDelayDebugFootstepEvent(pResult->GetHitPosition(),pResult->GetHitMaterialId(),pResult->GetHitComponent(),pHitEntity,materialSettings);
			}
			else if( audPedFootStepAudio::sm_bMouseFootstep)
			{
				audPedFootStepAudio::PlayDebugFootstepEvent(pResult->GetHitPosition(),pResult->GetHitMaterialId(),pResult->GetHitComponent(),pHitEntity,materialSettings);
				return;
			}
#endif
			Vector3 bulletPath = vBulletPath;
			bulletPath.Normalize();
			float fDotP = DotProduct(bulletPath, pResult->GetHitNormal());

#if __BANK //Allows us to turn off bullet impact sounds for easy collision testing
			if(!g_MeleeOnlyBulletImpacts && !doesMeleeImpacts)
#endif
			{

				if(fDotP < 0.0f)
				{
					// head on shots have a .35 chance of doing a ricochet, fully glancing ones 0.9
					f32 ricochetProbability = (1.f + fDotP) * sm_TimeBetweenShotsToRicoProb.CalculateValue(timeBetweenShots);
					if((AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_SHOULDPLAYRICOCHET)==AUD_TRISTATE_TRUE) &&
						(audEngineUtil::ResolveProbability(ricochetProbability) ||
						fwTimer::GetTimeWarp()<1.f))
					{
						if(!SUPERCONDUCTOR.GetGunFightConductor().GetFakeScenesConductor().IsFakingRicochets() BANK_ONLY(&&!g_DontPlayRicochets))
						{
							// trigger a ricochets_in at the bullet impact location
							Vector3 pos = pResult->GetHitPosition();
							PF_INCREMENT(audCollisionAudioEntity_Bullets);

							if (audWeaponAudioEntity::IsRubberBulletGun(weaponAudioHash) || audWeaponAudioEntity::IsRiotShotgun(weaponAudioHash))
							{
								audSoundSetFieldRef customSoundRef;								
								customSoundRef.Set(audWeaponAudioEntity::IsRubberBulletGun(weaponAudioHash) ? ATSTRINGHASH("Rubber_Bullet_Sounds", 0x50AB02CD) : ATSTRINGHASH("Riot_Shotgun_Sounds", 0x992CA88C), ATSTRINGHASH("Rico", 0x8398E83F));
								TriggerDeferredBulletImpact(customSoundRef, pos, impactCloserAttenuation, 1.f, playerShot, true, pHitEntity, pResult->GetHitMaterialId(), CAmmoInfo::SpecialType::None);
							}
							else if (audWeaponAudioEntity::IsRayGun(weaponAudioHash))
							{
								TriggerDeferredBulletImpact(ATSTRINGHASH("Laser_Bullets_Rico_Master", 0x57DC02A9), pos, impactCloserAttenuation, 1.f, playerShot, true, pHitEntity, pResult->GetHitMaterialId(), CAmmoInfo::SpecialType::None);
							}
							else
							{
								TriggerDeferredBulletImpact(ATSTRINGHASH("RICOCHETS_IN", 0x649832C8), pos, impactCloserAttenuation, 1.f, playerShot, true, pHitEntity, pResult->GetHitMaterialId(), CAmmoInfo::SpecialType::None);

								Vector3 pos2 = bulletPath;
								pos2.ReflectAbout(pResult->GetHitNormal());
								pos2.Scale(g_BulletRicochetPanDistance);
								pos2.Add(pResult->GetHitPosition());
								PF_INCREMENT(audCollisionAudioEntity_Bullets);
								TriggerDeferredBulletImpact(ATSTRINGHASH("RICOCHETS_OUT", 0x431F9671), pos2, impactCloserAttenuation, 1.f, playerShot, true, pHitEntity, pResult->GetHitMaterialId(), CAmmoInfo::SpecialType::None);
							}
						}
					}

					if(pHitEntity && pHitEntity->GetIsTypeVehicle() && ((CVehicle*)pHitEntity)->InheritsFromHeli() && cameraDistanceFromImpact > 30.f)
					{
						materialSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_CAR_METAL", 0xA0231769));
					}

					PF_INCREMENT(audCollisionAudioEntity_Bullets);

					bool useAutomatic = isAutomatic && !playerShot;
#if __BANK
					if(sm_AutomaticImpactOnlyOnVeh)
					{
						useAutomatic = useAutomatic && (pHitEntity && pHitEntity->GetIsTypeVehicle());
					}
#endif
					useAutomatic =  useAutomatic && (cameraDistanceFromImpact <= sm_DistanceToTriggerAutomaticBI);
					useAutomatic =  useAutomatic && (sm_TimeSinceLastAutomaticBulletHit >= sm_DtToTriggerAutomaticBI);

					bool useShotgun = isShotgun;

#if __BANK 
					if(sm_ShotgunDisabledForPlayer)
					{
						useShotgun = useShotgun && !playerShot;
					}
					if(sm_ShotgunImpactOnlyOnVeh)
					{
						useShotgun = useShotgun && (pHitEntity && pHitEntity->GetIsTypeVehicle());
					}
#endif

					useShotgun =  useShotgun && (cameraDistanceFromImpact <= sm_DistanceToTriggerShotgunBI);
					useShotgun =  useShotgun && (sm_TimeSinceLastShotgunBulletHit >= sm_DtToTriggerShotgunBI);

					u32 bulletImpactSound = useAutomatic ? materialSettings->AutomaticBulletImpactSound : materialSettings->BulletImpactSound;
					bulletImpactSound = useShotgun ? materialSettings->ShotgunBulletImpactSound : bulletImpactSound;

					u32 slowMoBulletImpactSound = useAutomatic ? materialSettings->SlowMoAutomaticBulletImpactSound : materialSettings->SlowMoBulletImpactSound;
					slowMoBulletImpactSound = useShotgun ? materialSettings->SlowMoShotgunBulletImpactSound : slowMoBulletImpactSound;

					u32 slowMoPresuckSound = useAutomatic ? materialSettings->SlowMoAutomaticBulletImpactPreSuck: materialSettings->SlowMoBulletImpactPreSuck;
					slowMoPresuckSound = useShotgun ? materialSettings->SlowMoShotgunBulletImpactPreSuck : slowMoPresuckSound;

					u32 slowMoPresuckTime = useAutomatic ? materialSettings->SlowMoAutomaticBulletImpactPreSuckTime: materialSettings->SlowMoBulletImpactPreSuckTime;
					slowMoPresuckTime = useShotgun ? materialSettings->SlowMoShotgunBulletImpactPreSuckTime : slowMoPresuckTime;

					if (bulletImpactSound == g_NullSoundHash)
					{
						bulletImpactSound = materialSettings->BulletImpactSound;
						slowMoBulletImpactSound = materialSettings->SlowMoBulletImpactSound;
						slowMoPresuckSound = materialSettings->SlowMoBulletImpactPreSuck;
						slowMoPresuckTime = materialSettings->SlowMoBulletImpactPreSuckTime;
						useAutomatic = false;
						useShotgun = false;
					}

#if __BANK
					if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && slowMoBulletImpactSound != g_NullSoundHash)
					{
						bulletImpactSound = slowMoBulletImpactSound;
					}
#endif

					if (audWeaponAudioEntity::IsRubberBulletGun(weaponAudioHash) || audWeaponAudioEntity::IsRiotShotgun(weaponAudioHash))
					{
						audSoundSet impactSoundSet;

						if (impactSoundSet.Init(audWeaponAudioEntity::IsRubberBulletGun(weaponAudioHash) ? ATSTRINGHASH("Rubber_Bullet_Sounds", 0x50AB02CD) : ATSTRINGHASH("Riot_Shotgun_Sounds", 0x992CA88C)))
						{
							audSoundSetFieldRef customSoundRef;

							if (pHitEntity && pHitEntity->GetIsTypeVehicle())
							{
								customSoundRef.Set(impactSoundSet, ATSTRINGHASH("Impact_Vehicle", 0xD6E48783));
							}
							else if (pHitEntity && pHitEntity->GetIsTypePed())
							{
								customSoundRef.Set(impactSoundSet, ATSTRINGHASH("Impact_Ped", 0x335EAE84));
							}
							else if (materialSettings->HardImpact != g_NullSoundHash)
							{
								customSoundRef.Set(impactSoundSet, ATSTRINGHASH("Impact_Hard", 0xE8CC836B));
							}
							else
							{
								customSoundRef.Set(impactSoundSet, ATSTRINGHASH("Impact_Soft", 0xB4DAE554));
							}

							TriggerDeferredBulletImpact(customSoundRef, pResult->GetHitPosition(), impactCloserAttenuation, 1.f, playerShot, false, pHitEntity, pResult->GetHitMaterialId(), ammoType, false REPLAY_ONLY(, slowMoBulletImpactSound));
						}
					}					
					else
					{
						//if we want the killshot gunfire to be different we need to play another sound here
						TriggerDeferredBulletImpact(bulletImpactSound, pResult->GetHitPosition(), impactCloserAttenuation, 1.f, playerShot, false, pHitEntity, pResult->GetHitMaterialId(), ammoType, false REPLAY_ONLY(, slowMoBulletImpactSound));

						if (audWeaponAudioEntity::IsRayGun(weaponAudioHash) && cameraDistanceFromImpact < sm_DistanceToTriggerRaygunBI)
						{
							TriggerDeferredBulletImpact(ATSTRINGHASH("Laser_Bullets_Impact_Master", 0xF438CF8F), pResult->GetHitPosition(), impactCloserAttenuation, 1.f, playerShot, false, pHitEntity, pResult->GetHitMaterialId(), ammoType, false REPLAY_ONLY(, slowMoBulletImpactSound));
						}
					}					

#if GTA_REPLAY
					if(slowMoPresuckSound != g_NullSoundHash)
					{
						CReplayMgr::ReplayPresuckSound(slowMoPresuckSound, slowMoPresuckTime, 0u, pHitEntity);
					}	
#endif
					
					if(useAutomatic)
					{
						sm_TimeSinceLastAutomaticBulletHit = 0;
					}
					if(useShotgun)
					{
						sm_TimeSinceLastShotgunBulletHit = 0;
					}
				}
			}


			// also trigger a melee impact
			if(materialSettings->BulletCollisionScaling > 0)
			{
				//f32 meleeImpactScale = s_MeleeImpactScaleFactor * (materialSettings->BulletCollisionScaling*0.01f);
				f32 meleeImpactScale = materialSettings->MaxImpulseMag * (materialSettings->BulletCollisionScaling*0.01f);
				TriggerDeferredMeleeImpact(pResult, meleeImpactScale, impactCloserImpulseDamping, true,playerShot);
			}
			// In case the material plays ricochet, send a message to the conductors in order to play it. 
			if(!doesMeleeImpacts && playerShot && (AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_SHOULDPLAYRICOCHET)==AUD_TRISTATE_TRUE) )
			{
				ConductorMessageData messageData;
				messageData.conductorName = GunfightConductor;
				messageData.message = FakeRicochet;
				messageData.entity = pHitEntity;
				naAssertf( FPIsFinite(pResult->GetHitPosition().x) && FPIsFinite(pResult->GetHitPosition().y) && FPIsFinite(pResult->GetHitPosition().z),
					"Bad hit position sent to the conductors.");
				messageData.vExtraInfo = pResult->GetHitPosition();
				SUPERCONDUCTOR.SendConductorMessage(messageData);
			}
			if(!doesMeleeImpacts && pHitEntity)
			{
				ConductorMessageData messageData;
				messageData.conductorName = GunfightConductor;
				messageData.entity = pHitEntity;
				naAssertf( FPIsFinite(pResult->GetHitPosition().x) && FPIsFinite(pResult->GetHitPosition().y) && FPIsFinite(pResult->GetHitPosition().z),
					"Bad hit position sent to the conductors.");
				messageData.vExtraInfo = pResult->GetHitPosition();
				CVehicle *vehicle = NULL;
				CPed *player = NULL;
				switch (pHitEntity->GetType())
				{
				case ENTITY_TYPE_BUILDING:
				case ENTITY_TYPE_ANIMATED_BUILDING:
					// First of all check if the material is flagged to play debris
					if(AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_GENERATEPOSTSHOOTOUTDEBRIS)==AUD_TRISTATE_TRUE)
					{
						player = CGameWorld::FindLocalPlayer();
						if(player)
						{
							if(cameraDistanceFromImpact <= SUPERCONDUCTOR.GetGunFightConductor().GetFakeScenesConductor().GetBuildingDistThresholdToPlayPostShootOut() || playerShot)
							{
								Vector3 groundNormal = player->GetGroundNormal();
								f32 dot = fabs(groundNormal.Dot(pResult->GetHitNormal()));
								if(dot <= 0.3f)
								{

									messageData.message = BuildingDebris;
									SUPERCONDUCTOR.SendConductorMessage(messageData);
								}
							}
						}
					}
					break;
				case ENTITY_TYPE_VEHICLE:
					vehicle = static_cast<CVehicle*>(pHitEntity);
					if(vehicle && vehicle->GetVehicleAudioEntity())
					{
						vehicle->GetVehicleAudioEntity()->UpdateBulletHits();
						messageData.message = VehicleDebris;
						SUPERCONDUCTOR.SendConductorMessage(messageData);
					}
					break;
				default:
					break;
				}
			}
		}
		else
		{
			naWarningf("Couldn't find audio material settings for bullet impact: %s", PGTAMATERIALMGR->GetMaterial(pResult->GetHitMaterialId()).GetName());
		}
	}
	sm_TimeSinceLastBulletHit = 0;
}

naEnvironmentGroup* audCollisionAudioEntity::CreateBulletImpactEnvironmentGroup(const Vec3V& position, CEntity* entity, const phMaterialMgr::Id matID)
{
	naEnvironmentGroup* environmentGroup = NULL;

	// Try grabbing an existing group or creating one off a CObject which will use portal-occlusion, otherwise just create a probe-based group
	environmentGroup = GetOcclusionGroup(position, entity, matID);
	if(!environmentGroup)
	{
		environmentGroup = naEnvironmentGroup::Allocate("BulletImpact");
		if(environmentGroup)
		{
			// SourceEnvironmentUpdateTime of 0 since we don't want to be doing lots of requests for all our groups
			environmentGroup->Init(NULL, 20, 0);
			environmentGroup->SetSource(position);
			environmentGroup->SetUseCloseProximityGroupsForUpdates(true);
			environmentGroup->SetUsePortalOcclusion(false);
		}
	}

	return environmentGroup;
}

naEnvironmentGroup* audCollisionAudioEntity::GetOcclusionGroup(const audCollisionEvent *collisionEvent)
{
	// First try and grab an already existing environmentGroup ( if it's a ped or vehicle )
	// occlusion groups don't have size or update rates any more, so just return one if either collision entity's got one.
	int index1 = 0, index2 = 1;

	//First person mode hack to make sure that collisions involving the player always get put in the player mixgroup. In the future perhaps this could be for all modes?
	if(collisionEvent->GetFlag(AUD_COLLISION_FLAG_PLAYER) && (!collisionEvent->entities[0] || !collisionEvent->entities[0]->GetIsTypePed() || !((CPed*)collisionEvent->entities[0].Get())->IsLocalPlayer()))
	{
		bool isFirstPerson = audNorthAudioEngine::IsAFirstPersonCameraActive(CGameWorld::FindLocalPlayer(), false);
		if(isFirstPerson)
		{
			index1 = 1;
			index2 = 0;
		}
	}
	naEnvironmentGroup* environmentGroup = NULL;
	environmentGroup = (naEnvironmentGroup*)(collisionEvent->entities[index1]?collisionEvent->entities[index1]->GetAudioEnvironmentGroup():NULL);
	if(environmentGroup)
	{
		return environmentGroup;
	}
	environmentGroup = (naEnvironmentGroup*)(collisionEvent->entities[index2]?collisionEvent->entities[index2]->GetAudioEnvironmentGroup():NULL);
	if(environmentGroup)
	{
		return environmentGroup;
	}

	// We don't already have one so create one
	environmentGroup = GetOcclusionGroup(VECTOR3_TO_VEC3V(collisionEvent->positions[0]), collisionEvent->entities[0], collisionEvent->materialIds[0]);
	if(environmentGroup)
	{
		return environmentGroup;
	}
	environmentGroup = GetOcclusionGroup(VECTOR3_TO_VEC3V(collisionEvent->positions[1]), collisionEvent->entities[1], collisionEvent->materialIds[1]);
	if(environmentGroup)
	{
		return environmentGroup;
	}

	return environmentGroup;
}

naEnvironmentGroup* audCollisionAudioEntity::GetOcclusionGroup(const Vec3V& position, CEntity *entity, const phMaterialMgr::Id matID)
{
	// Get the most appropriate occlusion group for this entity - if at all possible, don't create a new one, piggyback on the ones we've already got.
	naEnvironmentGroup* environmentGroup = NULL;

	if(entity)
	{
		switch(entity->GetType())
		{
		case ENTITY_TYPE_PED:
			environmentGroup = (naEnvironmentGroup*)(((CPed *)entity)->GetPedAudioEntity()->GetEnvironmentGroup(true));
			break;

		case ENTITY_TYPE_VEHICLE:
			environmentGroup = (naEnvironmentGroup*)(((CVehicle*)entity)->GetVehicleAudioEntity()->GetEnvironmentGroup());
			break;

		case ENTITY_TYPE_OBJECT:
		{
			environmentGroup = naEnvironmentGroup::Allocate("CollisionObj");
			if(environmentGroup)
			{
				// SourceEnvironmentUpdateTime of 0 since we don't want to be doing lots of requests for all our groups, and in interiors we get that when SetInteriorInfoWithInteriorLocation()
				environmentGroup->Init(NULL, 20, 0);
				environmentGroup->SetSource(position);
				environmentGroup->SetInteriorInfoWithEntity(entity);
				environmentGroup->SetUsePortalOcclusion(true);
				environmentGroup->SetMaxPathDepth(2);
				environmentGroup->SetUseCloseProximityGroupsForUpdates(true);
			}
			break;
		}
		case ENTITY_TYPE_MLO:
		{
			const s32 roomIdx = PGTAMATERIALMGR->UnpackRoomId(matID);
			if(roomIdx != 0)
			{
				const CInteriorProxy* intProxy = CInteriorProxy::GetInteriorProxyFromCollisionData(entity, NULL);
				if(intProxy)
				{
					environmentGroup = naEnvironmentGroup::Allocate("CollisionMLO");
					if(environmentGroup)
					{
						const fwInteriorLocation interiorLocation = CInteriorProxy::CreateLocation(intProxy, roomIdx);

						// SourceEnvironmentUpdateTime of 0 since we don't want to be doing lots of requests for all our groups, and in interiors we get that when SetInteriorInfoWithInteriorLocation()
						environmentGroup->Init(NULL, 20, 0);
						environmentGroup->SetSource(position);
						environmentGroup->SetInteriorInfoWithInteriorLocation(interiorLocation);
						environmentGroup->SetUsePortalOcclusion(true);
						environmentGroup->SetMaxPathDepth(2);
						environmentGroup->SetUseCloseProximityGroupsForUpdates(true);
					}
				}
			}
			break;
		}

		default:
			break;
		}
	}

	return environmentGroup;
}

bool audCollisionAudioEntity::ProcessImpactSounds(audCollisionEvent *collisionEvent, bool forceSoftImpact)
{
	Assert(collisionEvent);

	if((collisionEvent->entities[0] && collisionEvent->entities[0]->GetIsTypePed() && ((CPed*)collisionEvent->entities[0].Get())->IsLocalPlayer())
		|| (collisionEvent->entities[1] && collisionEvent->entities[1]->GetIsTypePed() && ((CPed*)collisionEvent->entities[1].Get())->IsLocalPlayer()))
	{
		collisionEvent->SetFlag(AUD_COLLISION_FLAG_PLAYER);
	}
	else
	{
		collisionEvent->ClearFlag(AUD_COLLISION_FLAG_PLAYER);
	}

	audEnvironmentGroupInterface *occlusionGroup = GetOcclusionGroup(collisionEvent);
	s32 numSoundsPlayed = 0;

	bool isInWater = false;

	// ped -> door collisions are special cased
	u32 doorIndex = ~0U;
	if(collisionEvent->entities[0] && collisionEvent->entities[1])
	{
		if(collisionEvent->entities[0]->GetType() == ENTITY_TYPE_OBJECT && !collisionEvent->entities[0]->IsBaseFlagSet(fwEntity::IS_FIXED) &&
			((CObject*)(collisionEvent->entities[0].Get()))->IsADoor() &&
			(((CDoor*)(collisionEvent->entities[0].Get()))->IsCreakingDoorType()))
		{
			if(collisionEvent->entities[1]->GetType() == ENTITY_TYPE_PED)
			{
				doorIndex = 0;
			}
		}
		else if(collisionEvent->entities[1]->GetType() == ENTITY_TYPE_OBJECT && !collisionEvent->entities[1]->IsBaseFlagSet(fwEntity::IS_FIXED) &&
			((CObject*)(collisionEvent->entities[1].Get()))->IsADoor() &&
			((((CDoor*)collisionEvent->entities[1].Get()))->IsCreakingDoorType()))
		{
			if(collisionEvent->entities[0]->GetType() == ENTITY_TYPE_PED)
			{
				doorIndex = 1;
			}
		}

		if(doorIndex != ~0U)
		{
			// we have a ped/door collision:
			// these are now handled int the door audio entity
		}
	}

	if(collisionEvent->entities[0] && collisionEvent->entities[0]->GetIsTypeObject())
	{
		// ignore weapon collisions
		if (collisionEvent->entities[0]->GetIsTypeObject())
		{
			CObject* object = static_cast<CObject*>(collisionEvent->entities[0].Get());
			if (object->GetWeapon())
			{
				if(!object->GetAsProjectile() && !object->GetWeapon()->GetAudioComponent().GetWasDropped())
				{
					return false;
				}
				object->GetWeapon()->GetAudioComponent().SetWasDropped(false);
				if(fwTimer::GetTimeInMilliseconds() - object->GetWeapon()->GetAudioComponent().GetLastImpactTime() < g_WeaponImpactFilterTime)
				{
					return false;
				}
				object->GetWeapon()->GetAudioComponent().SetLastImpactTime();
			}
		}
	}
	if(collisionEvent->entities[1] && collisionEvent->entities[1]->GetIsTypeObject())
	{
		// ignore weapon collisions
		if (collisionEvent->entities[1]->GetIsTypeObject())
		{
			CObject* object = static_cast<CObject*>(collisionEvent->entities[1].Get());
			if (object->GetWeapon())
			{
				if(!object->GetAsProjectile() && !object->GetWeapon()->GetAudioComponent().GetWasDropped())
				{
					return false;
				}
				object->GetWeapon()->GetAudioComponent().SetWasDropped(false);
				if(fwTimer::GetTimeInMilliseconds() - object->GetWeapon()->GetAudioComponent().GetLastImpactTime() < g_WeaponImpactFilterTime)
				{
					return false;
				}
				object->GetWeapon()->GetAudioComponent().SetLastImpactTime();
			}
			if(object->GetIsInWater())
			{
				isInWater = true;
			}
		}
	}

	for(s32 i=0; i<2; i++)
	{
		const audCategory* categoryToUse = NULL;

		CollisionMaterialSettings *materialSettings = collisionEvent->materialSettings[i];
		CollisionMaterialSettings *otherMaterialSettings = collisionEvent->materialSettings[(i + 1)&1];

		CEntity *entity = collisionEvent->entities[i];
		CEntity *otherEntity = collisionEvent->entities[(i+1)&1];

		bool isPed = entity && entity->GetIsTypePed();
		bool otherIsPed = otherEntity && otherEntity->GetIsTypePed();
		s16 requestedPitch = 0;

		CPed *ped = NULL;

		if(isPed)
		{
			ped = (CPed *)entity;
		}

		if(materialSettings == NULL)
		{
			continue;
		}

		//If the materials are the same, only play one collision (TODO:maybe we want to take account of weights too)
		if(numSoundsPlayed > 0 && materialSettings == otherMaterialSettings)
		{
			continue;
		}

		//Ignore collisions below the impulse mag threshold
		if(collisionEvent->impulseMagnitudes[i] < materialSettings->MinImpulseMag)
		{
#if __BANK
			if(g_AudioDebugEntity == entity && (g_DebugAudioImpacts || g_DebugAudioRolls || g_DebugAudioScrapes))
			{
				naDisplayf("[%u:%p] Ignoring impact due to impulse magnitude below min thresh: %f ", fwTimer::GetFrameCount(), collisionEvent, collisionEvent->impulseMagnitudes[i]);
			}
#endif
			continue;
		}


		// trigger ped impact sounds before earlying out due to silent impact since the ped stuff is free to scale impulses as it likes
		if(ped)
		{
			if(ped->GetPedAudioEntity()->GetIsInWater())  
			{
				ped->GetPedAudioEntity()->GetFootStepAudio().TriggerFallInWaterSplash(true);
				continue;
			}

			if(ped->GetPedType() != PEDTYPE_ANIMAL)
			{
				s32 otherIndex = (i+1)&1; 
				//naErrorf("Ped collision, material %s, impulseMag %f, component %d, time %d", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(collisionEvent->materialSettings[otherIndex]->NameTableOffset), collisionEvent->impulseMagnitudes[i], collisionEvent->components[i], fwTimer::GetTimeInMilliseconds()); 
				if(!ped->GetUsingRagdoll())
				{
					continue;
				}

				ped->GetPedAudioEntity()->TriggerImpactSounds(collisionEvent->positions[i],
					collisionEvent->materialSettings[otherIndex],
					collisionEvent->entities[otherIndex], 
					collisionEvent->impulseMagnitudes[i], 
					collisionEvent->components[i], 
					collisionEvent->components[otherIndex],
					collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE));	
				if(ped->GetPedAudioEntity()->ShouldUpBodyfallSoundsFromShooting())
				{
					categoryToUse = g_UppedCollisionCategory;
				}
			}				


			if(collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE) && collisionEvent->entities[1] && collisionEvent->entities[1]->GetType() == ENTITY_TYPE_PED)
			{
				CPed *hittingPed = (CPed*)(collisionEvent->entities[1].Get());
				CPedModelInfo *pedModelInfo = (CPedModelInfo*)hittingPed->GetBaseModelInfo();
				if(pedModelInfo)
				{
					if(BANK_ONLY(g_PedsHitLikeGirls || )!hittingPed->IsMale())
					{
						requestedPitch = 600;
					}
				}
			}			
		}


		if( doorIndex != ~0U && !collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE)) 
		{
			break;
		}
		f32 volume;
		u32 startOffsetPerc;

		ComputeStartOffsetAndVolumeFromImpulseMagnitude(collisionEvent, i, startOffsetPerc, volume, isPed);

		if(volume <= g_SilenceVolume || startOffsetPerc >= 100)
		{
#if __BANK
			if(g_AudioDebugEntity == collisionEvent->entities[i] && (g_DebugAudioImpacts || g_DebugAudioRolls || g_DebugAudioScrapes))
			{
				naDisplayf("[%u:%p] Ignoring silent impact: vel: %f vol: %f", fwTimer::GetFrameCount(), collisionEvent, collisionEvent->impulseMagnitudes[i], volume);
			}
#endif
			continue;
		}

		if (materialSettings->CollisionCount >= materialSettings->CollisionCountThreshold)
		{
			if(volume > materialSettings->VolumeThreshold)
			{
				materialSettings->VolumeThreshold = volume;
			}
			else
			{
				//naErrorf("Culling collision for %s at volume %f, collision count %u", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings->NameTableOffset), volume, materialSettings->CollisionCount);
				continue;
			}
		}

		//naErrorf("Impact on material %s, collision count %u", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings->NameTableOffset), materialSettings->CollisionCount); 

		//If a ped is involved in the collision, let the ped code work out what wants to be played
		if(( collisionEvent->entities[0] && collisionEvent->entities[0]->GetIsTypePed() ) && ( collisionEvent->entities[1] && collisionEvent->entities[1]->GetIsTypePed()))
		{
			continue;
		}

		if(collisionEvent->entities[0] && collisionEvent->entities[i]->GetIsTypeVehicle() && ((CVehicle*)collisionEvent->entities[i].Get())->GetVehicleAudioEntity() && !collisionEvent->GetFlag(AUD_COLLISION_FLAG_BULLET) && !collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE))
		{
			CVehicle* vehicle = (CVehicle*)collisionEvent->entities[i].Get();
				
			if(vehicle && !(vehicle->GetVehicleAudioEntity()->GetDrowningFactor() >= 1.f && !Water::IsCameraUnderwater()))
			{
				f32 wreckedVehicleImpactScale = g_WreckedVehicleImpactScale;
				f32 wreckedVehicleRolloffScale = g_WreckedVehicleRolloffScale;
				vehicle->GetVehicleAudioEntity()->GetCollisionAudio().Fakeimpact(collisionEvent->impulseMagnitudes[i]*wreckedVehicleImpactScale, wreckedVehicleRolloffScale, true);
			}
		}
		
#if __BANK
		DebugAudioImpact(collisionEvent->positions[0], collisionEvent->components[0], collisionEvent->components[1], collisionEvent->impulseMagnitudes[0], (audCollisionType)collisionEvent->type);
#endif // #if __BANK

		u32 hardImpact = materialSettings->HardImpact; 
		u32 hardImpactSlowMo = materialSettings->SlowMoHardImpact; 

		u32 solidImpact = materialSettings->SolidImpact;
		u32 softImpact = materialSettings->SoftImpact;

		u32 otherHardImpact = otherMaterialSettings? otherMaterialSettings->HardImpact: materialSettings->HardImpact;
		u32 otherHardImpactSlowMo = otherMaterialSettings? otherMaterialSettings->SlowMoHardImpact: materialSettings->SlowMoHardImpact;
		u32 otherSolidImpact = otherMaterialSettings? otherMaterialSettings->SolidImpact: materialSettings->SolidImpact;

		//naErrorf("Playing impact sound from material %s volume %f", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(collisionEvent->materialSettings[i]->NameTableOffset), volume); 

		bool pedsMakeSolidCollisions = g_PedsMakeSolidCollisions || AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_PEDSARESOLID) == AUD_TRISTATE_TRUE;
		
		if(hardImpact == g_NullSoundHash || (isPed && !collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE)))
		{
			hardImpact = 0;
			hardImpactSlowMo = g_NullSoundHash;
		}
		if(solidImpact == g_NullSoundHash)
		{
			solidImpact = 0;
		}
		if(otherHardImpact == g_NullSoundHash || (otherIsPed && !collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE)))
		{
			otherHardImpact = 0;
			otherHardImpactSlowMo = g_NullSoundHash;
		}
		if(otherSolidImpact == g_NullSoundHash && ((!otherIsPed || !pedsMakeSolidCollisions) && !collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE)))
		{
			otherSolidImpact = 0;
		}

		if(otherMaterialSettings && AUD_GET_TRISTATE_VALUE(otherMaterialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_ISSOFTFORLIGHTPROPS) == AUD_TRISTATE_TRUE && 
			GetAudioWeight(collisionEvent->entities[i]) <= AUDIO_WEIGHT_L)
		{
			forceSoftImpact = true;
		}

		if(forceSoftImpact)
		{
			otherHardImpact = 0;
			otherHardImpactSlowMo = g_NullSoundHash;
			otherSolidImpact = 0;
		}

		if(collisionEvent->GetFlag(AUD_COLLISION_FLAG_PUNCH) || collisionEvent->GetFlag(AUD_COLLISION_FLAG_KICK))
		{
			if(AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_PEDSARESOLID) != AUD_TRISTATE_TRUE)
			{
				otherHardImpact = 0;
				otherHardImpactSlowMo = g_NullSoundHash;
				otherSolidImpact = 0;
				if(collisionEvent->GetFlag(AUD_COLLISION_FLAG_PUNCH))
				{
					softImpact = collisionEvent->materialSettings[0]->PedPunch;
				}
				else //kick
				{
					softImpact = collisionEvent->materialSettings[0]->PedKick;
				}
			}
		}

		if(collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE) && collisionEvent->GetFlag(AUD_COLLISION_FLAG_PLAYER) && collisionEvent->entities[0] && collisionEvent->entities[0]->GetIsTypeVehicle())
		{
			bool isFirstPerson = audNorthAudioEngine::IsAFirstPersonCameraActive(CGameWorld::FindLocalPlayer(), false);
			if(isFirstPerson)
			{
				volume += g_PlayerVehicleMeleeCollisionVolumeScale;
			}
		}

		audSoundInitParams initParams;
		initParams.EnvironmentGroup = occlusionGroup;
		if(categoryToUse)
		{
				initParams.Category = categoryToUse;
		}
		initParams.StartOffset = startOffsetPerc;
		initParams.IsStartOffsetPercentage = true;
		initParams.Volume = volume;

		if(collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE) && collisionEvent->GetFlag(AUD_COLLISION_FLAG_PLAYER) && !collisionEvent->GetFlag(AUD_COLLISION_FLAG_FOOTSTEP))
		{
			bool isFirstPerson = audNorthAudioEngine::IsAFirstPersonCameraActive(CGameWorld::FindLocalPlayer(), false);
			if(isFirstPerson)
			{
				initParams.Volume += g_PlayerMeleeCollisionVolumeBoost;
				initParams.Category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("MELEE", 0x3B8F078A)); 
			}
		}
		
		if(collisionEvent->rollOffScale != 0.f)
		{
			// value of 0 means default
			initParams.VolumeCurveScale = collisionEvent->rollOffScale;
		}

		/*if(isPed)
		{
		naErrorf("Playing ped impact sounds volume %f, is using upped category: %s, time %d", volume, categoryToUse ? "true" : "false", fwTimer::GetTimeInMilliseconds());
		}*/

		initParams.Predelay = (u32)collisionEvent->predelay;
		initParams.Position = collisionEvent->positions[i];
		initParams.Pitch = requestedPitch;

		if(isInWater)
		{
			initParams.LPFCutoff = g_LFPCuttoffInWater;
			initParams.Volume -= g_VolumeReductionInWater;
		}

		u32 impactHash = 0;
		u32 slowMoImpactHash = g_NullSoundHash;
	
		if(otherHardImpact) //Only play hard impacts if both materials are hard
		{
			if(hardImpact)
			{
				impactHash = hardImpact;
				slowMoImpactHash = hardImpactSlowMo;
			}
			else if(solidImpact)
			{
				impactHash = solidImpact;
			}
		}
		else if(solidImpact && otherSolidImpact)
		{
			impactHash = solidImpact;
		}

		if(impactHash != 0)
		{
#if __BANK
			if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && slowMoImpactHash != g_NullSoundHash)
			{
				impactHash = slowMoImpactHash;
			}
#endif

			CreateAndPlaySound(impactHash, &initParams);

			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(impactHash, &initParams, NULL, NULL, eCollisionAudioEntity, slowMoImpactHash));
		}
		CreateAndPlaySound(softImpact, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(softImpact, &initParams, NULL, NULL, eCollisionAudioEntity));

		audPlayerCollisionEvent * playerEvent = GetPlayerCollisionEventFromHistory(entity);
		if(playerEvent)
		{
			playerEvent->lastImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
			collisionEvent->rollOffScale = sm_PlayerContextCollisionRolloff;
			if(volume > sm_PlayerContextVolumeForBlip)
			{
				playerEvent->blipMap = true;
				playerEvent->blipAmount = audDriverUtil::ComputeLinearVolumeFromDb(volume);
			}
			playerEvent->position = VECTOR3_TO_VEC3V(collisionEvent->positions[i]);
		}
			
		numSoundsPlayed++;

		if(collisionEvent->materialSettings[i] && g_UseCollisionIntensityCulling)
		{
			if(!g_RecentlyCollidedMaterials.IsMemberOfList(collisionEvent->materialSettings[i]))
			{
				g_RecentlyCollidedMaterials.Add(collisionEvent->materialSettings[i]);
			}
			collisionEvent->materialSettings[i]->CollisionCount++;
		}

#if __BANK
		PF_INCREMENT(CollisonsProcessed);
#endif

	}
	return (numSoundsPlayed > 0);
}

bool audCollisionAudioEntity::ProcessScrapeSounds(audCollisionEvent *collisionEvent)
{
	if(( collisionEvent->entities[0] && collisionEvent->entities[0]->GetIsTypePed() && (collisionEvent->components[0] == RAGDOLL_FOOT_LEFT || collisionEvent->components[0] == RAGDOLL_FOOT_RIGHT)) || ( collisionEvent->entities[1] && collisionEvent->entities[1]->GetIsTypePed()  && (collisionEvent->components[1] == RAGDOLL_FOOT_LEFT || collisionEvent->components[1] == RAGDOLL_FOOT_RIGHT)))
	{
		return false;
	}

	// If we're not playing any scrape sounds, kick them off
	if (!collisionEvent->scrapeSounds[0] && !collisionEvent->scrapeSounds[1])
	{
		collisionEvent->SetFlag(AUD_COLLISION_FLAG_PLAY_SCRAPE);
	}
	return true;
}

bool audCollisionAudioEntity::ProcessRollSounds(audCollisionEvent *collisionEvent)
{
	//Peds don't roll...unless we want to do bespoke ped stuff in which case this needs to be moved lower down
	if(( collisionEvent->entities[0] && collisionEvent->entities[0]->GetIsTypePed() ) || ( collisionEvent->entities[1] && collisionEvent->entities[1]->GetIsTypePed()))
	{
		return false;
	}

	if (!collisionEvent->rollSounds[0] && !collisionEvent->rollSounds[1])
	{
		collisionEvent->SetFlag(AUD_COLLISION_FLAG_PLAY_ROLL);
	}

	return true;
}

bool audCollisionAudioEntity::IsScrapeFromMagnitudes(const f32 bangMag, const f32 scrapeMag)
{
	// find out whether this predominantly a bang or a scrape and re-adjust the data accordingly
	const float bangScrapeBias = 0.f; 
	float bangRatio = bangMag / (bangMag+scrapeMag);
	float scrapeRatio = scrapeMag / (bangMag+scrapeMag);
	return scrapeRatio>= (bangRatio+bangScrapeBias);
}



bool audCollisionAudioEntity::IsOkToPlayImpacts(phManifold & manifold)
{
	bool okToPlayImpactSounds = true;
	
	//Filter out collisions that happen too frequently (e.g. the jiggling when objects are precariously balanced)
	u32 lastCollision = GetManifoldUserDataTime(manifold);
	u32 currentTime = (u16)(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0)&0xffff);

	SetManifoldUserDataTime(manifold, (u16)currentTime);
	if((s32(currentTime) - s32(lastCollision)) < 0)
	{
		currentTime += 0xffff;
	}

	u32 timeDelta = currentTime-lastCollision;
	if(timeDelta < g_MinTimeBetweenCollisions)
	{
		okToPlayImpactSounds = false;
	}

	return okToPlayImpactSounds; 
}

void audCollisionAudioEntity::UpdateManifold(VfxCollisionInfo_s & vfxColnInfo, phManifold & manifold,bool breakableCollisionA,bool breakableCollisionB)
{
#if __BANK
	if(g_DisableCollisionAudio)
	{
		return;
	}
#endif
	// First of all check if its a glass event and let the glassAudioEntity manage it.
	if(breakableCollisionB)
	{
		g_GlassAudioEntity.PlayShardCollision(vfxColnInfo.pEntityA,VEC3V_TO_VECTOR3(vfxColnInfo.vRelVelocity).Mag(),manifold.GetInstanceB());
		return;
	}
	else if(breakableCollisionA)
	{
		g_GlassAudioEntity.PlayShardCollision(vfxColnInfo.pEntityB,VEC3V_TO_VECTOR3(vfxColnInfo.vRelVelocity).Mag(),manifold.GetInstanceA());
		return;
	}

	CEntity* pEntityA = vfxColnInfo.pEntityA;
	CEntity* pEntityB = vfxColnInfo.pEntityB;

	// return if either entity isn't valid
	if (vfxColnInfo.pEntityA==NULL || vfxColnInfo.pEntityB==NULL)
	{
		return;
	}

	// check that entity a is a physical object (b may or may not be)
	Assertf(vfxColnInfo.pEntityA->GetIsPhysical() || vfxColnInfo.pEntityB->GetIsPhysical(), "Neither entity is physical (A:%s - B:%s)", vfxColnInfo.pEntityA->GetModelName(), vfxColnInfo.pEntityB->GetModelName());

	// if these entities are attached then ignore them
	if (pEntityA->GetIsPhysical())
	{
		CPhysical* pPhysicalA = static_cast<CPhysical*>(pEntityA);
		if (pPhysicalA->GetAttachParent()==pEntityB)
		{
			return ;
		}
	}

	if (pEntityB->GetIsPhysical())
	{
		CPhysical* pPhysicalB = static_cast<CPhysical*>(pEntityB);
		if (pPhysicalB->GetAttachParent()==pEntityA)
		{
			return ;
		}
	}

	// return if we have two invalid colliders
	if (vfxColnInfo.pColliderA==NULL && vfxColnInfo.pColliderB==NULL)
	{
		return;
	}

	// ignore collisions between different parts of the same ped
	if (pEntityA==pEntityB && pEntityA->GetIsTypePed())
	{
		return ;
	}

	// find out whether this predominantly a bang or a scrape and re-adjust the data accordingly
	float bangMag = vfxColnInfo.bangMagA;
	float scrapeMag = vfxColnInfo.scrapeMagA;

	if(bangMag > 1000.f) //Bad physics
	{
		//naErrorf("Crazy impact magnitude: %f", bangMag);
		return;
	}

	bool isAPedCollision = false, isPedRagdollA=false, isPedRagdollB=false;

	//We set up the materials early as we need access to them to make decisions about rolling
	phMaterialMgr::Id materialIds[2];
	materialIds[0] = vfxColnInfo.mtlIdA;
	materialIds[1] = vfxColnInfo.mtlIdB;

	CollisionMaterialSettings * materialSettings[2];

	for(int i=0; i<2; i++)
	{		
		CEntity * entity = (i==0) ? pEntityA:pEntityB;
		u32 component = (i==0) ?  vfxColnInfo.componentIdA:vfxColnInfo.componentIdB;
		// extract the material data from the collision and see if we have a model specific override
		phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(materialIds[i]);
		materialSettings[i] = GetMaterialOverride(entity, g_audCollisionMaterials[(s32)unpackedMtlIdA], component);

	}	


	//Melee collisions are handled seperately
	if(pEntityA->GetIsTypePed())
	{
		isAPedCollision = true;
		CPed* ped = (CPed*)pEntityA;
		if(pEntityB->GetIsTypeVehicle() && (pEntityB == ped->GetVehiclePedEntering() || pEntityB == ped->GetVehiclePedInside()))
		{
			return;
		}
		if(ped->GetPedIntelligence() && ped->GetPedIntelligence()->GetTaskMelee())
		{
			return;
		}
		if(CVfxHelper::GetGameCamWaterDepth()>0.0f)
		{
			return;
		}
		isPedRagdollA = ped->GetUsingRagdoll();
		if(!isPedRagdollA && !pEntityB->GetIsTypePed() && g_PedsOverrideImpactMag)
		{
			bangMag = ped->GetPedAudioEntity()->GetPlayerImpactMagOverride(bangMag, pEntityB, materialSettings[1]);
		}
		if(!isPedRagdollA && pEntityB->GetIsTypeObject() && ((CObject*)pEntityB)->IsADoor())
		{
			if(((CDoor*)pEntityB)->GetDoorAudioEntity())
			{
				((CDoor*)pEntityB)->GetDoorAudioEntity()->TriggerDoorImpact(ped, vfxColnInfo.vWorldPosA);
				return;
			}
		}
	}
	if(pEntityB->GetIsTypePed())
	{
		isAPedCollision = true;
		CPed* ped = (CPed*)pEntityB;
		if(pEntityA->GetIsTypeVehicle() && (pEntityA == ped->GetVehiclePedEntering() || pEntityA == ped->GetVehiclePedInside()))
		{
			return;
		}
		if(ped->GetPedIntelligence() && ped->GetPedIntelligence()->GetTaskMelee())
		{
			return;
		}
		if(CVfxHelper::GetGameCamWaterDepth()>0.0f)
		{
			return;
		}
		isPedRagdollB = ped->GetUsingRagdoll();
		if(!isPedRagdollB && ! pEntityA->GetIsTypePed() && g_PedsOverrideImpactMag)
		{
			bangMag = ped->GetPedAudioEntity()->GetPlayerImpactMagOverride(bangMag, pEntityB, materialSettings[0]);
		}
		if(!isPedRagdollB && pEntityA->GetIsTypeObject() && ((CObject*)pEntityA)->IsADoor())
		{
			if(((CDoor*)pEntityA)->GetDoorAudioEntity())
			{
				((CDoor*)pEntityA)->GetDoorAudioEntity()->TriggerDoorImpact(ped, vfxColnInfo.vWorldPosA);
				return;
			}
		}
	}

	// store some more useful information
	phInst* pInstA = pEntityA->GetFragInst();
	phInst* pInstB = pEntityB->GetFragInst();

	if(pInstA == NULL)
	{
		pInstA = pEntityA->GetCurrentPhysicsInst();
	}
	if(pInstB == NULL)
	{
		pInstB = pEntityB->GetCurrentPhysicsInst();
	}

	// return early if the data is no longer valid
	if (pInstA==NULL || pInstA->IsInLevel()==false ||
		pInstB==NULL || pInstB->IsInLevel()==false)
	{
		return ;
	}

	//See if we want to pass this contact to the vehicle entity to handle
	bool isVehicleCollisionAndBail = false;

	if(pEntityA->GetIsTypeVehicle() && !(((CVehicle*)pEntityA)->GetHealth() <= 0.f ))
	{
		if(!((CVehicle*)pEntityA)->GetVehicleAudioEntity()->IsDisabled()) 
		{
			audVehicleCollisionEvent vehicleCollision(vfxColnInfo, true);

			((CVehicle*)pEntityA)->GetVehicleAudioEntity()->GetCollisionAudio().ProcessContact(vfxColnInfo, manifold, pEntityB, vehicleCollision);
			isVehicleCollisionAndBail = true;

			// this allows very heavy objects to bang inside vehicles(crates inside titan in exile1)
			if(GetAudioWeight(pEntityB) == AUDIO_WEIGHT_VH && pEntityB->GetType() == ENTITY_TYPE_OBJECT)
			{
				isVehicleCollisionAndBail = false;
			}
		}
	}

	if(pEntityB->GetIsTypeVehicle() && !(((CVehicle*)pEntityB)->GetHealth() <= 0.f))
	{
		if(!((CVehicle*)pEntityB)->GetVehicleAudioEntity()->IsDisabled()) 
		{
			audVehicleCollisionEvent vehicleCollision(vfxColnInfo, false);

			((CVehicle*)pEntityB)->GetVehicleAudioEntity()->GetCollisionAudio().ProcessContact(vfxColnInfo, manifold, pEntityA, vehicleCollision);
			isVehicleCollisionAndBail = true;	

			// this allows very heavy objects to bang inside vehicles(crates inside titan in exile1)
			if(GetAudioWeight(pEntityA) == AUDIO_WEIGHT_VH && pEntityA->GetType() == ENTITY_TYPE_OBJECT)
			{
				isVehicleCollisionAndBail = false;
			}
		}
	}

	if(isVehicleCollisionAndBail)
	{
		return; //Vehicle collisions are handled in the vehicleaudioentity
	}

	HandleEntitySwinging(pEntityA);
	HandleEntitySwinging(pEntityB);

	// extract the collison normal information
	const Vec3V normalA = vfxColnInfo.vWorldNormalA; 
	// Vector3	normalB = VEC3V_TO_VECTOR3(vfxColnInfo.vWorldNormalB);

#if __DEV
	if (g_DebugAudioCollisions && g_AudioDebugEntity && (g_AudioDebugEntity==pEntityA || g_AudioDebugEntity==pEntityB))
	{
		naAssertf(0, "Debugging collisions");
	}
#endif
	
	//Work out the relative linear and angular speeds of the entities involved in the collision
	ScalarV turnSpeed(V_ZERO);
	if(pEntityA->GetIsPhysical())
	{
		const Vec3V turnVector = RCC_VEC3V(((CPhysical *)pEntityA)->GetAngVelocity());
		const ScalarV turnDotNorm = Dot(turnVector,normalA);
		turnSpeed = rage::SqrtSafe(MagSquared(turnVector) - turnDotNorm*turnDotNorm);
	}

	ScalarV otherTurnSpeed(V_ZERO);
	if(pEntityB->GetIsPhysical())
	{
		const Vec3V turnVector = RCC_VEC3V(((CPhysical *)pEntityB)->GetAngVelocity());
		const ScalarV turnDotNorm = Dot(turnVector,normalA);
		otherTurnSpeed = rage::SqrtSafe(MagSquared(turnVector) - turnDotNorm*turnDotNorm); 
	} 

	//Get the component of the velocities involved in the direction of the collision, 
	//not sure if this makes complete mathematical sense with regards to the angular
	//velocity but works ok so we'll go with it for now
	const ScalarV relativeMoveSpeed = Abs(Dot(vfxColnInfo.vRelVelocity, vfxColnInfo.vColnDir));//Mag(vfxColnInfo.vRelVelocity).Getf(); //Abs((moveSpeed - otherMoveSpeed).Dot(normalA)); 
	const ScalarV relativeTurnSpeed = Abs(turnSpeed+otherTurnSpeed);  //Abs((turnSpeed - otherTurnSpeed).Mag()); //.Dot(normalA)); //Abs((Dot(VECTOR3_TO_VEC3V(turnSpeed), vfxColnInfo.vColnDir) - Dot(VECTOR3_TO_VEC3V(otherTurnSpeed), vfxColnInfo.vColnDir)).Getf()); 
	const ScalarV relativeSpeed = Max(Max(relativeMoveSpeed,
		relativeTurnSpeed), ScalarVFromF32(bangMag));

	//Don't do collision sounds if there is not a big enough velocity involved
	if(IsGreaterThanAll(relativeSpeed,ScalarVFromF32(g_MinSpeedForCollision)))
	{
		float fRelativeTurnSpeed = relativeTurnSpeed.Getf();

		// extract the collision position
		Vector3 collPosA = VEC3V_TO_VECTOR3(vfxColnInfo.vWorldPosA);
		Vector3 collPosB = VEC3V_TO_VECTOR3(vfxColnInfo.vWorldPosB);

		if (!IsScrapeFromMagnitudes(bangMag, scrapeMag))
		{
			scrapeMag = 0.0f;
		}

		//Create a new event and an event pointer in case we want to use one from the history instead
		audCollisionEvent newEvent;
		newEvent.Init();
		audCollisionEvent *collisionEvent = NULL;
		bool hasFoundEvent = false;
		bool shouldFlipEntities = false;
		bool isRolling = false;

		//See if we have a rolling contact
		if((manifold.IsStaticFriction() && fRelativeTurnSpeed > g_MinRollSpeed && fRelativeTurnSpeed > scrapeMag) || (scrapeMag ==0.f && fRelativeTurnSpeed > 1.f))
		{
			isRolling = true;
		}

		//We set up the materials early as we need access to them to make decisions about rolling
		phMaterialMgr::Id materialIds[2];
		materialIds[0] = vfxColnInfo.mtlIdA;
		materialIds[1] = vfxColnInfo.mtlIdB;

		CollisionMaterialSettings * materialSettings[2];

		for(int i=0; i<2; i++)
		{		
			CEntity * entity = (i==0) ? pEntityA:pEntityB;
			u32 component = (i==0) ?  vfxColnInfo.componentIdA:vfxColnInfo.componentIdB;
			// extract the material data from the collision and see if we have a model specific override
			phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(materialIds[i]);
			materialSettings[i] = GetMaterialOverride(entity, g_audCollisionMaterials[(s32)unpackedMtlIdA], component);

		}	

		for(int i=0; i<2; i++)
		{
			if((materialSettings[i] && (fRelativeTurnSpeed < materialSettings[i]->MinRollSpeed)))
			{
				isRolling = false;
			}
		}


		if(isRolling && isAPedCollision)
		{
			fRelativeTurnSpeed = 0.f;
			isRolling = false;
		}

		//Rolling and scraping collisions use the history
		if((scrapeMag > 0.f) || isRolling)
		{
			collisionEvent = GetCollisionEventFromHistory(manifold); 

			if(collisionEvent)
			{
				const bool areEntities00Matched = (collisionEvent->entities[0] == pEntityA); // && collisionEvent->components[0] == manifold.GetComponentA());
				const bool areEntities11Matched = (collisionEvent->entities[1] == pEntityB); // && collisionEvent->components[1] == manifold.GetComponentB());
				const bool areEntities01Matched = (collisionEvent->entities[0] == pEntityB); // && collisionEvent->components[0] == manifold.GetComponentB());
				const bool areEntities10Matched = (collisionEvent->entities[1] == pEntityA); // && collisionEvent->components[1] == manifold.GetComponentA());
			    
				if(areEntities00Matched && areEntities11Matched)
				{
					hasFoundEvent = true;
				}
				else if(areEntities01Matched && areEntities10Matched)
				{
					hasFoundEvent = true;
					shouldFlipEntities = true; //found the event but the entities were the other way around last time so give them a flip
				}
			}

			if(!hasFoundEvent)
			{
				u16 eventIndex;
				collisionEvent = GetRollCollisionEventFromHistory(pEntityA, pEntityB, &eventIndex);
				if(collisionEvent)
				{
					SetManifoldUserDataIndex(manifold, eventIndex);
					hasFoundEvent = true;
				}
			}

			if(collisionEvent)
			{
				if((!pEntityA || pEntityA == collisionEvent->entities[1]) && (!pEntityB || pEntityB == collisionEvent->entities[0]))
				{
					shouldFlipEntities = true;
				}
			}

			if(!hasFoundEvent)
			{
				//naErrorf("New scrape sound time %d on materials %s and %s, entities %p, %p  isRoll: %s", audNorthAudioEngine::GetCurrentTimeInMs(), audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings[0]->NameTableOffset), audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings[1]->NameTableOffset), pEntityA, pEntityB, isRolling?"true":"false"); 
			
				u16 eventIndex;
				//We don't already have an event for collisions between these entities, so create a new one.
				collisionEvent = GetFreeCollisionEventFromHistory(&eventIndex);
					if(collisionEvent == NULL)
				{
	    				//naWarningf("The audio collision event history is full!");
						return;
				}
				SetManifoldUserDataIndex(manifold, eventIndex);

				collisionEvent->entities[0] = pEntityA;
				collisionEvent->entities[1] = pEntityB;
				collisionEvent->components[0] = (u8)vfxColnInfo.componentIdA;
				collisionEvent->components[1] = (u8)vfxColnInfo.componentIdB;
			}

			collisionEvent->positions[0] = collPosA;
			collisionEvent->positions[1] = collPosB;

			if(isRolling)
			{
				collisionEvent->type = AUD_COLLISION_TYPE_ROLL;
			}
			else
			{
				collisionEvent->type = AUD_COLLISION_TYPE_SCRAPE;
			}
		}
		else //It's not a scrape or a roll so do a one shot collision event without using the history
		{
			if(bangMag < 0.01f)
			{
				return;
			}
			collisionEvent = &newEvent;
			collisionEvent->type = AUD_COLLISION_TYPE_IMPACT; 

			collisionEvent->entities[0] = pEntityA;
			collisionEvent->entities[1] = pEntityB;
			collisionEvent->positions[0] = collPosA;
			collisionEvent->positions[1] = collPosB;
			collisionEvent->components[0] = (u8)vfxColnInfo.componentIdA;
			collisionEvent->components[1] = (u8)vfxColnInfo.componentIdB;
		}


	#if __BANK
		if(g_DebugFocusEntity && (pEntityA == g_pFocusEntity || pEntityB == g_pFocusEntity))
		{
			naErrorf("bangMag %f, scrapeMag %f, relativeTurnSpeed %f", bangMag, scrapeMag, fRelativeTurnSpeed);
		}
	#endif

		bool okToPlayImpactsounds = (collisionEvent->type == AUD_COLLISION_TYPE_IMPACT) ? IsOkToPlayImpacts(manifold) : false;

		collisionEvent->lastImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

		//TODO: choose between relativeMoveSpeed and bangMag 
		if(PopulateAudioCollisionEventFromImpact(collisionEvent, vfxColnInfo, bangMag, scrapeMag, fRelativeTurnSpeed, shouldFlipEntities))
		{
			audSoundInitParams initParams; //for small scrape/roll impacts

			switch(collisionEvent->type)
			{
				case AUD_COLLISION_TYPE_IMPACT:        
					if(okToPlayImpactsounds)
					{
						ProcessImpactSounds(collisionEvent);
					}
					break;

				case AUD_COLLISION_TYPE_SCRAPE:

					if(isAPedCollision)
					{
						for(int i=0; i<2; i++)
						{
							if(collisionEvent->entities[i] && collisionEvent->entities[i]->GetIsTypePed())
							{
								audPedAudioEntity * pedAudio = ((CPed*)collisionEvent->entities[i].Get())->GetPedAudioEntity();
								if(g_DoPedScrapeStartImpacts && (g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - pedAudio->GetLastScrapeTime()) > g_PedScrapeStartImpactTime)
								{
									float volume= 0.f;
									s32 pitch = 0;
									ComputePitchAndVolumeFromScrapeSpeed(collisionEvent, 0, pitch, volume);
									pedAudio->TriggerScrapeStartImpact(volume);
									okToPlayImpactsounds = false;
								}
								pedAudio->SetLastScrapeTime();
							}
						}
						
					}

					if(ShouldPlayScrapeImpact(collisionEvent, bangMag) && IsOkToPlayImpacts(manifold)) //Play big collisions in addition to scraping
					{
						ProcessImpactSounds(collisionEvent);
					}

					if(!isAPedCollision || isPedRagdollA || isPedRagdollB)
					{
						ProcessScrapeSounds(collisionEvent);

						collisionEvent->scrapeStopTimeMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + g_ScrapingCollisionHoldTimeMs;
					}

					break;
				case AUD_COLLISION_TYPE_ROLL:
					collisionEvent->rollUpdatedThisFrame = 1;
					if(ShouldPlayRollImpact(collisionEvent, bangMag) && IsOkToPlayImpacts(manifold)) //Play big collisions in addition to rolling
					{
						ProcessImpactSounds(collisionEvent);
					}

					ProcessRollSounds(collisionEvent);  

					collisionEvent->rollStopTimeMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + g_RollingCollisionHoldTimeMs;
					break;
				default:
					naAssertf(0, "Unrecognized collision type: %u", collisionEvent->type);
					break;
			}
		}
	}
}

bool audCollisionAudioEntity::CanPlayRollSounds(const audCollisionEvent * colEvent)
{
	return (!colEvent->materialSettings[0] || colEvent->materialSettings[0]->MinRollSpeed < colEvent->rollMagnitudes[0]) &&		
				(!colEvent->materialSettings[1] || colEvent->materialSettings[1]->MinRollSpeed < colEvent->rollMagnitudes[1]);
}

bool audCollisionAudioEntity::ShouldPlayRollImpact(const audCollisionEvent * colEvt, float bangMag)
{
	if((colEvt->entities[0] && colEvt->entities[0]->GetIsTypePed()) || (colEvt->entities[1] && colEvt->entities[1]->GetIsTypePed()))
	{
		return true;
	}

	return (colEvt->materialSettings[0] && bangMag > colEvt->materialSettings[0]->RollImpactMag) ||
		(colEvt->materialSettings[1] && bangMag > colEvt->materialSettings[1]->RollImpactMag);
}

bool audCollisionAudioEntity::ShouldPlayScrapeImpact(const audCollisionEvent * colEvt, float bangMag)
{
	if((colEvt->entities[0] && colEvt->entities[0]->GetIsTypePed()) || (colEvt->entities[1] && colEvt->entities[1]->GetIsTypePed()))
	{
		return true;
	}
	return (colEvt->materialSettings[0] && bangMag > colEvt->materialSettings[0]->ScrapeImpactMag) ||
		(colEvt->materialSettings[1] && bangMag > colEvt->materialSettings[1]->ScrapeImpactMag);
}

bool audCollisionAudioEntity::PopulateAudioCollisionEventFromImpact(audCollisionEvent *collisionEvent, const VfxCollisionInfo_s &collisioninfo,
	float bangMag, float scrapeMag, float rollMag, bool shouldFlipEntities)
{

	//Compensate for entities being the wrong way round
	u8 indexOffset = 0;
	if(shouldFlipEntities)
	{
		indexOffset = 1;
	}

	u8 otherIndex = (indexOffset + 1)&1;	

	AudioWeight weights[2];
	weights[indexOffset] = GetAudioWeight(collisionEvent->entities[indexOffset]);
	weights[otherIndex] = GetAudioWeight(collisionEvent->entities[otherIndex]);

	collisionEvent->material0IsMap = weights[0] == AUDIO_WEIGHT_VH;
	collisionEvent->material1IsMap = weights[1] == AUDIO_WEIGHT_VH;

	if(collisionEvent->entities[0] && collisionEvent->entities[0]->GetCurrentPhysicsInst())
	{
		collisionEvent->material0IsMap = collisionEvent->entities[0]->GetCurrentPhysicsInst()->GetClassType() == PH_INST_MAPCOL;
	}
	if(collisionEvent->entities[1] && collisionEvent->entities[1]->GetCurrentPhysicsInst())
	{
		collisionEvent->material1IsMap = collisionEvent->entities[1]->GetCurrentPhysicsInst()->GetClassType() == PH_INST_MAPCOL;
	}


	f32 weightScaling[2];
	weightScaling[indexOffset] = GetAudioWeightScaling(weights[indexOffset], weights[otherIndex]);
	weightScaling[otherIndex] = GetAudioWeightScaling(weights[otherIndex], weights[indexOffset]);

	//Magnitudes are calculated using the bang/scrape/roll multiplied by the other entity's weight
	collisionEvent->impulseMagnitudes[indexOffset] = bangMag * weightScaling[indexOffset];
	collisionEvent->impulseMagnitudes[otherIndex] = bangMag * weightScaling[otherIndex];

	rollMag = rollMag * g_RollScalar; 

	if(collisionEvent->type == AUD_COLLISION_TYPE_ROLL && (!collisionEvent->rollUpdatedThisFrame || rollMag * weightScaling[indexOffset] > collisionEvent->rollMagnitudes[indexOffset]))
	{
		collisionEvent->rollMagnitudes[indexOffset] = rollMag * weightScaling[indexOffset];
		collisionEvent->rollMagnitudes[otherIndex] = rollMag * weightScaling[otherIndex];
	}
	
	if(collisionEvent->type == AUD_COLLISION_TYPE_SCRAPE && (!collisionEvent->scrapeUpdatedThisFrame || (scrapeMag *weightScaling[indexOffset]) > collisionEvent->scrapeMagnitudes[indexOffset]))
	{
		collisionEvent->scrapeMagnitudes[indexOffset] = scrapeMag * weightScaling[indexOffset];
		collisionEvent->scrapeMagnitudes[otherIndex] = scrapeMag * weightScaling[otherIndex];
	}

	Vector3 normals[2];
	normals[indexOffset] = VEC3V_TO_VECTOR3(collisioninfo.vWorldNormalA);
	normals[otherIndex] = VEC3V_TO_VECTOR3(collisioninfo.vWorldNormalB);
	
	//Use some temporary material ids to let us override the materials for special cases
	phMaterialMgr::Id materialIds[2];
	materialIds[indexOffset] = collisioninfo.mtlIdA;
	materialIds[otherIndex] = collisioninfo.mtlIdB;

	for(u8 i=0; i<2; i++)
	{
		CEntity *entity = collisionEvent->entities[i];
		CEntity *otherEntity = collisionEvent->entities[(i + 1)&1];

		collisionEvent->materialSettings[i] = NULL;

		if(entity && entity->GetIsTypePed())
		{
			// bullet impacts and car impacts increase general impacts for a time, as does death
			// but not with vehicles
			if((otherEntity == NULL || ((CPed*)entity)->GetPedAudioEntity()->ShouldUpBodyfallSounds() || ((((CPed*)entity)->IsDead()) && collisionEvent->GetFlag(AUD_COLLISION_FLAG_MELEE))))
			{
				collisionEvent->impulseMagnitudes[i] *= g_BodyFallBoost;
			}
			else
			{
				collisionEvent->impulseMagnitudes[i] *= g_PedBuildingImpulseScalingFactor;			
			}

			if(collisionEvent->type == AUD_COLLISION_TYPE_SCRAPE)
			{
				collisionEvent->materialSettings[i] = ((CPed*)entity)->GetPedAudioEntity()->GetScrapeMaterialSettings();
			}

		}

		if(entity && entity->GetIsTypeObject() && entity->GetBaseModelInfo()->GetModelType() == MI_TYPE_VEHICLE)
		{
			CVehicleModelInfo * vehicleInfo = (CVehicleModelInfo *)(entity->GetBaseModelInfo());
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
				if(Water::IsCameraUnderwater())
				{
					collisionEvent->impulseMagnitudes[i] = 0.f;
				}
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
				CObject * object = (CObject*)entity;
				if(object->m_nObjectFlags.bCarWheel)
				{
					collisionEvent->materialSettings[i] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(vehicleCollisions->WheelFragMaterial);
				}
				else
				{
					collisionEvent->materialSettings[i] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(vehicleCollisions->FragMaterial);
				}
			}
		}
		
		
		collisionEvent->materialIds[i] = materialIds[i];

		if (!collisionEvent->materialSettings[i]) 
		{		
			// extract the material data from the collision and see if we have a model specific override
			phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(materialIds[i]);
			CollisionMaterialSettings * materialSettings = GetMaterialOverride(collisionEvent->entities[i], g_audCollisionMaterials[(s32)unpackedMtlIdA], collisionEvent->components[i]);
			collisionEvent->materialSettings[i] = materialSettings;
		}

		CPed *ped = NULL;

		if(entity->GetIsTypePed())
		{
			ped = ((CPed *)(entity));
		}

		if(ped && ped->GetPedType() == PEDTYPE_ANIMAL)
		{
			collisionEvent->impulseMagnitudes[i] *= g_AnimalImpactScalar;

			if(ped->GetCapsuleInfo()->IsBird())
			{
				collisionEvent->materialSettings[i] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_BIRD", 0xBDC5E9E2));
			}
			else if(ped->GetCapsuleInfo()->IsFish())
			{
				collisionEvent->materialSettings[i] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_FISH", 0x51392531));
			}
			else if(ped->GetCapsuleInfo()->IsQuadruped())
			{
				switch(ped->GetArchetype()->GetModelNameHash())
				{
				case 0x97207223: //ATSTRINGHASH("A_C_Horse", 0x97207223)
				case 0xFCFA9E1E: //ATSTRINGHASH("A_C_Cow", 0xFCFA9E1E)
				case 0xD86B5A95: //ATSTRINGHASH("a_c_deer", 0xD86B5A95) 
					collisionEvent->materialSettings[i] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_QUADPED_LARGE", 0x628B6797));
					break;
				case 0x14EC17EA: //ATSTRINGHASH("a_c_chop", 0x14EC17EA)
				case 0xCE5FF074: //ATSTRINGHASH("a_c_boar", 0xCE5FF074)
				case 0x644AC75E: //ATSTRINGHASH("a_c_coyote", 0x644AC75E)
				case 0x1250D7BA: //ATSTRINGHASH("a_c_mtlion", 0x1250D7BA)
				case 0xB11BAB56: //ATSTRINGHASH("a_c_pig", 0xB11BAB56)
				case 0x349F33E1: //ATSTRINGHASH("a_c_retriever", 0x349F33E1)
				case 0xC2D06F53: //ATSTRINGHASH("a_c_rhesus", 0xC2D06F53)
				case 0x9563221D: //ATSTRINGHASH("a_c_rottweiler", 0x9563221D)
				case 0xA8683715: //ATSTRINGHASH("a_c_chimp", 0xA8683715)
				case 0x573201B8: //ATSTRINGHASH("a_c_cat_01", 0x573201B8)
				case 0xFC4E657D: //ATSTRINGHASH("A_C_POODLE ", 0xFC4E657D)
				case 0x6D362854: //ATSTRINGHASH("A_C_PUG", 0x6D362854)
				case 0xAD7844BB: //ATSTRINGHASH("A_C_WESTY", 0xAD7844BB)
					collisionEvent->materialSettings[i] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_QUADPED_MED", 0xEDA0B3E4));
					break;
				case 0xC3B52966: //ATSTRINGHASH("a_c_rat", 0xC3B52966)
				case 0xDFB55C81: //ATSTRINGHASH("a_c_rabbit_01", 0xDFB55C81)
					collisionEvent->materialSettings[i] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_QUADPED_SMALL", 0xB5D17E88));
					break;
				default:
					char animalMaterial[32];
					formatf(animalMaterial, "AM_ANIMAL_%08X", ped->GetArchetype()->GetModelNameHash());
					CollisionMaterialSettings * animalSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(animalMaterial);
					if(animalSettings)
					{
						collisionEvent->materialSettings[0] = animalSettings;
					}
					else
					{
						collisionEvent->materialSettings[0] = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_QUADPED_MED", 0xEDA0B3E4));
					}
				}
			}
		}

		if(collisionEvent->materialSettings[indexOffset] && collisionEvent->materialSettings[indexOffset]->RollSound == g_NullSoundHash)
		{
			collisionEvent->rollMagnitudes[indexOffset] = 0.f;
		}
		if(collisionEvent->materialSettings[otherIndex] && collisionEvent->materialSettings[otherIndex]->RollSound == g_NullSoundHash)
		{
			collisionEvent->rollMagnitudes[otherIndex] = 0.f;
		}
	}

	if(collisionEvent->materialSettings[0] == collisionEvent->materialSettings[1])
	{
		if(collisionEvent->impulseMagnitudes[0] > collisionEvent->impulseMagnitudes[1])
		{
			collisionEvent->impulseMagnitudes[1] = 0.f;
		}
		else
		{
			collisionEvent->impulseMagnitudes[0] = 0.f;
		}

		if(collisionEvent->scrapeMagnitudes[0] > collisionEvent->scrapeMagnitudes[1])
		{
			collisionEvent->scrapeMagnitudes[1] = 0.f;
		}
		else
		{
			collisionEvent->scrapeMagnitudes[0] = 0.f;
		}

		if(collisionEvent->rollMagnitudes[0] > collisionEvent->rollMagnitudes[1])
		{
			collisionEvent->rollMagnitudes[1] = 0.f;
		}
		else
		{
			collisionEvent->rollMagnitudes[0] = 0.f;
		}
	}

	return true;
}

CollisionMaterialSettings * audCollisionAudioEntity::GetVehicleCollisionSettings()
{
		phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(m_MaterialIdCarBody);
		return g_audCollisionMaterials[(s32)unpackedMtlIdA];
}

CollisionMaterialSettings * audCollisionAudioEntity::GetCollisionSettings(s32 materialId)
{
	phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(materialId);
	return g_audCollisionMaterials[(s32)unpackedMtlIdA];
}



CollisionMaterialSettings *audCollisionAudioEntity::GetMaterialSettingsFromMatId(const CEntity * entity, phMaterialMgr::Id matId, u32 component)
{
	naAssertf(PGTAMATERIALMGR->UnpackMtlId(matId)<(phMaterialMgr::Id)0xffff, "Too big for s32 cast");
	naAssertf(PGTAMATERIALMGR->UnpackMtlId(matId)<(phMaterialMgr::Id)g_audCollisionMaterials.GetCount(), "Out of bounds");
	phMaterialMgr::Id unpackedMtlId = PGTAMATERIALMGR->UnpackMtlId(matId);
	return	GetMaterialOverride(entity, g_audCollisionMaterials[(s32)unpackedMtlId], component);
}

const ModelAudioCollisionSettings * audCollisionAudioEntity::GetFragComponentMaterialSettingsSafe(const ModelAudioCollisionSettings * settings, u32 component)
{
	if(!GetNumFragComponentsFromModelSettings(settings)) 
	{
		return settings;
	}
	
	return	GetFragComponentMaterialSettings(settings, component);
}

const ModelAudioCollisionSettings * audCollisionAudioEntity::GetFragComponentMaterialSettings(const ModelAudioCollisionSettings * settings, u32 component, bool dontUseParentMacs)
{

	//naAssertf(component<GetNumFragComponentsFromModelSettings(settings), "Attempted to get ModelAudioSettings for fragment but component id is invalid");
	if((component>=GetNumFragComponentsFromModelSettings(settings)))
	{ 
		if(dontUseParentMacs)
		{
			return NULL;
		}
		return settings;
	}

	//Fix up the frag component array as it sits underneath the variable MaterialOverride array
	//ModelAudioCollisionSettings::tFragComponentSetting * fragArray = (ModelAudioCollisionSettings::tFragComponentSetting *) (((u8*)(settings->MaterialOverride + settings->numMaterialOverrides)) + 1);	
	ModelAudioCollisionSettings::tFragComponentSetting * fragArray = (ModelAudioCollisionSettings::tFragComponentSetting *) &(settings->MaterialOverride[settings->numMaterialOverrides].Override);	


	const ModelAudioCollisionSettings * newSettings = NULL;
	
	if(fragArray[component].ModelSettings.IsValid())
	{
		newSettings = audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(fragArray[component].ModelSettings);
	}

	if(!newSettings && !dontUseParentMacs)
	{
		newSettings = settings;
	}

	return newSettings;
}


//material is the audStringHash of the CollisionMaterialSettings name
CollisionMaterialSettings * audCollisionAudioEntity::GetMaterialOverride(const CEntity * entity, CollisionMaterialSettings * base, u32 component)
{
	if(entity && entity->IsArchetypeSet())
	{
		const ModelAudioCollisionSettings *settings = entity->GetBaseModelInfo()->GetAudioCollisionSettings();
		// Check for overrides in our map.
		if(!settings || (settings == g_MacsDefault)) 
		{
			// Hacky fix for url:bugstar:3580010
			if(entity->GetBaseModelInfo()->GetModelNameHash() == ATSTRINGHASH("prop_ld_can_01b", 0x6D9C0AC))
			{
				settings = audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(ATSTRINGHASH("mod_prop_ld_can_01b", 0x65DDABE4));
			}

			auto mapOverride = sm_MacOverrideMap.Access(entity->GetBaseModelInfo()->GetModelNameHash());
			if(mapOverride)
			{
				settings = audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>((*mapOverride));
				entity->GetBaseModelInfo()->SetAudioCollisionSettings(settings,0);
			}
		}

		if(entity->GetIsTypeObject())
		{
			CWeapon * weapon = ((CObject*)entity)->GetWeapon();
			if(weapon && weapon->GetWeaponInfo() && weapon->GetWeaponInfo()->GetAudioCollisionHash())
			{
				const ModelAudioCollisionSettings * weaponOverride = audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(weapon->GetWeaponInfo()->GetAudioCollisionHash());
				if(weaponOverride)
				{
					settings = weaponOverride;
				}
			}
		}
	
		if(settings)
		{	
			//See if the model has frag components
			if(GetNumFragComponentsFromModelSettings(settings))
			{
				//Get individual model settings for the fragment
				const ModelAudioCollisionSettings *fragSettings = GetFragComponentMaterialSettings(settings, component);

				//If we only have one possible override, let's just use that
				if(fragSettings->numMaterialOverrides == 1 && fragSettings->MaterialOverride[0].Override.IsValid())
				{
					CollisionMaterialSettings * overrideMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(fragSettings->MaterialOverride[0].Override);
					return overrideMaterial ? overrideMaterial : base; 
				}
				
				for(s32 i=0; fragSettings && i<fragSettings->numMaterialOverrides; i++)
				{
				    if(base && fragSettings->MaterialOverride[i].Material.IsValid() && fragSettings->MaterialOverride[i].Override.IsValid() && base == audNorthAudioEngine::GetObject<CollisionMaterialSettings>(fragSettings->MaterialOverride[i].Material))
				    {
					    //Found an override...huzzah!
						CollisionMaterialSettings * overrideMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(fragSettings->MaterialOverride[i].Override);
					    return overrideMaterial ? overrideMaterial : base; 
				    }
				}
			}

			if(settings->numMaterialOverrides == 1 && settings->MaterialOverride[0].Override.IsValid())
			{
				CollisionMaterialSettings * overrideMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(settings->MaterialOverride[0].Override);
				return overrideMaterial ? overrideMaterial : base; 
			}
			
			//We have found model specific settings game object so see if there is an override for this material
			for(s32 i=0; i<settings->numMaterialOverrides; i++)
			{
			    if(base && settings->MaterialOverride[i].Material.IsValid() && settings->MaterialOverride[i].Override.IsValid() && base == audNorthAudioEngine::GetObject<CollisionMaterialSettings>(settings->MaterialOverride[i].Material))
			    {
				    //Found an override...huzzah!
					CollisionMaterialSettings * overrideMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(settings->MaterialOverride[i].Override);
				    return overrideMaterial ? overrideMaterial : base; 
			    }
			}
		}
	}

	return base;
}

AudioWeight audCollisionAudioEntity::GetAudioWeight(CEntity *entity)
{
	if(entity && entity->GetIsTypePed())
	{
		return AUDIO_WEIGHT_M;
	}

	if(entity && entity->GetIsTypeVehicle())
	{
		return AUDIO_WEIGHT_H;
	}

	if(entity && entity->GetIsTypeObject() && entity->IsArchetypeSet())
	{ 
		const ModelAudioCollisionSettings *settings = entity->GetBaseModelInfo()->GetAudioCollisionSettings();
		if(settings)
		{
			//If we have a frag model, use the weight of the heaviest thing in the fragment (this could be extended to return the parent settings weight if the entity is unbroken should we need/want that functionality)
			s32 numComponents = GetNumFragComponentsFromModelSettings(settings);
			fragInst * frag = entity->GetFragInst();
			if(numComponents && entity->GetFragInst())
			{
				AudioWeight weight = AUDIO_WEIGHT_VL;
				if(frag)
				{
					//naDisplayf("Model settings: %s, numComponents", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(settings->NameTableOffset), numComponents);

					//naAssertf(numComponents == frag->GetTypePhysics()->GetCompositeBounds()->GetNumBounds(), "Model components (%d) and Macs frags (%d) are out of sync for %s: regenerate them, or fix them up in rave", frag->GetTypePhysics()->GetCompositeBounds()->GetNumBounds(), numComponents, entity->GetModelName());
					if(numComponents != frag->GetTypePhysics()->GetCompositeBounds()->GetNumBounds())
					{
						return (AudioWeight)settings->Weight;
					}

					for(s32 i = 0; i < numComponents; i++)
					{
						if(!frag->GetChildBroken(i))
						{
							const ModelAudioCollisionSettings * fragMod = GetFragComponentMaterialSettings(settings, i);
							if(fragMod)
							{
								weight = (AudioWeight)((fragMod->Weight > weight) ? fragMod->Weight : weight); 
							}
						}
					}
					return weight;
				}
			}
			else
			{
				return (AudioWeight)settings->Weight;
			}
		}
	}

	return AUDIO_WEIGHT_VH;
}


f32 audCollisionAudioEntity::GetAudioWeightScaling(AudioWeight weight, AudioWeight otherWeight)
{
	s32 weightDifference = Max(weight - otherWeight, 0);

	return g_AudWeightDifferenceScaling[weightDifference]; 
}




#if __BANK

void audCollisionAudioEntity::PrintCollisionSoundDebug(const char * context, f32 impulse, f32 volume, u32 soundHash)
{
	Displayf("[NorthAudio] %s, %u, %f, %f", context,
		//g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(soundHash),
		soundHash,
		volume, impulse);
}

void audCollisionAudioEntity::DrawDebug()
{
	if(g_DebugAudioImpacts)
	{
		for(u32 i = 0; i < g_NumDebugCollisionInfos; i++)
		{
			if(!g_CollisionDebugInfo[i].hidden)
			{
				grcDebugDraw::Sphere(RCC_VEC3V(g_CollisionDebugInfo[i].position), 0.05f, Color32(255,255,0));
				char buf[256];

				if(g_CollisionDebugInfo[i].impulseMag > 0.f)
				{
					formatf(buf, sizeof(buf), "%u: mag: %f [type: %u comp: %d] [type: %u comp: %d]", g_CollisionDebugInfo[i].time, g_CollisionDebugInfo[i].impulseMag,g_CollisionDebugInfo[i].entType1, g_CollisionDebugInfo[i].component1,g_CollisionDebugInfo[i].entType2, g_CollisionDebugInfo[i].component2);
					grcDebugDraw::Text(RCC_VEC3V(g_CollisionDebugInfo[i].position), Color32(255,255,0), buf);
				}
			}
		}
	}
	if(g_DebugAudioRolls)
	{
		for(u32 i = 0; i < g_NumDebugCollisionInfos; i++)
		{
			if(!g_CollisionDebugInfo[i].hidden && g_CollisionDebugInfo[i].type == AUD_COLLISION_TYPE_ROLL)
			{
				grcDebugDraw::Sphere(RCC_VEC3V(g_CollisionDebugInfo[i].position), 0.05f, Color32(100,255,0));
				char buf[256];

				if(g_CollisionDebugInfo[i].impulseMag > 0.f)
				{
					formatf(buf, sizeof(buf), "%u: mag: %f [type: %u comp: %d] [type: %u comp: %d]", g_CollisionDebugInfo[i].time, g_CollisionDebugInfo[i].impulseMag,g_CollisionDebugInfo[i].entType1, g_CollisionDebugInfo[i].component1,g_CollisionDebugInfo[i].entType2, g_CollisionDebugInfo[i].component2);
					grcDebugDraw::Text(RCC_VEC3V(g_CollisionDebugInfo[i].position), Color32(100,255,0), buf);
				}
			}
		}
	}
	if(g_DebugAudioScrapes)
	{
		for(u32 i = 0; i < g_NumDebugCollisionInfos; i++)
		{
			if(!g_CollisionDebugInfo[i].hidden && g_CollisionDebugInfo[i].type == AUD_COLLISION_TYPE_SCRAPE)
			{
				grcDebugDraw::Sphere(RCC_VEC3V(g_CollisionDebugInfo[i].position), 0.05f, Color32(255,100,0));
				char buf[256];

				if(g_CollisionDebugInfo[i].impulseMag > 0.f)
				{
					formatf(buf, sizeof(buf), "%u: mag: %f [type: %u comp: %d] [type: %u comp: %d]", g_CollisionDebugInfo[i].time, g_CollisionDebugInfo[i].impulseMag,g_CollisionDebugInfo[i].entType1, g_CollisionDebugInfo[i].component1,g_CollisionDebugInfo[i].entType2, g_CollisionDebugInfo[i].component2);
					grcDebugDraw::Text(RCC_VEC3V(g_CollisionDebugInfo[i].position), Color32(255,100,0), buf);
				}
			}
		}
	}
}

void audCollisionAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Collisions",false);
		bank.PushGroup("Bullet impacts and ricochets",false);
		bank.AddToggle("Dialogue suppress bullet impacts.", &sm_DialogueSuppressBI);
		bank.AddSlider("Player's ricochets rolloff scale factor", &sm_PlayerRicochetsRollOffScaleFactor, 0.f, 10.f, 0.001f);
		bank.AddSlider("Player's bullet impacts rolloff scale factor", &sm_PlayerBIRollOffScaleFactor, 0.f, 10.f, 0.001f);
		bank.AddSlider("Player's headshot rolloff scale factor", &sm_PlayerHeadShotRollOffScaleFactor, 0.f, 10.f, 0.001f);
		bank.AddSlider("Raygun BI Max Distance", &sm_DistanceToTriggerRaygunBI, 0.f, 1000.f, 0.001f);		
		bank.PopGroup();
		bank.PushGroup("Automatic weapons bullet impacts",false);
		bank.AddToggle("Only do it for vehicles", &sm_AutomaticImpactOnlyOnVeh);
			bank.AddSlider("Distance to trigger.", &sm_DistanceToTriggerAutomaticBI, 0.f, 100000.f, 1.f);
			bank.AddSlider("Time to retrigger.", &sm_DtToTriggerAutomaticBI, 0, 10000, 100);
		bank.PopGroup();
		bank.PushGroup("Shotgun  bullet impacts",false);
			bank.AddToggle("Don't do it for the player", &sm_ShotgunDisabledForPlayer);
			bank.AddToggle("Only do it for vehicles", &sm_ShotgunImpactOnlyOnVeh);
			bank.AddSlider("Distance to trigger.", &sm_DistanceToTriggerShotgunBI, 0.f, 100000.f, 1.f);
			bank.AddSlider("Time to retrigger.", &sm_DtToTriggerShotgunBI, 0, 10000, 100);
		bank.PopGroup();
		bank.AddToggle("Disable", &g_DisableCollisionAudio);
		bank.AddSlider("Min speed for collision", &g_MinSpeedForCollision, 0.0f, 10.0f, 0.01f);
		bank.AddSlider("Min roll speed", &g_MinRollSpeed, 0.0f, 10.0f, 0.01f);
		bank.AddSlider("Scrape impact velocity", &g_ScrapeImpactVel, 0.0f, 20.0f, 0.1f);
		bank.AddSlider("Vehicle Scrape impact velocity", &g_VehicleScrapeImpactVel, 0.0f, 20.0f, 0.1f);
		bank.AddSlider("Roll impact velocity", &g_RollImpactVel, 0.0f, 20.0f, 0.1f);
		bank.AddSlider("Roll Scalar", &g_RollScalar, 0.1f, 10.0f, 0.1f);
		bank.AddSlider("Impact Scalar", &s_ImpactMagScalar, 0.f, 10.0f, 0.1f);
		bank.AddSlider("Collision time filter speed", &g_CollisionTimeFilterSpeed, 0.0f, 10.0f, 0.01f);
		bank.AddSlider("Min time between collisions", &g_MinTimeBetweenCollisions, 0, 500, 1);
		bank.AddSlider("Collision filter lifetime", &g_CollisionFilterLifetime, 0, 1000, 1); 
		bank.AddSlider("Wrecked vehicle scaling", &g_WreckedVehicleImpactScale, 0.f, 10.f, 0.1f);
		bank.AddSlider("Wrecked vehicle scaling", &g_WreckedVehicleRolloffScale, 0.f, 10.f, 0.1f);
		bank.AddSlider("Animal impact scaling", &g_AnimalImpactScalar, 0.f, 10.f, 0.1f);
		bank.AddSlider("Body fall boost", &g_BodyFallBoost, 0.f, 10.f, 0.1f);
		bank.AddSlider("Scrape against ped volume scale", &g_MaterialScrapeAgainstPedVolumeScaling, 0.f, 1.f, 0.01f);
		bank.AddSlider("Player context collision rolloff", &sm_PlayerContextCollisionRolloff, 0.f, 10.f, 0.1f);
		bank.AddSlider("Player context blip volume", &sm_PlayerContextVolumeForBlip, -100.f, 0.f, 1.f);
		bank.AddSlider("sm_PlayerCollisionEventSpeedThreshold", &sm_PlayerCollisionEventSpeedThreshold, 0.f, 10.f, 0.1f);
		//bank.AddSlider("Melee impact scale factor for bullet hits", &s_MeleeImpactScaleFactor, 0.f, 1000.f, 1.f);

		bank.AddText("Collision Material Override", g_CollisionMaterialOverrideName, 64, false);
		bank.AddToggle("Print bullet impact material", &g_PrintBulletImpactMaterial);
		bank.AddToggle("Print melee impact material", &g_PrintMeleeImpactMaterial);
		bank.AddToggle("Increase footsteps volume", &g_IncreaseFootstepsVolume);
		bank.AddToggle("Don't play ricochets", &g_DontPlayRicochets);
		bank.AddToggle("Do ped scrape start impacts", &g_DoPedScrapeStartImpacts);
		bank.AddToggle("g_DebugAudioCollisions", &g_DebugAudioCollisions);
		bank.AddToggle("g_DebugAudioImpacts", &g_DebugAudioImpacts);
		bank.AddToggle("g_DebugAudioScrapes", &g_DebugAudioScrapes);
		bank.AddToggle("g_DebugAudioRolls", &g_DebugAudioRolls);
		bank.AddButton("Clear debug history", CFA(ClearDebugHistory));
		bank.AddToggle("g_DebugPlayerVehImpacts", &g_DebugPlayerVehImpacts);
		bank.AddToggle("g_MeleeOnlyBulletImpacts", &g_MeleeOnlyBulletImpacts);
		bank.AddSlider("g_MaxRollSpeed", &g_MaxRollSpeed, 0.0f, 100.0f, 0.01f);
		bank.AddToggle("g_MapUsesPropRollSpeedSettings", &g_MapUsesPropRollSpeedSettings);
		bank.AddSlider("g_RollVolumeSmoothUp", &g_RollVolumeSmoothUp, 0.0f, 1.0f, 0.001f);
		bank.AddSlider("g_RollVolumeSmoothDown", &g_RollVolumeSmoothDown, 0.0f, 1.0f, 0.001f);
		bank.AddSlider("g_VelocityThesholdForPropSwing", &g_VelocityThesholdForPropSwing, 0.0f, 10.f, 0.1f);
		bank.AddSlider("g_VehicleOnVehicleImpulseScalingFactor", &g_VehicleOnVehicleImpulseScalingFactor, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("g_BoatImpulseScalingFactor", &g_BoatImpulseScalingFactor, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("g_UpsideDownCarImpulseScalingFactor", &g_UpsideDownCarImpulseScalingFactor, 0.0f, 100.0f, 0.1f);
		bank.AddToggle("PedsHitLikeGirls", &g_PedsHitLikeGirls);
		bank.AddSlider("g_PedBuildingImpulseScalingFactor", &g_PedBuildingImpulseScalingFactor, 0.f, 100.f, 0.1f);
		bank.AddSlider("ImpactRetriggerTime", &g_ImpactRetriggerTime, 0, 10000, 1); 
		bank.AddSlider("Frag collision start time", &g_FragCollisionStartTime, 0, 10000, 1);
		bank.AddToggle("Use collision intensity culling", &g_UseCollisionIntensityCulling);
		bank.AddSlider("Collison count threshold", &g_CollisionMaterialCountThreshold, 0, 100, 1);
		bank.AddSlider("Collison count decrease rate", &g_CollisionCountDecreaseRate, 0, 100, 1);
		bank.AddToggle("Debug focus entity", &g_DebugFocusEntity);
		bank.AddToggle("DisplaysPedScrapeMaterial", &g_DisplayPedScrapeMaterial);
		bank.AddSlider("g_PedWetSurfaceScrapeAtten", &g_PedWetSurfaceScrapeAtten, 0.f, 1.f, 1.f);
		
		bank.AddSlider("g_MaxBulletImpactDampingDistance", &g_MaxBulletImpactDampingDistance, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("g_MinBulletImpactDampingDistance", &g_MinBulletImpactDampingDistance, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("g_MaxBulletImpactCloserImpulseDamping", &g_MaxBulletImpactCloserImpulseDamping, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("g_MaxBulletImpactCloserAttenuation", &g_MaxBulletImpactCloserAttenuation, -100.0f, 100.0f, 0.1f);
		bank.AddSlider("g_MaxImpulseMagnitudeForMeleeDamageDoneGeneral", &g_MaxImpulseMagnitudeForMeleeDamageDoneGeneral, 0.f, 100.f, 0.1f);
		bank.AddToggle("Debug swinging props", &g_DebugSwingingProps);
		bank.AddSlider("Swing Vol Smooth Rate", &g_SwingVolSmoothRate, 0.f, 10.f, 0.1f);
		bank.AddSlider("Weapon impact filter time", &g_WeaponImpactFilterTime, 0, 10000, 100);
		bank.AddSlider("Ped scrape start impact time", &g_WeaponImpactFilterTime, 0, 10000, 100);
		bank.AddToggle("g_PedsMakeSolidCollisions", &g_PedsMakeSolidCollisions);
		bank.AddToggle("g_PedsOverrideImpactMag", &g_PedsOverrideImpactMag);
		bank.PushGroup("Audio Weights Difference scaling");
			bank.AddSlider("Difference of 1", &g_AudWeightDifferenceScaling[1], 0.f, 1.f, 0.01f);
			bank.AddSlider("Difference of 2", &g_AudWeightDifferenceScaling[2], 0.f, 1.f, 0.01f);
			bank.AddSlider("Difference of 3", &g_AudWeightDifferenceScaling[3], 0.f, 1.f, 0.01f);
			bank.AddSlider("Solid Roll Attenuation", &g_SolidRollAttenuation, -100.f, 0.f, 0.5f);
			bank.AddSlider("Soft Roll Attenuation", &g_SoftRollAttenuation, -100.f, 0.f, 0.5f);
		bank.PopGroup();
		bank.AddToggle("HardImpactsExpludeSolid", &g_HardImpactsExcludeSolid);
		bank.PushGroup("Model editor", true);
			bank.AddToggle("Enable model editor", &sm_EnableModelEditor);
			bank.AddToggle("Preserve Existing Settings", &g_PreserveMACSSettingsOnExport);
			bank.AddButton("Create MACS for current model", CFA(CreateModelAudioSettings));
			bank.AddButton("Create Frag MACS for current model", CFA(CreateFragModelAudioSettings));
			bank.AddButton("Create MACS for current frag component", CFA(CreateIndividualFragModelAudioSettings));
			bank.AddSlider("Batch create start index", &batchModelCreateStartIndex, 0, 2000, 1);
			bank.AddButton("Batch create models", CFA(BatchCreateMacs));
			bank.AddSlider("Batch create stop index", &batchModelCreateStopIndex, 0, 2000, 1);
			bank.AddText("Current Model", s_ModelSettingsCreator.audioSettingsName, sizeof(s_ModelSettingsCreator.audioSettingsName), false);
			bank.AddText("Current Material", s_PointAndClickMaterial, sizeof(s_PointAndClickMaterial), false);
			bank.AddText("Current Material Override", s_PointAndClickMaterialOverride, sizeof(s_PointAndClickMaterial), false);
			bank.AddText("Current Frag Component`", s_PointAndClickComponent, sizeof(s_PointAndClickComponent), false);
			bank.AddButton("View current model settings", CFA(OpenCurrentModelInRave));
			bank.AddButton("Synchronize Macs and models", CFA(SynchronizeMacsAndModels));
			bank.AddButton("Find new models", CFA(FindNewModels));
			bank.AddButton("Find default material props", CFA(FindLocalDefaultMaterialModels));
			bank.AddButton("Spawn new model object", CFA(SpawnNewModelObject));
			bank.AddButton("Spawn sync model object", CFA(SpawnSyncModelObject));
			bank.AddSlider("Set new model spawn object", &newModelSpawnIndex, 0, 2000, 1);
			bank.AddSlider("Set sync model spawn object", &syncModelSpawnIndex, 0, 2000, 1);
			bank.AddSeparator();
			bank.AddText("Rave Selected Settings", s_RaveSelectedMaterialSettingsName, 256, false);
			bank.AddToggle("Auto Spawn", &s_AutoSpawnRaveObjects);
			bank.AddButton("Spawn Rave Settings", CFA(audCollisionAudioEntity::SpawnRaveSettings));
			bank.AddText("Prop spawn Name", s_PropSpawnName, 256, false);
			bank.AddButton("Spawn Prop from Name", CFA(SpawnPropFromPropName));
			bank.AddSeparator();
			bank.AddToggle("Generate Collision Lists", &sm_ProcessSectors);
			bank.PushGroup("Simulate collision");
				bank.AddButton("Do Collision", CFA(SimulateCollision));
				bank.AddSlider("Velocity", &s_SimulateCollisionVel, 0.f, 20.f, 1.f);
				bank.AddToggle("Soft impacts", &s_SimulateCollisionSoftImpact);
				//bank.AddCombo("Impact Weight", &s_SimulateCollisionWeight, NUM_AUDIOWEIGHT, s_AudioWeightNames, NullCB, "Select an impact weight");
				bank.AddSlider("Impact Weight", &s_SimulateCollisionWeight1, 0, NUM_AUDIOWEIGHT-1, 1);
				bank.AddSlider("Other Impact Weight", &s_SimulateCollisionWeight1, 0, NUM_AUDIOWEIGHT-1, 1);
				bank.PopGroup();
		bank.PopGroup();
		bank.AddSlider("Player melee collision boost", &g_PlayerMeleeCollisionVolumeBoost, -100.f, 100.f, 1.f);
		bank.AddSlider("Player melee-vehicle collision scale", &g_PlayerVehicleMeleeCollisionVolumeScale, -100.f, 100.f, 1.f);
		bank.PopGroup();
}
#endif
