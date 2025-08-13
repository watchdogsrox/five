/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    renderer/OcclusionQueries.h
// PURPOSE : per Entity Occlusion Query handling.
// AUTHOR :  Alex H.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _OCCLUSIONQUERIES_H_
#define _OCCLUSIONQUERIES_H_

#include "atl/array.h"
#include "atl/freelist.h"
#include "grcore/device.h"
#include "grcore/stateblock.h "

#include "fwdrawlist/drawlist.h"
#include "DrawLists/drawList.h"

namespace rage
{
	class bkBank;
}

struct BoxQuery;

#define MAX_OCCLUSION_QUERIES 68
#if RSG_PC
// Enable extra buffered occlusion queries on PC as the driver can force us to render multiple frames behind...
#define MAX_NUM_BUFFERED_OCCLUSION_QUERIES	6
#elif __XENON
#define MAX_NUM_BUFFERED_OCCLUSION_QUERIES	4
#else
#define MAX_NUM_BUFFERED_OCCLUSION_QUERIES	3
#endif // RSG_PC

#define MAX_CONDITIONAL_QUERIES 8
	
namespace OcclusionQueries
{
	class dlCmdBoxOcclusionQueries: public dlCmdBase {
	public:
		enum {
			INSTRUCTION_ID = DC_BoxOcclusionQueries,
		};

		s32 GetCommandSize()						{ return(sizeof(*this)); }
		dlCmdBoxOcclusionQueries();
		
		static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdBoxOcclusionQueries &) cmd).Execute(); }
		void Execute();
		
	private:
		int maxQueries;
		int numQueries;
		BoxQuery* queries;
	};

	class dlCmdBoxConditionalQueries: public dlCmdBase {
	public:
		enum {
			INSTRUCTION_ID = DC_BoxConditionalQueries,
		};

		s32 GetCommandSize()						{ return(sizeof(*this)); }
		dlCmdBoxConditionalQueries();
		
		static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdBoxConditionalQueries &) cmd).Execute(); }
		void Execute();
		
	private:
		int maxQueries;
		int numQueries;
		BoxQuery* queries;
	};

	struct OcclusionQuery {
		enum {
			OQ_Allocated		= 0x00000001,
		};

		void Init();
		void Shutdown();
		
		void SetBoundingBox(Vec3V_In min, Vec3V_In max, Mat34V_In matrix);
		
		void SetIsAllocated(bool b);
		bool GetIsAllocated() const;
		
		Mat34V_Out GetMatrix() const;
		Vec3V_Out GetSize() const;
		
		Mat34V mtx;
		atRangeArray<grcOcclusionQuery, MAX_NUM_BUFFERED_OCCLUSION_QUERIES> m_occlusionQueries;
		atRangeArray<bool, MAX_NUM_BUFFERED_OCCLUSION_QUERIES> m_queryKicked;
		int pixelCount;
		unsigned int flags;
	} ;

	struct ConditionalQuery {
		enum {
			CQ_Allocated		= 0x00000001,
		};

		void Init();
		void Shutdown();
		
		void SetBoundingBox(Vec3V_In min, Vec3V_In max, Mat34V_In matrix);
		
		void SetIsAllocated(bool b);
		bool GetIsAllocated() const;
		
		Mat34V_Out GetMatrix() const;
		Vec3V_Out GetSize() const;
		
		Mat34V mtx;
		grcConditionalQuery m_conditionalQuery;
		unsigned int flags;
		
	} ;
	
	extern atRangeArray<OcclusionQuery,MAX_OCCLUSION_QUERIES> s_OcclusionQueryArray;
	extern atFreeList<u8,MAX_OCCLUSION_QUERIES> s_OcclusionQueryFreeList;
	extern atRangeArray<ConditionalQuery,MAX_CONDITIONAL_QUERIES> s_ConditionalQueryArray;
	extern atFreeList<u8,MAX_CONDITIONAL_QUERIES> s_ConditionalQueryFreeList;
	
	extern int s_BufferIdx;
	extern grcRasterizerStateHandle s_OcclusionRS;
	extern grcDepthStencilStateHandle s_OcclusionDS;
	extern grcBlendStateHandle s_OcclusionBS;
	
	
	void Init();
#if __BANK	
	void AddWidgets(bkBank &bk);
#endif // __BANK	
	void InitHWQueries();
	void Shutdown();
	
	unsigned int OQAllocate();
	void OQFree(unsigned int idx);
	
	void OQSetBoundingBox(unsigned int idx,Vec3V_In min, Vec3V_In max, Mat34V_In matrix);
	 
	grcOcclusionQuery OQGetQuery(unsigned int idx);
	int OQGetQueryResult(unsigned int idx);
	
	unsigned int CQAllocate();
	void CQFree(unsigned int idx);
	
	void CQSetBoundingBox(unsigned int idx,Vec3V_In min, Vec3V_In max, Mat34V_In matrix);
	 
	grcConditionalQuery CQGetQuery(unsigned int idx);
	
	inline grcRasterizerStateHandle GetRSBlock() { return s_OcclusionRS; }
	inline grcDepthStencilStateHandle GetDSBlock() { return s_OcclusionDS; }
	inline grcBlendStateHandle GetBSBLock() { return s_OcclusionBS; }
	
	void RenderBoxBasedQueries();
	void GatherQueryResults();

	int GetRenderBufferIdx();
	int GetUpdateBufferIdx();
	void FlipBufferIdx();
	
};

#endif // _OCCLUSIONQUERIES_H_
