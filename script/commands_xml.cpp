
// Rage headers
#include "script\wrapper.h"
#include "parser\manager.h"
// Game headers
#include "debug\BugstarIntegration.h"
#include "script\script.h"
#include "script\script_helper.h"

SCRIPT_OPTIMISATIONS()
namespace xml_commands
{

//Check if the XML file can be loaded
	
bool CommandLoadXmlFile (const char *pFileName )
{
	bool bXmlFileLoaded = false;

	USE_DEBUG_MEMORY();

	if (!CTheScripts::GetXmlTree())
	{
		INIT_PARSER;
	
		ASSET.PushFolder("common:/data/script/XML/");
	
		CTheScripts::SetXmlTree(PARSER.LoadTree(pFileName, "xml"));		//PARSE the file

		if (CTheScripts::GetXmlTree())
		{
			CTheScripts::SetCurrentXmlTreeNode(CTheScripts::GetXmlTree()->GetRoot());		//sets the current Node to the root 
			bXmlFileLoaded = true;
		}
		else
		{
			scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "LOAD_XML_FILE - Failed to load XML file");
		}
		
		ASSET.PopFolder();	
	}
	else
	{
		scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "LOAD_XML_FILE - XML file already loaded");
	}
	
	return bXmlFileLoaded;
}

//delete loaded xml file
void CommandDeleteXmlFile ()
{
	if (CTheScripts::GetXmlTree())
	{	
		delete CTheScripts::GetXmlTree();
		CTheScripts::SetXmlTree(NULL);
		CTheScripts::SetCurrentXmlTreeNode(NULL);
	}
}

//Gets a pointer to the next node in a ParTree
//it searches the tree:
//1)get child, if no child
//2)get sibling, if no sibling 
//3)get parent then sibling

parTreeNode* GetNextNode (parTreeNode * pCurrentNode)
{
	parTreeNode* pNode = pCurrentNode;			
	parTreeNode* pNodeCurrent = pCurrentNode;		//this is always points to a valid node 

	const int GetChildNode		= 0;
	const int GetSiblingNode	= 1;
	const int GetParentNode		= 2;
	
	int NodeType = GetChildNode; 

	bool bGettingNode = true;

	while (bGettingNode)
	{
		switch (NodeType)
		{
		case GetChildNode:						
			
			pNode = pNodeCurrent -> GetChild();	

				if(pNode)
				{
					bGettingNode = false;		//found a child
					break;
				}
				else
				{
					NodeType = GetSiblingNode;
				}
			

		case GetSiblingNode:

			pNode = pNodeCurrent -> GetSibling();
				
				if (pNode)
				{
					bGettingNode = false;			//found a sibling
					break;
				}
				else
				NodeType = GetParentNode;
		
		case GetParentNode:
			if (pNodeCurrent != CTheScripts::GetXmlTree()->GetRoot()) //not trying to get the parent of the root node
			{
				pNode = pNodeCurrent -> GetParent();		//this should always point to a valid node within the tree

				if (pNode)
				{
					if (pNode == CTheScripts::GetXmlTree()->GetRoot() )		// the pointer is back to the root
					{				
						bGettingNode = false;
						break; 	//back at the root 	
					}
					else
					{
						NodeType = GetSiblingNode;								//go get a sibling node, its not the root node 
					}
					pNodeCurrent = pNode;										//update pNodeCurrent to the new node 
				}
				else
				scriptAssertf (0, "Has grabbed a null parent node" );				
			}
			else
			{
				pNode = pNodeCurrent;											
				bGettingNode = false;
				break;
			}
		
		}
	}
	return pNode;
}

//Updates the static current node pointer to point to the next node in the tree 
void CommandGetNextNode()
{
	CTheScripts::SetCurrentXmlTreeNode(GetNextNode(CTheScripts::GetCurrentXmlTreeNode()));
}

//Counts the total number of nodes in a tree
int CommandGetTotalNumberOfNodes ()
{
	int NumberOfNodes = 0;
	parTreeNode * TempRoot = CTheScripts::GetXmlTree()->GetRoot();

	parTreeNode * TempNode = NULL;

	while (TempNode != CTheScripts::GetXmlTree()->GetRoot())	//ensures it counts at least once for the root node
	{
		NumberOfNodes++;
		TempNode = GetNextNode (TempRoot);
		TempRoot = TempNode;

	}
	return NumberOfNodes;
}

//Returns the name of the node
const char *GetNodeName ()
{
	parElement& CurrentElement = CTheScripts::GetCurrentXmlTreeNode()->GetElement();

	return CurrentElement.GetName();
}

//Returns the number of attributes for a given node
int GetNumberOfNodeAttributes ()
{
	int iNumberofAttributes= 0;
	
	parElement& CurrentElement = CTheScripts::GetCurrentXmlTreeNode()->GetElement();

	atArray<parAttribute>& ElementAttributeArray = CurrentElement.GetAttributes();
	
	iNumberofAttributes = ElementAttributeArray.GetCount();
	
	return iNumberofAttributes;
}
//Returns name of the attribute
const char * GetAttributeName(int AttributeIndex)
{
	int iNumberofAttributes= 0;
	const char *AttributeName = NULL;

	parElement& CurrentElement = CTheScripts::GetCurrentXmlTreeNode()->GetElement();

	atArray<parAttribute>& ElementAttributeArray = CurrentElement.GetAttributes();

	iNumberofAttributes = ElementAttributeArray.GetCount();

	if (AttributeIndex <= iNumberofAttributes -1)
		AttributeName = ElementAttributeArray[AttributeIndex].GetName();
	else
		Assertf(0, "Attribute does not exist cannot return a value");
	
	return AttributeName;
}

//Returns the int value of given attribute
int GetAttributeIntValue (int AttributeIndex)
{
	int iNumberofAttributes= 0;
	int iAttributeValue = 0;
	parElement& CurrentElement = CTheScripts::GetCurrentXmlTreeNode()->GetElement();

	atArray<parAttribute>& ElementAttributeArray = CurrentElement.GetAttributes();

	iNumberofAttributes = ElementAttributeArray.GetCount();

	if (AttributeIndex <= iNumberofAttributes -1)			//check that the script is checking for a valid index
	{
		iAttributeValue = ElementAttributeArray[AttributeIndex].FindIntValue();		//can take an int or string as an argument
	}
	else
		scriptAssertf(0, "Attribute does not exist cannot return a value");
	
		return iAttributeValue;
}

//Returns the float of given attribute
float GetAttributeFloatValue (int AttributeIndex)
{
	int iNumberofAttributes= 0;
	float fAttributeValue = 0;
	parElement& CurrentElement = CTheScripts::GetCurrentXmlTreeNode()->GetElement();

	atArray<parAttribute>& ElementAttributeArray = CurrentElement.GetAttributes();

	iNumberofAttributes = ElementAttributeArray.GetCount();

	if (AttributeIndex <= iNumberofAttributes -1)
	{
		fAttributeValue = ElementAttributeArray[AttributeIndex].FindFloatValue(); //can take an float or string as an argument
	}
	else
		scriptAssertf(0, "Attribute does not exist cannot return a value");

	return fAttributeValue;
}

//Returns the bool of given attribute
bool GetAttributeBoolValue (int AttributeIndex)
{
	int iNumberofAttributes= 0;
	bool bAttributeValue = 0;
	parElement& CurrentElement = CTheScripts::GetCurrentXmlTreeNode()->GetElement();

	atArray<parAttribute>& ElementAttributeArray = CurrentElement.GetAttributes();

	iNumberofAttributes = ElementAttributeArray.GetCount();

	if (AttributeIndex <= iNumberofAttributes -1)
	{
		bAttributeValue = ElementAttributeArray[AttributeIndex].FindBoolValue(); //can take an bool or string as an argument
	}
	else
		scriptAssertf(0, "Attribute does not exist cannot return a value");

	return bAttributeValue;
}

//Returns the string of given attribute
const char * GetAttributeStringValue (int AttributeIndex)
{
	int iNumberofAttributes= 0;
	const char *AttributeName = NULL;
	parElement& CurrentElement = CTheScripts::GetCurrentXmlTreeNode()->GetElement();

	atArray<parAttribute>& ElementAttributeArray = CurrentElement.GetAttributes();

	iNumberofAttributes = ElementAttributeArray.GetCount();

	if (AttributeIndex <= iNumberofAttributes -1)
		AttributeName = ElementAttributeArray[AttributeIndex].GetStringValue();	
	else
		scriptAssertf(0, "Attribute does not exist cannot return a value");

	return AttributeName;
}

//Checks the tree for a node with the same name. Its stops searching if the search gets back to the root node. 
bool CommandGotoXmlNode (const char* NodeName)
{
	int iNumberOfNodes = CommandGetTotalNumberOfNodes ();
	bool bFoundNamedNode = false;

	parTreeNode* pTreeNode = CTheScripts::GetCurrentXmlTreeNode(); 

	for (int i=0; i<iNumberOfNodes; i++)
	{
		pTreeNode = GetNextNode (pTreeNode);
		
		//search until we find the root, and then stop the search
		if  (pTreeNode != CTheScripts::GetXmlTree()->GetRoot())
		{
			if (StringEquals(pTreeNode->GetElement().GetName(), NodeName))
			{
				CTheScripts::SetCurrentXmlTreeNode(pTreeNode);
				bFoundNamedNode = true;
				break;
			}
		}
		else
		{
			//found the root node, check that we weren't searching for it and break the search
			if (StringEquals(pTreeNode->GetElement().GetName(), NodeName))
			{
				CTheScripts::SetCurrentXmlTreeNode(pTreeNode);
				bFoundNamedNode = true;
			}
			break;
		}
	}
	
	return bFoundNamedNode;
}

const char* CommandGetStringFromXmlNode()
{
	parTreeNode* pTreeNode = CTheScripts::GetCurrentXmlTreeNode();
	
	if(pTreeNode)
	{
		return pTreeNode->GetData(); 
	}
	return NULL; 
}

#if !__FINAL

#if BUGSTAR_INTEGRATION_ACTIVE

class CBugstarQueryOp : public CScriptOp
{
public:
	void Init()
	{
		Assertf(m_query == NULL, "Cannot initialise a bugstar query while one is running");
		m_queryString = BUGSTAR_GTA_PROJECT_PATH;
	}
	void AppendString(const char* pString)
	{
		Assertf(m_query == NULL, "Cannot initialise a bugstar query while one is running");
		m_queryString += pString;
	}
	void Start()
	{
		Assertf(m_query == NULL, "Cannot start a bugstar query while one is running");
		m_query = CBugstarIntegration::CreateHttpQuery();
		scriptVerifyf(m_query->Init(m_queryString.c_str(), 30), "START_BUGSTAR_QUERY: Bugstar query initialisation failed");
		m_queryString.Clear();

		CTheScripts::GetScriptOpList().AddOp(this);
	}
	bool Pending()
	{
		return m_query?m_query->Pending():false;
	}
	bool Succeeded()
	{
		Assertf(m_query, "There is no bugstar query currently running");
		bool rt = m_query->Succeeded();
		if(!rt)
			m_query->WriteBufferToFile("restError.html");
		return rt;
	}
	fwHttpQuery* GetQuery()
	{
		return m_query;
	}
	void Flush()
	{
		if(m_query)
		{
			while(m_query->Pending())
				sysIpcSleep(100);
			delete m_query;
		}
		m_query = NULL;

		CTheScripts::GetScriptOpList().RemoveOp(this);
	}

private:
	atString m_queryString;
	fwHttpQuery* m_query;

} g_BugstarQueryOp;

#endif //BUGSTAR_INTEGRATION_ACTIVE

#if BUGSTAR_INTEGRATION_ACTIVE
void StartBugstarQuery(const char* pQuery)
#else
void StartBugstarQuery(const char* )
#endif //BUGSTAR_INTEGRATION_ACTIVE
{
#if BUGSTAR_INTEGRATION_ACTIVE
	g_BugstarQueryOp.Init();
	g_BugstarQueryOp.AppendString(pQuery);
	g_BugstarQueryOp.Start();
#endif
}

void CommandStartBugstarQueryAppend()
{
#if BUGSTAR_INTEGRATION_ACTIVE
	g_BugstarQueryOp.Init();
#endif
}

#if BUGSTAR_INTEGRATION_ACTIVE
void CommandAddBugstarQueryString(const char* pQueryStringToAppend)
#else
void CommandAddBugstarQueryString(const char* )
#endif	//	BUGSTAR_INTEGRATION_ACTIVE
{
#if BUGSTAR_INTEGRATION_ACTIVE
	g_BugstarQueryOp.AppendString(pQueryStringToAppend);
#endif
}

void CommandStopBugstarQueryAppend()
{
#if BUGSTAR_INTEGRATION_ACTIVE
	g_BugstarQueryOp.Start();
#endif
}

bool IsBugstarQueryPending()
{
#if BUGSTAR_INTEGRATION_ACTIVE
	return g_BugstarQueryOp.Pending();
#else
	return false;
#endif
}

bool IsBugstarQuerySuccessful()
{
#if BUGSTAR_INTEGRATION_ACTIVE
	return g_BugstarQueryOp.Succeeded();
#else
	return false;
#endif
}

bool CreateXmlFromBugstarQuery()
{
#if BUGSTAR_INTEGRATION_ACTIVE
	scriptAssertf(g_BugstarQueryOp.GetQuery(), "CREATE_XML_FROM_BUGSTAR_QUERY: There is no active Bugstar query");

	bool bXmlFileLoaded = false;

	if (!CTheScripts::GetXmlTree())
	{
		CTheScripts::SetXmlTree(g_BugstarQueryOp.GetQuery()->CreateTree());

		if (CTheScripts::GetXmlTree())
		{
			CTheScripts::SetCurrentXmlTreeNode(CTheScripts::GetXmlTree()->GetRoot());		//sets the current Node to the root 
			bXmlFileLoaded = true;
		}
		else
		{
			scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "CREATE_XML_FROM_BUGSTAR_QUERY - Failed to create XML from bugstar request");
		}
	}
	else
	{
		scriptAssertf(0, "%s:%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "CREATE_XML_FROM_BUGSTAR_QUERY - XML file already loaded");
	}

	return bXmlFileLoaded;
#else
	return false;
#endif
}

void EndBugstarQuery()
{
#if BUGSTAR_INTEGRATION_ACTIVE
	g_BugstarQueryOp.Flush();
#endif
}

#endif //!__FINAL

void SetupScriptCommands()
{
		SCR_REGISTER_UNUSED(LOAD_XML_FILE,0x7308c3e9c183d6c0, CommandLoadXmlFile );
		SCR_REGISTER_UNUSED(DELETE_XML_FILE,0x0bd0e389aee58784, CommandDeleteXmlFile );
		SCR_REGISTER_UNUSED(GET_NUMBER_OF_XML_NODES,0x5fad86215aac9ea2, CommandGetTotalNumberOfNodes );
		SCR_REGISTER_UNUSED(GET_NEXT_XML_NODE,0x21079b91ee2d85f2, CommandGetNextNode );
		SCR_REGISTER_UNUSED(GET_XML_NODE_NAME,0xf9963e233c5e633e, GetNodeName );
		SCR_REGISTER_UNUSED(GET_NUMBER_OF_XML_NODE_ATTRIBUTES,0x41f2cfe090a1a1db, GetNumberOfNodeAttributes);
		SCR_REGISTER_UNUSED(GET_XML_NODE_ATTRIBUTE_NAME,0x8a56188e381d0998, GetAttributeName);
		SCR_REGISTER_UNUSED(GET_INT_FROM_XML_NODE_ATTRIBUTE,0x6bad273237de4c31, GetAttributeIntValue);
		SCR_REGISTER_UNUSED(GET_FLOAT_FROM_XML_NODE_ATTRIBUTE,0x14765a123105f84a, GetAttributeFloatValue);
		SCR_REGISTER_UNUSED(GET_BOOL_FROM_XML_NODE_ATTRIBUTE,0x6306cd55f1405357, GetAttributeBoolValue);
		SCR_REGISTER_UNUSED(GET_STRING_FROM_XML_NODE_ATTRIBUTE,0xcf604d18b5efa5d9, GetAttributeStringValue);
		SCR_REGISTER_UNUSED(GET_XML_NAMED_NODE,0x17e1549cfee24ed6, CommandGotoXmlNode);
		SCR_REGISTER_UNUSED(GET_STRING_FROM_XML_NODE,0x587b900010500f5b,CommandGetStringFromXmlNode);

		SCR_REGISTER_UNUSED(START_BUGSTAR_QUERY,0x0e1e1bbdc37a34dd, StartBugstarQuery );
		SCR_REGISTER_UNUSED(START_BUGSTAR_QUERY_APPEND,0xb45235d37b79e86b, CommandStartBugstarQueryAppend);
		SCR_REGISTER_UNUSED(ADD_BUGSTAR_QUERY_STRING,0x09a683fe4e043ff3, CommandAddBugstarQueryString);
		SCR_REGISTER_UNUSED(STOP_BUGSTAR_QUERY_APPEND,0x76d2065d217e0ef4, CommandStopBugstarQueryAppend);
		SCR_REGISTER_UNUSED(IS_BUGSTAR_QUERY_PENDING,0x4ebd295719063987, IsBugstarQueryPending );
		SCR_REGISTER_UNUSED(IS_BUGSTAR_QUERY_SUCCESSFUL,0xed5ea3222ddbfb65, IsBugstarQuerySuccessful );
		SCR_REGISTER_UNUSED(END_BUGSTAR_QUERY,0xf7417c99b0730709, EndBugstarQuery );
		SCR_REGISTER_UNUSED(CREATE_XML_FROM_BUGSTAR_QUERY,0xc128000fc02572f2, CreateXmlFromBugstarQuery );
}

} // namespace xml_commands
