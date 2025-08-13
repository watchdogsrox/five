/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/playerswitch/PlayerSwitchMgrLong.cpp
// PURPOSE : manages all state, camera, streaming behaviour etc for long-range player switch
// AUTHOR :  Ian Kiigan
// CREATED : 14/11/12
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/playerswitch/PlayerSwitchMgrLong.h"

#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/switch/SwitchCamera.h"
#include "camera/switch/SwitchDirector.h"
#include "camera/system/CameraMetadata.h"
#include "frontend/MiniMap.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/maptypesstore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwsys/timer.h"
#include "peds/PedFactory.h"
#include "peds/PlayerInfo.h"
#include "peds/ped.h"
#include "peds/pedpopulation.h"
#include "renderer/lights/lights.h"
#include "renderer/PlantsMgr.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "scene/FocusEntity.h"
#include "scene/LoadScene.h"
#include "scene/lod/LodMgr.h"
#include "scene/lod/LodScale.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/streamer/StreamVolume.h"
#include "scene/world/GameWorld.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streamingvisualize.h"
#include "timecycle/TimeCycle.h"
#include "task/Scenario/Types/TaskUseScenario.h"
#include "vehicles/cargen.h"
#include "vehicles/vehiclepopulation.h"
#include "vfx/misc/LODLightManager.h"
#include "vfx/misc/DistantLights.h"

#if __ASSERT
#include "streaming/streamingrequestlist.h"
#endif

#if !__FINAL
PARAM(debugPlayerSwitch, "");

const char* stateToString(CPlayerSwitchMgrLong::eState state)
{
	switch(state)
	{
	case CPlayerSwitchMgrLong::STATE_INTRO: return "STATE_INTRO";
	case CPlayerSwitchMgrLong::STATE_PREP_DESCENT: return "STATE_PREP_DESCENT";
	case CPlayerSwitchMgrLong::STATE_PREP_FOR_CUT: return "STATE_PREP_FOR_CUT";
	case CPlayerSwitchMgrLong::STATE_JUMPCUT_ASCENT: return "STATE_JUMPCUT_ASCENT";
	case CPlayerSwitchMgrLong::STATE_WAITFORINPUT_INTRO: return "STATE_WAITFORINPUT_INTRO";
	case CPlayerSwitchMgrLong::STATE_WAITFORINPUT: return "STATE_WAITFORINPUT";
	case CPlayerSwitchMgrLong::STATE_WAITFORINPUT_OUTRO: return "STATE_WAITFORINPUT_OUTRO";
	case CPlayerSwitchMgrLong::STATE_PAN: return "STATE_PAN";
	case CPlayerSwitchMgrLong::STATE_JUMPCUT_DESCENT: return "STATE_JUMPCUT_DESCENT";
	case CPlayerSwitchMgrLong::STATE_OUTRO_HOLD: return "STATE_OUTRO_HOLD";
	case CPlayerSwitchMgrLong::STATE_OUTRO_SWOOP: return "STATE_OUTRO_SWOOP";
	case CPlayerSwitchMgrLong::STATE_ESTABLISHING_SHOT: return "STATE_ESTABLISHING_SHOT";
	case CPlayerSwitchMgrLong::STATE_FINISHED: return "STATE_FINISHED";
	case CPlayerSwitchMgrLong::STATE_TOTAL: return "STATE_TOTAL";
	default: return "Invalid";
	}
}

const char* controlFlagsToString(s32 controlFlags)
{
	static atFixedString<32> flags;
	flags.Reset();
	for (int i=31; i>=0; i--)
	{
		flags.Append(((controlFlags & BIT(i))!=0) ? "1" : "0");
	}
	return flags.c_str();
}
#endif

atHashString CPlayerSwitchMgrLong::ms_switchFxName[CPlayerSwitchMgrLong::SWITCHFX_COUNT] = 
{	
	atHashString("SwitchOpenFranklinIn",	0x776fe6f3), 
	atHashString("SwitchOpenMichaelIn",		0x848674fd), 
	atHashString("SwitchOpenTrevorIn",		0x1a4e629d),
	atHashString("SwitchOpenFranklinOut",	0x1bae2d29), 
	atHashString("SwitchOpenMichaelOut",	0x15dca17b), 
	atHashString("SwitchOpenTrevorOut",		0x87f585eb), 
	atHashString("SwitchOpenNeutralIn",		0xe5cafc5b), 
	atHashString("SwitchOpenNeutral",		0xc36d571f), 
	atHashString("SwitchHUDIn",				0xb2895e1b),
	atHashString("SwitchHUDMichaelIn",		0x10493196),
	atHashString("SwitchHUDFranklinIn",		0x07e17b1a),
	atHashString("SwitchHUDTrevorIn",		0xee33c206),
	atHashString("SwitchOpenNeutralOut",	0x2977920c),
	atHashString("SwitchOpenFranklinMid",	0x22d04949), 
	atHashString("SwitchOpenMichaelMid",	0x621be8f8), 
	atHashString("SwitchOpenTrevorMid",		0xfb80db62),
	atHashString("SwitchOpenNeutralMid",	0x6cbdd92d),
	atHashString("SwitchOpenCopIn",			0xCAA23580),
	atHashString("SwitchOpenCopMid",		0x357FD84A),
	atHashString("SwitchOpenCopOut",		0x30BC3EB5),
	atHashString("SwitchOpenCrookIn",		0x2767DA43),
	atHashString("SwitchOpenCrookMid",		0x7990E0DD),
	atHashString("SwitchOpenCrookOut",		0x9BEBEC20),
	atHashString("SwitchOpenNeutralCnC",	0x53F72A36),
};

CPlayerSwitchMgrLong::eSwitchEffectType CPlayerSwitchMgrLong::ms_currentSwitchFxId = CPlayerSwitchMgrLong::SWITCHFX_NEUTRAL;

#if __ASSERT
static bool s_streamedOutroSphere = false;
#endif

#if PLAYERSWITCH_FORCES_LIGHTDIR
Vec3V lightPositionOverride = Vec3V(V_Z_AXIS_WZERO);
#endif // PLAYERSWITCH_FORCES_LIGHTDIR

static const float g_MinPanDurationInSecondsForMp = 1.0f;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartStreaming
// PURPOSE:		start requesting for the specified state
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong_Streamer::StartStreaming(const CPlayerSwitchMgrLong_State& stateToStreamFor)
{
	StopStreaming( stateToStreamFor.GetState() == CPlayerSwitchMgrLong::STATE_ESTABLISHING_SHOT );

	camSwitchCamera* pSwitchCamera = camInterface::GetSwitchDirector().GetSwitchCamera();
#if __ASSERT
	if(!pSwitchCamera)
	{
		camBaseCamera *pActiveCamera = camInterface::GetSwitchDirector().GetActiveCamera();
		Assertf(pSwitchCamera, "pSwitchCamera is NULL! ActiveCamera: %p %s %s",
			pActiveCamera,
			pActiveCamera ? pActiveCamera->GetClassId().GetCStr() : "(unknown class id)",
			pActiveCamera ? pActiveCamera->GetMetadata().m_Name.GetCStr() : "(unknown name)");
	}
#endif // __ASSERT

	switch (stateToStreamFor.GetState())
	{
	case CPlayerSwitchMgrLong::STATE_JUMPCUT_ASCENT:
		{
			//////////////////////////////////////////////////////////////////////////
			// ascending static shots
			m_bIsStreaming = true;

			camFrame cameraFrameToStream;
			pSwitchCamera->ComputeFrameForAscentCut( stateToStreamFor.GetJumpCutIndex(), cameraFrameToStream );
			const float fFarClip = Max( cameraFrameToStream.GetFarClip(), cameraFrameToStream.GetPosition().GetZ() );

			// start frustum based load scene at the specified position
			g_LoadScene.Start(
				VECTOR3_TO_VEC3V( cameraFrameToStream.GetPosition() ),	// position of camera
				VECTOR3_TO_VEC3V( cameraFrameToStream.GetFront() ),		// front vector
				fFarClip,												// far clip
				true,													// use frustum rather than sphere
				0,
				CLoadScene::LOADSCENE_OWNER_PLAYERSWITCH
			);

			// use rage-format camera matrix for streaming (via visibility system)
			Matrix34 worldMatrix;
			cameraFrameToStream.ComputeRageWorldMatrix( worldMatrix );
			g_LoadScene.SetCameraMatrix( MATRIX34_TO_MAT34V(worldMatrix) );

			// if streaming for ceiling, override required lod and map data asset type
			if (stateToStreamFor.GetJumpCutIndex() == m_switchParams.m_numJumpsAscent-1)
			{
				const fwBoxStreamerAssetFlags panMapDataAssetMask = (fwBoxStreamerAssetFlags) g_LodMgr.GetMapDataAssetMaskForLodLevel(m_switchParams.m_ceilingLodLevel);
				const u32 panLodMask = (1 << m_switchParams.m_ceilingLodLevel);

				g_LoadScene.OverrideRequiredAssets(panMapDataAssetMask, panLodMask);
			}

			g_LoadScene.SkipInteriorChecks();
			//////////////////////////////////////////////////////////////////////////
		}
		break;

	case CPlayerSwitchMgrLong::STATE_PAN:
		{
			//////////////////////////////////////////////////////////////////////////
			// camera pan
			m_bIsStreaming = true;

			camFrame cameraFramePanStart;
			camFrame cameraFramePanEnd;

			pSwitchCamera->ComputeFrameForAscentCeiling( cameraFramePanStart );
			pSwitchCamera->ComputeFrameForDescentCeiling( cameraFramePanEnd );

			// select lod level for streaming pan assets, and the .imap asset type to match
			const fwBoxStreamerAssetFlags panMapDataAssetMask	= (fwBoxStreamerAssetFlags) g_LodMgr.GetMapDataAssetMaskForLodLevel(m_switchParams.m_ceilingLodLevel);
			const u32 panLodMask			= (1 << m_switchParams.m_ceilingLodLevel);

			// start special line-segment streamer for specified map data and drawable assets
			CStreamVolumeMgr::RegisterLine(
				VECTOR3_TO_VEC3V( cameraFramePanStart.GetPosition() ),	// start of pan
				VECTOR3_TO_VEC3V( cameraFramePanEnd.GetPosition() ),	// end of pan
				panMapDataAssetMask,									// .imap asset mask
				true,													// scene streaming required
				CStreamVolumeMgr::VOLUME_SLOT_PRIMARY,
				(m_switchParams.m_switchType == CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM) ? 300.0f : FLT_MAX
			);

			CStreamVolume& volume = CStreamVolumeMgr::GetVolume( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );
			volume.SetLodMask( panLodMask );	// only request specific models for purposes of panning across world very quickly

			{
				/// WIP
				Matrix34 worldMatrix;
				cameraFramePanStart.ComputeRageWorldMatrix( worldMatrix );
				Vector3 offset = ( cameraFramePanEnd.GetPosition() - cameraFramePanStart.GetPosition() );

				volume.SetLineSeg( MATRIX34_TO_MAT34V(worldMatrix), VECTOR3_TO_VEC3V(offset), m_switchParams.m_fCeilingHeight);
			}
			
			//////////////////////////////////////////////////////////////////////////
		}
		break;

	case CPlayerSwitchMgrLong::STATE_JUMPCUT_DESCENT:
		{
			//////////////////////////////////////////////////////////////////////////
			// descending static shots
			m_bIsStreaming = true;

			camFrame cameraFrameToStream;
			pSwitchCamera->ComputeFrameForDescentCut( stateToStreamFor.GetJumpCutIndex(), cameraFrameToStream );
			const float fFarClip = Max( cameraFrameToStream.GetFarClip(), cameraFrameToStream.GetPosition().GetZ() );

			// start frustum based load scene at the specified position
			g_LoadScene.Start(
				VECTOR3_TO_VEC3V( cameraFrameToStream.GetPosition() ),	// position of camera
				VECTOR3_TO_VEC3V( cameraFrameToStream.GetFront() ),		// front vector
				fFarClip,												// far clip
				true,													// use frustum rather than sphere
				0,
				CLoadScene::LOADSCENE_OWNER_PLAYERSWITCH
			);

			// use rage-format camera matrix for streaming (via visibility system)
			Matrix34 worldMatrix;
			cameraFrameToStream.ComputeRageWorldMatrix( worldMatrix );
			g_LoadScene.SetCameraMatrix( MATRIX34_TO_MAT34V(worldMatrix) );

			g_LoadScene.SkipInteriorChecks();
			//////////////////////////////////////////////////////////////////////////
		}
		break;

	case CPlayerSwitchMgrLong::STATE_ESTABLISHING_SHOT:
		{
			//////////////////////////////////////////////////////////////////////////
			// establishing shot - stream the hold frame at the start (and end) of the swoop
			camFrame cameraFrameToStreamStart;
			camFrame cameraFrameToStreamEnd;
			const bool bHasValidFrameStart = pSwitchCamera->ComputeFrameForEstablishingShotStart( cameraFrameToStreamStart );
			const bool bHasValidFrameEnd = pSwitchCamera->ComputeFrameForEstablishingShotEnd( cameraFrameToStreamEnd );
			if ( bHasValidFrameStart && bHasValidFrameEnd )
			{
				//////////////////////////////////////////////////////////////////////////
				// 1. most important shot is the initial shot which holds for a beat - let's do that accurately
				m_bIsStreaming = true;

				// start frustum based load scene at the specified position
				g_LoadScene.Start(
					VECTOR3_TO_VEC3V( cameraFrameToStreamStart.GetPosition() ),	// position of camera
					VECTOR3_TO_VEC3V( cameraFrameToStreamStart.GetFront() ),		// front vector
					60.0f, //cameraFrameToStream.GetFarClip(),						// far clip
					true,													// use frustum rather than sphere
					0,
					CLoadScene::LOADSCENE_OWNER_PLAYERSWITCH
				);

				// use rage-format camera matrix for streaming (via visibility system)
				Matrix34 worldMatrix;
				cameraFrameToStreamStart.ComputeRageWorldMatrix( worldMatrix );
				g_LoadScene.SetCameraMatrix( MATRIX34_TO_MAT34V(worldMatrix) );

				//////////////////////////////////////////////////////////////////////////
				// 2. secondary streamer for the end shot (after interp)
				CStreamVolumeMgr::RegisterFrustum(
					VECTOR3_TO_VEC3V( cameraFrameToStreamEnd.GetPosition() ),
					VECTOR3_TO_VEC3V( cameraFrameToStreamEnd.GetFront() ),
					60.0f,
					fwBoxStreamerAsset::MASK_MAPDATA, true, CStreamVolumeMgr::VOLUME_SLOT_SECONDARY,
					true
				);
				CStreamVolume& volume = CStreamVolumeMgr::GetVolume( CStreamVolumeMgr::VOLUME_SLOT_SECONDARY );
				volume.SetLodMask(LODTYPES_FLAG_HD | LODTYPES_FLAG_ORPHANHD | LODTYPES_FLAG_LOD | LODTYPES_FLAG_SLOD1 );

				break;
			}
			//////////////////////////////////////////////////////////////////////////

			// intentional fall through if we don't have a valid frame for the establishing shot - just stream the same as outro
		}

	case CPlayerSwitchMgrLong::STATE_OUTRO_HOLD:
		{
			//////////////////////////////////////////////////////////////////////////
			// destination scene - this could be improved!
			m_bIsStreaming = true;

			// keeps map data files alive until load scene creates its streaming volume.
			spdSphere searchVolSphere(m_vDestPos, ScalarV(60.0f));
			CStreamVolumeMgr::RegisterSphere(searchVolSphere, fwBoxStreamerAsset::MASK_MAPDATA, false, CStreamVolumeMgr::VOLUME_SLOT_PRIMARY, false, false);

			// start frustum based load scene at the specified position
			g_LoadScene.Start(
				m_vDestPos,			// final destination of switch
				Vec3V(V_ZERO),		// no front vector needed
				60.0f,				// radius of sphere
				false,				// use sphere rather than frustum
				0,
				CLoadScene::LOADSCENE_OWNER_PLAYERSWITCH
			);
			g_LoadScene.OverrideRequiredAssets(fwBoxStreamerAsset::MASK_MAPDATA, LODTYPES_MASK_ALL);
			//////////////////////////////////////////////////////////////////////////

#if __ASSERT
			s_streamedOutroSphere = true;
#endif	//__ASSERT

		}

		break;


	default:
		Assert(false);
		break;
	}

	m_stateToStreamFor = stateToStreamFor;

	// only allow streaming to timeout on first and final scene stream (because they are the most expensive scenes)
	m_bCanTimeout =		(	(stateToStreamFor.GetState()==CPlayerSwitchMgrLong::STATE_OUTRO_HOLD)
						||	(stateToStreamFor.GetState()==CPlayerSwitchMgrLong::STATE_JUMPCUT_ASCENT)
						||	(stateToStreamFor.GetState()==CPlayerSwitchMgrLong::STATE_JUMPCUT_DESCENT)
						||	(stateToStreamFor.GetState()==CPlayerSwitchMgrLong::STATE_ESTABLISHING_SHOT)	);

	if (m_bCanTimeout)
	{
		m_timeout = STREAMING_TIMEOUT_NORMAL;

		switch ( stateToStreamFor.GetState() )
		{
		case CPlayerSwitchMgrLong::STATE_JUMPCUT_ASCENT:
			if ( !m_bStartedFromInterior && stateToStreamFor.GetJumpCutIndex()==0 )
			{
				m_timeout = STREAMING_TIMEOUT_SHORT;
			}
			break;
		case CPlayerSwitchMgrLong::STATE_JUMPCUT_DESCENT:
			{
				// descent only medium switches have a lot of streaming to do, so increase the timeout in that case
				if (stateToStreamFor.GetJumpCutIndex()==0 && m_switchParams.m_switchType==CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM)
				{
					m_timeout = STREAMING_TIMEOUT_LONG;
				}
			}
			break;
		case CPlayerSwitchMgrLong::STATE_OUTRO_HOLD:
			{
				m_timeout = STREAMING_TIMEOUT_OUTRO;
			}
			break;

		default:
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartStreamingSecondary
// PURPOSE:		secondary streaming volume helper to speed up descent by
//				requesting end scene a lot earlier
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong_Streamer::StartStreamingSecondary(Vec3V_In vPos)
{
	m_bIsStreamingSecondary = true;

	CStreamVolumeMgr::RegisterSphere(
		spdSphere(vPos, ScalarV(600.0f)),
		fwBoxStreamerAsset::MASK_MAPDATA_LODS,
		true,
		CStreamVolumeMgr::VOLUME_SLOT_SECONDARY
	);
	CStreamVolume& volume = CStreamVolumeMgr::GetVolume( CStreamVolumeMgr::VOLUME_SLOT_SECONDARY );
	//volume.SetLodMask(LODTYPES_FLAG_HD | LODTYPES_FLAG_ORPHANHD | LODTYPES_FLAG_LOD | LODTYPES_FLAG_SLOD1 );
	volume.SetLodMask(LODTYPES_FLAG_LOD | LODTYPES_FLAG_SLOD1 | LODTYPES_FLAG_SLOD2 | LODTYPES_FLAG_SLOD3 | LODTYPES_FLAG_SLOD4);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsFinishedStreaming
// PURPOSE:		returns true if the current streaming requirements are satisfied
//////////////////////////////////////////////////////////////////////////
bool CPlayerSwitchMgrLong_Streamer::IsFinishedStreaming()
{
	if ( IsStreaming() )
	{
		switch (m_stateToStreamFor.GetState())
		{
		case CPlayerSwitchMgrLong::STATE_JUMPCUT_ASCENT:
		case CPlayerSwitchMgrLong::STATE_JUMPCUT_DESCENT:
		case CPlayerSwitchMgrLong::STATE_OUTRO_HOLD:
			if (g_LoadScene.IsActive())
			{
				return g_LoadScene.IsLoaded();
			}
			break;

		case CPlayerSwitchMgrLong::STATE_ESTABLISHING_SHOT:
			{
				const bool bLoadSceneFinished = !g_LoadScene.IsActive() || g_LoadScene.IsLoaded();
				const bool bSecondaryFinished =  !CStreamVolumeMgr::IsVolumeActive(CStreamVolumeMgr::VOLUME_SLOT_SECONDARY) || CStreamVolumeMgr::HasLoaded(CStreamVolumeMgr::VOLUME_SLOT_SECONDARY);
				return ( bLoadSceneFinished && bSecondaryFinished );
			}
			break;

		case CPlayerSwitchMgrLong::STATE_PAN:
			return CStreamVolumeMgr::HasLoaded( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );

		default:
			break;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StopStreaming
// PURPOSE:		cancels any active streaming volumes etc
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong_Streamer::StopStreaming(bool bAll/*=false*/)
{
	m_bIsStreaming = false;
	m_bCanTimeout = false;

	g_LoadScene.Stop();
	CStreamVolumeMgr::Deregister( CStreamVolumeMgr::VOLUME_SLOT_PRIMARY );

	if (bAll)
	{
		CStreamVolumeMgr::Deregister( CStreamVolumeMgr::VOLUME_SLOT_SECONDARY );
		m_bIsStreamingSecondary = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Start
// PURPOSE:		initiate a long or medium range player switch between the specified peds,
//				with the specified number of jump cuts when ascending /	descending
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::StartInternal(CPed& oldPed, CPed& newPed, const CPlayerSwitchParams& params)
{
	STRVIS_SET_MARKER_TEXT("Player switch - START");

	Assertf( params.m_numJumpsAscent, "Need at least one jump cut on ascent" );
	Assertf( params.m_numJumpsDescent, "Need at least one jump cut on descent" );

	SetupSwitchEffectTypes(oldPed, newPed);

	// stop any active hud effect here to minimise the delay caused if done by script
	ANIMPOSTFXMGR.Stop(ms_switchFxName[SWITCHFX_HUD],AnimPostFXManager::kLongPlayerSwitch);
	ANIMPOSTFXMGR.Stop(ms_switchFxName[SWITCHFX_HUD_MICHAEL],AnimPostFXManager::kLongPlayerSwitch);
	ANIMPOSTFXMGR.Stop(ms_switchFxName[SWITCHFX_HUD_FRANKLIN],AnimPostFXManager::kLongPlayerSwitch);
	ANIMPOSTFXMGR.Stop(ms_switchFxName[SWITCHFX_HUD_TREVOR],AnimPostFXManager::kLongPlayerSwitch);

	if ((params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SUPPRESS_INTRO_FX) == 0)
	{
		// trigger initial switch effect (this one needs to loop and will be cut off on the first ascent cut)
		StartMainSwitchEffect(m_srcPedSwitchFxType, true, 0);
	}

	// init streaming helper
	m_streamer.Init( m_vDestPos, params, oldPed.GetInteriorLocation().IsValid() );

	// init switch cam
	camInterface::GetSwitchDirector().InitSwitch(0, oldPed, newPed, params);

	if ((params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_DESCENT_ONLY) != 0)
	{
		SetState( CPlayerSwitchMgrLong_State(STATE_PREP_DESCENT, 0) );
	}
	else
	{
		SetState( CPlayerSwitchMgrLong_State(STATE_INTRO, 0) );
	}

	CVehiclePopulation::LockPopulationRangeScale();

	// init audio
	audNorthAudioEngine::StartLongPlayerSwitch(oldPed, newPed);

	m_bOutroSceneOverridden	= false;
	m_pEstablishingShotData = NULL;

	// for shorter range medium switches, we try to keep the old map data in memory for reasons of persistence
	// (e.g. keeping damaged entities damaged and so on)
	m_bKeepStartSceneMapData = false;
	if (params.m_switchType==CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM && (params.m_controlFlags&CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST)==0)
	{
		const ScalarV dist2d = DistXYFast(m_vStartPos, m_vDestPos);
		m_bKeepStartSceneMapData = ( dist2d.Getf() < SHORT_RANGE_DIST );
	}

#if __ASSERT
	s_streamedOutroSphere = false;
#endif	//__ASSERT

	// set up our cargen info
	CTheCarGenerators::m_CarGenAreaOfInterestPosition = VEC3V_TO_VECTOR3(m_vDestPos);
	CTheCarGenerators::m_CarGenAreaOfInterestRadius = 50.f;

	// set on-screen spawn distances
	if(params.m_switchType==CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM)
	{
		m_onScreenSpawnDistance = 1.f; // don't care about spawning in view
	}
	else
	{
		// long switch
		m_onScreenSpawnDistance = 400.f; // we don't want to spawn cars in view
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Stop
// PURPOSE:		shut down the player switch and release any resources etc
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::StopInternal(bool bHard)
{
	STRVIS_SET_MARKER_TEXT("Player switch - STOP");

	CMiniMap::LockMiniMapPosition(Vector2(-9999.0f, -9999.0f));
	CMiniMap::SetVisible(true);

	// switch off all the things that definitely need to be switched off after a switch
	camInterface::GetSwitchDirector().ShutdownSwitch();
	m_streamer.StopStreaming(true);
	g_LodMgr.StopSlodMode();

#if ENABLE_DISTANT_CARS
	g_distantLights.EnableDistantCars(true);
#endif	//ENABLE_DISTANT_CARS

	SetSwitchShadows(false);
	CFocusEntityMgr::GetMgr().ClearPopPosAndVel();
	//@@: location CPLAYERSWITCHMGRLONG_STOPINTERNAL_SET_CAR_GENERATORS
	CTheCarGenerators::m_CarGenAreaOfInterestRadius = 0.f;	

	// stop the effect only if forced to or if the outro effect triggered was
	// SWITCHFX_NEUTRAL (this one doesn't fade out)
	if (bHard || ms_currentSwitchFxId == SWITCHFX_NEUTRAL || ms_currentSwitchFxId == SWITCHFX_NEUTRAL_CNC)
	{
		StopMainSwitchEffect();
	}
	ANIMPOSTFXMGR.Stop(ms_switchFxName[m_srcPedMidSwitchFxType],AnimPostFXManager::kLongPlayerSwitch);
	ANIMPOSTFXMGR.Stop(ms_switchFxName[m_dstPedMidSwitchFxType],AnimPostFXManager::kLongPlayerSwitch);

	ThePaths.ResetPlayerSwitchTarget();

	CVehiclePopulation::UnlockPopulationRangeScale();

	// Make sure we don't leave this on.
	CPedPopulation::SetUseStartupModeDueToWarping(false);

	audNorthAudioEngine::StopLongPlayerSwitch();

	CStreaming::ClearFlagForAll(STRFLAG_LOADSCENE);

	Lights::StartPedLightFadeUp(4.0f);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		update progress on active player switch
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::UpdateInternal()
{
	Assertf( !gStreamingRequestList.IsActive() || gStreamingRequestList.GetPrestreamMode() == SRL_PRESTREAM_FORCE_OFF || gStreamingRequestList.GetPrestreamMode() == SRL_PRESTREAM_FORCE_COMPLETELY_OFF, "Streaming request list with prestreaming active while a player switch is in progress! Level design bug" );

	camSwitchCamera* pSwitchCamera = camInterface::GetSwitchDirector().GetSwitchCamera();
#if __ASSERT
	if(!pSwitchCamera)
	{
		camBaseCamera *pActiveCamera = camInterface::GetSwitchDirector().GetActiveCamera();
		Assertf(pSwitchCamera, "pSwitchCamera is NULL! ActiveCamera: %p %s %s",
			pActiveCamera,
			pActiveCamera ? pActiveCamera->GetClassId().GetCStr() : "(unknown class id)",
			pActiveCamera ? pActiveCamera->GetMetadata().m_Name.GetCStr() : "(unknown name)");
	}
#endif // __ASSERT

	const CPlayerSwitchMgrLong_State nextState = GetNextState( m_currentState );

	const bool	panActive				= ( m_currentState.GetState()==STATE_PAN );
	const bool	jumpCutActive			= (m_currentState.GetState()==STATE_JUMPCUT_ASCENT || m_currentState.GetState()==STATE_JUMPCUT_DESCENT);
	const u32	timeSinceStateChange	= (fwTimer::GetTimeInMilliseconds_NonScaledClipped() - m_timeOfLastStateChange);
	const bool	bStreamingTimeout		= ( m_streamer.CanTimeout() && timeSinceStateChange>=m_streamer.GetTimeout() );
	const bool	bCameraReady			= ( pSwitchCamera->HasBehaviourFinished() );
	const bool	bStreamingReady			= ( bStreamingTimeout || !m_streamer.IsStreaming() || m_streamer.IsFinishedStreaming() || panActive);
	const bool	bJumpMinTimeMet			= (!jumpCutActive || timeSinceStateChange>=JUMPCUT_MIN_DURATION);
	const bool  bPedPopReady			= (m_currentState.GetState() != STATE_JUMPCUT_DESCENT || m_currentState.GetJumpCutIndex() != 0 || CPedPopulation::HasForcePopulationInitFinished());
	const bool	bVehiclePopReady		= (m_currentState.GetState() != STATE_JUMPCUT_DESCENT || m_currentState.GetJumpCutIndex() == 0 || CVehiclePopulation::HasInstantFillPopulationFinished()); // don't hold on too long for vehicles
	const bool  bScenarioPedsStreamedIn = (m_currentState.GetState() != STATE_JUMPCUT_DESCENT || m_currentState.GetJumpCutIndex() != 0 || timeSinceStateChange >= m_params.m_scenarioAnimsTimeout || !CTaskUseScenario::AreAnyPedsInAreaStreamingInScenarioAnims(Vec3V(CFocusEntityMgr::GetMgr().GetPopPos()), ScalarV(m_params.m_fRadiusOfStreamedScenarioPedCheck)));
    const bool  bVehicleMods            = (m_currentState.GetState() != STATE_JUMPCUT_DESCENT || m_currentState.GetJumpCutIndex() != 0 || CVehiclePopulation::HasInterestingVehicleStreamedIn());
	const bool  bPopReady				= (bStreamingTimeout || (bVehiclePopReady && bPedPopReady && bScenarioPedsStreamedIn && bVehicleMods));
	const bool	bCanAdvance				= ( bCameraReady && bStreamingReady && ClearToProceed(nextState) && bJumpMinTimeMet && bPopReady);
#if !__FINAL
	if ( bStreamingTimeout && bCanAdvance )
	{
		Warningf("Player switch: took too long to stream next scene, but advancing anyway");
	}

	if (PARAM_debugPlayerSwitch.Get())
	{
		Displayf("[Playerswitch] UpdateInternal #################################################");
		Displayf("[Playerswitch] UpdateInternal currentState            : jump %d : state %d : %s", m_currentState.GetJumpCutIndex(), m_currentState.GetState(), stateToString((eState)m_currentState.GetState()));
		Displayf("[Playerswitch] UpdateInternal nextState               : jump %d : state %d : %s", nextState.GetJumpCutIndex(), nextState.GetState(), stateToString((eState)nextState.GetState()));
		Displayf("[Playerswitch] UpdateInternal controlFlags            : %s : 0x%x", controlFlagsToString(m_params.m_controlFlags), m_params.m_controlFlags);
		Displayf("[Playerswitch] UpdateInternal panActive               : %s", panActive ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal jumpCutActive           : %s", jumpCutActive ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal timeSinceStateChange    : %d", timeSinceStateChange);
		Displayf("[Playerswitch] UpdateInternal ## - bPopReady = ");
		Displayf("[Playerswitch] UpdateInternal bStreamingTimeout       : %s", bStreamingTimeout ? "TRUE" : "FALSE");	
		Displayf("[Playerswitch] UpdateInternal ## || ");
		Displayf("[Playerswitch] UpdateInternal bVehiclePopReady        : %s", bVehiclePopReady ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal bPedPopReady            : %s", bPedPopReady ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal bScenarioPedsStreamedIn : %s", bScenarioPedsStreamedIn ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal bVehicleMods            : %s", bVehicleMods ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal ## - bCanAdvance = ");
		Displayf("[Playerswitch] UpdateInternal bCameraReady            : %s", bCameraReady ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal bStreamingReady         : %s", bStreamingReady ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal ClearToProceed          : %s", ClearToProceed(nextState) ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal bJumpMinTimeMet         : %s", bJumpMinTimeMet ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal bPopReady               : %s", bPopReady ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal ####");
		Displayf("[Playerswitch] UpdateInternal bCanAdvance             : %s", bCanAdvance ? "TRUE" : "FALSE");
		Displayf("[Playerswitch] UpdateInternal #################################################");
	}
#endif

	// tunnel activation special case
	if ( m_currentState.GetState()>STATE_PAN && g_LoadScene.IsActive()
		&& g_LoadScene.IsSafeToQueryInteriorProxy() && g_LoadScene.GetInteriorProxy()!=NULL
		&& g_LoadScene.GetInteriorProxy()->GetGroupId() && m_newPed && m_newPed->GetPortalTracker() &&
		g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(m_newPed->GetTransform().GetPosition(), fwBoxStreamerAsset::ASSET_STATICBOUNDS_MOVER) )
	{
		m_newPed->GetPortalTracker()->SetProbeType(CPortalTracker::PROBE_TYPE_FAR);
		m_newPed->GetPortalTracker()->RequestRescanNextUpdate();
	}

	switch (m_currentState.GetState())
	{
	case STATE_PAN:
		{
			//////////////////////////////////////////////////////////////////////////
			// activate secondary streamer a frame late, so that old entities have been kicked out by that point
			if (!CStreamVolumeMgr::IsVolumeActive(CStreamVolumeMgr::VOLUME_SLOT_SECONDARY))
			{
				Vec3V vSecondaryStreamPos = ( m_bOutroSceneOverridden ? m_overriddenOutroScene.GetPos() : m_vDestPos );
				m_streamer.StartStreamingSecondary(vSecondaryStreamPos);
			}
			//////////////////////////////////////////////////////////////////////////

			const fwBoxStreamerAssetFlags panMapDataAssetMask = (fwBoxStreamerAssetFlags) g_LodMgr.GetMapDataAssetMaskForLodLevel(m_params.m_ceilingLodLevel);
			g_StaticBoundsStore.GetBoxStreamer().OverrideRequiredAssets(0);	// don't bother requesting collision during pan
			g_MapDataStore.GetBoxStreamer().OverrideRequiredAssets(panMapDataAssetMask);	// only need low-lod imap files during pan
			break;
		}
	case STATE_JUMPCUT_DESCENT:
	case STATE_OUTRO_HOLD:
		{
			// Forcibly converting to real -- heavy handed and effective
			// But **EXPENSIVE**
			spdSphere sp = spdSphere(VECTOR3_TO_VEC3V(CFocusEntityMgr::GetMgr().GetPopPos()), ScalarV(m_params.m_fRadiusOfStreamedScenarioPedCheck));
			CObjectPopulation::SetScriptForceConvertVolumeThisFrame(sp);

			// Calling this in SetState() is too soon -- the overriden population center hasn't been processed yet
			CVehiclePopulation::UnlockPopulationRangeScale();

			break;
		}
	case STATE_OUTRO_SWOOP:
	case STATE_FINISHED:
		{
			break;
		}

	default:
		break;
	}

	if ( bCanAdvance )
	{

#if __ASSERT
		if (m_currentState.GetState()==STATE_JUMPCUT_DESCENT && m_currentState.GetJumpCutIndex()==0 && m_params.m_switchType==CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM)
		{
			Assert(m_streamer.IsStreaming());
			Assert(m_streamer.IsFinishedStreaming() || bStreamingTimeout);
		}
#endif

		SetState( nextState );
	}

	if (g_LodMgr.IsSlodModeActive())
	{
		CLODLightManager::SetUpdateLODLightsDisabledThisFrame();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetState
// PURPOSE:		state changes for player switch
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::SetState(const CPlayerSwitchMgrLong_State& newState)
{
	Displayf("[Playerswitch] SetState : jump %d : state %d", newState.GetJumpCutIndex(), newState.GetState());

	m_timeOfLastStateChange = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	camSwitchCamera* pSwitchCamera = camInterface::GetSwitchDirector().GetSwitchCamera();
#if __ASSERT
	if(!pSwitchCamera)
	{
		camBaseCamera *pActiveCamera = camInterface::GetSwitchDirector().GetActiveCamera();
		Assertf(pSwitchCamera, "pSwitchCamera is NULL! ActiveCamera: %p %s %s",
			pActiveCamera,
			pActiveCamera ? pActiveCamera->GetClassId().GetCStr() : "(unknown class id)",
			pActiveCamera ? pActiveCamera->GetMetadata().m_Name.GetCStr() : "(unknown name)");
	}
#endif // __ASSERT

	CPlayerSwitchMgrLong_State previousState = m_currentState;
	CPlayerSwitchMgrLong_State nextState = GetNextState( newState );

	m_currentState = newState;

	switch (m_currentState.GetState())
	{
		//////////////////////////////////////////////////////////////////////////
		// intro camera sequence
	case STATE_INTRO:
		{
			STRVIS_SET_MARKER_TEXT("Player switch INTRO");
			g_FrontendAudioEntity.TriggerLongSwitchSkyLoop(GetArcadeTeamFromEffectType(m_srcPedSwitchFxType));
			g_FrontendAudioEntity.TriggerLongSwitchSound(atHashWithStringBank("In", 0x4D5FA44B), GetArcadeTeamFromEffectType(m_srcPedSwitchFxType));
			pSwitchCamera->PerformIntro();
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// prep scene for descent-only switch
	case STATE_PREP_DESCENT:
		{
			STRVIS_SET_MARKER_TEXT("Player switch PREP DESCENT");
			m_streamer.StartStreaming(nextState);
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// hold while streaming first jump cut location
	case STATE_PREP_FOR_CUT:
		{
			STRVIS_SET_MARKER_TEXT("Player switch PREP FOR CUT");
			if (m_bOutroSceneOverridden)
			{
				Matrix34 worldMatrix = MAT34V_TO_MATRIX34(m_overriddenOutroScene.GetWorldMatrix());
				pSwitchCamera->OverrideDestination(worldMatrix);
			}
			if (m_pEstablishingShotData)
			{
				pSwitchCamera->SetEstablishingShotMetadata(m_pEstablishingShotData);
			}
			m_streamer.StartStreaming(nextState);
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// show flash, play sound effect, move camera, start streaming next location
	case STATE_JUMPCUT_ASCENT:
		{
			STRVIS_SET_MARKER_TEXT("Player switch JUMPCUT ASCENT");
			if (m_currentState.GetJumpCutIndex() == 0)
			{
				CMiniMap::SetVisible(false);
				CMiniMap::LockMiniMapPosition( Vector2(m_vDestPos.GetXf(), m_vDestPos.GetYf()) );

				SetSwitchShadows(true);

				// Ugly hack for FIB5 mission custom camera/player switch mix (bugstar:2004463).
				const atHashString fxNameFIB5("SwitchOpenNeutralFIB5",	0x4a4777e7);
				ANIMPOSTFXMGR.Stop(fxNameFIB5,AnimPostFXManager::kLongPlayerSwitch);
			}

			if (previousState.GetState() != STATE_WAITFORINPUT_OUTRO)
			{
				g_FrontendAudioEntity.TriggerLongSwitchSound(atHashWithStringBank("Hit_1", 0x576D444D), GetArcadeTeamFromEffectType(m_srcPedSwitchFxType));
				pSwitchCamera->PerformAscentCut(m_currentState.GetJumpCutIndex());

				int delayFrame = 0;
				if( (m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_DELAY_ASCENT_FX) != 0 )
				{
					delayFrame = 3;
				}

				StartMainSwitchEffect(GetNeutralFX(), true, delayFrame);

				if( m_params.m_numJumpsAscent > 1 )
				{
					ANIMPOSTFXMGR.Start(ms_switchFxName[m_srcPedMidSwitchFxType], 0U, true, true, true, delayFrame, AnimPostFXManager::kLongPlayerSwitch);
					float midSwithFxLevel = 1.0f-Saturate((float)(m_currentState.GetJumpCutIndex())/(float)Max((s32)m_params.m_numJumpsAscent-1,1));
					ANIMPOSTFXMGR.SetUserFadeLevel(ms_switchFxName[m_srcPedMidSwitchFxType], midSwithFxLevel);
				}
			}

			if (m_currentState.GetJumpCutIndex() == m_params.m_numJumpsAscent-1)
			{
				if ((m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SKIP_PAN)==0)
				{
					// when cutting to ceiling, switch to low-lod mode immediately for pan (if not being skipped)
					g_LodMgr.StartSlodMode(1 << m_params.m_ceilingLodLevel, CLodMgr::LODFILTER_MODE_PLAYERSWITCH);

#if ENABLE_DISTANT_CARS
					g_distantLights.EnableDistantCars(false);
#endif	//ENABLE_DISTANT_CARS
				}
			}

			// stream for either next jump cut or pan
			if (nextState.GetState() != STATE_WAITFORINPUT_INTRO)
			{
				m_streamer.StartStreaming(nextState);
			}
		}
		break;


		//////////////////////////////////////////////////////////////////////////
		// look up to the sky
	case STATE_WAITFORINPUT_INTRO:
		{
			STRVIS_SET_MARKER_TEXT("Player switch WAIT INTRO");
		//	pSwitchCamera->PerformLookUp();
			if ((s32)m_params.m_numJumpsAscent == 1)
			{
				ANIMPOSTFXMGR.Stop(ms_switchFxName[m_srcPedMidSwitchFxType],AnimPostFXManager::kLongPlayerSwitch);
				ANIMPOSTFXMGR.Stop(ms_switchFxName[m_dstPedMidSwitchFxType],AnimPostFXManager::kLongPlayerSwitch);
			}
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// hold camera and wait for user to select player ped
	case STATE_WAITFORINPUT:
		{
			m_streamer.StopStreaming(true);
			STRVIS_SET_MARKER_TEXT("Player switch WAIT INPUT");
		//	ms_vignetteEffect.StartFadeDown();
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// look back down to the ground
	case STATE_WAITFORINPUT_OUTRO:
		{
			STRVIS_SET_MARKER_TEXT("Player switch WAIT OUTRO");
		//	ms_vignetteEffect.StartFadeUp();
		//	pSwitchCamera->PerformLookDown();
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// pan camera quickly, showing only low lods
	case STATE_PAN:
		{
			STRVIS_SET_MARKER_TEXT("Player switch PAN");
	
			ANIMPOSTFXMGR.Stop(ms_switchFxName[m_srcPedMidSwitchFxType],AnimPostFXManager::kLongPlayerSwitch);
			ANIMPOSTFXMGR.Stop(ms_switchFxName[m_dstPedMidSwitchFxType],AnimPostFXManager::kLongPlayerSwitch);

			// We want to trigger the src move sound but then transition to the dst loop sound (if not the same as the src)
			g_FrontendAudioEntity.TriggerLongSwitchSound(atHashWithStringBank("Camera_Move", 0x3DEAEC2D), GetArcadeTeamFromEffectType(m_srcPedSwitchFxType));

			if (m_srcPedSwitchFxType != m_dstPedSwitchFxType)
			{
				g_FrontendAudioEntity.StopLongSwitchSkyLoop();
				g_FrontendAudioEntity.TriggerLongSwitchSkyLoop(GetArcadeTeamFromEffectType(m_dstPedSwitchFxType));
			}

			pSwitchCamera->PerformPan( CalcPanDuration() );

			// start requesting road nodes at the destination only, now it is known
			ThePaths.SetPlayerSwitchTarget(VEC3V_TO_VECTOR3(m_vDestPos));
			ThePaths.UpdateStreaming(true, false);

			// start requesting vehicles and peds at the destination, now it is known
			// only flush if we are in SP - in MP there might be other players in the area - allow the vehicles to remain - they will just transfer ownership to the other player and will disappear from our scope anyway. lavalley.
			if (!NetworkInterface::IsGameInProgress())
			{
				gPopStreaming.FlushAllVehicleModelsSoft();
			}
			CFocusEntityMgr::GetMgr().SetPopPosAndVel(VEC3V_TO_VECTOR3(m_vDestPos), Vector3(Vector3::ZeroType));
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// show flash, play sound effect, move camera, start streaming next location
	case STATE_JUMPCUT_DESCENT:
		{
			STRVIS_SET_MARKER_TEXT("Player switch JUMPCUT DESCENT");

			if (m_currentState.GetJumpCutIndex() == 0)
			{
				CPlantMgr::PreUpdateOnceForNewCameraPos( VEC3V_TO_VECTOR3(m_vDestPos) );

				CPedPopulation::InstantFillPopulation();	
				CPedPopulation::ForcePopulationInit(m_params.m_maxTimeToStayInStartupModeOnLongRangeDescent);

				// Make sure the ped population stays in startup mode until further notice.
				CPedPopulation::SetUseStartupModeDueToWarping(true);
				
				CTheCarGenerators::m_CarGenAreaOfInterestRadius = 0.f;
			}
			
			if(m_currentState.GetJumpCutIndex() != 0 || m_params.m_switchType == CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM)
			{
				CVehiclePopulation::InstantFillPopulation(); // keep instant filling to pump up the vehicle pop
			}

			pSwitchCamera->PerformDescentCut(m_currentState.GetJumpCutIndex());

			if (m_currentState.GetJumpCutIndex() != m_params.m_numJumpsDescent-1)
			{
				g_FrontendAudioEntity.TriggerLongSwitchSound(atHashWithStringBank("Hit_2", 0x65B7E0E2), GetArcadeTeamFromEffectType(m_dstPedSwitchFxType));

				StartMainSwitchEffect(GetNeutralFX(), true, 0);

				ANIMPOSTFXMGR.Stop(ms_switchFxName[m_srcPedMidSwitchFxType],AnimPostFXManager::kLongPlayerSwitch);
				ANIMPOSTFXMGR.Start(ms_switchFxName[m_dstPedMidSwitchFxType], 0U, true, true, true, 0U, AnimPostFXManager::kLongPlayerSwitch);
				float midSwithFxLevel = 1.0f-Saturate((float)(m_currentState.GetJumpCutIndex())/(float)Max((s32)m_params.m_numJumpsDescent-1,1));
				ANIMPOSTFXMGR.SetUserFadeLevel(ms_switchFxName[m_dstPedMidSwitchFxType], midSwithFxLevel);


				g_LodMgr.StopSlodMode();	// first jump cut down from ceiling, so stop slod mode

#if ENABLE_DISTANT_CARS
				g_distantLights.EnableDistantCars(true);
#endif	//ENABLE_DISTANT_CARS
			}

			// stream for either next jump cut, establishing shot or outro
			m_streamer.StartStreaming(nextState);

			// secondary streamer to speed things up a bit
			//if (m_currentState.GetJumpCutIndex() == m_params.m_numJumpsDescent-1)
#if 0
			if (previousState.GetState() == STATE_PAN)
			{
				if ( ((m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST)==0)
					&& (m_params.m_switchType==CPlayerSwitchInterface::SWITCH_TYPE_LONG) )
				{
					Vec3V vSecondaryStreamPos = ( m_bOutroSceneOverridden ? m_overriddenOutroScene.GetPos() : m_vDestPos );
					m_streamer.StartStreamingSecondary(vSecondaryStreamPos);
				}
			}
#endif

			CFocusEntityMgr::GetMgr().SetPopPosAndVel(VEC3V_TO_VECTOR3(m_vDestPos), Vector3(Vector3::ZeroType));
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// optional establishing shot occurs before outro
	case STATE_ESTABLISHING_SHOT:
		{
			STRVIS_SET_MARKER_TEXT("Player switch ESTABLISHING SHOT");

			g_LodMgr.StopSlodMode();	// this might still be on, if running with just one jump cut

#if ENABLE_DISTANT_CARS
			g_distantLights.EnableDistantCars(true);
#endif	//ENABLE_DISTANT_CARS

			SetSwitchShadows(false);

			m_streamer.StopStreaming(true);

			g_FrontendAudioEntity.TriggerLongSwitchSound(atHashWithStringBank("Hit_2", 0x65B7E0E2), GetArcadeTeamFromEffectType(m_dstPedSwitchFxType));
			g_FrontendAudioEntity.TriggerLongSwitchSound(atHashWithStringBank("Out", 0x12067E90), GetArcadeTeamFromEffectType(m_dstPedSwitchFxType));

			CPlantMgr::PreUpdateOnceForNewCameraPos( VEC3V_TO_VECTOR3(m_vDestPos) );

			StartMainSwitchEffectOutro();

			pSwitchCamera->PerformEstablishingShot();
			
			CTheCarGenerators::m_CarGenAreaOfInterestRadius = 0.f;

			// streaming for outro
			m_streamer.StartStreaming(nextState);
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// outro hold camera sequence
	case STATE_OUTRO_HOLD:
		{			
			STRVIS_SET_MARKER_TEXT("Player switch OUTRO HOLD");

			g_LodMgr.StopSlodMode();	// this might still be on, if running with just one jump cut

#if ENABLE_DISTANT_CARS
			g_distantLights.EnableDistantCars(true);
#endif	//ENABLE_DISTANT_CARS

			SetSwitchShadows(false);

			CPlantMgr::PreUpdateOnceForNewCameraPos( VEC3V_TO_VECTOR3(m_vDestPos) );

			m_streamer.StopStreaming(true);

			if(!m_pEstablishingShotData)
			{
				g_FrontendAudioEntity.TriggerLongSwitchSound(atHashWithStringBank("Hit_2", 0x65B7E0E2), GetArcadeTeamFromEffectType(m_dstPedSwitchFxType));
				StartMainSwitchEffectOutro();
			}

			pSwitchCamera->PerformOutroHold();
			CTheCarGenerators::m_CarGenAreaOfInterestRadius = 0.f;

			// No longer forcing the population init this late or in the later states
			// Forcing the objects to real in the area was the true
			// root of the issue -- See UpdateInternal
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// outro swoop camera sequence
	case STATE_OUTRO_SWOOP:
		{		
			STRVIS_SET_MARKER_TEXT("Player switch OUTRO SWOOP");
			m_streamer.StopStreaming(true);

			if(!m_pEstablishingShotData)
			{
				g_FrontendAudioEntity.TriggerLongSwitchSound(atHashWithStringBank("Out", 0x12067E90), GetArcadeTeamFromEffectType(m_dstPedSwitchFxType));
				g_FrontendAudioEntity.StopLongSwitchSkyLoop();
			}

			pSwitchCamera->PerformOutroSwoop();
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// finished switching behaviour
	case STATE_FINISHED:
		{
			STRVIS_SET_MARKER_TEXT("Player switch FINISHED");
#if __ASSERT
			if (s_streamedOutroSphere)
			{
				CPed* pPed = CGameWorld::FindLocalPlayer();
				if (pPed)
				{
					const Vec3V vPedPos = pPed->GetTransform().GetPosition();
					const ScalarV dist3d = Dist(vPedPos, m_vDestPos);
					const bool bPlayerHasMoved = IsGreaterThanAll(dist3d, ScalarV( 20.0f )) != 0;
					Assertf( !bPlayerHasMoved,
						"Player %s is %.2f metres from the end point (%.2f, %.2f, %.2f) of player switch! This can cause streaming issues",
						pPed->GetModelName(),
						dist3d.Getf(),
						m_vDestPos.GetXf(), m_vDestPos.GetYf(), m_vDestPos.GetZf()
					);
				}
			}
#endif
			Stop(false);
		}
		break;

	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CalcPanDuration
// PURPOSE:		using the pan start and end positions, and the pan
//				speed, determine the duration to be passed to camera code in ms
//////////////////////////////////////////////////////////////////////////
u32 CPlayerSwitchMgrLong::CalcPanDuration()
{
	camSwitchCamera* pSwitchCamera = camInterface::GetSwitchDirector().GetSwitchCamera();
#if __ASSERT
	if(!pSwitchCamera)
	{
		camBaseCamera *pActiveCamera = camInterface::GetSwitchDirector().GetActiveCamera();
		Assertf(pSwitchCamera, "pSwitchCamera is NULL! ActiveCamera: %p %s %s",
			pActiveCamera,
			pActiveCamera ? pActiveCamera->GetClassId().GetCStr() : "(unknown class id)",
			pActiveCamera ? pActiveCamera->GetMetadata().m_Name.GetCStr() : "(unknown name)");
	}
#endif // __ASSERT

	camFrame cameraFramePanStart;
	camFrame cameraFramePanEnd;
	pSwitchCamera->ComputeFrameForAscentCut( m_params.m_numJumpsAscent-1, cameraFramePanStart );
	pSwitchCamera->ComputeFrameForDescentCut( m_params.m_numJumpsDescent-1, cameraFramePanEnd );

	const float fPanSpeedMetresPerSecond = (float) m_params.m_fPanSpeed;
	const ScalarV distInMetres = Dist( VECTOR3_TO_VEC3V(cameraFramePanStart.GetPosition()), VECTOR3_TO_VEC3V(cameraFramePanEnd.GetPosition()) );
	float fPanDurationInSeconds = ( distInMetres.Getf() / fPanSpeedMetresPerSecond );

	if(NetworkInterface::IsGameInProgress())
	{
		//It's too late to modify the pan behaviour in single-player, but enforcing a minimum pan duration in multi-player should reduce the scope for a
		//perceived jump cut when the distance between the ascent and descent columns is short.
		fPanDurationInSeconds = Max(fPanDurationInSeconds, g_MinPanDurationInSecondsForMp);
	}

	return (u32) ( fPanDurationInSeconds * 1000.0f );	// TODO can camera code tolerate a 0ms pan?
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetNextState
// PURPOSE:		get the next state in the execution sequence
//////////////////////////////////////////////////////////////////////////
CPlayerSwitchMgrLong_State CPlayerSwitchMgrLong::GetNextState(const CPlayerSwitchMgrLong_State& currentState)
{
	CPlayerSwitchMgrLong_State nextState;

	switch (currentState.GetState())
	{
	case STATE_INTRO:
		nextState.SetJumpCutIndex(0);
		nextState.SetState(STATE_PREP_FOR_CUT);
		break;

	case STATE_PREP_DESCENT:
		nextState.SetJumpCutIndex(m_params.m_numJumpsDescent-1);
		nextState.SetState(STATE_JUMPCUT_DESCENT);
		break;

	case STATE_PREP_FOR_CUT:
		nextState.SetJumpCutIndex(0);
		nextState.SetState(STATE_JUMPCUT_ASCENT);
		break;

	case STATE_JUMPCUT_ASCENT:
		if (currentState.GetJumpCutIndex() == m_params.m_numJumpsAscent-1)
		{
			if ((m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST)!=0)
			{
				nextState.SetJumpCutIndex(m_params.m_numJumpsAscent-1);
				nextState.SetState(STATE_WAITFORINPUT_INTRO);
			}
			else if ((m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SKIP_PAN)!=0)
			{
				if ( ((m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SKIP_TOP_DESCENT)!=0) )
				{
					// skip to the state AFTER the top-most descending shot
					CPlayerSwitchMgrLong_State tmpState(STATE_JUMPCUT_DESCENT, m_params.m_numJumpsDescent-1);
					nextState = GetNextState(tmpState);
				}
				else
				{
					nextState.SetJumpCutIndex(m_params.m_numJumpsDescent-1);
					nextState.SetState(STATE_JUMPCUT_DESCENT);
				}

			}
			else
			{
				nextState.SetJumpCutIndex(m_params.m_numJumpsAscent-1);
				nextState.SetState(STATE_PAN);
			}
		}
		else
		{
			nextState.SetJumpCutIndex(currentState.GetJumpCutIndex()+1);
			nextState.SetState(STATE_JUMPCUT_ASCENT);
		}
		break;

	case STATE_WAITFORINPUT_INTRO:
		nextState.SetJumpCutIndex(m_params.m_numJumpsAscent-1);
		nextState.SetState(STATE_WAITFORINPUT);
		break;

	case STATE_WAITFORINPUT:
		nextState.SetJumpCutIndex(m_params.m_numJumpsAscent-1);
		nextState.SetState(STATE_WAITFORINPUT_OUTRO);
		break;

	case STATE_WAITFORINPUT_OUTRO:
		nextState.SetJumpCutIndex(m_params.m_numJumpsAscent-1);
		nextState.SetState(STATE_JUMPCUT_ASCENT);
		break;

	case STATE_PAN:
		nextState.SetJumpCutIndex(m_params.m_numJumpsDescent-1);
		nextState.SetState(STATE_JUMPCUT_DESCENT);
		break;

	case STATE_JUMPCUT_DESCENT:
		if (currentState.GetJumpCutIndex() == 0)
		{
			if (m_pEstablishingShotData)
			{
				nextState.SetJumpCutIndex(0);
				nextState.SetState(STATE_ESTABLISHING_SHOT);
			}
			else
			{
				nextState.SetJumpCutIndex(0);
				nextState.SetState(STATE_OUTRO_HOLD);
			}
		}
		else
		{
			nextState.SetJumpCutIndex(currentState.GetJumpCutIndex()-1);
			nextState.SetState(STATE_JUMPCUT_DESCENT);
		}
		break;

	case STATE_ESTABLISHING_SHOT:
		nextState.SetJumpCutIndex(0);
		nextState.SetState(STATE_OUTRO_HOLD);
		break;

	case STATE_OUTRO_HOLD:
		nextState.SetJumpCutIndex(0);
		nextState.SetState(STATE_OUTRO_SWOOP);
		break;

	case STATE_OUTRO_SWOOP:
		nextState.SetJumpCutIndex(0);
		nextState.SetState(STATE_FINISHED);
		break;

	case STATE_FINISHED:
		nextState.SetJumpCutIndex(0);
		nextState.SetState(STATE_FINISHED);
		break;

	default:
		break;
	}

	return nextState;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ClearToProceed
// PURPOSE:		returns true if it is OK to advance to the next state based on control flags
//////////////////////////////////////////////////////////////////////////
bool CPlayerSwitchMgrLong::ClearToProceed(const CPlayerSwitchMgrLong_State& nextState)
{
	switch (m_currentState.GetState())
	{
	case STATE_WAITFORINPUT:
		return ((m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST) == 0);

	default:
		break;
	}

	switch (nextState.GetState())
	{
	case STATE_JUMPCUT_ASCENT:
		return ((m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_ASCENT) == 0);

	case STATE_JUMPCUT_DESCENT:
		return ((m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_DESCENT) == 0);

	case STATE_PAN:
		return ((m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_MASK_PANDISABLED) == 0);

	case STATE_OUTRO_HOLD:
		return ((m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_OUTRO) == 0);

	default:
		break;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetSwitchShadows
// PURPOSE:		enable or disable switch specific shadow behaviour
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::SetSwitchShadows(bool bEnabled)
{
	if (bEnabled)
	{
		CRenderPhaseCascadeShadowsInterface::Script_SetShadowType(CSM_TYPE_QUADRANT);
#if PLAYERSWITCH_FORCES_LIGHTDIR
		g_timeCycle.SetDirLightOverride(lightPositionOverride);
		g_timeCycle.SetDirLightOverrideStrength(1.0f);
#endif // PLAYERSWITCH_FORCES_LIGHTDIR		

	}
	else
	{
		// restore normal behaviour
		CRenderPhaseCascadeShadowsInterface::Script_SetShadowType(CSM_TYPE_CASCADES);
#if PLAYERSWITCH_FORCES_LIGHTDIR
		g_timeCycle.SetDirLightOverrideStrength(0.0f);
#endif // PLAYERSWITCH_FORCES_LIGHTDIR		
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddSearches
// PURPOSE:		request collision for active switch etc
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::AddSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags)
{
	if (!IsActive())
		return;

	// static bounds store
	if ( (supportedAssetFlags&fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER)!=0 )
	{
		// don't start requesting collision at the end location until the ascent has completed
		if (GetState() >= STATE_PAN)
		{
			searchList.Grow() = fwBoxStreamerSearch(
				m_vDestPos,
				fwBoxStreamerSearch::TYPE_SWITCH,
				fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER | fwBoxStreamerAsset::FLAG_STATICBOUNDS_MATERIAL,
				true
			);
		}
	}
	// map data store
	else if ( (supportedAssetFlags&fwBoxStreamerAsset::MASK_MAPDATA)!=0 )
	{
		if ( m_bKeepStartSceneMapData )
		{
			searchList.Grow() = fwBoxStreamerSearch(
				m_vStartPos,
				fwBoxStreamerSearch::TYPE_SWITCH,
				fwBoxStreamerAsset::MASK_MAPDATA,
				true
			);
		}

		//////////////////////////////////////////////////////////////////////////
		// get map data for end location in as early as possible for vehicle pop code
		if (GetState() > STATE_PAN)
		{
			const ScalarV range = ScalarV(50.0f);
			const Vec3V vFocusPos = VECTOR3_TO_VEC3V( CFocusEntityMgr::GetMgr().GetPos() );
			const ScalarV dist2d = DistXYFast(vFocusPos, m_vDestPos);

			// guard against script moving the ped in the final few frames of the switch
			if ( Verifyf( IsLessThanOrEqualAll(dist2d, range),
				"Player switch is unable to stream map data at the end point - has script repositioned the ped to a position very far from (%.2f, %.2f, %.2f)",
				m_vDestPos.GetXf(), m_vDestPos.GetYf(), m_vDestPos.GetZf() ) )
			{
				searchList.Grow() = fwBoxStreamerSearch(
					m_vDestPos,
					fwBoxStreamerSearch::TYPE_SWITCH,
					fwBoxStreamerAsset::MASK_MAPDATA,
					true
				);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		if (m_params.m_switchType == CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM)
		{
			Vec3V vFocusPos = VECTOR3_TO_VEC3V( CFocusEntityMgr::GetMgr().GetPos() );
			Vec3V vFocusPosGround = vFocusPos;
			vFocusPosGround.SetZf( 0.0f );
		
			const u8 assetFlags			= (1 << m_params.m_ceilingLodLevel);

			searchList.Grow() = fwBoxStreamerSearch(
				vFocusPos, vFocusPosGround, fwBoxStreamerSearch::TYPE_SWITCH, assetFlags, true
			);
		}
		//////////////////////////////////////////////////////////////////////////
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	OverrideOutroScene
// PURPOSE:		allows script to override outro frame (e.g. when going to a mocap camera)
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::OverrideOutroScene(Mat34V_In worldMatrix, float fCamFov, float fCamFarClip) 
{
#if __ASSERT
	const Vec3V vOriginal = m_vDestPos;
	const Vec3V vNew = worldMatrix.d();
	const ScalarV range(50.0f);
	const ScalarV dist = Dist(vOriginal, vNew);
	Displayf("PLAYER SWITCH OVERRIDE OUTRO to (%4.2f, %4.2f, %4.2f)", vNew.GetXf(), vNew.GetYf(), vNew.GetZf());
	Assertf ( IsLessThanOrEqualAll(dist, range), "Script is overriding switch pos from (%4.2f, %4.2f, %4.2f) to (%4.2f, %4.2f, %4.2f) which is a distance of %4.2fm. Probably an error",
		vOriginal.GetXf(), vOriginal.GetYf(), vOriginal.GetZf(), vNew.GetXf(), vNew.GetYf(), vNew.GetZf(), dist.Getf() );
#endif	//__ASSERT

	m_bOutroSceneOverridden = true;
	m_overriddenOutroScene.Init(worldMatrix, fCamFarClip, fCamFov);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetDest
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::SetDest(CPed& newPed, const CPlayerSwitchParams& params)
{
	camSwitchCamera* pSwitchCamera = camInterface::GetSwitchDirector().GetSwitchCamera();
#if __ASSERT
	if(!pSwitchCamera)
	{
		camBaseCamera *pActiveCamera = camInterface::GetSwitchDirector().GetActiveCamera();
		Assertf(pSwitchCamera, "pSwitchCamera is NULL! ActiveCamera: %p %s %s",
			pActiveCamera,
			pActiveCamera ? pActiveCamera->GetClassId().GetCStr() : "(unknown class id)",
			pActiveCamera ? pActiveCamera->GetMetadata().m_Name.GetCStr() : "(unknown name)");
	}
#endif // __ASSERT

	Assert(m_currentState.GetState() == STATE_WAITFORINPUT);

	m_vDestPos = newPed.GetTransform().GetPosition();
	m_newPed = &newPed;
	m_params = params;
	m_streamer.Init(m_vDestPos, params, false);
	SetControlFlag(CPlayerSwitchInterface::CONTROL_FLAG_UNKNOWN_DEST, false);	// destination no longer unknown, so clear the flag

	// update destination effect type
	UpdateDesSwitchEffectType(newPed);

	// update camera - control flags, descent floor height, descent num cuts, dest pos
	pSwitchCamera->SetDest(newPed, params);

	Displayf("[Playerswitch] SetDest to (%4.2f, %4.2f, %4.2f) : controlFlags %d", m_vDestPos.GetXf(), m_vDestPos.GetYf(), m_vDestPos.GetZf(), params.m_controlFlags);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetEstablishingShot
// PURPOSE:		retrieves meta data for the specified establishing shot
//				and tells the switch mgr to use make use of it if available
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::SetEstablishingShot(const atHashString shotName)
{
	m_pEstablishingShotData = CPlayerSwitchEstablishingShot::FindShotMetaData(shotName);

	camSwitchCamera* pSwitchCamera = camInterface::GetSwitchDirector().GetSwitchCamera();
	if (pSwitchCamera)
	{
		pSwitchCamera->SetEstablishingShotMetadata(m_pEstablishingShotData);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ModifyLodScale
// PURPOSE:		adjust lod scale in certain states
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::ModifyLodScale(float& fLodScale)
{
	switch (m_currentState.GetState())
	{
	case STATE_JUMPCUT_ASCENT:
	case STATE_JUMPCUT_DESCENT:
	case STATE_ESTABLISHING_SHOT:
		fLodScale = 1.0f * g_LodScale.GetRootScale();
		break;
	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ReadyForAscent
// PURPOSE:		returns true if switch is paused before ascent but ready to proceed
//////////////////////////////////////////////////////////////////////////
bool CPlayerSwitchMgrLong::ReadyForAscent()
{
	if ( IsActive() && m_currentState.GetState()==STATE_PREP_FOR_CUT
		&& (m_params.m_controlFlags&CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_ASCENT)!=0 )
	{
		const u32	timeSinceStateChange	= (fwTimer::GetTimeInMilliseconds_NonScaledClipped() - m_timeOfLastStateChange);
		const bool	bStreamingTimeout		= ( m_streamer.CanTimeout() && timeSinceStateChange>=m_streamer.GetTimeout() );
		const bool	bStreamingReady			= ( bStreamingTimeout || !m_streamer.IsStreaming() || m_streamer.IsFinishedStreaming() );
		return bStreamingReady;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ReadyForDescent
// PURPOSE:		returns true if switch is paused before descent but ready to proceed
//////////////////////////////////////////////////////////////////////////
bool CPlayerSwitchMgrLong::ReadyForDescent()
{
	if ( IsActive() && (m_currentState.GetState()==STATE_PREP_DESCENT || m_currentState.GetState()==STATE_PAN)
		&& (m_params.m_controlFlags&CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_DESCENT)!=0 )
	{
		camSwitchCamera* pSwitchCamera = camInterface::GetSwitchDirector().GetSwitchCamera();
#if __ASSERT
		if(!pSwitchCamera)
		{
			camBaseCamera *pActiveCamera = camInterface::GetSwitchDirector().GetActiveCamera();
			Assertf(pSwitchCamera, "pSwitchCamera is NULL! ActiveCamera: %p %s %s",
				pActiveCamera,
				pActiveCamera ? pActiveCamera->GetClassId().GetCStr() : "(unknown class id)",
				pActiveCamera ? pActiveCamera->GetMetadata().m_Name.GetCStr() : "(unknown name)");
		}
#endif // __ASSERT

		const bool	bCameraReady			= (m_currentState.GetState()==STATE_PAN) ? pSwitchCamera->HasBehaviourFinished() : true;
		const u32	timeSinceStateChange	= (fwTimer::GetTimeInMilliseconds_NonScaledClipped() - m_timeOfLastStateChange);
		const bool	bStreamingTimeout		= ( m_streamer.CanTimeout() && timeSinceStateChange>=m_streamer.GetTimeout() );
		const bool	bStreamingReady			= ( bStreamingTimeout || !m_streamer.IsStreaming() || m_streamer.IsFinishedStreaming() );
		return ( bStreamingReady && bCameraReady );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	EnablePauseBeforeDescent
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::EnablePauseBeforeDescent()
{
	if ( Verifyf(IsActive() && m_currentState.GetState()==STATE_WAITFORINPUT, "Player switch - only valid when waiting for input destination") )
	{
		m_params.m_controlFlags |= CPlayerSwitchInterface::CONTROL_FLAG_PAUSE_BEFORE_DESCENT;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetupSwitchEffectTypes
// PURPOSE:		figure out what effect should be used for each ped
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::SetupSwitchEffectTypes(const CPed& oldPed, const CPed& newPed)
{
	ePedType srcPedType = oldPed.GetPedType();
	ePedType dstPedType = newPed.GetPedType();

	switch (srcPedType)
	{
	case PEDTYPE_PLAYER_0: //MICHAEL
		{
			m_srcPedSwitchFxType = SWITCHFX_MICHAEL_IN;
			m_srcPedMidSwitchFxType = SWITCHFX_MICHAEL_MID;
		}
		break;
	case PEDTYPE_PLAYER_1: //FRANKLIN
		{
			m_srcPedSwitchFxType = SWITCHFX_FRANKLIN_IN;
			m_srcPedMidSwitchFxType = SWITCHFX_FRANKLIN_MID;
		}
		break;
	case PEDTYPE_PLAYER_2: //TREVOR
		{
			m_srcPedSwitchFxType = SWITCHFX_TREVOR_IN;
			m_srcPedMidSwitchFxType = SWITCHFX_TREVOR_MID;
		}
		break;
	default:
		{
			switch (GetArcadeTeam())
			{
			case eArcadeTeam::AT_CNC_COP:
				m_srcPedSwitchFxType = SWITCHFX_CNC_COP_IN;
				m_srcPedMidSwitchFxType = SWITCHFX_CNC_COP_MID;
				break;
			case eArcadeTeam::AT_CNC_CROOK:
				m_srcPedSwitchFxType = SWITCHFX_CNC_CROOK_IN;
				m_srcPedMidSwitchFxType = SWITCHFX_CNC_CROOK_MID;
				break;
			default:
				m_srcPedSwitchFxType = SWITCHFX_NEUTRAL_IN;
				m_srcPedMidSwitchFxType = SWITCHFX_NEUTRAL_MID;
				break;
			}
		}
	}

	switch (dstPedType)
	{
	case PEDTYPE_PLAYER_0: //MICHAEL
		{
			m_dstPedSwitchFxType = SWITCHFX_MICHAEL_OUT;
			m_dstPedMidSwitchFxType = SWITCHFX_MICHAEL_MID;
		}						   
		break;								   
	case PEDTYPE_PLAYER_1: //FRANKLIN			   
		{										   
			m_dstPedSwitchFxType = SWITCHFX_FRANKLIN_OUT;
			m_dstPedMidSwitchFxType = SWITCHFX_FRANKLIN_MID;
		}										   
		break;								   
	case PEDTYPE_PLAYER_2: //TREVOR				   
		{										   
			m_dstPedSwitchFxType = SWITCHFX_TREVOR_OUT;
			m_dstPedMidSwitchFxType = SWITCHFX_TREVOR_MID;
		}
		break;
	default:
		{
			switch (GetArcadeTeam())
			{
			case eArcadeTeam::AT_CNC_COP:
				m_dstPedSwitchFxType = SWITCHFX_CNC_COP_OUT;
				m_dstPedMidSwitchFxType = SWITCHFX_CNC_COP_MID;
				break;
			case eArcadeTeam::AT_CNC_CROOK:
				m_dstPedSwitchFxType = SWITCHFX_CNC_CROOK_OUT;
				m_dstPedMidSwitchFxType = SWITCHFX_CNC_CROOK_MID;
				break;
			default:
				m_dstPedSwitchFxType = SWITCHFX_NEUTRAL_OUT;
				m_dstPedMidSwitchFxType = SWITCHFX_NEUTRAL_MID;
				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateDesSwitchEffectType
// PURPOSE:		figure out what effect should be used for each ped
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::UpdateDesSwitchEffectType(const CPed& newPed)
{
	ePedType dstPedType = newPed.GetPedType();
	switch (dstPedType)
	{
	case PEDTYPE_PLAYER_0: //MICHAEL
		{
			m_dstPedSwitchFxType = SWITCHFX_MICHAEL_OUT;
			m_dstPedMidSwitchFxType = SWITCHFX_MICHAEL_MID;
		}						   
		break;								   
	case PEDTYPE_PLAYER_1: //FRANKLIN			   
		{										   
			m_dstPedSwitchFxType = SWITCHFX_FRANKLIN_OUT;
			m_dstPedMidSwitchFxType = SWITCHFX_FRANKLIN_MID;
		}										   
		break;								   
	case PEDTYPE_PLAYER_2: //TREVOR				   
		{										   
			m_dstPedSwitchFxType = SWITCHFX_TREVOR_OUT;
			m_dstPedMidSwitchFxType = SWITCHFX_TREVOR_MID;
		}
		break;
	default:
		{	
			switch (GetArcadeTeam())
			{
			case eArcadeTeam::AT_CNC_COP:
				m_dstPedSwitchFxType = SWITCHFX_CNC_COP_OUT;
				m_dstPedMidSwitchFxType = SWITCHFX_CNC_COP_MID;
				break;
			case eArcadeTeam::AT_CNC_CROOK:
				m_dstPedSwitchFxType = SWITCHFX_CNC_CROOK_OUT;
				m_dstPedMidSwitchFxType = SWITCHFX_CNC_CROOK_MID;
				break;
			default:
				m_dstPedSwitchFxType = SWITCHFX_NEUTRAL_OUT;
				m_dstPedMidSwitchFxType = SWITCHFX_NEUTRAL_MID;
				break;
			}
		}
	}
}

CPlayerSwitchMgrLong::eSwitchEffectType CPlayerSwitchMgrLong::GetNeutralFX()
{
	if (NetworkInterface::IsInCopsAndCrooks())
	{
		return SWITCHFX_NEUTRAL_CNC;
	}
	return SWITCHFX_NEUTRAL;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetArcadeTeam
// PURPOSE:		figure out what team the local player is on in arcade mode
//////////////////////////////////////////////////////////////////////////
eArcadeTeam CPlayerSwitchMgrLong::GetArcadeTeam()
{
	eArcadeTeam arcadeTeam = eArcadeTeam::AT_NONE;
	if (NetworkInterface::IsInCopsAndCrooks())
	{
		CPed *pPlayer = FindPlayerPed();
		if(pPlayer && pPlayer->GetPlayerInfo())
		{
			arcadeTeam = pPlayer->GetPlayerInfo()->GetArcadeInformation().GetTeam();
		}
	}
	return arcadeTeam;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetArcadeTeamFromEffectType
// PURPOSE:		helper functiont o figure out what team the given effect type is used for
//////////////////////////////////////////////////////////////////////////
eArcadeTeam CPlayerSwitchMgrLong::GetArcadeTeamFromEffectType(eSwitchEffectType effectType)
{
	if (effectType >= SWITCHFX_CNC_COP_FIRST && effectType <= SWITCHFX_CNC_COP_LAST)
	{
		return eArcadeTeam::AT_CNC_COP;
	}
	else if (effectType >= SWITCHFX_CNC_CROOK_FIRST && effectType <= SWITCHFX_CNC_CROOK_LAST)
	{
		return eArcadeTeam::AT_CNC_CROOK;
	}

	return eArcadeTeam::AT_NONE;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartMainSwitchEffect
// PURPOSE:		triggers switch effect
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::StartMainSwitchEffect(eSwitchEffectType effectType, bool bForceLoop, unsigned int delayFrame)
{
	//@@: location CPLAYERSWITCHMGRLONG_STARTMAINSWITCHEFFECT
	ANIMPOSTFXMGR.Stop(ms_switchFxName[ms_currentSwitchFxId],AnimPostFXManager::kLongPlayerSwitch);
	ms_currentSwitchFxId = effectType; 

	bool bLoop = (bForceLoop || (effectType == SWITCHFX_NEUTRAL));
	ANIMPOSTFXMGR.Start(ms_switchFxName[ms_currentSwitchFxId], 0, bLoop, false, false, delayFrame, AnimPostFXManager::kLongPlayerSwitch);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StartMainSwitchEffectOutro
// PURPOSE:		triggers final switch effect for last descent jump cut
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::StartMainSwitchEffectOutro()
{
	//@@: location CPLAYERSWITCHMGRLONG_STARTMAINSWITCHEFFECTOUTRO
	ANIMPOSTFXMGR.Stop(ms_switchFxName[m_srcPedMidSwitchFxType],AnimPostFXManager::kLongPlayerSwitch);
	ANIMPOSTFXMGR.Stop(ms_switchFxName[m_dstPedMidSwitchFxType],AnimPostFXManager::kLongPlayerSwitch);
	if( (m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SUPPRESS_OUTRO_FX) == 0 )
	{
		StartMainSwitchEffect(m_dstPedSwitchFxType, false, 0);
	}
	else
	{
		// We still need to stop the previous effect, if not, we get weird.
		ANIMPOSTFXMGR.Stop(ms_switchFxName[ms_currentSwitchFxId],AnimPostFXManager::kLongPlayerSwitch);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	StopMainSwitchEffect
// PURPOSE:		stops current switch effect
//////////////////////////////////////////////////////////////////////////
void CPlayerSwitchMgrLong::StopMainSwitchEffect()
{
	ANIMPOSTFXMGR.Stop(ms_switchFxName[ms_currentSwitchFxId],AnimPostFXManager::kLongPlayerSwitch);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsSkippingDescent
// PURPOPSE:	returns true if the active long/med switch will not do any descent state
//////////////////////////////////////////////////////////////////////////
bool CPlayerSwitchMgrLong::IsSkippingDescent() const
{
	return ( IsActive() && (m_params.m_numJumpsDescent==1) && (m_params.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_SKIP_TOP_DESCENT)!=0 );
}
