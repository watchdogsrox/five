// rage
#include "audioengine/controller.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audiosoundtypes/sound.h"
#include "audioengine/engineutil.h"
#include "audiohardware/driverutil.h"
#include "cloth/environmentcloth.h"
#include "fragment/cache.h"
#include "grcore/debugdraw.h"
#include "pheffects/wind.h"

// game
#include "control/replay/replay.h"
#include "objectaudioentity.h"
#include "objects/object.h"
#include "weatheraudioentity.h"
#include "physics/gtaInst.h"

#include "debugaudio.h"

#include "audioentity.h"

AUDIO_OPTIMISATIONS()
//OPTIMISATIONS_OFF()


#if __BANK
f32 audObjectAudioEntity::sm_OverriddenWindStrength = 0.f;
bool audObjectAudioEntity::sm_UseInterpolatedWind = false;
bool audObjectAudioEntity::sm_FindCloth = false;
bool audObjectAudioEntity::sm_ShowClothPlaying = false;
bool audObjectAudioEntity::sm_OverrideWind = false;
#endif

f32 audObjectAudioEntity::sm_SqdDistanceThreshold = 225.f;

audObjectAudioEntity::audObjectAudioEntity():
m_ClothSound(NULL),
m_Object(NULL)
{
}

audObjectAudioEntity::~audObjectAudioEntity()
{
	if(m_ClothSound)
	{
		m_ClothSound->StopAndForget();
	}
}

void audObjectAudioEntity::Init(CObject *object)
{
	naAudioEntity::Init();
	audEntity::SetName("audObjectAudioEntity");
	naAssertf(object,"Null environment cloth in audObjectAudioEntity::Init");
	m_Object = object;
	CBaseModelInfo* pModelInfo = m_Object->GetBaseModelInfo();
	if(pModelInfo)
	{
		const ModelAudioCollisionSettings *settings = pModelInfo->GetAudioCollisionSettings();
		if(settings)
		{
			m_WindClothSounds.Init(settings->WindSounds);
		}
	}
	m_LastTimeTriggered = 0;
}


void audObjectAudioEntity::PreUpdateService(u32 /*timeInMs*/)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && (CReplayMgr::ShouldHideMapObject(m_Object) || CReplayMgr::IsPreCachingScene()))
	{
		return;
	}
#endif //GTA_REPLAY

	// Don't process this when we're playing a replay clip and the object is a map object that has been hidden by the replay system.
	if(m_Object->isRageEnvCloth())
	{
		f32 windStrength = g_WeatherAudioEntity.GetWindStrength();

#if __BANK
		if(sm_OverrideWind)
		{
			windStrength = sm_OverriddenWindStrength;
		}
#endif

		bool tunableCloth = false;

#if GTA_REPLAY
		// Not always valid to query fragInstGta in replays, rely on recorded cloth sounds instead
		if(!(CReplayMgr::IsEditModeActive() || CReplayMgr::IsEditorActive() || CReplayMgr::IsReplayInControlOfWorld()))
#endif
		{
			fragInstGta* pFragInst = (fragInstGta*)m_Object->GetCurrentPhysicsInst();
			Assertf(pFragInst, "Failed to allocate new fragInstGta.");
			if(pFragInst)
			{
				// Hoop jumping cloth setup.
				fragCacheEntry *cacheEntry = pFragInst->GetCacheEntry();
				if( cacheEntry )
				{
					fragHierarchyInst* hierInst = cacheEntry->GetHierInst();
					Assert( hierInst );
					environmentCloth * cloth = hierInst->envCloth;
					if( cloth && cloth->GetFlag(environmentCloth::CLOTH_IsInInterior))
					{
						if(cloth->GetFlag(environmentCloth::CLOTH_IsMoving) && !m_ClothSound && m_LastTimeTriggered + 1000 < g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
						{
							if(m_WindClothSounds.IsInitialised())
							{
								u32 soundNameHash = ATSTRINGHASH("CLOTH", 0x1951F94C);
								m_LastTimeTriggered = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
								audSoundInitParams initParams; 
								initParams.Tracker = m_Object->GetPlaceableTracker();
								initParams.EnvironmentGroup = CreateClothEnvironmentGroup();
								CreateAndPlaySound_Persistent(m_WindClothSounds.Find(soundNameHash),&m_ClothSound,&initParams);

#if GTA_REPLAY
								if(m_ClothSound && CReplayMgr::ShouldRecord())
								{
									CReplayMgr::ReplayRecordSoundPersistant(m_WindClothSounds.GetNameHash(), soundNameHash, &initParams, m_ClothSound, m_Object);
								}
#endif
							}
							if(m_ClothSound)
							{
								cloth->SetFlag(environmentCloth::CLOTH_IsMoving,false);
							}
						}
						else if ( !cloth->GetFlag(environmentCloth::CLOTH_IsMoving))
						{
							if(m_ClothSound)
							{
								m_ClothSound->StopAndForget();
							}
						}
						tunableCloth = true;
					}
				}
			}
		}
		
		f32 sqdDistance = LARGE_FLOAT;
		if(m_Object)
		{
			// Distance check to stop the sound. 
			sqdDistance = VEC3V_TO_VECTOR3(m_Object->GetTransform().GetPosition() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()).Mag2();
		}
		if(!tunableCloth)
		{
			if(!m_ClothSound && sqdDistance < sm_SqdDistanceThreshold )
			{
				if(m_WindClothSounds.IsInitialised())
				{
					audSoundInitParams initParams; 
					initParams.Tracker = m_Object->GetPlaceableTracker();
					initParams.EnvironmentGroup = CreateClothEnvironmentGroup();
					CreateSound_PersistentReference(m_WindClothSounds.Find(ATSTRINGHASH("CLOTH", 0x1951F94C)),&m_ClothSound,&initParams);
				}
				if(m_ClothSound)
				{
					//Use local wind 
					m_ClothSound->FindAndSetVariableValue(ATSTRINGHASH("Strength", 0xFF1C4627),windStrength);
					m_ClothSound->PrepareAndPlay();
				}
			}
			else if(m_ClothSound)
			{
				m_ClothSound->FindAndSetVariableValue(ATSTRINGHASH("Strength", 0xFF1C4627),windStrength);
			}
		}
		if(sqdDistance > sm_SqdDistanceThreshold && m_ClothSound)
		{
			m_ClothSound->StopAndForget();
		}

#if __BANK
		if(sm_FindCloth)
		{ 
			if(m_Object)
			{
				Color32 color = Color_red;
				if(m_WindClothSounds.IsInitialised())
				{
					color = Color_green;
				}
				grcDebugDraw::Capsule(m_Object->GetTransform().GetPosition()
					,VECTOR3_TO_VEC3V(VEC3V_TO_VECTOR3(m_Object->GetTransform().GetPosition()) + Vector3(0.f,0.f, 300.f)),1.f,color);
			}
		}
		if(sm_ShowClothPlaying)
		{ 
			if(m_Object && m_ClothSound)
			{
				Color32 color = Color_green;
				grcDebugDraw::Capsule(m_Object->GetTransform().GetPosition()
					,VECTOR3_TO_VEC3V(VEC3V_TO_VECTOR3(m_Object->GetTransform().GetPosition()) + Vector3(0.f,0.f, 300.f)),1.f,color);
			}
		}
#endif
	}
}
void audObjectAudioEntity::UpdateCreakSound(const Vector3 &UNUSED_PARAM(pos), const f32 UNUSED_PARAM(theta), const f32 UNUSED_PARAM(speed))
{
#if 0
	if(m_ControllerEntityId==AUD_INVALID_CONTROLLER_ENTITY_ID)
	{
		Init();
	}
	const f32 speedFactor = speed;
	const f32 absTheta = Abs(theta);
	//const f32 riseVol = absTheta * 2.f;
	//const f32 fallVol = (1.f - absTheta) * 2.f;
	//const f32 linVol = audDriverUtil::fsel(absTheta - 0.5f, fallVol, riseVol) * speedFactor;
	const f32 linVol = (1.f-Min(1.f,(absTheta*1.36f)))*speedFactor;
	const bool shouldPlay = (linVol > g_SilenceVolumeLin);
	const s32 pitch = static_cast<s32>(theta * 150.f);

	//char buf[128];
	//formatf(buf,sizeof(buf), "%f %f %f", theta, speed, linVol);
	//grcDebugDraw::Text(pos.x, pos.y, pos.z, Color32(255,255,0), buf);
	//
	if(shouldPlay)
	{
		if(!m_CreakSound)
		{
			audSoundInitParams initParams;
			static const audStringHash creakSound("SWAY_CREAK_LOOP");
			CreateAndPlaySound_Persistent(creakSound, &m_CreakSound, &initParams);
		}
		if(m_CreakSound)
		{
			m_CreakSound->SetRequestedPosition(pos);
			m_CreakSound->SetRequestedPitch(pitch);
			m_CreakSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(linVol));
		}
	}
	else
	{
		if(m_CreakSound)
		{
			m_CreakSound->StopAndForget();
		}
	}
#endif
}

naEnvironmentGroup* audObjectAudioEntity::CreateClothEnvironmentGroup()
{
	naEnvironmentGroup* environmentGroup = NULL;
 	if(m_Object)
	{
		environmentGroup = naEnvironmentGroup::Allocate("Cloth");
		if(environmentGroup)
		{
			environmentGroup->Init(NULL, 20.0f);
			environmentGroup->SetSource(m_Object->GetTransform().GetPosition());
			environmentGroup->SetInteriorInfoWithEntity(m_Object);
			environmentGroup->SetUsePortalOcclusion(true);
			environmentGroup->SetUseInteriorDistanceMetrics(true);
			environmentGroup->SetMaxPathDepth(2);
		}
	}
	return environmentGroup;
}

#if __BANK
void audObjectAudioEntity::AddWidgets(bkBank &bank){

	bank.PushGroup("Environment Cloths",false);
		bank.AddToggle("Find cloths",   &sm_FindCloth);
		bank.AddToggle("sm_ShowClothPlaying",   &sm_ShowClothPlaying);
		bank.AddToggle("Use interpolated wind",   &sm_UseInterpolatedWind);
		bank.AddToggle("Override wind strength",   &sm_OverrideWind);
		bank.AddSlider("overridden wind strength", &sm_OverriddenWindStrength, 0.0f, 1.0f, 0.001f);
		bank.AddSlider("sm_SqdDistanceThreshold", &sm_SqdDistanceThreshold, 0.0f, 10000.0f, 0.001f);
	bank.PopGroup();
}
#endif
