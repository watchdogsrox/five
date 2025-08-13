
#ifndef SCRIPT_SAVE_XML_TREES_H
#define SCRIPT_SAVE_XML_TREES_H

// Rage headers
#include "parser/treenode.h"

// Game headers
#include "SaveLoad/savegame_defines.h"

#if SAVEGAME_USES_XML

class CScriptSaveXmlTrees
{
private:
	atArray<parTreeNode*> TreeArray;

public:
	void Shutdown();

	void Append(parTreeNode *pTreeNode);

	parTreeNode *GetTreeNode(u32 TreeIndex);

	void DeleteTreeFromArray(u32 TreeIndex);
	int GetTreeCount();
};

#endif


#endif // SCRIPT_SAVE_XML_TREES_H

