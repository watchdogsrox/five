//
//
//    Filename: SeatModelInfo.h
//     Creator: Bryan Musson
//
//
#ifndef INC_SEAT_MODELINFO_H_
#define INC_SEAT_MODELINFO_H_

#include "ai\debug\types\VehicleDebugInfo.h"
#include "Animation/AnimBones.h"
#include "modelinfo/modelinfo_channel.h"
#include "scene/RegdRefTypes.h"
#include "vector/quaternion.h"
#include "Vehicles/Metadata/VehicleDebug.h"
#include "Vehicles/Metadata/VehicleEnterExitFlags.h"
#include "vehicles/VehicleDefines.h"

//forward definitions

class CEntity;
class CVehicleLayoutInfo;
class CVehicleEntryPointInfo;
class CPed;
class CVehicleSeatAnimInfo;

// Typedefs
typedef CVehicleEnterExitFlags::FlagsBitSet VehicleEnterExitFlags;

namespace rage 
{
	class crClip;
}

//////////////////////////////////////////////////////////////////////////
// CSeatAccessor
//////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
// Enum to make seat access more clear
//-------------------------------------------------------------------------
typedef enum 
{
	SA_invalid = -1,
	SA_directAccessSeat = 0,	// Straight into seat
	SA_indirectAccessSeat,		// by shuffling through a seat
	SA_indirectAccessSeat2,		// by shuffling through a seat
	SA_MaxSeats
} SeatAccess;

typedef enum
{
	SR_Any = 0,
	SR_Specific,
	SR_Prefer,
	Num_SeatRequestTypes
} SeatRequestType;

typedef enum 
{
	Seat_invalid = -1,
	Seat_driver = 0,
	Seat_frontPassenger,
	Seat_backLeft,
	Seat_backRight,
	Seat_numSeats
} Seat;

// This class allows an entry point to access multiple seats depending on the seat access type
class CSeatAccessor
{
public:

#if __BANK
	static const char* GetSeatRequestTypeString(s32 i)
	{
		switch (i)
		{
			case SR_Any: return "SR_Any";
			case SR_Specific: return "SR_Specific";
			case SR_Prefer: return "SR_Prefer";
			default: break;
		}
		return "SR_Unknown";
	}
#endif // __BANK

	enum SingleSeatAccess
	{
		SSA_Invalid = -1,
		SSA_DirectAccessSeat = 0,	// Straight into seat
		SSA_IndirectAccessSeat,		// by shuffling through a seat
		SSA_IndirectAccessSeat2,	// by shuffling through a seat
		SSA_MaxSingleAccessSeats
	};

	enum MultipleSeatAccess
	{
		MSA_Invalid = -1,
		MSA_DirectAccessSeat1 = 0,
		MSA_DirectAccessSeat2,
		MSA_DirectAccessSeat3,
		MSA_DirectAccessSeat4,
		MSA_DirectAccessSeat5,
		MSA_DirectAccessSeat6,
		MSA_DirectAccessSeat7,
		MSA_DirectAccessSeat8,
		MSA_MaxMultipleAccessSeats
	};

	enum SeatAccessType
	{
		eSeatAccessTypeInvalid = -1,
		eSeatAccessTypeSingle = 0,
		eSeatAccessTypeMultiple,
	};

	CSeatAccessor();
	~CSeatAccessor();

	// Returns how this seat can be accessed through this point, directly or indirectly
	SeatAccess	CanSeatBeReached(s32 iSeat) const;

	// Returns the seat that can be accessed through this point
	s32			GetSeat(SeatAccess iSeatAccess, s32 iSeatIndex) const;

	// Returns the seat that can be accessed through this point
	s32			GetSeatByIndex(s32 iSeatIndex) const;

	// Set the seat access type
	SeatAccessType	GetSeatAccessType() const { return m_eSeatAccessType; }
	void			SetSeatAccessType(SeatAccessType eSeatAccessType) { m_eSeatAccessType = eSeatAccessType; }

	void		AddSingleSeatAccess(s32 directSeat, s32 indirectSeat, s32 indirectSeat2);

	void		AddMultipleSeatAccess(s32 iSeat);

	void		ClearAccessableSeats() { m_aAccessibleSeats.clear(); }
	s32			GetNumSeatsAccessible() const { return m_aAccessibleSeats.GetCount(); }

private:

	// Seats accessible from this entry point
	// These are indicies into the seat array stored on the model info
	atFixedArray<s32, MSA_MaxMultipleAccessSeats> m_aAccessibleSeats;
	SeatAccessType m_eSeatAccessType;
};

//////////////////////////////////////////////////////////////////////////
// CEntryExitPoint
//////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------
// A point of entry into the vehicle
// 1 direct seat to access (right next to the door)
// 1 indirect seat (by shuffling through the direct seat)
//-------------------------------------------------------------------------
class CEntryExitPoint
{
public:
	CEntryExitPoint();
	~CEntryExitPoint() {}

	// Setup this entry exit point for the vehicle
	void		SetSeatAccess(s32 iBoneIndex, s32 iDoorHandleBoneIndex);

	// Returns how this seat can be accessed through this point, directly or indirectly
	SeatAccess	CanSeatBeReached(s32 iSeat) const;
	// Returns the seat that can be accessed through this point
	s32			GetSeat(SeatAccess iSeatAccess, s32 iSeatIndex = -1) const;	

	bool		IsSingleSeatAccess() const { return m_SeatAccessor.GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeSingle ? true : false; }
	s32			GetDoorBoneIndex() const { return (s32) m_iDoorBoneIndex; } 
	s32			GetDoorHandleBoneIndex() const { return (s32) m_iDoorHandleBoneIndex; } 
	bool		GetEntryPointPosition(const CEntity* pEntity, const CPed *pPed, Vector3& vPos, bool bRoughPosition = false, bool bAdjustForWater = false) const;

	CSeatAccessor&	GetSeatAccessor() { return m_SeatAccessor; }

	const CSeatAccessor&	GetSeatAccessor() const { return m_SeatAccessor; }

private:

	// The seat accessor determines which seats are accessible from this entry point
	CSeatAccessor	m_SeatAccessor;
	u32				m_uEntryPointHash;	// Unused?
	s16				m_iDoorBoneIndex;
	s16				m_iDoorHandleBoneIndex;
};


//////////////////////////////////////////////////////////////////////////
// CModelSeatInfo
//////////////////////////////////////////////////////////////////////////
class CModelSeatInfo
{
public:
	CModelSeatInfo() : m_pLayoutinfo(NULL) { };

	enum eEntryFlags
	{
		EF_AnimPlaying				= BIT0,
		EF_IsPickUp					= BIT1,
		EF_IsPullUp					= BIT2,
		EF_DontApplyEntryOffsets	= BIT3,
		EF_RoughPosition			= BIT4,
		EF_DontApplyOpenDoorOffsets = BIT5,
		EF_IsCombatEntry			= BIT6,
		EF_AdjustForWater			= BIT7,
		EF_ForceUseOpenDoor			= BIT8,
		EF_DontAdjustForVehicleOrientation = BIT9
	};

	void	Reset();

	void	InitFromLayoutInfo(const CVehicleLayoutInfo* pLayoutInfo, const crSkeletonData* pSkelData, s32 iNumSeatsOverride = -1);
	void	FindTargetSeatInfoFromLayout(const CVehicleLayoutInfo* pLayoutInfo, const CVehicleEntryPointInfo* pEntryInfo, s32 iSeatAccessIndex, s32& iTargetSeatIndex, s32& iShuffleSeatIndex, s32& iShuffleSeatIndex2);
	s32		GetEntryExitPointIndex(const CEntryExitPoint* pEntryExitPoint) const;
	s32		GetEntryPointIndexForSeat(s32 iSeat, const CEntity* pEntity, SeatAccess accessType = SA_directAccessSeat, bool bCheckSide = false, bool bLeftSide = false) const;
	s32		GetNumSeats() const {return m_iNumSeats;}
	void	SetNumSeats(u8 iNumSeats) {m_iNumSeats=iNumSeats;}
	s32		GetDriverSeat() const {return m_iDriverSeat;}
	void	SetDriverSeat(s8 iDriverSeat) {m_iDriverSeat=iDriverSeat;}
	s32		GetLayoutSeatIndex(int iSeatIndex) const;	
	s32		GetLayoutEntrypointIndex(int iEntryIndex) const;
	const CVehicleLayoutInfo* GetLayoutInfo() const {return m_pLayoutinfo;}

	static bool IsIndirectSeatAccess(SeatAccess eSeatAccess) { return eSeatAccess == SA_indirectAccessSeat || eSeatAccess == SA_indirectAccessSeat2; }
	static void AdjustEntryMatrixForVehicleOrientation(Matrix34& rEntryMtx, const CVehicle& rVeh, const CPed *pPed, const CModelSeatInfo& rModelSeatInfo, s32 iEntryPointIndex, bool bAlterEntryPointForOpenDoor);
	static bool AdjustPositionForGroundHeight(Vector3& vPosition, const CVehicle& rVeh, const CPed& rPed, const float fProbeHeight, const float fMaxGroundDiff);
	static bool RENDER_ENTRY_POINTS;
	static void CalculateSeatSituation(const CEntity* pEntity, Vector3& vSeatPosition, Quaternion& qSeatOrientation, s32 iEntryPointIndex, s32 iTargetSeat = -1);		
	static void	CalculateExitSituation(CEntity* pEntity, Vector3& vExitPosition, Quaternion& qExitOrientation, s32 iExitPointIndex, const crClip* pExitClip=NULL);
	static void	CalculateEntrySituation(const CEntity* pEntity, const CPed *pPed, Vector3& vEntryPosition, Quaternion& qEntryOrientation, s32 iEntryPointIndex, s32 iEntryFlags = 0, float fClipPhase = 0.0f, const Vector3 * pvPosModifier = NULL);
	static bool CalculateThroughWindowPosition(const CVehicle& rVehicle, Vector3& vExitPosition, s32 iSeatIndex);
	static bool IsInMPAndTargetVehicleShouldUseSeatDistance(const CVehicle* pVeh);

	// NOTE: If we start to call this function from different threads, we shouldn't use the static ms_EntryPointsDebug instance 
	// Entry Point Evaluation Functions
	static s32 EvaluateSeatAccessFromAllEntryPoints(const CPed* pPed, const CEntity* pEntity, SeatRequestType iSeatRequest, s32 iSeatRequested, bool bWarping, VehicleEnterExitFlags iFlags = VehicleEnterExitFlags(), s32 *pSeatIndex = NULL
#if __BANK
		, CRidableEntryPointEvaluationDebugInfo* pEntryPointsDebug = &CVehicleDebug::ms_EntryPointsDebug
#endif
		, s32 iPreferredEntryPoint = -1
		);
	static s32 EvaluateSeatAccessFromEntryPoint(const CPed* pPed, const CEntity* pEntity, SeatRequestType iSeatRequest, s32 iSeatRequested, s32 iEntryPoint, bool bWarping, VehicleEnterExitFlags iFlags = VehicleEnterExitFlags(), SeatAccess* pSeatAccess = NULL, s32 *pSeatIndex = NULL
#if __BANK
		, CRidableEntryPointEvaluationDebugInfo* pEntryPointsDebug = NULL
#endif
		);

	static bool ShouldLowerEntryPriorityDueToTurretAngle(const CPed* pPed, const CVehicleSeatAnimInfo* pSeatAnimInfo, const CVehicle* pTargetVehicle, const CVehicleEntryPointInfo* pEntryPointInfo, s32 iTargetSeat);
	static bool ShouldLowerEntryPriorityDueToTurretDoorBlock(s32 iSeatIndex, s32 iEntryPoint, const CPed* pPed, const CVehicle* pTargetVehicle);

	static bool ShouldInvalidateEntryPointDueToOnBoardJack(s32 iSeatIndex, s32 iEntryPoint, const CPed& rPed, const CVehicleSeatAnimInfo* pSeatAnimInfo, const CVehicle& rTargetVehicle, const CVehicleEntryPointInfo* pEntryPointInfo);

	static bool IsPositionClearForPed(const CPed* pPed, const CEntity* pEntity, const Matrix34& mEntryMatrix, const bool bExcludeVehicle = true, float fStartZOffset = 0.0f, CEntity** pHitEntity = NULL);
	

	CEntryExitPoint*		GetEntryExitPoint(s32 i) { modelinfoAssertf( (u32) i < (u32) m_iNumEntryExitPoints, "Invalid entry/exit point %d (only %d available)", i, m_iNumEntryExitPoints ); return &m_aEntryExitPoints[i]; }	
	const CEntryExitPoint*	GetEntryExitPoint(s32 i) const { modelinfoAssertf( (u32) i < (u32) m_iNumEntryExitPoints, "Invalid entry/exit point %d (only %d available)", i, m_iNumEntryExitPoints ); return &m_aEntryExitPoints[i]; }	

	s32						GetNumberEntryExitPoints() const { return m_iNumEntryExitPoints; }

	// Accessing bone ids from seat / door indicies
	s32						GetBoneIndexFromSeat(int iSeatIndex) const;
	s32						GetBoneIndexFromEntryPoint(int iEntryPointIndex) const;

	// Accessing seat / door indicies from bones
	// Returns -1 if not found	
	s32						GetEntryPointFromBoneIndex(int iBoneIndex) const;
	s32						GetSeatFromBoneIndex(int iBoneIndex) const; 

	s32						GetShuffleSeatForSeat(s32 iEntryExitPoint, s32 iSeat, bool bAlternateShuffleLink = false) const;
	static bool				ThereIsEnoughRoomToFitPed(const CPed* pPed, const CVehicle* pVehicle, s32 iSeat);

private:
	// Entry exit points for getting into seats in this vehicle
	CEntryExitPoint m_aEntryExitPoints[MAX_ENTRY_EXIT_POINTS];
	u8				m_iNumEntryExitPoints;
	s8			m_iLayoutEntryPointIndicies[MAX_ENTRY_EXIT_POINTS]; // Maps this vehicle's doors to the doors in its layout info (not 1:1 so we can support missing doors)

	// Seat information
	s8			m_iLayoutSeatIndicies[MAX_VEHICLE_SEATS];		// Maps this vehicle's seats to the seats in its layout info (not 1:1 so we can support missing seats)	
	s16			m_iSeatBoneIndicies[MAX_VEHICLE_SEATS];
	u8			m_iNumSeats;
	s8			m_iDriverSeat;

	const CVehicleLayoutInfo* m_pLayoutinfo;
};

class CSeatManager 
{
	// Seat interface
public:
	CSeatManager();

	void Init(s32 iMaxSeats, s32 iDriverSeat) {Assert(iMaxSeats<=MAX_VEHICLE_SEATS); m_iNoSeats=(s8)iMaxSeats; m_iDriverSeat=(s8)iDriverSeat; m_nNumPlayers=0; m_nNumScheduledOccupants = 0; }
	// 	// Returns a pointer to the driver
	CPed* GetDriver() const;
	CPed* GetLastDriver() const;
	// Return the ped in the given seat
	CPed* GetPedInSeat(s32 iSeatIndex) const;
	// Return the last ped GUID in the given seat
	CPed* GetLastPedInSeat(s32 iSeatIndex) const;
	// Counts the number of peds in seats
	int CountPedsInSeats(bool bIncludeDriversSeat = true, bool bIncludeDeadPeds = true, bool bIncludePedsUsingSeat = false, const CVehicle* pVeh = NULL, int iBossIDToIgnoreSameOrgPeds = -1) const;
	// Counts the number of free seats
	int CountFreeSeats(bool bIncludeDriversSeat = true, bool bCountPedsWeCanJack = false, CPed* pPedWantingToEnter = NULL) const;
	// Gets the number of seats in the vehicle
	s32 GetMaxSeats() const {return m_iNoSeats; }	
	// Gets the seat the ped is set in
	s32 GetPedsSeatIndex(const CPed* pPed) const;
	// Gets the seat the ped last sit in (as long as nobody has sit in it since)
	s32 GetLastPedsSeatIndex(const CPed* pPed) const;
	// Add ped in the given seat
	bool AddPedInSeat(CPed *pPed, s32 iSeat);
	// Remove the ped from the seat
	s32 RemovePedFromSeat(CPed *pPed);
	// How many players are in seats?
	u8 GetNumPlayers() const { return m_nNumPlayers; }
	// Set the last ped in the seat
	void SetLastPedInSeat(CPed* pPed, s32 iSeat);
	// Set the last ped in the driver seat
	void SetLastDriverFromNetwork(CPed* pPed);
	// Notify that a ped in this seat manager has changed into / from a player
	void ChangePedsPlayerStatus(const CPed &pPed, bool bBecomingPlayer);
	// Whether there have ever been any peds in any seat
	bool HasEverHadPedInAnySeat() const { return m_bHasEverHadOccupant; }
	void ResetHasEverHadPedInAnySeat() { m_bHasEverHadOccupant = false; }

	// Let the seat manager know that we have a ped that's in the scheduled ped in vehicle queue
	void AddScheduledOccupant() {m_nNumScheduledOccupants++; FastAssert(m_nNumScheduledOccupants <= 32); }
	// Call when one of the scheduled peds has been created
	void RemoveScheduledOccupant() {m_nNumScheduledOccupants--; FastAssert(m_nNumScheduledOccupants <= 32); } // make sure we don't wrap under
	// Get number of scheduled occupants
	u8	GetNumScheduledOccupants() const { return m_nNumScheduledOccupants; } 

private:
	s8	m_iNoSeats;
	s8	m_iDriverSeat;
	u8	m_nNumPlayers;
	u8	m_nNumScheduledOccupants: 7; // can only schedule 6 peds or so in a car (bus), but fill up this u8 with 7 bits
	u8	m_bHasEverHadOccupant : 1;
	RegdPed	m_pSeatOccupiers[MAX_VEHICLE_SEATS];
	RegdPed	m_pLastSeatOccupiers[MAX_VEHICLE_SEATS];
	RegdPed	m_pLastDriverFromNetwork;
};


//////////////////////////////////////////////////////////////////////////
// Inlined functions

__forceinline CPed* CSeatManager::GetDriver() const
{
	// Since this is inlined now, using (u32) trick to eliminate
	// a branch.  (a >= 0 && a < b) equivalent to ((u32)a < (u32)b).
	if((u32)m_iDriverSeat < (u32)GetMaxSeats())
	{
		return m_pSeatOccupiers[m_iDriverSeat];
	}
	// if a vehicle has no driver seat, return NULL (no driver)
	return NULL;
}


#endif // INC_SEAT_MODELINFO_H_



