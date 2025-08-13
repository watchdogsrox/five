//
// core/memorytest.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#include "memorytest.h"

#if __XENON && __INCLUDE_MEMORY_TEST_IN_XENON_BUILD

#include "grcore/stateblock.h"
#include "system/xtl.h"
#include <xbdm.h>
#include "text/text.h"
#include "text/TextConversion.h"
#include "renderer/renderthread.h"

bool CMemoryTest::m_bActive = false;
bool CMemoryTest::m_bTextArrayInUse = false;
atArray<char *> CMemoryTest::m_TextLines;
atArray<Color32> CMemoryTest::m_TextCols;
s32 CMemoryTest::m_iNumOutputStrings = 0;
s32 CMemoryTest::m_iMaxNumOutputStrings = 0;

void CMemoryTest::FoundMemoryError(u32 * pMemAddr, u32 iDesiredVal, u32 iActualVal, int iFailedAfter)
{
	AddOutputString(Color_white, NULL);
	AddOutputString(Color_red, "MEMORY ERROR FOUND!");
	AddOutputString(Color_red, "Value at address 0x%x - should have been 0x%x, but was 0x%x", pMemAddr, iDesiredVal, iActualVal);
	AddOutputString(Color_white, NULL);
	AddOutputString(Color_red, "Failed after %i tests", iFailedAfter);
	AddOutputString(Color_white, NULL);
	AddOutputString(Color_white, "Press any button to reboot..\n");

	bool bButtonPressed = false;
	while(!bButtonPressed)
	{
		ioPad::UpdateAll();

		u32 iButtons0 = ioPad::GetPad(0).GetButtons();
		u32 iButtons1 = ioPad::GetPad(1).GetButtons();

		if(iButtons0 != 0 || iButtons1 != 0)
		{
			bButtonPressed = true;
		}
	}
}

bool CMemoryTest::TestMemory(u32 * pMem, int iNumBytes, u32 iExpectedVal, int iTestIteration)
{
	int iNumDwords = iNumBytes / sizeof(u32);
	for(int i=0; i<iNumDwords; i++)
	{
		if(pMem[i] != iExpectedVal)
		{
			FoundMemoryError(&pMem[i], iExpectedVal, pMem[i], iTestIteration);
			return false;
		}
	}
	return true;
}

void RebootXenon()
{
	gRenderThreadInterface.SetRenderFunction(NULL);
	XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0);
}

#define NUM_MEMORY_BLOCKS	((int)MEMTYPE_XENON_EXTERNAL)

void CMemoryTest::StartTest()
{
	ioPad::UpdateAll();
	u32 iButtons0 = ioPad::GetPad(0).GetButtons();
	u32 iButtons1 = ioPad::GetPad(1).GetButtons();
	if(((iButtons0 & ioPad::L1) && (iButtons0 & ioPad::R1) && (iButtons0 & ioPad::L2) && (iButtons0 & ioPad::R2)) ||
		((iButtons1 & ioPad::L1) && (iButtons1 & ioPad::R1) && (iButtons1 & ioPad::L2) && (iButtons1 & ioPad::R2)))
	{

	}
	else
	{
		return;
	}

	m_iMaxNumOutputStrings = (int) (1.0f / CText::GetCharacterHeight()) * 2;
	AllocOutputStrings();

	AddOutputString(Color_white, "Performing XBox360 Memory Test.");
	AddOutputString(Color_white, NULL);

	gRenderThreadInterface.SetRenderFunction(MakeFunctor(Render));
	// flush the render thread to ensure it picks up the new render function
	gRenderThreadInterface.Flush();
	m_bActive = true;

	void * pMemoryBlocks[NUM_MEMORY_BLOCKS];
	int iMemoryBlockSizes[NUM_MEMORY_BLOCKS];
	int iTotalMem = 0;

	const int iMemStepSize = 1024*1024;

	for(int h=0; h<NUM_MEMORY_BLOCKS; h++)
	{
		pMemoryBlocks[h] = NULL;
		iMemoryBlockSizes[h] = 0;

		sysMemAllocator * pAllocator = sysMemAllocator::GetMaster().GetAllocator(h);
		size_t iMemSize = pAllocator->GetLargestAvailableBlock();

		void * pMem = NULL;
		while(!pMem && iMemSize > 0)
		{
			pMem = pAllocator->Allocate(iMemSize, 16);
			if(pMem)
				break;
			iMemSize -= iMemStepSize;
		}

		pMemoryBlocks[h] = pMem;
		iMemoryBlockSizes[h] = iMemSize;
		iTotalMem += iMemSize;
	}

	AddOutputString(Color_white, "Allocated %i Mb in %i blocks.", iTotalMem / (1024*1024), NUM_MEMORY_BLOCKS);
	AddOutputString(Color_white, NULL);

	bool bRunTestContinuously = false;
	bool bQuit = false;
	while(!bQuit)
	{
		for(int b=0; b<NUM_MEMORY_BLOCKS; b++)
		{
			AddOutputString(Color_white, "Testing block %i", b);

			DWORD * pBuffer = (DWORD*)pMemoryBlocks[b];
			int iMemToAlloc = iMemoryBlockSizes[b];

			if(!pBuffer)
				continue;

			AddOutputString(Color_white, "0%%");

			for(unsigned int i=0; i<256; i++)
			{
				int iPercentComplete = (int) ((((float)i) / 256.0f) * 100.0f);
				ModifyOutputString(m_iNumOutputStrings-1, Color_white, "%i%%", iPercentComplete);

				unsigned int testValue = i;
				unsigned int testValue32 = (testValue << 24) | (testValue<<16) | (testValue<<8) | testValue;

				XMemSet(pBuffer, testValue, iMemToAlloc);

				if(!TestMemory((u32*)pBuffer, iMemToAlloc, testValue32, i))
				{
					m_bActive = false;

					for(int h=0; h<NUM_MEMORY_BLOCKS; h++)
						if(pMemoryBlocks[h])
							delete[] pMemoryBlocks[h];
					DeallocOutputStrings();
					// Now reboot the console.
					RebootXenon();
					return;
				}

				testValue = i ^ 0xff;
				testValue32 = (testValue << 24) | (testValue<<16) | (testValue<<8) | testValue;

				XMemSet(pBuffer, testValue, iMemToAlloc);

				if(!TestMemory((u32*)pBuffer, iMemToAlloc, testValue32, i))
				{
					m_bActive = false;

					for(int h=0; h<NUM_MEMORY_BLOCKS; h++)
						if(pMemoryBlocks[h])
							delete[] pMemoryBlocks[h];
					DeallocOutputStrings();
					// Now reboot the console.
					RebootXenon();
					return;
				}
			}

			ModifyOutputString(m_iNumOutputStrings-1, Color_white, "100%%");
		}

		//*********************************************************************
		//	Test has completed successfully.  Your 360 devkit is probably ok

		if(!bRunTestContinuously)
		{
			AddOutputString(Color_white, NULL);
			AddOutputString(Color_green, "Memory appears to be ok.");
			AddOutputString(Color_white, NULL);
			AddOutputString(Color_white, "Press \'A\' to run test continuously, or any other button to reboot..");

			bool bButtonPressed = false;
			u32 iButtons0 = 0;
			u32 iButtons1 = 0;

			while(!bButtonPressed)
			{
				ioPad::UpdateAll();
				iButtons0 = ioPad::GetPad(0).GetButtons();
				iButtons1 = ioPad::GetPad(1).GetButtons();

				if(iButtons0 || iButtons1)
				{
					bButtonPressed = true;
				}
			}

			if((iButtons0 & ioPad::RDOWN) || (iButtons1 & ioPad::RDOWN))
			{
				AddOutputString(Color_white, "Restarting the memory test.");
				AddOutputString(Color_white, NULL);

				bRunTestContinuously = true;
			}
			else
			{
				bQuit = true;
			}
		}

		ClearOutputStrings();
	}

	m_bActive = false;

	for(int h=0; h<NUM_MEMORY_BLOCKS; h++)
		if(pMemoryBlocks[h])
			delete[] pMemoryBlocks[h];
	DeallocOutputStrings();

	// Now reboot the console.
	RebootXenon();
}

void CMemoryTest::AllocOutputStrings()
{
	for(int i=0; i<m_iMaxNumOutputStrings; i++)
	{
		char * pLine = new char[256];
		pLine[0] = 0;

		if(m_TextLines.GetCount() < m_TextLines.GetCapacity())
		{
			m_TextLines.Append() = pLine;
			m_TextCols.Append() = Color_white;
		}
		else
		{
			m_TextLines.Grow() = pLine;
			m_TextCols.Grow() = Color_white;
		}
	}
}
void CMemoryTest::DeallocOutputStrings()
{
	for(int i=0; i<m_iMaxNumOutputStrings; i++)
	{
		char * pLine = m_TextLines[i];
		delete[] pLine;
	}

	m_TextLines.Reset();
	m_TextCols.Reset();
}

void CMemoryTest::AddOutputString(Color32 iCol, const char * fmt, ...)
{
	while(m_bTextArrayInUse)
	{
		sysIpcYield(1);
	}
	m_bTextArrayInUse = true;

	char debugMessage[512];

	if(fmt)
	{
		va_list argptr;
		va_start(argptr, fmt);
		vsprintf(debugMessage, fmt, argptr);
		va_end(argptr);
	}
	else
	{
		debugMessage[0] = 0;
	}

	// Shuffle all the lines up, if we've reached capacity?
	if(m_iNumOutputStrings+1 == m_iMaxNumOutputStrings)
	{
		for(int i=1; i<m_iMaxNumOutputStrings; i++)
		{
			char * pAboveLine = m_TextLines[i-1];
			char * pLine = m_TextLines[i];
			strcpy(pAboveLine, pLine);

			m_TextCols[i-1] = m_TextCols[i];
		}
	}

	char * pLine = m_TextLines[m_iNumOutputStrings];
	strcpy(pLine, debugMessage);
	m_TextCols[m_iNumOutputStrings] = iCol;

	if(m_iNumOutputStrings+1 < m_iMaxNumOutputStrings)
		m_iNumOutputStrings++;

	m_bTextArrayInUse = false;
}

void CMemoryTest::ModifyOutputString(const int iIndex, Color32 iCol, const char * fmt, ...)
{
	while(m_bTextArrayInUse)
	{
		sysIpcYield(1);
	}
	m_bTextArrayInUse = true;

	char debugMessage[512];

	if(fmt)
	{
		va_list argptr;
		va_start(argptr, fmt);
		vsprintf(debugMessage, fmt, argptr);
		va_end(argptr);
	}
	else
	{
		debugMessage[0] = 0;
	}

	Assert(iIndex < m_iNumOutputStrings);
	if(iIndex < m_iNumOutputStrings)
	{
		char * pString = m_TextLines[iIndex];
		strcpy(pString, debugMessage);
		m_TextCols[iIndex] = iCol;
	}

	m_bTextArrayInUse = false;
}

void CMemoryTest::ClearOutputStrings()
{
	for(int t=0; t<m_TextLines.GetCount(); t++)
	{
		char * pString = m_TextLines[t];
		pString[0] = 0;
		m_TextCols[t] = Color_white;
	}
	m_iNumOutputStrings = 0;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	MemTestRender
// PURPOSE:	method called by renderthread to produce output from our memory tests.
/////////////////////////////////////////////////////////////////////////////////////
void CMemoryTest::Render()
{
	if(!m_bActive)
		return;

	while(m_bTextArrayInUse) // Should really use a critical section, but can prob get away with this.
	{
		sysIpcYield(1);
	}
	m_bTextArrayInUse = true;

	// Set up stuff needed for rendering text, etc
	// I pinched most of this out of Derek's CLoadingScreens::Render() function.

	CSystem::BeginRender();
#if __PS3
	grcTextureFactory::GetInstance().BindDefaultRenderTargets();
#endif //__PS3

	CTextLayout MemoryTestText;

	MemoryTestText.SetOrientation(FONT_CENTRE);
	MemoryTestText.SetColor(CRGBA(225,225,225,255));
	Vector2 vCalabTextSize = Vector2(0.33f, 0.4785f);
	Vector2 vCalabTextWrap = Vector2(0.15f, 0.85f);

	MemoryTestText.SetWrap(&vCalabTextWrap);
	MemoryTestText.SetScale(&vCalabTextSize);

	const grcViewport* pVp = grcViewport::GetDefaultScreen();
	//float screenX = (float)pVp->GetWidth();
	//float screenY = (float)pVp->GetHeight();
	float fYStep = CText::GetCharacterHeight();


	GRCDEVICE.Clear(true, Color32(0,0,0), false, 0, false, 0);

	grcViewport::SetCurrent(pVp);

	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);
	grcLightState::SetEnabled(false);
	grcWorldIdentity();

	// Render all the lines of text we have.
	Vector2 vTextPos(0.5f,0.1f);

	for(int s=0; s<m_iNumOutputStrings; s++)
	{
		char * pString = m_TextLines[s];
		if(pString)
		{
			MemoryTestText.SetColor(m_TextCols[s]);
			MemoryTestText.Render(&vTextPos, pString);
		}

		vTextPos.y += fYStep;
	}

	CText::Flush();

	grcViewport::SetCurrent(NULL);
	CSystem::EndRender();

	m_bTextArrayInUse = false;
}


#endif