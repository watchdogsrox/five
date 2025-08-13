// 
// audio/policescanner.h
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#ifndef POLICESCANNER_H_INCLUDED
#define POLICESCANNER_H_INCLUDED

#include "audiodefines.h"

#if NA_POLICESCANNER_ENABLED

#include "scannermanager.h"

#include "gameobjects.h"
#include "game/crime.h"

#include "audioentity.h"

#include "atl/array.h"
#include "audioengine/soundset.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audiohardware/waveslot.h"
#include "bank/bkmgr.h"
#include "renderer/color.h"

class CPed;
class CEntity;
class audVehicleAudioEntity;
class naEnvironmentGroup;

#define AUD_NUM_SCANNER_VOICES (3)

class CVehicle;

class audPoliceScanner
{
	friend class audScannerManager;
public:
	audPoliceScanner();
	virtual ~audPoliceScanner();

	void Init();
	void Shutdown();

	void ReportCrime(const audCrimeReport &params, const u32 voiceNum, bool isPlayerCrime = false);
	void ReportPlayerCrime(const Vector3 &pos, const eCrimeType crime, s32 delay = -1, bool ignoreResisitArrestCheck = false);

	void ReportPoliceSpottingPlayer(const Vector3 &pos, const s32 oldWantedLevel, const bool isCarKnown, bool forceSpotting = false,
		bool forceVehicle = false, bool forceAssitance = false, bool forceCritical = false);
	void ReportPoliceSpottingNonPlayer(const CPed* ped, f32 accuracy, bool force = false, bool forceVehicle = false);

	void ReportPoliceSpottingPlayerVehicle(CVehicle *veh);
	void ReportPoliceSpotting(const audPoliceSpotting &params);
	void ReportPoliceSpotting(const audPoliceSpotting &params, f32 accuracy);
	void ReportPoliceRefocus();
	void ReportPoliceRefocusCritical();
	void RequestAssistanceCritical();
	void ReportSuspectDown();
	void ReportSuspectArrested();

	void ReportRandomCrime(const Vector3& pos);

	void ReportDispatch(const u32 numUnits, const audUnitType unitType, const Vector3 &pos, bool forceAmbientPrio=false);

	void RequestAssistance(const Vector3 &pos,const audScannerPriority prio, bool fromScript = false);

	void ReportRandomDispatch(const audCrimeReport &params);

	void TriggerRandomChat();

	void TriggerVigilanteCrime(const u32 crimeId, const Vector3 &position);

	void TriggerRandomMedicalReport(const Vector3 &pos, naEnvironmentGroup *occlusionGroup);

	void SetNextAmbientMs(u32 uTime) {m_NextAmbientMs = uTime;}

#if __BANK
	void DrawDebug();
#endif

private:
	void GenerateDispatchOrders();
	void GenerateAmbientScanner(const u32 timeInMs);

	void ChooseInstructionAndCrimeSound(ScannerCrimeReport *metadata, u32 &instruction, u32 &crime);
	u32 ChooseWeightedVariation(audWeightedVariation *items, u32 numItems);
	bool FindSpecificLocation(const Vector3& pos, u32& locationDirectionSound, u32& locationSound, f32&probOfPlaying, const u32 voiceNum);
	void DescribePosition(const Vector3 &pos, audPositionDescription &desc, const u32 voiceNum, bool isAmbient=false);
	void DescribePositionForScriptedLine(const Vector3 &pos, audPositionDescription &desc, const u32 voiceNum);
	void GetVechicleColorSoundHashes(u32 voiceNum, u8 prefix, u8 colour, audVehicleDescription &desc);
	void DescribeVehicle(CVehicle *vehicle, audVehicleDescription &desc, const u32 voiceNum);

	u32 m_NextAmbientMs;

	ScannerSpecificLocationList* m_specificLocationListHash;

	atRangeArray<audSoundSet, AUD_NUM_SCANNER_VOICES> m_StreetAndAreaSoundMappings;
	atRangeArray<audSoundSet, AUD_NUM_SCANNER_VOICES> m_StreetAndAreaConjunctiveSoundMappings;
	atRangeArray<audSoundSet, AUD_NUM_SCANNER_VOICES> m_OverAreaSoundMappings;

	u32 m_LastTimeReportedSuicide;
	u32 m_LastTimeReportedSuspectDown;
	u32 m_LastTimeReportedSpottingPlayer;
};

extern audScannerManager g_AudioScannerManager;
extern audPoliceScanner g_PoliceScanner;
#endif // NA_POLICESCANNER_ENABLED
#endif // POLICESCANNER_H_INCLUDED

