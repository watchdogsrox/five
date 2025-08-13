/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : NodeViewer.h
// PURPOSE : Allows nodes to be added and edited around the map and saved out
// AUTHOR  : Derek Payne
// STARTED : 27/05/2010
//
// UPDATED : 11/09/2010
// AUTHOR  : Adam Munson
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef INC_NODE_VIEWER_H_
#define INC_NODE_VIEWER_H_

#if __BANK

#include "script/script.h"  // bank
#include "vector/vector3.h"

#define NOTE_NODES_CHAR_ARRAY_LENGTH (256)
#define NOTE_NODES_MAX_NUM_NODES_PER_SET (90)
#define NOTE_NODES_MAX_NUM_SETS (5)

struct NoteNode
{	
	atString blockName;
	atString label;
	Vector3 coord;
	float size;	
	s32 type;	
	s32 creation_number;
	s32 link;	
	s32 colour;
	s32 scalejaxxIndex;
};

struct NoteNodeSet
{
	atArray<NoteNode> nodes;
	atString name;
	s32 creation_number;
	s32 node_creation_number;
	bool open;
};

enum eNodeType
{
	TYPE_POINT_NODE = 0,
	TYPE_LINK_FIRST_NODE,
	TYPE_BOX_FIRST_NODE,	
	TYPE_ARROW,
	TYPE_SCALEJAXX,
	TYPE_TOTAL,					// Free node cannot be selected by user
	TYPE_FREE_NODE
};

class CNodeViewer
{
	enum eEditorLevel
	{
		EDITOR_DISPLAY = 0,
		EDITOR_EDIT,
		EDITOR_LOADING,
		EDITOR_SAVING,
		EDITOR_TEXT_INPUT
	};

	enum eNodeColour
	{		
		COLOUR_DEFAULT = 0,
		COLOUR_GREEN,
		COLOUR_PURPLE,
		COLOUR_YELLOW,
		COLOUR_ORANGE,
		COLOUR_BROWN,
		COLOUR_BLACK,								// Add any new colours in here above SELECTED
		COLOUR_SELECTED,
		COLOUR_INACTIVE_SET,
		COLOUR_TOTAL,								// Total used for the sm_Colours array size reserve			
		COLOUR_CHOICE_TOTAL = COLOUR_TOTAL - 2		// Used for the combo box total that can be selected in RAG
	};

public:

	static void Init(unsigned initMode);
	static void Shutdown(unsigned initMode);

	static void Activate();

	static void Update();
	static void Render();

	static void InitWidgets();

private:

	static void AddNewNode();
	static void CreateScalejaxx();
	static void DeleteNode();
	static void DeleteScalejaxx();
	static void EnterExitTextEditor();
	static Vector3 GetDebugCamPos();
	static void LimitChangedNode();
	static void LoadFile();
	static void ProcessTextEditorInput();
	static void RenameNodeSet();
	static void ResetNodeSet();
	static void SaveFile(bool isAutoSave);
	static void SaveXmlFile();
	static void SetAllNodesEmpty();
	static void SetCameraToClosestNode();
	static void SetClosestNode();
	static void SetColours();
	static void SetNextLinkType();
	static void SetNextNode();
	static void SetNodeColour();
	static void SetNodeName();		
	static void SetNodePosition();	
	static void SetNodeSize();
	static void SetNodeToGround();
	static void SetNodeType();	
	static void SetPreviousNode();	
	static void SetScalejaxxPosition(const Vector3 &newPos);
	static void SetScalejaxxVisibility();

	static atArray<NoteNodeSet> sm_NodeSets;
	static atArray<Color32> sm_Colours;
	static atArray<RegdPed> sm_ScalejaxxPeds;

	static atString sm_XmlSaveLocation;
	static atString sm_XmlAutosaveFilename;

	static char sm_GlobalName[NOTE_NODES_CHAR_ARRAY_LENGTH];
	static char sm_GlobalSetName[NOTE_NODES_CHAR_ARRAY_LENGTH];
	static char sm_TextField[NOTE_NODES_CHAR_ARRAY_LENGTH];
	static char sm_XmlFilename[128];	

	static bkGroup *sm_BankGroupMain;		// Pointer to main node viewer group
	static bkGroup *sm_BankEditControl;		// Pointer to Control group

	static float sm_DistanceBetweenDrops;
	static float sm_NodeSize;
	static float sm_DrawDistance;
	
	static eEditorLevel sm_EditorLevel;	

	static s32 sm_CurrentSet;
	static s32 sm_CurrentSelected;
	static s32 sm_TextFieldCursor;	
	static s32 sm_FirstLinkNode;		// Stores index of first link
	static s32 sm_FirstLinkType;		// Type of the first link
	static s32 sm_FirstLinkSet;		// Stores set of first link - don't allow links between sets
	static s32 sm_ColourSelection;
	static s32 sm_NodeLinkType;
	static s32 sm_OriginalKeyboardMode;

	static bool sm_IsEquidistantDrop;	
	static bool sm_IsShowAllNodesAndSets;
	static bool sm_IsCreatingLink;		// Used to change the menu text
	static bool sm_IsEditMode;
	static bool sm_IsActive;
};

#endif // #if __BANK

#endif  // INC_NODE_VIEWER_H_

// eof
