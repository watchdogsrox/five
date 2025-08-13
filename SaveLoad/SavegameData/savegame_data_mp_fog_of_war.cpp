
#include "savegame_data_mp_fog_of_war.h"
#include "savegame_data_mp_fog_of_war_parser.h"


// Game headers
#include "frontend/MiniMapCommon.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "script/script_hud.h"



#if SAVE_MULTIPLAYER_FOG_OF_WAR

// *************************** Island Heist Fog of War MP ************************************************

CMultiplayerFogOfWarSaveStructure::CMultiplayerFogOfWarSaveStructure()
{
	m_FogOfWarValues.Reset();

	m_MinX = 0;
	m_MinY = 0;
	m_MaxX = 0;
	m_MaxY = 0;
	m_FillValueForRestOfMap = 0;
	m_Enabled = false;
}

CMultiplayerFogOfWarSaveStructure::~CMultiplayerFogOfWarSaveStructure()
{
	m_FogOfWarValues.Reset();
}

void CMultiplayerFogOfWarSaveStructure::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CMultiplayerFogOfWarSaveStructure::PreSave"))
	{
		savegameAssertf(0, "CMultiplayerFogOfWarSaveStructure::PreSave - didn't expect this MP function to get called during the import/export of an SP savegame");
		return;
	}

#if ENABLE_FOG_OF_WAR
	m_Enabled = CScriptHud::GetMultiplayerFogOfWarSavegameDetails().Get(m_MinX, m_MinY, m_MaxX, m_MaxY, m_FillValueForRestOfMap);
	if (m_Enabled)
	{
		if ( (savegameMpFowVerifyf(m_MinX <= m_MaxX, "CMultiplayerFogOfWarSaveStructure::PreSave - expected m_MinX(%u) to be <= m_MaxX(%u)", m_MinX, m_MaxX))
			&& (savegameMpFowVerifyf(m_MinY <= m_MaxY, "CMultiplayerFogOfWarSaveStructure::PreSave - expected m_MinY(%u) to be <= m_MaxY(%u)", m_MinY, m_MaxY))
			&& (savegameMpFowVerifyf(m_MaxX < FOG_OF_WAR_RT_SIZE, "CMultiplayerFogOfWarSaveStructure::PreSave - expected m_MaxX(%u) to be < %u", m_MaxX, FOG_OF_WAR_RT_SIZE))
			&& (savegameMpFowVerifyf(m_MaxY < FOG_OF_WAR_RT_SIZE, "CMultiplayerFogOfWarSaveStructure::PreSave - expected m_MaxY(%u) to be < %u", m_MaxY, FOG_OF_WAR_RT_SIZE)) )
		{
			u8* fogOfWarLock = CMiniMap_Common::GetFogOfWarLockPtr();
			if (savegameMpFowVerifyf(fogOfWarLock, "CMultiplayerFogOfWarSaveStructure::PreSave - CMiniMap_Common::GetFogOfWarLockPtr() returned a NULL pointer"))
			{
				const u32 width = m_MaxX - m_MinX + 1;
				const u32 height = m_MaxY - m_MinY + 1;
				m_FogOfWarValues.Resize(width*height);

				savegameMpFowDebugf1("CMultiplayerFogOfWarSaveStructure::PreSave - saving %u u8 values in m_FogOfWarValues array. width=%u height=%u MinX=%u MaxX=%u MinY=%u MaxY=%u",
					(width*height), width, height, m_MinX, m_MaxX, m_MinY, m_MaxY);

				u32 writeIndex = 0;
				for (u32 row = m_MinY; row <= m_MaxY; row++)
				{
					for (u32 column = m_MinX; column <= m_MaxX; column++)
					{
						m_FogOfWarValues[writeIndex] = fogOfWarLock[(row*FOG_OF_WAR_RT_SIZE)+column];
						savegameMpFowDebugf2("CMultiplayerFogOfWarSaveStructure::PreSave - m_FogOfWarValues[%u]=%u (row=%u, column=%u)", 
							writeIndex, m_FogOfWarValues[writeIndex], row, column);
						writeIndex++;
					}
				}
			}
		}
	}
#endif	//	ENABLE_FOG_OF_WAR
}

void CMultiplayerFogOfWarSaveStructure::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CMultiplayerFogOfWarSaveStructure::PostLoad"))
	{
		savegameAssertf(0, "CMultiplayerFogOfWarSaveStructure::PostLoad - didn't expect this MP function to get called during the import/export of an SP savegame");
		return;
	}

#if ENABLE_FOG_OF_WAR
	savegameMpFowDebugf1("CMultiplayerFogOfWarSaveStructure::PostLoad - m_Enabled=%s m_MinX=%u m_MinY=%u m_MaxX=%u m_MaxY=%u m_FillValueForRestOfMap=%u",
		m_Enabled?"true":"false",
		m_MinX, m_MinY, m_MaxX, m_MaxY, m_FillValueForRestOfMap);

	CScriptHud::GetMultiplayerFogOfWarSavegameDetails().Set(m_Enabled, m_MinX, m_MinY, m_MaxX, m_MaxY, m_FillValueForRestOfMap);

	if (m_Enabled)
	{
		u8* fogOfWarLock = CMiniMap_Common::GetFogOfWarLockPtr(); 
		if (savegameMpFowVerifyf(fogOfWarLock, "CMultiplayerFogOfWarSaveStructure::PostLoad - CMiniMap_Common::GetFogOfWarLockPtr() returned a NULL pointer"))
		{
			memset(fogOfWarLock,m_FillValueForRestOfMap,FOG_OF_WAR_RT_SIZE*FOG_OF_WAR_RT_SIZE);

			if ( (savegameMpFowVerifyf(m_MinX <= m_MaxX, "CMultiplayerFogOfWarSaveStructure::PostLoad - expected m_MinX(%u) to be <= m_MaxX(%u)", m_MinX, m_MaxX))
				&& (savegameMpFowVerifyf(m_MinY <= m_MaxY, "CMultiplayerFogOfWarSaveStructure::PostLoad - expected m_MinY(%u) to be <= m_MaxY(%u)", m_MinY, m_MaxY))
				&& (savegameMpFowVerifyf(m_MaxX < FOG_OF_WAR_RT_SIZE, "CMultiplayerFogOfWarSaveStructure::PostLoad - expected m_MaxX(%u) to be < %u", m_MaxX, FOG_OF_WAR_RT_SIZE))
				&& (savegameMpFowVerifyf(m_MaxY < FOG_OF_WAR_RT_SIZE, "CMultiplayerFogOfWarSaveStructure::PostLoad - expected m_MaxY(%u) to be < %u", m_MaxY, FOG_OF_WAR_RT_SIZE)) )
			{
				u32 readIndex = 0;
				for (u32 row = m_MinY; row <= m_MaxY; row++)
				{
					for (u32 column = m_MinX; column <= m_MaxX; column++)
					{
						fogOfWarLock[(row*FOG_OF_WAR_RT_SIZE)+column] = m_FogOfWarValues[readIndex];
						savegameMpFowDebugf2("CMultiplayerFogOfWarSaveStructure::PostLoad - m_FogOfWarValues[%u]=%u (row=%u, column=%u)", 
							readIndex, m_FogOfWarValues[readIndex], row, column);
						readIndex++;
					}
				}
			}

			// I copied the following lines from CSimpleVariablesSaveStructure::PostLoad()
			CMiniMap::SetRequestFoWClear(false);
			CMiniMap::SetFoWMapValid(true);
#if __D3D11
			CMiniMap::SetUploadFoWTextureData(true);
#endif
		}
	}

#endif	//	ENABLE_FOG_OF_WAR
}
#endif	//	SAVE_MULTIPLAYER_FOG_OF_WAR


