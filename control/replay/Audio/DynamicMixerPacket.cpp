#include "DynamicMixerPacket.h"

#if GTA_REPLAY
 
#include "control/replay/ReplayInternal.h"
#include "audio/dynamicmixer.h"
#include "audio/northaudioengine.h"

tPacketVersion g_PacketDynamicMixerScene_v1 = 1;
//////////////////////////////////////////////////////////////////////////
CPacketDynamicMixerScene::CPacketDynamicMixerScene(const u32 hash, bool stop, u32 startOffset)
: CPacketEventTracked(PACKETID_DYNAMICMIXER_SCENE, sizeof(CPacketDynamicMixerScene), g_PacketDynamicMixerScene_v1)
{
	m_Hash = hash;
	m_stop = stop;
	m_StartOffset = startOffset;
}

//////////////////////////////////////////////////////////////////////////
bool CPacketDynamicMixerScene::Extract(CTrackedEventInfo<tTrackedSceneType>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK || m_stop)
	{
		if(eventInfo.m_pEffect[0].m_pScene)
		{
			eventInfo.m_pEffect[0].m_pScene->Stop();
		}
		return true;
	}

	if(eventInfo.m_pEffect[0].m_pScene == NULL)
	{
		if( GetPacketVersion() >= g_PacketDynamicMixerScene_v1 )
		{
			DYNAMICMIXER.StartScene(m_Hash, &eventInfo.m_pEffect[0].m_pScene, NULL, m_StartOffset);
		}
		else
		{
			DYNAMICMIXER.StartScene(m_Hash, &eventInfo.m_pEffect[0].m_pScene, NULL, 0);
		}

		if(eventInfo.m_pEffect[0].m_pScene)
		{
			eventInfo.m_pEffect[0].m_pScene->SetIsReplay(true);
		}
	}

	return true;
}

bool CPacketDynamicMixerScene::HasExpired(const CTrackedEventInfo<tTrackedSceneType>&) const
{ 
	return DYNAMICMIXER.GetActiveSceneFromHash(m_Hash) == NULL;
}


//---------------------------------------------------------
// Func: CPacketSceneSetVariable::Store
// Desc: Stores a sound variable update
//---------------------------------------------------------
CPacketSceneSetVariable::CPacketSceneSetVariable(u32 nameHash, f32 value)
	: CPacketEventTracked(PACKETID_SET_AUDIO_SCENE_VARIABLE, sizeof(CPacketSceneSetVariable))
	, CPacketEntityInfo()
	, m_NameHash(nameHash)
	,m_Value(value)
{}

//---------------------------------------------------------
// Func: CPacketSceneSetVariable::Extract
// Desc: Updates the position for a replay sound
//---------------------------------------------------------
void CPacketSceneSetVariable::Extract(CTrackedEventInfo<tTrackedSceneType>& pData) const
{
	audScene*& pScene = pData.m_pEffect[0].m_pScene;

	if( !pScene )
	{
		replayAssert(false && "No scene to set variable");
		return;
	}

	u32 nameHash = m_NameHash;
	f32 value = m_Value;
	pScene->SetVariableValue(nameHash, value);
}




#endif // GTA_REPLAY