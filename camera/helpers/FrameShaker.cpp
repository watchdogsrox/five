//
// camera/helpers/FrameShaker.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/FrameShaker.h"

#include "bank/bank.h"
#include "fwmaths/angle.h"
#include "grcore/debugdraw.h"
#include "profile/element.h"
#include "profile/group.h"
#include "profile/page.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/Oscillator.h"
#include "camera/system/CameraMetadata.h"
#include "camera/system/CameraFactory.h"
#include "system/controlMgr.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camOscillatingFrameShaker,0xB5C4028C)

#if __BANK
PF_PAGE(camOscillatingFrameShakerPage, "Camera Frame Shaker");

PF_GROUP(camOscillatingFrameShakerDetails);
PF_LINK(camOscillatingFrameShakerPage, camOscillatingFrameShakerDetails);

PF_VALUE_FLOAT(camOscillatingFrameShakerComponentOutput, camOscillatingFrameShakerDetails);

extern const char* parser_eShakeFrameComponent_Strings[];

static const u32	g_DebugNumStockShakes = 86;
static const char*	g_DebugStockShakeNames[g_DebugNumStockShakes] =
{
	"SMALL_EXPLOSION_SHAKE",
	"MEDIUM_EXPLOSION_SHAKE",
	"LARGE_EXPLOSION_SHAKE",
	"REPLAY_EXPLOSION_SHAKE",
	"GAMEPLAY_EXPLOSION_SHAKE",
	"GRENADE_EXPLOSION_SHAKE",
	"JOLT_SHAKE",
	"VEH_IMPACT_PITCH_SHAKE",
	"VEH_IMPACT_HEADING_SHAKE",
	"VEH_IMPACT_PITCH_HEADING_SHAKE_FPS",
	"BOAT_WATER_ENTRY_SHAKE",
	"FOLLOW_RUN_SHAKE",
	"CINEMATIC_SHOOTING_RUN_SHAKE",
	"FOLLOW_SWIM_SHAKE",
	"VIBRATE_SHAKE",
	"WOBBLY_SHAKE",
	"REPLAY_HAND_SHAKE",
	"HAND_SHAKE",
	"DAMPED_HAND_SHAKE",
	"SWITCH_HAND_SHAKE",
	"STUNT_HAND_SHAKE",
	"IDLE_HAND_SHAKE",
	"KILL_SHOT_SHAKE",
	"HIGH_SPEED_VIBRATION_POV_SHAKE",
	"ROAD_VIBRATION_SHAKE",
	"REPLAY_HIGH_SPEED_VEHICLE_SHAKE",
	"HIGH_SPEED_VEHICLE_SHAKE",
	"HIGH_SPEED_BOAT_SHAKE",
	"PLANE_PART_SPEED_SHAKE",
	"FIRST_PERSON_AIM_SHAKE",
	"REPLAY_SKY_DIVING_SHAKE",
	"SKY_DIVING_SHAKE",
	"PARACHUTING_SHAKE",
	"CAMERA_OPERATOR_SHAKE_X",
	"CAMERA_OPERATOR_SHAKE_Z",
	"CAMERA_OPERATOR_TURBULENCE_SHAKE",
	"HIGH_DIVE_SHAKE",
	"HIGH_FALL_SHAKE",
	"HAND_SHAKE_ROLL",
	"POV_IDLE_SHAKE",
	"HIGH_SPEED_POV_SHAKE",
	"REPLAY_DRUNK_SHAKE",
	"DRUNK_SHAKE",
	"FAMILY5_DRUG_TRIP_SHAKE",
	"LOW_ORBIT_INACCURACY_CAMERA_SHAKE",
	"LOW_ORBIT_HIGH_SPEED_CAMERA_SHAKE",
	"WATER_BOB_SHAKE",
	"DEFAULT_DEPLOY_PARACHUTE_SHAKE",
	"FIRST_PERSON_DEPLOY_PARACHUTE_SHAKE",
	"DEFAULT_KILL_EFFECT_SHAKE",
	"DEATH_FAIL_IN_EFFECT_SHAKE",
	"DEATH_FAIL_OUT_EFFECT_SHAKE",
	"FPS_ZOOM_IN_SHAKE",
	"FPS_BULLET_HIT_SHAKE",
	"FPS_MELEE_HIT_SHAKE",
	"FPS_VEHICLE_HIT_SHAKE",
	"FPS_STEERING_WHEEL_HIT_SHAKE",
	"FPS_DEATH_SHAKE",
	"FPS_THROW_SHAKE",
	"FPS_MAG_DROP_SHAKE",
	"FPS_MAG_RELOAD_SHAKE",
	"FPS_BOLT_RELOAD_SHAKE",
	"DEFAULT_THIRD_PERSON_RECOIL_SHAKE",
	"DEFAULT_THIRD_PERSON_ACCURACY_OFFSET_SHAKE",
	"PISTOL_RECOIL_SHAKE",
	"SMG_RECOIL_SHAKE",
	"ASSAULT_RIFLE_RECOIL_SHAKE",
	"MG_RECOIL_SHAKE",
	"MINIGUN_RECOIL_SHAKE",
	"DEFAULT_FIRST_PERSON_RECOIL_SHAKE",
	"FPS_SHOTGUN_PUMP_SHAKE",
	"SHOTGUN_RECOIL_SHAKE",
	"RPG_RECOIL_SHAKE",
	"GRENADE_LAUNCHER_RECOIL_SHAKE",
	"TANK_RECOIL_SHAKE",
	"FPS_PISTOL_RECOIL_SHAKE",
	"FPS_SMG_RECOIL_SHAKE",
	"FPS_ASSAULT_RIFLE_RECOIL_SHAKE",
	"FPS_MG_RECOIL_SHAKE",
	"FPS_MINIGUN_RECOIL_SHAKE",
	"FPS_SHOTGUN_RECOIL_SHAKE",
	"FPS_RPG_RECOIL_SHAKE",
	"FPS_GRENADE_LAUNCHER_RECOIL_SHAKE",
	"FPS_TANK_RECOIL_SHAKE",
	"CARBINE_RIFLE_RECOIL_SHAKE",
	"CARBINE_RIFLE_ACCURACY_OFFSET_SHAKE"
};

char	camOscillatingFrameShaker::ms_DebugCustomShakeName[100];
s32		camOscillatingFrameShaker::ms_DebugStockShakeNameIndex = 0;
s32		camOscillatingFrameShaker::ms_ShakeComponentToGraph = 0;
float	camOscillatingFrameShaker::ms_DebugShakeAmplitude = 1.0f;
float	camOscillatingFrameShaker::ms_ShakeComponentValueToGraph = 0.0f;
bool	camOscillatingFrameShaker::ms_DebugShouldStopShakingImmediately = false;
bool	camOscillatingFrameShaker::ms_RenderShakePath = false; 
#endif // __BANK


camOscillatingFrameShaker::camOscillatingFrameShaker(const camBaseObjectMetadata& metadata)
: camBaseFrameShaker(metadata)
, m_Metadata(static_cast<const camShakeMetadata&>(metadata))
, m_FrequencyScale(1.0f)
{
	//Instantiate all the oscillators and (optionally) envelopes.
	const int numMetadataComponents = m_Metadata.m_FrameComponents.GetCount();
	m_Components.Reserve(numMetadataComponents);
	for(int i=0; i<numMetadataComponents; i++)
	{
		const camShakeMetadataFrameComponent& metadataComponent = m_Metadata.m_FrameComponents[i];

		tShakeComponent& shakeComponent = m_Components.Append();

		shakeComponent.m_FrameComponent = (u32)metadataComponent.m_Component;

		const camOscillatorMetadata* oscillatorMetadata =
			camFactory::FindObjectMetadata<camOscillatorMetadata>(metadataComponent.m_OscillatorRef.GetHash());
		if(oscillatorMetadata)
		{
			shakeComponent.m_Oscillator = camFactory::CreateObject<camOscillator>(*oscillatorMetadata);
			if(cameraVerifyf(shakeComponent.m_Oscillator, "A camera frame shaker (name: %s, hash: %u) failed to create an oscillator (name: %s, hash: %u)",
				GetName(), GetNameHash(), SAFE_CSTRING(oscillatorMetadata->m_Name.GetCStr()),
				oscillatorMetadata->m_Name.GetHash()))
			{
				shakeComponent.m_Oscillator->SetUseGameTime(m_Metadata.m_UseGameTime);
			}
		}

		const camEnvelopeMetadata* envelopeMetadata =
			camFactory::FindObjectMetadata<camEnvelopeMetadata>(metadataComponent.m_EnvelopeRef.GetHash());
		if(envelopeMetadata)
		{
			shakeComponent.m_Envelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);

			if(cameraVerifyf(shakeComponent.m_Envelope, "A camera frame shaker (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
				GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()),
				envelopeMetadata->m_Name.GetHash()))
			{
				shakeComponent.m_Envelope->SetUseGameTime(m_Metadata.m_UseGameTime);	// Set flag first or start time will be incorrect.
				shakeComponent.m_Envelope->SetReversible(m_Metadata.m_IsReversible);
				shakeComponent.m_Envelope->Start();
			}
		}
	}
}

camOscillatingFrameShaker::~camOscillatingFrameShaker()
{
	//Clean-up the components.
	const int numComponents = m_Components.GetCount();
	for(int i=0; i<numComponents; i++)
	{
		tShakeComponent& shakeComponent = m_Components[i];

		if(shakeComponent.m_Oscillator)
		{
			delete shakeComponent.m_Oscillator;
		}

		if(shakeComponent.m_Envelope)
		{
			delete shakeComponent.m_Envelope;
		}
	}

	m_Components.Reset();
}

void camOscillatingFrameShaker::OverrideStartTime(u32 startTime)
{
	camBaseFrameShaker::OverrideStartTime(startTime);

	const int numComponents = m_Components.GetCount();
	for(int i=0; i<numComponents; i++)
	{
		tShakeComponent& shakeComponent = m_Components[i];

		if (shakeComponent.m_Envelope)
		{
			shakeComponent.m_Envelope->OverrideStartTime(startTime);
		}
	}
}

void camOscillatingFrameShaker::SetUseNonPausedCameraTime(bool b)
{
	camBaseFrameShaker::SetUseNonPausedCameraTime(b);

	const int numComponents = m_Components.GetCount();
	for(int i=0; i<numComponents; i++)
	{
		tShakeComponent& shakeComponent = m_Components[i];

		if(shakeComponent.m_Oscillator)
		{
			shakeComponent.m_Oscillator->SetUseNonPausedCameraTime(b);
		}
		
		if (shakeComponent.m_Envelope)
		{
			shakeComponent.m_Envelope->SetUseNonPausedCameraTime(b);
			if (b)
			{
				shakeComponent.m_Envelope->SetReversible(false);
			}
		}
	}
}

bool camOscillatingFrameShaker::UpdateFrame(float overallEnvelopeLevel, camFrame& frameToShake)
{
	//Check that we have at least one active component to update.
	bool hasAnActiveComponent = false;
	const int numComponents = m_Components.GetCount();
	for(int i=0; i<numComponents; i++)
	{
		tShakeComponent& shakeComponent = m_Components[i];
		if(!shakeComponent.m_Envelope || shakeComponent.m_Envelope->IsActive())
		{
			hasAnActiveComponent = true;
			break;
		}
	}

	if(!hasAnActiveComponent)
	{
		return false;
	}

	//Update the components.
	float vibrationScale = 0.0f;
	for(u32 i=0; i<numComponents; i++)
	{
		tShakeComponent& component = m_Components[i];
		if(component.m_Oscillator)
		{
			float componentEnvelopeLevel = 1.0f;
			if(component.m_Envelope)
			{
				if(component.m_Envelope->IsActive())
				{
					componentEnvelopeLevel = component.m_Envelope->Update(REPLAY_ONLY(m_TimeScaleFactor, m_TimeOffset));
				}
				else
				{
					continue; //The envelope has completed, so this component does not need to be updated.
				}
			}

			float shakeValue = component.m_Oscillator->Update(m_FrequencyScale REPLAY_ONLY(, m_TimeScaleFactor, m_TimeOffset));
			//Apply any component enveloping.
			shakeValue *= componentEnvelopeLevel;
			//Apply any overall enveloping.
			shakeValue *= overallEnvelopeLevel;
			//Apply any amplitude scaling.
			shakeValue *= m_Amplitude;

			vibrationScale = Max(vibrationScale, componentEnvelopeLevel*overallEnvelopeLevel*m_Amplitude);

#if __BANK
			if(component.m_FrameComponent == static_cast<u32>(ms_ShakeComponentToGraph))
			{
				ms_ShakeComponentValueToGraph += shakeValue;
			}
#endif // __BANK

			AppyShakeToFrameComponent(shakeValue, frameToShake, component.m_FrameComponent);
		}
	}

	if (m_Metadata.m_Vibration > 0.0f && vibrationScale > 0.10f)
	{
	#if __PS3 || RSG_ORBIS
		const float c_MinIntensity = 0.05f;
	#else
		const float c_MinIntensity = 0.10f;
	#endif
		float intensity = Clamp(m_Metadata.m_Vibration * vibrationScale, c_MinIntensity, 1.0f);
		CControlMgr::StartPlayerPadShakeByIntensity(10, intensity);
	}

	if (m_FrequencyScale != 1.0f)
	{
		// Blend frequency scale back to 1.0f
		float fFreqModifier = 1.0f - m_FrequencyScale;
		fFreqModifier *= GetReleaseLevel();
		m_FrequencyScale = 1.0f + fFreqModifier;
	}

#if __BANK	
	RenderShakePath();
#endif

	return true;
}

bool camOscillatingFrameShaker::ShakeFrame(float overallEnvelopeLevel, camFrame& frameToShake) const
{
	//Check that we have at least one active component to apply.
	bool hasAnActiveComponent = false;
	const int numComponents = m_Components.GetCount();
	for(int i=0; i<numComponents; i++)
	{
		const tShakeComponent& shakeComponent = m_Components[i];
		if(!shakeComponent.m_Envelope || shakeComponent.m_Envelope->IsActive())
		{
			hasAnActiveComponent = true;
			break;
		}
	}

	if(!hasAnActiveComponent)
	{
		return false;
	}

	//Apply the components.
	for(u32 i=0; i<numComponents; i++)
	{
		const tShakeComponent& component = m_Components[i];
		if(component.m_Oscillator)
		{
			float componentEnvelopeLevel = 1.0f;
			if(component.m_Envelope)
			{
				if(component.m_Envelope->IsActive())
				{
					componentEnvelopeLevel = component.m_Envelope->GetLevel();
				}
				else
				{
					continue; //The envelope has completed, so this component does not need to be updated.
				}
			}

			float shakeValue = component.m_Oscillator->GetLevel();
			//Apply any component enveloping.
			shakeValue *= componentEnvelopeLevel;
			//Apply any overall enveloping.
			shakeValue *= overallEnvelopeLevel;
			//Apply any amplitude scaling.
			shakeValue *= m_Amplitude;

			AppyShakeToFrameComponent(shakeValue, frameToShake, component.m_FrameComponent);
		}
	}

	return true;
}

void camOscillatingFrameShaker::AppyShakeToFrameComponent(float shakeValue, camFrame& frameToShake, u32 frameComponent) const
{
	if(shakeValue == 0.0f)
	{
		return; //The shakeValue is applied as an offset.
	}

	switch((eShakeFrameComponent)frameComponent)
	{
	case SHAKE_COMP_WORLD_POS_X:
		{
			Vector3 position = frameToShake.GetPosition();
			position.x += shakeValue;
			frameToShake.SetPosition(position);
		}
		break;

	case SHAKE_COMP_WORLD_POS_Y:
		{
			Vector3 position = frameToShake.GetPosition();
			position.y += shakeValue;
			frameToShake.SetPosition(position);
		}
		break;

	case SHAKE_COMP_WORLD_POS_Z:
		{
			Vector3 position = frameToShake.GetPosition();
			position.z += shakeValue;
			frameToShake.SetPosition(position);
		}
		break;

	case SHAKE_COMP_REL_POS_X:
		{
			Vector3 position = frameToShake.GetPosition();
			Vector3 offset = shakeValue * frameToShake.GetRight();
			position += offset;
			frameToShake.SetPosition(position);
		}
		break;

	case SHAKE_COMP_REL_POS_Y:
		{
			Vector3 position = frameToShake.GetPosition();
			Vector3 offset = shakeValue * frameToShake.GetFront();
			position += offset;
			frameToShake.SetPosition(position);
		}
		break;

	case SHAKE_COMP_REL_POS_Z:
		{
			Vector3 position = frameToShake.GetPosition();
			Vector3 offset = shakeValue * frameToShake.GetUp();
			position += offset;
			frameToShake.SetPosition(position);
		}
		break;

	case SHAKE_COMP_ROT_X:
		{
			Matrix34 worldMatrix = frameToShake.GetWorldMatrix();
			worldMatrix.RotateLocalX(shakeValue);
			frameToShake.SetWorldMatrix(worldMatrix);
		}
		break;

	case SHAKE_COMP_ROT_Y:
		{
			Vector3 eulers;
			frameToShake.GetWorldMatrix().ToEulersYXZ(eulers);
			eulers.y += shakeValue;
			eulers.y = fwAngle::LimitRadianAngleSafe(eulers.y);
			Matrix34 worldMatrix;
			worldMatrix.FromEulersYXZ(eulers);
			frameToShake.SetWorldMatrix(worldMatrix);
		}
		break;

	case SHAKE_COMP_ROT_Z:
		{
			Vector3 eulers;
			frameToShake.GetWorldMatrix().ToEulersYXZ(eulers);
			eulers.z += shakeValue;
			eulers.z = fwAngle::LimitRadianAngleSafe(eulers.z);
			Matrix34 worldMatrix;
			worldMatrix.FromEulersYXZ(eulers);
			frameToShake.SetWorldMatrix(worldMatrix);
		}
		break;

	case SHAKE_COMP_FOV:
		{
			float fov = frameToShake.GetFov();
			fov += shakeValue;
			//Applying an absolute offset to the rendered FOV is inherently dangerous at high or low zoom, so clamp here for safety.
			//It is safer to use SHAKE_COMP_ZOOM_FACTOR, so deprecate this component.
			fov = Clamp(fov, g_MinFov, g_MaxFov);
			frameToShake.SetFov(fov);
		}
		break;

	case SHAKE_COMP_NEAR_DOF:
		{
			float nearDof = frameToShake.GetNearInFocusDofPlane();
			nearDof += shakeValue;
			frameToShake.SetNearInFocusDofPlane(nearDof);
		}
		break;

	case SHAKE_COMP_FAR_DOF:
		{
			float farDof = frameToShake.GetFarInFocusDofPlane();
			farDof += shakeValue;
			frameToShake.SetFarInFocusDofPlane(farDof);
		}
		break;

	case SHAKE_COMP_MOTION_BLUR:
		{
			float strength = frameToShake.GetMotionBlurStrength();
			strength += shakeValue;
			frameToShake.SetMotionBlurStrength(strength);
		}
		break;

	case SHAKE_COMP_ZOOM_FACTOR:
		{
			const float zoomFactor = 1.0f + shakeValue;
			if(zoomFactor >= SMALL_FLOAT)
			{
				float fov = frameToShake.GetFov();
                fov /= zoomFactor;
                //always keep the fov inside the valid range.
				fov = Clamp(fov, g_MinFov, g_MaxFov); 
				frameToShake.SetFov(fov);
			}
		}
		break;

	case SHAKE_COMP_FULL_SCREEN_BLUR:
		{
			float blendLevel = frameToShake.GetDofFullScreenBlurBlendLevel();
			blendLevel += shakeValue;
			frameToShake.SetDofFullScreenBlurBlendLevel(blendLevel);
		}
		break;

	default:
		cameraErrorf("Invalid camera frame component in shaker");
	}

// 	TODO: Make this work again after the removal of camManager::GetLastFinalEffectiveLeafNodeFrame().
// 	
// 		// SHAKE_PAD
// 		if (0)//m_pFrameToShake && 
// 			//m_pFrameToShake == camManager::GetLastFinalEffectiveLeafNodeFrame() && 
// 			//!CGameWorld::GetMainPlayerInfo()->AreControlsDisabled() )
// 		{
// 			const float fRotationAngle = r.AngleX(Vector3(1.0f, 1.0f, 1.0f));
// 			/*				u8 iVal = (u8)(fRotationAngle/1.5f * 2.5f);
// 			#if __BANK
// 			if (CControlMgr::sm_bUseThisMultiplier)
// 			iVal = (u8)(fRotationAngle/1.5f * CControlMgr::sm_fMultiplier);
// 			#endif // __BANK
// 	
// 			const u8 iFreq = (iVal > 50) ? 50 : iVal;
// 			if (iFreq>0)
// 			{ // float camOscillatingFrameShaker::ms_drunkStrength = 1.0f; // really pissed = 1.0f.... sober as a judge = 0.0f
// 			const u16 iLength = (iFreq > 30) ? 30 : iFreq;
// 			CControlMgr::StartPlayerPadShake(iLength , iFreq);
// 			}
// 			*/				
// 			if(!CControlMgr::IsPlayerPadShaking())
// 			{
// 				static dev_float fRumbleStrengthMult = 0.02f;
// 				const float fRumbleIntensity = rage::Cosf(fRotationAngle) * fRumbleStrengthMult;
// 				const u32 iDuration = 30;
// 				CControlMgr::StartPlayerPadShakeByIntensity(iDuration,fRumbleIntensity);
// 			}
// 		}
}

void camOscillatingFrameShaker::AutoStart()
{
	camBaseFrameShaker::AutoStart();

	//Auto-start all envelopes associated with the components.
	const int numComponents = m_Components.GetCount();
	for(u32 i=0; i<numComponents; i++)
	{
		tShakeComponent& component	= m_Components[i];
		camEnvelope* envelope		= component.m_Envelope;
		if(envelope)
		{
			envelope->AutoStart();
		}
	}
}

void camOscillatingFrameShaker::SetFreqScale(float scale)
{
	if (scale != 1.0f)
	{
		m_FrequencyScale = scale;
		if (m_OverallEnvelope)
		{
			m_OverallEnvelope->AutoStart();
		}
	}
	else
	{
		m_FrequencyScale = scale;
	}
}

void camOscillatingFrameShaker::PostUpdateClass()
{
#if __STATS && __BANK
	PF_SET(camOscillatingFrameShakerComponentOutput, ms_ShakeComponentValueToGraph);
	ms_ShakeComponentValueToGraph = 0.0f;
#endif // __STATS && __BANK
}

#if __BANK
void camOscillatingFrameShaker::AddWidgets(bkBank &bank)
{
	ms_DebugCustomShakeName[0] = '\0';

	bank.PushGroup("Frame shaker", false);
	{
		bank.AddCombo("Stock shake name", &ms_DebugStockShakeNameIndex, g_DebugNumStockShakes, g_DebugStockShakeNames);
		bank.AddText("Custom shake name", ms_DebugCustomShakeName, 100);
		bank.AddSlider("Shake amplitude", &ms_DebugShakeAmplitude, 0.0f, 10.0f, 0.1f);
		bank.AddButton("Shake gameplay director", datCallback(CFA(ShakeGameplayDirector)));
		bank.AddButton("Stop shaking gameplay director", datCallback(CFA(StopShakingGameplayDirector)));
		bank.AddToggle("Stop shaking immediately", &ms_DebugShouldStopShakingImmediately);
		bank.AddToggle("Render shake path", &ms_RenderShakePath);
		bank.AddCombo("Shake component to graph", &ms_ShakeComponentToGraph, eShakeFrameComponent_NUM_ENUMS, parser_eShakeFrameComponent_Strings);
	}
	bank.PopGroup(); //Frame shaker.
}

void camOscillatingFrameShaker::ShakeGameplayDirector()
{
	const char* shakeName = (strlen(ms_DebugCustomShakeName) > 0) ? ms_DebugCustomShakeName : g_DebugStockShakeNames[ms_DebugStockShakeNameIndex];
	camInterface::GetGameplayDirector().Shake(shakeName, ms_DebugShakeAmplitude);
}

void camOscillatingFrameShaker::RenderShakePath()
{
	if(ms_RenderShakePath)
	{
		const Matrix34& mMat = camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix(); 
		
		Vector3 vOffset (0.0f, 1.0f, 0.0f); 

		mMat.Transform(vOffset); 
		
		grcDebugDraw::Sphere(vOffset, 0.01f, Color32(255, 0, 0), true); 
	}
}

void camOscillatingFrameShaker::StopShakingGameplayDirector()
{
	camInterface::GetGameplayDirector().StopShaking(ms_DebugShouldStopShakingImmediately);
}
#endif // __BANK
