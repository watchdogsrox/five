#ifndef SHAPETEST_PROBE_DESC_H
#define SHAPETEST_PROBE_DESC_H

// RAGE headers:

// Game headers:
#include "physics/WorldProbe/wpcommon.h"
#include "physics/WorldProbe/shapetestdesc.h"

namespace WorldProbe
{

	////////////////////////////////////
	// Descriptor class for line tests.
	////////////////////////////////////
	class CShapeTestProbeDesc : public CShapeTestDesc
	{
	public:

		CShapeTestProbeDesc()
			: CShapeTestDesc()
		{
			m_eShapeTestType = DIRECTED_LINE_TEST;
		}

		void SetStartAndEnd(const Vector3& vStart, const Vector3& vEnd)
		{
			physicsAssert(rage::FPIsFinite(vStart.x) && rage::FPIsFinite(vStart.y) && rage::FPIsFinite(vStart.z));
			physicsAssert(rage::FPIsFinite(vEnd.x) && rage::FPIsFinite(vEnd.y) && rage::FPIsFinite(vEnd.z));

			m_segProbe.Set(vStart, vEnd);
		}
		const Vector3& GetStart() const
		{
			return m_segProbe.A;
		}
		const Vector3& GetEnd() const
		{
			return m_segProbe.B;
		}
		void SetIsDirected(const bool bIsDirected)
		{
			m_eShapeTestType = bIsDirected ? DIRECTED_LINE_TEST : UNDIRECTED_LINE_TEST;
		}
		bool GetIsDirected() const
		{
			return m_eShapeTestType == DIRECTED_LINE_TEST;
		}

	private:
		phSegment m_segProbe;

		friend class CShapeTest;
	};

} // namespace WorldProbe


#endif // SHAPETEST_PROBE_DESC_H
