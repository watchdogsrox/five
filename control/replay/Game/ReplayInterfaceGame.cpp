#include "ReplayInterfaceGame.h"

#if GTA_REPLAY

#include "game/clock.h"
#include "game/weather.h"

#include "../ReplayController.h"
#include "../Misc/ReplayPacket.h"
#include "../Misc/WaterPacket.h"
#include "../ReplayLightingManager.h"
#include "../audio/AmbientAudioPacket.h"
#include "../Misc/ClothPacket.h"

#include "renderer/RenderPhases/RenderPhaseFX.h"

const u32 GAME_RECORD_BUFFER_SIZE		= 256*1024;

const u32 GAME_CREATE_BUFFER_SIZE		= 32*1024;
const u32 GAME_FULLCREATE_BUFFER_SIZE	= 64*1024;

#define GAME_INTERFACE_MAX_INTERIOR_PROXY_PRELOADS	96
#define GAME_INTERFACE_MAX_PER_FRAME_ROOM_REQUESTS	48

#define GAME_INTERFACE_MAX_PER_FRAME_CLOTH_VERTS				500
#define GAME_INTERFACE_MAX_PER_FRAME_CLOTH_PIECES				10

#define GAME_INTERFACE_INVALID_WATER_GRID_POS					0xffff


CReplayInterfaceGame::CReplayInterfaceGame(float& timeScale, bool& overrideTime, bool& overrideWeather)
: iReplayInterface(GAME_RECORD_BUFFER_SIZE, GAME_CREATE_BUFFER_SIZE, GAME_FULLCREATE_BUFFER_SIZE)
, m_numPacketDescriptors(0)
, m_timeScale(timeScale)
, m_overrideTime(overrideTime)
, m_overrideWeather(overrideWeather)
#if REPLAY_ENABLE_WATER
, m_waterFrameTimer(0)
#endif
, m_GameTime(0)
, m_FrameStep(0)
#if REPLAY_ENABLE_WATER
, m_recordWaterFrame(true)
#endif
, m_CubicPatchWaterFrameT0(NULL)
, m_CubicPatchWaterFrameTPlus1(NULL)
, m_interpWaterFrame(0.0f)
, m_PreviousSimPacket(NULL)
{
	AddPacketDescriptor<CPacketGameTime>(					PACKETID_GAMETIME,						STRINGIFY(CPacketGameTime));
	AddPacketDescriptor<CPacketClock>(						PACKETID_CLOCK,							STRINGIFY(CPacketClock));
	AddPacketDescriptor<CPacketWeather>(					PACKETID_WEATHER,						STRINGIFY(CPacketWeather));
	AddPacketDescriptor<CPacketThermalVisionValues>(		PACKETID_THERMALVISIONVALUES,			STRINGIFY(CPacketThermalVisionValues));
	AddPacketDescriptor<CPacketCubicPatchWaterFrame>(		PACKETID_CUBIC_PATCH_WATERFRAME,		STRINGIFY(CPacketWaterFrame));
	AddPacketDescriptor<CPacketWaterSimulate>(				PACKETID_WATERSIMULATE,					STRINGIFY(CPacketWaterSimulate));
	AddPacketDescriptor<CPacketScriptTCModifier>(			PACKETID_TC_SCRIPT_MODIFIER,			STRINGIFY(CPacketScriptTCModifier));
	AddPacketDescriptor<CPacketAlign>(						PACKETID_ALIGN,							STRINGIFY(CPacketAlign));
	AddPacketDescriptor<CPacketMapChange>(					PACKETID_MAP_CHANGE,					STRINGIFY(CPacketMapChange));
	AddPacketDescriptor<CPacketInteriorProxyStates>(		PACKETID_INTERIOR_PROXY_STATE,			STRINGIFY(CPacketInteriorProxyStates));
	AddPacketDescriptor<CPacketCutsceneLight>(				PACKETID_CUTSCENELIGHT,					STRINGIFY(CPacketCutsceneLight));
	AddPacketDescriptor<CPacketPortalOcclusionOverrides>(	PACKETID_PORTAL_OCCLUSION_OVERRIDES,	STRINGIFY(CPacketPortalOcclusionOverrides));
	AddPacketDescriptor<CPacketModifiedAmbientZoneStates>(	PACKETID_MODIFIED_AMBIENT_ZONE_STATES,	STRINGIFY(CPacketModifiedAmbientZoneStates));
	AddPacketDescriptor<CPacketActiveAlarms>(				PACKETID_ACTIVE_ALARMS,					STRINGIFY(CPacketActiveAlarms));
	AddPacketDescriptor<CPacketAudioPedScenarios>(			PACKETID_AUD_PED_SCENARIOS,				STRINGIFY(CPacketAudioPedScenarios));
	AddPacketDescriptor<CPacketInteriorEntitySet>(			PACKETID_INTERIOR_ENTITY_SET,			STRINGIFY(CPacketInteriorEntitySet));
	AddPacketDescriptor<CPacketInteriorEntitySet2>(			PACKETID_INTERIOR_ENTITY_SET2,			STRINGIFY(CPacketInteriorEntitySet2));
	AddPacketDescriptor<CPacketInterior>(					PACKETID_INTERIOR,						STRINGIFY(CPacketInterior));
	AddPacketDescriptor<CPacketDistantCarState>(			PACKETID_DISTANTCARSTATE,				STRINGIFY(CPacketDistantCarState));
	AddPacketDescriptor<CPacketTimeCycleModifier>(			PACKETID_TIMECYCLE_MODIFIER,			STRINGIFY(CPacketTimeCycleModifier));
	AddPacketDescriptor<CPacketClothPieces>(				PACKETID_CLOTH_PIECES,					STRINGIFY(CPacketClothPieces));
	AddPacketDescriptor<CPacketDisabledThisFrame>(			PACKETID_DISABLED_THIS_FRAME,			STRINGIFY(CPacketDisabledThisFrame));
	AddPacketDescriptor<CPacketPerPortalOcclusionOverrides>(PACKETID_PER_PORTAL_OCCLUSION_OVERRIDES,STRINGIFY(CPacketPerPortalOcclusionOverrides));
	
	m_InteriorManager.Init(GAME_INTERFACE_MAX_INTERIOR_PROXY_PRELOADS, GAME_INTERFACE_MAX_PER_FRAME_ROOM_REQUESTS);
	m_ClothPacketManager.Init(GAME_INTERFACE_MAX_PER_FRAME_CLOTH_VERTS, GAME_INTERFACE_MAX_PER_FRAME_CLOTH_PIECES);

	for(u32 i=0; i<NUM_LINKED_PACKET_TYPES; i++)
	{
		m_PacketLinkingInfo[i].m_PreviousCubicPatchSessionBlock = 0;
		m_PacketLinkingInfo[i].m_PreviousCubicPatchPosition = 0;
		m_PacketLinkingInfo[i].m_PreviousCubicPatchWaterFrame = NULL;
		m_PacketLinkingInfo[i].m_LastFoundCubicPatch = NULL;
	}
}


CReplayInterfaceGame::~CReplayInterfaceGame()
{
	m_ClothPacketManager.CleanUp();
	//m_InteriorManager.CleanUp();
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceGame::RecordFrame(u16 sessionBlockIndex)
{
	(void)sessionBlockIndex;
	CReplayRecorder recorder(m_recordBuffer, m_recordBufferSize);
	CReplayRecorder createPacketRecorder(m_pCreatePacketsBuffer, m_createPacketsBufferSize);
	CReplayRecorder fullCreatePacketRecorder(m_pFullCreatePacketsBuffer, m_fullCreatePacketsBufferSize);

	// This time is passed into CReplayMgr::RecordFrame(), which in turn passes it down to here.
	//m_GameTime = fwTimer::GetReplayTimeInMilliseconds();

	m_FrameStep = 0;
	if( m_PrevGameTime != 0 )
	{
		// Work out the frame time as we could be recording at a different framerate
		m_FrameStep = m_GameTime - m_PrevGameTime;
	}
	
	m_PrevGameTime = m_GameTime;

	recorder.RecordPacketWithParam<CPacketGameTime>(m_GameTime);
	recorder.RecordPacket<CPacketClock>();
	recorder.RecordPacket<CPacketWeather>();
	if(RenderPhaseSeeThrough::GetState())
	{
		recorder.RecordPacket<CPacketThermalVisionValues>();
	}

#if REPLAY_ENABLE_WATER
	if(Water::IsHeightSimulationActive())
	{
		recorder.RecordPacketWithParam<CPacketCubicPatchWaterFrame>(m_GameTime);
		recorder.RecordPacket<CPacketWaterSimulate>();

		m_recordWaterFrame = false;
		m_waterFrameTimer += m_FrameStep;
		if(m_waterFrameTimer > REPLAY_WATER_KEYFRAME_FREQ)
		{
			m_waterFrameTimer = 0;
			m_recordWaterFrame = true;
		}
	}
	else
	{
		//make sure when we resume we grab the water frame straight away
		//and without it being connected to the previously recorded packets
		ResetWaterTracking();
	}
#endif

//	recorder.RecordPacket<CPacketScriptTCModifier>();
	recorder.RecordPacket<CPacketMapChange>();
	recorder.RecordPacket<CPacketDisabledThisFrame>();
	recorder.RecordPacket<CPacketTimeCycleModifier>();
	recorder.RecordPacketWithParam<CPacketInteriorProxyStates, const atArray<CRoomRequest> &>(m_InteriorManager.GetRoomRequests());
	fullCreatePacketRecorder.RecordPacketWithParam<CPacketPortalOcclusionOverrides, const atMap<u32, u32>&>(naOcclusionPortalInfo::GetPortalSettingsOverrideMap());	
	fullCreatePacketRecorder.RecordPacketWithParam<CPacketPerPortalOcclusionOverrides, const atMap<u32, naOcclusionPerPortalSettingsOverrideList>&>(naOcclusionPortalInfo::GetPerPortalSettingsOverrideMap());	
	fullCreatePacketRecorder.RecordPacketWithParam<CPacketModifiedAmbientZoneStates, const atArray<audAmbientZoneReplayState>&>(g_AmbientAudioEntity.GetAmbientZoneReplayStates());
	fullCreatePacketRecorder.RecordPacketWithParam<CPacketActiveAlarms, const atArray<audReplayAlarmState>&>(g_AmbientAudioEntity.GetReplayAlarmStates());
	fullCreatePacketRecorder.RecordPacketWithParam<CPacketAudioPedScenarios, const atArray<audReplayScenarioState>&>(g_PedScenarioManager.GetReplayScenarioStates());
	
	m_InteriorManager.ResetRoomRequests();

	ReplayInteriorManager::RecordData(fullCreatePacketRecorder, createPacketRecorder);

	ReplayLightingManager::RecordPackets(recorder);

	m_ClothPacketManager.RecordPackets(recorder);

	//we've finished recording the light list for this frame, so lets reset it
	ReplayLightingManager::Reset();

	fullCreatePacketRecorder.RecordPacket<CPacketDistantCarState>();

	m_recordBufferUsed = recorder.GetMemUsed();
	m_createPacketsBufferUsed = createPacketRecorder.GetMemUsed();
	m_fullCreatePacketsBufferUsed = fullCreatePacketRecorder.GetMemUsed();
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceGame::FindPreloadRequests(ReplayController& controller, tPreloadRequestArray& preloadRequests, u32& requestCount, const u32 systemTime, bool /*isSingleFrame*/)
{
	while(IsRelevantPacket(controller.GetCurrentPacketID()))
	{
		const CPacketBase* pPacket = controller.ReadCurrentPacket<CPacketBase>();

		if(pPacket->ShouldPreload())
		{
			++requestCount;
			if(!preloadRequests.IsFull())
			{
				preloadData data;
				data.pPacket = pPacket; 
				data.systemTime = systemTime;
				preloadRequests.Push(data);
			}
		}

		controller.AdvancePacket();
	}
	return !preloadRequests.IsFull();
}


bool CReplayInterfaceGame::TryPreloadPacket(const CPacketBase *pPacket, bool &preloadSuccess, u32 currTime PRELOADDEBUG_ONLY(, atString& failureReason))
{
	switch (pPacket->GetPacketID())
	{
	case PACKETID_INTERIOR_PROXY_STATE:
		{
			const CPacketInteriorProxyStates *pProxyStates = static_cast < const CPacketInteriorProxyStates * > (pPacket);
			preloadSuccess = pProxyStates->AddPreloadRequests(m_InteriorManager, currTime PRELOADDEBUG_ONLY(, failureReason));
			
			return true;
		}
	default:
		{
			//This isn't the correct interface, ignore.
			return false;
		}
	}
}


void CReplayInterfaceGame::UpdatePreloadRequests(u32 time)
{
	// Usual pre-load update.
	m_modelManager.UpdatePreloadRequests(time);
	// Special interior proxy update.
	m_InteriorManager.UpdateInteriorProxyRequests(time);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceGame::PostRecordFrame(ReplayController& controller, u16 previousBlockFrameCount, bool blockChange)
{
	iReplayInterface::PostRecordFrame(controller, previousBlockFrameCount, blockChange);

	controller.GetCurrentBlock()->AddTimeSpan(m_FrameStep);

	if( blockChange )
	{
		CBlockInfo* block = controller.GetCurrentBlock();
		block->SetStartTime(m_GameTime);

		CBlockInfo* prevBlock = NULL;

		prevBlock = controller.GetBufferInfo().GetPrevBlock(controller.GetCurrentBlock());

		if (prevBlock && prevBlock->GetStatus() != REPLAYBLOCKSTATUS_EMPTY)
		{
			prevBlock->AddTimeSpan(m_GameTime - prevBlock->GetEndTime());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceGame::LinkUpdatePackets(ReplayController& controller, u16 UNUSED_PARAM(previousBlockFrameCount), u32 endPosition)
{
	while(controller.GetCurrentPosition() != endPosition)
	{
		CPacketBase* pBasePacket = controller.GetCurrentPacket<CPacketBase>();

		if(pBasePacket->GetPacketID() == PACKETID_CUBIC_PATCH_WATERFRAME)
		{
			UpdatePacketLinkingMaintainedInfo<CPacketCubicPatchWaterFrame>(controller, pBasePacket, m_PacketLinkingInfo[WATER_LINKED_PACKET_INDEX]);
		}
		else if(pBasePacket->GetPacketID() == PACKETID_CLOTH_PIECES)
		{
			UpdatePacketLinkingMaintainedInfo<CPacketClothPieces>(controller, pBasePacket, m_PacketLinkingInfo[CLOTH_LINKED_PACKET_INDEX]);
		}
		else if(pBasePacket->GetPacketID() == PACKETID_WATERSIMULATE)
		{
			if(m_PreviousSimPacket)
			{
				CBlockInfo const* pCurrBlock = controller.GetCurrentBlock();
				u16 currBlockIndex = pCurrBlock->GetSessionBlockIndex();

				m_PreviousSimPacket->ValidatePacket();
				m_PreviousSimPacket->SetNextOffset(currBlockIndex, controller.GetCurrentPosition());
			}

			m_PreviousSimPacket = static_cast<CPacketWaterSimulate*>(pBasePacket);
		}	

		controller.AdvancePacket();
	}
}

template < class PACKETTYPE >
void CReplayInterfaceGame::UpdatePacketLinkingMaintainedInfo(ReplayController& controller, CPacketBase *pBasePacket, PacketLinkingMaintainedInfo &linkingInfo)
{
	u16 currentSessionBlock = controller.GetAddress().GetSessionBlockIndex();
	u32 currentPosition = controller.GetCurrentPosition();

	CPacketLinkedBase* currentFrame = static_cast<CPacketLinkedBase*>(pBasePacket);

	if(linkingInfo.m_PreviousCubicPatchWaterFrame)
	{
		linkingInfo.m_PreviousCubicPatchWaterFrame->SetNextPacket(currentSessionBlock, currentPosition);
		reinterpret_cast < PACKETTYPE *> (linkingInfo.m_PreviousCubicPatchWaterFrame)->GetNextPacket(controller.GetBufferInfo())->ValidatePacket();
		replayDebugf2("Setting Next Water Packet Address to %i %i", currentSessionBlock, currentPosition);

		currentFrame->SetPreviousPacket(linkingInfo.m_PreviousCubicPatchSessionBlock, linkingInfo.m_PreviousCubicPatchPosition);
		reinterpret_cast < PACKETTYPE *> (currentFrame)->GetPreviousPacket(controller.GetBufferInfo())->ValidatePacket();
		replayDebugf2("Setting Previous Water Packet Address to %i %i", linkingInfo.m_PreviousCubicPatchSessionBlock, linkingInfo.m_PreviousCubicPatchPosition);
	}

	currentFrame->SetFrameRecorded(controller.GetCurrentFrameRef());

	linkingInfo.m_PreviousCubicPatchWaterFrame = currentFrame;
	linkingInfo.m_PreviousCubicPatchSessionBlock = currentSessionBlock;
	linkingInfo.m_PreviousCubicPatchPosition = currentPosition;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceGame::OffsetUpdatePackets(s64 offset, u8* pStart, u32 blockSize)
{
	for(u32 i=0; i<NUM_LINKED_PACKET_TYPES; i++)
	{
		u8* pPreviousCubicPatchWaterFrame = (u8*)m_PacketLinkingInfo[i].m_PreviousCubicPatchWaterFrame;

		if(pPreviousCubicPatchWaterFrame >= pStart && pPreviousCubicPatchWaterFrame < (pStart + blockSize))
		{
			pPreviousCubicPatchWaterFrame += offset;
			m_PacketLinkingInfo[i].m_PreviousCubicPatchWaterFrame = (CPacketLinkedBase*)pPreviousCubicPatchWaterFrame;
			m_PacketLinkingInfo[i].m_PreviousCubicPatchWaterFrame->ValidatePacket();
		}
	}

	u8* pPreviousSimPacket = (u8*)m_PreviousSimPacket;

	if(pPreviousSimPacket >= pStart && pPreviousSimPacket < (pStart + blockSize))
	{
		pPreviousSimPacket += offset;
		m_PreviousSimPacket = (CPacketWaterSimulate*)pPreviousSimPacket;
		m_PreviousSimPacket->ValidatePacket();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceGame::PlaybackSetup(ReplayController& controller)
{
	//go through and find the last water packet, this will then be terminated properly below
	controller.EnableModify();

	CPacketBase* pBasePacket = controller.GetCurrentPacket<CPacketBase>();
	eReplayPacketId packetID = pBasePacket->GetPacketID();

	u32 linkedIdx = 0;

	if((linkedIdx = GetLinkedPacketIndex(packetID)) != -1)
	{
		CPacketLinkedBase* waterFramePacket = static_cast<CPacketLinkedBase *>(pBasePacket);

		//make sure the first packet's previous packet it correctly set to be invalid
		if(m_PacketLinkingInfo[linkedIdx].m_LastFoundCubicPatch == NULL)
		{
			waterFramePacket->SetPreviousPacket(0, LINKED_PACKET_INVALID_BLOCK_POS);
		}

		m_PacketLinkingInfo[linkedIdx].m_LastFoundCubicPatch = waterFramePacket;
	}

	controller.DisableModify();
}

void CReplayInterfaceGame::ClearPacketInformation()
{
	ResetPacketLinking();

	for(u32 i=0; i<NUM_LINKED_PACKET_TYPES; i++)
	{
		// If we have any current packets from PlaybackSetup() then we make sure it's next is set to be invalid
		if(m_PacketLinkingInfo[i].m_LastFoundCubicPatch)
		{
			m_PacketLinkingInfo[i].m_LastFoundCubicPatch->SetNextPacket(0, LINKED_PACKET_INVALID_BLOCK_POS);
			m_PacketLinkingInfo[i].m_LastFoundCubicPatch = NULL;
		}
	}

	m_CubicPatchWaterFrameT0 = NULL;
	m_CubicPatchWaterFrameTPlus1 = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceGame::ResetCreatePacketsForCurrentBlock()
{
	m_waterFrameTimer = 0;
	m_recordWaterFrame = true;
}

//////////////////////////////////////////////////////////////////////////
ePlayPktResult CReplayInterfaceGame::PreprocessPackets(ReplayController& controller, bool /*entityMayNotExist*/)
{
	do 
	{
		eReplayPacketId packetID = controller.GetCurrentPacketID();

		switch(packetID)
		{
		case PACKETID_GAMETIME:
			controller.ReadCurrentPacket<CPacketGameTime>();
		break;
		case PACKETID_CLOCK:
			controller.ReadCurrentPacket<CPacketClock>();
		break;
		case PACKETID_WEATHER:
			{
				CPacketWeather const* pPacketWeather = controller.ReadCurrentPacket<CPacketWeather>();
				if(m_overrideWeather == false)
				{
					pPacketWeather->Extract();
				}
			}
		break;
		case PACKETID_THERMALVISIONVALUES:
			{
				CPacketThermalVisionValues const* pThermalVisionValues = controller.ReadCurrentPacket<CPacketThermalVisionValues>();
				pThermalVisionValues->Extract();
			}
		break;
#if REPLAY_ENABLE_WATER
		case PACKETID_CUBIC_PATCH_WATERFRAME:
			{
				// Cache the current water frame and corresponding interpolation value etc.
				m_CubicPatchWaterFrameT0 = controller.ReadCurrentPacket<CPacketCubicPatchWaterFrame>();
				m_CubicPatchWaterFrameTPlus1 = NULL;
				m_waterGridPos[0] = m_waterGridPos[1] = GAME_INTERFACE_INVALID_WATER_GRID_POS;
				m_interpWaterFrame = controller.GetInterp();
				u32 waterGameTime = controller.GetCurrentFrameInfo().GetGameTime();

				if(m_CubicPatchWaterFrameT0 && m_CubicPatchWaterFrameT0->HasNextPacket())
				{
					// Seek 1 water packet forwards.
					m_CubicPatchWaterFrameTPlus1 = m_CubicPatchWaterFrameT0->GetNextPacket(controller.GetBufferInfo());

					if(m_CubicPatchWaterFrameT0->HasPacketSkippingInfo() && m_CubicPatchWaterFrameTPlus1)
					{
						u32 frameMod = 4;

						// Compute a game time from the controller values.
						waterGameTime = (u32)(m_interpWaterFrame*(float)controller.GetNextFrameInfo().GetGameTime()  + (1.0f - m_interpWaterFrame)*(float)controller.GetCurrentFrameInfo().GetGameTime());
						CPacketCubicPatchWaterFrame const *pNewT0 = SeekWaterFrameBackwards(m_CubicPatchWaterFrameT0, controller, frameMod);

						if(pNewT0)
						{
							CPacketCubicPatchWaterFrame const *pNewTPlus1 = SeekWaterFrameForwards(m_CubicPatchWaterFrameTPlus1, controller, frameMod);

							if(pNewTPlus1)
							{
								s32 newT0 = pNewT0->GetGameTime();
								s32 newTPlus1 = pNewTPlus1->GetGameTime();

								// Very occasionally we come across game times which fall just outside this range. Clamp to ensure a valid interpolation.
								if(waterGameTime < newT0)
									waterGameTime = newT0;
								if(waterGameTime > newTPlus1)
									waterGameTime = newTPlus1;

								// Compute a new interpolation value and re-direct interpolation to these spaced out frames.
								m_interpWaterFrame = (float)(waterGameTime - newT0)/(float)(newTPlus1 - newT0);					
								// Collect the grid position from the regular current frame.
								m_CubicPatchWaterFrameT0->GetGridXAndYFromExtensions(m_waterGridPos[0], m_waterGridPos[1]);
								// Set the two new frames.
								m_CubicPatchWaterFrameT0 = pNewT0;
								m_CubicPatchWaterFrameTPlus1 = pNewTPlus1;
							}
						}
					}
				}
			}
			break;
		case PACKETID_WATERSIMULATE:
			{
				const CPacketWaterSimulate* pCurrentPacket = controller.ReadCurrentPacket<CPacketWaterSimulate>();
				pCurrentPacket->Extract();
			}
			break;
#endif // REPLAY_ENABLE_WATER
		case PACKETID_TC_SCRIPT_MODIFIER:
			{
				CPacketScriptTCModifier const* pPacketTCModifier = controller.ReadCurrentPacket<CPacketScriptTCModifier>();
				pPacketTCModifier->Extract();
			}
			break;
// 		case PACKETID_MAP_CHANGE:	// This packet is expensive and so shouldn't be called on preprocess
// 			{
// 				CPacketMapChange const * pPacketMapChange = controller.ReadCurrentPacket<CPacketMapChange>();
// 				pPacketMapChange->Extract();
// 			}
// 			break;
		case PACKETID_DISABLED_THIS_FRAME:
			{
				CPacketDisabledThisFrame const * pPacketDisable = controller.ReadCurrentPacket<CPacketDisabledThisFrame>();
				pPacketDisable->Extract();
			}
			break;
		case PACKETID_PORTAL_OCCLUSION_OVERRIDES:
			{
				CPacketPortalOcclusionOverrides const * pPacketModifyPortalOcclusion = controller.ReadCurrentPacket<CPacketPortalOcclusionOverrides>();
				pPacketModifyPortalOcclusion->Extract();
			}
			break;
		case PACKETID_PER_PORTAL_OCCLUSION_OVERRIDES:
			{
				CPacketPerPortalOcclusionOverrides const * pPacketModifyPerPortalOcclusion = controller.ReadCurrentPacket<CPacketPerPortalOcclusionOverrides>();
				pPacketModifyPerPortalOcclusion->Extract();
			}
			break;
		case PACKETID_MODIFIED_AMBIENT_ZONE_STATES:
			{
				CPacketModifiedAmbientZoneStates const * pPacketModifiedAmbientZoneStates = controller.ReadCurrentPacket<CPacketModifiedAmbientZoneStates>();
				pPacketModifiedAmbientZoneStates->Extract();				
			}
			break;
		case PACKETID_ACTIVE_ALARMS:
			{
				CPacketActiveAlarms const * pPacketActiveAlarms = controller.ReadCurrentPacket<CPacketActiveAlarms>();
				pPacketActiveAlarms->Extract();
			}
			break;
		case PACKETID_AUD_PED_SCENARIOS:
			{
				CPacketAudioPedScenarios const * pPacketAudioPedScenarios = controller.ReadCurrentPacket<CPacketAudioPedScenarios>();
				pPacketAudioPedScenarios->Extract();
			}
			break;
		case PACKETID_INTERIOR_PROXY_STATE:
			{
				CPacketInteriorProxyStates const *pProxyStates = controller.ReadCurrentPacket<CPacketInteriorProxyStates>();
				pProxyStates->Extract();
			}
			break;
		case PACKETID_INTERIOR_ENTITY_SET:
			{
				CPacketInteriorEntitySet const *pInteriorEntitySet = controller.ReadCurrentPacket<CPacketInteriorEntitySet>();
				pInteriorEntitySet->Extract(false);
			}
			break;
		case PACKETID_INTERIOR_ENTITY_SET2:
			{
				CPacketInteriorEntitySet2 const *pInteriorEntitySet = controller.ReadCurrentPacket<CPacketInteriorEntitySet2>();
				pInteriorEntitySet->Extract(false);
			}
			break;
		case PACKETID_DISTANTCARSTATE:
			{
				CPacketDistantCarState const * pPacketDistantCarState = controller.ReadCurrentPacket<CPacketDistantCarState>();
				pPacketDistantCarState->Extract();
			}
			break;
		case PACKETID_TIMECYCLE_MODIFIER:
			{
				CPacketTimeCycleModifier const* pPacketTCModifier = controller.ReadCurrentPacket<CPacketTimeCycleModifier>();
				pPacketTCModifier->Extract();
			}
			break;
		case PACKETID_CLOTH_PIECES:
			{
				CPacketClothPieces const* pClothPacket = controller.ReadCurrentPacket<CPacketClothPieces>();
				u32 currentTime = (u32)((float)(controller.GetNextFrameInfo().GetGameTime() - controller.GetCurrentFrameInfo().GetGameTime())*controller.GetInterp()) + controller.GetCurrentFrameInfo().GetGameTime();
				m_ClothPacketManager.Extract(pClothPacket, pClothPacket->HasNextPacket() ? pClothPacket->GetNextPacket(controller.GetBufferInfo()) : NULL, controller.GetInterp(), currentTime);
			}
			break;
		default:
			{
				controller.ReadCurrentPacket<CPacketBase>();
				if(!IsRelevantPacket(controller.GetCurrentPacketID()))
				{
					return PACKET_UNHANDLED;
				}
			}
		}	

		controller.AdvancePacket();

	} while (!controller.IsNextFrame());

	return PACKET_UNHANDLED;
}



CPacketCubicPatchWaterFrame const *CReplayInterfaceGame::SeekWaterFrameForwards(CPacketCubicPatchWaterFrame const *pStart, ReplayController& controller, u32 frameMod)
{
	while(pStart && ((pStart->GetPacketNumber() % frameMod) != 0))
	{
		if(pStart->HasNextPacket())
			pStart = pStart->GetNextPacket(controller.GetBufferInfo());
		else
			return pStart;
	}
	return pStart;

}

CPacketCubicPatchWaterFrame const *CReplayInterfaceGame::SeekWaterFrameBackwards(CPacketCubicPatchWaterFrame const *pStart, ReplayController& controller, u32 frameMod)
{
	while(pStart && ((pStart->GetPacketNumber() % frameMod) != 0))
	{
		if(pStart->HasPreviousPacket())
			pStart = pStart->GetPreviousPacket(controller.GetBufferInfo());
		else
			return NULL;
	}
	return pStart;
}

//////////////////////////////////////////////////////////////////////////
ePlayPktResult CReplayInterfaceGame::PlayPackets(ReplayController& controller, bool)
{
	//reset the light list for this frame
	ReplayLightingManager::Reset();

	do 
	{
		eReplayPacketId packetID = controller.GetCurrentPacketID();
		switch(packetID)
		{
			case PACKETID_GAMETIME:
			{
				CPacketGameTime const* pPacketGameTime = controller.ReadCurrentPacket<CPacketGameTime>();
				m_timeScale = pPacketGameTime->GetTimeScale();
			}
			break;
			case PACKETID_CLOCK:
			{
				CPacketClock const* pPacketClock = controller.ReadCurrentPacket<CPacketClock>();

				if(m_overrideTime == false)
				{
					CClock::SetTime(pPacketClock->GetClockHours(), pPacketClock->GetClockMinutes(), pPacketClock->GetClockSeconds());
				}
			}
			break;
			case PACKETID_WEATHER:
				controller.ReadCurrentPacket<CPacketWeather>();
			break;
			case PACKETID_CUBIC_PATCH_WATERFRAME:
				controller.ReadCurrentPacket<CPacketCubicPatchWaterFrame>();
				break;
			case PACKETID_WATERSIMULATE:
				controller.ReadCurrentPacket<CPacketWaterSimulate>();
			break;
			case PACKETID_TC_SCRIPT_MODIFIER:
				controller.ReadCurrentPacket<CPacketScriptTCModifier>();
			break;
			case  PACKETID_MAP_CHANGE:
			{
				CPacketMapChange const * pPacketMapChange = controller.ReadCurrentPacket<CPacketMapChange>();
				pPacketMapChange->Extract();
			}
			break;
			case  PACKETID_DISABLED_THIS_FRAME:
				{
					CPacketDisabledThisFrame const* pPacketDisable = controller.ReadCurrentPacket<CPacketDisabledThisFrame>();
					pPacketDisable->Extract();
				}
				break;
			case PACKETID_PORTAL_OCCLUSION_OVERRIDES:
				controller.ReadCurrentPacket<CPacketPortalOcclusionOverrides>();
			break;
			case PACKETID_PER_PORTAL_OCCLUSION_OVERRIDES:
				controller.ReadCurrentPacket<CPacketPerPortalOcclusionOverrides>();
				break;
			case PACKETID_MODIFIED_AMBIENT_ZONE_STATES:
				controller.ReadCurrentPacket<CPacketModifiedAmbientZoneStates>();
			break;
			case PACKETID_ACTIVE_ALARMS:
				controller.ReadCurrentPacket<CPacketActiveAlarms>();
			break;
			case PACKETID_AUD_PED_SCENARIOS:
				controller.ReadCurrentPacket<CPacketAudioPedScenarios>();
			break;
			case  PACKETID_INTERIOR_PROXY_STATE:
				controller.ReadCurrentPacket<CPacketInteriorProxyStates>();
				break;
			case PACKETID_INTERIOR_ENTITY_SET:
				controller.ReadCurrentPacket<CPacketInteriorEntitySet>();
				break;
			case PACKETID_INTERIOR_ENTITY_SET2:
				controller.ReadCurrentPacket<CPacketInteriorEntitySet2>();
				break;
			case PACKETID_CUTSCENELIGHT:
			{
				CPacketCutsceneLight const* pPacketCutsceneLight = controller.ReadCurrentPacket<CPacketCutsceneLight>();
				if( pPacketCutsceneLight )
				{
					pPacketCutsceneLight->Extract(controller.GetPlaybackFlags());
				}
				break;
			}
			case PACKETID_DISTANTCARSTATE:
				controller.ReadCurrentPacket<CPacketDistantCarState>();
				break;
			case PACKETID_TIMECYCLE_MODIFIER:
				controller.ReadCurrentPacket<CPacketTimeCycleModifier>();
				break;
			case PACKETID_CLOTH_PIECES:
				controller.ReadCurrentPacket<CPacketClothPieces>();
				break;
			default:
			{
				controller.ReadCurrentPacket<CPacketBase>();
				if(!IsRelevantPacket(controller.GetCurrentPacketID()))
					return PACKET_UNHANDLED;
			}
		}

		controller.AdvancePacket();
	} while (!controller.IsNextFrame());

	return PACKET_UNHANDLED;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceGame::JumpPackets(ReplayController& controller)
{
	do 
	{
		eReplayPacketId packetID = controller.GetCurrentPacketID();
		switch(packetID)
		{
		case PACKETID_GAMETIME:
			controller.ReadCurrentPacket<CPacketGameTime>();
			break;
		case PACKETID_CLOCK:
			controller.ReadCurrentPacket<CPacketClock>();
			break;
		case PACKETID_WEATHER:
			controller.ReadCurrentPacket<CPacketWeather>();
			break;
		case PACKETID_CUBIC_PATCH_WATERFRAME:
			m_CubicPatchWaterFrameT0 = controller.ReadCurrentPacket<CPacketCubicPatchWaterFrame>();
			break;
		case PACKETID_WATERSIMULATE:
			controller.ReadCurrentPacket<CPacketWaterSimulate>();
			break;
		case PACKETID_PORTAL_OCCLUSION_OVERRIDES:
			controller.ReadCurrentPacket<CPacketPortalOcclusionOverrides>();
			break;
		case PACKETID_PER_PORTAL_OCCLUSION_OVERRIDES:
			controller.ReadCurrentPacket<CPacketPerPortalOcclusionOverrides>();
			break;
		case PACKETID_MODIFIED_AMBIENT_ZONE_STATES:
			controller.ReadCurrentPacket<CPacketModifiedAmbientZoneStates>();
			break;
		case PACKETID_ACTIVE_ALARMS:
			controller.ReadCurrentPacket<CPacketActiveAlarms>();
			break;
		case PACKETID_AUD_PED_SCENARIOS:
			controller.ReadCurrentPacket<CPacketAudioPedScenarios>();
			break;
		case PACKETID_TC_SCRIPT_MODIFIER:
			controller.ReadCurrentPacket<CPacketScriptTCModifier>();
			break;
		case PACKETID_MAP_CHANGE:
			controller.ReadCurrentPacket<CPacketMapChange>();
			break;
		case  PACKETID_DISABLED_THIS_FRAME:
			controller.ReadCurrentPacket<CPacketDisabledThisFrame>();
			break;
		case PACKETID_INTERIOR_PROXY_STATE:
			controller.ReadCurrentPacket<CPacketInteriorProxyStates>();
			break;
		case PACKETID_INTERIOR_ENTITY_SET:
			{
				CPacketInteriorEntitySet const *pPacket = controller.ReadCurrentPacket<CPacketInteriorEntitySet>();
				pPacket->Extract(true);
				break;
			}
		case PACKETID_INTERIOR_ENTITY_SET2:
			{
				CPacketInteriorEntitySet2 const *pPacket = controller.ReadCurrentPacket<CPacketInteriorEntitySet2>();
				pPacket->Extract(true);
				break;
			}
		case PACKETID_CUTSCENELIGHT:
			controller.ReadCurrentPacket<CPacketCutsceneLight>();
			break;
		case PACKETID_DISTANTCARSTATE:
			controller.ReadCurrentPacket<CPacketDistantCarState>();
			break;
		case PACKETID_TIMECYCLE_MODIFIER:
			controller.ReadCurrentPacket<CPacketTimeCycleModifier>();
			break;
		case PACKETID_CLOTH_PIECES:
			controller.ReadCurrentPacket<CPacketClothPieces>();
			break;
		default:
			{
				controller.ReadCurrentPacket<CPacketBase>();
				if(!IsRelevantPacket(controller.GetCurrentPacketID()))
					return;
			}
		}

		controller.AdvancePacket();
	} while (!controller.IsNextFrame());
}


//////////////////////////////////////////////////////////////////////////
s32 CReplayInterfaceGame::GetLinkedPacketIndex(eReplayPacketId packetId)
{
	if(packetId == PACKETID_CUBIC_PATCH_WATERFRAME)
		return WATER_LINKED_PACKET_INDEX;
	else if(packetId == PACKETID_CLOTH_PIECES)
		return CLOTH_LINKED_PACKET_INDEX;
	else
		return -1;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceGame::FindPacketDescriptor(eReplayPacketId packetID, const CPacketDescriptorBase*& pDescriptor) const
{
	for(int i = 0; i < m_numPacketDescriptors; ++i)
	{
		const CPacketDescriptorBase& desc = m_packetDescriptors[i];
		if(desc.GetPacketID() == packetID)
		{
			pDescriptor = &desc;
			return true;
		}
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceGame::GetPacketName(eReplayPacketId packetID, const char*& packetName) const
{
	const CPacketDescriptorBase* pPacketDescriptor = NULL;
	if(FindPacketDescriptor(packetID, pPacketDescriptor))
	{
		packetName = pPacketDescriptor->GetPacketName();
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceGame::IsRelevantPacket(eReplayPacketId packetID) const
{
	const CPacketDescriptorBase* pPacketDescriptor = NULL;
	if(FindPacketDescriptor(packetID, pPacketDescriptor))
		return true;

	if(packetID == PACKETID_ALIGN)
		return true;

	return false;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceGame::PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
{
	CPacketBase const* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
	PrintOutPacketHeader(pBasePacket, mode, handle);

	const CPacketDescriptorBase* pPacketDescriptor = NULL;
	if(FindPacketDescriptor(pBasePacket->GetPacketID(), pPacketDescriptor))
	{
		pPacketDescriptor->PrintOutPacket(controller, mode, handle);
	}
	else
	{
		u32 packetSize = pBasePacket->GetPacketSize();
		replayAssert(packetSize != 0);
		controller.AdvanceUnknownPacket(packetSize);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceGame::FlushPreloadRequests()
{
	iReplayInterface::FlushPreloadRequests();
	m_InteriorManager.FlushPreloadRequests();
}



//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceGame::ExtractWaterFrame(float *pDest, float *pSecondaryDest)
{
	if(m_CubicPatchWaterFrameT0)
	{
		if(m_waterGridPos[0] != GAME_INTERFACE_INVALID_WATER_GRID_POS)
			m_CubicPatchWaterFrameT0->ExtractWithGridShift(m_interpWaterFrame, m_CubicPatchWaterFrameTPlus1, pDest, m_waterGridPos[0], m_waterGridPos[1]);
		else
			m_CubicPatchWaterFrameT0->Extract(m_interpWaterFrame, m_CubicPatchWaterFrameTPlus1, pDest);

		if(pSecondaryDest)
			sysMemCpy(pSecondaryDest, pDest, sizeof(float)*DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS);
	}
}

//////////////////////////////////////////////////////////////////////////
#if __BANK
bkGroup* CReplayInterfaceGame::AddDebugWidgets()
{
	bkGroup* pGroup = iReplayInterface::AddDebugWidgets("Game");
	if(pGroup == NULL)
		return NULL;

	//pGroup->AddButton("Toggle Use Recorded Cam", ToggleUseRecordedCam);

	return pGroup;
}
#endif // __BANK



//////////////////////////////////////////////////////////////////////////
/*
bool CPacketLinkedBase::UpCastAndValidatePacket() const
{
	if(GetPacketID() == PACKETID_CUBIC_PATCH_WATERFRAME)
		return static_cast < CPacketCubicPatchWaterFrame const* > (this)->ValidatePacket();
	else if(GetPacketID() == PACKETID_CLOTH_PIECES)
		return static_cast < CPacketClothPieces const* > (this)->ValidatePacket();

	replayAssertf(false, "CPacketLinkedBase::PacketValidate()...Unknown linked packet type.");
	return false;
}
*/

#endif // GTA_REPLAY
