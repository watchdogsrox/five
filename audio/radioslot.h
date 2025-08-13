// 
// audio/radioslot.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_RADIOSLOT_H
#define AUD_RADIOSLOT_H

#include "audiodefines.h"

#if NA_RADIO_ENABLED

#include "radioemitter.h"
#include "radiostation.h"
#include "streamslot.h"

const u32 g_MaxRadioSlots = 3;
const u32 g_NullRadioSlot = g_MaxRadioSlots;

const u32 g_NumRadioSlotEmitters = g_NumRadioStationShadowedEmitters;
const u32 g_NullRadioSlotEmitter = g_NumRadioSlotEmitters;

class audRadioSlot
{
public:
	static void InitClass();
	static void ShutdownClass();

	static void UpdateSlots(u32 timeInMs);

	static void SetPositionedPlayerVehicleRadioEmitterEnabled(bool enabled);
	static bool RequestRadioStationForEmitter(const audRadioStation *station, audRadioEmitter *emitter, u32 priority, s32 requestedPCMSourceId = -1);
	static bool IsStationActive(const audRadioStation *station);
	static void FindAndStopEmitter(audRadioEmitter *emitter, bool clearStation = true);
	static void SwapRadioEmitter(audRadioEmitter *oldEmitter, audRadioEmitter *newEmitter);
	static void FindAndRetuneEmitter(audRadioEmitter *emitter);
	static bool IsEmitterRetuning(audRadioEmitter *emitter);
	static bool IsEmitterActive(audRadioEmitter *emitter)
	{
		return (audRadioSlot::FindSlotByEmitter(emitter) != NULL);
	}
#if !__FINAL
	static void FindAndSkipForwardEmitter(audRadioEmitter *emitter, u32 timeToSkipMs);
	static void FindAndSkipForwardEmitterToTransition(audRadioEmitter *emitter, u32 timeToSkipMs);
#endif // !__FINAL
	static void SkipForwardSlots(u32 timeToSkipMs);
	static bool OnStopCallback(u32 userData);
	static bool HasStoppedCallback(u32 userData);

	static void ClearPendingRetunes()
	{
		audRadioStation::StopRetuneSounds();
	}

	audRadioSlot(u32 slotIndex);
	~audRadioSlot();
	const audRadioStation *GetStation()
	{
		return m_Station;
	}

	const audRadioStation *GetStationPendingRetune()
	{
		return m_RetuningStation;
	}
	u32 GetState()
	{
		return m_State;
	}
	bool HasStreamSlots()
	{
		return (m_StreamSlots[0] != NULL && m_StreamSlots[1] != NULL);
	}
	bool HasStreamSlot(u32 slot)
	{
		return (m_StreamSlots[slot] != NULL);
	}
	u32 GetPriority()
	{
		if(m_StreamSlots[0])
		{
			return m_StreamSlots[0]->GetPriority();
		}
		else if(m_StreamSlots[1])
		{
			return m_StreamSlots[1]->GetPriority();
		}
		else
		{
			return audStreamSlot::STREAM_PRIORITY_IDLE;
		}
	}

	bool IsFree(bool isRequestingPlayerRadio);
	bool IsRetuning();
	bool AllocateStreamSlot(u32 index, u32 priority = (u32)audStreamSlot::NUM_STREAM_PRIORITIES);

	void Update(u32 timeInMs);
	void SkipForward(u32 timeToSkipMs);
	void SkipForwardToTransition(u32 timeToSkipMs);


	BANK_ONLY(static void DrawDebug(audDebugDrawManager &drawMgr));

	f32 ComputeDistanceSqToNearestEmitter() const;
	f32 ComputeDistanceSqToNearestEmitter(CInteriorInst* pIntInst, s32 roomIdx) const;

	static audRadioSlot *FindSlotByStation(const audRadioStation *station);
	static audRadioSlot *FindSlotByEmitter(audRadioEmitter *emitter);

	void InvalidateStation();

protected:

	void FreeStreamSlot();

	BANK_ONLY(void DrawSlotDebug(audDebugDrawManager &drawMgr));

	static audRadioSlot *GetSlot(u32 index);
	
	
	static audRadioSlot *FindFreeSlot(u32 priority);
	static audRadioSlot *FindSlotToOverride(u32 priority);
	static audRadioSlot *FindSlotWithLeastEmitters(void);

	void StartRadioPlayback(const audRadioStation *station);
	bool ActionRadioRetune(u32 timeInMs);
	static s32 GetSlotIndex(audRadioSlot* slot);
	void CalculatePriority();
	bool AddEmitter(audRadioEmitter *emitter, bool shouldReplaceExisting = false, s32 requestedPCMChannel = -1, const audRadioStation* station = NULL);
	void RemoveEmitter(audRadioEmitter *emitter, bool clearRadioStation = true);
	void ClearEmitter(s32 index);
	void RetuneEmitter(audRadioEmitter *emitter);
	bool OverrideEmitters(audRadioEmitter *emitter, audRadioStation *newStation = NULL, s32 requestedPCMChannel = -1);
	void UpdateEmitters(void);
	s32 FindEmitterIndex(audRadioEmitter *emitter);
	bool HasEmitter(audRadioEmitter *emitter);
	audRadioEmitter *GetEmitter(u32 index)
	{
		return m_Emitters[index];
	}
	void Stop();	

	atRangeArray<audStreamSlot*, 2> m_StreamSlots;
	atRangeArray<audRadioEmitter *, g_NumRadioSlotEmitters> m_Emitters;
	atRangeArray<audEnvironmentGameMetric, g_NumRadioSlotEmitters> m_OcclusionMetrics;
	audRadioStation *m_Station;
	const audRadioStation *m_RetuningStation;
	u32 m_IdleStartTimeMs;
	u32 m_TimeToSkipForwardMs;
	u32 m_SlotIndex;
	u32 m_State;
	u32 m_RetuningEmitterIndex;
	u32 m_CurrentStationIndex;
	
	u32 m_Priority;
	
	bool m_ShouldSkipForward;
	bool m_WasUsingPositionedPlayerVehicleRadioReverb;

	static u32 sm_SoundBucketId;
	static audRadioStation *sm_RetuneStation;
	static audRadioSlot *sm_RadioSlots[g_MaxRadioSlots];
};
#endif // NA_RADIO_ENABLED
#endif // AUD_RADIOSLOT_H
