// 
// audio/gtaaudioenvironment.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
//  
#include "microphones.h"
#include "audio/debugaudio.h"
#include "audio/northaudioengine.h"
#include "audio/scriptaudioentity.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audioengine/environment.h"
#include "audiohardware/driverutil.h"
#include "audiohardware/submix.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/tracking/CinematicCamManCamera.h"
#include "camera/cinematic/camera/animated/CinematicAnimatedCamera.h"
#include "camera/cinematic/camera/tracking/CinematicPedCloseUpCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/camera/tracking/CinematicHeliChaseCamera.h"
#include "camera/cinematic/camera/tracking/CinematicIdleCamera.h"
#include "camera/cinematic/camera/tracking/CinematicStuntCamera.h"
#include "camera/cinematic/camera/tracking/CinematicTrainTrackCamera.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/gameplay/aim/FirstPersonPedAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAimInCoverCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonVehicleAimCamera.h"
#include "camera/gameplay/follow/FollowCamera.h"
#include "camera/gameplay/follow/FollowPedCamera.h"
#include "camera/gameplay/follow/FollowObjectCamera.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/replay/ReplayPresetCamera.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/system/CameraManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "camera/replay/ReplayRecordedCamera.h"
#include "grcore/debugdraw.h"

AUDIO_ENVIRONMENT_OPTIMISATIONS()

// Camera params
f32 g_CamToSecondRatio = 0.5f;
f32 g_MicLength = 1.0f;
f32 g_CinematicPullBack = 18.0f;
f32 g_CinematicPullBackBoat = 25.0f;
f32 g_CinematicPullBackHeli = 60.0f;

f32 g_MicToPlayerLocalEnvironmentRatio = 0.5f;

u8 naMicrophones::sm_ReplayMicType = 0;

bool g_InPlayersHead = false;

#if __BANK
bool g_ForceFreezeMicrophone = false;
bool g_DrawMicType = false;
bool g_ShowActiveRenderCamera = false;
bool g_ShowListenerDistances = false;
bool g_DebugMic = false;
bool g_DrawListeners = false;
bool g_Listener0NoInterp = false;
bool g_Listener1NoInterp = false;
u32 g_TimeToBlendIdleMic = 5000;

const char * g_ReplayMicType[NUM_REPLAY_MICS] = { 
	"Default",
	"AtCamera",
	"AtTarget",
	"Cinematic",
};


Vector3 naMicrophones::sm_MicrophoneDebugPos;
static char s_MicrophoneDebugPos[256] = "";

f32 naMicrophones::sm_PaningToVolInterp = 0.f;

bool naMicrophones::sm_EqualizeCinRollOffFirstListener = true;
bool naMicrophones::sm_EqualizeCinRollOffSecondListener = true;
bool naMicrophones::sm_CenterVehMic = false;
bool naMicrophones::sm_FixSecondMicDistance = false;
bool naMicrophones::sm_UnifyCinematicVolMatrix = false;
bool naMicrophones::sm_ShowListenersInfo = false;
bool naMicrophones::sm_EnableDebugMicPosition = false;
#endif
bool naMicrophones::sm_ZoomOnFirstListener = true;

f32 naMicrophones::sm_SniperAmbienceVolIncrease = 3.f;
f32 naMicrophones::sm_SniperAmbienceVolDecrease = -6.f;
f32 naMicrophones::sm_DeltaCApply = 0.125f;

bool naMicrophones::sm_FixLocalEnvironmentPosition = false;
bool naMicrophones::sm_PlayerFrontend = true;
bool naMicrophones::sm_SniperMicActive = false;
bool naMicrophones::sm_HeliMicActive = false;

naMicrophones::naMicrophones()
{
}

naMicrophones::~naMicrophones()
{
}

void naMicrophones::Init()
{
	m_SniperZoomToRatio.Init(ATSTRINGHASH("SNIPER_ZOOM_TO_RATIO", 0x9673EA6D));
	m_CamFovToRollOff.Init(ATSTRINGHASH("CAMERA_FOV_TO_ROLLOFF", 0xAA268C64));
	m_CinematicCamDistToMicDistance.Init(ATSTRINGHASH("CAM_DIST_TO_MIC_DIST_RATIO", 0x4A622C75));
	m_CinematicCamFovToFrontConeAngle.Init(ATSTRINGHASH("CAMERA_FOV_TO_FRONT_CONE_ANGLE", 0x7373086B));
	m_CinematicCamFovToRearConeAngle.Init(ATSTRINGHASH("CAMERA_FOV_TO_REAR_CONE_ANGLE", 0x2E440E62));
	m_CinematicCamFovToMicAttenuation.Init(ATSTRINGHASH("CAMERA_FOV_TO_MIC_ATTENUATIOn", 0x9E178B5));

	for(u32 listenerId = 0; listenerId < g_maxListeners; listenerId++)
	{
		m_LastMicDistance[listenerId] = 1.f;
		m_CurrentCinematicRollOff[listenerId] = 1.f;
		m_CurrentCinematicApply[listenerId] = 0.f;
		m_LastMicType[listenerId] = MIC_FOLLOW_PED;
	}
	m_PlayerHeadMatrixCached.Zero();
	m_LocalEnvironmentScanPosition.Zero();
	m_LocalEnvironmentScanPositionLastFrame.Zero();
	m_ScriptVolMic1Position.Zero();
	m_ScriptVolMic2Position.Zero();
	m_ScriptPanningPosition.Zero();

	m_PlayingCutsceneLastFrame = false;
	m_InterpolatingIdleCam = false;
	m_BlendLevel = 0.f;

	m_ScriptMicSettings = 0;
	m_MicFrozenThisFrame = false;

	m_LastCamera = const_cast<camBaseCamera*>(camInterface::GetDominantRenderedCamera());

	m_DefaultMicSettings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ATSTRINGHASH("DEFAULT_MIC", 0x0fb9f5cc6));
	naAssertf(m_DefaultMicSettings, "Fail getting the default microphone settings.");
	m_CachedMicrophoneSettings = NULL;

	m_CinFrontCosSmoother.Init(0.001f,0.001f,-1.f,1.f);
	m_CinRearCosSmoother.Init(0.001f,0.001f,-1.f,1.f);
	m_CinRearAttSmoother.Init(0.1f,0.1f,-100.f,0.f);

#if __BANK
	m_WasMicFrozenLastFrame = false;
	m_SecondMicPos = Vector3(0.0f, 0.0f, 0.0f);
	for(s32 i = 0;i< g_maxListeners; i++)
	{
		m_PanningMatrix[i].Zero();
		m_VolMatrix[i].Zero();
		m_Camera[i].Zero();
	}
#endif
}
void naMicrophones::UpdatePlayerHeadMatrix()
{
	BANK_ONLY(MicDebugInfo()); // has to happen on the main thread.
	CPed* playerPed = CGameWorld::FindLocalPlayer();
	if( playerPed )
	{
		// Set all the microphones on the player's head
		playerPed->GetBoneMatrixCached(m_PlayerHeadMatrixCached, BONETAG_HEAD);
	}
}
void naMicrophones::SetUpMicrophones()
{
	sm_PlayerFrontend = true;
#if __BANK
	if(sm_EnableDebugMicPosition)
	{
		Matrix34 mic;
		mic.Identity();
		mic.d = sm_MicrophoneDebugPos;

		g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(mic, 0);
		g_AudioEngine.GetEnvironment().SetPanningListenerMatrix(mic, 0);
		g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(mic, 0);
		g_AudioEngine.GetEnvironment().SetPanningListenerMatrix(mic, 1);
		//Listener Contribution, attenuation parameters and rollOff
		g_AudioEngine.GetEnvironment().SetListenerContribution(0.5f, 0);
		g_AudioEngine.GetEnvironment().SetListenerContribution(0.5f, 1);
		g_AudioEngine.GetEnvironment().SetDirectionalMicParameters(0.707f, -0.707f, -3,-3, 0,0);
		g_AudioEngine.GetEnvironment().SetDirectionalMicParameters(0.707f, -0.707f, -3,-3, 0,1);
		g_AudioEngine.GetEnvironment().SetRollOff(1.f,0);
		g_AudioEngine.GetEnvironment().SetRollOff(1.f,1);
		return ;
	}
#endif

	if( m_MicFrozenThisFrame BANK_ONLY(|| g_ForceFreezeMicrophone) )
	{
#if __BANK
		// Warn every second the mic is frozen
		static f32 s_NextLogTime = 0.f;
		if(s_NextLogTime <= 0.f)
		{
			s_NextLogTime = 1.f;
			CPed *pPlayer = CGameWorld::FindLocalPlayer();
			f32 playerDistanceVol0 = 0.f; 
			f32 playerDistanceVol1 = 0.f;
			f32 playerDistancePanning = 0.f;
			if(pPlayer)
			{
				playerDistanceVol0 = fabs((m_VolMatrix[0].d - VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition())).Mag());
				playerDistanceVol1 = fabs((m_VolMatrix[1].d - VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition())).Mag());
				playerDistancePanning = fabs((m_PanningMatrix[0].d - VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition())).Mag());
			}

			f32 cameraDistanceVol0 = 0.f; 
			f32 cameraDistanceVol1 = 0.f;
			f32 cameraDistancePanning = 0.f;
			Matrix34 mat = camInterface::GetMat();

			cameraDistanceVol0 = fabs((m_VolMatrix[0].d - mat.d).Mag());
			cameraDistanceVol1 = fabs((m_VolMatrix[1].d - mat.d).Mag());
			cameraDistancePanning = fabs((m_PanningMatrix[0].d - mat.d).Mag());

			naDisplayf("[Frozen microphone frame %u] [Vol 0: (%0.2f,%0.2f,%0.2f) distFromPlayer %0.2f distFromCamera %0.2f] [Vol 1: (%0.2f,%0.2f,%0.2f) distFromPlayer %0.2f distFromCamera %0.2f] [Panning: (%0.2f,%0.2f,%0.2f) distFromPlayer %0.2f distFromCamera %0.2f]"
				,fwTimer::GetFrameCount(), m_VolMatrix[0].d.x, m_VolMatrix[0].d.y, m_VolMatrix[0].d.z, playerDistanceVol0, cameraDistanceVol0
				, m_VolMatrix[1].d.x, m_VolMatrix[1].d.y, m_VolMatrix[1].d.z, playerDistanceVol1, cameraDistanceVol1
				, m_PanningMatrix[0].d.x, m_PanningMatrix[0].d.y, m_PanningMatrix[0].d.z, playerDistancePanning, cameraDistancePanning);
		}
		else
		{
			s_NextLogTime -= fwTimer::GetTimeStep();
		}
#endif
		return;
	}
#if	__BANK
	else if (m_WasMicFrozenLastFrame)
	{
		// Last frame mic frozen.
		naDisplayf("[Microphone unfrozen frame %u]",fwTimer::GetFrameCount());
		m_WasMicFrozenLastFrame = false;
	}
#endif

	bool success = false ; 
	MicrophoneSettings *micSettings = CalculateMicrophoneSettings();
	if (naVerifyf(micSettings,"No Mic Settings returned from call CalculateMicrophoneSettings()"))
	{
		if(AUD_GET_TRISTATE_VALUE(micSettings->Flags,FLAG_ID_MICROPHONESETTINGS_PLAYERFRONTEND) == AUD_TRISTATE_FALSE)
		{
			sm_PlayerFrontend = false;
		}
		//TODO: Give support for the mic interpolation.
		////if we are interpolating an idle camera, update the blending level
		//if(m_InterpolatingIdleCam)
		//{
		//	naDisplayf("BLEND LEVEL %f",m_BlendLevel);
		//	m_BlendLevel += micSettings->SecondListenerContribution * (fwTimer::GetTimeStepInMilliseconds()/g_TimeToBlendIdleMic);
		//	naAssertf(m_BlendLevel <= micSettings->SecondListenerContribution,"Bad idle mic interpolation level ");
		//}
#if GTA_REPLAY
		if(CReplayMgr::IsEditorActive())
		{
			success = SetUpReplayMics(micSettings);
		}
		else
#endif
		success = SetUpMicrophones((MicrophoneType)micSettings->MicType,micSettings);
	}
	if( !micSettings || !success)
	{
		// Disable the listener contribution since we don't have a valid microphone.
		g_AudioEngine.GetEnvironment().SetListenerContribution(0.1f,0);
		g_AudioEngine.GetEnvironment().SetListenerContribution(0.1f,1);
	}
	naAssertf((g_AudioEngine.GetEnvironment().GetListenerContribution(0) + g_AudioEngine.GetEnvironment().GetListenerContribution(1)) <= 1.f
		,"Wrong listener contribution :%f, settings :%s Frozen : %s Replay: %s ReplayMic : %u",(g_AudioEngine.GetEnvironment().GetListenerContribution(0) + g_AudioEngine.GetEnvironment().GetListenerContribution(1))
		,micSettings ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(micSettings->NameTableOffset) : ""
		,(m_MicFrozenThisFrame BANK_ONLY(||g_ForceFreezeMicrophone)) ? "Yes" : "" 
		,REPLAY_ONLY(CReplayMgr::IsEditorActive() ? "Yes" :) ""
		,sm_ReplayMicType);
#if __BANK
	if(!g_DrawMicType)
	{
		for (u32 i=0; i<g_maxListeners; i++)
		{
			m_PanningMatrix[i] = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix(i));
			m_VolMatrix[i] = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix(i));
			m_Camera[i] = camInterface::GetMat();
		}
	}
#endif
	//// Set the microphone a bit forward from the camera
	//Matrix34 listenerMatrix = camInterface::GetMat();
	//const atArray<tRenderedCameraObjectSettings>& renderedCameras = camInterface::GetRenderedCameras();
	////we only have one camera active, no blending.
	//if(renderedCameras.GetCount() == 1)
	//{
	//	const camBaseCamera* theActiveRenderedCamera = camInterface::GetDominantRenderedCamera();
	//	if(theActiveRenderedCamera->IsInterpolating())
	//	{
	//		const camBaseCamera* sourceCamera = const_cast<camBaseCamera*>(theActiveRenderedCamera)->GetFrameInterpolator()->GetSourceCamera();
	//		InterpolateMics(sourceCamera,theActiveRenderedCamera,listenerMatrix,const_cast<camBaseCamera*>(theActiveRenderedCamera)->GetFrameInterpolator()->GetBlendLevel());
	//	}
	//	else 
	//	{
	//		SetUpOtherMics(micSettings,listenerMatrix);
	//	}
	//}
	//else
	//{
	//	InterpolateMics((const camBaseCamera* )renderedCameras[0].m_Camera,(const camBaseCamera* )renderedCameras[1].m_Camera,listenerMatrix,(1.f - renderedCameras[0].m_BlendLevel));
	//}
	// now see if we should be fixing the ambience mic position
	// fix it if we've just gone into a cutscene, unfix it if we've just left
	bool playingCutscene = gVpMan.AreWidescreenBordersActive() || (
		CutSceneManager::GetInstance()&&CutSceneManager::GetInstance()->IsStreamedCutScene());
	if (playingCutscene && !m_PlayingCutsceneLastFrame)
	{
		FixAmbienceOrientation(true);
	}
	if (!playingCutscene)
	{
		FixAmbienceOrientation(false);
	}
	m_PlayingCutsceneLastFrame = playingCutscene;
	if( !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::DisableSniperAudio) && (g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ForceSniperAudio) || (micSettings && micSettings->MicType == MIC_SNIPER_RIFLE)))
	{
		sm_SniperMicActive = true;
	}
	else 
	{
		sm_SniperMicActive = false;
	}

	if(micSettings && micSettings->MicType == MIC_C_CHASE_HELI)
	{
		sm_HeliMicActive = true;
	}
	else
	{
		sm_HeliMicActive = false;
	}
}
bool naMicrophones::SetUpMicrophones(const MicrophoneType micType,const MicrophoneSettings *micSettings,f32 minFov /*= 14.f*/,f32 maxFov /*= 45.f*/, f32 maxFrontCosAngle /*= 45.f*/, f32 maxRearCosAngle /*= 135.f*/, f32 maxAtt /*= -3.f*/)
{
	bool success = true;
	// If we have parameters for both listeners, set them up 
	if (micSettings->numListenerParameters == g_maxListeners)
	{
		naAssertf((micSettings->ListenerParameters[0].ListenerContribution + micSettings->ListenerParameters[1].ListenerContribution) <= 1.f,
			"The total listener contribution can't be higher than 1.f, please check the microphone settings for the current mic.");
		success = SetUpFirstListener(micType,micSettings,micSettings->ListenerParameters[0].RearAttenuationFrontConeAngle
			,micSettings->ListenerParameters[0].RearAttenuationRearConeAngle,micSettings->ListenerParameters[0].CloseRearAttenuation
			,minFov,maxFov,maxFrontCosAngle,maxRearCosAngle,maxAtt) && success;
		success = SetUpSecondaryListener(micType,micSettings,micSettings->ListenerParameters[1].ListenerContribution,micSettings->ListenerParameters[1].RearAttenuationFrontConeAngle
			,micSettings->ListenerParameters[1].RearAttenuationRearConeAngle,micSettings->ListenerParameters[1].CloseRearAttenuation
			,minFov,maxFov,maxFrontCosAngle,maxRearCosAngle,maxAtt) && success;
	}
	// If we have parameters for only the first listener, set it up and use defaults for the second.
	else if (micSettings->numListenerParameters == 1)
	{
		naAssertf(micSettings->ListenerParameters[0].ListenerContribution <= 1.f, "The total listener contribution can't be higher than 1.f, please check the microphone settings for the current mic.");
		success = SetUpFirstListener(micType,micSettings,micSettings->ListenerParameters[0].RearAttenuationFrontConeAngle
			,micSettings->ListenerParameters[0].RearAttenuationRearConeAngle,micSettings->ListenerParameters[0].CloseRearAttenuation
			,minFov,maxFov,maxFrontCosAngle,maxRearCosAngle,maxAtt) && success;
		success = SetUpSecondaryListener(micType,m_DefaultMicSettings,fabs(1.f - micSettings->ListenerParameters[0].ListenerContribution),micSettings->ListenerParameters[0].RearAttenuationFrontConeAngle
			,micSettings->ListenerParameters[0].RearAttenuationRearConeAngle,micSettings->ListenerParameters[0].CloseRearAttenuation
			,minFov,maxFov,maxFrontCosAngle,maxRearCosAngle,maxAtt) && success;
	}
	// If we don't have parameters for any of the first listener,  use defaults for both.
	else 
	{
		naAssertf((m_DefaultMicSettings->ListenerParameters[0].ListenerContribution + m_DefaultMicSettings->ListenerParameters[1].ListenerContribution) <= 1.f, 
			"The total listener contribution can't be higher than 1.f, please check the microphone settings for the current mic.");
		success = SetUpFirstListener(micType,m_DefaultMicSettings,m_DefaultMicSettings->ListenerParameters[0].RearAttenuationFrontConeAngle
			,m_DefaultMicSettings->ListenerParameters[0].RearAttenuationRearConeAngle,m_DefaultMicSettings->ListenerParameters[0].CloseRearAttenuation
			,minFov,maxFov,maxFrontCosAngle,maxRearCosAngle,maxAtt) && success;
		success = SetUpSecondaryListener(micType,m_DefaultMicSettings,m_DefaultMicSettings->ListenerParameters[1].ListenerContribution,m_DefaultMicSettings->ListenerParameters[0].RearAttenuationFrontConeAngle
			,m_DefaultMicSettings->ListenerParameters[0].RearAttenuationRearConeAngle,m_DefaultMicSettings->ListenerParameters[0].CloseRearAttenuation
			,minFov,maxFov,maxFrontCosAngle,maxRearCosAngle,maxAtt) && success;
	}
	return success;
}

bool naMicrophones::SetUpFirstListener(const MicrophoneType micType,const MicrophoneSettings *micSettings,f32 minFrontCosAngle, f32 minRearCosAngle,f32 minAtt,f32 minFov ,f32 maxFov , f32 maxFrontCosAngle , f32 maxRearCosAngle , f32 maxAtt )
{
	camBaseCamera* theActiveRenderedCamera = const_cast<camBaseCamera*>(camInterface::GetDominantRenderedCamera());
	if(!theActiveRenderedCamera)
	{
		return false;
	}
	naAssertf(micSettings,"Something went wrong, null microphone settings");

	CPed* player = CGameWorld::FindLocalPlayer();
	// By default the vol and panning mics are placed where the camera is
	Matrix34 volMicMatrix = camInterface::GetMat();
	Matrix34 panningMicMatrix = volMicMatrix;

	// Microphone lenght
	f32 micLength = micSettings->ListenerParameters[0].MicLength;
	// we then make sure the micLength doesn't push us past the player, as this causes things to be panned to the rear, and to get filtered.
	if (player)
	{
		f32 distanceFromMicToPlayer = volMicMatrix.d.Dist(VEC3V_TO_VECTOR3(player->GetTransform().GetPosition()));
		// then subtract a wee bit off that, so we don't get flitting
		distanceFromMicToPlayer = Max(0.0f, distanceFromMicToPlayer-0.25f);
		micLength = Min( micSettings->ListenerParameters[0].MicLength, distanceFromMicToPlayer);
	}
	// Listener parameters: 
	f32 listenerContribution = micSettings->ListenerParameters[0].ListenerContribution;
	f32 frontCos = rage::Cosf( ( DtoR * micSettings->ListenerParameters[0].RearAttenuationFrontConeAngle));
	f32 rearCos  = rage::Cosf( ( DtoR * micSettings->ListenerParameters[0].RearAttenuationRearConeAngle));
	f32 closeRearAttenuation = micSettings->ListenerParameters[0].CloseRearAttenuation;
	f32 farRearAttenuation = micSettings->ListenerParameters[0].FarRearAttenuation;
	f32 rollOff = micSettings->ListenerParameters[0].RollOff;
	f32 globalOcclusionDampingFactor = 1.f;

	// Position the first listener based on the microphone type.
	switch (micType)
	{
	case MIC_SNIPER_RIFLE:
		{
			if (player)
			{
				// Pull back the volume mic from the player in the direction of the camera micLenght meters.
				Vector3 front = volMicMatrix.b;
				front.Normalize();
				volMicMatrix = MAT34V_TO_MATRIX34(player->GetTransform().GetMatrix());
				volMicMatrix.d = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition()) - (front* micLength);
			}
		}
		break;
	case MIC_DEBUG:
		{
			// Set all the microphones where the debug camera is
			volMicMatrix = camInterface::GetDebugDirector().GetFreeCamFrame().GetWorldMatrix();
#if GTA_REPLAY
			if( CReplayMgr::IsEditorActive())
			{
				volMicMatrix = camInterface::GetMat();
			}
#endif
			panningMicMatrix = volMicMatrix;
		}
		break;
	case MIC_PLAYER_HEAD:
		{
			volMicMatrix = m_PlayerHeadMatrixCached;
			volMicMatrix.a = m_PlayerHeadMatrixCached.c;
			volMicMatrix.c = m_PlayerHeadMatrixCached.a;
			// Then swap left-right
			volMicMatrix.a.Negate();

			Vector3 front = panningMicMatrix.b;
			front.Normalize();
			// Pull back the vol mic from the ped micLenght meters in the direction of the camera
			volMicMatrix.d = volMicMatrix.d - (front * micLength);
			// Keep the panning mic a meter behind the close volume mic.
			panningMicMatrix.d = volMicMatrix.d - front;
		}
		break;
	case MIC_C_MANCAM:
		{
			if(theActiveRenderedCamera)
			{
				// By default, the vol mic is towards the camera micLength meters
				Vector3 front = volMicMatrix.b;
				front.Normalize();
				volMicMatrix.d = volMicMatrix.d + front *  micLength;

				// Get the camera focus entity
				const CEntity* entity = theActiveRenderedCamera->GetAttachParent();
				f32 distance = 1.f;
				if(!entity)
				{
					entity = theActiveRenderedCamera->GetLookAtTarget();
				}
				if (entity)
				{
					// Distance to entity
					distance = fabs((volMicMatrix.d - VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition())).Mag());
					BANK_ONLY(if(sm_EqualizeCinRollOffFirstListener))
					{
						if(sm_ZoomOnFirstListener)
						{
							frontCos = m_CinFrontCosSmoother.CalculateValue(rage::Cosf( ( DtoR * m_CinematicCamFovToFrontConeAngle.CalculateRescaledValue(minFrontCosAngle,maxFrontCosAngle,minFov,maxFov,camInterface::GetFov()))),audNorthAudioEngine::GetCurrentTimeInMs());
							rearCos = m_CinRearCosSmoother.CalculateValue(rage::Cosf( ( DtoR * m_CinematicCamFovToRearConeAngle.CalculateRescaledValue(minRearCosAngle,maxRearCosAngle,minFov,maxFov,camInterface::GetFov()))),audNorthAudioEngine::GetCurrentTimeInMs());
							closeRearAttenuation = m_CinRearAttSmoother.CalculateValue(m_CinematicCamFovToMicAttenuation.CalculateRescaledValue(minAtt,maxAtt,minFov,maxFov,camInterface::GetFov()),audNorthAudioEngine::GetCurrentTimeInMs());
							farRearAttenuation = closeRearAttenuation;
						}
						if(m_LastMicType[0] == MIC_C_CHASE_HELI)
						{
							m_CurrentCinematicRollOff[0] = ComputeEquivalentRollOff(0,distance);
							m_CurrentCinematicApply[0] = 0.f;
						}
						else
						{
							f32 desireRollOff = rollOff;
							if(sm_ZoomOnFirstListener)
							{
								desireRollOff = m_CamFovToRollOff.CalculateRescaledValue(1.f,rollOff,minFov,maxFov,camInterface::GetFov());
							}
							if (m_CurrentCinematicRollOff[0] != desireRollOff)
							{
								m_CurrentCinematicRollOff[0] = Lerp(m_CurrentCinematicApply[0],m_CurrentCinematicRollOff[0],desireRollOff);
								m_CurrentCinematicApply[0] += sm_DeltaCApply * audNorthAudioEngine::GetTimeStep();
								m_CurrentCinematicApply[0] = Clamp(m_CurrentCinematicApply[0],0.f,1.f);
							}
						}
						rollOff = m_CurrentCinematicRollOff[0];
					}
				}
				m_LastMicType[0] = MIC_C_MANCAM;
				m_LastMicDistance[0] = distance;
			}
		}
		break;
	case MIC_C_CHASE_HELI:
		{
			if(theActiveRenderedCamera)
			{
				// By default, the vol mic is towards the camera micLength meters
				Vector3 front = volMicMatrix.b;
				front.Normalize();
				volMicMatrix.d = volMicMatrix.d + front *  micLength;

				// Get the camera focus entity
				const CEntity* entity = theActiveRenderedCamera->GetAttachParent();
				f32 distance = 1.f;
				if(!entity)
				{
					entity = theActiveRenderedCamera->GetLookAtTarget();
				}
				if (entity)
				{
					// Distance to entity
					distance = fabs((volMicMatrix.d - VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition())).Mag());
				}
					BANK_ONLY(if(sm_EqualizeCinRollOffFirstListener))
					{
						if(sm_ZoomOnFirstListener)
						{
							frontCos = m_CinFrontCosSmoother.CalculateValue(rage::Cosf( ( DtoR * m_CinematicCamFovToFrontConeAngle.CalculateRescaledValue(micSettings->ListenerParameters[0].RearAttenuationFrontConeAngle,45.f,14.f,45.f,camInterface::GetFov()))),audNorthAudioEngine::GetCurrentTimeInMs());
							rearCos = m_CinRearCosSmoother.CalculateValue(rage::Cosf( ( DtoR * m_CinematicCamFovToRearConeAngle.CalculateRescaledValue(micSettings->ListenerParameters[0].RearAttenuationRearConeAngle,135.f,14.f,45.f,camInterface::GetFov()))),audNorthAudioEngine::GetCurrentTimeInMs());
							closeRearAttenuation = m_CinRearAttSmoother.CalculateValue(m_CinematicCamFovToMicAttenuation.CalculateRescaledValue(micSettings->ListenerParameters[0].CloseRearAttenuation,-3.f,14.f,45.f,camInterface::GetFov()),audNorthAudioEngine::GetCurrentTimeInMs());
							farRearAttenuation = closeRearAttenuation;
						}
						if(m_LastMicType[0] == MIC_C_MANCAM)
						{
							m_CurrentCinematicRollOff[0] = ComputeEquivalentRollOff(0,distance);
							m_CurrentCinematicApply[0] = 0.f;
						}
						else
						{
							f32 desireRollOff = rollOff;
							if(sm_ZoomOnFirstListener)
							{
								desireRollOff = m_CamFovToRollOff.CalculateRescaledValue(1.f,rollOff,minFov,maxFov,camInterface::GetFov());
							}
							if (m_CurrentCinematicRollOff[0] != desireRollOff)
							{
								m_CurrentCinematicRollOff[0] = Lerp(m_CurrentCinematicApply[0],m_CurrentCinematicRollOff[0],desireRollOff);
								m_CurrentCinematicApply[0] += sm_DeltaCApply * audNorthAudioEngine::GetTimeStep();
								m_CurrentCinematicApply[0] = Clamp(m_CurrentCinematicApply[0],0.f,1.f);
							}
						}
						rollOff = m_CurrentCinematicRollOff[0];
					}
				m_LastMicType[0] = MIC_C_CHASE_HELI;
				m_LastMicDistance[0] = distance;
			}
		}
		break;
	case MIC_VEH_BONNET: 
		{
			if(theActiveRenderedCamera)
			{
				const CVehicle* vehicle = NULL; 
				const CEntity* entity = theActiveRenderedCamera->GetAttachParent();
				if (entity && entity->GetIsTypeVehicle())
				{
					vehicle = static_cast<const CVehicle*>(theActiveRenderedCamera->GetAttachParent());
				}
				else if( SUPERCONDUCTOR.GetVehicleConductor().JumpConductorActive() )
				{
					vehicle = CGameWorld::FindLocalPlayerVehicle();
				}

				Vector3 front = volMicMatrix.b;
				front.Normalize();
				if (vehicle && vehicle->GetVehicleType()!=VEHICLE_TYPE_TRAIN)
				{
					front = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetB());
					Vector3 dirToPanMic = panningMicMatrix.d - VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition());
					f32 proyectionMag = dirToPanMic.Dot(front);
					panningMicMatrix.d = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()) + (proyectionMag * front);
					panningMicMatrix.d.z = volMicMatrix.d.z;
					volMicMatrix.d = panningMicMatrix.d;
					panningMicMatrix.d = panningMicMatrix.d + (panningMicMatrix.b*micLength);
				}
				if (!sm_FixLocalEnvironmentPosition)
				{
					m_LocalEnvironmentScanPosition =  camInterface::GetMat().d * micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio + (1.0f-micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio) * volMicMatrix.d;
				}
			}
		}
		break;
	default:
		{
			// By default the first listener's vol mic is set up micLength meters from the camera towards its front direction
			Vector3 front = volMicMatrix.b;
			front.Normalize();
			volMicMatrix.d = volMicMatrix.d + front *  micLength;
		}
		break;
	}
	// Set our default position for local scanning stuff - move it towards the player in some cams.
	if (!sm_FixLocalEnvironmentPosition)
	{
		m_LocalEnvironmentScanPosition = volMicMatrix.d;
	}
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ScriptForceMicPosition))
	{
		volMicMatrix.d = m_ScriptVolMic1Position;
		panningMicMatrix.d = m_ScriptPanningPosition;
#if __BANK
		// Warn every second the mic is forced by script, in case they forget to clean up the flag and the mic remains overridden.
		static f32 s_NextWarnTime = 0.f;
		if(s_NextWarnTime <= 0.f)
		{
			s_NextWarnTime = 1.f;
			audWarningf("Microphone position overridden by script");
		}
		else
		{
			s_NextWarnTime -= fwTimer::GetTimeStep();
		}
#endif
	}
	// Set up the listener in the environment.
	bool success =  g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(volMicMatrix, 0);
	success = success && g_AudioEngine.GetEnvironment().SetPanningListenerMatrix(panningMicMatrix, 0);
	if(success)
	{
		// if setVolumeListenerMatrix or setPanningListenerMatrix return false, it means there has been an error and it's defaulting the contribution to 0.001f
		g_AudioEngine.GetEnvironment().SetListenerContribution(listenerContribution, 0);
	}

	//Listener Contribution, attenuation parameters and rollOff
	g_AudioEngine.GetEnvironment().SetDirectionalMicParameters(frontCos, rearCos, closeRearAttenuation,farRearAttenuation, micSettings->ListenerParameters[0].RearAttenuationType,0);
	naAssertf(rollOff >= 0.f, "Wrong interpolated rolloff");
	g_AudioEngine.GetEnvironment().SetRollOff(rollOff,0);
	audNorthAudioEngine::GetOcclusionManager()->SetGlobalOcclusionDampingFactor(globalOcclusionDampingFactor);

	return true;
}
bool naMicrophones::SetUpSecondaryListener(const MicrophoneType micType,const MicrophoneSettings *micSettings,f32 listenerContribution,f32 minFrontCosAngle, f32 minRearCosAngle,f32 minAtt,f32 minFov ,f32 maxFov , f32 maxFrontCosAngle , f32 maxRearCosAngle , f32 maxAtt )
{
	camBaseCamera* theActiveRenderedCamera = const_cast<camBaseCamera*>(camInterface::GetDominantRenderedCamera());
	if(!theActiveRenderedCamera)
	{
		return false;
	}
	naAssertf(micSettings,"Something went wrong, null microphone settings");

	// By default the vol and panning mics are placed where the camera is
	Matrix34 volMicMatrix = camInterface::GetMat();
	Matrix34 panningMicMatrix = volMicMatrix;

	f32 micLength = micSettings->ListenerParameters[1].MicLength;
	// Listener parameters: 
	f32 frontCos = rage::Cosf( ( DtoR * micSettings->ListenerParameters[1].RearAttenuationFrontConeAngle));
	f32 rearCos  = rage::Cosf( ( DtoR * micSettings->ListenerParameters[1].RearAttenuationRearConeAngle));
	f32 closeRearAttenuation = micSettings->ListenerParameters[1].CloseRearAttenuation;
	f32 farRearAttenuation = micSettings->ListenerParameters[1].FarRearAttenuation;
	f32 rollOff = micSettings->ListenerParameters[1].RollOff;
	f32 globalOcclusionDampingFactor = 1.f;

	switch (micType)
	{
	case MIC_FOLLOW_PED:
		{
			if(theActiveRenderedCamera)
			{
				Vector3 front = volMicMatrix.b;
				front.Normalize();
				// Pull back the vol mic from the ped micLenght meters in the direction of the camera
				const CEntity* entity = theActiveRenderedCamera->GetAttachParent();
				if(!entity)
				{
					entity = theActiveRenderedCamera->GetLookAtTarget();
				}
				if (entity)
				{
					volMicMatrix.d = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition()) - (front*micLength);
				}
#if GTA_REPLAY
				if( CReplayMgr::IsEditorActive())
				{
					if(!entity)
					{
						// ifthe player does not set a replay camera target, default to the player. 
						CPed* player = CGameWorld::FindLocalPlayer();
						if (player)
						{
							volMicMatrix.d = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition()) - (front*micLength);
						}
						// In replay if we are free cam, both volume microphones are together
						if(theActiveRenderedCamera->GetClassId() != camReplayFreeCamera::GetStaticClassId())
						{		
							g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(volMicMatrix, 0);
						}
					}
				}
#endif
				if (!sm_FixLocalEnvironmentPosition)
				{
					m_LocalEnvironmentScanPosition =  camInterface::GetMat().d * micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio + (1.0f-micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio) * volMicMatrix.d;
				}
			}
		}
		break;
	case MIC_SNIPER_RIFLE:
		{
			// Move the vol mic towards the cam direction micLength meters
			Vector3 front = volMicMatrix.b;
			front.Normalize();
			volMicMatrix.d += (front * micLength);
			if (!sm_FixLocalEnvironmentPosition)
			{
				m_LocalEnvironmentScanPosition =  camInterface::GetMat().d * micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio + (1.0f-micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio) * volMicMatrix.d;
			}
			rollOff = m_SniperZoomToRatio.CalculateRescaledValue(1.f,rollOff,7.5f,45.f,camInterface::GetFov());
		}
		break;
	case MIC_FOLLOW_VEHICLE:
		{
			if(theActiveRenderedCamera)
			{
				Vector3 front = volMicMatrix.b;
				front.Normalize();
				// By default push back the vol mic in the camera direction
				const CVehicle* vehicle = NULL; 
				const CEntity* entity = theActiveRenderedCamera->GetAttachParent();
				if(!entity)
				{
					entity = theActiveRenderedCamera->GetLookAtTarget();
				}
				if (entity && entity->GetIsTypeVehicle())
				{
					vehicle = static_cast<const CVehicle*>(theActiveRenderedCamera->GetAttachParent());
				}
				else if( SUPERCONDUCTOR.GetVehicleConductor().JumpConductorActive() || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OverrideMicrophoneSettings))
				{
					vehicle = CGameWorld::FindLocalPlayerVehicle();
				}
				//Keep the vol mic on the camera for trains.
				if (vehicle && vehicle->GetVehicleType()!=VEHICLE_TYPE_TRAIN)
				{
#if __BANK
					if(sm_CenterVehMic)
					{
						// push back the vol mic in the veh direction, therefore it will be center with the veh.
						front = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetB());
					}
#endif
					volMicMatrix.d = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()) - (front*micLength);
				}
				if (!sm_FixLocalEnvironmentPosition)
				{
					m_LocalEnvironmentScanPosition =  camInterface::GetMat().d * micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio + (1.0f-micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio) * volMicMatrix.d;
				}
			}
		}
		break;
	case MIC_C_MANCAM:
		{
			if(theActiveRenderedCamera)
			{
				Vector3 front = volMicMatrix.b;
				front.Normalize();
				// Get the camera focus entity
				const CEntity* entity = theActiveRenderedCamera->GetAttachParent();
				f32 distance = 1.f;
				if(!entity)
				{
					entity = theActiveRenderedCamera->GetLookAtTarget();
				}
				if (entity)
				{
					// Distance to entity
					distance = fabs((volMicMatrix.d - VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition())).Mag());
					// Check if we want to move the vol mic relative to the distance to the veh 
					BANK_ONLY(if(!sm_FixSecondMicDistance))
					{
						micLength = m_CinematicCamDistToMicDistance.CalculateRescaledValue(1.f,micLength,0.f,100.f,distance);
					}
					volMicMatrix.d = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition()) - (front * micLength);
					// Compute the real distance from the vol mic to the entity
					distance =  fabs((volMicMatrix.d - VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition())).Mag());
					BANK_ONLY(if(sm_EqualizeCinRollOffSecondListener))
					{
						if(!sm_ZoomOnFirstListener)
						{
							frontCos = m_CinFrontCosSmoother.CalculateValue(rage::Cosf( ( DtoR * m_CinematicCamFovToFrontConeAngle.CalculateRescaledValue(minFrontCosAngle,maxFrontCosAngle,minFov,maxFov,camInterface::GetFov()))),audNorthAudioEngine::GetCurrentTimeInMs());
							rearCos = m_CinRearCosSmoother.CalculateValue(rage::Cosf( ( DtoR * m_CinematicCamFovToRearConeAngle.CalculateRescaledValue(minRearCosAngle,maxRearCosAngle,minFov,maxFov,camInterface::GetFov()))),audNorthAudioEngine::GetCurrentTimeInMs());
							closeRearAttenuation = m_CinRearAttSmoother.CalculateValue(m_CinematicCamFovToMicAttenuation.CalculateRescaledValue(minAtt,maxAtt,minFov,maxFov,camInterface::GetFov()),audNorthAudioEngine::GetCurrentTimeInMs());
							farRearAttenuation = closeRearAttenuation;
						}
						if(m_LastMicType[1] == MIC_C_CHASE_HELI)
						{
							m_CurrentCinematicRollOff[1] = ComputeEquivalentRollOff(1,distance);
							m_CurrentCinematicApply[1] = 0.f;	
						}
						else
						{
							f32 desireRollOff = rollOff;
							if(!sm_ZoomOnFirstListener)
							{
								desireRollOff = m_CamFovToRollOff.CalculateRescaledValue(1.f,rollOff,minFov,maxFov,camInterface::GetFov());
							}
							if (m_CurrentCinematicRollOff[1] != desireRollOff)
							{
								m_CurrentCinematicRollOff[1] = Lerp(m_CurrentCinematicApply[1],m_CurrentCinematicRollOff[1],desireRollOff);
								m_CurrentCinematicApply[1] += sm_DeltaCApply * audNorthAudioEngine::GetTimeStep();
								m_CurrentCinematicApply[1] = Clamp(m_CurrentCinematicApply[1],0.f,1.f);
							}
						}
						rollOff = m_CurrentCinematicRollOff[1];
						m_LastMicType[1] = MIC_C_MANCAM;
						m_LastMicDistance[1] = distance;
					}
#if __BANK
					if(sm_UnifyCinematicVolMatrix)
					{
						g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(volMicMatrix, 0);
					}
#endif
					if (!sm_FixLocalEnvironmentPosition)
					{
						m_LocalEnvironmentScanPosition = camInterface::GetMat().d * micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio + (1.0f-micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio) * volMicMatrix.d;
					}
				}
#if __BANK
				Vector3 dirToVolMatrix = panningMicMatrix.d - volMicMatrix.d;
				f32 distanceToVolMatrix = dirToVolMatrix.Mag(); 
				dirToVolMatrix.Normalize();
				panningMicMatrix.d -= Lerp (sm_PaningToVolInterp,0.f,distanceToVolMatrix) * dirToVolMatrix;
#endif
			}
		}
		break;
	case MIC_C_CHASE_HELI:
		{
			if(theActiveRenderedCamera)
			{
				Vector3 front = volMicMatrix.b;
				front.Normalize();
				// Get the camera focus entity
				const CEntity* entity = theActiveRenderedCamera->GetAttachParent();
				f32 distance = 1.f;
				if(!entity)
				{
					entity = theActiveRenderedCamera->GetLookAtTarget();
				}
				if (entity)
				{
					// Distance to entity
					distance = fabs((volMicMatrix.d - VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition())).Mag());
					// Check if we want to move the vol mic relative to the distance to the veh 
					
#if __BANK
					if(!sm_FixSecondMicDistance REPLAY_ONLY( && (!CReplayMgr::IsEditorActive() || sm_ReplayMicType != replayMicCinematic)))
					{
						micLength = m_CinematicCamDistToMicDistance.CalculateRescaledValue(1.f,micLength,0.f,100.f,distance);
					}
#else
					REPLAY_ONLY( if(!CReplayMgr::IsEditorActive() || sm_ReplayMicType != replayMicCinematic))
					{
						micLength = m_CinematicCamDistToMicDistance.CalculateRescaledValue(1.f,micLength,0.f,100.f,distance);
					}
#endif
					volMicMatrix.d = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition()) - (front * micLength);
					// Compute the real distance from the vol mic to the entity
					distance =  fabs((volMicMatrix.d - VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition())).Mag());
					BANK_ONLY(if(sm_EqualizeCinRollOffSecondListener))
					{
						if(!sm_ZoomOnFirstListener)
						{
							frontCos = m_CinFrontCosSmoother.CalculateValue(rage::Cosf( ( DtoR * m_CinematicCamFovToFrontConeAngle.CalculateRescaledValue(micSettings->ListenerParameters[1].RearAttenuationFrontConeAngle,45.f,14.f,45.f,camInterface::GetFov()))),audNorthAudioEngine::GetCurrentTimeInMs());
							rearCos = m_CinRearCosSmoother.CalculateValue(rage::Cosf( ( DtoR * m_CinematicCamFovToRearConeAngle.CalculateRescaledValue(micSettings->ListenerParameters[1].RearAttenuationRearConeAngle,135.f,14.f,45.f,camInterface::GetFov()))),audNorthAudioEngine::GetCurrentTimeInMs());
							closeRearAttenuation = m_CinRearAttSmoother.CalculateValue(m_CinematicCamFovToMicAttenuation.CalculateRescaledValue(micSettings->ListenerParameters[1].CloseRearAttenuation,-3.f,14.f,45.f,camInterface::GetFov()),audNorthAudioEngine::GetCurrentTimeInMs());
							farRearAttenuation = closeRearAttenuation;
						}
						if(m_LastMicType[1] == MIC_C_MANCAM)
						{
							m_CurrentCinematicRollOff[1] = ComputeEquivalentRollOff(1,distance);
							m_CurrentCinematicApply[1] = 0.f;	
						}
						else
						{
							f32 desireRollOff = rollOff;
							if(!sm_ZoomOnFirstListener)
							{
								desireRollOff = m_CamFovToRollOff.CalculateRescaledValue(1.f,rollOff,14.f,45.f,camInterface::GetFov());
							}
							if (m_CurrentCinematicRollOff[1] != desireRollOff)
							{
								m_CurrentCinematicRollOff[1] = Lerp(m_CurrentCinematicApply[1],m_CurrentCinematicRollOff[1],desireRollOff);
								m_CurrentCinematicApply[1] += sm_DeltaCApply * audNorthAudioEngine::GetTimeStep();
								m_CurrentCinematicApply[1] = Clamp(m_CurrentCinematicApply[1],0.f,1.f);
							}
						}
						rollOff = m_CurrentCinematicRollOff[1];
						m_LastMicType[1] = MIC_C_CHASE_HELI;
						m_LastMicDistance[1] = distance;
					}
#if __BANK
					if(sm_UnifyCinematicVolMatrix)
					{
						g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(volMicMatrix, 0);
					}
#endif
					if (!sm_FixLocalEnvironmentPosition)
					{
						m_LocalEnvironmentScanPosition = camInterface::GetMat().d * micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio + (1.0f-micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio) * volMicMatrix.d;
					}
#if __BANK
					Vector3 dirToVolMatrix = panningMicMatrix.d - volMicMatrix.d;
					f32 distanceToVolMatrix = dirToVolMatrix.Mag(); 
					dirToVolMatrix.Normalize();
					panningMicMatrix.d -= Lerp (sm_PaningToVolInterp,0.f,distanceToVolMatrix) * dirToVolMatrix;
#endif
				}
			}
		}
		break;

	//case MIC_C_DEFAULT:
	//	if(naVerifyf(theActiveRenderedCamera, "Invalid camBaseCamera*. Secondary mic disabled."))
	//	{
	//		const CEntity* entity = theActiveRenderedCamera->GetAttachParent();
	//		if(!entity)
	//		{
	//			entity = theActiveRenderedCamera->GetLookAtTarget();
	//		}
	//		if (entity)
	//		{
	//			f32 distance = (volMicMatrix.d - VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition())).Mag();
	//			Matrix34 mic = listenerMatrix;
	//			f32 micLength = m_CinematicCamDistToMicDistance.CalculateRescaledValue(1.f,micSettings->ListenerParameters[1].MicLength,0.f,100.f,distance);
	//			mic.d = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition()) - (front * micLength);
	//			f32 rollOff = m_CamFovToRollOff.CalculateRescaledValue(1.f,micSettings->ListenerParameters[1].RollOff,14.f,45.f,camInterface::GetFov());
	//			g_AudioEngine.GetEnvironment().SetRollOff(rollOff,1);
	//			naDisplayf("CURRENT ROLLOFF, %f",rollOff);
	//			g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(mic, 1);
	//			//g_AudioEngine.GetEnvironment().SetListenerContribution(secondRatio, 1);
	//			if (!sm_FixLocalEnvironmentPosition)
	//			{
	//				m_LocalEnvironmentScanPosition = listenerMatrix.d * micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio + (1.0f-micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio) * mic.d;
	//			}
	//		}
	//	}
	//	else 
	//	{
	//		g_AudioEngine.GetEnvironment().SetListenerContribution(0.f, 1);
	//	}
	//	break;
	case MIC_C_TRAIN_TRACK:
		{
			globalOcclusionDampingFactor = 0.66f;
		}
		break;
	case MIC_C_IDLE:
		{
			if(theActiveRenderedCamera)
			{
				//Matrix34 lastMic;
				//if(m_LastCamera && m_LastCamera != theActiveRenderedCamera && !m_InterpolatingIdleCam)
				//{
				//	naDisplayf("INTERPOLATING ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
				//	m_InterpolatingIdleCam = true;
				//	if(m_BlendLevel >= micSettings->SecondListenerContribution)
				//	{
				//		naDisplayf("STOP ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
				//		m_InterpolatingIdleCam = false;
				//		m_BlendLevel = 0.f;
				//		m_LastCamera = theActiveRenderedCamera;
				//	}
				//	GetMicMatrixForSimpleCam(theActiveRenderedCamera, listenerMatrix, mic);
				//	GetMicMatrixForSimpleCam(m_LastCamera, listenerMatrix, lastMic);
				//	g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(mic, 1);
				//	g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(lastMic, 2);
				//	// See how zoomed in we are, and set our mix appropriately
				//	f32 farthestZoom = camInterface::GetFarDof();
				//	f32 nearestZoom = camInterface::GetNearDof();
				//	f32 currentZoom = camInterface::GetFov();
				//	f32 zoomRatio = 0.0f;
				//	if (nearestZoom != farthestZoom)
				//	{
				//		zoomRatio = RampValue (currentZoom, nearestZoom, farthestZoom, 1.0f, 0.0f);
				//	}
				//	zoomRatio *= m_BlendLevel;
				//	g_AudioEngine.GetEnvironment().SetListenerContribution(micSettings->SecondListenerContribution *zoomRatio, 1);
				//	farthestZoom = m_LastCamera->GetFrame().GetFarDof();
				//	nearestZoom =  m_LastCamera->GetFrame().GetNearDof();
				//	currentZoom =  m_LastCamera->GetFrame().GetFov();
				//	zoomRatio = 0.0f;
				//	if (nearestZoom != farthestZoom)
				//	{
				//		zoomRatio = RampValue (currentZoom, nearestZoom, farthestZoom, 1.0f, 0.0f);
				//	}
				//	zoomRatio *=  (micSettings->SecondListenerContribution - m_BlendLevel);
				//	g_AudioEngine.GetEnvironment().SetListenerContribution(micSettings->SecondListenerContribution *zoomRatio, 2);
				//}
				//else
				//{
				Vector3 front = volMicMatrix.b;
				front.Normalize();
				camCinematicIdleCamera* idleCam = (camCinematicIdleCamera*)theActiveRenderedCamera;
				const CEntity* targetEntity = idleCam->GetLookAtTarget();
				if (targetEntity)
				{
					volMicMatrix.d = VEC3V_TO_VECTOR3(targetEntity->GetTransform().GetPosition()) - (front*8.0f);
				}
				// See how zoomed in we are, and set our mix appropriately
				f32 farthestZoom = camInterface::GetFarDof();
				f32 nearestZoom = camInterface::GetNearDof();
				f32 currentZoom = camInterface::GetFov();
				f32 zoomRatio = 0.0f;
				if (nearestZoom != farthestZoom)
				{
					zoomRatio = RampValueSafe (currentZoom, nearestZoom, farthestZoom, 1.0f, 0.0f);
				}
				listenerContribution *= zoomRatio;

				//if(!m_LastCamera)
				//	m_LastCamera = theActiveRenderedCamera;
				//}
			}
			else 
			{
				g_AudioEngine.GetEnvironment().SetListenerContribution(0.f, 1);
			}
		}
		break;
	case MIC_DEBUG:
		{
			volMicMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix());
		}
		break;
	case MIC_PLAYER_HEAD:
		{
			if(theActiveRenderedCamera)
			{
				panningMicMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
				Vector3 front = volMicMatrix.b;
				front.Normalize();
				// Pull back the vol mic from the ped micLenght meters in the direction of the camera
				const CEntity* attachEntity = theActiveRenderedCamera->GetAttachParent();
				if (attachEntity)
				{
					volMicMatrix.d = VEC3V_TO_VECTOR3(attachEntity->GetTransform().GetPosition()) - (front*micLength);
				}
				if (!sm_FixLocalEnvironmentPosition)
				{
					m_LocalEnvironmentScanPosition =  camInterface::GetMat().d * micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio + (1.0f-micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio) * volMicMatrix.d;
				}
			}
		}
		break;
	case MIC_VEH_BONNET: 
		{
			if(theActiveRenderedCamera)
			{
				panningMicMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
				const CVehicle* vehicle = NULL; 
				const CEntity* entity = theActiveRenderedCamera->GetAttachParent();
				if (entity && entity->GetIsTypeVehicle())
				{
					vehicle = static_cast<const CVehicle*>(theActiveRenderedCamera->GetAttachParent());
				}
				else if( SUPERCONDUCTOR.GetVehicleConductor().JumpConductorActive() )
				{
					vehicle = CGameWorld::FindLocalPlayerVehicle();
				}

				Vector3 front = volMicMatrix.b;
				front.Normalize();
				if (vehicle && vehicle->GetVehicleType()!=VEHICLE_TYPE_TRAIN)
				{
					front = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetB());
					panningMicMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix());
					volMicMatrix.d = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()) - (front*micLength);
				}
				if (!sm_FixLocalEnvironmentPosition)
				{
					m_LocalEnvironmentScanPosition =  camInterface::GetMat().d * micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio + (1.0f-micSettings->ListenerParameters[1].MicToPlayerLocalEnvironmentRatio) * volMicMatrix.d;
				}
			}
		}
		break;
	default:break;
	}
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ScriptForceMicPosition))
	{
		volMicMatrix.d = m_ScriptVolMic2Position;
		panningMicMatrix.d = m_ScriptPanningPosition;
	}
	// Set up the listener in the environment.
	bool success =  g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(volMicMatrix, 1);
	success = success && g_AudioEngine.GetEnvironment().SetPanningListenerMatrix(panningMicMatrix, 1);
	if(success)
	{
		BANK_ONLY(g_AudioEngine.GetEnvironment().SetPanningListenerMatrix(panningMicMatrix, 0);)
		g_AudioEngine.GetEnvironment().SetListenerContribution(listenerContribution, 1);
	}
	//Listener Contribution, attenuation parameters and rollOff
	g_AudioEngine.GetEnvironment().SetDirectionalMicParameters(frontCos, rearCos, closeRearAttenuation,farRearAttenuation, micSettings->ListenerParameters[1].RearAttenuationType,1);
	g_AudioEngine.GetEnvironment().SetRollOff(rollOff,1);
	audNorthAudioEngine::GetOcclusionManager()->SetGlobalOcclusionDampingFactor(globalOcclusionDampingFactor);

	return true;
}
f32 naMicrophones::ComputeEquivalentRollOff(u32 listenerId,f32 distance)
{
	//naDisplayf("------------------------LISTENER %u--------------------------",listenerId);
	// Get prev rollOff
	f32 prevRollOff = g_AudioEngine.GetEnvironment().GetRollOff(listenerId);
	//naDisplayf("Previous RollOff %f distance %f",prevRollOff,m_LastMicDistance[listenerId]);
	// Compute the normalized distance
	f32 tightNormalisedDistance = m_LastMicDistance[listenerId] / prevRollOff;
	f32 wideFarExtraDistance = m_LastMicDistance[listenerId] - 5.0f;
	f32 wideFarNormalisedDistance = 5.0f + (wideFarExtraDistance / prevRollOff);
	f32 wideCloseNormalisedDistance = m_LastMicDistance[listenerId];
	f32 wideNormalisedDistance = Selectf(m_LastMicDistance[listenerId]-5.0f, wideFarNormalisedDistance, wideCloseNormalisedDistance);
	f32 prevNormalizedDistance = Selectf(prevRollOff - 1.0f, wideNormalisedDistance, tightNormalisedDistance);
	//naDisplayf("Previous normalize distance %f",prevNormalizedDistance);
	//naDisplayf("Previous distance attenuation %f",g_AudioEngine.GetEnvironment().GetDistanceAttenuation(0,prevRollOff,m_LastMicDistance[listenerId]));
	f32 newRollOff = 1.f;
	if(prevRollOff > 1.f)
	{
		newRollOff = ((prevRollOff* (distance - 5.f))/ (m_LastMicDistance[listenerId] - 5.f));
	}
	else 
	{
		newRollOff = (prevRollOff * distance) / m_LastMicDistance[listenerId];
	}
	//naDisplayf("Possible error  RollOff %f ",newRollOff);
	// Compensate the error.
	if (newRollOff < 1.f && prevRollOff > 1.f)
	{
		newRollOff = distance/prevNormalizedDistance;
	}
	else if (newRollOff > 1.f && prevRollOff <= 1.f)
	{
		newRollOff = (distance - 5.f)/(prevNormalizedDistance - 5.f);
	}

	//naDisplayf("Equivalent RollOff %f distance %f",newRollOff,distance);
	tightNormalisedDistance = distance / newRollOff;
	wideFarExtraDistance = distance - 5.0f;
	wideFarNormalisedDistance = 5.0f + (wideFarExtraDistance / newRollOff);
	wideCloseNormalisedDistance = distance;
	wideNormalisedDistance = Selectf(distance-5.0f, wideFarNormalisedDistance, wideCloseNormalisedDistance);
	prevNormalizedDistance = Selectf(newRollOff - 1.0f, wideNormalisedDistance, tightNormalisedDistance);
	//naDisplayf("Equivalent normalize distance %f",prevNormalizedDistance);
	//naDisplayf("Equivalent distance attenuation %f",g_AudioEngine.GetEnvironment().GetDistanceAttenuation(0,newRollOff,distance));
	newRollOff = Max(0.001f,newRollOff);
	return newRollOff;
}
void naMicrophones::InterpolateMics(const camBaseCamera* /*sourceCamera*/, const camBaseCamera* /*destCamera*/, Matrix34& /*listenerMatrix*/, f32 /*blendLevel*/)
{
	//	Matrix34 interpolatedMat;
	//	interpolatedMat.Zero();
	//	Matrix34 mic0,mic1;
	//	GetMicMatrixForSimpleCam(sourceCamera, listenerMatrix, mic0);
	//	GetMicMatrixForSimpleCam(destCamera, listenerMatrix, mic1);
	//	interpolatedMat.a =	mic0.a;
	//	interpolatedMat.b =	mic0.b;
	//	interpolatedMat.c =	mic0.c;
	//	interpolatedMat.d.Lerp(blendLevel, mic0.d, mic1.d);
	//	Quaternion sourceOrientation;
	//	sourceOrientation.FromMatrix34(mic0);
	//	Quaternion destinationOrientation;
	//	destinationOrientation.FromMatrix34(mic1);
	//	Quaternion resultOrientation;
	//	resultOrientation.SlerpNear(blendLevel, sourceOrientation, destinationOrientation);
	//	interpolatedMat.FromQuaternion(resultOrientation);
	//
	//	f32 contribution = 0.f;
	//	f32 RearAttenuationFrontConeAngle = 0.f;
	//	f32 RearAttenuationRearConeAngle = 0.f;
	//	f32 RearAttenuation = 0.f;
	//	f32 MicLength = 0.f;
	//	f32 MicToPlayerLocalEnvironmentRatio = 0.f;	
	//
	//	// source camera. 
	//	MicrophoneSettings *micSettings = GetMicrophoneSettings();
	//	if (naVerifyf(micSettings,"No Mic Settings returned from call GetMicrophoneSettings()"))
	//	{
	//		contribution += micSettings->SecondListenerContribution * blendLevel;
	//		RearAttenuationFrontConeAngle += micSettings->RearAttenuationFrontConeAngle * blendLevel;
	//		RearAttenuationRearConeAngle += micSettings->RearAttenuationRearConeAngle * blendLevel;
	//		RearAttenuation += micSettings->RearAttenuation * blendLevel;
	//		MicLength += micSettings->MicLength * blendLevel;
	//		MicToPlayerLocalEnvironmentRatio += micSettings->MicToPlayerLocalEnvironmentRatio * blendLevel;
	//	}
	//	micSettings = GetMicrophoneSettings(sourceCamera);
	//	if (naVerifyf(micSettings,"No Mic Settings returned from call GetMicrophoneSettings()"))
	//	{
	//		contribution += micSettings->SecondListenerContribution * (1.f - blendLevel);
	//		RearAttenuationFrontConeAngle += micSettings->RearAttenuationFrontConeAngle * (1.f - blendLevel);
	//		RearAttenuationRearConeAngle += micSettings->RearAttenuationRearConeAngle * (1.f - blendLevel);
	//		RearAttenuation += micSettings->RearAttenuation * (1.f - blendLevel);
	//		MicLength += micSettings->MicLength * (1.f - blendLevel);
	//		MicToPlayerLocalEnvironmentRatio += micSettings->MicToPlayerLocalEnvironmentRatio * (1.f - blendLevel);
	//
	//		g_AudioEngine.GetEnvironment().SetVolumeListenerMatrix(interpolatedMat, 1);
	//		g_AudioEngine.GetEnvironment().SetListenerContribution(contribution, 1);
	//#if __BANK
	//		m_PanningMatrix[1] = g_AudioEngine.GetEnvironment().GetPanningListenerMatrix();
	//		m_VolMatrix[1] = interpolatedMat;
	//		m_Camera[1] = camInterface::GetMat();
	//#endif
	//	}
}
MicrophoneSettings * naMicrophones::CalculateMicrophoneSettings( const camBaseCamera *camera )
{
	MicrophoneSettings *settings = NULL;
	const camBaseCamera* theActiveRenderedCamera = (camera)?camera:camInterface::GetDominantRenderedCamera();
	u32 activeCamNameHash = 0;
	u32 activeCamClassIDHash = 0;

#if GTA_REPLAY
	// In replay, take the camera type from the dominant camera at the time of recording, *unless* the user is in free (or preset) cam mode
	if(CReplayMgr::IsEditModeActive() && theActiveRenderedCamera && theActiveRenderedCamera->GetClassId() == camReplayRecordedCamera::GetStaticClassId())
	{		
		activeCamNameHash = CReplayMgr::GetReplayFrameData().m_cameraData.m_DominantCameraNameHash;
		activeCamClassIDHash = CReplayMgr::GetReplayFrameData().m_cameraData.m_DominantCameraClassIDHash;
	}
	else
#endif // GTA_REPLAY
	if(theActiveRenderedCamera)
	{
		activeCamNameHash = theActiveRenderedCamera->GetNameHash();
		activeCamClassIDHash = theActiveRenderedCamera->GetClassId().GetHash();
	}
	
	if(g_InPlayersHead)
	{
		settings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ATSTRINGHASH("FIRST_PERSON_MIC", 0xF71BB75E));
		naAssertf(settings,"Failed getting microphone settings for IN_PLAYERS_HEAD_MIC");
	}
	else if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ForceSniperAudio))
	{
		settings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ATSTRINGHASH("SNIPER_LOW_ZOOM_AIM_MIC", 0x9BAA361));
		naAssertf(settings,"Failed getting microphone settings for SNIPER_LOW_ZOOM_AIM_MIC");
	}
	else if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OverrideMicrophoneSettings))
	{
		settings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(m_ScriptMicSettings);
		naAssertf(settings,"Failed getting microphone settings for %u",m_ScriptMicSettings);
	}
	else
	{
		if(activeCamNameHash != 0 && activeCamClassIDHash != 0)
		{
			// First look for any microphone set up for the camera instance: 
			MicSettingsReference *ref = audNorthAudioEngine::GetObject<MicSettingsReference>(ATSTRINGHASH("MIC_REFERENCES", 0x0e602b85d));
			if(Verifyf(ref, "Couldn't find mic settings references"))
			{
				for (s32 i = 0; i < ref->numReferences; ++i)
				{
					if( activeCamNameHash == ref->Reference[i].CameraType)
					{
						settings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ref->Reference[i].MicSettings);
						break;
					}
				}
				if(!settings)
				{
					// We have no microphone settings for this instance, try and look for the camera type 
					for (s32 i = 0; i < ref->numReferences; ++i)
					{
						if( activeCamClassIDHash == ref->Reference[i].CameraType)
						{
							// Hacky GTAV fix: if we're using an old-style hood mounted camera, we want to use a different mic to the FPS in vehicle mic
							if(audNorthAudioEngine::IsRenderingHoodMountedVehicleCam() && activeCamClassIDHash == ATSTRINGHASH("camCinematicMountedCamera", 0x2F96EBCD))
							{
								settings = audNorthAudioEngine::GetObject<MicrophoneSettings>(ATSTRINGHASH("VEH_LAST_GEN_BONNET_MIC", 0x984C6561));
							}

							if(!settings)
							{
								settings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ref->Reference[i].MicSettings);
							}							
							break;
						}
					}
				}
			}
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive())
			{
				naAssertf(settings,"Failed getting microphone settings for %u",activeCamClassIDHash);
			}
			else
#endif // GTA_REPLAY
			{
				naAssertf(settings,"Failed getting microphone settings for %s",theActiveRenderedCamera->GetClassId().GetCStr());
			}
		}
	}
	if(!settings)
	{
		settings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ATSTRINGHASH("DEFAULT_MIC", 0x0fb9f5cc6));
	}
	m_CachedMicrophoneSettings = settings;
	return m_CachedMicrophoneSettings;
}

#if GTA_REPLAY
bool naMicrophones::SetUpReplayMics(const MicrophoneSettings* settings)
{
	bool success = false;
	MicrophoneSettings* overriddenSettings = const_cast<MicrophoneSettings*>(settings);
	camBaseCamera* theActiveRenderedCamera = const_cast<camBaseCamera*>(camInterface::GetDominantRenderedCamera());
	if(theActiveRenderedCamera)
	{
		const CEntity* entity = theActiveRenderedCamera->GetAttachParent();
		if(!entity)
		{
			entity = theActiveRenderedCamera->GetLookAtTarget();
		}

		switch(sm_ReplayMicType)
		{
		case replayMicDefault:
			if(theActiveRenderedCamera->GetClassId() != camReplayRecordedCamera::GetStaticClassId())
			{
				overriddenSettings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ATSTRINGHASH("REPLAY_CINEMATIC_MIC", 0x65744A10));
			}
			break;
		case replayMicAtCamera:
			overriddenSettings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ATSTRINGHASH("DEBUG_MIC", 0x2A64A375));
			break;
		case replayMicAtTarget:
			if (entity)
			{
				if (entity->GetIsTypePed())
				{
					overriddenSettings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ATSTRINGHASH("REPLAY_FOLLOW_PED_MIC", 0x65ABB9B6));
				}
				else if(entity->GetIsTypeVehicle())
				{
					overriddenSettings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ATSTRINGHASH("REPLAY_FOLLOW_VEHICLE_MIC", 0xF94F9B88));
				}
			}
			break;
		case replayMicCinematic:
			overriddenSettings =  audNorthAudioEngine::GetObject<MicrophoneSettings>(ATSTRINGHASH("REPLAY_CINEMATIC_MIC", 0x65744A10));
			break;
		default:
			break;
		}
		if( overriddenSettings )
		{
			success = SetUpMicrophones((MicrophoneType)overriddenSettings->MicType,overriddenSettings,10,100);
		}
		else
		{
			success = SetUpMicrophones((MicrophoneType)settings->MicType,settings,10,100);
		}
	}
	return success;
}
#endif // GTA_REPLAY

#if __BANK
const char* naMicrophones::GetMicName(u8 micType)
{
	switch (micType)
	{
	case MIC_FOLLOW_PED:
		return "Microphone - Follow ped";
	default: 
		return "Microphone - Invalid type";
	}
}
#endif

f32 naMicrophones::GetSniperAmbienceVolume(Vec3V_In soundPos,u32 listenerId)
{
	MicrophoneSettings *micSettings = GetMicrophoneSettings();
	if(micSettings && micSettings->MicType == MIC_SNIPER_RIFLE)
	{
		Vec3V positionRelativeToPanningListener = g_AudioEngine.GetEnvironment().ComputePositionRelativeToPanningListener(soundPos, listenerId);
		Vec3V vFront = Vec3V(V_Y_AXIS_WZERO);
		Vec3V relativePositionNormalised = Normalize(positionRelativeToPanningListener);

		f32 cosAngle = Dot(relativePositionNormalised, vFront).Getf();
		f32 cosConeAngle = g_AudioEngine.GetEnvironment().GetListener(listenerId).CosConeAngle;
		f32 cosRearConeAngle = rage::Cosf( ( DtoR * 90.f));
		if (cosAngle >= cosConeAngle)
		{
			return m_SniperZoomToRatio.CalculateRescaledValue(0.f,sm_SniperAmbienceVolIncrease,7.5f,45.f,camInterface::GetFov());
		}
		else if (cosAngle <= cosRearConeAngle)
		{
			return m_SniperZoomToRatio.CalculateRescaledValue(0.f,sm_SniperAmbienceVolDecrease,7.5f,45.f,camInterface::GetFov());
		}
		else 
		{
			f32 translatedRatio = (cosConeAngle - cosAngle)/(cosConeAngle - cosRearConeAngle);
			f32 increaseValue = m_SniperZoomToRatio.CalculateRescaledValue(0.f,sm_SniperAmbienceVolIncrease,7.5f,45.f,camInterface::GetFov());
			f32 decreaseValue = m_SniperZoomToRatio.CalculateRescaledValue(0.f,sm_SniperAmbienceVolDecrease,7.5f,45.f,camInterface::GetFov());
			return audCurveRepository::GetLinearInterpolatedValue(decreaseValue,increaseValue,0.f,1.f,1.f - translatedRatio);
		}
	}
	return 0.f;
}

bool naMicrophones::IsVisibleBySniper(CEntity *entity)
{
	bool visible = false; 
	if(sm_SniperMicActive)
	{
		spdSphere boundSphere;
		entity->GetBoundSphere(boundSphere);
		visible = IsVisibleBySniper(boundSphere);
	}
	return visible;
}

bool naMicrophones::IsVisibleBySniper(const spdSphere &boundSphere)
{
	bool visible = false; 
	if(sm_SniperMicActive)
	{
		if(camInterface::IsSphereVisibleInGameViewport(boundSphere.GetV4()))
		{
			visible = true;
		}
	}
	return visible;
}
bool naMicrophones::IsFirstPerson()
{
	return InPlayersHead() || IsVehBonnetMic();
}
bool naMicrophones::IsFollowMicType()
{
	if(GetMicrophoneSettings())
	{
		MicrophoneType micType = (MicrophoneType)GetMicrophoneSettings()->MicType;
		return  (micType == MIC_FOLLOW_PED || micType <= MIC_FOLLOW_VEHICLE );
	}
	return false;
}
bool naMicrophones::IsVehBonnetMic()
{
	if(GetMicrophoneSettings())
	{
		MicrophoneType micType = (MicrophoneType)GetMicrophoneSettings()->MicType;
		return  (micType == MIC_VEH_BONNET);
	}
	return false;
}
bool naMicrophones::InPlayersHead()
{
	if(GetMicrophoneSettings())
	{
		MicrophoneType micType = (MicrophoneType)GetMicrophoneSettings()->MicType;
		return  (micType == MIC_PLAYER_HEAD);
	}
	return false;
}
bool naMicrophones::IsCinematicMicActive()
{
	if(GetMicrophoneSettings())
	{
		MicrophoneType micType = (MicrophoneType)GetMicrophoneSettings()->MicType;
		return  (micType>= MIC_C_CHASE_HELI && micType <= MIC_C_TRAIN_TRACK ) || (micType >= MIC_C_DEFAULT && micType <= MIC_C_MANCAM) ;
	}
	return false;
}

// Script-controlled microphone functions
bool naMicrophones::EnableScriptControlledMicrophone()
{
	// we might decline this for a bunch of reasons
	// because I'm paranoid about giving this stuff, let's check if we already have script control
	if (m_ScriptControllingMics)
	{
		naErrorf("Attempting to script-control mics when already in control");
		return false;
	}
	// Clear everything out
	m_ScriptControlledMainMicActive = false;
	m_ScriptedDialogueFrontend = false;
	m_FixedAmbienceOrientation = false;
	m_ScriptControllingMics = true;
#if __BANK
	for (s32 i=0; i<MAX_CLOSE_MIC_PEDS; i++)
	{
		naAssertf(!m_CloseMicPed[i], "Dangling peds in script-controlled mic settings");
	}
#endif
	return true;
}

void naMicrophones::ReleaseScriptControlledMicrophone()
{
	if (!m_ScriptControllingMics)
	{
#if __BANK
		for (s32 i=0; i<MAX_CLOSE_MIC_PEDS; i++)
		{
			naAssertf(!m_CloseMicPed[i], "Dangling peds in script-controlled mic settings when releasing while not in control");
		}
#endif
		return;
	}

	m_ScriptControllingMics = false;
	m_ScriptControlledMainMicActive = false;
	m_ScriptedDialogueFrontend = false;
	m_FixedAmbienceOrientation = false;
	// loop through our ped mics, and release any references we have
	for (s32 i=0; i<MAX_CLOSE_MIC_PEDS; i++)
	{
		if (m_CloseMicPed[i])
		{
			m_CloseMicPed[i] = NULL;
		}
	}
}

void naMicrophones::CloseMicPed(u8 micId, CPed* ped)
{
	naAssertf(m_ScriptControllingMics, "Attemptiong to assign a close mic ped but we're not in control");
	// Make sure it's a valid slot, we don't already have a ped in that slot, and there's actually a ped
	naCErrorf(micId<MAX_CLOSE_MIC_PEDS, "micId is out of bounds of m_CloseMicPed[]");
	naCErrorf(!m_CloseMicPed[micId], "Attempting to assign %s as a close mic ped, but already have ped %s in this slot", ped->GetModelName(), m_CloseMicPed[micId]->GetModelName());
	naCErrorf(ped, "Null CPed ptr passed into CloseMicPed");

	if (micId>=MAX_CLOSE_MIC_PEDS || m_CloseMicPed[micId] || !ped)
	{
		return;
	}

	m_CloseMicPed[micId] = ped;
	AssertEntityPointerValid( ((CEntity*)m_CloseMicPed[micId]) );
}

void naMicrophones::RemoveCloseMicPed(u8 micId)
{
	naAssertf(m_ScriptControllingMics, "Attempting to remove a close mic ped but we're not in control");
	naCErrorf(micId<MAX_CLOSE_MIC_PEDS, "micId is out of bounds of m_CloseMicped[]");
	naCErrorf(m_CloseMicPed[micId], "Attempting to remove a close mic ped but there's no ped in the slot");
	if (micId<MAX_CLOSE_MIC_PEDS && m_CloseMicPed[micId])
	{
		AssertEntityPointerValid( m_CloseMicPed[micId] );
		m_CloseMicPed[micId] = NULL;
	}
}

void naMicrophones::FixScriptMicToCurrentPosition()
{
	naAssertf(m_ScriptControllingMics, "Attempting to fix script mic to current position but we're not in control");
	// Set the script-mic to the current mic position (use the volume one, as that's what we'll be setting)
	m_ScriptControlledMainMicActive = true;
	m_ScriptControlledMic = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix());
}

void naMicrophones::SetScriptMicPosition(const Vector3& position)
{
	naAssertf(m_ScriptControllingMics, "Attempting to set script mic position but we're not in control");
	// Just set its position, and leave orientation untouched - will doubtless be about to use LookAt
	m_ScriptControlledMainMicActive = true;
	m_ScriptControlledMic.d = position;
}

void naMicrophones::SetScriptMicPosition(const Vector3& panningPos,const Vector3& vol1Pos,const Vector3& vol2Pos)
{
	m_ScriptPanningPosition = panningPos;
	m_ScriptVolMic1Position = vol1Pos;
	m_ScriptVolMic2Position = vol2Pos;
}

void naMicrophones::SetScriptMicLookAt(const Vector3& position)
{
	naAssertf(m_ScriptControllingMics, "Attempting to set script mic orientation but we're not in control");
	m_ScriptControlledMainMicActive = true;
	m_ScriptControlledMic.LookAt(position);
}

void naMicrophones::RemoveScriptMic()
{
	naAssertf(m_ScriptControllingMics, "Attempting to remove mic script but we're not in control");
	m_ScriptControlledMainMicActive = false;
}

void naMicrophones::FixAmbienceOrientation(bool enable)
{
//	naAssertf(m_ScriptControllingMics, "Attempting to fix ambience orientation but we're not in control");
	m_FixedAmbienceOrientation = enable;
	m_AmbienceOrientationMatrix = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix());
}

void naMicrophones::PlayScriptedConversationFrontend(bool enable)
{
	naAssertf(m_ScriptControllingMics, "Attempting to play scripted frontednd conversation but we're not in control");
	m_ScriptedDialogueFrontend = enable;
}
#if __BANK
void StopDebug(void)
{
	g_DrawMicType = false;
	fwTimer::EndUserPause();
}
void naMicrophones::MicDebugInfo()
{
	if(sm_ShowListenersInfo)
	{
		CPed *pPlayer = CGameWorld::FindLocalPlayer();
		for (u32 i=0; i<g_maxListeners; i++)
		{
			char txt[128];
			formatf(txt,sizeof(txt),"Listener %u",i);
			grcDebugDraw::AddDebugOutput(Color_green,txt);
			f32 distance = (m_VolMatrix[i].d - VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition())).Mag();
			formatf(txt,sizeof(txt),"[Dist %f] [Fov %f] [RollOff %f] [FCA %f] [RCA %f] [CRA %f] [FRA %f]",distance,camInterface::GetFov()
				,g_AudioEngine.GetEnvironment().GetRollOff(i)
				,rage::Acosf(g_AudioEngine.GetEnvironment().GetListener(i).CosConeAngle) *RtoD
				,rage::Acosf(g_AudioEngine.GetEnvironment().GetListener(i).CosRearConeAngle) *RtoD
				,g_AudioEngine.GetEnvironment().GetListener(i).CloseDirectionalMicAttenuation
			,g_AudioEngine.GetEnvironment().GetListener(i).FarDirectionalMicAttenuation);
			grcDebugDraw::AddDebugOutput(Color_white,txt);
		}
	}
	if(g_ShowListenerDistances)
	{
		CPed *pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer)
		{
			char txt[128];
			formatf(txt,sizeof(txt),"Listeners distance to player");
			grcDebugDraw::AddDebugOutput(Color_green,txt);
			for (u32 i=0; i<g_maxListeners; i++)
			{
				formatf(txt,sizeof(txt),"	Listener :%d",i);
				grcDebugDraw::AddDebugOutput(Color_orange,txt);
				f32 distance = (m_PanningMatrix[i].d - VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition())).Mag();
				formatf(txt,sizeof(txt),"		Panning matrix is at: %f meters from the player.",distance);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
				distance = (m_VolMatrix[i].d - VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition())).Mag();
				formatf(txt,sizeof(txt),"		Volume matrix is at: %f meters from the player.",distance);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
				distance = (m_Camera[i].d - VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition())).Mag();
				formatf(txt,sizeof(txt),"		The camera matrix is at: %f meters from the player.",distance);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
			}
		}
		if(CDebugScene::FocusEntities_Get(0))
		{
			char txt[128];
			formatf(txt,sizeof(txt),"Listeners distance to focused entity");
			grcDebugDraw::AddDebugOutput(Color_green,txt);
			for (u32 i=0; i<g_maxListeners; i++)
			{
				formatf(txt,sizeof(txt),"	Listener :%d",i);
				grcDebugDraw::AddDebugOutput(Color_orange,txt);
				f32 distance = (m_PanningMatrix[i].d - VEC3V_TO_VECTOR3(CDebugScene::FocusEntities_Get(0)->GetTransform().GetPosition())).Mag();
				formatf(txt,sizeof(txt),"		Panning matrix is at: %f meters from the player.",distance);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
				distance = (m_VolMatrix[i].d - VEC3V_TO_VECTOR3(CDebugScene::FocusEntities_Get(0)->GetTransform().GetPosition())).Mag();
				formatf(txt,sizeof(txt),"		Volume matrix is at: %f meters from the player.",distance);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
				distance = (m_Camera[i].d - VEC3V_TO_VECTOR3(CDebugScene::FocusEntities_Get(0)->GetTransform().GetPosition())).Mag();
				formatf(txt,sizeof(txt),"		The camera matrix is at: %f meters from the player.",distance);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
			}
		}
	}
	if(g_ShowActiveRenderCamera)
	{
		const camBaseCamera* theActiveRenderedCamera = camInterface::GetDominantRenderedCamera();
		if( theActiveRenderedCamera )
		{
			char txt[64];
			formatf(txt,sizeof(txt),"%s",theActiveRenderedCamera->GetClassId().GetCStr());
			grcDebugDraw::AddDebugOutput(Color_white,txt); 
			formatf(txt,sizeof(txt),"%s",theActiveRenderedCamera->GetName());
			grcDebugDraw::AddDebugOutput(Color_white,txt); 
			Matrix34 mat = camInterface::GetMat();
			formatf(txt,sizeof(txt),"<%f,%f,%f>",mat.d.GetX(),mat.d.GetY(),mat.d.GetZ());
			grcDebugDraw::AddDebugOutput(Color_white,txt); 
			formatf(txt,sizeof(txt),"<%f,%f,%f>",mat.b.GetX(),mat.b.GetY(),mat.b.GetZ());
			grcDebugDraw::AddDebugOutput(Color_white,txt); 
		}

	}
	if(g_DebugMic)
	{
		fwTimer::StartUserPause();
		g_DebugMic = false;
		g_DrawMicType = true;
	}
	if(g_DrawMicType || g_DrawListeners)
	{
		for(s32 i = 0;i< g_maxListeners; i++)
		{
			//Cam
			grcDebugDraw::Sphere(m_Camera[i].d, 0.05f,Color32(0, 255, 0, 255));
			grcDebugDraw::Line(m_Camera[i].d,m_Camera[i].d + m_Camera[i].a,Color32(255, 0, 0, 255));
			grcDebugDraw::Line(m_Camera[i].d,m_Camera[i].d + m_Camera[i].b,Color32(0, 255, 0, 255));
			grcDebugDraw::Line(m_Camera[i].d,m_Camera[i].d + m_Camera[i].c,Color32(0, 0, 255, 255));
			//Panning
			grcDebugDraw::Sphere(m_PanningMatrix[i].d, 0.05f,Color32(255, 0, 0, 255));
			grcDebugDraw::Line(m_PanningMatrix[i].d,m_PanningMatrix[i].d + m_PanningMatrix[i].a,Color32(255, 0, 0, 255));
			grcDebugDraw::Line(m_PanningMatrix[i].d,m_PanningMatrix[i].d + m_PanningMatrix[i].b,Color32(0, 255, 0, 255));
			grcDebugDraw::Line(m_PanningMatrix[i].d,m_PanningMatrix[i].d + m_PanningMatrix[i].c,Color32(0, 0, 255, 255));
			//Volume
			grcDebugDraw::Sphere(m_VolMatrix[i].d, 0.05f,Color32(0, 0, 255, 255));
			grcDebugDraw::Line(m_VolMatrix[i].d,m_VolMatrix[i].d + m_VolMatrix[i].a,Color32(255, 0, 0, 255));
			grcDebugDraw::Line(m_VolMatrix[i].d,m_VolMatrix[i].d + m_VolMatrix[i].b,Color32(0, 255, 0, 255));
			grcDebugDraw::Line(m_VolMatrix[i].d,m_VolMatrix[i].d + m_VolMatrix[i].c,Color32(0, 0, 255, 255));
		}
	}
}
void naMicrophones::WarpMicrophone()
{
	if(sm_EnableDebugMicPosition)
	{
		// Try to parse the line.
		int nItems = 0;
		{
			// Get the locations to store the float values into.
			float* apVals[3] = {&sm_MicrophoneDebugPos.x, &sm_MicrophoneDebugPos.y, &sm_MicrophoneDebugPos.z};
			// Parse the line.
			char* pToken = NULL;
			const char* seps = " ,\t";
			pToken = strtok(s_MicrophoneDebugPos, seps);
			while(pToken)
			{
				// Try to read the token as a float.
				int success = sscanf(pToken, "%f", apVals[nItems]);
				Assertf(success, "Unrecognized token '%s' in warp position line.",pToken);
				if(success)
				{
					++nItems;
					Assertf((nItems < 16), "Too many tokens in warp position line." );
				}
				pToken = strtok(NULL, seps);
			}
		}
	}
}
void naMicrophones::AddWidgets(bkBank &bank)
{
	// GTA-specific, game-thread only things
	bank.PushGroup("Microphones");
		bank.AddToggle("Force Freeze Microphone", &g_ForceFreezeMicrophone);
		bank.AddToggle("Centre microphone to the veh. (FOLLOW_VEHICLE only)", &sm_CenterVehMic);
		bank.AddToggle("Enable microphone debug position", &sm_EnableDebugMicPosition);
		bank.AddText("Microphone debug position", s_MicrophoneDebugPos, sizeof(s_MicrophoneDebugPos));
		bank.AddButton("Set mic to debug position", WarpMicrophone);
		bank.AddToggle("Show listener distances", &g_ShowListenerDistances);
		bank.AddToggle("ShowActive render camera", &g_ShowActiveRenderCamera);
		bank.AddToggle("Show Listeners info", &sm_ShowListenersInfo);
		bank.AddToggle("Mic on player's head", &g_InPlayersHead);
		bank.AddToggle("Draw Listeners", &g_DrawListeners);
		bank.AddToggle("Debug mic", &g_DebugMic);
		bank.AddButton("Stop debug", datCallback(CFA(StopDebug)));
		bank.AddSlider("Time to blend idle microphone.", &g_TimeToBlendIdleMic, 0, 50000, 100);
		bank.AddSlider("Cam To Second ratio", &g_CamToSecondRatio, -0.01f, 0.99f, 0.01f);
		bank.AddSlider("Boom mic length", &g_MicLength, -2.0f, 5.0f, 0.1f);
		bank.AddSlider("Cinematic pullback length", &g_CinematicPullBack, 0.0f, 25.0f, 0.1f);
		bank.AddSlider("Cinematic pullback length - boat", &g_CinematicPullBackBoat, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Cinematic pullback length - heli", &g_CinematicPullBackHeli, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("g_MicToPlayerLocalEnvironmentRatio", &g_MicToPlayerLocalEnvironmentRatio, 0.0f, 1.0f, 0.1f);
		bank.PushGroup("Sniper specific");
			bank.AddSlider("Ambience sniper front cone vol increase (db)", &sm_SniperAmbienceVolIncrease, -100.0f, 24.f, 1.f);
			bank.AddSlider("Ambience sniper volume decrease (db)", &sm_SniperAmbienceVolDecrease, -100.0f, 24.f, 1.f);
		bank.PopGroup();
		bank.PushGroup("Cinematic microphones");
			bank.AddSlider("Delta interpolate apply per second", &sm_DeltaCApply, 0.0f, 100.f, 0.1f);
			bank.AddToggle("Equalize rolloff on cinCam changes: Listener 0", &sm_EqualizeCinRollOffFirstListener);
			bank.AddToggle("Equalize rolloff on cinCam changes: Listener 1", &sm_EqualizeCinRollOffSecondListener);
			bank.AddToggle("Fix cinCam listener 1 distance to entity", &sm_FixSecondMicDistance);
			bank.AddToggle("unify vol mic in cinCam", &sm_UnifyCinematicVolMatrix);
			bank.AddToggle("Apply the zooming algorith to the first listener", &sm_ZoomOnFirstListener);
			bank.AddSlider("Move paning matrix to vol position",&sm_PaningToVolInterp, 0.0f, 1.f, 0.01f);
			bank.PopGroup();
		bank.PushGroup("Replay microphones");
			bank.AddCombo("Mic type", &sm_ReplayMicType, NUM_REPLAY_MICS, &g_ReplayMicType[0], 0, NullCB, "MIc type" );
			bank.PopGroup();
	bank.PopGroup();
}
#endif


