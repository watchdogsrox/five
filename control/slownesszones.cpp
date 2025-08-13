/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    slownesszones.h
// PURPOSE : Contains the zones on the map that slow cars down.
// AUTHOR :  Obbe.
// CREATED : 24/9/07
//
/////////////////////////////////////////////////////////////////////////////////

// rage headers
#include "fwsys/fileExts.h"
#include "grcore/debugdraw.h"
#include "bank/bank.h"
#include "parser/manager.h"
#include "parser/macros.h"

// game headers
#include "control/slownesszones.h"
#include "core/game.h"
#include "vehicles/vehicle.h"
#include "debug/vectormap.h"
#include "Control/GameLogic.h"
#include "Peds/Ped.h"
#include "Peds/PlayerInfo.h"

// Parser headers
#include "slownesszones_parser.h"
#include "Scene/Scene.h"


CSlownessZoneManager	CSlownessZoneManager::m_SlownessZonesManagerInstance;

#if __BANK
bool	CSlownessZoneManager::m_bDisplayDebugInfo = false;
bool	CSlownessZoneManager::m_bDisplayZonesOnVMap = false;
bool	CSlownessZoneManager::m_bModifySelectedZone = false;
Vector3 CSlownessZoneManager::m_currentZoneMax;
Vector3 CSlownessZoneManager::m_currentZoneMin;
int		CSlownessZoneManager::m_currentZone = -1;
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Init
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////

void CSlownessZoneManager::Init(unsigned initMode)
{
	if ( initMode == INIT_SESSION )
	{
	#if __BANK
		m_bDisplayDebugInfo = false;
		m_bDisplayZonesOnVMap = false;
		m_bModifySelectedZone = false;
		m_currentZone = -1;
		m_currentZoneMin.Zero();
		m_currentZoneMax.Zero();
	#endif
		// Potentially new map - reset data then load it all again
		Reload();
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Shutdown
// PURPOSE : Shutdown the instance clearing all data
/////////////////////////////////////////////////////////////////////////////////

void CSlownessZoneManager::Shutdown()
{

	Reset();
#if __BANK
	m_currentZone = -1;
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Reset
// PURPOSE : Clears all data
/////////////////////////////////////////////////////////////////////////////////
void CSlownessZoneManager::Reset()
{
	m_SlownessZonesManagerInstance.m_aSlownessZone.Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Load
// PURPOSE : Load slowness zone info from the respective level directory
/////////////////////////////////////////////////////////////////////////////////


void CSlownessZoneManager::Load()
{
	if(Verifyf(!m_SlownessZonesManagerInstance.m_aSlownessZone.GetCount(), "SlownessZones Info has already been loaded"))
	{
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::SLOWNESS_ZONES_FILE);
		if (DATAFILEMGR.IsValid(pData))
		{
			Verifyf(fwPsoStoreLoader::LoadDataIntoObject(pData->m_filename, META_FILE_EXT, m_SlownessZonesManagerInstance), "Failed to load %s", pData->m_filename);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////

void CSlownessZoneManager::Update()
{
#if __BANK
	if (m_bDisplayDebugInfo)
	{
		char debugText[50];

		// Go through the vehicle pool and for each vehicle print out whether it's in a slowness zone.
		CVehicle::Pool *VehiclePool = CVehicle::GetPool();
		CVehicle* pVehicle;
		s32 i = (s32) VehiclePool->GetSize();

		while(i--)
		{
			pVehicle = VehiclePool->GetSlot(i);
			if(pVehicle)
			{
				if (pVehicle->m_nVehicleFlags.bInSlownessZone)
				{
					sprintf(debugText, "Slowness");
				}
				else
				{
					sprintf(debugText, ".");
				}
				grcDebugDraw::Text(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) + Vector3(0.f,0.f,2.0f), CRGBA(255, 255, 255, 255), debugText);
			}
		}
	}
	atArray<CSlownessZone>	&aZones = m_SlownessZonesManagerInstance.m_aSlownessZone;

	// Disply the zones on the vector map.
	if (m_bDisplayZonesOnVMap)
	{

		for (s32 i = 0 ; i < aZones.GetCount(); i++)
		{
			CVectorMap::DrawLine(Vector3(aZones[i].m_bBox.GetMinVector3().x, aZones[i].m_bBox.GetMinVector3().y, 0.0f), Vector3(aZones[i].m_bBox.GetMaxVector3().x, aZones[i].m_bBox.GetMinVector3().y, 0.0f), Color32(255, 255, 255, 255), Color32(255, 255, 255, 255));
			CVectorMap::DrawLine(Vector3(aZones[i].m_bBox.GetMaxVector3().x, aZones[i].m_bBox.GetMinVector3().y, 0.0f), Vector3(aZones[i].m_bBox.GetMaxVector3().x, aZones[i].m_bBox.GetMaxVector3().y, 0.0f), Color32(255, 255, 255, 255), Color32(255, 255, 255, 255));
			CVectorMap::DrawLine(Vector3(aZones[i].m_bBox.GetMaxVector3().x, aZones[i].m_bBox.GetMaxVector3().y, 0.0f), Vector3(aZones[i].m_bBox.GetMinVector3().x, aZones[i].m_bBox.GetMaxVector3().y, 0.0f), Color32(255, 255, 255, 255), Color32(255, 255, 255, 255));
			CVectorMap::DrawLine(Vector3(aZones[i].m_bBox.GetMinVector3().x, aZones[i].m_bBox.GetMaxVector3().y, 0.0f), Vector3(aZones[i].m_bBox.GetMinVector3().x, aZones[i].m_bBox.GetMinVector3().y, 0.0f), Color32(255, 255, 255, 255), Color32(255, 255, 255, 255));
		}
	}
	int zoneRender;
	int zoneRenderEnd;
	if ( m_bDisplayZonesOnVMap )
	{
		zoneRender = 0;
		zoneRenderEnd = aZones.GetCount();
	}
	else
	{
		if ( m_currentZone != -1 && m_currentZone < aZones.GetCount() )
		{
			zoneRender = m_currentZone;
			zoneRenderEnd = zoneRender + 1;
		}
		else
		{
			zoneRender = 0;
			zoneRenderEnd = 0;
		}
	}
	while (zoneRender < zoneRenderEnd)
	{
		const CSlownessZone &zone = aZones[static_cast<u32>(zoneRender)];
		grcDebugDraw::BoxAxisAligned( zone.m_bBox.GetMin(), zone.m_bBox.GetMax(), Color_green, false, 1);
		++zoneRender;
	};
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Works out whether this vehicle is in a slowness zone.
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

void CVehicle::UpdateInSlownessZone()
{
	if ( ( (GetRandomSeed() + fwTimer::GetSystemFrameCount()) & 15) == 0)		// Only do once in a while
	{
		m_nVehicleFlags.bInSlownessZone = false;
		Vector3 testCrs = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

		atArray<CSlownessZone>	&aZones = CSlownessZoneManager::m_SlownessZonesManagerInstance.m_aSlownessZone;

		for (s32 i = 0 ; i < aZones.GetCount(); i++)
		{
			if (aZones[i].m_bBox.ContainsPoint(RCC_VEC3V(testCrs)))
			{
				m_nVehicleFlags.bInSlownessZone = true;
				return;
			}
		}

		// cars in tunnels are all slowed
		if ( (GetVehicleType()==VEHICLE_TYPE_CAR) && GetPortalTracker())
		{
			CInteriorInst* pIntInst = GetPortalTracker()->GetInteriorInst();
			s32	roomIdx = GetPortalTracker()->m_roomIdx;
			if (pIntInst && roomIdx >= 0 && pIntInst->IsSubwayMLO())
			{
				m_nVehicleFlags.bInSlownessZone = true;
				return;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// __BANK build support
/////////////////////////////////////////////////////////////////////////////////


#if __BANK

void CSlownessZoneManager::Save()
{
	atString	filename;

	// NEED to get the current level?
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::SLOWNESS_ZONES_FILE);
	if (DATAFILEMGR.IsValid(pData))
	{
		Verifyf(PARSER.SaveObject(pData->m_filename, "meta", &m_SlownessZonesManagerInstance, parManager::XML), "Failed to save slownesszones");
	}
}

void CSlownessZoneManager::CentreAroundPlayer()
{
	m_currentZoneMin = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
	m_currentZoneMax = m_currentZoneMin;
	m_currentZoneMin -= Vector3(2.0f,2.0f,2.0f);
	m_currentZoneMax += Vector3(2.0f,2.0f,2.0f);
	ZoneUpdateCB();

}

void CSlownessZoneManager::WarpPlayerToCentre()
{
	if ( m_currentZone != -1 && m_currentZone < m_SlownessZonesManagerInstance.m_aSlownessZone.GetCount() )
	{
		FindPlayerPed()->SetPosition(VEC3V_TO_VECTOR3(m_SlownessZonesManagerInstance.m_aSlownessZone[m_currentZone].m_bBox.GetCenter()), true, true, true );
	}
}

void CSlownessZoneManager::AddZone()
{
	CSlownessZone &z = m_SlownessZonesManagerInstance.m_aSlownessZone.Grow(4);
	z.m_bBox.Set( RCC_VEC3V(m_currentZoneMin), RCC_VEC3V(m_currentZoneMax) );
	m_currentZone = m_SlownessZonesManagerInstance.m_aSlownessZone.GetCount()-1;
}

void CSlownessZoneManager::DeleteZone()
{
	if ( m_currentZone != -1 && m_currentZone < m_SlownessZonesManagerInstance.m_aSlownessZone.GetCount() )
	{
		m_SlownessZonesManagerInstance.m_aSlownessZone.DeleteFast(m_currentZone);
	}
	CheckCurrentZoneRangeCB();
}

void CSlownessZoneManager::ZoneUpdateCB()
{
	if ( m_currentZoneMin.x > m_currentZoneMax.x)
	{
		float tmp = m_currentZoneMin.x;
		m_currentZoneMin.x = m_currentZoneMax.x;
		m_currentZoneMax.x = tmp;
	}

	if ( m_currentZoneMin.y > m_currentZoneMax.y)
	{
		float tmp = m_currentZoneMin.y;
		m_currentZoneMin.y = m_currentZoneMax.y;
		m_currentZoneMax.y = tmp;
	}
	if ( m_currentZoneMin.z > m_currentZoneMax.z)
	{
		float tmp = m_currentZoneMin.z;
		m_currentZoneMin.z = m_currentZoneMax.z;
		m_currentZoneMax.z = tmp;
	}
	if ( m_currentZone != -1 && m_bModifySelectedZone && m_currentZone < m_SlownessZonesManagerInstance.m_aSlownessZone.GetCount() )
	{
		m_SlownessZonesManagerInstance.m_aSlownessZone[m_currentZone].m_bBox.Set(RCC_VEC3V(m_currentZoneMin), RCC_VEC3V(m_currentZoneMax));
	}
}

void CSlownessZoneManager::CheckCurrentZoneRangeCB()
{
	if ( m_currentZone >= m_SlownessZonesManagerInstance.m_aSlownessZone.GetCount() )
	{
		m_currentZone = m_SlownessZonesManagerInstance.m_aSlownessZone.GetCount()-1;
	}
	if ( m_currentZone != -1 )
	{
		m_currentZoneMin = m_SlownessZonesManagerInstance.m_aSlownessZone[m_currentZone].m_bBox.GetMinVector3();
		m_currentZoneMax = m_SlownessZonesManagerInstance.m_aSlownessZone[m_currentZone].m_bBox.GetMaxVector3();
	};
}

// Vehicle AI and Nodes -> Slowness zones
void CSlownessZoneManager::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Slowness zones", false);
	// debug widgets:
	bank.AddToggle("Display debug", &m_bDisplayDebugInfo);
	bank.AddToggle("Draw zones on VectorMap", &m_bDisplayZonesOnVMap);
	bank.AddButton("Load", Reload);		// Load the list of nodes
	bank.AddButton("Save", Save);		// Save the list of nodes
	bank.AddSeparator();
	bank.AddSlider("Current zone", &m_currentZone, -1, 100, 1, datCallback(CFA(CheckCurrentZoneRangeCB) ));
	bank.AddToggle("Modify selected zone", &m_bModifySelectedZone );
	bank.AddButton("Add", AddZone);
	bank.AddButton("Delete", DeleteZone);
	bank.AddButton("Centre around player", CentreAroundPlayer);
	bank.AddButton("Warp player to centre", WarpPlayerToCentre);
	bank.AddVector("Zone min",&m_currentZoneMin, -3500.0f , 7000.0f,10.0f, datCallback(CFA(ZoneUpdateCB)) );
	bank.AddVector("Zone max", &m_currentZoneMax, -3500.0f , 7000.0f,10.0f, datCallback(CFA(ZoneUpdateCB)) );
	bank.PopGroup();
}
#endif


void CSlownessZoneManager::Reload()
{
	Reset();
	Load();
}

