#ifndef WORLDPROBE_DEBUG_H
#define WORLDPROBE_DEBUG_H

#include "physics/WorldProbe/wpcommon.h"

#if WORLD_PROBE_DEBUG

// RAGE headers:

// Game headers:
#include "physics/WorldProbe/shapetestresults.h"

// Forward declarations:
namespace WorldProbe
{
	class CShapeTestResults;
}

//Spawning lots of cars on top of each other causes this to be quickly exceeded once there are explosions.
#define MAX_DEBUG_DRAW_BUFFER_SHAPES (1000)

namespace WorldProbe
{

	class CShapeTestDebugRenderData
	{
	public:

		eShapeTestType			m_ShapeTestType;

		Color32					m_Colour;
		const char *			m_strCodeFile;
		u32						m_nCodeLine;
		ELosContext				m_eLOSContext;

		//Data shared by all of the shape types
		Mat34V					m_Transform;
		Vec3V					m_Dims; //.x is radius of capsule or sphere
		Vec3V					m_StartPos; //For bounds and bounding boxes this is the bounding box min
		Vec3V					m_EndPos;

		bool					m_bPartOfBatch;
		bool					m_bDirected;
		bool					m_bHit; //For bounding box test
		bool					m_bBoundExists;
		bool					m_bAsyncTest;

		float					m_CompletionTime;

		phIntersection			m_Intersections[MAX_DEBUG_DRAW_INTERSECTIONS_PER_SHAPE];

		void DebugDraw() const;

	private:
		void DrawLine(const char* debugText) const;
		void DrawCapsule(const char* debugText) const;
		void DrawSphere(const char* debugText) const;
		void DrawBound(const char* debugText) const;
		void DrawBoundingBox(const char* debugText) const;
	};

	class CShapeTestTaskDebugData
	{
	public:
		CShapeTestTaskDebugData(const char *strCodeFile, u32 nCodeLine, ELosContext eLOSContext) 
			: m_strCodeFile(strCodeFile), m_nCodeLine(nCodeLine), m_eLOSContext(eLOSContext) {}

		const char* m_strCodeFile;
		u32 m_nCodeLine;
		ELosContext m_eLOSContext;
	};

#if ENABLE_ASYNC_STRESS_TEST
	void DebugAsyncStressTestOnce();
#endif // ENABLE_ASYNC_STRESS_TEST

} // namespace WorldProbe

#endif // WORLD_PROBE_DEBUG

#endif // WORLDPROBE_DEBUG_H
