//--------------------------------------------------------------------------------------
// companion_blips.h
//--------------------------------------------------------------------------------------
#if COMPANION_APP
#pragma once

#include "frontend/MiniMapCommon.h"

#define MAX_BLIP_MESSAGE_DATA_SIZE	700

enum eCompanionBlipFlags
{
	COMPANIONBLIP_CREW_INDICATOR	= (1 << 0),		// when this is set, secondary colour is active
	COMPANIONBLIP_TICK_INDICATOR	= (1 << 1),		// when this is set, blip has a green tick
	COMPANIONBLIP_TYPE_RADIUS		= (1 << 2),		// no icon just a radius and a size
	COMPANIONBLIP_FLASH_INDICATOR	= (1 << 3),		// when this is set, we want the blip flashing
	COMPANIONBLIP_NO_LEGEND			= (1 << 4),		// when this is set, the blip wont appear in legend
	COMPANIONBLIP_FRIEND_INDICATOR	= (1 << 5),		// when this is set, the friend indicator is shown
	COMPANIONBLIP_GOLD_TICK_INDICATOR = (1 <<6),	// when this is set, blip has a gold tick
};
//--------------------------------------------------------------------------------------
//	Companion blip structures
//
//	***** If the size of these structures change we need to match it on the companion
//	***** app HTML side.  If you need to change this you need to inform whoever is 
//	***** working on the companion app.
//--------------------------------------------------------------------------------------
typedef struct 
{
	double x;
	double y;
	BlipLinkage iconId;
	s32 id;
	s16 rotation;
	u8 r;
	u8 g;
	u8 b;
	u8 a;
	u8 labelSize;
	u8 flags;
	u8 priority;
	u8 category;
}sCompanionBlipBasic;
//	See above comments if this fires
CompileTimeAssertSize(sCompanionBlipBasic, 40, 40);

typedef struct  
{
	sCompanionBlipBasic basicInfo;
	char label[MAX_BLIP_NAME_SIZE];
	u32 secondaryColour;
	float scale;
}sCompanionBlip;

//--------------------------------------------------------------------------------------
//	Companion blip message class
//--------------------------------------------------------------------------------------
class CCompanionBlipMessage
{
public:
	//	Ctor/Dtor
	CCompanionBlipMessage();
	~CCompanionBlipMessage();

	//	Add a blip to the message
	bool AddBlip(sCompanionBlip& newBlip);
	//	Get encoded message
	char* GetEncodedMessage();
	//	Clear the message
	void ClearMessage();
	//	Get number of blips added
	u8 GetBlipsAdded() { return m_blipsAdded; }

private:
	//	Encode a message
	void Base64Encode(rage::u8* pDataIn, int sizeIn, char* pDataOut, int sizeOut);

	u8 m_data[MAX_BLIP_MESSAGE_DATA_SIZE];
	u8* m_pData;

	u16 m_currentDataSize;
	u8 m_blipsAdded;

	char m_encodedMessage[1015];
};

//--------------------------------------------------------------------------------------
//	Companion blip node class
//--------------------------------------------------------------------------------------
class CCompanionBlipNode
{
public:
	//	Ctor/Dtor
	CCompanionBlipNode(sCompanionBlip& blip);
	~CCompanionBlipNode();

	//	Update function
	void Update(sCompanionBlip& blip);
	//	Has been updated?
	bool HasBeenUpdated() { return m_updated; }
	//	Set updated
	void SetUpdated(bool updated) { m_updated = updated; }
	//	Is this blip node ready to be deleted?
	bool ReadyToDelete();

	//	Linked list node
	inlist_node<CCompanionBlipNode> m_node;

	sCompanionBlip m_blip;

private:
	bool m_updated;
};

#endif	//	COMPANION_APP