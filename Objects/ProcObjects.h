///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	"ProcObjects.h"
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	10 March 2004
//	WHAT:	Routines to handle procedurally created objects
//
///////////////////////////////////////////////////////////////////////////////

#ifndef PROCOBJECTS_H
#define	PROCOBJECTS_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "phcore/segment.h"
#include "physics/intersection.h"
#include "vector/vector3.h"

// framework
#include "vfx/vfxlist.h"

// user
#include "scene/RegdRefTypes.h"
#include "Objects/ProceduralInfo.h"
#include "physics/WorldProbe/shapetestresults.h"

///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

class C2dEffect;
class CEntity;
class CPlantLocTri;
class CPlantLocTriArray;

///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CEntityItem  /////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CEntityItem : public vfxListItem
{		
	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////	

	public: ///////////////////////////
	
		CEntity* 			m_pData;

		CPlantLocTri* 		m_pParentTri;
		RegdEnt 			m_pParentEntity;

		bool				m_allocatedMatrix;
		
#if __BANK
		float				triVert1[3];
		float				triVert2[3];
		float				triVert3[3];
#endif // __BANK

}; // CEntityItem

struct ProcObjectCreationInfo
{
	Vector3	pos;		// uw = pProcObjInfo
	Vector3 normal;		// uw = pLocTri
#if __64BIT
	CPlantLocTri* pLocTri;
	CProcObjInfo* pProcObjInfo;
#endif
};

struct ProcTriRemovalInfo
{
	CPlantLocTri* pLocTri;
	s32 procTagId;
};

//  CProcObjectMan  //////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CProcObjectMan
{	
	///////////////////////////////////
	// DEFINES 
	///////////////////////////////////

	#define	MAX_PROC_MATERIAL_INFOS	(256)
	#define MAX_PROC_COBJECTS		(200)
	#define MAX_PROC_CBUILDINGS		(2300)
	#define	MAX_ENTITY_ITEMS		(MAX_PROC_COBJECTS+MAX_PROC_CBUILDINGS)
	#define MAX_REMOVED_TRIS		(RSG_PC? 3072 : 1024)
	#define	MAX_ALLOCATED_MATRICES	(ENABLE_MATRIX_MEMBER? MAX_ENTITY_ITEMS : 1024)
	#define	MAX_SEGMENTS			(255)
	

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// constructor / destructor 
							CProcObjectMan				();
							~CProcObjectMan				();
		
		// 
		bool				Init					    ();
		void				Shutdown				    ();

		void 				Update						();	

		static s32			ProcessTriangleAdded		(CPlantLocTriArray& triTab, CPlantLocTri* pLocTri);
#if !__SPU
		static s32			ProcessTriangleAdded		(CPlantLocTri* pLocTri);
#endif
		void				ProcessTriangleRemoved		(CPlantLocTri* pLocTri, s32 procTagId);

		s32					ProcessEntityAdded			(CEntity* pEntity);
		void				ProcessEntityRemoved		(CEntity* pEntity);

		CEntityItem*		AddObject					(const Vector3& pos, const Vector3& normal, CProcObjInfo* pProcObjData, vfxList* pList, CEntity* pParentEntity, s32 seed);	
		void				RemoveObject				(CEntityItem* pEntityItem, vfxList* pList);

		CEntityItem*		GetEntityFromPool			();
		void				ReturnEntityToPool			(CEntityItem* pEntityItem);

		void				AddTriToRemoveList			(CPlantLocTri* pLocTri);
		void				LockListAccess				();
		void				UnlockListAccess			();

		s32					GetNumActiveObjects			();

		void				UpdateVisualDataSettings	();

#if __DEV
		bool				IsAllowedToDelete			();
#endif

#if __BANK
		void				InitWidgets					();
		void				RenderDebug					();
		void				RenderDebugEntityZones		();
		static void			ReloadData					();
#endif // __BANK


	private: //////////////////////////

#if __SPU
	public:
#endif
		static s32			AddObjects					(CPlantLocTriArray& triTab, CProcObjInfo* pProcObjInfo, CPlantLocTri* pLocTri);
#if __SPU
	private:
#endif
		s32					AddObjects					(CEntity* pEntity, C2dEffect* p2dEffect, s32 numObjsToCreate, phIntersection* pIntersections);
		void				AddObjectToAddList			(Vector3::Param pos, Vector3::Param normal, CProcObjInfo* pProcObjInfo, CPlantLocTri* pLocTri);
		void				ProcessAddAndRemoveLists	();	


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		
	
	public: ///////////////////////////
	
		s32 				m_numAllocatedMatrices;
		s32					m_numProcObjCObjects;								// CObjects as opposed to buildings
		s32					m_numProcObjCBuildings;								// CBuildings
		
#if __DEV
		bool				m_removingProcObject;
#endif

#if __BANK
		bool				m_disableEntityObjects;
		bool				m_disableCollisionObjects;
		bool				m_renderDebugPolys;
		bool				m_renderDebugZones;
		bool				m_ignoreMinDist;
		bool				m_ignoreSeeding;
		bool				m_enableSeeding;
		bool				m_forceOneObjPerTri;
		bool				m_printNumObjsSkipped;
		s32					m_bkNumActiveProcObjs;
		s32					m_bkMaxProcObjCObjects;
		s32					m_bkNumProcObjsToAdd;
		s32					m_bkNumProcObjsToAddSize;
		s32					m_bkNumProcObjsSkipped;
		s32					m_bkNumTrisToRemove;
		s32					m_bkNumTrisToRemoveSize;
#endif // __BANK
					
	private: //////////////////////////
		
		// pool and list of entity items
		CEntityItem			m_entityItems				[MAX_ENTITY_ITEMS];
		vfxList				m_entityPool;

		// list of entities created by objects (2d effects)
		vfxList				m_objectEntityList;

		phSegment						m_segments[MAX_SEGMENTS];
		phSegment*						m_pSegments[MAX_SEGMENTS];
		WorldProbe::CShapeTestHitPoint	m_intersections[MAX_SEGMENTS];
		WorldProbe::CShapeTestHitPoint*	m_pIntersections[MAX_SEGMENTS];

		atArray<ProcObjectCreationInfo>	m_objectsToCreate;
		atArray<ProcTriRemovalInfo>		m_trisToRemove;
		float							m_fMinRadiusForInclusionInHeightMap;

}; // CProcObjectMan

// wrapper class needed to interface with game skeleton code
class CProcObjectManWrapper
{
public:

    static void Init(unsigned initMode);
    static void Shutdown(unsigned shutdownMode);
    static void Update();
};

///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern CProcObjectMan	g_procObjMan;


#endif // PROCOBJECTS_H
