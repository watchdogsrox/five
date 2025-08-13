// Title	:	OrderManger.h
// Purpose	:	Manages all currently assigned orders
// Started	:	13/05/2010

#ifndef INC_ORDER_MANAGER_H_
#define INC_ORDER_MANAGER_H_
// Rage headers
#include "Vector/Vector3.h"
// Framework headers
#include "scene/Entity.h"
// Game headers
#include "game/Dispatch/DispatchEnums.h"
#include "game/Dispatch/Orders.h"
#include "scene/RegdRefTypes.h"

class COrder;
class COrdersArrayHandler;

// Order manager
class COrderManager
{
	friend class COrdersArrayHandler; // this needs direct access to the orders array

public:

	enum {MAX_ORDERS = 150};

	// A wrapper class for an order pointer, required by the network array syncing code. The network array handlers deal with arrays of
	// fixed size elements, and are not set up to deal with pointers to different classes of elements. This is an adaptor class used to get 
	// around that problem.
	class COrderPtr
	{
	public:
		COrderPtr() : m_pOrder(NULL) {}

		COrder* Get() const
		{
			return m_pOrder;
		}

		void Set(COrder* pOrder);
		void Clear(bool bDestroyOrder = true);
		void Copy(const COrder& order);

		netObject* GetNetworkObject() const;

		void Serialise(CSyncDataBase& serialiser)
		{
			u32 orderType = m_pOrder ? m_pOrder->GetType() : COrder::OT_Invalid;

			if (serialiser.GetIsMaximumSizeSerialiser())
				orderType = LARGEST_ORDER_TYPE;

			SERIALISE_UNSIGNED(serialiser, orderType, SIZEOF_ORDER_TYPE, "Order Type");

			if (m_pOrder && static_cast<u32>(m_pOrder->GetType()) != orderType)
			{
				Clear();
			}

			if (!m_pOrder)
			{
				m_pOrder = CreateOrder(orderType);
			}

			if (m_pOrder)
			{
				m_pOrder->Serialise(serialiser);
			}
		}

	protected:

		COrder* CreateOrder(u32 orderType);

		static unsigned LARGEST_ORDER_TYPE;
		static unsigned SIZEOF_ORDER_TYPE;

		COrder* m_pOrder;
	};

public:
	COrderManager();
	~COrderManager();
	
	void InitSession();
	void ShutdownSession();

	bool AddOrder( const COrder& order );
	void Update();
	void ClearAllOrders(bool bSetDirty = true);

	s32 GetMaxCount() const { return m_Orders.GetMaxCount(); }
	COrder* GetOrder(s32 iCount) const;

	s32 GetOrderIndex(COrder& order);

	COrder*	GetNewOrder(DispatchType dispatchType, CEntity* pEntity, CIncident* pIncident);

	static COrderManager& GetInstance() { return ms_instance; }

	// returns true if the order at this index is controlled by our machine in a network game
	bool IsOrderLocallyControlled(s32 index) const;

	// returns true if the order at this index is locally controlled, or not controlled by any machines
	bool IsOrderRemotelyControlled(s32 index) const;

	// returns true if this slot is free and not controlled by any machines
	bool IsSlotFree(s32 index) const;

	// Reassigns all orders to the given ped. This is used in MP when a ped becomes local again. Some orders setup state and start tasks on the ped. 
	// This needs to be reapplied whenever the ped becomes local again.
	void ReassignOrdersToPed(CPed* pPed);

protected:

	void ClearOrder(s32 iCount, bool bSetDirty = true);

	// adds the order to the specified slot
	bool AddOrderAtSlot(s32 index, COrder& order);

	// flags the order index as dirty so that it is sent across the network
	void DirtyOrder(s32 index);

private:
	atFixedArray<COrderPtr, MAX_ORDERS> m_Orders;
	static COrderManager ms_instance;
	COrdersArrayHandler* m_OrdersArrayHandler;
};

#endif // INC_INCIDENT_H_
