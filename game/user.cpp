// Title	:	user.cpp
// Author	:	Graeme
// Started	:	27.06.00

#include <stdio.h>

// Framework headers
#include "fwvehicleai\pathfindtypes.h"

// Game headers
#include "cutscene/CutSceneManagerNew.h"
#include "user.h"
#include "control\gamelogic.h"
#include "debug\debug.h"
#include "frontend/VideoEditor/ui/Playback.h"
#include "modelinfo\VehicleModelInfo.h"
#include "peds/Ped.h"
#include "peds/popzones.h"
#include "peds/PlayerInfo.h"
#include "scene\world\gameWorld.h"
#include "vehicles\Automobile.h"
#include "vehicleAi\pathfind.h"

#include "Text\TextConversion.h"

#define TIME_TO_DISPLAY_PLACE_NAME		(250)

CPlaceName CUserDisplay::AreaName;
CPlaceName CUserDisplay::DistrictName;
CStreetName CUserDisplay::StreetName;
#ifdef DISPLAY_PAGER
	CPager CUserDisplay::Pager;
#endif
COnscreenTimer CUserDisplay::OnscnTimer;
CCurrentVehicle CUserDisplay::CurrentVehicle;

#include "data/aes_init.h"
AES_INIT_4;

CPlaceName::CPlaceName(void)
{
	Init(false);
}


void CPlaceName::Init(bool bUseLocalNavZones)
{
	cOldName[0] = 0;
	m_cName[0] = 0;
	m_NameTextKey[0] = 0;
	pNavZone = NULL;
	Timer = 0;
	m_bUseLocalNavZones = bUseLocalNavZones;
}


const char *CPlaceName::GetForMap(float x, float y)
{
	//Vector3 PlayerPos;
	CPopZone *CurrNavZone = NULL;
	Vector3 CursorPos;

	CursorPos.x = x;
	CursorPos.y = y;
	CursorPos.z = 0;//PlayerPos.z;  // get z pos from current player position (only way to do it as far as i can see)
	
	if (m_bUseLocalNavZones)
	{
		CurrNavZone = CPopZones::FindSmallestForPosition(&CursorPos, ZONECAT_LOCAL_NAVIGATION, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
	}
	else
	{
		CurrNavZone = CPopZones::FindSmallestForPosition(&CursorPos, ZONECAT_NAVIGATION, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
	}
	
	return CurrNavZone ? CurrNavZone->GetTranslatedName() : "";
}

void CPlaceName::Process(void)
{
	Vector3 PlayerPos;
	CPopZone *CurrNavZone;

	if ((fwTimer::GetSystemFrameCount() & 31) == 17)
	{
		const CVehicle* vehicle = CGameWorld::FindLocalPlayerVehicle();
		if (vehicle)
		{
			PlayerPos = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition());
		}
		else
		{
			PlayerPos = CGameWorld::FindLocalPlayerCoors();
		}

		if (m_bUseLocalNavZones)
		{
			CurrNavZone = CPopZones::FindSmallestForPosition(&PlayerPos, ZONECAT_LOCAL_NAVIGATION, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
		}
		else
		{
			CurrNavZone = CPopZones::FindSmallestForPosition(&PlayerPos, ZONECAT_NAVIGATION, NetworkInterface::IsGameInProgress() ? ZONETYPE_MP : ZONETYPE_SP);
		}
		if (CurrNavZone == NULL)
		{
			pNavZone = NULL;
		}

		if (CurrNavZone != pNavZone || pNavZone == NULL)
		{
			if (CurrNavZone && pNavZone && (CurrNavZone->m_associatedTextId.GetHash() == pNavZone->m_associatedTextId.GetHash()) )
			{	//	Previous and current zones share the same text label

			}
			else
			{	//	Zones are different/have different text labels
				pNavZone = CurrNavZone;
			}
		}

		Display();
	}
}

void CPlaceName::ForceSetToDisplay()
{
	cOldName[0] = 1; // force it to be a new game
	Timer = TIME_TO_DISPLAY_PLACE_NAME;  // time for fading
}


bool CPlaceName::IsNewName()
{
	if ( strcmp(cOldName, m_cName) != 0 )
	{
		safecpy(cOldName, m_cName);
		return true;
	}
	
	return false;
}


void CPlaceName::Display(void)
{
	if (pNavZone)
	{
		if (!CPauseMenu::IsActive() || CVideoEditorPlayback::IsActive() )
		{
			const bool c_bOnIslandMap = CMiniMap::GetIsOnIslandMap();
			const char *pTextKey = pNavZone->m_associatedTextId.TryGetCStr();
			const u32 c_uIslandLocationHash = 3629398693;

			if(!c_bOnIslandMap && pNavZone->m_id.GetHash() == c_uIslandLocationHash)
			{
				safecpy(m_cName, TheText.Get("Oceana"));
			}
			else
			{
				safecpy(m_cName, pNavZone->GetTranslatedName());
			}

			if (pTextKey)
			{
				if(!c_bOnIslandMap && pNavZone->m_id.GetHash() == c_uIslandLocationHash)
				{
					safecpy(m_NameTextKey, TheText.Get("Oceana"), NELEM(m_NameTextKey));
				}
				else
				{
					safecpy(m_NameTextKey, pTextKey, NELEM(m_NameTextKey));
				}
			}
			else
			{
				safecpy(m_NameTextKey, "", NELEM(m_NameTextKey));
			}
		}
		else
		{

			safecpy(m_cName, GetForMap(CMiniMap::GetPauseMapCursor().x, CMiniMap::GetPauseMapCursor().y));
		}
	}
	else
	{
		m_cName[0] = 0;
		m_NameTextKey[0] = 0;
	}
}


CCurrentVehicle::CCurrentVehicle(void)
{
	Init();
}

void CCurrentVehicle::Init(void)
{
	cOldName[0] = 0;
	cName[0] = 0;
}

bool CCurrentVehicle::IsNewName()
{
	if ( strcmp(cOldName, cName) != 0 )
	{
		safecpy(cOldName, cName);
		return true;
	}

	return false;
}

void CCurrentVehicle::Process(void)
{
	Display();
}

void CCurrentVehicle::Display(void)
{
	CVehicle *pCurrentVehicle = CGameWorld::FindLocalPlayerVehicle(true);

	if (pCurrentVehicle)
	{
		const char *AsciiMake;
		const char *AsciiModel;

		CBaseModelInfo* pModelInfo;

		pModelInfo = pCurrentVehicle->GetBaseModelInfo();
		if(AssertVerify(pModelInfo) && AssertVerify(pModelInfo->GetModelType() == MI_TYPE_VEHICLE))
		{
			AsciiMake = ((CVehicleModelInfo*) pModelInfo)->GetVehicleMakeName();
			AsciiModel = ((CVehicleModelInfo*) pModelInfo)->GetGameName();
			bool hasMake = AsciiMake && (AsciiMake[0] != '\0');
			bool hasModel = AsciiModel && (AsciiModel[0] != '\0');
	
			cName[0] = '\0';
	
			if(hasMake)
			{
				safecpy(cName, TheText.Get(AsciiMake));	
			}
	
			if(hasMake && hasModel)
			{
				safecat( cName, " " );
			}
	
			if(hasModel)
			{
				safecat( cName, TheText.Get(AsciiModel) );
			}
		}
	}
	else
	{
		cOldName[0] = cName[0] = '\0';
	}
}













CStreetName::CStreetName(void)
{
	Init();
}


void CStreetName::Init(void)
{
	for (s32 c = 0; c < NUM_REMEMBERED_STREET_NAMES; c++)
	{
		RememberedStreetName[c] = 0;
		TimeInRange[c] = 0;
	}

	cOldName[0] = 0;
	m_cName[0] = 0;
	m_HashOfNameTextKey = 0;
}


bool CStreetName::IsNewName()
{
	if ( strcmp(cOldName, m_cName) != 0 )
	{
		safecpy(cOldName, m_cName);
		return true;
	}

	return false;
}


char const *CStreetName::GetForMap(float x, float y)
{
	Vector3	Coors(x,y,0.0f);

#define NUM_NODES (32)
	CNodeAddress	aNodes[NUM_NODES];
	s32 NodesFound = ThePaths.RecordNodesInCircle(Coors, 30.0f, NUM_NODES, aNodes, false, false, false);
	u32	nearestStreetName = 0;
	float	nearestDistance = 9999.9f;

	for (s32 node = 0; node < NodesFound; node++)
	{
		u32 StreetName = ThePaths.FindNodePointer(aNodes[node])->m_streetNameHash;

		if (StreetName != 0)
		{
			Vector3	nodeCoors;
			ThePaths.FindNodePointer(aNodes[node])->GetCoors(nodeCoors);
			float	dist = (Coors - nodeCoors).Mag();
			if (dist < nearestDistance)
			{
				nearestDistance = dist;
				nearestStreetName = StreetName;
			}
		}
	}

	return nearestStreetName ? TheText.Get(nearestStreetName, "Streetname") : "";
}



void CStreetName::Process(void)
{

	// Only do this once in a while
	if ((fwTimer::GetSystemFrameCount() & 31) == 19)
	{
		Vector3	Coors = CGameWorld::FindLocalPlayerCoors();
	
		// Find all nodes within printing range.
		#define NUM_NODES (32)
		CNodeAddress	aNodes[NUM_NODES];
		s32 NodesFound = ThePaths.RecordNodesInCircle(Coors, 20.0f, NUM_NODES, aNodes, false, false, false);
		bool bFoundStreet = false;

		for (s32 node = 0; node < NodesFound; node++)
		{
			Assert(ThePaths.IsRegionLoaded(aNodes[node]));
			u32 StreetName = ThePaths.FindNodePointer(aNodes[node])->m_streetNameHash;

			if (StreetName != 0)
			{
				s32 freeSlot = -1;
				s32 slot;
				for (slot = 0; slot < NUM_REMEMBERED_STREET_NAMES; slot++)
				{
					if (RememberedStreetName[slot] == StreetName)
					{
						bFoundStreet =  true;
						break;
					}
					else if (RememberedStreetName[slot] == 0)
					{
						freeSlot = slot;
					}
				}
				if (slot < NUM_REMEMBERED_STREET_NAMES)
				{		// We found it. Don't print it. Update time instead.
					TimeInRange[slot] = fwTimer::GetTimeInMilliseconds();
				}
				else
				{		// We didn't find this one. It is new->Print it.
					if (freeSlot < 0)
					{			// If there aren't any free slots->Remove the oldest one
						u32 Oldest = TimeInRange[0];
						freeSlot = 0;
						for (s32 s = 1; s < NUM_REMEMBERED_STREET_NAMES; s++)
						{
							if (TimeInRange[s] < Oldest)
							{
								Oldest = TimeInRange[s];
								freeSlot = s;
							}
						}
					}

					Assert(freeSlot >= 0);
					if (freeSlot >= 0)		// make sure we have a slot for it.
					{
						RememberedStreetName[freeSlot] = StreetName;
						TimeInRange[freeSlot] = fwTimer::GetTimeInMilliseconds();

						// Make sure it gets printed.
						if (CGameWorld::FindLocalPlayer()->GetPortalTracker()->GetCurrRoomHash() == 0)		// Don't print street names when we're inside an interior
						{
							safecpy(m_cName, TheText.Get(StreetName, "Streetname"));
							m_HashOfNameTextKey = StreetName;
							bFoundStreet =  true;
						}
					}
				}
			}
		}
		// Remove the street names that have expired.
		for (s32 s = 0; s < NUM_REMEMBERED_STREET_NAMES; s++)
		{
			if (RememberedStreetName[s] != 0 &&
				fwTimer::GetTimeInMilliseconds() > TimeInRange[s] + 30000)
			{
				RememberedStreetName[s] = 0;
			}
		}
		// If we didnt find a street clear strings
		if(!bFoundStreet)
		{
			// reset street name
			m_HashOfNameTextKey = 0;
			m_cName[0] = '\0';
		}
	}

	Display();
}

	// This bypasses the normal code and just finds the nearest node and uses its streetname.
	// This is much more likely to be the road the player is actually on
void CStreetName::UpdateStreetNameForDPadDown()
{
	CPed * pPlayerPed = FindPlayerPed();

	Vector3	Coors = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());

#define NUM_NODES (32)
	CNodeAddress	aNodes[NUM_NODES];
	s32 NodesFound = ThePaths.RecordNodesInCircle(Coors, 30.0f, NUM_NODES, aNodes, false, false, false);
	u32	nearestStreetName = 0;
	float	nearestDistance = 9999.9f;

	for (s32 node = 0; node < NodesFound; node++)
	{
		u32 StreetName = ThePaths.FindNodePointer(aNodes[node])->m_streetNameHash;

		if (StreetName != 0)
		{
			Vector3	nodeCoors;
			ThePaths.FindNodePointer(aNodes[node])->GetCoors(nodeCoors);
			float	dist = (Coors - nodeCoors).Mag();
			if (dist < nearestDistance)
			{
				nearestDistance = dist;
				nearestStreetName = StreetName;
			}
		}
	}

	if (nearestStreetName)
	{
		safecpy(m_cName, TheText.Get(nearestStreetName, "Streetname"));
	}
	else
	{
		m_cName[0] = '\0';
	}
}


void CStreetName::ForceSetToDisplay()
{

}


void CStreetName::Display(void)
{

}















void CUserDisplay::Init(unsigned /*initMode*/)
{
	AreaName.Init(false);
	DistrictName.Init(true);
	StreetName.Init();
	OnscnTimer.Init();
	CurrentVehicle.Init();
}

void CUserDisplay::Process(void)
{
#if __BANK
	if(!TheText.bShowTextIdOnly || (CutSceneManager::GetInstance() && !CutSceneManager::GetInstance()->IsCutscenePlayingBack()))
#endif //__BANK
	{
		AreaName.Process();
		DistrictName.Process();
		StreetName.Process();
		OnscnTimer.Process();
		CurrentVehicle.Process();
	}
}


/*void CUserDisplay::Display(void)
{
	PlaceName.Display();
}*/


