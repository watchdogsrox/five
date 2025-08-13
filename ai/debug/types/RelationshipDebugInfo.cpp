#include "ai\debug\types\RelationshipDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

// game headers
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "network/objects/entities/NetObjPlayer.h"

CRelationshipDebugInfo::CRelationshipDebugInfo(const CPed* pPed, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
{

}

void CRelationshipDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("RELATIONSHIPS");
		PushIndent();
			PushIndent();
				PushIndent();
					PrintRelationships();
}

bool CRelationshipDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	if (!m_Ped->GetPedIntelligence())
		return false;

	if (!m_Ped->GetPedIntelligence()->GetRelationshipGroup())
		return false;

	return true;
}

void CRelationshipDebugInfo::PrintRelationships()
{
	ColorPrintLn(Color_red, NetworkInterface::FriendlyFireAllowed() ? "FRIENDLY FIRE ON" : "FRIENDLY FIRE OFF");

	const CRelationshipGroup* pRelationshipGroup = m_Ped->GetPedIntelligence()->GetRelationshipGroup();
	const char* szRelGroupName = pRelationshipGroup->GetName().GetCStr();

	ColorPrintLn(Color_blue, szRelGroupName);

	const bool bTreatDislikeAsHateWhenInCombat = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_TreatDislikeAsHateWhenInCombat);
	ColorPrintLn((bTreatDislikeAsHateWhenInCombat ? Color_yellow : Color_grey), "TreatDislikeAsHateWhenInCombat: %s", (bTreatDislikeAsHateWhenInCombat ? "true" : "false"));

	const bool bTreatNonFriendlyAsHateWhenInCombat = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_TreatNonFriendlyAsHateWhenInCombat);
	ColorPrintLn((bTreatNonFriendlyAsHateWhenInCombat ? Color_yellow : Color_grey), "TreatNonFriendlyAsHateWhenInCombat: %s", (bTreatNonFriendlyAsHateWhenInCombat ? "true" : "false"));

	bool bInPassiveMode = false;
	if(NetworkInterface::IsGameInProgress() && m_Ped->IsPlayer())
	{
		if (m_Ped->GetNetworkObject())
		{
			CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(m_Ped->GetNetworkObject());
			if (pNetObjPlayer && pNetObjPlayer->IsPassiveMode())
			{
				bInPassiveMode = true;
			}
		}
	}
	ColorPrintLn((bInPassiveMode ? Color_yellow : Color_grey), "InPassiveMode: %s", (bInPassiveMode ? "true" : "false"));

	for (s32 i = 0; i < CRelationshipGroup::GetPool()->GetSize(); i++)
	{
		const CRelationshipGroup* pOtherGroup = CRelationshipManager::FindRelationshipGroupByIndex(i);
		if (pOtherGroup)
		{
			aiAssert((int)pOtherGroup->GetIndex() == i);
			for (eRelationType type = (eRelationType) 0; type < MAX_NUM_ACQUAINTANCE_TYPES; type = (eRelationType) (type + 1))
			{
				if (pRelationshipGroup->CheckRelationship(type, i))
				{
					switch(type)
					{
					case ACQUAINTANCE_TYPE_PED_RESPECT:
						ColorPrintLn(Color_green, "   Respects: %s", pOtherGroup->GetName().GetCStr());
						break;
					case ACQUAINTANCE_TYPE_PED_LIKE:
						ColorPrintLn(Color_green, "   Likes: %s", pOtherGroup->GetName().GetCStr());
						break;
					case ACQUAINTANCE_TYPE_PED_IGNORE:
						ColorPrintLn(Color_orange, "   Ignores: %s", pOtherGroup->GetName().GetCStr());
						break;
					case ACQUAINTANCE_TYPE_PED_DISLIKE:
						ColorPrintLn(Color_red, "   Dislikes: %s", pOtherGroup->GetName().GetCStr());
						break;
					case ACQUAINTANCE_TYPE_PED_WANTED:
						ColorPrintLn(Color_red, "   Wanted: %s", pOtherGroup->GetName().GetCStr());
						break;
					case ACQUAINTANCE_TYPE_PED_HATE:
						ColorPrintLn(Color_red, "   Hates: %s", pOtherGroup->GetName().GetCStr());
						break;
					case ACQUAINTANCE_TYPE_PED_DEAD:
						ColorPrintLn(Color_orange, "   Dead: %s", pOtherGroup->GetName().GetCStr());
						break;
					default:
						break;
					}
				}
			}
		}
	}
}

#endif // AI_DEBUG_OUTPUT_ENABLED