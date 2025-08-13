// 
// audio/music/vehiclemusic.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#include "vehiclemusic.h"
#include "musicplayer.h"
#include "audio/scriptaudioentity.h"
#include "game/wanted.h"
#include "peds/ped.h"
#include "peds/PlayerInfo.h"
#include "script/script.h"
#include "vehicles/Submarine.h"
#include "system/param.h"
#include "audioengine/engineutil.h"

AUDIO_MUSIC_OPTIMISATIONS()

PARAM(novehiclemusic, "Disable Vehicle specific music off-mission (Flying etc)");

void audVehicleMusic::Init()
{
	Reset();
}

void audVehicleMusic::Shutdown()
{

}

void audVehicleMusic::StopMusic()
{
	if(m_StartedMusic)
	{
		if(m_MusicType == Flying)
		{
			StopMusic("FLYING_STOP_QUICKLY");
		}
		else
		{
			StopMusic("SUBMARINE_STOP_MUSIC");
		}
	}
}

void audVehicleMusic::Update(const u32 UNUSED_PARAM(timeInMs))
{
	const bool playerIsOnAMission = CTheScripts::GetPlayerIsOnAMission();
	if(playerIsOnAMission || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::DisableFlightMusic))
	{
		if(m_StartedMusic)
		{
			if(m_MusicType == Flying)
			{
				StopMusic(playerIsOnAMission ? "FLYING_STOP_QUICKLY" : "FLYING_STOP_MUSIC");
			}
			else
			{
				StopMusic("SUBMARINE_STOP_MUSIC");
			}
		}
		m_InhibitTimer = Max(m_InhibitTimer, 25.f); // Don't allow flying music to start within 25 seconds of being on a mission
		return;
	}

	if(PARAM_novehiclemusic.Get())
		return;

	bool inhibitMusic = false;

	CPed *player = FindPlayerPed();
	if(!player ||
		(player->GetPlayerWanted() && player->GetPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL2) ||
		player->IsDead() || player->GetIsArrested())
	{
		inhibitMusic = true;
		// Don't allow flying music to start up within 20 seconds of wanted music
		m_InhibitTimer = Max(m_InhibitTimer, 20.f);
	}
	
	m_InhibitTimer -= fwTimer::GetTimeStep();
	if(m_InhibitTimer < 0.f)
	{
		m_InhibitTimer = 0.f;
	}
	else
	{
		inhibitMusic = true;
	}

	CVehicle *vehicle = player->GetVehiclePedInside();
	if(!inhibitMusic && vehicle && (vehicle->InheritsFromPlane() || vehicle->InheritsFromHeli()) && vehicle->GetModelNameHash() != ATSTRINGHASH("THRUSTER", 0x58CDAF30) && vehicle->GetStatus() != STATUS_WRECKED)
	{
		m_MusicType = Flying;

		if(vehicle->IsInAir())
		{
			m_StopTimer = 0.f;
			if(!m_StartedMusic)
			{
				StartMusic();
			}
		}
		else
		{
			m_StopTimer += fwTimer::GetTimeStep();
			// Wait for a while on the ground before stopping the music
			if(m_StartedMusic && m_StopTimer >= 5.f)
			{
				StopMusic("FLYING_STOP_MUSIC");
			}
		}
	}
	else if(!inhibitMusic && vehicle && (vehicle->InheritsFromSubmarine() || vehicle->InheritsFromSubmarineCar()))
	{
		m_MusicType = Underwater;
		float currentDepth = Water::IsCameraUnderwater()? vehicle->GetSubHandling()->GetCurrentDepth( vehicle ) : 0.f;

		if(vehicle->GetStatus() == STATUS_WRECKED)
		{
			if(m_StartedMusic)
			{
				StopMusic("SUBMARINE_CRASH");
			}
		}
		else if(currentDepth < -15.f)
		{
			if(!m_StartedMusic)
			{
				StartMusic();
			}
		}
		else if(currentDepth > -2.f)
		{
			if(m_StartedMusic)
			{
				StopMusic("SUBMARINE_STOP_MUSIC");
			}
		}
	}
	else if(m_StartedMusic)
	{
		//if(vehicle == NULL && player->)
		StopMusic(m_MusicType == Flying ? "FLYING_STOP_MUSIC" : "SUBMARINE_STOP_MUSIC");
	}
}

bool audVehicleMusic::TriggerEvent(const char *eventName)
{
	audDisplayf("Vehicle Music - %s", eventName);
	return g_InteractiveMusicManager.GetEventManager().TriggerEvent(eventName);
}

void audVehicleMusic::Reset()
{
	m_InhibitTimer = 0.f;
	m_StartedMusic = false;
	m_MusicType = None;
}

void audVehicleMusic::StartMusic()
{
	TriggerEvent(m_MusicType == Flying ? "FLYING_START_MUSIC" : "SUBMARINE_START_MUSIC");
	g_InteractiveMusicManager.SetRestoreMusic(false);
	m_StartedMusic = true;
}

void audVehicleMusic::StopMusic(const char *eventName)
{
	TriggerEvent(eventName);
	m_StartedMusic = false;
	
	// Wait for at least 20 seconds before bringing the music back in.
	m_InhibitTimer = Max(m_InhibitTimer, 20.f);
}

void audVehicleMusic::NotifyPlaneCrashed()
{
	if(m_StartedMusic)
	{
		StopMusic("FLYING_CRASH");
	}
}

void audVehicleMusic::NotifyPlayerBailed()
{
	if(m_StartedMusic)
	{
		StopMusic("FLYING_BAIL_OUT");
	}
}

