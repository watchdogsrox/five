
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/worldpoints.cpp
// PURPOSE : Holds an array of coords (read from the 2d effects data) ordered by sector
//			 These coords will never change so the array is only suitable for holding 2d effects
//			 attached to objects that can't be moved.
// AUTHOR :  Graeme
// CREATED : 9/3/06
//
/////////////////////////////////////////////////////////////////////////////////

// Framework headers
#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "peds/PlayerInfo.h"
#include "network/NetworkInterface.h"
#include "scene/world/gameWorld.h"
#include "scene/worldpoints.h"
#include "script/script.h"
#include "script/script_brains.h"

CWorld2dEffect_Sector CWorldPoints::m_aWorld2dEffectSectors[WORLD2DEFFECT_WIDTHINSECTORS * WORLD2DEFFECT_DEPTHINSECTORS] ;
s32 CWorldPoints::ms_currentSectorX = 0;
s32 CWorldPoints::ms_currentSectorY = 0;
s32 CWorldPoints::ms_currentSectorCurrentIndex = -1;

void CWorldPoints::Init(unsigned initMode)
{
    if(initMode == INIT_SESSION)
    {
		for (s32 i = 0; i < CModelInfo::GetWorld2dEffectArray().GetCount(); i++)
	    {
			C2dEffect *pEff = CModelInfo::GetWorld2dEffectArray()[i];

		    if (pEff->GetType() == ET_COORDSCRIPT)
		    {
			    CWorldPointAttr* wp = pEff->GetWorldPoint();

				wp->Reset(true);
		    }
	    }
    }
}

void CWorldPoints::Shutdown(unsigned /*shutdownMode*/)
{
}

void CWorldPoints::RemoveScriptBrain(s16 brainIndex)
{
	for (s32 i = 0; i < CModelInfo::GetWorld2dEffectArray().GetCount(); i++)
	{
		C2dEffect *pEff = CModelInfo::GetWorld2dEffectArray()[i];

		if (pEff->GetType() == ET_COORDSCRIPT)
		{
			CWorldPointAttr* wp = pEff->GetWorldPoint();

			if (wp->ScriptBrainToLoad == brainIndex)
			{
				CTheScripts::GetScriptBrainDispatcher().RemoveWaitingWorldPoint(pEff, brainIndex);

				wp->Reset(true);
			}
		}
	}
}

void CWorldPoints::SortWorld2dEffects()
{
	s32 NumberOfWorld2dEffects = CModelInfo::GetWorld2dEffectArray().GetCount();
	if (NumberOfWorld2dEffects == 0){
		return;
	}

	int* SectorIndices = Alloca(int, NumberOfWorld2dEffects);

	int loop;
	C2dEffect** ppFirstEff = &CModelInfo::GetWorld2dEffectArray()[0];

	for (loop = 0; loop < NumberOfWorld2dEffects; loop++)
	{
		C2dEffect* pEff = CModelInfo::GetWorld2dEffectArray()[loop];

		Vector3 Pos;
		pEff->GetPos(Pos);

		int SectorX = WORLD2DEFFECT_WORLDTOSECTORX(Pos.x);
		int SectorY = WORLD2DEFFECT_WORLDTOSECTORY(Pos.y);

		Assertf( (SectorX >= 0) && (SectorX < WORLD2DEFFECT_WIDTHINSECTORS), "Sector X of world point 2d effect is out of range");
		Assertf( (SectorY >= 0) && (SectorY < WORLD2DEFFECT_DEPTHINSECTORS), "Sector Y of world point 2d effect is out of range");

		SectorIndices[loop] = SectorX + (SectorY * WORLD2DEFFECT_WIDTHINSECTORS);
	}

//	sort SectorIndices and re-arrange the corresponding entries in World2dEffectStore at the same time
	for (loop = 0; loop < NumberOfWorld2dEffects; loop++)
	{
		for (int loop2 = (loop + 1); loop2 < NumberOfWorld2dEffects; loop2++)
		{
			C2dEffect* temp2dEffect;
			int tempInt;

			if (SectorIndices[loop] > SectorIndices[loop2])
			{
				//	swap SectorIndices entries
				tempInt = SectorIndices[loop];
				SectorIndices[loop] = SectorIndices[loop2];
				SectorIndices[loop2] = tempInt;

				//	swap the actual World2dEffects entries
				temp2dEffect = ppFirstEff[loop];
				ppFirstEff[loop] = ppFirstEff[loop2];
				ppFirstEff[loop2] = temp2dEffect;
			}
		}
	}

	for (loop = 0; loop < (WORLD2DEFFECT_WIDTHINSECTORS * WORLD2DEFFECT_DEPTHINSECTORS); loop++)
	{
		m_aWorld2dEffectSectors[loop].SetFirstWorld2dEffectIndex(-1);
		m_aWorld2dEffectSectors[loop].SetLastWorld2dEffectIndex(-1);
	}

	int SectorIndexForPreviousLoop = -1;
	int IndexOfSectorToUpdate = -1;
	for (loop = 0; loop < NumberOfWorld2dEffects; loop++)
	{
		if (Verifyf(SectorIndices[loop] != -1, "CWorldPoints::SortWorld2dEffects - expected all SectorIndices to have a valid value at this stage"))
		{
			if (SectorIndices[loop] != SectorIndexForPreviousLoop)
			{	//	If we've moved on to a new sector index
				if (IndexOfSectorToUpdate != -1)
				{	//	IndexOfSectorToUpdate will be the index of the sector that we've already called SetFirstWorld2dEffectIndex for.
					//	Now we complete its data by calling SetLastWorld2dEffectIndex
					m_aWorld2dEffectSectors[IndexOfSectorToUpdate].SetLastWorld2dEffectIndex(loop-1);					
				}
				IndexOfSectorToUpdate = SectorIndices[loop];
				m_aWorld2dEffectSectors[IndexOfSectorToUpdate].SetFirstWorld2dEffectIndex(loop);

				SectorIndexForPreviousLoop = SectorIndices[loop];
			}
		}
	}

	if (NumberOfWorld2dEffects > 0)
	{
		if (Verifyf(IndexOfSectorToUpdate != -1, "CWorldPoints::SortWorld2dEffects - expected IndexOfSectorToUpdate to have a valid index after looping through all world points"))
		{	//	IndexOfSectorToUpdate will be the index of the sector that we've already called SetFirstWorld2dEffectIndex for.
			//	Now we complete its data by setting its LastIndex to the last element in the World2dEffectStore
			m_aWorld2dEffectSectors[IndexOfSectorToUpdate].SetLastWorld2dEffectIndex(NumberOfWorld2dEffects-1);
		}
	}
}


void CWorldPoints::ScanForScriptTriggerWorldPoints(const Vector3 &vecCentre)
{
	if (CModelInfo::GetWorld2dEffectArray().GetCount() == 0){
		return;
	}

	u32 CurrentFrame = fwTimer::GetFrameCount();

	s32 nLeft = WORLD2DEFFECT_WORLDTOSECTORX(vecCentre.x - WORLDPOINT_SCAN_RANGE);
	s32 nBottom = WORLD2DEFFECT_WORLDTOSECTORY(vecCentre.y - WORLDPOINT_SCAN_RANGE);
	s32 nRight = WORLD2DEFFECT_WORLDTOSECTORX(vecCentre.x + WORLDPOINT_SCAN_RANGE);
	s32 nTop = WORLD2DEFFECT_WORLDTOSECTORY(vecCentre.y + WORLDPOINT_SCAN_RANGE);

	// If either X or Y are lower than nLeft or nBottom, respectively
	if( ms_currentSectorX < nLeft || ms_currentSectorY < nBottom )
	{
		// Reset X and Y
		ms_currentSectorX = nLeft;
		ms_currentSectorY = nBottom;

		// Reset index counters
		CWorld2dEffect_Sector& sector = GetWorld2dEffectSector(ms_currentSectorX, ms_currentSectorY);
		s32 currentSectorBeginIndex = sector.GetFirstWorld2dEffectIndex();
		// if begin index is invalid
		if (currentSectorBeginIndex < 0)
		{
			// Increment X
			ms_currentSectorX++;

			// Reset current sector index
			ms_currentSectorCurrentIndex = 0;
			
			// skip this sector
			return;
		}
		else // begin index is valid
		{
			ms_currentSectorCurrentIndex = currentSectorBeginIndex;
		}
	}
	else
	{
		CWorld2dEffect_Sector& currentSector = GetWorld2dEffectSector(ms_currentSectorX, ms_currentSectorY);
		s32 currentSectorEndIndex = currentSector.GetLastWorld2dEffectIndex();

		// If current sector index exceeds end index
		if( ms_currentSectorCurrentIndex > currentSectorEndIndex )
		{
			// Increment X
			ms_currentSectorX++;

			// If X exceeds nRight
			if( ms_currentSectorX > nRight )
			{
				// Reset X and Increment Y
				ms_currentSectorX = nLeft;
				ms_currentSectorY++;
			}

			// If Y exceeds nTop
			if( ms_currentSectorY > nTop )
			{
				// Reset X and Y
				ms_currentSectorX = nLeft;
				ms_currentSectorY = nBottom;
			}

			// Reset index counters
			CWorld2dEffect_Sector& nextSector = GetWorld2dEffectSector(ms_currentSectorX, ms_currentSectorY);
			s32 currentSectorBeginIndex = nextSector.GetFirstWorld2dEffectIndex();
			// if begin index is invalid
			if (currentSectorBeginIndex < 0)
			{
				// Increment X
				ms_currentSectorX++;

				// Reset current sector index
				ms_currentSectorCurrentIndex = 0;

				// skip this sector
				return;
			}
			else // begin index is valid
			{
				ms_currentSectorCurrentIndex = currentSectorBeginIndex;
			}
		}
	}

	CWorld2dEffect_Sector& currentSector = GetWorld2dEffectSector(ms_currentSectorX, ms_currentSectorY);
	s32 currentSectorBeginIndex = currentSector.GetFirstWorld2dEffectIndex();
	s32 currentSectorEndIndex = currentSector.GetLastWorld2dEffectIndex();

	// ensure current index is in valid range for current sector
	if( ms_currentSectorCurrentIndex < currentSectorBeginIndex )
	{
		ms_currentSectorCurrentIndex = currentSectorBeginIndex;
	}
	else if( ms_currentSectorCurrentIndex > currentSectorEndIndex )
	{
		// skip this frame
		return;
	}

	// Process a limited number of effects per frame
	static s32 s_numEffectsToScanPerFrameMAX = 32;
	static s32 s_numFramesToConsiderPointExpired = 100;
	const int stopIndexThisFrame = Min(ms_currentSectorCurrentIndex + s_numEffectsToScanPerFrameMAX - 1, currentSectorEndIndex);
	for(; ms_currentSectorCurrentIndex <= stopIndexThisFrame; ms_currentSectorCurrentIndex++)
	{
		C2dEffect* pEff = CModelInfo::GetWorld2dEffectArray()[ms_currentSectorCurrentIndex];

		if (pEff->GetType() == ET_COORDSCRIPT)
		{
			CWorldPointAttr* wp = pEff->GetWorldPoint();

			switch (wp->BrainStatus)
			{
				case OUT_OF_BRAIN_RANGE :

					if (wp->ScriptBrainToLoad == -1)
					{
						// ScriptBrainToLoad should be filled in here the first time and then should never need set again
						wp->ScriptBrainToLoad = (s16)CTheScripts::GetScriptsForBrains().GetIndexOfScriptBrainWithThisName(wp->m_scriptName, CScriptsForBrains::WORLD_POINT);
					}

					if (CTheScripts::GetScriptsForBrains().IsWorldPointWithinActivationRange(pEff, vecCentre) )
					{
						if (!CTheScripts::GetScriptsForBrains().CanBrainSafelyRun(wp->ScriptBrainToLoad))
						{
							//	This should prevent the brain triggering as soon as the player leaves the network game if he is standing within the activation range
							wp->BrainStatus = WAITING_TILL_OUT_OF_BRAIN_RANGE;
						}
						else
						{
							CTheScripts::GetScriptsForBrains().StartOrRequestNewStreamedScriptBrain(wp->ScriptBrainToLoad, pEff, CScriptsForBrains::WORLD_POINT, true);
						}
					}
					break;

				case WAITING_TILL_OUT_OF_BRAIN_RANGE :
					// Check for deactivation range
					if (CTheScripts::GetScriptsForBrains().IsWorldPointWithinActivationRange(pEff, vecCentre) == false)
					{
						wp->BrainStatus = OUT_OF_BRAIN_RANGE;
					}
					else if (CurrentFrame < wp->LastUpdatedInFrame)
					{
						//	frame counter has wrapped round
						wp->BrainStatus = OUT_OF_BRAIN_RANGE;
					}
					else
					{
						if ( (CurrentFrame - wp->LastUpdatedInFrame) > s_numFramesToConsiderPointExpired )
						{
							//	player has been further than WORLDPOINT_SCAN_RANGE metres from the effect for a few frames
							wp->BrainStatus = OUT_OF_BRAIN_RANGE;
						}
					}
					break;
			}// END switch
			wp->LastUpdatedInFrame = CurrentFrame;
		}// END if
	}// END for
}

void CWorldPoints::ReactivateAllWorldPointBrainsThatAreWaitingTillOutOfRange()
{
	s32 NumberOfWorld2dEffects = CModelInfo::GetWorld2dEffectArray().GetCount();

	for (int loop = 0; loop < NumberOfWorld2dEffects; loop++)
	{
		C2dEffect* pEff = CModelInfo::GetWorld2dEffectArray()[loop];

		if (pEff->GetType() == ET_COORDSCRIPT)
		{
			CWorldPointAttr* wp = pEff->GetWorldPoint();
			if (wp->ScriptBrainToLoad != -1)
			{
				if (wp->BrainStatus == WAITING_TILL_OUT_OF_BRAIN_RANGE)
				{
					wp->BrainStatus = OUT_OF_BRAIN_RANGE;
				}
			}
		}
	}	
}

void CWorldPoints::ReactivateNamedWorldPointBrainsWaitingTillOutOfRange(const char *pNameToSearchFor)
{
	strStreamingObjectName nameToSearchFor(pNameToSearchFor);
	s32 NumberOfWorld2dEffects = CModelInfo::GetWorld2dEffectArray().GetCount();

	for (int loop = 0; loop < NumberOfWorld2dEffects; loop++)
	{
		C2dEffect* pEff = CModelInfo::GetWorld2dEffectArray()[loop];

		if (pEff->GetType() == ET_COORDSCRIPT)
		{
			CWorldPointAttr* wp = pEff->GetWorldPoint();
			if (wp->ScriptBrainToLoad != -1)
			{
				if (wp->BrainStatus == WAITING_TILL_OUT_OF_BRAIN_RANGE)
				{
					if (wp->m_scriptName == nameToSearchFor)
					{
						wp->BrainStatus = OUT_OF_BRAIN_RANGE;
					}
				}
			}
		}
	}	
}

#if __DEV
void CWorldPoints::RenderDebug()
{
	bool	bSecondText = ((fwTimer::GetTimeInMilliseconds() / 1000) & 1);

	const float Range = 100.0f;
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vecCentre = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	s32 nLeft = WORLD2DEFFECT_WORLDTOSECTORX(vecCentre.x - Range);
	s32 nBottom = WORLD2DEFFECT_WORLDTOSECTORY(vecCentre.y - Range);
	s32 nRight = WORLD2DEFFECT_WORLDTOSECTORX(vecCentre.x + Range);
	s32 nTop = WORLD2DEFFECT_WORLDTOSECTORY(vecCentre.y + Range);

	for (s32 y = nBottom; y <= nTop; y++)
	{
		for (s32 x = nLeft; x <= nRight; x++)
		{
			CWorld2dEffect_Sector& sector = GetWorld2dEffectSector(x, y);

			//
			// Render all the world 2d effects
			//

			int IndexOfFirst2dEffect = sector.GetFirstWorld2dEffectIndex();
			if (IndexOfFirst2dEffect >= 0)
			{
				int IndexOfLast2dEffect = sector.GetLastWorld2dEffectIndex();

				for (int loop = IndexOfFirst2dEffect; loop <= IndexOfLast2dEffect; loop++)
				{
					C2dEffect	*pEff = CModelInfo::GetWorld2dEffectArray()[loop];

					Vector3 Pos;
					pEff->GetPos(Pos);

					//if( pEff->GetType() == ET_SPAWN_POINT)
					//	||	!bJustDebugScenarioPoints )
					{
						char string1[128];
						Color32	textColour;

						strcpy(string1, "W:");
						pEff->FindDebugColourAndText(&textColour, string1+2, bSecondText, NULL);

						grcDebugDraw::Text(Pos, textColour, string1);

						const CSpawnPoint *sp = pEff->GetSpawnPoint();
						if( sp )
						{
							grcDebugDraw::Sphere(Pos, 0.2f,textColour, true);
							Vector3 vDir = sp->GetSpawnPointDirection(NULL);
							grcDebugDraw::Line(Pos, Pos + vDir, textColour);
						}
					}
				}
			}
		}
	}
}
#endif

