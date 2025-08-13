#include "physics/WorldProbe/debug.h"

#if WORLD_PROBE_DEBUG

#include "network/live/livemanager.h"
#include "physics/WorldProbe/shapetesttask.h"

#include "fwdebug/debugdraw.h"
#include "fwsys/timer.h"

namespace WorldProbe
{
	void DebugDrawIntersection(const phIntersection& intersection)
	{
		Assert(intersection.IsAHit());

		u16 levelIndex = intersection.GetLevelIndex();
		u16 generationId = intersection.GetGenerationID();
		Vec3V position = intersection.GetPosition();
		if(CShapeTestManager::ms_bDebugDrawHitPoint)
		{
			grcDebugDraw::Sphere(position, CShapeTestManager::ms_fHitPointRadius, Color_orange, true);
		}
		if(CShapeTestManager::ms_bDebugDrawHitNormal)
		{
			grcDebugDraw::Arrow(position, AddScaled(position,intersection.GetNormal(),ScalarV(CShapeTestManager::ms_fHitPointNormalLength)),CShapeTestManager::ms_fHitPointRadius, Color_orange);
		}
		if(CShapeTestManager::ms_bDebugDrawHitPolyNormal)
		{
			grcDebugDraw::Arrow(position, AddScaled(position,intersection.GetIntersectedPolyNormal(),ScalarV(CShapeTestManager::ms_fHitPointNormalLength)),CShapeTestManager::ms_fHitPointRadius, Color_orange);
		}

		char textBuffer[1024];
		char* currentEnd = textBuffer;
		*currentEnd = 0;
		if(CShapeTestManager::ms_bDebugDrawHitInstanceName)
		{
			phInst* instance = PHLEVEL->IsLevelIndexGenerationIDCurrent(levelIndex,generationId) ? PHLEVEL->GetInstance(levelIndex) : NULL;
			currentEnd += sprintf(currentEnd, "%sInstance Name: %s", ((currentEnd==textBuffer) ? "" : ", "),  (instance ? instance->GetArchetype()->GetFilename() : "???"));
		}
		if(CShapeTestManager::ms_bDebugDrawHitHandle)
		{
			currentEnd += sprintf(currentEnd, "%sHandle: (%i,%i)", ((currentEnd==textBuffer) ? "" : ", "), intersection.GetLevelIndex(), intersection.GetGenerationID());
		}
		if(CShapeTestManager::ms_bDebugDrawHitComponentIndex)
		{
			currentEnd += sprintf(currentEnd, "%sComponent: %i", ((currentEnd==textBuffer) ? "" : ", "), intersection.GetComponent());
		}
		if(CShapeTestManager::ms_bDebugDrawHitPartIndex)
		{
			currentEnd += sprintf(currentEnd, "%sPart: %i", ((currentEnd==textBuffer) ? "" : ", "), intersection.GetPartIndex());
		}
		if(CShapeTestManager::ms_bDebugDrawHitMaterialName)
		{
			char materialNameBuffer[128];
			PGTAMATERIALMGR->GetMaterialName(PGTAMATERIALMGR->UnpackMtlId(intersection.GetMaterialId()), materialNameBuffer, 128);
			currentEnd += sprintf(currentEnd, "%sMaterial: %s", ((currentEnd==textBuffer) ? "" : ", "), materialNameBuffer);
		}
		if(CShapeTestManager::ms_bDebugDrawHitMaterialName)
		{
			currentEnd += sprintf(currentEnd, "%sMaterial Id: 0x%" I64FMT "X", ((currentEnd==textBuffer) ? "" : ", "), intersection.GetMaterialId());
		}
		grcDebugDraw::Text(position,Color_white,0,0,textBuffer);
	}
}

void WorldProbe::CShapeTestDebugRenderData::DebugDraw() const
{
	if(!WorldProbe::GetShapeTestManager()->ms_abContextFilters[m_eLOSContext])
		return;

	if(	   (m_bAsyncTest && !WorldProbe::CShapeTestManager::ms_bDebugDrawAsyncTestsEnabled) 
		|| (!m_bAsyncTest && !WorldProbe::CShapeTestManager::ms_bDebugDrawSynchronousTestsEnabled) )
		return;

	ASSERT_ONLY(
		int nDbgStringLength = istrlen(m_strCodeFile) + 1
									+ int(log((float)m_nCodeLine));
	)
	
	char debugText[1024]; physicsAssertf(nDbgStringLength < 1024, "Shape test function name buffer is not large enough for debug string.");
	formatf(debugText, "%s:%d [%i]", m_strCodeFile, m_nCodeLine, (int)m_CompletionTime);

	switch(m_ShapeTestType)
	{
		case DIRECTED_LINE_TEST:
		case UNDIRECTED_LINE_TEST:
			DrawLine(debugText);
			break;

		case CAPSULE_TEST:
		case SWEPT_SPHERE_TEST:
			DrawCapsule(debugText);
			break;

		case POINT_TEST:
			physicsAssertf(m_ShapeTestType != POINT_TEST, "Don't support point shape tests");
			break;

		case SPHERE_TEST:
			DrawSphere(debugText);
			break;

		case BOUND_TEST:
			DrawBound(debugText);
			break;

		case BOUNDING_BOX_TEST:
			DrawBoundingBox(debugText);
			break;

		case BATCHED_TESTS:
			physicsAssertf(m_ShapeTestType != BATCHED_TESTS, "Contents of batched test are rendered individually");
			break;

		case INVALID_TEST_TYPE:
		default:
			physicsAssertf(false, "Unknown shape test descriptor type: %d.", m_ShapeTestType);
			break;
	}
	
	for(int j=0; j < CShapeTestManager::ms_bMaxIntersectionsToDraw && m_Intersections[j].IsAHit(); ++j)
	{
		if(CShapeTestManager::ms_bIntersectionToDrawIndex == -1 || CShapeTestManager::ms_bIntersectionToDrawIndex == j)
		{
			DebugDrawIntersection(m_Intersections[j]);
		}
	}
}

void WorldProbe::CShapeTestDebugRenderData::DrawLine(const char* debugText) const
{
	if(m_bDirected)
		grcDebugDraw::Arrow(m_StartPos, m_EndPos, WorldProbe::CShapeTestManager::ms_fHitPointRadius, m_Colour);
	else
		grcDebugDraw::Line(m_StartPos, m_EndPos, m_Colour, m_Colour);

	if(debugText)
		CShapeTestManager::DebugRenderTextLineWorldSpace(debugText, m_StartPos, Color_black, 1, 1);
}

void WorldProbe::CShapeTestDebugRenderData::DrawCapsule(const char* debugText) const
{
	grcDebugDraw::Capsule(m_StartPos, m_EndPos, m_Dims.GetXf(), m_Colour, false);
	
	if(m_bDirected)
		grcDebugDraw::Arrow(m_StartPos, m_EndPos, WorldProbe::CShapeTestManager::ms_fHitPointRadius, m_Colour);
	
	if(debugText)
		CShapeTestManager::DebugRenderTextLineWorldSpace(debugText, m_StartPos, Color_black, 0, 1);
}

void WorldProbe::CShapeTestDebugRenderData::DrawSphere(const char* debugText) const
{
	grcDebugDraw::Sphere(m_StartPos, m_Dims.GetXf(), m_Colour, false);

	if(debugText)
		CShapeTestManager::DebugRenderTextLineWorldSpace(debugText, m_StartPos, Color_black, 0, 1);
}

void WorldProbe::CShapeTestDebugRenderData::DrawBound(const char* debugText) const
{
	if( m_bBoundExists )
	{
		grcDebugDraw::BoxOriented(m_StartPos, m_EndPos, m_Transform, m_Colour, m_bHit);
	}

	if(debugText)
		CShapeTestManager::DebugRenderTextLineWorldSpace(debugText, m_Transform.d(), Color_black, 0, 1);
}

void WorldProbe::CShapeTestDebugRenderData::DrawBoundingBox(const char* debugText) const
{
	if( m_bBoundExists )
	{
		rage::Color32 colour = m_Colour;

		if( m_bHit )
		{
			colour.SetAlpha(128); 
		}

		grcDebugDraw::BoxOriented(m_StartPos, m_EndPos, m_Transform, colour, m_bHit);
	}

	if(debugText)
		CShapeTestManager::DebugRenderTextLineWorldSpace(debugText, m_Transform.d(), Color_black, 0, 1);
}

////////////////////////////////////////////////
// Render shape test related debug information.
////////////////////////////////////////////////
void WorldProbe::CShapeTestManager::ResetPausedRenderBuffer()
{
	SYS_CS_SYNC(ms_ShapeDebugPausedRenderBufferCs);

	ms_ShapeDebugPausedRenderHighWaterMark = Max(ms_ShapeDebugPausedRenderHighWaterMark, ms_ShapeDebugPausedRenderCount);	
	ms_ShapeDebugPausedRenderCount = 0;
}

bool WorldProbe::CShapeTestManager::AddToDebugPausedRenderBuffer(const CShapeTestTaskData& shapeTestTaskData, WorldProbe::CShapeTestTaskResultsDescriptor* pResultsDescriptors, int numResultsDescriptors)
{
	if( fwTimer::IsGamePaused() )
	{
		return false;
	}

	bool bAdded = false;

	SYS_CS_SYNC(ms_ShapeDebugPausedRenderBufferCs);

	u32 uShapesToRender = (u32)numResultsDescriptors;
	//if( physicsVerifyf(ms_ShapeDebugPausedRenderCount + uShapesToRender <= MAX_DEBUG_DRAW_BUFFER_SHAPES, "No room in shape test paused render buffer" ) )
	if(ms_ShapeDebugPausedRenderCount + uShapesToRender <= MAX_DEBUG_DRAW_BUFFER_SHAPES)
	{
		CShapeTestDebugRenderData& renderData = ms_ShapeDebugPausedRenderBuffer[ms_ShapeDebugPausedRenderCount];
		ms_ShapeDebugPausedRenderCount += uShapesToRender;
		shapeTestTaskData.ConstructPausedRenderData(&renderData,pResultsDescriptors,numResultsDescriptors);
		bAdded = true;
	}

	return bAdded;
}

void WorldProbe::CShapeTestManager::RenderPausedRenderBuffer()
{
	SYS_CS_SYNC(ms_ShapeDebugPausedRenderBufferCs);

	for(u32 i = 0 ; i < ms_ShapeDebugPausedRenderCount ; i++)
	{
		ms_ShapeDebugPausedRenderBuffer[i].DebugDraw();
	}
}

void WorldProbe::CShapeTestManager::DebugRenderTextLineWorldSpace(const char* text, const Vec3V& worldPos, const Color32 textCol, const int lineNum, int persistFrames)
{
	grcDebugDraw::Text(worldPos, textCol, 0,(grcDebugDraw::GetScreenSpaceTextHeight()-1) * lineNum, text, true, persistFrames);
}

void WorldProbe::CShapeTestTaskResultsDescriptor::DebugDrawResults(ELosContext eLOSContext)
{
	IF_COMMERCE_STORE_RETURN();

	if(m_pResults == NULL || m_pResults->m_pResults == NULL 
		|| !WorldProbe::GetShapeTestManager()->ms_abContextFilters[eLOSContext])
		return;

	phIntersection* pInOutIntersections = &m_pResults->m_pResults[m_FirstResultIntersectionIndex];

	for(int j=0; j < CShapeTestManager::ms_bMaxIntersectionsToDraw && pInOutIntersections[j].IsAHit(); ++j)
	{
		if(CShapeTestManager::ms_bIntersectionToDrawIndex == -1 || CShapeTestManager::ms_bIntersectionToDrawIndex == j)
		{
			DebugDrawIntersection(pInOutIntersections[j]);
		}
	}
}

void WorldProbe::CShapeTestManager::DebugRenderTextLineNormalisedScreenSpace(const char* text, Vec2V_In screenPos, const Color32 textCol, const int lineNum, int persistFrames)
{
	Vec2V lineScreenPos = screenPos + Vec2V(0.0f, 0.0125f * lineNum);
	grcDebugDraw::Text(lineScreenPos, textCol, text, true, 1.0f, 1.0f, persistFrames);
}

void WorldProbe::CShapeTestManager::RenderDebug()
{
	IF_COMMERCE_STORE_RETURN();

	//Render all of the asynchronous tests in the system
	//Everything else gets rendered when the status changes

	{ // scope for critical section
		SYS_CS_SYNC(ms_RequestedAsyncShapeTestsCs);

		if(WorldProbe::CShapeTestManager::ms_bDebugDrawAsyncQueueSummary)
		{
			const Vec2V summaryScreenPos(0.03f, 0.05f);

			DebugRenderTextLineNormalisedScreenSpace("[Shape test async queue]", summaryScreenPos, Color_black, 0);

			for(int i = 0 ; i < ms_RequestedAsyncShapeTests.GetCount() ; i++)
			{
				CShapeTestTaskData* pTaskData = ms_RequestedAsyncShapeTests[i];
				const char* typeString = GetShapeTypeString(pTaskData->m_ShapeTest.GetShapeTestType());
				const char* contextString = GetContextString(pTaskData->m_DebugData.m_eLOSContext);
				char debugText[1024];
				snprintf(debugText, 1024, "%d. %s. %s. %s:%u", i, typeString, contextString, pTaskData->m_DebugData.m_strCodeFile, pTaskData->m_DebugData.m_nCodeLine);
				DebugRenderTextLineNormalisedScreenSpace(debugText, summaryScreenPos, pTaskData->m_TaskHandleCopy == 0 ? Color_grey : Color_black, i + 1);
			}
		}

		if(WorldProbe::CShapeTestManager::ms_bDebugDrawAsyncTestsEnabled || WorldProbe::CShapeTestManager::ms_bDebugDrawSynchronousTestsEnabled)
		{
			for(int i = 0 ; i < ms_RequestedAsyncShapeTests.GetCount() ; i++)
			{
				CShapeTestTaskData* pTaskData = ms_RequestedAsyncShapeTests[i];
				pTaskData->DebugDrawShape(WorldProbe::WAITING_ON_RESULTS);
			}
		}
	}


	if( fwTimer::IsGamePaused() )
	{
		RenderPausedRenderBuffer();
	}
}
#endif // WORLD_PROBE_DEBUG
