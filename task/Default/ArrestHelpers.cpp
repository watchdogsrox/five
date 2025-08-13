//
// Task/Default/ArrestHelpers.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

// Class header 
#include "Task/Default/ArrestHelpers.h"

// Game headers
#include "script/script_helper.h"
#include "streaming/streaming.h"
#include "peds/Ped.h"
#include "Task/Default/TaskCuffed.h"
#include "Peds/PedIntelligence.h"
#include "task/Default/TaskIncapacitated.h"

#if __BANK
	#include "Peds/PedDebugVisualiser.h"
	#include "scene/world/gameWorld.h"
	#include "Peds/rendering/PedVariationDebug.h"
	#include "Peds\pedfactory.h"
#endif

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

float CArrestHelpers::GetArrestNetworkTimeout()
{
	static dev_float s_fTimeOut = 2.0f;
	return s_fTimeOut;
}

bool CArrestHelpers::UsingNewArrestAnims()
{
	TUNE_GROUP_BOOL(ARREST,bUseNewAnims,true);
	return bUseNewAnims;
}

bool CArrestHelpers::CanArresterStepAroundToCuff()
{
	TUNE_GROUP_BOOL(ARREST,bStepAroundToCuff,false);
	return bStepAroundToCuff;
}

bool TestClipMoverAgainstWorld(const crClip *pClip, const Vector3 &vStartPosition, const Quaternion &qStartRotation, const float fCapsuleHeight, const float fCapsuleRadius, const CEntity **ppEntitiesToIgnore, const int iNumEntitiesToIgnore)
{
	bool bAnyHits = false;

	SCRIPT_ASSERT(pClip, "IS_ARREST_TYPE_VALID - Clip cannot be NULL!");
	if(pClip)
	{
		// Setup synced scene origin matrix
		Matrix34 mStart(Matrix34::IdentityType);
		mStart.d = vStartPosition;
		mStart.FromQuaternion(qStartRotation);

		//grcDebugDraw::Sphere(mStart.d, 0.25f, Color_red, false, 100);
		//grcDebugDraw::Axis(mStart, 1.0f, true, 100);

		// Add clip initial mover position and rotation
		fwAnimHelpers::ApplyInitialOffsetFromClip(*pClip, mStart);

		//grcDebugDraw::Sphere(mStart.d, 0.25f, Color_yellow, false, 100);
		//grcDebugDraw::Axis(mStart, 1.0f, true, 100);

		// Iterate through frames
		for(float fFrame = 0, fNum30Frames = pClip->GetNum30Frames(); fFrame < fNum30Frames && !bAnyHits; fFrame += 10)
		{
			float fPhase = pClip->Convert30FrameToPhase(fFrame);

			// Get clip current mover matrix
			Matrix34 mMover(Matrix34::IdentityType);
			fwAnimHelpers::GetMoverTrackMatrixDelta(*pClip, 0.0f, fPhase, mMover);

			// Add mover matrix to start matrix
			Matrix34 mFinal = mStart;
			mFinal.DotFromLeft(mMover);

			//grcDebugDraw::Sphere(mFinal.d, 0.25f, Color_green, false, 100);
			//grcDebugDraw::Axis(mFinal, 1.0f, true, 100);

			// Setup capsule shape test
			Vector3 vCapsuleStart = mFinal.d;
			Vector3 vCapsuleEnd = mFinal.d + Vector3(0.0f, 0.0f, fCapsuleHeight);
			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetCapsule(vCapsuleStart, vCapsuleEnd, fCapsuleRadius);
			capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
			capsuleDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
			capsuleDesc.SetIsDirected(false);
			if(ppEntitiesToIgnore && iNumEntitiesToIgnore > 0)
			{
				capsuleDesc.SetExcludeEntities(ppEntitiesToIgnore, iNumEntitiesToIgnore);
			}

			// Do capsule shape test
			bool bHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

			//grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(vCapsuleStart), VECTOR3_TO_VEC3V(vCapsuleEnd), fCapsuleRadius, bHit ? Color_red : Color_green, false);

			bAnyHits |= bHit;
		}
	}

	return bAnyHits;
}

////////////////////////////////////////////////////////////////////////////////

bool TestClipCameraAgainstWorld(const crClip *pClip, const Vector3 &vStartPosition, const Quaternion &qStartRotation, const float fSphereRadius, const CEntity **ppEntitiesToIgnore, const int iNumEntitiesToIgnore)
{
	bool bAnyHits = false;

	SCRIPT_ASSERT(pClip, "IS_ARREST_TYPE_VALID - Clip cannot be NULL!");
	if(pClip)
	{
		Matrix34 mTemp(Matrix34::IdentityType);

		// Setup synced scene origin matrix
		Matrix34 mStart(Matrix34::IdentityType);
		mStart.Translate(vStartPosition);
		mTemp.Identity(); mTemp.FromQuaternion(qStartRotation);
		mStart.Dot(mTemp, mStart);

		// Add clip initial mover position and rotation
		Vector3 vInitialPosition = fwAnimHelpers::GetCameraTrackTranslation(*pClip, 0.0f);
		mStart.Translate(vInitialPosition);
		Quaternion qInitialRotation = fwAnimHelpers::GetCameraTrackRotation(*pClip, 0.0f);
		mTemp.Identity(); mTemp.FromQuaternion(qInitialRotation);
		mStart.Dot(mTemp, mStart);

		// Iterate through frames
		for(float fFrame = 0, fNum30Frames = pClip->GetNum30Frames(); fFrame < fNum30Frames && !bAnyHits; fFrame ++)
		{
			float fPhase = pClip->Convert30FrameToPhase(fFrame);

			// Get clip current mover matrix
			Matrix34 mMover(Matrix34::IdentityType);
			fwAnimHelpers::GetCameraTrackMatrixDelta(*pClip, 0.0f, fPhase, mMover);

			// Add mover matrix to start matrix
			Matrix34 mFinal;
			mFinal.Dot(mMover, mStart);

			// Setup capsule shape test
			Vector3 vSphereStart = mFinal.d;
			WorldProbe::CShapeTestSphereDesc sphereDesc;
			sphereDesc.SetSphere(vSphereStart, fSphereRadius);
			sphereDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
			if(ppEntitiesToIgnore && iNumEntitiesToIgnore > 0)
			{
				sphereDesc.SetExcludeEntities(ppEntitiesToIgnore, iNumEntitiesToIgnore);
			}

			// Do capsule shape test
			bool bHit = WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc);

			//grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(vSphereStart), fSphereRadius, bHit ? Color_red : Color_green, false);

			bAnyHits |= bHit;
		}
	}

	return bAnyHits;
}

bool CArrestHelpers::IsArrestTypeValid(const CPed *pCopPed, const CPed *pCrookPed, int ArrestType)
{
	bool bResult = false;

	if(ArrestType == ARREST_CUFFING || ArrestType == ARREST_UNCUFFING)
	{
		static const atHashString clipDictionary("MP_ARRESTING",0x1532946A);

		crClipDictionary *pClipDictionary = NULL;
		bool bDidILoadThisDictionary = false;

		// Request clip dictionary

		strLocalIndex iClipDictionarySlot = g_ClipDictionaryStore.FindSlotFromHashKey(clipDictionary.GetHash());
		SCRIPT_ASSERT(iClipDictionarySlot != -1, "IS_ARREST_TYPE_VALID - Could not find clip dictionary!");
		if(iClipDictionarySlot != -1)
		{
			iClipDictionarySlot = g_ClipDictionaryStore.IsValidSlot(iClipDictionarySlot) ? iClipDictionarySlot : strLocalIndex(-1);
			SCRIPT_ASSERT(iClipDictionarySlot != -1, "IS_ARREST_TYPE_VALID - Could not find clip dictionary!");
			if(iClipDictionarySlot != -1)
			{
				iClipDictionarySlot = g_ClipDictionaryStore.IsObjectInImage(iClipDictionarySlot) ? iClipDictionarySlot : strLocalIndex(-1);
				SCRIPT_ASSERT(iClipDictionarySlot != -1, "IS_ARREST_TYPE_VALID - Could not find clip dictionary!");
				if(iClipDictionarySlot != -1)
				{
					if(!g_ClipDictionaryStore.HasObjectLoaded(iClipDictionarySlot))
					{
						bDidILoadThisDictionary = true;

						g_ClipDictionaryStore.StreamingRequest(iClipDictionarySlot, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE | STRFLAG_PRIORITY_LOAD);
						CStreaming::LoadAllRequestedObjects();
					}

					iClipDictionarySlot = g_ClipDictionaryStore.HasObjectLoaded(iClipDictionarySlot) ? iClipDictionarySlot : strLocalIndex(-1);
					SCRIPT_ASSERT(iClipDictionarySlot != -1, "IS_ARREST_TYPE_VALID - Could not load clip dictionary!");
					if(iClipDictionarySlot != -1)
					{
						pClipDictionary = g_ClipDictionaryStore.Get(iClipDictionarySlot);
						iClipDictionarySlot = pClipDictionary ? iClipDictionarySlot : strLocalIndex(-1);
						SCRIPT_ASSERT(iClipDictionarySlot != -1, "IS_ARREST_TYPE_VALID - Could not get clip dictionary!");
					}

					if(iClipDictionarySlot == -1)
					{
						bDidILoadThisDictionary = false;
					}
				}
			}
		}

		// Test arrest scenes

		if(pClipDictionary)
		{
			// Get scene position
			Vector3 vStartPosition = VEC3V_TO_VECTOR3(pCrookPed->GetTransform().GetPosition());
			bool bFoundGround = false;
			float fGroundZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, vStartPosition, &bFoundGround);
			if(bFoundGround)
			{
				vStartPosition.z = fGroundZ;
			}

			// Get scene rotation
			Vector3 vRoot = VEC3V_TO_VECTOR3(pCopPed->GetTransform().GetPosition());
			Vector3 vSpine3 = VEC3V_TO_VECTOR3(pCrookPed->GetTransform().GetPosition());
			Vector3 vOrientation = vSpine3 - vRoot;
			float fHeading = fwAngle::GetATanOfXY(vOrientation.x, vOrientation.y);
			fHeading -= (PI * 0.5f);
			while (fHeading < 0.0f)
			{
				fHeading += (PI * 2.0f);
			}
			Quaternion vStartRotation(Quaternion::IdentityType); vStartRotation.FromEulers(Vector3(0.0f, 0.0f, fHeading));


			//grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(vStartPosition), 1.0f, Color_white, false);

			// Get capsule size
			const CBaseCapsuleInfo *pCapsuleInfo = pCopPed->GetCapsuleInfo();
			const CBipedCapsuleInfo *pBipedCapsuleInfo = pCapsuleInfo->IsBiped() ? pCapsuleInfo->GetBipedCapsuleInfo() : NULL;
			const float fCapsuleHeight = pBipedCapsuleInfo->GetHeadHeight();
			const float fCapsuleRadius = pBipedCapsuleInfo->GetRadius();

			// Get entities to exclude
			const int iNumEntitiesToExclude = 2;
			const CEntity *ppEntitiesToExclude[iNumEntitiesToExclude] = { pCopPed, pCrookPed };

			// Setup arrest scenes to be tested
			struct ARREST_SCENE
			{
				ARREST_SCENE(const atHashString &copClipName, const atHashString &crookClipName, const atHashString &cameraClipName)
					: m_CopClipName(copClipName), m_CrookClipName(crookClipName), m_CameraClipName(cameraClipName)
				{
				}

				const atHashString m_CopClipName;
				const atHashString m_CrookClipName;
				const atHashString m_CameraClipName;
			};

			const ARREST_SCENE pArrestScenes[] = {
				ARREST_SCENE( atHashString("a_arrest_on_floor",0x2A0ABB8C), atHashString("b_arrest_on_floor",0xB188DA46), atHashString("CAM_B_ARREST_ON_FLOOR",0x3E367384) ),
				ARREST_SCENE( atHashString("A_UNCUFF",0xCA3A1E7A), atHashString("B_UNCUFF",0x14B1DBE3), atHashString("CAM_UNCUFF",0x57E2DD4C) )
			};

			const ARREST_SCENE &arrestScene = pArrestScenes[ArrestType];

			// Get arrest scene clips
			const crClip *pCopClip = pClipDictionary->GetClip(arrestScene.m_CopClipName.GetHash());
			SCRIPT_ASSERT(pCopClip, "IS_ARREST_TYPE_VALID - Could not get cop clip!");
			const crClip *pCrookClip = pClipDictionary->GetClip(arrestScene.m_CrookClipName.GetHash());
			SCRIPT_ASSERT(pCrookClip, "IS_ARREST_TYPE_VALID - Could not get crook clip!");
			const crClip *pCameraClip = pClipDictionary->GetClip(arrestScene.m_CameraClipName.GetHash());
			SCRIPT_ASSERT(pCameraClip, "IS_ARREST_TYPE_VALID - Could not get camera clip!");

			bool bAnyHits = false;

			if(pCopClip)
			{
				// Test cop clip mover against world
				bAnyHits |= TestClipMoverAgainstWorld(pCopClip, vStartPosition, vStartRotation, fCapsuleHeight, fCapsuleRadius, ppEntitiesToExclude, iNumEntitiesToExclude);
				if(bAnyHits) { taskDisplayf("IS_ARREST_TYPE_VALID - %s hit world!", pCopClip->GetName()); }
			}

			if(pCrookClip && !bAnyHits)
			{
				// Test crook clip mover against world
				bAnyHits |= TestClipMoverAgainstWorld(pCrookClip, vStartPosition, vStartRotation, fCapsuleHeight, fCapsuleRadius, ppEntitiesToExclude, iNumEntitiesToExclude);
				if(bAnyHits) { taskDisplayf("IS_ARREST_TYPE_VALID - %s hit world!", pCrookClip->GetName()); }
			}

			if(pCameraClip && !bAnyHits)
			{
				// Test camera clip camera against world
				bAnyHits |= TestClipCameraAgainstWorld(pCameraClip, vStartPosition, vStartRotation, fCapsuleRadius, ppEntitiesToExclude, iNumEntitiesToExclude);
				if(bAnyHits) { taskDisplayf("IS_ARREST_TYPE_VALID - %s hit world!", pCameraClip->GetName()); }
			}

			// If there are no hits return true
			bResult = !bAnyHits;
		}

		// Release clip dictionary

		if(bDidILoadThisDictionary)
		{
			g_ClipDictionaryStore.ClearRequiredFlag(iClipDictionarySlot.Get(), STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE | STRFLAG_PRIORITY_LOAD);
		}
	}
	else
	{
		bResult = true;
	}

	return bResult;
}

#if __BANK

//////////////////////////////////////////////////////////////////////////

float CArrestDebug::GetPairedAnimOverride()
{
	TUNE_GROUP_FLOAT(ARREST, fPairedAnimOverride, -1.0f, -1.0f, PI * 2.0f, 0.01f);
	return fPairedAnimOverride;
}

//////////////////////////////////////////////////////////////////////////

void CArrestDebug::SetupWidgets(bkBank& bank)
{
	bank.PushGroup("Arresting", false);

		bank.AddButton("Toggle Handcuffed",			ToggleHandcuffed);
		bank.AddButton("Toggle Can Be Arrested",	ToggleCanBeArrested);
		bank.AddButton("Toggle Can Arrest",			ToggleCanArrest);
		bank.AddButton("Toggle Can Uncuff",			ToggleCanUnCuff);
		bank.AddButton("Remove From Custody",		RemoveFromCustody);
		bank.AddButton("Local Ped Take Custody",	LocalPedTakeCustody);
		bank.AddButton("Create Arrestable Ped",		CreateArrestablePed);
		bank.AddButton("Create Uncuffable Ped",		CreateUncuffablePed);
		bank.AddButton("Incapacitated focus ped",   IncapacitatedFocusPed);
		bank.AddButton("Set Group - Cop",			SetRelationshipGroupToCop);
		bank.AddButton("Set Group - Gang",			SetRelationshipGroupToGang);

	bank.PopGroup(); // "arresting"
}

void CArrestDebug::ToggleHandcuffed()
{
	ToggleConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed);
}

void CArrestDebug::ToggleCanBeArrested()
{
	ToggleConfigFlag(CPED_CONFIG_FLAG_CanBeArrested);
}

void CArrestDebug::ToggleCanArrest()
{
	ToggleConfigFlag(CPED_CONFIG_FLAG_CanPerformArrest);
}

void CArrestDebug::ToggleCanUnCuff()
{
	ToggleConfigFlag(CPED_CONFIG_FLAG_CanPerformUncuff);
}

void CArrestDebug::RemoveFromCustody()
{
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus)
	{
		pFocus->SetInCustody(false, NULL);
	}
}

void CArrestDebug::ToggleConfigFlag(int nFlag)
{
	CPed * pPedToToggle = CPedDebugVisualiserMenu::GetFocusPed();

	if(!pPedToToggle)
	{
		pPedToToggle = CGameWorld::FindLocalPlayer();
	}

	if(pPedToToggle)
	{
		const ePedConfigFlags flag = (const ePedConfigFlags)nFlag;
		pPedToToggle->SetPedConfigFlag(flag, !pPedToToggle->GetPedConfigFlag(flag));
	}
}

void CArrestDebug::LocalPedTakeCustody()
{
#if ENABLE_TASKS_ARREST_CUFFED
	CPed * pCustodyPed = CPedDebugVisualiserMenu::GetFocusPed();
	CPed * pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(pLocalPlayer && 
		pCustodyPed && 
		!pCustodyPed->IsNetworkClone() && 
		(pLocalPlayer != pCustodyPed))
	{
		pCustodyPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed, true);
		pCustodyPed->SetInCustody(true, pLocalPlayer);
		CTaskInCustody* pTaskInCustody = rage_new CTaskInCustody(pLocalPlayer, true);
		CEventGivePedTask event(PED_TASK_PRIORITY_DEFAULT, pTaskInCustody, false, E_PRIORITY_INCUSTODY);
		pCustodyPed->GetPedIntelligence()->AddEvent(event, true);
	}
#endif // ENABLE_TASKS_ARREST_CUFFED
}

//////////////////////////////////////////////////////////////////////////

void CArrestDebug::CreateArrestablePed()
{
	CPed * pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(pLocalPlayer)
	{
		pLocalPlayer->SetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformArrest, true);
		CRelationshipGroup *pGroup = CRelationshipManager::FindRelationshipGroup(ATSTRINGHASH("COP", 0xA49E591C));
		if(pGroup)
		{
			pLocalPlayer->GetPedIntelligence()->SetRelationshipGroup(pGroup);
		}
	}

	CPed::CreateBank();
	CPedVariationDebug::CreatePedCB();
	CPed* pPed = CPedFactory::GetLastCreatedPed();
	if (pPed)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested, true);
		CRelationshipGroup *pGroup = CRelationshipManager::FindRelationshipGroup(ATSTRINGHASH("AMBIENT_GANG_MEXICAN", 0x11A9A7E3));
		if(pGroup)
		{
			pPed->GetPedIntelligence()->SetRelationshipGroup(pGroup);
		}

		pPed->GetPedIntelligence()->FlushImmediately( false );
		pPed->GetPedIntelligence()->AddTaskDefault( rage_new CTaskDoNothing( -1 ) );
	}
}

void CArrestDebug::CreateUncuffablePed()
{
	CPed * pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(pLocalPlayer)
	{
		pLocalPlayer->SetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformArrest, false);
		pLocalPlayer->SetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformUncuff, true);
		CRelationshipGroup *pGroup = CRelationshipManager::FindRelationshipGroup(ATSTRINGHASH("AMBIENT_GANG_MEXICAN", 0x11A9A7E3));
		if(pGroup)
		{
			pLocalPlayer->GetPedIntelligence()->SetRelationshipGroup(pGroup);
		}
	}

	CPed::CreateBank();
	CPedVariationDebug::CreatePedCB();
	CPed* pPed = CPedFactory::GetLastCreatedPed();
	if (pPed)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested, true);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed, true);
		CRelationshipGroup *pGroup = CRelationshipManager::FindRelationshipGroup(ATSTRINGHASH("AMBIENT_GANG_MEXICAN", 0x11A9A7E3));
		if(pGroup)
		{
			pPed->GetPedIntelligence()->SetRelationshipGroup(pGroup);
		}

		pPed->GetPedIntelligence()->FlushImmediately( false );
		pPed->GetPedIntelligence()->AddTaskDefault( rage_new CTaskDoNothing( -1 ) );
	}
}

void CArrestDebug::IncapacitatedFocusPed()
{
#if CNC_MODE_ENABLED
	CPed* pFocusPed = CPedDebugVisualiserMenu::GetFocusPed();
	if (pFocusPed)
	{
		CEventIncapacitated eventIncapacitated;
		pFocusPed->GetPedIntelligence()->AddEvent(eventIncapacitated);
	}
#endif
}

void CArrestDebug::SetRelationshipGroupToCop()
{
	CPed * pPed = CPedDebugVisualiserMenu::GetFocusPed();
	if(pPed)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformArrest, true);
		CRelationshipGroup *pGroup = CRelationshipManager::FindRelationshipGroup(ATSTRINGHASH("COP", 0xA49E591C));
		if(pGroup)
		{
			pPed->GetPedIntelligence()->SetRelationshipGroup(pGroup);
		}
	}
}

void CArrestDebug::SetRelationshipGroupToGang()
{
	CPed * pPed = CPedDebugVisualiserMenu::GetFocusPed();
	if(pPed)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformUncuff, true);
		CRelationshipGroup *pGroup = CRelationshipManager::FindRelationshipGroup(ATSTRINGHASH("COP", 0xA49E591C));
		if(pGroup)
		{
			pPed->GetPedIntelligence()->SetRelationshipGroup(pGroup);
		}
	}
}

#endif