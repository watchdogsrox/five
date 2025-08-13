#include "ReplaySoundRecorder.h"

#if GTA_REPLAY
#if AUD_ENABLE_RECORDER_INTERFACE

void replaySoundRecorder::OnDelete(audSound* pSnd)
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::StopTrackingFx(CTrackedEventInfo<tTrackedSoundType>(pSnd));
	}
}


void replaySoundRecorder::OnStop(audSound* pSnd)
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketSoundStop>(
			CPacketSoundStop(false),
			CTrackedEventInfo<tTrackedSoundType>(pSnd));
	}
}


void replaySoundRecorder::OnStopAndForget(audSound* pSnd)
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketSoundStop>(
			CPacketSoundStop(true),
			CTrackedEventInfo<tTrackedSoundType>(pSnd));
	}
}

// The rest will stay unimplemented until needed
void replaySoundRecorder::OnSetClientVariable(audSound*, f32)
{

}
void replaySoundRecorder::OnSetClientVariable(audSound*, u32)
{

}
void replaySoundRecorder::OnSetRequestedVolume(audSound* pSnd, const float vol)
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketSoundUpdateVolume>(
			CPacketSoundUpdateVolume(vol),
			CTrackedEventInfo<tTrackedSoundType>(pSnd));
	}
}
void replaySoundRecorder::OnSetRequestedPostSubmixVolumeAttenuation(audSound*, const f32)
{

}
void replaySoundRecorder::OnSetRequestedVolumeCurveScale(audSound*, const f32)
{

}
void replaySoundRecorder::OnSetRequestedPitch(audSound* pSnd, const s32 pitch)
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketSoundUpdatePitch>(
			CPacketSoundUpdatePitch(pitch),
			CTrackedEventInfo<tTrackedSoundType>(pSnd));
	}
}
void replaySoundRecorder::OnSetRequestedFrequencyRatio(audSound*, const f32)
{

}
void replaySoundRecorder::OnSetRequestedPan(audSound*, const s32)
{

}
void replaySoundRecorder::OnSetRequestedPosition(audSound* pSnd, Vec3V_In position)
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketSoundUpdatePosition>(
			CPacketSoundUpdatePosition(position),
			CTrackedEventInfo<tTrackedSoundType>(pSnd));
	}
}
void replaySoundRecorder::OnSetRequestedOrientation(audSound*, QuatV_In)
{

}
void replaySoundRecorder::OnSetRequestedOrientation(audSound*, audCompressedQuat)
{

}
void replaySoundRecorder::OnSetRequestedOrientation(audSound*, Mat34V_In)
{

}
void replaySoundRecorder::OnSetRequestedSourceEffectMix(audSound*, const f32, const f32)
{

}
void replaySoundRecorder::OnSetRequestedShouldAttenuateOverDistance(audSound*, TristateValue)
{

}
void replaySoundRecorder::OnSetRequestedQuadSpeakerLevels(audSound*, const f32*)
{

}
void replaySoundRecorder::OnSetRequestedDopplerFactor(audSound*, const f32)
{

}
void replaySoundRecorder::OnSetRequestedVirtualisationScoreOffset(audSound*, const u32)
{

}
void replaySoundRecorder::OnSetRequestedSpeakerMask(audSound*, const u8)
{

}
void replaySoundRecorder::OnSetRequestedListenerMask(audSound*, const u8)
{

}
void replaySoundRecorder::OnSetRequestedEnvironmentalLoudness(audSound*, const u8)
{

}
void replaySoundRecorder::OnSetRequestedEnvironmentalLoudnessFloat(audSound*, const f32)
{

}
void replaySoundRecorder::OnSetRequestedLPFCutoff(audSound*, const u32)
{

}
void replaySoundRecorder::OnSetRequestedLPFCutoff(audSound*, const f32)
{

}
void replaySoundRecorder::OnSetRequestedHPFCutoff(audSound*, const u32)
{

}
void replaySoundRecorder::OnSetRequestedHPFCutoff(audSound*, const f32)
{

}
void replaySoundRecorder::OnSetRequestedFrequencySmoothingRate(audSound*, const f32)
{

}

#endif // AUD_ENABLE_RECORDER_INTERFACE
#endif // GTA_REPLAY