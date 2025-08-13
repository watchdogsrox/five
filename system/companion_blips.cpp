//--------------------------------------------------------------------------------------
// companion_blips.cpp
//--------------------------------------------------------------------------------------
#include "companion.h"
#if COMPANION_APP
#include "companion_blips.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "frontend/PauseMenu.h"

static int companionLanguage = -1;
//--------------------------------------------------------------------------------------
//	Companion blip node class
//--------------------------------------------------------------------------------------
CCompanionBlipNode::CCompanionBlipNode(sCompanionBlip& blip)
{
	//	Store the blip
	m_blip = blip;
	//	Reset updated bool for new blips
	m_updated = true;
}

CCompanionBlipNode::~CCompanionBlipNode()
{

}

void CCompanionBlipNode::Update(sCompanionBlip& blip)
{
	if (m_blip.basicInfo.iconId != blip.basicInfo.iconId || 
		m_blip.basicInfo.x != blip.basicInfo.x || 
		m_blip.basicInfo.y != blip.basicInfo.y || 
		m_blip.basicInfo.rotation != blip.basicInfo.rotation || 
		m_blip.basicInfo.r != blip.basicInfo.r || 
		m_blip.basicInfo.g != blip.basicInfo.g || 
		m_blip.basicInfo.b != blip.basicInfo.b || 
		m_blip.basicInfo.a != blip.basicInfo.a || 
		m_blip.basicInfo.labelSize != blip.basicInfo.labelSize || 
		strcmp(m_blip.label, blip.label) != 0 ||
		m_blip.basicInfo.flags != blip.basicInfo.flags ||
		m_blip.basicInfo.priority != m_blip.basicInfo.priority ||
		m_blip.secondaryColour != blip.secondaryColour ||
		m_blip.scale != blip.scale ||
		m_blip.basicInfo.category != blip.basicInfo.category)
	{
		//if (m_blip.basicInfo.iconId != -1)
		{
			m_blip = blip;
		}
		m_updated = true;
	}
}

bool CCompanionBlipNode::ReadyToDelete()
{
	if (!HasBeenUpdated() && m_blip.basicInfo.iconId == -1)
	{
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------
//	Companion blip message class
//--------------------------------------------------------------------------------------
CCompanionBlipMessage::CCompanionBlipMessage()
{
	//	Initialise the pointer to the data
	m_pData = m_data;
	//	Clear the data
	memset(m_pData, 0, MAX_BLIP_MESSAGE_DATA_SIZE);
	//	Reset the current data size
	m_currentDataSize = 0;

	//	Reset the number of blips added
	m_blipsAdded = 0;
	//	We will reserve the first character for the message type
	m_pData++;
	m_currentDataSize++;
	//	We will reserve the second character for the number of blips
	m_pData++;
	m_currentDataSize++;
	//	We will reserve the third character for the language
	m_pData++;
	m_currentDataSize++;
	//	We will reserve the fourth character for the map display
	m_pData++;
	m_currentDataSize++;
}

CCompanionBlipMessage::~CCompanionBlipMessage()
{

}

bool CCompanionBlipMessage::AddBlip(sCompanionBlip& newBlip)
{
	int blipSize = sizeof(sCompanionBlipBasic) + newBlip.basicInfo.labelSize;

	if (newBlip.basicInfo.flags & COMPANIONBLIP_CREW_INDICATOR)
	{
		blipSize += sizeof(u32);
	}
	else
	if (newBlip.basicInfo.flags & COMPANIONBLIP_TYPE_RADIUS)
	{
		blipSize += sizeof(float);
	}

	//	Only add up to the size limit for this message
	if (m_currentDataSize + blipSize < MAX_BLIP_MESSAGE_DATA_SIZE)
	{
		//	Copy the basic structure into our blip data
		memcpy(m_pData, &newBlip.basicInfo, sizeof(sCompanionBlipBasic));
		//	Move the data along
		m_pData += sizeof(sCompanionBlipBasic);
		m_currentDataSize += sizeof(sCompanionBlipBasic);

		//	Add the label
		memcpy(m_pData, &newBlip.label, newBlip.basicInfo.labelSize);
		//	Move the data along
		m_pData += newBlip.basicInfo.labelSize;
		m_currentDataSize += newBlip.basicInfo.labelSize;

		if (newBlip.basicInfo.flags & COMPANIONBLIP_CREW_INDICATOR)
		{
			memcpy(m_pData, &newBlip.secondaryColour, sizeof(u32));
			m_pData += sizeof(u32);
			m_currentDataSize += sizeof(u32);
		}
		else
		if (newBlip.basicInfo.flags & COMPANIONBLIP_TYPE_RADIUS)
		{
			memcpy(m_pData, &newBlip.scale, sizeof(float));
			m_pData += sizeof(float);
			m_currentDataSize += sizeof(float);
		}

		//	Increment the number of blips
		m_blipsAdded++;

		return true;
	}

	return false;
}

char* CCompanionBlipMessage::GetEncodedMessage()
{
	//	Set the message type
	m_data[0] = MESSAGE_TYPE_BLIPS;
	//	Set the number of blips in the message
	m_data[1] = m_blipsAdded;
	//	Set the language
	if (!CPauseMenu::IsActive() || companionLanguage == -1)
	{
		companionLanguage = CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE);
	}
	m_data[2] = (u8)companionLanguage;

	//	Set the map display
	if (!CCompanionData::GetInstance()->GetIsInPlay())
	{
		//	Show loading screen while in a cutscene
		m_data[3] = 255;
	}
	else
	{
		m_data[3] = (u8)CCompanionData::GetInstance()->GetMapDisplay();
	}
	//	Encode the message
	Base64Encode(m_data, m_currentDataSize, m_encodedMessage, 1009);

	return m_encodedMessage;
}

void CCompanionBlipMessage::ClearMessage()
{
	//	Initialise the pointer to the data
	m_pData = m_data;
	//	Clear the data
	memset(m_pData, 0, MAX_BLIP_MESSAGE_DATA_SIZE);
	//	Reset the current data size
	m_currentDataSize = 0;
	//	Reset the number of blips added
	m_blipsAdded = 0;

	//	We will reserve the first character for message type
	m_pData++;
	m_currentDataSize++;
	//	We will reserve the second character for the number of blips
	m_pData++;
	m_currentDataSize++;
	//	We will reserve the third character for the language
	m_pData++;
	m_currentDataSize++;
	//	We will reserve the fourth character for the map display
	m_pData++;
	m_currentDataSize++;
}

void CCompanionBlipMessage::Base64Encode(rage::u8* pDataIn, int sizeIn, char* pDataOut, int sizeOut)
{
	// 	PF_FUNC(Base64Cost);
	unsigned int charsUsed = 0;

	datBase64::Encode(pDataIn, sizeIn, pDataOut, sizeOut, &charsUsed);
}

#endif	//	COMPANION_APP