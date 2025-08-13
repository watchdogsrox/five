// 
// audio/music/idlemusic.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#include "idlemusic.h"
#include "musicplayer.h"

#include "audio/environment/environment.h"
#include "audio/northaudioengine.h"
#include "game/wanted.h"
#include "peds/ped.h"
#include "peds/PlayerInfo.h"
#include "script/script.h"

#include "system/param.h"
#include "audioengine/engineutil.h"

AUDIO_MUSIC_OPTIMISATIONS()

PARAM(idlemusic, "Enable interactive music triggered when idle");

void audIdleMusic::Init()
{
	Reset();
}

void audIdleMusic::Shutdown()
{

}

enum IdleMusicTimings
{
	kMinIdleMusicWaitTime = 60000,
	kMaxIdleMusicWaitTime = 120000,
	kMinIdleMusicPlayTime = 60000,
	kMaxIdleMusicPlayTime = 100000,
};

void audIdleMusic::Update(const u32 timeInMs)
{
	if(CTheScripts::GetPlayerIsOnAMission())
	{
		if(m_StartedMusic)
		{
			StopMusic("STOP_IDLE_MUSIC_MISSION");
		}
		m_NextMusicTime = ~0U;
		return;
	}

	if(!PARAM_idlemusic.Get())
		return;

	bool inhibitMusic = false;

	const char *stopEvent = "STOP_IDLE_MUSIC";

	CPed *player = FindPlayerPed();
	if(player && player->GetPlayerWanted() && player->GetPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL2)
	{
		inhibitMusic = true;
		stopEvent = "STOP_IDLE_MUSIC_WANTED";
	}

	if(player && player->GetPlayerInfo()->GetPlayerState() != CPlayerInfo::PLAYERSTATE_PLAYING)
	{
		inhibitMusic = true;
		stopEvent = "STOP_IDLE_MUSIC_DEATHARREST";
	}
	
	if(!player || player->GetIsEnteringVehicle() || player->GetVehiclePedInside())
	{
		inhibitMusic = true;
		stopEvent = "STOP_IDLE_MUSIC_VEHICLE";
	}

	// Stop idle music when the player is firing
	if(timeInMs - m_LastWeaponFireTime < 1000U)
	{
		inhibitMusic = true;
		stopEvent = "STOP_IDLE_MUSIC_WEAPONFIRE";
	}

	if(audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior())
	{
		inhibitMusic = true;
		stopEvent = "STOP_IDLE_MUSIC_INTERIOR";
	}

	if(m_StartedMusic)
	{
		if(inhibitMusic)
		{
			StopMusic(stopEvent);
		}
		else if(timeInMs - m_LastMusicTime > m_MusicPlayDuration)
		{
			StopMusic("STOP_IDLE_MUSIC_TIMEDOUT");
		}
	}

	// Prevent choosing a new start time until we've exited the vehicle	and lost our wanted level, etc
	if(inhibitMusic)
	{
		m_NextMusicTime = ~0U;
	}

	if(m_NextMusicTime == ~0U && !inhibitMusic)
	{
		m_NextMusicTime = timeInMs + static_cast<u32>(audEngineUtil::GetRandomNumberInRange(kMinIdleMusicWaitTime, kMaxIdleMusicWaitTime));
	}

	if(!inhibitMusic && !m_StartedMusic && timeInMs > m_NextMusicTime)
	{
		StartMusic();
		m_LastMusicTime = timeInMs;
	}
}

void audIdleMusic::StopMusic()
{
	if(m_StartedMusic)
	{
		StopMusic("STOP_IDLE_MUSIC_MISSION");
	}
}

bool audIdleMusic::TriggerEvent(const char *eventName)
{
	audDisplayf("Idle Music - %s", eventName);
	return g_InteractiveMusicManager.GetEventManager().TriggerEvent(eventName);
}

void audIdleMusic::Reset()
{
	m_LastMusicTime = 0;
	m_NextMusicTime = ~0U;
	m_LastWeaponFireTime = 0U;
	m_StartedMusic = false;
	m_MusicPlayDuration = 0U;
}

void audIdleMusic::StartMusic()
{
	TriggerEvent("START_IDLE_MUSIC");
	m_StartedMusic = true;
	g_InteractiveMusicManager.SetRestoreMusic(false);
	m_MusicPlayDuration = static_cast<u32>(audEngineUtil::GetRandomNumberInRange(kMinIdleMusicPlayTime, kMaxIdleMusicPlayTime));
	audDisplayf("Started Idle music; will play for maximum of %.2f seconds", m_MusicPlayDuration * 0.001f);
}

void audIdleMusic::StopMusic(const char *eventName)
{
	TriggerEvent(eventName);
	m_StartedMusic = false;

	// Reset start timer
	m_NextMusicTime = ~0U;
}

void audIdleMusic::NotifyPlayerFiredWeapon()
{
	m_LastWeaponFireTime = g_InteractiveMusicManager.GetTimeInMs();
}

