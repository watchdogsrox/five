
#include "script/script_debug.h"

// Framework headers
#include "fwdebug/debugbank.h"
#include "fwutil/idgen.h"
#include "input/keyboard.h"
#include "input/keys.h"

// Game headers
#include "animation/debug/animdebug.h"
#include "animation/debug/AnimPlacementTool.h"
#include "camera/CamInterface.h"
#include "control/restart.h"
#include "debug/DebugScene.h"
#include "game/config.h"
#include "Peds/ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedFactory.h"
#include "Peds/pedpopulation.h"
#include "physics/physics.h"
#include "SaveLoad/GenericGameStorage.h"
#include "scene/world/GameWorld.h"
#include "scene/entities/compEntity.h"
#include "script/commands_debug.h"
#include "script/script.h"
#include "script/script_areas.h"
#include "script/script_cars_and_peds.h"
#include "script/script_channel.h"
#include "streaming/streaming.h"
#include "Vehicles/VehicleFactory.h"


#if __BANK

PARAM(scriptdebugger, "[script_debug] Activate script debugger");
PARAM(scriptdebugger_files, "[script_debug] comma-separated list of script files (without the .sc extension) that you want to debug");
PARAM(script_masks, "[script_debug] outputs a message whenever a script calls a mask command");

bool CScriptDebug::ms_bDisplayRunningScripts = false;
bool CScriptDebug::ms_bDisplayNumberOfScriptResources = false;
bool CScriptDebug::ms_bPrintCurrentContentsOfCleanupArray = false;

bool CScriptDebug::ms_bAutoSaveEnabled = true;
bool CScriptDebug::ms_bPerformAutoSave = false;
bool CScriptDebug::ms_bFadeInAfterLoadReadOnly = true;
bool CScriptDebug::ms_bDrawDebugLinesAndSpheres = false;
bool CScriptDebug::ms_bDebugMaskCommands = false;
bool CScriptDebug::ms_bPrintSuppressedCarModels = false;
bool CScriptDebug::ms_bPrintSuppressedPedModels = false;
bool CScriptDebug::ms_bPrintRestrictedPedModels = false;
bool CScriptDebug::ms_bOutputListOfAllScriptSpritesWhenLimitIsHit = false;

bool CScriptDebug::ms_bOutputScriptDisplayTextCommands = false;
s32 CScriptDebug::ms_NumberOfFramesToOutputScriptDisplayTextCommands = -1;

bool CScriptDebug::ms_bWriteNetworkCommandsToScriptLog = false;
bool CScriptDebug::ms_bDisplayNetworkCommandsInConsole = false;

bool CScriptDebug::ms_bFadeInAfterDeathArrestReadOnly = true;
bool CScriptDebug::ms_bPauseDeathArrestRestartReadOnly = false;
bool CScriptDebug::ms_bIgnoreNextRestartReadOnly = false;

bool CScriptDebug::ms_bWriteIS_NETWORK_SESSIONCommand = false;
bool CScriptDebug::ms_bWriteNETWORK_HAVE_SUMMONSCommand = false;
bool CScriptDebug::ms_bWriteIS_NETWORK_GAME_RUNNINGCommand = false;
bool CScriptDebug::ms_bWriteIS_NETWORK_PLAYER_ACTIVECommand = false;
bool CScriptDebug::ms_bWritePLAYER_WANTS_TO_JOIN_NETWORK_GAMECommand = false;

bool CScriptDebug::ms_bEnablePedDragging = false;
char CScriptDebug::ms_CreatePos[256] = "\0";

CScriptDebugPedHandler CScriptDebug::ms_PedHandler;
#if RSG_PC
bool CScriptDebug::ms_bFakeTampering = false;
#endif

CScriptDebugVehicleHandler CScriptDebug::ms_VehicleHandler;

CScriptDebugObjectHandler CScriptDebug::ms_ObjectHandler;

CScriptDebugLocateHandler CScriptDebug::ms_LocateHandler;

Vector3 CScriptDebug::ms_vClickedPos = VEC3_ZERO;
Vector3 CScriptDebug::ms_vClickedNormal = VEC3_ZERO;
Vector3 CScriptDebug::ms_vClickedPos2 = VEC3_ZERO;
Vector3 CScriptDebug::ms_vClickedNormal2 = VEC3_ZERO;
float CScriptDebug::ms_fPosFromCameraFront = 10.0f;
bool CScriptDebug::ms_bUseLineofSight = false;

char CScriptDebug::ms_FadeCommandTextContents[MAX_LENGTH_OF_FADE_COMMAND_TEXT_WIDGET];

fwDebugBank *CScriptDebug::ms_pBank = NULL;

const char *pNameOfScriptDebuggerBank = "Script Debugger";
bkCombo *CScriptDebug::ms_pComboBoxOfScriptThreads = NULL;
atArray<const char*> CScriptDebug::sm_ThreadNamesForDebugger;
s32 CScriptDebug::sm_ArrayIndexOfThreadToAttachDebuggerTo = 0;
bool CScriptDebug::sm_ComboBoxOfThreadNamesNeedsUpdated = false;


CScriptWidgetTree::CScriptWidgetNode *CScriptWidgetTree::ms_pCurrentWidgetNode = NULL;

CScriptDebugTextWidgets CScriptDebug::ms_TextWidgets;
CScriptDebugBitFieldWidgets CScriptDebug::ms_BitFieldWidgets;

atArray<CDebugAttachmentTool> CScriptDebug::ms_attachmentTools;
bkGroup* CScriptDebug::ms_pAttachmentGroup = NULL;

const char *CScriptDebug::ms_pScriptDebuggerPath = NULL;
atArray<atString> CScriptDebug::ms_ArrayOfNamesOfScriptsToBeDebugged;

char CScriptDebug::ms_ScriptThreadIdForDebugging[8];


#if SCRIPT_PROFILING
bool CScriptDebug::ms_bDisplayProfileOverview = false;
s32 CScriptDebug::ms_ProfileDisplayStartRow = 9;
#endif	//	SCRIPT_PROFILING

#endif	//	__BANK


#if !__FINAL
bool CScriptDebug::ms_bDbgFlag = false;					// If true stuff gets printed out

PARAM(displayScriptRequests, "[script_debug] print to the TTY whenever a script calls REQUEST_SCRIPT or SET_SCRIPT_AS_NO_LONGER_NEEDED");
bool CScriptDebug::ms_bDisplayScriptRequests = false;

bool CScriptDebug::ms_bDisableDebugCamAndPlayerWarping = false;	//	For build for Image Metrics

//	Some variables for controlling what gets displayed in the script model usage debug text
//	bool CScriptDebug::ms_bDisplayModelUsagePerScript = true;
//	bool CScriptDebug::ms_bCountModelUsage = true;
//	bool CScriptDebug::ms_bCountAnimUsage = true;
//	bool CScriptDebug::ms_bCountTextureUsage = true;

bool CScriptDebug::sm_bPlayerCoordsHaveBeenOverridden = false;

#endif	//	!__FINAL

#if __SCRIPT_MEM_DISPLAY
bool CScriptDebug::ms_bDisplayScriptMemoryUsage = false;
bool CScriptDebug::ms_bDisplayDetailedScriptMemoryUsage = false;
bool CScriptDebug::ms_bDisplayDetailedScriptMemoryUsageFiltered = false;
bool CScriptDebug::ms_bExcludeScriptFromMemoryUsage = false;
#endif	//	__SCRIPT_MEM_DISPLAY

ANIM_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
//	Debug attachment tool implementation
//////////////////////////////////////////////////////////////////////////

#if __BANK
void CDebugAttachmentTool::Update()
{
	if (m_bNeedsRebuild)
	{
		RegenerateWidgets();
	}

	//render the offsets here

	Vector3 startPos(VEC3_ZERO);
	Vector3 endPos(VEC3_ZERO);

	if (m_parent)
	{
		if (GetParentBone() > -1)
		{
			// get the parent bone position
			Matrix34 boneMtx(M34_IDENTITY);
			m_parent->GetSkeleton()->GetGlobalMtx(GetParentBone(), RC_MAT34V(boneMtx));

			startPos = boneMtx.d;
		}
		else
		{
			// get the parent entity matrix
			startPos = VEC3V_TO_VECTOR3(m_parent->GetTransform().GetPosition());
		}

		if (GetChildBone() > -1)
		{
			// get the child bone position
			Matrix34 boneMtx(M34_IDENTITY);
			m_child->GetSkeleton()->GetGlobalMtx(GetChildBone(), RC_MAT34V(boneMtx));
			endPos = boneMtx.d;
		}
		else
		{
			// get the child entity matrix
			endPos = VEC3V_TO_VECTOR3(m_child->GetTransform().GetPosition());
		}

		grcDebugDraw::Line(startPos, endPos, Color_yellow, Color_yellow);
	}


}

void CDebugAttachmentTool::AddWidgets(bkBank* pBank, const char * title)
{

	if (m_child)
	{
		m_bankInstances.PushAndGrow(pBank->PushGroup(title));
		{
			CreateWidgets(pBank);
		}
		pBank->PopGroup();
	}
	
}

void CDebugAttachmentTool::CreateWidgets(bkBank* pBank)
{
	pBank->AddVector("Position offset", &m_positionOffset, -10000.0f, 10000.0f,0.1f,
		datCallback(MFA(CDebugAttachmentTool::UpdatePositionOffset), (datBase*)this));
	pBank->AddSlider("Heading", &m_rotationOffset.z, -180.0f,180.0f,1.0f, 
		datCallback(MFA(CDebugAttachmentTool::UpdateRotationOffset), (datBase*)this));
	pBank->AddSlider("Pitch", &m_rotationOffset.x, -180.0f,180.0f,1.0f, 
		datCallback(MFA(CDebugAttachmentTool::UpdateRotationOffset), (datBase*)this));
	pBank->AddSlider("Roll", &m_rotationOffset.y, -180.0f,180.0f,1.0f, 
		datCallback(MFA(CDebugAttachmentTool::UpdateRotationOffset), (datBase*)this));

//	static const char * s_noBonesMessage = "No bones";

	if (m_parentBoneList.GetCount()>0) // If bones are available, create a selectable list of them
	{
		//add a combo to track the bone names
		pBank->AddCombo("Parent bone", &m_parentBone, m_parentBoneList.GetCount(), &m_parentBoneList[0]);
	}

	if (m_bAttachPhysically) //child bones are only available when attaching physically
	{
		if (m_childBoneList.GetCount()>0)
		{
			pBank->AddCombo("Child bone", &m_childBone, m_childBoneList.GetCount(), &m_childBoneList[0]);
		}
	}
	else
	{
		if (m_childBoneList.GetCount()>0)
		{
			pBank->AddCombo("Child bone", &m_childBone, m_childBoneList.GetCount(), 
				&m_childBoneList[0], datCallback(MFA(CDebugAttachmentTool::BuildOffsetsFromChildBone), (datBase*)this));
		}
	}


	if (m_child->GetIsTypePed()) //peds have special auto detachment options
	{
		pBank->AddToggle("Detach on death", &m_bDetachOnDeath);
		pBank->AddToggle("Detach on ragdoll", &m_bDetachOnRagdoll);
	}
	else
	{
		if (m_parent)
		{
			if (m_parent->GetIsTypePed())
			{
				pBank->AddToggle("Detach on parent death", &m_bDetachOnDeath , datCallback(MFA(CDebugAttachmentTool::Attach), (datBase*)this));
			}
		}
	}

	if (!m_parent->GetIsTypePed()) //don't support physical attachment to peds
	{
		pBank->AddToggle("Attach physically", &m_bAttachPhysically , datCallback(MFA(CDebugAttachmentTool::TriggerRebuild), (datBase*)this));
		if (m_bAttachPhysically)
		{
			pBank->PushGroup("Physical attachment options", false);
			{
				pBank->AddToggle("Constrain rotation", &m_bPhysicalConstrainRotation);
				if (m_parent)
				{
					pBank->AddToggle("Child collides with parent", &m_bChildCollidesWithParent);
					pBank->AddVector("Parent pivot offset", &m_physicalPivotOffset, -10000.0f, 10000.0f,0.1f);
				}
				pBank->AddSlider("Attachment strength", &m_strength, 0.0f,5000.0f, 1.0f);
			}
			pBank->PopGroup(); // "Physical attachment options"
		}
	}
	
	pBank->AddButton("Apply changes", datCallback(MFA(CDebugAttachmentTool::Attach), (datBase*)this));

	pBank->AddButton("Get script call", datCallback(MFA(CDebugAttachmentTool::OutputScript), (datBase*)this));

	m_bNeedsRebuild = false;
}

void CDebugAttachmentTool::TriggerRebuild()
{
	m_bNeedsRebuild = true;
}

void CDebugAttachmentTool::OutputScript()
{
	CScriptDebug::OutputScript(GetScriptCall());
}

void CDebugAttachmentTool::RemoveWidgets()
{
	//remove the widget groups we've created
	while (m_bankInstances.GetCount()>0)
	{
		m_bankInstances.Top()->Destroy();
		m_bankInstances.Top() = NULL;
		m_bankInstances.Pop();
	}
}

void CDebugAttachmentTool::RegenerateWidgets()
{
	for (s32 i=0 ; i<m_bankInstances.GetCount(); i++)
	{

		// find the parent bank
		bkBank* pBank = FindBank(m_bankInstances[i]);

		if (pBank)
		{
			//destroy all the groups children
			DestroyAllChildren(m_bankInstances[i]);

			// set the current group
			pBank->SetCurrentGroup(*m_bankInstances[i]);
			
			// regenerate the widgets
			CreateWidgets(pBank);

			//unset the current group
			pBank->UnSetCurrentGroup(*m_bankInstances[i]);
		
		}
	}
}

void CDebugAttachmentTool::BuildOffsetsFromChildBone()
{
	// Set the position and orientation based on the 
	// current object space matrix of the selected child bone

	
	s16 childBoneId = GetChildBone();

	if (m_child->GetSkeleton())
	{	
		Matrix34 boneMtx(M34_IDENTITY);
		
		if (childBoneId>-1)
		{
			boneMtx.Set( RCC_MATRIX34(m_child->GetSkeleton()->GetObjectMtx(childBoneId)) );
			boneMtx.Inverse();
			boneMtx.ToEulersXYZ(m_rotationOffset);
			m_positionOffset = boneMtx.d;
			m_rotationOffset*=RtoD; // convert to degrees
		}

		Attach();
	}
}

bkBank* CDebugAttachmentTool::FindBank(bkWidget* pWidget)
{
	while (pWidget)
	{
		if (pWidget)
		{
			bkBank* pBank = dynamic_cast<bkBank*>(pWidget);

			if (pBank)
			{
				return pBank;
			}
		}	
		
		pWidget = pWidget->GetParent();
	}

	return NULL;
}

void CDebugAttachmentTool::DestroyAllChildren(bkWidget* pWidget)
{
	while (pWidget->GetChild())
	{
		pWidget->GetChild()->Destroy();
	}
}

void CDebugAttachmentTool::Shutdown()
{
	Detach();

	m_child = NULL;
	m_parent = NULL;

	ClearSettings();

	m_bankInstances.clear();
}

void CDebugAttachmentTool::ClearSettings()
{
	m_parentBone=0;		//Index for our bone selector combo boxes
	m_childBone=0;

	m_rotationOffset.Zero();
	m_positionOffset.Zero();

	m_bDetachOnDeath = false;
	m_bDetachOnRagdoll = false;

	m_bAttachPhysically = false;

	m_bPhysicalConstrainRotation = false;
	m_strength = 500.0f;					// Higher values make the physical attachment harder to break
	m_physicalPivotOffset.Zero();		// The offset on the target object to pivot about when attaching objects physically
	m_bChildCollidesWithParent = false;

	m_parentBoneList.clear();	//pointers to the bone names for the current parent
	m_childBoneList.clear();	//pointers to the bone names for the current child
}

void CDebugAttachmentTool::LoadBoneList()
{
	m_parentBoneList.clear();

	if (m_parent)
	{
		m_parentBoneList.PushAndGrow("Entity");
		if (m_parent->GetSkeleton())
		{
			const crSkeletonData& SkelData = m_parent->GetSkeletonData();

			for (s32 i=0;i<SkelData.GetNumBones();i++)
			{
				m_parentBoneList.PushAndGrow(SkelData.GetBoneData(i)->GetName());
			}
		}
	}

	m_childBoneList.clear();

	if (m_child)
	{
		m_childBoneList.PushAndGrow("Entity");
		if (m_child->GetSkeleton())
		{
			const crSkeletonData& SkelData = m_child->GetSkeletonData();

			for (s32 i=0;i<SkelData.GetNumBones();i++)
			{
				m_childBoneList.PushAndGrow(SkelData.GetBoneData(i)->GetName());
			}
		}
	}
}

void CDebugAttachmentTool::Initialise(CPhysical* pChild, CPhysical* pParent)
{
	animAssertf(pChild, "Can't initialise an attachment tool with a null child");
	if (pChild)
	{
		m_child = pChild;
		if (pParent)
		{
			//we're attaching to a parent
			m_parent = pParent;
		}
		
		ClearSettings();

		LoadBoneList();

		Attach();
	}
}

void CDebugAttachmentTool::UpdatePositionOffset()
{
	if (m_bAttachPhysically)
	{
		Attach();
	}
	else
	{
		m_child->SetAttachOffset(m_positionOffset);
	}

}

void CDebugAttachmentTool::UpdateRotationOffset()
{
	Quaternion quat;

	quat.FromEulers(DtoR*m_rotationOffset);

	if (m_bAttachPhysically)
	{
		Attach();
	}
	else
	{
		m_child->SetAttachQuat(quat);
	}
}

void CDebugAttachmentTool::Attach()
{	

	//make sure the child's not already attached to something
	Detach();

	//need to identify ped or other object
	if (m_child)
	{
		if (m_child->GetIsTypePed())
		{
			// ped attachment
			ScriptAttachPed();
		}
		else
		{
			//object and vehicle attachment
			ScriptAttachOther();
		}
	}
}

void CDebugAttachmentTool::Detach()
{
	if (m_child)
	{
		//detach the child if necessary
		if (m_child->GetIsAttached())
		{

			//cast to a ped if it is one
			if (m_child->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(m_child.Get());

				pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS | DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR);
			}

			m_child->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS | DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR);
		}
	}
}

const char * CDebugAttachmentTool::GetScriptCall()
{
	static char scriptCall[256] = "";

	if (m_child->GetIsTypePed())
	{	//ped commands

		if (m_parent)
		{
			if (m_parent->GetIsTypePed())
			{	//ped to ped commands
				s16 ParentBone = GetParentBone(); 
				eAnimBoneTag BoneTag = BONETAG_INVALID; 

				if(ParentBone !=-1)
				{
					BoneTag = static_cast<CPed*>(m_parent.Get())->GetBoneTagFromBoneIndex(ParentBone); 
				}
				
				sprintf(scriptCall, "\r\nATTACH_PED_TO_PED ( ENTER_CHILD_PED, ENTER_PARENT_PED, %d, << %.3f, %.3f, %.3f >>, %.3f, %.3f, %s, %s )", 
					
					BoneTag, // TODO - convert this into the script enum name
					m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
					m_rotationOffset.z, m_rotationOffset.z, 
					CScriptDebug::GetScriptBool( m_bDetachOnDeath ),
					CScriptDebug::GetScriptBool( m_bDetachOnRagdoll )
					);
			}
			else if (m_parent->GetIsTypeVehicle())
			{	//ped to vehicle commands
				s16 ChildBone = GetChildBone(); 
				eAnimBoneTag BoneTag = BONETAG_INVALID; 

				if(ChildBone != -1)
				{
					BoneTag = static_cast<CPed*>(m_child.Get())->GetBoneTagFromBoneIndex(GetChildBone());
				}
				
				if (m_bAttachPhysically)
				{
					sprintf(scriptCall, "\r\nATTACH_PED_TO_VEHICLE_PHYSICALLY ( ENTER_PED, ENTER_VEHICLE, %d, %d, << %.3f, %.3f, %.3f >>, %.3f, %s, %s )", 
						 BoneTag, GetParentBone(),
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_strength, CScriptDebug::GetScriptBool( m_bPhysicalConstrainRotation ),
						CScriptDebug::GetScriptBool( m_bChildCollidesWithParent )
						);
				}
				else
				{
					sprintf(scriptCall, "\r\nATTACH_PED_TO_VEHICLE ( ENTER_PED , ENTER_VEHICLE, %d, %d, << %.3f, %.3f, %.3f >>, %.3f, %.3f, %s, %s)", 
						BoneTag, GetParentBone(),
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_rotationOffset.z, m_rotationOffset.z, 
						CScriptDebug::GetScriptBool( m_bDetachOnDeath ),
						CScriptDebug::GetScriptBool( m_bDetachOnRagdoll )
						);
				}
			}
			else
			{	//ped to object commands
				if (m_bAttachPhysically)
				{
					s16 ChildBone = GetChildBone(); 
					eAnimBoneTag BoneTag = BONETAG_INVALID; 

					if(ChildBone != -1)
					{
						BoneTag = static_cast<CPed*>(m_child.Get())->GetBoneTagFromBoneIndex(GetChildBone());
					}
					
					sprintf(scriptCall, "\r\nATTACH_PED_TO_OBJECT_PHYSICALLY ( ENTER_PED, ENTER_OBJECT, %d, %d, << %.3f, %.3f, %.3f >>, %.3f, %s, %s )", 
						BoneTag, GetParentBone(),
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_strength, CScriptDebug::GetScriptBool( m_bPhysicalConstrainRotation ),
						CScriptDebug::GetScriptBool( m_bChildCollidesWithParent )
						);
				}
				else
				{
					sprintf(scriptCall, "\r\nATTACH_PED_TO_OBJECT ( ENTER_PED , ENTER_OBJECT, %d, << %.3f, %.3f, %.3f >>, %.3f, %.3f, %s, %s)", 
						GetParentBone(),
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_rotationOffset.z, m_rotationOffset.z, 
						CScriptDebug::GetScriptBool( m_bDetachOnDeath ),
						CScriptDebug::GetScriptBool( m_bDetachOnRagdoll )
						);
				}
			}		
		}
		else
		{	//attach ped to world
			if (m_bAttachPhysically)
			{
				s16 ChildBone = GetChildBone(); 
				eAnimBoneTag BoneTag = BONETAG_INVALID; 

				if(ChildBone != -1)
				{
					BoneTag = static_cast<CPed*>(m_child.Get())->GetBoneTagFromBoneIndex(GetChildBone());
				}
				
				sprintf(scriptCall, "\r\nATTACH_PED_TO_WORLD_PHYSICALLY  (ENTER_PED, %d , , << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>, %s)", 
					BoneTag,
					m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
					0.0f, 0.0f, 0.0f,
					CScriptDebug::GetScriptBool( m_bFixRotation )
					);
			}

		}
		
	}
	else if (m_child->GetIsTypeVehicle())
	{	//vehicle commands
		if (m_parent)
		{
			if (m_parent->GetIsTypeVehicle())
			{	//vehicle to vehicle commands

				if (m_bAttachPhysically)
				{
						sprintf(scriptCall, "\r\nATTACH_VEHICLE_TO_VEHICLE_PHYSICALLY( CHILD_VEHICLE, PARENT_VEHICLE, %d, %d, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>, %.3f, %s)", 
						GetChildBone(), GetParentBone(), 
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_physicalPivotOffset.x, m_physicalPivotOffset.y, m_physicalPivotOffset.z,
						m_rotationOffset.x, m_rotationOffset.y, m_rotationOffset.z,
						m_strength, CScriptDebug::GetScriptBool( m_bPhysicalConstrainRotation )
						);				
				}
				else
				{
					sprintf(scriptCall, "\r\nATTACH_VEHICLE_TO_VEHICLE( CHILD_VEHICLE, PARENT_VEHICLE, %d, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>)", 
						GetParentBone(), 
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_rotationOffset.x, m_rotationOffset.y, m_rotationOffset.z
						);
				}
	
			}
			else
			{	//vehicle to object commands

				if (m_bAttachPhysically)
				{ //use object to object for this (there's no vehicle to object physically command)
					sprintf(scriptCall, "\r\nATTACH_OBJECT_TO_OBJECT_PHYSICALLY( CHILD_VEHICLE, PARENT_VEHICLE, %d, %d, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>, %.3f, %s)", 
						GetChildBone(), GetParentBone(), 
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_physicalPivotOffset.x, m_physicalPivotOffset.y, m_physicalPivotOffset.z,
						m_rotationOffset.x, m_rotationOffset.y, m_rotationOffset.z,
						m_strength, CScriptDebug::GetScriptBool( m_bPhysicalConstrainRotation )
						);	
				}
				else
				{ 
					sprintf(scriptCall, "\r\nATTACH_VEHICLE_TO_OBJECT( ENTER_VEHICLE, ENTER_OBJECT, %d, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>)", 
						GetParentBone(), 
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_rotationOffset.x, m_rotationOffset.y, m_rotationOffset.z
						);
				}
			}		
		}
	}
	else
	{	//object commands
		if (m_parent)
		{
			if (m_parent->GetIsTypePed())
			{ //object to ped commands
				s16 ParentBone = GetParentBone(); 
				eAnimBoneTag BoneTag = BONETAG_INVALID; 

				if(ParentBone !=-1)
				{
					BoneTag = static_cast<CPed*>(m_parent.Get())->GetBoneTagFromBoneIndex(ParentBone); 
				}

				sprintf(scriptCall, "\r\nATTACH_OBJECT_TO_PED ( ENTER_OBJECT, ENTER_PED, %d, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>, %s)", 
					BoneTag, // TODO - convert this into the script enum name
					m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
					m_rotationOffset.x, m_rotationOffset.y, m_rotationOffset.z,
					CScriptDebug::GetScriptBool( m_bDetachOnRagdoll )
					);
			}
			else if (m_parent->GetIsTypeVehicle())
			{ //object to vehicle commands
				if (m_bAttachPhysically)
				{
					sprintf(scriptCall, "\r\nATTACH_OBJECT_TO_VEHICLE_PHYSICALLY( ENTER_OBJECT, ENTER_VEHICLE, %d, %d, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>, %.3f, %s)", 
						GetParentBone(), GetChildBone(), 
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_physicalPivotOffset.x, m_physicalPivotOffset.y, m_physicalPivotOffset.z,
						m_rotationOffset.x, m_rotationOffset.y, m_rotationOffset.z,
						m_strength, CScriptDebug::GetScriptBool( m_bPhysicalConstrainRotation )
						);				
				}
				else
				{
					sprintf(scriptCall, "\r\nATTACH_OBJECT_TO_VEHICLE( ENTER_OBJECT, ENTER_VEHICLE, %d, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>)", 
						GetParentBone(), 
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_rotationOffset.x, m_rotationOffset.y, m_rotationOffset.z
						);
				}
			}
			else
			{ //generic object to object attachment
				if (m_bAttachPhysically)
				{
					sprintf(scriptCall, "\r\nATTACH_OBJECT_TO_OBJECT_PHYSICALLY( CHILD_OBJECT, PARENT_OBJECT, %d, %d, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>, %.3f, %s)", 
						GetChildBone(), GetParentBone(), 
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_physicalPivotOffset.x, m_physicalPivotOffset.y, m_physicalPivotOffset.z,
						m_rotationOffset.x, m_rotationOffset.y, m_rotationOffset.z,
						m_strength, CScriptDebug::GetScriptBool( m_bPhysicalConstrainRotation )
						);	
				}
				else
				{
					sprintf(scriptCall, "\r\nATTACH_OBJECT_TO_OBJECT( CHILD_OBJECT, PARENT_OBJECT, %d, << %.3f, %.3f, %.3f >>, << %.3f, %.3f, %.3f >>)", 
						GetParentBone(), 
						m_positionOffset.x, m_positionOffset.y, m_positionOffset.z,
						m_rotationOffset.x, m_rotationOffset.y, m_rotationOffset.z
						);
				}
			}
		}
	}

	return &scriptCall[0];

}



void CDebugAttachmentTool::ScriptAttachPed()
{
	
	CPed* pPed = static_cast<CPed*>(m_child.Get());

	if (pPed)
	{
		s16 childBoneId = GetChildBone();
		s16 parentBoneId = GetParentBone();
		u32 attachFlags = 0;

		if (!m_parent)
		{
			//attach the ped to the world physically (There's no script command for doing this non physically)
			m_bAttachPhysically = true;

			pPed->AttachToWorldUsingPhysics(childBoneId, m_bPhysicalConstrainRotation, VEC3_ZERO, &m_positionOffset, m_strength);
		}
		else
		{			
			if  (!m_bAttachPhysically)
			{
				attachFlags = ATTACH_STATE_PED|ATTACH_FLAG_INITIAL_WARP;
				if(m_bDetachOnDeath)
					attachFlags |= ATTACH_FLAG_AUTODETACH_ON_DEATH;
				if(m_bDetachOnRagdoll)
					attachFlags |= ATTACH_FLAG_AUTODETACH_ON_RAGDOLL;

				pPed->AttachPedToPhysical(m_parent, parentBoneId, attachFlags, &m_positionOffset, NULL, m_rotationOffset.z*DtoR, m_rotationOffset.z*DtoR);
			}
			else
			{
				attachFlags = ATTACH_STATE_RAGDOLL|ATTACH_FLAG_POS_CONSTRAINT;
				if(m_bPhysicalConstrainRotation)
					attachFlags |= ATTACH_FLAG_ROT_CONSTRAINT;
				if(m_bChildCollidesWithParent)
					attachFlags |= ATTACH_FLAG_DO_PAIRED_COLL;

				pPed->AttachToPhysicalUsingPhysics(m_parent, parentBoneId, childBoneId, attachFlags, m_positionOffset, m_strength);
			}

		}
	}
}

void CDebugAttachmentTool::ScriptAttachOther()
{

	s16 childBoneId = GetChildBone();
	s16 parentBoneId = GetParentBone();
	u32 attachFlags = 0;

	Quaternion quat;
	quat.FromEulers(DtoR*m_rotationOffset);
	
	if (m_parent)
	{

		if (m_bAttachPhysically)
		{
			attachFlags = ATTACH_STATE_PHYSICAL|ATTACH_FLAG_INITIAL_WARP|ATTACH_FLAG_POS_CONSTRAINT;
			if(m_bPhysicalConstrainRotation)
				attachFlags |= ATTACH_FLAG_ROT_CONSTRAINT;

			m_child->AttachToPhysicalUsingPhysics(m_parent, parentBoneId, childBoneId, attachFlags, &m_positionOffset, &quat, &m_physicalPivotOffset, m_strength);
		}
		else
		{
			attachFlags = ATTACH_STATE_BASIC|ATTACH_FLAG_INITIAL_WARP;

			if (m_parent->GetIsTypePed())
			{
				if (m_bDetachOnRagdoll)
				{
					attachFlags |= ATTACH_FLAG_AUTODETACH_ON_RAGDOLL;
				}
			}

			m_child->AttachToPhysicalBasic(m_parent, parentBoneId, attachFlags, &m_positionOffset, &quat);
		}
	}
}
#endif	//	__BANK

//////////////////////////////////////////////////////////////////////////
//	End of Debug attachment tool implementation
//////////////////////////////////////////////////////////////////////////


#if __BANK

//////////////////////////////////////////////////////////////////////////
//	CScriptWidgetNode
//////////////////////////////////////////////////////////////////////////

CScriptWidgetTree::CScriptWidgetNode::CScriptWidgetNode(u32 UniqueId, eWidgetType Type)
: m_UniqueId(UniqueId), 
	m_WidgetType(Type)
{
	m_pParent = NULL;
	m_pChild = NULL;
	m_pSibling = NULL;
}

CScriptWidgetTree::CScriptWidgetNode::~CScriptWidgetNode()
{
	RemoveSelf();
	ClearChildrenAndData();
}

void CScriptWidgetTree::CScriptWidgetNode::ClearChildrenAndData()
{
	while(m_pChild)
	{
		scriptAssertf(WIDGETTYPE_GROUP == m_WidgetType, "CScriptWidgetNode::ClearChildrenAndData - only widget groups should have child nodes");
		delete m_pChild;
	}
	ClearData();
}

void CScriptWidgetTree::CScriptWidgetNode::RemoveSelf()
{
	if (m_pParent)
	{
		CScriptWidgetNode **owner = &m_pParent->m_pChild;
		while (*owner != this)
			owner = &(*owner)->m_pSibling;
		*owner = m_pSibling;
	}
	else
	{
		scriptAssertf(m_pSibling==NULL, "CScriptWidgetNode::RemoveSelf - didn't expect a widget node without a parent to have a sibling. This should be the top level widget group for the script");
	}
	m_pParent = m_pSibling = NULL;
}

void CScriptWidgetTree::CScriptWidgetNode::ClearData()
{
	switch (m_WidgetType)
	{
		case WIDGETTYPE_STANDARD :
		{
			scriptAssertf(0, "CScriptWidgetNode::ClearData - nodes of WIDGETTYPE_STANDARD should never appear in the widget tree");
//			bkWidget* pWidget = CScriptWidgetTree::GetWidgetFromUniqueWidgetId(m_UniqueId);
//			if(scriptVerifyf(pWidget != NULL, "CScriptWidgetNode::ClearData - Widget id is invalid"))
//			{
//				CScriptDebug::GetWidgetBank()->Remove(*pWidget);
//			}
		}
			break;

		case WIDGETTYPE_GROUP :
		{
			bkGroup* pGroup = static_cast<bkGroup*>(CScriptWidgetTree::GetWidgetFromUniqueWidgetId(m_UniqueId));
			if(scriptVerifyf(pGroup != NULL, "CScriptWidgetNode::ClearData - Widget group id is invalid"))
			{
				if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
				{
					CScriptDebug::GetWidgetBank()->DeleteGroup(*pGroup);
				}
			}
		}
			break;

		case WIDGETTYPE_TEXT :
		{
			bkText *pTextWidget = static_cast<bkText*>(CScriptWidgetTree::GetWidgetFromUniqueWidgetId(m_UniqueId));
			CScriptDebug::GetTextWidgets().ClearTextWidget(pTextWidget);
		}
			break;

		case WIDGETTYPE_BITFIELD :
		{
			bkGroup *pBitFieldWidgetGroup = static_cast<bkGroup*>(CScriptWidgetTree::GetWidgetFromUniqueWidgetId(m_UniqueId));
			CScriptDebug::GetBitFieldWidgets().ClearBitFieldWidget(pBitFieldWidgetGroup);
		}
			break;
	}

	g_uniqueObjectIDGenerator.FreeID(m_UniqueId);
	m_UniqueId = 0;
}

void CScriptWidgetTree::CScriptWidgetNode::AddChildNode(CScriptWidgetNode *pNewChild)
{
	scriptAssertf(!pNewChild->m_pParent, "CScriptWidgetNode::AddChildNode - parent of new child should be NULL");
	scriptAssertf(!pNewChild->m_pChild, "CScriptWidgetNode::AddChildNode - child of new child should be NULL");
	scriptAssertf(!pNewChild->m_pSibling, "CScriptWidgetNode::AddChildNode - sibling of new child should be NULL");

	pNewChild->m_pParent = this;
	pNewChild->m_pSibling = this->m_pChild;
	this->m_pChild = pNewChild;
}

CScriptWidgetTree::CScriptWidgetNode *CScriptWidgetTree::CScriptWidgetNode::FindWidget(u32 UniqueId)
{
	if (m_UniqueId == UniqueId)
	{
		return this;
	}

	CScriptWidgetNode *pReturnNode = NULL;

	if (m_pChild)
	{
		pReturnNode = m_pChild->FindWidget(UniqueId);
	}

	if (!pReturnNode)
	{
		if (m_pSibling)
		{
			pReturnNode = m_pSibling->FindWidget(UniqueId);
		}
	}

	return pReturnNode;
}

//////////////////////////////////////////////////////////////////////////
//	End of CScriptWidgetNode
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//	CScriptWidgetTree
//////////////////////////////////////////////////////////////////////////

CScriptWidgetTree::CScriptWidgetTree()
: m_pTopLevelWidget(NULL)
{
}

CScriptWidgetTree::~CScriptWidgetTree()
{
	scriptAssertf(m_pTopLevelWidget == NULL, "~CScriptWidgetTree - expected all script widgets to have been cleaned up by now");
	RemoveAllWidgetNodes();
}

CScriptWidgetTree::CScriptWidgetNode *CScriptWidgetTree::FindWidgetNodeWithThisUniqueId(u32 UniqueId)
{
	if (m_pTopLevelWidget)
	{
		return m_pTopLevelWidget->FindWidget(UniqueId);
	}

	return NULL;
}

u32 CScriptWidgetTree::AddWidget(bkWidget *pWidget, eWidgetType WidgetType, const char *ASSERT_ONLY(pScriptName))
{
	u32 ReturnUniqueId = 0;

	bool bAllowCreation = true;
	bool bTopLevelWidgetGroup = false;
	if (ms_pCurrentWidgetNode == NULL)
	{
		bAllowCreation = false;
		if (scriptVerifyf(m_pTopLevelWidget == NULL, "CScriptWidgetTree::AddWidget - SET_CURRENT_WIDGET_GROUP needs to be called before you can add to a pre-existing widget group for %s", pScriptName))
		{
			if (scriptVerifyf(WIDGETTYPE_GROUP == WidgetType, "CScriptWidgetTree::AddWidget - you must create a widget group to contain all other widgets for %s", pScriptName))
			{
				bAllowCreation = true;
				bTopLevelWidgetGroup = true;
			}
		}
	}

	if (bAllowCreation)
	{
		//	Widget groups and text widgets are referred to by script after they're created so they need to return a Unique ID.
		//	Bit field widgets require special cleanup in CScriptWidgetNode::ClearData() (as do widget groups and text widgets) 
		//	so they also need added to the widget tree.
		//	Bit field widgets only need a Unique ID because ClearData() calls GetWidgetFromUniqueWidgetId()
		if (WidgetType == WIDGETTYPE_STANDARD)
		{
			return 0;
		}

		ReturnUniqueId = g_uniqueObjectIDGenerator.GetID(pWidget);

		CScriptWidgetNode *pNewWidgetNode = rage_new CScriptWidgetNode(ReturnUniqueId, WidgetType);

		if (bTopLevelWidgetGroup)
		{
			m_pTopLevelWidget = pNewWidgetNode;
		}
		else
		{
			if (scriptVerifyf(ms_pCurrentWidgetNode, "CScriptWidgetTree::AddWidget - expected ms_pCurrentWidgetNode to be valid"))
			{
				ms_pCurrentWidgetNode->AddChildNode(pNewWidgetNode);
			}
		}

		if (WIDGETTYPE_GROUP == WidgetType)
		{
			ms_pCurrentWidgetNode = pNewWidgetNode;
		}
	}

	return ReturnUniqueId;
}

void CScriptWidgetTree::DeleteWidget(u32 UniqueId, bool bIsWidgetGroup, const char *ASSERT_ONLY(pScriptName))
{
	CScriptWidgetNode *pWidgetNodeToDelete = FindWidgetNodeWithThisUniqueId(UniqueId);
	if (scriptVerifyf(pWidgetNodeToDelete, "CScriptWidgetTree::DeleteWidget - failed to find a widget node with the specified Unique Id in %s", pScriptName))
	{
		if (bIsWidgetGroup)
		{
			scriptAssertf(WIDGETTYPE_GROUP == pWidgetNodeToDelete->GetWidgetType(), "CScriptWidgetTree::DeleteWidget - %s script called DELETE_WIDGET_GROUP but this isn't a widget group", pScriptName);
			if (pWidgetNodeToDelete == m_pTopLevelWidget)
			{
				m_pTopLevelWidget = NULL;
				scriptAssertf(ms_pCurrentWidgetNode == NULL, "CScriptWidgetTree::DeleteWidget - maybe missing a STOP_WIDGET_GROUP or a CLEAR_CURRENT_WIDGET_GROUP before calling DELETE_WIDGET_GROUP on the top level widget group in %s script", pScriptName);
				ms_pCurrentWidgetNode = NULL;
			}
		}
		else
		{	//	seems a bit pointless to check for widget groups, but not for text widgets and bit field widgets
			scriptAssertf(WIDGETTYPE_GROUP != pWidgetNodeToDelete->GetWidgetType(), "CScriptWidgetTree::DeleteWidget - %s script called DELETE_WIDGET but this is a widget group", pScriptName);
		}
		
		delete pWidgetNodeToDelete;
	}
}

void CScriptWidgetTree::SetCurrentWidgetGroup(u32 UniqueId, const char *ASSERT_ONLY(pScriptName))
{
	scriptAssertf(ms_pCurrentWidgetNode==NULL, "CScriptWidgetTree::SetCurrentWidgetGroup - %s - STOP_WIDGET_GROUP or CLEAR_CURRENT_WIDGET_GROUP missing before this call to SET_CURRENT_WIDGET_GROUP", pScriptName);

	bkGroup* pGroup = static_cast<bkGroup*>(GetWidgetFromUniqueWidgetId(UniqueId));
	if (scriptVerifyf(pGroup, "CScriptWidgetTree::SetCurrentWidgetGroup - %s failed to find a widget group with the unique ID", pScriptName))
	{
		if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
		{
			CScriptDebug::GetWidgetBank()->SetCurrentGroup(*pGroup);
		}

		CScriptWidgetNode *pWidgetNode = FindWidgetNodeWithThisUniqueId(UniqueId);
		if (scriptVerifyf(pWidgetNode, "CScriptWidgetTree::SetCurrentWidgetGroup - failed to find a widget node with the specified id in the tree for %s", pScriptName))
		{
			if (scriptVerifyf(WIDGETTYPE_GROUP == pWidgetNode->GetWidgetType(), "CScriptWidgetTree::SetCurrentWidgetGroup - widget node with the specified unique id is not a WIDGETTYPE_GROUP"))
			{
				ms_pCurrentWidgetNode = pWidgetNode;
			}
		}
	}
}

void CScriptWidgetTree::ClearCurrentWidgetGroup(u32 UniqueId, const char *ASSERT_ONLY(pScriptName))
{
	bkGroup* pGroup = static_cast<bkGroup*>(GetWidgetFromUniqueWidgetId(UniqueId));
	if (scriptVerifyf(pGroup, "CScriptWidgetTree::ClearCurrentWidgetGroup - %s failed to find a widget group with the unique ID", pScriptName))
	{
		if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
		{
			CScriptDebug::GetWidgetBank()->UnSetCurrentGroup(*pGroup);
		}

		ms_pCurrentWidgetNode = NULL;
	}
}

void CScriptWidgetTree::RemoveAllWidgetNodes()
{
	if (m_pTopLevelWidget)
	{
		delete m_pTopLevelWidget;
		m_pTopLevelWidget = NULL;
	}
}

u32 CScriptWidgetTree::GetUniqueWidgetIdOfTopLevelWidget(const char *ASSERT_ONLY(pScriptName))
{
	if (scriptVerifyf(m_pTopLevelWidget, "CScriptWidgetTree::GetUniqueWidgetIdOfTopLevelWidget - %s doesn't have a top level widget group yet", pScriptName))
	{
		return m_pTopLevelWidget->GetUniqueId();
	}

	return 0;
}

void CScriptWidgetTree::CloseWidgetGroup()
{
	if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
	{
		CScriptDebug::GetWidgetBank()->PopGroup();
	}

	if (scriptVerifyf(ms_pCurrentWidgetNode, "CScriptWidgetTree::CloseWidgetGroup - expected ms_pCurrentWidgetNode to point to a valid widget node when closing a widget group"))
	{
		ms_pCurrentWidgetNode = ms_pCurrentWidgetNode->GetParent();
	}
}

void CScriptWidgetTree::CheckWidgetGroupsHaveBeenClosed(const char *ASSERT_ONLY(pScriptName))
{
	if (ms_pCurrentWidgetNode)
	{
		scriptAssertf(0, "CScriptWidgetTree::CheckWidgetGroupsHaveBeenClosed - %s is missing either a STOP_WIDGET_GROUP or a CLEAR_CURRENT_WIDGET_GROUP", pScriptName);
		ms_pCurrentWidgetNode = NULL;
	}
}

bkWidget *CScriptWidgetTree::GetWidgetFromUniqueWidgetId(u32 UniqueId)
{
	return static_cast<bkWidget*>(g_uniqueObjectIDGenerator.GetIDInstanceInfoFromID(UniqueId).m_uniqueBase);
}


//////////////////////////////////////////////////////////////////////////
//	End of CScriptWidgetTree
//////////////////////////////////////////////////////////////////////////

#endif	//	__BANK

//////////////////////////////////////////////////////////////////////////
//	CScriptDebug
//////////////////////////////////////////////////////////////////////////

void CScriptDebug::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
#if __BANK
		ms_ScriptThreadIdForDebugging[0] = '\0';

		ms_pScriptDebuggerPath = NULL;
		if (!PARAM_scriptdebugger.GetParameter(ms_pScriptDebuggerPath))
        {
            //attempt to grab it from the config file.
            ms_pScriptDebuggerPath = CGameConfig::Get().GetDebugScriptsPath().c_str();
        }
        scriptDisplayf("ms_pScriptDebuggerPath{%s}", ms_pScriptDebuggerPath);

		const char *pFilesToBeDebugged = NULL;
		PARAM_scriptdebugger_files.GetParameter(pFilesToBeDebugged);
		s32 string_length = 0;
		if (pFilesToBeDebugged && *pFilesToBeDebugged)
		{
			string_length = istrlen(pFilesToBeDebugged);
		}

		ms_ArrayOfNamesOfScriptsToBeDebugged.Reset();

		const s32 MaxLengthOfFileName = 64;
		char CurrentFileName[MaxLengthOfFileName];
		s32 CurrentFileNameLength = 0;
		for (s32 loop = 0; loop < string_length; loop++)
		{
			if  ( ( (pFilesToBeDebugged[loop] >= 'a') && (pFilesToBeDebugged[loop] <= 'z') )
				|| ( (pFilesToBeDebugged[loop] >= 'A') && (pFilesToBeDebugged[loop] <= 'Z') ) 
				|| ( (pFilesToBeDebugged[loop] >= '0') && (pFilesToBeDebugged[loop] <= '9') ) 
				|| (pFilesToBeDebugged[loop] == '_') )
			{
				if (scriptVerifyf(CurrentFileNameLength < (MaxLengthOfFileName-1), "CScriptDebug::Init - scriptdebugger_files command line param contains a file name that is longer than 63 characters"))
        {
					CurrentFileName[CurrentFileNameLength++] = pFilesToBeDebugged[loop];
				}
			}
			else if (pFilesToBeDebugged[loop] == ',')
			{
				if (CurrentFileNameLength > 0)
				{
					CurrentFileName[CurrentFileNameLength] = '\0';
					scriptDebugf3("Script to be debugged = %s\n", CurrentFileName);
					ms_ArrayOfNamesOfScriptsToBeDebugged.PushAndGrow(atString(CurrentFileName));
					CurrentFileNameLength = 0;
				}
			}
			else
			{
				scriptAssertf(0, "CScriptDebug::Init - scriptdebugger_files command line param contains an invalid character %c", pFilesToBeDebugged[loop]);
			}
		}

		if (CurrentFileNameLength > 0)
		{
			CurrentFileName[CurrentFileNameLength] = '\0';
			scriptDebugf3("Script to be debugged = %s\n", CurrentFileName);
			ms_ArrayOfNamesOfScriptsToBeDebugged.PushAndGrow(atString(CurrentFileName));
        }
#endif	//	__BANK

#if !__FINAL
		if (PARAM_displayScriptRequests.Get())
		{
			ms_bDisplayScriptRequests = true;
		}
#endif // !__FINAL
    }
    else if(initMode == INIT_SESSION)
    {
#if __BANK
		ms_bDisplayRunningScripts = false;
		ms_bDisplayNumberOfScriptResources = false;
		ms_bPrintCurrentContentsOfCleanupArray = false;

	    ms_bAutoSaveEnabled = true;
	    ms_bPerformAutoSave = false;
	    ms_bDrawDebugLinesAndSpheres = false;
		ms_bDebugMaskCommands = false;
		if (PARAM_script_masks.Get())
		{
			ms_bDebugMaskCommands = true;
		}
		ms_bPrintSuppressedCarModels = false;
		ms_bPrintSuppressedPedModels = false;
		ms_bPrintRestrictedPedModels = false;

		ms_bOutputScriptDisplayTextCommands = false;
		ms_NumberOfFramesToOutputScriptDisplayTextCommands = -1;

		ms_bEnablePedDragging = false;

		ms_TextWidgets.Init();

		ms_BitFieldWidgets.Init();

#if SCRIPT_PROFILING
		ms_bDisplayProfileOverview = false;
#endif	//	SCRIPT_PROFILING

#endif	//	__BANK

#if !__FINAL
		ms_bDbgFlag = false;			// debugging off at start
//		ms_bDisplayScriptRequests = false;
	    ms_bDisableDebugCamAndPlayerWarping = false;	//	For build for Image Metrics
#endif
	}
}

void CScriptDebug::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
	}
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
#if __BANK
		if (ms_pBank)
		{
#if __BANK
			//remove the anim placement tool widgets before destroying the bank, or all hell's gonna break loose
			CAnimPlacementEditor::RemoveWidgets();
			CAnimPlacementEditor::Shutdown();

			for (s32 i=0; i<ms_attachmentTools.GetCount(); i++)
			{
				ms_attachmentTools[i].Shutdown();
			}
#endif // __BANK

			CTheScripts::GetScriptHandlerMgr().ShutdownWidgets(ms_pBank);

			ms_pBank->Shutdown();
			ms_pBank = NULL;
		}

		ms_pComboBoxOfScriptThreads = NULL;
		sm_ArrayIndexOfThreadToAttachDebuggerTo = 0;
		sm_ComboBoxOfThreadNamesNeedsUpdated = false;

		bkBank *pScriptDebuggerBank = BANKMGR.FindBank(pNameOfScriptDebuggerBank);
		if (pScriptDebuggerBank)
		{
			BANKMGR.DestroyBank(*pScriptDebuggerBank);
		}
#endif // __BANK
	}
}

#if __BANK
bool CScriptDebug::ShouldThisScriptBeDebugged(const char *pScriptName)
{
	u32 hashKey = 0;
	char stringContainingHashKey[24];

	if (ms_pScriptDebuggerPath)	//	There's no point in comparing the strings if a path to the .scd files has not been set
	{
		u32 length_of_input_string = ustrlen(pScriptName);

		s32 SizeOfArray = ms_ArrayOfNamesOfScriptsToBeDebugged.GetCount();
		for (s32 loop = 0; loop < SizeOfArray; loop++)
		{
//	Some scripts are now launched using the hash of their name instead of a string containing that name
			hashKey = atStringHash(ms_ArrayOfNamesOfScriptsToBeDebugged[loop].c_str());
			formatf(stringContainingHashKey, "0x%x", hashKey);

			if (stricmp(pScriptName, stringContainingHashKey) == 0)
			{
				return true;
			}

			if (length_of_input_string == ms_ArrayOfNamesOfScriptsToBeDebugged[loop].length())
			{
				if (stricmp(pScriptName, ms_ArrayOfNamesOfScriptsToBeDebugged[loop].c_str()) == 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}
#endif	//	__BANK

void CScriptDebug::Process(void)
{
#if __BANK
	if (ms_pComboBoxOfScriptThreads)
	{
		if (sm_ComboBoxOfThreadNamesNeedsUpdated)
		{
			GtaThread::UpdateArrayOfNamesOfRunningThreads(sm_ThreadNamesForDebugger);
			ms_pComboBoxOfScriptThreads->UpdateCombo("Launch Debugger for Script", &sm_ArrayIndexOfThreadToAttachDebuggerTo, 
				sm_ThreadNamesForDebugger.GetCount(), &sm_ThreadNamesForDebugger[0]);
			sm_ComboBoxOfThreadNamesNeedsUpdated = false;
		}
	}

	static bool bSaveHasStarted = false;

	ms_bFadeInAfterLoadReadOnly = CGenericGameStorage::ms_bFadeInAfterSuccessfulLoad;

	ms_bFadeInAfterDeathArrestReadOnly = !CRestart::bSuppressFadeInAfterDeathArrest;
	ms_bPauseDeathArrestRestartReadOnly = CRestart::bPausePlayerRespawnAfterDeathArrest;
	ms_bIgnoreNextRestartReadOnly = CRestart::GetIgnoreNextRestart();


	if (ms_bDisplayRunningScripts)
	{
		GtaThread::DisplayAllRunningScripts(true);
	}

#if __REPORT_TOO_MANY_RESOURCES
	if (ms_bDisplayNumberOfScriptResources)
	{
		CGameScriptResource::DisplayResourceUsage(true);
	}
#endif	//	__REPORT_TOO_MANY_RESOURCES

	if (ms_bPrintSuppressedCarModels)
	{
		CScriptCars::GetSuppressedCarModels().PrintSuppressedModels();
	}
	
	if (ms_bPrintSuppressedPedModels)
	{
		CScriptPeds::GetSuppressedPedModels().PrintSuppressedModels();
	}

	if (ms_bPrintRestrictedPedModels)
	{
		CScriptPeds::GetRestrictedPedModels().PrintRestrictedModels();
	}

	if (ms_bOutputScriptDisplayTextCommands)
	{
		if (ms_NumberOfFramesToOutputScriptDisplayTextCommands > 0)
		{
			ms_NumberOfFramesToOutputScriptDisplayTextCommands--;
			if (ms_NumberOfFramesToOutputScriptDisplayTextCommands == 0)
			{
				ms_bOutputScriptDisplayTextCommands = false;
			}
		}
	}


	ms_BitFieldWidgets.UpdateBitFields();

	ProcessScriptDebugTools();

	if (ms_bPerformAutoSave)
	{
		if (bSaveHasStarted == false)
		{
			if (CGenericGameStorage::IsStorageDeviceBeingAccessed())
			{
				Displayf("Widget - Can't start an autosave just now\n");
			}
			else
			{
				CGenericGameStorage::QueueAutosave();
				bSaveHasStarted = true;
			}
		}

		if (!CGenericGameStorage::IsStorageDeviceBeingAccessed())
		{
			bSaveHasStarted = false;
			ms_bPerformAutoSave = false;
		}
	}

	if (ms_bPrintCurrentContentsOfCleanupArray)
	{
#if __DEV
		CTheScripts::GetScriptHandlerMgr().SpewObjectAndResourceInfo();
#endif	//	__DEV

		ms_bPrintCurrentContentsOfCleanupArray = false;
	}

#if SCRIPT_PROFILING
	DisplayProfileOverviewOfAllThreads();
#endif	//	SCRIPT_PROFILING

#if __BANK
	CAnimPlacementEditor::Update();
#endif // __BANK
#endif //__BANK
}


#if __BANK
void CScriptDebug::OutputScript(const char * text)
{
	FileHandle file = CFileMgr::OpenFileForAppending(CScriptDebug::GetNameOfScriptDebugOutputFile());
	scriptAssertf(file, "Could not open the script output file for writing");

	//output the controlling variable first
	Displayf( "%s", text );
	if (CFileMgr::IsValidFileHandle(file))
	{
		CFileMgr::Write(file, text, istrlen(text));
		CFileMgr::Write(file, "\r\n", 1);
		CFileMgr::CloseFile(file);
	}
}

void CScriptDebug::AttachSelectedObjects()
{
	CEntity* pEntity0 = CDebugScene::FocusEntities_Get(0);
	CEntity* pEntity1 = CDebugScene::FocusEntities_Get(1);

	if (pEntity0 && pEntity1)
	{
		//if there's a selected entity
		if (pEntity0->GetIsPhysical())
		{	
			//cast to a CPhysical
			CPhysical* pChild = static_cast<CPhysical*>(pEntity0);
			CPhysical* pParent = NULL;
			
			//is there a parent entity?
			if (pEntity1)
			{
				if (pEntity1->GetIsPhysical())
				{
					pParent = static_cast<CPhysical*>(pEntity1);
				}
			}
			if(pChild) 
			{
				atString toolName("Attachment: ");
				toolName += CModelInfo::GetBaseModelInfoName(pChild->GetModelId());
				if (pParent)
				{
					toolName += "->";
					toolName += CModelInfo::GetBaseModelInfoName(pParent->GetModelId());
				}

				CDebugAttachmentTool& tool = ms_attachmentTools.Grow();

				tool.Initialise(pChild, pParent);

				ms_pBank->SetCurrentGroup(*ms_pAttachmentGroup);

				tool.AddWidgets(ms_pBank, toolName);

				ms_pBank->UnSetCurrentGroup(*ms_pAttachmentGroup);
			}

		}
	}
}

void CScriptDebug::ClearAttachments()
{
	for (s32 i=0;i<ms_attachmentTools.GetCount(); i++)
	{
		ms_attachmentTools[i].RemoveWidgets();
		ms_attachmentTools[i].Shutdown();
	}
}

void CScriptDebug::UpdateAttachmentTools()
{
	for (s32 i=0;i<ms_attachmentTools.GetCount(); i++)
	{
		ms_attachmentTools[i].Update();
	}
}


u32 CScriptDebug::AddWidget(bkWidget *pWidget, eWidgetType WidgetType)
{
	if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptThread(), "CScriptDebug::AddWidget - can only add script widgets using script commands"))
	{
#if __DEV
		return CTheScripts::GetCurrentGtaScriptThread()->m_WidgetTree.AddWidget(pWidget, WidgetType, CTheScripts::GetCurrentScriptNameAndProgramCounter() );
#else
		return CTheScripts::GetCurrentGtaScriptThread()->m_WidgetTree.AddWidget(pWidget, WidgetType, CTheScripts::GetCurrentGtaScriptThread()->GetScriptName() );
#endif
	}

	return 0;
}


u32 CScriptDebug::GetUniqueWidgetIdOfTopLevelWidget()
{
	if (scriptVerifyf(CTheScripts::GetCurrentGtaScriptThread(), "CScriptDebug::GetUniqueWidgetIdOfTopLevelWidget - can only call this while processing scripts"))
	{
#if __DEV
		return CTheScripts::GetCurrentGtaScriptThread()->m_WidgetTree.GetUniqueWidgetIdOfTopLevelWidget(CTheScripts::GetCurrentScriptNameAndProgramCounter() );
#else
		return CTheScripts::GetCurrentGtaScriptThread()->m_WidgetTree.GetUniqueWidgetIdOfTopLevelWidget(CTheScripts::GetCurrentGtaScriptThread()->GetScriptName() );
#endif
	}

	return 0;
}


extern void UpdatePos1(void);
extern void UpdatePos2(void);

void CScriptDebug::ActivateBank() 
{
	ms_pBank->PushGroup("Script Debug Tools", false);
		ms_pBank->AddButton("Enable / Disable Debugging", ToggleClickingObjectsCB);
		ms_pBank->AddToggle("Enable Debugging", &ms_bEnablePedDragging, ActivateClickingObjectsCB);
		ms_pBank->AddText("Cursor Pos", ms_CreatePos, sizeof(ms_CreatePos),false);
		ms_pBank->PushGroup("Floating Cursors (use in water / space levels where there is no ground) ", false);
			ms_pBank->AddToggle("Activate floating cursor ", &ms_bUseLineofSight); 
			ms_pBank->AddSlider("Cursor offset (up/down arrows)", &ms_fPosFromCameraFront, 0.0f, 3000.0f, 0.01f, NullCB, "Locate z dimension" );
		ms_pBank->PopGroup();
		ms_PedHandler.CreateWidgets(ms_pBank);
		ms_VehicleHandler.CreateWidgets(ms_pBank);
		ms_ObjectHandler.CreateWidgets(ms_pBank);
		ms_pAttachmentGroup = ms_pBank->PushGroup("Attach objects");
		{
			ms_pBank->AddButton("Attach selected objects", AttachSelectedObjects);
			ms_pBank->AddButton("Clear all attachments", ClearAttachments);
		}
		ms_pBank->PopGroup();
		ms_LocateHandler.CreateWidgets(ms_pBank);
		ms_pBank->PushGroup("Measuring Tool",false);
			ms_pBank->AddToggle( "Turn on tool", &CPhysics::ms_bDebugMeasuringTool );
			ms_pBank->AddText("Pos1", CPhysics::ms_Pos1, sizeof(CPhysics::ms_Pos1), false, UpdatePos1);
			ms_pBank->AddText("Pos2", CPhysics::ms_Pos2, sizeof(CPhysics::ms_Pos2), false, UpdatePos2);
			ms_pBank->AddText("Diff", CPhysics::ms_Diff, sizeof(CPhysics::ms_Diff), false);
			ms_pBank->AddText("HeadingBetween", CPhysics::ms_HeadingDiffRadians, sizeof(CPhysics::ms_HeadingDiffRadians), false);
			ms_pBank->AddText("Distance", CPhysics::ms_Distance, sizeof(CPhysics::ms_Distance), false);
			ms_pBank->AddText("Horizontal dist", CPhysics::ms_HorizDistance, sizeof(CPhysics::ms_HorizDistance), false);
			ms_pBank->AddText("Vertical dist", CPhysics::ms_VerticalDistance, sizeof(CPhysics::ms_VerticalDistance), false);
		ms_pBank->PopGroup();
	ms_pBank->PopGroup();
}

void CScriptDebug::DeactivateBank()
{
	ms_ObjectHandler.RemoveWidgets();

	ms_VehicleHandler.RemoveWidgets();

	ms_pAttachmentGroup = NULL; 

	bkGroup* group = static_cast<bkGroup*>(BANKMGR.FindWidget("Script/Script Debug Tools"));
	if (group)
	{
		group->Destroy(); 
	}
}

void CScriptDebug::InitWidgets()
{
	if (AssertVerify(!ms_pBank))
	{
		ms_pBank = fwDebugBank::CreateBank("Script", MakeFunctor(ActivateBank), MakeFunctor(DeactivateBank), MakeFunctor(CreatePermanentWidgets));
	}

	bkBank *pScriptDebuggerBank = BANKMGR.FindBank(pNameOfScriptDebuggerBank);
	if (AssertVerify(!pScriptDebuggerBank))
	{
		pScriptDebuggerBank = &BANKMGR.CreateBank(pNameOfScriptDebuggerBank);

		if (AssertVerify(pScriptDebuggerBank))
		{
			GtaThread::UpdateArrayOfNamesOfRunningThreads(sm_ThreadNamesForDebugger);
			ms_pComboBoxOfScriptThreads = pScriptDebuggerBank->AddCombo("Launch Debugger for Script", &sm_ArrayIndexOfThreadToAttachDebuggerTo, 
				sm_ThreadNamesForDebugger.GetCount(), &sm_ThreadNamesForDebugger[0]);
			pScriptDebuggerBank->AddButton("Launch Debugger for selected script", LaunchDebuggerForThreadSelectedInCombo);
		}
	}

#if __BANK
	CAnimPlacementEditor::Initialise();
#endif // __BANK
}

void CScriptDebug::CreatePermanentWidgets(fwDebugBank* pBank)
{
	pBank->AddToggle("Display Running Scripts", &ms_bDisplayRunningScripts);
#if __REPORT_TOO_MANY_RESOURCES
	pBank->AddToggle("Display Number of Script Resources", &ms_bDisplayNumberOfScriptResources);
#endif	//	__REPORT_TOO_MANY_RESOURCES
	pBank->AddToggle("Display REQUEST_SCRIPT calls", &ms_bDisplayScriptRequests);
	pBank->AddToggle("Print Current Contents of Cleanup Array", &ms_bPrintCurrentContentsOfCleanupArray);

	pBank->AddToggle("Autosave Enabled", &ms_bAutoSaveEnabled);
	pBank->AddToggle("Perform Autosave", &ms_bPerformAutoSave);
#if RSG_PC
	pBank->AddToggle("Fake Script Tamper", &ms_bFakeTampering);
#endif
#if SCRIPT_PROFILING
	if (scrThread::IsProfilingEnabled()) {
		pBank->AddToggle("Display Script Profile Overview", &ms_bDisplayProfileOverview);
		pBank->AddSlider("Profile Display Height", &ms_ProfileDisplayStartRow, 0, 60, 1);	
		pBank->AddCombo("Sort Profile by", &scrThread::sm_ProfileSortMethod, scrThread::sm_ProfileSortMethodCount, scrThread::sm_ProfileSortMethodStrings);
	}
	else
		pBank->AddTitle("Add -scriptprofiling to cmd line for profiling");
#endif	//	SCRIPT_PROFILING

	ms_FadeCommandTextContents[0] = '\0';
	pBank->AddText("Last Fade Command", ms_FadeCommandTextContents, MAX_LENGTH_OF_FADE_COMMAND_TEXT_WIDGET, true);

	pBank->AddToggle("Fade in after load (read-only)", &ms_bFadeInAfterLoadReadOnly);
	pBank->AddToggle("Draw Debug Lines And Spheres", &ms_bDrawDebugLinesAndSpheres);
	pBank->AddToggle("Debug Mask Commands", &ms_bDebugMaskCommands);
	pBank->AddToggle("Print Suppressed Car Models", &ms_bPrintSuppressedCarModels);
	pBank->AddToggle("Print Suppressed Ped Models", &ms_bPrintSuppressedPedModels);
	pBank->AddToggle("Print Restricted Ped Models", &ms_bPrintRestrictedPedModels);

	pBank->AddToggle("Output Script Display Text Commands", &ms_bOutputScriptDisplayTextCommands);
	pBank->AddSlider("Number Of Frames To Output Script Display Text Commands", &ms_NumberOfFramesToOutputScriptDisplayTextCommands, -1, 300, 1);

	pBank->AddToggle("Print List of Sprites When Limit is Hit", &ms_bOutputListOfAllScriptSpritesWhenLimitIsHit);

//	pBank->AddText("ScriptId for debugging", ms_ScriptThreadIdForDebugging, sizeof(ms_ScriptThreadIdForDebugging), false);
//	pBank->AddButton("Launch Debugger for this ScriptId", LaunchScriptDebuggerForSelectedId);

#if SCRIPT_PROFILING
	if (scrThread::IsProfilingEnabled())
	{
		pBank->AddButton("Enable Profiling for selected script", EnableProfilingForThreadSelectedInCombo);
	}
	else
	{
		pBank->AddTitle("Add -scriptprofiling to cmd line for profiling");
	}
#endif	//	SCRIPT_PROFILING

	pBank->PushGroup("Markers for Area Check Commands");
		pBank->AddSlider("Distance Threshold (Near)", &CScriptAreas::ms_distThreshNear, 0.0f, 100.0f, 0.25f);
		pBank->AddSlider("Distance Threshold (Far)", &CScriptAreas::ms_distThreshFar, 0.0f, 100.0f, 0.25f);
		pBank->AddSlider("Alpha (Near)", &CScriptAreas::ms_alphaNear, 0, 255, 1);
		pBank->AddSlider("Alpha (Far)", &CScriptAreas::ms_alphaFar, 0, 255, 1);
		pBank->AddToggle("Arrow - Enabled", &CScriptAreas::ms_arrowEnabled);
		pBank->AddSlider("Arrow - Offset Z", &CScriptAreas::ms_arrowOffsetZ, -5.0f, 25.0f, 0.1f);
		pBank->AddSlider("Arrow - Scale (Near)", &CScriptAreas::ms_arrowScaleNear, -5.0f, 25.0f, 0.1f);
		pBank->AddSlider("Arrow - Scale (Far)", &CScriptAreas::ms_arrowScaleFar, -5.0f, 25.0f, 0.1f);
		pBank->AddSlider("Cylinder - Offset Z", &CScriptAreas::ms_cylinderOffsetZ, -5.0f, 25.0f, 0.1f);
		pBank->AddSlider("Cylinder - Scale XY (Near)", &CScriptAreas::ms_cylinderScaleXYNear, -5.0f, 25.0f, 0.1f);
		pBank->AddSlider("Cylinder - Scale XY (Far)", &CScriptAreas::ms_cylinderScaleXYFar, -5.0f, 25.0f, 0.1f);
		pBank->AddSlider("Cylinder - Scale Z (Near)", &CScriptAreas::ms_cylinderScaleZNear, -5.0f, 25.0f, 0.1f);
		pBank->AddSlider("Cylinder - Scale Z (Far)", &CScriptAreas::ms_cylinderScaleZFar, -5.0f, 25.0f, 0.1f);
	pBank->PopGroup();

	pBank->PushGroup("DeathArrest Respawn Controls");
		pBank->AddToggle("Fade in after DeathArrest (read-only)", &ms_bFadeInAfterDeathArrestReadOnly);
		pBank->AddToggle("Pause Death Arrest Restart (read-only)", &ms_bPauseDeathArrestRestartReadOnly);
		pBank->AddToggle("Ignore Next Restart (read-only)", &ms_bIgnoreNextRestartReadOnly);
	pBank->PopGroup();

	//		bkGroup* pGroup = 
	pBank->PushGroup("Network Command Options", false);
		pBank->AddToggle("Write IS_NETWORK_SESSION Command", &ms_bWriteIS_NETWORK_SESSIONCommand);
		pBank->AddToggle("Write NETWORK_HAVE_SUMMONS Command", &ms_bWriteNETWORK_HAVE_SUMMONSCommand);
		pBank->AddToggle("Write IS_NETWORK_GAME_RUNNING Command", &ms_bWriteIS_NETWORK_GAME_RUNNINGCommand);
		pBank->AddToggle("Write IS_NETWORK_PLAYER_ACTIVE Command", &ms_bWriteIS_NETWORK_PLAYER_ACTIVECommand);
		pBank->AddToggle("Write PLAYER_WANTS_TO_JOIN_NETWORK_GAME Command", &ms_bWritePLAYER_WANTS_TO_JOIN_NETWORK_GAMECommand);
		pBank->AddToggle("Write Network Commands To Script log file", &ms_bWriteNetworkCommandsToScriptLog);
		pBank->AddToggle("Display Network Commands In Console", &ms_bDisplayNetworkCommandsInConsole);
	pBank->PopGroup(); 

	CTheScripts::GetScriptHandlerMgr().InitWidgets(pBank);
}

void CScriptDebug::LaunchDebuggerForThreadSelectedInCombo()
{
	GtaThread::AllocateDebuggerForThreadInSlot(sm_ArrayIndexOfThreadToAttachDebuggerTo);
}

void CScriptDebug::EnableProfilingForThreadSelectedInCombo()
{
	GtaThread::EnableProfilingForThreadInSlot(sm_ArrayIndexOfThreadToAttachDebuggerTo);
}

void CScriptDebug::LaunchScriptDebuggerForSelectedId()
{
	s32 string_length = istrlen(ms_ScriptThreadIdForDebugging);
	for (s32 loop = 0; loop < string_length; loop++)
	{
		if ( (ms_ScriptThreadIdForDebugging[loop] < '0') || (ms_ScriptThreadIdForDebugging[loop] > '9') )
		{
			scriptAssertf(0, "ScriptId for debugging should only contain numbers");
			return;
		}
	}

	s32 threadId = atoi(ms_ScriptThreadIdForDebugging);

	GtaThread::AllocateDebuggerForThread((scrThreadId) threadId);
}

void CScriptDebug::SetContentsOfFadeCommandTextWidget(const char *pFadeCommandString)
{
#if __DEV
	if (CTheScripts::GetCurrentGtaScriptThread())
	{
		sprintf(ms_FadeCommandTextContents, "Frame = %d %s %s", fwTimer::GetSystemFrameCount(), CTheScripts::GetCurrentScriptNameAndProgramCounter(), pFadeCommandString);
	}
	else
#endif
	{
		sprintf(ms_FadeCommandTextContents, "Frame = %d %s", fwTimer::GetSystemFrameCount(), pFadeCommandString);
	}
}



#if SCRIPT_PROFILING
static s32 profileY;
static s32 profInt;

static void profDisplayf(bool severe,const char *fmt,...)
{
	va_list args;
	va_start(args,fmt);

	// don't run off the bottom of the screen
	if (!profileY)
		return;

	Color32 sevColor;
	if (severe) 
		sevColor.Set(240,0,0);
	else 
		sevColor.Set(240,240,240);

	// grcDebugDraw::PrintToScreenCoors(buffer, 10, profileY, sevColor, false);
	grcDebugDraw::AddDebugOutputExV(false,sevColor,fmt,args);

	va_end(args);

	--profileY;
	// Periodic spacing so it's easier to read
	if (++profInt == 4) 
	{
		grcDebugDraw::AddDebugOutputSeparator(3);
		profInt = 0;
	}
}

void CScriptDebug::DisplayProfileOverviewOfAllThreads()
{
	if (ms_bDisplayProfileOverview)
	{
		profileY = ms_ProfileDisplayStartRow;
		profInt = 3;		// for inserting a gap periodically so it's easier to read
		scrThread::DisplayProfileOverview(profDisplayf);
	}
}

void CScriptDebug::DisplayProfileDataForThisThread(GtaThread *pThread)
{
	profileY = ms_ProfileDisplayStartRow;
	profInt = 3;		// for inserting a gap periodically so it's easier to read
	pThread->DisplayProfileData(profDisplayf);
}
#endif	// SCRIPT_PROFILING


//Get the mouse position so we have coords to create a ped
void CScriptDebug::UpdateCreatePos()
{
	Vector3 vPos;
	Vector3 vNormal;
	void *entity;

	bool bHasPosition = false;
		
	if(ms_bUseLineofSight)	
	{
		UpdateFloatingCreatePos(vPos);
		bHasPosition = true;
	}	
	else
	{
		bHasPosition = CDebugScene::GetWorldPositionUnderMouse(vPos, ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE , &vNormal, &entity);
	}

	static bool sbLeftButtonHeld = false;
	static bool sbRightButtonHeld = false;

	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
	{
		sbLeftButtonHeld = true;
	}

	if( sbLeftButtonHeld &&
		ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT )
	{
		sbLeftButtonHeld = false;
	}
	
	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT )
	{
		sbRightButtonHeld = true;
	}

	if( sbRightButtonHeld &&
		ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT )
	{
		sbRightButtonHeld = false;
	}

	if( bHasPosition )
	{
		if( sbLeftButtonHeld )
		{
			ms_vClickedPos = vPos;
			ms_vClickedNormal = vNormal;
			sprintf(ms_CreatePos, "%.2f, %.2f, %.2f", ms_vClickedPos.x, ms_vClickedPos.y, ms_vClickedPos.z);
		}

		if (sbRightButtonHeld)
		{
			ms_vClickedPos2 = vPos;
			ms_vClickedNormal2 = vNormal;
			sprintf(ms_CreatePos, "%.2f, %.2f, %.2f", ms_vClickedPos2.x, ms_vClickedPos2.y, ms_vClickedPos2.z);
		}
	}

	grcDebugDraw::Sphere(ms_vClickedPos, 0.1f, Color32(1.0f, 0.0f, 0.0f) );
	if(!ms_bUseLineofSight)
	{
		grcDebugDraw::Line(ms_vClickedPos, ms_vClickedPos+(ms_vClickedNormal*0.4f), Color32(1.0f, 0.0f, 0.0f) );
	}

	ms_LocateHandler.DisplaySpheresForNonAxisAlignedLocate();
}


void CScriptDebug::UpdateFloatingCreatePos(Vector3 &CreatePos)
{
	Vector3 vMouseNear, vMouseFar;

	CDebugScene::GetMousePointing( vMouseNear, vMouseFar, false);

	CEntity* pEntity = CDebugScene::GetEntityUnderMouse();
	
	Vector3 vecCreateOffset = camInterface::GetFront();

	Vector3 VecNearToFar = vMouseFar - vMouseNear;
	
	VecNearToFar.Normalize();

	if(ioMapper::DebugKeyDown(KEY_UP))
	{
		ms_fPosFromCameraFront += 0.1f;
	}

	if(ioMapper::DebugKeyDown(KEY_DOWN))
	{
		ms_fPosFromCameraFront -= 0.1f;
	}
	
	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
	{
		if	(pEntity && (pEntity->GetIsTypeVehicle() || pEntity->GetIsTypeObject() || pEntity->GetIsTypePed()) )
		{		
			Vector3 EntityPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());

			ms_fPosFromCameraFront = (EntityPos - vMouseNear).Dot(VecNearToFar);
		}
	}

	CreatePos = vMouseNear + (VecNearToFar * ms_fPosFromCameraFront );
}

void CScriptDebug::ToggleClickingObjectsCB()
{
	ms_bEnablePedDragging = !ms_bEnablePedDragging;
	ActivateClickingObjectsCB();
}

void CScriptDebug::ActivateClickingObjectsCB()
{
	if(!ms_bEnablePedDragging)
	{
		CPedDebugVisualiser::SetDebugDisplay(CPedDebugVisualiser::eOff);
	}
	else
	{
		CPedDebugVisualiser::SetDebugDisplay(CPedDebugVisualiser::ePedNames);
	}
	CDebugScene::SetClickForObjects(ms_bEnablePedDragging);
}


//Updates the script tools
void CScriptDebug::ProcessScriptDebugTools()
{
	if (ms_bEnablePedDragging)
	{
		UpdateCreatePos();
		ms_LocateHandler.DisplayScriptLocates();
		
		ms_PedHandler.Process();
		ms_VehicleHandler.Process();
		ms_ObjectHandler.Process();

		UpdateAttachmentTools();
	}
}

CEntity* CScriptDebug::CreateScriptEntity(u32 modelIndex)
{
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));
	if(pModelInfo==NULL)
	{
		Displayf ("CScriptDebug::CreateScriptEntity - Invalid model: %d", modelIndex);
		return NULL;
	}
	if(pModelInfo)
	{
		if (pModelInfo->GetModelType() == MI_TYPE_PED)
		{
			return ms_PedHandler.CreateScriptPed(modelIndex);
		}
		else if (pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
		{
			return ms_VehicleHandler.CreateScriptVehicle(modelIndex);
		}
		else
		{
			return ms_ObjectHandler.CreateScriptObject(modelIndex);
		}
	}

	return NULL;
}


void SaveVectorToDebugFile(const Vector3 &VecToDisplay)
{
	char text[256]; 
	formatf(text, 256, "<<%f,%f,%f>>", VecToDisplay.x, VecToDisplay.y, VecToDisplay.z );
	
	CScriptDebug::OutputScript(text); 
}

//////////////////////////////////////////////////////////////////////////
//	CScriptDebugTextWidgets
//////////////////////////////////////////////////////////////////////////

void CScriptDebugTextWidgets::Init()
{
    for (u32 Counter = 0; Counter < MAX_NUMBER_OF_TEXT_WIDGETS; Counter++)
    {
		for (u32 Counter2 = 0; Counter2 < TextEditWidgetForScript::MAX_CHARACTERS_FOR_TEXT_WIDGET; Counter2++)
	    {
		    TextWidgets[Counter].WidgetTextContents[Counter2] = '\0';
	    }
	    TextWidgets[Counter].pWidget = NULL;
    }

	scriptDisplayf("CScriptDebugTextWidgets::Init - sizeof(TextEditWidgetForScript) = %d bytes, MAX_NUMBER_OF_TEXT_WIDGETS = %d, sizeof(CScriptDebugTextWidgets) = %d bytes", 
		(s32) sizeof(TextEditWidgetForScript), MAX_NUMBER_OF_TEXT_WIDGETS, (s32) sizeof(CScriptDebugTextWidgets) );
}

s32 CScriptDebugTextWidgets::FindFreeTextWidget()
{
	s32 loop = 0;
	while (loop < MAX_NUMBER_OF_TEXT_WIDGETS)
	{
		if (TextWidgets[loop].pWidget == NULL)
		{
			return loop;
		}

		loop++;
	}

#if __ASSERT
	scriptDisplayf("FindFreeTextWidget - no free text widgets. All %d text widgets have been used.", MAX_NUMBER_OF_TEXT_WIDGETS);
	for (loop = 0; loop < MAX_NUMBER_OF_TEXT_WIDGETS; loop++)
	{
		if (TextWidgets[loop].pWidget)
		{
			scriptDisplayf("Text Widget %d has name %s", loop, TextWidgets[loop].pWidget->GetTitle());
		}
	}

	scriptAssertf(0, "CTheScripts::FindFreeTextWidget - no free text widgets. All %d text widgets have been used.", MAX_NUMBER_OF_TEXT_WIDGETS);
#endif	//	__ASSERT

	return -1;
}

s32 CScriptDebugTextWidgets::AddTextWidget(const char *pName)
{
	s32 TextWidgetArrayIndex = FindFreeTextWidget();
	if (TextWidgetArrayIndex >= 0)
	{
		if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
		{
			strcpy(TextWidgets[TextWidgetArrayIndex].WidgetTextContents, "New text widget");
			TextWidgets[TextWidgetArrayIndex].pWidget = CScriptDebug::GetWidgetBank()->AddText(pName, TextWidgets[TextWidgetArrayIndex].WidgetTextContents, TextEditWidgetForScript::MAX_CHARACTERS_FOR_TEXT_WIDGET, false);
			return CScriptDebug::AddWidget(TextWidgets[TextWidgetArrayIndex].pWidget, WIDGETTYPE_TEXT);
		}
	}
	return 0;
}

const char *CScriptDebugTextWidgets::GetContentsOfTextWidget(s32 UniqueTextWidgetIndex)
{
//	scan through the existing widgets for the one that has a widget pointer that matches the widget pointer for TextWidgetIndex
	if(scriptVerifyf(UniqueTextWidgetIndex > 0, "%s:CScriptDebug::GetContentsOfTextWidget - Text Widget Index should be 1 or greater", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		bkText *pWidgetToMatch = static_cast<bkText*>(CScriptWidgetTree::GetWidgetFromUniqueWidgetId(UniqueTextWidgetIndex));

		for (s32 loop = 0; loop < MAX_NUMBER_OF_TEXT_WIDGETS; loop++)
		{
			if (TextWidgets[loop].pWidget == pWidgetToMatch)
			{
				return TextWidgets[loop].WidgetTextContents;
			}
		}

		scriptAssertf(0, "%s:CScriptDebug::GetContentsOfTextWidget - couldn't find a text widget with this ID", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	return NULL;
}

void CScriptDebugTextWidgets::SetContentsOfTextWidget(s32 UniqueTextWidgetIndex, const char *pNewContents)
{
//	scan through the existing widgets for the one that has a widget pointer that matches the widget pointer for TextWidgetIndex
	if(scriptVerifyf(UniqueTextWidgetIndex > 0, "%s:CScriptDebug::SetContentsOfTextWidget - Text Widget Index should be 1 or greater", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		bkText *pWidgetToMatch = static_cast<bkText*>(CScriptWidgetTree::GetWidgetFromUniqueWidgetId(UniqueTextWidgetIndex));

		for (s32 loop = 0; loop < MAX_NUMBER_OF_TEXT_WIDGETS; loop++)
		{
			if (TextWidgets[loop].pWidget == pWidgetToMatch)
			{
				strncpy(TextWidgets[loop].WidgetTextContents, pNewContents, TextEditWidgetForScript::MAX_CHARACTERS_FOR_TEXT_WIDGET);
				TextWidgets[loop].WidgetTextContents[TextEditWidgetForScript::MAX_CHARACTERS_FOR_TEXT_WIDGET-1] = '\0';
				return;
			}
		}
		scriptAssertf(0, "%s:CScriptDebug::SetContentsOfTextWidget - couldn't find a text widget with this ID", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}


void CScriptDebugTextWidgets::ClearTextWidget(bkText *pWidgetToMatch)
{
	for (s32 loop = 0; loop < MAX_NUMBER_OF_TEXT_WIDGETS; loop++)
	{
		if (TextWidgets[loop].pWidget == pWidgetToMatch)
		{
			//	Do I leave it up to the first widget group (the one that should contain all the other widgets)
			//	to actually delete this bkText widget?
			TextWidgets[loop].pWidget = NULL;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//	End of CScriptDebugTextWidgets
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//	CScriptDebugBitFieldWidgets
//////////////////////////////////////////////////////////////////////////

void CScriptDebugBitFieldWidgets::WidgetBitFieldStruct::Init()
{
	pGroup = NULL;	//	pointer to outermost widget group
	for (s32 loop = 0; loop < 32; loop++)
	{
		bBits[loop] = false;
//		pBitToggles[loop] = NULL;		//	or should these be pointers to bkWidget. Do I actually need to store these at all? Are they ever referred to?
	}
	pBitFieldWithinScript = NULL;
	PreviousFrameBitField = 0;
}

void CScriptDebugBitFieldWidgets::Init()
{
	for (u32 Counter = 0; Counter < MAX_NUMBER_OF_BIT_FIELD_WIDGETS; Counter++)
	{
		BitFields[Counter].Init();
	}
}

s32 CScriptDebugBitFieldWidgets::FindFreeBitFieldWidget()
{
	s32 loop = 0;
	while (loop < MAX_NUMBER_OF_BIT_FIELD_WIDGETS)
	{
		if (BitFields[loop].pGroup == NULL)
		{
			return loop;
		}

		loop++;
	}

	scriptAssertf(0, "CScriptDebugBitFieldWidgets::FindFreeBitFieldWidget - no free bit field widgets");
	return -1;
}


s32 CScriptDebugBitFieldWidgets::AddBitFieldWidget(const char *pName, s32& value)
{
	s32 BitFieldWidgetArrayIndex = FindFreeBitFieldWidget();
	if (BitFieldWidgetArrayIndex >= 0)
	{
		if (Verifyf(CScriptDebug::GetWidgetBank(), "There is no script widget bank"))
		{
			BitFields[BitFieldWidgetArrayIndex].pBitFieldWithinScript = &value;
			BitFields[BitFieldWidgetArrayIndex].PreviousFrameBitField = 0;

			BitFields[BitFieldWidgetArrayIndex].pGroup = CScriptDebug::GetWidgetBank()->PushGroup(pName, false);
			s32 UniqueID = CScriptDebug::AddWidget(BitFields[BitFieldWidgetArrayIndex].pGroup, WIDGETTYPE_BITFIELD);

			for (s32 byte_loop = 0; byte_loop < 4; byte_loop++)
			{
				s32 FirstBitIndex = byte_loop * 8;
				s32 LastBitIndex = FirstBitIndex + 7;
				char SubGroupName[16];
				sprintf(SubGroupName, "Bits %d to %d", FirstBitIndex, LastBitIndex);
				/* bkGroup* pByteGroup = */ CScriptDebug::GetWidgetBank()->PushGroup(SubGroupName, false);
				for (s32 bit_loop = FirstBitIndex; bit_loop <= LastBitIndex; bit_loop++)
				{
					char BitName[8];
					sprintf(BitName, "Bit %d", bit_loop);
					BitFields[BitFieldWidgetArrayIndex].bBits[bit_loop] = false;
	//				BitFields[BitFieldWidgetArrayIndex].pBitToggles[bit_loop] = 
					CScriptDebug::GetWidgetBank()->AddToggle(BitName, &BitFields[BitFieldWidgetArrayIndex].bBits[bit_loop]);
				}
				CScriptDebug::GetWidgetBank()->PopGroup();
			}
			CScriptDebug::GetWidgetBank()->PopGroup();

			return UniqueID;
		}
	}
	return 0;
}

void CScriptDebugBitFieldWidgets::UpdateBitFields()
{
	for (s32 loop = 0; loop < MAX_NUMBER_OF_BIT_FIELD_WIDGETS; loop++)
	{
		if (BitFields[loop].pBitFieldWithinScript)
		{
			for (s32 bit_loop = 0; bit_loop < 32; bit_loop++)
			{
				bool bCurrentBit = (*(BitFields[loop].pBitFieldWithinScript) & (0x01 << bit_loop)) != 0;
				bool bPreviousBit = (BitFields[loop].PreviousFrameBitField & (0x01 << bit_loop)) != 0;
				if (bCurrentBit != bPreviousBit)
				{	//	bit has been changed by script so update the widget and the previous bit
					BitFields[loop].bBits[bit_loop] = bCurrentBit;
					if (bCurrentBit)
					{
						BitFields[loop].PreviousFrameBitField |= (0x01 << bit_loop);
					}
					else
					{
						BitFields[loop].PreviousFrameBitField &= ~(0x01 << bit_loop);
					}
				}
				else
				{
					if (BitFields[loop].bBits[bit_loop] != bPreviousBit)
					{	//	widget has changed so update the current and previous bit
						if (BitFields[loop].bBits[bit_loop])
						{
							*(BitFields[loop].pBitFieldWithinScript) |= (0x01 << bit_loop);
							BitFields[loop].PreviousFrameBitField |= (0x01 << bit_loop);
						}
						else
						{
							*(BitFields[loop].pBitFieldWithinScript) &= ~(0x01 << bit_loop);
							BitFields[loop].PreviousFrameBitField &= ~(0x01 << bit_loop);
						}
					}
				}
			}
		}
	}
}

void CScriptDebugBitFieldWidgets::ClearBitFieldWidget(bkGroup *pWidgetGroupToMatch)
{
	for (s32 loop = 0; loop < MAX_NUMBER_OF_BIT_FIELD_WIDGETS; loop++)
	{
		if (BitFields[loop].pGroup == pWidgetGroupToMatch)
		{
			//	Do I leave it up to the first widget group (the one that should contain all the other widgets)
			//	to actually delete the bit field widget group (i.e. the bkGroup and all the bkToggle's it contains)?
			BitFields[loop].Init();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//	End of CScriptDebugBitFieldWidgets
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//	CScriptDebugLocateHandler
//////////////////////////////////////////////////////////////////////////

CScriptDebugLocateHandler::CScriptDebugLocateHandler()
{
	m_AxisAlignLocateName[0] = '\0';
	m_NonAxisAlignLocateName[0] = '\0';

	m_AxisAlignedLocate = false;
	m_NonAxisAlignedLocate = false;
	m_AreaWidth = 1.0f;

	m_SphereLocate = false;
	m_Radius = 1.0f;

	m_fLocateOffset = 0.0f;
	m_fLocateOffset2 = 1.0f;

	m_VecLocateDimensions = VEC3_IDENTITY;
}

void CScriptDebugLocateHandler::CreateWidgets(bkBank* pBank)
{
	const float MAX_LOCATE_SIZE = 10000.0f;	//B*1756412
	pBank->PushGroup("Locates", false);
		pBank->PushGroup("Axis Aligned Locate", false);
			pBank->AddToggle("Activate Axis aligned locate", &m_AxisAlignedLocate);
			pBank->AddSlider("Locate dimension x", &m_VecLocateDimensions.x, 0.0f, MAX_LOCATE_SIZE, 0.25f, NullCB, "Locate x dimension" );
			pBank->AddSlider("Locate dimension y", &m_VecLocateDimensions.y, 0.0f, MAX_LOCATE_SIZE, 0.25f, NullCB, "Locate y dimension" );
			pBank->AddSlider("Locate dimension z", &m_VecLocateDimensions.z, 0.0f, MAX_LOCATE_SIZE, 0.25f, NullCB, "Locate z dimension" );
			pBank->AddSlider("Offset z from ground", &m_fLocateOffset, -MAX_LOCATE_SIZE, MAX_LOCATE_SIZE, 0.25f, NullCB, "Locate z dimension");
			pBank->AddText("Locate Name", m_AxisAlignLocateName, sizeof(m_AxisAlignLocateName), false);
			pBank->AddButton("Print Axis aligned locate info", datCallback(MFA(CScriptDebugLocateHandler::GetLocateDebugInfo), (datBase*)this));
		pBank->PopGroup();
		pBank->PushGroup("Non-Axis Aligned Locate", false);
			pBank->AddToggle("Activate Non-Axis aligned locate", &m_NonAxisAlignedLocate);
			pBank->AddSlider("Pos Vec1 z offset (L Mouse Btn)", &m_fLocateOffset, -MAX_LOCATE_SIZE, MAX_LOCATE_SIZE, 0.25f, NullCB, "Locate z dimension" );
			pBank->AddSlider("Pos Vec2f z offset (R Mouse Btn)", &m_fLocateOffset2, -MAX_LOCATE_SIZE, MAX_LOCATE_SIZE, 0.25f, NullCB, "Locate z dimension" ); 
			pBank->AddSlider("Locate Width", &m_AreaWidth, 0.0f, MAX_LOCATE_SIZE, 0.25f, NullCB, "Locate z dimension" );
			pBank->AddText("Locate Name", m_NonAxisAlignLocateName, sizeof(m_NonAxisAlignLocateName), false);
			pBank->AddButton("Print non-Axis aligned locate info", datCallback(MFA(CScriptDebugLocateHandler::GetLocateDebugInfo), (datBase*)this));
		pBank->PopGroup();
		pBank->PushGroup("Sphere Locate");
			pBank->AddToggle("Sphere locate", &m_SphereLocate);
			pBank->AddSlider("Locate Radius", &m_Radius, 0.0f, MAX_LOCATE_SIZE, 0.25f, NullCB, "Sphere radius" );
			pBank->AddSlider("Offset z from ground", &m_fLocateOffset, -MAX_LOCATE_SIZE, MAX_LOCATE_SIZE, 0.25f, NullCB, "Locate z dimension" );
			pBank->AddButton("Print sphere locate info", datCallback(MFA(CScriptDebugLocateHandler::GetLocateDebugInfo), (datBase*)this));
		pBank->PopGroup(); 
	pBank->PopGroup();
}

void CScriptDebugLocateHandler::DisplayScriptLocates()
{	
	if (m_SphereLocate)
	{
		Vector3 VecCoors = CScriptDebug::GetClickedPos(); 
		VecCoors.z += m_fLocateOffset;

		grcDebugDraw::Sphere(VecCoors, m_Radius, Color32(0,0,255,255) ,false);
	}

	if (m_AxisAlignedLocate)
	{
		Vector3 VecCoors = CScriptDebug::GetClickedPos(); 
		VecCoors.z += m_VecLocateDimensions.z + m_fLocateOffset;

		CScriptDebug::DrawDebugCube((VecCoors.x - m_VecLocateDimensions.x), (VecCoors.y - m_VecLocateDimensions.y), (VecCoors.z - m_VecLocateDimensions.z),
				(VecCoors.x + m_VecLocateDimensions.x), (VecCoors.y + m_VecLocateDimensions.y), (VecCoors.z + m_VecLocateDimensions.z));
	}

	if(m_NonAxisAlignedLocate)
	{
		Vector3 vAreaStart = CScriptDebug::GetClickedPos();
		vAreaStart.z += m_fLocateOffset;

		Vector3 vAreaEnd = CScriptDebug::GetClickedPos2();
		vAreaEnd.z += m_fLocateOffset2;

		float RadiansBetweenFirstTwoPoints;
		float RadiansBetweenPoints1and4;
	
		Vector3 temp_vec;

		if (vAreaStart.z > vAreaEnd.z)
		{
			temp_vec = vAreaStart;
			vAreaStart = vAreaEnd;
			vAreaEnd = temp_vec;
		}

		// calculate the angle between vectors 
		RadiansBetweenFirstTwoPoints = fwAngle::GetRadianAngleBetweenPoints(vAreaStart.x, vAreaStart.y, vAreaEnd.x, vAreaEnd.y);

		// get the right angle from these 2 points
		RadiansBetweenPoints1and4 = RadiansBetweenFirstTwoPoints + (PI / 2);

		//work out a right angle from point

		while (RadiansBetweenPoints1and4 < 0.0f)
		{
			RadiansBetweenPoints1and4 += (2 * PI);
		}

		while (RadiansBetweenPoints1and4 > (2 * PI))
		{
			RadiansBetweenPoints1and4 -= (2 * PI);
		}

		
		const Vector2 vBottomLeft  (((m_AreaWidth * 0.5f) * rage::Sinf(RadiansBetweenPoints1and4)) + vAreaStart.x, ((m_AreaWidth * 0.5f) * -rage::Cosf(RadiansBetweenPoints1and4)) + vAreaStart.y) ;
		const Vector2 vBottomRight (((m_AreaWidth * 0.5f) * rage::Sinf(RadiansBetweenPoints1and4)) + vAreaEnd.x, ((m_AreaWidth * 0.5f) * -rage::Cosf(RadiansBetweenPoints1and4)) + vAreaEnd.y);

		const Vector2 vTopLeft( vAreaStart.x, vAreaStart.y );
		const Vector2 vTopRight( vAreaEnd.x, vAreaEnd.y );

		//calculate the vectors across the locate and to one corner, used when checking against the players vector
		// Vector2 vec1To2 = vTopRight - vTopLeft;
		Vector2 vec1To4 = vTopLeft - vBottomLeft;

		// used for drawing the locate area
		Vector2 vec1To4Copy = vec1To4;

		CScriptDebug::DrawDebugAngledCube(vAreaStart.x + vec1To4Copy.x, vAreaStart.y + vec1To4Copy.y, vAreaStart.z, vAreaEnd.x + vec1To4Copy.x, vAreaEnd.y + vec1To4Copy.y, vAreaEnd.z, vBottomRight.x, vBottomRight.y, 
			vBottomLeft.x, vBottomLeft.y);
	}
}

void CScriptDebugLocateHandler::DisplaySpheresForNonAxisAlignedLocate()
{
	if (m_NonAxisAlignedLocate)
	{
		Vector3 Clickpos = CScriptDebug::GetClickedPos();
		Clickpos.z += m_fLocateOffset;
		grcDebugDraw::Sphere(Clickpos, 0.05f, Color32(1.0f, 0.0f, 0.0f) );

		Clickpos = CScriptDebug::GetClickedPos2();
		grcDebugDraw::Sphere(Clickpos, 0.1f, Color32(1.0f, 0.0f, 1.0f) );
		//	Ask Tom if the following two lines should only be done if(!ms_bUseLineofSight)
		Vector3 vecNormal = CScriptDebug::GetClickedNormal2();
		grcDebugDraw::Line(Clickpos, Clickpos+(vecNormal*0.4f), Color32(1.0f, 0.0f, 1.0f) );

		Clickpos.z += m_fLocateOffset2;
		grcDebugDraw::Sphere(Clickpos, 0.05f, Color32(1.0f, 0.0f, 1.0f) );
	}
}

void CScriptDebugLocateHandler::GetLocateDebugInfo()
{
	Vector3 VecCoors = VEC3_ZERO;
	char text[256]; 
	if (m_SphereLocate)
	{
		VecCoors = CScriptDebug::GetClickedPos();
		if (!CScriptDebug::GetUseLineofSight())
		{	
			VecCoors.z += m_fLocateOffset;
		}

		formatf(text, 256, "\n//sphere Locate Info\nRadius = %f\nPos = <<%f,%f,%f>> ",m_Radius,VecCoors.x, VecCoors.y, VecCoors.z );
		CScriptDebug::OutputScript(text); 
	}
	
	
	if(m_AxisAlignedLocate)
	{
		VecCoors = CScriptDebug::GetClickedPos();
		if (!CScriptDebug::GetUseLineofSight())
		{
			VecCoors.z += m_VecLocateDimensions.z + m_fLocateOffset;
		}
		
		formatf(text, 256, "\n// Locate Info\nLocate Name:%s\nVECTORv %spos = <<%f,%f,%f>> )", m_AxisAlignLocateName, m_AxisAlignLocateName,VecCoors.x, VecCoors.y, VecCoors.z);
		CScriptDebug::OutputScript(text); 

		formatf(text, 256, "\nVECTOR v%sSize = <<%f,%f,%f>>\n", m_AxisAlignLocateName,m_VecLocateDimensions.x, m_VecLocateDimensions.y, m_VecLocateDimensions.z);
		CScriptDebug::OutputScript(text); 
		
		formatf(text, 256, "\nIS_ENTITY_AT_COORD(PedIndex, v%sPos,v%sSize)", m_AxisAlignLocateName,m_AxisAlignLocateName);
		CScriptDebug::OutputScript(text); 
		
		formatf(text, 256, "\nIS_ENTITY_AT_COORD(PedIndex, <<%f,%f,%f>>,<<%f,%f,%f>>)", VecCoors.x,VecCoors.y,VecCoors.z ,m_VecLocateDimensions.x, m_VecLocateDimensions.y, m_VecLocateDimensions.z);
		CScriptDebug::OutputScript(text);
		
		sprintf(m_AxisAlignLocateName, "");

	}

	if(m_NonAxisAlignedLocate)
	{
		VecCoors = CScriptDebug::GetClickedPos();
		VecCoors.z += m_fLocateOffset;

		Vector3 VecCoors2 = CScriptDebug::GetClickedPos2();
		VecCoors2.z += m_fLocateOffset2;

		formatf(text, 256, "// Angled Area Info\nLocate name:%s\nVECTOR v%sPos1 = <<%f,%f,%f>>", m_NonAxisAlignLocateName, m_NonAxisAlignLocateName,VecCoors.x, VecCoors.y, VecCoors.z, m_NonAxisAlignLocateName);
		CScriptDebug::OutputScript(text); 
		
		formatf(text, 256, "VECTOR v%sPos2 = <<%f,%f,%f>> ",m_NonAxisAlignLocateName,VecCoors2.x, VecCoors2.y, VecCoors2.z );
		CScriptDebug::OutputScript(text);

		formatf(text, 256, "FLOAT f%sWidth = %f",m_NonAxisAlignLocateName,m_AreaWidth );
		CScriptDebug::OutputScript(text);
		
		formatf(text, 256, "\nIS_ENTITY_IN_ANGLED_AREA( PedIndex, v%sPos1, v%sPos2, f%sWidth)",m_NonAxisAlignLocateName,m_NonAxisAlignLocateName, m_NonAxisAlignLocateName );
		CScriptDebug::OutputScript(text);
		
		formatf(text, 256, "\nIS_ENTITY_IN_ANGLED_AREA( PedIndex, <<%f,%f,%f>>, <<%f,%f,%f>>, %f)",VecCoors.x, VecCoors.y, VecCoors.z,VecCoors2.x, VecCoors2.y, VecCoors2.z, m_AreaWidth);
		CScriptDebug::OutputScript(text);

		formatf(text, 256, "\nSET_PED_ANGLED_DEFENSIVE_AREA( PedIndex, v%sPos1, v%sPos2, f%sWidth)",m_NonAxisAlignLocateName,m_NonAxisAlignLocateName, m_NonAxisAlignLocateName );
		CScriptDebug::OutputScript(text);

		formatf(text, 256, "\nSET_PED_ANGLED_DEFENSIVE_AREA( PedIndex, <<%f,%f,%f>>, <<%f,%f,%f>>, %f)",VecCoors.x, VecCoors.y, VecCoors.z,VecCoors2.x, VecCoors2.y, VecCoors2.z, m_AreaWidth);
		CScriptDebug::OutputScript(text);
	}
}

//////////////////////////////////////////////////////////////////////////
//	End of CScriptDebugLocateHandler
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//	CScriptDebugPedHandler
//////////////////////////////////////////////////////////////////////////

/*	See if this is needed at all
CScriptDebugPedHandler::CScriptDebugPedHandler()
{
	m_PedPos[0] = '\0';
	m_PedHeading[0] = '\0';
	m_PedName[0] = '\0'; 
	m_PedModelName[0] = '\0';
}
*/

void CScriptDebugPedHandler::CreateWidgets(bkBank* pBank)
{
	pBank->PushGroup("Peds", false);
		pBank->AddButton("Create Ped",datCallback(MFA(CScriptDebugPedHandler::CreateScriptPedUsingTheSelectedModel), (datBase*)this)); 
		m_pedSelector.AddWidgets(pBank, "Ped Model"); 
		pBank->AddText("OR - enter vehicle name", &m_searchPedName[0], STORE_NAME_LENGTH);
		pBank->AddButton("Delete Ped",datCallback(MFA(CScriptDebugPedHandler::DeleteScriptPed), (datBase*)this));
		pBank->AddText("Ped pos", m_PedPos, sizeof(m_PedPos),false, datCallback(MFA(CScriptDebugPedHandler::SetPedPosCB), (datBase*)this)); 
		pBank->AddText("Ped Heading", m_PedHeading, sizeof(m_PedHeading),false, datCallback(MFA(CScriptDebugPedHandler::SetPedHeadingCB), (datBase*)this));
		pBank->AddText("Ped Model Name", m_PedModelName, sizeof(m_PedModelName),false);
			pBank->AddText("Ped Debug Name", m_PedName, sizeof(m_PedName), false, datCallback(MFA(CScriptDebugPedHandler::SetDebugPedName), (datBase*)this));
		pBank->AddButton("Print Ped info", datCallback(MFA(CScriptDebugPedHandler::GetPedDebugInfo), (datBase*)this));
	pBank->PopGroup();
}

void CScriptDebugPedHandler::Process()
{
	CPed *pPed = CPedDebugVisualiserMenu::GetFocusPed();
	if (pPed)
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		sprintf(m_PedPos, "%.2f, %.2f, %.2f", vPedPosition.x, vPedPosition.y, vPedPosition.z);
		sprintf(m_PedHeading, "%.2f", RtoD*pPed->GetTransform().GetHeading());
		sprintf(m_PedName, "%s", pPed->m_debugPedName);
		sprintf(m_PedModelName, "%s", CModelInfo::GetBaseModelInfoName(pPed->GetModelId())); 
	}
	else
	{
		sprintf(m_PedPos, " ");
		sprintf(m_PedHeading, " ");
		sprintf(m_PedName, " ");
	}
}

//	RegdPed pPedScriptPed(NULL);
void CScriptDebugPedHandler::CreateScriptPedUsingTheSelectedModel()
{
	u32 nModelIndex = CModelInfo::GetModelIdFromName(m_searchPedName).GetModelIndex();

	if (CModelInfo::IsValidModelInfo(nModelIndex))
	{
		//load the entered ped
		CreateScriptPed(nModelIndex);
	}
	else
	{
		//load the selected ped
		nModelIndex = CModelInfo::GetModelIdFromName(m_pedSelector.GetSelectedModelName()).GetModelIndex();
		CreateScriptPed(nModelIndex);
	}
}

CPed* CScriptDebugPedHandler::CreateScriptPed(u32 modelIndex)
{
	Matrix34 TempMat;
	TempMat.Identity();
		
	Vector3 vecCreateOffset = CScriptDebug::GetClickedPos();

	vecCreateOffset.z += 1.0f;

	TempMat.d += vecCreateOffset;

	u32 pedInfoIndex = modelIndex;
	
	const CControlledByInfo localAiControl(false, false);

	// don't allow the creation of player ped type - it doesn't work!
	if (pedInfoIndex == CGameWorld::FindLocalPlayer()->GetModelIndex()) {return NULL;}

	fwModelId pedInfoId((strLocalIndex(pedInfoIndex)));
	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(pedInfoId))
	{
		CModelInfo::RequestAssets(pedInfoId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	CPed* pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, pedInfoId, &TempMat, true, true, true);
	pPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL);
	
	pPed->SetupMissionState();

	pPed->PopTypeSet(POPTYPE_TOOL);
	pPed->SetBlockingOfNonTemporaryEvents(true);
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater, false );
	
	pPed->GetPortalTracker()->RequestRescanNextUpdate();
	pPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));

	CGameWorld::Add(pPed, CGameWorld::OUTSIDE );

	return pPed;
}

//delete a created ped
void CScriptDebugPedHandler::DeleteScriptPed()
{
	CPed * pPed = CPedDebugVisualiserMenu::GetFocusPed();

	if (pPed)
	{
		if (!pPed->IsLocalPlayer())	
		{
			CPedFactory::GetFactory()->DestroyPed(pPed);
		}
	}
}

bool CScriptDebugPedHandler::GetVectorFromTextBox(Vector3 &vPos, const char* textbox)
{
	Vector3 vecPos(VEC3_ZERO);

	// Try to parse the line.
	int nItems = 0;
	{
		//	Make a copy of this string as strtok will destroy it.
		Assertf( (strlen(textbox) < 256), "Warp position line is too long to copy.");
		char warpPositionLine[256];
		safecpy(warpPositionLine, textbox, 256);

		// Get the locations to store the float values into.
		float* apVals[3] = {&vecPos.x, &vecPos.y, &vecPos.z};

		// Parse the line.
		char* pToken = NULL;
		const char* seps = " ,\t";
		pToken = strtok(warpPositionLine, seps);
		while(pToken)
		{
			// Try to read the token as a float.
			int success = sscanf(pToken, "%f", apVals[nItems]);
			Assertf(success, "Unrecognized token '%s' in warp position line.",pToken);
			if(success)
			{
				++nItems;
				Assertf((nItems < 4), "Too many tokens in warp position line." );
			}
			pToken = strtok(NULL, seps);
		}
	}

	if(nItems)
	{
		vPos.x = vecPos.x;
		vPos.y = vecPos.y;
		vPos.z = vecPos.z;
		return true; 
	}

	return false; 
}

bool CScriptDebugPedHandler::GetFloatFromTextBox(float &fHeading, const char* textbox)
{
	// Try to parse the line.
	int nItems = 0;
	{
		//	Make a copy of this string as strtok will destroy it.
		Assertf( (strlen(textbox) < 256), "Warp position line is too long to copy.");
		char warpPositionLine[256];
		safecpy(warpPositionLine, textbox, 256);

		// Get the locations to store the float values into.
		float* apVals[1] = {&fHeading};

		// Parse the line.
		char* pToken = NULL;
		const char* seps = " ,\t";
		pToken = strtok(warpPositionLine, seps);
		if(pToken)
		{
			// Try to read the token as a float.
			int success = sscanf(pToken, "%f", apVals[nItems]);
			Assertf(success, "Unrecognized token '%s' in warp position line.",pToken);
			if(success)
			{
				++nItems;
				Assertf((nItems < 2), "Too many tokens in warp position line." );
			}
			pToken = strtok(NULL, seps);
		}
	}

	if(nItems)
	{
		(void)fHeading; 
		return true; 
	}

	return false; 
}


void CScriptDebugPedHandler::SetPedPosCB()
{
	CPed * pPed = CPedDebugVisualiserMenu::GetFocusPed();

	if (pPed)
	{
		Vector3 vecPos(VEC3_ZERO);
		if (GetVectorFromTextBox(vecPos, m_PedPos))
		{
			pPed->Teleport(vecPos, pPed->GetTransform().GetHeading());
		}
	}
}

void CScriptDebugPedHandler::SetPedHeadingCB()
{
	CPed * pPed = CPedDebugVisualiserMenu::GetFocusPed();

	if (pPed)
	{
		float fHeading =0.0f;
		if (GetFloatFromTextBox(fHeading, m_PedHeading))
		{
			pPed->Teleport(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), fHeading*DtoR);
		}
	}
}

//Give the script a debug name
void CScriptDebugPedHandler::SetDebugPedName()
{
	CPed * pPed = CPedDebugVisualiserMenu::GetFocusPed();

	if (pPed)
	{
		strncpy(pPed->m_debugPedName, m_PedName, 15);
		pPed->m_debugPedName[15] = '\0';
	}
}

void CScriptDebugPedHandler::GetPedDebugInfo()
{
	CPed * pPed = CPedDebugVisualiserMenu::GetFocusPed();

	if (pPed)
	{	
		char text[256];
		
		Vector3 vec = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()); 
		
		formatf(text, 256, "// Ped Debug Info\nPED_INDEX\n VECTOR v%sPos = <<%f,%f,%f>>",pPed->m_debugPedName, pPed->m_debugPedName, vec.x, vec.y, vec.z);
		CScriptDebug::OutputScript(text);

		formatf(text, 256, "FLOAT f%sHeading = %f",pPed->m_debugPedName, RtoD*pPed->GetTransform().GetHeading());
		CScriptDebug::OutputScript(text);
		
		formatf(text, 256, "MODEL_NAMES %s",CModelInfo::GetBaseModelInfoName(pPed->GetModelId()));
		CScriptDebug::OutputScript(text);
		
		formatf(text, 256, "\nSET_PED_NAME_DEBUG (%s,%s)\n",pPed->m_debugPedName, pPed->m_debugPedName);
		CScriptDebug::OutputScript(text);
		
		formatf(text, 256, "%s = CREATE_PED( PED_TYPE ,%s,v%sPos, f%sHeading) ",pPed->m_debugPedName, CModelInfo::GetBaseModelInfoName(pPed->GetModelId()), pPed->m_debugPedName, pPed->m_debugPedName);
		CScriptDebug::OutputScript(text);
		
		formatf(text, 256, "SET_ENTITY_COORDS (%s, v%sPos) ",pPed->m_debugPedName, pPed->m_debugPedName);
		CScriptDebug::OutputScript(text);

		formatf(text, 256, "SET_PED_COORDS_KEEP_VEHICLE (%s, v%sPos) ",pPed->m_debugPedName, pPed->m_debugPedName);
		CScriptDebug::OutputScript(text);
		
		formatf(text, 256, "SET_ENTITY_HEADING (%s, f%sHeading)",pPed->m_debugPedName, pPed->m_debugPedName);
		CScriptDebug::OutputScript(text);
		
		formatf(text, 256, "\nSET_PED_NAME_DEBUG (PedIndex,%s)\n",pPed->m_debugPedName, pPed->m_debugPedName);
		CScriptDebug::OutputScript(text);

		formatf(text, 256, "PedIndex = CREATE_PED (PED_TYPE ,%s, <<%f,%f,%f>>, %f) ",CModelInfo::GetBaseModelInfoName(pPed->GetModelId()),vec.x, vec.y, vec.z,RtoD*pPed->GetTransform().GetHeading() );
		CScriptDebug::OutputScript(text);

		formatf(text, 256, "SET_ENTITY_COORDS (PedIndex, <<%f,%f,%f>>) ",vec.x, vec.y, vec.z);
		CScriptDebug::OutputScript(text);

		formatf(text, 256, "SET_PED_COORDS_KEEP_VEHICLE (PedIndex, <<%f,%f,%f>>) ",vec.x, vec.y, vec.z);
		CScriptDebug::OutputScript(text);
		
		formatf(text, 256, "SET_ENTITY_HEADING (PedIndex, %f) ",RtoD*pPed->GetTransform().GetHeading());
		CScriptDebug::OutputScript(text);
	
	}
}

//////////////////////////////////////////////////////////////////////////
//	End of CScriptDebugPedHandler
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//	CScriptDebugVehicleHandler
//////////////////////////////////////////////////////////////////////////

/*	See if this is needed at all
CScriptDebugVehicleHandler::CScriptDebugVehicleHandler()
{
	m_VehiclePos[0] = '\0';
	m_VehicleHeading[0] = '\0';
	m_VehicleName[0] = '\0'; 
	m_VehicleModelName[0] = '\0';
}
*/

void CScriptDebugVehicleHandler::CreateWidgets(bkBank* pBank)
{
	pBank->PushGroup("Vehicle", false);
		pBank->AddButton("Create Vehicle", datCallback(MFA(CScriptDebugVehicleHandler::CreateScriptVehicleUsingTheSelectedModel), (datBase*)this));
		m_vehicleSelector.AddWidgets(pBank, "Vehicle Model");
		pBank->AddText("OR - enter vehicle name", &m_vehicleSearchName[0], STORE_NAME_LENGTH);
		pBank->AddButton("Delete Vehicle", datCallback(MFA(CScriptDebugVehicleHandler::DeleteScriptVehicle), (datBase*)this));
		pBank->AddText("Vehicle Pos", m_VehiclePos, sizeof(m_VehiclePos),false, datCallback(MFA(CScriptDebugVehicleHandler::SetVehiclePosCB), (datBase*)this));
		pBank->AddText("Vehicle Heading", m_VehicleHeading, sizeof(m_VehicleHeading),false, datCallback(MFA(CScriptDebugVehicleHandler::SetVehicleHeadingCB), (datBase*)this));
		pBank->AddText("Vehicle Debug Name", m_VehicleName, sizeof(m_VehicleName), false, datCallback(MFA(CScriptDebugVehicleHandler::SetDebugVehicleName), (datBase*)this));
		pBank->AddText("Vehicle Model Name", m_VehicleModelName, sizeof(m_VehicleModelName),false);
		pBank->AddButton("Set Vehicle on ground", datCallback(MFA(CScriptDebugVehicleHandler::SetVehicleOnGround), (datBase*)this));
		pBank->AddButton("Print Vehicle info", datCallback(MFA(CScriptDebugVehicleHandler::GetVehicleDebugInfo), (datBase*)this));
	pBank->PopGroup();
}

void CScriptDebugVehicleHandler::RemoveWidgets()
{
	m_vehicleSelector.RemoveWidgets(); 
}

void CScriptDebugVehicleHandler::Process()
{
	CVehicle * pVehicle = NULL;

	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && (pEnt->GetType()==ENTITY_TYPE_VEHICLE))
		{
			pVehicle = (CVehicle*)pEnt;
			const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
			sprintf(m_VehiclePos, "%.2f, %.2f, %.2f", vVehiclePosition.x, vVehiclePosition.y, vVehiclePosition.z);
		
			float ReturnHeading = ( RtoD * pVehicle->GetTransform().GetHeading());
			if (ReturnHeading < 0.0f)
			{
				ReturnHeading += 360.0f;
			}
			if (ReturnHeading > 360.0f)
			{
				ReturnHeading -= 360.0f;
			}
			
			sprintf(m_VehicleHeading, "%.2f", ReturnHeading);
			sprintf(m_VehicleName, "%s", pVehicle->GetDebugName());
			sprintf(m_VehicleModelName, "%s", CModelInfo::GetBaseModelInfoName(pVehicle->GetModelId())); 
		}
	}

	if (!pVehicle)
	{
		sprintf(m_VehiclePos, " ");
		sprintf(m_VehicleHeading, " ");
		sprintf(m_VehicleName, " ");
	}
}

void CScriptDebugVehicleHandler::CreateScriptVehicleUsingTheSelectedModel()
{
	u32 nModelIndex = CModelInfo::GetModelIdFromName(m_vehicleSearchName).GetModelIndex();

	if (CModelInfo::IsValidModelInfo(nModelIndex))
	{
		//load the entered object
		CreateScriptVehicle(nModelIndex);
	}
	else
	{
		//load the selected object
		nModelIndex = CModelInfo::GetModelIdFromName(m_vehicleSelector.GetSelectedName()).GetModelIndex();
		CreateScriptVehicle(nModelIndex);
	}

}

CVehicle* CScriptDebugVehicleHandler::CreateScriptVehicle(u32 vehicleModelIndex)
{
	float carDropRotation = 0.0f;

	fwModelId vehicleModelId((strLocalIndex(vehicleModelIndex)));
	CVehicleModelInfo *pVehModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(vehicleModelId);

	bool bForceLoad = false;
	if(!CModelInfo::HaveAssetsLoaded(vehicleModelId))
	{
		CModelInfo::RequestAssets(vehicleModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		bForceLoad = true;
	}

	if(bForceLoad)
	{
		CStreaming::LoadAllRequestedObjects(true);
	}

	if(!CModelInfo::HaveAssetsLoaded(vehicleModelId))
	{
		return NULL;
	}

	Matrix34 tempMat;
	tempMat.Identity();
	tempMat.RotateZ(carDropRotation);

	Vector3 vecCreateOffset = CScriptDebug::GetClickedPos();
		
	vecCreateOffset.z -= (pVehModelInfo->GetBoundingBoxMin()).z;

	tempMat.d += vecCreateOffset ;

	CVehicle *pNewVehicle = CVehicleFactory::GetFactory()->Create(vehicleModelId, ENTITY_OWNEDBY_SCRIPT, POPTYPE_MISSION, &tempMat);

	if(pNewVehicle)
	{
		pNewVehicle->SetupMissionState();

		CGameWorld::Add(pNewVehicle, CGameWorld::OUTSIDE);
		pNewVehicle->GetPortalTracker()->RequestRescanNextUpdate();

		if (pNewVehicle->GetCarDoorLocks() == CARLOCK_LOCKED_INITIALLY)		// The game creates police cars with doors locked. For debug cars we don't want this.
		{
			pNewVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
		}
		pNewVehicle->PlaceOnRoadAdjust();
		pNewVehicle->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pNewVehicle->GetTransform().GetPosition()));
	}

	return pNewVehicle;
}

void CScriptDebugVehicleHandler::DeleteScriptVehicle()
{
	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && (pEnt->GetType()==ENTITY_TYPE_VEHICLE))
		{
			CVehicleFactory::GetFactory()->Destroy(static_cast<CVehicle*>(pEnt));
		}
	}
}

void CScriptDebugVehicleHandler::SetDebugVehicleName()
{
	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && (pEnt->GetIsTypeVehicle()))
		{
			CVehicle * pVehicle = (CVehicle*)pEnt;
			pVehicle->SetDebugName(m_VehicleName);
		}
	}
}

void CScriptDebugVehicleHandler::SetVehicleOnGround()
{
	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && (pEnt->GetIsTypeVehicle()))
		{
			CVehicle * pVehicle = (CVehicle*)pEnt;
			pVehicle->PlaceOnRoadAdjust();
		}
	}
}

void  CScriptDebugVehicleHandler::SetVehicleHeadingCB()
{
	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && (pEnt->GetIsTypeVehicle()))
		{
			float fHeading = 0.0f; 
			if (CScriptDebugPedHandler::GetFloatFromTextBox(fHeading, m_VehicleHeading))
			{
				CVehicle * pVehicle = (CVehicle*)pEnt;

				pVehicle->SetHeading(fHeading*DtoR);
			}
		}
	}
}

void  CScriptDebugVehicleHandler::SetVehiclePosCB()
{
	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && (pEnt->GetIsTypeVehicle()))
		{
			Vector3 vPos(VEC3_ZERO); 
			if (CScriptDebugPedHandler::GetVectorFromTextBox(vPos, m_VehiclePos))
			{
				CVehicle * pVehicle = (CVehicle*)pEnt;
				pVehicle->Teleport(vPos, pVehicle->GetTransform().GetHeading());
			}
		}
	}
}


void CScriptDebugVehicleHandler::GetVehicleDebugInfo()
{
	CVehicle * pVehicle = NULL;
	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && (pEnt->GetType()==ENTITY_TYPE_VEHICLE))
		{
			pVehicle = (CVehicle*)pEnt;
			char text[256];

			Vector3 vec = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

			formatf(text, 256, "\n//Vehicle Debug Info\nVEHICLE_INDEX %s \n VECTOR v%sPos = <<%f,%f,%f>>",pVehicle->GetDebugName(), pVehicle->GetDebugName(), vec.x, vec.y, vec.z);
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "FLOAT f%sHeading = %f",pVehicle->GetDebugName(), RtoD*pVehicle->GetTransform().GetHeading());
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "MODEL_NAMES %s",CModelInfo::GetBaseModelInfoName(pVehicle->GetModelId()));
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "\nSET_VEHICLE_NAME_DEBUG (%s,%s)\n",pVehicle->GetDebugName(), pVehicle->GetDebugName());
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "%s = CREATE_VEHICLE( %s ,v%sPos, f%sHeading) ",pVehicle->GetDebugName(),CModelInfo::GetBaseModelInfoName(pVehicle->GetModelId()), pVehicle->GetDebugName(), pVehicle->GetDebugName());
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "SET_ENTITY_COORDS (%s, v%sPos) ",pVehicle->GetDebugName(), pVehicle->GetDebugName());
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "SET_ENTITY_HEADING (%s, f%sHeading)",pVehicle->GetDebugName(), pVehicle->GetDebugName());
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "\nSET_VEHICLE_NAME_DEBUG (VehicleIndex,%s)\n",pVehicle->GetDebugName(), pVehicle->GetDebugName());
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "VehicleIndex = CREATE_VEHICLE (%s, <<%f,%f,%f>>, %f) ",CModelInfo::GetBaseModelInfoName(pVehicle->GetModelId()),vec.x, vec.y, vec.z,RtoD*pVehicle->GetTransform().GetHeading() );
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "SET_ENTITY_COORDS (VehicleIndex, <<%f,%f,%f>>) ",vec.x, vec.y, vec.z);
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "SET_ENTITY_HEADING (VehicleIndex, %f) ",RtoD*pVehicle->GetTransform().GetHeading());
			CScriptDebug::OutputScript(text);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//	End of CScriptDebugVehicleHandler
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//	CScriptDebugObjectHandler
//////////////////////////////////////////////////////////////////////////

void CScriptDebugObjectHandler::CreateWidgets(bkBank* pBank)
{
	pBank->PushGroup("Object", false);
		m_objectSelector.AddWidgets(pBank, "Object model");
		pBank->AddText("OR - enter object name", &m_ObjectModelName[0], STORE_NAME_LENGTH);
		pBank->AddButton("Reload model names", datCallback(MFA(CScriptDebugObjectHandler::ReloadModelList), (datBase*)this));
		pBank->AddButton("Create Object", datCallback(MFA(CScriptDebugObjectHandler::CreateScriptObjectUsingTheSelectedModel), (datBase*)this));
		pBank->AddButton("Delete Object", datCallback(MFA(CScriptDebugObjectHandler::DeleteScriptObject), (datBase*)this));
		const char *BuildingNameList[] = { "(none)" };
	//	pBank->AddCombo("Parent bone", &m_parentBone, m_parentBoneList.GetCount(), &m_parentBoneList[0]);
		m_CreatedBuildingsList = pBank->AddCombo("BuildingNames", &CreatedBuildingIndex, m_CreatedBuildings.GetCount(), BuildingNameList); 
		
		pBank->AddText("Entity Type" , m_ObjectType, sizeof(m_ObjectType),false);
		pBank->AddText("Pos z:shift&mse up-dwn)" , m_ObjectPos, sizeof(m_ObjectPos),false, datCallback(MFA(CScriptDebugObjectHandler::SetObjectPosCB), (datBase*)this));
		pBank->AddText("Rotation" , m_ObjectRot, sizeof(m_ObjectRot),false, datCallback(MFA(CScriptDebugObjectHandler::SetObjectRotCB), (datBase*)this));
		pBank->AddText("Model Name" , m_ObjectName, sizeof(m_ObjectName), false);
		pBank->AddButton("Activate physics", datCallback(MFA(CScriptDebugObjectHandler::ActivateObjectPhysics), (datBase*)this));
		pBank->AddText("Object Debug Name" , m_ObjectDebugName, sizeof(m_ObjectDebugName), false);
		pBank->AddButton("Print Object info", datCallback(MFA(CScriptDebugObjectHandler::GetObjectDebugInfo), (datBase*)this));
	pBank->PopGroup();

	if(m_CreatedBuildingsList)
	{
		m_CreatedBuildingsList->UpdateCombo( "BuildingList", &CreatedBuildingIndex, 1, NULL );
		m_CreatedBuildingsList->SetString(0,"none"); 
	}
}

void CScriptDebugObjectHandler::RemoveWidgets()
{
	m_objectSelector.RemoveWidgets();
}

void CScriptDebugObjectHandler::Process()
{
	CObject * pObject = NULL;

	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if (pEnt && pEnt->GetIsTypeObject())
		{
			pObject = (CObject*)pEnt;

			Vector3 vOut = VEC3_ZERO;
			Matrix34 m = MAT34V_TO_MATRIX34(pObject->GetMatrix());
			m.ToEulersXYZ(vOut);
			vOut *= RtoD;
			sprintf(m_ObjectType, "Object");
			const Vector3 vObjectPosition = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
			sprintf(m_ObjectPos, "%.2f, %.2f, %.2f", vObjectPosition.x, vObjectPosition.y, vObjectPosition.z);
			sprintf(m_ObjectRot, "%.2f, %.2f, %.2f", vOut.x, vOut.y, vOut.z);
			sprintf(m_ObjectName, "%s", CModelInfo::GetBaseModelInfoName(pObject->GetModelId()));
		}
		else
		{
			if(pEnt)
			{
				sprintf(m_ObjectType, "Building: Cannot create in script");
				sprintf(m_ObjectName, "%s", CModelInfo::GetBaseModelInfoName(pEnt->GetModelId()));
			}
		}
	}
}

void CScriptDebugObjectHandler::CreateScriptObjectUsingTheSelectedModel()
{
	//check if the user typed in a name rather than selecting one
	u32 nModelIndex = CModelInfo::GetModelIdFromName(m_ObjectModelName).GetModelIndex();

	if (CModelInfo::IsValidModelInfo(nModelIndex))
	{
		//load the entered object
		CreateScriptObject(nModelIndex);
	}
	else
	{
		//load the selected object
		CreateScriptObject(m_objectSelector.GetSelectedModelId().GetModelIndex());
	}
}

CObject* CScriptDebugObjectHandler::CreateScriptObject(u32 modelIndex)
{
	fwModelId modelId((strLocalIndex(modelIndex)));
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	if(pModelInfo==NULL)
	{
		Displayf ("CScriptDebug::CreateScriptObject - Invalid model: %d", modelIndex);
		return NULL;
	}
	if(pModelInfo)
	{
		RegdEnt pEnt(NULL); 

		if(pModelInfo->GetIsTypeObject())
		{
			pEnt = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_RANDOM, true);
		}
		else
		{
			scriptDisplayf("This is a building the asset will need to be changed if its going to be created by script");
			if (pModelInfo->GetModelType() == MI_TYPE_COMPOSITE){
				pEnt = rage_new CCompEntity( ENTITY_OWNEDBY_DEBUG );
			} else {
				pEnt = rage_new CBuilding( ENTITY_OWNEDBY_DEBUG );
			}

			if(pEnt)
			{
				pEnt->SetModelId(fwModelId(strLocalIndex(modelIndex)));

				BuidlingDescription BuildingDesc;
				BuildingDesc.pEnt = pEnt; 
				formatf(BuildingDesc.ObjectModelName, 256, "%s", pEnt->GetModelName()); 
				m_CreatedBuildings.PushAndGrow(BuildingDesc); 
				PopulateBuidingList(); 
				formatf(m_ObjectType, 256, "building, need asset fixed to be created by script"); 
			}
			
		}
		
		if (pEnt)
		{
			Matrix34 tempMat;
			tempMat.Identity();
			Vector3 vecCreateOffset = CScriptDebug::GetClickedPos();

			vecCreateOffset.z -= (pEnt->GetBoundingBoxMin()).z;

			tempMat.d += vecCreateOffset ;

			if(pEnt)
			{
				if (!pEnt->GetTransform().IsMatrix())
				{
					#if ENABLE_MATRIX_MEMBER
					Mat34V trans = MATRIX34_TO_MAT34V(tempMat);					
					pEnt->SetTransform(trans);
					pEnt->SetTransformScale(1.0f, 1.0f);		
					#else
					fwMatrixTransform* trans = rage_new fwMatrixTransform(MATRIX34_TO_MAT34V(tempMat));
					pEnt->SetTransform(trans);
					#endif
				}
				pEnt->SetMatrix(tempMat);

				if (fragInst* pInst = pEnt->GetFragInst())
				{
					pInst->SetResetMatrix(tempMat);
				}
				CGameWorld::Add(pEnt, CGameWorld::OUTSIDE );
			}

			if(pEnt->GetIsTypeObject())
			{
				CObject* pDynamicEnt = static_cast<CObject*>(pEnt.Get()); 
				pDynamicEnt->GetPortalTracker()->RequestRescanNextUpdate();
				pDynamicEnt->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition()));

				pDynamicEnt->SetupMissionState();
			}
		}


		// pEnt physics for static xrefs
		if (pEnt && pEnt->GetIsTypeBuilding()
			&& pEnt->GetCurrentPhysicsInst()==NULL && pEnt->IsBaseFlagSet(fwEntity::HAS_PHYSICS_DICT))
		{
			// create physics
			pEnt->InitPhys();
			if (pEnt->GetCurrentPhysicsInst())
			{
				pEnt->AddPhysics();
			}
		}

		if (pEnt && pEnt->GetIsTypeObject())
		{
			return static_cast<CObject*>(pEnt.Get());
		}

	}
	return NULL;
}

void CScriptDebugObjectHandler::DeleteScriptObject()
{
	CObject * pObject = NULL;

	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if (pEnt && pEnt->GetIsTypeObject())
		{
			pObject = (CObject*)pEnt;
	
			if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
			{
				CObjectPopulation::DestroyObject(pObject);
			}
		}
	}
	if(CreatedBuildingIndex>0)
	{
		if(m_CreatedBuildings[CreatedBuildingIndex-1].pEnt)
		{
			CGameWorld::Remove(m_CreatedBuildings[CreatedBuildingIndex-1].pEnt);
			delete m_CreatedBuildings[CreatedBuildingIndex-1].pEnt; 
			m_CreatedBuildings[CreatedBuildingIndex-1].pEnt = NULL; 
			m_CreatedBuildings.Delete(CreatedBuildingIndex-1); 
			PopulateBuidingList(); 
		}
	}
}

void CScriptDebugObjectHandler::ActivateObjectPhysics()
{
	CObject * pObject = NULL;

	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if (pEnt && pEnt->GetIsTypeObject())
		{
			pObject = (CObject*)pEnt;

			pObject->ActivatePhysics();
		}
	}
}

void CScriptDebugObjectHandler::ReloadModelList()
{
	m_objectSelector.RefreshList();
}

void  CScriptDebugObjectHandler::SetObjectRotCB()
{
	CObject * pObject = NULL;
	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if (pEnt && pEnt->GetIsTypeObject())
		{
			Vector3 vRot(VEC3_ZERO); 
			if (CScriptDebugPedHandler::GetVectorFromTextBox(vRot, m_ObjectRot))
			{
				pObject = (CObject*)pEnt;
				
				Matrix34 mMat; 
				mMat.Identity(); 

				mMat.d = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
				
				mMat.FromEulersXYZ(vRot*DtoR); 
				
				pObject->SetMatrix(mMat);
			}
		}
	}
}

void  CScriptDebugObjectHandler::SetObjectPosCB()
{
	CObject * pObject = NULL;
	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if (pEnt && pEnt->GetIsTypeObject())
		{
			Vector3 vPos(VEC3_ZERO); 
			if (CScriptDebugPedHandler::GetVectorFromTextBox(vPos, m_ObjectPos))
			{
				pObject = (CObject*)pEnt;
				pObject->Teleport(vPos, pObject->GetTransform().GetHeading());
			}
		}
	}
}

void CScriptDebugObjectHandler::PopulateBuidingList()
{	
	if(m_CreatedBuildingsList)
	{
		m_CreatedBuildingsList->UpdateCombo( "BuildingList", &CreatedBuildingIndex, m_CreatedBuildings.GetCount()+1, NULL );
		
		for (s32 i=0; i < m_CreatedBuildings.GetCount(); i++)
		{
			m_CreatedBuildingsList->SetString( i+1, m_CreatedBuildings[i].ObjectModelName);
		}
	}
}

void CScriptDebugObjectHandler::GetObjectDebugInfo()
{
	CObject * pObject = NULL;

	for(s32 i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if (pEnt && pEnt->GetIsTypeObject())
		{
			char text[256]; 

			pObject = (CObject*)pEnt;
			
			Vector3 vec = VEC3_ZERO;
			Matrix34 m = MAT34V_TO_MATRIX34(pObject->GetMatrix());
			m.ToEulersXYZ(vec);
			vec *= RtoD;
			
			Vector3 vecPos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());

			formatf(text, 256, "\n// Object Debug Info:\nOBJECT_INDEX %s \nVECTOR v%sPos = <<%f,%f,%f>>",m_ObjectDebugName, m_ObjectDebugName,vecPos.x, vecPos.y, vecPos.z );
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "VECTOR v%Rot = <<%f,%f,%f>>",m_ObjectDebugName, vec.x, vec.y, vec.z);
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "MODEL_NAMES %s",CModelInfo::GetBaseModelInfoName(pObject->GetModelId()));
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "\n%s = CREATE_OBJECT( %s ,v%sPos)\n",m_ObjectDebugName, CModelInfo::GetBaseModelInfoName(pObject->GetModelId()), m_ObjectDebugName);
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "SET_ENTITY_COORDS (%s, v%sPos) ",m_ObjectDebugName, m_ObjectDebugName);
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "SET_ENTITY_ROTATION (%s, v%sRot)",m_ObjectDebugName, m_ObjectDebugName);
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "\nObjectIndex = CREATE_OBJECT (%s, <<%f,%f,%f>>)\n",CModelInfo::GetBaseModelInfoName(pObject->GetModelId()),vecPos.x, vecPos.y, vecPos.z);
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "SET_ENTITY_COORDS (ObjectIndex, <<%f,%f,%f>>)",vecPos.x, vecPos.y, vecPos.z);
			CScriptDebug::OutputScript(text);

			formatf(text, 256, "SET_ENTITY_ROTATION (ObjectIndex, <<%f,%f,%f>>)",vec.x, vec.y, vec.z);
			CScriptDebug::OutputScript(text);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//	End of CScriptDebugObjectHandler
//////////////////////////////////////////////////////////////////////////

#endif	//	__BANK


//////////////////////////////////////////////////////////////////////////
//	Squares and Cubes
//////////////////////////////////////////////////////////////////////////

#if DEBUG_DRAW
const Color32 DbgLineColour(0, 0, 255, 255);

void CalculateGroundZForVector(Vector3 &vecCoord)
{
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestHitPoint hitPoint;
	WorldProbe::CShapeTestResults testResult(hitPoint);

	probeDesc.SetStartAndEnd(Vector3(vecCoord.x, vecCoord.y, -1000.0f), Vector3(vecCoord.x, vecCoord.y, 1000.0f));
	probeDesc.SetResultsStructure(&testResult);
	probeDesc.SetExcludeEntity(NULL);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
	vecCoord.z = testResult[0].GetHitPosition().z + 2.0f;	// Small safety margin
}

void CScriptDebug::DrawDebugSquare(float X1, float Y1, float X2, float Y2)
{
	Vector3 LinePoint1(X1, Y1, -1000.0f);
	Vector3 LinePoint2(X2, Y1, -1000.0f);
	Vector3 LinePoint3(X2, Y2, -1000.0f);
	Vector3 LinePoint4(X1, Y2, -1000.0f);

	CalculateGroundZForVector(LinePoint1);
	CalculateGroundZForVector(LinePoint2);
	CalculateGroundZForVector(LinePoint3);
	CalculateGroundZForVector(LinePoint4);

	grcDebugDraw::Line(LinePoint1, LinePoint2, DbgLineColour);
	grcDebugDraw::Line(LinePoint2, LinePoint3, DbgLineColour);
	grcDebugDraw::Line(LinePoint3, LinePoint4, DbgLineColour);
	grcDebugDraw::Line(LinePoint4, LinePoint1, DbgLineColour);
}

void CScriptDebug::DrawDebugAngledSquare(float X1, float Y1, float X2, float Y2, float X3, float Y3, float X4, float Y4)
{
	Vector3 LinePoint1(X1, Y1, -1000.0f);
	Vector3 LinePoint2(X2, Y2, -1000.0f);
	Vector3 LinePoint3(X3, Y3, -1000.0f);
	Vector3 LinePoint4(X4, Y4, -1000.0f);

	CalculateGroundZForVector(LinePoint1);
	CalculateGroundZForVector(LinePoint2);
	CalculateGroundZForVector(LinePoint3);
	CalculateGroundZForVector(LinePoint4);

	grcDebugDraw::Line(LinePoint1, LinePoint2,	DbgLineColour);
	grcDebugDraw::Line(LinePoint2, LinePoint3,	DbgLineColour);
	grcDebugDraw::Line(LinePoint3, LinePoint4,	DbgLineColour);
	grcDebugDraw::Line(LinePoint4, LinePoint1,	DbgLineColour);
}

void CScriptDebug::DrawDebugCube(float X1, float Y1, float Z1, float X2, float Y2, float Z2)
{
	grcDebugDraw::BoxAxisAligned(Vec3V(X1,Y1,Z1),Vec3V(X2,Y2,Z2),DbgLineColour, false);
}

void CScriptDebug::DrawDebugAngledCube(float X1, float Y1, float Z1, float X2, float Y2, float Z2, float X3, float Y3, float X4, float Y4, const Color32 &colour)
{
	Vector3 LinePointTop1(X1, Y1, Z1);
	Vector3 LinePointTop2(X2, Y2, Z1);
	Vector3 LinePointTop3(X3, Y3, Z1);
	Vector3 LinePointTop4(X4, Y4, Z1);

	Vector3 LinePointBottom1(X1, Y1, Z2);
	Vector3 LinePointBottom2(X2, Y2, Z2);
	Vector3 LinePointBottom3(X3, Y3, Z2);
	Vector3 LinePointBottom4(X4, Y4, Z2);

	// Top square
	grcDebugDraw::Line(LinePointTop1,LinePointTop2,colour);
	grcDebugDraw::Line(LinePointTop2,LinePointTop3,colour);
	grcDebugDraw::Line(LinePointTop3,LinePointTop4,colour);
	grcDebugDraw::Line(LinePointTop4,LinePointTop1,colour);

	// Bottom square
	grcDebugDraw::Line(LinePointBottom1,LinePointBottom2,colour);
	grcDebugDraw::Line(LinePointBottom2,LinePointBottom3,colour);
	grcDebugDraw::Line(LinePointBottom3,LinePointBottom4,colour);
	grcDebugDraw::Line(LinePointBottom4,LinePointBottom1,colour);

	// Vertical lines
	grcDebugDraw::Line(LinePointBottom1,LinePointTop1,colour);
	grcDebugDraw::Line(LinePointBottom2,LinePointTop2,colour);
	grcDebugDraw::Line(LinePointBottom3,LinePointTop3,colour);
	grcDebugDraw::Line(LinePointBottom4,LinePointTop4,colour);
}
#else // DEBUG_DRAW
void CScriptDebug::DrawDebugSquare(float, float, float, float) { /* NoOp */ }
void CScriptDebug::DrawDebugAngledSquare(float, float, float, float, float, float, float, float) { /* NoOp */ }
void CScriptDebug::DrawDebugCube(float, float, float, float, float, float) { /* NoOp */ }
void CScriptDebug::DrawDebugAngledCube(float, float, float, float, float, float, float, float, float , float, const Color32 &) { /* NoOp */ }
#endif // DEBUG_DRAW

//////////////////////////////////////////////////////////////////////////
//	End of Squares and Cubes
//////////////////////////////////////////////////////////////////////////

