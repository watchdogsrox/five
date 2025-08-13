//  
// audio/vehiclereflectionsaudio.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLEREFLECTIONSAUDIOENTITY_H
#define AUD_VEHICLEREFLECTIONSAUDIOENTITY_H

#include "audiodefines.h"
#include "audioengine/widgets.h"
#include "audioentity.h"
#include "audiohardware/submix.h"
#include "audioengine/engine.h"
#include "audioengine/soundset.h"
#include "physics/WorldProbe/WorldProbe.h"

class audVehicleReflectionsEntity;
class audVehicleAudioEntity;
class audEmitterAudioEntity;
class audAmbientZone;

extern DURANGO_ONLY(ALIGNAS(16)) audVehicleReflectionsEntity g_ReflectionsAudioEntity DURANGO_ONLY();

// ----------------------------------------------------------------
// Entity to manage vehicle reflections
// ----------------------------------------------------------------
class audVehicleReflectionsEntity : public naAudioEntity
{
// Public methods
public:
	AUDIO_ENTITY_NAME(audVehicleReflectionsEntity);

	audVehicleReflectionsEntity();
	virtual ~audVehicleReflectionsEntity();

	void Init();
	static void InitClass();
	void Update();

	audVehicleAudioEntity* GetActiveVehicle() const { return m_ActiveVehicle; } 
	bool AddVehicleReflection(audVehicleAudioEntity* vehicle);
	void RemoveVehicleReflections(audVehicleAudioEntity* vehicle);
	bool AreInteriorReflectionsActive() const;

	void AddAmbientZoneReflectionPosition(const Vector3& pos, audAmbientZone* ambientZone);
	void RemoveAmbientZoneReflectionPosition(audAmbientZone* ambientZone);
	void AddDebugReflectionPosition(const Vector3& pos);
	void SetProximityWhooshesEnabled(bool enabled) { m_ProximityWhooshesEnabled = enabled; }

	void AttachToVehicle(audVehicleAudioEntity* vehicle);
	void DetachFromVehicle(audVehicleAudioEntity* vehicle);
	void DetachFromAll();
	void Shutdown();
	bool IsFocusVehicleInStuntTunnel() const { return m_IsFocusVehicleInStuntTunnel; }
	bool IsStuntTunnelMaterial(phMaterialMgr::Id materialID) const { return m_StuntTunnelMaterialIDs.Find(materialID) != -1; }

	inline Vector3::Return GetCurrentSourcePosition() { return m_CurrentSourcePos; }

#if __BANK
	void UpdateDebug();
	void AddWidgets(bkBank &bank);
#endif

private:
	void StopAllReflectionsVoices();
	void UpdateLoadedInteriors();
	CInteriorProxy* IsPointInOrNearDynamicReflectionsInterior(const Vector3& position);
	void UpdateInteriorReflections();
	void UpdateBonnetCamStereoEffect();
	void UpdateProximityWhooshes(Mat34V_In matrix, CEntity* entity);
	void UpdateVehicleReflections();
	void UpdateStuntTunnelReflections();
	void CalculateTunnelDirection(CInteriorInst* interiorInst, const Vector3& position, Vector3& direction);
	void UpdateInteriorReflectionsVoice(u32 i);
	void StopInteriorReflectionsVoice(u32 i);
	void RegisterStuntTunnelMaterial(const char* materialName);

// Public types
public:
	struct audReflectionPoint
	{
		enum ReflectionPointType
		{
			kReflectionPointType_AmbientZone,
			kReflectionPointType_Debug,
		};

		Vector3 position;
		audAmbientZone* parentZone;
		ReflectionPointType type;
	};

// Private types
private:
	enum {kMaxReflectionPoints = 20};
	enum {kMaxStuntTunnelProbes = 4};
	enum {kMaxVehicleReflections = AUD_NUM_REFLECTIONS_OUTPUT_SUBMIXES};

	enum eInteriorReflection
	{
		INTERIOR_REFLECTION_LEFT,
		INTERIOR_REFLECTION_RIGHT,
		INTERIOR_REFLECTION_MAX,
	};

// Private variables
private:
	atFixedArray<s32, EFFECT_ROUTE_VEHICLE_ENGINE_MAX - EFFECT_ROUTE_VEHICLE_ENGINE_MIN> m_AttachedEngineSubmixes;
	atFixedArray<s32, EFFECT_ROUTE_VEHICLE_EXHAUST_MAX - EFFECT_ROUTE_VEHICLE_EXHAUST_MIN> m_AttachedExhaustSubmixes;
	atFixedArray<audReflectionPoint, kMaxReflectionPoints> m_ReflectionsPoints;

	struct InteriorReflection
	{
		audCurve m_DistanceToFilterInputCurve;
		CEntity* m_ReflectionsEntity;
		ReflectionsSettings* m_ReflectionsSettings;
		audMixerSubmix* m_ReflectionsSubmix;
		audSound* m_ReflectionsVoice;
		WorldProbe::CShapeTestProbeDesc m_ProbeDesc;
		WorldProbe::CShapeTestResults m_ProbeResults;
		audSimpleSmootherDiscrete m_ProbeDistSmoother;
		Vector3 m_ProbeHitPos;
		Vector3 m_TunnelRight;
		Vector3 m_PrevTunnelDirection;
		s32 m_ReflectionsSubmixIndex;
		f32 m_ProbeDist;
		f32 m_ProbeHitTimer;
		f32 m_OscillatorSpeed;
		f32 m_OscillatorDist;
		bool m_FirstUpdate;
		bool m_ProbeHit;
	};

	struct ProximityWhoosh
	{		
		WorldProbe::CShapeTestProbeDesc m_ProbeDesc;
		WorldProbe::CShapeTestResults m_ProbeResults;
		audSimpleSmootherDiscrete m_ProbeDistSmoother;
		Vector3 m_ProbeHitPos;
		f32 m_ProbeDist;
		f32 m_SmoothedProbeDist;
		u32 m_LastWhooshTime;
		bool m_ProbeHit;
	};

	struct StuntTunnelProbe
	{
		WorldProbe::CShapeTestProbeDesc m_ProbeDesc;
		WorldProbe::CShapeTestResults m_ProbeResults;
		audSimpleSmootherDiscrete m_ProbeDistSmoother;
		Vector3 m_ProbeHitPos;
		f32 m_ProbeLastHitTimer;
		bool m_ProbeHit;
	};

	struct VehicleReflection
	{
		audCurve m_DistanceToFilterInputCurve;
		audVehicleAudioEntity* m_ReflectionsEntity;
		ReflectionsSettings* m_ReflectionsSettings;
		audMixerSubmix* m_ReflectionsSubmix;
		audSound* m_ReflectionsVoice;
		audSimpleSmootherDiscrete m_DistSmoother;
		s32 m_ReflectionsSubmixIndex;
		bool m_FirstUpdate;
	};

	enum eReflectionsType
	{
		AUD_REFLECTIONS_TYPE_STEREO_EFFECT,
		AUD_REFLECTIONS_TYPE_INTERIOR,
		AUD_REFLECTIONS_TYPE_MAX
	};

	atArray<CInteriorProxy*> m_CurrentlyLoadedInteriors;
	atRangeArray<VehicleReflection, kMaxVehicleReflections> m_VehicleReflections;
	atRangeArray<InteriorReflection, INTERIOR_REFLECTION_MAX> m_InteriorReflections;
	atRangeArray<ProximityWhoosh, 2> m_ProximityWhooshes;
	atRangeArray<StuntTunnelProbe, kMaxStuntTunnelProbes> m_StuntTunnelProbes;
	atArray<phMaterialMgr::Id> m_StuntTunnelMaterialIDs;
	Vector3 m_LastStuntTunnelWhooshPosition;

	audMixerSubmix* m_ReflectionsSourceSubmix;

	audVehicleAudioEntity* m_ActiveVehicle;
	eReflectionsType m_ReflectionsTypeLastFrame;
	Vector3 m_CurrentSourcePos;
	f32 m_OscillationTimer;
	u32 m_LastStuntTunnelWhooshSound;
	u32 m_LastBonnetCamUpdateTime;
	BANK_ONLY(f32 m_DotTest);
	bool m_HasPlayedInteriorEnter;
	bool m_HasPlayedInteriorExit;
	bool m_ProximityWhooshesEnabled;
	bool m_LastStuntTunnelWhooshWasEnter;
	bool m_IsFocusVehicleInStuntTunnel;

	static audCurve sm_ReflectionsVehicleSpeedToVolCurve;
	static audCurve sm_ReflectionsVehicleRevRatioToVolCurve;
	static audCurve sm_ReflectionsVehicleMassToVolCurve;
	static audCurve sm_StaticReflectionsRollOffCurve;
	static audSoundSet sm_CameraSwitchSoundSet;

#if __BANK
	Vector3	m_DebugWorldPos;
#endif
};


#endif
