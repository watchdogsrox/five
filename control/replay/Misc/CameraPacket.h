#ifndef CAMERA_PACKET_H
#define CAMERA_PACKET_H

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/misc/InterpPacket.h"
#include "control/replay/PacketBasics.h"
#include "camera/viewports/ViewportFader.h"

#include "fwutil/Flags.h"

#include "scene/RegdRefTypes.h"


class replayCamFrame;

//////////////////////////////////////////////////////////////////////////
// We don't create or delete cameras so this packet is simply here to fill
// in the roll of the createcamera and deletecamera packets.
class CPacketCameraDummy : public CPacketBase
{
public:
	void				Store(s32 replayID)			{	CPacketBase::Store((eReplayPacketId)replayID, sizeof(CPacketCameraDummy));	}
	void				Store(const CEntity*)		{}

	const CReplayID&	GetReplayID() const			{	return ReplayIDInvalid;		}
	void				Cancel()					{}
	bool				IsFirstCreationPacket() const	{	return false;	}

	const CPacketCameraDummy*	GetCreatePacket() const			{	return NULL;	}
	FrameRef			GetFrameCreated() const		{	return FrameRef();		}

	u32					GetMapTypeDefHash() const { return 0; }
	bool				UseMapTypeDefHash() const { return false; }
	void				SetMapTypeDefIndex(strLocalIndex) {}
};


//////////////////////////////////////////////////////////////////////////
class CPacketCameraUpdate : public CPacketBase, public CPacketInterp
{
public:

	void				Store(fwFlags16 cachedFrameFlags);
	void				StoreExtensions(fwFlags16 cachedFrameFlags) { PACKET_EXTENSION_RESET(CPacketCameraUpdate); (void)cachedFrameFlags; }
	void				Extract(ReplayController& controller, replayCamFrame& cameraFrame, CReplayInterfaceManager& interfaceManager, fwFlags16& cachedFlags, CPacketCameraUpdate const* pNextPacket, float fInterp, bool rewinding) const;

	bool				GetHasCreateInfo() const				{	return false;	}
	const CPacketCameraDummy*	GetCreatePacket() const			{	return NULL;	}
	const CReplayID&	GetReplayID() const			{	return ReplayIDInvalid;		}

	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_CAMERAUPDATE, "Validation of CPacketCameraUpdate Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketInterp::ValidatePacket() && GetPacketID() == PACKETID_CAMERAUPDATE;
	}

private:

	void UpdateRenderPhase() const;

protected:

	CPacketPositionAndQuaternion20	m_posAndRot;
	CPacketVector<4>	m_DofPlanes;
	CPacketVector<2>	m_focusDistanceGridScaling;
	CPacketVector<2>	m_subjectMagPowerNearFar;
	float				m_DofBlurDiskRadius;
	float				m_fovy;
	float				m_nearClip;
	float				m_farClip;
	float				m_motionBlurStrength;
	float				m_fullScreenBlurBlendLevel;
	float				m_maxPixelDepth;
	float				m_maxPixelDepthBlendLevel;
	float				m_pixelDepthPowerFactor;
	float				m_fNumberOfLens;
	float				m_focalLengthMultiplier;
	float				m_fStopsAtMaxExposure;
	float				m_focusDistanceBias;
	float				m_maxNearInFocusDistance;
	float				m_maxNearInFocusDistanceBlendLevel;
	float				m_maxBlurRadiusAtNearInFocusLimit;
	float				m_blurDiskRadiusPowerFactorNear;
	float				m_overriddenFocusDistance;
	float				m_overriddenFocusDistanceBlendLevel;
	float				m_focusDistanceIncreaseSpringConstant;
	float				m_focusDistanceDecreaseSpringConstant;
	float				m_DofPlanesBlendLevel;
	fwFlags16			m_flags;
	CViewportFader		m_fader;

	CReplayID			m_TrackedEntityReplayID;

	u32					m_dominantCameraNameHash;
	u32					m_dominantCameraClassIDHash;

	bool				m_FirstPersonCamera;

	DECLARE_PACKET_EXTENSION(CPacketCameraUpdate);

	//Static data used on playback
	static Vector3		sm_LastFramePos;
	static float		sm_LastFrameFov;
	static Quaternion	sm_LastFrameQuaternion;
	static Vector3		sm_LastFramePosBeforeJumping;
	static const void*  sm_LastFramePacket;
};


//////////////////////////////////////////////////////////////////////////
class CCamInterp : public CInterpolator<CPacketCameraUpdate>
{
public:
	CCamInterp() { Reset(); }

	void Init(const ReplayController& controller, CPacketCameraUpdate const* pPrevPacket);

	void Reset()
	{
		CInterpolator<CPacketCameraUpdate>::Reset();

		m_sPrevFullPacketSize = m_sNextFullPacketSize = 0;
	}

	s32	GetPrevFullPacketSize() const	{ return m_sPrevFullPacketSize; }
	s32	GetNextFullPacketSize() const	{ return m_sNextFullPacketSize; }

protected:
	s32							m_sPrevFullPacketSize;
	s32							m_sNextFullPacketSize;
};

#	endif // GTA_REPLAY

#endif  // CAMERA_PACKET_H
