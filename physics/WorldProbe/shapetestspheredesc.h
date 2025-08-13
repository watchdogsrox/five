#ifndef SHAPETEST_SPHERE_DESC_H
#define SHAPETEST_SPHERE_DESC_H

// RAGE headers:

// Game headers:
//#include "physics/WorldProbe/wpcommon.h"
#include "physics/WorldProbe/shapetestdesc.h"

namespace WorldProbe
{

	//////////////////////////////////////
	// Descriptor class for sphere tests.
	//////////////////////////////////////
	class CShapeTestSphereDesc : public CShapeTestDesc
	{
	public:

		CShapeTestSphereDesc()
			: CShapeTestDesc(),
			m_vCentre(Vector3(VEC3_ZERO)),
			m_fRadius(0.0f),
			m_bTestBackFacingPolygons(false)
		{
			m_eShapeTestType = SPHERE_TEST;
		}

		void SetSphere(const Vector3& vCentre, const float fRadius)
		{
			physicsAssert(rage::FPIsFinite(vCentre.x) && rage::FPIsFinite(vCentre.y) && rage::FPIsFinite(vCentre.z));
			physicsAssert(rage::FPIsFinite(fRadius));

			m_vCentre = vCentre;
			m_fRadius = fRadius;
		}

		// By default sphere will not detect intersections when their center is behind a polygon
		void SetTestBackFacingPolygons(bool testBackFacingPolygons) { m_bTestBackFacingPolygons = testBackFacingPolygons; }
		bool GetTestBackFacingPolygons() const { return m_bTestBackFacingPolygons; }

	private:
		Vector3 m_vCentre;
		float m_fRadius;
		bool m_bTestBackFacingPolygons;

		friend class CShapeTest;
	};

} // namespace WorldProbe


#endif // SHAPETEST_SPHERE_DESC_H
