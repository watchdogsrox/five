//
// audio/audio_channel.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_AUDIO_CHANNEL_H
#define INC_AUDIO_CHANNEL_H

#include "diag/channel.h"
#include "audiohardware/channel.h"


RAGE_DECLARE_SUBCHANNEL(Audio,NorthAudio)
RAGE_DECLARE_SUBCHANNEL(Audio,SpeechEntity)
RAGE_DECLARE_SUBCHANNEL(Audio,Conversation)

#define naAssert(cond)						RAGE_ASSERT(Audio_NorthAudio,cond)
#define naAssertf(cond,fmt,...)				RAGE_ASSERTF(Audio_NorthAudio,cond,fmt,##__VA_ARGS__)
#define naVerifyf(cond,fmt,...)				RAGE_VERIFYF(Audio_NorthAudio,cond,fmt,##__VA_ARGS__)
#define naErrorf(fmt,...)					RAGE_ERRORF(Audio_NorthAudio,fmt,##__VA_ARGS__)
#define naCWarningf(cond,fmt,...)			RAGE_CONDLOGF(!(cond),Audio_NorthAudio,DIAG_SEVERITY_WARNING,fmt,##__VA_ARGS__)
#define naCErrorf(cond,fmt,...)				RAGE_CONDLOGF(!(cond),Audio_NorthAudio,DIAG_SEVERITY_ERROR,fmt,##__VA_ARGS__)
#define naWarningf(fmt,...)					RAGE_WARNINGF(Audio_NorthAudio,fmt,##__VA_ARGS__)
#define naDisplayf(fmt,...)					RAGE_DISPLAYF(Audio_NorthAudio,fmt,##__VA_ARGS__)
#define naDebugf1(fmt,...)					RAGE_DEBUGF1(Audio_NorthAudio,fmt,##__VA_ARGS__)
#define naDebugf2(fmt,...)					RAGE_DEBUGF2(Audio_NorthAudio,fmt,##__VA_ARGS__)
#define naDebugf3(fmt,...)					RAGE_DEBUGF3(Audio_NorthAudio,fmt,##__VA_ARGS__)
#define naLogf(severity,fmt,...)			RAGE_LOGF(Audio_NorthAudio,severity,fmt,##__VA_ARGS__)
#define naCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,Audio_NorthAudio,severity,fmt,##__VA_ARGS__)

#define naSpeechEntAssert(cond)									RAGE_ASSERT(Audio_SpeechEntity,cond)
#define naSpeechEntAssertf(speechEnt, cond,fmt,...)				RAGE_ASSERTF(Audio_SpeechEntity,cond,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntVerifyf(speechEnt, cond,fmt,...)				RAGE_VERIFYF(Audio_SpeechEntity,cond,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntFatalf(speechEnt, fmt,...)					RAGE_FATALF(Audio_SpeechEntity,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntErrorf(speechEnt, fmt,...)					RAGE_ERRORF(Audio_SpeechEntity,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntCWarningf(speechEnt, cond,fmt,...)			RAGE_CONDLOGF(!(cond),Audio_SpeechEntity,DIAG_SEVERITY_WARNING,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent()->GetParent() ? speechEnt->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntCErrorf(speechEnt, cond,fmt,...)				RAGE_CONDLOGF(!(cond),Audio_SpeechEntity,DIAG_SEVERITY_ERROR,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent()->GetParent() ? speechEnt->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntWarningf(speechEnt, fmt,...)					RAGE_WARNINGF(Audio_SpeechEntity,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntDisplayf(speechEnt, fmt,...)					RAGE_DISPLAYF(Audio_SpeechEntity,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntDebugf1(speechEnt, fmt,...)					RAGE_DEBUGF1(Audio_SpeechEntity,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntDebugf2(speechEnt, fmt,...)					RAGE_DEBUGF2(Audio_SpeechEntity,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntDebugf3(speechEnt, fmt,...)					RAGE_DEBUGF3(Audio_SpeechEntity,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntLogf(speechEnt, severity,fmt,...)			RAGE_LOGF(Audio_SpeechEntity,severity,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)
#define naSpeechEntCondLogf(speechEnt, cond,severity,fmt,...)	RAGE_CONDLOGF(cond,Audio_SpeechEntity,severity,"[%s] : [%u] " fmt,speechEnt && speechEnt->GetParent() ? speechEnt->GetParent()->GetModelName() : "NULL Parent",fwTimer::GetFrameCount(),##__VA_ARGS__)

#define naConvAssert(cond)						RAGE_ASSERT(Audio_Conversation,cond)
#define naConvAssertf(cond,fmt,...)				RAGE_ASSERTF(Audio_Conversation,cond,fmt,##__VA_ARGS__)
#define naConvVerifyf(cond,fmt,...)				RAGE_VERIFYF(Audio_Conversation,cond,fmt,##__VA_ARGS__)
#define naConvFatalf(fmt,...)					RAGE_FATALF(Audio_Conversation,fmt,##__VA_ARGS__)
#define naConvErrorf(fmt,...)					RAGE_ERRORF(Audio_Conversation,fmt,##__VA_ARGS__)
#define naConvCWarningf(cond,fmt,...)			RAGE_CONDLOGF(!(cond),Audio_Conversation,DIAG_SEVERITY_WARNING,fmt,##__VA_ARGS__)
#define naConvCErrorf(cond,fmt,...)				RAGE_CONDLOGF(!(cond),Audio_Conversation,DIAG_SEVERITY_ERROR,fmt,##__VA_ARGS__)
#define naConvWarningf(fmt,...)					RAGE_WARNINGF(Audio_Conversation,fmt,##__VA_ARGS__)
#define naConvDisplayf(fmt,...)					RAGE_DISPLAYF(Audio_Conversation,fmt,##__VA_ARGS__)
#define naConvDebugf1(fmt,...)					RAGE_DEBUGF1(Audio_Conversation,fmt,##__VA_ARGS__)
#define naConvDebugf2(fmt,...)					RAGE_DEBUGF2(Audio_Conversation,fmt,##__VA_ARGS__)
#define naConvDebugf3(fmt,...)					RAGE_DEBUGF3(Audio_Conversation,fmt,##__VA_ARGS__)
#define naConvLogf(severity,fmt,...)			RAGE_LOGF(Audio_Conversation,severity,fmt,##__VA_ARGS__)
#define naConvCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,Audio_Conversation,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(Audio,CutsceneAudio)

#define caAssert(cond)						RAGE_ASSERT(Audio_CutsceneAudio,cond)
#define caAssertf(cond,fmt,...)				RAGE_ASSERTF(Audio_CutsceneAudio,cond,fmt,##__VA_ARGS__)
#define caVerifyf(cond,fmt,...)				RAGE_VERIFYF(Audio_CutsceneAudio,cond,fmt,##__VA_ARGS__)
#define caErrorf(fmt,...)					RAGE_ERRORF(Audio_CutsceneAudio,fmt,##__VA_ARGS__)
#define caCWarningf(cond,fmt,...)			RAGE_CONDLOGF(!(cond),Audio_CutsceneAudio,DIAG_SEVERITY_WARNING,fmt,##__VA_ARGS__)
#define caCErrorf(cond,fmt,...)				RAGE_CONDLOGF(!(cond),Audio_CutsceneAudio,DIAG_SEVERITY_WARNING,fmt,##__VA_ARGS__)
#define caWarningf(fmt,...)					RAGE_WARNINGF(Audio_CutsceneAudio,fmt,##__VA_ARGS__)
#define caDisplayf(fmt,...)					RAGE_DISPLAYF(Audio_CutsceneAudio,fmt,##__VA_ARGS__)
#define caDebugf1(fmt,...)					RAGE_DEBUGF1(Audio_CutsceneAudio,fmt,##__VA_ARGS__)
#define caDebugf2(fmt,...)					RAGE_DEBUGF2(Audio_CutsceneAudio,fmt,##__VA_ARGS__)
#define caDebugf3(fmt,...)					RAGE_DEBUGF3(Audio_CutsceneAudio,fmt,##__VA_ARGS__)
#define caLogf(severity,fmt,...)			RAGE_LOGF(Audio_CutsceneAudio,severity,fmt,##__VA_ARGS__)
#define caCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,Audio_CutsceneAudio,severity,fmt,##__VA_ARGS__)

RAGE_DECLARE_SUBCHANNEL(Audio,NetworkAudio)

#define networkAudioAssert(cond)					RAGE_ASSERT(Audio_NetworkAudio,cond)
#define networkAudioAssertf(cond,fmt,...)			RAGE_ASSERTF(Audio_NetworkAudio,cond,fmt,##__VA_ARGS__)
#define networkAudioVerifyf(cond,fmt,...)			RAGE_VERIFYF(Audio_NetworkAudio,cond,fmt,##__VA_ARGS__)
#define networkAudioErrorf(fmt,...)					RAGE_ERRORF(Audio_NetworkAudio,fmt,##__VA_ARGS__)
#define networkAudioCWarningf(cond,fmt,...)			RAGE_CONDLOGF(!(cond),Audio_NetworkAudio,DIAG_SEVERITY_WARNING,fmt,##__VA_ARGS__)
#define networkAudioCErrorf(cond,fmt,...)			RAGE_CONDLOGF(!(cond),Audio_NetworkAudio,DIAG_SEVERITY_WARNING,fmt,##__VA_ARGS__)
#define networkAudioWarningf(fmt,...)				RAGE_WARNINGF(Audio_NetworkAudio,fmt,##__VA_ARGS__)
#define networkAudioDisplayf(fmt,...)				RAGE_DISPLAYF(Audio_NetworkAudio,fmt,##__VA_ARGS__)
#define networkAudioDebugf1(fmt,...)				RAGE_DEBUGF1(Audio_NetworkAudio,fmt,##__VA_ARGS__)
#define networkAudioDebugf2(fmt,...)				RAGE_DEBUGF2(Audio_NetworkAudio,fmt,##__VA_ARGS__)
#define networkAudioDebugf3(fmt,...)				RAGE_DEBUGF3(Audio_NetworkAudio,fmt,##__VA_ARGS__)
#define networkAudioLogf(severity,fmt,...)			RAGE_LOGF(Audio_NetworkAudio,severity,fmt,##__VA_ARGS__)
#define networkAudioCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,Audio_NetworkAudio,severity,fmt,##__VA_ARGS__)

#endif // INC_AUDIO_CHANNEL_H
