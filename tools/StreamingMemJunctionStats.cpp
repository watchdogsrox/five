//
// filename:	StreamingMemJunctionStats.cpp
// author:		Derek Ward <derek.ward@rockstarnorth.com>
// date:		01 Dec 2011
// description:	Streaming memory junction statistics generation.
//

// --- Include Files ------------------------------------------------------------

#include "StreamingMemJunctionStats.h"

// Rage headers
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "bank/slider.h"
#include "atl/array.h"
#include "file/stream.h"
#include "grcore/debugdraw.h"
#include "fwscene/stores/staticboundsstore.h"

// Game headers
#include "debug/debug.h"
#include "Camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "game/Clock.h"
#include "game/Weather.h"
#include "fwrenderer/renderthread.h"
#include "Objects/DummyObject.h"
#include "Objects/object.h"
#include "pathserver/PathServer.h"
#include "Peds/Ped.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/water.h"
#include "scene/AnimatedBuilding.h"
#include "scene/Building.h"
#include "scene/scene.h"
#include "scene/debug/SceneCostTracker.h"
#include "scene/portals/InteriorInst.h"
#include "scene/portals/PortalInst.h"
#include "scene/world/gameWorld.h"
#include "streaming/defragmentation.h"
#include "streaming/packfilemanager.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingmodule.h"


// Platform SDK headers
#if __XENON
#include "system/xtl.h"
#elif __PPU
#include <cell/rtc.h>
#endif // Platform SDK headers

#if SECTOR_TOOLS_EXPORT

// --- Defines ------------------------------------------------------------------

#define _STATSROOT_NETWORK		"K:\\streamGTA5\\stats\\memjunctionstats"
#define DEFAULT_OUTPUT_FILENAME "StreamingMemJunctionStats.csv"
// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

bool		CStreamingMemJunctionStats::m_bEnabled = true;
CStreamingMemJunction* CStreamingMemJunctionStats::m_junctions = NULL;
s32			CStreamingMemJunctionStats::m_junctionId = -1;
s32			CStreamingMemJunctionStats::m_numJunctions = 0;
float		CStreamingMemJunctionStats::m_textSize = 1.5f;									// size/scale of text on screen
Vector2		CStreamingMemJunctionStats::m_textPos	= Vector2(0.05f,0.05f);					// where to print text on screen
Color32		CStreamingMemJunctionStats::m_textColour= Color32(1.0f, 0.0f, 0.0f);			// colour of text on screen
Color32		CStreamingMemJunctionStats::m_textColourCapturing= Color32(1.0f, 1.0f, 0.0f);			// colour of text on screen
Color32		CStreamingMemJunctionStats::m_textColourCaptured= Color32(0.0f, 1.0f, 0.0f);			// colour of text on screen
bool		CStreamingMemJunctionStats::m_textBackground = true;									// draw a background behind the text
bool		CStreamingMemJunctionStats::m_capturing = false;
bool		CStreamingMemJunctionStats::m_captured = false;
bool		CStreamingMemJunctionStats::m_respawn = false;
s32			CStreamingMemJunctionStats::m_respawns = 0;
s32			CStreamingMemJunctionStats::m_maxRespawns = 100;
float		CStreamingMemJunctionStats::m_camHeading = 0.0f; 
float		CStreamingMemJunctionStats::m_camHeadingSpeed=0.05f;
eJunctionMemStatState CStreamingMemJunctionStats::m_state = E_JUNC_MEM_STATE_STARTUP;
float		CStreamingMemJunctionStats::m_numSettleSpins=1.0f;
float		CStreamingMemJunctionStats::m_numStartupSpins=1.0f;
char		CStreamingMemJunctionStats::m_junctionsSrcFile[MAX_PATH] = "common:/DATA/junctions.CSV";
char		CStreamingMemJunctionStats::m_junctionsDstFile[MAX_PATH] = "";
s32			CStreamingMemJunctionStats::m_junctionIdEnd=-1;
eCaptureType CStreamingMemJunctionStats::m_captureType=E_CAPTURE_TYPE_SPIN;
float		CStreamingMemJunctionStats::m_timePreCaptureStarted = 0.0f;
float		CStreamingMemJunctionStats::m_preCaptureDelay = 5.0f;
sysTimer	CStreamingMemJunctionStats::m_timer;
bool		CStreamingMemJunctionStats::m_setTimeEachFrame = false;
u32			CStreamingMemJunctionStats::m_hourOfDay = 12;

// --- Code ---------------------------------------------------------------------

void CStreamingMemJunctionStats::Init( )
{	
	if ( PARAM_runjuncmem.IsInitialized() && PARAM_runjuncmem.Get( ) )
	{
		SetDefaultCaptureDstFilename();
		HandleCommandLineArgs();
		LoadJunctions();
		SetWeatherAndTime();	
		PreReadDstFile();
	}
}

void CStreamingMemJunctionStats::HandleCommandLineArgs()
{
	HandleCaptureRange();
	HandleSrcDstFiles();
	HandleCaptureType();
	HandleSpins();
	HandleSetTimeEachFrame();
	HandlePreCaptureDelay();
	HandleSpinRate();
}

void  CStreamingMemJunctionStats::HandleSetTimeEachFrame()
{
	if ( PARAM_juncsettimeeachframe.IsInitialized() && PARAM_juncsettimeeachframe.Get( ) )
		m_setTimeEachFrame = true;
}

void  CStreamingMemJunctionStats::HandlePreCaptureDelay()
{
	float val;
	if ( PARAM_juncprecapdelay.IsInitialized() && PARAM_juncprecapdelay.Get( val ) )
		m_preCaptureDelay = val;
}

void  CStreamingMemJunctionStats::HandleSpinRate()
{
	float val;
	if ( PARAM_juncspinrate.IsInitialized() && PARAM_juncspinrate.Get( val ) )
		m_camHeadingSpeed = val;
}

void  CStreamingMemJunctionStats::HandleSpins()
{
	int numSpins = 1;
	if ( PARAM_juncstartupspin.IsInitialized() && PARAM_juncstartupspin.Get( numSpins ) )
		m_numStartupSpins = (float)numSpins;

	if ( PARAM_juncsettlespin.IsInitialized() && PARAM_juncsettlespin.Get( numSpins ) )
		m_numSettleSpins = (float)numSpins;
}

void  CStreamingMemJunctionStats::SetDefaultCaptureDstFilename()
{
	strcpy(m_junctionsDstFile,_STATSROOT_NETWORK);
	strcat(m_junctionsDstFile,"\\");
	strcat(m_junctionsDstFile,DEFAULT_OUTPUT_FILENAME);
	//sprintf(m_junctionsDstFile, "%s\\%s" STATSROOT_NETWORK,DEFAULT_OUTPUT_FILENAME); Wow this is bugged in ps3!
}

void  CStreamingMemJunctionStats::HandleCaptureType()
{
	const char* str = NULL;
	if ( PARAM_junccap.IsInitialized() && PARAM_junccap.Get( str ) )
	{
		if (!strcmp(str,"spin"))
			m_captureType = E_CAPTURE_TYPE_SPIN;
		else if (!strcmp(str,"simple"))
			m_captureType = E_CAPTURE_TYPE_SIMPLE;
		else if (!strcmp(str,"new"))
			m_captureType = E_CAPTURE_TYPE_NEW;
	}
}

void  CStreamingMemJunctionStats::HandleSrcDstFiles()
{
	const char* str = NULL;
	if ( PARAM_juncsrc.IsInitialized() && PARAM_juncsrc.Get( str ) )
		strcpy(m_junctionsSrcFile,str);

	if ( PARAM_juncdst.IsInitialized() && PARAM_juncdst.Get( str ))
		strcpy(m_junctionsDstFile,str);
}

void  CStreamingMemJunctionStats::HandleCaptureRange()
{
	if ( PARAM_juncstart.IsInitialized() && PARAM_juncstart.Get( m_junctionId ))
		m_junctionId-=2;

	if ( PARAM_juncend.IsInitialized() && PARAM_juncend.Get( m_junctionIdEnd ))
		m_junctionIdEnd-=1;
}

void CStreamingMemJunctionStats::Shutdown( )
{
	if (m_junctions)
	{
		delete m_junctions;
		m_junctions = NULL;
	}
}

void CStreamingMemJunctionStats::InvalidateJunction(const char *name)
{
	for (int i=0;i<m_numJunctions;i++)
	{
		if (!strcmp(m_junctions[i].GetName(),name))
		{
			Displayf("Invalidating %s",m_junctions[i].GetName());
			m_junctions[i].SetValid(false);
			break;
		}
	}
}

void CStreamingMemJunctionStats::PreReadDstFile()
{
	char* pLine;
	FileHandle fid = CFileMgr::OpenFile(m_junctionsDstFile, "rb");
	if (!CFileMgr::IsValidFileHandle(fid))
	{
		return; // doesn't have to exist
	}

	while( (pLine = CFileMgr::ReadLine(fid,false)) != NULL  )
	{
		Displayf("Line : %s",pLine);
		char name[MAX_JUNCTION_NAME_LENGTH];
		int numFound = sscanf(pLine, "%[^,]", &name[0]);
		if (numFound == 1 && strcmp(name,"name"))
		{
			InvalidateJunction(name);
		}
	}
}

void CStreamingMemJunctionStats::LoadJunctions( )
{
	FileHandle fid;
	char* pLine;

	CFileMgr::SetDir("");

	for (int pass=0;pass<2;pass++) // god I am lazy today
	{
		int numJunctionsRead = 0;
		fid = CFileMgr::OpenFile(m_junctionsSrcFile, "rb");

		if (!CFileMgr::IsValidFileHandle(fid))
		{
			Assertf(false,"Error: Could not open %s", m_junctionsSrcFile);
			return;
		}

		m_numJunctions = 0;		
		while( (pLine = CFileMgr::ReadLine(fid, false)) != NULL  )
		{
			CStreamingMemJunction junction;
			char name[MAX_JUNCTION_NAME_LENGTH],x[30],y[30],z[30];
			int numFound = sscanf(pLine, "%[^,],%[^,],%[^,],%[^,]", &name[0],&x[0],&y[0],&z[0]);
			if (numFound == 4)
			{
				if (strcmp(name,"name")) // skip hdr line
				{
					junction.SetName(name);
					junction.SetPos(Vector3((float)atof(x),(float)atof(y),(float)atof(z)));		
					junction.SetValid(true);
					
					if (pass==1)
					{
						Displayf("CStreamingMemJunctionStats Junction : %s (%f %f %f)",junction.GetName(),junction.GetPos().x,junction.GetPos().y,junction.GetPos().z);
						m_junctions[numJunctionsRead] = junction;
					}		
					numJunctionsRead++;
				}
			}
			else
			{
				Assertf(false,"Error: Bad line in %s : %s",m_junctionsSrcFile,pLine);
			}
		}
		CFileMgr::CloseFile(fid);
		
		m_numJunctions = numJunctionsRead;

		if (pass==0)
		{
			m_junctions = rage_new CStreamingMemJunction[m_numJunctions];
		}
	}
}


void CStreamingMemJunctionStats::SetWeatherAndTime()
{
	g_weather.ForceTypeClear();
	if (m_setTimeEachFrame)
	{
		CClock::SetTime(m_hourOfDay, 0, 0);// at present causes audRadioSlot::SkipForward to crash
	}
}

const char* CStreamingMemJunctionStats::StateString(char *str)
{
	switch(m_state)
	{
		case E_JUNC_MEM_STATE_INVALID :
			return "INVALID";
		case E_JUNC_MEM_STATE_STARTUP:
			return "STARTUP";
		case E_JUNC_MEM_STATE_NEXT_JUNCTION:
			return "NEXT JUNCTION";
		case E_JUNC_MEM_STATE_SPAWN:
			if (m_respawns==0)
				return "SPAWN";
			else
			{
				sprintf(str,"SPAWN (%d respawns)",m_respawns);
				return str;
			}
		case E_JUNC_MEM_STATE_SPIN1:
			return "SPIN1";
		case E_JUNC_MEM_STATE_STOP_STREAMING:			
			return "STOP STREAMING";
		case E_JUNC_MEM_STATE_SPIN2:
			return "SPIN2";
		case E_JUNC_MEM_STATE_CAPTURE_START:
			return "CAPTURE START";
		case E_JUNC_MEM_STATE_PRE_CAPTURE_DELAY:
			sprintf(str,"PRECAPTURE DELAY %5.2f",GetCaptureDelayRemainingTime());
			return str;
		case E_JUNC_MEM_STATE_CAPTURE:
			return "CAPTURING";
		case E_JUNC_MEM_STATE_CAPTURE_FINISH:
			return "FINISH";
		case E_JUNC_MEM_STATE_WAIT:			
			return "WAIT";
		case E_JUNC_MEM_STATE_FINISHED:
			return "FINISHED";
		default:
			return "UNKNOWN";
	}
}

Color32 CStreamingMemJunctionStats::StateColour()
{
	switch(m_state)
	{
		case E_JUNC_MEM_STATE_CAPTURE:
			return m_textColourCapturing;
		case E_JUNC_MEM_STATE_CAPTURE_FINISH:
			return m_textColourCaptured;
		case E_JUNC_MEM_STATE_FINISHED:
			return m_textColourCaptured;
		default:
			return m_textColour;
	}
}

void CStreamingMemJunctionStats::DisplayDebug()
{
#if DEBUG_DRAW
	char str[255] = "",str2[255] = "",str3[255] = "";

	if (m_junctions != NULL && m_junctionId>=0)
	{
		CStreamingMemJunction *pJunction = &m_junctions[m_junctionId];
		sprintf(str,"Junction %s [%d of %d] [%8.0f,%8.0f,%8.0f]",pJunction->GetName(),m_junctionId+1,m_numJunctions,pJunction->GetPos().x,pJunction->GetPos().y,pJunction->GetPos().z );
	}

	Color32 textColour = StateColour();
	const char *pStateDesc = StateString(str3);
	sprintf(str2,"%s %s", str2, pStateDesc); 

	grcDebugDraw::Text(m_textPos,m_textColour, str, false,m_textSize,m_textSize);
	Vector2 textPos = m_textPos;
	textPos.y *= 2.0f;
	grcDebugDraw::Text(textPos,textColour, str2, false,m_textSize,m_textSize);
#endif
}

void CStreamingMemJunctionStats::Finish()
{
	m_junctionId = -1;
//m_bEnabled = false;
}

void CStreamingMemJunctionStats::Spawn()
{
	m_captured = false;	
	CPed* pPlayer = FindPlayerPed( );
	if (pPlayer)
	{
		Vector3 spawnPos = m_junctions[m_junctionId].GetPos();

		pPlayer->Teleport( spawnPos, pPlayer->GetCurrentHeading() );
		if (m_respawns>m_maxRespawns)
		{
			m_respawn = false;
			return; // there was no collision found here - we have given up but on spawning here we need to skip the collision tests now.
		}
		CStreaming::LoadAllRequestedObjects( );		

		WorldProbe::CShapeTestFixedResults<> probeResult;
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetStartAndEnd(spawnPos, Vector3(spawnPos.x, spawnPos.y, spawnPos.z - 1000.0f));
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetExcludeEntity(NULL);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		probeDesc.SetContext(WorldProbe::LOS_Unspecified);

		bool collided = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);

		if (collided)
		{
			spawnPos = probeResult[0].GetHitPosition( );
			spawnPos.z += 0.5f;
			pPlayer->Teleport( spawnPos, pPlayer->GetCurrentHeading() );
			m_respawn = false;
		}

		// Couldn't get to a junction 
		if (m_respawns>m_maxRespawns)
		{
			Warningf("Warning: Junction %s %f %f %f - no collision found",m_junctions[m_junctionId].GetName(),m_junctions[m_junctionId].GetPos().x, m_junctions[m_junctionId].GetPos().y,m_junctions[m_junctionId].GetPos().z);
			//m_state=E_JUNC_MEM_STATE_NEXT_JUNCTION;
			m_respawn = false;			
			return;
		}

		if (collided)
		{		
			m_respawns = 0;
		}
		else
		{
			m_respawn = true;
			m_respawns++;
		}			
	}		
}

void CStreamingMemJunctionStats::StartCapture()
{
	m_captured = false;
	m_capturing = true;
	SetWeatherAndTime();
}

void CStreamingMemJunctionStats::Capture()
{
#if __BANK
	CJunctionTestResults junctionTestResults;
	junctionTestResults.Init();

	junctionTestResults.m_numArchetypes = (s32) fwArchetypeManager::GetPool().GetCount();

#if 0
	// disable this until tty-spew-causing memory issues are resolved
	sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);
	if (pAllocator)
	{
		for (s32 i = 0; i < (s32) fwArchetypeManager::GetPool().GetSize(); ++i)
		{
			fwArchetype** ppArchetype = fwArchetypeManager::GetPool().GetSlot(i);	
			if (ppArchetype && *ppArchetype)
			{
				if (pAllocator->IsValidPointer(*ppArchetype))
					junctionTestResults.m_archetypeMem += (s32) pAllocator->GetSizeWithOverhead(*ppArchetype);
			}
		}
	}
#endif

	if (m_captureType == E_CAPTURE_TYPE_SIMPLE)
	{
		// this is all bullshit. remove asap
		char reading1[200],reading2[200];
	
		junctionTestResults.m_overMem_Virtual = g_SceneCostTracker.GetCostTitleStr(reading2, "MAIN (Total):", SCENECOSTTRACKER_COST_LIST_TOTAL, SCENECOSTTRACKER_MEMTYPE_VIRT);
		junctionTestResults.m_overMem_Physical = g_SceneCostTracker.GetCostTitleStr(reading1, "VRAM (Total):", SCENECOSTTRACKER_COST_LIST_TOTAL, SCENECOSTTRACKER_MEMTYPE_PHYS);

	}
	else if (m_captureType == E_CAPTURE_TYPE_NEW)
	{
		//
		// new OVER test
		//

		Assertf(g_SceneMemoryCheck.IsFinished(), "Checking results before they are ready!");
		const CSceneMemoryCheckResult& results = g_SceneMemoryCheck.GetResult();
		
		junctionTestResults.Set(results);

		//////////////////////////////////////////////////////////////////////////
		// write out scene cost tracker log if required
		if (PARAM_junccosts.Get())
		{
			const char* logFileLocation = 0;
			PARAM_junccosts.Get(logFileLocation);

			char achTmp[RAGE_MAX_PATH];
			sprintf(achTmp, "%s\\%s.txt", logFileLocation, m_junctions[m_junctionId].GetName());

			Displayf("Writing scene costs log %s", achTmp);
			g_SceneCostTracker.WriteToLog(achTmp);
		}
		//////////////////////////////////////////////////////////////////////////
	}
	else
	{
		s32 virtualMem = 0;
		s32 physicalMem = 0;
		CStreamingDebug::CalculateRequestedObjectSizes(virtualMem, physicalMem);
		virtualMem >>= 10;
		physicalMem >>= 10;

		junctionTestResults.m_overMem_Virtual = virtualMem;
		junctionTestResults.m_overMem_Physical = physicalMem;
	}

	StoreCapture(junctionTestResults);
	m_captured = true;
#endif // _BANK
}

void CStreamingMemJunctionStats::WriteHeader()
{
	FileHandle fi = CFileMgr::OpenFileForWriting(m_junctionsDstFile);

	if(fi != INVALID_FILE_HANDLE)
	{
		char entry[1000];
		if (m_captureType == E_CAPTURE_TYPE_SPIN)
		{
			sprintf(entry,"name,x,y,z,physical,virtual,arch_num,arch_mem\r\n");
		}
		else
		{
			sprintf(entry,"name,x,y,z,vram,main,arch_num,arch_mem,vram_avail,main_avail,vram_scene,main_scene,vram_used,main_used\r\n");
		}			
		Displayf("Writing %s to %s", entry, m_junctionsDstFile);
		CFileMgr::Write(fi, entry, istrlen(entry));
		CFileMgr::CloseFile(fi);
	}
	else
	{
		Assertf(false,"Error: Could not open filename %s for WriteHeader.",m_junctionsDstFile);
	}
}

#if __BANK
void CStreamingMemJunctionStats::StoreCapture(CJunctionTestResults& results)
{
	FileHandle fi = INVALID_FILE_HANDLE;
	fi = CFileMgr::OpenFileForAppending(m_junctionsDstFile);

	if (fi!=INVALID_FILE_HANDLE && CFileMgr::GetTotalSize(fi)<=0)
	{
		WriteHeader();
	}

	if(fi == INVALID_FILE_HANDLE)
	{
		WriteHeader();
		fi = CFileMgr::OpenFileForAppending(m_junctionsDstFile);
	}

	if(fi != INVALID_FILE_HANDLE)
	{
		char entry[1000];
		sprintf(entry,"%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",

			m_junctions[m_junctionId].GetName(),

			(int)m_junctions[m_junctionId].GetPos().x,
			(int)m_junctions[m_junctionId].GetPos().y,
			(int)m_junctions[m_junctionId].GetPos().z,

			results.m_overMem_Physical,
			results.m_overMem_Virtual,

			results.m_numArchetypes,
			results.m_archetypeMem,

			results.m_availableMem_Physical,
			results.m_availableMem_Virtual,

			results.m_sceneCost_Physical,
			results.m_sceneCost_Virtual,

			results.m_unavailableMem_Physical,
			results.m_unavailableMem_Virtual			
		);
		Displayf("Writing %s to %s", entry, m_junctionsDstFile);
		CFileMgr::Write(fi, entry, istrlen(entry));
		CFileMgr::CloseFile(fi);
	}
	else
	{
		Assertf(false,"Error: Could not open filename %s for appending.",m_junctionsDstFile);
	}
}
#endif // __BANK

void CStreamingMemJunctionStats::FinishCapture()
{
	m_capturing = false;	
}

float CStreamingMemJunctionStats::GetCaptureDelayRemainingTime()
{
	float time = m_timer.GetTime();
	float elapsed = time-m_timePreCaptureStarted;
	float remaining = m_preCaptureDelay-elapsed;
	return remaining;
}

void CStreamingMemJunctionStats::RunSimpleCaptureStateMachine()
{
	RunSpinCaptureStateMachine();
}

void CStreamingMemJunctionStats::RunNewCaptureStateMachine()
{
	if (m_respawns>m_maxRespawns)
	{
		Spawn(); // spawn every frame if there was no collision found
	}

	switch (m_state)
	{
		case E_JUNC_MEM_STATE_INVALID:
			break;
		case E_JUNC_MEM_STATE_STARTUP:
			if (camInterface::IsFadedIn())
			{
				m_state = E_JUNC_MEM_STATE_NEXT_JUNCTION;
#if __BANK
				CScene::AddWidgets();
				g_SceneCostTracker.SetJunctionCaptureMode();
#endif // __BANK
			}
			break;
		case E_JUNC_MEM_STATE_NEXT_JUNCTION:
			NextJunction();

			if (m_junctionId>=m_numJunctions-1 || (m_junctionId>=m_junctionIdEnd && m_junctionIdEnd>0) )
			{					
				m_state = E_JUNC_MEM_STATE_FINISHED;
			}
			else
			{
#if USE_DEFRAGMENTATION
				// Disable defrag
				strStreamingEngine::GetDefragmentation()->FlushAndDisable();
#endif
				m_state = E_JUNC_MEM_STATE_SPAWN;
			}
			break;
		case E_JUNC_MEM_STATE_SPAWN:				
			m_camHeading = 0.0f;
			Spawn();
			if (!m_respawn)
			{
				m_state = E_JUNC_MEM_STATE_CAPTURE_START;

#if __BANK
				g_SceneMemoryCheck.Start();
#endif
			}				
			break;
		case E_JUNC_MEM_STATE_CAPTURE_START:
			// Remove stale RPF files
			strPackfileManager::EvictUnusedImages();

			// Try to remove drawlist allocations?
			for (int i = 0; i < 100; ++i)
				gRenderThreadInterface.Flush();

			StartCapture();
			m_state = E_JUNC_MEM_STATE_PRE_CAPTURE_DELAY;
			break;
		case E_JUNC_MEM_STATE_PRE_CAPTURE_DELAY:
#if __BANK
			if (g_SceneMemoryCheck.IsFinished())
#endif
			{
				m_state = E_JUNC_MEM_STATE_CAPTURE;
			}
			break;
		case E_JUNC_MEM_STATE_CAPTURE:
			Capture();
			if (m_captured)
			{
#if __BANK
				g_SceneMemoryCheck.Stop();
#endif
				m_state = E_JUNC_MEM_STATE_CAPTURE_FINISH;
			}
			break;
		case E_JUNC_MEM_STATE_CAPTURE_FINISH:
			FinishCapture();
			m_state = E_JUNC_MEM_STATE_WAIT;
			break;
		case E_JUNC_MEM_STATE_WAIT:
#if USE_DEFRAGMENTATION
			// Enable defrag
			strStreamingEngine::GetDefragmentation()->Enable();
#endif
			m_state = E_JUNC_MEM_STATE_NEXT_JUNCTION;
			break;
		case E_JUNC_MEM_STATE_FINISHED:
			Finish();
			break;
		default:
			break;
	}
}

void CStreamingMemJunctionStats::RunSpinCaptureStateMachine()
{
	float time = m_timer.GetTime();
	float remaining;

	if (m_respawns>m_maxRespawns)
	{
		Spawn(); // spawn every frame if there was no collision found
	}

	switch (m_state)
	{
		case E_JUNC_MEM_STATE_INVALID:
			break;
		case E_JUNC_MEM_STATE_STARTUP:
			if (camInterface::IsFadedIn())
			{
				m_state = E_JUNC_MEM_STATE_NEXT_JUNCTION;
#if __BANK
				CScene::AddWidgets();
				g_SceneCostTracker.SetJunctionCaptureMode();
#endif // __BANK
			}
			break;
		case E_JUNC_MEM_STATE_NEXT_JUNCTION:
				NextJunction();

				if (m_junctionId>=m_numJunctions-1 || (m_junctionId>=m_junctionIdEnd && m_junctionIdEnd>0) )
				{					
					m_state = E_JUNC_MEM_STATE_FINISHED;
				}
				else
				{
					m_state = E_JUNC_MEM_STATE_SPAWN;
				}
				break;
		case E_JUNC_MEM_STATE_SPAWN:				
				m_camHeading = 0.0f;
				Spawn();
				if (!m_respawn)
				{
					if (m_captureType == E_CAPTURE_TYPE_SPIN)
					{
						m_state = E_JUNC_MEM_STATE_SPIN1;						
					}
					else
					{
						m_state = E_JUNC_MEM_STATE_CAPTURE_START;
					}
				}				
			break;
		case E_JUNC_MEM_STATE_SPIN1:
			if(m_camHeading > TWO_PI*m_numStartupSpins)
			{
				m_state = E_JUNC_MEM_STATE_STOP_STREAMING;
				m_camHeading = 0.0f;
			}
			else
			{
				m_camHeading += m_camHeadingSpeed;
			}
			break;
		case E_JUNC_MEM_STATE_STOP_STREAMING:
			CStreaming::SetStreamingPaused(true);
			m_state = E_JUNC_MEM_STATE_SPIN2;
			break;
		case E_JUNC_MEM_STATE_SPIN2:
			if(m_camHeading > (TWO_PI)*m_numSettleSpins)
			{
				m_state = E_JUNC_MEM_STATE_CAPTURE_START;
				m_camHeading = 0.0f;
			}
			else
			{
				m_camHeading += m_camHeadingSpeed;
			}
			break;
		case E_JUNC_MEM_STATE_CAPTURE_START:
			StartCapture();
			m_state = E_JUNC_MEM_STATE_PRE_CAPTURE_DELAY;
			m_timePreCaptureStarted = time;
			break;
		case E_JUNC_MEM_STATE_PRE_CAPTURE_DELAY:	
			remaining = GetCaptureDelayRemainingTime();
			if (remaining<0.0f)
			{
				m_state = E_JUNC_MEM_STATE_CAPTURE;
			}
			break;
		case E_JUNC_MEM_STATE_CAPTURE:
			Capture();
			if (m_captured)
			{
				m_state = E_JUNC_MEM_STATE_CAPTURE_FINISH;
			}
			break;
		case E_JUNC_MEM_STATE_CAPTURE_FINISH:
			FinishCapture();
			m_state = E_JUNC_MEM_STATE_WAIT;
			break;
		case E_JUNC_MEM_STATE_WAIT:
			CStreaming::SetStreamingPaused(false);
			m_state = E_JUNC_MEM_STATE_NEXT_JUNCTION;
			break;
		case E_JUNC_MEM_STATE_FINISHED:
			Finish();
			break;
		default:
			break;
	}
}

void CStreamingMemJunctionStats::Run( )
{
	camInterface::GetGameplayDirector().SetRelativeCameraHeading(0.0f); // has the effect of reseting the pitch

	switch (m_captureType)
	{
		case E_CAPTURE_TYPE_SPIN:
			RunSpinCaptureStateMachine();
			break;
		case E_CAPTURE_TYPE_SIMPLE:
			RunSimpleCaptureStateMachine();
			break;
		case E_CAPTURE_TYPE_NEW:
			RunNewCaptureStateMachine();
			break;
		default:
			Assertf(false, "No recognised capture type specified.");
			break;
	}
	
	camInterface::GetGameplayDirector().SetRelativeCameraHeading(m_camHeading);

	DisplayDebug();
}

void CStreamingMemJunctionStats::NextJunction( )
{
	m_respawns = 0;

	m_junctionId++;

	while(m_junctionId<m_numJunctions && !m_junctions[m_junctionId].IsValid())
	{
		m_junctionId++; 
	}
}

#if __BANK
void 
CStreamingMemJunctionStats::InitWidgets( )
{
	bkBank* pWidgetBank = BANKMGR.FindBank( "Sector Tools" );
	if (AssertVerify(pWidgetBank))
	{
	}
}
#endif // __BANK


#endif // SECTOR_TOOLS_EXPORT
