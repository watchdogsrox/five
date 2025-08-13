//
// filename:	leveldata.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "leveldata.h"
#include "leveldata_parser.h"

// C headers

// Rage headers
#include "fwsys/fileExts.h"
#include "parser/manager.h"
#include "fwscene/stores/psostore.h"

// Game headers
#include "core/core_channel.h"

// --- Defines ------------------------------------------------------------------

#define LEVELDATA_FILENAME	"platformcrc:/data/levels"
#define ALT_LEVELDATA_FILENAME "common:/data/debug/altLevels.xml"

#if !__FINAL
#define MAX_NAME_SIZE 15
#endif // !__FINAL

PARAM(altLevelsXml, "use altLevels.xml for available level names, not level.xml");

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

//
// name:		CLevelData::Init
// description:	Initialise level data. Load XML file with level data in it
void CLevelData::Init()
{
	// Load XML file
	LoadXmlData();
}

void CLevelData::Shutdown()
{
	m_aLevelsData.Reset();
}


//
// name:		CLevelData::GetFilename
// description:	Return the file name for a level
const char* CLevelData::GetFilename(s32 iLevelId) const
{
	Assertf(iLevelId > 0 && iLevelId <= m_aLevelsData.GetCount(), "Level index is out of range");

	iLevelId--;

	if (0 > iLevelId || iLevelId >= m_aLevelsData.GetCount())
		return NULL;

	return m_aLevelsData[iLevelId].GetFilename();
}

//
// name:		CLevelData::GetLevelIndexFromTitleName
// description:	Return the level index from the title name.
s32 CLevelData::GetLevelIndexFromTitleName(const char* titleName)
{
	if (AssertVerify(titleName))
	{
		for (int i=0; i<m_aLevelsData.GetCount(); i++)
		{
			if (0 == strcmp(titleName, m_aLevelsData[i].GetTitleName()))
			{
				++i; // Level indexs start at number 1 not 0
				return i;
			}
		}

		coreErrorf("Invalid Title Name \"%s\"", titleName);
	}

	return -1;
}

//
// name:		CLevelData::GetTitleName
// description:	Return the title name for the level.
const char* CLevelData::GetTitleName(s32 iLevelId) const
{
	Assertf(iLevelId > 0 && iLevelId <= m_aLevelsData.GetCount(), "Level index is out of range");

	iLevelId--;

	if (0 > iLevelId || iLevelId >= m_aLevelsData.GetCount())
		return NULL;

	return m_aLevelsData[iLevelId].GetTitleName();
}

#if RSG_OUTPUT 
//
// name:		CLevelData::GetFriendlyName
// description:	Return the friendly name for the level.
const char*  CLevelData::GetFriendlyName(s32 iLevelId) const
{
	Assertf(iLevelId > 0 && iLevelId <= m_aLevelsData.GetCount(), "Level index is out of range");
	iLevelId--;
	if (0 > iLevelId || iLevelId >= m_aLevelsData.GetCount())
		return NULL;

	return m_aLevelsData[iLevelId].GetFriendlyName();
}
#endif

#if !RSG_FINAL
//
// name:		CLevelData::GetBugstarName
// description:	Return the name used by bugstar for the level.
const char*  CLevelData::GetBugstarName(s32 iLevelId) const
{
	Assertf(iLevelId > 0 && iLevelId <= m_aLevelsData.GetCount(), "Level index is out of range");
	iLevelId--;
	if (0 > iLevelId || iLevelId >= m_aLevelsData.GetCount())
		return NULL;

	return m_aLevelsData[iLevelId].GetBugstarName();
}

//
// name:		CLevelData::GetLevelIndexFromFriendlyName
// description:	Return the level that goes by pFriendlyName.
s32 CLevelData::GetLevelIndexFromFriendlyName(const char* pFriendlyName)
{
	if (pFriendlyName)
	{
		for (int iLevelId=0; iLevelId < m_aLevelsData.GetCount(); iLevelId++)
		{
			Assertf(strlen(m_aLevelsData[iLevelId].GetFriendlyName()) < MAX_NAME_SIZE, "Level name is too long \"%s\"", m_aLevelsData[iLevelId].GetFriendlyName());
			Assertf(strlen(pFriendlyName) < MAX_NAME_SIZE, "Level name is too long \"%s\"",pFriendlyName);

			char cLevel_A[MAX_NAME_SIZE];
			char cLevel_B[MAX_NAME_SIZE];

			formatf(cLevel_A, MAX_NAME_SIZE, "%s", m_aLevelsData[iLevelId].GetFriendlyName());
			formatf(cLevel_B, MAX_NAME_SIZE, "%s", pFriendlyName);

			if (strlen(m_aLevelsData[iLevelId].GetFriendlyName())+1 < MAX_NAME_SIZE && strlen(pFriendlyName)+1 < MAX_NAME_SIZE)
			{
				cLevel_A[strlen(m_aLevelsData[iLevelId].GetFriendlyName())+1] = '\0';
				cLevel_B[strlen(pFriendlyName)+1] = '\0';
			}
			else
			{
				cLevel_A[MAX_NAME_SIZE-1] = '\0';
				cLevel_B[MAX_NAME_SIZE-1] = '\0';
			}

			if (!strcmp(strupr(cLevel_A), strupr(cLevel_B)))
			{
				return iLevelId+1;
			}
		}
	}

	Errorf("Friendly level name (%s) not found", pFriendlyName);

	// Always default to level 1.
	return 1;
}

#endif // RSG_OUTPUT


// name:		CLevelData::GetName
// description:	Return true if the level exists.
bool CLevelData::Exists(s32 iLevelId)
{
	if (iLevelId < 0 && iLevelId >= m_aLevelsData.GetCount())
		return false;

	return true;
}


void CLevelData::LoadXmlData()
{
	INIT_PARSER;

	parTree* pTree = NULL;
#if !__FINAL
	if (PARAM_altLevelsXml.Get())
	{
		pTree = PARSER.LoadTree(ALT_LEVELDATA_FILENAME, "xml");
	}
	else
#endif //!__FINAL
	{
		fwPsoStoreLoader::LoadDataIntoObject(LEVELDATA_FILENAME, META_FILE_EXT, *this);
	}

	if (pTree)
	{
		Shutdown();

		parTreeNode* pRootNode= pTree->GetRoot();
		parTreeNode* pNode=NULL;

		while((pNode = pRootNode->FindChildWithName("level", pNode)) != NULL)
		{
			Assertf(pNode->GetElement().FindAttribute("title"),    "[CLevelData] Missing attribute \"title\"");
			Assertf(pNode->GetElement().FindAttribute("filename"), "[CLevelData] Missing attribute \"filename\"");

#if !__FINAL
			Assertf(pNode->GetElement().FindAttribute("friendlyName"), "[CLevelData] Missing attribute \"friendlyName\"");

			if( pNode->GetElement().FindAttribute("title") && pNode->GetElement().FindAttribute("filename")  && pNode->GetElement().FindAttribute("friendlyName") )
#else
			if( pNode->GetElement().FindAttribute("title") && pNode->GetElement().FindAttribute("filename") )
#endif
			{
				sLevelData newLevelData;
				newLevelData.m_cTitle = pNode->GetElement().FindAttribute("title")->GetStringValue();
				newLevelData.m_cFilename = pNode->GetElement().FindAttribute("filename")->GetStringValue();

#if !__FINAL
				newLevelData.m_cFriendlyName = pNode->GetElement().FindAttribute("friendlyName")->GetStringValue();
				if( pNode->GetElement().FindAttribute("bugstarName"))
				   newLevelData.m_cBugstarName = pNode->GetElement().FindAttribute("bugstarName")->GetStringValue();
#endif

				m_aLevelsData.PushAndGrow(newLevelData);
			}
		}

		delete pTree;
	}

	SHUTDOWN_PARSER;
}
