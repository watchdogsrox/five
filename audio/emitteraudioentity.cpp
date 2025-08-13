// 
// audio/emitteraudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 
#include "ambience/ambientaudioentity.h"
#include "ambience/audambientzone.h"
#include "emitteraudioentity.h"
#include "northaudioengine.h"
#include "audio/environment/environment.h"
#include "audio/environment/environmentgroup.h"
#include "music/musicplayer.h"
#include "radioaudioentity.h"
#include "scriptaudioentity.h"
#include "streamslot.h"
#include "weatheraudioentity.h"
#include "policescanner.h"
#include "profile/profiler.h"

#include "audioengine/engine.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audioengine/environment.h"
#include "audioengine/widgets.h"
#include "audiohardware/driverdefs.h"
#include "audiohardware/streamingwaveslot.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"

#include "grcore/debugdraw.h"
#include "game/clock.h"
#include "scene/scene.h"
#include "game/weather.h"
#include "scene/entity.h"
#include "objects/object.h"
#include "control/gamelogic.h"
#include "scene/RefMacros.h"
#include "scene/2dEffect.h"
#include "renderer/Lights/LightSource.h"
#include "system/filemgr.h"
#include "fwsys/timer.h"
#include "string/stringutil.h"
#include "TimeCycle/TimeCycle.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/VfxHelper.h"
#include "camera/CamInterface.h"
#include "frontend/loadingscreens.h"

AUDIO_AMBIENCE_OPTIMISATIONS()

PF_PAGE(AudioEmitterPage, "Audio Emitters");
PF_GROUP(AudioEmitters);
PF_LINK(AudioEmitterPage, AudioEmitters);
PF_TIMER(StaticEmitterUpdate, AudioEmitters);

PF_VALUE_INT(NumStaticEmitters, AudioEmitters);
PF_VALUE_INT(NumStaticEmittersPlaying, AudioEmitters);
PF_VALUE_INT(NumEntityEmitters, AudioEmitters);
PF_VALUE_INT(NumEntityEmittersPlaying, AudioEmitters);

#include "debugaudio.h"

audEmitterAudioEntity g_EmitterAudioEntity;

// min time in ms between emitter retriggers
static const u32 g_audMinEmitterRetriggerTime = 500;

static const u8 g_MaxStaticEmitterLists				= 99;
static const u8	g_MaxStaticEmitterListNameLength	= 64;
static const char *g_StaticEmitterListBaseName		= "AMBIENT_EMITTER_LIST_";

//Keep static emitters playing until we are 1.333 times the maximum distance away, to avoid ping-pong streaming.
static const f32 g_StaticEmitterMaxDistanceScalingForStop = 1.333f;

#if __BANK

XPARAM(audiodesigner);

char g_DrawStaticEmitterFilter[128]={0};
bool audEmitterAudioEntity::sm_ShouldDrawStaticEmitters = false;
bool audEmitterAudioEntity::sm_DrawStaticEmitterOpennessValues = false;
bool audEmitterAudioEntity::sm_ShouldDrawEntityEmitters = false;
extern char g_CreateEmitterName[128];
extern char g_DuplicateEmitterName[128];

bool g_DebugStaticEmitterEnvironmentGroups = false;
#endif

extern CPlayerSwitchInterface g_PlayerSwitch;
const u32 g_MaxEmittersLinkedToProps = 256;
u32 g_CurrentPropLinkId = 0;
u32 g_PropLinkedEmitters[g_MaxEmittersLinkedToProps];

bool g_StaticEmitterNeedsInteriorInfo = false;

f32 g_SpaceFillingInteriorDuckingVolumeLin = 0.5f;
f32 g_EmitterInteriorDuckingSmootherRate = 0.1f;

s32 StaticEmitterEntry::GetRequestedPCMChannel() const
{
	// See url:bugstar:5154546 - Investigate missing emitter channel in boiler room cam captures. We only actually care
	// about setting the channel on the two main_area emitters, but just hard coding all of these to ensure we never get
	// into a state where other emitters have stolen all the instances of the pcm channel that we want.
	if (nameHash == ATSTRINGHASH("SE_ba_dlc_int_01_main_area", 0x6772F86B) ||
		nameHash == ATSTRINGHASH("SE_ba_dlc_int_01_bars", 0xC7CB4) ||
		nameHash == ATSTRINGHASH("SE_ba_dlc_int_01_Bogs", 0xE9E54990) ||
		nameHash == ATSTRINGHASH("SE_ba_dlc_int_01_Entry_Hall", 0x7D5E6C2))
	{
		return 1;
	}	
	else if (nameHash == ATSTRINGHASH("SE_ba_dlc_int_01_main_area_2", 0xFD2A8934) ||
			 nameHash == ATSTRINGHASH("SE_ba_dlc_int_01_Entry_Stairs", 0xEE2730F0) ||
			 nameHash == ATSTRINGHASH("SE_ba_dlc_int_01_garage", 0xEE2AE5C4) ||
			 nameHash == ATSTRINGHASH("SE_ba_dlc_int_01_office", 0x4112DBDA) ||
			 nameHash == ATSTRINGHASH("SE_ba_dlc_int_01_rear_L_corridor", 0x7F43408F))
	{
		return 0;
	}

	else if (nameHash == ATSTRINGHASH("SE_h4_dlc_int_02_h4_Bogs", 0xABAAF6E8) ||
			 nameHash == ATSTRINGHASH("SE_h4_dlc_int_02_h4_main_front_02", 0x15069E11) ||
			 nameHash == ATSTRINGHASH("SE_h4_dlc_int_02_h4_lobby", 0xA8017F05) ||
			 nameHash == ATSTRINGHASH("SE_h4_dlc_int_02_h4_main_bar", 0x3C27B29E))
	{
		return 1;
	}
	else if (nameHash == ATSTRINGHASH("SE_h4_dlc_int_02_h4_main_front_01", 0x43837B0A) ||
			 nameHash == ATSTRINGHASH("SE_h4_dlc_int_02_h4_Entrance_Doorway", 0x739B10DE) ||
			 nameHash == ATSTRINGHASH("SE_h4_dlc_int_02_h4_main_room_cutscenes", 0x88424B8E))
	{
		return 0;
	}

	else if (nameHash == ATSTRINGHASH("SE_DLC_Hei4_Island_Beach_Party_Music_New_01_Left", 0x3C8BE84A))
	{
		return 0;
	}
	else if (nameHash == ATSTRINGHASH("SE_DLC_Hei4_Island_Beach_Party_Music_New_02_Right", 0xF796B9AD))
	{
		return 1;
	}	
	else if (nameHash == ATSTRINGHASH("SE_DLC_Hei4_Island_Beach_Party_Music_New_03_Reverb", 0x35BDD571) ||
			 nameHash == ATSTRINGHASH("SE_DLC_Hei4_Island_Beach_Party_Music_New_04_Reverb", 0xBF137CE3))
	{
		return 2;
	}

	return -1;
}

bool StaticEmitterEntry::IsHighPriorityEmitter() const
{
	if (nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_Meet_rm_Music_00", 0xCAF4290A) ||
		nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_Meet_rm_Music_01", 0xBD6A8DF7) ||
		nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_Meet_rm_Music_02", 0x3D1C0D60) ||
		nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_Meet_rm_Music_03", 0x61DD56E2) ||
		nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_Meet_rm_Music_04", 0x939EBA64) ||
		nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_Meet_rm_Music_05", 0x86611FE9) ||
		nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_sandbox_music_01", 0x4408D5CE) ||
		nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_sandbox_music_02", 0x71D5B16B) ||
		nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_sandbox_viewer_area_music_01", 0x4DC334CE) ||
		nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_sandbox_viewer_area_music_02", 0x5EDED705))
	{
		return true;
	}	

	if(nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_Main_rm_Vehicle_Noise_01", 0xFFB16118) ||
	   nameHash == ATSTRINGHASH("SE_tr_tuner_car_meet_Main_rm_Vehicle_Noise_02", 0xDB1497DF))
	{

		return true;
	}


	return false;
}

bool StaticEmitterEntry::IsClubEmitter() const
{
	return emitter && GetRequestedPCMChannel() != -1;
}

void audEmitterAudioEntity::Init()
{
	if(!m_PreserveEntityEmitters)
	{
		m_EntityEnabledList.Init(g_audMaxEntityEmitters);
		m_NumEntityEmitters = 0;
		m_NumEntities = 0;
		m_FirstFreeBin = 0;


		for(u32 i = 0; i < 255; i++)
		{
			m_EntityBins[i].numEntities = 0;
		}
	}

	SetActive(false);
	g_CurrentPropLinkId = 0;
	m_HasInitialisedStaticEmitters = false;
	m_FirstUpdate = true;
	m_ConeCurve.Init("LINEAR_RISE");
	m_FillsInteriorSpaceDuckingSmoother.Init(g_EmitterInteriorDuckingSmootherRate, true);
	m_LastListenerPos = Vec3V(V_ZERO);
}

void audEmitterAudioEntity::AddBuildingAnimEvent(u32 hash, Vec3V_In inPos)
{
	for(int i=0; i < g_audNumBuildingAnimEvents; i++ )
	{
		if(m_BuildingAnimEvents[i].hash == 0)
		{
			m_BuildingAnimEvents[i].hash = hash;
			m_BuildingAnimEvents[i].pos = inPos;
			return;
		}
	}
	naAssertf(0, "Run out of building anim events in emitter audio entity");
}

void audEmitterAudioEntity::ClearBuildingAnimEvents()
{
	for(int i=0; i < g_audNumBuildingAnimEvents; i++ )
	{
		m_BuildingAnimEvents[i].hash = 0;
	}
}

bool audEmitterAudioEntity::IsWithinRangeOfRadio(const Vector3& inPosition, float fRange)
{
	const float fRangeSq = rage::square(fRange);
	for(u32 scan = 0 ; scan < m_CurrentStaticEmitterIndex; ++scan)
	{
		if(m_StaticEmitterFlags[scan].bIsEnabled)
		{
			if (m_StaticEmitterFlags[scan].bIsPlayingRadio)
			{
				if (m_StaticEmitterList[scan].position.Dist2(inPosition) < fRangeSq)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void audEmitterAudioEntity::UpdateBuildingAnimEvents()
{
	for(int i=0; i<g_audNumBuildingAnimEvents; i++)
	{
		if(m_BuildingAnimEvents[i].hash)
		{
			audSoundInitParams initParams;
			initParams.Position = VEC3V_TO_VECTOR3(m_BuildingAnimEvents[i].pos);
			CreateAndPlaySound(m_BuildingAnimEvents[i].hash, &initParams);
			m_BuildingAnimEvents[i].hash = 0;
		}
	}
}

void audEmitterAudioEntity::AddStaticEmitter(u32 emitterName, const u16 gameTimeMinutes)
{
	StaticEmitter* emitter  = audNorthAudioEngine::GetObject<StaticEmitter>(emitterName);

	if(emitter)
	{
		audAssert(m_CurrentStaticEmitterIndex < g_audMaxStaticEmitters);
		if(m_CurrentStaticEmitterIndex < g_audMaxStaticEmitters)
		{
			m_StaticEmitterList[m_CurrentStaticEmitterIndex].emitter = emitter;
			m_StaticEmitterList[m_CurrentStaticEmitterIndex].nameHash = emitterName;

			TristateValue enabledValue = AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_STATICEMITTER_ENABLED);
			m_StaticEmitterFlags[m_CurrentStaticEmitterIndex].bIsEnabled = (enabledValue != AUD_TRISTATE_FALSE);
			NA_RADIO_ENABLED_ONLY(m_StaticEmitterList[m_CurrentStaticEmitterIndex].radioEmitter.SetEmitter(emitter));
			m_StaticEmitterList[m_CurrentStaticEmitterIndex].position.Set(emitter->Position.x, emitter->Position.y, emitter->Position.z);
			m_StaticEmitterList[m_CurrentStaticEmitterIndex].lastTriggerTime = 0;

			if(AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_STATICEMITTER_LINKEDTOPROP) == AUD_TRISTATE_TRUE)
			{
				Assert(g_CurrentPropLinkId < g_MaxEmittersLinkedToProps-1);
				if(g_CurrentPropLinkId < g_MaxEmittersLinkedToProps-1)
				{
					g_PropLinkedEmitters[g_CurrentPropLinkId++] = m_CurrentStaticEmitterIndex;
					m_StaticEmitterFlags[m_CurrentStaticEmitterIndex].bIsLinked = true;
					m_StaticEmitterList[m_CurrentStaticEmitterIndex].nextDamageFlipTime = 0;
				}
			}

			const u32 hospitalsRadio = ATSTRINGHASH("HOSPITALS_POLICE_SCANNER", 0x03094b005);

			m_StaticEmitterFlags[m_CurrentStaticEmitterIndex].bStartStopImmediatelyAtTimeLimits = AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_STATICEMITTER_STARTSTOPIMMEDIATELYATTIMELIMITS) != AUD_TRISTATE_FALSE;
			m_StaticEmitterFlags[m_CurrentStaticEmitterIndex].bIsActive24h = (emitter->MinTimeMinutes == 0 && emitter->MaxTimeMinutes == 1440);
			m_StaticEmitterFlags[m_CurrentStaticEmitterIndex].bIsScanner = (emitter->Sound == hospitalsRadio);
			m_StaticEmitterFlags[m_CurrentStaticEmitterIndex].bWasValidTime = audAmbientZone::IsValidTime(m_StaticEmitterList[m_CurrentStaticEmitterIndex].emitter->MinTimeMinutes, m_StaticEmitterList[m_CurrentStaticEmitterIndex].emitter->MaxTimeMinutes, gameTimeMinutes);
			m_StaticEmitterList[m_CurrentStaticEmitterIndex].occlusionGroup = NULL;
			m_StaticEmitterList[m_CurrentStaticEmitterIndex].alarmSettings = audNorthAudioEngine::GetObject<AlarmSettings>(emitter->AlarmSettings);

			class StreamingSoundQueryFn : public audSoundFactory::audSoundProcessHierarchyFn
			{
			public:
				bool containsStreamingSounds;

				StreamingSoundQueryFn() : containsStreamingSounds(false) {};

				void operator()(u32 classID, const void* UNUSED_PARAM(soundData))
				{
					if(classID == StreamingSound::TYPE_ID)
					{
						containsStreamingSounds = true;
					}
				}
			};

			StreamingSoundQueryFn streamingSoundQuery;
			SOUNDFACTORY.ProcessHierarchy(emitter->Sound, streamingSoundQuery);
			m_StaticEmitterFlags[m_CurrentStaticEmitterIndex].bIsStreamingSound = streamingSoundQuery.containsStreamingSounds;

			// Pack the distance values into the position W component to save fetching them from metadata each time we want to query them. Don't need to
			// be super accurate with these distances, so precision loss is acceptable.
			u16 minDist = 0u;
			u16 maxDist = 0u;			
			Assign(minDist, m_StaticEmitterList[m_CurrentStaticEmitterIndex].emitter->MinDistance);
			Assign(maxDist, m_StaticEmitterList[m_CurrentStaticEmitterIndex].emitter->MaxDistance);
			u32 packedDistance = (minDist << 16) | maxDist;
			m_StaticEmitterList[m_CurrentStaticEmitterIndex].position.SetWAsUnsignedInt(packedDistance);

			m_CurrentStaticEmitterIndex++;
		}
	}
}

void audEmitterAudioEntity::UpdateStaticEmitterPosition(StaticEmitter* emitter)
{
	for(u32 emitterIndex=0; emitterIndex<m_CurrentStaticEmitterIndex; emitterIndex++)
	{
		if(emitter == m_StaticEmitterList[emitterIndex].emitter)
		{
			m_StaticEmitterList[emitterIndex].position.Set(emitter->Position.x, emitter->Position.y, emitter->Position.z);
		}
	}
}

void audEmitterAudioEntity::ReInitStaticEmitters()
{
	for(u32 loop = 0; loop < m_CurrentStaticEmitterIndex; loop++)
	{
		StopEmitter(loop);
		m_StaticEmitterList[loop].Reset();
		m_StaticEmitterFlags[loop].Reset();
	}

	CreateStaticEmitters();
}

void audEmitterAudioEntity::InitStaticEmitters()
{
	//NOTE: This function must be called AFTER all downloadable audio content has been mounted and all game objects have been combined.
	// Otherwise downloadable static emitters will be ignored.

	// only do this work once
	if(!m_HasInitialisedStaticEmitters)
	{
		CreateStaticEmitters();
		m_HasInitialisedStaticEmitters = true;
		naAudioEntity::Init();
		audEntity::SetName("audEmitterAudioEntity");
	}
}

void audEmitterAudioEntity::CreateStaticEmitters()
{
	const u16 gameTimeMinutes = static_cast<u16>((CClock::GetHour() * 60) + CClock::GetMinute());

	g_CurrentPropLinkId = 0;
	m_CurrentStaticEmitterIndex = 0;
	char staticEmitterListName[8];
	u32 partialHash;

	const char* pLevelName = "gta5";
	s32 levelIndex = CGameLogic::GetCurrentLevelIndex();

	// Game level index value defaults to level 1 even with 0 levels loaded...
	if(levelIndex < CScene::GetLevelsData().GetCount())
	{
#if RSG_BANK
		if(PARAM_audiodesigner.Get())
		{
			pLevelName = audNorthAudioEngine::GetCurrentAudioLevelName();
		}
#endif
	}

	atHashString levelName = pLevelName;
	size_t len = strlen(pLevelName);
	
	if(levelName == "gta5" || len <= 5)
	{
		partialHash = atPartialStringHash(g_StaticEmitterListBaseName);
	}
	else
	{
		const char *suffix = pLevelName + 5;
		char levelSpecificStaticEmitterListName[64];
		formatf(levelSpecificStaticEmitterListName, "%s%s_", g_StaticEmitterListBaseName, suffix);
		partialHash = atPartialStringHash(levelSpecificStaticEmitterListName);
	}

	for(u8 listIndex=0; listIndex<g_MaxStaticEmitterLists; listIndex++)
	{
		formatf(staticEmitterListName, "%02u", listIndex + 1);

		StaticEmitterList *staticEmitterList = audNorthAudioEngine::GetObject<StaticEmitterList>(atStringHash(staticEmitterListName, partialHash));
		if(staticEmitterList)
		{
			for(u32 emitterIndex=0; emitterIndex<staticEmitterList->numEmitters; emitterIndex++)
			{					
				AddStaticEmitter(staticEmitterList->Emitter[emitterIndex].StaticEmitter, gameTimeMinutes);
			}
		}
		else
		{
			continue;
		}
	}
}

void audEmitterAudioEntity::Shutdown()
{
	BANK_ONLY(m_EntityEmitterDebugInfo.Reset());

	SetActive(false);
	for(u32 i = 0 ; i < m_CurrentStaticEmitterIndex; i++)
	{
		if(m_StaticEmitterFlags[i].bIsEnabled)
		{
			if(m_StaticEmitterList[i].sound)
			{
				// the controller will do this for us when the entity is destroyed but we may as well be explicit
				m_StaticEmitterList[i].sound->StopAndForget();
			}
			NA_RADIO_ENABLED_ONLY(m_StaticEmitterList[i].radioEmitter.SetOcclusionGroup(NULL);)
			if(m_StaticEmitterList[i].occlusionGroup)
			{
				m_StaticEmitterList[i].occlusionGroup->RemoveSoundReference();
				m_StaticEmitterList[i].occlusionGroup = NULL;
			}
		}
	}
	if(!m_PreserveEntityEmitters)
	{
		m_EntityEnabledList.Kill();
	}
	audEntity::Shutdown();
}

void audEmitterAudioEntity::StopAndResetStaticEmitters()
{
	for(u32 i = 0 ; i < m_CurrentStaticEmitterIndex; i++)
	{		
		if(m_StaticEmitterList[i].sound)
		{
			m_StaticEmitterList[i].sound->StopAndForget();
		}

#if NA_RADIO_ENABLED
		g_RadioAudioEntity.StopStaticRadio(&(m_StaticEmitterList[i].radioEmitter));
		m_StaticEmitterFlags[i].bIsPlayingRadio = false;
		m_StaticEmitterList[i].radioEmitter.SetOcclusionGroup(NULL);
#endif

		if(m_StaticEmitterList[i].occlusionGroup)
		{
			m_StaticEmitterList[i].occlusionGroup->RemoveSoundReference();
			m_StaticEmitterList[i].occlusionGroup = NULL;
		}

		audStreamSlot *streamSlot = m_StaticEmitterList[i].streamSlot;
		if(streamSlot)
		{
			streamSlot->Free();
			m_StaticEmitterList[i].streamSlot = NULL;
		}

		m_StaticEmitterFlags[i].bWasActive = false;
		m_StaticEmitterList[i].lastTriggerTime = 0;
	}

	for(u32 i = 0 ; i < m_EntityEmitterList.GetMaxCount(); i++)
	{
		if(m_EntityEmitterList[i].sound)
		{
			m_EntityEmitterList[i].sound->StopAndForget();
			m_EntityEmitterList[i].lastTriggerTime = 0u;
			m_EntityCheckTimes[i] = 0;
		}
	}
}

void audEmitterAudioEntity::ResetTimers()
{
	for(u32 i = 0 ; i < m_CurrentStaticEmitterIndex; i++)
	{
		m_StaticEmitterList[i].lastTriggerTime = 0;
	}

	for(u32 i = 0 ; i < g_audMaxEntityEmitters; i++)
	{
		m_EntityCheckTimes[i] = 0;
	}
}

void audEmitterAudioEntity::FindAudioBinAndIndex(CEntity *entity, u32 &bin, u32 &idx)
{
	bin = 0xff;
	idx = 0xff;
	if(entity->GetAudioBin() != 0xff)
	{
		bin = entity->GetAudioBin();
		for(u32 i = 0; i < m_EntityBins[bin].numEntities; i++)
		{
			if(m_EntityBins[bin].entities[i] == entity)
			{
				idx = i;
				return;
			}
		}
		naErrorf("Couldn't find emitter audio index for entity %s, even though it has a valid bin", entity->GetModelName());
	}
	else
	{
		// allocate bin/index for this entity
		if(m_NumEntities < (255*g_audNumEntitiesPerBin))
		{
			bin = m_FirstFreeBin;
			entity->SetAudioBin((u8)bin);
			idx = m_EntityBins[bin].numEntities++;
			m_EntityBins[bin].entities[idx] = entity;
			m_EntityBins[bin].tailEmitters[idx] = 0xffff;
			m_NumEntities++;
			if(m_EntityBins[bin].numEntities == g_audNumEntitiesPerBin)
			{
				m_FirstFreeBin++;
			}		
		}
	}
}

bool audEmitterAudioEntity::GetGeneratesWindAudio(const CAudioAttr *audioAttr) const
{
	EntityEmitter *emitter = audNorthAudioEngine::GetObject<EntityEmitter>(audioAttr->effectindex);
	if(emitter && AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_ENTITYEMITTER_WINDBASEDVOLUME)==AUD_TRISTATE_TRUE)
	{
		return true;
	}
	return false;
}

void audEmitterAudioEntity::RegisterEntityEmitter(CEntity *entity, u32 hash, Vector3 &offset, Quaternion &rotation)
{
	const u32 windThroughTrees = ATSTRINGHASH("EE_WIND_THROUGH_TREES", 0x894BAFB2);
	const u32 windThroughTrees2 = ATSTRINGHASH("EE_WIND_THROUGH_TREES_HUGE", 0x8C9873DC);
	const u32 windThroughTrees3 = ATSTRINGHASH("EE_WIND_THROUGH_TREES_SMALL", 0xF9CA1086);
	
	const u32 aircon1 = ATSTRINGHASH("EE_AIRCON_FAN_TINY", 0xDFD650E);
	const u32 aircon2 = ATSTRINGHASH("EE_AIRCON_FAN_SMALL", 0xCDCC1905);
	const u32 aircon3 = ATSTRINGHASH("EE_AIRCON_FAN_SMALL_ONE_WAY", 0xC311584C);
	const u32 alarmEmitter = ATSTRINGHASH("EE_ALARM", 0xAFAB9C80);

	const u32 generatorWithLightsSound = ATSTRINGHASH("EE_GENERATOR_MED_03", 0xC9D3D33E);
	const u32 generatorWithLightsModel = ATSTRINGHASH("prop_generator_03b", 0xFC96F411);

	DEBUG_STREAMING_ONLY(bool found = false);

	const u32 pickup1 = ATSTRINGHASH("EE_DREYFUSS_PAPER", 0x868F6E86);
	const u32 pickup2 = ATSTRINGHASH("EE_OMEGA_POWER_CELLS", 0x30EF1A34);

	const u32 radioEmitter = ATSTRINGHASH("EE_RADIO", 0x438EC5FD);

	const u32 stuntProp1 = ATSTRINGHASH("EE_DLC_STUNT_TUBE_ROTATING_ARM", 0xCCD6C2EA);
	const u32 stuntProp2 = ATSTRINGHASH("EE_DLC_STUNT_TUBE_ROTATING_ARM_CENTRE", 0xF77A996E);

	const bool isCritical = (hash == radioEmitter || hash == pickup1 || hash == pickup2);
	const bool frequentUpdates = (hash == stuntProp1 || hash == stuntProp2);
	if(m_NumEntityEmitters > (g_audMaxEntityEmitters-16) && !isCritical)
	{
		// Ensure we leave space for 'critical' emitters
		return;
	}
	
	// don't spawn entity emitters for fragments of prop_generator_03b
	if(entity->GetModelNameHash() == generatorWithLightsModel && hash == generatorWithLightsSound && entity->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
	{
		return;
	}

#if __BANK
	audEntityEmitterDebugInfo& emitterInfo = m_EntityEmitterDebugInfo.Grow();
	emitterInfo.hash = hash;
	emitterInfo.entity = entity;
	emitterInfo.offset = offset;
#endif

	if(hash == alarmEmitter || hash == radioEmitter)
	{
 		naCErrorf(entity->GetType() == ENTITY_TYPE_OBJECT, "Entity %s is not an object", entity->GetModelName());
		Vector3 searchPos = entity->TransformIntoWorldSpace(offset);
		for(u32 i = 0 ; i < g_CurrentPropLinkId; i++)
		{			
			if(!m_StaticEmitterList[g_PropLinkedEmitters[i]].isScriptLinkedEmitter)
			{
				// do we have a static emitter within a 4.25m radius?
				if((m_StaticEmitterList[g_PropLinkedEmitters[i]].position - searchPos).Mag2() < 18.f 
					&& ((m_StaticEmitterList[g_PropLinkedEmitters[i]].alarmSettings && hash == alarmEmitter) || (hash == radioEmitter)))
				{
					m_StaticEmitterList[g_PropLinkedEmitters[i]].linkedObject = entity;
#if AUD_DEBUG_STREAMING
					found = true;
					naDisplayf("Found linked prop for emitter %u at pos: %f %f %f [emitter: %f %f %f]", g_PropLinkedEmitters[i], searchPos.x, searchPos.y, searchPos.z, m_StaticEmitterList[g_PropLinkedEmitters[i]].position.x, m_StaticEmitterList[g_PropLinkedEmitters[i]].position.y, m_StaticEmitterList[g_PropLinkedEmitters[i]].position.z);
#endif
					break;
				}
			}
		}
		
#if AUD_DEBUG_STREAMING
		if(!found)
		{
			naWarningf("Failed to find linked prop for emitter at pos: %f %f %f", searchPos.x, searchPos.y, searchPos.z);
		}
#endif
		
	}
	else
	{
		if(hash != windThroughTrees && hash != windThroughTrees2 && hash != windThroughTrees3)		
		{
			const Vector3 pos = entity->TransformIntoWorldSpace(offset);		

			// ignore small aircons high up
			if(hash == aircon1 || hash == aircon3 || hash == aircon2)
			{
				if(pos.z > 49.f)
				{
					return;
				}
			}
			if(naVerifyf(m_NumEntityEmitters < g_audMaxEntityEmitters, "Too many entity emitters (limit is %d, trying to add hash %u)", g_audMaxEntityEmitters, hash))
			{
				EntityEmitter *emitter = audNorthAudioEngine::GetObject<EntityEmitter>(hash);
				if(emitter && ShouldEntityEmitterBeRegistered(emitter, pos))
				{
					u32 bin, entityIndex;
					FindAudioBinAndIndex(entity, bin, entityIndex);
					if(bin!=0xff)
					{
						for(u32 i = 0; i < g_audMaxEntityEmitters; i++)
						{
							if(!IsEntityEnabled(i))
							{
								if(emitter && AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_ENTITYEMITTER_VOLUMECONE) == AUD_TRISTATE_TRUE)
								{
									m_EntityEmitterList[i].rotation.Set(QUATERNION_TO_QUATV(rotation));
								}
								
								m_EntityEmitterList[i].offset = offset;
								m_EntityEmitterList[i].emitter = emitter;
								m_EntityEmitterList[i].lastTriggerTime = fwTimer::GetTimeInMilliseconds();
								m_EntityEmitterList[i].sound = NULL;
								m_EntityEmitterList[i].entity = entity;
								m_EntityEmitterList[i].frequentUpdates = frequentUpdates;
																																						
								Assign(m_EntityEmitterList[i].prevEmitterIdx, m_EntityBins[bin].tailEmitters[entityIndex]);
								Assign(m_EntityBins[bin].tailEmitters[entityIndex], i);

								// derive a 'random' seed based on world position
								const u32 mag = (u32)floorf(pos.Mag()*100.f);
								const u32 idx = mag % 10000;
								const f32 randomSeed = (idx / 10000.0f);
								m_EntityEmitterList[i].randomSeed = randomSeed;
								m_NumEntityEmitters++;
								m_EntityCheckTimes[i] = 0;
								m_EntityEnabledList.Set(i);
								
								return;
							}
						}
					}
				}
			}
		}
	}
}

void audEmitterAudioEntity::UnregisterEmittersForEntity(CEntity *entity)
{
#if __BANK
	for(u32 i = 0; i < m_EntityEmitterDebugInfo.GetCount(); )
	{
		if(m_EntityEmitterDebugInfo[i].entity == entity)
		{
			m_EntityEmitterDebugInfo.DeleteFast(i);
		}
		else
		{
			i++;
		}
	}
#endif

	if(entity->GetAudioBin() != 0xff)
	{
		u32 entityIdx = 0;
		const u32 bin = entity->GetAudioBin();
		for(; entityIdx < m_EntityBins[bin].numEntities; entityIdx++)
		{
			if(m_EntityBins[bin].entities[entityIdx] == entity)
			{
				break;
			}
		}
		if(naVerifyf(entityIdx<m_EntityBins[bin].numEntities, "Entity index %d is invalid", entityIdx))
		{
			m_EntityBins[bin].entities[entityIdx] = NULL;
				
			// remove all of the emitters for this entity
			u32 idx = m_EntityBins[bin].tailEmitters[entityIdx];
			m_EntityBins[bin].tailEmitters[entityIdx] = 0xffff;
			
			while(idx < g_audMaxEntityEmitters /*!= 0xffff*/)
			{
				naAssertf(m_EntityEmitterList[idx].entity == entity, "Entity %s has a valid index but doesn't match the entity in the emitter list", entity->GetModelName());
				naCErrorf(m_EntityEmitterList[idx].prevEmitterIdx != idx, "Index %d is same as the previous index", idx);
				//Assert(m_StaticEmitterFlags[idx].bIsEnabled);

				m_EntityEnabledList.Clear(idx);
				
				if(m_EntityEmitterList[idx].sound)
				{
					m_EntityEmitterList[idx].sound->StopAndForget();
				}
				m_EntityEmitterList[idx].entity = NULL;
				naAssertf(m_NumEntityEmitters, "Can't have less than 0 entity emitters!");
				m_NumEntityEmitters--;
				u32 temp = idx;
				idx = m_EntityEmitterList[idx].prevEmitterIdx;
				m_EntityEmitterList[temp].prevEmitterIdx = 0xffff;
			}
	
			// shuffle entities around in this bin if required (ie its not already the last one)
			if(entityIdx != static_cast<u32>(m_EntityBins[bin].numEntities)-1)
			{
				// move the last one into this slot
				m_EntityBins[bin].entities[entityIdx] = m_EntityBins[bin].entities[static_cast<u32>(m_EntityBins[bin].numEntities)-1];
				m_EntityBins[bin].tailEmitters[entityIdx] = m_EntityBins[bin].tailEmitters[static_cast<u32>(m_EntityBins[bin].numEntities)-1];
				
				m_EntityBins[bin].entities[static_cast<u32>(m_EntityBins[bin].numEntities)-1] = NULL;
				m_EntityBins[bin].tailEmitters[static_cast<u32>(m_EntityBins[bin].numEntities)-1] = 0xffff;
			}
			m_EntityBins[bin].numEntities--;
			m_NumEntities--;
			entity->SetAudioBin(0xff);

			// shuffle this bin around if ít's below the first free bin
			if(bin < m_FirstFreeBin)
			{
				// swap with the last full bin
				m_FirstFreeBin--;
				tEntityBin tempBin = m_EntityBins[m_FirstFreeBin];
				m_EntityBins[m_FirstFreeBin] = m_EntityBins[bin];
				m_EntityBins[bin] = tempBin;

				// update all entities in the bins that have been moved
				for(u32 i = 0; i < m_EntityBins[m_FirstFreeBin].numEntities; i++)
				{
					naAssertf(m_EntityBins[m_FirstFreeBin].entities[i], "Invalid entity in bin %d", m_FirstFreeBin);
					naCErrorf(m_EntityBins[m_FirstFreeBin].entities[i]->GetAudioBin() == bin, "Entity %s has a bin %d which doesn't match expected bin %d", entity->GetModelName(), m_EntityBins[m_FirstFreeBin].entities[i]->GetAudioBin(), bin);
					
					m_EntityBins[m_FirstFreeBin].entities[i]->SetAudioBin((u8)m_FirstFreeBin);
				}
				for(u32 i = 0; i < m_EntityBins[bin].numEntities; i++)
				{
					naAssertf(m_EntityBins[bin].entities[i], "Invalid entity in bin %d", bin);
					naCErrorf(m_EntityBins[bin].entities[i]->GetAudioBin() == m_FirstFreeBin, "Entity %s has a bin %d which doesn't match expected bin %d", entity->GetModelName(), m_EntityBins[bin].entities[i]->GetAudioBin(), m_FirstFreeBin);
					m_EntityBins[bin].entities[i]->SetAudioBin((u8)bin);
				}
			}
		}
		entity->SetAudioBin(0xff);
	}
}

void audEmitterAudioEntity::Update()
{
	if(m_IsActive)
	{
		UpdateStaticEmitters();
		UpdateEntityEmitters();
		UpdateBuildingAnimEvents();
		m_FirstUpdate = false;
	}

	PF_SET(NumStaticEmitters, m_CurrentStaticEmitterIndex);
	PF_SET(NumEntityEmitters, m_NumEntityEmitters);
#if __BANK
	if(sm_ShouldDrawStaticEmitters || sm_ShouldDrawEntityEmitters)
	{
		DrawDebug();
	}
#endif
}

void audEmitterAudioEntity::UpdateStaticEmitters()
{
	PF_FUNC(StaticEmitterUpdate);

	m_FillsInteriorSpaceDuckingSmoother.SetRate(g_EmitterInteriorDuckingSmootherRate);
	float duckLinear = 1.f;
	if(g_ScriptAudioEntity.ShouldDuckForScriptedConversation())
	{
		duckLinear = m_FillsInteriorSpaceDuckingSmoother.CalculateValue(g_SpaceFillingInteriorDuckingVolumeLin, fwTimer::GetTimeStep());
	}
	else
	{
		duckLinear = m_FillsInteriorSpaceDuckingSmoother.CalculateValue(1.f, fwTimer::GetTimeStep());
	}
	const float fillsInteriorSpaceDucking = audDriverUtil::ComputeDbVolumeFromLinear(duckLinear);

	u32 numStaticEmittersPlaying = 0;
	const u32 timeInMs = fwTimer::GetTimeInMilliseconds();
	const bool playerSwitchActive = g_PlayerSwitch.IsActive();
	const bool gameWorldHidden = camInterface::IsFadedOut() || CLoadingScreens::AreActive();
	const u16 gameTimeMinutes = static_cast<u16>((CClock::GetHour() * 60) + CClock::GetMinute());

	for(u32 i = 0 ; i < m_CurrentStaticEmitterIndex; i++)
	{
		const bool updateThisFrame = (fwTimer::GetFrameCount()&3) == (i&3) || m_StaticEmitterList[i].IsClubEmitter();

		// Script linked emitters follow around the attached object
		if(updateThisFrame && m_StaticEmitterFlags[i].bIsEnabled && m_StaticEmitterList[i].isScriptLinkedEmitter)
		{			
			UpdateScriptPositionedEmitter(i);
		}

		if(m_StaticEmitterFlags[i].bIsEnabled && (m_StaticEmitterFlags[i].bWasActive || updateThisFrame))
		{
			bool shouldPlay = ShouldStaticEmitterBePlaying(i, playerSwitchActive, gameWorldHidden, gameTimeMinutes);
			if(shouldPlay)
			{
				const Vector3 pos(m_StaticEmitterList[i].emitter->Position.x, m_StaticEmitterList[i].emitter->Position.y, m_StaticEmitterList[i].emitter->Position.z);

				// Update the environmentGroup and create one if we don't have one
				if(!m_StaticEmitterList[i].occlusionGroup)
				{
					 m_StaticEmitterList[i].occlusionGroup = CreateStaticEmitterEnvironmentGroup(m_StaticEmitterList[i].emitter, pos);
					 if(m_StaticEmitterList[i].occlusionGroup)
					 {
						 SetStaticEmitterInteriorInfo(i);

#if NA_RADIO_ENABLED
						 // Set the radio to reference the same environmentGroup.  We'll use the m_StaticEmitterList[i].occlusionGroup to maintain the correct refs.
						 m_StaticEmitterList[i].radioEmitter.SetOcclusionGroup(m_StaticEmitterList[i].occlusionGroup);
#endif

						 // We don't always have a sound going, so hold a reference so we don't lose the group when a scanner/radio is trying to play
						 m_StaticEmitterList[i].occlusionGroup->AddSoundReference();
					 }
				}

				if(m_StaticEmitterList[i].occlusionGroup)
				{
					UpdateStaticEmitterEnvironmentGroup(i);
				}

				NA_RADIO_ENABLED_ONLY(f32 opennessCutoff = kVoiceFilterLPFMaxCutoff);

				if(m_StaticEmitterFlags[i].bIsScanner)
				{
#if NA_POLICESCANNER_ENABLED
					if(timeInMs > m_StaticEmitterList[i].lastTriggerTime + 10000)
					{
						g_PoliceScanner.TriggerRandomMedicalReport(pos, m_StaticEmitterList[i].occlusionGroup);							
						m_StaticEmitterList[i].lastTriggerTime = timeInMs;
					}
#endif
				}
				else
				{		
#if NA_RADIO_ENABLED
					if(m_StaticEmitterFlags[i].bIsLinked && m_StaticEmitterList[i].linkedObject)
					{
						Vector3 linkedObjectPos = VEC3V_TO_VECTOR3(m_StaticEmitterList[i].linkedObject->GetTransform().GetPosition());
						m_StaticEmitterList[i].radioEmitter.SetPosition(linkedObjectPos);

						if(m_StaticEmitterList[i].sound)
						{
							m_StaticEmitterList[i].sound->SetRequestedPosition(linkedObjectPos);
						}

						Matrix34 objMat;
						m_StaticEmitterList[i].linkedObject->GetMatrixCopy(objMat);
						Vector3 vDown(0.f,0.f,1.f);
						f32 dp = DotProduct(objMat.b, vDown);
						dp = Max(0.f,dp);
			
						const f32 openness = 1.f - dp;
						opennessCutoff = 4000.f + (kVoiceFilterLPFMaxCutoff - 4000.f) * openness;

						if(m_StaticEmitterList[i].damageSound)
						{
							const f32 minFreq = 2000.f;
							const f32 maxFreq = kVoiceFilterLPFMaxCutoff;
							const f32 freq = minFreq + (openness * (maxFreq - minFreq));
							m_StaticEmitterList[i].damageSound->SetRequestedLPFCutoff((u32)freq);
						}
						
	#if __BANK
						if(sm_DrawStaticEmitterOpennessValues)
						{
							char buf[128];
							formatf(buf, sizeof(buf), "dp: %f open: %f", dp, openness);
							grcDebugDraw::Text(objMat.d,Color32(255,255,0), buf);
						}
	#endif
					}
					else
					{
						m_StaticEmitterList[i].radioEmitter.SetPosition(pos);
					}
#endif // NA_RDIO_ENABLED

					f32 emittedVolume = (f32)m_StaticEmitterList[i].emitter->EmittedVolume * 0.01f;//Convert from mB to dB.
					if(AUD_GET_TRISTATE_VALUE(m_StaticEmitterList[i].emitter->Flags, FLAG_ID_STATICEMITTER_FILLSINTERIORSPACE)==AUD_TRISTATE_TRUE)
					{
						emittedVolume += fillsInteriorSpaceDucking;
					}
					
					if(m_StaticEmitterFlags[i].bIsLinked)
					{
						if(m_StaticEmitterList[i].linkedObject && m_StaticEmitterList[i].linkedObject->GetType() == ENTITY_TYPE_OBJECT)
						{
							const f32 health = ((CObject*)m_StaticEmitterList[i].linkedObject.Get())->GetHealth();
							const f32 howHealthy = health*0.001f;
							const f32 undamagedHealth = m_StaticEmitterList[i].emitter->UndamagedHealth;
							const f32 brokenHealth = m_StaticEmitterList[i].emitter->BrokenHealth;

							if(howHealthy < brokenHealth)
							{
								if(!m_StaticEmitterFlags[i].bWasBrokenLastFrame)
								{
									audSoundInitParams initParams;
									initParams.EnvironmentGroup = m_StaticEmitterList[i].occlusionGroup;
									initParams.Tracker = m_StaticEmitterList[i].linkedObject->GetPlaceableTracker();
									CreateAndPlaySound(m_StaticEmitterList[i].emitter->OnBreakOneShot, &initParams);
									m_StaticEmitterFlags[i].bWasBrokenLastFrame = true;
								}
							}
							else
							{
								m_StaticEmitterFlags[i].bWasBrokenLastFrame = false;
							}

							if(howHealthy > undamagedHealth)
							{
								if(m_StaticEmitterList[i].damageSound)
								{
									// stop playing damage
									m_StaticEmitterList[i].damageSound->StopAndForget();
								}
							}
							else
							{
								if(howHealthy < brokenHealth)
								{
									emittedVolume = -100.f;
									if(m_StaticEmitterList[i].damageSound)
									{
										// stop playing damage
										m_StaticEmitterList[i].damageSound->StopAndForget();
									}
								}
								else if(!m_StaticEmitterList[i].alarmSettings)
								{
									if(timeInMs > m_StaticEmitterList[i].nextDamageFlipTime)
									{
										if(m_StaticEmitterList[i].damageSound)
										{
											// stop playing damage
											m_StaticEmitterList[i].damageSound->StopAndForget();

											// as damage increases, time between damage decreases
											m_StaticEmitterList[i].nextDamageFlipTime = timeInMs + audEngineUtil::GetRandomNumberInRange(50, (s32)(howHealthy * 2000.f));

										}
										else
										{
											if(howHealthy < undamagedHealth)
											{
												audSoundInitParams initParams;
												initParams.EnvironmentGroup = m_StaticEmitterList[i].occlusionGroup;
												initParams.Tracker = m_StaticEmitterList[i].linkedObject->GetPlaceableTracker();
												CreateAndPlaySound_Persistent(g_RadioAudioEntity.GetRadioSounds().Find(ATSTRINGHASH("EmitterDamage", 0x4EFF0991)), 
																				&m_StaticEmitterList[i].damageSound, &initParams);
											}
											m_StaticEmitterList[i].nextDamageFlipTime = timeInMs + audEngineUtil::GetRandomNumberInRange((s32)((1.f-howHealthy) * 2000.f), (s32)(howHealthy * 1000.f));
										}
									}
									if(m_StaticEmitterList[i].damageSound)
									{
										emittedVolume -= 4.f;
									}
								}
							}
						}
					}

#if NA_RADIO_ENABLED
					m_StaticEmitterList[i].radioEmitter.SetEmittedVolume(emittedVolume); 
					m_StaticEmitterList[i].radioEmitter.SetLPFCutoff(Min((u32)m_StaticEmitterList[i].emitter->FilterCutoff,(u32)opennessCutoff));
					m_StaticEmitterList[i].radioEmitter.SetRolloffFactor(m_StaticEmitterList[i].emitter->RolloffFactor * 0.01f);
					m_StaticEmitterList[i].radioEmitter.SetHPFCutoff(m_StaticEmitterList[i].emitter->HPFCutoff);
#endif

					if(!m_StaticEmitterList[i].alarmSettings)
					{
						const bool muteForScore = (AUD_GET_TRISTATE_VALUE(m_StaticEmitterList[i].emitter->Flags, FLAG_ID_STATICEMITTER_MUTEFORSCORE)==AUD_TRISTATE_TRUE);
						if(muteForScore)
						{
							emittedVolume += g_InteractiveMusicManager.GetStaticRadioEmitterVolumeOffset();
						}
					}

					numStaticEmittersPlaying++;
					if(!m_StaticEmitterList[i].sound && fwTimer::GetTimeInMilliseconds() > m_StaticEmitterList[i].lastTriggerTime + g_audMinEmitterRetriggerTime)
					{
						m_StaticEmitterList[i].lastTriggerTime = fwTimer::GetTimeInMilliseconds();

						if(m_StaticEmitterList[i].emitter->Sound && (m_StaticEmitterList[i].emitter->Sound != g_NullSoundHash))
						{
							audSoundInitParams initParams;
							initParams.EnvironmentGroup = m_StaticEmitterList[i].occlusionGroup;

							if(m_StaticEmitterFlags[i].bIsStreamingSound)
							{
								Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
							}
							
							if(!m_StaticEmitterList[i].alarmSettings)
							{
								//Only go up to 80% to avoid wasting disc access.
								initParams.StartOffset = audEngineUtil::GetRandomNumberInRange(0, 80);
								initParams.IsStartOffsetPercentage = true;
							}

							CreateSound_PersistentReference(m_StaticEmitterList[i].emitter->Sound, &m_StaticEmitterList[i].sound, &initParams); 
							if(m_StaticEmitterList[i].sound)
							{
								if( audNorthAudioEngine::GetMicrophones().IsSniping() && AUD_GET_TRISTATE_VALUE(m_StaticEmitterList[i].emitter->Flags, FLAG_ID_STATICEMITTER_IGNORESNIPER)==AUD_TRISTATE_TRUE)
								{
									m_StaticEmitterList[i].sound->SetRequestedListenerMask(1);
								}
								// Wave slots for alarm sounds are prepared externally
								if(m_StaticEmitterList[i].alarmSettings)
								{
									m_StaticEmitterList[i].sound->SetRequestedPosition(pos);
									f32 requestedVol = (f32)m_StaticEmitterList[i].emitter->EmittedVolume / 100.0f; //Convert mB to dB.
									m_StaticEmitterList[i].sound->SetRequestedVolume(requestedVol);
									m_StaticEmitterList[i].sound->SetRequestedLPFCutoff((u32)m_StaticEmitterList[i].emitter->FilterCutoff);		
									m_StaticEmitterList[i].sound->PrepareAndPlay();
								}
								else
								{
									if(m_StaticEmitterFlags[i].bIsStreamingSound)
									{
										audStreamClientSettings settings;
										settings.priority = (u8)(m_StaticEmitterList[i].IsHighPriorityEmitter() ? audStreamSlot::STREAM_PRIORITY_SCRIPT_AMBIENT_EMITTER : audStreamSlot::STREAM_PRIORITY_STATIC_AMBIENT_EMITTER);
										settings.stopCallback = &OnStopCallback;
										settings.hasStoppedCallback = &HasStoppedCallback;
										settings.userData = i;
										m_StaticEmitterList[i].streamSlot = audStreamSlot::AllocateSlot(&settings);
									}

									if(!m_StaticEmitterFlags[i].bIsStreamingSound ||m_StaticEmitterList[i].streamSlot)
									{
										m_StaticEmitterList[i].sound->SetRequestedPosition(pos);
										m_StaticEmitterList[i].sound->SetRequestedVolume(emittedVolume);
										m_StaticEmitterList[i].sound->SetRequestedLPFCutoff((u32)m_StaticEmitterList[i].emitter->FilterCutoff);		

										if(m_StaticEmitterFlags[i].bIsStreamingSound)
										{
											m_StaticEmitterList[i].sound->PrepareAndPlay(m_StaticEmitterList[i].streamSlot->GetWaveSlot(), true, -1);
										}
										else
										{
											m_StaticEmitterList[i].sound->PrepareAndPlay();
										}
									}
									else
									{
										//We don't have a slot to stream from so clean up our sound and try again next frame.
										m_StaticEmitterList[i].sound->StopAndForget();
									}
								}
							}
						}
#if NA_RADIO_ENABLED
						else if(!g_RadioAudioEntity.IsStaticRadioEmitterActive(&(m_StaticEmitterList[i].radioEmitter)))
						{
							const u32 radioStation = GetRadioStationForEmitter(i);
							m_StaticEmitterList[i].radioEmitter.SetIsClubEmitter(m_StaticEmitterList[i].IsClubEmitter());
							m_StaticEmitterList[i].radioEmitter.SetIsHighPriorityEmitter(m_StaticEmitterList[i].IsHighPriorityEmitter());

#if __BANK
							if (m_StaticEmitterList[i].IsClubEmitter())
							{
								audDisplayf("[CLUB EMITTERS] %s is active, attempting to retune to station %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_StaticEmitterList[i].emitter->NameTableOffset), audRadioStation::FindStation(radioStation) ? audRadioStation::FindStation(radioStation)->GetName() : "NULL");
							}
#endif

							//See if we have a radio station to play.
							if(g_RadioAudioEntity.RequestStaticRadio(&(m_StaticEmitterList[i].radioEmitter), radioStation, m_StaticEmitterList[i].GetRequestedPCMChannel()))
							{
								m_StaticEmitterList[i].playingRadioStation = GetRadioStationForEmitter(i);
								m_StaticEmitterFlags[i].bIsPlayingRadio = true;

#if __BANK
								if (m_StaticEmitterList[i].IsClubEmitter())
								{
									audDisplayf("[CLUB EMITTERS] %s retune successful, now playing station %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_StaticEmitterList[i].emitter->NameTableOffset), audRadioStation::FindStation(radioStation) ? audRadioStation::FindStation(m_StaticEmitterList[i].playingRadioStation)->GetName() : "NULL");
								}
#endif
							}
#if __BANK
							else
							{
								if (m_StaticEmitterList[i].IsClubEmitter())
								{
									audDisplayf("[CLUB EMITTERS] %s failed to retune!", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_StaticEmitterList[i].emitter->NameTableOffset));
								}
							}
#endif
						}
#endif // NA_RADIO_ENABLED
					}

					if(m_StaticEmitterList[i].sound)
					{
						m_StaticEmitterList[i].sound->SetRequestedVolume(emittedVolume);
						if( audNorthAudioEngine::GetMicrophones().IsSniping() && AUD_GET_TRISTATE_VALUE(m_StaticEmitterList[i].emitter->Flags, FLAG_ID_STATICEMITTER_IGNORESNIPER)==AUD_TRISTATE_TRUE)
						{
							m_StaticEmitterList[i].sound->SetRequestedListenerMask(1);
						}
					}
				}

				m_StaticEmitterFlags[i].bWasActive = true;
			}
			else if((m_StaticEmitterFlags[i].bIsPlayingRadio || m_StaticEmitterList[i].sound || m_StaticEmitterFlags[i].bIsScanner || m_StaticEmitterFlags[i].bWasActive || m_StaticEmitterList[i].occlusionGroup) 
				&& ShouldStaticEmitterBeStopped(i, gameTimeMinutes))
			{
				if(m_StaticEmitterList[i].sound)
				{
					m_StaticEmitterList[i].sound->StopAndForget();
					//We have finished with our stream slot, so free it.
					audStreamSlot *streamSlot = m_StaticEmitterList[i].streamSlot;
					if(streamSlot)
					{
						streamSlot->Free();
						m_StaticEmitterList[i].streamSlot = NULL;
					}
				}
				else
				{
#if NA_RADIO_ENABLED
#if __BANK
					audRadioStation* oldStation = audRadioStation::FindStation(m_StaticEmitterList[i].playingRadioStation);
					audRadioStation* newStation = audRadioStation::FindStation(GetRadioStationForEmitter(i));
#endif

					g_RadioAudioEntity.StopStaticRadio(&(m_StaticEmitterList[i].radioEmitter));
					m_StaticEmitterFlags[i].bIsPlayingRadio = false;

#if __BANK
					if (m_StaticEmitterList[i].IsClubEmitter())
					{
						audDisplayf("[CLUB EMITTERS] %s was stopped! Station switched %s -> %s, Disabled by script: %s, Missing linked object: %s, Invalid time: %s",
							audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_StaticEmitterList[i].emitter->NameTableOffset),
							oldStation ? oldStation->GetName() : "NULL",
							newStation ? newStation->GetName() : "NULL",
							(AUD_GET_TRISTATE_VALUE(m_StaticEmitterList[i].emitter->Flags, FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT) == AUD_TRISTATE_TRUE) ? "true" : "false",
							(m_StaticEmitterFlags[i].bIsLinked && !m_StaticEmitterList[i].linkedObject) ? "true" : "false",
							(!audAmbientZone::IsValidTime(m_StaticEmitterList[i].emitter->MinTimeMinutes, m_StaticEmitterList[i].emitter->MaxTimeMinutes, gameTimeMinutes)) ? "true" : "false");
					}
#endif
#endif // NA_RADIO_ENABLED
				}

				NA_RADIO_ENABLED_ONLY(m_StaticEmitterList[i].radioEmitter.SetOcclusionGroup(NULL);)
				if(m_StaticEmitterList[i].occlusionGroup)
				{
					m_StaticEmitterList[i].occlusionGroup->RemoveSoundReference();
					m_StaticEmitterList[i].occlusionGroup = NULL;
				}

				m_StaticEmitterFlags[i].bWasActive = false;
			}
		}
		//else if(!m_StaticEmitterFlags[i].bIsEnabled)
		//{
		//	//Ensure we stop and clean up this emitter.
		//	if(m_StaticEmitterList[i].sound)
		//	{
		//		m_StaticEmitterList[i].sound->StopAndForget();
		//		//We have finished with our stream slot, so free it.
		//		audStreamSlot *streamSlot = m_StaticEmitterList[i].streamSlot;
		//		if(streamSlot)
		//		{
		//			streamSlot->Free();
		//			m_StaticEmitterList[i].streamSlot = NULL;
		//		}
		//	}
		//	else
		//	{
		//		g_RadioAudioEntity.StopStaticRadio(&(m_StaticEmitterList[i].radioEmitter));
		//		naEnvironmentGroup *occlusionGroup = m_StaticEmitterList[i].radioEmitter.GetOcclusionGroup();
		//		if(occlusionGroup)
		//		{
		//			occlusionGroup->RemoveSoundReference();
		//			m_StaticEmitterList[i].radioEmitter.SetOcclusionGroup(NULL);
		//		}
		//	}
		//}
	}

	PF_SET(NumStaticEmittersPlaying,numStaticEmittersPlaying);
}

void audEmitterAudioEntity::CheckStaticEmittersForInteriorInfo()
{
	if (g_StaticEmitterNeedsInteriorInfo)
	{
		// look through all our statics, and if they don't have interior info, grab it
		for(u32 i = 0 ; i < m_CurrentStaticEmitterIndex; i++)
		{
			if(m_StaticEmitterFlags[i].bIsEnabled && m_StaticEmitterFlags[i].bNeedsInteriorInfo && !m_StaticEmitterList[i].linkedObject)
			{
				naEnvironmentGroup *occlusionGroup = m_StaticEmitterList[i].occlusionGroup;				
				if(occlusionGroup)
				{
					Vector3 pos(m_StaticEmitterList[i].emitter->Position.x, m_StaticEmitterList[i].emitter->Position.y,
						m_StaticEmitterList[i].emitter->Position.z);
					s32 roomIdx = 0;
					CInteriorInst* pIntInst = NULL;
					CPortalTracker::ProbeForInterior(pos, pIntInst, roomIdx, NULL, 20.0f);
					occlusionGroup->SetInteriorInfoWithInteriorInstance(pIntInst, roomIdx);
					m_StaticEmitterFlags[i].bNeedsInteriorInfo = false;
				}
			}
		}
		g_StaticEmitterNeedsInteriorInfo = false;
	}
}

void audEmitterAudioEntity::UpdateEntityEmitters()
{
	BANK_ONLY(u32 numSoundsPlaying = 0;)
	if(g_AudioEngine.IsAudioEnabled())
	{
		const u32 timeMs = fwTimer::GetTimeInMilliseconds();
		const bool isRaining = (g_weather.GetTimeCycleAdjustedRain() > 0.0f);
		const bool playerSwitchActive = g_PlayerSwitch.IsActive();

		Vec3V listenerPos = g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0);
		if(playerSwitchActive && g_PlayerSwitch.GetSwitchType() == CPlayerSwitchInterface::SWITCH_TYPE_LONG)
		{
			listenerPos = g_PlayerSwitch.GetLongRangeMgr().GetDestPos();
		}

		const bool listenerHasJumped = IsGreaterThan(MagSquared(listenerPos - m_LastListenerPos), ScalarV(Vec::V4VConstantSplat<0x43C80000>())).Getb(); // 400
		m_LastListenerPos = listenerPos;

		u32 jumpCheckTimer = 0;

		for(u32 i = 0 ; i < g_audMaxEntityEmitters; i++)
		{
			if(m_EntityEnabledList.IsSet(i))
			{
#if __BANK
				if(sm_ShouldDrawEntityEmitters)
				{
					if(m_EntityEmitterList[i].sound)
					{
						numSoundsPlaying++;
					}
				}
#endif

				// When the listener jumps in a single frame, force a quick update on any active emitters
				// spread over a few frames if necessary
				if(listenerHasJumped)
				{
					m_EntityCheckTimes[i] = timeMs + jumpCheckTimer++;
				}

				if(timeMs > m_EntityCheckTimes[i])
				{
					if(m_EntityEmitterList[i].sound && m_EntityEmitterList[i].ShouldStopOnRain() && isRaining)
					{
						m_EntityEmitterList[i].sound->StopAndForget();
					}
					bool shouldPlay = ShouldEntityEmitterBePlaying(i, true, playerSwitchActive);
					if(shouldPlay)
					{
						if(!m_EntityEmitterList[i].sound && fwTimer::GetTimeInMilliseconds() > m_EntityEmitterList[i].lastTriggerTime + g_audMinEmitterRetriggerTime)
						{
							Vector3 pos;
							audCompressedQuat orientation;
							orientation.Init();
							GetEntityEmitterPositionAndOrientation(i, pos,orientation);
							m_EntityEmitterList[i].lastTriggerTime = fwTimer::GetTimeInMilliseconds();
						
							naEnvironmentGroup *occlusionGroup = NULL;
							if(m_EntityEmitterList[i].IsOccluded())
							{
								occlusionGroup = CreateEntityEmitterEnvironmentGroup(&m_EntityEmitterList[i], pos, m_EntityEmitterList[i].emitter->MaxPathDepth);
							}
						
							
							CreateSound_PersistentReference(m_EntityEmitterList[i].emitter->Sound, &m_EntityEmitterList[i].sound); 
							
							if(m_EntityEmitterList[i].sound)
							{ 
								m_EntityEmitterList[i].sound->FindAndSetVariableValue(ATSTRINGHASH("randomSeed", 0x69AB332B), m_EntityEmitterList[i].randomSeed);
								
								m_EntityEmitterList[i].sound->GetRequestedSettings()->SetEnvironmentGroup(occlusionGroup);
								m_EntityEmitterList[i].sound->SetClientVariable(i | (1U<<31U));
								m_EntityEmitterList[i].sound->SetRequestedPosition(pos);
								m_EntityEmitterList[i].sound->SetRequestedOrientation(orientation);
								m_EntityEmitterList[i].sound->SetUpdateEntity(true);
								m_EntityEmitterList[i].sound->SetRequestedVolume(ComputeEntityEmitterConeAttenuation(i));
								m_EntityEmitterList[i].sound->PrepareAndPlay();
							}
							else
							{
								if(g_AudioEngine.IsAudioEnabled())
								{
									NOTFINAL_ONLY(naWarningf("Entity Emitter (%s:%u) with invalid sound ref (%u)", audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(m_EntityEmitterList[i].emitter->NameTableOffset), i, m_EntityEmitterList[i].emitter->Sound);)
								}
							}
						}
					}
					else if(!shouldPlay)
					{
						if(m_EntityEmitterList[i].sound)
						{
							m_EntityEmitterList[i].sound->StopAndForget();
						}
					}
				}
			}
		}
	}
	BANK_ONLY(PF_SET(NumEntityEmittersPlaying,numSoundsPlaying);)
}

f32 audEmitterAudioEntity::ComputeEntityEmitterConeAttenuation(u32 emitterIdx)
{
	if(m_EntityEmitterList[emitterIdx].IsConed())
	{
		Matrix34 offset;
		Matrix34 mat;

		// build up matrix for this emitter
		offset.d = m_EntityEmitterList[emitterIdx].offset;
		offset.Identity3x3();

		QuatV uncompressedQuatV;
		m_EntityEmitterList[emitterIdx].rotation.Get(uncompressedQuatV);
		offset.FromQuaternion(QUATV_TO_QUATERNION(uncompressedQuatV));
		
		mat.Set(offset);
		Matrix34 entityMatrix;
		m_EntityEmitterList[emitterIdx].entity->GetMatrixCopy(entityMatrix);
		mat.Dot(entityMatrix);

		// Get the position of the microphone
		// TODO: listener id is hardcoded to 0 at the mo...discuss
		Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0));
		// Turn that into a position relative to the sound source
		Vector3 translatedPosition = listenerPosition - mat.d;

		Vector3 forwards =  mat.b;
		float dotProduct = (translatedPosition.DotV(forwards)).x;
		float lengths = translatedPosition.Mag() * mat.b.Mag();
		float angle = 0.0f;
		if (lengths != 0.0f)
		{
			angle = AcosfSafe(dotProduct/lengths);
		}
		// This'll be in radians, turn it into degrees
		angle *= RtoD;
		if(!naVerifyf(FPIsFinite(angle), "Infinite angle returning 0.f"))
		{
			return 0.0f;
		}
		while (angle<0.0f)
		{
			angle += 360.0f;
		}

		return m_ConeCurve.CalculateRescaledValue(0.0f, m_EntityEmitterList[emitterIdx].emitter->ConeMaxAtten, 
																	m_EntityEmitterList[emitterIdx].emitter->ConeInnerAngle,
																	m_EntityEmitterList[emitterIdx].emitter->ConeOuterAngle, 
																	angle);
	}
	return 0.0f;
}

bool audEmitterAudioEntity::ShouldEntityEmitterBeRegistered(EntityEmitter *emitter, const Vector3& pos)
{
	f32 probability;
	// check time of day
	s32 hour = CClock::GetHour();
	if(hour > 9 && hour < 18)
	{
		probability = emitter->BusinessHoursProb;
	}

	else if(hour > 18 && hour < 23)
	{
		probability = emitter->EveningProb;
	}
	else
	{
		probability = emitter->NightProb;
	}
	if(AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_ENTITYEMITTER_MORELIKELYWHENHOT) == AUD_TRISTATE_TRUE)
	{
		// if the temperature is greater than 20 degrees, then scale probability up by as much as 2
		// up to 40 degrees
		float temp = g_weather.GetTemperature(RCC_VEC3V(pos));
		if(temp >= 20.f)
		{
			f32 scale = Clamp((temp - 20.f) / 20.f,0.0f,1.0f);
			probability *= 1.0f + scale;
		}
	}
	return audEngineUtil::ResolveProbability(probability);
}

u32 audEmitterAudioEntity::GetRadioStationForEmitter(const u32 emitterIdx) const
{
	if(m_StaticEmitterList[emitterIdx].emitter->RadioStationForScore != 0 && g_InteractiveMusicManager.IsMusicPlaying())
	{
		const u32 radioStation = m_StaticEmitterList[emitterIdx].emitter->RadioStationForScore;
		if(radioStation == ATSTRINGHASH("OFF", 0x77E9145) || audRadioStation::FindStation(radioStation))
		{
			return m_StaticEmitterList[emitterIdx].emitter->RadioStationForScore;
		}
	}
	return m_StaticEmitterList[emitterIdx].emitter->RadioStation;
}

bool audEmitterAudioEntity::ShouldStaticEmitterBePlaying(u32 emitterIdx, const bool playerSwitchActive, const bool gameWorldHidden, const u16 gameTimeMinutes)
{
	bool shouldPlay = true;

	if(m_StaticEmitterList[emitterIdx].alarmSettings && !g_AmbientAudioEntity.IsAlarmActive(m_StaticEmitterList[emitterIdx].alarmSettings))
	{
		shouldPlay = false;
	}
	else if(m_StaticEmitterFlags[emitterIdx].bIsPlayingRadio && 
		GetRadioStationForEmitter(emitterIdx) == ATSTRINGHASH("OFF", 0x77E9145))
	{
		shouldPlay = false;
	}
	else if(m_StaticEmitterFlags[emitterIdx].bIsPlayingRadio &&  m_StaticEmitterList[emitterIdx].playingRadioStation != 0 &&
		m_StaticEmitterList[emitterIdx].playingRadioStation != GetRadioStationForEmitter(emitterIdx))
	{				
		// Stop an emitter that is playing the wrong station
		shouldPlay = false;
	}
	else if(m_StaticEmitterFlags[emitterIdx].bIsLinked && !m_StaticEmitterList[emitterIdx].linkedObject)
	{
		shouldPlay = false;
	}

	// Emitter must wait until the distance becomes invalid before it can switch states
	if(!m_StaticEmitterFlags[emitterIdx].bStartStopImmediatelyAtTimeLimits && !m_StaticEmitterFlags[emitterIdx].bIsActive24h)
	{
		bool isValidDistance = true;
		bool isValidTime = true;
		bool isValidOnlyAtSwitchDestination = false;

		u32 packedMinMaxDistance = m_StaticEmitterList[emitterIdx].position.GetWAsUnsignedInt();
		u32 minDistanceSquared = (packedMinMaxDistance >> 16) * (packedMinMaxDistance >> 16);
		u16 maxDistanceSquared = (packedMinMaxDistance & 0xFFFF) * (packedMinMaxDistance & 0xFFFF);

		Vector3 relativePosition = m_StaticEmitterList[emitterIdx].position;
		relativePosition.Subtract(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).d);
		s32 dist2 = static_cast<s32>(relativePosition.Mag2());

		if(dist2 < minDistanceSquared || dist2 > maxDistanceSquared)
		{
			if(!playerSwitchActive)
			{
				isValidDistance = false;
			}
			// If we're doing a camera switch, emitters are valid at the target position too 
			else if(g_PlayerSwitch.GetSwitchType() == CPlayerSwitchInterface::SWITCH_TYPE_LONG)
			{
				relativePosition = m_StaticEmitterList[emitterIdx].position;
				relativePosition.Subtract(VEC3V_TO_VECTOR3(g_PlayerSwitch.GetLongRangeMgr().GetDestPos()));
				dist2 = static_cast<s32>(relativePosition.Mag2());

				if(dist2 < minDistanceSquared || dist2 > maxDistanceSquared)
				{
					isValidDistance = false;
				}
				else
				{
					isValidOnlyAtSwitchDestination = true;
				}
			}
		}

		// If the camera is faded out, we can start emitters freely. Also, emitters active at the far end of a long range switch are allowed to turn on immediately
		if(gameWorldHidden || m_FirstUpdate || isValidOnlyAtSwitchDestination)
		{			
			isValidTime = audAmbientZone::IsValidTime(m_StaticEmitterList[emitterIdx].emitter->MinTimeMinutes, m_StaticEmitterList[emitterIdx].emitter->MaxTimeMinutes, gameTimeMinutes);
			shouldPlay &= isValidTime;
		}
		// If we're outside the max distance, then the valid time parameter is allowed to alter
		if(!isValidDistance)
		{
			isValidTime = audAmbientZone::IsValidTime(m_StaticEmitterList[emitterIdx].emitter->MinTimeMinutes, m_StaticEmitterList[emitterIdx].emitter->MaxTimeMinutes, gameTimeMinutes);
			shouldPlay = false;
		}
		// If not, then use our cached time
		else
		{
			isValidTime = m_StaticEmitterFlags[emitterIdx].bWasValidTime;
			shouldPlay &= isValidTime;
		}

		m_StaticEmitterFlags[emitterIdx].bWasValidTime = isValidTime;
	}
	// Emitter is allowed to switch on/off immediately when the valid time changes
	else if(shouldPlay)
	{
		shouldPlay = audAmbientZone::IsValidTime(m_StaticEmitterList[emitterIdx].emitter->MinTimeMinutes, m_StaticEmitterList[emitterIdx].emitter->MaxTimeMinutes, gameTimeMinutes);

		if(shouldPlay && !m_StaticEmitterList[emitterIdx].isScriptLinkedEmitter)
		{
			u32 packedMinMaxDistance = m_StaticEmitterList[emitterIdx].position.GetWAsUnsignedInt();
			u32 minDistanceSquared = (packedMinMaxDistance >> 16) * (packedMinMaxDistance >> 16);
			u16 maxDistanceSquared = (packedMinMaxDistance & 0xFFFF) * (packedMinMaxDistance & 0xFFFF);

			Vector3 relativePosition = m_StaticEmitterList[emitterIdx].position;
			relativePosition.Subtract(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).d);
			s32 dist2 = static_cast<s32>(relativePosition.Mag2());

			if(dist2 < minDistanceSquared || dist2 > maxDistanceSquared)
			{
				if(!playerSwitchActive)
				{
					shouldPlay = false;
				}
				// If we're doing a camera switch, emitters are valid at the target position too 
				else if(g_PlayerSwitch.GetSwitchType() == CPlayerSwitchInterface::SWITCH_TYPE_LONG
					|| g_PlayerSwitch.GetSwitchType() == CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM)
				{
					relativePosition = m_StaticEmitterList[emitterIdx].position;
					relativePosition.Subtract(VEC3V_TO_VECTOR3(g_PlayerSwitch.GetLongRangeMgr().GetDestPos()));
					dist2 = static_cast<s32>(relativePosition.Mag2());

					if(dist2 < minDistanceSquared || dist2 > maxDistanceSquared)
					{
						shouldPlay = false;
					}
				}
				else
				{
					shouldPlay = false;
				}
			}
		}
	}	

	if(shouldPlay && AUD_GET_TRISTATE_VALUE(m_StaticEmitterList[emitterIdx].emitter->Flags, FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT) == AUD_TRISTATE_TRUE)
	{
		shouldPlay = false;
	}

	return shouldPlay;
}

bool audEmitterAudioEntity::ShouldStaticEmitterBeStopped(u32 emitterIdx, const u16 gameTimeMinutes)
{
	if(m_StaticEmitterList[emitterIdx].alarmSettings &&
		!g_AmbientAudioEntity.IsAlarmActive(m_StaticEmitterList[emitterIdx].alarmSettings))
	{
		return true;
	}
	else if(m_StaticEmitterFlags[emitterIdx].bIsPlayingRadio &&
		GetRadioStationForEmitter(emitterIdx) == ATSTRINGHASH("OFF", 0x77E9145))
	{
		return true;
	}
	else if(m_StaticEmitterFlags[emitterIdx].bIsPlayingRadio && m_StaticEmitterList[emitterIdx].playingRadioStation != 0 &&
			m_StaticEmitterList[emitterIdx].playingRadioStation != GetRadioStationForEmitter(emitterIdx))
	{		
		NOTFINAL_ONLY(audDisplayf("Retuning emitter %s to station %s", audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(m_StaticEmitterList[emitterIdx].emitter->NameTableOffset), audRadioStation::FindStation(GetRadioStationForEmitter(emitterIdx))->GetName());)
		return true;
	}
	else if(m_StaticEmitterFlags[emitterIdx].bIsLinked && !m_StaticEmitterList[emitterIdx].linkedObject)
	{
		return true;
	}
	else if(!audAmbientZone::IsValidTime(m_StaticEmitterList[emitterIdx].emitter->MinTimeMinutes, m_StaticEmitterList[emitterIdx].emitter->MaxTimeMinutes, gameTimeMinutes))
	{
		return true;
	}
	else
	{
		u32 packedMinMaxDistance = m_StaticEmitterList[emitterIdx].position.GetWAsUnsignedInt();
		u16 maxDistance = (packedMinMaxDistance & 0xFFFF);

		Vector3 rel(m_StaticEmitterList[emitterIdx].position);
		rel.Subtract(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).d);
		f32 dist2 = rel.Mag2();

		if(dist2 > ((maxDistance * g_StaticEmitterMaxDistanceScalingForStop) * (maxDistance * g_StaticEmitterMaxDistanceScalingForStop)))
		{
			return true;
		}
	}

	if(AUD_GET_TRISTATE_VALUE(m_StaticEmitterList[emitterIdx].emitter->Flags, FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT) == AUD_TRISTATE_TRUE)
	{
		return true;
	}

	return false;
}

bool audEmitterAudioEntity::ShouldEntityEmitterBePlaying(u32 emitterIdx, bool updateCheckTime, const bool playerSwitchActive)
{
	bool active = true;
	
	if(m_EntityEmitterList[emitterIdx].OnlyWhenRaining())
	{
		if(!g_weather.IsRaining())
		{
			active = false;
		}
	}

	// Hacky fix for url:bugstar:5510148
	if(m_EntityEmitterList[emitterIdx].entity && m_EntityEmitterList[emitterIdx].entity->GetModelNameHash() == ATSTRINGHASH("xs_propintxmas_tree_2018", 0x4665D8A) && !m_EntityEmitterList[emitterIdx].entity->IsVisible()) 
	{
		active = false;
	}

	if(m_EntityEmitterList[emitterIdx].ShouldStopOnRain() && g_weather.GetTimeCycleAdjustedRain() > 0.0f)
	{
		active = false;
	}

	if(audNorthAudioEngine::GetLastLoudSoundTime() + m_EntityEmitterList[emitterIdx].emitter->StopAfterLoudSound > fwTimer::GetTimeInMilliseconds())
	{
		active = false;
	}

	if(m_EntityEmitterList[emitterIdx].emitter->BrokenHealth >= 0.0f)
	{
		if(m_EntityEmitterList[emitterIdx].entity && m_EntityEmitterList[emitterIdx].entity->GetIsTypeObject())
		{
			CObject* object = (CObject*)m_EntityEmitterList[emitterIdx].entity;
			f32 healthFraction = object->GetHealth()/object->GetMaxHealth();

			if(healthFraction <= m_EntityEmitterList[emitterIdx].emitter->BrokenHealth)
			{
				active = false;
			}
		}
	}

	if(active)
	{
		Vector3 emitterPosition;
		GetEntityEmitterPosition(emitterIdx, emitterPosition);
		
		// compute distance to listener
		Vector3 relativePosition = emitterPosition;
		relativePosition.Subtract(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).d);
		f32 dist2 = relativePosition.Mag2();
		const f32 maxDist2 = m_EntityEmitterList[emitterIdx].emitter->MaxDistance * m_EntityEmitterList[emitterIdx].emitter->MaxDistance;
		f32 diff2 = dist2 - maxDist2;
		active = (dist2 <= maxDist2);

		// If we're doing a camera switch, emitters are valid at the target position too 
		if(playerSwitchActive)
		{
			if(g_PlayerSwitch.GetSwitchType() == CPlayerSwitchInterface::SWITCH_TYPE_LONG)
			{
				relativePosition = emitterPosition;
				relativePosition.Subtract(VEC3V_TO_VECTOR3(g_PlayerSwitch.GetLongRangeMgr().GetDestPos()));
				dist2 = relativePosition.Mag2();
				diff2 = dist2 - maxDist2;
				active |= (dist2 <= maxDist2);
			}
		}
		
		if(updateCheckTime)
		{
			// Damage-able items need more frequent updates whilst they're playing
			if((m_EntityEmitterList[emitterIdx].emitter->BrokenHealth >= 0.0f && m_EntityEmitterList[emitterIdx].sound) || m_EntityEmitterList[emitterIdx].frequentUpdates)
			{
				m_EntityCheckTimes[emitterIdx] = fwTimer::GetTimeInMilliseconds() + 250;
			}
			// pick a random next update time, larger if we're further away , +7s if we're playing
			else
			{
				// scale the time to next check by the proximity to the emitter radius
				f32 timeScale = Clamp(diff2 / (100.f*100.f), 0.0f, 1.0f) * 5.f;
				m_EntityCheckTimes[emitterIdx] = fwTimer::GetTimeInMilliseconds() + (u32)(audEngineUtil::GetRandomNumberInRange(250.f + (timeScale * 1250.f),500.f + (timeScale * 3500.f))) + (active?7000:0);
			}
		}
		return active;
	}
	else
	{
		return false;
	}
}

void audEmitterAudioEntity::UpdateLightAudio(CLightEntity* lightEntity, CEntity *entity, const C2dEffect *effect, const bool isLightOn, const u32 slotIndex)
{
	// audio update doesnt run when the game is paused but prerender apparently does.
	if( fwTimer::IsGamePaused())
	{
		return;
	}

	if(!isLightOn)
	{
		return;
	}

	const CLightAttr* l = effect->GetLight();

	if(l)
	{
		static const u16 RandomSeedRandomiser[8] = { 00000, 0064632, 0125525, 0146146,0175757, 0113045, 0052652, 0114315};
		u16 localRandomSeed = (u16) (((size_t)entity) / 4); 
		u32 localRandomSeed2 = (u32) (((size_t)lightEntity) / 256); 
		localRandomSeed ^= RandomSeedRandomiser[localRandomSeed2  % 8];
		const bool isFlashy = ((((localRandomSeed & 255) <= 16) && l->m_flashiness == FL_RANDOM_FLASHINESS) || l->m_flashiness == FL_RANDOM || l->m_flashiness == FL_RANDOM_OVERRIDE_IF_WET);

		if((l->m_flags & LIGHTFLAG_FORCE_BUZZING) || ((l->m_flags & LIGHTFLAG_ENABLE_BUZZING) && isFlashy) || ((l->m_flags & LIGHTFLAG_ENABLE_BUZZING) && (((localRandomSeed>>8) & 255) <= 14)))
		{
			const bool active = !entity->m_nFlags.bRenderDamaged;
			if(active)
			{
				Matrix34 boneMtx;
				CVfxHelper::GetMatrixFromBoneTag(RC_MAT34V(boneMtx), entity, (eAnimBoneTag)l->m_boneTag);

				if (!boneMtx.a.IsZero())
				{
					Vector3 srcpos;
					effect->GetPos(srcpos);
					Vector3	pos = boneMtx * srcpos;	
					// we know that bit 0 is set, so subtract 1 to keep in range [0,3]
					u32 soundIndex = ((localRandomSeed|1) & 3) - 1;
					static const u32 sounds[] = {
						ATSTRINGHASH("AMBIENCE_NEON_NEON_1", 0x0944d782a),
						ATSTRINGHASH("AMBIENCE_NEON_NEON_2", 0x0ac1da7ca),
						ATSTRINGHASH("AMBIENCE_NEON_NEON_3", 0x078203fd0)};
					fwUniqueObjId uniqueId = fwIdKeyGenerator::Get(entity, slotIndex);

					g_AmbientAudioEntity.RegisterEffectAudio(sounds[soundIndex], uniqueId, pos, entity);
				}
			}
		}	
	}
}
void audEmitterAudioEntity::GetEntityEmitterPosition(u32 emitterIdx, Vector3 &pos)
{
	// transform emitter position to world space using entity's matrix
	naAssertf(emitterIdx < g_audMaxEntityEmitters, "Emitter index %d out of bounds", emitterIdx);
	m_EntityEmitterList[emitterIdx].entity->TransformIntoWorldSpace(pos, m_EntityEmitterList[emitterIdx].offset);
}
void audEmitterAudioEntity::GetEntityEmitterPositionAndOrientation(u32 emitterIdx, Vector3 &pos,audCompressedQuat &orientation)
{
	// transform emitter position to world space using entity's matrix
	naAssertf(emitterIdx < g_audMaxEntityEmitters, "Emitter index %d out of bounds", emitterIdx);
	m_EntityEmitterList[emitterIdx].entity->TransformIntoWorldSpace(pos, m_EntityEmitterList[emitterIdx].offset);
	if(m_EntityEmitterList[emitterIdx].entity->GetPlaceableTracker())
	{
		orientation = m_EntityEmitterList[emitterIdx].entity->GetPlaceableTracker()->GetOrientation();
	}
}

void audEmitterAudioEntity::UpdateSound(audSound *UNUSED_PARAM(sound), audRequestedSettings *reqSets, u32 UNUSED_PARAM(timeInMs))
{
	u32 var;
	reqSets->GetClientVariable(var);
	if(var & (1U<<31U))
	{
		Vector3 pos;
		audCompressedQuat orientation;
		orientation.Init();

		u32 idx = var & ~(1U<<31U);
		GetEntityEmitterPositionAndOrientation(idx, pos, orientation);
		reqSets->SetPosition(pos);
		reqSets->SetOrientation(orientation);
		f32 coneVol = ComputeEntityEmitterConeAttenuation(idx);
		reqSets->SetVolume(coneVol);
	}
}

void audEmitterAudioEntity::NotifyLoudSound()
{
	// only do this once per frame
	static u32 frame = 0;
	if(g_AudioEngine.IsAudioEnabled() && frame != fwTimer::GetSystemFrameCount())
	{
		frame = fwTimer::GetSystemFrameCount();
		for(u32 i = 0 ; i < g_audMaxEntityEmitters; i++)
		{
			if(m_EntityEnabledList.IsSet(i) && m_EntityEmitterList[i].sound && m_EntityEmitterList[i].emitter->StopAfterLoudSound > 0)
			{
				m_EntityEmitterList[i].sound->StopAndForget();
			}
		}
	}
}

naEnvironmentGroup* audEmitterAudioEntity::CreateEntityEmitterEnvironmentGroup(EntityEmitterEntry* entityEmitter, Vector3& pos, u8 maxPathDepth)
{
	// Create a small occlusion group - try it with very little leakage, so they pop up quickly, as they're a very small visual thing.

	naEnvironmentGroup* environmentGroup = NULL;
	environmentGroup = naEnvironmentGroup::Allocate("EntityEmitter");
	if (environmentGroup)
	{
		// NULL entity; will be deleted when the sound is deleted
		environmentGroup->Init(NULL, 20);
		environmentGroup->SetSource(VECTOR3_TO_VEC3V(pos));
		environmentGroup->SetInteriorInfoWithEntity(entityEmitter->entity);
		if(audNorthAudioEngine::GetOcclusionManager()->GetIsPortalOcclusionEnabled())
		{
			environmentGroup->SetUsePortalOcclusion(true);
			environmentGroup->SetMaxPathDepth(maxPathDepth);
		}
	}
	return environmentGroup;
}

naEnvironmentGroup* audEmitterAudioEntity::CreateStaticEmitterEnvironmentGroup(const StaticEmitter* emitter, const Vector3& pos)
{
	naEnvironmentGroup* environmentGroup = NULL;

	if(naVerifyf(emitter, "Passed NULL StaticEmitter* when creating an environmentGroup"))
	{
		environmentGroup = naEnvironmentGroup::Allocate("StaticEmitter");
		if (environmentGroup)
		{
			environmentGroup->Init(NULL, 20, 0, 4000, 0.5f, 50);
			environmentGroup->SetSource(VECTOR3_TO_VEC3V(pos));
			if(audNorthAudioEngine::GetOcclusionManager()->GetIsPortalOcclusionEnabled())
			{
				environmentGroup->SetUsePortalOcclusion(true);
				environmentGroup->SetMaxPathDepth(emitter->MaxPathDepth);
				environmentGroup->SetForceMaxPathDepth(AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_STATICEMITTER_FORCEMAXPATHDEPTH) == AUD_TRISTATE_TRUE);
			}
			environmentGroup->SetLeaksInTaggedCutscenes(true);
			environmentGroup->SetMaxLeakage(emitter->MaxLeakage);
			environmentGroup->SetMinLeakageDistance(emitter->MinLeakageDistance);
			environmentGroup->SetMaxLeakageDistance(emitter->MaxLeakageDistance);
			environmentGroup->SetReverbOverrideSmall(emitter->SmallReverbSend);
			environmentGroup->SetReverbOverrideMedium(emitter->MediumReverbSend);
			environmentGroup->SetReverbOverrideLarge(emitter->LargeReverbSend);
			environmentGroup->SetFillsInteriorSpace(AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_STATICEMITTER_FILLSINTERIORSPACE) == AUD_TRISTATE_TRUE);
		}
	}
	return environmentGroup;
}

void audEmitterAudioEntity::SetStaticEmitterInteriorInfo(const u32 i)
{
	// Set the interior information on the environmentGroup.  If we have an entity then use that.
	if(m_StaticEmitterList[i].occlusionGroup && m_StaticEmitterList[i].emitter)
	{
		bool useGOInteriorInfo = true;
		if(m_StaticEmitterFlags[i].bIsLinked)
		{
			if(m_StaticEmitterList[i].linkedObject && m_StaticEmitterList[i].linkedObject->GetIsTypeObject())
			{
				m_StaticEmitterList[i].occlusionGroup->ForceSourceEnvironmentUpdate(m_StaticEmitterList[i].linkedObject);
				useGOInteriorInfo = false;
			}
		}

		if(useGOInteriorInfo)
		{
			if(m_StaticEmitterList[i].emitter->InteriorSettings 
				&& naVerifyf(m_StaticEmitterList[i].emitter->InteriorRoom, "We have InteriorSettings set on a StaticEmitter, but no InteriorRoom information"))
			{
				naAssertf(!m_StaticEmitterFlags[i].bIsLinked, "NULL or Non-Object entity for linked StaticEmitter, falling back to GO InteriorSettings/InteriorRoom");
				m_StaticEmitterList[i].occlusionGroup->SetInteriorInfoWithInteriorHashkey(m_StaticEmitterList[i].emitter->InteriorSettings, m_StaticEmitterList[i].emitter->InteriorRoom);
			}
			else
			{
				// Set up our occlusion group to search for interior info. This is presumably relatively expensive, so only do it on creation
				// We can't do this synchronously, because it involves line-checks on the game-audio tread - do a global flag that we need updated,
				// and then we'll trawl the list on the next frame.
				naAssertf(!m_StaticEmitterFlags[i].bIsLinked, "NULL or Non-Object entity and no InteriorSettings/InteriorRoom for linked StaticEmitter, searching for room info");
				g_StaticEmitterNeedsInteriorInfo = true;
				m_StaticEmitterFlags[i].bNeedsInteriorInfo = true;
			}
		}
	}
}

void audEmitterAudioEntity::UpdateStaticEmitterEnvironmentGroup(const u32 i)
{
	naEnvironmentGroup* environmentGroup = m_StaticEmitterList[i].occlusionGroup;
	if(environmentGroup)
	{
		if (m_StaticEmitterList[i].nameHash == ATSTRINGHASH("SE_DLC_Hei4_Island_Beach_Party_Music_New_01_Left", 0x3C8BE84A) ||
			m_StaticEmitterList[i].nameHash == ATSTRINGHASH("SE_DLC_Hei4_Island_Beach_Party_Music_New_02_Right", 0xF796B9AD))
		{
			f32 beachPartyInfluenceRatio = 0.f;

			for (u32 i = 0; i < g_AmbientAudioEntity.GetNumActiveZones(); i++)
			{
				const audAmbientZone* zone = g_AmbientAudioEntity.GetActiveZone(i);

				if (zone->GetNameHash() == ATSTRINGHASH("AZ_DLC_Hei4_Island_Beach_Party_Music_Leakage_Bounds", 0xDD7E1146))
				{
					beachPartyInfluenceRatio = zone->ComputeZoneInfluenceRatioAtPoint(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
				}
			}
			
			environmentGroup->SetForcedSameRoomAsListenerRatio(beachPartyInfluenceRatio);
		}

		// I guess keep checking to update interior information if it's a linked entity, as the entity streams in and out.
		// But StaticEmitters don't move, so we shouldn't be updating interior information every frame, just on creation.
		if(m_StaticEmitterFlags[i].bIsLinked && m_StaticEmitterList[i].linkedObject && m_StaticEmitterList[i].linkedObject->GetIsTypeObject())
		{
			environmentGroup->ForceSourceEnvironmentUpdate(m_StaticEmitterList[i].linkedObject);
		}

#if __BANK
		const StaticEmitter* emitter = m_StaticEmitterList[i].emitter;

		if(g_DebugStaticEmitterEnvironmentGroups && emitter)
		{
			environmentGroup->SetMaxLeakage(emitter->MaxLeakage);
			environmentGroup->SetMinLeakageDistance(emitter->MinLeakageDistance);
			environmentGroup->SetMaxLeakageDistance(emitter->MaxLeakageDistance);
			environmentGroup->SetReverbOverrideSmall(emitter->SmallReverbSend);
			environmentGroup->SetReverbOverrideMedium(emitter->MediumReverbSend);
			environmentGroup->SetReverbOverrideLarge(emitter->LargeReverbSend);
			if(AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_STATICEMITTER_FILLSINTERIORSPACE)==AUD_TRISTATE_TRUE)
			{
				environmentGroup->SetFillsInteriorSpace(true);
			}
			else
			{
				environmentGroup->SetFillsInteriorSpace(false);
			}

			SetStaticEmitterInteriorInfo(i);
		}
#endif
	}
}

void audEmitterAudioEntity::StopEmitter(u32 emitterIndex)
{
#if __BANK
	if (m_StaticEmitterList[emitterIndex].IsClubEmitter())
	{
		audDisplayf("[CLUB EMITTERS] %s is being stopped via ::StopEmitter", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_StaticEmitterList[emitterIndex].emitter->NameTableOffset));
	}
#endif

	audSound *emitterSound = m_StaticEmitterList[emitterIndex].sound;
	if(emitterSound)
	{
		emitterSound->StopAndForget();

		//We have finished with our stream slot, so free it.
		audStreamSlot *streamSlot = m_StaticEmitterList[emitterIndex].streamSlot;
		if(streamSlot)
		{
			streamSlot->Free();
			m_StaticEmitterList[emitterIndex].streamSlot = NULL;
		}
	}

	g_RadioAudioEntity.StopStaticRadio(&(m_StaticEmitterList[emitterIndex].radioEmitter));
	m_StaticEmitterFlags[emitterIndex].bIsPlayingRadio = false;
}

bool audEmitterAudioEntity::IsEmitterActive(u32 emitterIndex)
{
	return (m_StaticEmitterList[emitterIndex].sound) != NULL;
}

bool audEmitterAudioEntity::OnStopCallback(u32 emitterIndex)
{
	//We are losing our stream slot, so stop this emitter.
	g_EmitterAudioEntity.StopEmitter(emitterIndex);
	return true;
}

bool audEmitterAudioEntity::HasStoppedCallback(u32 emitterIndex)
{
	return !g_EmitterAudioEntity.IsEmitterActive(emitterIndex);
}

void audEmitterAudioEntity::SetEmitterRadioStation(const char *emitterName, const char *radioStationName)
{
	StaticEmitter *emitter = audNorthAudioEngine::GetObject<StaticEmitter>(emitterName);
	if(audVerifyf(emitter, "Failed to find static emitter %s", emitterName))
	{
		const u32 nameHash = atStringHash(radioStationName);
		if(audVerifyf(nameHash == ATSTRINGHASH("OFF", 0x77E9145) || audRadioStation::FindStation(radioStationName), "Invalid radio station name: %s (trying to retune emitter %s)", radioStationName, emitterName))
		{
			audDisplayf("Retuning %s to %s", emitterName, radioStationName);
			emitter->RadioStation = nameHash;
		}
	}
}

void audEmitterAudioEntity::SetEmitterLinkedObject(const char *emitterName, CEntity* entity, bool isScriptLinkedEmitter)
{
	StaticEmitter *emitter = audNorthAudioEngine::GetObject<StaticEmitter>(emitterName);
	if(audVerifyf(emitter, "Failed to find static emitter %s", emitterName))
	{
		for(u32 loop = 0; loop < m_CurrentStaticEmitterIndex; loop++)
		{
			if(m_StaticEmitterList[loop].emitter == emitter)
			{
				m_StaticEmitterList[loop].linkedObject = entity;
				m_StaticEmitterList[loop].isScriptLinkedEmitter = isScriptLinkedEmitter;

				if(isScriptLinkedEmitter)
				{
					UpdateScriptPositionedEmitter(loop);					
				}				
				return;
			}
		}
	}
}

void audEmitterAudioEntity::UpdateScriptPositionedEmitter(u32 emitterIndex)
{
	if(m_StaticEmitterList[emitterIndex].linkedObject)
	{		
		const Vector3 objectPosition = VEC3V_TO_VECTOR3(m_StaticEmitterList[emitterIndex].linkedObject->GetTransform().GetPosition());
		m_StaticEmitterList[emitterIndex].position = objectPosition;
		m_StaticEmitterList[emitterIndex].emitter->Position.x = objectPosition.x;
		m_StaticEmitterList[emitterIndex].emitter->Position.y = objectPosition.y;
		m_StaticEmitterList[emitterIndex].emitter->Position.z = objectPosition.z;

		if(m_StaticEmitterList[emitterIndex].occlusionGroup)
		{
			m_StaticEmitterList[emitterIndex].occlusionGroup->SetSource(VECTOR3_TO_VEC3V(objectPosition));
		}
	}	
}

bool audEmitterAudioEntity::SetEmitterEnabled(const char *emitterName, const bool enabled)
{
	StaticEmitter *emitter = audNorthAudioEngine::GetObject<StaticEmitter>(emitterName);
	if(audVerifyf(emitter, "Failed to find static emitter %s", emitterName))
	{
		if(enabled && AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT) == AUD_TRISTATE_TRUE)
		{
			audDisplayf("Enabling static emitter %s", emitterName);
			emitter->Flags &= ~(0x03 << (FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT * 2));
			AUD_SET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT, AUD_TRISTATE_FALSE);
			return true;
		}
		else if(!enabled && AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT) != AUD_TRISTATE_TRUE)
		{
			audDisplayf("Disabling static emitter %s", emitterName);
			emitter->Flags &= ~(0x03 << (FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT * 2));
			AUD_SET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT, AUD_TRISTATE_TRUE);
			return true;
		}
	}

	return false;
}

#if __BANK
char g_EmitterStationNameBuf[64]={0};
void audEmitterAudioEntity::DrawDebug()
{
	const bool playerSwitchActive = g_PlayerSwitch.IsActive();
	const bool gameWorldHidden = camInterface::IsFadedOut() || CLoadingScreens::AreActive();
	const u16 gameTimeMinutes = static_cast<u16>((CClock::GetHour() * 60) + CClock::GetMinute());
	const bool filterNames = strlen(g_DrawStaticEmitterFilter) > 0;

	if(sm_ShouldDrawStaticEmitters)
	{
		for(u32 i = 0; i < m_CurrentStaticEmitterIndex; i++)
		{
			const char *emitterName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_StaticEmitterList[i].emitter->NameTableOffset);

			if(filterNames && !stristr(emitterName, g_DrawStaticEmitterFilter))
			{
				continue;
			}

			if(m_StaticEmitterFlags[i].bIsEnabled)
			{
				Color32 color, sphereColor;
				Vector3 pos = Vector3(m_StaticEmitterList[i].emitter->Position.x,m_StaticEmitterList[i].emitter->Position.y,m_StaticEmitterList[i].emitter->Position.z);

				if(ShouldStaticEmitterBePlaying(i, playerSwitchActive, gameWorldHidden, gameTimeMinutes))
				{
					color = sphereColor = Color32(1.0f,0.0f,0.0f,0.8f);
				}
				else
				{
					color = sphereColor = Color32(1.0f,0.3f,0.3f,0.5f);
				}

				grcDebugDraw::Sphere(pos, 0.5f, color, true);
				grcDebugDraw::Sphere(pos, m_StaticEmitterList[i].emitter->MaxDistance, color, false);

				char nameString[128];
				const char *assetName = "";
				if(m_StaticEmitterList[i].sound)
				{
					assetName = m_StaticEmitterList[i].sound->GetName();
				}
				else
				{
					audRadioStation *station = audRadioStation::FindStation(m_StaticEmitterList[i].playingRadioStation);
					if(station)
					{
						assetName = station->GetName();
					}
				}

				if(m_StaticEmitterFlags[i].bIsLinked && m_StaticEmitterList[i].linkedObject)
				{
					f32 health = -1.f;
					if(m_StaticEmitterList[i].linkedObject && m_StaticEmitterList[i].linkedObject->GetType() == ENTITY_TYPE_OBJECT)
					{
						health = ((CObject*)m_StaticEmitterList[i].linkedObject.Get())->GetHealth();
					}

					grcDebugDraw::Line(RCC_VEC3V(pos), m_StaticEmitterList[i].linkedObject->GetTransform().GetPosition(), Color32(255,0,0), Color32(0,255,0));
					formatf(nameString, "%s: %s%s\nlinkedObj: %p\nhealth: %f", emitterName, assetName, AUD_GET_TRISTATE_VALUE(m_StaticEmitterList[i].emitter->Flags, FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT) == AUD_TRISTATE_TRUE ? " (disabled)" : "", m_StaticEmitterList[i].linkedObject.Get(), health);
				}
				else
				{
					formatf(nameString, "%s: %s%s", emitterName, assetName, AUD_GET_TRISTATE_VALUE(m_StaticEmitterList[i].emitter->Flags, FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT) == AUD_TRISTATE_TRUE ? " (disabled)" : "");
				}
				
				grcDebugDraw::Text(pos, color, nameString);
			}
		}
	}

	char buf[256];

	if(sm_ShouldDrawEntityEmitters)
	{
		for(u32 i = 0; i < g_audMaxEntityEmitters; i++)
		{
			if(m_EntityEnabledList.IsSet(i))
			{
				const char *name = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_EntityEmitterList[i].emitter->NameTableOffset);

				if(filterNames && !stristr(name, g_DrawStaticEmitterFilter))
				{
					continue;
				}

				Color32 color;
				const f32 r = 1.0f - m_EntityEmitterList[i].randomSeed;
				const f32 g = m_EntityEmitterList[i].randomSeed;
				const bool shouldPlay = ShouldEntityEmitterBePlaying(i, false, playerSwitchActive);
				if(shouldPlay)
				{
					color = Color32(r,g,1.0f,1.0f);
				}
				else
				{
					color = Color32(r,g,1.0f,0.5f);
				}

				if(m_EntityEmitterList[i].IsWindAffected())
				{
					color = Color32(color.GetBlue(), color.GetGreen(), color.GetRed());
				}

				Vector3 pos;
				GetEntityEmitterPosition(i, pos);

				Vector3 v1, v2, v3;

				v1.x = pos.x - 0.5f;
				v1.y = pos.y;
				v1.z = pos.z - 0.5f;

				v2.x = pos.x + 0.5f;
				v2.y = pos.y;
				v2.z = pos.z - 0.5f;

				v3.x = pos.x;
				v3.y = pos.y;
				v3.z = pos.z + 0.5f;

				grcDebugDraw::Poly(RCC_VEC3V(v1),RCC_VEC3V(v2),RCC_VEC3V(v3),color);
				grcDebugDraw::Poly(RCC_VEC3V(v3),RCC_VEC3V(v2),RCC_VEC3V(v1),color);

				if(m_EntityEmitterList[i].IsConed())
				{
					Matrix34 offset;
					Matrix34 mat;

					offset.d = m_EntityEmitterList[i].offset;
					offset.Identity3x3();

					QuatV uncompressedQuatV;
					m_EntityEmitterList[i].rotation.Get(uncompressedQuatV);
					offset.FromQuaternion(QUATV_TO_QUATERNION(uncompressedQuatV));

					mat.Set(offset);
					Matrix34 entityMatrix;
					m_EntityEmitterList[i].entity->GetMatrixCopy(entityMatrix);
					mat.Dot(entityMatrix);

					Vector3 end = mat.d + mat.b;
					grcDebugDraw::Line(mat.d, end, color);
				}				

				// compute distance to listener
				Vector3 listenerToPos = pos;
				listenerToPos.Subtract(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).d);
				f32 dist = listenerToPos.Mag();

				u32 timeToCheck = m_EntityCheckTimes[i] - fwTimer::GetTimeInMilliseconds();

				if(dist < 50.f)
				{
					f32 healthFraction = 1.0f;

					if(m_EntityEmitterList[i].emitter->BrokenHealth >= 0.0f)
					{
						if(m_EntityEmitterList[i].entity && m_EntityEmitterList[i].entity->GetIsTypeObject())
						{
							CObject* object = (CObject*)m_EntityEmitterList[i].entity;
							healthFraction = object->GetHealth()/object->GetMaxHealth();
						}
					}

					if(m_EntityEmitterList[i].IsConed())
					{
						formatf(buf, sizeof(buf), "ca: %.2f d: %f ttc: %u [%u] rs: %f health: %f", ComputeEntityEmitterConeAttenuation(i), dist, timeToCheck, m_EntityCheckTimes[i], m_EntityEmitterList[i].randomSeed, healthFraction);
					}
					else
					{
						formatf(buf, sizeof(buf), "d: %.2f ttc: %u [%u] rs: %f health: %f", dist, timeToCheck, m_EntityCheckTimes[i], m_EntityEmitterList[i].randomSeed, healthFraction);
					}
					grcDebugDraw::Text(pos, color, buf);
				}
				if(dist < 400.f)
				{
					grcDebugDraw::Text(pos + Vector3(0.f,0.f,1.f), color, const_cast<char*>(name));
				}
			}
		}

		for(u32 i = 0; i < m_EntityEmitterDebugInfo.GetCount(); i++)
		{
			bool emitterEnabled = false;

			for(u32 j = 0; j < g_audMaxEntityEmitters; j++)
			{
				if(m_EntityEnabledList.IsSet(j))
				{
					if(m_EntityEmitterDebugInfo[i].entity == m_EntityEmitterList[i].entity)
					{
						emitterEnabled = true;
						break;
					}
				}				
			}

			// Draw extra entries for emitters that exist but aren't explicitly linked to a static emitter
			if(!emitterEnabled)
			{
				const char* name = audNorthAudioEngine::GetMetadataManager().GetObjectName(m_EntityEmitterDebugInfo[i].hash);

				if(name)
				{
					if(filterNames && !stristr(name, g_DrawStaticEmitterFilter))
					{
						continue;
					}

					if(CEntity* pEntity = m_EntityEmitterDebugInfo[i].entity)
					{
						Color32 color = Color_red;				
						Vector3 pos;
						pEntity->TransformIntoWorldSpace(pos, m_EntityEmitterDebugInfo[i].offset);

						Vector3 v1, v2, v3;

						v1.x = pos.x - 0.5f;
						v1.y = pos.y;
						v1.z = pos.z - 0.5f;

						v2.x = pos.x + 0.5f;
						v2.y = pos.y;
						v2.z = pos.z - 0.5f;

						v3.x = pos.x;
						v3.y = pos.y;
						v3.z = pos.z + 0.5f;

						grcDebugDraw::Poly(RCC_VEC3V(v1),RCC_VEC3V(v2),RCC_VEC3V(v3),color);
						grcDebugDraw::Poly(RCC_VEC3V(v3),RCC_VEC3V(v2),RCC_VEC3V(v1),color);

						// compute distance to listener
						Vector3 listenerToPos = pos;
						listenerToPos.Subtract(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).d);
						f32 dist = listenerToPos.Mag();

						if(dist < 400.f)
						{
							for(u32 j = 0; j < m_CurrentStaticEmitterIndex; j++)
							{
								if(m_StaticEmitterFlags[j].bIsLinked && m_StaticEmitterList[j].linkedObject && m_StaticEmitterList[j].linkedObject == m_EntityEmitterDebugInfo[i].entity)
								{
									const f32 r = 1.0f - m_EntityEmitterList[i].randomSeed;
									const f32 g = m_EntityEmitterList[i].randomSeed;
									formatf(buf, sizeof(buf), "%s (%s)", const_cast<char*>(name), m_StaticEmitterList[j].linkedObject->GetModelName());
									grcDebugDraw::Text(pos, Color32(r,g,1.0f,1.0f), buf);
									emitterEnabled = true;
									break;
								}
							}

							if(!emitterEnabled)
							{
								formatf(buf, sizeof(buf), "%s (UNLINKED)", const_cast<char*>(name));
								grcDebugDraw::Text(pos, color, buf);
							}
						}
					}
				}
			}
		}
	}
}

void RetuneEmitter()
{
	g_EmitterAudioEntity.SetEmitterRadioStation(g_DrawStaticEmitterFilter, g_EmitterStationNameBuf);
}

bool g_DebugEmitterEnabled = false;
void SetEnabledEmitter()
{
	g_EmitterAudioEntity.SetEmitterEnabled(g_DrawStaticEmitterFilter, g_DebugEmitterEnabled);
}

extern bool g_WarpToStaticEmitter;
void audEmitterAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("audEmitterAudioEntity",false);
		bank.AddToggle("WarpToStaticEmitter", &g_WarpToStaticEmitter);
		bank.AddToggle("Draw Entity Emitters", &sm_ShouldDrawEntityEmitters);
		bank.AddToggle("Draw Static Emitters", &sm_ShouldDrawStaticEmitters);
		bank.AddToggle("Draw Static Emitter Openness Values", &sm_DrawStaticEmitterOpennessValues);
		bank.AddText("Filter by Name", g_DrawStaticEmitterFilter, sizeof(g_DrawStaticEmitterFilter), false);
		
		bank.AddSlider("Space Filling Interior Ducking Volume (linear)", &g_SpaceFillingInteriorDuckingVolumeLin, 0.0f, 1.0, 0.1f);
		bank.AddSlider("Interior Ducking Smoother Rate", &g_EmitterInteriorDuckingSmootherRate, 0.0001f, 10.f, 0.1f);
		
		bank.AddText("Radio Station Name", g_EmitterStationNameBuf, sizeof(g_EmitterStationNameBuf),false);
		bank.AddButton("Retune emitter", CFA(RetuneEmitter));
		bank.AddToggle("Enabled?", &g_DebugEmitterEnabled);
		bank.AddButton("SetEnabled", CFA(SetEnabledEmitter));

		bank.AddText("Emitter Name", g_CreateEmitterName, sizeof(g_CreateEmitterName));
		bank.AddButton("Create New Static Emitter At Camera Coords", CFA(audAmbientAudioEntity::CreateEmitterAtCurrentCoords));
		bank.AddButton("Move Existing Static Emitter To Camera Coords", CFA(audAmbientAudioEntity::MoveEmitterToCurrentCoords));
		bank.AddText("Emitter To Duplicate", g_DuplicateEmitterName, sizeof(g_DuplicateEmitterName));
		bank.AddButton("Create Duplicate At Current Coords", CFA(audAmbientAudioEntity::DuplicateEmitterAtCurrentCoords));

		bank.AddToggle("UpdateStaticEmitterEnvGroupsEveryFrame", &g_DebugStaticEmitterEnvironmentGroups);
	bank.PopGroup();
}

void audEmitterAudioEntity::HandleRaveStaticEmitterCreatedNotification(u32 nameHash)
{
	CreateOrUpdateEmitter(nameHash);
}

void audEmitterAudioEntity::RetuneEmittersToStation(audRadioStation* station)
{
	if (station)
	{
		for (u32 loop = 0; loop < m_CurrentStaticEmitterIndex; loop++)
		{
			if (m_StaticEmitterList[loop].playingRadioStation != 0u)
			{
				g_RadioAudioEntity.RequestStaticRadio(&(m_StaticEmitterList[loop].radioEmitter), station->GetNameHash(), m_StaticEmitterList[loop].GetRequestedPCMChannel());
			}
		}
	}
}

void audEmitterAudioEntity::CreateOrUpdateEmitter(u32 nameHash)
{
	StaticEmitter* emitter = audNorthAudioEngine::GetObject<StaticEmitter>(nameHash);

	if (emitter)
	{
		for (u32 loop = 0; loop < m_CurrentStaticEmitterIndex; loop++)
		{
			if (m_StaticEmitterList[loop].emitter == emitter)
			{
				UpdateStaticEmitterPosition(emitter);
				return;
			}
		}

		const u16 gameTimeMinutes = static_cast<u16>((CClock::GetHour() * 60) + CClock::GetMinute());
		AddStaticEmitter(nameHash, gameTimeMinutes);
	}
}
#endif
