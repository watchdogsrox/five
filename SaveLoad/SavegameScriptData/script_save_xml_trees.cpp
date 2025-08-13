
#include "script_save_xml_trees.h"

// **************************************** SaveGameDataBlock - Script - CScriptSaveXmlTrees ****************************

#if SAVEGAME_USES_XML

void CScriptSaveXmlTrees::Shutdown()
{
	//	Because the elements of TreeArray are parTreeNode* rather than parTreeNode, 
	//	Reset is not enough to call ~parTreeNode so I need to call delete 
	//	for each element to free all the memory for parTreeNodes
	int ArraySize = TreeArray.GetCount();
	for (int loop = 0; loop < ArraySize; loop++)
	{
		delete TreeArray[loop];
	}
	TreeArray.Reset();
}

void CScriptSaveXmlTrees::Append(parTreeNode *pTreeNode)
{
	TreeArray.PushAndGrow(pTreeNode);
}

parTreeNode *CScriptSaveXmlTrees::GetTreeNode(u32 TreeIndex)
{
	return TreeArray[TreeIndex];
}

void CScriptSaveXmlTrees::DeleteTreeFromArray(u32 TreeIndex)
{
	//		remove parTree from the array and delete the parTree
	//		will this work?
	delete TreeArray[TreeIndex];
	TreeArray.DeleteFast(TreeIndex);
}

int CScriptSaveXmlTrees::GetTreeCount()
{
	return TreeArray.GetCount();
}			  

#endif // SAVEGAME_USES_XML


