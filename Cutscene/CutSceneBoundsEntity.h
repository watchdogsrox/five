/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneBoundsEntity.cpp
// PURPOSE : A class to describe a bounding class for removal of objects and 
//			 blocking of areas.
// AUTHOR  : Thomas French
//
/////////////////////////////////////////////////////////////////////////////////


#ifndef CUTSCENE_BOUNDS_ENITY_H
#define CUTSCENE_BOUNDS_ENITY_H

//Rage
#include "vector/vector3.h"

//Game
#include "scene/Entity.h"
#include "cutfile/cutfobject.h"
#include "cutscene/cutsEntity.h"
#include "cutscene/cutscenedefine.h"
#include "cutscene/cutsevent.h"
#include "cutscene/cutsmanager.h"
#include "cutscene/cutseventargs.h"

#if __BANK
#define cutsceneBoundsEntityDebugf( fmt,...) if ( GetObject() ) { char debugStr[512]; CommonDebugStr(GetObject(), debugStr); cutsceneDebugf1("%s" fmt, debugStr,##__VA_ARGS__); }
#else
#define cutsceneBoundsEntityDebugf( fmt,...) do {} while(false)
#endif //__BANK

struct ClosestObjectToHideDataStruct
{
	CEntity* pClosestObject;
	u32 ModelIndexToCheck;

	float fClosestDistanceSquared;
	Vector3 CoordToCalculateDistanceFrom;
	
};

class CCutSceneBlockingBoundsEntity : public cutsUniqueEntity
{
public:

	CCutSceneBlockingBoundsEntity(const cutfObject* pObject); 
	~CCutSceneBlockingBoundsEntity(); 

	virtual void DispatchEvent( cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs, const float fTime = 0.0f , const u32 StickyId = 0);
	
	bool IsInside(const Vector3 &vVec); 

private:
	void AddPedGenBlockingArea(cutsManager* pManager); 
	
	void AddPedWanderBlockingArea(); 
	
	void AddPedScenarioBlockingArea();

	void RemovePedWanderBlockingArea(); 

	void RemovePedsAndCars(); 
	
	s32 GetType() const { return CUTSCENE_BLOCKING_BOUNDS_OBJECT_TYPE; } 

	const cutfBlockingBoundsObject* m_pBlockingBounds;
		
	Vector3 m_vWorldCornerPoints[4];	//corners in world coords.	
	
	Matrix34 m_mArea;					//Describe an angled area as a matrix and two vectors in scene space.
	
	Vector3 m_vAreaMin;
	
	Vector3 m_vAreaMax; 
	
	s32 m_ScenarioBlockingBoundId; 
#if __BANK
	virtual void DebugDraw() const; 
	void CommonDebugStr(const cutfObject* pObject, char * debugStr);
#endif
};


///////////////////////////////
// Hidden bounds
///////////////////////////////

class CCutSceneHiddenBoundsEntity : public cutsUniqueEntity
{
public:
	CCutSceneHiddenBoundsEntity(const cutfObject* pObject); 
	~CCutSceneHiddenBoundsEntity();

	virtual void DispatchEvent( cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs, const float fTime = 0.0f, const u32 StickyId = 0);
	
	virtual s32 GetType() const { return CUTSCENE_HIDDEN_OBJECT_GAME_ENTITY;  } 
	
private:
	
	void ComputeModelIndex(); 
	
	bool SetObjectInAreaVisibility(const Vector3 &vPos, float fRadius, s32 iModelIndex, bool bVisble);
	
	const cutfHiddenModelObject* m_pHiddenObject;

	u32 m_iModelIndex; 
	
	RegdEnt m_pEntity; 
	
	bool m_bSkippedCalled; 

	bool m_bHaveHidden; 
	bool m_bShouldHide; 
	bool m_bShouldShow; 
	bool m_bHaveShown; 

#if __BANK
	virtual void DebugDraw() const;
	void CommonDebugStr(const cutfObject* pObject, char * debugStr);
#endif
};

///////////////////////////////
// Removal bounds
///////////////////////////////

class CCutSceneFixupBoundsEntity : public cutsUniqueEntity
{
public:
	CCutSceneFixupBoundsEntity(const cutfObject* pObject); 
	~CCutSceneFixupBoundsEntity();

	virtual void DispatchEvent( cutsManager*pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs, const float fTime = 0.0f, const u32 StickyId = 0 );

	virtual s32 GetType() const { return CUTSCENE_FIX_UP_OBJECT_GAME_ENTITY;  } 

private:
	void FixupRequestedObjects(const Vector3 &vPos, float fRadius, s32 iModelIndex);
	
	const cutfFixupModelObject* m_pFixupObject; 

	u32 m_iModelIndex; 

#if __BANK
	void CommonDebugStr(const cutfObject* pObject, char * debugStr);
#endif
};

#endif