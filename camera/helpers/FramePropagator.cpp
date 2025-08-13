//
// camera/helpers/FramePropagator.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/FramePropagator.h"

#include "grcore/config_switches.h"
#include "profile/timebars.h"
#include "string/string.h"

#include "camera/camera_channel.h"
#include "camera/base/BaseCamera.h"
#include "camera/debug/FreeCamera.h"
#include "camera/helpers/NearClipScanner.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/Viewport.h"
#include "debug/TiledScreenCapture.h"
#include "frontend/PauseMenu.h"
#include "network/NetworkInterface.h"
#include "pathserver/ExportCollision.h"
#include "physics/WorldProbe/WorldProbe.h"
#include "scene/DynamicEntity.h"
#include "timeCycle/TimeCycle.h"
#include "Renderer/RenderPhases/RenderPhaseLensDistortion.h"
#include "input/headtracking.h"

CAMERA_OPTIMISATIONS()

PARAM(farclip, "[camFramePropagator] Override far clip value");
PARAM(noscanningnearclip, "[camFramePropagator] Bypass the scanning near-clip functionality");

//NOTE: We only require the near-clip scanner where we don't invert the depth buffer, where we have more issues with depth precision.
#define USE_SCANNING_NEAR_CLIP (!SUPPORT_INVERTED_PROJECTION)

bank_float	camFramePropagator::ms_MinMotionBlurStrength	= 0.000999f;

const u32 g_NearClipScannerHash = ATSTRINGHASH("DEFAULT_NEAR_CLIP_SCANNER", 0x02449518f);


camFramePropagator::camFramePropagator()
: m_NearClipScanner(NULL)
, m_NumUpdatesBypassed(0)
, m_ShouldDisableNearClipScanThisUpdate(false)
#if __BANK
, m_OverriddenDofPlanes(0.0f, g_DefaultNearInFocusDofPlane, g_DefaultFarInFocusDofPlane, 0.0f)
, m_OverriddenDofFocusDistanceGridScaling(1.0f, 1.0f)
, m_OverriddenDofSubjectMagPowerNearFar(1.0f, 1.0f)
, m_OverriddenNearClip(g_MinNearClip)
, m_OverriddenFarClip(100.0f)
, m_OverriddenDofStrength(g_DefaultDofStrength)
, m_OverriddenFov(g_DefaultFov)
, m_OverriddenMotionBlurStrength(1.0f)
, m_OverriddenDofMaxPixelDepth(LARGE_FLOAT)
, m_OverriddenDofMaxPixelDepthBlendLevel(0.0f)
, m_OverriddenDofPixelDepthPowerFactor(1.0f)
, m_OverriddenFNumberOfLens(1.0f)
, m_OverriddenFocalLengthMultiplier(1.0f)
, m_OverriddenDofFStopsAtMaxExposure(0.0f)
, m_OverriddenDofFocusDistanceBias(0.0f)
, m_OverriddenDofMaxNearInFocusDistance(LARGE_FLOAT)
, m_OverriddenDofMaxNearInFocusDistanceBlendLevel(0.0f)
, m_OverriddenDofMaxBlurRadiusAtNearInFocusLimit(1.0f)
, m_OverriddenDofBlurDiskRadiusPowerFactorNear(1.0f)
, m_OverriddenDofFocusDistanceIncreaseSpringConstant(0.0f)
, m_OverriddenDofFocusDistanceDecreaseSpringConstant(0.0f)
, m_OverriddenDofPlanesBlendLevel(0.0f)
, m_OverriddenDofFullScreenBlurBlendLevel(0.0f)
, m_OverriddenDofOverriddenFocusDistance(0.0f)
, m_OverriddenDofOverriddenFocusDistanceBlendLevel(0.0f)
, m_OverriddenUseHighQualityDofState(false)
, m_OverriddenUseShallowDofState(false)
, m_ShouldOverrideDofFocusDistanceGridScaling(false)
, m_ShouldOverrideDofSubjectMagPowerNearFar(false)
, m_ShouldOverrideNearClip(false)
, m_ShouldOverrideFarClip(false)
, m_ShouldOverrideDofStrength(false)
, m_ShouldOverrideUseHighQualityDof(false)
, m_ShouldOverrideUseShallowDof(false)
, m_ShouldOverrideFov(false)
, m_ShouldOverrideMotionBlurStrength(false)
, m_ShouldOverrideDofMaxPixelDepth(false)
, m_ShouldOverrideDofMaxPixelDepthBlendLevel(false)
, m_ShouldOverrideDofPixelDepthPowerFactor(false)
, m_ShouldOverrideFNumberOfLens(false)
, m_ShouldOverrideFocalLengthMultiplier(false)
, m_ShouldOverrideDofFStopsAtMaxExposure(false)
, m_ShouldOverrideDofFocusDistanceBias(false)
, m_ShouldOverrideDofMaxNearInFocusDistance(false)
, m_ShouldOverrideDofMaxNearInFocusDistanceBlendLevel(false)
, m_ShouldOverrideDofMaxBlurRadiusAtNearInFocusLimit(false)
, m_ShouldOverrideDofBlurDiskRadiusPowerFactorNear(false)
, m_ShouldOverrideDofFocusDistanceIncreaseSpringConstant(false)
, m_ShouldOverrideDofFocusDistanceDecreaseSpringConstant(false)
, m_ShouldOverrideDofPlanesBlendLevel(false)
, m_ShouldOverrideDofFullScreenBlurBlendLevel(false)
, m_ShouldOverrideDofOverriddenFocusDistance(false)
, m_ShouldOverrideDofOverriddenFocusDistanceBlendLevel(false)
, m_ShouldDisplayViewportAttachEntityStatus(false)
, m_DebugHasAttachEntityForViewport(false)
, m_DebugWasAttachEntityInvalidDueToNoLos(false)
#endif // __BANK
{
#if __BANK
	for(int i=0; i<camFrame::NUM_DOF_PLANES; i++)
	{
		m_ShouldOverrideDofPlanes[i] = false;
	}
#endif // __BANK
}

void camFramePropagator::Init()
{
#if USE_SCANNING_NEAR_CLIP
	if(!PARAM_noscanningnearclip.Get())
	{
		//NOTE: The near-clip scanner is an optional component and may not be defined for every platform.
		const camNearClipScannerMetadata* metadata = camFactory::FindObjectMetadata<camNearClipScannerMetadata>(g_NearClipScannerHash);
		if(metadata)
		{
			m_NearClipScanner = camFactory::CreateObject<camNearClipScanner>(g_NearClipScannerHash);
			cameraAssertf(m_NearClipScanner, "The camera frame propagator failed to create a near-clip scanner (hash: %u)", g_NearClipScannerHash);
		}
	}
#endif // USE_SCANNING_NEAR_CLIP

#if __BANK
	//Pull the optional development parameter for overriding the far clip.
	if(PARAM_farclip.Get())
	{
		m_ShouldOverrideFarClip = true;

		float overridenFarClip = 0.0f;
		if(PARAM_farclip.Get(overridenFarClip))
		{
			m_OverriddenFarClip = overridenFarClip;
		}
	}
#endif // __BANK
}

void camFramePropagator::Shutdown()
{
	if(m_NearClipScanner)
	{
		delete m_NearClipScanner;
		m_NearClipScanner = NULL;
	}
}

bool camFramePropagator::Propagate(camFrame& sourceFrame, const camBaseCamera* activeCamera, CViewport& destinationViewport)
{
	//make sure we always update the attach entity pointer in the viewport.
	const CEntity* attachEntity = activeCamera ? activeCamera->GetAttachParent() : NULL;
	destinationViewport.SetAttachEntity(attachEntity);

	if(!cameraVerifyf(destinationViewport.GetArea() > 0.0f, "A camera frame propagator cannot update a zero-sized viewport"))
	{
		return false;
	}

	const bool isFrameValid = ComputeIsFrameValid(sourceFrame, activeCamera);

	if(isFrameValid)
	{
#if __BANK
		if (TiledScreenCapture::IsEnabled())
		{
			// just set the camera and return
			destinationViewport.PropagateCameraFrame(sourceFrame);
			return isFrameValid;
		}
#endif // __BANK

		//Don't generate very minor motion blur.
		if(sourceFrame.GetMotionBlurStrength() < ms_MinMotionBlurStrength)
		{
			sourceFrame.SetMotionBlurStrength(0.0f);
		}

		UpdateFarClipFromTimeCycle(sourceFrame);
		
#if SUPPORT_MULTI_MONITOR
		// Apply multi-head transformation
		sourceFrame.SetFov( GRCDEVICE.GetMonitorConfig().transformFOV( sourceFrame.GetFov() ) );
#endif	//SUPPORT_MULTI_MONITOR
#if RSG_PC
		if (rage::ioHeadTracking::IsMotionTrackingEnabled())
		{
			sourceFrame.SetFov(CRenderPhaseLensDistortion::GetFOV());
		}
#endif // RSG_PC

		PF_PUSH_TIMEBAR_DETAIL("NEARCLIPSCAN");
		if(!m_ShouldDisableNearClipScanThisUpdate && m_NearClipScanner && !sourceFrame.GetFlags().IsFlagSet(camFrame::Flag_BypassNearClipScanner))
		{
			m_NearClipScanner->Update(activeCamera, sourceFrame);
		}
		PF_POP_TIMEBAR_DETAIL(); // NEARCLIPSCAN

		m_ShouldDisableNearClipScanThisUpdate = false;

#if __BANK
		bool overrideNearClip = false;

		// Use the command line setting
		XPARAM(overrideNearClip);
		float overrideNearClipValue = 0.0f;
		if(PARAM_overrideNearClip.Get(overrideNearClipValue))
		{
			m_OverriddenNearClip = overrideNearClipValue;
			m_ShouldOverrideNearClip = true;
			overrideNearClip = true;
		}

		if(overrideNearClip || m_ShouldOverrideNearClip)
		{
			sourceFrame.SetNearClip(m_OverriddenNearClip);
		}
		if(m_ShouldOverrideFarClip)
		{
			sourceFrame.SetFarClip(m_OverriddenFarClip);
		}

		Vector4 dofPlanes;
		sourceFrame.ComputeDofPlanes(dofPlanes);

		bool hasOverriddenDofPlanes = false;
		for(int i=0; i<camFrame::NUM_DOF_PLANES; i++)
		{
			if(m_ShouldOverrideDofPlanes[i])
			{
				dofPlanes[i]			= m_OverriddenDofPlanes[i];
				hasOverriddenDofPlanes	= true;
			}
		}

		if(hasOverriddenDofPlanes)
		{
			sourceFrame.SetDofPlanes(dofPlanes);
		}

		if(m_ShouldOverrideDofStrength)
		{
			sourceFrame.SetDofStrength(m_OverriddenDofStrength);
		}
		if(m_ShouldOverrideUseHighQualityDof)
		{
			sourceFrame.SetDofPlanesBlendLevel(m_OverriddenUseHighQualityDofState ? 1.0f : 0.0f);
		}
		if(m_ShouldOverrideUseShallowDof)
		{
			sourceFrame.GetFlags().ChangeFlag(camFrame::Flag_ShouldUseShallowDof, m_OverriddenUseShallowDofState);
		}
		if(m_ShouldOverrideFov)
		{
			sourceFrame.SetFov(m_OverriddenFov);
		}
		if(m_ShouldOverrideMotionBlurStrength)
		{
			sourceFrame.SetMotionBlurStrength(m_OverriddenMotionBlurStrength);
		}

		if(m_ShouldOverrideDofFocusDistanceGridScaling)
		{
			sourceFrame.SetDofFocusDistanceGridScaling(m_OverriddenDofFocusDistanceGridScaling);
		}
		else
		{
			m_OverriddenDofFocusDistanceGridScaling = sourceFrame.GetDofFocusDistanceGridScaling();
		}

		if(m_ShouldOverrideDofSubjectMagPowerNearFar)
		{
			sourceFrame.SetDofSubjectMagPowerNearFar(m_OverriddenDofSubjectMagPowerNearFar);
		}
		else
		{
			m_OverriddenDofSubjectMagPowerNearFar = sourceFrame.GetDofSubjectMagPowerNearFar();
		}

		if(m_ShouldOverrideDofMaxPixelDepth)
		{
			sourceFrame.SetDofMaxPixelDepth(m_OverriddenDofMaxPixelDepth);
		}
		else
		{
			m_OverriddenDofMaxPixelDepth = sourceFrame.GetDofMaxPixelDepth();
		}

		if(m_ShouldOverrideDofMaxPixelDepthBlendLevel)
		{
			sourceFrame.SetDofMaxPixelDepthBlendLevel(m_OverriddenDofMaxPixelDepthBlendLevel);
		}
		else
		{
			m_OverriddenDofMaxPixelDepthBlendLevel = sourceFrame.GetDofMaxPixelDepthBlendLevel();
		}

		if(m_ShouldOverrideDofPixelDepthPowerFactor)
		{
			sourceFrame.SetDofPixelDepthPowerFactor(m_OverriddenDofPixelDepthPowerFactor);
		}
		else
		{
			m_OverriddenDofPixelDepthPowerFactor = sourceFrame.GetDofPixelDepthPowerFactor();
		}

		if(m_ShouldOverrideFNumberOfLens)
		{
			sourceFrame.SetFNumberOfLens(m_OverriddenFNumberOfLens);
		}
		else
		{
			m_OverriddenFNumberOfLens = sourceFrame.GetFNumberOfLens();
		}

		if(m_ShouldOverrideFocalLengthMultiplier)
		{
			sourceFrame.SetFocalLengthMultiplier(m_OverriddenFocalLengthMultiplier);
		}
		else
		{
			m_OverriddenFocalLengthMultiplier = sourceFrame.GetFocalLengthMultiplier();
		}

		if(m_ShouldOverrideDofFStopsAtMaxExposure)
		{
			sourceFrame.SetDofFStopsAtMaxExposure(m_OverriddenDofFStopsAtMaxExposure);
		}
		else
		{
			m_OverriddenDofFStopsAtMaxExposure = sourceFrame.GetDofFStopsAtMaxExposure();
		}

		if(m_ShouldOverrideDofFocusDistanceBias)
		{
			sourceFrame.SetDofFocusDistanceBias(m_OverriddenDofFocusDistanceBias);
		}
		else
		{
			m_OverriddenDofFocusDistanceBias = sourceFrame.GetDofFocusDistanceBias();
		}

		if(m_ShouldOverrideDofMaxNearInFocusDistance)
		{
			sourceFrame.SetDofMaxNearInFocusDistance(m_OverriddenDofMaxNearInFocusDistance);
		}
		else
		{
			m_OverriddenDofMaxNearInFocusDistance = sourceFrame.GetDofMaxNearInFocusDistance();
		}

		if(m_ShouldOverrideDofMaxNearInFocusDistanceBlendLevel)
		{
			sourceFrame.SetDofMaxNearInFocusDistanceBlendLevel(m_OverriddenDofMaxNearInFocusDistanceBlendLevel);
		}
		else
		{
			m_OverriddenDofMaxNearInFocusDistanceBlendLevel = sourceFrame.GetDofMaxNearInFocusDistanceBlendLevel();
		}

		if(m_ShouldOverrideDofMaxBlurRadiusAtNearInFocusLimit)
		{
			sourceFrame.SetDofMaxBlurRadiusAtNearInFocusLimit(m_OverriddenDofMaxBlurRadiusAtNearInFocusLimit);
		}
		else
		{
			m_OverriddenDofMaxBlurRadiusAtNearInFocusLimit = sourceFrame.GetDofMaxBlurRadiusAtNearInFocusLimit();
		}

		if(m_ShouldOverrideDofBlurDiskRadiusPowerFactorNear)
		{
			sourceFrame.SetDofBlurDiskRadiusPowerFactorNear(m_OverriddenDofBlurDiskRadiusPowerFactorNear);
		}
		else
		{
			m_OverriddenDofBlurDiskRadiusPowerFactorNear = sourceFrame.GetDofBlurDiskRadiusPowerFactorNear();
		}

		if(m_ShouldOverrideDofFocusDistanceIncreaseSpringConstant)
		{
			sourceFrame.SetDofFocusDistanceIncreaseSpringConstant(m_OverriddenDofFocusDistanceIncreaseSpringConstant);
		}
		else
		{
			m_OverriddenDofFocusDistanceIncreaseSpringConstant = sourceFrame.GetDofFocusDistanceIncreaseSpringConstant();
		}

		if(m_ShouldOverrideDofFocusDistanceDecreaseSpringConstant)
		{
			sourceFrame.SetDofFocusDistanceDecreaseSpringConstant(m_OverriddenDofFocusDistanceDecreaseSpringConstant);
		}
		else
		{
			m_OverriddenDofFocusDistanceDecreaseSpringConstant = sourceFrame.GetDofFocusDistanceDecreaseSpringConstant();
		}

		if(m_ShouldOverrideDofPlanesBlendLevel)
		{
			sourceFrame.SetDofPlanesBlendLevel(m_OverriddenDofPlanesBlendLevel);
		}
		else
		{
			m_OverriddenDofPlanesBlendLevel = sourceFrame.GetDofPlanesBlendLevel();
		}

		if(m_ShouldOverrideDofFullScreenBlurBlendLevel)
		{
			sourceFrame.SetDofFullScreenBlurBlendLevel(m_OverriddenDofFullScreenBlurBlendLevel);
		}
		else
		{
			m_OverriddenDofFullScreenBlurBlendLevel = sourceFrame.GetDofFullScreenBlurBlendLevel();
		}

		if(m_ShouldOverrideDofOverriddenFocusDistance)
		{
			sourceFrame.SetDofOverriddenFocusDistance(m_OverriddenDofOverriddenFocusDistance);
		}
		else
		{
			m_OverriddenDofOverriddenFocusDistance = sourceFrame.GetDofOverriddenFocusDistance();
		}

		if(m_ShouldOverrideDofOverriddenFocusDistanceBlendLevel)
		{
			sourceFrame.SetDofOverriddenFocusDistanceBlendLevel(m_OverriddenDofOverriddenFocusDistanceBlendLevel);
		}
		else
		{
			m_OverriddenDofOverriddenFocusDistanceBlendLevel = sourceFrame.GetDofOverriddenFocusDistanceBlendLevel();
		}
#endif // __BANK

		bool wasEntityInvalidDueToNoLos = false;
		const CEntity* attachEntityForViewport = ComputeAttachEntityForViewport(sourceFrame, activeCamera, wasEntityInvalidDueToNoLos);

#if !__NO_OUTPUT
		if(wasEntityInvalidDueToNoLos)
		{
			cameraDebugf3("No LOS from camera to attach entity. NULLing viewport attach entity.");
		}
#endif // !__NO_OUTPUT

#if __BANK
		m_DebugHasAttachEntityForViewport		= (attachEntityForViewport != NULL);
		m_DebugWasAttachEntityInvalidDueToNoLos	= wasEntityInvalidDueToNoLos;
#endif // __BANK

		destinationViewport.SetAttachEntity(attachEntityForViewport);
		destinationViewport.PropagateCameraFrame(sourceFrame);
	}

	return isFrameValid;
}

bool camFramePropagator::ComputeIsFrameValid(const camFrame& frame, const camBaseCamera* OUTPUT_ONLY(activeCamera))
{
	bool isFrameValid = false;

	const int maxErrorLength = 128;
	char errorString[maxErrorLength];

	//Just validate the frame position for now.

	const Vector3& position = frame.GetPosition();
	if(position.IsNonZero())
	{
		//NOTE: We explicitly allow the free camera to travel beyond the world limits for debug purposes.
		if(g_CameraWorldLimits_AABB.ContainsPoint(VECTOR3_TO_VEC3V(position))
			NOTFINAL_ONLY( || (activeCamera && activeCamera->GetIsClassId(camFreeCamera::GetStaticClassId()))))
		{
			isFrameValid = true;
		}
		else
		{
			formatf(errorString, maxErrorLength, "The active camera is positioned outside the (expanded) world limits (%f, %f, %f)",
				position.x, position.y, position.z);
		}
	}
	else
	{
		safecpy(errorString, "The active camera is at the origin", maxErrorLength);
	}

#if !__NO_OUTPUT
	const char* cameraName = activeCamera ? activeCamera->GetName() : NULL;
#endif // !__FINAL

	if(isFrameValid)
	{
		if(m_NumUpdatesBypassed > 0)
		{
			cameraWarningf("camFramePropagator [%s]: The active camera frame is valid again - %u viewport update(s) bypassed",
				(cameraName ? cameraName : "UNKNOWN"), m_NumUpdatesBypassed);

			m_NumUpdatesBypassed = 0;
		}
	}
	else
	{
		if(m_NumUpdatesBypassed == 0)
		{
			cameraWarningf("camFramePropagator [%s]: %s - starting to bypass the viewport update", (cameraName ? cameraName : "UNKNOWN"),
				errorString);
		}

		m_NumUpdatesBypassed++;
	}

	return isFrameValid;
}

void camFramePropagator::UpdateFarClipFromTimeCycle(camFrame& frame)
{
	float farClip = frame.GetFarClip();

	const bool shouldIgnoreTimeCycleFarClip	= frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldIgnoreTimeCycleFarClip);
	const float timeCycleFarClip			= g_timeCycle.GetStaleFarClip();

	if(!shouldIgnoreTimeCycleFarClip && (timeCycleFarClip > 0.0f))
// 		cameraVerifyf(timeCycleFarClip > 0.0f, "The time-cycle specified an invalid far-clip distance (%fm), so bypassing the override",
// 		timeCycleFarClip))
	{
		farClip = timeCycleFarClip;
	}
	else if(!cameraVerifyf(farClip > 0.0f, "The camera system specified an invalid far-clip distance (%fm), so reseting to the default of %fm",
		farClip, g_DefaultFarClip))
	{
		farClip = g_DefaultFarClip; //For safety, reset to the default.
	}

	frame.SetFarClip(farClip);
}

//The viewport attach entity is used by portal visibility trackers. So we must ensure that there is no map collision between the camera and this entity,
//otherwise the portal traversal logic will be undermined.
const CEntity* camFramePropagator::ComputeAttachEntityForViewport(const camFrame& sourceFrame, const camBaseCamera* activeCamera, bool& wasEntityInvalidDueToNoLos) const
{
	wasEntityInvalidDueToNoLos = false;

	if(!activeCamera)
	{
		return NULL;
	}

	const CEntity* cameraAttachEntity = activeCamera->GetAttachParent();
	if(!cameraAttachEntity || !cameraAttachEntity->GetIsDynamic())
	{
		return NULL;
	}

	const CPortalTracker* portalTracker = static_cast<const CDynamicEntity*>(cameraAttachEntity)->GetPortalTracker();
	if(!portalTracker)
	{
		return NULL;
	}

	WorldProbe::CShapeTestProbeDesc lineTest;
	lineTest.SetIsDirected(true);
	lineTest.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
	lineTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	lineTest.SetContext(WorldProbe::LOS_Camera);
	lineTest.SetExcludeEntity(cameraAttachEntity, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);

	const Vector3& cameraPosition = sourceFrame.GetPosition();
	lineTest.SetStartAndEnd(cameraPosition, portalTracker->GetCurrentPos());

	wasEntityInvalidDueToNoLos = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);

	const CEntity* attachEntityForViewport = wasEntityInvalidDueToNoLos ? NULL : cameraAttachEntity;

	return attachEntityForViewport;
}

bool camFramePropagator::ComputeAndPropagateRevisedFarClipFromTimeCycle(camFrame& sourceFrame, CViewport& destinationViewport)
{
	if(sourceFrame.GetFlags().IsFlagSet(camFrame::Flag_ShouldIgnoreTimeCycleFarClip))
	{
		return false;
	}

	const float initialFarClip = sourceFrame.GetFarClip();

	g_timeCycle.Update(false);

	UpdateFarClipFromTimeCycle(sourceFrame);

	const float revisedFarClip = sourceFrame.GetFarClip();

	const bool hasFarClipChanged = !IsClose(initialFarClip, revisedFarClip);
	if(hasFarClipChanged)
	{
		destinationViewport.PropagateRevisedCameraFarClip(revisedFarClip);
	}

	return hasFarClipChanged;
}

#if __BANK
bool camFramePropagator::ComputeDebugOutputText(char* string, u32 maxLength) const
{
	if(m_ShouldDisplayViewportAttachEntityStatus)
	{
		safecpy(string, m_DebugHasAttachEntityForViewport ? "Viewport: Valid attach entity" :
			(m_DebugWasAttachEntityInvalidDueToNoLos ? "Viewport: No valid attach entity (no LOS from camera)" : "Viewport: No attach entity"), maxLength);
	}
	else
	{
		string[0] = '\0';
	}

	return m_ShouldDisplayViewportAttachEntityStatus;
}

void camFramePropagator::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Frame propagator", false);
	{
		bank.AddSlider("Min motion blur strength",			&ms_MinMotionBlurStrength,				0.0f, 1.0f, 0.0001f);

		bank.AddToggle("Override near clip",				&m_ShouldOverrideNearClip);
		bank.AddSlider("Overridden near clip",				&m_OverriddenNearClip,					g_MinNearClip, 10.0f, 0.01f);

		bank.AddToggle("Override far clip",					&m_ShouldOverrideFarClip);
		bank.AddSlider("Overridden far clip",				&m_OverriddenFarClip,					-9999.0f, 9999.0f, 10.0f);

		bank.AddToggle("Override near out-of-focus DOF",	&m_ShouldOverrideDofPlanes[camFrame::NEAR_OUT_OF_FOCUS_DOF_PLANE]);
		bank.AddSlider("Overridden near out-of-focus DOF",	&m_OverriddenDofPlanes[camFrame::NEAR_OUT_OF_FOCUS_DOF_PLANE],	-9999.0f, 9999.0f, 0.01f);

		bank.AddToggle("Override near in-focus DOF",		&m_ShouldOverrideDofPlanes[camFrame::NEAR_IN_FOCUS_DOF_PLANE]);
		bank.AddSlider("Overridden near in-focus DOF",		&m_OverriddenDofPlanes[camFrame::NEAR_IN_FOCUS_DOF_PLANE],		-9999.0f, 9999.0f, 0.01f);

		bank.AddToggle("Override far in-focus DOF",			&m_ShouldOverrideDofPlanes[camFrame::FAR_IN_FOCUS_DOF_PLANE]);
		bank.AddSlider("Overridden far in-focus DOF",		&m_OverriddenDofPlanes[camFrame::FAR_IN_FOCUS_DOF_PLANE],		-9999.0f, 9999.0f, 0.01f);

		bank.AddToggle("Override far out-of-focus DOF",		&m_ShouldOverrideDofPlanes[camFrame::FAR_OUT_OF_FOCUS_DOF_PLANE]);
		bank.AddSlider("Overridden far out-of-focus DOF",	&m_OverriddenDofPlanes[camFrame::FAR_OUT_OF_FOCUS_DOF_PLANE],	-9999.0f, 9999.0f, 0.01f);

		bank.AddToggle("Override DOF strength",				&m_ShouldOverrideDofStrength);
		bank.AddSlider("Overridden DOF strength",			&m_OverriddenDofStrength,				0.0f, 1.0f, 0.01f);

		bank.AddToggle("Override high-quality DOF",			&m_ShouldOverrideUseHighQualityDof);
		bank.AddToggle("Overridden high-quality DOF state",	&m_OverriddenUseHighQualityDofState);

		bank.AddToggle("Override shallow DOF",				&m_ShouldOverrideUseShallowDof);
		bank.AddToggle("Overridden shallow DOF state",		&m_OverriddenUseShallowDofState);

		bank.AddToggle("Override FOV",						&m_ShouldOverrideFov);
		bank.AddSlider("Overridden FOV",					&m_OverriddenFov,						-9999.0f, 9999.0f, 0.2f);

		bank.AddToggle("Override motion blur",				&m_ShouldOverrideMotionBlurStrength);
		bank.AddSlider("Overridden motion blur",			&m_OverriddenMotionBlurStrength,		-9999.0f, 9999.0f, 0.1f);

		bank.AddToggle("Display viewport attach entity status", &m_ShouldDisplayViewportAttachEntityStatus);

		bank.PushGroup("Adaptive DOF settings", false);
		{
			bank.AddToggle("Override Focus Distance Grid Scaling",				&m_ShouldOverrideDofFocusDistanceGridScaling);
			bank.AddSlider("Focus Distance Grid Scaling",						&m_OverriddenDofFocusDistanceGridScaling,				0.0f, 1.0f, 0.01f);

			bank.AddToggle("Override Subject Magnification Power Near/Far",		&m_ShouldOverrideDofSubjectMagPowerNearFar);
			bank.AddSlider("Subject Magnification Power Near/Far",				&m_OverriddenDofSubjectMagPowerNearFar,					0.0f, 10.0f, 0.01f);

			bank.AddToggle("Override Max Pixel Depth",							&m_ShouldOverrideDofMaxPixelDepth);
			bank.AddSlider("Max Pixel Depth",									&m_OverriddenDofMaxPixelDepth,							0.0f, 99999.0f, 0.1f);

			bank.AddToggle("Override Max Pixel Depth Blend Level",				&m_ShouldOverrideDofMaxPixelDepthBlendLevel);
			bank.AddSlider("Max Pixel Depth Blend Level",						&m_OverriddenDofMaxPixelDepthBlendLevel,				0.0f, 1.0f, 0.01f);

			bank.AddToggle("Override Pixel Depth Power Factor",					&m_ShouldOverrideDofPixelDepthPowerFactor);
			bank.AddSlider("Pixel Depth Power Factor",							&m_OverriddenDofPixelDepthPowerFactor,					0.0f, 1.0f, 0.01f);

			bank.AddToggle("Override F-Number Of Lens",							&m_ShouldOverrideFNumberOfLens);
			bank.AddSlider("F-Number Of Lens",									&m_OverriddenFNumberOfLens,								0.5f, 256.0f, 0.01f);

			bank.AddToggle("Override Focal Length Multiplier",					&m_ShouldOverrideFocalLengthMultiplier);
			bank.AddSlider("Focal Length Multiplier",							&m_OverriddenFocalLengthMultiplier,						0.1f, 10.0f, 0.01f);

			bank.AddToggle("Override F-Stops At Max Exposure",					&m_ShouldOverrideDofFStopsAtMaxExposure);
			bank.AddSlider("F-Stops At Max Exposure",							&m_OverriddenDofFStopsAtMaxExposure,					0.0f, 10.0f, 0.01f);

			bank.AddToggle("Override Focus Distance Bias",						&m_ShouldOverrideDofFocusDistanceBias);
			bank.AddSlider("Focus Distance Bias",								&m_OverriddenDofFocusDistanceBias,						-100.0f, 100.0f, 0.01f);

			bank.AddToggle("Override Max Near In-Focus Distance",				&m_ShouldOverrideDofMaxNearInFocusDistance);
			bank.AddSlider("Max Near In-Focus Distance",						&m_OverriddenDofMaxNearInFocusDistance,					0.0f, 99999.0f, 0.1f);

			bank.AddToggle("Override Max Near In-Focus Distance Blend Level",	&m_ShouldOverrideDofMaxNearInFocusDistanceBlendLevel);
			bank.AddSlider("Max Near In-Focus Distance Blend Level",			&m_OverriddenDofMaxNearInFocusDistanceBlendLevel,		0.0f, 1.0f, 0.01f);

			bank.AddToggle("Override Max Blur Radius At Near In-Focus Limit",	&m_ShouldOverrideDofMaxBlurRadiusAtNearInFocusLimit);
			bank.AddSlider("Max Blur Radius At Near In-Focus Limit",			&m_OverriddenDofMaxBlurRadiusAtNearInFocusLimit,		0.0f, 30.0f, 0.01f);

			bank.AddToggle("Override Blur Disk Radius Power Factor Near",		&m_ShouldOverrideDofBlurDiskRadiusPowerFactorNear);
			bank.AddSlider("Blur Disk Radius Power Factor Near",				&m_OverriddenDofBlurDiskRadiusPowerFactorNear,			0.0f, 10.0f, 0.01f);

			bank.AddToggle("Override Focus Distance Increase Spring Constant",	&m_ShouldOverrideDofFocusDistanceIncreaseSpringConstant);
			bank.AddSlider("Focus Distance Increase Spring Constant",			&m_OverriddenDofFocusDistanceIncreaseSpringConstant,	0.0f, 1000.0f, 0.1f);

			bank.AddToggle("Override Focus Distance Decrease Spring Constant",	&m_ShouldOverrideDofFocusDistanceDecreaseSpringConstant);
			bank.AddSlider("Focus Distance Decrease Spring Constant",			&m_OverriddenDofFocusDistanceDecreaseSpringConstant,	0.0f, 1000.0f, 0.1f);

			bank.AddToggle("Override DOF Planes Blend Level",					&m_ShouldOverrideDofPlanesBlendLevel);
			bank.AddSlider("DOF Planes Blend Level",							&m_OverriddenDofPlanesBlendLevel,						0.0f, 1.0f, 0.01f);

			bank.AddToggle("Override Full-Screen Blur Blend Level",				&m_ShouldOverrideDofFullScreenBlurBlendLevel);
			bank.AddSlider("Full-Screen Blur Blend Level",						&m_OverriddenDofFullScreenBlurBlendLevel,				0.0f, 1.0f, 0.01f);

			bank.AddToggle("Override Overridden Focus Distance",				&m_ShouldOverrideDofOverriddenFocusDistance);
			bank.AddSlider("Overridden Focus Distance",							&m_OverriddenDofOverriddenFocusDistance,				0.0f, 99999.0f, 0.01f);

			bank.AddToggle("Override Overridden Focus Distance Blend Level",	&m_ShouldOverrideDofOverriddenFocusDistanceBlendLevel);
			bank.AddSlider("Overridden Focus Distance Blend Level",				&m_OverriddenDofOverriddenFocusDistanceBlendLevel,		0.0f, 1.0f, 0.01f);
		}
		bank.PopGroup(); //Adaptive DOF settings

		if(m_NearClipScanner)
		{
			m_NearClipScanner->AddWidgetsForInstance();
		}
	}
	bank.PopGroup(); //Frame propagator.
}
#endif
