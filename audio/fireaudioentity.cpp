// 
// audio/fireaudioentity.cpp
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 


#include "fireaudioentity.h"
#include "pedaudioentity.h"
#include "northaudioengine.h"
#include "weatheraudioentity.h"
#include "grcore/debugdraw.h"
#include "game/wind.h"
#include "peds/ped.h"
#include "audiohardware/driverutil.h"
#include "audioengine/curve.h"
#include "audioengine/engineutil.h"
#include "Peds/PedFootstepHelper.h"
#include "vfx\misc/fire.h"
#include "weapons/Weapon.h"

#include "debugaudio.h"

audCurve g_audBurnStrengthToMainVol;

BANK_ONLY(bool g_AuditionFire = false;);
BANK_ONLY(bool g_DrawFireEntities = false;);

f32 g_MaxWindSpeedMag = 20.f;
s32 g_NextFireStartOffset = 0;
bank_u32 g_FireManagerFireTimeOut = 1000;  // stop a fire is not updated within this time frame

audFireSoundManager g_FireSoundManager;

EXT_PF_GROUP(TriggerSoundsGroup);
PF_COUNTER(audFireAudioEntity, TriggerSoundsGroup);

audSoundSet audFireAudioEntity::sm_FireSounds;
f32 audFireAudioEntity::sm_StartFireThreshold = 0.2f;
u32 audFireAudioEntity::sm_LastFireStartTime = 0;

sysCriticalSectionToken audFireSoundManager::sm_CritSec;

void audFireAudioEntity::InitClass()
{
	g_audBurnStrengthToMainVol.Init("BURN_STRENGTH_TO_MAIN_VOL");
	sm_FireSounds.Init("FIRE_SOUNDS");
	sm_LastFireStartTime = 0;
}
audFireAudioEntity::audFireAudioEntity()
{
	m_MainFireLoop = NULL;
	m_CloseFireLoop = NULL;
	m_WindFireLoop = NULL;
	m_Ped = NULL;
	m_LastUpdateTime = 0;
	m_LifeRatio = 0.0f;
	m_State = FIRESTATE_START;
	m_EntityInitialized = false;

	m_FireStartTime = 0;
	m_FireLifeSpan = ~0U;

	m_DistanceFromListener = 0.0f;
	m_LastDistanceListenerUpdateTime = 0;
}

void audFireAudioEntity::Shutdown() 
{ 
	s32 index = g_FireSoundManager.m_FireAudioEntity.Find(this);
	if(index != -1)	
	{
		g_FireSoundManager.RemoveFire(index);
	}

	if(m_MainFireLoop)
		m_MainFireLoop->StopAndForget();
	if(m_CloseFireLoop)
		m_CloseFireLoop->StopAndForget();
	if(m_WindFireLoop)
		m_WindFireLoop->StopAndForget();

	naAudioEntity::Shutdown();
	m_Ped = NULL; 
}

void audFireAudioEntity::DeferredInit()
{
	naAudioEntity::Init();
	audEntity::SetName("audFireAudioEntity - CFire");
	m_EntityInitialized = true;
}

void audFireAudioEntity::Init(const CFire *fire)
{
	naAssertf(fire, "Null fire pointer when initializing the fire audio entity");
	naAssertf(sm_FireSounds.IsInitialised(),"Failed initializing fire soundset" );
	
	// we can't call naAudioEntity::Init(); here as it causes threading issues
	m_EntityInitialized = false;

	m_State = FIRESTATE_STOPPED;
	g_NextFireStartOffset = (g_NextFireStartOffset+70)%100;
	m_WindFluctuator.Init(0.1f,0.1f,0.f,0.5f, 0.8f, 1.0f, 0.3f, 0.5f, 0.0f);
	m_Position = fire->GetPositionWorld();
	m_LifeRatio = fire->GetLifeRatio();
	m_LastUpdateTime = g_AudioEngine.GetTimeInMilliseconds(); 

	f32 distFromListener = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition() - m_Position).Mag();
	SetDistanceFromListener(distFromListener);

	// if the fire is close enough start the sound right away, hopefully won't overwhelm us
	static bank_float maxDistance = 10.0f;
	if(distFromListener < maxDistance)
	{
		m_State = FIRESTATE_START;
	}

	g_FireSoundManager.AddFire(this);
}

void audFireAudioEntity::PlayFireSound(const CFire *fire)
{
	if(m_MainFireLoop)
	{
		m_State = FIRESTATE_PLAYING;
		return;
	}

	audSoundInitParams initParams;
	Vec3V vv = fire->GetPositionWorld();
	Vector3 v = RCC_VECTOR3(vv);
	BANK_ONLY(initParams.IsAuditioning = g_AuditionFire;);
	initParams.Position = v;
	initParams.StartOffset = g_NextFireStartOffset;
	initParams.IsStartOffsetPercentage = true;
	//initParams.UpdateEntity = true;
	if(fire->GetEntity())
	{
		initParams.EnvironmentGroup = fire->GetEntity()->GetAudioEnvironmentGroup();
		if(fire->GetEntity()->GetIsTypePed())
		{
			m_Ped = (CPed *)fire->GetEntity();
		}
	}

	audMetadataRef whooshSoundRef = g_NullSoundRef;
	switch(fire->GetFireType())
	{
	case FIRETYPE_TIMED_MAP:
		PF_INCREMENT(audFireAudioEntity);
		CreateSound_PersistentReference(sm_FireSounds.Find("MAP_FIRE"), &m_MainFireLoop, &initParams);
		whooshSoundRef = sm_FireSounds.Find("MAP_FIRE_START");
		break;
	case FIRETYPE_TIMED_OBJ_GENERIC:
		PF_INCREMENT(audFireAudioEntity);
		CreateSound_PersistentReference(sm_FireSounds.Find("GENERIC_OBJECT_FIRE"), &m_MainFireLoop, &initParams);
		whooshSoundRef = sm_FireSounds.Find("GENERIC_OBJECT_FIRE_START");
		break;
	case FIRETYPE_TIMED_PETROL_TRAIL:
		PF_INCREMENT(audFireAudioEntity);
		CreateSound_PersistentReference(sm_FireSounds.Find("PETROL_TRAIL_FIRE"), &m_MainFireLoop, &initParams);
		whooshSoundRef = sm_FireSounds.Find("PETROL_TRAIL_FIRE_START");
		break;
	case FIRETYPE_TIMED_PETROL_POOL:
		PF_INCREMENT(audFireAudioEntity);
		CreateSound_PersistentReference(sm_FireSounds.Find("PETROL_POOL_FIRE"), &m_MainFireLoop, &initParams);
		whooshSoundRef = sm_FireSounds.Find("PETROL_POOL_FIRE_START");
		break;
	case FIRETYPE_TIMED_PED:
		PF_INCREMENT(audFireAudioEntity);
		CreateSound_PersistentReference(sm_FireSounds.Find("PED_FIRE"), &m_MainFireLoop, &initParams);
		whooshSoundRef = sm_FireSounds.Find("PED_FIRE_START");
		break;
	case FIRETYPE_REGD_FROM_PTFX:
		{
			PF_INCREMENT(audFireAudioEntity);

			bool allowedToPlayWhoosh = true;

			rage::ptxEffectInst* pFxInst = static_cast<rage::ptxEffectInst*>(fire->GetRegOwner());
			if(pFxInst)
			{
				rage::ptxEffectRule* effectRule = pFxInst->GetEffectRule();
				if(effectRule)
				{

					const char* name = effectRule->GetName();
					if(name)
					{
						if(atStringHash(name) == ATSTRINGHASH("weap_heist_flare_trail", 0xCD9F9FA8) ||
							atStringHash(name) == ATSTRINGHASH("proj_indep_flare_trail", 0xFEE69956) ||
							atStringHash(name) == ATSTRINGHASH("proj_heist_flare_trail", 0x652F9C28))
						{
							allowedToPlayWhoosh = false;
						}
						char fireSoundName[256];
						snprintf(fireSoundName, 256, "PTFX_%s", name);
						CreateSound_PersistentReference(atStringHash(fireSoundName), &m_MainFireLoop, &initParams);
					}
				}
			}
			if(!m_MainFireLoop)
			{
				CreateSound_PersistentReference(sm_FireSounds.Find("REG_PTFX_FIRE"), &m_MainFireLoop, &initParams);
			}
			
			if(allowedToPlayWhoosh)
			{
				whooshSoundRef = sm_FireSounds.Find("REG_PTFX_FIRE_START");
			}

		}
		break;
	case FIRETYPE_TIMED_VEH_GENERIC:
	case FIRETYPE_TIMED_VEH_WRECKED:
	case FIRETYPE_TIMED_VEH_WRECKED_2:
	case FIRETYPE_TIMED_VEH_WRECKED_3:
	case FIRETYPE_REGD_VEH_PETROL_TANK:
	case FIRETYPE_REGD_VEH_WHEEL:
		return;
	default:
		naAssertf(false,"Unsuported fire type :%d",fire->GetFireType());
		break;
	}
	PF_INCREMENT(audFireAudioEntity);
	initParams.ptrClientVar = fire;

	f32 whooshVolume = 0.0f;
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds(); 
	bool playStartWhoosh = true;
	if( (m_State == FIRESTATE_RESTART || fire->GetFireType() == FIRETYPE_TIMED_MAP) && g_FireSoundManager.GetCount() != 0 ) 
	{
		playStartWhoosh = false;
	}
	else
	{
		static bank_u32 fireWhooshTimeout = 5000;
		if(currentTime < sm_LastFireStartTime + fireWhooshTimeout)
		{
			if(fire->GetFireType() == FIRETYPE_TIMED_PETROL_TRAIL)
			{
				static bank_float startWhooshDuck = -6.0f;
				whooshVolume = startWhooshDuck;
			}
			else
			{
				playStartWhoosh = false;
			}
		}
		else
		{
			if(fire->GetFireType() == FIRETYPE_TIMED_PETROL_TRAIL)
			{
				static bank_float startWhooshDuck = 6.0f;
				whooshVolume = startWhooshDuck;
			}
		}
	}

	if(playStartWhoosh)
	{
		sm_LastFireStartTime = currentTime;
		
		initParams.Volume = whooshVolume;
		initParams.StartOffset = 0;
		initParams.IsStartOffsetPercentage = false;

		CreateAndPlaySound(whooshSoundRef, &initParams);
	}
	if(m_MainFireLoop) 
	{
		m_MainFireLoop->PrepareAndPlay();
		m_State = FIRESTATE_PLAYING;
	}
}

void audFireAudioEntity::Update(const CFire *fire, const f32 UNUSED_PARAM(timeStep))
{
	if(!m_EntityInitialized)
	{
		DeferredInit();
	}

	f32 boneVelocityScalar = 1.f;
	f32 velocityScalar = 1.f;
	if(fire->GetEntity())
	{
		if(fire->GetEntity()->GetIsTypePed())
		{
			const CPedFootStepHelper &pedFootstepHelper = ((CPed*)fire->GetEntity())->GetFootstepHelper();
			ScalarV maxSpeed = ScalarV(V_ZERO);
			for(u32 i = 0; i < MaxFeetTags -1 ; i++)
			{
				maxSpeed = Max(maxSpeed, pedFootstepHelper.GetFootVelocity((FeetTags)i));
			}
			boneVelocityScalar = Min(1.f, maxSpeed.Getf() * 1.7f);
			velocityScalar = Min(1.f, ((CPed*)fire->GetEntity())->GetVelocity().Mag()/15.f);
		}
	}
	m_LifeRatio = fire->GetLifeRatio();
	m_Position = fire->GetPositionWorld();
	UpdateFire(fire, boneVelocityScalar*velocityScalar, RCC_VECTOR3(m_Position));

	m_LastUpdateTime = g_AudioEngine.GetTimeInMilliseconds(); 
		
#if __BANK
	if(g_DrawFireEntities)
	{
		if(m_State == FIRESTATE_PLAYING)
			grcDebugDraw::Sphere(m_Position, fire->GetCurrStrength(), Color32(0,255,0,128));
		else
			grcDebugDraw::Sphere(m_Position, fire->GetCurrStrength(), Color32(255,0,0,128));

		if(fire->GetFireType() == FIRETYPE_REGD_FROM_PTFX)
		{
			rage::ptxEffectInst* pFxInst = static_cast<rage::ptxEffectInst*>(fire->GetRegOwner());
			if(pFxInst)
			{
				rage::ptxEffectRule* effectRule = pFxInst->GetEffectRule();
				if(effectRule)
				{
					const char* name = effectRule->GetName();
					if(name)
					{
						char txt[128];
						formatf(txt, "%s %f", name, fire->GetCurrStrength());
						grcDebugDraw::Text(m_Position, Color_blue, txt);
					}
				}
			}
		}
		else
		{
			char txt[128];
			formatf(txt, "%f", fire->GetCurrStrength());
			grcDebugDraw::Text(m_Position, Color_blue, txt);
		}
	}
#endif
	
	//char buf[256];
	//formatf(buf,sizeof(buf),"%f: %f, %f [%f]", fire->GetCurrStrength(), strengthVol, windSpeedVol, windSpeed);
	//grcDebugDraw::Text(fire->GetPosition().x, fire->GetPosition().y, fire->GetPosition().z, Color32(255,255,255), buf);
}

void audFireAudioEntity::UpdateFire(const CFire *fire, const f32 velocityScalar, const Vector3 &pos)
{
	if(m_State == FIRESTATE_START || m_State == FIRESTATE_RESTART)
	{
		if(fire->GetCurrStrength() > sm_StartFireThreshold || 
			fire->GetFireType() == FIRETYPE_TIMED_PETROL_TRAIL || fire->GetFireType() == FIRETYPE_TIMED_MAP || fire->GetFireType() == FIRETYPE_TIMED_PETROL_POOL)
		{
			PlayFireSound(fire);
		}
	}

	f32 strength = fire->GetCurrStrength();
	if(fire->GetFireType() == FIRETYPE_TIMED_MAP || fire->GetFireType() == FIRETYPE_TIMED_PETROL_TRAIL)
	{
		strength = 1.0f;
	}
	if(strength <= sm_StartFireThreshold || m_State == FIRESTATE_STOPPED)
	{
		if(m_MainFireLoop)
		{
			m_MainFireLoop->StopAndForget();
			m_MainFireLoop = NULL;
		}
	}
	else if(m_MainFireLoop)
	{
		const f32 strengthVol = audDriverUtil::ComputeDbVolumeFromLinear(g_audBurnStrengthToMainVol.CalculateValue(strength));
		Vector3 windSpeed;
		Vector3 position = pos;
		const f32 windStrength = g_WeatherAudioEntity.GetWindStrength();

		const f32 windSpeedVol = audDriverUtil::ComputeDbVolumeFromLinear(Max(0.5f, velocityScalar) * windStrength);
		if(m_MainFireLoop)
		{
			m_MainFireLoop->SetRequestedPosition(pos);
			m_MainFireLoop->SetRequestedVolume(strengthVol);
		}
		if(m_WindFireLoop)
		{
			m_WindFireLoop->SetRequestedPosition(pos);
			m_WindFireLoop->SetRequestedVolume(strengthVol + windSpeedVol);
		}
		if(m_CloseFireLoop)
		{
			m_CloseFireLoop->SetRequestedPosition(pos);
			m_CloseFireLoop->SetRequestedVolume(strengthVol);
		}
	}
}

void audFireAudioEntity::UpdateSound(audSound *UNUSED_PARAM(sound), audRequestedSettings *UNUSED_PARAM(reqSets), u32 UNUSED_PARAM(timeInMs))
{
	// Reinstate initParams.UpdateEntity = true; if this is required

	/*CFire *fire;
	void *ptr;
	reqSets->GetClientVariable(ptr);
	fire = (CFire*)ptr;

	if(false)
	{
		Vec3V vFirePos = fire->GetPositionWorld();
		reqSets->SetPosition(vFirePos);
	}*/
}

void audFireAudioEntity::PreUpdateService(u32 /*timeInMs*/)
{
	/*if(m_Vehicle && m_MainFireLoop && !m_Vehicle->m_Transmission.GetEngineOnFire())
	{
		StopAndForgetSounds(m_MainFireLoop, m_WindFireLoop, m_CloseFireLoop);
	}*/
}


//PURPOSE
//Supports audDynamicEntitySound queries
u32 audFireAudioEntity::QuerySoundNameFromObjectAndField(const u32 *objectRefs, u32 numRefs)
{
	naAssertf(objectRefs, "A null object ref array ptr has been passed into audFireAudioEntity::QuerySoundNameFromObjectAndField");
	naAssertf(numRefs >= 2,"The object ref list should always have at least two entries when entering QuerySoundNameFromObjectAndField");

	if(numRefs ==2)
	{
		//We're at the end of the object ref chain, but fire audioentity can only pass the queue on to another entity
		naWarningf("Attempting to query an object field in fireaudioentity but object is not a supported type");
	}
	else
	{
		--numRefs;
		 if(*objectRefs == ATSTRINGHASH("PED", 0x34D90761))
		{
			if(m_Ped)
			{
				++objectRefs;
				return m_Ped->GetPedAudioEntity()->QuerySoundNameFromObjectAndField(objectRefs, numRefs);
			}
		}
		else
		{
			naWarningf("Unsupported object type (%d) in reference chain passed into audFireAudioEntity::QuerySoundNameFromObjectAndField", *objectRefs);	
		}
	}
	return 0; 
}

#if __BANK

void audFireAudioEntity::AddWidgets(bkBank& bank)
{
	bank.PushGroup("FireAudioEntity",false);
		bank.AddToggle("Audition fire",&g_AuditionFire);
		bank.AddToggle("Draw fire entities.",&g_DrawFireEntities);
	bank.PopGroup();
}

#endif	// __BANK

audFireSoundManager::audFireSoundManager()
{
}

void audFireSoundManager::InitClass()
{
	m_FireAudioEntity.Reset();
	m_LastSortTime = 0;
}

void audFireSoundManager::ShutdownClass()
{
	for(int i=0; i<m_FireAudioEntity.GetCount(); i++)
	{
		m_FireAudioEntity[i]->Shutdown();
	}
}

int FireCompare(audFireAudioEntity* const* a, audFireAudioEntity* const* b)
{
	return (((*a)->GetDistanceFromListener() - (*b)->GetDistanceFromListener()) > 0.0f ? 1 : -1);
}

void audFireSoundManager::Update()
{
	SYS_CS_SYNC(sm_CritSec);
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds(); 
	static bank_u32 timeBetweenSorts = 200;
	if(currentTime > m_LastSortTime + timeBetweenSorts)
	{
		m_FireAudioEntity.QSort(0, -1, FireCompare);
		m_LastSortTime = currentTime;
	}

	for(int i=0; i<m_FireAudioEntity.GetCount(); i++)
	{
		static bank_s32 numberOfFireSounds = 10;
		static bank_float maxDistance = 150.0f;
		if(m_FireAudioEntity[i]->GetDistanceFromListener() < maxDistance && i < numberOfFireSounds)
		{
			if(m_FireAudioEntity[i]->GetState() == FIRESTATE_STOPPED)
				m_FireAudioEntity[i]->SetState(FIRESTATE_RESTART);
		}
		else
		{
			m_FireAudioEntity[i]->SetState(FIRESTATE_STOPPED);
		}

		static bank_u32 listenerDistanceUpdateInterval = 250;
		if(currentTime > m_FireAudioEntity[i]->GetLastDistanceListenerUpdateTime() + listenerDistanceUpdateInterval)
		{
			f32 distFromListener = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition() - m_FireAudioEntity[i]->GetPositionWorld()).Mag();
			m_FireAudioEntity[i]->SetDistanceFromListener(distFromListener);
		}

//#if __BANK
//		static bool showDebug = false;
//		if(showDebug)
//		{
//			static bank_float size = 0.3f;
//			if(m_FireAudioEntity[i]->GetState() == FIRESTATE_STOPPED)
//				grcDebugDraw::Sphere(m_FireAudioEntity[i]->GetPositionWorld(), size, Color_red);
//			else
//				grcDebugDraw::Sphere(m_FireAudioEntity[i]->GetPositionWorld(), size, Color_green);
//		}
//#endif
	}
}

void audFireSoundManager::AddFire(audFireAudioEntity *fireAudioEntity)
{
	SYS_CS_SYNC(sm_CritSec);
	f32 distFromFire = VEC3V_TO_VECTOR3(fireAudioEntity->GetPositionWorld() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()).Mag();
	fireAudioEntity->SetDistanceFromListener(distFromFire);

	m_FireAudioEntity.PushAndGrow(fireAudioEntity);
	
	//Displayf("Count %d", m_FireAudioEntity.GetCount());
}

void audFireSoundManager::RemoveFire(s32 i)
{
	SYS_CS_SYNC(sm_CritSec);
	m_FireAudioEntity.Delete(i);
}

