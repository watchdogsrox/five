#include "ai\debug\types\AttachmentsDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "entity\attachmententityextension.h"
#include "scene\Physical.h"

const char* CAttachmentsDebugInfo::GetAttachStateString(s32 i)
{
	switch (i)
	{
		case ATTACH_STATE_NONE : return "ATTACH_STATE_NONE";
		case ATTACH_STATE_BASIC : return "ATTACH_STATE_BASIC";
		case ATTACH_STATE_DETACHING : return "ATTACH_STATE_DETACHING";
		case ATTACH_STATE_PHYSICAL : return "ATTACH_STATE_PHYSICAL";
		case ATTACH_STATE_WORLD : return "ATTACH_STATE_WORLD";
		case ATTACH_STATE_WORLD_PHYSICAL : return "ATTACH_STATE_WORLD_PHYSICAL";
		case ATTACH_STATE_PED_WITH_ROT : return "ATTACH_STATE_PED_WITH_ROT";
		case ATTACH_STATE_RAGDOLL : return "ATTACH_STATE_RAGDOLL";
		case ATTACH_STATE_PED_ENTER_CAR : return "ATTACH_STATE_PED_ENTER_CAR";
		case ATTACH_STATE_PED_IN_CAR : return "ATTACH_STATE_PED_IN_CAR";
		case ATTACH_STATE_PED_EXIT_CAR : return "ATTACH_STATE_PED_EXIT_CAR";
		case ATTACH_STATE_PED_ON_GROUND : return "ATTACH_STATE_PED_ON_GROUND";
		default: break;
	}
	return "ATTACH_STATE_UNKNOWN";
}

CAttachmentsDebugInfo::CAttachmentsDebugInfo(const CPhysical* pPhys, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Phys(pPhys)
{

}

void CAttachmentsDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("ATTACHMENTS");
		PushIndent();
			PushIndent();
				PushIndent();
					PrintAttachments();
}

bool CAttachmentsDebugInfo::ValidateInput()
{
	if (!m_Phys)
		return false;

	return true;
}

void CAttachmentsDebugInfo::PrintAttachments()
{
	ColorPrintLn(Color_brown, "Attach Parent = %p (%s), Attach Bone = %i", m_Phys->GetAttachParent(), m_Phys->GetAttachParent() ? m_Phys->GetAttachParent()->GetModelName() : "NULL", m_Phys->GetAttachBone());
	ColorPrintLn(Color_green, "------------");
	ColorPrintLn(Color_green, "ATTACH STATE");
	ColorPrintLn(Color_green, "------------");
	ColorPrintLn(Color_orange, GetAttachStateString(m_Phys->GetAttachState()));
	ColorPrintLn(Color_green, "ATTACH FLAGS");
	ColorPrintLn(Color_green, "------------");
	
	if (m_Phys->GetAttachFlag(ATTACH_FLAG_ATTACHED_TO_WORLD_BASIC)) ColorPrintLn(Color_blue, "ATTACH_FLAG_ATTACHED_TO_WORLD_BASIC");
	if (m_Phys->GetAttachFlag(ATTACH_FLAG_POS_CONSTRAINT)) ColorPrintLn(Color_blue, "ATTACH_FLAG_POS_CONSTRAINT");
	if (m_Phys->GetAttachFlag(ATTACH_FLAG_ROT_CONSTRAINT)) ColorPrintLn(Color_blue, "ATTACH_FLAG_ROT_CONSTRAINT");
	if (m_Phys->GetAttachFlag(ATTACH_FLAG_DO_PAIRED_COLL)) ColorPrintLn(Color_blue, "ATTACH_FLAG_DO_PAIRED_COLL");
	if (m_Phys->GetAttachFlag(ATTACH_FLAG_INITIAL_WARP)) ColorPrintLn(Color_blue, "ATTACH_FLAG_INITIAL_WARP");
	if (m_Phys->GetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_NULL_ENTITY)) ColorPrintLn(Color_blue, "ATTACH_FLAG_DONT_ASSERT_ON_NULL_ENTITY");
	if (m_Phys->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_DEATH)) ColorPrintLn(Color_blue, "ATTACH_FLAG_AUTODETACH_ON_DEATH");
	if (m_Phys->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL)) ColorPrintLn(Color_blue, "ATTACH_FLAG_AUTODETACH_ON_RAGDOLL");
	if (m_Phys->GetAttachFlag(ATTACH_FLAG_COL_ON)) ColorPrintLn(Color_blue, "ATTACH_FLAG_COL_ON");
	if (m_Phys->GetAttachFlag(ATTACH_FLAG_DELETE_WITH_PARENT)) ColorPrintLn(Color_blue, "ATTACH_FLAG_DELETE_WITH_PARENT");

	ColorPrintLn(Color_blue, "------------");
	ColorPrintLn(Color_blue, "ATTACH OFFSETS");
	ColorPrintLn(Color_blue, "------------");

	Vector3 vAttachOffset = m_Phys->GetAttachOffset();
	Quaternion qAttachOffset = m_Phys->GetAttachQuat();

	Vector3 vAttachRotEulers;
	qAttachOffset.ToEulers(vAttachRotEulers);

	ColorPrintLn(Color_blue, "POS OFFSET : X: %.4f, Y: %.4f, Z: %.4f", vAttachOffset.x, vAttachOffset.y, vAttachOffset.z);
	ColorPrintLn(Color_blue, "ROT OFFSET (EULERS) : X: %.4f, Y: %.4f, Z: %.4f", vAttachRotEulers.x, vAttachRotEulers.y, vAttachRotEulers.z);
}

#endif // AI_DEBUG_OUTPUT_ENABLED