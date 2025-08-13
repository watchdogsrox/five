/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    zonecull.h
// PURPOSE : Stuff for culling models on the map
// AUTHOR :  Obbe.
// CREATED : 14/1/01
//
/////////////////////////////////////////////////////////////////////////////////
#include "renderer\zonecull.h"

#include "system\new.h"

// Rage headers
#include "bank/bkmgr.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths\random.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "scene\FileLoader.h"
#include "system\filemgr.h"
#include "modelinfo\ModelInfo.h"
#include "peds/Ped.h"
#include "renderer\Renderer.h"
#include "vehicles\Train.h"
#include "game\Zones.h"
#include "scene\world\gameWorld.h"


s32		CCullZones::NumAttributeZones;
s32		CCullZones::NumMirrorAttributeZones;
//s32		CCullZones::NumTunnelAttributeZones;
s32		CCullZones::CurrentFlags_Camera;
s32		CCullZones::CurrentFlags_Player;
bool		CCullZones::bRenderCullzones = false;
//bool		CCullZones::bMilitaryZonesDisabled = false;
vfxList		CCullZones::m_camZoneList;
s32		CCullZones::m_numCamZones;

CAttributeZone			CCullZones::aAttributeZones[MAX_NUM_ATTRIBUTE_ZONES];
//CAttributeZone			CCullZones::aTunnelAttributeZones[MAX_NUM_TUNNEL_ATTRIBUTE_ZONES];
CMirrorAttributeZone	CCullZones::aMirrorAttributeZones[MAX_NUM_MIRROR_ATTRIBUTE_ZONES];




/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Init
// PURPOSE :  Initialises the cullzones.
/////////////////////////////////////////////////////////////////////////////////

void CCullZones::Init(unsigned /*initMode*/)
{
	NumAttributeZones = 0;
	CurrentFlags_Camera = CurrentFlags_Player = 0;

//#if __BANK
//	bkBank* pBank = &BANKMGR.CreateBank("Zones", 0, 0, false);
//
//	// flags
//	pBank->AddToggle("Render Zones", &bRenderCullzones, NullCB);
//	pBank->AddSlider("Num Cam Zones", &m_numCamZones, 0, 9000000, 0);
//
//#endif // __BANK

}


///////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Updates the Cull zones.
///////////////////////////////////////////////////////////////////////////////

void CCullZones::Update(void)
{
	#define	UPDATE_DIST			(10.0f)											// update if the camera has moved more than this since it's last update
	#define UPDATE_DIST_SQ		(UPDATE_DIST*UPDATE_DIST)
	#define ADD_TO_LIST_RANGE	(30.0f)											// add to the list if within this range from the camera
	
	// build up a list of zones close to the camera (update every 10m moved)
	for (s32 vp=0; vp < gVpMan.GetNumViewports(); vp++)		// iterate over all the viewports
	{
		CViewport *pVp = gVpMan.GetViewport(vp);
		if (pVp && pVp->IsActive() && pVp->IsGame())
		{					
			CViewportGame *pVpScene = static_cast<CViewportGame*>(pVp);
			Vector3	currPos = pVpScene->GetFrame().GetPosition();
			float distToPrevUpdatePosSq = (currPos-pVpScene->GetLastZoneUpdatePos()).Mag2();

			if (distToPrevUpdatePosSq>UPDATE_DIST_SQ)
			{
				pVpScene->GetLastZoneUpdatePos() = currPos;
				m_camZoneList.RemoveAll();

				for (s32 zone=0; zone<NumAttributeZones; zone++)
				{
					Vector3	min, max;

					aAttributeZones[zone].ZoneDef.FindBoundingBox(&min, &max);

					if (currPos.x > min.x - ADD_TO_LIST_RANGE &&
						currPos.y > min.y - ADD_TO_LIST_RANGE &&
						currPos.z > min.z - ADD_TO_LIST_RANGE &&
						currPos.x < max.x + ADD_TO_LIST_RANGE &&
						currPos.y < max.y + ADD_TO_LIST_RANGE &&
						currPos.z < max.z + ADD_TO_LIST_RANGE)
					{
						// add if not in list already
						bool foundInList = false;
						CAttributeZone* pZone = (CAttributeZone*)m_camZoneList.GetHead();
						while (pZone)
						{
							if (pZone == &aAttributeZones[zone])
							{
								foundInList = true;
								break;
							}
							pZone = (CAttributeZone*)m_camZoneList.GetNext(pZone);
						}

						if (!foundInList)
						{
							m_camZoneList.AddItem(&aAttributeZones[zone]);
						}
					}
				}
			}
		}
	}

	m_numCamZones = m_camZoneList.GetNumItems();


	Vector3	TestCoors;

		// Don't do everything every frame
	switch (fwTimer::GetSystemFrameCount() & 7)
	{

		case 2:	// Update the attribute zone for the camera
			TestCoors = camInterface::GetPos();

			CurrentFlags_Camera = FindAttributesForCoors(TestCoors);


			break;
		case 4:
			break;

			
		case 6:	// Update the attribute zone for the player
			TestCoors = CGameWorld::FindLocalPlayerCoors();
			CurrentFlags_Player = FindAttributesForCoors(TestCoors);	// &CurrentWantedLevelDrop_Player);
			
//			if ( (!bMilitaryZonesDisabled) && (CurrentFlags_Player & ZONE_FLAGS_MILITARYAREA) )
//			{
//				if (!FindPlayerPed()->IsDead())
//				{
//					FindPlayerPed()->SetWantedLevelNoDrop(WANTED_LEVEL5);
//				}
//			}
			
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ForceCullZoneCoors
// PURPOSE :  Will force a certain cullzone until the next update is done.
//			  This will be used by Adam to stream a certain area in.
/////////////////////////////////////////////////////////////////////////////////

//void CCullZones::ForceCullZoneCoors(Vector3 Coors)
//{
//}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindCullZoneForCoors
// PURPOSE :  Goes through the cull zones and find out what we're in
/////////////////////////////////////////////////////////////////////////////////

/*
s32 CCullZones::FindCullZoneForCoors(Vector3 TestCoors)
{
	s32	Zone;

	Zone = 0;
	while (Zone < NumCullZones &&
			(TestCoors.x < aZones[Zone].MinX || TestCoors.x > aZones[Zone].MaxX || TestCoors.y < aZones[Zone].MinY || TestCoors.y > aZones[Zone].MaxY || TestCoors.z <aZones[Zone].MinZ || TestCoors.z > aZones[Zone].MaxZ) )
	{
		Zone++;
	}

	if (Zone == NumCullZones)
	{		// We didn't find a zone
		Zone = -1;
	}
	return (Zone);
}
*/

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindCenter
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

Vector3 CZoneDef::FindCenter()
{
	return Vector3(CornerX + Vec1X * 0.5f + Vec2X * 0.5f, CornerY + Vec1Y * 0.5f + Vec2Y * 0.5f, (MinZ + MaxZ) * 0.5f);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindBoundingBox
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CZoneDef::FindBoundingBox(Vector3 *pMin, Vector3 *pMax)
{
	pMin->x = (float)(CornerX + MIN(Vec1X, 0) + MIN(Vec2X, 0));
	pMax->x = (float)(CornerX + MAX(Vec1X, 0) + MAX(Vec2X, 0));
	pMin->y = (float)(CornerY + MIN(Vec1Y, 0) + MIN(Vec2Y, 0));
	pMax->y = (float)(CornerY + MAX(Vec1Y, 0) + MAX(Vec2Y, 0));
	pMin->z = MinZ;
	pMax->z = MaxZ;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsPointWithin
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

bool CZoneDef::IsPointWithin(const Vector3& TestCoors)
{
	if (TestCoors.z > MinZ && TestCoors.z < MaxZ)
	{
		float TestX = TestCoors.x - CornerX;
		float TestY = TestCoors.y - CornerY;
		float DotPr1 = TestX * Vec1X + TestY * Vec1Y;
		if (DotPr1 >= 0.0f && DotPr1 <= (Vec1X*Vec1X + Vec1Y*Vec1Y))
		{
			float DotPr2 = TestX * Vec2X + TestY * Vec2Y;
			if (DotPr2 >= 0.0f && DotPr2 <= (Vec2X*Vec2X + Vec2Y*Vec2Y))
			{
				return true;
			}
		}
	}
	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindAttributesForCoors
// PURPOSE :  Goes through the cull zones and finds out what the attributes are
/////////////////////////////////////////////////////////////////////////////////

s32 CCullZones::FindAttributesForCoors(const Vector3& TestCoors)
{
	u32 flags = 0;

	CAttributeZone* pZone = (CAttributeZone*)m_camZoneList.GetHead();
	while (pZone)
	{
		if (pZone->ZoneDef.IsPointWithin(TestCoors))
		{
			flags |= pZone->Flags;
		}
		pZone = (CAttributeZone*)m_camZoneList.GetNext(pZone);
	}

	return flags;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindTunnelAttributesForCoors
// PURPOSE :  Goes through the cull zones and finds out what the tunnel attributes are
/////////////////////////////////////////////////////////////////////////////////

/*
s32 CCullZones::FindTunnelAttributesForCoors(const Vector3& TestCoors)
{
	s32	Zone;
	s32	Flags = 0;

	for (Zone = 0; Zone < NumTunnelAttributeZones; Zone++)
	{
		if (aTunnelAttributeZones[Zone].ZoneDef.IsPointWithin(TestCoors))
		{
			Flags |= aTunnelAttributeZones[Zone].Flags;
		}
	}
	return Flags;
}
*/

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindMirrorAttributesForCoors
// PURPOSE :  Goes through the mirror attribute zones and finds out what the attributes are
/////////////////////////////////////////////////////////////////////////////////

CMirrorAttributeZone *CCullZones::FindMirrorAttributesForCoors(const Vector3& TestCoors)
{
	for (s32 Zone = 0; Zone < NumMirrorAttributeZones; Zone++)
	{
		if (aMirrorAttributeZones[Zone].ZoneDef.IsPointWithin(TestCoors))
		{
			return (&aMirrorAttributeZones[Zone]);
		}
	}
	return NULL;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindZoneWithCamStairsAttribute
// PURPOSE :  Goes through the cull zones and finds the one with CAM_STAIRS attribute
/////////////////////////////////////////////////////////////////////////////////

CAttributeZone *CCullZones::FindZoneWithStairsAttributeForPlayer()
{
	Vector3	TestCoors = CGameWorld::FindLocalPlayerCoors();
	CAttributeZone* pZone = (CAttributeZone*)m_camZoneList.GetHead();
	while (pZone)
	{
		if (pZone->Flags & ZONE_FLAGS_STAIRS)
		{
			if (pZone->ZoneDef.IsPointWithin(TestCoors))
			{
				return pZone;
			}
		}
		pZone = (CAttributeZone*)m_camZoneList.GetNext(pZone);
	}

	return NULL;
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddCullZone
// PURPOSE :  Add one cullzone. Works out what models are visible etc
/////////////////////////////////////////////////////////////////////////////////

void CCullZones::AddCullZone(const Vector3& posn, float ArgVec1X, float ArgVec1Y, float ArgMinZ, float ArgVec2X, float ArgVec2Y, float ArgMaxZ, u16 ArgFlags, s16 UNUSED_PARAM(ArgWantedLevelDrop))
{
	// Zones that have tunnel attributes are added to another list.
//	if (ArgFlags & ZONE_FLAGS_TUNNELTRANSITION || ArgFlags & ZONE_FLAGS_TUNNEL)
//	{
//		AddTunnelAttributeZone(posn, ArgVec1X, ArgVec1Y, ArgMinZ, ArgVec2X, ArgVec2Y, ArgMaxZ, ArgFlags);

		ArgFlags &= ~ZONE_FLAGS_TUNNELTRANSITION;
		ArgFlags &= ~ZONE_FLAGS_TUNNEL;
//	}

	if (ArgFlags)
	{
		// There are only attribute zones now
		Assertf(NumAttributeZones < MAX_NUM_ATTRIBUTE_ZONES-1, "Too many attribute zones");

		aAttributeZones[NumAttributeZones].ZoneDef.CornerX = s16(posn.x - ArgVec1X - ArgVec2X);
		aAttributeZones[NumAttributeZones].ZoneDef.CornerY = s16(posn.y - ArgVec1Y - ArgVec2Y);
		aAttributeZones[NumAttributeZones].ZoneDef.Vec1X = s16(ArgVec1X * 2.0f);
		aAttributeZones[NumAttributeZones].ZoneDef.Vec1Y = s16(ArgVec1Y * 2.0f);
		aAttributeZones[NumAttributeZones].ZoneDef.MinZ = s16(ArgMinZ);
		aAttributeZones[NumAttributeZones].ZoneDef.Vec2X = s16(ArgVec2X * 2.0f);
		aAttributeZones[NumAttributeZones].ZoneDef.Vec2Y = s16(ArgVec2Y * 2.0f);
		aAttributeZones[NumAttributeZones].ZoneDef.MaxZ = s16(ArgMaxZ);
		aAttributeZones[NumAttributeZones].Flags = ArgFlags;
//		aAttributeZones[NumAttributeZones].WantedLevelDrop = ArgWantedLevelDrop;

		Assert(aAttributeZones[NumAttributeZones].ZoneDef.Vec1X || aAttributeZones[NumAttributeZones].ZoneDef.Vec1Y);
		Assert(aAttributeZones[NumAttributeZones].ZoneDef.Vec2X || aAttributeZones[NumAttributeZones].ZoneDef.Vec2Y);

		NumAttributeZones++;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddTunnelAttributeZone
// PURPOSE :  Add one tunnel attribute zone.
/////////////////////////////////////////////////////////////////////////////////

/*
void CCullZones::AddTunnelAttributeZone(const Vector3& posn, float ArgVec1X, float ArgVec1Y, float ArgMinZ, float ArgVec2X, float ArgVec2Y, float ArgMaxZ, u16 ArgFlags)
{
	// There are only attribute zones now
	Assertf(NumTunnelAttributeZones < MAX_NUM_TUNNEL_ATTRIBUTE_ZONES-1, "Too many tunnel attribute zones");

	aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.CornerX = s16(posn.x - ArgVec1X - ArgVec2X);
	aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.CornerY = s16(posn.y - ArgVec1Y - ArgVec2Y);
	aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.Vec1X = s16(ArgVec1X * 2.0f);
	aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.Vec1Y = s16(ArgVec1Y * 2.0f);
	aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.MinZ = s16(ArgMinZ);
	aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.Vec2X = s16(ArgVec2X * 2.0f);
	aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.Vec2Y = s16(ArgVec2Y * 2.0f);
	aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.MaxZ = s16(ArgMaxZ);
	aTunnelAttributeZones[NumTunnelAttributeZones].Flags = ArgFlags;

	Assert(aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.Vec1X || aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.Vec1Y);
	Assert(aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.Vec2X || aTunnelAttributeZones[NumTunnelAttributeZones].ZoneDef.Vec2Y);

	NumTunnelAttributeZones++;
}
*/


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddMirrorAttributeZone
// PURPOSE :  Add one mirror attribute zone.
/////////////////////////////////////////////////////////////////////////////////

void CCullZones::AddMirrorAttributeZone(const Vector3& posn, float ArgVec1X, float ArgVec1Y, float ArgMinZ, float ArgVec2X, float ArgVec2Y, float ArgMaxZ, u16 ArgFlags, float ArgMirrorV, float ArgMirrorNormalX, float ArgMirrorNormalY, float ArgMirrorNormalZ)
{

	// There are only attribute zones now
	Assertf(NumMirrorAttributeZones < MAX_NUM_MIRROR_ATTRIBUTE_ZONES-1, "Too many mirror attribute zones");

	aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.CornerX = s16(posn.x - ArgVec1X - ArgVec2X);
	aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.CornerY = s16(posn.y - ArgVec1Y - ArgVec2Y);
	aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.Vec1X = s16(ArgVec1X * 2.0f);
	aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.Vec1Y = s16(ArgVec1Y * 2.0f);
	aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.MinZ = s16(ArgMinZ);
	aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.Vec2X = s16(ArgVec2X * 2.0f);
	aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.Vec2Y = s16(ArgVec2Y * 2.0f);
	aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.MaxZ = s16(ArgMaxZ);
	aMirrorAttributeZones[NumMirrorAttributeZones].Flags = (s8)ArgFlags;
	aMirrorAttributeZones[NumMirrorAttributeZones].MirrorV = ArgMirrorV;
	aMirrorAttributeZones[NumMirrorAttributeZones].MirrorNormalX = (s8)(ArgMirrorNormalX * 100.0f);
	aMirrorAttributeZones[NumMirrorAttributeZones].MirrorNormalY = (s8)(ArgMirrorNormalY * 100.0f);
	aMirrorAttributeZones[NumMirrorAttributeZones].MirrorNormalZ = (s8)(ArgMirrorNormalZ * 100.0f);

	Assert(aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.Vec1X || aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.Vec1Y);
	Assert(aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.Vec2X || aMirrorAttributeZones[NumMirrorAttributeZones].ZoneDef.Vec2Y);

	NumMirrorAttributeZones++;
}


















/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CamCloseInForPlayer
// PURPOSE :  Returns true if the camera should be closer to the player (underground).
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::CamCloseInForPlayer(void)
{
//	if (CurrentAttributeZone_Player < 0) return false;
//	if (aAttributeZones[CurrentAttributeZone_Player].Flags & ZONE_FLAGS_CAMCLOSEIN) return true;

	if (CurrentFlags_Player & ZONE_FLAGS_CAMCLOSEIN) return true;
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CamStairsForPlayer
// PURPOSE :  Returns true if the camera should be closer to the player (underground).
//			  On stairs. Cam should be close in and up a bit
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::CamStairsForPlayer(void)
{
//	if (CurrentAttributeZone_Player < 0) return false;
//	if (aAttributeZones[CurrentAttributeZone_Player].Flags & ZONE_FLAGS_STAIRS) return true;

	if (CurrentFlags_Player & ZONE_FLAGS_STAIRS) return true;
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Cam1stPersonForPlayer
// PURPOSE :  Returns true if the camera should be first person.
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::Cam1stPersonForPlayer(void)
{
//	if (CurrentAttributeZone_Player < 0) return false;
//	if (aAttributeZones[CurrentAttributeZone_Player].Flags & ZONE_FLAGS_1STPERSON) return true;

	if (CurrentFlags_Player & ZONE_FLAGS_1STPERSON) return true;
	return false;
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : NoPolice
// PURPOSE :  Returns true if there should be no police.
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::NoPolice(void)
{
//	if (CurrentAttributeZone_Player < 0) return false;
//	if (aAttributeZones[CurrentAttributeZone_Player].Flags & ZONE_FLAGS_NOPOLICE) return true;

	if (CurrentFlags_Player & ZONE_FLAGS_NOPOLICE) return true;
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PoliceAbandonCars
// PURPOSE :  Returns true if police should only pursue on foot.
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::PoliceAbandonCars(void)
{
	if (CurrentFlags_Player & ZONE_FLAGS_POLICEABANDONCARS) return true;
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InRoomForAudio
// PURPOSE :  If we're in a room for audio purposes the reverb should be switched off.
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::InRoomForAudio(void)
{
	if (CurrentFlags_Camera & ZONE_FLAGS_INROOMFORAUDIO) return true;
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FewerCars
// PURPOSE :  Locally reduce the number of cars (to 60% or so)
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::FewerCars(void)
{
	if (CurrentFlags_Player & ZONE_FLAGS_FEWERCARS) return true;
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FewerPeds
// PURPOSE :  Locally reduce the number of peds (to 60% or so)
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::FewerPeds(void)
{
	if (CurrentFlags_Player & ZONE_FLAGS_FEWERPEDS) return true;
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : WaterFudge
// PURPOSE :  If this bit is set we don't want to render Sandy's local water effect.
/////////////////////////////////////////////////////////////////////////////////

//bool CCullZones::WaterFudge(void)
//{
//	if (CurrentFlags_Camera & ZONE_FLAGS_WATERFUDGE) return true;
//	return false;
//}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : DoINeedToLoadCollision
// PURPOSE : 
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::DoINeedToLoadCollision(void)
{
//	if (CurrentAttributeZone_Player < 0) return false;
//	if (aAttributeZones[CurrentAttributeZone_Player].Flags & ZONE_FLAGS_LOADCOLLISION) return true;

	if (CurrentFlags_Player & ZONE_FLAGS_LOADCOLLISION) return true;
	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CamCloseInForCoors
// PURPOSE :  Returns true if the camera should be closer to the player (underground).
/////////////////////////////////////////////////////////////////////////////////

// THIS FUNCTION WILL BE REMOVED ONCE MARK HAS TAKEN IT OUT
//bool CCullZones::CamCloseInForCoors(Vector3 TestCoors)
//{
//	return false;
//}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CamNoRain
// PURPOSE :  Returns true if we don't want rain here.
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::CamNoRain(void)
{
//	if (CurrentAttributeZone_Camera < 0) return false;
//	if (aAttributeZones[CurrentAttributeZone_Camera].Flags & ZONE_FLAGS_NORAIN) return true;

	if (CurrentFlags_Camera & ZONE_FLAGS_NORAIN) return true;
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlayerNoRain
// PURPOSE :  Returns true if we don't want rain at the players coordinates.
/////////////////////////////////////////////////////////////////////////////////

bool CCullZones::PlayerNoRain(void)
{
//	if (CurrentAttributeZone_Player < 0) return false;
//	if (aAttributeZones[CurrentAttributeZone_Player].Flags & ZONE_FLAGS_NORAIN) return true;

	if (CurrentFlags_Player & ZONE_FLAGS_NORAIN) return true;
	return false;
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetWantedLevelDrop
// PURPOSE :  Returns true if we don't want rain here.
/////////////////////////////////////////////////////////////////////////////////

/*
s32 CCullZones::GetWantedLevelDrop(void)
{
//	if (CurrentAttributeZone_Player < 0) return 0;
//	return (aAttributeZones[CurrentAttributeZone_Player].WantedLevelDrop);

	return (CurrentWantedLevelDrop_Player);
}
*/

bool CCullZones::DoExtraAirResistanceForPlayer(void)
{
	if (CurrentFlags_Player & ZONE_FLAGS_EXTRAAIRRESISTANCE) return true;
	return false;
}



// NOTE: Points have to be in clockwise order seen from above

bool IsPointWithinArbitraryArea(float TestPointX, float TestPointY,
								float Point1X, float Point1Y,
								float Point2X, float Point2Y,
								float Point3X, float Point3Y,
								float Point4X, float Point4Y)
{
	if ((TestPointX - Point1X) * (Point2Y - Point1Y) - (TestPointY - Point1Y) * (Point2X - Point1X) < 0.0f) return false;
	if ((TestPointX - Point2X) * (Point3Y - Point2Y) - (TestPointY - Point2Y) * (Point3X - Point2X) < 0.0f) return false;
	if ((TestPointX - Point3X) * (Point4Y - Point3Y) - (TestPointY - Point3Y) * (Point4X - Point3X) < 0.0f) return false;
	if ((TestPointX - Point4X) * (Point1Y - Point4Y) - (TestPointY - Point4Y) * (Point1X - Point4X) < 0.0f) return false;
	
	return true;
}								


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateAtBeachForAudio
// PURPOSE :  Finds out whether the camera is near the beach.
//			  If this is the case Raymond will play an ambient sample involving waves.
/////////////////////////////////////////////////////////////////////////////////

/*
void CCullZones::UpdateAtBeachForAudio(void)
{
	bAtBeachForAudio = IsPointWithinArbitraryArea(camInterface::GetPos()->x, camInterface::GetPos()->y,
//								262.0f, -1510.0f,
//								580.0f, 1250.0f,
//								1500.0f, 1450.0f,
//								1200.0f, -1725.0f);
								400.0f,	-1644.4f,
								751.9f,	1267.8f,
								971.9f,	1216.2f,
								840.0f,	-1744.0f);
}
*/


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Render
// PURPOSE :  Renders one zone.
//
/////////////////////////////////////////////////////////////////////////////////

#if __BANK

void CZoneDef::Render(const Vector3& CamPos, CRGBA Colour)
{
	#define VIS_DIST	(300.0f)
	Vector3	MinCoors, MaxCoors;
	
	FindBoundingBox(&MinCoors, &MaxCoors);
		
	if (CamPos.x > MinCoors.x - VIS_DIST &&
		CamPos.y > MinCoors.y - VIS_DIST &&
		CamPos.z > MinCoors.z - VIS_DIST &&
		CamPos.x < MaxCoors.x + VIS_DIST &&
		CamPos.y < MaxCoors.y + VIS_DIST &&
		CamPos.z < MaxCoors.z + VIS_DIST)
	{
		grcDebugDraw::Line(Vector3((float)CornerX, (float)CornerY, (float)MinZ),										Vector3((float)(CornerX + Vec1X), (float)(CornerY + Vec1Y), (float)MinZ), Colour);
		grcDebugDraw::Line(Vector3((float)CornerX, (float)CornerY, (float)MinZ),										Vector3((float)(CornerX + Vec2X), (float)(CornerY + Vec2Y), (float)MinZ), Colour);
		grcDebugDraw::Line(Vector3((float)(CornerX + Vec1X), (float)(CornerY + Vec1Y), (float)MinZ),					Vector3((float)(CornerX + Vec2X + Vec1X), (float)(CornerY + Vec2Y + Vec1Y), (float)MinZ), Colour);
		grcDebugDraw::Line(Vector3((float)(CornerX + Vec2X), (float)(CornerY + Vec2Y), (float)MinZ),					Vector3((float)(CornerX + Vec2X + Vec1X), (float)(CornerY + Vec2Y + Vec1Y), (float)MinZ), Colour);
																														
		grcDebugDraw::Line(Vector3((float)CornerX, (float)CornerY, (float)MaxZ),										Vector3((float)(CornerX + Vec1X), (float)(CornerY + Vec1Y), (float)MaxZ), Colour);
		grcDebugDraw::Line(Vector3((float)CornerX, (float)CornerY, (float)MaxZ),										Vector3((float)(CornerX + Vec2X), (float)(CornerY + Vec2Y), (float)MaxZ), Colour);
		grcDebugDraw::Line(Vector3((float)(CornerX + Vec1X), (float)(CornerY + Vec1Y), (float)MaxZ),					Vector3((float)(CornerX + Vec2X + Vec1X), (float)(CornerY + Vec2Y + Vec1Y), (float)MaxZ), Colour);
		grcDebugDraw::Line(Vector3((float)(CornerX + Vec2X), (float)(CornerY + Vec2Y), (float)MaxZ),					Vector3((float)(CornerX + Vec2X + Vec1X), (float)(CornerY + Vec2Y + Vec1Y), (float)MaxZ), Colour);
																														
		grcDebugDraw::Line(Vector3((float)CornerX, (float)CornerY, (float)MinZ),										Vector3((float)CornerX, (float)CornerY, (float)MaxZ), Colour);
		grcDebugDraw::Line(Vector3((float)(CornerX + Vec1X), (float)(CornerY + Vec1Y), (float)MinZ),					Vector3((float)(CornerX + Vec1X), (float)(CornerY + Vec1Y), (float)MaxZ), Colour);
		grcDebugDraw::Line(Vector3((float)(CornerX + Vec2X), (float)(CornerY + Vec2Y), (float)MinZ),					Vector3((float)(CornerX + Vec2X), (float)(CornerY + Vec2Y), (float)MaxZ), Colour);
		grcDebugDraw::Line(Vector3((float)(CornerX + Vec1X + Vec2X), (float)(CornerY + Vec1Y + Vec2Y), (float)MinZ),	Vector3((float)(CornerX + Vec1X + Vec2X), (float)(CornerY + Vec1Y + Vec2Y), (float)MaxZ), Colour);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Render
// PURPOSE :  Renders some debug shit for them.
//
/////////////////////////////////////////////////////////////////////////////////

void CCullZones::Render()
{
	if (bRenderCullzones)
	{
		const Vector3& camPos = camInterface::GetPos();

		CAttributeZone* pZone = (CAttributeZone*)m_camZoneList.GetHead();
		while (pZone)
		{
			pZone->ZoneDef.Render(camPos, CRGBA(255, 0, 0, 255));
			pZone = (CAttributeZone*)m_camZoneList.GetNext(pZone);
		}
		for (s32 z = 0; z < NumAttributeZones; z++)
		{
			aAttributeZones[z].ZoneDef.Render(camPos, CRGBA(255, 255, 255, 255));
		}
/*
		for (s32 z = 0; z < NumTunnelAttributeZones; z++)
		{
			aTunnelAttributeZones[z].ZoneDef.Render(camPos, CRGBA(0, 255, 255, 255));
		}
*/
		for (s32 z = 0; z < NumMirrorAttributeZones; z++)
		{
			aMirrorAttributeZones[z].ZoneDef.Render(camPos, CRGBA(255, 255, 0, 255));
		}
	}
}
#endif // __BANK




