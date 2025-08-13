#ifndef __CSCRIPTMENU_H__
#define __CSCRIPTMENU_H__

#include "CMenuBase.h"
#include "atl/string.h"
#include "script/thread.h"

// Forward declarations
#if __BANK
namespace rage {
	class bkGroup;
}
#endif
class GtaThread;

/// MUST BE KEPT IN SYNC WITH SCRIPT
enum eOperationTypes
{
	kFill = 0,
	kLayoutChange = 1,
	kTriggerEvent = 2,
	kUpdate = 3,
	kPrepareButtons = 4,
	kUpdateInput = 5,
	kPopulatePeds = 100
};

/// MUST BE KEPT IN SYNC WITH SCRIPT
struct SPauseMenuScriptStruct
{
	SPauseMenuScriptStruct()
	{
		eTypeOfInteraction.Int = 0;
		MenuScreenId.Int = 0;
		PreviousId.Int = 0;
		iUniqueId.Int = 0;
	}

	SPauseMenuScriptStruct(s32 _type, s32 _menu, s32 _Previous, s32 _Unique)
	{
		eTypeOfInteraction.Int = _type;
		MenuScreenId.Int = _menu;
		PreviousId.Int = _Previous;
		iUniqueId.Int = _Unique;
	}

	scrValue eTypeOfInteraction;
	scrValue MenuScreenId;
	scrValue PreviousId;
	scrValue iUniqueId;
};



//////////////////////////////////////////////////////////////////////////
/// Pause Menu Type that contains a script it runs to inform menu decisions
class CScriptMenu : public CMenuBase
{
public:
	CScriptMenu(CMenuScreen& owner);
	virtual ~CScriptMenu();
	virtual bool Populate( MenuScreenId newScreenId );

	virtual void Init();
	virtual void LoseFocus();
	virtual void Update();
	virtual void LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir );
	virtual bool TriggerEvent(MenuScreenId MenuId, s32 iUniqueId);

	virtual void PrepareInstructionalButtons( MenuScreenId MenuId, s32 iUniqueId);

#if !__FINAL
	virtual void Render(const PauseMenuRenderDataExtra* UNUSED_PARAM(renderData));
#endif

#if __BANK
	virtual void AddWidgets(bkBank* pBank);
#endif

protected:

	void HandleStreaming();
	void LaunchScript( const SPauseMenuScriptStruct& args );
	void KillScript();

	virtual atString& GetScriptPath() { return m_ScriptPath; }
	
	atString		m_ScriptPath;
	scrProgramId	m_MyProgramId;
	int				m_MyStreamRequest;
	scrThreadId		m_MyThreadId;
	GtaThread*		m_pMyThread;
	bool			m_bContinualUpdateMode;
	bool			m_bAllowToLive;
	bool			m_bHasButtonLogic;
	int				m_StackSize;
	atArray<SPauseMenuScriptStruct>	m_QueuedActions;

#if __BANK
	bkGroup*		m_pMyGroup;
#endif
};


#endif // __CSCRIPTMENU_H__

