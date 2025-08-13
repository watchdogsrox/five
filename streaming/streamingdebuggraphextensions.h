#ifndef	__STREAMING_DEBUG_GRAPH_EXTENSIONS_H
#define	__STREAMING_DEBUG_GRAPH_EXTENSIONS_H

#if __BANK

#include "atl/map.h"
#include "renderer/PostScan.h"
#include "vector/vector2.h"
#include "vector/color32.h"
#include "fwscene/lod/LodTypes.h"
#include "fwscene/stores/dwdstore.h"
#include "scene/datafilemgr.h"
#include "streaming/streaming.h"
#include "streaming/streamingdebug.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginfo.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingallocator.h"
#include "streaming/streamingdebuggraph.h"

class CStreamGraphExtensionBase
{
public:

	CStreamGraphExtensionBase() 
	{
		m_bActive = false;
	}
	virtual ~CStreamGraphExtensionBase() {}

	bool	IsActive() { return m_bActive; }
	
	virtual void	Update()	{}							// Any key stuff
	virtual float	DrawMemoryBar(float x, float y) = 0;	// Draw the memory bar
	virtual float	DrawLegend(float x, float y) = 0;		// Draw the Legend
	virtual void	DrawFileList(float x, float y) = 0;		// Draw the File List
	virtual void	DrawFileInfo(float x, float y) = 0;
	virtual void	SwapBuffers() = 0;						// Called from CStreamGraph::SwapBuffers()
	virtual void	AddWidgets(bkBank* pBank) = 0;

public:

	bool	m_bActive;

	// Common functionality
	static float	backtraceX;
	static float	backtraceY;
	static void		DisplayBacktraceLine(size_t addr,const char* sym,size_t displacement);
};

class CStreamGraphExternalAllocations : public CStreamGraphExtensionBase
{
public:

	#define NUM_EXT_ALLOC_BUCKETS	(MEMBUCKET_DEBUG+1)

	CStreamGraphExternalAllocations() : CStreamGraphExtensionBase()
	{
		m_bActive = true;
	}


	void	Update();
	float	DrawMemoryBar(float x, float y);
	float	DrawLegend(float x, float y);
	void	DrawFileList(float x, float y);
	void	DrawFileInfo(float x, float y);

	void	SwapBuffers();

	void	AddWidgets(bkBank* pBank);

private:

	u32	GetExtAllocsInCurrentBucket(int bucket)
	{
		return m_Allocations[bucket].size();
	}

	strExternalAlloc *GetExtAlloc(int idx)
	{
		for(int i=0;i<NUM_EXT_ALLOC_BUCKETS;i++)
		{
			atArray<strExternalAlloc> &thisBucket = m_Allocations[i];
			if( idx < thisBucket.size() )
			{
				return &thisBucket[idx];
			}
			idx -= thisBucket.size();
		}
		return NULL;
	}

	static int	CompareExtAllocBySize(const strExternalAlloc* pA, const strExternalAlloc* pB);

	// NOTE: Special case, not double buffered
	s32 m_MaxSelection;
	s32 m_CurrentSelection;

	// Some storage to copy the current allocations into a local buffer
	atArray<strExternalAlloc> m_Allocations[NUM_EXT_ALLOC_BUCKETS];
};


class CStreamGraphAllocationsToResGameVirt : public CStreamGraphExtensionBase
{
public:

	typedef struct
	{
		int m_objIdx;
		int	m_size;
		int	m_Count;
	} ObjectAndSize;

	class ModuleAllocations
	{
	public:
		ModuleAllocations(): m_totalSize(0) {}
		atArray<ObjectAndSize> m_AllocationsByObject;
		int m_totalSize;
	};

	CStreamGraphAllocationsToResGameVirt() : CStreamGraphExtensionBase()
	{
		m_Allocations[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE].Resize(CStreamGraphBuffer::MAX_TRACKED_MODULES);

		// Do we need to do this if we intend to copy one from another?
//		m_AllocationSizes[1].Resize(CStreamGraphBuffer::MAX_TRACKED_MODULES);
	}

	virtual ~CStreamGraphAllocationsToResGameVirt();

	void	Update();
	float	DrawMemoryBar(float x, float y);
	float	DrawLegend(float x, float y);
	void	DrawFileList(float x, float y);
	void	DrawFileInfo(float x, float y);
	void	SwapBuffers();
	void	AddWidgets(bkBank* pBank);

	static int CompareObjectBySize(const ObjectAndSize* pA, const ObjectAndSize* pB);

	static void	PreChange(int moduleID, int objIndex, bool);
	static void	PostChange(int moduleID, int objIndex, bool);

	void	RecordAllocationChange(int moduleID, int objIdx, int size);

	atFixedArray<ModuleAllocations, CStreamGraphBuffer::MAX_TRACKED_MODULES>	&GetUpdateAllocations()
	{
		return m_Allocations[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE];
	}

	atFixedArray<ModuleAllocations, CStreamGraphBuffer::MAX_TRACKED_MODULES>	&GetRenderAllocations()
	{
		return m_Allocations[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER];
	}

	int		m_tempModuleID;
	int		m_preSize;

	atFixedArray<ModuleAllocations, CStreamGraphBuffer::MAX_TRACKED_MODULES> m_Allocations[SCENESTREAMINGMEMORYTRACKER_BUFFER_TOTAL];

	s32 m_MaxSelection;
	s32 m_CurrentSelection;

#define DEBUG_BUCKET_ALLOCATION_RECORDING (1)
#if DEBUG_BUCKET_ALLOCATION_RECORDING
	struct checkPacket
	{
		u32 size;
		u32 module;
		bool bReleased;
	};
	atMap<u32, checkPacket> m_instanceDebug;
#endif // DEBUG_BUCKET_ALLOCATION_RECORDING
};

extern	CStreamGraphAllocationsToResGameVirt *g_pResVirtAllocs;

class CStreamGraphExtensionManager
{
public:

	CStreamGraphExtensionManager();
	~CStreamGraphExtensionManager();

	void Update();			// Used for key presses check and compiling any data needed
	void SwapBuffers();		// Called from CStreamGraph::SwapBuffers
	void DebugDraw();
	void AddWidgets(bkBank* pBank);

	void AddExtension(CStreamGraphExtensionBase *pExtension);
	u32	GetNumExtensions()	{ return m_Extensions.size(); }
	CStreamGraphExtensionBase *GetExtension(u32 idx) { return m_Extensions[idx]; }

	bool IsSelected(CStreamGraphExtensionBase *pExt, int bufferID)
	{
		s32 CurrentSelected = m_CurrentSelected[bufferID];

		if(CurrentSelected >= 0 && CurrentSelected < m_Extensions.size() )
		{
			return m_Extensions[CurrentSelected] == pExt;
		}
		return false;
	}

	CStreamGraphExtensionBase *GetSelectedExt(int bufferID)
	{
		s32 CurrentSelected = m_CurrentSelected[bufferID];

		if( CurrentSelected >= 0 && CurrentSelected < m_Extensions.size() )
		{
			return m_Extensions[m_CurrentSelected[bufferID]];
		}
		return NULL;
	}

	// Get the number of extensions that are active
	u32	GetActiveCount()
	{
		u32 count = 0;
		for(int i=0;i<GetNumExtensions();i++)
		{
			CStreamGraphExtensionBase *pExt = m_Extensions[i];
			if( pExt->IsActive() )
			{
				count++;
			}
		}
		return count;
	}

	// Get the first 
	s32 GetFirstActive()
	{
		for(int i=0;i<GetNumExtensions();i++)
		{
			CStreamGraphExtensionBase *pExt = m_Extensions[i];
			if( pExt->IsActive() )
			{
				return i;
			}
		}
		return -1;
	}

	s32 GetLastActive()
	{
		for(int i=GetNumExtensions()-1;i>=0;i--)
		{
			CStreamGraphExtensionBase *pExt = m_Extensions[i];
			if( pExt->IsActive() )
			{
				return i;
			}
		}
		return -1;
	}

	s32 GetPrevActive()
	{
		for(int i=GetSelected()-1;i>=0;i--)
		{
			CStreamGraphExtensionBase *pExt = m_Extensions[i];
			if( pExt->IsActive() )
			{
				return i;
			}
		}
		return -1;
	}

	s32 GetNextActive()
	{
		for(int i=GetSelected()+1;i<GetNumExtensions();i++)
		{
			CStreamGraphExtensionBase *pExt = m_Extensions[i];
			if( pExt->IsActive() )
			{
				return i;
			}
		}
		return -1;
	}

	void SetSelected(s32 i)
	{
		m_CurrentSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE] = i;
	}

	s32 GetSelected()
	{
		return m_CurrentSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE];
	}

	// Set the active extension to be the first
	void SetFirstSelected()
	{
		SetSelected(GetFirstActive());
	}

	// Set the active extension to be the last
	void SetLastSelected()
	{
		SetSelected(GetLastActive());
	}

	// Set no active extension
	void SetNoneSelected()
	{
		SetSelected(-1);
	}

	bool	m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_TOTAL];

	// The ID of the currently selected extension bar (-1 none selected)
	s32		m_CurrentSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_TOTAL];

	CStreamGraphExternalAllocations		 *m_pExtAllocs;


	// Storage for the extensions
	atArray<CStreamGraphExtensionBase*> m_Extensions;
};



#endif //__BANK

#endif	//__STREAMING_DEBUG_GRAPH_EXTENSIONS_H