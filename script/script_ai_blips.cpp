// Framework headers
#include "fwmaths/angle.h"
#include "fwmaths/Vector.h"

// Game headers
#include "camera/CamInterface.h"
#include "frontend/MiniMap.h"
#include "frontend/Map/BlipEnums.h"
#include "peds/PedIntelligence.h"
#include "scene/world/GameWorld.h"
#include "game/ModelIndices.h"
#include "task/System/TaskTypes.h"

// Network headers
#include "network/NetworkInterface.h"
#include "network/Live/livemanager.h"
#include "network/objects/entities/NetObjPlayer.h"

#include "script/script.h"
#include "script/script_hud.h"
#include "script/script_helper.h"
#include "audio/scriptaudioentity.h"

#include "script_ai_blips.h"

// DON'T COMMIT
//OPTIMISATIONS_OFF()

//////////////////////////
//	CScriptPedAIBlips
//////////////////////////

// TODO: Make tunable?
float CScriptPedAIBlips::WEAPON_RANGE_MULTIPLIER_FOR_BLIPS	= 2.5f;
float CScriptPedAIBlips::HEARD_RADIUS						= 100.0f;
float CScriptPedAIBlips::BLIP_SIZE_VEHICLE					= 1.0f;
float CScriptPedAIBlips::BLIP_SIZE_NETWORK_VEHICLE			= 1.0f;
float CScriptPedAIBlips::BLIP_SIZE_PED						= 0.7f;
float CScriptPedAIBlips::BLIP_SIZE_NETWORK_PED				= 0.7f;
float CScriptPedAIBlips::BLIP_SIZE_OBJECT					= 0.7f;
float CScriptPedAIBlips::BLIP_SIZE_NETWORK_OBJECT			= 0.85f;
int	  CScriptPedAIBlips::NOTICABLE_TIME						= 3500;
int	  CScriptPedAIBlips::BLIP_FADE_PERIOD					= 1400;


atArray<CScriptPedAIBlips::AIBlip> CScriptPedAIBlips::m_PedsAndBlips;

void CScriptPedAIBlips::Init(unsigned UNUSED_PARAM(initMode))
{
	// Remove all registered blips/peds
	Reset();
}

void CScriptPedAIBlips::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	// Remove all registered blips/peds
	Reset();
}

void CScriptPedAIBlips::Update()
{
	UpdatePedAIBlips();
}

void CScriptPedAIBlips::UpdatePedAIBlips()
{
		
	for(int i=m_PedsAndBlips.size()-1; i>=0; i--)
	{
		AIBlip &theBlip = m_PedsAndBlips[i];

		bool bRemove = true;	// Assume we want to remove this ped unless we find otherwise
		
		//Check if the script has gone, if so, delete this reference.
		scrThread *pScriptThread = scrThread::GetThread(static_cast<scrThreadId>(theBlip.scrThreadID));
		// If no thread, remove == true, otherwise, remove == whether the ped is dead or not
		if( pScriptThread )
		{
			bRemove = theBlip.Update();	// Returns true if the ped is dead or otherwise gone.
		}

		if(bRemove)
		{
			theBlip.CleanupBlip();
			m_PedsAndBlips.DeleteFast( i );
		}
	}
}

bool CScriptPedAIBlips::IsExclusiveOwnerOfBlip(const AIBlip* referencingObject, s32 iBlipId)
{
	for(int i=m_PedsAndBlips.size()-1; i>=0; i--)
	{
		AIBlip &theBlip = m_PedsAndBlips[i];

		if( &theBlip == referencingObject )
			continue;

		if( theBlip.VehicleBlipID == iBlipId || theBlip.BlipID == iBlipId )
			return false;
	}

	return true;
}



// Sets a Ped to use AI blips (or not)
void CScriptPedAIBlips::SetPedHasAIBlip(s32 threadId, s32 PedID, bool bSet, s32 colourOverride)
{
	// Check if this ped is already registered for ai blips
	int	index = FindPedIDIndex(PedID);	

	if( bSet && index == -1 )
	{
		// We want to register a ped AI Blip, it's not already registered
		AIBlip &thisBlip = m_PedsAndBlips.Grow();
		thisBlip.Init( PedID, threadId, colourOverride );
	}
	else if( !bSet && index != -1 )
	{
		AIBlip &thisBlip = m_PedsAndBlips[index];
		thisBlip.CleanupBlip();

		m_PedsAndBlips.DeleteFast( index );
	}
}

bool CScriptPedAIBlips::DoesPedhaveAIBlip(s32 PedID)
{
	int	index = FindPedIDIndex(PedID);
	if( index != -1 )
	{
		return true;
	}
	return false;
}

void CScriptPedAIBlips::SetPedAIBlipGangID(s32 PedID, s32 GangID)
{
	int	index = FindPedIDIndex(PedID);	
	if(index != -1)
	{
		AIBlip &theBlip = m_PedsAndBlips[index];
		theBlip.iBlipGang = GangID;
	}
}

void CScriptPedAIBlips::SetPedHasConeAIBlip(s32 PedID, bool bSet)
{
	int	index = FindPedIDIndex(PedID);	
	if(index != -1)
	{
		AIBlip &theBlip = m_PedsAndBlips[index];
		theBlip.bShowCone = bSet;
	}
}

void CScriptPedAIBlips::SetPedHasForcedBlip(s32 PedID, bool bOnOff)
{
	int	index = FindPedIDIndex(PedID);	
	if(index != -1)
	{
		AIBlip &theBlip = m_PedsAndBlips[index];
		theBlip.bForceBlipOn = bOnOff;
	}
}

void CScriptPedAIBlips::SetPedAIBlipNoticeRange(s32 PedID, float range)
{
	int	index = FindPedIDIndex(PedID);	
	if(index != -1)
	{
		AIBlip &theBlip = m_PedsAndBlips[index];
		theBlip.fNoticableRadius = range;
	}
}

void CScriptPedAIBlips::SetPedAIBlipChangeColour(s32 PedID)
{
	int	index = FindPedIDIndex(PedID);	
	if(index != -1)
	{
		AIBlip &theBlip = m_PedsAndBlips[index];
		theBlip.bChangeColour = true;
	}
}

void CScriptPedAIBlips::SetPedAIBlipSprite(s32 PedID, int spriteID)
{
	int	index = FindPedIDIndex(PedID);	
	if(index != -1)
	{
		AIBlip &theBlip = m_PedsAndBlips[index];

		bool bShowHeight = false;  // retain the flag when we change the sprite
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(theBlip.BlipID);
		if (pBlip)
		{
			bShowHeight = CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_SHOW_HEIGHT);
		}

		CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, theBlip.BlipID, spriteID);
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEIGHT, theBlip.BlipID, bShowHeight);
	}
}

s32	CScriptPedAIBlips::GetAIBlipPedBlipIDX(s32 PedID)
{
	int	index = FindPedIDIndex(PedID);	
	if(index != -1)
	{
		AIBlip &theBlip = m_PedsAndBlips[index];
		return theBlip.BlipID;
	}

	return INVALID_BLIP_ID;
}

s32	CScriptPedAIBlips::GetAIBlipVehicleBlipIDX(s32 PedID)
{
	int	index = FindPedIDIndex(PedID);	
	if(index != -1)
	{
		AIBlip &theBlip = m_PedsAndBlips[index];
		return theBlip.VehicleBlipID;
	}
	return INVALID_BLIP_ID;
}

void CScriptPedAIBlips::Reset()
{
	for(int i=0;i<m_PedsAndBlips.size();i++)
	{
		AIBlip &theBlip = m_PedsAndBlips[i];
		theBlip.CleanupBlip();
	}
	m_PedsAndBlips.Reset();
}

s32	CScriptPedAIBlips::FindPedIDIndex(s32 pedID)
{
	for(int i=0;i<m_PedsAndBlips.size();i++)
	{
		if(m_PedsAndBlips[i].pedID == pedID)
		{
			return i;
		}
	}
	return -1;
}

//////////////////////////////
// AIBlip
//////////////////////////////

void CScriptPedAIBlips::AIBlip::Init(s32 PedID, s32 threadId, s32 colourOverride)
{
	// Script thread ID
	scrThreadID = THREAD_INVALID;

	// Variable params
	iBlipGang = -1;
	bForceBlipOn = false;
	fNoticableRadius = AI_BLIP_NOTICABLE_RADIUS;

	// Storage
	BlipID = INVALID_BLIP_ID;
	VehicleBlipID = INVALID_BLIP_ID;
	iNoticableTimer = 0;
	iFadeOutTimer = 0;
	iNoticableTimer_SP = 0;
	iFadeOutTimer_SP = 0;
	bIsFadingOut = false;
	bRespondingToSpecialAbility = false;

	blipColourOverride = (eBLIP_COLOURS)colourOverride;
	bChangeColour = blipColourOverride != BLIP_COLOUR_MAX;

	pedID = PedID;
	scrThreadID = threadId;	// Needed for allocation of script resources.

	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedID, 0);
	if( pPed )
	{
		// If the ped already has a blip, make sure we know about it
		BlipID = GetBlipFromEntity(pPed);

		if (NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
		{
			scriptDebugf1("Adding network ped blip : %s (%p)",pPed->GetNetworkObject()->GetLogName(), pPed);
		}
	}

}

//	returns TRUE if the ped is injured/doesn't exist, FALSE if the ped is still uninjured.
bool CScriptPedAIBlips::AIBlip::Update()
{
	CPed *pFocusPed = NULL;

	if ( NetworkInterface::IsGameInProgress() && NetworkInterface::IsInSpectatorMode() )
	{
		pFocusPed = const_cast<CPed*>(camInterface::FindFollowPed());
	}
	else
	{
		pFocusPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
	}

	
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedID, 0);

	// If there's no entity then there's not a lot that can be done.
	if( pPed && pFocusPed)
	{
		if(!pPed->IsInjured())
		{
			if(!bIsFadingOut && 
				(bForceBlipOn || IsPedNoticableToPlayer(pPed, pFocusPed, fNoticableRadius, bRespondingToSpecialAbility, iResponseTimer, iResponseTime)))
			{
				if( pPed->GetIsInVehicle() )
				{
					eBLIP_COLOURS blipColour = BLIP_COLOUR_MAX;
					//if ped is in any vehicle
					if( DoesBlipExist(BlipID) )
					{
						// cache off the ped blip for when we recreate it
						blipColour = CMiniMap::GetBlipColourValue( CMiniMap::GetBlip(BlipID) );
						RemoveBlip(BlipID);
					}

					if( !DoesBlipExist(BlipID) )
					{
						if( !DoesBlipExist(VehicleBlipID) )
						{
							CVehicle *pVehicle = pPed->GetVehiclePedInside();
							if(pVehicle)
							{
								if( !DoesBlipExist( GetBlipFromEntity(pVehicle) ) )
								{
									VehicleBlipID = CreateBlipForVehicle(pVehicle, true);

									if( NetworkInterface::IsGameInProgress() )
									{
										CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(pVehicle->GetModelId());

										if( blipColour == BLIP_COLOUR_MAX )
											blipColour = CMiniMap::GetBlipColourValue( CMiniMap::GetBlip(VehicleBlipID) );

										if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE && ((CVehicleModelInfo*)pModelInfo)->GetVehicleType()==VEHICLE_TYPE_HELI)
										{
											CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, VehicleBlipID, RADAR_TRACE_PLAYER_HELI); // RADAR_TRACE_ENEMY_HELI_SPIN can't be recolored
										} 
										else if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE && ((CVehicleModelInfo*)pModelInfo)->GetVehicleType()==VEHICLE_TYPE_PLANE)
										{
											CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, VehicleBlipID, BLIP_POLICE_PLANE_MOVE);
											CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEIGHT, VehicleBlipID, true);  // always show height
										}
										else
										{
											CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, VehicleBlipID, RADAR_TRACE_AI);
											CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEIGHT, VehicleBlipID, true);  // always show height
										}
										CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, VehicleBlipID, blipColourOverride != BLIP_COLOUR_MAX ? blipColourOverride : (int)blipColour);
									}
								}
								else
								{
									VehicleBlipID = GetBlipFromEntity(pVehicle);
								}
							}
						} // blip DOES exist:
						// maintain directional blips
						else if( NetworkInterface::IsGameInProgress() )
						{
							if( CMiniMap::GetBlipObjectNameId( CMiniMap::GetBlip(VehicleBlipID) ) == BLIP_POLICE_PLANE_MOVE )
							{
								if( CVehicle *pVehicle = pPed->GetVehiclePedInside() )
								{
									Matrix34 mat = MAT34V_TO_MATRIX34( pVehicle->GetMatrix() );

									Vector3 eulers;
									mat.ToEulersYXZ( eulers );
									// MAY need to check for Gimbal Lock, but currently script isn't either in the place it's faking it, so... fuck it?
									// also, the AI won't ascend/descend steeply, so for us, this works fine.
									float fHeading = rage::Wrap( (RtoD * eulers.z), 0.0f, 360.0f );
									CMiniMap::SetBlipParameter( BLIP_CHANGE_DIRECTION, VehicleBlipID, fHeading);
								}
							}
							
							// Make sure the blip is visible
							CMiniMap::SetBlipParameter(BLIP_CHANGE_ALPHA, VehicleBlipID, 255);
						}
					}
				}
				else
				{
					eBLIP_COLOURS blipColour = BLIP_COLOUR_MAX;
					//if ped is not in any vehicle
					if( DoesBlipExist(VehicleBlipID) )
					{
						// cache off the vehicle blip for when we recreate it
						blipColour = CMiniMap::GetBlipColourValue( CMiniMap::GetBlip(VehicleBlipID) );
						RemoveBlip(VehicleBlipID, true);

						if (!DoesBlipExist(BlipID))
						{
							pPed->SetCreatedBlip(false);		// ped blip doesn't currently exist but it should - clear flag to ensure that happens now.
						}
					}

					if( !DoesBlipExist(BlipID) )
					{
						if(!pPed->GetCreatedBlip())
						{
							BlipID = AddBlipForEntity(pPed);
							CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, BlipID, NetworkInterface::IsGameInProgress() ? CScriptPedAIBlips::BLIP_SIZE_NETWORK_PED : CScriptPedAIBlips::BLIP_SIZE_PED);
							CMiniMap::SetBlipParameter(BLIP_CHANGE_PRIORITY, BlipID, GetCorrectBlipPriority(BLIP_PRIORITY_ENEMY_AI));

							if( NetworkInterface::IsGameInProgress() )
							{
								if( blipColour == BLIP_COLOUR_MAX )
									blipColour = CMiniMap::GetBlipColourValue( CMiniMap::GetBlip(BlipID) );
								CMiniMap::SetBlipParameter(BLIP_CHANGE_OBJECT_NAME, BlipID, RADAR_TRACE_AI);
								CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, BlipID, blipColourOverride != BLIP_COLOUR_MAX ? blipColourOverride : (int)blipColour);
								SetBlipNameFromTextFile(BlipID, "BN_ENEMY");
							}
						}
					}

					if( DoesBlipExist(BlipID) )
					{
						switch( iBlipGang )
						{
						case BLIP_GANG_PROFESSIONAL:
							{
								CRGBA rgba = CHudColour::GetRGBA((eHUD_COLOURS)HUD_COLOUR_NET_PLAYER29);
								CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, BlipID, blipColourOverride != BLIP_COLOUR_MAX ? blipColourOverride : (int)rgba.GetColor());
								SetBlipNameFromTextFile(BlipID, "PROFESSIONAL_BLIP");
							}
							break;

						case BLIP_GANG_FRIENDLY:
							CMiniMap::SetBlipParameter(BLIP_CHANGE_FRIENDLY, BlipID, true);
							break;

						case BLIP_GANG_ENEMY:
							CMiniMap::SetBlipParameter(BLIP_CHANGE_FRIENDLY, BlipID, false);
							break;
						}

						// turn blip cone on or off
						CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_CONE, BlipID, bShowCone);
						// make sure blip is fully faded in
						CMiniMap::SetBlipParameter(BLIP_CHANGE_ALPHA, BlipID, 255);

						CMiniMap::SetBlipParameter(BLIP_CHANGE_SHOW_HEIGHT, BlipID, true);  // always show height

						CScriptHud::GetBlipFades().RemoveBlipFade(BlipID);
					}
				}

				bIsFadingOut = false;
				bChangeColour = false;
				// reset timers
				if( NetworkInterface::IsGameInProgress() )
				{
					iNoticableTimer  = GetNetworkGameTimer();
				}
				else
				{
					iNoticableTimer_SP = GetGameTimer();
				}
			}
			else
			{
				if( DoesBlipExist(VehicleBlipID) )
				{
					HandleBlipFade(true);
				}

				if( DoesBlipExist(BlipID) )
				{
					switch( iBlipGang )
					{
					case BLIP_GANG_PROFESSIONAL:
						{
							CRGBA rgba = CHudColour::GetRGBA((eHUD_COLOURS)HUD_COLOUR_NET_PLAYER29);
							CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, BlipID, blipColourOverride != BLIP_COLOUR_MAX ? blipColourOverride : (int)rgba.GetColor());
							SetBlipNameFromTextFile(BlipID, "PROFESSIONAL_BLIP");
						}
						break;

					case BLIP_GANG_FRIENDLY:
						CMiniMap::SetBlipParameter(BLIP_CHANGE_FRIENDLY, BlipID, true);
						break;

					case BLIP_GANG_ENEMY:
						CMiniMap::SetBlipParameter(BLIP_CHANGE_FRIENDLY, BlipID, false);
						break;
					}

					HandleBlipFade(false);

					if( bChangeColour )
					{
						if( NetworkInterface::IsGameInProgress() )
						{
							CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, BlipID, BLIP_COLOUR_RED);
						}
						bChangeColour = false;
					}
				}
			}
		}
		else
		{
			// Dead or injured
			// delete blip
			return true;
		}

		// Not injured or dead
		return false;
	}

	// No ped to reference
	return true;
}

void CScriptPedAIBlips::AIBlip::HandleBlipFade( bool isVehicle )
{
	float fPhase = 0.0f;
	if( bIsFadingOut == false )
	{
		bool bShouldFadeOut = false;

		if( NetworkInterface::IsGameInProgress() )
		{
			int time = GetNetworkGameTimer();
			if( (time - iNoticableTimer) > NOTICABLE_TIME )
			{
				bShouldFadeOut = true;
			}
		}
		else
		{
			if( (GetGameTimer() - iNoticableTimer_SP) > NOTICABLE_TIME )
			{
				bShouldFadeOut = true;
			}
		}

		if(bShouldFadeOut)
		{
			// time to start fading out
			if( NetworkInterface::IsGameInProgress() )
			{
				iFadeOutTimer = GetNetworkGameTimer();
			}
			else
			{
				iFadeOutTimer_SP = GetGameTimer();
			}
			bIsFadingOut = true;
		}
	}
	else
	{
		// set alpha
		if( NetworkInterface::IsGameInProgress() )
		{
			fPhase = (float)( GetNetworkGameTimer() - iFadeOutTimer ) / BLIP_FADE_PERIOD;
		}
		else
		{
			fPhase = (float)( GetGameTimer() - iFadeOutTimer_SP ) / BLIP_FADE_PERIOD;
		}

		if( fPhase > 1.0f )
		{
			// delete blip when fully faded out
			CleanupBlip();
		}
		else
		{
			int intAlpha = (int)(255.0f*(1.0f-fPhase));
			intAlpha = MAX(0, intAlpha);
			intAlpha = MIN(255, intAlpha);
			CMiniMap::SetBlipParameter(BLIP_CHANGE_ALPHA, isVehicle ? VehicleBlipID : BlipID, intAlpha );
		}
	}
}


bool CScriptPedAIBlips::AIBlip::IsPlayerFreeAimingAtEntity( CPed *pPlayer, CEntity *pTarget )
{
	if (pPlayer && pTarget)
	{
		if( !pPlayer->IsFatallyInjured() )
		{
			if(!NetworkInterface::IsGameInProgress() || !pPlayer->IsNetworkClone())
			{
				if(pPlayer->GetPlayerInfo())
				{
					const CPlayerPedTargeting & rPlayerTargeting = pPlayer->GetPlayerInfo()->GetTargeting();
					return (rPlayerTargeting.GetFreeAimTarget() == pTarget) || (rPlayerTargeting.GetFreeAimTargetRagdoll() == pTarget);
				}
			}
			else
			{
				CNetObjPlayer* netObjPlayer = SafeCast(CNetObjPlayer, pPlayer->GetNetworkObject());
				return ( netObjPlayer->IsFreeAimingLockedOnTarget() && pTarget == netObjPlayer->GetAimingTarget() );
			}
		}
	}
	return false;
}

bool CScriptPedAIBlips::AIBlip::IsPlayerTargettingEntity( CPed *pPlayer, CEntity *pTarget )
{
	if (pPlayer && pTarget && pPlayer->GetPlayerInfo())
	{
		if( !pPlayer->IsFatallyInjured() )
		{
			if (pPlayer->GetPlayerInfo()->GetTargeting().GetLockOnTarget() == pTarget)
			{
				return true;
			}
		}
	}
	return false;
}

float CScriptPedAIBlips::AIBlip::GetMaxRangeOfCurrentPedWeaponBlip(CPed *pPed)
{
	float range = -1.0f;
	if(pPed)
	{
		if(Verifyf( pPed->GetWeaponManager(), "Ped %s requires a weapon manager", pPed->GetModelName() ) )
		{
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon)
			{
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pWeapon->GetWeaponHash());
				if(pWeaponInfo)
				{
					range = pWeaponInfo->GetRange() * CScriptPedAIBlips::WEAPON_RANGE_MULTIPLIER_FOR_BLIPS;
				}
			}
		}
	}

	if( range > 400.0f )
		range = 400.0;

	return range;
}


bool CScriptPedAIBlips::AIBlip::DoesBlipExist(int iBlipIndex)
{
	if (iBlipIndex != INVALID_BLIP_ID)
	{
		return CMiniMap::IsBlipIdInUse(iBlipIndex);
	}
	return false;
}

int	CScriptPedAIBlips::AIBlip::GetBlipFromEntity(CEntity *pEntity)
{
	if(pEntity)
	{
		CMiniMapBlip* pBlip = NULL;

		if (pEntity->GetIsTypePed())
		{
			pBlip = CMiniMap::GetBlipAttachedToEntity(pEntity, BLIP_TYPE_CHAR, 0);
		}
		else if (pEntity->GetIsTypeVehicle())
		{
			pBlip = CMiniMap::GetBlipAttachedToEntity(pEntity, BLIP_TYPE_CAR, 0);
		}
		else if (pEntity->GetIsTypeObject())
		{
			CObject *pObject = static_cast<CObject*>(pEntity);

			if(pObject->m_nObjectFlags.bIsPickUp)
			{
				pBlip = CMiniMap::GetBlipAttachedToEntity(pObject, BLIP_TYPE_PICKUP_OBJECT, 0);
			}
			else
			{
				pBlip = CMiniMap::GetBlipAttachedToEntity(pObject, BLIP_TYPE_OBJECT, 0);
			}
		}

		if (pBlip)
		{
			//if we found a blip that's not in use, look again but for one that is in use
			if(pEntity->GetIsTypeVehicle() && !CMiniMap::IsBlipIdInUse(pBlip->m_iUniqueId))
			{
				pBlip = CMiniMap::GetBlipAttachedToEntity(pEntity, BLIP_TYPE_CAR, 0,  true);
			}
		}

		if (pBlip)
		{
			return (CMiniMap::GetUniqueBlipUsed(pBlip));
		}
	}

	return INVALID_BLIP_ID;
}

int	CScriptPedAIBlips::AIBlip::CreateBlipForVehicle(CVehicle *pVehicle,  bool isEnemyVehicle, bool useGreenVehicleBlip)
{
	return CreateBlipOnEntity(pVehicle, !isEnemyVehicle, useGreenVehicleBlip);
}


int	CScriptPedAIBlips::AIBlip::CreateBlipOnEntity(CEntity *pEntity, bool bIsFriendly, bool useGreenVehicleBlip)
{
	if( pEntity == NULL )
	{
		return INVALID_BLIP_ID;
	}

	int blipIDX = AddBlipForEntity(pEntity);

	if( pEntity->GetIsTypeVehicle() )
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, blipIDX, NetworkInterface::IsGameInProgress() ? CScriptPedAIBlips::BLIP_SIZE_NETWORK_VEHICLE : CScriptPedAIBlips::BLIP_SIZE_VEHICLE);
		if( !useGreenVehicleBlip )
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_FRIENDLY, blipIDX, bIsFriendly);
		}
		else
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, blipIDX, blipColourOverride != BLIP_COLOUR_MAX ? blipColourOverride : BLIP_COLOUR_GREEN);
		}
	}
	else if( pEntity->GetIsTypePed() )
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, blipIDX, NetworkInterface::IsGameInProgress() ? CScriptPedAIBlips::BLIP_SIZE_NETWORK_PED : CScriptPedAIBlips::BLIP_SIZE_PED);
		CMiniMap::SetBlipParameter(BLIP_CHANGE_FRIENDLY, blipIDX, bIsFriendly);
	}
	else if( pEntity->GetIsTypeObject() )
	{
		CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, blipIDX, NetworkInterface::IsGameInProgress() ? CScriptPedAIBlips::BLIP_SIZE_NETWORK_OBJECT : CScriptPedAIBlips::BLIP_SIZE_OBJECT);
	}
#if __BANK
	else
	{
		Assertf(0,"CScriptPedAIBlips::AIBlip::CreateBlipOnEntity() - Unknown Entity Type");
	}
#endif

	return blipIDX;
}


int	 CScriptPedAIBlips::AIBlip::AddBlipForEntity(CEntity *pEntity)
{
	int iNewBlipIndex = INVALID_BLIP_ID;

	if(pEntity)
	{
		eBLIP_TYPE blipType = BLIP_TYPE_UNUSED;
		float scale = 0.0f;

		CEntityPoolIndexForBlip entityPoolIndex;

		if (pEntity->GetIsTypePed())
		{
			blipType = BLIP_TYPE_CHAR;
			static_cast<CPed*>(pEntity)->SetPedConfigFlag( CPED_CONFIG_FLAG_BlippedByScript, true );

			entityPoolIndex = CEntityPoolIndexForBlip(pEntity, blipType);
		}
		else if (pEntity->GetIsTypeVehicle())
		{
			blipType = BLIP_TYPE_CAR;
			scale = 1.0f;

			entityPoolIndex = CEntityPoolIndexForBlip(pEntity, blipType);
		}
		else if (pEntity->GetIsTypeObject())
		{
			CObject *pObject = static_cast<CObject*>(pEntity);

			if(pObject->m_nObjectFlags.bIsPickUp)
			{
				blipType = BLIP_TYPE_PICKUP_OBJECT;
			}
			else
			{
				blipType = BLIP_TYPE_OBJECT;
			}
			entityPoolIndex = CEntityPoolIndexForBlip(pEntity, blipType);
		}
		else
		{
			Assertf(0, "CScriptPedAIBlips::AIBlip::AddBlipForEntity() - unrecognised entity type");
		}

		if ( (blipType != BLIP_TYPE_UNUSED) && (entityPoolIndex.IsValid()) )
		{

			// Allocate this as a script resource
			GtaThread *pGtaThread = static_cast<GtaThread*>(scrThread::GetThread(static_cast<scrThreadId>(scrThreadID)));
			if ( Verifyf((pGtaThread != NULL) && (pGtaThread->GetState() != scrThread::ABORTED),"CScriptPedAIBlips::AIBlip::AddBlipForEntity() - Trying to create a script resource on terminated script thread") )
			{
				CScriptResource_RadarBlip blip(blipType, entityPoolIndex, BLIP_DISPLAY_BOTH, pGtaThread->GetScriptName(), scale);
				iNewBlipIndex = pGtaThread->m_Handler->RegisterScriptResourceAndGetRef(blip);
			}

		}
	}

	Assertf(iNewBlipIndex != INVALID_BLIP_ID, " CScriptPedAIBlips::AIBlip::AddBlipForEntity - Failed to create blip for entity %s", pEntity->GetModelName());

	return iNewBlipIndex;
}

void CScriptPedAIBlips::AIBlip::RemoveBlip(s32 &blipID, bool bCheckExclusivity /* = false */)
{
	GtaThread *pGtaThread = static_cast<GtaThread*>(scrThread::GetThread(static_cast<scrThreadId>(scrThreadID)));
	if( (pGtaThread != NULL) && (pGtaThread->GetState() != scrThread::ABORTED) )
	{
		if( !bCheckExclusivity || CScriptPedAIBlips::IsExclusiveOwnerOfBlip(this, blipID) )
		{
			if( pGtaThread->bThisScriptCanRemoveBlipsCreatedByAnyScript )
			{
				CTheScripts::GetScriptHandlerMgr().RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_RADAR_BLIP, blipID); // searches all the script handler lists for the blip
			}
			else
			{
				if (!pGtaThread->m_Handler->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_RADAR_BLIP, blipID))
				{
					if (blipID != 0)
					{
						Warningf("CScriptPedAIBlips::AIBlip::RemoveBlip() - failed to find a blip with id %d for this script. Maybe the blip was created by another script or has already been deleted. It could also be that the blip was attached to a pickup or entity that has already been deleted.", blipID);
					}
				}
			}
		}
	}
	blipID = INVALID_BLIP_ID;
}

//This function should match script command GET_CORRECT_BLIP_PRIORITY() - but it doesn't.
//I'm only fixing known bugs at this point -JW
s32	CScriptPedAIBlips::AIBlip::GetCorrectBlipPriority(eBLIP_PRIORITY_TYPE type)
{
	switch(type)
	{
	case BLIP_PRIORITY_HIGHLIGHTED:
		// highest
		return BLIP_PRIORITY_HIGHEST;
		break;

		// high - highest
		// high
	case BLIP_PRIORITY_OTHER_TEAM:
		return BLIP_PRIORITY_HIGH;
		break;

		// med - high
	case BLIP_PRIORITY_SAME_TEAM:
	case BLIP_PRIORITY_ENEMY_AI:
		return BLIP_PRIORITY_MED_HIGH;
		break;

		// med - default - reserved for mission blips	

		// low - med
	case BLIP_PRIORITY_CUFFED_OR_KEYS:
		return BLIP_PRIORITY_LOW_MED;
		//break;

		// low

		// lowest-low
	case BLIP_PRIORITY_DEFAULT:
	case BLIP_PRIORITY_WANTED:
		return BLIP_PRIORITY_LOW_LOWEST;
		//break;

	default:
		break;

	}
	return BLIP_PRIORITY_LOWEST;
}

eHUD_COLOURS CScriptPedAIBlips::AIBlip::GetBlipHudColour(s32 iBlipIndex)
{
	eHUD_COLOURS iReturnHudColour = HUD_COLOUR_WHITE;
	if (Verifyf(iBlipIndex != INVALID_BLIP_ID, " - Blip %d doesn't exist", iBlipIndex))
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(iBlipIndex);
		if (pBlip)
		{
			CMiniMap_Common::GetColourFromBlipSettings(CMiniMap::GetBlipColourValue(pBlip), CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_BRIGHTNESS), &iReturnHudColour);
		}
	}
	return iReturnHudColour;
}

void CScriptPedAIBlips::AIBlip::SetBlipNameFromTextFile(s32 iBlipIndex, const char *pTextLabel)
{
	if (Verifyf(iBlipIndex != INVALID_BLIP_ID, "SetBlipNameFromTextFile - Blip %d doesn't exist", iBlipIndex))
	{
		if (Verifyf(strlen(pTextLabel) < MAX_BLIP_NAME_SIZE, "SET_BLIP_NAME_FROM_TEXT_FILE - text label is too long"))
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_NAME, iBlipIndex, pTextLabel);
		}
	}
}

void CScriptPedAIBlips::AIBlip::CleanupBlip()
{

	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedID, 0);
	if( pPed )
	{
		if (NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
		{
			scriptDebugf1("Removing network ped blip : %s (%p)",pPed->GetNetworkObject()->GetLogName(), pPed);
		}
	}

	if(DoesBlipExist(BlipID))
	{
		RemoveBlip(BlipID);
	}
	if(DoesBlipExist(VehicleBlipID))
	{
		RemoveBlip(VehicleBlipID, true);
	}
	BlipID = INVALID_BLIP_ID;
	VehicleBlipID = INVALID_BLIP_ID;
	// clear fading flag, otherwise system will never re-create the blip
	bIsFadingOut = false;
}

bool CScriptPedAIBlips::AIBlip::IsPedNoticableToPlayer(CPed *pPed, CPed *pPlayerPed, float fNoticableRadius, bool bRespondingToSpecialAbility, int iResponseTimer, int iResponseTime)
{
	if( pPed && pPlayerPed )		// Should not be required
	{
		if( !pPed->IsInjured() && !pPlayerPed->IsInjured() )
		{
			Vector3	deltaV = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
			float dist = deltaV.Mag();

			// if ped is shooting and within range of the player then blip them
			if( pPed->GetPedResetFlag( CPED_RESET_FLAG_FiringWeapon ) )
			{
				if( dist < GetMaxRangeOfCurrentPedWeaponBlip(pPed) )
				{
					return true;
				}
			}

			// if ped is running or shoot and within range of the player them blip them
			if( pPed->GetMotionData()->GetIsSprinting() || pPed->GetMotionData()->GetIsRunning() )
			{
				if( dist < fNoticableRadius )
				{
					return true;
				}
			}

			// if ped is involved in currently ongoing scripted conversation (in shootouts it might mean one line random dialogue)
			if( ( (g_ScriptAudioEntity.IsScriptedConversationOngoing() && !g_ScriptAudioEntity.IsScriptedConversationAMobileCall() ) &&	g_ScriptAudioEntity.IsPedInCurrentConversation(pPed) )
				|| ( pPed->GetSpeechAudioEntity() && pPed->GetSpeechAudioEntity()->IsAmbientSpeechPlaying() ) )
			{
				if(dist < CScriptPedAIBlips::HEARD_RADIUS)
				{
					return true;
				}
			}

			// if the player is pointing their weapon at the ped within range
			if( dist < fNoticableRadius )
			{
				if( IsPlayerFreeAimingAtEntity(pPlayerPed, pPed) || IsPlayerTargettingEntity(pPlayerPed, pPed) )
				{
					return true;
				}
			}

			// if ped is any vehicle
			if( pPed->GetIsInVehicle() )
			{

				// Always noticeable in a heli
				CVehicle *pVehicle = pPed->GetVehiclePedInside();
				if( pVehicle )
				{
					if ( pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE )
					{
						return true;
					}
				}

				const bool bSuppressNonHostileInVehicle = ( NetworkInterface::IsGameInProgress()
						&& !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) );

				if (( dist < fNoticableRadius ) && !bSuppressNonHostileInVehicle)
				{
					return true;
				}
			}

			///////////////////////////////////////////////////////////////////////////////
			// if is ped responding to Trevor's special ability, singleplayer only
			// THIS ISN'T REALLY BLIPS TBH, reproduced for fidelity, not sure if this'll be an issue.
			if( !NetworkInterface::IsGameInProgress() )
			{
				if( bRespondingToSpecialAbility == false)
				{
					if (pPlayerPed && CTheScripts::IsPlayerPlaying(pPlayerPed))
					{
						if(!pPlayerPed->IsInjured())
						{
							CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfo(pPlayerPed->GetModelId());
							u32 hashKey = pModel->GetHashKey();
							if(hashKey == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash())
							{
								pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_ForceCombatTaunt, true );

								CPlayerSpecialAbility* specialAbility = pPlayerPed->GetSpecialAbility();
								if(specialAbility && specialAbility->IsActive())
								{
									if(pPlayerPed->GetSpeechAudioEntity() && !pPlayerPed->GetSpeechAudioEntity()->IsAmbientSpeechPlaying())
									{
										iResponseTime				= 1000 + CRandomScripts::GetRandomNumberInRange(0, 501);
										iResponseTimer 				= GetGameTimer();
										bRespondingToSpecialAbility = true;
									}	
								}
							}
						}
					}
				}
				else
				{
					if(GetGameTimer() - iResponseTimer > iResponseTime)
					{
						bRespondingToSpecialAbility = false;
						return true;
					}
				}
			}
			///////////////////////////////////////////////////////////////////////////////

		}
	}

	return false;
}



