// Framework headers
#include "fwanimation/clipsets.h"
#include "fwanimation/animmanager.h"

// File headers
#include "ModelInfo/ModelInfo.h"
#include "Network/NetworkInterface.h"
#include "Weapons/Components/WeaponComponent.h"
#include "Weapons/Components/WeaponComponentManager.h"
#include "Weapons/WeaponScriptResource.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/Info/WeaponAnimationsManager.h"
#include "Weapons/Inventory/AmmoItem.h"
#include "pickups/Data/PickupDataManager.h"
#include "pickups/PickupManager.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////////

CWeaponScriptResource::CWeaponScriptResource( atHashWithStringNotFinal weaponHash, s32 iRequestFlags, s32 iExtraComponentFlags )
: m_WeaponHash( weaponHash )
{
	RequestAnims( iRequestFlags );

	weaponDisplayf("CWeaponScriptResource::CWeaponScriptResource - m_assetRequester has %d elements (max %d)", m_assetRequester.GetRequestCount(), m_assetRequester.GetRequestMaxCount() );

	// Request the models
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( m_WeaponHash );
	if (pWeaponInfo)
	{
		// Request main weapon model
		u32 iModelIndex = pWeaponInfo->GetModelIndex();
		fwModelId modelId((strLocalIndex(iModelIndex)));
		if(modelId.IsValid())
		{
#if __BANK
			OutputRequestArrayIfFull();
#endif	//__BANK
			if(weaponVerifyf(m_assetRequester.GetRequestCount() < m_assetRequester.GetRequestMaxCount(), "Ran out of request storage for [%s][%d], max [%d]", m_WeaponHash.TryGetCStr(), m_WeaponHash.GetHash(), m_assetRequester.GetRequestMaxCount()))
			{
				s32 tempLocalIndex = CModelInfo::AssignLocalIndexToModelInfo(modelId).Get();
				m_assetRequester.PushRequest(tempLocalIndex, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_MISSION_REQUIRED);

				weaponDisplayf("CWeaponScriptResource::CWeaponScriptResource - After requesting %s model, m_assetRequester has %d elements (max %d)", m_WeaponHash.TryGetCStr(), m_assetRequester.GetRequestCount(), m_assetRequester.GetRequestMaxCount() );
			}
		}

		// Request default weapon component models
		const CWeaponInfo::AttachPoints& attachPoints = pWeaponInfo->GetAttachPoints();

		weaponDisplayf("CWeaponScriptResource::CWeaponScriptResource - %s model has %d attach points", m_WeaponHash.TryGetCStr(), attachPoints.GetCount());

		for(s32 i = 0; i < attachPoints.GetCount(); i++)
		{
			const CWeaponComponentPoint::Components& components = attachPoints[i].GetComponents();

			weaponDisplayf("CWeaponScriptResource::CWeaponScriptResource - Attach point %d of %s model has %d components", i, m_WeaponHash.TryGetCStr(), components.GetCount());

			for(int j = 0; j < components.GetCount(); j++)
			{
				bool bLoadNonDefaultComponent = false;
				if(!components[j].m_Default && iExtraComponentFlags != 0)
				{
					if(iExtraComponentFlags & Weapon_Component_Flash && attachPoints[i].GetAttachBoneHash() == atHashString("WAPFlshLasr", 0x287A5AB6))
					{
						bLoadNonDefaultComponent = true;
					}
					else if(iExtraComponentFlags & Weapon_Component_Scope && attachPoints[i].GetAttachBoneHash() == atHashString("WAPScop", 0xBB85931))
					{
						bLoadNonDefaultComponent = true;
					}
					else if(iExtraComponentFlags & Weapon_Component_Supp && attachPoints[i].GetAttachBoneHash() == atHashString("WAPSupp", 0x6F0DE560))
					{
						bLoadNonDefaultComponent = true;
					}
					else if(iExtraComponentFlags & Weapon_Component_Sclip2 && attachPoints[i].GetAttachBoneHash() == atHashString("WAPClip", 0xDDEDC7B4))
					{
						bLoadNonDefaultComponent = true;
					}
					else if(iExtraComponentFlags & Weapon_Component_Grip && attachPoints[i].GetAttachBoneHash() == atHashString("WAPGrip", 0xB1339FC5))
					{
						bLoadNonDefaultComponent = true;
					}

				}

				if(components[j].m_Default || bLoadNonDefaultComponent)
				{
					const u32 uComponentHash = components[j].m_Name.GetHash();
					const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(uComponentHash);
					if(weaponVerifyf(pComponentInfo, "Component Info Hash [%d] does not exist in metadata", uComponentHash))
					{
						if(pComponentInfo->GetModelHash() != 0)
						{
							fwModelId modelId;
							CModelInfo::GetBaseModelInfoFromHashKey(pComponentInfo->GetModelHash(), &modelId);

							if(modelId.IsValid())
							{
#if __BANK
								OutputRequestArrayIfFull();
#endif	//__BANK
								if(weaponVerifyf(m_assetRequester.GetRequestCount() < m_assetRequester.GetRequestMaxCount(), "Ran out of request storage for [%s][%d], max [%d]", m_WeaponHash.TryGetCStr(), m_WeaponHash.GetHash(), m_assetRequester.GetRequestMaxCount()))
								{
									s32 tempLocalIndex = CModelInfo::AssignLocalIndexToModelInfo(modelId).Get();
									m_assetRequester.PushRequest(tempLocalIndex, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_MISSION_REQUIRED);

									weaponDisplayf("CWeaponScriptResource::CWeaponScriptResource - After requesting component %d of attach point %d of %s model, m_assetRequester has %d elements (max %d)", j, i, m_WeaponHash.TryGetCStr(), m_assetRequester.GetRequestCount(), m_assetRequester.GetRequestMaxCount() );
								}
							}
						}
					}
				}
			}
		}

		/// Request ordnance if needed.
		if(pWeaponInfo->GetCreateVisibleOrdnance())
		{
			const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
			if(pAmmoInfo)
			{
				// Request the assets
				u32 iModelIndex = pAmmoInfo->GetModelIndex();
				fwModelId modelId((strLocalIndex(iModelIndex)));
				if(modelId.IsValid())
				{
					if(weaponVerifyf(m_assetRequesterAmmo.GetRequestCount() < m_assetRequesterAmmo.GetRequestMaxCount(), "Ran out of request storage for [%s][%d], max [%d]", m_WeaponHash.TryGetCStr(), m_WeaponHash.GetHash(), m_assetRequesterAmmo.GetRequestMaxCount()))
					{
						s32 transientLocalIndex = CModelInfo::AssignLocalIndexToModelInfo(modelId).Get();
						m_assetRequesterAmmo.PushRequest(transientLocalIndex, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_MISSION_REQUIRED);
					}
				}
			}
		}

		// Request pickup model if it's not same as the weapon item.
		u32 uPickupHash = CPickupManager::ShouldUseMPPickups(NULL) ? pWeaponInfo->GetMPPickupHash() : pWeaponInfo->GetPickupHash();
		if(uPickupHash != 0)
		{
			const CPickupData* pPickupData = CPickupDataManager::GetPickupData( uPickupHash );
			u32 uModelHash = pPickupData->GetModelHash();
			if(pPickupData && uModelHash != pWeaponInfo->GetModelHash())
			{
				fwModelId modelId;
				CModelInfo::GetBaseModelInfoFromHashKey(uModelHash, &modelId);
				if(modelId.IsValid() && !CModelInfo::HaveAssetsLoaded(modelId))
				{
					if(weaponVerifyf(m_assetRequesterAmmo.GetRequestCount() < m_assetRequesterAmmo.GetRequestMaxCount(), "Ran out of request storage for [%s][%d], max [%d]", m_WeaponHash.TryGetCStr(), m_WeaponHash.GetHash(), m_assetRequesterAmmo.GetRequestMaxCount()))
					{
						s32 tempLocalIndex = CModelInfo::AssignLocalIndexToModelInfo(modelId).Get();
						m_assetRequesterAmmo.PushRequest(tempLocalIndex, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_MISSION_REQUIRED);
					}
				}
			}
		}
	}
}

CWeaponScriptResource::~CWeaponScriptResource()
{
	m_assetRequester.ClearRequiredFlags( STRFLAG_MISSION_REQUIRED );
}

void CWeaponScriptResource::RequestAnims( s32 iRequestFlags )
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( m_WeaponHash );
	if( pWeaponInfo )
	{
		// Load the weapon clipsets
		if( iRequestFlags & WRF_RequestBaseAnims )
		{
			RequestAnimsFromClipSet( pWeaponInfo->GetWeaponClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetInjuredWeaponClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetScopeWeaponClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetAlternativeClipSetWhenBlockedId() );
		}

		// Load the peds gun and movement cover clipsets
		if( iRequestFlags & WRF_RequestCoverAnims )
		{
			RequestAnimsFromClipSet( pWeaponInfo->GetPedCoverMovementClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetPedCoverAlternateMovementClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetPedCoverWeaponClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetHiCoverWeaponClipSetId() );
		}

		// Load the melee clipsets
		if( iRequestFlags & WRF_RequestMeleeAnims )
		{
			RequestAnimsFromClipSet( pWeaponInfo->GetMeleeClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetMeleeVariationClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetMeleeTauntClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetMeleeSupportTauntClipSetId() );
		}

		// Load motion clipsets
		if( iRequestFlags & WRF_RequestMotionAnims )
		{
			RequestAnimsFromClipSet( pWeaponInfo->GetPedMotionClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetPedMotionCrouchClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetPedMotionStrafingClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetPedMotionStrafingStealthClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetPedMotionStrafingUpperBodyClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetJumpUpperbodyClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetFallUpperbodyClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetFromStrafeTransitionUpperBodyClipSetId() );
		}

		if( iRequestFlags & WRF_RequestStealthAnims )
		{
			RequestAnimsFromClipSet( pWeaponInfo->GetStealthWeaponClipSetId() );
			RequestAnimsFromClipSet( pWeaponInfo->GetMeleeStealthClipSetId() );
		}

		if( iRequestFlags & WRF_RequestAllMovementVariationAnims )
		{
			CWeaponAnimationsSets::WeaponAnimationSetMap& map = CWeaponAnimationsManager::GetInstance().GetAnimationWeaponSets().m_WeaponAnimationsSets;
			CWeaponAnimationsSets::WeaponAnimationSetMap::Iterator i = map.Begin();
			while(i != map.End())
			{
				CWeaponAnimationsSet set = (*i);

				CWeaponAnimationsSet::WeaponAnimationMap& map2 = set.m_WeaponAnimations;
				CWeaponAnimationsSet::WeaponAnimationMap::Iterator i2 = map2.Begin();
				while(i2 != map2.End())
				{
					if(i2.GetKey() == pWeaponInfo->GetHash())
					{
						CWeaponAnimations anim = (*i2);
						if(anim.MotionClipSetHash.GetHash() != 0)
						{
							RequestAnimsFromClipSet(fwMvClipSetId(anim.MotionClipSetHash));
						}
					}
					++i2;
				}
				++i;
			}
		}
	}
}

void CWeaponScriptResource::RequestAnimsFromClipSet( const fwMvClipSetId &clipSetId )
{
	// Go through all the anim dictionaries in the clip set and request them to be streamed in
	if( clipSetId != CLIP_SET_ID_INVALID )
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet( clipSetId );
		while( pClipSet )
		{
			s32 animDictIndex = fwAnimManager::FindSlotFromHashKey( pClipSet->GetClipDictionaryName().GetHash() ).Get();
			if( Verifyf( animDictIndex > -1, "Can't find clip dictionary [%s]", pClipSet->GetClipDictionaryName().GetCStr() ) )
			{
				if ( m_assetRequester.RequestIndex( animDictIndex, fwAnimManager::GetStreamingModuleId() ) == -1 )	// make sure we are not duplicating anim requests
				{
#if __BANK
					OutputRequestArrayIfFull();
#endif	//__BANK
					if(weaponVerifyf(m_assetRequester.GetRequestCount() < m_assetRequester.GetRequestMaxCount(), "Ran out of request storage for [%s][%d], max [%d]", m_WeaponHash.TryGetCStr(), m_WeaponHash.GetHash(), m_assetRequester.GetRequestMaxCount()))
					{
						m_assetRequester.PushRequest( animDictIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_MISSION_REQUIRED );
						weaponDisplayf("CWeaponScriptResource::RequestAnimsFromClipSet - After requesting %s model from %s clip set, m_assetRequester has %d elements (max %d)", m_WeaponHash.TryGetCStr(), pClipSet->GetClipDictionaryName().GetCStr(), m_assetRequester.GetRequestCount(), m_assetRequester.GetRequestMaxCount() );
					}
				}
			}

			// Load the fallback clipset if it exists
			pClipSet = fwClipSetManager::GetClipSet( pClipSet->GetFallbackId() );
		}
	}
}

#if __BANK
void CWeaponScriptResource::OutputRequestArrayIfFull()
{
	if (m_assetRequester.GetRequestCount() >=  m_assetRequester.GetRequestMaxCount())
	{
		weaponDisplayf("CWeaponScriptResource::CWeaponScriptResource - Ran out of request storage. Request Count: %d, Max Count: %d", m_assetRequester.GetRequestCount(), m_assetRequester.GetRequestMaxCount());
		weaponDisplayf("CWeaponScriptResource::CWeaponScriptResource - Outputting request array contents:");
		for (int i = 0; i < m_assetRequester.GetRequestCount(); i++)
		{
			weaponDisplayf("m_assetRequester[%d]: object name: %s", i, m_assetRequester.GetRequest(i).GetName());
		}
	}
}
#endif	//__BANK

//////////////////////////////////////////////////////////////////////////////

CWeaponScriptResourceManager::WeaponResources CWeaponScriptResourceManager::ms_WeaponResources;

void CWeaponScriptResourceManager::Init()
{
}

void CWeaponScriptResourceManager::Shutdown()
{
	for( s32 i = 0; i < ms_WeaponResources.GetCount(); i++ )
	{
		delete ms_WeaponResources[i];
	}

	ms_WeaponResources.Reset();
}

s32 CWeaponScriptResourceManager::RegisterResource( atHashWithStringNotFinal weaponHash, s32 iRequestFlags, s32 iExtraComponentFlags )
{
	s32 iIndex = GetIndex( weaponHash );
	if(iIndex == -1)
	{
		iIndex = ms_WeaponResources.GetCount();
		ms_WeaponResources.PushAndGrow( rage_new CWeaponScriptResource( weaponHash, iRequestFlags, iExtraComponentFlags ) );
	}

	return ms_WeaponResources[iIndex]->GetHash();
}

void CWeaponScriptResourceManager::UnregisterResource( atHashWithStringNotFinal weaponHash )
{
	s32 iIndex = GetIndex( weaponHash );
	if( iIndex != -1 )
	{
		delete ms_WeaponResources[iIndex];
		ms_WeaponResources.DeleteFast( iIndex );
	}
}

CWeaponScriptResource* CWeaponScriptResourceManager::GetResource( atHashWithStringNotFinal weaponHash )
{
	s32 iIndex = GetIndex( weaponHash );
	if( iIndex != -1 )
	{
		return ms_WeaponResources[iIndex];
	}

	return NULL;
}

s32 CWeaponScriptResourceManager::GetIndex( atHashWithStringNotFinal weaponHash )
{
	for( s32 i = 0; i < ms_WeaponResources.GetCount(); i++ )
	{
		if( weaponHash.GetHash() == ms_WeaponResources[i]->GetHash() )
		{
			return i;
		}
	}

	return -1;
}
