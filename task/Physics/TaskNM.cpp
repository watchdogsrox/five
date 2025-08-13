// Filename   :	TaskNM.cpp
// Description:	FSM base class for all natural motion related tasks.

// --- Include Files ------------------------------------------------------------
 
// C headers  
// Rage headers
#include "crskeleton/Skeleton.h"
#include "fragment/Cache.h"
#include "fragment/Instance.h"
#include "fragment/Type.h"
#include "fragment/TypeChild.h"
#include "fragmentnm/messageparams.h"
#include "fragmentnm/manager.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/debugcontacts.h"
#include "physics/shapetest.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"

// Framework headers
#include "fwanimation/animmanager.h" 
#include "fwanimation/directorcomponentragdoll.h"
#include "fwanimation/pointcloud.h"
#include "fwdebug/picker.h"
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"

// Game headers
#include "ai/stats.h"
#include "animation/AnimDefines.h"
#include "animation/debug/AnimViewer.h"
#include "camera/CamInterface.h"
#include "debug/DebugScene.h"
#include "game/ModelIndices.h"
#include "Event/EventDamage.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfoExtensions.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "network/objects/Entities/NetObjPlayer.h"
#include "network/objects/prediction/NetBlenderPed.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPlacement.h"
#include "Peds/Ped.h"
#include "Peds/PlayerSpecialAbility.h"
#include "PedGroup/PedGroup.h"
#include "Physics/debugcontacts.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Physics/RagdollConstraints.h"
#include "scene/lod/LodScale.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"			// For CStreaming::HasObjectLoaded().
#include "System/ControlMgr.h"
#include "Task/Combat/TaskAnimatedHitByExplosion.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/Physics/NmDebug.h"
#include "Task/Physics/NmTaskMessageDefines.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/TaskNMBuoyancy.h"
#include "Task/Physics/TaskNMDangle.h"
#include "Task/Physics/TaskNMExplosion.h"
#include "Task/Physics/TaskNMFlinch.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMInjuredOnGround.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "Task/Physics/TaskNMOnFire.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMRiverRapids.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Physics/TaskRageRagdoll.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Movement/TaskAnimatedFallback.h"
#include "task/Movement/TaskFall.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Movement/Jumping/TaskInAir.h"
#include "task/Movement/TaskParachute.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/vehicle.h"
#include "vehicles/Bike.h"
#include "Vfx/Misc/Fire.h"
#include "renderer/Debug/EntitySelect.h"
#include "Weapons/WeaponDebug.h"
#include "peds/peddebugvisualiser.h"
#include "system/param.h"

#if ART_ENABLE_BSPY
#ifndef ROCKSTAR_GAME_EMBEDDED
#define ROCKSTAR_GAME_EMBEDDED
#endif
#ifndef ART_MONOLITHIC
#define ART_MONOLITHIC
#endif
#include <rockstar/NmRsSpy.h>
#include <rockstar/NmRsEngine.h>
#endif

using namespace AIStats;

// used to help debug clone NM tasks:
#if !__NO_OUTPUT
#define NET_DEBUG_TTY(str,...) nmDebugf3(str,##__VA_ARGS__) 
#else
#define NET_DEBUG_TTY(str,...) 
#endif

RAGE_DEFINE_CHANNEL(nm)

PARAM(nmtuning, "Enable tuning of naturalmotion parameters");

PARAM(disableRagdollPoolsSp, "Disable ragdoll pooling in single player by default");
PARAM(enableRagdollPoolsMp, "Enable ragdoll pooling in multi player by default");
PARAM(forceRagdollLOD, "Force all ragdolls to run the given LOD");

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMControl::Tunables CTaskNMControl::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMControl, 0x005675f3);

CTaskNMBehaviour::Tunables CTaskNMBehaviour::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMBehaviour, 0xbddf8154);
//////////////////////////////////////////////////////////////////////////

CTaskNMBehaviour::CRagdollPool CTaskNMBehaviour::sm_RagdollPools[kNumRagdollPools];

bool CTaskNMBehaviour::sm_OnlyUseRageRagdolls = false;
bool CTaskNMBehaviour::sm_DoOverrideBulletImpulses = false;
float CTaskNMBehaviour::sm_OverrideImpulse = 50.0f;
float CTaskNMBehaviour::sm_ArmsImpulseCap = 50.0f;
float CTaskNMBehaviour::sm_MaxShotUprightForce = 3.0f;
float CTaskNMBehaviour::sm_DontFallUntilDeadStrength = 0.0f;
float CTaskNMBehaviour::sm_MaxShotUprightTorque = 0.0f;
float CTaskNMBehaviour::sm_ThighImpulseMin = 150.0f;
float CTaskNMBehaviour::sm_CharacterHealth = 1.0f;
float CTaskNMBehaviour::sm_CharacterStrength = 1.0f;
float CTaskNMBehaviour::sm_StayUprightMagnitude = 0.0f;
float CTaskNMBehaviour::sm_RigidBodyImpulseRatio = 0.0f;
float CTaskNMBehaviour::sm_ShotRelaxAmount = 0.0f;
float CTaskNMBehaviour::sm_BulletPopupImpulse = 0.0f; // was 85.0f 
float CTaskNMBehaviour::sm_SuccessiveImpulseIncreaseScale = 0.0f; // // was 0.1f
float CTaskNMBehaviour::sm_LastStandMaxTimeBetweenHits = 0.25f;
bool CTaskNMBehaviour::sm_DoLastStand = true;

bool CTaskNMBehaviour::ms_bDisableBumpGrabForPlayer = true;

atHashString CNmParameter::ms_ResetAllParameters("reset",0xD424B9EA);

namespace ART
{
	extern NmRsEngine* gRockstarARTInstance;
}

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
NETWORK_OPTIMISATIONS() 

ART::MessageParams& CNmMessageList::GetMessage(atFinalHashString messageName, s32 numFreeParams)
{
	ART::MessageParams* pMessage = NULL;

	// do a search for an existing message
	for (s32 i=m_messages.GetCount()-1; i>=0; i--)
	{
		if (m_messages[i].m_name.GetHash()==messageName.GetHash() && m_messages[i].m_message->getUsedParamCount()<(m_messages[i].m_message->getMaxParamCount() - numFreeParams))
		{
			Assert(m_messages[i].m_message);
			return *m_messages[i].m_message;
		}
	}

	// if we got here we need to make a new one
	m_messages.Grow();
	m_messages[m_messages.GetCount()-1].m_name=messageName;
	pMessage = rage_new ART::MessageParams();
	m_messages[m_messages.GetCount()-1].m_message=pMessage;

	return *pMessage;
}

bool CNmMessageList::HasMessage(atFinalHashString messageName)
{
	// do a search for an existing message
	for (s32 i=m_messages.GetCount()-1; i>=0; i--)
	{
		if (m_messages[i].m_name.GetHash()==messageName.GetHash())
		{
			return true;
		}
	}

	return false;
}
void CNmMessageList::Post(fragInstNMGta* pInst)
{
	for (s32 i=0; i<m_messages.GetCount(); i++)
	{
		pInst->PostARTMessage(m_messages[i].m_name.GetCStr(), m_messages[i].m_message);
	}
}

void CNmMessageList::Clear()
{
	for (s32 i=0; i<m_messages.GetCount(); i++)
	{
		delete m_messages[i].m_message;
	}

	m_messages.ResetCount();
}

void CNmMessage::AddTo(CNmMessageList& list, CTaskNMBehaviour* pTask) const
{
	if (m_TaskMessage)
	{
		if (pTask)
			pTask->ApplyTaskTuningMessage(*this);
	}
	else
	{
		ART::MessageParams& params = list.GetMessage(m_Name, m_ForceNewMessage ? ART_MAXIMUM_PARAMETERS_IN_MESSAGEPARAMS : m_Params.GetCount());
		for (int i=0; i< m_Params.GetCount(); i++)
		{
			m_Params[i]->AddTo(params);
		}
	}
}

void CNmMessage::Post(CPed& ped, CTaskNMBehaviour* pTask) const
{
	if (m_TaskMessage)
	{
		if (pTask)
			pTask->ApplyTaskTuningMessage(*this);
	}
	else
	{
		ART::MessageParams params;
		for (int i=0; i< m_Params.GetCount(); i++)
		{
			m_Params[i]->AddTo(params);
		}
		ped.GetRagdollInst()->PostARTMessage(m_Name.GetCStr(), &params);
	}
}


CNmTuningSet::CNmTuningSet()
#if __BANK
	: m_pWidgets(NULL)
#endif //__BANK
{

}
CNmTuningSet::~CNmTuningSet()
{
#if __BANK
	m_pWidgets=NULL;
#endif //__BANK
}

#if __BANK

char CNmTuningSetGroup::ms_SetName[128] = "";


void CNmTuningSetGroup::AddParamWidgets(bkBank& bank)
{
	m_pWidgets = static_cast<bkGroup*>(bank.GetCurrentGroup());

	bank.PushGroup("Add / remove sets");
	bank.AddText("Set name", &ms_SetName[0], 128);
	bank.AddButton("Add set", datCallback(MFA(CNmTuningSetGroup::AddSet), (datBase*)this));
	bank.AddButton("Remove set", datCallback(MFA(CNmTuningSetGroup::RemoveSet), (datBase*)this));

	bank.PopGroup();
	for (atBinaryMap<CNmTuningSet, atHashString>::Iterator it = m_sets.Begin(); it != m_sets.End(); it++)
	{
		bank.PushGroup((*it).m_Id.TryGetCStr());
		(*it).AddWidgets(bank);
		bank.PopGroup();
	}
}

void CNmTuningSetGroup::AddSet()
{
	if(!PARAM_nmtuning.Get())
		return;

	atHashString setName(ms_SetName);
	if (m_sets.Access(setName) == NULL)
	{
		CNmTuningSet* entry = m_sets.Insert(setName);
		entry->m_Id.SetFromString(ms_SetName);
		entry->m_Priority=0;
		bkGroup* pGroup = m_pWidgets->AddGroup(ms_SetName);
		entry->AddWidgetsInternal(*pGroup);
		m_sets.FinishInsertion();
	}
}
void CNmTuningSetGroup::RemoveSet()
{
	atHashString setName(ms_SetName);
	m_sets.Remove(m_sets.GetIndexFromDataPtr(m_sets.Access(setName)));

	// find and delete the widget for the param
	bkWidget* child = m_pWidgets->GetChild();
	while(child)
	{
		bkWidget*lastChild = child;
		child = child->GetNext();
		if (atStringHash(lastChild->GetTitle())==setName.GetHash())
		{
			lastChild->Destroy();
		}
	}
}

RegdPed CTaskNMBehaviour::m_pTuningSetHistoryPed;
atArray<CTaskNMBehaviour::TuningSetEntry> CTaskNMBehaviour::m_TuningSetHistory;

//////////////////////////////////////////////////////////////////////////
// tuning set editor widgets
//////////////////////////////////////////////////////////////////////////

s32 CNmTuningSet::sm_SelectedMessage;
atArray<const char *> CNmTuningSet::sm_MessageNames;
atMap<u32, atArray<const char *> * > CNmTuningSet::sm_ParameterNamesMap;

void CNmParameterResetMessage::AddWidgets(bkGroup& bank, const NMBehavior& UNUSED_PARAM(behavior))
{
	bank.AddTitle("Reset all parameters");
}


// Float params

CNmParameterFloat::CNmParameterFloat(const NMParam* pParam)
{
	m_Name.SetFromString(pParam->GetName());
	m_Value = (float)::atof(pParam->GetInitValue());
}

void CNmParameterFloat::AddWidgets(bkGroup& bank, const NMBehavior& behavior)
{
	const NMParam* param = behavior.GetParam(m_Name.GetCStr());
	if (param)
	{
		// load the min and max from the nm data
		const char * pTooltip = param->GetDescription();
		if (pTooltip != NULL && strlen(pTooltip)>127)
		{
			pTooltip = NULL;
		}
		bank.AddSlider(m_Name.GetCStr(), &m_Value, param->GetMinValue(), param->GetMaxValue(),param->GetStepValue(), NullCB, pTooltip);
	}
	else
	{
		taskAssertf(0, "Unknown nm parameter '%s' in behavior '%s'", m_Name.GetCStr(), behavior.GetName() );
	}
}

CNmParameterRandomFloat::CNmParameterRandomFloat(const NMParam* pParam)
{
	m_Name.SetFromString(pParam->GetName());
	m_Min = pParam->GetMinValue();
	m_Max = pParam->GetMaxValue();
}

void CNmParameterRandomFloat::AddWidgets(bkGroup& bank, const NMBehavior& behavior)
{
	const NMParam* param = behavior.GetParam(m_Name.GetCStr());
	if (param)
	{
		// load the min and max from the nm data
		const char * pTooltip = param->GetDescription();
		if (pTooltip != NULL && strlen(pTooltip)>127)
		{
			pTooltip = NULL;
		}
		bkGroup* group = bank.AddGroup(m_Name.GetCStr());
		group->AddSlider("Min", &m_Min, param->GetMinValue(), param->GetMaxValue(),param->GetStepValue(), NullCB, pTooltip);
		group->AddSlider("Max", &m_Max, param->GetMinValue(), param->GetMaxValue(),param->GetStepValue(), NullCB, pTooltip);
	}
	else
	{
		taskAssertf(0, "Unknown nm parameter '%s' in behavior '%s'", m_Name.GetCStr(), behavior.GetName() );
	}
}


// int params

CNmParameterInt::CNmParameterInt(const NMParam* pParam)
{
	m_Name.SetFromString(pParam->GetName());
	m_Value = (int)atoi(pParam->GetInitValue());
}

void CNmParameterInt::AddWidgets(bkGroup& bank, const NMBehavior& behavior)
{
	const NMParam* param = behavior.GetParam(m_Name.GetCStr());
	if (param)
	{
		// load the min and max from the nm data
		const char * pTooltip = param->GetDescription();
		if (pTooltip != NULL && strlen(pTooltip)>127)
		{
			pTooltip = NULL;
		}
		bank.AddSlider(m_Name.GetCStr(), &m_Value, (s32)param->GetMinValue(), (s32)param->GetMaxValue(), (s32)param->GetStepValue(), NullCB, pTooltip);
	}
	else
	{
		taskAssertf(0, "Unknown nm parameter '%s' in behavior '%s'", m_Name.GetCStr(), behavior.GetName() );
	}
}

CNmParameterRandomInt::CNmParameterRandomInt(const NMParam* pParam)
{
	m_Name.SetFromString(pParam->GetName());
	m_Min = (s32)pParam->GetMinValue();
	m_Max = (s32)pParam->GetMaxValue();
}

void CNmParameterRandomInt::AddWidgets(bkGroup& bank, const NMBehavior& behavior)
{
	const NMParam* param = behavior.GetParam(m_Name.GetCStr());
	if (param)
	{
		// load the min and max from the nm data
		const char * pTooltip = param->GetDescription();
		if (pTooltip != NULL && strlen(pTooltip)>127)
		{
			pTooltip = NULL;
		}
		bkGroup* group = bank.AddGroup(m_Name.GetCStr());
		group->AddSlider("Min", &m_Min, (s32)param->GetMinValue(), (s32)param->GetMaxValue(), (s32)param->GetStepValue(), NullCB, pTooltip);
		group->AddSlider("Max", &m_Max, (s32)param->GetMinValue(), (s32)param->GetMaxValue(), (s32)param->GetStepValue(), NullCB, pTooltip);
	}
	else
	{
		taskAssertf(0, "Unknown nm parameter '%s' in behavior '%s'", m_Name.GetCStr(), behavior.GetName() );
	}
}

// string params

CNmParameterString::CNmParameterString(const NMParam* pParam)
{
	m_Name.SetFromString(pParam->GetName());
	m_Value.SetFromString(pParam->GetInitValue());
}

void CNmParameterString::AddWidgets(bkGroup& bank, const NMBehavior& behavior)
{
	const NMParam* param = behavior.GetParam(m_Name.GetCStr());
	if (param)
	{
		bank.AddText(m_Name.GetCStr(), &m_Value, NullCB);
	}
	else
	{
		taskAssertf(0, "Unknown nm parameter '%s' in behavior '%s'", m_Name.GetCStr(), behavior.GetName() );
	}
}

// vector params

CNmParameterVector::CNmParameterVector(const NMParam* pParam)
{
	m_Name.SetFromString(pParam->GetName());

	atString init(pParam->GetInitValue());
	atArray<atString> splitStrings;
	init.Split(splitStrings, ',');
	for(s32 i=0; i<splitStrings.GetCount(); i++)
	{
		switch(i)
		{
		case 0:
			m_Value.x = (float)atof(splitStrings[i]);
			break;
		case 1:
			m_Value.y = (float)atof(splitStrings[i]);
			break;
		case 2:
			m_Value.z = (float)atof(splitStrings[i]);
			break;
		}
	}

}

void CNmParameterVector::AddWidgets(bkGroup& bank, const NMBehavior& behavior)
{
	const NMParam* param = behavior.GetParam(m_Name.GetCStr());
	if (param)
	{
		// load the min and max from the nm data
		const char * pTooltip = param->GetDescription();
		if (pTooltip != NULL && strlen(pTooltip)>127)
		{
			pTooltip = NULL;
		}
		bank.AddVector(m_Name.GetCStr(), &m_Value, param->GetMinValue(), param->GetMaxValue(),param->GetStepValue(), NullCB, pTooltip);
	}
	else
	{
		taskAssertf(0, "Unknown nm parameter '%s' in behavior '%s'", m_Name.GetCStr(), behavior.GetName() );
	}
}

// bool params

CNmParameterBool::CNmParameterBool(const NMParam* pParam)
{
	m_Name.SetFromString(pParam->GetName());

	atHashString init(pParam->GetInitValue());
	if (init.GetHash()==ATSTRINGHASH("true", 0xB2F35C25))
	{
		m_Value = true;
	}
	else
	{
		m_Value = false;
	}
}

void CNmParameterBool::AddWidgets(bkGroup& bank, const NMBehavior& behavior)
{
	if(m_Name.GetHash()==ATSTRINGHASH("start", 0x84DC271F))
	{
		bank.AddToggle(m_Name.GetCStr(), &m_Value, NullCB, "Trigger the behaviour to start");
		return;
	}
	const NMParam* param = behavior.GetParam(m_Name.GetCStr());
	if (param)
	{
		// load the min and max from the nm data
		const char * pTooltip = param->GetDescription();
		if (pTooltip != NULL && strlen(pTooltip)>127)
		{
			pTooltip = NULL;
		}
		bank.AddToggle(m_Name.GetCStr(), &m_Value, NullCB, pTooltip);
	}
	else
	{
		taskAssertf(0, "Unknown nm parameter '%s' in behavior '%s'", m_Name.GetCStr(), behavior.GetName() );
	}
}

void CNmMessage::AddWidgets(bkGroup& bank)
{
	if(!PARAM_nmtuning.Get())
		return;

	const NMBehavior* pBehavior = NMBEHAVIORPOOL::InstanceRef().GetBehavior(m_Name.GetCStr());

	if (pBehavior)
	{
		m_pWidgets = bank.AddGroup(m_Name.GetCStr(), false, pBehavior->GetDescription());
		// parameter adding widgets
		bkGroup* editGroup = m_pWidgets->AddGroup("Add/remove params");
		atArray<const char *>* pList = *CNmTuningSet::sm_ParameterNamesMap.Access(m_Name.GetHash());
		if (pList && pList->GetCount()>0)
		{
			editGroup->AddCombo("parameter:", &m_SelectedParameter, pList->GetCount(), &(*pList)[0]);
		}
		editGroup->AddButton("Add param", datCallback(MFA(CNmMessage::AddSingleParameter), (datBase*)this));
		editGroup->AddButton("Add with random range", datCallback(MFA(CNmMessage::AddRandomRange), (datBase*)this));
		editGroup->AddButton("Delete param", datCallback(MFA(CNmMessage::RemoveSingleParameter), (datBase*)this));
		editGroup->AddButton("Add all params", datCallback(MFA(CNmMessage::AddAllParameters), (datBase*)this));
		editGroup->AddButton("Delete default value params", datCallback(MFA(CNmMessage::RemoveDefaultValueParameters), (datBase*)this));
		editGroup->AddButton("Delete all params", datCallback(MFA(CNmMessage::RemoveAllParameters), (datBase*)this));
		editGroup->AddButton("Move up", datCallback(MFA(CNmMessage::PromoteSelectedParam), (datBase*)this));
		editGroup->AddButton("Move down", datCallback(MFA(CNmMessage::DemoteSelectedParam), (datBase*)this));

		for (s32 i=0; i<m_Params.GetCount(); i++)
		{
			if (m_Params[i])
			{
				m_Params[i]->AddWidgets(*m_pWidgets, *pBehavior);
			}
		}
	}
	else
	{
		taskAssertf(0, "Unknown nm behavior '%s' in nm tuning set", m_Name.GetCStr() );
	}
}

void CNmMessage::AddSingleParameter()
{
	atArray<const char *>* pList = *CNmTuningSet::sm_ParameterNamesMap.Access(m_Name.GetHash());
	AddParameter((*pList)[m_SelectedParameter]);
}

void CNmMessage::AddRandomRange()
{
	atArray<const char *>* pList = *CNmTuningSet::sm_ParameterNamesMap.Access(m_Name.GetHash());
	AddParameter((*pList)[m_SelectedParameter], true);
}

void CNmMessage::AddAllParameters()
{
	if(!PARAM_nmtuning.Get())
		return;

	const NMBehavior* pBehavior = NMBEHAVIORPOOL::InstanceRef().GetBehavior(m_Name.GetCStr());

	if (pBehavior)
	{
		for (int i=0; i<pBehavior->GetNumParams(); i++)
		{
			AddParameter(pBehavior->GetParam(i)->GetName());
		}
	}
}

void CNmMessage::AddParameter(const char * name, bool withRandomRange)
{
	if(!PARAM_nmtuning.Get())
		return;

	const NMBehavior* pBehavior = NMBEHAVIORPOOL::InstanceRef().GetBehavior(m_Name.GetCStr());

	atHashString newParam(name);

	// special handling for the start and reset messages
	if (newParam.GetHash()==ATSTRINGHASH("start", 0x84DC271F))
	{
		CNmParameterBool* pNewParam = rage_new CNmParameterBool();
		pNewParam->m_Name.SetFromString("start");
		pNewParam->m_Value = true;
		m_Params.PushAndGrow(pNewParam);
		pNewParam->AddWidgets(*m_pWidgets, *pBehavior);
		return;
	}
	else if (newParam.GetHash()==CNmParameter::ms_ResetAllParameters.GetHash())
	{
		CNmParameterResetMessage* pNewParam = rage_new CNmParameterResetMessage();
		pNewParam->m_Name.SetFromString(CNmParameter::ms_ResetAllParameters.GetCStr());
		m_Params.PushAndGrow(pNewParam);
		pNewParam->AddWidgets(*m_pWidgets, *pBehavior);
		return;
	}

	const NMParam* pParam = pBehavior->GetParam(name);

	if (pParam)
	{
		bool hasParam = false;
		

		// check we don't already have this param
		for (s32 i=0; i<m_Params.GetCount(); i++)
		{
			if (m_Params[i]->m_Name.GetHash()==newParam.GetHash())
			{
				hasParam = true;
				break;
			}
		}

		if (!hasParam)
		{
			CNmParameter* pNewParam = NULL;

			if (pParam->IsFloat())
			{
				if (withRandomRange)
				{
					pNewParam = rage_new CNmParameterRandomFloat(pParam);
				}
				else
				{
					pNewParam = rage_new CNmParameterFloat(pParam);
				}
			}
			else if (pParam->IsInt())
			{
				if (withRandomRange)
				{
					pNewParam = rage_new CNmParameterRandomInt(pParam);
				}
				else
				{
					pNewParam = rage_new CNmParameterInt(pParam);
				}
			}
			else if (pParam->IsVector3())
			{
				pNewParam = rage_new CNmParameterVector(pParam);
			}
			else if (pParam->IsBool())
			{
				pNewParam = rage_new CNmParameterBool(pParam);
			}
			else if (pParam->IsString())
			{
				pNewParam = rage_new CNmParameterString(pParam);
			}
			else
			{
				Assertf(0, "Unknown parameter type for parameter '%s' in message '%s'",pParam->GetName(), m_Name.GetCStr());
			}

			if (pNewParam)
			{
				m_Params.PushAndGrow(pNewParam);

				pNewParam->AddWidgets(*m_pWidgets, *pBehavior);
			}
		}
	}
}

void CNmMessage::RemoveSingleParameter()
{
	atArray<const char *>* pList = *CNmTuningSet::sm_ParameterNamesMap.Access(m_Name.GetHash());
	RemoveParameter((*pList)[m_SelectedParameter]);
}

void CNmMessage::RemoveDefaultValueParameters()
{
	if(!PARAM_nmtuning.Get())
		return;

	const NMBehavior* pBehavior = NMBEHAVIORPOOL::InstanceRef().GetBehavior(m_Name.GetCStr());

	if (pBehavior)
	{
		atArray<const char*> paramsToDelete;

		for (s32 i=0; i<m_Params.GetCount(); i++)
		{
			// init a default parameter and compare the value. 
			// if they're the same, delete them
			const NMParam* pParam = pBehavior->GetParam(m_Params[i]->m_Name.GetCStr());

			if (pParam)
			{
				if (pParam->IsFloat())
				{
					CNmParameterFloat newParam(pParam);
					if (static_cast<CNmParameterFloat*>(m_Params[i])->m_Value==newParam.m_Value)
					{
						paramsToDelete.PushAndGrow(pParam->GetName());
					}
				}
				else if (pParam->IsInt())
				{
					CNmParameterInt newParam(pParam);
					if (static_cast<CNmParameterInt*>(m_Params[i])->m_Value==newParam.m_Value)
					{
						paramsToDelete.PushAndGrow(pParam->GetName());
					}
				}
				else if (pParam->IsVector3())
				{
					CNmParameterVector newParam(pParam);
					if (static_cast<CNmParameterVector*>(m_Params[i])->m_Value==newParam.m_Value)
					{
						paramsToDelete.PushAndGrow(pParam->GetName());
					}
				}
				else if (pParam->IsBool())
				{
					CNmParameterBool newParam(pParam);
					if (static_cast<CNmParameterBool*>(m_Params[i])->m_Value==newParam.m_Value)
					{
						paramsToDelete.PushAndGrow(pParam->GetName());
					}
				}
				else if (pParam->IsString())
				{
					CNmParameterString newParam(pParam);
					if (static_cast<CNmParameterString*>(m_Params[i])->m_Value.GetHash()==newParam.m_Value.GetHash())
					{
						paramsToDelete.PushAndGrow(pParam->GetName());
					}
				}
			}			
		}

		for (int i=0; i<paramsToDelete.GetCount(); i++)
		{
			RemoveParameter(paramsToDelete[i]);
		}
	}
}

void CNmMessage::RemoveAllParameters()
{
	if(!PARAM_nmtuning.Get())
		return;

	const NMBehavior* pBehavior = NMBEHAVIORPOOL::InstanceRef().GetBehavior(m_Name.GetCStr());

	if (pBehavior)
	{
		for (int i=0; i<pBehavior->GetNumParams(); i++)
		{
			RemoveParameter(pBehavior->GetParam(i)->GetName());
		}
	}
}

void CNmMessage::RemoveParameter(const char * name)
{
	CNmParameter* pParamToDelete = NULL;
	atHashString paramName(name);

	for (s32 i=0; i<m_Params.GetCount(); i++)
	{
		if (paramName.GetHash()==m_Params[i]->m_Name.GetHash())
		{
			pParamToDelete = m_Params[i];
			delete m_Params[i];
			break;
		}
	}

	if (pParamToDelete)
	{
		m_Params.DeleteMatches(pParamToDelete);
		// find and delete the widget for the param
		bkWidget* child = m_pWidgets->GetChild();
		while(child)
		{
			bkWidget*lastChild = child;
			child = child->GetNext();
			if (atStringHash(lastChild->GetTitle())==paramName.GetHash())
			{
				lastChild->Destroy();
			}
		}
	}
}

void CNmMessage::PromoteSelectedParam()
{
	atArray<const char *>* pList = *CNmTuningSet::sm_ParameterNamesMap.Access(m_Name.GetHash());
	atHashString paramName((*pList)[m_SelectedParameter]);

	for (s32 i=0; i<m_Params.GetCount(); i++)
	{
		if (paramName.GetHash()==m_Params[i]->m_Name.GetHash())
		{
			if (i>0)
			{
				CNmParameter* pPreviousParam = m_Params[i-1];
				m_Params[i-1] = m_Params[i];
				m_Params[i] = pPreviousParam;
			}
			break;
		}
	}

	RegenerateParamWidgets();
}

void CNmMessage::DemoteSelectedParam()
{
	atArray<const char *>* pList = *CNmTuningSet::sm_ParameterNamesMap.Access(m_Name.GetHash());
	atHashString paramName((*pList)[m_SelectedParameter]);

	for (s32 i=0; i<m_Params.GetCount(); i++)
	{
		if (paramName.GetHash()==m_Params[i]->m_Name.GetHash())
		{
			if (m_Params.GetCount()>(i+1))
			{
				CNmParameter* pNextParam = m_Params[i+1];
				m_Params[i+1] = m_Params[i];
				m_Params[i] = pNextParam;
			}
			break;
		}
	}

	RegenerateParamWidgets();
}

void CNmMessage::RegenerateParamWidgets()
{
	bkWidget* pWidget = m_pWidgets->GetChild();

	// skip the edit buttons ('Add/remove params')
	pWidget = pWidget->GetNext();

	while (pWidget)
	{
		bkWidget* delWidget = pWidget;
		pWidget = pWidget->GetNext();
		delWidget->Destroy();
	}

	const NMBehavior* pBehavior = NMBEHAVIORPOOL::InstanceRef().GetBehavior(m_Name.GetCStr());

	if (pBehavior)
	{
		for (s32 i=0; i<m_Params.GetCount(); i++)
		{
			m_Params[i]->AddWidgets(*m_pWidgets, *pBehavior);
		}
	}
}

NMBehaviorPool CNmTuningSet::sm_TaskMessageDefs;

void CNmTuningSet::AddWidgetsInternal(bkGroup& bank)
{
	if(!PARAM_nmtuning.Get())
		return;

	if (!NMBEHAVIORPOOL::IsInstantiated())
	{
		NMBEHAVIORPOOL::Instantiate();
		const char * pPath = "common:/data/naturalmotion";
#if __DEV
		extern ::rage::sysParam PARAM_nmfolder;
		PARAM_nmfolder.Get(pPath);
#endif //__DEV
		atVarString BehaviourFilePath("%s/%s", pPath, "Behaviours.xml" );
		NMBEHAVIORPOOL::InstanceRef().LoadFile(BehaviourFilePath.c_str());
		
		atVarString TaskFilePath("%s/%s", pPath, "TaskMessages.xml" );
		sm_TaskMessageDefs.LoadFile(TaskFilePath.c_str());

		// copy the task level definitions into the main list
		for (s32 i=0; i<sm_TaskMessageDefs.GetNumBehaviors();i++)
		{
			NMBEHAVIORPOOL::InstanceRef().AddBehavior(const_cast<NMBehavior*>(sm_TaskMessageDefs.GetBehavior(i)));
		}

		for (s32 i=0; i<NMBEHAVIORPOOL::InstanceRef().GetNumBehaviors(); i++)
		{
			const NMBehavior* pBehavior = NMBEHAVIORPOOL::InstanceRef().GetBehavior(i);
			sm_MessageNames.PushAndGrow(pBehavior->GetName());
			
			// create a list of the parameter names and add them to the map
			atArray<const char *>* pParamNameList = rage_new atArray<const char *>();

			// Only allow the start and reset params for messages we're going to send to nm
			if (!pBehavior->IsTaskMessage())
			{
				pParamNameList->PushAndGrow("start");
				pParamNameList->PushAndGrow(CNmParameter::ms_ResetAllParameters.GetCStr());
			}

			for (s32 j=0; j<pBehavior->GetNumParams(); j++)
			{
				pParamNameList->PushAndGrow(pBehavior->GetParam(j)->GetName());
			}

			// add the param name list to the map
			sm_ParameterNamesMap.Insert(atStringHash(pBehavior->GetName()), pParamNameList);
		}
	}

	bkWidget* slider = bank.AddSlider("Priority", &m_Priority, 0, 255, 1);
	m_pWidgets = static_cast<bkGroup*>(slider->GetParent());
	bank.AddToggle("Enabled", &m_Enabled );

	// message adding widgets
	bkGroup* editGroup = m_pWidgets->AddGroup("Add/remove messages");
	editGroup->AddCombo("message:",&sm_SelectedMessage, sm_MessageNames.GetCount(), &sm_MessageNames[0]);
	editGroup->AddButton("Add message", datCallback(MFA(CNmTuningSet::AddSingleMessage), (datBase*)this));
	editGroup->AddButton("Delete message", datCallback(MFA(CNmTuningSet::RemoveSingleMessage), (datBase*)this));
	editGroup->AddButton("Delete all messages", datCallback(MFA(CNmTuningSet::RemoveAllMessages), (datBase*)this));
	editGroup->AddButton("Move up", datCallback(MFA(CNmTuningSet::PromoteSelectedMessage), (datBase*)this));
	editGroup->AddButton("Move down", datCallback(MFA(CNmTuningSet::DemoteSelectedMessage), (datBase*)this));
	
	for (s32 i=0; i<m_Messages.GetCount(); i++)
	{
		m_Messages[i]->AddWidgets(*m_pWidgets);
	}
}

void CNmTuningSet::AddSingleMessage()
{
	AddMessage(sm_MessageNames[sm_SelectedMessage]);
}

void CNmTuningSet::AddMessage(const char * name)
{
	if(!PARAM_nmtuning.Get())
		return;

	const NMBehavior* pBehavior = NMBEHAVIORPOOL::InstanceRef().GetBehavior(name);

	if (pBehavior)
	{
		bool allowDuplicates = pBehavior->AllowDuplicates();
		bool hasMessage = false;
		atHashString newMessage(name);

		if (!allowDuplicates)
		{
			// check we don't already have this param
			for (s32 i=0; i<m_Messages.GetCount(); i++)
			{
				if (m_Messages[i]->m_Name.GetHash()==newMessage.GetHash())
				{
					hasMessage = true;
					break;
				}
			}
		}

		if (!hasMessage || allowDuplicates)
		{
			CNmMessage* pNewMessage = rage_new CNmMessage();
			pNewMessage->m_Name.SetFromString(name);
			pNewMessage->m_ForceNewMessage = allowDuplicates;
			pNewMessage->m_TaskMessage = pBehavior->IsTaskMessage();
			m_Messages.PushAndGrow(pNewMessage);
			pNewMessage->AddWidgets(*m_pWidgets);
		}
	}
}

void CNmTuningSet::RemoveSingleMessage()
{
	RemoveMessage(sm_MessageNames[sm_SelectedMessage]);
}

void CNmTuningSet::RemoveAllMessages()
{
	while (m_Messages.GetCount()>0)
	{
		RemoveMessage(m_Messages[m_Messages.GetCount()-1]->m_Name.GetCStr());
	}
}

void CNmTuningSet::RemoveMessage(const char * name)
{
	CNmMessage* pMessageToDelete = NULL;
	atHashString messageName(name);

	for (s32 i=0; i<m_Messages.GetCount(); i++)
	{
		if (messageName.GetHash()==m_Messages[i]->m_Name.GetHash())
		{
			pMessageToDelete = m_Messages[i];
			delete m_Messages[i];
			break;
		}
	}

	if (pMessageToDelete)
	{
		m_Messages.DeleteMatches(pMessageToDelete);
		// find and delete the widget for the param
		bkWidget* child = m_pWidgets->GetChild();
		while(child)
		{
			bkWidget*lastChild = child;
			child = child->GetNext();
			if (atStringHash(lastChild->GetTitle())==messageName.GetHash())
			{
				lastChild->Destroy();
				break;
			}
		}
	}
}

void CNmTuningSet::PromoteSelectedMessage()
{
	atHashString messageName(sm_MessageNames[sm_SelectedMessage]);

	for (s32 i=0; i<m_Messages.GetCount(); i++)
	{
		if (messageName.GetHash()==m_Messages[i]->m_Name.GetHash())
		{
			if (i>0)
			{
				CNmMessage* pPreviousMessage = m_Messages[i-1];
				m_Messages[i-1] = m_Messages[i];
				m_Messages[i] = pPreviousMessage;
			}
			break;
		}
	}

	RegenerateMessageWidgets();
}

void CNmTuningSet::DemoteSelectedMessage()
{
	atHashString messageName(sm_MessageNames[sm_SelectedMessage]);

	for (s32 i=0; i<m_Messages.GetCount(); i++)
	{
		if (messageName.GetHash()==m_Messages[i]->m_Name.GetHash())
		{
			if (m_Messages.GetCount()>(i+1))
			{
				CNmMessage* pNextMessage = m_Messages[i+1];
				m_Messages[i+1] = m_Messages[i];
				m_Messages[i] = pNextMessage;
			}
			break;
		}
	}

	RegenerateMessageWidgets();
}

void CNmTuningSet::RegenerateMessageWidgets()
{
	bkWidget* pWidget = m_pWidgets->GetChild();

	// skip the edit buttons ('Priority', 'Enabled' and 'Add/remove messages')
	pWidget = pWidget->GetNext();
	pWidget = pWidget->GetNext();
	pWidget = pWidget->GetNext();

	while (pWidget)
	{
		bkWidget* delWidget = pWidget;
		pWidget = pWidget->GetNext();
		delWidget->Destroy();
	}

	for (s32 i=0; i<m_Messages.GetCount(); i++)
	{
		m_Messages[i]->AddWidgets(*m_pWidgets);
	}
}

#endif //__BANK

void CNmTuningSet::AddTo(CNmMessageList& list, CTaskNMBehaviour* pTask) const
{
#if ART_ENABLE_BSPY
	if (pTask != NULL)
	{
		pTask->SetbSpyScratchpadString(atVarString("Tuning set %s", m_Id.TryGetCStr()));
	}
#endif

	for (int i=0; i< m_Messages.GetCount(); i++)
	{
		m_Messages[i]->AddTo(list, pTask);
	}
}

void CNmTuningSet::Post(CPed& ped, CTaskNMBehaviour* pTask) const
{
#if ART_ENABLE_BSPY
	if (pTask != NULL)
	{
		pTask->SetbSpyScratchpadString(atVarString("Tuning set %s", m_Id.TryGetCStr()));
	}
#endif

	for (int i=0; i< m_Messages.GetCount(); i++)
	{
		m_Messages[i]->Post(ped, pTask);
	}
}

void CNmTuningSet::Post(CPed& ped, CTaskNMBehaviour* pTask, atHashString messageHash) const
{
	for (int i=0; i< m_Messages.GetCount(); i++)
	{
		if (m_Messages[i]->m_Name.GetHash()==messageHash.GetHash())
		{
#if ART_ENABLE_BSPY
			if (pTask != NULL)
			{
				pTask->SetbSpyScratchpadString(atVarString("Tuning set %s (%s message only)", m_Id.TryGetCStr(), m_Messages[i]->m_Name.TryGetCStr()));
			}
#endif

			m_Messages[i]->Post(ped, pTask);
			break;
		}
	}
}

// Forward declarations:
class CTaskNMBalance; 

char CTaskNMBehaviour::sm_pParameterFile[128];

#if ART_ENABLE_BSPY
void CTaskNMBehaviour::SendTaskStateToBSpy()
{
	using namespace ART;
	int ibSpyID = bspyLastSeenAgent;
	CPed* pPed = GetPed();
	if(pPed != NULL)
	{
		ibSpyID = ART::gRockstarARTInstance->getBSpyIDFromCharacterID(pPed->GetRagdollInst()->GetNMAgentID());

		// ACTIVE state
		CTask* pActiveTask = pPed->GetPedIntelligence()->GetTaskActive();
		if(pActiveTask)
		{
			CTask* pTaskToPrint=pActiveTask;
			while(pTaskToPrint)
			{
				Assertf(pTaskToPrint->GetName(), "Found a task without a name!");
				bspyScratchpad(ibSpyID, "Active: The AI parent task", (const char*) pTaskToPrint->GetName());
				pTaskToPrint=pTaskToPrint->GetSubTask();
			}
		}

		// MOVE state
		if(!pPed->IsNetworkClone())
		{
			// Display the movement tasks
			pActiveTask=pPed->GetPedIntelligence()->GetActiveMovementTask();
			if(pActiveTask)
			{
				CTask* pTaskToPrint=pActiveTask;
				while(pTaskToPrint)
				{
					Assertf(pTaskToPrint->GetName(), "Found a task without a name!");
					bspyScratchpad(ibSpyID, "Move: The AI navigation task", (const char*) pTaskToPrint->GetName());
					pTaskToPrint=pTaskToPrint->GetSubTask();
				}
			}
		}
		
		// MOTION state
		pActiveTask = static_cast<CTask*>(pPed->GetPedIntelligence()->GetTaskManager()->GetActiveTask(PED_TASK_TREE_MOTION));
		if(pActiveTask)
		{
			CTask* pTaskToPrint=pActiveTask;
			while(pTaskToPrint)
			{
				Assertf(pTaskToPrint->GetName(), "Found a task without a name!");
				bspyScratchpad(ibSpyID, "Motion: The AI motion task", (const char*) pTaskToPrint->GetName());
				pTaskToPrint=pTaskToPrint->GetSubTask();
			}
		}
	}
}

void CTaskNMBehaviour::SetbSpyScratchpadString(const char* pDebugString)
{
	using namespace ART;
	int ibSpyID = bspyLastSeenAgent;
	CPed* pPed = GetPed();
	if(pPed != NULL)
	{
		ibSpyID = ART::gRockstarARTInstance->getBSpyIDFromCharacterID(pPed->GetRagdollInst()->GetNMAgentID());
	}

	bspyScratchpad(ibSpyID, GetName(), pDebugString);
}

void CTaskNMBehaviour::SetbSpyScratchpadBool(bool debugBool)
{
	using namespace ART;
	int ibSpyID = bspyLastSeenAgent;
	CPed* pPed = GetPed();
	if(pPed != NULL)
	{
		ibSpyID = ART::gRockstarARTInstance->getBSpyIDFromCharacterID(pPed->GetRagdollInst()->GetNMAgentID());
	}

	bspyScratchpad(ibSpyID, GetName(), debugBool);
}

void CTaskNMBehaviour::SetbSpyScratchpadVector3(const Vector3& pDebugVec)
{
	using namespace ART;
	int ibSpyID = bspyLastSeenAgent;
	CPed* pPed = GetPed();
	if(pPed != NULL)
	{
		ibSpyID = ART::gRockstarARTInstance->getBSpyIDFromCharacterID(pPed->GetRagdollInst()->GetNMAgentID());
	}

	bspyScratchpad(ibSpyID, GetName(), pDebugVec);
}

void CTaskNMBehaviour::SetbSpyScratchpadInt(int debugInt)
{
	using namespace ART;
	int ibSpyID = bspyLastSeenAgent;
	CPed* pPed = GetPed();
	if(pPed != NULL)
	{
		ibSpyID = ART::gRockstarARTInstance->getBSpyIDFromCharacterID(pPed->GetRagdollInst()->GetNMAgentID());
	}

	bspyScratchpad(ibSpyID, GetName(), debugInt);
}

void CTaskNMBehaviour::SetbSpyScratchpadFloat(float debugFloat)
{
	using namespace ART;
	int ibSpyID = bspyLastSeenAgent;
	CPed* pPed = GetPed();
	if(pPed != NULL)
	{
		ibSpyID = ART::gRockstarARTInstance->getBSpyIDFromCharacterID(pPed->GetRagdollInst()->GetNMAgentID());
	}

	bspyScratchpad(ibSpyID, GetName(), debugFloat);
}
#endif

#if ART_ENABLE_BSPY
#ifdef ROCKSTAR_GAME_EMBEDDED
#undef ROCKSTAR_GAME_EMBEDDED
#endif
#ifdef ART_MONOLITHIC
#undef ART_MONOLITHIC
#endif
#endif


// Tunable parameters /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CTaskNMBehaviour::ms_fStdPlayerRagdollControlForce = 0.02f;
bool CTaskNMBehaviour::ms_bUseParameterSets = true;
#if __DEV
bool CTaskNMBehaviour::ms_bDisplayDebug = false;
#endif // __DEV

u32 CTaskNMControl::m_DebugFlags = 0;
#if DEBUG_DRAW
bool CTaskNMControl::m_bDisplayFlags = false;
#endif

dev_bool sbDoPlayerRagdollInput = false;
dev_float sfRecoverEventThresholdPlayer = 1.0f;
dev_float sfRecoverEventThresholdAI = 1.0f;
dev_float sfRecoverEventThresholdWeakAI = 0.3f;
dev_u32 snPullPlayerOffCarTime = 10000;
dev_float sfPullPlayerOffCarMult = 0.01f;
dev_float sfPullPlayerOffCarMax = 20.0f;
dev_u32 snMaxMaxTimeToTimeOut = 100000;	// 100sec
dev_u32 snMinTimeBeforeBuoyancyInterrupt = 750;
dev_u32 snTimeOutBuoyancy = 575;
dev_u32 snTimeOutBuoyancyRelax = 1500;
dev_u32 snTimeOutRelax = 1000;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::IsMessageString(eNMStrings nString)
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	for(int i=0; i<NM_NUM_MESSAGES; i++)
	{
		if(CNmDefines::ms_nMessageList[i]==nString)
			return true;
	}
	return false;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
eEffectorMaskStrings CTaskNMBehaviour::GetMaskEnumFromString(const char * maskString)
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	for (int i=0; i<NM_NUM_MASK_STRINGS + 1; i++)
	{
		if (strcmp(maskString, CNmDefines::ms_aNMMaskStrings[i]) == 0)
			return (eEffectorMaskStrings) i;
	}

	return NM_MASK_FULL_BODY;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBehaviour::CTaskNMBehaviour(u32 nMinTime, u32 nMaxTime)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_bHasAborted = false;
	m_bHasFailed = false;
	m_bHasSucceeded = false;
	m_bDoBehaviorStart = true;
	m_bCuffedClipLoaded = false;
	m_RefFrameVelChecked = false;
	m_bUseAdaptiveAngularVel = false;
	m_bStartMessagesSent = false;

	m_suggestedClipId = CLIP_ID_INVALID;
	m_suggestedClipSetId = CLIP_SET_ID_INVALID;
	m_suggestedClipPhase = 0.0f;
	m_nSuggestedBlendOption = BLEND_FROM_NM_GETUP;
	m_nSuggestedNextTask = CTaskTypes::TASK_INVALID_ID;

	m_clipPoseSetId = CLIP_SET_ID_INVALID;
	m_clipPoseId = CLIP_ID_INVALID;
	m_iClipDictIndex = ANIM_DICT_INDEX_INVALID; 
	m_iClipHash = ANIM_HASH_INVALID; 

	Assert(nMinTime <= nMaxTime && nMaxTime < 65536);
	m_nMinTime = (u16)nMinTime;
	m_nMaxTime = (u16)nMaxTime;
	m_nStartTime = 0;
	m_nSettledStartTime = 0;

	m_AdaptiveAngVelMinVel = 0.0f;
	m_AdaptiveAngVelMaxVel = 15.0f;
	m_AdaptiveAngVelDampingVel2 = phArticulatedBody::sm_RagdollDampingAngV2;

	m_nFlags = 0;

	m_ContinuousContactTime = 0.0f;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBehaviour::CTaskNMBehaviour(const CTaskNMBehaviour& otherTask)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_bHasAborted = false;
	m_bHasFailed = false;
	m_bHasSucceeded = false;
	m_bDoBehaviorStart = true;
	m_bCuffedClipLoaded = false;
	m_RefFrameVelChecked = false;
	m_bUseAdaptiveAngularVel = false;
	m_bStartMessagesSent = otherTask.m_bStartMessagesSent;

	m_suggestedClipId = CLIP_ID_INVALID;
	m_suggestedClipSetId = CLIP_SET_ID_INVALID;
	m_suggestedClipPhase = 0.0f;
	m_nSuggestedBlendOption = otherTask.m_nSuggestedBlendOption;
	m_nSuggestedNextTask = otherTask.m_nSuggestedNextTask;

	m_clipPoseSetId = CLIP_SET_ID_INVALID;
	m_clipPoseId = CLIP_ID_INVALID;
	m_iClipDictIndex = ANIM_DICT_INDEX_INVALID; 
	m_iClipHash = ANIM_HASH_INVALID;

	m_nMinTime = otherTask.m_nMinTime;
	m_nMaxTime = otherTask.m_nMaxTime;
	m_nStartTime = otherTask.m_nStartTime;
	m_nSettledStartTime = otherTask.m_nSettledStartTime;

	m_AdaptiveAngVelMinVel = otherTask.m_AdaptiveAngVelMinVel;
	m_AdaptiveAngVelMaxVel = otherTask.m_AdaptiveAngVelMaxVel;
	m_AdaptiveAngVelDampingVel2 = otherTask.m_AdaptiveAngVelDampingVel2;

	m_nFlags = otherTask.m_nFlags;

	m_ContinuousContactTime = otherTask.m_ContinuousContactTime;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBehaviour::~CTaskNMBehaviour()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::IsHighLODNMAgent(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	bool isHiLODPed = pPed && pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH;
	return isHiLODPed && pPed->GetRagdollState()==RAGDOLL_STATE_PHYS && pPed->GetRagdollInst()->GetARTAssetID() != -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::ForceFallOver()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed* pPed = GetPed();

	if (pPed && pPed->GetRagdollInst())
	{
		sm_Tunables.m_forceFall.Post(*pPed, this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::ApplyTaskTuningMessage(const CNmMessage& message)
//////////////////////////////////////////////////////////////////////////
{
	if (!GetEntity())
		return;

	CPed* pPed = GetPed();

	if (!pPed)
		return;

	switch (message.m_Name.GetHash())
	{
		//////////////////////////////////////////////////////////////////////////
		//	Set ragdoll damping values from tuning
		//////////////////////////////////////////////////////////////////////////
		case NmTaskMessages::CTaskNM_RagdollDamping:
		{
			Assert(pPed->GetRagdollInst());
			if (pPed->GetRagdollInst()->GetArchetype())
			{
				phArchetypeDamp* pArch = static_cast<phArchetypeDamp*>(pPed->GetRagdollInst()->GetArchetype());
				for (s32 i=0; i<message.m_Params.GetCount(); i++)
				{
					const CNmParameter* pParam = message.m_Params[i];
					switch (message.m_Params[i]->m_Name)
					{
					case NmTaskMessages::LinConstant: pArch->ActivateDamping(phArchetypeDamp::LINEAR_C, ((const CNmParameterVector*)pParam)->m_Value); break;
					case NmTaskMessages::LinVelocity: pArch->ActivateDamping(phArchetypeDamp::LINEAR_V, ((const CNmParameterVector*)pParam)->m_Value); break;
					case NmTaskMessages::LinVelocity2: pArch->ActivateDamping(phArchetypeDamp::LINEAR_V2, ((const CNmParameterVector*)pParam)->m_Value); break;
					case NmTaskMessages::AngConstant: pArch->ActivateDamping(phArchetypeDamp::ANGULAR_C, ((const CNmParameterVector*)pParam)->m_Value); break;
					case NmTaskMessages::AngVelocity: pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V, ((const CNmParameterVector*)pParam)->m_Value); break;
					case NmTaskMessages::AngVelocity2: pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V2, ((const CNmParameterVector*)pParam)->m_Value); break;
					case NmTaskMessages::AdaptiveAngVelocity2Max: m_AdaptiveAngVelDampingVel2 = ((const CNmParameterFloat*)pParam)->m_Value; break;
					case NmTaskMessages::AdaptiveAngVelocity2: m_bUseAdaptiveAngularVel = ((const CNmParameterBool*)pParam)->m_Value; break;
					case NmTaskMessages::AdaptiveAngVelocity2_MinMag: m_AdaptiveAngVelMinVel = ((const CNmParameterFloat*)pParam)->m_Value; break;
					case NmTaskMessages::AdaptiveAngVelocity2_MaxMag: m_AdaptiveAngVelMaxVel = ((const CNmParameterFloat*)pParam)->m_Value; break;
					}
				}
			}
		}
		break;
		case NmTaskMessages::CTaskNM_UpdateCharacterHealth:
		{
			Assert(pPed->GetRagdollInst());
			float fHealth = 1.0f;
			// get the peds current health and convert it to appropriate nm health
			fHealth = pPed->GetHealth();
			fHealth = RampValueSafe(fHealth, pPed->GetInjuredHealthThreshold(), pPed->GetMaxHealth(), 0.0f, 1.0f);

			ART::MessageParams msgHealth;
			msgHealth.addFloat("characterHealth", fHealth);
			pPed->GetRagdollInst()->PostARTMessage("setCharacterHealth", &msgHealth);
		}
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::SendStartMessages(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (!m_bStartMessagesSent)
	{
		CNmMessageList list;
		// Stop all currently running behaviours before we start new ones (except for script controlled guys).
		if (GetTaskType() != CTaskTypes::TASK_NM_SCRIPT_CONTROL)
		{
			list.GetMessage(NMSTR_MSG(NM_STOP_ALL_MSG));
		}

		AddTuningSet(&sm_Tunables.m_Start);

		AddStartMessages(pPed, list);

		ApplyTuningSetsToList(list);

		// Send the handcuff message if applicable
		if (pPed->GetRagdollConstraintData() && (pPed->GetRagdollConstraintData()->HandCuffsAreEnabled() || pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled()))
		{
			ART::MessageParams& msg = list.GetMessage(NMSTR_MSG(NM_CONFIGURE_CONSTRAINTS_MSG));
			msg.addBool(NMSTR_PARAM(NM_START), true);
			if (pPed->GetRagdollConstraintData()->HandCuffsAreEnabled())
			{
				msg.addBool(NMSTR_PARAM(NM_CONFIGURE_CONSTRAINTS_HAND_CUFFS), true);
				msg.addBool(NMSTR_PARAM(NM_CONFIGURE_CONSTRAINTS_HAND_CUFFS_BEHIND_BACK), true);
			}
			if (pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled())
				msg.addBool(NMSTR_PARAM(NM_CONFIGURE_CONSTRAINTS_LEG_CUFFS), true);
		}

		list.Post(pPed->GetRagdollInst());

		m_bStartMessagesSent = true;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::QueryNmFeedbackMessage(ARTFeedbackInterfaceGta* pFeedbackInterface, enum eNMStrings paramName)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Helper function at file scope to test whether the feedback interface passed in corresponds to the
	// NM behaviour referenced by paramName.

	return !strcmp(NMSTR_PARAM(paramName), pFeedbackInterface->m_behaviourName);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Minimum functionality here, we just set a few choice behaviour feedback flags in the control task and leave
	// the specifics up to the actual derived behaviour class.

	CTaskNMControl *pNmControlTask = GetControlTask();
	if(QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		pNmControlTask->SetFeedbackFlags(pPed, CTaskNMControl::BALANCE_FAILURE);
	}

#if DEBUG_DRAW
	if(!CNmDebug::ms_bDrawFeedbackHistory)
		return;

	if(!CNmDebug::ms_bFbMsgShowFailure)
		return;

	// Allow filtering by ped address.
	if(CNmDebug::ms_bFbMsgOnlyShowFocusPed && (pPed != CNmDebug::ms_pFocusPed) )
		return;
	
	char zPedAddress[9];
	sprintf(zPedAddress, "%8p", pPed);
	atString feedbackMsg("(PED 0x");
	feedbackMsg += atString(zPedAddress);
	feedbackMsg += ") FAILURE: ";
	feedbackMsg += atString(pFeedbackInterface->m_behaviourName);
	CNmDebug::AddBehaviourFeedbackMessage(feedbackMsg.c_str());
#endif // DEBUG_DRAW
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::BehaviourSuccess(CPed* DEBUG_DRAW_ONLY(pPed), ARTFeedbackInterfaceGta* DEBUG_DRAW_ONLY(pFeedbackInterface))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Minimum functionality here, just pass through the feedback string to the NmDebug class for optional
	// display on-screen.

#if DEBUG_DRAW
	if(!CNmDebug::ms_bDrawFeedbackHistory)
		return;

	if(!CNmDebug::ms_bFbMsgShowSuccess)
		return;

	// Allow filtering by ped address.
	if(CNmDebug::ms_bFbMsgOnlyShowFocusPed && (pPed != CNmDebug::ms_pFocusPed) )
		return;

	char zPedAddress[9];
	sprintf(zPedAddress, "%8p", pPed);
	atString feedbackMsg("(PED 0x");
	feedbackMsg += atString(zPedAddress);
	feedbackMsg += ") SUCCESS: ";
	feedbackMsg += atString(pFeedbackInterface->m_behaviourName);
	CNmDebug::AddBehaviourFeedbackMessage(feedbackMsg.c_str());
#endif // DEBUG_DRAW
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::BehaviourStart(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Minimum functionality here, just pass through the feedback string to the NmDebug class for optional
	// display on-screen.

	CTaskNMControl *pNmControlTask = GetControlTask();
	if(QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		pNmControlTask->SetFeedbackFlags(pPed, CTaskNMControl::BALANCE_STARTED);
	}

	// Run an anim pose on the arms if the hands are cuffed
	bool bAnimPoseArms = true;
#if __BANK
	bAnimPoseArms = CPedDebugVisualiserMenu::ms_bAnimPoseArms;
#endif
	if (bAnimPoseArms && pPed->GetRagdollConstraintData()&& pPed->GetRagdollConstraintData()->HandCuffsAreEnabled() && 
		!(GetFlags()&CTaskNMBehaviour::DO_CLIP_POSE))
	{
		// Stream in the cuffed clips
		fwMvClipSetId clipSet = CLIP_SET_ID_INVALID;
		if (fwClipSetManager::GetClipSet(MOVE_PED_HANDCUFFED))
			clipSet = MOVE_PED_HANDCUFFED;
#if __BANK
		fwClipSetManager::StreamIn_DEPRECATED(clipSet);
#endif
		m_bCuffedClipLoaded = clipSet.GetHash() && fwClipSetManager::Request_DEPRECATED(clipSet);
		Assert(m_bCuffedClipLoaded);
		if (m_bCuffedClipLoaded && !pPed->GetIsDeadOrDying())
		{
			SetFlag(CTaskNMBehaviour::DO_CLIP_POSE);
			SetClipPoseHelperClip(MOVE_PED_HANDCUFFED, CLIP_IDLE);
			GetControlTask()->GetClipPoseHelper().SetAnimPoseType(ANIM_POSE_HANDS_CUFFED);
		}
	}

#if DEBUG_DRAW
	if(!CNmDebug::ms_bDrawFeedbackHistory)
		return;

	if(!CNmDebug::ms_bFbMsgShowStart)
		return;

	// Allow filtering by ped address.
	if(CNmDebug::ms_bFbMsgOnlyShowFocusPed && (pPed != CNmDebug::ms_pFocusPed) )
		return;

	char zPedAddress[9];
	sprintf(zPedAddress, "%8p", pPed);
	atString feedbackMsg("(PED 0x");
	feedbackMsg += atString(zPedAddress);
	feedbackMsg += ") START: ";
	feedbackMsg += atString(pFeedbackInterface->m_behaviourName);
	CNmDebug::AddBehaviourFeedbackMessage(feedbackMsg.c_str());
#endif // DEBUG_DRAW
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::BehaviourFinish(CPed* DEBUG_DRAW_ONLY(pPed), ARTFeedbackInterfaceGta* DEBUG_DRAW_ONLY(pFeedbackInterface))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Minimum functionality here, just pass through the feedback string to the NmDebug class for optional
	// display on-screen.

#if DEBUG_DRAW
	if(!CNmDebug::ms_bDrawFeedbackHistory)
		return;

	if(!CNmDebug::ms_bFbMsgShowFinish)
		return;

	// Allow filtering by ped address.
	if(CNmDebug::ms_bFbMsgOnlyShowFocusPed && (pPed != CNmDebug::ms_pFocusPed) )
		return;

	char zPedAddress[9];
	sprintf(zPedAddress, "%8p", pPed);
	atString feedbackMsg("(PED 0x");
	feedbackMsg += atString(zPedAddress);
	feedbackMsg += ") FINISH: ";
	feedbackMsg += atString(pFeedbackInterface->m_behaviourName);
	CNmDebug::AddBehaviourFeedbackMessage(feedbackMsg.c_str());
#endif // DEBUG_DRAW
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Minimum functionality here, just pass through the feedback string to the NmDebug class for optional
	// display on-screen.

	if (CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_HAND_ANIMATION_FB))
	{
		CTaskNMControl* pParentTask = GetControlTask();
		if (pParentTask)
		{
			nmTaskDebugf(this, "Set hand pose");
			pParentTask->SetCurrentHandPose((CTaskNMControl::eNMHandPose)pFeedbackInterface->m_args[1].m_int, (CMovePed::ePedHand)pFeedbackInterface->m_args[0].m_int);
		}
	}

	if (pPed->IsLocalPlayer() && CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_TEETER_FB))
	{
		// allow the player to influence the direction of movement in teeter
		float fLeanRatio = pPed->GetMotionData()->GetDesiredMbrY() / MOVEBLENDRATIO_SPRINT;

		//calculate the true desired movement vector
		Vector3 targetPos(0.0f, pPed->GetMotionData()->GetDesiredMbrY(), 0.0f);
		targetPos.RotateZ(pPed->GetMotionData()->GetDesiredHeading());

		// Request a lean in the direction of motion
		ART::MessageParams msgLean;
		msgLean.addVector3(NMSTR_PARAM(NM_FORCELEANDIR_DIR), targetPos.x, targetPos.y, targetPos.z );
		msgLean.addFloat(NMSTR_PARAM(NM_FORCELEANDIR_LEAN_AMOUNT), Clamp(fLeanRatio, -1.0f, 1.0f));
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_FORCELEANDIR_MSG), &msgLean);
	}

#if DEBUG_DRAW
	if(!CNmDebug::ms_bDrawFeedbackHistory)
		return;

	if(!CNmDebug::ms_bFbMsgShowEvent)
		return;

	// Allow filtering by ped address.
	if(CNmDebug::ms_bFbMsgOnlyShowFocusPed && (pPed != CNmDebug::ms_pFocusPed) )
		return;

	char zPedAddress[9];
	sprintf(zPedAddress, "%8p", pPed);
	atString feedbackMsg("(PED 0x");
	feedbackMsg += atString(zPedAddress);
	feedbackMsg += ") EVENT: ";
	feedbackMsg += atString(pFeedbackInterface->m_behaviourName);
	CNmDebug::AddBehaviourFeedbackMessage(feedbackMsg.c_str());
#endif // DEBUG_DRAW
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	bool bCanAbortAfterRagdollEnded = (!pPed->GetUsingRagdoll() && pEvent &&  pEvent->GetEventPriority()>=E_PRIORITY_GETUP); // to abort immediately after the ragdoll task ends (without doing a getup) the new incoming event must be high priority

	// immediate is a hard abort to default state - so do it, plus switch back to animated right now
	if(iPriority==ABORT_PRIORITY_IMMEDIATE || pPed->IsNetworkClone() || bCanAbortAfterRagdollEnded)
	{
		nmTaskDebugf(this, "Aborting for event '%s'. immediate: %s, netClone: %s, ragdollEnded: %s", pEvent ? pEvent->GetName().c_str() : "none", iPriority==ABORT_PRIORITY_IMMEDIATE ? "yes" : "no", pPed->IsNetworkClone() ? "yes" : "no", bCanAbortAfterRagdollEnded ? "yes" : "no");
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
	{
		CTaskNMControl* pParentTask = SafeCast(CTaskNMControl, GetParent());

		if (pParentTask->GetFlags() & CTaskNMControl::CLONE_LOCAL_SWITCH)
		{
			nmTaskDebugf(this, "Aborting for clone to local switch");
			return CTask::ShouldAbort(iPriority, pEvent);
		}
		else if (pParentTask->GetIsFlagSet(aiTaskFlags::HandleCloneSwapToSameTaskType))
		{
			nmTaskDebugf(this, "Aborting for clone task of same type");
			return CTask::ShouldAbort(iPriority, pEvent);
		}
	}

	// Get a pointer to a new task event if one is passed in
	const CEventNewTask* pNewTaskEvent = (pEvent && ((CEvent*)pEvent)->GetEventType() == EVENT_NEW_TASK ) ? static_cast<const CEventNewTask*>(pEvent) : NULL;
	if(pNewTaskEvent && pNewTaskEvent->GetNewTask())
	{
		// allowed to go on to a blend to clip
		if(pNewTaskEvent->GetNewTask()->GetTaskType()==CTaskTypes::TASK_BLEND_FROM_NM)
		{
			nmTaskDebugf(this, "Aborting for event '%s' with a new CTaskGetup", pEvent ? pEvent->GetName() : "none");
			return CTask::ShouldAbort(iPriority, pEvent);
		}
		// or to another new NM task
		else if(((CTask *) pNewTaskEvent->GetNewTask())->IsNMBehaviourTask())
		{
			nmTaskDebugf(this, "Aborting for event '%s' with new nm behaviour task %s", pEvent ? pEvent->GetName().c_str() : "none", ((CTask *) pNewTaskEvent->GetNewTask())->GetName().c_str());
			return CTask::ShouldAbort(iPriority, pEvent);
		}
	}
	else
	{
		const aiTask* pTaskResponse = pPed->GetPedIntelligence()->GetPhysicalEventResponseTask();
		if(pTaskResponse==NULL)
			pTaskResponse = pPed->GetPedIntelligence()->GetEventResponseTask();

		if(pTaskResponse)
		{
			if(DoesResponseHandleRagdoll(pPed, pTaskResponse))
			{
				nmTaskDebugf(this, "Aborting for event '%s' with ragdoll handling response task %s", pEvent ? pEvent->GetName().c_str() : "none", pTaskResponse->GetName().c_str());
				return CTask::ShouldAbort(iPriority, pEvent);
			}
		}
	}

	return false;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	nmTaskDebugf(this, "Aborting for event '%s'",  pEvent ? pEvent->GetName() : "none");

	if(iPriority==ABORT_PRIORITY_IMMEDIATE)
	{
		// don't switch to animated if it's a ragdoll task that's making us abort
		if(!pEvent || !((CEvent*)pEvent)->RequiresAbortForRagdoll())
		{
			if (!(m_nFlags & DONT_SWITCH_TO_ANIMATED_ON_ABORT))
			{
				bool bDoSwitchToAnimated = true;
				if (GetParent() && GetParent()->GetTaskType()==CTaskTypes::TASK_NM_CONTROL)
				{
					CTaskNMControl* pParent = smart_cast<CTaskNMControl*>(GetParent());
					if (pParent->GetFlags()&CTaskNMControl::ON_MOTION_TASK_TREE)
					{
						CTaskNMBehaviour* pLowestNMTask = pPed->GetPedIntelligence()->GetLowestLevelNMTask(pPed);
						if (pLowestNMTask && pLowestNMTask!=this)
						{
							bDoSwitchToAnimated = false;
						}
					}
				}

				if (bDoSwitchToAnimated)
				{
					NET_DEBUG_TTY("**CTaskNMBehaviour Immediate abort!** Switch to animated");
					nmTaskDebugf(this, "CTaskNMBehaviour::DoAbort - Switching ped to animated");
					pPed->SwitchToAnimated(true, true, true);
				}
			}
			else
			{
				NET_DEBUG_TTY("**CTaskNMBehaviour Immediate abort!** Leave as ragdoll");
			}
		}
	}

	m_bHasAborted = true;

	// Base class
	CTask::DoAbort(iPriority, pEvent);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMBehaviour::ProcessPreFSM()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if ((m_nFlags & DONT_FINISH) || !CPhysics::GetSimulator()->GetSleepEnabled())
		m_bHasSucceeded = false;

	if(m_bHasAborted || m_bHasFailed || m_bHasSucceeded)
	{
		nmTaskDebugf(this, "Ending behaviour: %s, %s, %s", m_bHasAborted? "Aborted" : "", m_bHasFailed? "Failed" : "", m_bHasSucceeded? "Succeeded" : "");
		return FSM_Quit;
	}

	// Let the ped know that a valid task is in control of the ragdoll.

	if (pPed->GetUsingRagdoll())
		pPed->TickRagdollStateFromTask(*this);

	// Prevent ped from switching weapons when removing the weapon so we don't switch without 'swapping' the weapon
	pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableCellphoneAnimations, true);

	if (m_bUseAdaptiveAngularVel && pPed->GetRagdollInst() && pPed->GetRagdollInst()->GetArchetype())
	{
		phArchetypeDamp* pArch = static_cast<phArchetypeDamp*>(pPed->GetRagdollInst()->GetArchetype());
		float angVel = pPed->GetAngVelocity().Mag();
		float t = Clamp(((angVel-m_AdaptiveAngVelMinVel) / (m_AdaptiveAngVelMaxVel - m_AdaptiveAngVelMinVel)), 0.0f, 1.0f);
		float damping = Lerp(t, phArticulatedBody::sm_RagdollDampingAngV2 , m_AdaptiveAngVelDampingVel2);
		pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V2, Vector3(damping, damping, damping));
		//nmTaskDebugf(this, "AngularVel2 damping set to %.3f, angular vel=%.3f, t=%.3f", damping, angVel, t);
	}

	return FSM_Continue;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMBehaviour::UpdateFSM(const s32 iState, const FSM_Event iEvent)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pPed);
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		FSM_State(State_StreamingClips)
			FSM_OnEnter
				StreamingClips_OnEnter(pPed);
			FSM_OnUpdate
				return StreamingClips_OnUpdate(pPed);

		FSM_State(State_BehaviourRunning)
			FSM_OnEnter
				BehaviourRunning_OnEnter(pPed);
			FSM_OnUpdate
				return BehaviourRunning_OnUpdate(pPed);
			FSM_OnExit
				BehaviourRunning_OnExit(pPed);

		FSM_State(State_AnimatedFallback)
				FSM_OnEnter
				AnimatedFallback_OnEnter(pPed);
			FSM_OnUpdate
				return AnimatedFallback_OnUpdate(pPed);
			FSM_OnExit
				AnimatedFallback_OnExit(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMBehaviour::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return UpdateFSM(iState, iEvent);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::ControlPassingAllowed(CPed* pPed, const netPlayer& player, eMigrationType migrationType)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If we're in the animated fall back state but haven't yet created a subtask then don't allow control passing
	// Certain animated fall back sub tasks (such as CTaskFallOver) don't allow control passing until they've entered a certain state
	// If those sub tasks haven't yet been created then we have to assume that we can't yet pass control!
	if (GetState() == State_AnimatedFallback && !GetSubTask() && !GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		return false;
	}

	return CTaskFSMClone::ControlPassingAllowed(pPed, player, migrationType);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone* CTaskNMBehaviour::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTaskNMBehaviour* pNewTask = static_cast<CTaskNMBehaviour*>(Copy());

	if (pNewTask && pNewTask->GetState() == State_AnimatedFallback)
	{
		const CTaskFallOver* pTaskFallOver = static_cast<CTaskFallOver*>(FindSubTaskOfType(CTaskTypes::TASK_FALL_OVER));
		if (pTaskFallOver != NULL)
		{
			pNewTask->m_suggestedClipId = pTaskFallOver->GetClipId();
			pNewTask->m_suggestedClipSetId = pTaskFallOver->GetClipSetId();
			pNewTask->m_suggestedClipPhase = pTaskFallOver->GetClipPhase();
		}
	}

	return pNewTask;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone* CTaskNMBehaviour::CreateTaskForLocalPed(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return CreateTaskForClonePed(pPed);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::Start_OnEnter(CPed* BANK_ONLY(pPed))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Due to assumptions made by the task control systems, an NM behaviour task must
	// be correctly assigned to a CTaskNMControl.

	taskAssertf(dynamic_cast<CTaskNMControl*>(GetParent()), "Parent task should be an NM Control task but is instead %s", GetParent() != NULL ? GetParent()->GetTaskName() : "(null)");

#if __BANK
	//tuning set history cleanup
	if (pPed && pPed==m_pTuningSetHistoryPed)
	{
		m_pTuningSetHistoryPed=NULL;
		m_TuningSetHistory.clear();
	}
#endif //__BANK
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMBehaviour::Start_OnUpdate(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (!nmVerifyf(GetParent() != NULL && GetParent()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL, 
		"CTaskNMBehaviour::Start_OnUpdate: Task %s has invalid parent task (%s), aborting!", 
		GetTaskName(), GetParent() != NULL ? GetParent()->GetTaskName() : "(null)"))
	{
		return FSM_Quit;
	}

	if (!pPed->GetUsingRagdoll())
	{
		if (pPed->GetMovePed().GetState() == CMovePed::kStateStaticFrame)
		{
			nmTaskDebugf(this, "Quitting task due to ped being in an animated static frame state");
			m_bHasSucceeded = true;
			m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
			return FSM_Quit;
		}
		else
		{
			nmTaskDebugf(this, "Switching to animated fallback");
			SetState(State_AnimatedFallback);
			return FSM_Continue;
		}
	}

	// Depending on the status of the control flags in the CTaskNMControl object responsible for
	// this task, we may need to set up some resources before starting this NM behaviour.
	CTaskNMControl *pControlTask = NULL;
	pControlTask = smart_cast<CTaskNMControl*>(GetParent());
	// TODO RA: For debugging, allow the clippose to be turned on for any ped from the RAG widget: A.I.\NaturalMotion\Request clipPose behaviour.
	if(pControlTask->GetFlags() & CTaskNMControl::DO_CLIP_POSE || pControlTask->m_DebugFlags & CTaskNMControl::DO_CLIP_POSE ||
		GetFlags()&CTaskNMBehaviour::DO_CLIP_POSE)
	{
		// If the controlling task wants an clipPose behaviour to run on part of the ragdoll, make
		// sure any clip resources are streamed in.
		SetState(State_StreamingClips);
	}
	else
	{
		// No resources required, just start the NM behaviour.
		SetState(State_BehaviourRunning);
	}

	return FSM_Continue;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::StreamingClips_OnEnter(CPed* UNUSED_PARAM(pPed))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Pass on the change of clip to the ClipPose helper class here so that it doesn't try to change
	// to an clip which isn't in memory.
	if (m_iClipDictIndex == ANIM_DICT_INDEX_INVALID)
	{
		GetClipPoseHelper().SelectClip(m_clipPoseSetId, m_clipPoseId);
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMBehaviour::StreamingClips_OnUpdate(CPed* UNUSED_PARAM(pPed))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Poll the clip streaming helper every cycle until it signals that it has
	//A hack to deal with the fact scripts will have loaded the clip and the clip pose helper does not deal with script clips
	
	bool bClipsLoaded = GetClipPoseHelper().RequestClips();
	

	if(bClipsLoaded)
	{
		SetState(State_BehaviourRunning);
	}

	return FSM_Continue;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::BehaviourRunning_OnEnter(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If not an NM agent, switch to the rage ragdoll task
	if (pPed->GetUsingRagdoll() && pPed->GetRagdollInst()->GetNMAgentID() == -1)
	{
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_RAGE_RAGDOLL;
		return;
	}

	Assert(pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH);

	bool onEnterSkipped = false;
	if (GetControlTask() && (GetControlTask()->GetFlags() & CTaskNMControl::ALREADY_RUNNING) && m_bDoBehaviorStart)
	{
		onEnterSkipped = true;
		m_bDoBehaviorStart = false;
	}

	if (m_bDoBehaviorStart)
	{
		// Set the start time.
		ResetStartTime();

		SendStartMessages(pPed);

		// Now do behaviour specific startup code.
		nmTaskDebugf(this, "Start behaviour");
		StartBehaviour(pPed);
	}

	// If we've been told to play an authored clip over part of the ragdoll, use the helper to achieve this.
	CTaskNMControl *pControlTask = GetControlTask();
	// TODO RA: For debugging, allow the clippose to be turned on for any ped from the RAG widget: A.I.\NaturalMotion\Request clipPose behaviour.
	if(pControlTask->GetFlags() & CTaskNMControl::DO_CLIP_POSE || pControlTask->m_DebugFlags & CTaskNMControl::DO_CLIP_POSE ||
		GetFlags()&CTaskNMBehaviour::DO_CLIP_POSE)
	{
		GetClipPoseHelper().StartBehaviour(pPed);
	}

	if (onEnterSkipped)
	{
		m_bDoBehaviorStart = true;
		GetControlTask()->ClearFlag(CTaskNMControl::ALREADY_RUNNING);
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMBehaviour::BehaviourRunning_OnUpdate(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (!pPed->GetUsingRagdoll())
	{
		if (pPed->GetMovePed().GetState() == CMovePed::kStateStaticFrame)
		{
			nmTaskDebugf(this, "Quitting task due to ped being in an animated static frame state");
			m_bHasSucceeded = true;
			m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
			return FSM_Quit;
		}
		else
		{
			nmTaskDebugf(this, "Switching to animated fallback");
			SetState(State_AnimatedFallback);
			return FSM_Continue;
		}
	}

	// If not an NM agent, switch to the rage ragdoll task
	if (pPed->GetUsingRagdoll() && pPed->GetRagdollInst()->GetNMAgentID() == -1)
	{
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_RAGE_RAGDOLL;
		return FSM_Continue;
	}

	Assert(pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH);

	// If balancing, allow extra penetration to reduce the chance of needing pushes
	phCollider *collider = const_cast<phCollider *>(pPed->GetCollider());
	Assert(collider);
	if (collider)
	{
		float extraPenetration = FRAGNMASSETMGR->ShouldAllowExtraPenetration(pPed->GetRagdollInst()->GetNMAgentID()) ? fragInstNM::GetExtraAllowedRagdollPenetration() : 0.0f;
		collider->SetExtraAllowedPenetration(extraPenetration);
	} 
	
	// is the ped in contact with a moving vehicle?
	CVehicle* pVehicle = NULL;
	if (pPed->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
	{
		const CCollisionRecord* pColRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_VEHICLE);
		if (pColRecord != NULL)
		{
			pVehicle = static_cast<CVehicle*>(pColRecord->m_pRegdCollisionEntity.Get());
		}
	}

	if (pVehicle)
	{
		m_ContinuousContactTime += fwTimer::GetTimeStep();
	}
	else
	{
		m_ContinuousContactTime = 0.0f;
	}
	
	// Execute any behaviour specific code.
	ControlBehaviour(pPed);

#if ART_ENABLE_BSPY
	// Send the task state to bSpy
	SendTaskStateToBSpy();
#endif

	// Ask the running behaviour task if it should exit or not.
	bool bReturn = false;
	
	if (!(m_nFlags & DONT_FINISH) && CPhysics::GetSimulator()->GetSleepEnabled())
		bReturn = FinishConditions(pPed);

	if(bReturn==false)
	{
		// If we've been told to play an authored clip over part of the ragdoll, use the helper to achieve this.
		CTaskNMControl *pControlTask = GetControlTask();
		// TODO RA: For debugging, allow the clippose to be turned on for any ped from the RAG widget: A.I.\NaturalMotion\Request clipPose behaviour.
		if(pControlTask->GetFlags() & CTaskNMControl::DO_CLIP_POSE || pControlTask->m_DebugFlags & CTaskNMControl::DO_CLIP_POSE ||
			GetFlags()&CTaskNMBehaviour::DO_CLIP_POSE)
		{
			CClipPoseHelper& refClipPoseHelper = GetClipPoseHelper();
			// Update any parameters which may change during a task's lifetime:
			if(refClipPoseHelper.GetNeedToStreamClips())
			{
				m_bDoBehaviorStart = false;
				SetState(State_StreamingClips);
			}
			else
			{
				refClipPoseHelper.ControlBehaviour(pPed);
			}
		}

		// We aren't quitting yet, so tell the ped we need to call ProcessPhysics() for this task.
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

		// If we haven't started a balance after a certain amount of time then consider the balance to have failed
		if ((fwTimer::GetTimeInMilliseconds() > m_nStartTime + snTimeOutRelax) && !pControlTask->IsFeedbackFlagSet(CTaskNMControl::BALANCE_STARTED))
		{
			pControlTask->SetFeedbackFlags(pPed, CTaskNMControl::BALANCE_FAILURE);
		}
	}
	else
	{
		nmTaskDebugf(this, "Ending behaviour - finish conditions met");
		SetState(State_Finish);
	}

	return FSM_Continue;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::BehaviourRunning_OnExit(CPed* UNUSED_PARAM(pPed))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If we've been playing an authored clip over part of the ragdoll, make sure that it has stopped
	// playing clips.
	taskAssert(dynamic_cast<CTaskNMControl*>(GetParent()));
	CTaskNMControl *pControlTask = smart_cast<CTaskNMControl*>(GetParent());
	// TODO RA: For debugging, allow the clippose to be turned on for any ped from the RAG widget: A.I.\NaturalMotion\Request clipPose behaviour.
	if(pControlTask->GetFlags() & CTaskNMControl::DO_CLIP_POSE || pControlTask->m_DebugFlags & CTaskNMControl::DO_CLIP_POSE ||
		GetFlags()&CTaskNMBehaviour::DO_CLIP_POSE)
	{
		GetClipPoseHelper().StopClip(NORMAL_BLEND_OUT_DELTA);
	}
}
//////////////////////////////////////////////////////////////////////////
void       CTaskNMBehaviour::AnimatedFallback_OnEnter(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	StartAnimatedFallback(pPed);
}

//////////////////////////////////////////////////////////////////////////
CTaskNMBehaviour::FSM_Return CTaskNMBehaviour::AnimatedFallback_OnUpdate(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	if (pPed->GetUsingRagdoll())
	{
		nmTaskDebugf(this, "Switching out of animated fallback");
		SetState(State_BehaviourRunning);
		return FSM_Continue;
	}

	bool bContinue = ControlAnimatedFallback(pPed);
	if (bContinue)
	{
		return FSM_Continue;
	}
	else
	{
		return FSM_Quit;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::CreateAnimatedFallbackTask(CPed* pPed, bool bFalling)
//////////////////////////////////////////////////////////////////////////
{
	fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
	fwMvClipId clipId = CLIP_ID_INVALID;

	CTaskNMControl* pControlTask = GetControlTask();

	Vector3 vecHeadPos(0.0f,0.0f,0.0f);
	pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);
	// If the ped is already prone then use an on-ground damage reaction
	if (vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf())
	{
		// If we're already on the ground don't do anything when falling
		const EstimatedPose pose = pPed->EstimatePose();
		if (pose == POSE_ON_BACK)
		{
			clipId = CLIP_DAM_FLOOR_BACK;
		}
		else
		{
			clipId = CLIP_DAM_FLOOR_FRONT;
		}

		Matrix34 rootMatrix;
		if (pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT))
		{
			float fAngle = AngleZ(pPed->GetTransform().GetForward(), RCC_VEC3V(rootMatrix.c)).Getf();
			if (fAngle < -QUARTER_PI)
			{
				clipId = CLIP_DAM_FLOOR_LEFT;
			}
			else if (fAngle > QUARTER_PI)
			{
				clipId = CLIP_DAM_FLOOR_RIGHT;
			}
		}

		clipSetId = pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();

		SetNewTask(rage_new CTaskAnimatedFallback(clipSetId, clipId, 0.0f, 0.0f));
	}
	else
	{
		if (bFalling)
		{
			float fStartPhase = 0.0f;
			// If this task is starting as the result of a migration then use the suggested clip and start phase
			CTaskNMControl* pControlTask = GetControlTask();
			if (pControlTask != NULL && (pControlTask->GetFlags() & CTaskNMControl::ALREADY_RUNNING) != 0 && 
				m_suggestedClipSetId != CLIP_SET_ID_INVALID && m_suggestedClipId != CLIP_SET_ID_INVALID)
			{
				clipSetId = m_suggestedClipSetId;
				clipId = m_suggestedClipId;
				fStartPhase = m_suggestedClipPhase;
			}
			else
			{
				CTaskFallOver::PickFallAnimation(pPed, CTaskFallOver::kContextStandard, CTaskFallOver::kDirFront, clipSetId, clipId);
			}

			CTaskFallOver* pNewTask = rage_new CTaskFallOver(clipSetId, clipId, 1.0f, fStartPhase);
			pNewTask->SetRunningLocally(true);

			SetNewTask(pNewTask);
		}
		// If this task is starting as the result of a migration then bypass the hit reaction
		else if (pControlTask != NULL && (pControlTask->GetFlags() & CTaskNMControl::ALREADY_RUNNING) == 0)
		{
			clipId = CLIP_DAM_FRONT;
			clipSetId = pPed->GetPedModelInfo()->GetAdditiveDamageClipSet();

			pPed->GetMovePed().GetAdditiveHelper().BlendInClipBySetAndClip(pPed, clipSetId, clipId);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::StartAnimatedFallback(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	CreateAnimatedFallbackTask(pPed, false);
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::ControlAnimatedFallback(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsNetworkClone() && !IsRunningLocally() && !GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		nmDebugf3("LOCAL: Controlling animated fallback task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");

		if (!GetSubTask())
		{
			// If this is running on a clone ped then wait until the owner ped has fallen over before starting the animated fall-back task
			CTaskNMControl* pTaskNMControl = GetControlTask();
			if (pTaskNMControl == NULL || pPed->IsDead() || ((pTaskNMControl->GetFlags() & CTaskNMControl::FORCE_FALL_OVER) != 0 || pTaskNMControl->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE)))
			{
				// Pretend that the balancer failed at this point (if it hasn't already been set) so that clones will know that the owner has 'fallen'
				if (pTaskNMControl == NULL || !pTaskNMControl->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE))
				{
					ARTFeedbackInterfaceGta feedbackInterface;
					feedbackInterface.SetParentInst(pPed->GetRagdollInst());
					strncpy(feedbackInterface.m_behaviourName, NMSTR_PARAM(NM_BALANCE_FB), ART_FEEDBACK_BHNAME_LENGTH);
					feedbackInterface.onBehaviourFailure();
				}

				CreateAnimatedFallbackTask(pPed, true);
			}
			// We won't have started playing a full body animation - so ensure the ped continues playing their idling animations
			else if ((pTaskNMControl->GetFlags() & CTaskNMControl::ON_MOTION_TASK_TREE) == 0)
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
			}
		}

		if (GetNewTask() != NULL || GetSubTask() != NULL)
		{
			// disable ped capsule control so the blender can set the velocity directly on the ped
			NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
		}

		return true;
	}
	else
	{
		// default control animated fallback - ends the task with a getup
		nmTaskDebugf(this, "Animated fallback - transitioning to the blend from nm task");
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
void       CTaskNMBehaviour::AnimatedFallback_OnExit(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	EndAnimatedFallback(pPed);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::InitStrings()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// check we've got the correct number of entries in the messages list
	Assert(CNmDefines::ms_nMessageList[NM_NUM_MESSAGES]==NM_STOP_ALL_MSG);
	// check the last entry in the nm strings list is correct
	Assert(!strncmp("nmstring_end", CNmDefines::ms_aNMStrings[NM_NUM_STRINGS], 12));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::IsParamForThisMessage(eNMStrings nMsgString, eNMStrings nParamString)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int nMessageIndex = -1;
	for(int i=0; i<NM_NUM_MESSAGES; i++)
	{
		if(CNmDefines::ms_nMessageList[i]==nMsgString)
		{
			nMessageIndex = i;
			break;
		}
	}

	// ok if we didn't find the message in the messages array, return false
	if(nMessageIndex==-1)
		return false;

	// if the param is lower than the desired message it can't be a param for that message, return false
	if(nParamString <= nMsgString)
		return false;

	// if the param is higher than the NEXT message, return false
	if(nMessageIndex < NM_NUM_MESSAGES - 1 && nParamString >= CNmDefines::ms_nMessageList[nMessageIndex + 1])
		return false;

	// must be ok!
	return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CTaskNMBehaviour::GetMessageStringIndex(eNMStrings nString)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	for(int i=0; i<NM_NUM_MESSAGES; i++)
	{
		if(CNmDefines::ms_nMessageList[i]==nString)
			return i;
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Code derived from CHandlingDataMgr::LoadHandlingData()
bool CTaskNMBehaviour::LoadTaskParameters()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
#if __BANK
	InitCreateWidgetsButton();
#endif

	return true;
}
//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::AddTuningSet(const CNmTuningSet* pSet)
//////////////////////////////////////////////////////////////////////////
{
	if (!pSet->IsEmpty() && pSet->IsEnabled())
	{
		m_Sets.PushAndGrow(pSet);

#if __BANK
		bool noSelection = false;
		if (g_PickerManager.GetSelectedEntity()==NULL && CDebugScene::FocusEntities_Get(0)==NULL)
		{
			noSelection = true;
		}

		// tuning set debugging
		if (GetEntity() && (noSelection || (GetPed()==g_PickerManager.GetSelectedEntity() || GetPed()==CDebugScene::FocusEntities_Get(0))))
		{
			if (m_pTuningSetHistoryPed!=GetPed())
			{
				m_pTuningSetHistoryPed = GetPed();
				m_TuningSetHistory.clear();
			}

			bool foundEntry = false;
			//search for an existing entry in the history
			for (s32 i=0; i<m_TuningSetHistory.GetCount(); i++)
			{
				if (m_TuningSetHistory[i].id==pSet->m_Id)
				{
					//update the time
					m_TuningSetHistory[i].time=fwTimer::GetTimeInMilliseconds();
					foundEntry = true;
					break;
				}
			}
		
			if (!foundEntry)
			{
				// add a new entry
				TuningSetEntry& entry = m_TuningSetHistory.Grow();
				entry.id=pSet->m_Id;
				entry.time=fwTimer::GetTimeInMilliseconds();
			}

			// sort the entries by time
			m_TuningSetHistory.QSort(0, -1, SortTuningSetHistoryFunc);
		}
#endif //__BANK
	}
}
//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::ClearTuningSets()
//////////////////////////////////////////////////////////////////////////
{
	m_Sets.clear();
}
//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::ApplyTuningSetsToList(CNmMessageList& list, bool bClearAfterSending/*=true*/)
//////////////////////////////////////////////////////////////////////////
{
	// sort the applied tuning sets by priority
	if (m_Sets.GetCount()>1)
		m_Sets.QSort(0, -1, CompareTuningSetPriorityFunc);

	for (int i=0; i<m_Sets.GetCount(); i++)
	{
		Assert(m_Sets[i]);
		m_Sets[i]->AddTo(list, this);
	}

	if (bClearAfterSending)
	{
		m_Sets.clear();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Code derived from CHandlingDataMgr::LoadHandlingData()
bool CTaskNMBehaviour::GetIsClipDictLoaded(s32 iClipDictIndex)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(iClipDictIndex != ANIM_DICT_INDEX_INVALID)
	{
		return CStreaming::HasObjectLoaded(strLocalIndex(iClipDictIndex), fwAnimManager::GetStreamingModuleId());
	}

	return false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(pPed->IsPlayer() && pPed->GetPlayerInfo() && (pPed->GetPlayerInfo()->GetPlayerDataHasControlOfRagdoll() || sbDoPlayerRagdollInput))
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		Vector2 vecStick;
		vecStick.x = pControl->GetPedWalkLeftRight().GetNorm() * 127.0f;
		vecStick.y = -pControl->GetPedWalkUpDown().GetNorm() * 127.0f;

#if USE_SIXAXIS_GESTURES
		// Sixaxis control of movement
		if(CControlMgr::GetPlayerPad() && CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_RAGDOLL))
		{
			CPadGesture* gesture = CControlMgr::GetPlayerPad()->GetPadGesture();
			if(gesture)
			{
				if(rage::Abs(vecStick.x) < 0.1f)
					vecStick.x = 127.0f*gesture->GetPadRollInRange(-0.5f,0.5f,true);
				if(rage::Abs(vecStick.y) < 0.1f)
					vecStick.y = -127.0f*gesture->GetPadPitchInRange(-0.5f,0.5f,true,false);
			}
		}
#endif // USE_SIXAXIS_GESTURES

		bool bApplyForce = false;
		bool bHeadLook = pPed->GetPlayerInfo()->GetPlayerDataHasControlOfRagdoll() != 0;

		if(vecStick.Mag2() > 0.0f)
		{
			bApplyForce = true;
		}
		//! This section is specific to the friend activity 'Drinking'. The
		//! m_bHasControlOfRagdoll flag is only set from that mission so we
		//! we use the flag to find out if the ped is drunk.
		//! If there is no input from the player we give some random directions.
		else if(pPed->GetPlayerInfo()->GetPlayerDataHasControlOfRagdoll())
		{
			/// add some random leaning
			static float x = 0.0f, y = 0.0f;
			static int frame = 0;

			if (frame <= 0) {
				x = fwRandom::GetRandomNumberInRange(-8.0f, 8.0f);
				y = fwRandom::GetRandomNumberInRange(-64.0f, 64.0f);
				frame = fwRandom::GetRandomNumberInRange(10, 120);
			}
			--frame;

			vecStick.x = x;
			vecStick.y = y;

			bApplyForce = true;
		}

		if(bApplyForce)
		{
			//! relative to player for debugging
#if 0
			Vector3 vecDesiredForce(0.0f, 0.0f, 0.0f);
			vecDesiredForce += pPed->GetA() * vecStick.x;
			vecDesiredForce -= pPed->GetC() * vecStick.y;
			Vector3 vLookAt(vecDesiredForce);
			vecDesiredForce *= pPed->GetMass()*GetPlayerControlForce();
#else
			float fStickAngle = rage::Atan2f(-vecStick.x, vecStick.y);
			fStickAngle += camInterface::GetHeading(); // // careful.. this looks dodgy to me - and could be ANY camera - DW 
			fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);
			Vector3 vecDesiredForce(-rage::Sinf(fStickAngle), rage::Cosf(fStickAngle), 0.0f);
			Vector3 vLookAt = vecDesiredForce * 3.0f;
			vecDesiredForce *= vecStick.Mag()*pPed->GetMass()*GetPlayerControlForce();
#endif


			Vector3 vecRootBonePos;
			pPed->GetBonePosition(vecRootBonePos, BONETAG_ROOT);
			vecRootBonePos -= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			pPed->ApplyForce(vecDesiredForce, vecRootBonePos, 0);

			if(bHeadLook)
			{
				/// Set look-at position based on intended direction, this helps
				/// to turn the ped.
				vLookAt += VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

				ART::MessageParams msg;
				msg.addBool(NMSTR_PARAM(NM_BALANCE_USE_BODY_TURN), true);
				msg.addBool(NMSTR_PARAM(NM_BALANCE_LOOKAT_B), true);
				msg.addVector3(NMSTR_PARAM(NM_BALANCE_LOOKAT_POS_VEC3), vLookAt.x, vLookAt.y, vLookAt.z);
				pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_BALANCE_MSG), &msg);
			}

			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::ProcessBalanceBehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	float fInstabilityLimit = 0.0f;
	if(pPed->IsPlayer())
		fInstabilityLimit = sfRecoverEventThresholdPlayer;
	else if(pPed->CheckAgilityFlags(AF_RECOVER_BALANCE) && pPed->GetHealth() > 130.0f)
		fInstabilityLimit = sfRecoverEventThresholdAI;
	else if(pPed->CheckAgilityFlags(AF_RECOVER_BALANCE) || pPed->GetHealth() > 130.0f)	// does this check need to be revised?
		fInstabilityLimit = sfRecoverEventThresholdWeakAI;

	if(fInstabilityLimit > 0.0f && QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		// ALMOST BALANCED
		float fBalanceInstability = pFeedbackInterface->m_args[0].m_float;
		if(fBalanceInstability < fInstabilityLimit)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::OnMovingGround(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Determine whether the ped is on a moving platform
	bool movingPlatform = pPed && pPed->GetCollider() && MagSquared(pPed->GetCollider()->GetReferenceFrameVelocity()).Getf() > 0.2f;
	if (m_RefFrameVelChecked)
		return movingPlatform;

	if (!movingPlatform && pPed->GetUsingRagdoll())
	{
		Assert(pPed->GetRagdollInst() && pPed->GetRagdollInst()->GetCacheEntry() && pPed->GetRagdollInst()->GetCacheEntry()->GetBound());
		phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
		Matrix34 pelvisMat;
		pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(0)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 

		// Send off a probe
		WorldProbe::CShapeTestProbeDesc losDesc;
		WorldProbe::CShapeTestFixedResults<> ragdollGroundProbeResults;
		losDesc.SetResultsStructure(&ragdollGroundProbeResults);
		static float depth = 1.2f;
		Vector3 vecStart = pelvisMat.d;
		Vector3 vecEnd = pelvisMat.d;
		vecEnd.z -= depth;
		losDesc.SetStartAndEnd(vecStart, vecEnd);
		u32 nIncludeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE;
		losDesc.SetIncludeFlags(nIncludeFlags);
		losDesc.SetExcludeInstance(pPed->GetCurrentPhysicsInst());
		WorldProbe::GetShapeTestManager()->SubmitTest(losDesc);
		WorldProbe::ResultIterator it;
		for(it = ragdollGroundProbeResults.begin(); it < ragdollGroundProbeResults.end(); ++it)
		{
			if(it->GetHitDetected())
			{
				CEntity* pHitEntity = CPhysics::GetEntityFromInst(it->GetHitInst());
				if(pHitEntity && pHitEntity->IsCollisionEnabled() && pHitEntity->GetIsPhysical() && pPed->ValidateGroundPhysical(*it->GetHitInst(), it->GetHitPositionV(), it->GetHitComponent()))
				{
					ScalarV currRefFrameVel = MagSquared(pPed->GetCollider()->GetReferenceFrameVelocity());
					ScalarV probedVel = ScalarV(((CPhysical*)pHitEntity)->GetVelocity().Mag2());
					if (IsLessThanAll(currRefFrameVel, probedVel))
					{
						pPed->GetCollider()->SetReferenceFrameVelocity(Vec3V(((CPhysical*)pHitEntity)->GetVelocity()).GetIntrin128ConstRef());	
						movingPlatform = true;
					}
				}
			}
		}

		m_RefFrameVelChecked = true;
	}

	return movingPlatform;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::CheckBlendOutThreshold(CPed* pPed, const CTaskNMBehaviour::Tunables::BlendOutThreshold& threshold)
//////////////////////////////////////////////////////////////////////////
{
	phCollider* pCollider = pPed->GetCollider();
	Assert(pCollider && pPed->GetUsingRagdoll());
	Vector3 vecRootLinVelocity = Vector3(Vector3::ZeroType);
	Vector3 vecRootAngVelocity = Vector3(Vector3::ZeroType);
	if (pCollider)
	{	
		vecRootLinVelocity = VEC3V_TO_VECTOR3(pCollider->GetVelocity());
		vecRootAngVelocity = VEC3V_TO_VECTOR3(pCollider->GetAngVelocity());
	}

	const CTaskNMBehaviour::Tunables::BlendOutThreshold* pThreshold = &threshold;

	CVehicle* pVehicle = NULL;
	if(pPed->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
	{
		const CCollisionRecord* pColRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_VEHICLE);
		if (pColRecord != NULL)
		{
			pVehicle = static_cast<CVehicle*>(pColRecord->m_pRegdCollisionEntity.Get());

			// only consider vehicles that you can stand on as valid for subtracting velocity.
			// Otherwise, the physics scanner will just kick off a balance task as soon as we exit.
			if (pVehicle != NULL && (pVehicle->CanPedsStandOnTop() || GetIsStuckOnVehicle()))
			{
				// subtract the speed of a vehicle
				if (pCollider != NULL && IsGreaterThanAll(MagSquared(pCollider->GetReferenceFrameVelocity()), ScalarV(V_FLT_SMALL_6)) != 0)
				{
					vecRootLinVelocity -= RCC_VECTOR3(pCollider->GetReferenceFrameVelocity());
				}
				else
				{
					Vector3 vecLocalSpeed = pVehicle->GetLocalSpeed(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), true);
					vecRootLinVelocity -= vecLocalSpeed;
				}

				pThreshold = &sm_Tunables.m_StuckOnVehicleBlendOutThresholds.PickBlendOut(pPed);
			}
		}
	}

	// check velocity
	if(vecRootLinVelocity.Mag2() < square(pThreshold->m_MaxLinearVelocity) && vecRootAngVelocity.Mag2() < square(pThreshold->m_MaxAngularVelocity))
	{
		float fSettleTimeModifier = 1.0f;
		if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(pPed))
		{
			TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fRagdollSettleTimeModifier, 0.25f, 0.0f, 1.0f, 0.01f);
			fSettleTimeModifier = fRagdollSettleTimeModifier;
		}

		// Peds should be settled for a (tunable) continuous period before blending out.
		if (m_nSettledStartTime==0)
		{
			m_nSettledStartTime=fwTimer::GetTimeInMilliseconds();
		}

		u32 settleTime = pThreshold->GetSettledTime(((u32)(size_t)this+(m_nStartTime<<12)));
		settleTime = (u32)((float)settleTime * fSettleTimeModifier);
		nmDebugf3("SettleTime=%u, ActualTime=%u", settleTime, fwTimer::GetTimeInMilliseconds() - m_nSettledStartTime);
		if ((fwTimer::GetTimeInMilliseconds() - m_nSettledStartTime)>=settleTime)
		{
			return true;
		}
	}
	else
	{
		m_nSettledStartTime = 0;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::ProcessFinishConditionsBase(CPed* pPed, eMonitorState UNUSED_PARAM(nState), int nFlags, const CTaskNMBehaviour::Tunables::BlendOutThreshold* pCustomBlendOutThreshold)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

	Assert(pPed->GetCollider() && pPed->GetUsingRagdoll());
	Vector3 vecRootLinVelocity = Vector3(Vector3::ZeroType);
	if (pPed->GetCollider())
	{	
		vecRootLinVelocity = VEC3V_TO_VECTOR3(pPed->GetCollider()->GetVelocity());
	}

	if((nFlags &FLAG_VEL_CHECK) && ((nFlags &FLAG_SKIP_MIN_TIME_CHECK) || (fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime)))
	{

		const Tunables::BlendOutThreshold& thresholds = pCustomBlendOutThreshold ? *pCustomBlendOutThreshold : sm_Tunables.m_StandardBlendOutThresholds.PickBlendOut(pPed);

		bool velocityThresholdMet = CheckBlendOutThreshold(pPed, thresholds);

		if (velocityThresholdMet)
		{
			if(pPed->GetHealth() < 110.0f && !pPed->IsPlayer() && (nFlags &FLAG_RELAX_AP_LOW_HEALTH))
				m_nSuggestedNextTask = CTaskTypes::TASK_NM_RELAX;
			else if (pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled() && pPed->GetRagdollConstraintData()->HandCuffsAreEnabled() && !pPed->IsDead())
				m_nSuggestedNextTask = CTaskTypes::TASK_NM_INJURED_ON_GROUND;
			else if (pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled() && !pPed->IsDead())
				m_nSuggestedNextTask = CTaskTypes::TASK_NM_DANGLE;
			else if (!pPed->GetIsDeadOrDying())
			{
				bool bShouldBeDead = pPed->ShouldBeDead();
/*
				// players running a local death reaction need to stay on the ground until they get a health update saying they are dead, or the task is aborted
				if (pPed->IsAPlayerPed() && pPed->IsNetworkClone() && static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->IsRunningLocalDeathResponseTask())
				{
					bShouldBeDead = true;

				}
	*/			
				// Weve passed the velocity check, and the ped should be dead, but isn't yet.
				// Generate a ragdoll death event to settle the ragdoll.
				if (bShouldBeDead)
				{
					CEventDeath deathEvent(false, fwTimer::GetTimeInMilliseconds(), true);
					pPed->GetPedIntelligence()->AddEvent(deathEvent);
					m_nSuggestedNextTask = CTaskTypes::TASK_RAGE_RAGDOLL;
				}
				else
				{
					m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
				}
			}

			nmDebugf3("[%u]Finish %s on %s - velocity check passed. Next task: %s",fwTimer::GetFrameCount(), GetTaskName(), pPed->GetModelName(), TASKCLASSINFOMGR.GetTaskName(m_nSuggestedNextTask));
			return true;
		}

		CVehicle* pVehicle = NULL;
		if(pPed->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
		{
			const CCollisionRecord * pColRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_VEHICLE);
			pVehicle = pColRecord ? static_cast<CVehicle*>(pColRecord->m_pRegdCollisionEntity.Get()) : NULL;

			// only consider vehicles that you can stand on as valid for subtracting velocity.
			// Otherwise, the physics scanner will just kick off a balance task as soon as we exit.
			// We purposely don't check the FLAG_ALLOW_STAND_ON_VEHICLE flag here as that flag usually gets set when a ped is stuck in the bed of a truck where
			// it wouldn't make much sense to start applying a force...
			bool canStandOnVehicle = pVehicle ? pVehicle->CanPedsStandOnTop() : false;
			if (!canStandOnVehicle)
			{
				pVehicle = NULL;
			}
		}

		// if the player gets stuck on a vehicle for a long period of time, try and pull him off
		if(pVehicle && pVehicle->GetVehicleType()!=VEHICLE_TYPE_BOAT && pPed->IsPlayer()
			&& fwTimer::GetTimeInMilliseconds() > m_nStartTime + snPullPlayerOffCarTime)
		{
			Vector3 vecForce(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA()));
			vecForce.Scale(pPed->GetMass());
			vecForce.Scale(rage::Min(sfPullPlayerOffCarMax, sfPullPlayerOffCarMult*(m_nStartTime + snPullPlayerOffCarTime - fwTimer::GetTimeInMilliseconds())));
			pPed->ApplyForce(vecForce, VEC3_ZERO, 0);
		}
	}

	// if ped is dead most tasks end immediately.
	if(pPed->IsDead() && ((nFlags &FLAG_SKIP_DEAD_CHECK)==0) && !ShouldContinueAfterDeath() )
	{
		nmDebugf3("[%u]Finish %s on %s - Ped is dead. Next task: %s",fwTimer::GetFrameCount(), GetTaskName(), pPed->GetModelName(), TASKCLASSINFOMGR.GetTaskName(m_nSuggestedNextTask));
		return true;
	}

	// early time out if dangling
	static float dangleOutTime = 4000.0f;
	if (pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled() && !pPed->GetRagdollConstraintData()->HandCuffsAreEnabled() && fwTimer::GetTimeInMilliseconds() > m_nStartTime + dangleOutTime)
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_DANGLE;
		nmDebugf3("[%u]Finish %s on %s - EArly out for dangle task. Next task: %s",fwTimer::GetFrameCount(), GetTaskName(), pPed->GetModelName(), TASKCLASSINFOMGR.GetTaskName(m_nSuggestedNextTask));
		return true;
	}

	// check end timer
	// make sure the task times out if running on a network clone, otherwise the ped can get stuck in the task if his velocity does not fall below 4.0f
	u32 nTimeOutTime = snMaxMaxTimeToTimeOut;
	if(m_nMaxTime > 0 && (vecRootLinVelocity.Mag2() < 4.0f || pPed->GetTransform().GetPosition().GetZf() < 0.0f || pPed->GetWaterLevelOnPed() > 0.6f || pPed->IsNetworkClone()))
		nTimeOutTime = m_nMaxTime;
	
	// if time expires then blend back to clip
	if(fwTimer::GetTimeInMilliseconds() > m_nStartTime + nTimeOutTime)
	{
		if (pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled() && pPed->GetRagdollConstraintData()->HandCuffsAreEnabled() && !pPed->IsDead())
			m_nSuggestedNextTask = CTaskTypes::TASK_NM_INJURED_ON_GROUND;
		else if (pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled() && !pPed->IsDead())
			m_nSuggestedNextTask = CTaskTypes::TASK_NM_DANGLE;
		else if (!pPed->GetIsDeadOrDying())
		{
			// The ped should be dead, but isn't yet.
			// Generate a ragdoll death event to settle the ragdoll.
			if (pPed->ShouldBeDead())
			{
				CEventDeath deathEvent(false, fwTimer::GetTimeInMilliseconds(), true);
				pPed->GetPedIntelligence()->AddEvent(deathEvent);
				m_nSuggestedNextTask = CTaskTypes::TASK_RAGE_RAGDOLL;
			}
			else
			{
				m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
			}
		}

		nmDebugf3("[%u]Finish %s on %s - time expired. Next task: %s",fwTimer::GetFrameCount(), GetTaskName(), pPed->GetModelName(), TASKCLASSINFOMGR.GetTaskName(m_nSuggestedNextTask));
		return true;
	}

	// If the current NM task has run for a minimum amount of time and we are sufficiently submerged then override with a buoyancy task!
	u32 nMinTimeBeforeBuoyancyInterrupt = Max((u32)m_nMinTime, snMinTimeBeforeBuoyancyInterrupt);
	if (fwTimer::GetTimeInMilliseconds() > (m_nStartTime + nMinTimeBeforeBuoyancyInterrupt) && pPed->GetWaterLevelOnPed() > 0.6f && (nFlags & FLAG_SKIP_WATER_CHECK) == 0)
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_BUOYANCY;
		nmDebugf3("[%u]Finish %s on %s - ped in water. Next task: %s",fwTimer::GetFrameCount(), GetTaskName(), pPed->GetModelName(), TASKCLASSINFOMGR.GetTaskName(m_nSuggestedNextTask));
		return true;
	}

	return false;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::ResetStartTime()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_nStartTime = fwTimer::GetTimeInMilliseconds();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::DoesResponseHandleRagdoll(const CPed* pPed, const aiTask* pResponseTask)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Assert(pResponseTask);
	bool bHandlesRagdoll = false;

	//Grab the game task.
	Assert(dynamic_cast<const CTask *>(pResponseTask));
	const CTask* pTask = static_cast<const CTask *>(pResponseTask);
	
	//Check if the task can handle the ragdoll.
	bHandlesRagdoll = pTask->HandlesRagdoll(pPed);

	return bHandlesRagdoll;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::ShouldReactToVehicleHit(CPed* pPed, CVehicle* pVehicle, const CCollisionRecord* pColRecord)
//////////////////////////////////////////////////////////////////////////
{
	float pushLimit = pPed->GetPedModelInfo()->GetMinActivationImpulse();

	Tunables::ActivationLimitModifiers& limits =  NetworkInterface::IsGameInProgress() ? sm_Tunables.m_MpActivationModifiers : sm_Tunables.m_SpActivationModifiers;

	float fPushValue = CalculateActivationForce(pPed, RAGDOLL_TRIGGER_IMPACT_CAR, pVehicle, &RCC_VEC3V(pColRecord->m_OtherCollisionPos), &RCC_VEC3V(pColRecord->m_MyCollisionNormal), pColRecord->m_OtherCollisionComponent);

	pushLimit *= limits.m_BumpedByCar;

	if (pVehicle && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceStandardBumpReactionThresholds))
	{
		if (pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedIntelligence()->IsFriendlyWith(*pPed))
		{
			pushLimit *= limits.m_BumpedByCarFriendly;
		}

		if (pPed->IsPlayer())
		{
			pushLimit*= limits.m_PlayerBumpedByCar;

			if (pVehicle->IsNetworkClone())
			{
				pushLimit*=sm_Tunables.m_PlayerBumpedByCloneCarActivationModifier;
			}
		}

		CTaskEnterVehicle* pTask = static_cast<CTaskEnterVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));

		if (pTask && pTask->GetVehicle()==pVehicle)
		{
			// For peds who are trying to enter your vehicle, modify the activation limit based on movement speed (fast moving peds tend to just face plant
			// and they're generally more forgiving when their capsules are being pushed around anyway).
			Vector2 currentMbr;
			pPed->GetMotionData()->GetCurrentMoveBlendRatio(currentMbr);
			float speed = currentMbr.Mag();
			if (speed > MOVEBLENDRATIO_RUN)
			{
				pushLimit *= Lerp(speed-MOVEBLENDRATIO_RUN, limits.m_Running, limits.m_Sprinting);
			}
			else if (speed>MOVEBLENDRATIO_WALK)
			{
				pushLimit *= Lerp(speed-MOVEBLENDRATIO_WALK, limits.m_Walking, limits.m_Running);
			}
			else
			{
				pushLimit *= Lerp(speed, 1.0f, limits.m_Walking);
			}
		}
	}

	return fPushValue>pushLimit;
}

float CTaskNMBehaviour::GetMinSpeedThreshold(const CPed* pPed, const Vector3& pedVel)
{
	// Want to use ped velocity irrespective of ground velocity to determine if the ped is stationary or not
	bool bIsStationary = pPed->GetMotionData()->GetCurrentMbrX()<=0.01f && pPed->GetMotionData()->GetCurrentMbrY()<=0.01f && 
		MagSquared(Subtract(RCC_VEC3V(pedVel), pPed->GetGroundVelocityIntegrated())).Getf()<0.05f && 
		(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP)==NULL);
	if (bIsStationary)
	{
		if (pPed->IsPlayer())
			return sm_Tunables.m_VehicleMinSpeedForStationaryPlayerActivation;
		else
			return sm_Tunables.m_VehicleMinSpeedForStationaryAiActivation;
	}
	else
	{
		if (pPed->IsPlayer())
			return sm_Tunables.m_VehicleMinSpeedForPlayerActivation;
		else
			return sm_Tunables.m_VehicleMinSpeedForAiActivation;
	}
}

bool CTaskNMBehaviour::DoesGroundPhysicalMatch(const CPhysical* pGroundPhysical, const CVehicle* pVehicle)
{
	if (pGroundPhysical == NULL || pVehicle == NULL)
	{
		return false;
	}

	// If we're standing on the vehicle...
	if (pGroundPhysical == pVehicle)
	{
		return true;
	}
	// If we're standing on the same ground that the vehicle is lying on...
	else if (pGroundPhysical == pVehicle->m_pGroundPhysical)
	{
		return true;
	}
	// If we're standing on a vehicle that is lying on the vehicle...
	else if (pGroundPhysical->GetIsTypeVehicle() && static_cast<const CVehicle*>(pGroundPhysical)->m_pGroundPhysical.Get() == pVehicle)
	{
		return true;
	}


	return false;
}

//////////////////////////////////////////////////////////////////////////
float CTaskNMBehaviour::CalculateActivationForce(const CPed* pPed, eRagdollTriggerTypes nTrigger, const CEntity* pEntityResponsible, Vec3V_ConstPtr pHitPosition, Vec3V_ConstPtr pHitNormal, int nEntityComponent, bool UNUSED_PARAM(testRagdollInst))
//////////////////////////////////////////////////////////////////////////
{
	if (!AssertVerify(pPed))
	{
		return 0.0f;
	}

	float fForce = 0.0f;

	switch (nTrigger)
	{
	case RAGDOLL_TRIGGER_IMPACT_CAR:
	case RAGDOLL_TRIGGER_IMPACT_CAR_WARNING:
	case RAGDOLL_TRIGGER_IMPACT_CAR_CAPSULE:
		{
			if(!pPed->IsNetworkClone())
			{
				// force activation if the ped has been being pushed for a while
				float pushTime = pPed->GetTimeCapsulePushedByVehicle();
				if (pushTime>0.0f && (pushTime>(pPed->IsPlayer() ? sm_Tunables.m_MaxVehicleCapsulePushTimeForPlayerRagdollActivation : sm_Tunables.m_MaxVehicleCapsulePushTimeForRagdollActivation)))
				{
					nmEntityDebugf(pPed, "CalculateActivationForce: Forcing activation for continuous vehicle push");
					return 100000.0f;
				}
			}

			if (nmVerifyf(pEntityResponsible != NULL && pEntityResponsible->GetIsTypeVehicle(), "CTaskNMBehaviour::CalculateActivationForce: Invalid responsible entity"))
			{
				// calculate the 'PushValue' for the vehicle
				const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntityResponsible);
				// get velocity of other inst
				Vec3V vecPosition = pPed->GetTransform().GetPosition();
				Vector3 vVehicleVelocity = pVehicle->GetLocalSpeed(pHitPosition != NULL ? RCC_VECTOR3(*pHitPosition) : RCC_VECTOR3(vecPosition), true, nEntityComponent);
				
				float fVehicleVelocityMag2 = vVehicleVelocity.Mag2();
				if (fVehicleVelocityMag2 <= SMALL_FLOAT)
				{
					return 0.0f;
				}
				
				float minSpeedThreshold = GetMinSpeedThreshold(pPed, pPed->GetVelocity());

				// when hitting the ped with an articulated part, use the mass of the part
				float fVehicleMass = pVehicle->GetMass();

				if (nEntityComponent>0)
				{
					Vector3 vComVelocity = pVehicle->GetLocalSpeed(pHitPosition != NULL ? RCC_VECTOR3(*pHitPosition) : RCC_VECTOR3(vecPosition), true);
					float fPartMass = 0.0f;
					fragInst* pFragInst = pVehicle->GetFragInst();
					if (pFragInst && pFragInst->GetTypePhysics() && nEntityComponent<pFragInst->GetTypePhysics()->GetNumChildren())
					{
						fragPhysicsLOD* pPhysicsLod = pFragInst->GetTypePhysics();
						// Set include flags only for the dummy chassis
						int nGroup = pFragInst->GetTypePhysics()->GetChild(nEntityComponent)->GetOwnerGroupPointerIndex();
						if(nGroup > -1)
						{
							fragTypeGroup* pGroup = pPhysicsLod->GetGroup(nGroup);
							int iChild = pGroup->GetChildFragmentIndex();
							for(int k = 0; k < pGroup->GetNumChildren(); k++)
							{
								fPartMass+=pFragInst->GetTypePhysics()->GetChild(iChild+k)->GetUndamagedMass();

							}
						}
						else
						{
							fPartMass = pFragInst->GetTypePhysics()->GetChild(nEntityComponent)->GetUndamagedMass();
						}
					}
					
					float fComMag2 = vComVelocity.Mag2();
					if ((fComMag2 < square(minSpeedThreshold)) || ((fVehicleVelocityMag2 * square(fPartMass)) >= (fComMag2 * square(fVehicleMass))))
					{
						// the parts momentum is higher than the vehicles as a whole.
						// use the part mass instead.
						nmEntityDebugf(pPed,"CalculateActivationForce: Using part mass instead of vehicle mass, veh: %s(%p), component: %d, veh mass:%.3f, veh vel Mag:%.3f, part mass:%.3f, part vel mag:%.3f", pVehicle ? pVehicle->GetModelName() : "none", pVehicle, nEntityComponent, fVehicleMass, vComVelocity.Mag(), fPartMass, vVehicleVelocity.Mag());
						fVehicleMass = fPartMass;
					}
				}

				// Use the relative velocity to determine impact velocity.  The ped could be standing on the vehicle that is hitting them or
				// both the ped and vehicle could be on moving ground - need to account for these cases
				Vec3V vecVel(RCC_VEC3V(vVehicleVelocity));

				if(DoesGroundPhysicalMatch(pPed->GetGroundPhysical(), pVehicle) || (pPed->GetGroundPhysical() != pPed->GetLastValidGroundPhysical() && DoesGroundPhysicalMatch(pPed->GetLastValidGroundPhysical(), pVehicle) && 
					((fwTimer::GetTimeInMilliseconds() - pPed->GetLastValidGroundPhysicalTime()) < static_cast<u32>(CTaskNMBehaviour::sm_Tunables.m_DurationRampDownCapsulePushedByVehicle * 1000.0f))))
				{
					ScalarV fVehicleVelocityMag = ScalarVFromF32(Sqrtf(fVehicleVelocityMag2));
					Vec3V vVehicleDirection(InvScale(vecVel, fVehicleVelocityMag));
					// Never want to counter more than the velocity of the vehicle itself
					ScalarV fDotProduct = Clamp(Dot(pPed->GetGroundVelocityIntegrated(), vVehicleDirection), ScalarV(V_ZERO), fVehicleVelocityMag);

					vecVel = SubtractScaled(vecVel, vVehicleDirection, fDotProduct);
				}
				else
				{
					vecVel = Subtract(vecVel, VECTOR3_TO_VEC3V(pVehicle->GetReferenceFrameVelocity()));
				}

				vecVel.SetZf(vecVel.GetZf() * sm_Tunables.m_VehicleFallingSpeedWeight);

				Vec3V vecOffset = vecPosition - pVehicle->GetTransform().GetPosition();

				float fDotVel = 0.0f;
				if (pHitNormal != NULL)
				{
					fDotVel = Dot(*pHitNormal, vecVel).Getf();
				}
				else
				{
					//grcDebugDraw::Line(vecPosition, vecPosition + vecVel, Color_yellow, Color_red);

					// get the front and right vectors of the vehicle
					spdAABB otherBox;
					otherBox = pVehicle->GetLocalSpaceBoundBox(otherBox);
					Mat34V matrix = pVehicle->GetMatrix();

					// dot the local velocity against the two (normalized) vectors. Choose the smallest one to test against (assume hit axis will be the one more perpendicular to motion)
					ScalarV velDotRight = Dot(matrix.a(), vecVel);
					ScalarV velDotFront = Dot(matrix.b(), vecVel);
					nmDebugf3("Choosing collision axis: velDotRight=%.3f, velDotFront=%.3f", velDotRight.Getf(), velDotFront.Getf());

					if ((Abs(velDotRight)>Abs(velDotFront)).Getb())
					{
						Vec3V frontVector = matrix.b()*((otherBox.GetMax().GetY() - otherBox.GetMin().GetY())*ScalarV(V_HALF));
						ScalarV offsetDotAxis = Dot(vecOffset, frontVector);
						offsetDotAxis = Clamp(offsetDotAxis, ScalarV(V_NEGONE), ScalarV(V_ONE));
						vecOffset = vecPosition - (matrix.d()+(offsetDotAxis*frontVector));
					}
					else
					{
						Vec3V rightVector = matrix.a()*((otherBox.GetMax().GetX() - otherBox.GetMin().GetX())*ScalarV(V_HALF));
						ScalarV offsetDotAxis = Dot(vecOffset, rightVector);
						offsetDotAxis = Clamp(offsetDotAxis, ScalarV(V_NEGONE), ScalarV(V_ONE));
						vecOffset = vecPosition - (matrix.d()+(offsetDotAxis*rightVector));
					}

					//grcDebugDraw::Line(vecPosition, vecPosition + vecOffset, Color_green, Color_blue);
					vecOffset = NormalizeSafe(vecOffset, Vec3V(V_X_AXIS_WONE));
					fDotVel = Dot(vecOffset, vecVel).Getf();
				}

				// don't activate for warnings if the vehicles velocity is below a certain threshold.
				if (nTrigger==RAGDOLL_TRIGGER_IMPACT_CAR_WARNING && fDotVel<sm_Tunables.m_VehicleMinSpeedForWarningActivation)
				{
					return 0.0f;
				}

				// If the ped is attached to an object, we always want NM to trigger to prevent the car driving through the ped
				bool bIgnoreVelocityChecks = false;

				if (pPed->IsDead())
				{
					bIgnoreVelocityChecks = true;
					nmEntityDebugf(pPed, "CalculateActivationForce: Ignoring min velocity for dead ped");
				}

				nmEntityDebugf(pPed,"CalculateActivationForce: vecVel=(%.3f, %.3f, %.3f=%.3f), ped ground velocity=(%.3f, %.3f, %.3f=%.3f), fDotVel=%.3f, fMinSpeedThreshold=%.3f, bIgnoreVelocityChecks=%s, vehicle:%s(%p), part:%d, mass: %.3f, pHitNormal=(%.3f, %.3f, %.3f=%.3f)", 
					vecVel.GetXf(), vecVel.GetYf(), vecVel.GetZf(), Mag(vecVel).Getf(), pPed->GetGroundVelocityIntegrated().GetXf(), pPed->GetGroundVelocityIntegrated().GetYf(), pPed->GetGroundVelocityIntegrated().GetZf(), Mag(pPed->GetGroundVelocityIntegrated()).Getf(),
					fDotVel, minSpeedThreshold, bIgnoreVelocityChecks ? "true" : "false", pVehicle != NULL ? pVehicle->GetModelName() : "none", pVehicle, nEntityComponent, fVehicleMass, 
					pHitNormal != NULL ? pHitNormal->GetXf() : 0.0f, pHitNormal != NULL ? pHitNormal->GetYf() : 0.0f, pHitNormal != NULL ? pHitNormal->GetZf() : 0.0f, pHitNormal != NULL ? Mag(*pHitNormal).Getf() : 0.0f);

				if(fDotVel > minSpeedThreshold || bIgnoreVelocityChecks )
				{
					//player ped only, if you're outside the side of the bbox, don't activate.
					if(pPed->IsPlayer() && pHitNormal == NULL)
					{
						ScalarV vBBMaxAndRadius;
						vBBMaxAndRadius.Setf(pVehicle->GetPhysArch()->GetBound()->GetBoundingBoxMax().GetXf() + PED_HUMAN_RADIUS);
						if(IsGreaterThanAll(Abs(Dot(vecOffset, pVehicle->GetTransform().GetMatrix().GetCol0())), vBBMaxAndRadius))
						{
							nmEntityDebugf(pPed, "CalculateActivationForce: Blocking activation for player ped outside of vehicle bounding box");
							return 0.0f;
						}
					}

					float multiplier = 1.0f;
	
					switch (pVehicle->GetVehicleType())
					{
						case VEHICLE_TYPE_PLANE: multiplier=sm_Tunables.m_VehicleActivationForceMultiplierPlane; break;
						case VEHICLE_TYPE_QUADBIKE: multiplier=sm_Tunables.m_VehicleActivationForceMultiplierQuadBike; break;
						case VEHICLE_TYPE_HELI: multiplier=sm_Tunables.m_VehicleActivationForceMultiplierHeli; break;
						case VEHICLE_TYPE_BIKE: multiplier=sm_Tunables.m_VehicleActivationForceMultiplierBike; break;
						case VEHICLE_TYPE_BICYCLE: multiplier=sm_Tunables.m_VehicleActivationForceMultiplierBicycle; break;
						case VEHICLE_TYPE_BOAT: multiplier=sm_Tunables.m_VehicleActivationForceMultiplierBoat; break;
						case VEHICLE_TYPE_TRAIN: multiplier=sm_Tunables.m_VehicleActivationForceMultiplierTrain; break;
						default: multiplier=sm_Tunables.m_VehicleActivationForceMultiplierDefault; break;
					}

					if (pVehicle->pHandling->mFlags & MF_IS_RC)
					{
						multiplier = sm_Tunables.m_VehicleActivationForceMultiplierRC;
					}

					const CVehicleModelInfoRagdollActivation* pExtension = pVehicle->GetVehicleModelInfo()->GetExtension<CVehicleModelInfoRagdollActivation>();
					if(pExtension)
					{
						multiplier*=pExtension->GetRagdollActivationModifier(nEntityComponent);
					}

					float fRelativeVelocityMag = Mag(vecVel).Getf();

					// Cap the mass used in the below calculation if the vehicle speed is slow, in an effort to
					// stop massive vehicles like buses from knocking over peds even if the bus is moving very slowly.
					static float sfMaxMassWhenMovingSlowly = 1200.0f;
					static float sfMaxCapMassVel = 2.0f;
					fVehicleMass = Min(fVehicleMass, RampValueSafe(fRelativeVelocityMag, minSpeedThreshold, sfMaxCapMassVel, sfMaxMassWhenMovingSlowly, fVehicleMass));

					fForce = fDotVel*fVehicleMass*multiplier;

					// Don't activate due to a train that you're standing on if your relative velocity magnitude with the train is small
					if (pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
					{
						CEntity *groundEntity = pPed->GetGroundPhysical() ? pPed->GetGroundPhysical() : pPed->GetLastValidGroundPhysical();
						if (groundEntity && groundEntity->GetIsTypeVehicle() && ((CVehicle*) groundEntity)->GetVehicleType() == VEHICLE_TYPE_TRAIN)
						{
							static float magDiffLim = 8.0f;
							if (fRelativeVelocityMag < magDiffLim)
							{
								nmEntityDebugf(pPed, "CalculateActivationForce: Blocking activation from the train we're standing on");
								fForce = 0.0f;
							}
						}
					}

					nmEntityDebugf(pPed, "Vehicle hit activation test: dotVel=%.3f, vehicle mass=%.3f, ResultantForce=%.3f, multiplier: %.3f", fDotVel, fVehicleMass, fForce, multiplier);
			
					// because planes have unusually wide geometry the test above tends to
					// break down. force activation of the ragdoll when hit by a heavy plane.
					static float massIgnoreVelPlane = 500.0f;
					static float minDotVel = 0.0f;
					if (pVehicle->InheritsFromPlane() && pVehicle->GetMass()>massIgnoreVelPlane && fDotVel>minDotVel)
					{
						bIgnoreVelocityChecks = true;
					}

					// Override the force if velocity checks should be ignored.
					if( bIgnoreVelocityChecks)
					{
						fForce = 10000.0f;
					}
				}
				else
				{
					nmEntityDebugf(pPed, "CalculateActivationForce: Vehicle velocity too low: fDotVel=%.3f, minVel=%.3f", fDotVel, minSpeedThreshold);
				}
			}
		}
	default:
		{

		}
	}

	return fForce;
}

dev_float DIST_SCORE_ZOFFSET_MULT = 1.0f;
dev_float DIST_SCORE_OFFSCREEN_MULT = 4.0f;
dev_float DIST_SCORE_DEFAULT_DIST = 10.0f;
dev_float DIST_SCORE_SCALE_DIST = 50.0f;

dev_float SPEED_SCORE_ZOFFSET_MULT = 1.0f;
dev_float SPEED_SCORE_OFFSCREEN_MULT = 0.5f;
dev_float SPEED_SCORE_DEFAULT_SPEED = 5.0f;
dev_float SPEED_SCORE_SCALE_SPEED = 2.0f;

dev_float SCORE_SCRIPT_FORCE = 10.0f;
dev_float SCORE_PL_FORCE = 10.0f;
dev_float SCORE_PL_HIGH	= 1.0f;
dev_float SCORE_AMB_FORCE = 5.0f;
dev_float SCORE_AMB_HIGH = 1.0f;
dev_float SCORE_AMB_FIRE = 0.7f;
dev_float SCORE_AMB_SHOT = 0.5f;
dev_float SCORE_AMB_FALL = 0.3f;
dev_float SCORE_AMB_REACT = 0.2f;
dev_float SCORE_AMB_DEAD = 0.5f;
dev_float SCORE_AMB_DEAD_UNDER = 0.3f;  // The under refers to having at one time been under another ped
dev_float SCORE_DRAG = 5.0f;
dev_float SCORE_ZERO = 0.0f;

dev_float SCORES_THRESHOLD = 0.1f;

dev_float EXPLOSION_FORCE_SCORE_MODIFIER = 3.0f;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::CanUseRagdoll(CPed* pPed, eRagdollTriggerTypes nTrigger, CEntity* pEntityResponsible, float fPushValue)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	float fEventScore = WantsToRagdoll(pPed, nTrigger, pEntityResponsible, fPushValue);
	if(fEventScore > 0.0f)
    {
		return CanUseRagdoll(pPed, nTrigger, fEventScore * CalcRagdollMultiplierScore(pPed, nTrigger));
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CTaskNMBehaviour::WantsToRagdoll(CPed* pPed, eRagdollTriggerTypes nTrigger, CEntity* pEntityResponsible, float fPushValue)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (!AssertVerify(pPed))
	{
		return 0.0f;
	}

#if GTA_REPLAY
	//Dont allow ragdolls when replay is in control of the world - all the bones should be recorded.
	if(CReplayMgr::IsReplayInControlOfWorld())
		return 0.0f;
#endif

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	// Request to update the bounds in case a ragdoll activation occurs
	pPed->RequestRagdollBoundsUpdate();
#endif

	// Okay to ragdoll if you're already ragdolling
	if (pPed->GetUsingRagdoll() || pPed->GetRagdollState() >= RAGDOLL_STATE_PHYS_ACTIVATE)
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - PASSED. Ragdoll already active", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return SCORE_SCRIPT_FORCE;
	}

	// if this ped doesn't have a ragdoll loaded, or it's ragdoll doesn't have a NM agent setup
	if(pPed->GetRagdollInst()==NULL) // || pPed->GetRagdollInst()->GetType()->GetARTAssetID()==-1)
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ragdoll inst NULL", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return 0.0f;
	}

	// Reset the requested physics LOD as this is only ever set from this function
	pPed->GetRagdollInst()->RequestPhysicsLODChangeOnActivation(-1);

	// Don't allow the ped to enter nm if it has no endorphin rig.
	if (!pPed->GetRagdollInst()->GetTypePhysics()->GetBodyType())
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. No endorphin rig", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return 0.0f;
	}


	// *See fragInstNMGta::PrepareForActivation* fragInstNMGta::PrepareForActivation will no longer call UpdateFixedWaitingForCollision which 
	//   allows ragdolls to activate until the next physics update where it will get properly fixed. This should be
	//   no different than an active ragdoll losing collision and deactivating. The benefit is that we don't have to
	//   call UpdateFixedWaitingForCollision in CTaskNMBehaviour::CanUseRagdoll. 
	/*
	// If we aren't fixed, check for nearby collision
	if(!pPed->GetIsFixedUntilCollisionFlagSet())
	{
		pPed->UpdateFixedWaitingForCollision(true);
	}*/

	// if this ped's physics has been frozen
	if(pPed->GetIsAnyFixedFlagSet())
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Fixed flag set. Fixed:%s, Network:%s, Collision:%s", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], pPed->GetIsFixedFlagSet() ? "TRUE" : "FALSE", pPed->GetIsFixedByNetworkFlagSet() ? "TRUE" : "FALSE", pPed->GetIsFixedUntilCollisionFlagSet() ? "TRUE" : "FALSE");
		return 0.0f;
	}

	// if the game is being run with ragdolls turned off
	if(!CPhysics::CanUseRagdolls())
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. All ragdolls disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return 0.0f;
	}

	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_NeverRagdoll))
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. CPED_RESET_FLAG_NeverRagdoll", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return 0.0f;
	}

	if (pPed->GetRagdollInst()->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE) != 0)
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ragdoll set to never activate", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return 0.0f;
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceRagdollUponDeath) && nTrigger == RAGDOLL_TRIGGER_DIE)
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - PASSED. ForceRagdollUponDeath set for dying ped", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return SCORE_SCRIPT_FORCE;	
	}

	// Don't let collisions re-activate dead peds in the water
	if (nTrigger == RAGDOLL_TRIGGER_IMPACT_OBJECT && pPed->m_nFlags.bPossiblyTouchesWater && 
		pPed->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate && pPed->m_Buoyancy.GetWaterLevelOnSample(0)>0.0f)
	{
		if (pPed->GetRagdollState() < RAGDOLL_STATE_PHYS && pPed->IsDead())
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Dead ped in water", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}
	}

	float pedMass = pPed->GetCapsuleInfo() ? pPed->GetCapsuleInfo()->GetMass() : pPed->GetMass();

	// If you're "falling off" another ped that is much smaller than you
	if (nTrigger == RAGDOLL_TRIGGER_PHYSICAL_FALLOFF && pPed->GetGroundPhysical() && pPed->GetGroundPhysical() == pEntityResponsible && pEntityResponsible->GetIsTypePed())
	{
		float groundMass = pPed->GetGroundPhysical()->GetMass();
		static float scale = 2.0f;  // This is here temporarily until a bug where all animated peds have a mass of 25 gets fixed
		if (groundMass < pedMass * scale)
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Falling off smaller ped.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}
	}

	//Check if the ped is being electrocuted on a bike.
	bool bPedOnBike = pPed->GetMyVehicle() && pPed->GetVehiclePedInside() && (pPed->GetVehiclePedInside()->InheritsFromBike());

	bool bElectrocutedOnBike = bPedOnBike && (nTrigger == RAGDOLL_TRIGGER_ELECTRIC);
	bool bDyingOnBike = bPedOnBike && (nTrigger == RAGDOLL_TRIGGER_DIE);
	bool bIgnoreAttachmentCheck = false;

	const fwAttachmentEntityExtension *pedAttachExt = pPed->GetAttachmentExtension();

	// Peds getting in/out or inside vehicles can't use ragdolls if their collision is off, or we're blocking activation (bad pose)
	if (!bElectrocutedOnBike && !bDyingOnBike)
	{
		if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount)) //disabled for horses too for now
		{
			if (pedAttachExt)
			{
				// If we've been setup to detach on ragdoll or death, should be ok to use the ragdoll
				if (!pedAttachExt->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL) && !pedAttachExt->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_DEATH))
				{
					if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
					{
						const CTaskExitVehicle* pExitTask = static_cast<const CTaskExitVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
						if (pExitTask && pExitTask->GetState() < CTaskExitVehicle::State_ExitSeat)
						{
							nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ped exiting vehicle not yet started to animate out", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
							return 0.0f;
						}
						else
						{
							const CTaskExitVehicleSeat* pExitSeatTask = static_cast<const CTaskExitVehicleSeat*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
							if (pExitSeatTask)
							{
								const s32 iExitSeatState = pExitSeatTask->GetState();
								if (iExitSeatState == CTaskExitVehicleSeat::State_BeJacked ||
									iExitSeatState == CTaskExitVehicleSeat::State_StreamedBeJacked ||
									iExitSeatState == CTaskExitVehicleSeat::State_ExitSeat ||
									iExitSeatState == CTaskExitVehicleSeat::State_ExitToAim ||
									iExitSeatState == CTaskExitVehicleSeat::State_FleeExit ||
									iExitSeatState == CTaskExitVehicleSeat::State_JumpOutOfSeat ||
									iExitSeatState == CTaskExitVehicleSeat::State_StreamedExit)
								{
									TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MIN_TIME_IN_STATE_TO_ALLOW_RAGDOLL_DEAD_FALLOUT, 0.6f, 0.0f, 1.0f, 0.01f);
									TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MIN_TIME_IN_STATE_TO_ALLOW_RAGDOLL, 0.15f, 0.0f, 1.0f, 0.01f);
									const bool bDeadFallOut = iExitSeatState == CTaskExitVehicleSeat::State_BeJacked && pPed->ShouldBeDead();
									const bool bFleeExit = iExitSeatState == CTaskExitVehicleSeat::State_FleeExit;
									const float fMinTimeInState = bDeadFallOut ? MIN_TIME_IN_STATE_TO_ALLOW_RAGDOLL_DEAD_FALLOUT : (bFleeExit ? CTaskExitVehicleSeat::ms_Tunables.m_MinTimeInStateToAllowRagdollFleeExit : MIN_TIME_IN_STATE_TO_ALLOW_RAGDOLL);
									if (pExitSeatTask->GetTimeInState() < fMinTimeInState)
									{
										nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ped exiting vehicle not passed min time condition (%.2f/%.2f)", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], pExitSeatTask->GetTimeInState(), fMinTimeInState);
										return 0.0f;
									}
								}
							}
						}
					}

					const bool bIsInTurretSeat = pPed->GetIsInVehicle() ? pPed->GetMyVehicle()->GetSeatAnimationInfo(pPed)->IsTurretSeat() : false;
					
					if (pPed->GetPedResetFlag(CPED_RESET_FLAG_BlockRagdollActivationInVehicle))
					{
						nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ped blocking ragdoll in vehicle", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
						return 0.0f;
					}

					if (bIsInTurretSeat && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
					{
						bool bIsAATrailer = MI_TRAILER_TRAILERSMALL2.IsValid() && pPed->GetMyVehicle()->GetModelIndex() == MI_TRAILER_TRAILERSMALL2;
						if (!(pPed->GetMyVehicle()->GetSeatAnimationInfo(pPed)->IsStandingTurretSeat() || bIsAATrailer) && pPed->GetMyVehicle()->GetTransform().GetC().GetZf() >= 0.0f)
						{
							nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ped in upright turreted vehicle", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
							return 0.0f;
						}
					}
					else if (!pPed->IsCollisionEnabled())
					{
						nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ped in vehicle", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
						return 0.0f;
					}

					if (bIsInTurretSeat && nTrigger == RAGDOLL_TRIGGER_NETWORK)
					{
						// fix for B*2087299 - clone peds not detaching from a mounted turret. The more generic fix, to ignore the checks if the trigger is RAGDOLL_TRIGGER_NETWORK_CLONE
						// is probably safe but I'm not 100% certain so I'll minimise the fix for now
						bIgnoreAttachmentCheck = true;
					}
				}
			}
		}
	}

	// if this ped is attached to something, then don't ragdoll unless we're trying to grab hold of a vehicle we're attached too and that,
	// and unless we're running 'slung over shoulder'

	if(pedAttachExt && pedAttachExt->GetAttachParent()
	 && 	nTrigger!=RAGDOLL_TRIGGER_SLUNG_OVER_SHOULDER
	 && 	nTrigger!=RAGDOLL_TRIGGER_VEHICLE_GRAB 
	 && 	nTrigger!=RAGDOLL_TRIGGER_VEHICLE_FALLOUT
	 &&		nTrigger!=RAGDOLL_TRIGGER_VEHICLE_JUMPOUT
	 &&		nTrigger!=RAGDOLL_TRIGGER_SCRIPT_VEHICLE_FALLOUT 
	 && 	nTrigger!=RAGDOLL_TRIGGER_CLIMB_FAIL
	 &&     !bIgnoreAttachmentCheck
	 && 	!pedAttachExt->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL)
	 && 	!(pedAttachExt->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_DEATH) && pPed->IsInjured()))
	{
		// If we're attached to a vehicle, then allow activation in most cases when a good animation fallback isn't available
		CEntity* pAttachParent = static_cast<CEntity*>(pedAttachExt->GetAttachParent());
		if (pAttachParent->GetIsTypeVehicle() && 
			(nTrigger==RAGDOLL_TRIGGER_BULLET || nTrigger==RAGDOLL_TRIGGER_EXPLOSION || nTrigger==RAGDOLL_TRIGGER_WATERJET || nTrigger==RAGDOLL_TRIGGER_ELECTRIC || 
			nTrigger==RAGDOLL_TRIGGER_RUBBERBULLET || nTrigger==RAGDOLL_TRIGGER_IMPACT_CAR_WARNING || nTrigger==RAGDOLL_TRIGGER_IMPACT_CAR || nTrigger==RAGDOLL_TRIGGER_ACTIVATE_ON_COLLISION)
		   )
		{
			// TODO - Add the AI/vehicle logic to decide whether we do have a good animation fallback here.  If we do, disallow activation.
			static bool bAnimationFallbackExists = false;
			if (bAnimationFallbackExists)
			{
				nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ped attached to a vehicle and a good fallback animation can be used", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
				return 0.0f;
			}
		}
		else
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ped attached", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}
	}

	// if this ped's ragdoll has been locked
	if(pPed->GetRagdollState()==RAGDOLL_STATE_ANIM_LOCKED || (PHSIM->GetSleepEnabled() && pPed->GetRagdollState()==RAGDOLL_STATE_ANIM_PRESETUP))
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ragdoll state locked", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return 0.0f;
	}

    // if network code is disabling switching to ragdoll
    if(!NetworkInterface::CanUseRagdoll(pPed, nTrigger, pEntityResponsible, fPushValue))
    {
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Blocked by network code", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return 0.0f;
	}


	// config flags for disabling certain types of ragdoll activation
	if (!pPed->ShouldBeDead() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowBlockDeadPedRagdollActivation))
	{
		if ( (nTrigger==RAGDOLL_TRIGGER_IMPACT_PLAYER_PED) &&  (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact) || pPed->GetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromPlayerPedImpact) || pPed->GetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromPlayerPedImpactReset) || pPed->IsNetworkClone()))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Player ped impact disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( (nTrigger==RAGDOLL_TRIGGER_IMPACT_PLAYER_PED_RAGDOLL) &&  (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact) || pPed->GetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromPlayerRagdollImpact) || pPed->GetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromPlayerRagdollImpactReset) || pPed->IsNetworkClone()))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Player ragdoll impact disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( (nTrigger==RAGDOLL_TRIGGER_IMPACT_PED_RAGDOLL) &&  (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact) || pPed->GetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromAiRagdollImpact) || pPed->GetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromAiRagdollImpactReset) || pPed->IsNetworkClone()))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Ai ragdoll impact disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( (nTrigger==RAGDOLL_TRIGGER_IMPACT_CAR || nTrigger== RAGDOLL_TRIGGER_IMPACT_CAR_WARNING || nTrigger==RAGDOLL_TRIGGER_PHYSICAL_FALLOFF || nTrigger==RAGDOLL_TRIGGER_IMPACT_CAR_CAPSULE || nTrigger==RAGDOLL_TRIGGER_IMPACT_CAR_SIDE_SWIPE || nTrigger==RAGDOLL_TRIGGER_FORKLIFT_FORKS_FALLOFF) &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromVehicleImpact))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Vehicle impact disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( nTrigger==RAGDOLL_TRIGGER_BULLET &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromBulletImpact))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Bullet impact disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( nTrigger==RAGDOLL_TRIGGER_ELECTRIC &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromElectrocution))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Electric disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( nTrigger==RAGDOLL_TRIGGER_EXPLOSION &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Explosion disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( nTrigger==RAGDOLL_TRIGGER_FIRE &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromFire))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Fire disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( (nTrigger==RAGDOLL_TRIGGER_IMPACT_OBJECT || nTrigger==RAGDOLL_TRIGGER_IMPACT_OBJECT_COVER) &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromImpactObject))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Object impact disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( nTrigger==RAGDOLL_TRIGGER_MELEE &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromMelee))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Melee disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( nTrigger==RAGDOLL_TRIGGER_FALL &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromFalling))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Fall disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( nTrigger==RAGDOLL_TRIGGER_WATERJET &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromWaterJet))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Water jet impact disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}

		if ( nTrigger==RAGDOLL_TRIGGER_IN_WATER &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromDrowning))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. In water disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}
		if ( nTrigger==RAGDOLL_TRIGGER_RUBBERBULLET &&  pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromRubberBullet))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Rubber bullet impact disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}
		if ( nTrigger==RAGDOLL_TRIGGER_SMOKE_GRENADE && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromSmokeGrenade))
		{
			nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Smoke grenade disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			return 0.0f;
		}
	}

	if (nTrigger==RAGDOLL_TRIGGER_IMPACT_CAR_WARNING && !CTaskNMBrace::sm_Tunables.m_AllowWarningActivations)
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Vehicle warning disabled", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return 0.0f;
	}

	// Don't ragdoll if we're climb a ladder and another ped is underneath us.  CTaskClimbLadder will handle this itself.
	const bool bPedIsOnLadder = pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_CLIMB_LADDER);
	if(bPedIsOnLadder && pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypePed())
	{
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - FAILED. Climbing ladder", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return 0.0f;
	}
	// Don't ragdoll for collisions below the ped's minimum activation impulse.
	if (
		nTrigger == RAGDOLL_TRIGGER_IMPACT_CAR 
		|| nTrigger == RAGDOLL_TRIGGER_IMPACT_CAR_WARNING 
		|| nTrigger == RAGDOLL_TRIGGER_IMPACT_CAR_CAPSULE
		|| nTrigger == RAGDOLL_TRIGGER_IMPACT_PED_RAGDOLL 
		|| nTrigger == RAGDOLL_TRIGGER_IMPACT_PLAYER_PED_RAGDOLL 
		|| nTrigger == RAGDOLL_TRIGGER_IMPACT_PLAYER_PED 
		|| nTrigger == RAGDOLL_TRIGGER_IMPACT_OBJECT 
		|| nTrigger == RAGDOLL_TRIGGER_IMPACT_OBJECT_COVER)
	{
		// Always allow activation from car impacts if we're entering/exiting a vehicle
		if (nTrigger != RAGDOLL_TRIGGER_IMPACT_CAR || !pPed->GetIsAttachedInCar())
		{
			float pushLimit = pPed->GetPedModelInfo()->GetMinActivationImpulse();
			nmDebugf3("[%u] CanUseRagdoll:%s(%p) %s - initial pushLimit=%.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], pushLimit);

			Tunables::ActivationLimitModifiers& limits =  NetworkInterface::IsGameInProgress() ? sm_Tunables.m_MpActivationModifiers : sm_Tunables.m_SpActivationModifiers;

			if (nTrigger==RAGDOLL_TRIGGER_IMPACT_CAR || nTrigger==RAGDOLL_TRIGGER_IMPACT_CAR_WARNING || nTrigger==RAGDOLL_TRIGGER_IMPACT_CAR_CAPSULE)
			{
				if (fPushValue==0.0f)
				{
					// Calculate the push value for vehicles here rather than passing it in.
					// We need to do this as vehicle activations happen from a number of different
					// locations (e.g. ragdoll and capsule collisions and ai scanners)
					fPushValue = CalculateActivationForce(pPed, nTrigger, pEntityResponsible);
				}
				pushLimit *= limits.m_BumpedByCar;

				if (pEntityResponsible && pEntityResponsible->GetIsTypeVehicle() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceStandardBumpReactionThresholds) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_UsingMoverExtraction))
				{
					const CVehicle* pVehResponsible = static_cast<const CVehicle*>(pEntityResponsible);
					if (pVehResponsible->GetDriver() && pVehResponsible->GetDriver()->GetPedIntelligence()->IsFriendlyWith(*pPed))
					{
						// If this is exiting a vehicle or has just finished exiting a vehicle then don't apply the 'friendly' modifier since the ped could end up getting
						// stuck between the vehicle they are eixting and the vehicle hitting them causing issues in resolving the collision
						if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_ExitVehicleTaskFinishedThisFrame))
						{
							pushLimit *= limits.m_BumpedByCarFriendly;
						}
					}

					if (pPed->IsPlayer())
					{
						pushLimit*= limits.m_PlayerBumpedByCar;

						if(pVehResponsible->IsNetworkClone())
						{
							if(!pPed->IsNetworkClone())
							{
								pushLimit*=sm_Tunables.m_PlayerBumpedByCloneCarActivationModifier;
							}
							else
							{
								// Clone players must be harder to switch to ragdoll than their owners so the clone ragdolls when ragdoll tasks are cloned across
								pushLimit*=sm_Tunables.m_ClonePlayerBumpedByCarActivationModifier;
							}
						}
						else
						{
							if(pPed->IsNetworkClone())
							{
								// Clone players must be harder to switch to ragdoll than their owners so the clone ragdolls when ragdoll tasks are cloned across
								pushLimit*=sm_Tunables.m_ClonePlayerBumpedByCarActivationModifier;
							}
						}
					}
					else if (pPed->IsNetworkClone())
					{
						// Clone peds must be harder to switch to ragdoll than their owners so the clone ragdolls when ragdoll tasks are cloned across
						pushLimit*=sm_Tunables.m_ClonePedBumpedByCarActivationModifier;
					}

					CTaskEnterVehicle* pTask = static_cast<CTaskEnterVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));

					if (pTask && pTask->GetVehicle()==pVehResponsible)
					{
						// For peds who are trying to enter your vehicle, modify the activation limit based on movement speed (fast moving peds tend to just face plant
						// and they're generally more forgiving when their capsules are being pushed around anyway).
						Vector2 currentMbr;
						pPed->GetMotionData()->GetCurrentMoveBlendRatio(currentMbr);
						float speed = currentMbr.Mag();
						if (speed > MOVEBLENDRATIO_RUN)
						{
							pushLimit *= Lerp(speed-MOVEBLENDRATIO_RUN, limits.m_Running, limits.m_Sprinting);
						}
						else if (speed>MOVEBLENDRATIO_WALK)
						{
							pushLimit *= Lerp(speed-MOVEBLENDRATIO_WALK, limits.m_Walking, limits.m_Running);
						}
						else
						{
							pushLimit *= Lerp(speed, 1.0f, limits.m_Walking);
						}
					}
				}	

				pushLimit = Min(pushLimit, pPed->IsPlayer() ? limits.m_MaxPlayerActivationLimit : limits.m_MaxAiActivationLimit );
			}
			else if (nTrigger==RAGDOLL_TRIGGER_IMPACT_PLAYER_PED)
			{
				if (!pEntityResponsible || !pEntityResponsible->GetIsTypePed())
				{
					nmEntityDebugf(pPed, "CanUseRagdoll(%s): FAILED - No responsible ped provided.", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
					return 0.0f;
				}

				CPed* pOtherPed = static_cast<CPed*>(pEntityResponsible);
				CTaskMeleeActionResult* pTaskMeleeActionResult = pOtherPed->GetPedIntelligence()->GetTaskMeleeActionResult();
				if (pTaskMeleeActionResult != NULL && pTaskMeleeActionResult->GetTargetEntity() == pPed)
				{
					nmEntityDebugf(pPed, "CanUseRagdoll(%s): FAILED - Trying to melee this ped.", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
					return 0.0f;
				}

				// Decide activation force based on speed and other ped behaviour.
				Vec3V desiredVel = VECTOR3_TO_VEC3V(pOtherPed->GetAnimatedVelocity());
				desiredVel = pOtherPed->GetTransform().Transform3x3(desiredVel);
				const Vec3V& actualVel = RCC_VEC3V(pOtherPed->GetVelocity());

				Vec3V vecVel;

				if (MagSquared(desiredVel).Getf()>MagSquared(actualVel).Getf())
					vecVel = desiredVel;
				else
					vecVel = actualVel;

				// don't activate the ragdoll from running into stationary player peds.
				if (MagSquared(vecVel).Getf()<0.05f)
				{
					nmEntityDebugf(pPed, "CanUseRagdoll(%s): FAILED - Bumped into stationary player ped.", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
					return 0.0f;
				}
				else
				{
					if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ActivateRagdollFromMinorPlayerContact) && pOtherPed->IsPlayer())
					{
						nmEntityDebugf(pPed, "CanUseRagdoll(%s): Raising activation force for CPED_CONFIG_FLAG_ActivateRagdollFromMinorPlayerContact", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
						fPushValue = 100000.0f;
					}
					else
					{
						if (Mag(vecVel).Getf() < CTaskNMBehaviour::sm_Tunables.m_SpActivationModifiers.m_BumpedByPedMinVel)
						{	
							nmEntityDebugf(pPed, "CanUseRagdoll(%s): FAILED - Ped moving too slowly. fVel=%.3f, minVel=%.3f", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], Mag(vecVel).Getf(), CTaskNMBehaviour::sm_Tunables.m_SpActivationModifiers.m_BumpedByPedMinVel);
							return 0.0f;
						}

						// get the relative velocity, and use that to calculate the dotVel and activation force
						Vec3V relVel = vecVel -RCC_VEC3V( pPed->GetVelocity());
						Vec3V vecVelNorm = Normalize(relVel);
						const float fVel = Mag(relVel).Getf();
						Vec3V vecOffset = Normalize(pPed->GetMatrix().d() - pOtherPed->GetMatrix().d());
						ScalarV vDotVel = Dot(vecOffset, vecVelNorm);
						float fDotVel = vDotVel.Getf();

						bool bIgnoreMinDotVel = false;

						// activate the ragdoll if the ped is continuously running into the capsule (even if out of the range below)
						u32 startPushTime = pPed->GetTimeCapsuleFirstPushedByPlayerCapsule();
						if (startPushTime>0 && ((fwTimer::GetTimeInMilliseconds()-startPushTime)>(sm_Tunables.m_MaxPlayerCapsulePushTimeForRagdollActivation)))
						{
							nmEntityDebugf(pPed, "CanUseRagdoll(%s): Allowing activation out of bump angle range for continuous player push", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
							bIgnoreMinDotVel = true;
						}

						// don't activate peds when we're not running directly into them.
						if (fDotVel<CTaskNMBehaviour::sm_Tunables.m_SpActivationModifiers.m_BumpedByPedMinDotVel && !bIgnoreMinDotVel)
						{
							nmEntityDebugf(pPed, "CanUseRagdoll(%s): FAILED - Bump angle too shallow. fDotVel=%.3f, minDitVel=%.3f", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], fDotVel, CTaskNMBehaviour::sm_Tunables.m_SpActivationModifiers.m_BumpedByPedMinDotVel);
							return 0.0f;
						}
						else
						{
							if (CTaskNMBehaviour::sm_Tunables.m_ExcludePedBumpAngleFromPushCalculation)
							{
								fPushValue = fVel*pOtherPed->GetMass()*CTaskNMBehaviour::sm_Tunables.m_PedActivationForceMultiplier;
								nmEntityDebugf(pPed, "CanUseRagdoll:(%s) - push calculation fVel=%.3f, mass=%.3f, fPushValue=%.3f", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], fVel, pOtherPed->GetMass(), fPushValue);
							}
							else
							{
								fPushValue = fDotVel*fVel*pOtherPed->GetMass()*CTaskNMBehaviour::sm_Tunables.m_PedActivationForceMultiplier;
								nmEntityDebugf(pPed, "CanUseRagdoll:(%s) - push calculation fDotVel=%.3f, fVel=%.3f, mass=%.3f, fPushValue=%.3f", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], fDotVel, fVel, pOtherPed->GetMass(), fPushValue);
							}
						}
					}
				}
				pushLimit *= limits.m_BumpedByPed;
				nmEntityDebugf(pPed, "CanUseRagdoll:(%s) - BumpedByPlayer pushLimit=%.3f",  CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], pushLimit);
				
				if (pPed->GetCapsuleInfo() && pPed->GetCapsuleInfo()->IsQuadruped())
				{
					pushLimit *= limits.m_BumpedByPedIsQuadruped;
				}

				if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceStandardBumpReactionThresholds))
				{
					if (ShouldUseArmedBumpResistance(pPed))
					{
						pushLimit *= fragInstNMGta::m_ArmedPedBumpResistance;
						nmEntityDebugf(pPed, "CanUseRagdoll:(%s) - Armed ped resistance pushLimit=%.3f", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], pushLimit);
					}
					else if (pEntityResponsible && pEntityResponsible->GetIsTypePed())
					{
						const CPed* pPedResponsible = static_cast<const CPed*>(pEntityResponsible);
						if (pPedResponsible->GetPedIntelligence()->IsFriendlyWith(*pPed) && pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2()>0.0f)
						{
							pushLimit *= limits.m_BumpedByPedFriendly;
							nmEntityDebugf(pPed, "CanUseRagdoll:(%s) - friendly ped resistance pushLimit=%.3f", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], pushLimit);

						}
					}
				} 
			}
			else if (nTrigger==RAGDOLL_TRIGGER_IMPACT_PLAYER_PED_RAGDOLL || nTrigger==RAGDOLL_TRIGGER_IMPACT_PED_RAGDOLL)
			{
				if (!pEntityResponsible || !pEntityResponsible->GetIsTypePed())
				{
					nmEntityDebugf(pPed, "CanUseRagdoll(%s): FAILED - No responsible ped provided.", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
					return 0.0f;
				}
				CPed* pOtherPed = static_cast<CPed*>(pEntityResponsible);

				// Allow moving ragdolls to activate live non-player peds
				if (!pPed->IsPlayer() && !pPed->IsDead() && pOtherPed->GetRagdollInst()->GetARTAssetID() >= 0
					&& pPed->GetRagdollInst()->GetARTAssetID() >= 0)
				{

					if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ActivateRagdollFromMinorPlayerContact) && pOtherPed->IsPlayer())
					{
						nmEntityDebugf(pPed, "CanUseRagdoll(%s): Raising activation force for CPED_CONFIG_FLAG_ActivateRagdollFromMinorPlayerContact", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
						fPushValue = 100000.0f;
					}
					else
					{
						Vector3 otherLinVel(VEC3_ZERO);

						Assert(pOtherPed->GetCollider());
						if (pOtherPed->GetUsingRagdoll())
						{						
							phArticulatedBody *body = ((phArticulatedCollider*) pOtherPed->GetCollider())->GetBody();
							otherLinVel = VEC3V_TO_VECTOR3(body->GetLinearVelocity(0));
						}
						else
						{
							otherLinVel = pOtherPed->GetVelocity();
						}

						Vector3 vToPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition());
						vToPed.Normalize();
						//static float dotMin = 2.0f; // Considering a non-unit vel
						float dot = vToPed.Dot(otherLinVel);

						fPushValue = dot*pOtherPed->GetMass();
					}
				}
				else
				{
					nmEntityDebugf(pPed, "CanUseRagdoll(%s): FAILED - Bumped by ragdoll. IsPlayer:%s, IsDead:%s", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], pPed->IsPlayer() ? "true" : "false", pPed->IsDead() ? "true" : "false");
					return 0.0f;
				}

				bool bTreatOtherPedAsPlayer = pOtherPed->IsPlayer();
				// Treat the other ped as a player if they were recently damaged by the player
				static dev_float snMaxTimeSinceDamagedByPlayer = 250;
				for (int i = 0; i < pOtherPed->GetWeaponDamageInfoCount() && !bTreatOtherPedAsPlayer; i++)
				{
					const CPhysical::WeaponDamageInfo& weaponDamageInfo = pOtherPed->GetWeaponDamageInfo(i);
					if (weaponDamageInfo.pWeaponDamageEntity != NULL && (weaponDamageInfo.weaponDamageHash == WEAPONTYPE_RAMMEDBYVEHICLE || weaponDamageInfo.weaponDamageHash == WEAPONTYPE_RUNOVERBYVEHICLE) &&
						(fwTimer::GetTimeInMilliseconds() < (weaponDamageInfo.weaponDamageTime + snMaxTimeSinceDamagedByPlayer)))
					{
						switch (weaponDamageInfo.pWeaponDamageEntity->GetType())
						{
						case ENTITY_TYPE_PED:
							{
								bTreatOtherPedAsPlayer = static_cast<CPed*>(weaponDamageInfo.pWeaponDamageEntity.Get())->IsPlayer();
								nmEntityDebugf(pPed, "CanUseRagdoll(%s): Treating this as an impact by the player since the ragdolling ped was recently damaged by the player", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
								break;
							}
						case ENTITY_TYPE_VEHICLE:
							{
								if (static_cast<CVehicle*>(weaponDamageInfo.pWeaponDamageEntity.Get())->GetDriver() != NULL)
								{
									bTreatOtherPedAsPlayer = static_cast<CVehicle*>(weaponDamageInfo.pWeaponDamageEntity.Get())->GetDriver()->IsPlayer();
									nmEntityDebugf(pPed, "CanUseRagdoll(%s): Treating this as an impact by the player since the ragdolling ped was recently damaged by the player", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
								}
								break;
							}
						default:
							{
								break;
							}
						}
					}
				}

				pushLimit *= bTreatOtherPedAsPlayer ? limits.m_BumpedByPlayerRagdoll : limits.m_BumpedByPedRagdoll;
				nmDebugf3("[%u] CanUseRagdoll:%s(%p) %s - BumpedByPedRagdoll pushLimit=%.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], pushLimit);
			
				if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceStandardBumpReactionThresholds))
				{
					if (ShouldUseArmedBumpResistance(pPed))
					{
						pushLimit *= fragInstNMGta::m_ArmedPedBumpResistance;
						nmEntityDebugf(pPed, "CanUseRagdoll:(%s) - Armed ped resistance pushLimit=%.3f", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], pushLimit);
					}
					else if (pEntityResponsible && pEntityResponsible->GetIsTypePed())
					{
						const CPed* pPedResponsible = static_cast<const CPed*>(pEntityResponsible);
						if (pPedResponsible->GetPedIntelligence()->IsFriendlyWith(*pPed) && pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2()>0.0f)
						{
							pushLimit *= limits.m_BumpedByPedFriendly;
							nmEntityDebugf(pPed, "CanUseRagdoll:(%s) - friendly ped resistance pushLimit=%.3f", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], pushLimit);
						}
					}
				} 
			}
			else if (nTrigger==RAGDOLL_TRIGGER_IMPACT_OBJECT || nTrigger==RAGDOLL_TRIGGER_IMPACT_OBJECT_COVER)
			{

				Vec3V vecVel(V_ZERO);
				if (pEntityResponsible && pEntityResponsible->GetCurrentPhysicsInst() && pEntityResponsible->GetCurrentPhysicsInst()->IsInLevel())
				{
					phCollider *pOtherCollider = CPhysics::GetSimulator()->GetCollider(pEntityResponsible->GetCurrentPhysicsInst());
					if(pOtherCollider)
					{
						nmAssert(pPed->GetRagdollInst());
						vecVel = pOtherCollider->GetLocalVelocity(pPed->GetRagdollInst()->GetPosition().GetIntrin128());
					}
					else if (pEntityResponsible->GetIsPhysical())
					{
						vecVel = RCC_VEC3V(static_cast<CPhysical*>(pEntityResponsible)->GetVelocity());
					}
				}

				ScalarV velMag = Mag(vecVel);
				if (velMag.Getf()<sm_Tunables.m_ObjectMinSpeedForActivation)
				{
					nmEntityDebugf(pPed, "CanUseRagdoll:(%s) - FAILED. Object too slow (velMag:%.3f, threshold:%.3f)", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], velMag.Getf(), sm_Tunables.m_ObjectMinSpeedForActivation);
					return 0.0f;
				}

				pushLimit *= limits.m_BumpedByObject;
				
				// Allow object impacts to frozen corpses through - otherwise objects can be pushed through them
				if (pPed->IsDead())
				{
					fPushValue = 1000.0f;
				}
			}

			if (fPushValue < pushLimit)
			{
				nmEntityDebugf(pPed, "CanUseRagdoll:(%s) - FAILED. Impact too soft. PushValue:%.3f, Limit %.3f, Responsible entity: %s(%p)", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], fPushValue, pushLimit, pEntityResponsible?pEntityResponsible->GetModelName():"none", pEntityResponsible);
				return 0.0f;
			}
		}
		else
		{
			fPushValue = 1000.0f;
		}
	}

	return CalcRagdollEventScore(pPed, nTrigger, pEntityResponsible, fPushValue);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::CanUseRagdoll(CPed* pPed, eRagdollTriggerTypes nTrigger, float fScore)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Okay to ragdoll if you're already ragdolling
	if (pPed->GetUsingRagdoll() || pPed->GetRagdollState() >= RAGDOLL_STATE_PHYS_ACTIVATE)
	{
		if (nTrigger == RAGDOLL_TRIGGER_IMPACT_CAR)
		{
			// Up the minimum stiffness to ensure stability
			if (pPed->GetRagdollInst()->HasArticulatedBody())
			{
				static float minStiffnessCarImpact = 0.5f;
				pPed->GetRagdollInst()->GetArticulatedBody()->SetBodyMinimumStiffness(minStiffnessCarImpact);
				pPed->GetRagdollInst()->GetArticulatedBody()->SetStiffness(minStiffnessCarImpact);
			}
		}

#if __ASSERT
		pPed->m_CanUseRagdollSuccessfull=true;
#endif //__ASSERT
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - PASSED. Ragdoll already active", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return true;
	}

	// Not sure how safe this is as we're bypassing all the checks below that ensure we're not going to replace an on-screen ragdolling ped if there aren't any rage ragdolls left in the pool...
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceRagdollUponDeath) && nTrigger == RAGDOLL_TRIGGER_DIE)
	{
#if __ASSERT
		pPed->m_CanUseRagdollSuccessfull=true;
#endif //__ASSERT
		pPed->SetDesiredRagdollPool(kRagdollPoolRageRagdoll);
		nmDebugf2("[%u] CanUseRagdoll:%s(%p) %s - PASSED. ForceRagdollUponDeath set for dying ped", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
		return true;	
	}

	if(fScore > SCORES_THRESHOLD)
	{
		// Don't worry about NM agent capacity.  If it overflows a non-NM ragdoll can be used.
		int nPrimaryArtAssetId = pPed->GetRagdollInst()->GetType()->GetARTAssetID();
		eRagdollPool poolToUse = kRagdollPoolNmGameplay;

		bool bUseRageRagdoll = false;
		if (nPrimaryArtAssetId == -1 || nTrigger==RAGDOLL_TRIGGER_DIE)
		{
			bUseRageRagdoll = true;
		}

		bool bUsingNMAgent = false;

		// For now, only use rage ragdoll on dead peds and animals
		if (!bUseRageRagdoll)
		{
			// Get the total number of nm agents available from the asset manager
			s32 agentCount = FRAGNMASSETMGR->GetAgentCount(nPrimaryArtAssetId);
			if(agentCount>0)
			{
				// we always reserve 1 nm agent for the local player
				if (pPed->IsLocalPlayer() && RagdollPoolHasSpaceForPed(kRagdollPoolLocalPlayer, *pPed, true, fScore))
				{
					poolToUse = kRagdollPoolLocalPlayer;
					bUsingNMAgent = true;
				}
				else if (RagdollPoolHasSpaceForPed(kRagdollPoolNmGameplay, *pPed, true, fScore))	// gameplay reserved agents
				{
					poolToUse = kRagdollPoolNmGameplay;
					bUsingNMAgent = true;
				}
				else
				{
					// only early out here if pooling is enabled. If not, we'll fall back to a rage ragdoll (
					// This is to keep physics performance testscripts working).
					if (IsPoolingEnabled())
					{
						nmEntityDebugf(pPed, "CanUseRagdoll: %s - FAILED. No nm agents available.", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
						return false;
					}
				}
			}
			else
			{
				// only early out here if pooling is enabled. If not, we'll fall back to a rage ragdoll (
				// This is to keep physics performance test scripts working).
				if (IsPoolingEnabled())
				{
					nmEntityDebugf(pPed, "CanUseRagdoll: %s - FAILED. Nm agent pool exhausted!", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
					return false;
				}
			}
		}

		if (!bUsingNMAgent)
		{
			if (FRAGNMASSETMGR->GetNonNMRagdollCapacity() > FRAGNMASSETMGR->GetNumActiveNonNMRagdolls())
			{
				if (RagdollPoolHasSpaceForPed(kRagdollPoolRageRagdoll, *pPed, true, fScore))
				{
					poolToUse = kRagdollPoolRageRagdoll;
				}
				else
				{
					nmEntityDebugf(pPed, "CanUseRagdoll: %s - FAILED. No rage ragdolls available.", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
					return false;
				}
			}
			else
			{
				nmEntityDebugf(pPed, "CanUseRagdoll: %s - FAILED. Rage ragdoll pool exhausted!", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
				return false;
			}
		}

		Vector3 vLocalPlayerPos = CPlayerInfo::ms_cachedMainPlayerPos;

		const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
		if (pLocalPlayer && pLocalPlayer->GetPedResetFlag(CPED_RESET_FLAG_UsingDrone))
		{
			CNetGamePlayer* localPlayer = NetworkInterface::GetLocalPlayer();
			if (localPlayer)
			{
				vLocalPlayerPos = NetworkInterface::GetPlayerFocusPosition(*localPlayer);
			}
		}

		float distToPlayer = Dist(pPed->GetTransform().GetPosition(), RCC_VEC3V(vLocalPlayerPos)).Getf();

		int numActiveRagdolls = FRAGNMASSETMGR->GetNumActiveNonNMRagdolls() + 
			(FRAGNMASSETMGR->GetAgentCapacity(0) - FRAGNMASSETMGR->GetAgentCount(0));

		//! DMKH. Experimental ragdoll LOD'ing.
		CTaskDyingDead* pTaskDead = static_cast<CTaskDyingDead*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));
		bool bPedStartDead = (pTaskDead != NULL && pTaskDead->StartDead() && pTaskDead->GetState() == CTaskDyingDead::State_DeadAnimated && pTaskDead->GetSnapToGroundStage() == CTaskDyingDead::kSnapPoseReceived);

		if(NetworkInterface::IsGameInProgress() && !pPed->IsLocalPlayer() && (nTrigger == RAGDOLL_TRIGGER_DIE) && !bPedStartDead )
		{
			if(!pPed->GetIsVisibleInSomeViewportThisFrame())
			{
				static dev_float noRagDollDist = 7.5f;
				static dev_u32 nMaxRagdollsToActivate = 5;

				if(distToPlayer < noRagDollDist && (numActiveRagdolls < nMaxRagdollsToActivate) )
				{
					pPed->GetRagdollInst()->RequestPhysicsLODChangeOnActivation(fragInst::RAGDOLL_LOD_MEDIUM);
				}
				else
				{
					nmEntityDebugf(pPed, "CanUseRagdoll: %s - FAILED. MP Dead ped not in Viewport", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
					return false;
				}
			}
			else
			{
				// Let local player have the best experience. We should be stricter with other peds as these won't be out focus.
				if(pPed->GetWeaponDamageEntity() != CGameWorld::FindLocalPlayer())
				{
					static dev_float noRagDollDistAI = 10.0f;
					static dev_float noRagDollDistPlayer = 20.0f;

					float noRagDollDist = pPed->IsPlayer() ? noRagDollDistPlayer : noRagDollDistAI;

					// Be quite aggressive about when to ragdoll.
					if(distToPlayer > noRagDollDist)
					{
						nmEntityDebugf(pPed, "CanUseRagdoll: %s - FAILED. MP Dead ped not instigated by local player", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
						return false;
					}
					pPed->GetRagdollInst()->RequestPhysicsLODChangeOnActivation(fragInst::RAGDOLL_LOD_MEDIUM);
				}
			}
		}
		
		// If activating due to an explosion, set an appropriate ragdoll LOD
		if (nTrigger == RAGDOLL_TRIGGER_EXPLOSION && !pPed->IsPlayer())
		{
			static dev_float medLODDist = 7.0f;
			static dev_float lowLODDist = 14.0f;

			static int maxNumHighLODRagdolls = 10;
			if (numActiveRagdolls > maxNumHighLODRagdolls && !pPed->IsDead())
			{
				if (distToPlayer > lowLODDist)
				{
					pPed->GetRagdollInst()->RequestPhysicsLODChangeOnActivation(fragInst::RAGDOLL_LOD_LOW);
				}
				else if (distToPlayer > medLODDist)
				{	
					pPed->GetRagdollInst()->RequestPhysicsLODChangeOnActivation(fragInst::RAGDOLL_LOD_MEDIUM);
				}
			}
		}

		if (nTrigger == RAGDOLL_TRIGGER_IMPACT_CAR)
		{
			// Up the minimum stiffness once activated to ensure stability
			if (pPed->GetRagdollInst()->GetCached())
				pPed->GetRagdollInst()->GetCacheEntry()->SetIncreaseMinStiffnessOnActivation(true);
		}

		// Ensure that peds shot out of helicopters don't get switched to using low LOD ragdolls as falls using rage ragdolls don't look very good
		if(NetworkInterface::IsGameInProgress() && !pPed->IsLocalPlayer() &&
			(nTrigger == RAGDOLL_TRIGGER_EXPLOSION || nTrigger == RAGDOLL_TRIGGER_BULLET || nTrigger == RAGDOLL_TRIGGER_MELEE || nTrigger == RAGDOLL_TRIGGER_DIE) &&
			(!pPed->GetAttachParent() || pPed->GetAttachParent()->GetType() != ENTITY_TYPE_VEHICLE || !static_cast<CVehicle*>(pPed->GetAttachParent())->GetIsAircraft()))
		{
			if(!pPed->GetPedDrawHandler().GetVarData().GetIsHighLOD() || !pPed->GetIsOnScreen())
			{
				pPed->GetRagdollInst()->RequestPhysicsLODChangeOnActivation((s8)fragInst::RAGDOLL_LOD_LOW);
			}
		}

		int iForcedRagdollLOD;
		if(PARAM_forceRagdollLOD.Get(iForcedRagdollLOD))
		{
			pPed->GetRagdollInst()->RequestPhysicsLODChangeOnActivation((s8)iForcedRagdollLOD);
		}

#if __ASSERT
		pPed->m_CanUseRagdollSuccessfull=true;
#endif //__ASSERT
		pPed->SetDesiredRagdollPool(poolToUse);
		nmEntityDebugf(pPed, "CanUseRagdoll: %s - PASSED. All tests passed. Ragdoll score = %.3f", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], fScore);
		return true;
	}

	// If we get here and haven't decided whether or not a particular situation should result in a ped
	// ragdolling, default to "no".
	nmEntityDebugf(pPed, "CanUseRagdoll: %s - FAILED. Default (score too low %.3f < %.3f)", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger], fScore, SCORES_THRESHOLD);
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::ShouldUseArmedBumpResistance(const CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
	if (pPedIntelligence && pPedIntelligence->GetQueriableInterface())
	{
		if (
			pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE)
			|| pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT)
			|| pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GUN)
			)
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClipPoseHelper& CTaskNMBehaviour::GetClipPoseHelper() 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// ClipPose helper class is in CTaskNMControl which must be the behaviour task's parent. Use a pointer to the
	// control task to get at the helper.
	taskAssertf(dynamic_cast<CTaskNMControl*>(GetParent()), "A natural motion behaviour task must be an immediate child of CTaskNMControl.");
	CTaskNMControl *pControlTask = smart_cast<CTaskNMControl*>(GetParent());

	return pControlTask->GetClipPoseHelper();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::SetClipPoseHelperClip(const fwMvClipSetId &clipSetId, const fwMvClipId &nClipId)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_clipPoseSetId = clipSetId;
	m_clipPoseId = nClipId;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::SetClipPoseHelperClip(s32 nClipDict, u32 nClip)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_iClipDictIndex = nClipDict;
	m_iClipHash = nClip;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::GetClipPoseHelperClip(fwMvClipSetId &clipSetId, fwMvClipId& nClipId)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	clipSetId = m_clipPoseSetId;
	nClipId = m_clipPoseId;

	if(m_clipPoseSetId == CLIP_SET_ID_INVALID || m_clipPoseId == CLIP_ID_INVALID)
		return false;
	else
		return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::GetClipPoseHelperClip(s32 &nClipDict, u32 &nClip)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	nClipDict = m_iClipDictIndex;
	nClip = m_iClipHash;
	
	if(m_iClipDictIndex >= 0 )
	{
		const char* pDictName = fwAnimManager::GetName(strLocalIndex(m_iClipDictIndex));
		
		if (pDictName)
		{
			const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_iClipDictIndex, m_iClipHash); 
			
			if(pClip)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	return false; 
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CClonedNMControlInfo ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClonedNMControlInfo::CClonedNMControlInfo(s32 nmTaskType, u32 nControlFlags, bool bHasFallen, u16 randomSeed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: m_nNMTaskType(-1)
, m_nControlFlags(nControlFlags & ((1<<CTaskNMControl::NUM_SYNCED_CONTROL_FLAGS)-1)) // remove any control flags we don't need
, m_bHasFallen(bHasFallen)
, m_randomSeed(randomSeed)
{
	if (nmTaskType != -1)
	{
		Assertf(nmTaskType>=CTaskTypes::TASK_NM_RELAX && nmTaskType<CTaskTypes::TASK_RAGDOLL_LAST, "Unrecognised NM task %d in CClonedNMControlInfo", nmTaskType);
		m_nNMTaskType = (s8)(nmTaskType-CTaskTypes::TASK_NM_RELAX);
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClonedNMControlInfo::CClonedNMControlInfo()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: m_nNMTaskType(-1)
, m_nControlFlags(0)
, m_bHasFallen(false)
, m_randomSeed(0)
{

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone *CClonedNMControlInfo::CreateCloneFSMTask()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// min and max time will be got from the task info of the nm task running under the control info task
	return rage_new CTaskNMControl(0, 0, NULL, m_nControlFlags);
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMControl methods: ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CTaskNMControl::ms_bTeeterEnabled = true;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMControl::CTaskNMControl(u32 nMinTime, u32 nMaxTime, aiTask* pForceFirstSubTask, u32 nNMControlFlags, float fDamageTaken)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: m_nMinTime(nMinTime),
m_nMaxTime(nMaxTime),
m_nStartTime(0),
m_nFlags(nNMControlFlags),
m_nFeedbackFlags(ALL_FEEDBACK_FLAGS_CLEAR),
m_ForceNextSubTask(pForceFirstSubTask),
m_fDamageTaken(fDamageTaken),
m_currentTask(-1),
m_nextTask(-1),
m_waitForNextTask(false),
m_bHasAborted(false),
m_bGrabbed2Handed(false),
m_randomSeed(0),
m_bCloneTaskFinished(false),
m_alwaysAllowControlPassing(false),
m_DriveToGetupMatchedBlendOutSet(NMBS_INVALID),
m_pDriveToGetupMatchedBlendOutPoseItem(NULL),
m_nDriveToGetupMatchTimer(0),
m_DriveToGetupTarget(),
m_pDriveToGetupMoveTask(NULL)
{
	// TODO RA: For now, we have to include the possibility that a blend from NM task might still exist.
	ASSERT_ONLY(CTask *forceNextSubTask = (CTask *) m_ForceNextSubTask.GetTask();)
	taskAssert(IsValidNMControlSubTask(forceNextSubTask));

	if(pForceFirstSubTask)
	{
		taskAssertf(pForceFirstSubTask->GetTaskType()>=CTaskTypes::TASK_NM_RELAX && pForceFirstSubTask->GetTaskType()<CTaskTypes::TASK_RAGDOLL_LAST, "Non NM task %d passed to CTaskNMControl", pForceFirstSubTask->GetTaskType());
		if (((CTask*)pForceFirstSubTask)->IsNMBehaviourTask())
		{
			CTaskNMBehaviour* pTask = smart_cast<CTaskNMBehaviour*>(pForceFirstSubTask);
			if (pTask->HasBalanceFailed())
			{
				m_nFeedbackFlags |= BALANCE_FAILURE;		
			}
		}
	}

	for(int pedHand = 0; pedHand < CMovePed::kNumHands; pedHand++)
	{
		m_CurrentHandPoses[pedHand] = HAND_POSE_NONE;
	}

    ClearGetGrabParametersCallback();
	SetInternalTaskType(CTaskTypes::TASK_NM_CONTROL);

	NET_DEBUG_TTY("Created NM control task 0x%p\n", this); 
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMControl::~CTaskNMControl()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	NET_DEBUG_TTY("Delete NM control task 0x%p\n", this);
    ClearGetGrabParametersCallback();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
aiTask* CTaskNMControl::Copy() const
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	aiTask* pClonedTask = m_ForceNextSubTask.GetTask()->Copy();
	taskAssert(pClonedTask);
	CTaskNMControl *newTask = rage_new CTaskNMControl(m_nMinTime, m_nMaxTime, pClonedTask, m_nFlags, m_fDamageTaken);

	// Pass the anim pose type
	newTask->m_clipPoseHelper.SetAnimPoseType(m_clipPoseHelper.GetAnimPoseType());

	return newTask;
}
#if !__FINAL
atString CTaskNMControl::GetName() const
{
	atString ret = CTask::GetName();

	if (m_ForceNextSubTask.GetTask())
	{
		atVarString str(" - ForcedNMTask:%s", m_ForceNextSubTask.GetTask()->GetName().c_str());
		ret += str;
	}

	return ret;
}
#endif // !__FINAL

#if !__NO_OUTPUT
void CTaskNMControl::Debug() const
{
	aiTask::Debug();
}
#endif //!__NO_OUTPUT

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMControl::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// handle the case where a ped has just migrated locally while running a CTaskNMControl task and has not updated the task yet before processing events
	if (NetworkInterface::IsGameInProgress() && iPriority != ABORT_PRIORITY_IMMEDIATE && !GetSubTask() && !GetIsFlagSet(aiTaskFlags::HandleCloneSwapToSameTaskType) && !GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		return false;
	}

	return CTaskFSMClone::ShouldAbort(iPriority, pEvent);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMControl::MakeAbortable( const AbortPriority iPriority, const aiEvent* pEvent)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (GetIsFlagSet(aiTaskFlags::HandleCloneSwapToSameTaskType))
	{
		SetDontSwitchToAnimatedOnAbort(true);
	}

	if (pEvent && ((const CEvent*)pEvent)->RequiresAbortForRagdoll())
	{
		m_nFlags |= ABORTING_FOR_RAGDOLL_EVENT;
	}

	if (!CTaskFSMClone::MakeAbortable(iPriority, pEvent))
	{
		m_nFlags &= ~ABORTING_FOR_RAGDOLL_EVENT;

		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMControl::CleanUp()
//////////////////////////////////////////////////////////////////////////
{
	// Make sure we don't accidentally leave the ragdoll on
	// this can occur if a state transition happens higher up
	// the ai task tree when NM control is being run as a subtask.
	CPed* pPed = GetPed();

	NET_DEBUG_TTY("Cleanup NM control task (Seq: %d, task: 0x%p)\n", GetNetSequenceID(), this);

	// Remote peds (when running a shot task locally) haven't had their health updated yet but their death state should be up-to-date.
	bool bPedShouldBeDead = pPed->ShouldBeDead() || (pPed->IsNetworkClone() && pPed->GetIsDeadOrDying());

	// a clone player running a local death response task is also treated as dead (he may still have health > 0)
	if (!bPedShouldBeDead && pPed->IsNetworkClone() && pPed->IsAPlayerPed())
	{
		bPedShouldBeDead = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->IsRunningDeathResponseTask();
	}

	nmDebugf2("[%u] NM Control-cleanup:%s(%p)", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
	if (!(m_nFlags & ABORTING_FOR_RAGDOLL_EVENT) && !(m_nFlags & CLONE_LOCAL_SWITCH) && !GetIsFlagSet(aiTaskFlags::HandleCloneSwapToSameTaskType))
	{
		NET_DEBUG_TTY("Cleanup unhandled ragdoll\n");
		CleanupUnhandledRagdoll(pPed);
	}

	if (pPed->GetMovePed().GetState()==CMovePed::kStateStaticFrame && !bPedShouldBeDead)
	{
		NET_DEBUG_TTY("Non dead ped left in ragdoll frame. Switching to animation\n");
		nmTaskDebugf(this, "CTaskNMControl::Cleanup - Non dead ped left in ragdoll frame. Switching to animation!");
		pPed->GetMovePed().SwitchToAnimated(0.0f, false);
	}

	if ((GetFlags() & EQUIPPED_WEAPON_OBJECT_VISIBLE) != 0 && !pPed->GetUsingRagdoll() && pPed->IsPlayer() && !pPed->IsNetworkClone() && !pPed->GetIsDeadOrDying() && !pPed->IsInjured())
	{
		CPedWeaponManager* pPedWeaponManager = pPed->GetWeaponManager();
		if (pPedWeaponManager != NULL)
		{
			CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
			if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
			{
				static_cast<CTaskHumanLocomotion*>(pTask)->SetInstantBlendNextWeaponClipSet(true);
			}

			CObject* pEquippedWeaponObject = pPedWeaponManager->GetEquippedWeaponObject();
			if (pEquippedWeaponObject != NULL)
			{
				if ((GetFlags() & EQUIPPED_WEAPON_OBJECT_VISIBLE) != 0)
				{
					pEquippedWeaponObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
				}
			}
			
			ClearFlag(EQUIPPED_WEAPON_OBJECT_VISIBLE);
		}
	}

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PreferInjuredGetup, false);

	if (m_pDriveToGetupMoveTask)
	{
		delete m_pDriveToGetupMoveTask;
		m_pDriveToGetupMoveTask = NULL;
	}

}

//////////////////////////////////////////////////////////////////////////
void CTaskNMControl::SetCurrentHandPose(eNMHandPose pose, CMovePed::ePedHand pedHand, float blendDuration)
//////////////////////////////////////////////////////////////////////////
{
	if (pose !=m_CurrentHandPoses[pedHand])
	{
		CPed* pPed = GetPed();
		const crClip* pClip = NULL;
		taskAssert(pPed);
		CMovePed& move = pPed->GetMovePed();

		//get the hand pose clip
		switch(pose)
		{
		case HAND_POSE_LOOSE:
			{
				pClip = fwAnimManager::GetClipIfExistsBySetId(CLIP_SET_HAND_POSES, CLIP_HAND_POSE_NATURAL);
			}
			break;
		case HAND_POSE_GRAB:
			{
				pClip = fwAnimManager::GetClipIfExistsBySetId(CLIP_SET_HAND_POSES, CLIP_HAND_POSE_GRAB);
			}
			break;
		case HAND_POSE_BRACED:
			{
				pClip = fwAnimManager::GetClipIfExistsBySetId(CLIP_SET_HAND_POSES, CLIP_HAND_POSE_FLAT_FLOOR);
			}
			break;
		case HAND_POSE_HOLD_WEAPON:
			{
				// use weapon holding idle for now
				fwMvClipSetId upperBodyClipSet = pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetAppropriateWeaponClipSetId(pPed);

				if (pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsMeleeFist())
				{
					fwMvClipId CLIP_MELEE_FIST_GRIP("GRIP_IDLE",0x3ec63b58);
					pClip = fwAnimManager::GetClipIfExistsBySetId(upperBodyClipSet, CLIP_MELEE_FIST_GRIP);
				}
				else
				{

					if (pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsMeleeFist())
					{
						fwMvClipId CLIP_MELEE_FIST_GRIP("GRIP_IDLE",0x3ec63b58);
						pClip = fwAnimManager::GetClipIfExistsBySetId(upperBodyClipSet, CLIP_MELEE_FIST_GRIP);
					}
					else
					{
						pClip = fwAnimManager::GetClipIfExistsBySetId(upperBodyClipSet, CLIP_AIM_MED_LOOP);
					}
				}
			}
			break;
		case HAND_POSE_FLAIL:
			{
				pClip = fwAnimManager::GetClipIfExistsBySetId(CLIP_SET_HAND_POSES, CLIP_HAND_POSE_FLAIL);
			}
			break;
		case HAND_POSE_IMPACT:
			{
				pClip = fwAnimManager::GetClipIfExistsBySetId(CLIP_SET_HAND_POSES, CLIP_HAND_POSE_IMPACT);
			}
			break;
		default:
			// do nothing
			break;
		}
		
		move.PlayHandAnimation(pClip, blendDuration, pedHand);

		m_CurrentHandPoses[pedHand] = pose;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMControl::OverridesNetworkBlender(CPed *pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    bool overrideNetworkBlender = false;

    if(GetSubTask())
    {
        s32 taskType = GetSubTask()->GetTaskType();

        overrideNetworkBlender = (taskType == CTaskTypes::TASK_BLEND_FROM_NM);

		if(pPed)
		{
			CTaskParachute* taskParachute	= static_cast<CTaskParachute*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
			if(taskType == CTaskTypes::TASK_BLEND_FROM_NM && taskParachute && taskParachute->GetState() == CTaskParachute::State_CrashLand)
			{
				overrideNetworkBlender = false;
			}
		}

        if(IsRunningLocally())
        {
            switch(taskType)
            {
            case CTaskTypes::TASK_NM_SHOT:
            case CTaskTypes::TASK_NM_EXPLOSION:
                overrideNetworkBlender = true;
                break;
            default:
                break;
            }
        }

		if(taskType == CTaskTypes::TASK_NM_HIGH_FALL)
		{
            overrideNetworkBlender = false;
		}
    }

	return overrideNetworkBlender;
}

bool CTaskNMControl::OverridesNetworkHeadingBlender(CPed* pPed)
{
	if(pPed)
	{
		if(GetSubTask())
		{
			s32 taskType = GetSubTask()->GetTaskType();	

			if(taskType == CTaskTypes::TASK_BLEND_FROM_NM)
			{
				CTaskParachute* taskParachute	= static_cast<CTaskParachute*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
				if(taskParachute && taskParachute->GetState() == CTaskParachute::State_CrashLand)
				{
					return true;
				}
			}
			else
			{
				if(!pPed->GetUsingRagdoll())
				{
					return true;
				}
			}
		}
	}	

	return CTaskFSMClone::OverridesNetworkHeadingBlender(pPed);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CreateQueriableState
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo *CTaskNMControl::CreateQueriableState() const
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	s32 taskType = -1;

	if (GetSubTask())
	{
		taskType = GetSubTask()->GetTaskType();
	}

	bool bBalanceFailure = IsFeedbackFlagSet(BALANCE_FAILURE);

	u16 randomSeed = 0;
	if (GetPed() && GetPed()->GetRagdollInst())
		randomSeed = GetPed()->GetRagdollInst()->GetRandomSeed();

	return rage_new CClonedNMControlInfo(taskType, m_nFlags, bBalanceFailure, randomSeed);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ReadQueriableState
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_NM_CONTROL );
	CClonedNMControlInfo *controlInfo = static_cast<CClonedNMControlInfo*>(pTaskInfo);

	if (controlInfo->GetNMTaskType() == -1)
	{
		NET_DEBUG_TTY("== 0x%p: GOT UPDATE at frame %u: NO NEXT TASK ==\n", this, fwTimer::GetFrameCount());
	}
	else
	{
		NET_DEBUG_TTY("== 0x%p: GOT UPDATE at frame %u: %s ==\n", this, fwTimer::GetFrameCount(), TASKCLASSINFOMGR.GetTaskName(controlInfo->GetNMTaskType()));
	}

	if (controlInfo->GetNMTaskType() != -1)
	{
		m_nextTask = controlInfo->GetNMTaskType();
	}
	else
	{
		m_nextTask = -1;
	}

	m_randomSeed = controlInfo->GetRandomSeed();

	// if the master ped has fallen and the clone hasn't, then force him to relax so that he does also
	if (controlInfo->GetHasFallen() && !IsFeedbackFlagSet(BALANCE_FAILURE) && m_nextTask != CTaskTypes::TASK_BLEND_FROM_NM)
	{
		NET_DEBUG_TTY("**Balance lost - force fall over**\n");
		m_nFlags |= CTaskNMControl::FORCE_FALL_OVER;
	}

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// OnCloneTaskNoLongerRunningOnOwner
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::OnCloneTaskNoLongerRunningOnOwner()
{
	NET_DEBUG_TTY("** clone task no longer running on owner **\n");
	m_nextTask = CTaskTypes::TASK_BLEND_FROM_NM;
	m_bCloneTaskFinished = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMControl::IsInScope(const CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(m_nextTask == CTaskTypes::TASK_NM_HIGH_FALL)
	{
		return true;
	}

	if(GetSubTask())
	{
		if(GetSubTask()->GetTaskType() == CTaskTypes::TASK_NM_HIGH_FALL)
		{
			return true;
		}
	}

	return CTaskFSMClone::IsInScope(pPed);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UpdateClonedFSM
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMControl::FSM_Return CTaskNMControl::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(iEvent == OnUpdate)
	{
		if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_CUFFED))
		{
			NET_DEBUG_TTY("ABORT - cuffed task running\n");

			SetState(State_Finish);
			return FSM_Continue;
		}

		// when the owner transitions from dying to dead they start a new CTaskNMControl task with a CTaskNMRelax subtask
		// we don't want the clone to abort their currently running subtask in this case as they should transition naturally
		// to a relax task themselves when their currently running subtask finishes
		if (!IsRunningLocally())
		{
			// quit this NM task immediately if we have another one with a higher sequence
			CTaskInfo* pInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_NM_CONTROL, PED_TASK_PRIORITY_MAX, false);

			if (pInfo && pInfo->GetNetSequenceID() > GetNetSequenceID())
			{
				NET_DEBUG_TTY("ABORT - another TASK_NM_CONTROL running with a higher sequence (%d)\n", pInfo->GetNetSequenceID());
				SetDontSwitchToAnimatedOnAbort(true);
				return FSM_Quit;
			}
		}
	    // locally running clone tasks should only persist temporarily while we wait for an update confirming that the ped has ragdolled on the other side, if not we need to abort
		else if (!pPed->GetIsDeadOrDying() && GetTimeRunning() > 1.0f)
	    {
		    // don't abort if the parent task is also handling ragdoll, in this case the ped will remain in ragdoll when the task quits
		    if (!pPed->GetPedIntelligence()->GetLowestLevelRagdollTask(pPed))
		    {
				NET_DEBUG_TTY("ABORT - TASK_NM_CONTROL running locally for too long\n");
			    return FSM_Quit;
		    }
		    else
		    {
			    CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());
			    // quit this NM task if the master ped is not ragdolling
			    if (pPedObj && !pPedObj->IsUsingRagdoll())
			    {
					NET_DEBUG_TTY("ABORT - TASK_NM_CONTROL running locally for too long without master ragdolling\n");
				    return FSM_Quit;
			    }
		    }
	    }
	}

	// if the clone task has been locally created for a local hit reaction, just run as a normal task 
	if (IsRunningLocally())
	{
		return UpdateFSM(iState, iEvent);
	}
	else
	{
		// ignore Z blending for most NM subtasks, his avoids the clone floating in the air if the pelvis of the master ped is above the ground 
		if (GetSubTask() && pPed->GetUsingRagdoll())
		{
			bool disableZBlending = true;

			switch (GetSubTask()->GetTaskType())
			{
			case CTaskTypes::TASK_NM_BRACE:
			case CTaskTypes::TASK_NM_EXPLOSION:
			case CTaskTypes::TASK_NM_THROUGH_WINDSCREEN:
			case CTaskTypes::TASK_RAGE_RAGDOLL:
				disableZBlending = false;
				break;
			case CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE:
				disableZBlending = static_cast<CTaskNMJumpRollFromRoadVehicle*>(GetSubTask())->IsZBlendingDisabled();
			default:
				break;
			}

			if (disableZBlending)
			{
				NetworkInterface::DisableNetworkZBlendingForRagdolls(*pPed);
			}
		}

		FSM_Begin
			FSM_State(State_Start)
				FSM_OnUpdate
					return Start_OnUpdateClone(pPed);

			FSM_State(State_ControllingTask)
				FSM_OnEnter
					ControllingTask_OnEnter(pPed);
				FSM_OnUpdate
					return ControllingTask_OnUpdateClone(pPed);

			FSM_State(State_DecidingOnNextTask)
				FSM_OnEnter
					DecidingOnNextTask_OnEnterClone(pPed);
				FSM_OnUpdate
					return DecidingOnNextTask_OnUpdateClone(pPed);

			FSM_State(State_Finish)
				FSM_OnEnter
					Finish_OnEnterClone(pPed);
				FSM_OnUpdate
					return FSM_Quit;
		FSM_End
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone*	CTaskNMControl::CreateTaskForClonePed(CPed *pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	aiTask* forceNextSubTask = m_ForceNextSubTask.GetTask() ? m_ForceNextSubTask.GetTask()->Copy() : NULL;

	// this task is not networked if running as a subtask of animated attach
	if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ANIMATED_ATTACH)
	{
		return NULL;
	}

	// prevent the control NM subtask from switching the ped to animated when it aborts
	SetDontSwitchToAnimatedOnAbort(true);

	u32 flagsForNewTask = m_nFlags;

	m_nFlags |= CLONE_LOCAL_SWITCH;

	if (!forceNextSubTask && GetSubTask())
	{
		if (GetSubTask()->IsClonedFSMTask())
		{
			forceNextSubTask = static_cast<CTaskFSMClone*>(GetSubTask())->CreateTaskForClonePed(pPed);
		}
		else
		{
			forceNextSubTask = GetSubTask()->Copy();
		}
		
		flagsForNewTask |= CTaskNMControl::ALREADY_RUNNING;
	}

	CTaskNMControl* pNewTask = rage_new CTaskNMControl(m_nMinTime, m_nMaxTime, forceNextSubTask, flagsForNewTask);

	NET_DEBUG_TTY("@@@@@@@@@@@@@@@@ %s MIGRATED REMOTELY. New task: 0x%p @@@@@@@@@@@@@@@@\n", GetPed()->GetNetworkObject()->GetLogName(), pNewTask); 
	
	if (forceNextSubTask)
	{
		NOTFINAL_ONLY(NET_DEBUG_TTY("Forcing sub task to %s", forceNextSubTask->GetTaskName());)
	}
	else if (m_nextTask != -1)
	{
		NET_DEBUG_TTY("** no subtask running. Next task set to %s **", TASKCLASSINFOMGR.GetTaskName(m_nextTask));
		pNewTask->m_nextTask = m_nextTask;
	}
	else
	{
		NET_DEBUG_TTY("** no subtask running. Next task is -1 **");
	}

	return pNewTask;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone*	CTaskNMControl::CreateTaskForLocalPed(CPed *pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	aiTask* forceNextSubTask = m_ForceNextSubTask.GetTask() ? m_ForceNextSubTask.GetTask()->Copy() : NULL;

	// prevent the control NM subtask from switching the ped to animated when it aborts
	SetDontSwitchToAnimatedOnAbort(true);

	u32 flagsForNewTask = m_nFlags;

	// keep the ragdoll running for the next task
	m_nFlags |= CLONE_LOCAL_SWITCH;

	if (!forceNextSubTask)
	{
		if (GetSubTask())
		{
			if (GetSubTask()->IsClonedFSMTask())
			{
				forceNextSubTask = static_cast<CTaskFSMClone*>(GetSubTask())->CreateTaskForLocalPed(pPed);
			}
			else
			{
				forceNextSubTask = GetSubTask()->Copy();
			}

			if (AssertVerify(forceNextSubTask) && static_cast<CTask*>(forceNextSubTask)->IsNMBehaviourTask())
			{
				smart_cast<CTaskNMBehaviour*>(forceNextSubTask)->ClearFlag(CTaskNMBehaviour::DONT_FINISH);
			}

			flagsForNewTask |= CTaskNMControl::ALREADY_RUNNING;
		}
		else if (m_nextTask != -1)
		{
			CTaskInfo *taskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(m_nextTask, PED_TASK_PRIORITY_MAX, false);

			if(taskInfo)
			{
				forceNextSubTask = taskInfo->CreateCloneFSMTask();
			}
		}
	}

	// the relax flags are only used by clones
	flagsForNewTask &= ~(CTaskNMControl::DO_RELAX | CTaskNMControl::DOING_RELAX);

	CTaskNMControl* pNewTask = rage_new CTaskNMControl(m_nMinTime, m_nMaxTime, forceNextSubTask, flagsForNewTask);

	NET_DEBUG_TTY("@@@@@@@@@@@@@@@@ %s MIGRATED LOCALLY. This task: 0x%p. New task: 0x%p @@@@@@@@@@@@@@@@\n", pPed->GetNetworkObject()->GetLogName(), this, pNewTask); 

	if (forceNextSubTask)
	{
		NOTFINAL_ONLY(NET_DEBUG_TTY("Forcing next task to %s", forceNextSubTask->GetTaskName());)
	}

	return pNewTask;
}

bool CTaskNMControl::HandleLocalToRemoteSwitch(CPed* pPed, CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo))
{
	// Continue to locally control the task when dead - essentially decoupling the owner and clone
	if (pPed->GetIsDeadOrDying())
	{
		return false;
	}

	if (GetSubTask())
	{
		m_currentTask = GetSubTask()->GetTaskType();

		// if our ped has fallen, inform the machine controlling the ped
		if (m_nFeedbackFlags & CTaskNMControl::BALANCE_FAILURE)
		{
			CRagdollRequestEvent::Trigger(pPed->GetNetworkObject()->GetObjectID());
		}

		// inform the sub-task to keep running until told to stop
		if (GetSubTask()->IsNMBehaviourTask())
		{
			smart_cast<CTaskNMBehaviour*>(GetSubTask())->SetFlag(CTaskNMBehaviour::DONT_FINISH);
		}
	}

	return true;
}

void CTaskNMControl::WarpCloneRagdollingPed(CPed *pPed, const Vector3 &newPosition)
{
    if(pPed && pPed->GetUsingRagdoll())
    {
		nmTaskDebugf(this, "Warp clone ragdolling ped called: pos ( %.3f, %.3f, %.3f)", newPosition.x, newPosition.y, newPosition.z);
        pPed->Teleport(newPosition, 0.0f, false, true, false, true, true);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::SetGetGrabParametersCallback(fnGetGrabParamsCallback pGetGrabParamsCallback, aiTask *pTask)
{
    m_pTask                  = pTask;
    m_pGetGrabParamsCallback = pGetGrabParamsCallback;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::ClearGetGrabParametersCallback()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    m_pTask                  = 0;
    m_pGetGrabParamsCallback = 0;
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMControl::ResetStartTime()
//////////////////////////////////////////////////////////////////////////
{
	m_nStartTime = fwTimer::GetTimeInMilliseconds();

	CTask* pSubTask = GetSubTask();
	if (pSubTask && pSubTask->IsNMBehaviourTask())
	{
		smart_cast<CTaskNMBehaviour*>(pSubTask)->ResetStartTime();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float CTaskNMControl::GetCNCRagdollDurationModifier()
{
	TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fRagdollDurationModifier, 0.5f, 0.0f, 1.0f, 0.01f);
	return fRagdollDurationModifier;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::SetFeedbackFlags(CPed* pPed, u32 nFeedbackFlagsToSet) 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (NetworkInterface::IsGameInProgress() && pPed->IsNetworkClone())
	{
		bool bRunningLocally = IsRunningLocally();

		if (!bRunningLocally && GetParent() && GetParent()->IsClonedFSMTask() && static_cast<CTaskFSMClone*>(GetParent())->IsRunningLocally())
		{
			bRunningLocally = true;
		}

		// we need to trigger a network event when a clone ped starts falling, to inform the master ped to fall also
		if (!bRunningLocally &&	!pPed->IsAPlayerPed() && !(m_nFeedbackFlags & CTaskNMControl::BALANCE_FAILURE) && (nFeedbackFlagsToSet & CTaskNMControl::BALANCE_FAILURE))
		{
			CRagdollRequestEvent::Trigger(pPed->GetNetworkObject()->GetObjectID());
		}
	}

	m_nFeedbackFlags |= nFeedbackFlagsToSet;
}

//////////////////////////////////////////////////////////////////////////
CTask* CTaskNMControl::FindBackgroundAiTask(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	CTask* pTask = NULL;

	if (pPed)
	{
		pTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP);
		
		if (!pTask)
		{
			pTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
		}
		if (!pTask)
		{
			pTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_DEFAULT);
		}
	}

	return pTask;
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMControl::CleanupUnhandledRagdoll(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	if (pPed)
	{
		bool bPedShouldBeDead = pPed->ShouldBeDead() || pPed->GetIsDeadOrDying();

		// a clone player running a local death response task is also treated as dead (he may still have health > 0)
		if (!bPedShouldBeDead && pPed->IsNetworkClone() && pPed->IsAPlayerPed())
		{
			bPedShouldBeDead = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->IsRunningDeathResponseTask();
		}

		if (pPed->GetUsingRagdoll() && !bPedShouldBeDead)
		{
			// Only switch to animated if another task hasn't taken over control of the ragdoll
			// and we don't have a ragdoll event incoming.
			CTask* pTask = pPed->GetPedIntelligence()->GetLowestLevelRagdollTask(pPed);
			if (!pTask && !pPed->GetPedIntelligence()->HasRagdollEvent())
			{
				if (pPed->IsNetworkClone())
				{
					bool bSwitchToAnimated = true;

					// don't switch to animated if there is a dying dead task waiting to start on the clone
					if (pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_DYING_DEAD, PED_TASK_PRIORITY_MAX, false) == NULL)
					{
						bSwitchToAnimated = false;
					}

					// don't switch to animated if there is a parachute task waiting to start on the clone
					if (pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_PARACHUTE, PED_TASK_PRIORITY_MAX, false) == NULL)
					{
						bSwitchToAnimated = false;
					}

					if (bSwitchToAnimated)
					{
						nmEntityDebugf(pPed, "CTaskNMControl::CleanupUnhandledRagdoll switching to animated (1)");
						pPed->SwitchToAnimated();
					}
				}
				else
				{
					// Certain ragdoll events (ones without a lifetime) will be considered 'expired' while we're in the process of
					// responding to them (events are ticked prior to responses being generated)
					// We therefore look at the event response tasks to determine if the ragdoll is unhandled or not...
					const aiTask* pTaskResponse = pPed->GetPedIntelligence()->GetPhysicalEventResponseTask();
					if (pTaskResponse == NULL)
					{
						pTaskResponse = pPed->GetPedIntelligence()->GetEventResponseTask();
					}

					if (pTaskResponse == NULL || !CTaskNMBehaviour::DoesResponseHandleRagdoll(pPed, pTaskResponse))
					{
						nmEntityDebugf(pPed, "CTaskNMControl::CleanupUnhandledRagdoll switching to animated (2)");
						pPed->SwitchToAnimated();
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMControl::IsValidNMControlSubTask(const CTask* pSubTask)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return (!pSubTask || pSubTask->IsNMBehaviourTask() || pSubTask->IsBlendFromNMTask() || pSubTask->GetTaskType() == CTaskTypes::TASK_RAGE_RAGDOLL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMControl::ProcessPreFSM()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Only set weapon switching block while offline
	if( !NetworkInterface::IsGameInProgress() )
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );
	}

	// Block cellphone anims while running nme
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableCellphoneAnimations, true);

	if(pPed->GetCoverPoint())
	{
		aiTask* pNMTask = NULL;
		if(!pPed->IsDead())
		{
			pNMTask = pPed->GetPedIntelligence()->FindTaskPhysicalResponseByType(CTaskTypes::TASK_NM_CONTROL);
			if(!pNMTask)
			{
				pNMTask = pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_NM_CONTROL, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
			}
		}

		if(pNMTask && pNMTask == this)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint, true);
		}
	}

	// There were some indications of animation timeslicing possibly having a negative
	// effect on NM (see B* 933652: "NM shot - Bad staggering behaviour for dying ped on ground"),
	// so to be safe, block it whenever this task is running. It was later confirmed that similar
	// problems occurred without timeslicing, but disabling it during NM still seems like a good idea.
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceAnimUpdate);

	// Ensure that we don't timeslice the ai update whilst the ragdoll is actively
	// simulating. Active ragdolls need to be carefully managed by controlling ai.
	if (pPed->GetUsingRagdoll())
	{
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
		pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}

	// Give the getup controlling ai the opportunity to stream in any required assets
	if (!pPed->GetIsDeadOrDying())
	{
		CTaskGetUp::RequestAssetsForGetup(pPed);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMControl::ProcessWeaponHiding(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	if (pPed && pPed->IsPlayer() && !pPed->IsNetworkClone() && !pPed->GetIsDeadOrDying() && !pPed->IsInjured())
	{
		CPedWeaponManager* pPedWeaponManager = pPed->GetWeaponManager();
		if (pPedWeaponManager != NULL)
		{
			if (pPed->GetUsingRagdoll())
			{
				CObject* pEquippedWeaponObject = pPedWeaponManager->GetEquippedWeaponObject();
				if (pEquippedWeaponObject != NULL && (pPedWeaponManager->GetIsArmed2Handed() || !pEquippedWeaponObject->GetIsVisible()))
				{
					if (pEquippedWeaponObject->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY))
					{
						pEquippedWeaponObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
						pPedWeaponManager->UnregisterNMWeapon();
						SetFlag(EQUIPPED_WEAPON_OBJECT_VISIBLE);
					}

					if (pEquippedWeaponObject->IsCollisionEnabled())
					{
						pEquippedWeaponObject->DisableCollision(NULL, true);
					}
				}
			}
			else
			{
				CTaskGetUp* pTaskGetUp = static_cast<CTaskGetUp*>(FindSubTaskOfType(CTaskTypes::TASK_GET_UP));
				if (pTaskGetUp != NULL && pTaskGetUp->m_bBlendedInDefaultArms)
				{
					if ((GetFlags() & EQUIPPED_WEAPON_OBJECT_VISIBLE) != 0)
					{
						CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
						if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
						{
							static_cast<CTaskHumanLocomotion*>(pTask)->SetInstantBlendNextWeaponClipSet(true);
						}

						CObject* pEquippedWeaponObject = pPedWeaponManager->GetEquippedWeaponObject();
						if (pEquippedWeaponObject != NULL)
						{
							if ((GetFlags() & EQUIPPED_WEAPON_OBJECT_VISIBLE) != 0)
							{
								pEquippedWeaponObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
							}
						}

						ClearFlag(EQUIPPED_WEAPON_OBJECT_VISIBLE);
					}
				}
				else
				{
					CObject* pEquippedWeaponObject = pPedWeaponManager->GetEquippedWeaponObject();
					if (pEquippedWeaponObject != NULL)
					{
						if ((GetFlags() & EQUIPPED_WEAPON_OBJECT_VISIBLE) != 0)
						{
							pEquippedWeaponObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
						}
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMControl::ProcessPostFSM()
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed* pPed = GetPed();

	// force the ped to drop if told to by a network update
	// Do this in ProcessPostFSM because we want to give the sub-task a chance to kick off its behaviors (including calling stopAllBehaviors) before forcing the fall
	if ((m_nFlags & CTaskNMControl::FORCE_FALL_OVER) && GetSubTask() && GetSubTask()->IsNMBehaviourTask() && pPed->GetRagdollInst())
	{
		// temp fix to prevent damage electric behaviour getting replaced with a relax. This is going to get replaced with a more robust way
		// of getting the ped to fall
		if (!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DAMAGE_ELECTRIC))
		{
			NET_DEBUG_TTY("***FALLING***\n");
			nmEntityDebugf(pPed, "NM Control - ForceFallOver called");
			smart_cast<CTaskNMBehaviour*>(GetSubTask())->ForceFallOver();
		}
		m_nFlags &= ~CTaskNMControl::FORCE_FALL_OVER;
	}

	ProcessWeaponHiding(pPed);

	return FSM_Continue;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMControl::UpdateFSM(const s32 iState, const FSM_Event iEvent)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pPed);
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		FSM_State(State_ControllingTask)
			FSM_OnEnter
				ControllingTask_OnEnter(pPed);
			FSM_OnUpdate
				return ControllingTask_OnUpdate(pPed);

		FSM_State(State_DecidingOnNextTask)
			FSM_OnEnter
				DecidingOnNextTask_OnEnter(pPed);
			FSM_OnUpdate
				return DecidingOnNextTask_OnUpdate(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::Start_OnEnter(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	NET_DEBUG_TTY("Start NM control on local. Seq: %d, task: 0x%p\n", GetNetSequenceID(), this);

	if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(pPed))
	{
		const float fCNCRagdollDurationModifier = CTaskNMControl::GetCNCRagdollDurationModifier();
		m_nMinTime = (u32)((float)m_nMinTime * fCNCRagdollDurationModifier);
		m_nMaxTime = (u32)((float)m_nMaxTime * fCNCRagdollDurationModifier);
	}

	// If we haven't switched to ragdoll mode on this ped, check that it's safe to do so and
	// then turn it on.

	if(!pPed->GetUsingRagdoll() && pPed->GetRagdollInst() && pPed->GetRagdollInst()->CheckCanActivate(pPed, false))
	{
		// Network peds are allowed to run NM tasks without being able to actually switch to ragdoll since they need to synchronize properly
		// with clones/owners that could be allowed to ragdoll. We need to check here that we're actually allowed to ragdoll before switching
		if (NetworkInterface::IsGameInProgress())
		{
			SwitchClonePedToRagdoll(pPed);
		}
		else
		{
			// Not sure if we even need this switch to ragdoll at this point since we should have already switched at some earlier point and this
			// could introduce issues where peds switch to ragdoll without calling CTaskNMBehaviour::CanUseRagdoll
			pPed->SwitchToRagdoll(*this);
		}

		// update the weapon hiding logic immediately (otherwise it won't kick in for a frame)
		ProcessWeaponHiding(pPed);
	}

	// If simulating as a non-NM ragdoll, switch tasks 
	if (pPed->GetRagdollInst()->IsSimulatingAsNonNMRagdoll())
	{
		// If it was a shot that we were trying to apply to an NM agent, apply it here
		if (m_ForceNextSubTask.GetTask() && m_ForceNextSubTask.GetTask()->GetTaskType() == CTaskTypes::TASK_NM_SHOT)
		{
			const CTaskNMShot *pShotTask = smart_cast<const CTaskNMShot *>(GetForcedSubTask());
			int numbounds = pPed->GetRagdollInst()->GetCacheEntry()->GetBound()->GetNumActiveBounds();
			if (pShotTask && pShotTask->GetComponent() < numbounds)
			{
				float impulseMag = pShotTask->GetWorldImpulse().Mag();
				Vector3 worldHitPos;
				pShotTask->GetWorldHitPosition(pPed, worldHitPos);
				pPed->GetRagdollInst()->ApplyBulletForce(pPed, impulseMag, pShotTask->GetWorldImpulse() / impulseMag, 
					worldHitPos, pShotTask->GetComponent(), pShotTask->GetWeaponHash());
			}
		}

		// Let TaskRageRagdoll know whether it's being activated via the rag prototype widgets
		bool bRunningPrototype = false;
		if (m_ForceNextSubTask.GetTask() && m_ForceNextSubTask.GetTask()->GetTaskType() == CTaskTypes::TASK_NM_PROTOTYPE)
		{
			bRunningPrototype = true;
		}

		m_ForceNextSubTask.SetTask(rage_new CTaskRageRagdoll(bRunningPrototype));
	}

	// Weapon hiding normally happens in ProcessPostFSM because other tasks un-hide the weapon so we need to re-hide it after the task update
	// Process weapon hiding once inline here so that we know how to pose the hands below
	ProcessWeaponHiding(pPed);

	if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject() && pPed->GetWeaponManager()->GetEquippedWeaponObject()->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY))
	{
		SetCurrentHandPose(HAND_POSE_HOLD_WEAPON, CMovePed::kHandRight);

		if(pPed->GetNMTwoHandedWeaponBothHandsConstrained()) 
		{
			SetCurrentHandPose(HAND_POSE_HOLD_WEAPON, CMovePed::kHandLeft);
		}
		else
		{
			SetCurrentHandPose(HAND_POSE_LOOSE, CMovePed::kHandLeft);
		}
	}
	else
	{
		SetCurrentHandPose(HAND_POSE_LOOSE, CMovePed::kHandLeft);
		SetCurrentHandPose(HAND_POSE_LOOSE, CMovePed::kHandRight);
	}

	// When CTaskNMControl is instantiated, it should be given an initial NM behaviour task to
	// execute (stored in m_pForceFirstSubtask). If we haven't been given an initial task, we
	// instantiate a "relax" behaviour by default.
	if(!m_ForceNextSubTask.GetTask())
	{
		m_ForceNextSubTask.SetTask(rage_new CTaskNMRelax(2000, 8000));
	}

	// TODO RA: Putting the code to select which clip the clipPose behaviour uses here
	// for now. Once clips have been created specifically for this, the selection code
	// can go in the logic which instantiates the NM behaviour task.
	// For now though, allow an clip to be selected by the NM behaviour task and just check
	// here if we need to use a default.

	// We need a dynamic cast here since some NM behaviour tasks might not inherit from
	// CTaskNMBehaviour (e.g. CTaskBlendFromNM) and we wish to skip them here.
	CTask *pForceNextSubTask = (CTask *) m_ForceNextSubTask.GetTask();
	CTaskNMBehaviour* pNmBehaviourTask = pForceNextSubTask->IsNMBehaviourTask() ? (CTaskNMBehaviour *) pForceNextSubTask : NULL;

	fwMvClipSetId clipSetId;
	fwMvClipId nClipId;
	s32 nClipDictIndex;
	u32 nClipHash; 

	if(pNmBehaviourTask && pNmBehaviourTask->GetClipPoseHelperClip(clipSetId, nClipId))
	{
		GetClipPoseHelper().SelectClip(clipSetId, nClipId);
	}
	//added this if the access is hash a dictionary index and an clip hash. 
	else if (pNmBehaviourTask && pNmBehaviourTask->GetClipPoseHelperClip(nClipDictIndex, nClipHash)) 
	{
		GetClipPoseHelper().SelectClip(nClipDictIndex, nClipHash);
	}
	//Re implement RA
	//else if(GetClipPoseHelper().GetClipSet() == CLIP_SET_ID_INVALID || GetClipPoseHelper().GetClipId() == CLIP_ID_INVALID)
	//{
	//	GetClipPoseHelper().SelectClip(CLIP_SET_BUSTED, CLIP_PERP_HANDS_UP);
	//}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMControl::Start_OnUpdate(CPed* OUTPUT_ONLY(pPed))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Now that the state has been instantiated, we switch to the task control state.

	Assertf(m_ForceNextSubTask.GetTask(), "Attempting to switch to control state for a non-existent task!");
	nmEntityDebugf(pPed, "NM Control starting - Subtask %s, parent task %s", m_ForceNextSubTask.GetTask() ? m_ForceNextSubTask.GetTask()->GetName() : "None", GetParent() ? GetParent()->GetName() : "None" );
	SetState(State_ControllingTask);

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMControl::Start_OnUpdateClone(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	NET_DEBUG_TTY("Start NM control on clone. Seq: %d, task: 0x%p, ped ragdolling : %s\n", GetNetSequenceID(), this, pPed->GetUsingRagdoll() ? "true" : "false");

	if (m_ForceNextSubTask.GetTask())
	{
		NOTFINAL_ONLY(NET_DEBUG_TTY("Forcing next subtask %s\n", m_ForceNextSubTask.GetTask()->GetTaskName());)

		m_currentTask = m_ForceNextSubTask.GetTask()->GetTaskType();

		SwitchClonePedToRagdoll(pPed);

		SetState(State_ControllingTask);
	}
	else
	{
		if (m_nextTask == -1)
		{
			// wait until we have an NM task 
			NET_DEBUG_TTY("No next task - wait\n");

			if (m_bCloneTaskFinished)
			{
				SetState(State_Finish);
			}

			return FSM_Continue;
		}
		else 
		{
			if (m_nextTask == CTaskTypes::TASK_BLEND_FROM_NM && !pPed->GetUsingRagdoll())
			{
				// just quit if we have to start with a blend from NM
				NET_DEBUG_TTY("QUIT - next task is TASK_BLEND_FROM_NM and ped is not ragdolling\n");
				m_ForceNextSubTask.SetTask(NULL);
			}
			else
			{
				// if there is no parent task, ped won't be set out of vehicle, in that case we do it here
				// limiting scope of change by only doing this for peds sitting by a turret
				if(!GetParent() && pPed->GetIsInVehicle() && pPed->GetMyVehicle()->GetVehicleWeaponMgr() && pPed->GetMyVehicle()->GetSeatManager())
				{
					CVehicle* vehicle =  pPed->GetMyVehicle();
					int seatIndex = vehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
					if(vehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(seatIndex))
					{
						pPed->SetPedOutOfVehicle(CPed::PVF_Warp|CPed::PVF_IgnoreSafetyPositionCheck|CPed::PVF_DontResetDefaultTasks|CPed::PVF_NoNetBlenderTeleport);
					}
				}
				SwitchClonePedToRagdoll(pPed);
				
				m_ForceNextSubTask.SetTask(CreateNewNMTaskClone(pPed));
				// TODO RA: Putting the code to select which clip the clippose behaviour uses here
				// for now. Once clips have been created specifically for this, the selection code
				// can go in the logic which instantiates the NM behaviour task.
				// For now though, allow an clip to be selected by the NM behaviour task and just check
				// here if we need to use a default.
				// 			if(GetClipPoseHelper().GetClipSet() == CLIP_SET_ID_INVALID || GetClipPoseHelper().GetClipId() == CLIP_ID_INVALID)
				// 				GetClipPoseHelper().SelectClip(CLIP_SET_BUSTED, CLIP_PERP_HANDS_UP);
			}
		}

		if (!m_ForceNextSubTask.GetTask())
		{
			SetState(State_Finish);
		}
		else
		{
			// Now that the state has been instantiated, we switch to the task control state.
			SetState(State_ControllingTask);
		}
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::ControllingTask_OnEnter(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// A new NM behaviour task should either have been created in "Start_OnEnter" or in
	// "DecidingOnNextTask_OnEnter" (stored in m_ForceNextSubTask) and we just pass it to the
	// FSM task control system here.

	if (taskVerifyf(m_ForceNextSubTask.GetTask(), "Trying to add a NULL task pointer onto the task stack."))
	{
		SetNewTask(m_ForceNextSubTask.RelinquishTask());	

		// inform the task to keep running until told to stop (dead peds are exempt)
		if (pPed->IsNetworkClone() && !(m_nFlags & DOING_RELAX) && !IsRunningLocally() && !pPed->GetIsDeadOrDying() && static_cast<CTask*>(GetNewTask())->IsNMBehaviourTask())
		{
			smart_cast<CTaskNMBehaviour*>(GetNewTask())->SetFlag(CTaskNMBehaviour::DONT_FINISH);
		}

		// Due to assumptions made by the task control systems, an NM behaviour task must
		// be correctly assigned to a CTaskNMControl.

		// TODO RA: For now, we have to include the possibility that a blend from NM task might still exist.
		ASSERT_ONLY(CTask *newTask = (CTask *) GetNewTask();)
		taskAssertf(IsValidNMControlSubTask(newTask), "Sub task should be derived from CTaskNMBehaviour but found %s.", GetNewTask()->GetName().c_str());

		// restore the default damping values to the ragdoll prior to running the next behaviour.
		// (Task level tuning can alter these, and we don't want to leave the old values around)
		if (pPed->GetRagdollInst()->GetArchetype())
		{
			phArchetypeDamp* pArch = static_cast<phArchetypeDamp*>(pPed->GetRagdollInst()->GetArchetype());
			pArch->ActivateDamping(phArchetypeDamp::LINEAR_C, Vector3(phArticulatedBody::sm_RagdollDampingLinC, phArticulatedBody::sm_RagdollDampingLinC, phArticulatedBody::sm_RagdollDampingLinC));
			pArch->ActivateDamping(phArchetypeDamp::LINEAR_V, Vector3(phArticulatedBody::sm_RagdollDampingLinV, phArticulatedBody::sm_RagdollDampingLinV, phArticulatedBody::sm_RagdollDampingLinV));
			pArch->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(phArticulatedBody::sm_RagdollDampingLinV2, phArticulatedBody::sm_RagdollDampingLinV2, phArticulatedBody::sm_RagdollDampingLinV2));
			pArch->ActivateDamping(phArchetypeDamp::ANGULAR_C, Vector3(phArticulatedBody::sm_RagdollDampingAngC, phArticulatedBody::sm_RagdollDampingAngC, phArticulatedBody::sm_RagdollDampingAngC));
			pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(phArticulatedBody::sm_RagdollDampingAngV, phArticulatedBody::sm_RagdollDampingAngV, phArticulatedBody::sm_RagdollDampingAngV));
			pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V2, Vector3(phArticulatedBody::sm_RagdollDampingAngV2, phArticulatedBody::sm_RagdollDampingAngV2, phArticulatedBody::sm_RagdollDampingAngV2));
		}
	}
	else
	{
		SetState(State_Finish);
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMControl::ControllingTask_OnUpdate(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	NET_DEBUG_TTY("LOCAL: Controlling NM task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");

	// If we've been given a new NM sub task to switch to, abort the current sub task if it hasn't finished.
	if(m_ForceNextSubTask.GetTask() && GetSubTask())
	{
		GetSubTask()->MakeAbortable(ABORT_PRIORITY_IMMEDIATE, NULL);
	}

	// Check if the current NM behaviour task has finished. If it has, we
	// switch states to decide which behaviour task to spawn next.
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
#if DEBUG_DRAW
		ms_debugDraw.Clear(12345); // Clear the flag debug text.
#endif //DEBUG_DRAW
		nmEntityDebugf(pPed, "NM Control - SubtaskFinished");
		SetState(State_DecidingOnNextTask);
		return FSM_Continue;
	}
	else if ((m_nFlags&CTaskNMControl::ON_MOTION_TASK_TREE)==0 && !pPed->GetIsSwimming() && pPed->GetUsingRagdoll())
	{
		// if we're doing an nm task on the main task tree, we want to force the motion task to
		// something sensible
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_DoNothing);

		// make sure we're running a player movement task 
		// so we can read control intentions
		if ( pPed->IsLocalPlayer())
		{
			CTask* pTask = pPed->GetPedIntelligence()->GetMovementResponseTask();
			if (!pTask || (pTask->GetTaskType()!=CTaskTypes::TASK_MOVE_PLAYER))
			{
				pPed->GetPedIntelligence()->AddTaskMovementResponse( rage_new CTaskMovePlayer());
			}

			// Mark the movement task as still in use this frame
			CTaskMoveInterface* pMoveInterface = pPed->GetPedIntelligence()->GetMovementResponseTask()->GetMoveInterface();
			pMoveInterface->SetCheckedThisFrame(true);

#if !__FINAL
			pMoveInterface->SetOwner(this);
#endif /* !__FINAL */

		}
	}

#if DEBUG_DRAW
	char sFlagDebugString[100];
	if(IsFeedbackFlagSet(BALANCE_FAILURE))
		sprintf(sFlagDebugString, "NM FEEDBACK FLAGS: BALANCE_FAILURE");
	else
		sprintf(sFlagDebugString, "NM FEEDBACK FLAGS: ");
	ms_debugDraw.AddText(pPed->GetTransform().GetPosition(), 0, -100, sFlagDebugString, Color32(0xff, 0xff, 0x00), 0, 12345);
	if(!m_bDisplayFlags)
		ms_debugDraw.Clear(12345);
#endif //DEBUG_DRAW

	HandleDriveToGetup(pPed);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::BumpPerformanceTime(u32 time)
//////////////////////////////////////////////////////////////////////////
{
	s32 timePassed = (s32) (fwTimer::GetTimeInMilliseconds() - m_nStartTime);
	s32 minTime = (s32)m_nMinTime;
	m_nMinTime = (u16)Max(timePassed + (s32)time, minTime - timePassed);
	m_nMaxTime = Max(m_nMinTime, m_nMaxTime);
}

//////////////////////////////////////////////////////////////////////////
float CTaskNMBehaviour::CalculateUprightDot(const CPed* pPed)
{
	if (pPed)
	{
		phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
		Mat34V boundMat;
		Transform(boundMat, pPed->GetRagdollInst()->GetMatrix(), bound->GetCurrentMatrix(RAGDOLL_BUTTOCKS));
		Vector3 pelvisPos = VEC3V_TO_VECTOR3(boundMat.GetCol3());
		Transform(boundMat, pPed->GetRagdollInst()->GetMatrix(), bound->GetCurrentMatrix(RAGDOLL_NECK));
		Vector3 pelvisToHead = VEC3V_TO_VECTOR3(boundMat.GetCol3()) - pelvisPos;
		pelvisToHead.Normalize();

		return pelvisToHead.z;
	}
	return 0.0f;
}


//////////////////////////////////////////////////////////////////////////
void CTaskNMControl::HandleDriveToGetup(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	PF_FUNC(HandleDriveToGetup);

	bool bDrivingToGetup = false;

	if (pPed->GetUsingRagdoll())
	{
		CTaskNMBehaviour* pSubTask = GetSubTask() && GetSubTask()->IsNMBehaviourTask() ? smart_cast<CTaskNMBehaviour*>(GetSubTask()) : NULL;
		
		// Attempt to drive to the getup pose before blending out
		Vector3 vecRootSpeed = pPed->GetLocalSpeed(Vector3(0.0f,0.0f,0.0f), false, 0);
		Matrix34 rootBone;
		pPed->GetBoneMatrix(rootBone, BONETAG_ROOT);
		if (
			sm_Tunables.m_DriveToGetup.m_AllowDriveToGetup												// Don't drive if it's not allowed
			&& (!sm_Tunables.m_DriveToGetup.m_OnlyAllowForShot || (pSubTask && pSubTask->GetTaskType()==CTaskTypes::TASK_NM_SHOT))
																										// Don't drive if we're only allowed to drive for shot tasks and we're not running a shot task
			&& !(GetFlags() & BLOCK_DRIVE_TO_GETUP)														// Don't drive for if it is currently being blocked?
			&& (IsFeedbackFlagSet(BALANCE_FAILURE) || sm_Tunables.m_DriveToGetup.m_AllowWhenBalanced)	// Don't drive if balancing unless we're allowed to drive while balancing
			&& GetPed()->GetHealth() > sm_Tunables.m_DriveToGetup.m_MinHealth							// Don't drive for dead and dying peds
			&& vecRootSpeed.Mag2()<square(sm_Tunables.m_DriveToGetup.m_MaxSpeed)						// Don't drive if still moving quickly
			&& abs(rootBone.c.z)<sm_Tunables.m_DriveToGetup.m_MaxUprightRatio							// Don't drive for upright peds
			&& (fwTimer::GetTimeInMilliseconds()-m_nStartTime > m_nMinTime)								// Don't drive to the getup if we're still forcing simulation
			)
		{
			// Pose matching is expensive and chances
			if (fwTimer::GetTimeInMilliseconds() > m_nDriveToGetupMatchTimer)
			{
				// Copy the position from the root matrix.
				rootBone.Identity3x3();

				CNmBlendOutSetList list;
				CTaskGetUp::SelectBlendOutSets(pPed, list, &rootBone);
				if (list.GetFilter().GetKeyCount() > 0)
				{
					fwMvClipSetId set;
					fwMvClipId clip;
					pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>()->GetPoseMatcher().FindBestMatch(*pPed->GetSkeleton(), set, clip, rootBone, &list.GetFilter());

					// grab the clip and update the itms
					const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(set, clip);
					if (pClip)
					{
						CNmBlendOutPoseItem* pPoseItem = list.FindPoseItem(set, clip, m_DriveToGetupMatchedBlendOutSet);
						// Want to limit the amount of data we capture so only capture the 'drive-to-getup' pose matches when they change!
						if (pPoseItem != NULL && pPoseItem != m_pDriveToGetupMatchedBlendOutPoseItem)
						{
#if __BANK
							CAnimViewer::BlendOutCapture(pPed, list, m_DriveToGetupMatchedBlendOutSet, pPoseItem, "Drive To Getup ");
#endif
							m_pDriveToGetupMatchedBlendOutPoseItem = pPoseItem;
							m_DriveToGetupTarget = list.GetTarget();
							if (m_pDriveToGetupMoveTask)
							{
								delete m_pDriveToGetupMoveTask;
								m_pDriveToGetupMoveTask = NULL;
							}
							m_pDriveToGetupMoveTask = list.RelinquishMoveTask();
						}
					}
				}

				m_nDriveToGetupMatchTimer = fwTimer::GetTimeInMilliseconds() + sm_Tunables.m_DriveToGetup.m_MatchTimer;
			}

			if (m_pDriveToGetupMatchedBlendOutPoseItem != NULL && m_pDriveToGetupMatchedBlendOutPoseItem->HasClipSet())
			{
				bDrivingToGetup = true;
				if (!(GetFlags() & DRIVING_TO_GETUP_POSE))
				{
					//send the messages to start driving to the getup pose
					sm_Tunables.m_OnEnableDriveToGetup.Post(*pPed, pSubTask);
					SetFlag(DRIVING_TO_GETUP_POSE);
				}

				const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_pDriveToGetupMatchedBlendOutPoseItem->GetClipSet(), m_pDriveToGetupMatchedBlendOutPoseItem->GetClip());
				if (pClip != NULL)
				{
					pPed->GetRagdollInst()->SetActivePoseFromClip(pClip, 0.0f, false);
				}
			}
		}
	}

	if (!bDrivingToGetup)
	{
		ClearDriveToGetup(pPed);
	}

}

//////////////////////////////////////////////////////////////////////////
void CTaskNMControl::ClearDriveToGetup(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{
	if(GetFlags() & DRIVING_TO_GETUP_POSE)
	{
		CTaskNMBehaviour* pSubTask = GetSubTask() && GetSubTask()->IsNMBehaviourTask() ? smart_cast<CTaskNMBehaviour*>(GetSubTask()) : NULL;

		sm_Tunables.m_OnDisableDriveToGetup.Post(*pPed, pSubTask);
		m_DriveToGetupMatchedBlendOutSet = NMBS_INVALID;
		m_pDriveToGetupMatchedBlendOutPoseItem = NULL;
		m_nDriveToGetupMatchTimer = 0;
		ClearFlag(DRIVING_TO_GETUP_POSE);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::SwitchClonePedToRagdoll(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (pPed->GetRagdollState() == RAGDOLL_STATE_ANIM_PRESETUP)
	{
		// the ped may not have been rendered yet so we need to force the presetup here
		pPed->ForceRagdollPreSetup();
	}

	// If we haven't switched to ragdoll mode on this ped, check that it's safe to do so and
	// then turn it on.
	if(!pPed->GetUsingRagdoll())
	{
		if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_NUM, NULL, 0.0f) && 
			pPed->GetRagdollInst() && pPed->GetRagdollInst()->CheckCanActivate(pPed, false))
		{
			// Set the random seed from the controlling ped
			pPed->GetRagdollInst()->SetRandomSeed(m_randomSeed);

			pPed->SwitchToRagdoll(*this);
		}
		else
		{
			NET_DEBUG_TTY("** Ped can't ragdoll **\n");
		}
	}
	else
	{
		NET_DEBUG_TTY("Ped already ragdolling\n");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::ControllingTask_OnExit(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If we've been running an "clipPose" behaviour internally, stop it here.
	if(m_nFlags & CTaskNMControl::DO_CLIP_POSE)
	{
		GetClipPoseHelper().StopClip(INSTANT_BLEND_OUT_DELTA);
	}

	ClearDriveToGetup(pPed);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMControl::ControllingTask_OnUpdateClone(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	static const float ABORT_DISTANCE = 2.0f;

	NET_DEBUG_TTY("CLONE: Controlling NM task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");


	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		NET_DEBUG_TTY("* subtask finished\n");

		SetState(State_DecidingOnNextTask);
	}
	else if (m_nFlags & CTaskNMControl::DOING_RELAX && m_nextTask != CTaskTypes::TASK_BLEND_FROM_NM)
	{
		// wait until the relax has finished
	}
	else if ((m_nextTask != -1 && m_nextTask != m_currentTask) || (m_nFlags & CTaskNMControl::DO_RELAX))
	{
		if (m_nFlags & CTaskNMControl::DO_RELAX)
			NET_DEBUG_TTY("* force relax, abort\n");
		else
			NET_DEBUG_TTY("* next task = %s, abort\n", TASKCLASSINFOMGR.GetTaskName(m_nextTask));

		// abort current NM task, keeping ragdoll active
		if (GetSubTask())
			GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, 0);

		SetState(State_DecidingOnNextTask);
	}
	else if (m_bCloneTaskFinished)
	{
		if (OverridesNetworkBlender(pPed))
		{
			// abort the task if the ped is getting up and the main ped has got too far away
			Vector3 diff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - static_cast<CNetBlenderPed*>(pPed->GetNetworkObject()->GetNetBlender())->GetCurrentPredictedPosition();

			if (diff.XYMag2() > ABORT_DISTANCE)
			{
				NET_DEBUG_TTY("* aborting for getup because owner ped is too far from clone\n");
				return FSM_Quit;
			}
		}
	}
	return FSM_Continue;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::DecidingOnNextTask_OnEnter(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// A new NM behaviour task is required. Figure out which one we want to start next,
	// instantiate it and store the pointer to the new task in m_ForceNextSubTask so that
	// "State_ControllingTask" can add pass it to the FSM task management system.

	// If m_ForceNextSubTask has been set by ForceNewSubTask(), we are done.
	if(m_ForceNextSubTask.GetTask())
		return;

	// TODO RA: The fall-back logic below is necessary while some tasks rely on CTaskTypes
	// conditionals. Make all NM tasks decide on their next state and only pass some data
	// to tell CTaskNMControl how to instantiate the new task.
	// TODO RA: Probably want to merge CTaskBlendFromNM as a separate state in this task which
	// would be used instead of returning from here to "State_ControllingTask".


	// We need a dynamic cast here since some NM behaviour tasks might not inherit from
	// CTaskNMBehaviour (e.g. CTaskBlendFromNM) and we wish to skip them here.

	CTask *pSubTask = GetSubTask();
	CTaskNMBehaviour* pOldTaskNM = pSubTask->IsNMBehaviourTask() ? (CTaskNMBehaviour *) pSubTask : NULL;

	if(pOldTaskNM && pOldTaskNM->GetSuggestedNextTask()!=CTaskTypes::TASK_INVALID_ID)
	{
		if (pOldTaskNM->GetSuggestedNextTask() == CTaskTypes::TASK_RAGE_RAGDOLL)
		{
			m_ForceNextSubTask.SetTask(rage_new CTaskRageRagdoll());
		}
		else
		{
			// This should be the default case; NM tasks define what they want to do next.
			m_ForceNextSubTask.SetTask(CreateNewNMTask(pOldTaskNM->GetSuggestedNextTask(), pPed));
		}
		nmEntityDebugf(pPed, "NM Control DecidingOnNextTask - Using suggested next task: %s", m_ForceNextSubTask.GetTask() ? m_ForceNextSubTask.GetTask()->GetName() : "None" );
	}
	else
	{
		if(GetSubTask()->GetTaskType()==CTaskTypes::TASK_BLEND_FROM_NM)
		{
			if(pPed->GetUsingRagdoll() && !pPed->ShouldBeDead())
			{
				// Uh oh. We just finished a blend back to clip, but we're still running our
				// ragdoll; use relax as a fallback.
				m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_NM_RELAX, pPed));
				nmEntityDebugf(pPed, "NM Control DecidingOnNextTask - Finished getup, but in ragdoll. Falling back on %s", m_ForceNextSubTask.GetTask() ? m_ForceNextSubTask.GetTask()->GetName() : "None" );
			}
			else
			{
				// We finished blending back to clip, so we're done.
				m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_FINISHED, pPed));
				nmEntityDebugf(pPed, "NM Control DecidingOnNextTask - Finished getup, ending task");
			}
		}
		else if (GetSubTask()->GetTaskType() == CTaskTypes::TASK_RAGE_RAGDOLL)
		{
			if (pPed->IsDead() || pPed->IsFatallyInjured())
			{
				m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_FINISHED, pPed));
				nmEntityDebugf(pPed, "NM Control DecidingOnNextTask - Finished rage ragdoll on dead ped, ending task");
			}
			else
			{
				m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_BLEND_FROM_NM, pPed));
				nmEntityDebugf(pPed, "NM Control DecidingOnNextTask - Finished rage ragdoll, next task: %s", m_ForceNextSubTask.GetTask() ? m_ForceNextSubTask.GetTask()->GetName() : "None" );
			}
		}
		else
		{
			// The only other subtasks supported here are derived from CTaskNMBehaviour.
			Assert(pOldTaskNM);

			// don't bother with a get up if this is locally running, we are still waiting to see if the master ped has ragdolled, and at this point the answer is probably no 
			bool bLocallyRunningCloneTask = pPed->IsNetworkClone() && IsRunningLocally() && !pPed->GetPedIntelligence()->GetLowestLevelRagdollTask(pPed);

			if(pPed->GetUsingRagdoll() && (m_nFlags & CTaskNMControl::DO_BLEND_FROM_NM) && !bLocallyRunningCloneTask)
			{
				// By default, if ragdolling just blend back to clip (if we haven't been requested
				// to skip this step by another task/event).
				m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_BLEND_FROM_NM, pPed));
				nmEntityDebugf(pPed, "NM Control DecidingOnNextTask - Finished nm behaviour, next task: %s", m_ForceNextSubTask.GetTask() ? m_ForceNextSubTask.GetTask()->GetName() : "None" );
			}
			else
			{
				// Otherwise we're done.
				m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_FINISHED, pPed));
				nmEntityDebugf(pPed, "NM Control DecidingOnNextTask - Finished sub task, ending task");
			}
		}
	}

	// Each behaviour will decide for itself whether 
	// to block the drive to getup task.
	ClearFlag(BLOCK_DRIVE_TO_GETUP);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::DecidingOnNextTask_OnEnterClone(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	NET_DEBUG_TTY("Decide on next task\n");

	// force a relax if the ped is dead but still standing at this point
	if (!IsFeedbackFlagSet(BALANCE_FAILURE) && m_nextTask != CTaskTypes::TASK_RAGE_RAGDOLL && pPed->GetIsDeadOrDying() && !(m_nFlags & CTaskNMControl::DOING_RELAX))
	{
		NET_DEBUG_TTY("Ped is dead, force a relax\n");
		m_nFlags |= CTaskNMControl::DO_RELAX;
		m_nFeedbackFlags |= BALANCE_FAILURE;
	}

	if (pPed->GetIsDeadOrDying())
	{
		NET_DEBUG_TTY("ABORT - ped is dead\n");
		m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_FINISHED, pPed));
	}
	else if ((m_nFlags & CTaskNMControl::DO_RELAX) && !m_bCloneTaskFinished)
	{
		NET_DEBUG_TTY("FORCE RELAX\n");
		m_nFlags &= ~CTaskNMControl::DO_RELAX;
		m_nFlags |= CTaskNMControl::DOING_RELAX;

		// force a relax to get the ped to fall to the ground
		m_ForceNextSubTask.SetTask(rage_new CTaskNMRelax(2000, 2000));
	}
	else if (m_nFlags & CTaskNMControl::DOING_RELAX)
	{
		m_nFlags &= ~CTaskNMControl::DOING_RELAX;

		NET_DEBUG_TTY("Finished relax: Create new task %s\n", TASKCLASSINFOMGR.GetTaskName(m_nextTask));
		// start the next task dictated by the queriable state update
		m_ForceNextSubTask.SetTask(CreateNewNMTaskClone(pPed));
	}
	else if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_BLEND_FROM_NM)
	{
		if(pPed->GetUsingRagdoll())
		{
			if (m_bCloneTaskFinished)
			{
				NET_DEBUG_TTY("** Error. Ped still in ragdoll but main task has finished. Switch to animated. ***\n");
				pPed->SwitchToAnimated();
				Assert(!pPed->GetUsingRagdoll());
				m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_FINISHED, pPed));
			}
			else
			{
				NET_DEBUG_TTY("** Error. Ped still in ragdoll. ***x\n");

				if (m_nextTask == CTaskTypes::TASK_BLEND_FROM_NM)
				{
					NET_DEBUG_TTY("Wait for next task\n");
					m_waitForNextTask = true;
				}
				else
				{
					NET_DEBUG_TTY("Create new task for %s\n", TASKCLASSINFOMGR.GetTaskName(m_nextTask));
					m_ForceNextSubTask.SetTask(CreateNewNMTaskClone(pPed));
				}
			}
		}
		else if (m_nextTask != CTaskTypes::TASK_BLEND_FROM_NM)
		{
			NET_DEBUG_TTY("Blend from NM -> %s\n", TASKCLASSINFOMGR.GetTaskName(m_nextTask));

			if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_NETWORK, NULL, 0.0f) && pPed->GetRagdollInst()->CheckCanActivate(pPed, false))
			{
				pPed->SwitchToRagdoll(*this);
				m_ForceNextSubTask.SetTask(CreateNewNMTaskClone(pPed));
			}
			else
			{
				NET_DEBUG_TTY("** Ped can't ragdoll. QUIT **\n");
				m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_FINISHED, pPed));
			}
		}
		else
		{
			NET_DEBUG_TTY("Blend from NM -> finish\n");

			// We finished blending back to clip, so we're done.
			m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_FINISHED, pPed));
		}
	}
	else if (m_nextTask != -1 && m_nextTask != m_currentTask)
	{
		NET_DEBUG_TTY("Create new task %s\n", TASKCLASSINFOMGR.GetTaskName(m_nextTask));
		// start the next task dictated by the queriable state update
		m_ForceNextSubTask.SetTask(CreateNewNMTaskClone(pPed));
	}
	else if (!pPed->GetUsingRagdoll())
	{
		NET_DEBUG_TTY("ABORT - not ragdolling\n");
		// We've just come from a ragdoll task, but we're not using ragdoll, so quit.
		m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_FINISHED, pPed));
	}
	else if (pPed->GetRagdollInst() && pPed->GetRagdollInst()->GetNMAgentID()==-1)
	{
		NET_DEBUG_TTY("WAIT - no NM agent\n");

		Displayf("%s lost it's NM agent\n", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "??");

		// the ped has come to rest, starting another relax will cause an infinite loop (the subtask will immediately quit). We need to wait for the next task.
		m_waitForNextTask = true;
	}
	else
	{
#if !__NO_OUTPUT
		if (GetSubTask() && GetSubTask()->IsNMBehaviourTask())
		{
			CTaskNMBehaviour* pBehaviourTask = smart_cast<CTaskNMBehaviour*>(GetSubTask());

			if (pBehaviourTask->GetHasAborted())
			{
				// this assert can fire if we receive an update from the new owner of the ped and there is no sub task (m_nextTask == -1)
				nmTaskDebugf(this, "Clone ped %s NM behaviour task %s was aborted prematurely", pPed->GetNetworkObject()->GetLogName(), pBehaviourTask->GetTaskName());
			}
			else if (pBehaviourTask->GetHasFailed())
			{
				nmTaskDebugf(this, "Clone ped %s NM behaviour task %s has failed prematurely", pPed->GetNetworkObject()->GetLogName(), pBehaviourTask->GetTaskName());
			}
			else if (pBehaviourTask->GetHasSucceeded())
			{
				nmTaskDebugf(this, "Clone ped %s NM behaviour task %s has succeeded prematurely", pPed->GetNetworkObject()->GetLogName(), pBehaviourTask->GetTaskName());
			}
			else
			{
				nmTaskDebugf(this, "Clone ped %s NM behaviour task %s has terminated prematurely", pPed->GetNetworkObject()->GetLogName(), pBehaviourTask->GetTaskName());
			}
		}
		else
		{
			nmTaskDebugf(this, "Clone ped %s NM control subtask %s has terminated prematurely", pPed->GetNetworkObject()->GetLogName(), GetSubTask() ? GetSubTask()->GetTaskName() : "?");
		}
#endif // !__NO_OUTPUT

		// no next task! Just do a balance or relax until we know what it is
		if (pPed->IsDead() || IsFeedbackFlagSet(BALANCE_FAILURE))
		{
			NET_DEBUG_TTY("No next task! Do relax\n");
			m_nFlags |= CTaskNMControl::DOING_RELAX;

			// force a relax to get the ped to fall to the ground
			m_ForceNextSubTask.SetTask(rage_new CTaskNMRelax(2000, 2000));
		}
		else if (!pPed->GetIsSwimming())
		{
			NET_DEBUG_TTY("No next task! Do balance\n");
			m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_NM_BALANCE, pPed));
		}
		else
		{
			NET_DEBUG_TTY("No next task! Ped is swimming so quit\n");
			m_ForceNextSubTask.SetTask(CreateNewNMTask(CTaskTypes::TASK_FINISHED, pPed));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMControl::DecidingOnNextTask_OnUpdate(CPed* UNUSED_PARAM(pPed))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(!m_ForceNextSubTask.GetTask())
	{
		SetState(State_Finish);
	}
	else
	{
		SetState(State_ControllingTask);
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskNMControl::DecidingOnNextTask_OnUpdateClone(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// We have decided on a new task already in "OnEnter" so we can switch back to
	// "State_ControllingTask" and allow it to manage the new task.

	if (m_waitForNextTask)
	{
		if (m_nextTask != m_currentTask)
		{
			if (aiVerify(m_nextTask != -1))
			{
				NET_DEBUG_TTY("Wait over: create new task %s\n", TASKCLASSINFOMGR.GetTaskName(m_nextTask));
				m_ForceNextSubTask.SetTask(CreateNewNMTaskClone(pPed));
			}
			else
			{
				m_ForceNextSubTask.SetTask(NULL);
			}

			if (!m_ForceNextSubTask.GetTask())
			{
				NET_DEBUG_TTY("QUIT - couldn't create next task\n");
				SetState(State_Finish);
			}
			else
			{
				SetState(State_ControllingTask);
			}

			m_waitForNextTask = false;
		}
		else if (m_bCloneTaskFinished)
		{
			SetState(State_Finish);
		}
	}
	else if(!m_ForceNextSubTask.GetTask())
	{
		NET_DEBUG_TTY("QUIT - no next task\n");
		SetState(State_Finish);
	}
	else
	{
		SetState(State_ControllingTask);
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::Finish_OnEnterClone(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Need to update the blender for the ped with the last position received
	// in order to blend back to where we should be.
	CNetObjPed     *netObjPed  = static_cast<CNetObjPed *>(pPed->GetNetworkObject());
	CNetBlenderPed *netBlender = netObjPed ? static_cast<CNetBlenderPed *>(netObjPed->GetNetBlender()) : 0;

	if(AssertVerify(netBlender))
	{
		netBlender->UpdatePosition(netBlender->GetLastPositionReceived(), CNetwork::GetNetworkTime());
		netBlender->UpdateHeading (netBlender->GetLastHeadingReceived(), CNetwork::GetNetworkTime());
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
aiTask* CTaskNMControl::CreateNewNMTask(const int iSubTaskType, CPed *pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	aiTask* pNewSubTask = NULL;
	switch(iSubTaskType)
	{
// 	case CTaskTypes::TASK_SIMPLE_CLIMB:
// 		m_bGrabbed2Handed=true; //so when we return to complexinair, it knows to go into climb
// 		pNewSubTask = rage_new CTaskBlendFromNM(BLEND_FROM_NM_CLIP, CLIP_SET_CLIMB_PED, CLIP_CLIMB_IDLE, CLIP_ID_INVALID);
// 		break;

	case CTaskTypes::TASK_FINISHED:
		{
			pNewSubTask = NULL;
		}
		break;

	case CTaskTypes::TASK_NM_RELAX:
		{
			pNewSubTask = rage_new CTaskNMRelax(Min(m_nMinTime, snTimeOutRelax), Max(m_nMaxTime, 1000u));
		}
		break;

	case CTaskTypes::TASK_NM_BUOYANCY:
		{
			u32 nTimeOutBuoyancy = snTimeOutBuoyancy;

			if (pPed->GetPrimaryWoundData()->valid && (pPed->GetPrimaryWoundData()->component == RAGDOLL_HEAD || pPed->GetPrimaryWoundData()->numHits > 5))
			{
				nTimeOutBuoyancy = snTimeOutBuoyancyRelax;
			}

			// Can only run the buoyancy task for a certain amount of time...
			pNewSubTask = rage_new CTaskNMBuoyancy(Min(m_nMinTime, nTimeOutBuoyancy), nTimeOutBuoyancy);
		}
		break;

	case CTaskTypes::TASK_NM_RIVER_RAPIDS:
		{
			pNewSubTask = rage_new CTaskNMRiverRapids(m_nMinTime, m_nMaxTime);
		}
		break;

	case CTaskTypes::TASK_NM_INJURED_ON_GROUND:
		{
			pNewSubTask = rage_new CTaskNMInjuredOnGround(m_nMinTime, m_nMaxTime);
		}
		break;

	case CTaskTypes::TASK_NM_DANGLE:
		{
			Vector3 zero(0.0f,0.0f,0.0f);
			pNewSubTask = rage_new CTaskNMDangle(zero);
		}
		break;

	case CTaskTypes::TASK_NM_BRACE:
		{
			Assertf(false, "CTaskNMControl::CreateSubTask - TASK_NM_BRACE not supported");
			pNewSubTask = rage_new CTaskBlendFromNM();
		}
		break;

	case CTaskTypes::TASK_NM_SHOT:
		{
			Assertf(false, "CTaskNMControl::CreateSubTask - TASK_NM_SHOT not supported");
			pNewSubTask = rage_new CTaskBlendFromNM();
		}
		break;

	case CTaskTypes::TASK_NM_HIGH_FALL:
		{
			bool fromBrace = (GetFlags()&BLOCK_QUICK_GETUP_ON_HIGH_FALL)!=0;
			pNewSubTask = rage_new CTaskNMHighFall(m_nMinTime, NULL, fromBrace?CTaskNMHighFall::HIGHFALL_FROM_CAR_HIT : CTaskNMHighFall::HIGHFALL_IN_AIR, NULL, fromBrace);
			ClearFlag(BLOCK_QUICK_GETUP_ON_HIGH_FALL); // unset the block quick getup flag
		}
		break;

	case CTaskTypes::TASK_NM_BALANCE:
		{
			pNewSubTask = rage_new CTaskNMBalance(m_nMinTime, m_nMaxTime, NULL, 0);
			static_cast<CTaskNMBalance*>(pNewSubTask)->EvaluateAndSetType(pPed);
		}
		break;

	case CTaskTypes::TASK_NM_EXPLOSION:
		{
			Vector3 vecZero(0.0f,0.0f,0.0f);
			pNewSubTask = rage_new CTaskNMExplosion(m_nMinTime, m_nMaxTime, vecZero);
		}
		break;

	case CTaskTypes::TASK_NM_ONFIRE:
		{
			pNewSubTask = rage_new CTaskNMOnFire(m_nMinTime, m_nMaxTime);
		}
		break;

		// the transition back to clip
	case CTaskTypes::TASK_BLEND_FROM_NM:
		{
			// Depending on the event, we may have been requested to skip the blend from NM (ped died for example).
			if(m_nFlags & CTaskNMControl::DO_BLEND_FROM_NM)
			{

				if(pPed->GetIsDeadOrDying())
				{
					NET_DEBUG_TTY("Ped is dying, can't create task\n");				
				}
				else
				{
					pNewSubTask = rage_new CTaskBlendFromNM(m_DriveToGetupMatchedBlendOutSet);
					if (GetSubTask())
					{
						if (m_DriveToGetupTarget.GetIsValid())
						{
							static_cast<CTaskBlendFromNM*>(pNewSubTask)->SetForcedSetTarget(m_DriveToGetupTarget);
						}

						if (m_pDriveToGetupMoveTask)
						{
							static_cast<CTaskBlendFromNM*>(pNewSubTask)->SetForcedSetMoveTask((CTask*)m_pDriveToGetupMoveTask->Copy());
						}

						if (GetSubTask()->IsNMBehaviourTask() && static_cast<CTaskNMBehaviour*>(GetSubTask())->GetIsStuckOnVehicle())
						{
							// Maintain the damage entity during the blend-from-NM.  This will prevent the ped from re-entering NM 
							// during the blend and for CCollisionEventScanner::RAGDOLL_VEHICLE_IMPACT_DAMAGE_TIME_LIMIT ms afterwards
							static_cast<CTaskBlendFromNM*>(pNewSubTask)->MaintainDamageEntity(true);
						}

						if (GetSubTask()->GetTaskType()==CTaskTypes::TASK_NM_BALANCE)
						{
							CTaskNMBalance* pTask = static_cast<CTaskNMBalance*>(GetSubTask());

							// only want to do the bumped reaction if we've been bumped by another entity
							if (pTask->GetLookAtEntity())
							{
								CTaskBlendFromNM* pNewTask = static_cast<CTaskBlendFromNM*>(pNewSubTask);
								pNewTask->SetAllowBumpedReaction(true);
								pNewTask->SetBumpedByEntity(pTask->GetLookAtEntity());
							}
						}
					}

					if ((m_nFlags&ON_MOTION_TASK_TREE)!=0)
					{
						static_cast<CTaskBlendFromNM*>(pNewSubTask)->SetRunningAsMotionTask(true);
					}
				}
			}
		}
		break;

	default:
		Assertf(false, "CTaskNMControl::CreateNewNMTask : no switch case for NM task type %d", iSubTaskType);
		break;
	}

	return pNewSubTask;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
aiTask* CTaskNMControl::CreateNewNMTaskClone(CPed *pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	aiTask* pNewSubTask = NULL;

	// Use the queriable state task infos to generate the appropriate NM subtask.
	// TASK_BLEND_FROM_NM is a special case (it is not cloned) - use non-clone task creation to avoid code duplication.
	if (m_nextTask != -1)
	{
		if (m_nextTask == CTaskTypes::TASK_BLEND_FROM_NM)
		{
			// force a blend
			m_nFlags |= CTaskNMControl::DO_BLEND_FROM_NM;

			pNewSubTask = CreateNewNMTask(CTaskTypes::TASK_BLEND_FROM_NM, pPed);
		}
		else if (taskVerifyf(m_nextTask >= CTaskTypes::TASK_NM_RELAX && m_nextTask<CTaskTypes::TASK_RAGDOLL_LAST, "CTaskNMControl::CreateNewNMTaskClone - m_nextTask (%d) is not an NM task (%s)", m_nextTask, pPed->GetNetworkObject()->GetLogName()))
		{
			CTaskInfo* pInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(m_nextTask, PED_TASK_PRIORITY_MAX, false);

			if (pInfo)
			{
				pNewSubTask = pPed->GetPedIntelligence()->CreateCloneTaskFromInfo(pInfo);
				taskAssertf(pNewSubTask, "CTaskNMControl: failed to create a subtask of type %d", m_nextTask);
			}
			else
			{
				aiWarningf("CTaskNMControl::CreateNewNMTaskClone: failed to find task %d in the queriable interface for %s", m_nextTask, pPed->GetNetworkObject()->GetLogName());

				// we have no task info - this can happen very rarely when the ped is running more synced tasks than we support in the task slots. Just give him a default relax in this case
				m_nextTask = CTaskTypes::TASK_NM_RELAX;
				pNewSubTask = CreateNewNMTask(m_nextTask, pPed);
				taskAssertf(pNewSubTask, "CTaskNMControl: failed to create a subtask of type %d", m_nextTask);
			}

			if (pNewSubTask && m_pGetGrabParamsCallback)
			{
				(*m_pGetGrabParamsCallback)(m_pTask, pNewSubTask);
			}
		}
	}

	m_currentTask = pNewSubTask ? pNewSubTask->GetTaskType() : -1;

	m_nFlags &= ~ALREADY_RUNNING;

	return pNewSubTask;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMControl::ForceNewSubTask(aiTask* pNewSubTask)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If m_ForceNextSubTask isn't NULL, then we have a task that we never started properly. It should get
	// nulled in ControllingTask_OnEnter() if the switch happened.
	if(m_ForceNextSubTask.GetTask())
	{
		Assertf(false, "CTaskNMControl::ForceNewSubTask - trying to replace unused sub task");
	}

	m_ForceNextSubTask.SetTask(pNewSubTask);
}
#if __BANK
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::InitCreateWidgetsButton()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	bkBank* pBank = &BANKMGR.CreateBank("NaturalMotion");
	if (pBank)
	{
		pBank->AddButton("Create NaturalMotion Widgets", InitWidgets);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::InitWidgets()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Destroy the Load Widgets button
	bkBank* pBank = BANKMGR.FindBank("NaturalMotion");
	if(pBank)
		pBank->Destroy();

	// Create the new bank
	pBank = &BANKMGR.CreateBank("NaturalMotion");
	if (pBank)
	{
		pBank->AddToggle("Only use rage ragdolls", &sm_OnlyUseRageRagdolls, NullCB, ""); 

		pBank->PushGroup("Impulse Modifiers", false);
		{
			pBank->AddToggle("Do Last Stand Mode", &sm_DoLastStand, NullCB, "");
			pBank->AddSlider("Last Stand Max Time Between Hits", &sm_LastStandMaxTimeBetweenHits, 0.0f, 1.0f, 0.001f);
			pBank->AddSlider("Armed ped bump resistance", &fragInstNMGta::m_ArmedPedBumpResistance, 1.0f, 5.0f, 0.1f);
			//pBank->AddSlider("Don't fall until dead", &sm_DontFallUntilDeadStrength, 0.0f, 1.0f, 0.1f);
			pBank->AddSlider("Max shot upright force", &sm_MaxShotUprightForce, 0.0f, 8.0f, 0.25f);
			pBank->AddSlider("Max shot upright torque", &sm_MaxShotUprightTorque, 0.0f, 8.0f, 0.25f);
			pBank->AddToggle("Override bullet impulses", &sm_DoOverrideBulletImpulses, NullCB, ""); 
			pBank->AddSlider("Override impulse", &sm_OverrideImpulse, 0.0f, 400.0f, 1.0f);
			pBank->AddSlider("Shot to forearms impulse cap", &sm_ArmsImpulseCap, 0.0f, 200.0f, 1.0f);
			pBank->AddSlider("Minimum standing thigh impulse", &sm_ThighImpulseMin, 0.0f, 300.0f, 1.0f);
			pBank->AddSlider("Bullet pop up impulse", &sm_BulletPopupImpulse, 0.0f, 200.0f, 1.0f);
			pBank->AddSlider("Successive impulse increase scale", &sm_SuccessiveImpulseIncreaseScale, 0.0f, 1.0f, 0.01f);
			pBank->PopGroup();
		}

		pBank->PushGroup("Selected Agent Debug Output", false);
		{
			pBank->AddSlider("Character Health", &sm_CharacterHealth, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Character Strength", &sm_CharacterStrength, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Stay Upright Magnitude", &sm_StayUprightMagnitude, 0.0f, 8.0f, 0.01f);
			pBank->AddSlider("RigidBody Impulse Ratio", &sm_RigidBodyImpulseRatio, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Shot Relax Amount", &sm_ShotRelaxAmount, 0.0f, 1.0f, 0.01f);
			pBank->PopGroup();
		}
	}	
}
#endif

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::Tunables::TunableForce::ShouldApply(u32 startTime)
//////////////////////////////////////////////////////////////////////////
{
	if (!m_Enable)
	{
		return false;
	}
	
	u32 timePassed = fwTimer::GetTimeInMilliseconds() - startTime;

	if ( timePassed < m_Delay)
	{
		return false;
	}

	if (timePassed > (m_Delay+m_Duration))
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::Tunables::TunableForce::GetImpulseVec(Vec3V_InOut forceVec, Vec3V_In velVec, const CPed* pHitPed)
//////////////////////////////////////////////////////////////////////////
{
	ScalarV mag(1.0f);

	mag *= ScalarV(m_Mag);

	if (m_ScaleWithVelocity && (m_VelocityMax > m_VelocityMin))
	{
		float velMag = Mag(velVec).Getf();
		float t = (velMag-m_VelocityMin)/(m_VelocityMax - m_VelocityMin);
		t=Clamp(t, 0.0f, 1.0f);
		float forceMag = Lerp(t, m_ForceAtMinVelocity, m_ForceAtMaxVelocity);

		mag *= ScalarV(forceMag);
	}

	if (m_ScaleWithUpright)
	{
		// Decrease the force magnitude as the hit ped falls down
		phBoundComposite *bound = pHitPed->GetRagdollInst()->GetCacheEntry()->GetBound();
		Mat34V boundMat;
		Transform(boundMat, pHitPed->GetRagdollInst()->GetMatrix(), bound->GetCurrentMatrix(RAGDOLL_BUTTOCKS));
		Vec3V pelvisPos = boundMat.GetCol3();
		Transform(boundMat, pHitPed->GetRagdollInst()->GetMatrix(), bound->GetCurrentMatrix(RAGDOLL_NECK));
		Vec3V pelvisToHead = boundMat.GetCol3() - pelvisPos;
		pelvisToHead = NormalizeSafe(pelvisToHead, Vec3V(V_ZERO));
		Assert(pelvisToHead.GetXf() == pelvisToHead.GetXf());
		if (pelvisToHead.GetXf() != pelvisToHead.GetXf())
		{
			pelvisToHead.ZeroComponents();
		}

		ScalarV uprightDot = Max(ScalarV(V_ZERO), pelvisToHead.GetZ());
		mag*=uprightDot;
	}

	// convert force to impulse
	mag*=ScalarV(fwTimer::GetTimeStep());

	if (m_ClampImpulse)
	{
		mag = Clamp(mag, ScalarV(m_MinImpulse), ScalarV(m_MaxImpulse));
	}	

	forceVec*=mag;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::Tunables::InverseMassScales::Apply(phContactIterator* pImpacts)
//////////////////////////////////////////////////////////////////////////
{
	bool appliedScale = false;
	if (!pImpacts)
	{
		return appliedScale;
	}

	if (m_ApplyPedScale || m_ApplyVehicleScale)
	{
		appliedScale = true;
		pImpacts->SetMassInvScales(m_ApplyPedScale ? m_PedScale : 1.0f, m_ApplyVehicleScale ? m_VehicleScale : 1.0f);
#if PDR_ENABLED
		if (pImpacts->GetCachedManifold().GetInstanceA() == pImpacts->GetOtherInstance())
		{
			PDR_ONLY(debugPlayback::RecordModificationToContact(*pImpacts, "VehicleHit(vehicle[A])", 0.0f));
		}
		else
		{
			PDR_ONLY(debugPlayback::RecordModificationToContact(*pImpacts, "VehicleHit(vehicle[B])", 0.0f));
		}
#endif // PDR_ENABLED
	}

	return appliedScale;
}

//////////////////////////////////////////////////////////////////////////
const CTaskNMBehaviour::Tunables::BlendOutThreshold& CTaskNMBehaviour::Tunables::StandardBlendOutThresholds::PickBlendOut(const CPed* pPed) const
//////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsPlayer())
	{
		if (NetworkInterface::IsGameInProgress())
		{
			return m_PlayerMp;
		}
		else
		{
			return m_Player;
		}
	}
	else
	{
		return m_Ai;
	}
}

//////////////////////////////////////////////////////////////////////////
u32 CTaskNMBehaviour::Tunables::BlendOutThreshold::GetSettledTime(u32 seed) const
//////////////////////////////////////////////////////////////////////////
{ 
	if (m_RandomiseSettledTime)
	{
		return (u32)(((float)((u16)(seed))/65535.0f) * (m_SettledTimeMS-m_SettledTimeMinMS)) + m_SettledTimeMinMS;
	}
	else
	{
		return m_SettledTimeMS;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::Tunables::ParserPostLoad()
//////////////////////////////////////////////////////////////////////////
{
	OnMaxGameplayAgentsChanged();
	OnMaxRageRagdollsChanged();
	CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolLocalPlayer).SetMaxRagdolls(1);

	if (PARAM_disableRagdollPoolsSp.Get())
		m_EnableRagdollPooling = false;
	if (PARAM_enableRagdollPoolsMp.Get())
		m_EnableRagdollPoolingMp = true;
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::Tunables::OnMaxPlayerAgentsChanged()
//////////////////////////////////////////////////////////////////////////
{
	CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolLocalPlayer).SetMaxRagdolls( (NetworkInterface::IsGameInProgress() && m_ReserveLocalPlayerNmAgentMp) || (!NetworkInterface::IsGameInProgress() && m_ReserveLocalPlayerNmAgent) ? 1 : 0 );
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::Tunables::OnMaxGameplayAgentsChanged()
//////////////////////////////////////////////////////////////////////////
{
	CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolNmGameplay).SetMaxRagdolls( NetworkInterface::IsGameInProgress() ? m_MaxGameplayNmAgentsMp : m_MaxGameplayNmAgents);
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::Tunables::OnMaxRageRagdollsChanged()
//////////////////////////////////////////////////////////////////////////
{
	CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll).SetMaxRagdolls( NetworkInterface::IsGameInProgress() ? m_MaxRageRagdollsMp : m_MaxRageRagdolls);
}

//////////////////////////////////////////////////////////////////////////
CTaskNMBehaviour::CRagdollPool::CRagdollPool()
//////////////////////////////////////////////////////////////////////////
	: m_MaxRagdolls(0)
{

}

//////////////////////////////////////////////////////////////////////////
CTaskNMBehaviour::CRagdollPool::~CRagdollPool()
//////////////////////////////////////////////////////////////////////////
{

}

//////////////////////////////////////////////////////////////////////////
void	CTaskNMBehaviour::CRagdollPool::AddToPool(CPed& ped)
//////////////////////////////////////////////////////////////////////////
{
		RegdPed newPed( ped );
		m_Peds.PushAndGrow(newPed);
}

//////////////////////////////////////////////////////////////////////////
void	CTaskNMBehaviour::CRagdollPool::RemoveFromPool(CPed& ped)
//////////////////////////////////////////////////////////////////////////
{
	for (s32 i=0; i<m_Peds.GetCount(); i++)
	{
		if (m_Peds[i].Get()==&ped)
		{
			m_Peds.Delete(i);
			return;
		}
	}
}
//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::AddToRagdollPool(CTaskNMBehaviour::eRagdollPool pool, CPed& ped)
//////////////////////////////////////////////////////////////////////////
{
	if (pool>=0 && pool<kNumRagdollPools)
	{
		RemoveFromRagdollPool(ped);
		CRagdollPool& poolObj = GetRagdollPool(pool);

		if (IsPoolingEnabled() && poolObj.GetFreeSlots()<=0)
		{
			// if the pool is full, boot out the lowest priority ragdoll. 
			// Avoid network clone players as the forced local dying dead task will not be abortable, which will happen when the player respawns
			CPed* minPriorityPed = NULL;
			float minPriority = FLT_MAX;
			for (s32 i=0; i< poolObj.GetRagdollCount(); i++)
			{
				CPed* pPed = poolObj.GetPed(i);
				if (pPed && !(pPed->IsPlayer() && pPed->IsNetworkClone()))
				{
					float taskPriority = CTaskNMBehaviour::CalcRagdollScore(pPed, RAGDOLL_TRIGGER_NUM, NULL, -1);
					if (taskPriority<minPriority)
					{
						minPriorityPed = pPed;
						minPriority = taskPriority;
					}
				}
			}

			if (minPriorityPed!=NULL)
			{
				if (pool!=kRagdollPoolRageRagdoll && minPriorityPed->ShouldBeDead() && CTaskNMBehaviour::CanUseRagdoll(minPriorityPed, RAGDOLL_TRIGGER_DIE))
				{
					// switch to a dead rage ragdoll
					if (minPriorityPed->IsNetworkClone())
					{
						// force a local dying dead clone task on the ped. This won't work for players as the dead dying task will not be able to quit once started.
						CTaskDyingDead* pDeadTask = rage_new CTaskDyingDead(NULL, CTaskDyingDead::Flag_startDead, fwTimer::GetTimeInMilliseconds());
						minPriorityPed->GetPedIntelligence()->AddLocalCloneTask(pDeadTask, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
					}
					else
					{
						BANK_ONLY(nmEntityDebugf(minPriorityPed, "Aborting and switching to rage ragdoll with score %.4f for new activation on ped %s(%p)", minPriority, ped.GetDebugName(), &ped));
						CEventDeath event(false, fwTimer::GetTimeInMilliseconds(), true);
						ASSERT_ONLY(CEvent* pAddedEvent = )minPriorityPed->GetPedIntelligence()->AddEvent(event);
#if __ASSERT
						if (!pAddedEvent)
						{
							minPriorityPed->SpewRagdollTaskInfo();
							nmAssertf(pAddedEvent,  "Failed to add dead task event while switching dead ped to rage ragdoll. Check the log for more info.");
						}
#endif // __ASSERT
					}
					
					minPriorityPed->GetRagdollInst()->SwitchFromNMToRageRagdoll(false); // This will force the task to route to TaskRageRagdoll
					CTaskNMBehaviour::AddToRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *minPriorityPed);
				}
				else
				{
					Vec3V vecRootVelocity(V_ZERO);
					if(minPriorityPed->GetCollider() != NULL)
					{
						vecRootVelocity = minPriorityPed->GetCollider()->GetVelocity();
					}

					// Abort the least important ragdoll
					// Switch the ped back to animation using the ragdoll frame.
					// The running behavior's animated fall-back state / nm control cleanup will take care of blending out the ragdoll frame as appropriate.
					CTaskDyingDead* pTaskDead = static_cast<CTaskDyingDead*>(minPriorityPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));
					if (pTaskDead != NULL)
					{
						BANK_ONLY(nmEntityDebugf(minPriorityPed, "Aborting ragdoll via dying task with score %.4f for new activation on ped %s(%p)", minPriority, ped.GetDebugName(), &ped));
						pTaskDead->DealWithDeactivatedPhysics(minPriorityPed);
					}
					else
					{
						BANK_ONLY(nmEntityDebugf(minPriorityPed, "Aborting ragdoll task with score %.4f for new activation on ped %s(%p)", minPriority, ped.GetDebugName(), &ped));
						minPriorityPed->SwitchToAnimated(true, true, true, false, false, true, true);
					}

					// Try and maintain velocity in the animated ped.
					minPriorityPed->SetVelocity(RCC_VECTOR3(vecRootVelocity));

#if __ASSERT
					if (minPriorityPed->GetUsingRagdoll())
					{
						minPriorityPed->SpewRagdollTaskInfo();
					}
#endif //__ASSERT
					taskAssertf(!minPriorityPed->GetUsingRagdoll(), "CTaskNMBehaviour::AddToRagdollPool failed to switch lowest priority ragdoll ped back to animation!");
				}
			}
		}

		GetRagdollPool(pool).AddToPool(ped);
		ped.SetCurrentRagdollPool(pool);
	}
}
//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::RemoveFromRagdollPool(CPed& ped)
//////////////////////////////////////////////////////////////////////////
{
	eRagdollPool pool = ped.GetCurrentRagdollPool();
	if (pool>=0 && pool<kNumRagdollPools)
	{
		GetRagdollPool(pool).RemoveFromPool(ped);
		ped.SetCurrentRagdollPool(kRagdollPoolInvalid);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBehaviour::RagdollPoolHasSpaceForPed(CTaskNMBehaviour::eRagdollPool pool, const CPed& ped, bool abortExisting, float priority)
//////////////////////////////////////////////////////////////////////////
{	
	if (!IsPoolingEnabled())
		return true;

	if (pool>=0 && pool<kNumRagdollPools)
	{
		// ped is already in the pool
		if (ped.GetCurrentRagdollPool()==pool)
			return true;
		// free space available
		if (GetRagdollPool(pool).GetFreeSlots()>0)
			return true;

		if (abortExisting)
		{
			CRagdollPool& poolObj = GetRagdollPool(pool);
			CPed* minPriorityPed = NULL;
			float minPriority  = FLT_MAX;
			for (s32 i=0; i< poolObj.GetRagdollCount(); i++)
			{
				CPed* pPed = poolObj.GetPed(i);
				if (pPed)
				{
					float taskPriority = CTaskNMBehaviour::CalcRagdollScore(pPed, RAGDOLL_TRIGGER_NUM, NULL, -1.0f);
					if (taskPriority<minPriority)
					{
						minPriorityPed = pPed;
						minPriority = taskPriority;
					}
				}
			}

			if ((minPriorityPed!=NULL) && (minPriority<priority))
			{
				return true;
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
float CTaskNMBehaviour::CalcRagdollEventScore(CPed* pPed, eRagdollTriggerTypes nTrigger, CEntity* pEntityResponsible, float fPushValue)
//////////////////////////////////////////////////////////////////////////
{
	bool bPedIsPlayer = pPed->IsAPlayerPed();
	bool bPedIsLocalPlayer = bool(pPed==CGameWorld::FindLocalPlayer()) || (bPedIsPlayer && NetworkInterface::IsSpectatingPed(pPed)); // treat the spectated player as the local player here (we need to see them ragdoll if possible)
	bool bLocalPlayerResponsible = bool(pEntityResponsible && (pEntityResponsible == CGameWorld::FindLocalPlayer() || pEntityResponsible == CGameWorld::FindLocalPlayerVehicle()));

	float fPedMass = pPed->GetCapsuleInfo() ? pPed->GetCapsuleInfo()->GetMass() : pPed->GetMass();

	if (NetworkInterface::IsGameInProgress() && bPedIsPlayer && NetworkInterface::IsSpectatingPed(pPed))
	{
		// treat the spectated player as the local player here (we need to see them ragdoll if possible)
		bPedIsLocalPlayer = true;
	}

	////////////////////
	// Calculate a score based on the event itself
	////////////////////

	float fEventScore = 0.0f;
	switch(nTrigger)
	{
	case RAGDOLL_TRIGGER_BULLET:
	case RAGDOLL_TRIGGER_MELEE:
		if(bPedIsPlayer)
		{
			// Treat handcuffed players the same as injured when hit by melee
			if(nTrigger == RAGDOLL_TRIGGER_MELEE || pPed->IsInjured())
			{
				if(bPedIsLocalPlayer)
				{
					fEventScore = SCORE_PL_FORCE;
				}
				else
				{
					fEventScore = SCORE_PL_HIGH;
				}
			}
			else if (nTrigger==RAGDOLL_TRIGGER_BULLET && pPed->IsNetworkClone() && !pPed->GetIsInVehicle() && pEntityResponsible==CGameWorld::FindLocalPlayer())
			{
				// remote players being shot by our player are allowed to ragdoll (it only used when the remote player is dying)
				fEventScore = SCORE_PL_HIGH;
			}
			else
			{
				fEventScore = SCORE_ZERO;
			}
		}
		else
		{
			if (!sm_Tunables.m_BlockOffscreenShotReactions || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
			{
				if(bLocalPlayerResponsible)
				{
					fEventScore = SCORE_AMB_FORCE;
				}
				else
				{
					fEventScore = SCORE_AMB_SHOT;
				}
			}
			else
			{
				nmEntityDebugf(pPed, "CanUseRagdoll: %s - FAILED. Character not visible on screen.", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			}
		}
		break;

	case RAGDOLL_TRIGGER_EXPLOSION:
		{
			fEventScore = rage::Min(1.0f, (fPushValue * EXPLOSION_FORCE_SCORE_MODIFIER) / (fPedMass * rage::Abs(CPhysics::GetGravitationalAcceleration())));

			// if force value is -1.0 this trigger must have come from script, so force activation
			if(fPushValue == -1.0f)
			{
				fEventScore = SCORE_SCRIPT_FORCE;
			}

			// check for explosions hitting the cops inside heli's, want them to ragdoll
			if(pPed->GetAttachParent() && ((CPhysical *) pPed->GetAttachParent())->GetIsTypeVehicle() && ((CVehicle*)pPed->GetAttachParent())->GetVehicleType()==VEHICLE_TYPE_HELI)
			{
				if(bLocalPlayerResponsible)
				{
					fEventScore = SCORE_AMB_FORCE;
				}
				else
				{
					fEventScore = SCORE_AMB_SHOT;
				}
			}

			if(bPedIsPlayer && fEventScore < 0.1f)
			{
				fEventScore = 0.0f;
			}
		}
		break;

	case RAGDOLL_TRIGGER_FIRE:
	case RAGDOLL_TRIGGER_SMOKE_GRENADE:
		{
			if(bPedIsLocalPlayer)
			{
				fEventScore = SCORE_PL_FORCE;
			}
			else
			{
				fEventScore = SCORE_AMB_FIRE;
			}
		}
		break;

	case RAGDOLL_TRIGGER_FALL:
		{
			if(bPedIsLocalPlayer)
			{
				fEventScore = SCORE_PL_FORCE;
			}
			else
			{
				fEventScore = SCORE_AMB_FALL;
			}
		}
		break;

	case RAGDOLL_TRIGGER_IN_WATER:
		{
			if(!bPedIsPlayer)
			{
				fEventScore = SCORE_AMB_FALL;
			}
		}
		break;

	case RAGDOLL_TRIGGER_WATERJET:
		{
			if(bLocalPlayerResponsible)
			{
				fEventScore = SCORE_AMB_HIGH;
			}
			else
			{
				fEventScore = SCORE_AMB_SHOT;
			}
		}
		break;

	case RAGDOLL_TRIGGER_DIE:
		{
			if(bPedIsLocalPlayer)
			{
				fEventScore = SCORE_PL_FORCE;
			}
			else if (pPed->GetTimeOfFirstBeingUnderAnotherRagdoll()!=0) // don't allow activation if another ragdoll has been resting on us
			{
				fEventScore = SCORE_AMB_DEAD_UNDER;
			}
			else
			{
				CTaskDyingDead* pTaskDead = static_cast<CTaskDyingDead*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));
				if (pTaskDead != NULL && pTaskDead->StartDead() && pTaskDead->GetState() == CTaskDyingDead::State_DeadAnimated && pTaskDead->GetSnapToGroundStage() == CTaskDyingDead::kSnapPoseReceived)
				{
					// These peds might not be properly conformed to the ground/slope so it's important we allow them to switch to ragdoll
					fEventScore = SCORE_AMB_FORCE;
				}
				else
				{
					fEventScore = SCORE_AMB_DEAD;
				}
			}
		}
		break;

	case RAGDOLL_TRIGGER_IMPACT_CAR_WARNING:
		{
			if(bPedIsPlayer)
			{
				fEventScore = SCORE_ZERO;
			}
			else if(bLocalPlayerResponsible)
			{
				fEventScore = SCORE_AMB_HIGH;
			}
			else
			{
				fEventScore = SCORE_AMB_FALL;
			}
		}
		break;

	case RAGDOLL_TRIGGER_IMPACT_CAR_SIDE_SWIPE:
		{
			if (bPedIsLocalPlayer)
			{
				fEventScore = SCORE_PL_FORCE;
			}
			else if (bLocalPlayerResponsible)
			{
				fEventScore = SCORE_AMB_HIGH;
			}
			else
			{
				fEventScore = SCORE_AMB_FALL;
			}
		}
		break;

	case RAGDOLL_TRIGGER_IMPACT_CAR:
	case RAGDOLL_TRIGGER_IMPACT_CAR_CAPSULE:
		{
			if(bPedIsLocalPlayer)
			{
				if(fPushValue > sm_Tunables.m_VehicleMinSpeedForPlayerActivation)
				{
					fEventScore = SCORE_PL_FORCE;
				}
			}
			else if(fPushValue > sm_Tunables.m_VehicleMinSpeedForAiActivation)
			{
				fEventScore = SCORE_AMB_HIGH;
			}
		}
		break;

	case RAGDOLL_TRIGGER_IMPACT_PLAYER_PED:
	case RAGDOLL_TRIGGER_IMPACT_PLAYER_PED_RAGDOLL:
	case RAGDOLL_TRIGGER_IMPACT_PED_RAGDOLL:
		{
			if(bPedIsPlayer)
			{
				fEventScore = 0.0f;
			}
			else
			{
				if(bLocalPlayerResponsible)
				{
					if(!pPed->IsNetworkClone())
					{
						fEventScore = 1.0f;
					}
					else if(pPed->GetNetworkObject() &&
						!(pPed->GetUsingRagdoll() || pPed->GetRagdollState() >= RAGDOLL_STATE_PHYS_ACTIVATE) &&
						!pPed->GetNetworkObject()->IsPendingOwnerChange())
					{
						CRequestControlEvent::Trigger(pPed->GetNetworkObject()  NOTFINAL_ONLY(, "TaskNM"));
					}
				}
				else if(pEntityResponsible && pEntityResponsible->GetIsTypePed()
					&& ((CPed*)pEntityResponsible)->GetUsingRagdoll())
				{
					fEventScore = rage::Min(1.0f, fPushValue / fPedMass);
					if(fEventScore < 0.1f)
					{
						fEventScore = 0.0f;
					}
				}
			}
		}
		break;

	case RAGDOLL_TRIGGER_IMPACT_OBJECT_COVER:
		{
			fEventScore = 1.0f;
		}
		break;

	case RAGDOLL_TRIGGER_IMPACT_OBJECT:
		{
			bool bPedHitByContainerAttachedToHandler = false;
			if(pEntityResponsible && pEntityResponsible->GetIsTypeObject())
			{
				if(static_cast<CObject*>(pEntityResponsible)->IsAttachedToHandler())
				{
					// Still only want to activate if the container is being moved quickly enough.
					dev_float sfVelThreshold = 0.3f;
					if(pEntityResponsible->GetCollider() && MagSquared(pEntityResponsible->GetCollider()->GetVelocity()).Getf() > sfVelThreshold)
					{
						bPedHitByContainerAttachedToHandler = true;
					}
				}
			}

			u32 objectNameHash = pEntityResponsible->GetBaseModelInfo()->GetModelNameHash();
			bool bPedHitByRollerCoaster = false;
			if( objectNameHash == MI_ROLLER_COASTER_CAR_1.GetName().GetHash() || 
				objectNameHash == MI_ROLLER_COASTER_CAR_2.GetName().GetHash() )
			{
				bPedHitByRollerCoaster = true;
			}

			bool isTombstone = pEntityResponsible->GetIsTypeObject() ? SafeCast(CObject, pEntityResponsible)->m_nObjectFlags.bIsNetworkedFragment : false;

			if(bPedIsPlayer && !bPedHitByContainerAttachedToHandler && !bPedHitByRollerCoaster && !isTombstone)
			{
				nmEntityDebugf(pPed, "CanUseRagdoll: %s - FAILED. Object impacts disabled on player peds.", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
				fEventScore = 0.0f;
			}
			else
			{
				fEventScore = rage::Min(1.0f, fPushValue / fPedMass);
				if(fEventScore < 0.1f)
				{
					fEventScore = 0.0f;
				}
			}
		}
		break;

	case RAGDOLL_TRIGGER_PHYSICAL_FALLOFF:
		{
			if(pEntityResponsible)
			{
				if(pEntityResponsible->GetIsTypeVehicle())
				{						
					static float sfPlayerFalloffCarMinSpeed = 5.0f;
					static float sfAIFalloffCarMinSpeed = 2.0f;
					static float sfFallOffBikeMinSpeed = 0.45f;

					CVehicle* pVehicle = (CVehicle*)pEntityResponsible;
					if(pVehicle->m_nVehicleFlags.bForceEnemyPedsFalloff && (pVehicle->GetDriver() == NULL || !pVehicle->GetDriver()->GetPedIntelligence()->IsFriendlyWith(*pPed)))
					{
						fEventScore = bPedIsPlayer ? SCORE_PL_HIGH : SCORE_AMB_SHOT;
					}
					else if(!pVehicle->InheritsFromBoat() && pVehicle->GetVehicleType()!=VEHICLE_TYPE_PLANE && pVehicle->GetVehicleType()!=VEHICLE_TYPE_TRAIN)// will be changing this to use a check against whether the poly we're standing on is part of a vehicle interior once that is setup.
					{
						float fVehicleVelocityMag2 = pVehicle->GetVelocityIncludingReferenceFrame().Mag2();
						if(bPedIsPlayer && fVehicleVelocityMag2 > square(sfPlayerFalloffCarMinSpeed))
						{
							fEventScore = SCORE_PL_HIGH;
						}
						else if(!bPedIsPlayer && fVehicleVelocityMag2 > square(sfAIFalloffCarMinSpeed))
						{
							fEventScore = SCORE_AMB_SHOT;
						}
						// Otherwise, if we're standing on a bike that's being picked up by someone else then force us to fall off if the bike's moving with enough speed
						else if (pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE && static_cast<CBike*>(pVehicle)->m_nBikeFlags.bGettingPickedUp && pVehicle->GetDriver() != pPed)
						{
							if (fVehicleVelocityMag2 >= square(sfFallOffBikeMinSpeed))
							{
								fEventScore = SCORE_PL_HIGH;
							}
						}
					}
					else if(pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE || pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN)
					{
						static float sfPlayerFalloffPitchDifference = 0.4f;
						static float sfPlayerFalloffRollDifference = 0.4f;

						Vector3 vPedCentre;
						pPed->GetBoundCentre(vPedCentre);
						spdAABB tempBox;
						const spdAABB &vehicleBox = pVehicle->GetBoundBox(tempBox);

						bool shouldRagdoll = false;
						if(!vehicleBox.ContainsPoint(RCC_VEC3V(vPedCentre)))//check the ped is inside the vehicle, otherwise rag doll
						{
							shouldRagdoll = true;
						}
						else if(pVehicle->GetTransform().GetPitch() - pPed->GetTransform().GetPitch() > sfPlayerFalloffPitchDifference)
						{
							shouldRagdoll = true;
						}
						else if(pVehicle->GetTransform().GetRoll() - pPed->GetTransform().GetRoll() > sfPlayerFalloffRollDifference)
						{
							shouldRagdoll = true;
						}
						else if( pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE )
						{
							shouldRagdoll = true;
						}

						// check the vehicle is moving (don't want to ragdoll whilst climbing on stationary upside down planes for example)
						if (shouldRagdoll)
						{
							if(bPedIsPlayer && pVehicle->GetVelocity().Mag2() > sfPlayerFalloffCarMinSpeed*sfPlayerFalloffCarMinSpeed)
							{
								fEventScore = SCORE_PL_HIGH;
							}
							else if(!bPedIsPlayer && pVehicle->GetVelocity().Mag2() > sfAIFalloffCarMinSpeed*sfAIFalloffCarMinSpeed)
							{
								fEventScore = SCORE_PL_HIGH;
							}
						}
					}
				}
				else if(pEntityResponsible->GetIsTypePed())
				{
					// oops - standing on top of an animated ped - fall over please!
					CPed* pPed = (CPed*)pEntityResponsible;
					if(!pPed->GetUsingRagdoll())
					{
						if(bPedIsPlayer)
						{
							fEventScore = SCORE_PL_HIGH;
						}
						else
						{
							fEventScore = SCORE_AMB_SHOT;
						}
					}
				}
			}
		}
		break;

	case RAGDOLL_TRIGGER_HELIBLADES:
	case RAGDOLL_TRIGGER_VEHICLE_FALLOUT:
	case RAGDOLL_TRIGGER_VEHICLE_JUMPOUT:
		{
			if(bPedIsLocalPlayer)
			{
				fEventScore = SCORE_PL_FORCE;
			}
			else
			{
				if(pPed->IsDead())
				{
					fEventScore = SCORE_AMB_FORCE;
				}
				else
				{
					fEventScore = SCORE_AMB_SHOT;
				}
			}
		}
		break;

	case RAGDOLL_TRIGGER_FORKLIFT_FORKS_FALLOFF:
		{
			fEventScore = SCORE_PL_FORCE;
		}

	case RAGDOLL_TRIGGER_VEHICLE_GRAB:
		{
			if(bPedIsLocalPlayer)
			{
				fEventScore = SCORE_PL_FORCE;
			}
			else
			{
				fEventScore = SCORE_AMB_HIGH;
			}
		}
		break;

	case RAGDOLL_TRIGGER_REACTION_FLINCH:
		{
			if(bPedIsLocalPlayer)
			{
				fEventScore = SCORE_ZERO;
			}
			else
			{
				fEventScore = SCORE_AMB_REACT;
			}
		}
		break;

	case RAGDOLL_TRIGGER_SCRIPT:
	case RAGDOLL_TRIGGER_SCRIPT_VEHICLE_FALLOUT:
	case RAGDOLL_TRIGGER_DEBUG:
	case RAGDOLL_TRIGGER_NETWORK:
		{
			fEventScore = SCORE_SCRIPT_FORCE;
		}
		break;

	case RAGDOLL_TRIGGER_ELECTRIC:
	case RAGDOLL_TRIGGER_RUBBERBULLET:
		{
			if(bPedIsPlayer)
			{
				if(true BANK_ONLY(&& (!NetworkInterface::IsGameInProgress() || CWeaponDebug::GetUseRagdollForStungunAndRubbergun())))
				{
					fEventScore = SCORE_PL_FORCE;
				}
			}
			else
			{
				fEventScore = SCORE_AMB_HIGH;
			}
		}
		break;

	case RAGDOLL_TRIGGER_DRAGGED_TO_SAFETY:
		{
			fEventScore = SCORE_DRAG;
		}
		break;

		// high priority code requests
	case RAGDOLL_TRIGGER_ACTIVATE_ON_COLLISION:
	case RAGDOLL_TRIGGER_DRUNK:
	case RAGDOLL_TRIGGER_IMPACT_PROJECTILE:
	case RAGDOLL_TRIGGER_SLUNG_OVER_SHOULDER:
	case RAGDOLL_TRIGGER_ANIMATED_ATTACH:
	case RAGDOLL_TRIGGER_CLIMB_FAIL: 
		{
			fEventScore = SCORE_PL_FORCE;
		}
		break;

	case RAGDOLL_TRIGGER_NUM:
		{
			fEventScore = SCORE_AMB_REACT;
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
			{
				// Resist being booted from a ragdoll pool if we're onscreen, recently added, and have a high relative velocity.
				// Forcing these types of ragdolls back to animated tends to look bad and can create awkward collision situations (see B*1206945).
				if (pPed->GetRagdollInst() != NULL && pPed->GetRagdollInst()->GetActivationStartTime() > 0)
				{
					static float sfJustActivated = 0.5f;
					u32 uTimeSinceActivated = fwTimer::GetTimeInMilliseconds() - pPed->GetRagdollInst()->GetActivationStartTime();
					fEventScore = RampValue(((float)uTimeSinceActivated) / 1000.0f, 0.0f, sfJustActivated, SCORE_AMB_FORCE, SCORE_AMB_HIGH);
				}
			}
		}
		break;

	case RAGDOLL_TRIGGER_SNOWBALL:
		{
			if (!sm_Tunables.m_BlockOffscreenShotReactions || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
			{
				if(bLocalPlayerResponsible)
				{
					fEventScore = SCORE_AMB_FORCE;

				}
				else
				{
					fEventScore = SCORE_AMB_SHOT;
				}
			}
			else
			{
				nmEntityDebugf(pPed, "CanUseRagdoll: %s - FAILED. Character not visible on screen.", CNmDefines::ms_aRagdollTriggerTypeNames[nTrigger]);
			}
		}
		break;

	default:
		{
			Assert(false);
		}
		break;
	}

	return fEventScore;
}

//////////////////////////////////////////////////////////////////////////
float CTaskNMBehaviour::CalcRagdollMultiplierScore(CPed* pPed, eRagdollTriggerTypes nTrigger)
//////////////////////////////////////////////////////////////////////////
{
	bool bPedIsPlayer = pPed->IsAPlayerPed();
	bool bPedIsLocalPlayer = bool(pPed==CGameWorld::FindLocalPlayer()) || (bPedIsPlayer && NetworkInterface::IsSpectatingPed(pPed)); // treat the spectated player as the local player here (we need to see them ragdoll if possible)

	if (NetworkInterface::IsGameInProgress() && bPedIsPlayer && NetworkInterface::IsSpectatingPed(pPed))
	{
		// treat the spectated player as the local player here (we need to see them ragdoll if possible)
		bPedIsLocalPlayer = true;
	}

	bool bForceVisibleOnScreen = false;
	if (nTrigger == RAGDOLL_TRIGGER_DIE && !bPedIsLocalPlayer && pPed->GetTimeOfFirstBeingUnderAnotherRagdoll() == 0)
	{
		CTaskDyingDead* pTaskDead = static_cast<CTaskDyingDead*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));
		if (pTaskDead != NULL && pTaskDead->StartDead() && pTaskDead->GetState() == CTaskDyingDead::State_DeadAnimated && pTaskDead->GetSnapToGroundStage() == CTaskDyingDead::kSnapPoseReceived)
		{
			// Don't punish peds that started dead for being off-screen since that's the best time for them to switch to ragdoll!
			bForceVisibleOnScreen = true;
		}
	}

	////////////////////
	// Calculate a score based on distance
	////////////////////

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	Vector3 vCamOffset;
	if(!bPedIsLocalPlayer)
	{
		vCamOffset = vPedPosition - camInterface::GetPos();
		vCamOffset.z *= DIST_SCORE_ZOFFSET_MULT;

		// need to scale the distance to the camera down by the lodding distance multiplier. This takes into account things like sniper scopes.
		float fLodScale = g_LodScale.GetGlobalScale();
		if (fLodScale > 0.0f)
		{
			vCamOffset /= fLodScale;
		}
	}
	else
	{
		vCamOffset.Zero();
	}

	Vector3 vPlayerOffset = vPedPosition - CGameWorld::FindLocalPlayerCoors();
	vPlayerOffset.z *= DIST_SCORE_ZOFFSET_MULT;

	float fDistanceScore = rage::Min(vCamOffset.Mag2(), vPlayerOffset.Mag2());
	if(fDistanceScore > 0.0f)
	{
		fDistanceScore = rage::Sqrtf(fDistanceScore);
	}

	if(!bForceVisibleOnScreen && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen) && PHSIM->GetSleepEnabled())
	{
		fDistanceScore *= DIST_SCORE_OFFSCREEN_MULT;
	}

	fDistanceScore = RampValue(fDistanceScore, DIST_SCORE_DEFAULT_DIST, DIST_SCORE_SCALE_DIST + DIST_SCORE_DEFAULT_DIST, 1.0f, SCORES_THRESHOLD);

	float fSpeedScore = 0.0f;
	// Do we have a high velocity relative to the ground?
	const phCollider* pCollider = pPed->GetCollider();
	if (pCollider != NULL)
	{
		Vec3V vPedSpeed = Subtract(pCollider->GetVelocity(), pCollider->GetReferenceFrameVelocity());
		vPedSpeed.SetZf(vPedSpeed.GetZf() * SPEED_SCORE_ZOFFSET_MULT);
		ScalarV fSpeed2 = MagSquared(vPedSpeed);
		if (fSpeed2.Getf() > 0.0f)
		{
			fSpeedScore = Sqrt(fSpeed2).Getf();
		}
	}

	if(!bForceVisibleOnScreen && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen) && PHSIM->GetSleepEnabled())
	{
		fSpeedScore *= SPEED_SCORE_OFFSCREEN_MULT;
	}

	// SC: Not sure if this one is still really necessary - I believe this was implemented before a lot of other logic/fixes were in place...
	if (pPed->GetFrameCollisionHistory() != NULL)
	{
		// If we're being hit by a vehicle and either we're visible or the vehicle is visible then bump our priority
		const CCollisionRecord* pCollisionRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_VEHICLE);
		if (pCollisionRecord != NULL && pCollisionRecord->m_pRegdCollisionEntity != NULL && pCollisionRecord->m_pRegdCollisionEntity->GetIsTypeVehicle())
		{
			const CEntity* pVisibleEntity = NULL;
			if (pPed->GetIsVisibleInSomeViewportThisFrame())
			{
				pVisibleEntity = pPed;
			}
			else if (static_cast<const CVehicle*>(pCollisionRecord->m_pRegdCollisionEntity.Get())->GetIsVisibleInSomeViewportThisFrame())
			{
				pVisibleEntity = pCollisionRecord->m_pRegdCollisionEntity.Get();
			}

			if (pVisibleEntity != NULL)
			{
				// Test against the game viewport.
				Vector3 vCenter;
				pVisibleEntity->GetBoundCentre(vCenter);
				float fRadius = pVisibleEntity->GetBoundRadius();
				if (camInterface::IsSphereVisibleInGameViewport(vCenter, fRadius))
				{
					Vector3 vLocalSpeed = static_cast<const CVehicle*>(pCollisionRecord->m_pRegdCollisionEntity.Get())->GetLocalSpeed(pCollisionRecord->m_OtherCollisionPos, true, pCollisionRecord->m_OtherCollisionComponent);
					if (vLocalSpeed.Mag2() > SMALL_FLOAT)
					{
						vLocalSpeed.Normalize();
						float fDotSpeed = pCollisionRecord->m_MyCollisionNormal.Dot(vLocalSpeed);
						if (fDotSpeed > 0.0f)
						{
							fSpeedScore = rage::Max(fDotSpeed * SPEED_SCORE_SCALE_SPEED, fSpeedScore);
						}
					}
				}
			}
		}
	}

	fSpeedScore = RampValue(fSpeedScore, 0.0f, SPEED_SCORE_DEFAULT_SPEED, 1.0f, SPEED_SCORE_SCALE_SPEED);

	// otherwise take
	return fDistanceScore * fSpeedScore;
}

//////////////////////////////////////////////////////////////////////////
float CTaskNMBehaviour::CalcRagdollScore(CPed* pPed, eRagdollTriggerTypes nTrigger, CEntity* pEntityResponsible, float fPushValue)
//////////////////////////////////////////////////////////////////////////
{
	float fEventScore = CalcRagdollEventScore(pPed, nTrigger, pEntityResponsible, fPushValue);
	float fMultiplierScore = CalcRagdollMultiplierScore(pPed, nTrigger);

	return fEventScore * fMultiplierScore;
}

bool CTaskNMBehaviour::GetIsStuckOnVehicle()
{
	return (m_ContinuousContactTime > sm_Tunables.m_StuckOnVehicleMaxTime && GetPed()->GetPedResetFlag(CPED_RESET_FLAG_RagdollOnVehicle));
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::StartNetworkGame()
//////////////////////////////////////////////////////////////////////////
{
	GetRagdollPool(kRagdollPoolLocalPlayer).SetMaxRagdolls( sm_Tunables.m_ReserveLocalPlayerNmAgentMp ? 1 : 0 );
	GetRagdollPool(kRagdollPoolNmGameplay).SetMaxRagdolls(  sm_Tunables.m_MaxGameplayNmAgentsMp );
	GetRagdollPool(kRagdollPoolRageRagdoll).SetMaxRagdolls(  sm_Tunables.m_MaxRageRagdollsMp );
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBehaviour::EndNetworkGame()
//////////////////////////////////////////////////////////////////////////
{
	GetRagdollPool(kRagdollPoolLocalPlayer).SetMaxRagdolls( sm_Tunables.m_ReserveLocalPlayerNmAgent ? 1 : 0 );
	GetRagdollPool(kRagdollPoolNmGameplay).SetMaxRagdolls(  sm_Tunables.m_MaxGameplayNmAgents );
	GetRagdollPool(kRagdollPoolRageRagdoll).SetMaxRagdolls(  sm_Tunables.m_MaxRageRagdolls );
}
