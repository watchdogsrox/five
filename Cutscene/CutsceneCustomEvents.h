//
// Cutscene/CutSceneCustomEvents.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//
// WARNING: THIS IS AN AUTO GENERATED FILE FROM CUTFEVENTDEFEDIT. DO NOT EDIT.
//

#ifndef CUTSCENE_CUTSCENECUSTOMEVENTS_H
#define CUTSCENE_CUTSCENECUSTOMEVENTS_H

#include "cutfile/cutfevent.h"
#include "cutfile/cutfeventargs.h"

using namespace rage;

namespace CutSceneCustomEvents {

//##############################################################################

enum ECutsceneCustomEvent {
	CUTSCENE_SET_OVERLAY_PROPERTIES_EVENT = CUTSCENE_EVENT_FIRST_RESERVED_FOR_PROJECT_USE,
	CUTSCENE_SET_OVERLAY_METHOD_EVENT,
	CUTSCENE_SET_VEHICLE_DAMAGE_EVENT,
	CUTSCENE_SET_WATER_REFRACT_INDEX_EVENT,
	CUTSCENE_SET_CASCADE_SHADOW_BOUNDS_EVENT,
	CUTSCENE_SET_CASCADE_SHADOW_BOUNDS_PROPERTIES_EVENT,
	CUTSCENE_CUSTOM_CLOTH_EVENT,
	CUTSCENE_LAST_CUSTOM_EVENT,
	CUTSCENE_LOAD_INTERIOR_EVENT = CUTSCENE_LAST_CUSTOM_EVENT
};

//##############################################################################

static const char* s_cutsceneEventIdDisplayNames[] = {
	"Set Overlay Properties",
	"Set Overlay Method",
	"Set Vehicle Damage",
	"Set Water Refract Index",
	"Set Cascade Shadow Bounds",
	"Set Cascade Shadow Bounds Properties",
	"Custom cloth",
	"Load Interior"
};

// PURPOSE: Pass this function in to cutfEvent::AddEventIdDisplayNameFunc(...).
static inline const char* GetCutsceneCustomEventIdDisplayName( s32 iEventId )
{
	if ( (iEventId >= CUTSCENE_EVENT_FIRST_RESERVED_FOR_PROJECT_USE) && (iEventId <= CUTSCENE_LAST_CUSTOM_EVENT) )
	{
		int iEventIdOffset = iEventId - CUTSCENE_EVENT_FIRST_RESERVED_FOR_PROJECT_USE;
		return s_cutsceneEventIdDisplayNames[iEventIdOffset];
	}

	return "";
}

//##############################################################################

static const s32 c_cutsceneCustomEventSortingRanks[] = {
	CUTSCENE_LAST_EVENT_RANK + 1, // CUTSCENE_SET_OVERLAY_PROPERTIES_EVENT
	CUTSCENE_LAST_EVENT_RANK + 1, // CUTSCENE_SET_OVERLAY_METHOD_EVENT
	CUTSCENE_LAST_EVENT_RANK + 1, // CUTSCENE_SET_VEHICLE_DAMAGE_EVENT
	CUTSCENE_LAST_EVENT_RANK + 1, // CUTSCENE_SET_WATER_REFRACT_INDEX_EVENT
	CUTSCENE_LAST_EVENT_RANK + 1, // CUTSCENE_SET_CASCADE_SHADOW_BOUNDS_EVENT
	CUTSCENE_LAST_EVENT_RANK + 1, // CUTSCENE_SET_CASCADE_SHADOW_BOUNDS_PROPERTIES_EVENT
	CUTSCENE_LAST_EVENT_RANK + 1, // CUTSCENE_CUSTOM_CLOTH_EVENT
	CUTSCENE_LAST_EVENT_RANK + 1, // CUTSCENE_LOAD_INTERIOR_EVENT
};

// PURPOSE: Pass this function in to cutfEvent::AddEventIdRankFunc(...).
static inline s32 GetCutsceneCustomEventIdRank( s32 iEventId )
{
	if ( (iEventId >= CUTSCENE_EVENT_FIRST_RESERVED_FOR_PROJECT_USE) && (iEventId <= CUTSCENE_LAST_CUSTOM_EVENT) )
	{
		int iEventIdOffset = iEventId - CUTSCENE_EVENT_FIRST_RESERVED_FOR_PROJECT_USE;
		return c_cutsceneCustomEventSortingRanks[iEventIdOffset];
	}

	return CUTSCENE_LAST_UNLOAD_EVENT_RANK;
}

//##############################################################################

static const s32 c_cutsceneCustomOppositeEventIds[] = {
	CUTSCENE_NO_OPPOSITE_EVENT, // CUTSCENE_SET_OVERLAY_PROPERTIES_EVENT
	CUTSCENE_NO_OPPOSITE_EVENT, // CUTSCENE_SET_OVERLAY_METHOD_EVENT
	CUTSCENE_NO_OPPOSITE_EVENT, // CUTSCENE_SET_VEHICLE_DAMAGE_EVENT
	CUTSCENE_NO_OPPOSITE_EVENT, // CUTSCENE_SET_WATER_REFRACT_INDEX_EVENT
	CUTSCENE_NO_OPPOSITE_EVENT, // CUTSCENE_SET_CASCADE_SHADOW_BOUNDS_EVENT
	CUTSCENE_NO_OPPOSITE_EVENT, // CUTSCENE_SET_CASCADE_SHADOW_BOUNDS_PROPERTIES_EVENT
	CUTSCENE_NO_OPPOSITE_EVENT, // CUTSCENE_CUSTOM_CLOTH_EVENT
	CUTSCENE_NO_OPPOSITE_EVENT, // CUTSCENE_LOAD_INTERIOR_EVENT
};

// PURPOSE: Pass this function in to cutfEvent::AddOppositeEventIdFunc(...).
static inline s32 GetCutsceneCustomOppositeEventId( s32 iEventId )
{
	if ( (iEventId >= CUTSCENE_EVENT_FIRST_RESERVED_FOR_PROJECT_USE) && (iEventId <= CUTSCENE_LAST_CUSTOM_EVENT) )
	{
		int iEventIdOffset = iEventId - CUTSCENE_EVENT_FIRST_RESERVED_FOR_PROJECT_USE;
		return c_cutsceneCustomOppositeEventIds[iEventIdOffset];
	}

	return CUTSCENE_NO_OPPOSITE_EVENT;
}

//##############################################################################

enum ECutsceneCustomEventArgsType {
	CUTSCENE_GENERIC_EVENT_ARGS_TYPE = CUTSCENE_EVENT_ARGS_TYPE_FIRST_RESERVED_FOR_PROJECT_USE,
	CUTSCENE_TESTGENERIC_EVENT_ARGS_TYPE,
	CUTSCENE_LAST_CUSTOM_EVENT_ARGS_TYPE,
	CUTSCENE_CUSTOM_EVENT_ARGS_TYPE = CUTSCENE_LAST_CUSTOM_EVENT_ARGS_TYPE
};

//##############################################################################

static const char* s_cutsceneEventArgsTypeNames[] = {
	"Generic",
	"TestGeneric",
	"Custom",
};

// PURPOSE: Pass this function in to cutfEventArgs::AddEventArgsTypeNameFunc(...).
static inline const char* GetCutsceneCustomEventArgsTypeName( s32 iEventArgsType )
{
	if ( (iEventArgsType >= CUTSCENE_EVENT_ARGS_TYPE_FIRST_RESERVED_FOR_PROJECT_USE) && (iEventArgsType <= CUTSCENE_LAST_CUSTOM_EVENT_ARGS_TYPE) )
	{
		int iEventArgsTypeOffset = iEventArgsType - CUTSCENE_EVENT_ARGS_TYPE_FIRST_RESERVED_FOR_PROJECT_USE;
		return s_cutsceneEventArgsTypeNames[iEventArgsTypeOffset];
	}

	return "";
}

//##############################################################################

class ICutsceneGenericCustomEventArgs : public cutfEventArgs {
public:
	ICutsceneGenericCustomEventArgs() { }
	~ICutsceneGenericCustomEventArgs() { }
};

//##############################################################################

class ICutsceneTestGenericCustomEventArgs : public cutfEventArgs {
public:
	ICutsceneTestGenericCustomEventArgs() { }
	~ICutsceneTestGenericCustomEventArgs() { }
};

//##############################################################################

class ICutsceneCustomCustomEventArgs : public cutfEventArgs {
public:
	ICutsceneCustomCustomEventArgs() { }
	~ICutsceneCustomCustomEventArgs() { }

	float GetCapsuleLength() const { return m_attributeList.FindAttributeFloatValue( "CapsuleLength", -1.000000f ); }

	float GetCapsuleRadius() const { return m_attributeList.FindAttributeFloatValue( "CapsuleRadius", -1.000000f ); }

	int GetEventType() const { return m_attributeList.FindAttributeIntValue( "EventType", 0 ); }

	float GetPositionX() const { return m_attributeList.FindAttributeFloatValue( "PositionX", -1.000000f ); }

	float GetPositionY() const { return m_attributeList.FindAttributeFloatValue( "PositionY", -1.000000f ); }

	float GetPositionZ() const { return m_attributeList.FindAttributeFloatValue( "PositionZ", -1.000000f ); }

	float GetRotationX() const { return m_attributeList.FindAttributeFloatValue( "RotationX", -1.000000f ); }

	float GetRotationY() const { return m_attributeList.FindAttributeFloatValue( "RotationY", -1.000000f ); }

	float GetRotationZ() const { return m_attributeList.FindAttributeFloatValue( "RotationZ", -1.000000f ); }
};

//##############################################################################

} // namespace CutSceneCustomEvents

//##############################################################################

#endif // CUTSCENE_CUTSCENECUSTOMEVENTS_H
