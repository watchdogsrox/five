// Title	:	OrderManager.cpp
// Purpose	:	Manages all currently assigned orders
// Started	:	13/05/2010

#include "Game/Dispatch/OrderManager.h"

// Game includes
#include "game/Dispatch/Orders.h"
#include "Network/Arrays/NetworkArrayHandlerTypes.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"

// Framework includes
#include "ai/aichannel.h"

AI_OPTIMISATIONS()

/////////////////////////////////////////////////////////
// COrderManager::COrderPtr
/////////////////////////////////////////////////////////
unsigned COrderManager::COrderPtr::LARGEST_ORDER_TYPE = COrder::OT_Police; // the incident with the largest sync data
unsigned COrderManager::COrderPtr::SIZEOF_ORDER_TYPE	= datBitsNeeded<COrder::OT_NumTypes>::COUNT;

void COrderManager::COrderPtr::Set(COrder* pOrder)
{
	aiAssert(pOrder);
	aiAssert(!m_pOrder);
	m_pOrder = pOrder;
}

void COrderManager::COrderPtr::Clear(bool bDestroyOrder)
{
	if (m_pOrder)
	{
		if (bDestroyOrder)
		{
			delete m_pOrder;
		}

		m_pOrder = NULL;
	}
}

void COrderManager::COrderPtr::Copy(const COrder& order)
{
	if (m_pOrder && m_pOrder->GetType() != order.GetType())
	{
		Clear();
	}

	if (!m_pOrder)
	{
		m_pOrder = CreateOrder(order.GetType());
	}

	if (m_pOrder)
	{
		m_pOrder->CopyNetworkInfo(order);
	}
}

netObject* COrderManager::COrderPtr::GetNetworkObject() const
{
	if (m_pOrder)
	{
		return NetworkUtils::GetNetworkObjectFromEntity(m_pOrder->GetEntity());
	}

	return NULL;
}

COrder* COrderManager::COrderPtr::CreateOrder(u32 orderType)
{
	COrder* pOrder = NULL;

	if (COrder::GetPool()->GetNoOfFreeSpaces() == 0)
	{
		aiFatalAssertf(0, "Ran out of COrders, pool full");
	}
	else
	{
		switch (orderType)
		{
		case COrder::OT_Basic:
			pOrder = rage_new COrder();
			break;
		case COrder::OT_Police:
			pOrder = rage_new CPoliceOrder();
			break;
		case COrder::OT_Swat:
			pOrder = rage_new CSwatOrder();
			break;
		case COrder::OT_Ambulance:
			pOrder = rage_new CAmbulanceOrder();
			break;
		case COrder::OT_Fire:
			pOrder = rage_new CFireOrder();
			break;
		case COrder::OT_Gang:
			pOrder = rage_new CGangOrder();
			break;
		default:
			aiAssertf(0, "Unrecognised order type");
			break;
		}
	}

	return pOrder;
}

/////////////////////////////////////////////////////////
// COrderManager
/////////////////////////////////////////////////////////

COrderManager COrderManager::ms_instance;

COrderManager::COrderManager()
: m_Orders(MAX_ORDERS)
, m_OrdersArrayHandler(NULL)
{

}

COrderManager::~COrderManager()
{
}

void COrderManager::InitSession()
{
}

void COrderManager::ShutdownSession()
{
	ClearAllOrders(false);
}

bool COrderManager::AddOrder( const COrder& order )
{
	if (aiVerifyf(COrder::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of COrders"))
	{
		if (order.GetEntity())
		{
			// don't assign orders to entities about to be removed
			if (order.GetEntity()->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
			{
				return false;
			}

			// don't assign orders to non-networked entities in MP
			if (NetworkInterface::IsGameInProgress() && !NetworkUtils::GetNetworkObjectFromEntity(order.GetEntity()))
			{
				return false;
			}
		}

		COrder* pNewOrder = order.Clone();

		aiAssertf(pNewOrder->GetType() == order.GetType(), "An order class does not have Clone() defined");

		s32 i = 0;
		s32 freeSlot = -1;

		for( i = 0; i < MAX_ORDERS; i++ )
		{
			if (NetworkInterface::IsGameInProgress())
			{
				if (IsSlotFree(i))
				{
					freeSlot = i;
				}
				else if (m_Orders[i].Get() && m_Orders[i].Get()->GetOrderID() == pNewOrder->GetOrderID())
				{
					// there is already an order for this ped which has not been assigned yet (which was received from another machine)
					delete pNewOrder;
					return false;
				}
				else if (m_OrdersArrayHandler && m_OrdersArrayHandler->GetElementIDForIndex(i) == pNewOrder->GetOrderID())
				{
					Assert(m_OrdersArrayHandler->IsElementLocallyArbitrated(i));

					// there was an order for this ped which is still being cleaned up by the array handler, so we can't add a new one just now
					delete pNewOrder;
					return false;
				}
			}
			else
			{
				if (IsSlotFree(i))
				{
					freeSlot = i;
					break;
				}
			}
		}

		aiAssertf(freeSlot != -1, "COrderManager: Orders array is full");

		if (freeSlot != -1 && AddOrderAtSlot(freeSlot, *pNewOrder))
		{
			GetOrder(freeSlot)->AssignOrderToEntity();
			return true;
		}
		else
		{
			delete pNewOrder;
		}
	}

	return false;
}

void COrderManager::Update()
{
	if(NetworkInterface::IsGameInProgress())
		m_OrdersArrayHandler = NetworkInterface::GetArrayManager().GetOrdersArrayHandler();
	else
		m_OrdersArrayHandler = NULL;

	u32 numActiveOrders = 0;

	for( s32 i = 0; i < MAX_ORDERS; i++ )
	{
		COrder* pOrder = GetOrder(i);
		if( pOrder)
		{
			numActiveOrders++;

			if (pOrder->GetFlagForDeletion() )
			{
				if (pOrder->IsLocallyControlled())
				{
					ClearOrder(i);
				}
				else
				{
					// order has migrated to another machine, so clear the deletion flag
					pOrder->SetFlagForDeletion(false);
				}
			}
			else if (NetworkInterface::IsGameInProgress() && !pOrder->GetAssignedToEntity())
			{
				pOrder->AssignOrderToEntity();
			}
		}
	}

	aiAssertf(COrder::GetPool()->GetNoOfUsedSpaces()==0 || numActiveOrders >= COrder::GetPool()->GetNoOfUsedSpaces()-1, "Orders are being leaked. Active orders = %d, pool size = %d", numActiveOrders, COrder::GetPool()->GetNoOfUsedSpaces());
}

void COrderManager::ClearAllOrders(bool bSetDirty)
{
	for( s32 i = 0; i < MAX_ORDERS; i++ )
	{
		COrder* pOrder = GetOrder(i);
		if( pOrder && (!bSetDirty || pOrder->IsLocallyControlled()))
		{
			ClearOrder(i, bSetDirty);
		}
	}
}

COrder* COrderManager::GetOrder( s32 iCount ) const
{
	aiAssert(iCount >= 0 && iCount < m_Orders.GetCount());
	return m_Orders[iCount].Get();
}

void COrderManager::ClearOrder(s32 iCount, bool bSetDirty)
{
	if (m_Orders[iCount].Get())
	{
		m_Orders[iCount].Get()->RemoveOrderFromEntity();

		m_Orders[iCount].Clear();

		if (bSetDirty)
		{
			DirtyOrder(iCount);	
		}
	}
}

s32 COrderManager::GetOrderIndex(COrder& order)
{
	s32 index = -1;

	for( s32 i = 0; i < MAX_ORDERS; i++ )
	{
		if( GetOrder(i) == &order)
		{
			index = i;
			break;
		}
	}

	Assertf(index != -1, "Order not found");

	return index;
}


bool COrderManager::IsOrderLocallyControlled(s32 index) const
{
	if (NetworkInterface::IsGameInProgress() && m_OrdersArrayHandler)
	{
		return m_OrdersArrayHandler->IsElementLocallyArbitrated(index);
	}

	return true;
}

bool COrderManager::IsOrderRemotelyControlled(s32 index) const
{
	if (NetworkInterface::IsGameInProgress() && m_OrdersArrayHandler)
	{
		return m_OrdersArrayHandler->IsElementRemotelyArbitrated(index);
	}

	return false;
}

bool COrderManager::IsSlotFree(s32 index) const
{
	bool bFree = false;

	if (!GetOrder(index))
	{
		if (NetworkInterface::IsGameInProgress() && m_OrdersArrayHandler)
		{
			bFree = (m_OrdersArrayHandler->GetElementArbitration(index) == NULL);
		}
		else
		{
			bFree = true;
		}
	}

	return bFree;
}

bool COrderManager::AddOrderAtSlot(s32 index, COrder& order)
{
	if (aiVerifyf(IsSlotFree(index), "Trying to add an order to slot %d, which is still arbitrated by %s", index, (NetworkInterface::IsGameInProgress() && m_OrdersArrayHandler && m_OrdersArrayHandler->GetElementArbitration(index) != NULL) ? m_OrdersArrayHandler->GetElementArbitration(index)->GetLogName() : "??"))
	{
		m_Orders[index].Set(&order);
		order.SetOrderIndex(index);
		DirtyOrder(index);
		return true;
	}

	return false;
}

void COrderManager::DirtyOrder(s32 index) 
{
	if (aiVerifyf(!IsOrderRemotelyControlled(index), "Trying to dirty an order controlled by another machine"))
	{
		if (NetworkInterface::IsGameInProgress() && m_OrdersArrayHandler)
		{
			// flag orders array handler as dirty so that this order gets sent to the other machines
			m_OrdersArrayHandler->SetElementDirty(index);
		}
	}
}

void COrderManager::ReassignOrdersToPed(CPed* pPed)
{
	for( s32 i = 0; i < MAX_ORDERS; i++ )
	{
		COrder* pOrder = GetOrder(i);

		if( pOrder && pOrder->GetEntity() == (CEntity*)pPed)
		{
			if (pPed->GetPedIntelligence()->GetOrder() == pOrder)
			{
				pOrder->AssignOrderToEntity();
			}
		}
	}

}
