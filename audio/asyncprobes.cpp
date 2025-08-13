#include "asyncprobes.h"
#include "fwsys/timer.h"

AUDIO_OPTIMISATIONS()

//-------------------------------------------------------------------------------------------------------------------
bool audCachedLos::Clear()
{
	m_vStart.Zero();
	m_vEnd.Zero();
	m_vIntersectPos.Zero();
	m_vIntersectNormal.Zero();

	m_hShapeTest.Reset();
	m_pExcludeEntity = NULL;
	m_iIncludeFlags = 0;
	m_iLifeTime = AUD_LOS_FAILSAFE_LIFETIME;
	m_MaterialId = phMaterialMgr::DEFAULT_MATERIAL_ID;
	m_Component = 0;
	m_Instance = NULL;
	m_bContainsProbe = false;
	m_bHasProbeResults = false;
	m_bHasBeenProcessed = false;
	m_bHitSomething = false;
	return !m_hShapeTest.GetResultsWaitingOrReady();
}

//-------------------------------------------------------------------------------------------------------------------
void audCachedLos::AddProbe(const Vector3 & vStart, const Vector3 & vEnd, CEntity * pExcludeEntity, const u32 iIncludeFlags, bool bForceImmediate)
{
	// Dont bother submitting any new test if we already got results.
	if(!m_hShapeTest.GetResultsWaitingOrReady())
	{

		m_vStart = vStart;
		m_vEnd = vEnd;
		m_pExcludeEntity = pExcludeEntity;
		m_iIncludeFlags = iIncludeFlags;

		m_bContainsProbe = true;
		m_bHasBeenProcessed = false;
		m_bHasProbeResults = false;

		if(!bForceImmediate)
		{
			m_hShapeTest.Reset();

			// Attempt to issue this probe asynchronously
			WorldProbe::CShapeTestProbeDesc probeData;
			probeData.SetResultsStructure(&m_hShapeTest);
			probeData.SetStartAndEnd(vStart, vEnd);
			probeData.SetExcludeEntity(pExcludeEntity);
			probeData.SetIncludeFlags(iIncludeFlags);
			probeData.SetContext(WorldProbe::EAudioLocalEnvironment);
			probeData.SetIsDirected(true);
			WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
		}

		// If it couldn't be issued async, then process it immediately
		if(bForceImmediate || m_hShapeTest.GetResultsStatus() == WorldProbe::SUBMISSION_FAILED)
		{
			m_bHasProbeResults = true;

			WorldProbe::CShapeTestHitPoint result; 
			WorldProbe::CShapeTestResults probeResults(result);
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetStartAndEnd(vStart, vEnd);
			probeDesc.SetExcludeEntity(pExcludeEntity);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeDesc.SetContext(WorldProbe::EAudioLocalEnvironment);
			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				m_vIntersectPos = probeResults[0].GetHitPosition();
				m_vIntersectNormal = probeResults[0].GetHitNormal();
				m_Instance = probeResults[0].GetHitInst();
				m_Component = probeResults[0].GetHitComponent();
				m_MaterialId= probeResults[0].GetHitMaterialId();
				m_bHitSomething = true;
			}
			else
			{
				m_bHitSomething = false;
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------------------------
void audCachedLos::GetProbeResult()
{
	if(!m_bHasProbeResults && m_hShapeTest.GetResultsReady())
	{
		if( m_hShapeTest.GetNumHits() > 0u )
		{
			m_vIntersectPos = m_hShapeTest[0].GetHitPosition();
			m_vIntersectNormal = m_hShapeTest[0].GetHitNormal();
			m_Instance = m_hShapeTest[0].GetHitInst();
			m_Component = m_hShapeTest[0].GetHitComponent();
			m_MaterialId= m_hShapeTest[0].GetHitMaterialId();
			m_bHitSomething = true;
		}
		else
		{
			m_bHitSomething = false;
		}

		m_bHasProbeResults = true;
		m_hShapeTest.Reset();
	}
	else if(m_hShapeTest.GetResultsStatus() != WorldProbe::TEST_NOT_SUBMITTED)
	{
		m_iLifeTime -= fwTimer::GetTimeStepInMilliseconds();
		if(m_iLifeTime <= 0)
		{
			Clear();
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
audCachedLosOneResult::audCachedLosOneResult()
{
	Clear();
	m_GroundPosition.Zero();
	m_GroundNormal.Zero();
}
//-------------------------------------------------------------------------------------------------------------------
bool audCachedLosOneResult::Clear()
{
	m_GroundPosition.Zero();
	m_GroundNormal.Zero();
	return m_MainLineTest.Clear();
}
//-------------------------------------------------------------------------------------------------------------------
void audCachedLosOneResult::GetProbeResults()
{
	m_MainLineTest.GetProbeResult();
}
//-------------------------------------------------------------------------------------------------------------------
bool audCachedLosOneResult::IsComplete()
{
	if(m_MainLineTest.m_hShapeTest.GetResultsReady() && !m_MainLineTest.m_bHasBeenProcessed)
	{
		return false;
	}
	return true;
}
//-------------------------------------------------------------------------------------------------------------------
audCachedMultipleTestResults::audCachedMultipleTestResults()
{
	m_HasFoundTheSolution = false;
	m_ResultPosition = Vector3(0.f,0.f,0.f);
}

audCachedMultipleTestResults::~audCachedMultipleTestResults()
{
}
//-------------------------------------------------------------------------------------------------------------------
audCachedLosMultipleResults::audCachedLosMultipleResults()
{
	ClearAll();
}
//-------------------------------------------------------------------------------------------------------------------
void audCachedLosMultipleResults::ClearAll()
{
	s32 i;
	for(i=0; i<NumMainLOS; i++)
		m_MainLos[i].Clear();
	for(i=0; i<NumMidLOS; i++)
		m_MidLos[i].Clear();
	for(i=0; i<NumNearVertLOS; i++)
		m_NearVerticalLos[i].Clear();
}
//-------------------------------------------------------------------------------------------------------------------
void audCachedLosMultipleResults::GetAllProbeResults()
{
	s32 i;
	for(i=0; i<NumMainLOS; i++)
		m_MainLos[i].GetProbeResult();
	for(i=0; i<NumMidLOS; i++)
		m_MidLos[i].GetProbeResult();
	for(i=0; i<NumNearVertLOS; i++)
		m_NearVerticalLos[i].GetProbeResult();
}
//-------------------------------------------------------------------------------------------------------------------
bool audCachedLosMultipleResults::AreAllComplete()
{
	s32 i;
	for(i=0; i<NumMainLOS; i++)
		if(m_MainLos[i].m_hShapeTest.GetResultsReady() && !m_MainLos[i].m_bHasProbeResults)
			return false;

	for(i=0; i<NumMidLOS; i++)
		if(m_MidLos[i].m_hShapeTest.GetResultsReady() && !m_MidLos[i].m_bHasProbeResults)
			return false;

	for(i=0; i<NumNearVertLOS; i++)
		if(m_NearVerticalLos[i].m_hShapeTest.GetResultsReady() && !m_NearVerticalLos[i].m_bHasProbeResults)
			return false;

	return true;
}
//-------------------------------------------------------------------------------------------------------------------
audOcclusionIntersections::audOcclusionIntersections()
{
	ClearAll();
}
//-------------------------------------------------------------------------------------------------------------------
void audOcclusionIntersections::ClearAll()
{
	for(int i = 0; i < AUD_MAX_TARGETTING_INTERSECTIONS; i++)
	{
		m_Position[i] = Vector3(0.0f, 0.0f, 0.0f);
		m_MaterialFactor[i] = 0.0f;
#if __BANK
		m_MaterialID[i] = 0;
#endif
	}
	numIntersections = 0;
}






