// 
// audio/music/wantedmusic.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#include "wantedmusic.h"
#include "musicplayer.h"
#include "audio/dynamicmixer.h"
#include "audio/northaudioengine.h"
#include "audio/scriptaudioentity.h"
#include "game/wanted.h"
#include "peds/ped.h"
#include "peds/PlayerInfo.h"
#include "script/script.h"

#include "system/param.h"

AUDIO_MUSIC_OPTIMISATIONS()

PARAM(nowantedmusic, "Disable interactive music triggered by wanted level");

void audWantedMusic::Init()
{
	m_Scene = NULL;
	Reset();
}

void audWantedMusic::Shutdown()
{

}

void audWantedMusic::Update(const u32 UNUSED_PARAM(timeInMs))
{
	if((!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::WantedMusicOnMission) && CTheScripts::GetPlayerIsOnAMission()) ||
		(!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::WantedMusicOnMission) && audNorthAudioEngine::IsScreenFadedOut()) ||
		 g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::WantedMusicDisabled) || PARAM_nowantedmusic.Get() REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()))
	{
		// Assume script is taking over; reset our state and bail
		if(m_LastWantedLevel >= WANTED_LEVEL3)
		{
			TriggerEvent("WANTED_MUSIC_DISABLED");
		}
		m_LastWantedLevel = WANTED_CLEAN;
		m_TriggeredEnterVehicle = false;
		m_WasOutsideCircle = false;
		m_StartedMusic = false;
		StopScene();

		m_DisabledTimer = 5.f;
		return;
	}
	else
	{
		m_DisabledTimer -= fwTimer::GetTimeStep();
		if(m_DisabledTimer > 0.f)
		{
			m_LastWantedLevel = WANTED_CLEAN;
			return;
		}
	}
	char buf[32];
	CPed *player = FindPlayerPed();
	if(player && player->GetPlayerWanted() && player->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_PLAYING)
	{
		const CWanted *wanted = player->GetPlayerWanted();

		const s32 wantedLevel = wanted->GetWantedLevel();

		if(wantedLevel != m_LastWantedLevel)
		{
			if(wantedLevel < WANTED_LEVEL3)
			{
				if(m_LastWantedLevel >= WANTED_LEVEL3)
				{
					if(g_InteractiveMusicManager.IsScoreMutedForRadio())
					{
						TriggerEvent("STOP_WANTED_MUSIC_MUTED");
					}
					else
					{
						//@@: location AUDWANTEDMUSIC_UPDATE_TRIGGER_ESCAPE_WANTED
						TriggerEvent(formatf(buf, "ESCAPED_WANTED_L%d", m_LastWantedLevel));
					}
					
					m_StartedMusic = false;
					StopScene();
				}
			}
			else if(wantedLevel >= WANTED_LEVEL3)
			{
				if(!m_StartedMusic)
				{
					m_StartedMusic = true;
					StartScene();
					//@@: location AUDWANTEDMUSIC_UPDATE_TRIGGER_GAINED_WANTED
					TriggerEvent("GAINED_WANTED");
				}
				TriggerEvent(formatf(buf, "GAINED_WANTED_L%d", wantedLevel));
			}

			m_LastWantedLevel = wantedLevel;
		}

		const bool copsAreSearching = wanted->CopsAreSearching();
		const bool outsideCircle = (wanted->m_TimeOutsideCircle > 0);

		if(wantedLevel >= WANTED_LEVEL3)
		{			
			if(copsAreSearching != m_WereCopsSearching)
			{
				TriggerEvent(formatf(buf, "%s_WANTED_L%d", copsAreSearching ? "SEARCHING" : "REGAINED", wantedLevel));
			}

			if(outsideCircle != m_WasOutsideCircle)
			{
				TriggerEvent(formatf(buf, "%s_WANTED_L%d", !outsideCircle ? "EVADING" : "REGAINED", wantedLevel));
			}

			bool inVehicleWithRadio = false;
			if(player->GetIsEnteringVehicle() || player->GetVehiclePedInside())
			{
				CVehicle *vehicle = player->GetIsEnteringVehicle() ? player->GetVehiclePedEntering() : player->GetVehiclePedInside();
				if(vehicle && vehicle->GetVehicleAudioEntity())
				{
					if((player->GetIsEnteringVehicle() && vehicle->GetVehicleAudioEntity()->GetHasNormalRadio())
														|| vehicle->GetVehicleAudioEntity()->IsRadioSwitchedOn())
					{
						inVehicleWithRadio = true;
					}
				}
			}

			if(inVehicleWithRadio && !m_TriggeredEnterVehicle)
			{
				TriggerEvent(formatf(buf, "ENTER_VEHICLE_L%d", wantedLevel));
				m_TriggeredEnterVehicle = true;
			}
			else if(!inVehicleWithRadio && m_TriggeredEnterVehicle)
			{
				TriggerEvent(formatf(buf, "EXIT_VEHICLE_L%d", wantedLevel));
				m_TriggeredEnterVehicle = false;
			}
		}
		m_WereCopsSearching = copsAreSearching;
		m_WasOutsideCircle = outsideCircle;
	}
	else if(m_LastWantedLevel >= WANTED_LEVEL3 && m_StartedMusic)
	{
		char buf[32];
		if(g_InteractiveMusicManager.IsScoreMutedForRadio())
		{
			TriggerEvent("STOP_WANTED_MUSIC_MUTED");
		}
		else
		{
			TriggerEvent(formatf(buf, "DIED_L%d", m_LastWantedLevel));
		}

		m_StartedMusic = false;
		StopScene();
		m_DisabledTimer = 15.f;
	}
}

void audWantedMusic::StopMusic()
{	
	if(m_StartedMusic)
	{
		if(m_LastWantedLevel >= WANTED_LEVEL3)
		{
			TriggerEvent("WANTED_MUSIC_DISABLED");
		}
		m_LastWantedLevel = WANTED_CLEAN;
		m_TriggeredEnterVehicle = false;
		m_WasOutsideCircle = false;
		m_StartedMusic = false;
		StopScene();
	}	
}

bool audWantedMusic::TriggerEvent(const char *eventName)
{
	char buf[32];
	formatf(buf, "ME_%s", eventName);
	audDisplayf("Wanted Music - %s", buf);
	g_InteractiveMusicManager.SetRestoreMusic(false);
	return g_InteractiveMusicManager.GetEventManager().TriggerEvent(buf);
}

void audWantedMusic::NotifyArrested()
{
	if(PARAM_nowantedmusic.Get())
		return;

	if(m_LastWantedLevel >= WANTED_LEVEL3 && m_StartedMusic)
	{
		char buf[32];
		if(g_InteractiveMusicManager.IsScoreMutedForRadio())
		{
			TriggerEvent("STOP_WANTED_MUSIC_MUTED");
		}
		else
		{
			TriggerEvent(formatf(buf, "BUSTED_L%d", m_LastWantedLevel));
		}

		m_StartedMusic = false;
		StopScene();
	}
	Reset();
}

void audWantedMusic::NotifyDied()
{
	if(PARAM_nowantedmusic.Get())
		return;

	if(m_LastWantedLevel >= WANTED_LEVEL3 && m_StartedMusic)
	{
		char buf[32];
		if(g_InteractiveMusicManager.IsScoreMutedForRadio())
		{
			TriggerEvent("STOP_WANTED_MUSIC_MUTED");
		}
		else
		{
			TriggerEvent(formatf(buf, "DIED_L%d", m_LastWantedLevel));
		}

		m_StartedMusic = false;
		StopScene();
	}
	Reset();
}

void audWantedMusic::NotifyCloseNetwork()
{
	if(PARAM_nowantedmusic.Get())
		return;

	if(m_LastWantedLevel >= WANTED_LEVEL3 && m_StartedMusic)
	{	
		TriggerEvent("WANTED_MUSIC_DISABLED");
		
		m_StartedMusic = false;
		StopScene();
	}
	Reset();

	m_DisabledTimer = 20.f;
}

void audWantedMusic::Reset()
{
	m_LastWantedLevel = 0;
	m_StartedMusic = false;
	m_WereCopsSearching = false;
	m_WasOutsideCircle = false;
	m_TriggeredEnterVehicle = false;
	m_DisabledTimer = 5.f;
}

void audWantedMusic::StartScene()
{
	if(!m_Scene)
	{
		DYNAMICMIXER.StartScene(ATSTRINGHASH("WANTED_MUSIC_SCENE", 0x753DFE99), &m_Scene);
	}
}

void audWantedMusic::StopScene()
{
	if(m_Scene)
	{
		m_Scene->Stop();
		m_Scene = NULL;
	}
}
