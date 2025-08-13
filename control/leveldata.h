//
// filename:	leveldata.h
// description:	Maintains information about the levels: Title Label and Default 
//               level setup file (Load IDEs first, then the models and after that the IPLs).
//

#ifndef INC_LEVELDATA_H_
#define INC_LEVELDATA_H_

// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers
#include "atl/array.h"
#include "atl/string.h"
#include "parser/macros.h"

// Game headers

// --- Structure/Class Definitions ----------------------------------------------

struct sLevelData
{
	//Title Label
	atString  m_cTitle;

	//Default level setup file (Load IDEs first, then the models and after that the IPLs)
	atString  m_cFilename;

//#if !__FINAL
	atString  m_cBugstarName;
	atString  m_cFriendlyName;
//#endif

	sLevelData()
	{
	}
	virtual ~sLevelData()
	{
		m_cTitle.Clear();
		m_cFilename.Clear();

#if RSG_OUTPUT
		m_cFriendlyName.Clear();
#endif
	}

	void Init() 
	{
	}

	const char* GetTitleName() const { return m_cTitle;    }
	const char* GetFilename() const  { return m_cFilename; }

#if RSG_OUTPUT
	const char* GetFriendlyName() const { return m_cFriendlyName; }
	const char* GetBugstarName() const { return m_cBugstarName; }
#endif 

	PAR_PARSABLE;
};

//PURPOSE
//  Maintains certain information about the levels:
//    - Title Label.
//    - Default level setup file (Load IDEs first, then the models and after that the IPLs).
class CLevelData
{
public:
	virtual ~CLevelData(){}
	void Init();
	void Shutdown();
	const char*  GetFilename(s32 iLevelId) const;  // first level is 1 not 0
	const char*  GetTitleName(s32 iLevelId) const;
	s32        GetLevelIndexFromTitleName(const char* titleName);

#if RSG_OUTPUT 
	const char*  GetFriendlyName(s32 iLevelId) const;
#endif
#if !RSG_FINAL
	s32        GetLevelIndexFromFriendlyName(const char* pFriendlyName);
	const char*  GetBugstarName(s32 iLevelId) const;
#endif

	bool         Exists(s32 iLevelId);

	//PURPOSE: Retunr the total number of levels.
	inline int   GetCount() const { return m_aLevelsData.GetCount(); }

private:
	void LoadXmlData();

	//All Level information:
	//    - Title Label
	//    - Default level setup file (Load IDEs first, then the models and after that the IPLs)
	atArray< sLevelData > m_aLevelsData;

	PAR_PARSABLE;
};

// --- Globals ------------------------------------------------------------------

#endif // !INC_LEVELDATA_H_
