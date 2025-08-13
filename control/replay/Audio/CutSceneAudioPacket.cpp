#include "CutSceneAudioPacket.h"

#if GTA_REPLAY
#include "audio/cutsceneaudioentity.h" 
#include "audio/northaudioengine.h"
#include "audio/pedaudioentity.h"
#include "Control/Replay/ReplayInterfacePed.h"
#include "control/replay/ReplayInternal.h"
#include "control/replay/audio/SoundPacket.h"
#include "cutscene/CutSceneManagerNew.h"
#include "cutscene/CutSceneStore.h"
#include "peds/ped.h" 

////////////////////////////////////////////////////////////
CCutSceneAudioPacket::CCutSceneAudioPacket(u32 trackTime, u32 startTime)
	: CPacketEvent(PACKETID_SOUND_CUTSCENE, sizeof(CCutSceneAudioPacket))
	, CPacketEntityInfo()
{
	m_CutFileHash = 0;
	if(CutSceneManager::GetInstance())
	{
		m_CutFileHash = CutSceneManager::GetInstance()->GetSceneHashString()->GetHash();
	}

	m_TrackTime = trackTime;
	m_Startime = startTime;
}

void CCutSceneAudioPacket::Extract(const CEventInfo<void>&) const
{
	if(g_CutsceneAudioEntity.IsCutsceneTrackPrepared() &&
	   CutSceneManager::GetInstance()->GetSceneHashString()->IsNotNull() &&
	   CutSceneManager::GetInstance()->GetSceneHashString()->GetHash() == m_CutFileHash)
	{
		g_CutsceneAudioEntity.TriggerCutscene();

		//Keep replay in-sync with the cut scene audio
		if(m_TrackTime != g_CutsceneAudioEntity.GetCutsceneTime() )
		{
			replayDebugf1("Cut scene not in sync PacketTime:%i AudioTime:%i", m_TrackTime, g_CutsceneAudioEntity.GetCutsceneTime());
			
			CReplayMgr::SetCutSceneAudioSync(g_CutsceneAudioEntity.GetCutsceneTime() - m_TrackTime);
		}	
	}
}

ePreloadResult CCutSceneAudioPacket::Preload(const CEventInfo<void>& eventInfo) const
{

	if(eventInfo.GetPreloadOffsetTime() < 0)
	{
		return PRELOAD_SUCCESSFUL;
	}

	if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_CURSOR_SPEED) || eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK))
	{
		return PRELOAD_SUCCESSFUL;
	}

	if(!replayVerify(CutSceneManager::GetInstance()))
	{
		//Unable to handle this packet, need to stop preloading
		return PRELOAD_SUCCESSFUL;
	}
	
	if(!replayVerify(m_CutFileHash != 0))
	{
		//Unable to handle this packet, need to stop preloading
		return PRELOAD_SUCCESSFUL;
	}

	if(CutSceneManager::GetInstance()->GetSceneHashString()->IsNotNull() && CutSceneManager::GetInstance()->GetSceneHashString()->GetHash() != m_CutFileHash)
	{
		//Unable to handle this packet, already loading a different cutscene file
		return PRELOAD_SUCCESSFUL;
	}

	if(audNorthAudioEngine::IsAudioUpdateCurrentlyRunning())
	{
		replayDebugf1("CCutSceneAudioPacket audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), preloading will fail");
		return PRELOAD_FAIL;
	}

	if(audNorthAudioEngine::IsAudioPaused())
	{
		replayDebugf1("CCutSceneAudioPacket audNorthAudioEngine::IsAudioPaused(), preloading will fail this frame");
		return PRELOAD_FAIL;
	}

	if(!replayVerifyf(CutSceneManager::GetInstance()->GetSceneHashString()->GetHash() == 0 || m_CutFileHash == CutSceneManager::GetInstance()->GetSceneHashString()->GetHash(), "CCutSceneAudioPacket can't load 2 cutscenes at once"))
	{
		return PRELOAD_FAIL;
	}

	CutSceneManager::GetInstance()->SetSceneHashString(m_CutFileHash);

	bool bIsLoaded = false;

	//wait for our streaming system to stream in the files
	if(CutSceneManager::GetInstance()->LoadCutFile("") && g_CutSceneStore.HasObjectLoaded(g_CutSceneStore.FindSlotFromHashKey(m_CutFileHash)) )
	{
		//wait for audio to not be prepared
		if(!g_CutsceneAudioEntity.IsCutsceneTrackPrepared())
		{
			if(CutSceneManager::GetInstance()->ReplayLoadCutFile())
			{
				s32 timeToLoad = m_TrackTime - eventInfo.GetPreloadOffsetTime(); 
				if(timeToLoad < m_Startime)
				{
					timeToLoad = m_Startime;
				}

				g_CutsceneAudioEntity.InitCutsceneTrack((u32)timeToLoad);
			}
		}
		else
		{
			bIsLoaded = true;
		}
	}	
	
	return bIsLoaded ? PRELOAD_SUCCESSFUL : PRELOAD_FAIL;
}


////////////////////////////////////////////////////////////
CCutSceneStopAudioPacket::CCutSceneStopAudioPacket(bool isSkipping, u32 releaseTime)
	: CPacketEvent(PACKETID_SOUND_CUTSCENE_STOP, sizeof(CCutSceneStopAudioPacket))
	, CPacketEntityInfo()
{
	m_IsSkipping = isSkipping;
	m_ReleaseTime = releaseTime;
}

void CCutSceneStopAudioPacket::Extract(const CEventInfo<void>&) const
{
	if(g_CutsceneAudioEntity.IsCutsceneTrackPrepared())
	{
		g_CutsceneAudioEntity.StopCutscene(m_IsSkipping, m_ReleaseTime);
	}
}


////////////////////////////////////////////////////////////
CSynchSceneAudioPacket::CSynchSceneAudioPacket(u32 hash, const char* sceneName, u32 sceneTime, bool usePosition, bool useEntity, Vector3 pos)
	: CPacketEvent(PACKETID_SOUND_SYNCHSCENE, sizeof(CSynchSceneAudioPacket))
	, CPacketEntityInfo()
{
	m_Hash = hash;
	safecpy(m_SceneName, sceneName);
	m_SceneTime = sceneTime;
	m_UsePosition = usePosition;
	m_Position.Store(pos);
	m_UseEntity = useEntity;
}

void CSynchSceneAudioPacket::Extract(const CEventInfo<CEntity>&) const
{
	//Always init scene, IsSyncSceneTrackPrepared init with zero time if not.
	if(!g_CutsceneAudioEntity.HasSyncSceneBeenInitialized(m_SceneName))
	{
		g_CutsceneAudioEntity.InitSynchedScene(m_SceneName,m_SceneTime);
	}

	if(g_CutsceneAudioEntity.IsSyncSceneTrackPrepared(m_SceneName))
	{
		g_CutsceneAudioEntity.TriggerSyncScene(m_Hash);

		//Keep replay in-sync with the synch scene audio
		u32 synchTime =  g_CutsceneAudioEntity.GetSyncSceneTime(m_Hash);
		if(m_SceneTime != synchTime)
		{
			replayDebugf1("Synched scene not in sync PacketTime:%i AudioTime:%i", m_SceneTime, synchTime);

			CReplayMgr::SetCutSceneAudioSync(synchTime - m_SceneTime);
		}
	}
}

ePreloadResult CSynchSceneAudioPacket::Preload(const CEventInfo<CEntity>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_CURSOR_SPEED))
	{
		return PRELOAD_SUCCESSFUL;
	}

	if(audNorthAudioEngine::IsAudioUpdateCurrentlyRunning())
	{
		replayDebugf1("CSynchSceneAudioPacket audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), preloading will fail");
		return PRELOAD_FAIL;
	}

	if(!g_CutsceneAudioEntity.HasSyncSceneBeenInitialized(m_SceneName))
	{
		s32 timeToLoad = m_SceneTime - eventInfo.GetPreloadOffsetTime(); 
		g_CutsceneAudioEntity.InitSynchedScene(m_SceneName,Max(0,timeToLoad));
	}

	if(g_CutsceneAudioEntity.HasSyncSceneBeenTriggered(m_SceneName))
	{
		return PRELOAD_SUCCESSFUL;
	}

	if(m_UsePosition)
	{
		Vector3 pos;
		m_Position.Load(pos);
		g_CutsceneAudioEntity.SetScenePlayPosition(m_Hash, VECTOR3_TO_VEC3V(pos));
	}

	if(m_UseEntity && eventInfo.GetEntity() != NULL)
	{
		g_CutsceneAudioEntity.SetScenePlayEntity(m_Hash, eventInfo.GetEntity());
	}

	//Wait for audio to be prepared
	if(!g_CutsceneAudioEntity.IsSyncSceneTrackPrepared(m_SceneName))
	{
		return PRELOAD_FAIL;
	}

	return PRELOAD_SUCCESSFUL;
}


////////////////////////////////////////////////////////////
CSynchSceneStopAudioPacket::CSynchSceneStopAudioPacket(u32 hash)
	: CPacketEvent(PACKETID_SOUND_SYNCHSCENE_STOP, sizeof(CSynchSceneStopAudioPacket))
	, CPacketEntityInfo()
{
	m_Hash = hash;
}

void CSynchSceneStopAudioPacket::Extract(const CEventInfo<void>&) const
{
	if(m_Hash != 0)
	{
		g_CutsceneAudioEntity.StopSyncScene(m_Hash);
	}
	else
	{
		g_CutsceneAudioEntity.StopSyncScene();
	}
}

#endif // GTA_REPLAY
