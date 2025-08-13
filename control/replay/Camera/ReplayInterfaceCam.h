#ifndef REPLAYINTERFACECAM_H
#define REPLAYINTERFACECAM_H

#include "Control/Replay/ReplaySettings.h"

#if GTA_REPLAY

#include "Control/Replay/Misc/CameraPacket.h"
#include "Control/Replay/ReplayInterface.h"

#include "camera/base/BaseCamera.h"

class CReplayInterfaceManager;

#define CAMPACKETDESCRIPTOR(id, et, type, recFunc) CPacketDescriptor<camBaseCamera>(id, et, sizeof(type), #type, \
	&iReplayInterface::template PrintOutPacketDetails<type>,\
	recFunc,\
	NULL)


template<>
class CReplayInterfaceTraits<camBaseCamera>
{
public:
	typedef CPacketCameraDummy	tCreatePacket;
	typedef CPacketCameraUpdate	tUpdatePacket;
	typedef CPacketCameraDummy	tDeletePacket;
	typedef CCamInterp			tInterper;
	typedef camBaseCamera::Pool		tPool;

	typedef deletionData<1>	tDeletionData;

	static const char*	strShort;
	static const char*	strLong;
	static const char*	strShortFriendly;
	static const char*	strLongFriendly;

	static const int	maxDeletionSize;
};



class CReplayInterfaceCamera
	: public CReplayInterface<camBaseCamera>
{
public:
	CReplayInterfaceCamera(replayCamFrame& cameraFrame, CReplayInterfaceManager& interfaceManager, bool& useRecordedCam);
	~CReplayInterfaceCamera(){}

	void		RecordFrame(u16 sessionBlockIndex);
	void		PostRecordFrame(ReplayController& controller, u16 previousBlockFrameCount, bool blockChange);

	void		PostProcess();
	void		PostRender();

	void		PreClearWorld();

	bkGroup* 	AddDebugWidgets();
	bool		MatchesType(const atHashWithStringNotFinal& type) const		{	return type == atHashWithStringNotFinal("Camera",0x25584DB6);}

	bool		IsRelevantPacket(eReplayPacketId packetId) const			{	return packetId == PACKETID_CAMERAUPDATE || packetId == PACKETID_ALIGN;		}

	bool		MatchesType(const CEntity*) const								{	return false;	}

	bool		EnsureIsBeingRecording(CEntity*)							{	replayFatalAssertf(false, "");	return false; }

	void		ClearElementsFromWorld()	{}	

private:

	ePlayPktResult PreprocessPackets(ReplayController& controller, bool entityMayNotExist);
	ePlayPktResult PlayPackets(ReplayController&, bool);

	void			JumpPackets(ReplayController& controller);

	static void	ToggleUseRecordedCam()	{	*sm_pUseRecordedCamera = !*sm_pUseRecordedCamera;	}

	replayCamFrame&	m_cameraFrame;

	CReplayInterfaceManager& m_interfaceManager;

	fwFlags16	m_cachedFlags;
	fwFlags16	m_cachedRecordFlags;

	static bool* sm_pUseRecordedCamera;

	CCamInterp	m_interper;	
};

#define INTERPER_CAM InterpWrapper<CReplayInterfaceTraits<camBaseCamera>::tInterper, CReplayInterfaceTraits<camBaseCamera>::tUpdatePacket>

template<>
void CReplayInterface<camBaseCamera>::ResetAllEntities();

template<>
void CReplayInterface<camBaseCamera>::StopAllEntitySounds();

template<>
void CReplayInterface<camBaseCamera>::ApplyFades(ReplayController& controller, s32);

template<>
void CReplayInterface<camBaseCamera>::ApplyFadesSecondPass();

template<>
bool CReplayInterface<camBaseCamera>::TryPreloadPacket(const CPacketBase*, bool&, u32 PRELOADDEBUG_ONLY(, atString&));

template<>
void CReplayInterface<camBaseCamera>::PreUpdatePacket(ReplayController&);

template<>
void CReplayInterface<camBaseCamera>::PreUpdatePacket(const CPacketCameraUpdate*, const CPacketCameraUpdate*, float, const CReplayState&, bool);

template<>
void CReplayInterface<camBaseCamera>::PlayUpdatePacket(ReplayController&, bool);

template<>
void CReplayInterface<camBaseCamera>::PlayCreatePacket(ReplayController&);

template<>
void CReplayInterface<camBaseCamera>::PlayDeletePacket(ReplayController&);

template<>
void CReplayInterface<camBaseCamera>::PostCreateElement(CPacketCameraDummy const*, camBaseCamera*, const CReplayID&);

template<>
void CReplayInterface<camBaseCamera>::JumpUpdatePacket(ReplayController&);

template<>
void CReplayInterface<camBaseCamera>::OnDelete(CPhysical*);

template<>
void CReplayInterface<camBaseCamera>::ClearWorldOfEntities();

template<>
bool CReplayInterface<camBaseCamera>::EnsureIsBeingRecorded(const CEntity*);

template<>
void CReplayInterface<camBaseCamera>::RegisterForRecording(CPhysical*, FrameRef);

template<>
void CReplayInterface<camBaseCamera>::RegisterAllCurrentEntities();

template<>
void CReplayInterface<camBaseCamera>::DeregisterAllCurrentEntities();

template<>
bool CReplayInterface<camBaseCamera>::EnsureWillBeRecorded(const CEntity*);

template<>
bool CReplayInterface<camBaseCamera>::SetCreationUrgent(const CReplayID&);

template<>
template<typename UPDATEPACKET, typename SUBTYPE>
bool CReplayInterface<camBaseCamera>::RecordElementInternal(camBaseCamera*, CReplayRecorder&, CReplayRecorder&, CReplayRecorder&, u16 );

template<>
void CReplayInterface<camBaseCamera>::LinkCreatePacketsForPlayback(ReplayController&, u32);

template<>
void CReplayInterface<camBaseCamera>::PostStorePacket(CPacketCameraUpdate*, const FrameRef&, const u16);

template<>
bool CReplayInterface<camBaseCamera>::RemoveElement(camBaseCamera*, const CPacketCameraDummy*, bool);

template<>
void CReplayInterface<camBaseCamera>::FindEntity(CEntityGet<CEntity>&);

template<>
void CReplayInterface<camBaseCamera>::SetEntity(const CReplayID&, camBaseCamera*, bool);

template<>
void CReplayInterface<camBaseCamera>::SetCurrentlyBeingRecorded(int, camBaseCamera*);

template<>
void CReplayInterface<camBaseCamera>::PrintOutEntities();

template<>
void CReplayInterface<camBaseCamera>::ResetEntityCreationFrames();

template<>
void CReplayInterface<camBaseCamera>::ProcessRewindDeletions(FrameRef);

template<>
u32	CReplayInterface<camBaseCamera>::GetHash(camBaseCamera*, u32);

#if __BANK
template<>
bool CReplayInterface<camBaseCamera>::ScanForAnomalies(char*, int&) const;
#endif // __BANK
#endif // GTA_REPLAY

#endif // REPLAYINTERFACECAM_H
