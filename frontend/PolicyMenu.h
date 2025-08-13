#ifndef INC_POLICYMENU_H_
#define INC_POLICYMENU_H_

#include "frontend/SocialClubMenu.h"

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
class VideoUploadPolicyMenu: public BaseSocialClubMenu
{
	enum eVUPolicyState
	{
		VU_IDLE,
		VU_SHOWING,
		VU_ACCEPT,
		VU_CANCEL
	};

public:
	VideoUploadPolicyMenu();
	~VideoUploadPolicyMenu();

	void Init();
	virtual void Shutdown();
	void Update();
	void Render();

	void UpdateShowing();

	bool IsIdle() const {return m_state == VU_IDLE;}
	bool HasAccepted() const {return m_state == VU_ACCEPT;}
	bool HasCancelled() const {return m_state == VU_CANCEL;}
	void GoToState(eVUPolicyState newState);

	void GoToShowing();

private:
	eVUPolicyState m_state;

	struct ComplexObject
	{
		enum Enum
		{
			Root,
			Pages_Policy,
			UpArrow,
			DownArrow,

			Noof
		};
	};

	CComplexObject m_complexObject[ComplexObject::Noof];
};

typedef atSingleton<VideoUploadPolicyMenu> SVideoUploadPolicyMenu;


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
class PolicyMenu: public BaseSocialClubMenu
{
public:
	PolicyMenu();
	~PolicyMenu();

	static void Open();
	static void Close();
	static void UpdateWrapper();
	static void RenderWrapper();

	void Init();

	static bool HasAccepted();
	static bool HasCancelled();

private:
	static void LockRender() {sm_renderingSection.Lock();}
	static void UnlockRender() {sm_renderingSection.Unlock();}

	static sysCriticalSectionToken sm_renderingSection;
};

typedef atSingleton<SocialClubMenu> SPolicyMenu;

#endif // INC_POLICYMENU_H_

// eof
