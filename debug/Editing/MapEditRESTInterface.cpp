#if __BANK

#include "parser/restparserservices.h"

#include "MapEditRESTInterface.h"
#include "MapEditRESTInterface_parser.h"
#include "camera/CamInterface.h"

CMapEditRESTInterface g_MapEditRESTInterface;

void	CMapEditRESTInterface::Init(void)
{
		parRestRegisterSingleton("Scene/OutgoingChanges", g_MapEditRESTInterface, NULL);
		m_bInitialised = true;
}

void	CMapEditRESTInterface::InitIfRequired()
{
	if (!m_bInitialised)
	{
		Init();
	}
}

void	CMapEditRESTInterface::Update()
{
	InitIfRequired();

	m_gameCamera.m_vPos		= VECTOR3_TO_VEC3V( camInterface::GetPos() );
	m_gameCamera.m_vFront	= VECTOR3_TO_VEC3V( camInterface::GetFront() );
	m_gameCamera.m_vUp		= VECTOR3_TO_VEC3V( camInterface::GetUp() );
	m_gameCamera.m_fov		= camInterface::GetFov();
}

#endif	//__BANK
