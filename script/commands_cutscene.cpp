// Rage headers
#include "script/wrapper.h"

// Game headers
#include "Cutscene/CutSceneManagerNew.h"
#include "cutscene/AnimatedModelEntity.h"
#include "script/script_helper.h"
#include "peds/ped.h"
#include "peds/rendering/PedVariationPack.h"
#include "Handlers/GameScriptEntity.h"
#include "commands_entity.h"
#include "cutscene/cutscene_channel.h"
#include "Cutscene/CutSceneStore.h"

ANIM_OPTIMISATIONS();

namespace cutscene_commands
{

#if !__NO_OUTPUT

const char * FindStringFromHash(int hash)
{
	atHashString tempHashString(hash);
	return tempHashString.TryGetCStr() ? tempHashString.TryGetCStr() : "Unknown hash";
}

#define bool_as_string(boolValue) boolValue ? "TRUE" : "FALSE"

#define hash_as_string(hashValue) atVarString("%d(%s)", hashValue, FindStringFromHash(hashValue)).c_str()

#define vector_args(vecValue) vecValue.x, vecValue.y, vecValue.z

#define cutsceneScriptErrorf(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_ERROR) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_ERROR) ) { char debugStr[512]; CutSceneManager::CommonDebugStr(debugStr); cutsceneErrorf("%s-%s:" fmt, CTheScripts::GetCurrentScriptNameAndProgramCounter(), debugStr,##__VA_ARGS__); }
#define cutsceneScriptDisplayf(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DISPLAY) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DISPLAY) ) { char debugStr[512]; CutSceneManager::CommonDebugStr(debugStr); cutsceneDisplayf("%s-%s:" fmt, CTheScripts::GetCurrentScriptNameAndProgramCounter(), debugStr,##__VA_ARGS__); }
#define cutsceneScriptDebugf1(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG1) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG1) ) { char debugStr[512]; CutSceneManager::CommonDebugStr(debugStr); cutsceneDebugf1("%s-%s:" fmt, CTheScripts::GetCurrentScriptNameAndProgramCounter(), debugStr,##__VA_ARGS__); }
#define cutsceneScriptDebugf2(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG2) ) { char debugStr[512]; CutSceneManager::CommonDebugStr(debugStr); cutsceneDebugf2("%s-%s:" fmt, CTheScripts::GetCurrentScriptNameAndProgramCounter(), debugStr,##__VA_ARGS__); }
#define cutsceneScriptDebugf3(fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG3) ) { char debugStr[512]; CutSceneManager::CommonDebugStr(debugStr); cutsceneDebugf3("%s-%s:" fmt, CTheScripts::GetCurrentScriptNameAndProgramCounter(), debugStr,##__VA_ARGS__); }
#else
#define bool_as_string(boolValue) ""

#define vector_args(vecValue) 

#define cutsceneScriptErrorf(fmt,...) do {} while(false)
#define cutsceneScriptDisplayf(fmt,...) do {} while(false)
#define cutsceneScriptDebugf1(fmt,...) do {} while(false)
#define cutsceneScriptDebugf2(fmt,...) do {} while(false)
#define cutsceneScriptDebugf3(fmt,...) do {} while(false)
#endif //!__NO_OUTPUT

	void CommandNewSetCutSceneOrigin(const scrVector &scrVecPos, float fHeading, int ConcatSection)
	{
		cutsceneScriptDebugf1("SET_CUT_SCENE_ORIGIN: scrVecPos: %.3f,%.3f.%.3f, fHeading: %.3f, ConcatSection: %d", vector_args(scrVecPos), fHeading, ConcatSection);

		Vector3 vPos(scrVecPos);
		if(CutSceneManager::GetInstance())
		{
			if(CutSceneManager::GetInstance()->IsConcatted())
			{
				if(ConcatSection == -1)
				{
					int count = CutSceneManager::GetInstance()->GetConcatDataList().GetCount(); 
					for(int concatSection = 0; concatSection < count; concatSection++)
					{
						if(CutSceneManager::GetInstance()->IsConcatSectionValid(concatSection))
						{
							CutSceneManager::GetInstance()->StoreOriginalOrientationData(concatSection); 
							CutSceneManager::GetInstance()->OverrideConcatSectionPosition(vPos, concatSection);
							CutSceneManager::GetInstance()->OverrideConcatSectionHeading(fHeading, concatSection);
						}
					}
					CutSceneManager::GetInstance()->SetSeamlessTriggerOrigin(Vector3(0.0f, 0.0f, 0.0f), false);
				}
				else if(scriptVerifyf(CutSceneManager::GetInstance()->IsConcatSectionValid(ConcatSection), "%s:SET_CUTSCENE_ORIGIN - Invalid concat section.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					CutSceneManager::GetInstance()->StoreOriginalOrientationData(ConcatSection); 
					CutSceneManager::GetInstance()->OverrideConcatSectionPosition(vPos, ConcatSection);
					CutSceneManager::GetInstance()->OverrideConcatSectionHeading(fHeading, ConcatSection);

					//update the trigger area so that it renders correctly
					CutSceneManager::GetInstance()->SetSeamlessTriggerOrigin(Vector3(0.0f, 0.0f, 0.0f), false);
				}
			}
			else
			{
				CutSceneManager::GetInstance()->StoreOriginalOrientationData(0); 
				//update the scene origin
				CutSceneManager::GetInstance()->SetNewStartPos(vPos);
				CutSceneManager::GetInstance()->SetNewStartHeading(fHeading);

				//update the trigger area so that it renders correctly
				CutSceneManager::GetInstance()->SetSeamlessTriggerOrigin(Vector3(0.0f, 0.0f, 0.0f), false);
			}

		}
	}

	void CommandNewSetCutSceneOriginAndOrientation(const scrVector &scrVecPos, const scrVector &rotation, int ConcatSection)
	{
		cutsceneScriptDebugf1("SET_CUT_SCENE_ORIGIN_AND_ORIENTATION: scrVecPos: %.3f,%.3f.%.3f, rotation: %.3f, %.3f, %.3f, ConcatSection: %d", vector_args(scrVecPos), vector_args(rotation), ConcatSection);
		Vector3 vPos(scrVecPos);
		if(CutSceneManager::GetInstance())
		{
			if(CutSceneManager::GetInstance()->IsConcatted())
			{
				if(scriptVerifyf(CutSceneManager::GetInstance()->IsConcatSectionValid(ConcatSection), "%s:SET_CUTSCENE_ORIGIN_AND_ORIENTATION - Invalid concat section.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					CutSceneManager::GetInstance()->StoreOriginalOrientationData(ConcatSection); 
					CutSceneManager::GetInstance()->OverrideConcatSectionPosition(vPos, ConcatSection);
					CutSceneManager::GetInstance()->OverrideConcatSectionHeading(rotation.z, ConcatSection);
					CutSceneManager::GetInstance()->OverrideConcatSectionPitch(rotation.x, ConcatSection);
					CutSceneManager::GetInstance()->OverrideConcatSectionRoll(rotation.y, ConcatSection);

					//update the trigger area so that it renders correctly
					CutSceneManager::GetInstance()->SetSeamlessTriggerOrigin(Vector3(0.0f, 0.0f, 0.0f), false);
				}
			}
			else
			{
				CutSceneManager::GetInstance()->StoreOriginalOrientationData(0); 
				//update the scene origin
				CutSceneManager::GetInstance()->SetSceneOffset(vPos);
				CutSceneManager::GetInstance()->SetSceneRotation(rotation.z);
				CutSceneManager::GetInstance()->SetScenePitch(rotation.x);
				CutSceneManager::GetInstance()->SetSceneRoll(rotation.y);

				//update the trigger area so that it renders correctly
				CutSceneManager::GetInstance()->SetSeamlessTriggerOrigin(Vector3(0.0f, 0.0f, 0.0f), false);
			}

		}
	}

//Streaming
	//Request a scenes assets
	void CommandRequestCutScene(const char *pCutName, int PlayBackContextFlags)
	{
		cutsceneScriptDebugf1("REQUEST_CUTSCENE: pCutName: %s, PlayBackContextFlags: %d", pCutName, PlayBackContextFlags);
		scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
		CutSceneManager::GetInstance()->RequestCutscene(pCutName, false, cutsManager::SKIP_FADE, cutsManager::SKIP_FADE, cutsManager::SKIP_FADE, cutsManager::SKIP_FADE, scriptThreadId , PlayBackContextFlags);
	}

	void CommandRequestCutSceneWithPlayBackList(const char *pCutName, int SceneSection, int PlayBackContextFlags)
	{
		cutsceneScriptDebugf1("REQUEST_CUTSCENE_WITH_PLAYBACK_LIST: pCutName: %s, SceneSection: %d, PlayBackContextFlags: %d", pCutName, SceneSection, PlayBackContextFlags);
		scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
		CutSceneManager::GetInstance()->SetConcatSectionPlaybackFlags(SceneSection);
		CutSceneManager::GetInstance()->RequestCutscene(pCutName, false, cutsManager::SKIP_FADE, cutsManager::SKIP_FADE, cutsManager::SKIP_FADE, cutsManager::SKIP_FADE, scriptThreadId , PlayBackContextFlags);
	}

	void CommandRequestAndPlayCutscene(const char *pCutName, int PlayBackContextFlags)
	{
		cutsceneScriptDebugf1("REQUEST_AND_PLAY_CUTSCENE: pCutName: %s, PlayBackContextFlags: %d", pCutName, PlayBackContextFlags);
		// fade out the game and start loading the data and play the cutscene:
		scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
		CutSceneManager::GetInstance()->RequestCutscene(pCutName, true, cutsManager::FORCE_FADE, cutsManager::FORCE_FADE, cutsManager::FORCE_FADE, cutsManager::FORCE_FADE, scriptThreadId , PlayBackContextFlags);
	}

	//Streams out a streamed cut scene.
	void CommandUnLoadStreamedSeamlessCutScene()
	{
		cutsceneScriptDebugf1("REMOVE_CUTSCENE");
		scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
		CutSceneManager::GetInstance()->UnloadPreStreamedScene(scriptThreadId);
	}

	void CommandRequestCutFile(const char* pCutName)
	{
		cutsceneScriptDebugf1("REQUEST_CUT_FILE: pCutName: %s", pCutName);
		s32 index = g_CutSceneStore.FindSlot(pCutName).Get();
		if(SCRIPT_VERIFY_TWO_STRINGS(index != -1 && g_CutSceneStore.IsValidSlot(strLocalIndex(index)), "REQUEST_CUT_FILE - Invalid cut file name", pCutName))
		{
			CScriptResource_CutFile cutFileResource(index, (u32)0);
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(cutFileResource);
		}
	}

	bool CommandHasCutFileLoaded(const char *pCutName)
	{
		s32 index = g_CutSceneStore.FindSlot(pCutName).Get();
		if(SCRIPT_VERIFY_TWO_STRINGS(index != -1 && g_CutSceneStore.IsValidSlot(strLocalIndex(index)), "HAS_CUT_FILE_LOADED - Invalid cut file name", pCutName))
		{
			if(CTheScripts::GetCurrentGtaScriptHandler())
			{
				if(scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CUT_FILE, index), "HAS_CUT_FILE_LOADED - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pCutName))
				{
					cutsceneScriptDebugf3("HAS_CUT_FILE_LOADED: pCutName: %s, %s", pCutName, bool_as_string(g_CutSceneStore.HasObjectLoaded(strLocalIndex(index))));
					return g_CutSceneStore.HasObjectLoaded(strLocalIndex(index));
				}
			}
		}
		cutsceneScriptDebugf3("HAS_CUT_FILE_LOADED: pCutName: %s, FALSE", pCutName);
		return false;
	}

	void CommandRemoveCutFile(const char *pCutName)
	{
		cutsceneScriptDebugf1("REMOVE_CUT_FILE: pCutName: %s", pCutName);
		s32 index = g_CutSceneStore.FindSlot(pCutName).Get();
		if(SCRIPT_VERIFY_TWO_STRINGS(index != -1 && g_CutSceneStore.IsValidSlot(strLocalIndex(index)), "REMOVE_CUT_FILE - Invalid cut file name", pCutName))
		{
			if(CTheScripts::GetCurrentGtaScriptHandler())
			{
				if(scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CUT_FILE, index), "REMOVE_CUT_FILE - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pCutName))
				{
					CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CUT_FILE, index);
				}
			}
		}
	}

	scrVector CommandGetCutFileOffset(const char *pCutName, int iConcatIndex)
	{
		s32 index = g_CutSceneStore.FindSlot(pCutName).Get();
		if(SCRIPT_VERIFY_TWO_STRINGS(index != -1 && g_CutSceneStore.IsValidSlot(strLocalIndex(index)), "GET_CUT_FILE_OFFSET - Invalid cut file name", pCutName))
		{
			if(CTheScripts::GetCurrentGtaScriptHandler())
			{
				if(scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CUT_FILE, index), "GET_CUT_FILE_OFFSET - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pCutName))
				{
					if(SCRIPT_VERIFY_TWO_STRINGS(g_CutSceneStore.HasObjectLoaded(strLocalIndex(index)), "GET_CUT_FILE_OFFSET - %s has not loaded", pCutName))
					{
						cutfCutsceneFile2 *pCutFile = g_CutSceneStore.Get(strLocalIndex(index));
						if(SCRIPT_VERIFY_TWO_STRINGS(pCutFile, "GET_CUT_FILE_OFFSET - %s cut file is NULL", pCutName))
						{
							if(pCutFile->GetConcatDataList().GetCount() > 0)
							{
								if(SCRIPT_VERIFY_TWO_STRINGS(iConcatIndex < pCutFile->GetConcatDataList().GetCount() && iConcatIndex >= 0 , "GET_CUT_FILE_OFFSET - %s concat index var is out of range for the concat sections of this scene ", pCutName))
								{
									return pCutFile->GetConcatDataList()[iConcatIndex].vOffset;
								}
							}
							else
							{
								return pCutFile->GetOffset();
							}
						}
					}
				}
			}
		}
		return VEC3_ZERO;
	}


	scrVector CommandGetCutFileRotation(const char *pCutName, int iConcatIndex)
	{
		s32 index = g_CutSceneStore.FindSlot(pCutName).Get();
		Vector3 rotation(VEC3_ZERO); 
		if(SCRIPT_VERIFY_TWO_STRINGS(index != -1 && g_CutSceneStore.IsValidSlot(strLocalIndex(index)), "GET_CUT_FILE_ROTATION - Invalid cut file name", pCutName))
		{
			if(CTheScripts::GetCurrentGtaScriptHandler())
			{
				if(scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CUT_FILE, index), "GET_CUT_FILE_ROTATION - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pCutName))
				{
					if(SCRIPT_VERIFY_TWO_STRINGS(g_CutSceneStore.HasObjectLoaded(strLocalIndex(index)), "GET_CUT_FILE_ROTATION - %s has not loaded", pCutName))
					{
						cutfCutsceneFile2 *pCutFile = g_CutSceneStore.Get(strLocalIndex(index));
						if(SCRIPT_VERIFY_TWO_STRINGS(pCutFile, "GET_CUT_FILE_ROTATION - %s cut file is NULL", pCutName))
						{
							if(pCutFile->GetConcatDataList().GetCount() > 0)
							{
								if(SCRIPT_VERIFY_TWO_STRINGS(iConcatIndex < pCutFile->GetConcatDataList().GetCount() && iConcatIndex >= 0 , "GET_CUT_FILE_OFFSET - %s concat index var is out of range for the concat sections of this scene ", pCutName))
								{
									rotation.z = pCutFile->GetConcatDataList()[iConcatIndex].fRotation; 
								}
							}
							else
							{
								rotation.z = pCutFile->GetRotation();
							}
						}
					}
				}
			}
		}
		return rotation;
	}

	int CommandGetCutFileConcatCount(const char *pCutName)
	{
		s32 index = g_CutSceneStore.FindSlot(pCutName).Get();
		if(SCRIPT_VERIFY_TWO_STRINGS(index != -1 && g_CutSceneStore.IsValidSlot(strLocalIndex(index)), "GET_CUT_FILE_CONCAT_COUNT - Invalid cut file name", pCutName))
		{
			if(CTheScripts::GetCurrentGtaScriptHandler())
			{
				if(scriptVerifyf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CUT_FILE, index), "GET_CUT_FILE_CONCAT_COUNT - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pCutName))
				{
					if(SCRIPT_VERIFY_TWO_STRINGS(g_CutSceneStore.HasObjectLoaded(strLocalIndex(index)), "GET_CUT_FILE_CONCAT_COUNT - %s has not loaded", pCutName))
					{
						cutfCutsceneFile2 *pCutFile = g_CutSceneStore.Get(strLocalIndex(index));
						if(SCRIPT_VERIFY_TWO_STRINGS(pCutFile, "GET_CUT_FILE_CONCAT_COUNT - %s cut file is NULL", pCutName))
						{
							return pCutFile->GetConcatDataList().GetCount();
						}
					}
				}
			}
		}
		return 0;
	}
//Play Back
	//starts a streamed cut scene
	void CommandPlaySeamlessCutScene(int optionFlags)
	{
		cutsceneScriptDebugf1("START_CUTSCENE: optionFlags: %d", optionFlags);
		scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
		scriptAssertf(CutSceneManager::GetInstance()->HasSceneStartedInTheSameFrameAsObjectsAreRegistered(),
		"START_SEAMLESS_CUTSCENE: Script has registered objects and started the scene in a different frames. Register objects and start the scene in the same frame");
		CutSceneManager::GetInstance()->PlaySeamlessCutScene(scriptThreadId, (u32)optionFlags);
	}

	void CommandPlaySeamlessCutsceneAtCoord(const scrVector &scrVecPos, int optionFlags)
	{
		cutsceneScriptDebugf1("START_CUTSCENE_AT_COORDS: scrVecPos: %.3f,%.3f.%.3f, optionFlags: %d", scrVecPos.x, scrVecPos.y, scrVecPos.z, optionFlags);
		Vector3 vPos(scrVecPos);
		if(CutSceneManager::GetInstance())
		{
			scriptAssertf(CutSceneManager::GetInstance()->HasSceneStartedInTheSameFrameAsObjectsAreRegistered(),
			"START_SEAMLESS_CUTSCENE_AT_COORDS: Script has registered objects and started the scene in a different frames. Register objects and start the scene in the same frame");

			if(CutSceneManager::GetInstance()->IsConcatted())
			{
				if(scriptVerifyf(CutSceneManager::GetInstance()->IsConcatSectionValid(0), "%s:START_SEAMLESS_CUTSCENE_AT_COORDS - Invalid concat section.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					CutSceneManager::GetInstance()->StoreOriginalOrientationData(0); 
					CutSceneManager::GetInstance()->OverrideConcatSectionPosition(vPos, 0);

					//update the trigger area so that it renders correctly
					CutSceneManager::GetInstance()->SetSeamlessTriggerOrigin(Vector3(0.0f, 0.0f, 0.0f), false);

					//CutSceneManager::GetInstance()->SetNewStartHeading(fHeading);
					scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
					CutSceneManager::GetInstance()->PlaySeamlessCutScene(scriptThreadId, (u32)optionFlags);
				}
			}
			else
			{
				//update the scene origin
				CutSceneManager::GetInstance()->StoreOriginalOrientationData(0); 
				CutSceneManager::GetInstance()->SetNewStartPos(vPos);

				//update the trigger area so that it renders correctly
				CutSceneManager::GetInstance()->SetSeamlessTriggerOrigin(Vector3(0.0f, 0.0f, 0.0f), false);
				
				//CutSceneManager::GetInstance()->SetNewStartHeading(fHeading);
				scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
				CutSceneManager::GetInstance()->PlaySeamlessCutScene(scriptThreadId, (u32)optionFlags);
			}
		}
	}

	void CommandStopCutscene(bool DeleteAllRegisteredEntities)
	{
		cutsceneScriptDebugf1("STOP_CUTSCENE: DeleteAllRegisteredEntities: %s", bool_as_string(DeleteAllRegisteredEntities));
		CutSceneManager::GetInstance()->TriggerCutsceneSkip(true);
		CutSceneManager::GetInstance()->SetDeleteAllRegisteredEntites(DeleteAllRegisteredEntities);
	}

	void CommandStopCutSceneAndDontProgressAnim()
	{
		cutsceneScriptDebugf1("STOP_CUTSCENE_IMMEDIATELY");
		if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
		{
			CutSceneManager::GetInstance()->StopCutsceneAndDontProgressAnim();
		}
	}

//info
	int CommandGetCutsceneTime()
	{
		return ((s32)(CutSceneManager::GetInstance()->GetCutSceneCurrentTime() * 1000.0f) );
	}

	int CommandGetCutscenePlayTime()
	{
		return ((s32)(CutSceneManager::GetInstance()->GetCutScenePlayTime() * 1000.0f) );
	}

	int CommandGetCutsceneTotalDuration()
	{
		const CutSceneManager *pCutSceneManager = CutSceneManager::GetInstance();
		if(pCutSceneManager)
		{
			const cutfCutsceneFile2 *pCutFile = pCutSceneManager->GetCutfFile();
			if(pCutFile)
			{
				return ((s32)(pCutFile->GetTotalDuration() * 1000.0f) );
			}
		}

		return 0;
	}

	int CommandGetCutsceneEndTime()
	{
		return ((s32)(CutSceneManager::GetInstance()->GetEndTime() * 1000.0f));
	}

	int CommandGetCutscenePlayDuration()
	{
		return ((s32)(CutSceneManager::GetInstance()->GetCutSceneDuration() * 1000.0f));
	}

	bool CommandWasCutsceneSkipped()
	{
		bool bWasSkipped = CutSceneManager::GetInstance()->WasSkipped();
		return bWasSkipped;
	}

	bool CommandHasCutsceneFinished()
	{
		return (!CutSceneManager::GetInstance()->IsRunning());
	}

	bool CommandIsCutscenePlaying()
	{
		return (CutSceneManager::GetInstance()->IsRunning());
	}

	bool CommandIsCutsceneActive()
	{
		return (CutSceneManager::GetInstance()->IsActive());
	}

	bool CommandHasCutsceneLoaded()
	{
		return (CutSceneManager::GetInstance()->IsLoaded());
	}

	bool CommandsHasThisCutSceneLoaded(const char* pSceneName)
	{
		u32 SceneHandleHash = atStringHash(pSceneName);

		if(CutSceneManager::GetInstance()->GetSceneHashString()->GetHash() == SceneHandleHash)
		{
			return (CutSceneManager::GetInstance()->IsLoaded());
		}
		return false;
	}


	int CommandGetCutsceneSectionPlaying()
	{
		return CutSceneManager::GetInstance()->GetCurrentSection();
	}
	
	int CommandGetCutsceneConcatSectionPlaying()
	{
		if(CutSceneManager::GetInstance())
		{
			return CutSceneManager::GetInstance()->GetCurrentConcatSection();
		}
		return -1; 
	}

	bool CommandIsCutsceneAuthorized(const char* scene )
	{
#if CUTSCENE_AUTHORIZED_FOR_PLAYBACK
		const CutSceneManager *pCutSceneManager = CutSceneManager::GetInstance();

		if(pCutSceneManager)
		{
			atHashString SceneHash(scene); 
			bool isAuthorized = CutSceneManager::GetInstance()->IsSceneAuthorized(SceneHash);
			if (!isAuthorized)
			{
				cutsceneScriptDebugf1("IS_CUTSCENE_AUTHORIZED: returning FALSE for scene %s. Reason: Scene not in Authorized list", scene);
			}
			return isAuthorized;
		}
		cutsceneScriptDebugf1("IS_CUTSCENE_AUTHORIZED: returning FALSE for scene %s. Reason: no cutscene manager", scene);
		return false;
#else
		scene = NULL; 
		return true; 
#endif
	}

	const char* CommandGetSceneName()
	{
#if !__FINAL
		if(CutSceneManager::GetInstance()->IsActive())
		{
			CutSceneManager::GetInstance()->GetSceneHashString()->GetCStr(); 
		}
#endif
		return NULL; 
	}

	void CommandSetCutSceneReplayRecord(bool bEnable)
	{
		cutsceneScriptDebugf1("SET_CUTSCENE_REPLAY_RECORD: bEnable: %s", bool_as_string(bEnable));
		CutSceneManager::GetInstance()->SetEnableReplayRecord(bEnable);
	}

	void CommandSetScriptCanStartCutScene(int ScriptId)
	{
		cutsceneScriptDebugf1("SET_SCRIPT_CAN_START_CUTSCENE: ScriptId: %d", ScriptId);
		scrThreadId Id = (scrThreadId)ScriptId;

		CutSceneManager::GetInstance()->OverrideScriptThreadId(Id);
	}

	int CommandGetCutsceneStoreSize()
	{
		return g_CutSceneStore.GetSize();
	}

	const char* CommandGetCutSceneName(int cutSceneIndex)
	{
		static char cReturnString[128];
		cReturnString[0] = '\0';

		cutfCutsceneFile2Def* pDef = g_CutSceneStore.GetSlot(strLocalIndex(cutSceneIndex));
		if(pDef)
		{
#if !__FINAL
			formatf(cReturnString, 128, "%s", pDef->m_name.GetCStr());
#else
			formatf(cReturnString, 128, "%x", pDef->m_name.GetHash());
#endif
		}

		return (const char*)&cReturnString;
	}

	bool CommandIsPlayBackFlagSet(int flag)
	{
		if(CutSceneManager::GetInstance()->IsActive())
		{
			return CutSceneManager::GetInstance()->GetPlayBackFlags().IsFlagSet(flag);
		}
		return false;
	}

	bool CommandCanRequestAudioForSyncedScenePlayBack()
	{
		if(CutSceneManager::GetInstance()->IsCutscenePlayingBack())
		{
			return CutSceneManager::GetInstance()->CanScriptRequestSyncedSceneAudioPostScene();
		}
		return false;
	}


enum RegisterdUse
{
	RE_EXISTING_SCRIPT_ENTITY_ANIMTAED_BY_CUTSCENE,
	RE_EXISTING_SCRIPT_ENTITY_ANIMTAED_AND_DELETED_BY_CUTSCENE,
	RE_CUTSCENE_TO_CREATE_SCRIPT_ENTITY,
	RE_CUTSCENE_DONT_ANIMATE
};

	scrVector CommandGetCutSceneEntityCoords(const char* pSceneName, int ModelName)
	{
		atHashString SceneHandleHash(pSceneName);
		atHashString ModelNameHash(ModelName);

		if (CutSceneManager::GetInstance()->IsRunning())
		{
			CCutsceneAnimatedModelEntity* pAnimModelEnt = CutSceneManager::GetInstance()->GetAnimatedModelEntityFromSceneHandle(SceneHandleHash, ModelNameHash);

			if(pAnimModelEnt)
			{
				CEntity* pEnt  = pAnimModelEnt->GetGameEntity();

				if(pEnt)
				{
					return  VEC3V_TO_VECTOR3 (pEnt->GetTransform().GetPosition());
				}
			}
		}
		return VEC3_ZERO;
	}

	float CommandGetCutSceneEntityHeading(const char* pSceneName, int ModelName)
	{
		atHashString SceneHandleHash(pSceneName);
		atHashString ModelNameHash(ModelName);

		if (CutSceneManager::GetInstance()->IsRunning())
		{
			CCutsceneAnimatedModelEntity* pAnimModelEnt = CutSceneManager::GetInstance()->GetAnimatedModelEntityFromSceneHandle(SceneHandleHash, ModelNameHash);

			if(pAnimModelEnt)
			{
				CEntity* pEnt  = pAnimModelEnt->GetGameEntity();

				if(pEnt)
				{
					return (pEnt->GetTransform().GetHeading() * RtoD);
				}
			}
		}
		return 0.0f;
	}

	// Keep in sync with CUTSCENE_ENTITY_SEARCH_RESULT (commands_cutscene.sch)
	enum eCutsceneEntitySearchResult
	{
		kCutsceneNotRunning = 0,
		kCutfileLoading,
		kHandleExists,
		kHandleNotFound,
	};

	int CommandDoesCutSceneHandleExist(const char * sceneHandle)
	{
		atHashString SceneHandleHash(sceneHandle);
		atHashString ModelNameHash((u32)0);

		if(!CutSceneManager::GetInstance()->IsActive())
		{
			return kCutsceneNotRunning;
		}

		if (!CutSceneManager::GetInstance()->IsCutfileLoaded())
		{
			return kCutfileLoading;
		}

		if (CutSceneManager::GetInstance()->GetAnimatedModelEntityFromSceneHandle(SceneHandleHash, ModelNameHash))
		{
			return kHandleExists;
		}
		else
		{
			return kHandleNotFound;
		}
	}

	//**	Entity Commands		**
	void CommandRegisterEnityWithCutScene(int EntityIndex, const char* pSceneName, int flags, int ModelNames, int options )
	{
		cutsceneScriptDebugf1("REGISTER_ENTITY_FOR_CUTSCENE: EntityIndex: %d, pSceneName: %s, flags: %d, ModelNames: %s, options, %d", EntityIndex, pSceneName, flags, hash_as_string(ModelNames), options);
		atHashString ModelNameHash;

		const CPhysical* pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, 0);

		if(pEntity)
		{
			CBaseModelInfo* pModel = pEntity->GetBaseModelInfo();
			if (SCRIPT_VERIFY(pModel, "REGISTER_ENTITY_FOR_CUTSCENE - couldn't find model for the entity's model index")
				&& SCRIPT_VERIFY_TWO_STRINGS(pEntity->GetDrawable(), "REGISTER_ENTITY_FOR_CUTSCENE - Entity (%s) must have a drawable", pSceneName)
				&& SCRIPT_VERIFY_TWO_STRINGS(pEntity->GetDrawable()->GetSkeletonData(),"REGISTER_ENTITY_FOR_CUTSCENE - Entity (%s) must have a skeleton data",pSceneName ))
			{
				ModelNameHash =  atHashString(pModel->GetHashKey());
			}
			else
			{
				return;
			}
		}
		else
		{
			ModelNameHash = atHashString(ModelNames);
		}

		atHashString SceneHandleHash(pSceneName);

		if(SCRIPT_VERIFY(!CutSceneManager::GetInstance()->IsRunning(), "Cannot register objects while the scene is running"))
		{
			if(SCRIPT_VERIFY(ModelNameHash.GetHash() !=0 || ((options&CCutsceneAnimatedModelEntity::CEO_IGNORE_MODEL_NAME)!=0),"REGISTER_ENTITY_FOR_CUTSCENE has no valid model hash" ))
			{
				if(SCRIPT_VERIFY(SceneHandleHash.GetHash() !=0,"REGISTER_ENTITY_FOR_CUTSCENE has no valid scene name" ))
				{
					if (SCRIPT_VERIFY(CutSceneManager::GetInstance()->HasSceneStartedInTheSameFrameAsObjectsAreRegistered(),
						"REGISTER_ENTITY_FOR_CUTSCENE: Must register objects in the same frame as the scene starts." ))
					{

							switch(flags)
							{
							case RE_EXISTING_SCRIPT_ENTITY_ANIMTAED_BY_CUTSCENE:
								{
									CPhysical* pEntity = const_cast<CPhysical*>(CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex));
									if (pEntity)
									{
										cutsceneScriptDebugf1("RegisterEntity: Registered script entity '%s-%s'(%p) with handle '%s' to be animated by the cutscene", pEntity->GetDebugName(), ModelNameHash.TryGetCStr(), pEntity, SceneHandleHash.TryGetCStr());
										//possibly pass in registration use for asserting against ig objects getting deleted
										CutSceneManager::GetInstance()->RegisterGameEntity(pEntity, SceneHandleHash, ModelNameHash, false, false, true, (u32)options);
									}
								}
								break;

							case RE_EXISTING_SCRIPT_ENTITY_ANIMTAED_AND_DELETED_BY_CUTSCENE:
								{
									CPhysical* pEntity = const_cast<CPhysical*>(CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex));
									if (pEntity)
									{
										cutsceneScriptDebugf1("RegisterEntity: Registered script entity '%s-%s'(%p) with handle '%s' to be animated and deleted by the cutscene", pEntity->GetDebugName(),  ModelNameHash.TryGetCStr(), pEntity, SceneHandleHash.TryGetCStr());
										CutSceneManager::GetInstance()->RegisterGameEntity(pEntity, SceneHandleHash, ModelNameHash, true, false, true, (u32)options);
									}
								}
								break;

							case RE_CUTSCENE_TO_CREATE_SCRIPT_ENTITY:
								{
									cutsceneScriptDebugf1("RegisterEntity: Registered script entity '%s' with handle '%s' to be created by the cutscene", ModelNameHash.TryGetCStr(), SceneHandleHash.TryGetCStr());
									CutSceneManager::GetInstance()->RegisterGameEntity(NULL, SceneHandleHash, ModelNameHash, false, true, true, (u32)options);
								}
								break;

							case RE_CUTSCENE_DONT_ANIMATE:
								{
									cutsceneScriptDebugf1("RegisterEntity: Registered script entity '%s' with handle '%s' to not be animated by the cutscene", ModelNameHash.TryGetCStr(), SceneHandleHash.TryGetCStr());
									CutSceneManager::GetInstance()->RegisterGameEntity(NULL, SceneHandleHash, ModelNameHash, false, false, false, (u32)options );
								}
								break;
							}
					}
				}
			}

		}
	}

	int CommandGetRegisteredEntityIndexCreatedByCutScene(const char* pSceneName, int ModelName)
	{
		atHashString SceneHandleHash(pSceneName);
		atHashString ModelNameHash(ModelName);

		if (CutSceneManager::GetInstance()->IsRunning())
		{
			CCutsceneAnimatedModelEntity* pAnimModelEnt = CutSceneManager::GetInstance()->GetAnimatedModelEntityFromSceneHandle(SceneHandleHash, ModelNameHash);

			if(pAnimModelEnt)
			{
				if(pAnimModelEnt->IsRegisteredWithScript())
				{
					if(pAnimModelEnt->m_RegisteredEntityFromScript.bCreatedForScript)
					{
						CEntity* pEnt = NULL;

						if(pAnimModelEnt->IsScriptRegisteredGameEntity())
						{
							pEnt = pAnimModelEnt->GetGameEntity();
						}
						else if(pAnimModelEnt->IsScriptRegisteredRepositionOnlyEntity())
						{
							pEnt = pAnimModelEnt->GetGameRepositionOnlyEntity();
						}

						if(pEnt)
						{
							CPhysical* pPhysical = NULL;

							pPhysical = static_cast<CPhysical*>(pEnt);

							if(pPhysical)
							{
								if (!pPhysical->GetExtension<CScriptEntityExtension>())
								{
									CTheScripts::RegisterEntity(pPhysical, false, false);
								}

								int NewIndex = CTheScripts::GetGUIDFromEntity(*pPhysical);
																
								/*cutsceneDisplayf("CommandGetRegisteredEntityIndexCreatedByCutScene SUCCESS (\"%s\" %u) scene handle (\"%s\" %u) = %d ",
									ModelNameHash.TryGetCStr(),
									ModelNameHash.GetHash(),
									SceneHandleHash.TryGetCStr(),
									SceneHandleHash.GetHash(), NewIndex);*/

								return  NewIndex;
							}
						}
					}
				}
			}
		}

		/*
		cutsceneDisplayf("CommandGetRegisteredEntityIndexCreatedByCutScene FAIL (\"%s\" %u) scene handle (\"%s\" %u) = 0",
			atHashString(ModelNameHash).TryGetCStr(),
			ModelNameHash,
			atHashString(SceneHandleHash).TryGetCStr(),
			SceneHandleHash);
		*/

		return 0;
	}

	void CommandSetVehicleModelPlayerWillExitTheSceneIn(int VehicleModelHash)
	{
		cutsceneScriptDebugf1("SET_VEHICLE_MODEL_PLAYER_WILL_EXIT_SCENE: VehicleModelHash: %s", hash_as_string(VehicleModelHash));
		CutSceneManager::GetInstance()->SetVehicleModelHashPlayerWillExitTheSceneIn((u32) VehicleModelHash ); 
	}

	void CommandRenderTriggerArea()
	{
#if __DEV
		CutSceneManager::GetInstance()->RenderCutSceneTriggerArea();
#endif
	}

	void CommandSetSeamlessCSTriggerArea(const scrVector &vTriggerPoint, float fTriggerRadius, float fTriggerOrient, float fTriggerAngle)
	{
		cutsceneScriptDebugf1("SET_CUTSCENE_TRIGGER_AREA: vTriggerPoint: %.3f, %.3f, %.3f, fTriggerRadius: %.3f, fTriggerOrient: %.3f, fTriggerAngle: %.3f", vector_args(vTriggerPoint), fTriggerRadius, fTriggerOrient, fTriggerAngle);
		CutSceneManager::GetInstance()->SetSeamlessTriggerArea(vTriggerPoint,fTriggerRadius, fTriggerOrient, fTriggerAngle );
	}

	bool CommandIsPedInTriggerArea()
	{
		return (CutSceneManager::GetInstance()->CanPedTriggerSCS());
	}

	//Check if we can set the ped for cut scene exit
	bool CommmandCanSetExitStateForEntity( const char* pSceneName, int ModelName)
	{
		atHashString SceneNameHash(pSceneName);
		atHashString ModelNameHash(ModelName);

		return (CutSceneManager::GetInstance()->CanSetCutSceneExitStateForEntity(SceneNameHash, ModelNameHash));
	}

	//Check if we can set the ped for cut scene exit
	bool CommmandCanSetEnterStateForEntity(const char* pSceneName, int ModelName)
	{
		atHashString SceneNameHash(pSceneName);
		atHashString ModelNameHash(ModelName);

		return (CutSceneManager::GetInstance()->CanSetCutSceneEnterStateForEntity(SceneNameHash, ModelNameHash));
	}

	bool CommandCanSetExitStateForCamera(bool bHideNonRegisteredEntities)
	{
		return (CutSceneManager::GetInstance()->CanSetCutSceneExitStateForCamera(bHideNonRegisteredEntities));
	}


	void  CommandSetPadCanVibrateDuringCutScene(bool bCanVibrate)
	{
		cutsceneScriptDebugf1("SET_PAD_CAN_SHAKE_DURING_CUTSCENE: bCanVibrate: %s", bool_as_string(bCanVibrate));
		if (CutSceneManager::GetInstance()->IsRunning())
		{
			CutSceneManager::GetInstance()->SetCanVibratePadDuringCutScene(bCanVibrate);
		}
	}

	void CommandSetCutSceneFadeValues(bool fadeOutAtStart, bool fadeInAtStart, bool fadeOutAtEnd, bool fadeInAtEnd)
	{
		cutsceneScriptDebugf1("SET_CUTSCENE_FADE_VALUES: fadeOutAtStart: %s, fadeInAtStart: %s, fadeOutAtEnd: %s, fadeInAtEnd: %s", bool_as_string(fadeOutAtStart), bool_as_string(fadeInAtStart), bool_as_string(fadeOutAtEnd), bool_as_string(fadeInAtEnd) );
		CutSceneManager::GetInstance()->SetScriptHasOverridenFadeValues(true);

		if(fadeOutAtStart)
		{
			CutSceneManager::GetInstance()->OverrideFadeOutAtStart(cutsManager::DEFAULT_FADE);
		}
		else
		{
			CutSceneManager::GetInstance()->OverrideFadeOutAtStart(cutsManager::SKIP_FADE);
		}

		if(fadeInAtStart)
		{
			CutSceneManager::GetInstance()->OverrideFadeInAtStart(cutsManager::DEFAULT_FADE);
		}
		else
		{
			CutSceneManager::GetInstance()->OverrideFadeInAtStart(cutsManager::SKIP_FADE);
		}

		if(fadeInAtEnd)
		{
			CutSceneManager::GetInstance()->OverrideFadeInAtEnd(cutsManager::DEFAULT_FADE);
		}
		else
		{
			CutSceneManager::GetInstance()->OverrideFadeInAtEnd(cutsManager::SKIP_FADE);
		}

		if(fadeOutAtEnd)
		{
			CutSceneManager::GetInstance()->OverrideFadeOutAtEnd(cutsManager::DEFAULT_FADE);
		}
		else
		{
			CutSceneManager::GetInstance()->OverrideFadeOutAtEnd(cutsManager::SKIP_FADE);
		}
	}

	void CommandSetCutSceneCanBeSkippedInMutiplayer(bool bSkipped)
	{
		cutsceneScriptDebugf1("NETWORK_SET_MOCAP_CUTSCENE_CAN_BE_SKIPPED: bSkipped: %s", bool_as_string(bSkipped));
		if(NetworkInterface::IsGameInProgress())
		{
			if(CutSceneManager::GetInstance()->IsRunning())
			{
				CutSceneManager::GetInstance()->SetCanSkipCutSceneInMultiplayer(bSkipped);
			}
		}
	}

	void CommandSetCarGeneratorsCanBeActiveDuringCutscene(bool bActive)
	{
		cutsceneScriptDebugf1("SET_CAR_GENERATORS_CAN_UPDATE_DURING_CUTSCENE: bActive: %s", bool_as_string(bActive));
		if(CutSceneManager::GetInstance())
		{
			if(CutSceneManager::GetInstance()->IsRunning())
			{
				CutSceneManager::GetInstance()->SetCarGenertorsUpdateDuringCutscene(bActive);
			}
		}
	}

	void CommandSetCanUseMobilePhoneDuringCutscene(bool bCanUseMobile)
	{
		cutsceneScriptDebugf1("SET_CAN_USE_MOBILE_PHONE_DURING_CUTSCENE: bCanUseMobile: %s", bool_as_string(bCanUseMobile));
		if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
		{
			CutSceneManager::GetInstance()->SetCanUseMobilePhoneDuringCutscene(bCanUseMobile);
		}
	}

	bool CommandCanUseMobilePhoneDuringCutscene()
	{
		if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
		{
			return CutSceneManager::GetInstance()->CanUseMobilePhoneDuringCutscene();
		}
		return false;
	}

	void CommandSetPlayerRepostionCoords(const scrVector &vTriggerPoint)
	{
		cutsceneScriptDebugf1("SET_NON_ANIMATED_PLAYER_COORDS_FOR_END_OF_CUTSCENE: vTriggerPoint: %.3f, %.3f, %.3f", vector_args(vTriggerPoint));
		if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
		{
			CutSceneManager::GetInstance()->SetRepositionCoordAtSceneEnd(vTriggerPoint);
		}
	}

	void CommandSetCutsceneCanBeSkipped(bool Skipped)
	{
		// debug 2 for this one as it seems to be called every frame by script.
		cutsceneScriptDebugf2("SET_CUTSCENE_CAN_BE_SKIPPED: Skipped: %s", bool_as_string(Skipped));
		if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
		{
			CutSceneManager::GetInstance()->SetCanBeSkipped(Skipped);
		}
	}

	bool CommandCanChangeEntityPreUpdateLoading()
	{
		bool bResult = false;
		if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive())
		{
			bResult = CutSceneManager::GetInstance()->CanScriptChangeEntitiesPreUpdateLoading();
		}

		cutsceneDebugf3("CAN_REQUEST_ASSETS_FOR_CUTSCENE_ENTITY called on frame:%u result:%s", fwTimer::GetFrameCount(), bResult ? "TRUE" : "FALSE");
		return bResult;
	}

	void CommandSetCanDisplayMinimapDuringCutsceneThisUpdate()
	{
		cutsceneScriptDebugf2("SET_CAN_DISPLAY_MINIMAP_DURING_CUTSCENE_THIS_UPDATE");
		if (CutSceneManager::GetInstance())
		{
			if (SCRIPT_VERIFY(CutSceneManager::GetInstance()->GetOptionFlags().IsFlagSet(CUTSCENE_NO_WIDESCREEN_BORDERS), "SET_CAN_DISPLAY_MINIMAP_DURING_CUTSCENE_THIS_UPDATE can only be called with cutscenes started with the CUTSCENE_NO_WIDESCREEN_BORDERS flag"))
			{
				CutSceneManager::GetInstance()->SetDisplayMiniMapThisUpdate(true);
			}
		}
	}

	void CommandSetCutScenePedVariation(const char* pSceneName, int ComponentID, int DrawableID, int TextureID, int ModelName)
	{
		cutsceneScriptDebugf1("SET_CUTSCENE_PED_COMPONENT_VARIATION: pSceneName: %s, ComponentID: %d, DrawableID: %d, TextureID: %d, ModelName: %s", pSceneName, ComponentID, DrawableID, TextureID, hash_as_string(ModelName));
		if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive() && CutSceneManager::GetInstance()->CanScriptChangeEntitiesPreUpdateLoading())
		{			
			if(SCRIPT_VERIFY(DrawableID > -1, "SET_CUTSCENE_PED_VARIATION - DrawableID must be >-1") 
			&& SCRIPT_VERIFY(TextureID > -1, "SET_CUTSCENE_PED_VARIATION - TextureID must be >-1") )
			{
				atHashString SceneNameHash(pSceneName);
				atHashString ModelNameHash(ModelName);
				CutSceneManager::GetInstance()->SetCutScenePedVariation(SceneNameHash, ComponentID, DrawableID, TextureID, ModelNameHash);
				//cutsceneDisplayf("CommandSetCutScenePedVariation SUCCESS. handle(%s), ComponentID (%s/%d),  DrawableID (%d), TextureID (%d), ModelName (%s | %d)", pSceneName, CPedVariationData::GetVarOrPropSlotName(ComponentID), ComponentID, DrawableID, TextureID, ModelNameHash.TryGetCStr(), ModelName);
			}
		}
		else
		{
#if !__NO_OUTPUT
			if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive())
			{
				cutsceneScriptErrorf("SET_CUTSCENE_PED_COMPONENT_VARIATION FAIL. Called on the wrong frame! - CAN_REQUEST_ASSETS_FOR_CUTSCENE_ENTITY: %s", bool_as_string(CutSceneManager::GetInstance()->CanScriptChangeEntitiesPreUpdateLoading()));
			}
#endif // !__NO_OUTPUT
			//cutsceneDisplayf("CommandSetCutScenePedVariation FAIL. handle(%s), ComponentID (%d),  DrawableID (%d), TextureID (%d), ModelName (%s | %d)", pSceneName, ComponentID, DrawableID, TextureID, atHashString(ModelName).TryGetCStr(), ModelName);
		}
	}

	void CommandSetCutScenePedPropVariation(const char* pSceneName, int Position, int NewPropIndex, int NewTextIndex, int ModelName)
	{
		cutsceneScriptDebugf1("SET_CUTSCENE_PED_PROP_VARIATION: pSceneName: %s, Position: %d, NewPropIndex: %d, NewTextIndex: %d, ModelName: %s", pSceneName, Position, NewPropIndex, NewTextIndex, hash_as_string(ModelName));
		if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive() && CutSceneManager::GetInstance()->CanScriptChangeEntitiesPreUpdateLoading())
		{
			if(SCRIPT_VERIFY(NewPropIndex > -2, "SET_CUTSCENE_PED_PROP_VARIATION - NewPropIndex must be >-2"))
			{
				atHashString SceneNameHash(pSceneName);
				atHashString ModelNameHash(ModelName);
				//cutsceneDisplayf("CommandSetCutScenePedPropVariation SUCCESS. handle(%s), NewPropIndex (%d),  NewTextIndex (%d), ModelName (%s | %d)", pSceneName, NewPropIndex, NewTextIndex, ModelNameHash.TryGetCStr(), ModelName);
				CutSceneManager::GetInstance()->SetCutScenePedPropVariation(SceneNameHash, Position, NewPropIndex, NewTextIndex, ModelNameHash);
			}
		}
		else
		{
#if !__NO_OUTPUT
			if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive())
			{
				cutsceneScriptErrorf("SET_CUTSCENE_PED_PROP_VARIATION FAIL. Called on the wrong frame! - CAN_REQUEST_ASSETS_FOR_CUTSCENE_ENTITY: %s", bool_as_string(CutSceneManager::GetInstance()->CanScriptChangeEntitiesPreUpdateLoading()));
			}
#endif // !__NO_OUTPUT
			//cutsceneDisplayf("CommandSetCutScenePedPropVariation FAIL. handle(%s), NewPropIndex (%d),  NewTextIndex (%d), ModelName (%s | %d)", pSceneName, NewPropIndex, NewTextIndex, atHashString(ModelName).TryGetCStr(), ModelName);
		}
	}

	void CommandSetCutscenePedVariationFromPed(const char* pSceneName, int PedIndex, int ModelName)
	{
		cutsceneScriptDebugf1("SET_CUTSCENE_PED_COMPONENT_VARIATION_FROM_PED: pSceneName: %s, PedIndex: %d, ModelName: %s", pSceneName, PedIndex, hash_as_string(ModelName));
		if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive() && CutSceneManager::GetInstance()->CanScriptChangeEntitiesPreUpdateLoading())
		{
			const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				atHashString SceneNameHash(pSceneName);
				atHashString ModelNameHash(ModelName);
				CutSceneManager::GetInstance()->SetCutScenePedVariationFromPed(SceneNameHash, pPed, ModelNameHash);
				//cutsceneDisplayf("CommandSetCutscenePedVariationFromPed SUCCESS. handle(%s), PedIndex (%d),  ModelName (%s | %d), varInfoIdx (%d)", pSceneName, PedIndex, ModelNameHash.TryGetCStr(), ModelName);
			}
			else
			{
				cutsceneScriptErrorf("SET_CUTSCENE_PED_COMPONENT_VARIATION_FROM_PED FAIL. Unable to find ped index: %d to register with ModelName: %d", PedIndex, ModelName);
			}
		}
		else
		{
#if !__NO_OUTPUT
			if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive())
			{
				cutsceneScriptErrorf("SET_CUTSCENE_PED_COMPONENT_VARIATION_FROM_PED FAIL. Called on the wrong frame! - CAN_REQUEST_ASSETS_FOR_CUTSCENE_ENTITY: %s", bool_as_string(CutSceneManager::GetInstance()->CanScriptChangeEntitiesPreUpdateLoading()));
			}
#endif // !__NO_OUTPUT
			//cutsceneDisplayf("CommandSetCutscenePedVariationFromPed FAIL. handle(%s), PedIndex (%d),  ModelName (%s | %d), varInfoIdx (%d)", pSceneName, PedIndex, atHashString(ModelName).TryGetCStr(), ModelName);
		}
	}
	
	void CommandSetCutsceneEntityModel(const char* pSceneName, int ModelName, int NewModelName)
	{
		cutsceneScriptDebugf1("SET_CUTSCENE_ENTITY_MODEL: pSceneName: %s, ModelName: %s, NewModelName: %s", pSceneName, hash_as_string(ModelName), hash_as_string(NewModelName));
		if(NetworkInterface::IsGameInProgress())
		{
			if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive() && CutSceneManager::GetInstance()->CanScriptChangeEntityModel())
			{
				atHashString SceneNameHash(pSceneName);
				atHashString ModelNameHash((u32)ModelName);
				atHashString NewModelNameHash((u32)NewModelName); 
				CutSceneManager::GetInstance()->ChangeCutSceneModel(SceneNameHash, ModelNameHash,NewModelNameHash);
			}
			else
			{
#if !__NO_OUTPUT
				if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive())
				{
					cutsceneScriptErrorf("SET_CUTSCENE_ENTITY_MODEL FAIL. Called on the wrong frame! - CAN_CHANGE_CUTSCENE_MODELS: %s", bool_as_string(CutSceneManager::GetInstance()->CanScriptChangeEntityModel()));
				}
#endif // !__NO_OUTPUT
			}
		}
	}

	bool CommandCanChangeCutsceneEntityModel()
	{
		if(NetworkInterface::IsGameInProgress())
		{
			if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive())
			{
				return CutSceneManager::GetInstance()->CanScriptChangeEntityModel();
			}
		}
		return false;
	}

	void CommandSetCutsceneEntityStreamingFlags(const char* pSceneName, int ModelName, int flags)
	{
		cutsceneScriptDebugf1("SET_CUTSCENE_ENTITY_STREAMING_FLAGS: pSceneName: %s, ModelName: %s, flags: %d", pSceneName, hash_as_string(ModelName), flags);
		if(NetworkInterface::IsGameInProgress())
		{
			if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsActive() && CutSceneManager::GetInstance()->CanScriptChangeEntitiesPreUpdateLoading())
			{
				atHashString SceneNameHash(pSceneName);
				atHashString ModelNameHash((u32)ModelName);
				CutSceneManager::GetInstance()->SetCutSceneEntityStreamingFlags(SceneNameHash, ModelNameHash, (u32)flags); 
			}
		}
	}

	bool CommandDoesCutSceneEntityExist(const char* pSceneName, int ModelName)
	{
		atHashString SceneNameHash(pSceneName);
		atHashString ModelNameHash(ModelName);
		CCutsceneAnimatedModelEntity* pAnimModelEnt = CutSceneManager::GetInstance()->GetAnimatedModelEntityFromSceneHandle(SceneNameHash, ModelNameHash);
		if(pAnimModelEnt)
		{
			if(pAnimModelEnt->GetGameEntity())
			{
				return true;
			}
		}

		return false;
	}

	int CommandGetEntityIndexFromCutSceneEntity(const char* pSceneName, int ModelName)
	{
		if (CutSceneManager::GetInstance()->IsRunning())
		{
			atHashString SceneNameHash(pSceneName);
			atHashString ModelNameHash(ModelName);
			CCutsceneAnimatedModelEntity* pAnimModelEnt = CutSceneManager::GetInstance()->GetAnimatedModelEntityFromSceneHandle(SceneNameHash, ModelNameHash);
			if(pAnimModelEnt)
			{
				CEntity* pEnt = NULL;
				pEnt = pAnimModelEnt->GetGameEntity();

				if(pEnt)
				{
					CPhysical* pPhysical = NULL;

					pPhysical = static_cast<CPhysical*>(pEnt);

					if(pPhysical)
					{
						int NewIndex = CTheScripts::GetGUIDFromEntity(*pPhysical);
						return  NewIndex;
					}
				}
			}
		}
		return 0;
	}

	bool CommandHasCutSceneCutThisFrame()
	{
		if(CutSceneManager::GetInstance())
		{
			return (CutSceneManager::GetInstance()->GetHasCutThisFrame());
		}
		return false;
	}

	void CommandStartMultiheadFade(bool MULTIHEAD_ONLY(bFadeIn), bool MULTIHEAD_ONLY(bInstant), bool MULTIHEAD_ONLY(bManual), bool MULTIHEAD_ONLY(bFullscreenMovie))
	{
#if USE_MULTIHEAD_FADE
		if(CutSceneManager::GetInstance())
		{
			CutSceneManager::SetManualBlinders(bManual);
			CutSceneManager::StartMultiheadFade(bFadeIn, bInstant, bFullscreenMovie);
		}
#endif
	}

	void CommandStartMultiheadFadeManual(bool MULTIHEAD_ONLY(bManual))
	{
#if USE_MULTIHEAD_FADE
		if(CutSceneManager::GetInstance())
		{
			CutSceneManager::SetManualBlinders(bManual);
		}
#endif
	}

	void CommandSetMultiheadLinger(int MULTIHEAD_ONLY(iLinger))
	{
#if USE_MULTIHEAD_FADE
		if(CutSceneManager::GetInstance())
		{
			CutSceneManager::SetBlinderDelay(iLinger);
		}
#endif
	}

	bool CommandIsMultiheadFadeUp()
	{
#if USE_MULTIHEAD_FADE
		if(CutSceneManager::GetInstance())
		{
			return CutSceneManager::GetAreBlindersUp();
		}
#endif

		return false;
	}

	//void CommandSetRegisteredEntityGenericWeaponType(const char* pSceneName, int GenericType, int ModelName)
	//{
	//	u32 SceneHandleHash = atStringHash(pSceneName);
	//	u32 ModelNameHash = (u32) ModelName;

	//	if(SCRIPT_VERIFY(GenericType > 0, "The generic type must be valid greater than 0"))
	//	{
	//		CCutsceneAnimatedModelEntity* pAnimModelEnt = CutSceneManager::GetInstance()->GetAnimatedModelEntityFromSceneHandle(SceneHandleHash, ModelNameHash);

	//		if(pAnimModelEnt && pAnimModelEnt->GetType() == CUTSCENE_WEAPON_GAME_ENTITY)
	//		{
	//			CCutSceneAnimatedWeaponEntity* pWeapon = static_cast<CCutSceneAnimatedWeaponEntity*>(pAnimModelEnt);
	//
	//			if(pWeapon)
	//			{
	//				u32 CurrentGenericWeaponType = pWeapon->GetCutObjectGenericWeaponType();
	//				if(SCRIPT_VERIFY( CurrentGenericWeaponType == GenericType, "The entities generic type is %d, but script is trying to set to %d" ))
	//				{
	//					if(SCRIPT_VERIFY( CurrentGenericWeaponType > 0, "The entity does not support generic weapon types"))
	//					{
	//						pWeapon->SetRegisteredGenericWeaponType((u32)GenericType);
	//					}
	//				}
	//			}
	//		}
	//	}
	//}
	///////////////////////////////////////////////////////////////////////////////////

	void SetupScriptCommands()
	{
//streaming new
		SCR_REGISTER_UNUSED(REQUEST_AND_START_CUTSCENE,0x801ca94735fd5710,CommandRequestAndPlayCutscene );
		SCR_REGISTER_SECURE(REQUEST_CUTSCENE,0xd536fd78d8a135f1,CommandRequestCutScene );
		SCR_REGISTER_SECURE(REQUEST_CUTSCENE_WITH_PLAYBACK_LIST,0xcb3859a62f123aef,CommandRequestCutSceneWithPlayBackList );
		SCR_REGISTER_SECURE(REMOVE_CUTSCENE,0x6d23f8c14190d353, CommandUnLoadStreamedSeamlessCutScene);
		SCR_REGISTER_SECURE(HAS_CUTSCENE_LOADED,0xc6398aabc3e92273, CommandHasCutsceneLoaded);
		SCR_REGISTER_SECURE(HAS_THIS_CUTSCENE_LOADED,0x1df60fc4fe15e596, CommandsHasThisCutSceneLoaded);
		SCR_REGISTER_SECURE(SET_SCRIPT_CAN_START_CUTSCENE,0x24aaa4ac4a70a5fe,CommandSetScriptCanStartCutScene);
		SCR_REGISTER_SECURE(CAN_REQUEST_ASSETS_FOR_CUTSCENE_ENTITY,0x2566f947aab7b2a7,CommandCanChangeEntityPreUpdateLoading);
		SCR_REGISTER_SECURE(IS_CUTSCENE_PLAYBACK_FLAG_SET,0x21839852f3265abe,CommandIsPlayBackFlagSet);
		SCR_REGISTER_UNUSED(CAN_CHANGE_CUTSCENE_MODELS,0x1ed6ba3f9c6f146d,CommandCanChangeCutsceneEntityModel);
		SCR_REGISTER_UNUSED(SET_CUTSCENE_ENTITY_MODEL,0x9fd979474a98dd0c, CommandSetCutsceneEntityModel);
		SCR_REGISTER_SECURE(SET_CUTSCENE_ENTITY_STREAMING_FLAGS,0xf1bab9fb6bd91384, CommandSetCutsceneEntityStreamingFlags);

//cutfile 
		SCR_REGISTER_SECURE(REQUEST_CUT_FILE,0x8f907f205dd3e8f4, CommandRequestCutFile );
		SCR_REGISTER_SECURE(HAS_CUT_FILE_LOADED,0xfd4fb17e3c8b78b7, CommandHasCutFileLoaded );
		SCR_REGISTER_SECURE(REMOVE_CUT_FILE,0x3602199b816e7434, CommandRemoveCutFile );
		SCR_REGISTER_UNUSED(GET_CUT_FILE_OFFSET,0x44ccf83f6406241a, CommandGetCutFileOffset );
		SCR_REGISTER_UNUSED(GET_CUT_FILE_ROTATION,0x7e929cacf6fc1447, CommandGetCutFileRotation );
		SCR_REGISTER_SECURE(GET_CUT_FILE_CONCAT_COUNT,0x90982a45733f1007, CommandGetCutFileConcatCount );
//Playback New
		SCR_REGISTER_SECURE(START_CUTSCENE,0xf44ee79112016b61, CommandPlaySeamlessCutScene);
		SCR_REGISTER_SECURE(START_CUTSCENE_AT_COORDS,0x275067d2dbc6803a, CommandPlaySeamlessCutsceneAtCoord);
		SCR_REGISTER_SECURE(STOP_CUTSCENE,0xedfedff9573687b1, CommandStopCutscene);
		SCR_REGISTER_SECURE(STOP_CUTSCENE_IMMEDIATELY,0xa43ada94826528f5,CommandStopCutSceneAndDontProgressAnim);
//info
		SCR_REGISTER_SECURE(SET_CUTSCENE_ORIGIN,0x421a38a0193e4ad2, CommandNewSetCutSceneOrigin);
		SCR_REGISTER_SECURE(SET_CUTSCENE_ORIGIN_AND_ORIENTATION,0x3ac9c2334eac6f2f, CommandNewSetCutSceneOriginAndOrientation);
		SCR_REGISTER_SECURE(GET_CUTSCENE_TIME,0xc10fe9051dbb0e91, CommandGetCutsceneTime);
		SCR_REGISTER_UNUSED(GET_CUTSCENE_PLAY_TIME,0x6450a93bbb26612e, CommandGetCutscenePlayTime);
		SCR_REGISTER_SECURE(GET_CUTSCENE_TOTAL_DURATION,0x4d53e03fee2ecbe3, CommandGetCutsceneTotalDuration);
		SCR_REGISTER_SECURE(GET_CUTSCENE_END_TIME,0x9e3be9bfa8265d37, CommandGetCutsceneEndTime);
		SCR_REGISTER_UNUSED(GET_CUTSCENE_PLAY_DURATION,0xf56cecf7a79fc764, CommandGetCutscenePlayDuration);
		SCR_REGISTER_SECURE(WAS_CUTSCENE_SKIPPED,0x1af916e03ddf8d8a, CommandWasCutsceneSkipped);
		SCR_REGISTER_SECURE(HAS_CUTSCENE_FINISHED,0xa08a61313445db46, CommandHasCutsceneFinished);
		SCR_REGISTER_SECURE(IS_CUTSCENE_ACTIVE,0x496531e41fcf5116, CommandIsCutsceneActive);
		SCR_REGISTER_SECURE(IS_CUTSCENE_PLAYING,0xf34d8fcae3fd4ee4, CommandIsCutscenePlaying);
		SCR_REGISTER_SECURE(GET_CUTSCENE_SECTION_PLAYING,0x2fceb6dbcb58a6b1, CommandGetCutsceneSectionPlaying);
		SCR_REGISTER_UNUSED(GET_CUTSCENE_ENTITY_COORDS,0x8c6270017b363875, CommandGetCutSceneEntityCoords);
		SCR_REGISTER_UNUSED(GET_CUTSCENE_ENTITY_HEADING,0xa1a74e5049e26197, CommandGetCutSceneEntityHeading);
		SCR_REGISTER_SECURE(GET_ENTITY_INDEX_OF_CUTSCENE_ENTITY,0xe621fda45e283be7, CommandGetEntityIndexFromCutSceneEntity);
		SCR_REGISTER_SECURE(GET_CUTSCENE_CONCAT_SECTION_PLAYING,0x6c748aa36e826fec, CommandGetCutsceneConcatSectionPlaying);
		SCR_REGISTER_SECURE(IS_CUTSCENE_AUTHORIZED,0x92e86d9b38775a8e, CommandIsCutsceneAuthorized);
		SCR_REGISTER_UNUSED(GET_CUTSCENE_NAME,0xbfc66c07ab993b54, CommandGetSceneName);
		SCR_REGISTER_UNUSED(SET_CUTSCENE_REPLAY_RECORD,0xd540d662f1d9eb5a, CommandSetCutSceneReplayRecord);

//Register New
		SCR_REGISTER_SECURE(DOES_CUTSCENE_HANDLE_EXIST,0xed7e20741c7645d0,CommandDoesCutSceneHandleExist);
		SCR_REGISTER_SECURE(REGISTER_ENTITY_FOR_CUTSCENE,0x48f297980c708db7,CommandRegisterEnityWithCutScene);
		SCR_REGISTER_SECURE(GET_ENTITY_INDEX_OF_REGISTERED_ENTITY,0xa77263f8c4822da4,CommandGetRegisteredEntityIndexCreatedByCutScene);
		SCR_REGISTER_SECURE(SET_VEHICLE_MODEL_PLAYER_WILL_EXIT_SCENE,0x4aefc38782ce71c1,CommandSetVehicleModelPlayerWillExitTheSceneIn);
		//SCR_REGISTER_UNUSED(SET_REGISTERED_WEAPON_GENERIC_WEAPON_TYPE ,0x842483d5,CommandSetRegisteredEntityGenericWeaponType);

//Trigger New
		SCR_REGISTER_UNUSED(DISPLAY_CUTSCENE_TRIGGER_AREA,0x1d8d8f95a3b496cd, CommandRenderTriggerArea);
		SCR_REGISTER_SECURE(SET_CUTSCENE_TRIGGER_AREA,0x8dc4635f60bd9157, CommandSetSeamlessCSTriggerArea);
		SCR_REGISTER_UNUSED(IS_PLAYER_IN_CUTSCENE_TRIGGER_AREA,0x0cef59984c813512, CommandIsPedInTriggerArea);

//Exit/Enter states
		SCR_REGISTER_SECURE(CAN_SET_ENTER_STATE_FOR_REGISTERED_ENTITY,0x54bc7662cd08cf63, CommmandCanSetEnterStateForEntity);
		SCR_REGISTER_SECURE(CAN_SET_EXIT_STATE_FOR_REGISTERED_ENTITY,0x6f2e1326db60d575, CommmandCanSetExitStateForEntity);
		SCR_REGISTER_SECURE(CAN_SET_EXIT_STATE_FOR_CAMERA,0xa337b111df07bb88, CommandCanSetExitStateForCamera);
//pad
		SCR_REGISTER_SECURE(SET_PAD_CAN_SHAKE_DURING_CUTSCENE,0xee61327a6ed2d0b8, CommandSetPadCanVibrateDuringCutScene);

//Fade
		SCR_REGISTER_SECURE(SET_CUTSCENE_FADE_VALUES,0x12b9b4a15f36e8b1, CommandSetCutSceneFadeValues);
		SCR_REGISTER_SECURE(SET_CUTSCENE_MULTIHEAD_FADE,0x10b0b2bec7faa911, CommandStartMultiheadFade);
		SCR_REGISTER_SECURE(SET_CUTSCENE_MULTIHEAD_FADE_MANUAL,0x6df9dcada6488c5d, CommandStartMultiheadFadeManual);
		SCR_REGISTER_UNUSED(SET_CUTSCENE_MULTIHEAD_LINGER,0xb816ac1bd0df1855, CommandSetMultiheadLinger);
		SCR_REGISTER_SECURE(IS_MULTIHEAD_FADE_UP,0x131a54781ecd3789, CommandIsMultiheadFadeUp);
		

//multiplayer
		SCR_REGISTER_SECURE(NETWORK_SET_MOCAP_CUTSCENE_CAN_BE_SKIPPED,0x4370f25a335dd315, CommandSetCutSceneCanBeSkippedInMutiplayer);
//car gens
		SCR_REGISTER_SECURE(SET_CAR_GENERATORS_CAN_UPDATE_DURING_CUTSCENE,0x986ebe24f15d47a8, CommandSetCarGeneratorsCanBeActiveDuringCutscene);
//Mobile phone
		SCR_REGISTER_UNUSED(SET_CAN_USE_MOBILE_PHONE_DURING_CUTSCENE,0x98b4755ceceeda45, CommandSetCanUseMobilePhoneDuringCutscene );
		SCR_REGISTER_SECURE(CAN_USE_MOBILE_PHONE_DURING_CUTSCENE,0xc1bce12fcf5f630d,  CommandCanUseMobilePhoneDuringCutscene);

		SCR_REGISTER_UNUSED(SET_NON_ANIMATED_PLAYER_COORDS_FOR_END_OF_CUTSCENE,0x3b5e3feab16677d8, CommandSetPlayerRepostionCoords);

//Skipping
		SCR_REGISTER_SECURE(SET_CUTSCENE_CAN_BE_SKIPPED,0x9e174892f219031c, CommandSetCutsceneCanBeSkipped);

// HUD
		SCR_REGISTER_SECURE(SET_CAN_DISPLAY_MINIMAP_DURING_CUTSCENE_THIS_UPDATE,0x7460f14068278ccb, CommandSetCanDisplayMinimapDuringCutsceneThisUpdate);

//Variations
		SCR_REGISTER_SECURE(SET_CUTSCENE_PED_COMPONENT_VARIATION,0x44f8efeff0cda3fe, CommandSetCutScenePedVariation);
		SCR_REGISTER_SECURE(SET_CUTSCENE_PED_COMPONENT_VARIATION_FROM_PED,0x3817498c7b1515dd, CommandSetCutscenePedVariationFromPed);
		SCR_REGISTER_SECURE(DOES_CUTSCENE_ENTITY_EXIST,0x3b4294cabaaf7b71, CommandDoesCutSceneEntityExist);
		SCR_REGISTER_SECURE(SET_CUTSCENE_PED_PROP_VARIATION,0x6d690024c2bb1053, CommandSetCutScenePedPropVariation);

		SCR_REGISTER_SECURE(HAS_CUTSCENE_CUT_THIS_FRAME,0x0d080b3eba2acfb7, CommandHasCutSceneCutThisFrame);

// Info
		SCR_REGISTER_UNUSED(GET_LENGTH_OF_CUTSCENE_LIST,0xe88a697db1c24def, CommandGetCutsceneStoreSize);
		SCR_REGISTER_UNUSED(GET_CUTSCENE_NAME_FROM_LIST,0x6c38d23666a23aa8, CommandGetCutSceneName);
// audio
		SCR_REGISTER_UNUSED(CAN_REQUEST_AUDIO_FOR_SYNCED_SCENE_DURING_CUTSCENE,0x985d752b282700f4, CommandCanRequestAudioForSyncedScenePlayBack);
	}
}

// eof
