#ifndef AUD_VOICECHATAUDIOENTITY_H
#define AUD_VOICECHATAUDIOENTITY_H

#include "audioengine/soundset.h"
#include "audiohardware/ringbuffer.h"

#include "fwaudio/audioentity.h"
#include "atl/map.h"
#include "avchat/voicechat.h"
#include "rline/rlgamerinfo.h"
#include "scene/regdreftypes.h"

class CPed;
namespace rage
{
	class audSound;
}

class audVoiceChatAudioProvider : VoiceChatAudioProvider
{
public:

	audVoiceChatAudioProvider() : m_LocalPlayerRingbuffer(NULL)
	{

	}

	void Init();
	void Shutdown();

	virtual void LocalPlayerAudioReceived(const s16 *data, const u32 lengthSamples);
	virtual void NotifyRemotePlayerAdded(const rlGamerId &gamerId);
	virtual void NotifyRemotePlayerRemoved(const rlGamerId &gamerId);
	virtual void RemotePlayerAudioReceived(const s16 *data, const u32 lengthSamples, const rlGamerId &gamerId);
	virtual void RemoveAllRemotePlayers();

	audReferencedRingBuffer *GetLocalPlayerRingBuffer() { return m_LocalPlayerRingbuffer; }
	audReferencedRingBuffer *FindRemotePlayerRingBuffer(const rlGamerId &gamerId);
private:

	struct RemoteTalkerInfo
	{
		rlGamerId gamerId;
		audReferencedRingBuffer *ringBuffer;
	};
	atArray<RemoteTalkerInfo> m_RemoteTalkers;
	audReferencedRingBuffer *m_LocalPlayerRingbuffer;
};

class audVoiceChatAudioEntity : public fwAudioEntity
{
public:

	audVoiceChatAudioEntity();

	void Init();
	void Shutdown();
	void UpdateLocalPlayer(const CPed *ped, const bool hasLoudhailer);

	void AddTalker(const rlGamerId &gamerId, const CPed *ped);
	void RemoveTalker(const rlGamerId &gamerId);
    void RemoveAllTalkers();
    void UpdateRemoteTalkers();

private:

	audVoiceChatAudioProvider m_Provider;

	struct TalkerInfo
	{
		enum State {
			Idle = 0,
			Allocated,
			Initialised,
		};	
		TalkerInfo() : ped(NULL), sound(NULL), state(Idle), hadLoudhailer(false), isTagged(false) {}
		RegdConstPed ped;
		audSound *sound;
		rlGamerId gamerId;
		State state;	
		bool hadLoudhailer;
        bool isTagged;
	};

	virtual void ProcessAnimEvents() {};
	virtual void HandleLoopingAnimEvent(const audAnimEvent & ) {};
	

	enum {MaxTalkers = 18};
	// atRangeArray rather than atArray since we can't move persistent audSound ptrs around
	atRangeArray<TalkerInfo, MaxTalkers> m_RemoteTalkers;

	audSound *m_LocalPlayerSound;
	audSound *m_LocalPlayerSubmixSound;
	
	audSoundSet m_SoundSet;
	bool m_LocalPlayerHasLoudhailer;

};



#endif // AUD_VOICECHATAUDIOENTITY_H
