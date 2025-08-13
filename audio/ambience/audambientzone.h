#ifndef AUD_AMBIENTZONE_H
#define AUD_AMBIENTZONE_H

// Rage Headers
#include "vector/vector3.h"
#include "fwutil/QuadTree.h"

// Framework Headers
#include "fwmaths/rect.h"

// Game Headers
#include "ambientaudioentity.h"
#include "audio/waveslotmanager.h"
#include "audio/ambience/auddirectionalambience.h"
#include "audio/speechmanager.h"
#include "audio/gameobjects.h"

const u32 g_MaxPlayingRules = 16;

#define LOUD_SOUND_REMEMBER_TIME (20000)

namespace rage
{
	struct AmbientRule;
	struct AmbientZone;
	struct AmbientZoneList;
	struct AmbientSlotMap;
	struct AmbientBankMap;
	class audSound;
	class Color32;
}

struct audDLCBankMap
{
	const AmbientBankMap* bankMap;
	u32 chunkNameHash;
};

struct audPlayingRuleState
{
	audPlayingRuleState() : 
		sound(NULL)
	  , rule(NULL)
	  , speechSlot(NULL)
	  , environmentGroup(NULL)
	  , speechSlotID(-1)
	{}

	Vec3V				position;
	audSound*			sound;
	AmbientRule*		rule;
	audWaveSlot*		speechSlot;
	naEnvironmentGroup* environmentGroup;
	f32					minDistance2;
	f32					maxDistance2;
	u16					minTimeMinutes;
	u16					maxTimeMinutes;
	s32					speechSlotID;
	bool				stopOnLoudSound; 
	bool				triggerOnLoudSound; 
	bool				stopWhenRaining; 
	bool				followListener;
	rage::u8			spawnHeight; 

	BANK_ONLY(const char *name);
};

class audAmbientZone
{
public:
	audAmbientZone() : 
		  m_NumRules(0)
		, m_BuiltUpFactor(0.f)
		, m_ZoneData(NULL)
		, m_Active(false)
		, m_InteriorProxy(NULL)
		, m_RoomIdx(INTLOC_ROOMINDEX_LIMBO)
		, m_ZoneNameHash(0u)
	{}

	~audAmbientZone()
	{
		SetActive(false);
		m_Rules.Reset();
	}

	void Init(AmbientZone *zone, u32 zoneNameHash, bool isInteriorLinkedZone, f32 masterRotationAngle = 0.0f);
	void Update(const u32 timeInMs, Vec3V_In listenerPos, const bool isRaining, const f32 loudSoundExclusionRadius, const u16 hours);
	f32 GetDistanceFromPos( const Vector3& inputPosition ) const;
	u32 GetNumInstancesPlaying(AmbientRule* rule) const;
	void StopAllSounds(bool forceStop = true);
	void FreeWaveSlots();
	void CalculateDynamicBankUsage();
	void UpdateDynamicWaveSlots(const u16 gameTimeMinutes, bool isRaining, Vec3V_In listenerPos);
	void SetActive(bool active);
	void GetPedDensityValues(f32& minDensity, f32& maxDensity) const;
	void ComputeActivationZoneCenter();
	void ComputePositioningZoneCenter();
	void CalculatePositioningLine();
	void ComputeShoreLines();

	Vec3V_Out ComputeClosestPointToPosition(Vec3V_In position) const;
	Vec3V_Out ComputeClosestPointOnActivationZoneBoundary(Vec3V_In position) const;
	Vec3V_Out ComputeClosestPointOnPositioningZoneBoundary(Vec3V_In position) const;
	Vec3V_Out ComputeClosestPointOnBoundary(Vec3V_In position, Vec3V_In size, Vec3V_In center, const f32 rotation) const;

	bool HasToOccludeRain();
	bool IsPointInActivationRange(Vec3V_In position) const;
	bool IsPointInPositioningZone(Vec3V_In position) const;
	bool IsPointInPositioningZoneFlat(Vec3V_In position) const;
	fwRect GetActivationZoneAABB() const;
	fwRect GetPositioningZoneAABB() const;

	inline bool IsSphere() const							{ return m_IsSphere;}
	inline bool IsInteriorLinkedZone() const				{ return m_IsInteriorLinkedZone; }
	inline bool IsActive() const							{ return m_Active; }
	inline const AmbientZone * GetZoneData() const			{ return m_ZoneData; }
	inline AmbientZone * GetZoneDataEditable() const		{ return m_ZoneData; }
	inline f32 GetBuiltUpFactor() const						{ return m_BuiltUpFactor; }
	inline f32 GetMasterRotationAngle() const				{ return m_MasterRotationAngle; }
	inline Vector3 GetInitialActivationZoneSize() const		{ return m_InitialActivationZone.Size; }
	inline Vector3 GetInitialPositioningZoneSize() const	{ return m_InitialPositioningZone.Size; }

	inline const Vector3& GetActivationZoneCenterCalculated()	const { return RCC_VECTOR3(m_ActivationZoneCenter); }
	inline const Vector3& GetPositioningZoneCenterCalculated()	const { return RCC_VECTOR3(m_PositioningZoneCenter); }

	inline const Vector3& GetActivationZoneCenter() const 	{ return m_ZoneData->ActivationZone.Centre; }
	inline const Vector3& GetPositioningZoneCenter() const	{ return m_ZoneData->PositioningZone.Centre; }
	inline const Vector3& GetActivationZoneSize() const 	{ return m_ZoneData->ActivationZone.Size; }
	inline const Vector3& GetPositioningZoneSize() const 	{ return m_ZoneData->PositioningZone.Size; }

	inline void ForceStateUpdate()							{ m_ForceStateUpdate = true; }
	inline void SetActivationZoneSize(Vector3& size) 		{ m_ZoneData->ActivationZone.Size = size; }
	inline void SetPositioningZoneSize(Vector3& size) 		{ m_ZoneData->PositioningZone.Size = size; }
	inline void SetActivationZoneCenter(Vector3& center) 	{ m_ZoneData->ActivationZone.Centre = center; }
	inline void SetPositioningZoneCenter(Vector3& center) 	{ m_ZoneData->PositioningZone.Centre = center; }

	inline u16 GetActivationZoneRotationAngle() const		{ return m_ZoneData->ActivationZone.RotationAngle; }
	inline u16 GetPositioningZoneRotationAngle() const		{ return m_ZoneData->PositioningZone.RotationAngle; }

	inline const Vector3& GetPositioningZonePostRotationOffset() const	{ return m_ZoneData->PositioningZone.PostRotationOffset; }
	inline const Vector3& GetActivationZonePostRotationOffset() const	{ return m_ZoneData->ActivationZone.PostRotationOffset; }

	inline void SetPositioningZonePostRotationOffset(Vector3& offset) const	{ m_ZoneData->PositioningZone.PostRotationOffset = offset; }
	inline void SetActivationZonePostRotationOffset(Vector3& offset) const	{ m_ZoneData->ActivationZone.PostRotationOffset = offset; }

	inline const Vector3& GetPositioningZoneSizeScale() const	{ return m_ZoneData->PositioningZone.SizeScale; }
	inline const Vector3& GetActivationZoneSizeScale() const	{ return m_ZoneData->ActivationZone.SizeScale; }

	inline void SetPositioningZoneSizeScale(Vector3& scale) const	{ m_ZoneData->PositioningZone.SizeScale = scale; }
	inline void SetActivationZoneSizeScale(Vector3& scale) const	{ m_ZoneData->ActivationZone.SizeScale = scale; }

	inline void SetActivationZoneRotationAngle(u16 newAngle)	{ m_ZoneData->ActivationZone.RotationAngle = newAngle; }
	inline void SetPositioningZoneRotationAngle(u16 newAngle)	{ m_ZoneData->PositioningZone.RotationAngle = newAngle; }

	inline const fwGeom::fwLine* GetPositioningLine() const	{ return &m_PositioningLine; }
	inline u32 GetNumRules() const							{ return m_NumRules; }
	inline AmbientRule* GetRule(u32 index) const			{ return m_Rules[index];}
	inline f32 GetPositioningArea() const					{ return (m_PositioningZoneSize.GetX() * m_PositioningZoneSize.GetY()).Getf(); }
	inline f32 GetPedDensityScalar() const					{ return m_ZoneData->PedDensityScalar; }
	inline bool HasCustomPedDensityTODCurve() const			{ return m_PedDensityTOD.IsValid(); }
	inline u32 GetEnvironmentRule() const					{ return m_ZoneData ? m_ZoneData->EnvironmentRule : 0; }
	inline u32 GetNameHash() const							{ return m_ZoneNameHash; }

	inline void SetInteriorInformation(CInteriorProxy* intProxy, s32 roomIdx) { m_InteriorProxy = intProxy; m_RoomIdx = roomIdx; }

	f32 ComputeZoneInfluenceRatioAtPoint(Vec3V_In point) const;
	u32 GetMaxSimultaneousRules() const;
	f32 GetHeightFactorInPositioningZone(Vec3V_In pos) const;

	static void InitClass();
	static void ResetMixerSceneApplyValues();
	static void UpdateMixerSceneApplyValues();
	static const AmbientBankMap* GetBankMap() { return sm_BankMap; }
	static const AmbientBankMap* GetDLCBankMap(u32 index) { return sm_DLCBankMaps[index].bankMap; }
	static atArray<audDLCBankMap>& GetDLCBankMapList() { return sm_DLCBankMaps; }
	static u32 GetNumDLCBankMaps() { return sm_DLCBankMaps.GetCount(); }
	static bool HasBeenInitialised() { return sm_HasBeenInitialised; }

#if __BANK
	void DebugDraw() const;
	void DebugDrawPlayingSounds() const;
	void DrawAnchorPoint(Vec3V_In zoneCentre, Vec3V_In zoneSize, f32 rotationDegrees) const;
	void DebugDrawRules(f32& playingRuleYOffset, f32& activeRuleYOffset) const;
	void DebugDrawBounds(Vec3V_In center, Vec3V_In size, const f32 rotationAngle, Color32 color, bool solid) const;
	inline const char *GetName() const { return m_Name; }
	static void DebugDrawMixerScenes();
#endif

	static audWaveSlotManager sm_WaveSlotManager;
	static s32 sm_NumStreamingRulesPlaying;
	static audCurve sm_DefaultPedDensityTODCurve;

	template<class T>
	static bool IsValidTime(T minTime, T maxTime, T currentTime)
	{
		bool validTimeToPlay = (currentTime < maxTime && currentTime >= minTime);

		// Check if this rule is overnight...
		if(maxTime < minTime)
		{
			validTimeToPlay = !(currentTime < minTime && currentTime >= maxTime);
		}

		return validTimeToPlay;
	}

private:
	void InitBounds();

	Vec3V_Out ComputeRandomPositionForRule(Vec3V_In listenerPosition, const AmbientRule *rule) const;
	Vec3V_Out ComputeClosestPointOnGround(Vec3V_In position) const;

	bool IsPointOverWater(Vec3V_In pos) const;
	bool IsValidRuleToPlay(const u32 ruleIndex, const u32 timeInMs, const u16 gameTimeMinutes, const bool isRaining) const;	
	inline bool IsValidDistance(f32 actualDistance, f32 minDistance, f32 maxDistance) const { return actualDistance >= minDistance && actualDistance < maxDistance; }
	bool AreAllConditionsValid(AmbientRule* rule, audSound* sound = NULL)  const;
	bool IsConditionValid(AmbientRule* rule, u32 conditionIndex, audSound* sound = NULL)  const;
	void FreeRuleStreamingSlot(u32 ruleIndex);
	
	static bool StartAmbientZoneMixerScene(u32 sceneName);
	static bool StopAmbientZoneMixerScene(u32 sceneName);
	static void SetMixerSceneApplyValue(u32 sceneName, f32 applyValue);

	static bool RegisterGlobalPlayingRule(AmbientRule* rule);
	static bool DeregisterGlobalPlayingRule(AmbientRule* rule);
	static bool IsGlobalRuleAllowedToPlay(AmbientRule* rule);

private:
	BANK_ONLY(const char *m_Name);

	Vec3V m_ActivationZoneCenter;
	Vec3V m_PositioningZoneCenter;
	Vec3V m_ActivationZoneSize;
	Vec3V m_PositioningZoneSize;

	AmbientZone::tActivationZone m_InitialActivationZone;
	AmbientZone::tPositioningZone m_InitialPositioningZone;

	audCurve m_PedDensityTOD;
	fwGeom::fwLine m_PositioningLine;

	atArray<audShoreLine*> m_ShoreLines;
	atArray<AmbientRule *> m_Rules;
	atArray<audDynamicWaveSlot *> m_WaveSlots;
	atArray<audPlayingRuleState> m_PlayingRules;

	AmbientZone *m_ZoneData;
	CInteriorProxy* m_InteriorProxy;
	TristateValue m_DefaultEnabledState;

	f32 m_BuiltUpFactor;
	f32 m_MasterRotationAngle;
	s32 m_RoomIdx;
	u32 m_ZoneNameHash;
	u8 m_NumRules;
	u8 m_NumShoreLines;
	bool m_IsSphere : 1;
	bool m_Active : 1;
	bool m_IsInteriorLinkedZone : 1;
	bool m_ForceStateUpdate : 1;
	bool m_IsReflectionZone : 1; 
	
	static AmbientSlotMap* sm_SlotMap;
	static const AmbientBankMap* sm_BankMap;
	static atArray<audDLCBankMap> sm_DLCBankMaps;
	static atRangeArray<AmbientRule*, 30> sm_GlobalPlayingRules;
	static bool sm_HasBeenInitialised;

	struct AmbientZoneMixerScene
	{
		AmbientZoneMixerScene()
		{
			applyValue = 0.0f;
			refCount = 0;
			mixerSceneName = 0U;
			mixerScene = NULL;
		}

		f32 applyValue;
		u32 refCount;
		u32 mixerSceneName;
		audScene* mixerScene;
	};

	enum {kMaxAmbientZoneMixerScenes = 10};
	static atRangeArray<AmbientZoneMixerScene, kMaxAmbientZoneMixerScenes> sm_AmbientZoneMixerScenes;
};


class audAmbientAudioEntity;

// Quadtree helper class.
class ZonesQuadTreeUpdateFunction : public fwQuadTreeNode::fwQuadTreeFn
{
public:
	ZonesQuadTreeUpdateFunction(atArray<audAmbientZone*>* activeZones, atArray<audAmbientZone*>* activeEnvironmentZone, u32 timeInMs, Vec3V_In zoneActivationPos, bool isRaining, f32 loudSoundExclusionRadius, u16 gameTimeMinutes):
	m_ActiveZones(activeZones)
		, m_ActiveEnvironmentZones(activeEnvironmentZone)
		, m_TimeInMs(timeInMs)
		, m_ZoneActivationPosition(zoneActivationPos)
		, m_IsRaining(isRaining)
		, m_gameTimeMinutes(gameTimeMinutes)
		, m_LoudSoundExclusionRadius(loudSoundExclusionRadius)
	{
	}

	void operator()(const Vector3& UNUSED_PARAM(posn), void* data)
	{
		audAmbientZone* zone = static_cast<audAmbientZone*>(data);

		if(zone)
		{
#if __BANK
			if(audAmbientAudioEntity::sm_DrawActiveQuadTreeNodes)
			{
				// Keep the height, overwrite the width/depth
				Vector3 renderSize = zone->GetZoneData()->ActivationZone.Size;
				renderSize.SetX(zone->GetActivationZoneAABB().GetWidth());
				renderSize.SetY(zone->GetActivationZoneAABB().GetHeight());

				const Color32 quadTreeColor = Color32(0,255,255);

				if(zone->GetZoneData()->Shape == kAmbientZoneCuboid ||
					zone->GetZoneData()->Shape == kAmbientZoneCuboidLineEmitter)
				{
					zone->DebugDrawBounds(RCC_VEC3V(zone->GetActivationZoneCenterCalculated()),
						RCC_VEC3V(renderSize), 
						0.0f, 
						quadTreeColor, 
						false);
				}
			}
#endif

			// This zone isn't active, but potentially should be because it passed our quad tree AABB test
			if(!zone->IsActive())
			{
				zone->Update(m_TimeInMs, m_ZoneActivationPosition, m_IsRaining, m_LoudSoundExclusionRadius, m_gameTimeMinutes);		

				// The zone just turned active! Remember it in our active zone list, so that we make sure we update it next frame. This ensures that it'll
				// get disabled correctly even if the activation point jumps off somewhere random
				if(zone->IsActive())
				{
					m_ActiveZones->PushAndGrow(zone);
					if(zone->GetEnvironmentRule() != 0)
					{
						m_ActiveEnvironmentZones->PushAndGrow(zone);
					}
				}
			}
		}

#if __BANK
		audAmbientAudioEntity::sm_numZonesReturnedByQuadTree++;
#endif
	}

private:
	atArray<audAmbientZone*>* m_ActiveZones;
	atArray<audAmbientZone*>* m_ActiveEnvironmentZones;
	bool m_IsRaining;
	f32 m_LoudSoundExclusionRadius;
	u32 m_TimeInMs;
	u16 m_gameTimeMinutes;
	Vec3V m_ZoneActivationPosition;
};

class DynamicBankListBuilderFn : public audSoundFactory::audSoundProcessHierarchyFn
{
public:
	atArray<u32> dynamicBankList;

	void operator()(u32 classID, const void* soundData)
	{
		u32 bankID = AUD_INVALID_BANK_ID;

		if(classID == SimpleSound::TYPE_ID)
		{
			const SimpleSound * sound = reinterpret_cast<const SimpleSound*>(soundData);
			bankID = g_AudioEngine.GetSoundManager().GetFactory().GetBankIndexFromMetadataRef(sound->WaveRef.BankName);
		}
		else if(classID == GranularSound::TYPE_ID)
		{
			const GranularSound * sound = reinterpret_cast<const GranularSound*>(soundData);
			bankID = g_AudioEngine.GetSoundManager().GetFactory().GetBankIndexFromMetadataRef(sound->Channel0.BankName);
		}

		if(bankID < AUD_INVALID_BANK_ID)
		{			
			// Bail out if we've already encountered this bank
			for(u32 existingBankLoop = 0; existingBankLoop < dynamicBankList.GetCount(); existingBankLoop++)
			{
				if(dynamicBankList[existingBankLoop] == bankID)
				{
					return;
				}
			}

			const char* bankName = g_AudioEngine.GetSoundManager().GetFactory().GetBankNameFromIndex(bankID);
			u32 bankHash = atStringHash(bankName);

			const AmbientBankMap* ambienceBankMap = audAmbientZone::GetBankMap();

			// We've got our bank - now verify that its one that can be dynamically loaded
			for(u32 bankMapLoop = 0; bankMapLoop < ambienceBankMap->numBankMaps; bankMapLoop++)
			{
				if(ambienceBankMap->BankMap[bankMapLoop].BankName == bankHash)
				{
					dynamicBankList.PushAndGrow(bankID);
					return;
				}
			}

			for(u32 dlcBankMapLoop = 0; dlcBankMapLoop < audAmbientZone::GetNumDLCBankMaps(); dlcBankMapLoop++)
			{
				ambienceBankMap = audAmbientZone::GetDLCBankMap(dlcBankMapLoop);

				for(u32 bankMapLoop = 0; bankMapLoop < ambienceBankMap->numBankMaps; bankMapLoop++)
				{
					if(ambienceBankMap->BankMap[bankMapLoop].BankName == bankHash)
					{
						dynamicBankList.PushAndGrow(bankID);
						return;
					}
				}
			}
		}
	}
};

#endif // AUD_AMBIENTZONE_H
