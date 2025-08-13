#ifndef __REPLAY_RAYFIRE_MANAGER_H__
#define __REPLAY_RAYFIRE_MANAGER_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayRayFirePacket.h"
#include "scene/entities/compEntity.h"

class PacketRayFireReplayHandlingData
{
public:

	bool operator == (const PacketRayFireInfo& otherInfo) const
	{
		return (m_Info.m_ModelHash == otherInfo.m_ModelHash && !(m_Info.m_ModelPos != otherInfo.m_ModelPos));
	}

	PacketRayFireInfo	m_Info;
};

class RayFireReplayLoadingData
{
public:
	RayFireReplayLoadingData()
	{
		m_ModelInfoHash = 0;
	}

	void	Load(const PacketRayFirePreLoadData &loadInfo);
	bool	HasLoaded();
	void	Release(u32 uID);
	bool	ContainsDataFor(const PacketRayFirePreLoadData &info);
	void	ReleaseAll();

	u32						m_ModelInfoHash;	// Rayfire Model Hash
	atFixedArray<u32, 8>	m_RefsByUID;		// Also used to say if this is active or not.

	bool					m_bUseImapForStartAndEnd;

	// imap group swapper
	u32						m_imapSwapIndex;
	u32						m_imapSwapEndIndex;		// m_imapSwapIndex for start, m_imapSwapEndIndex for end

	strRequest				m_startRequest;
	strRequest				m_endRequest;
	strRequestArray<20>		m_animRequests;
	strRequest				m_ptfxRequest;
};

class ReplayRayFireManager
{
public:

#define MAX_RAYFIRES_PRELOAD_COUNT			(10)
#define MAX_RAYFIRES_PRELOAD_REQUEST_COUNT	(20)

	static void OnEntry();						// Called at start of playback, Saves current state
	static void OnExit();						// Called at end of playback, Re-sets state before replay
	static void	Process();						// Called each frame during playback
	static void SubmitPacket(const PacketRayFireInfo &packet);

	// Pre-load
	static int Load(const PacketRayFirePreLoadData &info);
	static bool HasLoaded( int loadIndex );
	static void	Release(int loadIndex, u32 uID);
	static int FindLoadIndex(const PacketRayFirePreLoadData &info);
	static RayFireReplayLoadingData *GetLoadingData(int loadIndex)
	{
		if( loadIndex >= 0 && loadIndex < MAX_RAYFIRES_PRELOAD_COUNT )
		{
			return &m_LoadingData[loadIndex];
		}
		return NULL;
	}

	static void	AddDeferredLoad(const PacketRayFirePreLoadData &info);
	static void	ProcessDeferred();

private:

	static void ResetPacketData();
	static void ResetPreLoadData();

	static CCompEntity	*FindCompEntity(PacketRayFireInfo &packet);

	static atArray<PacketRayFireInfo>				m_PacketData;
	static atArray<PacketRayFireReplayHandlingData>	m_RayFireData;
	static RayFireReplayLoadingData					m_LoadingData[MAX_RAYFIRES_PRELOAD_COUNT];

	static bool										m_DefferedPreLoadReset;
	static atFixedArray<PacketRayFirePreLoadData, MAX_RAYFIRES_PRELOAD_REQUEST_COUNT>		m_DeferredLoads;
};

#endif	//GTA_REPLAY

#endif // !__REPLAY_RAYFIRE_MANAGER_H__
