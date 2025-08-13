// 
// audio/environment/microphones.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_MICROPHONES_H
#define AUD_MICROPHONES_H

#include "atl/array.h"
#include "audio/gameobjects.h"
#include "audioengine/environment.h"
#include "audioengine/curve.h"
#include "audioengine/curverepository.h"
#include "audioengine/widgets.h"
#include "bank/bank.h"
#include "Peds/ped.h"
class camBaseCamera;

#define MAX_CLOSE_MIC_PEDS 3

enum eReplayMicType
{
	replayMicDefault,
	replayMicAtCamera,
	replayMicAtTarget,
	replayMicCinematic,
	NUM_REPLAY_MICS
};

class naMicrophones
{
public:
	naMicrophones();
	~naMicrophones();

	void Init();
	void SetUpMicrophones();
	void UpdatePlayerHeadMatrix();

	void CloseMicPed(u8 micId, CPed* ped);
	void FixAmbienceOrientation(bool enable);
	void FixScriptMicToCurrentPosition();
	void ReleaseScriptControlledMicrophone();
	void RemoveCloseMicPed(u8 micId);
	void RemoveScriptMic();
	void SetScriptMicLookAt(const Vector3& position);
	void SetScriptMicPosition(const Vector3& position);
	void SetScriptMicPosition(const Vector3& panningPos,const Vector3& vol1Pos,const Vector3& vol2Pos);
	void SetScriptMicSettings(const u32 micSettings) {m_ScriptMicSettings = micSettings;};
	void PlayScriptedConversationFrontend(bool enable);

	bool IsTinyRacersMicrophoneActive() { return m_ScriptMicSettings == ATSTRINGHASH("SR_TR_MIC", 0x65921E57); }

	Matrix34 &GetAmbienceMic() 
	{
		return m_AmbienceOrientationMatrix;
	}
	const Vector3& GetLocalEnvironmentScanPosition() { return m_LocalEnvironmentScanPosition; }

	MicrophoneSettings* GetMicrophoneSettings() {return m_CachedMicrophoneSettings;}


	f32 GetSniperAmbienceVolume(Vec3V_In soundPos = Vec3V(V_ZERO),u32 listenerId = 0);

	void FreezeMicrophone() { m_MicFrozenThisFrame = true;};
	void UnFreezeMicrophone() { m_MicFrozenThisFrame = false;};

	bool IsMicFrozen() {return m_MicFrozenThisFrame;};
	bool IsFirstPerson();
	bool InPlayersHead();
	bool IsVehBonnetMic();
	bool IsFollowMicType();
	bool IsCinematicMicActive();
	bool EnableScriptControlledMicrophone();
	bool IsAmbienceOrientationFixed(){return m_FixedAmbienceOrientation;};
	// PURPOSE:
	// if Entity, returns true if the player is aiming the sniper and that entity is visible.
	// if !Entity, use for the ambience sounds, return true if the soundPosition is in the listenerId cone (using dot products) while aiming the sniper.
	bool IsVisibleBySniper(CEntity *entity = NULL);
	bool IsVisibleBySniper(const spdSphere &boundSphere);

	static void SetReplayMicType(u8 micType) { sm_ReplayMicType = micType;};

	static bool IsSniping() {return sm_SniperMicActive;}
	static bool IsHeliMicActive() {return sm_HeliMicActive;}

	static bool IsPlayerFrontend(){return sm_PlayerFrontend;}

#if __BANK
	void MicDebugInfo();
	void DrawDebug() const;

	const char* GetMicName(u8 micType);
	
	static void AddWidgets(bkBank &bank);
#endif

private:
#if GTA_REPLAY
	bool SetUpReplayMics(const MicrophoneSettings* settings);
#endif // GTA_REPLAY
	bool SetUpMicrophones(const MicrophoneType micType,const MicrophoneSettings *micSettings,f32 minFov = 14.f,f32 maxFov = 45.f, f32 maxFrontCosAngle = 45.f, f32 maxRearCosAngle = 135.f, f32 maxAtt = -3.f);
	bool SetUpFirstListener(const MicrophoneType micType,const MicrophoneSettings *micSettings,f32 minFrontCosAngle, f32 minRearCosAngle,f32 minAtt,f32 minFov,f32 maxFov, f32 maxFrontCosAngle, f32 maxRearCosAngle, f32 maxAtt);
	bool SetUpSecondaryListener(const MicrophoneType micType,const MicrophoneSettings *micSettingss, f32 firstListenerContribution,f32 minFrontCosAngle, f32 minRearCosAngle,f32 minAtt,f32 minFov,f32 maxFov, f32 maxFrontCosAngle, f32 maxRearCosAngle, f32 maxAtt );
	void InterpolateMics(const camBaseCamera* sourceCamera, const camBaseCamera* destCamera, Matrix34 &listenerMatrix,f32 blendLevel);
	
	f32 ComputeEquivalentRollOff(u32 listenerId, f32 distance);

	MicrophoneSettings* CalculateMicrophoneSettings(const camBaseCamera *camera = NULL);

#if __BANK
	 static void WarpMicrophone();
#endif 

	Matrix34 									m_ScriptControlledMic;
	Matrix34 									m_AmbienceOrientationMatrix;
	Matrix34 									m_PlayerHeadMatrixCached;

	Vector3 									m_LocalEnvironmentScanPosition;
	Vector3 									m_LocalEnvironmentScanPositionLastFrame;
	Vector3 									m_ScriptVolMic1Position;
	Vector3 									m_ScriptVolMic2Position;
	Vector3 									m_ScriptPanningPosition;
	
	RegdPed m_CloseMicPed[MAX_CLOSE_MIC_PEDS];
	atRangeArray<f32,g_maxListeners> 			m_LastMicDistance;
	atRangeArray<f32,g_maxListeners> 			m_CurrentCinematicRollOff;
	atRangeArray<f32,g_maxListeners>			m_CurrentCinematicApply;
	atRangeArray<MicrophoneType,g_maxListeners>	m_LastMicType;

	camBaseCamera *								m_LastCamera;

	MicrophoneSettings* 						m_DefaultMicSettings; 
	MicrophoneSettings* 						m_CachedMicrophoneSettings; 

	audCurve 									m_CamFovToRollOff;
	audCurve 									m_SniperZoomToRatio;
	audCurve 									m_CinematicCamDistToMicDistance;
	audCurve 									m_CinematicCamFovToFrontConeAngle;
	audCurve 									m_CinematicCamFovToRearConeAngle;
	audCurve 									m_CinematicCamFovToMicAttenuation;

	audSmoother									m_CinFrontCosSmoother;
	audSmoother									m_CinRearCosSmoother;
	audSmoother									m_CinRearAttSmoother;

	f32											m_BlendLevel;

	u32											m_ScriptMicSettings;

	bool										m_MicFrozenThisFrame;
	bool 										m_ScriptControllingMics;
	bool 										m_ScriptControlledMainMicActive;
	bool 										m_ScriptedDialogueFrontend;
	bool 										m_FixedAmbienceOrientation;
	bool 										m_PlayingCutsceneLastFrame;
	bool 										m_InterpolatingIdleCam;

	static f32 									sm_DeltaCApply;
	static f32 									sm_SniperAmbienceVolIncrease;
	static f32 									sm_SniperAmbienceVolDecrease;

	static u8									sm_ReplayMicType;

	static bool									sm_PlayerFrontend;
	static bool									sm_FixLocalEnvironmentPosition;
	static bool									sm_SniperMicActive;
	static bool									sm_HeliMicActive;

#if __BANK
	bool m_WasMicFrozenLastFrame; 
	Vector3 m_SecondMicPos;
	atRangeArray<Matrix34,g_maxListeners> m_PanningMatrix,m_VolMatrix,m_Camera;
	static f32 sm_PaningToVolInterp;
	static Vector3 sm_MicrophoneDebugPos;
	static bool sm_EqualizeCinRollOffFirstListener;
	static bool sm_EqualizeCinRollOffSecondListener;
	static bool sm_CenterVehMic;
	static bool sm_FixSecondMicDistance;
	static bool sm_UnifyCinematicVolMatrix;
	static bool sm_ShowListenersInfo;
	static bool sm_EnableDebugMicPosition;
#endif
	static bool sm_ZoomOnFirstListener;

};

#endif //AUD_MICROPHONES_H


