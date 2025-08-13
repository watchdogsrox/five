// 
// audio/policescanner.cpp
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 


#include "audiodefines.h"

#if NA_POLICESCANNER_ENABLED

#include "northaudioengine.h"
#include "boataudioentity.h"
#include "gameobjects.h"
#include "policescanner.h"
#include "radioaudioentity.h"
#include "caraudioentity.h"	
#include "scriptaudioentity.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audioengine/soundfactory.h"
#include "audiohardware/waveslot.h"

#include "camera/CamInterface.h"
#include "grcore/debugdraw.h"
#include "game/crime.h"
#include "game/modelindices.h"
#include "game/user.h"
#include "text/text.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "peds/PedFactory.h"
#include "Peds/PedGeometryAnalyser.h"
#include "peds/popzones.h"
#include "peds/PopCycle.h"
#include "scene/entity.h"
#include "streaming/streamingengine.h"
#include "fwsys/timer.h"
#include "vehicles/vehicle.h"
#include "vehicles/boat.h"
#include "vehicles/train.h"
#include "vector/vector3.h"
#include "script/script.h"
#include "script/script_hud.h"

#include "text/TextConversion.h"

#include "debugaudio.h"

AUDIO_OPTIMISATIONS()

extern bool g_ScannerEnabled;
extern bool g_ShowAreaDebug;
extern bool g_PrintScannerTriggers;
extern u32 g_MinTimeBetweenReports;
extern u32 g_MinTimeBetweenCriticalReports;
extern u32 g_ScannerCrimeReportTime;
extern f32 g_ReportCrimeProbability;
extern f32 g_ReportSpottingProbability;
extern f32 g_UnsureOfNewAreaProb;
extern f32 g_ErrAfterInProb;
extern f32 g_ReportSuspectHeadingProb;
extern f32 g_ReportCarProb;
extern f32 g_ReportCarPrefixProb;
extern f32 g_DistFactorToConsiderCentral;
extern f32 g_AreaSizeTooSmallForDirection;
extern f32 g_MinPlayerMovementThreshold;
extern u32 g_DelayBeforeErr;
extern u32 g_DelayAfterErr;
extern u32 g_DelayAfterStreet;
extern u32 g_MinGapAfterScriptedReport;
extern f32 g_MinCopDistSqForCopInstSet;
extern f32 g_MinPedDistSqForPedInstSet;
extern f32 g_SquelchProb;
extern u32 g_ScannerPreemptMs;
extern const char* g_VoiceNumToStringMap[AUD_NUM_SCANNER_VOICES];
extern audCategory *g_PoliceScannerCategory;
extern audCategory *g_PoliceScannerDeathScreenCategory;
extern CopDispatchInteractionSettings* g_CopDispatchInteractionSettings;
// heading
extern atRangeArray<u32 ,4> g_AudioPoliceScannerDir;

f32 g_SpeedInfoMinAccuracy = 0.5f;
f32 g_HeadingInfoMinAccuracy = 0.5f;
f32 g_VehicleInfoMinAccuracy = 0.5f;
f32 g_CarPrefixInfoMinAccuracy = 0.5f;
f32 g_CarCategoryInfoMinAccuracy = 0.5f;
f32 g_CarModelInfoMinAccuracy = 0.5f;
f32 g_MinPctForLocationAt = 0.15f;
f32 g_MinPctForLocationNear =0.6f;
f32 g_MinHeightToSayOver = 60.0f;

u32 g_SquelchPostDelay = 0;
u32 g_SquelchSoundHash = ATSTRINGHASH("POLICE_RADIO_SQUELCH_MASTER", 0x8838F411);

audMetadataRef g_ScannerNoConjunctiveSoundRef;
audMetadataRef g_ScannerNoDirectionSoundRef;
audMetadataRef g_ScannerNoConjNoDirSoundRef;
audMetadataRef g_ScannerNullSoundRef;

#if __BANK
extern bool g_PrintCarColour;
extern bool g_DebuggingScanner;
extern char g_TestStreetName[128];
extern char g_TestAreaName[128];
extern bool g_DebugCopDispatchInteraction;
extern bool g_DrawScannerSpecificLocations;
#endif

audPoliceScanner g_PoliceScanner;

audPoliceScanner::audPoliceScanner()
{
}

audPoliceScanner::~audPoliceScanner()
{

}

void audPoliceScanner::Init()
{
	m_specificLocationListHash = audNorthAudioEngine::GetObject<ScannerSpecificLocationList>(ATSTRINGHASH("SCANNER_SPECIFIC_LOCATIONS_LIST", 0x0ee60fff2));
	for(int i=0; i< AUD_NUM_SCANNER_VOICES; ++i)
	{
		char buf[64];
		formatf(buf, sizeof(buf), "STREET_AND_AREA_NAME_MAPPING%s", g_VoiceNumToStringMap[i]);
		m_StreetAndAreaSoundMappings[i].Init(atStringHash(buf));
		formatf(buf, sizeof(buf), "STREET_AND_AREA_NAME_CONJUNCTIVE_MAPPING%s", g_VoiceNumToStringMap[i]);
		m_StreetAndAreaConjunctiveSoundMappings[i].Init(atStringHash(buf));
		formatf(buf, sizeof(buf), "OVER_AREA_MAPPING%s", g_VoiceNumToStringMap[i]);
		m_OverAreaSoundMappings[i].Init(atStringHash(buf));
		if(!m_StreetAndAreaSoundMappings[i].IsInitialised() || !m_StreetAndAreaConjunctiveSoundMappings[i].IsInitialised() || 
			!m_OverAreaSoundMappings[i].IsInitialised())
			naWarningf("Failed to initialize police scanner street and area mapping for voice number %u", i);
	}

	g_ScannerNoConjunctiveSoundRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(ATSTRINGHASH("POLICE_SCANNER_NO_CONJUCTIVE", 0x36841CCC));
	g_ScannerNoDirectionSoundRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(ATSTRINGHASH("POLICE_SCANNER_NO_DIRECTION", 0x639D68E0));
	g_ScannerNoConjNoDirSoundRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(ATSTRINGHASH("POLICE_SCANNER_NO_CONJUCTIVE_NO_DIRECTION", 0x78D81B47));
	g_ScannerNullSoundRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(ATSTRINGHASH("NULL_SOUND", 0xE38FCF16));
	

	m_LastTimeReportedSuicide = 0;
	m_LastTimeReportedSuspectDown = 0;
	m_LastTimeReportedSpottingPlayer = 0;
}

void audPoliceScanner::Shutdown()
{

}

void audPoliceScanner::GenerateAmbientScanner(const u32 timeInMs)
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
		return;

	if(timeInMs > m_NextAmbientMs)
	{
		if(FindPlayerPed() && FindPlayerPed()->GetPlayerWanted() && FindPlayerPed()->GetPlayerWanted()->GetWantedLevel() != WANTED_CLEAN)
		{
			m_NextAmbientMs = timeInMs + audEngineUtil::GetRandomNumberInRange(7500, 15000);
			return;
		}

		s32 i = audEngineUtil::GetRandomNumberInRange(-2,3);
		// if the player has a wanted level then only trigger ambient chat, otherwise it would be somewhat confusing
		if(FindPlayerPed()->GetPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL1)
		{
			i = 3;
		}

		Vector3 pos = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
		// pick a random position away from the player
		f32 xOffset = audEngineUtil::GetRandomNumberInRange(0.f, 1000.f);
		f32 yOffset = audEngineUtil::GetRandomNumberInRange(0.f, 1000.f);
		pos.x = pos.x + (Sign(audEngineUtil::GetRandomNumberInRange(-1.f,1.f)) * xOffset);
		pos.y = pos.y + (Sign(audEngineUtil::GetRandomNumberInRange(-1.f,1.f)) * yOffset);

		switch(i)
		{
		case -2:
		case -1:
		case 0:
			g_PoliceScanner.ReportRandomCrime(pos);
			break;
		case 1:
			{
				s32 numUnits = audEngineUtil::GetRandomNumberInRange(0,7);
				audUnitType type = static_cast<audUnitType>(audEngineUtil::GetRandomNumberInRange(0,2));
				ReportDispatch(numUnits, type, pos, true);
			}
			break;
		case 2:
			{
				RequestAssistance(pos, AUD_SCANNER_PRIO_AMBIENT);
			}
			break;
		case 3:
			TriggerRandomChat();
			break;
		default:
			break;
		}

		m_NextAmbientMs = timeInMs + audEngineUtil::GetRandomNumberInRange(7500, 15000);
	}
}

void audPoliceScanner::ReportSuspectArrested()
{
	if(g_ScannerEnabled && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
	{
		audScannerSentence* sentence = g_AudioScannerManager.AllocateSentence();
		//Shouldn't be required, but I just hate not having it here.
		if(!sentence)
			return;

#if __BANK
		formatf(sentence->debugName, "Suspect Arrested");
#endif

		sentence->sentenceType = AUD_SCANNER_SUSPECT_DOWN;
		sentence->timeRequested = g_AudioEngine.GetTimeInMilliseconds() + audEngineUtil::GetRandomNumberInRange(1000,3000);
		sentence->priority = AUD_SCANNER_PRIO_CRITICAL;
		
		u32 idx = 0;
		u32 thisIsControl = g_AudioScannerManager.GetScannerHashForVoice("THIS_IS_CONTROL", sentence->voiceNum);
		u32 suspectArrested = g_AudioScannerManager.GetScannerHashForVoice("SUSPECT_IN_CUSTODY", sentence->voiceNum);

		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] THIS_IS_CONTROL");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(thisIsControl);

		sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(100, 1000);
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] SUSPECT_IN_CUSTODY");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(suspectArrested);

		sentence->numPhrases = idx;
		sentence->inUse = true;
	}
}

void audPoliceScanner::ReportSuspectDown()
{
	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if(g_ScannerEnabled && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner) &&
		currentTime - m_LastTimeReportedSuicide > 5000 && currentTime - m_LastTimeReportedSuspectDown > 15000 )
	{
		audScannerSentence* sentence = g_AudioScannerManager.AllocateSentence();
		//Shouldn't be required, but I just hate not having it here.
		if(!sentence)
			return;

		m_LastTimeReportedSuspectDown = currentTime;
#if __BANK
		formatf(sentence->debugName, "Suspect Down");
#endif
		
		sentence->sentenceType = AUD_SCANNER_SUSPECT_DOWN;
		sentence->timeRequested = g_AudioEngine.GetTimeInMilliseconds() + audEngineUtil::GetRandomNumberInRange(1000,3000);
		sentence->priority = AUD_SCANNER_PRIO_CRITICAL;
		
		u32 idx = 0;
		u32 thisIsControl = g_AudioScannerManager.GetScannerHashForVoice("THIS_IS_CONTROL", sentence->voiceNum);
		u32 suspectArrested = g_AudioScannerManager.GetScannerHashForVoice("SUSPECT_DOWN", sentence->voiceNum);

		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] THIS_IS_CONTROL");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(thisIsControl);

		sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(100, 1000);
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] SUSPECT_DOWN");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(suspectArrested);

		sentence->numPhrases = idx;
		sentence->inUse = true;
	}
}

void audPoliceScanner::ReportPlayerCrime(const Vector3 &pos, const eCrimeType crime, s32 delay, bool ignoreResisitArrestCheck)
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
		return;

	// HACK: don't report any crimes if Ross' script is screwing with the on screen wanted level
	if(FindPlayerPed()->GetPlayerWanted()->m_fMultiplier < 0.9f || (FindPlayerPed()->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN && CScriptHud::iFakeWantedLevel > 0))
	{
		return;
	}

	u32 now = g_AudioEngine.GetTimeInMilliseconds();
	//We want to delay the call to RESIST_ARREST, as it was happening a ton and blocking out other crimes.  If we don't get another
	//	crime report within a little bit of time, we report RESIST_ARREST for real
	if(crime == CRIME_RESIST_ARREST && !ignoreResisitArrestCheck)
	{
		g_AudioScannerManager.ReportResistArrest(pos, delay);
		return;
	}
	else
	{
		g_AudioScannerManager.ResetResistArrest();
	}

	if(g_AudioScannerManager.HasCrimeBeenReportedRecently(crime, now))
		return;

	audCrimeReport params;
	params.crimeId = static_cast<u32>(crime);
	params.delay = delay >= 0 ? delay : audEngineUtil::GetRandomNumberInRange(3000,6000);	
	if(params.crimeId < MAX_CRIMES)
	{	
		u32 voiceNum = g_AudioScannerManager.SelectScannerVoice();
		const u32 crimeNameHash = g_AudioScannerManager.GetScannerHashForVoice(g_CrimeNamePHashes[params.crimeId], voiceNum);
		ScannerCrimeReport *metadata = audNorthAudioEngine::GetObject<ScannerCrimeReport>(crimeNameHash);
		if(metadata)
		{
			params.priority = AUD_SCANNER_PRIO_HIGH;
			DescribePosition(pos, params.posDescription,voiceNum);
			params.crime = metadata;
			params.subtype = crime;
			ReportCrime(params, voiceNum, true);
		}
	}
}

void audPoliceScanner::ReportCrime(const audCrimeReport &params, const u32 voiceNum, bool isPlayerCrime)
{
	if(g_ScannerEnabled)
	{
		bool isSuicide = params.crimeId == CRIME_SUICIDE;

		u32 idx = 0;
		u32 instructionSound;
		u32 crimeSound;
					
		ChooseInstructionAndCrimeSound(params.crime, instructionSound, crimeSound);

		//Don't play suicide unless there is a cop around to see it
		if(isSuicide)
		{
			if(instructionSound == g_NullSoundHash || instructionSound != params.crime->PoliceAroundInstructionSndRef)
				return;

			m_LastTimeReportedSuicide = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		}

		// populate sentence structure 
		//		second argument doesn't matter, since the first arg is "false"
		audScannerSentence* sentence = g_AudioScannerManager.AllocateSentence(false,AUD_SCANNER_PRIO_AMBIENT,voiceNum);
		//Shouldn't be required, but I just hate not having it here.
		if(!sentence)
			return;

		sentence->isPlayerCrime = isPlayerCrime;

#if __BANK
		formatf(sentence->debugName, "Report Crime %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(params.crime->NameTableOffset));
#endif

		sentence->sentenceType = isSuicide ? AUD_SCANNER_SUSPECT_DOWN : AUD_SCANNER_CRIME_REPORT;
		sentence->category = isSuicide && g_PoliceScannerDeathScreenCategory ? g_PoliceScannerDeathScreenCategory : g_PoliceScannerCategory;
		sentence->sentenceSubType = params.subtype;
		sentence->timeRequested = g_AudioEngine.GetTimeInMilliseconds() + params.delay;
		sentence->priority = params.priority;
		sentence->shouldDuckRadio = params.duckRadio;
		sentence->crimeId = (eCrimeType)params.crimeId;

		
		// INSTRUCTION
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] instructionSound");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(instructionSound);


		if(audEngineUtil::ResolveProbability(g_SquelchProb))
		{
			// SQUELCH
			sentence->phrases[idx].slot = SCANNER_SLOT_NULL; //resident sound
			sentence->phrases[idx].postDelay = g_SquelchPostDelay;
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] squelch");
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_SquelchSoundHash);
		}
		
		// CRIME
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] crimeSound");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(crimeSound);

		g_AudioScannerManager.PopulateSentenceWithPosDescription(params.posDescription, sentence, idx);

		sentence->numPhrases = idx;
		
		sentence->playAcknowledge = (params.crime && audEngineUtil::ResolveProbability(params.crime->AcknowledgeSituationProbability)) BANK_ONLY(|| g_DebugCopDispatchInteraction);

		sentence->inUse = true;
	}
}

void audPoliceScanner::ReportRandomDispatch(const audCrimeReport &params)
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
		return;

	if(g_ScannerEnabled)
	{
		u32 idx = 0;
		u32 instructionSound;
		u32 crimeSound;

		ChooseInstructionAndCrimeSound(params.crime, instructionSound, crimeSound);

		// populate sentence structure
		audScannerSentence* sentence = g_AudioScannerManager.AllocateSentence();
		//Shouldn't be required, but I just hate not having it here.
		if(!sentence)
			return;
		
#if __BANK
		formatf(sentence->debugName, "Report Crime %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(params.crime->NameTableOffset));
#endif

		sentence->sentenceType = AUD_SCANNER_CRIME_REPORT;
		sentence->sentenceSubType = params.crimeId;
		sentence->timeRequested = g_AudioEngine.GetTimeInMilliseconds() + audEngineUtil::GetRandomNumberInRange(2000,4000);
		sentence->priority = params.priority;

		// POLICE CAR NAME
		const u32 voiceNum = sentence->voiceNum;
		const u32 policeCarName = g_AudioScannerManager.GetScannerHashForVoice("POLICE_CAR_NAME", voiceNum);
		const u32 policeCarNumber = g_AudioScannerManager.GetScannerHashForVoice("POLICE_CAR_NUMBER", voiceNum);
		const u32 dispatchTo = g_AudioScannerManager.GetScannerHashForVoice("INSTRUCTIONS_DISPATCH_TO", voiceNum);
		const u32 forSound = g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_CONJUNCTIVES_FOR", voiceNum);
		//const u32 randomA = g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_A", voiceNum);
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_CAR_NAME");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(policeCarName);
		// POLICE CAR NUMBER
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_CAR_NUMBER");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(policeCarNumber);
		// DISPATCH TO
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_4;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] INSTRUCTIONS_DISPATCH_TO");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(dispatchTo);
		//No more "errs" needed as they are now baked into assets.  I'm still going to keep this code around and commented out, in case this
		//	changes
//		if(audEngineUtil::ResolveProbability(g_ErrAfterInProb))
//		{
//			sentence->phrases[idx-1].postDelay = g_DelayBeforeErr;
//			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(); //SCANNER_SMALL_SLOT_3;
//			sentence->phrases[idx].postDelay = g_DelayAfterErr;
//			sentence->phrases[idx].sound = NULL;
//#if __BANK
//			if(g_PrintScannerTriggers)
//				formatf(sentence->phrases[idx].debugName, 64, "POLICE_SCANNER_ERR");
//#endif
//			sentence->phrases[idx++].soundHash = g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_ERR", voiceNum);
//		}
		
		if(audEngineUtil::ResolveProbability(g_SquelchProb))
		{
			// SQUELCH
			sentence->phrases[idx].slot = SCANNER_SLOT_NULL; //resident sound
			sentence->phrases[idx].postDelay = g_SquelchPostDelay;
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] squelch");
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_SquelchSoundHash);
		}

		// AREA
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] areaSound");
#endif
		sentence->phrases[idx++].soundMetaRef = params.posDescription.areaSound;
		// FOR
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_2;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_CONJUNCTIVES_FOR");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(forSound);
		// CRIME
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] crimeSound");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(crimeSound);

		sentence->numPhrases = idx;

		sentence->inUse = true;
	}
}

void audPoliceScanner::RequestAssistance(const Vector3 &pos, const audScannerPriority prio, bool fromScript)
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner) && !fromScript)
		return;

	if(g_ScannerEnabled)
	{
		u32 idx = 0;
		// populate sentence structure
		audScannerSentence* sentence = g_AudioScannerManager.AllocateSentence();
		//Shouldn't be required, but I just hate not having it here.
		if(!sentence)
			return;

#if __BANK
		formatf(sentence->debugName, "Request Assistance");
#endif

		audPositionDescription posDescription;
		DescribePosition(pos, posDescription, sentence->voiceNum, prio == AUD_SCANNER_PRIO_AMBIENT);
		posDescription.streetSound.SetInvalid();
		
		sentence->sentenceType = AUD_SCANNER_ASSISTANCE_REQUIRED;
		sentence->timeRequested = g_AudioEngine.GetTimeInMilliseconds();
		sentence->priority = prio;
		sentence->areaHash = posDescription.areaSound;

		const u32 assistanceRequired = g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_ASSISTANCE_REQUIRED_ASSISTANCE_REQUIRED", sentence->voiceNum);
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_ASSISTANCE_REQUIRED_ASSISTANCE_REQUIRED");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(assistanceRequired);

		g_AudioScannerManager.PopulateSentenceWithPosDescription(posDescription, sentence, idx);

		sentence->numPhrases = idx;

		sentence->inUse = true;
	}
}

void audPoliceScanner::ReportDispatch(const u32 units, const audUnitType type, const Vector3 &pos, bool forceAmbientPrio)
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
		return;

	// populate sentence structure
	audScannerSentence* sentence = g_AudioScannerManager.AllocateSentence();
	//Shouldn't be required, but I just hate not having it here.
	if(!sentence)
		return;

#if __BANK
	formatf(sentence->debugName, "Report Dispatch");
#endif

	if(sentence->inUse && sentence->priority > AUD_SCANNER_PRIO_LOW)
	{
		return;
	}

	audPositionDescription posDescription;

	DescribePosition(pos, posDescription, sentence->voiceNum, forceAmbientPrio);
	
	sentence->sentenceType = AUD_SCANNER_MULTI_DISPATCH;
	sentence->sentenceSubType = type;
	sentence->timeRequested = g_AudioEngine.GetTimeInMilliseconds();
	sentence->priority = AUD_SCANNER_PRIO_NORMAL;
	// let the player know if the cavalry are after him
	if(type == AUD_UNIT_AIR || units >= 4)
	{
		sentence->priority = AUD_SCANNER_PRIO_HIGH;	
	}
	sentence->areaHash = posDescription.areaSound;

	if(forceAmbientPrio)
		sentence->priority = AUD_SCANNER_PRIO_AMBIENT;

	u32 idx = 0;

	u32 dispatchOrder;

	if(type == AUD_UNIT_SWAT)
	{
		dispatchOrder = ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_SWAT_TEAM_FROM", 0xF18E985C);
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_SWAT_TEAM_FROM");
#endif
	}
	else if(type == AUD_UNIT_FBI)
	{
		dispatchOrder = ATPARTIALSTRINGHASH("DISPATCH_FIB_TEAM", 0x2B8D5457);
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] DISPATCH_FIB_TEAM");
#endif
	}
	else if(type == AUD_UNIT_AIR)
	{
		// const u32 singleAir = ATSTRINGHASH("POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_AIR_UNIT_FROM", 0x0ad1413e0);
		// const u32 multiAir = ATSTRINGHASH("POLICE_SCANNER_DISPATCH_MULTIPLE_AIR_UNITS_FROM", 0x0ee44d908);
		if(units == 1)
		{
			dispatchOrder = ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_AIR_UNIT_FROM", 0xD93E45FE);
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_AIR_UNIT_FROM");
#endif
		}
		else
		{
			dispatchOrder = ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_MULTIPLE_AIR_UNITS_FROM", 0x8033A774);
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_DISPATCH_MULTIPLE_AIR_UNITS_FROM");
#endif
		}
	}
	else
	{
		u32 numUnits = units;
		static const u32 unitDispatch[] = {
			ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_A_UNIT_FROM", 0xF21226C6),
			ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_2_UNITS_FROM", 0x4B5DAF78),
			ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_3_UNITS_FROM", 0x1DCFDDCF),
			ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_4_UNITS_FROM", 0xDE7B0F0),
			ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_5_UNITS_FROM", 0xB4C5B144),
			ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_6_UNITS_FROM", 0x5F67BCDB),
			ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_7_UNITS_FROM", 0x86B7A8A1)
		};
		
		const u32 numDispatchLines = sizeof(unitDispatch) / sizeof(unitDispatch[0]);

		const u32 lineIndex = Clamp<u32>(numUnits, 1, numDispatchLines) - 1;
		dispatchOrder = unitDispatch[lineIndex];
#if __BANK
		static const char* unitDispatchString[] = {
			"POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_A_UNIT_FROM",
			"POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_2_UNITS_FROM",
			"POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_3_UNITS_FROM",
			"POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_4_UNITS_FROM",
			"POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_5_UNITS_FROM",
			"POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_6_UNITS_FROM",
			"POLICE_SCANNER_DISPATCH_UNIT_FROM_DFROM_DISPATCH_7_UNITS_FROM"
		};

		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] %s", unitDispatchString[lineIndex]);
#endif
	}

	sentence->phrases[idx].postDelay = 0;
	sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
	sentence->phrases[idx].sound = NULL;
	sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice(dispatchOrder, sentence->voiceNum));

	sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
	sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(100, 2000);
	sentence->phrases[idx].sound = NULL;
#if __BANK
	if(g_PrintScannerTriggers)
		formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] areaSound");
#endif
	sentence->phrases[idx++].soundMetaRef = posDescription.areaSound;

	sentence->numPhrases = idx;
	sentence->playResponding = audEngineUtil::ResolveProbability(g_CopDispatchInteractionSettings ?
				g_CopDispatchInteractionSettings->UnitRespondingDispatch.Probability: 0.5f);
	BANK_ONLY(sentence->playResponding = sentence->playResponding || g_DebugCopDispatchInteraction;)
	sentence->inUse = true;
}

void audPoliceScanner::TriggerRandomChat()
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
		return;

	if(g_ScannerEnabled)
	{
		u32 idx = 0;
		// populate sentence structure
		audScannerSentence* sentence = g_AudioScannerManager.AllocateSentence();
		//Shouldn't be required, but I just hate not having it here.
		if(!sentence)
			return;

#if __BANK
		formatf(sentence->debugName, "Random Chat");
#endif
		
		sentence->sentenceType = AUD_SCANNER_RANDOM_CHAT;
		sentence->timeRequested = g_AudioEngine.GetTimeInMilliseconds();
		sentence->priority = AUD_SCANNER_PRIO_AMBIENT;

		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
		sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(500,2000);
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_RANDOM_CHAT");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_RANDOM_CHAT", sentence->voiceNum));

		sentence->numPhrases = idx;

		sentence->inUse = true;
	}
}

void audPoliceScanner::ReportPoliceSpottingPlayerVehicle(CVehicle *veh)
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
		return;

	CPed * player = FindPlayerPed();
	if(player)
	{
		if(player->GetVehiclePedInside() != veh)
		{
			naErrorf("Reporting police spotting player's vehicle but it's not the player's vehicle they've spotted");
		}
		const Vector3 pos = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition());
		ReportPoliceSpottingPlayer(pos, 0, true, true, true);	
	}
}

void audPoliceScanner::ReportPoliceRefocus()
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
		return;

	CPed * player = FindPlayerPed();
	if(player)
	{
		const Vector3 pos = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition());
		ReportPoliceSpottingPlayer(pos, 0, true, true);	
	}
}

void audPoliceScanner::ReportPoliceRefocusCritical()
{
	CPed * player = FindPlayerPed();
	if(player)
	{
		const Vector3 pos = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition());
		ReportPoliceSpottingPlayer(pos, 0, true, true, false, false, true);	
	}
}

void audPoliceScanner::RequestAssistanceCritical()
{
	CPed * player = FindPlayerPed();
	if(player)
	{
		const Vector3 pos = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition());
		ReportPoliceSpottingPlayer(pos, 0, true, true, false, true, true);
	}	
}

void audPoliceScanner::ReportPoliceSpottingPlayer(const Vector3 &pos, const s32 UNUSED_PARAM(oldWantedLevel), const bool UNUSED_PARAM(isCarKnown), bool forceSpotting, bool forceVehicle, 
													bool forceAssitance, bool forceCritical)
{	
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
		return;

	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	if(BANK_ONLY(!g_DebuggingScanner &&)
		(currentTime - m_LastTimeReportedSpottingPlayer < 20000 || currentTime - g_AudioScannerManager.GetLastTimePlayerWasClean() < 20000) )
	{
		return;
	}

	m_LastTimeReportedSpottingPlayer = currentTime;

	if(g_ScannerEnabled) 
	{
		if(forceSpotting || !forceAssitance)
		{
			if(g_AudioScannerManager.CanDoCopSpottingLine() && audEngineUtil::ResolveProbability(0.5f))
			{
				g_AudioScannerManager.TriggerCopDispatchInteraction(SCANNER_CDIT_PLAYER_SPOTTED);
				return;
			}

			CPed * player = FindPlayerPed();
			audPoliceSpotting params;
			params.vehDescription.audioEntity = NULL;
			u32 voiceNum = g_AudioScannerManager.SelectScannerVoice();
			DescribePosition(pos, params.posDescription, voiceNum);
			
			CVehicle *veh = player->GetVehiclePedInside();
			if(veh)
			{
				DescribeVehicle(veh, params.vehDescription, voiceNum);
				params.vehDescription.isInVehicle = true;
				// convert to mph
				params.speed = Min(91.f,DotProduct(veh->GetVelocity(), VEC3V_TO_VECTOR3(veh->GetTransform().GetB())) * 2.23693629f);
			}
			else
			{
				// if the player is getting into a car or on a train then we dont want to report they're on foot since it sounds silly
				if(player->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE) ||
					!player->GetPedAudioEntity() || player->GetPedAudioEntity()->HasBeenOnTrainRecently())
				{
					params.forceVehicle = false;
					params.vehDescription.isVehicleKnown = false;
					params.vehDescription.isInVehicle = true;
					//Also, don't play street if playing location
					params.posDescription.streetSound.SetInvalid();
				}
				else
				{
					params.vehDescription.isInVehicle = false;
					params.speed = 0.f;
				}
			}

			params.priority = (forceCritical?AUD_SCANNER_PRIO_CRITICAL:(forceSpotting?AUD_SCANNER_PRIO_HIGH:AUD_SCANNER_PRIO_LOW));
			params.forceVehicle = forceVehicle;
			params.heading = g_AudioScannerManager.GetPlayerHeading();
			params.isPlayerHeading = true;
			ReportPoliceSpotting(params);
		}
		else if(forceAssitance || audEngineUtil::ResolveProbability(g_ReportSpottingProbability*0.3f))
		{
			RequestAssistance(pos, forceCritical ? AUD_SCANNER_PRIO_CRITICAL : AUD_SCANNER_PRIO_NORMAL);
		}		
	}
}

//Called from script...includes an accuracy value
void audPoliceScanner::ReportPoliceSpottingNonPlayer(const CPed* ped, f32 accuracy, bool force, bool forceVehicle)
{	
	if(!ped)
		return;

	if(g_ScannerEnabled) 
	{
		if((force || audEngineUtil::ResolveProbability(g_ReportSpottingProbability)))
		{
			audPoliceSpotting params;
			params.vehDescription.audioEntity = NULL;
			u32 voiceNum = g_AudioScannerManager.SelectScannerVoice();
			DescribePosition(VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition()), params.posDescription, voiceNum);

			CVehicle *veh = ped->GetVehiclePedInside();
			if(!veh || !ped->GetPedIntelligence() || !ped->GetPedIntelligence()->GetQueriableInterface())
			{
				// if the ped is getting into a car then we dont want to report they're on foot since it sounds silly
				if(ped->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE))
				{
					params.vehDescription.isInVehicle = true;
				}
			}
			if(veh)
			{
				DescribeVehicle(veh, params.vehDescription, voiceNum);
				params.vehDescription.isInVehicle = true;
				// convert to mph
				params.speed = Min(91.f,DotProduct(veh->GetVelocity(), VEC3V_TO_VECTOR3(veh->GetTransform().GetB())) * 2.23693629f);
			}
			else
			{
				params.vehDescription.isInVehicle = false;
				params.speed = 0.f;
			}

			params.priority = (force?AUD_SCANNER_PRIO_HIGH:AUD_SCANNER_PRIO_LOW);
			params.forceVehicle = forceVehicle;
			params.fromScript = true;
			params.heading = AUD_DIR_UNKNOWN;
			if(ped->GetVelocity().Mag2() > (g_MinPlayerMovementThreshold*g_MinPlayerMovementThreshold))
			{
				Vector3 pedMovement(VEC3V_TO_VECTOR3(ped->GetTransform().GetForward()));

				// split into 90 degree segments, with north spanning 315 -> 45
				params.heading = audScannerManager::CalculateAudioDirection(pedMovement);
			}
			params.isPlayerHeading = false;
			ReportPoliceSpotting(params, accuracy);
		}
		else if(audEngineUtil::ResolveProbability(g_ReportSpottingProbability*0.3f))
		{
			RequestAssistance(VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition()), AUD_SCANNER_PRIO_NORMAL, true);
		}		
	}
}

void audPoliceScanner::ReportPoliceSpotting(const audPoliceSpotting &params)
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner) && !params.fromScript)
		return;

	// populate sentence structure
	audScannerSentence* sentence = g_AudioScannerManager.AllocateSentence(true, AUD_SCANNER_PRIO_LOW);
	if(!sentence)
		return;

#if __BANK
	formatf(sentence->debugName, "Report Police Spotting");
#endif

	sentence->sentenceType = AUD_SCANNER_SUSPECT_SPOTTED;
	sentence->timeRequested = g_AudioEngine.GetTimeInMilliseconds();
	sentence->priority = params.priority;

	bool shouldNotPlayHeading = false;
	if(params.isPlayerHeading)
	{
		sentence->vehicleHistory.isPlayer = params.isPlayerHeading;
		if(FindPlayerVehicle())
		{
			if(FindPlayerVehicle()->GetVelocity().Mag2() < 100.0f)
			{
				shouldNotPlayHeading = true;
			}
		}
		else if(FindPlayerPed() && FindPlayerPed()->GetVelocity().Mag2() < 20.0f)
		{
			shouldNotPlayHeading = true;
		}
	}
	
	u32 idx = 0;

	// SUSPECT
	sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_1;
	sentence->phrases[idx].postDelay = 0;
	sentence->phrases[idx].sound = NULL;
#if __BANK
	if(g_PrintScannerTriggers)
		formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_SUSPECT");
#endif
	sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_SUSPECT", sentence->voiceNum));

	if(params.speed > 50.f && audEngineUtil::ResolveProbability(0.75f))
	{
		// round to nearest 10mph
		u32 speed = static_cast<u32>(params.speed);
		speed -= (speed%10);	
		char buf[32];
		formatf(buf,sizeof(buf),"SPEED_%u",speed);
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64,"[audScannerSpew] %s", buf);
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice(buf, sentence->voiceNum));
		sentence->sentenceSubType = 1;
	}
	else
	{
		if( !shouldNotPlayHeading && (params.heading != AUD_DIR_UNKNOWN || params.isPlayerHeading) && audEngineUtil::ResolveProbability(g_ReportSuspectHeadingProb))
		{
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].sound = NULL;
			sentence->phrases[idx++].isPlayerHeading = params.isPlayerHeading;
#if __BANK
			if(g_PrintScannerTriggers)
			{
				if(params.isPlayerHeading)
				{
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew]  %s", "POLICE_SCANNER player heading");
				}
				else
				{
					switch(params.heading)
					{
					case 0:
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] %s", "POLICE_SCANNER_HEADING_NORTH");
						break;
					case 1:
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] %s", "POLICE_SCANNER_HEADING_EAST");
						break;
					case 2:
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] %s", "POLICE_SCANNER_HEADING_SOUTH");
						break;
					case 3:
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] %s", "POLICE_SCANNER_HEADING_WEST");
						break;
					default:
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] no heading");
						break;
					}
				}
			}
#endif
			if(params.heading < g_AudioPoliceScannerDir.size())
			{
				SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice(g_AudioPoliceScannerDir[params.heading], sentence->voiceNum));
			}
			
			sentence->sentenceSubType = 2;
		}
		else
		{
			// LAST SEEN
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_LAST_SEEN");
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_LAST_SEEN", sentence->voiceNum));
			sentence->sentenceSubType = 3;
		}
	}

	// either report car details or location details
	if(params.forceVehicle || (params.vehDescription.isVehicleKnown && audEngineUtil::ResolveProbability(g_ReportCarProb) && (!params.vehDescription.audioEntity || !params.vehDescription.audioEntity->m_HasScannerDescribedModel)))
	{
		if(params.vehDescription.isInVehicle)
		{
			bool reportModel;

			// IN/ON
			if(params.vehDescription.isMotorbike)
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_2;
				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_ON_SHORT");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_ON_SHORT", sentence->voiceNum));
			}
			else
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_2;
				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_IN");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_IN", sentence->voiceNum));
				//sentence->phrases[idx++].soundHash = g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_IN", 0);
			}
			

			// A
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_3;
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_A");
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_A", sentence->voiceNum));
			//sentence->phrases[idx++].soundHash = g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_A", 0);

			// PREFIX
			if(params.vehDescription.prefixSound != 0 && params.vehDescription.prefixSound != g_NullSoundHash && audEngineUtil::ResolveProbability(g_ReportCarPrefixProb))
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] prefixSound");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.prefixSound);	
				reportModel = false;
			}
			else
			{
				reportModel = true;
			}

			// COLOUR
			if(params.vehDescription.colourSound != 0 && params.vehDescription.colourSound != g_NullSoundHash)
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] coloursound");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.colourSound);
				
				sentence->vehicleHistory.hasDescribedColour = true;
				
			}

			// MAKE / CATEGORY
			if((params.vehDescription.manufacturerSound == 0 || params.vehDescription.manufacturerSound == g_NullSoundHash) || (params.vehDescription.audioEntity && params.vehDescription.category && !params.vehDescription.audioEntity->m_HasScannerDescribedCategory && audEngineUtil::ResolveProbability(0.8f)))
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
				sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(400, 2000);
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] categorySound");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.category);
				sentence->vehicleHistory.hasDescribedCategory = true;
				reportModel = false;
			}
			else
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] manufacturerSound");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.manufacturerSound);	
				
				sentence->vehicleHistory.hasDescribedManufacturer = true;
			}

			if(reportModel && params.vehDescription.modelSound != 0 && params.vehDescription.modelSound != g_NullSoundHash)
			{
				if(params.vehDescription.category == g_NullSoundHash || audEngineUtil::ResolveProbability(0.5f) || (params.vehDescription.modelSound != g_NullSoundHash && params.vehDescription.audioEntity && params.vehDescription.audioEntity->m_HasScannerDescribedCategory))
				{
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
					sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(100, 2000);
					sentence->phrases[idx].sound = NULL;
#if __BANK
					if(g_PrintScannerTriggers)
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] modelSound");
#endif
					sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.modelSound);
					sentence->vehicleHistory.hasDescribedModel = true;
					sentence->vehicleHistory.hasDescribedCategory = true;
				}
				else
				{
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
					sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(100, 2000);
					sentence->phrases[idx].sound = NULL;
#if __BANK
					if(g_PrintScannerTriggers)
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] categorySound");
#endif
					sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.category);
					sentence->vehicleHistory.hasDescribedCategory = true;
				}
			}
			else
			{
				// not reporting model so make was the last phrase - post delay it
				sentence->phrases[idx-1].postDelay = audEngineUtil::GetRandomNumberInRange(100, 2000);
			}
			
			sentence->vehicleHistory.isOnFoot = false;
			sentence->vehicleHistory.audioEntity = params.vehDescription.audioEntity;
		}
		sentence->sentenceSubType = 1;
	}
	else if (!params.vehDescription.isInVehicle && audEngineUtil::ResolveProbability(0.5f))
	{
		// FOOT
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
		sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(100, 2000);
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_ON_FOOT");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_ON_FOOT", sentence->voiceNum));

		sentence->vehicleHistory.isOnFoot = true;
		sentence->sentenceSubType = 1;
	}
	else
	{
		g_AudioScannerManager.PopulateSentenceWithPosDescription(params.posDescription, sentence, idx);
	}

	sentence->playAcknowledge = audEngineUtil::ResolveProbability(0.5f);

	if(idx > 0)
	{
		sentence->numPhrases = idx;
		sentence->inUse = true;
	}
}

//A version that takes an accuracy value.  This is for non-player suspect sightings triggered from script
void audPoliceScanner::ReportPoliceSpotting(const audPoliceSpotting &params, f32 accuracy)
{
	// populate sentence structure
	audScannerSentence* sentence = g_AudioScannerManager.AllocateSentence(true, AUD_SCANNER_PRIO_LOW);
	if(!sentence)
		return;

#if __BANK
	formatf(sentence->debugName, "Report Police Spotting w/ Accuracy");
#endif

	sentence->sentenceType = AUD_SCANNER_SUSPECT_SPOTTED;
	sentence->timeRequested = g_AudioEngine.GetTimeInMilliseconds();
	sentence->priority = params.priority;

	u32 idx = 0;

	// SUSPECT
	sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_1;
	sentence->phrases[idx].postDelay = 0;
	sentence->phrases[idx].sound = NULL;
#if __BANK
	if(g_PrintScannerTriggers)
		formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_SUSPECT");
#endif
	sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_SUSPECT", sentence->voiceNum));

	if(params.speed > 50.f && accuracy > g_SpeedInfoMinAccuracy)
	{
		// round to nearest 10mph
		u32 speed = static_cast<u32>(params.speed);
		speed -= (speed%10);	
		char buf[32];
		formatf(buf,sizeof(buf),"SPEED_%u",speed);
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64,"[audScannerSpew] %s", buf);
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice(buf, sentence->voiceNum));
		sentence->sentenceSubType = 1;
	}
	else
	{
		if( (params.heading != AUD_DIR_UNKNOWN || params.isPlayerHeading) && accuracy > g_HeadingInfoMinAccuracy)
		{
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].sound = NULL;
			sentence->phrases[idx].isPlayerHeading = params.isPlayerHeading;
#if __BANK
			if(g_PrintScannerTriggers)
			{
				if(params.isPlayerHeading)
				{
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] %s", "POLICE_SCANNER player heading");
				}
				else
				{
					switch(params.heading)
					{
					case 0:
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] %s", "POLICE_SCANNER_HEADING_NORTH");
						break;
					case 1:
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] %s", "POLICE_SCANNER_HEADING_EAST");
						break;
					case 2:
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] %s", "POLICE_SCANNER_HEADING_SOUTH");
						break;
					case 3:
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] %s", "POLICE_SCANNER_HEADING_WEST");
						break;
					default:
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] no heading");
						break;
					}
				}
			}
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice(g_AudioPoliceScannerDir[params.heading], sentence->voiceNum));
			sentence->sentenceSubType = 2;
		}
		else
		{
			// LAST SEEN
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_LAST_SEEN");
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_LAST_SEEN", sentence->voiceNum));
			sentence->sentenceSubType = 3;
		}
	}

	// either report car details or location details
	if(params.forceVehicle || (params.vehDescription.isVehicleKnown && accuracy > g_VehicleInfoMinAccuracy && (!params.vehDescription.audioEntity || !params.vehDescription.audioEntity->m_HasScannerDescribedModel)))
	{
		if(params.vehDescription.isInVehicle)
		{
			bool reportModel;

			// IN/ON
			if(params.vehDescription.isMotorbike)
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_2;
				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_ON_SHORT");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_ON_SHORT", sentence->voiceNum));
			}
			else
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_2;
				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_IN");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_IN", sentence->voiceNum));
				//sentence->phrases[idx++].soundHash = g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_IN", 0);
			}


			// A
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_3;
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_A");
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_A", sentence->voiceNum));
			//sentence->phrases[idx++].soundHash = g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_A", 0);

			// PREFIX
			if(params.vehDescription.prefixSound != 0 && params.vehDescription.prefixSound != g_NullSoundHash && accuracy > g_CarPrefixInfoMinAccuracy)
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] prefixSound");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.prefixSound);	
				reportModel = false;
			}
			else
			{
				reportModel = true;
			}

			// COLOUR
			if(params.vehDescription.colourSound != 0 && params.vehDescription.colourSound != g_NullSoundHash)
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] coloursound");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.colourSound);

				sentence->vehicleHistory.hasDescribedColour = true;

			}

			// MAKE / CATEGORY
			if((params.vehDescription.manufacturerSound == 0 || 
				params.vehDescription.manufacturerSound == g_NullSoundHash) || 
				(params.vehDescription.audioEntity && params.vehDescription.category && !params.vehDescription.audioEntity->m_HasScannerDescribedCategory && accuracy > g_CarCategoryInfoMinAccuracy))
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
				sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(400, 2000);
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] categorySound");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.category);
				sentence->vehicleHistory.hasDescribedCategory = true;
				reportModel = false;
			}
			else
			{
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] manufacturerSound");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.manufacturerSound);	

				sentence->vehicleHistory.hasDescribedManufacturer = true;
			}

			if(reportModel && params.vehDescription.modelSound != 0 && params.vehDescription.modelSound != g_NullSoundHash)
			{
				if(params.vehDescription.category == g_NullSoundHash || 
					accuracy > g_CarModelInfoMinAccuracy  || 
					(params.vehDescription.modelSound != g_NullSoundHash && params.vehDescription.audioEntity && params.vehDescription.audioEntity->m_HasScannerDescribedCategory))
				{
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
					sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(100, 2000);
					sentence->phrases[idx].sound = NULL;
#if __BANK
					if(g_PrintScannerTriggers)
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] modelSound");
#endif
					sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.modelSound);
					sentence->vehicleHistory.hasDescribedModel = true;
					sentence->vehicleHistory.hasDescribedCategory = true;
				}
				else
				{
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
					sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(100, 2000);
					sentence->phrases[idx].sound = NULL;
#if __BANK
					if(g_PrintScannerTriggers)
						formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] categorySound");
#endif
					sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(params.vehDescription.category);
					sentence->vehicleHistory.hasDescribedCategory = true;
				}
			}
			else
			{
				// not reporting model so make was the last phrase - post delay it
				sentence->phrases[idx-1].postDelay = audEngineUtil::GetRandomNumberInRange(100, 2000);
			}

			sentence->vehicleHistory.isOnFoot = false;
			sentence->vehicleHistory.audioEntity = params.vehDescription.audioEntity;
		}
		else
		{
			// FOOT
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
			sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(100, 2000);
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] POLICE_SCANNER_ON_FOOT");
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_ON_FOOT", sentence->voiceNum));

			sentence->vehicleHistory.isOnFoot = true;
		}
		sentence->sentenceSubType = 1;
	}
	else
	{
		g_AudioScannerManager.PopulateSentenceWithPosDescription(params.posDescription, sentence, idx);
	}

	if(idx > 0)
	{
		sentence->numPhrases = idx;
		sentence->inUse = true;
	}
}

bool audPoliceScanner::FindSpecificLocation(const Vector3& pos, u32& locationDirectionSound, u32& locationSound, f32&probOfPlaying, const u32 voiceNum)
{
	bool isInAircraft = false;
	if(FindPlayerPed() && FindPlayerPed()->GetVehiclePedInside())
	{
		isInAircraft = FindPlayerPed()->GetVehiclePedInside()->GetVehicleType() == VEHICLE_TYPE_PLANE ||
			FindPlayerPed()->GetVehiclePedInside()->GetVehicleType() == VEHICLE_TYPE_HELI ||
			FindPlayerPed()->GetVehiclePedInside()->GetVehicleType() == VEHICLE_TYPE_BLIMP;
	}

	if(!m_specificLocationListHash || isInAircraft)
		return false;

	ScannerSpecificLocation* smallestLocation = NULL;
	f32 smallestLocationRadiusSq = FLT_MAX;
	f32 smallestLocationDistSq = 0;
	Vector3 smallestLocationPos = ORIGIN;

	for(int i=0; i<m_specificLocationListHash->numLocations; i++)
	{
		ScannerSpecificLocation* location = audNorthAudioEngine::GetObject<ScannerSpecificLocation>(m_specificLocationListHash->Location[i].Ref);
		if(location)
		{
			f32 radiusSquared = location->Radius * location->Radius;
			Vector3 locVec(location->Position.x, location->Position.y, location->Position.z);
			f32 distSq = (pos - locVec).Mag2();
			if(distSq < radiusSquared && radiusSquared < smallestLocationRadiusSq)
			{
				smallestLocation = location;
				smallestLocationRadiusSq = radiusSquared;
				smallestLocationPos.Set(locVec);
				smallestLocationDistSq = distSq;
			}
		}
	}

	if(smallestLocation)
	{
		char buf[256];
		if(Verifyf(voiceNum < smallestLocation->numSounds, "ScannerSpecificLocation object does not have enough sounds.  See output for details"))
		{
			locationSound = smallestLocation->Sounds[voiceNum].Sound;
		}
		else
		{
#if !__FINAL
			Errorf("Missing sound for voice %i SpecificLocation %s", voiceNum, 
				audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(smallestLocation->NameTableOffset));
#endif
			return false;
		}
		probOfPlaying = smallestLocation->ProbOfPlaying;
		f32 pctOfRadiusFromCenter = sqrt(smallestLocationDistSq)/smallestLocation->Radius;
		if(pctOfRadiusFromCenter <= g_MinPctForLocationAt)
		{
			formatf(buf, sizeof(buf), "POLICE_SCANNER_CONJUNCTIVES_AT");
		}
		else if(pctOfRadiusFromCenter <= g_MinPctForLocationNear)
		{
			formatf(buf, sizeof(buf), "POLICE_SCANNER_CONJUNCTIVES_NEAR");
		}
		else
		{
			const Vector3 north = Vector3(0.0f, 1.0f, 0.0f);
			const Vector3 east = Vector3(1.0f,0.0f,0.0f);
			Vector3 dir = pos - smallestLocationPos;
			dir.z = 0.0f;

			dir.NormalizeFast();
			f32 cosangle = dir.Dot(north);
			f32 angle = AcosfSafe(cosangle);
			f32 degrees = RtoD*angle;
			f32 actualDegrees = 0.0f;

			if(dir.Dot(east) <= 0.0f)
			{
				actualDegrees = 360.0f - degrees;
			}
			else
			{
				actualDegrees = degrees;
			}

			// split into 90 degree segments, with north spanning 315 -> 45
			u32 segment = static_cast<u32>(((actualDegrees+45.f) / 90.0f)) % 4;

			static const char *ofdirections[] = {"NORTH_OF_UHM","EAST_OF_UHM","SOUTH_OF_UHM","WEST_OF_UHM"};
			formatf(buf, sizeof(buf), "POLICE_SCANNER_NEAR_DIR_%s", ofdirections[segment]);
		}

		locationDirectionSound = g_AudioScannerManager.GetScannerHashForVoice(buf, voiceNum);
		return true;
	}

	return false;
}

void audPoliceScanner::DescribePosition(const Vector3 &pos, audPositionDescription &desc, const u32 voiceNum, bool isAmbient)
{
	char buf[255];
	const char *inDirection;

	CPopZone *zone = CPopZones::FindSmallestForPosition(&pos, ZONECAT_ANY, ZONETYPE_SP);
	naAssertf(zone, "Attempted to find smallest zone for position but a null ptr was returned.  Pos: %f, %f, %f", pos.GetX(), pos.GetY(), pos.GetZ());

	const float fSecondSurfaceInterp=0.0f;
	bool hitGround = false;
	float ground = WorldProbe::FindGroundZFor3DCoord(fSecondSurfaceInterp, pos.x, pos.y, pos.z, &hitGround);

	if(!isAmbient)
	{
		if( (!hitGround || pos.z - ground > g_MinHeightToSayOver) && FindPlayerVehicle() &&
			(FindPlayerVehicle()->GetVehicleType() == VEHICLE_TYPE_PLANE || FindPlayerVehicle()->GetVehicleType() == VEHICLE_TYPE_HELI ||
				FindPlayerVehicle()->GetVehicleType() == VEHICLE_TYPE_BLIMP)
			)
		{
			CPopZone* navZone = CPopZones::FindSmallestForPosition(&pos, ZONECAT_NAVIGATION, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
			char areaName[256];
			if(navZone)
			{
				safecpy(areaName, navZone->m_associatedTextId.TryGetCStr(), NELEM(areaName));
			}
			else
			{
				safecpy(areaName, CUserDisplay::AreaName.GetNameTextKey(), NELEM(areaName));
			}

			desc.areaConjunctiveSound = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(
				g_AudioScannerManager.GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_CONJUNCTIVES_OVER", 0x71FFB146), voiceNum));
			desc.areaSound = m_OverAreaSoundMappings[voiceNum].Find(areaName);
			return;
		}
	}

	f32 probOfPlaying = 0.0f;
	if(!FindSpecificLocation(pos, desc.locationDirectionSound, desc.locationSound, probOfPlaying, voiceNum) || !audEngineUtil::ResolveProbability(probOfPlaying))
	{
		//found location, but failed prob check
		desc.locationDirectionSound = 0;
		desc.locationSound = 0;
	}

	// search through all remembered streets and choose the most recent
	u32 mostRecentIndex = 0;
	for(u32 i = 0 ; i < NUM_REMEMBERED_STREET_NAMES; i++)
	{
		if(CUserDisplay::StreetName.TimeInRange[i] >= CUserDisplay::StreetName.TimeInRange[mostRecentIndex])
		{
			mostRecentIndex = i;
		}
	}

	audMetadataRef conjunctiveSound;
	conjunctiveSound.SetInvalid();
#if __BANK
	if(g_DebuggingScanner && g_TestStreetName[0] != '\0')
	{
		desc.streetSound = m_StreetAndAreaSoundMappings[voiceNum].Find(g_TestStreetName);
		conjunctiveSound = m_StreetAndAreaConjunctiveSoundMappings[voiceNum].Find(g_TestStreetName);

		if(conjunctiveSound == g_ScannerNullSoundRef)
			desc.streetConjunctiveSound = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(
								g_AudioScannerManager.GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_ON_LONG", 0x22EB3D83), voiceNum));
		else if(conjunctiveSound == g_ScannerNoConjunctiveSoundRef)
			desc.streetConjunctiveSound.SetInvalid();
		else
			desc.streetConjunctiveSound = conjunctiveSound;
	}
	else
#endif
	if(CUserDisplay::StreetName.RememberedStreetName[mostRecentIndex] != 0)
	{		
		desc.streetSound = m_StreetAndAreaSoundMappings[voiceNum].Find(CUserDisplay::StreetName.RememberedStreetName[mostRecentIndex]);
		conjunctiveSound = m_StreetAndAreaConjunctiveSoundMappings[voiceNum].Find(CUserDisplay::StreetName.RememberedStreetName[mostRecentIndex]);

		if(conjunctiveSound == g_ScannerNullSoundRef)
			desc.streetConjunctiveSound = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(
												g_AudioScannerManager.GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_ON_LONG", 0x22EB3D83), voiceNum));
		else if(conjunctiveSound == g_ScannerNoConjunctiveSoundRef)
			desc.streetConjunctiveSound.SetInvalid();
		else
			desc.streetConjunctiveSound = conjunctiveSound;
	}
	else
	{
		desc.streetSound.SetInvalid();
	}

#if __BANK
	if(g_DebuggingScanner && g_TestAreaName[0] != '\0')
	{
		desc.areaSound = m_StreetAndAreaSoundMappings[voiceNum].Find(g_TestAreaName);
		conjunctiveSound = m_StreetAndAreaConjunctiveSoundMappings[voiceNum].Find(g_TestAreaName);


		if(conjunctiveSound == g_ScannerNullSoundRef)
			desc.areaConjunctiveSound = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(
											g_AudioScannerManager.GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_IN", 0xA0F9744D), voiceNum));
		else if(conjunctiveSound == g_ScannerNoConjunctiveSoundRef)
			desc.areaConjunctiveSound.SetInvalid();
		else if(conjunctiveSound == g_ScannerNoDirectionSoundRef)
		{
			desc.areaConjunctiveSound = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(
							g_AudioScannerManager.GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_IN", 0xA0F9744D), voiceNum));
		}
		else if(conjunctiveSound == g_ScannerNoConjNoDirSoundRef)
		{
			desc.areaConjunctiveSound.SetInvalid();
		}
		else
			desc.areaConjunctiveSound = conjunctiveSound;
	}
	else
#endif
    if(zone)
    {
	    // compute zone midpoint
	    const Vector3 centre = Vector3((f32)zone->m_uberZoneCentreX, (f32)zone->m_uberZoneCentreY, 0.5f * ((f32)CPopZones::GetPopZoneZ1(zone) + (f32)CPopZones::GetPopZoneZ2(zone)));
	    const Vector3 north = Vector3(0.0f, 1.0f, 0.0f);
	    const Vector3 east = Vector3(1.0f,0.0f,0.0f);
	    Vector3 dir = pos - centre;
	    dir.z = 0.0f;
	    const f32 distToCentre2 = dir.Mag2();

	    desc.inDirectionSound = 0;

		CPopZone* navZone = CPopZones::FindSmallestForPosition(&pos, ZONECAT_NAVIGATION, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
		char areaName[256];
		if(navZone)
		{
			safecpy(areaName, navZone->m_associatedTextId.TryGetCStr(), NELEM(areaName));
		}
		else
		{
			safecpy(areaName, CUserDisplay::AreaName.GetNameTextKey(), NELEM(areaName));
		}

		bool dontSayDirection = false;
		desc.areaSound = m_StreetAndAreaSoundMappings[voiceNum].Find(areaName);
		conjunctiveSound = m_StreetAndAreaConjunctiveSoundMappings[voiceNum].Find(areaName);

		if(conjunctiveSound == g_ScannerNullSoundRef)
			desc.areaConjunctiveSound = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(
						g_AudioScannerManager.GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_IN", 0xA0F9744D), voiceNum));
		else if(conjunctiveSound == g_ScannerNoConjunctiveSoundRef)
			desc.areaConjunctiveSound.SetInvalid();
		else if(conjunctiveSound == g_ScannerNoDirectionSoundRef)
		{
			desc.areaConjunctiveSound = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(
						g_AudioScannerManager.GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_IN", 0xA0F9744D), voiceNum));
			dontSayDirection = true;
		}
		else if(conjunctiveSound == g_ScannerNoConjNoDirSoundRef)
		{
			desc.areaConjunctiveSound.SetInvalid();
			dontSayDirection = true;
		}
		else
			desc.areaConjunctiveSound = conjunctiveSound;

#if __BANK
		if(g_ShowAreaDebug)
		{
			Displayf("Police Scanner Area Selected: %s", areaName);
		}
#endif

	    // check the zone is big enough to warrant saying direction
	    if(zone->m_bBigZoneForScanner)
	    {
		    // check how close we are to the centre of the zone, ignoring z
		    f32 distFactor = Min(1.f, distToCentre2 / (90.f*90.f));
		    f32 actualDegrees = 0.0f;
			
			if(!dontSayDirection)
			{
			    if(distFactor <= g_DistFactorToConsiderCentral)
			    {
				    inDirection = "IN_CENTRAL";
			    }
			    else
			    {
				    dir.NormalizeFast();
				    f32 cosangle = dir.Dot(north);
				    f32 angle = AcosfSafe(cosangle);
				    f32 degrees = RtoD*angle;
    				
				    if(dir.Dot(east) <= 0.0f)
				    {
					    actualDegrees = 360.0f - degrees;
				    }
				    else
				    {
					    actualDegrees = degrees;
				    }

				    // split into 90 degree segments, with north spanning 315 -> 45
				    u32 segment = static_cast<u32>(((actualDegrees+45.f) / 90.0f)) % 4;

				    static const char *indirections[] = {"NORTH","EAST","SOUTH","WEST"};
				    static const char *indirectionsern[] = {"NORTHERN","EASTERN","SOUTHERN","WESTERN"};
				    // cheeky fudge - if the area name has an odd number of characters then use -ern
				    if(strlen(areaName) % 2)
				    {
					    inDirection = indirectionsern[segment];
				    }
				    else
				    {
					    inDirection = indirections[segment];
				    }
			    }
			    formatf(buf, sizeof(buf), "POLICE_SCANNER_IN_DIRECTION_%s", inDirection);
			    desc.inDirectionSound = g_AudioScannerManager.GetScannerHashForVoice(buf, voiceNum);
		    }
	    }
    }
}

void audPoliceScanner::DescribePositionForScriptedLine(const Vector3 &pos, audPositionDescription &desc, const u32 voiceNum)
{
	char buf[255];
	const char *direction, *inDirection;

	f32 probOfPlaying = 0.0f;
	if(!FindSpecificLocation(pos, desc.locationDirectionSound, desc.locationSound, probOfPlaying, voiceNum) || !audEngineUtil::ResolveProbability(probOfPlaying))
	{
		//found location, but failed prob check
		desc.locationDirectionSound = 0;
		desc.locationSound = 0;
	}

	desc.streetSound.SetInvalid();
	CNodeAddress	aNode;
	s32 NodesFound = ThePaths.RecordNodesInCircle(pos, 5.0f, 1, &aNode, false, false, false);
	if(NodesFound != 0)
	{
		u32 streetName = ThePaths.FindNodePointer(aNode)->m_streetNameHash;

		if(streetName != 0)
		{
			desc.streetSound = m_StreetAndAreaSoundMappings[voiceNum].Find(streetName);
			audMetadataRef conjunctiveSound = m_StreetAndAreaConjunctiveSoundMappings[voiceNum].Find(streetName);

			if(conjunctiveSound == g_ScannerNullSoundRef)
				desc.streetConjunctiveSound = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(
						g_AudioScannerManager.GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_ON_LONG", 0x22EB3D83), voiceNum));
			else if(conjunctiveSound == g_ScannerNoConjunctiveSoundRef)
				desc.streetConjunctiveSound.SetInvalid();
			else
				desc.streetConjunctiveSound = conjunctiveSound;
		}
		else
		{
			desc.streetSound.SetInvalid();
		}
	}
	
	CPopZone *zone = CPopZones::FindSmallestForPosition(&pos, ZONECAT_ANY, ZONETYPE_SP);
	naAssertf(zone, "Attempted to find smallest zone for position but a null ptr was returned");

	if(zone)
	{
		// compute zone midpoint
		const Vector3 centre = Vector3((f32)zone->m_uberZoneCentreX, (f32)zone->m_uberZoneCentreY, 0.5f * ((f32)CPopZones::GetPopZoneZ1(zone) + (f32)CPopZones::GetPopZoneZ2(zone)));
		const Vector3 north = Vector3(0.0f, 1.0f, 0.0f);
		const Vector3 east = Vector3(1.0f,0.0f,0.0f);
		Vector3 dir = pos - centre;
		dir.z = 0.0f;
		const f32 distToCentre2 = dir.Mag2();

		desc.inDirectionSound = 0;

		char areaName[256];
		safecpy( areaName, zone->GetTranslatedName() );
		for(u32 i = 0; i < strlen(areaName); i++)
		{
			if(areaName[i] == ' ')
			{
				areaName[i] = '_';
			}
		}

		bool dontSayDirection = false;
		audMetadataRef conjunctiveSound = m_StreetAndAreaConjunctiveSoundMappings[voiceNum].Find(CUserDisplay::AreaName.GetNameTextKey());

		if(conjunctiveSound == g_ScannerNullSoundRef)
			desc.areaConjunctiveSound = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(
							g_AudioScannerManager.GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_IN", 0xA0F9744D), voiceNum));
		else if(conjunctiveSound == g_ScannerNoConjunctiveSoundRef)
			desc.areaConjunctiveSound.SetInvalid();
		else if(conjunctiveSound == g_ScannerNoDirectionSoundRef)
		{
			desc.areaConjunctiveSound = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(
							g_AudioScannerManager.GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_IN", 0xA0F9744D), voiceNum));
			dontSayDirection = true;
		}
		else if(conjunctiveSound == g_ScannerNoConjNoDirSoundRef)
		{
			desc.areaConjunctiveSound.SetInvalid();
			dontSayDirection = true;
		}
		else
			desc.areaConjunctiveSound = conjunctiveSound;

#if __BANK
		if(g_ShowAreaDebug)
		{
			Displayf("Police Scanner Area Selected: %s", areaName);
		}
#endif

		// check the zone is big enough to warrant saying direction
		if(zone->m_bBigZoneForScanner)
		{
			// check how close we are to the centre of the zone, ignoring z
			f32 distFactor = Min(1.f, distToCentre2 / (90.f*90.f));
			f32 actualDegrees = 0.0f;
			
			if(!dontSayDirection)
			{
				if(distFactor <= g_DistFactorToConsiderCentral)
				{
					// we're close enough to call this central
					direction = "CENTRAL";
					inDirection = "IN_CENTRAL";
				}
				else
				{
					dir.NormalizeFast();
					f32 cosangle = dir.Dot(north);
					f32 angle = AcosfSafe(cosangle);
					f32 degrees = RtoD*angle;

					if(dir.Dot(east) <= 0.0f)
					{
						actualDegrees = 360.0f - degrees;
					}
					else
					{
						actualDegrees = degrees;
					}

					// split into 90 degree segments, with north spanning 315 -> 45
					u32 segment = static_cast<u32>(((actualDegrees+45.f) / 90.0f)) % 4;

					static const char *directionsern[] = {"NORTHERN","EASTERN","SOUTHERN","WESTERN"};
					//char *indirections[] = {"IN_NORTH","IN_EAST","IN_SOUTH","IN_WEST"};
					//char *indirectionsern[] = {"IN_NORTHERN","IN_EASTERN","IN_SOUTHERN","IN_WESTERN"};
					// cheeky fudge - if the area name has an odd number of characters then use -ern
					if(strlen(areaName) % 2)
					{
						inDirection = directionsern[segment];
					}
					else
					{
						inDirection = directionsern[segment];
					}
				}
				formatf(buf, sizeof(buf), "POLICE_SCANNER_IN_DIRECTION_%s", inDirection);
				desc.inDirectionSound = g_AudioScannerManager.GetScannerHashForVoice(buf, voiceNum);
			}
		}
	}
}

void audPoliceScanner::GetVechicleColorSoundHashes(u32 voiceNum, u8 prefix, u8 colour, audVehicleDescription &desc)
{
	char buf[64];
	formatf(buf, "%.2i_SCANNER_VOICE_PARAMS", voiceNum);
	const ScannerVoiceParams* params = audNorthAudioEngine::GetObject<ScannerVoiceParams>(buf);
	if(!params)
	{
		prefix = colour = 0;
		return;
	}

	//These need to be kept in line with the enums found in VehicleModelInfoColors.h.  If we're having issues around here, that would
	//	be the first thing to check.
	switch(prefix)
	{
	case CVehicleModelColor::EVehicleModelAudioPrefix_bright:
		switch(colour)
		{

		case CVehicleModelColor::EVehicleModelAudioColor_black:
			desc.colourSound = params->Bright.BlackSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_blue:
			desc.colourSound = params->Bright.BlueSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_brown:
			desc.colourSound = params->Bright.BrownSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_beige:
			desc.colourSound = params->Bright.BeigeSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_graphite:
			desc.colourSound = params->Bright.GraphiteSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_green:
			desc.colourSound = params->Bright.GreenSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_grey:
			desc.colourSound = params->Bright.GreySound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_orange:
			desc.colourSound = params->Bright.OrangeSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_pink:
			desc.colourSound = params->Bright.PinkSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_red:
			desc.colourSound = params->Bright.RedSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_silver:
			desc.colourSound = params->Bright.SilverSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_white:
			desc.colourSound = params->Bright.WhiteSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_yellow:
			desc.colourSound = params->Bright.YellowSound;
			break;
		default:
			desc.colourSound = g_NullSoundHash;
		}
		break;
	case CVehicleModelColor::EVehicleModelAudioPrefix_light:
		switch(colour)
		{

		case CVehicleModelColor::EVehicleModelAudioColor_black:
			desc.colourSound = params->Light.BlackSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_blue:
			desc.colourSound = params->Light.BlueSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_brown:
			desc.colourSound = params->Light.BrownSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_beige:
			desc.colourSound = params->Light.BeigeSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_graphite:
			desc.colourSound = params->Light.GraphiteSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_green:
			desc.colourSound = params->Light.GreenSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_grey:
			desc.colourSound = params->Light.GreySound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_orange:
			desc.colourSound = params->Light.OrangeSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_pink:
			desc.colourSound = params->Light.PinkSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_red:
			desc.colourSound = params->Light.RedSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_silver:
			desc.colourSound = params->Light.SilverSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_white:
			desc.colourSound = params->Light.WhiteSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_yellow:
			desc.colourSound = params->Light.YellowSound;
			break;
		default:
			desc.colourSound = g_NullSoundHash;
		}
		break;
	case CVehicleModelColor::EVehicleModelAudioPrefix_dark:
		switch(colour)
		{

		case CVehicleModelColor::EVehicleModelAudioColor_black:
			desc.colourSound = params->Dark.BlackSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_blue:
			desc.colourSound = params->Dark.BlueSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_brown:
			desc.colourSound = params->Dark.BrownSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_beige:
			desc.colourSound = params->Dark.BeigeSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_graphite:
			desc.colourSound = params->Dark.GraphiteSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_green:
			desc.colourSound = params->Dark.GreenSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_grey:
			desc.colourSound = params->Dark.GreySound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_orange:
			desc.colourSound = params->Dark.OrangeSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_pink:
			desc.colourSound = params->Dark.PinkSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_red:
			desc.colourSound = params->Dark.RedSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_silver:
			desc.colourSound = params->Dark.SilverSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_white:
			desc.colourSound = params->Dark.WhiteSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_yellow:
			desc.colourSound = params->Dark.YellowSound;
			break;
		default:
			desc.colourSound = g_NullSoundHash;
		}
		break;
	default:
		switch(colour)
		{

		case CVehicleModelColor::EVehicleModelAudioColor_black:
			desc.colourSound = params->NoPrefix.BlackSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_blue:
			desc.colourSound = params->NoPrefix.BlueSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_brown:
			desc.colourSound = params->NoPrefix.BrownSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_beige:
			desc.colourSound = params->NoPrefix.BeigeSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_graphite:
			desc.colourSound = params->NoPrefix.GraphiteSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_green:
			desc.colourSound = params->NoPrefix.GreenSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_grey:
			desc.colourSound = params->NoPrefix.GreySound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_orange:
			desc.colourSound = params->NoPrefix.OrangeSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_pink:
			desc.colourSound = params->NoPrefix.PinkSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_red:
			desc.colourSound = params->NoPrefix.RedSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_silver:
			desc.colourSound = params->NoPrefix.SilverSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_white:
			desc.colourSound = params->NoPrefix.WhiteSound;
			break;
		case CVehicleModelColor::EVehicleModelAudioColor_yellow:
			desc.colourSound = params->NoPrefix.YellowSound;
			break;
		default:
			desc.colourSound = g_NullSoundHash;
		}
	}
}

void audPoliceScanner::DescribeVehicle(CVehicle *vehicle, audVehicleDescription &desc, const u32 voiceNum)
{
	naAssertf(vehicle, "In DescribeVehicle a null CVehicle * has been passed in");
	if(!vehicle || !CVehicleModelInfo::GetVehicleColours() || !vehicle->GetVehicleAudioEntity())
		return;

	const ScannerVehicleParams* scannerParams = audNorthAudioEngine::GetObject<ScannerVehicleParams>(vehicle->GetVehicleAudioEntity()->GetScannerVehicleSettingsHash());
	if(!scannerParams)
		return;

	bool isBikeOrCar = vehicle->GetVehicleType() == VEHICLE_TYPE_CAR || vehicle->GetVehicleType() == VEHICLE_TYPE_BIKE;
	bool shouldBlockColorReporting = AUD_GET_TRISTATE_VALUE(scannerParams->Flags, FLAG_ID_SCANNERVEHICLEPARAMS_BLOCKCOLORREPORTING) == AUD_TRISTATE_TRUE;

	if(isBikeOrCar && !shouldBlockColorReporting)
	{
		if(voiceNum==0)
			CVehicleModelInfo::GetVehicleColours()->GetAudioDataForColour(vehicle->GetBodyColour1(), desc.prefixSound, desc.colourSound);
		else
		{
			u8 prefix, colour;
			CVehicleModelInfo::GetVehicleColours()->GetAudioIndexesForColour(vehicle->GetBodyColour1(), prefix, colour);
			GetVechicleColorSoundHashes(voiceNum, prefix, colour, desc);
		}
	}
	
#if __BANK
	if(g_PrintCarColour)
	{
#if ENABLE_VEHICLECOLOURTEXT
		const char *colourText = CVehicleModelInfo::GetVehicleColours()->GetVehicleColourText(vehicle->GetBodyColour1());
#else
		const char *colourText = "";
#endif
		naDisplayf("Car colour: %u [%s, %s]", vehicle->GetBodyColour1(), 
			vehicle->GetVehicleModelInfo() ? vehicle->GetVehicleModelInfo()->GetGameName() : "", colourText);
	}
#endif
	desc.isVehicleKnown = vehicle->m_nVehicleFlags.bWanted;
	if(vehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
	{
		const audBoatAudioEntity *const boatEntity = static_cast<audBoatAudioEntity*>(vehicle->GetVehicleAudioEntity());
		if(voiceNum==0)
		{
			desc.modelSound = boatEntity->GetScannerModelHash();
			desc.manufacturerSound = boatEntity->GetScannerManufacturerHash();
			desc.category = boatEntity->GetScannerCategoryHash();
		}
		else
		{
			if(scannerParams && voiceNum < scannerParams->numVoices)
			{
				u32 index = voiceNum - 1;
				desc.modelSound = scannerParams->Voice[index].ModelSound;
				desc.manufacturerSound = scannerParams->Voice[index].ManufacturerSound;
				desc.category = scannerParams->Voice[index].CategorySound;
			}
		}
	}
	else
	{
		if(vehicle->GetVehicleAudioEntity()->IsDamageModel() && isBikeOrCar)
		{
			desc.colourSound = g_AudioScannerManager.GetScannerHashForVoice("POLICE_SCANNER_COLOUR_FUCKED", voiceNum);
			desc.prefixSound = 0;
		}

		if(voiceNum==0)
		{
			desc.modelSound = vehicle->GetVehicleAudioEntity()->GetCarModelSoundHash();
			desc.manufacturerSound = vehicle->GetVehicleAudioEntity()->GetCarMakeSoundHash();
			desc.category = vehicle->GetVehicleAudioEntity()->GetCarCategorySoundHash();
		}
		else
		{
			if(scannerParams && voiceNum <= scannerParams->numVoices)
			{
				u32 index = voiceNum - 1;
				desc.modelSound = scannerParams->Voice[index].ModelSound;
				desc.manufacturerSound = scannerParams->Voice[index].ManufacturerSound;
				desc.category = scannerParams->Voice[index].CategorySound;
				if(scannerParams->Voice[index].ScannerColorOverride != 0 && scannerParams->Voice[index].ScannerColorOverride != g_NullSoundHash)
				{
					desc.colourSound = scannerParams->Voice[index].ScannerColorOverride;
				}
			}
		}
	}
	
	desc.audioEntity = vehicle->GetVehicleAudioEntity();

	if(CVehicle::IsTaxiModelId(vehicle->GetModelId()))
	{
		desc.category = 0;
	}

    CVehicleModelInfo* vmi = vehicle->GetVehicleModelInfo();
    Assert(vmi);

	if(vmi && vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EMERGENCY_SERVICE))
	{
		desc.colourSound = 0;
		desc.manufacturerSound = 0;
		desc.prefixSound = 0;
		desc.category = 0;
	}

	if(vehicle->GetLiveryId() != -1 || !isBikeOrCar)
	{
		desc.colourSound = 0;
		desc.prefixSound = 0;
	}

	desc.isMotorbike = (vehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || vehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || vehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE);
}

void audPoliceScanner::ChooseInstructionAndCrimeSound(ScannerCrimeReport *metadata, u32 &instruction, u32 &crime)
{
	Assertf(metadata, "NULL metadata passed into ChooseInstructionAndCrimeSound()");
	if(Verifyf(metadata->numCrimeSets > 0, "Found zero crimesets in ScannerCrimeReport object."))
		crime = ChooseWeightedVariation((audWeightedVariation*)&(metadata->CrimeSet[0]), metadata->numCrimeSets);
	Assertf(metadata->GenericInstructionSndRef != g_NullSoundHash, "Found ScannerCrimeReport object with null GenericInstructionSndRef: %s", 
		audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(metadata->NameTableOffset));

	CPed* localPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if(!localPlayer)
		instruction = metadata->GenericInstructionSndRef;
	else
	{
		Vector3 playerPos = VEC3V_TO_VECTOR3(localPlayer->GetMatrix().d());
		CPed::Pool *pPedPool = CPed::GetPool();
		s32 i=pPedPool->GetSize();
		while(i--)
		{
			CPed *pPed = pPedPool->GetSlot(i);

			if (pPed && (pPed->GetPedType() == PEDTYPE_COP || pPed->GetPedType() == PEDTYPE_SWAT) && 
				VEC3V_TO_VECTOR3(pPed->GetMatrix().d()).Dist2(playerPos) < g_MinCopDistSqForCopInstSet)
			{
				instruction = metadata->PoliceAroundInstructionSndRef != g_NullSoundHash ? metadata->PoliceAroundInstructionSndRef : metadata->GenericInstructionSndRef;
				return;
			}
		}

		const CPed* nearestPed = CPedGeometryAnalyser::GetNearestPed(playerPos, localPlayer);
		if(nearestPed && VEC3V_TO_VECTOR3(nearestPed->GetMatrix().d()).Dist2(playerPos) < g_MinPedDistSqForPedInstSet)
		{
			instruction = metadata->PedsAroundInstructionSndRef != g_NullSoundHash ? metadata->PedsAroundInstructionSndRef : metadata->GenericInstructionSndRef;
			return;
		}
	}
	instruction = metadata->GenericInstructionSndRef;
}

u32 audPoliceScanner::ChooseWeightedVariation(audWeightedVariation *items, u32 numItems)
{
	audWeightedVariation item;
	f32 weightSum = 0.0f;
	for(u32 i = 0 ; i < numItems; i++)
	{
		// local copy to work around f32 misalignment
		sysMemCpy(&item ,&items[i], sizeof(item));
		weightSum += item.weight;
	}
	f32 r = audEngineUtil::GetRandomNumberInRange(0.0f, weightSum);
	
	for(u32 i = 0 ; i < numItems; i++)
	{
		// local copy to work around f32 misalignment
		sysMemCpy(&item ,&items[i], sizeof(item));
		r -= item.weight;
		if(r <= 0.0f)
		{
			return item.item;
		}
	}
	naErrorf("Failed to choose a weighted variation");
	return 0;
}

const u32 g_RandomPoliceCrimes[] = {

	ATPARTIALSTRINGHASH("CRIME_CAR_SET_ON_FIRE", 0xBEEF7C),
	ATPARTIALSTRINGHASH("CRIME_COP_SET_ON_FIRE", 0xAFB906A9),
	ATPARTIALSTRINGHASH("CRIME_DAMAGE_TO_PROPERTY", 0xA1AEDA12),
	ATPARTIALSTRINGHASH("CRIME_DISTURBANCE", 0x908F6D41),
	ATPARTIALSTRINGHASH("CRIME_DRIVE_AGAINST_TRAFFIC", 0x2552F9EB),
	ATPARTIALSTRINGHASH("CRIME_DRIVEBY", 0xB42C2F96),
	ATPARTIALSTRINGHASH("CRIME_DRUG_DEAL", 0xC78E4073),
	ATPARTIALSTRINGHASH("CRIME_FLEEING_CRIME_SCENE", 0xC266F7EC),
	ATPARTIALSTRINGHASH("CRIME_GANG_ACTIVITY", 0xC05CE519),
	ATPARTIALSTRINGHASH("CRIME_GUN_SPREE", 0xC250E2A5),
	ATPARTIALSTRINGHASH("CRIME_HIT_COP", 0x9BFE4239),
	ATPARTIALSTRINGHASH("CRIME_HIT_PED", 0x3F3B898A),
	ATPARTIALSTRINGHASH("CRIME_PED_SET_ON_FIRE", 0x14C484E3),
	ATPARTIALSTRINGHASH("CRIME_POSSESSION_GUN", 0xE6362A4E),
	ATPARTIALSTRINGHASH("CRIME_RECKLESS_DRIVING", 0xC532F032),
	ATPARTIALSTRINGHASH("CRIME_RIDING_BIKE_WITHOUT_HELMET", 0xB83B657),
	ATPARTIALSTRINGHASH("CRIME_RUN_REDLIGHT", 0x9055005F),
	ATPARTIALSTRINGHASH("CRIME_RUNOVER_COP", 0xE1D3D9FC),
	ATPARTIALSTRINGHASH("CRIME_RUNOVER_PED", 0xB7169919),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_COP", 0xF326D5BD),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_GUN_FIRED", 0x23F16FE0),
	ATPARTIALSTRINGHASH("CRIME_SHOOT_PED", 0xEFAC9542),
	ATPARTIALSTRINGHASH("CRIME_SPEEDING", 0x8152C06C),
	ATPARTIALSTRINGHASH("CRIME_STAB_COP", 0x14C52012),
	ATPARTIALSTRINGHASH("CRIME_STAB_PED", 0x33867C7F),
	ATPARTIALSTRINGHASH("CRIME_STEAL_CAR", 0x5DA9475F),
	ATPARTIALSTRINGHASH("CRIME_STEAL_VEHICLE", 0x693084E),
	ATPARTIALSTRINGHASH("CRIME_TARGET_COP", 0x4F6A0C11)
};
const u32 g_NumRandomPoliceCrimes = sizeof(g_RandomPoliceCrimes)/sizeof(g_RandomPoliceCrimes[0]);

void audPoliceScanner::TriggerRandomMedicalReport(const Vector3 &emitterPos, naEnvironmentGroup *occlusionGroup)
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
		return;

	Vector3 pos = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
	// pick a random position away from the player
	f32 xOffset = audEngineUtil::GetRandomNumberInRange(400.f, 900.f);
	f32 yOffset = audEngineUtil::GetRandomNumberInRange(400.f, 900.f);
	pos.x = pos.x + (Sign(audEngineUtil::GetRandomNumberInRange(-1.f,1.f)) * xOffset);
	pos.y = pos.y + (Sign(audEngineUtil::GetRandomNumberInRange(-1.f,1.f)) * yOffset);

	audCrimeReport params;
	
	u32 voiceNum = g_AudioScannerManager.SelectScannerVoice();
	u32 ambulanceReportHash = g_AudioScannerManager.GetScannerHashForVoice("CRIME_AMBIENT_MEDICAL_EVENT", voiceNum);
	params.crime = audNorthAudioEngine::GetObject<ScannerCrimeReport>(ambulanceReportHash);
	params.priority = AUD_SCANNER_PRIO_AMBIENT;
	DescribePosition(pos,params.posDescription, voiceNum);
	// we only know have street info for the player
	params.posDescription.streetSound.SetInvalid();

	u32 idx = 0;
	u32 instructionSound;
	u32 crimeSound;

	ChooseInstructionAndCrimeSound(params.crime, instructionSound, crimeSound);

	// populate sentence structure
	audScannerSentence* sentence = g_AudioScannerManager.AllocateSentence(false,AUD_SCANNER_PRIO_AMBIENT, voiceNum);
	if(!sentence)
		return;

#if __BANK
	formatf(sentence->debugName, "Medical Report %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(params.crime->NameTableOffset));
#endif
	
	sentence->category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("AMBIENCE_HOSPITAL_SCANNER", 0x0c04c9957));
	sentence->position = emitterPos;
	sentence->occlusionGroup = occlusionGroup;
	sentence->tracker = NULL;
	sentence->isPositioned = true;
	sentence->backgroundSFXHash = ATSTRINGHASH("POSITIONED_NOISE_LOOP", 0x01083f52b);

	sentence->sentenceType = AUD_SCANNER_TRAIN;
	sentence->sentenceSubType = 0;
	sentence->timeRequested = g_AudioEngine.GetTimeInMilliseconds() + audEngineUtil::GetRandomNumberInRange(0,5000);
	sentence->priority = AUD_SCANNER_PRIO_CRITICAL;
	sentence->shouldDuckRadio = false;


	// INSTRUCTION
	sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
	sentence->phrases[idx].postDelay = 0;
	sentence->phrases[idx].sound = NULL;
#if __BANK
	if(g_PrintScannerTriggers)
		formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] instructinSound");
#endif
	sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(instructionSound);

	// CRIME
	sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
	sentence->phrases[idx].postDelay = 0;
	sentence->phrases[idx].sound = NULL;
#if __BANK
	if(g_PrintScannerTriggers)
		formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] crimeSound");
#endif
	sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(crimeSound);

	g_AudioScannerManager.PopulateSentenceWithPosDescription(params.posDescription, sentence, idx);

	sentence->numPhrases = idx;

	sentence->inUse = true;

}

void audPoliceScanner::ReportRandomCrime(const Vector3& pos)
{
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowScriptTriggerPoliceScanner))
		return;

	audCrimeReport params;

	u32 voiceNum = g_AudioScannerManager.SelectScannerVoice();

	u32 crimeNameHash = 0;
	CVehicle *veh = FindPlayerVehicle();
	if(veh && veh->GetModelIndex() == MI_CAR_AMBULANCE)
	{
		//static const audStringHash ambulanceReport("CRIME_AMBIENT_MEDICAL_EVENT");
		crimeNameHash = g_AudioScannerManager.GetScannerHashForVoice("CRIME_AMBIENT_MEDICAL_EVENT", voiceNum);
	}
	else if(veh && veh->GetModelIndex() == MI_CAR_FIRETRUCK)
	{
		//static const audStringHash fireReport("CRIME_AMBIENT_FIRE");
		crimeNameHash = g_AudioScannerManager.GetScannerHashForVoice("CRIME_AMBIENT_FIRE", voiceNum);
	}
	else
	{
		crimeNameHash = g_AudioScannerManager.GetScannerHashForVoice(g_RandomPoliceCrimes[audEngineUtil::GetRandomNumberInRange(0,g_NumRandomPoliceCrimes)], voiceNum);
	}
	params.crimeId = audEngineUtil::GetRandomNumberInRange(0, MAX_CRIMES);
	params.subtype = params.crimeId;
	
	ScannerCrimeReport *metadata = audNorthAudioEngine::GetObject<ScannerCrimeReport>(crimeNameHash);
	if(metadata)
	{
		params.crime = metadata;
		params.priority = AUD_SCANNER_PRIO_AMBIENT;
		DescribePosition(pos,params.posDescription, voiceNum, true);
		// we only know have street info for the player
		params.posDescription.streetSound.SetInvalid();
		if(audEngineUtil::ResolveProbability(0.5f) BANK_ONLY(&& !g_DebuggingScanner))
		{
			ReportRandomDispatch(params);
		}
		else
		{
			ReportCrime(params, voiceNum);
		}
	}
}

void audPoliceScanner::TriggerVigilanteCrime(const u32 crimeId, const Vector3 &position)
{
	if(g_ScannerEnabled)
	{
		static const u32 crimeHashes[] = 
		{
			ATPARTIALSTRINGHASH("CRIME_FLEEING_CRIME_SCENE", 0xC266F7EC),
			ATPARTIALSTRINGHASH("CRIME_STEAL_CAR", 0x5DA9475F),
			ATPARTIALSTRINGHASH("CRIME_GUN_SPREE", 0xC250E2A5),
			ATPARTIALSTRINGHASH("CRIME_STEAL_CAR", 0x5DA9475F),
			ATPARTIALSTRINGHASH("CRIME_DRIVEBY", 0xB42C2F96),
			ATPARTIALSTRINGHASH("CRIME_DRUG_DEAL", 0xC78E4073),
			ATPARTIALSTRINGHASH("CRIME_GANG_ACTIVITY", 0xC05CE519)
		};

		u32 voiceNum = g_AudioScannerManager.SelectScannerVoice();
		audCrimeReport	params;
		params.crime = audNorthAudioEngine::GetObject<ScannerCrimeReport>(g_AudioScannerManager.GetScannerHashForVoice(crimeHashes[crimeId], voiceNum));
		params.crimeId = crimeId + 1000;
		params.delay = 0;
		params.priority = AUD_SCANNER_PRIO_CRITICAL;
		params.subtype = 0;
		params.duckRadio = true;
		DescribePosition(position, params.posDescription, voiceNum);

		ReportCrime(params, voiceNum);
	}
}

#if __BANK

void audPoliceScanner::DrawDebug()
{
	if(g_DrawScannerSpecificLocations)
	{
		m_specificLocationListHash = audNorthAudioEngine::GetObject<ScannerSpecificLocationList>(ATSTRINGHASH("SCANNER_SPECIFIC_LOCATIONS_LIST", 0x0ee60fff2));

		if(m_specificLocationListHash)
		{
			for(int i=0; i<m_specificLocationListHash->numLocations; i++)
			{
				ScannerSpecificLocation* location = audNorthAudioEngine::GetObject<ScannerSpecificLocation>(m_specificLocationListHash->Location[i].Ref);
				if(location)
				{
					Vec3V pos(location->Position.x, location->Position.y, location->Position.z);
					grcDebugDraw::Sphere(pos,
						location->Radius,
						Color32(1.0f,0.0f,0.0f,1.0f), 
						true);

					grcDebugDraw::Text(pos, Color32(1.0f,1.0f,0.0f,1.0f), audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(location->NameTableOffset));
				}
			}
		}
		
	}
	
}

#endif//__BANK

#endif // NA_POLICESCANNER_ENABLED
