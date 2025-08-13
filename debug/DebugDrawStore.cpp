// File header
#include "Debug/DebugDrawStore.h"
#include "Debug/VectorMap.h"

#include "ai/aichannel.h"
#include "vector/colors.h"
#include "system/memmanager.h"

#if DEBUG_DRAW

// Keep this up to date!
#define LARGEST_DEBUG_DRAWABLE (sizeof(CDebugDrawableText))

CompileTimeAssert(sizeof(CDebugDrawable)				<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableAxis)			<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableLine)			<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableLine2d)			<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableVectorMapLine)	<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableVectorMapCircle) <= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableCone)			<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableSphere)			<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableCapsule)			<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableText)			<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableBoxOrientated)	<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawablePoly)			<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableArrow)			<= LARGEST_DEBUG_DRAWABLE);
CompileTimeAssert(sizeof(CDebugDrawableCross)			<= LARGEST_DEBUG_DRAWABLE);

void CDebugDrawableVectorMapLine::Render() const 
{ 
	CVectorMap::DrawLine(RCC_VECTOR3(m_vStart), RCC_VECTOR3(m_vEnd), m_colour, m_colour); 
}

void CDebugDrawableVectorMapCircle::Render() const
{
	CVectorMap::DrawCircle(RCC_VECTOR3(m_vPos), m_fRadius, m_colour, false);
}

CDebugDrawStore::CDebugDrawStore()
: m_pPool(NULL)
{
}

CDebugDrawStore::CDebugDrawStore(s32 iStoreMax)
: m_pPool(NULL)
{
	InitStoreSize(iStoreMax);
}

void CDebugDrawStore::InitStoreSize(s32 iStoreMax)
{
	if (aiVerifyf(!m_pPool, "Pool pointer wasn't NULL"))
	{
		USE_DEBUG_MEMORY();

#if COMMERCE_CONTAINER
		sysMemManager::GetInstance().DisableFlexHeap();
		m_pPool = rage_new DebugDrawPool(iStoreMax, "CDebugDrawStore", 0, LARGEST_DEBUG_DRAWABLE);
		sysMemManager::GetInstance().EnableFlexHeap();
#else
		m_pPool = rage_new DebugDrawPool(iStoreMax, "CDebugDrawStore", 0, LARGEST_DEBUG_DRAWABLE);
#endif
	}
}

void CDebugDrawStore::AddAxis(Mat34V_In mtx, float scale, bool drawArrows, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		Color32 unused = Color_purple;
		new(pStorage) CDebugDrawableAxis(mtx, unused, scale, drawArrows, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::AddLine(Vec3V_In vStart, Vec3V_In vEnd, const Color32& colour, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableLine(vStart, vEnd, colour, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::AddLine(const Vector2& vStart, const Vector2& vEnd, const Color32& colour, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableLine2d(vStart, vEnd, colour, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::AddVectorMapLine(Vec3V_In vStart, Vec3V_In vEnd, const Color32& colour, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableVectorMapLine(vStart, vEnd, colour, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::AddVectorMapCircle(Vec3V_In vPos, float fRadius, const Color32& colour, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableVectorMapCircle(vPos, fRadius, colour, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::AddSphere(Vec3V_In vPos, float fRadius, const Color32& colour, u32 uExpiryTime, u32 uKey, bool bSolid, u32 uNumSteps)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableSphere(vPos, fRadius, colour, uExpiryTime, uKey, bSolid, uNumSteps);
	}
}

void CDebugDrawStore::AddCone(Vec3V_In start, Vec3V_In end, float r, const Color32 col, bool cap, bool solid, int numSides, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableCone(start, end, r, col, cap, solid, numSides, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::AddCapsule(Vec3V_In vStart, Vec3V_In vEnd, float fRadius, const Color32& colour, u32 uExpiryTime, u32 uKey, bool bSolid)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableCapsule(vStart, vEnd, fRadius, colour, uExpiryTime, uKey, bSolid);
	}
}

void CDebugDrawStore::AddText(Vec3V_In vPos, const s32 iScreenOffsetX, const s32 iScreenOffsetY, const char* szText, const Color32& colour, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableText(vPos, iScreenOffsetX, iScreenOffsetY, szText, colour, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::Add2DText(Vec3V_In vPos, const char* szText, const Color32& colour, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableText(vPos, 0, 0, szText, colour, uExpiryTime, uKey, true);
	}
}

void CDebugDrawStore::AddOBB(Vec3V_In vMin, Vec3V_In vMax, Mat34V_In mat, const Color32& colour, u32 uExpiryTime , u32 uKey )
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableBoxOrientated(vMin, vMax, mat, colour, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::AddPoly(Vec3V_In v1, Vec3V_In v2, Vec3V_In v3, const Color32& colour, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawablePoly(v1,v2,v3, colour, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::AddArrow(Vec3V_In vStart, Vec3V_In vEnd, float fArrowHeadSize, const Color32& colour, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableArrow(vStart, vEnd, fArrowHeadSize, colour, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::AddCross(Vec3V_In pos, const float size, const Color32& col, u32 uExpiryTime, u32 uKey)
{
	CDebugDrawable* pStorage = GetStorage(uKey);
	if(pStorage)
	{
		new(pStorage) CDebugDrawableCross(pos, size, col, uExpiryTime, uKey);
	}
}

void CDebugDrawStore::Render()
{
	if(m_pPool && m_pPool->GetNoOfUsedSpaces() > 0)
	{
		for(s32 i = 0; i < m_pPool->GetSize(); i++)
		{
			CDebugDrawable* pDrawable = m_pPool->GetSlot(i);
			if(pDrawable)
			{
				if(pDrawable->GetHasExpired())
				{
					Delete(pDrawable);
				}
				else
				{
					pDrawable->Render();
				}
			}
		}
	}
}

void CDebugDrawStore::ClearAll()
{
	if(m_pPool)
	{
		for(s32 i = 0; i < m_pPool->GetSize(); i++)
		{
			CDebugDrawable* pDrawable = m_pPool->GetSlot(i);
			if(pDrawable)
			{
				Delete(pDrawable);
			}
		}
	}
}

void CDebugDrawStore::Clear(u32 uKey)
{
	if(m_pPool)
	{
		if(uKey != 0)
		{
			for(s32 i = 0; i < m_pPool->GetSize(); i++)
			{
				CDebugDrawable* pDrawable = m_pPool->GetSlot(i);
				if(pDrawable && pDrawable->GetKey() == uKey)
				{
					// We have found the key, delete it
					Delete(pDrawable);
					break;
				}
			}
		}
	}
}

CDebugDrawable* CDebugDrawStore::GetStorage(u32 uKey)
{
	sysCriticalSection lock(m_criticalSection);

	if(m_pPool)
	{
		// Clear any drawable currently using the key
		Clear(uKey);

		if(m_pPool->GetNoOfFreeSpaces() == 0)
		{
			DeleteOldestDrawable();
		}

		if(m_pPool->GetNoOfFreeSpaces() != 0)
		{
			return m_pPool->New();
		}
	}

	return NULL;
}

void CDebugDrawStore::DeleteOldestDrawable()
{
	if(m_pPool)
	{
		CDebugDrawable* pOldestDrawable = NULL;

		for(s32 i = 0; i < m_pPool->GetSize(); i++)
		{
			CDebugDrawable* pDrawable = m_pPool->GetSlot(i);
			if(pDrawable)
			{
				if(pOldestDrawable == NULL || pDrawable->GetCreationTime() < pOldestDrawable->GetCreationTime())
				{
					pOldestDrawable = pDrawable;
				}
			}
		}

		if(pOldestDrawable)
		{
			Delete(pOldestDrawable);
		}
	}
}

void CDebugDrawStore::Delete(CDebugDrawable* pDrawable)
{
	sysCriticalSection lock(m_criticalSection);
	if(m_pPool)
	{
		m_pPool->Delete(pDrawable);
	}
}

#endif // DEBUG_DRAW
