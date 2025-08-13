//
// filename:	AnimatedBuilding.h
// description:	
//

#ifndef INC_ANIMATEDBUILDING_H_
#define INC_ANIMATEDBUILDING_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "fwanimation/animdirector.h"
#include "fwtl/pool.h"
// Game headers
#include "DynamicEntity.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------
class CMoveAnimatedBuilding;


//
// name:		CAnimatedBuilding
// description:	Describes an animating non physical object in the map
class CAnimatedBuilding : public CDynamicEntity
{
public:
	enum
	{
		FORCEANIM_DIST_MAX		= 400,	// max distance to camera for animated buildings which are forced to animate
		FORCEANIM_SIZE_MIN		= 5		// min size (bounding radius) for animated buildings which are forced to animate
	};

protected:

	virtual void Add();
	virtual void Remove();
	virtual void AddToInterior(fwInteriorLocation interiorLocation);
	virtual void RemoveFromInterior();

#if !__SPU
	virtual fwMove *CreateMoveObject();
	virtual const fwMvNetworkDefId &GetAnimNetworkMoveInfo() const;
#endif // !__SPU

public:
	CAnimatedBuilding(const eEntityOwnedBy ownedBy);
	~CAnimatedBuilding();

	FW_REGISTER_CLASS_POOL(CAnimatedBuilding);	// was 40000

	CMoveAnimatedBuilding &GetMoveAnimatedBuilding()			{ Assert(GetAnimDirector()); return (CMoveAnimatedBuilding&) (GetAnimDirector()->GetMove()); }
	void UpdateRateAndPhase();

	virtual void InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex);
	virtual bool CreateDrawable();
	virtual void DeleteDrawable();
	virtual bool RequiresProcessControl() const { return true; }
	virtual bool ProcessControl();

#if !__NO_OUTPUT
	static void PrintSkeletonData();
#endif
};

// --- Globals ------------------------------------------------------------------

#endif // !INC_ANIMATEDBUILDING_H_
