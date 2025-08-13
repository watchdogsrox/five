#include "system/eaptr.h"
#include "system/cache.h"
#include "system/dependency.h"
#include "system/interlocked.h"
#include "system/ipc.h"
#include "grcore/matrix43.h"

using namespace rage;

void CopyOffMatrixSetImpl(Mat34V* objectMtx, Mat34V* invJointScaleMtx, u16* lodSkelMap, Matrix43* outMtxs, const bool isSkinned, const u32 numBones, u16 firstBoneToScale, u16 lastBoneToScale, float fScaleFactor)
{
#	if RSG_XENON
		Matrix43* const outMtxsBegin = outMtxs;
#	endif

	bool bDoAnyScaling = fabs(fScaleFactor - 1.0f) > SMALL_FLOAT;
	if( isSkinned )
	{
		if (lodSkelMap)
		{
			for (u32 i = 0; i < numBones; ++i, ++outMtxs)
			{
				Mat34V mtx;
				rage::Transform(mtx, objectMtx[lodSkelMap[i]], invJointScaleMtx[lodSkelMap[i]]);
				if (bDoAnyScaling && lodSkelMap[i] >= firstBoneToScale && lodSkelMap[i] <= lastBoneToScale)
				{
					Vec3V vScale(fScaleFactor, fScaleFactor, fScaleFactor);
					Scale(mtx, vScale, mtx);
				}
				outMtxs->FromMatrix34(mtx);
			}
		}
		else
		{
			for(u32 i = 0; i < numBones; ++i, ++outMtxs, ++objectMtx, ++invJointScaleMtx)
			{
				Mat34V mtx;
				rage::Transform(mtx, *objectMtx, *invJointScaleMtx);
				if (bDoAnyScaling && i >= firstBoneToScale && i <= lastBoneToScale)
				{
					Vec3V vScale(fScaleFactor, fScaleFactor, fScaleFactor);
					Scale(mtx, vScale, mtx);
				}
				outMtxs->FromMatrix34(mtx);
			}
		}
	}
	else
	{
		if (lodSkelMap)
		{
			for(u32 i = 0; i < numBones; ++i, ++outMtxs)
			{
				Mat34V mtx = objectMtx[lodSkelMap[i]];
				if (bDoAnyScaling && lodSkelMap[i] >= firstBoneToScale && lodSkelMap[i] <= lastBoneToScale)
				{
					Vec3V vScale(fScaleFactor, fScaleFactor, fScaleFactor);
					Scale(mtx, vScale, mtx);
				}
				outMtxs->FromMatrix34(mtx);
			}
		}
		else
		{
			for(u32 i = 0; i < numBones; ++i, ++objectMtx, ++outMtxs)
			{
				Mat34V mtx = *objectMtx;
				if (bDoAnyScaling && i >= firstBoneToScale && i <= lastBoneToScale)
				{
					Vec3V vScale(fScaleFactor, fScaleFactor, fScaleFactor);
					Scale(mtx, vScale, mtx);
				}
				outMtxs->FromMatrix34(mtx);
			}
		}
	}

#	if RSG_XENON
		WritebackDC(outMtxsBegin, (uptr)outMtxs-(uptr)outMtxsBegin);
#	endif
}

bool CopyOffMatrixSetSPU_Dependency(const sysDependency& dependency)
{
	Mat34V*				objectMtx					= static_cast< Mat34V* >( dependency.m_Params[0].m_AsPtr );
	Mat34V*				invJointScaleMtx			= static_cast< Mat34V* >( dependency.m_Params[1].m_AsPtr );
	u16*				lodSkelMap					= static_cast< u16* >( dependency.m_Params[2].m_AsPtr );
	Matrix43*			outMtxs						= static_cast< Matrix43* >( dependency.m_Params[3].m_AsPtr );
	const bool			isSkinned					= dependency.m_Params[4].m_AsShort.m_Low != 0;
	const u16			numBones					= static_cast< u16 >( dependency.m_Params[4].m_AsShort.m_High );
	u16					firstBoneToScale			= static_cast< u16 >( dependency.m_Params[5].m_AsShort.m_Low );
	u16					lastBoneToScale				= static_cast< u16 >( dependency.m_Params[5].m_AsShort.m_High );
	float				fScaleFactor				= static_cast< float >( dependency.m_Params[6].m_AsFloat );
	sysEaPtr< u32 >		finishedDependencyCountEa	= static_cast< u32* >( dependency.m_Params[7].m_AsPtr );
	u32					expectedDepdencyCount		= dependency.m_Params[8].m_AsUInt;
	sysIpcSema*			pSema						= static_cast< sysIpcSema*>(dependency.m_Params[9].m_AsPtr);	

	CopyOffMatrixSetImpl( objectMtx, invJointScaleMtx, lodSkelMap, outMtxs, isSkinned, numBones, firstBoneToScale, lastBoneToScale, fScaleFactor );

	u32 finishedCount = sysInterlockedIncrement(finishedDependencyCountEa.ToPtr());
	if(finishedCount == expectedDepdencyCount)
	{
		sysIpcSignalSema(*pSema);
	}
	
	return true;
}
