#ifndef SHAPETEST_CAPSULE_DESC_H
#define SHAPETEST_CAPSULE_DESC_H

// RAGE headers:

// Game headers:
#include "physics/WorldProbe/wpcommon.h"
#include "physics/WorldProbe/shapetestdesc.h"

namespace WorldProbe
{

	///////////////////////////////////////
	// Descriptor class for capsule tests.
	///////////////////////////////////////
	class CShapeTestCapsuleDesc : public CShapeTestDesc
	{
	public:

		CShapeTestCapsuleDesc()
			: CShapeTestDesc(),
			m_segProbe(Vector3(VEC3_ZERO), Vector3(VEC3_ZERO)),
			m_fRadius(0.0f),
			m_bDoInitialSphereCheck(false)
		{
			m_eShapeTestType = CAPSULE_TEST;
		}

		void SetCapsule(const Vector3& vStart, const Vector3& vEnd, const float fRadius)
		{
			physicsAssert(rage::FPIsFinite(vStart.x) && rage::FPIsFinite(vStart.y) && rage::FPIsFinite(vStart.z));
			physicsAssert(rage::FPIsFinite(vEnd.x) && rage::FPIsFinite(vEnd.y) && rage::FPIsFinite(vEnd.z));
			physicsAssert(rage::FPIsFinite(fRadius));

			m_segProbe.Set(vStart, vEnd);
			m_fRadius = fRadius;
		}
		void SetCapsule(const Vector3& vStart, const Vector3& vDirUnnormalised, const float ASSERT_ONLY(fLength), const float fRadius)
		{
			physicsAssert(rage::FPIsFinite(vStart.x) && rage::FPIsFinite(vStart.y) && rage::FPIsFinite(vStart.z));
			physicsAssert(rage::FPIsFinite(fRadius));
			physicsAssert(Vector3(vDirUnnormalised).Mag2() >= square(0.999f * fLength));
			physicsAssert(Vector3(vDirUnnormalised).Mag2() <= square(1.001f * fLength));

			m_segProbe.Set(vStart, vStart+vDirUnnormalised);
			m_fRadius = fRadius;
		}
		void SetIsDirected(const bool bIsDirected)
		{
			m_eShapeTestType = bIsDirected ? SWEPT_SPHERE_TEST : CAPSULE_TEST;
		}
		bool GetIsDirected() const
		{
			return m_eShapeTestType == SWEPT_SPHERE_TEST;
		}
		void SetDoInitialSphereCheck(const bool bDoInitialSphereCheck)
		{
			m_bDoInitialSphereCheck = bDoInitialSphereCheck;
		}
		bool GetDoInitialSphereCheck() const
		{
			return m_bDoInitialSphereCheck;
		}

		const Vector3& GetStart() const { return m_segProbe.A; }
		const Vector3& GetEnd() const { return m_segProbe.B; }
		float GetRadius() const { return m_fRadius; }

	private:
		phSegment m_segProbe;
		float m_fRadius;
		bool m_bDoInitialSphereCheck;

		friend class CShapeTest;
		friend class CShapeTestManager;
	};

} // namespace WorldProbe


#endif // SHAPETEST_CAPSULE_DESC_H
