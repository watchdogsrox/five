//  
// audio/ambientaudioentity.cpp
//  
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "ambientaudioentity.h"
#include "audambientzone.h"
#include "water/audshoreline.h"
#include "water/audshorelinePool.h"
#include "water/audshorelineRiver.h"
#include "water/audshorelineOcean.h"
#include "water/audshorelineLake.h"
#include "audio/emitteraudioentity.h"
#include "audio/northaudioengine.h"
#include "audio/vehicleaudioentity.h"
#include "audio/radioaudioentity.h"
#include "audio/radiostation.h"
#include "audio/weatheraudioentity.h"
#include "audioengine/engine.h"
#include "audio/music/musicplayer.h"
#include "audiosoundtypes/randomizedsound.h"
#include "audioengine/engineutil.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audiosoundtypes/sound.h"
#include "audio/gameobjects.h"
#include "control/replay/Audio/AmbientAudioPacket.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/WarpManager.h"
#include "system/memops.h"
#include "frontend/MiniMap.h" 
#include "grcore/debugdraw.h"
#include "game/clock.h"
#include "control/gamelogic.h"
#include "control/replay/replay.h"
#include "camera/scripted/ScriptedFlyCamera.h"
#include "scene/scene.h"
#include "renderer/water.h"
#include "modelinfo/mlomodelinfo.h"
#include "Peds/ped.h"
#include "physics/WorldProbe/worldprobe.h"
#include "game/weather.h"
#include "spatialdata/sphere.h"
#include "audiosoundtypes/simplesound.h"
#include "audio/radioaudioentity.h"
#include "audio/cutsceneaudioentity.h"
#include "audio/speechmanager.h"
#include "audio/scriptaudioentity.h"
#include "audiohardware/driver.h"
#include "audiosoundtypes/envelopesound.h"
//#include "scene/portals/InteriorInst.h"
#include "scene/portals/InteriorProxy.h"
#include "vehicles/vehiclepopulation.h"
#include "streaming/populationstreaming.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Peds/PopCycle.h"
#include "vfx/VfxHelper.h"
#include "vfx/ptfx/ptfxattach.h"

#include "rmptfx/ptxeffectinst.h"

#include "fwscene/world/WorldLimits.h"
#include "fwmaths/geomutil.h"

#include "audio/debugaudio.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/debug/FreeCamera.h"
#include "peds/PedFactory.h"

#include "system/controlMgr.h"
#include "system/control.h"
#include "system/Pad.h"

PF_PAGE(AmbientAudioTimingPage, "audAmbientAudioEntity Timings");
PF_GROUP(AmbientAudioTimings);
PF_LINK(AmbientAudioTimingPage, AmbientAudioTimings);
PF_TIMER(UpdateHighwayAmbience, AmbientAudioTimings);
PF_VALUE_INT(NumRequestedAudioEffects, AmbientAudioTimings);
AUDIO_AMBIENCE_OPTIMISATIONS()

audAmbientAudioEntity g_AmbientAudioEntity;
extern CPlayerSwitchInterface g_PlayerSwitch;
extern Vector3 g_Directions[4];

audCurve g_BlockedToVolume;
audCurve g_BlockedToPedDensityScale;
audCurve g_LoudSoundExclusionRadiusCurve;

f32 g_HighwayPassbySpeed = 10.0f;
f32 g_HighwayPassbyLifeTime = 12.0f;
f32 g_HighwayPassbyTyreBumpFrontBackOffsetScale = 0.66f;
f32 g_HighwayRoadNodeSearchMinDist = 25.0f;
f32 g_HighwayRoadNodeSearchMaxDist = 100.0f;
f32 g_HighwayRoadNodeRecalculationDist = 20.0f;
f32 g_HighwayRoadNodeCullDist = 60.0f;
f32 g_HighwayRoadNodeSearchUpOffset = 5.0f;
u32 g_SFXRMSSmoothingRateIncrease = 5000000;
u32 g_SFXRMSSmoothingRateDecrease = 100000;
bool g_HighwayOnlySearchUp = true;

f32 g_DefaultMaxAmbientHeightAboveGround = 300.0f;

f32 audAmbientAudioEntity::sm_CameraDistanceToShoreThreshold = 10000.f;
f32 audAmbientAudioEntity::sm_CameraDepthInWaterThreshold = 10.f;
u32 audAmbientAudioEntity::sm_ScriptUnderWaterStream = 0;

bool audAmbientAudioEntity::sm_PlayingUnderWaterSound = false;
bool audAmbientAudioEntity::sm_WasCameraUnderWater = false;

audAmbientZone* audAmbientAudioEntity::sm_CityZone = NULL;

audEnvironmentMetricsInternal audAmbientAudioEntity::sm_MetricsInternal;
audEnvironmentSoundMetrics audAmbientAudioEntity::sm_Metrics;

BANK_ONLY(extern bool g_DirectionalAmbienceChannelLeakageEnabled;)
BANK_ONLY(char g_DrawZoneFilter[128]={0};)
BANK_ONLY(char g_EditZoneFilter[128]={0};)
BANK_ONLY(char g_CreateZoneName[128]={0};)
BANK_ONLY(char g_CreateRuleName[128]={0};)
BANK_ONLY(char g_CreateEmitterName[128]={0};)
BANK_ONLY(char g_DuplicateEmitterName[128]={0};)
BANK_ONLY(char g_TestAlarmName[128]={0};)
BANK_ONLY(bool g_TestAlarmSkipStartup = false;)
BANK_ONLY(bool g_TestAlarmStopImmediate = true;)
BANK_ONLY(bool g_DrawSpawnFX = false;)
BANK_ONLY(bool g_DebugShoreGeneratesWaves = false;)
BANK_ONLY(bool g_DrawRegisteredEffects = false;)
BANK_ONLY(bool g_EnableAmbientAudio = true;)
BANK_ONLY(bool g_DrawDirectionalManagers = false;)
BANK_ONLY(bool g_DrawDirectionalManagerFinalSpeakerLevels = false;)
BANK_ONLY(bool g_DirectionalManagerIgnoreBlockage = false;)
BANK_ONLY(bool g_ShowDirectionalAmbienceCalculations = false;)
BANK_ONLY(bool g_MuteDirectionalManager = false;)
BANK_ONLY(bool g_DrawActivationZoneAABB = false;)
BANK_ONLY(bool g_DrawRuleInfo = false;)
BANK_ONLY(bool g_DrawLoadedDLCChunks = false;)
BANK_ONLY(bool g_DrawPlayingSounds = false;)
BANK_ONLY(bool g_DrawLoudSoundExclusionSphere = false;)
BANK_ONLY(bool g_DrawWaveSlotManager = false;)
BANK_ONLY(bool g_DrawInteriorZones = false;)
BANK_ONLY(bool g_DrawActiveMixerScenes = false;)
BANK_ONLY(bool g_ForceDisableStreaming = false;)
BANK_ONLY(bool g_StopAllRules = false;)
BANK_ONLY(bool g_TestZoneOverWaterLogic = false;)
BANK_ONLY(bool g_PrintWaterHeightOnMouseClick = false;)
BANK_ONLY(bool g_StopAllDirectionalAmbiences = false;)
BANK_ONLY(bool g_DirectionalManagerForceVol = false;)
BANK_ONLY(bool g_DrawClampedAndScaledWallaValues = false;)
BANK_ONLY(f32 g_DirectionalManagerForcedVol = 0.0f;)
BANK_ONLY(s32  g_DirectionalAmbienceIndexToRender= 0;)
BANK_ONLY(f32  g_DrawRuleInfoScroll = 0.0f;)
BANK_ONLY(s32  g_numActiveZones = 0;)
BANK_ONLY(s32  g_ZoneDebugDrawDistance = -1;)
BANK_ONLY(f32  g_ZoneEditorMovementSpeed = 1.0f;)
BANK_ONLY(s32  g_DrawAmbientZones = audAmbientAudioEntity::AMBIENT_ZONE_RENDER_NONE;)
BANK_ONLY(s32  g_ZoneActivationType = audAmbientAudioEntity::ZONE_ACTIVATION_TYPE_LISTENER;)
BANK_ONLY(s32  g_AmbientZoneRenderType = audAmbientAudioEntity::AMBIENT_ZONE_RENDER_TYPE_WIREFRAME;)
BANK_ONLY(s32  g_ActivationZoneRenderType = audAmbientAudioEntity::AMBIENT_ZONE_RENDER_TYPE_OFF;)
BANK_ONLY(s32  g_DirectionalManagerRenderDir = AUD_AMB_DIR_LOCAL;)
BANK_ONLY(s32  g_EditZoneDimension = audAmbientAudioEntity::AMBIENT_ZONE_DIMENSION_CAMERA_RELATIVE;)
BANK_ONLY(s32  g_EditZoneAttribute = audAmbientAudioEntity::AMBIENT_ZONE_EDIT_BOTH;)
BANK_ONLY(s32  g_EditZoneAnchor = audAmbientAudioEntity::AMBIENT_ZONE_EDIT_ANCHOR_CENTRE;)
BANK_ONLY(audAmbientZone* g_CurrentEditZone = NULL;)
BANK_ONLY(bool g_ZoneEditorActive = false;)
BANK_ONLY(bool g_EnableLinePickerTool = false;)
BANK_ONLY(bool g_DrawLinePickerCollisionSphere = false;)
BANK_ONLY(bool g_DrawZoneAnchorPoint = true;)
BANK_ONLY(AmbientZone* g_RaveCreatedZone = NULL; )
BANK_ONLY(audAmbientZone* g_SmallestPedDensityZone = NULL; )
BANK_ONLY(audAmbientZone* g_SecondSmallestPedDensityZone = NULL; )
BANK_ONLY(const char* g_WallaSpeechSettingsListName = NULL;)
BANK_ONLY(const char* g_WallaSpeechSettingsName = NULL;)
BANK_ONLY(char g_WallaSpeechSettingsContext[32] = {0};)
BANK_ONLY(f32 g_DrawPedWallaSpeechScroll = 0.0f;)

BANK_ONLY(s32  audAmbientAudioEntity::sm_numZonesReturnedByQuadTree = 0;)
BANK_ONLY(s32  audAmbientAudioEntity::sm_ShoreLineEditorState = audAmbientAudioEntity::STATE_WAITING_FOR_ACTION;)
BANK_ONLY(u8   audAmbientAudioEntity::sm_ShoreLineType = AUD_WATER_TYPE_POOL;);

BANK_ONLY(f32  audAmbientAudioEntity::sm_DebugShoreLineWidth = 0.f;)
BANK_ONLY(f32  audAmbientAudioEntity::sm_DebugShoreLineHeight = 0.f;)
BANK_ONLY(bool  audAmbientAudioEntity::sm_EditWidth = false;)
BANK_ONLY(bool  audAmbientAudioEntity::sm_EditActivationBoxSize = false;)
BANK_ONLY(bool  audAmbientAudioEntity::sm_WarpPlayerToShoreline = false;)
BANK_ONLY(bool  audAmbientAudioEntity::sm_MouseLeftPressed = false;)
BANK_ONLY(bool  audAmbientAudioEntity::sm_MouseRightPressed = false;)
BANK_ONLY(bool  audAmbientAudioEntity::sm_MouseMiddlePressed = false;)
BANK_ONLY(bool audAmbientAudioEntity::sm_DrawShoreLines = false;)
BANK_ONLY(bool audAmbientAudioEntity::sm_DrawPlayingShoreLines = false;)
BANK_ONLY(bool audAmbientAudioEntity::sm_DrawActivationBox = false;)
BANK_ONLY(bool audAmbientAudioEntity::sm_TestForWaterAtPed = false;)
BANK_ONLY(bool audAmbientAudioEntity::sm_TestForWaterAtCursor = false;)
BANK_ONLY(bool audAmbientAudioEntity::sm_DrawActiveQuadTreeNodes = false;)
BANK_ONLY(bool audAmbientAudioEntity::sm_AddingShorelineActivationBox = false;)
BANK_ONLY(bool audAmbientAudioEntity::sm_OverrideUnderWaterStream = false;)
BANK_ONLY(static char s_OverriddenUnderWaterStream[128] = "";);
BANK_ONLY(static int s_ShorelineToEdit = 0;)
BANK_ONLY(static char s_ShoreLineName[64] = "";);
BANK_ONLY(static bkBank* s_ShoreLineEditorBank = nullptr;)
BANK_ONLY(static bkGroup* s_ShorelineEditorGroup = nullptr;)
BANK_ONLY(static bkButton* s_CreateShorelineWidgetsButton = nullptr;)

#if __BANK
XPARAM(audiodesigner);
PARAM(disableambientaudio, "Disable the ambient audio system");
#endif

namespace rage 
{
	extern spdAABB g_WorldLimits_MapDataExtentsAABB;
}

#define QUAD_TREE_DEPTH (5)

#if __BANK
const char * g_ZoneActivationPointOptionNames[3] = { 
	"Listener Position",
	"Player Position",
	"Off",
};

const char * g_ZoneRenderOptionNames[4] = { 
	"None",
	"Active Only",
	"Inactive Only",
	"All",
};

const char * g_ZoneRenderModeOptionNames[3] = { 
	"Wireframe",
	"Solid",
	"Invisible",
};

const char * g_ZoneEditDimensionNames[4] = { 
	"Camera Relative",
	"X Axis",
	"Y Axis",
	"Z Axis",
};

const char * g_ZoneEditAttributeNames[3] = { 
	"Positioning Zone",
	"Activation Zone",
	"Positioning and Activation Zones",
};

const char * g_ZoneEditAnchorNames[5] = { 
	"Top Left",
	"Top Right",
	"Bottom Left",
	"Bottom Right",
	"Centre"
};

const char * g_AmbientDirectionNames[5] = { 
	"North",
	"East",
	"South",
	"West",
	"Local"
};
#endif

bool g_SpawnEffectsEnabled = true;
bool g_DrawNearestHighwayNodes = false;
bool g_PlayHighwayTyreBumps = true;
bool g_EnableHighwayAmbience = true;

extern audCurve g_OWOToCutoff;
extern audCurve g_OWOToVolume;
extern audCurve g_FRFToVolume;
extern bool g_IncludeAmountToOccludeOutsideWorldAfterPortals;
extern CPopulationStreaming gPopStreaming;

#if __BANK
bool g_DebugDrawWallaSpeech = false;
bool g_LogWallaSpeech = false;
bool g_ForceWallaSpeechRetrigger = false;
f32 g_WallaSpeechVolumeTrim = 0.0f;
f32 g_WallaSpeechTriggerMinDensity = 0.0f;
bool g_DebugDrawAmbientRadioEmitter = false;
#endif

bool g_MuteWalla = false;
bool g_EnableWallaSpeech = true;
bool g_DisplayExteriorWallaDebug = false;
bool g_DisplayInteriorWallaDebug = false;
bool g_DisplayHostileWallaDebug = false;
bool g_OverridePedDensity = false;
bool g_OverridePedPanic = false;
f32 g_OverridenPedDensity = 1.0f;
f32 g_OverridenPedPanic = 1.0f;
u32 g_WallaReuseTime = 800;
f32 g_PanicCooldownTime = 45.0f;
f32 g_PedDensitySmoothingIncrease = 0.005f;
f32 g_PedDensitySmoothingDecrease = 0.002f;
f32 g_PedDensitySmoothingInterior = 0.010f;
bool g_EnableAmbientRadioEmitter = true;

Vector3 g_HydrantSprayPos;
bool g_IsHydrantSpraying = false;

#define AUD_CLEAR_TRISTATE_VALUE(flagvar, flagid) (flagvar &= ~(0 | ((AUD_GET_TRISTATE_VALUE(flagvar, flagid)&0x03) << (flagid<<1))))

#if __BANK
// ----------------------------------------------------------------
// Update the zone editor
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdateDebugZoneEditor()
{
	if(!g_CurrentEditZone)
	{
		return;
	}


	sysCriticalSection lock(m_Lock);

	CPad& debugPad = CControlMgr::GetDebugPad();
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();

	if(debugPad.SelectJustDown() SCE_ONLY(|| debugPad.TouchClickJustDown()))
	{
		g_ZoneEditorActive = !g_ZoneEditorActive;
		debugDirector.SetShouldIgnoreDebugPadCameraToggle(false);
	}

	if(!g_ZoneEditorActive)
	{
		return;
	}

	if(g_EnableLinePickerTool)
	{
		if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter)
		{
			UpdateLinePickerTool();
			g_CurrentEditZone->CalculatePositioningLine();
			return;
		}
	}

	debugDirector.GetFreeCam()->DisableAllControls();
	debugDirector.SetShouldIgnoreDebugPadCameraToggle(true);

	s32 activationZoneRotation = g_CurrentEditZone->GetActivationZoneRotationAngle();
	s32 positioningZoneRotation = g_CurrentEditZone->GetPositioningZoneRotationAngle();

	f32 leftAnalogMoveX = (debugPad.GetLeftStickX() / 128.0f) * g_ZoneEditorMovementSpeed;
	f32 leftAnalogMoveY = (debugPad.GetLeftStickY() / 128.0f) * g_ZoneEditorMovementSpeed;

	f32 rightAnalogMoveX = debugPad.GetRightStickX() / 128.0f;
	f32 rightAnalogMoveY = debugPad.GetRightStickY() / 128.0f;

	f32 analogAmountShrink = debugPad.GetLeftShoulder2() / 255.0f;
	f32 analogAmountGrow = debugPad.GetRightShoulder2() / 255.0f;

	u16 digitalAmountShrink = debugPad.LeftShoulder1JustDown() ? 1 : 0;
	u16 digitalAmountGrow = debugPad.RightShoulder1JustDown() ? 1 : 0;

	bool dpadLeftPressed = debugPad.DPadLeftJustDown();
	bool dpadRightPressed = debugPad.DPadRightJustDown();
	bool dpadUpPressed = debugPad.DPadUpJustDown();
	bool dpadDownPressed = debugPad.DPadDownJustDown();

	Vector3 currentPositioningZoneCenter = g_CurrentEditZone->GetPositioningZoneCenter();
	Vector3 currentActivationZoneCenter = g_CurrentEditZone->GetActivationZoneCenter();
	Vector3 currentPositioningZoneSize = g_CurrentEditZone->GetPositioningZoneSize();
	Vector3 currentActivationZoneSize = g_CurrentEditZone->GetActivationZoneSize();
	Vector3 currentPositioningZonePostRotationOffset = g_CurrentEditZone->GetPositioningZonePostRotationOffset();
	Vector3 currentActivationZonePostRotationOffset = g_CurrentEditZone->GetActivationZonePostRotationOffset();
	Vector3 currentPositioningZoneSizeScale = g_CurrentEditZone->GetPositioningZoneSizeScale();
	Vector3 currentActivationZoneSizeScale = g_CurrentEditZone->GetActivationZoneSizeScale();

	if(debugPad.ShockButtonLJustDown())
	{
		g_EditZoneAttribute++;

		if(g_EditZoneAttribute > AMBIENT_ZONE_EDIT_BOTH)
		{
			g_EditZoneAttribute = 0;
		}
	}

	if(debugPad.ShockButtonRJustDown())
	{
		g_EditZoneAnchor++;

		if(g_EditZoneAnchor > AMBIENT_ZONE_EDIT_ANCHOR_CENTRE)
		{
			g_EditZoneAnchor = 0;
		}
	}

	if(debugPad.ButtonCircleJustDown())
	{
		g_EditZoneDimension = AMBIENT_ZONE_DIMENSION_Z_ONLY;
	}
	else if(debugPad.ButtonSquareJustDown())
	{
		g_EditZoneDimension = AMBIENT_ZONE_DIMENSION_X_ONLY;
	}
	else if(debugPad.ButtonCrossJustDown())
	{
		g_EditZoneDimension = AMBIENT_ZONE_DIMENSION_Y_ONLY;
	}
	else if(debugPad.ButtonTriangleJustDown())
	{
		g_EditZoneDimension = AMBIENT_ZONE_DIMENSION_CAMERA_RELATIVE;
	}

	switch(g_EditZoneDimension)
	{
	case AMBIENT_ZONE_DIMENSION_CAMERA_RELATIVE:
		{
			activationZoneRotation += digitalAmountShrink;
			activationZoneRotation -= digitalAmountGrow;
			positioningZoneRotation += digitalAmountShrink;
			positioningZoneRotation -= digitalAmountGrow;

			activationZoneRotation += (u16)(analogAmountShrink * 5.0f);
			activationZoneRotation -= (u16)(analogAmountGrow * 5.0f);
			positioningZoneRotation += (u16)(analogAmountShrink * 5.0f);
			positioningZoneRotation -= (u16)(analogAmountGrow * 5.0f);

			if(activationZoneRotation > 360)
			{
				activationZoneRotation -= 360;
			}
			else if(activationZoneRotation < 0)
			{
				activationZoneRotation += 360;
			}

			if(positioningZoneRotation > 360)
			{
				positioningZoneRotation -= 360;
			}
			else if(positioningZoneRotation < 0)
			{
				positioningZoneRotation += 360;
			}

			Vector3 forwardDir = debugDirector.GetFreeCam()->GetFrame().GetWorldMatrix().b;
			forwardDir.SetZ(0.0f);
			forwardDir.Normalize();

			Vector3 leftDir = debugDirector.GetFreeCam()->GetFrame().GetWorldMatrix().a;
			leftDir.SetZ(0.0f);
			leftDir.Normalize();

			Vector3 forwardMovement = forwardDir * -leftAnalogMoveY;
			Vector3 leftMovement = leftDir * leftAnalogMoveX;

			// Move the zone(s) relative to the camera forward direction
			if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter)
			{
				currentPositioningZonePostRotationOffset.SetX(currentPositioningZonePostRotationOffset.GetX() + forwardMovement.GetX() + leftMovement.GetX());
				currentPositioningZonePostRotationOffset.SetY(currentPositioningZonePostRotationOffset.GetY() + forwardMovement.GetY() + leftMovement.GetY());

				currentActivationZoneCenter.SetX(currentActivationZoneCenter.GetX() + forwardMovement.GetX() + leftMovement.GetX());
				currentActivationZoneCenter.SetY(currentActivationZoneCenter.GetY() + forwardMovement.GetY() + leftMovement.GetY());

				currentActivationZoneSize.SetX(currentActivationZoneSize.GetX() * (1.0f + (0.1f * rightAnalogMoveX)));
				currentActivationZoneSize.SetY(currentActivationZoneSize.GetY() * (1.0f + (0.1f * -rightAnalogMoveY)));

			}
			else if(g_CurrentEditZone->IsInteriorLinkedZone())
			{
				currentPositioningZonePostRotationOffset.SetX(currentPositioningZonePostRotationOffset.GetX() + forwardMovement.GetX() + leftMovement.GetX());
				currentPositioningZonePostRotationOffset.SetY(currentPositioningZonePostRotationOffset.GetY() + forwardMovement.GetY() + leftMovement.GetY());
				currentActivationZonePostRotationOffset.SetX(currentActivationZonePostRotationOffset.GetX() + forwardMovement.GetX() + leftMovement.GetX());
				currentActivationZonePostRotationOffset.SetY(currentActivationZonePostRotationOffset.GetY() + forwardMovement.GetY() + leftMovement.GetY());

				// Scale the zones along the appropriate axis
				currentPositioningZoneSizeScale.SetX(currentPositioningZoneSizeScale.GetX() * (1.0f + (0.1f * rightAnalogMoveX)));
				currentActivationZoneSizeScale.SetX(currentActivationZoneSizeScale.GetX() * (1.0f + (0.1f * rightAnalogMoveX)));
				currentPositioningZoneSizeScale.SetY(currentPositioningZoneSizeScale.GetY() * (1.0f + (0.1f * -rightAnalogMoveY)));
				currentActivationZoneSizeScale.SetY(currentActivationZoneSizeScale.GetY() * (1.0f + (0.1f * -rightAnalogMoveY)));
			}
			else
			{
				currentPositioningZoneCenter.SetX(currentPositioningZoneCenter.GetX() + forwardMovement.GetX() + leftMovement.GetX());
				currentPositioningZoneCenter.SetY(currentPositioningZoneCenter.GetY() + forwardMovement.GetY() + leftMovement.GetY());
				currentActivationZoneCenter.SetX(currentActivationZoneCenter.GetX() + forwardMovement.GetX() + leftMovement.GetX());
				currentActivationZoneCenter.SetY(currentActivationZoneCenter.GetY() + forwardMovement.GetY() + leftMovement.GetY());

				// Scale the zones along the appropriate axis
				currentPositioningZoneSize.SetX(currentPositioningZoneSize.GetX() * (1.0f + (0.1f * rightAnalogMoveX)));
				currentActivationZoneSize.SetX(currentActivationZoneSize.GetX() * (1.0f + (0.1f * rightAnalogMoveX)));
				currentPositioningZoneSize.SetY(currentPositioningZoneSize.GetY() * (1.0f + (0.1f * -rightAnalogMoveY)));
				currentActivationZoneSize.SetY(currentActivationZoneSize.GetY() * (1.0f + (0.1f * -rightAnalogMoveY)));
			}

			if(dpadUpPressed)
			{
				if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter)
				{
					currentPositioningZonePostRotationOffset.SetZ(currentPositioningZonePostRotationOffset.GetZ() + 1.0f);
					currentActivationZoneCenter.SetZ(currentActivationZoneCenter.GetZ() + 1.0f);
				}
				else if(g_CurrentEditZone->IsInteriorLinkedZone())
				{
					currentPositioningZonePostRotationOffset.SetZ(currentPositioningZonePostRotationOffset.GetZ() + 1.0f);
					currentActivationZonePostRotationOffset.SetZ(currentActivationZonePostRotationOffset.GetZ() + 1.0f);
				}
				else
				{
					currentPositioningZoneCenter.SetZ(currentPositioningZoneCenter.GetZ() + 1.0f);
					currentActivationZoneCenter.SetZ(currentActivationZoneCenter.GetZ() + 1.0f);
				}
			}
			else if(dpadDownPressed)
			{
				if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter)
				{
					currentPositioningZonePostRotationOffset.SetZ(currentPositioningZonePostRotationOffset.GetZ() - 1.0f);
					currentActivationZoneCenter.SetZ(currentActivationZoneCenter.GetZ() - 1.0f);
				}
				else if(g_CurrentEditZone->IsInteriorLinkedZone())
				{
					currentPositioningZonePostRotationOffset.SetZ(currentPositioningZonePostRotationOffset.GetZ() - 1.0f);
					currentActivationZonePostRotationOffset.SetZ(currentActivationZonePostRotationOffset.GetZ() - 1.0f);
				}
				else
				{
					currentPositioningZoneCenter.SetZ(currentPositioningZoneCenter.GetZ() - 1.0f);
					currentActivationZoneCenter.SetZ(currentActivationZoneCenter.GetZ() - 1.0f);
				}
			}

			if(dpadLeftPressed)
			{
				if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter)
				{
					currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() - 1.0f);
				}
				else if(g_CurrentEditZone->IsInteriorLinkedZone())
				{
					currentPositioningZoneSizeScale.SetZ(currentPositioningZoneSizeScale.GetZ()  - 1.0f);
					currentActivationZoneSizeScale.SetZ(currentActivationZoneSizeScale.GetZ() - 1.0f);
				}
				else
				{
					currentPositioningZoneSize.SetZ(currentPositioningZoneSize.GetZ()  - 1.0f);
					currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() - 1.0f);
				}
			}
			else if(dpadRightPressed)
			{
				if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter)
				{
					currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() + 1.0f);
				}
				if(g_CurrentEditZone->IsInteriorLinkedZone())
				{
					currentPositioningZoneSizeScale.SetZ(currentPositioningZoneSizeScale.GetZ() + 1.0f);
					currentPositioningZoneSizeScale.SetZ(currentActivationZoneSizeScale.GetZ() + 1.0f);
				}
				else
				{
					currentPositioningZoneSize.SetZ(currentPositioningZoneSize.GetZ() + 1.0f);
					currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() + 1.0f);
				}
			}
		}
		break;
	case AMBIENT_ZONE_DIMENSION_X_ONLY:
		{
			if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter)
			{
				currentActivationZoneSize.SetX(currentActivationZoneSize.GetX() * (1.0f - (0.1f * analogAmountShrink)));
				currentActivationZoneSize.SetX(currentActivationZoneSize.GetX() * (1.0f + (0.1f * analogAmountGrow)));
				currentActivationZoneSize.SetX(currentActivationZoneSize.GetX() + digitalAmountGrow);
				currentActivationZoneSize.SetX(currentActivationZoneSize.GetX() - digitalAmountShrink);

				currentPositioningZonePostRotationOffset.SetX(currentPositioningZonePostRotationOffset.GetX() + leftAnalogMoveX);
				currentPositioningZonePostRotationOffset.SetX(currentPositioningZonePostRotationOffset.GetX() + (rightAnalogMoveX/10.0f));

				currentActivationZoneCenter.SetX(currentActivationZoneCenter.GetX() + leftAnalogMoveX);
				currentActivationZoneCenter.SetX(currentActivationZoneCenter.GetX() + (rightAnalogMoveX/10.0f));
			}
			else if(g_CurrentEditZone->IsInteriorLinkedZone())
			{
				currentPositioningZoneSizeScale.SetX(currentPositioningZoneSizeScale.GetX() * (1.0f - (0.1f * analogAmountShrink)));
				currentPositioningZoneSizeScale.SetX(currentPositioningZoneSizeScale.GetX() * (1.0f + (0.1f * analogAmountGrow)));
				currentPositioningZoneSizeScale.SetX(currentPositioningZoneSizeScale.GetX() + digitalAmountGrow);
				currentPositioningZoneSizeScale.SetX(currentPositioningZoneSizeScale.GetX() - digitalAmountShrink);

				currentActivationZoneSizeScale.SetX(currentActivationZoneSizeScale.GetX() * (1.0f - (0.1f * analogAmountShrink)));
				currentActivationZoneSizeScale.SetX(currentActivationZoneSizeScale.GetX() * (1.0f + (0.1f * analogAmountGrow)));
				currentActivationZoneSizeScale.SetX(currentActivationZoneSizeScale.GetX() + digitalAmountGrow);
				currentActivationZoneSizeScale.SetX(currentActivationZoneSizeScale.GetX() - digitalAmountShrink);

				currentPositioningZonePostRotationOffset.SetX(currentPositioningZonePostRotationOffset.GetX() + leftAnalogMoveX);
				currentPositioningZonePostRotationOffset.SetX(currentPositioningZonePostRotationOffset.GetX() + (rightAnalogMoveX/10.0f));

				currentActivationZonePostRotationOffset.SetX(currentActivationZonePostRotationOffset.GetX() + leftAnalogMoveX);
				currentActivationZonePostRotationOffset.SetX(currentActivationZonePostRotationOffset.GetX() + (rightAnalogMoveX/10.0f));
			}
			else
			{
				currentPositioningZoneSize.SetX(currentPositioningZoneSize.GetX() * (1.0f - (0.1f * analogAmountShrink)));
				currentPositioningZoneSize.SetX(currentPositioningZoneSize.GetX() * (1.0f + (0.1f * analogAmountGrow)));
				currentPositioningZoneSize.SetX(currentPositioningZoneSize.GetX() + digitalAmountGrow);
				currentPositioningZoneSize.SetX(currentPositioningZoneSize.GetX() - digitalAmountShrink);

				currentActivationZoneSize.SetX(currentActivationZoneSize.GetX() * (1.0f - (0.1f * analogAmountShrink)));
				currentActivationZoneSize.SetX(currentActivationZoneSize.GetX() * (1.0f + (0.1f * analogAmountGrow)));
				currentActivationZoneSize.SetX(currentActivationZoneSize.GetX() + digitalAmountGrow);
				currentActivationZoneSize.SetX(currentActivationZoneSize.GetX() - digitalAmountShrink);

				currentPositioningZoneCenter.SetX(currentPositioningZoneCenter.GetX() + leftAnalogMoveX);
				currentPositioningZoneCenter.SetX(currentPositioningZoneCenter.GetX() + (rightAnalogMoveX/10.0f));

				currentActivationZoneCenter.SetX(currentActivationZoneCenter.GetX() + leftAnalogMoveX);
				currentActivationZoneCenter.SetX(currentActivationZoneCenter.GetX() + (rightAnalogMoveX/10.0f));
			}
		}
		break;
	case AMBIENT_ZONE_DIMENSION_Y_ONLY:
		{
			if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter)
			{
				currentActivationZoneSize.SetY(currentActivationZoneSize.GetY() * (1.0f - (0.1f * analogAmountShrink)));
				currentActivationZoneSize.SetY(currentActivationZoneSize.GetY() * (1.0f + (0.1f * analogAmountGrow)));
				currentActivationZoneSize.SetY(currentActivationZoneSize.GetY() + digitalAmountGrow);
				currentActivationZoneSize.SetY(currentActivationZoneSize.GetY() - digitalAmountShrink);

				currentPositioningZonePostRotationOffset.SetY(currentPositioningZonePostRotationOffset.GetY() + leftAnalogMoveY);
				currentPositioningZonePostRotationOffset.SetY(currentPositioningZonePostRotationOffset.GetY() + (rightAnalogMoveY/10.0f));

				currentActivationZoneCenter.SetY(currentActivationZoneCenter.GetY() + leftAnalogMoveY);
				currentActivationZoneCenter.SetY(currentActivationZoneCenter.GetY() + (rightAnalogMoveY/10.0f));
			}
			else if(g_CurrentEditZone->IsInteriorLinkedZone())
			{
				currentPositioningZoneSizeScale.SetY(currentPositioningZoneSizeScale.GetY() * (1.0f - (0.1f * analogAmountShrink)));
				currentPositioningZoneSizeScale.SetY(currentPositioningZoneSizeScale.GetY() * (1.0f + (0.1f * analogAmountGrow)));
				currentPositioningZoneSizeScale.SetY(currentPositioningZoneSizeScale.GetY() + digitalAmountGrow);
				currentPositioningZoneSizeScale.SetY(currentPositioningZoneSizeScale.GetY() - digitalAmountShrink);

				currentActivationZoneSizeScale.SetY(currentActivationZoneSizeScale.GetY() * (1.0f - (0.1f * analogAmountShrink)));
				currentActivationZoneSizeScale.SetY(currentActivationZoneSizeScale.GetY() * (1.0f + (0.1f * analogAmountGrow)));
				currentActivationZoneSizeScale.SetY(currentActivationZoneSizeScale.GetY() + digitalAmountGrow);
				currentActivationZoneSizeScale.SetY(currentActivationZoneSizeScale.GetY() - digitalAmountShrink);

				currentPositioningZonePostRotationOffset.SetY(currentPositioningZonePostRotationOffset.GetY() + leftAnalogMoveY);
				currentPositioningZonePostRotationOffset.SetY(currentPositioningZonePostRotationOffset.GetY() + (rightAnalogMoveY/10.0f));

				currentActivationZonePostRotationOffset.SetY(currentActivationZonePostRotationOffset.GetY() + leftAnalogMoveY);
				currentActivationZonePostRotationOffset.SetY(currentActivationZonePostRotationOffset.GetY() + (rightAnalogMoveY/10.0f));
			}
			else
			{
				currentPositioningZoneSize.SetY(currentPositioningZoneSize.GetY() * (1.0f - (0.1f * analogAmountShrink)));
				currentPositioningZoneSize.SetY(currentPositioningZoneSize.GetY() * (1.0f + (0.1f * analogAmountGrow)));
				currentPositioningZoneSize.SetY(currentPositioningZoneSize.GetY() + digitalAmountGrow);
				currentPositioningZoneSize.SetY(currentPositioningZoneSize.GetY() - digitalAmountShrink);

				currentActivationZoneSize.SetY(currentActivationZoneSize.GetY() * (1.0f - (0.1f * analogAmountShrink)));
				currentActivationZoneSize.SetY(currentActivationZoneSize.GetY() * (1.0f + (0.1f * analogAmountGrow)));
				currentActivationZoneSize.SetY(currentActivationZoneSize.GetY() + digitalAmountGrow);
				currentActivationZoneSize.SetY(currentActivationZoneSize.GetY() - digitalAmountShrink);

				currentPositioningZoneCenter.SetY(currentPositioningZoneCenter.GetY() + leftAnalogMoveY);
				currentPositioningZoneCenter.SetY(currentPositioningZoneCenter.GetY() + (rightAnalogMoveY/10.0f));

				currentActivationZoneCenter.SetY(currentActivationZoneCenter.GetY() + leftAnalogMoveY);
				currentActivationZoneCenter.SetY(currentActivationZoneCenter.GetY() + (rightAnalogMoveY/10.0f));
			}
		}
		break;
	case AMBIENT_ZONE_DIMENSION_Z_ONLY:
		{
			if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter)
			{
				currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() * (1.0f - (0.1f * analogAmountShrink)));
				currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() * (1.0f + (0.1f * analogAmountGrow)));
				currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() + digitalAmountGrow);
				currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() - digitalAmountShrink);

				currentPositioningZonePostRotationOffset.SetZ(currentPositioningZonePostRotationOffset.GetZ() - leftAnalogMoveY);
				currentPositioningZonePostRotationOffset.SetZ(currentPositioningZonePostRotationOffset.GetZ() - (rightAnalogMoveY/10.0f));

				currentActivationZoneCenter.SetZ(currentActivationZoneCenter.GetZ() - leftAnalogMoveY);
				currentActivationZoneCenter.SetZ(currentActivationZoneCenter.GetZ() - (leftAnalogMoveY /10.0f));
			}
			else if(g_CurrentEditZone->IsInteriorLinkedZone())
			{
				currentPositioningZoneSizeScale.SetZ(currentPositioningZoneSizeScale.GetZ() * (1.0f - (0.1f * analogAmountShrink)));
				currentPositioningZoneSizeScale.SetZ(currentPositioningZoneSizeScale.GetZ() * (1.0f + (0.1f * analogAmountGrow)));
				currentPositioningZoneSizeScale.SetZ(currentPositioningZoneSizeScale.GetZ() + digitalAmountGrow);
				currentPositioningZoneSizeScale.SetZ(currentPositioningZoneSizeScale.GetZ() - digitalAmountShrink);

				currentActivationZoneSizeScale.SetZ(currentActivationZoneSizeScale.GetZ() * (1.0f - (0.1f * analogAmountShrink)));
				currentActivationZoneSizeScale.SetZ(currentActivationZoneSizeScale.GetZ() * (1.0f + (0.1f * analogAmountGrow)));
				currentActivationZoneSizeScale.SetZ(currentActivationZoneSizeScale.GetZ() + digitalAmountGrow);
				currentActivationZoneSizeScale.SetZ(currentActivationZoneSizeScale.GetZ() - digitalAmountShrink);

				currentPositioningZonePostRotationOffset.SetZ(currentPositioningZonePostRotationOffset.GetZ() - leftAnalogMoveY);
				currentPositioningZonePostRotationOffset.SetZ(currentPositioningZonePostRotationOffset.GetZ() - (rightAnalogMoveY/10.0f));

				currentActivationZonePostRotationOffset.SetZ(currentActivationZonePostRotationOffset.GetZ() - leftAnalogMoveY);
				currentActivationZonePostRotationOffset.SetZ(currentActivationZonePostRotationOffset.GetZ() - (leftAnalogMoveY /10.0f));
			}
			else
			{
				currentPositioningZoneSize.SetZ(currentPositioningZoneSize.GetZ() * (1.0f - (0.1f * analogAmountShrink)));
				currentPositioningZoneSize.SetZ(currentPositioningZoneSize.GetZ() * (1.0f + (0.1f * analogAmountGrow)));
				currentPositioningZoneSize.SetZ(currentPositioningZoneSize.GetZ() + digitalAmountGrow);
				currentPositioningZoneSize.SetZ(currentPositioningZoneSize.GetZ() - digitalAmountShrink);

				currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() * (1.0f - (0.1f * analogAmountShrink)));
				currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() * (1.0f + (0.1f * analogAmountGrow)));
				currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() + digitalAmountGrow);
				currentActivationZoneSize.SetZ(currentActivationZoneSize.GetZ() - digitalAmountShrink);

				currentPositioningZoneCenter.SetZ(currentPositioningZoneCenter.GetZ() - leftAnalogMoveY);
				currentPositioningZoneCenter.SetZ(currentPositioningZoneCenter.GetZ() - (rightAnalogMoveY/10.0f));

				currentActivationZoneCenter.SetZ(currentActivationZoneCenter.GetZ() - leftAnalogMoveY);
				currentActivationZoneCenter.SetZ(currentActivationZoneCenter.GetZ() - (leftAnalogMoveY /10.0f));
			}
		}
		break;
	}

	if(g_EditZoneAttribute == AMBIENT_ZONE_EDIT_POSITIONING || g_EditZoneAttribute == AMBIENT_ZONE_EDIT_BOTH)
	{
		if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter)
		{
			g_CurrentEditZone->SetPositioningZonePostRotationOffset(currentPositioningZonePostRotationOffset);
		}
		else if(g_CurrentEditZone->IsInteriorLinkedZone())
		{
			g_CurrentEditZone->SetPositioningZonePostRotationOffset(currentPositioningZonePostRotationOffset);
			g_CurrentEditZone->SetPositioningZoneSizeScale(currentPositioningZoneSizeScale);
		}
		else
		{			
			if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboid)
			{
				currentPositioningZoneCenter += CalculateAnchorPointSizeCompensation(g_CurrentEditZone->GetPositioningZoneSize(), 
																					 currentPositioningZoneSize, 
																					 positioningZoneRotation + g_CurrentEditZone->GetMasterRotationAngle());
			}			

			g_CurrentEditZone->SetPositioningZoneSize(currentPositioningZoneSize);

			if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboid)
			{
				currentPositioningZoneCenter += CalculateAnchorPointRotationCompensation(currentPositioningZoneSize, 
																						 g_CurrentEditZone->GetPositioningZoneRotationAngle() + g_CurrentEditZone->GetMasterRotationAngle(), 
																						 positioningZoneRotation + g_CurrentEditZone->GetMasterRotationAngle());			
			}			

			g_CurrentEditZone->SetPositioningZoneCenter(currentPositioningZoneCenter);
		}

		g_CurrentEditZone->SetPositioningZoneRotationAngle((u16)positioningZoneRotation);
	}

	if(g_EditZoneAttribute == AMBIENT_ZONE_EDIT_ACTIVATION || g_EditZoneAttribute == AMBIENT_ZONE_EDIT_BOTH)
	{
		if(g_CurrentEditZone->IsInteriorLinkedZone())
		{
			g_CurrentEditZone->SetActivationZonePostRotationOffset(currentActivationZonePostRotationOffset);
			g_CurrentEditZone->SetActivationZoneSizeScale(currentActivationZoneSizeScale);
		}
		else
		{			
			if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboid)
			{
				currentActivationZoneCenter += CalculateAnchorPointSizeCompensation(g_CurrentEditZone->GetActivationZoneSize(), 
																					currentActivationZoneSize, 
																					activationZoneRotation + g_CurrentEditZone->GetMasterRotationAngle());
			}			

			g_CurrentEditZone->SetActivationZoneSize(currentActivationZoneSize);

			if(g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboid)
			{
				currentActivationZoneCenter += CalculateAnchorPointRotationCompensation(currentActivationZoneSize, 
																						g_CurrentEditZone->GetActivationZoneRotationAngle() + g_CurrentEditZone->GetMasterRotationAngle(), 
																						activationZoneRotation + g_CurrentEditZone->GetMasterRotationAngle());
			}			

			g_CurrentEditZone->SetActivationZoneCenter(currentActivationZoneCenter);
		}
		
		g_CurrentEditZone->SetActivationZoneRotationAngle((u16)activationZoneRotation);
	}

	g_CurrentEditZone->ComputeActivationZoneCenter();
	g_CurrentEditZone->ComputePositioningZoneCenter();
	g_CurrentEditZone->CalculatePositioningLine();

	// Delete and re-add to keep the quad tree AABBs up to date
	m_ZoneQuadTree.DeleteItem(g_CurrentEditZone);
	m_ZoneQuadTree.AddItemRestrictDupes((void*)g_CurrentEditZone, g_CurrentEditZone->GetActivationZoneAABB());
}

// ----------------------------------------------------------------
// Calculate the amount of position offset to apply when a zone size 
// is scaled, but the anchor point is not at the zone centre
// ----------------------------------------------------------------
Vector3 audAmbientAudioEntity::CalculateAnchorPointSizeCompensation(const Vector3& initialSize, const Vector3& newSize, f32 rotationAngleDegrees) const
{
	Mat34V mat(V_IDENTITY);			
	Mat34VRotateLocalZ(mat, ScalarVFromF32(rotationAngleDegrees * DtoR));			
	Vector3 sizeDifferenceX = Vector3(newSize.x - initialSize.x, 0.0f, 0.0f);
	Vector3 sizeDifferenceY = Vector3(0.0f, newSize.y - initialSize.y, 0.0f);

	sizeDifferenceX = VEC3V_TO_VECTOR3(Transform(mat, VECTOR3_TO_VEC3V(sizeDifferenceX)));
	sizeDifferenceY = VEC3V_TO_VECTOR3(Transform(mat, VECTOR3_TO_VEC3V(sizeDifferenceY)));

	Vector3 offset;
	offset.Zero();

	switch(g_EditZoneAnchor)
	{
	case AMBIENT_ZONE_EDIT_ANCHOR_TOP_LEFT:
		offset += sizeDifferenceX * 0.5f;
		offset -= sizeDifferenceY * 0.5f;
		break;
	case AMBIENT_ZONE_EDIT_ANCHOR_TOP_RIGHT:
		offset -= sizeDifferenceX * 0.5f;
		offset -= sizeDifferenceY * 0.5f;
		break;
	case AMBIENT_ZONE_EDIT_ANCHOR_BOTTOM_LEFT:
		offset += sizeDifferenceX * 0.5f;
		offset += sizeDifferenceY * 0.5f;
		break;
	case AMBIENT_ZONE_EDIT_ANCHOR_BOTTOM_RIGHT:
		offset -= sizeDifferenceX * 0.5f;
		offset += sizeDifferenceY * 0.5f;
		break;
	}	

	return offset;
}

// ----------------------------------------------------------------
// Calculate the amount of position offset to apply when a zone rotation 
// is changed, but the anchor point is not at the zone centre
// ----------------------------------------------------------------
Vector3 audAmbientAudioEntity::CalculateAnchorPointRotationCompensation(const Vector3& zoneSize, f32 initialRotationDegrees, f32 newRotationDegrees) const
{
	Vector3 anchorToCentre;
	anchorToCentre.Zero();

	switch(g_EditZoneAnchor)
	{
	case AMBIENT_ZONE_EDIT_ANCHOR_TOP_LEFT:
		anchorToCentre = Vector3(zoneSize.x * 0.5f, -zoneSize.y * 0.5f, 0.0f);
		break;
	case AMBIENT_ZONE_EDIT_ANCHOR_TOP_RIGHT:
		anchorToCentre = Vector3(-zoneSize.x * 0.5f, -zoneSize.y * 0.5f, 0.0f);
		break;
	case AMBIENT_ZONE_EDIT_ANCHOR_BOTTOM_LEFT:
		anchorToCentre = Vector3(zoneSize.x * 0.5f, zoneSize.y * 0.5f, 0.0f);
		break;
	case AMBIENT_ZONE_EDIT_ANCHOR_BOTTOM_RIGHT:
		anchorToCentre = Vector3(-zoneSize.x * 0.5f, zoneSize.y * 0.5f, 0.0f);
		break;
	}			

	Mat34V oldMatrix(V_IDENTITY);
	Mat34V newMatrix(V_IDENTITY);

	Mat34VRotateLocalZ(oldMatrix, ScalarVFromF32(initialRotationDegrees * DtoR));
	Mat34VRotateLocalZ(newMatrix, ScalarVFromF32(newRotationDegrees * DtoR));

	Vector3 oldOffset = VEC3V_TO_VECTOR3(Transform(oldMatrix, VECTOR3_TO_VEC3V(anchorToCentre)));
	Vector3 newOffset = VEC3V_TO_VECTOR3(Transform(newMatrix, VECTOR3_TO_VEC3V(anchorToCentre)));

	return newOffset - oldOffset;
}

// ----------------------------------------------------------------
// Update the line picker tool
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdateLinePickerTool()
{
	Vector3	worldPos;
	if(g_DrawLinePickerCollisionSphere)
	{
		CDebugScene::GetWorldPositionUnderMouse(worldPos);
		grcDebugDraw::Sphere(worldPos, 0.3f, Color32(0xff, 0x00, 0x00, 0xff));
	}

	if(CDebugScene::GetMouseLeftPressed())
	{
		CDebugScene::GetWorldPositionUnderMouse(worldPos);
		g_CurrentEditZone->GetZoneDataEditable()->PositioningZone.RotationAngle = 0;
		g_CurrentEditZone->GetZoneDataEditable()->PositioningZone.PostRotationOffset = Vector3(0, 0, 0);
		g_CurrentEditZone->GetZoneDataEditable()->PositioningZone.Centre = worldPos;
	}
	else if(CDebugScene::GetMouseRightPressed())
	{
		CDebugScene::GetWorldPositionUnderMouse(worldPos);
		g_CurrentEditZone->GetZoneDataEditable()->PositioningZone.RotationAngle = 0;
		g_CurrentEditZone->GetZoneDataEditable()->PositioningZone.PostRotationOffset = Vector3(0, 0, 0);
		g_CurrentEditZone->GetZoneDataEditable()->PositioningZone.Size = worldPos;
	}
}

// ----------------------------------------------------------------
// Draw all debug info related to the ambient entity
// ----------------------------------------------------------------
void audAmbientAudioEntity::DebugDraw()
{
	sysCriticalSection lock(m_Lock);

	f32 playingRuleYRenderPos = 0.15f;
	f32 directionalAmbienceRenderPos = 0.15f;
	f32 activeRuleYRenderPos = 0.15f - (1.0f * g_DrawRuleInfoScroll);

	if(g_DrawWaveSlotManager)
	{
		audAmbientZone::sm_WaveSlotManager.DebugDraw(0.05f, 0.6f);
	}

	if(g_DrawLoudSoundExclusionSphere)
	{
		Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
		grcDebugDraw::Sphere(listenerPosition, m_LoudSoundExclusionRadius, Color32(255,0,0), false);
	}

	if(g_DrawInteriorZones)
	{
		char tempString[256];
		CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

		if(pIntInst)
		{
			formatf(tempString, "Interior %s, Room %s", pIntInst->GetModelName(), CPortalVisTracker::GetPrimaryRoomIdx() < pIntInst->GetNumRooms() ? pIntInst->GetRoomName(CPortalVisTracker::GetPrimaryRoomIdx()) : "UNKNOWN");
			grcDebugDraw::Text(Vector2(0.15f, 0.2f), Color32(255,255,255), tempString);

			formatf(tempString, "Transform: %f, %f, %f", VEC3V_ARGS(pIntInst->GetTransform().GetPosition()));
			grcDebugDraw::Text(Vector2(0.15f, 0.22f), Color32(255,255,255), tempString);

			Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
			Vector3 transformedPos = VEC3V_TO_VECTOR3(pIntInst->GetTransform().UnTransform(RCC_VEC3V(listenerPosition)));
			formatf(tempString, "Interior relative listener pos: %f, %f, %f", transformedPos.GetX(), transformedPos.GetY(), transformedPos.GetZ());
			grcDebugDraw::Text(Vector2(0.15f, 0.24f), Color32(255,255,255), tempString);
		}
		else
		{
			formatf(tempString, "No Active Interiors");
			grcDebugDraw::Text(Vector2(0.15f, 0.2f), Color32(255,255,255), tempString);
		}
	}

	if(g_DrawActiveMixerScenes)
	{
		audAmbientZone::DebugDrawMixerScenes();
	}

	if(g_DrawNearestHighwayNodes)
	{		
		char tempString[256];
		for(u32 loop = 0; loop < m_NumHighwayNodesFound; loop++)
		{
			const CPathNode* node = ThePaths.FindNodePointerSafe(m_HighwaySearchNodes[loop]);

			if(node)
			{
				Vector3 nodePos;
				node->GetCoors(nodePos);
				formatf(tempString, "Node! (Speed: %d)", node->m_2.m_speed);
				grcDebugDraw::Text(nodePos, Color32(255,255,255), tempString);
			}
		}

		formatf(tempString, "%d valid nodes found (%d in radius)", m_NumHighwayNodesFound, m_NumHighwayNodesInRadius);
		grcDebugDraw::Text(Vector2(0.15f, 0.15f), Color32(255,255,255), tempString);

		formatf(tempString, "Global Volume %.02f", m_GlobalHighwayVolumeSmoother.GetLastValue());
		grcDebugDraw::Text(Vector2(0.15f, 0.17f), Color32(255,255,255), tempString);

		for(u32 loop = 0; loop < kMaxPassbySounds; loop++)
		{
			if(m_HighwayPassbySounds[loop].sound)
			{
				formatf(tempString, "%s (Reverb: %.02f Dist %.02f)", m_HighwayPassbySounds[loop].vehicleModel->GetGameName(), m_HighwayPassbySounds[loop].reverbAmount, m_HighwayPassbySounds[loop].position.Dist(VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition())));

				grcDebugDraw::Text(m_HighwayPassbySounds[loop].position, Color32(255,255,255), tempString);
				grcDebugDraw::Sphere(m_HighwayPassbySounds[loop].position, 2.f, Color32(255,0,0));

				if(!m_HighwayPassbySounds[loop].lastBumpPos.IsZero())
				{
					grcDebugDraw::Text(m_HighwayPassbySounds[loop].lastBumpPos, Color32(255,255,255), "BUMP!");
					grcDebugDraw::Sphere(m_HighwayPassbySounds[loop].lastBumpPos, 1.f, Color32(0,0,255));
				}

				grcDebugDraw::Line(m_HighwayPassbySounds[loop].startPos, m_HighwayPassbySounds[loop].endPos, Color32(0,255,0));
			}
		}
	}

	if(g_DirectionalAmbienceIndexToRender < 0)
	{
		g_DirectionalAmbienceIndexToRender = m_DirectionalAmbienceManagers.GetCount() - 1;
	}

	if(g_DirectionalAmbienceIndexToRender >= m_DirectionalAmbienceManagers.GetCount())
	{
		g_DirectionalAmbienceIndexToRender = 0;
	}

	if(g_MuteDirectionalManager)
	{
		m_DirectionalAmbienceManagers[g_DirectionalAmbienceIndexToRender].ToggleMute();
		g_MuteDirectionalManager = false;
	}

	if(g_DrawDirectionalManagers)
	{
		char tempString[256];
		formatf(tempString, "Directional Ambiences");
		grcDebugDraw::Text(Vector2(0.15f, directionalAmbienceRenderPos), Color32(255,255,255), tempString);
		directionalAmbienceRenderPos += 0.05f;

		for(s32 loop = 0; loop < m_DirectionalAmbienceManagers.GetCount(); loop++)
		{
			const char* directionalAmbienceName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_DirectionalAmbienceManagers[loop].GetDirectionalAmbience()->NameTableOffset);

			if(loop == g_DirectionalAmbienceIndexToRender)
			{
				formatf(tempString, "%s (Vol: %.02f) <-----", directionalAmbienceName, m_DirectionalAmbienceManagers[loop].GetLastParentZoneVolume());
				m_DirectionalAmbienceManagers[loop].DebugDraw();
			}
			else
			{
				formatf(tempString, "%s (Vol: %.02f)", directionalAmbienceName, m_DirectionalAmbienceManagers[loop].GetLastParentZoneVolume());
			}

			if(m_DirectionalAmbienceManagers[loop].AreAnyParentZonesActive())
			{	
				grcDebugDraw::Text(Vector2(0.15f, directionalAmbienceRenderPos), Color32(0,255,0), tempString );
			}
			else
			{	
				grcDebugDraw::Text(Vector2(0.15f, directionalAmbienceRenderPos), Color32(255,0,0), tempString );
			}

			directionalAmbienceRenderPos += 0.02f;
		}

		audDirectionalAmbienceManager::DebugDrawInputVariables((audAmbienceDirection)g_DirectionalManagerRenderDir);
	}

	if(g_DrawLoadedDLCChunks)
	{		
		grcDebugDraw::Text(Vector2(0.05f, 0.05f), Color32(0,0,255), "Loaded DLC Chunks" );

		for(u32 loop = 0; loop < m_LoadedDLCChunks.GetCount(); loop++)
		{
			u32 chunkID = audNorthAudioEngine::GetMetadataManager().FindChunkId(m_LoadedDLCChunks[loop]);
			const char* chunkName = audNorthAudioEngine::GetMetadataManager().GetChunk(chunkID).chunkName;
			grcDebugDraw::Text(Vector2(0.05f, 0.09f + (0.02f * loop)), Color32(255,255,255), chunkName);
		}
	}

	if(g_DrawRuleInfo)
	{
		const f32 hours = static_cast<f32>(CClock::GetHour()) + (CClock::GetMinute()/60.f);

		char tempString[256];

		formatf(tempString, "Rain: %.02f", g_weather.GetTimeCycleAdjustedRain());
		grcDebugDraw::Text(Vector2(0.1f, 0.05f), Color32(0,0,255), tempString );

		formatf(tempString, "Min Rain: %.02f", GetMinRainLevel());
		grcDebugDraw::Text(Vector2(0.1f, 0.075f), Color32(0,0,255), tempString );

		formatf(tempString, "Time: %.02f", hours);
		grcDebugDraw::Text(Vector2(0.25f, 0.05f), Color32(0,0,255), tempString );

		formatf(tempString, "Active Zones: %d/%d", g_numActiveZones, m_Zones.GetCount() );
		grcDebugDraw::Text(Vector2(0.35f, 0.05f), Color32(0,0,255), tempString );

		formatf(tempString, "Quad Tree Results: %d/%d", sm_numZonesReturnedByQuadTree, m_Zones.GetCount() );
		grcDebugDraw::Text(Vector2(0.5f, 0.05f), Color32(0,0,255), tempString );

		formatf(tempString, "World Height Factor: %f", GetWorldHeightFactorAtListenerPosition() );
		grcDebugDraw::Text(Vector2(0.5f, 0.075f), Color32(0,0,255), tempString );

		if(g_AudioEngine.GetSoundManager().GetVariableAddress(ATSTRINGHASH("Game.Player.Speed", 0x8CE50515)))
		{
			formatf(tempString, "Player Speed: %.02f", *g_AudioEngine.GetSoundManager().GetVariableAddress(ATSTRINGHASH("Game.Player.Speed", 0x8CE50515)));
			grcDebugDraw::Text(Vector2(0.25f, 0.075f), Color32(0,0,255), tempString );
		}

		u32 numItemsAtEachLevel[QUAD_TREE_DEPTH];
		u32 numNodesAtEachLevel[QUAD_TREE_DEPTH];

		for(u32 loop = 0; loop < QUAD_TREE_DEPTH; loop++)
		{
			numItemsAtEachLevel[loop] = 0;
			numNodesAtEachLevel[loop] = 0;
		}

		m_ZoneQuadTree.GetEntryCount(&numItemsAtEachLevel[0]);
		m_ZoneQuadTree.GetNodeCount(&numNodesAtEachLevel[0]);

		u32 totalEntries = 0;
		u32 totalNodes = 0;

		for(u32 loop = 0; loop < QUAD_TREE_DEPTH; loop++ )
		{
			totalEntries += numItemsAtEachLevel[loop];
			totalNodes += numNodesAtEachLevel[loop];
		}

		formatf(tempString, "Entries: %d", totalEntries );
		grcDebugDraw::Text(Vector2(0.75f, 0.05f), Color32(0,0,255), tempString );

		formatf(tempString, "Nodes: %d", totalNodes );
		grcDebugDraw::Text(Vector2(0.85f, 0.05f), Color32(0,0,255), tempString );

		grcDebugDraw::Text(Vector2(0.1f, 0.1f), Color32(0,0,255), "Playing Rules" );
		grcDebugDraw::Text(Vector2(0.33f, 0.1f), Color32(0,0,255), "Distance" );
		grcDebugDraw::Text(Vector2(0.45f, 0.1f), Color32(0,0,255), "Active Rules" );
		grcDebugDraw::Text(Vector2(0.7f, 0.1f), Color32(0,0,255), "Min Time" );
		grcDebugDraw::Text(Vector2(0.76f, 0.1f), Color32(0,0,255), "Max Time" );
		grcDebugDraw::Text(Vector2(0.83f, 0.1f), Color32(0,0,255), "Next" );
		grcDebugDraw::Text(Vector2(0.89f, 0.1f), Color32(0,0,255), "Bank Status" );
	}

	for(atArray<audAmbientZone*>::iterator i = m_Zones.begin(); i != m_Zones.end(); i++)
	{
		bool isActive = (*i)->IsActive();

		// Only draw the entity if it meets the rendering criteria
		if( (g_DrawAmbientZones == audAmbientAudioEntity::AMBIENT_ZONE_RENDER_ALL) ||
			(g_DrawAmbientZones == audAmbientAudioEntity::AMBIENT_ZONE_RENDER_ACTIVE_ONLY && isActive) ||
			(g_DrawAmbientZones == audAmbientAudioEntity::AMBIENT_ZONE_RENDER_INACTIVE_ONLY && !isActive) )
		{
			// Check how far this zone is from the activation point and render it if its within range
			f32 distFromListener = 0.0f;
			
			if( (*i)->GetZoneData() )
			{
				distFromListener = (*i)->GetPositioningZoneCenterCalculated().Dist(RCC_VECTOR3(m_ZoneActivationPosition));
			}

			// We treat draw distance of -1 as infinite
			if( distFromListener < g_ZoneDebugDrawDistance ||
				g_ZoneDebugDrawDistance < 0 )
			{
				(*i)->DebugDraw();
			}
		}

		if(g_DrawPlayingSounds)
		{
			(*i)->DebugDrawPlayingSounds();
		}

		if(g_DrawRuleInfo)
		{	
			(*i)->DebugDrawRules(playingRuleYRenderPos, activeRuleYRenderPos);
		}
	}	

	for(u32 loop = 0; loop < m_InteriorZones.GetCount(); loop++)
	{
		if(g_DrawInteriorZones)
		{
			m_InteriorZones[loop].DebugDraw();
		}

		if(g_DrawPlayingSounds)
		{
			m_InteriorZones[loop].DebugDrawPlayingSounds();
		}

		if(g_DrawRuleInfo)
		{
			m_InteriorZones[loop].DebugDrawRules(playingRuleYRenderPos, activeRuleYRenderPos);
		}
	}

	if(sm_TestForWaterAtCursor)
	{
		Vector3	worldPos;
		CDebugScene::GetWorldPositionUnderMouse(worldPos);		
		f32 waterZ = 0.0f;
		CVfxHelper::GetWaterZ(VECTOR3_TO_VEC3V(worldPos), waterZ);
		worldPos.z = Max(waterZ + 0.3f, worldPos.z);
		grcDebugDraw::Sphere(worldPos, 1.0f, IsPointOverWater(worldPos)? Color32(0.0f, 0.0f, 1.0f, 0.4f) : Color32(0.0f, 1.0f, 0.0f, 0.4f));
	}

	if(g_PrintWaterHeightOnMouseClick)
	{
		if (CDebugScene::GetMouseLeftPressed())
		{
			f32 waterZ = FLT_MIN;
			Vector3	worldPos;
			CDebugScene::GetWorldPositionUnderMouse(worldPos);
			bool levelFound = Water::GetWaterLevel(worldPos, &waterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
			audDisplayf("Water level: %f Found:%s", waterZ, levelFound? "true" : "false");
		} 
	}

	if(g_CurrentEditZone)
	{
		g_CurrentEditZone->DebugDraw();

		char tempString[256];
		grcDebugDraw::Text(Vector2(0.1f, 0.1f), Color32(255,255,255), "Ambient Zone Debug Editor" );

		if(g_ZoneEditorActive)
		{
			grcDebugDraw::Text(Vector2(0.1f, 0.12f), Color32(255,255,255), "Zone Editing Active (Press Select/Back To Stop Editing)" );
			formatf(tempString, "Currently Editing Zone: %s", g_CurrentEditZone->GetName());
			grcDebugDraw::Text(Vector2(0.1f, 0.14f), Color32(255,255,255), tempString);
		}
		else
		{
			grcDebugDraw::Text(Vector2(0.1f, 0.12f), Color32(255,255,255), "Free Cam Mode Active (Press Select/Back To Start Editing)" );
			formatf(tempString, "Currently Viewing Zone: %s", g_CurrentEditZone->GetName());
			grcDebugDraw::Text(Vector2(0.1f, 0.14f), Color32(255,255,255), tempString);
		}

		if(g_EnableLinePickerTool && (g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter || g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneSphereLineEmitter))
		{
			formatf(tempString, "Editing Positioning Line");
			grcDebugDraw::Text(Vector2(0.1f, 0.16f), Color32(255,255,255), tempString);

			formatf(tempString, "Left Click to Set Start Pos");
			grcDebugDraw::Text(Vector2(0.1f, 0.18f), Color32(255,255,255), tempString);

			formatf(tempString, "Right Click to Set End Pos");
			grcDebugDraw::Text(Vector2(0.1f, 0.20f), Color32(255,255,255), tempString);
		}
		else
		{
			formatf(tempString, "Editing Dimension: %s", g_ZoneEditDimensionNames[g_EditZoneDimension]);
			grcDebugDraw::Text(Vector2(0.1f, 0.16f), Color32(255,255,255), tempString);

			formatf(tempString, "Editing Element: %s", g_ZoneEditAttributeNames[g_EditZoneAttribute]);
			grcDebugDraw::Text(Vector2(0.1f, 0.18f), Color32(255,255,255), tempString);
			
			if(!g_CurrentEditZone->IsInteriorLinkedZone() && g_CurrentEditZone->GetZoneData()->Shape == kAmbientZoneCuboid)
			{
				formatf(tempString, "Anchor Point: %s", g_ZoneEditAnchorNames[g_EditZoneAnchor]);
				grcDebugDraw::Text(Vector2(0.1f, 0.20f), Color32(255,255,255), tempString);
			}			

			grcDebugDraw::Text(Vector2(0.77f, 0.8f), g_EditZoneDimension == AMBIENT_ZONE_DIMENSION_CAMERA_RELATIVE? Color32(0,255,0) : Color32(255,0,0), g_ZoneEditDimensionNames[0]);
			grcDebugDraw::Text(Vector2(0.85f, 0.85f), g_EditZoneDimension == AMBIENT_ZONE_DIMENSION_Z_ONLY? Color32(0,255,0) : Color32(255,0,0), g_ZoneEditDimensionNames[3]);
			grcDebugDraw::Text(Vector2(0.8f, 0.9f), g_EditZoneDimension == AMBIENT_ZONE_DIMENSION_Y_ONLY? Color32(0,255,0) : Color32(255,0,0), g_ZoneEditDimensionNames[2]);
			grcDebugDraw::Text(Vector2(0.75f, 0.85f), g_EditZoneDimension == AMBIENT_ZONE_DIMENSION_X_ONLY? Color32(0,255,0) : Color32(255,0,0), g_ZoneEditDimensionNames[1]);

			grcDebugDraw::Text(Vector2(0.1f, 0.24f), Color32(255,255,255), "Activation Zone" );

			if(g_CurrentEditZone->IsInteriorLinkedZone())
			{
				formatf(tempString, "    Post Rotation Offset: %.02f, %.02f, %.02f ", g_CurrentEditZone->GetActivationZonePostRotationOffset().GetX(), g_CurrentEditZone->GetActivationZonePostRotationOffset().GetY(), g_CurrentEditZone->GetActivationZonePostRotationOffset().GetZ());
				grcDebugDraw::Text(Vector2(0.1f, 0.26f), Color32(255,255,255), tempString );

				formatf(tempString, "    Size Scale: %.02f, %.02f, %.02f ", g_CurrentEditZone->GetActivationZoneSizeScale().GetX(), g_CurrentEditZone->GetActivationZoneSizeScale().GetY(), g_CurrentEditZone->GetActivationZoneSizeScale().GetZ());
				grcDebugDraw::Text(Vector2(0.1f, 0.28f), Color32(255,255,255), tempString );
			}
			else
			{
				formatf(tempString, "    Centre: %.02f, %.02f, %.02f ", g_CurrentEditZone->GetActivationZoneCenter().GetX(), g_CurrentEditZone->GetActivationZoneCenter().GetY(), g_CurrentEditZone->GetActivationZoneCenter().GetZ());
				grcDebugDraw::Text(Vector2(0.1f, 0.26f), Color32(255,255,255), tempString );

				formatf(tempString, "    Size: %.02f, %.02f, %.02f ", g_CurrentEditZone->GetActivationZoneSize().GetX(), g_CurrentEditZone->GetActivationZoneSize().GetY(), g_CurrentEditZone->GetActivationZoneSize().GetZ());
				grcDebugDraw::Text(Vector2(0.1f, 0.28f), Color32(255,255,255), tempString );
			}

			formatf(tempString, "    RotationAngle: %d", g_CurrentEditZone->GetActivationZoneRotationAngle());
			grcDebugDraw::Text(Vector2(0.1f, 0.30f), Color32(255,255,255), tempString );


			grcDebugDraw::Text(Vector2(0.1f, 0.32f), Color32(255,255,255), "Positioning Zone" );

			if(g_CurrentEditZone->IsInteriorLinkedZone())
			{
				formatf(tempString, "    Post Rotation Offset: %.02f, %.02f, %.02f ", g_CurrentEditZone->GetPositioningZonePostRotationOffset().GetX(), g_CurrentEditZone->GetPositioningZonePostRotationOffset().GetY(), g_CurrentEditZone->GetPositioningZonePostRotationOffset().GetZ());
				grcDebugDraw::Text(Vector2(0.1f, 0.34f), Color32(255,255,255), tempString );

				formatf(tempString, "    Size Scale: %.02f, %.02f, %.02f ", g_CurrentEditZone->GetPositioningZoneSizeScale().GetX(), g_CurrentEditZone->GetPositioningZoneSizeScale().GetY(), g_CurrentEditZone->GetPositioningZoneSizeScale().GetZ());
				grcDebugDraw::Text(Vector2(0.1f, 0.36f), Color32(255,255,255), tempString );
			}
			else
			{
				formatf(tempString, "    Centre: %.02f, %.02f, %.02f ", g_CurrentEditZone->GetPositioningZoneCenter().GetX(), g_CurrentEditZone->GetPositioningZoneCenter().GetY(), g_CurrentEditZone->GetPositioningZoneCenter().GetZ());
				grcDebugDraw::Text(Vector2(0.1f, 0.34f), Color32(255,255,255), tempString );

				formatf(tempString, "    Size: %.02f, %.02f, %.02f ", g_CurrentEditZone->GetPositioningZoneSize().GetX(), g_CurrentEditZone->GetPositioningZoneSize().GetY(), g_CurrentEditZone->GetPositioningZoneSize().GetZ());
				grcDebugDraw::Text(Vector2(0.1f, 0.36f), Color32(255,255,255), tempString );
			}

			formatf(tempString, "    RotationAngle: %d", g_CurrentEditZone->GetPositioningZoneRotationAngle());
			grcDebugDraw::Text(Vector2(0.1f, 0.38f), Color32(255,255,255), tempString );

			grcDebugDraw::Text(Vector2(0.68f, 0.1f), Color32(255,255,255), "L3 - Change Zone component being edited");
			grcDebugDraw::Text(Vector2(0.68f, 0.12f), Color32(255,255,255), "R3 - Change Zone Lock Type");			
			grcDebugDraw::Text(Vector2(0.68f, 0.14f), Color32(255,255,255), "Select/Back - Toggle Editor/Free Cam");
			grcDebugDraw::Text(Vector2(0.68f, 0.16f), Color32(255,255,255), "Face Buttons - Toggle Editor Mode");

			switch(g_EditZoneDimension)
			{
			case AMBIENT_ZONE_DIMENSION_CAMERA_RELATIVE:

				grcDebugDraw::Text(Vector2(0.68f, 0.20f), Color32(255,255,255), "L2 / R2 - Rotate zone left and right");
				grcDebugDraw::Text(Vector2(0.68f, 0.22f), Color32(255,255,255), "L1 / R1 - Rotate zone left and right fine");
				grcDebugDraw::Text(Vector2(0.68f, 0.24f), Color32(255,255,255), "Left Analogue Stick - Zone X + Y position ");
				grcDebugDraw::Text(Vector2(0.68f, 0.26f), Color32(255,255,255), "Right Analogue Stick - Zone X + Y size");
				grcDebugDraw::Text(Vector2(0.68f, 0.28f), Color32(255,255,255), "D-pad Up / D-pad Down - Zone Z position");
				grcDebugDraw::Text(Vector2(0.68f, 0.30f), Color32(255,255,255), "D-pad Left / D-pad Right - Zone Z size");
				break;
			case AMBIENT_ZONE_DIMENSION_X_ONLY:
				grcDebugDraw::Text(Vector2(0.68f, 0.20f), Color32(255,255,255), "Left Analogue Stick - Zone X position");
				grcDebugDraw::Text(Vector2(0.68f, 0.22f), Color32(255,255,255), "Right Analogue Stick - Zone X position fine");
				grcDebugDraw::Text(Vector2(0.68f, 0.24f), Color32(255,255,255), "L2 / R2 - Zone X size");
				grcDebugDraw::Text(Vector2(0.68f, 0.26f), Color32(255,255,255), "L1 / R1 - Zone X size fine");				
				break;
			case AMBIENT_ZONE_DIMENSION_Y_ONLY:
				grcDebugDraw::Text(Vector2(0.68f, 0.20f), Color32(255,255,255), "Left Analogue Stick - Zone Y position");
				grcDebugDraw::Text(Vector2(0.68f, 0.22f), Color32(255,255,255), "Right Analogue Stick - Zone Y position fine");
				grcDebugDraw::Text(Vector2(0.68f, 0.24f), Color32(255,255,255), "L2 / R2 - Zone Y size");
				grcDebugDraw::Text(Vector2(0.68f, 0.26f), Color32(255,255,255), "L1 / R1 - Zone Y size fine");
				break;
			case AMBIENT_ZONE_DIMENSION_Z_ONLY:
				grcDebugDraw::Text(Vector2(0.68f, 0.18f), Color32(255,255,255), "Left Analogue Stick - Zone Z position");
				grcDebugDraw::Text(Vector2(0.68f, 0.20f), Color32(255,255,255), "Right Analogue Stick - Zone Z position fine");
				grcDebugDraw::Text(Vector2(0.68f, 0.22f), Color32(255,255,255), "L2 / R2 - Zone Z size");
				grcDebugDraw::Text(Vector2(0.68f, 0.24f), Color32(255,255,255), "L1 / R1 - Zone Z size fine");
				break;
			}
		}
	}
}
#endif

audAmbientAudioEntity::audAmbientAudioEntity() :
m_ZoneQuadTree(fwRect(WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_XMAX, WORLDLIMITS_REP_YMAX), QUAD_TREE_DEPTH),
m_ShoreLinesQuadTree(fwRect(WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_XMAX, WORLDLIMITS_REP_YMAX), QUAD_TREE_DEPTH)
{
}

audAmbientAudioEntity::~audAmbientAudioEntity()
{
}

Vec3V_Out audAmbientAudioEntity::GetListenerPosition(audAmbientZone* ambientZone) const
{
	const AmbientZone* zoneData = ambientZone ? ambientZone->GetZoneData() : NULL;
	Vec3V listenerPosition = g_AudioEngine.GetEnvironment().GetPanningListenerPosition();

#if __BANK
	if( g_ZoneActivationType == ZONE_ACTIVATION_TYPE_PLAYER )
	{
		listenerPosition = CGameWorld::FindLocalPlayer()->GetTransform().GetPosition();
	}
	else if( g_ZoneActivationType == ZONE_ACTIVATION_TYPE_NONE )
	{
		// Just set to some value miles away from the level
		listenerPosition = Vec3V(9999.0f, 9999.0f, 9999.0f);
	}
	else 
#endif
	if(zoneData && (AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_USEPLAYERPOSITION) == AUD_TRISTATE_TRUE || (ambientZone && ambientZone->GetNameHash() == ATSTRINGHASH("AZ_ch_dlc_arcade_Office_Area", 0xD1D3D72F) && audNorthAudioEngine::IsFirstPersonActive())))
	{
		listenerPosition = CGameWorld::FindLocalPlayer()->GetTransform().GetPosition();
	}

	return listenerPosition;
}

void audAmbientAudioEntity::Init()
{
#if __BANK
	if(PARAM_disableambientaudio.Get())
	{
		g_EnableAmbientAudio = false;
	}
#endif

	m_LoadedLevelName = "";
	naAudioEntity::Init();
	InitPrivate();
}

void audAmbientAudioEntity::InitPrivate()
{
	USE_MEMBUCKET(MEMBUCKET_AUDIO);

	for(u32 i = 0; i < g_MaxRegisteredAudioEffects; i++)
	{
		m_RegisteredAudioEffects[i].inUse = false;
	}

	m_PlayerSpeedVariable = g_AudioEngine.GetSoundManager().GetVariableAddress(ATSTRINGHASH("Game.Player.Speed", 0x8CE50515));
	audAssert(m_PlayerSpeedVariable);

	m_IsPLayerInTheCity = true; 
	m_OccludeRain = false; 

	m_ForceQuadTreeUpdate = false;
	m_RequestedAudioEffectsWriteIndex = 0;
	m_NumRequestedAudioEffects[0] = 0;
	m_NumRequestedAudioEffects[1] = 0;
	m_NumFramesInteriorActive = 0;
	m_LastHighwayPassbyTime = 0;
	m_PrevRoadInfo = NULL;
	m_WasPlayerDead = false;

	m_FakeRadioEmitterSettingsLastFrame = NULL;
	m_FakeRadioEmitterState = AMBIENT_RADIO_OFF;
	m_FakeRadioEmitterType = AMBIENT_RADIO_TYPE_STATIC;
	m_AmbientRadioSettings = NULL;
	m_FakeRadioEmitterListenerSpawnPoint = Vec3V(V_ZERO);
	m_FakeRadioEmitterVolLin = 0.0f;
	m_FakeRadioEmitterDistance = 0.0f;
	m_FakeRadioEmitterTimeActive = 0.0f;
	m_FakeRadioEmitterLifeTime = -1.0f;
	m_FakeRadioEmitterStartPan = 0;
	m_FakeRadioEmitterEndPan = 0;
	m_NextFakeStaticRadioTriggerTime = 0;
	m_NextFakeVehicleRadioTriggerTime = 0;
	m_FakeRadioEmitterAttackTime = 0;
	m_FakeRadioEmitterReleaseTime = 0;
	m_FakeRadioEmitterHoldTime = 0;
	m_FakeRadioEmitterRadius = 0;
	m_FakeRadioEmitterRadioStation = g_OffRadioStation;
	m_FakeRadioEmitterRadioStationHash = 0;

	m_ClosestShoreIdx = -1;
	m_RainOnWaterSounds.Init(ATSTRINGHASH("RAIN_ON_WATER", 0x6012B8B));
	m_WallaSpeechSlotID = -1;
	m_WallaPedGroupProcessingIndex = 0;

	for(u32 loop = 0; loop < WALLA_TYPE_MAX; loop++)
	{
		m_ExteriorWallaSounds[loop] = NULL;
		m_InteriorWallaSounds[loop] = NULL;
	}

	m_CurrentWindZone = NULL; 
	m_RainOnWaterSound = NULL;
	m_MinValidPedDensityExterior = 0.0f;
	m_MaxValidPedDensityExterior = 1.0f;
	m_MinValidPedDensityInterior = 0.0f;
	m_MaxValidPedDensityInterior = 1.0f;
	m_PedDensityScalarExterior = 1.0f;
	m_PedDensityScalarInterior = 1.0f;
	m_LoudSoundExclusionRadius = 0.0f;
	m_WallaSpeechTimer = 0.0f;
	m_ForcePanic = 0.0f;
	m_UnderWaterScene = NULL;
	m_UnderWaterSubScene = NULL;
	m_NumHighwayNodesFound = 0;
	m_NumHighwayNodesInRadius = 0;
	m_RoadNodesToHighwayVol.Init(ATSTRINGHASH("ROAD_NODE_DENSITY_TO_HIGHWAY_VOL", 0x2C49DF65));
	m_MaxPedDensityToWallaSpeechTriggerRate.Init(ATSTRINGHASH("PED_DENSITY_TO_WALLA_SPEECH_TRIGGER_RATE", 0xE53B4AB0));

	m_DefaultExteriorWallaSoundSetName = ATSTRINGHASH("EXTERIOR_WALLA_DEFAULT", 0x8A963FB6);
	m_ExteriorWallaSoundSet.Init(m_DefaultExteriorWallaSoundSetName);
	m_InteriorWallaSoundSet.Reset();

	m_GlobalHighwayVolumeSmoother.Init(0.00005f, true);

	m_CurrentExteriorWallaSoundSetName = m_DefaultExteriorWallaSoundSetName;
	m_CurrentInteriorWallaSoundSetName = g_NullSoundHash;
	m_DoesInteriorWallaRequireStreamSlot = false;
	m_DoesExteriorWallaRequireStreamSlot = DoesSoundRequireStreamSlot(m_ExteriorWallaSoundSet.Find(ATSTRINGHASH("Heavy", 0xB1AEBE06)));
	m_ExteriorWallaSwitchActive = false;
	m_InteriorWallaSwitchActive = false;
	m_WorldHeightFactor = 0.0f;
	m_UnderwaterCreakFactor = 0.0f;

	m_WallaSpeechSelectedVoice = 0u;
	m_WallaSpeechSelectedContext = 0u;

	m_CurrentInteriorRoomWallaSoundSet = g_NullSoundHash;
	m_CurrentInteriorRoomHasWalla = false;

	m_UnderWaterSoundSet.Init(ATSTRINGHASH("UNDER_WATER_SOUNDSET", 0x4746F143));
	m_UnderWaterDeepSound = NULL;
	m_WallaSpeechSound = NULL;
	m_UnderwaterStreamSlot = NULL;
	m_InteriorWallaStreamSlot = NULL;
	m_ExteriorWallaStreamSlot = NULL;
#if __BANK
	g_CurrentEditZone = NULL;
#endif
	REPLAY_ONLY(m_ReplayOverrideUnderWaterStream = false;);
	m_ZoneQuadTree = fwQuadTreeNode(fwRect(WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_XMAX, WORLDLIMITS_REP_YMAX), QUAD_TREE_DEPTH);

	for(u32 i = 0; i < 2; i++)
	{
		m_PanicWallaSpeechSlotIds[i] = -1;
		m_PanicWallaSpeechSlots[i] = NULL;
	}

	m_PanicWallaSound = NULL;
	m_PanicAssetPrepareState = PANIC_ASSETS_REQUEST_SLOTS;

	CheckAndAddZones();
	
	g_BlockedToVolume.Init("BLOCKED_TO_LINEAR_VOLUME");
	g_BlockedToPedDensityScale.Init("BLOCKED_TO_PED_DENSITY_SCALAR");
	g_LoudSoundExclusionRadiusCurve.Init("AMBIENCE_CURVES_LOUD_SOUND_EXCLUSION_RADIUS");

	m_NumFramesInteriorActive = 0;
	m_InteriorRoomIndexLastFrame = -1;

	m_WallaSmootherExterior.Init(g_PedDensitySmoothingIncrease, g_PedDensitySmoothingDecrease, true);
	m_WallaSmootherInterior.Init(g_PedDensitySmoothingInterior, true);
	m_WallaSmootherHostile.Init(g_PedDensitySmoothingIncrease, true);

	m_TimeOfDayToPassbyFrequency.Init("TIME_OF_DAY_TO_PASSBY_FREQUENCY");
	const f32 hours = static_cast<f32>(CClock::GetHour()) + (CClock::GetMinute()/60.f);
	m_HighwayPassbyTimeFrequency = m_TimeOfDayToPassbyFrequency.CalculateValue(hours) * audEngineUtil::GetRandomNumberInRange(0.5f, 3.0f);

	m_NumPedToWallaExterior.Init("WALLA_NUM_PEDS_TO_DENSITY");
	m_NumPedToWallaHostile.Init("WALLA_NUM_HOSTILE_PEDS_TO_DENSITY");
	m_NumPedToWallaInterior.Init("WALLA_NUM_PEDS_TO_DENSITY_INTERIOR");
	m_PedWallaDensityPostPanic.Init("PED_WALLA_DENSITY_POST_PANIC");
	m_PedDensityToPanicVol.Init("PED_DENSITY_TO_PANIC_VOL");
	m_ScriptDesiredPedDensityExteriorApplyFactor = 0.0f;
	m_ScriptDesiredPedDensityExterior = 1.0f;
	m_ScriptControllingPedDensityExterior = -1;
	m_ScriptDesiredPedDensityInteriorApplyFactor = 0.0f;
	m_ScriptDesiredPedDensityInterior = 1.0f;
	m_ScriptControllingPedDensityInterior = -1;
	m_ScriptControllingExteriorWallaSoundSet = -1;
	m_ScriptControllingInteriorWallaSoundSet = -1;
	m_PedPanicTimer = 0;
	m_PanicWallaBank = g_AudioEngine.GetSoundManager().GetFactory().GetBankIndexFromName("S_MISC\\PANIC_WALLA");
	m_DefaultRoadInfo = audNorthAudioEngine::GetObject<AudioRoadInfo>(ATSTRINGHASH("DEFAULT", 0xE4DF46D5));
	m_CurrentPedWallaSpeechSettings = NULL;
	m_RMSLevelSmoother.Init(1.0f/g_SFXRMSSmoothingRateIncrease, 1.0f/g_SFXRMSSmoothingRateDecrease, true);

	Vector3 coneDir = Vector3(0.0f,0.0f,-1.0f);
	m_HighwayPassbyVolumeCone.Init("LINEAR_RISE", coneDir, -1.0f, 40.0f, 70.0f);

	for(u32 loop = 0; loop < kMaxActiveAlarms; loop++)
	{
		m_ActiveAlarms[loop].alarmSettings = NULL;
	}

	ResetEnvironmentRule();

	m_LastUpdateTimeMs = 0;

#if __BANK
	m_DebugShoreLine = NULL;
#endif
	m_LastTimePlayerInWater = 0;
}

#if __BANK
void audAmbientAudioEntity::HandleRaveZoneCreatedNotification(AmbientZone* newZoneData)
{
	sysCriticalSection lock(m_Lock);
	g_RaveCreatedZone = newZoneData;
}

void audAmbientAudioEntity::ProcessRaveZoneCreatedNotification(AmbientZone* newZoneData)
{
	atString zoneName = atString(audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(newZoneData->NameTableOffset));

	if(zoneName.StartsWith("AZ_"))
	{
		for(u32 i = 0; i < m_Zones.GetCount(); i++)
		{
			if(strcmp(m_Zones[i]->GetName(), zoneName.c_str()) == 0)
			{
				// Already added
				if(m_Zones[i]->GetZoneData() != newZoneData)
				{
					m_Zones[i]->StopAllSounds();
					m_Zones[i]->Init(newZoneData, atStringHash(zoneName), false);
					m_Zones[i]->CalculateDynamicBankUsage();
				}

				return;
			}
		}

		for(u32 i = 0; i < m_Zones.GetCount(); i++)
		{
			m_Zones[i]->SetActive(false);
		}
	
		m_ActiveZones.Reset();

		audAmbientZone* newZone = rage_new audAmbientZone();
		newZone->Init(newZoneData, atStringHash(zoneName), false);
		newZone->CalculateDynamicBankUsage();
		m_ZoneQuadTree.AddItemRestrictDupes((void*)newZone, newZone->GetActivationZoneAABB());
		m_Zones.PushAndGrow(newZone);
	}
	else
	{
		audWarningf("Ignoring new zone - does not start with AZ_ prefix");
	}
}
void audAmbientAudioEntity::HandleRaveShoreLineCreatedNotification(ShoreLineAudioSettings* settings)
{
	sysCriticalSection lock(m_Lock);

	sysMemAllocator* debugHeap = &sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

	if(m_DebugShoreLine)
	{
		m_DebugShoreLine->Load(settings);
		m_ShoreLinesQuadTree.AddItemRestrictDupes((void*)m_DebugShoreLine, fwRect(m_DebugShoreLine->GetActivationBoxAABB()));
		// after adding the new shoreline to the quad tree, free up the reference for future creation
		m_DebugShoreLine = NULL;
		m_CurrentDebugShoreLineIndex = 0;
		sm_ShoreLineEditorState = STATE_WAITING_FOR_ACTION;
		naDisplayf("Done!");
	}

	// restore the debug heap
	sysMemAllocator::SetCurrent(*debugHeap);

}
void audAmbientAudioEntity::HandleRaveShoreLineEditedNotification()
{
	m_DebugShoreLine = NULL;
	m_CurrentDebugShoreLineIndex = 0;
	sm_ShoreLineEditorState = STATE_WAITING_FOR_ACTION;
	naDisplayf("Done!");
}
#endif

void audAmbientAudioEntity::Shutdown()
{
	DestroyAllZonesAndShorelines();
	
	for(u32 i = 0; i < 2; i++)
	{
		for(u32 j = 0; j < g_MaxRegisteredAudioEffects; j++)
		{
			m_RequestedAudioEffects[i][j].entity = NULL;
		}
	}
	for(u32 i = 0; i < g_MaxRegisteredAudioEffects; i++)
	{
		if(m_RegisteredAudioEffects[i].sound)
		{
			naCErrorf(m_RegisteredAudioEffects[i].inUse, "Registered audio effect has a sound but is not in use");
			m_RegisteredAudioEffects[i].sound->StopAndForget();
		}
		if(m_RegisteredAudioEffects[i].occlusionGroup)
		{
			naCErrorf(m_RegisteredAudioEffects[i].inUse, "Registered audio effect has an occlusion group but is not in use");
			m_RegisteredAudioEffects[i].occlusionGroup->RemoveSoundReference();
			m_RegisteredAudioEffects[i].occlusionGroup = NULL;
		}
	}

	m_LoadedLevelName = "";
	sm_WasCameraUnderWater = false;
	if(m_UnderWaterScene)
	{
		m_UnderWaterScene->Stop();
		m_UnderWaterScene = NULL;
	}
	if(m_UnderWaterSubScene)
	{
		m_UnderWaterSubScene->Stop();
		m_UnderWaterSubScene = NULL;
	}
	audEntity::Shutdown();
}

void audAmbientAudioEntity::IncrementAudioEffectWriteIndex()
{
	m_RequestedAudioEffectsWriteIndex = (m_RequestedAudioEffectsWriteIndex + 1) % 2;
	m_NumRequestedAudioEffects[m_RequestedAudioEffectsWriteIndex] = 0;
}

void audAmbientAudioEntity::RegisterEffectAudio(const u32 soundNameHash, const fwUniqueObjId uniqueId, const Vector3 &pos, CEntity *entity, const f32 vol, const s32 pitch)
{
	if(!fwTimer::IsGamePaused())
	{
		if(audVerifyf(m_NumRequestedAudioEffects[m_RequestedAudioEffectsWriteIndex] < g_MaxRegisteredAudioEffects, "Failed to register audio effect"))
		{
			m_RequestedAudioEffects[m_RequestedAudioEffectsWriteIndex][m_NumRequestedAudioEffects[m_RequestedAudioEffectsWriteIndex]].soundNameHash = soundNameHash;
			m_RequestedAudioEffects[m_RequestedAudioEffectsWriteIndex][m_NumRequestedAudioEffects[m_RequestedAudioEffectsWriteIndex]].uniqueId = uniqueId;
			m_RequestedAudioEffects[m_RequestedAudioEffectsWriteIndex][m_NumRequestedAudioEffects[m_RequestedAudioEffectsWriteIndex]].pos = pos;
			m_RequestedAudioEffects[m_RequestedAudioEffectsWriteIndex][m_NumRequestedAudioEffects[m_RequestedAudioEffectsWriteIndex]].vol = vol;
			m_RequestedAudioEffects[m_RequestedAudioEffectsWriteIndex][m_NumRequestedAudioEffects[m_RequestedAudioEffectsWriteIndex]].pitch = pitch;
			m_RequestedAudioEffects[m_RequestedAudioEffectsWriteIndex][m_NumRequestedAudioEffects[m_RequestedAudioEffectsWriteIndex]].entity = entity;
			m_NumRequestedAudioEffects[m_RequestedAudioEffectsWriteIndex]++;
		}		
	}
}

void audAmbientAudioEntity::StartSpawnEffect(ptxEffectInst *inst)
{
	if(!fwTimer::IsGamePaused())
	{
		if(g_SpawnEffectsEnabled)
		{
			const char *name = inst->GetEffectRule()->GetName();
			const u32 nameHash = atStringHash(name);

			// B*3739018 - Special case code to prevent large water splashes triggering for seaplanes landing in water
			if(nameHash == ATSTRINGHASH("water_splash_plane_in", 0xE307DD4))
			{
				ptfxAttachInfo* pAttachInfo = ptfxAttach::Find(inst);
				
				if(pAttachInfo && pAttachInfo->m_pAttachEntity)
				{
					CEntity* entity = (CEntity*)pAttachInfo->m_pAttachEntity.Get();

					if(entity->GetIsTypeVehicle())
					{
						audVehicleAudioEntity* vehicleAudioEntity = ((CVehicle*)entity)->GetVehicleAudioEntity();

						if(vehicleAudioEntity && vehicleAudioEntity->IsSeaPlane())
						{
							return;
						}
					}
				}
			}

			if(g_AudioEngine.GetSoundManager().GetFactory().GetMetadataPtr(nameHash))
			{
				sysCriticalSection lock(m_Lock);
				for(u32 i = 0; i < g_MaxRegisteredAudioEffects; i++)
				{
					if(!m_RegisteredAudioEffects[i].inUse)
					{
						m_RegisteredAudioEffects[i].inst = inst;
						m_RegisteredAudioEffects[i].requestedThisFrame = true;
						m_RegisteredAudioEffects[i].uniqueId = (fwUniqueObjId)~0U;
						m_RegisteredAudioEffects[i].sound = NULL;
						m_RegisteredAudioEffects[i].inUse = true;
						m_RegisteredAudioEffects[i].needsPlay = true;
						return;
					}
				}
			}
		}
	}
}	

void audAmbientAudioEntity::StopSpawnEffect(ptxEffectInst *inst)
{
	if(g_SpawnEffectsEnabled)
	{
		sysCriticalSection lock(m_Lock);
		for(u32 i = 0; i < g_MaxRegisteredAudioEffects; i++)
		{
			if(m_RegisteredAudioEffects[i].inUse && m_RegisteredAudioEffects[i].inst == inst)
			{
				if(m_RegisteredAudioEffects[i].sound)
				{
					m_RegisteredAudioEffects[i].sound->StopAndForget();
				}
			}
		}
	}
}

void audAmbientAudioEntity::RecycleSpawnEffect(ptxEffectInst *inst)
{
	if(g_SpawnEffectsEnabled)
	{
		const char *name = inst->GetEffectRule()->GetName();
		const u32 nameHash = atStringHash(name);
		if(SOUNDFACTORY.GetMetadataPtr(nameHash))
		{
			sysCriticalSection lock(m_Lock);
			for(u32 i = 0; i < g_MaxRegisteredAudioEffects; i++)
			{
				if(m_RegisteredAudioEffects[i].inUse && m_RegisteredAudioEffects[i].inst == inst)
				{
					m_RegisteredAudioEffects[i].inst = NULL;
					return;
				}
			}
		}
	}
}

void audAmbientAudioEntity::AddDirectionalAmbience(DirectionalAmbience* directionalAmbience, audAmbientZone* parentZone, f32 volumeScale)
{
	if(directionalAmbience)
	{
		// Check through all the existing ambience managers and see if we haven't already created this one
		for(u32 loop = 0; loop < m_DirectionalAmbienceManagers.GetCount(); loop++)
		{
			if(m_DirectionalAmbienceManagers[loop].GetDirectionalAmbience() == directionalAmbience)
			{
				m_DirectionalAmbienceManagers[loop].RegisterParentZone(parentZone, volumeScale);
				return;
			}
		}

		audDirectionalAmbienceManager directionalManager;
		directionalManager.Init(directionalAmbience);
		directionalManager.RegisterParentZone(parentZone, volumeScale);

		const AmbientBankMap* ambienceBankMap = audNorthAudioEngine::GetObject<AmbientBankMap>(ATSTRINGHASH("AMBIENCE_BANK_MAP_AUTOGENERATED", 0x08e3df280));

		for(u32 directionLoop = 0; directionLoop < 4; directionLoop++)
		{
			DynamicBankListBuilderFn bankListBuilder;
			SOUNDFACTORY.ProcessHierarchy(directionalManager.GetSoundHash(directionLoop), bankListBuilder);

			if(bankListBuilder.dynamicBankList.GetCount() == 1)
			{
				directionalManager.SetDynamicBankID(directionLoop, bankListBuilder.dynamicBankList[0]);

				// Ok - we know which bank this sound requires. Now we need to work out what slot type that bank gets loaded into
				if(ambienceBankMap)
				{
					u32 bankHash = atStringHash(g_AudioEngine.GetSoundManager().GetFactory().GetBankNameFromIndex(bankListBuilder.dynamicBankList[0]));

					for(u32 bankMapLoop = 0; bankMapLoop < ambienceBankMap->numBankMaps; bankMapLoop++)
					{
						// So we've found a bank->slot mapping for our bank
						if(ambienceBankMap->BankMap[bankMapLoop].BankName == bankHash)
						{
							// Record the relevant slot in the rule. Now the rule is fully clued up - it know what bank it
							// needs to load and what slot type it needs to go into
							directionalManager.SetDynamicSlotType(directionLoop, ambienceBankMap->BankMap[bankMapLoop].SlotType);
						}
					}
				}
			}

			naAssertf(bankListBuilder.dynamicBankList.GetCount() < 2, 
				"Directional ambience sound is referencing more than one dynamically loaded bank (%s + %s)", 
				g_AudioEngine.GetSoundManager().GetFactory().GetBankNameFromIndex(bankListBuilder.dynamicBankList[0]),
				g_AudioEngine.GetSoundManager().GetFactory().GetBankNameFromIndex(bankListBuilder.dynamicBankList[1]));
		}

		m_DirectionalAmbienceManagers.PushAndGrow(directionalManager, 1);
	}
}
//----------------------------------------------------------------------------------------------------------------------
void audAmbientAudioEntity::UpdateSound(audSound *sound, audRequestedSettings *reqSets, u32 /*timeInMs*/)
{
	f32 playerInteriorRatio = 0.0f;
	f32 interiorOffsetVolume = 1.0f;
	u32 clientVariable;
	reqSets->GetClientVariable(clientVariable);
	audEnvironmentSounds soundId = (audEnvironmentSounds)clientVariable;
	switch(soundId)
	{
	case AUD_ENV_SOUND_WIND:
		audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior(NULL, &playerInteriorRatio);
		playerInteriorRatio = ClampRange(playerInteriorRatio,0.f,1.f);
		interiorOffsetVolume = audDriverUtil::ComputeDbVolumeFromLinear(1.f - playerInteriorRatio);
		sound->SetRequestedVolume(interiorOffsetVolume);
		if( interiorOffsetVolume <= g_SilenceVolume)
		{
			sound->StopAndForget();
		}
		break;
	default:
		break;
	}
}

void audAmbientAudioEntity::Update(const u32 timeInMs)
{
	const u32 timeStepMs = timeInMs > m_LastUpdateTimeMs ? timeInMs - m_LastUpdateTimeMs : 0;
	const float timeStepS = timeStepMs * 0.001f;
	m_LastUpdateTimeMs = timeInMs;

	sysCriticalSection lock(m_Lock);
	const u32 readIndex = (m_RequestedAudioEffectsWriteIndex + 1) % 2;
	for(u32 i = 0; i < g_MaxRegisteredAudioEffects; i++)
	{
		if(!m_RegisteredAudioEffects[i].inst)
		{
			m_RegisteredAudioEffects[i].requestedThisFrame = false;
		}
		m_RequestedAudioEffects[readIndex][i].processed = false;
	}

	PF_SET(NumRequestedAudioEffects, m_NumRequestedAudioEffects[readIndex]);

	// update existing sounds
	for(u32 i = 0; i < m_NumRequestedAudioEffects[readIndex]; i++)
	{
		// search for this instance in the list
		for(u32 j = 0; j < g_MaxRegisteredAudioEffects; j++)
		{
			if(m_RegisteredAudioEffects[j].inUse &&
				m_RegisteredAudioEffects[j].inst == NULL && m_RegisteredAudioEffects[j].uniqueId == m_RequestedAudioEffects[readIndex][i].uniqueId)
			{
				// found a match; update this sound
				m_RegisteredAudioEffects[j].requestedThisFrame = true;
				m_RequestedAudioEffects[readIndex][i].processed = true;
				if(!m_RegisteredAudioEffects[j].sound)
				{
					audSoundInitParams initParams;
					initParams.Position = m_RequestedAudioEffects[readIndex][i].pos;
					initParams.Volume = m_RequestedAudioEffects[readIndex][i].vol;
					initParams.Pitch = (s16)m_RequestedAudioEffects[readIndex][i].pitch;
					CreateAndPlaySound_Persistent(m_RequestedAudioEffects[readIndex][i].soundNameHash, &m_RegisteredAudioEffects[j].sound, &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(m_RequestedAudioEffects[readIndex][i].soundNameHash, &initParams, m_RegisteredAudioEffects[j].sound, NULL, eAmbientSoundEntity);)
				}
				else
				{
					m_RegisteredAudioEffects[j].sound->SetRequestedPosition(m_RequestedAudioEffects[readIndex][i].pos);
					m_RegisteredAudioEffects[j].sound->SetRequestedPitch(m_RequestedAudioEffects[readIndex][i].pitch);
					m_RegisteredAudioEffects[j].sound->SetRequestedVolume(m_RequestedAudioEffects[readIndex][i].vol);
				}
				if(m_RegisteredAudioEffects[j].occlusionGroup && m_RequestedAudioEffects[readIndex][i].entity)
				{
					m_RegisteredAudioEffects[j].occlusionGroup->SetSource(VECTOR3_TO_VEC3V(m_RequestedAudioEffects[readIndex][i].pos));
					m_RegisteredAudioEffects[j].occlusionGroup->ForceSourceEnvironmentUpdate(m_RequestedAudioEffects[readIndex][i].entity);
				}
	
				break;
			}
		}
	}

	// uberhack
	g_IsHydrantSpraying = false;
	// stop unrequested sounds to make room for new ones
	for(u32 i = 0; i < g_MaxRegisteredAudioEffects; i++)
	{
		if(m_RegisteredAudioEffects[i].inUse)
		{
			if(!m_RegisteredAudioEffects[i].requestedThisFrame)
			{
				if(m_RegisteredAudioEffects[i].sound)
				{
					m_RegisteredAudioEffects[i].sound->StopAndForget();
				}
				if(m_RegisteredAudioEffects[i].occlusionGroup)
				{
					m_RegisteredAudioEffects[i].occlusionGroup->RemoveSoundReference();
					m_RegisteredAudioEffects[i].occlusionGroup = NULL;
				}
				m_RegisteredAudioEffects[i].inUse = false;
			}
			else
			{
				if(m_RegisteredAudioEffects[i].inst)
				{
					if(m_RegisteredAudioEffects[i].needsPlay)
					{
						m_RegisteredAudioEffects[i].needsPlay = false;
						Assert(!m_RegisteredAudioEffects[i].sound);
						CreateAndPlaySound_Persistent(m_RegisteredAudioEffects[i].inst->GetEffectRule()->GetName(), &m_RegisteredAudioEffects[i].sound);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(atStringHash(m_RegisteredAudioEffects[i].inst->GetEffectRule()->GetName()), NULL, m_RegisteredAudioEffects[i].sound, NULL, eAmbientSoundEntity);)
					}
					if(m_RegisteredAudioEffects[i].sound)
					{
						const Vector3 pos = VEC3V_TO_VECTOR3(m_RegisteredAudioEffects[i].inst->GetCurrPos());
						if(FPIsFinite(pos.Mag2()))
						{
#if __BANK
							if(g_DrawSpawnFX)
							{
								grcDebugDraw::Sphere(pos, 0.3f, Color32(255,0,0));
							}
#endif
							if(m_RegisteredAudioEffects[i].sound->GetSoundTypeID() == VariableBlockSound::TYPE_ID)
							{
								const f32 collisionEvo = m_RegisteredAudioEffects[i].inst->GetEvoValueFromHash(ATSTRINGHASH("collision", 0x86618E11));
								m_RegisteredAudioEffects[i].sound->FindAndSetVariableValue(ATSTRINGHASH("collision", 0x86618E11), collisionEvo);
								if(collisionEvo == 0.f && (VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition()) - pos).Mag2() < (35.f*35.f))
								{
									// uberhack
									g_HydrantSprayPos = pos;
									g_IsHydrantSpraying = true;
								} 
							}
							m_RegisteredAudioEffects[i].sound->SetRequestedPosition(pos);
						}
					}
					else
					{
						if(m_RegisteredAudioEffects[i].occlusionGroup)
						{
							m_RegisteredAudioEffects[i].occlusionGroup->RemoveSoundReference();
							m_RegisteredAudioEffects[i].occlusionGroup = NULL;
						}
						m_RegisteredAudioEffects[i].inst = NULL;
						m_RegisteredAudioEffects[i].inUse = false;
					}
				}
			}		
		}
	}

	// start new sounds
	for(u32 i = 0; i < m_NumRequestedAudioEffects[readIndex]; i++)
	{
		if(!m_RequestedAudioEffects[readIndex][i].processed)
		{
			for(u32 j = 0; j < g_MaxRegisteredAudioEffects; j++)
			{
				if(!m_RegisteredAudioEffects[j].inUse)
				{
					m_RegisteredAudioEffects[j].inUse = true;
					m_RegisteredAudioEffects[j].uniqueId = m_RequestedAudioEffects[readIndex][i].uniqueId;
					if(m_RequestedAudioEffects[readIndex][i].entity)
					{
						if(m_RegisteredAudioEffects[j].occlusionGroup)
						{
							m_RegisteredAudioEffects[j].occlusionGroup->RemoveSoundReference();
							m_RegisteredAudioEffects[j].occlusionGroup = NULL;
						}
						m_RegisteredAudioEffects[j].occlusionGroup = naEnvironmentGroup::Allocate("AmbientEffect");
						if(m_RegisteredAudioEffects[j].occlusionGroup)
						{
							m_RegisteredAudioEffects[j].occlusionGroup->Init(NULL, 40.f);							
							m_RegisteredAudioEffects[j].occlusionGroup->SetSource(VECTOR3_TO_VEC3V(m_RequestedAudioEffects[readIndex][i].pos));
							m_RegisteredAudioEffects[j].occlusionGroup->ForceSourceEnvironmentUpdate(m_RequestedAudioEffects[readIndex][i].entity);
							m_RegisteredAudioEffects[j].occlusionGroup->AddSoundReference();
						}
					}
					audSoundInitParams initParams;
					initParams.Position = m_RequestedAudioEffects[readIndex][i].pos;
					initParams.Volume = m_RequestedAudioEffects[readIndex][i].vol;
					initParams.Pitch = (s16)m_RequestedAudioEffects[readIndex][i].pitch;  
					initParams.EnvironmentGroup = m_RegisteredAudioEffects[j].occlusionGroup;
					Assert(!m_RegisteredAudioEffects[j].sound);
					CreateAndPlaySound_Persistent(m_RequestedAudioEffects[readIndex][i].soundNameHash, &m_RegisteredAudioEffects[j].sound, &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(m_RequestedAudioEffects[readIndex][i].soundNameHash, &initParams, m_RegisteredAudioEffects[j].sound, NULL, eAmbientSoundEntity, ReplaySound::CONTEXT_AMBIENT_REGISTERED_EFFECTS);)
					break;
				}
			}
		}
	}

#if __BANK && __DEV
	if(g_DrawRegisteredEffects)
	{
		for(u32 j = 0; j < g_MaxRegisteredAudioEffects; j++)
		{
			if(m_RegisteredAudioEffects[j].inUse && m_RegisteredAudioEffects[j].sound)
			{
				grcDebugDraw::Sphere(m_RegisteredAudioEffects[j].sound->GetRequestedPosition(), 1.f, Color32(0.f,1.f,0.5f,Max(0.25f,audDriverUtil::ComputeLinearVolumeFromDb(m_RegisteredAudioEffects[j].sound->GetRequestedVolume()))));
				char buf[128];
				Vector3 pos = VEC3V_TO_VECTOR3(m_RegisteredAudioEffects[j].sound->GetRequestedPosition());
				formatf(buf, sizeof(buf), "%s: %f, %d", m_RegisteredAudioEffects[j].sound->GetName(), m_RegisteredAudioEffects[j].sound->GetRequestedVolume(), m_RegisteredAudioEffects[j].sound->GetRequestedPitch());
				grcDebugDraw::Text(pos, Color32(255,255,0), buf);
			}
		}
	}
	if(sm_DrawPlayingShoreLines)
	{
		audShoreLineRiver::DrawPlayingShorelines();
	}
#endif
	// update zones

#if __BANK
	if(!g_EnableAmbientAudio)
	{
		return;
	}

	g_numActiveZones = 0;
		 
	for(u32 i = 0; i < m_Zones.GetCount(); i++)
	{
		if(m_Zones[i]->IsActive())
		{
			g_numActiveZones++;
		}
	}
#endif // __BANK

	UpdateWalla();
	UpdateWallaSpeech();	
	UpdateAlarms();
	UpdateWorldHeight();
	UpdateStreamedAmbientSounds();

	Vec3V prevZoneActivationPostion = m_ZoneActivationPosition;
	m_ZoneActivationPosition = GetListenerPosition();
	const bool listenerHasJumped = IsGreaterThan(MagSquared(m_ZoneActivationPosition - prevZoneActivationPostion), ScalarV(Vec::V4VConstantSplat<0x43C80000>())).Getb(); // 400
	const bool isRaining = g_weather.GetTimeCycleAdjustedRain() > GetMinRainLevel();

	if(listenerHasJumped)
	{
		m_ForceQuadTreeUpdate = true;
	}

	if(audNorthAudioEngine::GetLastLoudSoundTime() > 0)
	{
		// Use fwTimer to work out the elapsed time, as that's what GetLastLoudSoundTime uses
		m_LoudSoundExclusionRadius = g_LoudSoundExclusionRadiusCurve.CalculateValue(((f32)(fwTimer::GetTimeInMilliseconds() - audNorthAudioEngine::GetLastLoudSoundTime()))/1000.0f);
		if(m_LoudSoundExclusionRadius < 0.0f)
		{
			m_LoudSoundExclusionRadius = 0.0f;
		}
	}
	else
	{
		m_LoudSoundExclusionRadius = 0.0f;
	}

	// Do this before updating the zones, as it can load/unload banks
	const u16 gameTimeMinutes = static_cast<u16>((CClock::GetHour() * 60) + CClock::GetMinute());

	for(u32 loop = 0; loop < m_InteriorZones.GetCount(); loop++)
	{
		if(m_InteriorZones[loop].IsActive())
		{
			m_InteriorZones[loop].UpdateDynamicWaveSlots(gameTimeMinutes, isRaining, GetListenerPosition(&m_InteriorZones[loop]));
		}
	}
	m_IsPLayerInTheCity = false;
	m_OccludeRain = false;
	for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
	{
		m_ActiveZones[loop]->UpdateDynamicWaveSlots(gameTimeMinutes, isRaining, GetListenerPosition(m_ActiveZones[loop]));
		if(m_ActiveZones[loop] == sm_CityZone)
		{
			m_IsPLayerInTheCity = true;
		}
		if(m_ActiveZones[loop]->HasToOccludeRain())
		{
			m_OccludeRain = true;
		}
	}
	
	audAmbientZone::ResetMixerSceneApplyValues();
	UpdateInteriorZones(timeInMs, isRaining, m_LoudSoundExclusionRadius, gameTimeMinutes);

	// Go through and update all the active zones. We randomize the order in which we update the zones as this prevents distant zones from blocking closer 
	// zones from playing rules shared by both. Each rule gets given a retrigger timeout if it fails to play due to spawning too far away, so by randomizing 
	// the order we ensure that closer zones get a fair chance to play the rule too
	if(m_ActiveZones.GetCount())
	{
		s32 selectedZone = audEngineUtil::GetRandomNumberInRange(0, m_ActiveZones.GetCount() - 1);
		bool directionForwards = audEngineUtil::ResolveProbability(0.5f);

		for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
		{
			m_ActiveZones[selectedZone]->Update(timeInMs, GetListenerPosition(m_ActiveZones[selectedZone]), isRaining, m_LoudSoundExclusionRadius, gameTimeMinutes);
			selectedZone += directionForwards ? 1 : -1;

			if(selectedZone >= m_ActiveZones.GetCount())
			{
				selectedZone = 0;
			}
			else if(selectedZone < 0)
			{
				selectedZone = m_ActiveZones.GetCount() - 1;
			}
		}

		for(u32 loop = 0; loop < m_ActiveZones.GetCount(); )
		{
			if( !m_ActiveZones[loop]->IsActive() )
			{
				if(m_ActiveZones[loop]->GetEnvironmentRule() != 0)
					m_ActiveEnvironmentZones.DeleteMatches(m_ActiveZones[loop]);

				m_ActiveZones.Delete(loop);
			}
			else
			{
				loop++;
			}
		}
	}

	audAmbientZone::UpdateMixerSceneApplyValues();
	UpdateAmbientRadioEmitters();
	UpdateEnvironmentRule();

	if((fwTimer::GetSystemFrameCount() & 31) == PROCESS_AMBIENT_ZONE_QUAD_TREE || m_ForceQuadTreeUpdate)
	{
#if __BANK
		sm_numZonesReturnedByQuadTree = 0;
#endif

		ZonesQuadTreeUpdateFunction ambientZonesUpdateFunction(&m_ActiveZones, &m_ActiveEnvironmentZones, timeInMs, m_ZoneActivationPosition, isRaining, m_LoudSoundExclusionRadius, gameTimeMinutes);
		m_ZoneQuadTree.ForAllMatching(RCC_VECTOR3(m_ZoneActivationPosition), ambientZonesUpdateFunction);
		m_ForceQuadTreeUpdate = false;
	}

	audShoreLineOcean::ResetDistanceToShore();
	audShoreLineRiver::ResetDistanceToShore();
	audShoreLineLake::ResetDistanceToShore();
	// TODO - Can we re-enable this at some point? Would needs to store a list of active shorelines, as with ambient zones
	//if((fwTimer::GetSystemFrameCount() & 31) == PROCESS_SHORELINE_QUAD_TREE)
	{
		int activeShoreLineCount = m_ActiveShoreLines.GetCount();
		audShoreLine **cachedShoreLineStorage = Alloca(audShoreLine *, activeShoreLineCount + 1);		// +1, just in case it's 0
		atUserArray<audShoreLine*>	cachedShoreLines(cachedShoreLineStorage, (u16) activeShoreLineCount, true);
		sysMemCpy(cachedShoreLineStorage, m_ActiveShoreLines.GetElements(), sizeof(audShoreLine *) * activeShoreLineCount);

		m_ActiveShoreLines.Resize(0);
		ShoreLinesQuadTreeUpdateFunction shoreLinesUpdateFunction(&m_ActiveShoreLines);
		m_ShoreLinesQuadTree.ForAllMatching(RCC_VECTOR3(m_ZoneActivationPosition), shoreLinesUpdateFunction);
		for(u32 cachedShore = 0; cachedShore < activeShoreLineCount; cachedShore ++)
		{
			cachedShoreLines[cachedShore]->UpdateActivation();
		}
	}
	audShoreLineRiver::CheckShorelineChange();

	s32 closestIdx = m_ClosestShoreIdx;
	m_ClosestShoreIdx = -1;
	f32 closestDistance = LARGE_FLOAT;
	// Go through and update all the active shorelines. 
	for(u32 loop = 0; loop < m_ActiveShoreLines.GetCount(); loop ++)
	{
		m_ActiveShoreLines[loop]->Update(fwTimer::GetTimeStep());
		if(isRaining)
		{
			f32 distanceToShore = m_ActiveShoreLines[loop]->GetDistanceToShore();
			if(closestDistance > distanceToShore)
			{
				closestDistance = distanceToShore;
				m_ClosestShoreIdx = loop;
			}
		}
	}
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
	bool playerInWater = (pPlayer && pPlayer->GetIsInWater()) || (pVehicle && pVehicle->GetIsInWater());
	if(isRaining && m_ClosestShoreIdx != -1 && !playerInWater)
	{
		if(m_RainOnWaterSound)
		{
			m_RainOnWaterSound->SetReleaseTime(300);
			m_RainOnWaterSound->StopAndForget();
		}
		if(m_ClosestShoreIdx != closestIdx)
		{
			audShoreLine::StopRainOnWaterSound();
		}
		m_ActiveShoreLines[m_ClosestShoreIdx]->PlayRainOnWaterSound(m_RainOnWaterSounds.Find(ATSTRINGHASH("PLAYER_OUT_OF_WATER", 0x879DF093)));
	}
	else 
	{
		audShoreLine::StopRainOnWaterSound();
		if(playerInWater && isRaining)
		{
			if(!m_RainOnWaterSound)
			{
				audSoundInitParams initParams;
				initParams.AttackTime = 300;
				initParams.Position = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
				initParams.Volume = g_WeatherAudioEntity.GetRainVerticallAtt();
				CreateAndPlaySound_Persistent(m_RainOnWaterSounds.Find(ATSTRINGHASH("PLAYER_IN_WATER", 0xE90565B0)),&m_RainOnWaterSound,&initParams);
			}
			else
			{
				m_RainOnWaterSound->SetRequestedPosition(pPlayer->GetTransform().GetPosition());
				m_RainOnWaterSound->SetRequestedVolume(g_WeatherAudioEntity.GetRainVerticallAtt());
			}
		}
		else
		{
			if(m_RainOnWaterSound)
			{
				m_RainOnWaterSound->SetReleaseTime(300);
				m_RainOnWaterSound->StopAndForget();
			}
			m_ClosestShoreIdx = -1;
		}
	}
	// Update directional managers
#if __BANK
	if(sm_DrawActivationBox || sm_DrawShoreLines || sm_DrawPlayingShoreLines)
	{
		char txt[128];
		formatf(txt,"Number active shorelines :%u", m_ActiveShoreLines.GetCount());
		grcDebugDraw::AddDebugOutput(Color_white,txt);
	}
	if(g_StopAllDirectionalAmbiences)
	{
		for(u32 i = 0; i < m_DirectionalAmbienceManagers.GetCount(); i++)
		{
			m_DirectionalAmbienceManagers[i].Stop();
		}
	}
	else
#endif
	{
		UpdateDirectionalAmbiences(timeStepS);
	}

#if __BANK
	if(sm_TestForWaterAtPed)
	{
		CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
		if(pLocalPlayer)
		{
			Vector3 playerPosition = pLocalPlayer->GetPreviousPosition();
			grcDebugDraw::Sphere(playerPosition, 1.0f, IsPointOverWater(playerPosition)? Color32(0.0f, 0.0f, 1.0f, 0.4f) : Color32(0.0f, 1.0f, 0.0f, 0.4f));
		}
	}

	if(g_CurrentEditZone)
	{
		UpdateDebugZoneEditor();
	}

	m_WallaSmootherExterior.SetRates(g_PedDensitySmoothingIncrease, g_PedDensitySmoothingDecrease);
	m_WallaSmootherInterior.SetRate(g_PedDensitySmoothingInterior);
	m_WallaSmootherHostile.SetRate(g_PedDensitySmoothingIncrease);

	if(g_RaveCreatedZone)
	{
		ProcessRaveZoneCreatedNotification(g_RaveCreatedZone);
		g_RaveCreatedZone = NULL;
	}
#endif

#if 0
	if(fwTimer::GetSystemFrameCount() % 1800 == 0)
	{
		audVehicleAudioEntity::UpdateAmbientTrainStations();
	}
#endif

	const Vec3V listenerPos = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition();
	bool forceOverLand = false;
	
	// Go through all the active zones
	for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
	{
		// Check if this zone is forcing a water decision, and if the listener is within the positioning zone
		if(m_ActiveZones[loop]->GetZoneData()->ZoneWaterCalculation == AMBIENT_ZONE_FORCE_OVER_LAND &&
		   m_ActiveZones[loop]->IsPointInPositioningZoneFlat(listenerPos))
		{
			forceOverLand = true;
			break;
		}
	}

	bool isListenerOverWater = false;

	if(!forceOverLand) // We don't have any zones that are modifying the value, so just check the ocean shorelines as normal
	{
		WaterTestResultType waterType;
		f32 waterz;
		waterType = CVfxHelper::GetWaterZ(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(), waterz);
		isListenerOverWater = (waterType == WATERTEST_TYPE_OCEAN) && (!audShoreLineOcean::GetClosestShore() || (audShoreLineOcean::IsListenerOverOcean() && audShoreLineOcean::GetSqdDistanceIntoWater() >= sm_CameraDistanceToShoreThreshold));
	}

	if(isListenerOverWater)
	{
		audShoreLineOcean::PlayOutInTheOceanSound();
	}
	else
	{
		audShoreLineOcean::StopOutInTheOceanSound();
	}
}
// ----------------------------------------------------------------
// Force an update of all ocean shorelines so that we can update
// our Out to Sea metrics (ie. closest ocean shoreline + distance 
// from listener)
// ----------------------------------------------------------------
void audAmbientAudioEntity::ForceOceanShorelineUpdateFromNearestRoad()
{	
	// When the player warps to a point far out in the ocean, there may not be any shorelines in the vicinity. This breaks our 'is over ocean'
	// queries as they rely on checking whether a given point lies to the left or right of a shoreline (so if we have no shoreline, this can't be done).
	// This function takes the listener position and gradually moves it towards the nearest node road position in incremental steps, trying to find the 
	// first shoreline between us and the road. Useful if you need valid ocean metrics after warping.
	const Vector3 listPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
	const CNodeAddress nearestRoad = ThePaths.FindNodeClosestToCoors(listPos, 2500.f);
	audDisplayf("Forcing ocean shorline update from (%.02f, %.02f, %.02f", listPos.GetX(), listPos.GetY(), listPos.GetZ());

	if(!nearestRoad.IsEmpty())
	{
		const CPathNode *closestNode = ThePaths.FindNodePointerSafe(nearestRoad);
		
		if(closestNode)
		{
			const Vector3 closestNodePos = closestNode->GetPos();
			audDisplayf("Closest road node found at (%.02f, %.02f, %.02f) - Distance %.02f", closestNodePos.GetX(), closestNodePos.GetY(), closestNodePos.GetZ(), closestNodePos.Dist(listPos));

			class ShoreLinesQuadTreeCalculateClosestFunction : public fwQuadTreeNode::fwQuadTreeFn
			{
			public:
				void operator()(const Vector3& position, void* data)
				{
					audShoreLine* shoreLine = static_cast<audShoreLine*>(data);

					if(shoreLine && shoreLine->GetWaterType() == AUD_WATER_TYPE_OCEAN)
					{
						((audShoreLineOcean*)shoreLine)->ComputeClosestShore(VECTOR3_TO_VEC3V(position));
					}
				}
			};
			
			const u32 numIterations = 5;
			const Vector3 direction = closestNodePos - listPos;
			const Vector3 directionStep = direction/numIterations;
			audShoreLineOcean::ResetDistanceToShore(true);

			for(int loop = 0; loop <= 5 && !audShoreLineOcean::GetClosestShore(); loop++)
			{
				const Vector3 searchPosition = listPos + (directionStep * (f32)loop);
				ShoreLinesQuadTreeCalculateClosestFunction shoreLinesFunction;
				m_ShoreLinesQuadTree.ForAllMatching(searchPosition, shoreLinesFunction);
				audShoreLineOcean* closestShoreline = audShoreLineOcean::GetClosestShore();

				if(closestShoreline)
				{
					Vector3 centrePoint,leftPoint,rightPoint;
					closestShoreline->CalculatePoints(centrePoint,leftPoint,rightPoint);
				}
			}

			audAssertf(audShoreLineOcean::GetClosestShore(), "Failed to find closest shore");
		}
	}
}

// ----------------------------------------------------------------
// Get the minimum rain level to be classified as 'raining'
// ----------------------------------------------------------------
f32 audAmbientAudioEntity::GetMinRainLevel() const
{
	const f32 oldMinRainLevel = GetMinRainLevelForWeatherType(g_weather.GetPrevType().m_hashName);
	const f32 currentMinRainLevel = GetMinRainLevelForWeatherType(g_weather.GetNextType().m_hashName);
	return oldMinRainLevel + ((currentMinRainLevel - oldMinRainLevel) * g_weather.GetInterp());
}

// ----------------------------------------------------------------
// Get the min rain level depending on the current weather type
// ----------------------------------------------------------------
f32 audAmbientAudioEntity::GetMinRainLevelForWeatherType(const u32 weatherType)
{
	switch(weatherType)
	{
	case 0xB677829F: // "THUNDER"
		return 0.2f;
		break;
	default:
		return 0.3f;
		break;
	}
}

// ----------------------------------------------------------------
// Update the directional ambiences
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdateDirectionalAmbiences(const float timeStepS)
{
	const f32 heightAboveWorldBlanket = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.z - CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.x, MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.y);
	const Vec3V north = Vec3V(0.0f, 1.0f, 0.0f);
	const Vec3V east = Vec3V(1.0f,0.0f,0.0f);
	const Vec3V right = NormalizeFast(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().a());
	const ScalarV cosangle = Dot(right, north);

	const ScalarV angle = Arccos(Clamp(cosangle, ScalarV(V_NEGONE), ScalarV(V_ONE)));
	const ScalarV degrees = angle * ScalarV(V_TO_DEGREES);
	ScalarV actualDegrees;

	if(IsTrue(IsLessThanOrEqual(Dot(right, east), ScalarV(V_ZERO))))
	{
		actualDegrees = ScalarV(360.0f) - degrees;
	}
	else
	{
		actualDegrees = degrees;
	}

	s32 basePan = 360 - ((s32)actualDegrees.Getf() - 90);
	const s32 directionalPans[4] = {0,90,180,270};

	// Pre-calculate all the per-direction speaker volumes, as these will be the same for each directional ambience
	atRangeArray<f32, 16> perDirectionSpeakerVolumes;

	for(u32 direction = 0; direction < 4; direction++)
	{
		const s16 pan = (basePan + directionalPans[direction]) % 360;
		sm_MetricsInternal.environmentSoundMetrics = &sm_Metrics;

		// Just pretend we're using the positioned route so that ComputeSpeakerVolumesFromPosition gives us correct ear attenuation values
		sm_Metrics.routingMetric.effectRoute = EFFECT_ROUTE_POSITIONED;
		sm_Metrics.pan = pan;

		g_AudioEngine.GetEnvironment().ComputeSpeakerVolumesFromPan(&sm_MetricsInternal, audDriver::GetNumOutputChannels());

		for(u32 speakerIndex = 0; speakerIndex < 4; speakerIndex++)
		{
			perDirectionSpeakerVolumes[(direction * 4) + speakerIndex] = sm_MetricsInternal.relativeChannelVolumes[speakerIndex].GetLinear();
		}
	}

	for(u32 i = 0; i < m_DirectionalAmbienceManagers.GetCount(); i++)
	{
		m_DirectionalAmbienceManagers[i].Update(timeStepS, basePan, directionalPans, perDirectionSpeakerVolumes.GetElements(), heightAboveWorldBlanket);
	}
}

void audAmbientAudioEntity::UpdateEnvironmentRule()
{
	ResetEnvironmentRule();

	if(audNorthAudioEngine::GetGtaEnvironment() && m_ActiveEnvironmentZones.GetCount() > 0)
	{
		atArray<f32> weights;
		weights.ResizeGrow(m_ActiveEnvironmentZones.GetCount());
		f32 totalWeight = 0.0f;
		f32 heighestWeight = 0.0f;
		atArray<f32> reverbWeights;
		reverbWeights.ResizeGrow(m_ActiveEnvironmentZones.GetCount());
		f32 totalReverbWeight = 0.0f;
		f32 heighestReverbWeight = 0.0f;
		for(int i=0; i<m_ActiveEnvironmentZones.GetCount(); ++i)
		{
			weights[i] = 0.0f;
			reverbWeights[i] = 0.0f;
			if(m_ActiveEnvironmentZones[i])
			{
				weights[i] = m_ActiveEnvironmentZones[i]->ComputeZoneInfluenceRatioAtPoint(m_ZoneActivationPosition);
				totalWeight += weights[i];
				heighestWeight = Max(heighestWeight, weights[i]);

				const EnvironmentRule* environmentRule = audNorthAudioEngine::GetObject<EnvironmentRule>(m_ActiveEnvironmentZones[i]->GetEnvironmentRule());
				if(environmentRule && AUD_GET_TRISTATE_VALUE(environmentRule->Flags, FLAG_ID_ENVIRONMENTRULE_OVERRIDEREVERB) == AUD_TRISTATE_TRUE)
				{
					reverbWeights[i] = weights[i];
					totalReverbWeight += weights[i];
					heighestReverbWeight = Max(heighestReverbWeight, weights[i]);
				}
			}
		}

		bool flagsSet = false;
		bool echoListSet = false;
		if(totalWeight != 0.0f)
		{
			for(int i=0; i<m_ActiveEnvironmentZones.GetCount(); ++i)
			{
				if(!m_ActiveEnvironmentZones[i])
					continue;
				f32 ratio = weights[i]/totalWeight;
				naAssertf(ratio>=0.0f && ratio<=1.0f, "Bad ratio when calculating average audio ambient environment rule");

				f32 reverbRatio = 0.0f;
				if(totalReverbWeight != 0.0f)
				{
					reverbRatio = reverbWeights[i] / totalReverbWeight;
				}
				naAssertf(reverbRatio>=0.0f && reverbRatio<=1.0f, "Bad reverb ratio when calculating average audio ambient environment rule");

				EnvironmentRule* environmentRule = audNorthAudioEngine::GetObject<EnvironmentRule>(m_ActiveEnvironmentZones[i]->GetEnvironmentRule());
				if(environmentRule)
				{
					m_AverageEnvironmentRule.ReverbSmall += environmentRule->ReverbSmall * reverbRatio;
					m_AverageEnvironmentRule.ReverbMedium += environmentRule->ReverbMedium * reverbRatio;
					m_AverageEnvironmentRule.ReverbLarge += environmentRule->ReverbLarge * reverbRatio;
					m_AverageEnvironmentRule.ReverbDamp += environmentRule->ReverbDamp * reverbRatio;

					m_AverageEnvironmentRule.EchoDelay += environmentRule->EchoDelay * ratio;
					m_AverageEnvironmentRule.EchoDelayVariance += environmentRule->EchoDelayVariance * ratio;
					m_AverageEnvironmentRule.EchoAttenuation += environmentRule->EchoAttenuation * ratio;
					m_AverageEnvironmentRule.BaseEchoVolumeModifier += environmentRule->BaseEchoVolumeModifier * ratio;
					m_AverageEnvironmentRule.EchoNumber = m_AverageEnvironmentRule.EchoNumber + 
						(u8)(environmentRule->EchoNumber * weights[i]);

					m_AverageEnvironmentRule.Flags = environmentRule->Flags;
					m_AverageEnvironmentRule.EchoSoundList = environmentRule->EchoSoundList;

					flagsSet = true;
					echoListSet = true;
				}
			}

			m_AverageEnvironmentRule.ReverbSmall = Clamp(m_AverageEnvironmentRule.ReverbSmall, 0.0f, 1.0f); 
			m_AverageEnvironmentRule.ReverbMedium = Clamp(m_AverageEnvironmentRule.ReverbMedium, 0.0f, 1.0f); 
			m_AverageEnvironmentRule.ReverbLarge = Clamp(m_AverageEnvironmentRule.ReverbLarge, 0.0f, 1.0f);
			m_AverageEnvironmentRule.ReverbDamp = Clamp(m_AverageEnvironmentRule.ReverbDamp, 0.0f, 1.0f); 
			m_AverageEnvironmentRule.EchoAttenuation = Clamp(m_AverageEnvironmentRule.EchoAttenuation, -100.0f, 0.0f);
			m_AverageEnvironmentRule.BaseEchoVolumeModifier = Clamp(m_AverageEnvironmentRule.BaseEchoVolumeModifier, -100.0f, 0.0f);


			//all this mixing of floats and u8s with division thrown in is leading to rounding down which stinks.  Probably better to just add an
			//		extra echo in case.  I know its ugly, but it sounds better.
			m_AverageEnvironmentRule.EchoNumber = (u8)Clamp((u8)(m_AverageEnvironmentRule.EchoNumber/totalWeight) + 1, 0, 5);
			m_ActiveEnvironmentZoneScale = heighestWeight;
			m_ActiveEnvironmentZoneReverbScale = heighestReverbWeight;
		}
	}
}

void audAmbientAudioEntity::ResetEnvironmentRule()
{
	m_ActiveEnvironmentZoneScale = 0.0f;
	m_ActiveEnvironmentZoneReverbScale = 0.0f;

	//Reset Data
	m_AverageEnvironmentRule.ReverbSmall = 0.0f;
	m_AverageEnvironmentRule.ReverbMedium = 0.0f;
	m_AverageEnvironmentRule.ReverbLarge = 0.0f;
	m_AverageEnvironmentRule.ReverbDamp = 0.0f;
	m_AverageEnvironmentRule.EchoDelay = 0.0f;
	m_AverageEnvironmentRule.EchoDelayVariance = 0.0f;
	m_AverageEnvironmentRule.EchoAttenuation = 0.0f;
	m_AverageEnvironmentRule.BaseEchoVolumeModifier = 0.0f;
	m_AverageEnvironmentRule.EchoNumber = 0;
	m_AverageEnvironmentRule.EchoSoundList = 0;
	m_AverageEnvironmentRule.Flags = 0xAAAAAAAA;
}

void audAmbientAudioEntity::UpdateShoreLineWaterHeights()
{
	for(u32 loop = 0; loop < m_ActiveShoreLines.GetCount(); loop++)
	{
		m_ActiveShoreLines[loop]->UpdateWaterHeight();
	}
	audShoreLineOcean::ComputeListenerOverOcean();
#if __BANK
	if(m_DebugShoreLine)
	{
		m_DebugShoreLine->UpdateWaterHeight(true);
	}
#endif
}
void audAmbientAudioEntity::OverrideUnderWaterStream(const char* streamName,bool override)
{
	OverrideUnderWaterStream(atStringHash(streamName),override);
}
void audAmbientAudioEntity::OverrideUnderWaterStream(u32 streamHashName,bool override)
{
	REPLAY_ONLY(if(!CReplayMgr::IsEditorActive()))
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if (naVerifyf(script, "Failed to set script audio flag %s; unable to get script %s", "UnderWaterStreamOverriden", CTheScripts::GetCurrentScriptName()))
		{
			script->SetAudioFlag("UnderWaterStreamOverriden", override);
		}
	}
#if GTA_REPLAY
	else 
	{
		m_ReplayOverrideUnderWaterStream = override;
	}
#endif
	sm_ScriptUnderWaterStream = streamHashName;
}
void audAmbientAudioEntity::SetVariableOnUnderWaterStream(const char* variableName, f32 variableValue)
{
	if(m_UnderWaterDeepSound)
	{
		m_UnderWaterDeepSound->SetVariableValueDownHierarchyFromName(variableName,variableValue);
	}
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketSetUnderWaterStreamVariable>(CPacketSetUnderWaterStreamVariable(atStringHash(variableName),variableValue));
	}
#endif
}
void audAmbientAudioEntity::SetVariableOnUnderWaterStream(u32 variableNameHash, f32 variableValue)
{
	if(m_UnderWaterDeepSound)
	{
		m_UnderWaterDeepSound->SetVariableValueDownHierarchyFromName(variableNameHash,variableValue);
	}
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketSetUnderWaterStreamVariable>(CPacketSetUnderWaterStreamVariable(variableNameHash,variableValue));
	}
#endif
}
void audAmbientAudioEntity::UpdateCameraUnderWater()
{
	audMetadataRef underWaterSoundRef = m_UnderWaterSoundSet.Find(ATSTRINGHASH("UNDER_WATER_DEEP_LOOP", 0x37B7D641));
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::UnderWaterStreamOverriden) REPLAY_ONLY(|| m_ReplayOverrideUnderWaterStream))
	{
		underWaterSoundRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(sm_ScriptUnderWaterStream);
	}
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketOverrideUnderWaterStream>(CPacketOverrideUnderWaterStream(sm_ScriptUnderWaterStream,g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::UnderWaterStreamOverriden)));
	}
#endif // GTA_REPLAY
#if __BANK
	if(sm_OverrideUnderWaterStream && strlen(s_OverriddenUnderWaterStream) != 0)
	{
		underWaterSoundRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(atStringHash(s_OverriddenUnderWaterStream));
	}
#endif

	const camBaseCamera* theActiveRenderedCamera = camInterface::GetDominantRenderedCamera();
	const bool isCreatorModeFlyCamActive = theActiveRenderedCamera && (camScriptedFlyCamera::GetStaticClassId() == theActiveRenderedCamera->GetClassId());

	f32 cameraDepth = Water::GetCameraWaterDepth();
	f32 depthApply = cameraDepth / sm_CameraDepthInWaterThreshold;
	depthApply = Clamp(depthApply,0.f,1.f);
	//f32 distanceApply = audShoreLineOcean::GetClosestDistanceToShore() / sm_CameraDistanceToShoreThreshold;
	//distanceApply = Clamp(distanceApply,0.f,1.f);
	//f32 apply = Max(depthApply,distanceApply);
	f32 apply = depthApply;
	CPed *player = CGameWorld::FindFollowPlayer();
	CVehicle * vehicle = CGameWorld::FindFollowPlayerVehicle(); 
	if((player && player->GetIsInWater()) || (vehicle && vehicle->GetIsInWater()) REPLAY_ONLY(|| (CReplayMgr::IsReplayInControlOfWorld() && Water::IsCameraUnderwater() && !audRadioStation::IsPlayingEndCredits())))
	{
		m_LastTimePlayerInWater = audNorthAudioEngine::GetCurrentTimeInMs();
		// grab a slot
		if(m_UnderwaterStreamSlot == NULL)
		{
			audStreamClientSettings settings;
			settings.priority = audStreamSlot::STREAM_PRIORITY_PLAYER_RADIO;
			settings.stopCallback = &OnUnderwaterStopCallback;
			settings.hasStoppedCallback = &HasUnderwaterStoppedCallback;
			settings.userData = 0;

			m_UnderwaterStreamSlot = audStreamSlot::AllocateSlot(&settings);
		}

		if(!m_UnderWaterDeepSound && m_UnderwaterStreamSlot && (m_UnderwaterStreamSlot->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE))
		{
			audSoundInitParams initParams;
			//initParams.SetVariableValue(ATSTRINGHASH("Depth", 0xBCA972D6),depthApply);
			Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
			CreateSound_PersistentReference(underWaterSoundRef, &m_UnderWaterDeepSound, &initParams);
			sm_PlayingUnderWaterSound = false;
		}

		if(m_UnderWaterDeepSound && !sm_PlayingUnderWaterSound )
		{
			if(m_UnderWaterDeepSound->Prepare(m_UnderwaterStreamSlot->GetWaveSlot(), true) == AUD_PREPARED)
			{
				if(!Water::IsCameraUnderwater())
				{
					m_UnderWaterDeepSound->SetRequestedVolume(g_SilenceVolume);
				}
				m_UnderWaterDeepSound->FindAndSetVariableValue(ATSTRINGHASH("Depth", 0xBCA972D6),depthApply);
				m_UnderWaterDeepSound->FindAndSetVariableValue(ATSTRINGHASH("CreakApply", 0x70193597),m_UnderwaterCreakFactor);
				m_UnderWaterDeepSound->Play();
				sm_PlayingUnderWaterSound = true;
			}
		}
	}
	// Dynamic mixer for under water 
	if(Water::IsCameraUnderwater() && !audRadioStation::IsPlayingEndCredits())
	{
		// Send message to the superconductor to stop the QUITE_SCENE
		ConductorMessageData message;
		message.conductorName = SuperConductor;
		message.message = StopQuietScene;
		message.bExtraInfo = false;
		message.vExtraInfo = Vector3((f32)audSuperConductor::sm_UnderWaterQSFadeOutTime
									,(f32)audSuperConductor::sm_UnderWaterQSDelayTime
									,(f32)audSuperConductor::sm_UnderWaterQSFadeInTime);
		SUPERCONDUCTOR.SendConductorMessage(message);
		if(!sm_WasCameraUnderWater)
		{
			sm_WasCameraUnderWater = true;

			if(!camInterface::IsFadedOut())
			{
				audSoundInitParams initParams;
				initParams.Position = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
				CreateAndPlaySound(m_UnderWaterSoundSet.Find(ATSTRINGHASH("GOING_UNDER_WATER", 0xDAC54D5D)),&initParams);
			}
		}
	
		// When we go under water trigger the audio scene, it will do the logic itself.
		if(vehicle && (vehicle->InheritsFromSubmarine() || vehicle->InheritsFromSubmarineCar()))
		{
			if(m_UnderWaterScene)
			{
				m_UnderWaterScene->Stop();
				m_UnderWaterScene = NULL;
			}
			if(!m_UnderWaterSubScene)
			{
				// Sub car? Start the sub car scene
				if(vehicle->InheritsFromSubmarineCar())
				{
					MixerScene *sceneSettings = g_AudioEngine.GetDynamicMixManager().GetObject<MixerScene>(ATSTRINGHASH("UNDER_WATER_SUB_CAR_SCENE", 0xBF470AEB));
					if(sceneSettings)
					{
						DYNAMICMIXER.StartScene(sceneSettings, &m_UnderWaterSubScene);
					}				
				}

				if (!m_UnderWaterSubScene)
				{
					if (vehicle->GetModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70))
					{
						MixerScene *sceneSettings = g_AudioEngine.GetDynamicMixManager().GetObject<MixerScene>(ATSTRINGHASH("UNDER_WATER_SUB_SCENE_KOSATKA", 0x8F57CE8D));
						if (sceneSettings)
						{
							DYNAMICMIXER.StartScene(sceneSettings, &m_UnderWaterSubScene);
						}
					}
				}

				// Not a sub-car (or couldn't start the sub-car scene for whatever reason)? Use the regular sub scene instead
				if(!m_UnderWaterSubScene)
				{
					MixerScene *sceneSettings = g_AudioEngine.GetDynamicMixManager().GetObject<MixerScene>(ATSTRINGHASH("UNDER_WATER_SUB_SCENE", 0x29B6879A));
					if(sceneSettings)
					{
						DYNAMICMIXER.StartScene(sceneSettings, &m_UnderWaterSubScene);
					}
				}				

				// Last resort - (possible if appropriate DLC packs aren't mounted) - just start the standard under water scene
				if(!m_UnderWaterSubScene)
				{
					DYNAMICMIXER.StartScene(ATSTRINGHASH("UNDER_WATER_SCENE", 0xD9C561EC),&m_UnderWaterSubScene);
				}
			}
		}
		else
		{
			if(m_UnderWaterSubScene)
			{
				m_UnderWaterSubScene->Stop();
				m_UnderWaterSubScene = NULL;

			}
			if(!m_UnderWaterScene)
			{
				if (!isCreatorModeFlyCamActive)
				{
					DYNAMICMIXER.StartScene(ATSTRINGHASH("UNDER_WATER_SCENE", 0xD9C561EC),&m_UnderWaterScene);
				}
			}
		}
		if(m_UnderWaterScene)
		{
			f32 scubaApply =  0.f; 
			f32 underwaterPlayerRadioApply = 1.f;
			if (player && player->GetPedAudioEntity() && player->GetPedAudioEntity()->GetFootStepAudio().IsWearingScubaSuit())
			{
				scubaApply = 1.f;
			}

			// Apply the player radio effect unless we're using the radio wheel
			if(audNorthAudioEngine::GetActiveSlowMoMode() == AUD_SLOWMO_RADIOWHEEL)
			{
				underwaterPlayerRadioApply = 0.f;
			}

			m_UnderWaterScene->SetVariableValue(ATSTRINGHASH("underWaterScubbaApply", 0x221D4029),scubaApply);
			m_UnderWaterScene->SetVariableValue(ATSTRINGHASH("underWaterPlayerRadioApply", 0xF9AFBCC8),underwaterPlayerRadioApply);
			m_UnderWaterScene->SetVariableValue(ATSTRINGHASH("underWaterDeepApply", 0xFB69B673),apply);
		}
		if(m_UnderWaterSubScene)
		{
			f32 scubaApply =  0.f; 
			f32 underwaterPlayerRadioApply = 1.f;
			if (player && player->GetPedAudioEntity() && player->GetPedAudioEntity()->GetFootStepAudio().IsWearingScubaSuit())
			{
				scubaApply = 1.f;
			}

			// Apply the player radio effect unless we're using the radio wheel
			if(audNorthAudioEngine::GetActiveSlowMoMode() == AUD_SLOWMO_RADIOWHEEL)
			{
				underwaterPlayerRadioApply = 0.f;
			}

			m_UnderWaterSubScene->SetVariableValue(ATSTRINGHASH("underWaterScubbaApply", 0x221D4029),scubaApply);
			m_UnderWaterSubScene->SetVariableValue(ATSTRINGHASH("underWaterPlayerRadioApply", 0xF9AFBCC8),underwaterPlayerRadioApply);
			m_UnderWaterSubScene->SetVariableValue(ATSTRINGHASH("underWaterDeepApply", 0xFB69B673),apply);
		}

		if (m_UnderWaterDeepSound)
		{
			m_UnderWaterDeepSound->SetRequestedVolume(0.f);
			m_UnderWaterDeepSound->FindAndSetVariableValue(ATSTRINGHASH("Depth", 0xBCA972D6),depthApply);
			m_UnderWaterDeepSound->FindAndSetVariableValue(ATSTRINGHASH("CreakApply", 0x70193597),m_UnderwaterCreakFactor);
		}
	}
	else if(!Water::IsCameraUnderwater() || audRadioStation::IsPlayingEndCredits())
	{
		if(sm_WasCameraUnderWater)
		{
			sm_WasCameraUnderWater = false;

			if(!camInterface::IsFadedOut())
			{
				audSoundInitParams initParams;
				initParams.Position = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
				CreateAndPlaySound(m_UnderWaterSoundSet.Find(ATSTRINGHASH("COMMING_OUT_OF_WATER", 0x43D23247)),&initParams);
			}
		}
		if(m_UnderWaterScene)
		{
			m_UnderWaterScene->Stop();
			m_UnderWaterScene = NULL;
		}
		if(m_UnderWaterSubScene)
		{
			m_UnderWaterSubScene->Stop();
			m_UnderWaterSubScene = NULL;
		}
		if(m_UnderWaterDeepSound)
		{
			m_UnderWaterDeepSound->SetRequestedVolume(g_SilenceVolume);
		}
		if(!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::UnderWaterStreamOverriden) && ((vehicle && !vehicle->GetIsInWater()) || (!vehicle && (!player || (player && !player->GetIsInWater())))))
		{
			if( m_LastTimePlayerInWater + 3000 < audNorthAudioEngine::GetCurrentTimeInMs())
			{
				StopUnderWaterSound();
			}
		}
	}
}

bool audAmbientAudioEntity::OnUnderwaterStopCallback(u32 UNUSED_PARAM(userData))
{
	//We are losing our stream slot, so stop the scripted stream.
	g_AmbientAudioEntity.StopUnderWaterSound();
	return true;
}

bool audAmbientAudioEntity::OnInteriorWallaStopCallback(u32 UNUSED_PARAM(userData))
{
	//We are losing our stream slot, so stop the scripted stream.
	g_AmbientAudioEntity.StopInteriorWallaSounds();
	return true;
}

bool audAmbientAudioEntity::OnExteriorWallaStopCallback(u32 UNUSED_PARAM(userData))
{
	//We are losing our stream slot, so stop the scripted stream.
	g_AmbientAudioEntity.StopExteriorWallaSounds();
	return true;
}

void audAmbientAudioEntity::StopUnderWaterSound()
{
	if(m_UnderWaterDeepSound)
	{
		m_UnderWaterDeepSound->StopAndForget();
	}
	sm_PlayingUnderWaterSound = false;
	if (m_UnderwaterStreamSlot)
	{
		m_UnderwaterStreamSlot->Free();
		m_UnderwaterStreamSlot = NULL;
	}
}

void audAmbientAudioEntity::StopInteriorWallaSounds()
{
	for(u32 loop = 0; loop < WALLA_TYPE_MAX; loop++)
	{
		StopAndForgetSounds(m_InteriorWallaSounds[loop]);
	}

	if(m_InteriorWallaStreamSlot)
	{
		m_InteriorWallaStreamSlot->Free();
		m_InteriorWallaStreamSlot = NULL;
	}
}

void audAmbientAudioEntity::StopExteriorWallaSounds()
{
	for(u32 loop = 0; loop < WALLA_TYPE_MAX; loop++)
	{
		StopAndForgetSounds(m_ExteriorWallaSounds[loop]);
	}

	if(m_ExteriorWallaStreamSlot)
	{
		m_ExteriorWallaStreamSlot->Free();
		m_ExteriorWallaStreamSlot = NULL;
	}
}

bool audAmbientAudioEntity::HasUnderwaterStoppedCallback(u32 UNUSED_PARAM(userData))
{
	return g_AmbientAudioEntity.HasStopUnderWaterSound();
}

bool audAmbientAudioEntity::HasInteriorWallaStoppedCallback(u32 UNUSED_PARAM(userData))
{
	return g_AmbientAudioEntity.HasStoppedInteriorWallaSound();
}

bool audAmbientAudioEntity::HasExteriorWallaStoppedCallback(u32 UNUSED_PARAM(userData))
{
	return g_AmbientAudioEntity.HasStoppedExteriorWallaSound();
}

bool audAmbientAudioEntity::HasStopUnderWaterSound()
{
	bool hasStopped = true;
	if(m_UnderwaterStreamSlot)
	{
		//Check if we are still loading or playing from our allocated wave slot
		audWaveSlot *waveSlot = m_UnderwaterStreamSlot->GetWaveSlot();
		if(waveSlot && (waveSlot->GetIsLoading() || (waveSlot->GetReferenceCount() > 0)))
		{
			hasStopped = false;
		}
	}
	return hasStopped;
}

bool audAmbientAudioEntity::HasStoppedInteriorWallaSound()
{
	bool hasStopped = true;
	if(m_InteriorWallaStreamSlot)
	{
		//Check if we are still loading or playing from our allocated wave slot
		audWaveSlot *waveSlot = m_InteriorWallaStreamSlot->GetWaveSlot();
		if(waveSlot && (waveSlot->GetIsLoading() || (waveSlot->GetReferenceCount() > 0)))
		{
			hasStopped = false;
		}
	}
	return hasStopped;
}

bool audAmbientAudioEntity::HasStoppedExteriorWallaSound()
{
	bool hasStopped = true;
	if(m_ExteriorWallaStreamSlot)
	{
		//Check if we are still loading or playing from our allocated wave slot
		audWaveSlot *waveSlot = m_ExteriorWallaStreamSlot->GetWaveSlot();
		if(waveSlot && (waveSlot->GetIsLoading() || (waveSlot->GetReferenceCount() > 0)))
		{
			hasStopped = false;
		}
	}
	return hasStopped;
}

void audAmbientAudioEntity::UpdateInteriorZones(const u32 timeInMs, const bool isRaining, const f32 loudSoundExclusionRadius, const u16 gameTimeMinutes)
{
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	CInteriorProxy* pInteriorProxy = CPortalVisTracker::GetPrimaryInteriorProxy();
	s32 currentRoomInst = CPortalVisTracker::GetPrimaryRoomIdx();

	if(m_InteriorProxyLastFrame != pInteriorProxy BANK_ONLY(&& !g_ZoneEditorActive))
	{
		audNorthAudioEngine::GetGtaEnvironment()->ForceAmbientMetricsUpdate();

		for(u32 loop = 0; loop < m_InteriorZones.GetCount(); loop++)
		{
			m_InteriorZones[loop].SetActive(false);
		}

		m_InteriorZones.clear();
		m_NumFramesInteriorActive = 0;
		m_InteriorRoomIndexLastFrame = -1;
	}
	else
	{
		for(u32 loop = 0; loop < m_InteriorZones.GetCount(); loop++)
		{
			m_InteriorZones[loop].Update(timeInMs, GetListenerPosition(&m_InteriorZones[loop]), isRaining, loudSoundExclusionRadius, gameTimeMinutes);
		}
	}

	if(pIntInst)
	{
		if(pInteriorProxy == m_InteriorProxyLastFrame)
		{
			if(m_NumFramesInteriorActive < CInteriorProxy::STATE_COOLDOWN)
			{
				m_NumFramesInteriorActive++;

				// Interior has just finished the cooldown phase - now good to query
				if(m_NumFramesInteriorActive >= CInteriorProxy::STATE_COOLDOWN)
				{		
					for(u32 i = 0; i < pIntInst->GetNumRooms(); i++)
					{
						const InteriorSettings* intSettings = NULL;
						const InteriorRoom* intRoom = NULL;
						audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForInterior(pIntInst, i, intSettings, intRoom);

						if(intSettings && intRoom)
						{
							AmbientZone* ambientZoneData = audNorthAudioEngine::GetObject<AmbientZone>(intRoom->AmbientZone);

							if(ambientZoneData)
							{
								spdAABB roomBoundingBox;
								pIntInst->GetRoomBoundingBox(i, roomBoundingBox);

								ambientZoneData->PositioningZone.Centre = VEC3V_TO_VECTOR3(roomBoundingBox.GetCenter());
								ambientZoneData->PositioningZone.Size = 2 * VEC3V_TO_VECTOR3(roomBoundingBox.GetExtent());

								ambientZoneData->ActivationZone.Centre = VEC3V_TO_VECTOR3(roomBoundingBox.GetCenter());
								ambientZoneData->ActivationZone.Size = 2 * VEC3V_TO_VECTOR3(roomBoundingBox.GetExtent());

								audAmbientZone ambientZone;
								ambientZone.Init(ambientZoneData, intRoom->AmbientZone.Get(), true, pIntInst->GetTransform().GetHeading() * RtoD);
								ambientZone.CalculateDynamicBankUsage();
								ambientZone.SetInteriorInformation(pInteriorProxy, i);
								m_InteriorZones.PushAndGrow(ambientZone);
							}
						}
					}
				}
			}

			// If we enter a building or change rooms, reset the wave slot pointers on all the active exterior zones. In Update(), we calculate the
			// dynamic bank usage for the interior zones first, meaning that doing this allows the interior zone rules to have first say over
			// which wave slots they use.
			if(m_NumFramesInteriorActive >= CInteriorProxy::STATE_COOLDOWN)
			{
				if(currentRoomInst != m_InteriorRoomIndexLastFrame)
				{
					audNorthAudioEngine::GetGtaEnvironment()->ForceAmbientMetricsUpdate();
					
					for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
					{
						m_ActiveZones[loop]->FreeWaveSlots();
					}
				}

				m_InteriorRoomIndexLastFrame = currentRoomInst;
				UpdateInteriorWalla();
			}
		}
	}

	m_InteriorProxyLastFrame = pInteriorProxy;
}

void audAmbientAudioEntity::PostUpdate()
{
	UpdateCameraUnderWater();
	UpdateHighwayAmbience(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
	const u32 readIndex = m_RequestedAudioEffectsWriteIndex;

	// clear out all entity refs
	for(u32 i = 0; i < m_NumRequestedAudioEffects[readIndex]; i++)
	{
		if(m_RequestedAudioEffects[readIndex][i].entity)
		{
			m_RequestedAudioEffects[readIndex][i].entity = NULL;
		}
	}
}

bool audAmbientAudioEntity::IsZoneActive(u32 zoneNameHash) const
{
	for (u32 i = 0; i < GetNumActiveZones(); i++)
	{
		const audAmbientZone* ambientZone = GetActiveZone(i);

		if (ambientZone && ambientZone->GetNameHash() == zoneNameHash)
		{
			return ambientZone->IsActive();
		}
	}

	return false;
}

#if GTA_REPLAY
void audAmbientAudioEntity::AddReplayActiveAlarm(audReplayAlarmState alarmState)
{ 
	naAssertf(audNorthAudioEngine::GetObject<AlarmSettings>(alarmState.alarmNameHash), "AddReplayActiveAlarm - Could not find alarm with name %u", alarmState.alarmNameHash);
	m_ReplayActiveAlarms.PushAndGrow(alarmState); 
}

void audAmbientAudioEntity::UpdateReplayAlarmStates()
{
	if(!CReplayMgr::IsEditModeActive())
	{
		m_ReplayActiveAlarms.clear();

		for(u32 loop = 0; loop < m_ActiveAlarms.GetMaxCount(); loop++)
		{
			if(m_ActiveAlarms[loop].alarmSettings)
			{
				audReplayAlarmState alarmState;
				alarmState.alarmNameHash = m_ActiveAlarms[loop].alarmName;
				alarmState.timeAlive = (fwTimer::GetTimeInMilliseconds() - m_ActiveAlarms[loop].timeStarted);
				m_ReplayActiveAlarms.PushAndGrow(alarmState);
			}
		}
	}
}

void audAmbientAudioEntity::UpdateAmbientZoneReplayStates()
{
	m_ReplayAmbientZoneStates.clear();

	// First add any non persistent state changes
	for(u32 nonPersistentStateIndex = 0; nonPersistentStateIndex < m_ZonesWithNonPersistentStateFlags.GetCount(); nonPersistentStateIndex++)
	{
		audAmbientZoneReplayState zoneReplayState;
		zoneReplayState.zoneNameHash = m_ZonesWithNonPersistentStateFlags[nonPersistentStateIndex].zoneNameHash;
		zoneReplayState.nonPersistantState = (u8)(m_ZonesWithNonPersistentStateFlags[nonPersistentStateIndex].enabled? AUD_TRISTATE_TRUE : AUD_TRISTATE_FALSE);
		zoneReplayState.persistentFlagChanged = false;
		m_ReplayAmbientZoneStates.PushAndGrow(zoneReplayState);
	}

	// Now add any persistent changes
	for(u32 persistentStateIndex = 0; persistentStateIndex < m_ZonesWithModifiedEnabledFlags.GetCount(); persistentStateIndex++)
	{
		bool alreadyExists = false;

		// If we've already added this zone (due to a non-persistent state change), we just need to set the persistent state flag
		for(u32 replayZoneStateIndex = 0; replayZoneStateIndex < m_ReplayAmbientZoneStates.GetCount(); replayZoneStateIndex++)
		{
			if(m_ReplayAmbientZoneStates[replayZoneStateIndex].zoneNameHash == m_ZonesWithModifiedEnabledFlags[persistentStateIndex])
			{
				m_ReplayAmbientZoneStates[replayZoneStateIndex].persistentFlagChanged = true;
				alreadyExists = true;
				break;
			}
		}

		// If we didn't already have a record of this zone, add a new one with the non-persistent state set to 'unspecified'
		if(!alreadyExists)
		{
			audAmbientZoneReplayState zoneReplayState;
			zoneReplayState.zoneNameHash = m_ZonesWithModifiedEnabledFlags[persistentStateIndex];
			zoneReplayState.persistentFlagChanged = true;
			zoneReplayState.nonPersistantState = (u8)AUD_TRISTATE_UNSPECIFIED;
			m_ReplayAmbientZoneStates.PushAndGrow(zoneReplayState);
		}		
	}
}

void audAmbientAudioEntity::AddAmbientZoneReplayState(audAmbientZoneReplayState zoneState)
{
#if __ASSERT
	AmbientZone *zone = audNorthAudioEngine::GetObject<AmbientZone>(zoneState.zoneNameHash);
	audAssertf(zone != NULL, "Failed to find replay ambient audio zone zone %u", zoneState.zoneNameHash);
#endif

	m_ReplayAmbientZoneStates.PushAndGrow(zoneState); 
}

bool audAmbientAudioEntity::IsZoneEnabledInReplay(u32 zoneNameHash, TristateValue defaultEnabledState)
{
	for(u32 zoneIndex = 0; zoneIndex < m_ReplayAmbientZoneStates.GetCount(); zoneIndex++)
	{
		if(m_ReplayAmbientZoneStates[zoneIndex].zoneNameHash == zoneNameHash)
		{
			// Non persistent state takes priority
			if(m_ReplayAmbientZoneStates[zoneIndex].nonPersistantState != AUD_TRISTATE_UNSPECIFIED)
			{
				return m_ReplayAmbientZoneStates[zoneIndex].nonPersistantState == AUD_TRISTATE_TRUE? true : false;
			}
			// Otherwise if we've flipped the default state, return the opposite
			else if(m_ReplayAmbientZoneStates[zoneIndex].persistentFlagChanged)
			{
				return defaultEnabledState == AUD_TRISTATE_TRUE? false : true;
			}
		}
	}

	return defaultEnabledState == AUD_TRISTATE_TRUE? true : false;
}
#endif

const audAmbientZone* audAmbientAudioEntity::GetZone(u32 index)			
{ 
	return m_Zones[index]; 
}

const audAmbientZone* audAmbientAudioEntity::GetInteriorZone(u32 index)			
{ 
	return &m_InteriorZones[index]; 
}

void audAmbientAudioEntity::UpdateHighwayAmbience(const u32 UNUSED_PARAM(timeInMs))
{
	PF_FUNC(UpdateHighwayAmbience);

	if(!g_EnableHighwayAmbience)
	{
		for(u32 loop = 0; loop < kMaxPassbySounds; loop++)
		{
			StopAndForgetSounds(m_HighwayPassbySounds[loop].sound, m_HighwayPassbySounds[loop].soundWet);
		}

		return;
	}

	const Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
	Vector3 calculationPoint = listenerMatrix.d;

	// Raise the calculation height a bit to prevent us detecting nodes on the road where the player is actually standing
	calculationPoint.z += g_HighwayRoadNodeSearchUpOffset;

	m_GlobalHighwayVolumeSmoother.CalculateValue(m_RoadNodesToHighwayVol.CalculateValue((f32)m_NumHighwayNodesInRadius), fwTimer::GetTimeInMilliseconds());

	// Don't bother searching if we know there are no highways in the vicinity
	if(audNorthAudioEngine::GetGtaEnvironment()->GetHighwayFactorForDirection(AUD_AMB_DIR_LOCAL) > 0.0f)
	{
		bool vehicleSpawned = false;

		if(fwTimer::GetTimeInMilliseconds() - m_LastHighwayPassbyTime > (m_HighwayPassbyTimeFrequency * 1000.0f))
		{
			if(calculationPoint.Dist2(m_LastHighwayNodeCalculationPoint) > g_HighwayRoadNodeRecalculationDist * g_HighwayRoadNodeRecalculationDist)
			{
				m_NumHighwayNodesFound = ThePaths.RecordNodesInCircleFacingCentre(calculationPoint, g_HighwayRoadNodeSearchMinDist, g_HighwayRoadNodeSearchMaxDist, kMaxHighwayNodes, m_HighwaySearchNodes, false, true, false, true, g_HighwayOnlySearchUp, m_NumHighwayNodesInRadius);
				m_LastHighwayNodeCalculationPoint = calculationPoint;
			}

			if(m_NumHighwayNodesFound > 0)
			{
				CNodeAddress startNodeAddress = m_HighwaySearchNodes[audEngineUtil::GetRandomNumberInRange(0, m_NumHighwayNodesFound - 1)];
				CPathFind::FindNodeClosestToNodeFavorDirectionOutput findNodeOutput;
				
				if (!startNodeAddress.IsEmpty())
				{
					const CPathNode* startNode = ThePaths.FindNodePointerSafe(startNodeAddress);
					const CPathNode* endNode = NULL;

					if(startNode)
					{
						Vector3 vehicleOrientation = YAXIS;
						vehicleOrientation.RotateZ(DtoR * ThePaths.FindNodeOrientationForCarPlacement(startNodeAddress));

						Vector2 orientation2D;
						vehicleOrientation.GetVector2XY(orientation2D);
						CPathFind::FindNodeClosestToNodeFavorDirectionInput findNodeInput = CPathFind::FindNodeClosestToNodeFavorDirectionInput(startNodeAddress, orientation2D);
						findNodeInput.m_bIncludeSwitchedOffNodes = true;
						findNodeInput.m_bCanFollowIncomingLinks = false;
						findNodeInput.m_bCanFollowOutgoingLinks = true;
						ThePaths.FindNodeClosestToNodeFavorDirection(findNodeInput, findNodeOutput);

						if(!findNodeOutput.m_Node.IsEmpty())
						{
							endNode = ThePaths.FindNodePointerSafe(findNodeOutput.m_Node);
						}
					}

					if(startNode && endNode && startNode != endNode)
					{
						Vector3 startPos, endPos;
						startNode->GetCoors(startPos);
						endNode->GetCoors(endPos);

						Vector3 velocity;
						velocity.NormalizeFast(endPos - startPos);

						for(u32 loop = 0; loop < kMaxPassbySounds; loop++)
						{
							if(m_HighwayPassbySounds[loop].sound == NULL && m_HighwayPassbySounds[loop].soundWet == NULL)
							{
								CLoadedModelGroup & loadedCars = gPopStreaming.GetAppropriateLoadedCars();
								strLocalIndex modelIndex = strLocalIndex(loadedCars.PickRandomCarModel(false));

								if(CModelInfo::IsValidModelInfo(modelIndex.Get()))
								{
									CVehicleModelInfo* modelInfo = ((CVehicleModelInfo* )CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex))));

									if(modelInfo)
									{
										CarAudioSettings *settings = audNorthAudioEngine::GetObject<CarAudioSettings>(modelInfo->GetModelNameHash());

										if(settings)
										{
											m_HighwayPassbySounds[loop].vehicleModel = modelInfo;
											m_HighwayPassbySounds[loop].carAudioSettings = settings;
											m_HighwayPassbySounds[loop].distanceSinceLastFrontTyreBump = (m_HighwayPassbySounds[loop].vehicleModel->GetBoundingBoxMax().y - m_HighwayPassbySounds[loop].vehicleModel->GetBoundingBoxMin().y) * g_HighwayPassbyTyreBumpFrontBackOffsetScale;
											m_HighwayPassbySounds[loop].distanceSinceLastRearTyreBump = 0;
											m_HighwayPassbySounds[loop].lifeTime = 0;

											// The previous road is likely to be the same as the current road, so do a quick check to see if we can just use the old settings
											if(startNode->m_streetNameHash == m_PrevRoadInfoHash && m_PrevRoadInfo != NULL)
											{
												m_HighwayPassbySounds[loop].tyreBumpDistance = m_PrevRoadInfo->TyreBumpDistance;
											}
											else
											{
												AudioRoadInfo* roadInfo = audNorthAudioEngine::GetObject<AudioRoadInfo>(startNode->m_streetNameHash);

												if(!roadInfo)
												{
													roadInfo = m_DefaultRoadInfo;
												}

												if(roadInfo)
												{
													m_HighwayPassbySounds[loop].tyreBumpDistance = roadInfo->TyreBumpDistance;
													m_PrevRoadInfoHash = startNode->m_streetNameHash;
													m_PrevRoadInfo = roadInfo;
												}
												else
												{
													// Just some vaguely sensible value
													m_HighwayPassbySounds[loop].tyreBumpDistance = 12.0f;
												}
											}

											m_HighwayPassbySounds[loop].velocity = velocity;

											// Start the sound a bit further back along the path
											m_HighwayPassbySounds[loop].position = startPos;
											m_HighwayPassbySounds[loop].startNodeAddress = startNodeAddress;
											m_HighwayPassbySounds[loop].startPos = startPos;
											m_HighwayPassbySounds[loop].endNodeAddress = findNodeOutput.m_Node;											
											m_HighwayPassbySounds[loop].endPos = endPos;
											m_HighwayPassbySounds[loop].pathNodesValid = true;
											m_HighwayPassbySounds[loop].isSlipLane = startNode->IsSlipLane();
											m_HighwayPassbySounds[loop].reverbSmoother.Reset();
											m_HighwayPassbySounds[loop].tyreBumpsEnabled = true;

											for(u32 otherSound = 0; otherSound < kMaxPassbySounds; otherSound++)
											{
												if(otherSound != loop && m_HighwayPassbySounds[otherSound].tyreBumpsEnabled && m_HighwayPassbySounds[otherSound].sound)
												{
													if(m_HighwayPassbySounds[otherSound].position.Dist2(startPos) < 60.0f * 60.0f)
													{
														m_HighwayPassbySounds[loop].tyreBumpsEnabled = false;
														break;
													}
												}
											}
											
											vehicleSpawned = true;

											CreateAndPlaySound_Persistent(ATSTRINGHASH("HIGHWAY_PASSBY_CONCRETE_ROLL_SIDE", 0x5D4BE759), &(m_HighwayPassbySounds[loop].sound));
											CreateAndPlaySound_Persistent(ATSTRINGHASH("HIGHWAY_PASSBY_CONCRETE_ROLL", 0xA7052E50), &(m_HighwayPassbySounds[loop].soundWet));

											if(m_HighwayPassbySounds[loop].sound && m_HighwayPassbySounds[loop].soundWet)
											{
												m_HighwayPassbySounds[loop].sound->SetRequestedEnvironmentalLoudnessFloat(1.0f);
												m_HighwayPassbySounds[loop].soundWet->SetRequestedEnvironmentalLoudnessFloat(1.0f);

												// Recalculate a random frequency
												const f32 hours = static_cast<f32>(CClock::GetHour()) + (CClock::GetMinute()/60.f);

												// Scale back the frequency a bit if we've only found a few nodes (to differentiate small bridge vs. massive spaghetti junction)
												f32 nodeDensityScalar = 1.0f/Clamp(m_NumHighwayNodesFound/20.0f, 0.25f, 1.0f);
												m_HighwayPassbyTimeFrequency = m_TimeOfDayToPassbyFrequency.CalculateValue(hours) * nodeDensityScalar * audEngineUtil::GetRandomNumberInRange(0.5f, 3.0f);
												break;
											}
										}
									}
								}
							}
						}
					}
				}
			}

			if(vehicleSpawned)
			{
				m_LastHighwayPassbyTime = fwTimer::GetTimeInMilliseconds();
			}
			else
			{
				m_LastHighwayPassbyTime += 500u;
			}
		}
	}

	for(u32 loop = 0; loop < kMaxPassbySounds; loop++)
	{
		if(m_HighwayPassbySounds[loop].sound && m_HighwayPassbySounds[loop].soundWet)
		{
			// Sound has traveled to the next node, we need to calculate another
			if(m_HighwayPassbySounds[loop].pathNodesValid && m_HighwayPassbySounds[loop].position.Dist2(m_HighwayPassbySounds[loop].startPos) > m_HighwayPassbySounds[loop].endPos.Dist2(m_HighwayPassbySounds[loop].startPos))
			{
				Vector2 direction2D;
				m_HighwayPassbySounds[loop].velocity.GetVector2XY(direction2D);
				CPathFind::FindNodeClosestToNodeFavorDirectionOutput findNodeOutPut;
				CPathFind::FindNodeClosestToNodeFavorDirectionInput findNodeInput = CPathFind::FindNodeClosestToNodeFavorDirectionInput(m_HighwayPassbySounds[loop].endNodeAddress, direction2D);
				findNodeInput.m_bIncludeSwitchedOffNodes = true;
				findNodeInput.m_bCanFollowIncomingLinks = false;
				findNodeInput.m_bCanFollowOutgoingLinks = true;
				ThePaths.FindNodeClosestToNodeFavorDirection(findNodeInput, findNodeOutPut);

				if(!findNodeOutPut.m_Node.IsEmpty())
				{
					m_HighwayPassbySounds[loop].startPos = m_HighwayPassbySounds[loop].endPos;
					m_HighwayPassbySounds[loop].startNodeAddress = m_HighwayPassbySounds[loop].endNodeAddress;
					const CPathNode* startNode = ThePaths.FindNodePointerSafe(m_HighwayPassbySounds[loop].startNodeAddress);

					if(startNode)
					{
						m_HighwayPassbySounds[loop].isSlipLane = startNode->IsSlipLane();
					}
					else
					{
						m_HighwayPassbySounds[loop].isSlipLane = false;
					}

					m_HighwayPassbySounds[loop].endNodeAddress = findNodeOutPut.m_Node;
					const CPathNode* endNode = ThePaths.FindNodePointerSafe(m_HighwayPassbySounds[loop].endNodeAddress);

					if(endNode)
					{
						endNode->GetCoors(m_HighwayPassbySounds[loop].endPos);
						m_HighwayPassbySounds[loop].velocity = m_HighwayPassbySounds[loop].endPos - m_HighwayPassbySounds[loop].position;
						m_HighwayPassbySounds[loop].velocity.Normalize();
					}
					else
					{
						m_HighwayPassbySounds[loop].pathNodesValid = false;
					}
				}
				else
				{
					// Setting the path node to invalid will disable finding any additional nodes, so the sound will just travel off
					// in the distance that it is currently traveling, and stop as normal when it gets far enough away
					m_HighwayPassbySounds[loop].pathNodesValid = false;
				}
			}

			Vector3 distanceTravelled = m_HighwayPassbySounds[loop].velocity * fwTimer::GetTimeStep() * g_HighwayPassbySpeed;
			f32 distanceTravelledMag = distanceTravelled.Mag();

			Vector3 previousPosition = m_HighwayPassbySounds[loop].position;
			m_HighwayPassbySounds[loop].position = m_HighwayPassbySounds[loop].position + distanceTravelled;
			m_HighwayPassbySounds[loop].distanceSinceLastFrontTyreBump += distanceTravelledMag;
			m_HighwayPassbySounds[loop].distanceSinceLastRearTyreBump += distanceTravelledMag;
			m_HighwayPassbySounds[loop].sound->SetRequestedPosition(m_HighwayPassbySounds[loop].position);
			m_HighwayPassbySounds[loop].soundWet->SetRequestedPosition(m_HighwayPassbySounds[loop].position);
			m_HighwayPassbySounds[loop].lifeTime += fwTimer::GetTimeStep();

			Matrix34 matrix; 
			matrix.Identity();
			Vector3 closestPoint;
			fwGeom::fwLine positioningLine;
			Vector3 lineDirection = m_HighwayPassbySounds[loop].endPos - m_HighwayPassbySounds[loop].startPos;
			positioningLine.m_start = m_HighwayPassbySounds[loop].startPos - (1000.0f * lineDirection);
			positioningLine.m_end = m_HighwayPassbySounds[loop].startPos + (1000.0f * lineDirection);
			positioningLine.FindClosestPointOnLine(listenerMatrix.d, closestPoint);

			f32 highwayWidth = m_HighwayPassbySounds[loop].isSlipLane? 2.0f : 4.0f;
			Vector3 closestPointToListener = listenerMatrix.d - closestPoint;
			closestPointToListener.SetZ(0.0f);
			f32 distanceFromLine = closestPointToListener.Mag();
			closestPointToListener.NormalizeFast(closestPointToListener);

			matrix.Translate(closestPoint + closestPointToListener * Min(highwayWidth, distanceFromLine));
			f32 reverbAmount = 1.0f + m_HighwayPassbyVolumeCone.ComputeAttenuation(matrix);
			const f32 PI_over_2 = (PI / 2.0f);

			f32 verticalDistVolumeScale = 1.0f;
			f32 verticalDist = m_HighwayPassbySounds[loop].position.z - calculationPoint.z;

			if(verticalDist < g_HighwayRoadNodeSearchUpOffset)
			{
				verticalDistVolumeScale = Clamp(verticalDist/g_HighwayRoadNodeSearchUpOffset, 0.0f, 1.0f);
			}

			f32 volumeDry = Sinf(1.0f - reverbAmount * PI_over_2) * verticalDistVolumeScale;
			f32 volumeWet = Sinf(reverbAmount * PI_over_2) * verticalDistVolumeScale;		

			m_HighwayPassbySounds[loop].sound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(volumeDry * m_GlobalHighwayVolumeSmoother.GetLastValue()));
			m_HighwayPassbySounds[loop].soundWet->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(volumeWet * m_GlobalHighwayVolumeSmoother.GetLastValue()));
			m_HighwayPassbySounds[loop].reverbAmount = m_HighwayPassbySounds[loop].reverbSmoother.CalculateValue(reverbAmount); 

			f32 distOnLine = m_HighwayPassbySounds[loop].position.Dist2(closestPoint);
			f32 prevDistFromListenerSq = previousPosition.Dist2(listenerMatrix.d);
			f32 distFromListenerSq = m_HighwayPassbySounds[loop].position.Dist2(listenerMatrix.d);

			// If we're moving away and get outside the max range, or if we get below the min height, kill the sounds
			if(((prevDistFromListenerSq < distFromListenerSq) && (m_HighwayPassbySounds[loop].position.Dist2(listenerMatrix.d) > g_HighwayRoadNodeCullDist * g_HighwayRoadNodeCullDist)) ||
				m_HighwayPassbySounds[loop].position.GetZ() < calculationPoint.GetZ())
			{
				StopAndForgetSounds(m_HighwayPassbySounds[loop].sound, m_HighwayPassbySounds[loop].soundWet);
				continue;
			}

			m_HighwayPassbySounds[loop].distLastFrame = distOnLine;

			bool playTyreBump = false;
			u32 tyreBumpHash = 0;
			u32 tyreBumpHashWet = 0;

			if(m_HighwayPassbySounds[loop].tyreBumpsEnabled)
			{
				if(m_HighwayPassbySounds[loop].distanceSinceLastFrontTyreBump > m_HighwayPassbySounds[loop].tyreBumpDistance)
				{
					playTyreBump = true;
					m_HighwayPassbySounds[loop].distanceSinceLastFrontTyreBump -= m_HighwayPassbySounds[loop].tyreBumpDistance;
					tyreBumpHash = m_HighwayPassbySounds[loop].carAudioSettings->FreewayPassbyTyreBumpFrontSide;
					tyreBumpHashWet = m_HighwayPassbySounds[loop].carAudioSettings->FreewayPassbyTyreBumpFront;
				}

				if(m_HighwayPassbySounds[loop].distanceSinceLastRearTyreBump > m_HighwayPassbySounds[loop].tyreBumpDistance)
				{
					playTyreBump = true;
					m_HighwayPassbySounds[loop].distanceSinceLastRearTyreBump -= m_HighwayPassbySounds[loop].tyreBumpDistance;
					tyreBumpHash = m_HighwayPassbySounds[loop].carAudioSettings->FreewayPassbyTyreBumpBackSide;
					tyreBumpHashWet = m_HighwayPassbySounds[loop].carAudioSettings->FreewayPassbyTyreBumpBack;
				}
			}
				
			if(playTyreBump && g_PlayHighwayTyreBumps)
			{
				audSound* tyreBumpSound = NULL;
				audSound* tyreBumpSoundWet = NULL;

				audSoundInitParams initParams;
				initParams.Position = m_HighwayPassbySounds[loop].position;
				initParams.Predelay = audEngineUtil::GetRandomNumberInRange(0, 100);
				initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeWet * m_GlobalHighwayVolumeSmoother.GetLastValue());
				CreateSound_LocalReference(tyreBumpHashWet, &tyreBumpSoundWet, &initParams);

				initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(volumeDry * m_GlobalHighwayVolumeSmoother.GetLastValue());
				CreateSound_LocalReference(tyreBumpHash, &tyreBumpSound, &initParams);

				if(tyreBumpSound)
				{
					tyreBumpSound->SetRequestedEnvironmentalLoudnessFloat(1.0f);
					tyreBumpSound->PrepareAndPlay();
				}

				if(tyreBumpSoundWet)
				{
					tyreBumpSoundWet->SetRequestedEnvironmentalLoudnessFloat(1.0f);
					tyreBumpSoundWet->PrepareAndPlay();
				}

#if __BANK
				m_HighwayPassbySounds[loop].lastBumpPos = m_HighwayPassbySounds[loop].position;
#endif			
			}
		}
	}
}

// ----------------------------------------------------------------
// Draw any ped walla debug info
// ----------------------------------------------------------------
void audAmbientAudioEntity::DebugDrawWallaExterior()
{
#if __BANK
	static const char *meterNamesDirection[] = {"North", "East", "South", "West", "Final"};
	static audMeterList meterListPanic;
	static audMeterList meterListDensity;
	static audMeterList meterListBlocked;
	static f32 meterValuesDensity[5];
	static f32 pedCount[4];
	static f32 fractionPanicing[4];
	static f32 blockedFactor[4];

	for (s32 i=0; i<4; i++)
	{
		pedCount[i] = audNorthAudioEngine::GetGtaEnvironment()->GetExteriorPedCountForDirection((audAmbienceDirection)i);

		if(pedCount[i] > 0)
		{
			fractionPanicing[i] = audNorthAudioEngine::GetGtaEnvironment()->GetExteriorPedPanicCountForDirection((audAmbienceDirection)i)/pedCount[i];
		}
		else
		{
			fractionPanicing[i] = 0.0f;
		}

		blockedFactor[i] = audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection((audAmbienceDirection)i);
		
		f32 density = 0.0f;
		
		if(g_DrawClampedAndScaledWallaValues)
		{
			f32 blockedFactor = g_BlockedToPedDensityScale.CalculateValue(audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection((audAmbienceDirection)i));
			density = Clamp(GetExteriorPedDensityForDirection((audAmbienceDirection)i) * blockedFactor * m_PedDensityScalarExterior, m_MinValidPedDensityExterior, m_MaxValidPedDensityExterior);
		}
		else
		{
			density = GetExteriorPedDensityForDirection((audAmbienceDirection)i);
		}
		
		meterValuesDensity[i] = density;
	}

	meterValuesDensity[4] = m_FinalPedDensityExterior;

	meterListDensity.left = 790.f;
	meterListDensity.bottom = 620.f;
	meterListDensity.width = 400.f;
	meterListDensity.height = 200.f;
	meterListDensity.title = "Exterior Ped Density"; 
	meterListDensity.values = &meterValuesDensity[0];
	meterListDensity.names = meterNamesDirection;
	meterListDensity.numValues = sizeof(meterNamesDirection)/sizeof(meterNamesDirection[0]);
	audNorthAudioEngine::DrawLevelMeters(&meterListDensity);

	meterListPanic.left = 790.f;
	meterListPanic.bottom = 320.f;
	meterListPanic.width = 400.f;
	meterListPanic.height = 200.f;
	meterListPanic.title = "Peds Panicing"; 
	meterListPanic.values = &fractionPanicing[0];
	meterListPanic.names = meterNamesDirection;
	meterListPanic.numValues = sizeof(meterNamesDirection)/sizeof(meterNamesDirection[0]) - 1;
	audNorthAudioEngine::DrawLevelMeters(&meterListPanic);

	meterListBlocked.left = 190.f;
	meterListBlocked.bottom = 620.f;
	meterListBlocked.width = 400.f;
	meterListBlocked.height = 200.f;
	meterListBlocked.title = "Blocked Factor"; 
	meterListBlocked.values = &blockedFactor[0];
	meterListBlocked.names = meterNamesDirection;
	meterListBlocked.numValues = sizeof(meterNamesDirection)/sizeof(meterNamesDirection[0]) - 1;
	audNorthAudioEngine::DrawLevelMeters(&meterListBlocked);

	char tempString[128];
	if(m_ScriptDesiredPedDensityExteriorApplyFactor > 0.0f)
	{
		formatf(tempString, "Script Adjusting Exterior Density! Requested %.02f, Apply Factor %.02f", m_ScriptDesiredPedDensityExterior, m_ScriptDesiredPedDensityExteriorApplyFactor);
		grcDebugDraw::Text(Vector2(0.3f, 0.1f), Color32(255,0,0), tempString);
	}

	formatf(tempString, "Panic Cooldown: %.02f", rage::Clamp(g_PanicCooldownTime - m_PedPanicTimer, 0.0f, g_PanicCooldownTime));
	grcDebugDraw::Text(Vector2(0.48f, 0.3f), Color32(255,255,255), tempString);

	formatf(tempString, "Ped/Listener Height Diff: %.02f", CalculatePedsToListenerHeightDiff());
	grcDebugDraw::Text(Vector2(0.18f, 0.3f), Color32(255,255,255), tempString);

	Vector3 listPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
	formatf(tempString, "Exterior Occlusion: %.02f", audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionFor3DPosition(listPos));
	grcDebugDraw::Text(Vector2(0.18f, 0.32f), Color32(255,255,255), tempString);

	formatf(tempString, "Exterior Occlusion (inc portals): %.02f", audNorthAudioEngine::GetOcclusionManager()->GetOutsideWorldOcclusionAfterPortals());
	grcDebugDraw::Text(Vector2(0.18f, 0.34f), Color32(255,255,255), tempString);

	formatf(tempString, "Ped Counts:");
	grcDebugDraw::Text(Vector2(0.48f, 0.7f), Color32(255,255,255), tempString);

	formatf(tempString, "North: %d", (u32)pedCount[0]);
	grcDebugDraw::Text(Vector2(0.50f, 0.72f), Color32(255,255,255), tempString);

	formatf(tempString, "East: %d", (u32)pedCount[1]);
	grcDebugDraw::Text(Vector2(0.50f, 0.74f), Color32(255,255,255), tempString);

	formatf(tempString, "South: %d", (u32)pedCount[2]);
	grcDebugDraw::Text(Vector2(0.50f, 0.76f), Color32(255,255,255), tempString);

	formatf(tempString, "West: %d", (u32)pedCount[3]);
	grcDebugDraw::Text(Vector2(0.50f, 0.78f), Color32(255,255,255), tempString);

	formatf(tempString, "Max Density: %.02f", m_MaxValidPedDensityExterior);
	grcDebugDraw::Text(Vector2(0.48f, 0.80f), Color32(255,255,255), tempString);

	formatf(tempString, "Min Density: %.02f", m_MinValidPedDensityExterior);
	grcDebugDraw::Text(Vector2(0.48f, 0.82f), Color32(255,255,255), tempString);

	formatf(tempString, "Density Scalar: %.02f", m_PedDensityScalarExterior);
	grcDebugDraw::Text(Vector2(0.48f, 0.84f), Color32(255,255,255), tempString);

	if(g_SmallestPedDensityZone)
	{
		formatf(tempString, "Primary ped walla zone: %s", g_SmallestPedDensityZone->GetName());
		grcDebugDraw::Text(Vector2(0.48f, 0.90f), Color32(255,255,255), tempString);

		if(g_SecondSmallestPedDensityZone)
		{
			formatf(tempString, "Secondary ped walla zone: %s", g_SecondSmallestPedDensityZone->GetName());
			grcDebugDraw::Text(Vector2(0.48f, 0.92f), Color32(255,255,255), tempString);
		}
	}
#endif
}

// ----------------------------------------------------------------
// Draw any ped walla debug info
// ----------------------------------------------------------------
void audAmbientAudioEntity::DebugDrawWallaHostile()
{
#if __BANK
	static const char *meterNamesDirection[] = {"North", "East", "South", "West", "Final"};
	static audMeterList meterListPanic;
	static audMeterList meterListDensity;
	static audMeterList meterListBlocked;
	static f32 meterValuesDensity[5];
	static f32 pedCount[4];
	static f32 blockedFactor[4];

	for (s32 i=0; i<4; i++)
	{
		pedCount[i] = audNorthAudioEngine::GetGtaEnvironment()->GetHostilePedCountForDirection((audAmbienceDirection)i);
		blockedFactor[i] = audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection((audAmbienceDirection)i);

		f32 density = GetHostilePedDensityForDirection((audAmbienceDirection)i);
		meterValuesDensity[i] = density;
	}

	meterValuesDensity[4] = m_FinalPedDensityHostile;

	meterListDensity.left = 790.f;
	meterListDensity.bottom = 620.f;
	meterListDensity.width = 400.f;
	meterListDensity.height = 200.f;
	meterListDensity.title = "Hostile Ped Density"; 
	meterListDensity.values = &meterValuesDensity[0];
	meterListDensity.names = meterNamesDirection;
	meterListDensity.numValues = sizeof(meterNamesDirection)/sizeof(meterNamesDirection[0]);
	audNorthAudioEngine::DrawLevelMeters(&meterListDensity);

	meterListBlocked.left = 190.f;
	meterListBlocked.bottom = 620.f;
	meterListBlocked.width = 400.f;
	meterListBlocked.height = 200.f;
	meterListBlocked.title = "Blocked Factor"; 
	meterListBlocked.values = &blockedFactor[0];
	meterListBlocked.names = meterNamesDirection;
	meterListBlocked.numValues = sizeof(meterNamesDirection)/sizeof(meterNamesDirection[0]) - 1;
	audNorthAudioEngine::DrawLevelMeters(&meterListBlocked);

	char tempString[128];
	formatf(tempString, "Ped Counts:");
	grcDebugDraw::Text(Vector2(0.48f, 0.7f), Color32(255,255,255), tempString);

	formatf(tempString, "North: %d", (u32)pedCount[0]);
	grcDebugDraw::Text(Vector2(0.50f, 0.72f), Color32(255,255,255), tempString);

	formatf(tempString, "East: %d", (u32)pedCount[1]);
	grcDebugDraw::Text(Vector2(0.50f, 0.74f), Color32(255,255,255), tempString);

	formatf(tempString, "South: %d", (u32)pedCount[2]);
	grcDebugDraw::Text(Vector2(0.50f, 0.76f), Color32(255,255,255), tempString);

	formatf(tempString, "West: %d", (u32)pedCount[3]);
	grcDebugDraw::Text(Vector2(0.50f, 0.78f), Color32(255,255,255), tempString);
#endif
}

// ----------------------------------------------------------------
// Draw any ped walla debug info
// ----------------------------------------------------------------
void audAmbientAudioEntity::DebugDrawWallaInterior()
{
#if __BANK
	static const char *meterNamesDirection[] = {"North", "East", "South", "West", "Final"};
	static audMeterList meterListPanic;
	static audMeterList meterListDensity;
	static audMeterList meterListBlocked;
	static f32 meterValuesDensity[5];
	static f32 pedCount[4];

	for (s32 i=0; i<4; i++)
	{
		pedCount[i] = audNorthAudioEngine::GetGtaEnvironment()->GetInteriorOccludedPedCountForDirection((audAmbienceDirection)i);
		f32 density = GetInteriorPedDensityForDirection((audAmbienceDirection)i);
		meterValuesDensity[i] = density;
	}

	meterValuesDensity[4] = m_FinalPedDensityInterior;

	meterListDensity.left = 790.f;
	meterListDensity.bottom = 620.f;
	meterListDensity.width = 400.f;
	meterListDensity.height = 200.f;
	meterListDensity.title = "Interior Ped Density"; 
	meterListDensity.values = &meterValuesDensity[0];
	meterListDensity.names = meterNamesDirection;
	meterListDensity.numValues = sizeof(meterNamesDirection)/sizeof(meterNamesDirection[0]);
	audNorthAudioEngine::DrawLevelMeters(&meterListDensity);

	char tempString[128];

	if(m_ScriptDesiredPedDensityInteriorApplyFactor > 0.0f)
	{
		formatf(tempString, "Script Adjusting Interior Density! Requested %.02f, Apply Factor %.02f", m_ScriptDesiredPedDensityInterior, m_ScriptDesiredPedDensityInteriorApplyFactor);
		grcDebugDraw::Text(Vector2(0.3f, 0.1f), Color32(255,0,0), tempString);
	}
	
	formatf(tempString, "Ped/Listener Height Diff: %.02f", CalculatePedsToListenerHeightDiff());
	grcDebugDraw::Text(Vector2(0.18f, 0.3f), Color32(255,255,255), tempString);

	formatf(tempString, "Ped Counts:");
	grcDebugDraw::Text(Vector2(0.48f, 0.7f), Color32(255,255,255), tempString);

	formatf(tempString, "North: %.02f", pedCount[0]);
	grcDebugDraw::Text(Vector2(0.50f, 0.72f), Color32(255,255,255), tempString);

	formatf(tempString, "East: %.02f", pedCount[1]);
	grcDebugDraw::Text(Vector2(0.50f, 0.74f), Color32(255,255,255), tempString);

	formatf(tempString, "South: %.02f", pedCount[2]);
	grcDebugDraw::Text(Vector2(0.50f, 0.76f), Color32(255,255,255), tempString);

	formatf(tempString, "West: %.02f", pedCount[3]);
	grcDebugDraw::Text(Vector2(0.50f, 0.78f), Color32(255,255,255), tempString);

	formatf(tempString, "Max Density: %.02f", m_MaxValidPedDensityInterior);
	grcDebugDraw::Text(Vector2(0.50f, 0.80f), Color32(255,255,255), tempString);

	formatf(tempString, "Min Density: %.02f", m_MinValidPedDensityInterior);
	grcDebugDraw::Text(Vector2(0.50f, 0.82f), Color32(255,255,255), tempString);

	formatf(tempString, "Density Scalar: %.02f", m_PedDensityScalarInterior);
	grcDebugDraw::Text(Vector2(0.50f, 0.84f), Color32(255,255,255), tempString);
#endif
}

// ----------------------------------------------------------------
// Calculate the min and max ped density from the currently active zones
// ----------------------------------------------------------------
void audAmbientAudioEntity::CalculateActiveZonesMinMaxPedDensity(f32& minPedDensity, f32& maxPedDensity, f32& pedDensityScalar, bool interiorZones)
{
	const Vec3V listPos = g_AudioEngine.GetEnvironment().GetPanningListenerPosition();
	audAmbientZone* smallestZone = NULL;
	audAmbientZone* secondSmallestZone = NULL;

	f32 smallestArea = FLT_MAX;
	f32 secondSmallestArea = FLT_MAX;

	const f32 defaultMinDensity = 0.0f;
	const f32 defaultMaxDensity = audAmbientZone::sm_DefaultPedDensityTODCurve.CalculateValue(static_cast<f32>(CClock::GetHour()) + (CClock::GetMinute()/60.f));
	const f32 defaultPedDensityScalar = 1.0f;

	f32 calculatedMinDensity = defaultMinDensity;
	f32 calculatedMaxDensity = defaultMaxDensity;
	f32 calculatedPedDensityScalar = defaultPedDensityScalar;

	const u32 numZones = interiorZones? m_InteriorZones.GetCount() : m_ActiveZones.GetCount();

	for(u32 loop = 0; loop < numZones; loop++)
	{
		audAmbientZone* zone = interiorZones? &m_InteriorZones[loop] : m_ActiveZones[loop];

		if(zone->GetZoneData()->MinPedDensity != 0.0f || 
		   zone->GetZoneData()->MaxPedDensity != 1.0f || 
		   zone->GetZoneData()->PedDensityScalar != 1.0f || 
		   zone->HasCustomPedDensityTODCurve())
		{
			const f32 zoneArea = zone->GetPositioningArea();

			if(zoneArea < smallestArea || smallestZone == NULL)
			{
				if(smallestZone != NULL)
				{
					if(zoneArea < secondSmallestArea || secondSmallestZone == NULL)
					{
						secondSmallestZone = smallestZone;
						secondSmallestArea = smallestArea;
					}
				}

				smallestZone = zone;
				smallestArea = zoneArea;
			}
			else if(zoneArea < secondSmallestArea || secondSmallestZone == NULL)
			{
				secondSmallestZone = zone;
				secondSmallestArea = zoneArea;
			}
		}
	}

	if(smallestZone)
	{
		f32 smallestZoneMinDensity = 0.0f;
		f32 smallestZoneMaxDensity = 0.0f;
		f32 smallestZonePedDensityScalar = 0.0f;

		f32 secondSmallestZoneMinDensity = defaultMinDensity;
		f32 secondSmallestZoneMaxDensity = defaultMaxDensity;
		f32 secondSmallestZoneDensityScalar = defaultPedDensityScalar;

		if(secondSmallestZone)
		{
			secondSmallestZone->GetPedDensityValues(secondSmallestZoneMinDensity, secondSmallestZoneMaxDensity);
			secondSmallestZoneDensityScalar = secondSmallestZone->GetPedDensityScalar();

			const f32 secondSmallestZoneInfluence = secondSmallestZone->ComputeZoneInfluenceRatioAtPoint(listPos);
			secondSmallestZoneMinDensity = ((secondSmallestZoneMinDensity * secondSmallestZoneInfluence) + (defaultMinDensity * (1.0f - secondSmallestZoneInfluence)));
			secondSmallestZoneMaxDensity = ((secondSmallestZoneMaxDensity * secondSmallestZoneInfluence) + (defaultMaxDensity * (1.0f - secondSmallestZoneInfluence)));
			secondSmallestZoneDensityScalar = ((secondSmallestZoneDensityScalar * secondSmallestZoneInfluence) + (defaultPedDensityScalar * (1.0f - secondSmallestZoneInfluence)));
		}

		smallestZone->GetPedDensityValues(smallestZoneMinDensity, smallestZoneMaxDensity);
		smallestZonePedDensityScalar = smallestZone->GetPedDensityScalar();

		const f32 smallestZoneInfluence = smallestZone->ComputeZoneInfluenceRatioAtPoint(listPos);
		calculatedMinDensity = ((smallestZoneMinDensity * smallestZoneInfluence) + (secondSmallestZoneMinDensity * (1.0f - smallestZoneInfluence)));
		calculatedMaxDensity = ((smallestZoneMaxDensity * smallestZoneInfluence) + (secondSmallestZoneMaxDensity * (1.0f - smallestZoneInfluence)));
		calculatedPedDensityScalar = ((smallestZonePedDensityScalar * smallestZoneInfluence) + (secondSmallestZoneDensityScalar * (1.0f - smallestZoneInfluence)));
	}

#if __BANK
	if(!interiorZones)
	{
		g_SmallestPedDensityZone = smallestZone;
		g_SecondSmallestPedDensityZone = secondSmallestZone;
	}
#endif
	
	minPedDensity = calculatedMinDensity;
	maxPedDensity = calculatedMaxDensity;
	pedDensityScalar = calculatedPedDensityScalar;
}

// ----------------------------------------------------------------
// Calculate the difference in z pos between the peds and the listener
// ----------------------------------------------------------------
f32 audAmbientAudioEntity::CalculatePedsToListenerHeightDiff()
{
	const Matrix34 listenerMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
	f32 averageZPos = audNorthAudioEngine::GetGtaEnvironment()->GetAveragePedZPos();
	return abs(averageZPos - listenerMatrix.d.z);
}

// ----------------------------------------------------------------
// Check if the given alarm is active
// ----------------------------------------------------------------
bool audAmbientAudioEntity::IsAlarmActive(AlarmSettings* alarmSettings)
{
	for(u32 loop = 0; loop < kMaxActiveAlarms; loop++)
	{
		if(m_ActiveAlarms[loop].alarmSettings == alarmSettings)
		{
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Trigger an ambient streaming sound
// ----------------------------------------------------------------
bool audAmbientAudioEntity::TriggerStreamedAmbientOneShot(const u32 soundName, audSoundInitParams* initParams)
{
	// Only allow if there are lots of slots free
	if(g_SpeechManager.GetNumVacantAmbientSlots() <= 3)
	{
		return false;
	}

	for(u32 loop = 0; loop < m_AmbientStreamedSoundSlots.GetMaxCount(); loop++)
	{
		AmbientStreamedSoundSlot* streamedSoundSlot = &m_AmbientStreamedSoundSlots[loop];

		// Check if the slot is free
		if(!streamedSoundSlot->sound && !streamedSoundSlot->speechSlot)
		{
			// Try to grab a free ambient speech slot at low priority
			s32 speechSlotId = g_SpeechManager.GetAmbientSpeechSlot(NULL, NULL, -1.0f);

			if(speechSlotId >= 0)
			{
				audWaveSlot* speechSlot = g_SpeechManager.GetAmbientWaveSlotFromId(speechSlotId);

				// Success! Use this slot to load and play our asset
				if(speechSlot)
				{
					streamedSoundSlot->speechSlotID = speechSlotId;
					streamedSoundSlot->speechSlot = speechSlot;

					initParams->WaveSlot = speechSlot;
					initParams->AllowLoad = true;
					initParams->PrepareTimeLimit = 5000;

					g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(speechSlotId, 0, 
#if __BANK
						"AMBIENT_AUDIO_ENTITY_TRIGGERED",
#endif
						0);

					CreateAndPlaySound_Persistent(soundName, &streamedSoundSlot->sound, initParams);
					return streamedSoundSlot->sound != NULL;

				}
				else
				{
					g_SpeechManager.FreeAmbientSpeechSlot(speechSlotId, true);
					continue;
				}
			}
			else
			{
				continue;
			}
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Update the streaming sounds and free waveslots for any that have stopped
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdateStreamedAmbientSounds()
{
	for(u32 loop = 0; loop < m_AmbientStreamedSoundSlots.GetMaxCount(); loop++)
	{
		if(m_AmbientStreamedSoundSlots[loop].speechSlotID >= 0)
		{
			if(!m_AmbientStreamedSoundSlots[loop].sound)
			{
				g_SpeechManager.FreeAmbientSpeechSlot(m_AmbientStreamedSoundSlots[loop].speechSlotID, true);
				m_AmbientStreamedSoundSlots[loop].speechSlotID = -1;
				m_AmbientStreamedSoundSlots[loop].speechSlot = NULL;
			}
		}
	}
}

// ----------------------------------------------------------------
// Stop all the streaming sounds 
// ----------------------------------------------------------------
void audAmbientAudioEntity::StopAllStreamedAmbientSounds()
{
	for(u32 loop = 0; loop < m_AmbientStreamedSoundSlots.GetMaxCount(); loop++)
	{		
		if(m_AmbientStreamedSoundSlots[loop].sound)
		{
			m_AmbientStreamedSoundSlots[loop].sound->StopAndForget();
		}
	}
}

// ----------------------------------------------------------------
// Check if the given sound requires a stream slot to play
// ----------------------------------------------------------------
bool audAmbientAudioEntity::DoesSoundRequireStreamSlot(audMetadataRef sound)
{
	class StreamingSoundQueryFn : public audSoundFactory::audSoundProcessHierarchyFn
	{
	public:
		bool containsStreamingSounds;

		StreamingSoundQueryFn() : containsStreamingSounds(false) {};

		void operator()(u32 classID, const void* UNUSED_PARAM(soundData))
		{
			if(classID == StreamingSound::TYPE_ID)
			{
				containsStreamingSounds = true;
			}
		}
	};

	StreamingSoundQueryFn streamingSoundQuery;
	SOUNDFACTORY.ProcessHierarchy(sound, streamingSoundQuery);
	return streamingSoundQuery.containsStreamingSounds;
}

// ----------------------------------------------------------------
// Set the sound set to use for ped walla
// ----------------------------------------------------------------
bool audAmbientAudioEntity::SetScriptWallaSoundSet(u32 soundSetName, s32 scriptThreadID, bool interior)
{
	if(interior)
	{
		naAssertf(m_ScriptControllingInteriorWallaSoundSet == -1 || scriptThreadID == m_ScriptControllingInteriorWallaSoundSet, "Script %d attempting to modify interior audio ped walla type when script %d already has control!", scriptThreadID, m_ScriptControllingPedDensityInterior);

		if(m_ScriptControllingInteriorWallaSoundSet == -1 || scriptThreadID == m_ScriptControllingInteriorWallaSoundSet)
		{
			if(soundSetName != m_CurrentInteriorWallaSoundSetName)
			{
				if(m_InteriorWallaSoundSet.Init(soundSetName))
				{
					m_CurrentInteriorWallaSoundSetName = soundSetName;
					m_DoesInteriorWallaRequireStreamSlot = DoesSoundRequireStreamSlot(m_InteriorWallaSoundSet.Find(ATSTRINGHASH("Heavy", 0xB1AEBE06)));
					m_ScriptControllingInteriorWallaSoundSet = scriptThreadID;
					m_InteriorWallaSwitchActive = true;
					return true;
				}
			}
		}
	}
	else
	{
		naAssertf(m_ScriptControllingExteriorWallaSoundSet == -1 || scriptThreadID == m_ScriptControllingExteriorWallaSoundSet, "Script %d attempting to modify exterior audio ped walla type when script %d already has control!", scriptThreadID, m_ScriptControllingPedDensityExterior);

		if(m_ScriptControllingExteriorWallaSoundSet == -1 || scriptThreadID == m_ScriptControllingExteriorWallaSoundSet)
		{
			if(soundSetName != m_CurrentExteriorWallaSoundSetName)
			{
				if(m_ExteriorWallaSoundSet.Init(soundSetName))
				{
					m_CurrentExteriorWallaSoundSetName = soundSetName;
					m_DoesExteriorWallaRequireStreamSlot = DoesSoundRequireStreamSlot(m_ExteriorWallaSoundSet.Find(ATSTRINGHASH("Heavy", 0xB1AEBE06)));
					m_ScriptControllingExteriorWallaSoundSet = scriptThreadID;
					m_ExteriorWallaSwitchActive = true;
					return true;
				}
			}
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Cancel any changes to the walla sound set
// ----------------------------------------------------------------
void audAmbientAudioEntity::CancelScriptWallaSoundSet(s32 scriptThreadID)
{
	if(m_ScriptControllingExteriorWallaSoundSet == scriptThreadID)
	{
		if(m_DefaultExteriorWallaSoundSetName != m_CurrentExteriorWallaSoundSetName)
		{
			m_CurrentExteriorWallaSoundSetName = m_DefaultExteriorWallaSoundSetName;
			m_ExteriorWallaSoundSet.Init(m_DefaultExteriorWallaSoundSetName);
			m_DoesExteriorWallaRequireStreamSlot = DoesSoundRequireStreamSlot(m_ExteriorWallaSoundSet.Find(ATSTRINGHASH("Heavy", 0xB1AEBE06)));
			m_ExteriorWallaSwitchActive = true;
		}
		
		m_ScriptControllingExteriorWallaSoundSet = -1;
	}

	if(m_ScriptControllingInteriorWallaSoundSet == scriptThreadID)
	{
		// Script has terminated, so fall back to whatever the interior settings has requested
		if(m_CurrentInteriorRoomHasWalla)
		{
			if(m_CurrentInteriorWallaSoundSetName != m_CurrentInteriorRoomWallaSoundSet)
			{
				m_CurrentInteriorWallaSoundSetName = m_CurrentInteriorRoomWallaSoundSet;
				m_InteriorWallaSoundSet.Reset();
				m_InteriorWallaSoundSet.Init(m_CurrentInteriorWallaSoundSetName);
				m_DoesInteriorWallaRequireStreamSlot = DoesSoundRequireStreamSlot(m_InteriorWallaSoundSet.Find(ATSTRINGHASH("Heavy", 0xB1AEBE06)));
				m_InteriorWallaSwitchActive = true;
			}
		}
		// Room settings has requested no walla, so just clear the soundset and fade out
		else
		{
			m_CurrentInteriorWallaSoundSetName = g_NullSoundHash;
			m_InteriorWallaSoundSet.Reset();
			m_InteriorWallaSwitchActive = true;
		}

		m_ScriptControllingInteriorWallaSoundSet = -1;
		
	}
}

// ----------------------------------------------------------------
// Set the ped density via a script command
// ----------------------------------------------------------------
bool audAmbientAudioEntity::SetScriptDesiredPedDensity(f32 desiredDensity, f32 desiredDensityApplyFactor, s32 scriptThreadID, bool interior)
{ 
	if(interior)
	{
		naAssertf(m_ScriptControllingPedDensityInterior == -1 || scriptThreadID == m_ScriptControllingPedDensityInterior, "Script %d attempting to modify audio ped density controls when script %d already has control!", scriptThreadID, m_ScriptControllingPedDensityInterior);

		if(m_ScriptControllingPedDensityInterior == -1 || scriptThreadID == m_ScriptControllingPedDensityInterior)
		{
			m_ScriptDesiredPedDensityInterior = desiredDensity; 
			m_ScriptDesiredPedDensityInteriorApplyFactor = desiredDensityApplyFactor; 
			m_ScriptControllingPedDensityInterior = scriptThreadID;
			return true;
		}
	}
	else
	{
		naAssertf(m_ScriptControllingPedDensityExterior == -1 || scriptThreadID == m_ScriptControllingPedDensityExterior, "Script %d attempting to modify audio ped density controls when script %d already has control!", scriptThreadID, m_ScriptControllingPedDensityExterior);

		if(m_ScriptControllingPedDensityExterior == -1 || scriptThreadID == m_ScriptControllingPedDensityExterior)
		{
			m_ScriptDesiredPedDensityExterior = desiredDensity; 
			m_ScriptDesiredPedDensityExteriorApplyFactor = desiredDensityApplyFactor; 
			m_ScriptControllingPedDensityExterior = scriptThreadID;
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Cancel any script changes to the ped density
// ----------------------------------------------------------------
void audAmbientAudioEntity::CancelScriptPedDensityChanges(s32 scriptThreadID)
{ 
	if(m_ScriptControllingPedDensityExterior == scriptThreadID)
	{
		m_ScriptDesiredPedDensityExterior = 0.0f; 
		m_ScriptDesiredPedDensityExteriorApplyFactor = 0.0f; 
		m_ScriptControllingPedDensityExterior = -1;
	}

	if(m_ScriptControllingPedDensityInterior == scriptThreadID)
	{
		m_ScriptDesiredPedDensityInterior = 0.0f; 
		m_ScriptDesiredPedDensityInteriorApplyFactor = 0.0f; 
		m_ScriptControllingPedDensityInterior = -1;
	}
}

// ----------------------------------------------------------------
// Update the interior walla
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdateInteriorWalla()
{
	s32 roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

	// Work out our interior settings and what room we are in
	const InteriorSettings* interiorSettings = NULL;
	const InteriorRoom* roomSettings = NULL;
	audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForInterior(pIntInst, roomIdx, interiorSettings, roomSettings);

	if(interiorSettings)
	{
		bool currentInteriorInstanceHasWallaSound = (AUD_GET_TRISTATE_VALUE(interiorSettings->Flags, FLAG_ID_INTERIORSETTINGS_HASINTERIORWALLA) == AUD_TRISTATE_TRUE);
		u32 currentInteriorInstanceWallaSound = interiorSettings->InteriorWallaSoundSet;

		if(roomSettings)
		{
			if(AUD_GET_TRISTATE_VALUE(roomSettings->Flags, FLAG_ID_INTERIORROOM_HASINTERIORWALLA) == AUD_TRISTATE_TRUE)
			{
				m_CurrentInteriorRoomHasWalla = true;
				m_CurrentInteriorRoomWallaSoundSet = roomSettings->InteriorWallaSoundSet;
			}
			else if(AUD_GET_TRISTATE_VALUE(roomSettings->Flags, FLAG_ID_INTERIORROOM_HASINTERIORWALLA) == AUD_TRISTATE_UNSPECIFIED)
			{
				m_CurrentInteriorRoomHasWalla = currentInteriorInstanceHasWallaSound;
				m_CurrentInteriorRoomWallaSoundSet = currentInteriorInstanceWallaSound;
			}
			else
			{
				m_CurrentInteriorRoomHasWalla = false;
			}

			if(m_ScriptControllingInteriorWallaSoundSet == -1)
			{
				if(m_CurrentInteriorRoomHasWalla)
				{
					// The room has walla and its not the same as the existing walla, so we need to switch
					if(m_CurrentInteriorRoomWallaSoundSet != m_CurrentInteriorWallaSoundSetName)
					{
						m_CurrentInteriorWallaSoundSetName = m_CurrentInteriorRoomWallaSoundSet;
						m_InteriorWallaSoundSet.Reset();
						m_InteriorWallaSoundSet.Init(m_CurrentInteriorWallaSoundSetName);
						m_DoesInteriorWallaRequireStreamSlot = DoesSoundRequireStreamSlot(m_InteriorWallaSoundSet.Find(ATSTRINGHASH("Heavy", 0xB1AEBE06)));
						m_InteriorWallaSwitchActive = true;
					}
				}
				else
				{
					m_CurrentInteriorWallaSoundSetName = g_NullSoundHash;
					m_InteriorWallaSoundSet.Reset();
					m_InteriorWallaSwitchActive = true;
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Used by speech sounds to query what dynamic speech line to play
// ----------------------------------------------------------------
void audAmbientAudioEntity::QuerySpeechVoiceAndContextFromField(const u32 field, u32& voiceNameHash, u32& contextNamePartialHash)
{
	if(field == ATSTRINGHASH("WALLA_SPEECH", 0x89E6B9DF))
	{
		voiceNameHash = m_WallaSpeechSelectedVoice;
		contextNamePartialHash = m_WallaSpeechSelectedContext;
	}
}

// ----------------------------------------------------------------
// Get the last time a voice context was used for ambient walla
// ----------------------------------------------------------------
bool audAmbientAudioEntity::GetLastTimeVoiceContextWasUsedForAmbientWalla(u32 voice, u32 contextName, u32& timeInMs)
{
	bool found = false;
	u32 mostRecentTime = 0;

	for(u32 loop = 0; loop < m_AmbientWallaHistory.GetCount(); loop++)
	{
		if(m_AmbientWallaHistory[loop].voiceName == voice &&
		   m_AmbientWallaHistory[loop].context == contextName)
		{
			mostRecentTime = Max(mostRecentTime, m_AmbientWallaHistory[loop].timeTriggered);
			found = true;
		}
	}

	timeInMs = mostRecentTime;
	return found;
}

// ----------------------------------------------------------------
// Update the walla speech
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdateWallaSpeech()
{
	if(!g_EnableWallaSpeech)
	{
		return;
	}

#if __BANK
	m_RMSLevelSmoother.SetRates(1.0f/g_SFXRMSSmoothingRateIncrease, 1.0f/g_SFXRMSSmoothingRateDecrease);
#endif

	bool processingComplete = ProcessValidWallaSpeechGroups();
	m_WallaSpeechTimer = Max(m_WallaSpeechTimer - fwTimer::GetTimeStep(), 0.0f);

	CVehicle* playerVehicle = FindPlayerVehicle();
	bool vehicleIsValid = (playerVehicle == NULL || playerVehicle->InheritsFromBicycle());
	bool inInterior = (CPortalVisTracker::GetPrimaryInteriorInst() != NULL);
	bool environmentIsValid = !g_weather.IsRaining() && !Water::IsCameraUnderwater() && m_AverageFractionPanicingExterior < 0.2f;
	bool heightValid = Abs(g_AudioEngine.GetEnvironment().GetPanningListenerPosition().GetZf() - audNorthAudioEngine::GetGtaEnvironment()->GetAveragePedZPos()) < 10.0f;
	const u32 timeInMS = fwTimer::GetTimeInMilliseconds();

	// We're not playing any walla speech and we want to be
	if(processingComplete && 
	   !m_WallaSpeechSound && 
	   m_WallaSpeechSlotID == -1 && 
	   ((vehicleIsValid && environmentIsValid && !inInterior && heightValid) BANK_ONLY(|| g_ForceWallaSpeechRetrigger)))
	{
		if(m_WallaSpeechTimer <= 0.0f BANK_ONLY(|| g_ForceWallaSpeechRetrigger))
		{
			bool reRandomiseTimer = true;
			f32 maxPedDensityForDirection = 0.0f;
			m_CurrentPedWallaSpeechSettings = NULL;

#if __BANK
			g_WallaSpeechSettingsListName = NULL;
			g_WallaSpeechSettingsName = NULL;
#endif

			// If we've got an array of valid voices, pick a random one to play
			if(m_ValidWallaVoices.GetCount() > 0)
			{
				audAmbientZone* smallestZone = NULL;
				f32 smallestZoneArea = 0.0f;

				for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
				{
					if(m_ActiveZones[loop]->GetZoneData()->PedWallaSettings != 0)
					{
						const f32 zoneArea = m_ActiveZones[loop]->GetPositioningArea();

						if(smallestZone == NULL || zoneArea < smallestZoneArea)
						{
							smallestZone = m_ActiveZones[loop];
							smallestZoneArea = zoneArea;
						}
					}
				}

				PedWallaSettingsList* pedWallaSettingsList = NULL;

				// Check if the current smallest zone wants to override the ped walla settings
				if(smallestZone && smallestZone->GetZoneData()->PedWallaSettings != 0)
				{
					pedWallaSettingsList = audNorthAudioEngine::GetObject<PedWallaSettingsList>(smallestZone->GetZoneData()->PedWallaSettings);
				}
				else
				{
					pedWallaSettingsList = audNorthAudioEngine::GetObject<PedWallaSettingsList>(ATSTRINGHASH("WALLA_SPEECH_LIST_DEFAULT", 0xF7470A9F));
				}

				if(pedWallaSettingsList)
				{
#if __BANK
					g_WallaSpeechSettingsListName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(pedWallaSettingsList->NameTableOffset);
#endif

					// Bias gender selection according to the actual ped population
					bool chooseMale = audEngineUtil::ResolveProbability(audNorthAudioEngine::GetGtaEnvironment()->GetPedFractionMale());
					u32 randomSettingsIndex = 0;
					u32 randomVoiceName = 0;

					u32 numValidGenderVoices = 0;
					PedWallaValidVoice* const validGenderVoices = Alloca(PedWallaValidVoice, m_ValidWallaVoices.GetCount());

					// We've got a gender, so need to pick a valid ped voice group
					for(u32 loop = 0; loop < m_ValidWallaVoices.GetCount(); loop++)
					{
						if(m_ValidWallaVoices[loop].isMale == chooseMale)
						{
							validGenderVoices[numValidGenderVoices] = m_ValidWallaVoices[loop];
							numValidGenderVoices++;
						}
					}

					if(numValidGenderVoices > 0)
					{
						PedWallaValidVoice randomVoice = validGenderVoices[audEngineUtil::GetRandomNumberInRange(0, numValidGenderVoices - 1)];
						randomVoiceName = randomVoice.voiceNameHash;
						const s8 maleInt = chooseMale? 1 : 0;
						const s8 gangInt = randomVoice.isGang? 1 : 0;

						u32 numValidPedSettings = 0;
						u32* const validPedSettings = Alloca(u32, pedWallaSettingsList->numPedWallaSettingsInstances);
						f32* const validPedSettingsWeights = Alloca(f32, pedWallaSettingsList->numPedWallaSettingsInstances);
						f32 pedSettingsWeightSum = 0.0f;

						// We need to find walla settings that match both the gender and the gang status of the given ped
						for(u32 loop = 0; loop < pedWallaSettingsList->numPedWallaSettingsInstances; loop++)
						{
							if(pedWallaSettingsList->PedWallaSettingsInstance[loop].IsMale == -1 || pedWallaSettingsList->PedWallaSettingsInstance[loop].IsMale == maleInt)
							{
								if(pedWallaSettingsList->PedWallaSettingsInstance[loop].IsGang == -1 || pedWallaSettingsList->PedWallaSettingsInstance[loop].IsGang == gangInt)
								{
									validPedSettings[numValidPedSettings] = pedWallaSettingsList->PedWallaSettingsInstance[loop].PedWallaSettings;
									validPedSettingsWeights[numValidPedSettings] = pedWallaSettingsList->PedWallaSettingsInstance[loop].Weight;
									pedSettingsWeightSum += pedWallaSettingsList->PedWallaSettingsInstance[loop].Weight;
									numValidPedSettings++;
								}
							}
						}

						if(numValidPedSettings > 0)
						{
							f32 randomPedWeight = audEngineUtil::GetRandomNumberInRange(0.f, pedSettingsWeightSum);

							for(u32 loop = 0; loop < numValidPedSettings; loop++)
							{
								randomPedWeight -= validPedSettingsWeights[loop];
								if(randomPedWeight <= 0.f)
								{
									m_CurrentPedWallaSpeechSettings = audNorthAudioEngine::GetObject<PedWallaSpeechSettings>(validPedSettings[loop]);
									break;
								}
							}						
						}
					}

					if(m_CurrentPedWallaSpeechSettings)
					{
#if __BANK
						g_WallaSpeechSettingsName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_CurrentPedWallaSpeechSettings->NameTableOffset);
#endif

						f32 directionalWeightSum = 0.f;
						f32 directionWeights[AUD_AMB_DIR_LOCAL];
						f32 minWallaDensityForSpeech = m_CurrentPedWallaSpeechSettings->PedDensityThreshold;

						if(pedWallaSettingsList->PedWallaSettingsInstance[randomSettingsIndex].PedDensityThreshold >= 0.0f)
						{
							minWallaDensityForSpeech = pedWallaSettingsList->PedWallaSettingsInstance[randomSettingsIndex].PedDensityThreshold;
						}

#if __BANK
						g_WallaSpeechTriggerMinDensity = minWallaDensityForSpeech;
#endif

						// Sum up the walla contribution from all directions
						for(u32 direction = 0; direction < AUD_AMB_DIR_LOCAL; direction++)
						{
							f32 blockedFactor = g_BlockedToPedDensityScale.CalculateValue(audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection((audAmbienceDirection)direction));
							f32 densityForDirection = Clamp(GetExteriorPedDensityForDirection((audAmbienceDirection)direction) * blockedFactor * m_PedDensityScalarExterior, m_MinValidPedDensityExterior, m_MaxValidPedDensityExterior);

#if __BANK
							if(g_ForceWallaSpeechRetrigger)
							{
								// Force a valid ped density to ensure we keep retriggering
								densityForDirection = Max(densityForDirection, minWallaDensityForSpeech);
							}
#endif

							if(densityForDirection >= minWallaDensityForSpeech)
							{
								directionWeights[direction] = densityForDirection;
								maxPedDensityForDirection = Max(maxPedDensityForDirection, directionWeights[direction]);
								directionalWeightSum += directionWeights[direction];
							}
							else
							{
								directionWeights[direction] = 0.0f;
							}
						}

						if(maxPedDensityForDirection >= minWallaDensityForSpeech)
						{
							if(m_CurrentPedWallaSpeechSettings)
							{
								// Only allow if there are lots of slots free
								if(g_SpeechManager.GetNumVacantAmbientSlots() > 4)
								{
									// Try to grab a free ambient speech slot at low priority
									m_WallaSpeechSlotID = g_SpeechManager.GetAmbientSpeechSlot(NULL, NULL, -1.0f);
								}

								if(m_WallaSpeechSlotID >= 0)
								{
									audWaveSlot* speechSlot = g_SpeechManager.GetAmbientWaveSlotFromId(m_WallaSpeechSlotID);

									// Success! Use this slot to load and play our asset
									if(speechSlot)
									{
										// Choose a random weighted direction, and pick a random position from somewhere in that quadrant
										Vector3 randomPosition = Vector3(0, 0, 0);
										f32 randomDirectionWeight = audEngineUtil::GetRandomNumberInRange(0.f, directionalWeightSum);

										for(u32 direction = 0; direction < AUD_AMB_DIR_LOCAL; direction++)
										{
											randomDirectionWeight -= directionWeights[direction];
											if(randomDirectionWeight <= 0.f)
											{
												randomPosition = g_Directions[direction];
												randomPosition.RotateZ(audEngineUtil::GetRandomNumberInRange(-45, 45) * DtoR);
												break;
											}
										}

										u32 randomContextIndex = audEngineUtil::GetRandomNumberInRange(0, m_CurrentPedWallaSpeechSettings->numSpeechContexts - 1);
										const char* partialContextName = m_CurrentPedWallaSpeechSettings->SpeechContexts[randomContextIndex].ContextName;
										char contextString[32];

										u32 numValidVariations = 0;
										bool isPhoneConversation = false;
										u32 randomConversationVariation = 0;

										// If this context only has one type, just use that
										if(m_CurrentPedWallaSpeechSettings->SpeechContexts[randomContextIndex].Variations == 1)
										{
											formatf(contextString, partialContextName);
											numValidVariations = audSpeechSound::FindNumVariationsForVoiceAndContext(randomVoiceName, atPartialStringHash(contextString));
										}
										else
										{
											isPhoneConversation = true;

											// This context has multiple types, so generate a valid list and pick from that
											for(; numValidVariations < AUD_MAX_PHONE_CONVERSATIONS_PER_VOICE; numValidVariations++)
											{
												formatf(contextString, partialContextName, numValidVariations + 1);
												if(audSpeechSound::FindNumVariationsForVoiceAndContext(randomVoiceName, atPartialStringHash(contextString)) == 0)
												{
													break;
												}
											}

											randomConversationVariation = audEngineUtil::GetRandomNumberInRange(1, numValidVariations);
											formatf(contextString, partialContextName, randomConversationVariation);
										}

#if __BANK
										formatf(g_WallaSpeechSettingsContext, contextString);
#endif

										if(numValidVariations > 0)
										{
											// Set up our fields that will be queried by the dynamic speech sound
											m_WallaSpeechSelectedVoice = randomVoiceName;
											m_WallaSpeechSelectedContext = atPartialStringHash(contextString);
											bool historyCheckValid = true;

											if(isPhoneConversation)
											{
												for(u32 loop = 0; loop < m_AmbientWallaHistory.GetCount(); loop++)
												{
													if(m_AmbientWallaHistory[loop].isPhoneConversation)
													{
														if(m_WallaSpeechSelectedVoice == m_AmbientWallaHistory[loop].voiceName &&
															randomConversationVariation == m_AmbientWallaHistory[loop].conversationIndex)
														{
															historyCheckValid = false;
															break;
														}
													}
												}
											}
											else
											{
												for(u32 loop = 0; loop < m_AmbientWallaHistory.GetCount(); loop++)
												{
													if(m_WallaSpeechSelectedVoice == m_AmbientWallaHistory[loop].voiceName &&
														m_WallaSpeechSelectedContext == m_AmbientWallaHistory[loop].context)
													{
														historyCheckValid = false;
														break;
													}
												}
											}

											if(historyCheckValid && isPhoneConversation)
											{
												u32 lastTimePlayedInMs = 0;

												// Avoid replaying phone conversations that we've used recently
												s32 historyIndex = g_SpeechManager.WhenWasPhoneConversationLastPlayed(m_WallaSpeechSelectedVoice, randomConversationVariation, lastTimePlayedInMs);

												if(historyIndex >= 0)
												{
													if(timeInMS - lastTimePlayedInMs < 60000)
													{
														historyCheckValid = false;
													}
												}
											}

											if(historyCheckValid)
											{
												if(m_AmbientWallaHistory.IsFull())
												{
													m_AmbientWallaHistory.Pop();
												}

												g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(m_WallaSpeechSlotID, randomVoiceName, 
#if __BANK
													"AMBIENT_WALLA_SPEECH",
#endif
													0);

												AmbientWallaHistory wallaHistory;
												wallaHistory.voiceName = randomVoiceName;
												wallaHistory.context = m_WallaSpeechSelectedContext;
												wallaHistory.timeTriggered = timeInMS;
												wallaHistory.conversationIndex = randomConversationVariation;
												wallaHistory.isPhoneConversation = isPhoneConversation;
												m_AmbientWallaHistory.Push(wallaHistory);

												audSoundInitParams initParams;
												initParams.RemoveHierarchy = false;
												initParams.WaveSlot = speechSlot;
												initParams.AllowLoad = true;
												initParams.PrepareTimeLimit = 5000;

												Vector3 finalPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition()) + (randomPosition * 40.0f);
												finalPosition.SetZ(audNorthAudioEngine::GetGtaEnvironment()->GetAveragePedZPos());
												initParams.Position = finalPosition;

												naEnvironmentGroup* environmentGroup = naEnvironmentGroup::Allocate("WallaSpeech");
												if(environmentGroup)
												{
													environmentGroup->Init(NULL, 20);
													environmentGroup->SetSource(VECTOR3_TO_VEC3V(initParams.Position));
													environmentGroup->SetUsePortalOcclusion(true);
													environmentGroup->SetMaxPathDepth(3);
													environmentGroup->SetUseInteriorDistanceMetrics(true);
													environmentGroup->SetUseCloseProximityGroupsForUpdates(true);
													environmentGroup->SetInteriorInfoWithInteriorInstance(NULL, INTLOC_ROOMINDEX_LIMBO);
													initParams.EnvironmentGroup = environmentGroup;;
												}

												CreateAndPlaySound_Persistent(m_CurrentPedWallaSpeechSettings->SpeechSound, &m_WallaSpeechSound, &initParams);

#if __BANK
												if(g_LogWallaSpeech)
												{
													if(smallestZone)
													{
														audDisplayf("Walla Speech Smallest Zone: %s", smallestZone->GetName());
													}

													audDisplayf("Walla Speech Settings List: %s", g_WallaSpeechSettingsListName);
													audDisplayf("Walla Speech Settings: %s", g_WallaSpeechSettingsName);

													for(u32 loop = 0; loop < m_ValidWallaVoices.GetCount(); loop++)
													{
														if(m_ValidWallaVoices[loop].voiceNameHash == m_WallaSpeechSelectedVoice)
														{
															audDisplayf("Walla Speech Voice: %s", m_ValidWallaVoices[loop].pedVoiceGroupName);
															break;
														}
													}

													audDisplayf("Walla Speech Context: %s", contextString);
												}
#endif
											}
											else
											{
												reRandomiseTimer = false;
											}
										}
										else
										{
											// We got this far but failed simply because the chosen ped didn't have sufficient variations, so retry next frame
											reRandomiseTimer = false;
										}
									}
								}
							}
						}
					}
				}
			}

			// Pick a new time to play some speech based on how densely populated the area is
			if(reRandomiseTimer)
			{
				m_WallaSpeechTimer = m_MaxPedDensityToWallaSpeechTriggerRate.CalculateValue(maxPedDensityForDirection);

				// Reset our ped querying to grab up to date valid groups
				m_WallaPedGroupProcessingIndex = 0;
			}
		}
		// Intermittently check and shorten the re-trigger time to ensure that we don't get stuck with massively long times if the ped density spikes
		else if((fwTimer::GetSystemFrameCount() & 31) == PROCESS_WALLA_SPEECH_RETRIGGER && m_WallaSpeechTimer > 15.0f)
		{
			f32 maxPedDensityForDirection = 0.0f;

			for(u32 direction = 0; direction < AUD_AMB_DIR_LOCAL; direction++)
			{
				f32 blockedFactor = g_BlockedToPedDensityScale.CalculateValue(audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection((audAmbienceDirection)direction));
				f32 densityForDirection = Clamp(GetExteriorPedDensityForDirection((audAmbienceDirection)direction) * blockedFactor * m_PedDensityScalarExterior, m_MinValidPedDensityExterior, m_MaxValidPedDensityExterior);
				maxPedDensityForDirection = Max(maxPedDensityForDirection, densityForDirection);
			}

			f32 newTriggerRate = m_WallaSpeechTimer + ((m_MaxPedDensityToWallaSpeechTriggerRate.CalculateValue(maxPedDensityForDirection) - m_WallaSpeechTimer) * 0.5f);
			m_WallaSpeechTimer = Min(m_WallaSpeechTimer, newTriggerRate);
		}
	}

	f32* smoothedLoudnessPtr = g_AudioEngine.GetSoundManager().GetVariableAddress(ATSTRINGHASH("Game.Audio.RMSLevels.SFX", 0xA37BC1CC));

	if(smoothedLoudnessPtr)
	{
		// The sfx loudness is also smoothed by the sound manager, but we need different rates to prevent it peaking too quickly during loud events
		f32 smoothedLoudness = m_RMSLevelSmoother.CalculateValue(*smoothedLoudnessPtr, fwTimer::GetTimeInMilliseconds());

		if(m_WallaSpeechSound)
		{
			audAssert(m_CurrentPedWallaSpeechSettings);

			// Speech sound needs to ride the SFX RMS level, but is also capped to prevent it ever going too loud
			f32 volumeCap = m_CurrentPedWallaSpeechSettings->MaxVolume * 0.01f;
			f32 finalVolume = Min(volumeCap, audDriverUtil::ComputeDbVolumeFromLinear(smoothedLoudness) + (m_CurrentPedWallaSpeechSettings->VolumeAboveRMSLevel * 0.01f));

#if __BANK
			finalVolume += g_WallaSpeechVolumeTrim;
#endif

			m_WallaSpeechSound->SetRequestedPostSubmixVolumeAttenuation(finalVolume);
		}
	}
	
	if(!m_WallaSpeechSound && m_WallaSpeechSlotID >= 0)
	{
		g_SpeechManager.FreeAmbientSpeechSlot(m_WallaSpeechSlotID, true);
		m_WallaSpeechSlotID = -1;
	}

#if __BANK
	if(g_DebugDrawWallaSpeech)
	{
		DebugDrawWallaSpeech();
	}
#endif
}

// ----------------------------------------------------------------
// Process the current valid walla speech groups
// ----------------------------------------------------------------
bool audAmbientAudioEntity::ProcessValidWallaSpeechGroups()
{
	const u32 numGroups = CPopCycle::GetPopGroups().GetPedCount();

	if(m_WallaPedGroupProcessingIndex == 0 BANK_ONLY(|| g_ForceWallaSpeechRetrigger))
	{
		m_ValidWallaVoices.Reset();
		m_ValidWallaPedModels.Reset();
		m_WallaPedModelProcessingIndex = 0;
		m_WallaPedGroupProcessingIndex= 0;
	}
	
	u32 group = m_WallaPedGroupProcessingIndex;
	
	for(; group < numGroups; group++)
	{
		// Is this ped group valid in the current area?
		if(CPopCycle::GetCurrentPedGroupPercentage(group) > 0)
		{   
			u32 numModels = CPopCycle::GetPopGroups().GetPedGroup(group).GetCount();

			// If so, go through all the valid ped models and extract their voices
			for(u32 model = 0; model < numModels; model++)
			{
				fwModelId modelId((strLocalIndex(CPopCycle::GetPopGroups().GetPedGroup(group).GetModelIndex(model))));

				if (!modelId.IsValid())
					continue;

				CPedModelInfo* pedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);

				if (pedModelInfo && m_ValidWallaPedModels.GetAvailable() > 0)
				{
					if(m_ValidWallaPedModels.Find(pedModelInfo) == -1)
					{
						m_ValidWallaPedModels.Push(pedModelInfo);
					}
				}
			}

#if __BANK
			if(!g_ForceWallaSpeechRetrigger)
#endif
			{
				// Not massively time critical, so spread the workload by only processing one per frame
				group++;
				break;
			}
		}
	}

	m_WallaPedGroupProcessingIndex = group;

	if(m_WallaPedGroupProcessingIndex >= numGroups)
	{
		u32 modelIndex = m_WallaPedModelProcessingIndex;
		for(; modelIndex < m_ValidWallaPedModels.GetCount(); modelIndex++)
		{
			CPedModelInfo* pedModelInfo = m_ValidWallaPedModels[modelIndex];
			bool isMale = pedModelInfo->IsMale();
			bool isGang = pedModelInfo->GetIsGang() || pedModelInfo->GetPersonalitySettings().GetIsGang();

			audMetadataObjectInfo objectInfo;
			if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(pedModelInfo->GetPedVoiceGroup(), objectInfo))
			{
				if(objectInfo.GetType() == PedRaceToPVG::TYPE_ID)
				{
					const PedRaceToPVG* thisPRtoPVG = objectInfo.GetObject<PedRaceToPVG>();

					if(thisPRtoPVG)
					{
						CheckAndAddPedWallaVoice(thisPRtoPVG->Universal, isMale, isGang);
						CheckAndAddPedWallaVoice(thisPRtoPVG->White, isMale, isGang);
						CheckAndAddPedWallaVoice(thisPRtoPVG->Black, isMale, isGang);
						CheckAndAddPedWallaVoice(thisPRtoPVG->Chinese, isMale, isGang);
						CheckAndAddPedWallaVoice(thisPRtoPVG->Latino, isMale, isGang);
						CheckAndAddPedWallaVoice(thisPRtoPVG->Arab, isMale, isGang);
						CheckAndAddPedWallaVoice(thisPRtoPVG->Balkan, isMale, isGang);
						CheckAndAddPedWallaVoice(thisPRtoPVG->Jamaican, isMale, isGang);
						CheckAndAddPedWallaVoice(thisPRtoPVG->Korean, isMale, isGang);
						CheckAndAddPedWallaVoice(thisPRtoPVG->Italian, isMale, isGang);
						CheckAndAddPedWallaVoice(thisPRtoPVG->Pakistani, isMale, isGang);

#if __BANK
						if(!g_ForceWallaSpeechRetrigger)
#endif
						{
							// Not massively time critical, so spread the workload by only processing one per frame
							modelIndex++;
							break;
						}
					}
				}
			}
		}

		m_WallaPedModelProcessingIndex = modelIndex;
	}

	return m_WallaPedModelProcessingIndex == (u32)m_ValidWallaPedModels.GetCount();
}

// ----------------------------------------------------------------
// Check and add a walla PVG
// ----------------------------------------------------------------
void audAmbientAudioEntity::CheckAndAddPedWallaVoice(u32 voiceGroupHash, bool isMale, bool isGang)
{
	if(voiceGroupHash > 0u)
	{
		PedVoiceGroups* pedVoiceGroups = audNorthAudioEngine::GetObject<PedVoiceGroups>(voiceGroupHash);

		if(pedVoiceGroups)
		{
			for(u32 loop = 0; loop < pedVoiceGroups->numPrimaryVoices; loop++)
			{
				bool voiceValid = true;

				// Verify that we haven't already added this voice
				for(u32 existingVoiceLoop = 0; existingVoiceLoop < m_ValidWallaVoices.GetCount(); existingVoiceLoop++)
				{
					if(m_ValidWallaVoices[existingVoiceLoop].voiceNameHash == pedVoiceGroups->PrimaryVoice[loop].VoiceName)
					{
						voiceValid = false;		
						break;
					}
				}

				// Does this voice have at least one valid phone conversation set
				if(voiceValid && audSpeechSound::FindNumVariationsForVoiceAndContext(pedVoiceGroups->PrimaryVoice[loop].VoiceName,"PHONE_CONV1_INTRO") != 0)
				{
					if(m_ValidWallaVoices.GetAvailable() > 0)
					{
						PedWallaValidVoice validVoice;
						validVoice.voiceNameHash = pedVoiceGroups->PrimaryVoice[loop].VoiceName;
						validVoice.isMale = isMale;
						validVoice.isGang = isGang;
#if __BANK
						validVoice.pedVoiceGroupName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(pedVoiceGroups->NameTableOffset);
#endif

						m_ValidWallaVoices.Push(validVoice);
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Update ambient radio emitters
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdateAmbientRadioEmitters()
{
	const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	const u32 timeStep = timeInMs - m_FakeRadioTimeLastFrame;

	const CPed* playerPed = FindPlayerPed();
	const CVehicle* playerVehicle = FindPlayerVehicle();
	const Vec3V pedPos = playerPed? playerPed->GetTransform().GetPosition() : Vec3V(V_ZERO);
	const bool isScorePlaying = g_InteractiveMusicManager.IsMusicPlaying() && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowScoreAndRadio);
	const bool inInterior = (CPortalVisTracker::GetPrimaryInteriorInst() != NULL);
	bool isEnvironmentValid = true;

	for(u32 direction = 0; direction < AUD_AMB_DIR_LOCAL; direction++)
	{
		if(audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection((audAmbienceDirection)direction) > 0)
		{
			isEnvironmentValid = false;
			break;
		}
	}

	bool isVehicleValid = true;
	
	if(playerVehicle && !playerVehicle->InheritsFromBicycle())
	{
		isVehicleValid = false;
	}

	if(!isVehicleValid)
	{
		m_NextFakeVehicleRadioTriggerTime = Max(m_NextFakeVehicleRadioTriggerTime, timeInMs + 5000);
		m_NextFakeStaticRadioTriggerTime = Max(m_NextFakeStaticRadioTriggerTime, timeInMs + 5000);
	}
	
	bool isValidToPlay = g_EnableAmbientRadioEmitter && isVehicleValid && isEnvironmentValid && !g_RadioAudioEntity.StopEmittersForPlayerVehicle() && !isScorePlaying && !g_RadioAudioEntity.IsInFrontendMode() && !inInterior;

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		isValidToPlay = false;
	}
#endif

	RandomisedRadioEmitterSettings* radioEmitterSettings = NULL;

	if(isValidToPlay BANK_ONLY(|| g_DebugDrawAmbientRadioEmitter))
	{
		audAmbientZone* smallestZone = NULL;
		f32 smallestZoneArea = 0.0f;

		for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
		{
			if(m_ActiveZones[loop]->GetZoneData()->RandomisedRadioSettings != 0)
			{
				const f32 zoneArea = m_ActiveZones[loop]->GetPositioningArea();

				if(smallestZone == NULL || zoneArea < smallestZoneArea)
				{
					smallestZone = m_ActiveZones[loop];
					smallestZoneArea = zoneArea;
				}
			}
		}

		if(smallestZone && smallestZone->GetZoneData())
		{
			radioEmitterSettings = audNorthAudioEngine::GetObject<RandomisedRadioEmitterSettings>(smallestZone->GetZoneData()->RandomisedRadioSettings);
		}
	}

	if(radioEmitterSettings && m_FakeRadioEmitterSettingsLastFrame == NULL)
	{
		m_NextFakeVehicleRadioTriggerTime = timeInMs + audEngineUtil::GetRandomNumberInRange((s32)radioEmitterSettings->VehicleEmitterConfig.RetriggerTimeMin, (s32)radioEmitterSettings->VehicleEmitterConfig.RetriggerTimeMax);
		m_NextFakeStaticRadioTriggerTime = timeInMs + audEngineUtil::GetRandomNumberInRange((s32)radioEmitterSettings->StaticEmitterConfig.RetriggerTimeMin, (s32)radioEmitterSettings->StaticEmitterConfig.RetriggerTimeMax);
	}

	m_FakeRadioEmitterSettingsLastFrame = radioEmitterSettings;

	bool staticEmittersAllowed = false;
	bool vehicleEmittersAllowed = false;
	f32 gameTime = CClock::GetHour() + (CClock::GetMinute()/60.0f);

	if(radioEmitterSettings)
	{
		staticEmittersAllowed = radioEmitterSettings->VehicleEmitterBias < 1.0f && audAmbientZone::IsValidTime(radioEmitterSettings->StaticEmitterConfig.MinTime, radioEmitterSettings->StaticEmitterConfig.MaxTime, gameTime);
		vehicleEmittersAllowed = radioEmitterSettings->VehicleEmitterBias > 0.0f && audAmbientZone::IsValidTime(radioEmitterSettings->VehicleEmitterConfig.MinTime, radioEmitterSettings->VehicleEmitterConfig.MaxTime, gameTime);
	}

	if(m_FakeRadioEmitterState == AMBIENT_RADIO_OFF && (!isValidToPlay || !radioEmitterSettings))
	{
		m_NextFakeStaticRadioTriggerTime += timeStep;
		m_NextFakeVehicleRadioTriggerTime += timeStep;
	}
	else if(m_FakeRadioEmitterState != AMBIENT_RADIO_OFF)
	{
		if(m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_STATIC)
		{
			m_NextFakeVehicleRadioTriggerTime += timeStep;
		}
		else
		{
			m_NextFakeStaticRadioTriggerTime += timeStep;
		}
	}

	staticEmittersAllowed &= (timeInMs > m_NextFakeStaticRadioTriggerTime);
	vehicleEmittersAllowed &= (timeInMs > m_NextFakeVehicleRadioTriggerTime);

	CalculateValidAmbientRadioStations();

	if(isValidToPlay && !staticEmittersAllowed && !vehicleEmittersAllowed)
	{
		isValidToPlay = false;

		if(m_FakeRadioEmitterState == AMBIENT_RADIO_OFF && radioEmitterSettings && m_ValidRadioStations.GetCount() == 0)
		{
			m_NextFakeStaticRadioTriggerTime += timeStep;
			m_NextFakeVehicleRadioTriggerTime += timeStep;
		}
	}

	switch(m_FakeRadioEmitterState)
	{
	case AMBIENT_RADIO_OFF:
		{
			if(isValidToPlay && radioEmitterSettings)
			{
				if(staticEmittersAllowed)
				{
					m_FakeRadioEmitterType = AMBIENT_RADIO_TYPE_STATIC;
				}
				else if(vehicleEmittersAllowed)
				{
					m_FakeRadioEmitterType = AMBIENT_RADIO_TYPE_VEHICLE;
				}
					
				if(m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_STATIC)
				{
					m_AmbientRadioSettings = audNorthAudioEngine::GetObject<StaticEmitter>(radioEmitterSettings->StaticEmitterConfig.StaticEmitter);
				}
				else
				{
					m_AmbientRadioSettings = audNorthAudioEngine::GetObject<StaticEmitter>(radioEmitterSettings->VehicleEmitterConfig.StaticEmitter);
				}

				if(m_AmbientRadioSettings)
				{
					m_FakeRadioEmitterRadioStation = g_OffRadioStation;
					
					if(m_ValidRadioStations.GetCount() > 0)
					{
						m_FakeRadioEmitterRadioStationHash = m_ValidRadioStations[audEngineUtil::GetRandomNumberInRange(0, m_ValidRadioStations.GetCount() - 1)]->GetNameHash();
						m_FakeRadioEmitterRadioStation = g_RadioAudioEntity.RequestStaticRadio(&m_FakeRadioEmitter, m_FakeRadioEmitterRadioStationHash);
					}
					else
					{
						m_NextFakeStaticRadioTriggerTime += timeStep;
						m_NextFakeVehicleRadioTriggerTime += timeStep;
					}

					// If we found a valid station, choose a random distance and start/end positions
					if(m_FakeRadioEmitterRadioStation != g_OffRadioStation)
					{
						m_FakeRadioEmitterDistance = audEngineUtil::GetRandomNumberInRange(m_AmbientRadioSettings->MinDistance, m_AmbientRadioSettings->MaxDistance);
						const Vec3V listPos = g_AudioEngine.GetEnvironment().GetPanningListenerPosition();

						if(m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_VEHICLE)
						{
							m_FakeRadioEmitterStartPan = audEngineUtil::GetRandomNumberInRange(0.0f, 360.0f);	
							f32 randomPan = audEngineUtil::GetRandomNumberInRange(radioEmitterSettings->VehicleEmitterConfig.MinPanAngleChange, radioEmitterSettings->VehicleEmitterConfig.MaxPanAngleChange);

							if(audEngineUtil::ResolveProbability(0.5f))
							{
								randomPan *= -1.0f;
							}
							
							m_FakeRadioEmitterEndPan = m_FakeRadioEmitterStartPan + randomPan;
						}
						else
						{
							m_FakeRadioEmitterStartPan = CalculateCurrentPedDirectionPan() + audEngineUtil::GetRandomNumberInRange(-45.0f, 45.0f);		
							m_FakeRadioEmitterEndPan = m_FakeRadioEmitterStartPan;		
						}

						const Vec3V panVector = g_AudioEngine.GetEnvironment().ComputePositionFromPan(m_FakeRadioEmitterStartPan);
						const Vector3 startPos = VEC3V_TO_VECTOR3(listPos + (panVector * ScalarV(m_FakeRadioEmitterDistance)));
						m_FakeRadioEmitter.SetPosition(startPos);

						bool useOcclusionGroup = false;
						
						if(m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_VEHICLE)
						{
							useOcclusionGroup = AUD_GET_TRISTATE_VALUE(radioEmitterSettings->Flags, FLAG_ID_RANDOMISEDRADIOEMITTERSETTINGS_USEOCCLUSIONONVEHICLEEMITTERS) == AUD_TRISTATE_TRUE;
						}
						else
						{
							useOcclusionGroup = AUD_GET_TRISTATE_VALUE(radioEmitterSettings->Flags, FLAG_ID_RANDOMISEDRADIOEMITTERSETTINGS_USEOCCLUSIONONSTATICEMITTERS) == AUD_TRISTATE_TRUE;
						}
						
						if(useOcclusionGroup)
						{
							naEnvironmentGroup *occlusionGroup = audEmitterAudioEntity::CreateStaticEmitterEnvironmentGroup(m_AmbientRadioSettings, startPos);

							if(occlusionGroup)
							{
								m_FakeRadioEmitter.SetOcclusionGroup((naEnvironmentGroup *)occlusionGroup);
								occlusionGroup->AddSoundReference();
								occlusionGroup->SetInteriorInfoWithInteriorInstance(NULL, INTLOC_ROOMINDEX_LIMBO);
								((naEnvironmentGroup *)occlusionGroup)->SetSourceEnvironmentUpdates(0, 0, 0.0f);
							}
						}

						if(m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_VEHICLE)
						{
							m_FakeRadioEmitterAttackTime = audEngineUtil::GetRandomNumberInRange((s32)radioEmitterSettings->VehicleEmitterConfig.MinAttackTime, (s32)radioEmitterSettings->VehicleEmitterConfig.MaxAttackTime)/1000.0f;
							m_FakeRadioEmitterReleaseTime = audEngineUtil::GetRandomNumberInRange((s32)radioEmitterSettings->VehicleEmitterConfig.MinReleaseTime, (s32)radioEmitterSettings->VehicleEmitterConfig.MaxReleaseTime)/1000.0f;
							m_FakeRadioEmitterHoldTime = audEngineUtil::GetRandomNumberInRange((s32)radioEmitterSettings->VehicleEmitterConfig.MinHoldTime, (s32)radioEmitterSettings->VehicleEmitterConfig.MaxHoldTime)/1000.0f;
							m_FakeRadioEmitterLifeTime = m_FakeRadioEmitterAttackTime + m_FakeRadioEmitterHoldTime + m_FakeRadioEmitterReleaseTime;
							m_FakeRadioEmitterListenerSpawnPoint = listPos;
							m_FakeRadioEmitterRadius = 0.0f;
						}
						else
						{
							m_FakeRadioEmitterHoldTime = -1.0f;
							m_FakeRadioEmitterLifeTime = -1.0f;
							m_FakeRadioEmitterListenerSpawnPoint = pedPos;
							m_FakeRadioEmitterListenerSpawnPoint.SetZ(0.0f);
							m_FakeRadioEmitterRadius = audEngineUtil::GetRandomNumberInRange(radioEmitterSettings->StaticEmitterConfig.MinFadeRadius, radioEmitterSettings->StaticEmitterConfig.MaxFadeRadius);
						}

						m_FakeRadioEmitterState = AMBIENT_RADIO_FADE_IN;
						m_FakeRadioEmitterVolLin = 0.0f;
						m_FakeRadioEmitterTimeActive = 0.0f;
					}
				}
			}
		}
		break;

	case AMBIENT_RADIO_FADE_IN:
		{
			if(m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_VEHICLE)
			{
				m_FakeRadioEmitterVolLin = Min(m_FakeRadioEmitterVolLin + (fwTimer::GetTimeStep() * (1.0f/m_FakeRadioEmitterAttackTime)), 1.0f);
			}
			else
			{
				Vec3V currentSpawnPoint = pedPos;
				currentSpawnPoint.SetZf(0.0f);
				f32 distanceTravelled = Dist(currentSpawnPoint, m_FakeRadioEmitterListenerSpawnPoint).Getf();
				m_FakeRadioEmitterListenerSpawnPoint = currentSpawnPoint;
				m_FakeRadioEmitterVolLin = Min(m_FakeRadioEmitterVolLin + (distanceTravelled/m_FakeRadioEmitterRadius), 1.0f);
			}

			if(!isValidToPlay)
			{
				m_FakeRadioEmitterState = AMBIENT_RADIO_FADE_OUT;
			}
			else if(m_FakeRadioEmitterVolLin >= 1.0f)
			{
				m_FakeRadioEmitterState = AMBIENT_RADIO_ON;
			}
		}
		break;

	case AMBIENT_RADIO_ON:
		{
			if(!isValidToPlay)
			{
				m_FakeRadioEmitterState = AMBIENT_RADIO_FADE_OUT;
			}
			else if(m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_STATIC)
			{
				m_FakeRadioEmitterState = AMBIENT_RADIO_FADE_OUT;
			}
			else
			{
				if(m_FakeRadioEmitterTimeActive >= m_FakeRadioEmitterLifeTime - m_FakeRadioEmitterReleaseTime)
				{
					m_FakeRadioEmitterState = AMBIENT_RADIO_FADE_OUT;
				}
			}
		}
		break;

	case AMBIENT_RADIO_FADE_OUT:
		{
			if(inInterior || isScorePlaying)
			{
				m_FakeRadioEmitterVolLin = Max(m_FakeRadioEmitterVolLin - (fwTimer::GetTimeStep() * 0.5f), 0.0f);
			}
			else if(m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_VEHICLE)
			{
				m_FakeRadioEmitterVolLin = Max(m_FakeRadioEmitterVolLin - (fwTimer::GetTimeStep() * (1.0f/m_FakeRadioEmitterReleaseTime)), 0.0f);
			}
			else
			{
				Vec3V currentSpawnPoint = pedPos;
				currentSpawnPoint.SetZf(0.0f);
				f32 distanceTravelled = Dist(currentSpawnPoint, m_FakeRadioEmitterListenerSpawnPoint).Getf();
				m_FakeRadioEmitterListenerSpawnPoint = currentSpawnPoint;
				m_FakeRadioEmitterVolLin = Max(m_FakeRadioEmitterVolLin - (distanceTravelled/m_FakeRadioEmitterRadius), 0.0f);
			}

			if(m_FakeRadioEmitterVolLin <= 0.0f)
			{
				g_RadioAudioEntity.StopStaticRadio(&m_FakeRadioEmitter);

				naEnvironmentGroup *occlusionGroup = m_FakeRadioEmitter.GetOcclusionGroup();
				if(occlusionGroup)
				{
					occlusionGroup->RemoveSoundReference();
					m_FakeRadioEmitter.SetOcclusionGroup(NULL);
				}

				m_FakeRadioEmitterRadioStation = g_OffRadioStation;
				m_AmbientRadioSettings = NULL;
				m_FakeRadioEmitterState = AMBIENT_RADIO_OFF;

				if(radioEmitterSettings)
				{
					u32 randomReTriggerTime = m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_VEHICLE? audEngineUtil::GetRandomNumberInRange((s32)radioEmitterSettings->VehicleEmitterConfig.RetriggerTimeMin, (s32)radioEmitterSettings->VehicleEmitterConfig.RetriggerTimeMax) : 
																									audEngineUtil::GetRandomNumberInRange((s32)radioEmitterSettings->StaticEmitterConfig.RetriggerTimeMin, (s32)radioEmitterSettings->StaticEmitterConfig.RetriggerTimeMax);

					if(m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_VEHICLE)
					{
						m_NextFakeVehicleRadioTriggerTime = timeInMs + randomReTriggerTime;
					}
					else
					{
						m_NextFakeStaticRadioTriggerTime = timeInMs + randomReTriggerTime;
					}
				}
			}
		}
		break;
	}

	if(m_FakeRadioEmitterState != AMBIENT_RADIO_OFF)
	{
		m_FakeRadioEmitterTimeActive += fwTimer::GetTimeStep();

		if(m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_VEHICLE)
		{
			const f32 currentPan = m_FakeRadioEmitterStartPan + ((m_FakeRadioEmitterEndPan - m_FakeRadioEmitterStartPan) * (m_FakeRadioEmitterTimeActive/m_FakeRadioEmitterLifeTime));
			const Vec3V panVector = g_AudioEngine.GetEnvironment().ComputePositionFromPan(currentPan);
			const Vector3 finalPos = VEC3V_TO_VECTOR3(m_FakeRadioEmitterListenerSpawnPoint + (panVector * ScalarV(m_FakeRadioEmitterDistance)));
			m_FakeRadioEmitter.SetPosition(finalPos);
		}

		if(m_AmbientRadioSettings)
		{
			f32 exteriorOcclusionAtListener = audNorthAudioEngine::GetOcclusionManager()->GetOutsideWorldOcclusionAfterPortals();
			m_FakeRadioEmitter.SetEmittedVolume(audDriverUtil::ComputeDbVolumeFromLinear(Clamp(m_FakeRadioEmitterVolLin * (1.0f - exteriorOcclusionAtListener), 0.f, 1.f) * audDriverUtil::ComputeLinearVolumeFromDb(m_AmbientRadioSettings->EmittedVolume * 0.01f)));
			m_FakeRadioEmitter.SetRolloffFactor(m_AmbientRadioSettings->RolloffFactor * 0.01f);
			m_FakeRadioEmitter.SetLPFCutoff((u32)m_AmbientRadioSettings->FilterCutoff);
			m_FakeRadioEmitter.SetHPFCutoff(m_AmbientRadioSettings->HPFCutoff);
		}
	}

#if __BANK
	if(g_DebugDrawAmbientRadioEmitter)
	{
		char buf[128];
		f32 xPos = 0.05f;
		f32 yPos = 0.05f;

		if(m_AmbientRadioSettings)
		{
			formatf(buf, sizeof(buf), "Radio Emitter Settings: %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_AmbientRadioSettings->NameTableOffset));
		}
		else if(radioEmitterSettings)
		{
			formatf(buf, sizeof(buf), "Radio Emitter Settings: %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(radioEmitterSettings->NameTableOffset));
		}
		else
		{
			formatf(buf, sizeof(buf), "Radio Emitter Settings: NULL");
		}
		
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.02f;

		if(m_FakeRadioEmitterState == AMBIENT_RADIO_OFF && !radioEmitterSettings)
		{
			formatf(buf, sizeof(buf), "Next Static Trigger Time: N/A");
			grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
			yPos += 0.02f;

			formatf(buf, sizeof(buf), "Next Vehicle Trigger Time: N/A");
			grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
			yPos += 0.02f;
		}
		else
		{
			s32 nextStaticTriggerTimeMS = ((s32)m_NextFakeStaticRadioTriggerTime - (s32)timeInMs);

			if(radioEmitterSettings && (radioEmitterSettings->VehicleEmitterBias >= 1.0f || !audAmbientZone::IsValidTime(radioEmitterSettings->StaticEmitterConfig.MinTime, radioEmitterSettings->StaticEmitterConfig.MaxTime, gameTime)))
			{
				formatf(buf, sizeof(buf), "Next Static Trigger Time: N/A");
			}
			else
			{
				formatf(buf, sizeof(buf), "Next Static Trigger Time: %.02f", Max(nextStaticTriggerTimeMS, 0) / 1000.0f);
			}

			grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
			yPos += 0.02f;

			s32 nextVehicleTriggerTimeMS = ((s32)m_NextFakeVehicleRadioTriggerTime - (s32)timeInMs);

			if(radioEmitterSettings && (radioEmitterSettings->VehicleEmitterBias <= 0.0f || !audAmbientZone::IsValidTime(radioEmitterSettings->VehicleEmitterConfig.MinTime, radioEmitterSettings->VehicleEmitterConfig.MaxTime, gameTime)))
			{
				formatf(buf, sizeof(buf), "Next Vehicle Trigger Time: N/A");
			}
			else
			{
				formatf(buf, sizeof(buf), "Next Vehicle Trigger Time: %.02f", Max(nextVehicleTriggerTimeMS, 0) / 1000.0f);
			}

			grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
			yPos += 0.02f;
		}

		formatf(buf, sizeof(buf), "Ped Pan: %.02f", CalculateCurrentPedDirectionPan());
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.02f;

		f32 maxBlockedFactor = 0.0f;

		for(u32 direction = 0; direction < AUD_AMB_DIR_LOCAL; direction++)
		{
			maxBlockedFactor = Max(maxBlockedFactor, audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection((audAmbienceDirection)direction));
		}

		formatf(buf, sizeof(buf), "Max Blocked Factor: %.02f", maxBlockedFactor);
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.02f;

		formatf(buf, sizeof(buf), "Static Emitters Valid: %s", radioEmitterSettings && radioEmitterSettings->VehicleEmitterBias < 1.0f && audAmbientZone::IsValidTime(radioEmitterSettings->StaticEmitterConfig.MinTime, radioEmitterSettings->StaticEmitterConfig.MaxTime, gameTime)? "True" : "False");
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.02f;

		formatf(buf, sizeof(buf), "Vehicle Emitters Valid: %s", radioEmitterSettings && radioEmitterSettings->VehicleEmitterBias > 0.0f && audAmbientZone::IsValidTime(radioEmitterSettings->VehicleEmitterConfig.MinTime, radioEmitterSettings->VehicleEmitterConfig.MaxTime, gameTime)? "True" : "False");
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.02f;

		const char *stateNames[] = { "Off", "Fading In", "On", "Fading Out" };
		formatf(buf, sizeof(buf), "Radio State: %s", stateNames[m_FakeRadioEmitterState]);
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.02f;

		const char *typeNames[] = { "Static", "Vehicle" };
		formatf(buf, sizeof(buf), "Radio Type: %s", typeNames[m_FakeRadioEmitterType]);
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.02f;

		formatf(buf, sizeof(buf), "Volume Linear: %.02f", m_FakeRadioEmitterVolLin);
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.02f;

		const char* currentRadioStationName = "None";
		if(m_FakeRadioEmitterState != AMBIENT_RADIO_OFF)
		{
			for(u32 i = 0; i < audRadioStation::GetNumUnlockedStations(); i++)
			{
				const audRadioStation *station = audRadioStation::GetStation(i);

				if(station->GetNameHash() == m_FakeRadioEmitterRadioStationHash)
				{
					currentRadioStationName = station->GetName();
					break;
				}
			}
		}

		formatf(buf, sizeof(buf), "Current Station: %s", currentRadioStationName);
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.04f;

		formatf(buf, sizeof(buf), "Valid Stations:");
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.02f;

		CalculateValidAmbientRadioStations();

		for(s32 loop = 0; loop < m_ValidRadioStations.GetCount(); loop++)
		{
			formatf(buf, sizeof(buf), m_ValidRadioStations[loop]->GetName());
			grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
			yPos += 0.02f;
		}

		if(m_FakeRadioEmitterState != AMBIENT_RADIO_OFF)
		{
			Vector3 currentRadioPosition;
			m_FakeRadioEmitter.GetPosition(currentRadioPosition);
			Color32 color = m_FakeRadioEmitterType == AMBIENT_RADIO_TYPE_VEHICLE? Color32(0.0f, 1.0f, 0.0f, m_FakeRadioEmitterVolLin) : Color32(1.0f * (1.0f - m_FakeRadioEmitterVolLin), 1.0f * m_FakeRadioEmitterVolLin, 0.0f, 1.0f);
			CPed* localPlayerPed = FindPlayerPed();

			if(localPlayerPed)
			{
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()), currentRadioPosition, color, color);
			}

			grcDebugDraw::Sphere(currentRadioPosition, 1.0f, color);
		}
	}
#endif

	m_FakeRadioTimeLastFrame = timeInMs;
}

// ----------------------------------------------------------------
// Calculate which radio stations are valid to play
// ----------------------------------------------------------------
void audAmbientAudioEntity::CalculateValidAmbientRadioStations()
{
	m_ValidRadioStations.Reset();

	// Choose a random music radio station that is already getting streamed physically
	for(u32 i = 0; i < audRadioStation::GetNumUnlockedStations() && !m_ValidRadioStations.IsFull(); i++)
	{
		const audRadioStation *station = audRadioStation::GetStation(i);

		if(station && !station->IsTalkStation() && !station->IsHidden() && !station->isDjSpeaking() && station->IsStreamingPhysically())
		{
			m_ValidRadioStations.Push(station);
		}
	}

	if(m_ValidRadioStations.GetCount() > 0)
	{
		if(m_ValidRadioStations.GetCount() > 1)
		{
			// If we've got multiple stations available, avoid choosing the same one twice
			for(s32 loop = 0; loop < m_ValidRadioStations.GetCount(); loop++)
			{
				if(m_ValidRadioStations[loop]->GetNameHash() == m_FakeRadioEmitterRadioStationHash)
				{
					m_ValidRadioStations.Delete(loop);
					break;
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Calculate the pan angle of the listener
// ----------------------------------------------------------------
f32 audAmbientAudioEntity::CalculateCurrentPedDirectionPan()
{
	CPed* playerPed = FindPlayerPed();
	
	if(playerPed)
	{
		const Vec3V north = Vec3V(0.0f, 1.0f, 0.0f);
		const Vec3V east = Vec3V(1.0f,0.0f,0.0f);
		const Vec3V right = NormalizeFast(playerPed->GetMatrix().a());
		const ScalarV cosangle = Dot(right, north);
		const ScalarV angle = Arccos(Clamp(cosangle, ScalarV(V_NEGONE), ScalarV(V_ONE)));
		const ScalarV degrees = angle * ScalarV(V_TO_DEGREES);
		ScalarV actualDegrees;

		if(IsTrue(IsLessThanOrEqual(Dot(right, east), ScalarV(V_ZERO))))
		{
			actualDegrees = ScalarV(360.0f) - degrees;
		}
		else
		{
			actualDegrees = degrees;
		}

		return actualDegrees.Getf() - 90;
	}

	return 0.0f;
}

#if __BANK
// ----------------------------------------------------------------
// Debug draw the walla speech system
// ----------------------------------------------------------------
void audAmbientAudioEntity::DebugDrawWallaSpeech()
{
	if(m_WallaSpeechSound)
	{
		const Vec3V soundPosition = m_WallaSpeechSound->GetRequestedPosition();
		grcDebugDraw::Line(FindPlayerPed()->GetTransform().GetPosition(), soundPosition, Color32(0, 255, 0), Color32(0, 255, 0));
		grcDebugDraw::Sphere(soundPosition, 1.0f, Color32(0, 255, 0));
	}

	const u32 numGroups = CPopCycle::GetPopGroups().GetPedCount();
	u32 validGroups = 0;

	for(u32 group = 0; group < numGroups; group++)
	{
		if(CPopCycle::GetCurrentPedGroupPercentage(group) > 0)
		{   
			validGroups++;
		}
	}

	char tempString[128];
	f32* smoothedLoudnessPtr = g_AudioEngine.GetSoundManager().GetVariableAddress(ATSTRINGHASH("Game.Audio.RMSLevels.SFXSmoothed", 0xEEE20C63));

	formatf(tempString, "Fraction Male: %.02f", audNorthAudioEngine::GetGtaEnvironment()->GetPedFractionMale());
	grcDebugDraw::Text(Vector2(0.5f, 0.06f), Color32(255,255,255), tempString);

	formatf(tempString, "Last Trigger Min Density: %.02f", g_WallaSpeechTriggerMinDensity);
	grcDebugDraw::Text(Vector2(0.5f, 0.08f), Color32(255,255,255), tempString);

	formatf(tempString, "Trigger Time: %.02f", m_WallaSpeechTimer);
	grcDebugDraw::Text(Vector2(0.5f, 0.1f), Color32(255,255,255), tempString);

	if(smoothedLoudnessPtr)
	{
		formatf(tempString, "Smoothed RMS Loudness: %.02f", audDriverUtil::ComputeDbVolumeFromLinear(m_RMSLevelSmoother.GetLastValue()));
		grcDebugDraw::Text(Vector2(0.5f, 0.12f), Color32(255,255,255), tempString);
	}

	if(m_WallaSpeechSound)
	{
		formatf(tempString, "Voice Volume: %.02f", m_WallaSpeechSound->GetRequestedPostSubmixVolumeAttenuation());
		grcDebugDraw::Text(Vector2(0.5f, 0.14f), Color32(255,255,255), tempString);
	}

	if(g_WallaSpeechSettingsListName)
	{
		formatf(tempString, "List: %s", g_WallaSpeechSettingsListName);
		grcDebugDraw::Text(Vector2(0.7f, 0.06f), Color32(255,255,255), tempString);
	}

	if(g_WallaSpeechSettingsName)
	{
		formatf(tempString, "Settings: %s", g_WallaSpeechSettingsName);
		grcDebugDraw::Text(Vector2(0.7f, 0.08f), Color32(255,255,255), tempString);

		formatf(tempString, "Context: %s", g_WallaSpeechSettingsContext);
		grcDebugDraw::Text(Vector2(0.7f, 0.1f), Color32(255,255,255), tempString);
	}
	
	f32 xCoord = 0.1f;
	f32 yCoord = 0.1f - g_DrawPedWallaSpeechScroll;

	formatf(tempString, "Valid Groups: %d/%d", validGroups, numGroups);
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
	yCoord += 0.02f;

	formatf(tempString, "Valid Voices: %d", m_ValidWallaVoices.GetCount());
	grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
	yCoord += 0.04f;

	for(u32 loop = 0; loop < m_ValidWallaVoices.GetCount(); loop++)
	{
		formatf(tempString, "%s", m_ValidWallaVoices[loop].pedVoiceGroupName);
		grcDebugDraw::Text(Vector2(xCoord, yCoord), m_WallaSpeechSound && m_ValidWallaVoices[loop].voiceNameHash == m_WallaSpeechSelectedVoice? Color32(0,255,0) : Color32(255,255,255), tempString );
		yCoord += 0.02f;
	}
}
#endif

// ----------------------------------------------------------------
// Update the walla system
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdateWalla()
{
#if __BANK
	if (g_DisplayInteriorWallaDebug)
	{
		DebugDrawWallaInterior();
	}
	else if(g_DisplayExteriorWallaDebug)
	{
		DebugDrawWallaExterior();
	}
	else if(g_DisplayHostileWallaDebug)
	{
		DebugDrawWallaHostile();
	}
#endif

	f32 totalExteriorDensity = 0;
	f32 totalHostileDensity = 0;
	f32 totalInteriorCount = 0;
	f32 totalExteriorFractionPanicing = 0.0f;

	if((fwTimer::GetSystemFrameCount() & 31) == PROCESS_EXTERIOR_WALLA_DENSITY)
	{
		CalculateActiveZonesMinMaxPedDensity(m_MinValidPedDensityExterior, m_MaxValidPedDensityExterior, m_PedDensityScalarExterior, false);
	}
	else if((fwTimer::GetSystemFrameCount() & 31) == PROCESS_INTERIOR_WALLA_DENSITY)
	{
		CalculateActiveZonesMinMaxPedDensity(m_MinValidPedDensityInterior, m_MaxValidPedDensityInterior, m_PedDensityScalarInterior, true);
	}

	for (s32 loop = 0; loop < 4; loop++)
	{
		f32 blockedFactor = g_BlockedToPedDensityScale.CalculateValue(audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorForDirection((audAmbienceDirection)loop));
		totalExteriorDensity += Clamp(GetExteriorPedDensityForDirection((audAmbienceDirection)loop) * blockedFactor * m_PedDensityScalarExterior, m_MinValidPedDensityExterior, m_MaxValidPedDensityExterior);
		totalHostileDensity += GetHostilePedDensityForDirection((audAmbienceDirection)loop);
			
		totalInteriorCount += audNorthAudioEngine::GetGtaEnvironment()->GetInteriorOccludedPedCountForDirection((audAmbienceDirection)loop);

		f32 exteriorPedCount = audNorthAudioEngine::GetGtaEnvironment()->GetExteriorPedCountForDirection((audAmbienceDirection)loop);

		if(exteriorPedCount > 0)
		{
			totalExteriorFractionPanicing += audNorthAudioEngine::GetGtaEnvironment()->GetExteriorPedPanicCountForDirection((audAmbienceDirection)loop)/exteriorPedCount;
		}
	}

	f32 averageExteriorDensity = totalExteriorDensity / 4;
	f32 averageHostileDensity = totalHostileDensity / 4;
	f32 averageInteriorDensity = Clamp((f32)m_NumPedToWallaInterior.CalculateValue(totalInteriorCount) * m_PedDensityScalarInterior, m_MinValidPedDensityInterior, m_MaxValidPedDensityInterior);

#if __BANK
	if(g_OverridePedDensity)
	{
		averageInteriorDensity = g_OverridenPedDensity;
	}
#endif

	m_AverageFractionPanicingExterior = totalExteriorFractionPanicing / 4;

	if(g_OverridePedPanic)
	{
		m_AverageFractionPanicingExterior = g_OverridenPedPanic;
	}

	f32 pedDensityPanicScalar = 1.0f;

	// If we're in the middle of panicing, we start scaling the density according to the post panic behaviour curve
	if(m_PanicAssetPrepareState == PANIC_ASSETS_PLAYING || m_PanicAssetPrepareState == PANIC_ASSETS_COOLDOWN)
	{
		pedDensityPanicScalar = m_PedWallaDensityPostPanic.CalculateValue(m_PedPanicTimer);
	}

	// Smoothly scale down to zero when we're switching between walla types so that we don't ever have multiple layers playing over the top of each other
	f32 exteriorWallaSwitchScalar = m_ExteriorWallaSwitchActive? 0.0f : 1.0f;
	f32 interiorWallaSwitchScalar = m_InteriorWallaSwitchActive? 0.0f : 1.0f;

	m_FinalPedDensityExterior = m_WallaSmootherExterior.CalculateValue(exteriorWallaSwitchScalar * pedDensityPanicScalar * ((m_ScriptDesiredPedDensityExterior * m_ScriptDesiredPedDensityExteriorApplyFactor) + (averageExteriorDensity * (1.0f - m_ScriptDesiredPedDensityExteriorApplyFactor))));
	m_FinalPedDensityInterior = m_WallaSmootherInterior.CalculateValue(interiorWallaSwitchScalar * pedDensityPanicScalar * ((m_ScriptDesiredPedDensityInterior * m_ScriptDesiredPedDensityInteriorApplyFactor) + (averageInteriorDensity * (1.0f - m_ScriptDesiredPedDensityInteriorApplyFactor))));
	m_FinalPedDensityHostile = m_WallaSmootherHostile.CalculateValue(averageHostileDensity);

	g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Ambience.PedDensity", 0x98ACE024), Max(m_FinalPedDensityExterior, m_FinalPedDensityInterior));
	g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Ambience.PedDensityHostile", 0x622CC27C), m_FinalPedDensityHostile);

	if(m_FinalPedDensityExterior > 0.0f && !g_MuteWalla)
	{
		if(m_ExteriorWallaSoundSet.IsInitialised())
		{
			if(m_DoesExteriorWallaRequireStreamSlot)
			{
				if(!m_ExteriorWallaStreamSlot)
				{
					audStreamClientSettings settings;
					settings.priority = audStreamSlot::STREAM_PRIORITY_STATIC_AMBIENT_EMITTER;
					settings.stopCallback = &OnExteriorWallaStopCallback;
					settings.hasStoppedCallback = &HasExteriorWallaStoppedCallback;
					settings.userData = 0;
					m_ExteriorWallaStreamSlot = audStreamSlot::AllocateSlot(&settings);
				}

				if(m_ExteriorWallaStreamSlot)
				{
					if(!m_ExteriorWallaSounds[WALLA_TYPE_STREAMED] && (m_ExteriorWallaStreamSlot->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE))
					{
						audSoundInitParams initParams;
						Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
						CreateSound_PersistentReference(m_ExteriorWallaSoundSet.Find(ATSTRINGHASH("Heavy", 0xB1AEBE06)), &m_ExteriorWallaSounds[WALLA_TYPE_STREAMED], &initParams);
					}

					if(m_ExteriorWallaSounds[WALLA_TYPE_STREAMED] && m_ExteriorWallaSounds[WALLA_TYPE_STREAMED]->GetPlayState() != AUD_SOUND_PLAYING)
					{
						if(m_ExteriorWallaSounds[WALLA_TYPE_STREAMED]->Prepare(m_ExteriorWallaStreamSlot->GetWaveSlot(), true) == AUD_PREPARED)
						{
							m_ExteriorWallaSounds[WALLA_TYPE_STREAMED]->Play();
						}
					}
				}
			}
			else
			{
				if(!m_ExteriorWallaSounds[WALLA_TYPE_HEAVY])
				{
					CreateAndPlaySound_Persistent(m_ExteriorWallaSoundSet.Find(ATSTRINGHASH("Heavy", 0xB1AEBE06)), &m_ExteriorWallaSounds[WALLA_TYPE_HEAVY]);
				}

				if(!m_ExteriorWallaSounds[WALLA_TYPE_MEDIUM])
				{
					CreateAndPlaySound_Persistent(m_ExteriorWallaSoundSet.Find(ATSTRINGHASH("Medium", 0x74E06FAC)), &m_ExteriorWallaSounds[WALLA_TYPE_MEDIUM]);
				}

				if(!m_ExteriorWallaSounds[WALLA_TYPE_SPARSE])
				{
					CreateAndPlaySound_Persistent(m_ExteriorWallaSoundSet.Find(ATSTRINGHASH("Sparse", 0x44DA1B65)), &m_ExteriorWallaSounds[WALLA_TYPE_SPARSE]);
				}
			}
		}
	}
	else
	{
		StopExteriorWallaSounds();
		m_ExteriorWallaSwitchActive = false;
	}

	if(m_FinalPedDensityInterior > 0.0f && !g_MuteWalla)
	{
		if(m_InteriorWallaSoundSet.IsInitialised())
		{
			if(m_DoesInteriorWallaRequireStreamSlot)
			{
				if(!m_InteriorWallaStreamSlot)
				{
					audStreamClientSettings settings;
					settings.priority = audStreamSlot::STREAM_PRIORITY_STATIC_AMBIENT_EMITTER;
					settings.stopCallback = &OnInteriorWallaStopCallback;
					settings.hasStoppedCallback = &HasInteriorWallaStoppedCallback;
					settings.userData = 0;
					m_InteriorWallaStreamSlot = audStreamSlot::AllocateSlot(&settings);
				}

				if(m_InteriorWallaStreamSlot)
				{
					if(!m_InteriorWallaSounds[WALLA_TYPE_STREAMED] && (m_InteriorWallaStreamSlot->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE))
					{
						audSoundInitParams initParams;
						initParams.WaveSlot = m_InteriorWallaStreamSlot->GetWaveSlot();
						initParams.AllowLoad = true;
						initParams.PrepareTimeLimit = 5000;
						Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
						CreateAndPlaySound_Persistent(m_InteriorWallaSoundSet.Find(ATSTRINGHASH("Heavy", 0xB1AEBE06)), &m_InteriorWallaSounds[WALLA_TYPE_STREAMED], &initParams);
					}
				}
			}
			else
			{
				if(!m_InteriorWallaSounds[WALLA_TYPE_HEAVY])
				{
					CreateAndPlaySound_Persistent(m_InteriorWallaSoundSet.Find(ATSTRINGHASH("Heavy", 0xB1AEBE06)), &m_InteriorWallaSounds[WALLA_TYPE_HEAVY]);
				}

				if(!m_InteriorWallaSounds[WALLA_TYPE_MEDIUM])
				{
					CreateAndPlaySound_Persistent(m_InteriorWallaSoundSet.Find(ATSTRINGHASH("Medium", 0x74E06FAC)), &m_InteriorWallaSounds[WALLA_TYPE_MEDIUM]);
				}

				if(!m_InteriorWallaSounds[WALLA_TYPE_SPARSE])
				{
					CreateAndPlaySound_Persistent(m_InteriorWallaSoundSet.Find(ATSTRINGHASH("Sparse", 0x44DA1B65)), &m_InteriorWallaSounds[WALLA_TYPE_SPARSE]);
				}
			}
		}
	}
	else
	{
		StopInteriorWallaSounds();
		m_InteriorWallaSwitchActive = false;
	}

	bool exteriorWallaPlaying = false;
	bool interiorWallaPlaying = false;

	static const float minCutoff = 300.0f;

	//Vector3 listPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
	f32 exteriorOcclusionAtListener = audNorthAudioEngine::GetOcclusionManager()->GetOutsideWorldOcclusionAfterPortals();
	const f32 interiorCutoff =  minCutoff + ((kVoiceFilterLPFMaxCutoff - minCutoff) * audNorthAudioEngine::GetOcclusionManager()->GetOutsideWorldOcclusion());
	const f32 exteriorCutoff =  minCutoff + ((kVoiceFilterLPFMaxCutoff - minCutoff) * (1.0f - exteriorOcclusionAtListener));

	for(u32 loop = 0; loop < WALLA_TYPE_MAX; loop++)
	{
		if(m_ExteriorWallaSounds[loop] && m_ExteriorWallaSounds[loop]->GetPlayState() == AUD_SOUND_PLAYING)
		{
			m_ExteriorWallaSounds[loop]->SetRequestedLPFCutoff(exteriorCutoff);
			m_ExteriorWallaSounds[loop]->FindAndSetVariableValue(ATSTRINGHASH("PED_DENSITY", 0x74DA213E), m_FinalPedDensityExterior);
			m_ExteriorWallaSounds[loop]->FindAndSetVariableValue(ATSTRINGHASH("PEDS_TO_LISTENER_HEIGHT_DIFF", 0x61C1908B), CalculatePedsToListenerHeightDiff());
			exteriorWallaPlaying = true;
		}

		if(m_InteriorWallaSounds[loop] && m_InteriorWallaSounds[loop]->GetPlayState() == AUD_SOUND_PLAYING)
		{
			m_InteriorWallaSounds[loop]->SetRequestedLPFCutoff(interiorCutoff);
			m_InteriorWallaSounds[loop]->FindAndSetVariableValue(ATSTRINGHASH("PED_DENSITY", 0x74DA213E), m_FinalPedDensityInterior);
			m_InteriorWallaSounds[loop]->FindAndSetVariableValue(ATSTRINGHASH("PEDS_TO_LISTENER_HEIGHT_DIFF", 0x61C1908B), CalculatePedsToListenerHeightDiff());
			interiorWallaPlaying = true;
		}
	}

	// Haven't started for some reason - reset the smoother so that we lerp in smoothly when we do
	if(!exteriorWallaPlaying)
	{
		m_WallaSmootherExterior.Reset();
		m_WallaSmootherExterior.CalculateValue(0.0f);
	}

	if(!interiorWallaPlaying)
	{
		m_WallaSmootherInterior.Reset();
		m_WallaSmootherInterior.CalculateValue(0.0f);
	}

	UpdatePanicWalla(m_AverageFractionPanicingExterior, m_FinalPedDensityExterior, &m_WallaSmootherExterior);
}

// ----------------------------------------------------------------
// Check if a given point is over any water
// ----------------------------------------------------------------
bool audAmbientAudioEntity::IsPointOverWater(const Vector3& pos)
{
	s32 smallestZoneIndex = -1;
	f32 smallestZoneArea = 0.0f;

	// Go through all the active zones
	for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
	{
		// Check if this zone is forcing a water decision, and if the listener is within the positioning zone
		if(m_ActiveZones[loop]->GetZoneData()->ZoneWaterCalculation != AMBIENT_ZONE_USE_SHORELINES &&
			m_ActiveZones[loop]->IsPointInPositioningZoneFlat(VECTOR3_TO_VEC3V(pos)))
		{
			const f32 zoneArea = m_ActiveZones[loop]->GetPositioningArea();

			// We use the decision made by the active zone with the smallest area, so update our index if this is the first zone or we've found a smaller one
			if(smallestZoneIndex == -1 || zoneArea < smallestZoneArea)
			{
				smallestZoneIndex = loop;
				smallestZoneArea = zoneArea;
			}
		}
	}

	if(smallestZoneIndex >= 0) // We have a zone that wants to make a decision, so defer to that
	{
		if(m_ActiveZones[smallestZoneIndex]->GetZoneData()->ZoneWaterCalculation == AMBIENT_ZONE_FORCE_OVER_LAND)
		{
			return false;
		}
		else if(m_ActiveZones[smallestZoneIndex]->GetZoneData()->ZoneWaterCalculation == AMBIENT_ZONE_FORCE_OVER_WATER)
		{
			return true;
		}
	}

	fwPtrListSingleLink shoreLineList;
	m_ShoreLinesQuadTree.GetAllMatchingNoDupes(pos, shoreLineList);
	fwPtrNode* ptrNode = shoreLineList.GetHeadPtr();

	while(ptrNode)
	{
		audShoreLine* shoreLine = static_cast<audShoreLine*>(ptrNode->GetPtr());

		if(shoreLine->GetActivationBoxAABB().IsInside(Vector2(pos.x, pos.y)))
		{
			if(shoreLine->IsPointOverWater(pos))
			{
				return true;
			}
		}

		ptrNode = ptrNode->GetNextPtr();
	}

	return false;
}

// ----------------------------------------------------------------
// Get all the shorelines intersecting the given aabb
// ----------------------------------------------------------------
void audAmbientAudioEntity::GetAllShorelinesIntersecting(const fwRect& bb, atArray<audShoreLine*>& shorelines)
{
	ShoreLineQuadTreeIntersectFunction shoreLineIntersectFn(&shorelines, bb);
	m_ShoreLinesQuadTree.ForAllMatching(bb, shoreLineIntersectFn);
}

// ----------------------------------------------------------------
// Force peds to panic
// ----------------------------------------------------------------
void audAmbientAudioEntity::ForcePedPanic()
{
	// Just need enough time to load in and start playing
	m_ForcePanic = 5.0f;
}

#if __BANK
// ----------------------------------------------------------------
// Test preparing an alarm
// ----------------------------------------------------------------
void audAmbientAudioEntity::TestPrepareAlarm()
{
	PrepareAlarm(g_TestAlarmName);
}

// ----------------------------------------------------------------
// Test starting an alarm
// ----------------------------------------------------------------
void audAmbientAudioEntity::TestStartAlarm()
{
	StartAlarm(g_TestAlarmName, g_TestAlarmSkipStartup);
}

// ----------------------------------------------------------------
// Test stopping an alarm
// ----------------------------------------------------------------
void audAmbientAudioEntity::TestStopAlarm()
{
	StopAlarm(g_TestAlarmName, g_TestAlarmStopImmediate);
}

// ----------------------------------------------------------------
// Test a streamed sound
// ----------------------------------------------------------------
void audAmbientAudioEntity::TestStreamedSound()
{
	audSoundInitParams initParams;
	initParams.Position = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
	bool triggered = TriggerStreamedAmbientOneShot(ATSTRINGHASH("PED_VOICE_NIGHT_OUT", 0xBC3DB395), &initParams); 
	audDisplayf("Streamed sound was %s", triggered? "successfully triggered" : "NOT triggered");
}
#endif

// ----------------------------------------------------------------
// Update the panic walla assets
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdatePanicWalla(f32 averageFractionPanicing, f32 pedDensity, audSimpleSmootherDiscrete* smoother)
{
	if(m_PanicAssetPrepareState == PANIC_ASSETS_REQUEST_SLOTS)
	{
		smoother->SetRates(g_PedDensitySmoothingIncrease, g_PedDensitySmoothingDecrease);
	}

	if(m_ForcePanic > 0.0f)
	{
		m_ForcePanic -= fwTimer::GetTimeStep();
	}

	if((averageFractionPanicing > 0.25f || m_ForcePanic > 0.0f) && pedDensity > 0.2f)
	{
		if(m_PanicAssetPrepareState == PANIC_ASSETS_REQUEST_SLOTS)
		{
			bool slotsReady = true;

			// We have two panic wave files that get played back to back with a sequential sound, so grab an ambient speech slot for each
			for(u32 loop = 0; loop < 2; loop++)
			{
				if(m_PanicWallaSpeechSlotIds[loop] == -1)
				{
					s32 speechSlotId = g_SpeechManager.GetAmbientSpeechSlot(NULL);

					if(speechSlotId >= 0)
					{
						audWaveSlot* speechSlot = g_SpeechManager.GetAmbientWaveSlotFromId(speechSlotId);

						if(speechSlot)
						{
							m_PanicWallaSpeechSlots[loop] = speechSlot;
							m_PanicWallaSpeechSlotIds[loop] = speechSlotId;
							g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(speechSlotId, 0, 
#if __BANK
								"PANIC_WALLA",
#endif
								0);
						}
						else
						{
							slotsReady = false;
						}
					}
					else
					{
						slotsReady = false;
					}
				}
			}

			// Slots have been loaded, so create the master random sound
			if(slotsReady)
			{
				Assertf(m_PanicWallaSound == NULL, "Expected panic walla to be null at this stage");
				audSoundInitParams initParams;
				initParams.AllowLoad = false;

				// PPU updated bucket required for sequential overlap sound splice mode to work on PS3
				Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());

				initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(m_PedDensityToPanicVol.CalculateValue(pedDensity));
				CreateSound_PersistentReference(ATSTRINGHASH("PANIC_WALLA_MASTER", 0x1AA85291), &m_PanicWallaSound, &initParams);

				// Based on the random variation that the random sound chose, we need to load in the appropriate wave files, so work out 
				// the names of those wave files now
				if(m_PanicWallaSound)
				{
					u32 lastRandomAssetSelected = ((audRandomizedSound*)m_PanicWallaSound)->GetSelectedVariationIndex();
					atString startString = atVarString("PANIC_WALLA_START_%02d", lastRandomAssetSelected + 1);
					atString endString = atVarString("PANIC_WALLA_END_%02d", lastRandomAssetSelected + 1);
					m_PanicWallaAssetHashes[0] = atStringHash(startString);
					m_PanicWallaAssetHashes[1] = atStringHash(endString);
					audDisplayf("Loading panic walla assets %s %s", startString.c_str(), endString.c_str());
					m_PanicAssetPrepareState = PANIC_ASSETS_REQUEST_LOAD;
				}
			}
			else
			{
				// Don't just sit on one slot if we can't get them both
				StopAndUnloadPanicAssets();
			}
		}

		if(m_PanicAssetPrepareState == PANIC_ASSETS_REQUEST_LOAD)
		{
			// Load in the required wave files
			for(u32 loop = 0; loop < 2; loop++)
			{
				if(m_PanicWallaSpeechSlots[loop])
				{
					m_PanicWallaSpeechSlots[loop]->LoadWave(m_PanicWallaBank, m_PanicWallaAssetHashes[loop]);
				}
			}

			m_PanicAssetPrepareState = PANIC_ASSETS_WAIT_FOR_LOAD;
		}

		if(m_PanicAssetPrepareState == PANIC_ASSETS_WAIT_FOR_LOAD)
		{
			bool isLoaded = true;

			for(u32 loop = 0; loop < 2; loop++)
			{
				audWaveSlot::audWaveSlotLoadStatus loadStatus = audWaveSlot::NOT_REQUESTED;

				if(m_PanicWallaSpeechSlots[loop])
				{
					loadStatus = m_PanicWallaSpeechSlots[loop]->GetWaveLoadingStatus(m_PanicWallaBank, m_PanicWallaAssetHashes[loop]);
				}

				if(loadStatus == audWaveSlot::LOADING)
				{
					isLoaded = false;
				}
				else if(loadStatus != audWaveSlot::LOADED)
				{
					StopAndUnloadPanicAssets();
					m_PanicAssetPrepareState = PANIC_ASSETS_REQUEST_SLOTS;
					isLoaded = false;
				}
			}

			if(isLoaded)
			{
				m_PanicAssetPrepareState = PANIC_ASSETS_PREPARE_AND_PLAY;
			}
		}

		if(m_PanicAssetPrepareState == PANIC_ASSETS_PREPARE_AND_PLAY)
		{
			if(m_PanicWallaSound)
			{
				m_PanicWallaSound->PrepareAndPlay();
				m_PanicAssetPrepareState = PANIC_ASSETS_PLAYING;
				smoother->SetRates(0.1f, 0.1f);
				m_PedPanicTimer = 0;
			}
		}
	}
	// Nobody is panicing, so reset the state machine
	else if(m_PanicAssetPrepareState < PANIC_ASSETS_PLAYING)
	{
		StopAndUnloadPanicAssets();
		m_PanicAssetPrepareState = PANIC_ASSETS_REQUEST_SLOTS;
	}

	if(m_PanicAssetPrepareState == PANIC_ASSETS_PLAYING || m_PanicAssetPrepareState == PANIC_ASSETS_COOLDOWN)
	{
		m_PedPanicTimer += fwTimer::GetTimeStep();

		if(m_PanicAssetPrepareState == PANIC_ASSETS_PLAYING)
		{
			// Sound has finished naturally, so free up the wave slots
			if(m_PanicWallaSound == NULL)
			{
				StopAndUnloadPanicAssets();
				m_PanicAssetPrepareState = PANIC_ASSETS_COOLDOWN;
			}
		}

		// Cooldown period has expired, so allow more panicing
		if(m_PanicAssetPrepareState == PANIC_ASSETS_COOLDOWN)
		{
			if(m_PedPanicTimer > g_PanicCooldownTime && averageFractionPanicing == 0.0f)
			{
				m_PanicAssetPrepareState = PANIC_ASSETS_REQUEST_SLOTS;
			}
		}
	}
}

// ----------------------------------------------------------------
// Stop and unload all the panic assets
// ----------------------------------------------------------------
void audAmbientAudioEntity::StopAndUnloadPanicAssets()
{
	StopAndForgetSounds(m_PanicWallaSound);

	for(u32 loop = 0; loop < 2; loop++)
	{
		if(m_PanicWallaSpeechSlotIds[loop] != -1)
		{
			g_SpeechManager.FreeAmbientSpeechSlot(m_PanicWallaSpeechSlotIds[loop], true);
			m_PanicWallaSpeechSlotIds[loop] = -1;
		}

		m_PanicWallaSpeechSlots[loop] = NULL;
	}
}

// ----------------------------------------------------------------
// Get the exterior ped density for a given direction
// ----------------------------------------------------------------
f32 audAmbientAudioEntity::GetExteriorPedDensityForDirection(audAmbienceDirection direction)
{
	f32 intensity = m_NumPedToWallaExterior.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetExteriorPedCountForDirection(direction));

	if (g_OverridePedDensity)
	{
		intensity = g_OverridenPedDensity;
	}

	return intensity;
}

// ----------------------------------------------------------------
// Get the hostile ped density for a given direction
// ----------------------------------------------------------------
f32 audAmbientAudioEntity::GetHostilePedDensityForDirection(audAmbienceDirection direction)
{
	f32 intensity = m_NumPedToWallaHostile.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetHostilePedCountForDirection(direction));

	if (g_OverridePedDensity)
	{
		intensity = g_OverridenPedDensity;
	}

	return intensity;
}

// ----------------------------------------------------------------
// Get the interior ped density for a given direction
// ----------------------------------------------------------------
f32 audAmbientAudioEntity::GetInteriorPedDensityForDirection(audAmbienceDirection direction)
{
	f32 intensity = m_NumPedToWallaInterior.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetInteriorOccludedPedCountForDirection(direction));

	if (g_OverridePedDensity)
	{
		intensity = g_OverridenPedDensity;
	}

	return intensity;
}

void audAmbientAudioEntity::LoadShorelines(u32 shorelineListName, u32 linkShoreline)
{ 
	BANK_ONLY(m_DebugShoreLine = NULL;);
	m_ShoreLinesQuadTree = fwQuadTreeNode(fwRect(WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_XMAX, WORLDLIMITS_REP_YMAX), QUAD_TREE_DEPTH);
	const s32 numChunks = (s32)audNorthAudioEngine::GetMetadataManager().GetNumChunks();
	for(u32 i = 0; i < numChunks; i++)
	{
		audMetadataObjectInfo info;
		if(audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(shorelineListName, i, info))
		{
			const ShoreLineList *list = info.GetObject<ShoreLineList>();
			if(list)
			{
				atArray<audShoreLineOcean* > oceanShorelines;
				atArray<u32> oceanShorelinesHashes;
				oceanShorelines.Reset();
				oceanShorelinesHashes.Reset();
				atArray<audShoreLineRiver* > riverShorelines;
				atArray<u32> riverShorelinesHashes;
				riverShorelines.Reset();
				riverShorelinesHashes.Reset();
				atArray<audShoreLineLake* > lakeShorelines;
				atArray<u32> lakeShorelinesHashes;
				lakeShorelines.Reset();
				lakeShorelinesHashes.Reset();
				for(u32 i = 0; i < list->numShoreLines; i++)
				{
					audMetadataObjectInfo info;
					if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(list->ShoreLines[i].ShoreLine,info))
					{
						audShoreLine* newShore = NULL;
						switch(info.GetType())
						{
						case ShoreLinePoolAudioSettings::TYPE_ID:
							newShore = rage_new audShoreLinePool(info.GetObject<ShoreLinePoolAudioSettings>());
							break;
						case ShoreLineRiverAudioSettings::TYPE_ID:
							newShore = rage_new audShoreLineRiver(info.GetObject<ShoreLineRiverAudioSettings>());
							riverShorelines.PushAndGrow(static_cast<audShoreLineRiver*>(newShore));
							riverShorelinesHashes.PushAndGrow(list->ShoreLines[i].ShoreLine);
							break;
						case ShoreLineOceanAudioSettings::TYPE_ID:
							newShore = rage_new audShoreLineOcean(info.GetObject<ShoreLineOceanAudioSettings>());
							oceanShorelines.PushAndGrow(static_cast<audShoreLineOcean*>(newShore));
							oceanShorelinesHashes.PushAndGrow(list->ShoreLines[i].ShoreLine);
							break;
						case ShoreLineLakeAudioSettings::TYPE_ID:
							newShore = rage_new audShoreLineLake(info.GetObject<ShoreLineLakeAudioSettings>());
							lakeShorelines.PushAndGrow(static_cast<audShoreLineLake*>(newShore));
							lakeShorelinesHashes.PushAndGrow(list->ShoreLines[i].ShoreLine);
							break;
						default:
							naAssertf(false,"Wrong shoreline type when loading.");		
							break;
						}
						if (newShore)
						{
							newShore->Load();
							m_ShoreLinesQuadTree.AddItemRestrictDupes((void*)newShore, fwRect(newShore->GetActivationBoxAABB()));
						}
					}
					else
					{
						naAssertf(false, "Invalid object info in audAmbientAudioEntity::LoadShorelines()");			
					}
				}
				naAssertf(oceanShorelines.GetCount() == oceanShorelinesHashes.GetCount(),"Something went wrong when trying to link the ocean shorelines, please contact the audio team.");
				naAssertf(riverShorelines.GetCount() == riverShorelinesHashes.GetCount(),"Something went wrong when trying to link the river shorelines, please contact the audio team.");
				naAssertf(lakeShorelines.GetCount() == lakeShorelinesHashes.GetCount(),"Something went wrong when trying to link the river shorelines, please contact the audio team.");
				// Link ocean shorelines to each other. 
				for(u32 i = 0; i < oceanShorelines.GetCount(); i ++)
				{
					u32 nextOceanShorelineHash = oceanShorelines[i]->GetOceanSettings()->NextShoreline;
					if( nextOceanShorelineHash == 0 && oceanShorelines.GetCount() > 1)
					{
						nextOceanShorelineHash = linkShoreline;
					}
					for(u32 j = 0; j < oceanShorelinesHashes.GetCount(); j ++)
					{
						if(oceanShorelinesHashes[j] == nextOceanShorelineHash)
						{
							naAssertf(i != j,"Trying to link an ocean shoreline with itself");
							oceanShorelines[i]->SetNextOceanReference(oceanShorelines[j]);
							oceanShorelines[j]->SetPrevOceanReference(oceanShorelines[i]);
						}
					}
				}
				// Link river shorelines to each other. 
				for(u32 i = 0; i < riverShorelines.GetCount(); i ++)
				{
					u32 nextRiverShorelineHash = riverShorelines[i]->GetRiverSettings()->NextShoreline;
					for(u32 j = 0; j < riverShorelinesHashes.GetCount(); j ++)
					{
						if(riverShorelinesHashes[j] == nextRiverShorelineHash)
						{
							naAssertf(i != j,"Trying to link an river shoreline with itself");
							riverShorelines[i]->SetNextRiverReference(riverShorelines[j]);
							riverShorelines[j]->SetPrevRiverReference(riverShorelines[i]);
						}
					}
				}
				// Link lake shorelines to each other. 
				for(u32 i = 0; i < lakeShorelines.GetCount(); i ++)
				{
					u32 nextLakeShorelineHash = lakeShorelines[i]->GetLakeSettings()->NextShoreline;
					for(u32 j = 0; j < lakeShorelinesHashes.GetCount(); j ++)
					{
						if(lakeShorelinesHashes[j] == nextLakeShorelineHash)
						{
							naAssertf(i != j,"Trying to link an river shoreline with itself");
							lakeShorelines[i]->SetNextLakeReference(lakeShorelines[j]);
							lakeShorelines[j]->SetPrevLakeReference(lakeShorelines[i]);
						}
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Check if rule streaming is enabled
// ----------------------------------------------------------------
bool audAmbientAudioEntity::IsRuleStreamingAllowed() const
{
	return *m_PlayerSpeedVariable < 15.0f BANK_ONLY(&& !g_ForceDisableStreaming); 
}

// ----------------------------------------------------------------
// Add core zones
// ----------------------------------------------------------------
void audAmbientAudioEntity::AddCoreZones()
{
	audMetadataObjectInfo info;

	if(audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(ATSTRINGHASH("AMBIENT_ZONE_LIST", 0x08b802028), 0, info))
	{
		const AmbientZoneList *list = info.GetObject<AmbientZoneList>();
		if(list)
		{
			m_Zones.Reserve(list->numZones);

			for(u32 i = 0; i < list->numZones; i++)
			{
				AmbientZone *zone = audNorthAudioEngine::GetObject<AmbientZone>(list->Zone[i].Ref);
				naAssertf(zone, "Invalid Ambient zone in audAmbientAudioEntity::Init()");	

				if(zone)
				{
					audAmbientZone* newZone = rage_new audAmbientZone();
					newZone->Init(zone, list->Zone[i].Ref, false);
					newZone->CalculateDynamicBankUsage();
					m_ZoneQuadTree.AddItemRestrictDupes((void*)newZone, newZone->GetActivationZoneAABB());
					m_Zones.Push(newZone);

					if(list->Zone[i].Ref == ATSTRINGHASH("AZ_LS_SIRENS_CITY", 0xE2EE1901))
					{
						sm_CityZone = newZone;
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Destroy all zones
// ----------------------------------------------------------------
void audAmbientAudioEntity::DestroyAllZonesAndShorelines()
{
	DestroyUnreferencedDLCZones(true);
	m_DirectionalAmbienceManagers.Reset();

	for(u32 zoneIndex = 0; zoneIndex < m_Zones.GetCount(); zoneIndex++)
	{
		m_Zones[zoneIndex]->SetActive(false);
	}

	for(s32 loop = 0; loop < m_Zones.GetCount(); loop++)
	{
		delete m_Zones[loop];
	}

	m_Zones.Reset();
	m_ActiveZones.Reset();
	m_ActiveEnvironmentZones.Reset();
	m_DLCZones.Reset();
	m_DLCShores.Reset();
	m_LoadedDLCChunks.Reset();
	m_LoadedShorelinesDLCChunks.Reset();
	m_ActiveShoreLines.Reset();
	m_ZoneQuadTree.DeleteAllItems();
	m_ZoneQuadTree.DestroyTree();
	m_ZoneQuadTree = fwQuadTreeNode(fwRect(WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_XMAX, WORLDLIMITS_REP_YMAX), QUAD_TREE_DEPTH);

	fwPtrListSingleLink shoreLineList;
	m_ShoreLinesQuadTree.GetAllNoDupes(shoreLineList);
	fwPtrNode* ptrNode = shoreLineList.GetHeadPtr();

	while(ptrNode)
	{
		audShoreLine* shoreLine = static_cast<audShoreLine*>(ptrNode->GetPtr());
		delete shoreLine;
		ptrNode = ptrNode->GetNextPtr();
	}

	m_ShoreLinesQuadTree.DeleteAllItems();
	m_ShoreLinesQuadTree.DestroyTree();
	m_ShoreLinesQuadTree = fwQuadTreeNode(fwRect(WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_XMAX, WORLDLIMITS_REP_YMAX), QUAD_TREE_DEPTH);
}

// ----------------------------------------------------------------
// Load any new DLC zones
// ----------------------------------------------------------------
void audAmbientAudioEntity::LoadNewDLCZones(const char* zoneListName, const char* bankMapName)
{
	const s32 numChunks = (s32)audNorthAudioEngine::GetMetadataManager().GetNumChunks();

	// Check for zones in newly loaded chunks. Start from chunk 1 - chunk 0 is the core game data, chunk 1+ is DLC
	for(u32 chunkIndex = 1; chunkIndex < numChunks; chunkIndex++)
	{
		const audMetadataChunk& chunk = audNorthAudioEngine::GetMetadataManager().GetChunk(chunkIndex);

		if(chunk.enabled && m_LoadedDLCChunks.Find(chunk.chunkNameHash) == -1)
		{
			audMetadataObjectInfo info;

			if(audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(atHashValue(bankMapName), chunkIndex, info))
			{
				const AmbientBankMap *bankMap = info.GetObject<AmbientBankMap>();

				if(bankMap)
				{					
					audDLCBankMap dlcBankMap;
					dlcBankMap.chunkNameHash = chunk.chunkNameHash;
					dlcBankMap.bankMap = bankMap;
					audAmbientZone::GetDLCBankMapList().PushAndGrow(dlcBankMap);
				}
			}

			if(audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(atHashValue(zoneListName), chunkIndex, info))
			{			
				const AmbientZoneList *list = info.GetObject<AmbientZoneList>();
				if(list)
				{
					for(u32 i = 0; i < list->numZones; i++)
					{
						AmbientZone *zone = audNorthAudioEngine::GetObject<AmbientZone>(list->Zone[i].Ref);

						if(zone)
						{
							audAmbientZone* newZone = rage_new audAmbientZone();
							newZone->Init(zone, list->Zone[i].Ref, false);
							newZone->CalculateDynamicBankUsage();
							m_ZoneQuadTree.AddItemRestrictDupes((void*)newZone, newZone->GetActivationZoneAABB());
							m_Zones.PushAndGrow(newZone);

							audDLCZone dlcZone;
							dlcZone.chunkNameHash = chunk.chunkNameHash;
							dlcZone.zone = newZone;
							m_DLCZones.PushAndGrow(dlcZone);
						}
					}
				}
			}

			m_LoadedDLCChunks.PushAndGrow(chunk.chunkNameHash);
		}		
	}
}

// ----------------------------------------------------------------
// Load any new DLC zones
// ----------------------------------------------------------------
void audAmbientAudioEntity::LoadNewDLCShorelines(u32 shorelineListName, u32 linkShoreline)
{
	const s32 numChunks = (s32)audNorthAudioEngine::GetMetadataManager().GetNumChunks();

	// Check for shorelines in newly loaded chunks. Start from chunk 1 - chunk 0 is the core game data, chunk 1+ is DLC
	for(u32 chunkIndex = 1; chunkIndex < numChunks; chunkIndex++)
	{
		const audMetadataChunk& chunk = audNorthAudioEngine::GetMetadataManager().GetChunk(chunkIndex);

		if(chunk.enabled && m_LoadedShorelinesDLCChunks.Find(chunk.chunkNameHash) == -1)
		{
			audMetadataObjectInfo info;
			if(audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(shorelineListName, chunkIndex, info))
			{
				const ShoreLineList *list = info.GetObject<ShoreLineList>();
				if(list)
				{
					atArray<audShoreLineOcean* > oceanShorelines;
					atArray<u32> oceanShorelinesHashes;
					oceanShorelines.Reset();
					oceanShorelinesHashes.Reset();
					atArray<audShoreLineRiver* > riverShorelines;
					atArray<u32> riverShorelinesHashes;
					riverShorelines.Reset();
					riverShorelinesHashes.Reset();
					atArray<audShoreLineLake* > lakeShorelines;
					atArray<u32> lakeShorelinesHashes;
					lakeShorelines.Reset();
					lakeShorelinesHashes.Reset();
					for(u32 i = 0; i < list->numShoreLines; i++)
					{
						audMetadataObjectInfo info;
						if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(list->ShoreLines[i].ShoreLine,info))
						{
							audShoreLine* newShore = NULL;
							switch(info.GetType())
							{
							case ShoreLinePoolAudioSettings::TYPE_ID:
								newShore = rage_new audShoreLinePool(info.GetObject<ShoreLinePoolAudioSettings>());
								break;
							case ShoreLineRiverAudioSettings::TYPE_ID:
								newShore = rage_new audShoreLineRiver(info.GetObject<ShoreLineRiverAudioSettings>());
								riverShorelines.PushAndGrow(static_cast<audShoreLineRiver*>(newShore));
								riverShorelinesHashes.PushAndGrow(list->ShoreLines[i].ShoreLine);
								break;
							case ShoreLineOceanAudioSettings::TYPE_ID:
								newShore = rage_new audShoreLineOcean(info.GetObject<ShoreLineOceanAudioSettings>());
								oceanShorelines.PushAndGrow(static_cast<audShoreLineOcean*>(newShore));
								oceanShorelinesHashes.PushAndGrow(list->ShoreLines[i].ShoreLine);
								break;
							case ShoreLineLakeAudioSettings::TYPE_ID:
								newShore = rage_new audShoreLineLake(info.GetObject<ShoreLineLakeAudioSettings>());
								lakeShorelines.PushAndGrow(static_cast<audShoreLineLake*>(newShore));
								lakeShorelinesHashes.PushAndGrow(list->ShoreLines[i].ShoreLine);
								break;
							default:
								naAssertf(false,"Wrong shoreline type when loading.");		
								break;
							}
							if (newShore)
							{
								newShore->Load();
								m_ShoreLinesQuadTree.AddItemRestrictDupes((void*)newShore, fwRect(newShore->GetActivationBoxAABB()));

								audDLCShore dlcShore;
								dlcShore.chunkNameHash = chunk.chunkNameHash;
								dlcShore.zone = newShore;
								m_DLCShores.PushAndGrow(dlcShore);
							}
						}
						else
						{
							naAssertf(false, "Invalid object info in audAmbientAudioEntity::LoadShorelines()");			
						}
					}
					naAssertf(oceanShorelines.GetCount() == oceanShorelinesHashes.GetCount(),"Something went wrong when trying to link the ocean shorelines, please contact the audio team.");
					naAssertf(riverShorelines.GetCount() == riverShorelinesHashes.GetCount(),"Something went wrong when trying to link the river shorelines, please contact the audio team.");
					naAssertf(lakeShorelines.GetCount() == lakeShorelinesHashes.GetCount(),"Something went wrong when trying to link the river shorelines, please contact the audio team.");
					// Link ocean shorelines to each other. 
					for(u32 i = 0; i < oceanShorelines.GetCount(); i ++)
					{
						u32 nextOceanShorelineHash = oceanShorelines[i]->GetOceanSettings()->NextShoreline;
						if( nextOceanShorelineHash == 0 && oceanShorelines.GetCount() > 1)
						{
							nextOceanShorelineHash = linkShoreline;
						}
						for(u32 j = 0; j < oceanShorelinesHashes.GetCount(); j ++)
						{
							if(oceanShorelinesHashes[j] == nextOceanShorelineHash)
							{
								naAssertf(i != j,"Trying to link an ocean shoreline with itself");
								oceanShorelines[i]->SetNextOceanReference(oceanShorelines[j]);
								oceanShorelines[j]->SetPrevOceanReference(oceanShorelines[i]);
							}
						}
					}
					// Link river shorelines to each other. 
					for(u32 i = 0; i < riverShorelines.GetCount(); i ++)
					{
						u32 nextRiverShorelineHash = riverShorelines[i]->GetRiverSettings()->NextShoreline;
						for(u32 j = 0; j < riverShorelinesHashes.GetCount(); j ++)
						{
							if(riverShorelinesHashes[j] == nextRiverShorelineHash)
							{
								naAssertf(i != j,"Trying to link an river shoreline with itself");
								riverShorelines[i]->SetNextRiverReference(riverShorelines[j]);
								riverShorelines[j]->SetPrevRiverReference(riverShorelines[i]);
							}
						}
					}
					// Link lake shorelines to each other. 
					for(u32 i = 0; i < lakeShorelines.GetCount(); i ++)
					{
						u32 nextLakeShorelineHash = lakeShorelines[i]->GetLakeSettings()->NextShoreline;
						for(u32 j = 0; j < lakeShorelinesHashes.GetCount(); j ++)
						{
							if(lakeShorelinesHashes[j] == nextLakeShorelineHash)
							{
								naAssertf(i != j,"Trying to link an river shoreline with itself");
								lakeShorelines[i]->SetNextLakeReference(lakeShorelines[j]);
								lakeShorelines[j]->SetPrevLakeReference(lakeShorelines[i]);
							}
						}
					}
				}
			}
			m_LoadedShorelinesDLCChunks.PushAndGrow(chunk.chunkNameHash);
		}
	}
}

// ----------------------------------------------------------------
// Destroy any unreferenced DLC zones
// ----------------------------------------------------------------
void audAmbientAudioEntity::DestroyUnreferencedDLCZones(bool forceDestroy)
{
	const s32 numChunks = (s32)audNorthAudioEngine::GetMetadataManager().GetNumChunks();

	// Check our existing DLC zones to see if their chunk has been unloaded. If so, their metadata references
	// are no longer valid so they need to be disabled and deleted
	for(u32 dlcZoneIndex = 0; dlcZoneIndex < m_DLCZones.GetCount(); )
	{
		bool referenced = false;

		if(!forceDestroy)
		{
			for(u32 chunkIndex = 1; chunkIndex < numChunks; chunkIndex++)
			{
				const audMetadataChunk& chunk = audNorthAudioEngine::GetMetadataManager().GetChunk(chunkIndex);

				if(chunk.enabled && chunk.chunkNameHash == m_DLCZones[dlcZoneIndex].chunkNameHash)
				{
					referenced = true;
					break;
				}
			}
		}

		if(!referenced)
		{
			audAmbientZone* zoneToDelete = m_DLCZones[dlcZoneIndex].zone;

			// SetActive stops all sounds and frees waveslots
			zoneToDelete->SetActive(false);

			// Remove the zone from the quad tree and any lists that reference it
			if(m_ActiveEnvironmentZones.GetCount() != 0 && zoneToDelete->GetEnvironmentRule() != 0)
				m_ActiveEnvironmentZones.DeleteMatches(zoneToDelete);

			s32 activeZoneIndex = m_ActiveZones.Find(zoneToDelete);

			if(activeZoneIndex >= 0) 
			{
				m_ActiveZones.Delete(activeZoneIndex);
			}

			m_Zones.Delete(m_Zones.Find(zoneToDelete));
			m_ZoneQuadTree.DeleteItem(zoneToDelete);
			delete zoneToDelete;
			m_DLCZones.Delete(dlcZoneIndex);
		}
		else
		{
			dlcZoneIndex++;
		}
	}

	// Check our existing DLC shores to see if their chunk has been unloaded. If so, their metadata references
	// are no longer valid so they need to be disabled and deleted
	for(u32 dlcShoreIndex = 0; dlcShoreIndex < m_DLCShores.GetCount(); )
	{
		bool referenced = false;

		if(!forceDestroy)
		{
			for(u32 chunkIndex = 1; chunkIndex < numChunks; chunkIndex++)
			{
				const audMetadataChunk& chunk = audNorthAudioEngine::GetMetadataManager().GetChunk(chunkIndex);

				if(chunk.enabled && chunk.chunkNameHash == m_DLCShores[dlcShoreIndex].chunkNameHash)
				{
					referenced = true;
					break;
				}
			}
		}

		if(!referenced)
		{
			audShoreLine* shoreToDelete = m_DLCShores[dlcShoreIndex].zone;

			s32 activeShoreIndex = m_ActiveShoreLines.Find(shoreToDelete);

			if(activeShoreIndex >= 0) 
			{
				m_ActiveShoreLines.Delete(activeShoreIndex);
			}

			m_ShoreLinesQuadTree.DeleteItem(shoreToDelete);
			delete shoreToDelete;
			m_DLCShores.Delete(dlcShoreIndex);
		}
		else
		{
			dlcShoreIndex++;
		}
	}


	for(u32 chunkIndex = 1; chunkIndex < numChunks; chunkIndex++)
	{
		const audMetadataChunk& chunk = audNorthAudioEngine::GetMetadataManager().GetChunk(chunkIndex);

		if(!chunk.enabled)
		{
			atArray<audDLCBankMap>& dlcBankMapList = audAmbientZone::GetDLCBankMapList();

			for(u32 dlcBankMapIndex = 0; dlcBankMapIndex < dlcBankMapList.GetCount(); dlcBankMapIndex++)
			{
				if(dlcBankMapList[dlcBankMapIndex].chunkNameHash == chunk.chunkNameHash)
				{
					dlcBankMapList.Delete(dlcBankMapIndex);
					continue;
				}
			}
		}
	}
	
	for(u32 chunkIndex = 1; chunkIndex < numChunks; chunkIndex++)
	{
		const audMetadataChunk& chunk = audNorthAudioEngine::GetMetadataManager().GetChunk(chunkIndex);

		if(!chunk.enabled)
		{
			s32 chunkArrayIndex = m_LoadedDLCChunks.Find(chunk.chunkNameHash);

			if(chunkArrayIndex >= 0)
			{
				m_LoadedDLCChunks.Delete(chunkArrayIndex);
				m_LoadedShorelinesDLCChunks.Delete(chunkArrayIndex);
			}
		}
	}
}

// ----------------------------------------------------------------
// Check for any newly added DLC zones
// ----------------------------------------------------------------
void audAmbientAudioEntity::CheckAndAddZones()
{
	if(!audAmbientZone::HasBeenInitialised())
	{
		return;
	}

	s32 levelIndex = CGameLogic::GetCurrentLevelIndex();

	// Game level index value defaults to level 1 even with 0 levels loaded...
	if(levelIndex < CScene::GetLevelsData().GetCount())
	{		
		//const char* pLevelName = audNorthAudioEngine::GetCurrentAudioLevelName();

		//////////////////////
		// TEMP! Disabled loading of map specific data unless you're running -audiodesigner. This is just to prevent the
		// world being totally silent until we've got some ambient zones marked out.
		const char* pLevelName = "gta5";

#if RSG_BANK
		if(PARAM_audiodesigner.Get())
		{
			pLevelName = audNorthAudioEngine::GetCurrentAudioLevelName();
		}
#endif
		////////////////////////

		size_t len = strlen(pLevelName);
		atHashString levelName = pLevelName;
		bool isCoreLevel = false;
		char zoneListName[64];
		char bankMapListName[64];
		char shorelineListName[64];

		// ignore "gta5_"
		if(levelName == "gta5")
		{
			formatf(zoneListName, "AMBIENT_ZONE_LIST");
			formatf(bankMapListName, "AMBIENCE_BANK_MAP_AUTOGENERATED");
			formatf(shorelineListName, "ShoreLineList");
			isCoreLevel = true;
		}
		else
		{
			if(len > 5)
			{
				const char *suffix = pLevelName + 5;
				formatf(zoneListName, "AMBIENT_ZONE_LIST_%s", suffix);
				formatf(bankMapListName, "AMBIENCE_BANK_MAP_AUTOGENERATED_%s", suffix);
				formatf(shorelineListName, "ShoreLineList_%s", suffix);
			}
		}

		if(levelName != m_LoadedLevelName)
		{
			DestroyAllZonesAndShorelines();

			if(isCoreLevel)
			{
				// Do this before loading ambient zones, as they need to know which shore lines they contain
				LoadShorelines(atStringHash(shorelineListName), ATSTRINGHASH("SHORELINE_OCEAN_EL_BURRO_HEIGHTS_0_0", 0x87971D3A));
				AddCoreZones();
			}
			else
			{
				// TODO - Need to pass in the correct link shoreline depending on level, once RAVE data has been configured
				LoadShorelines(atStringHash(shorelineListName), 0);
			}
		}
		else
		{
			DestroyUnreferencedDLCZones(false);
		}

		LoadNewDLCZones(zoneListName, bankMapListName);
		LoadNewDLCShorelines(atStringHash(shorelineListName), 0);
		m_LoadedLevelName = levelName;
	}
}

// ----------------------------------------------------------------
// Check the temporary status of an ambient zone
// ----------------------------------------------------------------
TristateValue audAmbientAudioEntity::GetZoneNonPersistentStatus(const u32 ambientZone) const
{
	AmbientZone* zone = audNorthAudioEngine::GetObject<AmbientZone>(ambientZone);

	if(zone)
	{
		return AUD_GET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT);
	}

	return AUD_TRISTATE_UNSPECIFIED;
}

// ----------------------------------------------------------------
// Set a given zone to be enabled or disabled on a temporary basis
// ----------------------------------------------------------------
bool audAmbientAudioEntity::SetZoneNonPersistentStatus(const u32 ambientZone, const bool enabled, const bool forceUpdate)
{
	AmbientZone* zone = audNorthAudioEngine::GetObject<AmbientZone>(ambientZone);

	if(zone)
	{
		if(audVerifyf(AUD_GET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT) == AUD_TRISTATE_UNSPECIFIED, "Failed to set status of ambient zone %s, does anothers script already have control?", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(zone->NameTableOffset)))
		{
			AUD_CLEAR_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT);
			AUD_SET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT, enabled? AUD_TRISTATE_TRUE : AUD_TRISTATE_FALSE);

			if(forceUpdate)
			{
				for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
				{
					if(m_ActiveZones[loop]->GetZoneData() == zone)
					{
						m_ActiveZones[loop]->ForceStateUpdate();
					}
				}

				for(u32 loop = 0; loop < m_InteriorZones.GetCount(); loop++)
				{
					if(m_InteriorZones[loop].GetZoneData() == zone)
					{
						m_InteriorZones[loop].ForceStateUpdate();
					}
				}
			}

			bool alreadyExists = false;

			// If this zone is already being modified, just update the flag
			for(u32 loop = 0; loop < m_ZonesWithNonPersistentStateFlags.GetCount(); loop++)
			{
				if(m_ZonesWithNonPersistentStateFlags[loop].zoneNameHash == ambientZone)
				{
					m_ZonesWithNonPersistentStateFlags[loop].enabled = enabled;
					alreadyExists = true;
					break;
				}
			}

			// New modification? Add to our list
			if(!alreadyExists)
			{
				audAmbientZoneEnabledState zoneState;
				zoneState.zoneNameHash = ambientZone;
				zoneState.enabled = enabled;
				m_ZonesWithNonPersistentStateFlags.PushAndGrow(zoneState);
			}

			BANK_ONLY(audDisplayf("Ambient Zone non-persistent state change: Zone %s State %s Force Update %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(zone->NameTableOffset), enabled? "Enabled" : "Disabled", forceUpdate? "Yes" : "No");)
			m_ForceQuadTreeUpdate = true;
			REPLAY_ONLY(UpdateAmbientZoneReplayStates());
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Clear the non-persistent status of this zone
// ----------------------------------------------------------------
bool audAmbientAudioEntity::ClearZoneNonPersistentStatus(const u32 ambientZone, const bool forceUpdate)
{
	AmbientZone* zone = audNorthAudioEngine::GetObject<AmbientZone>(ambientZone);

	if(zone && AUD_GET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT) != AUD_TRISTATE_UNSPECIFIED)
	{
		AUD_CLEAR_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT);
		AUD_SET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT, AUD_TRISTATE_UNSPECIFIED);

		if(forceUpdate)
		{
			for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
			{
				if(m_ActiveZones[loop]->GetZoneData() == zone)
				{
					m_ActiveZones[loop]->ForceStateUpdate();
				}
			}
		}

		// Delete this zone from our list of modified zones
		for(u32 index = 0; index < m_ZonesWithNonPersistentStateFlags.GetCount(); index++)
		{
			if(m_ZonesWithNonPersistentStateFlags[index].zoneNameHash == ambientZone)
			{
				m_ZonesWithNonPersistentStateFlags.Delete(index);
				break;
			}
		}

		BANK_ONLY(audDisplayf("Ambient Zone non-persistent state cleared: Zone %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(zone->NameTableOffset));)
		m_ForceQuadTreeUpdate = true;
		REPLAY_ONLY(UpdateAmbientZoneReplayStates());
		return true;
	}

	return false;
}

// ----------------------------------------------------------------
// Set a given zone to be enabled or disabled on a permanent basis
// ----------------------------------------------------------------
void audAmbientAudioEntity::SetZonePersistentStatus(const u32 ambientZone, const bool enabled, const bool forceUpdate)
{
	AmbientZone* zone = audNorthAudioEngine::GetObject<AmbientZone>(ambientZone); 
	if(zone)
	{
		s32 zoneIndex = m_ZonesWithModifiedEnabledFlags.Find(ambientZone);
		bool currentlyEnabled = AUD_GET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT) == AUD_TRISTATE_TRUE;

		if(enabled != currentlyEnabled)
		{
			AUD_CLEAR_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT);
			AUD_SET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT, enabled? AUD_TRISTATE_TRUE : AUD_TRISTATE_FALSE);
			BANK_ONLY(audDisplayf("Ambient Zone persistent state change: Zone %s State %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(zone->NameTableOffset), enabled? "Enabled" : "Disabled");)

			if(zoneIndex < 0)
			{
				m_ZonesWithModifiedEnabledFlags.PushAndGrow(ambientZone);
			}
			else
			{
				m_ZonesWithModifiedEnabledFlags.Delete(zoneIndex);
			}

			if(forceUpdate)
			{
				for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
				{
					if(m_ActiveZones[loop]->GetZoneData() == zone)
					{
						m_ActiveZones[loop]->ForceStateUpdate();
					}
				}
			}

			m_ForceQuadTreeUpdate = true;
			REPLAY_ONLY(UpdateAmbientZoneReplayStates());
		}
	}
}

// ----------------------------------------------------------------
// Clear all permanent zone status changes
// ----------------------------------------------------------------
void audAmbientAudioEntity::ClearAllPersistentZoneStatusChanges(const bool forceUpdate)
{
	for(s32 loop = 0; loop < m_ZonesWithModifiedEnabledFlags.GetCount(); loop++)
	{
		AmbientZone* zone = audNorthAudioEngine::GetObject<AmbientZone>(m_ZonesWithModifiedEnabledFlags[loop]);

		if(zone)
		{
			bool currentlyEnabled = AUD_GET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT) == AUD_TRISTATE_TRUE;
			AUD_CLEAR_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT);
			AUD_SET_TRISTATE_VALUE(zone->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT, !currentlyEnabled? AUD_TRISTATE_TRUE : AUD_TRISTATE_FALSE);
			BANK_ONLY(audDisplayf("Ambient Zone persistent state change cleared: Zone %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(zone->NameTableOffset));)

			if(forceUpdate)
			{
				for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
				{
					if(m_ActiveZones[loop]->GetZoneData() == zone)
					{
						m_ActiveZones[loop]->ForceStateUpdate();
					}
				}
			}
		}
	}

	m_ForceQuadTreeUpdate = true;
	m_ZonesWithModifiedEnabledFlags.clear();
	REPLAY_ONLY(UpdateAmbientZoneReplayStates());
}

// ----------------------------------------------------------------
// Prepare the given alarm
// ----------------------------------------------------------------
bool audAmbientAudioEntity::PrepareAlarm(const char* alarmName)
{
	AlarmSettings* alarmSettings = audNorthAudioEngine::GetObject<AlarmSettings>(alarmName);
	audAssertf(alarmSettings, "Failed to find alarm %s", alarmName);
	return PrepareAlarm(alarmSettings);
}

// ----------------------------------------------------------------
// Prepare the given alarm
// ----------------------------------------------------------------
bool audAmbientAudioEntity::PrepareAlarm(u32 alarmNameHash)
{
	AlarmSettings* alarmSettings = audNorthAudioEngine::GetObject<AlarmSettings>(alarmNameHash);
	audAssertf(alarmSettings, "Failed to find alarm %u", alarmNameHash);
	return PrepareAlarm(alarmSettings);
}

// ----------------------------------------------------------------
// Prepare the given alarm
// ----------------------------------------------------------------
bool audAmbientAudioEntity::PrepareAlarm(AlarmSettings* alarmSettings)
{
	if(alarmSettings)
	{
		u32 bankIndex = SOUNDFACTORY.GetBankNameIndexFromHash(alarmSettings->BankName);
		audAssertf(bankIndex<AUD_INVALID_BANK_ID, "Failed to find bank %u for alarm", alarmSettings->BankName);

		if (bankIndex<AUD_INVALID_BANK_ID)
		{
			// In replay, bank loading is managed by its own system, so just wait until that has completed before continuing
			if(REPLAY_ONLY(CReplayMgr::IsEditModeActive() ||) g_ScriptAudioEntity.RequestScriptBank(bankIndex, false, AUD_NET_ALL_PLAYERS))
			{
				if(g_ScriptAudioEntity.IsScriptBankLoaded(bankIndex))
				{
					return true;
				}
			}
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Start the given alarm
// ----------------------------------------------------------------
void audAmbientAudioEntity::StartAlarm(const char* alarmName, bool skipStartup)
{
	AlarmSettings* alarmSettings = audNorthAudioEngine::GetObject<AlarmSettings>(alarmName);
	audAssertf(alarmSettings, "Failed to find alarm %s", alarmName);
	StartAlarm(alarmSettings, atStringHash(alarmName), skipStartup);
}

// ----------------------------------------------------------------
// Start the given alarm
// ----------------------------------------------------------------
void audAmbientAudioEntity::StartAlarm(u32 alarmNameHash, bool skipStartup)
{
	AlarmSettings* alarmSettings = audNorthAudioEngine::GetObject<AlarmSettings>(alarmNameHash);
	audAssertf(alarmSettings, "Failed to find alarm %u", alarmNameHash);
	StartAlarm(alarmSettings, alarmNameHash, skipStartup);
}

// ----------------------------------------------------------------
// Start the given alarm
// ----------------------------------------------------------------
void audAmbientAudioEntity::StartAlarm(AlarmSettings* alarmSettings, u32 alarmNameHash, bool skipStartup)
{	
	if(alarmSettings)
	{
		s32 alarmIndex = -1;
		bool alreadyPlaying = false;

		for(u32 loop = 0; loop < kMaxActiveAlarms; loop++)
		{
			if(m_ActiveAlarms[loop].alarmSettings == alarmSettings)
			{
				alreadyPlaying = true;
				alarmIndex = loop;
				break;
			}
			else if(m_ActiveAlarms[loop].alarmSettings == NULL)
			{
				alarmIndex = loop;
			}
		}

		audAssertf(alarmIndex >= 0 , "Failed to play alarm - insufficient free alarm slots (max %d)", kMaxActiveAlarms);

		if(alarmIndex >= 0)
		{
			if(PrepareAlarm(alarmSettings))
			{
				m_ActiveAlarms[alarmIndex].alarmName = alarmNameHash;

				if(!m_ActiveAlarms[alarmIndex].primaryAlarmSound)
				{
					audSoundInitParams initParams;
					initParams.Position = alarmSettings->CentrePosition;
					CreateAndPlaySound_Persistent(alarmSettings->AlarmLoop, &m_ActiveAlarms[alarmIndex].primaryAlarmSound, &initParams);
					m_ActiveAlarms[alarmIndex].alarmSettings = alarmSettings;					
					m_ActiveAlarms[alarmIndex].alarmDecayCurve.Init(alarmSettings->AlarmDecayCurve);
					m_ActiveAlarms[alarmIndex].interiorSettings = audNorthAudioEngine::GetObject<InteriorSettings>(alarmSettings->InteriorSettings);
					REPLAY_ONLY(UpdateReplayAlarmStates();)
				}

				if(!alreadyPlaying)
				{
					if(skipStartup)
					{
						m_ActiveAlarms[alarmIndex].timeStarted = fwTimer::GetTimeInMilliseconds() - 120000;
					}
					else
					{
						m_ActiveAlarms[alarmIndex].timeStarted = fwTimer::GetTimeInMilliseconds();
					}
				}

				// Force an update to set correct intial volumes etc.
				m_ActiveAlarms[alarmIndex].stopAtMaxDistance = false;
				UpdateAlarms(true);
			}
		}	
	}		
}

// ----------------------------------------------------------------
// Stop the given alarm
// ----------------------------------------------------------------
void audAmbientAudioEntity::StopAlarm(const char* alarmName, bool stopInstantly)
{
	AlarmSettings* alarmSettings = audNorthAudioEngine::GetObject<AlarmSettings>(alarmName);
	audAssertf(alarmSettings, "Failed to find alarm %s", alarmName);
	StopAlarm(alarmSettings, stopInstantly);	
}

// ----------------------------------------------------------------
// Stop the given alarm
// ----------------------------------------------------------------
void audAmbientAudioEntity::StopAlarm(u32 alarmNameHash, bool stopInstantly)
{
	AlarmSettings* alarmSettings = audNorthAudioEngine::GetObject<AlarmSettings>(alarmNameHash);
	audAssertf(alarmSettings, "Failed to find alarm %u", alarmNameHash);
	StopAlarm(alarmSettings, stopInstantly);	
}

// ----------------------------------------------------------------
// Stop the given alarm
// ----------------------------------------------------------------
void audAmbientAudioEntity::StopAlarm(AlarmSettings* alarmSettings, bool stopInstantly)
{
	if(alarmSettings)
	{
		for(u32 loop = 0; loop < kMaxActiveAlarms; loop++)
		{
			if(m_ActiveAlarms[loop].alarmSettings == alarmSettings)
			{
				if(stopInstantly)
				{ 
					m_ActiveAlarms[loop].alarmSettings = NULL;

					if(m_ActiveAlarms[loop].primaryAlarmSound)
					{
						m_ActiveAlarms[loop].primaryAlarmSound->StopAndForget();
					}
				}
				else
				{
					m_ActiveAlarms[loop].stopAtMaxDistance = true;
				}

				g_ScriptAudioEntity.ScriptBankNoLongerNeeded(SOUNDFACTORY.GetBankNameIndexFromHash(alarmSettings->BankName));
			}
		}

		REPLAY_ONLY(UpdateReplayAlarmStates();)
	}
}

// ----------------------------------------------------------------
// StopAllAlarms
// ----------------------------------------------------------------
void audAmbientAudioEntity::StopAllAlarms(bool stopInstantly)
{
	for(u32 loop = 0; loop < kMaxActiveAlarms; loop++)
	{
		if(m_ActiveAlarms[loop].alarmSettings)
		{
			if(stopInstantly)
			{ 
				m_ActiveAlarms[loop].alarmSettings = NULL;

				if(m_ActiveAlarms[loop].primaryAlarmSound)
				{
					m_ActiveAlarms[loop].primaryAlarmSound->StopAndForget();
				}
			}
			else
			{
				m_ActiveAlarms[loop].stopAtMaxDistance = true;
			}
		}
	}

	REPLAY_ONLY(UpdateReplayAlarmStates();)
}

// ----------------------------------------------------------------
// Update the world height factor
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdateWorldHeight()
{
	m_WorldHeightFactor = ComputeWorldHeightFactorAtListenerPosition();
	m_UnderwaterCreakFactor = Water::GetCameraWaterDepth() > 0.0f ? ComputeUnderwaterCreakFactorAtListenerPosition() : 0.0f;
}

// ----------------------------------------------------------------
// Compute the underwater creak factor
// ----------------------------------------------------------------
f32 audAmbientAudioEntity::ComputeUnderwaterCreakFactorAtListenerPosition() const
{
	const Vec3V listPos = g_AudioEngine.GetEnvironment().GetPanningListenerPosition();
	f32 maxCreakFactor = 0.0f;

	for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
	{
		if(m_ActiveZones[loop]->GetZoneData() && m_ActiveZones[loop]->GetZoneData()->UnderwaterCreakFactor > 0)
		{
			f32 creakFactor = m_ActiveZones[loop]->GetZoneData()->UnderwaterCreakFactor * m_ActiveZones[loop]->ComputeZoneInfluenceRatioAtPoint(listPos);
			maxCreakFactor = Max(creakFactor, maxCreakFactor);
		}
	}
	
	return maxCreakFactor;
}

// ----------------------------------------------------------------
// Get the max world height if any ambient zones are altering it
// ----------------------------------------------------------------
f32 audAmbientAudioEntity::ComputeWorldHeightFactorAtListenerPosition() 
{
	const Vec3V listPos = g_AudioEngine.GetEnvironment().GetPanningListenerPosition();
	s32 smallestAreaIndex = -1;
	f32 smallestArea = FLT_MAX;

	for(u32 loop = 0; loop < m_ActiveZones.GetCount(); loop++)
	{
		if(m_ActiveZones[loop]->GetZoneData()->MaxWindInfluence >= 0)
		{
			f32 zoneArea = m_ActiveZones[loop]->GetPositioningArea();

			if(m_ActiveZones[loop]->GetPositioningArea() < smallestArea || smallestAreaIndex == -1)
			{
				smallestAreaIndex = loop;
				smallestArea = zoneArea;
			}
		}
	}
	m_CurrentWindZone = NULL;

	if(smallestAreaIndex >= 0)
	{
		f32 heightRatio = m_ActiveZones[smallestAreaIndex]->GetHeightFactorInPositioningZone(listPos);
		f32 minHeight = Max(m_ActiveZones[smallestAreaIndex]->GetZoneData()->MinWindInfluence, 0.0f);
		f32 maxHeight = Min(m_ActiveZones[smallestAreaIndex]->GetZoneData()->MaxWindInfluence, 1.0f);
		m_CurrentWindZone = m_ActiveZones[smallestAreaIndex];
		return minHeight + (heightRatio * (maxHeight  - minHeight));
	}

	return 0.0f;
}

// ----------------------------------------------------------------
// Update the active alarms
// ----------------------------------------------------------------
void audAmbientAudioEntity::UpdateAlarms(bool REPLAY_ONLY(isForcedUpdate))
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && !isForcedUpdate)
	{
		for(u32 loop = 0; loop < kMaxActiveAlarms; loop++)
		{
			if(m_ActiveAlarms[loop].alarmSettings)
			{
				bool alarmStillActive = false;

				for(u32 replayAlarmIdx = 0; replayAlarmIdx < m_ReplayActiveAlarms.GetCount(); replayAlarmIdx++)
				{
					if(m_ReplayActiveAlarms[replayAlarmIdx].alarmNameHash == m_ActiveAlarms[loop].alarmName)
					{
						alarmStillActive = true;
						break;
					}
				}

				if(!alarmStillActive)
				{
					StopAlarm(m_ActiveAlarms[loop].alarmSettings, true);
				}
			}
		}

		for(u32 loop = 0; loop < m_ReplayActiveAlarms.GetCount(); loop++)
		{
			StartAlarm(m_ReplayActiveAlarms[loop].alarmNameHash, true);
		}
	}
#endif

	CPed* playerPed = FindPlayerPed();

	// Kill all alarms once the player dies or is arrested
	if(playerPed)
	{
		if(playerPed->GetIsArrested() || playerPed->IsDead())
		{
			if(camInterface::IsFadedOut() && !m_WasPlayerDead)
			{
				StopAllAlarms();
				m_WasPlayerDead = true;
			}
		}
		else
		{
			m_WasPlayerDead = false;
		}
	}

	for(u32 loop = 0; loop < kMaxActiveAlarms; loop++)
	{
		if(m_ActiveAlarms[loop].alarmSettings)
		{
			if(m_ActiveAlarms[loop].stopAtMaxDistance REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
			{
				Vector3 alarmCentre = m_ActiveAlarms[loop].alarmSettings->CentrePosition;

				if(m_ActiveAlarms[loop].primaryAlarmSound)
				{
					alarmCentre = VEC3V_TO_VECTOR3(m_ActiveAlarms[loop].primaryAlarmSound->GetRequestedPosition());
				}

				f32 distFromListenerSq = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0)).Dist2(alarmCentre);

				if(distFromListenerSq > (m_ActiveAlarms[loop].alarmSettings->StopDistance * m_ActiveAlarms[loop].alarmSettings->StopDistance))
				{
					if(m_ActiveAlarms[loop].primaryAlarmSound)
					{
						m_ActiveAlarms[loop].primaryAlarmSound->StopAndForget();
					}

					m_ActiveAlarms[loop].stopAtMaxDistance = false;
					m_ActiveAlarms[loop].alarmSettings = NULL;
					REPLAY_ONLY(UpdateReplayAlarmStates();)
				}
			}

			if(m_ActiveAlarms[loop].primaryAlarmSound)
			{
				f32 timeAliveSecs = (fwTimer::GetTimeInMilliseconds() - m_ActiveAlarms[loop].timeStarted)/1000.0f;

#if GTA_REPLAY
				if(CReplayMgr::IsEditModeActive())
				{
					for(u32 loop = 0; loop < m_ReplayActiveAlarms.GetCount(); loop++)
					{
						if(m_ActiveAlarms[loop].alarmName == m_ReplayActiveAlarms[loop].alarmNameHash)
						{
							timeAliveSecs = m_ReplayActiveAlarms[loop].timeAlive/1000.0f;
							break;
						}						
					}
				}
#endif

				f32 volume = m_ActiveAlarms[loop].alarmDecayCurve.CalculateValue(timeAliveSecs);
				m_ActiveAlarms[loop].primaryAlarmSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(volume));

				// Alarms with an interior settings object will have the alarm sound follow the player whilst the interior is active
				if(m_ActiveAlarms[loop].interiorSettings)
				{
					Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
					CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

					if(pIntInst)
					{
						CMloModelInfo*	pModelInfo = reinterpret_cast<CMloModelInfo*>( pIntInst->GetBaseModelInfo());

						if(pModelInfo)
						{
							InteriorSettings* interiorSettings = audNorthAudioEngine::GetObject<InteriorSettings>(pModelInfo->GetHashKey());

							if(interiorSettings == m_ActiveAlarms[loop].interiorSettings)
							{
								m_ActiveAlarms[loop].primaryAlarmSound->SetRequestedPosition(listenerPosition);
							}
						}
					}
				}
			}
		}
	}
}

#if __BANK
void audAmbientAudioEntity::RenderShoreLinesDebug()
{
	ShoreLinesQuadTreeRenderFunction shoreLinesRenderFunction;
	m_ShoreLinesQuadTree.ForAllMatching(RCC_VECTOR3(m_ZoneActivationPosition), shoreLinesRenderFunction);
}
// ----------------------------------------------------------------
// Does the given name match the test name
// ----------------------------------------------------------------
bool audAmbientAudioEntity::MatchName(const char *name, const char *testString)
{
	char buf1[128];
	char buf2[128];
	const s32 len1 = istrlen(name);
	const s32 len2 = istrlen(testString);

	Assert(len1 < sizeof(buf1));
	Assert(len2 < sizeof(buf2));

	for(s32 i = 0; i < len1; i++)
	{
		buf1[i] = static_cast<char>( toupper(name[i]) );
	}
	buf1[len1] = 0;

	for(s32 i = 0; i < len2; i++)
	{
		buf2[i] = static_cast<char>( toupper(testString[i]) );
	}
	buf2[len2] = 0;
	return strstr(buf1,buf2) != NULL;
}

void audAmbientAudioEntity::CreateZone(bool interiorZone, const char* zoneName, const Vector3& positioningCenter, const Vector3& positioningSize, const u16 positioningRotation, const Vector3& activationCenter, const Vector3& activationSize, const u16 activationRotation)
{
	char xmlMsg[4096];
	char tmpBuf[256] = {0};

	SerialiseMessageStart(xmlMsg, tmpBuf, "AmbientZone", zoneName);
	SerialiseString(xmlMsg, tmpBuf, "Shape", "kAmbientZoneCuboid");
	SerialiseBool(xmlMsg, tmpBuf, "InteriorZone", interiorZone);

	SerialiseTag(xmlMsg, tmpBuf, "ActivationZone", true);
	SerialiseVector(xmlMsg, tmpBuf, "Centre", activationCenter);
	SerialiseVector(xmlMsg, tmpBuf, "Size", activationSize);
	SerialiseInt(xmlMsg, tmpBuf, "RotationAngle", activationRotation);
	SerialiseTag(xmlMsg, tmpBuf, "ActivationZone", false);

	SerialiseTag(xmlMsg, tmpBuf, "PositioningZone", true);
	SerialiseVector(xmlMsg, tmpBuf, "Centre", positioningCenter);
	SerialiseVector(xmlMsg, tmpBuf, "Size", positioningSize);
	SerialiseInt(xmlMsg, tmpBuf, "RotationAngle", positioningRotation);
	SerialiseTag(xmlMsg, tmpBuf, "PositioningZone", false);
	SerialiseMessageEnd(xmlMsg, tmpBuf, "AmbientZone");

	naDisplayf("XML message is %d characters long \n", istrlen(xmlMsg));

	audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
	bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
	naAssertf(success, "Failed to send xml message to rave");
	naDisplayf("%s", (success)? "Success":"Failed");
}

void audAmbientAudioEntity::UpdateRaveZone(audAmbientZone* zone, const char* zoneName)
{
	if(!zone)
	{
		return;
	}

	char xmlMsg[4096];
	char tmpBuf[256] = {0};

	if(!audVerifyf(sizeof(AmbientZone) == 624, "Ambient zone data size has changed (currently %" SIZETFMT "d)! Export function needs updating!", sizeof(AmbientZone)))
	{
		return;
	}

	// Check the number of flags hasn't changed - if so, the export function needs updating
	CompileTimeAssert(FLAG_ID_AMBIENTZONE_MAX == 11);

	const AmbientZone* zoneData = zone->GetZoneData();

	if(zoneName)
	{
		SerialiseMessageStart(xmlMsg, tmpBuf, "AmbientZone", zoneName);
	}
	else
	{
		SerialiseMessageStart(xmlMsg, tmpBuf, "AmbientZone", zone->GetName());
		zoneData = audNorthAudioEngine::GetObject<AmbientZone>(zone->GetName());
	}

	switch(zone->GetZoneData()->Shape)
	{
	case kAmbientZoneCuboid:
		SerialiseString(xmlMsg, tmpBuf, "Shape", "kAmbientZoneCuboid");
		break;
	case kAmbientZoneCuboidLineEmitter:
		SerialiseString(xmlMsg, tmpBuf, "Shape", "kAmbientZoneCuboidLineEmitter");
		break;
	case kAmbientZoneSphere:
		SerialiseString(xmlMsg, tmpBuf, "Shape", "kAmbientZoneSphere");
		break;
	case kAmbientZoneSphereLineEmitter:
		SerialiseString(xmlMsg, tmpBuf, "Shape", "kAmbientZoneSphereLineEmitter");
		break;
	}
	
	SerialiseTag(xmlMsg, tmpBuf, "ActivationZone", true);
	SerialiseVector(xmlMsg, tmpBuf, "Centre", zone->GetActivationZoneCenter());
	SerialiseVector(xmlMsg, tmpBuf, "Size", zone->GetActivationZoneSize());
	SerialiseInt(xmlMsg, tmpBuf, "RotationAngle", zone->GetActivationZoneRotationAngle());
	SerialiseVector(xmlMsg, tmpBuf, "PostRotationOffset", zoneData->ActivationZone.PostRotationOffset);
	SerialiseVector(xmlMsg, tmpBuf, "SizeScale", zoneData->ActivationZone.SizeScale);
	SerialiseTag(xmlMsg, tmpBuf, "ActivationZone", false);

	SerialiseTag(xmlMsg, tmpBuf, "PositioningZone", true);
	SerialiseVector(xmlMsg, tmpBuf, "Centre", zone->GetPositioningZoneCenter());
	SerialiseVector(xmlMsg, tmpBuf, "Size", zone->GetPositioningZoneSize());
	SerialiseInt(xmlMsg, tmpBuf, "RotationAngle", zone->GetPositioningZoneRotationAngle());
	SerialiseVector(xmlMsg, tmpBuf, "PostRotationOffset", zoneData->PositioningZone.PostRotationOffset);
	SerialiseVector(xmlMsg, tmpBuf, "SizeScale", zoneData->PositioningZone.SizeScale);
	SerialiseTag(xmlMsg, tmpBuf, "PositioningZone", false);

	SerialiseFloat(xmlMsg, tmpBuf, "BuiltUpFactor", zoneData->BuiltUpFactor);
	SerialiseInt(xmlMsg, tmpBuf, "NumRulesToPlay", zoneData->NumRulesToPlay);

	for(u32 loop = 0; loop < zoneData->numRules; loop++)
	{
		AmbientRule* ambientRule = audNorthAudioEngine::GetObject<AmbientRule>(zoneData->Rule[loop].Ref);

		if(ambientRule)
		{
			SerialiseTag(xmlMsg, tmpBuf, "Rule", true);
			SerialiseString(xmlMsg, tmpBuf, "Ref", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(ambientRule->NameTableOffset));
			SerialiseTag(xmlMsg, tmpBuf, "Rule", false);
		}
	}

	SerialiseBool(xmlMsg, tmpBuf, "InteriorZone", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_INTERIORZONE)==AUD_TRISTATE_TRUE);

	u8* numDirectionalAmbiences = (u8*)(&zoneData->Rule[zoneData->numRules]);
	AmbientZone::tDirAmbience* directionalAmbiences = (AmbientZone::tDirAmbience*)(numDirectionalAmbiences + sizeof(u32));

	for(u8 i=0; i < *numDirectionalAmbiences; i++)
	{
		DirectionalAmbience* directionalAmbience = audNorthAudioEngine::GetObject<DirectionalAmbience>(directionalAmbiences[i].Ref);

		if(directionalAmbience)
		{
			SerialiseTag(xmlMsg, tmpBuf, "DirAmbience", true);
			SerialiseString(xmlMsg, tmpBuf, "Ref", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(directionalAmbience->NameTableOffset));
			SerialiseTag(xmlMsg, tmpBuf, "DirAmbience", false);
		}
	}

	SerialiseFloat(xmlMsg, tmpBuf, "MinPedDensity", zoneData->MinPedDensity);
	SerialiseFloat(xmlMsg, tmpBuf, "MaxPedDensity", zoneData->MaxPedDensity);
	audCurve pedDensityCurve;
	pedDensityCurve.Init(zoneData->PedDensityTOD);
	SerialiseString(xmlMsg, tmpBuf, "PedDensity", pedDensityCurve.GetName());
	SerialiseBool(xmlMsg, tmpBuf, "ZoneEnabledStatePersistent", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT)==AUD_TRISTATE_TRUE);
	
	// Don't save this! It exists in the gameobject but should not be modifiable by us!
	//SerialiseBool(xmlMsg, tmpBuf, "ZoneEnabledStateNonPersistent", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT)==AUD_TRISTATE_TRUE);

	SerialiseFloat(xmlMsg, tmpBuf, "PedDensityScalar", zoneData->PedDensityScalar);
	SerialiseBool(xmlMsg, tmpBuf, "UsePlayerPosition", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_USEPLAYERPOSITION)==AUD_TRISTATE_TRUE);
	SerialiseBool(xmlMsg, tmpBuf, "DisableInMultiplayer", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_DISABLEINMULTIPLAYER)==AUD_TRISTATE_TRUE);
	SerialiseFloat(xmlMsg, tmpBuf, "MaxWindInfluence", zoneData->MaxWindInfluence);
	SerialiseFloat(xmlMsg, tmpBuf, "MinWindInfluence", zoneData->MinWindInfluence);

	EnvironmentRule* environmentRule = audNorthAudioEngine::GetObject<EnvironmentRule>(zoneData->EnvironmentRule);

	if(environmentRule)
	{
		SerialiseString(xmlMsg, tmpBuf, "EnvironmentRule", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(environmentRule->NameTableOffset));
	}

	SerialiseBool(xmlMsg, tmpBuf, "ScaleMaxWorldHeightWithZoneInfluence", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_SCALEMAXWORLDHEIGHTWITHZONEINFLUENCE)==AUD_TRISTATE_TRUE);
	SerialiseBool(xmlMsg, tmpBuf, "HasTunnelReflections", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_HASTUNNELREFLECTIONS)==AUD_TRISTATE_TRUE);
	SerialiseBool(xmlMsg, tmpBuf, "HasReverbLinkedReflections", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_HASREVERBLINKEDREFLECTIONS)==AUD_TRISTATE_TRUE);
	SerialiseBool(xmlMsg, tmpBuf, "HasRandomStaticRadioEmitters", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_HASRANDOMSTATICRADIOEMITTERS)==AUD_TRISTATE_TRUE);
	SerialiseBool(xmlMsg, tmpBuf, "HasRandomVehicleRadioEmitters", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_HASRANDOMVEHICLERADIOEMITTERS)==AUD_TRISTATE_TRUE);
	SerialiseBool(xmlMsg, tmpBuf, "OccludeRain", AUD_GET_TRISTATE_VALUE(zoneData->Flags, FLAG_ID_AMBIENTZONE_OCCLUDERAIN)==AUD_TRISTATE_TRUE);
	SerialiseString(xmlMsg, tmpBuf, "ZoneWaterCalculation", AmbientZoneWaterType_ToString((AmbientZoneWaterType)zoneData->ZoneWaterCalculation));

	PedWallaSettingsList* pedWallaSettings = audNorthAudioEngine::GetObject<PedWallaSettingsList>(zoneData->PedWallaSettings);

	if(pedWallaSettings)
	{
		SerialiseString(xmlMsg, tmpBuf, "PedWallaSettings", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(pedWallaSettings->NameTableOffset));
	}

	MixerScene* mixerScene = DYNAMICMIXMGR.GetObject<MixerScene>(zoneData->AudioScene);

	if(mixerScene)
	{
		SerialiseString(xmlMsg, tmpBuf, "AudioScene", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(mixerScene->NameTableOffset));
	}

	SerialiseMessageEnd(xmlMsg, tmpBuf, "AmbientZone");
	
	naDisplayf("XML message is %d characters long \n", istrlen(xmlMsg));

	audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
	bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
	naAssertf(success, "Failed to send xml message to rave");
	naDisplayf("%s", (success)? "Success":"Failed");
}

void audAmbientAudioEntity::MoveZoneToCameraCoords()
{
	if(g_CurrentEditZone)
	{
		Vector3 listPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
		g_CurrentEditZone->SetPositioningZoneCenter(listPos);
		g_CurrentEditZone->SetActivationZoneCenter(listPos);
		g_CurrentEditZone->ComputeActivationZoneCenter();
		g_CurrentEditZone->ComputePositioningZoneCenter();
		g_CurrentEditZone->CalculatePositioningLine();
	}
}

void audAmbientAudioEntity::CreateZoneAtCurrentCoords()
{
	if(g_CreateZoneName[0] != 0)
	{
		Vector3 listPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
		spdAABB positioningBoundingBox = spdAABB(VECTOR3_TO_VEC3V(listPos - Vector3(5, 5, 5)), VECTOR3_TO_VEC3V(listPos + Vector3(5, 5, 5)));
		spdAABB activationBoundingBox = spdAABB(VECTOR3_TO_VEC3V(listPos - Vector3(10, 10, 10)), VECTOR3_TO_VEC3V(listPos + Vector3(10, 10, 10)));

		Vector3 activationCenter;
		Vector3 activationSize;
		Vector3 positioningCenter;
		Vector3 positioningSize;

		positioningCenter = VEC3V_TO_VECTOR3(positioningBoundingBox.GetCenter());
		positioningSize = 2 * VEC3V_TO_VECTOR3(positioningBoundingBox.GetExtent());

		activationCenter = VEC3V_TO_VECTOR3(activationBoundingBox.GetCenter());
		activationSize = 2 * VEC3V_TO_VECTOR3(activationBoundingBox.GetExtent());

		CreateZone(false, g_CreateZoneName, positioningCenter, positioningSize, 0, activationCenter, activationSize, 0);
	}
}

void audAmbientAudioEntity::SerialiseFloat(char* xmlMessage, char* tempbuffer, const char* elementName, f32 value)
{
	sprintf(tempbuffer, "			<%s>%.02f</%s>\n", elementName, value, elementName);
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
}

void audAmbientAudioEntity::SerialiseInt(char* xmlMessage, char* tempbuffer, const char* elementName, u32 value)
{
	sprintf(tempbuffer, "			<%s>%d</%s>\n", elementName, value, elementName);
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
}

void audAmbientAudioEntity::SerialiseUInt(char* xmlMessage, char* tempbuffer, const char* elementName, u32 value)
{
	sprintf(tempbuffer, "			<%s>%u</%s>\n", elementName, value, elementName);
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
}

void audAmbientAudioEntity::SerialiseString(char* xmlMessage, char* tempbuffer, const char* elementName, const char* value)
{
	sprintf(tempbuffer, "			<%s>%s</%s>\n", elementName, value, elementName);
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
}

void audAmbientAudioEntity::SerialiseSoundName(char* xmlMessage, char* tempbuffer, const char* elementName, u32 hash, bool defaultToNull)
{
	const Sound* soundPtr = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataPtr(hash);

	if(soundPtr)
	{
		const audMetadataManager &metadataManager = SOUNDFACTORY.GetMetadataManager();
		SerialiseString(xmlMessage, tempbuffer, elementName, metadataManager.GetObjectNameFromNameTableOffset(soundPtr->NameTableOffset));
	}
	else if(defaultToNull)
	{
		SerialiseString(xmlMessage, tempbuffer, elementName, "NULL_SOUND");
	}
}

void audAmbientAudioEntity::SerialiseBool(char* xmlMessage, char* tempbuffer, const char* elementName, bool value)
{
	sprintf(tempbuffer, "			<%s>%s</%s>\n", elementName, value? "yes":"no", elementName);
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
}

void audAmbientAudioEntity::SerialiseBool(char* xmlMessage, char* tempbuffer, const char* elementName, u32 flags, u32 tristateValue)
{
	SerialiseBool(xmlMessage, tempbuffer, elementName, AUD_GET_TRISTATE_VALUE(flags, tristateValue) == AUD_TRISTATE_TRUE);
}

void audAmbientAudioEntity::SerialiseVector(char* xmlMessage, char* tempbuffer, const char* elementName, const Vector3& value)
{
	SerialiseTag(xmlMessage, tempbuffer, elementName, true);
	SerialiseFloat(xmlMessage, tempbuffer, "x", value.GetX());
	SerialiseFloat(xmlMessage, tempbuffer, "y", value.GetY());
	SerialiseFloat(xmlMessage, tempbuffer, "z", value.GetZ());
	SerialiseTag(xmlMessage, tempbuffer, elementName, false);
}

void audAmbientAudioEntity::SerialiseVector(char* xmlMessage, char* tempbuffer, const char* elementName, f32 x, f32 y, f32 z)
{
	SerialiseTag(xmlMessage, tempbuffer, elementName, true);
	SerialiseFloat(xmlMessage, tempbuffer, "x", x);
	SerialiseFloat(xmlMessage, tempbuffer, "y", y);
	SerialiseFloat(xmlMessage, tempbuffer, "z", z);
	SerialiseTag(xmlMessage, tempbuffer, elementName, false);
}

void audAmbientAudioEntity::SerialiseTag(char* xmlMessage, char* tempbuffer, const char* elementName, bool start)
{
	if(start)
	{
		sprintf(tempbuffer, "			<%s>\n", elementName);
	}
	else
	{
		sprintf(tempbuffer, "			</%s>\n", elementName);
	}
	
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
}

void audAmbientAudioEntity::SerialiseMessageStart(char* xmlMessage, char* tempbuffer, const char* objectType, const char* objectName)
{
	sprintf(xmlMessage, "<RAVEMessage>\n");
	naDisplayf("%s", xmlMessage);
	sprintf(tempbuffer, "	<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", atStringHash("BASE"));
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
	sprintf(tempbuffer, "		<%s name=\"%s\">\n", objectType, objectName);
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
}

void audAmbientAudioEntity::SerialiseMessageEnd(char* xmlMessage, char* tempbuffer, const char* objectType)
{
	sprintf(tempbuffer, "		</%s>\n", objectType);
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
	sprintf(tempbuffer, "	</EditObjects>\n");
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
	sprintf(tempbuffer, "</RAVEMessage>\n");
	naDisplayf("%s", tempbuffer);
	strcat(xmlMessage, tempbuffer);
}

const char *FindVariableName(const u32 hash)
{
	const char *varNames[] = {
		"Game.Clock.DayOfWeek"
		"Game.Clock.Hours"
		"Game.Clock.Minutes"
		"Game.Clock.Seconds"
		"Game.Clock.Days"
		"Game.Clock.Months"
		"Game.Clock.SecsPerMin"
		"Game.Weather.Wind.Temperature"
		"Game.Weather.Wind.Blustery"
		"Game.Weather.Wind.Strength"
		"Game.Player.Position.x"
		"Game.Player.Position.y"
		"Game.Player.Position.z"
		"Game.Ambience.PedDensity"
		"Game.Player.Speed"
		"Game.Weather.Rain.Level"
		"Game.Weather.Rain.LinearVolume"
		"Game.Weather.RoadWetness"
		"Game.Clock.DecimalHours"
		"Game.Clock.TimeWarp"
		"Game.Player.Health"
		"Game.Player.MaxHealth"
		"Game.Microphone.Position.x"
		"Game.Microphone.Position.y"
		"Game.Microphone.Position.z"
		"Game.Ambience.AverageCarSpeed"
		"Game.Water.CameraDepth"
		"Game.Ambience.PedDensityHostile"
		"Game.Player.BreathState"
		"Game.Audio.RMSLevels.Master"
		"Game.Audio.RMSLevels.SFX"
		"Game.Audio.RMSLevels.SFXSmoothed"
		"Game.Audio.RMSLevels.MasterSmoothed"
		"Game.Lighting.CurrentExposure"
		"Game.Lighting.TargetExposure"
		"Game.Lighting.SunVisibility"
		"Game.Player.InteriorRatio"
	};

	const u32 numVarNames = sizeof(varNames) / sizeof(varNames[0]);
	for(u32 i = 0 ; i < numVarNames; i++)
	{
		// NOTE: this is a slow search but that's fine as it's only used for serialization
		if(atStringHash(varNames[i]) == hash)
		{
			return varNames[i];
		}
	}
	return NULL;
}

void audAmbientAudioEntity::DuplicateEmitterAtCurrentCoords()
{
	UpdateEmitterToCurrentCoords(g_DuplicateEmitterName, g_CreateEmitterName);
}

void audAmbientAudioEntity::MoveEmitterToCurrentCoords()
{
	UpdateEmitterToCurrentCoords(g_CreateEmitterName, g_CreateEmitterName);
}

void audAmbientAudioEntity::UpdateEmitterToCurrentCoords(const char* sourceEmitterName, const char* savedEmitterName)
{
	if(sourceEmitterName && savedEmitterName)
	{
		StaticEmitter* emitterData = audNorthAudioEngine::GetObject<StaticEmitter>(sourceEmitterName);

		if(!emitterData)
		{
			naDisplayf("Could not find an emitter named %s!", g_CreateEmitterName);
			return;
		}

		if(!audVerifyf(sizeof(StaticEmitter) == 92, "Static emitter data size has changed (currently %" SIZETFMT "d)! Export function needs updating!", sizeof(StaticEmitter)))
		{
			return;
		}

		// Check the number of flags hasn't changed - if so, the export function needs updating
		CompileTimeAssert(FLAG_ID_STATICEMITTER_MAX == 12);

		Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
		char xmlMsg[4096];
		char tmpBuf[256] = {0};

		SerialiseMessageStart(xmlMsg, tmpBuf, "StaticEmitter", savedEmitterName);
		SerialiseSoundName(xmlMsg, tmpBuf, "Sound", emitterData->Sound, true);
		SerialiseObjectName<RadioStationSettings>(xmlMsg, tmpBuf, "RadioStation", emitterData->RadioStation);
		SerialiseVector(xmlMsg, tmpBuf, "Position", listenerPosition);
		SerialiseFloat(xmlMsg, tmpBuf, "MinDistance", emitterData->MinDistance);
		SerialiseFloat(xmlMsg, tmpBuf, "MaxDistance", emitterData->MaxDistance);
		SerialiseInt(xmlMsg, tmpBuf, "EmittedVolume", emitterData->EmittedVolume);
		SerialiseInt(xmlMsg, tmpBuf, "FilterCutoff", emitterData->FilterCutoff);
		SerialiseInt(xmlMsg, tmpBuf, "HPFCutoff", emitterData->HPFCutoff);
		SerialiseInt(xmlMsg, tmpBuf, "RolloffFactor", emitterData->RolloffFactor);
		SerialiseObjectName<InteriorSettings>(xmlMsg, tmpBuf, "InteriorSettings", emitterData->InteriorSettings);
		SerialiseObjectName<InteriorRoom>(xmlMsg, tmpBuf, "InteriorRoom", emitterData->InteriorRoom);
		SerialiseObjectName<RadioStationSettings>(xmlMsg, tmpBuf, "RadioStationForScore", emitterData->RadioStationForScore);
		SerialiseBool(xmlMsg, tmpBuf, "Enabled", emitterData->Flags, FLAG_ID_STATICEMITTER_ENABLED);
		SerialiseBool(xmlMsg, tmpBuf, "GeneratesBeats", emitterData->Flags, FLAG_ID_STATICEMITTER_GENERATESBEATS);
		SerialiseBool(xmlMsg, tmpBuf, "LinkedToProp", emitterData->Flags, FLAG_ID_STATICEMITTER_LINKEDTOPROP);
		SerialiseBool(xmlMsg, tmpBuf, "ConedFront", emitterData->Flags, FLAG_ID_STATICEMITTER_CONEDFRONT);
		SerialiseBool(xmlMsg, tmpBuf, "FillsInteriorSpace", emitterData->Flags, FLAG_ID_STATICEMITTER_FILLSINTERIORSPACE);
		SerialiseFloat(xmlMsg, tmpBuf, "MaxLeakage", emitterData->MaxLeakage);
		SerialiseInt(xmlMsg, tmpBuf, "MinLeakageDistance", emitterData->MinLeakageDistance);
		SerialiseInt(xmlMsg, tmpBuf, "MaxLeakageDistance", emitterData->MaxLeakageDistance);
		SerialiseBool(xmlMsg, tmpBuf, "PlayFullRadio", emitterData->Flags, FLAG_ID_STATICEMITTER_PLAYFULLRADIO);
		SerialiseBool(xmlMsg, tmpBuf, "MuteForScore", emitterData->Flags, FLAG_ID_STATICEMITTER_MUTEFORSCORE);
		SerialiseObjectName<AlarmSettings>(xmlMsg, tmpBuf, "AlarmSettings", emitterData->AlarmSettings);
		SerialiseBool(xmlMsg, tmpBuf, "DisabledByScript", emitterData->Flags, FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT);
		SerialiseSoundName(xmlMsg, tmpBuf, "OnBreakOneShot", emitterData->OnBreakOneShot, true);
		SerialiseInt(xmlMsg, tmpBuf, "MaxPathDepth", emitterData->MaxPathDepth);
		SerialiseInt(xmlMsg, tmpBuf, "SmallReverbSend", emitterData->SmallReverbSend);
		SerialiseInt(xmlMsg, tmpBuf, "MediumReverbSend", emitterData->MediumReverbSend);
		SerialiseInt(xmlMsg, tmpBuf, "LargeReverbSend", emitterData->LargeReverbSend);
	
		const f32 mintimeHours = Floorf(emitterData->MinTimeMinutes/60.0f);
		const f32 minTimeMinutesFraction = (emitterData->MinTimeMinutes - (mintimeHours * 60.0f))/60.0f;

		const f32 maxtimeHours = Floorf(emitterData->MaxTimeMinutes/60.0f);
		const f32 maxTimeMinutesFraction = (emitterData->MaxTimeMinutes - (maxtimeHours * 60.0f))/60.0f;

		SerialiseFloat(xmlMsg, tmpBuf, "MinTime", mintimeHours + minTimeMinutesFraction);
		SerialiseFloat(xmlMsg, tmpBuf, "MaxTime", maxtimeHours + maxTimeMinutesFraction);
		SerialiseBool(xmlMsg, tmpBuf, "StartStopImmediatelyAtTimeLimits", emitterData->Flags, FLAG_ID_STATICEMITTER_STARTSTOPIMMEDIATELYATTIMELIMITS);
		SerialiseFloat(xmlMsg, tmpBuf, "BrokenHealth", emitterData->BrokenHealth);
		SerialiseFloat(xmlMsg, tmpBuf, "UndamagedHealth", emitterData->UndamagedHealth);
		SerialiseBool(xmlMsg, tmpBuf, "MuteForFrontendRadio", emitterData->Flags, FLAG_ID_STATICEMITTER_MUTEFORFRONTENDRADIO);
		SerialiseBool(xmlMsg, tmpBuf, "ForceMaxPathDepth", emitterData->Flags, FLAG_ID_STATICEMITTER_FORCEMAXPATHDEPTH);
		SerialiseBool(xmlMsg, tmpBuf, "IgnoreSniper", emitterData->Flags, FLAG_ID_STATICEMITTER_IGNORESNIPER);
		SerialiseMessageEnd(xmlMsg, tmpBuf, "StaticEmitter");

		naDisplayf("XML message is %d characters long \n", istrlen(xmlMsg));

		audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
		bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
		naAssertf(success, "Failed to send xml message to rave");
		naDisplayf("%s", (success)? "Success":"Failed");

		g_EmitterAudioEntity.UpdateStaticEmitterPosition(emitterData);
	}
}

void audAmbientAudioEntity::MoveRuleToCurrentCoords(bool interiorRelative)
{
	if(g_CreateRuleName[0] != 0)
	{
		AmbientRule* ruleData = audNorthAudioEngine::GetObject<AmbientRule>(g_CreateRuleName);

		if(!ruleData)
		{
			naDisplayf("Could not find a rule named %s!", g_CreateRuleName);
			return;
		}

		if(!audVerifyf(sizeof(AmbientRule) == 144, "Ambient rule data size has changed (currently %" SIZETFMT "d)! Export function needs updating!", sizeof(AmbientRule)))
		{
			return;
		}

		// Check the number of flags hasn't changed - if so, the export function needs updating
		CompileTimeAssert(FLAG_ID_AMBIENTRULE_MAX == 12);

		Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());

		if(interiorRelative)
		{
			CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

			if(pIntInst)
			{
				listenerPosition = VEC3V_TO_VECTOR3(pIntInst->GetTransform().UnTransform(RCC_VEC3V(listenerPosition)));
			}
			else
			{
				naDisplayf("Cannot generate interior rule because there is no interior currently active!");
				return;
			}
		}

		char xmlMsg[4096];
		char tmpBuf[256] = {0};

		const f32 mintimeHours = Floorf(ruleData->MinTimeMinutes/60.0f);
		const f32 minTimeMinutesFraction = (ruleData->MinTimeMinutes - (mintimeHours * 60.0f))/60.0f;

		const f32 maxtimeHours = Floorf(ruleData->MaxTimeMinutes/60.0f);
		const f32 maxTimeMinutesFraction = (ruleData->MaxTimeMinutes - (maxtimeHours * 60.0f))/60.0f;

		SerialiseMessageStart(xmlMsg, tmpBuf, "AmbientRule", g_CreateRuleName);
		SerialiseFloat(xmlMsg, tmpBuf, "Weight", ruleData->Weight);
		SerialiseFloat(xmlMsg, tmpBuf, "MinDist", ruleData->MinDist);
		SerialiseFloat(xmlMsg, tmpBuf, "MaxDist", ruleData->MaxDist);
		SerialiseFloat(xmlMsg, tmpBuf, "MinTime", mintimeHours + minTimeMinutesFraction);
		SerialiseFloat(xmlMsg, tmpBuf, "MaxTime", maxtimeHours + maxTimeMinutesFraction);
		SerialiseInt(xmlMsg, tmpBuf, "MinRepeatTime", ruleData->MinRepeatTime);
		SerialiseInt(xmlMsg, tmpBuf, "MinRepeatTimeVariance", ruleData->MinRepeatTimeVariance);

		const Sound* soundPtr = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataPtr(ruleData->Sound);

		if(soundPtr)
		{
			const audMetadataManager &metadataManager = SOUNDFACTORY.GetMetadataManager();
			SerialiseString(xmlMsg, tmpBuf, "Sound", metadataManager.GetObjectNameFromNameTableOffset(soundPtr->NameTableOffset));
		}
		else
		{
			SerialiseString(xmlMsg, tmpBuf, "Sound", "NULL_SOUND");
		}

		const audCategory* category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ruleData->Category);

		if(category)
		{
			SerialiseString(xmlMsg, tmpBuf, "Category", category->GetNameString());
		}
		else
		{
			SerialiseString(xmlMsg, tmpBuf, "Category", "BASE");
		}

		SerialiseBool(xmlMsg, tmpBuf, "IgnoreInitialMinRepeatTime", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_IGNOREINITIALMINREPEATTIME) == AUD_TRISTATE_TRUE);
		SerialiseBool(xmlMsg, tmpBuf, "StopWhenRaining", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_STOPWHENRAINING) == AUD_TRISTATE_TRUE);
		SerialiseBool(xmlMsg, tmpBuf, "StopOnLoudSound", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_STOPONLOUDSOUND) == AUD_TRISTATE_TRUE);
		SerialiseBool(xmlMsg, tmpBuf, "FollowListener", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_FOLLOWLISTENER) == AUD_TRISTATE_TRUE);

		switch(ruleData->SpawnHeight)
		{
		case AMBIENCE_HEIGHT_RANDOM:
			SerialiseString(xmlMsg, tmpBuf, "SpawnHeight", "AMBIENCE_HEIGHT_RANDOM");
			break;
		case AMBIENCE_LISTENER_HEIGHT:
			SerialiseString(xmlMsg, tmpBuf, "SpawnHeight", "AMBIENCE_LISTENER_HEIGHT");
			break;
		case AMBIENCE_WORLD_BLANKET_HEIGHT:
			SerialiseString(xmlMsg, tmpBuf, "SpawnHeight", "AMBIENCE_WORLD_BLANKET_HEIGHT");
			break;
		}

		SerialiseVector(xmlMsg, tmpBuf, "ExplicitSpawnPosition", listenerPosition);

		if(interiorRelative)
		{
			SerialiseString(xmlMsg, tmpBuf, "ExplicitSpawnPositionUsage", "RULE_EXPLICIT_SPAWN_POS_INTERIOR_RELATIVE");
		}
		else
		{
			SerialiseString(xmlMsg, tmpBuf, "ExplicitSpawnPositionUsage", "RULE_EXPLICIT_SPAWN_POS_WORLD_RELATIVE");
		}

		for(u32 loop = 0; loop < ruleData->numConditions; loop++)
		{
			SerialiseTag(xmlMsg, tmpBuf, "Condition", true);

			// This isn't ideal, but variables are only stored as hashes
			const char *varName = FindVariableName(ruleData->Condition[loop].ConditionVariable);
			if(varName)
			{
				SerialiseString(xmlMsg, tmpBuf, "ConditionVariable", varName);
			}
			else
			{
				audAssertf(false, "Unrecognised condition variable for rule %s - aborting so that we don't accidentally lose settings", g_CreateRuleName);
				return;
			}			
			
			switch(ruleData->Condition[loop].ConditionType)
			{
			case RULE_IF_CONDITION_LESS_THAN:
				SerialiseString(xmlMsg, tmpBuf, "ConditionType", "RULE_IF_CONDITION_LESS_THAN");
				break;
			case RULE_IF_CONDITION_LESS_THAN_OR_EQUAL_TO:
				SerialiseString(xmlMsg, tmpBuf, "ConditionType", "RULE_IF_CONDITION_LESS_THAN_OR_EQUAL_TO");
				break;
			case RULE_IF_CONDITION_GREATER_THAN:
				SerialiseString(xmlMsg, tmpBuf, "ConditionType", "RULE_IF_CONDITION_GREATER_THAN");
				break;
			case RULE_IF_CONDITION_GREATER_THAN_OR_EQUAL_TO:
				SerialiseString(xmlMsg, tmpBuf, "ConditionType", "RULE_IF_CONDITION_GREATER_THAN_OR_EQUAL_TO");
				break;
			case RULE_IF_CONDITION_EQUAL_TO:
				SerialiseString(xmlMsg, tmpBuf, "ConditionType", "RULE_IF_CONDITION_EQUAL_TO");
				break;
			case RULE_IF_CONDITION_NOT_EQUAL_TO:
				SerialiseString(xmlMsg, tmpBuf, "ConditionType", "RULE_IF_CONDITION_NOT_EQUAL_TO");
				break;
			default:
				audAssertf(false, "Unrecognised condition type for rule %s - aborting so that we don't accidentally lose settings", g_CreateRuleName);
				return;
			}
			
			SerialiseFloat(xmlMsg, tmpBuf, "ConditionValue", ruleData->Condition[loop].ConditionValue);

			switch(ruleData->Condition[loop].BankLoading)
			{
			case INFLUENCE_BANK_LOAD:
				SerialiseString(xmlMsg, tmpBuf, "BankLoading", "INFLUENCE_BANK_LOAD");
				break;
			case DONT_INFLUENCE_BANK_LOAD:
				SerialiseString(xmlMsg, tmpBuf, "BankLoading", "DONT_INFLUENCE_BANK_LOAD");
				break;
			default:
				audAssertf(false, "Unrecognised bank load type for rule %s - aborting so that we don't accidentally lose settings", g_CreateRuleName);
				return;
			}
			
			SerialiseTag(xmlMsg, tmpBuf, "Condition", false);
		}

		SerialiseInt(xmlMsg, tmpBuf, "MaxLocalInstances", ruleData->MaxLocalInstances);
		SerialiseInt(xmlMsg, tmpBuf, "MaxGlobalInstances", ruleData->MaxGlobalInstances);
		SerialiseInt(xmlMsg, tmpBuf, "BlockabilityFactor", ruleData->BlockabilityFactor);
		SerialiseBool(xmlMsg, tmpBuf, "UseOcclusion", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_USEOCCLUSION) == AUD_TRISTATE_TRUE);
		SerialiseBool(xmlMsg, tmpBuf, "StreamingSound", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_STREAMINGSOUND) == AUD_TRISTATE_TRUE);
		SerialiseBool(xmlMsg, tmpBuf, "StopOnZoneDeactivation", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_STOPONZONEDEACTIVATION) == AUD_TRISTATE_TRUE);
		SerialiseBool(xmlMsg, tmpBuf, "TriggerOnLoudSound", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_TRIGGERONLOUDSOUND) == AUD_TRISTATE_TRUE);
		SerialiseBool(xmlMsg, tmpBuf, "CanTriggerOverWater", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_CANTRIGGEROVERWATER) == AUD_TRISTATE_TRUE);
		SerialiseBool(xmlMsg, tmpBuf, "CheckConditionsEachFrame", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_CHECKCONDITIONSEACHFRAME) == AUD_TRISTATE_TRUE);
		SerialiseInt(xmlMsg, tmpBuf, "MaxPathDepth", ruleData->MaxPathDepth);
		SerialiseBool(xmlMsg, tmpBuf, "UsePlayerPosition", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_USEPLAYERPOSITION) == AUD_TRISTATE_TRUE);
		SerialiseBool(xmlMsg, tmpBuf, "DisableInMultiplayer", AUD_GET_TRISTATE_VALUE(ruleData->Flags, FLAG_ID_AMBIENTRULE_DISABLEINMULTIPLAYER) == AUD_TRISTATE_TRUE);
		SerialiseMessageEnd(xmlMsg, tmpBuf, "AmbientRule");

		naDisplayf("XML message is %d characters long \n", istrlen(xmlMsg));

		audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
		bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
		naAssertf(success, "Failed to send xml message to rave");
		naDisplayf("%s", (success)? "Success":"Failed");
	}
}

void audAmbientAudioEntity::CreateRuleAtCurrentCoords(bool interiorRelative)
{
	if(g_CreateRuleName[0] != 0)
	{
		Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());

		if(interiorRelative)
		{
			CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

			if(pIntInst)
			{
				listenerPosition = VEC3V_TO_VECTOR3(pIntInst->GetTransform().UnTransform(RCC_VEC3V(listenerPosition)));
			}
			else
			{
				naDisplayf("Cannot generate interior rule because there is no interior currently active!");
				return;
			}
		}

		char xmlMsg[4096];
		char tmpBuf[256] = {0};

		SerialiseMessageStart(xmlMsg, tmpBuf, "AmbientRule", g_CreateRuleName);
		SerialiseVector(xmlMsg, tmpBuf, "ExplicitSpawnPosition", listenerPosition);
		SerialiseInt(xmlMsg, tmpBuf, "MaxGlobalInstances", 1);
		SerialiseFloat(xmlMsg, tmpBuf, "MinDist", 0.0f);

		if(interiorRelative)
		{
			SerialiseString(xmlMsg, tmpBuf, "ExplicitSpawnPositionUsage", "RULE_EXPLICIT_SPAWN_POS_INTERIOR_RELATIVE");
		}
		else
		{
			SerialiseString(xmlMsg, tmpBuf, "ExplicitSpawnPositionUsage", "RULE_EXPLICIT_SPAWN_POS_WORLD_RELATIVE");
		}

		SerialiseMessageEnd(xmlMsg, tmpBuf, "AmbientRule");
		naDisplayf("XML message is %" SIZETFMT "d characters long \n", strlen(xmlMsg));

		audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
		bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
		naAssertf(success, "Failed to send xml message to rave");
		naDisplayf("%s", (success)? "Success":"Failed");
	}
}

void audAmbientAudioEntity::CreateEmitterAtCurrentCoords()
{
	if(g_CreateEmitterName[0] != 0)
	{
		Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());

		char xmlMsg[4096];
		char tmpBuf[256] = {0};

		SerialiseMessageStart(xmlMsg, tmpBuf, "StaticEmitter", g_CreateEmitterName);
		SerialiseVector(xmlMsg, tmpBuf, "Position", listenerPosition);
		SerialiseMessageEnd(xmlMsg, tmpBuf, "StaticEmitter");
		naDisplayf("XML message is %" SIZETFMT "d characters long \n", strlen(xmlMsg));

		audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
		bool success = rc.SendXmlMessage(xmlMsg, istrlen(xmlMsg));
		naAssertf(success, "Failed to send xml message to rave");
		naDisplayf("%s", (success)? "Success":"Failed");
	}
}

void audAmbientAudioEntity::DuplicatedEditedZone()
{
	if(g_CreateZoneName[0] != 0)
	{
		UpdateRaveZone(g_CurrentEditZone, g_CreateZoneName);
	}
}

void audAmbientAudioEntity::CreateZoneForCurrentInteriorRoom()
{
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

	if(pIntInst)
	{
		s32 roomIndex = CPortalVisTracker::GetPrimaryRoomIdx();

		if(roomIndex < pIntInst->GetNumRooms())
		{
			const char* roomName = pIntInst->GetRoomName(roomIndex);

			if(roomName && stricmp("limbo", roomName) != 0)
			{
				Vector3 activationCenter;
				Vector3 activationSize;
				Vector3 positioningCenter;
				Vector3 positioningSize;

				spdAABB roomBoundingBox;
				pIntInst->CalcRoomBoundBox(roomIndex, roomBoundingBox);

				positioningCenter = VEC3V_TO_VECTOR3(roomBoundingBox.GetCenter());
				positioningSize = 2 * VEC3V_TO_VECTOR3(roomBoundingBox.GetExtent());

				activationCenter = VEC3V_TO_VECTOR3(roomBoundingBox.GetCenter());
				activationSize = 2 * VEC3V_TO_VECTOR3(roomBoundingBox.GetExtent());

				char tmpBuf[256] = {0};
				sprintf(tmpBuf, "IZ_%s_%s", pIntInst->GetModelName(), roomName);

				CreateZone(true, tmpBuf, positioningCenter, positioningSize, 0, activationCenter, activationSize, 0);
			}
		}
	}
}

void audAmbientAudioEntity::ToggleEditZone()
{	
	if(g_CurrentEditZone)
	{
		g_ZoneEditorActive = false;
		g_CurrentEditZone = NULL;

		camDebugDirector& debugDirector = camInterface::GetDebugDirector();
		debugDirector.SetShouldIgnoreDebugPadCameraToggle(false);
		return;
	}

	if(g_EditZoneFilter[0] != 0)
	{
		for(s32 loop = 0; loop < m_InteriorZones.GetCount(); loop++)
		{
			if(strcmp(m_InteriorZones[loop].GetName(), g_EditZoneFilter) == 0)
			{
				g_CurrentEditZone = &m_InteriorZones[loop];
				return;
			}
		}

		for(s32 loop = 0; loop < m_Zones.GetCount(); loop++)
		{
			if(strcmp(m_Zones[loop]->GetName(), g_EditZoneFilter) == 0 )
			{
				g_CurrentEditZone = m_Zones[loop];
				return;
			}
		}
	}
}

void audAmbientAudioEntity::ApplyEditedZoneChangesToRAVE()
{
	UpdateRaveZone(g_CurrentEditZone);
}

void audAmbientAudioEntity::SerialiseShoreLines()
{
	if(naVerifyf(m_DebugShoreLine,"Can't add points without having a shoreline. Please add a shoreline first"))
	{	
		naDisplayf("Serialising shoreline....");
		m_DebugShoreLine->Serialise(s_ShoreLineName,sm_ShoreLineEditorState == STATE_EDITING);
		// By doing the serialise call twice we get the rave callback and we can add the shore in real time. 
		m_DebugShoreLine->Serialise(s_ShoreLineName,sm_ShoreLineEditorState == STATE_EDITING);
	}
}

void audAmbientAudioEntity::AddDebugShorePoint()
{
	if(naVerifyf(m_DebugShoreLine,"Can't add points without having a shoreline. Please add a shoreline first"))
	{	
		if(CPauseMenu::IsActive())
		{
			m_DebugShoreLine->AddDebugShorePoint(CMiniMap::GetPauseMapCursor());
		}
		else
		{
			Vector3 listPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
			Vector2 pos(listPos.x, listPos.y);
			m_DebugShoreLine->AddDebugShorePoint(pos);
			m_CurrentDebugShoreLineIndex ++;
		}
	}
}

void audAmbientAudioEntity::AddDebugShore()
{
	if( sm_ShoreLineEditorState == STATE_WAITING_FOR_ACTION &&  !m_DebugShoreLine)
	{
		switch(sm_ShoreLineType)
		{
		case AUD_WATER_TYPE_POOL:
			m_DebugShoreLine = rage_new audShoreLinePool();
			break;
		case AUD_WATER_TYPE_RIVER:
			m_DebugShoreLine = rage_new audShoreLineRiver();
			break;
		case AUD_WATER_TYPE_OCEAN:
			m_DebugShoreLine = rage_new audShoreLineOcean();
			break;
		case AUD_WATER_TYPE_LAKE:
			m_DebugShoreLine = rage_new audShoreLineLake();
			break;
		default:
			break;
		}
		m_DebugShoreLine->InitDebug();
		sm_ShoreLineEditorState = STATE_ADDING;
		m_CurrentDebugShoreLineIndex = 0;
		naDisplayf("Adding shoreline, please set up the type, parameters and points and serialize the shoreline to finish");
	}
	else 
	{
		naAssertf(false,"Can't create a new shoreline while doing other stuff, please finish the current work first and try again.");
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audAmbientAudioEntity::AddShoreLineTypeWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Audio");
	bkWidget* waterTypeWidget =  BANKMGR.FindWidget("Audio/AmbientAudio/Water/Shorelines/ShoreLine Editor/" ) ;
	if(waterTypeWidget)
	{
		u32 numTypes = (u32)NUM_WATERTYPE;
		rage::bkCombo* waterTypeCombo = bank->AddCombo("Water type", &sm_ShoreLineType,numTypes, NULL);
		if (waterTypeCombo != NULL)
		{
			for(u32 i=AUD_WATER_TYPE_POOL; i < numTypes; i++)
			{
				switch (i)
				{
				case AUD_WATER_TYPE_POOL:
					waterTypeCombo->SetString(i, "POOL");
					break;
				case AUD_WATER_TYPE_RIVER:
					waterTypeCombo->SetString(i, "RIVER");
					break;
				case AUD_WATER_TYPE_LAKE:
					waterTypeCombo->SetString(i, "LAKE");
					break;
				case AUD_WATER_TYPE_OCEAN:
					waterTypeCombo->SetString(i, "OCEAN");
					break;
				default:
					naAssertf(false,"Wrong water type");
					break;
				}
			}
		}
	}
}
void audAmbientAudioEntity::EditShoreLine()
{
	if(naVerifyf(!m_DebugShoreLine,"Can't edit a shoreline while working with another one. Please one at a time."))
	{
		const ShoreLineList *list = audNorthAudioEngine::GetObject<ShoreLineList>(ATSTRINGHASH("ShoreLineList", 0xB855DFE1));
		if(list)
		{
			audMetadataObjectInfo info;
			if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(list->ShoreLines[s_ShorelineToEdit].ShoreLine,info))
			{
				switch(info.GetType())
				{
					case ShoreLinePoolAudioSettings::TYPE_ID:
						m_DebugShoreLine = rage_new audShoreLinePool(info.GetObject<ShoreLinePoolAudioSettings>());
						sm_ShoreLineType = AUD_WATER_TYPE_POOL;
						break;
					case ShoreLineRiverAudioSettings::TYPE_ID:
						m_DebugShoreLine = rage_new audShoreLineRiver(info.GetObject<ShoreLineRiverAudioSettings>());
						sm_ShoreLineType = AUD_WATER_TYPE_RIVER;
						break;
					case ShoreLineOceanAudioSettings::TYPE_ID:
						m_DebugShoreLine = rage_new audShoreLineOcean(info.GetObject<ShoreLineOceanAudioSettings>());
						sm_ShoreLineType = AUD_WATER_TYPE_OCEAN;
						break;
					case ShoreLineLakeAudioSettings::TYPE_ID:
						m_DebugShoreLine = rage_new audShoreLineLake(info.GetObject<ShoreLineLakeAudioSettings>());
						sm_ShoreLineType = AUD_WATER_TYPE_LAKE;
						break;
					default:
						naAssertf(false,"Wrong shoreline type when editing.");
					break;
				}
				m_DebugShoreLine->InitDebug();
				formatf(s_ShoreLineName,sizeof(s_ShoreLineName),"%s",info.GetName());
				sm_ShoreLineEditorState = STATE_EDITING;
				m_CurrentDebugShoreLineIndex  = 0;
				sm_DrawShoreLines = false;
				sm_DrawPlayingShoreLines = false;
				sm_WarpPlayerToShoreline = true;
			}
		}
	}
}
void audAmbientAudioEntity::WarpToShoreline()
{
	const ShoreLineList *list = audNorthAudioEngine::GetObject<ShoreLineList>(ATSTRINGHASH("ShoreLineList", 0xB855DFE1));
	if(list)
	{
		audMetadataObjectInfo info;
		if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(list->ShoreLines[s_ShorelineToEdit].ShoreLine,info))
		{
			Vector3 wrapPoint;
			switch(info.GetType())
			{
			case ShoreLinePoolAudioSettings::TYPE_ID:
				wrapPoint = Vector3(info.GetObject<ShoreLinePoolAudioSettings>()->ShoreLinePoints[0].X
					,info.GetObject<ShoreLinePoolAudioSettings>()->ShoreLinePoints[0].Y
					,100.f);
				break;
			case ShoreLineRiverAudioSettings::TYPE_ID:
				wrapPoint = Vector3(info.GetObject<ShoreLineRiverAudioSettings>()->ShoreLinePoints[0].X
					,info.GetObject<ShoreLineRiverAudioSettings>()->ShoreLinePoints[0].Y
					,100.f);
				break;
			case ShoreLineOceanAudioSettings::TYPE_ID:
				wrapPoint = Vector3(info.GetObject<ShoreLineOceanAudioSettings>()->ShoreLinePoints[0].X
					,info.GetObject<ShoreLineOceanAudioSettings>()->ShoreLinePoints[0].Y
					,100.f);
				break;
			case ShoreLineLakeAudioSettings::TYPE_ID:
				wrapPoint = Vector3(info.GetObject<ShoreLineLakeAudioSettings>()->ShoreLinePoints[0].X
					,info.GetObject<ShoreLineLakeAudioSettings>()->ShoreLinePoints[0].Y
					,100.f);
				break;
			default:
				naAssertf(false,"Wrong shoreline type when editing.");
				break;
			}
			CWarpManager::SetWarp(wrapPoint, wrapPoint, 0.f, true, true, 600.f);
		}
	}
}

void audAmbientAudioEntity::WarpToZone()
{
	AmbientZone *zone = audNorthAudioEngine::GetObject<AmbientZone>(g_EditZoneFilter);
	if(zone)
	{
		CWarpManager::SetWarp(zone->PositioningZone.Centre, Vector3(0, 0, 0), 0.f, true, true, 600.f);
	}
}

void audAmbientAudioEntity::UpdateEditorAddingMode()
{
	Vector3 worldPos;
	char txt[64];
	formatf(txt,"Adding shoreline");
	grcDebugDraw::AddDebugOutput(Color_white,txt);
	naAssertf(m_DebugShoreLine, "to be able to add points you first need to add the shoreline.");
	//MODE_USE_CAMERA, handle via rag callbacks 
	if(!sm_AddingShorelineActivationBox)
	{
		if (CDebugScene::GetMouseLeftPressed())// CLICK AND DRAG
		{
			sm_MouseLeftPressed = true;
			CDebugScene::GetWorldPositionUnderMouse(worldPos);
			m_DebugShoreLine->AddDebugShorePoint(Vector2(worldPos.GetX(),worldPos.GetY()));
		} 
		if(CDebugScene::GetMouseLeftReleased())
		{
			if(sm_MouseLeftPressed)
			{
				naDisplayf("Point %d added with coords X: %f Y: %f",m_CurrentDebugShoreLineIndex, worldPos.GetX(),worldPos.GetY());
				m_CurrentDebugShoreLineIndex++;
			}
			sm_MouseLeftPressed = false;
		} 
		if (sm_MouseLeftPressed)
		{
			CDebugScene::GetWorldPositionUnderMouse(worldPos);
			m_DebugShoreLine->EditShorelinePoint(m_CurrentDebugShoreLineIndex,worldPos);
		}
	}
	else 
	{
		// Update width and height 
		if(!sm_EditActivationBoxSize)
		{
			sm_DebugShoreLineWidth =  m_DebugShoreLine->GetActivationBoxAABB().GetWidth();
			sm_DebugShoreLineHeight = m_DebugShoreLine->GetActivationBoxAABB().GetWidth();
		}
		else
		{
			m_DebugShoreLine->SetActivationBoxSize(sm_DebugShoreLineWidth,sm_DebugShoreLineHeight);
		}
		// Centre mouse changes both
		if (CDebugScene::GetMouseMiddlePressed())
		{
			sm_MouseMiddlePressed = true;
			CDebugScene::GetWorldPositionUnderMouse(worldPos);
			m_DebugShoreLine->MoveActivationBox(Vector2(worldPos.GetX(),worldPos.GetY()));
		}
		if(CDebugScene::GetMouseMiddleReleased())
		{
			sm_MouseMiddlePressed = false;
		} 
		if (sm_MouseMiddlePressed)
		{
			CDebugScene::GetWorldPositionUnderMouse(worldPos);
			m_DebugShoreLine->MoveActivationBox(Vector2(worldPos.GetX(),worldPos.GetY()));
		}
		if (CDebugScene::GetMouseLeftPressed())
		{
			sm_MouseLeftPressed = true;
		}
		if(CDebugScene::GetMouseLeftReleased())
		{
			sm_MouseLeftPressed = false;
		} 
		if (sm_MouseLeftPressed)
		{
			m_DebugShoreLine->RotateActivationBox(1.f);
		}
		if (CDebugScene::GetMouseRightPressed())
		{
			sm_MouseRightPressed = true;
		}
		if(CDebugScene::GetMouseRightReleased())
		{
			sm_MouseRightPressed = false;
		} 
		if (sm_MouseRightPressed)
		{
			m_DebugShoreLine->RotateActivationBox(-1.f);
		}

	}
}
void audAmbientAudioEntity::UpdateEditorEditingMode()
{
	Vector3 worldPos;
	CPad& debugPad = CControlMgr::GetDebugPad();
	char txt[64];
	if (!sm_AddingShorelineActivationBox)
	{
		formatf(txt,"Editing shoreline, current node : %d", m_CurrentDebugShoreLineIndex);
		grcDebugDraw::AddDebugOutput(Color_green,txt);
		if(debugPad.DPadLeftJustDown())
		{
			m_CurrentDebugShoreLineIndex --;
			if(m_CurrentDebugShoreLineIndex < 0)
			{
				m_CurrentDebugShoreLineIndex = m_DebugShoreLine->GetNumPoints() - 1;
			}
			sm_EditWidth = false;
		}
		else if(debugPad.DPadRightJustDown())
		{
			m_CurrentDebugShoreLineIndex = (m_CurrentDebugShoreLineIndex + 1)%m_DebugShoreLine->GetNumPoints();
			sm_EditWidth = false;
		}
		if(CDebugScene::GetMouseLeftPressed())
		{
			sm_MouseLeftPressed = true;
		}
		if(CDebugScene::GetMouseLeftReleased())
		{
			sm_MouseLeftPressed = false;
		} 
		if (sm_MouseLeftPressed)
		{
			CDebugScene::GetWorldPositionUnderMouse(worldPos);
			m_DebugShoreLine->EditShorelinePoint(m_CurrentDebugShoreLineIndex,worldPos);
		}
		if(sm_EditWidth)
		{
			m_DebugShoreLine->EditShorelinePointWidth(m_CurrentDebugShoreLineIndex);
		}
	}
	else 
	{
		// Update width and height 
		if(!sm_EditActivationBoxSize && m_DebugShoreLine->GetSettings())
		{
			sm_DebugShoreLineWidth =  m_DebugShoreLine->GetSettings()->ActivationBox.Size.Width;
			sm_DebugShoreLineHeight =  m_DebugShoreLine->GetSettings()->ActivationBox.Size.Height;
		}
		else
		{
			m_DebugShoreLine->SetActivationBoxSize(sm_DebugShoreLineWidth,sm_DebugShoreLineHeight);
		}
		// Centre mouse changes both
		if (CDebugScene::GetMouseMiddlePressed())
		{
			sm_MouseMiddlePressed = true;
			CDebugScene::GetWorldPositionUnderMouse(worldPos);
			m_DebugShoreLine->MoveActivationBox(Vector2(worldPos.GetX(),worldPos.GetY()));
		}
		if(CDebugScene::GetMouseMiddleReleased())
		{
			sm_MouseMiddlePressed = false;
		} 
		if (sm_MouseMiddlePressed)
		{
			CDebugScene::GetWorldPositionUnderMouse(worldPos);
			m_DebugShoreLine->MoveActivationBox(Vector2(worldPos.GetX(),worldPos.GetY()));
		}
		if (CDebugScene::GetMouseLeftPressed())
		{
			sm_MouseLeftPressed = true;
		}
		if(CDebugScene::GetMouseLeftReleased())
		{
			sm_MouseLeftPressed = false;
		} 
		if (sm_MouseLeftPressed)
		{
			m_DebugShoreLine->RotateActivationBox(1.f);
		}
		if (CDebugScene::GetMouseRightPressed())
		{
			sm_MouseRightPressed = true;
		}
		if(CDebugScene::GetMouseRightReleased())
		{
			sm_MouseRightPressed = false;
		} 
		if (sm_MouseRightPressed)
		{
			m_DebugShoreLine->RotateActivationBox(-1.f);
		}
	}
}
void audAmbientAudioEntity::SetRiverWidthAtPoint()
{
	sm_EditWidth = true;
}
void audAmbientAudioEntity::UpdateShoreLineEditor()
{
#if __BANK
	if(sm_WarpPlayerToShoreline && m_DebugShoreLine)
	{
		Vector3 wrapPoint;
		if(m_DebugShoreLine->GetPointZ(0,wrapPoint))
		{
			CWarpManager::SetWarp(wrapPoint, wrapPoint, 0.f, true, true, 600.f);
			sm_WarpPlayerToShoreline = false;
		}
	}
#endif
	switch(sm_ShoreLineEditorState)
	{
	case STATE_ADDING:
		UpdateEditorAddingMode();
		break;
	case STATE_EDITING:
		UpdateEditorEditingMode();
		break;
	default:
		break;
	}
	if(m_DebugShoreLine)
	{
		m_DebugShoreLine->DrawShoreLines(false,m_CurrentDebugShoreLineIndex);
		if(sm_AddingShorelineActivationBox)
		{
			m_DebugShoreLine->DrawActivationBox();
		}
	}
}
void audAmbientAudioEntity::AddShoreLinesCombo()
{
	bkBank* bank = BANKMGR.FindBank("Audio");

	bkWidget* shoreLineWidget =  BANKMGR.FindWidget("Audio/AmbientAudio/Water/Shorelines/ShoreLine editor/" ) ;
	if(shoreLineWidget)
	{
		rage::bkCombo* pShorelineCombo = nullptr;
		const s32 numChunks = (s32)audNorthAudioEngine::GetMetadataManager().GetNumChunks();
		for (u32 chunkId = 0; chunkId < numChunks; chunkId++)
		{
			audMetadataObjectInfo info;
			if (audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(ATSTRINGHASH("ShoreLineList", 0xB855DFE1), chunkId, info))
			{
				const ShoreLineList *list = info.GetObject<ShoreLineList>();

				if (naVerifyf(list, "Couldn't find autogenerated shoreline list"))
				{
					if (!pShorelineCombo)
					{
						pShorelineCombo = bank->AddCombo("Shore lines", &s_ShorelineToEdit, list->numShoreLines, NULL);
					}
					
					if (pShorelineCombo != NULL)
					{
						for (int i = 0; i < list->numShoreLines; i++)
						{
							pShorelineCombo->SetString(i, audNorthAudioEngine::GetMetadataManager().GetObjectName(list->ShoreLines[i].ShoreLine));
						}
					}
				}
			}
		}
	}
}

void audAmbientAudioEntity::CancelShorelineWork()
{
	audShoreLine::Cancel();
	formatf(s_ShoreLineName,"");
	sm_ShoreLineType = AUD_WATER_TYPE_POOL;
	sm_ShoreLineEditorState = STATE_WAITING_FOR_ACTION;
	sm_AddingShorelineActivationBox = false;
	if(m_DebugShoreLine)
	{
		delete(m_DebugShoreLine);
		m_DebugShoreLine = NULL;
	}
}

void audAmbientAudioEntity::ResetDebugShoreActivationBox()
{
	if(m_DebugShoreLine)
	{
		m_DebugShoreLine->GetActivationBoxAABB().Init();
		for (u32 i = 0; i < m_DebugShoreLine->GetNumPoints(); i ++)
		{
			m_DebugShoreLine->GetActivationBoxAABB().Add(m_DebugShoreLine->GetPoint(i).x,m_DebugShoreLine->GetPoint(i).y);
		}
	}
}
void PrintMapCB()
{
	naDisplayf("Current Map Coordinates: (%f,%f)", CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y);
}
void AddShorePointCB()
{
	g_AmbientAudioEntity.AddDebugShorePoint();
}

void AddShoreCB()
{
	g_AmbientAudioEntity.AddDebugShore();
}
void WarpShorelineCB()
{
	g_AmbientAudioEntity.WarpToShoreline();
}
void WarpZoneCB()
{
	g_AmbientAudioEntity.WarpToZone();
}
void EditShorelineCB()
{
	g_AmbientAudioEntity.EditShoreLine();
	
}
void SerialiseShoreLinesCB()
{
	g_AmbientAudioEntity.SerialiseShoreLines();
}
void SetShorelinePointsCB()
{
	audAmbientAudioEntity::sm_AddingShorelineActivationBox = false;
}
void SetActivationBoxCB()
{
	audAmbientAudioEntity::sm_AddingShorelineActivationBox = true;
}
void ResetActivationBoxCB()
{
	g_AmbientAudioEntity.ResetDebugShoreActivationBox();
}
void CancelCB()
{
	g_AmbientAudioEntity.CancelShorelineWork();
}
void SelectNextDirectionalAmbienceCB()
{
	g_DirectionalAmbienceIndexToRender++;
}

void SelectPrevDirectionalAmbienceCB()
{
	g_DirectionalAmbienceIndexToRender--;
}

void audAmbientAudioEntity::CreateShorelineEditorWidgets()
{	
	if (s_ShorelineEditorGroup && s_ShoreLineEditorBank)
	{
		bkBank& bank = *s_ShoreLineEditorBank;
		bank.SetCurrentGroup(*s_ShorelineEditorGroup);

		if (s_CreateShorelineWidgetsButton)
		{
			s_CreateShorelineWidgetsButton->Destroy();
		}
		
		bank.AddTitle("Creator");
		bank.AddText("Name", s_ShoreLineName, sizeof(s_ShoreLineName), false);
		AddShoreLineTypeWidgets();
		bank.AddButton("Add Shore", CFA(AddShoreCB));
		bank.AddButton("Add Shore Point using camera", CFA(AddShorePointCB));
		bank.AddButton("Cancel Create", CFA(CancelCB));
		bank.AddToggle("Edit ActivationBox sizes", &sm_EditActivationBoxSize);
		bank.AddSlider("ActivationBox width", &sm_DebugShoreLineWidth, 0.f, 9999.f, 0.1f);
		bank.AddSlider("ActivationBox Height", &sm_DebugShoreLineHeight, 0.f, 9999.f, 0.1f);
		audShoreLine::AddEditorWidgets(bank);
		audShoreLinePool::AddEditorWidgets(bank);
		audShoreLineRiver::AddEditorWidgets(bank);
		audShoreLineOcean::AddEditorWidgets(bank);
		audShoreLineLake::AddEditorWidgets(bank);
		bank.AddTitle("Editor");
		g_AmbientAudioEntity.AddShoreLinesCombo();
		bank.AddButton("Warp player to selected shoreline", CFA(WarpShorelineCB));
		bank.AddButton("Edit selected shoreline", CFA(EditShorelineCB));
		bank.AddButton("Cancel Edit", CFA(CancelCB));
		bank.AddSeparator();
		bank.AddButton("Set shoreline points", CFA(SetShorelinePointsCB));
		bank.AddButton("Set activation box", CFA(SetActivationBoxCB));
		bank.AddButton("Reset activation box", CFA(ResetActivationBoxCB));
		bank.AddButton("Serialise Shore Lines", CFA(SerialiseShoreLinesCB));
	}	
}

// ----------------------------------------------------------------
// Add widgets to RAG tool
// ----------------------------------------------------------------
extern bool g_WarpToAmbientZone;
void audAmbientAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("AmbientAudio", false); 
		bank.AddToggle("Enable Ambient Audio", &g_EnableAmbientAudio);
		bank.PushGroup("Ambience Zones");
			bank.AddToggle("WarpToAmbientZone", &g_WarpToAmbientZone);
			bank.AddToggle("Stop All Ambient Rules", &g_StopAllRules);
			bank.AddCombo("Render Ambience Zones", &g_DrawAmbientZones, 4, &g_ZoneRenderOptionNames[0], 0, NullCB, "Choose what types of ambience zones should be rendered" );
			bank.AddToggle("Render Loud Sound Exclusion Sphere", &g_DrawLoudSoundExclusionSphere);
			bank.AddCombo("Zone Boundaries", &g_AmbientZoneRenderType, 3, &g_ZoneRenderModeOptionNames[0], 0, NullCB, "Choose how to render the zone boundaries" );
			bank.AddCombo("Activation Ranges", &g_ActivationZoneRenderType, 3, &g_ZoneRenderModeOptionNames[0], 0, NullCB, "Choose how to render the activation ranges" );
			bank.AddCombo("Zone Activation Point", &g_ZoneActivationType, 3, &g_ZoneActivationPointOptionNames[0], 0, NullCB, "Choose whether zones are activated based on the player of the listener position" );
			bank.AddSlider("Max Draw Distance", &g_ZoneDebugDrawDistance, -1, 8000, 1, NullCB, "The radius in which we draw any ambience zones. -1 = infinite" );
			bank.AddToggle("Draw Loaded Metadata Chunks", &g_DrawLoadedDLCChunks);
			bank.AddToggle("Draw Active Rule Information", &g_DrawRuleInfo);
			bank.AddSlider("Active Rule Scroll", &g_DrawRuleInfoScroll, 0.0f, 2.0f, 0.1f, NullCB, "Use this to scroll the active rule list" );
			bank.AddToggle("Draw Playing Sound Positions", &g_DrawPlayingSounds);
			bank.AddToggle("Draw Activation Zone AABBs", &g_DrawActivationZoneAABB);
			bank.AddToggle("Draw Active Quad Tree Nodes", &sm_DrawActiveQuadTreeNodes);
			bank.AddToggle("Draw Wave Slot Manager", &g_DrawWaveSlotManager);
			bank.AddToggle("Draw Interior Zones", &g_DrawInteriorZones);
			bank.AddToggle("Draw Active Mixer Scenes", &g_DrawActiveMixerScenes);
			bank.AddToggle("Force Disable Streaming", &g_ForceDisableStreaming);
			bank.AddText("Filter by Name", g_DrawZoneFilter, sizeof(g_DrawZoneFilter));
			bank.AddButton("Print Map Coords", CFA(PrintMapCB));
		bank.AddButton("Create Zone For Current Room", datCallback(MFA(audAmbientAudioEntity::CreateZoneForCurrentInteriorRoom), (datBase*)this), "");
		bank.PopGroup();

		bank.PushGroup("Zone Editing");
			bank.AddButton("Toggle Zone Editor On/Off", datCallback(MFA(audAmbientAudioEntity::ToggleEditZone), (datBase*)this), "Toggle editing of the selected zone");
			bank.AddSlider("Zone Movement Speed Multiplier", &g_ZoneEditorMovementSpeed, 0.01f, 10.0f, 0.1f, NullCB, "");
			bank.AddText("Zone To Edit", g_EditZoneFilter, sizeof(g_EditZoneFilter));
			bank.AddCombo("Edit Dimension", &g_EditZoneDimension, 4, &g_ZoneEditDimensionNames[0], 0, NullCB, "Choose how to edit the zone boundaries" );
			bank.AddCombo("Edit Attribute", &g_EditZoneAttribute, 3, &g_ZoneEditAttributeNames[0], 0, NullCB, "Choose which part of the zone to edit" );
			bank.AddCombo("Anchor Point", &g_EditZoneAnchor, 5, &g_ZoneEditAnchorNames[0], 0, NullCB, "Choose which part of the zone is locked in place" );
			bank.AddButton("Move Zone To Camera Coords", datCallback(MFA(audAmbientAudioEntity::MoveZoneToCameraCoords), (datBase*)this), "");
			bank.AddButton("Apply changes to RAVE", datCallback(MFA(audAmbientAudioEntity::ApplyEditedZoneChangesToRAVE), (datBase*)this), "");
			bank.AddButton("Warp player to selected zone", CFA(WarpZoneCB));
			bank.AddText("New Zone Name", g_CreateZoneName, sizeof(g_CreateZoneName));
			bank.AddButton("Create New Zone", datCallback(MFA(audAmbientAudioEntity::CreateZoneAtCurrentCoords), (datBase*)this), "");
			bank.AddButton("Duplicate Edited Zone", datCallback(MFA(audAmbientAudioEntity::DuplicatedEditedZone), (datBase*)this), "");
			bank.AddText("Rule Name", g_CreateRuleName, sizeof(g_CreateRuleName));
			bank.AddButton("Create New Rule At World Relative Coords", datCallback(MFA(audAmbientAudioEntity::CreateRuleAtWorldCoords), (datBase*)this), "");
			bank.AddButton("Create New Rule At Interior Relative Coords", datCallback(MFA(audAmbientAudioEntity::CreateRuleAtInteriorCoords), (datBase*)this), "");
			bank.AddButton("Move Existing Rule To World Relative Coords", datCallback(MFA(audAmbientAudioEntity::MoveRuleToWorldCoords), (datBase*)this), "");
			bank.AddButton("Move Existing Rule To Interior Relative Coords", datCallback(MFA(audAmbientAudioEntity::MoveRuleToInteriorCoords), (datBase*)this), "");
			bank.AddToggle("Enable Line Picker Tool", &g_EnableLinePickerTool);
			bank.AddToggle("Draw Line Picker Collision Sphere", &g_DrawLinePickerCollisionSphere);
			bank.AddToggle("Draw Zone Anchor Point", &g_DrawZoneAnchorPoint);
		bank.PopGroup();

		bank.PushGroup("Directional Ambience Managers");
			bank.AddToggle("Stop All Directional Ambiences", &g_StopAllDirectionalAmbiences);
			bank.AddToggle("Render Directional Managers", &g_DrawDirectionalManagers);
			bank.AddToggle("Draw Directional Manager Final Speaker Levels", &g_DrawDirectionalManagerFinalSpeakerLevels);
			bank.AddToggle("Ignore Blocked Factor", &g_DirectionalManagerIgnoreBlockage);
			bank.AddButton("Prev Manager", CFA(SelectPrevDirectionalAmbienceCB));
			bank.AddButton("Next Manager", CFA(SelectNextDirectionalAmbienceCB));
			bank.AddToggle("Toggle Mute Current Manager", &g_MuteDirectionalManager);
			bank.AddToggle("Headphone Leakage Enabled", &g_DirectionalAmbienceChannelLeakageEnabled);			
			bank.AddCombo("Render Direction", &g_DirectionalManagerRenderDir, 5, &g_AmbientDirectionNames[0], 0, NullCB, "Direction to render information for" );
			bank.AddToggle("Show Calculations For Direction", &g_ShowDirectionalAmbienceCalculations);
			bank.AddToggle("Force Sector Volume", &g_DirectionalManagerForceVol);
			bank.AddSlider("Forced Sector Volume", &g_DirectionalManagerForcedVol, 0.0f, 1.0f, 0.01f, NullCB, "" );
		bank.PopGroup();

		bank.PushGroup("Particle Effects");
			bank.AddToggle("Enable Particle FX", &g_SpawnEffectsEnabled);
			bank.AddToggle("Draw Registered Particle FX", &g_DrawRegisteredEffects);
			bank.AddToggle("Draw Playing Particle FX", &g_DrawSpawnFX);
		bank.PopGroup();

		bank.PushGroup("Highways");
			bank.AddToggle("Enable Highway Ambience", &g_EnableHighwayAmbience);
			bank.AddToggle("Render Nearest Highway Nodes", &g_DrawNearestHighwayNodes);
			bank.AddSlider("Highway Recalculation Distance", &g_HighwayRoadNodeRecalculationDist, 0.0f, 1000.0f, 1.0f, NullCB, "" );
			bank.AddSlider("Highway Search Min Distance", &g_HighwayRoadNodeSearchMinDist, 0.0f, 1000.0f, 1.0f, NullCB, "" );
			bank.AddSlider("Highway Search Max Distance", &g_HighwayRoadNodeSearchMaxDist, 0.0f, 1000.0f, 1.0f, NullCB, "" );
			bank.AddSlider("Highway Road Noise Cull Distance", &g_HighwayRoadNodeCullDist, 0.0f, 1000.0f, 1.0f, NullCB, "" );
			bank.AddToggle("Only search up for highways", &g_HighwayOnlySearchUp);
			bank.AddSlider("Highway Search Up Offset", &g_HighwayRoadNodeSearchUpOffset, 0.0f, 100.0f, 1.0f, NullCB, "" );
			bank.AddToggle("Play Highway Tyre Bumps", &g_PlayHighwayTyreBumps);
			bank.AddSlider("Highway Passby Speed", &g_HighwayPassbySpeed, 0.0f, 100.0f, 1.0f, NullCB, "" );
			bank.AddSlider("Highway Passby Lifetime", &g_HighwayPassbyLifeTime, 0.0f, 30.0f, 1.0f, NullCB, "" );
			bank.AddSlider("Highway Passby Tyre Front/Back Dist Scale", &g_HighwayPassbyTyreBumpFrontBackOffsetScale, 0.0f, 2.0f, 0.01f, NullCB, "" );
		bank.PopGroup();

		bank.PushGroup("Water");
			bank.PushGroup("Shorelines");
			bank.AddButton("Force Ocean Shoreline Update", datCallback(MFA(audAmbientAudioEntity::ForceOceanShorelineUpdateFromNearestRoad), (datBase*)this), "");
			bank.AddSlider("Camera dist to shore threshold", &sm_CameraDistanceToShoreThreshold, 0.0f, 10000000.0f, 0.1f);
			if(g_AudioEngine.GetRemoteControl().IsPresent())
			{
				s_ShoreLineEditorBank = &bank;
				s_ShorelineEditorGroup = bank.PushGroup("ShoreLine Editor");
					s_CreateShorelineWidgetsButton = bank.AddButton("Create Editor Widgets (don't click until game has finished loading!)", CFA(CreateShorelineEditorWidgets));					
				bank.PopGroup();
			}
				bank.PushGroup("Misc");
				bank.AddToggle("Draw Shore Lines", &sm_DrawShoreLines);
					bank.AddToggle("Draw Shore Lines Playing", &sm_DrawPlayingShoreLines);
					bank.AddToggle("Draw activation boxes", &sm_DrawActivationBox);
					bank.AddToggle("Test For Water At Ped Pos", &sm_TestForWaterAtPed);
					bank.AddToggle("Test For Water At Cursor Pos", &sm_TestForWaterAtCursor);
					bank.AddToggle("Test Ambient Zone Water Logic", &g_TestZoneOverWaterLogic);
					bank.AddToggle("Print Water Height On Click", &g_PrintWaterHeightOnMouseClick);
					audShoreLine::AddWidgets(bank);
					audShoreLinePool::AddWidgets(bank);
					audShoreLineRiver::AddWidgets(bank);
					audShoreLineOcean::AddWidgets(bank);
					audShoreLineLake::AddWidgets(bank);
				bank.PopGroup();
			bank.PopGroup();
			bank.PushGroup("Under water");
				bank.AddSlider("Camera depth in water threshold", &sm_CameraDepthInWaterThreshold, 0.0f, 100.0f, 0.1f);
				bank.AddToggle("Override underwater stream", &sm_OverrideUnderWaterStream);
				bank.AddText("Name", s_OverriddenUnderWaterStream, sizeof(s_OverriddenUnderWaterStream), false);
				bank.PopGroup();
		bank.PopGroup();

		bank.PushGroup("Walla",false);
			bank.AddToggle("Mute Walla", &g_MuteWalla);
			bank.AddToggle("Display Exterior Ped Walla Debug", &g_DisplayExteriorWallaDebug);
			bank.AddToggle("Display Interior Ped Walla Debug", &g_DisplayInteriorWallaDebug);
			bank.AddToggle("Display Hostile Ped Walla Debug", &g_DisplayHostileWallaDebug);
			bank.AddToggle("Draw Clamped/Scaled/Blocked Walla Values", &g_DrawClampedAndScaledWallaValues);
			bank.AddToggle("Override Ped Density", &g_OverridePedDensity);
			bank.AddSlider("Overriden Ped Density", &g_OverridenPedDensity, 0.0f, 1.0f, 0.1f);
			bank.AddSlider("Ped Density Smoothing Increase", &g_PedDensitySmoothingIncrease, 0.0f, 1.0f, 0.0001f);
			bank.AddSlider("Ped Density Smoothing Decrease", &g_PedDensitySmoothingDecrease, 0.0f, 1.0f, 0.0001f);
			bank.AddSlider("Ped Density Smoothing Interior", &g_PedDensitySmoothingInterior, 0.0f, 1.0f, 0.0001f);
			bank.AddToggle("Override Ped Panic", &g_OverridePedPanic);
			bank.AddSlider("Overriden Ped Panic", &g_OverridenPedPanic, 0.0f, 1.0f, 0.1f);
			bank.AddButton("Script Force Ped Panic", datCallback(MFA(audAmbientAudioEntity::ForcePedPanic), (datBase*)this), "");
		bank.PopGroup();

		bank.PushGroup("Walla Speech",false);
			bank.AddToggle("Enable Walla Speech", &g_EnableWallaSpeech);
			bank.AddToggle("Debug Draw Walla Speech", &g_DebugDrawWallaSpeech);
			bank.AddToggle("Log Walla Speech To Output", &g_LogWallaSpeech);
			bank.AddSlider("Active Ped Group Scroll", &g_DrawPedWallaSpeechScroll, 0.0f, 1.0f, 0.1f, NullCB, "Use this to scroll the valid ped list" );
			bank.AddToggle("Force Continual Retrigger", &g_ForceWallaSpeechRetrigger);
			bank.AddToggle("Display Exterior Ped Walla Debug", &g_DisplayExteriorWallaDebug);
			bank.AddSlider("Speech Volume Trim", &g_WallaSpeechVolumeTrim, -100.0f, 100.0f, 0.1f);
			bank.AddSlider("RMS Volume Smoothing (Increase)", &g_SFXRMSSmoothingRateIncrease, 0, 10000000, 1000);
			bank.AddSlider("RMS Volume Smoothing (Decrease)", &g_SFXRMSSmoothingRateDecrease, 0, 10000000, 1000);
		bank.PopGroup();

		bank.PushGroup("Ambient Radio Emitters",false);
			bank.AddToggle("Enable Ambient Radio Emitters", &g_EnableAmbientRadioEmitter);
			bank.AddToggle("Draw Radio Emitters", &g_DebugDrawAmbientRadioEmitter);
		bank.PopGroup();

		bank.PushGroup("Alarms",false);
			bank.AddText("Alarm Name", g_TestAlarmName, sizeof(g_TestAlarmName));
			bank.AddButton("Prepare Alarm", datCallback(MFA(audAmbientAudioEntity::TestPrepareAlarm), (datBase*)this), "");
			bank.AddButton("Start Alarm", datCallback(MFA(audAmbientAudioEntity::TestStartAlarm), (datBase*)this), "");
			bank.AddToggle("Skip Startup", &g_TestAlarmSkipStartup);
			bank.AddButton("Stop Alarm", datCallback(MFA(audAmbientAudioEntity::TestStopAlarm), (datBase*)this), "");
			bank.AddToggle("Stop Immediately", &g_TestAlarmStopImmediate);
			bank.AddToggle("Draw Static Emitters", &audEmitterAudioEntity::sm_ShouldDrawStaticEmitters);
			bank.AddText("Emitter Name", g_CreateEmitterName, sizeof(g_CreateEmitterName));
			bank.AddButton("Create New Static Emitter At Camera Coords", CFA(CreateEmitterAtCurrentCoords));
			bank.AddButton("Move Existing Static Emitter To Camera Coords", CFA(MoveEmitterToCurrentCoords));
			bank.AddText("Emitter To Duplicate", g_DuplicateEmitterName, sizeof(g_DuplicateEmitterName));
			bank.AddButton("Create Duplicate At Current Coords", CFA(DuplicateEmitterAtCurrentCoords));
		bank.PopGroup();

		bank.PushGroup("Streamed Sounds",false);
			bank.AddButton("Test Streamed Sound", datCallback(MFA(audAmbientAudioEntity::TestStreamedSound), (datBase*)this), "");
			bank.AddButton("Stop All Streamed Sounds", datCallback(MFA(audAmbientAudioEntity::StopAllStreamedAmbientSounds), (datBase*)this), "");
		bank.PopGroup();

	bank.PopGroup();
}
#endif
