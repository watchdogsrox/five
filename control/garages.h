
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    garages.cpp
// PURPOSE : Logic for garages, bomb shops, respray places etc
// AUTHOR :  Obbe.
// CREATED : 5/10/00
// UPDATED : 18/11/2011 - Flavius
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _GARAGES_H_
#define _GARAGES_H_

// Rage headers
#include "parser/macros.h"

// Framework headers
#include "fwutil/PtrList.h"

// Game headers
#include "audio/dooraudioentity.h"
#include "scene/Entity.h"
#include "scene/RegdRefTypes.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclestorage.h"

#define MAX_NUM_GARAGES (40)
#define GARAGE_NAME_LENGTH (7)
#define MAX_NUM_STORED_CARS_IN_SAFEHOUSE (2)

// The distance at which garage doors open for approaching cars
#define DOOROPENDISTSQR (8.0f * 8.0f)

#define NUM_SAFE_HOUSES	(30)

class CAutomobile;
class CVehicle;

#define SAFE_HOUSE_GARAGES_REMOVE_CARS_DIST	(145.0f)		// At this distance from the player the safe house garages close and save the cars on it
#define SAFE_HOUSE_GARAGES_CREATE_CARS_DIST	(130.0f)		// At this distance from the player the safe house garages open and create the cars on it

#define SAFE_HOUSE_ENCLOSED_GARAGES_REMOVE_CARS_DIST	(25.0f)		// At this distance from the player the safe house garages close and save the cars on it
#define SAFE_HOUSE_ENCLOSED_GARAGES_CREATE_CARS_DIST	(20.0f)		// At this distance from the player the safe house garages open and create the cars on it

#define GARAGE_META_FILE "common:/data/garages"
#define GARAGE_META_FILE_EXT "meta"

struct CGarageFlags
{
	u8	bClosingEmpty : 1;
	u8	bLeaveCameraAlone : 1;
	u8	bDoorSlidingPreviousFrame : 1;
	u8	bDoorSlidingThisFrame : 1;		// This doesn't need to be saved. Only valid during CGarage::Update
	u8	bSafehouseGarageWasOpenBeforeSave : 1;
	u8  bSavingVehiclesEnabled : 1;
	u8	bSavingVehiclesWasEnabledBeforeSave : 1;
	u8	bDisableTidyingUp : 1;
};

enum GarageType
{
	GARAGE_NONE = 0,
	GARAGE_MISSION,
	GARAGE_RESPRAY,
	GARAGE_FOR_SCRIPT_TO_OPEN_AND_CLOSE,
	GARAGE_AMBIENT_PARKING,
	GARAGE_SAFEHOUSE_PARKING_ZONE,
};

enum GarageState
{
	GSTATE_CLOSED = 0,
	GSTATE_OPEN,
	GSTATE_CLOSING,
	GSTATE_OPENING,
	GSTATE_OPENEDWAITINGTOBEREACTIVATED,
	GSTATE_MISSIONCOMPLETED,
};

typedef u32 BoxFlags; // flags indicating which boxes in a garage are occupied

/////////////////////////////////////////////////////////////////////////////////
// A class to contain one garage
/////////////////////////////////////////////////////////////////////////////////

class CGarage
{
public:
	static const unsigned NET_OCCUPIED_CHECK_FREQ = 1000; // time between occupancy checks in MP
	enum { ALL_BOXES_INDEX = -1};
public:
	CGarage();
	struct Box
	{
		float BaseX, BaseY, BaseZ;
		float Delta1X, Delta1Y;
		float Delta2X, Delta2Y;
		float CeilingZ;
		bool useLineIntersection;
		PAR_SIMPLE_PARSABLE;
	};

	float MinX, MaxX, MinY, MaxY, MinZ, MaxZ;			// These are now defining an axis aligned bounding box. Handy for quick checks.

	u32				TimeOfNextEvent;
	RegdAutomobile	pCarToCollect;					// We'll only open for this specific type of car
	atHashString	name;
	atHashString	owner;
	u32				m_DoorSlideTimeOutTimer;

	GarageType		type;
	GarageState		state;
	GarageType		originalType;					// A few variables that restore the garage to it's original state.
	s8				safeHouseIndex;					// This numbers gets assigned as the garages are registered.
	CGarageFlags	Flags;

	bool			m_IsEnclosed;
	bool			m_IsMPGarage;					// If this an MP garage, then the box ID's below have meaning!
	s8				m_InteriorBoxIDX;				// Which box defines the interior area (-1 for none)
	s8				m_ExteriorBoxIDX;				// Which box defines the garage extents

	RegdDoor		m_pDoorObject;					// Points to the object that needs to be moved up and down when the garage opens/closes.
	float			m_oldDoorOpenness;				// To store what the openness was last frame so the garage code can progress even if the door is stuck.
	u32				m_oldDoorFrameCount;
	atArray<Box>	m_boxes;
	
	// network members
	atArray<PlayerFlags>	m_boxesOccupiedForPlayers;	// flags for each network player, indicating whether each box is occupied on their machine
	BoxFlags				m_lastBoxesOccupied;		// the last local boxes occupied state for this garage	
	VehicleType				m_permittedVehicleType;
	bool					m_bBoxesOccupiedHasChanged;	// the occupation boxes have changed, send them out to players

	void	Update(s32 MyIndex);
	void	UpdateSize(int boxIndex, const Box &box);

	bool	IsBeyondStoreDistance2(float dist2);
	bool	IsWithinRestoreDistance2(float dist2);

	bool	SlideDoorOpen();
	bool	SlideDoorClosed();
	float	FindDoorOpenness();
	bool	IsStaticPlayerCarEntirelyInside();
	bool	IsEntityEntirelyInside3D(const CEntity *pEntity, float Margin = 0.0f, int boxIndex = ALL_BOXES_INDEX);
	bool	IsEntityEntirelyOutside(const CEntity *pEntity, float Margin=0.0f, int boxIndex = ALL_BOXES_INDEX);
	bool	AllPlayerEntitiesEntirelyOutside(float Margin=0.0f);
	bool	AnyNetworkPlayersOnFootInGarage(float Margin = 0.0f);
	bool	IsGarageEmpty(int boxIndex = ALL_BOXES_INDEX, bool alternateSearch=false);
	bool	IsPlayerOutsideGarage(const CEntity *pEntity, float Margin=0.0f, int boxIndex = ALL_BOXES_INDEX);
	bool	IsPlayerEntirelyInsideGarage(const CEntity *pEntity, float Margin = 0.0f, int boxIndex = ALL_BOXES_INDEX);
	bool	IsObjectEntirelyInsideGarage(const CEntity *pEntity, float Margin = 0.0f, int boxIndex = ALL_BOXES_INDEX);
	bool	AreEntitiesEntirelyInsideGarage(bool peds, bool vehs, bool objs, float Margin, int boxIndex = CGarage::ALL_BOXES_INDEX);
	bool	IsAnyEntityEntirelyInsideGarage(bool peds, bool vehs, bool objs, float Margin, int boxIndex = CGarage::ALL_BOXES_INDEX);
	bool	IsEntityTouching3D(const CEntity *pEntity, int boxIndex = ALL_BOXES_INDEX);
	bool	EntityHasASphereWayOutsideGarage(CEntity *pEntity, float Margin = 0.0f);
	bool	IsAnyOtherCarTouchingGarage(CVehicle *pException);
	void	ThrowCarsNearDoorOutOfGarage(const CVehicle *pException);
	bool	IsAnyOtherPedTouchingGarage(class CPed *pException);
	bool	IsAnyCarBlockingDoor();
	s32		CountCarsWithCenterPointWithinGarage(CEntity *pException);
	void	RemoveCarsBlockingDoorNotInside();
	s32		FindMaxNumStoredCarsForGarage();
	void	OpenThisGarage();
	void	CloseThisGarage();
	float	CalcDistToGarageRectangleSquared(float X, float Y);
	bool	CanThisVehicleBeStoredInThisGarage(const CVehicle *pVehicle);
	void	StoreAndRemoveCarsForThisHideOut(CStoredVehicle *aStoredCars, bool bRemoveCars = true);
	bool	RestoreCarsForThisHideOut(CStoredVehicle *aStoredCars);
	void	TidyUpGarage();
	void	TidyUpGarageClose();
	void	PlayerArrestedOrDied();

	bool	IsPointInsideGarage(const Vector3& Point, int boxIndex = ALL_BOXES_INDEX);
	bool	IsPointInsideGarage(const Vector3& Point, float Margin, int boxIndex = ALL_BOXES_INDEX);
	bool    IsLineIntersectingGarage(const Vector3& start, const Vector3 &end, int boxIndex = ALL_BOXES_INDEX);
	bool    IsBoxIntersectingGarage(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, int boxIndex = ALL_BOXES_INDEX);
	void	CalcCentrePointOfGarage(Vector3& result);
	void	FindAllObjectsInGarage(atArray<CEntity*> &objects, bool checkPeds, bool checkVehicles, bool checkObjects);

	void	SetState(GarageState newState) { state = newState; }
	GarageState	GetState() const { return state; }

	void	SetType(GarageType newType) { type = newType; }

	// network functions
	void	NetworkInit(unsigned garageIndex);
	void	NetworkUpdate(unsigned garageIndex);
	void	UpdateOccupiedBoxes(bool alternateSearch);
	bool	IsGarageEmptyOnAllMachines(int boxIndex = ALL_BOXES_INDEX);
	void	ReceiveOccupiedStateFromPlayer(PhysicalPlayerIndex playerIndex, BoxFlags boxesOccupied);
	void	PlayerHasLeft(PhysicalPlayerIndex playerIndex);
	void	PlayerHasJoined(PhysicalPlayerIndex playerIndex, unsigned garageIndex);

	static void BuildQuadTestPoints(const CEntity *pEntity, Vector3 &p0, Vector3 &p1, Vector3 &p2, Vector3 &p3);
	static void BuildBoxTestPoints(const CEntity *pEntity,  Vector3 &p0, Vector3 &p1, Vector3 &p2, Vector3 &p3,
															Vector3 &p4, Vector3 &p5, Vector3 &p6, Vector3 &p7);

	void	SafeSetDoorFixedPhysics(bool bFixed, bool bNetwork);

private:
	void PrintMessage(const char *pTextLabel);
};


inline CGarage::CGarage() :
m_lastBoxesOccupied(0)
{ 
	type		 = GARAGE_NONE; 
	originalType = GARAGE_NONE; 

	MinX = FLT_MAX; 
	MaxX = -FLT_MAX; 

	MinY = FLT_MAX;
	MaxY = -FLT_MAX;

	MinZ = FLT_MAX;
	MaxZ = -FLT_MAX;
}

/////////////////////////////////////////////////////////////////////////////////
// A class to contain the functions.
/////////////////////////////////////////////////////////////////////////////////

class CGarages
{
public:
	class CGarageInitData
	{
	public:
		atArray<CGarage::Box> m_boxes;

		GarageType type;
		atHashString name;
		atHashString owner;
		int permittedVehicleType;
		bool startedWithVehicleSavingEnabled;

		bool isEnclosed;				// Enclosed Flag
		bool isMPGarage;				// A flag to say if this is an MP Garage or not (if the boxIDs below are in effect)
		s8	InteriorBoxIDX;				// Which box defines the interior area
		s8	ExteriorBoxIDX;				// Which box defines the garage extents.

		PAR_SIMPLE_PARSABLE;
	};

	class CGarageInitDataList
	{
	public:
		atArray<CGarageInitData> garages;
		PAR_SIMPLE_PARSABLE;
	};

	static	atArray<CGarage>	aGarages;
	static	bool				RespraysAreFree;			// resprays can be made free during missions
	static	bool				NoResprays;					// resprays are disabled.
	static	bool				ResprayHappened;			// Has a respray happened? (for the script to test)
	static	CGarage*			pGarageCamShouldBeOutside;	// Camera should be outside since stuff is going on
	static 	s32					LastGaragePlayerWasIn;		// the last garage any part of the player's vehicle was in
	static	s32					NumSafeHousesRegistered;	// How many safehouses have been registered on the map
	static	u32					timeForNextCollisionTest;	// when to trigger a new collision test for spawning vehicles

#if __BANK
	static	bool				bDebugDisplayGarages;		// Render a cube for the garage activation area
	static	bool				bAlternateOccupantSearch;	// Loop through ped and vehicle pools directly, versus fwSearch
#endif // __BANK

	static	CStoredVehicle		aCarsInSafeHouse[NUM_SAFE_HOUSES][MAX_NUM_STORED_CARS_IN_SAFEHOUSE];

	static	void	ClearAll();
	static	void	Init(unsigned initMode);
	static	void	Shutdown(unsigned shutdownMode);
	static	void	Load();
#if __BANK
	static	void	Save();
#endif // __BANK

	static	void	Update();
	static	s32		AddOne(float BaseX, float BaseY, float BaseZ, float Delta1X, float Delta1Y, float Delta2X, float Delta2Y, float CeilingZ, GarageType type, atHashString name, atHashString owner, bool isMpGarage, bool isEnclosed, s8 iBoxIDX, s8 eBoxIDX);
	static  s32		AddOne(const atArray<CGarage::Box> &boxes, GarageType type, atHashString name, atHashString owner, int permittedVehicleType, bool vehicleSavingEnabled, bool isMpGarage, bool isEnclosed, s8 iBoxIDX, s8 eBoxIDX);
	static  s32		GetNumGarages();
	static	s32		FindGarageIndex(char *pName);
	static	s32		FindGarageIndexFromNameHash(u32 NameHash);
	static	s32		FindGarageWithSafeHouseIndex(s32 safeHouseIdx);
	static	void	ChangeGarageType(s32 NumGarage, s32 NewType);
	static	bool	IsCarSprayable(CVehicle *pCar);
	static	void	SetTargetCarForMissionGarage(s32 NumGarage, CAutomobile *pCar);
	static	bool	HasCarBeenDroppedOffYet(s32 NumGarage);
	static	s32		QueryCarsCollected(s32 NumGarage);
	static	bool	HasImportExportGarageCollectedThisCar(s32 NumGarage, s32 NumCar);
	static	bool	IsGarageOpen(s32 NumGarage);
	static	bool	IsGarageClosed(s32 NumGarage);
	static	bool	HasThisCarBeenCollected(s32 NumGarage, s32 CarIndex);
	static	bool	IsThisCarWithinGarageArea(s32 NumGarage, const CEntity *pEntity);
	static	bool	HasCarBeenCrushed(s32 CarId);
	static	CGarage* GarageCameraShouldBeOutside();
	static	void	PlayerArrestedOrDied();
	static	void	CloseHideOutGaragesBeforeSave(bool bByGameLogic = false);
	static	void	OpenHideOutGaragesAfterSave();
	static  void    CloseSafeHouseGarages();
	static	void	StopCarFromBlowingUp(CAutomobile *pCar);
	static  bool	IsVehicleWithinHideOutGarageThatCanSaveIt(const CVehicle *pVehicle);
	static	bool	IsPointWithinAnyGarage(const Vector3 &Point);
	static	bool	PlayerIsInteractingWithGarage();
	static	void	AllRespraysCloseOrOpen(bool bOpen);
	static	bool	AttachDoorToGarage(class CDoor *pObj);
	static	bool	IsPaynSprayActive();

	static bool		IsGarageEmpty(s32 garageIndex, int boxIndex = CGarage::ALL_BOXES_INDEX); 

	static bool		IsPlayerEntirelyInsideGarage(s32 garageIndex, const CEntity *pPlayer, float margin, int boxIndex = CGarage::ALL_BOXES_INDEX);
	static bool		IsPlayerEntirelyOutsideGarage(s32 garageIndex, const CEntity *pPlayer, float margin, int boxIndex = CGarage::ALL_BOXES_INDEX);
	static bool		IsPlayerPartiallyInsideGarage(s32 garageIndex, const CEntity *pPlayer, int boxIndex = CGarage::ALL_BOXES_INDEX);
	static bool		AreEntitiesEntirelyInsideGarage(s32 garageIndex, bool peds, bool vehs, bool objs, float margin, int boxIndex = CGarage::ALL_BOXES_INDEX);
	static bool		IsAnyEntityEntirelyInsideGarage(s32 garageIndex, bool peds, bool vehs, bool objs, float margin, int boxIndex = CGarage::ALL_BOXES_INDEX);


	static bool		IsObjectEntirelyInsideGarage(s32 garageIndex, const CEntity *pPlayer, float margin, int boxIndex = CGarage::ALL_BOXES_INDEX);
	static bool		IsObjectEntirelyOutsideGarage(s32 garageIndex, const CEntity *pPlayer, float margin, int boxIndex = CGarage::ALL_BOXES_INDEX);
	static bool		IsObjectPartiallyInsideGarage(s32 garageIndex, const CEntity *pPlayer, int boxIndex = CGarage::ALL_BOXES_INDEX);
	static void     ThrowCarsNearDoorOutOfGarage(s32 garageIndex, const CVehicle *pExecption);

	static void		FindAllObjectsInGarage(s32 garageIndex, atArray<CEntity*> &objects, bool checkPeds, bool checkVehicles, bool checkObjects);
	static bool     IsLineIntersectingGarage(s32 garageIndex, const Vector3& start, const Vector3 &end, int boxIndex = CGarage::ALL_BOXES_INDEX);
	static bool     IsBoxIntersectingGarage(s32 garageIndex, const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, int boxIndex= CGarage::ALL_BOXES_INDEX);

	// network functions
	static  void	NetworkInit();
	static  void	PlayerHasLeft(PhysicalPlayerIndex playerIndex);
	static  void	PlayerHasJoined(PhysicalPlayerIndex playerIndex);
	static bool		IsGarageEmptyOnAllMachines(s32 garageIndex, int boxIndex = CGarage::ALL_BOXES_INDEX); 

#if __BANK
	static  void	InitWidgets();
#endif
};



inline bool CGarages::IsGarageEmpty(s32 garageIndex, int boxIndex) 
{ 
	if (Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex))
	{
		return aGarages[garageIndex].IsGarageEmpty(boxIndex);
	}

	return false;
}

inline bool CGarages::IsGarageEmptyOnAllMachines(s32 garageIndex, int boxIndex) 
{ 
	if (Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex))
	{
		return aGarages[garageIndex].IsGarageEmptyOnAllMachines(boxIndex);
	}

	return false;
}

inline bool CGarages::IsPlayerEntirelyInsideGarage(s32 garageIndex, const CEntity *pPlayer, float margin, int boxIndex)
{
	if (Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex) && 
		Verifyf(boxIndex >= CGarage::ALL_BOXES_INDEX && boxIndex < aGarages[garageIndex].m_boxes.GetCount(), "Invalid Box Index %d for Garage %d", boxIndex, garageIndex))
	{
		return aGarages[garageIndex].IsPlayerEntirelyInsideGarage(pPlayer, margin, boxIndex);
	}

	return false;
}

inline bool CGarages::IsPlayerEntirelyOutsideGarage(s32 garageIndex, const CEntity *pPlayer, float margin, int boxIndex)
{
	if (Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex) &&
		Verifyf(boxIndex >= CGarage::ALL_BOXES_INDEX && boxIndex < aGarages[garageIndex].m_boxes.GetCount(), "Invalid Box Index %d for Garage %d", boxIndex, garageIndex))
	{
		return aGarages[garageIndex].IsPlayerOutsideGarage(pPlayer, margin, boxIndex);
	}

	return false;
}


inline bool CGarages::IsObjectEntirelyInsideGarage(s32 garageIndex, const CEntity *pEntity, float margin, int boxIndex)
{
	if (Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex) && 
		Verifyf(boxIndex >= CGarage::ALL_BOXES_INDEX && boxIndex < aGarages[garageIndex].m_boxes.GetCount(), "Invalid Box Index %d for Garage %d", boxIndex, garageIndex))
	{
		return aGarages[garageIndex].IsObjectEntirelyInsideGarage(pEntity, margin, boxIndex);
	}

	return false;
}

inline bool CGarages::IsObjectEntirelyOutsideGarage(s32 garageIndex, const CEntity *pEntity, float margin, int boxIndex)
{
	if (Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex) && 
		Verifyf(boxIndex >= CGarage::ALL_BOXES_INDEX && boxIndex < aGarages[garageIndex].m_boxes.GetCount(), "Invalid Box Index %d for Garage %d", boxIndex, garageIndex))
	{
		return aGarages[garageIndex].IsEntityEntirelyOutside(pEntity, margin, boxIndex);
	}

	return false;
}

inline void CGarages::ThrowCarsNearDoorOutOfGarage(s32 garageIndex, const CVehicle *pExecption)
{
	if (Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex))
	{
		aGarages[garageIndex].ThrowCarsNearDoorOutOfGarage(pExecption);
	}
}

inline void CGarages::FindAllObjectsInGarage(s32 garageIndex, atArray<CEntity*> &objects, bool checkPeds, bool checkVehicles, bool checkObjects)
{
	if (Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex))
	{
		aGarages[garageIndex].FindAllObjectsInGarage(objects, checkPeds, checkVehicles, checkObjects);
	}
}

#endif

