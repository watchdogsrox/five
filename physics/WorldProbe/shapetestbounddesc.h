#ifndef SHAPETEST_BOUND_DESC_H
#define SHAPETEST_BOUND_DESC_H

// RAGE headers:

// Game headers:
#include "physics/WorldProbe/wpcommon.h"
#include "physics/WorldProbe/shapetestdesc.h"

namespace WorldProbe
{

	/////////////////////////////////////
	// Descriptor class for bound tests.
	/////////////////////////////////////
	class CShapeTestBoundDesc : public CShapeTestDesc
	{
	public:

		CShapeTestBoundDesc()
			: CShapeTestDesc(),
			m_pBound(NULL),
			m_pTransformMatrix(NULL)
		{
			m_eShapeTestType = BOUND_TEST;
			m_bTreatPolyhedralBoundsAsPrimitives = true;
		}

		void SetBound(phBound* pBound)
		{
			m_pBound = pBound;
		}
		void SetBound(const phBound* pBound)
		{
			m_pBound = const_cast<phBound*>(pBound);
		}
		// For convenience, allow bound to be derived from an entity instance.
		void SetBoundFromEntity(const CEntity* pEntity);
		void SetTransform(Matrix34* pMatrix)
		{
			m_pTransformMatrix = pMatrix;
		}
		void SetTransform(const Matrix34* pMatrix)
		{
			m_pTransformMatrix = const_cast<Matrix34*>(pMatrix);
		}

	private:
		phBound* m_pBound;
		Matrix34* m_pTransformMatrix;

		friend class CShapeTest;
	};

} // namespace WorldProbe


#endif // SHAPETEST_BOUND_DESC_H
