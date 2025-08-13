///////////////////////////////////////////////////////////////////////////////
//  
//	FILE:	Breakable.h
//	BY	: 	
//	FOR	:	Rockstar North
//	ON	:	14 June 2005
//
///////////////////////////////////////////////////////////////////////////////

#ifndef BREAKABLE_H
#define BREAKABLE_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////
#include "system/system.h"
#include "system/tls.h"
#include "debug/Debug.h"
#include "renderer/renderer.h"

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

class gtaFragType;
class CEntity;

namespace rage
{
	class rmcDrawable;
	class Matrix34;
	class crSkeleton;
	class fragType;
	class grmMatrixSet;
}

///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CBreakable  ///////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CBreakable
{	
///////////////////////////////////		
// FUNCTIONS 
///////////////////////////////////		
public:
	CBreakable();

	static void Insert(fragInst* movableId);
	static void Remove(fragInst* movableId);

	static void InitFragInst(fragInst* context);
	bool AddToDrawBucket(const rmcDrawable& toDraw, const Matrix34& matrix, const grmMatrixSet* matrixset, grmMatrixSet* sharedMatrixset, float lod, u16 drawableStats, float lodMult, bool forceHiLOD);
	static bool AddToDrawBucketFragCallback(const rmcDrawable& toDraw, const Matrix34& matrix, const crSkeleton* skeleton, grmMatrixSet* sharedMatrixset, float lod);

	void SetDrawBucketMask(u32 bucketMask)		{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_drawBucketMask = bucketMask;	}
	void SetRenderMode(eRenderMode renderMode)	{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_renderMode = renderMode;}
	void SetModelType(int type)					{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_modelType = type; }
	void SetEntityAlpha(int alpha)				{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_entityAlpha = alpha; }
	void SetIsVehicleMod(bool isMod)			{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_bIsVehicleMod = isMod; }
	void SetIsHd(bool isHd)						{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_bIsHd = isHd; }
	void SetIsWreckedVehicle(bool isWrecked)	{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_bIsWreckedVehicle = isWrecked; }
	void SetClampedLod(s8 clampedLod)			{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_clampedLod = clampedLod; }
	void SetLowLodMultiplier(float lowLodMult)	{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_lowLodMult = lowLodMult; }

	void SetLODFlags(u32 phaseLODs, u32 lastLODIdx) 
	{
		Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); 

		m_phaseLODs = phaseLODs;
		m_lastLODIdx = lastLODIdx;
	}

#if RSG_PC || __ASSERT
	void SetModelInfo(CBaseModelInfo *modelInfo)
	{
		m_modelInfo = modelInfo;
	}
#endif // RSG_PC || __ASSERT

	bool GetIsHd() const						{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_bIsHd; }
	bool GetIsWreckedVehicle() const			{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_bIsWreckedVehicle; }
	s8 GetClampedLod() const					{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return (s8)m_clampedLod; }
	float GetLowLodMultiplier() const			{Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_lowLodMult; }

	static int GetNumFragCacheObjects() 			{return m_nFragCacheObjectCount;}
	static void SetInsertDirectToRenderList(bool bOn)	{m_bInsertDirectToRenderList = bOn;}

	static void InitGlassInst(phGlassInst& pGlassInst);
	static void InitGlassBound(phBound& pGlassBound);
	static void InitShardInst(phInst& pShardInst);

	static bool AllowBreaking();

protected:
	u32 SelectVehicleLodLevel(const rmcDrawable& toDraw, float dist, float lodMult, bool forceHiLOD);

///////////////////////////////////		
// VARIABLES 
///////////////////////////////////		
protected:
	u32				m_drawBucketMask;
	eRenderMode		m_renderMode;
	int				m_modelType; // -1 if not set
	static int		m_nFragCacheObjectCount;
	int				m_entityAlpha;
	float			m_lowLodMult;
	u32				m_phaseLODs : 15;
	u32				m_lastLODIdx : 2;
	u32				m_clampedLod : 8;
	u32				m_pad : 7;
#if RSG_PC || __ASSERT
	CBaseModelInfo *m_modelInfo;
#endif // RSG_PC || __ASSERT

	// TODO -- pack these (and the int's above) better
	static bool			m_bInsertDirectToRenderList;
	bool			m_bIsVehicleMod;
	bool			m_bIsHd;
	bool			m_bIsWreckedVehicle;

}; // CBreakable


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

// extern CBreakable g_breakable;
extern __THREAD CBreakable *g_pBreakable;


#endif // BREAKABLE_H


///////////////////////////////////////////////////////////////////////////////
