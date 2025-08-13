/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : NodeViewer.cpp
// PURPOSE : Allows nodes to be added and edited around the map and saved out
// AUTHOR  : Derek Payne
// STARTED : 27/05/2010
//
// UPDATED : 11/09/2010
// AUTHOR  : Adam Munson
//
/////////////////////////////////////////////////////////////////////////////////

#if __BANK

#include "NodeViewer.h"

#include "BlockView.h"							// Used to find current block nodes are created in

#include "camera/CamInterface.h"				// camera
#include "camera/debug/debugdirector.h"		
#include "camera/debug/FreeCamera.h"		
#include "camera/helpers/Frame.h"
#include "frontend/NewHud.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/PedModelInfo.h"
#include "parser/manager.h"						// XML
#include "parser/tree.h"
#include "parser/treenode.h"
#include "parsercore/attribute.h"
#include "parsercore/element.h"
#include "parsercore/stream.h"
#include "Peds/PedFactory.h"					// for Scalejaxx ped creation
#include "Peds/PedIntelligence.h"
#include "Peds/ped.h"
#include "physics/WorldProbe/worldprobe.h"					// findground pos
#include "scene/world/gameWorld.h"
#include "streaming/streaming.h"
#include "system/controlmgr.h"					// controlmgr
#include "system/FileMgr.h"
#include "system/pad.h"							// pad
#include "task/General/TaskBasic.h"
#include "text/text.h"
#include "text/TextConversion.h"

#define NOTE_NODES_FILE_VERSION					(1)
#define NOTE_NODES_VIEWER_GROUP_NAME			"Node Viewer"
#define NOTE_NODES_DEBUG_BANK_NAME				"Debug"
#define NOTE_NODES_POINT_NODE_SIZE				(1.65f)
#define NOTE_NODES_BOX_NODE_SIZE				(1.4f)
#define NOTE_NODES_NODE_ALPHA					(170)
#define NOTE_NODES_EQUIDISTANT_DROP_DISTANCE	(50.0f)
#define NOTE_NODES_MAX_NODE_SIZE				(12.0f)
#define NOTE_NODES_MIN_NODE_SIZE				(1.0f)

atArray<NoteNodeSet> CNodeViewer::sm_NodeSets;
atArray<Color32> CNodeViewer::sm_Colours;
atArray<RegdPed> CNodeViewer::sm_ScalejaxxPeds;

static const char *s_NodeColour[] =
{
	"Default (Blue)",
	"Green",
	"Purple",
	"Yellow",
	"Orange",
	"Brown",
	"Black"
};

static const char *s_NodeType[] =
{
	"Point",
	"Link",
	"Box",
	"Arrow",
	"ScaleJaxx"
};

float CNodeViewer::sm_DistanceBetweenDrops;
float CNodeViewer::sm_NodeSize;
float CNodeViewer::sm_DrawDistance;

bkGroup* CNodeViewer::sm_BankGroupMain		= NULL;
bkGroup* CNodeViewer::sm_BankEditControl	= NULL;

CNodeViewer::eEditorLevel CNodeViewer::sm_EditorLevel;

s32 CNodeViewer::sm_CurrentSet;
s32 CNodeViewer::sm_CurrentSelected;
s32 CNodeViewer::sm_TextFieldCursor;
s32 CNodeViewer::sm_FirstLinkNode;
s32 CNodeViewer::sm_FirstLinkSet;
s32 CNodeViewer::sm_FirstLinkType;
s32 CNodeViewer::sm_ColourSelection;
s32 CNodeViewer::sm_NodeLinkType;
s32 CNodeViewer::sm_OriginalKeyboardMode;

char CNodeViewer::sm_TextField[NOTE_NODES_CHAR_ARRAY_LENGTH];
char CNodeViewer::sm_GlobalName[NOTE_NODES_CHAR_ARRAY_LENGTH];
char CNodeViewer::sm_GlobalSetName[NOTE_NODES_CHAR_ARRAY_LENGTH];
char CNodeViewer::sm_XmlFilename[128];

atString CNodeViewer::sm_XmlSaveLocation;
atString CNodeViewer::sm_XmlAutosaveFilename;

bool CNodeViewer::sm_IsActive = false;
bool CNodeViewer::sm_IsCreatingLink;
bool CNodeViewer::sm_IsEditMode = true;
bool CNodeViewer::sm_IsEquidistantDrop;
bool CNodeViewer::sm_IsShowAllNodesAndSets;


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetAllNodesEmpty()
// PURPOSE:	clears all the nodes arrays
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetAllNodesEmpty()
{

	// Reset the node arrays
	for (s32 s = 0; s < sm_NodeSets.GetCount(); s++)
	{
		for (s32 n = 0; n < sm_NodeSets[s].nodes.GetCount(); n++)
		{
			sm_NodeSets[s].nodes.Reset();
		}
	}

	sm_NodeSets.Reset();
	sm_NodeSets.Reserve(NOTE_NODES_MAX_NUM_SETS);
	
	char buffer[16];
	for (s32 s = 0; s < NOTE_NODES_MAX_NUM_SETS; s++)
	{		
		NoteNodeSet newNodeSet;
		buffer[0] = '\0';
		formatf(buffer, 16, "Set %d", s+1);
		newNodeSet.name = buffer;
		newNodeSet.creation_number = -1; 
		newNodeSet.node_creation_number = 0;
		newNodeSet.open = false;		
		newNodeSet.nodes.Reset();

		sm_NodeSets.Append() = newNodeSet;
	}

	// The indexes are cleared above, go through array if it has any 
	// entries and delete the peds from the world.
	for(s32 i = 0; i < sm_ScalejaxxPeds.GetCount(); i++)
	{
		CPedFactory::GetFactory()->DestroyPed(sm_ScalejaxxPeds[i]);
	}
	sm_ScalejaxxPeds.Reset();

	sm_CurrentSet = 0;
	sm_CurrentSelected = 0;
	sm_IsEquidistantDrop = false;
	sm_DistanceBetweenDrops = NOTE_NODES_EQUIDISTANT_DROP_DISTANCE;	

	sm_GlobalName[0] = '\0';
	sm_GlobalSetName[0] = '\0';
	sm_TextField[0] = '\0';

	sm_IsCreatingLink = false;
	sm_FirstLinkNode = -1;
	sm_FirstLinkSet = -1;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::Init()
// PURPOSE:	Initialises the node viewer at start of game.
//			Note Init() is called per level.
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::Init(unsigned /*initMode*/)
{
	sm_IsActive = false;
	sm_EditorLevel = EDITOR_LOADING;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::Activate()
// PURPOSE:	Adds all bank widgets and clears values, or changes level state and
//			saves and closes
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::Activate()
{
	bkBank *bank = BANKMGR.FindBank(NOTE_NODES_DEBUG_BANK_NAME);
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();

	if (debugDirector.IsFreeCamActive() == false)
		return;

	if (sm_IsActive)
	{
		if (bank && sm_BankGroupMain)
		{			
			if(sm_IsEditMode == true)
			{
				// Only add the widgets if they aren't there already
				if(sm_BankEditControl == NULL)
				{
					//Initialise values
					sm_CurrentSet			= 0;
					sm_CurrentSelected		= 0;
					sm_FirstLinkNode		= -1;
					sm_FirstLinkSet			= -1;
					sm_FirstLinkType		= -1;
					sm_ColourSelection		= CNodeViewer::COLOUR_DEFAULT;
					sm_NodeLinkType			= TYPE_POINT_NODE;
					sm_OriginalKeyboardMode	= KEYBOARD_MODE_GAME;

					sm_DistanceBetweenDrops	= NOTE_NODES_EQUIDISTANT_DROP_DISTANCE;
					sm_NodeSize				= NOTE_NODES_POINT_NODE_SIZE;
					sm_DrawDistance			= 250.0f;

					sm_IsCreatingLink			= false;
					sm_IsEquidistantDrop		= false;
					sm_IsShowAllNodesAndSets	= false;

					bank->SetCurrentGroup(*sm_BankGroupMain);
					sm_BankEditControl = bank->PushGroup("Edit", true);

					// Set as the main group so the Edit Control group is attached to that rather
					// than the debug bank
					bank->AddToggle("Edit Mode", &CNodeViewer::sm_IsEditMode, &CNodeViewer::Activate);
					bank->AddSlider("Current Set", &CNodeViewer::sm_CurrentSet, 0, NOTE_NODES_MAX_NUM_SETS-1, 1, &CNodeViewer::ResetNodeSet);
					bank->AddText("Current Set Name", sm_GlobalSetName, sizeof(sm_GlobalSetName), false, &CNodeViewer::RenameNodeSet);
					bank->AddToggle("Show all sets", &CNodeViewer::sm_IsShowAllNodesAndSets);

					bank->AddButton("Add New Node", &CNodeViewer::AddNewNode);
					bank->AddButton("Delete Selected Node", &CNodeViewer::DeleteNode);					
					bank->AddCombo("Node Type", &CNodeViewer::sm_NodeLinkType, TYPE_TOTAL, s_NodeType);
					bank->AddButton("Set Node to Type", &CNodeViewer::SetNodeType);
					bank->AddCombo("Node Colour", &CNodeViewer::sm_ColourSelection, COLOUR_CHOICE_TOTAL, s_NodeColour);
					bank->AddButton("Change Node Colour", &CNodeViewer::SetNodeColour);
					bank->AddButton("Move Node", &CNodeViewer::SetNodePosition);
					bank->AddButton("Set Node to Ground", &CNodeViewer::SetNodeToGround);
					bank->AddSlider("Node Size", &CNodeViewer::sm_NodeSize, NOTE_NODES_MIN_NODE_SIZE, NOTE_NODES_MAX_NODE_SIZE, 0.1f, &CNodeViewer::SetNodeSize);
					bank->AddText("Rename Node", sm_GlobalName, sizeof(sm_GlobalName), false, &CNodeViewer::SetNodeName);
					bank->AddButton("Select Closest Node", &CNodeViewer::SetClosestNode);
					bank->AddButton("Warp to Closest Node", &CNodeViewer::SetCameraToClosestNode);
					bank->AddSlider("Current Node", &CNodeViewer::sm_CurrentSelected, -1, NOTE_NODES_MAX_NUM_NODES_PER_SET-1, 1, &CNodeViewer::LimitChangedNode);

					bank->AddToggle("Equidistant Drop", &CNodeViewer::sm_IsEquidistantDrop);
					bank->AddSlider("Distance Between Drops", &CNodeViewer::sm_DistanceBetweenDrops, 20.0f, 200.0f, 1.0f);
					bank->AddSlider("Draw Distance", &CNodeViewer::sm_DrawDistance, 100.0f, 500.0f, 10.0f);
									
					bank->AddButton("Load XML File (File Dialog)", &CNodeViewer::LoadFile);
					bank->AddText("XML Save Filename", sm_XmlFilename, sizeof(sm_XmlFilename));
					bank->AddButton("Save Changes", &CNodeViewer::SaveXmlFile);					

					bank->PopGroup();
					bank->UnSetCurrentGroup(*sm_BankGroupMain);					

					sm_XmlAutosaveFilename = "common:/data/node_routes/autosave.xml";
					sm_XmlSaveLocation = "common:/data/node_routes/";	// Save location

					// Make sure local folder exists for autosave file
					Assert(ASSET.CreateLeadingPath(sm_XmlAutosaveFilename ));
					safecpy(sm_XmlFilename, "<changeme>");					// Default save location					

					SetColours();
					SetAllNodesEmpty();
				}
				sm_EditorLevel = EDITOR_EDIT;
			}
			else
			{
				sm_EditorLevel = EDITOR_DISPLAY;
			}
		}

		sm_OriginalKeyboardMode = CControlMgr::GetKeyboard().GetKeyboardMode();

		// Commented out to stop database loading for now - needs changing
		/*

		// load from database:

		DebugNodeSys::Init();
		for (s32 iCount = 0; iCount < NOTE_NODES_MAX_NUM_SETS; iCount++)
		{
			if (!DebugNodeSys::LoadSet(0, iCount, &sm_NodeSets[iCount], sizeof(DebugNodeSet)))
				Displayf("Node Viewer: Failed to load Set %d\n", iCount);
			else
				Displayf("Node Viewer: Successfully loaded Set %d\n", iCount);
		}*/
	}
	else
	{
		sm_EditorLevel = EDITOR_LOADING;
		SaveFile(true);		// Autosave

		if (bank && sm_BankEditControl && sm_BankGroupMain)
		{
			bank->SetCurrentGroup(*sm_BankGroupMain);
			bank->DeleteGroup(*sm_BankEditControl);
			bank->UnSetCurrentGroup(*sm_BankGroupMain);
			sm_BankEditControl = NULL;
		}
		
		SetAllNodesEmpty();
		ResetNodeSet();
		sm_Colours.Reset();

		//DebugNodeSys::Shutdown();

		CControlMgr::GetKeyboard().SetCurrentMode(sm_OriginalKeyboardMode);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::Shutdown()
// PURPOSE:	Shuts down the whole of the node viewer
//			Note ShutdownLevel() is called per level finish.
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::Shutdown(unsigned /*shutdownMode*/)
{
	if (!sm_IsActive)  // only shut down if it has been active
		return;

	sm_IsActive = false;
	sm_Colours.Reset();
	sm_ScalejaxxPeds.Reset();

	// Reset the node arrays
	for (s32 s = 0; s < sm_NodeSets.GetCount(); s++)
	{
		for (s32 n = 0; n < sm_NodeSets[s].nodes.GetCount(); n++)
		{
			sm_NodeSets[s].nodes.Reset();
		}
	}	
	sm_NodeSets.Reset();

	Activate();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::Update()
// PURPOSE:	main update for the node viewer
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::Update()
{
	CPad& pPad = CControlMgr::GetDebugPad();
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();	
	
	// check to switch on/off if START & SELECT are pressed in Debug cam/pad:
	if (debugDirector.IsFreeCamActive() && pPad.StartJustDown() && pPad.SelectJustDown() &&
		// Added as it was possible to turn off node viewer while in keyboard input
		sm_EditorLevel != EDITOR_TEXT_INPUT) 
	{
		sm_IsActive = !sm_IsActive;
		sm_IsEditMode = true;
		Activate();
		return;
	}

	//
	// check to see if debug cam is still active, if not, shut down Node Viewer:
	//
	if (!debugDirector.IsFreeCamActive())
	{
		Shutdown(0);
		return;
	}

	if(!sm_IsActive)
		return;

	// Disable some of the free cam controls while using node viewer
	camFreeCamera* freeCamera = camInterface::GetDebugDirector().GetFreeCam();
	Assert(freeCamera);

	freeCamera->SetDPadDownEnabled(false);
	freeCamera->SetDPadLeftEnabled(false);
	freeCamera->SetDPadRightEnabled(false);
	freeCamera->SetDPadUpEnabled(false);
	freeCamera->SetLeftShoulder1Enabled(false);
	freeCamera->SetLeftShoulder2Enabled(false);
	freeCamera->SetRightShoulder1Enabled(false);
	freeCamera->SetRightShoulder2Enabled(false);

	//
	// check pad user input:
	//
	switch (sm_EditorLevel)
	{
		case EDITOR_EDIT:  // at the dropping nodes level
		{	
			if (pPad.ButtonCircleJustDown())
			{
				sm_IsActive = false;
				sm_IsEditMode = false;
				Activate();
				break;
			}

			if (pPad.RightShoulder1JustDown())
			{
				AddNewNode();
			}
			
			if (pPad.LeftShoulder1JustDown())
			{
				DeleteNode();
			}

			if (pPad.DPadLeftJustDown())
			{
				SetPreviousNode();
			}

			if (pPad.DPadRightJustDown())
			{
				SetNextNode();
			}

			if (pPad.DPadUpJustDown())
			{
				if (sm_CurrentSet > 0)
					sm_CurrentSet--;

				ResetNodeSet();
			}

			if (pPad.DPadDownJustDown())
			{
				if (sm_CurrentSet < NOTE_NODES_MAX_NUM_SETS-1)
					sm_CurrentSet++;

				ResetNodeSet();
			}

			if (pPad.RightShoulder2JustDown())
			{
				SetNextLinkType();
			}	

			if (pPad.ButtonTriangleJustDown())
			{
				SetNodeType();
			}

			if (pPad.LeftShoulder2JustDown())
			{
				SetNodeToGround();
			}

			if (pPad.ShockButtonRJustDown())
			{
				sm_IsShowAllNodesAndSets = !sm_IsShowAllNodesAndSets;
				SetScalejaxxVisibility();
			}

			if (pPad.StartJustDown())
			{
				// Only enter text editor if a proper node is selected
				if(sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type != TYPE_FREE_NODE)
				{
					EnterExitTextEditor();
				}
			}

			// auto drop a node if sm_IsEquidistantDrop is on:
			if (sm_IsEquidistantDrop)
			{
				if (sm_NodeSets[sm_CurrentSet].node_creation_number > 0)
				{
					const Vector3 currentPos = GetDebugCamPos();
					const Vector3 lastNodePos = sm_NodeSets[sm_CurrentSet].nodes[sm_NodeSets[sm_CurrentSet].node_creation_number-1].coord;  // previous node

					Vector3 vecDiff = currentPos - lastNodePos;
					float distanceFromLastNode = vecDiff.Mag();

					if (distanceFromLastNode > sm_DistanceBetweenDrops)
					{
						AddNewNode();
					}
				}
			}

			break;
		}

		case EDITOR_LOADING:
		case EDITOR_DISPLAY:
		{
			if (pPad.ButtonCircleJustDown())
			{
				sm_IsActive = false;
				sm_IsEditMode = false;
				Activate();
			}
			break;
		}

		case EDITOR_TEXT_INPUT:
		{
			if (pPad.ButtonCircleJustDown())
			{
				//Properly shut down the text editor
				EnterExitTextEditor();
				sm_IsActive = false;
				sm_IsEditMode = false;
				Activate();
				break;
			}
			ProcessTextEditorInput();
			break;
		}

		default:
		{
			Assertf(0, "NodeViewer: Invalid editor mode!");
			break;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::Render()
// PURPOSE: renders the nodes
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::Render()
{
	if (!sm_IsActive)
		return;

	//
	// add any helper messages:
	//
	CTextLayout MenuTextLayout;

#define TEXT_SPACING (0.04f)

	MenuTextLayout.SetOrientation(FONT_LEFT);
	MenuTextLayout.SetColor(CRGBA(225,225,225,255));
	Vector2 textPos = Vector2(0.05f, 0.05f);
	Vector2 textSize = Vector2(0.33f, 0.4785f);
	Vector2 textWrap = Vector2(0.05f, 0.7f);

	CHudTools::AdjustForWidescreen(WIDESCREEN_FORMAT_SIZE_ONLY, NULL, &textSize, NULL);
	MenuTextLayout.SetScale(&textSize);
	MenuTextLayout.SetWrap(&textWrap);

	//
	// title:
	//
	MenuTextLayout.Render(textPos, "~bold~~italic~N O D E  V I E W E R");
	textPos.y += (TEXT_SPACING * 2);

	if (sm_EditorLevel == EDITOR_LOADING)  // quick display whilst it loads
	{
		MenuTextLayout.Render(textPos, "Loading, please wait...");
		CText::Flush();
		return;
	}

	switch (sm_EditorLevel)
	{
		case EDITOR_EDIT:  // at the dropping nodes level
		{
			//
			// controls:
			//
			MenuTextLayout.Render(textPos, "~PAD_RB~  Add new node" );
			textPos.y += TEXT_SPACING;

			MenuTextLayout.Render(textPos, "~PAD_LB~  Delete Node");
			textPos.y += TEXT_SPACING;

			MenuTextLayout.Render(textPos, "~PAD_DPAD_UP~ ~PAD_DPAD_DOWN~  Select Set");
			textPos.y += TEXT_SPACING;

			MenuTextLayout.Render(textPos, "~PAD_DPAD_LEFT~ ~PAD_DPAD_RIGHT~  Select Node");
			textPos.y += TEXT_SPACING;

			MenuTextLayout.Render(textPos, "~PAD_LT~  Set to Ground");
			textPos.y += TEXT_SPACING;		

			MenuTextLayout.Render(textPos, "~PAD_RT~  Change Node Type");
			textPos.y += TEXT_SPACING;		

			char const * debugText = NULL;

			if (sm_IsCreatingLink == false && sm_NodeLinkType == TYPE_LINK_FIRST_NODE)
			{
				debugText = "~PAD_Y~  Link (Start)";
			}
			else if (sm_IsCreatingLink == false && sm_NodeLinkType == TYPE_BOX_FIRST_NODE)
			{
				debugText = "~PAD_Y~  Box (Start)";
			}
			else if (sm_IsCreatingLink == false && sm_NodeLinkType == TYPE_ARROW)
			{
				debugText = "~PAD_Y~  Arrow (Start)";
			}
			else if (sm_IsCreatingLink && sm_NodeLinkType == TYPE_LINK_FIRST_NODE)
			{
				debugText = "~PAD_Y~  Link (Finish)";
			}
			else if (sm_IsCreatingLink && sm_NodeLinkType == TYPE_BOX_FIRST_NODE)
			{
				debugText = "~PAD_Y~  Box (Finish)";
			}
			else if (sm_IsCreatingLink && sm_NodeLinkType == TYPE_ARROW)
			{
				debugText = "~PAD_Y~  Arrow (Finish)";
			}
			else if (sm_NodeLinkType == TYPE_SCALEJAXX)
			{
				debugText = "~PAD_Y~  Scalejaxx";
			}
			else
			{
				debugText = "~PAD_Y~  Point";
			}
			
			MenuTextLayout.Render(textPos, debugText );
			textPos.y += TEXT_SPACING;

			MenuTextLayout.Render(textPos, "~PAD_START~  Rename Node");
			textPos.y += TEXT_SPACING;


			debugText = sm_IsShowAllNodesAndSets ? "~PAD_RSTICK_ALL~  Show active Set only" : "~PAD_RSTICK_ALL~  Show all Sets";
			MenuTextLayout.Render(textPos, debugText);
			textPos.y += TEXT_SPACING;

			MenuTextLayout.Render(textPos, "~PAD_START~ ~PAD_BACK~  Quit");
			textPos.y += TEXT_SPACING * 3;

			//
			// stats:
			//
			char menuString[300] = {0};
			formatf(menuString, 300, "Current Set: %d   Name: %s", sm_CurrentSet+1, sm_NodeSets[sm_CurrentSet].name.c_str());

			MenuTextLayout.Render(textPos, menuString);
			textPos.y += TEXT_SPACING;

			if (sm_NodeSets[sm_CurrentSet].node_creation_number != 0)
			{
				formatf(menuString, 300, "Current node: %d/%d", sm_CurrentSelected+1, sm_NodeSets[sm_CurrentSet].node_creation_number);
				MenuTextLayout.Render(textPos, menuString);
				textPos.y += TEXT_SPACING;
			}

			break;
		}

		case EDITOR_TEXT_INPUT:
		{
			MenuTextLayout.Render(textPos, "RENAMING NODE (Press RET to finish editing)");
			textPos.y += TEXT_SPACING * 3;

			char displayString[280];
			formatf(displayString, 280, "New Node Name: %s", sm_GlobalName);

			MenuTextLayout.Render(textPos, displayString);
			textPos.y += TEXT_SPACING;
			break;
		}

		case EDITOR_DISPLAY:
		{
			break;
		}

		default:
		{
			Assertf(0, "NodeViewer: Invalid editor mode!");
			break;
		}
	}
	
	CText::Flush();

	// render shadow on ground (under camera):
	Vector3 cameraPos = GetDebugCamPos();
	bool hasFoundGround = false;
	float groundPos = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, cameraPos.x, cameraPos.y, cameraPos.z, &hasFoundGround);

	if (!hasFoundGround)
		groundPos = cameraPos.z - 10.0f;
	
	// Shadow
	Vector3 groundVec = Vector3(cameraPos.x, cameraPos.y, groundPos);	
	grcDebugDraw::Sphere(groundVec, 2.0f, CRGBA(0,0,0,185));
	
	Color32 textColour(0, 0, 255, NOTE_NODES_NODE_ALPHA); 

	// render current nodes:
	for (s32 s = 0; s < sm_NodeSets.GetCount(); s++)
	{
		if (!sm_IsShowAllNodesAndSets && s != sm_CurrentSet)
			continue;

		for (s32 n = 0; n < sm_NodeSets[s].node_creation_number; n++)
		{
			if (sm_NodeSets[s].nodes[n].type != TYPE_FREE_NODE)
			{
				// Only draw if close to camera
				const Vector3 distanceFromCam = sm_NodeSets[s].nodes[n].coord - cameraPos; //(static_cast<Vector3>(sm_NodeSets[s].nodes[n].coord)) - cameraPos;
				if (distanceFromCam.Mag() < sm_DrawDistance)
				{
					const s32 nodeLinkIndex = sm_NodeSets[s].nodes[n].link;

					// Scalejaxx need special treatment - don't draw a sphere
					if(sm_NodeSets[s].nodes[n].type == TYPE_SCALEJAXX && n == sm_CurrentSelected && s == sm_CurrentSet)
					{
						grcDebugDraw::Cross(RCC_VEC3V(sm_NodeSets[s].nodes[n].coord), sm_NodeSets[s].nodes[n].size, sm_Colours[COLOUR_SELECTED]);
					}
					else if(sm_NodeSets[s].nodes[n].type == TYPE_SCALEJAXX)
					{
						// Same as above here - previously empty - until the ped creation has been implemented
						grcDebugDraw::Cross(RCC_VEC3V(sm_NodeSets[s].nodes[n].coord), sm_NodeSets[s].nodes[n].size, sm_Colours[sm_NodeSets[s].nodes[n].colour]);
					}
					// If we're in display mode, use it's colour
					else if (sm_IsEditMode == false)
					{
						grcDebugDraw::Sphere(sm_NodeSets[s].nodes[n].coord, sm_NodeSets[s].nodes[n].size, sm_Colours[sm_NodeSets[s].nodes[n].colour]);
					}
					// If in a different set to the active set, use a set colour
					else if (sm_IsShowAllNodesAndSets && s != sm_CurrentSet)
					{
						grcDebugDraw::Sphere(sm_NodeSets[s].nodes[n].coord, sm_NodeSets[s].nodes[n].size, sm_Colours[COLOUR_INACTIVE_SET]);
					}
					else if(n != sm_CurrentSelected)
					{
						grcDebugDraw::Sphere(sm_NodeSets[s].nodes[n].coord, sm_NodeSets[s].nodes[n].size, sm_Colours[sm_NodeSets[s].nodes[n].colour]);
					}
					// If it is selected, force it to be red
					else
					{
						grcDebugDraw::Sphere(sm_NodeSets[s].nodes[n].coord, sm_NodeSets[s].nodes[n].size, sm_Colours[COLOUR_SELECTED]);
					}	

					// Only draw if it's a completed link
					if (nodeLinkIndex != -1) 
					{
						if (sm_NodeSets[s].nodes[n].type == TYPE_BOX_FIRST_NODE)
						{					
							grcDebugDraw::BoxAxisAligned(RCC_VEC3V(sm_NodeSets[s].nodes[n].coord), RCC_VEC3V(sm_NodeSets[s].nodes[nodeLinkIndex].coord), 
								sm_Colours[sm_NodeSets[s].nodes[n].colour]);
						}
						else if (sm_NodeSets[s].nodes[n].type == TYPE_LINK_FIRST_NODE) 
						{						
							grcDebugDraw::Line(RCC_VEC3V(sm_NodeSets[s].nodes[n].coord), RCC_VEC3V(sm_NodeSets[s].nodes[nodeLinkIndex].coord), 
								sm_Colours[sm_NodeSets[s].nodes[n].colour]);
						}
						else if (sm_NodeSets[s].nodes[n].type == TYPE_ARROW)
						{
							grcDebugDraw::Arrow(RCC_VEC3V(sm_NodeSets[s].nodes[n].coord), RCC_VEC3V(sm_NodeSets[s].nodes[nodeLinkIndex].coord), 
								15.0f, sm_Colours[sm_NodeSets[s].nodes[n].colour]);
						}
					}

					Vector3 nodeTextPos = sm_NodeSets[s].nodes[n].coord;
					nodeTextPos.z += (sm_NodeSets[s].nodes[n].size * 1.3f);
					grcDebugDraw::Text(nodeTextPos, textColour, sm_NodeSets[s].nodes[n].label);
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetNodeName()
// PURPOSE: renames the node
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetNodeName()
{
	if (!sm_IsActive)
		return;

	char nodeName[NOTE_NODES_CHAR_ARRAY_LENGTH] = "";
	if (sm_GlobalName[0] == '\0')
		formatf(nodeName, NOTE_NODES_CHAR_ARRAY_LENGTH, "Node %d (%d)", sm_CurrentSelected, sm_CurrentSet + 1);
	else
		formatf(nodeName, NOTE_NODES_CHAR_ARRAY_LENGTH, "%s (%d)", sm_GlobalName, sm_CurrentSet + 1);
	
	sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].label = nodeName;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::RenameNodeSet()
// PURPOSE: renames the node set
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::RenameNodeSet()
{
	if (!sm_IsActive)
		return;
	char setName[NOTE_NODES_CHAR_ARRAY_LENGTH] = "";
	if (sm_GlobalSetName[0] == '\0')
		formatf(setName, NOTE_NODES_CHAR_ARRAY_LENGTH, "Node Set %d", sm_CurrentSet+1);
	else
		formatf(setName, NOTE_NODES_CHAR_ARRAY_LENGTH, "%s", sm_GlobalSetName);
	sm_NodeSets[sm_CurrentSet].name = setName;
	
	SaveFile(true);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::GetDebugCamPos()
// PURPOSE: returns the camera position
/////////////////////////////////////////////////////////////////////////////////////
Vector3 CNodeViewer::GetDebugCamPos()
{
	Vector3 cameraPos(0,0,0);
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	const bool isFreeCamActive = debugDirector.IsFreeCamActive();
	if (isFreeCamActive)
	{
		const camFrame& freeCamFrame = debugDirector.GetFreeCamFrame();
		cameraPos = freeCamFrame.GetPosition();
	}

	return cameraPos;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::AddNewNode()
// PURPOSE: adds 1 new node to the current set
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::AddNewNode()
{
	if (!sm_IsActive)
		return;

	if (sm_NodeSets[sm_CurrentSet].node_creation_number == NOTE_NODES_MAX_NUM_NODES_PER_SET)
		return;

	char buffer[NOTE_NODES_CHAR_ARRAY_LENGTH] = "";

	// if this is the 1st node we create on this set, then set it as in use
	if (sm_NodeSets[sm_CurrentSet].node_creation_number == 0)
	{
		sm_NodeSets[sm_CurrentSet].open = true;
		sm_NodeSets[sm_CurrentSet].creation_number = sm_CurrentSet;  // store the set number

		ResetNodeSet();
	}
	
	const Vector3 cameraPos = GetDebugCamPos();

	NoteNode newNoteNode;
	newNoteNode.creation_number	= sm_CurrentSet;	// store set this node was created in
	newNoteNode.coord			= cameraPos;		// set node position as camera position
	newNoteNode.type			= TYPE_POINT_NODE;
	newNoteNode.size			= NOTE_NODES_POINT_NODE_SIZE;
	newNoteNode.colour			= sm_ColourSelection;
	newNoteNode.link			= -1;
	newNoteNode.scalejaxxIndex	= -1;
	
	buffer[0] = '\0';
	formatf(buffer, NOTE_NODES_CHAR_ARRAY_LENGTH, "Node %d (%d)", sm_NodeSets[sm_CurrentSet].node_creation_number + 1, sm_CurrentSet + 1);
	newNoteNode.label = buffer;

	// Find the block number to get the name
	s32 blockNum = CBlockView::GetCurrentBlockInside(cameraPos);
	const char* blockName = CBlockView::GetBlockName(blockNum);
	newNoteNode.blockName = blockName;

	sm_NodeSets[sm_CurrentSet].nodes.PushAndGrow(newNoteNode);

	sm_CurrentSelected = sm_NodeSets[sm_CurrentSet].node_creation_number;  // set the new node as the currently selected one

	if (sm_NodeSets[sm_CurrentSet].node_creation_number <= NOTE_NODES_MAX_NUM_NODES_PER_SET -1)
		sm_NodeSets[sm_CurrentSet].node_creation_number++;  // move onto next node
	else
	{
		Assertf(0, "NodeViewer: Exceeded max nodes (%d)", NOTE_NODES_MAX_NUM_NODES_PER_SET);
	}

	SaveFile(true);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::DeleteNode()
// PURPOSE: removes node
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::DeleteNode()
{
	if (!sm_IsActive)
		return;

	if (sm_CurrentSelected == -1)
		return;

	if (sm_NodeSets[sm_CurrentSet].node_creation_number <= 0)
		return;	

	if(sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type == TYPE_SCALEJAXX)
	{
		DeleteScalejaxx();
		sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].scalejaxxIndex = -1;
	}

	// delete this node
	sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type = TYPE_FREE_NODE;
	sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].link = -1;
	
	for (s32 n = 0; n < sm_NodeSets[sm_CurrentSet].nodes.GetCount(); n++)
	{
		// Loop through all nodes in set to see if they point to the node just deleted
		// and remove link if so
		if (sm_NodeSets[sm_CurrentSet].nodes[n].link == sm_CurrentSelected)
		{
			sm_NodeSets[sm_CurrentSet].nodes[n].link = -1;
			sm_NodeSets[sm_CurrentSet].nodes[n].type = TYPE_POINT_NODE;
		}
		// If a node points to a node in a higher index to the one just removed,
		// decrement the link by 1 (the node positions are moved down later to 
		// fill in the gap by the newly deleted node
		else if (sm_NodeSets[sm_CurrentSet].nodes[n].link > sm_CurrentSelected)
		{
			sm_NodeSets[sm_CurrentSet].nodes[n].link -= 1;
		}
	}

	sm_NodeSets[sm_CurrentSet].nodes.Delete(sm_CurrentSelected);

	// adjust the counters:
	sm_NodeSets[sm_CurrentSet].node_creation_number--;
	sm_CurrentSelected -= 1;
	LimitChangedNode();

	// if this is the 1st node we delete on this set, then set it as no longer in use as there are no active nodes anymore here
	if (sm_NodeSets[sm_CurrentSet].node_creation_number == 0)
		sm_NodeSets[sm_CurrentSet].open = false;
	
	SaveFile(true);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::LimitChangedNode()
// PURPOSE: deals with wrapping and limiting the adjusted selected node within the
//			active nodes we have
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::LimitChangedNode()
{
	if (!sm_IsActive)
		return;

	if (sm_CurrentSelected < 0)
	{
		if (sm_NodeSets[sm_CurrentSet].node_creation_number != 0)
			sm_CurrentSelected = sm_NodeSets[sm_CurrentSet].node_creation_number-1;  // wrap round to end
		else
			sm_CurrentSelected = 0;
	}

	if (sm_CurrentSelected >= sm_NodeSets[sm_CurrentSet].node_creation_number || sm_CurrentSelected >= NOTE_NODES_MAX_NUM_NODES_PER_SET)
		sm_CurrentSelected = 0;  // wrap round to beginning
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetNodeSize()
// PURPOSE: Change the size of a node (only used for point nodes)
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetNodeSize()
{
	if (!sm_IsActive)
		return;
	
	sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].size = sm_NodeSize;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetNextLinkType()
// PURPOSE: Change the type of node/link to be used later
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetNextLinkType()
{
	if(!sm_IsActive)
		return;

	// Don't allow the type to be changed if in the middle of creating a link
	if(sm_IsCreatingLink)
		return;

	sm_NodeLinkType++;

	// If past the last type, loop to start
	if(sm_NodeLinkType == TYPE_TOTAL)
	{
		sm_NodeLinkType = 0;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetNextNode()
// PURPOSE: moves on one node
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetNextNode()
{
	if (!sm_IsActive)
		return;
	
	sm_CurrentSelected++;
	LimitChangedNode();

	if (sm_NodeSets[sm_CurrentSet].nodes.GetCount() != 0)
		sm_NodeSize = sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].size;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetPreviousNode()
// PURPOSE: goes back one node
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetPreviousNode()
{
	if (!sm_IsActive)
		return;

	sm_CurrentSelected--;
	LimitChangedNode();

	if (sm_NodeSets[sm_CurrentSet].nodes.GetCount() != 0)
		sm_NodeSize = sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].size;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::ResetNodeSet()
// PURPOSE: sets the inital values when we go to a new set
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::ResetNodeSet()
{
	if (!sm_IsActive)
		return;

	sm_CurrentSelected = 0;

	safecpy(sm_GlobalSetName, sm_NodeSets[sm_CurrentSet].name);

	// if this set is not currently open, then display it as empty:
	if (!sm_NodeSets[sm_CurrentSet].open)
	{
		strcat(sm_GlobalSetName, " - EMPTY");
	}	

	SetScalejaxxVisibility();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetScalejaxxVisibility()
// PURPOSE: sets a Scalejaxx's visibility depending on sm_IsShowAllNodesAndSets and
//			the current set in use.
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetScalejaxxVisibility()
{
	if (!sm_IsActive)
		return;

	if(sm_ScalejaxxPeds.GetCount() > 0)
	{
		// Disable drawing on Scalejaxx's not in this set - to save checking every frame
		for(s32 s = 0; s < sm_NodeSets.GetCount(); s++)
		{
			for(s32 n = 0; n < sm_NodeSets[s].nodes.GetCount(); n++)
			{
				if ( sm_NodeSets[s].nodes[n].type == TYPE_SCALEJAXX)
				{
					const s32 index = sm_NodeSets[s].nodes[n].scalejaxxIndex;

					if(index != -1)
					{
						if(sm_IsShowAllNodesAndSets == false && sm_CurrentSet != s)
						{				
							sm_ScalejaxxPeds[index]->SetIsVisibleForModule(SETISVISIBLE_MODULE_DEBUG, false);
						}
						else
						{
							sm_ScalejaxxPeds[index]->SetIsVisibleForModule(SETISVISIBLE_MODULE_DEBUG, true);
						}
					}
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::EnterExitTextEditor()
// PURPOSE: enter and exit the text editor
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::EnterExitTextEditor()
{
	if (!sm_IsActive)
		return;

	if (sm_EditorLevel == EDITOR_TEXT_INPUT)
	{
		safecpy(sm_GlobalName, sm_TextField);

		sm_TextField[0] = '\0';
		sm_TextFieldCursor = 0;

		SetNodeName();

		CControlMgr::GetKeyboard().SetCurrentMode(sm_OriginalKeyboardMode);

		sm_EditorLevel = EDITOR_EDIT;
	}
	else if (sm_EditorLevel == EDITOR_EDIT)
	{
		sm_OriginalKeyboardMode = CControlMgr::GetKeyboard().GetKeyboardMode();   // store original mode

		CControlMgr::GetKeyboard().SetCurrentMode(KEYBOARD_MODE_GAME);

		sm_EditorLevel = EDITOR_TEXT_INPUT;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::ProcessTextEditorInput()
// PURPOSE: processes the text editior input
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::ProcessTextEditorInput()
{
	// switch off on RETURN
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RETURN, KEYBOARD_MODE_GAME))
	{
		EnterExitTextEditor();
		return;
	}

	// backspace
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_BACK, KEYBOARD_MODE_GAME))
	{
		if (sm_TextFieldCursor > 0)
			sm_TextFieldCursor--;

		sm_TextField[sm_TextFieldCursor] = '\0';
	}
	else
	// actual key
	{
		const char* pKeysPressed = CControlMgr::GetKeyboard().GetKeyPressedBuffer(KEYBOARD_MODE_GAME);

		if (pKeysPressed != NULL)
		{
			sm_TextField[sm_TextFieldCursor] = *pKeysPressed;

			if (sm_TextFieldCursor < 253)
				sm_TextFieldCursor++;

			sm_TextField[sm_TextFieldCursor] = '\0';
		}
	}

	formatf(sm_GlobalName, "%s_", sm_TextField);

	SetNodeName();  // do the rename
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetNodeToGround()
// PURPOSE: sets the z pos of the node to be the ground position
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetNodeToGround()
{
	if (!sm_IsActive)
		return;
	
	if (sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type == TYPE_FREE_NODE)
		return;

	Vector3 nodePos = sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].coord;

	bool hasFoundGround = false;
	float groundPos = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, nodePos.x, nodePos.y, nodePos.z, &hasFoundGround);

	if (hasFoundGround)
	{
		// Move off the ground by half the sphere height, to stop half being below the floor
		groundPos += (sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].size / 2);
		sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].coord.z = groundPos;  // set z pos to ground level

		/*if(sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type == TYPE_SCALEJAXX)
		{
			SetScalejaxxPosition(sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].coord);
		}*/
	}
	SaveFile(true);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetNodePosition()
// PURPOSE: moves the current selected node to current cam position
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetNodePosition()
{
	if (!sm_IsActive)
		return;

	if (sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type != TYPE_FREE_NODE)
	{
		sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].coord = GetDebugCamPos();
		if(sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type == TYPE_SCALEJAXX)
		{
			SetScalejaxxPosition(GetDebugCamPos());
		}
	}
	SaveFile(true);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetNodeType()
// PURPOSE: This is a two-stage process which depends on the sm_IsCreatingBox flag.  
//			If this is the first node to be used to create the link, if it's already 
//			a start node for a link or box it will clear that first.  If it's the 
//			second node for a link or box then it completes the link.
// NOTES:	Links are stored one way - the first node is given the index of the 
//			second node, but the second node doesn't store the index of the first node
//			to allow chaining in any order, and for nodes to be linked to multiple
//			times if desired (but only one link can originate from a node).
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetNodeType()
{
	if (!sm_IsActive)
		return;

	// Don't create a link if no nodes yet!
	if (sm_NodeSets[sm_CurrentSet].node_creation_number == 0)
		return;

	// This is to prevent RAG use trying to change when in middle of creating a link
	// which is prevented in the Update() function for controller presses too.
	if (sm_IsCreatingLink && sm_NodeLinkType != sm_FirstLinkType)
	{
		Displayf("Link type changed while a link was being created - cancel the link or re-select the same type again");
		return;
	}

	// If a scalejaxx requested, only add if it's not a scalejaxx already
	if (sm_NodeLinkType == TYPE_SCALEJAXX && sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type != TYPE_SCALEJAXX)
	{
		sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type = sm_NodeLinkType;
		CreateScalejaxx();
		return;
	}
	// Node is already a scalejaxx - do nothing
	else if(sm_NodeLinkType == TYPE_SCALEJAXX)
	{
		return;
	}
	
	if (sm_NodeLinkType == TYPE_POINT_NODE)
	{
		// If node is currently a scalejaxx, it needs deleting from the array and the 
		// manager
		if(sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type == TYPE_SCALEJAXX)
		{
			DeleteScalejaxx();
			sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].scalejaxxIndex = -1;
		}
		// Set to point node, leaving whatever size value it may have had previously
		sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type = sm_NodeLinkType;
		sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].link = -1;

		return;
	}

	if (sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type != TYPE_FREE_NODE)
	{
		// This is the first node in a pair
		if (sm_IsCreatingLink == false)
		{
			// If node is currently a scalejaxx, it needs deleting from the array and the 
			// manager
			if(sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type == TYPE_SCALEJAXX)
			{
				DeleteScalejaxx();
				sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].scalejaxxIndex = -1;
			}
			// Get rid of current link
			sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].link = -1;
			sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type = sm_NodeLinkType;
			sm_FirstLinkNode = sm_CurrentSelected;
			sm_FirstLinkType = sm_NodeLinkType;
			sm_FirstLinkSet = sm_CurrentSet;
			sm_IsCreatingLink = true;
		}
		else
		{
			// If we're trying to complete a link with 2 different types, or
			// if the same node as the start box node, then just cancel creating
			if (sm_CurrentSelected == sm_FirstLinkNode || sm_FirstLinkType != sm_NodeLinkType)
			{
				sm_FirstLinkNode = -1;
				sm_FirstLinkSet = -1;
				sm_FirstLinkType = TYPE_FREE_NODE;
				sm_IsCreatingLink = false;
			}
			else
			{
				// Used to avoid nodes being linked each way
				const s32 nodeExistingLink = sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].link;

				// Make sure the first link node is in same set and so on
				if (sm_CurrentSet == sm_FirstLinkSet && 
					sm_CurrentSet != -1 &&
					sm_FirstLinkNode != -1 &&
					nodeExistingLink != sm_FirstLinkNode)
				{
					sm_NodeSets[sm_FirstLinkSet].nodes[sm_FirstLinkNode].link = sm_CurrentSelected;
					sm_FirstLinkNode = -1;
					sm_FirstLinkSet = -1;
					sm_IsCreatingLink = false;
				}
			}
		}
	}
	SaveFile(true);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetClosestNode()
// PURPOSE: selects the closest node to the camera
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetClosestNode()
{
	if (!sm_IsActive)
		return;

	if (sm_CurrentSelected == -1)
		return;

	const Vector3 camPos = GetDebugCamPos();
	Vector3 vecBetween;
	float distBetween;

	// Find closest Node, just check current set if it's the only one
	// visible currently.
	s32 closestNode = -1;
	float closestDist = 100000.0f;
	if (sm_IsShowAllNodesAndSets == false)
	{
		for(s32 n = 0; n < sm_NodeSets[sm_CurrentSet].nodes.GetCount(); n++)
		{
			if(sm_NodeSets[sm_CurrentSet].nodes[n].type != TYPE_FREE_NODE)
			{
				vecBetween = sm_NodeSets[sm_CurrentSet].nodes[n].coord - camPos; 
				distBetween = vecBetween.Mag();
				if (distBetween < closestDist)
				{
					closestDist = distBetween;
					closestNode = n;
				}
			}
		}
	}
	// Otherwise check all sets and nodes to find the closest
	else
	{
		s32 closestNodeSet = -1;
		for (s32 s = 0; s < sm_NodeSets.GetCount(); s++)
		{
			for (s32 n = 0; n < sm_NodeSets[s].nodes.GetCount(); n++)
			{
				if(sm_NodeSets[s].nodes[n].type != TYPE_FREE_NODE)
				{
					vecBetween = sm_NodeSets[s].nodes[n].coord - camPos;
					distBetween = vecBetween.Mag();
					if (distBetween < closestDist)
					{
						closestDist = distBetween;
						closestNode = n;
						closestNodeSet = s;
					}
				}
			}
		}
		if (closestNodeSet != -1)
		{
			sm_CurrentSet = closestNodeSet;
			ResetNodeSet();
		}
	}
	// Update if the node has changed
	if (closestNode != -1)
	{
		sm_CurrentSelected = closestNode;
	}	
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetColours()
// PURPOSE: populates the sm_Colours array
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetColours()
{
	sm_Colours.Reserve(COLOUR_TOTAL);
	// Same order as the eNodeColour enum
	sm_Colours.Append()= Color32(0, 0, 255, NOTE_NODES_NODE_ALPHA); // Default (Blue)	
	sm_Colours.Append()= Color32(0, 255, 0, NOTE_NODES_NODE_ALPHA); // Green
	sm_Colours.Append()= Color32(128, 0, 128, NOTE_NODES_NODE_ALPHA); // Purple
	sm_Colours.Append()= Color32(255, 255, 0, NOTE_NODES_NODE_ALPHA); // Yellow
	sm_Colours.Append()= Color32(255, 128, 0, NOTE_NODES_NODE_ALPHA); // Orange
	sm_Colours.Append()= Color32(140, 70, 20, NOTE_NODES_NODE_ALPHA); // Brown
	sm_Colours.Append()= Color32(0, 0, 0, NOTE_NODES_NODE_ALPHA); // Black
	sm_Colours.Append()= Color32(255, 0, 0, NOTE_NODES_NODE_ALPHA); // Selected (Red)
	sm_Colours.Append()= Color32(255, 255, 255, NOTE_NODES_NODE_ALPHA); // Inactive Set (White)
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetNodeColour()
// PURPOSE: changes the currently selected node's colour (visible once it's not 
//			selected)
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetNodeColour()
{
	sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].colour = sm_ColourSelection;
	SaveFile(true);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetCameraToClosestNode()
// PURPOSE: moves the camera to the 
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetCameraToClosestNode()
{
	if(!sm_IsActive)
		return;

	// Find the closest node to the current camera position
	SetClosestNode();

	if(sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type != TYPE_FREE_NODE)
	{
		// Warp the camera to it.
		camDebugDirector& debugDirector = camInterface::GetDebugDirector();
		const bool freeCamActive = debugDirector.IsFreeCamActive();
		if(freeCamActive)
		{
			camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();
			freeCamFrame.SetPosition(sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].coord);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::CreateScalejaxx()
// PURPOSE: Creates and adds a new scalejaxx to the array of peds, and gives the
//			node an index of the item in the array to be used later if deleted.
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::CreateScalejaxx()
{
	Matrix34 tempMat;
	tempMat.Identity();
	tempMat.d = sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].coord;

	// Always use the business model, if that's not found, use something from a set index
	// (need to make sure that's it's not too low otherwise it will be a cutscene character
	// or something else which might crash
	fwModelId pedModelId = CModelInfo::GetModelIdFromName("a_m_y_business_01");
	if(!pedModelId.IsValid())
	{
		Displayf("Couldn't load a_m_y_business_01.");
		Assert(false);
		return;
	}
	const CControlledByInfo localAiControl(false, false);

	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(pedModelId))
	{
		CModelInfo::RequestAssets(pedModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}
	if (!CModelInfo::HaveAssetsLoaded(pedModelId))
	{
		Displayf("Ped has not successfully streamed. Cannot create at this point.");
		return;
	}

	RegdPed& pPedVarDebugPed = sm_ScalejaxxPeds.Grow();

	pPedVarDebugPed = CPedFactory::GetFactory()->CreatePed(localAiControl, pedModelId, &tempMat, true, true, false);
	Assertf(pPedVarDebugPed, "Node Viewer: Couldn't create a debug ped scalejaxx!");

	pPedVarDebugPed->SetIsStanding(true);
	pPedVarDebugPed->SetRagdollState(RAGDOLL_STATE_ANIM_LOCKED);
	pPedVarDebugPed->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_PRIMARY, rage_new CTaskDoNothing(-1), PED_TASK_PRIORITY_DEFAULT);
	pPedVarDebugPed->SetBlockingOfNonTemporaryEvents( true);
	pPedVarDebugPed->SetFixedPhysics(true);
	pPedVarDebugPed->m_nPhysicalFlags.bNotDamagedByAnything = true;

	// Add and disable physics on the ped
	CGameWorld::Add(pPedVarDebugPed, CGameWorld::OUTSIDE, true);

	const s32 index = sm_ScalejaxxPeds.GetCount() - 1;
	if(index != -1)
	{
		sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].scalejaxxIndex = index;
	}
	else
	{
		sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].type = TYPE_POINT_NODE;
		sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].scalejaxxIndex = -1;
	}

	SaveFile(true);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::DeleteScalejaxx()
// PURPOSE: Deletes a scalejaxx from the game world as well as the array holding the 
//			scalejaxx's, then adjusts any other scalejaxx nodes in this set if needed.
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::DeleteScalejaxx()
{
	const s32 indexToRemove = sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].scalejaxxIndex;
	if(indexToRemove == -1)
		return;

	// Remove the ped from the game world and the array
	CPedFactory::GetFactory()->DestroyPed(sm_ScalejaxxPeds[indexToRemove]); 
	sm_ScalejaxxPeds.Delete(indexToRemove);

	// Need to go through the rest of the nodes in this set to see if any of them are
	// scalejaxx's, and if they link to one above the one just deleted, shift down their
	// index one since one has just been removed from the array.
	for(s32 s = 0; s < sm_NodeSets.GetCount(); s++)
	{
		for(s32 n = 0; n < sm_NodeSets[s].nodes.GetCount(); n++)
		{
			if(sm_NodeSets[s].nodes[n].scalejaxxIndex != -1)
			{
				if(sm_NodeSets[s].nodes[n].scalejaxxIndex > indexToRemove)
				{
					sm_NodeSets[s].nodes[n].scalejaxxIndex--;
				}
			}
		}
	}

	sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].scalejaxxIndex = -1;

	SaveFile(true);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SetScalejaxxPosition()
// PURPOSE: Changes the position of the scalejaxx
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SetScalejaxxPosition(const Vector3 &newPos)
{
	if (!sm_IsActive)
		return;

	s32 scalejaxxToMove = sm_NodeSets[sm_CurrentSet].nodes[sm_CurrentSelected].scalejaxxIndex;
	if (scalejaxxToMove < sm_ScalejaxxPeds.GetCount())
	{
		sm_ScalejaxxPeds[scalejaxxToMove]->Teleport(newPos, 0.0f);
	}

	SaveFile(true);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::LoadFile()
// PURPOSE: loads the nodes from an xml file
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::LoadFile()
{
	char fileDest[NOTE_NODES_CHAR_ARRAY_LENGTH];
	safecpy(fileDest, sm_XmlSaveLocation.c_str());

	if(!BANKMGR.OpenFile(fileDest, NOTE_NODES_CHAR_ARRAY_LENGTH, "*.xml", false, "NodeViewer file (*.xml)"))
	{
		Errorf("NodeViewer failed to open file!");
		return;
	}

	SetAllNodesEmpty();  // ensure everything is cleared

	// Attempt to parse the note nodes XML data file and put our data into an
	// XML DOM (Document Object Model) tree.
	parTree* pTree = PARSER.LoadTree(fileDest, "xml");
	if(!pTree)
	{
		Errorf("Failed to load note nodes file '%s.xml'", fileDest);
	}

	// Try to get the root DOM node.
	const parTreeNode* pRootNode = pTree->GetRoot();
	Assert(pRootNode);
	Assert(stricmp(pRootNode->GetElement().GetName(), "NoteNodes") == 0);

	// Make sure we have the correct version.
	int version = pRootNode->GetElement().FindAttributeIntValue("version", 0);
	Assert(version == NOTE_NODES_FILE_VERSION);
	if(version != NOTE_NODES_FILE_VERSION)
	{
		// Free all the memory created for the DOM tree (and its children).
		delete pTree;
		return;
	}

	// Go over any and all the DOM child trees and DOM nodes off of the root
	// DOM node.
	parTreeNode::ChildNodeIterator i = pRootNode->BeginChildren();

	char tempBuff[NOTE_NODES_CHAR_ARRAY_LENGTH] = "";
	s32 setNumber = 0;			// Counter for set index
	for(; i != (*i)->EndChildren(); ++i)
	{
		Assert(stricmp((*i)->GetElement().GetName(), "Set") == 0);
		s32 nodeNumber = 0;

		// Add the note node to our local data.
		// These are of the form:
		// <Node label="Node 1 (1)" creation_number="0" link="-1" type="4" x="2042.589722" y="3796.646240" z="54.940281" size="1.650000" colour="0" /> 
		const char* tempTitle = (*i)->GetElement().FindAttributeStringValue("name", NULL, tempBuff, NOTE_NODES_CHAR_ARRAY_LENGTH);
		sm_NodeSets[setNumber].name = tempTitle;
		
		sm_NodeSets[setNumber].creation_number		= (*i)->GetElement().FindAttributeIntValue("creation_number", -1);
		sm_NodeSets[setNumber].node_creation_number	= (*i)->GetElement().FindAttributeIntValue("node_creation_number", 0);
		sm_NodeSets[setNumber].open					= (*i)->GetElement().FindAttributeBoolValue("open", false);

		sm_NodeSets[setNumber].nodes.Reset();
		sm_NodeSets[setNumber].nodes.Reserve(sm_NodeSets[setNumber].node_creation_number);

		parTreeNode::ChildNodeIterator j = (*i)->BeginChildren();
		for (; j != (*i)->EndChildren(); ++j)
		{
			Assert(stricmp((*j)->GetElement().GetName(), "Node") == 0);
			
			NoteNode newNoteNode;
			tempBuff[0] = '\0';
			const char* tempLabel = (*j)->GetElement().FindAttributeStringValue("label", NULL, tempBuff, NOTE_NODES_CHAR_ARRAY_LENGTH);
			
			newNoteNode.label = tempLabel;

			newNoteNode.creation_number	= (*j)->GetElement().FindAttributeIntValue("creation_number", -1);
			newNoteNode.link			= (*j)->GetElement().FindAttributeIntValue("link", -1);
			newNoteNode.type			= (*j)->GetElement().FindAttributeIntValue("type", TYPE_FREE_NODE);
			newNoteNode.coord.x			= (*j)->GetElement().FindAttributeFloatValue("x", 0.0f);
			newNoteNode.coord.y			= (*j)->GetElement().FindAttributeFloatValue("y", 0.0f);
			newNoteNode.coord.z			= (*j)->GetElement().FindAttributeFloatValue("z", 0.0f);
			newNoteNode.size			= (*j)->GetElement().FindAttributeFloatValue("size", NOTE_NODES_POINT_NODE_SIZE);
			newNoteNode.colour			= (*j)->GetElement().FindAttributeIntValue("colour", COLOUR_DEFAULT);
			
			tempBuff[0] = '\0';		
			const char* tempBlockName = (*j)->GetElement().FindAttributeStringValue("blockName", NULL, tempBuff, NOTE_NODES_CHAR_ARRAY_LENGTH);

			Displayf("blockname is: %s", tempBlockName);
			if ( tempBlockName != NULL)
			{
				Displayf("Not null");
				newNoteNode.blockName = tempBlockName;
			}
			// If the node doesn't have a block name already then find out what block it is in
			// for backwards compatibility	
			else if ( !tempBlockName || tempBlockName == NULL || strcmp(tempBlockName, "") == 0 )
			{
				Displayf("Getting block");
				s32 blockNum = CBlockView::GetCurrentBlockInside(newNoteNode.coord);
				tempBlockName = CBlockView::GetBlockName(blockNum);
			}	
			Displayf("blockname is now: %s", tempBlockName);
			newNoteNode.blockName = tempBlockName;

			sm_NodeSets[setNumber].nodes.Append() = newNoteNode;

			if(sm_NodeSets[setNumber].nodes[nodeNumber].type == TYPE_SCALEJAXX)
			{
				sm_CurrentSet = setNumber;
				sm_CurrentSelected = nodeNumber;
				CreateScalejaxx();
			}
			else
			{
				newNoteNode.scalejaxxIndex = -1;
			}

			nodeNumber++;
		}
		setNumber++;
	}

	ResetNodeSet();
	SetCameraToClosestNode();

	// Free all the memory created for the DOM tree (and its children).
	delete pTree;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SaveXmlFile()
// PURPOSE: Callback function for user-requested save from RAG
//
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SaveXmlFile()
{
	SaveFile(false);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::SaveFile(bool isAutoSave)
// PURPOSE: saves the nodes to the database
// NOTES:	The scalejaxx index is not saved since the array is reset when loading
//			a file, and repopulated then.  To make it simpler, it will just get a new
//			index when added to the array again.
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::SaveFile(bool isAutoSave)
{
	parTree *pTree = rage_new parTree();
	Assert(pTree);

	parTreeNode* pRootNode = pTree->CreateRoot();
	Assert(pRootNode);
	parElement& rootElm = pRootNode->GetElement();
	rootElm.SetName("NoteNodes");
	rootElm.AddAttribute("version", NOTE_NODES_FILE_VERSION, false);
	pTree->SetRoot(pRootNode);

	for (s32 s = 0; s < sm_NodeSets.GetCount(); s++)
	{
		// Add the note node to the DOM tree.
		parTreeNode* pNoteNodeXMLNodeSet = parTreeNode::CreateStdLeaf("Set", "", false);
		Assert(pNoteNodeXMLNodeSet);
		pNoteNodeXMLNodeSet->GetElement().AddAttribute("name", sm_NodeSets[s].name, false);
		pNoteNodeXMLNodeSet->GetElement().AddAttribute("creation_number", sm_NodeSets[s].creation_number, false);
		pNoteNodeXMLNodeSet->GetElement().AddAttribute("node_creation_number", sm_NodeSets[s].node_creation_number, false);
		pNoteNodeXMLNodeSet->GetElement().AddAttribute("open", sm_NodeSets[s].open, false);

		pNoteNodeXMLNodeSet->AppendAsChildOf(pRootNode);

		for (s32 n = 0; n < sm_NodeSets[s].nodes.GetCount(); n++)
		{
			if(sm_NodeSets[s].nodes[n].type != TYPE_FREE_NODE)
			{
				// Add the note node to the DOM tree.
				parTreeNode* pNoteNodeXMLNode = parTreeNode::CreateStdLeaf("Node", "", false);
				Assert(pNoteNodeXMLNode);

				pNoteNodeXMLNode->GetElement().AddAttribute("label", sm_NodeSets[s].nodes[n].label, false);
				pNoteNodeXMLNode->GetElement().AddAttribute("creation_number", sm_NodeSets[s].nodes[n].creation_number, false);
				pNoteNodeXMLNode->GetElement().AddAttribute("link", sm_NodeSets[s].nodes[n].link, false);
				pNoteNodeXMLNode->GetElement().AddAttribute("type", sm_NodeSets[s].nodes[n].type, false);
				pNoteNodeXMLNode->GetElement().AddAttribute("x", sm_NodeSets[s].nodes[n].coord.x, false);
				pNoteNodeXMLNode->GetElement().AddAttribute("y", sm_NodeSets[s].nodes[n].coord.y, false);
				pNoteNodeXMLNode->GetElement().AddAttribute("z", sm_NodeSets[s].nodes[n].coord.z, false);
				pNoteNodeXMLNode->GetElement().AddAttribute("size", sm_NodeSets[s].nodes[n].size, false);
				pNoteNodeXMLNode->GetElement().AddAttribute("colour", sm_NodeSets[s].nodes[n].colour, false);
				pNoteNodeXMLNode->GetElement().AddAttribute("blockName", sm_NodeSets[s].nodes[n].blockName, false);
				
				pNoteNodeXMLNode->AppendAsChildOf(pNoteNodeXMLNodeSet);
			}	
		}
	}
	
	bool isSaved;
	char fileName[NOTE_NODES_CHAR_ARRAY_LENGTH] = "\0";

	// Just the filename is given by the user, the location is 
	// always to the NoteNodes folder
	if(isAutoSave == false)
	{
		safecpy(fileName, sm_XmlSaveLocation.c_str());
		strcat(fileName, sm_XmlFilename);	
	}
	else
	{
		safecpy(fileName, sm_XmlAutosaveFilename.c_str());
	}

	// Save out the XML DOM tree into a file.	
	isSaved = PARSER.SaveTree(fileName, "xml", pTree, parManager::XML);
	if(!isSaved)
	{
		Errorf("Failed to save note nodes file '%s'", fileName);
	}
	else
	{
		Displayf("NoteNodes file saved to '%s'", fileName);
	}

	// Free all the memory created for the DOM tree (and its children).
	delete pTree;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNodeViewer::CreateBankWidgets()
// PURPOSE: creates the bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CNodeViewer::InitWidgets()
{
	bkBank *bank = BANKMGR.FindBank(NOTE_NODES_DEBUG_BANK_NAME);

	if(bank)
	{
		sm_BankGroupMain = bank->PushGroup(NOTE_NODES_VIEWER_GROUP_NAME, true);
		bank->AddToggle("Node Viewer Active", &CNodeViewer::sm_IsActive, &CNodeViewer::Activate);
		bank->PopGroup();
	}
}

#endif // __BANK

// eof
