#include "vehicleengine.h"
#include "audioeffecttypes/biquadfiltereffect.h"
#include "audioeffecttypes/waveshapereffect.h"
#include "audioengine/category.h"
#include "audioengine/engine.h"
#include "audiohardware/submix.h"
#include "audio/caraudioentity.h"
#include "audio/boataudioentity.h"
#include "audio/vehicleaudioentity.h"
#include "audiohardware/granularmix.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "camera/CamInterface.h"
#include "peds/PlayerInfo.h"
#include "peds/PedIntelligence.h"

#include "game/weather.h"
#include "timecycle/TimeCycle.h"
#include "vehicles/vehicle.h"
#include "vehicles/Submarine.h"

AUDIO_VEHICLES_OPTIMISATIONS()

audCurve audVehicleEngine::sm_RevsOffVolCurve;
audCurve audVehicleEngine::sm_ExhaustRattleVol;
audCurve audVehicleEngine::sm_StartLoopVolCurve;

f32 audVehicleEngine::sm_AutoShutOffCutoffTime = 3.0f;
u32 audVehicleEngine::sm_AutoShutOffIgnitionTime = 200;
f32 audVehicleEngine::sm_AutoShutOffRestartDelay = 0.1f;

u32 g_ScreenFadedEngineSwitchTime = 200;
u32 g_SportsCarRevsEngineSwitchTime = 200;
u32 g_RocketBoostCooldownTime = 10000;

extern u32 g_EngineStopDuration;
extern f32 g_RevMaxIncreaseRate; 
extern f32 g_RevMaxDecreaseRate;
extern f32 g_ExhaustPopRateThreshold;
extern f32 g_audEngineSteamSoundVolRange;
extern f32 g_EngineVolumeTrim;
extern u32 g_EngineSwitchFadeInTime;
extern u32 g_EngineSwitchFadeOutTime;

extern audCategory* g_EngineCategory;
extern audCategory* g_EngineLoudCategory;

extern const u32 g_TemperatureVariableHash;
extern const u32 g_VariationVariableHash; 

extern audCurve g_EnginePowerToDistortion;
extern bool g_GranularEnabled;
extern bool g_GranularEnableNPCGranularEngines;
extern bool g_GranularForceLowQuality;
extern bool g_GranularForceHighQuality;

#if __BANK
extern bool g_GranularForceRevLimiter;
extern bool g_MuteFanbeltDamage;
extern bool g_ForceDamage;
extern f32 g_ForcedDamageFactor;
#endif

// -------------------------------------------------------------------------------
// audVehicleKersSystem constructor
// -------------------------------------------------------------------------------
audVehicleKersSystem::audVehicleKersSystem() :
	  m_Parent(NULL)
	, m_KERSBoostLoop(NULL)
	, m_KERSRechargeLoop(NULL)
	, m_JetWhineSound(NULL)
	, m_KersState(AUD_KERS_STATE_OFF)
	, m_KersRechargeRate(0.0f)
	, m_LastKersBoostTime(0u)
	, m_LastKersBoostFailTime(0u)
{
}

// -------------------------------------------------------------------------------
// audVehicleKersSystem Init
// -------------------------------------------------------------------------------
void audVehicleKersSystem::Init(audVehicleAudioEntity* parent)
{
	m_Parent = parent;
}

// -------------------------------------------------------------------------------
// audVehicleKersSystem StopAllSounds
// -------------------------------------------------------------------------------
void audVehicleKersSystem::StopAllSounds()
{
	m_Parent->StopAndForgetSoundsContinueUpdating(m_KERSBoostLoop, m_KERSRechargeLoop, m_JetWhineSound);
	m_KersState = AUD_KERS_STATE_OFF;
}

// -------------------------------------------------------------------------------
// audVehicleKersSystem StopJetWhineSound
// -------------------------------------------------------------------------------
void audVehicleKersSystem::StopJetWhineSound()
{
	if(m_JetWhineSound)
	{
		m_JetWhineSound->StopAndForget(true);
	}
}

// -------------------------------------------------------------------------------
// audVehicleKersSystem Update
// -------------------------------------------------------------------------------
void audVehicleKersSystem::Update()
{
	if(m_Parent)
	{
		bool boostBoneAboveWater = true;

		if (m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("Toreador", 0x56C8A5EF))
		{
			boostBoneAboveWater = false;

			for (int i = 0; i < VFXVEHICLE_NUM_ROCKET_BOOSTS; i++)
			{
				Mat34V boneMtx;
				CVehicle* pVehicle = m_Parent->GetVehicle();
				CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
				s32 rocketBoostBoneId = pModelInfo->GetBoneIndex(VEH_ROCKET_BOOST + i);

				if (i == 0 && rocketBoostBoneId == -1)
				{
					rocketBoostBoneId = pModelInfo->GetBoneIndex(VEH_EXHAUST);
				}

				if (rocketBoostBoneId > -1)
				{
					CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pVehicle, rocketBoostBoneId);

					if (!IsZeroAll(boneMtx.GetCol0()))
					{
						f32 waterDepth;
						CVfxHelper::GetWaterDepth(boneMtx.GetCol3(), waterDepth);

						if (waterDepth <= 0.0f)
						{
							boostBoneAboveWater = true;
						}
					}
				}
			}
		}

		if(!m_Parent->IsDisabled())
		{
			const u32 kersSoundsetName = GetKersSoundSetName();

			switch(m_KersState)
			{
			case AUD_KERS_STATE_BOOSTING:
				{
					if(!m_KERSBoostLoop)
					{
						audSoundSet kersSoundset;

						if(kersSoundset.Init(kersSoundsetName))
						{
							audSoundInitParams initParams;
							initParams.UpdateEntity = true;
							initParams.u32ClientVar = AUD_VEHICLE_SOUND_ENGINE;			
							initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
							m_Parent->CreateAndPlaySound_Persistent(kersSoundset.Find(ATSTRINGHASH("activate", 0xE64E0807)), &m_KERSBoostLoop, &initParams);

							if(m_KERSBoostLoop)
							{
								REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(kersSoundsetName, ATSTRINGHASH("activate", 0xE64E0807), &initParams, m_KERSBoostLoop, m_Parent->GetVehicle());)
							}
						}
					}

					if(m_KERSBoostLoop)
					{
						m_KERSBoostLoop->FindAndSetVariableValue(ATSTRINGHASH("chargeRemaining", 0x44E9F02C), GetKersBoostRemaining());
						m_KERSBoostLoop->FindAndSetVariableValue(ATSTRINGHASH("underwater", 0xA2663418), boostBoneAboveWater ? 0.f : 1.f);

						if(m_Parent->GetVehicle()->InheritsFromSubmarineCar())
						{
							m_KERSBoostLoop->FindAndSetVariableValue(ATSTRINGHASH("submarinemode", 0x331C9916), static_cast<CSubmarineCar*>(m_Parent->GetVehicle())->IsInSubmarineMode() ? 1.f : 0.f);
						}

						m_LastKersBoostTime = fwTimer::GetTimeInMilliseconds();
					}

					if(m_KERSRechargeLoop)
					{
						m_KERSRechargeLoop->StopAndForget(true);
					}
				}
				break;

			case AUD_KERS_STATE_RECHARGING:
				{
					if(m_KersRechargeRate > 0.0f)
					{
						if(!m_KERSRechargeLoop)
						{
							audSoundSet kersSoundset;

							if(kersSoundset.Init(kersSoundsetName))
							{
								audSoundInitParams initParams;
								initParams.UpdateEntity = true;
								initParams.u32ClientVar = AUD_VEHICLE_SOUND_ENGINE;			
								initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
								m_Parent->CreateAndPlaySound_Persistent(kersSoundset.Find(ATSTRINGHASH("recharge", 0xFB025813)), &m_KERSRechargeLoop, &initParams);

								if(m_KERSRechargeLoop)
								{
									REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(kersSoundsetName, ATSTRINGHASH("recharge", 0xFB025813), &initParams, m_KERSRechargeLoop, m_Parent->GetVehicle());)
								}
							}
						}

						if(m_KERSRechargeLoop)
						{
							m_KERSRechargeLoop->FindAndSetVariableValue(ATSTRINGHASH("rate", 0x7E68C088), m_KersRechargeRate * 30);
							m_KERSRechargeLoop->FindAndSetVariableValue(ATSTRINGHASH("chargeRemaining", 0x44E9F02C), GetKersBoostRemaining());
						}
					}
					else if(m_KERSRechargeLoop)
					{
						m_KERSRechargeLoop->StopAndForget(true);
					}

					if(m_KERSBoostLoop)
					{
						m_KERSBoostLoop->StopAndForget(true);
					}
				}
				break;

			default:
				m_Parent->StopAndForgetSoundsContinueUpdating(m_KERSRechargeLoop, m_KERSBoostLoop);
				break;
			}		
		}		
		else
		{
			m_Parent->StopAndForgetSoundsContinueUpdating(m_KERSRechargeLoop, m_KERSBoostLoop);
		}
	}	
}

// -------------------------------------------------------------------------------
// audVehicleKersSystem GetKersSoundSetName
// -------------------------------------------------------------------------------
u32 audVehicleKersSystem::GetKersSoundSetName() const
{
	// Temp fix for V DLC - should be moved into audio metadata for future projects
	const u32 modelNameHash = m_Parent->GetVehicleModelNameHash();

	if(modelNameHash == ATSTRINGHASH("LECTRO", 0x26321E67))
	{
		return ATSTRINGHASH("LECTRO_KERS_SOUNDSET", 0x9FDAAA0C);
	}
	else if(modelNameHash == ATSTRINGHASH("VINDICATOR", 0xAF599F01))
	{
		return ATSTRINGHASH("VINDICATOR_KERS_SOUNDSET", 0xD46FFCB6);
	}
	else if (modelNameHash == ATSTRINGHASH("Toreador", 0x56C8A5EF))
	{
		if (m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("dlc_h4_toreador_boost_sounds", 0xE5D0ADAF);
		}
		else
		{
			return ATSTRINGHASH("dlc_h4_toreador_boost_npc_sounds", 0x7DBB48FC);
		}
	}
	else if (modelNameHash == ATSTRINGHASH("FORMULA", 0x1446590A))
	{
		if (m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("dlc_ch_formula_kers_sounds", 0x44072C9D);
		}
		else
		{
			return ATSTRINGHASH("dlc_ch_formula_kers_npc_sounds", 0x2DA99AD9);
		}
	}
	else if (modelNameHash == ATSTRINGHASH("FORMULA2", 0x8B213907))
	{	
		if (m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("dlc_ch_formula2_kers_sounds", 0x695BBFA8);
		}
		else
		{
			return ATSTRINGHASH("dlc_ch_formula2_kers_npc_sounds", 0xFE208B94);
		}
	}
	else if (modelNameHash == ATSTRINGHASH("OPENWHEEL1", 0x58F77553))
	{
		if (m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("dlc_sum20_openwheel1_kers_sounds", 0x893FE1E0);
		}
		else
		{
			return ATSTRINGHASH("dlc_sum20_openwheel1_kers_npc_sounds", 0x2D7A59BF);
		}
	}
	else if (modelNameHash == ATSTRINGHASH("OPENWHEEL2", 0x4669D038))
	{
		if (m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("dlc_sum20_openwheel2_kers_sounds", 0x4A06F8CC);
		}
		else
		{
			return ATSTRINGHASH("dlc_sum20_openwheel2_kers_npc_sounds", 0x9D80AEDC);
		}
	}
	else if(modelNameHash == ATSTRINGHASH("STARLING", 0x9A9EB7DE))
	{
		if(m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("DLC_Smuggler_Starling_Sounds", 0xC99D5122);
		}
		else
		{
			return ATSTRINGHASH("DLC_Smuggler_Starling_NPC_Sounds", 0x1C2ADE23);
		}
	}
	else if(modelNameHash == ATSTRINGHASH("MOGUL", 0xD35698EF) ||
			modelNameHash == ATSTRINGHASH("TULA", 0x3E2E4F8A) || 
			modelNameHash == ATSTRINGHASH("BOMBUSHKA", 0xFE0A508C))
	{
		if(m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("DLC_SM_JATO_Sounds", 0x721906BF);
		}
		else
		{
			return ATSTRINGHASH("DLC_SM_JATO_NPC_Sounds", 0xD8F86B5F);
		}
	}	
	else if(modelNameHash == ATSTRINGHASH("VOLTIC2", 0x3AF76F4A))
	{
		if(m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("DLC_ImportExport_Voltic2_Sounds", 0x243BA50);
		}
		else
		{
			return ATSTRINGHASH("DLC_ImportExport_Voltic2_NPC_Sounds", 0xE2D93E04);
		}
	}
	else if(modelNameHash == ATSTRINGHASH("OPPRESSOR", 0x34B82784))
	{
		if(m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("DLC_Gunrunning_Oppressor_Sounds", 0xC98DA953);
		}
		else
		{
			return ATSTRINGHASH("DLC_Gunrunning_Oppressor_NPC_Sounds", 0x23FDA269);
		}
	}
	else if(modelNameHash == ATSTRINGHASH("VIGILANTE", 0xB5EF4C33))
	{
		if(m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("DLC_SM_Vigilante_Sounds", 0x8DA9F2D3);
		}
		else
		{
			return ATSTRINGHASH("DLC_SM_Vigilante_NPC_Sounds", 0x85F7F382);
		}
	}
	else if(modelNameHash == ATSTRINGHASH("THRUSTER", 0x58CDAF30))
	{
		if(m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("DLC_XM_JATO_Sounds", 0x1F0EE445);
		}
		else
		{
			return ATSTRINGHASH("DLC_XM_JATO_NPC_Sounds", 0x45580054);
		}
	}
	else if (modelNameHash == ATSTRINGHASH("SCRAMJET", 0xD9F0503D))
	{
		if (m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("DLC_BTL_scramjet_Sounds", 0x527F2941);
		}
		else
		{
			return ATSTRINGHASH("DLC_BTL_scramjet_NPC_Sounds", 0xD9AB36B8);
		}
	}
	else if (modelNameHash == ATSTRINGHASH("OPPRESSOR2", 0x7B54A9D3))
	{
		if (m_Parent->IsFocusVehicle())
		{
			return ATSTRINGHASH("DLC_BTL_oppressor2_Rocket_Player_Sounds", 0x5E5366A6);
		}
		else
		{
			return ATSTRINGHASH("DLC_BTL_oppressor2_Rocket_NPC_Sounds", 0x6CEEDD6F);
		}
	}

	return 0u;
}

// -------------------------------------------------------------------------------
// Set KERS system active (or not)
// -------------------------------------------------------------------------------
void audVehicleKersSystem::SetKERSState(audKersState state)
{
	m_KersState = state;

	if(m_KersState != AUD_KERS_STATE_RECHARGING)
	{
		m_KersRechargeRate = 0.0f;
	}
}

// -------------------------------------------------------------------------------
// Called when the KERS system is recharging
// -------------------------------------------------------------------------------
void audVehicleKersSystem::SetKersRechargeRate(f32 rechargeRate)
{		
	m_KersRechargeRate = rechargeRate;	
}

// -------------------------------------------------------------------------------
// Trigger KERS boost activation sound
// -------------------------------------------------------------------------------
void audVehicleKersSystem::TriggerKersBoostActivate()
{
	if(m_Parent && !m_Parent->IsDisabled())
	{
		audSoundSet kersSoundset;
		const u32 soundsetName = GetKersSoundSetName();

		if(kersSoundset.Init(soundsetName))
		{
			audMetadataRef soundRef = kersSoundset.Find(ATSTRINGHASH("maxboost", 0x79B9A1AC));

			if(soundRef.IsValid())
			{
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.u32ClientVar = AUD_VEHICLE_SOUND_ENGINE;			
				m_Parent->CreateDeferredSound(kersSoundset.Find(ATSTRINGHASH("maxboost", 0x79B9A1AC)), m_Parent->GetVehicle(), &initParams, true, true);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, ATSTRINGHASH("maxboost", 0x79B9A1AC), &initParams, m_Parent->GetVehicle()));
			}			
		}
	}	
}

// -------------------------------------------------------------------------------
// Trigger KERS boost activation fail sound
// -------------------------------------------------------------------------------
void audVehicleKersSystem::TriggerKersBoostActivateFail()
{
	if(m_Parent && !m_Parent->IsDisabled())
	{
		const u32 time = fwTimer::GetTimeInMilliseconds();

		if(time - m_LastKersBoostFailTime > 250)
		{
			audSoundSet kersSoundset;
			const u32 soundsetName = GetKersSoundSetName();		

			if(kersSoundset.Init(soundsetName))
			{
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.u32ClientVar = AUD_VEHICLE_SOUND_ENGINE;			
				m_Parent->CreateDeferredSound(kersSoundset.Find(ATSTRINGHASH("activate_fail", 0x2A684B3A)), m_Parent->GetVehicle(), &initParams, true, true);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, ATSTRINGHASH("activate_fail", 0x2A684B3A), &initParams, m_Parent->GetVehicle()));
				m_LastKersBoostFailTime = time;
			}
		}
	}
}

// -------------------------------------------------------------------------------
// audVehicleKersSystem GetKersBoostRemaining
// -------------------------------------------------------------------------------
f32 audVehicleKersSystem::GetKersBoostRemaining() const
{
	if(m_Parent->GetVehicle()->pHandling->hFlags & HF_HAS_KERS)
	{
		return m_Parent->GetVehicle()->m_Transmission.GetKERSRemaining()/CTransmission::ms_KERSDurationMax;
	}
	else if(m_Parent->GetVehicle()->HasRocketBoost())
	{
		return m_Parent->GetVehicle()->GetRocketBoostRemaining()/m_Parent->GetVehicle()->GetRocketBoostCapacity();
	}

	return 0.f;
}

// -------------------------------------------------------------------------------
// Update jet whine
// -------------------------------------------------------------------------------
void audVehicleKersSystem::UpdateJetWhine(audVehicleVariables* UNUSED_PARAM(state))
{
	if(!m_JetWhineSound)
	{
		const u32 modelNameHash = m_Parent->GetVehicleModelNameHash();

		if(modelNameHash == ATSTRINGHASH("VOLTIC2", 0x3AF76F4A) || 
		   modelNameHash == ATSTRINGHASH("OPPRESSOR", 0x34B82784) ||
		   modelNameHash == ATSTRINGHASH("VIGILANTE", 0xB5EF4C33) ||
		   modelNameHash == ATSTRINGHASH("SCRAMJET", 0xD9F0503D))
		{
			audSoundSet rocketWhineSoundSet;

			if(rocketWhineSoundSet.Init(GetKersSoundSetName()))
			{
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.u32ClientVar = AUD_VEHICLE_SOUND_ENGINE;			
				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
				m_Parent->CreateAndPlaySound_Persistent(rocketWhineSoundSet.Find(ATSTRINGHASH("jet_whine", 0x118AFEFA)), &m_JetWhineSound, &initParams);
			}
		}
	}	

	if(m_JetWhineSound)
	{
		m_JetWhineSound->FindAndSetVariableValue(ATSTRINGHASH("chargeRemaining", 0x44E9F02C), GetKersBoostRemaining());
	}
}

// -------------------------------------------------------------------------------
// audVehicleEngine constructor
// -------------------------------------------------------------------------------
audVehicleEngine::audVehicleEngine() :
  m_Parent(NULL)
, m_VehicleEngineAudioSettings(NULL)
, m_GranularEngineAudioSettings(NULL)
, m_EngineState(AUD_ENGINE_OFF)
, m_AutoShutOffSystemState(AUD_AUTOSHUTOFF_INACTIVE)
, m_WasEngineOnLastFrame(false)
, m_WasEngineStartingLastFrame(false)
, m_EngineAutoShutOffRestartTime(0u)
, m_EngineStopTime(0u)
, m_EngineLastRevs(0.0f)
, m_IgnitionVariation(0.0f)
, m_IgnitionPitch(1.0f)
, m_ForceGranularLowQuality(false)
, m_IsMisfiring(false)
, m_HasTriggeredBackfire(false)
, m_LastMisfireTime(0u)
, m_ElectricToCombustionFade(1.0f)
, m_ConsecutiveExhaustPops(0)
REPLAY_ONLY(, m_LastReplayUpdateTime(0u))
, m_HasBreakableFanbelt(false)
, m_HasDraggableExhaust(false)
, m_RevsOffLoop(NULL)
, m_UnderLoadLoop(NULL)
, m_InductionLoop(NULL)
, m_IgnitionSound(NULL)
, m_StartupSound(NULL)
, m_ExhaustRattleLoop(NULL)
, m_ExhaustDragSound(NULL)
, m_BlownExhaustLow(NULL)
, m_BlownExhaustHigh(NULL)
, m_StartLoop(NULL)
, m_EngineSteamSound(NULL)
, m_EngineCoolingSound(NULL)
, m_EngineCoolingFan(NULL)
, m_ExhaustPopSound(NULL)
, m_RevLimiterPopSound(NULL)
, m_ShutdownSound(NULL)
, m_VehicleShutdownSound(NULL)
, m_HotwireSound(NULL)
, m_FanbeltDamageSound(NULL)
, m_EngineMode(COMBUSTION)
, m_RevsOffActive(false)
, m_GranularEngineActive(false)
, m_GranularEngineQuality(audGranularMix::GrainPlayerQualityMax)
, m_HasBeenInitialised(false)
, m_QualitySettingChanged(false)
, m_NumUpdates(0)
, m_HasTriggeredLimiterPop(false)
#if GTA_REPLAY
, m_ReplayIsHotwiring(false)
#endif
{
}

// -------------------------------------------------------------------------------
// Initialise all the static data members
// -------------------------------------------------------------------------------
void audVehicleEngine::InitClass()
{
	VerifyExists(sm_RevsOffVolCurve.Init(ATSTRINGHASH("ENGINE_REVS_OFF_VOL", 0xF1078E66)), "Invalid RevsOffVolCurve");
	VerifyExists(sm_ExhaustRattleVol.Init(ATSTRINGHASH("DAMAGE_CURVES_EXHAUST_RATTLE_VOL", 0x864CBCE)), "invalid damage to exhaust rattle curve");
	VerifyExists(sm_StartLoopVolCurve.Init(ATSTRINGHASH("START_LOOP_VOL_CURVE", 0x99B00627)), "Invalid StartLoopVolCurve");	
}

// -------------------------------------------------------------------------------
// audVehicleEngine Init
// -------------------------------------------------------------------------------
void audVehicleEngine::Init(audCarAudioEntity* parent)
{
	naAssertf(parent, "Initialising engine without a valid vehicle");

	if(parent)
	{
		m_Parent = parent;
		m_VehicleEngineAudioSettings = parent->GetVehicleEngineAudioSettings();
		m_GranularEngineAudioSettings = parent->GetGranularEngineAudioSettings();
	}

	InitCommon();
	m_HasBreakableFanbelt = ENTITY_SEED_PROB(m_Parent->GetVehicle()->GetRandomSeed(), 0.1f);
	m_HasDraggableExhaust = ENTITY_SEED_PROB(m_Parent->GetVehicle()->GetRandomSeed(), 0.1f);

	m_engine.Init(parent, audVehicleEngineComponent::ENGINE_SOUND_TYPE_ENGINE);
	m_exhaust.Init(parent, audVehicleEngineComponent::ENGINE_SOUND_TYPE_EXHAUST);

	audGranularMix::audGrainPlayerQuality engineQuality = GetDesiredEngineQuality();	

	if(g_GranularEnabled && m_GranularEngineAudioSettings)
	{
		if((engineQuality == audGranularMix::GrainPlayerQualityHigh && m_GranularEngineAudioSettings->EngineAccel != g_NullSoundHash) ||
		   (engineQuality == audGranularMix::GrainPlayerQualityLow && m_GranularEngineAudioSettings->NPCEngineAccel != g_NullSoundHash && g_GranularEnableNPCGranularEngines))
		{
			m_granularEngine.Init(parent, audGranularEngineComponent::ENGINE_SOUND_TYPE_ENGINE, engineQuality);
			m_granularExhaust.Init(parent, audGranularEngineComponent::ENGINE_SOUND_TYPE_EXHAUST, engineQuality);
			m_GranularEngineActive = true;
			m_GranularEngineQuality = engineQuality;
		}
	}
	
	m_ElectricEngine.Init(parent);
	m_Turbo.Init(parent);
	m_Transmission.Init(parent);
	m_HasBeenInitialised = true;
}

// -------------------------------------------------------------------------------
// audVehicleEngine Init
// -------------------------------------------------------------------------------
void audVehicleEngine::Init(audBoatAudioEntity* parent)
{
	naAssertf(parent, "Initialising engine without a valid vehicle");

	if(parent)
	{
		m_Parent = parent;
		m_GranularEngineAudioSettings = m_Parent->GetGranularEngineAudioSettings();
	}

	InitCommon();
	m_HasBreakableFanbelt = false;
	m_HasDraggableExhaust = false;

	audGranularMix::audGrainPlayerQuality engineQuality = GetDesiredEngineQuality();

	if(g_GranularEnabled && m_GranularEngineAudioSettings)
	{
		if((engineQuality == audGranularMix::GrainPlayerQualityHigh && m_GranularEngineAudioSettings->EngineAccel != g_NullSoundHash) ||
			(engineQuality == audGranularMix::GrainPlayerQualityLow && m_GranularEngineAudioSettings->NPCEngineAccel != g_NullSoundHash && g_GranularEnableNPCGranularEngines))
		{
			m_granularEngine.Init(parent, audGranularEngineComponent::ENGINE_SOUND_TYPE_ENGINE, engineQuality);
			m_granularExhaust.Init(parent, audGranularEngineComponent::ENGINE_SOUND_TYPE_EXHAUST, engineQuality);
			m_GranularEngineActive = true;
			m_GranularEngineQuality = engineQuality;
			m_HasBeenInitialised = true;
		}
	}
}

// -------------------------------------------------------------------------------
// Get desired engine quality
// -------------------------------------------------------------------------------
audGranularMix::audGrainPlayerQuality audVehicleEngine::GetDesiredEngineQuality() const
{	
	if(g_GranularForceLowQuality)
	{
		return audGranularMix::GrainPlayerQualityLow;
	}
	else if(g_GranularForceHighQuality)
	{
		return audGranularMix::GrainPlayerQualityHigh;
	}
	else
	{
		if(m_ForceGranularLowQuality)
		{
			return audGranularMix::GrainPlayerQualityLow;
		}
		else
		{
			return m_Parent->IsFocusVehicle() ? audGranularMix::GrainPlayerQualityHigh : audGranularMix::GrainPlayerQualityLow;
		}
	}
}

// -------------------------------------------------------------------------------
// Initialise anything vehicle-independent
// -------------------------------------------------------------------------------
void audVehicleEngine::InitCommon()
{
	m_GranularEngineActive = false;
	m_AutoShutOffSystemState = AUD_AUTOSHUTOFF_INACTIVE;
	m_RevsOffLoopVolSmoother.Init(0.002f,0.001f,true);
	m_UnderLoadRatioSmoother.Init(0.004f,0.001f,true);
	m_ExhaustRattleSmoother.Init(0.0005f, true);
	m_StartLoopVolSmoother.Init(0.0004f,0.00007f);
	m_FanBeltVolumeSmoother.Init(0.0005f, 0.0015f, true);
	m_ExhaustPopSmoother.Init(0.00005f, 0.001f, true);
	m_EngineSteamFluctuator.Init(0.01f, 0.01f, 0.8f,0.95f,1.0f,1.1f,0.7f, 0.4f, 1.f);

	if( m_Parent->GetVehicle() )
	{
		m_IgnitionPitch = (f32)(150 - (m_Parent->GetVehicle()->GetRandomSeed() % 300));
	}

	m_IgnitionVariation = audEngineUtil::GetRandomNumberInRange(0.0f,1.0f);
	m_NumUpdates = 0;
	m_KersSystem.Init(m_Parent);
}

// -------------------------------------------------------------------------------
// audVehicleEngine Update
// -------------------------------------------------------------------------------
void audVehicleEngine::Update(audVehicleVariables *state)
{
#if GTA_REPLAY
	u32 currentTime = fwTimer::GetTimeInMilliseconds();

	if(CReplayMgr::IsEditModeActive())
	{
		if(currentTime < m_LastReplayUpdateTime)
		{
			m_WasEngineOnLastFrame = false;
		}
	}		
#endif

	CVehicle* vehicle = m_Parent->GetVehicle();
	const bool isEngineOn = !m_Parent->ShouldForceEngineOff() && vehicle && vehicle->m_nVehicleFlags.bEngineOn;

	if(m_Parent->HasAutoShutOffEngine())
	{
		UpdateAutoShutOffSystem(state, isEngineOn);
	}
	else
	{
		CheckForEngineStartStop(state, isEngineOn);
	}
	
	UpdateIgnition(state);

	if(m_Parent->GetDynamicWaveSlot())
	{
		if(m_EngineState != AUD_ENGINE_OFF)
		{
			if(!m_GranularEngineActive && m_VehicleEngineAudioSettings)
			{
				if(!m_engine.AreSoundsValid())
				{
					m_engine.Start();
				}
				else 
				{
					if(!m_engine.HasPrepared())
					{
						if(m_engine.Prepare())
						{
							m_engine.Play();
						}
					}
				}

				if(!m_exhaust.AreSoundsValid())
				{
					m_exhaust.Start();
				}
				else
				{
					if(!m_exhaust.HasPrepared())
					{
						if(m_exhaust.Prepare())
						{
							m_exhaust.Play();
						}
					}
				}

				if(m_engine.HasPrepared() && m_engine.HasPrepared())
				{
					if(!m_engine.AreSoundsValid())
					{
						m_engine.Stop(-1);
						m_engine.Start();
					}
					else
					{
						m_engine.Update(state);
					}

					if(!m_exhaust.AreSoundsValid())
					{
						m_exhaust.Stop(-1);
						m_exhaust.Start();
					}
					else
					{
						m_exhaust.Update(state);
					}
				}
			}
			else if(m_GranularEngineActive)
			{	
				if(!m_granularEngine.IsSoundValid(m_GranularEngineQuality))
				{
					m_granularEngine.Start();
				}
				else 
				{
					if(!m_granularEngine.HasPrepared())
					{
						if( m_granularEngine.Prepare() )
						{
							m_granularEngine.Play();
						}
					}
				}

				if(!m_granularExhaust.IsSoundValid(m_GranularEngineQuality))
				{
					m_granularExhaust.Start();
				}
				else
				{
					if(!m_granularExhaust.HasPrepared())
					{
						if( m_granularExhaust.Prepare() )
						{
							m_granularExhaust.Play();
						}
					}
				}

				if(m_granularEngine.HasPrepared() && m_granularExhaust.HasPrepared())
				{
					if(!m_granularEngine.IsSoundValid(m_GranularEngineQuality))
					{
						m_granularEngine.Stop(-1);
						m_granularEngine.Start();
					}

					if( !m_granularExhaust.IsSoundValid(m_GranularEngineQuality) )
					{
						m_granularExhaust.Stop(-1);
						m_granularExhaust.Start();
					}
				}
					
				// Updating channel volumes can potentially trigger a lot of simultaneous decodes if a new channel is turned on, so only do both engine & exhaust on the same frame for high quality cars, otherwise stagger the updates
				bool updateChannelVolumes = m_GranularEngineQuality == audGranularMix::GrainPlayerQualityHigh || m_granularEngine.HasQualitySettingChanged() || m_NumUpdates % audGranularEngineComponent::ENGINE_SOUND_TYPE_MAX == audGranularEngineComponent::ENGINE_SOUND_TYPE_ENGINE;
				m_granularEngine.Update(state, updateChannelVolumes);

				updateChannelVolumes = m_GranularEngineQuality == audGranularMix::GrainPlayerQualityHigh || m_granularExhaust.HasQualitySettingChanged() || m_NumUpdates % audGranularEngineComponent::ENGINE_SOUND_TYPE_MAX == audGranularEngineComponent::ENGINE_SOUND_TYPE_EXHAUST;
				m_granularExhaust.Update(state, updateChannelVolumes);
			}
		}
	}

	if( m_EngineState != AUD_ENGINE_OFF )
	{
		m_ElectricEngine.Update(state);
		m_Turbo.Update(state);
		m_Transmission.Update(state);

		if(m_Parent->AreExhaustPopsEnabled())
		{
			UpdateExhaustPops(state);
		}

		if( m_exhaust.HasPlayed() && 
			m_engine.HasPlayed() )
		{
			UpdateStartLoop(state);
		}

		if(m_Parent->GetVehicle()->HasRocketBoost())
		{
			m_KersSystem.UpdateJetWhine(state);	
		}

		UpdateRevsOff(state);
		UpdateEngineLoad(state);
		UpdateInduction(state);
		UpdateDamage(state);
		UpdateHybridEngine(state);
		UpdateExhaustRattle(state);
		m_KersSystem.Update();
	}
	else if (m_Parent->GetVehicleModelNameHash() == ATSTRINGHASH("Toreador", 0x56C8A5EF))
	{
		if (m_Parent->GetVehicle()->m_nVehicleFlags.bEngineOn)
		{
			m_KersSystem.Update();
		}
		else
		{
			m_KersSystem.StopAllSounds();
		}
	}

	bool hotwireActive = false;

	if(vehicle)
	{
		m_WasEngineOnLastFrame = isEngineOn;
		m_WasEngineStartingLastFrame = m_Parent->GetVehicle()->m_nVehicleFlags.bEngineStarting || m_Parent->ShouldForceEngineOff();

		if(IsHotwiringVehicle())
		{
			//@@: location AUDVEHICLEENGINE_UPDATE_SET_HOTWIRING_ACTIVE
			m_WasEngineStartingLastFrame = true;
			hotwireActive = true;
		}
	}

	if(hotwireActive && m_EngineState == AUD_ENGINE_OFF)
	{
		if(!m_HotwireSound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			initParams.UpdateEntity = true;
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_ENGINE;
			m_Parent->CreateAndPlaySound_Persistent(m_Parent->GetExtrasSoundSet()->Find(ATSTRINGHASH("VehicleHotwire", 0xE16CA9BF)), &m_HotwireSound, &initParams);
		}
	}
	else if(m_HotwireSound)
	{
		m_HotwireSound->StopAndForget();
	}

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		m_LastReplayUpdateTime = currentTime;
	}		
#endif
	
	m_EngineLastRevs = state->revs;
	m_NumUpdates++;
}

bool audVehicleEngine::IsHotwiringVehicle() const
{
	if(m_Parent && m_Parent->IsPlayerVehicle())
	{
		CPed* playerPed = FindPlayerPed();

		if(playerPed)
		{
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive())
			{
				return m_ReplayIsHotwiring;
			}
#endif
			const CTaskMotionInAutomobile* pTask = static_cast<const CTaskMotionInAutomobile*>(playerPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));

			if(pTask)
			{
				return pTask->IsHotwiringVehicle();
			}
		}
	}
	return false;
}

// -------------------------------------------------------------------------------
// Update the automatic engine shutoff system
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateAutoShutOffSystem(audVehicleVariables* state, bool engineOn)
{
	if(m_AutoShutOffSystemState == AUD_AUTOSHUTOFF_ACTIVE && m_Parent->GetAutoShutOffTimer() == 0.0f)
	{
		if(m_Parent->GetVehicle()->m_nVehicleFlags.bEngineOn)
		{
			// AI cars must actually start moving before the engine comes back on. Prevents cars that are
			// eg. blipping their throttle from constantly restarting the engine
			if(m_Parent->GetVehicle()->m_Transmission.GetThrottle() > 0.1f &&
			   (m_Parent->IsPlayerDrivingVehicle() || state->fwdSpeedAbs > 0.1f))
			{
				m_AutoShutOffSystemState = AUD_AUTOSHUTOFF_RESTARTING;
				m_EngineAutoShutOffRestartTime = fwTimer::GetTimeInMilliseconds();
				m_AutoShutOffRestartTimer = 0.0f;
			}
		}
		else
		{
			m_AutoShutOffSystemState = AUD_AUTOSHUTOFF_INACTIVE;
		}
	}
	else if(m_AutoShutOffSystemState == AUD_AUTOSHUTOFF_RESTARTING)
	{
		m_AutoShutOffRestartTimer += fwTimer::GetTimeStep();

		if(m_AutoShutOffRestartTimer > sm_AutoShutOffRestartDelay)
		{
			m_WasEngineOnLastFrame = false;
			m_WasEngineStartingLastFrame = true;
			CheckForEngineStartStop(state, engineOn);
			m_AutoShutOffSystemState = AUD_AUTOSHUTOFF_INACTIVE;
		}
	}
	else
	{
		if(m_Parent->GetAutoShutOffTimer() > sm_AutoShutOffCutoffTime)
		{
			if(m_AutoShutOffSystemState == AUD_AUTOSHUTOFF_INACTIVE)
			{
				if(m_EngineState == AUD_ENGINE_ON || m_EngineState == AUD_ENGINE_STARTING)
				{
					m_AutoShutOffSystemState = AUD_AUTOSHUTOFF_ACTIVE;
					m_EngineState = AUD_ENGINE_STOPPING;
					m_EngineStopTime = fwTimer::GetTimeInMilliseconds();
					m_Parent->StopAndForgetSounds(m_RevLimiterPopSound);
					StopEngine(true);
				}
			}
		}
		else
		{
			CheckForEngineStartStop(state, engineOn);
		}
	}
}

// -------------------------------------------------------------------------------
// Check if the engine has been started or stopped in the last frame
// -------------------------------------------------------------------------------
void audVehicleEngine::CheckForEngineStartStop(audVehicleVariables* state, bool engineOn)
{
	CVehicle* vehicle = m_Parent->GetVehicle();

	if(vehicle)
	{
		// If the vehicle's engine is on
		if(engineOn)
		{
			// If it wasn't on last frame then we need to start the audio engine
			if(!m_WasEngineOnLastFrame)
			{
				// Randomise our next misfire time each time the engine starts
				m_LastMisfireTime = state->timeInMs + (u32)audEngineUtil::GetRandomNumberInRange(7000,20000);

				if(m_EngineState == AUD_ENGINE_OFF )
				{
					// Don't fade in if the engine is starting, but do fade in if its not (ie if script started it immediately)
					bool fadeIn = !m_WasEngineStartingLastFrame;

					if(fadeIn)
					{
						m_EngineState = AUD_ENGINE_ON;
					}
					else
					{
						// Hybrid cars start in electric mode
						if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR)
						{
							CarAudioSettings* carSettings = ((audCarAudioEntity*)m_Parent)->GetCarAudioSettings();

							if(carSettings && (carSettings->EngineType == HYBRID || carSettings->EngineType == ELECTRIC))
							{
								m_EngineMode = ELECTRIC;
								m_ElectricToCombustionFade = 0.0f;
							}
						}

						bool isSubmarine = m_Parent->GetVehicle()->InheritsFromSubmarine();

						// Submarine startup has a bubble-y element to it, which we only want to play when submerged
						if(!m_StartupSound)
						{
							if(!isSubmarine || m_Parent->GetVehicle()->GetIsInWater())
							{
								audSoundInitParams initParams;
								initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
								initParams.TrackEntityPosition = true;

								if(IsGranularEngineActive())
								{
									initParams.Volume = m_GranularEngineAudioSettings->MasterVolume/100.f;
								}
								else if(m_VehicleEngineAudioSettings)
								{
									initParams.Volume = m_VehicleEngineAudioSettings->MasterVolume/100.f;	
								}

								m_Parent->CreateAndPlaySound_Persistent(m_Parent->GetEngineStartupSound(), &m_StartupSound, &initParams);
							}
						}
						
						m_EngineState = AUD_ENGINE_STARTING;
					}


					if(m_GranularEngineActive)
					{	
						if(fadeIn)
						{
							if(m_QualitySettingChanged)
							{
								m_granularEngine.SetFadeInRequired(400);
								m_granularExhaust.SetFadeInRequired(400);
							}
							else
							{
								u32 fadeTime = GetEngineSwitchFadeTime();
								m_granularEngine.SetFadeInRequired(fadeTime);
								m_granularExhaust.SetFadeInRequired(fadeTime);
							}
						}

						m_granularEngine.Start();
						m_granularExhaust.Start();
					}
					else if(m_VehicleEngineAudioSettings)
					{
						if(fadeIn)
						{
							if(m_QualitySettingChanged)
							{
								m_engine.SetFadeInRequired(400);
								m_exhaust.SetFadeInRequired(400);
							}
							else
							{
								u32 fadeTime = GetEngineSwitchFadeTime();
								m_engine.SetFadeInRequired(fadeTime);
								m_exhaust.SetFadeInRequired(fadeTime);
							}
						}

						m_engine.Start();
						m_exhaust.Start();
					}

					m_EngineOffTemperature = 100.0f;
					m_Parent->EngineStarted();
				}
				else if(m_EngineState == AUD_ENGINE_STOPPING)
				{
					// Engine got restarted before we actually called StopEngine, so just flick back to on
					m_EngineState = AUD_ENGINE_ON;
				}
			}
			else
			{
				if(m_EngineState == AUD_ENGINE_STARTING)
				{
					// If we're not granular, or we are and have started playback, allow the player to interrupt the 
					// starting behaviour if they start accelerating
					if(!IsGranularEngineActive() || GetGranularEngine()->AreAnySoundsPlaying())
					{
						if(vehicle->GetThrottle()>0.1f || state->fwdSpeed<-0.1f)
						{
							m_EngineState = AUD_ENGINE_ON;
						}
					}
				}
			}
		}
		// If the vehicle's engine is turned off
		else
		{
			if(m_WasEngineOnLastFrame)
			{
				m_EngineState = AUD_ENGINE_STOPPING;
				m_EngineStopTime = fwTimer::GetTimeInMilliseconds();
				m_Parent->StopAndForgetSounds(m_RevLimiterPopSound);
			}
			else if( m_EngineState == AUD_ENGINE_STOPPING )
			{ 
				if(!m_Parent->IsStartupSoundPlaying())
				{
					if(fwTimer::GetTimeInMilliseconds() - m_EngineStopTime > g_EngineStopDuration)
					{
						if(state->revs <= 0.2f)
						{
							StopEngine(true);
						}
					}
				}
			}
		}
	}
}

// -------------------------------------------------------------------------------
// Stop the engine
// -------------------------------------------------------------------------------
void audVehicleEngine::StopEngine(bool playStopSound)
{
	if(m_Parent)
	{
		CVehicle* vehicle = m_Parent->GetVehicle();

		if(m_EngineState == AUD_ENGINE_STOPPING && playStopSound)
		{
			m_engine.Stop(400);
			m_exhaust.Stop(400);
			m_granularEngine.Stop(400);
			m_granularExhaust.Stop(400);

			if(vehicle)
			{
				// Play a sound depending on the manner in which the vehicle stopped
				u32 engineShutdownHash = g_NullSoundHash;
				u32 vehicleShutdownHash = m_Parent->GetVehicleShutdownSound();

				const f32 engineHealth = m_Parent->ComputeEffectiveEngineHealth();
				f32 engineDamageFactor = 1.0f - (engineHealth / CTransmission::GetEngineHealthMax());

				if((vehicle->m_nVehicleFlags.bIsDrowning && m_Parent->GetAudioVehicleType() != AUD_VEHICLE_BOAT) || m_Parent->IsDamageModel() || engineDamageFactor >= 0.8f)
				{
					if(m_EngineMode != ELECTRIC)
					{
						engineShutdownHash = ATSTRINGHASH("ENGINE_BREAKDOWN", 0xF9192597);
					}
				}
				else
				{
					engineShutdownHash = m_Parent->GetEngineShutdownSound();
				}

				bool isSubmarine = m_Parent->GetVehicle()->InheritsFromSubmarine();

				// Submarine shutdown has a bubble-y element to it, which we only want to play when submerged
				if(!isSubmarine || m_Parent->GetVehicle()->GetIsInWater())
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();

					// Don't update the entity for submarines - we want the sound audible above and below water
					if(!isSubmarine)
					{
						initParams.UpdateEntity = true;
					}

					initParams.Tracker = vehicle->GetPlaceableTracker();

					if(IsGranularEngineActive())
					{
						initParams.Volume = m_GranularEngineAudioSettings->MasterVolume/100.f;
					}
					else if(m_VehicleEngineAudioSettings)
					{
						initParams.Volume = m_VehicleEngineAudioSettings->MasterVolume/100.f;	
					}

					if(!m_ShutdownSound && engineShutdownHash != g_NullSoundHash)
					{
						m_Parent->CreateAndPlaySound_Persistent(engineShutdownHash, &m_ShutdownSound, &initParams);
					}

					// Only play this if the vehicle has really, truly, properly shutdown (rather than breaking or going into energy saving mode)
					if(!vehicle->m_nVehicleFlags.bEngineOn && engineShutdownHash != ATSTRINGHASH("ENGINE_BREAKDOWN", 0xF9192597))
					{
						if(!m_VehicleShutdownSound && vehicleShutdownHash != g_NullSoundHash)
						{
							m_Parent->CreateAndPlaySound_Persistent(vehicleShutdownHash, &m_VehicleShutdownSound, &initParams);
						}
					}
				}
			}
		}
		else if(vehicle && vehicle->GetStatus() == STATUS_WRECKED)
		{
			// disable release if we're stopping engine sounds because the vehicle is wrecked
			m_engine.Stop(-1);
			m_exhaust.Stop(-1);

			m_granularEngine.Stop(-1);
			m_granularExhaust.Stop(-1);
		}
		else
		{
			// Streamed out/converting to dummy - fade out the engine. Fade time now reduced, as the dynamic activation range will mean that you shouldn't
			// hear the change in most normal situations. In cases where you do, there must be loads of vehicles in the immediate vicinity so the swap won't
			// be noticable anyway. Having shorter duration also aids with the vehicle bank swapping, as the waveslot will become will be free much sooner.
			u32 releaseTime = (u32)(g_EngineSwitchFadeOutTime * m_Parent->GetEngineSwitchFadeTimeScalar());
			
			if(audVehicleAudioEntity::IsPlayerVehicleStarving())
			{
				releaseTime = 200;
			}
			else if(m_Parent->IsForcedGameObjectResetRequired())
			{
				releaseTime = 400;
			}
				
			m_engine.Stop(releaseTime);
			m_exhaust.Stop(releaseTime);

			m_granularEngine.Stop(releaseTime);
			m_granularExhaust.Stop(releaseTime);
		}

		// If we're wrecked, stop the steam sound, otherwise let it fade out gradually as the engine cools
		if(vehicle && vehicle->GetStatus() == STATUS_WRECKED)
		{
			m_Parent->StopAndForgetSounds(m_EngineSteamSound);
		}

		m_Transmission.Stop();
		m_Turbo.Stop();
		m_ElectricEngine.Stop();
		m_Parent->SetBoostActive(false);

		// Toreador boost can remain on even if the audio engine is technically off
		if (m_Parent->GetVehicleModelNameHash() != ATSTRINGHASH("Toreador", 0x56C8A5EF) || !m_Parent->GetVehicle()->m_nVehicleFlags.bEngineOn)
		{
			m_KersSystem.StopAllSounds();
		}
		
		m_Parent->StopAndForgetSounds(m_ExhaustRattleLoop, m_RevsOffLoop, m_UnderLoadLoop, m_InductionLoop, m_BlownExhaustLow, m_BlownExhaustHigh, m_StartLoop, m_StartupSound, m_IgnitionSound, m_RevLimiterPopSound, m_FanbeltDamageSound, m_ExhaustDragSound);		
	}

	if(m_EngineState != AUD_ENGINE_OFF)
	{
		m_EngineOffTemperature = m_Parent->GetVehicle()->m_EngineTemperature;
	}

	m_QualitySettingChanged = false;

	if (m_EngineState == AUD_ENGINE_STOPPING)
	{
		m_Parent->FreeWaveSlot(false);
	}

	m_EngineState = AUD_ENGINE_OFF;
}

// -------------------------------------------------------------------------------
// Update induction
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateInduction(audVehicleVariables *state)
{
	if (!m_VehicleEngineAudioSettings)
		return;

	// Kill the induction sound when we enter the mod shop and are idling, otherwise it won't 
	// switch when we upgrade the engine.
	if((m_Parent->IsInCarModShop() && state->revs <= 0.2f) || m_Parent->ShouldForceEngineOff())
	{
		if(m_InductionLoop)
		{
			m_InductionLoop->StopAndForget();
		}

		return;
	}

	if(!m_InductionLoop)
	{
		audSoundInitParams initParams;
		initParams.UpdateEntity = true;
		initParams.u32ClientVar = (u32)AUD_VEHICLE_SOUND_ENGINE;
		initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
		u32 inductionLoopHash = m_VehicleEngineAudioSettings->InductionLoop;

		if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR)
		{
			if(((audCarAudioEntity*)m_Parent)->IsEngineUpgraded() && m_VehicleEngineAudioSettings->UpgradedInductionLoop != g_NullSoundHash)
			{
				inductionLoopHash = m_VehicleEngineAudioSettings->UpgradedInductionLoop;
			}
		}

		if(inductionLoopHash != g_NullSoundHash)
		{
			m_Parent->CreateAndPlaySound_Persistent(inductionLoopHash, &m_InductionLoop, &initParams);
		}
	}

	if(m_InductionLoop)
	{
		m_InductionLoop->FindAndSetVariableValue(ATSTRINGHASH("revs", 0x44D3B231), state->revs);
		m_InductionLoop->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), state->throttle);
	}
}

// -------------------------------------------------------------------------------
// Update exhaust rattle
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateExhaustRattle(audVehicleVariables *state)
{
	CVehicle* vehicle = m_Parent->GetVehicle();

	if(vehicle && m_VehicleEngineAudioSettings)
	{
		if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR)
		{
			if(((audCarAudioEntity*)m_Parent)->GetCarAudioSettings()->EngineType != ELECTRIC)
			{
				f32 revsOffVolLin = m_RevsOffLoopVolSmoother.GetLastValue();
				revsOffVolLin *= audDriverUtil::ComputeLinearVolumeFromDb(state->exhaustPostSubmixVol + g_EngineVolumeTrim);
				f32 bodyDamageFactor = m_Parent->ComputeEffectiveBodyDamageFactor();
				f32 revsFactor = (state->revs - 0.2f) * 1.25f;

				if(m_EngineState == AUD_ENGINE_STARTING)
				{
					revsOffVolLin = 1.0f;
				}
				else if(m_ShutdownSound)
				{
					revsOffVolLin = 1.0f;
					revsFactor = 0.25f;
				}

				f32 exhaustRattleVolLin = (revsFactor * 0.6f) * revsOffVolLin * sm_ExhaustRattleVol.CalculateValue(bodyDamageFactor);
				f32 exhaustRattleVolLinSmoothed = m_ExhaustRattleSmoother.CalculateValue(exhaustRattleVolLin, g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
				m_Parent->UpdateLoopWithLinearVolumeAndPitch(&m_ExhaustRattleLoop, ATSTRINGHASH("DAMAGE_EXHAUST_RATTLE", 0x367D8F), exhaustRattleVolLinSmoothed, 0, m_Parent->GetEnvironmentGroup(), NULL, true, false);

				if(m_HasDraggableExhaust BANK_ONLY( || g_ForceDamage))
				{
					f32 volumeLin = 0.0f;
					u32 timeSinceStationary = state->timeInMs - ((audCarAudioEntity*)m_Parent)->GetLastTimeStationary();
					const u32 maxExhaustDragTime = 2500;
					const u32 exhaustDragFadeTime = 1500;

					if(timeSinceStationary < maxExhaustDragTime)
					{
						if(state->numWheelsTouchingFactor == 1.0f && bodyDamageFactor > 0.5f)
						{
							volumeLin = 1.0f - Min(state->fwdSpeedAbs/30.0f, 1.0f);
							volumeLin *= Min(state->fwdSpeedAbs/3.0f, 1.0f);
							volumeLin *= Min(bodyDamageFactor/0.7f, 1.0f);

							if(timeSinceStationary > exhaustDragFadeTime)
							{
								volumeLin *= 1.0f - Min((timeSinceStationary - exhaustDragFadeTime)/(f32)(maxExhaustDragTime - exhaustDragFadeTime), 1.0f);
							}
						}
					}

					m_Parent->UpdateLoopWithLinearVolumeAndPitch(&m_ExhaustDragSound, ATSTRINGHASH("DAMAGE_EXHAUST_DRAG_PULL_AWAY", 0x4B0B6D81), volumeLin, state->roadNoisePitch, m_Parent->GetEnvironmentGroup(), NULL, true, true);
				}
				else if(m_ExhaustDragSound)
				{
					m_ExhaustDragSound->StopAndForget();
				}
			}
		}
	}
}

// -------------------------------------------------------------------------------
// Update engine load
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateEngineLoad(audVehicleVariables *state)
{
	CVehicle* vehicle = m_Parent->GetVehicle();

	if(vehicle)
	{
		f32 revRatio = (state->revs - 0.2f) * 1.25f;
		f32 desiredUnderLoadRatio = vehicle->m_Transmission.IsEngineUnderLoad() && !vehicle->GetHandBrake()? (1.0f - Min(((state->gear - 1) + revRatio) / 3.0f, 1.0f)) : 0.0f;

		if(m_Parent->HasHeavyRoadNoise())
		{
			desiredUnderLoadRatio = Max(desiredUnderLoadRatio, Min(m_Parent->GetVehicle()->GetTransform().GetB().GetZf()/0.15f, 0.25f) * Min(state->fwdSpeedRatio/0.3f, 1.0f));
		}

		f32 underLoadVolLin = m_UnderLoadRatioSmoother.CalculateValue(desiredUnderLoadRatio, state->timeInMs) * Min(state->fwdSpeedAbs/3.0f, 1.0f);

		if(!IsGranularEngineActive() && !m_UnderLoadLoop && underLoadVolLin > g_SilenceVolumeLin)
		{
			audSoundInitParams initParams;
			initParams.Category = g_EngineLoudCategory;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			initParams.UpdateEntity = true;
			Assign(initParams.EffectRoute, m_Parent->GetExhaustEffectRoute());
			m_Parent->CreateAndPlaySound_Persistent(m_Parent->GetExtrasSoundSet()->Find(ATSTRINGHASH("Vehicle_UnderLoadLoop", 0xA52BD7E7)), &m_UnderLoadLoop, &initParams);
		}

		if(m_UnderLoadLoop)
		{
			if(underLoadVolLin > g_SilenceVolumeLin)
			{
				s32 pitch = audDriverUtil::ConvertRatioToPitch(state->pitchRatio);
				m_UnderLoadLoop->SetClientVariable((u32)AUD_VEHICLE_SOUND_EXHAUST);
				m_UnderLoadLoop->SetRequestedPitch(pitch);
				m_UnderLoadLoop->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(underLoadVolLin));
				m_UnderLoadLoop->SetRequestedEnvironmentalLoudnessFloat(state->environmentalLoudness);
				m_UnderLoadLoop->SetRequestedFrequencySmoothingRate(m_Parent->GetFrequencySmoothingRate());
			}
			else
			{
				m_UnderLoadLoop->StopAndForget();
			}
		}
	}
}

// -------------------------------------------------------------------------------
// Update revs off
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateRevsOff(audVehicleVariables *state)
{
	CVehicle* vehicle = m_Parent->GetVehicle();

	if(vehicle && m_VehicleEngineAudioSettings)
	{
		m_RevsOffActive = false;

		if(m_EngineState == AUD_ENGINE_STARTING && state->revs > CTransmission::ms_fIdleRevs)
		{
			m_RevsOffActive = true;
		}
		else if(m_IsMisfiring || (state->revs < m_EngineLastRevs && state->throttle <= 0.1f && m_EngineState == AUD_ENGINE_ON && state->gear != 0 && state->timeInMs > m_Transmission.GetLastGearChangeTimeMs()+500))
		{
			m_RevsOffActive = true;
		}

		f32 revRatio = (state->revs - 0.2f) * 1.25f;
		f32 revsOffVolLin = m_RevsOffLoopVolSmoother.CalculateValue(m_RevsOffActive ? revRatio : 0.0f, state->timeInMs);

		if(!IsGranularEngineActive() && !m_RevsOffLoop && revsOffVolLin > g_SilenceVolumeLin)
		{
			audSoundInitParams initParams;
			initParams.Category = g_EngineLoudCategory;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			initParams.UpdateEntity = true;
			Assign(initParams.EffectRoute, m_Parent->GetExhaustEffectRoute());
			m_Parent->CreateAndPlaySound_Persistent(m_VehicleEngineAudioSettings->RevsOffLoop, &m_RevsOffLoop, &initParams);
		}

		if(m_RevsOffLoop)
		{
			if(revsOffVolLin > g_SilenceVolumeLin)
			{
				s32 pitch = audDriverUtil::ConvertRatioToPitch(state->pitchRatio);
				m_RevsOffLoop->SetClientVariable((u32)AUD_VEHICLE_SOUND_EXHAUST);
				m_RevsOffLoop->SetRequestedPitch(pitch);
				m_RevsOffLoop->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(revsOffVolLin));
				m_RevsOffLoop->SetRequestedEnvironmentalLoudnessFloat(state->environmentalLoudness);
				m_RevsOffLoop->SetRequestedFrequencySmoothingRate(m_Parent->GetFrequencySmoothingRate());
			}
			else
			{
				m_RevsOffLoop->StopAndForget();
			}
		}
	}
}

// -------------------------------------------------------------------------------
// Update hybrid engine
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateHybridEngine(audVehicleVariables * state)
{
	if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR)
	{
		CarAudioSettings* carSettings = ((audCarAudioEntity*)m_Parent)->GetCarAudioSettings();

		if(carSettings->EngineType == HYBRID)
		{
			if(m_EngineMode == ELECTRIC && state->fwdSpeedRatio > 0.4f && state->gear > 0)
			{
				m_EngineMode = COMBUSTION;
			}
			// Testing against m_Transmission.GetRevRatio() here as state->revs is faked based on the fwd speed for hybrid vehicles
			else if(m_EngineMode == COMBUSTION && state->throttle < 0.1f && m_Parent->GetVehicle()->m_Transmission.GetRevRatio() == CTransmission::ms_fIdleRevs && state->fwdSpeedRatio < 0.15f)
			{
				m_EngineMode = ELECTRIC;
			}

			if(m_EngineMode == COMBUSTION)
			{
				m_ElectricToCombustionFade += (fwTimer::GetTimeStep() * 2.0f);
			}
			else
			{
				m_ElectricToCombustionFade -= (fwTimer::GetTimeStep() * 2.0f);
			}

			m_ElectricToCombustionFade = Clamp(m_ElectricToCombustionFade, 0.0f, 1.0f);
			f32 combustionVolumeScale = m_ElectricToCombustionFade;
			f32 electricVolumeScale = 1.0f - m_ElectricToCombustionFade;
			
			m_granularEngine.SetVolumeScale(combustionVolumeScale);
			m_granularExhaust.SetVolumeScale(combustionVolumeScale);

			m_engine.SetVolumeScale(combustionVolumeScale);
			m_exhaust.SetVolumeScale(combustionVolumeScale);

			m_ElectricEngine.SetVolumeScale(electricVolumeScale);
		}
	}
}

// -------------------------------------------------------------------------------
// Update engine damage stuff
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateDamage(audVehicleVariables *state)
{
	CVehicle* vehicle = m_Parent->GetVehicle();

	if(vehicle && m_Parent->GetAudioVehicleType() != AUD_VEHICLE_BOAT)
	{
		// Update misfires - this should only occur on vehicles that are actually damaged (not 'fake' audio damaged)
		if(!(m_IsMisfiring && m_HasTriggeredBackfire) && 
			 state->gear > 0 && 
			 !vehicle->m_nVehicleFlags.bIsDrowning && 
			 state->clutchRatio > 0.1f && 
			 m_LastMisfireTime + 3000 < state->timeInMs && 
			 m_VehicleEngineAudioSettings &&
			 m_Parent->GetVehicle()->GetVehicleDamage()->GetEngineHealth() < 300.f)
			{
				if(state->gasPedal > 0.8f &&
				   state->fwdSpeedRatio > 0.1f && 
				   m_Parent->GetRevsSmoother()->GetLastValue() < 0.7f && 
				   m_Parent->GetRevsSmoother()->GetLastValue() > 0.5f)
				{
					m_LastMisfireTime = state->timeInMs;
					m_HasTriggeredBackfire = false;
					m_IsMisfiring = true;
				}
			}

		f32 radBurstDamage = vehicle->GetVehicleDamage()->GetEngineHealth();

#if __BANK
		if(g_ForceDamage && m_Parent->IsPlayerVehicle())
		{
			radBurstDamage = ((1.0f - g_ForcedDamageFactor) * 1000.0f);
		}
#endif

		// Update radiator damage
		if(m_EngineSteamSound)
		{
			if (radBurstDamage > ENGINE_DAMAGE_RADBURST)
			{
				m_EngineSteamSound->StopAndForget();
			}
		}
		else
		{
			// ignore always damaged flag for radburst SFX
			if (radBurstDamage < ENGINE_DAMAGE_RADBURST && vehicle->m_nVehicleFlags.bEngineOn)
			{
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();

				// If we're getting into a pre-damaged vehicle then ramp up the steam volume slowly so it doesn't just come in at full
				// volume as soon as the engine starts
				if(m_EngineState == AUD_ENGINE_STARTING)
				{
					initParams.AttackTime = 15000;
				}

				m_Parent->CreateSound_PersistentReference(ATSTRINGHASH("DAMAGE_RADIATOR_HISS", 0xD536089C), &m_EngineSteamSound, &initParams);
				if(m_EngineSteamSound)
				{
					m_EngineSteamSound->SetUpdateEntity(true);
					m_EngineSteamSound->SetClientVariable((u32)AUD_VEHICLE_SOUND_ENGINE);
					m_EngineSteamSound->PrepareAndPlay();
				}
			}
		}

		f32 desiredFanbeltVolLin = m_Parent->GetDrowningFactor() == 0.0f ? (0.05f + (0.45f * state->throttle)) + (0.5f * state->fwdSpeedRatio) : 0.0f;
		f32 fanbeltVolumeLin = m_FanBeltVolumeSmoother.CalculateValue(desiredFanbeltVolLin, state->timeInMs);
		f32 engineDamageFactor = state->engineDamageFactor;

		if(engineDamageFactor > 0.5f && fanbeltVolumeLin > g_SilenceVolumeLin && (m_HasBreakableFanbelt BANK_ONLY( || g_ForceDamage)))
		{
			if(!m_FanbeltDamageSound BANK_ONLY( &&!g_MuteFanbeltDamage))
			{
				const audMetadataRef fanbeltSoundRef = audCarAudioEntity::GetDamageSoundSet()->Find("Fanbelt_Damage");
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
				initParams.TrackEntityPosition = true;
				initParams.UpdateEntity = true;
				m_Parent->CreateAndPlaySound_Persistent(fanbeltSoundRef, &m_FanbeltDamageSound, &initParams);
			}

			if(m_FanbeltDamageSound)
			{
				f32 revRatio = (state->revs - 0.2f) * 1.25f;
				m_FanbeltDamageSound->FindAndSetVariableValue(ATSTRINGHASH("revs", 0x44D3B231), revRatio);
				m_FanbeltDamageSound->FindAndSetVariableValue(ATSTRINGHASH("damageFactor", 0xAD8C0EF4), engineDamageFactor);
				m_FanbeltDamageSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(fanbeltVolumeLin));

#if __BANK
				if(g_MuteFanbeltDamage)
				{
					m_FanbeltDamageSound->StopAndForget();
				}
#endif
			}
		}
		else if(m_FanbeltDamageSound)
		{
			m_FanbeltDamageSound->StopAndForget();
		}
	}
}

// -------------------------------------------------------------------------------
// audVehicleEngine UpdateExhaustPops
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateExhaustPops(audVehicleVariables* state)
{
	CVehicle* vehicle = m_Parent->GetVehicle();

	if(vehicle && m_VehicleEngineAudioSettings)
	{
		bool revLimiterPopsEnabled = false;

		CVehicle* vehicle = m_Parent->GetVehicle();

		if(vehicle->m_nFlags.bPossiblyTouchesWater)
		{
			f32 waterZ = 0.f;
			const Vector3 exhaustPos = VEC3V_TO_VECTOR3(vehicle->TransformIntoWorldSpace(m_Parent->GetExhaustOffsetPos()));			
			vehicle->m_Buoyancy.GetWaterLevelIncludingRivers(exhaustPos, &waterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL);

			// Exhaust is underwater, so don't do any pops
			if(waterZ > exhaustPos.z)
			{
				if(m_RevLimiterPopSound)
				{
					m_Parent->StopAndForgetSounds(m_RevLimiterPopSound);
				}

				return;
			}
		}
		
		if(vehicle->InheritsFromSubmarineCar())
		{
			// Don't let sub cars exhaust pop while transforming
			CSubmarineCar* pSubCar = static_cast<CSubmarineCar*>(vehicle);

			if(pSubCar->GetAnimationState() != CSubmarineCar::State_Finished || pSubCar->GetAnimationPhase() > 0.f)
			{
				if(m_RevLimiterPopSound)
				{
					m_Parent->StopAndForgetSounds(m_RevLimiterPopSound);
				}

				return;
			}
		}

		bool exhaustBroken = state->engineDamageFactor >= 0.7f;

		if(!exhaustBroken)
		{
			if(m_GranularEngineActive && m_GranularEngineAudioSettings)
			{
				if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR)
				{
					if(AUD_GET_TRISTATE_VALUE(m_GranularEngineAudioSettings->Flags, FLAG_ID_GRANULARENGINEAUDIOSETTINGS_HASREVLIMITER) == AUD_TRISTATE_TRUE || ((audCarAudioEntity*)m_Parent)->IsEngineUpgraded())
					{
						u32 revLimiterPopHash = m_GranularEngineAudioSettings->RevLimiterPopSound;

						if(((audCarAudioEntity*)m_Parent)->IsExhaustUpgraded() && m_GranularEngineAudioSettings->UpgradedRevLimiterPop != g_NullSoundHash)
						{
							revLimiterPopHash = m_GranularEngineAudioSettings->UpgradedRevLimiterPop;
						}

						if((state->onRevLimiter BANK_ONLY(|| g_GranularForceRevLimiter)) && revLimiterPopHash != g_NullSoundHash)
						{
							revLimiterPopsEnabled = true;

							if(!m_RevLimiterPopSound && !m_HasTriggeredLimiterPop)
							{
								audSoundInitParams initParams;
								initParams.UpdateEntity = true;
								initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
								initParams.u32ClientVar = AUD_VEHICLE_SOUND_EXHAUST;						
								m_Parent->CreateAndPlaySound_Persistent(revLimiterPopHash, &m_RevLimiterPopSound, &initParams);
								m_HasTriggeredLimiterPop = true;

								if(m_RevLimiterPopSound)
								{
									bool hasVisualPops = (AUD_GET_TRISTATE_VALUE(m_GranularEngineAudioSettings->Flags, FLAG_ID_GRANULARENGINEAUDIOSETTINGS_VISUALFXONLIMITER) == AUD_TRISTATE_TRUE);
									hasVisualPops |= ((audCarAudioEntity*)m_Parent)->GetExhaustUpgradeLevel() == 1.0f;

#if GTA_REPLAY
									if(!CReplayMgr::IsEditModeActive())
#endif
									{
										vehicle->m_nVehicleFlags.bAudioBackfired = hasVisualPops;
									}									
								}
							}
						}
					}
				}
			}
		}

		if(!revLimiterPopsEnabled)
		{
			m_HasTriggeredLimiterPop = false;
		}

		if(m_RevLimiterPopSound)
		{
			m_RevLimiterPopSound->SetRequestedVolume(state->exhaustConeAtten);
		}

		u32 exhaustPopHash = m_VehicleEngineAudioSettings->ExhaustPops;
		bool exhaustUpgraded = false;

		if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR)
		{
			if(((audCarAudioEntity*)m_Parent)->IsExhaustUpgraded())
			{
				exhaustUpgraded = true;

				if(m_VehicleEngineAudioSettings->UpgradedExhaustPops != g_NullSoundHash)
				{
					exhaustPopHash = m_VehicleEngineAudioSettings->UpgradedExhaustPops;
				}
			}
		}

		if(exhaustPopHash != g_NullSoundHash)
		{
			f32 exhaustSmootherValue = m_ExhaustPopSmoother.CalculateValue(state->revs * Max(state->fwdSpeedRatio * 1.2f, 1.0f), g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));

			if(state->throttleInput == 0.0f && 
				exhaustSmootherValue > 0.5f && 
				fwTimer::GetTimeInMilliseconds() - m_Parent->GetVehicleEngine()->GetTurbo()->GetLastDumpValveExhaustPopTime() > 2000 &&
				!vehicle->m_nVehicleFlags.bIsDrowning &&
				state->rawRevs < m_EngineLastRevs)
			{
				if(!m_ExhaustPopSound && m_ConsecutiveExhaustPops == 0)
				{
					audSoundInitParams initParams;
					initParams.UpdateEntity = true;
					initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
					initParams.u32ClientVar = AUD_VEHICLE_SOUND_EXHAUST;
					m_Parent->CreateAndPlaySound_Persistent(exhaustPopHash, &m_ExhaustPopSound, &initParams);
					m_ExhaustPopSmoother.Reset();
					m_ExhaustPopSmoother.CalculateValue(exhaustSmootherValue - 0.1f);

					if(m_ExhaustPopSound)
					{
#if GTA_REPLAY
						if(!CReplayMgr::IsEditModeActive())
#endif
						{
							vehicle->m_nVehicleFlags.bAudioBackfired = true;
							REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(exhaustPopHash, &initParams, m_ExhaustPopSound, m_Parent->GetVehicle()));
						}						
					}
					
					m_ConsecutiveExhaustPops++;
				}
			}
			else if(state->throttleInput > 0.1f)
			{
				m_ConsecutiveExhaustPops = 0;
			}
		}

		if(m_ExhaustPopSound)
		{
			if(vehicle->m_nVehicleFlags.bIsDrowning)
			{
				m_ExhaustPopSound->StopAndForget();
			}
			else
			{
				m_ExhaustPopSound->SetRequestedVolume(state->exhaustConeAtten);
			}
		}

		if(m_IsMisfiring && state->timeInMs - m_LastMisfireTime < 550 && state->timeInMs - m_LastMisfireTime > 400 && !m_HasTriggeredBackfire)
		{
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			initParams.Volume = state->exhaustConeAtten;
			initParams.Position = VEC3V_TO_VECTOR3(vehicle->TransformIntoWorldSpace(m_Parent->GetExhaustOffsetPos()));
			initParams.u32ClientVar = AUD_VEHICLE_SOUND_EXHAUST;
			if(m_Parent->CreateAndPlaySound(ATSTRINGHASH("EX_POPS_BACKFIRE", 0x38BEF758), &initParams))
			{
#if GTA_REPLAY
				if(!CReplayMgr::IsEditModeActive())
#endif
				{
					vehicle->m_nVehicleFlags.bAudioBackfiredBanger = true;
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ATSTRINGHASH("EX_POPS_BACKFIRE", 0x38BEF758), &initParams, m_Parent->GetVehicle()));
				}				
			}

			m_HasTriggeredBackfire = true;
		}
		else if(m_IsMisfiring && state->timeInMs - m_LastMisfireTime >= 550)
		{
			m_IsMisfiring = false;
			m_HasTriggeredBackfire = false;

			if(m_Parent->IsPlayerVehicle())
			{
				m_LastMisfireTime = state->timeInMs + (u32)audEngineUtil::GetRandomNumberInRange(7000,20000);
			}
			else
			{
				m_LastMisfireTime = state->timeInMs + (u32)audEngineUtil::GetRandomNumberInRange(15000,30000);
			}
		}
	}
}

// -------------------------------------------------------------------------------
// audVehicleEngine UpdateIgnition
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateIgnition(audVehicleVariables* state)
{
	CVehicle* vehicle = m_Parent->GetVehicle();
	bool performingAutoShutOffRestart = false;

	if(m_Parent->HasAutoShutOffEngine())
	{
		if(fwTimer::GetTimeInMilliseconds() - m_EngineAutoShutOffRestartTime < sm_AutoShutOffIgnitionTime)
		{
			performingAutoShutOffRestart = true;
		}
	}

	if(vehicle)
	{
		if((vehicle->m_nVehicleFlags.bEngineStarting || performingAutoShutOffRestart) && m_Parent->GetVehicle()->GetDriver() && !m_IgnitionSound)
		{
			if(!vehicle->m_nVehicleFlags.bIsDrowning)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
				s32 predelay = 0;

				if(!m_Parent->IsKickstarted())
				{
					if(!performingAutoShutOffRestart && vehicle->m_failedEngineStartAttempts>0)
					{
						// delay the retry
						predelay = audEngineUtil::GetRandomNumberInRange(300,650);
					}
				}

				initParams.Predelay = predelay;
				initParams.Pitch = (s16) m_IgnitionPitch;
				initParams.UpdateEntity = true;
				initParams.VolumeCurveScale = state->rollOffScale;

				m_Parent->CreateSound_PersistentReference(m_Parent->GetEngineIgnitionLoopSound(), &m_IgnitionSound, &initParams);
				if(m_IgnitionSound)
				{
					m_IgnitionSound->FindAndSetVariableValue(g_VariationVariableHash, m_IgnitionVariation);

					m_IgnitionSound->SetClientVariable((u32)AUD_VEHICLE_SOUND_ENGINE);
					m_IgnitionSound->PrepareAndPlay();
				}		
			}
		}
		else if(m_IgnitionSound && ((!performingAutoShutOffRestart && !vehicle->m_nVehicleFlags.bEngineStarting) || m_Parent->GetVehicle()->GetDriver() == NULL))
		{
			// Kickstarts are one shots, so let them play out fully
			if(!m_Parent->IsKickstarted())
			{
				m_IgnitionSound->StopAndForget();
			}
		}
	}
}

// -------------------------------------------------------------------------------
// audVehicleEngine UpdateStartLoop
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateStartLoop(audVehicleVariables* state)
{
	if(m_VehicleEngineAudioSettings)
	{
		f32 volLin=0.0f;

		if(m_VehicleEngineAudioSettings->StartLoop > 0)
		{
			volLin = state->throttle * sm_StartLoopVolCurve.CalculateValue(state->revs) * audDriverUtil::ComputeLinearVolumeFromDb(state->exhaustPostSubmixVol + g_EngineVolumeTrim);
		}

		volLin = m_StartLoopVolSmoother.CalculateValue(volLin,state->timeInMs);

		// dont want to ever stop this loop since pitch smoothing will be out of sync with the main engine
		m_Parent->UpdateLoopWithLinearVolumeAndPitchRatio(&m_StartLoop, m_VehicleEngineAudioSettings->StartLoop, volLin, state->pitchRatio, m_Parent->GetEnvironmentGroup(), g_EngineCategory, false, true);
	}
}

// -------------------------------------------------------------------------------
// audVehicleEngine SetGranularPitch
// -------------------------------------------------------------------------------
void audVehicleEngine::SetGranularPitch(s32 pitch)
{
	for(u32 loop = 0; loop < audGranularMix::GrainPlayerQualityMax; loop++)
	{
		audSound* engineSound = GetGranularEngine()->GetGranularSound((audGranularMix::audGrainPlayerQuality)loop);
		audSound* exhaustSound = GetGranularExhaust()->GetGranularSound((audGranularMix::audGrainPlayerQuality)loop);

		if(engineSound)
		{
			engineSound->SetRequestedPitch(pitch);
		}
		
		if(exhaustSound)
		{
			exhaustSound->SetRequestedPitch(pitch);
		}
	}
}

// -------------------------------------------------------------------------------
// audVehicleEngine CalculateDialRPMRatio
// -------------------------------------------------------------------------------
f32 audVehicleEngine::CalculateDialRPMRatio() const
{
	if(m_GranularEngineActive)
	{		
		return m_granularEngine.CalculateDialRPMRatio();
	}
	else
	{
		return m_EngineLastRevs;
	}
}

// -------------------------------------------------------------------------------
// audVehicleEngine UpdateEngineCooling
// -------------------------------------------------------------------------------
void audVehicleEngine::UpdateEngineCooling(audVehicleVariables* UNUSED_PARAM(state))
{
	CVehicle* vehicle = m_Parent->GetVehicle();

	if(vehicle && m_Parent->GetAudioVehicleType() != AUD_VEHICLE_BOAT)
	{
		static const f32 engineCooldownTemperatureRange = 20.0f;
		const f32 baseTemperature = g_weather.GetTemperature(m_Parent->GetVehicle()->GetTransform().GetPosition());
		const bool isRocketCoolingActive = vehicle->HasRocketBoost() && (m_KersSystem.GetKersState() == audVehicleKersSystem::AUD_KERS_STATE_BOOSTING || (fwTimer::GetTimeInMilliseconds() - m_KersSystem.GetLastKersBoostTime()) < g_RocketBoostCooldownTime);
		const bool isCoolingFanOn = vehicle->m_nVehicleFlags.bIsCoolingFanOn || isRocketCoolingActive;
		const bool shouldEngineCoolingStop = (vehicle->m_nVehicleFlags.bIsDrowning || m_Parent->IsDisabled() || m_Parent->IsInAmphibiousBoatMode() || (!isCoolingFanOn && (vehicle->m_nVehicleFlags.bEngineOn || vehicle->m_EngineTemperature <= m_EngineOffTemperature - engineCooldownTemperatureRange)));
		const bool shouldEngineCoolingStart = (!vehicle->m_nVehicleFlags.bIsDrowning && !m_Parent->IsDisabled() && (isCoolingFanOn || (m_EngineState == AUD_ENGINE_OFF && vehicle->m_EngineTemperature > baseTemperature + 50.f)));
		f32 engineCoolingProgress = Clamp((engineCooldownTemperatureRange - (m_EngineOffTemperature - vehicle->m_EngineTemperature))/engineCooldownTemperatureRange, 0.0f, 1.0f);

		if(isRocketCoolingActive)
		{
			const f32 rocketCoolingProgress = 1.f - Clamp((fwTimer::GetTimeInMilliseconds() - m_KersSystem.GetLastKersBoostTime()) / (f32)g_RocketBoostCooldownTime, 0.0f, 1.0f);
			engineCoolingProgress = Max(engineCoolingProgress, rocketCoolingProgress);
		}

		if(m_EngineSteamSound)
		{
			if(m_EngineState == AUD_ENGINE_OFF && engineCoolingProgress == 0.0f)
			{
				m_Parent->StopAndForgetSounds(m_EngineSteamSound);
			}
			else
			{
				f32 freqRatio = m_EngineSteamFluctuator.CalculateValue();

				// Computing this manually rather than using the audVehicleVariables value as the steam can persist once the vehicle becomes 
				// super dummy, at which point we stop calculating the vehicle variables
				const f32 engineHealth = m_Parent->ComputeEffectiveEngineHealth();
				f32 engineHealthFactor = Clamp(engineHealth / CTransmission::GetEngineHealthMax(), 0.0f, 1.0f);

#if __BANK
				if(g_ForceDamage && m_Parent->IsPlayerVehicle())
				{
					engineHealthFactor = (1.0f - g_ForcedDamageFactor);
				}
#endif

				f32 engineOffVol = m_EngineState == AUD_ENGINE_ON ? 0.0f : audDriverUtil::ComputeDbVolumeFromLinear(engineCoolingProgress);
				f32 drowningVol = audDriverUtil::ComputeDbVolumeFromLinear(1.0f - m_Parent->GetDrowningFactor());
				m_EngineSteamSound->SetRequestedVolume((engineHealthFactor * g_audEngineSteamSoundVolRange) + engineOffVol + drowningVol);
				m_EngineSteamSound->SetRequestedFrequencyRatio(freqRatio);
			}
		}

		if(m_EngineCoolingSound)
		{
			if(shouldEngineCoolingStop)
			{
				m_EngineCoolingSound->StopAndForget();
			}
			else
			{
				((audSound*)m_EngineCoolingSound)->FindAndSetVariableValue(g_TemperatureVariableHash, engineCoolingProgress);
			}
		}
		else
		{
			if(shouldEngineCoolingStart && !shouldEngineCoolingStop)
			{
				audSoundInitParams initParams;

				if(vehicle->m_nVehicleFlags.bIsCoolingFanOn)
				{
					initParams.AttackTime = 15500;
				}

				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
				m_Parent->CreateSound_PersistentReference(ATSTRINGHASH("HEAT_STRESS_HEAT_TICK_LOOP", 0x578B87D4), &m_EngineCoolingSound, &initParams);

				if(m_EngineCoolingSound)
				{
					m_EngineCoolingSound->SetUpdateEntity(true);
					m_EngineCoolingSound->SetClientVariable((u32)AUD_VEHICLE_SOUND_ENGINE);
					((audSound*)m_EngineCoolingSound)->FindAndSetVariableValue(g_TemperatureVariableHash, engineCoolingProgress);

					m_EngineCoolingSound->PrepareAndPlay();
				}
			}
		}

		if(isCoolingFanOn && !m_EngineCoolingFan && !vehicle->m_nVehicleFlags.bIsDrowning && !m_Parent->IsDisabled() && m_VehicleEngineAudioSettings)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup();
			initParams.RemoveHierarchy = false;
			m_Parent->CreateSound_PersistentReference(m_VehicleEngineAudioSettings->CoolingFan, &m_EngineCoolingFan, &initParams);
			if(m_EngineCoolingFan)
			{
				m_EngineCoolingFan->SetUpdateEntity(true);
				m_EngineCoolingFan->SetClientVariable((u32)AUD_VEHICLE_SOUND_ENGINE);
				m_EngineCoolingFan->PrepareAndPlay();
			}
		}
		else if((!isCoolingFanOn||vehicle->m_nVehicleFlags.bIsDrowning||m_Parent->IsDisabled()) && m_EngineCoolingFan)
		{
			m_EngineCoolingFan->StopAndForget();
		}
	}
}

// -------------------------------------------------------------------------------
// Convert to a dummy vehicle
// -------------------------------------------------------------------------------
void audVehicleEngine::ConvertToDummy()
{
	StopEngine(false);
	m_KersSystem.StopAllSounds();
}

// -------------------------------------------------------------------------------
// Convert to a disabled vehicle
// -------------------------------------------------------------------------------
void audVehicleEngine::ConvertToDisabled()
{
	m_Parent->StopAndForgetSounds(m_EngineCoolingFan, m_EngineSteamSound, m_EngineCoolingSound);
}

// -------------------------------------------------------------------------------
// Called when the engine quality setting changes
// -------------------------------------------------------------------------------
void audVehicleEngine::QualitySettingChanged()
{
	bool wasGranularEngineActive = m_GranularEngineActive;
	m_QualitySettingChanged = true;
	m_GranularEngineActive = false;

	audGranularMix::audGrainPlayerQuality engineQuality = GetDesiredEngineQuality();

	if(g_GranularEnabled && m_GranularEngineAudioSettings)
	{
		if((engineQuality == audGranularMix::GrainPlayerQualityHigh && m_GranularEngineAudioSettings->EngineAccel != g_NullSoundHash) ||
			(engineQuality == audGranularMix::GrainPlayerQualityLow && m_GranularEngineAudioSettings->NPCEngineAccel != g_NullSoundHash && g_GranularEnableNPCGranularEngines))
		{
			m_granularEngine.Init(m_Parent, audGranularEngineComponent::ENGINE_SOUND_TYPE_ENGINE, engineQuality);
			m_granularExhaust.Init(m_Parent, audGranularEngineComponent::ENGINE_SOUND_TYPE_EXHAUST, engineQuality);
			m_GranularEngineActive = true;
			m_GranularEngineQuality = engineQuality;
		}
	}

	bool freeWaveSlot = true;

	// Switched from non-granular to granular? Just fade out and start over. Shouldn't ever happen in the final game.
	if(wasGranularEngineActive && !m_GranularEngineActive)
	{
		m_engine.Stop(400);
		m_exhaust.Stop(400);
		m_granularEngine.Stop(400);
		m_granularExhaust.Stop(400);
		m_EngineState = AUD_ENGINE_OFF;
	}
	// Switched from granular high quality to granular low quality - trigger the crossfade
	else if(wasGranularEngineActive && m_GranularEngineActive)
	{
		m_granularEngine.QualitySettingChanged();
		m_granularExhaust.QualitySettingChanged();
	}
	// Non-granular to non-granular. No change - just leave everything as-is
	else if(!wasGranularEngineActive && !m_GranularEngineActive)
	{
		m_QualitySettingChanged = false;
		freeWaveSlot = false;
	}

	if(freeWaveSlot)
	{
		m_Parent->ReapplyDSPEffects();
		m_Parent->FreeWaveSlot(false);
	}

	// Jet whine uses player bank sounds, so need to stop it to switch
	m_KersSystem.StopJetWhineSound();
}

// -------------------------------------------------------------------------------
// Get the engine switch fade time
// -------------------------------------------------------------------------------
u32 audVehicleEngine::GetEngineSwitchFadeTime() const
{
	if(m_Parent->GetAudioVehicleType() == AUD_VEHICLE_CAR && ((audCarAudioEntity*)m_Parent)->IsSportsCarRevsSoundPlayingOrAboutToPlay())
	{
		return g_SportsCarRevsEngineSwitchTime;
	}
	else if(camInterface::IsFadingIn() || camInterface::IsFadedOut())
	{
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive() && camInterface::IsFadedOut())
		{
			return 0;
		}
		else
#endif
		{
			return g_ScreenFadedEngineSwitchTime;
		}		
	}
	else
	{
		return (u32)(g_EngineSwitchFadeInTime * m_Parent->GetEngineSwitchFadeTimeScalar());
	}
}

// -------------------------------------------------------------------------------
// Convert from a dummy vehicle
// -------------------------------------------------------------------------------
void audVehicleEngine::ConvertFromDummy()
{
	CVehicle* vehicle = m_Parent->GetVehicle();

	m_ExhaustRattleSmoother.Reset();
	m_ExhaustPopSmoother.Reset();
	m_FanBeltVolumeSmoother.Reset();

	m_ExhaustRattleSmoother.CalculateValue(0.0f);
	m_ExhaustPopSmoother.CalculateValue(0.0f);
	m_FanBeltVolumeSmoother.CalculateValue(0.0f);

	if(vehicle)
	{
		// If the vehicle's engine is on and we're out of sync
		if(vehicle->m_nVehicleFlags.bEngineOn && !m_WasEngineStartingLastFrame && m_EngineState == AUD_ENGINE_OFF && !m_Parent->ShouldForceEngineOff())
		{
			m_EngineState = AUD_ENGINE_ON;

			if(m_GranularEngineActive)
			{
				if(m_QualitySettingChanged)
				{
					u32 fadeTime = 400;					
					REPLAY_ONLY(fadeTime = camInterface::IsFadedOut()? 0 : fadeTime;)
					m_granularEngine.SetFadeInRequired(fadeTime);
					m_granularExhaust.SetFadeInRequired(fadeTime);
				}
				else
				{
					u32 fadeTime = GetEngineSwitchFadeTime();
					m_granularEngine.SetFadeInRequired(fadeTime);
					m_granularExhaust.SetFadeInRequired(fadeTime);
				}
				
				m_granularEngine.Start();
				m_granularExhaust.Start();
			}
			else if(m_VehicleEngineAudioSettings)
			{
				if(m_QualitySettingChanged)
				{
					u32 fadeTime = 400;
					REPLAY_ONLY(fadeTime = camInterface::IsFadedOut()? 0 : fadeTime;)
					m_engine.SetFadeInRequired(fadeTime);
					m_exhaust.SetFadeInRequired(fadeTime);
				}
				else
				{
					u32 fadeTime = GetEngineSwitchFadeTime();
					m_engine.SetFadeInRequired(fadeTime);
					m_exhaust.SetFadeInRequired(fadeTime);
				}

				m_engine.Start();
				m_exhaust.Start();
			}
		}
	}
}
