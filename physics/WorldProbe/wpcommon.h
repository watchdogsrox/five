#ifndef WORLD_PROBE_COMMON_H
#define WORLD_PROBE_COMMON_H

// Game forward declares:
class CEntity;
class CVehicle;
class CPed;

// RAGE forward declares:
namespace rage
{
	class phIntersection;
	class phInst;
	class phBoundBox;
	class Vector3;
	class phBoundBox;
};

// Forward declarations:
namespace WorldProbe
{
	class CShapeTestManager;
	CShapeTestManager* GetShapeTestManager();

	class CShapeTestDesc;

	class CShapeTestHitPoint;
	class CShapeTestResults;

	class CShapeTestProbeDesc;
	class CShapeTestCapsuleDesc;
	class CShapeTestSphereDesc;
	class CShapeTestBoundDesc;
	class CShapeTestBoundingBoxDesc;
	class CShapeTestBatchDesc;

	class CShapeTestDebugRenderData;
}

// RAGE headers:
#include "physics/levelbase.h"
#include "physics/shapetest.h"
#include "physics/cullshape.h"
#include "system/ipc.h"
#include "vector/colors.h"

// Game headers:
#include "physics/gtaArchetype.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics_channel.h"
#include "scene/RegdRefTypes.h"

#define MAX_SHAPE_TEST_INTERSECTIONS	(32)
#define PROCESS_LOS_MAX_EXCEPTIONS		(32)

#define WORLD_PROBE_DEBUG (__BANK && !(__DEV && __XENON))

#if WORLD_PROBE_DEBUG

// Turn this on to track the total number of calls to CShapeTestManager::SubmitTest() from each code location. Provides
// a button in RAG to dump the values at any time.
#define WORLD_PROBE_FREQUENCY_LOGGING 0
#define MAX_SHAPETEST_CALLS_FOR_LOGGING 500

#define MAX_DEBUG_DRAW_INTERSECTIONS_PER_SHAPE	(8)

#endif // WORLD_PROBE_DEBUG

#define MAX_ASYNCHRONOUS_REQUESTED_SHAPE_TESTS	(120)	//The total number that can be "in the system" (either waiting or being processed)

#define MAX_ASYNCHRONOUS_PROCESSING_SHAPE_TESTS (20)	//The number given to sysTaskManager to be processed

//Some defines for dual surface to avoid confusion about the values that describe top and bottom surfaces.
#define TOP_SURFACE (0.0f)
#define BOTTOM_SURFACE (1.0f)

#define MAX_INCLUDE_EXCLUDE_INSTANCES 128

/*
The flags which are input to the WorldProbe functions are defined in "physics/gtaArchetype.h"
*/

#if WORLD_PROBE_DEBUG
// This macro allows recording the context from which a test was submitted for labeling shape tests
// when debug drawing is enabled.
#define SubmitTest(...) DebugSubmitTest(__FILE__, __LINE__, __VA_ARGS__)
#define FindGroundZForCoord(...) DebugFindGroundZForCoord(__FILE__, __LINE__, __VA_ARGS__)
#define FindGroundZFor3DCoord(...) DebugFindGroundZFor3DCoord(__FILE__, __LINE__, __VA_ARGS__)
#define FindGroundZFor3DCoord(...) DebugFindGroundZFor3DCoord(__FILE__, __LINE__, __VA_ARGS__)

#endif // WORLD_PROBE_DEBUG

//Record all of the handles that come from and are freed by the sysTaskManager for debugging
#define RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION (0)

namespace WorldProbe
{
	enum eLineOfSightOptions
	{
		//Exclude material options
		LOS_IGNORE_SHOOT_THRU		= BIT(0),		// ignore materials with shoot thru flag set (if not ignoring shoot through VFX)
		LOS_IGNORE_SEE_THRU			= BIT(1),		// ignore materials with see thru flag set (if not ignoring shoot through VFX)
		LOS_IGNORE_NO_COLLISION		= BIT(2),		// ignore entities with collision turned off (if not ignoring shoot through VFX)
		LOS_IGNORE_NO_CAM_COLLISION	= BIT(3),		// ignore materials with camera collision turned off (inclusive of when camera clipping is allowed)
		LOS_IGNORE_POLY_SHOOT_THRU	= BIT(4),
		LOS_IGNORE_NOT_COVER		= BIT(5),
		LOS_GO_THRU_GLASS			= BIT(6),

		//Include material options
		LOS_IGNORE_SHOOT_THRU_FX	= BIT(7),	

		//Include entity options
		LOS_TEST_VEH_TYRES			= BIT(8),

		LOS_NO_RESULT_SORTING		= BIT(9)
	};
	enum ELosContext
	{
		LOS_GeneralAI,
		LOS_CombatAI,
		LOS_Camera,
		LOS_Audio,
		LOS_Unspecified,
		LOS_SeekCoverAI,
		LOS_SeparateAI,
		LOS_FindCoverAI,
		LOS_Weapon,

		EPedTargetting,
		EPedAcquaintances,
		ELosCombatAI,
		EPlayerCover,
		EAudioOcclusion,
		EAudioLocalEnvironment,
		EMovementAI,
		EScript,
		EHud,
		EMelee,
        ENetwork,
		ENotSpecified,
		ERiverBoundQuery,
		EVehicle,
		LOS_NumContexts
	};
	enum eEntityIncludeExcludeOptions // (EIEO)
	{
		EIEO_DONT_ADD_VEHICLE = (1<<0),
		EIEO_DONT_ADD_VEHICLE_OCCUPANTS = (1<<1),
		EIEO_DONT_ADD_WEAPONS = (1<<2),
		EIEO_DONT_ADD_PED_ATTACHMENTS = (1<<3)
	};
	enum eBlockingMode
	{
		PERFORM_SYNCHRONOUS_TEST,
		PERFORM_ASYNCHRONOUS_TEST
	};
	enum eTestDomain
	{
		TEST_IN_LEVEL = 0,
		TEST_AGAINST_INDIVIDUAL_OBJECTS
	};
	enum eShapeTestType
	{
		INVALID_TEST_TYPE = -1,
		DIRECTED_LINE_TEST = 0,
		UNDIRECTED_LINE_TEST,
		CAPSULE_TEST,
		SWEPT_SPHERE_TEST,
		POINT_TEST,
		SPHERE_TEST,
		BOUND_TEST,
		BOUNDING_BOX_TEST,
		BATCHED_TESTS
	};
	enum eResultsState
	{
		// This CShapeTestResults is conceptually a NULL handle.
		TEST_NOT_SUBMITTED = 0, // Initial state (and state after reset).
		TEST_ABORTED,
		SUBMISSION_FAILED,

		// This CShapeTestResults is interesting:
		WAITING_ON_RESULTS,
		RESULTS_READY
	};

#if !WORLD_PROBE_DEBUG
#define StartProbeTimer(n)
#define StopProbeTimer(n)
#else // !WORLD_PROBE_DEBUG
	void StartProbeTimer(const WorldProbe::ELosContext eContext);
	void StopProbeTimer(const WorldProbe::ELosContext eContext);
#endif // !WORLD_PROBE_DEBUG

	phMaterialMgrGta::Id ConvertLosOptionsToIgnoreMaterialFlags(int losOptions);

#if WORLD_PROBE_DEBUG
	float DebugFindGroundZForCoord(const char *strCodeFile, u32 nCodeLine, const float fSecondSurfaceInterp, const float x, const float y, const ELosContext eContext=LOS_Unspecified, WorldProbe::CShapeTestResults* pShapeTestResults=0);
	float DebugFindGroundZFor3DCoord(const char *strCodeFile, u32 nCodeLine, const float fSecondSurfaceInterp, const float x, const float y, const float z, bool *pBool = NULL, Vector3* pNormal = NULL, phInst **ppInst = NULL, const ELosContext eContext=LOS_Unspecified);
	float DebugFindGroundZFor3DCoord(const char *strCodeFile, u32 nCodeLine, const float fSecondSurfaceInterp, const Vector3 &vPosition, bool *pBool = NULL, Vector3* pNormal = NULL, phInst **PpInst = NULL, const ELosContext eContext=LOS_Unspecified);
#else
	float FindGroundZForCoord(const float fSecondSurfaceInterp, const float x, const float y, const ELosContext eContext=LOS_Unspecified, WorldProbe::CShapeTestResults* pShapeTestResults=0);
	float FindGroundZFor3DCoord(const float fSecondSurfaceInterp, const float x, const float y, const float z, bool *pBool = NULL, Vector3* pNormal = NULL, phInst **ppInst = NULL, const ELosContext eContext=LOS_Unspecified);
	float FindGroundZFor3DCoord(const float fSecondSurfaceInterp, const Vector3 &vPosition, bool *pBool = NULL, Vector3* pNormal = NULL, phInst **PpInst = NULL, const ELosContext eContext=LOS_Unspecified);
#endif

	// Use fixed size arrays for include and exclude instances. Include and exclude lists don't have to be the same
	//  size but it simplifies the code a bit.
	typedef atFixedArray<const phInst*, MAX_INCLUDE_EXCLUDE_INSTANCES> InExInstanceArray;

#if __BANK
	const char* GetShapeTypeString(eShapeTestType eTestType);
	const char* GetContextString(ELosContext eContext);
#endif // __BANK
} // namespace WorldProbe

#endif // WORLD_PROBE_COMMON_H
