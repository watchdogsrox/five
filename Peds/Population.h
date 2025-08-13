// Title	:	Population.h
// Author	:	Gordon Speirs
// Started	:	06/04/00

#ifndef _POPULATION_H_
#define _POPULATION_H_


#include "Peds\PedType.h"
#include "Peds\popcycle.h"
#include "Peds\gangs.h"

#define DEBUG_POPULATION(X)

#define POPULATION_CULL_RANGE_FAR_AWAY		(65.0f) 	// for special peds like the ones generated at roadblocks
#define POPULATION_ADD_MIN_RADIUS			(42.5f)		// min distance from the camera a ped can be added
#define POPULATION_ADD_MAX_RADIUS			(50.5f)		// max distance from the camera a ped can be added
#define POPULATION_CULL_RANGE_OFF_SCREEN	(25.0f)		// peds outside this range will be removed
#define POPULATION_CULL_RANGE				(54.5f)		// peds outside this range will be removed


#define ADD_TO_POPULATION			(0)
#define SUBTRACT_FROM_POPULATION	(1)



#define STANDARD_MAX_NUMBER_OF_PEDS_IN_USE (4)	// We only ever want as many peds as this
#define STANDARD_MAX_NUMBER_OF_PEDS_IN_USE_INTERIOR (40)	// We only ever want as many peds as this
#define NETWORK_MAX_NUMBER_OF_PEDS_IN_USE (25)	// We only ever want as many peds as this in a network game


class CPed;
class CVehicle;
class CObject;
class CDummyObject;
class C2dEffect;
class CInteriorInst;





class CPopulation
{
public:
	static s32 ms_nNumCivMale;
	static s32 ms_nNumCivFemale;
	static s32 ms_nNumCop;
	static s32 ms_nNumEmergency;

    static s32 ms_nTotalCarPassengerPeds;
	static s32 ms_nTotalCivPeds;
	static s32 ms_nTotalMissionPeds;
	static s32 ms_nTotalPeds;
	
	static u32 m_CountDownToPedsAtStart;

	static bool	 m_bDontCreateRandomCops;

	static s32 m_nNoPedSpawnTimer;

#if __DEV
	static s32	OverrideNumberOfPeds;
	static bool		m_bCreatePedsAnywhere;	// not only where ped density is set on the surface
#endif




public:


	static Vector3	RegenerationPoint_a;
	static Vector3	RegenerationPoint_b;
	static Vector3	RegenerationFront;
	static bool		bZoneChangeHasHappened;
	
	static float    PedDensityMultiplier;
	static s32	m_AllRandomPedsThisType;

	static s32	MaxNumberOfPedsInUse;
	static bool	 	bInPoliceStation;
	
public:
		
	static void		Init();
	static void		InitSession();
	static void		Update(bool bAddPeds);
	
	static bool		AddToPopulation(float fMinRad, float fMaxRad, float fMinRadClose, float fMaxRadClose);
	static CPed*	AddPedInCar(CVehicle *pVehicle, bool bDriver, s32 CarRating = -1, s32 seat_num=0, bool bMustBeMale = false, bool bCriminal = false);
	static CPed*	AddExistingPedInCar(CPed *pPed, CVehicle *pVehicle);
	static CPed*	AddPed(ePedType PedType, u32 PedOcc, const Vector3& vecPosition, bool bGiveDefaultTask);
	
	static void		RemovePed(CPed* pPed);

	static void		GeneratePedsAtStartOfGame();
	static void		ManagePed(CPed* pPed, const Vector3& vecCentre);
	static void		ManagePopulation();
	static s32	FindNumberOfPedsWeCanPlaceOnBenches();
	static float	FindPedDensityMultiplierCullZone();

	static void		RemovePedsIfThePoolGetsFull(void);
	static void		RemoveAllRandomPeds();
	static bool		TestRoomForDummyObject(CObject* pObject);
	static bool		TestSafeForRealObject(CDummyObject *pDummy);

	static bool		ArePedStatsCompatible(s32 pedStat1, s32 pedStat2);
	static bool		PedMICanBeCreatedAtAttractor(s32 MI);
	static bool		PedMICanBeCreatedAtThisAttractor(s32 MI, char *pBrain);
	static bool		PedMICanBeCreatedInInterior(s32 MI);
	static s32	ChooseCivilianOccupation(bool bMustBeMale = false, bool bMustBeFemale = false, s32 MustHaveAnimGroup = -1, s32 AvoidThisOccupation = -1, s32 compatibleWithThisPedStat = -1, bool bOnlyOnFoots = false, bool bAtAttractor=false, char *pAttractorScriptName = NULL);
	static s32	ChooseCivilianOccupationForVehicle(bool bMustBeMale, CVehicle *pVeh);
	static s32	ChooseGangOccupation(s32 gang);
	static void		PlaceGangMembers(const ePedType ePedType, const int iNoOfGangMembers, const Vector3& vCentrePos);
 	static void		PlaceCouple(const ePedType malePedType, const s32 iMaleOccupation, const ePedType femalePedType, const s32 iFemaleOccupation, const Vector3& vCentrePos);
    static void		ChooseCivilianCoupleOccupations(s32& iMaleOccupation, s32& iFemaleOccupation);
    static bool		IsMale(const s32 iOccupation);
    static bool		IsFemale(const s32 iOccupation);
    static bool		IsSkateable(const Vector3& vPos);

	static void UpdatePedCount(CPed *pPed, u8 AddOrSub);

	static s32 GeneratePedsAtAttractors(const Vector3& Coors, float minRad, float maxRad, float minRadClose, float maxRadClose, s32 decisionMaker, s32 NumToCreate);
	static bool8 AddPedAtAttractor(s32 pedModelIndex, C2dEffect* pEffect, const Vector3& effectPos, CEntity* pEntity, s32 decisionMaker);
	static bool IsCorrectTimeOfDayForEffect(const C2dEffect* pEffect);


	static inline void StopSpawningPeds(u32 noSpawnTime);

#ifdef ONLINE_PLAY
	static void	NetworkRegisterAllPeds();
#endif
};




void	CPopulation::StopSpawningPeds(u32 noSpawnTime)
{
	m_nNoPedSpawnTimer = noSpawnTime;
}

#endif
