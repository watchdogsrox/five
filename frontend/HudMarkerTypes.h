/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HudMarkerTypes.h
// PURPOSE : Defines types for the Hud Marker system
// AUTHOR  : Aaron Baumbach (Ruffian)
// STARTED : 06/02/2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_HUD_MARKER_TYPES_H_
#define INC_HUD_MARKER_TYPES_H_

#include "vector/vector2.h"
#include "vector/vector3.h"
#include "vector/color32.h"
#include "fwmaths/random.h"
#include "fwmaths/Rect.h"
#include "atl/array.h"
#include "fwutil/Flags.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

#define MAX_NUM_MARKERS				(500)
#define MAX_NUM_MARKER_GROUPS		(32)

template<typename T, T _SIZE>
struct HudID final
{
	static_assert(std::is_unsigned<T>::value, "T must be an unsigned interger type");

	HudID() { Invalidate(); }
	HudID(T id) { m_id = id; }
	~HudID() { Invalidate(); } //No virtual destructor

	//Script initialises values to 0 so we want to consider that invalid (-1)
	inline static HudID FromScript(s32 id) { return (T)(id - 1); }
	inline s32 ToScript() const { return m_id + 1; }

	inline static T Min() { return 0; }
	inline static T Max() { return _SIZE - 1; }
	inline static T Size() { return _SIZE; }
	inline static T Rand() { return (T)fwRandom::GetRandomNumberInRange(Min(), Max()); }
	inline static bool IsValid(const T& val) { return val >= Min() && val <= Max(); }
	inline static void Invalidate(T& val) { val = (T)-1; }

	inline bool IsValid() const { return IsValid(m_id); }
	inline void Invalidate() { Invalidate(m_id); }
	inline T Get() const { return m_id; }

	//Conversion
	inline operator T&() { return m_id; }
	inline operator T() const { return m_id; }

	//Comparison
	inline bool operator==(const HudID& rhs) const { return m_id == rhs.m_id; }
	inline bool operator!=(const HudID& rhs) const { return !(*this == rhs); }

	//Increment/Decrement
	inline HudID& operator++() { m_id++; return *this; }
	inline HudID operator++(int) { HudID temp = *this; ++*this; return temp; }
	inline HudID& operator--() { m_id--; return *this; }
	inline HudID operator--(int) { HudID temp = *this; --*this; return temp; }
private:
	T m_id;
};

typedef HudID<u16, MAX_NUM_MARKERS> HudMarkerID;
typedef HudID<u8, MAX_NUM_MARKER_GROUPS> HudMarkerGroupID;

struct SHudMarkerRenderState
{
public:
	typedef fwFlags16 DirtyFlagsBitmask;

	SHudMarkerRenderState()
		: DirtyFlags(0)
		, ScreenPosition(0, 0)
		, CamDistanceSq(FLT_MAX)
		, Color(1.0f, 1.0f, 1.0f)
		, DistanceTextAlpha(0)
		, IconIndex(-1)
		, Depth((u16 )-1)
		, Scale(0.0f)
		, IsPendingRemoval(false)
		, IsVisible(false)
		, IsClamped(false)
		, IsFocused(false)
		, IsWaypoint(false)
	{
		Invalidate();
	}

	inline void Invalidate()
	{
		DirtyFlags.SetAllFlags();
	}

	enum Flags
	{
		Dirty_ScreenTransform	= BIT0,
		Dirty_PedDistance		= BIT1,
		Dirty_Icon				= BIT2,
		Dirty_Color				= BIT3,
		Dirty_Alpha				= BIT4,
		Dirty_DistanceTextAlpha = BIT5,
		Dirty_IsVisible			= BIT6,
		Dirty_IsClamped			= BIT7,
		Dirty_IsFocused			= BIT8,
		Dirty_IsWaypoint		= BIT9,
		Dirty_NUM				= 10	//If this goes above 16, please increase DirtyFlagsBitmask size
	};
	DirtyFlagsBitmask DirtyFlags;

	Vector2 ScreenPosition;
	float ScreenRotation; //North relative heading
	float CamDistanceSq;
	u16 PedDistance;
	Color32 Color;
	u8 Alpha;
	u8 DistanceTextAlpha;
	s32 IconIndex;
	u16 Depth;
	float Scale;
	bool IsPendingRemoval : 1;
	bool IsVisible : 1;
	bool IsClamped : 1;
	bool IsFocused : 1;
	bool IsWaypoint : 1;
};

struct SHudMarkerState
{
	SHudMarkerState()
		: BlipID(0)
		, WorldPosition(Vector3::ZeroType)
		, WorldHeightOffset(0.0f)
		, PedCullDistance(500.0f)
		, PedDistance(FLT_MAX)
		, CamDistanceSq(FLT_MAX)
		, ScreenPosition(0, 0)
		, ScreenRotation(0)
		, Color(1.0f, 1.0f, 1.0f)
		, Alpha(0)
		, DistanceTextAlpha(0)
		, Scale(0.0f)
		, IconIndex(-1)
		, IsInitialised(false)
		, IsPendingRemoval(false)
		, HasRenderableShutdown(false)
		, IsVisible(false)
		, IsClamped(false)
		, ClampEnabled(false)
		, IsOccluded(false)
		, IsFocused(false)
		, IsWaypoint(false)
	{
	}

	~SHudMarkerState()
	{
	}

	HudMarkerID ID;
	HudMarkerGroupID GroupID;
	s32 BlipID;
	Vector3 WorldPosition;
	float WorldHeightOffset;
	float PedCullDistance;
	float PedDistance;
	float CamDistanceSq;
	Vector2 ScreenPosition;
	float ScreenRotation;
	Color32 Color;
	u8 Alpha;
	u8 DistanceTextAlpha;
	float Scale;
	s32 IconIndex;

	bool IsInitialised : 1;
	bool IsPendingRemoval : 1;
	bool HasRenderableShutdown : 1;
	bool IsVisible : 1;
	bool IsClamped : 1;
	bool ClampEnabled : 1;
	bool IsOccluded : 1;
	bool IsFocused : 1;
	bool IsWaypoint : 1;
};

struct SHudMarkerGroup
{
	SHudMarkerGroup()
		: MaxClampNum(1)
		, ForceVisibleNum(1)
		, MaxVisible(HudMarkerID::Size())
		, MinDistance(0)
		, MaxDistance(FLT_MAX)
		, MinTextDistance(0.0f)
		, IsSolo(false)
		, IsMuted(false)
		, AlwaysShowDistanceTextForClosest(false)
	{
		Reset();
	}

	inline void Reset()
	{
		NumClamped = 0;
		NumVisible = 0;
	}

	//Config
	int MaxClampNum;
	int ForceVisibleNum;
	int MaxVisible;
	float MinDistance;
	float MaxDistance;
	float MinTextDistance;
	bool IsSolo : 1; //Hide all other non-solo groups
	bool IsMuted : 1; //Hide this group
	bool AlwaysShowDistanceTextForClosest : 1;

private:
	friend class CHudMarkerManager;

	//Transient
	int NumClamped;
	int NumVisible;
};

enum HudMarkerClampMode
{
	ClampMode_None,
	ClampMode_Radial,
	ClampMode_Rectangular,
	ClampMode_NUM
};

enum HudMarkerDistanceMode
{
	DistanceMode_Default,
	DistanceMode_Metric,
	DistanceMode_Metric_MetresOnly,
	DistanceMode_Imperial,
	DistanceMode_Imperial_FeetOnly,
	DistanceMode_NUM
};

typedef atFixedArray<SHudMarkerState,		MAX_NUM_MARKERS>		HudMarkerStateArray;
typedef atFixedArray<SHudMarkerRenderState,	MAX_NUM_MARKERS>		HudMarkerRenderStateArray;
typedef atFixedArray<HudMarkerID,			MAX_NUM_MARKERS>		HudMarkerIDArray;
typedef atFixedArray<SHudMarkerGroup,		MAX_NUM_MARKER_GROUPS>	HudMarkerGroupArray;

#endif //INC_HUD_MARKER_TYPES_H_