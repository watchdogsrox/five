#ifndef CULL_VOLUME_CAPSULE_DESC_H
#define CULL_VOLUME_CAPSULE_DESC_H

// RAGE headers:

// Game headers:
#include "physics/WorldProbe/cullvolumedesc.h"

namespace WorldProbe
{

	//////////////////////////////////////////////////////////////
	// Descriptor class to describe a capsule shaped cull volume.
	//////////////////////////////////////////////////////////////
	class CCullVolumeCapsuleDesc : public CCullVolumeDesc
	{
	public:
		CCullVolumeCapsuleDesc()
			: CCullVolumeDesc(),
			m_vStartPos(Vector3(VEC3_ZERO)),
			m_vAxis(Vector3(VEC3_ZERO)),
			m_fLength(0.0f),
			m_fRadius(0.0f)
		{
			m_eCullVolumeType = CULL_VOLUME_CAPSULE;
		}

	public:
		void SetCapsule(const Vector3& vStart, const Vector3& vEnd, const float fRadius)
		{
			physicsAssert(rage::FPIsFinite(vStart.x) && rage::FPIsFinite(vStart.y) && rage::FPIsFinite(vStart.z));
			physicsAssert(rage::FPIsFinite(vEnd.x) && rage::FPIsFinite(vEnd.y) && rage::FPIsFinite(vEnd.z));
			physicsAssert(rage::FPIsFinite(fRadius));

			m_vStartPos = vStart;
			m_vAxis = vEnd-vStart;
			m_fLength = m_vAxis.Mag();
			m_vAxis.Normalize();
			m_fRadius = fRadius;
		}
		void SetCapsule(const Vector3& vStart, const Vector3& vDirNormalised, const float fLength, const float fRadius)
		{
			physicsAssert(rage::FPIsFinite(vStart.x) && rage::FPIsFinite(vStart.y) && rage::FPIsFinite(vStart.z));
			physicsAssert(rage::FPIsFinite(fRadius));
			physicsAssert(Vector3(vDirNormalised).Mag2() >= square(0.999f));
			physicsAssert(Vector3(vDirNormalised).Mag2() <= square(1.001f));

			m_vStartPos = vStart;
			m_vAxis = vDirNormalised;
			m_fLength = fLength;
			m_fRadius = fRadius;
		}

	private:
		Vector3 m_vStartPos;
		Vector3 m_vAxis;
		float m_fLength;
		float m_fRadius;

		friend class CShapeTest;
	};

} // namespace WorldProbe

#endif // CULL_VOLUME_CAPSULE_DESC_H
