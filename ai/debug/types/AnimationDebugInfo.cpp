#include "ai/debug/types/AnimationDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

// game headers
#include "ik/IkManagerSolverTypes.h"
#include "ik/solvers/ArmIkSolver.h"
#include "ik/solvers/BodyLookSolver.h"
#include "ik/solvers/BodyLookSolverProxy.h"
#include "ik/solvers/HeadSolver.h"
#include "ik/solvers/LegSolver.h"
#include "ik/solvers/LegSolverProxy.h"
#include "ik/solvers/QuadLegSolver.h"
#include "ik/solvers/QuadrupedReactSolver.h"
#include "ik/solvers/RootSlopeFixupSolver.h"
#include "ik/solvers/TorsoReactSolver.h"
#include "ik/solvers/TorsoSolver.h"
#include "ik/solvers/TorsoVehicleSolver.h"
#include "ik/solvers/TorsoVehicleSolverProxy.h"
#include "Peds/Ped.h"
#include "Peds/PedIKSettings.h"
#include "scene/DynamicEntity.h"

// rage headers
#include "ai/aichannel.h"
#include "crmetadata/property.h"
#include "crmetadata/propertyattributes.h"
#include "crmotiontree/nodeblend.h"
#include "crmotiontree/nodeblendn.h"
#include "crmotiontree/nodeclip.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/animhelpers.h"
#include "fwanimation/directorcomponentmotiontree.h"
#include "fwscene/stores/clipdictionarystore.h"

atString CAnimClipDebugIterator::GetClipDebugInfo(const crClip* pClip, float Time, float weight, bool& addedContent, bool& preview, float rate/* = 1.0f*/, bool hideRate /*= false*/, bool hideName /*= false*/)
{
	atString clipDictionaryName("UNKNOWN");
	atString addedContentStr;
	atString patchedContentStr;
	bool patchContent = false;
	addedContent = false;
	preview = false;

	const crClipDictionary* pClipDict = pClip->GetDictionary();
	if (pClipDict)
	{
		/* Iterate through the clip dictionaries in the clip dictionary store */
		const crClipDictionary *clipDictionary = NULL;
		strLocalIndex clipDictionaryIndex(0), clipDictionaryCount(g_ClipDictionaryStore.GetSize());
		for (; clipDictionaryIndex < clipDictionaryCount; clipDictionaryIndex.Set(clipDictionaryIndex.Get() + 1))
		{
			if (g_ClipDictionaryStore.IsValidSlot(clipDictionaryIndex))
			{
				clipDictionary = g_ClipDictionaryStore.Get(clipDictionaryIndex);
				if(clipDictionary == pClip->GetDictionary())
				{
					clipDictionaryName = g_ClipDictionaryStore.GetName(clipDictionaryIndex);

					if (clipDictionaryIndex.IsValid())
					{
						strStreamingModule* pModule = g_ClipDictionaryStore.GetStreamingModule();
						strIndex index         = pModule->GetStreamingIndex(clipDictionaryIndex);
						const strStreamingInfo* info = strStreamingEngine::GetInfo().GetStreamingInfo(index);

						preview = (fiCollection::GetCollectionIndex(info->GetHandle()) == 0);

						strStreamingFile* file = strPackfileManager::GetImageFileFromHandle(info->GetHandle());
						patchContent           = file && (file->m_IsPatchfile || file->m_IsOverlay || file->m_bNew);
						if (patchContent)
						{
							if (const char* szPatchedContentPath = file->m_name.TryGetCStr())
							{
								atString patchedContentPath(szPatchedContentPath);
								atString dummy;
								patchedContentPath.Split(patchedContentStr, dummy, ':');
							}
						}
					}

					break;
				}
			}
		}
	}

	atString outText;

	outText += atVarString("W:%.3f", weight);

	atString clipName(pClip->GetName());
	fwAnimHelpers::GetDebugClipName(*pClip, clipName);

	atString userName;
	const crProperties *pProperties = pClip->GetProperties();
	if(pProperties)
	{
		static u32 uUserKey = ATSTRINGHASH("SourceUserName", 0x69497CB4);
		const crProperty *pProperty = pProperties->FindProperty(uUserKey);
		if(pProperty)
		{
			const crPropertyAttribute *pPropertyAttribute = pProperty->GetAttribute(uUserKey);
			if(pPropertyAttribute && pPropertyAttribute->GetType() == crPropertyAttribute::kTypeInt)
			{
				const crPropertyAttributeInt *pPropertyAttributeInt = static_cast< const crPropertyAttributeInt * >(pPropertyAttribute);

				int iUserHash = pPropertyAttributeInt->GetInt();
				u32 uUserHash = *reinterpret_cast< u32 * >(&iUserHash);
				const char *szUser = atHashString(uUserHash).TryGetCStr();
				if(szUser)
				{
					userName = szUser;
				}
				else
				{
					userName = atVarString("%u", uUserHash);
				}
			}
		}
	}

	outText += atVarString(", %s/%s, T:%.3f", clipDictionaryName.c_str(), clipName.c_str(), Time);

	if (pClip->GetDuration() > 0.0f)
	{
		outText += atVarString(", P:%.3f", Time / pClip->GetDuration());
	}

	if (!hideRate)
	{
		outText += atVarString(", R:%.3f", rate);
	}

	if (!hideName)
	{
		outText += atVarString(", %s", userName.c_str() ? userName.c_str() : "No User");
	}

	if (patchContent)
	{
		outText += atVarString(", %s", patchedContentStr.c_str());
	}

	if (addedContent)
	{
		outText += atVarString(", %s", addedContentStr.c_str());
	}

	if (preview)
	{
		outText += ", PREVIEW";
	}

	addedContent |= patchContent;

	return outText; 
}

CAnimClipDebugIterator::CAnimClipDebugIterator(CAnimationDebugInfo* pAnimDebugInfo, crmtNode* UNUSED_PARAM(referenceNode))
	: m_AnimDebugInfo(pAnimDebugInfo) 
{ 
};

void CAnimClipDebugIterator::Visit(crmtNode& node)
{
	crmtIterator::Visit(node);

	if (node.GetNodeType()==crmtNode::kNodeClip)
	{
		crmtNodeClip* pClipNode = static_cast<crmtNodeClip*>(&node);
		if (pClipNode 
			&& pClipNode->GetClipPlayer().HasClip())
		{
			//get the weight from the parent node (if it exists)
			float weight = CalcVisibleWeight(node);
			//if (weight > 0.0f)
			{
				const crClip* clip = pClipNode->GetClipPlayer().GetClip();
				bool addedContent = false;
				bool preview = false;
				/* To display the clip dictionary name we need to find the clip dictionary index in the clip dictionary store */

				atString outText = GetClipDebugInfo(clip, pClipNode->GetClipPlayer().GetTime(), weight, addedContent, preview); 
				Color32 colour = addedContent ? CRGBA(0,150,255,255) : CRGBA(255,192,96,255);

				m_AnimDebugInfo->ColorPrintLn(colour, outText);
			}
		}
	}
}

float CAnimClipDebugIterator::CalcVisibleWeight(crmtNode& node)
{
	crmtNode * pNode = &node;

	float weight = 1.0f;

	crmtNode* pParent = pNode->GetParent();

	while (pParent)
	{
		float outWeight = 1.0f;

		switch(pParent->GetNodeType())
		{
		case crmtNode::kNodeBlend:
			{
				crmtNodePairWeighted* pPair = static_cast<crmtNodePairWeighted*>(pParent);

				if (pNode==pPair->GetFirstChild())
				{
					outWeight = 1.0f - pPair->GetWeight();
				}
				else
				{
					outWeight = pPair->GetWeight();
				}
			}
			break;
		case crmtNode::kNodeBlendN:
			{
				crmtNodeN* pN = static_cast<crmtNodeN*>(pParent);
				u32 index = pN->GetChildIndex(*pNode);
				outWeight = pN->GetWeight(index);
			}
			break;
		default:
			outWeight = 1.0f;
			break;
		}

		weight*=outWeight;

		pNode = pParent;
		pParent = pNode->GetParent();
	}

	return weight;
}

CAnimationDebugInfo::CAnimationDebugInfo(const CDynamicEntity* pDyn, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Dyn(pDyn)
{

}

void CAnimationDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("ANIMATIONS");
		PushIndent();
			PushIndent();
				PushIndent();
					PrintAnimations();

}

bool CAnimationDebugInfo::ValidateInput()
{
	if (!m_Dyn)
		return false;

	if (!m_Dyn->GetAnimDirector())
		return false;

	return true;
}

void CAnimationDebugInfo::PrintAnimations()
{
	CAnimClipDebugIterator it (this);
	m_Dyn->GetAnimDirector()->GetMove().GetMotionTree().Iterate(it);

	fwAnimDirectorComponentMotionTree* pComponentMotionTreeMidPhysics = m_Dyn->GetAnimDirector()->GetComponentByPhase<fwAnimDirectorComponentMotionTree>(fwAnimDirectorComponent::kPhaseMidPhysics);
	if (pComponentMotionTreeMidPhysics)
	{
		crmtNode *pRootNode = pComponentMotionTreeMidPhysics->GetMotionTree().GetRootNode();
		if (pRootNode)
		{
			it.Start();
			it.Iterate(*pRootNode);
			it.Finish();
		}
	}
}

// In sync with enum eLookIkReturnCode (IkManager.h)
static const char* aszLookIkReturnCode[] =
{
	"None",						// LOOKIK_RC_NONE
	"Solver Disabled",			// LOOKIK_RC_DISABLED
	"Invalid Param",			// LOOKIK_RC_INVALID_PARAM
	"Ped Dead",					// LOOKIK_RC_CHAR_DEAD
	"Blocked by script/code",	// LOOKIK_RC_BLOCKED
	"Blocked by anim",			// LOOKIK_RC_BLOCKED_BY_ANIM
	"Blocked by tag",			// LOOKIK_RC_BLOCKED_BY_TAG
	"Ped offscreen",			// LOOKIK_RC_OFFSCREEN
	"Ped out of range",			// LOOKIK_RC_RANGE
	"Target outside FOV",		// LOOKIK_RC_FOV
	"Blocked by priority",		// LOOKIK_RC_PRIORITY
	"Invalid target",			// LOOKIK_RC_INVALID_TARGET
	"Solver timed out",			// LOOKIK_RC_TIMED_OUT
	"Solver aborted"			// LOOKIK_RC_ABORT
};

CIkDebugInfo::CIkDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams)
	: CAIDebugInfo(rPrintParams)
	, m_Ped(pPed)
{

}

void CIkDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("IK");
		PushIndent(3);
			PrintIkDebug();
}

bool CIkDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	return true;
}

void CIkDebugInfo::PrintIkDebug()
{
	CPed& rPed = const_cast<CPed&>(*m_Ped.Get());
	Color32 colour(79,79,255);

#if ENABLE_HEAD_SOLVER
	// Head solver
	if (rPed.GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeHead)))
	{
		ColorPrintLn(colour, "Head Solver : Not supported");
	}
	else
	{
		CHeadIkSolver* pHeadSolver = static_cast<CHeadIkSolver*>(rPed.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeHead));
		if (pHeadSolver)
		{
			ColorPrintLn(colour, "Head Solver : Blend(%f)", pHeadSolver->GetHeadBlend());
		}
		else
		{
			ColorPrintLn(colour, "Head Solver : Supported but not active");
		}
	}
#endif // ENABLE_HEAD_SOLVER

	// Body look solver
	if (rPed.GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeBodyLook)))
	{
		ColorPrintLn(colour, "Body Look Solver : Not supported");
	}
	else
	{
		CBodyLookIkSolverProxy* pProxy = static_cast<CBodyLookIkSolverProxy*>(rPed.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeBodyLook));
		CBodyLookIkSolver* pBodyLookSolver = pProxy ? static_cast<CBodyLookIkSolver*>(pProxy->GetSolver()) : NULL;
		if (pBodyLookSolver)
		{
			ColorPrintLn(colour, "Body Look Solver : Blend Torso(%4.2f), Neck(%4.2f), Head(%4.2f), Eyes(%4.2f)",	pBodyLookSolver->GetBlend(CBodyLookIkSolver::COMP_TORSO),
				pBodyLookSolver->GetBlend(CBodyLookIkSolver::COMP_NECK),
				pBodyLookSolver->GetBlend(CBodyLookIkSolver::COMP_HEAD),
				pBodyLookSolver->GetBlend(CBodyLookIkSolver::COMP_EYE));
		}
		else
		{
			ColorPrintLn(colour, "Body Look Solver : Supported but not active\n");
		}
	}

#if __DEV
	ColorPrintLn(colour, "Body Look Solver : Last return code(%s)", aszLookIkReturnCode[(int)rPed.GetIkManager().GetLookIkReturnCode()]);
#endif // __DEV

	// Torso solver
	if (rPed.GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeTorso)))
	{
		ColorPrintLn(colour, "Torso Solver : Not supported");
	}
	else
	{
		CTorsoIkSolver* pTorsoSolver = static_cast<CTorsoIkSolver*>(rPed.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeTorso));
		if (pTorsoSolver)
		{
			ColorPrintLn(colour, "Torso Solver : Blend(%f)", pTorsoSolver->GetTorsoBlend());
		}
		else
		{
			ColorPrintLn(colour, "Torso Solver : Supported but not active");
		}
	}

	// Torso react solver
	if (rPed.GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeTorsoReact)))
	{
		ColorPrintLn(colour, "Torso React Solver : Not supported");
	}
	else
	{
		CTorsoReactIkSolver* pTorsoReactSolver = static_cast<CTorsoReactIkSolver*>(rPed.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeTorsoReact));
		if (pTorsoReactSolver)
		{
			ColorPrintLn(colour, "Torso React Solver : Active");
		}
		else
		{
			ColorPrintLn(colour, "Torso React Solver : Supported but not active");
		}
	}

	// Torso vehicle solver
	if (rPed.GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeTorsoVehicle)))
	{
		ColorPrintLn(colour, "Torso Vehicle Solver : Not supported");
	}
	else
	{
		CTorsoVehicleIkSolverProxy* pProxy = static_cast<CTorsoVehicleIkSolverProxy*>(rPed.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeTorsoVehicle));
		CTorsoVehicleIkSolver* pTorsoVehicleSolver = pProxy ? static_cast<CTorsoVehicleIkSolver*>(pProxy->GetSolver()) : NULL;
		if (pTorsoVehicleSolver)
		{
			ColorPrintLn(colour, "Torso Vehicle Solver : Active");
		}
		else
		{
			ColorPrintLn(colour, "Torso Vehicle Solver : Supported but not active");
		}
	}

	// Root slope fixup solver
	if (rPed.GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup)))
	{
		ColorPrintLn(colour, "Root Slope Fixup Solver : Not supported");
	}
	else
	{
		CRootSlopeFixupIkSolver* pRootSlopeFixupSolver = static_cast<CRootSlopeFixupIkSolver*>(rPed.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
		if (pRootSlopeFixupSolver)
		{
			ColorPrintLn(colour, "Root Slope Fixup Solver : Active");
		}
		else
		{
			ColorPrintLn(colour, "Root Slope Fixup Solver : Supported but not active");
		}
	}

	// Arm solver
	if (rPed.GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeArm)))
	{
		ColorPrintLn(colour, "Arm Solver : Not supported");
	}
	else
	{
		CArmIkSolver* pArmIkSolver = static_cast<CArmIkSolver*>(rPed.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeArm));
		if (pArmIkSolver /*&& pArmIkSolver->GetArmActive(crIKSolverArms::LEFT_ARM)*/)
		{
			ColorPrintLn(colour, "Arm Solver - Left : Blend(%f)", pArmIkSolver->GetArmBlend(crIKSolverArms::LEFT_ARM));
		}
		else
		{
			ColorPrintLn(colour, "Arm Solver - Left : Supported but not active");
		}

		if (pArmIkSolver /*&& pArmIkSolver->GetArmActive(crIKSolverArms::RIGHT_ARM)*/)
		{
			ColorPrintLn(colour, "Arm Solver - Right: Blend(%f)", pArmIkSolver->GetArmBlend(crIKSolverArms::RIGHT_ARM));
		}
		else
		{
			ColorPrintLn(colour, "Arm Solver - Right : Supported but not active");
		}
	}

	// Leg solver
	if (rPed.GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeLeg)))
	{
		ColorPrintLn(colour, "Leg Solver : Not supported");
	}
	else
	{
		CLegIkSolverProxy* pProxy = static_cast<CLegIkSolverProxy*>(rPed.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeLeg));
		CLegIkSolver* pLegSolver = pProxy ? static_cast<CLegIkSolver*>(pProxy->GetSolver()) : NULL;
		if (pLegSolver)
		{
			u32 legIkMode = rPed.m_PedConfigFlags.GetPedLegIkMode();
			if (legIkMode == LEG_IK_MODE_FULL)
			{
				ColorPrintLn(colour, "Leg Solver - LEG_IK_MODE_FULL");
			}
			else if (legIkMode == LEG_IK_MODE_FULL_MELEE)
			{
				ColorPrintLn(colour, "Leg Solver - LEG_IK_MODE_FULL_MELEE");
			}
			else if (legIkMode == LEG_IK_MODE_PARTIAL)
			{
				ColorPrintLn(colour, "Leg Solver - LEG_IK_MODE_PARTIAL");
			}
			else if (legIkMode == LEG_IK_MODE_OFF)
			{
				ColorPrintLn(colour, "Leg Solver : LEG_IK_MODE_OFF");
			}
		}
		else
		{
			ColorPrintLn(colour, "Leg Solver : Supported but not active");
		}

		if (pLegSolver)
		{
			ColorPrintLn(colour, "Leg Solver - Pelvis : Blend(%f)", pLegSolver->GetPelvisBlend());
		}

		if (pLegSolver)
		{
			ColorPrintLn(colour, "Leg Solver - Left : Blend(%f)", pLegSolver->GetLegBlend(CLegIkSolver::LEFT_LEG));
		}

		if (pLegSolver)
		{
			ColorPrintLn(colour, "Leg Solver - Right : Blend(%f)", pLegSolver->GetLegBlend(CLegIkSolver::RIGHT_LEG));
		}

		u8 externallyDrivenDOFs = rPed.GetPedModelInfo()->GetExternallyDrivenDOFs();
		if (externallyDrivenDOFs & HIGH_HEELS)
		{
			ColorPrintLn(colour, "High heel expression dof : Value(%f)", rPed.GetHighHeelExpressionDOF());
			ColorPrintLn(colour, "High heel expression meta dof : Value(%f)", rPed.GetHighHeelExpressionMetaDOF());
		}
	}

#if IK_QUAD_LEG_SOLVER_ENABLE
	// Quad leg solver
	if (rPed.GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeQuadLeg)))
	{
		ColorPrintLn(colour, "Quad Leg Solver : Not supported");
	}
	else
	{
		CQuadLegIkSolverProxy* pProxy = static_cast<CQuadLegIkSolverProxy*>(rPed.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeQuadLeg));
		CQuadLegIkSolver* pLegSolver = pProxy ? static_cast<CQuadLegIkSolver*>(pProxy->GetSolver()) : NULL;
		if (pLegSolver)
		{
			ColorPrintLn(colour, "Quad Leg Solver : Active");
		}
		else
		{
			ColorPrintLn(colour, "Quad Leg Solver : Supported but not active");
		}
	}
#endif // IK_QUAD_LEG_SOLVER_ENABLE

#if IK_QUAD_REACT_SOLVER_ENABLE
	// Quadruped react solver
	if (rPed.GetIKSettings().IsIKFlagDisabled(CBaseIkManager::eIkSolverType(IKManagerSolverTypes::ikSolverTypeQuadrupedReact)))
	{
		ColorPrintLn(colour, "Quadruped React Solver : Not supported");
	}
	else
	{
		CQuadrupedReactSolver* pQuadrupedReactSolver = static_cast<CQuadrupedReactSolver*>(rPed.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeQuadrupedReact));
		if (pQuadrupedReactSolver)
		{
			ColorPrintLn(colour, "Quadruped React Solver : Active");
		}
		else
		{
			ColorPrintLn(colour, "Quadruped React Solver : Supported but not active");
		}
	}
#endif // IK_QUAD_REACT_SOLVER_ENABLE
}

#endif // AI_DEBUG_OUTPUT_ENABLED