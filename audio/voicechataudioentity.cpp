#include "audiosoundtypes/externalstreamsound.h"
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "audioengine/engine.h"
#include "peds/ped.h"
#include "weapons/WeaponManager.h"
#include "weapons/WeaponTypes.h"
#include "northaudioengine.h"
#include "voicechataudioentity.h"

#include "system/memory.h"
#include "system/param.h"

PARAM(noloudhailereffect, "Disable loudhailer effect for remote players");

void audVoiceChatAudioProvider::Init()
{
	// Set up an audio ring buffer that we can pipe local player voice chat into, for the loudhailer
	// effect.
	enum {localPlayerVoiceDataSize = 4000};
	
	// This ringbuffer will be freed on the mixer thread, when the consuming streamplayer is destroyed
	// We don't have any knowledge of the network heap on that thread, so ensure we allocate the ringbuffer from the main heap.
	sysMemAllocator* pNetworkHeap = &sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

	u8 *localPlayerVoiceData = rage_aligned_new(16) u8[localPlayerVoiceDataSize];	
	m_LocalPlayerRingbuffer = rage_new audReferencedRingBuffer();

	// Restore the network heap
	sysMemAllocator::SetCurrent(*pNetworkHeap);

	m_LocalPlayerRingbuffer->Init(localPlayerVoiceData, localPlayerVoiceDataSize, true);
	m_LocalPlayerRingbuffer->DisableWarnings();

	VoiceChat::SetAudioProvider(this);
}

void audVoiceChatAudioProvider::Shutdown()
{
	if(m_LocalPlayerRingbuffer)
	{
		sysMemAllocator* pNetworkHeap = &sysMemAllocator::GetCurrent();
		sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

		m_LocalPlayerRingbuffer->Release();

		sysMemAllocator::SetCurrent(*pNetworkHeap);

		m_LocalPlayerRingbuffer = NULL;
	}

	VoiceChat::SetAudioProvider(NULL);
}

void audVoiceChatAudioProvider::LocalPlayerAudioReceived(const s16 *data, const u32 lengthSamples)
{
	if(m_LocalPlayerRingbuffer)
	{
		m_LocalPlayerRingbuffer->WriteData(data, lengthSamples<<1);
	}
}

void audVoiceChatAudioProvider::NotifyRemotePlayerAdded(const rlGamerId &gamerId)
{
	if(PARAM_noloudhailereffect.Get())
	{
		return;
	}
	RemoteTalkerInfo &talker = m_RemoteTalkers.Grow();

	enum {remotePlayerBufferSize = 4000};
	u8 *voiceData = rage_aligned_new(16) u8[remotePlayerBufferSize];
	talker.ringBuffer = rage_new audReferencedRingBuffer();
	talker.ringBuffer->Init(voiceData, remotePlayerBufferSize, true);
	talker.ringBuffer->DisableWarnings();
	talker.gamerId = gamerId;
}

void audVoiceChatAudioProvider::NotifyRemotePlayerRemoved(const rlGamerId &gamerId)
{
	for(s32 i = 0; i < m_RemoteTalkers.GetCount(); i++)
	{
		if(m_RemoteTalkers[i].gamerId == gamerId)
		{
			m_RemoteTalkers[i].ringBuffer->Release();
			m_RemoteTalkers.Delete(i);
			return;
		}
	}
}

void audVoiceChatAudioProvider::RemoveAllRemotePlayers()
{
	for(s32 i = 0; i < m_RemoteTalkers.GetCount(); i++)
	{
		m_RemoteTalkers[i].ringBuffer->Release();
	}
	m_RemoteTalkers.Reset();
}

void audVoiceChatAudioProvider::RemotePlayerAudioReceived(const s16 *data, const u32 lengthSamples, const rlGamerId &gamerId)
{
	for(s32 i = 0; i < m_RemoteTalkers.GetCount(); i++)
	{
		if(m_RemoteTalkers[i].gamerId == gamerId)
		{
			m_RemoteTalkers[i].ringBuffer->WriteData(data, lengthSamples<<1);
			return;
		}
	}
}

audReferencedRingBuffer *audVoiceChatAudioProvider::FindRemotePlayerRingBuffer(const rlGamerId &gamerId)
{
	for(s32 i = 0; i < m_RemoteTalkers.GetCount(); i++)
	{
		if(m_RemoteTalkers[i].gamerId == gamerId)
		{
			return m_RemoteTalkers[i].ringBuffer;
		}
	}
	return NULL;
}

audVoiceChatAudioEntity::audVoiceChatAudioEntity()
	: m_LocalPlayerSound(NULL)
	, m_LocalPlayerSubmixSound(NULL)
	, m_LocalPlayerHasLoudhailer(false)
{

}

void audVoiceChatAudioEntity::Init()
{
	fwAudioEntity::Init();

	m_SoundSet.Init(ATSTRINGHASH("VoiceChat_Sounds", 0xC323CC11));

	audSoundInitParams initParams;
	initParams.Volume = -100.f;
	initParams.EffectRoute = EFFECT_ROUTE_LOCAL_VOICE;
	Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
	CreateSound_PersistentReference(m_SoundSet.Find(ATSTRINGHASH("Local_Player_Voice", 0x8460F301)), &m_LocalPlayerSound, &initParams);

	m_Provider.Init();

	if(m_LocalPlayerSound && m_LocalPlayerSound->GetSoundTypeID() == ExternalStreamSound::TYPE_ID)
	{
		
		audExternalStreamSound *streamSound = reinterpret_cast<audExternalStreamSound*>(m_LocalPlayerSound);
		streamSound->InitStreamPlayer(m_Provider.GetLocalPlayerRingBuffer(), 1U, 16000U);
		streamSound->SetRequestedSourceEffectMix(1.f, 0.f);
		streamSound->PrepareAndPlay();
		
		audSoundInitParams submixInitParams;
		submixInitParams.EffectRoute = EFFECT_ROUTE_POSITIONED;

		Assign(submixInitParams.SourceEffectSubmixId, audDriver::GetMixer()->GetSubmixIndex(g_AudioEngine.GetEnvironment().GetLocalVoiceSubmix()));
		CreateAndPlaySound_Persistent(m_SoundSet.Find(ATSTRINGHASH("Local_Player_Submix", 0xC25DE601)), &m_LocalPlayerSubmixSound, &submixInitParams);
	}
}

void audVoiceChatAudioEntity::Shutdown()
{
	fwAudioEntity::Shutdown();

	m_Provider.Shutdown();
}

void audVoiceChatAudioEntity::UpdateLocalPlayer(const CPed *ped, const bool hasLoudhailer)
{
	Vector3 localPlayerPos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
	if(m_LocalPlayerSubmixSound)
	{
		audRequestedSettings *reqSets = m_LocalPlayerSubmixSound->GetRequestedSettings();

		reqSets->SetPosition(localPlayerPos);
		if(ped->GetAudioEnvironmentGroup())
		{
			reqSets->GetEnvironmentGameMetric() = ped->GetAudioEnvironmentGroup()->GetEnvironmentGameMetric();
		}
		reqSets->SetEnvironmentalLoudness(255U);
		reqSets->GetEnvironmentGameMetric().SetReverbMedium(0.3f);
	}

	if(m_LocalPlayerSound)
	{
		m_LocalPlayerSound->SetRequestedVolume(hasLoudhailer ? 0.0f : -100.0f);
	}

	if(hasLoudhailer && !m_LocalPlayerHasLoudhailer)
	{
		audSoundInitParams initParams;	
		initParams.EffectRoute = EFFECT_ROUTE_LOCAL_VOICE;
		initParams.Pan = 0;
		CreateAndPlaySound(m_SoundSet.Find(ATSTRINGHASH("StartLoudhailer", 0x1ABD87B0)), &initParams);
	}
	else if(!hasLoudhailer && m_LocalPlayerHasLoudhailer)
	{
		audSoundInitParams initParams;
		initParams.EffectRoute = EFFECT_ROUTE_LOCAL_VOICE;
		initParams.Pan = 0;
		CreateAndPlaySound(m_SoundSet.Find(ATSTRINGHASH("StopLoudhailer", 0x69527609)), &initParams);
	}
	m_LocalPlayerHasLoudhailer = hasLoudhailer;
}

void audVoiceChatAudioEntity::AddTalker(const rlGamerId &gamerId, const CPed *ped)
{
	if(PARAM_noloudhailereffect.Get())
	{
		return;
	}

    for(s32 i = 0; i < m_RemoteTalkers.GetMaxCount(); i++)
    {
        if(m_RemoteTalkers[i].state != TalkerInfo::Idle && m_RemoteTalkers[i].gamerId == gamerId)
        {
            // we already have this guy
            return;
        }
    }

	for(s32 i = 0; i < m_RemoteTalkers.GetMaxCount(); i++)
	{
		if(m_RemoteTalkers[i].state == TalkerInfo::Idle)
		{
			m_RemoteTalkers[i].ped = ped;
			m_RemoteTalkers[i].gamerId = gamerId;
			audSoundInitParams initParams;
			Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
			CreateSound_PersistentReference(m_SoundSet.Find(ATSTRINGHASH("Remote_Player_Voice", 0x2D6C6E81)), &m_RemoteTalkers[i].sound, &initParams);
			m_RemoteTalkers[i].state = TalkerInfo::Allocated;

			return;
		}
	}
}

void audVoiceChatAudioEntity::RemoveTalker(const rlGamerId &gamerId)
{
	for(s32 i = 0; i < m_RemoteTalkers.GetMaxCount(); i++)
	{
		if(m_RemoteTalkers[i].state != TalkerInfo::Idle && m_RemoteTalkers[i].gamerId == gamerId)
		{
			m_RemoteTalkers[i].ped = NULL;
			if(m_RemoteTalkers[i].sound)
			{
				m_RemoteTalkers[i].sound->StopAndForget();
			}
			m_RemoteTalkers[i].state = TalkerInfo::Idle;
			return;
		}
	}
}

void audVoiceChatAudioEntity::RemoveAllTalkers()
{
    for(s32 i = 0; i < m_RemoteTalkers.GetMaxCount(); i++)
    {
        if(m_RemoteTalkers[i].state != TalkerInfo::Idle)
        {
            m_RemoteTalkers[i].ped = NULL;
            if(m_RemoteTalkers[i].sound)
            {
                m_RemoteTalkers[i].sound->StopAndForget();
            }
            m_RemoteTalkers[i].state = TalkerInfo::Idle;
        }
    }
}

void audVoiceChatAudioEntity::UpdateRemoteTalkers()
{
	if(PARAM_noloudhailereffect.Get())
	{
		return;
	}

	for(s32 i = 0; i < m_RemoteTalkers.GetMaxCount(); i++)
	{
		TalkerInfo &talker = m_RemoteTalkers[i];
		switch(talker.state)
		{
		case TalkerInfo::Idle:
			break;
		case TalkerInfo::Allocated:
			if(talker.sound == NULL)
			{
				talker.ped = NULL;
				talker.state = TalkerInfo::Idle;
				break;
			}
			else if(talker.sound->GetSoundTypeID() != ExternalStreamSound::TYPE_ID)
			{
				talker.ped = NULL;
				talker.sound->StopAndForget();
				talker.state = TalkerInfo::Idle;
				break;
			}
			else
			{		
				audReferencedRingBuffer *ringBuffer = m_Provider.FindRemotePlayerRingBuffer(m_RemoteTalkers[i].gamerId);
				if(ringBuffer)
				{
					audExternalStreamSound *externalStreamSound = reinterpret_cast<audExternalStreamSound*>(talker.sound);
					externalStreamSound->InitStreamPlayer(ringBuffer, 1, 16000U);
					externalStreamSound->SetRequestedVolume(-100.f);
					externalStreamSound->SetGain(9.f);
					externalStreamSound->PrepareAndPlay();
					
					talker.state = TalkerInfo::Initialised;
					// intentional fall-through
				}
				else
				{
					// Wait until we find an associated ring buffer
					break;
				}
			}
		case TalkerInfo::Initialised:
			if(talker.sound == NULL)
			{
				talker.ped = NULL;
				talker.state = TalkerInfo::Idle;
			}
			else if(talker.ped == NULL)
			{
				talker.sound->StopAndForget();
				talker.state = TalkerInfo::Idle;
			}
			else
			{
				audRequestedSettings *reqSets = talker.sound->GetRequestedSettings();
				reqSets->SetPosition(VEC3V_TO_VECTOR3(talker.ped->GetTransform().GetPosition()));
				if(talker.ped->GetAudioEnvironmentGroup())
				{
					// Inherit ped environment
					reqSets->GetEnvironmentGameMetric() = talker.ped->GetAudioEnvironmentGroup()->GetEnvironmentGameMetric();
				}
					// override with our 'loudhailer' settings
				reqSets->GetEnvironmentGameMetric().SetReverbMedium(0.5f);
				reqSets->SetEnvironmentalLoudness(255U);

				reqSets->SetLowPassFilterCutoff(3000U);
				reqSets->SetHighPassFilterCutoff(500U);

				reqSets->SetPostSubmixVolumeAttenuation(9.f);
				bool bRemotePlayerHasLoudhailer = talker.ped->GetWeaponManager()->GetEquippedWeapon() && 
					talker.ped->GetWeaponManager()->GetEquippedWeapon()->GetWeaponHash() == WEAPONTYPE_LOUDHAILER && 
					talker.ped->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun);

				reqSets->SetVolume(bRemotePlayerHasLoudhailer ? 0.f : -100.f);

				if(bRemotePlayerHasLoudhailer && !talker.hadLoudhailer)
				{
					audSoundInitParams initParams;
					initParams.Tracker = talker.ped->GetPlaceableTracker();					
					CreateAndPlaySound(m_SoundSet.Find(ATSTRINGHASH("StartLoudhailer", 0x1ABD87B0)), &initParams);
				}

				if(!bRemotePlayerHasLoudhailer && talker.hadLoudhailer)
				{
					audSoundInitParams initParams;
					initParams.Tracker = talker.ped->GetPlaceableTracker();					
					CreateAndPlaySound(m_SoundSet.Find(ATSTRINGHASH("StopLoudhailer", 0x69527609)), &initParams);				
				}

				talker.hadLoudhailer = bRemotePlayerHasLoudhailer;
			}
			break;
		}
	}
}

