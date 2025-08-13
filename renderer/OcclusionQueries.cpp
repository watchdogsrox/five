/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    renderer/OcclusionQueries.cpp
// PURPOSE : per Entity Occlusion Query handling.
// AUTHOR :  Alex H.
//
/////////////////////////////////////////////////////////////////////////////////
#include "grcore/allocscope.h"
#include "grcore/debugdraw.h"

#include "renderer/OcclusionQueries.h"
#include "renderer/Deferred/DeferredConfig.h"
#include "renderer/Sprite2d.h"
#include "system/FindSize.h"

atRangeArray<OcclusionQueries::OcclusionQuery,MAX_OCCLUSION_QUERIES> OcclusionQueries::s_OcclusionQueryArray;
atFreeList<u8,MAX_OCCLUSION_QUERIES> OcclusionQueries::s_OcclusionQueryFreeList;
static int s_OQAllocatedQueries;

atRangeArray<OcclusionQueries::ConditionalQuery,MAX_CONDITIONAL_QUERIES> OcclusionQueries::s_ConditionalQueryArray;
atFreeList<u8,MAX_CONDITIONAL_QUERIES> OcclusionQueries::s_ConditionalQueryFreeList;
static int s_CQAllocatedQueries;

int OcclusionQueries::s_BufferIdx;
grcRasterizerStateHandle OcclusionQueries::s_OcclusionRS;
grcDepthStencilStateHandle OcclusionQueries::s_OcclusionDS;
grcBlendStateHandle OcclusionQueries::s_OcclusionBS;

#if __BANK
static bool s_OQrenderOpaque = false;
static bool s_OQDebugRender = false;
static bool s_CQrenderOpaque = false;
#endif // __BANK

struct BoxQuery 
{
	Mat34V mtx;
	union {
		grcOcclusionQuery OQuery;
		grcConditionalQuery CQuery;
	};
} ;

//
// Occlusion Queries
//		

OcclusionQueries::dlCmdBoxOcclusionQueries::dlCmdBoxOcclusionQueries() 
{ 
	maxQueries = s_OQAllocatedQueries;
	Assert(maxQueries);
	
	queries = gDCBuffer->AllocateObjectFromPagedMemory<BoxQuery>(DPT_LIFETIME_RENDER_THREAD, sizeof(BoxQuery) * maxQueries);
	
	BoxQuery* pDest = queries;
	Assert(pDest);
	
	int bufferIdx = GetRenderBufferIdx();
	
	numQueries = 0;
	for(int i=0;i<s_OcclusionQueryArray.GetMaxCount();i++)
	{
		OcclusionQuery &query = s_OcclusionQueryArray[i];
		
		if( query.GetIsAllocated() )
		{
			pDest->mtx = query.mtx;
			pDest->OQuery = query.m_occlusionQueries[bufferIdx];
			query.m_queryKicked[bufferIdx] = true;
			Assert(pDest->OQuery);
			
			pDest++;
			numQueries++;
		}
	}

	Assert(numQueries<=maxQueries);
}

void OcclusionQueries::dlCmdBoxOcclusionQueries::Execute()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	BoxQuery* matrices = queries;

	if( numQueries > 0 )
	{
		grcRasterizerStateHandle currentRS = grcStateBlock::RS_Active;
		grcDepthStencilStateHandle currentDSS = grcStateBlock::DSS_Active;
		grcBlendStateHandle currentBS = grcStateBlock::BS_Active;

#if __BANK
		grcStateBlock::SetStates(GetRSBlock(), GetDSBlock(), s_OQrenderOpaque ? grcStateBlock::BS_Default : GetBSBLock());
#else // __BANK
		grcStateBlock::SetStates(GetRSBlock(), GetDSBlock(), GetBSBLock());
#endif // __BANK
						
		for(int i=0;i<numQueries;i++)
		{
			const Vec3V a = matrices->mtx.a();
			const Vec3V b = matrices->mtx.b();
			const Vec3V c = matrices->mtx.c();
			const Vec3V d = matrices->mtx.d();

			const Vec3V size = 	Vec3V(a.GetW(),b.GetW(),c.GetW());
			
			Mat34V mtx(a,b,c,d);
			mtx.SetM30M31M32Zero();
			
			grcOcclusionQuery query = matrices->OQuery;
			Assert(query);
			
			GRCDEVICE.BeginOcclusionQuery(query);
	
			grcDrawSolidBox(VEC3V_TO_VECTOR3(size), MAT34V_TO_MATRIX34(mtx), Color32(0xFFFFFFFF));
		
			GRCDEVICE.EndOcclusionQuery(query);
			
			matrices++;
		}

		grcWorldIdentity();
		
		grcStateBlock::SetStates(currentRS, currentDSS, currentBS);
	}
}


void OcclusionQueries::OcclusionQuery::Init()
{
	mtx = Mat34V(V_ZERO);
	pixelCount = -1;
	for(int i=0;i<m_queryKicked.GetMaxCount();i++)
	{
		m_queryKicked[i] = false;
	}
	SetIsAllocated(true);
}

void OcclusionQueries::OcclusionQuery::Shutdown()
{
	SetIsAllocated(false);
}

void OcclusionQueries::OcclusionQuery::SetBoundingBox(Vec3V_In min, Vec3V_In max, Mat34V_In matrix)
{
	const Vec3V size = max - min;

	const Vec3V center = (max + min) * ScalarV(V_HALF);
	
	const Vec3V worldcenter = Transform(matrix,center);
	
	Vec3V a = matrix.a();
	Vec3V b = matrix.b();
	Vec3V c = matrix.c();
	Vec3V d = worldcenter;

	a.SetW(size.GetX());
	b.SetW(size.GetY());
	c.SetW(size.GetZ());
	d.SetW(mtx.d().GetW());
	
	const Mat34V temp(a,b,c,d);
	mtx = temp;
}

void OcclusionQueries::OcclusionQuery::SetIsAllocated(bool b)
{
	if( b )
		flags |= OQ_Allocated;
	else
		flags &= ~OQ_Allocated;
}

bool OcclusionQueries::OcclusionQuery::GetIsAllocated() const
{
	return (flags & OQ_Allocated) == OQ_Allocated;
}

Mat34V_Out OcclusionQueries::OcclusionQuery::GetMatrix() const
{
	return mtx;
}

Vec3V_Out OcclusionQueries::OcclusionQuery::GetSize() const
{
	const Vec3V a = mtx.a();
	const Vec3V b = mtx.b();
	const Vec3V c = mtx.c();
	
	const Vec3V res(a.GetW(),b.GetW(),c.GetW());
	
	return res;
}

//
// Conditional Queries
//		
		
OcclusionQueries::dlCmdBoxConditionalQueries::dlCmdBoxConditionalQueries() 
{ 
	maxQueries = s_CQAllocatedQueries;
	Assert(maxQueries);
	
	queries = gDCBuffer->AllocateObjectFromPagedMemory<BoxQuery>(DPT_LIFETIME_RENDER_THREAD, sizeof(BoxQuery) * maxQueries);
	
	BoxQuery* pDest = queries;
	Assert(pDest);
	
	numQueries = 0;
	for(int i=0;i<s_ConditionalQueryArray.GetMaxCount();i++)
	{
		ConditionalQuery &query = s_ConditionalQueryArray[i];
		
		if( query.GetIsAllocated() )
		{
			pDest->mtx = query.mtx;
			pDest->CQuery = query.m_conditionalQuery;
			Assert(pDest->CQuery);
			
			pDest++;
			numQueries++;
		}
	}

	Assert(numQueries<=maxQueries);
}

void OcclusionQueries::dlCmdBoxConditionalQueries::Execute()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	BoxQuery* matrices = queries;

	if( numQueries > 0 )
	{
		grcRasterizerStateHandle currentRS = grcStateBlock::RS_Active;
		grcDepthStencilStateHandle currentDSS = grcStateBlock::DSS_Active;
		grcBlendStateHandle currentBS = grcStateBlock::BS_Active;

		Color32 boxColor(0x00FFFFFF);

#if __BANK
		if (s_OQrenderOpaque)
		{
			grcStateBlock::SetStates(GetRSBlock(), GetDSBlock(), grcStateBlock::BS_Default);
			boxColor.Set(0xFFFFFFFF);
		}
		else
		{
			grcStateBlock::SetStates(GetRSBlock(), GetDSBlock(), GetBSBLock());
		}
#else // __BANK
		grcStateBlock::SetStates(GetRSBlock(), GetDSBlock(), GetBSBLock());
#endif // __BANK
		
		for(int i=0;i<numQueries;i++)
		{
			const Vec3V a = matrices->mtx.a();
			const Vec3V b = matrices->mtx.b();
			const Vec3V c = matrices->mtx.c();
			const Vec3V d = matrices->mtx.d();

			const Vec3V size = 	Vec3V(a.GetW(),b.GetW(),c.GetW());
			
			Mat34V mtx(a,b,c,d);
			mtx.SetM30M31M32Zero();
			
			grcConditionalQuery query = matrices->CQuery;
			Assert(query);
			
			GRCDEVICE.BeginConditionalQuery(query);
	
			grcDrawSolidBox(VEC3V_TO_VECTOR3(size), MAT34V_TO_MATRIX34(mtx), boxColor);
		
			GRCDEVICE.EndConditionalQuery(query);
			
			matrices++;
		}

		grcWorldIdentity();
		
		grcStateBlock::SetStates(currentRS, currentDSS, currentBS);
	}
}


void OcclusionQueries::ConditionalQuery::Init()
{
	mtx = Mat34V(V_ZERO);
	SetIsAllocated(true);
}

void OcclusionQueries::ConditionalQuery::Shutdown()
{
	SetIsAllocated(false);
}

void OcclusionQueries::ConditionalQuery::SetBoundingBox(Vec3V_In min, Vec3V_In max, Mat34V_In matrix)
{
	const Vec3V size = max - min;

	const Vec3V center = (max + min) * ScalarV(V_HALF);
	
	const Vec3V worldcenter = Transform(matrix,center);
	
	Vec3V a = matrix.a();
	Vec3V b = matrix.b();
	Vec3V c = matrix.c();
	Vec3V d = worldcenter;

	a.SetW(size.GetX());
	b.SetW(size.GetY());
	c.SetW(size.GetZ());
	d.SetW(mtx.d().GetW());
	
	const Mat34V temp(a,b,c,d);
	mtx = temp;
}

void OcclusionQueries::ConditionalQuery::SetIsAllocated(bool b)
{
	if( b )
		flags |= CQ_Allocated;
	else
		flags &= ~CQ_Allocated;
}

bool OcclusionQueries::ConditionalQuery::GetIsAllocated() const
{
	return (flags & CQ_Allocated) == CQ_Allocated;
}

Mat34V_Out OcclusionQueries::ConditionalQuery::GetMatrix() const
{
	return mtx;
}

Vec3V_Out OcclusionQueries::ConditionalQuery::GetSize() const
{
	const Vec3V a = mtx.a();
	const Vec3V b = mtx.b();
	const Vec3V c = mtx.c();
	
	const Vec3V res(a.GetW(),b.GetW(),c.GetW());
	
	return res;
}
		
void OcclusionQueries::Init()
{
	s_OcclusionQueryFreeList.MakeAllFree();
	s_ConditionalQueryFreeList.MakeAllFree();
	
	s_OQAllocatedQueries = 0;
	s_CQAllocatedQueries = 0;
	
	s_BufferIdx	= 0;
	
	for(int i=0;i<s_OcclusionQueryArray.GetMaxCount();i++)
	{
		s_OcclusionQueryArray[i].Init();
		s_OcclusionQueryArray[i].SetIsAllocated(false);
	}

	for(int i=0;i<s_ConditionalQueryArray.GetMaxCount();i++)
	{
		s_ConditionalQueryArray[i].Init();
		s_ConditionalQueryArray[i].SetIsAllocated(false);
	}
	
	InitHWQueries();

	grcRasterizerStateDesc RSDesc;
	RSDesc.CullMode=grcRSV::CULL_NONE;
	s_OcclusionRS = grcStateBlock::CreateRasterizerState(RSDesc);

	grcDepthStencilStateDesc DSDesc;
	DSDesc.DepthEnable = true;
	DSDesc.DepthWriteMask = false;
	DSDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	s_OcclusionDS = grcStateBlock::CreateDepthStencilState(DSDesc);

	grcBlendStateDesc BSDesc;
	BSDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	
	s_OcclusionBS = grcStateBlock::CreateBlendState(BSDesc);
	
	DLC_REGISTER_EXTERNAL(dlCmdBoxOcclusionQueries);
	DLC_REGISTER_EXTERNAL(dlCmdBoxConditionalQueries);
	
}

#if __BANK
void OcclusionQueries::AddWidgets(bkBank &bk)
{
	bk.PushGroup("Visibility Queries", false);
	bk.AddToggle("Render opaque occlusion queries",&s_OQrenderOpaque);
	bk.AddToggle("Render opaque conditional queries",&s_CQrenderOpaque);
	bk.AddToggle("Render debug info",&s_OQDebugRender);
	bk.PopGroup();
}
#endif // __BANK

void OcclusionQueries::InitHWQueries()
{
	for(int i=0;i<s_OcclusionQueryArray.GetMaxCount();i++)
	{
		OcclusionQuery& query = s_OcclusionQueryArray[i];
		for(int j=0;j<query.m_occlusionQueries.GetMaxCount();j++)
		{
			query.m_occlusionQueries[j] = GRCDEVICE.CreateOcclusionQuery();
			Assert(query.m_occlusionQueries[j]);
		}
	}

	for(int i=0;i<s_ConditionalQueryArray.GetMaxCount();i++)
	{
		ConditionalQuery& query = s_ConditionalQueryArray[i];
		query.m_conditionalQuery = GRCDEVICE.CreateConditionalQuery();
	}
}


void OcclusionQueries::Shutdown()
{
	for(int i=0;i<s_OcclusionQueryArray.GetMaxCount();i++)
	{
		if( s_OcclusionQueryArray[i].GetIsAllocated() )
		{
			s_OcclusionQueryArray[i].Shutdown();

			OcclusionQuery& query = s_OcclusionQueryArray[i];
			for(int j=0;j<query.m_occlusionQueries.GetMaxCount();j++)
			{
				if( query.m_occlusionQueries[i] )
					GRCDEVICE.DestroyOcclusionQuery(query.m_occlusionQueries[i]);
			}
		}
	}
	
	for(int i=0;i<s_ConditionalQueryArray.GetMaxCount();i++)
	{
		if( s_ConditionalQueryArray[i].GetIsAllocated() )
		{
			s_ConditionalQueryArray[i].Shutdown();

			ConditionalQuery& query = s_ConditionalQueryArray[i];
			if( query.m_conditionalQuery )
			{
					GRCDEVICE.DestroyConditionalQuery(query.m_conditionalQuery);
			}
		}
	}
	
}

unsigned int OcclusionQueries::OQAllocate()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	int newId = s_OcclusionQueryFreeList.Allocate();
		
	if( newId > -1 )
	{
		s_OcclusionQueryArray[newId].Init();
		s_OQAllocatedQueries++;
	}
		
	return newId + 1;
}

void OcclusionQueries::OQFree(unsigned int idx)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	Assert(idx != 0);
	Assert(s_OcclusionQueryArray[idx - 1].GetIsAllocated());
	s_OcclusionQueryArray[idx - 1].Shutdown();
	s_OcclusionQueryFreeList.Free(idx - 1);
	s_OQAllocatedQueries--;
}

void OcclusionQueries::OQSetBoundingBox(unsigned int idx,Vec3V_In min, Vec3V_In max, Mat34V_In matrix)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	Assert(idx != 0);
	Assert(s_OcclusionQueryArray[idx - 1].GetIsAllocated());
	s_OcclusionQueryArray[idx - 1].SetBoundingBox(min,max,matrix);
}

grcOcclusionQuery OcclusionQueries::OQGetQuery(unsigned int idx)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	Assert(idx != 0);
	Assert(s_OcclusionQueryArray[idx - 1].GetIsAllocated());
	int bufferIdx = GetRenderBufferIdx();

	OcclusionQuery& query = s_OcclusionQueryArray[idx - 1];
	
	return query.m_occlusionQueries[bufferIdx];
}

int OcclusionQueries::OQGetQueryResult(unsigned int idx)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	Assert(idx != 0);
	Assert(s_OcclusionQueryArray[idx - 1].GetIsAllocated());
	return s_OcclusionQueryArray[idx - 1].pixelCount;
}

unsigned int OcclusionQueries::CQAllocate()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	int newId = s_ConditionalQueryFreeList.Allocate();
		
	if( newId > -1 )
	{
		s_ConditionalQueryArray[newId].Init();
		s_CQAllocatedQueries++;
	}
		
	return newId + 1;
}

void OcclusionQueries::CQFree(unsigned int idx)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	Assert(idx != 0);
	Assert(s_ConditionalQueryArray[idx - 1].GetIsAllocated());
	
	s_ConditionalQueryArray[idx - 1].Shutdown();
	s_ConditionalQueryFreeList.Free(idx - 1);
	s_CQAllocatedQueries--;
}

void OcclusionQueries::CQSetBoundingBox(unsigned int idx,Vec3V_In min, Vec3V_In max, Mat34V_In matrix)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	Assert(idx != 0);
	Assert(s_ConditionalQueryArray[idx - 1].GetIsAllocated());
	s_ConditionalQueryArray[idx - 1].SetBoundingBox(min,max,matrix);
}

grcConditionalQuery OcclusionQueries::CQGetQuery(unsigned int idx)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	Assert(idx != 0);
	Assert(s_ConditionalQueryArray[idx - 1].GetIsAllocated());
	
	ConditionalQuery& query = s_ConditionalQueryArray[idx - 1];
	
	return query.m_conditionalQuery;
}

void OcclusionQueries::RenderBoxBasedQueries()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	int OQueries = s_OQAllocatedQueries;
	if( OQueries )
	{
		DLC(OcclusionQueries::dlCmdBoxOcclusionQueries,());
	}

	int CQueries = s_CQAllocatedQueries;
	if( CQueries )
	{
		DLC(OcclusionQueries::dlCmdBoxConditionalQueries,());
	}

}

void OcclusionQueries::GatherQueryResults()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	
	int bufferIdx = GetUpdateBufferIdx();

	for(int i=0;i<s_OcclusionQueryArray.GetMaxCount();i++)
	{
		OcclusionQuery &query = s_OcclusionQueryArray[i];
		
		if( query.GetIsAllocated() )
		{
			grcOcclusionQuery readQuery = query.m_occlusionQueries[bufferIdx];

			bool wasKicked = query.m_queryKicked[bufferIdx];
			
			int result = -1;
			u32 drawCount = 0;
#if __BANK
			bool HWFailure = false;
#endif // __BANK			
			if( readQuery && wasKicked)
			{
				if( GRCDEVICE.GetOcclusionQueryData(readQuery,drawCount) )
				{
					result = (int)drawCount;
				}
#if __BANK
				else
				{
					HWFailure = true;
				}
#endif // __BANK				
					
			}
			
			query.pixelCount = result;
			
#if __BANK
			if( s_OQDebugRender )
			{
				const Vec3V d = query.mtx.d();
				
				char str[128];
				sprintf(str,"%d",query.pixelCount);
				Color32 col = query.pixelCount == -1 ? Color32(0xff,0x00,0x00) :  Color32(0x00,0xff,0x00);
				col = HWFailure == true ? Color32(0x00,0x00,0xff) : col;
				
				grcDebugDraw::Text(d, col, str, false);
			}
#endif // __BANK			
		}
	}
	
	FlipBufferIdx();
}

int OcclusionQueries::GetRenderBufferIdx() 
{ 
	return s_BufferIdx; 
}

int OcclusionQueries::GetUpdateBufferIdx() 
{ 
	return (s_BufferIdx+1) % MAX_NUM_BUFFERED_OCCLUSION_QUERIES;
}

void OcclusionQueries::FlipBufferIdx() 
{ 
	s_BufferIdx = (s_BufferIdx+1) % MAX_NUM_BUFFERED_OCCLUSION_QUERIES;
}


