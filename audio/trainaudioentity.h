//  
// audio/trainaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_TRAINAUDIOENTITY_H
#define AUD_TRAINAUDIOENTITY_H

#include "vehicleaudioentity.h"

struct audScannerSentence;

// ----------------------------------------------------------------
// audTrainAudioEntity class
// ----------------------------------------------------------------
class audTrainAudioEntity : public audVehicleAudioEntity
{
// Public methods
public:
	audTrainAudioEntity();
	virtual ~audTrainAudioEntity();

	AUDIO_ENTITY_NAME(audTrainAudioEntity);

	virtual void Reset();
	virtual void UpdateVehicleSpecific(audVehicleVariables& vehicleVariables, audVehicleDSPSettings& variables);
	virtual bool InitVehicleSpecific(); 
	virtual void TriggerDoorOpenSound(eHierarchyId doorIndex);
	virtual void TriggerDoorCloseSound(eHierarchyId doorIndex, const bool isBroken);
	virtual VehicleCollisionSettings* GetVehicleCollisionSettings() const;
	virtual void PostUpdate();

	f32 GetTrainSpeed() { return m_TrainSpeed; }

	TrainAudioSettings* GetTrainAudioSettings();

	void AnnounceTrainAtStation();
	void AnnounceTrainLeavingStation();
	void AnnounceNextStation(s32 stationId);
	void TrainHasMovedTrackNode();
	void SetTrainRolloff(f32 rolloff);

	inline s32 GetAnnouncedNextStation() const {return m_AnnouncedNextStation;}
	inline s32 GetAnnouncedCurrentStation() const {return m_AnnouncedCurrentStation;}

	static void InitClass();
	static void UpdateActivationRanges();

#if __BANK
	static void AddWidgets(bkBank &bank);
#endif

	s32 GetShockwaveID() const				{ return m_ShockwaveId; }
	void SetShockwaveID(s32 shockwave) 		{ m_ShockwaveId = shockwave; }

// Private methods
private:
	void StartTrainSound(u32 soundID, audSound** sound, audSoundInitParams* initParams, u32 releaseTime);
	void InitTrainSettings();
	void UpdateTrainEngineSounds(f32 rolloffScalar);
	void StopTrainEngineSounds(u32 releaseTime);
	void UpdateTrainAmbientSounds(f32 rolloffScalar);
	void StopTrainAmbientSounds(u32 releaseTime);
	void UpdateCableCarSounds();
	CTrain* FindClosestLinkedCarriage(Vector3::Param position);

	virtual u32 GetVehicleHornSoundHash(bool ignoreMods = false);
	virtual u32 GetNormalPattern() const	 { return m_VehicleHornHash; }
	virtual u32 GetAggressivePattern() const { return GetNormalPattern(); }
	virtual u32 GetHeldDownPattern() const	 { return ATSTRINGHASH("SUBWAY_TRAIN_HELD_DOWN", 0xE50BFF12); }

	static bool ShouldPlayStationAnnouncements();
	static void UpdateAmbientTrainStations();
	static void InitScannerSentenceForTrain(audScannerSentence *);

#if __BANK
	void UpdateDebug();
#endif

// Private attributes
private:
	audSimpleSmoother m_AngleSmoother;
	audSimpleSmoother m_TrainDistanceFactorSmoother;
	audSimpleSmootherDiscrete m_BrakeVolumeSmoother;
	Vec3V m_ClosestCarriageToListenerPos;
	Vec3V m_LinkedTrainMatrixCol0;
	Vec3V m_LinkedTrainMatrixCol1;
	Vector3 m_ClosestPointOnTrainToListener;
	TrainAudioSettings* m_TrainAudioSettings; 
	u32 m_TrainBrakeReleaseNextTriggerTime;
	u32 m_TrainLastWheelTriggerTime;

	fwGeom::fwLine m_TrackRumblePositioningLine;
	u32 m_TrainAirHornNextTriggerTime;
	f32 m_TimeElapsedTrainTurn;
	u32 m_VehicleHornHash;

	audSound *m_TrainCarriage, *m_TrainDriveTone, *m_TrainDriveToneSynth, *m_TrainGrind, *m_TrainSqueal;
	audSound *m_TrainIdle, *m_TrainRumble, *m_TrainBrakes, *m_ApproachingTrainRumble;
	audSound *m_TrainAirHorn;

	audVehicleLOD m_DesiredLODLastFrame;
	f32 m_TrainSpeed;
	f32 m_TrainAcceleration;
	f32 m_TrainDistance;
	u32 m_LastTrainAnnouncement;
	u32 m_TrainStartOffset;
	u32 m_FramesAtDesiredLOD;
	s32 m_LastStationAnnounced;
	s32 m_AnnouncedNextStation;
	s32 m_AnnouncedCurrentStation;
	u32 m_NumTrackNodesSearched;
	s32 m_ShockwaveId;
	u8 m_TrackRumbleUpdateFrame;
	bool m_RumblePositioningLineValid : 1;
	bool m_TrainEngine : 1;
	bool m_HasPlayedBigBrakeRelease : 1;
	bool m_LinkedBackwards : 1;
	bool m_LinkedTrainValid : 1;

	audCurve m_CarriagePitchCurve, m_CarriageVolumeCurve, m_DriveTonePitchCurve, m_DriveToneVolumeCurve;
	audCurve m_DriveToneSynthPitchCurve, m_DriveToneSynthVolumeCurve, m_GrindPitchCurve, m_GrindVolumeCurve;
	audCurve m_TrainIdlePitchCurve, m_TrainIdleVolumeCurve, m_SquealPitchCurve, m_SquealVolumeCurve;
	audCurve m_WheelVolumeCurve, m_RumbleVolumeCurve, m_WheelDelayCurve, m_ScrapeSpeedVolumeCurve;
	audCurve m_BrakeVelocityPitchCurve, m_BrakeVelocityVolumeCurve, m_BrakeAccelerationPitchCurve, m_BrakeAccelerationVolumeCurve;
	audCurve m_ApproachingRumbleDistanceToIntensity;
	audCurve m_TrainDistanceToRollOff;

	static audCurve sm_TrainDistanceToAttackTime;
	static u32 sm_NumTrainsInActivationRange;
	static f32 sm_TrainActivationRangeScale;
	static u8 sm_TrackUpdateRumbleFrameIndex;
};

#endif // AUD_TRAINAUDIOENTITY_H





















