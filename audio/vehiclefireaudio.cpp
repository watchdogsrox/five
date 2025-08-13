// 
// audio/vehiclefireaudio.cpp
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#include "caraudioentity.h" 
#include "traileraudioentity.h"
#include "boataudioentity.h"
#include "vehiclefireaudio.h"
#include "northaudioengine.h"
#include "grcore/debugdraw.h"
#include "audio/heliaudioentity.h"
#include "audio/planeaudioentity.h"
#include "audiohardware/driverutil.h"
#include "audioengine/curve.h"
#include "audioengine/engineutil.h"
#include "vehicles/vehicle.h"
#include "vfx\misc/fire.h"
#include "frontendaudioentity.h"

#include "debugaudio.h"

AUDIO_VEHICLES_OPTIMISATIONS()

audCurve audVehicleFireAudio::sm_VehBurnStrengthToMainVol;
f32		audVehicleFireAudio::sm_StartVehFireThreshold = 0.05f;
BANK_ONLY(bool g_AuditionVehFire = false;);
BANK_ONLY(bool g_DrawVehFireEntities = false;);

void audVehicleFireAudio::InitClass()
{
	sm_VehBurnStrengthToMainVol.Init("BURN_STRENGTH_TO_MAIN_VOL");
}
audVehicleFireAudio::audVehicleFireAudio()
{
	m_Parent = NULL;
	m_NumberOfCurrentFires = 0;

	for (u32 i = 0; i < audMaxVehFires; i ++)
	{
		m_VehicleFires[i].fire = NULL;
		m_VehicleFires[i].fireSound = NULL;
		m_VehicleFires[i].fireType = audInvalidFire;
		m_VehicleFires[i].soundStarted = false;
	}
}

void audVehicleFireAudio::Init(audVehicleAudioEntity *pParent)
{
	if(naVerifyf(pParent,"Null pointer when initializing fire audio"))
	{
		m_Parent = pParent;
		for (u32 i = 0; i < audMaxVehFires; i ++)
		{
			m_VehicleFires[i].fire = NULL;
			m_VehicleFires[i].fireSound = NULL;
			m_VehicleFires[i].fireType = audInvalidFire;
			m_VehicleFires[i].soundStarted = false;
		}
		m_NumberOfCurrentFires = 0;
	}
}

// Stop all the sounds, but don't unregister the fires
void audVehicleFireAudio::StopAllSounds()
{
	for (s32 i = 0;i < audMaxVehFires; i++)
	{
		if(m_VehicleFires[i].fireSound)
		{
			m_VehicleFires[i].fireSound->StopAndForget();
		}

		m_VehicleFires[i].soundStarted = false;
	}
}

void audVehicleFireAudio::Update()
{
	if(naVerifyf(m_Parent,"Null vehaudioentity pointer on audVehicleFireAudio::Update"))
	{
		CVehicle* pVeh = m_Parent->GetVehicle();
		f32 fireEvo = (pVeh->IsDummy() ? 0.f : pVeh->m_Transmission.GetFireEvo());
		
		// Don't bother processing fires unless we actually have some active or are due to start the engine fire
		if(fireEvo > sm_StartVehFireThreshold || m_NumberOfCurrentFires > 0)
		{
			if(!m_VehicleFireSounds.IsInitialised())
			{
				bool foundSettings = false;
				if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR)
				{
					audCarAudioEntity* carAudioEntity = static_cast<audCarAudioEntity*>(m_Parent);
					if(carAudioEntity->GetCarAudioSettings())
					{
						m_VehicleFireSounds.Init(carAudioEntity->GetCarAudioSettings()->FireAudio);
						foundSettings = true;
					}
				}
				else if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
				{
					audPlaneAudioEntity* planeAudioEntity = static_cast<audPlaneAudioEntity*>(m_Parent);
					if(planeAudioEntity->GetPlaneAudioSettings())
					{
						m_VehicleFireSounds.Init(planeAudioEntity->GetPlaneAudioSettings()->FireAudio);
						foundSettings = true;
					}
				}
				else if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_HELI)
				{
					audHeliAudioEntity* heliAudioEntity = static_cast<audHeliAudioEntity*>(m_Parent);
					if(heliAudioEntity->GetHeliAudioSettings())
					{
						m_VehicleFireSounds.Init(heliAudioEntity->GetHeliAudioSettings()->FireAudio);
						foundSettings = true;
					}
				}
				else if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_TRAILER)
				{
					audTrailerAudioEntity* trailerAudioEntity = static_cast<audTrailerAudioEntity*>(m_Parent);
					if(trailerAudioEntity->GetTrailerAudioSettings())
					{
						m_VehicleFireSounds.Init(trailerAudioEntity->GetTrailerAudioSettings()->FireAudio);
						foundSettings = true;
					}
				}
				else if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_BOAT)
				{
					audBoatAudioEntity* boatAudioEntity = static_cast<audBoatAudioEntity*>(m_Parent);
					if(boatAudioEntity->GetBoatAudioSettings())
					{
						m_VehicleFireSounds.Init(boatAudioEntity->GetBoatAudioSettings()->FireAudio);
						foundSettings = true;
					}
				}

				if(!foundSettings)
				{
					m_VehicleFireSounds.Init(ATSTRINGHASH("VEH_FIRE_SOUNDSET", 0x2FF627AC));
				}
			}

			u32 cutoff = m_Parent->CalculateVehicleSoundLPFCutoff();	

			// Update the engine fire independently since it's not a real fire (CFire)
			UpdateEngineFire(fireEvo, cutoff);

			// Update the rest, actual CFires
			for (u32 i = 0; i < audMaxVehFires; i++)
			{
				// see if we have a cache non engine fire that needs started
				if((m_VehicleFires[i].fireType != audInvalidFire && m_VehicleFires[i].fireType != audEngineFire) && !m_VehicleFires[i].soundStarted)
				{
					StartVehFire(i);
				}
				if(m_VehicleFires[i].fireType != audInvalidFire && m_VehicleFires[i].fire && m_VehicleFires[i].fireType != audEngineFire)
				{
					if(m_VehicleFires[i].fire->GetCurrStrength() <= sm_StartVehFireThreshold 
						|| (m_VehicleFires[i].fire->GetEntity() && m_VehicleFires[i].fire->GetEntity()->m_nFlags.bIsOnFire == false))
					{
						if(m_VehicleFires[i].fireSound)
						{
							m_VehicleFires[i].fireSound->StopAndForget();
						}
						// I think these should be removed from the check for the sound existing, what if it already went NULL, this will at least clean up
						m_VehicleFires[i].fire->SetAudioRegistered(false);
						m_VehicleFires[i].fire = NULL;
						m_VehicleFires[i].fireType = audInvalidFire;
						m_VehicleFires[i].soundStarted = false;
						m_NumberOfCurrentFires--;
					}
					else if(m_VehicleFires[i].fireSound)
					{
						const f32 strengthVol = audDriverUtil::ComputeDbVolumeFromLinear(sm_VehBurnStrengthToMainVol.CalculateValue(Max(0.25f,m_VehicleFires[i].fire->GetCurrStrength())));
#if __BANK
						if(g_DrawVehFireEntities)
						{
							grcDebugDraw::Sphere(m_VehicleFires[i].fire->GetPositionWorld(), Max(0.25f, m_VehicleFires[i].fire->GetCurrStrength()), Color32(255,0,0,128));

							static const char* soundNames[] = { "ENGINE", "TANK", "TYRE", "GENERIC", "WRECKED", "FIRE_START" };
							grcDebugDraw::Text(m_VehicleFires[i].fire->GetPositionWorld(), Color32(1.0f, 0.0f, 0.0f), soundNames[m_VehicleFires[i].fireType]);
						}
#endif
						m_VehicleFires[i].fireSound->SetRequestedPosition(VEC3V_TO_VECTOR3(m_VehicleFires[i].fire->GetPositionWorld()));
						m_VehicleFires[i].fireSound->SetRequestedVolume(strengthVol);
						m_VehicleFires[i].fireSound->SetRequestedLPFCutoff(cutoff);
					}
				}
			}
		}
	}
}

void audVehicleFireAudio::UpdateEngineFire(f32 fireEvo, u32 lpfCutoff)
{
	CVehicle* pVeh = m_Parent->GetVehicle();
	if(naVerifyf(pVeh,"Null veh pointer on audVehicleFireAudioEntity::UpdateEngineFire"))
	{	
		if(fireEvo <= sm_StartVehFireThreshold)
		{
			for(u32 i = 0; i < audMaxVehFires; i++)
			{
				if(m_VehicleFires[i].fireType == audEngineFire)
				{
					if(m_VehicleFires[i].fireSound)
					{
						m_VehicleFires[i].fireSound->StopAndForget();
					}
					// I think these should be removed from the check for the sound existing, what if it already went NULL, this will at least clean up
					m_VehicleFires[i].fire = NULL;
					m_VehicleFires[i].fireType = audInvalidFire;
					m_VehicleFires[i].soundStarted = false;
					m_NumberOfCurrentFires--;
				}
			}
		}
		else
		{
			const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
			//Look if we have a engine fire sound already.
			s32 engineFireIndex = -1;
			for(u32 i = 0; i < audMaxVehFires; i++)
			{
				if(m_VehicleFires[i].fireType == audEngineFire)
				{
					engineFireIndex = i;
					break;
				}
			}
			if(engineFireIndex == -1)
			{
				// Find the first free slot
				s32 freeIndex = -1;
				for (s32 i = 0;i < audMaxVehFires; i++)
				{
					if(m_VehicleFires[i].fireType == audInvalidFire)
					{
						freeIndex = i;
						break;
					}
				}
				if(freeIndex == -1 || freeIndex == audMaxVehFires)
				{
					return;
				}
				// We don't yet have an engine fire, add it. 
				audSoundInitParams initParams;
				BANK_ONLY(initParams.IsAuditioning = g_AuditionVehFire;)
				initParams.Position = vVehiclePosition;
				initParams.IsStartOffsetPercentage = true;
				initParams.EnvironmentGroup = pVeh->GetAudioEnvironmentGroup();

				g_FrontendAudioEntity.CreateAndPlaySound(m_VehicleFireSounds.Find("FIRE_START"), &initParams);

				m_VehicleFires[freeIndex].fire = NULL;
				m_VehicleFires[freeIndex].fireType = audEngineFire;
				m_Parent->CreateSound_PersistentReference(m_VehicleFireSounds.Find("ENGINE"), &m_VehicleFires[freeIndex].fireSound, &initParams);
				if(m_VehicleFires[freeIndex].fireSound)
				{
					m_VehicleFires[freeIndex].fireSound->PrepareAndPlay();
				}

				m_NumberOfCurrentFires ++;
			}
			else 
			{
				naAssertf(m_VehicleFires[engineFireIndex].fireType != audInvalidFire,"Invalid fire type, something went wrong.");
				// Means we already have a engine fire sound
				if(!m_VehicleFires[engineFireIndex].fireSound)
				{
					audSoundInitParams initParams;
					BANK_ONLY(initParams.IsAuditioning = g_AuditionVehFire;);
					initParams.Position = vVehiclePosition;
					initParams.EnvironmentGroup = pVeh->GetAudioEnvironmentGroup();
					initParams.IsStartOffsetPercentage = true;
					g_FrontendAudioEntity.CreateAndPlaySound(m_VehicleFireSounds.Find("FIRE_START"), &initParams);
					g_FrontendAudioEntity.CreateSound_PersistentReference(m_VehicleFireSounds.Find("ENGINE"), &m_VehicleFires[engineFireIndex].fireSound, &initParams);
					if(m_VehicleFires[engineFireIndex].fireSound)
					{
						m_VehicleFires[engineFireIndex].fireSound->PrepareAndPlay();
					}
				}
				const f32 strengthVol = audDriverUtil::ComputeDbVolumeFromLinear(sm_VehBurnStrengthToMainVol.CalculateValue(Max(0.25f,fireEvo)));

				if(m_VehicleFires[engineFireIndex].fireSound)
				{
					m_VehicleFires[engineFireIndex].fireSound->SetRequestedPosition(vVehiclePosition);
					m_VehicleFires[engineFireIndex].fireSound->SetRequestedVolume(strengthVol);
					m_VehicleFires[engineFireIndex].fireSound->SetRequestedLPFCutoff(lpfCutoff);
				}
			}
#if __BANK
			if(g_DrawVehFireEntities)
			{
				grcDebugDraw::Sphere(vVehiclePosition,Max(0.25f,fireEvo),Color32(255,0,0,128));
			}
#endif
		}
	}
}

void audVehicleFireAudio::StartVehFire(s32 index)
{
	if(naVerifyf(m_Parent,"Null vehaudioentity pointer on audVehicleFireAudio::RegisterVehTankFire"))
	{
		if((CVehicle*)m_VehicleFires[index].fire->GetEntity() == m_Parent->GetVehicle())
		{
			//@@: location AUDVEHICLEFIREAUDIO_STARTVEHFIRE
			audSoundInitParams initParams;
			Vec3V vv = m_VehicleFires[index].fire->GetPositionWorld();
			Vector3 vFirePosition = RCC_VECTOR3(vv);
			BANK_ONLY(initParams.IsAuditioning = g_AuditionVehFire;);
			initParams.Position = vFirePosition;
			initParams.IsStartOffsetPercentage = true;
			initParams.EnvironmentGroup = m_Parent->GetVehicle()->GetAudioEnvironmentGroup();
			g_FrontendAudioEntity.CreateAndPlaySound(m_VehicleFireSounds.Find("FIRE_START"), &initParams);

			static const u32 soundNames[] = {
				0,//audEngineFire = 0,
				ATSTRINGHASH("TANK", 0x4044647),//audTankFire, 
				ATSTRINGHASH("TYRE", 0x6D3A1CE5), //audTyreFire, 
				ATSTRINGHASH("GENERIC", 0x9681DED9),//audGenericFire,
				ATSTRINGHASH("WRECKED", 0x50AECCC9)//audVehWreckedFire,
			};

			g_FrontendAudioEntity.CreateSound_PersistentReference(m_VehicleFireSounds.Find(soundNames[m_VehicleFires[index].fireType]), &m_VehicleFires[index].fireSound, &initParams);

			if(m_VehicleFires[index].fireSound)
			{
				m_VehicleFires[index].fireSound->PrepareAndPlay();
			}
			m_VehicleFires[index].soundStarted = true;
		}
		else
		{
			UnregisterVehFire(m_VehicleFires[index].fire);
		}
	}
}

void audVehicleFireAudio::RegisterVehFire(CFire* fire,audVehicleFires firetype)
{
	if(naVerifyf(m_Parent,"Null vehaudioentity pointer on audVehicleFireAudio::RegisterVehTankFire"))
	{
		if(naVerifyf(fire->GetEntity(),"Fire is not registered with any entity"))
		{
			if(naVerifyf((CVehicle*)fire->GetEntity() == m_Parent->GetVehicle(), "Trying to register a fire in the wrong vehicle"))
			{
				if(m_NumberOfCurrentFires < audMaxVehFires)
				{
					// Find the first free slot
					s32 freeIndex = -1;
					for (s32 i = 0;i < audMaxVehFires; i++)
					{
						if(m_VehicleFires[i].fireType == audInvalidFire)
						{
							freeIndex = i;
							break;
						}
					}
					if(freeIndex == -1 || freeIndex == audMaxVehFires)
					{
						return;
					}

					m_VehicleFires[freeIndex].soundStarted = false;
					m_VehicleFires[freeIndex].fire = fire;
					m_VehicleFires[freeIndex].fireType = firetype;

					m_NumberOfCurrentFires ++;
				}
			}
		}		
	}
}

void audVehicleFireAudio::UnregisterVehFire(CFire* fire)
{
	for (s32 i = 0; i < audMaxVehFires; i++)
	{
		if(m_VehicleFires[i].fire == fire)
		{
			if(m_VehicleFires[i].fireType != audInvalidFire)
			{
				if(m_VehicleFires[i].fireSound)
				{
					m_VehicleFires[i].fireSound->StopAndForget();
				}
			
				fire->SetAudioRegistered(false);
				m_VehicleFires[i].fireType = audInvalidFire;
				m_VehicleFires[i].fire = NULL;
				m_VehicleFires[i].soundStarted = false;
				m_NumberOfCurrentFires--;
			}

			break;
		}
	}
}

void audVehicleFireAudio::RegisterVehTankFire(CFire* fire)
{
	RegisterVehFire(fire,audTankFire);
}
void audVehicleFireAudio::RegisterVehTyreFire(CFire* fire)
{
	RegisterVehFire(fire,audTyreFire);
}
void audVehicleFireAudio::RegisterVehWreckedFire(CFire* fire)
{
	RegisterVehFire(fire,audVehWreckedFire);
}
void audVehicleFireAudio::RegisterVehGenericFire(CFire* fire)
{
	RegisterVehFire(fire,audGenericFire);
}

#if __BANK

void audVehicleFireAudio::AddWidgets(bkBank& bank)
{
	bank.PushGroup("audVehicleFireAudioEntity",false);
	bank.AddToggle("Audition veh fire",&g_AuditionVehFire);
	bank.AddToggle("Draw veh fire entities.",&g_DrawVehFireEntities);
	bank.AddSlider("sm_StartVehFireThreshold", &sm_StartVehFireThreshold, 0.f, 1.f, 0.01f);
	bank.PopGroup();
}

#endif	// __BANK
