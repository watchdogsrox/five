/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : NetworkErrorItem.cpp
// PURPOSE : Used to represent network errors (Not signed in, no cable, etc...)
//
// AUTHOR  : james.strain
// STARTED : June 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "NetworkErrorItem.h"
#if UI_PAGE_DECK_ENABLED
#include "NetworkErrorItem_parser.h"
#include "network/NetworkInterface.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/ui_channel.h"

FWUI_DEFINE_TYPE( CNetworkErrorItem, 0x12C80E8A );

CNetworkErrorItem::CNetworkErrorItem()
    : superclass()
    , m_currentErrorType(CNetworkErrorItem::DEFAULT)
{
}

bool CNetworkErrorItem::UpdateErrorType()
{
    enum eNetworkAccessCode accessCode = NetworkInterface::CheckNetworkAccess(eNetworkAccessArea::AccessArea_Landing);
    eCardErrorType updatedErrorType = m_currentErrorType;

    switch (accessCode)
    {
	case eNetworkAccessCode::Access_Denied_NotSignedIn: { updatedErrorType = CNetworkErrorItem::NOT_SIGNED_IN; } break;
	case eNetworkAccessCode::Access_Denied_NoNetworkAccess: { updatedErrorType = CNetworkErrorItem::NO_LINK_CONNECTION; } break;
	case eNetworkAccessCode::Access_Denied_NotSignedOnline: { updatedErrorType = CNetworkErrorItem::NOT_SIGNED_ONLINE; } break;
	case eNetworkAccessCode::Access_Denied_NoRosPrivilege: { updatedErrorType = CNetworkErrorItem::NO_SCS_PRIVILEGE; } break;
	case eNetworkAccessCode::Access_Denied_NoOnlinePrivilege: { updatedErrorType = CNetworkErrorItem::NO_PERMISSIONS; } break;
    case eNetworkAccessCode::Access_Denied_RosBanned: { updatedErrorType = CNetworkErrorItem::ROS_BANNED; } break;
	case eNetworkAccessCode::Access_Denied_RosSuspended: { updatedErrorType = CNetworkErrorItem::ROS_SUSPENDED; } break;
    default: { updatedErrorType = CNetworkErrorItem::DEFAULT; } break;
    }

    bool const c_changed = m_currentErrorType != updatedErrorType;
    m_currentErrorType = updatedErrorType;
    return c_changed;
}

char const * CNetworkErrorItem::GetItemTitleText() const 
{
    SErrorDetails const& c_detail = GetCurrentErrorDetail();
    return TheText.Get( c_detail.m_title );
}

char const * CNetworkErrorItem::GetDescriptionText() const
{
	SErrorDetails const& c_detail = GetCurrentErrorDetail();

	rtry
	{
		// this isn't my finest hour but needs must
		static bool s_HasBuiltStaticString = false;
		if(m_currentErrorType == ROS_SUSPENDED)
		{
			static char s_Description[MAX_CHARS_IN_MESSAGE];
			if(!s_HasBuiltStaticString) 
			{
				const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
				rcheckall(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex));

				const rlRosCredentials &credentials = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());

				bool granted = false;
				u64 endPosix = 0;
				rcheckall(credentials.HasPrivilegeEndDate(RLROS_PRIVILEGEID_BANNED, &granted, &endPosix));

				static const unsigned MAX_DATE_STRING = 64;
				char dateString[MAX_DATE_STRING];
				CNetwork::FormatPosixTimeAsUntilDate(endPosix, dateString, MAX_DATE_STRING);

				CSubStringWithinMessage subStrings[1];
				subStrings[0].SetCharPointer(dateString);
			
				CMessages::InsertNumbersAndSubStringsIntoString(
					TheText.Get(c_detail.m_description),
					nullptr, 0,
					subStrings, 1,
					s_Description, MAX_CHARS_IN_MESSAGE);
				
				s_HasBuiltStaticString = true; 
			}

			return s_Description;
		}
		else
		{
			// reset this in case it needs updated if we cycle back to this error
			s_HasBuiltStaticString = false;
		}
	}
	rcatchall
	{
		// just use the standard ban message
		return TheText.Get(m_rosBannedError.m_description);
	}

    return TheText.Get( c_detail.m_description );
}

char const * CNetworkErrorItem::GetTooltipText() const 
{
    // Tooltip is optional
    SErrorDetails const& c_detail = GetCurrentErrorDetail();
    return c_detail.m_tooltip.IsNotNull() ? TheText.Get( c_detail.m_tooltip ) : nullptr;
}

CNetworkErrorItem::SErrorDetails const& CNetworkErrorItem::GetCurrentErrorDetail() const
{
    switch( m_currentErrorType )
    {
    case NOT_SIGNED_IN:
        {
            return m_signInError;
        }

    case NO_LINK_CONNECTION:
        {
            return m_connectionLinkError;
        }

    case NOT_SIGNED_ONLINE:
        {
            return m_signOnlineError;
        }

    case NO_SCS_PRIVILEGE:
        {
            return m_scsPrivilegeError;
        }

    case NO_PERMISSIONS:
        {
            return m_permissionsError;
        }

	case ROS_BANNED:
		{
			return m_rosBannedError;
		}

	case ROS_SUSPENDED:
		{
			return m_rosSuspendedError;
		}

    default:
        {
            return m_defaultError;
        }
    }
}

void CNetworkErrorItem::UpdateVisualsDerived(CComplexObject& displayObject)
{
    // If our error type has changed, mark out details as unset so they get refreshed
    bool const c_changed = UpdateErrorType();
    if( c_changed )
    {
        MarkDetailsSet( false );
    }

    superclass::UpdateVisualsDerived( displayObject );
}

#endif // UI_PAGE_DECK_ENABLED
