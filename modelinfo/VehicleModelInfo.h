//
//
//    Filename: VehicleModelInfo.h
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Class describing a vehicle model
//
//
#ifndef INC_VEHICLE_MODELINFO_H_
#define INC_VEHICLE_MODELINFO_H_

// Rage headers
#include "vector/vector3.h"
#include "vector/quaternion.h"
#include "fwscene/stores/psostore.h"

// Game headers
#include "game/ModelIndices.h"
#include "modelinfo/BaseModelInfo.h"
#include "modelinfo/ModelSeatInfo.h"
#include "modelinfo/VehicleModelInfoEnums.h"
#include "modelinfo/VehicleModelInfoFlags.h"
#include "modelinfo/VehicleModelInfoPlates.h"
#include "renderer/Color.h"
#include "renderer/HierarchyIds.h"
#include "streaming/streamingrequest.h"
#include "task/Combat/Cover/Cover.h" // For the CCoverPointInfo class
#include "vehicles/VehicleDefines.h"
#include "vehicles/vehicle_channel.h"

#if !__SPU
	// Rage headers
	#include "fwscene/stores/txdstore.h"

	// Game headers
	#include "scene/loader/MapData_Extensions.h"
	#include "scene/loader/MapData_Misc.h"
	#include "fwtl/pool.h"
	#include "vfx/metadata/vfxvehicleinfo.h"
#endif //!__SPU...


class phInstGta;
class CHandlingData;
class CTxdRelationship;
class CVehicleLayoutInfo;
class CPOVTuningInfo;
class CVehicleCoverBoundOffsetInfo;
class CVehicleScenarioLayoutInfo;
class CVehicleSeatInfo;
class CVehicleSeatAnimInfo;
class CVehicleEntryPointAnimInfo;
class CVehicleEntryPointInfo;
class CVehicleExplosionInfo;
class CVehicleModColors;
class CVehicleModelInfoVarGlobal;
class CVehicleModelColorList;
class CVehicleModelInfoVariation;
class CVehicleVariationData;
class CVehicleVariationInstance;
class CVfxVehicleInfo;
struct vehicleLightSettings;
struct sirenSettings;
class CFirstPersonDriveByLookAroundData;

#if __BANK
class CUiGadgetWindow;
class CUiGadgetList;
class CUiGadgetText;
class CVehicleModelInfo;

struct HDVehilceRefCount
{
	CVehicleModelInfo *pVMI;
	u32                count;
};

struct sDebugSize;
#endif

// Typedefs
typedef CVehicleModelInfoFlags::FlagsBitSet VehicleModelInfoFlags;

namespace rage {
	class bkBank;
}

#define CARCOL_TEXTURE_IDENTIFIER	'@'
#define REMAP_TEXTURE_IDENTIFIER	'#'
#define LIGHTS_TEXTURE_IDENTIFIER	'~'

#define VEHICLE_MAX_NUM_RESIDENT_TXDS		(5)
#define VEHICLE_LIGHTS_TEXTURE		"vehiclelights128"
#define VEHICLE_LIGHTSON_TEXTURE	"vehiclelightson128"

#define MAX_PER_VEHICLE_MATERIALS		(48)
#define MAX_PER_VEHICLE_MATERIALS2		(32)
#define MAX_PER_VEHICLE_MATERIALS3		(16)
#define MAX_PER_VEHICLE_MATERIALS4		(16)
#define MAX_PER_VEHICLE_DIRT_MATERIALS	(32)
#define NUM_DIRT_LEVELS					(16)

#define MAX_VEHICLE_GAME_NAME		(12)



#define INVALID_STEER_ANGLE			(999.99f)


#define CUSTOM_PLATE_NUM_CHARS		(8)

#define MAT1_COLOUR_RED		0x3c
#define MAT1_COLOUR_GREEN	0xff
#define MAT1_COLOUR_BLUE	0
#define MAT2_COLOUR_RED		0xff
#define MAT2_COLOUR_GREEN	0
#define MAT2_COLOUR_BLUE	0xaf
#define MAT3_COLOUR_RED		0x0
#define MAT3_COLOUR_GREEN	0xff
#define MAT3_COLOUR_BLUE	0xff
#define MAT4_COLOUR_RED		0xff
#define MAT4_COLOUR_GREEN	0
#define MAT4_COLOUR_BLUE	0xff

#define MAT1_COLOUR			0x00ff3c
#define MAT2_COLOUR			0xaf00ff
#define MAT3_COLOUR			0xffff00
#define MAT4_COLOUR			0xff00ff

#define LIGHT_FRONT_LEFT_COLOUR		0x00afff
#define LIGHT_FRONT_RIGHT_COLOUR	0xc8ff00
#define LIGHT_REAR_LEFT_COLOUR		0x00ffb9
#define LIGHT_REAR_RIGHT_COLOUR		0x003cff

#define ENABLE_VEHICLECOLOURTEXT	(__BANK || (__XENON && !__FINAL))

#define	VEH_MAX_DECAL_BONE_FLAGS	128

#define CHECK_VEHICLE_SETUP

enum eDoorId
{
	VEH_EXT_DOORS_INVALID_ID = -1,
	VEH_EXT_DOOR_DSIDE_F,
	VEH_EXT_DOOR_DSIDE_R,
	VEH_EXT_DOOR_PSIDE_F,
	VEH_EXT_DOOR_PSIDE_R,
	VEH_EXT_BONNET,
	VEH_EXT_BOOT
};

enum eWindowId
{
	VEH_EXT_WINDOWS_INVALID_ID = -1,
	VEH_EXT_WINDSCREEN,
	VEH_EXT_WINDSCREEN_R,
	VEH_EXT_WINDOW_LF,
	VEH_EXT_WINDOW_RF,
	VEH_EXT_WINDOW_LR,
	VEH_EXT_WINDOW_RR,
	VEH_EXT_WINDOW_LM,
	VEH_EXT_WINDOW_RM,

	VEH_EXT_NUM_WINDOWS
};

enum VehicleType
{
	// If you change this enum check the type functions in CVehicle are still valid

	VEHICLE_TYPE_NONE = -1,

	VEHICLE_TYPE_CAR = 0, // Start of types inheriting from automobile
		// derived from automobile
		VEHICLE_TYPE_PLANE,
		VEHICLE_TYPE_TRAILER,
		VEHICLE_TYPE_QUADBIKE,
		VEHICLE_TYPE_DRAFT,
		VEHICLE_TYPE_SUBMARINECAR,
		VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE,
		VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE,

		// Derived from CRotaryWingAircraft (which derives CAutomobile!)
		VEHICLE_TYPE_HELI,
		VEHICLE_TYPE_BLIMP,
		VEHICLE_TYPE_AUTOGYRO, // End of types inheriting from automobile 

	VEHICLE_TYPE_BIKE,
		// derived from bike
		VEHICLE_TYPE_BICYCLE,

	VEHICLE_TYPE_BOAT,
	VEHICLE_TYPE_TRAIN,
	VEHICLE_TYPE_SUBMARINE,

	VEHICLE_TYPE_NUM
};

// atomic type
enum VehicleAtomicType
{
	VEHICLE_ATOMIC_NONE						= 0,
	VEHICLE_ATOMIC_OK 						= (1<<0),
	VEHICLE_ATOMIC_DAMAGED 					= (1<<1),
	VEHICLE_ATOMIC_MASK 					= 3,
	VEHICLE_ATOMIC_LEFT 					= (1<<2),	// the following four flags define where on the vehicle
	VEHICLE_ATOMIC_RIGHT 					= (1<<3),	// a component is. This is used to order the drawing
	VEHICLE_ATOMIC_FRONT		 			= (1<<4),	// of components with transparency e.g doors, windscreens
	VEHICLE_ATOMIC_REAR 					= (1<<5),
	VEHICLE_ATOMIC_ALPHA 					= (1<<6),	// this components drawing is always posponed
	VEHICLE_ATOMIC_FLAT 					= (1<<7),	// this component is flat and can easily be hidden when not facing the camera
	VEHICLE_ATOMIC_REARDOOR 				= (1<<8),	// this component is a rear door
	VEHICLE_ATOMIC_FRONTDOOR			 	= (1<<9),	// this component is a front door
	VEHICLE_ATOMIC_DONTCULL 				= (1<<10),	// dont cull this component 
	VEHICLE_ATOMIC_UPGRADE 					= (1<<11),	// this is an upgrade
	VEHICLE_ATOMIC_DONTRENDERALPHA			= (1<<12),	// flag to stop alpha being rendered (eg switch off windows)
	VEHICLE_ATOMIC_UNIQUE_MATERIALS 		= (1<<13),	// Don't attempt to merge materials on this atomic
	VEHICLE_ATOMIC_PIPE_NO_EXTRA_PASSES_LOD	= (1<<13),	// Like VEHICLE_ATOMIC_PIPE_NO_EXTRA_PASSES, but set accordingly to vehicle's distance to camera
	VEHICLE_ATOMIC_PIPE_NO_EXTRA_PASSES		= (1<<14),	// render pipe for this atomic should not draw 2nd (envmap) and/or 3rd (spec) pass
														// useful when vehicle blew up and should be rendered in simplified form (1 pass visible only)
	VEHICLE_ATOMIC_TOP	= (1<<15)
};



// hierarchy flags
#define CLUMP_NO_FRAMEID		(0x1)		// don't store frame id
#define VEHICLE_CRUSH			(0x2)	// remove hierarchy under this component
#define VEHICLE_ADD_WHEELS		(0x4) 	// add a wheel to this component
#define VEHICLE_STORE_POSN		(0x8)	// store position
#define VEHICLE_DOOR			(0x10)	// this component is a door. Used to calculate number of passengers
#define VEHICLE_LEFT			(0x20)	// the following four flags define where on the vehicle
#define VEHICLE_RIGHT			(0x40)	// a component is. This is used to order the drawing
#define VEHICLE_FRONT			(0x80)	// of components with transparency e.g doors, windscreens
#define VEHICLE_REAR			(0x100)
#define VEHICLE_XTRACOMP		(0x200)
#define VEHICLE_ALPHA			(0x400)	// this component is always drawn last
#define VEHICLE_WINDSCREEN		(0x800) // this component isn't drawn when in first person mode
#define VEHICLE_FLAT			(0x1000) // this component is flat and can easily be hidden when not facing the camera
#define VEHICLE_REARDOOR		(0x2000) // this component is flat and can easily be hidden when not facing the camera
#define VEHICLE_FRONTDOOR		(0x4000) // this component is flat and can easily be hidden when not facing the camera
#define VEHICLE_TOP				(0x8000) // this component is above the vehicle (for drawing order)
#define VEHICLE_PARENT_WHEEL	(0x10000)	// this is the wheel to clone to the other wheels
#define VEHICLE_UPGRADE_POSN	(0x20000)	// Vehicle upgrade position
#define VEHICLE_UNIQUE_MATERIALS	(0x40000)	// Component has unique materials
#define VEHICLE_NO_DAMAGE_MODEL		(0x80000)	// component has no damage model (but probably should)
#define VEHICLE_PARENT_CLONE_ATOMIC	(0x100000)	// for trains we want to clone and copy the bogeys
#define VEHICLE_ADD_CLONE_ATOMIC	(0x200000)	// after we've added the wheels
#define VEHICLE_DONTCULL		(0x400000)	// don't attempt to cull this object

enum eDialsMovies
{
    DIALS_VEHICLE,
    DIALS_AIRCRAFT,
    DIALS_LAZER,
	DIALS_LAZER_VINTAGE,
	DIALS_PBUS_2,
    MAX_DIALS_MOVIES
};

enum eVehicleLodType
{
	VLT_HD = 0,
	VLT_LOD0,
	VLT_LOD1,
	VLT_LOD2,
	VLT_LOD3,
	VLT_LOD_FADE,
	VLT_MAX
};

//-------------------------------------------------------------------------
// The direction you need to move in enter in get to the seat
//-------------------------------------------------------------------------
typedef enum
{
	Dir_invalid = -1,
	Dir_left = 0,
	Dir_right,
	Dir_front,
	Dir_back
} EntryExitDirection;

class CVehicleModelInfo;
class CCustomShaderEffectVehicleType;
class CSeaPlaneHandlingData;

struct VehicleHDStreamReq
{
	strRequestArray<2>			m_Requests;
	CVehicleModelInfo*		m_pTargetVehicleMI;
};

struct sVehicleDashboardData
{
	bool HaveVehicleDialsChanged(const sVehicleDashboardData& nd)
	{
		const float eps = 0.001f;
		return !rage::IsClose(revs, nd.revs, eps) ||
			   !rage::IsClose(speed, nd.speed, eps) ||
			   !rage::IsClose(fuel, nd.fuel, eps) ||
			   !rage::IsClose(engineTemp, nd.engineTemp, eps) ||
			   !rage::IsClose(vacuum, nd.vacuum, eps) ||
			   !rage::IsClose(boost, nd.boost, eps) ||
			   !rage::IsClose(oilTemperature, nd.oilTemperature, eps) ||
			   !rage::IsClose(oilPressure, nd.oilPressure, eps) ||
			   !rage::IsClose(currentGear, nd.currentGear, eps);
	}

	bool HaveVehicleLightsChanged(const sVehicleDashboardData& nd)
	{
		return indicatorLeft != nd.indicatorLeft ||
			   indicatorRight != nd.indicatorRight ||
			   handBrakeLight != nd.handBrakeLight ||
			   engineLight != nd.engineLight ||
			   absLight != nd.absLight ||
			   fuelLight != nd.fuelLight ||
			   oilLight != nd.oilLight ||
			   headLightsLight != nd.headLightsLight ||
			   fullBeamLight != nd.fullBeamLight ||
			   batteryLight != nd.batteryLight;
	}

	bool HaveAircraftDialsChanged(const sVehicleDashboardData& nd)
	{
		const float eps = 0.001f;
		return !rage::IsClose(aircraftFuel, nd.aircraftFuel, eps) ||
			   !rage::IsClose(aircraftTemp, nd.aircraftTemp, eps) ||
			   !rage::IsClose(aircraftOilPressure, nd.aircraftOilPressure, eps) ||
			   !rage::IsClose(aircraftBattery, nd.aircraftBattery, eps) ||
			   !rage::IsClose(aircraftFuelPressure, nd.aircraftFuelPressure, eps) ||
			   !rage::IsClose(aircraftAirSpeed, nd.aircraftAirSpeed, eps) ||
			   !rage::IsClose(aircraftVerticalSpeed, nd.aircraftVerticalSpeed, eps) ||
			   !rage::IsClose(aircraftCompass, nd.aircraftCompass, eps) ||
			   !rage::IsClose(aircraftRoll, nd.aircraftRoll, eps) ||
			   !rage::IsClose(aircraftPitch, nd.aircraftPitch, eps) ||
			   !rage::IsClose(aircraftAltitudeSmall, nd.aircraftAltitudeSmall, eps) ||
			   !rage::IsClose(aircraftAltitudeLarge, nd.aircraftAltitudeLarge, eps);
	}

	bool HaveAircraftLightsChanged(const sVehicleDashboardData& nd)
	{

		return aircraftGearUp != nd.aircraftGearUp ||
			   aircraftGearDown != nd.aircraftGearDown ||
			   pressureAlarm != nd.pressureAlarm;
	}

	bool HaveAircraftHudDialsChanged(const sVehicleDashboardData& nd)
	{
		const float eps = 0.001f;
		return !rage::IsClose(aircraftAir, nd.aircraftAir, eps) ||
			   !rage::IsClose(aircraftFuel, nd.aircraftFuel, eps) ||
			   !rage::IsClose(aircraftOil, nd.aircraftOil, eps) ||
			   !rage::IsClose(aircraftVacuum, nd.aircraftVacuum, eps) ||
			   !rage::IsClose(aircraftPitch, nd.aircraftPitch, eps) ||
			   !rage::IsClose(aircraftRoll, nd.aircraftRoll, eps) ||
			   !rage::IsClose(aircraftAltitudeLarge, nd.aircraftAltitudeLarge, eps) ||
			   !rage::IsClose(aircraftAirSpeed, nd.aircraftAirSpeed, eps);
	}

    float revs;
    float speed;
    float fuel;
    float engineTemp;
	float vacuum;
	float boost;
	float currentGear;
	float oilTemperature;
	float oilPressure;
	float aircraftFuel;
	float aircraftOil;
	float aircraftTemp;
	float aircraftAir;
	float aircraftVacuum;
	float aircraftPitch;
	float aircraftRoll;
	float aircraftAltitudeSmall;
	float aircraftAltitudeLarge;
	float aircraftAirSpeed;
	float aircraftVerticalSpeed;
	float aircraftCompass;
	float aircraftFuelPressure;
	float aircraftOilPressure;
	float aircraftBattery;

    bool aircraftGearUp;
    bool aircraftGearDown;
	bool pressureAlarm;
    bool indicatorLeft;
    bool indicatorRight;
    bool handBrakeLight;
    bool engineLight;
    bool absLight;
    bool fuelLight;
    bool oilLight;
    bool headLightsLight;
    bool fullBeamLight;
    bool batteryLight;
};

enum eSwankness // 0 = crap, 5 = super nice
{
	SWANKNESS_0 = 0,
	SWANKNESS_1,
	SWANKNESS_2,
	SWANKNESS_3,
	SWANKNESS_4,
	SWANKNESS_5,
	SWANKNESS_MAX
};

enum eVehiclePlateType
{
	VPT_FRONT_AND_BACK_PLATES = 0,
	VPT_FRONT_PLATES,
	VPT_BACK_PLATES,
	VPT_NONE,
	VPT_MAX
};

enum eVehicleDashboardType
{
    VDT_BANSHEE = 0,
    VDT_BOBCAT,
    VDT_CAVALCADE,
    VDT_COMET,
    VDT_DUKES,
    VDT_FACTION,
    VDT_FELTZER,
    VDT_FEROCI,
    VDT_FUTO,
    VDT_GENTAXI,
    VDT_MAVERICK,
    VDT_PEYOTE,
    VDT_RUINER,
    VDT_SPEEDO,
    VDT_SULTAN,
    VDT_SUPERGT,
    VDT_TAILGATER,
    VDT_TRUCK,
    VDT_TRUCKDIGI,
    VDT_INFERNUS,
    VDT_ZTYPE,
    VDT_LAZER,
	VDT_SPORTBK,
	VDT_RACE,
	VDT_LAZER_VINTAGE,
	VDT_PBUS2,

    VDT_MAX
};

struct VehicleClassInfo
{
	float m_fMaxSpeedEstimate;
	float m_fMaxTraction;
	float m_fMaxAgility;
	float m_fMaxAcceleration;
	float m_fMaxBraking;
};

enum eVehicleClass
{
    VC_COMPACT = 0,
    VC_SEDAN,
    VC_SUV,
    VC_COUPE,
    VC_MUSCLE,
    VC_SPORT_CLASSIC,
    VC_SPORT,
    VC_SUPER,
    VC_MOTORCYCLE,
    VC_OFF_ROAD,
    VC_INDUSTRIAL,
    VC_UTILITY,
    VC_VAN,
    VC_CYCLE,
    VC_BOAT,
    VC_HELICOPTER,
    VC_PLANE,
	VC_SERVICE,
	VC_EMERGENCY,
	VC_MILITARY,
	VC_COMMERCIAL,
	VC_RAIL,
    VC_OPEN_WHEEL,

    VC_MAX
};

//
// name:		CVehicleStructure
// description:	Class containing info ascertained from the vehicle model structure
class CVehicleStructure
{
public:
	atFixedBitSet<VEH_MAX_DECAL_BONE_FLAGS> m_decalBoneFlags;

	s8		m_nBoneIndices[VEH_NUM_NODES];
	s8		m_nWheelInstances[NUM_VEH_CWHEELS_MAX][2];	//[][0] contains this wheels child index, [][1] contains child to render
	float	m_fWheelTyreRadius[NUM_VEH_CWHEELS_MAX];
	float	m_fWheelRimRadius[NUM_VEH_CWHEELS_MAX];
	float	m_fWheelScaleInv[NUM_VEH_CWHEELS_MAX];
	float	m_fWheelTyreWidth[NUM_VEH_CWHEELS_MAX];
	u16		m_isRearWheel : 10; // NUM_VEH_CWHEELS_MAX == 10
	u16		m_numSkinnedWheels : 4; // [0, NUM_VEH_CWHEELS_MAX]
	bool	m_bWheelInstanceSeparateRear : 1;

	CVehicleStructure();

#if !__SPU
	void* operator new(size_t RAGE_NEW_EXTRA_ARGS_UNUSED) {return m_pInfoPool->New();}
	void operator delete(void *pVoid) {m_pInfoPool->Delete((CVehicleStructure*)pVoid);}
	static fwPool<class CVehicleStructure>* m_pInfoPool;
#endif
} ;

class CVehicleModelInfoMetaData
{
public:


private:

	PAR_SIMPLE_PARSABLE;
};

// class encapsulating vehicle model info data allocated at run time
class CVehicleModelInfoData
{
friend class CVehicleModelInfo;

public:
	CVehicleModelInfoData()
	{
		m_SeatInfo.Reset();

		for (s32 i = 0; i < HEIGHT_MAP_SIZE; ++i)
			m_afHeightMap[i] = 0.f;

		for (s32 i = 0; i < MAX_VEHICLE_SEATS; ++i)
			m_iSeatFragChildren[i] = -1;		

		for (s32 i = 0; i < MAX_NUM_LIVERIES; ++i)
			m_liveries[i] = 0xFFFFFFFF;

		for (s32 i = 0; i < MAX_NUM_LIVERIES; ++i)
			m_liveries2[i] = 0xFFFFFFFF;

		m_liveryCount = 0;
		m_livery2Count = 0;

		m_pHDShaderEffectType = NULL;
		m_pStructure = NULL;

		m_iCoverPoints = 0;
		m_lastColUsed = 0;
		m_numVehicleModelRefs = 0;
		m_numVehicleModelRefsParked = 0;
		m_numVehicleModelRefsInReusePool = 0;
		m_numAvailableLODs = 0;

        m_dialsRenderTargetUniqueId = 0;
	}

	CVehicleStructure*	m_pStructure;

private:
	CModelSeatInfo		m_SeatInfo;

    atFinalHashString   m_dialsTextureHash;
    u32                 m_dialsRenderTargetUniqueId;
    grcTextureHandle    m_dialsTexture;

	enum { MAX_COVERPOINTS_PER_VEHICLE = 10 };
	CCoverPointInfo		m_aCoverPoints[MAX_COVERPOINTS_PER_VEHICLE];

	// The height of the vehicle at 6 points
	//	Height map		
	//					
	//	|-------|		
	//	| 0	  3	|		
	//	|		|		
	//	| 1	  4	|		
	//	|		|		
	//	| 2	  5	|		
	//	|-------|		
	//
	enum { HEIGHT_MAP_SIZE = 6 };
	float				m_afHeightMap[HEIGHT_MAP_SIZE];

	s16					m_iSeatFragChildren[MAX_VEHICLE_SEATS];

	u32					m_liveries[MAX_NUM_LIVERIES];
	u32					m_liveries2[MAX_NUM_LIVERIES];

	atArray<Vec3V>		m_WheelOffsets;
	atArray<u16>		m_vehicleInstances; // NOTE: vehicle pool size cannot be larger than a u16

	CCustomShaderEffectVehicleType*	m_pHDShaderEffectType;

	s32					m_NumTimesUsed;
	s32					m_iCoverPoints;

	u32					m_lastColUsed;

	s16					m_numVehicleModelRefs;
	s16					m_numVehicleModelRefsParked;
	s16					m_numVehicleModelRefsInReusePool;

	s8					m_liveryCount;
	s8					m_livery2Count;

	u8					m_numAvailableLODs;
};

////////////////////////////////////////////////////////////////////////////////
// CDriverInfo
////////////////////////////////////////////////////////////////////////////////

class CDriverInfo
{
public:

	inline atHashString GetDriverName() const { return m_driverName; }
	inline atHashString GetNPCName() const { return m_npcName; }

private:

	atHashString m_driverName;
	atHashString m_npcName;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CDoorStiffnessInfo
////////////////////////////////////////////////////////////////////////////////

class CDoorStiffnessInfo
{
public:

	inline eDoorId GetDoorId() const { return m_doorId; }
	inline float GetStiffnessMult() const { return m_stiffnessMult; }

private:

	eDoorId m_doorId;
	float m_stiffnessMult;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAdditionalVfxWaterSample
////////////////////////////////////////////////////////////////////////////////

class CAdditionalVfxWaterSample
{
public:

	inline const Vector3& GetPosition() const { return m_position; }
	inline float GetSize() const { return m_size; }
	inline int GetComponent() const { return m_component; }

private:

	Vector3 m_position;
	float m_size;
	int m_component;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CVfxExtraInfo
////////////////////////////////////////////////////////////////////////////////

class CVfxExtraInfo
{
public:

	inline u32 GetPtFxExtras() const { return m_ptFxExtras; }
	inline atHashString GetPtFxName() const { return m_ptFxName; }
	inline Vec3V_Out GetPtFxOffset() const { return m_ptFxOffset; }
	inline float GetPtFxRangeSqr() const { return m_ptFxRange*m_ptFxRange; }
	inline float GetPtFxSpeedEvoMin() const { return m_ptFxSpeedEvoMin; }
	inline float GetPtFxSpeedEvoMax() const { return m_ptFxSpeedEvoMax; }

private:

	u32 m_ptFxExtras;
	atHashString m_ptFxName;
	Vec3V m_ptFxOffset;
	float m_ptFxRange;
	float m_ptFxSpeedEvoMin;
	float m_ptFxSpeedEvoMax;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CMobilePhoneSeatIKOffset
////////////////////////////////////////////////////////////////////////////////

class CMobilePhoneSeatIKOffset
{
public:

	u32 GetSeatIndex() const { return m_SeatIndex; }
	const Vector3 &GetOffset() const { return m_Offset; }

private:

	Vector3 m_Offset;
	u8		m_SeatIndex;

	PAR_SIMPLE_PARSABLE;
};

//
// 
//
//
class CVehicleModelInfo : public CBaseModelInfo
{
	friend class CVehicle;

	enum {
		FRONT_LEFT_LIGHT=0,
		FRONT_RIGHT_LIGHT,
		REAR_LEFT_LIGHT,
		REAR_RIGHT_LIGHT
	};

	enum eExtraIncludes
	{
		EXTRA_1 = 1,	// starting at 1 because for some reason the extras bitmask in Vehicle.cpp doesn't use bit 0
		EXTRA_2,
		EXTRA_3,
		EXTRA_4,
		EXTRA_5,
		EXTRA_6,
		EXTRA_7,
		EXTRA_8,
		EXTRA_9,
		EXTRA_10,
	};

public:
	static const s32 MAX_VEHICLE_INFO_FLAGS = CVehicleModelInfoFlags::Flags_NUM_ENUMS;

	struct CVehicleOverrideRagdollThreshold
	{
		s32 m_MinComponent;
		s32 m_MaxComponent;
		float m_ThresholdMult;

		PAR_SIMPLE_PARSABLE;
	};

	// Intermediate struct used when constructing this class from xml (ie vehicle.meta)
	struct InitData {
		InitData();

		ConstString m_modelName; 
		ConstString m_txdName;
		ConstString m_handlingId; 
		ConstString m_gameName;
		ConstString m_vehicleMakeName;
		ConstString m_expressionDictName;
		ConstString m_expressionName;
		ConstString m_animConvRoofDictName;
		ConstString m_animConvRoofName;
		ConstString m_ptfxAssetName;
		
		atHashString m_audioNameHash;
		atHashString m_layout;
		atHashString m_POVTuningInfo;
		atHashString m_coverBoundOffsets;
		atHashString m_explosionInfo;
		atHashString m_scenarioLayout;
		atHashString m_cameraName;
		atHashString m_aimCameraName;
		atHashString m_bonnetCameraName;
		atHashString m_povCameraName;
		atHashString m_povTurretCameraName;
		atHashString m_vfxInfoName;
		atArray<atHashString> m_firstPersonDrivebyData;
		Vector3 m_FirstPersonDriveByIKOffset;
		Vector3	m_FirstPersonDriveByUnarmedIKOffset;
		Vector3	m_FirstPersonDriveByLeftPassengerIKOffset;
		Vector3	m_FirstPersonDriveByRightPassengerIKOffset;
		Vector3	m_FirstPersonDriveByRightRearPassengerIKOffset;
		Vector3	m_FirstPersonDriveByLeftPassengerUnarmedIKOffset;
		Vector3	m_FirstPersonDriveByRightPassengerUnarmedIKOffset;
		Vector3 m_FirstPersonProjectileDriveByIKOffset;
		Vector3 m_FirstPersonProjectileDriveByPassengerIKOffset;
		Vector3 m_FirstPersonProjectileDriveByRearLeftIKOffset;
		Vector3 m_FirstPersonProjectileDriveByRearRightIKOffset;
		Vector3 m_FirstPersonVisorSwitchIKOffset;
		Vector3 m_FirstPersonMobilePhoneOffset;
		Vector3 m_FirstPersonPassengerMobilePhoneOffset;
		atArray<CMobilePhoneSeatIKOffset> m_FirstPersonMobilePhoneSeatIKOffset;
		Vector3 m_PovCameraOffset;
		Vector3 m_PovPassengerCameraOffset;
		Vector3 m_PovRearPassengerCameraOffset;
		float m_PovCameraVerticalAdjustmentForRollCage;
		bool m_shouldUseCinematicViewMode;
		bool m_shouldCameraTransitionOnClimbUpDown;
		bool m_shouldCameraIgnoreExiting;
		bool m_AllowPretendOccupants;
		bool m_AllowJoyriding;
		bool m_AllowSundayDriving;
		bool m_AllowBodyColorMapping;

		float m_wheelScale;
		float m_wheelScaleRear;
		float m_dirtLevelMin;
		float m_dirtLevelMax;
		float m_envEffScaleMin;
		float m_envEffScaleMax;
		float m_envEffScaleMin2;
		float m_envEffScaleMax2;
		float m_damageMapScale;
		float m_damageOffsetScale; // How much to reduce collision deformation, must match the artist created deformation reduction texture.
		Color32	m_diffuseTint;
		float m_steerWheelMult;
		float m_firstPersonSteerWheelMult;
		float m_HDTextureDist;
		float m_lodDistances[VLT_MAX];

		u16 m_frequency;
		eSwankness m_swankness;

		u16 m_maxNum;

		VehicleModelInfoFlags m_flags;

		VehicleType m_type;

		eVehiclePlateType m_plateType;
        eVehicleDashboardType m_dashboardType;

        eVehicleClass m_vehicleClass;

		atArray<atHashString> m_trailers;
		atArray<atHashString> m_additionalTrailers;

		atArray<CDriverInfo> m_drivers;

		atArray<u32> m_extraIncludes;
		atArray<CVfxExtraInfo> m_vfxExtraInfos;
		u32 m_requiredExtras;

		eVehicleWheelType m_wheelType;

		atArray<eDoorId> m_doorsWithCollisionWhenClosed;

		atArray<eDoorId> m_driveableDoors;

		atArray<eWindowId> m_animConvRoofWindowsAffected;

		atArray<CDoorStiffnessInfo> m_doorStiffnessMultipliers;

		u8 m_identicalModelSpawnDistance;
		u8 m_maxNumOfSameColor;
		float m_defaultBodyHealth;

		float m_pretendOccupantsScale;
		float m_visibleSpawnDistScale;

		atArray<atHashString> m_rewards;
		atArray<atHashString> m_cinematicPartCamera;

		atHashString m_NmBraceOverrideSet;

		Vector3 m_buoyancySphereOffset;
		float m_buoyancySphereSizeScale;

		atArray<CAdditionalVfxWaterSample> m_additionalVfxWaterSamples;

		float m_trackerPathWidth;

		float m_weaponForceMult;

		bool m_bumpersNeedToCollideWithMap;
		bool m_needsRopeTexture;

		CVehicleOverrideRagdollThreshold* m_pOverrideRagdollThreshold;

		float m_maxSteeringWheelAngle;
		float m_maxSteeringWheelAnimAngle;

		float m_minSeatHeight;

		Vector3 m_lockOnPositionOffset;

		float m_LowriderArmWindowHeight;
		float m_LowriderLeanAccelModifier;

		s32 m_numSeatsOverride;

		PAR_SIMPLE_PARSABLE;
	};
	struct InitDataList {
		atArray<InitData> m_InitDatas;
		atString m_residentTxd;
		atArray<atString> m_residentAnims;
		atArray <CTxdRelationship> m_txdRelationships;
		PAR_SIMPLE_PARSABLE;
	};


	CVehicleModelInfo() {}
	
	virtual void Init();
	virtual void Shutdown();
	virtual void ShutdownExtra() { }

	virtual void InitMasterDrawableData(u32 modelIdx);
	virtual void DeleteMasterDrawableData();

	virtual void InitMasterFragData(u32 modelIdx);
	virtual void DeleteMasterFragData();
	virtual void SetPhysics(phArchetypeDamp* pPhysics);

	virtual void DestroyCustomShaderEffects();

	void Init(const InitData& rInitData);
	void ReInit(const InitData& rInitData);

	// Accessor function for the Seat info
	const CModelSeatInfo* GetModelSeatInfo() const { modelinfoAssert(m_data); return &m_data->m_SeatInfo; }
	
	// set type of object e.g. car, boat etc
	void		SetGameName(const char* pName) {formatf(m_gameName,sizeof(m_gameName),"%s",pName);}
	const char* GetGameName() const {return m_gameName;}
	void		SetVehicleMakeName(const char* pName) {formatf(m_vehicleMakeName,sizeof(m_vehicleMakeName),"%s",pName);}
	const char* GetVehicleMakeName() const {return m_vehicleMakeName;}
	
	CVehicleStructure *GetStructure() const { modelinfoAssert(m_data); return m_data->m_pStructure;}

	void	SetBoneIndexes(ObjectNameIdAssociation *pAssocArray, u16* pNameHashes, bool bOverRide);
	s32		GetBoneIndex(s32 component) const { modelinfoAssert(m_data); return m_data->m_pStructure->m_nBoneIndices[component];}
	void	InitVehData(s32 modelIdx);
	void	InitPhys();

	void	SetDecalBoneFlags();
	bool	GetDecalBoneFlag(s32 boneIdx) const { modelinfoAssert(m_data); return m_data->m_pStructure->m_decalBoneFlags.IsSet(boneIdx);}

	virtual void InitWaterSamples();

	// Safely attempt to set the mass/angular inertia of a group attached to the given bone index
	// includeSubTree will make sure all the child groups are included in the calculation
	void	SetBonesGroupMassAndAngularInertia(s32 boneIndex, float mass, const Vector3& angularInertia, fragGroupBits& tunedGroups, bool includeSubTree = false);
	// Same as above, but mass input only
	// Angular inertia will be scaled by the mass change but will retain it's shape
	// Currently does not support scaling subtrees proportionally for you, but that can be added if there is need
	void	SetBonesGroupMass(s32 boneIndex, float mass, fragGroupBits& tunedGroups);
	void	SetGroupMass(s32 groupIndex, float mass, fragGroupBits& tunedGroups);
	void	SetJointStiffness(s32 boneIndex, float fStiffness, fragGroupBits& tunedGroups);

	void	InitFragType(CHandlingData* pHandling);
	void	InitFromFragType();
	void	CalculateMaxFlatVelocityEstimate(CHandlingData *pHandling);
	void	CalculateHeightsAboveRoad(float *pFrontHeight, float *pRearHeight);
	s32		GetGangPopGroup() const { return m_gangPopGroup; }
	void	SetGangPopGroup(s32 value) { m_gangPopGroup = value; }

	void		SetVehicleType(VehicleType type) {m_vehicleType = type;}
	VehicleType GetVehicleType() const {return m_vehicleType;}

	eVehiclePlateType GetVehiclePlateType() const { return (eVehiclePlateType)m_plateType; }
	eVehicleClass GetVehicleClass() const { return (eVehicleClass)m_vehicleClass; }
    eVehicleDashboardType GetVehicleDashboardType() const { return (eVehicleDashboardType)m_dashboardType; }

	// Some useful queries for casting
	// Lets you easily figure out what the base class might be to avoid long 'if' statements
	bool	GetIsRotaryAircraft() const { return (GetVehicleType() >= VEHICLE_TYPE_HELI && GetVehicleType() <= VEHICLE_TYPE_AUTOGYRO);}
	
	// Remember aircraft inherit from CAutomobile!
	bool	GetIsAutomobile() const {return (GetVehicleType() >= VEHICLE_TYPE_CAR && GetVehicleType() <= VEHICLE_TYPE_AUTOGYRO);}
	bool	GetIsBoat() const { return (GetVehicleType() == VEHICLE_TYPE_BOAT); }
	bool	GetIsBike() const { return (GetVehicleType() >= VEHICLE_TYPE_BIKE && GetVehicleType() <= VEHICLE_TYPE_BICYCLE); }
	bool	GetIsBicycle() const { return (GetVehicleType() == VEHICLE_TYPE_BICYCLE); }
	bool	GetIsTrain() const { return (GetVehicleType() == VEHICLE_TYPE_TRAIN); }
	bool	GetIsSubmarine() const { return (GetVehicleType() == VEHICLE_TYPE_SUBMARINE); }
	bool	GetIsPlane() const {return (GetVehicleType() == VEHICLE_TYPE_PLANE);}
	bool	GetIsTrailer() const { return GetVehicleType() == VEHICLE_TYPE_TRAILER; }
	bool	GetIsQuadBike() const { return GetVehicleType() == VEHICLE_TYPE_QUADBIKE; }
	bool	GetIsDraftVeh() const { return GetVehicleType() == VEHICLE_TYPE_DRAFT; }
	bool	GetIsSubmarineCar() const { return GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR; }
	bool	GetIsAmphibiousVehicle() const { return GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE; }
	bool	GetIsAmphibiousQuad() const { return GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE; }
	bool	GetIsHeli() const { return GetVehicleType() == VEHICLE_TYPE_HELI; }
	bool	GetIsJetSki() const { return GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_JETSKI); }

	void	SetWheels(float scale, float scaleRear) { m_tyreRadiusF = m_rimRadiusF = scale; m_tyreRadiusR = m_rimRadiusR = scaleRear;}
	float	GetTyreRadius(bool bFrontWheel=true) {return (bFrontWheel ? m_tyreRadiusF : m_tyreRadiusR);}
	float	GetRimRadius(bool bFrontWheel=true) {return (bFrontWheel ? m_rimRadiusF : m_rimRadiusR);}

	void	GetWheelPosn(s32 num, Vector3& posn, bool bSkipTransforms=false);
	bool	GetOriginalCompPosition(Vector3& posn, s32 nId);
	void	SetHandlingId(s32 id) {m_handlingId = id;}
	s32		GetHandlingId() const {return m_handlingId;}
#if __BANK
	void	SetHandlingIdHash(atHashString id) {m_handlingIdHash = id;}
	atHashString	GetHandlingIdHash() const {return m_handlingIdHash;}
#endif // __BANK
	void	SetVehicleFreq(s32 freq) {m_freq = freq;}
	s32		GetVehicleFreq() {return m_freq;}
	void	SetVehicleMaxNumber(u16 maxNumber)	{m_maxNumber = maxNumber;}
	u16		GetVehicleMaxNumber()				{return static_cast<u16>(m_maxNumber * ms_maxNumberMultiplier);}
#if __BANK
	void	SetVehicleDLCPack(const char* fileName);
	atHashString GetVehicleDLCPack() const {return m_dlcpack;}
#endif //__BANK
	void	SetVehicleSwankness(eSwankness swankness) {m_swankness = swankness;}
	eSwankness	GetVehicleSwankness() const {return m_swankness;}
	float	GetSteeringWheelMult(const CPed* pDriver = NULL);
	void	SetSteeringWheelMult(float fSteeringWheelMult) { m_fSteeringWheelMult = fSteeringWheelMult; }
	float	GetFirstPersonSteeringWheelMult() const { return m_fFirstPersonSteeringWheelMult; }
	void	SetFirstPersonSteeringWheelMult(float fFirstPersonSteeringWheelMult) { m_fFirstPersonSteeringWheelMult = fFirstPersonSteeringWheelMult; }

#if !__SPU
	Vec3V_Out CalculateWheelOffset(eHierarchyId wheelId) const;
	void	CacheWheelOffsets();
#endif // !__SPU

	// flags
	bool	GetVehicleFlag(CVehicleModelInfoFlags::Flags flag) const { return m_VehicleFlags.BitSet().IsSet(flag); }

	bool	GetCanPassengersBeKnockedOff();
	static const bool AllowsVehicleEntryWhenStoodOn(const CVehicle& veh);

	// valid range: <0; 15>
	u32		GetDefaultDirtLevelMin()						{ return(u32(m_defaultDirtLevelMin));		}
	u32		GetDefaultDirtLevelMax()						{ return(u32(m_defaultDirtLevelMax));		}
	void	SetDefaultDirtLevel(u32 dirtMin, u32 dirtMax)	{ this->m_defaultDirtLevelMin=u8(dirtMin&0x0f); this->m_defaultDirtLevelMax=u8(dirtMax&0x0f);}

	// damageMap scale: accessed as float <0;1>, stored as byte
	float	GetDamageMapScale()								{ return float(this->m_damageMapScale)/255.0f;				}
	void	SetDamageMapScale(float ds)						{ u32 d=u32(ds*255.0f); this->m_damageMapScale=(u8)MIN(d,255);	}

	// damageOffset scale: accessed as float <0;1>, stored as byte
	float	GetDamageOffsetScale()								{ return float(this->m_damageOffsetScale)/255.0f;				}
	void	SetDamageOffsetScale(float ds)						{ u32 d=u32(ds*255.0f); this->m_damageOffsetScale=(u8)MIN(d,255);	}
		
	// envEff min/max scale: accessed as float <0;1>, stored as byte
	float	GetEnvEffScaleMin()								{ return float(this->m_envEffScaleMin)/255.0f;				}
	float	GetEnvEffScaleMax()								{ return float(this->m_envEffScaleMax)/255.0f;				}
	void	SetEnvEffScale(float efmin, float efmax)		{ u32 iefmin=u32(efmin*255.0f); this->m_envEffScaleMin=(u8)MIN(iefmin,255);	u32 iefmax=u32(efmax*255.0f); this->m_envEffScaleMax=(u8)MIN(iefmax,255);}

	float	GetEnvEffScaleMin2()							{ return float(this->m_envEffScaleMin2)/255.0f;				}
	float	GetEnvEffScaleMax2()							{ return float(this->m_envEffScaleMax2)/255.0f;				}
	void	SetEnvEffScale2(float efmin, float efmax)		{ u32 iefmin=u32(efmin*255.0f); this->m_envEffScaleMin2=(u8)MIN(iefmin,255);	u32 iefmax=u32(efmax*255.0f); this->m_envEffScaleMax2=(u8)MIN(iefmax,255);}

	Color32	GetDiffuseTint()								{ return this->m_diffuseTint;		}
	void	SetDiffuseTint(Color32 tint)					{ this->m_diffuseTint=tint;			}

	float	GetVehicleLodDistance(eVehicleLodType lod) const{ return m_lodDistances[lod]*GetLodMultiplierBiasAsDistanceScalar(); }
	u32		GetNumAvailableLODs()							{ modelinfoAssert(m_data); return m_data->m_numAvailableLODs; }
	float	GetDefaultBodyHealth()							{ return m_defaultBodyHealth; }

	u32		GetCameraNameHash() const {return m_uCameraNameHash;}
	void	SetCameraNameHash(u32 uHash) { m_uCameraNameHash = uHash; }
	u32		GetAimCameraNameHash() const {return m_uAimCameraNameHash;}
	void	SetAimCameraNameHash(u32 uHash) { m_uAimCameraNameHash = uHash; }
	u32		GetBonnetCameraNameHash() const {return m_uBonnetCameraNameHash;}
	void	SetBonnetCameraNameHash(u32 uHash) { m_uBonnetCameraNameHash = uHash; }
	u32		GetPovTurretCameraNameHash() const {return m_uPovTurretCameraNameHash;}
	void	SetPovTurretCameraNameHash(u32 uHash) { m_uPovTurretCameraNameHash = uHash; }

	const Vector3& GetFirstPersonDriveByIKOffset() const { return m_FirstPersonDriveByIKOffset; }
	const Vector3& GetFirstPersonDriveByUnarmedIKOffset() const { return m_FirstPersonDriveByUnarmedIKOffset; }
	const Vector3& GetFirstPersonDriveByLeftPassengerIKOffset() const { return m_FirstPersonDriveByLeftPassengerIKOffset; }
	const Vector3& GetFirstPersonDriveByRightPassengerIKOffset() const { return m_FirstPersonDriveByRightPassengerIKOffset; }
	const Vector3& GetFirstPersonDriveByRightRearPassengerIKOffset() const { return m_FirstPersonDriveByRightRearPassengerIKOffset; }
	const Vector3& GetFirstPersonDriveByLeftPassengerUnarmedIKOffset() const { return m_FirstPersonDriveByLeftPassengerUnarmedIKOffset; }
	const Vector3& GetFirstPersonDriveByRightPassengerUnarmedIKOffset() const { return m_FirstPersonDriveByRightPassengerUnarmedIKOffset; }
	const Vector3& GetFirstPersonProjectileDriveByIKOffset() const { return m_FirstPersonProjectileDriveByIKOffset; }
	const Vector3& GetFirstPersonProjectileDriveByPassengerIKOffset() const { return m_FirstPersonProjectileDriveByPassengerIKOffset; }
	const Vector3& GetFirstPersonProjectileDriveByRearLeftIKOffset() const { return m_FirstPersonProjectileDriveByRearLeftIKOffset; }
	const Vector3& GetFirstPersonProjectileDriveByRearRightIKOffset() const { return m_FirstPersonProjectileDriveByRearRightIKOffset; }
	const Vector3& GetFirstPersonVisorSwitchIKOffset() const { return m_FirstPersonVisorSwitchIKOffset; }
	void	SetFirstPersonDriveByIKOffset(const Vector3& offset) { m_FirstPersonDriveByIKOffset = offset; }
	void	SetFirstPersonDriveByUnarmedIKOffset(const Vector3& offset) { m_FirstPersonDriveByUnarmedIKOffset = offset; }
	void	SetFirstPersonDriveByLeftPassengerIKOffset(const Vector3& offset) { m_FirstPersonDriveByLeftPassengerIKOffset = offset; }
	void	SetFirstPersonDriveByRightPassengerIKOffset(const Vector3& offset) { m_FirstPersonDriveByRightPassengerIKOffset = offset; }
	void	SetFirstPersonDriveByRightRearPassengerIKOffset(const Vector3& offset) { m_FirstPersonDriveByRightRearPassengerIKOffset = offset; }
	void	SetFirstPersonDriveByLeftPassengerUnarmedIKOffset(const Vector3& offset) { m_FirstPersonDriveByLeftPassengerUnarmedIKOffset = offset; }
	void	SetFirstPersonDriveByRightPassengerUnarmedIKOffset(const Vector3& offset) { m_FirstPersonDriveByRightPassengerUnarmedIKOffset = offset; }
	void	SetFirstPersonProjectileDriveByIKOffset(const Vector3& offset) { m_FirstPersonProjectileDriveByIKOffset = offset; }
	void	SetFirstPersonProjectileDriveByPassengerIKOffset(const Vector3& offset) { m_FirstPersonProjectileDriveByPassengerIKOffset = offset; }
	void	SetFirstPersonProjectileDriveByRearLeftIKOffset(const Vector3& offset) { m_FirstPersonProjectileDriveByRearLeftIKOffset = offset; }
	void	SetFirstPersonProjectileDriveByRearRightIKOffset(const Vector3& offset) { m_FirstPersonProjectileDriveByRearRightIKOffset = offset; }
	void	SetFirstPersonVisorSwitchIKOffset(const Vector3& offset) { m_FirstPersonVisorSwitchIKOffset = offset; }
	const Vector3& GetFirstPersonMobilePhoneOffset() const { return m_FirstPersonMobilePhoneOffset; }
	const Vector3& GetFirstPersonPassengerMobilePhoneOffset() const { return m_FirstPersonPassengerMobilePhoneOffset; }
	const Vector3& GetPassengerMobilePhoneIKOffset(u32 iSeat);
	void	SetFirstPersonMobilePhoneOffset(const Vector3& offset) { m_FirstPersonMobilePhoneOffset = offset; }
	void	SetFirstPersonPassengerMobilePhoneOffset(const Vector3& offset) { m_FirstPersonPassengerMobilePhoneOffset = offset; }
	u32		GetPovCameraNameHash() const { return m_uPovCameraNameHash; }
	void	SetPovCameraNameHash(u32 uHash) { m_uPovCameraNameHash = uHash; }
	const Vector3& GetPovCameraOffset() const { return m_PovCameraOffset; }
	void	SetPovCameraOffset(const Vector3& offset) { m_PovCameraOffset = offset; }
	float	GetPovCameraVerticalAdjustmentForRollCage() const { return m_PovCameraVerticalAdjustmentForRollCage; }
	void	SetPovCameraVerticalAdjustmentForRollCage(float offset) { m_PovCameraVerticalAdjustmentForRollCage = offset; }
	const Vector3& GetPovPassengerCameraOffset() const { return m_PovPassengerCameraOffset; }
	void	SetPovPassengerCameraOffset(const Vector3& offset) { m_PovPassengerCameraOffset = offset; }
	const Vector3& GetPovRearPassengerCameraOffset() const { return m_PovRearPassengerCameraOffset; }
	void	SetPovRearPassengerCameraOffset(const Vector3& offset) { m_PovRearPassengerCameraOffset = offset; }
	u32		GetAudioNameHash() const {return m_audioNameHash;}
	void	SetAudioNameHash(u32 uHash) { m_audioNameHash = uHash; }
	bool	ShouldUseCinematicViewMode() const {return m_shouldUseCinematicViewMode;}
	bool	ShouldCameraTransitionOnClimbUpDown() const {return m_bShouldCameraTransitionOnClimbUpDown;}
	bool	ShouldCameraIgnoreExiting() const {return m_bShouldCameraIgnoreExiting;}
	bool	GetAllowPretendOccupants() const {return m_bAllowPretendOccupants;}
	bool	GetAllowJoyriding() const {return m_bAllowJoyriding;}
	bool	GetAllowSundayDriving() const {return m_bAllowSundayDriving;}
	bool	GetAllowBodyColorMapping() const	{ return m_bAllowBodyColorMapping; }
	bool	GetHasSeatCollision() const { return m_bHasSeatCollision; }
	bool	GetHasSteeringWheelBone() const { return m_bHasSteeringWheelBone; }

	float	GetPretendOccupantsScale() const { return m_pretendOccupantsScale; }
	float	GetVisibleSpawnDistScale() const { return m_visibleSpawnDistScale; }

	u32		GetConvertibleRoofAnimNameHash() const {return m_uConvertibleRoofAnimHash;}
	void	SetConvertibleRoofAnimNameHash(u32 uHash) { m_uConvertibleRoofAnimHash = uHash; }

	s32		GetNumPossibleColours() const { return m_numPossibleColours;}
	void	SetNumPossibleColours(s32 num) { m_numPossibleColours = num;}
	void	UpdateCarColourTexture(u8 r, u8 g, u8 b, s32 index = 0);
	u8		GetPossibleColours(u32 col, u32 index) const		{ TrapGE(col,u32(NUM_VEH_BASE_COLOURS)); TrapGE(index,u32(MAX_VEH_POSSIBLE_COLOURS));	return m_possibleColours[col][index];}
	void	SetPossibleColours(u32 col, u32 index, s32 value)	{ TrapGE(col,u32(NUM_VEH_BASE_COLOURS)); TrapGE(index,u32(MAX_VEH_POSSIBLE_COLOURS));	m_possibleColours[col][index] = static_cast<u8>(value);}
	u32		GetLastColUsed() { modelinfoAssert(m_data); return m_data->m_lastColUsed; }
	u8		GetLiveryColor(s32 livery, s32 color, s32 index) const;
	u8		GetLiveryColorIndex(s32 livery, s32 color) const;

	void	UpdateModelColors(const CVehicleVariationData& varData, const int maxNumCols);
	void	UpdateModKits(const CVehicleVariationData& varData);
	void	UpdateWindowsWithExposedEdges(const CVehicleVariationData& varData);
	void	UpdatePlateProbabilities(const CVehicleVariationData& varData);
	void	GenerateVariation(CVehicle* pVehicle, CVehicleVariationInstance& variation, bool bFullyRandom = false);


//#if ENABLE_VEHICLECOLOURTEXT
//	//void SetColourText(char *str, u8 index) { strcpy(ms_vehicleColourText[index], str);}
//	static char	*GetVehicleColourText(s32 col) {return ms_VehicleColours->m_Colors[col].m_colorName;}
//#endif	
	void	ChooseVehicleColour(u8& col1, u8& col2, u8& col3, u8& col4, u8& col5, u8& col6, s32 inc = 1);
	void	GetNextVehicleColour(CVehicle *pVehicle, u8& col1, u8& col2, u8& col3, u8& col4, u8& col5, u8& col6);
	void	ChooseVehicleColourFancy(CVehicle *pNewVehicle, u8& col1, u8& col2, u8& col3, u8& col4, u8& col5, u8& col6);
	int		SelectLicensePlateTextureIndex() const;
	const vehicleLightSettings *GetLightSettings() const;
	const sirenSettings *GetSirenSettings() const;
	bool HasSirenSettings() const { return m_sirenSettings > 0; }
	u8		GetLightSettingsId() const { return m_lightSettings; }
	u8		GetSirenSettingsId() const { return m_sirenSettings; }

	inline void	AddVehicleModelRef() { modelinfoAssert(m_data); m_data->m_numVehicleModelRefs++; }
	inline void	RemoveVehicleModelRef() { modelinfoAssert(m_data); m_data->m_numVehicleModelRefs--; modelinfoAssert(m_data->m_numVehicleModelRefs>=0); }
	inline s32	GetNumVehicleModelRefs() const { return m_data ? (s32)m_data->m_numVehicleModelRefs : 0; }

	inline void	AddVehicleModelRefParked() { modelinfoAssert(m_data); m_data->m_numVehicleModelRefsParked++; }
	inline void	RemoveVehicleModelRefParked() { modelinfoAssert(m_data); m_data->m_numVehicleModelRefsParked--; modelinfoAssert(m_data->m_numVehicleModelRefsParked>=0); }
	inline s32	GetNumVehicleModelRefsParked() const { return m_data ? (s32)m_data->m_numVehicleModelRefsParked : 0; }

	inline void	AddVehicleModelRefReusePool() { modelinfoAssert(m_data); m_data->m_numVehicleModelRefsInReusePool++;}
	inline void	RemoveVehicleModelRefReusePool() { modelinfoAssert(m_data); m_data->m_numVehicleModelRefsInReusePool--; modelinfoAssert(m_data->m_numVehicleModelRefsInReusePool >= 0);}
	inline s32	GetNumVehicleModelRefsReusePool() const {return m_data ? (s32)m_data->m_numVehicleModelRefsInReusePool : 0;}

	// Fills out iIndicies with streaming indicies of any dependencies
	// And returns the number of dependencies.
	int		GetStreamingDependencies(strIndex* iIndicies, int iMaxNumIndicies);

	// Load texture callback
	static void UseCommonVehicleTexDicationary();
	static void StopUsingCommonVehicleTexDicationary();

	s32		GetNumTimesUsed()		{ modelinfoAssert(m_data); return m_data->m_NumTimesUsed; }
	void	ResetNumTimesUsed()	{ modelinfoAssert(m_data); m_data->m_NumTimesUsed = 0; }
	void	IncreaseNumTimesUsed()	{ modelinfoAssert(m_data); m_data->m_NumTimesUsed = MIN(m_data->m_NumTimesUsed+1, 120); }

	u32		GetLastTimeUsed()	{ return m_LastTimeUsed; }
	void	SetLastTimeUsed(u32 LastUsed)	{ m_LastTimeUsed = LastUsed; }

	atHashString GetNmBraceOverrideSet() { return m_NmBraceOverrideSet; }

	static void ConstructStaticData();
	static void DestroyStaticData();

	// Cover point query functions
	s32					GetNumCoverPoints( void ) { modelinfoAssert(m_data); return m_data->m_iCoverPoints; }
	CCoverPointInfo*	GetCoverPoint( s32 iCoverPoint );
	void				FindNextCoverPointOffsetInDirection( Vector3& vOut, const Vector3& vStart, const bool bClockwise ) const;	

	s32						GetEntryExitPointIndex(const CEntryExitPoint* pEntryExitPoint) const;

#if !__FINAL
	// Draws debug lines representing the position and height of the capsules
	void	DebugDrawCollisionModel( Matrix34& mMat );
#endif
	
	inline bool HasLightRemap(void) const { return m_bRemap; }
	inline void SetHasLightRemap(bool remap) { m_bRemap = remap; }

	inline bool HasTailLight(void) const { return m_bHasTailLight; }
	inline bool HasIndicatorLight(void) const { return m_bHasIndicatorLight; }
	inline bool HasBrakeLight(void) const { return m_bHasBrakeLight; }

	inline bool HasExtraLights(void) const { return m_bHasExtraLights; }
	inline bool HasNeons(void) const { return m_bHasNeons; }
	inline bool DoubleExtraLights(void) const { return m_bDoubleExtralights; }

	inline bool HasRequestedDrivers() const { return m_bHasRequestedDrivers; }
	inline void SetHasRequestedDrivers(bool val) { m_bHasRequestedDrivers = val; }

	inline float GetApproximateBonnetHeight() const { modelinfoAssert(m_data); return m_data->m_afHeightMap[0]; }

	inline float GetHeightMapHeight(int index) const { modelinfoAssert(m_data); return m_data->m_afHeightMap[index]; }

	inline s8 GetLiveriesCount() const			{ modelinfoAssert(m_data); return m_data->m_liveryCount; }
	inline u32 GetLiveryHash(int idx) const		{ modelinfoAssert(m_data); modelinfoAssert(idx<MAX_NUM_LIVERIES); modelinfoAssert(idx<m_data->m_liveryCount);  return m_data->m_liveries[idx]; }

	inline s8 GetLiveries2Count() const			{ modelinfoAssert(m_data); return m_data->m_livery2Count; }
	inline u32 GetLivery2Hash(int idx) const	{ modelinfoAssert(m_data); modelinfoAssert(idx<MAX_NUM_LIVERIES); modelinfoAssert(idx<m_data->m_livery2Count);  return m_data->m_liveries2[idx]; }

	float GetWheelBaseMultiplier() const { return m_wheelBaseMultiplier; }

	s32 GetFragChildForSeat(s32 seatIndex) const { modelinfoAssert(m_data); modelinfoAssertf(seatIndex >= 0 && seatIndex < MAX_VEHICLE_SEATS,"Invalid seat index"); return m_data->m_iSeatFragChildren[seatIndex]; }

#if __DEV
	void CheckMissingColour();
#endif

#if __BANK
	void CheckDependenciesAreLoaded();
	void DumpVehicleModelInfos();
	void DumpAverageVehicleSize();
	void DebugRecalculateVehicleCoverPoints();

	static atArray<sDebugSize> ms_vehicleSizes;
#endif

	// Layout and seats
	// IMPORTANT: A seat index is specific to a vehicle model and might not match the layout information's indicies
	// if the vehicle is missing seats.
	// Always use the getters inside CVehicleModelInfo instead of accessing the layout directly		

	const CVehicleSeatInfo* GetSeatInfo(int iSeatIndex) const;
	const CVehicleSeatAnimInfo* GetSeatAnimationInfo(int iSeatIndex) const;

	const CVehicleEntryPointInfo* GetEntryPointInfo(int iEntryPointIndex) const;
	const CVehicleEntryPointAnimInfo* GetEntryPointAnimInfo(int iEntryPointIndex) const;

	// Get seat and entry point indicies from the info
	s32 GetSeatIndex(const CVehicleSeatInfo* pSeatInfo) const;
	s32 GetEntryPointIndex(const CVehicleEntryPointInfo* pEntryPointInfo) const;

	// Always go through VehicleModelInfo functions when accessing doors / seats
	// Because a layout can have more seats / doors than a specific model
	const CVehicleLayoutInfo* GetVehicleLayoutInfo() const;
	const CPOVTuningInfo* GetPOVTuningInfo() const;
	const CVehicleCoverBoundOffsetInfo* GetVehicleCoverBoundOffsetInfo() const;
	const CFirstPersonDriveByLookAroundData* GetFirstPersonLookAroundData(s32 iSeat) const;

	const CVehicleExplosionInfo* GetVehicleExplosionInfo() const;

	///Scenario Layout accessors.
	const CVehicleScenarioLayoutInfo* GetVehicleScenarioLayoutInfo() const;
	void SetVehicleScenarioLayoutInfoHash(u32 uHash) {m_uVehicleScenarioLayoutInfoHash = uHash; }
	u32 GetVehicleScenarioLayoutInfoHash() const { return m_uVehicleScenarioLayoutInfoHash; }

	Vec3V_Out GetWheelOffset(eHierarchyId wheel) const;

	// PURPOSE: Add a reference to this archetype and add ref to any HD assets loaded
	virtual void AddHDRef(bool bRenderRef /* = false */);
	virtual void RemoveHDRef(bool bRenderRef /* = false */);
	inline u32 GetNumHdRefs()				{ return(m_numHDRefs + m_numHDRenderRefs); }

	// HD stuff
	// fragment + txd parent
	void SetupHDFiles(const char* pName);
	void ConfirmHDFiles(void);

	void AddToHDInstanceList(size_t id);
	void RemoveFromHDInstanceList(size_t id);

	static void UpdateHDRequests(void);

	inline	s32 GetHDFragmentIndex() const { return(m_HDfragIdx); }
	inline	s32 GetHDTxdIndex() const { return(m_HDtxdIdx); }
	bool	SetupHDCustomShaderEffects();

	void SetVehicleMetaDataFile(const char* pName);
	strLocalIndex GetVehicleMetaDataFileIndex() const { return strLocalIndex(-1); /*m_vehicleMetaDataFileIndex;*/ }
	const CVehicleModelInfoMetaData* GetVehicleMetaData() const;

    bool HasDials() const { return m_data && m_data->m_dialsTextureHash.GetHash() != 0; }
    void RequestDials(const sVehicleDashboardData& params);
	static void RenderDialsToRenderTarget(u32 targetId);
    static void ReleaseDials();
	static void ResetRadioInfo() { ms_lastTrackId = 0xffffffff; }

private:
	// HD stuff
	void RequestAndRefHDFiles(void);
	void ReleaseRequestOrRemoveRefHD(void);
	void ReleaseHDFiles(void);

	static void SetupHDSharedTxd(const char* pName);

public:
	gtaFragType* GetHDFragType() const;
		
	void	SetHDDist(float HDDist) { m_lodDistances[VLT_HD] = HDDist; }
	float	GetHDDist(void) { return(GetVehicleLodDistance(VLT_HD)); }
	bool	GetHasHDFiles(void) const { return (GetVehicleLodDistance(VLT_HD) > 0.0f); }
		
	CCustomShaderEffectVehicleType*	GetHDMasterCustomShaderEffect(void) { modelinfoAssert(m_data); return m_data->m_pHDShaderEffectType; }

	bool		GetRequestLoadHDFiles(void) const { return (m_bRequestLoadHDFiles); }
	void 		SetRequestLoadHDFiles(bool bSet) { m_bRequestLoadHDFiles = bSet; }
	bool		GetAreHDFilesLoaded(void) const { return (m_bAreHDFilesLoaded); }
	void		SetAreHDFilesLoaded(bool bSet) { m_bAreHDFilesLoaded = bSet; }

	fwModelId	GetRandomTrailer() const;
	fwModelId	GetRandomLoadedTrailer() const;
	u32			GetTrailerCount() const { return m_trailers.GetCount(); }
	fwModelId	GetTrailer(u32 idx, CVehicleModelInfo*& vmi) const;
	bool		GetIsCompatibleTrailer(CVehicleModelInfo * trailerModelInfo);

	u32			GetRandomDriver() const;
	u32			GetDriverCount() const { return m_drivers.GetCount(); }
	u32			GetDriver(u32 idx) const { fwModelId driverModelId; CModelInfo::GetBaseModelInfoFromName(m_drivers[idx].GetDriverName(), &driverModelId); return driverModelId.GetModelIndex(); }
	u32			GetDriverModelHash(u32 idx) const { return m_drivers[idx].GetDriverName().GetHash(); }
#if !__NO_OUTPUT
	const char*	TryGetDriverName(u32 idx) const { return m_drivers[idx].GetDriverName().TryGetCStr(); }
#endif // __NO_OUTPUT
	u32			GetDriverNpcHash(u32 idx) const { return m_drivers[idx].GetNPCName().GetHash(); }

	u32			GetNumExtraIncludeGroups() const { return m_extraIncludes.GetCount(); }
	u32			GetExtraIncludeGroup(u32 index) const { return m_extraIncludes[index]; }
	u32			GetRequiredExtras() const { return m_requiredExtras; }

	eVehicleWheelType GetWheelType() const { return m_wheelType; }

	// vfx
	CVfxVehicleInfo* GetVfxInfo() {return m_pVfxInfo;}
#if !__SPU
	void		SetVfxInfo(u32 vfxInfoHashName) {m_pVfxInfo = g_vfxVehicleInfoMgr.GetInfo(vfxInfoHashName);}
#if __DEV
	void		SetVfxInfo(CVfxVehicleInfo* pVfxVehicleInfo) {m_pVfxInfo = pVfxVehicleInfo;}
#endif
#endif

	u32			GetNumModKits() const { return m_modKits.GetCount(); }
	u16			GetModKit(u32 index) const { Assert(index < (unsigned)m_modKits.GetCount()); return m_modKits[index]; }
	float		GetHDTextureDistance() { return m_HDTextureDist; }

	bool		GetDoesWindowHaveExposedEdges(eHierarchyId id) const { if (id >= VEH_FIRST_WINDOW && id <= VEH_LAST_WINDOW) { return (m_windowsWithExposedEdges & BIT((int)id - VEH_FIRST_WINDOW)) != 0; } return false; }

	void		AddVehicleInstance(CVehicle* veh);
	void		RemoveVehicleInstance(CVehicle* veh);
	CVehicle*	GetVehicleInstance(u32 index);
	u32			GetNumVehicleInstances() const				{ modelinfoAssert(m_data); return m_data->m_vehicleInstances.GetCount(); }
	float		GetDistanceSqrToClosestInstance(const Vector3& pos, bool cargens);
	float		GetIdenticalModelSpawnDistance() const { return (float)m_identicalModelSpawnDistance; }

	bool		HasAnyMissionInstances();

	u32			GetMaxNumberOfSameColor() const { return (u32)m_maxNumOfSameColor; }
	bool		HasAnyColorsToSpawn() const;

	u32				GetNumRewards()					const	{ return m_rewards.GetCount(); }
	atHashString	GetRewardHash(u32 index)		const	{ Assert(index < (u32)(m_rewards.GetCount())); return m_rewards[index]; }

	bool		GetShouldTrackVehicle() const { return GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_TRACKED_FOR_TRAILS); }
	float		GetTrackerPathWidth() const { return m_trackerPathWidth; }

	float		GetWeaponForceMult() const { return m_weaponForceMult; }

	u32				GetNumCinematicPartCameras()	const		{ return m_cinematicPartCamera.GetCount(); }
	atHashString	GetCinematicPartCameraHash(u32 index) const	{ Assert(index < (u32)(m_cinematicPartCamera.GetCount())); return m_cinematicPartCamera[index]; }

	bool		NeedsRopeTexture() const { return m_bNeedsRopeTexture; }

	static void UpdateVehicleClassInfo(CVehicleModelInfo* pVehicleModelInfo);

	float		GetMaxSteeringWheelAngle() const { return m_maxSteeringWheelAngle; }
	float		GetMaxSteeringWheelAnimAngle() const { return m_maxSteeringWheelAnimAngle; }

#if __BANK
	void		SetMinSeatHeight(float fMinSeatHeight) { m_minSeatHeight = fMinSeatHeight; }
#endif
	float		GetMinSeatHeight() const { return m_minSeatHeight; }

	Vector3		GetLockOnPositionOffset() const { return m_lockOnPositionOffset; }

	float		GetLowriderArmWindowHeight() const { return m_LowriderArmWindowHeight; }
	float		GetLowriderLeanAccelModifier() const { return m_LowriderLeanAccelModifier; }

	s32			GetNumSeatsOverride() const { return m_numSeatsOverride; }

#if __BANK
	static void	DumpVehicleInstanceDataCB();
#endif

private:
	friend struct CVehicleOffsetInfo;

	void SetAtomicRenderCallbacks();
	void PreprocessHierarchy();
	void ReduceMaterialsInVehicle();

	void GetVehicleBBoxForBuoyancy(Vector3& vecBBoxMin, Vector3& vecBBoxMax);
	void GenerateWaterSamplesForVehicle();
	void GenerateWaterSamplesForBike();
	void GenerateWaterSamplesForPlane();
	void GenerateWaterSamplesForBoat();
	void GenerateWaterSamplesForSubmarine();
	void GenerateWaterSamplesForHeli();

	void GeneratePontoonSamples( CSeaPlaneHandlingData* pSeaPlaneHandling, u32 numPontoonSamples, u32 sampleStartIndex, u32 nTotalNumSamples );

	// Helper function to make sure any changes in the vehicle buoyancy code mean we have to also fix up the submarine buoyancy
	// constant code in TaskVehiclePlayer.
public:
	float ComputeBuoyancyConstantFromSubmergeValue(float fSubmergeValue, float fSizeTotal, bool bRecompute=false);

	CVehicleModelInfoData* m_data;

		fwPtrListSingleLink	m_HDInstanceList;// HD streaming

private:
	// Private function to check the height of the vehicle at a specific location
	void CalculateVehicleCoverPoints( void );
	void SizeCoverTestForVehicle(phInstGta* pTestInst, Vector3* aTestPositions, float& fCapsuleRadius, float& fCapsuleLength);
	void InitLayout( void );	

	void CalculateStreamingDependencies();

	//Size ... 56b
	strIndex	m_iDependencyStreamingIndicies[MAX_VEHICLE_STR_DEPENDENCIES];// Anim dependencies (calculated from layout)

	u8			m_possibleColours[NUM_VEH_BASE_COLOURS][MAX_VEH_POSSIBLE_COLOURS];

	//Size ... 32b
	s8			m_liveryColors[MAX_NUM_LIVERIES][MAX_NUM_LIVERY_COLORS]; // indexes into m_possibleColours
	
	float		m_lodDistances[VLT_MAX]; // distances for hd assets and various drawable lods

	//size ... 12b
	char		m_gameName[MAX_VEHICLE_GAME_NAME];
	char		m_vehicleMakeName[MAX_VEHICLE_GAME_NAME];

	//size ... 8b
	atArray<u16>	m_modKits;
	atArray<atHashString> m_rewards;
	atArray<atHashString> m_cinematicPartCamera;
	atArray<atHashString> m_trailers;			//trailers a truck can spawn with
	atArray<atHashString> m_additionalTrailers;	//additional trailers the player can attach to
	atArray<CDriverInfo> m_drivers;
	atArray<u32> m_extraIncludes;
	atArray<CVfxExtraInfo> m_vfxExtraInfos;
	CVehicleModelPlateProbabilities::array_type m_plateProbabilities;

	//size ... 4b
	VehicleType			m_vehicleType;
#if __BANK
	atHashString		m_dlcpack;
#endif //__BANK
	eSwankness			m_swankness;		// 0 = crap, 5 = super nice
	Color32				m_diffuseTint;
	eVehicleWheelType	m_wheelType;
	atHashString		m_NmBraceOverrideSet;

	Vector3		m_FirstPersonDriveByIKOffset;
	Vector3		m_FirstPersonDriveByUnarmedIKOffset;
	Vector3		m_FirstPersonDriveByLeftPassengerIKOffset;
	Vector3		m_FirstPersonDriveByRightPassengerIKOffset;
	Vector3		m_FirstPersonDriveByRightRearPassengerIKOffset;
	Vector3		m_FirstPersonDriveByLeftPassengerUnarmedIKOffset;
	Vector3		m_FirstPersonDriveByRightPassengerUnarmedIKOffset;
	Vector3		m_FirstPersonProjectileDriveByIKOffset;
	Vector3		m_FirstPersonProjectileDriveByPassengerIKOffset;
	Vector3		m_FirstPersonProjectileDriveByRearLeftIKOffset;
	Vector3		m_FirstPersonProjectileDriveByRearRightIKOffset;
	Vector3		m_FirstPersonVisorSwitchIKOffset;
	Vector3		m_FirstPersonMobilePhoneOffset;
	Vector3		m_FirstPersonPassengerMobilePhoneOffset;
	atArray<CMobilePhoneSeatIKOffset> m_aFirstPersonMobilePhoneSeatIKOffset;

	Vector3		m_PovCameraOffset;
	Vector3		m_PovPassengerCameraOffset;
	Vector3		m_PovRearPassengerCameraOffset;
	float		m_PovCameraVerticalAdjustmentForRollCage;

	float		m_tyreRadiusF;		// Front wheels
	float		m_tyreRadiusR;	// Rear wheels
	float		m_rimRadiusF;
	float		m_rimRadiusR;
	float		m_defaultBodyHealth;
	float		m_trackerPathWidth;
	float		m_fSteeringWheelMult;	// How much does the steering wheel rotate with the wheels
	float		m_fFirstPersonSteeringWheelMult;
	float		m_wheelBaseMultiplier;	// used by car ai to make large vehicles steer in early (between 0.0 and 1.0)
	float		m_HDTextureDist;// HD streaming
	float		m_pretendOccupantsScale;
	float		m_visibleSpawnDistScale;	// Used as a scaling factor for spawning, for large vehicles, etc
	float		m_weaponForceMult;

	s32			m_handlingId;
#if __BANK
	atHashString	m_handlingIdHash;
#endif // __BANK
	s32			m_freq;
	s32			m_gangPopGroup;
	s32			m_numPossibleColours;
	s32			m_HDfragIdx;// HD streaming
	s32			m_HDtxdIdx;// HD streaming
	//s32			m_vehicleMetaDataFileIndex;

	u32			m_uCameraNameHash;
	u32			m_uAimCameraNameHash;
	u32			m_uBonnetCameraNameHash;
	u32			m_uPovCameraNameHash;
	u32			m_uPovTurretCameraNameHash;
	u32			m_audioNameHash;
	u32         m_uConvertibleRoofAnimHash; //This is to be removed once the prefix of teh vehicle name has been removed frmo the front of the animation name
	u32			m_uVehicleScenarioLayoutInfoHash;
	u32			m_requiredExtras;
	u32			m_LastTimeUsed;

	CVfxVehicleInfo* m_pVfxInfo; // VFX info
	const CVehicleLayoutInfo* m_pVehicleLayoutInfo;
	const CPOVTuningInfo* m_pPOVTuningInfo;
	const CVehicleCoverBoundOffsetInfo*	m_pVehicleCoverBoundOffsetInfo;
	const CVehicleExplosionInfo* m_pVehicleExplosionInfo;
	atArray<const CFirstPersonDriveByLookAroundData *> m_apFirstPersonLookAroundData;

    eVehicleDashboardType m_dashboardType;

	//Size ... 2b
	u16			m_maxNumber;
	s16			m_numHDRefs;
	s16			m_numHDRenderRefs;

	//Size ... 1b
	u8			m_lightSettings;
	u8			m_sirenSettings;
	u8			m_envEffScaleMin;
	u8			m_envEffScaleMax;
	u8			m_envEffScaleMin2;
	u8			m_envEffScaleMax2;
	u8			m_defaultDirtLevelMin;
	u8			m_defaultDirtLevelMax;	// default dirt level for this modelinfo <0; 15>
	u8			m_damageMapScale;
	u8			m_damageOffsetScale;
	u8			m_uNumDependencies;// Anim dependencies (calculated from layout)
	u8			m_identicalModelSpawnDistance;
	u8			m_maxNumOfSameColor;
	u8			m_windowsWithExposedEdges; // first bit is VEH_FIRST_WINDOW

	//Size ... bits
    u8              m_vehicleClass                  :5;
	u8				m_plateType						:2;
	u8				m_bRequestLoadHDFiles			:1;
	u8				m_bAreHDFilesLoaded				:1;
	u8				m_bRequestHDFilesRemoval		:1;


	// Anim dependencies (calculated from layout)
	bool			m_bDependenciesValid			:1;

	//Seat information
	bool			m_bRemap						:1; // Lights Remap
	bool			m_bHasTailLight					:1;
	bool			m_bHasIndicatorLight			:1;
	bool			m_bHasBrakeLight				:1;
	bool			m_bHasExtraLights				:1;
	bool			m_bHasNeons						:1;
	bool			m_bDoubleExtralights			:1;

	bool			m_bAllowPretendOccupants		:1;
	bool			m_shouldUseCinematicViewMode	:1; // 1 BYTE
	bool			m_bShouldCameraTransitionOnClimbUpDown :1;
	bool			m_bShouldCameraIgnoreExiting	:1;

	bool			m_bAllowJoyriding				:1;
	bool			m_bAllowSundayDriving			:1;
	bool			m_bHasRequestedDrivers			:1;
	bool			m_bAllowBodyColorMapping		:1;
	bool			m_bHasSeatCollision				:1;
	bool			m_bHasSteeringWheelBone			:1;
	bool			m_bNeedsRopeTexture				:1;

	float			m_maxSteeringWheelAngle;
	float			m_maxSteeringWheelAnimAngle;

	float			m_minSeatHeight;

	Vector3			m_lockOnPositionOffset;

	float			m_LowriderArmWindowHeight;
	float			m_LowriderLeanAccelModifier;

	s32				m_numSeatsOverride;

	//flags
	VehicleModelInfoFlags m_VehicleFlags;

	CompileTimeAssert(VEH_LAST_WINDOW - VEH_FIRST_WINDOW + 1 <= 8);
	CompileTimeAssert(VPT_MAX <= 4); // we store the value in m_plateType with two bits
	CompileTimeAssert(VC_MAX < 32); // m_vehicleClass in 5 bits

	//////////////////////////////////////////////////////////////////////////
	// Static members

public:

	static void InitClass(unsigned initMode);
	static void ShutdownClass(unsigned shutdownMode);

	static void SetupCommonData();
	static void DeleteCommonData();

	static void UnloadResidentObjects();
	static void LoadResidentObjects(bool bBlockUntilDone=false);

    static s8 GetNumHdVehicles() { return ms_numHdVehicles; }

	// Car colours
	static void LoadVehicleColours();
#if __BANK
	static void AddWidgets(bkBank & b);
	static void RefreshVehicleWidgets();
	static void RefreshAllVehicleBodyColors();
	static void RefreshAllVehicleLightSettings();
	static void SaveVehicleColours(const char* filename);
	static void ReloadColourMetadata();

	static void SaveVehicleVariations();

	static void AddHDWidgets(bkBank &bank);
	static void DebugUpdate();
	static void UpdateCellForAsset(CUiGadgetText* pResult, u32 row, u32 col, void * extraCallbackData );
	static void UpdateCellForItem(CUiGadgetText* pResult, u32 row, u32 col, void * extraCallbackData );
	static void ShowSelectTxdInTexViewCB();
	static void GetHDMemoryUsage(u32 &physicalMem, u32 &virtualMem);

	static const eHierarchyId* GetSetupDoorIds() { return ms_aSetupDoorIds; }
#endif // __BANK
	static void LoadVehicleColoursDat();
	static void LoadVehicleColoursDat(const char* pFilename, bool bLoadColours);

	static void LoadVehicleMetaFile(const char *filename, bool permanent, s32 mapTypeDefIndex);
	static void UnloadVehicleMetaFile(const char *filename);
	static void UnloadModelInfoAssets(const char *name);

	static void AppendVehicleVariations(const char *filename);
	static void AppendVehicleColors(const char *filename);
	static void AppendVehicleColorsGen9(const char *filename);
	
	static void AddResidentObject(strLocalIndex index, int module);
	static const atArray<strIndex>& GetResidentObjects() {return ms_residentObjects;}
	static void SetCommonTxd(int txd) {ms_commonTxd = txd;}
	static int GetCommonTxd() {return ms_commonTxd;}
	static CVehicleModelInfoVarGlobal *GetVehicleColours() { return ms_VehicleColours; }
	static CVehicleModelInfoVariation *GetVehicleVariations() { return ms_VehicleVariations; }
	static CVehicleModColors* GetVehicleModColors() { return ms_ModColors; }

	static CVehicleModColorsGen9* GetVehicleModColorsGen9();
	static void LoadVehicleModColorsGen9(const char *filename);

	static void ValidateHorns(CVehicleKit &rKit);

	static VehicleClassInfo *GetVehicleClassInfo(eVehicleClass vehicleClass) { return &ms_VehicleClassInfo[vehicleClass]; }

    static void Update();

	static eHierarchyId ms_aSetupWheelIds[NUM_VEH_CWHEELS_MAX];

	static dev_float STD_BOAT_LINEAR_V_COEFF;
	static dev_float STD_BOAT_LINEAR_C_COEFF;
	static dev_float STD_BOAT_ANGULAR_V2_COEFF_Y;
	static dev_float STD_VEHICLE_ANGULAR_V_COEFF;

#if __BANK
	static u32 ms_numLoadedInfos;
#endif // __BANK

#if !__SPU
	// Set the vehicle bias on global scaling [0.0 = normal = e.g. x (1.0f + bias)]
	static void  SetLodMultiplierBias( float multiplierBias );
	static inline float GetLodMultiplierBias()							{ return ms_LodMultiplierBias;}
	static inline float GetLodMultiplierBiasAsDistanceScalar()			{ return 1.0f + GetLodMultiplierBias(); }
#else
	static inline float GetLodMultiplierBiasAsDistanceScalar()			{ return 1.0f; }
#endif

	static void SetMaxNumberMultiplier(const float maxNumberMultiplier)	{ ms_maxNumberMultiplier = maxNumberMultiplier; }

private:
	template <typename T, typename Id>
	static int AppendArray(rage::atArray<T> &to, const rage::atArray<T> &from, Id invalidId)
	{
		int nAdded = 0;

		for(int i = 0; i < from.GetCount(); i++)
		{
			bool found = false;

			if((Id)from[i].GetId() != invalidId)
			{
				for(int j = 0; j < to.GetCount(); j++)
				{
					if(to[j].GetId() == from[i].GetId())
					{
						found = true;
						break;
					}
				}
			}

			if(!found)
			{
				to.PushAndGrow(from[i]);
				nAdded++;
			}
		}

		return nAdded;
	}

	static float ms_maxNumberMultiplier;
	
	// new vehicle color data (loaded from xml)
	static CVehicleModelInfoVarGlobal * ms_VehicleColours;
	static fwPsoStoreLoadInstance ms_VehicleColoursLoadInst;

	static CVehicleModelInfoVariation* ms_VehicleVariations;
	static fwPsoStoreLoadInstance ms_VehicleVariationsLoadInst;

	static CVehicleModColors* ms_ModColors;
	static fwPsoStoreLoadInstance ms_ModColorsLoadInst;

	static eHierarchyId ms_aSetupDoorIds[NUM_VEH_DOORS_MAX];

	// resident objects
	static atArray<strIndex> ms_residentObjects;
	static int ms_commonTxd;

	// A separate vehicle bias for LOD scaling
#if !__SPU
	static float ms_LodMultiplierBias;
#endif


	static u8 ms_currentCol[4];
	static bool ms_lightsOn[4];
	
	static fwPtrListSingleLink ms_HDStreamRequests;

    static s8 ms_numHdVehicles;

	static atFixedArray<VehicleClassInfo, VC_MAX> ms_VehicleClassInfo;

	static sVehicleDashboardData ms_cachedDashboardData;
    static u32 ms_dialsRenderTargetOwner;
    static u32 ms_dialsRenderTargetSizeX;
    static u32 ms_dialsRenderTargetSizeY;
    static u32 ms_dialsRenderTargetId;
    static u32 ms_dialsFrameCountReq;
    static s32 ms_dialsMovieId;
    static s32 ms_dialsType;
    static s32 ms_activeDialsMovie;
    static atFinalHashString ms_rtNameHash;
	static u32 ms_lastTrackId;
	static const char* ms_lastStationStr;
	static u8 ms_requestPerFrame;

#if __BANK
	static bool ms_showHDMemUsage;
	static bool ms_ListHDAssets;
	static u32  ms_HDTotalPhysicalMemory;
	static u32  ms_HDTotalVirtualMemory;
	static atArray<HDVehilceRefCount> ms_HDVehicleInfos;

	static CUiGadgetWindow*	ms_pDebugWindow;
	static CUiGadgetList*	ms_pHDAssetList;
	static CUiGadgetList*	ms_pSelectItemList;
	static CVehicleModelInfo* ms_pSelectedVehicle;
#endif // __BANK
};

inline Vec3V_Out CVehicleModelInfo::GetWheelOffset(eHierarchyId wheel) const
{
    if (!modelinfoVerifyf(m_data, "GetWheelOffset called on non streamed in vehicle model '%s'", GetModelName()))
        return Vec3V(V_ZERO);
	Assertf(m_data->m_WheelOffsets.GetCount() > 0, "Wheel offsets accessed before they were calculated. Krehan wants a bug.");
	Assertf(wheel >= VEH_WHEEL_FIRST_WHEEL && wheel <= VEH_WHEEL_LAST_WHEEL, "Trying to access wheel offset on a non-wheel bone (%d)", wheel);
	Assertf( GetBoneIndex(wheel) > -1, "Requesting wheel offset of a type of wheel that this vehicle doesn't have");
	return m_data->m_WheelOffsets[wheel - VEH_WHEEL_FIRST_WHEEL];
}

inline void CVehicleModelInfo::AddResidentObject(strLocalIndex index, int module)
{
	strIndex streamingIndex = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(index);

	if(ms_residentObjects.Find(streamingIndex) == -1)
	{
		ms_residentObjects.PushAndGrow(streamingIndex);
	}
}

#define CVehicleModelInfo_Size64bit 800 BANK_ONLY(+ 16)  // RSG_PC and RSG_DURANGO

// ps3 is packing these a little bit better (at the moment)
//CompileTimeAssertSize( CVehicleModelInfo, 640 BANK_ONLY(+ 16), CVehicleModelInfo_Size64bit );

//#include "system/findsize.h"
//FindSize(CVehicleModelInfo); // 1456
//#include "system/findoffset.h"
//FindOffset(CVehicleModelInfo, m_padding); // 1397

#endif // INC_VEHICLE_MODELINFO_H_

