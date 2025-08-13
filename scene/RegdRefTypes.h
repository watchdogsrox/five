/////////////////////////////////////////////////////////////////////////////////
// Title	:	RegdRefTypes.h
// Author	:	Adam Croston
// Started	:	11/06/08
//
// The fwRegdRef template and associated typedefs are designed to make it so we
// can remove most of the code that manages the REGREFing and TIDYREFing of
// various types of pointers across the code-base...
// In general it should be both much easier to use and much more reliable than
// doing such REFing by hand.
//
// Some info about the fwRegdRef template:
//	Any class passed in as a template parameter must be derived from
//		CRefAwareBase. 
//	Its Constructor will call AddKnownRef for you (if the pointer passed in
//		isn't just null). 
//	Its Destructor will call RemoveKnownRef for you (if the stored pointer
//		isn't just null). 
//	It's Operator= does the right thing in regards to RemoveKnownRef and
//		AddKnownRef on the old and new pointers when necessary and appropriate. 
//	Its dereferencing operators (Operator*, operator->, etc.) returns the
//		pointer or asserts if it is null. 
//	It has is a Get() function to save your butt when you really need direct
//		access to the pointer. 
//
// A few specific types of fwRegdRef are already defined for convenience:
//	typedef	fwRegdRef<class CEntity>				RegdEnt;
//	typedef	fwRegdRef<const class CEntity>			RegdConstEnt;
//	typedef	fwRegdRef<class CDynamicEntity>			RegdDyn;
//	typedef	fwRegdRef<class CPhysical>				RegdPhy;
//	typedef	fwRegdRef<class CPed>					RegdPed;
//	typedef	fwRegdRef<class CPlayerSettingsObject>	RegdPlayerSettingsObject;
//	typedef	fwRegdRef<class CVehicle>				RegdVeh;
//	typedef	fwRegdRef<class CObject>				RegdObj;
//	typedef fwRegdRef<class CAutomobile>			RegdAutomobile;
//	typedef	fwRegdRef<class CBoat>					RegdBoat;
//	typedef	fwRegdRef<class CTrain>					RegdTrain;
//	typedef	fwRegdRef<class CLightEntity>			RegdLight;
//
// These should be true drop in replacements for the equivalent pointers so
// you will be able to simply replace things like "CEntity* m_target" with
// "RegdEnt m_target" and have everything else Just Work (tm).  You shouldn’t
// ever need to use anything except the typedefs above in your code. Also,
// you don't have to include ped.h in your header just to use RegdPed- the
// types above are quasi forward declared in entity.h, so if you already have
// that included (which most headers seem to have) then you are good to go.
//
// You should be looking for entity pointers in your classes (that are
// probably already at least partially REGREFd and TIDYREFd) and replace them
// with the equivalent fwRegdRef.
//
// Once you have replaced a pointer with an equivalent fwRegdRef remember to
// remove all the old REGREFs and TIDYREFs on that var!!!
//
// You will probably find yourself simply removing most REF related code in
// constructors and destructors.
//
// You will also find yourself changing code like:
//	if(m_pPedTarget)
//	{
//		TIDYREF(m_pPedTarget, (CEntity**)&m_pPedTarget);
//	}
//	m_pPedTarget = pPed;
//	if(m_pPedTarget)
//	{
//		REGREF(m_pPedTarget, (CEntity**)&m_pPedTarget);
//	}
//
// To simply be:
//	m_pPedTarget = pPed;
//
// If you run into any weird compilation errors you can usually just replace
// "m_target" with "m_target.Get()" and things will be fine. (Or you can
// whine about it to me.)  Because of all the wonderful and weird ways we
// use pointer I had to overload all sorts of operators, conversions, and
// casts and I might not have gotten it quite right, so it’s possible there
// are some odd errors lurking in here.  Please help me by keeping an eye
// out for any such things.  Not to worry too much though; if we run into
// any problematic operators, conversions, or casts we can just remove them
// and use good ole ".Get()" In the calling code to get around any problems.
/////////////////////////////////////////////////////////////////////////////////
#ifndef REGD_REF_TYPES_H_
#define REGD_REF_TYPES_H_

#include "fwtl\regdrefs.h"


///////////////////////////////////////////////////////////////////////////////
// Non entity based fwRegdRef, defined here for convenience.
//
// These should be true drop in replacements for the equivalent pointers.
// You should be able to replace "CRefAwareBase* m_target" with
// "RefAwareBaseRef m_target" and have everything else just work (tm).
// If you run into any wierd compilation errors you can usually just replace
// "m_target" with "m_target.Get()" and things will be fine.
///////////////////////////////////////////////////////////////////////////////
typedef	fwRegdRef<fwRefAwareBase>		RefAwareBaseRef;

// Make "forward declared" RegdRefs for the camera system objects and add
// comparisons of them to RegdCamBaseObject objects where appropriate.
typedef	fwRegdRef<class camBaseObject>			RegdCamBaseObject;
typedef	fwRegdRef<const class camBaseObject>	RegdConstCamBaseObject;
typedef	fwRegdRef<class camBaseCamera>			RegdCamBaseCamera;
typedef	fwRegdRef<const class camBaseCamera>	RegdConstCamBaseCamera;
typedef	fwRegdRef<class camBaseDirector>		RegdCamBaseDirector;
typedef	fwRegdRef<const class camBaseDirector>	RegdConstCamBaseDirector;
typedef	fwRegdRef<class camFrame>				RegdCamFrame;
typedef	fwRegdRef<const class camFrame>			RegdConstCamFrame;
typedef	fwRegdRef<class camBaseFrameShaker>		RegdCamBaseFrameShaker;
typedef	fwRegdRef<const class camBaseFrameShaker> RegdConstCamBaseFrameShaker;
typedef	fwRegdRef<class camCollision>			RegdCamCollision;
typedef	fwRegdRef<const class camCollision>		RegdConstCamCollision;

MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdCamBaseObject, RegdConstCamBaseObject);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdCamBaseCamera, RegdCamBaseObject);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdCamBaseCamera, RegdConstCamBaseCamera);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdCamBaseDirector, RegdCamBaseObject);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdCamBaseFrameShaker, RegdCamBaseObject);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdCamCollision, RegdCamBaseObject);

// Make a "forward declared"  RegdRef to CDummyObject...
typedef	fwRegdRef<class CDummyObject>	RegdDummyObject;

// Make a "forward declared"  RegdRef to patrol nodes and links.
// This is done here so that files can work on them, but without having to
// actually include the main patrol headers, after i read the comment above :)
typedef	fwRegdRef<class CPatrolNode>	RegdPatrolNode;
typedef	fwRegdRef<class CPatrolLink>	RegdPatrolLink;
// Relationship groups
typedef	fwRegdRef<class CRelationshipGroup>	RegdRelationshipGroup;
// Incidents
typedef	fwRegdRef<class CIncident>			RegdIncident;
typedef	fwRegdRef<const class CIncident>	RegdConstIncident;
// Orders
typedef	fwRegdRef<class COrder>				RegdOrder;

// Make "forward declared" RegdRefs for the weapon system objects and add
// comparisons of them to RegdCamBaseObject objects where appropriate.
typedef	fwRegdRef<class CWeapon>			RegdWeapon;
typedef	fwRegdRef<const class CWeaponInfo>	RegdConstWeaponInfo;
// Action Manager
typedef	fwRegdRef<const class CActionResult>		RegdConstActionResult;
typedef	fwRegdRef<const class CActionDefinition>	RegdConstActionDefinition;

///////////////////////////////////////////////////////////////////////////////
// Entity based fwRegdRef, defined here for convenience.
//
// Quasi forward declared here, so the definition doesn't have to live in
// the same place as CEntity/CPed (and thus force people to include it in
// places where they had previously just forward declared the pointer type).
//
// These should be true drop in replacements for the equivalent pointers.
// You should be able to replace "CEntity* m_target" with "RegdEnt m_target"
// and have everything else just work (tm).
// Remember to remove all those dame REGREFs and TIDYREFs on m_target once you
// have done this though!
// If you run into any weird compilation errors you can usually just replace
// "m_target" with "m_target.Get()" and things will be fine.
///////////////////////////////////////////////////////////////////////////////
typedef	fwRegdRef<class CEntity> RegdEnt;
typedef	fwRegdRef<const class CEntity>	RegdConstEnt;

// Make a "forward declared" RegdRef for CDynamicEntities and add comparison of
// them to RegdEnt objects.
typedef	fwRegdRef<class CDynamicEntity> RegdDyn;
typedef	fwRegdRef<const class CDynamicEntity> RegdConstDyn;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdDyn, RegdEnt);

// Make a "forward declared" RegdRef for CPhysicals and add comparison of
// them to RegdEnt objects.
typedef	fwRegdRef<class CPhysical> RegdPhy;
typedef	fwRegdRef<const class CPhysical> RegdConstPhy;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdPhy, RegdEnt);

// Make a "forward declared" RegdRef for CPeds and add comparison of them
// to RegdEnt objects.
typedef	fwRegdRef<class CPed> RegdPed;
typedef	fwRegdRef<const class CPed> RegdConstPed;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdPed, RegdEnt);

// Make a "forward declared" RegdRef for CPlayerSettingsObjects and add comparison of
// them to RegdPed and RegdEnt objects.
typedef	fwRegdRef<class CPlayerSettingsObject> RegdPlayerSettingsObject;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdPlayerSettingsObject, RegdPed);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdPlayerSettingsObject, RegdEnt);

// Make a "forward declared" RegdRef for CVehicles and add comparison of
// them to RegdEnt objects.
typedef	fwRegdRef<class CVehicle> RegdVeh;
typedef	fwRegdRef<const class CVehicle> RegdConstVeh;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdVeh, RegdEnt);

// Make a "forward declared" RegdRef for CObjects and add comparison of
// them to RegdEnt objects.
typedef	fwRegdRef<class CObject> RegdObj;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdObj, RegdEnt);

// Make a "forward declared" RegdRef for CDoors and add comparison of
// them to RegdEnt objects.
typedef	fwRegdRef<class CDoor> RegdDoor;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdDoor, RegdEnt);

// Make a "forward declared" RegdRef for CAutomobile and add comparison of
// them to RegdVeh and RegdEnt objects.
typedef fwRegdRef<class CAutomobile> RegdAutomobile;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdAutomobile, RegdVeh);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdAutomobile, RegdEnt);

// Make a "forward declared" RegdRef for CBoat and add comparison of
// them to RegdVeh and RegdEnt objects.
typedef fwRegdRef<class CBoat> RegdBoat;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdBoat, RegdVeh);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdBoat, RegdEnt);

// Make a "forward declared" RegdRef for CTrain and add comparison of
// them to RegdVeh and RegdEnt objects.
typedef fwRegdRef<class CTrain> RegdTrain;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdTrain, RegdVeh);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdTrain, RegdEnt);

// Make a "forward declared" RegdRef for CLightEntity and add comparison of
// them to RegdEnt objects.
typedef	fwRegdRef<class CLightEntity> RegdLight;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdLight, RegdEnt);

/// Make a "forward declared" RegdRef for CProjectile and add comparison of
// them to RegdEnt objects.
typedef fwRegdRef<class CProjectile> RegdProjectile;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdProjectile, RegdObj);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdProjectile, RegdEnt);

/// Make a "forward declared" RegdRef for CPickup and add comparison of
// them to RegdEnt objects.
typedef fwRegdRef<class CPickup> RegdPickup;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdPickup, RegdObj);
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdPickup, RegdEnt);

#endif // REGD_REF_TYPES_H_
