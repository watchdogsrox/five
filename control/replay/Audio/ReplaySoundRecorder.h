#ifndef INC_REPLAYSOUNDRECORDER_H_
#define INC_REPLAYSOUNDRECORDER_H_

#include "control/replay/replay.h"

#if GTA_REPLAY
#include "audiosoundtypes/sound.h"

#if AUD_ENABLE_RECORDER_INTERFACE

class replaySoundRecorder : public audSoundRecorderInterface
{
public:
	void	OnDelete(audSound*);

	void	OnStop(audSound*);
	void	OnStopAndForget(audSound*);

	void	OnSetClientVariable(audSound*, f32);
	void	OnSetClientVariable(audSound*, u32);

	void	OnSetRequestedVolume(audSound*, const float);

	void	OnSetRequestedPostSubmixVolumeAttenuation(audSound*, const f32);
	void	OnSetRequestedVolumeCurveScale(audSound*, const f32);
	void	OnSetRequestedPitch(audSound*, const s32);
	void	OnSetRequestedFrequencyRatio(audSound*, const f32);
	void	OnSetRequestedPan(audSound*, const s32);
	void	OnSetRequestedPosition(audSound*, Vec3V_In);

	void	OnSetRequestedOrientation(audSound*, QuatV_In);
	void	OnSetRequestedOrientation(audSound*, audCompressedQuat);
	void	OnSetRequestedOrientation(audSound*, Mat34V_In);
	void	OnSetRequestedSourceEffectMix(audSound*, const f32, const f32);
	void	OnSetRequestedShouldAttenuateOverDistance(audSound*, TristateValue);
	void	OnSetRequestedQuadSpeakerLevels(audSound*, const f32*);
	void	OnSetRequestedDopplerFactor(audSound*, const f32);
	void	OnSetRequestedVirtualisationScoreOffset(audSound*, const u32);
	void	OnSetRequestedSpeakerMask(audSound*, const u8);
	void	OnSetRequestedListenerMask(audSound*, const u8);
	void	OnSetRequestedEnvironmentalLoudness(audSound*, const u8);
	void	OnSetRequestedEnvironmentalLoudnessFloat(audSound*, const f32);
	void	OnSetRequestedLPFCutoff(audSound*, const u32);
	void	OnSetRequestedLPFCutoff(audSound*, const f32);
	void	OnSetRequestedHPFCutoff(audSound*, const u32);
	void	OnSetRequestedHPFCutoff(audSound*, const f32);
	void	OnSetRequestedFrequencySmoothingRate(audSound*, const f32);
};

#endif // AUD_ENABLE_RECORDER_INTERFACE
#endif // GTA_REPLAY

#endif // INC_REPLAYSOUNDRECORDER_H_