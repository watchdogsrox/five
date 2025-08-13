/////////////////////////////////////////////////////////////////////////////////
// FILE :		PedHelmetComponent.h
// PURPOSE :	A class to hold the helmet model/position data for a ped/player
//				Pulled out of CPed and CPedPlayer classes.
// AUTHOR :		Mike Currington.
// CREATED :	06/12/10
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_PEDHELMETCOMPONENT_H
#define INC_PEDHELMETCOMPONENT_H

// rage includes
#include "vector/vector3.h"

// game includes
#include "Peds/rendering/PedProps.h"
#include "Peds/rendering/pedvariationds.h"

// Forward definitions
class CPed;

//Hack
#define ALTERNATIVE_FLIGHT_HELMET_PLAYER_TWO 21
#define ALTERNATIVE_FLIGHT_HELMET_PLAYER_ONE 20
#define ALTERNATIVE_FLIGHT_HELMET_PLAYER_ZERO 27
#define ALTERNATIVE_HELMET_PLAYER_TWO_ARMY 11		//Trevor alternate helmet (army-style)
#define ALTERNATIVE_HELMET_PLAYER_TWO_MOTOX 23		//Trevor alternate helmet (moto-x style)

class CPedHelmetComponent
{
public:
	CPedHelmetComponent();	// deliberately not implemented
	CPedHelmetComponent(CPed * pParentPed);

	void Update(void);
	void UpdateInactive(const bool okToRemoveImmediately);

	bool IsHelmetEnabled() const;
	bool IsHelmetInHand();
	bool EnableHelmet(eAnchorPoints anchorOverrideID = ANCHOR_NONE, bool bTakingOff = false); 
	void DisableHelmet(bool bDropHelmet = false, const Vector3* impulse = NULL, bool bSafeRestore = false);
	void RestoreOldHat(bool bSafeRestore);
	bool HasPilotHelmetEquippedInAircraftInFPS(bool bTreatBonnetCameraAsFirstPerson) const;

	void SetStoredHatIndices(int nPropIndex, int nTexIndex);
	void RestoreStoredHatIfSafe(const float fMinDistance);	// Restore the stored hat if in range and not wearing a helmet, clear out the stored hat data
	bool RestoreStoredHat();									// Restore the stored hat (always), clear out the stored hat data.  Returns true if the hat model was set

	bool HasHelmetOfType(ePedCompFlags PropFlag = PV_FLAG_DEFAULT_HELMET) const;
	bool HasHelmetForVehicle(const CVehicle* pVehicle) const;
	bool IsAlreadyWearingAHelmetProp(const CVehicle* pVehicle) const;
	bool IsWearingHelmetWithGoggles();
	bool IsHelmetTexValid(CPed* pPed, s32 HelmetTexId, ePedCompFlags PropFlag);

	int GetStoredHatPropIndex() const { return m_nStoredHatPropIdx; }
	int GetStoredHatTexIndex()  const { return m_nStoredHatTexIdx; }

	void SetPropFlag(ePedCompFlags propflag);
	ePedCompFlags GetPropFlag() { return m_CurrentPropFlag; }
	
	int GetNetworkHelmetIndex() const { return m_NetworkHelmetIndex; }
	int GetNetworkTextureIndex() const { return m_NetworkTextureIndex; }
	void SetNetworkHelmetIndex(int const index) { m_NetworkHelmetIndex = index; }
	void SetNetworkTextureIndex(int const index) { m_NetworkTextureIndex = index; }

	// Override the random helmet texture/prop index with a specific one
	void SetHelmetTexIndex(int nHelmetTexIndex, bool bScriptGiveHelmet = false);
	void SetHelmetPropIndex(int nHelmetPropIndex, bool bIncludeBicycles = true);

	//Helmet streaming
	bool RequestPreLoadProp(bool bRestoreHat = false);
	bool IsPreLoadingProp();
	bool HasPreLoadedProp();
	void ClearPreLoadedProp();

	inline void RegisterPendingHatRestore(void) { Assert(!m_bPendingHatRestore); m_bPendingHatRestore = true;	}
	inline bool IsPendingHatRestore(void)		{ return m_bPendingHatRestore;									}

	// B*2216490: Functions accessed from script to define vehicle specific helmet prop indexes.
	static void AddVehicleHelmetAssociation(const u32 uVehicleHash, const int iHelmetPropIndex);
	static void ClearVehicleHelmetAssociation(const u32 uVehicleHash);
	static void ClearAllVehicleHelmetAssociations();
	// B*2216490: Set/clear the helmet index from the hash map.
	void SetHelmetIndexFromHashMap();
	void ClearHelmetIndexUsedFromHashMap() { m_nHelmetPropIndexFromHashMap = -1; }

	static bool ShouldUsePilotFlightHelmet(const CPed *pPed);
	static fwMvClipSetId GetAircraftPutOnTakeOffHelmetFPSClipSet(const CVehicle* pVehicle);
	
	int GetHelmetPropIndexOverride() const { return m_nHelmetPropIndex; }

	void SetVisorPropIndices(bool bSupportsVisors, bool bVisorUp, s32 HelmetVisorUpPropId, s32 HelmetVisorDownPropId);
	void SetCurrentHelmetSupportsVisors(bool bSupportsVisors) { m_bSupportsVisors = bSupportsVisors; }
	bool GetCurrentHelmetSupportsVisors() const { return m_bSupportsVisors; }
	void SetIsVisorUp(bool bUp) { m_bIsVisorUp = bUp; }
	bool GetIsVisorUp() const { return m_bIsVisorUp; }
	void SetHelmetVisorUpPropIndex(int const index) { m_nHelmetVisorUpPropIndex = index ; }
	void SetHelmetVisorDownPropIndex(int const index) { m_nHelmetVisorDownPropIndex = index ; }
	int GetHelmetVisorUpPropIndex() const { return m_nHelmetVisorUpPropIndex; }
	int GetHelmetVisorDownPropIndex() const { return m_nHelmetVisorDownPropIndex; }

#if __BANK
	static void InitWidgets();
#endif	// __BANK

protected:

	s32 GetHelmetPropIndex(ePedCompFlags PropFlag) const;
	int GetHelmetTexIndex(int nPropIndex);

	bool UseOverrideInMP() const;

#if __BANK
	// Static RAG widget functions
	static void WidgetAddVehicleHelmetAssociation();
	static void WidgetClearVehicleHelmetAssociation();
	static void WidgetClearAllVehicleHelmetAssociations();
#endif	// __BANK
	
protected:

	CPed *			m_pParentPed;

	// Helmet vars
	float			m_fHelmetInactiveDeletionTimer;				// timer to determine whether helmet should be removed
	int				m_nHelmetVisorUpPropIndex;					// the index of the version of the helmet with visor up (if it supports the visor feature)
	int				m_nHelmetVisorDownPropIndex;				// the index of the version of the helmet with visor down (if it supports the visor feature)
	bool			m_bIsVisorUp;								// bool to signify if current version of the helmet has visor up or down
	bool			m_bSupportsVisors;							// bool to track if current helmet supoports visors
	int				m_nHelmetPropIndex;							// the index of the helmet prop to use
	int				m_nHelmetTexIndex;							// the index of the helmet texture to use
	bool			m_bUseOverridenHelmetPropIndexOnBiycles;	// Defaulted to true. If set to false (from SET_HELMET_PROP_INDEX), we won't use m_nHelmetPropIndex when on a bicycle.
	int				m_iMostRecentVehicleType;					// Stores the type of vehicle the ped was last in. Set in Update(), used in GetHelmetPropIndex().
	int				m_nStoredHatPropIdx;						// store the active head prop before we enable the helmet
	int				m_nStoredHatTexIdx;
	ePedCompFlags	m_CurrentPropFlag;
	int				m_NetworkHelmetIndex;						// The Network index we're wearing (used for syncing)
	int				m_NetworkTextureIndex;						// The Network texture index we're wearing (used for syncing)
	ePedCompFlags	m_DesiredPropFlag;
	bool			m_bPreLoadingProp;							// Are we pre loading the helmet prop
	int				m_nPreLoadedTexture;						// Store the preloaded texture to be put on

	bool			m_bPendingHatRestore:1;						// We need to restore the hat outside of a task running as it's been aborted before a slot is free to use

	// B*2216490: Hash map to define an array of valid helmets for the a specific vehicle.
	//static typedef atMap<u32, atArray<int>> VehicleHashToHelmetPropIdxMap;
	static atMap<u32, atArray<int> > sm_VehicleHashToHelmetPropIndex;
	int m_nHelmetPropIndexFromHashMap;

#if __BANK
	// Static RAG member variables
	static atHashString sm_uVehicleHashString;
	static int sm_iHelmetPropIndex;
#endif	// __BANK

};


#endif // INC_PEDHELMETCOMPONENT_H
