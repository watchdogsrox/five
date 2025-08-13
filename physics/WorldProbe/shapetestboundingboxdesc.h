#ifndef SHAPETEST_BOUNDINGBOX_DESC_H
#define SHAPETEST_BOUNDINGBOX_DESC_H

// RAGE headers:

// Game headers:
//#include "physics/WorldProbe/wpcommon.h"
#include "physics/WorldProbe/shapetestdesc.h"

namespace WorldProbe
{

	//There isn't a phShapeTest<X> to do bounding box tests. We iterate through the level's insts ourselves
	//This corresponds to the data of a phShapeTest<X> for a bounding box test. 
	class CBoundingBoxShapeTestData
	{
	public:
		CShapeTestResults* m_pResults;
		InExInstanceArray m_aExcludeInstList;
		phBound* m_pBound;
		Matrix34 m_TransformMatrix;
		Vector3 m_vExtendBBoxMin;
		Vector3 m_vExtendBBoxMax;
		u32 m_nIncludeFlags;
		u32 m_nTypeFlags;
		u32 m_nExcludeTypeFlags;
		u32 m_nNumHits;
		u8 m_nStateIncludeFlags;
	};

	////////////////////////////////////////////
	// Descriptor class for bounding box tests.
	////////////////////////////////////////////
	class CShapeTestBoundingBoxDesc : public CShapeTestDesc
	{
	public:
		CShapeTestBoundingBoxDesc()
			: CShapeTestDesc(),
			m_pBound(NULL),
			m_pTransformMatrix(NULL),
			m_vExtendBBoxMin(VEC3_ZERO),
			m_vExtendBBoxMax(VEC3_ZERO)
		{
			m_eShapeTestType = BOUNDING_BOX_TEST;
		}

		void SetBound(phBound* pBound)
		{
			m_pBound = pBound;
		}
		void SetBound(const phBound* pBound)
		{
			m_pBound = const_cast<phBound*>(pBound);
		}
		void SetTransform(Matrix34* pMatrix)
		{
			m_pTransformMatrix = pMatrix;
		}
		void SetTransform(const Matrix34* pMatrix)
		{
			m_pTransformMatrix = const_cast<Matrix34*>(pMatrix);
		}
		void SetExtensionToBoundingBoxMin(const Vector3& vExpandMin)
		{
			physicsAssert(rage::FPIsFinite(vExpandMin.x) && rage::FPIsFinite(vExpandMin.y) && rage::FPIsFinite(vExpandMin.z));
			physicsAssert(vExpandMin.x <= 0.0f && vExpandMin.y <= 0.0f && vExpandMin.z <= 0.0f);

			m_vExtendBBoxMin = vExpandMin;
		}
		void SetExtensionToBoundingBoxMax(const Vector3& vExpandMax)
		{
			physicsAssert(rage::FPIsFinite(vExpandMax.x) && rage::FPIsFinite(vExpandMax.y) && rage::FPIsFinite(vExpandMax.z));

			m_vExtendBBoxMax = vExpandMax;
		}

	private:
		phBound* m_pBound;
		Matrix34* m_pTransformMatrix;
		Vector3 m_vExtendBBoxMin;
		Vector3 m_vExtendBBoxMax;

		friend class CShapeTest;
	};

} // namespace WorldProbe


#endif // SHAPETEST_BOUNDINGBOX_DESC_H
