#ifndef __MINIMAPCOMPONENT_H__
#define __MINIMAPCOMPONENT_H__


#include "Vector/Vector3.h"
#include "atl/map.h"
#include "frontend/MiniMapCommon.h"


class CMiniMapBlip;
struct PauseMenuRenderDataExtra;

class CMiniMapMenuComponent
{
public:
	CMiniMapMenuComponent(): m_bIsActive(false), m_uBlipCount(0) {}
	~CMiniMapMenuComponent() {}

	void InitClass();
	void ShutdownClass();
	void ResetBlips();

	void SetActive(bool bActive);
	bool IsActive()										{ return m_bIsActive; }

	bool DoesBlipExist(int iIndex);

	bool SnapToBlip(int iIndex);
	bool SnapToBlip(CMiniMapBlip *pBlip);
	bool SnapToBlipUsingUniqueId(int iIndex);
	bool SnapToBlipWithDistanceCheck(s32 iKey, float fDistanceRequirement);
	void SetWaypoint();

	s32 CreateBlip(Vector3& vPosition);
	s32 CreateBlip(Vector3& vPosition, u32 uKey);

	bool RemoveBlip(s32 iIndex);
	bool RemoveBlipByUniqueId(s32 iUniqueId);
	bool RemoveBlip(CMiniMapBlip* pBlip);

	int GetNumberOfBlips()								{ return m_pGalleryBlips.GetNumUsed(); }
	void SetBlipObject(const BlipLinkage& iBlipObject)	{ m_iBlipLinkageId = iBlipObject;}
	const BlipLinkage& GetBlipObject()					{ return m_iBlipLinkageId; }

	void RenderMap(const PauseMenuRenderDataExtra& renderData);
private:
	void ResetClass();

private:

	BlipLinkage	 m_iBlipLinkageId;
	bool m_bIsActive;
	u8	m_uBlipCount;

	atMap<s32,s32> m_pGalleryBlips;

};

extern CMiniMapMenuComponent sMiniMapMenuComponent;
#endif