#include "PropManagementHelper.h"

#include "streaming/streamingvisualize.h"

#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/task/taskchannel.h"
#include "modelinfo/BaseModelInfo.h"
#include "Peds/ped.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/info/ScenarioInfoManager.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Weapons/components/WeaponComponentFactory.h"
#include "Weapons/components/WeaponComponentManager.h"
#include "weapons/inventory/PedWeaponManager.h"

AI_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

////////////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CPropManagementHelper, CONFIGURED_FROM_FILE, atHashString("CPropManagementHelper",0x8da54456));


CPropManagementHelper::CPropManagementHelper() 
	: m_PropStreamStatus(PS_NotRequired)
	, m_PropInEnvironment(NULL)
	, m_PropStreamingObjectNameHash(strStreamingObjectName::Null())
	, m_Flags(0)
{
}

CPropManagementHelper::~CPropManagementHelper()
{
	ReleasePropStreamingRef();
}

void CPropManagementHelper::ReleasePropStreamingRef()
{
	if( m_PropStreamStatus == PS_LoadedAndReffed )
	{
		if ( taskVerifyf ( m_PropStreamingObjectNameHash.IsNotNull(), "We should have valid prop here" ) )
		{
			fwModelId propModelId;
			CBaseModelInfo* pPropModelInfo = CModelInfo::GetBaseModelInfoFromName(m_PropStreamingObjectNameHash, &propModelId);
			if ( taskVerifyf ( pPropModelInfo, "We should have valid prop model here" ) )
			{
				pPropModelInfo->RemoveRef();
			}
		}
	}

	m_PropStreamingObjectNameHash.Clear();
	m_PropStreamStatus = PS_NotRequired;

	// It looks like we don't expect this flag to be set unless we are in our PS_Loading
	// state, which is no longer the case if we got here. See
	// B* 1738976: "Ignorable Assert - PropManagementHelper.cpp(399): [ai_task] Error: Assert(IsPropLoading()) FAILED",
	// where the m_PropStreamingObjectNameHash.IsNotNull() assert also failed right after the IsPropLoading() assert,
	// further indicating that we would have probably gone through this function.
	m_Flags.ClearFlag(Flag_ForcedLoading);
}

void CPropManagementHelper::ReleasePropRefIfUnshared(CPed* pPed)
{
	if ( !IsPropSharedAmongTasks() )
	{
		if( IsHoldingProp() && pPed )
		{
			DisposeOfProp(*pPed);
		} 
		else
		{
			SetHoldingProp(pPed, false);
		}
		ReleasePropStreamingRef();
	}
}

void CPropManagementHelper::SetHoldingProp(CPed* ped, bool bHolding)
{
	if (bHolding) 
	{ 
		m_Flags.SetFlag(Flag_HoldingProp); 
	} 
	else 
	{ 
		m_Flags.ClearFlag(Flag_HoldingProp); 
	} 
	if (ped)
	{
		ped->SetPedConfigFlag(CPED_CONFIG_FLAG_IsHoldingProp, bHolding);
	}
}

void CPropManagementHelper::Process(Context& in_Context)
{
	ProcessPropLoading(in_Context);
	ProcessClipEvents(in_Context);
	ProcessReattemptWeaponObjectInfo(in_Context);
}

void CPropManagementHelper::ProcessReattemptWeaponObjectInfo (Context& in_Context)
{
	if (!m_Flags.IsFlagSet(Flag_ReattemptSetWeaponObjectInfo))
	{
		return;
	}
	else
	{
		CPedWeaponManager* pWeaponMgr = in_Context.m_Ped.GetWeaponManager();
		if ( weaponVerifyf(pWeaponMgr, "CPropManagementHelper being processed on a ped without a weapon manager") )
		{
			CObject* pWeaponObject = pWeaponMgr->GetEquippedWeaponObject();
			if( pWeaponObject )
			{
				SetWeaponObjectPropInfo(pWeaponObject, in_Context.m_Ped);
				m_Flags.ClearFlag(Flag_ReattemptSetWeaponObjectInfo);
			}
		}
	}
}

void CPropManagementHelper::ProcessClipEvents(Context& in_Context)
{
	if( IsPropLoaded() )
	{
		// check if our create prop flag is set
		if( m_Flags.IsFlagSet(Flag_MoveCreatePropTag) )
		{
			HandleClipEventCreateProp(in_Context);

			if( m_PropStreamingObjectNameHash.IsNotNull() )
			{
				bool hasDestroyTag = false;
				bool hasReleaseTag = false;
				HasDestroyOrReleaseTag(in_Context, hasDestroyTag, hasReleaseTag);

				if( !hasDestroyTag && !hasReleaseTag)
				{
					m_Flags.SetFlag(Flag_PropPersistsOverClips);
					taskAssert(m_PropStreamingObjectNameHash.GetHash() != 0);
				}
			}
		}
		else
		{
			// check if our destroy prop flag is set
			if( m_Flags.IsFlagSet(Flag_MoveDestroyPropTag) )
			{
				HandleClipEventDestroyProp(in_Context);
			}
			// check if our release prop flag is set
			else if( m_Flags.IsFlagSet(Flag_MoveReleasePropTag) )
			{
				HandleClipEventReleaseProp(in_Context);
			}
		}

		// check if our flashlight flag is set
		if ( m_Flags.IsFlagSet(Flag_MoveFlashlightTag) )
		{
			// Scan for Flashlight events [1/24/2013 mdawe]
			if ( in_Context.m_ClipHelper )
			{
				const crClip* pClip = in_Context.m_ClipHelper->GetClip();
				if (pClip && pClip->GetTags())
				{
					crTagIterator it(*pClip->GetTags(), in_Context.m_ClipHelper->GetLastPhase(), in_Context.m_ClipHelper->GetPhase(), CClipEventTags::FlashLight);
					while (*it)
					{
						// For each Flashlight tag, get the name and on/off attributes [1/24/2013 mdawe]
						const crTag* pTag = *it;			
						const crPropertyAttribute* nameAttrib = pTag->GetProperty().GetAttribute(CClipEventTags::FlashLightName);
						const crPropertyAttribute* onAttrib		= pTag->GetProperty().GetAttribute(CClipEventTags::FlashLightOn);

						if ( nameAttrib && nameAttrib->GetType() == crPropertyAttribute::kTypeHashString &&
							onAttrib		&& onAttrib->GetType()   == crPropertyAttribute::kTypeBool )
						{
							const crPropertyAttributeHashString* nameHashAttrib = static_cast<const crPropertyAttributeHashString*>(nameAttrib);
							const crPropertyAttributeBool*			 onBoolAttrib		= static_cast<const crPropertyAttributeBool*>(onAttrib);

							HandleClipEventFlashLight(in_Context, nameHashAttrib->GetHashString(), onBoolAttrib->GetBool());
						}

						++it;
					}
				}
			}
		}
	}
}

void CPropManagementHelper::ProcessMoveSignals(Context& in_Context)
{
	// if our prop is loaded
	if( IsPropLoaded() )
	{
		// if we have a valid clip helper
		if ( in_Context.m_ClipHelper )
		{
			// check if we have a prop creation tag
			if( in_Context.m_ClipHelper->IsEvent<crPropertyAttributeBool>(CClipEventTags::Object, CClipEventTags::Create, true) )
			{
				m_Flags.SetFlag(Flag_MoveCreatePropTag);
			}
			else
			{
				// check for destroy tags
				if( in_Context.m_ClipHelper->IsEvent<crPropertyAttributeBool>(CClipEventTags::Object, CClipEventTags::Destroy, true) )
				{
					m_Flags.SetFlag(Flag_MoveDestroyPropTag);
				}
				// check for release tags
				else if( in_Context.m_ClipHelper->IsEvent<crPropertyAttributeBool>(CClipEventTags::Object, CClipEventTags::Release, true) )
				{
					m_Flags.SetFlag(Flag_MoveReleasePropTag);
				}
			}
		
			// check if we have a flashlight on/off tag
			if ( in_Context.m_ClipHelper->IsEvent<crPropertyAttributeBool>(CClipEventTags::FlashLight, CClipEventTags::FlashLightOn, true) )
			{
				m_Flags.SetFlag(Flag_MoveFlashlightTag);
			}
			else if ( in_Context.m_ClipHelper->IsEvent<crPropertyAttributeBool>(CClipEventTags::FlashLight, CClipEventTags::FlashLightOn, false) )
			{
				m_Flags.SetFlag(Flag_MoveFlashlightTag);
			}
		}
	}
}

void CPropManagementHelper::ProcessClipEnding (Context& in_Context)
{
	bool hasDestroyTag = false;
	bool hasReleaseTag = false;
	HasDestroyOrReleaseTag(in_Context, hasDestroyTag, hasReleaseTag);

	if (hasDestroyTag)
	{
		HandleClipEventDestroyProp(in_Context);
	}
	else if (hasReleaseTag)
	{
		HandleClipEventReleaseProp(in_Context);
	}
}


void CPropManagementHelper::HandleClipEventCreateProp(Context& in_Context)
{
	// Clear our create flag
	m_Flags.ClearFlag(Flag_MoveCreatePropTag);

	//Allow net ped to change equipped prop status
	CNetObjPed     *netObjPed  = static_cast<CNetObjPed *>(in_Context.m_Ped.GetNetworkObject());
	bool bNetPedOverridingProps = false;
	if(netObjPed)
	{
		bNetPedOverridingProps = netObjPed->GetTaskOverridingProps();
		netObjPed->SetTaskOverridingProps(true);
	}

	if( weaponVerifyf(in_Context.m_Ped.GetInventory(), "Not expected to get here with no inventory (%s)", in_Context.m_Ped.GetModelName() ) )
	{
		if (UsePropFromEnvironment() && m_PropInEnvironment)
		{
			GrabPropFromEnvironment(in_Context.m_Ped);
		}
		else
		{
			CWeaponItem* pItem = in_Context.m_Ped.GetInventory()->AddWeapon( OBJECTTYPE_OBJECT );
			if( pItem )
			{
				pItem->SetModelHash( m_PropStreamingObjectNameHash.GetHash() );

				if ( weaponVerifyf(in_Context.m_Ped.GetWeaponManager(), "Not expected to get here with no weaponmanager (%s)", in_Context.m_Ped.GetModelName() ) )
				{
					if( in_Context.m_Ped.GetWeaponManager()->EquipWeapon( OBJECTTYPE_OBJECT ) )
					{
						in_Context.m_Ped.GetWeaponManager()->CreateEquippedWeaponObject( IsCreatePropInLeftHand() ? CPedEquippedWeapon::AP_LeftHand : CPedEquippedWeapon::AP_RightHand );
						if( in_Context.m_Ped.GetWeaponManager()->GetEquippedWeaponObject() )
						{
							in_Context.m_Ped.GetWeaponManager()->GetEquippedWeaponObject()->m_nObjectFlags.bAmbientProp = true;

							CObject* pProp = in_Context.m_Ped.GetWeaponManager()->GetEquippedWeaponObject();
							if( pProp )
							{
								SetHoldingProp(&in_Context.m_Ped, true);
							}
						}
					}
				}
			}
		}
		taskAssertf(IsHoldingProp(),"%s m_PropStreamingObjectNameHash %x, name %s ",in_Context.m_Ped.GetDebugName(), m_PropStreamingObjectNameHash.GetHash(), m_PropStreamingObjectNameHash.GetCStr()) ;
	}

	//Put net ped override prop status back how it was when entering function
	if(netObjPed)
	{
		netObjPed->SetTaskOverridingProps(bNetPedOverridingProps);
	}
}

void CPropManagementHelper::HandleClipEventDestroyProp(Context& in_Context)
{
	// Clear our destroy flag
	m_Flags.ClearFlag(Flag_MoveDestroyPropTag);

	if( weaponVerifyf(in_Context.m_Ped.GetInventory(), "Not expected to get here with no inventory (%s)", in_Context.m_Ped.GetModelName() ) )
	{
		in_Context.m_Ped.GetInventory()->RemoveWeapon( OBJECTTYPE_OBJECT );
	}

	if ( weaponVerifyf(in_Context.m_Ped.GetWeaponManager(), "Not expected to get here with no weaponmanager (%s)", in_Context.m_Ped.GetModelName() ) )
	{
		in_Context.m_Ped.GetWeaponManager()->SetWeaponInstruction(CPedWeaponManager::WI_AmbientTaskDestroyWeapon);
	}

	SetDestroyProp(false);
	SetHoldingProp(&in_Context.m_Ped, false);
	ReleasePropStreamingRef();
}

void CPropManagementHelper::HandleClipEventReleaseProp(Context& in_Context)
{
	// Clear our release flag
	m_Flags.ClearFlag(Flag_MoveReleasePropTag);

	if ( weaponVerifyf(in_Context.m_Ped.GetWeaponManager(), "Not expected to get here with no weaponmanager (%s)", in_Context.m_Ped.GetModelName() ) )
	{
		const bool bActivatePhysics = (!UsePropFromEnvironment() && !DontActivatePhysicsOnRelease()); //If putting back into the environment, don't activate physics on the object
		DropProp(in_Context.m_Ped, bActivatePhysics);
		in_Context.m_Ped.GetWeaponManager()->SetWeaponInstruction(CPedWeaponManager::WI_AmbientTaskDropAndRevertToStored);
	}

	SetDestroyProp(false);
	SetHoldingProp(&in_Context.m_Ped, false);
	ReleasePropStreamingRef();
}

void CPropManagementHelper::HandleClipEventFlashLight(Context& in_Context, atHashString lightHash, bool bOnOff)
{
	// Clear our flashlight flag
	m_Flags.ClearFlag(Flag_MoveFlashlightTag);

	CPedWeaponManager* pPedWeaponMgr = in_Context.m_Ped.GetWeaponManager();
	if (pPedWeaponMgr)
	{
		// Grab the weapon information and the weapon object. [1/24/2013 mdawe]
		CWeapon* pEquippedWeapon = pPedWeaponMgr->GetEquippedWeapon();
		CObject* pWeaponObject = pPedWeaponMgr->GetEquippedWeaponObject();
		if (pEquippedWeapon && pWeaponObject)
		{
			// First, check to see if the flashlight component exists.
			CWeaponComponentFlashLight* pFlashLight = pEquippedWeapon->GetFlashLightComponent();
			if (!pFlashLight)
			{
				// No component yet, so create it [1/24/2013 mdawe]
				const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(lightHash);
				if (pComponentInfo)
				{
					CWeaponComponent* pComponent = WeaponComponentFactory::Create(pComponentInfo, pWeaponObject->GetWeapon(), pWeaponObject);
					if (pComponent)
					{
						pWeaponObject->GetWeapon()->AddComponent(pComponent);
					}
				}
			}

			// If we have or created a flashlight component, turn it on or off. [1/24/2013 mdawe]
			if (pFlashLight)
			{
				pFlashLight->SetActive(bOnOff);
			}
		}
	}
}

void CPropManagementHelper::HasDestroyOrReleaseTag(Context& in_Context, bool& hasDestroyOut, bool& hasReleaseOut) const
{
	hasReleaseOut = false;
	hasDestroyOut = false;
	if (in_Context.m_ClipHelper && in_Context.m_ClipHelper->GetClip() && in_Context.m_ClipHelper->GetClip()->GetTags())
	{ 
		CClipEventTags::CTagIterator<CClipEventTags::CObjectEventTag> it(*in_Context.m_ClipHelper->GetClip()->GetTags(), in_Context.m_ClipHelper->GetPhase(), 1.0f, CClipEventTags::Object);

		while(*it)
		{
			hasReleaseOut |= (*it)->GetData()->IsRelease();
			hasDestroyOut |= (*it)->GetData()->IsDestroy();

			++it;
		}
	}
}


void CPropManagementHelper::ProcessPropLoading(Context& in_Context)
{
	if (IsPropLoading() || IsForcedLoading())
	{
		STRVIS_AUTO_CONTEXT(strStreamingVisualize::PROPMGMT);
		taskAssert(IsPropLoading());

		if( taskVerifyf( m_PropStreamingObjectNameHash.IsNotNull(), "Props in loading state but no prop specified" ) )
		{
			// Get the model index
			fwModelId propModelId;
			CBaseModelInfo *pPropModelInfo = CModelInfo::GetBaseModelInfoFromName(m_PropStreamingObjectNameHash, &propModelId);
			if (pPropModelInfo)
			{
				// Make sure the prop model has loaded
				if( CModelInfo::HaveAssetsLoaded( propModelId ) )
				{
					pPropModelInfo->AddRef();
					m_PropStreamStatus = PS_LoadedAndReffed;
					if (m_Flags.IsFlagSet(Flag_ForcedLoading))
					{
						GivePedProp(in_Context.m_Ped);
						m_Flags.SetFlag(Flag_PropPersistsOverClips);
					}
					m_Flags.ClearFlag(Flag_ForcedLoading);
				}
				else
				{
					CModelInfo::RequestAssets( propModelId, 0 );
				}
			}
		}
		else
		{
			m_PropStreamStatus = PS_NotRequired;
		}
	}
}

void CPropManagementHelper::RequestProp(const strStreamingObjectName& in_PropStreamingObjectName)
{
	if ( !IsPropLoading() && !IsPropLoaded() )
	{
		m_PropStreamStatus = PS_Loading;
		m_PropStreamingObjectNameHash = in_PropStreamingObjectName;
	}
}

bool CPropManagementHelper::PickPermanentProp (CPed* pPed, const CConditionalAnimsGroup* pConditionalAnimsGroup, s16& iInOutConditionalAnimChosen, bool bForcePick) 
{
	if (!pPed)
	{
		return false;
	}

	u32 uiPropHash = 0;
	int animIndexWithinGroup = -1;

	s32 iFlags = 0;
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = pPed;
	conditionData.iScenarioFlags = iFlags;
	bool choseProp = false;
#if __BANK
	//if (in_Context.m_Flags.IsFlagSet(Flag_UseDebugAnimData))
	//{
	//	//Need to make sure that a prop is selected or not ... perhaps we did not have one selected.
	//	if (m_DebugInfo.m_PropName.GetHash())
	//	{
	//		uiPropHash = m_DebugInfo.m_PropName.GetHash();
	//		animIndexWithinGroup = m_ConditionalAnimChosen;//This seems to equate to this value when doing our override ... 
	//		choseProp = true;
	//	}
	//}
	//else
#endif
	{
		choseProp = CAmbientAnimationManager::RandomlyChoosePropForSpawningPed( conditionData, pConditionalAnimsGroup, uiPropHash, animIndexWithinGroup, iInOutConditionalAnimChosen);
	}

	if (choseProp)
	{
		float fProbability = 0.0f;
		bool  bLeftHand			= false;
		bool  bDestroyProp	= false;

		if (pConditionalAnimsGroup && animIndexWithinGroup >= 0)
		{
			const CConditionalAnims* pAnims = pConditionalAnimsGroup->GetAnims(animIndexWithinGroup);
			fProbability = pAnims->GetChanceOfSpawningWithAnything();
			bLeftHand    = pAnims->GetCreatePropInLeftHand();
			bDestroyProp = pAnims->ShouldDestroyPropInsteadOfDropping();
		}		

		if (bForcePick)
		{
			fProbability = 1.0f;
		}

#if __BANK
		//if (m_Flags.IsFlagSet(Flag_UseDebugAnimData))
		//{
		//	//probability should not be a factor here as we have debug chosen a prop to begin with.
		//	aiAssert(uiPropHash);
		//	fProbability = 1.0f;
		//}
#endif

#if __DEV
		TUNE_GROUP_BOOL(AMBIENT_DEBUG, FORCE_PROP_SPAWNING, false);
		if (FORCE_PROP_SPAWNING)
		{
			fProbability = 1.0f;
		}
#endif

		if( fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < fProbability )
		{
			Assert(iInOutConditionalAnimChosen < 0 || iInOutConditionalAnimChosen == animIndexWithinGroup);
			iInOutConditionalAnimChosen = static_cast<s16>(animIndexWithinGroup);

			SetCreatePropInLeftHand(bLeftHand);
			SetDestroyProp(bDestroyProp);

			// If this fails, it can probably be fixed by calling ReleasePropStreamingRef().
			taskAssertf(m_PropStreamStatus != PS_LoadedAndReffed, "CPropManagmentHelper::PickPermanentProp called with streaming status = PS_LoadedAndReffed. ConditionalAnimIndex = %d, propHash = %u", iInOutConditionalAnimChosen, uiPropHash);

			m_PropStreamStatus = PS_Loading;
			SetForcedLoading();
			m_PropStreamingObjectNameHash.SetHash(uiPropHash);
		}
	}
	return choseProp;
}


void CPropManagementHelper::UpdateExistingOrMissingProp (CPed& ped)
{
	CPedWeaponManager* mgr = ped.GetWeaponManager();
	if (mgr)
	{
		// If an old ambient task has left the ped with a weapon, claim it for this task
		if(IsPropNotRequired())
		{
			const CPedWeaponManager::eWeaponInstruction instr = mgr->GetWeaponInstruction();
			if( instr == CPedWeaponManager::WI_AmbientTaskDropAndRevertToStored ||
				instr == CPedWeaponManager::WI_AmbientTaskDropAndRevertToStored2ndAttempt ||
				instr == CPedWeaponManager::WI_RevertToStoredWeapon ||
				instr == CPedWeaponManager::WI_AmbientTaskDestroyWeapon ||
				instr == CPedWeaponManager::WI_AmbientTaskDestroyWeapon2ndAttempt )
			{
				SetDestroyProp( (instr == CPedWeaponManager::WI_AmbientTaskDestroyWeapon || instr == CPedWeaponManager::WI_AmbientTaskDestroyWeapon2ndAttempt) );

				// Clear the instruction
				mgr->SetWeaponInstruction( CPedWeaponManager::WI_None );

				// Update status
				SetHoldingProp(&ped, true);
			}
		}

		// If we have lost our object, update our prop status
		if (IsHoldingProp())
		{
			if(mgr->GetEquippedWeaponHash() != OBJECTTYPE_OBJECT)
			{
				// We either no longer have the object, or we have changed our weapon
				SetHoldingProp(&ped, false);
			}
		}
	}
}


void CPropManagementHelper::SetPropWeaponInstructionForDisposal(CPed& ped)
{
	if(IsHoldingProp())
	{
		if ( taskVerifyf(ped.GetWeaponManager(), "Expected weapon manager for ped, since it supposedly is holding a prop."))
		{
			if(ShouldDestroyProp())
			{
				ped.GetWeaponManager()->SetWeaponInstruction(CPedWeaponManager::WI_AmbientTaskDestroyWeapon);
			}
			else
			{
				ped.GetWeaponManager()->SetWeaponInstruction(CPedWeaponManager::WI_AmbientTaskDropAndRevertToStored);
			}
		}
	}
}

bool CPropManagementHelper::DisposeOfPropIfInappropriate(CPed& ped, s32 iScenarioIndex, CPropManagementHelper* pPropManagementHelper)
{
	CPedWeaponManager* mgr = ped.GetWeaponManager();
	if(!mgr)
	{
		return false;
	}

	// If the prop is a weapon or similar non-prop weapons, ignore if the scenario allows it,
	// otherwise destroy it.
	if(mgr->GetEquippedWeaponHash() != OBJECTTYPE_OBJECT)
	{
		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(iScenarioIndex);

		if (!pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::AllowEquippedWeapons))
		{
			const bool bAllowDrop = false;
			mgr->DestroyEquippedWeaponObject(bAllowDrop);
		}
		return false;
	}

	// Make sure we have a CObject, which is marked with bAmbientProp.
	CObject* pObject = mgr->GetEquippedWeaponObject();
	if(!pObject || !pObject->m_nObjectFlags.bAmbientProp)
	{
		return false;
	}

	// Get the scenario info.
	const CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(iScenarioIndex);
	if(!pScenarioInfo)
	{
		return false;
	}

	// Get the conditional animation group.
	const CConditionalAnimsGroup* pGrp = pScenarioInfo->GetClipData();
	if(!pGrp)
	{
		return false;
	}

	// Get the model hash of the object we are holding.
	CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo();
	if(!pModelInfo)
	{
		return false;
	}
	u32 propModelHash = pModelInfo->GetModelNameHash();

	// Loop over the conditional animations and see if our prop is in one of
	// the prop model sets.
	// Note: it's possible that the scenario might be able to deal with other props too,
	// using conditions on the prop type, but at least for now it's probably safest to
	// dispose of anything that's not explicitly requested.
	bool propIsAppropriate = false;
	const int numAnims = pGrp->GetNumAnims();
	
	// If this group has no specified conditional anims, allow the existing prop [4/24/2013 mdawe]
	if (numAnims == 0)
	{
		return false;
	}

	for(int i = 0; i < numAnims; i++)
	{
		const CConditionalAnims* pAnims = pGrp->GetAnims(i);
		if(!pAnims)
		{
			continue;
		}
		u32 propSetHash = pAnims->GetPropSetHash();
		if(!propSetHash)
		{
			continue;
		}
		const CAmbientModelSet* pPropSet = CAmbientAnimationManager::GetPropSetFromHash(propSetHash);
		if(!pPropSet)
		{
			continue;
		}
		if(pPropSet->GetContainsModel(propModelHash))
		{
			// Found a prop set containing the model.
			propIsAppropriate = true;
			break;
		}
	}

	// If the prop is in one of the prop sets, don't drop it.
	if(propIsAppropriate)
	{
		return false;
	}

	if (pPropManagementHelper)
	{
		// Drop or destroy the prop.
		pPropManagementHelper->DisposeOfProp(ped);
	}
	return true;
}


void CPropManagementHelper::DisposeOfProp(CPed& ped)
{
	CPedWeaponManager* mgr = ped.GetWeaponManager();
	if(!mgr)
	{
		return;
	}

	// Make sure we have a CObject
	CObject* pObject = mgr->GetEquippedWeaponObject();
	if (!pObject)
	{
		// Any place where we would SetHoldingProp to true requires a pObject. 
		// If the object isn't there anymore, it's been cleaned up already and this ped is clearly not holding it any longer. [1/7/2013 mdawe]
		SetHoldingProp(&ped, false);
		return;
	}

	// Make sure we don't dispose an object that's a weapon or similar non-prop weapon. Allow anything marked as an ambient prop.
	if(mgr->GetEquippedWeaponHash() != OBJECTTYPE_OBJECT && !pObject->m_nObjectFlags.bAmbientProp)
	{
	  // Any place where we would SetHoldingProp to true also sets the object's bAmbientProp flag.
		// If for some reason the object equipped isn't an OBJECTTYPE_OBJECT and doesn't have flag, we're not holding the prop any more.
		SetHoldingProp(&ped, false);
		return;
	}

	// Set the proper weapon instruction on the prop before removing it.
	SetPropWeaponInstructionForDisposal(ped);

	// Stop any weapon-related animations if this was a weapon.
	if ( pObject->GetWeapon() )
	{
		pObject->GetWeapon()->StopAnim(SLOW_BLEND_OUT_DELTA);
	}

	// Get the weapon instruction, to determine if we should destroy the object rather than dropping it,
	// and clear the weapon instruction.
	const CPedWeaponManager::eWeaponInstruction instr = mgr->GetWeaponInstruction();
	mgr->SetWeaponInstruction(CPedWeaponManager::WI_None);
	if(instr == CPedWeaponManager::WI_AmbientTaskDestroyWeapon || instr == CPedWeaponManager::WI_AmbientTaskDestroyWeapon2ndAttempt)
	{
		ped.GetInventory()->RemoveWeapon(OBJECTTYPE_OBJECT);
		SetHoldingProp(&ped, false);
	}
	else
	{
		const bool bActivatePhysics = true;
		DropProp(ped, bActivatePhysics);
	}
}


void CPropManagementHelper::DropProp(CPed& ped, bool activatePhys)
{
	// If CPropManagmentHelper ever gets folded into the WeaponManager, this would be good to turn on again. [3/22/2013 mdawe]
	//taskAssert(IsHoldingProp());

	CObject* pObject = ped.GetWeaponManager()->DropWeapon(OBJECTTYPE_OBJECT, true);
	if(pObject)
	{
		if(!activatePhys)
		{
			// Would probably be better to not activate it in the first place:
			pObject->DeActivatePhysics();

			// If we are not activating physics, we are presumably putting the prop back in
			// a proper position, at which it probably shouldn't be considered as uprooted.
			// From looking at the code, it seems like it would be safe to just set this flag to false
			// like this.
			pObject->m_nObjectFlags.bHasBeenUprooted = false;
		}
		else
		{
			if (pObject->GetCurrentPhysicsInst() && pObject->GetCurrentPhysicsInst()->IsInLevel())
			{
				//Apply a random impulse to the object so it rotates as it falls.
				const float fMinImpulse =  0.f;
				const float fMaxImpulse = 15.f;

				const float fXImpulse = fwRandom::GetRandomNumberInRange(fMinImpulse, fMaxImpulse);
				const float fYImpulse = fwRandom::GetRandomNumberInRange(fMinImpulse, fMaxImpulse);
				const float fZImpulse = fwRandom::GetRandomNumberInRange(fMinImpulse, fMaxImpulse);

				// Find the object's bounding radius to create an offset within that radius.
				Vector3 vWorldCentroid;
				float fCentroidHalfRadius = pObject->GetBoundCentreAndRadius(vWorldCentroid) / 2.f;

				const float fXDirection = fwRandom::GetRandomNumberInRange(-fCentroidHalfRadius, fCentroidHalfRadius);
				const float fYDirection = fwRandom::GetRandomNumberInRange(-fCentroidHalfRadius, fCentroidHalfRadius);
				const float fZDirection = fwRandom::GetRandomNumberInRange(-fCentroidHalfRadius, fCentroidHalfRadius);

				Vector3 vAngImpulse(fXImpulse, fYImpulse, fZImpulse);
				Vector3 vOffset(fXDirection, fYDirection, fZDirection);

				pObject->ApplyAngImpulse(vAngImpulse, vOffset);
			}
		}

		// If this is not an ambient prop it's probably something we grabbed from the environment
		// through UseExistingObject(), and we should probably destroy the weapon object.
		if(!pObject->m_nObjectFlags.bAmbientProp)
		{
			pObject->SetWeapon(NULL);
		}
	}

	SetDestroyProp(false);
	SetHoldingProp(&ped, false);
}


void CPropManagementHelper::GrabPropFromEnvironment(CPed& ped)
{
	taskAssert(m_PropInEnvironment);

	DisposeOfProp(ped);
	if(IsHoldingProp())
	{
		return;
	}

	CPedWeaponManager* pWeapMgr = ped.GetWeaponManager();
	if(!taskVerifyf(pWeapMgr, "Need CPedWeaponManager to grab prop from environment."))
	{
		return;
	}
	CPedInventory* pInventory = ped.GetInventory();
	if(!taskVerifyf(pInventory, "Need CPedInventory to grab prop from environment."))
	{
		return;
	}

	CWeaponItem* pItem = pInventory->AddWeapon(OBJECTTYPE_OBJECT);
	if(!pItem)
	{
		return;
	}

	// Set the correct model.
	pItem->SetModelHash(m_PropInEnvironment->GetBaseModelInfo()->GetModelNameHash());

	if(!pWeapMgr->EquipWeapon(OBJECTTYPE_OBJECT))
	{
		pInventory->RemoveWeapon(OBJECTTYPE_OBJECT);
		return;
	}

	if(!pWeapMgr->GetPedEquippedWeapon()->UseExistingObject(*m_PropInEnvironment, IsCreatePropInLeftHand() ? CPedEquippedWeapon::AP_LeftHand : CPedEquippedWeapon::AP_RightHand))
	{
		return;
	}

	SetHoldingProp(&ped, true);
	SetDestroyProp(false);
}

bool CPropManagementHelper::GivePedProp( CPed& pPed )
{
	taskAssert(!IsHoldingProp());
	fwModelId iPropId = CModelInfo::GetModelIdFromName( m_PropStreamingObjectNameHash );

	// Make sure the prop model has loaded
	if( taskVerifyf(iPropId.IsValid(), "Invalid model %s",m_PropStreamingObjectNameHash.GetCStr()) && CModelInfo::HaveAssetsLoaded( iPropId) )
	{
		if( weaponVerifyf(pPed.GetInventory(), "Not expected to get here with no inventory (%s at coords %f,%f,%f)  Please warp to these coords and take a screenshot showing tasks/animations if possible.", 
			pPed.GetModelName(), pPed.GetTransform().GetPosition().GetXf(), pPed.GetTransform().GetPosition().GetYf(), pPed.GetTransform().GetPosition().GetZf() ) )
		{
			CWeaponItem* pItem = pPed.GetInventory()->AddWeapon( OBJECTTYPE_OBJECT );
			if( pItem )
			{
				// Set the correct model
				pItem->SetModelHash( m_PropStreamingObjectNameHash.GetHash() );
				CPedWeaponManager* pWeaponMgr = pPed.GetWeaponManager();
				weaponAssert(pWeaponMgr);
				if( pWeaponMgr->EquipWeapon( OBJECTTYPE_OBJECT ) )
				{
					pWeaponMgr->CreateEquippedWeaponObject( IsCreatePropInLeftHand() ? CPedEquippedWeapon::AP_LeftHand : CPedEquippedWeapon::AP_RightHand );

					CObject* pWeaponObject = pWeaponMgr->GetEquippedWeaponObject();
					if( pWeaponObject )
					{
						SetWeaponObjectPropInfo(pWeaponObject, pPed);
						// Success
						return true;
					}
					else
					{
						m_Flags.SetFlag(Flag_ReattemptSetWeaponObjectInfo);
					}
				}
				else
				{
					pPed.GetInventory()->RemoveWeapon( OBJECTTYPE_OBJECT );
				}
			}
		}
	}
	else
	{	
		if( !strStreamingEngine::GetIsLoadingPriorityObjects() )
		{
			CModelInfo::RequestAssets( iPropId, 0 );
		}
	}

	return false;
}

void CPropManagementHelper::SetForcedLoading()
{ 
	if ( taskVerifyf(!IsPropLoaded(), "Shouldn't SetForcedLoading on a prop that's already loaded") ) 
	{
		taskAssert(IsPropLoading());
		m_Flags.SetFlag(Flag_ForcedLoading); 
	}
}

void CPropManagementHelper::SetWeaponObjectPropInfo( CObject* pWeaponObject, CPed& pPed )
{
	weaponAssert(pWeaponObject);
	pWeaponObject->m_nObjectFlags.bAmbientProp = true;

	// get the scenario info
	const CScenarioInfo* pScenarioInfo = NULL;
	s32 uScenarioType = CPed::GetScenarioType(pPed);
	if (uScenarioType != -1)
	{
		pScenarioInfo = CScenarioManager::GetScenarioInfo(uScenarioType);
	}
	// if we have valid scenario info
	if (pScenarioInfo != NULL && pWeaponObject->GetWeapon() != NULL)
	{
		// set the prop's end of life timeout
		pWeaponObject->GetWeapon()->SetPropEndOfLifeTimeoutMS(pScenarioInfo->GetPropEndOfLifeTimeoutMS());
	}

	SetHoldingProp(&pPed, true);
}

