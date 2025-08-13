#ifndef __REPLAY_INTERIOR_MANAGER_H__
#define __REPLAY_INTERIOR_MANAGER_H__

#include "control/replay/ReplaySettings.h"
#include "control/replay/ReplayEnums.h"

#if GTA_REPLAY

#include "ReplayController.h"
#include "ReplayRecorder.h"
#include "Misc\InteriorPacket.h"

class CPacketInteriorEntitySet;

class ReplayInteriorManager
{
public:
private:
	struct InteriorProxyRequest
	{
		InteriorProxyRequest()
			: m_lastPreloadTime(0)
			, m_ProxyNameHash(0xffffffff)
			, m_positionHash(0)
			, m_pInteriorProxy(NULL)
			, m_stateRequest(0)
		{}
		InteriorProxyRequest(u32 proxyNameHash, u32 posHash, u32 stateRequest)
			: m_lastPreloadTime(0)
			, m_ProxyNameHash(proxyNameHash)
			, m_positionHash(posHash)
			, m_pInteriorProxy(NULL)
			, m_stateRequest(stateRequest)
		{}

		class CInteriorInst *GetInterior();

		void	Release();
		bool	HasLoaded();
		bool	IsFree() const
		{
			return m_lastPreloadTime == 0 && m_ProxyNameHash == 0xffffffff;
		}

		u32	m_ProxyNameHash;
		u32 m_positionHash;
		u32	m_lastPreloadTime;
		u32 m_stateRequest;
		class CInteriorProxy *m_pInteriorProxy;
		atArray < u32 >	m_RoomRequestTimes;

		bool operator==(const InteriorProxyRequest& other) const
		{
			return (m_ProxyNameHash == other.m_ProxyNameHash) && (m_positionHash == other.m_positionHash);
		}
	};

public:
	void Init(u32 noOfInteriorProxyRequests, u32 noOfRoomRequests);
public:
	static void	OnRecordStart();			
	static void RecordData(CReplayRecorder& fullCreatePacketRecorder, CReplayRecorder& createPacketRecorder);
	static void CollectData();
	static void ResetEntitySetLastState(bool copyLastBlockData);

	static void OnEntry();
	static void OnExit();
	static void SubmitNewInteriorData(u8 count, const ReplayInteriorData *pData);
	static void ConsolidateInteriorData(atArray<ReplayInteriorData>& interiorArray, bool onlyForceDisabledCapped);
	static void SetInteriorData(atArray<ReplayInteriorData>& interiorArray);
	static CInteriorProxy *GetProxyAtCoords(const Vector3& vecPositionToCheck, u32 typeKey, bool onlyActive = true);

	static void AddInteriorStatePacket(const CPacketInterior* pPacket) { m_interiorStates.PushAndGrow(pPacket);}
	static void ProcessInteriorStatePackets();
private:
	static void BackupEntitySetData(atArray<atArray<SEntitySet>>& set);
	static void RestoreEntitySetData(atArray<atArray<SEntitySet>>& set);
	static void PrecalcEntitySets(CReplayRecorder& fullCreatePacketRecorder, CReplayRecorder& createPacketRecorder, atArray<atArray<SEntitySet>>& sourceData);

public:
	// Interior Proxy and room related.
	bool AddInteriorProxyPreloadRequest(u32 interiorProxyName, u32 posHash, CInteriorProxy::eProxy_State stateRequest, u32 currGameTime);
	bool AddRoomRequest(u32 interiorProxyName, u32 posHash, u32 roomIndex, u32 currGameTime);
	void UpdateInteriorProxyRequests(u32 time);
	void FlushPreloadRequests();
private:
	InteriorProxyRequest *FindOrAddPreloadRequest(InteriorProxyRequest &inReq, bool& preloadSuccess, u32 currGameTime);

public:
	void ResetRoomRequests();
	const atArray <CRoomRequest> &GetRoomRequests();
	void RecordRoomRequest(u32 proxyNameHash, u32 posHash, u32 roomIndex);

	static void DoHacks(bool isRecordedMP);

private:
	static void RequestInteriorProxy(class CInteriorProxy *pIntProxy, CInteriorProxy::eProxy_State stateRequest);
	static void ReleaseInteriorProxy(class CInteriorProxy *pIntProxy);
	static bool HasInteriorProxyLoaded(class CInteriorProxy *pIntProxy, CInteriorProxy::eProxy_State stateRequest);

private:
	// Data.
	static atArray<ReplayInteriorData>		ms_InteriorState;
	static atArray<ReplayInteriorData>		ms_interiorStateBackup;
	static CPacketInterior					ms_interiorPacket;
	static bool								ms_mismatch;
	static bool								ms_extractedPacket;

	// Interior Entity Set Stuff
	static atArray<atArray<SEntitySet>>		ms_EntitySetLastState;

	// On going requests (playback).
	atArray <InteriorProxyRequest>			m_InteriorProxyRequests;
	// Per frame buffer of room requests (recording).
	atArray <CRoomRequest>					m_RoomRequests;

	static atArray<const CPacketInterior*>	m_interiorStates;

	static bool								m_dt1_03_carparkHack;
	static bool								m_dt1_03_carparkHackOnlyOnce;
};

#endif	//GTA_REPLAY

#endif	//__REPLAY_IPL_MANAGER_H__
