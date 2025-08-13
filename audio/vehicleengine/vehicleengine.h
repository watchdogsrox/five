// 
// audio/vehicleengineaudio.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLE_ENGINE_H
#define AUD_VEHICLE_ENGINE_H

#include "audio/gameobjects.h"
#include "audioengine/curve.h"
#include "audioengine/widgets.h"

#include "turbo.h"
#include "transmission.h"
#include "electricengine.h"
#include "vehicleenginecomponent.h"
#include "granularenginecomponent.h"
#include "audio/vehicleaudioentity.h"

namespace rage
{
	class audSound;
}

class audBoatAudioEntity;

class audVehicleKersSystem
{
// Public types
public:
	enum audKersState
	{
		AUD_KERS_STATE_BOOSTING,
		AUD_KERS_STATE_RECHARGING,
		AUD_KERS_STATE_OFF,
	};

// Public functions
public:
	explicit audVehicleKersSystem();
	virtual ~audVehicleKersSystem() {};

	void Init(audVehicleAudioEntity* parent);
	void Update();
	void StopAllSounds();	
	void StopJetWhineSound();

	void SetKERSState(audKersState state);
	void SetKersRechargeRate(f32 rechargeRate);
	void TriggerKersBoostActivate();
	void TriggerKersBoostActivateFail();
	void UpdateJetWhine(audVehicleVariables *state);
	
	u32 GetKersSoundSetName() const;

	inline audKersState GetKersState() const	{ return m_KersState; }
	inline u32 GetLastKersBoostTime() const		{ return m_LastKersBoostTime; }

// Private functions
private:	
	f32 GetKersBoostRemaining() const;

// Private variables
private:
	audVehicleAudioEntity*	m_Parent;
	audSound*				m_JetWhineSound;
	audSound*				m_KERSBoostLoop;
	audSound*				m_KERSRechargeLoop;
	audKersState			m_KersState;
	f32						m_KersRechargeRate;
	u32						m_LastKersBoostTime;
	u32						m_LastKersBoostFailTime;
};

// ----------------------------------------------------------
// Class for modelling engine sounds
// ----------------------------------------------------------
class audVehicleEngine
{
// Public types
public:
	enum audVehicleEngineState
	{
		AUD_ENGINE_STARTING,
		AUD_ENGINE_STOPPING,
		AUD_ENGINE_ON,
		AUD_ENGINE_OFF,
	};

	enum audVehicleAutoShutoffSystemState
	{
		AUD_AUTOSHUTOFF_INACTIVE,
		AUD_AUTOSHUTOFF_ACTIVE,
		AUD_AUTOSHUTOFF_RESTARTING,
	};

// Public functions
public:
	explicit audVehicleEngine();
	virtual ~audVehicleEngine() {};

	static void InitClass();

	void Init(audCarAudioEntity* parent);
	void Init(audBoatAudioEntity* parent);
	void Update(audVehicleVariables *state);
	void UpdateEngineCooling(audVehicleVariables* state);
	void SetGranularPitch(s32 pitch);
	f32 CalculateDialRPMRatio() const;
	
	void ForceEngineState(audVehicleEngineState state) { m_EngineState = state; }
	void ConvertToDisabled();
	void ConvertToDummy();
	void ConvertFromDummy();
	void QualitySettingChanged();
	void SetHasBreakableFanbelt(bool hasBreakableFanbelt)	{m_HasBreakableFanbelt = hasBreakableFanbelt;}
	void SetForceGranularLowQuality(bool forceLowQuality) { m_ForceGranularLowQuality = forceLowQuality; }
	bool IsForcedGranularLowQuality() const { return m_ForceGranularLowQuality; }

	audGranularMix::audGrainPlayerQuality GetDesiredEngineQuality() const;

	inline audVehicleEngineState GetState() const	{return m_EngineState;}
	inline u32 GetAutoShutOffRestartTime() const	{return m_EngineAutoShutOffRestartTime;}
	inline f32 GetEngineLastRevs() const			{return m_EngineLastRevs;}
	inline f32 GetUnderLoadRatio() const			{return m_UnderLoadRatioSmoother.GetLastValue();}
	inline bool IsRevsOffActive() const				{return m_RevsOffActive;}
	inline bool IsMisfiring() const					{return m_IsMisfiring;}
	inline bool WasEngineOnLastFrame() const		{return m_WasEngineOnLastFrame;}
	inline bool IsExhaustPopActive() const			{return m_ExhaustPopSound != NULL;}	
	inline bool IsGranularEngineActive() const		{return m_GranularEngineActive;}
	inline bool IsLowQualityGranular() const		{return m_GranularEngineQuality == audGranularMix::GrainPlayerQualityLow;}
	inline bool HasBeenInitialised() const			{return m_HasBeenInitialised;}
	inline bool IsAutoShutOffSystemEngaged() const	{return m_AutoShutOffSystemState != AUD_AUTOSHUTOFF_INACTIVE;}
	inline audSound* GetStartupSound() const		{return m_StartupSound;} 
	inline CarEngineTypes GetEngineMode() const		{return m_EngineMode;}
	inline audSound** GetExhaustPopSoundPtr()		{return &m_ExhaustPopSound; }
	inline audVehicleTurbo* GetTurbo()				{return &m_Turbo;}
	inline audVehicleKersSystem* GetKersSystem()	{return &m_KersSystem;}
	inline audGranularMix::audGrainPlayerQuality GetGranularEngineQuality() const { return m_GranularEngineQuality; }

	audVehicleElectricEngine const* GetElectricEngine() const		{return &m_ElectricEngine;}
	audVehicleTransmission const* GetTransmission() const			{return &m_Transmission;}
	audGranularEngineComponent const* GetGranularEngine() const		{return &m_granularEngine;}
	audGranularEngineComponent const* GetGranularExhaust() const	{return &m_granularExhaust;}
	
	bool IsHotwiringVehicle() const;
#if GTA_REPLAY
	void SetReplayIsHotwiring(bool val) { m_ReplayIsHotwiring = val; }
#endif
// Private functions
private: 
	void InitCommon();
	void UpdateAutoShutOffSystem(audVehicleVariables* state, bool engineOn);
	void CheckForEngineStartStop(audVehicleVariables* state, bool engineOn);
	void StopEngine(bool playStopSound);

	void UpdateExhaustPops(audVehicleVariables* state);
	void UpdateIgnition(audVehicleVariables* state);
	void UpdateStartLoop(audVehicleVariables *state);
	void UpdateRevsOff(audVehicleVariables *state);
	void UpdateEngineLoad(audVehicleVariables *state);
	void UpdateExhaustRattle(audVehicleVariables *state);
	void UpdateInduction(audVehicleVariables *state);
	void UpdateDamage(audVehicleVariables *state);
	void UpdateHybridEngine(audVehicleVariables *state);

	u32 GetEngineSwitchFadeTime() const;
	
// Private attributes:
private:
	static audCurve sm_RevsOffVolCurve;
	static audCurve sm_ExhaustRattleVol;
	static audCurve sm_StartLoopVolCurve;

	static f32 sm_AutoShutOffCutoffTime;
	static u32 sm_AutoShutOffIgnitionTime;
	static f32 sm_AutoShutOffRestartDelay;

	audVehicleAudioEntity *						m_Parent;
	VehicleEngineAudioSettings *				m_VehicleEngineAudioSettings;
	GranularEngineAudioSettings *				m_GranularEngineAudioSettings;
	audVehicleEngineState						m_EngineState;
	audVehicleAutoShutoffSystemState			m_AutoShutOffSystemState;
	audGranularMix::audGrainPlayerQuality		m_GranularEngineQuality;
	audVehicleKersSystem						m_KersSystem;

	u32						m_EngineAutoShutOffRestartTime;
	u32						m_EngineStopTime;
	f32						m_EngineLastRevs;
	f32						m_IgnitionVariation;
	f32						m_IgnitionPitch;
	f32						m_EngineOffTemperature;
	f32						m_AutoShutOffRestartTimer;
	u32						m_NumUpdates;
	u32						m_LastMisfireTime;
	f32						m_ElectricToCombustionFade;
	u32						m_ConsecutiveExhaustPops;
	REPLAY_ONLY(u32			m_LastReplayUpdateTime;)

	audSimpleSmootherDiscrete	m_RevsOffLoopVolSmoother;
	audSimpleSmootherDiscrete	m_UnderLoadRatioSmoother;
	audSimpleSmootherDiscrete	m_StartLoopVolSmoother;
	audSimpleSmoother			m_ExhaustRattleSmoother;
	audSimpleSmootherDiscrete	m_FanBeltVolumeSmoother;
	audSimpleSmootherDiscrete	m_ExhaustPopSmoother;
	audFluctuatorProbBased		m_EngineSteamFluctuator;
	
	audSound*				m_InductionLoop;
	audSound*				m_IgnitionSound;
	audSound*				m_StartupSound;
	audSound*				m_RevsOffLoop;
	audSound*				m_UnderLoadLoop;
	audSound*				m_ExhaustRattleLoop;
	audSound*				m_BlownExhaustLow;
	audSound*				m_BlownExhaustHigh;
	audSound*				m_StartLoop;
	audSound*				m_ShutdownSound;
	audSound*				m_VehicleShutdownSound;
	audSound*				m_EngineSteamSound;
	audSound*				m_EngineCoolingSound;
	audSound*				m_EngineCoolingFan;
	audSound*				m_ExhaustPopSound;
	audSound*				m_RevLimiterPopSound;
	audSound*				m_HotwireSound;
	audSound*				m_FanbeltDamageSound;
	audSound*				m_ExhaustDragSound;

	audVehicleTransmission		m_Transmission;
	audVehicleTurbo				m_Turbo;
	audVehicleElectricEngine	m_ElectricEngine;
	audVehicleEngineComponent	m_engine;
	audVehicleEngineComponent	m_exhaust;
	audGranularEngineComponent	m_granularEngine;
	audGranularEngineComponent	m_granularExhaust;
	CarEngineTypes				m_EngineMode;

	bool						m_RevsOffActive : 1;
	bool						m_GranularEngineActive : 1;
	bool						m_HasBeenInitialised : 1;
	bool						m_QualitySettingChanged : 1;
	bool						m_WasEngineOnLastFrame : 1;
	bool						m_WasEngineStartingLastFrame : 1;
	bool						m_IsMisfiring : 1;
	bool						m_ForceGranularLowQuality : 1;
	bool						m_HasTriggeredBackfire : 1;
	bool						m_HasBreakableFanbelt : 1;
	bool						m_HasDraggableExhaust : 1;
	bool						m_HasTriggeredLimiterPop : 1;
#if GTA_REPLAY
	bool						m_ReplayIsHotwiring : 1;
#endif
};

#endif // AUD_VEHICLE_ENGINE_H

