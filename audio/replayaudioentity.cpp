

#include "replayaudioentity.h"

#if GTA_REPLAY

#include "audio/frontendaudioentity.h"
#include "frontend/VideoEditor/Core/VideoProjectAudioTrackProvider.h"
#include "replaycoordinator/ReplayAudioTrackProvider.h"
#include "audio/frontendaudioentity.h"

#if __BANK
bool g_ReplayAudEntityDebugDraw = false;
#endif

audReplayAudioEntity g_ReplayAudioEntity;

audReplayAudioEntity::audReplayAudioEntity()
{
	m_ShouldDuckMusic = false;
	m_PreviewMarkerSFX = false;
	m_PreviewSFXStartTime = 0;
	m_PreviewSFXNameHash = g_NullSoundHash;
	m_SFXVolumeAffectingMusic = 0.0f;
}

audReplayAudioEntity::~audReplayAudioEntity()
{

}

void audReplayAudioEntity::Init()
{
	naAudioEntity::Init();
	audEntity::SetName("audReplayAudioEntity");

	ReplayStreamSound streamSound;
	streamSound.m_Duration = 0;
	streamSound.m_Sound = NULL;
	streamSound.m_Status = REPLAY_STREAM_STATUS_IDLE;
	streamSound.m_StreamSlot = NULL;
	streamSound.m_MarkerIndex = -1;
	streamSound.m_Type = REPLAY_STREAM_SFX;
	streamSound.m_IsValid = false;
    streamSound.m_IsPreview = false;
	streamSound.m_ShouldPlay = false;

	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		m_ReplayStream.Push(streamSound);
	}

	for(int i=0; i<CMontage::MAX_MARKERS_PER_CLIP; i++)
	{
		m_MarkerSFXValid[i] = true;
	}
	m_ShouldDuckMusic = false;
	m_PreviewMarkerSFX = false;
	m_PreviewSFXStartTime = 0;
	m_PreviewSFXNameHash = g_NullSoundHash;
	m_SFXVolumeAffectingMusic = 0.0f;

}


void audReplayAudioEntity::SetPreviewMarkerSFX(bool preview, u32 sfxNameHash)
{ 
	m_PreviewMarkerSFX = preview; 
	if(preview)
	{
		m_PreviewSFXStartTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
		m_PreviewSFXNameHash = sfxNameHash;
	}
	else
	{
		m_PreviewSFXStartTime = 0;
		m_PreviewSFXNameHash = g_NullSoundHash;
	}
}

void audReplayAudioEntity::UpdatePlay(s32 slot)
{
	if(!m_ReplayStream[slot].m_Sound || m_ReplayStream[slot].m_Type == REPLAY_STREAM_AMBIENT_STOP)
	{
		Stop(slot);
		return;
	}
	
	if(m_ReplayStream[slot].m_Sound && m_ReplayStream[slot].m_Type == REPLAY_STREAM_AMBIENT)
	{
		if(m_ReplayStream[slot].m_Sound->GetPlayState() == AUD_SOUND_PLAYING && m_ReplayStream[slot].m_Sound->GetSoundTypeID() == StreamingSound::TYPE_ID)
		{
			const audStreamingSound *sound = reinterpret_cast<const audStreamingSound *>(m_ReplayStream[slot].m_Sound);
			for(s32 i = 0; i < sound->GetWavePlayerIdCount(); i++)
			{
				const s32 wavePlayer = sound->GetWavePlayerId(i);
				if(wavePlayer != -1)
				{
					audPcmSourceInterface::SetParam(wavePlayer, audPcmSource::Params::PauseGroup, 0U);
				}
			}
		}
	}

	UpdateVolume(slot);
}

void audReplayAudioEntity::UpdateVolume(s32 slot)
{
	// Set the appropriate volume on any non-preview sounds
	if(m_ReplayStream[slot].m_Sound && !m_ReplayStream[slot].m_IsPreview)
	{
		switch(m_ReplayStream[slot].m_Type)
		{
		case REPLAY_STREAM_SFX:
			m_ReplayStream[slot].m_Sound->SetRequestedVolume(g_FrontendAudioEntity.GetReplaySFXOneshotVolume());
			break;

		case REPLAY_STREAM_AMBIENT:
			m_ReplayStream[slot].m_Sound->SetRequestedVolume(g_FrontendAudioEntity.GetReplayCustomAmbienceVolume());
			break;

		default:
			break;
		}
	}
}

void audReplayAudioEntity::UpdateFree(s32 slot)
{
	if(m_ReplayStream[slot].m_Sound)
	{
		m_ReplayStream[slot].m_Sound->StopAndForget();
	}

	if(HasSoundStopped(slot))
	{
		ClearReplayStream(slot);
	}
}

void audReplayAudioEntity::PreUpdateService(u32 UNUSED_PARAM(timeInMs))
{
	//if(!CReplayMgr::IsReplayInControlOfWorld())
	//	return;

#if __BANK
	if(g_ReplayAudEntityDebugDraw)
	{
		DebugDraw();
	}
#endif

	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		switch(m_ReplayStream[i].m_Status)
		{
		case REPLAY_STREAM_STATUS_IDLE:
			break;

		case REPLAY_STREAM_STATUS_PRELOADING:
			UpdatePreload(i);
			break;

		case REPLAY_STREAM_STATUS_PRELOADED:
			if(m_ReplayStream[i].m_ShouldPlay)
			{
				PlayInternal(i);
			}
			break;

		case REPLAY_STREAM_STATUS_PLAYING:
			UpdatePlay(i);
			break;

		case REPLAY_STREAM_STATUS_FREEING:
			UpdateFree(i);
			break;
		}
	}

	if(m_PreviewSFXNameHash != g_NullSoundHash)
	{
		g_ReplayAudioEntity.StopAllPreviewSFX(m_PreviewSFXNameHash);
		u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
		if(currentTime > m_PreviewSFXStartTime + REPLAY_SFX_PREVIEW_WAIT_TIME)
		{
			Preload(REPLAY_SFX_PREVIEW_MARKER_ID, m_PreviewSFXNameHash, REPLAY_STREAM_SFX, 0);
			if(g_ReplayAudioEntity.IsReadyToPlay(REPLAY_SFX_PREVIEW_MARKER_ID, m_PreviewSFXNameHash))
			{
				Displayf("Preview SFX on marker %d", REPLAY_SFX_PREVIEW_MARKER_ID);
				g_ReplayAudioEntity.Play(REPLAY_SFX_PREVIEW_MARKER_ID, m_PreviewSFXNameHash, true);
				m_PreviewSFXStartTime = 0;
				m_PreviewSFXNameHash = g_NullSoundHash;
				m_PreviewMarkerSFX = false;
			}
		}
	}
}

void audReplayAudioEntity::Shutdown()
{
	StopAllStreams();
}

void audReplayAudioEntity::StopAllPreviewSFX(u32 preserveSoundHash)
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_MarkerIndex == REPLAY_SFX_PREVIEW_MARKER_ID && m_ReplayStream[i].m_SoundHash != preserveSoundHash)
		{
			Stop(i, false);
		}
	}
}

void audReplayAudioEntity::ClearReplayStream(s32 index)
{
	if(m_ReplayStream[index].m_Sound)
	{
		m_ReplayStream[index].m_Sound->StopAndForget();
	}
	m_ReplayStream[index].m_Sound = NULL;

	if(m_ReplayStream[index].m_StreamSlot)
	{
		m_ReplayStream[index].m_StreamSlot->Free();
	}
	m_ReplayStream[index].m_StreamSlot = NULL;

	m_ReplayStream[index].m_Duration = 0;
	m_ReplayStream[index].m_Status = REPLAY_STREAM_STATUS_IDLE;
	m_ReplayStream[index].m_MarkerIndex = -1;
	m_ReplayStream[index].m_Type = REPLAY_STREAM_SFX;
	m_ReplayStream[index].m_IsValid = false;
	m_ReplayStream[index].m_ShouldPlay = false;

}

void audReplayAudioEntity::InvalidateSFX()
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_MarkerIndex != REPLAY_SFX_PREVIEW_MARKER_ID)
		{
			m_ReplayStream[i].m_IsValid = false;
		}
	}
}

void audReplayAudioEntity::ClearInvalidSFX()
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_Type == REPLAY_STREAM_SFX && m_ReplayStream[i].m_IsValid == false)
		{
			Stop(i, false);
		}
	}
}

bool audReplayAudioEntity::OnStreamStopCallback(u32 userData)
{
	g_ReplayAudioEntity.Stop(userData);
	return true;
}

bool audReplayAudioEntity::HasStreamStoppedCallback(u32 userData)
{
	return g_ReplayAudioEntity.HasSoundStopped(userData);
}

bool audReplayAudioEntity::HasSoundStopped(u32 userData)
{
	bool hasStopped = true;
	if(m_ReplayStream[userData].m_StreamSlot)
	{
		//Check if we are still loading or playing from our allocated wave slot
		audWaveSlot *waveSlot = m_ReplayStream[userData].m_StreamSlot->GetWaveSlot();
		if(waveSlot && (waveSlot->GetIsLoading() || (waveSlot->GetReferenceCount() > 0)))
		{
			hasStopped = false;
		}
	}
	return hasStopped;
}

bool audReplayAudioEntity::AreAllStreamingSoundsPrepared()
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_Type != REPLAY_STREAM_AMBIENT_STOP)
		{			
			if(m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PRELOADING ||
				m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_FREEING)
			{
				return false;
			}			
		}
	}

	return true;
}

s32 audReplayAudioEntity::Preload(s32 markerIndex, u32 nameHash, audReplayStreamType type, u32 startOffset)
{
	//if(!CReplayMgr::IsReplayInControlOfWorld())
	//	return -1;

	s32 adjustedMarkerIndex = (type == REPLAY_STREAM_SFX ? markerIndex : markerIndex + REPLAY_AMBIENT_TRACK_MARKER_ID);
	if(nameHash == g_NullSoundHash)
	{
		return -1;
	}

	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_MarkerIndex == adjustedMarkerIndex &&
			m_ReplayStream[i].m_SoundHash == nameHash && 
			m_ReplayStream[i].m_Type != REPLAY_STREAM_AMBIENT_STOP && 
			(m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PRELOADING || 
			 m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PRELOADED ||
			 m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PLAYING))
		{
			m_ReplayStream[i].m_IsValid = true;
			return i;
		}
	}

	s32 slot = -1;
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_IDLE)
		{
			slot = i;
			break;
		}
	}
	if(slot == -1 )
	{
		return -1;
	}

	m_ReplayStream[slot].m_Status = REPLAY_STREAM_STATUS_PRELOADING;
	m_ReplayStream[slot].m_MarkerIndex = adjustedMarkerIndex;
	m_ReplayStream[slot].m_SoundHash = nameHash;
	m_ReplayStream[slot].m_Type = type;
	m_ReplayStream[slot].m_IsValid = true;
	m_ReplayStream[slot].m_StartOffset = startOffset;

	return slot;
}

void audReplayAudioEntity::UpdatePreload(s32 slot)
{
	if(!m_ReplayStream[slot].m_StreamSlot)
	{
		audStreamClientSettings settings;
		settings.stopCallback = &OnStreamStopCallback;
		settings.userData = slot;
		m_ReplayStream[slot].m_StreamSlot = audStreamSlot::AllocateSlot(&settings);
	}	

	if(m_ReplayStream[slot].m_StreamSlot)
	{	
		if(!m_ReplayStream[slot].m_Sound)
		{
			audSoundInitParams initParams;
			initParams.EffectRoute = EFFECT_ROUTE_FRONT_END;
			initParams.StartOffset = m_ReplayStream[slot].m_StartOffset;
			if(m_ReplayStream[slot].m_Type == REPLAY_STREAM_AMBIENT)
			{
				initParams.AttackTime = REPLAY_AMBIENT_ATTACK_TIME;
			}
			initParams.TimerId = 1;
			initParams.Category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("MUSIC_SLIDER", 0xA4D158B0));
			Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
			CreateSound_PersistentReference(m_ReplayStream[slot].m_SoundHash, &m_ReplayStream[slot].m_Sound, &initParams);
			if(!m_ReplayStream[slot].m_Sound)
			{
				m_ReplayStream[slot].m_Status = REPLAY_STREAM_STATUS_FREEING;
				return;
			}			
			m_ReplayStream[slot].m_Duration = CVideoProjectAudioTrackProvider::GetMusicTrackDurationMs(m_ReplayStream[slot].m_SoundHash);
		}
		else
		{
			if(m_ReplayStream[slot].m_Sound->Prepare(m_ReplayStream[slot].m_StreamSlot->GetWaveSlot(), true) == AUD_PREPARED)
			{
				m_ReplayStream[slot].m_Status = REPLAY_STREAM_STATUS_PRELOADED;
			}
		}
	}
}

void audReplayAudioEntity::PlayAmbient(s32 markerIndex, u32 nameHash, bool isPreview)
{
	Play(markerIndex + REPLAY_AMBIENT_TRACK_MARKER_ID, nameHash, isPreview);
}

void audReplayAudioEntity::Play(s32 markerIndex, u32 nameHash, bool isPreview)
{
	s32 slot = -1;

	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_MarkerIndex == markerIndex &&
			m_ReplayStream[i].m_SoundHash == nameHash && 
			m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PRELOADED)
		{
			slot = i;
			break;
		}
	}

	if(slot < 0)
		return;
	
	m_ReplayStream[slot].m_IsPreview = isPreview;
	m_ReplayStream[slot].m_ShouldPlay = true;
}

void audReplayAudioEntity::PlayInternal(s32 slot)
{
	audAssert(m_ReplayStream[slot].m_Status == REPLAY_STREAM_STATUS_PRELOADED);

	if(!m_ReplayStream[slot].m_IsPreview)
	{
		// Support volume interpolation whilst the sound is playing, but jump to the target
		// volume as soon as a new one starts (assumes only 1 instance can be active at once)
		switch(m_ReplayStream[slot].m_Type)
		{
		case REPLAY_STREAM_SFX:
			g_FrontendAudioEntity.JumpToTargetSFXOneShotVolume();
			break;

		case REPLAY_STREAM_AMBIENT:
			g_FrontendAudioEntity.JumpToTargetCustomAmbienceVolume();
			break;

		default:
			break;
		}
	}

	if(m_ReplayStream[slot].m_Sound)
	{
		m_ReplayStream[slot].m_Sound->StopAndForget();
	}

	audSoundInitParams initParams;
	initParams.EffectRoute = EFFECT_ROUTE_FRONT_END;
	initParams.StartOffset = m_ReplayStream[slot].m_StartOffset;
	initParams.Category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("MUSIC_SLIDER", 0xA4D158B0));
	Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());

	if(m_ReplayStream[slot].m_IsPreview)
	{
		initParams.TimerId = 1;
	}

	initParams.WaveSlot = m_ReplayStream[slot].m_StreamSlot->GetWaveSlot();
	CreateSound_PersistentReference(m_ReplayStream[slot].m_SoundHash, &m_ReplayStream[slot].m_Sound, &initParams);

	if(m_ReplayStream[slot].m_Sound)	
	{
		if(m_ReplayStream[slot].m_Type == REPLAY_STREAM_AMBIENT)
		{
			m_ReplayStream[slot].m_Sound->SetShouldPersistOverClears(true);
		}
			
		m_ReplayStream[slot].m_Duration = CVideoProjectAudioTrackProvider::GetMusicTrackDurationMs(m_ReplayStream[slot].m_SoundHash);
		m_ReplayStream[slot].m_Status = REPLAY_STREAM_STATUS_PLAYING;		
		m_ReplayStream[slot].m_Sound->PrepareAndPlay(m_ReplayStream[slot].m_StreamSlot->GetWaveSlot(), false, 5000);
		m_ReplayStream[slot].m_ShouldPlay = false;
		UpdateVolume(slot);
	}
	else
	{
		m_ReplayStream[slot].m_Status = REPLAY_STREAM_STATUS_FREEING;
		return;
	}

	s32 markerIndex = m_ReplayStream[slot].m_MarkerIndex;

	//stop any other SFX sounds, leave ambient
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		// This will look for other sfx markers and stop the old sound if a new sound is played.
		// A marker number 10000+ is an ambient track and won't stop an sfx track.
		if(i != slot)
		{			
			if(m_ReplayStream[i].m_MarkerIndex != markerIndex &&			
				markerIndex < 10000 &&			
				m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PLAYING &&
				m_ReplayStream[i].m_Type == REPLAY_STREAM_SFX)
			{
				Stop(i);
			}
		}		
	}
}

void audReplayAudioEntity::StopExpiredAmbientTracks(s32 markerIndex)
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_Type == REPLAY_STREAM_AMBIENT && m_ReplayStream[i].m_MarkerIndex != markerIndex + REPLAY_AMBIENT_TRACK_MARKER_ID)
		{
			Stop(i, true);
		}
	}
}

bool audReplayAudioEntity::IsLoading(s32 markerIndex, u32 nameHash)
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_MarkerIndex == markerIndex &&
			m_ReplayStream[i].m_SoundHash == nameHash && 
			m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PRELOADING)
		{
			return true;
		}
	}
	return false;
}

bool audReplayAudioEntity::IsLoaded(s32 markerIndex, u32 nameHash)
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_MarkerIndex == markerIndex &&
			m_ReplayStream[i].m_SoundHash == nameHash && 
			m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PRELOADED)
		{
			return true;
		}
	}
	return false;
}

bool audReplayAudioEntity::IsReadyToPlay(s32 markerIndex, u32 nameHash)
{
	return IsLoaded(markerIndex, nameHash) && !IsPlaying(markerIndex, nameHash);	
}

bool audReplayAudioEntity::IsAmbientReadyToPlay(s32 markerIndex, u32 nameHash)
{
	return IsLoaded(markerIndex + REPLAY_AMBIENT_TRACK_MARKER_ID, nameHash) && !IsPlaying(markerIndex + REPLAY_AMBIENT_TRACK_MARKER_ID, nameHash);	
}

bool audReplayAudioEntity::IsLoadedOrPlaying(s32 markerIndex, u32 nameHash)
{
	return IsLoaded(markerIndex, nameHash) || IsPlaying(markerIndex, nameHash);	
}

bool audReplayAudioEntity::IsPlaying(s32 markerIndex, u32 nameHash)
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_MarkerIndex == markerIndex &&
			m_ReplayStream[i].m_SoundHash == nameHash && 
			m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PLAYING)
		{
			return true;
		}
	}
	return false;
}

bool audReplayAudioEntity::IsAmbientPlaying(s32 markerIndex, u32 nameHash)
{
	return IsPlaying(markerIndex + REPLAY_AMBIENT_TRACK_MARKER_ID, nameHash);
}

bool audReplayAudioEntity::IsSFXPlaying()
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_Type == REPLAY_STREAM_SFX &&
			m_ReplayStream[i].m_Sound && 
			m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PLAYING)
		{
			return true;
		}
	}
	return false;
}

void audReplayAudioEntity::Stop(s32 markerIndex, u32 nameHash, bool release)
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_MarkerIndex == markerIndex &&
			m_ReplayStream[i].m_SoundHash == nameHash && 
			m_ReplayStream[i].m_Sound)
		{
			Stop(i, release);
			return;
		}
	}
}

void audReplayAudioEntity::Stop(s32 slot, bool release)
{
	if(slot < 0)
		return;

	if(m_ReplayStream[slot].m_Sound)
	{
		if(release)
		{
			if(m_ReplayStream[slot].m_Type == REPLAY_STREAM_AMBIENT || m_ReplayStream[slot].m_Type == REPLAY_STREAM_AMBIENT_STOP)
				m_ReplayStream[slot].m_Sound->SetReleaseTime(REPLAY_AMBIENT_RELEASE_TIME);
			else
				m_ReplayStream[slot].m_Sound->SetReleaseTime(REPLAY_SFX_RELEASE_TIME);
		}
	}

	if(m_ReplayStream[slot].m_Status != REPLAY_STREAM_STATUS_IDLE)
	{
		m_ReplayStream[slot].m_Status = REPLAY_STREAM_STATUS_FREEING;
	}
	m_ReplayStream[slot].m_MarkerIndex = -1;
	m_ReplayStream[slot].m_Duration = 0;
	m_ReplayStream[slot].m_SoundHash = g_NullSoundHash;
}

void audReplayAudioEntity::StopAllStreams()
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		Stop(i, false);
	}
}

void audReplayAudioEntity::StopAllSFX()
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_Type == REPLAY_STREAM_SFX)
		{
			Stop(i, false);
		}
	}
}

void audReplayAudioEntity::StopAllPlayingSFX()
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_Type == REPLAY_STREAM_SFX &&	m_ReplayStream[i].m_Status == REPLAY_STREAM_STATUS_PLAYING)
		{
			Stop(i, true);
		}
	}
}

void audReplayAudioEntity::StopAllAmbient(bool release)
{
	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{
		if(m_ReplayStream[i].m_Type == REPLAY_STREAM_AMBIENT)
		{
			Stop(i, release);
		}
	}
}


#if __BANK	
void audReplayAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("ReplayAudioEntity", false);

	bank.AddToggle("DebugDraw", &g_ReplayAudEntityDebugDraw);

	bank.PopGroup();
}

const char* ReplayStatusStr[] = {
	"REPLAY_STREAM_STATUS_IDLE",
	"REPLAY_STREAM_STATUS_PRELOADING",
	"REPLAY_STREAM_STATUS_PRELOADED",
	"REPLAY_STREAM_STATUS_PLAYING",
	"REPLAY_STREAM_STATUS_FREEING"
};

const char* ReplaySoundType[] = {
	"REPLAY_STREAM_SFX",
	"REPLAY_STREAM_AMBIENT",
	"REPLAY_STREAM_AMBIENT_STOP"
};

const char *prio[] = 	
{
	"PrioIdle",
	"PrioEntityEmitter_Far",
	"PrioEntityEmitter_Near",
	"PrioStaticEmitter",
	"PrioEntityEmitter_Network",
	"ScriptedVehicleRadio",
	"PrioFrontendMenu",
	"PrioScript",
	"PrioScriptEmitter",
	"PrioMusic",
	"PrioPlayerRadio",
	"PrioReplayMusic",
	"PrioCutscene",
};

const char *states[] =
{
	"Idle",
	"Active",
	"Freeing"
};

void audReplayAudioEntity::DebugDraw()
{
	char tempString[256];
	static bank_float lineInc = 0.02f;
	static f32 lineBegin = 0.3f;
	static f32 lineLeft = 0.3f;
	f32 lineBase = lineBegin;

	for(int i=0; i<NUM_REPLAY_STREAM_SOUNDS; i++)
	{

#define THE_NEXT_LINE	grcDebugDraw::Text(Vector2(lineLeft, lineBase), Color32(255,255,255), tempString ); lineBase += lineInc;

		sprintf(tempString, "ReplaySlot : %d %d %u %s %d %d %d %s %s", i, 
			m_ReplayStream[i].m_MarkerIndex, m_ReplayStream[i].m_SoundHash,
			m_ReplayStream[i].m_IsValid ? "VALID":"-----",
			m_ReplayStream[i].m_StartOffset,
			m_ReplayStream[i].m_Sound ? 1:0, m_ReplayStream[i].m_StreamSlot ? 1:0,
			ReplayStatusStr[m_ReplayStream[i].m_Status], ReplaySoundType[m_ReplayStream[i].m_Type]); THE_NEXT_LINE;
	}

	sprintf(tempString, "");
	THE_NEXT_LINE;
	THE_NEXT_LINE;
	
	for(u32 slotIndex=0; slotIndex < g_NumStreamSlots; slotIndex++)
	{
		audStreamSlot* slot = audStreamSlot::GetSlot(slotIndex);
		if(slot)
		{
			sprintf(tempString,"StreamSlot: %s %s Priority: [%s]", slot->GetWaveSlot()->GetSlotName(), states[slot->GetState()], prio[slot->GetPriority()]); THE_NEXT_LINE;
		}
	}
}
#endif

#endif


