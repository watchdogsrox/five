#ifndef REPLAY_CHANNEL_H 
#define REPLAY_CHANNEL_H 


#include "system/memops.h"
#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY 

#include <stdio.h>

#if !RSG_ORBIS
static void	DumpReplayPackets(char*, unsigned int);
#endif

# if !DO_REPLAY_OUTPUT

#define replayAssert(cond)						do {} while(false)
#define replayAssertf(cond,fmt,...)				do {} while(false)
#define replayFatalAssertf(cond,fmt,...)		do {} while(false)
#define replayVerify(cond)						do {} while(false)
#define replayVerifyf(cond,fmt,...)				do {} while(false)
#define replayErrorf(fmt,...)					do {} while(false)
#define replayWarningf(fmt,...)					do {} while(false)
#define replayDisplayf(fmt,...)					do {} while(false)
#define replayDebugf1(fmt,...)					do {} while(false)
#define replayDebugf2(fmt,...)					do {} while(false)
#define replayDebugf3(fmt,...)					do {} while(false)
#define replayLogf(severity,fmt,...)			do {} while(false)

#define replayFatalDumpAssertf(cond,ptr,memSize,fmt,...)	do {} while(false)


#else
#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(replay)

#define replayAssert(cond)						RAGE_ASSERT(replay,cond)
#define replayAssertf(cond,fmt,...)				RAGE_ASSERTF(replay,cond,fmt,##__VA_ARGS__)
#define replayFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(replay,cond,fmt,##__VA_ARGS__)
#define replayVerify(cond)						RAGE_VERIFY(replay,cond)
#define replayVerifyf(cond,fmt,...)				RAGE_VERIFYF(replay,cond,fmt,##__VA_ARGS__)
#define replayErrorf(fmt,...)					RAGE_ERRORF(replay,fmt,##__VA_ARGS__)
#define replayWarningf(fmt,...)					RAGE_WARNINGF(replay,fmt,##__VA_ARGS__)
#define replayDisplayf(fmt,...)					RAGE_DISPLAYF(replay,fmt,##__VA_ARGS__)
#define replayDebugf1(fmt,...)					RAGE_DEBUGF1(replay,fmt,##__VA_ARGS__)
#define replayDebugf2(fmt,...)					RAGE_DEBUGF2(replay,fmt,##__VA_ARGS__)
#define replayDebugf3(fmt,...)					RAGE_DEBUGF3(replay,fmt,##__VA_ARGS__)
#define replayLogf(severity,fmt,...)			RAGE_LOGF(replay,severity,fmt,##__VA_ARGS__)

#define replayFatalDumpAssertf(cond,ptr,memSize,fmt,...)	if(cond == false)	DumpReplayPackets((char*)ptr, memSize);\
													RAGE_FATALASSERTF(replay,cond,fmt,##__VA_ARGS__)

#endif

static inline void PrintOutReplayData(char* position, const unsigned int numRows, const unsigned int numColumns)
{
	const int maxFoundPackets = 64;
	char* pFoundPacketIDs[maxFoundPackets] = {0};
	char* pFoundPacketSizes[maxFoundPackets] = {0};
	int foundPacketsCount = 0;
	const unsigned int maxColumns = 128;
	replayFatalAssertf(numColumns <= maxColumns, "Too many columns requested in PrintOutReplayData");
	char* pCurrPosition = position;
	for(int i = 0; i < numRows; ++i)
	{
		// columns
		// *4 - 3 for the raw bytes with space and 1 for the string representation
		// +19 - for the initial memory address printout
		// +1 - for the end null terminator
		char hexStr[maxColumns*4+19+1] = {0};
		sprintf(hexStr, "0x%p ", pCurrPosition);

		char* hexStrPtr = &hexStr[19];
		int j = 0;
		int baseGuardCount = 0;
		for(; j < numColumns; ++j)
		{
			char temp[9] = {0};
			char a = pCurrPosition[j];
			sprintf(temp, "%08x", a);
			hexStrPtr[(j*3)]	= temp[6];
			hexStrPtr[(j*3)+1] = temp[7];
			hexStrPtr[(j*3)+2] = ' ';

			const char aa = -86;//0xa;
			if(a == aa)//temp[6] == 0xa && temp[7] == 0xa)
			{
				++baseGuardCount;
				if(baseGuardCount == 4)
				{
					baseGuardCount = 0;
					if(&(pCurrPosition[j - 11]) >= position && foundPacketsCount < maxFoundPackets)
					{
						pFoundPacketIDs[foundPacketsCount] = &(pCurrPosition[j - 11]); // Offset from the end of the base guard to the packet ID...
						pFoundPacketSizes[foundPacketsCount] = &(pCurrPosition[j - 7]);
						++foundPacketsCount;
					}
				}
			}
			else
			{
				baseGuardCount = 0;
			}
		}
		sysMemCpy(&hexStrPtr[(j*3)], pCurrPosition, numColumns);

		for(int k = 0; k < numColumns; ++k)
		{
			// Fix up some ascii codes so we print period '.' instead of...
			char& ch = hexStrPtr[(j*3)+k];
			if(ch == 0x00 ||	// null
				ch == 0x0a ||	// new line
				ch == 0x0d ||	// carriage return
				ch == 0x09)		// tab
				ch = '.';
		}

		replayDebugf1("%s", hexStr);

		pCurrPosition += numColumns;
	}

	if(foundPacketsCount > 0)
	{
		replayDebugf1("Found packets:");
		for(int i = 0; i < foundPacketsCount; ++i)
		{
			char temp[128] = {0};
			u8 id = (u8)*(pFoundPacketIDs[i]);
			u32 size = (u32)*(u8*)(pFoundPacketSizes[i]);
			sprintf(temp, "0x%X - size %u", id, size);
			replayDebugf1("%s", temp);
		}

		if(foundPacketsCount == maxFoundPackets)
		{
			replayDebugf1("More packet exist...(truncated to %d)", maxFoundPackets);
		}
	}
}


static inline void	DumpReplayPackets(char* pPosition, unsigned int precedingMemorySize = 0)
{
	const unsigned int memorySize = precedingMemorySize;
	const unsigned int numColumns = 32;
	const unsigned int numRows = memorySize / numColumns;

	char* pStartPos = pPosition - memorySize;
	char* pCurrPosition = pStartPos;

	replayDebugf1("=================================================");
	replayDebugf1("Start of Replay Memory Dump");
	replayDebugf1("Dump Size: %d", memorySize);
	replayDebugf1("Columns : %d", numColumns);
	replayDebugf1("Dump Start Address: 0x%p", pStartPos);
	replayDebugf1("Offending Packet Address: 0x%p", pPosition);
	replayDebugf1("=================================================");
	if(precedingMemorySize > 0)
	{
		replayDebugf1("Preceding memory:");
		PrintOutReplayData(pCurrPosition, numRows, numColumns);
		replayDebugf1("");
	}
	replayDebugf1("Packet memory:");
	PrintOutReplayData(pPosition, 2, numColumns);
	replayDebugf1("=================================================");
	replayDebugf1("End of Replay Memory Dump");
	replayDebugf1("=================================================");
}

#endif // GTA_REPLAY

#endif // REPLAY_CHANNEL_H
