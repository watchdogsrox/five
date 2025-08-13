// Title	:	IncidentManger.h
// Purpose	:	Manages currently active incidents
// Started	:	13/05/2010

#ifndef INC_INCIDENT_MANAGER_H_
#define INC_INCIDENT_MANAGER_H_
// Rage headers
#include "Vector/Vector3.h"
// Framework headers
#include "ai/aichannel.h"
#include "scene/Entity.h"
// Game headers
#include "Game/Dispatch/DispatchEnums.h"
#include "Game/Dispatch/Incidents.h"
#include "scene/RegdRefTypes.h"

class CIncident;
class CIncidentsArrayHandler;

// Incident manager
class CIncidentManager
{
	friend class CIncidentsArrayHandler; // this needs direct access to the incidents array

public:

	enum {MAX_INCIDENTS = 50};

	// A wrapper class for an incident pointer, required by the network array syncing code. The network array handlers deal with arrays of
	// fixed size elements, and are not set up to deal with pointers to different classes of elements. This is an adaptor class used to get 
	// around that problem.
	class CIncidentPtr
	{
	public:
		CIncidentPtr() : m_pIncident(NULL) {}

		CIncident* Get() const
		{
			return m_pIncident;
		}

		void Set(CIncident* pIncident);
		void Clear(bool bDestroyIncident = true);
		void Copy(const CIncident& incident);

		netObject* GetNetworkObject() const;

		void Migrate();

		void Serialise(CSyncDataBase& serialiser)
		{
			u32 incidentType = m_pIncident ? m_pIncident->GetType() : CIncident::IT_Invalid;

			if (serialiser.GetIsMaximumSizeSerialiser())
				incidentType = LARGEST_INCIDENT_TYPE;

			SERIALISE_INCIDENT_TYPE(serialiser, incidentType, "Incident Type");

			if (m_pIncident && static_cast<u32>(m_pIncident->GetType()) != incidentType)
			{
				Clear();
			}

			if (!m_pIncident)
			{
				m_pIncident = CreateIncident(incidentType);
			}
			
			if (m_pIncident)
			{
				Assert(static_cast<u32>(m_pIncident->GetType()) == incidentType);
				m_pIncident->Serialise(serialiser);
			}
		}


	protected:

		CIncident* CreateIncident(u32 incidentType);

		static unsigned LARGEST_INCIDENT_TYPE;

		CIncident* m_pIncident;
	};

public:
	CIncidentManager();
	~CIncidentManager();
	
	void InitSession();
	void ShutdownSession();

	CIncident* AddIncident(const CIncident& incident, bool bTestForEqualIncid, int* incidentSlot = NULL);
	CIncident* AddFireIncident(CFire* pFire);
	void Update();
	void ClearIncident(CIncident* pIncident);
	void ClearIncident(s32 iCount, bool bSetDirty = true);
	void ClearAllIncidents(bool bSetDirty = true);
	void ClearAllIncidentsOfType(s32 iType);
	void ClearExcitingStuffFromArea(Vec3V_In center, const float& radius);	// Called from CLEAR_AREA
	const CIncident* FindIncidentOfTypeWithEntity(u32 uIncidentType, const CEntity& rEntity) const;

	void RemoveEntityFromIncidents(const CEntity& entity);

	s32 GetMaxCount() const { return m_Incidents.GetMaxCount(); }
	CIncident* GetIncident(s32 iCount) const;
	CIncident* GetIncidentFromIncidentId(u32 incidentID, unsigned* incidentIndex = NULL) const;

	s32 GetIncidentIndex(CIncident* pIncident) const;

	u8 GetIncidentCount() const { return m_IncidentCount; }

	CIncidentsArrayHandler* GetIncidentsArrayHandler() const { return m_IncidentsArrayHandler; }

#if __BANK
	void DebugRender() const;
	static bool s_bRenderDebug;
	static void RenderDispatchedTypesForIncident(s32& iNoTexts, Color32 debugColor, atArray<CIncident*>& pIncidentArray);
	static Vector2 GetTextPosition(s32& iNoTexts, s32 iIndentLevel = 0);
	static Color32 GetColorForDispatchType(DispatchType dt);
	static bool		IsPedInExclusionList(CPed* pPed, atArray<CPed*>& pExcludedPedsArray);
#endif

	static CIncidentManager& GetInstance() { return ms_instance; }

	// returns true if the incident at this index is controlled by our machine in a network game
	bool IsIncidentLocallyControlled(s32 index) const;

	// returns true if the incident at this index is locally controlled, or not controlled by any machines
	bool IsIncidentRemotelyControlled(s32 index) const;

	// called when the incident is altered and an update is required to be sent out
	void SetIncidentDirty(s32 index) const;

	// returns true if this slot is free and not controlled by any machines
	bool IsSlotFree(s32 index) const;

	// returns an incident that this fire can be associated with
	CFireIncident* FindFireIncidentForFire(CFire* pFire);

protected:

	// adds the incident to the specified slot. Returns true if the incident was correctly added.
	bool AddIncidentAtSlot(s32 index, CIncident& incident);

	// flags the incident index as dirty so that it is sent across the network
	void DirtyIncident(s32 index);

private:
	atFixedArray<CIncidentPtr, MAX_INCIDENTS> m_Incidents;
	static CIncidentManager ms_instance;
	u8 m_IncidentCount;
	CIncidentsArrayHandler* m_IncidentsArrayHandler;
};

#endif // INC_INCIDENT_H_
