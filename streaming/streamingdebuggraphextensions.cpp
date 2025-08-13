

#include "bank/bank.h"
#include "bank/bkmgr.h"

#include "streamingdebuggraph.h"
#include "streamingdebuggraphextensions.h"
#include "streaming/streamingdebug.h"

#include "text/text.h"
#include "text/textconversion.h"
#include "fwdebug/debugdraw.h"
#include "grcore/viewport.h"
#include "fwmaths/Rect.h"

#include "vector/color32.h"
#include "vector/colors.h"
#include "renderer/sprite2d.h"

#include "system/controlMgr.h"

#if __BANK

STREAMING_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////////
//	CStreamGraphExtensionBase
/////////////////////////////////////////////////////////////////////////////////////


float CStreamGraphExtensionBase::backtraceX;
float CStreamGraphExtensionBase::backtraceY;
void CStreamGraphExtensionBase::DisplayBacktraceLine(size_t addr,const char* sym,size_t displacement)
{
	char achMsg[512];
	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	Color32 color = Color_white;
	sprintf(achMsg,"0x%" SIZETFMT "x - %s - %" SIZETFMT "u", addr, sym, displacement );
	DebugTextLayout.SetColor(color);
	DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(backtraceX, backtraceY), achMsg);
	backtraceY += 16.0f;
}

/////////////////////////////////////////////////////////////////////////////////////
//	CStreamGraphExternalAllocations
/////////////////////////////////////////////////////////////////////////////////////

void CStreamGraphExternalAllocations::Update()
{
	if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RIGHT, KEYBOARD_MODE_DEBUG, "move forward in file list") )
	{
		m_CurrentSelection++;
	}

	if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_LEFT, KEYBOARD_MODE_DEBUG, "move back in file list") )
	{
		m_CurrentSelection--;
	}

	m_CurrentSelection = Clamp( m_CurrentSelection, 0, m_MaxSelection-1 );
}

float CStreamGraphExternalAllocations::DrawMemoryBar(float x, float y)
{
	float heightOfBarDrawn = 0.0f;

	char achMsg[200];
	bool bDoneBackground = false;
	float curX = float(x + STREAMGRAPH_X_BORDER); 

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	DebugTextLayout.SetColor(Color32(255, 255, 255, 255));

	u32 totalBarVirtMemory = 0;
	u32 totalBarPhysMemory = 0;

	for ( s32 xAxisIdx = 0; xAxisIdx<NUM_EXT_ALLOC_BUCKETS; xAxisIdx++ )
	{
		atArray<strExternalAlloc> &thisBucket = m_Allocations[xAxisIdx];

		u32 memoryInBar = 0;

		for(int i=0;i<thisBucket.size();i++)
		{

			if ( DEBUGSTREAMGRAPH.m_ShowPhysicalMemory && thisBucket[i].IsVRAM() )
			{
				totalBarPhysMemory		+= thisBucket[i].m_Size;
				memoryInBar				+= thisBucket[i].m_Size;
			}
			if ( DEBUGSTREAMGRAPH.m_ShowVirtualMemory && thisBucket[i].IsMAIN() )
			{
				totalBarVirtMemory		+= thisBucket[i].m_Size;
				memoryInBar				+= thisBucket[i].m_Size;
			}
		}

		if ( !bDoneBackground )
		{
			bDoneBackground = true;
			DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
			DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y), "ExternalAllocations");

			DEBUGSTREAMGRAPH.DrawMemoryMarkers(x, y);
		}

		float barWidth = ( (float)(memoryInBar) / (float)( DEBUGSTREAMGRAPH.GetMaxMemMB() * 1024 * 1024 ) ) * DEBUGSTREAMGRAPH.GetGraphWidth();

		Color32 barColour1 = Color32(0,255,0,255);
		Color32 barColour2 = GetLegendColour( xAxisIdx ); 
		if( ( curX - x ) + barWidth > DEBUGSTREAMGRAPH.GetGraphWidth() )
		{
			barWidth = DEBUGSTREAMGRAPH.GetGraphWidth() - ( curX - x );
			barColour1.Set( 255, 0, 0, 255 ); 
			barColour2.Set( 255, 0, 0, 255 );
		}

		fwRect barRect1((float) curX,(float) y + STREAMGRAPH_BAR_SPACING(),								  (float) curX + barWidth,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()*(2.0f/3.0f) );
		fwRect barRect2((float) curX,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()*(2.0f/3.0f),(float) curX + barWidth,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT());

		CSprite2d::DrawRectSlow(barRect1, barColour1);
		CSprite2d::DrawRectSlow(barRect2, barColour2);

		curX += barWidth;
	}

	if ( totalBarVirtMemory+totalBarPhysMemory > 0 ) 
	{
		heightOfBarDrawn = STREAMGRAPH_BAR_SPACING();

#ifdef __PS3
		sprintf( achMsg, "%.3f MB (M), %.3f MB (V)", (float)totalBarVirtMemory / (1024.0f*1024.0f),  (float)totalBarPhysMemory / (1024.0f*1024.0f) );
#else
		sprintf( achMsg, "%.3f MB", (float)(totalBarVirtMemory+totalBarPhysMemory) / (1024.0f*1024.0f) );
#endif
		DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x + 150, y), achMsg);
	}

	if( DEBUGSTREAMGRAPH.m_pStreamGraphExtManager->GetSelectedExt( SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER ) == this )
	{
		DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x - 40, y), ">>");			

		// Draw in white the contribution of the currently selected asset
		u32  selectedSegmentMemorySize       = 0;
		u32  accumulatedMemoryBeforeSelected = 0;

		s32	count = 0;
		for(int i=0;i<NUM_EXT_ALLOC_BUCKETS;i++)
		{
			atArray<strExternalAlloc> &thisBucket = m_Allocations[i];
			for(int j=0;j<thisBucket.size();j++)
			{
				if ( DEBUGSTREAMGRAPH.m_ShowPhysicalMemory && thisBucket[j].IsVRAM() ) 
				{
					selectedSegmentMemorySize = thisBucket[j].m_Size;
				}
				if ( DEBUGSTREAMGRAPH.m_ShowVirtualMemory && thisBucket[j].IsMAIN() )
				{
					selectedSegmentMemorySize = thisBucket[j].m_Size;
				}

				if(count == m_CurrentSelection)
				{
					i = NUM_EXT_ALLOC_BUCKETS;
					break;
				}
				count++;

				if ( DEBUGSTREAMGRAPH.m_ShowPhysicalMemory && thisBucket[j].IsVRAM() ) 
				{
					accumulatedMemoryBeforeSelected += thisBucket[j].m_Size;
				}
				if ( DEBUGSTREAMGRAPH.m_ShowVirtualMemory && thisBucket[j].IsMAIN() )
				{
					accumulatedMemoryBeforeSelected += thisBucket[j].m_Size;
				}
			}
		}

		float  selectedFileSegmentXStart	= x + STREAMGRAPH_X_BORDER + ( (float)(accumulatedMemoryBeforeSelected)  / (float)( DEBUGSTREAMGRAPH.GetMaxMemMB() * 1024 * 1024 ) ) * DEBUGSTREAMGRAPH.GetGraphWidth();
		float  selectedFileSegmentXEnd		= selectedFileSegmentXStart + ( (float)(selectedSegmentMemorySize) / (float)( DEBUGSTREAMGRAPH.GetMaxMemMB() * 1024 * 1024 ) ) * DEBUGSTREAMGRAPH.GetGraphWidth();

		selectedFileSegmentXEnd = Max( selectedFileSegmentXStart + 1, selectedFileSegmentXEnd );

		fwRect selectedFileSegmentRect
			(
			(float) selectedFileSegmentXStart,					// left
			(float) y + STREAMGRAPH_BAR_SPACING(),						// bottom
			(float) selectedFileSegmentXEnd,					// right
			(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()	// top
			);

		Color32 selectedFileSegmentColor( 255, 255, 255, 255 );

		CSprite2d::DrawRectSlow( selectedFileSegmentRect, selectedFileSegmentColor );

	}

	return heightOfBarDrawn;
}

float CStreamGraphExternalAllocations::DrawLegend(float x, float y)
{
	const float fBoxW = 10.0f;

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	DebugTextLayout.SetColor(Color32(255, 255, 255, 255));

	y += (grcViewport::GetDefaultScreen()->GetHeight() < 1042 ? 0.0f : 16.0f);
	float yEnd = y;

	s32 numCategories = NUM_EXT_ALLOC_BUCKETS;

	for (u32 i=0; i<numCategories; i++)
	{
		Color32 boxColor  = GetLegendColour(i);
		const char* label = strStreamingEngine::GetAllocator().GetBucketName(i);	//   sBucketLegend[i].label;

		float fScreenX = (float)x;
		float fScreenY = y + i * 20.0f;

		fwRect boxRect(fScreenX,fScreenY + fBoxW,fScreenX + fBoxW,fScreenY);
		CSprite2d::DrawRectSlow(boxRect, boxColor);
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(fScreenX+10.0f, fScreenY-5.0f), label);
	}
	yEnd = y + (numCategories + 1) * 20;
	return yEnd;
}

void CStreamGraphExternalAllocations::DrawFileList(float x, float y)
{
	if(m_CurrentSelection < 0)
		return;

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));

	s32 NumListEntries = 10;
	// Incase the list can't be filled.
	NumListEntries = Clamp(NumListEntries, 0, m_MaxSelection);
	// Calc the offset into the list
	s32 ListOffset = m_CurrentSelection - (NumListEntries>>1);
	if( ListOffset >= (m_MaxSelection - NumListEntries) )
	{
		ListOffset = m_MaxSelection - (NumListEntries);
	}
	if( ListOffset < 0 )
	{
		ListOffset = 0;
	}

	const float headerSize = 16*STREAMGRAPH_FONTSCALE_FACTOR();

	float xStart= x;
	float xEnd = x + DEBUGSTREAMGRAPH.GetFileListWidth();
	float yStart = y + 5 + headerSize;
	float yEnd = yStart + NumListEntries * 16;
	float yLength = yEnd - yStart;
	float y0 = yStart + yLength * (float)ListOffset / (float)m_MaxSelection;
	float y1 = y0 + yLength * (float)NumListEntries / (float)m_MaxSelection;

	fwRect scrollBarThumb(xEnd - 14, y1, xEnd - 4, y0);
	CSprite2d::DrawRectSlow(scrollBarThumb, Color_white);
	fwRect scrollBarThumbInner(xEnd - 12, y1, xEnd - 6, y0);
	CSprite2d::DrawRectSlow(scrollBarThumbInner, Color_black);

	float trackLeft = xEnd - 11;
	float trackRight = xEnd - 7;

	u32 totalFiles = m_MaxSelection;
	u32 totalFilesSoFar = 0;
	s32 nBuckets = NUM_EXT_ALLOC_BUCKETS;

	for (u32 i=0; i<nBuckets; i++)
	{
		u32 numFiles = GetExtAllocsInCurrentBucket(i);

		float yStartFraction = (float)totalFilesSoFar / (float)totalFiles;
		float yEndFraction = (float)(totalFilesSoFar + numFiles) / (float)totalFiles;
		float yStartCategory = Lerp(yStartFraction, yStart, yEnd);
		float yEndCategory = Lerp(yEndFraction, yStart, yEnd);

		Color32 categoryColor  = GetLegendColour(i);
		fwRect scrollBarTrack(trackLeft,yEndCategory, trackRight, yStartCategory);
		CSprite2d::DrawRectSlow(scrollBarTrack, categoryColor);

		totalFilesSoFar += numFiles;
	}

	strExternalAlloc *pAlloc = GetExtAlloc(m_CurrentSelection);
	if(!pAlloc)
		return;

	// header
	Color32 color = Color_white;
	DebugTextLayout.SetColor(color);
	char fileListHeader[128];
	const char * barName = strStreamingEngine::GetAllocator().GetBucketName(pAlloc->m_BucketID);	//sBucketLegend[ pAlloc->m_BucketID ].label;
	snprintf(fileListHeader, 128, "%s: %i allocations out of %d", barName, GetExtAllocsInCurrentBucket(pAlloc->m_BucketID), m_MaxSelection);
	DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(xStart, yStart - headerSize), fileListHeader);

	fwRect headerUnderline(xStart,yStart - 2,xEnd - 4,yStart - 1);
	CSprite2d::DrawRectSlow(headerUnderline, Color_white);

	// list
	for(int i = 0; i < NumListEntries; ++i)
	{
		char buffer[128];
		Color32 color = i == (m_CurrentSelection - ListOffset) ? Color_green : Color_white;
		strExternalAlloc *pThisAlloc = GetExtAlloc(ListOffset + i);
		sprintf(buffer,"%p - Size = %d - %s",pThisAlloc->m_pMemory, pThisAlloc->m_Size, pThisAlloc->IsMAIN() ? "MAIN" : "VRAM" );
		DebugTextLayout.SetColor(color);
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(xStart, i * 16 + yStart), buffer);
	}
}

void CStreamGraphExternalAllocations::DrawFileInfo( float x, float y )
{
	if(m_CurrentSelection < 0)
		return;

	backtraceX = x;
	backtraceY = y;
	strExternalAlloc *pThisAlloc = GetExtAlloc(m_CurrentSelection);

	if(pThisAlloc)
	{
		sysStack::PrintRegisteredBacktrace(pThisAlloc->m_BackTraceHandle, DisplayBacktraceLine);
	}
}

// Sorts by size (largest first)
int CStreamGraphExternalAllocations::CompareExtAllocBySize(const strExternalAlloc* pA, const strExternalAlloc* pB)
{
	return pB->m_Size - pA->m_Size;
}

void CStreamGraphExternalAllocations::SwapBuffers()
{
	// Copy into the array array of external allocations
	// Clear out each bucket
	for(int i=0;i<NUM_EXT_ALLOC_BUCKETS;i++)
	{
		m_Allocations[i].Reset();
	}

	// Fill each bucket with appropriate data
	m_MaxSelection = 0;
	atArray<strExternalAlloc> &Allocs = strStreamingEngine::GetAllocator().m_ExtAllocTracker.m_Allocations;
	for(int i=0;i<Allocs.size();i++)
	{
		if( Allocs[i].IsMAIN() && !DEBUGSTREAMGRAPH.m_ShowVirtualMemory)
			continue;

		if( Allocs[i].IsVRAM() && !DEBUGSTREAMGRAPH.m_ShowPhysicalMemory)
			continue;

		m_Allocations[Allocs[i].m_BucketID].PushAndGrow(Allocs[i]);
		m_MaxSelection++;
	}

	// Now sort by size
	for(int i=0;i<NUM_EXT_ALLOC_BUCKETS;i++)
	{
		m_Allocations[i].QSort(0,-1,CompareExtAllocBySize);
	}

	// Now clamp to the max value.
	m_CurrentSelection = Clamp( m_CurrentSelection, 0, m_MaxSelection-1 );
}

void CStreamGraphExternalAllocations::AddWidgets(bkBank* pBank)
{
	pBank->AddToggle("Display External Allocations from Streaming Module", &m_bActive);
}




/////////////////////////////////////////////////////////////////////////////////////
//	CStreamGraphAllocationsToResGameVirt
/////////////////////////////////////////////////////////////////////////////////////

void	CStreamGraphAllocationsToResGameVirt::Update()
{
	if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RIGHT, KEYBOARD_MODE_DEBUG, "move forward in file list") )
	{
		m_CurrentSelection++;
	}

	if( CControlMgr::GetKeyboard().GetKeyJustDown(KEY_LEFT, KEYBOARD_MODE_DEBUG, "move back in file list") )
	{
		m_CurrentSelection--;
	}

	m_CurrentSelection = Clamp( m_CurrentSelection, 0, m_MaxSelection-1 );
}

CStreamGraphAllocationsToResGameVirt::~CStreamGraphAllocationsToResGameVirt()
{
	streamAssert(false);
}

float	CStreamGraphAllocationsToResGameVirt::DrawMemoryBar(float x, float y)
{
	float heightOfBarDrawn = 0.0f;

	char achMsg[200];
	bool bDoneBackground = false;
	float curX = float(x + STREAMGRAPH_X_BORDER); 

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	DebugTextLayout.SetColor(Color32(255, 255, 255, 255));

	u32 totalBarVirtMemory = 0;
	u32 totalBarPhysMemory = 0;

	atFixedArray<ModuleAllocations, CStreamGraphBuffer::MAX_TRACKED_MODULES> &Allocations = GetRenderAllocations();

	for ( s32 xAxisIdx = 0; xAxisIdx<Allocations.size(); xAxisIdx++ )
	{
		atArray<ObjectAndSize> &thisModule = Allocations[xAxisIdx].m_AllocationsByObject;

		u32 memoryInBar = 0;

		for(int i=0;i<thisModule.size();i++)
		{
			// Ensure this doesn't go negative (as there is a bug where sometimes they are)
			int size = thisModule[i].m_size;
			if(size < 0)
				size = 0;

			totalBarVirtMemory		+= size;
			memoryInBar				+= size;
		}

		if ( !bDoneBackground )
		{
			bDoneBackground = true;
			DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
			DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y), "AllocationsToGameResVirt");
			DEBUGSTREAMGRAPH.DrawMemoryMarkers(x, y);
		}

		float barWidth = ( (float)(memoryInBar) / (float)( DEBUGSTREAMGRAPH.GetMaxMemMB() * 1024 * 1024 ) ) * DEBUGSTREAMGRAPH.GetGraphWidth();

		Color32 barColour1 = Color32(0,255,0,255);
		Color32 barColour2 = GetLegendColour( xAxisIdx ); 
		if( ( curX - x ) + barWidth > DEBUGSTREAMGRAPH.GetGraphWidth() )
		{
			barWidth = DEBUGSTREAMGRAPH.GetGraphWidth() - ( curX - x );
			barColour1.Set( 255, 0, 0, 255 ); 
			barColour2.Set( 255, 0, 0, 255 );
		}

		fwRect barRect1((float) curX,(float) y + STREAMGRAPH_BAR_SPACING(),								  (float) curX + barWidth,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()*(2.0f/3.0f) );
		fwRect barRect2((float) curX,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()*(2.0f/3.0f),(float) curX + barWidth,(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT());

		CSprite2d::DrawRectSlow(barRect1, barColour1);
		CSprite2d::DrawRectSlow(barRect2, barColour2);

		curX += barWidth;
	}

	if ( totalBarVirtMemory+totalBarPhysMemory > 0 ) 
	{
		heightOfBarDrawn = STREAMGRAPH_BAR_SPACING();

#ifdef __PS3
		sprintf( achMsg, "%.3f MB (M), %.3f MB (V)", (float)totalBarVirtMemory / (1024.0f*1024.0f),  (float)totalBarPhysMemory / (1024.0f*1024.0f) );
#else
		sprintf( achMsg, "%.3f MB", (float)(totalBarVirtMemory+totalBarPhysMemory) / (1024.0f*1024.0f) );
#endif
		DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x + 150, y), achMsg);
	}

	if( DEBUGSTREAMGRAPH.m_pStreamGraphExtManager->GetSelectedExt( SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER ) == this )
	{
		DebugTextLayout.SetColor(Color32(255, 255, 255, 255));
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x - 40, y), ">>");			

		// Draw in white the contribution of the currently selected asset
		u32  selectedSegmentMemorySize       = 0;
		u32  accumulatedMemoryBeforeSelected = 0;

		s32	count = 0;
		for(int i=0;i<Allocations.size();i++)
		{
			ModuleAllocations &modAllocs = Allocations[i];

			selectedSegmentMemorySize = Max(0,modAllocs.m_totalSize,0,modAllocs.m_totalSize);

			if(count == m_CurrentSelection)
			{
				i = Allocations.size();
				break;
			}
			count++;
			accumulatedMemoryBeforeSelected += Max(0,modAllocs.m_totalSize,0,modAllocs.m_totalSize);
		}

		float  selectedFileSegmentXStart	= x + STREAMGRAPH_X_BORDER + ( (float)(accumulatedMemoryBeforeSelected)  / (float)( DEBUGSTREAMGRAPH.GetMaxMemMB() * 1024 * 1024 ) ) * DEBUGSTREAMGRAPH.GetGraphWidth();
		float  selectedFileSegmentXEnd		= selectedFileSegmentXStart + ( (float)(selectedSegmentMemorySize) / (float)( DEBUGSTREAMGRAPH.GetMaxMemMB() * 1024 * 1024 ) ) * DEBUGSTREAMGRAPH.GetGraphWidth();

		selectedFileSegmentXEnd = Max( selectedFileSegmentXStart + 1, selectedFileSegmentXEnd );

		fwRect selectedFileSegmentRect
			(
			(float) selectedFileSegmentXStart,					// left
			(float) y + STREAMGRAPH_BAR_SPACING(),						// bottom
			(float) selectedFileSegmentXEnd,					// right
			(float) y + STREAMGRAPH_BAR_SPACING() - STREAMGRAPH_BAR_HEIGHT()	// top
			);

		Color32 selectedFileSegmentColor( 255, 255, 255, 255 );

		CSprite2d::DrawRectSlow( selectedFileSegmentRect, selectedFileSegmentColor );
	}

	return heightOfBarDrawn;
}

float	CStreamGraphAllocationsToResGameVirt::DrawLegend(float x, float y)
{
	const float fBoxW = 10.0f;

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	DebugTextLayout.SetColor(Color32(255, 255, 255, 255));

	float yEnd = y;

	int maxModules = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();

	s32 numCategories = maxModules;

	for (u32 i=0; i<numCategories; i++)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(i);
		Color32 boxColor  = GetLegendColour(i);
		const char* label = pModule->GetModuleName();

		float fScreenX = (float)x;
		float fScreenY = y + i * 20.0f;

		fwRect boxRect(fScreenX,fScreenY + fBoxW,fScreenX + fBoxW,fScreenY);
		CSprite2d::DrawRectSlow(boxRect, boxColor);
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(fScreenX+10.0f, fScreenY-5.0f), label);
	}
	yEnd = y + (numCategories) * 20;
	return yEnd;
}

void	CStreamGraphAllocationsToResGameVirt::DrawFileList(float x, float y)
{
	if(m_CurrentSelection < 0)
		return;

	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));

	s32 NumListEntries = 6;
	// Incase the list can't be filled.
	NumListEntries = Clamp(NumListEntries, 0, m_MaxSelection);
	// Calc the offset into the list
	s32 ListOffset = m_CurrentSelection - (NumListEntries>>1);
	if( ListOffset >= (m_MaxSelection - NumListEntries) )
	{
		ListOffset = m_MaxSelection - (NumListEntries);
	}
	if( ListOffset < 0 )
	{
		ListOffset = 0;
	}

	const float headerSize = 16;

	float xStart= x;
	float xEnd = x + DEBUGSTREAMGRAPH.GetFileListWidth();
	float yStart = y + 5 + headerSize;
	float yEnd = yStart + NumListEntries * 16;
	float yLength = yEnd - yStart;
	float y0 = yStart + yLength * (float)ListOffset / (float)m_MaxSelection;
	float y1 = y0 + yLength * (float)NumListEntries / (float)m_MaxSelection;

	fwRect scrollBarThumb(xEnd - 14, y1, xEnd - 4, y0);
	CSprite2d::DrawRectSlow(scrollBarThumb, Color_white);
	fwRect scrollBarThumbInner(xEnd - 12, y1, xEnd - 6, y0);
	CSprite2d::DrawRectSlow(scrollBarThumbInner, Color_black);

	float trackLeft = xEnd - 11;
	float trackRight = xEnd - 7;

	u32 totalFiles = m_MaxSelection;
	u32 totalFilesSoFar = 0;
	s32 nBuckets = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();

	for (u32 i=0; i<nBuckets; i++)
	{
		u32 numFiles = 1;

		float yStartFraction = (float)totalFilesSoFar / (float)totalFiles;
		float yEndFraction = (float)(totalFilesSoFar + numFiles) / (float)totalFiles;
		float yStartCategory = Lerp(yStartFraction, yStart, yEnd);
		float yEndCategory = Lerp(yEndFraction, yStart, yEnd);

		Color32 categoryColor  = GetLegendColour(i);
		fwRect scrollBarTrack(trackLeft,yEndCategory, trackRight, yStartCategory);
		CSprite2d::DrawRectSlow(scrollBarTrack, categoryColor);

		totalFilesSoFar += numFiles;
	}

	// header
	Color32 color = Color_white;
	DebugTextLayout.SetColor(color);
	char fileListHeader[128];
	//const char * barName = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(m_CurrentSelection)->GetModuleName();
	snprintf(fileListHeader, 128, "Module:");
	DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(xStart, yStart - headerSize), fileListHeader);

	fwRect headerUnderline(xStart,yStart - 2,xEnd - 4,yStart - 1);
	CSprite2d::DrawRectSlow(headerUnderline, Color_white);

	// list
	for(int i = 0; i < NumListEntries; ++i)
	{
		char buffer[128];
		Color32 color = i == (m_CurrentSelection - ListOffset) ? Color_green : Color_white;

		const char * barName = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(ListOffset + i)->GetModuleName();
		sprintf(buffer,"%s",barName);
		DebugTextLayout.SetColor(color);
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(xStart, i * 16 + yStart), buffer);
	}
}



// Sorts by size (largest first)
int CStreamGraphAllocationsToResGameVirt::CompareObjectBySize(const ObjectAndSize* pA, const ObjectAndSize* pB)
{
	return pB->m_size - pA->m_size;
}

void	CStreamGraphAllocationsToResGameVirt::DrawFileInfo(float x, float y)
{
	if(m_CurrentSelection < 0)
		return;

	char buffer[200];
	CTextLayout DebugTextLayout;
	DebugTextLayout.SetScale(Vector2(0.3f, 0.3f));
	DebugTextLayout.SetColor(Color_white);

	int	numTopAllocs = 20;


	atFixedArray<ModuleAllocations, CStreamGraphBuffer::MAX_TRACKED_MODULES> &Allocations = GetRenderAllocations();
	ModuleAllocations &modAllocs = Allocations[m_CurrentSelection];
	modAllocs.m_AllocationsByObject.QSort(0,-1,CompareObjectBySize);

	strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(m_CurrentSelection);

	// Header
	sprintf(buffer,"Total Objects: %d = %d total (bytes)", modAllocs.m_AllocationsByObject.size(), modAllocs.m_totalSize );
	DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y), buffer);
	y+=18;

	if( modAllocs.m_AllocationsByObject.size() )
	{
		sprintf(buffer,"Top %d Allocations by size:", numTopAllocs );
		DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, y), buffer);
		y+=18;

		// Draw the top allocs
		for(int i=0;i<modAllocs.m_AllocationsByObject.size() && i<numTopAllocs;i++)
		{
			ObjectAndSize &objAndSize = modAllocs.m_AllocationsByObject[i];

			sprintf(buffer,"Object:- %s = %d instances = %d (bytes)", pModule->GetName(strLocalIndex(objAndSize.m_objIdx)), objAndSize.m_Count, objAndSize.m_size );
			DebugTextLayout.Render(STREAMGRAPH_SCREEN_COORDS(x, i * 16 + y), buffer);
		}
	}
}

void	CStreamGraphAllocationsToResGameVirt::SwapBuffers()
{
	USE_DEBUG_MEMORY();
	m_Allocations[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER].Reset();
	m_Allocations[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER] = m_Allocations[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE];

	m_MaxSelection = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();
}

void	CStreamGraphAllocationsToResGameVirt::AddWidgets(bkBank* pBank)
{
	pBank->AddToggle("Display Allocations To Game Resource Virtual by Module", &m_bActive);
}

struct sStreamGraphAllocationsWatermark 
{
	int		moduleId;
	size_t	size;
};

__THREAD sStreamGraphAllocationsWatermark m_ResourceMemoryUse[16];
__THREAD int m_ResourceMemoryUseHead = -1;


void	CStreamGraphAllocationsToResGameVirt::PreChange(int moduleID, int UNUSED_PARAM(objIndex), bool)
{
	if(CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		++m_ResourceMemoryUseHead;
		if(m_ResourceMemoryUseHead < NELEM(m_ResourceMemoryUse))
		{
			m_ResourceMemoryUse[m_ResourceMemoryUseHead].moduleId = moduleID;
			m_ResourceMemoryUse[m_ResourceMemoryUseHead].size = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed(MEMBUCKET_RESOURCE);

			/*
			if ( m_ResourceMemoryUseHead > 0 )
			{
				streamDisplayf("-=[ Recursion detected - IN @ %d ]=-", m_ResourceMemoryUseHead);
			}
		*/
		}
	}
}


void	CStreamGraphAllocationsToResGameVirt::PostChange(int moduleID, int objIndex, bool UNUSED_PARAM(isLoading))
{
	if(CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		if(m_ResourceMemoryUseHead < NELEM(m_ResourceMemoryUse))
		{
			size_t postSize = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed(MEMBUCKET_RESOURCE);
			int diff = ptrdiff_t_to_int(postSize - m_ResourceMemoryUse[m_ResourceMemoryUseHead].size);
			if( diff )
			{
				streamAssertf( moduleID == m_ResourceMemoryUse[m_ResourceMemoryUseHead].moduleId, "Difference in modules %p vs %p", 
					strStreamingEngine::GetInfo().GetModuleMgr().GetModule(m_ResourceMemoryUse[m_ResourceMemoryUseHead].moduleId), strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleID)  );

				/*
				strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleID);
				streamDisplayf("%s PostChange:: Module %s, objIndex %d = %s - Size diff = %d (%d - %d)", isLoading ? "Loading":"Removing", pModule->GetModuleName(), objIndex,
							pModule->GetName(objIndex), diff, postSize,m_ResourceMemoryUse[m_ResourceMemoryUseHead].size );
				*/

				//streamAssert( (diff >= 0 && isLoading) || (diff <= 0 && !isLoading) );
				if(g_pResVirtAllocs)
					g_pResVirtAllocs->RecordAllocationChange(moduleID, objIndex, diff);
			}

			int prevHead = m_ResourceMemoryUseHead-1;
			if ( prevHead >= 0)
			{
	//			streamDisplayf("-=[ Recursion detected - OUT - adding %d to %d ]=-", diff,m_ResourceMemoryUse[prevHead].size );
				m_ResourceMemoryUse[prevHead].size += diff;
			}
		}

		--m_ResourceMemoryUseHead;
		streamAssert( m_ResourceMemoryUseHead >= -1 );
	}
}

void	CStreamGraphAllocationsToResGameVirt::RecordAllocationChange(int moduleID, int objIndex, int size)
{
	USE_DEBUG_MEMORY();

	atFixedArray<ModuleAllocations, CStreamGraphBuffer::MAX_TRACKED_MODULES> &updateSizes = GetUpdateAllocations();

	// Update
	bool found = false;
	ModuleAllocations &ModuleAllocs = updateSizes[moduleID];
	for(int i=0;i<ModuleAllocs.m_AllocationsByObject.size();i++)
	{
		ObjectAndSize &ObjSize = ModuleAllocs.m_AllocationsByObject[i];
		if( ObjSize.m_objIdx == objIndex )
		{
			ObjSize.m_size += size;
			ObjSize.m_Count++;
			found = true;
			break;
		}
	}
	// Didn't find a record, add a new one
	if(!found)
	{
		ObjectAndSize thisObject;
		thisObject.m_objIdx = objIndex;
		thisObject.m_size = size;
		thisObject.m_Count = 1;
		updateSizes[moduleID].m_AllocationsByObject.Grow(128) = thisObject;
	}

	updateSizes[moduleID].m_totalSize += size;

	//streamAssertf(updateSizes[moduleID].m_totalSize >=0, "ERROR: module allocations to the resource bucket have gone -ve!");
}

CStreamGraphAllocationsToResGameVirt *g_pResVirtAllocs = NULL;

/////////////////////////////////////////////////////////////////////////////////////
//	CStreamGraphExtensionManager
/////////////////////////////////////////////////////////////////////////////////////

CStreamGraphExtensionManager::CStreamGraphExtensionManager()
{
	m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE] = false;
	m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER] = false;

	m_CurrentSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE] = -1;
	m_CurrentSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER] = -1;

	// Create and add extensions
	m_pExtAllocs = rage_new CStreamGraphExternalAllocations;
	g_pResVirtAllocs = rage_new CStreamGraphAllocationsToResGameVirt;	// Needs to be a global pointer as it's referenced externally

	// Add the extensions
	AddExtension(m_pExtAllocs);
	AddExtension(g_pResVirtAllocs);
}

CStreamGraphExtensionManager::~CStreamGraphExtensionManager()
{
	delete m_pExtAllocs;
	delete g_pResVirtAllocs;
	g_pResVirtAllocs = NULL;
}


// Keys left and right
void CStreamGraphExtensionManager::Update()
{
	// Keep in a loop incase we want to update unselected, but active bars in the future.
	for(int i=0;i<m_Extensions.size();i++)
	{
		CStreamGraphExtensionBase *pExtension = m_Extensions[i];

		if(pExtension->IsActive() && IsSelected( pExtension, SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE ))
		{
			pExtension->Update();
		}
	}

}

void CStreamGraphExtensionManager::SwapBuffers()
{

	m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER] = m_bExtensionSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE];
	m_CurrentSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_RENDER] = m_CurrentSelected[SCENESTREAMINGMEMORYTRACKER_BUFFER_UPDATE];

	for(int i=0;i<m_Extensions.size();i++)
	{
		CStreamGraphExtensionBase *pExtension = m_Extensions[i];
		if(pExtension->IsActive())
		{
			pExtension->SwapBuffers();
		}
	}
}

// Probably won't be used
void CStreamGraphExtensionManager::DebugDraw()
{
}

void CStreamGraphExtensionManager::AddWidgets(bkBank* pBank)
{
	pBank->AddTitle("Extensions");
	for(int i=0;i<m_Extensions.size();i++)
	{
		CStreamGraphExtensionBase *pExtension = m_Extensions[i];
		pExtension->AddWidgets(pBank);
	}
}

void CStreamGraphExtensionManager::AddExtension(CStreamGraphExtensionBase *pExtension)
{
	m_Extensions.PushAndGrow(pExtension);
}

//CStreamGraphExtensionManager g_StreamGraphExtManager;

/////////////////////////////////////////////////////////////////////////////////////


#endif //__BANK
