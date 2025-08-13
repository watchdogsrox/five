#ifndef AUD_DIRECTIONALAMBIENCE_H
#define AUD_DIRECTIONALAMBIENCE_H

#include "audio/environment/environment.h"

struct audDynamicWaveSlot;
class audAmbientZone;

// ----------------------------------------------------------------
// Helper class that plays a four panned sounds around the listener
// corresponding to some sort of data coming from the world. The associated game
// object allows you to set sounds, volume curves, etc. and then derived classes
// can implement the function to actually determine the sound volumes coming from
// a given direction
// ----------------------------------------------------------------
class audDirectionalAmbienceManager
{
public:
	audDirectionalAmbienceManager();
	virtual ~audDirectionalAmbienceManager() { Stop(); };

	virtual void Init(DirectionalAmbience* ambience);
	void Update(const float timeStepS, const s32 basePan, const s32* directionalPans, const f32* perDirectionChannelVolumes, const f32 heightAboveWorldBlanket);
	void Stop();
	void RegisterParentZone(audAmbientZone* ambientZone, f32 volumeScale);
	bool AreAnyParentZonesActive();

#if __BANK
	void DebugDraw();
	static void DebugDrawInputVariables(audAmbienceDirection direction);
#endif

	inline u32 GetSoundHash(u32 index) const					{ return m_directionalSoundHashes[index]; }
	inline u32 GetDynamicBankID(u32 index) const				{ return m_dynamicBankIDs[index]; }
	inline DirectionalAmbience* GetDirectionalAmbience() const	{ return m_directionalAmbience; }
	inline f32 GetLastParentZoneVolume() const					{ return m_parentZoneSmoother.GetLastValue(); }

	inline void ToggleMute()									{ m_muted = !m_muted; }
	inline void SetDynamicBankID(u32 index, u32 bankId)			{ m_dynamicBankIDs[index] = bankId; }
	inline void SetDynamicSlotType(u32 index, u32 slotID)		{ m_dynamicSlotTypes[index] = slotID; }

private:
	virtual void UpdateInternal() {};
	virtual f32 GetVolumeForDirection(audAmbienceDirection direction);
	void InitAmbienceCurve(audCurve* curve, u32 hash, u32 fallback);

protected:
	DirectionalAmbience* m_directionalAmbience;

private:
	struct DirectionalAmbienceParentZone 
	{
		audAmbientZone* ambientZone;
		f32 volumeScale;
	};

	enum { kNumDirections = 4 };

private:
	atArray<DirectionalAmbienceParentZone> m_ParentZones;
	atRangeArray<audSound*, kNumDirections> m_directionalSound;
	audSimpleSmoother m_parentZoneSmoother;
	audSimpleSmoother m_OutToSeaVolumeSmoother;
	atRangeArray<audSimpleSmoother, kNumDirections> m_directionalSoundSmoother;
	s32 m_CurrentUpdateDirection;

	atRangeArray<f32, AUD_AMB_DIR_MAX> m_CachedSectorVols;
	atRangeArray<u32, kNumDirections> m_directionalSoundHashes;
	atRangeArray<u32, kNumDirections> m_dynamicBankIDs;
	atRangeArray<u32, kNumDirections> m_dynamicSlotTypes;
	atRangeArray<audDynamicWaveSlot*, kNumDirections> m_DynamicWaveSlots;
	bool m_useSinglePannedSound;
	bool m_stopWhenRaining;
	bool m_muted;
	bool m_wasMuted;

	audCurve m_timeToVolumeCurve;
	audCurve m_occlusionToVolCurve;
	audCurve m_occlusionToCutoffCurve;
	audCurve m_heightToCutOffCurve;
	audCurve m_heightAboveBlanketToVolCurve;

	audCurve m_builtUpFactorToVolCurve;
	audCurve m_buildingDensityToVolCurve;
	audCurve m_treeDensityToVolCurve;
	audCurve m_waterFactorToVolCurve;
	audCurve m_highwayFactorToVolCurve;
	audCurve m_vehicleDensityToVolCurve;

#if __BANK
	atRangeArray<f32, kNumDirections> m_finalVolumes;
	atRangeArray<f32, kNumDirections> m_forcedVolumes;
	bool m_useForcedVolumes;
	bool m_debugDrawThisManager;
#endif
};

#endif // AUD_AMBIENTAUDIOENTITY_H
