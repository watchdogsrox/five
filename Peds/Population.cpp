// Title	:	Population.cpp
// Author	:	Gordon Speirs
// Started	:	06/04/00


// Rage headers
#include "system/param.h"

// Game headers
#include "Population.h"
#include "Animation\animmanager.h"
#include "Camera\Camera.h"
#include "Control\Darkel.h"
#include "Control\gamelogic.h"
#include "Control\Pathfind.h"
#include "Control\replay.h"
#include "Control\roadblocks.h"
#include "Debug\Debug.h"
#include "Debug\DebugGlobals.h"
#include "Debug\timebars.h"
#include "Event\Events.h"
#include "Game\cheat.h"
#include "Game\Clock.h"
#include "Game\ModelIndices.h"
#include "Game\weather.h"
#include "Game\zones.h"
#include "Maths\Maths.h"
#include "Maths\random.h"
#include "Maths\vector.h"
#include "ModelInfo\ModelInfo.h"
#include "ModelInfo\ModelInfo.h"
#include "Objects\DummyObject.h"
#include "Objects\Object.h"
#include "PathServer\PathServer.h"
#include "PedGroup\PedGroup.h"
#include "Peds\gangs.h"
#include "peds\ped.h"
#include "Peds\PedDebugVisualiser.h"
#include "Peds\PedDecision.h"
#include "Peds\PedFactory.h"
#include "Peds\pedgen.h"
#include "Peds\PedIntelligence.h"
#include "Peds\pedplacement.h"
#include "Peds\PedType.h"
#include "Peds\PlayerPed.h"
#include "Peds\popcycle.h"
#include "Peds\pedvariationmgr.h"
#include "Physics\BoundsStore.h"
#include "Physics\GtaArchetype.h"
#include "Physics\WorldProbe.h"
#include "Renderer\Water.h"
#include "Renderer\zonecull.h"
#include "Scene\iplstore.h"
#include "scene\portals\interiorInst.h"
#include "Script\script.h"
#include "Streaming\populationstreaming.h"
#include "Streaming\streaming.h"
#include "System\FileMgr.h"
#include "System\Pad.h"
#include "Task\TaskBasic.h"
#include "Task\TaskCarUtils.h"
#include "Task\TaskGoto.h"
#include "Task\TaskKillController\TaskKillController.h"
#include "Task\TaskManager.h"
#include "Task\TaskPartner.h"
#include "Task\TaskSecondary.h"
#include "Task\TaskSeekEntity.h"
#include "fwutil\PtrList.h"
#include "Vehicles\Automobile.h"
#include "Vehicles\VehicleFactory.h"
#include "Vehicles\cargen.h"
#include "Vehicles\vehiclepopulation.h"
#include "control\persistence.h"

//network headers
#ifdef ONLINE_PLAY
#include "Network\network.h"
#endif 

PARAM(nopeds, "[population] Dont create any random peds");
PARAM(doragdolls, "[physics] Do allow ragdolls to activate");
PARAM(allragdolls, "[physics] Force all peds to be allow ragdolls");


// grrr. This macro conflicts under visual studio.
#if defined (MSVC_COMPILER)
#ifdef max
#undef max
#endif //max
#endif //MSVC_COMPILER

#define FRAMESTOCOMPLETEMANAGEPOPULATION	(32)

#define GANG_CREATION_DISTANCE_OFFSET (30.0f)

#define CIVILIAN_COUPLE_POPULATION (0.9f)

#define SUNBATHER_TOWEL_SPACING (3.0f)

//#define ADD_TO_POP_DEBUG

s32 CPopulation::ms_nNumCivMale;
s32 CPopulation::ms_nNumCivFemale;
s32 CPopulation::ms_nNumCop;
s32 CPopulation::ms_nNumEmergency;
s32 CPopulation::ms_nTotalCarPassengerPeds;
s32 CPopulation::ms_nTotalCivPeds;
s32 CPopulation::ms_nTotalPeds;
s32 CPopulation::ms_nTotalMissionPeds;
u32 CPopulation::m_CountDownToPedsAtStart;
bool  CPopulation::m_bDontCreateRandomCops = false;
Vector3	CPopulation::RegenerationPoint_a;
Vector3	CPopulation::RegenerationPoint_b;
Vector3 CPopulation::RegenerationFront;
bool	CPopulation::bZoneChangeHasHappened;
float   CPopulation::PedDensityMultiplier = 1.0f;
s32	CPopulation::m_AllRandomPedsThisType = -1;
s32	CPopulation::MaxNumberOfPedsInUse = STANDARD_MAX_NUMBER_OF_PEDS_IN_USE;							// This can be changed by the inifile values
bool	CPopulation::bInPoliceStation = false;
s32	CPopulation::m_nNoPedSpawnTimer = 0;


#if __DEV
s32	CPopulation::OverrideNumberOfPeds = -1;
bool	CPopulation::m_bCreatePedsAnywhere = false;
#endif


// Name			:	Init
// Purpose		:	Init CPopulation stuff
// Parameters	:	None
// Returns		:	Nothing

void CPopulation::Init()
{
#if __BANK
	// Set up some bank stuff.
	bkBank &bank = BANKMGR.CreateBank("Population and Zones");
	bank.AddToggle("Print ped population debug", &CPopCycle::m_bDisplayDebugInfo);
	bank.AddToggle("Print car population debug", &CPopCycle::m_bDisplayCarDebugInfo);
	bank.AddSlider("Override Number of Cars", &CVehiclePopulation::OverrideNumberOfCars, -1, 1000, 1);
	bank.AddSlider("Override Number of Parked Cars", &CVehiclePopulation::OverrideNumberOfParkedCars, -1, 1000, 1);
	bank.AddSlider("Override Number of Peds", &CPopulation::OverrideNumberOfPeds, -1, 1000, 1);
	bank.AddSlider("Max number of cars in use", &CVehiclePopulation::MaxNumberOfCarsInUse, 0, 200, 1);
	bank.AddToggle("Render Zones", &CCullZones::bRenderCullzones, NullCB);
	bank.AddSlider("Num Cam Zones", &CCullZones::m_numCamZones, 0, 9000000, 0);
	bank.AddToggle("Render Car Generators", &CTheCarGenerators::gs_bDisplayCarGenerators, NullCB);
	bank.AddToggle("Create Peds Anywhere", &CPopulation::m_bCreatePedsAnywhere);
	bank.AddToggle("Generate test ambulance", &gbGenerateAmbulance);
	bank.AddToggle("Generate test firetruck", &gbGenerateFiretruck);
#endif
}


// Name			:	Initialise
// Purpose		:	Init CPopulation stuff
// Parameters	:	None
// Returns		:	Nothing

void CPopulation::InitSession()
{
	DEBUGLOG("Initialising CPopulation...\n");

#if __DEV
	if(PARAM_nopeds.Get())
		gbNoPedsGeneration = true;

	if(PARAM_doragdolls.Get())
		gbDoRagdolls = true;

	if(PARAM_allragdolls.Get())
		gbForceRagdolls = true;
#endif

	ms_nNumCivMale				= 0;
	ms_nNumCivFemale			= 0;
	ms_nNumCop					= 0;
	ms_nNumEmergency			= 0;
	ms_nTotalCarPassengerPeds   = 0;
	ms_nTotalCivPeds			= 0;
	ms_nTotalPeds				= 0;
	ms_nTotalMissionPeds		= 0;
	
	m_CountDownToPedsAtStart = 5;	// Give game time to settle down

	bZoneChangeHasHappened = false;

	PedDensityMultiplier = 1.0f;
	m_AllRandomPedsThisType = -1;
	MaxNumberOfPedsInUse = STANDARD_MAX_NUMBER_OF_PEDS_IN_USE;							// This can be changed by the inifile values
	bInPoliceStation = false;
	m_bDontCreateRandomCops = false;

	
		// Set up a default ped for the game to use if there aren't any appropriate peds available
	gPopStreaming.SetDefaultPedMI(CModelInfo::GetModelIndex("m_y_streetblk_02"));
	ASSERT(gPopStreaming.GetDefaultPedMI() >= 0);
	CStreaming::RequestObject(gPopStreaming.GetDefaultPedMI(), CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
	CStreaming::LoadAllRequestedObjects();

	DEBUGLOG("CPopulation ready\n");
} // end - CPopulation::Initialise







// Name			:	Update
// Purpose		:	Updates CPopulation stuff
// Parameters	:	None
// Returns		:	Nothing

void CPopulation::Update(bool bAddPeds)
{
	if (CReplay::ReplayGoingOn()) return;
	
	CPersistenceMgr::RevivePersistentPedsAndVehicles();
	ManagePopulation();
	RemovePedsIfThePoolGetsFull();

	// Give game a couple of frames to get everything initialised. Then we generate peds around player.
	if (m_CountDownToPedsAtStart)
	{
		m_CountDownToPedsAtStart--;
		if (!m_CountDownToPedsAtStart)
		{
			GeneratePedsAtStartOfGame();
		}
	}
	else
	{		// Don't do this until the game has settled down (or peds get created at 0.0f, 0.0f, 0.0f)

		ms_nTotalCivPeds = ms_nNumCivMale + ms_nNumCivFemale;
		ms_nTotalPeds = ms_nTotalCivPeds + /*ms_nTotalGangPeds +*/ ms_nNumCop + ms_nNumEmergency;
		if ( /* rage !CCutsceneMgr::IsCutsceneProcessing() &&*/ bAddPeds)
		{
			bool bMorePedsNeeded = AddToPopulation
				(POPULATION_ADD_MIN_RADIUS * CRenderer::GetGenerationDistanceScale(), 
				 POPULATION_ADD_MAX_RADIUS * CRenderer::GetGenerationDistanceScale(), 
				 POPULATION_CULL_RANGE_OFF_SCREEN - 10.0f, 
				 POPULATION_CULL_RANGE_OFF_SCREEN);

			if (bMorePedsNeeded)
			{
				GeneratePedsAtAttractors(FindPlayerCentreOfWorld(),
					 POPULATION_ADD_MIN_RADIUS * CRenderer::GetGenerationDistanceScale(),
					 POPULATION_ADD_MAX_RADIUS * CRenderer::GetGenerationDistanceScale(), 
					 POPULATION_CULL_RANGE_OFF_SCREEN - 10.0f, 
					 POPULATION_CULL_RANGE_OFF_SCREEN,
					 -1,
					 1);
			}
		}
	}

	DEBUG_POPULATION
	(
		CDebug::DebugString("Civilian", 0, 3, 0xffffffff);
		CDebug::DebugInt(ms_nTotalCivPeds, 20, 3, 2);

		CDebug::DebugString("Cop", 0, 4, 0xffffffff);
		CDebug::DebugInt(ms_nNumCop, 20, 4, 2);

		CDebug::DebugString("Emergency", 0, 5, 0xffffffff);
		CDebug::DebugInt(ms_nNumEmergency, 20, 5, 2);

		CDebug::DebugString("Gang", 0, 6, 0xffffffff);
		CDebug::DebugInt(ms_nTotalGangPeds, 20, 6, 2);

		CDebug::DebugString("Total", 0, 9, 0xffffffff);
		CDebug::DebugInt(ms_nTotalPeds, 20, 9, 2);

		CDebug::DebugString("Mission Peds", 0, 10, 0xffffffff);
		CDebug::DebugInt(ms_nTotalMissionPeds, 20, 10, 2);

	)

	if (m_nNoPedSpawnTimer > 0)
		m_nNoPedSpawnTimer -= CTimer::GetMilliseconds();

} // end - CPopulation::Update

// Name			:	GeneratePedsAtStartOfGame
// Purpose		:	When a new game is started the streets seem a bit empty.
//					This function should be called to fill the area around the player.
// Parameters	:	None
// Returns		:	Nothing

void CPopulation::GeneratePedsAtStartOfGame()
{
	s32	Tries;

	for (Tries = 0; Tries < 100; Tries++)
	{
		ms_nTotalCivPeds = ms_nNumCivMale + ms_nNumCivFemale;
		ms_nTotalPeds = ms_nTotalCivPeds + ms_nNumCop + ms_nNumEmergency;
        ms_nTotalPeds -= ms_nTotalCarPassengerPeds;
		AddToPopulation(10.0f, POPULATION_ADD_MAX_RADIUS, 10.0f, POPULATION_ADD_MAX_RADIUS);
	}	
	
	GeneratePedsAtAttractors(FindPlayerCentreOfWorld(), 10.0f, POPULATION_ADD_MAX_RADIUS, 10.0f, POPULATION_ADD_MAX_RADIUS, -1, 1);

}



//
// name:		ManagePed
// description:	Code to manage a ped
void CPopulation::ManagePed(CPed* pPed, const Vector3& vecCentre)
{
#ifdef ONLINE_PLAY
	// don't manage peds controlled remotely by another machine
	if (pPed->IsNetworkClone())
		return;
#endif

	if (!pPed->IsPlayer())
	{
		//Delete ped if he can be deleted and he isn't in a vehicle and he isn't attached to a vehicle.	
		if (pPed->CanBeDeleted() && !pPed->m_PedAiFlags.bInVehicle && (0==pPed->m_pAttachToEntity || pPed->m_pAttachToEntity->GetType()!=ENTITY_TYPE_VEHICLE))
		{
			s32 timeSinceDead = CTimer::GetElapsedTimeInMilliseconds() - pPed->m_nTimeOfDeath;
			if(pPed->GetPedState() == PED_DEAD && 
				(timeSinceDead > PED_REMOVED_AFTER_DEATH ||
					(CDarkel::FrenzyOnGoing() && timeSinceDead > PED_REMOVED_AFTER_DEATH_INRAMPAGE) /* rage||
					(timeSinceDead > PED_REMOVED_AFTER_DEATH_GANGWARS && CGangWars::GangWarFightingGoingOn())*/ ))
			{
				pPed->m_PedAiFlags.bFadeOut = true;
			}
			// Test whether ped should be removed because of time-out or something
			if (pPed->m_PedAiFlags.bFadeOut /* rage&& CVisibilityPlugins::GetClumpAlpha((RpClump*)pPed->GetRwObject()) == 0*/)
			{
				RemovePed(pPed);
			}
			else
			{	
				float DiffX = pPed->GetPosition().x - vecCentre.x;
				float DiffY = pPed->GetPosition().y - vecCentre.y;
				float Dist = rage::Sqrtf(DiffX*DiffX + DiffY*DiffY);

				bool	bRemovePed = false;	

				float fDist=Dist;
				fDist *= pPed->fRemoveRangeMultiplier;
				if((pPed->GetPedType()>=PEDTYPE_GANG1)&&(pPed->GetPedType()<=PEDTYPE_GANG10))
				{
				    fDist-=GANG_CREATION_DISTANCE_OFFSET;
				}
/*rage
				else if((pPed->m_nPedFlags.bDeadPedInFrontOfCar) &&(pPed->m_pMyAccidentVehicle))
				{
				    //If a ped has been made as a dead ped in front of a car and the car is still alive
				    //then don't delete the ped just because it is too far from the camera.  Wait until 
				    //the car has been deleted and the ped is too far from the camera.
				    fDist=0;
				}
*/
				if ( fDist > POPULATION_CULL_RANGE_FAR_AWAY * CRenderer::GetGenerationDistanceScale() )
				{
					bRemovePed = true;
				}
				else if ( (!pPed->m_PedAiFlags.bCullExtraFarAway) &&
							fDist > POPULATION_CULL_RANGE * CRenderer::GetGenerationDistanceScale())
				{
					bRemovePed = true;
				}
				else if (fDist > POPULATION_CULL_RANGE_OFF_SCREEN)
				{
					bool	bPedIsOnScreen = pPed->GetIsOnScreen();
				
					if ( (CTimer::GetElapsedTimeInMilliseconds() > pPed->DontUseSmallerRemovalRange && (!bPedIsOnScreen))						 	  
						 	&&gCamInterface.IsModeActive(CCamTypes::CAM_TYPE_PHOTO)
						 	&& (!gCamInterface.IsLookingLeft()) && (!gCamInterface.IsLookingRight()) && (!gCamInterface.IsLookingBehind()))
					{
						bRemovePed = true;
					}
					if (bPedIsOnScreen)
					{		// Ped is on-screen and not too far away. Don't remove for a wee while.
						if (pPed->GetPedType() == PEDTYPE_COP)
						{
							pPed->DontUseSmallerRemovalRange = CTimer::GetElapsedTimeInMilliseconds() + 12000;
						}
						else
						{
							pPed->DontUseSmallerRemovalRange = CTimer::GetElapsedTimeInMilliseconds() + 6000;
						}
					}
				}
				else
				{		// Guy is nearby. Don't remove for a wee while
					if (pPed->GetPedType() == PEDTYPE_COP)
					{
						pPed->DontUseSmallerRemovalRange = CTimer::GetElapsedTimeInMilliseconds() + 12000;
					}
					else
					{
						pPed->DontUseSmallerRemovalRange = CTimer::GetElapsedTimeInMilliseconds() + 6000;
					}
				}		


#ifdef ONLINE_PLAY
				if (bRemovePed && pPed->GetNetworkObject())
				{
					bRemovePed = ((CNetObjPed*)pPed->GetNetworkObject())->TryToPassControlOutOfScope();
				}
#endif
				if (bRemovePed)
				{
					if(pPed->GetIsOnScreen())
						pPed->m_PedAiFlags.bFadeOut = true;
					else
						RemovePed(pPed);
				}
			}
		}
	}
}


// Name			:	ManagePopulation
// Purpose		:	Scans through peds. Makes any dummy peds that are close enough 
//					into active peds, and makes any active peds that are far enough
//					away into dummy peds.
// Parameters	:	None
// Returns		:	Nothing

void CPopulation::ManagePopulation(void)
{
	Vector3			vecCentre;
	Vector3			vecTargetObject, vecResetDist;
	Vector3			vecTargetPed;
	CPed::Pool			*pPedPool = CPed::GetPool();
//	CDummyObject::Pool 	*pDummyPool = CDummyObject::GetPool();
//	CObject::Pool 		*pObjPool = CObject::GetPool();
	CPed			*pPed;
//	CDummyObject	*pDummy;
//	CObject			*pObj;
	s32			CurrSlot;
	s32			Batch;
//	s32			PoolSize;


	Batch = (CTimer::GetFrameCount() & (FRAMESTOCOMPLETEMANAGEPOPULATION - 1));
	
	vecCentre = FindPlayerCentreOfWorld();	//	Might need to change this for multiplayer
											//	so that an equal number of peds are created
											//	around each player


// Put this stuff back in when Dummies and Objects are working
/*
	PoolSize = pObjPool->GetSize();
	
	for (CurrSlot = ((Batch * PoolSize) / FRAMESTOCOMPLETEMANAGEPOPULATION); CurrSlot < (((Batch + 1) * PoolSize) / FRAMESTOCOMPLETEMANAGEPOPULATION); CurrSlot++)
	{
		pObj = pObjPool->GetSlot(CurrSlot);

		if (pObj && pObj->GetModelIndex()>=0)
		{
			ManageObject(pObj, vecCentre);
		}
	}

	// Scan through all dummy peds and convert to real peds or delete as appropriate
	PoolSize = pDummyPool->GetSize();
	
	for (CurrSlot = ((Batch * PoolSize) / FRAMESTOCOMPLETEMANAGEPOPULATION); CurrSlot < (((Batch + 1) * PoolSize) / FRAMESTOCOMPLETEMANAGEPOPULATION); CurrSlot++)
	{
		pDummy = pDummyPool->GetSlot(CurrSlot);
		
		// If dummy exists, 
		if(pDummy)
		{
			ManageDummy(pDummy, vecCentre);
		}
	}
*/
	CurrSlot = pPedPool->GetSize();
	
	while (CurrSlot--)
	{
		pPed = pPedPool->GetSlot(CurrSlot);
		
		if(pPed)
		{
			ManagePed(pPed, vecCentre);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindNumberOfPedsWeCanPlaceOnBenches
// PURPOSE :  Used to work out how many peds we have spare to create on benches and
//			  wait in queues, use teller machines etc.
/////////////////////////////////////////////////////////////////////////////////

s32 CPopulation::FindNumberOfPedsWeCanPlaceOnBenches()
{
	s32	ReturnVal = 0;
	s32	MaxNumCivilians = (s32)rage::Min((float)CPopulation::MaxNumberOfPedsInUse, CPopCycle::m_NumOther_Peds);
	MaxNumCivilians = (s32)(MaxNumCivilians * CPopulation::PedDensityMultiplier * CPopulation::FindPedDensityMultiplierCullZone());

	ReturnVal = MaxNumCivilians - (CPopulation::ms_nNumCivMale + CPopulation::ms_nNumCivFemale);

	ReturnVal += 2;	// This extra one is an attempt to favour peds on benches because they look cool. Without this fudge we wouldn't get them as much.

//	if (ReturnVal > 0)
//	{		// Find the MI of the extra ped we would create.
//		*pMI = CPopulation::ChooseCivilianOccupation(false, false, -1, -1, -1, true);
//		if (*pMI < 0) return 0;		// Could find a Model that was actually loaded in. Never mind then.
//	}
	return ReturnVal;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindPedDensityMultiplierCullZone
// PURPOSE :  Finds the number the ped population is reduced by here.
/////////////////////////////////////////////////////////////////////////////////

float CPopulation::FindPedDensityMultiplierCullZone()
{
	if (CCullZones::FewerPeds()) return 0.6f;

	return 1.0f;
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemovePedsIfThePoolGetsFull
// PURPOSE :  If the ped pool fills up this function will remove some peds.
/////////////////////////////////////////////////////////////////////////////////

void CPopulation::RemovePedsIfThePoolGetsFull(void)
{
	
	if ((CTimer::GetFrameCount() & 7) == 5)
	{
		CPed::Pool *pool = CPed::GetPool();

		if (pool->GetNoOfFreeSpaces() < 8)
		{		// Need to remove at least one ped
			s32 		i=pool->GetSize();
			CPed		*pPed;
			float		NearestDist = 9999999.9f, Dist;
			CPed		*pNearestPed = NULL;

			while(i--)
			{
				pPed = pool->GetSlot(i);
				if(pPed)
				{
					if ( pPed->CanBeDeleted() )
					{
						Dist = (*gCamInterface.GetPos() - pPed->GetPosition()).Mag();
						if (Dist < NearestDist)
						{
							NearestDist = Dist;
							pNearestPed = pPed;
						}
					}
				}
			}
			if (pNearestPed)
			{
				RemovePed(pNearestPed);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveAllRandomPeds
// PURPOSE :  Removes all the peds we can. Used for entering interior
/////////////////////////////////////////////////////////////////////////////////

void CPopulation::RemoveAllRandomPeds()
{
	CPed::Pool *pool = CPed::GetPool();
	s32 		i=pool->GetSize();
	CPed		*pPed;

	while(i--)
	{
		pPed = pool->GetSlot(i);
		if(pPed)
		{
			if ( pPed->CanBeDeleted() && !pPed->IsPlayer())
			{
				RemovePed(pPed);
			}
		}
	}
}






// Name			:	AddToPopulation
// Purpose		:	Adds a ped of the specified type to the current population
// Parameters	:	PedType - type of ped to add
//					fMinRad, fMaxRad - min and max radii, ped will be added somewhere
//					within these two boundaries (***try to keep at least 10 metres between
//					the two otherwise it may not work very well***)
// Returns		:	Nothing

bool CPopulation::AddToPopulation(float fMinRad, float fMaxRad, float fMinRadClose, float fMaxRadClose)
{
	ePedType 	NewPedType;
	s32 		NewPedOccupation;
	s32   	NewPedMaleOccupation=-1;
	s32   	NewPedFemaleOccupation=-1;
	Vector3 	PlayerPos;
	Vector3 	vecResult;
	bool 		bResult;
	bool 		bMoreCopsNeeded = false;
	CNodeAddress 	FromNode, ToNode;
	float 		RandomFraction, GroundZ;
	s32		NumPedsInGroup, PedInGroup;
	float		MaxNumPeds;
	CPed		*pFirstPed = NULL, *pThisPed;
	bool		FoundGround;
	u8		MaxCopsInPursuit;
	CPlayerInfo	*pPlayer = CWorld::GetMainPlayerInfo();
	bool		bMorePedsNeeded = false;

	if (m_nNoPedSpawnTimer > 0)
		return false;

#if __DEV
	if(gbNoPedsGeneration)
		return false;
#endif

	PlayerPos = FindPlayerCentreOfWorld();

	Vector3 vecPedSpawnOrigin, vecPedSpawnLookahead;

	 // check against actual number of cops on map rather than just those that are in pursuit or we might create far too many

	MaxCopsInPursuit = pPlayer->pPed->GetPlayerWanted()->m_nMaxCopsInPursuit;
	
	if(ms_nNumCop < MaxCopsInPursuit)
	{
		if( !pPlayer->pPed->m_PedAiFlags.bInVehicle &&
			(  CVehiclePopulation::NumLawEnforcerCars >= pPlayer->pPed->GetPlayerWanted()->m_nMaxCopCarsInPursuit
			|| CVehiclePopulation::NumRandomCars >= pPlayer->CarDensityForCurrentZone*CVehiclePopulation::CarDensityMultiplier
			|| CVehiclePopulation::NumRandomCars+CVehiclePopulation::NumLawEnforcerCars+CVehiclePopulation::NumMissionCars+CVehiclePopulation::NumParkedCars+CVehiclePopulation::NumAmbulancesOnDuty+CVehiclePopulation::NumFireTrucksOnDuty >= CVehiclePopulation::MaxNumberOfCarsInUse) )
		{
			bMoreCopsNeeded = true;
			fMinRad = POPULATION_ADD_MIN_RADIUS;
			fMaxRad = POPULATION_ADD_MAX_RADIUS;
		}
	}	

 	MaxNumPeds = (float)CPopulation::MaxNumberOfPedsInUse;
	
	float	Mult = 1.0f; // rage CIniFile::PedNumberMultiplier;
	if (CDarkel::FrenzyOnGoing()) Mult = 1.0f;
	
	MaxNumPeds = MIN(MaxNumPeds, CPopCycle::m_NumCops_Peds + CPopCycle::m_NumOther_Peds );

	MaxNumPeds *= PedDensityMultiplier;

#if __DEV
	if (OverrideNumberOfPeds >= 0)
	{
		MaxNumPeds = (float)OverrideNumberOfPeds;
	}
#endif

//	MaxNumPeds *= CPopulation::PedDensityMultiplier * CPopulation::FindPedDensityMultiplierCullZone();
	
	if (ms_nTotalPeds < MaxNumPeds)
	{
		bMorePedsNeeded = true;	// Pass this back so that we can create a ped at an attractor as well.
	}
	
	if ((ms_nTotalPeds < MaxNumPeds) || bMoreCopsNeeded)
	{
	
#ifdef ADD_TO_POP_DEBUG
		
		static int iNoOfTries=0;
		static int iNoOfSuccesses1=0;
		static int iNoOfSuccesses2=0;
		static int iNoOfSuccesses3=0;
		static int iNoOfSuccesses4=0;
		
		iNoOfTries++;
		
		if(iNoOfTries==1000)
		{
		    iNoOfTries=1;
		    iNoOfSuccesses1=0;
		    iNoOfSuccesses2=0;
		    iNoOfSuccesses3=0;
		    iNoOfSuccesses4=0;
		}
		
		const float fSuccessFrac1=(float)iNoOfSuccesses1/(float)iNoOfTries;
		const float fSuccessFrac2=(float)iNoOfSuccesses2/(float)iNoOfTries;
		const float fSuccessFrac3=(float)iNoOfSuccesses3/(float)iNoOfTries;
		const float fSuccessFrac4=(float)iNoOfSuccesses4/(float)iNoOfTries;

		char s[255];
		sprintf(s,"Fracs are %f %f %f %f N is %d \n",fSuccessFrac1,fSuccessFrac2,fSuccessFrac3,fSuccessFrac4,ms_nTotalPeds);
		DEBUGLOG(s); 

#endif

        // Generate a random number and use this in conjunction with the ped ratios 
        // to decide which type of ped to create
        // Once the ped type has been decided, maybe need special functions 
        // to decide on the ped occupation

		if (bMoreCopsNeeded)
		{	//	Create a police ped
			// Obbe put the next lines back in ones we have cop models.
			NewPedType = PEDTYPE_COP;
			NewPedOccupation = gPopStreaming.FindPoliceOfficerMI();

			if( NewPedOccupation == -1 )
			{
				return bMorePedsNeeded;
			}
		}
		else
		{
			if (CPopCycle::FindNewPedType(&NewPedType, &NewPedOccupation, false)) // && FindPlayerWanted()->GetWantedLevel() <= WANTED_LEVEL2)) This wanted level thing doesn't workk because Christians script grabs all peds and gives them an indoor decision maker. Cops just stand there with their hands in the air.
			{
				if (NewPedType == PEDTYPE_CIVMALE || NewPedType == PEDTYPE_CIVFEMALE)
				{
					if (fwRandom::GetRandomNumberInRange(0.0f,1.0f)>CIVILIAN_COUPLE_POPULATION)
					{
						ChooseCivilianCoupleOccupations(NewPedMaleOccupation, NewPedFemaleOccupation);
	                    if((-1==NewPedMaleOccupation)||(-1==NewPedFemaleOccupation))
	                    {
	                        return bMorePedsNeeded;
	                    }
	                    NewPedType = ((CPedModelInfo*)CModelInfo::GetModelInfo(NewPedMaleOccupation))->GetDefaultPedType();
					}
				}
				if (m_AllRandomPedsThisType > 0)
				{
					NewPedType = (ePedType)m_AllRandomPedsThisType;
				}
			}
			else
			{
				return bMorePedsNeeded;
			}
		}


		// If we are talking about a gang member we work out a random number that decides the
		// number of peds in this group. Note we have a chance of 0 as well to keep the total
		// expected peds to be created at 1 so that the ratio stuff is still valid.
		if (NewPedType >= PEDTYPE_GANG1 && NewPedType <= PEDTYPE_GANG10)
		{

    		NumPedsInGroup=CPedGroupPlacer::ComputeRandomisedGangSize();
  
		}
    	else
		{
			NumPedsInGroup = 1;
		}
		

        //Gang peds need to be created a greater distance from the player/camera because they
        //get made in clusters of large numbers of peds.  This makes it much more obvious that
        //they are being created.  		
		float fMinR=fMinRad;
		float fMaxR=fMaxRad;
		float fMinRClose=fMinRadClose;
		float fMaxRClose=fMaxRadClose;
		if (NewPedType >= PEDTYPE_GANG1 && NewPedType <= PEDTYPE_GANG10)
		{
			fMinR+=GANG_CREATION_DISTANCE_OFFSET;
			fMaxR+=GANG_CREATION_DISTANCE_OFFSET;
			fMinRClose+=0.0f;
			fMaxRClose+=0.0f;
	    }
	

		//***********************************************************************
		//
		// Ped density is now a value from 0..3 inclusive.
		// All surface types have a ped density associated with them.
		//
		// 0 = no peds ever created here (eg. roads)
		// ...
		// 3 = most preferred ped creation places (eg. pavements)
		//
		// Areas such as grass & fields might have a ped density of 1 or 2.
		//
		// We should avoid only looking for generation coords with a ped
		// density of 3, because then peds will only ever be created if there
		// is an area of pavement nearby.  (unless that's the desired behaviour!)
		//
		//***********************************************************************

		s32 iRequiredPedDensity = fwRandom::GetRandomNumberInRange(1, 4);
		Assert(iRequiredPedDensity >= 1 && iRequiredPedDensity < 4);

#if	__DEV
		// Allow us to toggle peds being created anywhere on map (for when artists have not used surface types which make peds)
		if(m_bCreatePedsAnywhere)
			iRequiredPedDensity = 0;
#endif

		bResult = CPathServer::GetPedGenerationCoords(
			PlayerPos,
			fMinR, fMaxR, fMinRClose, fMaxRClose,
			iRequiredPedDensity,
			&vecResult
		);

    	if (bResult)
	    {
//	    	s32	Density = MIN(ThePaths.FindNodePointer(FromNode)->Density, ThePaths.FindNodePointer(ToNode)->Density);
//	    	if ((fwRandom::GetRandomNumber() & 15) <= Density)
	    	{
#ifdef ADD_TO_POP_DEBUG
	    	    iNoOfSuccesses1++;
#endif	    	    
		    	// Randomize the coordinates a bit
//		    	ThePaths.TakeWidthIntoAccountForCoors(FromNode, ToNode, fwRandom::GetRandomNumber(), &vecResult.x, &vecResult.y);
		    
				if (NewPedType >= PEDTYPE_GANG1 && NewPedType <= PEDTYPE_GANG10)
	    		{
		    	    PlaceGangMembers(NewPedType,NumPedsInGroup,vecResult);
			    }
			    else if((NewPedMaleOccupation>=0)&&(NewPedFemaleOccupation>=0))
			    {   
			        PlaceCouple(PEDTYPE_CIVMALE,NewPedMaleOccupation,PEDTYPE_CIVFEMALE,NewPedFemaleOccupation,vecResult);
			    }
			    else
			    {   		    
	    			for (PedInGroup = 0; PedInGroup < NumPedsInGroup; PedInGroup++)
	    			{
	    				// don't bother making ped if model hasn't loaded
	    				if (NewPedType == PEDTYPE_COP)
	    				{
// can assume cop is in memory
							/*
							//NEEDS_MOVED_TO_NY_LAYER
							if (NewPedOccupation == COPTYPE_FBI)
	    					{
	    						if(!CStreaming::HasObjectLoaded(MODELID_FBI_PED, CModelInfo::GetStreamingModuleId()) ||
	    							!CStreaming::HasObjectLoaded(CWeaponInfo::GetWeaponInfo(WEAPONTYPE_MP5)->GetModelId(), CModelInfo::GetStreamingModuleId()))
	    							return bMorePedsNeeded;
	    					}
	    					else if (NewPedOccupation == COPTYPE_SWAT)
	    					{
	    						if(!CStreaming::HasObjectLoaded(MODELID_SWAT_PED, CModelInfo::GetStreamingModuleId()) ||
	    							!CStreaming::HasObjectLoaded(CWeaponInfo::GetWeaponInfo(WEAPONTYPE_MICRO_UZI)->GetModelId(), CModelInfo::GetStreamingModuleId()))
	    							return bMorePedsNeeded;
	    					}
	    					else if (NewPedOccupation == COPTYPE_ARMY)
	    					{
	    						if(!CStreaming::HasObjectLoaded(MODELID_ARMY_PED, CModelInfo::GetStreamingModuleId()) ||
	    							!CStreaming::HasObjectLoaded(CWeaponInfo::GetWeaponInfo(WEAPONTYPE_MP5)->GetModelId(), CModelInfo::GetStreamingModuleId()) ||
	    							!CStreaming::HasObjectLoaded(CWeaponInfo::GetWeaponInfo(WEAPONTYPE_GRENADE)->GetModelId(), CModelInfo::GetStreamingModuleId()))
	    							return bMorePedsNeeded;
	    					}
	    					else if(NewPedType != PEDTYPE_COP)
	    					{
	    						Assertf(0, "CPopulation::AddToPopulation - Unknown cop type");
	    					}
							//NEEDS_MOVED_TO_NY_LAYER
							*/
	    				}
	    				

	    				// ~dodgy~ temp hack till collision model problem gets resolved (dropping
	    				// peds into the world at the specified coord results in them getting stuck
	    				// in the map and dying)
	    				vecResult.z += 0.7f;
	    				// If there are more peds to follow we will randomise the coordinates a bit
	    				if (PedInGroup < NumPedsInGroup)
	    				{
	    					RandomFraction = (fwRandom::GetRandomNumber() & 255) / 256.0f;
	    									
	    					if(pFirstPed)
	    					{
	    						float fx = fwRandom::GetRandomNumberInRange(((float)PedInGroup)*0.75f,(((float)PedInGroup)+1.0f)*0.75f);
	    						float fy = fwRandom::GetRandomNumberInRange(((float)PedInGroup)*0.75f,(((float)PedInGroup)+1.0f)*0.75f);

	    						if(fwRandom::GetRandomNumber() & 1)
	    						{
	    							fx = -fx;
	    						}
	    						if(fwRandom::GetRandomNumber() & 1)
	    						{
	    							fy = -fy;
	    						}
	    						vecResult.x = pFirstPed->GetPosition().x + fx;
	    						vecResult.y = pFirstPed->GetPosition().y + fy;
	    					}
	    				}
	    				
	    				bResult = CPedPlacement::IsPositionClearForPed(vecResult);
	    			
	    				if (bResult)
	    				{
#ifdef ADD_TO_POP_DEBUG
	    				    iNoOfSuccesses2++;
#endif	    				    
	    					// If there are more peds to follow we will randomise the coordinates a bit
	    					if (PedInGroup+1 < NumPedsInGroup)
	    					{
	    						// add a bit to stop the peds getting created halfway into the ground
	    						GroundZ = 0.7f + CWorldProbe::FindGroundZFor3DCoord(vecResult.x, vecResult.y, vecResult.z + 2.0f, &FoundGround);
	    						if (!FoundGround)
	    						{
	    //							sprintf(gString2, "No ground below ped creation coors: %f %f %f\n", vecResult.x, vecResult.y, vecResult.z);
	    //							printf("%s", gString2);							
	    							return bMorePedsNeeded;
	    						}
	    						vecResult.z = MAX(vecResult.z, GroundZ);
	    					}

	    					bool 	bGoAhead = true;

	    					if ( (gCamInterface.IsSphereVisible(vecResult, 2.0f)) && ( ((Vector3)(vecResult - PlayerPos)).XYMag() < POPULATION_ADD_MIN_RADIUS))
	    					{
	    						bGoAhead = false;
	    					}
	    					
       					    //Don't allow skaters on the beach.
       					    if(bGoAhead && PEDSTAT_SKATER==((CPedModelInfo*)CModelInfo::GetModelInfo(NewPedOccupation))->GetDefaultPedStats())
       					    {
       					        bGoAhead=IsSkateable(vecResult);
       				     	}

    					    
	    					// Do a bit of a special case check. We don't want enemy gang members in the grove.
	    					if (bGoAhead && NewPedType >= PEDTYPE_GANG1 && NewPedType <= PEDTYPE_GANG10 && NewPedType != PEDTYPE_GANG2)
	    					{
	    						if (vecResult.x > 2400.0f && vecResult.x < 2540.0f && vecResult.y > -1730.0f && vecResult.y < -1625.0f)
	    						{
	    							bGoAhead = false;
	    						}
	    					} 
	    		
#ifdef ONLINE_PLAY
							if (CNetwork::IsGameInProgress() && bGoAhead)
							{
								// don't add peds if they are going to appear in front of other network players
								//bGoAhead = !CNetwork::GetObjectManager().IsInScopeWithAnyRemotePlayer(CNetworkObject::NETOBJTYPE_PED, vecResult);
								bGoAhead = !CNetwork::GetObjectManager().IsVisibleToAnyRemotePlayer(vecResult, CModelInfo::GetModelInfo(NewPedOccupation)->GetBoundingSphereRadius());
							}
#endif
	    					// Do a final check to make sure the guy is not visible AND closeby
	    					if (bGoAhead)
	    					{
#ifdef ADD_TO_POP_DEBUG
	    					    iNoOfSuccesses3++;
#endif	    					    
	    						pThisPed = AddPed(NewPedType, NewPedOccupation, vecResult, true);
	    						
								// Randomise this ped with attributes and different cothes and shit
								CPedVariationMgr::SetVarRandom(pThisPed);
								CPedPropsMgr::SetPedPropsRandomly(pThisPed, 0.4f, 0.25f);

	  
/* rage_materials
								if(IsSunbather(NewPedOccupation) && fwRandom::GetRandomNumber()&3)
	                            {   
	                                // Test for surfaces to sunbathe on
	                                CGtaIntersection colPoint;
	                                phInst* pColInst=0;
	                                CWorldProbe::ProcessVerticalLine(vecResult+Vector3(0,0,2),vecResult.z-2,0,colPoint,pColInst,phArchetypeGta::GTA_ALL_MAP_TYPES,NULL);
	                                if(pColInst)
	                                {
	                                    if(g_surfaceInfos.IsBeach(colPoint.GetSurfaceTypeB()) || 
	                                    	colPoint.GetSurfaceTypeB() == SURFACE_TYPE_CONCRETE_BEACH || 
	                                    	colPoint.GetSurfaceTypeB() == SURFACE_TYPE_PARKGRASS ||
	                                    	CCheat::IsCheatActive(CCheat::BEACHPARTY_CHEAT))
	                                    {
	                                        if(CTaskComplexSunbathe::CanSunbathe() || CCheat::IsCheatActive(CCheat::BEACHPARTY_CHEAT))
	                                        {
	                                        	bIsSunbather=true;
	                                                
                                                // Search for any other ped in a range of 1 metres to avoid towel overlaps.
                                                CEntity* hitPeds[2]={0,0};
                                                int NumHitPeds=2;
                                                if(CPedPlacement::IsPositionClearForPed(vecResult,SUNBATHER_TOWEL_SPACING,NumHitPeds,hitPeds))
                                                {
                                                    bool bHit=false;
                                                    int k;
                                                    for(k=0;k<NumHitPeds;k++)
                                                    {
                                                        if((hitPeds[k])&&(hitPeds[k]!=pThisPed))
                                                        {
                                                            bIsSunbather=false;
                                                        }
                                                    }
	                                            }
	                                            
	                                            if (colPoint.GetSurfaceTypeB() != SURFACE_TYPE_PARKGRASS)
		                                           		bCreateBeachToys = true;
	                                        }
	                                    }
	                                }
	                            }*/

/*
	                            if(bIsSunbather)
	                            {
	                            	Vector3		pos(pThisPed->GetPosition());
			                        CGtaIntersection	TestColPoint;
									phInst		*TestInst;
	                            	
                              		// check slope is not too steep
      								if (CWorldProbe::ProcessVerticalLine(pos+Vector3(0.0f,0.0f,10.0f), -10.0f, 0, TestColPoint,	TestInst, phArchetypeGta::GTA_ALL_MAP_TYPES))
									{
	                               		if (DotProduct(TestColPoint.GetNormal(), Vector3(0.0f, 0.0f, 1.0f)) > 0.95f)
                                      	{
	                               			const float fHeading=TWO_PI*fwRandom::GetRandomNumberInRange(0.0f,1.0f);
	                                		pThisPed->m_fDesiredHeading=fHeading;
	                                		pThisPed->m_fCurrentHeading=fHeading;    
	                                		pThisPed->SetHeading(fHeading);
	                                
											//	                                
			                                // "Towels on beach" stuff starts here:
											//
											
											//
											// make sure we have all needed models loaded:
											//
											
											CObject *pTowel = NULL;
											
			                               	if (bCreateBeachToys)
			                            	{			
		                            			pos.z = TestColPoint.GetPosition().z+0.04f;
			                            		pTowel = CWater::CreateBeachToy(pos, BEACHTOY_BEACHTOWEL_ANY);
			                            		
		    									if(pTowel)
		    									{
		    										float heading = fHeading;	
		    										pTowel->SetHeading(heading);
		    										
		    										// place towel on ground properly
		    										pTowel->GetMatrix().c = TestColPoint.GetNormal();
		    										pTowel->GetMatrix().a = CrossProduct(pTowel->GetMatrix().c, pTowel->GetMatrix().b);
		    										pTowel->GetMatrix().b = CrossProduct(pTowel->GetMatrix().c, pTowel->GetMatrix().a);
		    					
// rage		    										pTowel->UpdateRwMatrix();
//													pTowel->UpdateRwFrame();
											
													if((fwRandom::GetRandomNumber()&0x03)==0)
													{
														Vector3 toyPos = pos;
														toyPos += pTowel->GetMatrix().a * fwRandom::GetRandomNumberInRange(-0.5f, 0.5f);
														toyPos += pTowel->GetMatrix().b * fwRandom::GetRandomNumberInRange(-1.0f, 1.0f);
														CWater::CreateBeachToy(toyPos, BEACHTOY_LOTION);
													}
												}
		    								}
		    								
	                                		if(fwRandom::GetRandomNumber()&3)
			                                {    
			                                	// ped already lying down
			               						pThisPed->GetPedIntelligence()->AddTaskPrimary(rage_new CTaskComplexSunbathe(pTowel, false));
		                                    }
			                                else
			                                {
			                                	// ped standing up about to lie down                  
			    	       						pThisPed->GetPedIntelligence()->AddTaskPrimary(rage_new CTaskComplexSunbathe(pTowel, true));
			                              	}                                  
										}		                            	
	                            	}
                            
	                            }
	                            else
*/
	                            {					

	//     						       	pThisPed->GetPedIntelligence()->AddTaskDefault(CTaskComplexWander::GetWanderTaskByPedType(*pThisPed)); Obbe:This has already been done. Doing it again is slow because the wander nodes are calculated again.
      						       	CTheScripts::ScriptsForBrains.CheckIfNewEntityNeedsScript(pThisPed, CScriptsForBrains::PED_STREAMED);	//	, NULL);
	        				    }
	        					
	    					
	        					if (PedInGroup)		// Tell our rage_new ped he has a leader
	        					{
	        						//pThisPed->SetLeader(pFirstPed);
	        						//pThisPed->SetPedState(PED_HANG_OUT);
	        						//pFirstPed->SetPedState(PED_HANG_OUT);
	        					}
	        					else
	        					{
	        						pFirstPed = pThisPed;
	        					}
#ifdef ADD_TO_POP_DEBUG	        					
	        					iNoOfSuccesses4++;
#endif
	        					// set ped to invisible, so that they can be faded in
// rage	        					CVisibilityPlugins::SetClumpAlpha((RpClump*)pThisPed->GetRwObject(), 0);				
	        					
	        					if (PedInGroup+1 > NumPedsInGroup || NumPedsInGroup <= 1)
	        					    return bMorePedsNeeded;
	    				    }
	        				else
	        				{
	        					return bMorePedsNeeded;
	        				}
	    			    }
	        			else
	        			{
	        				return bMorePedsNeeded;
	                    }
	    		    }
	    		}
    		}
		}
	} 
	return bMorePedsNeeded;
}





// Name			:	AddPed
// Purpose		:	Adds a ped to the world
// Parameters	:	PedType - type of ped to add
//					PedOcc - which model to display for the ped
//					vecPosition - position to add the ped
// Returns		:	Nothing

CPed* CPopulation::AddPed(ePedType PedType, u32 PedOcc, const Vector3& vecPosition, bool bGiveDefaultTask)
{
	CPed *pPed;
	
	s32 RandomInt;
	eWeaponType weaponType;
	Matrix34 tempMat;
	tempMat.Identity();
	tempMat.d = vecPosition;

	// use pedfactory to create the ped:
	pPed = CPedFactory::GetFactory()->Create(PedType, PedOcc, &tempMat);
	ASSERT(pPed);


	// SetWanderPath() is called outside this function now. Not all peds need to find a path to
	// wander down eg peds in cars	
	switch(PedType)
	{
		case PEDTYPE_CIVMALE:
		case PEDTYPE_CIVFEMALE:
//			pPed = new CCivilianPed(PedType, PedOcc);
//			ASSERT(pPed);

			if(CCheat::IsCheatActive(CCheat::WEAPONSFORALL_CHEAT) || CCheat::IsCheatActive(CCheat::MAYHEM_CHEAT))
			{
				switch (fwRandom::GetRandomNumber() % 5)
				{
					case 0:
						RandomInt = WEAPONTYPE_PISTOL;
						break;
					case 1:
						RandomInt = WEAPONTYPE_BASEBALLBAT;
						break;
					case 2:
						RandomInt = WEAPONTYPE_SHOTGUN;
						break;
//					case 3:
//						RandomInt = WEAPONTYPE_M4;
//						break;
					default:
						RandomInt = WEAPONTYPE_ROCKETLAUNCHER;
						break;
				}
			
			
			
//				RandomInt = fwRandom::GetRandomNumberInRange(WEAPONTYPE_UNARMED, WEAPONTYPE_TEARGAS+1);

				if(RandomInt != WEAPONTYPE_UNARMED)
				{
					pPed->GiveDelayedWeapon((eWeaponType)RandomInt, 25001);	// > 25000 == infinite ammo
					pPed->SetCurrentWeapon(CWeaponInfo::GetWeaponInfo((eWeaponType)RandomInt)->m_nWeaponSlot);
				}
			}
			break;

		case PEDTYPE_GANG1:
		case PEDTYPE_GANG2:
		case PEDTYPE_GANG3:
		case PEDTYPE_GANG4:
		case PEDTYPE_GANG5:
		case PEDTYPE_GANG6:
		case PEDTYPE_GANG7:
		case PEDTYPE_GANG8:
		case PEDTYPE_GANG9:
		case PEDTYPE_GANG10:

			weaponType = WEAPONTYPE_UNARMED;
			
			{
				// give weapons to about 1/3 of gang members
				if (fwRandom::GetRandomNumberInRange(0, 100) < 33)
				{
					RandomInt = fwRandom::GetRandomNumberInRange(0, 100);
					
					if (CGangs::Gang[PedType - PEDTYPE_GANG1].ThirdWeapon != WEAPONTYPE_UNARMED)
					{
						if (RandomInt < 33)
							weaponType = CGangs::Gang[PedType - PEDTYPE_GANG1].FirstWeapon;
						else if (RandomInt < 66)
							weaponType = CGangs::Gang[PedType - PEDTYPE_GANG1].SecondWeapon;
						else
							weaponType = CGangs::Gang[PedType - PEDTYPE_GANG1].ThirdWeapon;
					}
					else if (CGangs::Gang[PedType - PEDTYPE_GANG1].SecondWeapon != WEAPONTYPE_UNARMED)
					{
						if (RandomInt < 50)
							weaponType = CGangs::Gang[PedType - PEDTYPE_GANG1].FirstWeapon;
						else
							weaponType = CGangs::Gang[PedType - PEDTYPE_GANG1].SecondWeapon;
					}
					else 
					{
						weaponType = CGangs::Gang[PedType - PEDTYPE_GANG1].FirstWeapon;
					}
				}
			}
				
			if (weaponType != WEAPONTYPE_UNARMED)
			{
				pPed->GiveDelayedWeapon(weaponType, 25001);	// > 25000 == infinite ammo
				pPed->SetCurrentWeapon(CWeaponInfo::GetWeaponInfo(weaponType)->m_nWeaponSlot);
			}
			
			break;

		default:
			break;
	}

	// Set ped position and add to world	
	pPed->SetPosition(vecPosition);
	pPed->SetOrientation(DtoR * (0.0f), DtoR * (0.0f), DtoR * (0.0f));
	CWorld::Add(pPed);

	// give default task
	if(bGiveDefaultTask)
	{
	    pPed->GetPedIntelligence()->AddTaskDefault(pPed->GetPedData()->ComputeWanderTask(*pPed));
		
		if(CCheat::IsCheatActive(CCheat::MAYHEM_CHEAT))
		{
			CPed* pClosest=NULL;
			float closestDist=FLT_MAX;
			
			// find closest ped for this one to attack
			CPed::Pool *pool = CPed::GetPool();
			CPed* pTestPed;
			int i;
			Vector3 dir;
			float dist;
			i=pool->GetSize();
			while(i--)
			{
				pTestPed = pool->GetSlot(i);
				if(pTestPed && pTestPed != pPed)
				{
					dir = pPed->GetPosition() - pTestPed->GetPosition();
					dist = dir.Mag2();
					
					if (dist < closestDist)
					{
						pClosest = pTestPed;
						closestDist = dist;
					}
				}
			}

			if(pClosest)
			{
				CEventAcquaintancePedHate eventHate(pClosest);	 
				eventHate.SetResponseTaskType(CTaskTypes::TASK_COMPLEX_KILL_PED_ON_FOOT);
				pPed->GetPedIntelligence()->AddEvent(eventHate);	
			}
		}
    }
    	
	CPopulation::UpdatePedCount(pPed, ADD_TO_POPULATION);

	return(pPed);
} // end - CPopulation::AddPed





// Name			:	RemovePed
// Purpose		:	Removes a ped from the world
// Parameters	:	pPed - ptr to the ped to remove
// Returns		:	Nothing

void CPopulation::RemovePed(CPed* pPed)
{
	// is ped in world?
	if (pPed->GetEntryInfoList().GetHeadPtr())
	{
		CPersistenceMgr::PopulationPedAboutToBeRemoved(pPed);	
		CWorld::Remove(pPed);
	}

	CPedFactory::GetFactory()->Destroy(pPed);
}







bool CPopulation::ArePedStatsCompatible(s32 pedStat1, s32 pedStat2)
{
	// none of these ped stat types want partners
	if (pedStat1==PEDSTAT_PLAYER 		|| pedStat2==PEDSTAT_PLAYER 		||
		pedStat1==PEDSTAT_COP    		|| pedStat2==PEDSTAT_COP    		||
		pedStat1==PEDSTAT_MEDIC  		|| pedStat2==PEDSTAT_MEDIC  		||
		pedStat1==PEDSTAT_FIRE  		|| pedStat2==PEDSTAT_FIRE  			||
		pedStat1==PEDSTAT_TRAMP_MALE  	|| pedStat2==PEDSTAT_TRAMP_MALE  	||
		pedStat1==PEDSTAT_TRAMP_FEMALE  || pedStat2==PEDSTAT_TRAMP_FEMALE  	||
		pedStat1==PEDSTAT_TOURIST 		|| pedStat2==PEDSTAT_TOURIST 		||
		pedStat1==PEDSTAT_PROSTITUTE  	|| pedStat2==PEDSTAT_PROSTITUTE  	||
		pedStat1==PEDSTAT_CRIMINAL  	|| pedStat2==PEDSTAT_CRIMINAL  		||
		pedStat1==PEDSTAT_BUSKER  		|| pedStat2==PEDSTAT_BUSKER  		||
		pedStat1==PEDSTAT_TAXIDRIVER 	|| pedStat2==PEDSTAT_TAXIDRIVER 	||
		pedStat1==PEDSTAT_PSYCHO  		|| pedStat2==PEDSTAT_PSYCHO  		||
		pedStat1==PEDSTAT_STEWARD  		|| pedStat2==PEDSTAT_STEWARD  		||
		pedStat1==PEDSTAT_SPORTSFAN  	|| pedStat2==PEDSTAT_SPORTSFAN  	||
		pedStat1==PEDSTAT_SHOPPER  		|| pedStat2==PEDSTAT_SHOPPER  		||
		pedStat1==PEDSTAT_OLDSHOPPER  	|| pedStat2==PEDSTAT_OLDSHOPPER  	||
		pedStat1==PEDSTAT_BEACH_GUY  	|| pedStat2==PEDSTAT_BEACH_GUY  	||
		pedStat1==PEDSTAT_BEACH_GIRL  	|| pedStat2==PEDSTAT_BEACH_GIRL  	||
		pedStat1==PEDSTAT_SKATER  		|| pedStat2==PEDSTAT_SKATER  		||
		pedStat1==PEDSTAT_STD_MISSION  	|| pedStat2==PEDSTAT_STD_MISSION  	||
		pedStat1==PEDSTAT_COWARD  		|| pedStat2==PEDSTAT_COWARD)
	{
		return false;
	}
		
	// no gang members want partners
	if ((pedStat1>=PEDSTAT_GANG1 && pedStat1<=PEDSTAT_GANG7) ||
		(pedStat2>=PEDSTAT_GANG1 && pedStat2<=PEDSTAT_GANG7))
	{
		return false;
	}
	
	// only group old people together
	if (pedStat1==PEDSTAT_OLD_GUY || pedStat1==PEDSTAT_OLD_GIRL)
	{
		// the other must be old 
		if (pedStat2==PEDSTAT_OLD_GUY || pedStat2==PEDSTAT_OLD_GIRL)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	// not coped with any of the following - they can be paired with each other ok
//	PEDSTAT_STREET_GUY
//	PEDSTAT_SUIT_GUY
//	PEDSTAT_SENSIBLE_GUY
//	PEDSTAT_GEEK_GUY
//	PEDSTAT_TOUGH_GUY
	
//	PEDSTAT_STREET_GIRL
//	PEDSTAT_SUIT_GIRL
//	PEDSTAT_SENSIBLE_GIRL
//	PEDSTAT_GEEK_GIRL
//	PEDSTAT_TOUGH_GIRL
	
	return true;
}

///////////////////////////////////////////////////////////////
// FUNCTION: PedMICanBeCreatedAtAttractor
//			 Works out whether a ped with this MI is suitable at an attractor

bool CPopulation::PedMICanBeCreatedAtAttractor(s32 MI)
{
	switch (((CPedModelInfo*)CModelInfo::GetModelInfo(MI))->GetDefaultPedType())
	{
//		case PEDTYPE_COP:		There are some attractors specifically for cops.
		case PEDTYPE_DEALER:
		case PEDTYPE_MEDIC:
		case PEDTYPE_FIRE:
		case PEDTYPE_CRIMINAL:
		case PEDTYPE_BUM:
		case PEDTYPE_PROSTITUTE:
			return false;
			break;
		default:
			break;
	}


	return true;
}

///////////////////////////////////////////////////////////////
// FUNCTION: PedMICanBeCreatedAtThisAttractor
//			 Works out whether a ped with this MI is suitable at an attractor

bool CPopulation::PedMICanBeCreatedAtThisAttractor(s32 MI, char *pBrain)
{
	if (pBrain == NULL) return true;
	ePedType PedType = ((CPedModelInfo*)CModelInfo::GetModelInfo(MI))->GetDefaultPedType();

		// The special attractors for cops.
	if (!stricmp(pBrain, "COPSIT") ||
		!stricmp(pBrain, "COPLOOK") ||
		!stricmp(pBrain, "BROWSE"))
	{
		if (PedType == PEDTYPE_COP)
		{
			return true;
		}
		return false;
	}


		// Cops don't belong on any other attractors
	if (PedType == PEDTYPE_COP)
	{
		return false;
	}

	return true;
}

 



///////////////////////////////////////////////////////////////
// FUNCTION: PedMICanBeCreatedInInterior
//			 Works out whether a ped with this MI should be created in interior

bool CPopulation::PedMICanBeCreatedInInterior(s32 MI)
{
	switch (((CPedModelInfo*)CModelInfo::GetModelInfo(MI))->GetDefaultPedType())
	{
		case PEDTYPE_COP:
		case PEDTYPE_DEALER:
		case PEDTYPE_MEDIC:
		case PEDTYPE_FIRE:
		case PEDTYPE_CRIMINAL:
		case PEDTYPE_BUM:
		case PEDTYPE_PROSTITUTE:
		case PEDTYPE_GANG1:
		case PEDTYPE_GANG2:
		case PEDTYPE_GANG3:
		case PEDTYPE_GANG4:
		case PEDTYPE_GANG5:
		case PEDTYPE_GANG6:
		case PEDTYPE_GANG7:
		case PEDTYPE_GANG8:
		case PEDTYPE_GANG9:
		case PEDTYPE_GANG10:
			return false;
			break;
		default:
			break;
	}

	return true;
}



s32 CPopulation::ChooseCivilianOccupation(bool bMustBeMale, bool bMustBeFemale, s32 MustHaveAnimGroup, s32 AvoidThisOccupation, s32 compatibleWithThisPedStat, bool bOnlyOnFoots, bool bAtAttractor, char *pAttractorScriptName)
{
// Pick the one ped that is appropriate and has the smallest number of occurences at the moment.
	
	s32 AppropriateMembers = gPopStreaming.GetAppropriateLoadedPeds().CountMembers();

	s32	LeastFrequentAppropriatePed = -1;
	s32	LeastFrequentAppropriateUsage = 10000;

	for (s32 i = 0; i < AppropriateMembers; i++)
	{
		s32 MI = gPopStreaming.GetAppropriateLoadedPeds().GetMember(i);
		ASSERT(CStreaming::HasObjectLoaded(MI, CModelInfo::GetStreamingModuleId()));
		ASSERT(MI >= 0);

		if (MI != AvoidThisOccupation &&
			((!bOnlyOnFoots) || ((CPedModelInfo *)CModelInfo::GetModelInfo(MI))->CanPedDriveCars(ON_FOOT)) )
		{
			if ( (!bMustBeMale) || IsMale(MI) )
			{
				if ( (!bMustBeFemale) || IsFemale(MI) )
				{
					if (MustHaveAnimGroup < 0 || MustHaveAnimGroup == ((CPedModelInfo*)CModelInfo::GetModelInfo(MI))->GetMotionAnimGroup())
					{
						if ( (!bAtAttractor) || (PedMICanBeCreatedAtAttractor(MI) && CPopulation::PedMICanBeCreatedAtThisAttractor(MI, pAttractorScriptName)))
						{
							if (compatibleWithThisPedStat <= -1 || ArePedStatsCompatible(((CPedModelInfo*)CModelInfo::GetModelInfo(MI))->GetDefaultPedStats(), compatibleWithThisPedStat))
							{
								if (((CPedModelInfo*)CModelInfo::GetModelInfo(MI))->GetNumRefs() < LeastFrequentAppropriateUsage)
								{
									LeastFrequentAppropriateUsage = ((CPedModelInfo*)CModelInfo::GetModelInfo(MI))->GetNumRefs();
									LeastFrequentAppropriatePed = MI;
								}
							}
						}
					}
				}
			}
		}
	}


	if (LeastFrequentAppropriatePed < 0)
	{
		return gPopStreaming.GetDefaultPedMI();
	}

	return LeastFrequentAppropriatePed;
}

void CPopulation::ChooseCivilianCoupleOccupations(s32& iPedMaleOccupation, s32& iPedFemaleOccupation)
{
	bool	bFirstIsMale = true;
	bool	bSecondIsMale = false;

    iPedMaleOccupation=ChooseCivilianOccupation(bFirstIsMale, bSecondIsMale, -1);

 	if (iPedMaleOccupation >= 0)
	{
 		iPedFemaleOccupation=ChooseCivilianOccupation(bSecondIsMale, bFirstIsMale, -1, iPedMaleOccupation, ((CPedModelInfo*)CModelInfo::GetModelInfo(iPedMaleOccupation))->GetDefaultPedStats());
	}

	if (iPedFemaleOccupation < 0)
	{
		iPedMaleOccupation = iPedFemaleOccupation = -1;
		return;
	}


	CPedModelInfo* pMaleModelInfo=(CPedModelInfo*)CModelInfo::GetModelInfo(iPedMaleOccupation);
	CPedModelInfo* pFemaleModelInfo=(CPedModelInfo*)CModelInfo::GetModelInfo(iPedFemaleOccupation);
	
	if((pMaleModelInfo->GetDefaultPedStats()!=PEDSTAT_SKATER)&&(pFemaleModelInfo->GetDefaultPedStats()!=PEDSTAT_SKATER))
	{
		return;
	}
	if((pMaleModelInfo->GetDefaultPedStats()==PEDSTAT_SKATER)&&(pFemaleModelInfo->GetDefaultPedStats()==PEDSTAT_SKATER))
	{
		return;
	}
	iPedMaleOccupation = iPedFemaleOccupation = -1;
	return;
}


s32 CPopulation::ChooseCivilianOccupationForVehicle(bool bMustBeMale, CVehicle *pVeh)
{
	ASSERT(pVeh);

#define MAXUSAGE (4)
	s32	Usage = 0;
//	s32	MI;

	CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetModelInfo(pVeh->GetModelIndex());
	s32	VehicleList = pModelInfo->GetVehicleList();

// Use a new strategy to create them.
// Start with the ones that aren't used yet.
// Then the ones that are only used once
// Twice and then stop.
	
			// On the first pass we check for identical peds in the same car.
			// On the seconds pass we don't.
	s32 AppropriateMembers = gPopStreaming.GetAppropriateLoadedPeds().CountMembers();
	for (s32 Pass = 0; Pass < 2; Pass++)
	{
		for (Usage = 0; Usage <= MAXUSAGE; Usage++)
		{
			for (s32 i = 0; i < AppropriateMembers; i++)
			{

				s32 MI = gPopStreaming.GetAppropriateLoadedPeds().GetMember(i);
				ASSERT(CStreaming::HasObjectLoaded(MI, CModelInfo::GetStreamingModuleId()));

				if (MI >= 0 && (Usage == MAXUSAGE || CModelInfo::GetModelInfo(MI)->GetNumRefs() == Usage))
				{
					if (((CPedModelInfo *)CModelInfo::GetModelInfo(MI))->CanPedDriveCars(VehicleList))
					{
						if ( (!bMustBeMale) || IsMale(MI) )
						{
							// If this ped is already in the car we only pick it if this is the last iteration
							if (Pass == 1)
							{
								return MI;
							}
							bool	bIdenticalPedInCar = false;
							if (pVeh->pDriver && pVeh->pDriver->GetModelIndex() == MI)
							{	
								bIdenticalPedInCar = true;
							}
							for (s32 P = 0; P < 3; P++)
							{
								if (pVeh->pPassengers[P] && pVeh->pPassengers[P]->GetModelIndex() == MI)
								{
									bIdenticalPedInCar = true;
								}
							}
							
							if (!bIdenticalPedInCar)
							{
								return MI;
							}
						}
					}
				}
			}
		}
	}
	
	return -1;



}



bool CPopulation::IsMale(const s32 iOccupation)
{
	CPedModelInfo* pModelInfo = ((CPedModelInfo*)CModelInfo::GetModelInfo(iOccupation));
	return pModelInfo->GetDefaultPedType() == PEDTYPE_CIVMALE;

}

bool CPopulation::IsFemale(const s32 iOccupation)
{
	CPedModelInfo* pModelInfo = ((CPedModelInfo*)CModelInfo::GetModelInfo(iOccupation));
	return pModelInfo->GetDefaultPedType() == PEDTYPE_CIVFEMALE;
}



bool CPopulation::IsSkateable(const Vector3& vPos)
{
    CGtaIntersection intersection;
    phInst* pColInst=0;
    CWorldProbe::ProcessVerticalLine(vPos+Vector3(0,0,2),vPos.z-2,0,intersection,pColInst,phArchetypeGta::GTA_ALL_MAP_TYPES);
    if(pColInst)
    {   
//        if(!IsSkateable(colPoint.GetSurfaceTypeB()))
/* rage_materials		if (!g_surfaceInfos.IsSkateable(intersection.GetSurfaceTypeB()))
        {
            return false;
        }*/
    }
    else
    {
        return false;
    }
    
    return true;
}
/*
bool CPopulation::IsSkateable(const u8 surfaceType)
{
    //if(COLPOINT_SURFACETYPE_DEFAULT==surfaceType)
    //{
    //    return true;
    //}
    if(COLPOINT_SURFACETYPE_TARMAC==surfaceType)
    {
        return true;
    }
    if(COLPOINT_SURFACETYPE_PAVEMENT==surfaceType)
    {
        return true;
    }
    return false;
}
*/


s32 CPopulation::ChooseGangOccupation(s32 gang)
{
	return CGangs::ChooseGangPedModel(gang);
}


CPed* CPopulation::AddPedInCar(CVehicle *pVehicle, bool bDriver, s32 CarRating, s32 seat_num, bool bMustBeMale, bool bCriminal)
{
	ePedType NewPedType;
	s32 NewPedOccupation;
	bool assumePedIsAvailable = true;
	Vector3 vecDriverPosition;
	int door = -1;
	Vector3 posn = FindPlayerCoors();

	// Find which zone the driver is in
	vecDriverPosition = pVehicle->GetPosition();

	// Specific vehicles have specific drivers.
	s32 DriverMI = -1; //FindSpecificDriverModelForCar_ToUse(pVehicle->GetModelIndex());

	if (bDriver && (DriverMI>=0) && CStreaming::HasObjectLoaded(DriverMI, CModelInfo::GetStreamingModuleId()))
	{		// Pick the driver specifically for this vehicle.
		NewPedOccupation = DriverMI;
		ASSERT(NewPedOccupation >= 0);
		NewPedType = ((CPedModelInfo*)CModelInfo::GetModelInfo(NewPedOccupation))->GetDefaultPedType();
	}
	else
	{
		if (pVehicle->GetModelIndex() == MI_CAR_AMBULANCE)
		{
			//NEEDS_MOVED_TO_NY_LAYER
			NewPedType = PEDTYPE_MEDIC;
			NewPedOccupation = MI_PED_MEDIC; //CStreaming::GetDefaultMedicModel();		
		}
		else if (pVehicle->GetModelIndex() == MI_CAR_FIRETRUCK)
		{		
			//NEEDS_MOVED_TO_NY_LAYER
			NewPedType = PEDTYPE_FIRE;
			NewPedOccupation = MI_PED_FIREMAN; //CStreaming::GetDefaultFiremanModel();
		}
		else
/*
			case MODELID_CAR_COPCAR_LA:
			case MODELID_CAR_COPCAR_SF:
			case MODELID_CAR_COPCAR_LV:
			case MODELID_CAR_COPCAR_RURAL:
			case MODELID_BOAT_PREDATOR:
			case MODELID_HELI_POLMAVERICK:
				//	Create a police ped
				
				//NEEDS_MOVED_TO_NY_LAYER
				NewPedType = PEDTYPE_COP;
				NewPedOccupation = COPTYPE_NORMAL;
				
				break;
			
			case MODELID_BIKE_POLICE:
				//	Create a swat ped
				
				//NEEDS_MOVED_TO_NY_LAYER
				NewPedType = PEDTYPE_COP;
				NewPedOccupation = COPTYPE_MOTORCYCLE;
				
				break;
			
			case MODELID_CAR_ENFORCER:
				//	Create a swat ped
				
				//NEEDS_MOVED_TO_NY_LAYER
				NewPedType = PEDTYPE_COP;
				NewPedOccupation = COPTYPE_SWAT;
				
				break;
			
			case MODELID_CAR_FBI:
				//	Create a fbi ped
				
				//NEEDS_MOVED_TO_NY_LAYER
				NewPedType = PEDTYPE_COP;
				NewPedOccupation = COPTYPE_FBI;
				
				break;
			
			case MODELID_CAR_BARRACKS:
			case MODELID_CAR_RHINO:
				//	Create a army ped
				
				//NEEDS_MOVED_TO_NY_LAYER
				NewPedType = PEDTYPE_COP;
				NewPedOccupation = COPTYPE_ARMY;
				
				break;
			


			case MODELID_CAR_STREAKCARRIAGE:
				
				//NEEDS_MOVED_TO_NY_LAYER
				NewPedOccupation = CPopulation::ChooseCivilianOccupation();
				if(NewPedOccupation == -1)
					NewPedOccupation = MODELID_DEFAULT_PED;
				NewPedType = ((CPedModelInfo*)CModelInfo::GetModelInfo(NewPedOccupation))->GetDefaultPedType();
				
				break;
				
	// done using FindSpecificDriverModelForCar()
			case MODELID_CAR_TAXI:	
			case MODELID_CAR_CABBIE:
	//		case MODELID_CAR_BORGNINE:
	//		case MODELID_CAR_KAUFMAN:
			    //The car is a taxi. Only attempt to make a taxi driver ped if the ped will
			    //be the driver of the taxi.  This prevents taxis having taxi drivers
			    //as passengers. 
			    if(bDriver)
			    {
			        //Only get to here if the taxi has no driver.  The new ped is allowed to 
			        //be a taxi driver.
	    	   	    //if(CGeneral::GetRandomTrueFalse())
	    		    {
	    		    	NewPedOccupation = MODELID_PED_DEFAULT_CABBIE;
	    		    	NewPedType = PEDTYPE_CIVMALE;
	    		    	break;
	    	        }   
			    }
			default:
*/
//JumpbackIn:
		{
			assumePedIsAvailable = false;
//				for(i=0; i<NUM_OF_GANGS; i++)
//				{
//					if(pVehicle->GetModelIndex() == CPopulation::GetCarGroupCar(POPCYCLE_TYPEGROUPS+i, 0)) // CGangs::Gang[i].VehicleModelIndex)
//					{
//						NewPedType = (ePedType)(PEDTYPE_GANG1 + i);
//						// not using GangIndexOfLoadedPedModel because its too bloody complex
//						NewPedOccupation = ChooseGangOccupation(i);
//						break;
//					}
//				}
//				if(i != NUM_OF_GANGS)
//					break;

			if (CarRating >= GANG1_CAR && CarRating <= GANG10_CAR)
			{
				NewPedType = (ePedType)(PEDTYPE_GANG1 + (CarRating - GANG1_CAR));
				NewPedOccupation = ChooseGangOccupation(CarRating - GANG1_CAR);
			}
			else
			{
				//	Create a normal civilian. 
				ASSERT(pVehicle->GetModelIndex() >= 0);
//				CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetModelInfo(pVehicle->GetModelIndex());

				ASSERT(CPopCycle::m_pCurrZone);

				NewPedOccupation = ChooseCivilianOccupationForVehicle(bMustBeMale, pVehicle);
//				NewPedOccupation = ChooseCivilianOccupation(bCriminal);

				if (NewPedOccupation < 0)
				{
					NewPedOccupation = gPopStreaming.GetDefaultPedMI();
				}
/*
					s32 count = MAX_NUM_PEDS_LOADED;
					while(count--)
					{
						if (NewPedOccupation < 0) NewPedOccupation = MODELID_DEFAULT_PED;

						//Check that the model has loaded ok.
						ASSERT(NewPedOccupation >= 0);
						if((CModelInfo::GetModelInfo(NewPedOccupation)->GetRwObject()))  
						{
							//Check if the model is already used for one of the car passengers or driver.
							//We aim to avoid duplicates in cars.
							if((!pVehicle->IsPassenger(NewPedOccupation))&&(!pVehicle->IsDriver(NewPedOccupation)))
							{
			        			ASSERT(NewPedOccupation >= 0);
								if(((CPedModelInfo*)CModelInfo::GetModelInfo(NewPedOccupation))->CanPedDriveCars(pModelInfo->GetVehicleList()))
		       					{
		       						//The ped is driving and the model can drive so accept the ped model.
		   	    					break;      
		   						}
							}
					}
					NewPedOccupation = ChooseCivilianOccupation(bCriminal);
					}
					// If couldn't find a ped then use the default ped
					if(count == -1)
						NewPedOccupation = defaultPed;
	*/

				ASSERT(NewPedOccupation >= 0);
				NewPedType = ((CPedModelInfo*)CModelInfo::GetModelInfo(NewPedOccupation))->GetDefaultPedType();
			}
		}
	}

	if (NewPedOccupation < 0) NewPedOccupation = gPopStreaming.GetDefaultPedMI();

	// If ped hasn't loaded yet just generate the default ped
	ASSERT(NewPedOccupation >= 0);
// rage	if(assumePedIsAvailable == false && 
// rage		CModelInfo::GetModelInfo(NewPedOccupation)->GetRwObject() == NULL)
//	{
//		NewPedOccupation = defaultPed;
//		NewPedType = ((CPedModelInfo*)CModelInfo::GetModelInfo(NewPedOccupation))->GetDefaultPedType();
//	}	
		
	// Add ped
	CPed *pPed = AddPed(NewPedType, NewPedOccupation, vecDriverPosition, false);
//	CPed *pPed = CPedFactory::GetFactory()->Create(PEDTYPE_CIVMALE,NewPedOccupation);
	
	// Set ped in car
	if (seat_num < 0)
	{
		door = 0;
	}
	else
	{
		door = CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(*pVehicle, seat_num);
	}
	CCarEnterExit::SetPedInCarDirect(pPed, pVehicle, door, bDriver);

	if (bCriminal)
	{
		// force the ped to be a criminal and adjust the population accordingly
		UpdatePedCount(pPed, SUBTRACT_FROM_POPULATION);
		pPed->SetPedType(PEDTYPE_CRIMINAL);
		UpdatePedCount(pPed, ADD_TO_POPULATION);
	}
	
	return(pPed); 

}

CPed* CPopulation::AddExistingPedInCar(CPed *pPed, CVehicle* UNUSED_PARAM(pVehicle))
{
/*	
	CPed *pPed = AddPed(NewPedType, NewPedOccupation, vecDriverPosition);
	
	pPed->SetUsesCollision(false);
	pPed->m_nCurrentWeapon = 0;
	CAnimManager::AddAnimation((RpClump*)pPed->m_pRwObject, ANIM_STD_PED, ANIM_STD_CAR_SIT);
*/
	return(pPed);

}


void CPopulation::UpdatePedCount(CPed *pPed, u8 AddOrSub)	// ePedType PedType
{
	if (AddOrSub == ADD_TO_POPULATION)
	{
		if (!pPed->m_PedAiFlags.bHasBeenAddedToPopulation)
		{
			pPed->m_PedAiFlags.bHasBeenAddedToPopulation = true;
			switch(pPed->GetPedType())
			{
				case PEDTYPE_CIVMALE:
					ms_nNumCivMale++;
					break;

				case PEDTYPE_CIVFEMALE:
					ms_nNumCivFemale++;
					break;

				case PEDTYPE_COP:
					ms_nNumCop++;
					break;

				case PEDTYPE_MEDIC:
				case PEDTYPE_FIRE:
					ms_nNumEmergency++;
					break;

				case PEDTYPE_GANG1:
				case PEDTYPE_GANG2:
				case PEDTYPE_GANG3:
				case PEDTYPE_GANG4:
				case PEDTYPE_GANG5:
				case PEDTYPE_GANG6:
				case PEDTYPE_GANG7:
				case PEDTYPE_GANG8:
				case PEDTYPE_GANG9:
				case PEDTYPE_GANG10:
//					ms_nNumGang[pPed->GetPedType() - PEDTYPE_GANG1]++;
					ms_nNumCivMale++;
					break;

				case PEDTYPE_DEALER:
					ms_nNumCivMale++;
					break;

				case PEDTYPE_CRIMINAL :
					ms_nNumCivMale++;
					break;
					
				case PEDTYPE_PROSTITUTE :
					ms_nNumCivFemale++;
					break;
					
				case PEDTYPE_PLAYER1:
				case PEDTYPE_PLAYER2:
				case PEDTYPE_PLAYER_NETWORK:
				case PEDTYPE_BUM :
				case PEDTYPE_SPECIAL:
				case PEDTYPE_MISSION :
					break;
				default:
#ifndef FINAL				
					CDebug::DebugMessage("Unknown ped type, UpdatePedCount, Population.cpp");
#endif					
					break;
			}
		}
	}
	else	//	SUBTRACT_FROM_POPULATION
	{
		if (pPed->m_PedAiFlags.bHasBeenAddedToPopulation)
		{
			pPed->m_PedAiFlags.bHasBeenAddedToPopulation = false;

			switch(pPed->GetPedType())
			{
				case PEDTYPE_CIVMALE:
					ms_nNumCivMale--;
					Assertf(ms_nNumCivMale >= 0, "UpdatePedCount - ms_nNumCivMale is less than 0");
					break;

				case PEDTYPE_CIVFEMALE:
					ms_nNumCivFemale--;				
					Assertf(ms_nNumCivFemale >= 0, "UpdatePedCount - ms_nNumCivFemale is less than 0");
					break;

				case PEDTYPE_COP:
					ms_nNumCop--;
					Assertf(ms_nNumCop >= 0, "UpdatePedCount - ms_nNumCop is less than 0");
					break;

				case PEDTYPE_MEDIC:
				case PEDTYPE_FIRE:
					ms_nNumEmergency--;
					Assertf(ms_nNumEmergency >= 0, "UpdatePedCount - ms_nNumEmergency is less than 0");
					break;

				case PEDTYPE_GANG1:
				case PEDTYPE_GANG2:
				case PEDTYPE_GANG3:
				case PEDTYPE_GANG4:
				case PEDTYPE_GANG5:
				case PEDTYPE_GANG6:
				case PEDTYPE_GANG7:
				case PEDTYPE_GANG8:
				case PEDTYPE_GANG9:
				case PEDTYPE_GANG10:
					ms_nNumCivMale--;
					Assertf(ms_nNumCivMale >= 0, "UpdatePedCount - ms_nNumCivMale is less than 0");
//					ms_nNumGang[pPed->GetPedType() - PEDTYPE_GANG1]--;
//					Assertf(ms_nNumGang[pPed->GetPedType() - PEDTYPE_GANG1] >= 0, "UpdatePedCount - ms_nNumGang is less than 0");
					break;

				case PEDTYPE_DEALER:
					ms_nNumCivMale--;
					Assertf(ms_nNumCivMale >= 0, "UpdatePedCount - ms_nNumCivMale is less than 0");
					break;

				case PEDTYPE_CRIMINAL :
					ms_nNumCivMale--;
					Assertf(ms_nNumCivMale >= 0, "UpdatePedCount - ms_nNumCivMale is less than 0");
					break;
					
				case PEDTYPE_PROSTITUTE :
					ms_nNumCivFemale--;
					Assertf(ms_nNumCivFemale >= 0, "UpdatePedCount - ms_nNumCivFemale is less than 0");
					break;
					
				case PEDTYPE_PLAYER1:
				case PEDTYPE_PLAYER2:
				case PEDTYPE_PLAYER_NETWORK:
				case PEDTYPE_BUM :
				case PEDTYPE_SPECIAL:
				case PEDTYPE_MISSION :
					break;
				default:
#ifndef FINAL				
					CDebug::DebugMessage("Unknown ped type, UpdatePedCount, Population.cpp");
#endif					
					break;
			}
		}
	}
}










 
   
void CPopulation::PlaceGangMembers(const ePedType eGangPedType, const int iNoOfGangMembers, const Vector3& vCentrePos)
{   
    //Choose whether to place the gang chatting in a circle or walking in a formation.
 
	CPedGroupPlacer groupPlacer;
   	groupPlacer.PlaceGroup(eGangPedType,iNoOfGangMembers,vCentrePos,CPedGroupDefaultTaskAllocatorTypes::DEFAULT_TASK_ALLOCATOR_RANDOM);    
}







void CPopulation::PlaceCouple(const ePedType malePedType, const s32 iMaleOccupation, const ePedType femalePedType, const s32 iFemaleOccupation, const Vector3& vManPos)
{
    if((PEDTYPE_CIVMALE!=malePedType)||(PEDTYPE_CIVFEMALE!=femalePedType))
    {
        return;
    }


    
   //If the circle is visible to the camera and is near the camera 
    //then don't place the couple
    const float fRadius=1.5f;
	Vector3 copyBlooodyConsts = vManPos;
    if(gCamInterface.IsSphereVisible(copyBlooodyConsts,fRadius))
    {
        if( ((Vector3)(vManPos-FindPlayerPed()->GetPosition())).XYMag()<
            POPULATION_ADD_MIN_RADIUS)
	    {
		    return;
		}
	}
	
	//Test if the circle is clear of any obstructions.
	if(!CPedPlacement::IsPositionClearForPed(vManPos,CModelInfo::GetModelInfo(iMaleOccupation)->GetBoundingSphereRadius()))
	{
	    return;
	}

    //Find the ground that the man will stand on.
    bool bFoundGround;	
    const float fGroundZ=1.0f+CWorldProbe::FindGroundZFor3DCoord(vManPos.x,vManPos.y,vManPos.z+1.0f,&bFoundGround);
    if(!bFoundGround)
    {
        return;
    }
    const float z= MAX(vManPos.z,fGroundZ);	

	// check man model has loaded
// rage	if(CModelInfo::GetModelInfo(iMaleOccupation)->GetRwObject() == NULL)
//		return;
		
	CPed* pManPed = AddPed(malePedType,iMaleOccupation,Vector3(vManPos.x,vManPos.y,z), true);
	if(0==pManPed)
	    return;

	
    // set ped to invisible, so that the woman can be faded in.
// rage	CVisibilityPlugins::SetClumpAlpha((RpClump*)pManPed->GetRwObject(), 0);	
	
	// check model has loaded
// rage	if(CModelInfo::GetModelInfo(iFemaleOccupation)->GetRwObject() == NULL)
//		return;
	    
	CPed* pWomanPed=AddPed(femalePedType,iFemaleOccupation,Vector3(vManPos.x,vManPos.y,z), true);
	if(pWomanPed)
	{

	    const Vector2& vOffset2D=CTaskComplexFollowLeaderInFormation::ms_offsets.GetOffSet(1,0);
//	    pWomanPed->GetPedIntelligence()->AddTaskPrimary(new CTaskComplexFollowLeaderAnyMeans(NULL, pManPed,Vector3(vOffset2D.x,vOffset2D.y,0)));

		// JB : if speeds are too slow or too different, then don't create this couple - they will not
		// be able to keep up with each other well enough..
		float fWomanWalkSpeed = pWomanPed->GetWalkAnimSpeed(ANIM_MOVE_WALK);
		float fManWalkSpeed = pManPed->GetWalkAnimSpeed(ANIM_MOVE_WALK);
		if(fWomanWalkSpeed < 0.75f || fManWalkSpeed < 0.75f || rage::Abs(fWomanWalkSpeed - fManWalkSpeed) > 0.45f)
		{
			return;
		}
		
		if (fWomanWalkSpeed > fManWalkSpeed)
		{
			pManPed->GetPedIntelligence()->AddTaskPrimary(rage_new CTaskComplexBeInCouple(pWomanPed, false));
			pWomanPed->GetPedIntelligence()->AddTaskPrimary(rage_new CTaskComplexBeInCouple(pManPed, true));

	    }
	    else
	    {
			pWomanPed->GetPedIntelligence()->AddTaskPrimary(rage_new CTaskComplexBeInCouple(pManPed, false));
			pManPed->GetPedIntelligence()->AddTaskPrimary(rage_new CTaskComplexBeInCouple(pWomanPed, true));
	    }
	    
	    
#ifndef FINAL			
		char maleStat[64];
		char femaleStat[64];		
		CPedStats::GetPedStatNameFromId(pManPed->m_pPedStats->m_ePedStatType, maleStat);
		CPedStats::GetPedStatNameFromId(pWomanPed->m_pPedStats->m_ePedStatType, femaleStat);
		DEBUGLOG2("COUPLE INFO: Placed couple - %s with %s\n", maleStat, femaleStat);
#endif
	    
	    Vector3 vWomanPos=pManPed->GetPosition()+Vector3(vOffset2D.x,vOffset2D.y,0);
	    bool bFoundGround;	
        const float fGroundZ=1.0f+CWorldProbe::FindGroundZFor3DCoord(vWomanPos.x,vWomanPos.y,vWomanPos.z+1.0f,&bFoundGround);
        if(!bFoundGround)
        {
            RemovePed(pManPed);
            RemovePed(pWomanPed);
            return;
        }
        else
        {
            const float z= MAX(vWomanPos.z,fGroundZ);	
            pWomanPed->SetPosition(Vector3(vWomanPos.x, vWomanPos.y, z));
    
            //Test if any hit object isn't the man ped.         
            CEntity* pHitObjects[3]={0,0,0};
		    CPedPlacement::IsPositionClearForPed(vWomanPos,CModelInfo::GetModelInfo(iFemaleOccupation)->GetBoundingSphereRadius(),3,pHitObjects);
            int k;
            for(k=0;k<3;k++)
            {
                CEntity* pHitEntity=pHitObjects[k];
                if(pHitEntity)
                {  
                    if((pHitEntity)&&(pHitEntity!=pManPed)&&(pHitEntity!=pWomanPed))
                    {
                        RemovePed(pManPed);
                        RemovePed(pWomanPed);
                        return;
                    }
                }
            }
            
            // set ped to invisible, so that the woman can be faded in.
// rage    		CVisibilityPlugins::SetClumpAlpha((RpClump*)pWomanPed->GetRwObject(), 0);	
        }
    }
}

bool CPopulation::IsCorrectTimeOfDayForEffect(const C2dEffect* pEffect)
{	
	ASSERT(pEffect && ET_PEDQUEUE==pEffect->m_type);
	
	if(QT_ATM==pEffect->attr.q.m_type || 
	   QT_SEAT==pEffect->attr.q.m_type || 
	   QT_BUS_STOP==pEffect->attr.q.m_type || 
	   QT_PARK==pEffect->attr.q.m_type)
	{
		//Only use these during the day.
		const int iTime=CClock::GetGameClockHours();
		return ((iTime>8) && (iTime<20));
	}
	else 
	{
		//Use every other effect at all times of the day.
		return true;
	}
}

s32 CPopulation::GeneratePedsAtAttractors(const Vector3& Coors, float minRad, float maxRad, float minRadClose, float maxRadClose, s32 decisionMaker, s32 NumToCreate)
{
	s32 	PedsCreated = 0;
	s32	Tries = 0;

//	s32 numPedsCanCreate = FindNumberOfPedsWeCanPlaceOnBenches(&pedModelIndex);

	if (NumToCreate == 0) return 0;

	// Search for the objects in the area (include dummies since the objects may have been created as dummies this frame during entryexit)
	#define  	MAX_NEAR_ENTITIES 	(512)
	s32 		numNearEntities = -1;
	CEntity* 	pNearEntities[MAX_NEAR_ENTITIES];
//	Vector3		playerPos = FindPlayerCentreOfWorld(CWorld::PlayerInFocus);		
	CWorld::FindObjectsInRange(Coors, maxRad, false, &numNearEntities, MAX_NEAR_ENTITIES, pNearEntities, true, false, false, true, false);

	while (NumToCreate > 0 && Tries < 12)
	{
		// search for an attractor in range - preferably on screen
		Tries++;

		// search through these looking for ones with interiors
		for (s32 i=0; i<numNearEntities; i++)
		{	
			if (NumToCreate==0)
			{
				return PedsCreated;
			}
		
/* rage
			if (pNearEntities[i]->GetRwObject() == NULL)
			{
				// we're not interested in ones which haven't loaded in yet
				continue;
			}
			
			if (CGame::currArea != pNearEntities[i]->GetAreaCode())
			{
				// we're not interested in ones which aren't in our area code
				continue;
			}
*/
			C2dEffect* pEffect = pNearEntities[i]->GetRandom2dEffect(ET_PEDQUEUE, true);

			if (pEffect && IsCorrectTimeOfDayForEffect(pEffect))
			{
				// Don't generate peds at disabled attractors
				if(pEffect->attr.q.m_flags & PEDQUEUE_DISABLED)
				{
			        if(!pNearEntities[i]->GetIsTypeObject() || 
			        	!((CObject *)pNearEntities[i])->m_nObjectFlags.bEnableDisabledAttractors)
			        	continue;
			    }   
			    
				// get the distance from the player to the attractor
				Vector3 effectPos = pEffect->m_posn;
				effectPos = pNearEntities[i]->GetMatrix() * effectPos;
				Vector3 vec = Coors - effectPos;
				float dist = vec.Mag();
					
				float minDist, maxDist;	
				if (gCamInterface.IsSphereVisible(effectPos, 2.0f))
				{
					minDist = minRad;
					maxDist = maxRad;
				}
				else
				{
					minDist = minRadClose;
					maxDist = maxRadClose;
				}
				
				if (dist>minDist && dist<maxDist)
				{
					s32 MI;
					
// rage					if (!bInPoliceStation || fwRandom::GetRandomNumberInRange(0, 100) > 70 /*rage|| !PedMICanBeCreatedAtThisAttractor(MODELID_COP_LA_PED, pEffect->attr.q.m_scriptName)*/)
					{
						MI = CPopulation::ChooseCivilianOccupation(false, false, -1, -1, -1, true, true, pEffect->attr.q.m_scriptName);
					}
//					else
//					{
//						MI = MODELID_COP_LA_PED;
//						decisionMaker = CDecisionMakerTypes::DECISION_MAKER_PED_COP;
//					}

					if (MI > 0 && MI != gPopStreaming.GetDefaultPedMI())
					{				
						if (AddPedAtAttractor(MI, pEffect, effectPos, pNearEntities[i], decisionMaker))
						{
							PedsCreated++;
						
							if (decisionMaker == - 1)
							{
								// these must be ambient peds being added - only add the one
								return PedsCreated;
							}	
							NumToCreate--;
							Tries = 0;
	//						numPedsCanCreate = FindNumberOfPedsWeCanPlaceOnBenches(&pedModelIndex);
						}
					}	
				}
			}
		}
	}
	return PedsCreated;
}


bool8 CPopulation::AddPedAtAttractor(s32 pedModelIndex, C2dEffect* pEffect, const Vector3& effectPos, CEntity* pEntity, s32 decisionMaker)
{
	Assertf(pEntity, "Entity ptr is null");
	Assertf(!pEntity->GetIsTypeDummy(), "Entity is null or is type dummy");

	//Check that area around the effect pos is clear of other peds.
	//A ped could have immediately finishing using the attractor at this pos and end up stuck
	//inside the next ped generated at the attractor.
	
	//Compute the closest ped to effectPos.
	CPed* pClosestPed=0;
//	float fClosest2=std::numeric_limits<float>::max();
	float fClosest2 = 99999.9f;
	CPed::Pool *pool = CPed::GetPool();
	CPed* pPed;
	s32 i=pool->GetSize();	
	while(i--)
	{
		pPed = pool->GetSlot(i);
		if(pPed)
		{	
			Vector3 vDiff;
			vDiff = effectPos-pPed->GetPosition();
			vDiff.z=0;
			const float f2=vDiff.Mag2();
			if(f2<fClosest2)
			{
				fClosest2=f2;
				pClosestPed=pPed;
			}
		}
	}		
	if(fClosest2 > (PED_RADIUS*PED_RADIUS))
	{	
		if (GetPedAttractorManager()->HasQueueTailArrivedAtSlot(pEffect, pEntity))
		//(0==GetPedAttractorManager()->GetPedUsingEffect(pEffect,pEntity))
		{
			// add a ped at this attractor
			ePedType pedType = ((CPedModelInfo*)CModelInfo::GetModelInfo(pedModelIndex))->GetDefaultPedType();
			CPed* pPed = CPopulation::AddPed(pedType, pedModelIndex, effectPos, false);
			if (pPed)
			{			
				CPopulation::UpdatePedCount(pPed, ADD_TO_POPULATION);	

				// Randomise this ped with attributes and different cothes and shit
				CPedVariationMgr::SetVarRandom(pPed);
				CPedPropsMgr::SetPedPropsRandomly(pPed, 0.4f, 0.25f);

				pPed->SetCharCreatedBy(RANDOM_CHAR);	
				if(decisionMaker == -1)
				{
					decisionMaker = CDecisionMakerTypes::DECISION_MAKER_PED_RANDOM1;
				}
				pPed->GetPedIntelligence()->SetPedDecisionMakerType(decisionMaker);	
				pPed->GetPedIntelligence()->AddTaskDefault(pPed->GetPedData()->ComputeWanderTask(*pPed));
				CPedAttractorPedPlacer::PlacePedAtEffect(*pEffect, pEntity, pPed, 0.02f);
				pPed->m_PedAiFlags.bUseAttractorInstantly = true;
											
				CEventAttractor event(pEffect, pEntity, true);
	            event.SetResponseTaskType(CTaskTypes::TASK_COMPLEX_USE_EFFECT);
	            pPed->GetPedIntelligence()->AddEvent(event);
	            pPed->GetPedIntelligence()->ProcessEventHandler();
	            pPed->GetPedIntelligence()->Process();
	            //ASSERT(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_USE_EFFECT));
		
				return true;
			}
		}
	}
	
	return false;
}









//
// name:		NetworkRegisterAllPeds
// description:	Registers all peds with the network object manager when a new network
//				game starts.
//
#ifdef ONLINE_PLAY
void CPopulation::NetworkRegisterAllPeds()
{
	CPed::Pool *pool = CPed::GetPool();
	CPed		*pPed;

	for (s32 i=0; i<pool->GetSize(); i++)
	{
		pPed = pool->GetSlot(i);

		if(pPed && 
		   pPed->GetPedType() != PEDTYPE_PLAYER1 &&
		   pPed->GetPedType() != PEDTYPE_PLAYER2)
		{
			const PlayerId playerID = (const PlayerId)CWorld::GetMainPlayer();
			CNetwork::GetObjectManager().RegisterObject(pPed, playerID);
		}
	}
}
#endif
