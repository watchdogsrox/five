/////////////////////////////////////////////////////////////////////////////////
// Title	:	PopZones.h
// Author	:	Graeme, Adam Croston
// Started	:	19.06.00
// Purpose	:	To keep track of population areas on the map.
/////////////////////////////////////////////////////////////////////////////////
#ifndef POP_ZONES_H_
#define POP_ZONES_H_

// Rage headers
#include "atl/array.h"		// For atArray
#include "atl/hashstring.h"
#include "vector/vector3.h"	// For Vector3

// Game headers
#include "text/text.h"		// For char

enum eZoneCategory
{
	ZONECAT_ANY = -1,
    ZONECAT_NAVIGATION,
	ZONECAT_LOCAL_NAVIGATION,
    ZONECAT_INFORMATION,
    ZONECAT_MAP
};

enum eZoneType
{
    ZONETYPE_ANY,
    ZONETYPE_SP,
    ZONETYPE_MP
};

// When updating this enum, update ZONE_SCUMMINESS in commands_zone.sch [2/4/2013 mdawe]
enum eZoneScumminess
{
	SCUMMINESS_POSH,
	SCUMMINESS_NICE,
	SCUMMINESS_ABOVE_AVERAGE,
	SCUMMINESS_BELOW_AVERAGE,
	SCUMMINESS_CRAP,
	SCUMMINESS_SCUM,

	NUM_SCUMMINESS_LEVELS
};

enum eZoneSpecialAttributeType
{
	SPECIAL_NONE = 0,
	SPECIAL_AIRPORT,

	NUM_SPECIAL_ATTRIBUTE_TYPES
};

/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopZone
// Purpose		:	To store a a single population zone.
/////////////////////////////////////////////////////////////////////////////////
class CPopZone
{
	friend class CPopZones;
public:

	CPopZone();

	char *GetTranslatedName() const;

    atFinalHashString m_id;
    atFinalHashString m_associatedTextId;

	float m_vehDirtMin, m_vehDirtMax, m_dirtGrowScale;	// dirt range, scale and color
	float m_pedDirtMin, m_pedDirtMax;
	Color32 m_dirtCol;

	atHashWithStringNotFinal m_vfxRegionHashName;

	s16	m_uberZoneCentreX;			// calculated midpoint between all zones with the same m_associatedTextId
	s16	m_uberZoneCentreY;			// calculated midpoint between all zones with the same m_associatedTextId
	
	float m_fPopBaseRangeScale;		// Base range scale for this zone

	s16 m_iPopHeightScaleLowZ;			// There 3 variables control scaling up of vehicle population range based upon how far
	s16 m_iPopHeightScaleHighZ;			// above 'm_iPopHeightScaleLowZ' is, up to a max height of 'm_iPopHeightScaleHighZ' at
	float m_fPopHeightScaleMultiplier;	// which point the multiplier will be at its maxiumum of m_fPopHeightScaleMultiplier/.

	u8  m_scumminess : 3;				// Scumminess level of the zone. Correspondes to eZoneScumminess
	u8	m_plantsMgrTxdIdx : 3;		// PlantsMgr txd index in the zone (7=default)
	u8  m_lawResponseDelayType : 2;	// the type of law response delay to use (matches against a time in CWanted)
	u8	m_vehicleResponseType : 2;  // the zone type for vehicle response that this zone uses (used to compare against condition vehicle sets)
	u8	m_bNoCops : 1;
	u8	m_bBigZoneForScanner : 1;	// is this zone big enough to warrant saying "East <zone name>" etc on the scanner
	u8  m_specialAttributeType : 1; // Special attributes for the zone, if any.	
	u8  m_bAllowScenarioWeatherExits : 1; // Does this zone allow peds to leave their scenarios in case of rain/snow/etc
	u8	m_bLetHurryAwayLeavePavements : 1; // Does this zone allow hurry away to take peds off of pavement.

	s16 m_MPGangTerritoryIndex;		// In multiplayer, groups of adjacent zones can be given the same territory index 
	// (-1 means the zone doesn't belong to a territory that can be taken over by a gang)
};
CompileTimeAssert(NUM_SCUMMINESS_LEVELS <= 8);			 //If this fails, m_scumminess needs more space
CompileTimeAssert(NUM_SPECIAL_ATTRIBUTE_TYPES <= 2); //If this fails, m_specialAttributeType needs more space.

// Everything you need to search for a pop zone - location and type info
class CPopZoneSearchInfo
{
	friend class CPopZones;
public:
	// init values to zero
	// enabled by default
	CPopZoneSearchInfo();

	eZoneCategory m_ZoneCategory : 15; // more than enough bits
	u8	m_bEnabled: 1;
	s8	m_popScheduleIndexForSP;	// Acter, Acter industrial ...(from PopCycle.h)
	s8	m_popScheduleIndexForMP;	// Acter, Acter industrial ...(from PopCycle.h)

	s16	m_x1, m_y1, m_z1;
	s16	m_x2, m_y2, m_z2;

};

/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopZones
// Purpose		:	TODO
/////////////////////////////////////////////////////////////////////////////////
class CPopZones
{
public:
	enum {INVALID_ZONEID = -1};

	static void			Init                        (unsigned initMode);
	static void			Update						(void);
	
	// Population zones.
	static void	CreatePopZone (const char* pZoneIdName, const Vector3 &boxMin, const Vector3 &boxMax, const char* pAssociatedTextIdName, eZoneCategory ZoneCategory);
	// get population zone from name
	static CPopZone* GetPopZone(const char* pName);
	// get population zone from id
	static CPopZone* GetPopZone(int zoneId);
	// get population zone id from zone pointer
	static int GetPopZoneId(const CPopZone* pZone);

	static CPopZone* FindSmallestForPosition(const Vector3* pPoint, eZoneCategory zoneCategory, eZoneType zoneType);
	static bool	DoesPointLiesWithinPopZone	 (const Vector3* pPoint, const CPopZone* pZone);	
	static bool	DoesPointLieWithinPopZoneFast(int pPoint_X, int pPoint_Y, int pPoint_Z, const CPopZoneSearchInfo* pZone);
	static int	SmallestDistanceToPopZoneEdge(int pPoint_X, int pPoint_Y, int pPoint_Z, const CPopZoneSearchInfo* pZone);	
	static bool	IsPointInPopZonesWithText	 (const Vector3* pPoint, const char* pZoneLabel);

#if __BANK

	static bool			bPrintZoneInfo;
	static bool			bDisplayZonesOnVMap;
	static bool			bDisplayZonesInWorld;
	static bool			bDisplayOnlySlowLawResponseZones;
	static bool			bDisplayNonSlowLawResponseZones;

	static void			InitWidgets				(bkBank& bank);

#endif // __BANK

	static void	SetPopScheduleIndices(int zone, s32 popScheduleIndexForSP, s32 popScheduleIndexForMP);
	static void SetVehDirtRange (int zone, float dirtMin, float dirtMax);
	static void SetPedDirtRange (int zone, float dirtMin, float dirtMax);
	static void SetDirtGrowScale(int zone, float dirtScale);
	static void SetDirtColor (int zone, const Color32& dirtCol);
	static void	SetAllowCops (int zone, bool allowCops);
	static void SetMPGangTerritoryIndex(s32 zone, s32 territoryIndex);

	static void	SetEnabled (int zone, bool bEnabled);
	static bool	GetEnabled (int zone);

	static void SetVfxRegionHashName(int zone, atHashWithStringNotFinal hashName);
	static void SetPlantsMgrTxdIdx(int zone, u8 txdIdx);
	static void SetLawResponseDelayType(int zone, int iLawResponseDelayType);
	static void SetVehicleResponseType(int zone, int iVehicleResponseType);
	static void SetSpecialAttribute(int zone, int iSpecialAttributeType);

	static void SetScumminess (int zone, int eScumminess);
	static int  GetScumminess (int zone);

	static void SetPopulationRangeScaleParameters(int zone, float fBaseRangeScale, float fLowZ, float fHightZ, float fMaxHeightMultiplier);

	static void SetAllowScenarioWeatherExits(int zone, int iAllowScenarioWeatherExits);

	static void SetAllowHurryAwayToLeavePavement(int zone, int iAllow);

	static int GetZoneScumminessFromString(char* pString);
	static int GetSpecialZoneAttributeFromString(char* pString);

protected:
	static void ResetPopZones (void);
	static void PostCreation (void);
    static bool IsValidForSearchType (const CPopZoneSearchInfo *zone, eZoneCategory zoneCategory, eZoneType zoneType);
#if __ASSERT
	static bool IsEntirelyInsideAnother (const CPopZone* pSmallZone, const CPopZone* pBigZone);
	static bool DoesOverlapInOneDimension(s32 FirstMin, s32 FirstMax, s32 SecondMin, s32 SecondMax);
	static bool Overlaps(const CPopZone* pFirstZone, const CPopZone* pSecondZone);
	static void CheckForOverlap(void);
#endif // __ASSERT

public:
	static const CPopZoneSearchInfo* GetSearchInfoFromPopZone(const CPopZone* pPopZone);
	
	static s16 GetPopZoneX1(const CPopZone* zone) { return GetSearchInfoFromPopZone(zone)->m_x1;}
	static s16 GetPopZoneY1(const CPopZone* zone) { return GetSearchInfoFromPopZone(zone)->m_y1;} 
	static s16 GetPopZoneZ1(const CPopZone* zone)  { return GetSearchInfoFromPopZone(zone)->m_z1;} 
	static s16 GetPopZoneX2(const CPopZone* zone) { return GetSearchInfoFromPopZone(zone)->m_x2;} 
	static s16 GetPopZoneY2(const CPopZone* zone) { return GetSearchInfoFromPopZone(zone)->m_y2;} 
	static s16 GetPopZoneZ2(const CPopZone* zone) { return GetSearchInfoFromPopZone(zone)->m_z2;}
	static eZoneCategory GetPopZoneZoneCategory(const CPopZone* zone) { return GetSearchInfoFromPopZone(zone)->m_ZoneCategory; }
	static s8 GetPopZonePopScheduleIndexForSP(const CPopZone* zone) { return GetSearchInfoFromPopZone(zone)->m_popScheduleIndexForSP; }
	static s8 GetPopZonePopScheduleIndexForMP(const CPopZone* zone) { return GetSearchInfoFromPopZone(zone)->m_popScheduleIndexForMP; }
	static bool GetPopZoneEnabled(const CPopZone* zone) { return GetSearchInfoFromPopZone(zone)->m_bEnabled; }
protected:
	static atArray<CPopZoneSearchInfo> sm_popZoneSearchInfos;
	static atArray<CPopZone> 	sm_popZones;

	struct SmallestPopZoneCacheEntry
	{
		SmallestPopZoneCacheEntry():m_Result(NULL){};
		Vector3 m_Point;
		eZoneCategory m_ZoneCategory:16;
		eZoneType m_ZoneType:16;
		float m_smallestDistanceToEdge;
		CPopZone* m_Result;
	};
#define CACHED_SMALLEST_POP_ZONE_RESULTS (4)	
	static SmallestPopZoneCacheEntry sm_smallestPopZoneCache[CACHED_SMALLEST_POP_ZONE_RESULTS];
	static u8 sm_smallestPopZoneCacheIndex;
};


#endif // POP_ZONES_H_
