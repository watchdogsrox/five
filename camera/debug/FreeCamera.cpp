//
// camera/debug/FreeCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#if !__FINAL //No debug cameras in final builds.

#include "camera/debug/FreeCamera.h"

#include "system/param.h"

#include "fwdebug/picker.h"

#include "bank/data.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/FramePropagator.h"
#include "camera/helpers/NearClipScanner.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/TiledScreenCapture.h"
#include "Frontend/PauseMenu.h"
#include "peds/Ped.h"
#include "physics/physics.h"
#include "renderer/lights/lights.h"
#include "scene/EntityIterator.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "system/controlMgr.h"
#include "event/EventNetwork.h"
#include "event/EventGroup.h"
#include "text/messages.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFreeCamera,0x89B66D3)

PARAM(rdrDebugCamera, "Enable RDR debug camera");
PARAM(debugCameraStreamingFocus, "Use the debug camera as the streaming focus, when active");

#if __BANK
const float g_MaxDistanceToAttachParent = 10.0f; //TODO: Migrate into metadata.
#endif // __BANK

namespace
{
#if __BANK
	static bool	 DEBUG_PERSIST_SPEED_MULTIPLIERS = false;
#endif
	static float GLOBAL_SPEED_MULTIPLIER = 1.0f;
	static float MOVEMENT_SPEED_MULTIPLIER = 1.0f;
	static float ROTATION_SPEED_MULTIPLIER = 1.0f;
}

camFreeCamera::camFreeCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camFreeCameraMetadata&>(metadata))
, m_LeftStickEnabled(true)
, m_RightStickEnabled(true)
, m_CrossEnabled(true)
, m_DPadDownEnabled(true)
, m_DPadLeftEnabled(true)
, m_DPadRightEnabled(true)
, m_DPadUpEnabled(true)
, m_LeftShoulder1Enabled(true)
, m_LeftShoulder2Enabled(true)
, m_LeftShockEnabled(true)
, m_RightShoulder1Enabled(true)
, m_RightShoulder2Enabled(true)
, m_RightShockEnabled(true)
, m_SquareEnabled(true)	
, m_TriangleEnabled(true)
, m_ShouldUseRdrMapping(PARAM_rdrDebugCamera.Get())
, m_ShouldOverrideStreamingFocus(PARAM_debugCameraStreamingFocus.Get())
#if __BANK
, m_DebugOrientation(VEC3_ZERO)
, m_DebugLightColour(VEC3_IDENTITY)
, m_DebugLightPosition(VEC3_ZERO)
, m_DebugLightDirection(ZAXIS)
, m_DebugLightTangent(YAXIS)
, m_DebugLightRange(10.0f)
, m_DebugLightIntensity(10.0f)
, m_DebugOverridenNearClip(g_MinNearClip)
, m_IsDebugLightActive(false)
, m_IsDebugLightAttached(true)
, m_ShouldUseADebugRimLight(false)
, m_ShouldDebugOverrideNearClip(false)
, m_DebugMountRelativePosition(VEC3_ZERO)
, m_DebugMountRelativeHeading(0.0f)
, m_DebugMountRelativePitch(0.0f)
, m_ShouldDebugMountPlayer(false)
, m_ShouldDebugMountPickerSelection(false)
, m_ShouldDebugMountNearestPedVehicleOrObject(false)
, m_CameraWidgetData(NULL)
#endif // __BANK
{
	Reset();
}

#if __BANK
camFreeCamera::~camFreeCamera()
{
	m_CameraWidgetData = NULL; 
}
#endif
// ¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦
// ¦ Left Stick			- Rotate camera
// ¦ [],X				- Move forward / backward.
// ¦ Right Stick		- N S E W movement.
// ¦ L1 & Right Stick	- Movement in the plane of the projection plane.
// ¦ Triangle			- Allow roll with L1 and R1 (L1 & R1 resets roll)
// ¦ R2					- Warp player to camera position.
// ¦ L3					- Select movement speed slow, normal, fast 
// ¦ R2					- Drop player.
// ¦
// ¦ If RDR controls are enabled, use RDR control scheme:
// ¦ Left Stick			- Move forward / backward & strafe left / right
// ¦ Right Stick		- Rotate Camera
// ¦ L1 & Left Stick	- N S E W movement. (Left stick left/right = W/E, buttons X/A = N/S)
// ¦ Triangle			- (Same as above) Allow roll with L1 and R1 (L1 & R1 resets roll).
// ¦ DPad				- (Same as above) left/right changes FOV. Down resets FOV. Up toggles slow movement
// ¦ R2					- Same as above
// ¦
// ¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦¦
bool camFreeCamera::Update()
{
#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		return true;
	}
#endif // __BANK

	CPad& pad = CControlMgr::GetDebugPad();
	CPad* playerPad = CControlMgr::GetPlayerPad();

	//Allow toggling of whether to override the streaming focus with the free camera position and velocity on Start+Triangle/Y. 
	const bool shouldToggleOverrideStreamingFocus = playerPad && (playerPad->GetPressedDebugButtons() & ioPad::RUP);
	if(shouldToggleOverrideStreamingFocus)
	{
		m_ShouldOverrideStreamingFocus = !m_ShouldOverrideStreamingFocus;

#if __BANK
		const char* stringLabel	= m_ShouldOverrideStreamingFocus ? "HELP_DBGCMSFON" : "HELP_DBGCMSFOFF";
		char *string			= TheText.Get(stringLabel);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string);
#endif // __BANK
	}

	m_Frame.GetFlags().ChangeFlag(camFrame::Flag_ShouldOverrideStreamingFocus, m_ShouldOverrideStreamingFocus);

	const bool isDebugStartDown = (pad.GetDebugButtons() & ioPad::START) || (playerPad && (playerPad->GetDebugButtons() & ioPad::START));
	if(isDebugStartDown)
	{
		//Early-out if the start button is held down, on either pad, as special debug buttons may be pressed in combination with this.
		//We do not want this debug input to interact with the camera controls.
		return true;
	}

	float translationSpeedScale[3] = {0.1f, 1.0f, 8.0f};
	float rotationSpeedScale[3] = {0.3f, 1.0f, 1.0f};

	//Capture stick input:
	Vector2 translateInput;
	Vector2 rotateInput;
	translateInput.x = translateInput.y = rotateInput.x = rotateInput.y = 0.0f;

	const float maxStickValue = 128.0f;

	if(!m_ShouldUseRdrMapping)
	{
		if(m_RightStickEnabled)
		{
			translateInput.x	= (float)pad.GetRightStickX() / maxStickValue;
			translateInput.y	= (float)-pad.GetRightStickY() / maxStickValue;
		}
		
		if(m_LeftStickEnabled)
		{
			rotateInput.x		= (float)pad.GetLeftStickX() / maxStickValue;
			rotateInput.y		= (float)-pad.GetLeftStickY() / maxStickValue;
		}

		if(m_LeftShockEnabled && pad.ShockButtonLJustDown())	//select movement speed (3 speeds)
		{
#if !__NO_OUTPUT
			const char* logString[3] = {"SLOW", "NORMAL", "FAST"};
#endif
			m_MovementSpeed ++;
			if(m_MovementSpeed > 2)
				m_MovementSpeed = 0;

			cameraDisplayf("%s free camera", logString[m_MovementSpeed]);
		}
	}
	else
	{
		if(m_LeftStickEnabled)
		{
			translateInput.x		= (float)pad.GetLeftStickX() / maxStickValue;
		}

		if(m_LeftShoulder2Enabled && pad.GetLeftShoulder2() && m_RightStickEnabled)
		{
			//Holding left trigger reserves the right-stick for vertical translation input.
			translateInput.y	= -(float)pad.GetRightStickY() / maxStickValue;
			rotateInput.Zero();
		}
		else
		{
			translateInput.y	= Clamp(static_cast<float>(pad.GetButtonSquare()), 0.0f, 1.0f) + -Clamp(static_cast<float>(pad.GetButtonCross()), 0.0f, 1.0f);

			if(m_RightStickEnabled)
			{
				float invertYLook	= (CPauseMenu::GetMenuPreference(PREF_INVERT_LOOK)) ? -1.0f : 1.0f;
				rotateInput.x		= (float)pad.GetRightStickX() / maxStickValue;
				rotateInput.y		= invertYLook * ((float)pad.GetRightStickY() / maxStickValue);
			}
		}

		if(pad.GetShockButtonL())
			m_MovementSpeed = 2;
		else if(pad.GetShockButtonR())
			m_MovementSpeed = 0;
		else
			m_MovementSpeed = 1;
	}
	if(m_MovementSpeed == 0)
		m_OverrideNearClip = true;
	else
		m_OverrideNearClip = false;

	float timeStep = fwTimer::GetNonPausableCamTimeStep();

	UpdateMouseControl(timeStep);

	timeStep *= GLOBAL_SPEED_MULTIPLIER;
	float forwardSpeedDelta		= m_Metadata.m_ForwardAcceleration * timeStep * MOVEMENT_SPEED_MULTIPLIER;
	float strafeSpeedDelta		= m_Metadata.m_StrafeAcceleration * timeStep * MOVEMENT_SPEED_MULTIPLIER;
	float verticalSpeedDelta	= m_Metadata.m_VerticalAcceleration * timeStep * MOVEMENT_SPEED_MULTIPLIER;
	float maxTranslationSpeed	= m_Metadata.m_MaxTranslationSpeed;

	float headingSpeedDelta		= m_Metadata.m_HeadingAcceleration * DtoR * timeStep * ROTATION_SPEED_MULTIPLIER;
	float pitchSpeedDelta		= m_Metadata.m_PitchAcceleration * DtoR * timeStep * ROTATION_SPEED_MULTIPLIER;
	float rollSpeedDelta		= m_Metadata.m_RollAcceleration * DtoR * timeStep * ROTATION_SPEED_MULTIPLIER;
	float fovSpeedDelta			= m_Metadata.m_FovAcceleration * timeStep;

	forwardSpeedDelta	*= translationSpeedScale[m_MovementSpeed];
	strafeSpeedDelta	*= translationSpeedScale[m_MovementSpeed];
	verticalSpeedDelta	*= translationSpeedScale[m_MovementSpeed];
	maxTranslationSpeed *= translationSpeedScale[m_MovementSpeed];

	headingSpeedDelta	*= rotationSpeedScale[m_MovementSpeed];
	pitchSpeedDelta		*= rotationSpeedScale[m_MovementSpeed];
	rollSpeedDelta		*= rotationSpeedScale[m_MovementSpeed];

	//Move forward.
	if(!m_ShouldUseRdrMapping)
	{
		if(m_SquareEnabled && pad.GetButtonSquare())
		{
			m_ForwardSpeed += forwardSpeedDelta;
		}
		else if(m_CrossEnabled && pad.GetButtonCross())
		{
			m_ForwardSpeed -= forwardSpeedDelta;
		}
		else
		{
			m_ForwardSpeed = 0.0f; //Stop instantly.
		}

		m_ForwardSpeed = Clamp(m_ForwardSpeed, -maxTranslationSpeed, maxTranslationSpeed);
	}
	else
	{
		if(m_LeftStickEnabled)
		{
			float stickVal = -static_cast<float>(pad.GetLeftStickY()) / maxStickValue;
			m_ForwardSpeed += Sign(stickVal) * forwardSpeedDelta;
			m_ForwardSpeed = Abs(stickVal) * Clamp(m_ForwardSpeed, -maxTranslationSpeed, maxTranslationSpeed);
		}
		else
		{
			m_ForwardSpeed = 0.0f;
		}
	}

	//Move sideways OR East/West.
	m_StrafeSpeed = m_StrafeSpeed + (Sign(translateInput.x) * strafeSpeedDelta);
	m_StrafeSpeed  = Abs(translateInput.x) * Clamp(m_StrafeSpeed, -maxTranslationSpeed, maxTranslationSpeed);

	//Move vertically OR North/South.
	m_VerticalSpeed = m_VerticalSpeed + (Sign(translateInput.y) * verticalSpeedDelta);
	m_VerticalSpeed  = Abs(translateInput.y) * Clamp(m_VerticalSpeed, -maxTranslationSpeed, maxTranslationSpeed);

	//Apply positional motion:
	Vector3 worldPosition;
#if __BANK
	if(m_DebugMountEntity)
	{
		worldPosition = VEC3V_TO_VECTOR3(m_DebugMountEntity->GetTransform().Transform(RCC_VEC3V(m_DebugMountRelativePosition)));
	}
	else
#endif // __BANK
	{
		const Matrix34& matrix = m_Frame.GetWorldMatrix();
		worldPosition = matrix.d + (matrix.b * m_ForwardSpeed * timeStep);

		if((!m_ShouldUseRdrMapping && (m_LeftShoulder1Enabled && pad.GetLeftShoulder1())) || (m_ShouldUseRdrMapping && !(m_LeftShoulder1Enabled && pad.GetLeftShoulder1())))
		{
			//Camera-relative (sideways/vertical.)
			worldPosition += matrix.a * m_StrafeSpeed * timeStep;
			worldPosition += matrix.c * m_VerticalSpeed * timeStep;
		}
		else
		{
			//World-relative (E&W / N&S.)
			worldPosition.x += m_StrafeSpeed * timeStep;
			worldPosition.y += m_VerticalSpeed * timeStep;
		}
	}

	m_Frame.SetPosition(worldPosition);

	//FOV modifier:
	float fov = m_Frame.GetFov();

	if(m_DPadDownEnabled && pad.GetDPadDown())
	{
		m_FovSpeed = 0.0f;	//Reset.
		fov = g_DefaultFov;
	}
	else if(m_DPadRightEnabled && pad.GetDPadRight())
	{
		m_FovSpeed += fovSpeedDelta;
	}
	else if(m_DPadLeftEnabled && pad.GetDPadLeft())
	{
		m_FovSpeed -= fovSpeedDelta;
	}
	else
	{
		m_FovSpeed = 0.0f;	//Stop instantly.
	}

	m_FovSpeed  = Clamp(m_FovSpeed, -m_Metadata.m_MaxFovSpeed, m_Metadata.m_MaxFovSpeed);
	fov += m_FovSpeed * timeStep;
	fov = Clamp(fov, g_MinFov, g_MaxFov);
	m_Frame.SetFov(fov);

	//Apply rotations:
	float heading;
	float pitch;
	float roll;
	m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

	//Heading:
	m_HeadingSpeed				= m_HeadingSpeed + (Sign(rotateInput.x) * headingSpeedDelta);
	m_HeadingSpeed				= Abs(rotateInput.x) * Clamp(m_HeadingSpeed, -m_Metadata.m_MaxRotationSpeed * DtoR, m_Metadata.m_MaxRotationSpeed * DtoR);
	const float headingOffset	= -m_HeadingSpeed * timeStep;

	//Pitch:
	m_PitchSpeed				= m_PitchSpeed + (Sign(rotateInput.y) * pitchSpeedDelta);
	m_PitchSpeed				= Abs(rotateInput.y) * Clamp(m_PitchSpeed, -m_Metadata.m_MaxRotationSpeed * DtoR, m_Metadata.m_MaxRotationSpeed * DtoR);
	const float pitchOffset		= -m_PitchSpeed * timeStep;

#if __BANK
	if(m_DebugMountEntity)
	{
		const fwTransform& debugMountTransform = m_DebugMountEntity->GetTransform();

		const float mountHeading	= debugMountTransform.GetHeading();
		const float mountPitch		= debugMountTransform.GetPitch();

		heading	= mountHeading + m_DebugMountRelativeHeading + headingOffset;
		heading	= fwAngle::LimitRadianAngleSafe(heading);

		pitch	= mountPitch + m_DebugMountRelativePitch + pitchOffset;
		pitch	= Clamp(pitch, -m_Metadata.m_MaxPitch * DtoR, m_Metadata.m_MaxPitch * DtoR);

		//Cache the relative orientation *after* the world-space orientation limits have been applied.
		m_DebugMountRelativeHeading	= fwAngle::LimitRadianAngleSafe(heading - mountHeading);
		m_DebugMountRelativePitch	= fwAngle::LimitRadianAngleSafe(pitch - mountPitch);
	}
	else
#endif // __BANK
	{
		heading	+= headingOffset;
		heading	= fwAngle::LimitRadianAngle(heading);

		pitch	+= pitchOffset;
		pitch	= Clamp(pitch, -m_Metadata.m_MaxPitch * DtoR, m_Metadata.m_MaxPitch * DtoR);
	}

	//Roll:
	if(m_TriangleEnabled && pad.GetButtonTriangle())
	{
		if(m_RightShoulder1Enabled && pad.GetRightShoulder1() && m_LeftShoulder1Enabled && pad.GetLeftShoulder1())
		{
			m_RollSpeed = 0.0f;	//Reset.
			roll = 0.0f;
		}
		else if(m_RightShoulder1Enabled && pad.GetRightShoulder1())
		{
			m_RollSpeed += rollSpeedDelta;
		}
		else if(m_LeftShoulder1Enabled && pad.GetLeftShoulder1())
		{
			m_RollSpeed -= rollSpeedDelta;
		}
		else
		{
			m_RollSpeed = 0.0f;	//Stop instantly.
		}
	}
	else 
	{
		m_RollSpeed = 0.0f;		//Stop instantly.
	}

	m_RollSpeed  = Clamp(m_RollSpeed, -m_Metadata.m_MaxRotationSpeed * DtoR, m_Metadata.m_MaxRotationSpeed * DtoR);
	roll += m_RollSpeed * timeStep;

	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);

#if __BANK
	//Cache the Euler angles, in degrees, for use in a RAG widget.
	m_DebugOrientation.Set(pitch, roll, heading);
	m_DebugOrientation.Scale(RtoD);
#endif // __BANK

	if (m_RightShoulder2Enabled)
	{
		if (pad.GetRightShoulder2())
		{
			m_NumUpdatesToPushNearClip = m_Metadata.m_NumUpdatesToPushNearClipWhenTeleportingFollowPed;

			TeleportTarget(worldPosition, false);

			if(m_LeftShoulder1Enabled && pad.GetLeftShoulder1())
			{
				//Quick exit form debug cam.
				camInterface::GetDebugDirector().DeactivateFreeCam();

				if (NetworkInterface::IsGameInProgress())
				{
					GetEventScriptNetworkGroup()->Add(CEventNetworkCheatTriggered(CEventNetworkCheatTriggered::CHEAT_WARP));
				}
			}
		}
		else if (pad.RightShoulder2JustUp() && NetworkInterface::IsGameInProgress())
		{
			GetEventScriptNetworkGroup()->Add(CEventNetworkCheatTriggered(CEventNetworkCheatTriggered::CHEAT_WARP));
		}
	}

	//Push the near-clip out to the maximum supported by the near-clip scanner, effectively bypassing the scanner.
	const bool shouldPushNearClip = (m_NumUpdatesToPushNearClip > 0);
	if(shouldPushNearClip)
	{
		m_NumUpdatesToPushNearClip--;
	}

	float nearClipToApply	= shouldPushNearClip && camManager::GetFramePropagator().GetNearClipScanner() ?
								camManager::GetFramePropagator().GetNearClipScanner()->ComputeMaxNearClipAtPosition(worldPosition) :
								m_ClonedFrame.GetNearClip();
#if __BANK
	if(m_ShouldDebugOverrideNearClip || m_OverrideNearClip)
	{
		nearClipToApply = m_DebugOverridenNearClip;
	}
#endif // __BANK

	m_Frame.SetNearClip(nearClipToApply);

#if __BANK
	//Add the debug light that can be attached and detached by the widgets:
	if(m_IsDebugLightActive)
	{		
		if(m_IsDebugLightAttached)
		{
			//Move the light with the camera frame.
			const Vector3 lightDir(0.0f, 1.0f, 0.0f);
			const Vector3 lightTan(1.0f, 0.0f, 0.0f);

			m_Frame.GetWorldMatrix().Transform3x3(lightDir, m_DebugLightDirection);
			m_Frame.GetWorldMatrix().Transform3x3(lightTan, m_DebugLightTangent);

			m_DebugLightPosition = worldPosition;
		}

		//It looks like this light must be added every update and is automatically removed when we no longer add it.
		CLightSource light(LIGHT_TYPE_SPOT, LIGHTFLAG_CUTSCENE, m_DebugLightPosition, m_DebugLightColour, m_DebugLightIntensity, LIGHT_ALWAYS_ON);
		light.SetDirTangent(m_DebugLightDirection, m_DebugLightTangent);
		light.SetRadius(m_DebugLightRange);
		light.SetSpotlight(m_DebugLightRange * 0.5f, m_DebugLightRange);
		Lights::AddSceneLight(light);
	}
#endif  // __BANK

	// called every 'frame' since we don't want any dodgy code forgetting to renable a control because that would be annoying.
	ReEnableAllControls();

	return true;
}

//TODO: Mouse support is a bit weird looking. Rework or remove.
void camFreeCamera::UpdateMouseControl(const float timeStep)
{
	if(!m_IsMouseEnabled)
	{
		return;
	}

	const s32 mouseX = ioMouse::GetX();
	const s32 mouseY = ioMouse::GetY();
	const s32 mouseButtons = ioMouse::GetButtons();

	float scaleX = (float)gVpMan.GetGameViewport()->GetGrcViewport().GetWidth() / (float)grcViewport::GetDefaultScreen()->GetWidth();
	float scaleY = (float)gVpMan.GetGameViewport()->GetGrcViewport().GetHeight() / (float)grcViewport::GetDefaultScreen()->GetHeight();

	phSegment groundSegment;
	gVpMan.GetGameViewport()->GetGrcViewport().ReverseTransform(mouseX * scaleX, mouseY * scaleY, (Vec3V&)groundSegment.A, (Vec3V&)groundSegment.B);
	Vector3 groundPosition = Lerp(groundSegment.A.z / (groundSegment.A.z - groundSegment.B.z), groundSegment.A, groundSegment.B);

	if(mouseButtons && !m_IsMouseButtonDown)
	{
		m_CachedMouseX = mouseX;
		m_CachedMouseY = mouseY;

		//Test for collision with the map between the centres of the near and far clip planes.
		phSegment screenCentreSegment;
		gVpMan.GetGameViewport()->GetGrcViewport().ReverseTransform(0.5f * gVpMan.GetGameViewport()->GetGrcViewport().GetWidth(),
			0.5f * gVpMan.GetGameViewport()->GetGrcViewport().GetHeight(), (Vec3V&)screenCentreSegment.A, (Vec3V&)screenCentreSegment.B);
		phIntersection intersect;
		if (CPhysics::GetLevel()->TestProbe(screenCentreSegment, &intersect, 0, ArchetypeFlags::GTA_ALL_MAP_TYPES))
		{
			m_OrbitPosition = RCC_VECTOR3(intersect.GetPosition());
		}
		else
		{
			m_OrbitPosition = Lerp(screenCentreSegment.A.z / (screenCentreSegment.A.z - screenCentreSegment.B.z), screenCentreSegment.A, screenCentreSegment.B);
		}
		m_OrbitDistance = (m_OrbitPosition - m_Frame.GetPosition()).Mag();

		if((mouseButtons & ioMouse::MOUSE_LEFT) && (m_NumUpdatesMouseButtonUp < 15))
		{
			//Double click to teleport.
			Vector3 teleportPosition;
			if(CPhysics::GetLevel()->TestProbe(groundSegment, &intersect, 0, ArchetypeFlags::GTA_ALL_MAP_TYPES))
			{
				teleportPosition = RCC_VECTOR3(intersect.GetPosition());
				teleportPosition.z += 1.0f;
			}
			else
			{
				teleportPosition = groundPosition;
				teleportPosition.z += 10.0f;
			}

			//Drop target ped/vehicle.
			TeleportTarget(teleportPosition);
		}
	}
	else
	{
		if(mouseButtons & ioMouse::MOUSE_LEFT)
		{
			m_NumUpdatesMouseButtonUp = 0;
		}
		else
		{
			m_NumUpdatesMouseButtonUp++;
		}

		if((m_CachedMouseX != mouseX) || (m_CachedMouseY != mouseY))
		{
			//Invalidate double-click.
			m_NumUpdatesMouseButtonUp = 100;
		}
	}

	Vector3 worldPosition = m_Frame.GetPosition();

	if(mouseButtons & ioMouse::MOUSE_LEFT)
	{
		//Rotate:
		float heading;
		float pitch;
		float roll;
		m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

		const float headingSpeedDelta = m_Metadata.m_HeadingAcceleration * DtoR * timeStep;
		heading -= (mouseX - m_CachedMouseX) * headingSpeedDelta * 3.0f;
		heading  = fwAngle::LimitRadianAngleSafe(heading);

		const float pitchSpeedDelta = m_Metadata.m_PitchAcceleration * DtoR * timeStep;
		pitch -= (mouseY - m_CachedMouseY) * pitchSpeedDelta * 3.0f;
		pitch = Clamp(pitch, -m_Metadata.m_MaxPitch * DtoR, m_Metadata.m_MaxPitch * DtoR);

		m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);

		if(!KEYBOARD.KeyDown(KEY_LSHIFT))
		{
			//Orbit about point.
			worldPosition = m_OrbitPosition - (m_Frame.GetFront() * m_OrbitDistance);
		}
	}

	if(mouseButtons & ioMouse::MOUSE_MIDDLE)
	{
		if(KEYBOARD.KeyDown(KEY_LSHIFT))
		{
			//Zoom.
			worldPosition += m_Frame.GetFront() * ((m_CachedMouseY - mouseY) * timeStep);
		}
		else
		{
			//Translate (drag world.)
			gVpMan.GetGameViewport()->GetGrcViewport().ReverseTransform(m_CachedMouseX * scaleX, m_CachedMouseY * scaleY, (Vec3V&)groundSegment.A, (Vec3V&)groundSegment.B);
			Vector3 oldGroundPosition = Lerp(groundSegment.A.z / (groundSegment.A.z - groundSegment.B.z), groundSegment.A, groundSegment.B);	
			worldPosition += oldGroundPosition - groundPosition;
		}
	}

	m_Frame.SetPosition(worldPosition + (m_Frame.GetFront() * (ioMouse::GetDZ() * 10.0f * timeStep)));

	m_CachedMouseX = mouseX;
	m_CachedMouseY = mouseY;
	m_IsMouseButtonDown = (mouseButtons != 0);
}

void camFreeCamera::TeleportTarget(const Vector3& position, bool shouldLoadScene)
{
	CPed* targetPed = const_cast<CPed*>(camInterface::FindFollowPed());
	if(targetPed && cameraVerifyf(!targetPed->IsNetworkClone(), "camFreeCamera trying to teleport a network clone"))
	{
		//gRenderThreadInterface.Flush();	// needed on last gen but causing issues with the batcher on NG (B*2058215)

		const CVehicle* followVehicle	= camInterface::GetGameplayDirector().GetFollowVehicle();
		const s32 entryExitState		= camInterface::GetGameplayDirector().GetVehicleEntryExitState();
		if(followVehicle && (entryExitState == camGameplayDirector::INSIDE_VEHICLE))
		{
			const_cast<CVehicle*>(followVehicle)->Teleport(position, GetFrame().ComputeHeading());
		}
		else if (targetPed->GetMyMount())
		{
			targetPed->GetMyMount()->Teleport(position, GetFrame().ComputeHeading(), true);
		}
		else
		{
			targetPed->Teleport(position, GetFrame().ComputeHeading());
		}
	}

	if(shouldLoadScene)
	{
		g_SceneStreamerMgr.LoadScene(position);
	}
}

void camFreeCamera::SetFrame(const camFrame& frame)
{
	camBaseCamera::SetFrame(frame);

	m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);

	//Persist a copy of this frame so that we may use it as a fall-back.
	m_ClonedFrame.CloneFrom(frame);
}

void camFreeCamera::Reset()
{
	ResetAngles();
	ResetSpeeds();

	camFrame defaultFrame;
	m_ClonedFrame.CloneFrom(defaultFrame);

	m_NumUpdatesToPushNearClip = 0;

	m_IsMouseEnabled		= false;
	m_IsMouseButtonDown		= false;
	m_OverrideNearClip		= false;
	m_MovementSpeed			= 1;

#if __BANK
	m_DebugMountEntity							= NULL;
	m_ShouldDebugMountPlayer					= false;
	m_ShouldDebugMountPickerSelection			= false;
	m_ShouldDebugMountNearestPedVehicleOrObject	= false;
#endif // __BANK

	m_Frame.SetPosition(m_Metadata.m_StartPosition);

	m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
}

void camFreeCamera::ResetAngles()
{
	m_Frame.SetWorldMatrix(M34_IDENTITY);

#if __BANK
	m_DebugOrientation.Zero();
#endif // __BANK
}

void camFreeCamera::ResetSpeeds()
{
#if __BANK
	if(!DEBUG_PERSIST_SPEED_MULTIPLIERS)
#endif
	{
		GLOBAL_SPEED_MULTIPLIER	= 1.0f;
		MOVEMENT_SPEED_MULTIPLIER	= 1.0f;
		ROTATION_SPEED_MULTIPLIER	= 1.0f;
	}

	m_ForwardSpeed	= 0.0f;
	m_StrafeSpeed	= 0.0f;
	m_VerticalSpeed = 0.0f;
	m_HeadingSpeed	= 0.0f;
	m_PitchSpeed	= 0.0f;
	m_RollSpeed		= 0.0f;
	m_FovSpeed		= 0.0f;
}

void camFreeCamera::SetAllControlsEnabled(bool enabled)
{
	SetLeftStickEnabled(enabled);
	SetRightStickEnabled(enabled);
	SetCrossEnabled(enabled);
	SetDPadDownEnabled(enabled);
	SetDPadLeftEnabled(enabled);
	SetDPadRightEnabled(enabled);
	SetDPadUpEnabled(enabled);
	SetLeftShoulder1Enabled(enabled);
	SetLeftShoulder2Enabled(enabled);
	SetLeftShockEnabled(enabled);
	SetRightShoulder1Enabled(enabled);
	SetRightShoulder2Enabled(enabled);
	SetRightShockEnabled(enabled);
	SetSquareEnabled(enabled);	
	SetTriangleEnabled(enabled);
}

bool camFreeCamera::ComputeIsCarryingFollowPed() const
{
	//NOTE: We invalidate on quick exit from the debug camera.
	CPad& pad						= CControlMgr::GetDebugPad();
	const bool isCarryingFollowPed	= (m_RightShoulder2Enabled && pad.GetRightShoulder2() && !(m_LeftShoulder1Enabled && pad.GetLeftShoulder1()));

	return isCarryingFollowPed;
}

#if __BANK
void camFreeCamera::AddWidgetsForInstance()
{
	if(m_WidgetGroup == NULL)
	{
		bkBank* bank = BANKMGR.FindBank("Camera");
		if(bank)
		{
			m_WidgetGroup = bank->PushGroup("Free camera", false);
			{
/*				bank->AddToggle("Enable RDR Control Scheme:", &sEnablem_ShouldUseRdrMapping);
				bank->PushGroup("RDR Control Scheme");
				{
					bank->AddToggle("Invert Look Y:", &sRDRInvertYLook);

					bank->AddSlider("Super Speed Multiplier:", &sRDRSuperSpeedMultiplier, -FLT_MAX/1000.0f, FLT_MAX/1000.0f, 1.0f);
					bank->AddSlider("Super Speed Max Speed:", &sRDRMaxSuperSpeed, -FLT_MAX/1000.0f, FLT_MAX/1000.0f, 1.0f);
				}
				bank->PopGroup();
				bank->AddTitle("");
				bank->AddSeparator();
*/
				bank->AddTitle("DEPRECATED: Please use [Camera->Frame propagator->Override near clip]");
				bank->AddToggle("Should override near clip",	&m_ShouldDebugOverrideNearClip, NullCB,
					"DEPRECATED: Please use [Camera->Frame propagator->Override near clip]", "ARGBColor:64:255:0:0");

				bank->AddTitle("DEPRECATED: Please use [Camera->Frame propagator->Overridden near clip]");
				bank->AddSlider("Overridden near clip distance", &m_DebugOverridenNearClip,			g_MinNearClip, g_MaxNearClip, 0.01f, NullCB,
					"DEPRECATED: Please use [Camera->Frame propagator->Overridden near clip]", "ARGBColor:64:255:0:0");

				bank->AddSlider("Position",						const_cast<Vector3*>(&m_Frame.GetPosition()),	-99999.0f, 99999.0f, 0.01f);
				bank->AddSlider("Orientation",					&m_DebugOrientation,							-180.0f, 180.0f, 0.01f, datCallback(MFA(camFreeCamera::OnDebugOrientationChange), this));
				bank->AddToggle("RDR style camera",				&m_ShouldUseRdrMapping);
				bank->AddToggle("Override streaming focus",		&m_ShouldOverrideStreamingFocus);
				bank->AddToggle("Should mount player",			&m_ShouldDebugMountPlayer,			datCallback(MFA(camFreeCamera::OnDebugMountChange), this));
				bank->AddToggle("Should mount picker selection", &m_ShouldDebugMountPickerSelection, datCallback(MFA(camFreeCamera::OnDebugMountChange), this));
				bank->AddToggle("Should mount nearest ped, vehicle or object", &m_ShouldDebugMountNearestPedVehicleOrObject, datCallback(MFA(camFreeCamera::OnDebugMountChange), this));

				bank->PushGroup("Speeds", false);
				{
					bank->AddButton("Reset speeds", datCallback(MFA(camFreeCamera::ResetSpeeds), this), "Reset free camera speeds");
					bank->AddSlider("Global speed multiplier",			&GLOBAL_SPEED_MULTIPLIER,		-1000.0f, 1000.0f, 0.001f);
					bank->AddSlider("Movement speed multiplier",		&MOVEMENT_SPEED_MULTIPLIER,	-1000.0f, 1000.0f, 0.001f);
					bank->AddSlider("Rotation speed multiplier",		&ROTATION_SPEED_MULTIPLIER,	-1000.0f, 1000.0f, 0.001f);
					bank->AddToggle("Should persist speed multipliers", &DEBUG_PERSIST_SPEED_MULTIPLIERS);
				}
				bank->PopGroup();

				bank->PushGroup("Light", false);
				{
					bank->AddToggle("Enable cam light",			&m_IsDebugLightActive);
					bank->AddToggle("Attach light to camera",	&m_IsDebugLightAttached);
					bank->AddToggle("Use a rim light",			&m_ShouldUseADebugRimLight);
					bank->AddSlider("Position",					&m_DebugLightPosition,				-3000.0f, 3000.0f, 0.1f);
					bank->AddSlider("Colour",					&m_DebugLightColour,				0.0f, 1.0f, 1.0f);
					bank->AddSlider("Range",					&m_DebugLightRange,					1.0f, 50.0f, 1.0f);
					bank->AddSlider("Intensity",				&m_DebugLightIntensity,				0.0f, 100.0f, 0.1f);
				}
				bank->PopGroup();

				bank->AddButton("Reset angles",	datCallback(MFA(camFreeCamera::ResetAngles), this), "Reset the angles of the free camera (clearing any roll)");

				m_CameraWidgetData = bank->AddDataWidget("Camera Override Data", NULL,1024, datCallback(MFA(camFreeCamera::BankCameraWidgetReceiveDataCallback), (datBase*)this), 0, false);
			}
			bank->PopGroup();
		}
	}
}

void camFreeCamera::BankCameraWidgetReceiveDataCallback( )
{
	u8* data = m_CameraWidgetData->GetData();
	u16 length = m_CameraWidgetData->GetLength();

	if(!Verifyf(data != NULL, "Free Camera widget bank received NULL data."))
		return;

	if(!Verifyf(length > 0, "Free Camera widget bank received data of zero length."))
		return;

	float* buffer = (float*) &data[0];

	Vector3 position = VEC3_ZERO;
	Vector3 up = ZAXIS;
	Vector3 front = YAXIS;
	float fov = 45.0f; 

	position.Set(buffer[0], buffer[1], buffer[2]); 
	up.Set(buffer[3], buffer[4], buffer[5]);
	front.Set(buffer[6], buffer[7], buffer[8]);
	fov = buffer[9]; 

	m_Frame.SetFov(fov); 

	m_Frame.SetPosition(position);
	m_Frame.SetWorldMatrixFromFrontAndUp(front, up);
}

void camFreeCamera::OnDebugOrientationChange()
{
	Vector3 orientationEulers;
	orientationEulers.SetScaled(m_DebugOrientation, DtoR);

	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(orientationEulers.z, orientationEulers.x, orientationEulers.y);
}

void camFreeCamera::OnDebugMountChange()
{
	if(m_ShouldDebugMountPlayer)
	{
		m_DebugMountEntity = camInterface::FindFollowPed();

		m_ShouldDebugMountPickerSelection			= false;
		m_ShouldDebugMountNearestPedVehicleOrObject	= false;
	}
	else if(m_ShouldDebugMountPickerSelection)
	{
		m_DebugMountEntity = static_cast<const CEntity*>(g_PickerManager.GetSelectedEntity());

		m_ShouldDebugMountNearestPedVehicleOrObject = false;
	}
	else if(m_ShouldDebugMountNearestPedVehicleOrObject)
	{
		const Vector3& currentPosition = m_Frame.GetPosition();

		m_DebugMountEntity = FindNearestPedVehicleOrObjectToPosition(currentPosition, g_MaxDistanceToAttachParent);
	}
	else
	{
		m_DebugMountEntity = NULL;
	}

	if(m_DebugMountEntity)
	{
		const fwTransform& mountTransform = m_DebugMountEntity->GetTransform();

		const Vector3& currentPosition	= m_Frame.GetPosition();
		m_DebugMountRelativePosition	= VEC3V_TO_VECTOR3(mountTransform.UnTransform(VECTOR3_TO_VEC3V(currentPosition)));

		const float mountHeading	= mountTransform.GetHeading();
		const float mountPitch		= mountTransform.GetPitch();

		float currentHeading;
		float currentPitch;
		m_Frame.ComputeHeadingAndPitch(currentHeading, currentPitch);

		m_DebugMountRelativeHeading	= fwAngle::LimitRadianAngle(currentHeading - mountHeading);
		m_DebugMountRelativePitch	= fwAngle::LimitRadianAngle(currentPitch - mountPitch);
	}

	m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
}

const CEntity* camFreeCamera::FindNearestPedVehicleOrObjectToPosition(const Vector3& position, float maxDistanceToTest) const
{
	float distance2ToNearestEntity	= LARGE_FLOAT;
	CEntity* nearestEntity			= NULL;

	Vec3V vIteratorPos = VECTOR3_TO_VEC3V(position);
	CEntityIterator entityIterator(IteratePeds | IterateVehicles | IterateObjects, NULL, &vIteratorPos, maxDistanceToTest);

	CEntity* nextEntity = entityIterator.GetNext();
	while(nextEntity)
	{
		const Vector3 entityPosition	= VEC3V_TO_VECTOR3(nextEntity->GetTransform().GetPosition());
		const float distance2ToEntity	= position.Dist2(entityPosition);
		if(distance2ToEntity < distance2ToNearestEntity)
		{
			distance2ToNearestEntity	= distance2ToEntity;
			nearestEntity				= nextEntity;
		}

		nextEntity = entityIterator.GetNext();
	}

	return nearestEntity;
}
#endif // __BANK

#endif // !__FINAL
