#ifndef CULL_VOLUME_BOX_DESC_H
#define CULL_VOLUME_BOX_DESC_H

// RAGE headers:

// Game headers:
#include "physics/WorldProbe/cullvolumedesc.h"

namespace WorldProbe
{

	//////////////////////////////////////////////////////////
	// Descriptor class to describe a box shaped cull volume.
	//////////////////////////////////////////////////////////
	class CCullVolumeBoxDesc : public CCullVolumeDesc
	{
	public:
		CCullVolumeBoxDesc()
			: CCullVolumeDesc(),
			m_matBoxAxes(Matrix34::IdentityType),
			m_vBoxHalfWidths(Vector3::ZeroType)
		{
			m_eCullVolumeType = CULL_VOLUME_BOX;
		}

	public:
		void SetBox(const Matrix34& matBoxAxes, const Vector3& vBoxHalfWidths)
		{
			physicsAssert(rage::FPIsFinite(matBoxAxes.a.x) && rage::FPIsFinite(matBoxAxes.a.y) && rage::FPIsFinite(matBoxAxes.a.z));
			physicsAssert(rage::FPIsFinite(matBoxAxes.b.x) && rage::FPIsFinite(matBoxAxes.b.y) && rage::FPIsFinite(matBoxAxes.b.z));
			physicsAssert(rage::FPIsFinite(matBoxAxes.c.x) && rage::FPIsFinite(matBoxAxes.c.y) && rage::FPIsFinite(matBoxAxes.c.z));
			physicsAssert(rage::FPIsFinite(vBoxHalfWidths.x) && rage::FPIsFinite(vBoxHalfWidths.y) && rage::FPIsFinite(vBoxHalfWidths.z));

			m_matBoxAxes = matBoxAxes;
			m_vBoxHalfWidths = vBoxHalfWidths;
		}

	private:
		Matrix34 m_matBoxAxes;
		Vector3 m_vBoxHalfWidths;

		friend class CShapeTest;
	};

} // namespace WorldProbe

#endif // CULL_VOLUME_BOX_DESC_H
