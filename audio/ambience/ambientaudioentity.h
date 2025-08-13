#ifndef AUD_AMBIENTAUDIOENTITY_H
#define AUD_AMBIENTAUDIOENTITY_H

#include "audio/audioentity.h"

// Rage headers
#include "atl/array.h" 
#include "spatialdata/aabb.h"
#include "system/criticalsection.h"
#include "audioengine/soundfactory.h"
#include "audioengine/soundset.h"

// Framework headers
#include "fwutil/idgen.h"
#include "fwutil/QuadTree.h"

// Game headers
#include "scene/RegdRefTypes.h"
#include "system/FileMgr.h"
#include "audio/streamslot.h"
#include "audio/waveslotmanager.h"
#include "audio/environment/environment.h"
#include "audio/ambience/auddirectionalambience.h"
#include "audio/dynamicmixer.h"
#include "audio/northaudioengine.h"

class audAmbientAudioEntity;
extern  DURANGO_ONLY(ALIGNAS(16))audAmbientAudioEntity g_AmbientAudioEntity DURANGO_ONLY();

// PURPOSE
//	Defines the max number of sounds requested by the effects system per frame
const u32 g_MaxRegisteredAudioEffects = 128;

namespace rage
{
	struct AmbientRule;
	struct AmbientZone;
	struct AmbientZoneList;
	struct AmbientSlotMap;
	struct AmbientBankMap;
	class ptxEffectInst;
	class bkBank;
	class audSound;
	class Color32;
}

class audShoreLine;
class audAmbientZone;
class naEnvironmentGroup;

struct audRegisteredAudioEffectRequest 
{
	Vector3			pos;
	RegdEnt			entity;
	fwUniqueObjId	uniqueId;
	u32				soundNameHash;
	f32				vol;
	s32				pitch;
	bool			processed;
};

struct audRegisteredAudioEffect
{
	audSound*			sound;
	naEnvironmentGroup*	occlusionGroup;
	fwUniqueObjId		uniqueId;
	ptxEffectInst*		inst;
	bool				inUse;
	bool				requestedThisFrame; 
	bool				needsPlay;
};

struct audAmbientZoneEnabledState
{
	u32 zoneNameHash;
	bool enabled;
};

#if GTA_REPLAY
struct audAmbientZoneReplayState
{
	u32 zoneNameHash;
	u8 nonPersistantState : 2;
	bool persistentFlagChanged : 1;	
};

struct audReplayAlarmState
{
	u32 alarmNameHash;
	u32 timeAlive;
};
#endif

class audAmbientAudioEntity : public naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audAmbientAudioEntity);

	audAmbientAudioEntity();
	~audAmbientAudioEntity();

	void Init();
	void Shutdown();

	virtual void QuerySpeechVoiceAndContextFromField(const u32 field, u32& voiceNameHash, u32& contextNamePartialHash);
	virtual void UpdateSound(audSound *sound, audRequestedSettings *reqSets, u32 timeInMs);

	void RegisterEffectAudio(const u32 soundNameHash, const fwUniqueObjId uniqueId, const Vector3 &pos, CEntity *entity, const f32 vol = 0.f, const s32 pitch = 0);
	void IncrementAudioEffectWriteIndex();
	void Update(const u32 timeInMs);
	void PostUpdate();

	inline u32 GetNumZones() const									{ return m_Zones.GetCount(); }
	inline u32 GetNumActiveZones() const							{ return m_ActiveZones.GetCount(); }
	inline u32 GetNumInteriorZones() const							{ return m_InteriorZones.GetCount(); }
	inline const audAmbientZone* GetActiveZone(u32 index) const		{ return m_ActiveZones[index]; }
	inline f32 GetWorldHeightFactorAtListenerPosition() const		{ return m_WorldHeightFactor; }
	inline f32 GetUnderwaterCreakFactorAtListenerPosition() const	{ return m_UnderwaterCreakFactor; }

#if GTA_REPLAY
	const atArray<audAmbientZoneReplayState>& GetAmbientZoneReplayStates() { return m_ReplayAmbientZoneStates; }
	void ClearAmbientZoneReplayStates() { return m_ReplayAmbientZoneStates.clear(); }
	void AddAmbientZoneReplayState(audAmbientZoneReplayState zoneState);
	bool IsZoneEnabledInReplay(u32 zoneNameHash, TristateValue defaultEnabledState);
	void UpdateAmbientZoneReplayStates();
#endif

	const audAmbientZone* GetCurrentWindElevationZone() const {return m_CurrentWindZone;};
	const audAmbientZone* GetZone(u32 index);
	const audAmbientZone* GetInteriorZone(u32 index);
	bool IsZoneActive(u32 zoneNameHash) const;
	void AddDirectionalAmbience(DirectionalAmbience* directionalAmbience, audAmbientZone* parentZone, f32 volumeScale);
	void StartSpawnEffect(ptxEffectInst *inst);
	void StopSpawnEffect(ptxEffectInst *inst);
	void RecycleSpawnEffect(ptxEffectInst *inst);
	void LoadShorelines(u32 shorelineListName, u32 linkShoreline);
	void LoadNewDLCShorelines(u32 shorelineListName, u32 linkShoreline);
	void ForcePedPanic();
	bool IsPointOverWater(const Vector3& pos);
	void GetAllShorelinesIntersecting(const fwRect& bb, atArray<audShoreLine*>& shorelines);
	bool TriggerStreamedAmbientOneShot(const u32 soundName, audSoundInitParams* initParams);
	void StopAllStreamedAmbientSounds();
	bool IsRuleStreamingAllowed() const;
	void CheckAndAddZones();
	void DestroyAllZonesAndShorelines();
	void AddCoreZones();
	void ForceOceanShorelineUpdateFromNearestRoad();

	void OverrideUnderWaterStream(const char* streamName,bool override);
	void OverrideUnderWaterStream(u32 streamHashName,bool override);
	void SetVariableOnUnderWaterStream(const char* variableName, f32 variableValue);
	void SetVariableOnUnderWaterStream(u32 variableNameHash, f32 variableValue);
	bool IsAlarmActive(AlarmSettings* alarmSettings);
	void UpdateShoreLineWaterHeights();
	bool GetLastTimeVoiceContextWasUsedForAmbientWalla(u32 voice, u32 contextName, u32& timeInMs);

	bool SetScriptDesiredPedDensity(f32 desiredDensity, f32 desiredDensityApplyFactor, s32 scriptThreadID, bool interior);
	void CancelScriptPedDensityChanges(s32 scriptThreadID);

	bool SetScriptWallaSoundSet(u32 soundSetName, s32 scriptThreadID, bool interior);
	void CancelScriptWallaSoundSet(s32 scriptThreadID);

	bool IsPlayerInTheCity(){return m_IsPLayerInTheCity;}
	bool HasToOccludeRain(){return m_OccludeRain;}

	TristateValue GetZoneNonPersistentStatus(const u32 ambientZone) const;
	bool SetZoneNonPersistentStatus(const u32 ambientZone, const bool disabled, const bool forceUpdate);
	bool ClearZoneNonPersistentStatus(const u32 ambientZone, const bool forceUpdate);

	void SetZonePersistentStatus(const u32 ambientZone, const bool disabled, const bool forceUpdate);
	void ClearAllPersistentZoneStatusChanges(const bool forceUpdate);

	bool PrepareAlarm(const char* alarmName);
	bool PrepareAlarm(u32 alarmNameHash);
	bool PrepareAlarm(AlarmSettings* alarmSettings);

	void StartAlarm(const char* alarmName, bool skipStartup);
	void StartAlarm(u32 alarmName, bool skipStartup);
	void StartAlarm(AlarmSettings* alarmSettings, u32 alarmNameHash, bool skipStartup);

	void StopAlarm(const char* alarmName, bool stopInstantly);
	void StopAlarm(u32 alarmNameHash, bool stopInstantly);
	void StopAlarm(AlarmSettings* alarmSettings, bool stopInstantly);

	void StopAllAlarms(bool stopInstantly = true);
	void UpdateAlarms(bool isForcedUpdate = false);
	
#if GTA_REPLAY	
	void UpdateReplayAlarmStates();
	const atArray<audReplayAlarmState>& GetReplayAlarmStates() { return m_ReplayActiveAlarms; }
	void ClearReplayActiveAlarms() { return m_ReplayActiveAlarms.clear(); }
	void AddReplayActiveAlarm(audReplayAlarmState alarmState);
#endif

	EnvironmentRule* GetAmbientEnvironmentRule() { return m_ActiveEnvironmentZoneScale==0.0f ? NULL : &m_AverageEnvironmentRule; }
	f32 GetAmbientEnvironmentRuleScale() { return m_ActiveEnvironmentZoneScale; }
	f32 GetAmbientEnvironmentRuleReverbScale() const { return m_ActiveEnvironmentZoneReverbScale; }
	
	enum {kAmbientQuadTreeUpdateDist = 10};
	enum {kMaxActiveAlarms = 6};

#if __BANK
	static void SerialiseFloat(char* xmlMessage, char* tempbuffer, const char* elementName, f32 value);
	static void SerialiseInt(char* xmlMessage, char* tempbuffer, const char* elementName, u32 value);
	static void SerialiseUInt(char* xmlMessage, char* tempbuffer, const char* elementName, u32 value);
	static void SerialiseString(char* xmlMessage, char* tempbuffer, const char* elementName, const char* value);
	static void SerialiseBool(char* xmlMessage, char* tempbuffer, const char* elementName, bool value);
	static void SerialiseBool(char* xmlMessage, char* tempbuffer, const char* elementName, u32 flags, u32 tristateValue);
	static void SerialiseVector(char* xmlMessage, char* tempbuffer, const char* elementName, const Vector3& value);
	static void SerialiseVector(char* xmlMessage, char* tempbuffer, const char* elementName, f32 x, f32 y, f32 z);
	static void SerialiseSoundName(char* xmlMessage, char* tempbuffer, const char* elementName, u32 hash, bool defaultToNull);
	static void SerialiseTag(char* xmlMessage, char* tempbuffer, const char* elementName, bool start);
	static void SerialiseMessageStart(char* xmlMessage, char* tempbuffer, const char* objectType, const char* objectName);
	static void SerialiseMessageEnd(char* xmlMessage, char* tempbuffer, const char* objectType);

	static bool MatchName(const char *name, const char *testString);

	static void MoveEmitterToCurrentCoords();
	static void DuplicateEmitterAtCurrentCoords();
	static void CreateEmitterAtCurrentCoords();
	static void UpdateEmitterToCurrentCoords(const char* sourceEmitterName, const char* savedEmitterName);

	void CreateZone(bool interiorZone, const char* zoneName, const Vector3& positioningCenter, const Vector3& positioningSize, const u16 positioningRotation, const Vector3& activationCenter, const Vector3& activationSize, const u16 activationRotation);
	void UpdateRaveZone(audAmbientZone* zone, const char* zoneName = NULL);
	void HandleRaveZoneCreatedNotification(AmbientZone* newZone);
	void ProcessRaveZoneCreatedNotification(AmbientZone* newZone);
	void HandleRaveShoreLineCreatedNotification(ShoreLineAudioSettings* settings);
	void HandleRaveShoreLineEditedNotification();
	void CreateZoneForCurrentInteriorRoom();
	void CreateZoneAtCurrentCoords();
	void CreateRuleAtCurrentCoords(bool interiorRelative);
	void CreateRuleAtWorldCoords()		{ CreateRuleAtCurrentCoords(false); }
	void CreateRuleAtInteriorCoords()	{ CreateRuleAtCurrentCoords(true); }
	void MoveZoneToCameraCoords();
	void MoveRuleToCurrentCoords(bool interiorRelative);
	void MoveRuleToWorldCoords()		{ MoveRuleToCurrentCoords(false); }
	void MoveRuleToInteriorCoords()		{ MoveRuleToCurrentCoords(true); }
	void DuplicatedEditedZone();
	void ToggleEditZone();
	void ApplyEditedZoneChangesToRAVE();
	void SerialiseShoreLines();
	void AddWidgets(bkBank &bank);
	void DebugDraw();
	void UpdateDebugZoneEditor();
	void UpdateLinePickerTool();
	void TestPrepareAlarm();
	void WarpToZone();
	void TestStartAlarm();
	void TestStopAlarm();
	void TestStreamedSound();
	//Shorelines
	void AddDebugShorePoint();
	void AddDebugShore();
	static void AddShoreLineTypeWidgets();
	static void CreateShorelineEditorWidgets();
	void EditShoreLine();
	void UpdateShoreLineEditor();
	void UpdateEditorAddingMode();
	void UpdateEditorEditingMode();
	void SetRiverWidthAtPoint();
	void WarpToShoreline();
	void AddShoreLinesCombo();
	void CancelShorelineWork();
	void ResetDebugShoreActivationBox();

	void RenderShoreLinesDebug();

	enum eAmbientZoneRenderType
	{
		AMBIENT_ZONE_RENDER_NONE,
		AMBIENT_ZONE_RENDER_ACTIVE_ONLY,
		AMBIENT_ZONE_RENDER_INACTIVE_ONLY,
		AMBIENT_ZONE_RENDER_ALL,
	};

	enum eAmbientZoneEditDimension
	{
		AMBIENT_ZONE_DIMENSION_CAMERA_RELATIVE,
		AMBIENT_ZONE_DIMENSION_X_ONLY,
		AMBIENT_ZONE_DIMENSION_Y_ONLY,
		AMBIENT_ZONE_DIMENSION_Z_ONLY,
	};

	enum eAmbientZoneEditType
	{
		AMBIENT_ZONE_EDIT_POSITIONING,
		AMBIENT_ZONE_EDIT_ACTIVATION,
		AMBIENT_ZONE_EDIT_BOTH,
	};

	enum eAmbientZoneLockDimension
	{
		AMBIENT_ZONE_EDIT_ANCHOR_TOP_LEFT,
		AMBIENT_ZONE_EDIT_ANCHOR_TOP_RIGHT,
		AMBIENT_ZONE_EDIT_ANCHOR_BOTTOM_LEFT,
		AMBIENT_ZONE_EDIT_ANCHOR_BOTTOM_RIGHT,
		AMBIENT_ZONE_EDIT_ANCHOR_CENTRE,
	};

	enum eAmbientZoneRenderMode
	{
		AMBIENT_ZONE_RENDER_TYPE_WIREFRAME,
		AMBIENT_ZONE_RENDER_TYPE_SOLID,
		AMBIENT_ZONE_RENDER_TYPE_OFF,
	};

	// Enum to control what position is used as the point that zones activate around. During gameplay we always
	// want to be using listener position, but other options are useful for visualizing ambience playback from 
	// an external position without altering its behaviour
	enum eZoneActivationType
	{
		ZONE_ACTIVATION_TYPE_LISTENER,
		ZONE_ACTIVATION_TYPE_PLAYER,
		ZONE_ACTIVATION_TYPE_NONE,
	};

	enum eShoreLineEditorState
	{
		STATE_WAITING_FOR_ACTION = 0,
		STATE_ADDING,
		STATE_EDITING,
	};
	enum eShoreLineCreatorMode
	{
		MODE_CLICK_AND_DRAG = 0,
		MODE_ONE_CLICK,
		MODE_USE_CAMERA,
		MAX_MODES,
	};

	static s32 sm_numZonesReturnedByQuadTree;
	static bool sm_DrawActiveQuadTreeNodes;

	static bool sm_DrawShoreLines;
	static bool sm_DrawPlayingShoreLines;
	static bool sm_DrawActivationBox;
	static bool sm_TestForWaterAtPed;
	static bool sm_TestForWaterAtCursor;
	static bool sm_AddingShorelineActivationBox; 
#endif

	enum eWallaType
	{
		WALLA_TYPE_STREAMED,
		WALLA_TYPE_HEAVY = WALLA_TYPE_STREAMED,
		WALLA_TYPE_MEDIUM,
		WALLA_TYPE_SPARSE,
		WALLA_TYPE_MAX,
	};

	enum eAmbientProcessFrame
	{
		PROCESS_INTERIOR_WALLA_DENSITY = 0,
		PROCESS_EXTERIOR_WALLA_DENSITY = 7,
		PROCESS_WORLD_MAX_HEIGHT = 10,
		PROCESS_AMBIENT_ZONE_QUAD_TREE = 14,
		PROCESS_SHORELINE_QUAD_TREE = 21,
		PROCESS_WALLA_SPEECH_RETRIGGER = 28,
	};
	
private:
	void InitPrivate();
	Vec3V_Out GetListenerPosition(audAmbientZone* ambientZone = NULL) const;

	void UpdateCameraUnderWater();
	void StopUnderWaterSound();
	void DestroyUnreferencedDLCZones(bool forceDestroy);
	void LoadNewDLCZones(const char* zoneListName, const char* bankMapName);
	bool HasStopUnderWaterSound();

	void UpdateHighwayAmbience(const u32 timeInMs);
	void UpdateInteriorZones(const u32 timeInMs, const bool isRaining, const f32 loudSoundExclusionRadius, const u16 gameTimeMinutes);
	f32 GetExteriorPedDensityForDirection(audAmbienceDirection direction);
	f32 GetHostilePedDensityForDirection(audAmbienceDirection direction);
	f32 GetInteriorPedDensityForDirection(audAmbienceDirection direction);

	void UpdateStreamedAmbientSounds();
	void UpdateWallaSpeech();
	bool ProcessValidWallaSpeechGroups();
	f32 GetMinRainLevel() const;
	static f32 GetMinRainLevelForWeatherType(const u32 weatherType);

#if __BANK
	void DebugDrawWallaSpeech();
#endif 

	void CheckAndAddPedWallaVoice(u32 voiceGroupHash, bool isMale, bool isGang);
	void UpdateWalla();
	void UpdateInteriorWalla();
	void StopInteriorWallaSounds();
	void StopExteriorWallaSounds();

	void UpdateDirectionalAmbiences(const float timeStepS);
	bool HasStoppedInteriorWallaSound();
	bool HasStoppedExteriorWallaSound();

	void UpdateWorldHeight();
	f32 ComputeWorldHeightFactorAtListenerPosition();
	f32 ComputeUnderwaterCreakFactorAtListenerPosition() const;

	void CalculateActiveZonesMinMaxPedDensity(f32& minPedDensity, f32& maxPedDensity, f32& pedDensityScalar, bool interiorZones);
	void UpdatePanicWalla(f32 averageFractionPanicing, f32 pedDensity, audSimpleSmootherDiscrete* smoother);
	void StopAndUnloadPanicAssets();
	void DebugDrawWallaExterior();
	void DebugDrawWallaHostile();
	void DebugDrawWallaInterior();
	f32 CalculatePedsToListenerHeightDiff();
	void SetZoneFlag(AmbientZone* zone, AmbientZoneFlagIds flagId, TristateValue value);
	bool DoesSoundRequireStreamSlot(audMetadataRef sound);

	void UpdateEnvironmentRule();
	void ResetEnvironmentRule();
	void UpdateAmbientRadioEmitters();
	void CalculateValidAmbientRadioStations();
	f32 CalculateCurrentPedDirectionPan();

	Vector3 CalculateAnchorPointSizeCompensation(const Vector3& initialSize, const Vector3& newSize, f32 rotationAngleDegrees) const;
	Vector3 CalculateAnchorPointRotationCompensation(const Vector3& zoneSize, f32 initialRotationDegrees, f32 newRotationDegrees) const;

#if __BANK
	template <class _T> static void SerialiseObjectName(char* xmlMessage, char* tempbuffer, const char* elementName, u32 hash)
	{
		const _T* object = audNorthAudioEngine::GetObject<_T>(hash);

		if(object)
		{
			sprintf(tempbuffer, "			<%s>%s</%s>\n", elementName, audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(object->NameTableOffset), elementName);
			naDisplayf("%s", tempbuffer);
			strcat(xmlMessage, tempbuffer);
		}
	}

#if 0
	template <class _T> static _T *GetObject(const audMetadataRef metadataReference)
	{
		return sm_MetadataMgr.GetObject<_T>(metadataReference);
	}
#endif

#endif
	static bool OnUnderwaterStopCallback(u32 userData);
	static bool HasUnderwaterStoppedCallback(u32 userData);

	static bool OnInteriorWallaStopCallback(u32 userData);
	static bool HasInteriorWallaStoppedCallback(u32 userData);

	static bool OnExteriorWallaStopCallback(u32 userData);
	static bool HasExteriorWallaStoppedCallback(u32 userData);

private:
	// these arrays store requests from the particle update thread
	enum eAmbientRadioEmitterState
	{
		AMBIENT_RADIO_OFF,
		AMBIENT_RADIO_FADE_IN,
		AMBIENT_RADIO_ON,
		AMBIENT_RADIO_FADE_OUT,
	};
	
	enum eAmbientRadioEmitterType
	{
		AMBIENT_RADIO_TYPE_STATIC,
		AMBIENT_RADIO_TYPE_VEHICLE,
	};

	struct audDLCZone
	{
		audAmbientZone* zone;
		u32 chunkNameHash;
	};

	struct audDLCShore
	{
		audShoreLine* zone;
		u32 chunkNameHash;
	};

	StaticEmitter* m_AmbientRadioSettings;
	eAmbientRadioEmitterState m_FakeRadioEmitterState;
	eAmbientRadioEmitterType m_FakeRadioEmitterType;
	audAmbientRadioEmitter m_FakeRadioEmitter;
	RandomisedRadioEmitterSettings* m_FakeRadioEmitterSettingsLastFrame;
	atHashString m_LoadedLevelName;
	Vec3V m_FakeRadioEmitterListenerSpawnPoint;
	f32 m_FakeRadioEmitterVolLin;
	f32 m_FakeRadioEmitterDistance;
	f32 m_FakeRadioEmitterTimeActive;
	f32 m_FakeRadioEmitterLifeTime;
	f32 m_FakeRadioEmitterStartPan;
	f32 m_FakeRadioEmitterEndPan;
	f32 m_FakeRadioEmitterAttackTime;
	f32 m_FakeRadioEmitterHoldTime;
	f32 m_FakeRadioEmitterRadius;
	f32 m_FakeRadioEmitterReleaseTime;
	u32 m_NextFakeStaticRadioTriggerTime;
	u32 m_NextFakeVehicleRadioTriggerTime;
	u32 m_FakeRadioTimeLastFrame;
	u32 m_FakeRadioEmitterRadioStationHash;
	u8 m_FakeRadioEmitterRadioStation;

	audRegisteredAudioEffectRequest m_RequestedAudioEffects[2][g_MaxRegisteredAudioEffects];
	audRegisteredAudioEffect m_RegisteredAudioEffects[g_MaxRegisteredAudioEffects];

	u32 m_NumRequestedAudioEffects[2];
	u32 m_RequestedAudioEffectsWriteIndex;
	f32 m_LoudSoundExclusionRadius;

	atArray<audDirectionalAmbienceManager>	m_DirectionalAmbienceManagers;
	atArray<audAmbientZone*>				m_Zones;
	atArray<audAmbientZone>					m_InteriorZones;
	atArray<audAmbientZone*>				m_ActiveZones;
	atArray<audAmbientZone*>				m_ActiveEnvironmentZones;
	atArray<audShoreLine*>					m_ActiveShoreLines;
	atArray<audDLCZone>						m_DLCZones;
	atArray<audDLCShore>					m_DLCShores;
	atArray<u32>							m_LoadedDLCChunks;
	atArray<u32>							m_LoadedShorelinesDLCChunks;
	atArray<u32>							m_ZonesWithModifiedEnabledFlags;
	atArray<audAmbientZoneEnabledState>		m_ZonesWithNonPersistentStateFlags;

#if GTA_REPLAY
	atArray<audAmbientZoneReplayState>		m_ReplayAmbientZoneStates;
	atArray<audReplayAlarmState>			m_ReplayActiveAlarms;
	bool									m_ReplayOverrideUnderWaterStream;
#endif

	sysCriticalSectionToken					m_Lock;
	EnvironmentRule							m_AverageEnvironmentRule; //an average of all our currently active environment zones
	f32										m_ActiveEnvironmentZoneScale;
	f32										m_ActiveEnvironmentZoneReverbScale;

	// Shore lines.
	fwQuadTreeNode m_ShoreLinesQuadTree;
#if __BANK
	audShoreLine* m_DebugShoreLine;// Used by the creation tool.
	static f32	sm_DebugShoreLineWidth;
	static f32	sm_DebugShoreLineHeight;
	static s32 sm_ShoreLineEditorState;
	static u8  sm_ShoreLineType;
	static bool sm_MouseLeftPressed;
	static bool sm_MouseRightPressed;
	static bool sm_MouseMiddlePressed;
	static bool sm_WarpPlayerToShoreline;
	static bool	sm_EditActivationBoxSize;
	static bool	sm_EditWidth;
#endif
	Vec3V m_ZoneActivationPosition;
	fwQuadTreeNode m_ZoneQuadTree;
	//audScriptStreamState m_ScriptStreamState;
	audSoundSet m_UnderWaterSoundSet;
	audSound*	m_UnderWaterDeepSound;
	audSound*	m_WallaSpeechSound;
	f32* m_PlayerSpeedVariable;
	audStreamSlot* m_UnderwaterStreamSlot;
	audScene *	m_UnderWaterScene;
	audScene *	m_UnderWaterSubScene;
	PedWallaSpeechSettings* m_CurrentPedWallaSpeechSettings;

	audSound*	m_RainOnWaterSound;
	audAmbientZone *m_CurrentWindZone;

	audCurve m_TimeOfDayToPassbyFrequency;

	audSimpleSmootherDiscrete m_WallaSmootherExterior;
	audSimpleSmoother m_WallaSmootherInterior;
	audSimpleSmoother m_WallaSmootherHostile;
	audSimpleSmootherDiscrete m_RMSLevelSmoother;
	audSimpleSmoother m_GlobalHighwayVolumeSmoother;

	audCurve m_NumPedToWallaExterior;
	audCurve m_NumPedToWallaHostile;
	audCurve m_NumPedToWallaInterior;

	audCurve m_PedWallaDensityPostPanic;
	audCurve m_PedDensityToPanicVol;

	audCurve m_RoadNodesToHighwayVol;
	audCurve m_MaxPedDensityToWallaSpeechTriggerRate;

	audStreamSlot* m_InteriorWallaStreamSlot;
	audStreamSlot* m_ExteriorWallaStreamSlot;

	bool m_DoesExteriorWallaRequireStreamSlot;
	bool m_ExteriorWallaSwitchActive;
	audSoundSet m_ExteriorWallaSoundSet;
	u32 m_CurrentExteriorWallaSoundSetName;
	u32 m_DefaultExteriorWallaSoundSetName;
	audSound* m_ExteriorWallaSounds[WALLA_TYPE_MAX];

	bool m_DoesInteriorWallaRequireStreamSlot;
	bool m_InteriorWallaSwitchActive;
	audSoundSet m_InteriorWallaSoundSet;
	u32 m_CurrentInteriorWallaSoundSetName;
	u32 m_DefaultInteriorWallaSoundSetName;
	audSound* m_InteriorWallaSounds[WALLA_TYPE_MAX];
	u32 m_CurrentInteriorRoomWallaSoundSet;
	bool m_CurrentInteriorRoomHasWalla;

	f32 m_ScriptDesiredPedDensityExterior;
	f32 m_ScriptDesiredPedDensityExteriorApplyFactor;
	s32 m_ScriptControllingPedDensityExterior;

	s32	m_ClosestShoreIdx;
	audSoundSet m_RainOnWaterSounds;

	f32 m_ScriptDesiredPedDensityInterior;
	f32 m_ScriptDesiredPedDensityInteriorApplyFactor;
	s32 m_ScriptControllingPedDensityInterior;

	s32 m_ScriptControllingExteriorWallaSoundSet;
	s32 m_ScriptControllingInteriorWallaSoundSet;

	u32 m_WallaSpeechSelectedVoice;
	u32 m_WallaSpeechSelectedContext;

	u32 m_LastTimePlayerInWater;

	f32 m_FinalPedDensityExterior;
	f32 m_FinalPedDensityInterior;
	f32 m_FinalPedDensityHostile;
	f32 m_WorldHeightFactor;
	f32 m_AverageFractionPanicingExterior;
	f32 m_UnderwaterCreakFactor;
	f32 m_WallaSpeechTimer;
	s32	m_WallaSpeechSlotID;
	u32 m_WallaPedGroupProcessingIndex;
	u32 m_WallaPedModelProcessingIndex;
	u32 m_LastUpdateTimeMs;

	s32 m_PanicWallaSpeechSlotIds[2];
	audWaveSlot* m_PanicWallaSpeechSlots[2];
	u32 m_PanicWallaAssetHashes[2];
	audSound* m_PanicWallaSound;
	f32 m_PedPanicTimer;
	u32 m_PanicWallaBank;
	f32 m_MinValidPedDensityExterior;
	f32 m_MaxValidPedDensityExterior;
	f32 m_PedDensityScalarExterior;
	f32 m_MinValidPedDensityInterior;
	f32 m_MaxValidPedDensityInterior;
	f32 m_PedDensityScalarInterior;
	f32 m_ForcePanic;
	bool m_ForceQuadTreeUpdate;
	bool m_WasPlayerDead;
	bool m_IsPLayerInTheCity;
	bool m_OccludeRain;

	static audAmbientZone* sm_CityZone;

	static audEnvironmentMetricsInternal sm_MetricsInternal;
	static audEnvironmentSoundMetrics sm_Metrics;

	static f32 sm_CameraDistanceToShoreThreshold;
	static f32 sm_CameraDepthInWaterThreshold;
	
	static u32 sm_ScriptUnderWaterStream;

	static bool sm_PlayingUnderWaterSound;
	static bool sm_WasCameraUnderWater;

	BANK_ONLY(s32 m_CurrentDebugShoreLineIndex);
	BANK_ONLY(static bool sm_OverrideUnderWaterStream);

	enum {kNumPanicWallaVariations = 8};

	enum ePanicAssetState
	{
		PANIC_ASSETS_REQUEST_SLOTS,
		PANIC_ASSETS_REQUEST_LOAD,
		PANIC_ASSETS_WAIT_FOR_LOAD,
		PANIC_ASSETS_PREPARE_AND_PLAY,
		PANIC_ASSETS_PLAYING,
		PANIC_ASSETS_COOLDOWN,
	};

	ePanicAssetState m_PanicAssetPrepareState;

	CInteriorProxy* m_InteriorProxyLastFrame;
	s32 m_InteriorRoomIndexLastFrame;
	u32 m_NumFramesInteriorActive;

	Vector3 m_ClosestHighwayPos1, m_ClosestHighwayPos2;
	bool m_HighwayPosValid;

	u32 m_LastHighwayPassbyTime;
	f32 m_HighwayPassbyTimeFrequency;
	AudioRoadInfo* m_DefaultRoadInfo;
	AudioRoadInfo* m_PrevRoadInfo;
	u32 m_PrevRoadInfoHash;
	audVolumeCone m_HighwayPassbyVolumeCone;

	enum { kMaxHighwayNodes = 512 };
	CNodeAddress m_HighwaySearchNodes[kMaxHighwayNodes];
	u32 m_NumHighwayNodesFound;
	u32 m_NumHighwayNodesInRadius;
	Vector3 m_LastHighwayNodeCalculationPoint;

	struct HighwayPassbySound
	{
		HighwayPassbySound()
		{
			sound = NULL;
			soundWet = NULL;
			reverbAmount = 0.0f;
			pathNodesValid = false;
			isSlipLane = false;
			tyreBumpsEnabled = false;
			reverbSmoother.Init(0.01f, true);
		}

		audSimpleSmoother reverbSmoother;
		Vector3 position;
		Vector3 velocity;
		Vector3 startPos;
		Vector3 endPos;
		CNodeAddress startNodeAddress;
		CNodeAddress endNodeAddress;
		audSound* sound;
		audSound* soundWet;
		CVehicleModelInfo* vehicleModel;
		CarAudioSettings* carAudioSettings;
		f32 tyreBumpDistance;
		f32 distanceSinceLastFrontTyreBump;
		f32 distanceSinceLastRearTyreBump;
		f32 lifeTime;
		f32 reverbAmount;
		f32 distLastFrame;
		bool isSlipLane;
		bool pathNodesValid;
		bool tyreBumpsEnabled;
		
		BANK_ONLY(Vector3 lastBumpPos);
	};

	enum {kMaxPassbySounds = 8};
	atRangeArray<HighwayPassbySound, kMaxPassbySounds> m_HighwayPassbySounds;

	struct ActiveAlarm
	{
		ActiveAlarm()
		{
			primaryAlarmSound = NULL;
			interiorSettings = NULL;
			stopAtMaxDistance = false;
			alarmName = 0u;
		}

		audSound* primaryAlarmSound;
		audCurve alarmDecayCurve;
		AlarmSettings* alarmSettings;
		InteriorSettings* interiorSettings;
		u32 timeStarted;
		u32 alarmName;
		bool stopAtMaxDistance;
	};
	
	atRangeArray<ActiveAlarm, kMaxActiveAlarms> m_ActiveAlarms;

	struct PedWallaValidVoice
	{
		BANK_ONLY(const char* pedVoiceGroupName;)
		u32 voiceNameHash;
		bool isMale;
		bool isGang;
	};

	enum {kMaxValidPedWallaVoices = 64};
	enum {kMaxValidPedVoiceGroups = 256};
	atFixedArray<PedWallaValidVoice, kMaxValidPedWallaVoices> m_ValidWallaVoices;
	atFixedArray<CPedModelInfo*, kMaxValidPedVoiceGroups> m_ValidWallaPedModels;

	struct AmbientStreamedSoundSlot
	{
		AmbientStreamedSoundSlot()
		{
			sound = NULL;
			speechSlot = NULL;
			speechSlotID = 0;
		}

		audSound*		sound;
		audWaveSlot*	speechSlot;
		s32				speechSlotID;
	};

	enum {kMaxAmbientStreamedSounds = 1};
	atRangeArray<AmbientStreamedSoundSlot, kMaxAmbientStreamedSounds> m_AmbientStreamedSoundSlots;

	struct AmbientWallaHistory
	{
		u32 voiceName;
		u32 context;
		u32 timeTriggered;
		u32 conversationIndex;
		bool isPhoneConversation;
	};

	enum {kMaxAmbientWallaHistorySlots = 25};
	atQueue<AmbientWallaHistory, kMaxAmbientWallaHistorySlots> m_AmbientWallaHistory;
	atFixedArray<const audRadioStation*,4> m_ValidRadioStations;
};

#endif // AUD_AMBIENTAUDIOENTITY_H
