// FILE :    VehicleAILod.h
// PURPOSE : Controls the LOD of the vehicles AI/physics update.
// CREATED : 16-03-2011

#ifndef VEHICLE_AILOD_H
#define VEHICLE_AILOD_H


// Rage headers
#include "system/bit.h"

// Framework headers
#include "fwutil/Flags.h"

// Game headers
#include "VehicleAI/VehicleAILodManager.h"

// Forward Definitions
namespace rage { class bkBank; }
class CVehicle;



// CPedAILod controls the allocation
class CVehicleAILod
{
	friend CVehicleAILodManager;
public:

	// Lodding flags
	enum AiLodFlags
	{
		AL_LodEventScanning					= BIT0,	// Core AI event scanning

		//----------------------------------------------
		// dummy inst = true
		// collisions = true
		// gravity = true
		// dummy constraint = true
		// wheel collisions = virtual road
		// transmission = true
		// wheel integrator = true

		AL_LodDummy							= BIT1,	

		//----------------------------------------------
		// dummy inst = true
		// collisions = false
		// gravity = false
		// dummy constraint = false
		// wheel collisions = path nodes
		// transmission = false
		// wheel integrator = false

		AL_LodSuperDummy					= BIT2,

		//----------------------------------------------
		// dummy inst = false
		// collisions = true
		// gravity = true
		// dummy constraint = false
		// wheel collisions = false
		// transmission = false
		// wheel integrator = false

		AL_LodTimeslicing					= BIT4,		// Timeslicing of whole updates to vehicle AI and other processing

		AL_DefaultLodFlags					= 0,
		AL_DefaultMissionBlockedLodFlags	= AL_LodEventScanning|AL_LodDummy|AL_LodSuperDummy,
		AL_DefaultRandomBlockedLodFlags		= 0 // what does this mean?
	};

	CVehicleAILod();
	~CVehicleAILod();

	// Sets/Gets a new set of LOD flags
	bool		IsLodFlagSet(const s32 iFlag) const		{ return m_LodFlags.IsFlagSet(iFlag); }

	// Disable a lod state - forcing lod state will override this
	void		SetBlockedLodFlags(const s32 val)			{ m_BlockedLodFlags.SetAllFlags(val); }
	void		SetBlockedLodFlag(const s32 iFlag)		{ m_BlockedLodFlags.SetFlag(iFlag); }
	void		ClearBlockedLodFlag(const s32 iFlag)		{ m_BlockedLodFlags.ClearFlag(iFlag); }
	bool		IsBlockedFlagSet(const s32 iFlag) const	{ return m_BlockedLodFlags.IsFlagSet(iFlag); }

	// Force a lod state always on
	void		SetForcedLodFlags(const s32 val)			{ m_ForcedLodFlags.SetAllFlags(val); }
	void		SetForcedLodFlag(const s32 iFlag)			{ m_ForcedLodFlags.SetFlag(iFlag); }
	void		ClearForcedLodFlag(const s32 iFlag)		{ m_ForcedLodFlags.ClearFlag(iFlag); }
	bool		IsForcedFlagSet(const s32 iFlag) const	{ return m_ForcedLodFlags.IsFlagSet(iFlag); }

	eVehicleDummyMode GetDummyMode() const;

private:

	// Only settable via the lod manager
	void		SetLodFlags(const s32 iFlags);
	void		SetLodFlag(const s32 iFlag)				{ SetLodFlags(m_LodFlags|iFlag); }
	void		ClearLodFlag(const s32 iFlag)			{ SetLodFlags(m_LodFlags&~iFlag); }

	fwFlags32 m_LodFlags;				// Stores the current set of LOD flags
	fwFlags32 m_BlockedLodFlags;		// Stores a set of blocked lod flags, reset each frame
	fwFlags32 m_ForcedLodFlags;			// Stores a set of forced lod flags, set until disabled

	// PURPOSE:	When in timesliced mode, this is true if this vehicle should do a full update on this frame.
	bool m_TimeslicedUpdateThisFrame;

#if __BANK
	fwFlags32 m_DebugLowLodFlags;		// Override default lod flags
	fwFlags32 m_DebugHighLodFlags;		// Override default lod flags
#endif // __BANK
};

////////////////////////////////////////////////////////////////////////////////

inline void	CVehicleAILod::SetLodFlags(const s32 iFlags)
{
	s32 iNewFlags = iFlags;
	iNewFlags |= m_ForcedLodFlags;
#if __BANK
	iNewFlags &= ~m_DebugHighLodFlags;
	iNewFlags |=  m_DebugLowLodFlags;
#endif // __BANK
	m_LodFlags.SetAllFlags(iNewFlags);
}



#endif // VEHICLE_AILOD_H


