// Title	:	IncidentManger.cpp
// Purpose	:	Manages currently active incidents
// Started	:	13/05/2010

#include "Game/Dispatch/IncidentManager.h"

// Game headers
#include "debug/DebugScene.h"
#include "Game/Dispatch/Incidents.h"
#include "Game/Dispatch/DispatchManager.h"
#include "Game/Dispatch/OrderManager.h"
#include "Game/Dispatch/Orders.h"
#include "Game/zones.h"
#include "Network/Arrays/NetworkArrayHandlerTypes.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "scene/Entity.h"
#include "vehicleAi/pathfind.h"
#include "vfx/misc/Fire.h"

AI_OPTIMISATIONS()

CIncidentManager CIncidentManager::ms_instance;

#if __BANK
bool CIncidentManager::s_bRenderDebug = false;
#endif

/////////////////////////////////////////////////////////
// CIncidentManager::CIncidentPtr
/////////////////////////////////////////////////////////
unsigned CIncidentManager::CIncidentPtr::LARGEST_INCIDENT_TYPE = CIncident::IT_Fire; // the incident with the largest sync data

void CIncidentManager::CIncidentPtr::Set(CIncident* pIncident)
{
	aiAssert(pIncident);
	aiAssert(!m_pIncident);
	m_pIncident = pIncident;
}

void CIncidentManager::CIncidentPtr::Clear(bool bDestroyIncident)
{
	if (m_pIncident)
	{
		if (bDestroyIncident)
		{
			delete m_pIncident;
		}
		m_pIncident = NULL;
	}
}

void CIncidentManager::CIncidentPtr::Copy(const CIncident& incident)
{
	if (m_pIncident && m_pIncident->GetType() != incident.GetType())
	{
		Clear();
	}

	if (!m_pIncident)
	{
		m_pIncident = CreateIncident(incident.GetType());
	}

	if (m_pIncident)
	{
		Assert(m_pIncident->GetType() == incident.GetType());
		m_pIncident->CopyNetworkInfo(incident);
	}
}

void CIncidentManager::CIncidentPtr::Migrate()
{
	if (m_pIncident)
	{
		m_pIncident->Migrate();
	}
}

netObject* CIncidentManager::CIncidentPtr::GetNetworkObject() const
{
	if (m_pIncident)
	{
		return NetworkUtils::GetNetworkObjectFromEntity(m_pIncident->GetEntity());
	}

	return NULL;
}

CIncident* CIncidentManager::CIncidentPtr::CreateIncident(u32 incidentType)
{
	CIncident* pIncident = NULL;

	if (CIncident::GetPool()->GetNoOfFreeSpaces() == 0)
	{
		aiFatalAssertf(0, "Ran out of CIncidents, pool full");
	}
	else
	{
		switch (incidentType)
		{
		case CIncident::IT_Wanted:
			pIncident = rage_new CWantedIncident();
			break;
		case CIncident::IT_Arrest:
			pIncident = rage_new CArrestIncident();
			break;
		case CIncident::IT_Fire:
			pIncident = rage_new CFireIncident();
			break;
		case CIncident::IT_Injury:
			pIncident = rage_new CInjuryIncident();
			break;
		case CIncident::IT_Scripted:
			pIncident = rage_new CScriptIncident();
			break;
		default:
			aiAssertf(0, "Unrecognised incident type");
			break;
		}
	}

	return pIncident;
}

/////////////////////////////////////////////////////////
// CIncidentManager
/////////////////////////////////////////////////////////

CIncidentManager::CIncidentManager()
: m_Incidents(MAX_INCIDENTS)
, m_IncidentCount(0)
, m_IncidentsArrayHandler(NULL)
{
}

CIncidentManager::~CIncidentManager()
{
}

void CIncidentManager::InitSession()
{
}

void CIncidentManager::ShutdownSession()
{
	ClearAllIncidents(false);
}

CIncident* CIncidentManager::AddIncident( const CIncident& incident, bool bTestForEqualIncid, int* incidentSlot )
{
	const CEntity* pEnt = incident.GetEntity();
	if(pEnt && pEnt->GetIsDynamic())
	{
		const CDynamicEntity* pDynamic = SafeCast(const CDynamicEntity, pEnt);

		if (pDynamic && NetworkInterface::IsGameInProgress())
		{
			if (!pDynamic->GetNetworkObject())
			{
				aiAssertf(0, "Trying to add an incident for an entity with no network object");
				return NULL;
			}
			if (pDynamic->IsNetworkClone())
			{
				aiAssertf(0, "Trying to add an incident for a network clone");
				return NULL;
			}

			// can't add an incident if there is already an array slot using this id in MP
			if (NetworkInterface::IsGameInProgress())
			{
				CIncidentsArrayHandler* pIncidentsHandler = NetworkInterface::GetArrayManager().GetIncidentsArrayHandler();

				if (pIncidentsHandler && pIncidentsHandler->IsIDInUse((u32)pDynamic->GetNetworkObject()->GetObjectID()))
				{
					if (incident.GetType() == CIncident::IT_Scripted)
					{
						aiAssertf(0, "Failed to add script incident for %s, there is already another incident using this id", pDynamic->GetNetworkObject()->GetLogName());
					}

					if (incident.GetType() == CIncident::IT_Wanted)
					{
						aiAssertf(0, "Failed to add wanted incident for %s, there is already another incident using this id", pDynamic->GetNetworkObject()->GetLogName());
					}

					return NULL;
				}
			}
		}
	}

	// don't allow multiple incidents of the same type
	if(bTestForEqualIncid)
	{
		for(int i = 0; i < MAX_INCIDENTS; i++ )
		{
			CIncident* pCurrIncident = GetIncident(i);

			if (pCurrIncident && pCurrIncident->IsEqual(incident))
			{
				return NULL;
			}
		}
	}

	if (aiVerifyf(CIncident::GetPool()->GetNoOfFreeSpaces() > 0, "CIncident pool is full"))
	{
		CIncident* pNewIncident = incident.Clone();

		aiAssertf(pNewIncident->GetType() == incident.GetType(), "An incident class does not have Clone() defined");

		// allow the incident to determine which dispatch services it requires
		pNewIncident->RequestDispatchServices();

		bool bCanProcessIncident = false;

		// if all the required services are disabled, then we can dump the incident
		for ( s32 dt = 0; dt < DT_Max; dt++)
		{
			if (pNewIncident->GetRequestedResources(dt) > 0 && CDispatchManager::GetInstance().IsDispatchEnabled((DispatchType)dt))
			{
				bCanProcessIncident = true;
				break;
			}
		}

		if (bCanProcessIncident)
		{
			int freeSlot = -1;

			for(int j = 0; j < MAX_INCIDENTS; j++ )
			{
				if (m_IncidentsArrayHandler)
				{ 
					if (IsSlotFree(j))
					{
						if (freeSlot == -1)
							freeSlot = j;
					}
					else if (m_IncidentsArrayHandler->GetElementIDForIndex(j)== incident.GetIncidentID())
					{
						// we can't add the incident if there is a slot in the array still using this id (ie another incident with the same entity, or an incident cleaning itself up)
						bCanProcessIncident = false;
						break;
					}
				}
				else if (IsSlotFree(j) )
				{
					freeSlot = j;
					break;
				}
			}
		
			if (bCanProcessIncident && aiVerifyf(freeSlot != -1, "CIncidentManager: Incidents array is full"))
			{
				if (AddIncidentAtSlot(freeSlot, *pNewIncident))
				{
					if (incidentSlot)
					{
						*incidentSlot = freeSlot;
					}
					return pNewIncident;
				}
			}
		}

		delete pNewIncident;
	}

	return NULL;
}

CIncident* CIncidentManager::AddFireIncident(CFire* pFire)
{
	CIncident* pNewIncident = NULL;

	if( aiVerify(pFire) && CFireIncident::GetCount() < CFireIncident::MaxFireIncidents )
	{
		CEntity* pEntity = pFire->GetEntity();
		Vec3V v3Position = pFire->GetPositionWorld();

		bool bAddIncident = true;

		// don't add incidents for burning fragments (car doors, etc)
		if (pEntity && pEntity->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
		{
			bAddIncident = false;
		}

		netObject* pEntityObj = NULL;
		
		if (NetworkInterface::IsGameInProgress())
		{
			pEntityObj = pEntity ? NetworkUtils::GetNetworkObjectFromEntity(pEntity) : NULL;

			// don't add an incident if the entity on fire is a network clone or has no network object
			if (!pEntityObj || pEntityObj->IsClone())
			{
				bAddIncident = false;
			}
		}

		if (bAddIncident)
		{
			bool bAddedToExisting = false;

			CFireIncident* pExistingIncident = FindFireIncidentForFire(pFire);

			if (pExistingIncident)
			{
				bAddedToExisting = true;
				pNewIncident = pExistingIncident;
			}
			else if (NetworkInterface::IsGameInProgress())
			{
				// we can't add the incident if there is already another incident for this network object. The net object id is used to
				// identify the incident over the network.
				for( s32 i = 0; i < MAX_INCIDENTS; i++ )
				{
					CIncident* pIncident = GetIncident(i);
					
					if (pIncident && pIncident->GetNetworkObject() == pEntityObj)
					{
						bAddIncident = false;
						break;
					}
				}
			}

			if (!bAddedToExisting && bAddIncident)
			{
				//Fire trucks don't go down turned off road nodes any longer but we still want them to attempt to drive to more locations in the city
				float fDistanceToOnNode = (CMapAreas::GetAreaIndexFromPosition(VEC3V_TO_VECTOR3(v3Position)) == 0 ) ? 90.0f : 40.0f;

				// don't add incidents if the firetruck won't be able to reach it
				CNodeAddress CurrentNode;
				CurrentNode = ThePaths.FindNodeClosestToCoors(VEC3V_TO_VECTOR3(v3Position), fDistanceToOnNode,  CPathFind::IgnoreSwitchedOffNodes);
				//BANK_ONLY(Displayf("FindNodeClosestToCoors frame %d for fire entity:%s (%6.2f,%6.2f,%6.2f)", 
				//		fwTimer::GetSystemFrameCount(),
				//			pEntity != NULL ? pEntity->GetModelName() : "Null", 
				//			v3Position.GetXf(),v3Position.GetYf(), v3Position.GetZf() );)
				if (CurrentNode.IsEmpty())
				{
					bAddIncident = false;
				}

				if (bAddIncident)
				{
					CFireIncident fireIncident;
					fireIncident.AddFire(pFire);
					fireIncident.SetEntity(pEntity);
					fireIncident.SetLocation(RCC_VECTOR3(v3Position));
					//dont test for dups as we already combined fire incidents above
					pNewIncident = CIncidentManager::GetInstance().AddIncident(fireIncident, false);
				}
			}
		}
	}

	return pNewIncident;
}

void CIncidentManager::Update()
{
	if(NetworkInterface::IsGameInProgress())
		m_IncidentsArrayHandler = NetworkInterface::GetArrayManager().GetIncidentsArrayHandler();
	else
		m_IncidentsArrayHandler = NULL;

	u8 incidents = 0; // Local variable to keep track of number of incidents
	for( s32 i = 0; i < MAX_INCIDENTS; i++ )
	{
		CIncident* pIncident = GetIncident(i);
		if( pIncident)
		{
			incidents++;
			if(IsIncidentLocallyControlled(i))
			{
				if( pIncident->Update(fwTimer::GetTimeStep()) == CIncident::IS_Finished )
				{
					ClearIncident(i);
				}
				else
				{
					pIncident->RequestDispatchServices();
				}
			}	
			else
			{
				pIncident->UpdateRemote(fwTimer::GetTimeStep());
			}
		}
	}
	m_IncidentCount = incidents; // Update member variable, so calls to GetIncidentCount don't get half-baked data
}

void CIncidentManager::ClearAllIncidents(bool bSetDirty)
{
	for( s32 i = 0; i < MAX_INCIDENTS; i++ )
	{
		CIncident* pIncident = GetIncident(i);

		if( pIncident && (!bSetDirty || IsIncidentLocallyControlled(i)))
		{
			ClearIncident(i, bSetDirty);
		}
	}
}

void CIncidentManager::ClearIncident(CIncident* pIncidentToDelete)
{
	for( s32 i = 0; i < MAX_INCIDENTS; i++ )
	{
		if( GetIncident(i) == pIncidentToDelete )
		{
			ClearIncident(i);
			return;
		}
	}
}

void CIncidentManager::ClearIncident(s32 iCount, bool bSetDirty)
{
	if (m_Incidents[iCount].Get())
	{
		m_Incidents[iCount].Clear();

		if (bSetDirty)
		{
			DirtyIncident(iCount);	
		}
	}
}

void CIncidentManager::ClearAllIncidentsOfType(s32 iType)
{
	for( s32 i = 0; i < MAX_INCIDENTS; i++ )
	{
		CIncident* pIncident = GetIncident(i);
		if( pIncident && IsIncidentLocallyControlled(i))
		{
			if( pIncident->GetType() == iType )
			{
				ClearIncident(i);
			}
		}
	}
}

void CIncidentManager::ClearExcitingStuffFromArea(Vec3V_In center, const float& radius)
{
	// Not sure to what extent it would be safe to do this stuff in MP games, so
	// for now, this function has no effect in that case.
	if(NetworkInterface::IsGameInProgress())
	{
		return;
	}

	const Vec3V centerV = center;
	const ScalarV radiusV = LoadScalar32IntoScalarV(radius);
	const ScalarV radiusSqV = Scale(radiusV, radiusV);

	for(int i = 0; i < MAX_INCIDENTS; i++)
	{
		const CIncident* pIncident = GetIncident(i);

		if(!pIncident)
		{
			continue;
		}

		if(pIncident->GetType() != CIncident::IT_Fire)
		{
			// Only concerned with fire right now.
			// Note: I was considering CInjuryIncident, but when I tested it, the incident
			// already got removed when the injured ped got removed, so we probably don't
			// have to deal with them here. CScriptIncident we probably shouldn't touch, and
			// CWantedIncident is probably dealt with by the wanted system.
			continue;
		}

		const CFireIncident* pFireIncident = static_cast<const CFireIncident*>(pIncident);
		if(pFireIncident->AreAnyFiresActive())
		{
			// There are still fires active, probably shouldn't clear it.
			continue;
		}

		const Vec3V incidentPosV = VECTOR3_TO_VEC3V(pIncident->GetLocation());
		const ScalarV distSqV = DistSquared(centerV, incidentPosV);
		if(IsGreaterThanAll(distSqV, radiusSqV))
		{
			continue;
		}

		ClearIncident(i);
	}
}

const CIncident* CIncidentManager::FindIncidentOfTypeWithEntity(u32 uIncidentType, const CEntity& rEntity) const
{
	//Iterate over the incidents.
	for(s32 i = 0; i < MAX_INCIDENTS; ++i)
	{
		//Ensure the incident is valid.
		const CIncident* pIncident = GetIncident(i);
		if(!pIncident)
		{
			continue;
		}
		
		//Ensure the entity matches.
		if(&rEntity != pIncident->GetEntity())
		{
			continue;
		}
		
		//Ensure the type matches.
		if(uIncidentType != (u32)pIncident->GetType())
		{
			continue;
		}
		
		return pIncident;
	}
	
	return NULL;
}

void CIncidentManager::RemoveEntityFromIncidents(const CEntity& entity)
{
	for( s32 i = 0; i < MAX_INCIDENTS; i++ )
	{
		CIncident* pIncident = GetIncident(i);

		if (pIncident && pIncident->GetEntity() == &entity)
		{
			pIncident->SetEntity(NULL);
		}
	}
}


CIncident* CIncidentManager::GetIncident( s32 iCount ) const
{
	if (aiVerify(iCount >= 0 && iCount < MAX_INCIDENTS))
	{
		return m_Incidents[iCount].Get();
	}

	return NULL;
}

CIncident* CIncidentManager::GetIncidentFromIncidentId(u32 incidentID, unsigned* incidentIndex) const
{
	for( s32 i = 0; i < MAX_INCIDENTS; i++ )
	{
		CIncident* pIncident = GetIncident(i);

		if (pIncident && pIncident->GetIncidentID() == incidentID)
		{
			if (incidentIndex)
			{
				*incidentIndex = i;
			}

			return pIncident;
		}
	}

	return NULL;
}

s32 CIncidentManager::GetIncidentIndex(CIncident* pIncident) const
{
	for( s32 i = 0; i < MAX_INCIDENTS; i++ )
	{
		if( GetIncident(i) == pIncident )
		{
			return i;
		}
	}

	aiAssertf(0, "CIncidentManager::GetIncidentIndex: Incident not found");

	return -1;
}

bool CIncidentManager::IsIncidentLocallyControlled(s32 index) const
{
	if (NetworkInterface::IsGameInProgress() && m_IncidentsArrayHandler)
	{
		return m_IncidentsArrayHandler->IsElementLocallyArbitrated(index);
	}

	return true;
}

bool CIncidentManager::IsIncidentRemotelyControlled(s32 index) const
{
	if (NetworkInterface::IsGameInProgress() && m_IncidentsArrayHandler)
	{
		return m_IncidentsArrayHandler->IsElementRemotelyArbitrated(index);
	}

	return false;
}

void CIncidentManager::SetIncidentDirty(s32 index) const
{
	if (NetworkInterface::IsGameInProgress() && m_IncidentsArrayHandler && AssertVerify(m_IncidentsArrayHandler->IsElementLocallyArbitrated(index)))
	{
		m_IncidentsArrayHandler->SetElementDirty(index);
	}
}

bool CIncidentManager::IsSlotFree(s32 index) const
{
	bool bFree = false;

	if (!GetIncident(index))
	{
		if (NetworkInterface::IsGameInProgress() && m_IncidentsArrayHandler)
		{
			bFree = (m_IncidentsArrayHandler->GetElementArbitration(index) == NULL);
		}
		else
		{
			bFree = true;
		}
	}

	return bFree;
}

CFireIncident* CIncidentManager::FindFireIncidentForFire(CFire* pFire)
{
	//see if we can add the fire to any existing incident
	for( s32 i = 0; i < MAX_INCIDENTS; i++ )
	{
		CIncident* pIncident = GetIncident(i);

		if (pIncident && pIncident->GetType() == CIncident::IT_Fire)
		{
			CFireIncident* pFireIncident = static_cast<CFireIncident*>(pIncident);
			
			if (pFireIncident->ShouldClusterWith(pFire))
			{
				pFireIncident->AddFire(pFire);
				return pFireIncident;
			}
		}
	}

	return NULL;
}

bool CIncidentManager::AddIncidentAtSlot(s32 index, CIncident& incident)
{
	if (aiVerifyf(IsSlotFree(index), "Trying to add an incident to slot %d, which is still arbitrated by %s", index, (NetworkInterface::IsGameInProgress() && m_IncidentsArrayHandler && m_IncidentsArrayHandler->GetElementArbitration(index) != NULL) ? m_IncidentsArrayHandler->GetElementArbitration(index)->GetLogName() : "??"))
	{
		incident.SetIncidentIndex(index);
		m_Incidents[index].Set(&incident);
		DirtyIncident(index);
		return true;
	}

	return false;
}

void CIncidentManager::DirtyIncident(s32 index) 
{
	if (aiVerifyf(!IsIncidentRemotelyControlled(index), "Trying to dirty an incident controlled by another machine"))
	{
		// flag incidents array handler as dirty so that this incident gets sent to the other machines
		if (NetworkInterface::IsGameInProgress() && m_IncidentsArrayHandler)
		{
			m_IncidentsArrayHandler->SetElementDirty(index);
		}
	}
}

#if __BANK
void CIncidentManager::DebugRender() const
{
	if (s_bRenderDebug)
	{
		atArray<CIncident*> aDebugWantedIncidents;
		atArray<CIncident*> aDebugArrestIncidents;
		atArray<CIncident*> aDebugFireIncidents;
		atArray<CIncident*> aDebugScriptIncidents;
		atArray<CIncident*> aDebugInjuryIncidents;

		s32 iSize = GetMaxCount();

		for( s32 i = 0; i < iSize; i++ )
		{
			CIncident* pIncident = GetIncident(i);
			if( pIncident )
			{
				if( pIncident->GetType() == CIncident::IT_Wanted )
				{
					aDebugWantedIncidents.PushAndGrow(pIncident);
				}
				else if( pIncident->GetType() == CIncident::IT_Arrest)
				{
					aDebugArrestIncidents.PushAndGrow(pIncident);
				}
				else if ( pIncident->GetType() == CIncident::IT_Fire)
				{
					aDebugFireIncidents.PushAndGrow(pIncident);
				}
				else if ( pIncident->GetType() == CIncident::IT_Scripted)
				{
					aDebugScriptIncidents.PushAndGrow(pIncident);
				}
				else if (pIncident->GetType() == CIncident::IT_Injury)
				{
					aDebugInjuryIncidents.PushAndGrow(pIncident);
				}
			}
		}

		s32 iNoTexts = 0;

		Color32 debugColor = Color_purple;

		char szText[64];
		formatf(szText, "===================================");
		grcDebugDraw::Text(GetTextPosition(iNoTexts), debugColor, szText);
		formatf(szText, "         Incident Debug");
		grcDebugDraw::Text(GetTextPosition(iNoTexts), debugColor, szText);
		formatf(szText, "===================================");
		grcDebugDraw::Text(GetTextPosition(iNoTexts), debugColor, szText);

		debugColor = Color_green;
		
		s32 iNumWantedIncidents = aDebugWantedIncidents.GetCount();
		formatf(szText, "Number Of Wanted Incidents : %i", iNumWantedIncidents);
		grcDebugDraw::Text(GetTextPosition(iNoTexts), debugColor, szText);
		RenderDispatchedTypesForIncident(iNoTexts, debugColor, aDebugWantedIncidents);

		s32 iNumArrestIncidents = aDebugArrestIncidents.GetCount();
		formatf(szText, "Number Of Arrest Incidents : %i", iNumArrestIncidents);
		grcDebugDraw::Text(GetTextPosition(iNoTexts), debugColor, szText);
		RenderDispatchedTypesForIncident(iNoTexts, debugColor, aDebugArrestIncidents);

		s32 iNumFireIncidents = aDebugFireIncidents.GetCount();
		formatf(szText, "Number Of Fire Incidents : %i", iNumFireIncidents);
		grcDebugDraw::Text(GetTextPosition(iNoTexts), debugColor, szText);
		RenderDispatchedTypesForIncident(iNoTexts, debugColor, aDebugFireIncidents);

		s32 iNumScriptedIncidents = aDebugScriptIncidents.GetCount();
		formatf(szText, "Number Of Scripted Incidents : %i", iNumScriptedIncidents);
		grcDebugDraw::Text(GetTextPosition(iNoTexts), debugColor, szText);
		RenderDispatchedTypesForIncident(iNoTexts, debugColor, aDebugScriptIncidents);

		s32 iNumInjuryIncidents = aDebugInjuryIncidents.GetCount();
		formatf(szText, "Number Of Injury Incidents : %i", iNumInjuryIncidents);
		grcDebugDraw::Text(GetTextPosition(iNoTexts), debugColor, szText);
		RenderDispatchedTypesForIncident(iNoTexts, debugColor, aDebugInjuryIncidents);
	}
}

void CIncidentManager::RenderDispatchedTypesForIncident(s32& iNoTexts, Color32 debugColor, atArray<CIncident*>& incidentArray)
{
	s32 iSize = incidentArray.GetCount();

	char szText[64];

	// Go through all incidents and render the type and number of dispatched services
	for (s32 i=0; i<iSize; i++)
	{
		atArray<CPed*> excludePedsArray;
		CIncident* pIncident = incidentArray[i];
		if (pIncident)
		{
			formatf(szText, "Incident %i", i);
			grcDebugDraw::Text(GetTextPosition(iNoTexts,1), debugColor, szText);

			CDispatchManager::GetInstance().RenderResourceUsageForIncident(iNoTexts, debugColor, pIncident);

			formatf(szText, "Dispatch Type");
			grcDebugDraw::Text(GetTextPosition(iNoTexts,2), debugColor, szText);

			s32 iNumOrders = COrderManager::GetInstance().GetMaxCount();

			for( s32 j = 0; j < iNumOrders; j++ )
			{
				COrder* pOrder = COrderManager::GetInstance().GetOrder(j);
				if( pOrder && pOrder->GetIncident() == pIncident)
				{
					Color32 dispatchColor = GetColorForDispatchType(pOrder->GetDispatchType()); 

					if (pOrder->GetEntity() && pOrder->GetEntity()->GetIsTypePed())
					{
						CPed* pOrderedPed = static_cast<CPed*>(pOrder->GetEntity());
						
						// Peds in the same vehicle are added to the exclusion list so this only
						// happens per dispatch vehicle
						if (pOrderedPed && !IsPedInExclusionList(pOrderedPed, excludePedsArray))
						{	
							if (pOrderedPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
								formatf(szText, "%s - In Vehicle", pOrder->GetDispatchTypeName());
							else
								formatf(szText, "%s - On Foot", pOrder->GetDispatchTypeName());

							grcDebugDraw::Text(GetTextPosition(iNoTexts,3), dispatchColor, szText);
							excludePedsArray.PushAndGrow(pOrderedPed);

							CVehicle* pVehicle = pOrderedPed->GetMyVehicle();

							if (pVehicle && pOrderedPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
							{
								s32 iNoSeats = pVehicle->GetSeatManager()->GetMaxSeats();

								s32 iNumPedsInVehicle = 1;

								for (s32 k = 0; k < iNoSeats; k++)
								{
									CPed* pPedInSeat = pVehicle->GetSeatManager()->GetPedInSeat(k);
									if (pPedInSeat && pPedInSeat != pOrderedPed)
									{
										const COrder* pPedInSeatsOrder = pPedInSeat->GetPedIntelligence()->GetOrder();
										// Ensure the other peds are responding to the same incident as me
										if (pPedInSeatsOrder && pPedInSeatsOrder->GetIncident() == pIncident)
										{
											excludePedsArray.PushAndGrow(pPedInSeat);
											++iNumPedsInVehicle;
										}
									}
								}
								formatf(szText, "Number Of Peds In Vehicle : %i", iNumPedsInVehicle);
								grcDebugDraw::Text(GetTextPosition(iNoTexts,4), dispatchColor, szText);
							}
						}
					}
				}
			}

			formatf(szText, "Peds Responding To Incident %i : %i", i, excludePedsArray.GetCount());
			grcDebugDraw::Text(GetTextPosition(iNoTexts,1), debugColor, szText);
		}
	}
}

Vector2 CIncidentManager::GetTextPosition(s32& iNoTexts, s32 iIndentLevel)
{
	TUNE_FLOAT(s_fScreenTextPosX, 0.06f, 0.0f, 1.0f, 0.01f);
	TUNE_FLOAT(s_fScreenTextPosY, 0.05f, 0.0f, 1.0f, 0.01f);
	TUNE_FLOAT(s_fVerticalOffset, 0.015f, 0.0f, 1.0f, 0.001f);
	TUNE_FLOAT(s_fIndentOffset, 0.015f, 0.0f, 1.0f, 0.001f);

	return Vector2(s_fScreenTextPosX + s_fIndentOffset * iIndentLevel, s_fScreenTextPosY + s_fVerticalOffset * iNoTexts++);
}

Color32 CIncidentManager::GetColorForDispatchType(DispatchType dt)
{
	switch (dt)
	{
		case DT_PoliceAutomobile:				return Color_blue;
		case DT_PoliceHelicopter:				return Color_turquoise;
		case DT_FireDepartment:					return Color_red;
		case DT_SwatAutomobile:					return Color_orange;
		case DT_AmbulanceDepartment:			return Color_green;
		case DT_PoliceRiders:					return Color_blue;
		case DT_PoliceVehicleRequest:			return Color_brown;
		case DT_PoliceRoadBlock:				return Color_blue;
		case DT_Gangs:							return Color_DarkOliveGreen;
		case DT_SwatHelicopter:					return Color_DarkOrange;
		case DT_PoliceBoat:						return Color_blue;
		case DT_ArmyVehicle:					return Color_DarkOliveGreen;
		case DT_BikerBackup:					return Color_brown;
		case DT_Assassins:						return Color_bisque;
		default: aiAssert(0);
	}
	return Color_green;
}

bool CIncidentManager::IsPedInExclusionList(CPed* pPed, atArray<CPed*>& pExcludedPedsArray)
{
	s32 iSize = pExcludedPedsArray.GetCount();

	for (s32 i=0; i<iSize; i++)
	{
		if (pExcludedPedsArray[i] == pPed)
		{
			return true;
		}
	}
	return false;
}

#endif
