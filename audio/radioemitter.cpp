// 
// audio/radioemitter.cpp
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#include "radioemitter.h"

#if NA_RADIO_ENABLED

#include "music/musicplayer.h"
#include "radioaudioentity.h"
#include "audioengine/engine.h"
#include "audiohardware/driverdefs.h"
#include "audio/caraudioentity.h"
#include "audio/northaudioengine.h"
#include "vehicles/vehicle.h"
#include "Peds/ped.h"
#include "scene/world/GameWorld.h"

#include "debugaudio.h"

AUDIO_MUSIC_OPTIMISATIONS()

audEntityRadioEmitter::audEntityRadioEmitter(CEntity *entity) :
m_Entity(entity)
{
}

void audEntityRadioEmitter::GetPosition(Vector3 &position) const
{
	if(m_Entity)
	{
		Vec3V offset = Vec3V(V_ZERO);

		if(m_Entity->GetIsTypeVehicle())
		{
			CVehicle* vehicle = (CVehicle*)m_Entity;
			const audVehicleAudioEntity* vehicleAudioEntity = vehicle->GetVehicleAudioEntity();

			if(vehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_CAR)
			{
				offset = ((audCarAudioEntity*)vehicleAudioEntity)->GetRadioEmitterOffset();				
			}
		}

		position = VEC3V_TO_VECTOR3(m_Entity->GetTransform().Transform(offset));
	}
	else
	{
		position.Set(0.0f, 0.0f, 0.0f);
	}
}

u32 audEntityRadioEmitter::GetLPFCutoff() const
{
	u32 cutoff = kVoiceFilterLPFMaxCutoff;
	if(m_Entity && m_Entity->GetIsTypeVehicle())
	{
		cutoff = ((CVehicle *)m_Entity)->GetVehicleAudioEntity()->GetRadioLPFCutoff();
	}

	return cutoff;
}

#if __BANK
const char* audEntityRadioEmitter::GetName() const
{ 
	return m_Entity ? m_Entity->GetDebugName() : "Entity Radio Emitter"; 
}
#endif

u32 audEntityRadioEmitter::GetHPFCutoff() const
{
	u32 cutoff = 0;
	if(m_Entity && m_Entity->GetIsTypeVehicle())
	{
		cutoff = ((CVehicle *)m_Entity)->GetVehicleAudioEntity()->GetRadioHPFCutoff();
	}

	return cutoff;
}

float audEntityRadioEmitter::GetRolloffFactor() const
{
	float rolloff = 1.f;
	if(m_Entity && m_Entity->GetIsTypeVehicle())
	{
		rolloff = ((CVehicle *)m_Entity)->GetVehicleAudioEntity()->GetRadioRolloff();
	}
	return rolloff;
}

naEnvironmentGroup *audEntityRadioEmitter::GetOcclusionGroup()
{
	naEnvironmentGroup *occlusionGroup = NULL;
	if(m_Entity && m_Entity->GetIsTypeVehicle())
	{
		occlusionGroup = (naEnvironmentGroup*)(((CVehicle *)m_Entity)->GetVehicleAudioEntity()->GetEnvironmentGroup());
	}

	return occlusionGroup;
}

void audEntityRadioEmitter::SetOcclusionGroup(naEnvironmentGroup *UNUSED_PARAM(occlusionGroup))
{
}

bool audEntityRadioEmitter::IsReplaceable()
{
	if(m_Entity)
	{
		return m_Entity->GetIsTypeVehicle(); //Only replace vehicle emitters.
	}
	else
	{
		return true;
	}
}

void audEntityRadioEmitter::SetRadioStation(const audRadioStation *station)
{	
	if(m_Entity && m_Entity->GetIsTypeVehicle())
	{
		((CVehicle *)m_Entity)->GetVehicleAudioEntity()->SetRadioStation(station);
	}
	else if((!m_Entity || m_Entity->GetIsTypePed()) && station)
	{
		//Don't set the player radio station to off, as this is handled at a higher-level.
		g_RadioAudioEntity.SetPlayerRadioStation(station);
	}
}

bool audEntityRadioEmitter::IsPlayerVehicle()
{
	return (m_Entity != NULL && FindPlayerVehicle()==(CVehicle*)m_Entity);
}

bool audEntityRadioEmitter::IsLastPlayerVehicle()
{
	return (m_Entity != NULL && m_Entity == g_RadioAudioEntity.GetLastPlayerVehicle());
}

void audEntityRadioEmitter::NotifyTrackChanged() const
{
	if (m_Entity && m_Entity->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_Entity);
		pVehicle->NotifyPassengersOfRadioTrackChange();
	}
}

u32 audEntityRadioEmitter::GetPriority() const
{
	if(!m_Entity || (m_Entity->GetIsTypePed() && ((CPed*)m_Entity)->IsLocalPlayer()))
	{
		return audStreamSlot::STREAM_PRIORITY_PLAYER_RADIO;
	}
	else if(m_Entity->GetIsTypeVehicle() && CGameWorld::FindLocalPlayer()->GetVehiclePedInside() == m_Entity)
	{
		return audStreamSlot::STREAM_PRIORITY_PLAYER_RADIO;
	}
	else if(m_Entity->GetIsTypeVehicle() && ((CVehicle*)m_Entity)->GetVehicleAudioEntity() && ((CVehicle*)m_Entity)->GetVehicleAudioEntity()->GetScriptRequestedRadioStation())
	{
		return audStreamSlot::STREAM_PRIORITY_SCRIPT_VEHICLE;
	}
	else
	{
		Vector3 pos;
		GetPosition(pos);
		f32 dist2 = (VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) - pos).Mag2();
		if(dist2 >= (20.f * 20.f))
		{
			return audStreamSlot::STREAM_PRIORITY_ENTITY_EMITTER_FAR;
		}
		else
		{
			if(m_Entity->GetIsTypeVehicle() && ((CVehicle*)m_Entity)->ContainsPlayer())
			{
				return audStreamSlot::STREAM_PRIORITY_ENTITY_EMITTER_NETWORK;
			}
			return audStreamSlot::STREAM_PRIORITY_ENTITY_EMITTER_NEAR;
		}	
	}
}

f32 audEntityRadioEmitter::GetEmittedVolume() const
{
	if(m_Entity && m_Entity->GetIsTypeVehicle())
	{
		CVehicle *veh = (CVehicle*)m_Entity;
		return veh->GetVehicleAudioEntity()->GetAmbientRadioVolume();
	}
	else
	{
		// don't want the mobile phone radio attached to Niko audible when its not frontend
		return -100.f;
	}
}

f32 audEntityRadioEmitter::GetEnvironmentalLoudness() const
{
	if(m_Entity && m_Entity->GetIsTypeVehicle())
	{
		CVehicle *veh = (CVehicle*)m_Entity;
		
		if(veh->GetVehicleAudioEntity()->IsRadioUpgraded())
		{
			return 1.f;
		}
	}

	return 0.f;
}

float audStaticRadioEmitter::GetEmittedVolume() const
{
	float emittedVol = m_EmittedVolume;
	const bool muteForScore = (AUD_GET_TRISTATE_VALUE(m_StaticEmitter->Flags, FLAG_ID_STATICEMITTER_MUTEFORSCORE)==AUD_TRISTATE_TRUE);
	if(muteForScore)
	{
		emittedVol += g_InteractiveMusicManager.GetStaticRadioEmitterVolumeOffset();
	}
	
	return emittedVol;	
}

#if __BANK
const char* audStaticRadioEmitter::GetName() const
{
	if (m_StaticEmitter)
	{
		return audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(m_StaticEmitter->NameTableOffset);
	}

	return "NULL";
}
#endif

float audAmbientRadioEmitter::GetEmittedVolume() const
{
	float emittedVol = m_EmittedVolume;
	emittedVol += g_InteractiveMusicManager.GetStaticRadioEmitterVolumeOffset();
	return emittedVol;	
}

void audStaticRadioEmitter::SetRadioStation(const audRadioStation *UNUSED_PARAM(station))
{

}

u32 audStaticRadioEmitter::GetPriority() const
{	
	if(m_IsClubEmitter || m_IsHighPriorityEmitter || (m_StaticEmitter && m_StaticEmitter->RadioStation == ATSTRINGHASH("HIDDEN_RADIO_STRIP_CLUB", 0x3B91F5CC)))
	{
		return audStreamSlot::STREAM_PRIORITY_SCRIPT;
	}
	return audStreamSlot::STREAM_PRIORITY_STATIC_AMBIENT_EMITTER;	
}
#endif // NA_RADIO_ENABLED
