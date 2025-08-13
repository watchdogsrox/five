#ifndef TASK_SYSTEM_ASYNCPROBEHELPER_H
#define TASK_SYSTEM_ASYNCPROBEHELPER_H

#include "physics/WorldProbe/shapetestresults.h"
#include "physics/WorldProbe/wpcommon.h"

namespace WorldProbe
{
	class CShapeTestProbeDesc;
}

namespace Rage
{
	class Vector3;
}


//-------------------------------------------------------------------------
// Helper used to asynchronously carry out LOS tests
//-------------------------------------------------------------------------
typedef enum
{
	PS_WaitingForResult,
	PS_Clear,
	PS_Blocked
} ProbeStatus;
class CAsyncProbeHelper
{
public:
	CAsyncProbeHelper();
	~CAsyncProbeHelper();

	// Starts an asynchronous probe test
	bool			StartTestLineOfSight	( WorldProbe::CShapeTestProbeDesc& probeData);
	// Returns the result of the probe test, false means search not yet complete.
	bool			GetProbeResults		( ProbeStatus& probeStatus );
	// The same as above but also provides intersection information
	bool			GetProbeResultsWithIntersection( ProbeStatus& probeStatus, rage::Vector3* pIntersectionPos);
	// The same as above but also provides the intersection normal
	bool			GetProbeResultsWithIntersectionAndNormal( ProbeStatus& probeStatus, rage::Vector3* pIntersectionPos, rage::Vector3* pIntersectionNormal);
	// The same as above but with everything that vfx requires
	bool			GetProbeResultsVfx( ProbeStatus& probeStatus, Vector3* pIntersectionPos, Vector3* pIntersectionNormal, u16* pIntersectionPhysInstLevelIndex, u16* pIntersectionPhysInstGenerationId, u16* pIntersectionComponent, phMaterialMgr::Id* pIntersectionMtlId, float* pIntersectionTValue);
	// Resets any probe test currently active
	void			ResetProbe			( void );
	// Returns true if a probe test is currently underway
	bool			IsProbeActive		( void ) const { return m_bWaitingForResult; }
	// Return a const reference to the whole shape test result structure.
	// Keep this const as it is also the handle for the async test!!
	const WorldProbe::CShapeTestResults& GetProbeResultsStruct() { return m_hHandle; }
private:
	bool				m_bWaitingForResult;
	WorldProbe::CShapeTestSingleResult	m_hHandle;
};

#endif // TASK_SYSTEM_ASYNCPROBEHELPER_H
