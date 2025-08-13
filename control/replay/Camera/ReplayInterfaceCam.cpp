#include "ReplayInterfaceCam.h"

#if GTA_REPLAY

#include "../ReplayInterfaceImpl.h"
#include "Camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "Control/Replay/ReplayInternal.h"
#include "Control/Replay/ReplayController.h"
#include "Streaming/streaming.h"
#include "scene/world/GameWorld.h"

bool* CReplayInterfaceCamera::sm_pUseRecordedCamera = NULL;

const char*	CReplayInterfaceTraits<camBaseCamera>::strShort = "CAM";
const char*	CReplayInterfaceTraits<camBaseCamera>::strLong = "CAMERA";
const char*	CReplayInterfaceTraits<camBaseCamera>::strShortFriendly = "Cam";
const char*	CReplayInterfaceTraits<camBaseCamera>::strLongFriendly = "ReplayInterface-Camera";

const int	CReplayInterfaceTraits<camBaseCamera>::maxDeletionSize	= 1;	// Prevent 0 sized array

const u32 CAM_RECORD_BUFFER_SIZE		= 1*1024;
const u32 CAM_CREATE_BUFFER_SIZE		= 0;		// There are no creation packets for cameras
const u32 CAM_FULLCREATE_BUFFER_SIZE	= 0;

//////////////////////////////////////////////////////////////////////////
CReplayInterfaceCamera::CReplayInterfaceCamera(replayCamFrame& cameraFrame, CReplayInterfaceManager& interfaceManager, bool& useRecordedCam)
	: CReplayInterface<camBaseCamera>(*camBaseCamera::GetPool(), CAM_RECORD_BUFFER_SIZE, CAM_CREATE_BUFFER_SIZE, CAM_FULLCREATE_BUFFER_SIZE)
	, m_cameraFrame(cameraFrame)
	, m_interfaceManager(interfaceManager)
	, m_cachedRecordFlags(0)
{
	m_relevantPacketMin = PACKETID_CAMERAUPDATE;
	m_relevantPacketMax	= PACKETID_CAMERAUPDATE;

	m_createPacketID	= PACKETID_INVALID;
	m_updatePacketID	= PACKETID_CAMERAUPDATE;
	m_deletePacketID	= PACKETID_INVALID;
	m_packetGroup		= REPLAY_PACKET_GROUP_NONE;

	m_enableSizing		= true;
	m_enableRecording	= true;
	m_enablePlayback	= true;
	m_enabled			= true;

	m_updateOnPreprocess = true;

	sm_pUseRecordedCamera = &useRecordedCam;

	AddPacketDescriptor<CPacketCameraUpdate>(PACKETID_CAMERAUPDATE, STRINGIFY(CPacketCameraUpdate));
}


//////////////////////////////////////////////////////////////////////////
//
void CReplayInterfaceCamera::RecordFrame(u16 sessionBlockIndex)
{
	(void)sessionBlockIndex;
	CReplayRecorder recorder(m_recordBuffer, m_recordBufferSize);
	CReplayRecorder createPacketRecorder(m_pCreatePacketsBuffer, m_createPacketsBufferSize);

	recorder.RecordPacketWithParam<CPacketCameraUpdate>(m_cachedRecordFlags);
	m_cachedRecordFlags = 0;

	m_recordBufferUsed = recorder.GetMemUsed();
	m_createPacketsBufferUsed = createPacketRecorder.GetMemUsed();
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceCamera::PostRecordFrame(ReplayController& controller, u16 previousBlockFrameCount, bool blockChange)
{
	CReplayInterface<camBaseCamera>::PostRecordFrame(controller, previousBlockFrameCount, blockChange);
}


//////////////////////////////////////////////////////////////////////////
ePlayPktResult CReplayInterfaceCamera::PreprocessPackets(ReplayController& controller, bool /*entityMayNotExist*/)
{
	do 
	{
		eReplayPacketId packetID = controller.GetCurrentPacketID();

		if(packetID == PACKETID_CAMERAUPDATE)
		{
			CPacketCameraUpdate const* pPacket = controller.ReadCurrentPacket<CPacketCameraUpdate>();

			m_interper.Init(controller, pPacket);
			InterpWrapper<CCamInterp> interper(m_interper, controller);

			pPacket->Extract(controller, m_cameraFrame, m_interfaceManager, m_cachedFlags, m_interper.GetNextPacket(), controller.GetInterp(), controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK));
		}
		else
		{
			controller.ReadCurrentPacket<CPacketBase>();
			if(!IsRelevantPacket(controller.GetCurrentPacketID()))
				return PACKET_UNHANDLED;
			controller.AdvancePacket();
		}

	} while (!controller.IsNextFrame());

	return PACKET_UNHANDLED;
}

//////////////////////////////////////////////////////////////////////////
ePlayPktResult CReplayInterfaceCamera::PlayPackets(ReplayController& controller, bool)
{
	do 
	{
		eReplayPacketId packetID = controller.GetCurrentPacketID();
		if(packetID == PACKETID_CAMERAUPDATE)
		{
			CPacketCameraUpdate const* pPacket = controller.ReadCurrentPacket<CPacketCameraUpdate>();

			m_interper.Init(controller, pPacket);
			InterpWrapper<CCamInterp> interper(m_interper, controller);

			pPacket->Extract(controller, m_cameraFrame, m_interfaceManager, m_cachedFlags, m_interper.GetNextPacket(), controller.GetInterp(), controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK));
		}
		else
		{
			controller.ReadCurrentPacket<CPacketBase>();
			if(!IsRelevantPacket(controller.GetCurrentPacketID()))
				return PACKET_UNHANDLED;

			controller.AdvancePacket();
		}
		
	} while (!controller.IsNextFrame());

	return PACKET_UNHANDLED;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceCamera::JumpPackets(ReplayController& controller)
{
	do 
	{
		eReplayPacketId packetID = controller.GetCurrentPacketID();
		if(packetID == PACKETID_CAMERAUPDATE)
		{
			CPacketCameraUpdate const* pPacket = controller.ReadCurrentPacket<CPacketCameraUpdate>();

			m_interper.Init(controller, pPacket);
			InterpWrapper<CCamInterp> interper(m_interper, controller);

			pPacket->Extract(controller, m_cameraFrame, m_interfaceManager, m_cachedFlags, m_interper.GetNextPacket(), controller.GetInterp(), controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK));

		}
		else
		{
			controller.ReadCurrentPacket<CPacketBase>();
			if(!IsRelevantPacket(controller.GetCurrentPacketID()))
				return;
			controller.AdvancePacket();
		}

		
	} while (!controller.IsNextFrame());

	return;
}

void CReplayInterfaceCamera::PostRender()
{
	const camFrame& renderedFrame = camInterface::GetFrame();
	m_cachedRecordFlags = m_cachedRecordFlags | renderedFrame.GetFlags();
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceCamera::PostProcess()
{
	m_cachedFlags.ClearAllFlags();
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceCamera::PreClearWorld()
{
	CViewport* gameViewport = gVpMan.GetGameViewport();
	if(replayVerifyf(gameViewport, "The game viewport does not exist"))
	{
		int count = RENDERPHASEMGR.GetRenderPhaseCount();
		for(s32 i=0; i<count; i++)
		{
			CRenderPhase& renderPhase = (CRenderPhase&)RENDERPHASEMGR.GetRenderPhase(i);

			if (renderPhase.IsScanRenderPhase())
			{
				CRenderPhaseScanned& scanPhase = (CRenderPhaseScanned&)renderPhase;

				if (scanPhase.IsUpdatePortalVisTrackerPhase())
				{
					CPortalTracker* portalTracker = scanPhase.GetPortalVisTracker();
					if(replayVerifyf(portalTracker, "The game viewport does not have a portal tracker"))
					{
						if(portalTracker && portalTracker->GetInteriorInst())
						{
							portalTracker->RemoveFromInterior();
							portalTracker->Teleport();			//Prevent a rescan overwriting newly set room.
						}
					}
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Base class overrides to prevent compilation of the template base functions
// with camBaseCamera which does not support much of the functionality.
template<>
void CReplayInterface<camBaseCamera>::ResetAllEntities()
{}
template<>
void CReplayInterface<camBaseCamera>::StopAllEntitySounds()
{}
template<>
void CReplayInterface<camBaseCamera>::ApplyFades(ReplayController&, s32)
{}
template<>
void CReplayInterface<camBaseCamera>::ApplyFadesSecondPass()
{}
template<>
bool CReplayInterface<camBaseCamera>::TryPreloadPacket(const CPacketBase*, bool&, u32 PRELOADDEBUG_ONLY(, atString&))
{ 	return false; 	}
template<>
void CReplayInterface<camBaseCamera>::PreUpdatePacket(ReplayController&)
{}
template<>
void CReplayInterface<camBaseCamera>::PreUpdatePacket(const CPacketCameraUpdate*, const CPacketCameraUpdate*, float, const CReplayState&, bool)
{}
template<>
void CReplayInterface<camBaseCamera>::PlayUpdatePacket(ReplayController&, bool)
{}
template<>
void CReplayInterface<camBaseCamera>::PlayCreatePacket(ReplayController&)
{}
template<>
void CReplayInterface<camBaseCamera>::PlayDeletePacket(ReplayController&)
{}
template<>
void CReplayInterface<camBaseCamera>::PostCreateElement(CPacketCameraDummy const*, camBaseCamera*, const CReplayID&)
{}
template<>
void CReplayInterface<camBaseCamera>::JumpUpdatePacket(ReplayController&)
{}
template<>
void CReplayInterface<camBaseCamera>::OnDelete(CPhysical*)
{}
template<>
void CReplayInterface<camBaseCamera>::ClearWorldOfEntities()
{}
template<>
bool CReplayInterface<camBaseCamera>::EnsureIsBeingRecorded(const CEntity*)
{	return false;	}
template<>
void CReplayInterface<camBaseCamera>::RegisterForRecording(CPhysical*, FrameRef)
{}
template<>
void CReplayInterface<camBaseCamera>::RegisterAllCurrentEntities()
{}
template<>
void CReplayInterface<camBaseCamera>::DeregisterAllCurrentEntities()
{
	m_LatestUpdatePackets.Reset();
}
template<>
bool CReplayInterface<camBaseCamera>::EnsureWillBeRecorded(const CEntity*)
{	return false;	}
template<>
bool CReplayInterface<camBaseCamera>::SetCreationUrgent(const CReplayID&)
{	return false;	}
template<>
template<typename UPDATEPACKET, typename SUBTYPE>
bool CReplayInterface<camBaseCamera>::RecordElementInternal(camBaseCamera*, CReplayRecorder&, CReplayRecorder&, CReplayRecorder&, u16 )
{	return true;	}
template<>
void CReplayInterface<camBaseCamera>::LinkCreatePacketsForPlayback(ReplayController&, u32)
{}

template<>
void CReplayInterface<camBaseCamera>::PostStorePacket(CPacketCameraUpdate*, const FrameRef&, const u16)
{}

template<>
bool CReplayInterface<camBaseCamera>::RemoveElement(camBaseCamera*, const CPacketCameraDummy*, bool)
{	return true;	}

template<>
void CReplayInterface<camBaseCamera>::FindEntity(CEntityGet<CEntity>&)
{}

template<>
void CReplayInterface<camBaseCamera>::SetEntity(const CReplayID&, camBaseCamera*, bool)
{}

template<>
void CReplayInterface<camBaseCamera>::SetCurrentlyBeingRecorded(int, camBaseCamera*)
{}

template<>
void CReplayInterface<camBaseCamera>::PrintOutEntities()
{}

template<>
void CReplayInterface<camBaseCamera>::ResetEntityCreationFrames()
{
	ClearDeletionInformation();
}

template<>
void CReplayInterface<camBaseCamera>::ProcessRewindDeletions(FrameRef)
{}

template<>
u32	CReplayInterface<camBaseCamera>::GetHash(camBaseCamera*, u32)
{	return 0;	}

template<>
void CReplayInterface<camBaseCamera>::ClrEntity(const CReplayID&, const camBaseCamera*)
{}

#if __BANK
template<>
bool CReplayInterface<camBaseCamera>::ScanForAnomalies(char*, int&) const
{	return false;	}

//////////////////////////////////////////////////////////////////////////
bkGroup* CReplayInterfaceCamera::AddDebugWidgets()
{
	bkGroup* pGroup = iReplayInterface::AddDebugWidgets("Camera");
	if(pGroup == NULL)
		return NULL;

	pGroup->AddButton("Toggle Use Recorded Cam", ToggleUseRecordedCam);

	return pGroup;
}
#endif // __BANK

#endif // GTA_REPLAY
