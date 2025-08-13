#ifndef _ANCHORHELPER_H_
#define _ANCHORHELPER_H_

// RAGE headers:
#include "vector/vector3.h"

// Game headers:


// Forward declarations:
class CVehicle;


// This is a separate class because at least submarines need to anchor too and they aren't derived from CBoat.
class CAnchorHelper
{
public:
	CAnchorHelper(CVehicle* pParent=NULL);

	static bool IsVehicleAnchorable(const CVehicle* pVehicle);
	// Used to simplify getting the anchor helper on arbitrary vehicles. Use after checking if this vehicle is of
	// anchorable type. Asserts and returns NULL otherwise.
	static CAnchorHelper* GetAnchorHelperPtr(CVehicle* pVehicle);
	static const CAnchorHelper* GetAnchorHelperPtr(const CVehicle* pVehicle) { return const_cast<const CAnchorHelper*>(GetAnchorHelperPtr(const_cast<CVehicle*>(pVehicle))); }

	void SetParent(CVehicle* pParent) { m_pParent = pParent; }

	void Anchor(bool bAnchor, bool bForceConstraints=false);
	bool IsAnchored() const { return m_nFlags.bLockedToXY==1; }
	bool CanAnchorHere( bool excludeAllPedsStandingOnParent = false ) const;
	bool UsingLowLodMode() const { return m_nFlags.bLowLodAnchorMode==1; }
	bool ShouldForcePlayerBoatToRemainAnchored() const { return m_nFlags.bForcePlayerBoatAnchor==1; }
	void ForcePlayerBoatToRemainAnchored(bool bValue) { m_nFlags.bForcePlayerBoatAnchor = bValue ? 1 : 0; }
	bool ShouldAlwaysUseLowLodMode() const { return m_nFlags.bForceLowLodMode==1; }
	void ForceLowLodModeAlways(bool bValue) { m_nFlags.bForceLowLodMode = bValue ? 1: 0; }

	void SetLowLodMode(bool bValue) { m_nFlags.bLowLodAnchorMode = bValue ? 1 : 0; }

	// Return the horizontal angular difference between orientation of the boat when physically constrained and the
	// current orientation when switching back from low-LOD buoyancy mode.
	float GetYawOffsetFromConstrainedOrientation() const;

//private: // TODO - Would be nice to make this private, but first we need to move the Low LOD anchor stuff in here.
	void CreateAnchorConstraints();

#if __BANK
public:
	void RenderDebug() const;

	static bool ms_bDebugModeEnabled;
#endif // __BANK

private:
	Vector3 m_vAnchorPosProw;
	Vector3 m_vAnchorPosStern;
	Vector3 m_vAnchorFwdVec;
	Vector3 m_vAnchorSideVec;

	CVehicle* m_pParent;

	struct AnchorFlags
	{
		u8 bLockedToXY 				: 1;
		u8 bLowLodAnchorMode		: 1;
		u8 bForcePlayerBoatAnchor	: 1;
		u8 bForceLowLodMode			: 1;
	};
	AnchorFlags m_nFlags;
};

#endif //_ANCHORHELPER_H_
