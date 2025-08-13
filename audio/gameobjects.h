// 
// gameobjects.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
// 
// Automatically generated metadata structure definitions - do not edit.
// 

#ifndef AUD_GAMEOBJECTS_H
#define AUD_GAMEOBJECTS_H

#include "vector/vector3.h"
#include "vector/vector4.h"
#include "audioengine/metadataref.h"

// macros for dealing with packed tristates
#ifndef AUD_GET_TRISTATE_VALUE
	#define AUD_GET_TRISTATE_VALUE(flagvar, flagid) (TristateValue)((flagvar >> (flagid<<1)) & 0x03)
	#define AUD_SET_TRISTATE_VALUE(flagvar, flagid, trival) ((flagvar |= ((trival&0x03) << (flagid<<1))))
	namespace rage
	{
		enum TristateValue
		{
			AUD_TRISTATE_FALSE = 0,
			AUD_TRISTATE_TRUE,
			AUD_TRISTATE_UNSPECIFIED,
		}; // enum TristateValue
	} // namespace rage
#endif // !defined AUD_GET_TRISTATE_VALUE

namespace rage
{
	#define GAMEOBJECTS_SCHEMA_VERSION 151
	
	#define GAMEOBJECTS_METADATA_COMPILER_VERSION 2
	
	// NOTE: doesn't include abstract objects
	#define AUD_NUM_GAMEOBJECTS 121
	
	#define AUD_DECLARE_GAMEOBJECTS_FUNCTION(fn) void* GetAddressOf_##fn(rage::u8 classId);
	
	#define AUD_DEFINE_GAMEOBJECTS_FUNCTION(fn) \
	void* GetAddressOf_##fn(rage::u8 classId) \
	{ \
		switch(classId) \
		{ \
			case VehicleCollisionSettings::TYPE_ID: return (void*)&VehicleCollisionSettings::s##fn; \
			case TrailerAudioSettings::TYPE_ID: return (void*)&TrailerAudioSettings::s##fn; \
			case CarAudioSettings::TYPE_ID: return (void*)&CarAudioSettings::s##fn; \
			case VehicleEngineAudioSettings::TYPE_ID: return (void*)&VehicleEngineAudioSettings::s##fn; \
			case CollisionMaterialSettings::TYPE_ID: return (void*)&CollisionMaterialSettings::s##fn; \
			case StaticEmitter::TYPE_ID: return (void*)&StaticEmitter::s##fn; \
			case EntityEmitter::TYPE_ID: return (void*)&EntityEmitter::s##fn; \
			case HeliAudioSettings::TYPE_ID: return (void*)&HeliAudioSettings::s##fn; \
			case MeleeCombatSettings::TYPE_ID: return (void*)&MeleeCombatSettings::s##fn; \
			case SpeechContextSettings::TYPE_ID: return (void*)&SpeechContextSettings::s##fn; \
			case TriggeredSpeechContext::TYPE_ID: return (void*)&TriggeredSpeechContext::s##fn; \
			case SpeechContext::TYPE_ID: return (void*)&SpeechContext::s##fn; \
			case SpeechContextVirtual::TYPE_ID: return (void*)&SpeechContextVirtual::s##fn; \
			case SpeechParams::TYPE_ID: return (void*)&SpeechParams::s##fn; \
			case SpeechContextList::TYPE_ID: return (void*)&SpeechContextList::s##fn; \
			case BoatAudioSettings::TYPE_ID: return (void*)&BoatAudioSettings::s##fn; \
			case WeaponSettings::TYPE_ID: return (void*)&WeaponSettings::s##fn; \
			case ShoeAudioSettings::TYPE_ID: return (void*)&ShoeAudioSettings::s##fn; \
			case OldShoeSettings::TYPE_ID: return (void*)&OldShoeSettings::s##fn; \
			case MovementShoeSettings::TYPE_ID: return (void*)&MovementShoeSettings::s##fn; \
			case FootstepSurfaceSounds::TYPE_ID: return (void*)&FootstepSurfaceSounds::s##fn; \
			case ModelPhysicsParams::TYPE_ID: return (void*)&ModelPhysicsParams::s##fn; \
			case SkiAudioSettings::TYPE_ID: return (void*)&SkiAudioSettings::s##fn; \
			case RadioStationList::TYPE_ID: return (void*)&RadioStationList::s##fn; \
			case RadioStationSettings::TYPE_ID: return (void*)&RadioStationSettings::s##fn; \
			case RadioStationTrackList::TYPE_ID: return (void*)&RadioStationTrackList::s##fn; \
			case RadioTrackCategoryData::TYPE_ID: return (void*)&RadioTrackCategoryData::s##fn; \
			case ScannerCrimeReport::TYPE_ID: return (void*)&ScannerCrimeReport::s##fn; \
			case PedRaceToPVG::TYPE_ID: return (void*)&PedRaceToPVG::s##fn; \
			case PedVoiceGroups::TYPE_ID: return (void*)&PedVoiceGroups::s##fn; \
			case FriendGroup::TYPE_ID: return (void*)&FriendGroup::s##fn; \
			case StaticEmitterList::TYPE_ID: return (void*)&StaticEmitterList::s##fn; \
			case ScriptedScannerLine::TYPE_ID: return (void*)&ScriptedScannerLine::s##fn; \
			case ScannerArea::TYPE_ID: return (void*)&ScannerArea::s##fn; \
			case ScannerSpecificLocation::TYPE_ID: return (void*)&ScannerSpecificLocation::s##fn; \
			case ScannerSpecificLocationList::TYPE_ID: return (void*)&ScannerSpecificLocationList::s##fn; \
			case AmbientZone::TYPE_ID: return (void*)&AmbientZone::s##fn; \
			case AmbientRule::TYPE_ID: return (void*)&AmbientRule::s##fn; \
			case AmbientZoneList::TYPE_ID: return (void*)&AmbientZoneList::s##fn; \
			case AmbientSlotMap::TYPE_ID: return (void*)&AmbientSlotMap::s##fn; \
			case AmbientBankMap::TYPE_ID: return (void*)&AmbientBankMap::s##fn; \
			case EnvironmentRule::TYPE_ID: return (void*)&EnvironmentRule::s##fn; \
			case TrainStation::TYPE_ID: return (void*)&TrainStation::s##fn; \
			case InteriorSettings::TYPE_ID: return (void*)&InteriorSettings::s##fn; \
			case InteriorWeaponMetrics::TYPE_ID: return (void*)&InteriorWeaponMetrics::s##fn; \
			case InteriorRoom::TYPE_ID: return (void*)&InteriorRoom::s##fn; \
			case DoorAudioSettings::TYPE_ID: return (void*)&DoorAudioSettings::s##fn; \
			case DoorTuningParams::TYPE_ID: return (void*)&DoorTuningParams::s##fn; \
			case DoorList::TYPE_ID: return (void*)&DoorList::s##fn; \
			case ItemAudioSettings::TYPE_ID: return (void*)&ItemAudioSettings::s##fn; \
			case ClimbingAudioSettings::TYPE_ID: return (void*)&ClimbingAudioSettings::s##fn; \
			case ModelAudioCollisionSettings::TYPE_ID: return (void*)&ModelAudioCollisionSettings::s##fn; \
			case TrainAudioSettings::TYPE_ID: return (void*)&TrainAudioSettings::s##fn; \
			case WeatherAudioSettings::TYPE_ID: return (void*)&WeatherAudioSettings::s##fn; \
			case WeatherTypeAudioReference::TYPE_ID: return (void*)&WeatherTypeAudioReference::s##fn; \
			case BicycleAudioSettings::TYPE_ID: return (void*)&BicycleAudioSettings::s##fn; \
			case PlaneAudioSettings::TYPE_ID: return (void*)&PlaneAudioSettings::s##fn; \
			case ConductorsDynamicMixingReference::TYPE_ID: return (void*)&ConductorsDynamicMixingReference::s##fn; \
			case StemMix::TYPE_ID: return (void*)&StemMix::s##fn; \
			case InteractiveMusicMood::TYPE_ID: return (void*)&InteractiveMusicMood::s##fn; \
			case StartTrackAction::TYPE_ID: return (void*)&StartTrackAction::s##fn; \
			case StopTrackAction::TYPE_ID: return (void*)&StopTrackAction::s##fn; \
			case SetMoodAction::TYPE_ID: return (void*)&SetMoodAction::s##fn; \
			case MusicEvent::TYPE_ID: return (void*)&MusicEvent::s##fn; \
			case StartOneShotAction::TYPE_ID: return (void*)&StartOneShotAction::s##fn; \
			case StopOneShotAction::TYPE_ID: return (void*)&StopOneShotAction::s##fn; \
			case BeatConstraint::TYPE_ID: return (void*)&BeatConstraint::s##fn; \
			case BarConstraint::TYPE_ID: return (void*)&BarConstraint::s##fn; \
			case DirectionalAmbience::TYPE_ID: return (void*)&DirectionalAmbience::s##fn; \
			case GunfightConductorIntensitySettings::TYPE_ID: return (void*)&GunfightConductorIntensitySettings::s##fn; \
			case AnimalParams::TYPE_ID: return (void*)&AnimalParams::s##fn; \
			case AnimalVocalAnimTrigger::TYPE_ID: return (void*)&AnimalVocalAnimTrigger::s##fn; \
			case ScannerVoiceParams::TYPE_ID: return (void*)&ScannerVoiceParams::s##fn; \
			case ScannerVehicleParams::TYPE_ID: return (void*)&ScannerVehicleParams::s##fn; \
			case AudioRoadInfo::TYPE_ID: return (void*)&AudioRoadInfo::s##fn; \
			case MicSettingsReference::TYPE_ID: return (void*)&MicSettingsReference::s##fn; \
			case MicrophoneSettings::TYPE_ID: return (void*)&MicrophoneSettings::s##fn; \
			case CarRecordingAudioSettings::TYPE_ID: return (void*)&CarRecordingAudioSettings::s##fn; \
			case CarRecordingList::TYPE_ID: return (void*)&CarRecordingList::s##fn; \
			case AnimalFootstepSettings::TYPE_ID: return (void*)&AnimalFootstepSettings::s##fn; \
			case AnimalFootstepReference::TYPE_ID: return (void*)&AnimalFootstepReference::s##fn; \
			case ShoeList::TYPE_ID: return (void*)&ShoeList::s##fn; \
			case ClothAudioSettings::TYPE_ID: return (void*)&ClothAudioSettings::s##fn; \
			case ClothList::TYPE_ID: return (void*)&ClothList::s##fn; \
			case ExplosionAudioSettings::TYPE_ID: return (void*)&ExplosionAudioSettings::s##fn; \
			case GranularEngineAudioSettings::TYPE_ID: return (void*)&GranularEngineAudioSettings::s##fn; \
			case ShoreLinePoolAudioSettings::TYPE_ID: return (void*)&ShoreLinePoolAudioSettings::s##fn; \
			case ShoreLineLakeAudioSettings::TYPE_ID: return (void*)&ShoreLineLakeAudioSettings::s##fn; \
			case ShoreLineRiverAudioSettings::TYPE_ID: return (void*)&ShoreLineRiverAudioSettings::s##fn; \
			case ShoreLineOceanAudioSettings::TYPE_ID: return (void*)&ShoreLineOceanAudioSettings::s##fn; \
			case ShoreLineList::TYPE_ID: return (void*)&ShoreLineList::s##fn; \
			case RadioTrackSettings::TYPE_ID: return (void*)&RadioTrackSettings::s##fn; \
			case ModelFootStepTuning::TYPE_ID: return (void*)&ModelFootStepTuning::s##fn; \
			case GranularEngineSet::TYPE_ID: return (void*)&GranularEngineSet::s##fn; \
			case RadioDjSpeechAction::TYPE_ID: return (void*)&RadioDjSpeechAction::s##fn; \
			case SilenceConstraint::TYPE_ID: return (void*)&SilenceConstraint::s##fn; \
			case ReflectionsSettings::TYPE_ID: return (void*)&ReflectionsSettings::s##fn; \
			case AlarmSettings::TYPE_ID: return (void*)&AlarmSettings::s##fn; \
			case FadeOutRadioAction::TYPE_ID: return (void*)&FadeOutRadioAction::s##fn; \
			case FadeInRadioAction::TYPE_ID: return (void*)&FadeInRadioAction::s##fn; \
			case ForceRadioTrackAction::TYPE_ID: return (void*)&ForceRadioTrackAction::s##fn; \
			case SlowMoSettings::TYPE_ID: return (void*)&SlowMoSettings::s##fn; \
			case PedScenarioAudioSettings::TYPE_ID: return (void*)&PedScenarioAudioSettings::s##fn; \
			case PortalSettings::TYPE_ID: return (void*)&PortalSettings::s##fn; \
			case ElectricEngineAudioSettings::TYPE_ID: return (void*)&ElectricEngineAudioSettings::s##fn; \
			case PlayerBreathingSettings::TYPE_ID: return (void*)&PlayerBreathingSettings::s##fn; \
			case PedWallaSpeechSettings::TYPE_ID: return (void*)&PedWallaSpeechSettings::s##fn; \
			case AircraftWarningSettings::TYPE_ID: return (void*)&AircraftWarningSettings::s##fn; \
			case PedWallaSettingsList::TYPE_ID: return (void*)&PedWallaSettingsList::s##fn; \
			case CopDispatchInteractionSettings::TYPE_ID: return (void*)&CopDispatchInteractionSettings::s##fn; \
			case RadioTrackTextIds::TYPE_ID: return (void*)&RadioTrackTextIds::s##fn; \
			case RandomisedRadioEmitterSettings::TYPE_ID: return (void*)&RandomisedRadioEmitterSettings::s##fn; \
			case TennisVocalizationSettings::TYPE_ID: return (void*)&TennisVocalizationSettings::s##fn; \
			case DoorAudioSettingsLink::TYPE_ID: return (void*)&DoorAudioSettingsLink::s##fn; \
			case SportsCarRevsSettings::TYPE_ID: return (void*)&SportsCarRevsSettings::s##fn; \
			case FoliageSettings::TYPE_ID: return (void*)&FoliageSettings::s##fn; \
			case ReplayRadioStationTrackList::TYPE_ID: return (void*)&ReplayRadioStationTrackList::s##fn; \
			case MacsModelOverrideList::TYPE_ID: return (void*)&MacsModelOverrideList::s##fn; \
			case MacsModelOverrideListBig::TYPE_ID: return (void*)&MacsModelOverrideListBig::s##fn; \
			case SoundFieldDefinition::TYPE_ID: return (void*)&SoundFieldDefinition::s##fn; \
			case GameObjectHashList::TYPE_ID: return (void*)&GameObjectHashList::s##fn; \
			default: return NULL; \
		} \
	}
	
	// PURPOSE - Gets the type id of the parent class from the given type id
	rage::u32 gGameObjectsGetBaseTypeId(const rage::u32 classId);
	
	// PURPOSE - Determines if a type inherits from another type
	bool gGameObjectsIsOfType(const rage::u32 objectTypeId, const rage::u32 baseTypeId);
	
	// PURPOSE - Determines if a type inherits from another type
	template<class _ObjectType>
	bool gGameObjectsIsOfType(const _ObjectType* const obj, const rage::u32 baseTypeId)
	{
		return gGameObjectsIsOfType(obj->ClassId, baseTypeId);
	}
	
	// 
	// Enumerations
	// 
	enum RadioGenre
	{
		RADIO_GENRE_OFF = 0,
		RADIO_GENRE_MODERN_ROCK,
		RADIO_GENRE_CLASSIC_ROCK,
		RADIO_GENRE_POP,
		RADIO_GENRE_MODERN_HIPHOP,
		RADIO_GENRE_CLASSIC_HIPHOP,
		RADIO_GENRE_PUNK,
		RADIO_GENRE_LEFT_WING_TALK,
		RADIO_GENRE_RIGHT_WING_TALK,
		RADIO_GENRE_COUNTRY,
		RADIO_GENRE_DANCE,
		RADIO_GENRE_MEXICAN,
		RADIO_GENRE_REGGAE,
		RADIO_GENRE_JAZZ,
		RADIO_GENRE_MOTOWN,
		RADIO_GENRE_SURF,
		RADIO_GENRE_UNSPECIFIED,
		NUM_RADIOGENRE,
		RADIOGENRE_MAX = NUM_RADIOGENRE,
	}; // enum RadioGenre
	const char* RadioGenre_ToString(const RadioGenre value);
	RadioGenre RadioGenre_Parse(const char* str, const RadioGenre defaultValue);
	
	enum AmbientRadioLeakage
	{
		LEAKAGE_BASSY_LOUD = 0,
		LEAKAGE_BASSY_MEDIUM,
		LEAKAGE_MIDS_LOUD,
		LEAKAGE_MIDS_MEDIUM,
		LEAKAGE_MIDS_QUIET,
		LEAKAGE_CRAZY_LOUD,
		LEAKAGE_MODDED,
		LEAKAGE_PARTYBUS,
		NUM_AMBIENTRADIOLEAKAGE,
		AMBIENTRADIOLEAKAGE_MAX = NUM_AMBIENTRADIOLEAKAGE,
	}; // enum AmbientRadioLeakage
	const char* AmbientRadioLeakage_ToString(const AmbientRadioLeakage value);
	AmbientRadioLeakage AmbientRadioLeakage_Parse(const char* str, const AmbientRadioLeakage defaultValue);
	
	enum GPSType
	{
		GPS_TYPE_NONE = 0,
		GPS_TYPE_ANY,
		NUM_GPSTYPE,
		GPSTYPE_MAX = NUM_GPSTYPE,
	}; // enum GPSType
	const char* GPSType_ToString(const GPSType value);
	GPSType GPSType_Parse(const char* str, const GPSType defaultValue);
	
	enum GPSVoice
	{
		GPS_VOICE_FEMALE = 0,
		GPS_VOICE_MALE,
		GPS_VOICE_EXTRA1,
		GPS_VOICE_EXTRA2,
		GPS_VOICE_EXTRA3,
		NUM_GPSVOICE,
		GPSVOICE_MAX = NUM_GPSVOICE,
	}; // enum GPSVoice
	const char* GPSVoice_ToString(const GPSVoice value);
	GPSVoice GPSVoice_Parse(const char* str, const GPSVoice defaultValue);
	
	enum RadioType
	{
		RADIO_TYPE_NONE = 0,
		RADIO_TYPE_NORMAL,
		RADIO_TYPE_EMERGENCY_SERVICES,
		RADIO_TYPE_NORMAL_OFF_MISSION_AND_MP,
		NUM_RADIOTYPE,
		RADIOTYPE_MAX = NUM_RADIOTYPE,
	}; // enum RadioType
	const char* RadioType_ToString(const RadioType value);
	RadioType RadioType_Parse(const char* str, const RadioType defaultValue);
	
	enum VehicleVolumeCategory
	{
		VEHICLE_VOLUME_VERY_LOUD = 0,
		VEHICLE_VOLUME_LOUD,
		VEHICLE_VOLUME_NORMAL,
		VEHICLE_VOLUME_QUIET,
		VEHICLE_VOLUME_VERY_QUIET,
		NUM_VEHICLEVOLUMECATEGORY,
		VEHICLEVOLUMECATEGORY_MAX = NUM_VEHICLEVOLUMECATEGORY,
	}; // enum VehicleVolumeCategory
	const char* VehicleVolumeCategory_ToString(const VehicleVolumeCategory value);
	VehicleVolumeCategory VehicleVolumeCategory_Parse(const char* str, const VehicleVolumeCategory defaultValue);
	
	enum SpeechVolumeType
	{
		CONTEXT_VOLUME_NORMAL = 0,
		CONTEXT_VOLUME_SHOUTED,
		CONTEXT_VOLUME_PAIN,
		CONTEXT_VOLUME_MEGAPHONE,
		CONTEXT_VOLUME_FROM_HELI,
		CONTEXT_VOLUME_FRONTEND,
		NUM_SPEECHVOLUMETYPE,
		SPEECHVOLUMETYPE_MAX = NUM_SPEECHVOLUMETYPE,
	}; // enum SpeechVolumeType
	const char* SpeechVolumeType_ToString(const SpeechVolumeType value);
	SpeechVolumeType SpeechVolumeType_Parse(const char* str, const SpeechVolumeType defaultValue);
	
	enum SpeechAudibilityType
	{
		CONTEXT_AUDIBILITY_NORMAL = 0,
		CONTEXT_AUDIBILITY_CLEAR,
		CONTEXT_AUDIBILITY_CRITICAL,
		CONTEXT_AUDIBILITY_LEAD_IN,
		NUM_SPEECHAUDIBILITYTYPE,
		SPEECHAUDIBILITYTYPE_MAX = NUM_SPEECHAUDIBILITYTYPE,
	}; // enum SpeechAudibilityType
	const char* SpeechAudibilityType_ToString(const SpeechAudibilityType value);
	SpeechAudibilityType SpeechAudibilityType_Parse(const char* str, const SpeechAudibilityType defaultValue);
	
	enum SpeechRequestedVolumeType
	{
		SPEECH_VOLUME_UNSPECIFIED = 0,
		SPEECH_VOLUME_NORMAL,
		SPEECH_VOLUME_SHOUTED,
		SPEECH_VOLUME_FRONTEND,
		SPEECH_VOLUME_MEGAPHONE,
		SPEECH_VOLUME_HELI,
		NUM_SPEECHREQUESTEDVOLUMETYPE,
		SPEECHREQUESTEDVOLUMETYPE_MAX = NUM_SPEECHREQUESTEDVOLUMETYPE,
	}; // enum SpeechRequestedVolumeType
	const char* SpeechRequestedVolumeType_ToString(const SpeechRequestedVolumeType value);
	SpeechRequestedVolumeType SpeechRequestedVolumeType_Parse(const char* str, const SpeechRequestedVolumeType defaultValue);
	
	enum SpeechResolvingFunction
	{
		SRF_DEFAULT = 0,
		SRF_TIME_OF_DAY,
		SRF_PED_TOUGHNESS,
		SRF_TARGET_GENDER,
		NUM_SPEECHRESOLVINGFUNCTION,
		SPEECHRESOLVINGFUNCTION_MAX = NUM_SPEECHRESOLVINGFUNCTION,
	}; // enum SpeechResolvingFunction
	const char* SpeechResolvingFunction_ToString(const SpeechResolvingFunction value);
	SpeechResolvingFunction SpeechResolvingFunction_Parse(const char* str, const SpeechResolvingFunction defaultValue);
	
	enum InteriorRainType
	{
		RAIN_TYPE_NONE = 0,
		RAIN_TYPE_CORRUGATED_IRON,
		RAIN_TYPE_PLASTIC,
		NUM_INTERIORRAINTYPE,
		INTERIORRAINTYPE_MAX = NUM_INTERIORRAINTYPE,
	}; // enum InteriorRainType
	const char* InteriorRainType_ToString(const InteriorRainType value);
	InteriorRainType InteriorRainType_Parse(const char* str, const InteriorRainType defaultValue);
	
	enum InteriorType
	{
		INTERIOR_TYPE_NONE = 0,
		INTERIOR_TYPE_ROAD_TUNNEL,
		INTERIOR_TYPE_SUBWAY_TUNNEL,
		INTERIOR_TYPE_SUBWAY_STATION,
		INTERIOR_TYPE_SUBWAY_ENTRANCE,
		INTERIOR_TYPE_ABANDONED_WAREHOUSE,
		NUM_INTERIORTYPE,
		INTERIORTYPE_MAX = NUM_INTERIORTYPE,
	}; // enum InteriorType
	const char* InteriorType_ToString(const InteriorType value);
	InteriorType InteriorType_Parse(const char* str, const InteriorType defaultValue);
	
	enum ClimbSounds
	{
		CLIMB_SOUNDS_CONCRETE = 0,
		CLIMB_SOUNDS_CHAINLINK,
		CLIMB_SOUNDS_METAL,
		CLIMB_SOUNDS_WOOD,
		NUM_CLIMBSOUNDS,
		CLIMBSOUNDS_MAX = NUM_CLIMBSOUNDS,
	}; // enum ClimbSounds
	const char* ClimbSounds_ToString(const ClimbSounds value);
	ClimbSounds ClimbSounds_Parse(const char* str, const ClimbSounds defaultValue);
	
	enum AudioWeight
	{
		AUDIO_WEIGHT_VL = 0,
		AUDIO_WEIGHT_L,
		AUDIO_WEIGHT_M,
		AUDIO_WEIGHT_H,
		AUDIO_WEIGHT_VH,
		NUM_AUDIOWEIGHT,
		AUDIOWEIGHT_MAX = NUM_AUDIOWEIGHT,
	}; // enum AudioWeight
	const char* AudioWeight_ToString(const AudioWeight value);
	AudioWeight AudioWeight_Parse(const char* str, const AudioWeight defaultValue);
	
	enum AmbientRuleHeight
	{
		AMBIENCE_HEIGHT_RANDOM = 0,
		AMBIENCE_LISTENER_HEIGHT,
		AMBIENCE_WORLD_BLANKET_HEIGHT,
		AMBIENCE_HEIGHT_WORLD_BLANKET_HEIGHT_PLUS_EXPLICIT,
		AMBIENCE_LISTENER_HEIGHT_HEIGHT_PLUS_EXPLICIT,
		AMBIENCE_HEIGHT_PZ_FRACTION,
		NUM_AMBIENTRULEHEIGHT,
		AMBIENTRULEHEIGHT_MAX = NUM_AMBIENTRULEHEIGHT,
	}; // enum AmbientRuleHeight
	const char* AmbientRuleHeight_ToString(const AmbientRuleHeight value);
	AmbientRuleHeight AmbientRuleHeight_Parse(const char* str, const AmbientRuleHeight defaultValue);
	
	enum AmbientRuleExplicitPositionType
	{
		RULE_EXPLICIT_SPAWN_POS_DISABLED = 0,
		RULE_EXPLICIT_SPAWN_POS_WORLD_RELATIVE,
		RULE_EXPLICIT_SPAWN_POS_INTERIOR_RELATIVE,
		NUM_AMBIENTRULEEXPLICITPOSITIONTYPE,
		AMBIENTRULEEXPLICITPOSITIONTYPE_MAX = NUM_AMBIENTRULEEXPLICITPOSITIONTYPE,
	}; // enum AmbientRuleExplicitPositionType
	const char* AmbientRuleExplicitPositionType_ToString(const AmbientRuleExplicitPositionType value);
	AmbientRuleExplicitPositionType AmbientRuleExplicitPositionType_Parse(const char* str, const AmbientRuleExplicitPositionType defaultValue);
	
	enum AmbientRuleConditionTypes
	{
		RULE_IF_CONDITION_LESS_THAN = 0,
		RULE_IF_CONDITION_LESS_THAN_OR_EQUAL_TO,
		RULE_IF_CONDITION_GREATER_THAN,
		RULE_IF_CONDITION_GREATER_THAN_OR_EQUAL_TO,
		RULE_IF_CONDITION_EQUAL_TO,
		RULE_IF_CONDITION_NOT_EQUAL_TO,
		NUM_AMBIENTRULECONDITIONTYPES,
		AMBIENTRULECONDITIONTYPES_MAX = NUM_AMBIENTRULECONDITIONTYPES,
	}; // enum AmbientRuleConditionTypes
	const char* AmbientRuleConditionTypes_ToString(const AmbientRuleConditionTypes value);
	AmbientRuleConditionTypes AmbientRuleConditionTypes_Parse(const char* str, const AmbientRuleConditionTypes defaultValue);
	
	enum AmbientRuleBankLoad
	{
		INFLUENCE_BANK_LOAD = 0,
		DONT_INFLUENCE_BANK_LOAD,
		NUM_AMBIENTRULEBANKLOAD,
		AMBIENTRULEBANKLOAD_MAX = NUM_AMBIENTRULEBANKLOAD,
	}; // enum AmbientRuleBankLoad
	const char* AmbientRuleBankLoad_ToString(const AmbientRuleBankLoad value);
	AmbientRuleBankLoad AmbientRuleBankLoad_Parse(const char* str, const AmbientRuleBankLoad defaultValue);
	
	enum PedTypes
	{
		HUMAN = 0,
		DOG,
		DEER,
		BOAR,
		COYOTE,
		MTLION,
		PIG,
		CHIMP,
		COW,
		ROTTWEILER,
		ELK,
		RETRIEVER,
		RAT,
		BIRD,
		FISH,
		SMALL_MAMMAL,
		SMALL_DOG,
		CAT,
		RABBIT,
		NUM_PEDTYPES,
		PEDTYPES_MAX = NUM_PEDTYPES,
	}; // enum PedTypes
	const char* PedTypes_ToString(const PedTypes value);
	PedTypes PedTypes_Parse(const char* str, const PedTypes defaultValue);
	
	enum CarEngineTypes
	{
		COMBUSTION = 0,
		ELECTRIC,
		HYBRID,
		BLEND,
		NUM_CARENGINETYPES,
		CARENGINETYPES_MAX = NUM_CARENGINETYPES,
	}; // enum CarEngineTypes
	const char* CarEngineTypes_ToString(const CarEngineTypes value);
	CarEngineTypes CarEngineTypes_Parse(const char* str, const CarEngineTypes defaultValue);
	
	enum HumanPedTypes
	{
		AUD_PEDTYPE_MICHAEL = 0,
		AUD_PEDTYPE_FRANKLIN,
		AUD_PEDTYPE_TREVOR,
		AUD_PEDTYPE_MAN,
		AUD_PEDTYPE_WOMAN,
		AUD_PEDTYPE_GANG,
		AUD_PEDTYPE_COP,
		NUM_HUMANPEDTYPES,
		HUMANPEDTYPES_MAX = NUM_HUMANPEDTYPES,
	}; // enum HumanPedTypes
	const char* HumanPedTypes_ToString(const HumanPedTypes value);
	HumanPedTypes HumanPedTypes_Parse(const char* str, const HumanPedTypes defaultValue);
	
	enum ShoeTypes
	{
		SHOE_TYPE_BARE = 0,
		SHOE_TYPE_BOOTS,
		SHOE_TYPE_SHOES,
		SHOE_TYPE_HIGH_HEELS,
		SHOE_TYPE_FLIPFLOPS,
		SHOE_TYPE_FLIPPERS,
		SHOE_TYPE_TRAINERS,
		SHOE_TYPE_CLOWN,
		SHOE_TYPE_GOLF,
		NUM_SHOETYPES,
		SHOETYPES_MAX = NUM_SHOETYPES,
	}; // enum ShoeTypes
	const char* ShoeTypes_ToString(const ShoeTypes value);
	ShoeTypes ShoeTypes_Parse(const char* str, const ShoeTypes defaultValue);
	
	enum DoorTypes
	{
		AUD_DOOR_TYPE_SLIDING_HORIZONTAL = 0,
		AUD_DOOR_TYPE_SLIDING_VERTICAL,
		AUD_DOOR_TYPE_GARAGE,
		AUD_DOOR_TYPE_BARRIER_ARM,
		AUD_DOOR_TYPE_STD,
		NUM_DOORTYPES,
		DOORTYPES_MAX = NUM_DOORTYPES,
	}; // enum DoorTypes
	const char* DoorTypes_ToString(const DoorTypes value);
	DoorTypes DoorTypes_Parse(const char* str, const DoorTypes defaultValue);
	
	enum WaterType
	{
		AUD_WATER_TYPE_POOL = 0,
		AUD_WATER_TYPE_RIVER,
		AUD_WATER_TYPE_LAKE,
		AUD_WATER_TYPE_OCEAN,
		NUM_WATERTYPE,
		WATERTYPE_MAX = NUM_WATERTYPE,
	}; // enum WaterType
	const char* WaterType_ToString(const WaterType value);
	WaterType WaterType_Parse(const char* str, const WaterType defaultValue);
	
	enum LakeSize
	{
		AUD_LAKE_SMALL = 0,
		AUD_LAKE_MEDIUM,
		AUD_LAKE_LARGE,
		NUM_LAKESIZE,
		LAKESIZE_MAX = NUM_LAKESIZE,
	}; // enum LakeSize
	const char* LakeSize_ToString(const LakeSize value);
	LakeSize LakeSize_Parse(const char* str, const LakeSize defaultValue);
	
	enum RiverType
	{
		AUD_RIVER_BROOK_WEAK = 0,
		AUD_RIVER_BROOK_STRONG,
		AUD_RIVER_LA_WEAK,
		AUD_RIVER_LA_STRONG,
		AUD_RIVER_WEAK,
		AUD_RIVER_MEDIUM,
		AUD_RIVER_STRONG,
		AUD_RIVER_RAPIDS_WEAK,
		AUD_RIVER_RAPIDS_STRONG,
		NUM_RIVERTYPE,
		RIVERTYPE_MAX = NUM_RIVERTYPE,
	}; // enum RiverType
	const char* RiverType_ToString(const RiverType value);
	RiverType RiverType_Parse(const char* str, const RiverType defaultValue);
	
	enum OceanWaterType
	{
		AUD_OCEAN_TYPE_BEACH = 0,
		AUD_OCEAN_TYPE_ROCKS,
		AUD_OCEAN_TYPE_PORT,
		AUD_OCEAN_TYPE_PIER,
		NUM_OCEANWATERTYPE,
		OCEANWATERTYPE_MAX = NUM_OCEANWATERTYPE,
	}; // enum OceanWaterType
	const char* OceanWaterType_ToString(const OceanWaterType value);
	OceanWaterType OceanWaterType_Parse(const char* str, const OceanWaterType defaultValue);
	
	enum OceanWaterDirection
	{
		AUD_OCEAN_DIR_NORTH = 0,
		AUD_OCEAN_DIR_NORTH_EAST,
		AUD_OCEAN_DIR_EAST,
		AUD_OCEAN_DIR_SOUTH_EAST,
		AUD_OCEAN_DIR_SOUTH,
		AUD_OCEAN_DIR_SOUTH_WEST,
		AUD_OCEAN_DIR_WEST,
		AUD_OCEAN_DIR_NORTH_WEST,
		NUM_OCEANWATERDIRECTION,
		OCEANWATERDIRECTION_MAX = NUM_OCEANWATERDIRECTION,
	}; // enum OceanWaterDirection
	const char* OceanWaterDirection_ToString(const OceanWaterDirection value);
	OceanWaterDirection OceanWaterDirection_Parse(const char* str, const OceanWaterDirection defaultValue);
	
	enum MicrophoneType
	{
		MIC_FOLLOW_PED = 0,
		MIC_FOLLOW_VEHICLE,
		MIC_C_CHASE_HELI,
		MIC_C_IDLE,
		MIC_C_TRAIN_TRACK,
		MIC_SNIPER_RIFLE,
		MIC_DEBUG,
		MIC_PLAYER_HEAD,
		MIC_C_DEFAULT,
		MIC_C_MANCAM,
		MIC_VEH_BONNET,
		NUM_MICROPHONETYPE,
		MICROPHONETYPE_MAX = NUM_MICROPHONETYPE,
	}; // enum MicrophoneType
	const char* MicrophoneType_ToString(const MicrophoneType value);
	MicrophoneType MicrophoneType_Parse(const char* str, const MicrophoneType defaultValue);
	
	enum MicAttenuationType
	{
		REAR_ATTENUATION_DEFAULT = 0,
		REAR_ATTENUATION_ALWAYS,
		NUM_MICATTENUATIONTYPE,
		MICATTENUATIONTYPE_MAX = NUM_MICATTENUATIONTYPE,
	}; // enum MicAttenuationType
	const char* MicAttenuationType_ToString(const MicAttenuationType value);
	MicAttenuationType MicAttenuationType_Parse(const char* str, const MicAttenuationType defaultValue);
	
	enum MaterialType
	{
		HOLLOW_METAL = 0,
		HOLLOW_PLASTIC,
		GLASS,
		OTHER,
		NUM_MATERIALTYPE,
		MATERIALTYPE_MAX = NUM_MATERIALTYPE,
	}; // enum MaterialType
	const char* MaterialType_ToString(const MaterialType value);
	MaterialType MaterialType_Parse(const char* str, const MaterialType defaultValue);
	
	enum SurfacePriority
	{
		GRAVEL = 0,
		SAND,
		GRASS,
		PRIORITY_OTHER,
		TARMAC,
		NUM_SURFACEPRIORITY,
		SURFACEPRIORITY_MAX = NUM_SURFACEPRIORITY,
	}; // enum SurfacePriority
	const char* SurfacePriority_ToString(const SurfacePriority value);
	SurfacePriority SurfacePriority_Parse(const char* str, const SurfacePriority defaultValue);
	
	enum FilterMode
	{
		kLowPass2Pole = 0,
		kHighPass2Pole,
		kBandPass2Pole,
		kBandStop2Pole,
		kLowPass4Pole,
		kHighPass4Pole,
		kBandPass4Pole,
		kBandStop4Pole,
		kPeakingEQ,
		kLowShelf2Pole,
		kLowShelf4Pole,
		kHighShelf2Pole,
		kHighShelf4Pole,
		NUM_FILTERMODE,
		FILTERMODE_MAX = NUM_FILTERMODE,
	}; // enum FilterMode
	const char* FilterMode_ToString(const FilterMode value);
	FilterMode FilterMode_Parse(const char* str, const FilterMode defaultValue);
	
	enum SlowMoType
	{
		AUD_SLOWMO_GENERAL = 0,
		AUD_SLOWMO_CINEMATIC,
		AUD_SLOWMO_THIRD_PERSON_CINEMATIC_AIM,
		AUD_SLOWMO_STUNT,
		AUD_SLOWMO_WEAPON,
		AUD_SLOWMO_SWITCH,
		AUD_SLOWMO_SPECIAL,
		AUD_SLOWMO_CUSTOM,
		AUD_SLOWMO_RADIOWHEEL,
		AUD_SLOWMO_PAUSEMENU,
		NUM_SLOWMOTYPE,
		SLOWMOTYPE_MAX = NUM_SLOWMOTYPE,
	}; // enum SlowMoType
	const char* SlowMoType_ToString(const SlowMoType value);
	SlowMoType SlowMoType_Parse(const char* str, const SlowMoType defaultValue);
	
	enum ClatterType
	{
		AUD_CLATTER_TRUCK_CAB = 0,
		AUD_CLATTER_TRUCK_CAGES,
		AUD_CLATTER_TRUCK_TRAILER_CHAINS,
		AUD_CLATTER_TAXI,
		AUD_CLATTER_BUS,
		AUD_CLATTER_TRANSIT,
		AUD_CLATTER_TRANSIT_CLOWN,
		AUD_CLATTER_DETAIL,
		AUD_CLATTER_NONE,
		NUM_CLATTERTYPE,
		CLATTERTYPE_MAX = NUM_CLATTERTYPE,
	}; // enum ClatterType
	const char* ClatterType_ToString(const ClatterType value);
	ClatterType ClatterType_Parse(const char* str, const ClatterType defaultValue);
	
	enum RandomDamageClass
	{
		RANDOM_DAMAGE_ALWAYS = 0,
		RANDOM_DAMAGE_WORKHORSE,
		RANDOM_DAMAGE_OCCASIONAL,
		RANDOM_DAMAGE_NONE,
		NUM_RANDOMDAMAGECLASS,
		RANDOMDAMAGECLASS_MAX = NUM_RANDOMDAMAGECLASS,
	}; // enum RandomDamageClass
	const char* RandomDamageClass_ToString(const RandomDamageClass value);
	RandomDamageClass RandomDamageClass_Parse(const char* str, const RandomDamageClass defaultValue);
	
	enum AmbientZoneWaterType
	{
		AMBIENT_ZONE_FORCE_OVER_WATER = 0,
		AMBIENT_ZONE_FORCE_OVER_LAND,
		AMBIENT_ZONE_USE_SHORELINES,
		NUM_AMBIENTZONEWATERTYPE,
		AMBIENTZONEWATERTYPE_MAX = NUM_AMBIENTZONEWATERTYPE,
	}; // enum AmbientZoneWaterType
	const char* AmbientZoneWaterType_ToString(const AmbientZoneWaterType value);
	AmbientZoneWaterType AmbientZoneWaterType_Parse(const char* str, const AmbientZoneWaterType defaultValue);
	
	enum RevLimiterApplyType
	{
		REV_LIMITER_ENGINE_ONLY = 0,
		REV_LIMITER_EXHAUST_ONLY,
		REV_LIMITER_BOTH_CHANNELS,
		NUM_REVLIMITERAPPLYTYPE,
		REVLIMITERAPPLYTYPE_MAX = NUM_REVLIMITERAPPLYTYPE,
	}; // enum RevLimiterApplyType
	const char* RevLimiterApplyType_ToString(const RevLimiterApplyType value);
	RevLimiterApplyType RevLimiterApplyType_Parse(const char* str, const RevLimiterApplyType defaultValue);
	
	enum FakeGesture
	{
		kAbsolutelyGesture = 0,
		kAgreeGesture,
		kComeHereFrontGesture,
		kComeHereLeftGesture,
		kDamnGesture,
		kDontKnowGesture,
		kEasyNowGesture,
		kExactlyGesture,
		kForgetItGesture,
		kGoodGesture,
		kHelloGesture,
		kHeyGesture,
		kIDontThinkSoGesture,
		kIfYouSaySoGesture,
		kIllDoItGesture,
		kImNotSureGesture,
		kImSorryGesture,
		kIndicateBackGesture,
		kIndicateLeftGesture,
		kIndicateRightGesture,
		kNoChanceGesture,
		kYesGesture,
		kYouUnderstandGesture,
		NUM_FAKEGESTURE,
		FAKEGESTURE_MAX = NUM_FAKEGESTURE,
	}; // enum FakeGesture
	const char* FakeGesture_ToString(const FakeGesture value);
	FakeGesture FakeGesture_Parse(const char* str, const FakeGesture defaultValue);
	
	enum ShellCasingType
	{
		SHELLCASING_METAL_SMALL = 0,
		SHELLCASING_METAL,
		SHELLCASING_METAL_LARGE,
		SHELLCASING_PLASTIC,
		SHELLCASING_NONE,
		NUM_SHELLCASINGTYPE,
		SHELLCASINGTYPE_MAX = NUM_SHELLCASINGTYPE,
	}; // enum ShellCasingType
	const char* ShellCasingType_ToString(const ShellCasingType value);
	ShellCasingType ShellCasingType_Parse(const char* str, const ShellCasingType defaultValue);
	
	enum FootstepLoudness
	{
		FOOTSTEP_LOUDNESS_SILENT = 0,
		FOOTSTEP_LOUDNESS_QUIET,
		FOOTSTEP_LOUDNESS_MEDIUM,
		FOOTSTEP_LOUDNESS_LOUD,
		NUM_FOOTSTEPLOUDNESS,
		FOOTSTEPLOUDNESS_MAX = NUM_FOOTSTEPLOUDNESS,
	}; // enum FootstepLoudness
	const char* FootstepLoudness_ToString(const FootstepLoudness value);
	FootstepLoudness FootstepLoudness_Parse(const char* str, const FootstepLoudness defaultValue);
	
	enum TrackCats
	{
		RADIO_TRACK_CAT_ADVERTS = 0,
		RADIO_TRACK_CAT_IDENTS,
		RADIO_TRACK_CAT_MUSIC,
		RADIO_TRACK_CAT_NEWS,
		RADIO_TRACK_CAT_WEATHER,
		RADIO_TRACK_CAT_DJSOLO,
		RADIO_TRACK_CAT_TO_NEWS,
		RADIO_TRACK_CAT_TO_AD,
		RADIO_TRACK_CAT_USER_INTRO,
		RADIO_TRACK_CAT_USER_OUTRO,
		RADIO_TRACK_CAT_INTRO_MORNING,
		RADIO_TRACK_CAT_INTRO_EVENING,
		RADIO_TRACK_CAT_TAKEOVER_MUSIC,
		RADIO_TRACK_CAT_TAKEOVER_IDENTS,
		RADIO_TRACK_CAT_TAKEOVER_DJSOLO,
		NUM_RADIO_TRACK_CATS,
		NUM_TRACKCATS,
		TRACKCATS_MAX = NUM_TRACKCATS,
	}; // enum TrackCats
	const char* TrackCats_ToString(const TrackCats value);
	TrackCats TrackCats_Parse(const char* str, const TrackCats defaultValue);
	
	enum PVGBackupType
	{
		kMaleCowardly = 0,
		kMaleBrave,
		kMaleGang,
		kFemale,
		kCop,
		NUM_PVGBACKUPTYPE,
		PVGBACKUPTYPE_MAX = NUM_PVGBACKUPTYPE,
	}; // enum PVGBackupType
	const char* PVGBackupType_ToString(const PVGBackupType value);
	PVGBackupType PVGBackupType_Parse(const char* str, const PVGBackupType defaultValue);
	
	enum ScannerSlotType
	{
		LARGE_SCANNER_SLOT = 0,
		SMALL_SCANNER_SLOT,
		NUM_SCANNERSLOTTYPE,
		SCANNERSLOTTYPE_MAX = NUM_SCANNERSLOTTYPE,
	}; // enum ScannerSlotType
	const char* ScannerSlotType_ToString(const ScannerSlotType value);
	ScannerSlotType ScannerSlotType_Parse(const char* str, const ScannerSlotType defaultValue);
	
	enum AmbientZoneShape
	{
		kAmbientZoneCuboid = 0,
		kAmbientZoneSphere,
		kAmbientZoneCuboidLineEmitter,
		kAmbientZoneSphereLineEmitter,
		NUM_AMBIENTZONESHAPE,
		AMBIENTZONESHAPE_MAX = NUM_AMBIENTZONESHAPE,
	}; // enum AmbientZoneShape
	const char* AmbientZoneShape_ToString(const AmbientZoneShape value);
	AmbientZoneShape AmbientZoneShape_Parse(const char* str, const AmbientZoneShape defaultValue);
	
	enum BankingStyle
	{
		kRotationAngle = 0,
		kRotationSpeed,
		NUM_BANKINGSTYLE,
		BANKINGSTYLE_MAX = NUM_BANKINGSTYLE,
	}; // enum BankingStyle
	const char* BankingStyle_ToString(const BankingStyle value);
	BankingStyle BankingStyle_Parse(const char* str, const BankingStyle defaultValue);
	
	enum MusicConstraintTypes
	{
		kConstrainStart = 0,
		kConstrainEnd,
		NUM_MUSICCONSTRAINTTYPES,
		MUSICCONSTRAINTTYPES_MAX = NUM_MUSICCONSTRAINTTYPES,
	}; // enum MusicConstraintTypes
	const char* MusicConstraintTypes_ToString(const MusicConstraintTypes value);
	MusicConstraintTypes MusicConstraintTypes_Parse(const char* str, const MusicConstraintTypes defaultValue);
	
	enum MusicArea
	{
		kMusicAreaEverywhere = 0,
		kMusicAreaCity,
		kMusicAreaCountry,
		NUM_MUSICAREA,
		MUSICAREA_MAX = NUM_MUSICAREA,
	}; // enum MusicArea
	const char* MusicArea_ToString(const MusicArea value);
	MusicArea MusicArea_Parse(const char* str, const MusicArea defaultValue);
	
	enum SyncMarkerType
	{
		kNoSyncMarker = 0,
		kAnyMarker,
		kBeatMarker,
		kBarMarker,
		NUM_SYNCMARKERTYPE,
		SYNCMARKERTYPE_MAX = NUM_SYNCMARKERTYPE,
	}; // enum SyncMarkerType
	const char* SyncMarkerType_ToString(const SyncMarkerType value);
	SyncMarkerType SyncMarkerType_Parse(const char* str, const SyncMarkerType defaultValue);
	
	enum AnimalType
	{
		kAnimalNone = 0,
		kAnimalBoar,
		kAnimalChicken,
		kAnimalDog,
		kAnimalRottweiler,
		kAnimalHorse,
		kAnimalCow,
		kAnimalCoyote,
		kAnimalDeer,
		kAnimalEagle,
		kAnimalFish,
		kAnimalHen,
		kAnimalMonkey,
		kAnimalMountainLion,
		kAnimalPigeon,
		kAnimalRat,
		kAnimalSeagull,
		kAnimalCrow,
		kAnimalPig,
		kAnimalChickenhawk,
		kAnimalCormorant,
		kAnimalRhesus,
		kAnimalRetriever,
		kAnimalChimp,
		kAnimalHusky,
		kAnimalShepherd,
		kAnimalCat,
		kAnimalWhale,
		kAnimalDolphin,
		kAnimalSmallDog,
		kAnimalHammerhead,
		kAnimalStingray,
		kAnimalRabbit,
		kAnimalOrca,
		NUM_ANIMALTYPE,
		ANIMALTYPE_MAX = NUM_ANIMALTYPE,
	}; // enum AnimalType
	const char* AnimalType_ToString(const AnimalType value);
	AnimalType AnimalType_Parse(const char* str, const AnimalType defaultValue);
	
	enum RadioDjSpeechCategories
	{
		kDjSpeechIntro = 0,
		kDjSpeechOutro,
		kDjSpeechGeneral,
		kDjSpeechTime,
		kDjSpeechTo,
		NUM_RADIODJSPEECHCATEGORIES,
		RADIODJSPEECHCATEGORIES_MAX = NUM_RADIODJSPEECHCATEGORIES,
	}; // enum RadioDjSpeechCategories
	const char* RadioDjSpeechCategories_ToString(const RadioDjSpeechCategories value);
	RadioDjSpeechCategories RadioDjSpeechCategories_Parse(const char* str, const RadioDjSpeechCategories defaultValue);
	
	// disable struct alignment
	#if !__SPU
	#pragma pack(push, r1, 1)
	#endif // !__SPU
		// 
		// PedVoiceGroups
		// 
		enum PedVoiceGroupsFlagIds
		{
			FLAG_ID_PEDVOICEGROUPS_MAX = 0,
		}; // enum PedVoiceGroupsFlagIds
		
		struct PedVoiceGroups
		{
			static const rage::u32 TYPE_ID = 30;
			static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
			
			PedVoiceGroups() :
				ClassId(0xFF),
				NameTableOffset(0XFFFFFF),
				PVGType(kMaleCowardly),
				VoicePriority(3),
				RingtoneSounds(4114177648U)
			{}
			
			// PURPOSE - Returns a pointer to the field whose name is specified by the hash
			void* GetFieldPtr(const rage::u32 fieldNameHash);
			
			rage::u32 ClassId : 8;
			rage::u32 NameTableOffset : 24;
			
			rage::u8 PVGType;
			
			struct tPVGBits
			{
				union
				{
					rage::u8 Value;
					struct
					{
						#if __BE
							bool padding:7; // padding
							bool Populated:1;
						#else // __BE
							bool Populated:1;
							bool padding:7; // padding
						#endif // __BE
					} BitFields;
				};
			} PVGBits; // struct tPVGBits
			
			rage::u8 VoicePriority;
			rage::u32 RingtoneSounds;
			
			static const rage::u8 MAX_PRIMARYVOICES = 16;
			rage::u8 numPrimaryVoices;
			struct tPrimaryVoice
			{
				rage::u32 VoiceName;
				rage::u32 ReferenceCount;
				rage::u32 RunningTab;
			} PrimaryVoice[MAX_PRIMARYVOICES]; // struct tPrimaryVoice
			
			
			static const rage::u8 MAX_MINIVOICES = 16;
			rage::u8 numMiniVoices;
			struct tMiniVoice
			{
				rage::u32 VoiceName;
				rage::u32 ReferenceCount;
				rage::u32 RunningTab;
			} MiniVoice[MAX_MINIVOICES]; // struct tMiniVoice
			
			
			static const rage::u8 MAX_GANGVOICES = 16;
			rage::u8 numGangVoices;
			struct tGangVoice
			{
				rage::u32 VoiceName;
				rage::u32 ReferenceCount;
				rage::u32 RunningTab;
			} GangVoice[MAX_GANGVOICES]; // struct tGangVoice
			
			
			static const rage::u8 MAX_BACKUPPVGS = 16;
			rage::u8 numBackupPVGs;
			struct tBackupPVG
			{
				rage::u32 PVG;
			} BackupPVG[MAX_BACKUPPVGS]; // struct tBackupPVG
			
		} SPU_ONLY(__attribute__((packed))); // struct PedVoiceGroups
		
		// 
		// AnimalParams
		// 
		enum AnimalParamsFlagIds
		{
			FLAG_ID_ANIMALPARAMS_MAX = 0,
		}; // enum AnimalParamsFlagIds
		
		struct AnimalParams
		{
			static const rage::u32 TYPE_ID = 73;
			static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
			
			AnimalParams() :
				ClassId(0xFF),
				NameTableOffset(0XFFFFFF),
				BasePriority(5),
				MinFarDistance(30.0f),
				MaxDistanceToBankLoad(60.0f),
				MaxSoundInstances(4U),
				Type(kAnimalNone),
				ScrapeVolCurve(4185979440U), // COLLISION_ANIMAL_SCRAPE_VOLUME
				ScrapePitchCurve(1887668371U), // COLLISION_ANIMAL_SCRAPE_PITCH
				BigVehicleImpact(0U),
				VehicleSpeedForBigImpact(5.0f),
				RunOverSound(0U)
			{}
			
			// PURPOSE - Returns a pointer to the field whose name is specified by the hash
			void* GetFieldPtr(const rage::u32 fieldNameHash);
			
			rage::u32 ClassId : 8;
			rage::u32 NameTableOffset : 24;
			
			rage::s32 BasePriority;
			float MinFarDistance;
			float MaxDistanceToBankLoad;
			rage::u32 MaxSoundInstances;
			rage::u8 Type;
			rage::u32 ScrapeVolCurve;
			rage::u32 ScrapePitchCurve;
			rage::u32 BigVehicleImpact;
			float VehicleSpeedForBigImpact;
			rage::u32 RunOverSound;
			
			static const rage::u8 MAX_CONTEXTS = 32;
			rage::u8 numContexts;
			struct tContext
			{
				char ContextName[32];
				float VolumeOffset;
				float RollOff;
				rage::u32 Priority;
				float Probability;
				rage::s32 MinTimeBetween;
				rage::s32 MaxTimeBetween;
				float MaxDistance;
				
				struct tContextBits
				{
					union
					{
						rage::u8 Value;
						struct
						{
							#if __BE
								bool padding:6; // padding
								bool AllowOverlap:1;
								bool SwapWhenUsedUp:1;
							#else // __BE
								bool SwapWhenUsedUp:1;
								bool AllowOverlap:1;
								bool padding:6; // padding
							#endif // __BE
						} BitFields;
					};
				} ContextBits; // struct tContextBits
				
			} Context[MAX_CONTEXTS]; // struct tContext
			
		} SPU_ONLY(__attribute__((packed))); // struct AnimalParams
		
		// 
		// AnimalVocalAnimTrigger
		// 
		enum AnimalVocalAnimTriggerFlagIds
		{
			FLAG_ID_ANIMALVOCALANIMTRIGGER_MAX = 0,
		}; // enum AnimalVocalAnimTriggerFlagIds
		
		struct AnimalVocalAnimTrigger
		{
			static const rage::u32 TYPE_ID = 74;
			static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
			
			AnimalVocalAnimTrigger() :
				ClassId(0xFF),
				NameTableOffset(0XFFFFFF)
			{}
			
			// PURPOSE - Returns a pointer to the field whose name is specified by the hash
			void* GetFieldPtr(const rage::u32 fieldNameHash);
			
			rage::u32 ClassId : 8;
			rage::u32 NameTableOffset : 24;
			
			
			static const rage::u8 MAX_ANIMTRIGGERS = 32;
			rage::u8 numAnimTriggers;
			struct tAnimTriggers
			{
				rage::u8 Animal;
				
				static const rage::u8 MAX_ANGRYCONTEXTS = 8;
				rage::u8 numAngryContexts;
				struct tAngryContexts
				{
					rage::u32 Context;
					float Weight;
				} AngryContexts[MAX_ANGRYCONTEXTS]; // struct tAngryContexts
				
				
				static const rage::u8 MAX_PLAYFULCONTEXTS = 8;
				rage::u8 numPlayfulContexts;
				struct tPlayfulContexts
				{
					rage::u32 Context;
					float Weight;
				} PlayfulContexts[MAX_PLAYFULCONTEXTS]; // struct tPlayfulContexts
				
			} AnimTriggers[MAX_ANIMTRIGGERS]; // struct tAnimTriggers
			
		} SPU_ONLY(__attribute__((packed))); // struct AnimalVocalAnimTrigger
		
	// enable struct alignment
	#if !__SPU
	#pragma pack(pop, r1)
	#endif // !__SPU
	// 
	// AudBaseObject
	// 
	enum AudBaseObjectFlagIds
	{
		FLAG_ID_AUDBASEOBJECT_MAX = 0,
	}; // enum AudBaseObjectFlagIds
	
	struct AudBaseObject
	{
		static const rage::u32 TYPE_ID = 0;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AudBaseObject() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
	}; // struct AudBaseObject
	
	// 
	// VehicleCollisionSettings
	// 
	enum VehicleCollisionSettingsFlagIds
	{
		FLAG_ID_VEHICLECOLLISIONSETTINGS_MAX = 0,
	}; // enum VehicleCollisionSettingsFlagIds
	
	struct VehicleCollisionSettings
	{
		static const rage::u32 TYPE_ID = 1;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		VehicleCollisionSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			MediumIntensity(0U),
			HighIntensity(0U),
			SmallScrapeImpact(3817852694U), // NULL_SOUND
			SlowScrapeImpact(3817852694U), // NULL_SOUND
			SlowScrapeLoop(3817852694U), // NULL_SOUND
			RollSound(3817852694U), // NULL_SOUND
			VehOnVehCrashSound(3817852694U), // NULL_SOUND
			HighImpactSweetenerSound(3817852694U), // NULL_SOUND
			SweetenerImpactThreshold(20.0f),
			ScrapePitchCurve(1270168322U), // COLLISION_VEHICLE_SCRAPE_PITCH
			ScrapeVolCurve(1800623179U), // COLLISION_VEHICLE_SCRAPE_VOLUME
			SlowScrapeVolCurve(3839909367U), // COLLISION_VEHICLE_SLOW_SCRAPE_VOLUME
			ScrapeImpactVolCurve(1193600127U), // COLLISION_IMPACT_VOLUME
			SlowScrapeImpactCurve(1193600127U), // COLLISION_IMPACT_VOLUME
			ImpactStartOffsetCurve(218633025U), // COLLISION_IMPACT_START_OFFSET
			ImpactVolCurve(1193600127U), // COLLISION_IMPACT_VOLUME
			VehicleImpactStartOffsetCurve(218633025U), // COLLISION_IMPACT_START_OFFSET
			VehicleImpactVolCurve(1193600127U), // COLLISION_IMPACT_VOLUME
			VelocityImpactScalingCurve(1731435240U), // VEH_VEL_IMPACT_SCALING
			FakeImpactStartOffsetCurve(4077230118U), // VEHICLE_FAKE_IMPACT_START_OFFSET
			FakeImpactVolCurve(77132216U), // VEHICLE_FAKE_IMPACT_VOLUME
			FakeImpactMin(1.0f),
			FakeImpactMax(20.0f),
			FakeImpactScale(1.0f),
			VehicleSizeScale(1.0f),
			FakeImpactTriggerDelta(6.0f),
			FakeImpactSweetenerThreshold(30.0f),
			DamageVolCurve(203783793U), // IMPACT_DAMAGE_VOLUME
			JumpLandVolCurve(4188035607U), // COLLISION_VEHICLE_JUMPLAND_VOLUME
			VehicleMaterialSettings(3279074866U), // AM_CAR_BASE_METAL
			DeformationSound(3817852694U), // NULL_SOUND
			ImpactDebris(3817852694U), // NULL_SOUND
			GlassDebris(3817852694U), // NULL_SOUND
			PostImpactDebris(3817852694U), // NULL_SOUND
			PedCollisionMin(1.0f),
			PedCollisionMax(20.0f),
			CollisionMin(1.0f),
			CollisionMax(20.0f),
			VehicleCollisionMin(0.0f),
			VehicleCollisionMax(1.0f),
			VehicleSweetenerThreshold(10.0f),
			ScrapeMin(1.0f),
			ScrapeMax(20.0f),
			DamageMin(20.0f),
			DamageMax(130.0f),
			TrainImpact(3817852694U), // NULL_SOUND
			TrainImpactLoop(3817852694U), // NULL_SOUND
			WaveImpactLoop(3817852694U), // NULL_SOUND
			FragMaterial(0U),
			WheelFragMaterial(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 MediumIntensity;
		rage::u32 HighIntensity;
		rage::u32 SmallScrapeImpact;
		rage::u32 SlowScrapeImpact;
		rage::u32 SlowScrapeLoop;
		rage::u32 RollSound;
		rage::u32 VehOnVehCrashSound;
		rage::u32 HighImpactSweetenerSound;
		float SweetenerImpactThreshold;
		rage::u32 ScrapePitchCurve;
		rage::u32 ScrapeVolCurve;
		rage::u32 SlowScrapeVolCurve;
		rage::u32 ScrapeImpactVolCurve;
		rage::u32 SlowScrapeImpactCurve;
		rage::u32 ImpactStartOffsetCurve;
		rage::u32 ImpactVolCurve;
		rage::u32 VehicleImpactStartOffsetCurve;
		rage::u32 VehicleImpactVolCurve;
		rage::u32 VelocityImpactScalingCurve;
		rage::u32 FakeImpactStartOffsetCurve;
		rage::u32 FakeImpactVolCurve;
		float FakeImpactMin;
		float FakeImpactMax;
		float FakeImpactScale;
		float VehicleSizeScale;
		float FakeImpactTriggerDelta;
		float FakeImpactSweetenerThreshold;
		rage::u32 DamageVolCurve;
		rage::u32 JumpLandVolCurve;
		rage::u32 VehicleMaterialSettings;
		rage::u32 DeformationSound;
		rage::u32 ImpactDebris;
		rage::u32 GlassDebris;
		rage::u32 PostImpactDebris;
		float PedCollisionMin;
		float PedCollisionMax;
		float CollisionMin;
		float CollisionMax;
		float VehicleCollisionMin;
		float VehicleCollisionMax;
		float VehicleSweetenerThreshold;
		float ScrapeMin;
		float ScrapeMax;
		float DamageMin;
		float DamageMax;
		rage::u32 TrainImpact;
		rage::u32 TrainImpactLoop;
		rage::u32 WaveImpactLoop;
		rage::u32 FragMaterial;
		rage::u32 WheelFragMaterial;
		
		static const rage::u8 MAX_MELEEMATERIALOVERRIDES = 64;
		rage::u8 numMeleeMaterialOverrides;
		struct tMeleeMaterialOverride
		{
			rage::audMetadataRef Material;
			rage::audMetadataRef Override;
		} MeleeMaterialOverride[MAX_MELEEMATERIALOVERRIDES]; // struct tMeleeMaterialOverride
		
	}; // struct VehicleCollisionSettings
	
	// 
	// TrailerAudioSettings
	// 
	enum TrailerAudioSettingsFlagIds
	{
		FLAG_ID_TRAILERAUDIOSETTINGS_MAX = 0,
	}; // enum TrailerAudioSettingsFlagIds
	
	struct TrailerAudioSettings
	{
		static const rage::u32 TYPE_ID = 2;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		TrailerAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			BumpSound(1771950309U), // SUSPENSION_UP_COLLISION
			ClatterType(AUD_CLATTER_NONE),
			LinkStressSound(3817852694U), // NULL_SOUND
			ModelCollisionSettings(0U),
			FireAudio(804661164U), // VEH_FIRE_SOUNDSET
			TrailerBumpVolumeBoost(0),
			ClatterSensitivityScalar(1.0f),
			ClatterVolumeBoost(0),
			ChassisStressSensitivityScalar(1.0f),
			ChassisStressVolumeBoost(0),
			LinkStressVolumeBoost(0),
			LinkStressSensitivityScalar(1.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 BumpSound;
		rage::u8 ClatterType;
		rage::u32 LinkStressSound;
		rage::u32 ModelCollisionSettings;
		rage::u32 FireAudio;
		rage::s32 TrailerBumpVolumeBoost;
		float ClatterSensitivityScalar;
		rage::s32 ClatterVolumeBoost;
		float ChassisStressSensitivityScalar;
		rage::s32 ChassisStressVolumeBoost;
		rage::s32 LinkStressVolumeBoost;
		float LinkStressSensitivityScalar;
	}; // struct TrailerAudioSettings
	
	// 
	// CarAudioSettings
	// 
	enum CarAudioSettingsFlagIds
	{
		FLAG_ID_CARAUDIOSETTINGS_SPORTSCARREVSENABLED = 0,
		FLAG_ID_CARAUDIOSETTINGS_REVERSEWARNING,
		FLAG_ID_CARAUDIOSETTINGS_BIGRIGBRAKES,
		FLAG_ID_CARAUDIOSETTINGS_DOOROPENWARNING,
		FLAG_ID_CARAUDIOSETTINGS_DISABLEAMBIENTRADIO,
		FLAG_ID_CARAUDIOSETTINGS_HEAVYROADNOISE,
		FLAG_ID_CARAUDIOSETTINGS_IAMNOTACAR,
		FLAG_ID_CARAUDIOSETTINGS_TYRECHIRPSENABLED,
		FLAG_ID_CARAUDIOSETTINGS_ISTOYCAR,
		FLAG_ID_CARAUDIOSETTINGS_HASCBRADIO,
		FLAG_ID_CARAUDIOSETTINGS_DISABLESKIDS,
		FLAG_ID_CARAUDIOSETTINGS_CAUSECONTROLLERRUMBLE,
		FLAG_ID_CARAUDIOSETTINGS_MOBILECAUSESRADIOINTERFERENCE,
		FLAG_ID_CARAUDIOSETTINGS_ISKICKSTARTED,
		FLAG_ID_CARAUDIOSETTINGS_HASALARM,
		FLAG_ID_CARAUDIOSETTINGS_MAX,
	}; // enum CarAudioSettingsFlagIds
	
	struct CarAudioSettings
	{
		static const rage::u32 TYPE_ID = 3;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		CarAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			Engine(0U),
			GranularEngine(0U),
			HornSounds(3006832378U), // HORN_LIST_DEFAULT
			DoorOpenSound(3817852694U), // NULL_SOUND
			DoorCloseSound(3817852694U), // NULL_SOUND
			BootOpenSound(0U),
			BootCloseSound(0U),
			RollSound(3817852694U), // NULL_SOUND
			BrakeSqueekFactor(0.5f),
			SuspensionUp(2095958071U), // SUSPENSION_UP
			SuspensionDown(1257191667U), // SUSPENSION_DOWN
			MinSuspCompThresh(0.45f),
			MaxSuspCompThres(6.9f),
			VehicleCollisions(12778311U), // VEHICLE_COLLISION_CAR
			CarMake(0U),
			CarModel(0U),
			CarCategory(0U),
			ScannerVehicleSettings(0U),
			JumpLandSound(2783670720U), // JUMP_LAND_INTACT
			DamagedJumpLandSound(452066476U), // JUMP_LAND_LOOSE
			JumpLandMinThresh(31U),
			JumpLandMaxThresh(36U),
			VolumeCategory(VEHICLE_VOLUME_NORMAL),
			GpsType(GPS_TYPE_NONE),
			RadioType(RADIO_TYPE_NORMAL),
			RadioGenre(RADIO_GENRE_UNSPECIFIED),
			IndicatorOn(3817852694U), // NULL_SOUND
			IndicatorOff(3817852694U), // NULL_SOUND
			Handbrake(3817852694U), // NULL_SOUND
			GpsVoice(GPS_VOICE_FEMALE),
			AmbientRadioVol(0),
			RadioLeakage(LEAKAGE_MIDS_MEDIUM),
			ParkingTone(1733173069U), // PARKING_TONES
			RoofStuckSound(1442578544U), // AUTOMATIC_ROOF_BROKEN
			FreewayPassbyTyreBumpFront(2036240614U), // HIGHWAY_PASSBY_TYRE_BUMP
			FreewayPassbyTyreBumpBack(2036240614U), // HIGHWAY_PASSBY_TYRE_BUMP
			FireAudio(804661164U), // VEH_FIRE_SOUNDSET
			StartupRevs(3087523646U), // STARTUP_SEQUENCE_DEFAULT
			WindNoise(3817852694U), // NULL_SOUND
			FreewayPassbyTyreBumpFrontSide(4113460614U), // HIGHWAY_PASSBY_TYRE_BUMP_SIDE
			FreewayPassbyTyreBumpBackSide(4113460614U), // HIGHWAY_PASSBY_TYRE_BUMP_SIDE
			MaxRollOffScalePlayer(6.0f),
			MaxRollOffScaleNPC(3.0f),
			ConvertibleRoofSoundSet(0U),
			OffRoadRumbleSoundVolume(0),
			SirenSounds(0U),
			AlternativeGranularEngines(0U),
			AlternativeGranularEngineProbability(0.0f),
			StopStartProb(0U),
			NPCRoadNoise(205482709U), // NPC_ROADNOISE_PASSES_DEFAULT
			NPCRoadNoiseHighway(899548619U), // NPC_ROADNOISE_PASSES_DEFAULT_HIGHWAYS
			ForkliftSounds(0U),
			TurretSounds(0U),
			ClatterType(AUD_CLATTER_NONE),
			DiggerSounds(0U),
			TowTruckSounds(0U),
			EngineType(COMBUSTION),
			ElectricEngine(0U),
			Openness(0.0f),
			ReverseWarningSound(3817852694U), // NULL_SOUND
			RandomDamage(RANDOM_DAMAGE_NONE),
			WindClothSound(3817852694U), // NULL_SOUND
			CarSpecificShutdownSound(3817852694U), // NULL_SOUND
			ClatterSensitivityScalar(1.0f),
			ClatterVolumeBoost(0),
			ChassisStressSensitivityScalar(1.0f),
			ChassisStressVolumeBoost(0),
			VehicleRainSound(3817852694U), // NULL_SOUND
			AdditionalRevsIncreaseSmoothing(0),
			AdditionalRevsDecreaseSmoothing(0),
			AdditionalGearChangeSmoothing(0),
			AdditionalGearChangeSmoothingTime(0),
			ConvertibleRoofInteriorSoundSet(0U),
			VehicleRainSoundInterior(3817852694U), // NULL_SOUND
			CabinToneLoop(0U),
			InteriorViewEngineOpenness(0.0f),
			JumpLandSoundInterior(3817852694U), // NULL_SOUND
			DamagedJumpLandSoundInterior(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 Engine;
		rage::u32 GranularEngine;
		rage::u32 HornSounds;
		rage::u32 DoorOpenSound;
		rage::u32 DoorCloseSound;
		rage::u32 BootOpenSound;
		rage::u32 BootCloseSound;
		rage::u32 RollSound;
		float BrakeSqueekFactor;
		rage::u32 SuspensionUp;
		rage::u32 SuspensionDown;
		float MinSuspCompThresh;
		float MaxSuspCompThres;
		rage::u32 VehicleCollisions;
		rage::u32 CarMake;
		rage::u32 CarModel;
		rage::u32 CarCategory;
		rage::u32 ScannerVehicleSettings;
		rage::u32 JumpLandSound;
		rage::u32 DamagedJumpLandSound;
		rage::u32 JumpLandMinThresh;
		rage::u32 JumpLandMaxThresh;
		rage::u8 VolumeCategory;
		rage::u8 GpsType;
		rage::u8 RadioType;
		rage::u8 RadioGenre;
		rage::u32 IndicatorOn;
		rage::u32 IndicatorOff;
		rage::u32 Handbrake;
		rage::u8 GpsVoice;
		rage::s8 AmbientRadioVol;
		rage::u8 RadioLeakage;
		rage::u32 ParkingTone;
		rage::u32 RoofStuckSound;
		rage::u32 FreewayPassbyTyreBumpFront;
		rage::u32 FreewayPassbyTyreBumpBack;
		rage::u32 FireAudio;
		rage::u32 StartupRevs;
		rage::u32 WindNoise;
		rage::u32 FreewayPassbyTyreBumpFrontSide;
		rage::u32 FreewayPassbyTyreBumpBackSide;
		float MaxRollOffScalePlayer;
		float MaxRollOffScaleNPC;
		rage::u32 ConvertibleRoofSoundSet;
		rage::s32 OffRoadRumbleSoundVolume;
		rage::u32 SirenSounds;
		rage::u32 AlternativeGranularEngines;
		float AlternativeGranularEngineProbability;
		rage::u32 StopStartProb;
		rage::u32 NPCRoadNoise;
		rage::u32 NPCRoadNoiseHighway;
		rage::u32 ForkliftSounds;
		rage::u32 TurretSounds;
		rage::u8 ClatterType;
		rage::u32 DiggerSounds;
		rage::u32 TowTruckSounds;
		rage::u8 EngineType;
		rage::u32 ElectricEngine;
		float Openness;
		rage::u32 ReverseWarningSound;
		rage::u8 RandomDamage;
		rage::u32 WindClothSound;
		rage::u32 CarSpecificShutdownSound;
		float ClatterSensitivityScalar;
		rage::s32 ClatterVolumeBoost;
		float ChassisStressSensitivityScalar;
		rage::s32 ChassisStressVolumeBoost;
		rage::u32 VehicleRainSound;
		rage::u16 AdditionalRevsIncreaseSmoothing;
		rage::u16 AdditionalRevsDecreaseSmoothing;
		rage::u16 AdditionalGearChangeSmoothing;
		rage::u16 AdditionalGearChangeSmoothingTime;
		rage::u32 ConvertibleRoofInteriorSoundSet;
		rage::u32 VehicleRainSoundInterior;
		rage::u32 CabinToneLoop;
		float InteriorViewEngineOpenness;
		rage::u32 JumpLandSoundInterior;
		rage::u32 DamagedJumpLandSoundInterior;
	}; // struct CarAudioSettings
	
	// 
	// VehicleEngineAudioSettings
	// 
	enum VehicleEngineAudioSettingsFlagIds
	{
		FLAG_ID_VEHICLEENGINEAUDIOSETTINGS_MAX = 0,
	}; // enum VehicleEngineAudioSettingsFlagIds
	
	struct VehicleEngineAudioSettings
	{
		static const rage::u32 TYPE_ID = 4;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		VehicleEngineAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			MasterVolume(0),
			MaxConeAttenuation(0),
			FXCompensation(0),
			NonPlayerFXComp(0),
			LowEngineLoop(0U),
			HighEngineLoop(0U),
			LowExhaustLoop(0U),
			HighExhaustLoop(0U),
			RevsOffLoop(0U),
			MinPitch(-500),
			MaxPitch(1000),
			IdleEngineSimpleLoop(0U),
			IdleExhaustSimpleLoop(0U),
			IdleMinPitch(0),
			IdleMaxPitch(1500),
			InductionLoop(0U),
			InductionMinPitch(0),
			InductionMaxPitch(0),
			TurboWhine(3817852694U), // NULL_SOUND
			TurboMinPitch(0),
			TurboMaxPitch(1200),
			DumpValve(3817852694U), // NULL_SOUND
			DumpValveProb(100U),
			TurboSpinupSpeed(100U),
			GearTransLoop(3817852694U), // NULL_SOUND
			GearTransMinPitch(0),
			GearTransMaxPitch(600),
			GTThrottleVol(300U),
			Ignition(652097730U), // IGNITION
			EngineShutdown(3668957718U), // VEHICLES_ENGINE_RESIDENT_SHUT_DOWN_1
			CoolingFan(3817852694U), // NULL_SOUND
			ExhaustPops(0U),
			StartLoop(0U),
			MasterTurboVolume(0),
			MasterTransmissionVolume(0),
			EngineStartUp(3817852694U), // NULL_SOUND
			EngineSynthDef(0U),
			EngineSynthPreset(0U),
			ExhaustSynthDef(0U),
			ExhaustSynthPreset(0U),
			EngineSubmixVoice(1225003942U), // DEFAULT_CAR_ENGINE_SUBMIX_CONTROL
			ExhaustSubmixVoice(1479769906U), // DEFAULT_CAR_EXHAUST_SUBMIX_CONTROL
			UpgradedTransmissionVolumeBoost(0),
			UpgradedGearChangeInt(3817852694U), // NULL_SOUND
			UpgradedGearChangeExt(3817852694U), // NULL_SOUND
			UpgradedEngineVolumeBoost_PostSubmix(0),
			UpgradedEngineSynthDef(0U),
			UpgradedEngineSynthPreset(0U),
			UpgradedExhaustVolumeBoost_PostSubmix(0),
			UpgradedExhaustSynthDef(0U),
			UpgradedExhaustSynthPreset(0U),
			UpgradedDumpValve(3817852694U), // NULL_SOUND
			UpgradedTurboVolumeBoost(0),
			UpgradedGearTransLoop(3817852694U), // NULL_SOUND
			UpgradedTurboWhine(3817852694U), // NULL_SOUND
			UpgradedInductionLoop(3817852694U), // NULL_SOUND
			UpgradedExhaustPops(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::s32 MasterVolume;
		rage::s32 MaxConeAttenuation;
		rage::s32 FXCompensation;
		rage::s32 NonPlayerFXComp;
		rage::u32 LowEngineLoop;
		rage::u32 HighEngineLoop;
		rage::u32 LowExhaustLoop;
		rage::u32 HighExhaustLoop;
		rage::u32 RevsOffLoop;
		rage::s32 MinPitch;
		rage::s32 MaxPitch;
		rage::u32 IdleEngineSimpleLoop;
		rage::u32 IdleExhaustSimpleLoop;
		rage::s32 IdleMinPitch;
		rage::s32 IdleMaxPitch;
		rage::u32 InductionLoop;
		rage::s32 InductionMinPitch;
		rage::s32 InductionMaxPitch;
		rage::u32 TurboWhine;
		rage::s32 TurboMinPitch;
		rage::s32 TurboMaxPitch;
		rage::u32 DumpValve;
		rage::u32 DumpValveProb;
		rage::u32 TurboSpinupSpeed;
		rage::u32 GearTransLoop;
		rage::s32 GearTransMinPitch;
		rage::s32 GearTransMaxPitch;
		rage::u32 GTThrottleVol;
		rage::u32 Ignition;
		rage::u32 EngineShutdown;
		rage::u32 CoolingFan;
		rage::u32 ExhaustPops;
		rage::u32 StartLoop;
		rage::s32 MasterTurboVolume;
		rage::s32 MasterTransmissionVolume;
		rage::u32 EngineStartUp;
		rage::u32 EngineSynthDef;
		rage::u32 EngineSynthPreset;
		rage::u32 ExhaustSynthDef;
		rage::u32 ExhaustSynthPreset;
		rage::u32 EngineSubmixVoice;
		rage::u32 ExhaustSubmixVoice;
		rage::s32 UpgradedTransmissionVolumeBoost;
		rage::u32 UpgradedGearChangeInt;
		rage::u32 UpgradedGearChangeExt;
		rage::s32 UpgradedEngineVolumeBoost_PostSubmix;
		rage::u32 UpgradedEngineSynthDef;
		rage::u32 UpgradedEngineSynthPreset;
		rage::s32 UpgradedExhaustVolumeBoost_PostSubmix;
		rage::u32 UpgradedExhaustSynthDef;
		rage::u32 UpgradedExhaustSynthPreset;
		rage::u32 UpgradedDumpValve;
		rage::s32 UpgradedTurboVolumeBoost;
		rage::u32 UpgradedGearTransLoop;
		rage::u32 UpgradedTurboWhine;
		rage::u32 UpgradedInductionLoop;
		rage::u32 UpgradedExhaustPops;
	}; // struct VehicleEngineAudioSettings
	
	// 
	// CollisionMaterialSettings
	// 
	enum CollisionMaterialSettingsFlagIds
	{
		FLAG_ID_COLLISIONMATERIALSETTINGS_ISSOFTFORLIGHTPROPS = 0,
		FLAG_ID_COLLISIONMATERIALSETTINGS_DRYMATERIAL,
		FLAG_ID_COLLISIONMATERIALSETTINGS_SHOULDPLAYRICOCHET,
		FLAG_ID_COLLISIONMATERIALSETTINGS_ISRESONANT,
		FLAG_ID_COLLISIONMATERIALSETTINGS_TREATASFIXED,
		FLAG_ID_COLLISIONMATERIALSETTINGS_LOOSESURFACE,
		FLAG_ID_COLLISIONMATERIALSETTINGS_SCRAPESFORPEDS,
		FLAG_ID_COLLISIONMATERIALSETTINGS_PEDSARESOLID,
		FLAG_ID_COLLISIONMATERIALSETTINGS_USECUSTOMCURVES,
		FLAG_ID_COLLISIONMATERIALSETTINGS_SHELLCASINGSIGNOREHARDNESS,
		FLAG_ID_COLLISIONMATERIALSETTINGS_SURFACESETTLEISLOOP,
		FLAG_ID_COLLISIONMATERIALSETTINGS_GENERATEPOSTSHOOTOUTDEBRIS,
		FLAG_ID_COLLISIONMATERIALSETTINGS_TRIGGERCLATTERONTOUCH,
		FLAG_ID_COLLISIONMATERIALSETTINGS_MAX,
	}; // enum CollisionMaterialSettingsFlagIds
	
	struct CollisionMaterialSettings
	{
		static const rage::u32 TYPE_ID = 5;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		CollisionMaterialSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			HardImpact(3817852694U), // NULL_SOUND
			SolidImpact(3817852694U), // NULL_SOUND
			SoftImpact(3817852694U), // NULL_SOUND
			ScrapeSound(3817852694U), // NULL_SOUND
			PedScrapeSound(3817852694U), // NULL_SOUND
			BreakSound(3817852694U), // NULL_SOUND
			DestroySound(3817852694U), // NULL_SOUND
			SettleSound(3817852694U), // NULL_SOUND
			BulletImpactSound(3817852694U), // NULL_SOUND
			AutomaticBulletImpactSound(3817852694U), // NULL_SOUND
			ShotgunBulletImpactSound(3817852694U), // NULL_SOUND
			BigVehicleImpactSound(3817852694U), // NULL_SOUND
			PedPunch(2083042351U), // PED_PUNCH_HARD_MESH
			PedKick(3254844223U), // PED_KICK_HARD_MESH
			MediumIntensity(0U),
			HighIntensity(0U),
			Hardness(100),
			MinImpulseMag(0.0f),
			MaxImpulseMag(6.0f),
			ImpulseMagScalar(100),
			MaxScrapeSpeed(24.0f),
			MinScrapeSpeed(1.0f),
			ScrapeImpactMag(3.0f),
			MaxRollSpeed(3.0f),
			MinRollSpeed(0.1f),
			RollImpactMag(3.0f),
			BulletCollisionScaling(100),
			FootstepCustomImpactSound(0U),
			FootstepMaterialHardness(1.0f),
			FootstepMaterialLoudness(1.0f),
			FootstepScaling(50),
			ScuffstepScaling(20),
			FootstepImpactScaling(0),
			ScuffImpactScaling(0),
			SkiSettings(2555456425U), // SKIS_GENERIC
			ImpactStartOffsetCurve(218633025U), // COLLISION_IMPACT_START_OFFSET
			ImpactVolCurve(1193600127U), // COLLISION_IMPACT_VOLUME
			ScrapePitchCurve(2256680303U), // COLLISION_SCRAPE_PITCH
			ScrapeVolCurve(2010537297U), // COLLISION_SCRAPE_VOLUME
			FastTyreRoll(3817852694U), // NULL_SOUND
			DetailTyreRoll(3817852694U), // NULL_SOUND
			MainSkid(4009869971U), // VEHICLES_WHEEL_LOOPS_MAIN_TARMAC_SKID
			SideSkid(1128710436U), // VEHICLES_WHEEL_LOOPS_SIDE_TARMAC_SKID
			MetalShellCasingSmall(3909942000U), // SHELL_CASINGS_METAL_BOUNCE
			MetalShellCasingMedium(3909942000U), // SHELL_CASINGS_METAL_BOUNCE
			MetalShellCasingLarge(3909942000U), // SHELL_CASINGS_METAL_BOUNCE
			MetalShellCasingSmallSlow(3819042694U), // SHELL_CASINGS_METAL_SLOWMO
			MetalShellCasingMediumSlow(3819042694U), // SHELL_CASINGS_METAL_SLOWMO
			MetalShellCasingLargeSlow(3819042694U), // SHELL_CASINGS_METAL_SLOWMO
			PlasticShellCasing(2730589674U), // SHELL_CASINGS_PLASTIC_BOUNCE
			PlasticShellCasingSlow(2730589674U), // SHELL_CASINGS_PLASTIC_BOUNCE
			RollSound(3817852694U), // NULL_SOUND
			RainLoop(3817852694U), // NULL_SOUND
			TyreBump(3874905581U), // TYRE_BUMPS_CONCRETE
			ShockwaveSound(3817852694U), // NULL_SOUND
			RandomAmbient(3817852694U), // NULL_SOUND
			ClimbSettings(0U),
			Dirtiness(0.0f),
			SurfaceSettle(3817852694U), // NULL_SOUND
			RidgedSurfaceLoop(3817852694U), // NULL_SOUND
			CollisionCount(0U),
			CollisionCountThreshold(3U),
			VolumeThreshold(-100.0f),
			MaterialID(0),
			DebrisLaunch(3817852694U), // NULL_SOUND
			DebrisLand(3817852694U), // NULL_SOUND
			OffRoadSound(3817852694U), // NULL_SOUND
			Roughness(0.0f),
			DebrisOnSlope(3817852694U), // NULL_SOUND
			BicycleTyreRoll(3817852694U), // NULL_SOUND
			OffRoadRumbleSound(3817852694U), // NULL_SOUND
			StealthSweetener(3817852694U), // NULL_SOUND
			Scuff(3817852694U), // NULL_SOUND
			MaterialType(OTHER),
			SurfacePriority(PRIORITY_OTHER),
			WheelSpinLoop(3817852694U), // NULL_SOUND
			BicycleTyreGritSound(3817852694U), // NULL_SOUND
			PedSlideSound(3861882180U), // BONNET_SLIDE_DEFAULT
			PedRollSound(3861882180U), // BONNET_SLIDE_DEFAULT
			TimeInAirToTriggerBigLand(0.8f),
			MeleeOverideMaterial(0U),
			NameHash(0U),
			SlowMoHardImpact(3817852694U), // NULL_SOUND
			SlowMoBulletImpactSound(3817852694U), // NULL_SOUND
			SlowMoAutomaticBulletImpactSound(3817852694U), // NULL_SOUND
			SlowMoShotgunBulletImpactSound(3817852694U), // NULL_SOUND
			SlowMoBulletImpactPreSuck(3817852694U), // NULL_SOUND
			SlowMoBulletImpactPreSuckTime(0),
			SlowMoAutomaticBulletImpactPreSuck(3817852694U), // NULL_SOUND
			SlowMoAutomaticBulletImpactPreSuckTime(0),
			SlowMoShotgunBulletImpactPreSuck(3817852694U), // NULL_SOUND
			SlowMoShotgunBulletImpactPreSuckTime(0)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 HardImpact;
		rage::u32 SolidImpact;
		rage::u32 SoftImpact;
		rage::u32 ScrapeSound;
		rage::u32 PedScrapeSound;
		rage::u32 BreakSound;
		rage::u32 DestroySound;
		rage::u32 SettleSound;
		rage::u32 BulletImpactSound;
		rage::u32 AutomaticBulletImpactSound;
		rage::u32 ShotgunBulletImpactSound;
		rage::u32 BigVehicleImpactSound;
		rage::u32 PedPunch;
		rage::u32 PedKick;
		rage::u32 MediumIntensity;
		rage::u32 HighIntensity;
		rage::u8 Hardness;
		float MinImpulseMag;
		float MaxImpulseMag;
		rage::u16 ImpulseMagScalar;
		float MaxScrapeSpeed;
		float MinScrapeSpeed;
		float ScrapeImpactMag;
		float MaxRollSpeed;
		float MinRollSpeed;
		float RollImpactMag;
		rage::u8 BulletCollisionScaling;
		rage::u32 FootstepCustomImpactSound;
		float FootstepMaterialHardness;
		float FootstepMaterialLoudness;
		rage::audMetadataRef FootstepSettings;
		rage::u8 FootstepScaling;
		rage::u8 ScuffstepScaling;
		rage::u8 FootstepImpactScaling;
		rage::u8 ScuffImpactScaling;
		rage::u32 SkiSettings;
		rage::audMetadataRef AnimalReference;
		rage::audMetadataRef WetMaterialReference;
		rage::u32 ImpactStartOffsetCurve;
		rage::u32 ImpactVolCurve;
		rage::u32 ScrapePitchCurve;
		rage::u32 ScrapeVolCurve;
		rage::u32 FastTyreRoll;
		rage::u32 DetailTyreRoll;
		rage::u32 MainSkid;
		rage::u32 SideSkid;
		rage::u32 MetalShellCasingSmall;
		rage::u32 MetalShellCasingMedium;
		rage::u32 MetalShellCasingLarge;
		rage::u32 MetalShellCasingSmallSlow;
		rage::u32 MetalShellCasingMediumSlow;
		rage::u32 MetalShellCasingLargeSlow;
		rage::u32 PlasticShellCasing;
		rage::u32 PlasticShellCasingSlow;
		rage::u32 RollSound;
		rage::u32 RainLoop;
		rage::u32 TyreBump;
		rage::u32 ShockwaveSound;
		rage::u32 RandomAmbient;
		rage::u32 ClimbSettings;
		float Dirtiness;
		rage::u32 SurfaceSettle;
		rage::u32 RidgedSurfaceLoop;
		rage::u32 CollisionCount;
		rage::u32 CollisionCountThreshold;
		float VolumeThreshold;
		rage::s32 MaterialID;
		
		struct tSoundSet
		{
			rage::u32 SoundSetRef;
		} SoundSet; // struct tSoundSet
		
		rage::u32 DebrisLaunch;
		rage::u32 DebrisLand;
		rage::u32 OffRoadSound;
		float Roughness;
		rage::u32 DebrisOnSlope;
		rage::u32 BicycleTyreRoll;
		rage::u32 OffRoadRumbleSound;
		rage::u32 StealthSweetener;
		rage::u32 Scuff;
		rage::u8 MaterialType;
		rage::u8 SurfacePriority;
		rage::u32 WheelSpinLoop;
		rage::u32 BicycleTyreGritSound;
		rage::u32 PedSlideSound;
		rage::u32 PedRollSound;
		float TimeInAirToTriggerBigLand;
		rage::u32 MeleeOverideMaterial;
		rage::u32 NameHash;
		rage::u32 SlowMoHardImpact;
		rage::u32 SlowMoBulletImpactSound;
		rage::u32 SlowMoAutomaticBulletImpactSound;
		rage::u32 SlowMoShotgunBulletImpactSound;
		rage::u32 SlowMoBulletImpactPreSuck;
		rage::s32 SlowMoBulletImpactPreSuckTime;
		rage::u32 SlowMoAutomaticBulletImpactPreSuck;
		rage::s32 SlowMoAutomaticBulletImpactPreSuckTime;
		rage::u32 SlowMoShotgunBulletImpactPreSuck;
		rage::s32 SlowMoShotgunBulletImpactPreSuckTime;
	}; // struct CollisionMaterialSettings
	
	// 
	// StaticEmitter
	// 
	enum StaticEmitterFlagIds
	{
		FLAG_ID_STATICEMITTER_ENABLED = 0,
		FLAG_ID_STATICEMITTER_GENERATESBEATS,
		FLAG_ID_STATICEMITTER_LINKEDTOPROP,
		FLAG_ID_STATICEMITTER_CONEDFRONT,
		FLAG_ID_STATICEMITTER_FILLSINTERIORSPACE,
		FLAG_ID_STATICEMITTER_PLAYFULLRADIO,
		FLAG_ID_STATICEMITTER_MUTEFORSCORE,
		FLAG_ID_STATICEMITTER_DISABLEDBYSCRIPT,
		FLAG_ID_STATICEMITTER_STARTSTOPIMMEDIATELYATTIMELIMITS,
		FLAG_ID_STATICEMITTER_MUTEFORFRONTENDRADIO,
		FLAG_ID_STATICEMITTER_FORCEMAXPATHDEPTH,
		FLAG_ID_STATICEMITTER_IGNORESNIPER,
		FLAG_ID_STATICEMITTER_MAX,
	}; // enum StaticEmitterFlagIds
	
	struct StaticEmitter
	{
		static const rage::u32 TYPE_ID = 6;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		StaticEmitter() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			Sound(3817852694U), // NULL_SOUND
			RadioStation(0U),
			MinDistance(0.0f),
			MaxDistance(20.0f),
			EmittedVolume(0),
			FilterCutoff(23900),
			HPFCutoff(0),
			RolloffFactor(100),
			InteriorSettings(0U),
			InteriorRoom(0U),
			RadioStationForScore(0U),
			MaxLeakage(1.0f),
			MinLeakageDistance(0),
			MaxLeakageDistance(0),
			AlarmSettings(0U),
			OnBreakOneShot(3817852694U), // NULL_SOUND
			MaxPathDepth(3),
			SmallReverbSend(0),
			MediumReverbSend(0),
			LargeReverbSend(0),
			MinTimeMinutes(0),
			MaxTimeMinutes(1440),
			BrokenHealth(0.94f),
			UndamagedHealth(0.99f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 Sound;
		rage::u32 RadioStation;
		
		struct tPosition
		{
			float x;
			float y;
			float z;
		} Position; // struct tPosition
		
		float MinDistance;
		float MaxDistance;
		rage::s32 EmittedVolume;
		rage::u16 FilterCutoff;
		rage::u16 HPFCutoff;
		rage::u16 RolloffFactor;
		rage::u32 InteriorSettings;
		rage::u32 InteriorRoom;
		rage::u32 RadioStationForScore;
		float MaxLeakage;
		rage::u16 MinLeakageDistance;
		rage::u16 MaxLeakageDistance;
		rage::u32 AlarmSettings;
		rage::u32 OnBreakOneShot;
		rage::u8 MaxPathDepth;
		rage::u8 SmallReverbSend;
		rage::u8 MediumReverbSend;
		rage::u8 LargeReverbSend;
		rage::u16 MinTimeMinutes;
		rage::u16 MaxTimeMinutes;
		float BrokenHealth;
		float UndamagedHealth;
	}; // struct StaticEmitter
	
	// 
	// EntityEmitter
	// 
	enum EntityEmitterFlagIds
	{
		FLAG_ID_ENTITYEMITTER_ONLYWHENRAINING = 0,
		FLAG_ID_ENTITYEMITTER_MORELIKELYWHENHOT,
		FLAG_ID_ENTITYEMITTER_VOLUMECONE,
		FLAG_ID_ENTITYEMITTER_WINDBASEDVOLUME,
		FLAG_ID_ENTITYEMITTER_GENERATESTREERAIN,
		FLAG_ID_ENTITYEMITTER_STOPWHENRAINING,
		FLAG_ID_ENTITYEMITTER_COMPUTEOCCLUSION,
		FLAG_ID_ENTITYEMITTER_MAX,
	}; // enum EntityEmitterFlagIds
	
	struct EntityEmitter
	{
		static const rage::u32 TYPE_ID = 7;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		EntityEmitter() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			Sound(3817852694U), // NULL_SOUND
			MaxDistance(100.0f),
			BusinessHoursProb(1.0f),
			EveningProb(1.0f),
			NightProb(1.0f),
			ConeInnerAngle(0.0f),
			ConeOuterAngle(0.0f),
			ConeMaxAtten(0.0f),
			StopAfterLoudSound(0U),
			MaxPathDepth(3),
			BrokenHealth(-1.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 Sound;
		float MaxDistance;
		float BusinessHoursProb;
		float EveningProb;
		float NightProb;
		float ConeInnerAngle;
		float ConeOuterAngle;
		float ConeMaxAtten;
		rage::u32 StopAfterLoudSound;
		rage::u8 MaxPathDepth;
		float BrokenHealth;
	}; // struct EntityEmitter
	
	// 
	// HeliAudioSettings
	// 
	enum HeliAudioSettingsFlagIds
	{
		FLAG_ID_HELIAUDIOSETTINGS_DISABLEAMBIENTRADIO = 0,
		FLAG_ID_HELIAUDIOSETTINGS_HASMISSILELOCKSYSTEM,
		FLAG_ID_HELIAUDIOSETTINGS_AIRCRAFTWARNINGVOICEISMALE,
		FLAG_ID_HELIAUDIOSETTINGS_MAX,
	}; // enum HeliAudioSettingsFlagIds
	
	struct HeliAudioSettings
	{
		static const rage::u32 TYPE_ID = 8;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		HeliAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			RotorLoop(0U),
			RearRotorLoop(0U),
			ExhaustLoop(0U),
			BankingLoop(0U),
			CabinToneLoop(0U),
			ThrottleSmoothRate(0.005f),
			BankAngleVolumeCurve(0U),
			BankThrottleVolumeCurve(0U),
			BankThrottlePitchCurve(0U),
			RotorThrottleVolumeCurve(0U),
			RotorThrottlePitchCurve(0U),
			RotorThrottleGapCurve(0U),
			RearRotorThrottleVolumeCurve(0U),
			ExhaustThrottleVolumeCurve(0U),
			ExhaustThrottlePitchCurve(0U),
			RotorConeFrontAngle(0),
			RotorConeRearAngle(0),
			RotorConeAtten(-1500),
			RearRotorConeFrontAngle(0),
			RearRotorConeRearAngle(0),
			RearRotorConeAtten(-1500),
			ExhaustConeFrontAngle(0),
			ExhaustConeRearAngle(0),
			ExhaustConeAtten(-1500),
			Filter1ThrottleResonanceCurve(0U),
			Filter2ThrottleResonanceCurve(0U),
			BankingResonanceCurve(0U),
			RotorVolumeStartupCurve(0U),
			BladeVolumeStartupCurve(0U),
			RotorPitchStartupCurve(0U),
			RearRotorVolumeStartupCurve(0U),
			ExhaustVolumeStartupCurve(0U),
			RotorGapStartupCurve(0U),
			StartUpOneShot(0U),
			BladeConeUpAngle(0),
			BladeConeDownAngle(0),
			BladeConeAtten(-1500),
			ThumpConeUpAngle(0),
			ThumpConeDownAngle(0),
			ThumpConeAtten(-1500),
			ScannerMake(3817852694U), // NULL_SOUND
			ScannerModel(3817852694U), // NULL_SOUND
			ScannerCategory(3817852694U), // NULL_SOUND
			ScannerVehicleSettings(0U),
			RadioType(RADIO_TYPE_NORMAL),
			RadioGenre(RADIO_GENRE_UNSPECIFIED),
			DoorOpen(3817852694U), // NULL_SOUND
			DoorClose(3817852694U), // NULL_SOUND
			DoorLimit(3817852694U), // NULL_SOUND
			DamageLoop(0U),
			RotorSpeedToTriggerSpeedCurve(0U),
			RotorVolumePostSubmix(0),
			EngineSynthDef(0U),
			EngineSynthPreset(0U),
			ExhaustSynthDef(0U),
			ExhaustSynthPreset(0U),
			EngineSubmixVoice(3366128323U), // DEFAULT_HELI_ENGINE_SUBMIX_CONTROL
			ExhaustSubmixVoice(3536749701U), // DEFAULT_HELI_EXHAUST_SUBMIX_CONTROL
			RotorLowFreqLoop(0U),
			ExhaustPitchStartupCurve(0U),
			VehicleCollisions(2221459117U), // VEHICLE_COLLISION_HELI
			FireAudio(804661164U), // VEH_FIRE_SOUNDSET
			DistantLoop(0U),
			SecondaryDoorStartOpen(3817852694U), // NULL_SOUND
			SecondaryDoorStartClose(3817852694U), // NULL_SOUND
			SecondaryDoorClose(3817852694U), // NULL_SOUND
			SecondaryDoorLimit(3817852694U), // NULL_SOUND
			SuspensionUp(2095958071U), // SUSPENSION_UP
			SuspensionDown(1257191667U), // SUSPENSION_DOWN
			MinSuspCompThresh(0.4f),
			MaxSuspCompThres(1.0f),
			DamageOneShots(0U),
			DamageWarning(3106242784U), // HELI_DAMAGE_WARNING
			TailBreak(232137733U), // HELI_TAIL_BREAK
			RotorBreak(1971580398U), // HELI_ROTOR_BREAK
			RearRotorBreak(1979485215U), // HELI_REAR_ROTOR_BREAK
			CableDeploy(347111341U), // HELICOPTER_CABLE_DEPLOY
			HardScrape(1769283095U), // MULTI_SCRAPE
			EngineCooling(1468762068U), // HEAT_STRESS_HEAT_TICK_LOOP
			AltitudeWarning(2663611655U), // ALTITUDE_WARNING
			HealthToDamageVolumeCurve(3920017295U), // HELI_DAMAGE_VOLUME_CURVE
			HealthBelow600ToDamageVolumeCurve(3920017295U), // HELI_DAMAGE_VOLUME_CURVE
			DamageBelow600Loop(0U),
			AircraftWarningSeriousDamageThresh(0.003f),
			AircraftWarningCriticalDamageThresh(0.003f),
			AircraftWarningMaxSpeed(100.0f),
			SimpleSoundForLoading(3817852694U), // NULL_SOUND
			SlowMoRotorLoop(2430700147U), // PLAYER_SWITCH_HELI_MASTER
			SlowMoRotorLowFreqLoop(3817852694U), // NULL_SOUND
			VehicleRainSound(3817852694U), // NULL_SOUND
			VehicleRainSoundInterior(3817852694U), // NULL_SOUND
			StartUpFailOneShot(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 RotorLoop;
		rage::u32 RearRotorLoop;
		rage::u32 ExhaustLoop;
		rage::u32 BankingLoop;
		rage::u32 CabinToneLoop;
		float ThrottleSmoothRate;
		rage::u32 BankAngleVolumeCurve;
		rage::u32 BankThrottleVolumeCurve;
		rage::u32 BankThrottlePitchCurve;
		rage::u32 RotorThrottleVolumeCurve;
		rage::u32 RotorThrottlePitchCurve;
		rage::u32 RotorThrottleGapCurve;
		rage::u32 RearRotorThrottleVolumeCurve;
		rage::u32 ExhaustThrottleVolumeCurve;
		rage::u32 ExhaustThrottlePitchCurve;
		rage::u16 RotorConeFrontAngle;
		rage::u16 RotorConeRearAngle;
		rage::s16 RotorConeAtten;
		rage::u16 RearRotorConeFrontAngle;
		rage::u16 RearRotorConeRearAngle;
		rage::s16 RearRotorConeAtten;
		rage::u16 ExhaustConeFrontAngle;
		rage::u16 ExhaustConeRearAngle;
		rage::s16 ExhaustConeAtten;
		rage::u32 Filter1ThrottleResonanceCurve;
		rage::u32 Filter2ThrottleResonanceCurve;
		rage::u32 BankingResonanceCurve;
		rage::u32 RotorVolumeStartupCurve;
		rage::u32 BladeVolumeStartupCurve;
		rage::u32 RotorPitchStartupCurve;
		rage::u32 RearRotorVolumeStartupCurve;
		rage::u32 ExhaustVolumeStartupCurve;
		rage::u32 RotorGapStartupCurve;
		rage::u32 StartUpOneShot;
		rage::u16 BladeConeUpAngle;
		rage::u16 BladeConeDownAngle;
		rage::s16 BladeConeAtten;
		rage::u16 ThumpConeUpAngle;
		rage::u16 ThumpConeDownAngle;
		rage::s16 ThumpConeAtten;
		rage::u32 ScannerMake;
		rage::u32 ScannerModel;
		rage::u32 ScannerCategory;
		rage::u32 ScannerVehicleSettings;
		rage::u8 RadioType;
		rage::u8 RadioGenre;
		rage::u32 DoorOpen;
		rage::u32 DoorClose;
		rage::u32 DoorLimit;
		rage::u32 DamageLoop;
		rage::u32 RotorSpeedToTriggerSpeedCurve;
		rage::s16 RotorVolumePostSubmix;
		rage::u32 EngineSynthDef;
		rage::u32 EngineSynthPreset;
		rage::u32 ExhaustSynthDef;
		rage::u32 ExhaustSynthPreset;
		rage::u32 EngineSubmixVoice;
		rage::u32 ExhaustSubmixVoice;
		rage::u32 RotorLowFreqLoop;
		rage::u32 ExhaustPitchStartupCurve;
		rage::u32 VehicleCollisions;
		rage::u32 FireAudio;
		rage::u32 DistantLoop;
		rage::u32 SecondaryDoorStartOpen;
		rage::u32 SecondaryDoorStartClose;
		rage::u32 SecondaryDoorClose;
		rage::u32 SecondaryDoorLimit;
		rage::u32 SuspensionUp;
		rage::u32 SuspensionDown;
		float MinSuspCompThresh;
		float MaxSuspCompThres;
		rage::u32 DamageOneShots;
		rage::u32 DamageWarning;
		rage::u32 TailBreak;
		rage::u32 RotorBreak;
		rage::u32 RearRotorBreak;
		rage::u32 CableDeploy;
		rage::u32 HardScrape;
		rage::u32 EngineCooling;
		rage::u32 AltitudeWarning;
		rage::u32 HealthToDamageVolumeCurve;
		rage::u32 HealthBelow600ToDamageVolumeCurve;
		rage::u32 DamageBelow600Loop;
		float AircraftWarningSeriousDamageThresh;
		float AircraftWarningCriticalDamageThresh;
		float AircraftWarningMaxSpeed;
		rage::u32 SimpleSoundForLoading;
		rage::u32 SlowMoRotorLoop;
		rage::u32 SlowMoRotorLowFreqLoop;
		rage::u32 VehicleRainSound;
		rage::u32 VehicleRainSoundInterior;
		rage::u32 StartUpFailOneShot;
	}; // struct HeliAudioSettings
	
	// 
	// MeleeCombatSettings
	// 
	enum MeleeCombatSettingsFlagIds
	{
		FLAG_ID_MELEECOMBATSETTINGS_MAX = 0,
	}; // enum MeleeCombatSettingsFlagIds
	
	struct MeleeCombatSettings
	{
		static const rage::u32 TYPE_ID = 9;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		MeleeCombatSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			SwipeSound(1851024669U), // OVERRIDE
			GeneralHitSound(1851024669U), // OVERRIDE
			PedHitSound(1851024669U), // OVERRIDE
			PedResponseSound(3817852694U), // NULL_SOUND
			HeadTakeDown(3817852694U), // NULL_SOUND
			BodyTakeDown(3817852694U), // NULL_SOUND
			SmallAnimalHitSound(3817852694U), // NULL_SOUND
			SmallAnimalResponseSound(3817852694U), // NULL_SOUND
			BigAnimalHitSound(3817852694U), // NULL_SOUND
			BigAnimalResponseSound(3817852694U), // NULL_SOUND
			SlowMoPedHitSound(3817852694U), // NULL_SOUND
			SlowMoPedResponseSound(3817852694U), // NULL_SOUND
			SlowMoHeadTakeDown(3817852694U), // NULL_SOUND
			SlowMoBodyTakeDown(3817852694U), // NULL_SOUND
			SlowMoSmallAnimalHitSound(3817852694U), // NULL_SOUND
			SlowMoSmallAnimalResponseSound(3817852694U), // NULL_SOUND
			SlowMoBigAnimalHitSound(3817852694U), // NULL_SOUND
			SlowMoBigAnimalResponseSound(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 SwipeSound;
		rage::u32 GeneralHitSound;
		rage::u32 PedHitSound;
		rage::u32 PedResponseSound;
		rage::u32 HeadTakeDown;
		rage::u32 BodyTakeDown;
		rage::u32 SmallAnimalHitSound;
		rage::u32 SmallAnimalResponseSound;
		rage::u32 BigAnimalHitSound;
		rage::u32 BigAnimalResponseSound;
		rage::u32 SlowMoPedHitSound;
		rage::u32 SlowMoPedResponseSound;
		rage::u32 SlowMoHeadTakeDown;
		rage::u32 SlowMoBodyTakeDown;
		rage::u32 SlowMoSmallAnimalHitSound;
		rage::u32 SlowMoSmallAnimalResponseSound;
		rage::u32 SlowMoBigAnimalHitSound;
		rage::u32 SlowMoBigAnimalResponseSound;
	}; // struct MeleeCombatSettings
	
	// 
	// SpeechContextSettings
	// 
	enum SpeechContextSettingsFlagIds
	{
		FLAG_ID_SPEECHCONTEXTSETTINGS_MAX = 0,
	}; // enum SpeechContextSettingsFlagIds
	
	struct SpeechContextSettings
	{
		static const rage::u32 TYPE_ID = 10;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		SpeechContextSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_CONTEXTS = 400;
		rage::u16 numContexts;
		struct tContexts
		{
			rage::u32 Context;
		} Contexts[MAX_CONTEXTS]; // struct tContexts
		
	}; // struct SpeechContextSettings
	
	// 
	// TriggeredSpeechContext
	// 
	enum TriggeredSpeechContextFlagIds
	{
		FLAG_ID_TRIGGEREDSPEECHCONTEXT_MAX = 0,
	}; // enum TriggeredSpeechContextFlagIds
	
	struct TriggeredSpeechContext : AudBaseObject
	{
		static const rage::u32 TYPE_ID = 11;
		static const rage::u32 BASE_TYPE_ID = AudBaseObject::TYPE_ID;
		
		TriggeredSpeechContext() :
			TriggeredContextRepeatTime(0),
			PrimaryRepeatTime(0),
			PrimaryRepeatTimeOnSameVoice(-1),
			BackupRepeatTime(0),
			BackupRepeatTimeOnSameVoice(-1),
			TimeCanNextUseTriggeredContext(0U),
			TimeCanNextPlayPrimary(0U),
			TimeCanNextPlayBackup(0U),
			PrimarySpeechContext(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::s32 TriggeredContextRepeatTime;
		rage::s32 PrimaryRepeatTime;
		rage::s32 PrimaryRepeatTimeOnSameVoice;
		rage::s32 BackupRepeatTime;
		rage::s32 BackupRepeatTimeOnSameVoice;
		rage::u32 TimeCanNextUseTriggeredContext;
		rage::u32 TimeCanNextPlayPrimary;
		rage::u32 TimeCanNextPlayBackup;
		rage::u32 PrimarySpeechContext;
		
		static const rage::u8 MAX_BACKUPSPEECHCONTEXTS = 16;
		rage::u8 numBackupSpeechContexts;
		struct tBackupSpeechContext
		{
			rage::u32 SpeechContext;
			float Weight;
		} BackupSpeechContext[MAX_BACKUPSPEECHCONTEXTS]; // struct tBackupSpeechContext
		
	}; // struct TriggeredSpeechContext : AudBaseObject
	
	// 
	// SpeechContext
	// 
	enum SpeechContextFlagIds
	{
		FLAG_ID_SPEECHCONTEXT_ALLOWREPEAT = 0,
		FLAG_ID_SPEECHCONTEXT_FORCEPLAY,
		FLAG_ID_SPEECHCONTEXT_ISCONVERSATION,
		FLAG_ID_SPEECHCONTEXT_ISPAIN,
		FLAG_ID_SPEECHCONTEXT_ISCOMBAT,
		FLAG_ID_SPEECHCONTEXT_ISOKAYONMISSION,
		FLAG_ID_SPEECHCONTEXT_ISURGENT,
		FLAG_ID_SPEECHCONTEXT_ISMUTEDDURINGSLOWMO,
		FLAG_ID_SPEECHCONTEXT_ISPITCHEDDURINGSLOWMO,
		FLAG_ID_SPEECHCONTEXT_MAX,
	}; // enum SpeechContextFlagIds
	
	struct SpeechContext : AudBaseObject
	{
		static const rage::u32 TYPE_ID = 12;
		static const rage::u32 BASE_TYPE_ID = AudBaseObject::TYPE_ID;
		
		SpeechContext() :
			RepeatTime(0),
			RepeatTimeOnSameVoice(-1),
			VolumeType(CONTEXT_VOLUME_NORMAL),
			Audibility(CONTEXT_AUDIBILITY_NORMAL),
			GenderNonSpecificVersion(0U),
			TimeCanNextPlay(0U),
			Priority(3)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ContextNamePHash;
		rage::s32 RepeatTime;
		rage::s32 RepeatTimeOnSameVoice;
		rage::u8 VolumeType;
		rage::u8 Audibility;
		rage::u32 GenderNonSpecificVersion;
		rage::u32 TimeCanNextPlay;
		rage::u8 Priority;
		
		static const rage::u8 MAX_FAKEGESTURES = 4;
		rage::u8 numFakeGestures;
		struct tFakeGesture
		{
			rage::u8 GestureEnum;
		} FakeGesture[MAX_FAKEGESTURES]; // struct tFakeGesture
		
	}; // struct SpeechContext : AudBaseObject
	
	// 
	// SpeechContextVirtual
	// 
	enum SpeechContextVirtualFlagIds
	{
		FLAG_ID_SPEECHCONTEXTVIRTUAL_ALLOWREPEAT = 0,
		FLAG_ID_SPEECHCONTEXTVIRTUAL_FORCEPLAY,
		FLAG_ID_SPEECHCONTEXTVIRTUAL_ISCONVERSATION,
		FLAG_ID_SPEECHCONTEXTVIRTUAL_ISPAIN,
		FLAG_ID_SPEECHCONTEXTVIRTUAL_ISCOMBAT,
		FLAG_ID_SPEECHCONTEXTVIRTUAL_ISOKAYONMISSION,
		FLAG_ID_SPEECHCONTEXTVIRTUAL_ISURGENT,
		FLAG_ID_SPEECHCONTEXTVIRTUAL_ISMUTEDDURINGSLOWMO,
		FLAG_ID_SPEECHCONTEXTVIRTUAL_ISPITCHEDDURINGSLOWMO,
		FLAG_ID_SPEECHCONTEXTVIRTUAL_MAX,
	}; // enum SpeechContextVirtualFlagIds
	
	struct SpeechContextVirtual : SpeechContext
	{
		static const rage::u32 TYPE_ID = 13;
		static const rage::u32 BASE_TYPE_ID = SpeechContext::TYPE_ID;
		
		SpeechContextVirtual() :
			ResolvingFunction(SRF_DEFAULT)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u8 ResolvingFunction;
		
		static const rage::u8 MAX_SPEECHCONTEXTS = 16;
		rage::u8 numSpeechContexts;
		struct tSpeechContexts
		{
			rage::u32 SpeechContext;
		} SpeechContexts[MAX_SPEECHCONTEXTS]; // struct tSpeechContexts
		
	}; // struct SpeechContextVirtual : SpeechContext
	
	// 
	// SpeechParams
	// 
	enum SpeechParamsFlagIds
	{
		FLAG_ID_SPEECHPARAMS_ALLOWREPEAT = 0,
		FLAG_ID_SPEECHPARAMS_FORCEPLAY,
		FLAG_ID_SPEECHPARAMS_SAYOVERPAIN,
		FLAG_ID_SPEECHPARAMS_PRELOADONLY,
		FLAG_ID_SPEECHPARAMS_ISCONVERSATIONINTERRUPT,
		FLAG_ID_SPEECHPARAMS_ADDBLIP,
		FLAG_ID_SPEECHPARAMS_ISURGENT,
		FLAG_ID_SPEECHPARAMS_INTERRUPTAMBIENTSPEECH,
		FLAG_ID_SPEECHPARAMS_ISMUTEDDURINGSLOWMO,
		FLAG_ID_SPEECHPARAMS_ISPITCHEDDURINGSLOWMO,
		FLAG_ID_SPEECHPARAMS_MAX,
	}; // enum SpeechParamsFlagIds
	
	struct SpeechParams : AudBaseObject
	{
		static const rage::u32 TYPE_ID = 14;
		static const rage::u32 BASE_TYPE_ID = AudBaseObject::TYPE_ID;
		
		SpeechParams() :
			PreloadTimeoutInMs(30000U),
			RequestedVolume(SPEECH_VOLUME_UNSPECIFIED),
			Audibility(CONTEXT_AUDIBILITY_NORMAL),
			RepeatTime(0),
			RepeatTimeOnSameVoice(-1)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		
		struct tOverrideContextSettings
		{
			tOverrideContextSettings()
			 : Value(0U)
			{
			}
			union
			{
				rage::u8 Value;
				struct
				{
					#if __BE
						bool padding:4; // padding
						bool OverrideRepeatTimeOnSameVoice:1;
						bool OverrideRepeatTime:1;
						bool OverrideForcePlay:1;
						bool OverrideAllowRepeat:1;
					#else // __BE
						bool OverrideAllowRepeat:1;
						bool OverrideForcePlay:1;
						bool OverrideRepeatTime:1;
						bool OverrideRepeatTimeOnSameVoice:1;
						bool padding:4; // padding
					#endif // __BE
				} BitFields;
			};
		} OverrideContextSettings; // struct tOverrideContextSettings
		
		rage::u32 PreloadTimeoutInMs;
		rage::u8 RequestedVolume;
		rage::u8 Audibility;
		rage::s32 RepeatTime;
		rage::s32 RepeatTimeOnSameVoice;
	}; // struct SpeechParams : AudBaseObject
	
	// 
	// SpeechContextList
	// 
	enum SpeechContextListFlagIds
	{
		FLAG_ID_SPEECHCONTEXTLIST_MAX = 0,
	}; // enum SpeechContextListFlagIds
	
	struct SpeechContextList
	{
		static const rage::u32 TYPE_ID = 15;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		SpeechContextList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_SPEECHCONTEXTS = 64;
		rage::u8 numSpeechContexts;
		struct tSpeechContexts
		{
			rage::u32 SpeechContext;
		} SpeechContexts[MAX_SPEECHCONTEXTS]; // struct tSpeechContexts
		
	}; // struct SpeechContextList
	
	// 
	// BoatAudioSettings
	// 
	enum BoatAudioSettingsFlagIds
	{
		FLAG_ID_BOATAUDIOSETTINGS_DISABLEAMBIENTRADIO = 0,
		FLAG_ID_BOATAUDIOSETTINGS_ISSUBMARINE,
		FLAG_ID_BOATAUDIOSETTINGS_MAX,
	}; // enum BoatAudioSettingsFlagIds
	
	struct BoatAudioSettings
	{
		static const rage::u32 TYPE_ID = 16;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		BoatAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			Engine1Loop(0U),
			Engine1Vol(2047542242U), // BOAT_ENGINE_VOLUME
			Engine1Pitch(113167440U), // BOAT_ENGINE_PITCH
			Engine2Loop(0U),
			Engine2Vol(2047542242U), // BOAT_ENGINE_VOLUME
			Engine2Pitch(113167440U), // BOAT_ENGINE_PITCH
			LowResoLoop(0U),
			LowResoLoopVol(2047542242U), // BOAT_ENGINE_VOLUME
			LowResoPitch(113167440U), // BOAT_ENGINE_PITCH
			ResoLoop(0U),
			ResoLoopVol(2047542242U), // BOAT_ENGINE_VOLUME
			ResoPitch(113167440U), // BOAT_ENGINE_PITCH
			WaterTurbulance(0U),
			WaterTurbulanceVol(2047542242U), // BOAT_ENGINE_VOLUME
			WaterTurbulancePitch(1799210146U), // BOAT_WATER_TURBULANCE_PITCH
			ScannerMake(3817852694U), // NULL_SOUND
			ScannerModel(3817852694U), // NULL_SOUND
			ScannerCategory(3817852694U), // NULL_SOUND
			ScannerVehicleSettings(0U),
			RadioType(RADIO_TYPE_NORMAL),
			RadioGenre(RADIO_GENRE_UNSPECIFIED),
			HornLoop(3817852694U), // NULL_SOUND
			IgnitionOneShot(1326271245U), // SPEEDBOAT_IGNITION
			ShutdownOneShot(640993156U), // SPEEDBOAT_SHUTDOWN
			EngineVolPostSubmix(0),
			ExhaustVolPostSubmix(0),
			EngineSynthDef(0U),
			EngineSynthPreset(0U),
			ExhaustSynthDef(0U),
			ExhaustSynthPreset(0U),
			VehicleCollisions(1296314264U), // VEHICLE_COLLISION_BOAT
			EngineSubmixVoice(1368457706U), // DEFAULT_BOAT_ENGINE_SUBMIX_CONTROL
			ExhaustSubmixVoice(3406795460U), // DEFAULT_BOAT_EXHAUST_SUBMIX_CONTROL
			WaveHitSound(2681268625U), // BOAT_WAVE_HIT
			LeftWaterSound(0U),
			IdleHullSlapLoop(4294927036U), // IDLE_HULL_SLAP
			IdleHullSlapSpeedToVol(2112336202U), // BOAT_SPEED_TO_HULL_SLAP_VOLUME
			GranularEngine(0U),
			BankSpraySound(78067818U), // GENERAL_WATER_FAST_BOAT_BANK_SPRAY
			IgnitionLoop(652097730U), // IGNITION
			EngineStartUp(0U),
			SubTurningEnginePitchModifier(0U),
			SubTurningSweetenerSound(3817852694U), // NULL_SOUND
			RevsToSweetenerVolume(3454258691U), // CONSTANT_ONE
			TurningToSweetenerVolume(0U),
			TurningToSweetenerPitch(0U),
			DryLandScrape(3817852694U), // NULL_SOUND
			MaxRollOffScalePlayer(6.0f),
			MaxRollOffScaleNPC(3.0f),
			Openness(1.0f),
			DryLandHardScrape(3817852694U), // NULL_SOUND
			DryLandHardImpact(3817852694U), // NULL_SOUND
			WindClothSound(3817852694U), // NULL_SOUND
			FireAudio(804661164U), // VEH_FIRE_SOUNDSET
			DoorOpen(3817852694U), // NULL_SOUND
			DoorClose(3817852694U), // NULL_SOUND
			DoorLimit(3817852694U), // NULL_SOUND
			DoorStartClose(3817852694U), // NULL_SOUND
			SubExtrasSound(3817852694U), // NULL_SOUND
			SFXBankSound(3817852694U), // NULL_SOUND
			SubmersibleCreaksSound(3817852694U), // NULL_SOUND
			WaveHitBigAirSound(1944386200U), // BOAT_WAVE_HIT_BIG_AIR
			BigAirMinTime(1.2f),
			VehicleRainSound(3817852694U), // NULL_SOUND
			VehicleRainSoundInterior(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 Engine1Loop;
		rage::u32 Engine1Vol;
		rage::u32 Engine1Pitch;
		rage::u32 Engine2Loop;
		rage::u32 Engine2Vol;
		rage::u32 Engine2Pitch;
		rage::u32 LowResoLoop;
		rage::u32 LowResoLoopVol;
		rage::u32 LowResoPitch;
		rage::u32 ResoLoop;
		rage::u32 ResoLoopVol;
		rage::u32 ResoPitch;
		rage::u32 WaterTurbulance;
		rage::u32 WaterTurbulanceVol;
		rage::u32 WaterTurbulancePitch;
		rage::u32 ScannerMake;
		rage::u32 ScannerModel;
		rage::u32 ScannerCategory;
		rage::u32 ScannerVehicleSettings;
		rage::u8 RadioType;
		rage::u8 RadioGenre;
		rage::u32 HornLoop;
		rage::u32 IgnitionOneShot;
		rage::u32 ShutdownOneShot;
		rage::s16 EngineVolPostSubmix;
		rage::s16 ExhaustVolPostSubmix;
		rage::u32 EngineSynthDef;
		rage::u32 EngineSynthPreset;
		rage::u32 ExhaustSynthDef;
		rage::u32 ExhaustSynthPreset;
		rage::u32 VehicleCollisions;
		rage::u32 EngineSubmixVoice;
		rage::u32 ExhaustSubmixVoice;
		rage::u32 WaveHitSound;
		rage::u32 LeftWaterSound;
		rage::u32 IdleHullSlapLoop;
		rage::u32 IdleHullSlapSpeedToVol;
		rage::u32 GranularEngine;
		rage::u32 BankSpraySound;
		rage::u32 IgnitionLoop;
		rage::u32 EngineStartUp;
		rage::u32 SubTurningEnginePitchModifier;
		rage::u32 SubTurningSweetenerSound;
		rage::u32 RevsToSweetenerVolume;
		rage::u32 TurningToSweetenerVolume;
		rage::u32 TurningToSweetenerPitch;
		rage::u32 DryLandScrape;
		float MaxRollOffScalePlayer;
		float MaxRollOffScaleNPC;
		float Openness;
		rage::u32 DryLandHardScrape;
		rage::u32 DryLandHardImpact;
		rage::u32 WindClothSound;
		rage::u32 FireAudio;
		rage::u32 DoorOpen;
		rage::u32 DoorClose;
		rage::u32 DoorLimit;
		rage::u32 DoorStartClose;
		rage::u32 SubExtrasSound;
		rage::u32 SFXBankSound;
		rage::u32 SubmersibleCreaksSound;
		rage::u32 WaveHitBigAirSound;
		float BigAirMinTime;
		rage::u32 VehicleRainSound;
		rage::u32 VehicleRainSoundInterior;
	}; // struct BoatAudioSettings
	
	// 
	// WeaponSettings
	// 
	enum WeaponSettingsFlagIds
	{
		FLAG_ID_WEAPONSETTINGS_ISPLAYERAUTOFIRE = 0,
		FLAG_ID_WEAPONSETTINGS_ISNPCAUTOFIRE,
		FLAG_ID_WEAPONSETTINGS_ISNONLETHAL,
		FLAG_ID_WEAPONSETTINGS_RECOCKWHENSNIPING,
		FLAG_ID_WEAPONSETTINGS_DOESMELEEIMPACTS,
		FLAG_ID_WEAPONSETTINGS_MAX,
	}; // enum WeaponSettingsFlagIds
	
	struct WeaponSettings
	{
		static const rage::u32 TYPE_ID = 17;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		WeaponSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			FireSound(3817852694U), // NULL_SOUND
			SuppressedFireSound(3817852694U), // NULL_SOUND
			AutoSound(3817852694U), // NULL_SOUND
			ReportSound(3817852694U), // NULL_SOUND
			ReportVolumeDelta(-2.0f),
			ReportPredelayDelta(50U),
			TailEnergyValue(0.2f),
			EchoSound(3817852694U), // NULL_SOUND
			SuppressedEchoSound(3817852694U), // NULL_SOUND
			ShellCasingSound(3817852694U), // NULL_SOUND
			SwipeSound(3817852694U), // NULL_SOUND
			GeneralStrikeSound(3817852694U), // NULL_SOUND
			PedStrikeSound(3817852694U), // NULL_SOUND
			HeftSound(3817852694U), // NULL_SOUND
			PutDownSound(3817852694U), // NULL_SOUND
			RattleSound(3817852694U), // NULL_SOUND
			RattleLandSound(3817852694U), // NULL_SOUND
			PickupSound(2328742057U), // FRONTEND_GAME_PICKUP_WEAPON
			ShellCasing(SHELLCASING_METAL),
			SafetyOn(3817852694U), // NULL_SOUND
			SafetyOff(3817852694U), // NULL_SOUND
			SpecialWeaponSoundSet(0U),
			BankSound(3817852694U), // NULL_SOUND
			InteriorShotSound(3817852694U), // NULL_SOUND
			ReloadSounds(0U),
			IntoCoverSound(3817852694U), // NULL_SOUND
			OutOfCoverSound(3817852694U), // NULL_SOUND
			BulletImpactTimeFilter(0U),
			LastBulletImpactTime(0U),
			RattleAimSound(3817852694U), // NULL_SOUND
			SmallAnimalStrikeSound(3817852694U), // NULL_SOUND
			BigAnimalStrikeSound(3817852694U), // NULL_SOUND
			SlowMoFireSound(3817852694U), // NULL_SOUND
			HitPedSound(3817852694U), // NULL_SOUND
			SlowMoFireSoundPresuck(3817852694U), // NULL_SOUND
			SlowMoSuppressedFireSound(3817852694U), // NULL_SOUND
			SlowMoSuppressedFireSoundPresuck(3817852694U), // NULL_SOUND
			SlowMoReportSound(3817852694U), // NULL_SOUND
			SlowMoInteriorShotSound(3817852694U), // NULL_SOUND
			SlowMoPedStrikeSound(3817852694U), // NULL_SOUND
			SlowMoPedStrikeSoundPresuck(3817852694U), // NULL_SOUND
			SlowMoBigAnimalStrikeSound(3817852694U), // NULL_SOUND
			SlowMoBigAnimalStrikeSoundPresuck(3817852694U), // NULL_SOUND
			SlowMoSmallAnimalStrikeSound(3817852694U), // NULL_SOUND
			SlowMoSmallAnimalStrikeSoundPresuck(3817852694U), // NULL_SOUND
			SlowMoFireSoundPresuckTime(0),
			SlowMoSuppressedFireSoundPresuckTime(0),
			SlowMoPedStrikeSoundPresuckTime(0),
			SlowMoBigAnimalStrikeSoundPresuckTime(0),
			SlowMoSmallAnimalStrikeSoundPresuckTime(0),
			SuperSlowMoFireSound(3817852694U), // NULL_SOUND
			SuperSlowMoFireSoundPresuck(3817852694U), // NULL_SOUND
			SuperSlowMoSuppressedFireSound(3817852694U), // NULL_SOUND
			SuperSlowMoSuppressedFireSoundPresuck(3817852694U), // NULL_SOUND
			SuperSlowMoReportSound(3817852694U), // NULL_SOUND
			SuperSlowMoInteriorShotSound(3817852694U), // NULL_SOUND
			SuperSlowMoPedStrikeSound(3817852694U), // NULL_SOUND
			SuperSlowMoPedStrikeSoundPresuck(3817852694U), // NULL_SOUND
			SuperSlowMoBigAnimalStrikeSound(3817852694U), // NULL_SOUND
			SuperSlowMoBigAnimalStrikeSoundPresuck(3817852694U), // NULL_SOUND
			SuperSlowMoSmallAnimalStrikeSound(3817852694U), // NULL_SOUND
			SuperSlowMoSmallAnimalStrikeSoundPresuck(3817852694U), // NULL_SOUND
			SuperSlowMoFireSoundPresuckTime(9999),
			SuperSlowMoSuppressedFireSoundPresuckTime(9999),
			SuperSlowMoPedStrikeSoundPresuckTime(9999),
			SuperSlowMoBigAnimalStrikeSoundPresuckTime(9999),
			SuperSlowMoSmallAnimalStrikeSoundPresuckTime(9999)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 FireSound;
		rage::u32 SuppressedFireSound;
		rage::u32 AutoSound;
		rage::u32 ReportSound;
		float ReportVolumeDelta;
		rage::u32 ReportPredelayDelta;
		float TailEnergyValue;
		rage::u32 EchoSound;
		rage::u32 SuppressedEchoSound;
		rage::u32 ShellCasingSound;
		rage::u32 SwipeSound;
		rage::u32 GeneralStrikeSound;
		rage::u32 PedStrikeSound;
		rage::u32 HeftSound;
		rage::u32 PutDownSound;
		rage::u32 RattleSound;
		rage::u32 RattleLandSound;
		rage::u32 PickupSound;
		rage::u8 ShellCasing;
		rage::u32 SafetyOn;
		rage::u32 SafetyOff;
		rage::u32 SpecialWeaponSoundSet;
		rage::u32 BankSound;
		rage::u32 InteriorShotSound;
		rage::u32 ReloadSounds;
		rage::u32 IntoCoverSound;
		rage::u32 OutOfCoverSound;
		rage::u32 BulletImpactTimeFilter;
		rage::u32 LastBulletImpactTime;
		rage::u32 RattleAimSound;
		rage::u32 SmallAnimalStrikeSound;
		rage::u32 BigAnimalStrikeSound;
		rage::u32 SlowMoFireSound;
		rage::u32 HitPedSound;
		rage::u32 SlowMoFireSoundPresuck;
		rage::u32 SlowMoSuppressedFireSound;
		rage::u32 SlowMoSuppressedFireSoundPresuck;
		rage::u32 SlowMoReportSound;
		rage::u32 SlowMoInteriorShotSound;
		rage::u32 SlowMoPedStrikeSound;
		rage::u32 SlowMoPedStrikeSoundPresuck;
		rage::u32 SlowMoBigAnimalStrikeSound;
		rage::u32 SlowMoBigAnimalStrikeSoundPresuck;
		rage::u32 SlowMoSmallAnimalStrikeSound;
		rage::u32 SlowMoSmallAnimalStrikeSoundPresuck;
		rage::s32 SlowMoFireSoundPresuckTime;
		rage::s32 SlowMoSuppressedFireSoundPresuckTime;
		rage::s32 SlowMoPedStrikeSoundPresuckTime;
		rage::s32 SlowMoBigAnimalStrikeSoundPresuckTime;
		rage::s32 SlowMoSmallAnimalStrikeSoundPresuckTime;
		rage::u32 SuperSlowMoFireSound;
		rage::u32 SuperSlowMoFireSoundPresuck;
		rage::u32 SuperSlowMoSuppressedFireSound;
		rage::u32 SuperSlowMoSuppressedFireSoundPresuck;
		rage::u32 SuperSlowMoReportSound;
		rage::u32 SuperSlowMoInteriorShotSound;
		rage::u32 SuperSlowMoPedStrikeSound;
		rage::u32 SuperSlowMoPedStrikeSoundPresuck;
		rage::u32 SuperSlowMoBigAnimalStrikeSound;
		rage::u32 SuperSlowMoBigAnimalStrikeSoundPresuck;
		rage::u32 SuperSlowMoSmallAnimalStrikeSound;
		rage::u32 SuperSlowMoSmallAnimalStrikeSoundPresuck;
		rage::s32 SuperSlowMoFireSoundPresuckTime;
		rage::s32 SuperSlowMoSuppressedFireSoundPresuckTime;
		rage::s32 SuperSlowMoPedStrikeSoundPresuckTime;
		rage::s32 SuperSlowMoBigAnimalStrikeSoundPresuckTime;
		rage::s32 SuperSlowMoSmallAnimalStrikeSoundPresuckTime;
	}; // struct WeaponSettings
	
	// 
	// ShoeAudioSettings
	// 
	enum ShoeAudioSettingsFlagIds
	{
		FLAG_ID_SHOEAUDIOSETTINGS_MAX = 0,
	}; // enum ShoeAudioSettingsFlagIds
	
	struct ShoeAudioSettings
	{
		static const rage::u32 TYPE_ID = 18;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ShoeAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Walk(3817852694U), // NULL_SOUND
			DirtyWalk(3817852694U), // NULL_SOUND
			CreakyWalk(3817852694U), // NULL_SOUND
			GlassyWalk(3817852694U), // NULL_SOUND
			Run(3817852694U), // NULL_SOUND
			DirtyRun(3817852694U), // NULL_SOUND
			CreakyRun(3817852694U), // NULL_SOUND
			GlassyRun(3817852694U), // NULL_SOUND
			WetWalk(3817852694U), // NULL_SOUND
			WetRun(3817852694U), // NULL_SOUND
			SoftWalk(3817852694U), // NULL_SOUND
			Scuff(3817852694U), // NULL_SOUND
			Land(3817852694U), // NULL_SOUND
			ShoeHardness(1.0f),
			ShoeCreakiness(1.0f),
			ShoeType(SHOE_TYPE_SHOES),
			ShoeFitness(1.0f),
			LadderShoeDown(3817852694U), // NULL_SOUND
			LadderShoeUp(3817852694U), // NULL_SOUND
			Kick(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 Walk;
		rage::u32 DirtyWalk;
		rage::u32 CreakyWalk;
		rage::u32 GlassyWalk;
		rage::u32 Run;
		rage::u32 DirtyRun;
		rage::u32 CreakyRun;
		rage::u32 GlassyRun;
		rage::u32 WetWalk;
		rage::u32 WetRun;
		rage::u32 SoftWalk;
		rage::u32 Scuff;
		rage::u32 Land;
		float ShoeHardness;
		float ShoeCreakiness;
		rage::u8 ShoeType;
		float ShoeFitness;
		rage::u32 LadderShoeDown;
		rage::u32 LadderShoeUp;
		rage::u32 Kick;
	}; // struct ShoeAudioSettings
	
	// 
	// OldShoeSettings
	// 
	enum OldShoeSettingsFlagIds
	{
		FLAG_ID_OLDSHOESETTINGS_MAX = 0,
	}; // enum OldShoeSettingsFlagIds
	
	struct OldShoeSettings
	{
		static const rage::u32 TYPE_ID = 19;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		OldShoeSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			ScuffHard(3817852694U), // NULL_SOUND
			Scuff(3817852694U), // NULL_SOUND
			Ladder(3817852694U), // NULL_SOUND
			Jump(3817852694U), // NULL_SOUND
			Run(3817852694U), // NULL_SOUND
			Walk(3817852694U), // NULL_SOUND
			Sprint(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 ScuffHard;
		rage::u32 Scuff;
		rage::u32 Ladder;
		rage::u32 Jump;
		rage::u32 Run;
		rage::u32 Walk;
		rage::u32 Sprint;
	}; // struct OldShoeSettings
	
	// 
	// MovementShoeSettings
	// 
	enum MovementShoeSettingsFlagIds
	{
		FLAG_ID_MOVEMENTSHOESETTINGS_MAX = 0,
	}; // enum MovementShoeSettingsFlagIds
	
	struct MovementShoeSettings
	{
		static const rage::u32 TYPE_ID = 20;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		MovementShoeSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			AudioEventLoudness(FOOTSTEP_LOUDNESS_MEDIUM)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::audMetadataRef HighHeels;
		rage::audMetadataRef Leather;
		rage::audMetadataRef RubberHard;
		rage::audMetadataRef RubberSoft;
		rage::audMetadataRef Bare;
		rage::audMetadataRef HeavyBoots;
		rage::audMetadataRef FlipFlops;
		rage::u8 AudioEventLoudness;
	}; // struct MovementShoeSettings
	
	// 
	// FootstepSurfaceSounds
	// 
	enum FootstepSurfaceSoundsFlagIds
	{
		FLAG_ID_FOOTSTEPSURFACESOUNDS_MAX = 0,
	}; // enum FootstepSurfaceSoundsFlagIds
	
	struct FootstepSurfaceSounds
	{
		static const rage::u32 TYPE_ID = 21;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		FootstepSurfaceSounds() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::audMetadataRef Normal;
	}; // struct FootstepSurfaceSounds
	
	// 
	// ModelPhysicsParams
	// 
	enum ModelPhysicsParamsFlagIds
	{
		FLAG_ID_MODELPHYSICSPARAMS_MAX = 0,
	}; // enum ModelPhysicsParamsFlagIds
	
	struct ModelPhysicsParams
	{
		static const rage::u32 TYPE_ID = 22;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ModelPhysicsParams() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			NumFeet(2U),
			PedType(HUMAN),
			FootstepTuningValues(2925934527U), // DEFAULT_TUNING_VALUES
			StopSpeedThreshold(0.9f),
			WalkSpeedThreshold(2.0f),
			RunSpeedThreshold(4.9f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 NumFeet;
		rage::u8 PedType;
		rage::u32 FootstepTuningValues;
		float StopSpeedThreshold;
		float WalkSpeedThreshold;
		float RunSpeedThreshold;
		
		static const rage::u8 MAX_MODES = 20;
		rage::u8 numModes;
		struct tModes
		{
			rage::u32 Name;
			float GroundDistanceDownEpsilon;
			float GroundDistanceUpEpsilon;
			float DownSpeedEpsilon;
			float HindGroundDistanceDownEpsilon;
			float HindGroundDistanceUpEpsilon;
			float SpeedSmootherIncreaseRate;
			float SpeedSmootherDecreaseRate;
			rage::u32 TimeToRetrigger;
		} Modes[MAX_MODES]; // struct tModes
		
	}; // struct ModelPhysicsParams
	
	// 
	// SkiAudioSettings
	// 
	enum SkiAudioSettingsFlagIds
	{
		FLAG_ID_SKIAUDIOSETTINGS_MAX = 0,
	}; // enum SkiAudioSettingsFlagIds
	
	struct SkiAudioSettings
	{
		static const rage::u32 TYPE_ID = 23;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		SkiAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			StraightSound(3817852694U), // NULL_SOUND
			MinSpeed(0.0f),
			MaxSpeed(20.0f),
			TurnSound(3817852694U), // NULL_SOUND
			MinTurn(0.0f),
			MaxTurn(0.3f),
			TurnEdgeSound(3817852694U), // NULL_SOUND
			MinSlopeEdge(0.0f),
			MaxSlopeEdge(0.5f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 StraightSound;
		float MinSpeed;
		float MaxSpeed;
		rage::u32 TurnSound;
		float MinTurn;
		float MaxTurn;
		rage::u32 TurnEdgeSound;
		float MinSlopeEdge;
		float MaxSlopeEdge;
	}; // struct SkiAudioSettings
	
	// 
	// RadioStationList
	// 
	enum RadioStationListFlagIds
	{
		FLAG_ID_RADIOSTATIONLIST_MAX = 0,
	}; // enum RadioStationListFlagIds
	
	struct RadioStationList
	{
		static const rage::u32 TYPE_ID = 24;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		RadioStationList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_STATIONS = 100;
		rage::u8 numStations;
		struct tStation
		{
			rage::audMetadataRef StationSettings;
		} Station[MAX_STATIONS]; // struct tStation
		
	}; // struct RadioStationList
	
	// 
	// RadioStationSettings
	// 
	enum RadioStationSettingsFlagIds
	{
		FLAG_ID_RADIOSTATIONSETTINGS_NOBACK2BACKMUSIC = 0,
		FLAG_ID_RADIOSTATIONSETTINGS_BACK2BACKADS,
		FLAG_ID_RADIOSTATIONSETTINGS_PLAYWEATHER,
		FLAG_ID_RADIOSTATIONSETTINGS_PLAYNEWS,
		FLAG_ID_RADIOSTATIONSETTINGS_SEQUENTIALMUSIC,
		FLAG_ID_RADIOSTATIONSETTINGS_IDENTSINSTEADOFADS,
		FLAG_ID_RADIOSTATIONSETTINGS_LOCKED,
		FLAG_ID_RADIOSTATIONSETTINGS_HIDDEN,
		FLAG_ID_RADIOSTATIONSETTINGS_PLAYSUSERSMUSIC,
		FLAG_ID_RADIOSTATIONSETTINGS_HASREVERBCHANNEL,
		FLAG_ID_RADIOSTATIONSETTINGS_ISMIXSTATION,
		FLAG_ID_RADIOSTATIONSETTINGS_MAX,
	}; // enum RadioStationSettingsFlagIds
	
	struct RadioStationSettings
	{
		static const rage::u32 TYPE_ID = 25;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		RadioStationSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			Order(1000),
			NextStationSettingsPtr(0U),
			Genre(RADIO_GENRE_UNSPECIFIED),
			AmbientRadioVol(0)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::s32 Order;
		rage::u32 NextStationSettingsPtr;
		rage::u8 Genre;
		rage::s8 AmbientRadioVol;
		char Name[32];
		
		static const rage::u32 MAX_TRACKLISTS = 30;
		rage::u32 numTrackLists;
		struct tTrackList
		{
			rage::audMetadataRef TrackListRef;
		} TrackList[MAX_TRACKLISTS]; // struct tTrackList
		
	}; // struct RadioStationSettings
	
	// 
	// RadioStationTrackList
	// 
	enum RadioStationTrackListFlagIds
	{
		FLAG_ID_RADIOSTATIONTRACKLIST_LOCKED = 0,
		FLAG_ID_RADIOSTATIONTRACKLIST_SHUFFLE,
		FLAG_ID_RADIOSTATIONTRACKLIST_MAX,
	}; // enum RadioStationTrackListFlagIds
	
	struct RadioStationTrackList
	{
		static const rage::u32 TYPE_ID = 26;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		RadioStationTrackList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			Category(RADIO_TRACK_CAT_MUSIC),
			NextValidSelectionTime(0U),
			HistoryWriteIndex(0),
			TotalNumTracks(0),
			NextTrackListPtr(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u8 Category;
		rage::u32 NextValidSelectionTime;
		rage::u8 HistoryWriteIndex;
		rage::u8 numHistorySpaceElems;
		rage::u32 HistorySpace[10];
		rage::u16 TotalNumTracks;
		rage::u32 NextTrackListPtr;
		
		static const rage::u8 MAX_TRACKS = 250;
		rage::u8 numTracks;
		struct tTrack
		{
			rage::u32 Context;
			rage::u32 SoundRef;
		} Track[MAX_TRACKS]; // struct tTrack
		
	}; // struct RadioStationTrackList
	
	// 
	// RadioTrackCategoryData
	// 
	enum RadioTrackCategoryDataFlagIds
	{
		FLAG_ID_RADIOTRACKCATEGORYDATA_MAX = 0,
	}; // enum RadioTrackCategoryDataFlagIds
	
	struct RadioTrackCategoryData
	{
		static const rage::u32 TYPE_ID = 27;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		RadioTrackCategoryData() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_CATEGORYS = 30;
		rage::u8 numCategorys;
		struct tCategory
		{
			rage::u8 Category;
			rage::u32 Value;
		} Category[MAX_CATEGORYS]; // struct tCategory
		
	}; // struct RadioTrackCategoryData
	
	// 
	// ScannerCrimeReport
	// 
	enum ScannerCrimeReportFlagIds
	{
		FLAG_ID_SCANNERCRIMEREPORT_MAX = 0,
	}; // enum ScannerCrimeReportFlagIds
	
	struct ScannerCrimeReport
	{
		static const rage::u32 TYPE_ID = 28;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ScannerCrimeReport() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			GenericInstructionSndRef(3817852694U), // NULL_SOUND
			PedsAroundInstructionSndRef(3817852694U), // NULL_SOUND
			PoliceAroundInstructionSndRef(3817852694U), // NULL_SOUND
			AcknowledgeSituationProbability(0.5f),
			SmallCrimeSoundRef(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 GenericInstructionSndRef;
		rage::u32 PedsAroundInstructionSndRef;
		rage::u32 PoliceAroundInstructionSndRef;
		float AcknowledgeSituationProbability;
		rage::u32 SmallCrimeSoundRef;
		
		static const rage::u8 MAX_CRIMESETS = 32;
		rage::u8 numCrimeSets;
		struct tCrimeSet
		{
			rage::u32 SoundRef;
			float Weight;
		} CrimeSet[MAX_CRIMESETS]; // struct tCrimeSet
		
	}; // struct ScannerCrimeReport
	
	// 
	// PedRaceToPVG
	// 
	enum PedRaceToPVGFlagIds
	{
		FLAG_ID_PEDRACETOPVG_MAX = 0,
	}; // enum PedRaceToPVGFlagIds
	
	struct PedRaceToPVG : AudBaseObject
	{
		static const rage::u32 TYPE_ID = 29;
		static const rage::u32 BASE_TYPE_ID = AudBaseObject::TYPE_ID;
		
		PedRaceToPVG() :
			Universal(0U),
			White(0U),
			Black(0U),
			Chinese(0U),
			Latino(0U),
			Arab(0U),
			Balkan(0U),
			Jamaican(0U),
			Korean(0U),
			Italian(0U),
			Pakistani(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 Universal;
		rage::u32 White;
		rage::u32 Black;
		rage::u32 Chinese;
		rage::u32 Latino;
		rage::u32 Arab;
		rage::u32 Balkan;
		rage::u32 Jamaican;
		rage::u32 Korean;
		rage::u32 Italian;
		rage::u32 Pakistani;
		
		static const rage::u8 MAX_FRIENDPVGS = 32;
		rage::u8 numFriendPVGs;
		struct tFriendPVGs
		{
			rage::u32 PVG;
		} FriendPVGs[MAX_FRIENDPVGS]; // struct tFriendPVGs
		
	}; // struct PedRaceToPVG : AudBaseObject
	
	// 
	// FriendGroup
	// 
	enum FriendGroupFlagIds
	{
		FLAG_ID_FRIENDGROUP_MAX = 0,
	}; // enum FriendGroupFlagIds
	
	struct FriendGroup : AudBaseObject
	{
		static const rage::u32 TYPE_ID = 31;
		static const rage::u32 BASE_TYPE_ID = AudBaseObject::TYPE_ID;
		
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
	}; // struct FriendGroup : AudBaseObject
	
	// 
	// StaticEmitterList
	// 
	enum StaticEmitterListFlagIds
	{
		FLAG_ID_STATICEMITTERLIST_MAX = 0,
	}; // enum StaticEmitterListFlagIds
	
	struct StaticEmitterList
	{
		static const rage::u32 TYPE_ID = 32;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		StaticEmitterList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_EMITTERS = 1024;
		rage::u16 numEmitters;
		struct tEmitter
		{
			rage::u32 StaticEmitter;
		} Emitter[MAX_EMITTERS]; // struct tEmitter
		
	}; // struct StaticEmitterList
	
	// 
	// ScriptedScannerLine
	// 
	enum ScriptedScannerLineFlagIds
	{
		FLAG_ID_SCRIPTEDSCANNERLINE_DISABLESCANNERSFX = 0,
		FLAG_ID_SCRIPTEDSCANNERLINE_ENABLEECHO,
		FLAG_ID_SCRIPTEDSCANNERLINE_MAX,
	}; // enum ScriptedScannerLineFlagIds
	
	struct ScriptedScannerLine
	{
		static const rage::u32 TYPE_ID = 33;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ScriptedScannerLine() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		
		static const rage::u16 MAX_PHRASES = 256;
		rage::u16 numPhrases;
		struct tPhrase
		{
			rage::u32 SoundRef;
			rage::u8 Slot;
			rage::u16 PostDelay;
		} Phrase[MAX_PHRASES]; // struct tPhrase
		
	}; // struct ScriptedScannerLine
	
	// 
	// ScannerArea
	// 
	enum ScannerAreaFlagIds
	{
		FLAG_ID_SCANNERAREA_MAX = 0,
	}; // enum ScannerAreaFlagIds
	
	struct ScannerArea
	{
		static const rage::u32 TYPE_ID = 34;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ScannerArea() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			ConjunctiveSound(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 ConjunctiveSound;
		
		struct tScannerAreaBits
		{
			tScannerAreaBits()
			 : Value(0U)
			{
			}
			union
			{
				rage::u8 Value;
				struct
				{
					#if __BE
						bool padding:7; // padding
						bool NeverPlayDirection:1;
					#else // __BE
						bool NeverPlayDirection:1;
						bool padding:7; // padding
					#endif // __BE
				} BitFields;
			};
		} ScannerAreaBits; // struct tScannerAreaBits
		
	}; // struct ScannerArea
	
	// 
	// ScannerSpecificLocation
	// 
	enum ScannerSpecificLocationFlagIds
	{
		FLAG_ID_SCANNERSPECIFICLOCATION_MAX = 0,
	}; // enum ScannerSpecificLocationFlagIds
	
	struct ScannerSpecificLocation
	{
		static const rage::u32 TYPE_ID = 35;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ScannerSpecificLocation() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Radius(100.0f),
			ProbOfPlaying(1.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		rage::Vector3 Position;
		
		float Radius;
		float ProbOfPlaying;
		
		static const rage::u8 MAX_SOUNDS = 16;
		rage::u8 numSounds;
		struct tSounds
		{
			rage::u32 Sound;
		} Sounds[MAX_SOUNDS]; // struct tSounds
		
	}; // struct ScannerSpecificLocation
	
	// 
	// ScannerSpecificLocationList
	// 
	enum ScannerSpecificLocationListFlagIds
	{
		FLAG_ID_SCANNERSPECIFICLOCATIONLIST_MAX = 0,
	}; // enum ScannerSpecificLocationListFlagIds
	
	struct ScannerSpecificLocationList
	{
		static const rage::u32 TYPE_ID = 36;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ScannerSpecificLocationList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_LOCATIONS = 1000;
		rage::u16 numLocations;
		struct tLocation
		{
			rage::u32 Ref;
		} Location[MAX_LOCATIONS]; // struct tLocation
		
	}; // struct ScannerSpecificLocationList
	
	// 
	// AmbientZone
	// 
	enum AmbientZoneFlagIds
	{
		FLAG_ID_AMBIENTZONE_INTERIORZONE = 0,
		FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT,
		FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT,
		FLAG_ID_AMBIENTZONE_USEPLAYERPOSITION,
		FLAG_ID_AMBIENTZONE_DISABLEINMULTIPLAYER,
		FLAG_ID_AMBIENTZONE_SCALEMAXWORLDHEIGHTWITHZONEINFLUENCE,
		FLAG_ID_AMBIENTZONE_HASTUNNELREFLECTIONS,
		FLAG_ID_AMBIENTZONE_HASREVERBLINKEDREFLECTIONS,
		FLAG_ID_AMBIENTZONE_HASRANDOMSTATICRADIOEMITTERS,
		FLAG_ID_AMBIENTZONE_HASRANDOMVEHICLERADIOEMITTERS,
		FLAG_ID_AMBIENTZONE_OCCLUDERAIN,
		FLAG_ID_AMBIENTZONE_MAX,
	}; // enum AmbientZoneFlagIds
	
	struct AmbientZone
	{
		static const rage::u32 TYPE_ID = 37;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AmbientZone() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			Shape(kAmbientZoneCuboid),
			BuiltUpFactor(0.0f),
			MinPedDensity(0.0f),
			MaxPedDensity(1.0f),
			PedDensityTOD(0U),
			PedDensityScalar(1.0f),
			MaxWindInfluence(-1.0f),
			MinWindInfluence(-1.0f),
			WindElevationSounds(0U),
			EnvironmentRule(0U),
			AudioScene(0U),
			UnderwaterCreakFactor(-1.0f),
			PedWallaSettings(0U),
			RandomisedRadioSettings(0U),
			NumRulesToPlay(4),
			ZoneWaterCalculation(AMBIENT_ZONE_FORCE_OVER_LAND)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u8 Shape;
		
		struct tActivationZone
		{
			
			rage::Vector3 Centre;
			
			
			rage::Vector3 Size;
			
			
			rage::Vector3 PostRotationOffset;
			
			
			rage::Vector3 SizeScale;
			
			rage::u16 RotationAngle;
		} ActivationZone; // struct tActivationZone
		
		
		struct tPositioningZone
		{
			
			rage::Vector3 Centre;
			
			
			rage::Vector3 Size;
			
			
			rage::Vector3 PostRotationOffset;
			
			
			rage::Vector3 SizeScale;
			
			rage::u16 RotationAngle;
		} PositioningZone; // struct tPositioningZone
		
		float BuiltUpFactor;
		float MinPedDensity;
		float MaxPedDensity;
		rage::u32 PedDensityTOD;
		float PedDensityScalar;
		float MaxWindInfluence;
		float MinWindInfluence;
		rage::u32 WindElevationSounds;
		rage::u32 EnvironmentRule;
		rage::u32 AudioScene;
		float UnderwaterCreakFactor;
		rage::u32 PedWallaSettings;
		rage::u32 RandomisedRadioSettings;
		rage::u8 NumRulesToPlay;
		rage::u8 ZoneWaterCalculation;
		
		static const rage::u8 MAX_RULES = 32;
		rage::u8 numRules;
		struct tRule
		{
			rage::u32 Ref;
		} Rule[MAX_RULES]; // struct tRule
		
		
		static const rage::u8 MAX_DIRAMBIENCES = 32;
		rage::u8 numDirAmbiences;
		struct tDirAmbience
		{
			rage::u32 Ref;
			float Volume;
		} DirAmbience[MAX_DIRAMBIENCES]; // struct tDirAmbience
		
	}; // struct AmbientZone
	
	// 
	// AmbientRule
	// 
	enum AmbientRuleFlagIds
	{
		FLAG_ID_AMBIENTRULE_IGNOREINITIALMINREPEATTIME = 0,
		FLAG_ID_AMBIENTRULE_STOPWHENRAINING,
		FLAG_ID_AMBIENTRULE_STOPONLOUDSOUND,
		FLAG_ID_AMBIENTRULE_FOLLOWLISTENER,
		FLAG_ID_AMBIENTRULE_USEOCCLUSION,
		FLAG_ID_AMBIENTRULE_STREAMINGSOUND,
		FLAG_ID_AMBIENTRULE_STOPONZONEDEACTIVATION,
		FLAG_ID_AMBIENTRULE_TRIGGERONLOUDSOUND,
		FLAG_ID_AMBIENTRULE_CANTRIGGEROVERWATER,
		FLAG_ID_AMBIENTRULE_CHECKCONDITIONSEACHFRAME,
		FLAG_ID_AMBIENTRULE_USEPLAYERPOSITION,
		FLAG_ID_AMBIENTRULE_DISABLEINMULTIPLAYER,
		FLAG_ID_AMBIENTRULE_MAX,
	}; // enum AmbientRuleFlagIds
	
	struct AmbientRule
	{
		static const rage::u32 TYPE_ID = 38;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AmbientRule() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			Sound(3817852694U), // NULL_SOUND
			Category(1155669136U), // BASE
			LastPlayTime(0U),
			DynamicBankID(-1),
			DynamicSlotType(0U),
			Weight(1.0f),
			MinDist(70.0f),
			MaxDist(250.0f),
			MinTimeMinutes(0),
			MaxTimeMinutes(1440),
			MinRepeatTime(0),
			MinRepeatTimeVariance(0),
			SpawnHeight(AMBIENCE_HEIGHT_RANDOM),
			ExplicitSpawnPositionUsage(RULE_EXPLICIT_SPAWN_POS_DISABLED),
			MaxLocalInstances(-1),
			MaxGlobalInstances(-1),
			BlockabilityFactor(100),
			MaxPathDepth(3)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		
		rage::Vector3 ExplicitSpawnPosition;
		
		rage::u32 Sound;
		rage::u32 Category;
		rage::u32 LastPlayTime;
		rage::s32 DynamicBankID;
		rage::u32 DynamicSlotType;
		float Weight;
		float MinDist;
		float MaxDist;
		rage::u16 MinTimeMinutes;
		rage::u16 MaxTimeMinutes;
		rage::u16 MinRepeatTime;
		rage::u16 MinRepeatTimeVariance;
		rage::u8 SpawnHeight;
		rage::u8 ExplicitSpawnPositionUsage;
		rage::s8 MaxLocalInstances;
		rage::s8 MaxGlobalInstances;
		rage::u8 BlockabilityFactor;
		rage::u8 MaxPathDepth;
		
		static const rage::u8 MAX_CONDITIONS = 5;
		rage::u8 numConditions;
		struct tCondition
		{
			rage::u32 ConditionVariable;
			float ConditionValue;
			rage::u8 ConditionType;
			rage::u8 BankLoading;
		} Condition[MAX_CONDITIONS]; // struct tCondition
		
	}; // struct AmbientRule
	
	// 
	// AmbientZoneList
	// 
	enum AmbientZoneListFlagIds
	{
		FLAG_ID_AMBIENTZONELIST_MAX = 0,
	}; // enum AmbientZoneListFlagIds
	
	struct AmbientZoneList
	{
		static const rage::u32 TYPE_ID = 39;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AmbientZoneList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_ZONES = 1000;
		rage::u16 numZones;
		struct tZone
		{
			rage::u32 Ref;
		} Zone[MAX_ZONES]; // struct tZone
		
	}; // struct AmbientZoneList
	
	// 
	// AmbientSlotMap
	// 
	enum AmbientSlotMapFlagIds
	{
		FLAG_ID_AMBIENTSLOTMAP_MAX = 0,
	}; // enum AmbientSlotMapFlagIds
	
	struct AmbientSlotMap
	{
		static const rage::u32 TYPE_ID = 40;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AmbientSlotMap() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_SLOTMAPS = 255;
		rage::u8 numSlotMaps;
		struct tSlotMap
		{
			rage::u32 WaveSlotName;
			rage::u32 SlotType;
			rage::u32 Priority;
		} SlotMap[MAX_SLOTMAPS]; // struct tSlotMap
		
	}; // struct AmbientSlotMap
	
	// 
	// AmbientBankMap
	// 
	enum AmbientBankMapFlagIds
	{
		FLAG_ID_AMBIENTBANKMAP_MAX = 0,
	}; // enum AmbientBankMapFlagIds
	
	struct AmbientBankMap
	{
		static const rage::u32 TYPE_ID = 41;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AmbientBankMap() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_BANKMAPS = 255;
		rage::u8 numBankMaps;
		struct tBankMap
		{
			rage::u32 BankName;
			rage::u32 SlotType;
		} BankMap[MAX_BANKMAPS]; // struct tBankMap
		
	}; // struct AmbientBankMap
	
	// 
	// EnvironmentRule
	// 
	enum EnvironmentRuleFlagIds
	{
		FLAG_ID_ENVIRONMENTRULE_OVERRIDEREVERB = 0,
		FLAG_ID_ENVIRONMENTRULE_OVERRIDEECHOS,
		FLAG_ID_ENVIRONMENTRULE_RANDOMECHOPOSITIONS,
		FLAG_ID_ENVIRONMENTRULE_HEIGHTRESTRICTIONS,
		FLAG_ID_ENVIRONMENTRULE_MAX,
	}; // enum EnvironmentRuleFlagIds
	
	struct EnvironmentRule : AudBaseObject
	{
		static const rage::u32 TYPE_ID = 42;
		static const rage::u32 BASE_TYPE_ID = AudBaseObject::TYPE_ID;
		
		EnvironmentRule() :
			ReverbSmall(0.0f),
			ReverbMedium(0.0f),
			ReverbLarge(0.0f),
			ReverbDamp(0.0f),
			EchoDelay(250.0f),
			EchoDelayVariance(75.0f),
			EchoAttenuation(-100.0f),
			EchoNumber(0),
			EchoSoundList(0U),
			BaseEchoVolumeModifier(0.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		float ReverbSmall;
		float ReverbMedium;
		float ReverbLarge;
		float ReverbDamp;
		float EchoDelay;
		float EchoDelayVariance;
		float EchoAttenuation;
		rage::u8 EchoNumber;
		rage::u32 EchoSoundList;
		float BaseEchoVolumeModifier;
	}; // struct EnvironmentRule : AudBaseObject
	
	// 
	// TrainStation
	// 
	enum TrainStationFlagIds
	{
		FLAG_ID_TRAINSTATION_MAX = 0,
	}; // enum TrainStationFlagIds
	
	struct TrainStation
	{
		static const rage::u32 TYPE_ID = 43;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		TrainStation() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			StationNameSound(3817852694U), // NULL_SOUND
			TransferSound(3817852694U), // NULL_SOUND
			Letter(3817852694U), // NULL_SOUND
			ApproachingStation(3817852694U), // NULL_SOUND
			SpareSound1(3817852694U), // NULL_SOUND
			SpareSound2(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 StationNameSound;
		rage::u32 TransferSound;
		rage::u32 Letter;
		rage::u32 ApproachingStation;
		rage::u32 SpareSound1;
		rage::u32 SpareSound2;
		rage::u8 StationNameLen;
		char StationName[255];
	}; // struct TrainStation
	
	// 
	// InteriorSettings
	// 
	enum InteriorSettingsFlagIds
	{
		FLAG_ID_INTERIORSETTINGS_HASINTERIORWALLA = 0,
		FLAG_ID_INTERIORSETTINGS_INHIBITIDLEMUSIC,
		FLAG_ID_INTERIORSETTINGS_BUILDALLEXTENDEDPATHS,
		FLAG_ID_INTERIORSETTINGS_BUILDINTERINTERIORPATHS,
		FLAG_ID_INTERIORSETTINGS_ALLOWQUIETSCENE,
		FLAG_ID_INTERIORSETTINGS_USEBOUNDINGBOXOCCLUSION,
		FLAG_ID_INTERIORSETTINGS_MAX,
	}; // enum InteriorSettingsFlagIds
	
	struct InteriorSettings
	{
		static const rage::u32 TYPE_ID = 44;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		InteriorSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			InteriorWallaSoundSet(3565506855U), // INTERIOR_WALLA_STREAMED_DEFAULT
			InteriorReflections(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 InteriorWallaSoundSet;
		rage::u32 InteriorReflections;
		
		static const rage::u8 MAX_ROOMS = 64;
		rage::u8 numRooms;
		struct tRoom
		{
			rage::audMetadataRef Ref;
		} Room[MAX_ROOMS]; // struct tRoom
		
	}; // struct InteriorSettings
	
	// 
	// InteriorWeaponMetrics
	// 
	enum InteriorWeaponMetricsFlagIds
	{
		FLAG_ID_INTERIORWEAPONMETRICS_MAX = 0,
	}; // enum InteriorWeaponMetricsFlagIds
	
	struct InteriorWeaponMetrics
	{
		static const rage::u32 TYPE_ID = 45;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		InteriorWeaponMetrics() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Wetness(0.0f),
			Visability(0.2f),
			LPF(24000U),
			Predelay(0U),
			Hold(0.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		float Wetness;
		float Visability;
		rage::u32 LPF;
		rage::u32 Predelay;
		float Hold;
	}; // struct InteriorWeaponMetrics
	
	// 
	// InteriorRoom
	// 
	enum InteriorRoomFlagIds
	{
		FLAG_ID_INTERIORROOM_HASINTERIORWALLA = 0,
		FLAG_ID_INTERIORROOM_MAX,
	}; // enum InteriorRoomFlagIds
	
	struct InteriorRoom
	{
		static const rage::u32 TYPE_ID = 46;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		InteriorRoom() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			RoomName(0U),
			InteriorType(INTERIOR_TYPE_NONE),
			ReverbSmall(0.0f),
			ReverbMedium(0.0f),
			ReverbLarge(0.0f),
			RoomToneSound(3817852694U), // NULL_SOUND
			RainType(RAIN_TYPE_NONE),
			ExteriorAudibility(1.0f),
			RoomOcclusionDamping(0.0f),
			NonMarkedPortalOcclusion(0.0f),
			DistanceFromPortalForOcclusion(0.0f),
			DistanceFromPortalFadeDistance(0.0f),
			WeaponMetrics(0U),
			InteriorWallaSoundSet(3565506855U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 RoomName;
		rage::audMetadataRef AmbientZone;
		rage::u8 InteriorType;
		float ReverbSmall;
		float ReverbMedium;
		float ReverbLarge;
		rage::u32 RoomToneSound;
		rage::u8 RainType;
		float ExteriorAudibility;
		float RoomOcclusionDamping;
		float NonMarkedPortalOcclusion;
		float DistanceFromPortalForOcclusion;
		float DistanceFromPortalFadeDistance;
		rage::u32 WeaponMetrics;
		rage::u32 InteriorWallaSoundSet;
	}; // struct InteriorRoom
	
	// 
	// DoorAudioSettings
	// 
	enum DoorAudioSettingsFlagIds
	{
		FLAG_ID_DOORAUDIOSETTINGS_MAX = 0,
	}; // enum DoorAudioSettingsFlagIds
	
	struct DoorAudioSettings
	{
		static const rage::u32 TYPE_ID = 47;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		DoorAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Sounds(0U),
			MaxOcclusion(0.7f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 Sounds;
		rage::audMetadataRef TuningParams;
		float MaxOcclusion;
	}; // struct DoorAudioSettings
	
	// 
	// DoorTuningParams
	// 
	enum DoorTuningParamsFlagIds
	{
		FLAG_ID_DOORTUNINGPARAMS_ISRAILLEVELCROSSING = 0,
		FLAG_ID_DOORTUNINGPARAMS_MAX,
	}; // enum DoorTuningParamsFlagIds
	
	struct DoorTuningParams
	{
		static const rage::u32 TYPE_ID = 48;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		DoorTuningParams() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			OpenThresh(0.0f),
			HeadingThresh(0.0f),
			ClosedThresh(0.0f),
			SpeedThresh(0.1f),
			SpeedScale(1.0f),
			HeadingDeltaThreshold(0.0001f),
			AngularVelocityThreshold(0.003f),
			DoorType(AUD_DOOR_TYPE_STD)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		float OpenThresh;
		float HeadingThresh;
		float ClosedThresh;
		float SpeedThresh;
		float SpeedScale;
		float HeadingDeltaThreshold;
		float AngularVelocityThreshold;
		rage::u8 DoorType;
	}; // struct DoorTuningParams
	
	// 
	// DoorList
	// 
	enum DoorListFlagIds
	{
		FLAG_ID_DOORLIST_MAX = 0,
	}; // enum DoorListFlagIds
	
	struct DoorList
	{
		static const rage::u32 TYPE_ID = 49;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		DoorList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_DOORS = 1000;
		rage::u16 numDoors;
		struct tDoor
		{
			rage::u32 PropName;
			rage::audMetadataRef DoorAudioSettings;
		} Door[MAX_DOORS]; // struct tDoor
		
	}; // struct DoorList
	
	// 
	// ItemAudioSettings
	// 
	enum ItemAudioSettingsFlagIds
	{
		FLAG_ID_ITEMAUDIOSETTINGS_MAX = 0,
	}; // enum ItemAudioSettingsFlagIds
	
	struct ItemAudioSettings
	{
		static const rage::u32 TYPE_ID = 50;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ItemAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::audMetadataRef WeaponSettings;
		
		static const rage::u8 MAX_CONTEXTSETTINGS = 64;
		rage::u8 numContextSettings;
		struct tContextSettings
		{
			rage::u32 Context;
			rage::audMetadataRef WeaponSettings;
		} ContextSettings[MAX_CONTEXTSETTINGS]; // struct tContextSettings
		
	}; // struct ItemAudioSettings
	
	// 
	// ClimbingAudioSettings
	// 
	enum ClimbingAudioSettingsFlagIds
	{
		FLAG_ID_CLIMBINGAUDIOSETTINGS_MAX = 0,
	}; // enum ClimbingAudioSettingsFlagIds
	
	struct ClimbingAudioSettings
	{
		static const rage::u32 TYPE_ID = 51;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ClimbingAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			climbLaunch(3817852694U), // NULL_SOUND
			climbFoot(3817852694U), // NULL_SOUND
			climbKnee(3817852694U), // NULL_SOUND
			climbScrape(3817852694U), // NULL_SOUND
			climbHand(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 climbLaunch;
		rage::u32 climbFoot;
		rage::u32 climbKnee;
		rage::u32 climbScrape;
		rage::u32 climbHand;
	}; // struct ClimbingAudioSettings
	
	// 
	// ModelAudioCollisionSettings
	// 
	enum ModelAudioCollisionSettingsFlagIds
	{
		FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_ISRESONANT = 0,
		FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_TURNOFFRAINVOLONPROPS,
		FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_SWINGINGPROP,
		FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_RAINLOOPTRACKSLISTENER,
		FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_PLACERAINLOOPONTOPOFBOUNDS,
		FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_RESONANCELOOPTRACKSLISTENER,
		FLAG_ID_MODELAUDIOCOLLISIONSETTINGS_MAX,
	}; // enum ModelAudioCollisionSettingsFlagIds
	
	struct ModelAudioCollisionSettings
	{
		static const rage::u32 TYPE_ID = 52;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ModelAudioCollisionSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			LastFragTime(0U),
			MediumIntensity(0U),
			HighIntensity(0U),
			BreakSound(3817852694U), // NULL_SOUND
			DestroySound(3817852694U), // NULL_SOUND
			UprootSound(3817852694U), // NULL_SOUND
			WindSounds(0U),
			SwingSound(3817852694U), // NULL_SOUND
			MinSwingVel(0.1f),
			MaxswingVel(5.0f),
			RainLoop(3817852694U), // NULL_SOUND
			ShockwaveSound(3817852694U), // NULL_SOUND
			RandomAmbient(3817852694U), // NULL_SOUND
			EntityResonance(3817852694U), // NULL_SOUND
			Weight(AUDIO_WEIGHT_M)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 LastFragTime;
		rage::u32 MediumIntensity;
		rage::u32 HighIntensity;
		rage::u32 BreakSound;
		rage::u32 DestroySound;
		rage::u32 UprootSound;
		rage::u32 WindSounds;
		rage::u32 SwingSound;
		float MinSwingVel;
		float MaxswingVel;
		rage::u32 RainLoop;
		rage::u32 ShockwaveSound;
		rage::u32 RandomAmbient;
		rage::u32 EntityResonance;
		rage::u8 Weight;
		
		static const rage::u8 MAX_MATERIALOVERRIDES = 64;
		rage::u8 numMaterialOverrides;
		struct tMaterialOverride
		{
			rage::audMetadataRef Material;
			rage::audMetadataRef Override;
		} MaterialOverride[MAX_MATERIALOVERRIDES]; // struct tMaterialOverride
		
		
		static const rage::u32 MAX_FRAGCOMPONENTSETTINGS = 256;
		rage::u32 numFragComponentSettings;
		struct tFragComponentSetting
		{
			rage::audMetadataRef ModelSettings;
		} FragComponentSetting[MAX_FRAGCOMPONENTSETTINGS]; // struct tFragComponentSetting
		
	}; // struct ModelAudioCollisionSettings
	
	// 
	// TrainAudioSettings
	// 
	enum TrainAudioSettingsFlagIds
	{
		FLAG_ID_TRAINAUDIOSETTINGS_MAX = 0,
	}; // enum TrainAudioSettingsFlagIds
	
	struct TrainAudioSettings
	{
		static const rage::u32 TYPE_ID = 53;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		TrainAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			DriveTone(0U),
			DriveToneSynth(0U),
			IdleLoop(0U),
			Brakes(0U),
			BigBrakeRelease(0U),
			BrakeRelease(0U),
			WheelDry(0U),
			AirHornOneShot(0U),
			BellLoopCrosing(0U),
			BellLoopEngine(0U),
			AmbientCarriage(0U),
			AmbientRumble(0U),
			AmbientGrind(0U),
			AmbientSqueal(0U),
			CarriagePitchCurve(424604923U), // SUBWAY_CARRIAGE_PITCH
			CarriageVolumeCurve(2086004744U), // SUBWAY_CARRIAGE_VOLUME
			DriveTonePitchCurve(3575425038U), // SUBWAY_DRIVE_TONE_PITCH
			DriveToneVolumeCurve(2315049992U), // SUBWAY_DRIVE_TONE_VOLUME
			DriveToneSynthPitchCurve(2853505231U), // SUBWAY_DRIVE_TONE_SYNTH_PITCH
			DriveToneSynthVolumeCurve(2362081187U), // SUBWAY_DRIVE_TONE_SYNTH_VOLUME
			GrindPitchCurve(3123948585U), // SUBWAY_GRIND_PITCH
			GrindVolumeCurve(3669158791U), // SUBWAY_GRIND_VOLUME
			TrainIdlePitchCurve(135851773U), // SUBWAY_IDLE_PITCH
			TrainIdleVolumeCurve(2790246344U), // SUBWAY_IDLE_VOLUME
			SquealPitchCurve(3096118849U), // SUBWAY_SQUEAL_PITCH
			SquealVolumeCurve(1034145554U), // SUBWAY_SQUEAL_VOLUME
			ScrapeSpeedVolumeCurve(3873334346U), // SUBWAY_SCRAPE_SPEED_VOLUME
			WheelVolumeCurve(1180265007U), // SUBWAY_WHEEL_VOLUME
			WheelDelayCurve(855040938U), // SUBWAY_WHEEL_DELAY
			RumbleVolumeCurve(1886860914U), // SUBWAY_RUMBLE_VOLUME
			BrakeVelocityPitchCurve(3077542682U), // SUBWAY_BRAKE_VELOCITY_PITCH
			BrakeVelocityVolumeCurve(1703205213U), // SUBWAY_BRAKE_VELOCITY_VOLUME
			BrakeAccelerationPitchCurve(2388576451U), // SUBWAY_BRAKE_ACCELERATION_PITCH
			BrakeAccelerationVolumeCurve(844880444U), // SUBWAY_BRAKE_ACCELERATION_VOLUME
			TrainApproachTrackRumble(4028963543U), // TRAIN_APPROACH_TRACK_RUMBLE
			TrackRumbleDistanceToIntensity(4156373648U), // SUBWAY_RUMBLE_DISTANCE_TO_INTENSITY
			TrainDistanceToRollOffScale(3454258691U), // CONSTANT_ONE
			VehicleCollisions(1122753141U), // VEHICLE_COLLISION_TRAIN
			ShockwaveIntensityScale(1.0f),
			ShockwaveRadiusScale(1.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 DriveTone;
		rage::u32 DriveToneSynth;
		rage::u32 IdleLoop;
		rage::u32 Brakes;
		rage::u32 BigBrakeRelease;
		rage::u32 BrakeRelease;
		rage::u32 WheelDry;
		rage::u32 AirHornOneShot;
		rage::u32 BellLoopCrosing;
		rage::u32 BellLoopEngine;
		rage::u32 AmbientCarriage;
		rage::u32 AmbientRumble;
		rage::u32 AmbientGrind;
		rage::u32 AmbientSqueal;
		rage::u32 CarriagePitchCurve;
		rage::u32 CarriageVolumeCurve;
		rage::u32 DriveTonePitchCurve;
		rage::u32 DriveToneVolumeCurve;
		rage::u32 DriveToneSynthPitchCurve;
		rage::u32 DriveToneSynthVolumeCurve;
		rage::u32 GrindPitchCurve;
		rage::u32 GrindVolumeCurve;
		rage::u32 TrainIdlePitchCurve;
		rage::u32 TrainIdleVolumeCurve;
		rage::u32 SquealPitchCurve;
		rage::u32 SquealVolumeCurve;
		rage::u32 ScrapeSpeedVolumeCurve;
		rage::u32 WheelVolumeCurve;
		rage::u32 WheelDelayCurve;
		rage::u32 RumbleVolumeCurve;
		rage::u32 BrakeVelocityPitchCurve;
		rage::u32 BrakeVelocityVolumeCurve;
		rage::u32 BrakeAccelerationPitchCurve;
		rage::u32 BrakeAccelerationVolumeCurve;
		rage::u32 TrainApproachTrackRumble;
		rage::u32 TrackRumbleDistanceToIntensity;
		rage::u32 TrainDistanceToRollOffScale;
		rage::u32 VehicleCollisions;
		float ShockwaveIntensityScale;
		float ShockwaveRadiusScale;
	}; // struct TrainAudioSettings
	
	// 
	// WeatherAudioSettings
	// 
	enum WeatherAudioSettingsFlagIds
	{
		FLAG_ID_WEATHERAUDIOSETTINGS_MAX = 0,
	}; // enum WeatherAudioSettingsFlagIds
	
	struct WeatherAudioSettings
	{
		static const rage::u32 TYPE_ID = 54;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		WeatherAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Strength(0.0f),
			Blustery(0.0f),
			Temperature(0.0f),
			TimeOfDayAffectsTemperature(0.0f),
			WhistleVolumeOffset(0.0f),
			WindGust(3817852694U), // NULL_SOUND
			AudioScene(0U),
			WindGustEnd(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		float Strength;
		float Blustery;
		float Temperature;
		float TimeOfDayAffectsTemperature;
		float WhistleVolumeOffset;
		rage::u32 WindGust;
		rage::u32 AudioScene;
		rage::u32 WindGustEnd;
		
		static const rage::u8 MAX_WINDSOUNDS = 6;
		rage::u8 numWindSounds;
		struct tWindSound
		{
			rage::u32 WindSoundComponent;
		} WindSound[MAX_WINDSOUNDS]; // struct tWindSound
		
	}; // struct WeatherAudioSettings
	
	// 
	// WeatherTypeAudioReference
	// 
	enum WeatherTypeAudioReferenceFlagIds
	{
		FLAG_ID_WEATHERTYPEAUDIOREFERENCE_MAX = 0,
	}; // enum WeatherTypeAudioReferenceFlagIds
	
	struct WeatherTypeAudioReference
	{
		static const rage::u32 TYPE_ID = 55;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		WeatherTypeAudioReference() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_REFERENCES = 32;
		rage::u8 numReferences;
		struct tReference
		{
			rage::u32 WeatherName;
			rage::audMetadataRef WeatherType;
		} Reference[MAX_REFERENCES]; // struct tReference
		
	}; // struct WeatherTypeAudioReference
	
	// 
	// BicycleAudioSettings
	// 
	enum BicycleAudioSettingsFlagIds
	{
		FLAG_ID_BICYCLEAUDIOSETTINGS_MAX = 0,
	}; // enum BicycleAudioSettingsFlagIds
	
	struct BicycleAudioSettings
	{
		static const rage::u32 TYPE_ID = 56;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		BicycleAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			ChainLoop(3817852694U), // NULL_SOUND
			SprocketSound(3817852694U), // NULL_SOUND
			RePedalSound(3817852694U), // NULL_SOUND
			GearChangeSound(3817852694U), // NULL_SOUND
			RoadNoiseVolumeScale(1.0f),
			SkidVolumeScale(1.0f),
			SuspensionUp(2095958071U), // SUSPENSION_UP
			SuspensionDown(1257191667U), // SUSPENSION_DOWN
			MinSuspCompThresh(0.45f),
			MaxSuspCompThres(6.9f),
			JumpLandSound(2783670720U), // JUMP_LAND_INTACT
			DamagedJumpLandSound(452066476U), // JUMP_LAND_LOOSE
			JumpLandMinThresh(31U),
			JumpLandMaxThresh(36U),
			VehicleCollisions(2398302013U), // VEHICLE_COLLISION_BICYCLE
			BellSound(3817852694U), // NULL_SOUND
			BrakeBlock(3817852694U), // NULL_SOUND
			BrakeBlockWet(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 ChainLoop;
		rage::u32 SprocketSound;
		rage::u32 RePedalSound;
		rage::u32 GearChangeSound;
		float RoadNoiseVolumeScale;
		float SkidVolumeScale;
		rage::u32 SuspensionUp;
		rage::u32 SuspensionDown;
		float MinSuspCompThresh;
		float MaxSuspCompThres;
		rage::u32 JumpLandSound;
		rage::u32 DamagedJumpLandSound;
		rage::u32 JumpLandMinThresh;
		rage::u32 JumpLandMaxThresh;
		rage::u32 VehicleCollisions;
		rage::u32 BellSound;
		rage::u32 BrakeBlock;
		rage::u32 BrakeBlockWet;
	}; // struct BicycleAudioSettings
	
	// 
	// PlaneAudioSettings
	// 
	enum PlaneAudioSettingsFlagIds
	{
		FLAG_ID_PLANEAUDIOSETTINGS_ENGINESATTACHEDTOWINGS = 0,
		FLAG_ID_PLANEAUDIOSETTINGS_HASMISSILELOCKSYSTEM,
		FLAG_ID_PLANEAUDIOSETTINGS_ISJET,
		FLAG_ID_PLANEAUDIOSETTINGS_DISABLEAMBIENTRADIO,
		FLAG_ID_PLANEAUDIOSETTINGS_AIRCRAFTWARNINGVOICEISMALE,
		FLAG_ID_PLANEAUDIOSETTINGS_MAX,
	}; // enum PlaneAudioSettingsFlagIds
	
	struct PlaneAudioSettings
	{
		static const rage::u32 TYPE_ID = 57;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		PlaneAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			EngineLoop(3817852694U), // NULL_SOUND
			ExhaustLoop(3817852694U), // NULL_SOUND
			IdleLoop(3817852694U), // NULL_SOUND
			DistanceLoop(3817852694U), // NULL_SOUND
			PropellorLoop(3817852694U), // NULL_SOUND
			BankingLoop(3817852694U), // NULL_SOUND
			EngineConeFrontAngle(0),
			EngineConeRearAngle(0),
			EngineConeAtten(-1500),
			ExhaustConeFrontAngle(0),
			ExhaustConeRearAngle(0),
			ExhaustConeAtten(-1500),
			PropellorConeFrontAngle(0),
			PropellorConeRearAngle(0),
			PropellorConeAtten(-1500),
			EngineThrottleVolumeCurve(0U),
			EngineThrottlePitchCurve(0U),
			ExhaustThrottleVolumeCurve(0U),
			ExhaustThrottlePitchCurve(0U),
			PropellorThrottleVolumeCurve(0U),
			PropellorThrottlePitchCurve(0U),
			IdleThrottleVolumeCurve(0U),
			IdleThrottlePitchCurve(0U),
			DistanceThrottleVolumeCurve(0U),
			DistanceThrottlePitchCurve(0U),
			DistanceVolumeCurve(0U),
			StallWarning(3817852694U), // NULL_SOUND
			DoorOpen(3817852694U), // NULL_SOUND
			DoorClose(3817852694U), // NULL_SOUND
			DoorLimit(3817852694U), // NULL_SOUND
			LandingGearDeploy(3817852694U), // NULL_SOUND
			LandingGearRetract(3817852694U), // NULL_SOUND
			Ignition(3817852694U), // NULL_SOUND
			TyreSqueal(3817852694U), // NULL_SOUND
			BankingThrottleVolumeCurve(0U),
			BankingThrottlePitchCurve(0U),
			BankingAngleVolCurve(0U),
			AfterburnerLoop(0U),
			BankingStyle(kRotationAngle),
			AfterburnerThrottleVolCurve(0U),
			EngineSynthDef(0U),
			EngineSynthPreset(0U),
			ExhaustSynthDef(0U),
			ExhaustSynthPreset(0U),
			EngineSubmixVoice(770649551U), // DEFAULT_PLANE_ENGINE_SUBMIX_CONTROL
			ExhaustSubmixVoice(911897457U), // DEFAULT_PLANE_EXHAUST_SUBMIX_CONTROL
			PropellorBreakOneShot(546884953U), // PROP_PLANE_ENGINE_BREAKDOWN
			WindNoise(3817852694U), // NULL_SOUND
			PeelingPitchCurve(0U),
			DivingFactor(1),
			MaxDivePitch(750),
			DiveAirSpeedThreshold(50),
			DivingRateApproachingGround(-30),
			PeelingAfterburnerPitchScalingFactor(2.0f),
			Rudder(3817852694U), // NULL_SOUND
			Aileron(3817852694U), // NULL_SOUND
			Elevator(3817852694U), // NULL_SOUND
			DoorStartOpen(3817852694U), // NULL_SOUND
			DoorStartClose(3817852694U), // NULL_SOUND
			VehicleCollisions(3581910264U), // VEHICLE_COLLISION_PLANE
			FireAudio(804661164U), // VEH_FIRE_SOUNDSET
			EngineMissFire(3817852694U), // NULL_SOUND
			IsRealLODRange(5000000U),
			SuspensionUp(2095958071U), // SUSPENSION_UP
			SuspensionDown(1257191667U), // SUSPENSION_DOWN
			MinSuspCompThresh(0.4f),
			MaxSuspCompThres(1.0f),
			DamageEngineSpeedVolumeCurve(352902987U), // PLANE_JET_DAMAGE_THROTTLE_VOL
			DamageEngineSpeedPitchCurve(2569650137U), // PLANE_JET_DAMAGE_PITCH
			DamageHealthVolumeCurve(3433411471U), // PLANE_JET_DAMAGE_VOL
			TurbineWindDown(45330392U), // JET_PLANE_COOLING_TURBINE
			JetDamageLoop(1581846411U), // JET_PLANE_DAMAGE_LOOP
			AfterburnerThrottlePitchCurve(406516903U), // PLANE_JET_AFTERBURNER_PITCH
			NPCEngineSmoothAmount(0.003f),
			FlybySound(3817852694U), // NULL_SOUND
			DivingSound(3817852694U), // NULL_SOUND
			DivingSoundPitchFactor(1),
			DivingSoundVolumeFactor(1),
			MaxDiveSoundPitch(750),
			RadioType(RADIO_TYPE_NORMAL),
			RadioGenre(RADIO_GENRE_UNSPECIFIED),
			AircraftWarningSeriousDamageThresh(0.003f),
			AircraftWarningCriticalDamageThresh(0.003f),
			AircraftWarningMaxSpeed(100.0f),
			SimpleSoundForLoading(3817852694U), // NULL_SOUND
			FlyAwaySound(3817852694U), // NULL_SOUND
			VehicleRainSound(3817852694U), // NULL_SOUND
			VehicleRainSoundInterior(3817852694U), // NULL_SOUND
			CabinToneLoop(0U),
			WaveHitSound(3817852694U), // NULL_SOUND
			WaveHitBigAirSound(3817852694U), // NULL_SOUND
			BigAirMinTime(1.2f),
			LeftWaterSound(0U),
			IdleHullSlapLoop(3817852694U), // NULL_SOUND
			IdleHullSlapSpeedToVol(0U),
			WaterTurbulenceSound(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 EngineLoop;
		rage::u32 ExhaustLoop;
		rage::u32 IdleLoop;
		rage::u32 DistanceLoop;
		rage::u32 PropellorLoop;
		rage::u32 BankingLoop;
		rage::u16 EngineConeFrontAngle;
		rage::u16 EngineConeRearAngle;
		rage::s16 EngineConeAtten;
		rage::u16 ExhaustConeFrontAngle;
		rage::u16 ExhaustConeRearAngle;
		rage::s16 ExhaustConeAtten;
		rage::u16 PropellorConeFrontAngle;
		rage::u16 PropellorConeRearAngle;
		rage::s16 PropellorConeAtten;
		rage::u32 EngineThrottleVolumeCurve;
		rage::u32 EngineThrottlePitchCurve;
		rage::u32 ExhaustThrottleVolumeCurve;
		rage::u32 ExhaustThrottlePitchCurve;
		rage::u32 PropellorThrottleVolumeCurve;
		rage::u32 PropellorThrottlePitchCurve;
		rage::u32 IdleThrottleVolumeCurve;
		rage::u32 IdleThrottlePitchCurve;
		rage::u32 DistanceThrottleVolumeCurve;
		rage::u32 DistanceThrottlePitchCurve;
		rage::u32 DistanceVolumeCurve;
		rage::u32 StallWarning;
		rage::u32 DoorOpen;
		rage::u32 DoorClose;
		rage::u32 DoorLimit;
		rage::u32 LandingGearDeploy;
		rage::u32 LandingGearRetract;
		rage::u32 Ignition;
		rage::u32 TyreSqueal;
		rage::u32 BankingThrottleVolumeCurve;
		rage::u32 BankingThrottlePitchCurve;
		rage::u32 BankingAngleVolCurve;
		rage::u32 AfterburnerLoop;
		rage::u8 BankingStyle;
		rage::u32 AfterburnerThrottleVolCurve;
		rage::u32 EngineSynthDef;
		rage::u32 EngineSynthPreset;
		rage::u32 ExhaustSynthDef;
		rage::u32 ExhaustSynthPreset;
		rage::u32 EngineSubmixVoice;
		rage::u32 ExhaustSubmixVoice;
		rage::u32 PropellorBreakOneShot;
		rage::u32 WindNoise;
		rage::u32 PeelingPitchCurve;
		rage::s16 DivingFactor;
		rage::s16 MaxDivePitch;
		rage::s16 DiveAirSpeedThreshold;
		rage::s16 DivingRateApproachingGround;
		float PeelingAfterburnerPitchScalingFactor;
		rage::u32 Rudder;
		rage::u32 Aileron;
		rage::u32 Elevator;
		rage::u32 DoorStartOpen;
		rage::u32 DoorStartClose;
		rage::u32 VehicleCollisions;
		rage::u32 FireAudio;
		rage::u32 EngineMissFire;
		rage::u32 IsRealLODRange;
		rage::u32 SuspensionUp;
		rage::u32 SuspensionDown;
		float MinSuspCompThresh;
		float MaxSuspCompThres;
		rage::u32 DamageEngineSpeedVolumeCurve;
		rage::u32 DamageEngineSpeedPitchCurve;
		rage::u32 DamageHealthVolumeCurve;
		rage::u32 TurbineWindDown;
		rage::u32 JetDamageLoop;
		rage::u32 AfterburnerThrottlePitchCurve;
		float NPCEngineSmoothAmount;
		rage::u32 FlybySound;
		rage::u32 DivingSound;
		rage::s16 DivingSoundPitchFactor;
		rage::s16 DivingSoundVolumeFactor;
		rage::s16 MaxDiveSoundPitch;
		rage::u8 RadioType;
		rage::u8 RadioGenre;
		float AircraftWarningSeriousDamageThresh;
		float AircraftWarningCriticalDamageThresh;
		float AircraftWarningMaxSpeed;
		rage::u32 SimpleSoundForLoading;
		rage::u32 FlyAwaySound;
		rage::u32 VehicleRainSound;
		rage::u32 VehicleRainSoundInterior;
		rage::u32 CabinToneLoop;
		rage::u32 WaveHitSound;
		rage::u32 WaveHitBigAirSound;
		float BigAirMinTime;
		rage::u32 LeftWaterSound;
		rage::u32 IdleHullSlapLoop;
		rage::u32 IdleHullSlapSpeedToVol;
		rage::u32 WaterTurbulenceSound;
	}; // struct PlaneAudioSettings
	
	// 
	// ConductorsDynamicMixingReference
	// 
	enum ConductorsDynamicMixingReferenceFlagIds
	{
		FLAG_ID_CONDUCTORSDYNAMICMIXINGREFERENCE_MAX = 0,
	}; // enum ConductorsDynamicMixingReferenceFlagIds
	
	struct ConductorsDynamicMixingReference
	{
		static const rage::u32 TYPE_ID = 58;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ConductorsDynamicMixingReference() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_REFERENCES = 32;
		rage::u8 numReferences;
		struct tReference
		{
			rage::u32 Type;
			rage::u32 Scene;
			rage::u32 MixGroup;
		} Reference[MAX_REFERENCES]; // struct tReference
		
	}; // struct ConductorsDynamicMixingReference
	
	// 
	// StemMix
	// 
	enum StemMixFlagIds
	{
		FLAG_ID_STEMMIX_MAX = 0,
	}; // enum StemMixFlagIds
	
	struct StemMix
	{
		static const rage::u32 TYPE_ID = 59;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		StemMix() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Stem1Volume(0),
			Stem2Volume(0),
			Stem3Volume(0),
			Stem4Volume(0),
			Stem5Volume(0),
			Stem6Volume(0),
			Stem7Volume(0),
			Stem8Volume(0)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::s16 Stem1Volume;
		rage::s16 Stem2Volume;
		rage::s16 Stem3Volume;
		rage::s16 Stem4Volume;
		rage::s16 Stem5Volume;
		rage::s16 Stem6Volume;
		rage::s16 Stem7Volume;
		rage::s16 Stem8Volume;
	}; // struct StemMix
	
	// 
	// MusicTimingConstraint
	// 
	enum MusicTimingConstraintFlagIds
	{
		FLAG_ID_MUSICTIMINGCONSTRAINT_MAX = 0,
	}; // enum MusicTimingConstraintFlagIds
	
	struct MusicTimingConstraint
	{
		static const rage::u32 TYPE_ID = 60;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		MusicTimingConstraint() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
	}; // struct MusicTimingConstraint
	
	// 
	// MusicAction
	// 
	enum MusicActionFlagIds
	{
		FLAG_ID_MUSICACTION_MAX = 0,
	}; // enum MusicActionFlagIds
	
	struct MusicAction
	{
		static const rage::u32 TYPE_ID = 61;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		MusicAction() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			Constrain(kConstrainStart),
			DelayTime(0.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u8 Constrain;
		
		static const rage::u32 MAX_TIMINGCONSTRAINTS = 2;
		rage::u32 numTimingConstraints;
		struct tTimingConstraints
		{
			rage::audMetadataRef Constraint;
		} TimingConstraints[MAX_TIMINGCONSTRAINTS]; // struct tTimingConstraints
		
		float DelayTime;
	}; // struct MusicAction
	
	// 
	// InteractiveMusicMood
	// 
	enum InteractiveMusicMoodFlagIds
	{
		FLAG_ID_INTERACTIVEMUSICMOOD_MAX = 0,
	}; // enum InteractiveMusicMoodFlagIds
	
	struct InteractiveMusicMood : AudBaseObject
	{
		static const rage::u32 TYPE_ID = 62;
		static const rage::u32 BASE_TYPE_ID = AudBaseObject::TYPE_ID;
		
		InteractiveMusicMood() :
			FadeInTime(400),
			FadeOutTime(3000),
			AmbMusicDuckingVol(0.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::s16 FadeInTime;
		rage::s16 FadeOutTime;
		float AmbMusicDuckingVol;
		
		static const rage::u8 MAX_STEMMIXES = 16;
		rage::u8 numStemMixes;
		struct tStemMixes
		{
			rage::audMetadataRef StemMix;
			rage::audMetadataRef OnTriggerAction;
			float MinDuration;
			float MaxDuration;
			float FadeTime;
			rage::u8 Constrain;
			
			static const rage::u8 MAX_TIMINGCONSTRAINTS = 2;
			rage::u8 numTimingConstraints;
			struct tTimingConstraints
			{
				rage::audMetadataRef Constraint;
			} TimingConstraints[MAX_TIMINGCONSTRAINTS]; // struct tTimingConstraints
			
		} StemMixes[MAX_STEMMIXES]; // struct tStemMixes
		
	}; // struct InteractiveMusicMood : AudBaseObject
	
	// 
	// StartTrackAction
	// 
	enum StartTrackActionFlagIds
	{
		FLAG_ID_STARTTRACKACTION_OVERRIDERADIO = 0,
		FLAG_ID_STARTTRACKACTION_RANDOMSTARTOFFSET,
		FLAG_ID_STARTTRACKACTION_MUTERADIOOFFSOUND,
		FLAG_ID_STARTTRACKACTION_MAX,
	}; // enum StartTrackActionFlagIds
	
	struct StartTrackAction : MusicAction
	{
		static const rage::u32 TYPE_ID = 63;
		static const rage::u32 BASE_TYPE_ID = MusicAction::TYPE_ID;
		
		StartTrackAction() :
			Song(0U),
			VolumeOffset(0.0f),
			FadeInTime(-1),
			FadeOutTime(-1),
			StartOffsetScalar(0.0f),
			LastSong(1000U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 Song;
		rage::audMetadataRef Mood;
		float VolumeOffset;
		rage::s32 FadeInTime;
		rage::s32 FadeOutTime;
		float StartOffsetScalar;
		rage::u32 LastSong;
		
		static const rage::u8 MAX_ALTSONGS = 64;
		rage::u8 numAltSongs;
		struct tAltSongs
		{
			rage::u32 Song;
			rage::u8 ValidArea;
		} AltSongs[MAX_ALTSONGS]; // struct tAltSongs
		
	}; // struct StartTrackAction : MusicAction
	
	// 
	// StopTrackAction
	// 
	enum StopTrackActionFlagIds
	{
		FLAG_ID_STOPTRACKACTION_UNFREEZERADIO = 0,
		FLAG_ID_STOPTRACKACTION_MAX,
	}; // enum StopTrackActionFlagIds
	
	struct StopTrackAction : MusicAction
	{
		static const rage::u32 TYPE_ID = 64;
		static const rage::u32 BASE_TYPE_ID = MusicAction::TYPE_ID;
		
		StopTrackAction() :
			FadeTime(-1)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::s32 FadeTime;
	}; // struct StopTrackAction : MusicAction
	
	// 
	// SetMoodAction
	// 
	enum SetMoodActionFlagIds
	{
		FLAG_ID_SETMOODACTION_MAX = 0,
	}; // enum SetMoodActionFlagIds
	
	struct SetMoodAction : MusicAction
	{
		static const rage::u32 TYPE_ID = 65;
		static const rage::u32 BASE_TYPE_ID = MusicAction::TYPE_ID;
		
		SetMoodAction() :
			VolumeOffset(0.0f),
			FadeInTime(-1),
			FadeOutTime(-1)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::audMetadataRef Mood;
		float VolumeOffset;
		rage::s32 FadeInTime;
		rage::s32 FadeOutTime;
	}; // struct SetMoodAction : MusicAction
	
	// 
	// MusicEvent
	// 
	enum MusicEventFlagIds
	{
		FLAG_ID_MUSICEVENT_MAX = 0,
	}; // enum MusicEventFlagIds
	
	struct MusicEvent
	{
		static const rage::u32 TYPE_ID = 66;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		MusicEvent() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_ACTIONS = 8;
		rage::u8 numActions;
		struct tActions
		{
			rage::audMetadataRef Action;
		} Actions[MAX_ACTIONS]; // struct tActions
		
	}; // struct MusicEvent
	
	// 
	// StartOneShotAction
	// 
	enum StartOneShotActionFlagIds
	{
		FLAG_ID_STARTONESHOTACTION_UNFREEZERADIO = 0,
		FLAG_ID_STARTONESHOTACTION_OVERRIDERADIO,
		FLAG_ID_STARTONESHOTACTION_MAX,
	}; // enum StartOneShotActionFlagIds
	
	struct StartOneShotAction : MusicAction
	{
		static const rage::u32 TYPE_ID = 67;
		static const rage::u32 BASE_TYPE_ID = MusicAction::TYPE_ID;
		
		StartOneShotAction() :
			Sound(0U),
			Predelay(-1),
			FadeInTime(-1),
			FadeOutTime(-1),
			SyncMarker(kNoSyncMarker)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 Sound;
		rage::s32 Predelay;
		rage::s32 FadeInTime;
		rage::s32 FadeOutTime;
		rage::u8 SyncMarker;
	}; // struct StartOneShotAction : MusicAction
	
	// 
	// StopOneShotAction
	// 
	enum StopOneShotActionFlagIds
	{
		FLAG_ID_STOPONESHOTACTION_MAX = 0,
	}; // enum StopOneShotActionFlagIds
	
	struct StopOneShotAction : MusicAction
	{
		static const rage::u32 TYPE_ID = 68;
		static const rage::u32 BASE_TYPE_ID = MusicAction::TYPE_ID;
		
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
	}; // struct StopOneShotAction : MusicAction
	
	// 
	// BeatConstraint
	// 
	enum BeatConstraintFlagIds
	{
		FLAG_ID_BEATCONSTRAINT_MAX = 0,
	}; // enum BeatConstraintFlagIds
	
	struct BeatConstraint : MusicTimingConstraint
	{
		static const rage::u32 TYPE_ID = 69;
		static const rage::u32 BASE_TYPE_ID = MusicTimingConstraint::TYPE_ID;
		
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		
		struct tValidSixteenths
		{
			tValidSixteenths()
			 : Value(0U)
			{
			}
			union
			{
				rage::u16 Value;
				struct
				{
					#if __BE
						bool _16:1;
						bool _15:1;
						bool _14:1;
						bool _13:1;
						bool _12:1;
						bool _11:1;
						bool _10:1;
						bool _9:1;
						bool _8:1;
						bool _7:1;
						bool _6:1;
						bool _5:1;
						bool _4:1;
						bool _3:1;
						bool _2:1;
						bool _1:1;
					#else // __BE
						bool _1:1;
						bool _2:1;
						bool _3:1;
						bool _4:1;
						bool _5:1;
						bool _6:1;
						bool _7:1;
						bool _8:1;
						bool _9:1;
						bool _10:1;
						bool _11:1;
						bool _12:1;
						bool _13:1;
						bool _14:1;
						bool _15:1;
						bool _16:1;
					#endif // __BE
				} BitFields;
			};
		} ValidSixteenths; // struct tValidSixteenths
		
	}; // struct BeatConstraint : MusicTimingConstraint
	
	// 
	// BarConstraint
	// 
	enum BarConstraintFlagIds
	{
		FLAG_ID_BARCONSTRAINT_MAX = 0,
	}; // enum BarConstraintFlagIds
	
	struct BarConstraint : MusicTimingConstraint
	{
		static const rage::u32 TYPE_ID = 70;
		static const rage::u32 BASE_TYPE_ID = MusicTimingConstraint::TYPE_ID;
		
		BarConstraint() :
			PatternLength(8),
			ValidBar(1)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::s32 PatternLength;
		rage::s32 ValidBar;
	}; // struct BarConstraint : MusicTimingConstraint
	
	// 
	// DirectionalAmbience
	// 
	enum DirectionalAmbienceFlagIds
	{
		FLAG_ID_DIRECTIONALAMBIENCE_MONOPANNEDSOURCE = 0,
		FLAG_ID_DIRECTIONALAMBIENCE_STOPWHENRAINING,
		FLAG_ID_DIRECTIONALAMBIENCE_MAX,
	}; // enum DirectionalAmbienceFlagIds
	
	struct DirectionalAmbience
	{
		static const rage::u32 TYPE_ID = 71;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		DirectionalAmbience() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			SoundNorth(0U),
			SoundEast(0U),
			SoundSouth(0U),
			SoundWest(0U),
			VolumeSmoothing(1.0f),
			TimeToVolume(0U),
			OcclusionToVol(0U),
			HeightToCutOff(0U),
			OcclusionToCutOff(0U),
			BuiltUpFactorToVol(0U),
			BuildingDensityToVol(0U),
			TreeDensityToVol(0U),
			WaterFactorToVol(0U),
			InstanceVolumeScale(1.0f),
			HeightAboveBlanketToVol(0U),
			HighwayFactorToVol(0U),
			VehicleCountToVol(0U),
			MaxDistanceOutToSea(-1.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 SoundNorth;
		rage::u32 SoundEast;
		rage::u32 SoundSouth;
		rage::u32 SoundWest;
		float VolumeSmoothing;
		rage::u32 TimeToVolume;
		rage::u32 OcclusionToVol;
		rage::u32 HeightToCutOff;
		rage::u32 OcclusionToCutOff;
		rage::u32 BuiltUpFactorToVol;
		rage::u32 BuildingDensityToVol;
		rage::u32 TreeDensityToVol;
		rage::u32 WaterFactorToVol;
		float InstanceVolumeScale;
		rage::u32 HeightAboveBlanketToVol;
		rage::u32 HighwayFactorToVol;
		rage::u32 VehicleCountToVol;
		float MaxDistanceOutToSea;
	}; // struct DirectionalAmbience
	
	// 
	// GunfightConductorIntensitySettings
	// 
	enum GunfightConductorIntensitySettingsFlagIds
	{
		FLAG_ID_GUNFIGHTCONDUCTORINTENSITYSETTINGS_MAX = 0,
	}; // enum GunfightConductorIntensitySettingsFlagIds
	
	struct GunfightConductorIntensitySettings
	{
		static const rage::u32 TYPE_ID = 72;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		GunfightConductorIntensitySettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			ObjectsMediumIntensity(0U),
			ObjectsHighIntensity(0U),
			VehiclesMediumIntensity(0U),
			VehiclesHighIntensity(0U),
			GroundMediumIntensity(0U),
			GroundHighIntensity(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		struct tCommon
		{
			rage::u32 MaxTimeAfterLastShot;
		} Common; // struct tCommon
		
		
		struct tGoingIntoCover
		{
			float TimeToFakeBulletImpacts;
			float MinTimeToReTriggerFakeBI;
			float MaxTimeToReTriggerFakeBI;
		} GoingIntoCover; // struct tGoingIntoCover
		
		
		struct tRunningAway
		{
			float MinTimeToFakeBulletImpacts;
			float MaxTimeToFakeBulletImpacts;
			float VfxProbability;
		} RunningAway; // struct tRunningAway
		
		
		struct tOpenSpace
		{
			float MinTimeToFakeBulletImpacts;
			float MaxTimeToFakeBulletImpacts;
		} OpenSpace; // struct tOpenSpace
		
		rage::u32 ObjectsMediumIntensity;
		rage::u32 ObjectsHighIntensity;
		rage::u32 VehiclesMediumIntensity;
		rage::u32 VehiclesHighIntensity;
		rage::u32 GroundMediumIntensity;
		rage::u32 GroundHighIntensity;
	}; // struct GunfightConductorIntensitySettings
	
	// 
	// ScannerVoiceParams
	// 
	enum ScannerVoiceParamsFlagIds
	{
		FLAG_ID_SCANNERVOICEPARAMS_MAX = 0,
	}; // enum ScannerVoiceParamsFlagIds
	
	struct ScannerVoiceParams
	{
		static const rage::u32 TYPE_ID = 75;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ScannerVoiceParams() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		struct tNoPrefix
		{
			rage::u32 BlackSound;
			rage::u32 BlueSound;
			rage::u32 BrownSound;
			rage::u32 BeigeSound;
			rage::u32 GraphiteSound;
			rage::u32 GreenSound;
			rage::u32 GreySound;
			rage::u32 OrangeSound;
			rage::u32 PinkSound;
			rage::u32 RedSound;
			rage::u32 SilverSound;
			rage::u32 WhiteSound;
			rage::u32 YellowSound;
		} NoPrefix; // struct tNoPrefix
		
		
		struct tBright
		{
			rage::u32 BlackSound;
			rage::u32 BlueSound;
			rage::u32 BrownSound;
			rage::u32 BeigeSound;
			rage::u32 GraphiteSound;
			rage::u32 GreenSound;
			rage::u32 GreySound;
			rage::u32 OrangeSound;
			rage::u32 PinkSound;
			rage::u32 RedSound;
			rage::u32 SilverSound;
			rage::u32 WhiteSound;
			rage::u32 YellowSound;
		} Bright; // struct tBright
		
		
		struct tLight
		{
			rage::u32 BlackSound;
			rage::u32 BlueSound;
			rage::u32 BrownSound;
			rage::u32 BeigeSound;
			rage::u32 GraphiteSound;
			rage::u32 GreenSound;
			rage::u32 GreySound;
			rage::u32 OrangeSound;
			rage::u32 PinkSound;
			rage::u32 RedSound;
			rage::u32 SilverSound;
			rage::u32 WhiteSound;
			rage::u32 YellowSound;
		} Light; // struct tLight
		
		
		struct tDark
		{
			rage::u32 BlackSound;
			rage::u32 BlueSound;
			rage::u32 BrownSound;
			rage::u32 BeigeSound;
			rage::u32 GraphiteSound;
			rage::u32 GreenSound;
			rage::u32 GreySound;
			rage::u32 OrangeSound;
			rage::u32 PinkSound;
			rage::u32 RedSound;
			rage::u32 SilverSound;
			rage::u32 WhiteSound;
			rage::u32 YellowSound;
		} Dark; // struct tDark
		
		
		struct tExtraPrefix
		{
			rage::u32 Battered;
			rage::u32 BeatUp;
			rage::u32 Chopped;
			rage::u32 Custom;
			rage::u32 Customized;
			rage::u32 Damaged;
			rage::u32 Dented;
			rage::u32 Dirty;
			rage::u32 Distressed;
			rage::u32 Mint;
			rage::u32 Modified;
			rage::u32 RunDown1;
			rage::u32 RunDown2;
			rage::u32 Rusty;
		} ExtraPrefix; // struct tExtraPrefix
		
	}; // struct ScannerVoiceParams
	
	// 
	// ScannerVehicleParams
	// 
	enum ScannerVehicleParamsFlagIds
	{
		FLAG_ID_SCANNERVEHICLEPARAMS_BLOCKCOLORREPORTING = 0,
		FLAG_ID_SCANNERVEHICLEPARAMS_MAX,
	}; // enum ScannerVehicleParamsFlagIds
	
	struct ScannerVehicleParams
	{
		static const rage::u32 TYPE_ID = 76;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ScannerVehicleParams() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		
		static const rage::u8 MAX_VOICES = 8;
		rage::u8 numVoices;
		struct tVoice
		{
			rage::u32 ManufacturerSound;
			rage::u32 ModelSound;
			rage::u32 CategorySound;
			rage::u32 ScannerColorOverride;
		} Voice[MAX_VOICES]; // struct tVoice
		
	}; // struct ScannerVehicleParams
	
	// 
	// AudioRoadInfo
	// 
	enum AudioRoadInfoFlagIds
	{
		FLAG_ID_AUDIOROADINFO_MAX = 0,
	}; // enum AudioRoadInfoFlagIds
	
	struct AudioRoadInfo
	{
		static const rage::u32 TYPE_ID = 77;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AudioRoadInfo() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			RoadName(0U),
			TyreBumpDistance(12.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 RoadName;
		float TyreBumpDistance;
	}; // struct AudioRoadInfo
	
	// 
	// MicSettingsReference
	// 
	enum MicSettingsReferenceFlagIds
	{
		FLAG_ID_MICSETTINGSREFERENCE_MAX = 0,
	}; // enum MicSettingsReferenceFlagIds
	
	struct MicSettingsReference
	{
		static const rage::u32 TYPE_ID = 78;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		MicSettingsReference() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_REFERENCES = 64;
		rage::u8 numReferences;
		struct tReference
		{
			rage::u32 CameraType;
			rage::audMetadataRef MicSettings;
		} Reference[MAX_REFERENCES]; // struct tReference
		
	}; // struct MicSettingsReference
	
	// 
	// MicrophoneSettings
	// 
	enum MicrophoneSettingsFlagIds
	{
		FLAG_ID_MICROPHONESETTINGS_PLAYERFRONTEND = 0,
		FLAG_ID_MICROPHONESETTINGS_DISTANTPLAYERWEAPONS,
		FLAG_ID_MICROPHONESETTINGS_DISABLEBULLETSBY,
		FLAG_ID_MICROPHONESETTINGS_MAX,
	}; // enum MicrophoneSettingsFlagIds
	
	struct MicrophoneSettings
	{
		static const rage::u32 TYPE_ID = 79;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		MicrophoneSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			MicType(MIC_FOLLOW_PED)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u8 MicType;
		
		static const rage::u8 MAX_LISTENERPARAMETERS = 2;
		rage::u8 numListenerParameters;
		struct tListenerParameters
		{
			float ListenerContribution;
			float RearAttenuationFrontConeAngle;
			float RearAttenuationRearConeAngle;
			float CloseRearAttenuation;
			float FarRearAttenuation;
			float RollOff;
			rage::u8 RearAttenuationType;
			float MicLength;
			float MicToPlayerLocalEnvironmentRatio;
		} ListenerParameters[MAX_LISTENERPARAMETERS]; // struct tListenerParameters
		
	}; // struct MicrophoneSettings
	
	// 
	// CarRecordingAudioSettings
	// 
	enum CarRecordingAudioSettingsFlagIds
	{
		FLAG_ID_CARRECORDINGAUDIOSETTINGS_MAX = 0,
	}; // enum CarRecordingAudioSettingsFlagIds
	
	struct CarRecordingAudioSettings
	{
		static const rage::u32 TYPE_ID = 80;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		CarRecordingAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Mixgroup(0U),
			VehicleModelId(65535U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 Mixgroup;
		rage::u32 VehicleModelId;
		
		static const rage::u8 MAX_EVENTS = 128;
		rage::u8 numEvents;
		struct tEvent
		{
			float Time;
			rage::u32 Sound;
			rage::u32 OneShotScene;
		} Event[MAX_EVENTS]; // struct tEvent
		
		
		static const rage::u8 MAX_PERSISTENTMIXERSCENES = 3;
		rage::u8 numPersistentMixerScenes;
		struct tPersistentMixerScenes
		{
			rage::u32 Scene;
			float StartTime;
			float EndTime;
		} PersistentMixerScenes[MAX_PERSISTENTMIXERSCENES]; // struct tPersistentMixerScenes
		
	}; // struct CarRecordingAudioSettings
	
	// 
	// CarRecordingList
	// 
	enum CarRecordingListFlagIds
	{
		FLAG_ID_CARRECORDINGLIST_MAX = 0,
	}; // enum CarRecordingListFlagIds
	
	struct CarRecordingList
	{
		static const rage::u32 TYPE_ID = 81;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		CarRecordingList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_CARRECORDINGS = 1000;
		rage::u16 numCarRecordings;
		struct tCarRecording
		{
			rage::u32 Name;
			rage::audMetadataRef CarRecordingAudioSettings;
		} CarRecording[MAX_CARRECORDINGS]; // struct tCarRecording
		
	}; // struct CarRecordingList
	
	// 
	// AnimalFootstepSettings
	// 
	enum AnimalFootstepSettingsFlagIds
	{
		FLAG_ID_ANIMALFOOTSTEPSETTINGS_MAX = 0,
	}; // enum AnimalFootstepSettingsFlagIds
	
	struct AnimalFootstepSettings
	{
		static const rage::u32 TYPE_ID = 82;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AnimalFootstepSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			WalkAndTrot(3817852694U), // NULL_SOUND
			Gallop1(3817852694U), // NULL_SOUND
			Gallop2(3817852694U), // NULL_SOUND
			Scuff(3817852694U), // NULL_SOUND
			Jump(3817852694U), // NULL_SOUND
			Land(3817852694U), // NULL_SOUND
			LandHard(3817852694U), // NULL_SOUND
			Clumsy(3817852694U), // NULL_SOUND
			SlideLoop(3817852694U), // NULL_SOUND
			AudioEventLoudness(FOOTSTEP_LOUDNESS_MEDIUM)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 WalkAndTrot;
		rage::u32 Gallop1;
		rage::u32 Gallop2;
		rage::u32 Scuff;
		rage::u32 Jump;
		rage::u32 Land;
		rage::u32 LandHard;
		rage::u32 Clumsy;
		rage::u32 SlideLoop;
		rage::u8 AudioEventLoudness;
	}; // struct AnimalFootstepSettings
	
	// 
	// AnimalFootstepReference
	// 
	enum AnimalFootstepReferenceFlagIds
	{
		FLAG_ID_ANIMALFOOTSTEPREFERENCE_MAX = 0,
	}; // enum AnimalFootstepReferenceFlagIds
	
	struct AnimalFootstepReference
	{
		static const rage::u32 TYPE_ID = 83;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AnimalFootstepReference() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_ANIMALREFERENCES = 1000;
		rage::u16 numAnimalReferences;
		struct tAnimalReference
		{
			rage::u32 Name;
			rage::audMetadataRef AnimalFootstepSettings;
		} AnimalReference[MAX_ANIMALREFERENCES]; // struct tAnimalReference
		
	}; // struct AnimalFootstepReference
	
	// 
	// ShoeList
	// 
	enum ShoeListFlagIds
	{
		FLAG_ID_SHOELIST_MAX = 0,
	}; // enum ShoeListFlagIds
	
	struct ShoeList
	{
		static const rage::u32 TYPE_ID = 84;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ShoeList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_SHOES = 1000;
		rage::u16 numShoes;
		struct tShoe
		{
			rage::u32 ShoeName;
			rage::audMetadataRef ShoeAudioSettings;
		} Shoe[MAX_SHOES]; // struct tShoe
		
	}; // struct ShoeList
	
	// 
	// ClothAudioSettings
	// 
	enum ClothAudioSettingsFlagIds
	{
		FLAG_ID_CLOTHAUDIOSETTINGS_HAVEPOCKETS = 0,
		FLAG_ID_CLOTHAUDIOSETTINGS_FLAPSINWIND,
		FLAG_ID_CLOTHAUDIOSETTINGS_MAX,
	}; // enum ClothAudioSettingsFlagIds
	
	struct ClothAudioSettings
	{
		static const rage::u32 TYPE_ID = 85;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ClothAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			ImpactSound(2143502167U), // BODY_FALL_BODY
			WalkSound(0U),
			RunSound(0U),
			SprintSound(0U),
			IntoCoverSound(3817852694U), // NULL_SOUND
			OutOfCoverSound(3817852694U), // NULL_SOUND
			WindSound(3817852694U), // NULL_SOUND
			Intensity(0.0f),
			PlayerVersion(0U),
			BulletImpacts(798085190U), // GENERIC_PED_BULLETIMPACTS
			PedRollSound(3817852694U), // NULL_SOUND
			ScrapeMaterialSettings(0U),
			JumpLandSound(3817852694U), // NULL_SOUND
			MeleeSwingSound(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 ImpactSound;
		rage::u32 WalkSound;
		rage::u32 RunSound;
		rage::u32 SprintSound;
		rage::u32 IntoCoverSound;
		rage::u32 OutOfCoverSound;
		rage::u32 WindSound;
		float Intensity;
		rage::u32 PlayerVersion;
		rage::u32 BulletImpacts;
		rage::u32 PedRollSound;
		rage::u32 ScrapeMaterialSettings;
		rage::u32 JumpLandSound;
		rage::u32 MeleeSwingSound;
	}; // struct ClothAudioSettings
	
	// 
	// ClothList
	// 
	enum ClothListFlagIds
	{
		FLAG_ID_CLOTHLIST_MAX = 0,
	}; // enum ClothListFlagIds
	
	struct ClothList
	{
		static const rage::u32 TYPE_ID = 86;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ClothList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_CLOTHS = 1000;
		rage::u16 numCloths;
		struct tCloth
		{
			rage::u32 ClothName;
			rage::audMetadataRef ClothAudioSettings;
		} Cloth[MAX_CLOTHS]; // struct tCloth
		
	}; // struct ClothList
	
	// 
	// ExplosionAudioSettings
	// 
	enum ExplosionAudioSettingsFlagIds
	{
		FLAG_ID_EXPLOSIONAUDIOSETTINGS_DISABLEDEBRIS = 0,
		FLAG_ID_EXPLOSIONAUDIOSETTINGS_DISABLESHOCKWAVE,
		FLAG_ID_EXPLOSIONAUDIOSETTINGS_ISCAREXPLOSION,
		FLAG_ID_EXPLOSIONAUDIOSETTINGS_MAX,
	}; // enum ExplosionAudioSettingsFlagIds
	
	struct ExplosionAudioSettings
	{
		static const rage::u32 TYPE_ID = 87;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ExplosionAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			ExplosionSound(774271614U), // MAIN_EXPLOSION
			DebrisSound(2945760942U), // DEBRIS_RANDOM_ONE_SHOTS
			DeafeningVolume(16.0f),
			DebrisTimeScale(1.0f),
			DebrisVolume(0.0f),
			ShockwaveIntensity(1.0f),
			ShockwaveDelay(0.55f),
			SlowMoExplosionSound(3817852694U), // NULL_SOUND
			SlowMoExplosionPreSuckSound(3817852694U), // NULL_SOUND
			SlowMoExplosionPreSuckSoundTime(0),
			SlowMoExplosionPreSuckMixerScene(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 ExplosionSound;
		rage::u32 DebrisSound;
		float DeafeningVolume;
		float DebrisTimeScale;
		float DebrisVolume;
		float ShockwaveIntensity;
		float ShockwaveDelay;
		rage::u32 SlowMoExplosionSound;
		rage::u32 SlowMoExplosionPreSuckSound;
		rage::s32 SlowMoExplosionPreSuckSoundTime;
		rage::u32 SlowMoExplosionPreSuckMixerScene;
	}; // struct ExplosionAudioSettings
	
	// 
	// GranularEngineAudioSettings
	// 
	enum GranularEngineAudioSettingsFlagIds
	{
		FLAG_ID_GRANULARENGINEAUDIOSETTINGS_GEARCHANGEWOBBLESENABLED = 0,
		FLAG_ID_GRANULARENGINEAUDIOSETTINGS_PUBLISH,
		FLAG_ID_GRANULARENGINEAUDIOSETTINGS_HASREVLIMITER,
		FLAG_ID_GRANULARENGINEAUDIOSETTINGS_VISUALFXONLIMITER,
		FLAG_ID_GRANULARENGINEAUDIOSETTINGS_EXHAUSTPROXIMITYMIXERSCENEENABLED,
		FLAG_ID_GRANULARENGINEAUDIOSETTINGS_MAX,
	}; // enum GranularEngineAudioSettingsFlagIds
	
	struct GranularEngineAudioSettings
	{
		static const rage::u32 TYPE_ID = 88;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		GranularEngineAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			MasterVolume(0),
			EngineAccel(3817852694U), // NULL_SOUND
			ExhaustAccel(3817852694U), // NULL_SOUND
			EngineVolume_PreSubmix(0),
			ExhaustVolume_PreSubmix(0),
			AccelVolume_PreSubmix(0),
			DecelVolume_PreSubmix(0),
			IdleVolume_PreSubmix(0),
			EngineRevsVolume_PreSubmix(0),
			ExhaustRevsVolume_PreSubmix(0),
			EngineThrottleVolume_PreSubmix(0),
			ExhaustThrottleVolume_PreSubmix(0),
			EngineVolume_PostSubmix(0),
			ExhaustVolume_PostSubmix(0),
			EngineMaxConeAttenuation(-300),
			ExhaustMaxConeAttenuation(-300),
			EngineRevsVolume_PostSubmix(0),
			ExhaustRevsVolume_PostSubmix(0),
			EngineThrottleVolume_PostSubmix(0),
			ExhaustThrottleVolume_PostSubmix(0),
			GearChangeWobbleLength(30U),
			GearChangeWobbleLengthVariance(0.2f),
			GearChangeWobbleSpeed(0.26f),
			GearChangeWobbleSpeedVariance(0.2f),
			GearChangeWobblePitch(0.15f),
			GearChangeWobblePitchVariance(0.2f),
			GearChangeWobbleVolume(0.0f),
			GearChangeWobbleVolumeVariance(0.2f),
			EngineClutchAttenuation_PostSubmix(0),
			ExhaustClutchAttenuation_PostSubmix(0),
			EngineSynthDef(0U),
			EngineSynthPreset(0U),
			ExhaustSynthDef(0U),
			ExhaustSynthPreset(0U),
			NPCEngineAccel(3817852694U), // NULL_SOUND
			NPCExhaustAccel(3817852694U), // NULL_SOUND
			RevLimiterPopSound(3817852694U), // NULL_SOUND
			MinRPMOverride(0U),
			MaxRPMOverride(0U),
			EngineSubmixVoice(1225003942U), // DEFAULT_CAR_ENGINE_SUBMIX_CONTROL
			ExhaustSubmixVoice(1479769906U), // DEFAULT_CAR_EXHAUST_SUBMIX_CONTROL
			ExhaustProximityVolume_PostSubmix(600),
			RevLimiterGrainsToPlay(5U),
			RevLimiterGrainsToSkip(2U),
			SynchronisedSynth(3817852694U), // NULL_SOUND
			UpgradedEngineVolumeBoost_PostSubmix(0),
			UpgradedEngineSynthDef(0U),
			UpgradedEngineSynthPreset(0U),
			UpgradedExhaustVolumeBoost_PostSubmix(0),
			UpgradedExhaustSynthDef(0U),
			UpgradedExhaustSynthPreset(0U),
			DamageSynthHashList(0U),
			UpgradedRevLimiterPop(1755537032U), // UPGRADE_POPS_MULTI
			EngineIdleVolume_PostSubmix(0),
			ExhaustIdleVolume_PostSubmix(0),
			StartupRevsVolumeBoostEngine_PostSubmix(500),
			StartupRevsVolumeBoostExhaust_PostSubmix(500),
			RevLimiterApplyType(REV_LIMITER_EXHAUST_ONLY),
			RevLimiterVolumeCut(0.2f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::s32 MasterVolume;
		rage::u32 EngineAccel;
		rage::u32 ExhaustAccel;
		rage::s32 EngineVolume_PreSubmix;
		rage::s32 ExhaustVolume_PreSubmix;
		rage::s32 AccelVolume_PreSubmix;
		rage::s32 DecelVolume_PreSubmix;
		rage::s32 IdleVolume_PreSubmix;
		rage::s32 EngineRevsVolume_PreSubmix;
		rage::s32 ExhaustRevsVolume_PreSubmix;
		rage::s32 EngineThrottleVolume_PreSubmix;
		rage::s32 ExhaustThrottleVolume_PreSubmix;
		rage::s32 EngineVolume_PostSubmix;
		rage::s32 ExhaustVolume_PostSubmix;
		rage::s32 EngineMaxConeAttenuation;
		rage::s32 ExhaustMaxConeAttenuation;
		rage::s32 EngineRevsVolume_PostSubmix;
		rage::s32 ExhaustRevsVolume_PostSubmix;
		rage::s32 EngineThrottleVolume_PostSubmix;
		rage::s32 ExhaustThrottleVolume_PostSubmix;
		rage::u32 GearChangeWobbleLength;
		float GearChangeWobbleLengthVariance;
		float GearChangeWobbleSpeed;
		float GearChangeWobbleSpeedVariance;
		float GearChangeWobblePitch;
		float GearChangeWobblePitchVariance;
		float GearChangeWobbleVolume;
		float GearChangeWobbleVolumeVariance;
		rage::s32 EngineClutchAttenuation_PostSubmix;
		rage::s32 ExhaustClutchAttenuation_PostSubmix;
		rage::u32 EngineSynthDef;
		rage::u32 EngineSynthPreset;
		rage::u32 ExhaustSynthDef;
		rage::u32 ExhaustSynthPreset;
		rage::u32 NPCEngineAccel;
		rage::u32 NPCExhaustAccel;
		rage::u32 RevLimiterPopSound;
		rage::u32 MinRPMOverride;
		rage::u32 MaxRPMOverride;
		rage::u32 EngineSubmixVoice;
		rage::u32 ExhaustSubmixVoice;
		rage::s32 ExhaustProximityVolume_PostSubmix;
		rage::u32 RevLimiterGrainsToPlay;
		rage::u32 RevLimiterGrainsToSkip;
		rage::u32 SynchronisedSynth;
		rage::s32 UpgradedEngineVolumeBoost_PostSubmix;
		rage::u32 UpgradedEngineSynthDef;
		rage::u32 UpgradedEngineSynthPreset;
		rage::s32 UpgradedExhaustVolumeBoost_PostSubmix;
		rage::u32 UpgradedExhaustSynthDef;
		rage::u32 UpgradedExhaustSynthPreset;
		rage::u32 DamageSynthHashList;
		rage::u32 UpgradedRevLimiterPop;
		rage::s32 EngineIdleVolume_PostSubmix;
		rage::s32 ExhaustIdleVolume_PostSubmix;
		rage::s32 StartupRevsVolumeBoostEngine_PostSubmix;
		rage::s32 StartupRevsVolumeBoostExhaust_PostSubmix;
		rage::u8 RevLimiterApplyType;
		float RevLimiterVolumeCut;
	}; // struct GranularEngineAudioSettings
	
	// 
	// ShoreLineAudioSettings
	// 
	enum ShoreLineAudioSettingsFlagIds
	{
		FLAG_ID_SHORELINEAUDIOSETTINGS_ISININTERIOR = 0,
		FLAG_ID_SHORELINEAUDIOSETTINGS_MAX,
	}; // enum ShoreLineAudioSettingsFlagIds
	
	struct ShoreLineAudioSettings
	{
		static const rage::u32 TYPE_ID = 89;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ShoreLineAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		
		struct tActivationBox
		{
			
			struct tCenter
			{
				float X;
				float Y;
			} Center; // struct tCenter
			
			
			struct tSize
			{
				float Width;
				float Height;
			} Size; // struct tSize
			
			float RotationAngle;
		} ActivationBox; // struct tActivationBox
		
	}; // struct ShoreLineAudioSettings
	
	// 
	// ShoreLinePoolAudioSettings
	// 
	enum ShoreLinePoolAudioSettingsFlagIds
	{
		FLAG_ID_SHORELINEPOOLAUDIOSETTINGS_ISININTERIOR = 0,
		FLAG_ID_SHORELINEPOOLAUDIOSETTINGS_TREATASLAKE,
		FLAG_ID_SHORELINEPOOLAUDIOSETTINGS_MAX,
	}; // enum ShoreLinePoolAudioSettingsFlagIds
	
	struct ShoreLinePoolAudioSettings : ShoreLineAudioSettings
	{
		static const rage::u32 TYPE_ID = 90;
		static const rage::u32 BASE_TYPE_ID = ShoreLineAudioSettings::TYPE_ID;
		
		ShoreLinePoolAudioSettings() :
			WaterLappingMinDelay(-500),
			WaterLappingMaxDelay(500),
			WaterSplashMinDelay(5000),
			WaterSplashMaxDelay(14000),
			FirstQuadIndex(-1),
			SecondQuadIndex(-1),
			ThirdQuadIndex(-1),
			FourthQuadIndex(-1),
			SmallestDistanceToPoint(0.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::s32 WaterLappingMinDelay;
		rage::s32 WaterLappingMaxDelay;
		rage::s32 WaterSplashMinDelay;
		rage::s32 WaterSplashMaxDelay;
		rage::s32 FirstQuadIndex;
		rage::s32 SecondQuadIndex;
		rage::s32 ThirdQuadIndex;
		rage::s32 FourthQuadIndex;
		float SmallestDistanceToPoint;
		
		static const rage::u16 MAX_SHORELINEPOINTS = 1000;
		rage::u16 numShoreLinePoints;
		struct tShoreLinePoints
		{
			float X;
			float Y;
		} ShoreLinePoints[MAX_SHORELINEPOINTS]; // struct tShoreLinePoints
		
	}; // struct ShoreLinePoolAudioSettings : ShoreLineAudioSettings
	
	// 
	// ShoreLineLakeAudioSettings
	// 
	enum ShoreLineLakeAudioSettingsFlagIds
	{
		FLAG_ID_SHORELINELAKEAUDIOSETTINGS_ISININTERIOR = 0,
		FLAG_ID_SHORELINELAKEAUDIOSETTINGS_MAX,
	}; // enum ShoreLineLakeAudioSettingsFlagIds
	
	struct ShoreLineLakeAudioSettings : ShoreLineAudioSettings
	{
		static const rage::u32 TYPE_ID = 91;
		static const rage::u32 BASE_TYPE_ID = ShoreLineAudioSettings::TYPE_ID;
		
		ShoreLineLakeAudioSettings() :
			NextShoreline(0U),
			LakeSize(AUD_LAKE_SMALL)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 NextShoreline;
		rage::u8 LakeSize;
		
		static const rage::u8 MAX_SHORELINEPOINTS = 32;
		rage::u8 numShoreLinePoints;
		struct tShoreLinePoints
		{
			float X;
			float Y;
		} ShoreLinePoints[MAX_SHORELINEPOINTS]; // struct tShoreLinePoints
		
	}; // struct ShoreLineLakeAudioSettings : ShoreLineAudioSettings
	
	// 
	// ShoreLineRiverAudioSettings
	// 
	enum ShoreLineRiverAudioSettingsFlagIds
	{
		FLAG_ID_SHORELINERIVERAUDIOSETTINGS_ISININTERIOR = 0,
		FLAG_ID_SHORELINERIVERAUDIOSETTINGS_MAX,
	}; // enum ShoreLineRiverAudioSettingsFlagIds
	
	struct ShoreLineRiverAudioSettings : ShoreLineAudioSettings
	{
		static const rage::u32 TYPE_ID = 92;
		static const rage::u32 BASE_TYPE_ID = ShoreLineAudioSettings::TYPE_ID;
		
		ShoreLineRiverAudioSettings() :
			NextShoreline(0U),
			RiverType(AUD_RIVER_MEDIUM),
			DefaultHeight(-9999.9f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 NextShoreline;
		rage::u8 RiverType;
		float DefaultHeight;
		
		static const rage::u8 MAX_SHORELINEPOINTS = 32;
		rage::u8 numShoreLinePoints;
		struct tShoreLinePoints
		{
			float X;
			float Y;
			float RiverWidth;
		} ShoreLinePoints[MAX_SHORELINEPOINTS]; // struct tShoreLinePoints
		
	}; // struct ShoreLineRiverAudioSettings : ShoreLineAudioSettings
	
	// 
	// ShoreLineOceanAudioSettings
	// 
	enum ShoreLineOceanAudioSettingsFlagIds
	{
		FLAG_ID_SHORELINEOCEANAUDIOSETTINGS_ISININTERIOR = 0,
		FLAG_ID_SHORELINEOCEANAUDIOSETTINGS_WAVEDETECTION,
		FLAG_ID_SHORELINEOCEANAUDIOSETTINGS_MAX,
	}; // enum ShoreLineOceanAudioSettingsFlagIds
	
	struct ShoreLineOceanAudioSettings : ShoreLineAudioSettings
	{
		static const rage::u32 TYPE_ID = 93;
		static const rage::u32 BASE_TYPE_ID = ShoreLineAudioSettings::TYPE_ID;
		
		ShoreLineOceanAudioSettings() :
			OceanType(AUD_OCEAN_TYPE_BEACH),
			OceanDirection(AUD_OCEAN_DIR_NORTH),
			NextShoreline(0U),
			WaveStartDPDistance(10.0f),
			WaveStartHeight(0.5f),
			WaveBreaksDPDistance(5.0f),
			WaveBreaksHeight(0.65f),
			WaveEndDPDistance(0.0f),
			WaveEndHeight(0.7f),
			RecedeHeight(0.5f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u8 OceanType;
		rage::u8 OceanDirection;
		rage::u32 NextShoreline;
		float WaveStartDPDistance;
		float WaveStartHeight;
		float WaveBreaksDPDistance;
		float WaveBreaksHeight;
		float WaveEndDPDistance;
		float WaveEndHeight;
		float RecedeHeight;
		
		static const rage::u8 MAX_SHORELINEPOINTS = 32;
		rage::u8 numShoreLinePoints;
		struct tShoreLinePoints
		{
			float X;
			float Y;
		} ShoreLinePoints[MAX_SHORELINEPOINTS]; // struct tShoreLinePoints
		
	}; // struct ShoreLineOceanAudioSettings : ShoreLineAudioSettings
	
	// 
	// ShoreLineList
	// 
	enum ShoreLineListFlagIds
	{
		FLAG_ID_SHORELINELIST_MAX = 0,
	}; // enum ShoreLineListFlagIds
	
	struct ShoreLineList
	{
		static const rage::u32 TYPE_ID = 94;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ShoreLineList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_SHORELINES = 1000;
		rage::u16 numShoreLines;
		struct tShoreLines
		{
			rage::u32 ShoreLine;
		} ShoreLines[MAX_SHORELINES]; // struct tShoreLines
		
	}; // struct ShoreLineList
	
	// 
	// RadioTrackSettings
	// 
	enum RadioTrackSettingsFlagIds
	{
		FLAG_ID_RADIOTRACKSETTINGS_MAX = 0,
	}; // enum RadioTrackSettingsFlagIds
	
	struct RadioTrackSettings
	{
		static const rage::u32 TYPE_ID = 95;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		RadioTrackSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Sound(0U),
			StartOffset(0.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 Sound;
		
		struct tHistory
		{
			rage::u8 Category;
			rage::u32 Sound;
		} History; // struct tHistory
		
		float StartOffset;
	}; // struct RadioTrackSettings
	
	// 
	// ModelFootStepTuning
	// 
	enum ModelFootStepTuningFlagIds
	{
		FLAG_ID_MODELFOOTSTEPTUNING_MAX = 0,
	}; // enum ModelFootStepTuningFlagIds
	
	struct ModelFootStepTuning
	{
		static const rage::u32 TYPE_ID = 96;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ModelFootStepTuning() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			FootstepPitchRatioMin(-600.0f),
			FootstepPitchRatioMax(400.0f),
			InLeftPocketProbability(0.5f),
			HasKeysProbability(0.1f),
			HasMoneyProbability(0.1f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		float FootstepPitchRatioMin;
		float FootstepPitchRatioMax;
		float InLeftPocketProbability;
		float HasKeysProbability;
		float HasMoneyProbability;
		
		static const rage::u8 MAX_MODES = 10;
		rage::u8 numModes;
		struct tModes
		{
			rage::u32 Name;
			
			struct tShoe
			{
				float VolumeOffset;
				rage::u32 LPFCutoff;
				rage::u32 AttackTime;
				float DragScuffProbability;
			} Shoe; // struct tShoe
			
			
			struct tDirtLayer
			{
				float VolumeOffset;
				rage::u32 LPFCutoff;
				rage::u32 AttackTime;
				rage::u32 SweetenerCurve;
			} DirtLayer; // struct tDirtLayer
			
			
			struct tCreakLayer
			{
				float VolumeOffset;
				rage::u32 LPFCutoff;
				rage::u32 AttackTime;
				rage::u32 SweetenerCurve;
			} CreakLayer; // struct tCreakLayer
			
			
			struct tGlassLayer
			{
				float VolumeOffset;
				rage::u32 LPFCutoff;
				rage::u32 AttackTime;
				rage::u32 SweetenerCurve;
			} GlassLayer; // struct tGlassLayer
			
			
			struct tWetLayer
			{
				float VolumeOffset;
				rage::u32 LPFCutoff;
				rage::u32 AttackTime;
				rage::u32 SweetenerCurve;
			} WetLayer; // struct tWetLayer
			
			
			struct tCustomImpact
			{
				float VolumeOffset;
				rage::u32 LPFCutoff;
				rage::u32 AttackTime;
			} CustomImpact; // struct tCustomImpact
			
			float MaterialImpactImpulseScale;
		} Modes[MAX_MODES]; // struct tModes
		
	}; // struct ModelFootStepTuning
	
	// 
	// GranularEngineSet
	// 
	enum GranularEngineSetFlagIds
	{
		FLAG_ID_GRANULARENGINESET_MAX = 0,
	}; // enum GranularEngineSetFlagIds
	
	struct GranularEngineSet
	{
		static const rage::u32 TYPE_ID = 97;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		GranularEngineSet() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_GRANULARENGINES = 10;
		rage::u8 numGranularEngines;
		struct tGranularEngines
		{
			rage::u32 GranularEngine;
			float Weight;
		} GranularEngines[MAX_GRANULARENGINES]; // struct tGranularEngines
		
	}; // struct GranularEngineSet
	
	// 
	// RadioDjSpeechAction
	// 
	enum RadioDjSpeechActionFlagIds
	{
		FLAG_ID_RADIODJSPEECHACTION_MAX = 0,
	}; // enum RadioDjSpeechActionFlagIds
	
	struct RadioDjSpeechAction : MusicAction
	{
		static const rage::u32 TYPE_ID = 98;
		static const rage::u32 BASE_TYPE_ID = MusicAction::TYPE_ID;
		
		RadioDjSpeechAction() :
			Station(0U),
			Category(kDjSpeechGeneral)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 Station;
		rage::u8 Category;
	}; // struct RadioDjSpeechAction : MusicAction
	
	// 
	// SilenceConstraint
	// 
	enum SilenceConstraintFlagIds
	{
		FLAG_ID_SILENCECONSTRAINT_MAX = 0,
	}; // enum SilenceConstraintFlagIds
	
	struct SilenceConstraint : MusicTimingConstraint
	{
		static const rage::u32 TYPE_ID = 99;
		static const rage::u32 BASE_TYPE_ID = MusicTimingConstraint::TYPE_ID;
		
		SilenceConstraint() :
			MinimumDuration(0.15f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		float MinimumDuration;
	}; // struct SilenceConstraint : MusicTimingConstraint
	
	// 
	// ReflectionsSettings
	// 
	enum ReflectionsSettingsFlagIds
	{
		FLAG_ID_REFLECTIONSSETTINGS_FILTERENABLED = 0,
		FLAG_ID_REFLECTIONSSETTINGS_STATICREFLECTIONS,
		FLAG_ID_REFLECTIONSSETTINGS_INVERTPHASECHANNEL0,
		FLAG_ID_REFLECTIONSSETTINGS_INVERTPHASECHANNEL1,
		FLAG_ID_REFLECTIONSSETTINGS_INVERTPHASECHANNEL2,
		FLAG_ID_REFLECTIONSSETTINGS_INVERTPHASECHANNEL3,
		FLAG_ID_REFLECTIONSSETTINGS_INVERTPHASECHANNEL4,
		FLAG_ID_REFLECTIONSSETTINGS_INVERTPHASECHANNEL5,
		FLAG_ID_REFLECTIONSSETTINGS_MAX,
	}; // enum ReflectionsSettingsFlagIds
	
	struct ReflectionsSettings
	{
		static const rage::u32 TYPE_ID = 100;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ReflectionsSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			MinDelay(0.0f),
			MaxDelay(2.0f),
			DelayTimeScalar(1.0f),
			DelayTimeAddition(0.1f),
			EnterSound(0U),
			ExitSound(0U),
			SubmixVoice(216594444U), // DEFAULT_REFLECTIONS_SUBMIX_CONTROL
			Smoothing(1.0f),
			PostSubmixVolumeAttenuation(0),
			RollOffScale(1.0f),
			FilterMode(kPeakingEQ),
			FilterFrequencyMin(10000.0f),
			FilterFrequencyMax(10000.0f),
			FilterResonanceMin(1.0f),
			FilterResonanceMax(1.0f),
			FilterBandwidthMin(100.0f),
			FilterBandwidthMax(100.0f),
			DistanceToFilterInput(1108420702U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		float MinDelay;
		float MaxDelay;
		float DelayTimeScalar;
		float DelayTimeAddition;
		rage::u32 EnterSound;
		rage::u32 ExitSound;
		rage::u32 SubmixVoice;
		float Smoothing;
		rage::s16 PostSubmixVolumeAttenuation;
		float RollOffScale;
		rage::u8 FilterMode;
		float FilterFrequencyMin;
		float FilterFrequencyMax;
		float FilterResonanceMin;
		float FilterResonanceMax;
		float FilterBandwidthMin;
		float FilterBandwidthMax;
		rage::u32 DistanceToFilterInput;
	}; // struct ReflectionsSettings
	
	// 
	// AlarmSettings
	// 
	enum AlarmSettingsFlagIds
	{
		FLAG_ID_ALARMSETTINGS_MAX = 0,
	}; // enum AlarmSettingsFlagIds
	
	struct AlarmSettings
	{
		static const rage::u32 TYPE_ID = 101;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AlarmSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			AlarmLoop(3817852694U), // NULL_SOUND
			AlarmDecayCurve(1667993428U), // AMBIENCE_DEFAULT_ALARM_DECAY
			StopDistance(1000U),
			InteriorSettings(0U),
			BankName(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 AlarmLoop;
		rage::u32 AlarmDecayCurve;
		rage::u32 StopDistance;
		rage::u32 InteriorSettings;
		rage::u32 BankName;
		
		rage::Vector3 CentrePosition;
		
	}; // struct AlarmSettings
	
	// 
	// FadeOutRadioAction
	// 
	enum FadeOutRadioActionFlagIds
	{
		FLAG_ID_FADEOUTRADIOACTION_MAX = 0,
	}; // enum FadeOutRadioActionFlagIds
	
	struct FadeOutRadioAction : MusicAction
	{
		static const rage::u32 TYPE_ID = 102;
		static const rage::u32 BASE_TYPE_ID = MusicAction::TYPE_ID;
		
		FadeOutRadioAction() :
			FadeTime(0.5f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		float FadeTime;
	}; // struct FadeOutRadioAction : MusicAction
	
	// 
	// FadeInRadioAction
	// 
	enum FadeInRadioActionFlagIds
	{
		FLAG_ID_FADEINRADIOACTION_MAX = 0,
	}; // enum FadeInRadioActionFlagIds
	
	struct FadeInRadioAction : MusicAction
	{
		static const rage::u32 TYPE_ID = 103;
		static const rage::u32 BASE_TYPE_ID = MusicAction::TYPE_ID;
		
		FadeInRadioAction() :
			FadeTime(0.5f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		float FadeTime;
	}; // struct FadeInRadioAction : MusicAction
	
	// 
	// ForceRadioTrackAction
	// 
	enum ForceRadioTrackActionFlagIds
	{
		FLAG_ID_FORCERADIOTRACKACTION_UNFREEZESTATION = 0,
		FLAG_ID_FORCERADIOTRACKACTION_MAX,
	}; // enum ForceRadioTrackActionFlagIds
	
	struct ForceRadioTrackAction : MusicAction
	{
		static const rage::u32 TYPE_ID = 104;
		static const rage::u32 BASE_TYPE_ID = MusicAction::TYPE_ID;
		
		ForceRadioTrackAction() :
			Station(0U),
			NextIndex(0U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 Station;
		rage::u32 NextIndex;
		
		static const rage::u8 MAX_TRACKLISTS = 16;
		rage::u8 numTrackLists;
		struct tTrackList
		{
			rage::audMetadataRef Track;
		} TrackList[MAX_TRACKLISTS]; // struct tTrackList
		
	}; // struct ForceRadioTrackAction : MusicAction
	
	// 
	// SlowMoSettings
	// 
	enum SlowMoSettingsFlagIds
	{
		FLAG_ID_SLOWMOSETTINGS_MAX = 0,
	}; // enum SlowMoSettingsFlagIds
	
	struct SlowMoSettings
	{
		static const rage::u32 TYPE_ID = 105;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		SlowMoSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Scene(0U),
			Priority(AUD_SLOWMO_GENERAL),
			Release(0.0f),
			SlowMoSound(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 Scene;
		rage::u8 Priority;
		float Release;
		rage::u32 SlowMoSound;
	}; // struct SlowMoSettings
	
	// 
	// PedScenarioAudioSettings
	// 
	enum PedScenarioAudioSettingsFlagIds
	{
		FLAG_ID_PEDSCENARIOAUDIOSETTINGS_ISSTREAMING = 0,
		FLAG_ID_PEDSCENARIOAUDIOSETTINGS_ALLOWSHAREDOWNERSHIP,
		FLAG_ID_PEDSCENARIOAUDIOSETTINGS_MAX,
	}; // enum PedScenarioAudioSettingsFlagIds
	
	struct PedScenarioAudioSettings
	{
		static const rage::u32 TYPE_ID = 106;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		PedScenarioAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			MaxInstances(1U),
			Sound(3817852694U), // NULL_SOUND
			SharedOwnershipRadius(100.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		rage::u32 MaxInstances;
		rage::u32 Sound;
		float SharedOwnershipRadius;
		
		static const rage::u8 MAX_PROPOVERRIDES = 16;
		rage::u8 numPropOverrides;
		struct tPropOverride
		{
			rage::u32 PropName;
			rage::u32 Sound;
		} PropOverride[MAX_PROPOVERRIDES]; // struct tPropOverride
		
	}; // struct PedScenarioAudioSettings
	
	// 
	// PortalSettings
	// 
	enum PortalSettingsFlagIds
	{
		FLAG_ID_PORTALSETTINGS_MAX = 0,
	}; // enum PortalSettingsFlagIds
	
	struct PortalSettings
	{
		static const rage::u32 TYPE_ID = 107;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		PortalSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			MaxOcclusion(0.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		float MaxOcclusion;
	}; // struct PortalSettings
	
	// 
	// ElectricEngineAudioSettings
	// 
	enum ElectricEngineAudioSettingsFlagIds
	{
		FLAG_ID_ELECTRICENGINEAUDIOSETTINGS_MAX = 0,
	}; // enum ElectricEngineAudioSettingsFlagIds
	
	struct ElectricEngineAudioSettings
	{
		static const rage::u32 TYPE_ID = 108;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ElectricEngineAudioSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			MasterVolume(0),
			SpeedLoop(3817852694U), // NULL_SOUND
			SpeedLoop_MinPitch(0),
			SpeedLoop_MaxPitch(600),
			SpeedLoop_ThrottleVol(0),
			BoostLoop(3817852694U), // NULL_SOUND
			BoostLoop_MinPitch(0),
			BoostLoop_MaxPitch(1200),
			BoostLoop_SpinupSpeed(100U),
			BoostLoop_Vol(0),
			RevsOffLoop(3817852694U), // NULL_SOUND
			RevsOffLoop_MinPitch(-500),
			RevsOffLoop_MaxPitch(1000),
			RevsOffLoop_Vol(0),
			BankLoadSound(3817852694U), // NULL_SOUND
			EngineStartUp(3817852694U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::s32 MasterVolume;
		rage::u32 SpeedLoop;
		rage::s32 SpeedLoop_MinPitch;
		rage::s32 SpeedLoop_MaxPitch;
		rage::s32 SpeedLoop_ThrottleVol;
		rage::u32 BoostLoop;
		rage::s32 BoostLoop_MinPitch;
		rage::s32 BoostLoop_MaxPitch;
		rage::u32 BoostLoop_SpinupSpeed;
		rage::s32 BoostLoop_Vol;
		rage::u32 RevsOffLoop;
		rage::s32 RevsOffLoop_MinPitch;
		rage::s32 RevsOffLoop_MaxPitch;
		rage::s32 RevsOffLoop_Vol;
		rage::u32 BankLoadSound;
		rage::u32 EngineStartUp;
	}; // struct ElectricEngineAudioSettings
	
	// 
	// PlayerBreathingSettings
	// 
	enum PlayerBreathingSettingsFlagIds
	{
		FLAG_ID_PLAYERBREATHINGSETTINGS_MAX = 0,
	}; // enum PlayerBreathingSettingsFlagIds
	
	struct PlayerBreathingSettings
	{
		static const rage::u32 TYPE_ID = 109;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		PlayerBreathingSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			TimeBetweenLowRunBreaths(0),
			TimeBetweenHighRunBreaths(0),
			TimeBetweenExhaustedBreaths(0),
			TimeBetweenFinalBreaths(0),
			MinBreathStateChangeWaitToLow(0),
			MaxBreathStateChangeWaitToLow(0),
			MinBreathStateChangeLowToHighFromWait(0),
			MaxBreathStateChangeLowToHighFromWait(0),
			MinBreathStateChangeHighToLowFromLow(0),
			MaxBreathStateChangeHighToLowFromLow(0),
			MinBreathStateChangeLowToHighFromHigh(0),
			MaxBreathStateChangeLowToHighFromHigh(0),
			MinBreathStateChangeExhaustedToIdleFromLow(0),
			MaxBreathStateChangeExhaustedToIdleFromLow(0),
			MinBreathStateChangeExhaustedToIdleFromHigh(0),
			MaxBreathStateChangeExhaustedToIdleFromHigh(0),
			MinBreathStateChangeLowToHighFromExhausted(0),
			MaxBreathStateChangeLowToHighFromExhausted(0)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::s32 TimeBetweenLowRunBreaths;
		rage::s32 TimeBetweenHighRunBreaths;
		rage::s32 TimeBetweenExhaustedBreaths;
		rage::s32 TimeBetweenFinalBreaths;
		rage::s32 MinBreathStateChangeWaitToLow;
		rage::s32 MaxBreathStateChangeWaitToLow;
		rage::s32 MinBreathStateChangeLowToHighFromWait;
		rage::s32 MaxBreathStateChangeLowToHighFromWait;
		rage::s32 MinBreathStateChangeHighToLowFromLow;
		rage::s32 MaxBreathStateChangeHighToLowFromLow;
		rage::s32 MinBreathStateChangeLowToHighFromHigh;
		rage::s32 MaxBreathStateChangeLowToHighFromHigh;
		rage::s32 MinBreathStateChangeExhaustedToIdleFromLow;
		rage::s32 MaxBreathStateChangeExhaustedToIdleFromLow;
		rage::s32 MinBreathStateChangeExhaustedToIdleFromHigh;
		rage::s32 MaxBreathStateChangeExhaustedToIdleFromHigh;
		rage::s32 MinBreathStateChangeLowToHighFromExhausted;
		rage::s32 MaxBreathStateChangeLowToHighFromExhausted;
	}; // struct PlayerBreathingSettings
	
	// 
	// PedWallaSpeechSettings
	// 
	enum PedWallaSpeechSettingsFlagIds
	{
		FLAG_ID_PEDWALLASPEECHSETTINGS_MAX = 0,
	}; // enum PedWallaSpeechSettingsFlagIds
	
	struct PedWallaSpeechSettings
	{
		static const rage::u32 TYPE_ID = 110;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		PedWallaSpeechSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			SpeechSound(3817852694U), // NULL_SOUND
			VolumeAboveRMSLevel(0),
			MaxVolume(0),
			PedDensityThreshold(0.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 SpeechSound;
		rage::s16 VolumeAboveRMSLevel;
		rage::s16 MaxVolume;
		float PedDensityThreshold;
		
		static const rage::u8 MAX_SPEECHCONTEXTS = 20;
		rage::u8 numSpeechContexts;
		struct tSpeechContexts
		{
			char ContextName[32];
			rage::s8 Variations;
		} SpeechContexts[MAX_SPEECHCONTEXTS]; // struct tSpeechContexts
		
	}; // struct PedWallaSpeechSettings
	
	// 
	// AircraftWarningSettings
	// 
	enum AircraftWarningSettingsFlagIds
	{
		FLAG_ID_AIRCRAFTWARNINGSETTINGS_MAX = 0,
	}; // enum AircraftWarningSettingsFlagIds
	
	struct AircraftWarningSettings
	{
		static const rage::u32 TYPE_ID = 111;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		AircraftWarningSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			MinTimeBetweenDamageReports(1000)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::s32 MinTimeBetweenDamageReports;
		
		struct tTargetedLock
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} TargetedLock; // struct tTargetedLock
		
		
		struct tMissleFired
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} MissleFired; // struct tMissleFired
		
		
		struct tAcquiringTarget
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} AcquiringTarget; // struct tAcquiringTarget
		
		
		struct tTargetAcquired
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} TargetAcquired; // struct tTargetAcquired
		
		
		struct tAllClear
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} AllClear; // struct tAllClear
		
		
		struct tPlaneWarningStall
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} PlaneWarningStall; // struct tPlaneWarningStall
		
		
		struct tAltitudeWarningLow
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
			float DownProbeLength;
		} AltitudeWarningLow; // struct tAltitudeWarningLow
		
		
		struct tAltitudeWarningHigh
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} AltitudeWarningHigh; // struct tAltitudeWarningHigh
		
		
		struct tEngine_1_Fire
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} Engine_1_Fire; // struct tEngine_1_Fire
		
		
		struct tEngine_2_Fire
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} Engine_2_Fire; // struct tEngine_2_Fire
		
		
		struct tEngine_3_Fire
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} Engine_3_Fire; // struct tEngine_3_Fire
		
		
		struct tEngine_4_Fire
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} Engine_4_Fire; // struct tEngine_4_Fire
		
		
		struct tDamagedSerious
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} DamagedSerious; // struct tDamagedSerious
		
		
		struct tDamagedCritical
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} DamagedCritical; // struct tDamagedCritical
		
		
		struct tOverspeed
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} Overspeed; // struct tOverspeed
		
		
		struct tTerrain
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
			float ForwardProbeLength;
		} Terrain; // struct tTerrain
		
		
		struct tPullUp
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
			rage::s32 MaxTimeSinceTerrainTriggerToPlay;
		} PullUp; // struct tPullUp
		
		
		struct tLowFuel
		{
			rage::s32 MinTimeInStateToTrigger;
			rage::s32 MaxTimeBetweenTriggerAndPlay;
			rage::s32 MinTimeBetweenPlay;
		} LowFuel; // struct tLowFuel
		
	}; // struct AircraftWarningSettings
	
	// 
	// PedWallaSettingsList
	// 
	enum PedWallaSettingsListFlagIds
	{
		FLAG_ID_PEDWALLASETTINGSLIST_MAX = 0,
	}; // enum PedWallaSettingsListFlagIds
	
	struct PedWallaSettingsList
	{
		static const rage::u32 TYPE_ID = 112;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		PedWallaSettingsList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_PEDWALLASETTINGSINSTANCES = 16;
		rage::u8 numPedWallaSettingsInstances;
		struct tPedWallaSettingsInstance
		{
			rage::u32 PedWallaSettings;
			float Weight;
			float PedDensityThreshold;
			rage::s8 IsMale;
			rage::s8 IsGang;
		} PedWallaSettingsInstance[MAX_PEDWALLASETTINGSINSTANCES]; // struct tPedWallaSettingsInstance
		
	}; // struct PedWallaSettingsList
	
	// 
	// CopDispatchInteractionSettings
	// 
	enum CopDispatchInteractionSettingsFlagIds
	{
		FLAG_ID_COPDISPATCHINTERACTIONSETTINGS_MAX = 0,
	}; // enum CopDispatchInteractionSettingsFlagIds
	
	struct CopDispatchInteractionSettings
	{
		static const rage::u32 TYPE_ID = 113;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		CopDispatchInteractionSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			MinTimeBetweenInteractions(60000),
			MinTimeBetweenInteractionsVariance(15000),
			FirstLinePredelay(1000),
			FirstLinePredelayVariance(250),
			SecondLinePredelay(1000),
			SecondLinePredelayVariance(250),
			ScannerPredelay(6000),
			ScannerPredelayVariance(1000),
			MinTimeBetweenSpottedAndVehicleLinePlays(1500),
			MinTimeBetweenSpottedAndVehicleLinePlaysVariance(500),
			MaxTimeAfterVehicleChangeSpottedLineCanPlay(5000),
			TimePassedSinceLastLineToForceVoiceChange(60000),
			NumCopsKilledToForceVoiceChange(4U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::s32 MinTimeBetweenInteractions;
		rage::s32 MinTimeBetweenInteractionsVariance;
		rage::s32 FirstLinePredelay;
		rage::s32 FirstLinePredelayVariance;
		rage::s32 SecondLinePredelay;
		rage::s32 SecondLinePredelayVariance;
		rage::s32 ScannerPredelay;
		rage::s32 ScannerPredelayVariance;
		rage::s32 MinTimeBetweenSpottedAndVehicleLinePlays;
		rage::s32 MinTimeBetweenSpottedAndVehicleLinePlaysVariance;
		rage::s32 MaxTimeAfterVehicleChangeSpottedLineCanPlay;
		rage::s32 TimePassedSinceLastLineToForceVoiceChange;
		rage::u32 NumCopsKilledToForceVoiceChange;
		
		struct tSuspectCrashedVehicle
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} SuspectCrashedVehicle; // struct tSuspectCrashedVehicle
		
		
		struct tSuspectEnteredFreeway
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} SuspectEnteredFreeway; // struct tSuspectEnteredFreeway
		
		
		struct tSuspectEnteredMetro
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} SuspectEnteredMetro; // struct tSuspectEnteredMetro
		
		
		struct tSuspectIsInCar
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} SuspectIsInCar; // struct tSuspectIsInCar
		
		
		struct tSuspectIsOnFoot
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} SuspectIsOnFoot; // struct tSuspectIsOnFoot
		
		
		struct tSuspectIsOnMotorcycle
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} SuspectIsOnMotorcycle; // struct tSuspectIsOnMotorcycle
		
		
		struct tSuspectLeftFreeway
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} SuspectLeftFreeway; // struct tSuspectLeftFreeway
		
		
		struct tRequestBackup
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} RequestBackup; // struct tRequestBackup
		
		
		struct tAcknowledgeSituation
		{
			rage::s32 MinTimeAfterCrime;
			rage::s32 MaxTimeAfterCrime;
		} AcknowledgeSituation; // struct tAcknowledgeSituation
		
		
		struct tUnitRespondingDispatch
		{
			float Probability;
		} UnitRespondingDispatch; // struct tUnitRespondingDispatch
		
		
		struct tRequestGuidanceDispatch
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
			rage::s32 TimeNotSpottedToTrigger;
			float ProbabilityOfMegaphoneLine;
		} RequestGuidanceDispatch; // struct tRequestGuidanceDispatch
		
		
		struct tHeliMaydayDispatch
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} HeliMaydayDispatch; // struct tHeliMaydayDispatch
		
		
		struct tShotAtHeli
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} ShotAtHeli; // struct tShotAtHeli
		
		
		struct tHeliApproachingDispatch
		{
			rage::s32 MinTimeBetween;
			rage::s32 MinTimeBetweenVariance;
		} HeliApproachingDispatch; // struct tHeliApproachingDispatch
		
	}; // struct CopDispatchInteractionSettings
	
	// 
	// RadioTrackTextIds
	// 
	enum RadioTrackTextIdsFlagIds
	{
		FLAG_ID_RADIOTRACKTEXTIDS_MAX = 0,
	}; // enum RadioTrackTextIdsFlagIds
	
	struct RadioTrackTextIds
	{
		static const rage::u32 TYPE_ID = 114;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		RadioTrackTextIds() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u32 MAX_TEXTIDS = 255;
		rage::u32 numTextIds;
		struct tTextId
		{
			rage::u32 OffsetMs;
			rage::u32 TextId;
		} TextId[MAX_TEXTIDS]; // struct tTextId
		
	}; // struct RadioTrackTextIds
	
	// 
	// RandomisedRadioEmitterSettings
	// 
	enum RandomisedRadioEmitterSettingsFlagIds
	{
		FLAG_ID_RANDOMISEDRADIOEMITTERSETTINGS_USEOCCLUSIONONSTATICEMITTERS = 0,
		FLAG_ID_RANDOMISEDRADIOEMITTERSETTINGS_USEOCCLUSIONONVEHICLEEMITTERS,
		FLAG_ID_RANDOMISEDRADIOEMITTERSETTINGS_MAX,
	}; // enum RandomisedRadioEmitterSettingsFlagIds
	
	struct RandomisedRadioEmitterSettings
	{
		static const rage::u32 TYPE_ID = 115;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		RandomisedRadioEmitterSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA),
			VehicleEmitterBias(0.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		float VehicleEmitterBias;
		
		struct tStaticEmitterConfig
		{
			rage::u32 StaticEmitter;
			float MinTime;
			float MaxTime;
			float MinFadeRadius;
			float MaxFadeRadius;
			rage::u32 RetriggerTimeMin;
			rage::u32 RetriggerTimeMax;
		} StaticEmitterConfig; // struct tStaticEmitterConfig
		
		
		struct tVehicleEmitterConfig
		{
			rage::u32 StaticEmitter;
			float MinTime;
			float MaxTime;
			rage::u32 MinAttackTime;
			rage::u32 MaxAttackTime;
			rage::u32 MinHoldTime;
			rage::u32 MaxHoldTime;
			rage::u32 MinReleaseTime;
			rage::u32 MaxReleaseTime;
			float MinPanAngleChange;
			float MaxPanAngleChange;
			rage::u32 RetriggerTimeMin;
			rage::u32 RetriggerTimeMax;
		} VehicleEmitterConfig; // struct tVehicleEmitterConfig
		
	}; // struct RandomisedRadioEmitterSettings
	
	// 
	// TennisVocalizationSettings
	// 
	enum TennisVocalizationSettingsFlagIds
	{
		FLAG_ID_TENNISVOCALIZATIONSETTINGS_MAX = 0,
	}; // enum TennisVocalizationSettingsFlagIds
	
	struct TennisVocalizationSettings
	{
		static const rage::u32 TYPE_ID = 116;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		TennisVocalizationSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		struct tLite
		{
			float NullWeight;
			float LiteWeight;
			float MedWeight;
			float StrongWeight;
		} Lite; // struct tLite
		
		
		struct tMed
		{
			float NullWeight;
			float LiteWeight;
			float MedWeight;
			float StrongWeight;
		} Med; // struct tMed
		
		
		struct tStrong
		{
			float NullWeight;
			float LiteWeight;
			float MedWeight;
			float StrongWeight;
		} Strong; // struct tStrong
		
	}; // struct TennisVocalizationSettings
	
	// 
	// DoorAudioSettingsLink
	// 
	enum DoorAudioSettingsLinkFlagIds
	{
		FLAG_ID_DOORAUDIOSETTINGSLINK_MAX = 0,
	}; // enum DoorAudioSettingsLinkFlagIds
	
	struct DoorAudioSettingsLink
	{
		static const rage::u32 TYPE_ID = 117;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		DoorAudioSettingsLink() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::audMetadataRef DoorAudioSettings;
	}; // struct DoorAudioSettingsLink
	
	// 
	// SportsCarRevsSettings
	// 
	enum SportsCarRevsSettingsFlagIds
	{
		FLAG_ID_SPORTSCARREVSSETTINGS_MAX = 0,
	}; // enum SportsCarRevsSettingsFlagIds
	
	struct SportsCarRevsSettings
	{
		static const rage::u32 TYPE_ID = 118;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		SportsCarRevsSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			EngineVolumeBoost(200),
			ExhaustVolumeBoost(600),
			RollOffBoost(15.0f),
			MinTriggerTime(60000U),
			MinRepeatTime(120000U),
			AttackTimeScalar(1.0f),
			ReleaseTimeScalar(0.25f),
			SmallReverbSend(0),
			MediumReverbSend(0),
			LargeReverbSend(30),
			JunctionTriggerSpeed(1.0f),
			JunctionStopSpeed(0.5f),
			JunctionMinDistance(25U),
			JunctionMaxDistance(70U),
			PassbyTriggerSpeed(10.0f),
			PassbyStopSpeed(5.0f),
			PassbyMinDistance(10U),
			PassbyMaxDistance(70U),
			PassbyLookaheadTime(2.0f),
			ClusterTriggerDistance(20U),
			ClusterTriggerSpeed(10.0f)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::s32 EngineVolumeBoost;
		rage::s32 ExhaustVolumeBoost;
		float RollOffBoost;
		rage::u32 MinTriggerTime;
		rage::u32 MinRepeatTime;
		float AttackTimeScalar;
		float ReleaseTimeScalar;
		rage::u8 SmallReverbSend;
		rage::u8 MediumReverbSend;
		rage::u8 LargeReverbSend;
		float JunctionTriggerSpeed;
		float JunctionStopSpeed;
		rage::u32 JunctionMinDistance;
		rage::u32 JunctionMaxDistance;
		float PassbyTriggerSpeed;
		float PassbyStopSpeed;
		rage::u32 PassbyMinDistance;
		rage::u32 PassbyMaxDistance;
		float PassbyLookaheadTime;
		rage::u32 ClusterTriggerDistance;
		float ClusterTriggerSpeed;
	}; // struct SportsCarRevsSettings
	
	// 
	// FoliageSettings
	// 
	enum FoliageSettingsFlagIds
	{
		FLAG_ID_FOLIAGESETTINGS_MAX = 0,
	}; // enum FoliageSettingsFlagIds
	
	struct FoliageSettings
	{
		static const rage::u32 TYPE_ID = 119;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		FoliageSettings() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Run(1922424370U), // BUSH_RUN_TEST
			Sprint(4016528369U), // BUSH_SPRINT_TEST
			Walk(1090937910U)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		rage::u32 Run;
		rage::u32 Sprint;
		rage::u32 Walk;
	}; // struct FoliageSettings
	
	// 
	// ReplayRadioStationTrackList
	// 
	enum ReplayRadioStationTrackListFlagIds
	{
		FLAG_ID_REPLAYRADIOSTATIONTRACKLIST_LOCKED = 0,
		FLAG_ID_REPLAYRADIOSTATIONTRACKLIST_MAX,
	}; // enum ReplayRadioStationTrackListFlagIds
	
	struct ReplayRadioStationTrackList
	{
		static const rage::u32 TYPE_ID = 120;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		ReplayRadioStationTrackList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF),
			Flags(0xAAAAAAAA)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		rage::u32 Flags;
		
		
		static const rage::u8 MAX_TRACKS = 250;
		rage::u8 numTracks;
		struct tTrack
		{
			rage::u32 TrackId;
			rage::u32 SoundRef;
		} Track[MAX_TRACKS]; // struct tTrack
		
	}; // struct ReplayRadioStationTrackList
	
	// 
	// MacsModelOverrideList
	// 
	enum MacsModelOverrideListFlagIds
	{
		FLAG_ID_MACSMODELOVERRIDELIST_MAX = 0,
	}; // enum MacsModelOverrideListFlagIds
	
	struct MacsModelOverrideList
	{
		static const rage::u32 TYPE_ID = 121;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		MacsModelOverrideList() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u8 MAX_ENTRYS = 32;
		rage::u8 numEntrys;
		struct tEntry
		{
			rage::u32 Model;
			rage::u32 Macs;
		} Entry[MAX_ENTRYS]; // struct tEntry
		
	}; // struct MacsModelOverrideList
	
	// 
	// MacsModelOverrideListBig
	// 
	enum MacsModelOverrideListBigFlagIds
	{
		FLAG_ID_MACSMODELOVERRIDELISTBIG_MAX = 0,
	}; // enum MacsModelOverrideListBigFlagIds
	
	struct MacsModelOverrideListBig
	{
		static const rage::u32 TYPE_ID = 122;
		static const rage::u32 BASE_TYPE_ID = 0xFFFFFFFF;
		
		MacsModelOverrideListBig() :
			ClassId(0xFF),
			NameTableOffset(0XFFFFFF)
		{}
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		rage::u32 ClassId : 8;
		rage::u32 NameTableOffset : 24;
		
		
		static const rage::u16 MAX_ENTRYS = 32768;
		rage::u16 numEntrys;
		struct tEntry
		{
			rage::u32 Model;
			rage::u32 Macs;
		} Entry[MAX_ENTRYS]; // struct tEntry
		
	}; // struct MacsModelOverrideListBig
	
	// 
	// SoundFieldDefinition
	// 
	enum SoundFieldDefinitionFlagIds
	{
		FLAG_ID_SOUNDFIELDDEFINITION_MAX = 0,
	}; // enum SoundFieldDefinitionFlagIds
	
	struct SoundFieldDefinition : AudBaseObject
	{
		static const rage::u32 TYPE_ID = 123;
		static const rage::u32 BASE_TYPE_ID = AudBaseObject::TYPE_ID;
		
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		
		static const rage::u8 MAX_EMITTERS = 240;
		rage::u8 numEmitters;
		struct tEmitter
		{
			
			struct tSphericalCoords
			{
				float Azimuth;
				float Zenith;
			} SphericalCoords; // struct tSphericalCoords
			
		} Emitter[MAX_EMITTERS]; // struct tEmitter
		
		
		static const rage::u16 MAX_HULLINDICES = 1100;
		rage::u16 numHullIndices;
		struct tHullIndices
		{
			rage::u32 Index;
		} HullIndices[MAX_HULLINDICES]; // struct tHullIndices
		
	}; // struct SoundFieldDefinition : AudBaseObject
	
	// 
	// GameObjectHashList
	// 
	enum GameObjectHashListFlagIds
	{
		FLAG_ID_GAMEOBJECTHASHLIST_MAX = 0,
	}; // enum GameObjectHashListFlagIds
	
	struct GameObjectHashList : AudBaseObject
	{
		static const rage::u32 TYPE_ID = 124;
		static const rage::u32 BASE_TYPE_ID = AudBaseObject::TYPE_ID;
		
		
		// PURPOSE - Returns a pointer to the field whose name is specified by the hash
		void* GetFieldPtr(const rage::u32 fieldNameHash);
		
		
		static const rage::u32 MAX_GAMEOBJECTHASHES = 65535;
		rage::u32 numGameObjectHashes;
		struct tGameObjectHashes
		{
			rage::u32 GameObjectHash;
		} GameObjectHashes[MAX_GAMEOBJECTHASHES]; // struct tGameObjectHashes
		
	}; // struct GameObjectHashList : AudBaseObject
	
} // namespace rage
#endif // AUD_GAMEOBJECTS_H
