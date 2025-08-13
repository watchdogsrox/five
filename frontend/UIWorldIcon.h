#ifndef __UIWORLD_ICON_H__
#define __UIWORLD_ICON_H__

// rage
#include "atl/atfixedstring.h"
#include "atl/array.h"
#include "atl/singleton.h"
#include "script/thread.h"

//game
#include "renderer/sprite2d.h"
#include "scene/RegdRefTypes.h"

#define INVALID_WORLD_ICON_ID -1
#define WORLD_ICON_KEEP_RENDERING -1
#define QUERY_PIXEL_HISTORY 4

class UIWorldIcon
{
	friend class UIWorldIconManager;

public:
	UIWorldIcon();
	~UIWorldIcon();

	void Init(CEntity* pEnt, const char* pIconTexture, int id, scrThreadId scriptId);
	void Init(const Vector3& pos, const char* pIconTexture, int id, scrThreadId scriptId);

	void Clear();
	void Update();
	void AddToDrawList(bool isPlayedAlive);

	bool HasEntity() const {return m_Entity != NULL;}
	bool IsEntity(CEntity* pEnt) const {return m_Entity == pEnt;}
	bool IsAtPosition(const Vector3& pos) const {return m_entityPos == pos;}
	bool IsFlaggedForDeletion() const { return m_flaggedForDeletion || (m_hasEntity && IsEntity(NULL)); }
	void FlagForDeletion() {m_flaggedForDeletion = true;}

	void SetColor(const Color32& rColor) {m_icon.SetColor(rColor);}
	void SetBGColor(const Color32&) {}
	void SetClamp(bool ) {}
	void SetVisibility(bool visibility) {m_visibility = visibility;}
	void SetShowBG(bool ) {}
	void SetLifetime(int lifetime) {m_lifetime = lifetime;}
	void SetText(const char*) {}

	int GetId() const {return m_id;}
	inline bool IsValid() const {return m_id != INVALID_WORLD_ICON_ID;}

	scrThreadId GetScriptId() const {return m_scriptId;}

private:

	void InitCommon(const char* pIconTexture);

	static void UpdateViewport();

	int m_id;
	scrThreadId m_scriptId;
	CUIWorldIconQuad m_icon;

	RegdEnt m_Entity;
	Vector3 m_entityPos;

	atFixedString<32> m_texture;


	bool m_visibility;
	bool m_flaggedForDeletion;
	bool m_hasEntity;
	u8 m_renderDelay; // We need to add a delay before rendering to give the occlusion check some time to finish.
	int m_lifetime;


	static const grcViewport* sm_pViewport;

	static bank_float sm_pedHeight;
	static bank_float sm_vehicleHeight;
	static bank_float sm_entityHeight;
	static bank_float sm_aboveScreenBoundry; // deprecated

	static bank_float sm_maxScaleDistance; // deprecated
	static bank_float sm_minScaleDistance; // deprecated
	static bank_float sm_scaleAtMaxDistance; // deprecated
	static bank_float sm_scaleAtMinDistance; // deprecated

	static bank_float sm_iconWidth;
	static bank_float sm_iconHeight;

	static bank_float sm_borderWidth; // deprecated
	static bank_float sm_borderHeight; // deprecated

	static bank_bool sm_defaultShowBG; // deprecated
	static bank_bool sm_defaultShouldClamp; // deprecated
	static bank_bool sm_defaultVisibility;
	static bank_s32 sm_defaultLifetime;
	static bank_u8 sm_renderDelay;

	static bank_float sm_distanceToShowText; // deprecated
	static bank_float sm_defaultTextScale; // deprecated
	static bank_float sm_defaultTextPosOffsetX; // deprecated
	static bank_float sm_defaultTextPosOffsetY; // deprecated
	static bank_u8 sm_defaultTextBGAlpha; // deprecated
	static bank_bool sm_defaultUseTextBG; // deprecated
	static atFinalHashString sm_defaultIconText; // deprecated
};




class UIWorldIconManager
{
public:
	static void Open();
	static void Close();
	static void UpdateWrapper();
	static void AddItemsToDrawListWrapper();

	UIWorldIconManager();
	~UIWorldIconManager();

	void Update();
	void AddItemsToDrawList();

	UIWorldIcon* Add(CEntity* pEnt, const char* pTextureName, scrThreadId scriptId);
	UIWorldIcon* Add(const Vector3& pos, const char* pTextureName, scrThreadId scriptId);
	void Remove(CEntity* pEnt);
	void Remove(const Vector3& pos);
	void Remove(int iconId);
	UIWorldIcon* Find(CEntity* pEnt);
	UIWorldIcon* Find(const Vector3& pos);
	UIWorldIcon* Find(int iconId);

	void RemovePendingDeletions();


	void RemoveAll();

#if __BANK
	static void CreateBankWidgets();
	static void InitWidgets();
	static void BankResetIconDefaultsWrapper();
	void BankResetIconDefaults();
#endif

private:

	int GetNextId();
	int GetNextId(UIWorldIcon*& pIcon);

	bool m_isPlayerAlive;
	int m_currentId;
	atRangeArray<UIWorldIcon, 10> m_worldIcons;
};

typedef atSingleton<UIWorldIconManager> SUIWorldIconManager;

#endif // __UIWORLD_ICON_H__
