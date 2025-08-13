
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    objectpopulation.h
// PURPOSE : Deals with creation and removal of objects
// AUTHOR :  Obbe.
// CREATED : 10/05/06
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _OBJECTPOPULATION_H_
#define _OBJECTPOPULATION_H_

// rage includes
#include "spatialdata/sphere.h"

// fw includes
#include "atl/array.h"
#include "entity/archetypemanager.h"

// game includes
#include "pathserver/NavGenParam.h"		// For NAVMESH_EXPORT
#include "scene/Entity.h"
#include "scene/EntityId.h"
#include "scene/EntityTypes.h"
#include "scene/RegdRefTypes.h"
#include "vector/vector3.h"

#define OBJECT_POPULATION_RESET_RANGE			(80.0f)
#define OBJECT_POPULATION_RESET_RANGE_SQR		(80.0f*80.0f)

#define NUM_CLOSE_DUMMIES_PER_FRAME				(10)
#define NUM_PRIORITY_DUMMIES_PER_FRAME			(32)
#define NUM_NEVERDUMMY_PROCESSED				(50)

struct sCloseDummy
{
	RegdEnt ent;
	float dist;
};

class CDummyObject;
class CObject;


enum
{
	CLEAROBJ_FLAG_BROADCAST						= BIT(0),		// broadcast event for network game
	CLEAROBJ_FLAG_FORCE							= BIT(1),		// if set, reset the object even if it is onscreen
	CLEAROBJ_FLAG_INCLUDE_DOORS					= BIT(2),		// clear area of objects should also process doors
	CLEAROBJ_FLAG_INCLUDE_OBJWITHBRAINS			= BIT(3),		// clear area of objects should also process objects with brains
	CLEAROBJ_FLAG_EXCLUDE_LADDERS				= BIT(4)		// clear area of objects should not process ladders
};

/////////////////////////////////////////////////////////////////////////////////
// A class to contain the functions.
/////////////////////////////////////////////////////////////////////////////////


class CObjectPopulation
{
public:
	static	void	Init(unsigned initMode);
	static	void	Shutdown(unsigned shutdownMode);
	static	void	Process();

	static CObject* CreateObject(fwModelId modelId, const eEntityOwnedBy ownedBy, bool bCreateDrawable, bool bInitPhys = true, bool bClone = true, s32 PoolIndex = -1, bool bCalcAmbientScales = false);
	static CObject* CreateObject(CDummyObject* pDummy, const eEntityOwnedBy ownedBy);
	
	struct CreateObjectInput
	{
		CreateObjectInput(fwModelId iModelId, eEntityOwnedBy nEntityOwnedBy, bool bCreateDrawable)
		: m_iModelId(iModelId)
		, m_nEntityOwnedBy(nEntityOwnedBy)
		, m_bCreateDrawable(bCreateDrawable)
		, m_bInitPhys(true)
		, m_bClone(true)
		, m_bForceClone(false)
		, m_bForceObjectType(false)
		, m_iPoolIndex(-1)
		, m_bCalcAmbientScales(true)
		{}
		
		fwModelId		m_iModelId;
		eEntityOwnedBy	m_nEntityOwnedBy;
		bool			m_bCreateDrawable;
		bool			m_bInitPhys;
		bool			m_bClone;
		bool			m_bForceClone;
		bool			m_bForceObjectType;
		s32				m_iPoolIndex;
		bool			m_bCalcAmbientScales;
	};
	static CObject* CreateObject(const CreateObjectInput& rInput);
	
	static void		DestroyObject(CObject* pObject, bool convertedToDummy = false);

	static void		TryToFreeUpTempObjects(s32 num);
	static void		DeleteAllTempObjects();
	static void		DeleteAllTempObjectsInArea(const Vector3& Centre, float Radius);
	static void		DeleteAllMissionObjects();

	static void		ManageObjectPopulation(bool bAllObjects, bool cleanFrags = false);
	static void		ManageObject(CObject* pObj, const Vector3& vecCentre, bool bScreenFadedOut, bool cleanFrags, bool bIsFirstPersonAiming);
	static void		ManageDummy(CDummyObject* pDummy, const Vector3& vecCentre, bool bIsFirstPersonAiming);
	static CObject*	ConvertToRealObject(CDummyObject* pDummy);
	static CEntity*	ConvertToDummyObject(CObject* pObject, bool bForce = false);
	static bool		TestRoomForDummyObject(CObject* pObject);
	static bool		TestSafeForRealObject(CDummyObject* pDummy);
	static void		ConvertAllObjectsToDummyObjects(bool bConvertObjectsWithBrains);
	static void		ConvertAllObjectsInAreaToDummyObjects(const Vector3& Centre, float Radius, bool bConvertObjectsWithBrains, CObject *pExceptionObject, bool bConvertDoors, bool bForce=false, bool bExcludeLadders =false);
	static void		FlushAllSuppressedDummyObjects();

	static void		SetHighPriorityDummyObjects();
	static void		SetAllowPinnedObjects(bool bAllow) { ms_bAllowPinnedObjects = bAllow; }
	static bool		FoundHighPriorityDummyCB(CEntity* pEntity, void* pData);

	static void		InsertSortedDummyClose(CEntity* ent, float dist);
	static sCloseDummy* GetCloseDummiesArray() { return ms_closeDummiesToProcess; }

	static void		SetScriptForceConvertVolumeThisFrame(const spdSphere& volume)
	{
		ms_bScriptRequiresForceConvert = true;
		ms_volumeForScriptForceConvert = volume;
	}

	static void		RegisterDummyObjectMarkedAsNeverDummy(CDummyObject* pDummy);
	static void		ForceConvertDummyObjectsMarkedAsNeverDummy();
	static void		SubmitDummyObjectsForAmbientScaleCalc();

	static bool		IsBlacklistedForMP(atHashString modelName) { return(ms_blacklistForMP.BinarySearch(modelName.GetHash()) != -1); }


	// global flag to enable IM support only when needed
	static void		EnableCObjectsAlbedoSupport(bool enable)		{ sm_bCObjectsAlbedoSupportEnabled = enable;	}
	static bool		IsCObjectsAlbedoSupportEnabled()				{ return sm_bCObjectsAlbedoSupportEnabled;		}
		
	static void		SetCObjectsAlbedoRenderingActive(bool enable)	{ sm_bCObjectsAlbedoRenderingActive = enable;	}
	static bool		IsCObjectsAlbedoRenderingActive()				{ return(sm_bCObjectsAlbedoSupportEnabled && sm_bCObjectsAlbedoRenderingActive); }

	static void		RenderCObjectsAlbedo();

#if NAVMESH_EXPORT
	static void ManageObjectForNavMeshExport(CObject* pObj);
	static void ManageDummyForNavMeshExport(CDummyObject* pObj);
#endif

#if __BANK
	static void InitWidgets();
	static void UpdateDebugDrawing();
#endif

private:

	static void		PropagateExtensionsToRealObject(CDummyObject* pDummy, CObject* pObject);
	static void		PropagateExtensionsToDummyObject(CObject* pObject, CDummyObject* pDummy);
	static bool		IsObjectRequiredBySniper(CEntity* pEntity, bool bIsFirstPersonAiming);

	static bool		IsObjectRequiredByScript(CEntity* pEntity)
	{
		if (ms_bScriptRequiresForceConvert)
		{
			return ms_volumeForScriptForceConvert.IntersectsSphere( pEntity->GetBoundSphere() );
		}
		return false;
	}

	static void		RenderAllAlbedoObjects();
	static void		SetCObjectsAlbedoRenderState();

	static bool		sm_bCObjectsAlbedoSupportEnabled;
	static bool		sm_bCObjectsAlbedoRenderingActive;


	static void		SuppressDummyUntilOffscreen(CDummyObject* pDummyObject);
	static void		UpdateSuppressedDummyObjects();

	static s32		ManageHighPriorityDummyObjects(const Vector3& vPos, bool bIsFirstPersonAiming);

	static void		ForceConvertForScript();
	static bool		ForceConvertDummyCB(CEntity* pEntity, void* pData);

	static atArray<entitySelectID> ms_dummiesThatNeedAmbientScaleUpdate;
	static atArray<RegdDummyObject> ms_suppressedDummyObjects;
	static atArray<RegdDummyObject> ms_highPriorityDummyObjects;
	static bool ms_bAllowPinnedObjects;

	static sCloseDummy ms_closeDummiesToProcess[NUM_CLOSE_DUMMIES_PER_FRAME];

	static bool ms_bScriptRequiresForceConvert;
	static spdSphere ms_volumeForScriptForceConvert;

	static u16 ms_numNeverDummies;
	static u16 ms_neverDummyList[NUM_NEVERDUMMY_PROCESSED];

	static atVector<u32>	ms_blacklistForMP;

#if __BANK
	static float ms_currentRealConversionsPerFrame;
	static float ms_currentDummyConversionsPerFrame;
	static u32 ms_realConversionsThisFrame;
	static u32 ms_dummyConversionsThisFrame;
	static bool ms_objectPopDebugInfo;
	static bool ms_objectPopDebugDrawingVM;
	static bool ms_drawRealObjectsOnVM;
	static bool ms_drawDummyObjectsOnVM;
	static bool ms_showRealToDummyObjectConversionsOnVM;
	static bool ms_showDummyToRealObjectConversionsOnVM;

#define EXPENSIVE_OBJECT_CONVERSIONS_TO_TRACK (16)
#define EXPENSIVE_OBJECT_CONVERSION_NAME_LENGTH (64)
	static bool ms_showMostExpensiveRealObjectConversions;
	
	// Struct to keep track of an object's name and how long it took to convert
	struct ObjectConversionStruct
	{
		ObjectConversionStruct():m_conversionTime(0){sysMemSet(m_objectName,0,EXPENSIVE_OBJECT_CONVERSION_NAME_LENGTH);}
		~ObjectConversionStruct(){};

		ObjectConversionStruct& operator= (const ObjectConversionStruct& rhs)
		{
			formatf(m_objectName, EXPENSIVE_OBJECT_CONVERSION_NAME_LENGTH, "%s", rhs.m_objectName);
			m_conversionTime = rhs.m_conversionTime;
			return *this;
		}

		char m_objectName[EXPENSIVE_OBJECT_CONVERSION_NAME_LENGTH];
		float m_conversionTime;
	};

	
	static ObjectConversionStruct* ms_pExpensiveObjectConversions;	

	static void TrackObjectConversion(CObject* pObject, float msTime);
	static void ClearTrackedObjectConversions();
	static void ConvertObjectsAroundPlayerToDummy();
#endif // __BANK
};



#endif
