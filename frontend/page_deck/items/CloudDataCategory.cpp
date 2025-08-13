/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CloudDataCategory.cpp
// PURPOSE : An item category made at least partially of downloaded data
//
/////////////////////////////////////////////////////////////////////////////////
#include "CloudDataCategory.h"

#if UI_PAGE_DECK_ENABLED
#include "CloudDataCategory_parser.h"

// rage
#include "string/stringutil.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/page_deck/items/CloudCardItem.h"
#include "frontend/page_deck/items/uiCloudSupport.h"
#include "frontend/page_deck/PageItemBase.h"
#include "frontend/ui_channel.h"
#include "network/NetworkInterface.h"
#include "network/SocialClubServices/SocialClubFeedTilesMgr.h"

CCloudDataCategory::CCloudDataCategory()
    : superclass()
    , m_requestedOptions(RequestOptions::INVALID)
    , m_requestStatus(RequestStatus::NONE)
    , m_AwaitingTicket(false)
{
}

CCloudDataCategory::~CCloudDataCategory()
{
    ClearCloudData();
}

IPageItemCollection::RequestStatus CCloudDataCategory::RequestItemData(RequestOptions const options)
{
    m_requestStatus = RequestStatus::NONE;

    rtry
    {
        CSocialClubFeedTilesRow* row = uiCloudSupport::GetSource(m_cloudSource);
        rverifyall(row != nullptr);

        int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
        rcheck(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, gnetDebug1("RequestItemData Skipped. Invalid gamer index"));

        const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);

        m_requestStatus = RequestStatus::PENDING;
        m_requestedOptions = options;

        if (cred.IsValid())
        {
            m_AwaitingTicket = false;
            // Always call RenewTiles first. It will be skipped internally if just done
            row->RenewTiles();
        }
        else
        {
            m_AwaitingTicket = true;
            gnetDebug1("RequestItemData RenewTiles delayed. No ticket yet");
        }

        const int DEFAUT_FEED_META_TIMEOUT_MS = 10000;
        int waitMS = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FEED_META_TIMEOUT_MS", 0xEF457E6C), DEFAUT_FEED_META_TIMEOUT_MS);

        m_metaDataTimeout.Clear();
        m_metaDataTimeout.SetLongFrameThreshold(netTimeout::DEFAULT_LONG_FRAME_THRESHOLD_MS);
        m_metaDataTimeout.InitMilliseconds(waitMS);

        // The update does all we need for the initial check as well
        UpdateRequest();

        return m_requestStatus;
    }
    rcatchall
    {
    }

    Failed();
    return m_requestStatus;
}

IPageItemCollection::RequestStatus CCloudDataCategory::UpdateRequest()
{
    CSocialClubFeedTilesRow* row = uiCloudSupport::GetSource(m_cloudSource);
    if (row == nullptr)
    {
        if (m_requestStatus == RequestStatus::PENDING || m_requestStatus == RequestStatus::PENDING_RICH_DATA)
        {
            Failed();
        }

        return GetRequestStatus();
    }

    const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
    const eNetworkAccessCode access = NetworkInterface::CheckNetworkAccess(eNetworkAccessArea::AccessArea_Landing);

    // On any page, if the Landing network check is valid but we have no access we can early out
    if (NetworkInterface::IsReadyToCheckNetworkAccess(eNetworkAccessArea::AccessArea_Landing) && access != eNetworkAccessCode::Access_Granted)
    {
        gnetDebug1("CCloudDataCategory no network access. AwaitingTicket: %s", LogBool(m_AwaitingTicket));
        Failed();

        return GetRequestStatus();
    }

    const TilesMetaDownloadState metaState = row->GetMetaDownloadState();
    const TilesImagesDownloadState richDataState = row->GetUIPreloadState();

    m_metaDataTimeout.Update();

    // If we timed out and have the metadata we show the page. If we have no metadata we fall back to the backup page.
    if (m_metaDataTimeout.IsTimedOut())
    {
        if (metaState == TilesMetaDownloadState::MetaReady)
        {
            gnetDebug1("CCloudDataCategory image download timeout. AwaitingTicket: %s", LogBool(m_AwaitingTicket));
            Succeeded();
        }
        else
        {
            gnetWarning("CCloudDataCategory meta download timeout. AwaitingTicket: %s", LogBool(m_AwaitingTicket));
            Failed();
        }

        return GetRequestStatus();
    }

    // If we're still waiting for the ticket check that first. Without it the other checks won't matter.
    if (m_AwaitingTicket)
    {
        if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && rlRos::GetCredentials(localGamerIndex).IsValid())
        {
            m_AwaitingTicket = false;
            row->RenewTiles();
        }

        return GetRequestStatus();
    }

    switch (m_requestStatus)
    {
    case RequestStatus::SUCCESS:
        {
            if( m_cloudItems.GetCount() != 0 )
            {
                // Skip if we've populated the items already, otherwise fall through as if pending
                break;
            }
        } 
    case RequestStatus::PENDING:
        {
            if (metaState == TilesMetaDownloadState::DownloadingMeta)
            {
            }
            else if (metaState == TilesMetaDownloadState::MetaReady)
            {
                ClearCloudData();

                if (!FillData(*row))
                {
                    Failed();
                }
                else if (((int)m_requestedOptions & (int)RequestOptions::RICH_DATA) != 0)
                {
                    LoadRichData(*row);
                    m_requestStatus = RequestStatus::PENDING_RICH_DATA;
                }
                else
                {
                    Succeeded();
                }
            }
            else
            {
                Failed();
            }
        } break;
    case RequestStatus::PENDING_RICH_DATA:
        {
            if (richDataState == TilesImagesDownloadState::DownloadingBasics || richDataState == TilesImagesDownloadState::DownloadingMore)
            {
            }
            else if (richDataState == TilesImagesDownloadState::Ready)
            {
                // Clear the rich data flag so we don't re-request it if we refresh the page
                m_requestedOptions = RequestOptions::INVALID;
                Succeeded();
            }
            else
            {
                Failed();
            }
        } break;
    case RequestStatus::FAILED:
        {
        } break;
    default:
        {
            gnetError("[%s]  CCloudDataCategory - Unexpected request state %u", m_cloudSource.TryGetCStr(), (unsigned)m_requestStatus);
            Failed();
        } break;
    }

    return GetRequestStatus();
}

IPageItemCollection::RequestStatus CCloudDataCategory::MarkAsTimedOut()
{
    if (m_requestStatus == RequestStatus::SUCCESS || m_requestStatus == RequestStatus::FAILED)
    {
        gnetWarning("[%s] CCloudDataCategory - Timeout even though we're finished. state %u", m_cloudSource.TryGetCStr(), (unsigned)m_requestStatus);
    }
    else if (m_requestStatus == RequestStatus::PENDING_RICH_DATA)
    {
        gnetDebug1("[%s] CCloudDataCategory - Timeout but we have enough data. state %u", m_cloudSource.TryGetCStr(), (unsigned)m_requestStatus);
        Succeeded();
    }
    else
    {
        gnetDebug1("[%s] CCloudDataCategory - Timeout. Use fallback data. state %u", m_cloudSource.TryGetCStr(), (unsigned)m_requestStatus);
        Failed();
    }

    return m_requestStatus;
}

IPageItemCollection::RequestStatus CCloudDataCategory::GetRequestStatus() const
{
    return m_requestStatus;
}

void CCloudDataCategory::AddCloudItem(CPageItemBase* item)
{
    if (item)
    {
        m_cloudItems.PushAndGrow(item);
    }
}

void CCloudDataCategory::ClearCloudData()
{
    for (ItemIterator itr = m_cloudItems.begin(); itr != m_cloudItems.end(); ++itr)
    {
        CPageItemBase* item = *itr;

        // If it's not a CCloudCardItem we didn't create it and it should be part of m_items
        // so it will be deleted there. A bit easy to overlook when making changes but
        // should work for all our current needs
        if (item != nullptr && item->GetIsClass<CCloudCardItem>())
        {
            delete item;
        }
    }

    m_cloudItems.ResetCount();
}

void CCloudDataCategory::Failed()
{
    gnetError("[%s] CCloudDataCategory - Failed", m_cloudSource.TryGetCStr());
    // The failed state isn't handled so we always set success for now. The user
    // will currently see an empty page or a fallback page
    m_requestStatus = RequestStatus::SUCCESS;
    m_AwaitingTicket = false;
    OnFailed();
}

void CCloudDataCategory::Succeeded()
{
    gnetDebug1("[%s] CCloudDataCategory - Succeeded", m_cloudSource.TryGetCStr());
    m_requestStatus = RequestStatus::SUCCESS;
    m_AwaitingTicket = false;
    OnSucceeded();
}

bool CCloudDataCategory::FillData(CSocialClubFeedTilesRow& row)
{
    const unsigned rowIdx = 0;

    for (unsigned i = 0; i < ScFeedTile::MAX_CAROUSEL_ITEMS; ++i)
    {
        const ScFeedTile* tile = row.GetTile(i, rowIdx);

        if (tile && !uiCloudSupport::CanBeShown(tile->m_Feed))
        {
            continue;
        }

        CCloudCardItem* ci = (tile != nullptr) ? rage_new CCloudCardItem() : nullptr;

        if (ci != nullptr)
        {
            uiCloudSupport::FillCarouselCard(*ci, tile->m_Feed);
            ci->SetTrackingData(-1, ScFeedPost::GetDlinkTrackingId(tile->m_Feed));
            AddCloudItem(ci);
        }
    }

    return true;
}

bool CCloudDataCategory::LoadRichData(CSocialClubFeedTilesRow& row)
{
    //TODO_ANDI: it would be better to only load and wait for the data visible on the 'first' page
    row.PreloadUI(1, ScFeedTile::MAX_CAROUSEL_ITEMS);
    return true;
}

void CCloudDataCategory::OnFailed()
{
}

void CCloudDataCategory::OnSucceeded()
{
}

void CCloudDataCategory::ForEachItemDerived(superclass::PerItemLambda action)
{
    for (ItemIterator itr = m_cloudItems.begin(); itr != m_cloudItems.end(); ++itr)
    {
        if (*itr)
        {
            action(**itr);
        }
    }
}

void CCloudDataCategory::ForEachItemDerived(superclass::PerItemConstLambda action) const
{
    for (ConstItemIterator itr = m_cloudItems.begin(); itr != m_cloudItems.end(); ++itr)
    {
        if (*itr)
        {
            action(**itr);
        }
    }
}

#endif // UI_PAGE_DECK_ENABLED
