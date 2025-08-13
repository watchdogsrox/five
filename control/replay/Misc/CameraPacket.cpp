//=====================================================================================================
// name:		CameraPacket.cpp
//=====================================================================================================

#include "control/replay/ReplayInternal.h"
#include "control/replay/Misc/CameraPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "camera/CamInterface.h"
#include "camera/helpers/Frame.h"
#include "camera/base/BaseCamera.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "control/replay/Replay.h"

//////////////////////////////////////////////////////////////////////////
void CCamInterp::Init(const ReplayController& controller, CPacketCameraUpdate const* pPrevPacket)
{
	// Setup the previous packet
	SetPrevPacket(pPrevPacket);

	m_sPrevFullPacketSize += pPrevPacket->GetPacketSize();

	// Look for/Setup the next packet
	if (pPrevPacket->HasNextOffset())
	{
		CPacketCameraUpdate const* pNextPacket = NULL;
		controller.GetNextPacket(pPrevPacket, pNextPacket);
		SetNextPacket(pNextPacket);

		m_sNextFullPacketSize += pNextPacket->GetPacketSize();
	}
	else
	{
		replayAssertf(false, "CInterpolator::Init");
	}
}


// If this fails then camFrame's size has changed. Ensure the added parameters are saved and loaded in this CameraPacket.cpp file (only if needed for replay).
// NOTE: If this is after launch make the the parameters are added to the end of CPacketCameraUpdate and update it's version number!
CompileTimeAssert(sizeof(camFrame) == 224);
Vector3				CPacketCameraUpdate::sm_LastFramePos;
float				CPacketCameraUpdate::sm_LastFrameFov;
Quaternion			CPacketCameraUpdate::sm_LastFrameQuaternion;
Vector3				CPacketCameraUpdate::sm_LastFramePosBeforeJumping;
const void*			CPacketCameraUpdate::sm_LastFramePacket = NULL;

tPacketVersion g_PacketCameraUpdate_v1 = 1;
//////////////////////////////////////////////////////////////////////////
void CPacketCameraUpdate::Store(fwFlags16 cachedFrameFlags)
{
	PACKET_EXTENSION_RESET(CPacketCameraUpdate);
	CPacketBase::Store(PACKETID_CAMERAUPDATE, sizeof(CPacketCameraUpdate), g_PacketCameraUpdate_v1);
	CPacketInterp::Store();

	const camFrame& renderedFrame = camInterface::GetFrame();
	
	m_posAndRot.StoreMatrix(renderedFrame.GetWorldMatrix());
	Vector4 planes;
	renderedFrame.ComputeDofPlanes(planes);
	m_DofPlanes.Store(planes);

	m_focusDistanceGridScaling.Store(renderedFrame.GetDofFocusDistanceGridScaling());
	m_subjectMagPowerNearFar.Store(renderedFrame.GetDofSubjectMagPowerNearFar());
	m_DofBlurDiskRadius			= renderedFrame.GetDofBlurDiskRadius();
	m_fovy						= renderedFrame.GetFov();
	m_nearClip					= renderedFrame.GetNearClip();
	m_farClip					= renderedFrame.GetFarClip();
	m_motionBlurStrength		= renderedFrame.GetMotionBlurStrength();
	m_maxPixelDepth				= renderedFrame.GetDofMaxPixelDepth();
	m_maxPixelDepthBlendLevel	= renderedFrame.GetDofMaxPixelDepthBlendLevel();
	m_pixelDepthPowerFactor		= renderedFrame.GetDofPixelDepthPowerFactor();
	m_fNumberOfLens				= renderedFrame.GetFNumberOfLens();
	m_focalLengthMultiplier		= renderedFrame.GetFocalLengthMultiplier();
	m_fStopsAtMaxExposure		= renderedFrame.GetDofFStopsAtMaxExposure();
	m_focusDistanceBias			= renderedFrame.GetDofFocusDistanceBias();
	m_maxNearInFocusDistance	= renderedFrame.GetDofMaxNearInFocusDistance();
	m_maxNearInFocusDistanceBlendLevel	  = renderedFrame.GetDofMaxNearInFocusDistanceBlendLevel();
	m_maxBlurRadiusAtNearInFocusLimit	  = renderedFrame.GetDofMaxBlurRadiusAtNearInFocusLimit();
	m_blurDiskRadiusPowerFactorNear		  = renderedFrame.GetDofBlurDiskRadiusPowerFactorNear();
	m_overriddenFocusDistance			  = renderedFrame.GetDofOverriddenFocusDistance();
	m_overriddenFocusDistanceBlendLevel	  = renderedFrame.GetDofOverriddenFocusDistanceBlendLevel();
	m_focusDistanceIncreaseSpringConstant = renderedFrame.GetDofFocusDistanceIncreaseSpringConstant();
	m_focusDistanceDecreaseSpringConstant = renderedFrame.GetDofFocusDistanceDecreaseSpringConstant();
	m_DofPlanesBlendLevel				  = renderedFrame.GetDofPlanesBlendLevel();
	m_fullScreenBlurBlendLevel			  = renderedFrame.GetDofFullScreenBlurBlendLevel();

	m_fader = camInterface::GetFader();

	m_flags = renderedFrame.GetFlags() | cachedFrameFlags;

	// Store camera target info
	const CEntity *pTrackedEntity = camInterface::GetCachedCameraAttachParent();
	if( pTrackedEntity && pTrackedEntity->GetIsPhysical() )
	{
		// DEBUG
		//Displayf("Recording tracking entity %s", pTrackedEntity->GetModelName());
		// DEBUG

		m_TrackedEntityReplayID = ((CPhysical*)(pTrackedEntity))->GetReplayID();
	}
	else
	{
		m_TrackedEntityReplayID = ReplayIDInvalid;
	}

	m_FirstPersonCamera = camInterface::IsDominantRenderedCameraAnyFirstPersonCamera() || camInterface::GetGameplayDirector().IsFirstPersonAiming();

	const camBaseCamera* pDominantCamera = camInterface::GetDominantRenderedCamera();
	m_dominantCameraNameHash	= pDominantCamera->GetNameHash();
	m_dominantCameraClassIDHash = pDominantCamera->GetClassId().GetHash();
}


//////////////////////////////////////////////////////////////////////////
void CPacketCameraUpdate::Extract(ReplayController& controller, replayCamFrame& cameraFrame, CReplayInterfaceManager& interfaceManager, fwFlags16& cachedFlags, CPacketCameraUpdate const* pNextPacket, float fInterp, bool rewinding) const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketCameraUpdate) == 0);
	(void)pNextPacket;
	(void)fInterp;

	Matrix34 mat;

	// We don't want to set the interp to be 0 if we are simply changing cameras between in vehicle and on foot etc.
	// Ideally we would detect the entity being moved and needing an interpolate update but for now just do this on cutscene cameras
	// Might need to be exanded for other cameras.
	bool cameraCut = false;
	if(GetPacketVersion() >= g_PacketCameraUpdate_v1)
	{
		if(pNextPacket->m_flags.IsFlagSet(camFrame::Flag_HasCutPosition) && pNextPacket->m_flags.IsFlagSet(camFrame::Flag_HasCutOrientation) && 
		!m_flags.IsFlagSet(camFrame::Flag_HasCutPosition) && !m_flags.IsFlagSet(camFrame::Flag_HasCutOrientation))
		{
			cameraCut = true;
		}
		
		// Always cut the camera if going in or out of first person camera
		if(m_FirstPersonCamera != pNextPacket->m_FirstPersonCamera)
		{
			cameraCut = true;
		}
	}
	else
	{
		if(pNextPacket->m_flags.IsFlagSet(camFrame::Flag_HasCutPosition) || pNextPacket->m_flags.IsFlagSet(camFrame::Flag_HasCutOrientation))
		{
			cameraCut = true;
		}
	}

	float fovy = m_fovy;
	if(pNextPacket != NULL && !cameraCut)
	{
		m_posAndRot.Interpolate(pNextPacket->m_posAndRot, fInterp, mat);
		fovy = Lerp(fInterp, m_fovy, pNextPacket->m_fovy);
			
		// Data used if the camera no longer has a next packet
		sm_LastFramePacket = pNextPacket;
		sm_LastFrameFov = m_fovy; 
		m_posAndRot.LoadPosition(sm_LastFramePos);
		m_posAndRot.GetQuaternion(sm_LastFrameQuaternion);
	}
	else
	{
		if(GetPacketVersion() >= g_PacketCameraUpdate_v1)
		{
			// If we don't have a next packet and it's a camera cut, interpolate the camera using the last frame data
			// Don't work if we're playing backwards which is fine, set the default mat.
			// Improvement - calculate time delta between frames and use with interpolation.
			if(sm_LastFramePacket == this && !controller.GetIsFirstFrame())
			{
				CPacketPositionAndQuaternion20 posAndRot = m_posAndRot;

				float fNewInterp = 1.0f + fInterp;
				Matrix34 newMat;
				Vector3 pos;
				Quaternion quat;
				Quaternion nextQuat;

				m_posAndRot.GetQuaternion(nextQuat);
				quat.Slerp(fNewInterp, sm_LastFrameQuaternion, nextQuat);
				newMat.FromQuaternion(quat);

				m_posAndRot.LoadPosition(pos);
				Vector3 posDelta = pos - sm_LastFramePos;
				Vector3 NextPos = pos + posDelta;
				newMat.MakeTranslate(NextPos);

				posAndRot.StoreMatrix(newMat);
				m_posAndRot.Interpolate(posAndRot, fInterp, mat);

				fovy = Lerp(fNewInterp, sm_LastFrameFov, m_fovy);
			}
			else
			{
				m_posAndRot.LoadMatrix(mat);
			}

			// Fix the fp camera, have to stop all entities interpolating so they are in sync
			if(m_FirstPersonCamera || pNextPacket->m_FirstPersonCamera)
			{
				m_posAndRot.LoadMatrix(mat);
				controller.SetInterp( 0.0f );
			}
		}
		else
		{
			// Don't interp the controller(obj/peds/veh/etc) if we're playing a cut scene or if the camera has jumped a large distance.
			m_posAndRot.LoadMatrix(mat);
			controller.SetInterp( 0.0f );
		}
	}

	Vector4 dofPlanes;
	m_DofPlanes.Load(dofPlanes);
	Vector2 gridScaling;
	m_focusDistanceGridScaling.Load(gridScaling);
	Vector2 subjectMagPowerNearFar;
	m_subjectMagPowerNearFar.Load(subjectMagPowerNearFar);
 
 	cameraFrame.m_Frame.SetWorldMatrix(mat, false);
	cameraFrame.m_Frame.SetDofPlanes(dofPlanes);
	cameraFrame.m_Frame.SetDofFocusDistanceGridScaling(gridScaling);
	cameraFrame.m_Frame.SetDofSubjectMagPowerNearFar(subjectMagPowerNearFar);
	cameraFrame.m_Frame.SetDofBlurDiskRadius(m_DofBlurDiskRadius);
 	cameraFrame.m_Frame.SetFov(fovy);
 	cameraFrame.m_Frame.SetNearClip(m_nearClip);
 	cameraFrame.m_Frame.SetFarClip(m_farClip);
 	cameraFrame.m_Frame.SetMotionBlurStrength(m_motionBlurStrength);
	cameraFrame.m_Frame.SetDofMaxPixelDepth(m_maxPixelDepth);
	cameraFrame.m_Frame.SetDofMaxPixelDepthBlendLevel(m_maxPixelDepthBlendLevel);
	cameraFrame.m_Frame.SetDofPixelDepthPowerFactor(m_pixelDepthPowerFactor);
	cameraFrame.m_Frame.SetFNumberOfLens(m_fNumberOfLens);
	cameraFrame.m_Frame.SetFocalLengthMultiplier(m_focalLengthMultiplier);
	cameraFrame.m_Frame.SetDofFStopsAtMaxExposure(m_fStopsAtMaxExposure);
	cameraFrame.m_Frame.SetDofFocusDistanceBias(m_focusDistanceBias);
	cameraFrame.m_Frame.SetDofMaxNearInFocusDistance(m_maxNearInFocusDistance);
	cameraFrame.m_Frame.SetDofMaxNearInFocusDistanceBlendLevel(m_maxNearInFocusDistanceBlendLevel);
	cameraFrame.m_Frame.SetDofMaxBlurRadiusAtNearInFocusLimit(m_maxBlurRadiusAtNearInFocusLimit);
	cameraFrame.m_Frame.SetDofBlurDiskRadiusPowerFactorNear(m_blurDiskRadiusPowerFactorNear);
	cameraFrame.m_Frame.SetDofOverriddenFocusDistance(m_overriddenFocusDistance);
	cameraFrame.m_Frame.SetDofOverriddenFocusDistanceBlendLevel(m_overriddenFocusDistanceBlendLevel);
	cameraFrame.m_Frame.SetDofFocusDistanceIncreaseSpringConstant(m_focusDistanceIncreaseSpringConstant);
	cameraFrame.m_Frame.SetDofFocusDistanceDecreaseSpringConstant(m_focusDistanceDecreaseSpringConstant);
	cameraFrame.m_Frame.SetDofPlanesBlendLevel(m_DofPlanesBlendLevel);
	cameraFrame.m_Frame.SetDofFullScreenBlurBlendLevel(m_fullScreenBlurBlendLevel);

	// Get the currently set flags...
	u16 currFlags = cachedFlags;
	
	// If we're rewinding and the 'next' replay frame requested a portal scan, then do it for this frame
	if(rewinding && pNextPacket->m_flags.IsFlagSet(camFrame::Flag_RequestPortalRescan))
		currFlags |= camFrame::Flag_RequestPortalRescan;

	// OR the packets flags with the current flags (so we keep any from frames that were skipped over)
	cachedFlags.SetAllFlags(currFlags | m_flags);

	// If we're rewinding and the 'current' replay frame flags say portal scan then cancel it
	// This is because this would have been for playing forwards.
	if(rewinding && m_flags.IsFlagSet(camFrame::Flag_RequestPortalRescan))
		cachedFlags.ClearFlag(camFrame::Flag_RequestPortalRescan);

	// Set this flag so that the near clip is not altered during playback.
	cachedFlags.SetFlag(camFrame::Flag_BypassNearClipScanner);

	// Set the camera to rescan for the first frame if the player is in first person mode on the first frame
	// Fixes issue if the player was already in the same interior before loading the clip
	if(controller.GetIsFirstFrame() && m_FirstPersonCamera)
	{
		cachedFlags.SetFlag(camFrame::Flag_RequestPortalRescan);
	}

	cameraFrame.m_Frame.GetFlags() = cachedFlags;

	// Restore camera target entity, if one exists.
	if(	m_TrackedEntityReplayID != ReplayIDInvalid )
	{
		cameraFrame.m_Target = interfaceManager.FindEntity(m_TrackedEntityReplayID);
	}
	else
	{
		cameraFrame.m_Target = NULL;
	}

	cameraFrame.m_Fader = m_fader;

	cameraFrame.m_DominantCameraNameHash	= m_dominantCameraNameHash;
	cameraFrame.m_DominantCameraClassIDHash = m_dominantCameraClassIDHash;

	// Freeze render phase if the camera has moved to a location where the world isn't loaded when jumping
	UpdateRenderPhase();
}

void CPacketCameraUpdate::UpdateRenderPhase() const
{
	Vector3 newPosition;
	m_posAndRot.LoadPosition(newPosition);

	if(CReplayMgrInternal::IsJumpPreparing())
	{
		Vector3 distanceCameraMoved = newPosition - sm_LastFramePosBeforeJumping; 
		const float smallDistanceThreshold = 1.0f;
		const bool bAllRequiredMapDataLoaded = fwMapDataStore::GetStore().GetBoxStreamer().HasLoadedAboutPos(VECTOR3_TO_VEC3V(newPosition), fwBoxStreamerAsset::MASK_MAPDATA);
		if(!bAllRequiredMapDataLoaded && (!CReplayMgr::IsFineScrubbing() && distanceCameraMoved.Mag2() > smallDistanceThreshold))
		{
			// Map isn't loaded, pause render phase
			CReplayMgr::PauseRenderPhase();
		}
		else
		{
			// Fallback - always pause if the camrea moves more than a set distance
			const float fMaxDistanceToJumpWithoutFreezingRenderPhase2 = 100.0f * 100.0f;
			if(!CReplayMgr::IsFineScrubbing() && distanceCameraMoved.Mag2() > fMaxDistanceToJumpWithoutFreezingRenderPhase2)
			{
				CReplayMgr::PauseRenderPhase();
			}

			// Map is load, store last know loaded location.
			sm_LastFramePosBeforeJumping = newPosition; 
		}
	}
	else
	{
		if(!CReplayMgr::IsPreCachingScene())
		{
			// Map is load, store last know loaded location.
			sm_LastFramePosBeforeJumping = newPosition; 
		}
	}
}


#endif // GTA_REPLAY
