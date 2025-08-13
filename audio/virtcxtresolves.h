// 
// audio/virtcxtresolves.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VIRTCXTRESOLVES_H
#define AUD_VIRTCXTRESOLVES_H

#include "audio/speechaudioentity.h"
#include "game\Clock.h"
#include "Peds\ped.h"

namespace audVirtCxtResolves{

	//0:Generic 1:Morning 2:Evening
	SpeechContext * ResolveTimeOfDay(u8 numContexts, u32* speechContexts)
	{
		if(numContexts == 0)
			return NULL;
		if(numContexts < 3)
			return static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[0]));

		const int iEveningStart = 19;
		const int iMorningStart = 7;
		const int iMorningEnd = 11;

		int iHours = CClock::GetHour();
		if(iHours>iEveningStart || iHours<iMorningStart)
			return static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[2]));
		else if(iHours<iMorningEnd)
			return static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[1]));

		return static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[0]));
	}

	//0:High 1:Mid
	SpeechContext * ResolveSpeakerToughness(u8 numContexts, u32* speechContexts, CPed* speaker)
	{
		if(numContexts == 0 || !speaker || !speaker->GetSpeechAudioEntity())
			return NULL;
		if(numContexts < 2)
			return static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[0]));

		switch(speaker->GetSpeechAudioEntity()->GetPedToughness())
		{
		case kMaleBrave:
		case kMaleGang:
		case kCop:
			return static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[0]));
		default:
			return static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[1]));
		}
	}

	//<!-- 0:Male 1:Female 2:Generic-->
	SpeechContext * ResolveTargetGender(u8 numContexts, u32* speechContexts, CPed* speaker, CPed* replyingPed)
	{
		if(numContexts== 0 || !speaker || !speaker->GetSpeechAudioEntity())
			return NULL;
		if(numContexts < 3 || !replyingPed)
			return static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[2]));

		u32 ambientVoicehash = speaker->GetSpeechAudioEntity()->GetAmbientVoiceName();
		SpeechContext* context2 = static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[2]));
		bool hasGeneric = (s32)audSpeechSound::FindNumVariationsForVoiceAndContext(ambientVoicehash, context2 ? context2->ContextNamePHash : 0) > 0;
		if(replyingPed->IsMale())
		{
			SpeechContext* context0 = static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[0]));
			bool hasMaleVersion = (s32)audSpeechSound::FindNumVariationsForVoiceAndContext(ambientVoicehash, context0 ? context0->ContextNamePHash : 0) > 0;
			if(!hasGeneric)
				return context0;
			else if(!hasMaleVersion)
				return context2;
			else 
				return audEngineUtil::ResolveProbability(0.5f) ? context0 : context2;
		}
		else
		{
			SpeechContext* context1 = static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContexts[1]));
			bool hasFemaleVersion = (s32)audSpeechSound::FindNumVariationsForVoiceAndContext(ambientVoicehash, context1 ? context1->ContextNamePHash : 0) > 0;
			if(!hasGeneric)
				return context1;
			else if(!hasFemaleVersion)
				return context2;
			else 
				return audEngineUtil::ResolveProbability(0.5f) ? context1 : context2;
		}
	}
}

#endif //AUD_VIRTCXTRESOLVES_H