#include "uiCloudSupport.h"

// framework
#include "fwscene/stores/txdstore.h"

// game
#include "control/ScriptRouterContext.h"
#include "control/ScriptRouterLink.h"
#include "frontend/landing_page/messages/uiLandingPageMessages.h"
#include "frontend/page_deck/items/CardItem.h"
#include "frontend/page_deck/items/CloudCardItem.h"
#include "network/live/NetworkLegalRestrictionsManager.h"
#include "network/SocialClubServices/SocialClubFeedTilesMgr.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"
#include "network/cloud/Tunables.h"

#if GEN9_LANDING_PAGE_ENABLED

namespace uiCloudSupport
{

struct StickerBuild
{
    atHashString m_StickerId;
    IItemStickerInterface::StickerType m_StickerType;
    atHashString m_StickerText;
};

static StickerBuild s_CloudStickers[] =
{
    { ATSTRINGHASH("p_live", 0x8DDC70B6),           IItemStickerInterface::StickerType::Primary,             ATSTRINGHASH("UI_STICKER_LIVE", 0x52E23026) },
    { ATSTRINGHASH("p_sale", 0x866DC26A),           IItemStickerInterface::StickerType::Primary,             ATSTRINGHASH("UI_STICKER_SALE", 0x3B6F5A38) },
    { ATSTRINGHASH("p_expire", 0x6E0E1E7C),         IItemStickerInterface::StickerType::Primary,             ATSTRINGHASH("UI_STICKER_EXPIRES_SOON", 0x2FAE36C7) },
    { ATSTRINGHASH("p_limited_offer", 0x8CFA0616),  IItemStickerInterface::StickerType::Primary,             ATSTRINGHASH("UI_STICKER_LIMITED_TIME_OFFER", 0xCAD89055) },
    { ATSTRINGHASH("p_25off", 0xAEBFD04C),          IItemStickerInterface::StickerType::PrimarySale,         ATSTRINGHASH("UI_STICKER_25OFF", 0xD4034468) },
    { ATSTRINGHASH("p_30off", 0x82C9B4D1),          IItemStickerInterface::StickerType::PrimarySale,         ATSTRINGHASH("UI_STICKER_30OFF", 0x598F6461) },
    { ATSTRINGHASH("p_40off", 0x4A0F12B0),          IItemStickerInterface::StickerType::PrimarySale,         ATSTRINGHASH("UI_STICKER_40OFF", 0xE9ADA7D0) },
    { ATSTRINGHASH("p_50off", 0x7D7A3549),          IItemStickerInterface::StickerType::PrimarySale,         ATSTRINGHASH("UI_STICKER_50OFF", 0xC12C8158) },
    { ATSTRINGHASH("p_60off", 0x187FA74C),          IItemStickerInterface::StickerType::PrimarySale,         ATSTRINGHASH("UI_STICKER_60OFF", 0x5E256C60) },
    { ATSTRINGHASH("p_75off", 0x3CCDA5A6),          IItemStickerInterface::StickerType::PrimarySale,         ATSTRINGHASH("UI_STICKER_75OFF", 0x51020F1E) },
    { ATSTRINGHASH("p_80off", 0x78A9C40E),          IItemStickerInterface::StickerType::PrimarySale,         ATSTRINGHASH("UI_STICKER_80OFF", 0xBE7DAF8D) },
    { ATSTRINGHASH("p_out", 0x3C2F4B18),            IItemStickerInterface::StickerType::Primary,             ATSTRINGHASH("UI_STICKER_OUT_NOW", 0x6CD5E5B8) },
    { ATSTRINGHASH("p_available", 0xB475A92),       IItemStickerInterface::StickerType::Primary,             ATSTRINGHASH("UI_STICKER_NOW_AVAILABLE", 0x93AF294) },
    { ATSTRINGHASH("p_limited", 0xA797C621),        IItemStickerInterface::StickerType::Primary,             ATSTRINGHASH("UI_STICKER_LIMITED_TIME", 0xFE5D76D5) },
    { ATSTRINGHASH("s_new", 0xA8263A12),            IItemStickerInterface::StickerType::Secondary,           ATSTRINGHASH("UI_STICKER_NEW", 0xB56EC9E) },
    { ATSTRINGHASH("s_free", 0x81EA10E3),           IItemStickerInterface::StickerType::Secondary,           ATSTRINGHASH("UI_STICKER_FREE", 0x945A451A) },
    { ATSTRINGHASH("s_exclusive", 0xC82036FB),      IItemStickerInterface::StickerType::Secondary,           ATSTRINGHASH("UI_STICKER_EXCLUSIVE_OFFER", 0xBCD42451) },
    { ATSTRINGHASH("s_just", 0x29262088),           IItemStickerInterface::StickerType::Secondary,           ATSTRINGHASH("UI_STICKER_JUST_FOR_YOU", 0x405085C3) },
    { ATSTRINGHASH("s_choose", 0x70D32EF2),         IItemStickerInterface::StickerType::Secondary,           ATSTRINGHASH("UI_STICKER_CHOOSE_EITHER", 0x3C454D23) },
    { ATSTRINGHASH("s_choose_one", 0x799EFAD3),     IItemStickerInterface::StickerType::Secondary,           ATSTRINGHASH("UI_STICKER_CHOOSE_ONE", 0x478FAF05) },
    { ATSTRINGHASH("s_claim", 0xA60151FE),          IItemStickerInterface::StickerType::Secondary,           ATSTRINGHASH("UI_STICKER_CLAIM_FOR_FREE", 0xE7C7769A) },
    { ATSTRINGHASH("s_gift", 0x980992BF),           IItemStickerInterface::StickerType::SecondaryGift,       ATSTRINGHASH("UI_STICKER_BONUS_GIFT", 0xA827BBB9) },
    { ATSTRINGHASH("s_members", 0xCF42D272),        IItemStickerInterface::StickerType::Secondary,           ATSTRINGHASH("UI_STICKER_MEMBERS", 0x47C29D79) },
    { ATSTRINGHASH("t_bonus", 0x28E6D1EF),          IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_BONUS_PAYMENTS", 0x49422D0A) },
    { ATSTRINGHASH("t_coming", 0xA490317B),         IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_COMING_SOON", 0x27C6FC3E) },
    { ATSTRINGHASH("t_2xreward", 0x54232304),       IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_2X_REWARDS", 0x3079D9CF) },
    { ATSTRINGHASH("t_3xreward", 0x3E2DE5F8),       IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_3X_REWARDS", 0x9BB9AA4E) },
    { ATSTRINGHASH("t_2xrp", 0xA6BC5323),           IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_2X_RP", 0xC0A0834B) },
    { ATSTRINGHASH("t_3xrp", 0x9E6219AC),           IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_3X_RP", 0x69ED7F76) },
    { ATSTRINGHASH("t_2xcash", 0x580799FD),         IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_2X_CASH", 0x1A4B1759) },
    { ATSTRINGHASH("t_3xcash", 0xB1A33363),         IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_3X_CASH", 0x384819F3) },
    { ATSTRINGHASH("t_freeheist", 0xB9692412),      IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_FREE_HEIST", 0xDF39471B) },
    { ATSTRINGHASH("t_diamonds", 0xBCB60ED9),       IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_GUARANTEED_DIAMONDS", 0xA66CFF12) },
    { ATSTRINGHASH("t_panther", 0x94CEAF9),         IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_GUARANTEED_STATUE", 0x27016901) },
    { ATSTRINGHASH("t_rfeatured", 0x5D230573),      IItemStickerInterface::StickerType::TertiaryRockstar,    ATSTRINGHASH("UI_STICKER_R_FEATURED", 0xC4F83491) },
    { ATSTRINGHASH("ps_gift", 0xDA417CF1),          IItemStickerInterface::StickerType::PlatformSpecific,    ATSTRINGHASH("UI_STICKER_SPECIAL_GIFT", 0xF74801F8) },
    { ATSTRINGHASH("ps_discount", 0xFE58B83A),      IItemStickerInterface::StickerType::PlatformSpecific,    ATSTRINGHASH("UI_STICKER_SPECIAL_DISCOUNT", 0x9D87EA7E) },
    { ATSTRINGHASH("tw_gift", 0x1A0D2D1C),          IItemStickerInterface::StickerType::Twitch,              ATSTRINGHASH("UI_STICKER_SPECIAL_GIFT", 0xF74801F8) },
    { ATSTRINGHASH("tw_discount", 0xA7F21935),      IItemStickerInterface::StickerType::Twitch,              ATSTRINGHASH("UI_STICKER_SPECIAL_DISCOUNT", 0x9D87EA7E) },
    { ATSTRINGHASH("c_redeem", 0x6B217BE2),         IItemStickerInterface::StickerType::TickText,            ATSTRINGHASH("UI_STICKER_REDEEM_IN_GAME", 0xA4B5D54B) },
    { ATSTRINGHASH("c_wardrobe", 0x9A4B340F),       IItemStickerInterface::StickerType::TickText,            ATSTRINGHASH("UI_STICKER_ADDED_TO_WARDROBE", 0x40465AA7) },
    { ATSTRINGHASH("c_playtoget", 0x9AAC5813),      IItemStickerInterface::StickerType::Tertiary,            ATSTRINGHASH("UI_STICKER_PLAY_TO_GET", 0x930E50CD) },
    { ATSTRINGHASH("c_added", 0x163E6D34),          IItemStickerInterface::StickerType::TickText,            ATSTRINGHASH("UI_STICKER_ADDED", 0x826DE59) },
    { ATSTRINGHASH("c_tick", 0x382D68DD),           IItemStickerInterface::StickerType::Tick,                atHashString() },
    { ATSTRINGHASH("u_unavailable", 0x9ABFD04C),    IItemStickerInterface::StickerType::TickText,            ATSTRINGHASH("UI_STICKER_UNAVAILABLE", 0xE851F3AD) },
    { ATSTRINGHASH("u_lock", 0x1DDB9E0B),           IItemStickerInterface::StickerType::Lock,                atHashString() },
    { ATSTRINGHASH("p_custom", 0xFE2BDED9),         IItemStickerInterface::StickerType::Primary,             atHashString() },
    { ATSTRINGHASH("s_custom", 0x6CAC1AB2),         IItemStickerInterface::StickerType::Secondary,           atHashString() },
    { ATSTRINGHASH("t_custom", 0xC8A4CEDF),         IItemStickerInterface::StickerType::Tertiary,            atHashString() },
    { ATSTRINGHASH("ps_custom", 0xF9E8EC95),        IItemStickerInterface::StickerType::PlatformSpecific,    atHashString() },
    { ATSTRINGHASH("tw_custom", 0xDFA003E2),        IItemStickerInterface::StickerType::Twitch,              atHashString() }
};

static LayoutMapping s_LayoutMapping[(int)CLandingOnlineLayout::LAYOUT_COUNT] =
{
    {1, {LayoutPos(0, 0),  LayoutPos(-1,-1),  LayoutPos(-1,-1),  LayoutPos(-1,-1)}}, //LAYOUT_ONE_PRIMARY
    {2, {LayoutPos(0, 0),  LayoutPos( 1, 0),  LayoutPos(-1,-1),  LayoutPos(-1,-1)}}, //LAYOUT_TWO_PRIMARY
    {3, {LayoutPos(0, 0),  LayoutPos( 1, 0),  LayoutPos( 1, 2),  LayoutPos(-1,-1)}}, //LAYOUT_ONE_PRIMARY_TWO_SUB
    {3, {LayoutPos(0, 0),  LayoutPos( 0, 2),  LayoutPos( 1, 2),  LayoutPos(-1,-1)}}, //LAYOUT_ONE_WIDE_TWO_SUB
    {4, {LayoutPos(0, 0),  LayoutPos( 0, 2),  LayoutPos( 1, 2),  LayoutPos( 1, 3)}}, //LAYOUT_ONE_WIDE_ONE_SUB_TWO_MINI
    {4, {LayoutPos(0, 0),  LayoutPos( 0, 2),  LayoutPos( 0, 3),  LayoutPos( 1, 2)}}, //LAYOUT_ONE_WIDE_TWO_MINI_ONE_SUB
    {4, {LayoutPos(0, 0),  LayoutPos( 1, 0),  LayoutPos( 0, 2),  LayoutPos( 1, 2)}}, //LAYOUT_FOUR_SUB
    {1, {LayoutPos(0, 0),  LayoutPos(-1,-1),  LayoutPos(-1,-1),  LayoutPos(-1,-1)}}  //LAYOUT_FULLSCREEN
};

bool FillLandingPageLayout(CLandingOnlineLayout::eLayoutType& layout, unsigned& layoutPublishId)
{
    #define LOWEST_LAYOUT_PRIORITY 10

    int bestPriority = LOWEST_LAYOUT_PRIORITY + 1;
    bool found = false;

    rtry
    {
        CSocialClubFeedLanding& ld = CSocialClubFeedTilesMgr::Get().Landing();
        rcheckall(ld.GetMetaDownloadState() == TilesMetaDownloadState::MetaReady);

        const ScFeedTileRow& row = ld.GetTiles();
        const int num = row.GetCount();

        // Pick the first valid layout with the highest priority; which is the one with the lowest number.
        // If posts have a valid "lpid" we group them by that value and ensure all needed ones are published.
        // Posts without a layout value can be used as long as they fit into the tested layout from another post.
        // This makes it much easier to manage data on ScAdmin, especially in dev.

        for (int i = 0; i < num; ++i)
        {
            const ScFeedTile* tile = row[i];
            if (tile == nullptr)
            {
                continue;
            }

            const ScFeedPost& post = tile->m_Feed;

            const ScFeedParam* param = post.GetParam(ScFeedParam::LAYOUT_TYPE_ID);
            const atHashString lay = param ? param->ValueAsHash() : atHashString();

            if (lay == atHashString())
            {
                continue;
            }

            const ScFeedParam* lpidParam = post.GetParam(ScFeedParam::LAYOUT_PUBLISH_ID);
            const unsigned lipd = lpidParam ? (unsigned)lpidParam->ValueAsInt64() : 0;

            const ScFeedParam* lpriorityParam = post.GetParam(ScFeedParam::LAYOUT_PRIORITY_ID);
            int lpriority = (lpriorityParam && lpriorityParam->m_Value.length() > 0) ? (int)lpriorityParam->ValueAsInt64() : LOWEST_LAYOUT_PRIORITY;

            CLandingOnlineLayout::eLayoutType tempLayout = CLandingOnlineLayout::LAYOUT_COUNT;

            if (lay == ATSTRINGHASH("1prim", 0xDEA3C1EA)) { tempLayout = CLandingOnlineLayout::LAYOUT_ONE_PRIMARY; }
            else if (lay == ATSTRINGHASH("2prim", 0x265FF395)) { tempLayout = CLandingOnlineLayout::LAYOUT_TWO_PRIMARY; }
            else if (lay == ATSTRINGHASH("1prim2sub", 0xC8541FC5)) { tempLayout = CLandingOnlineLayout::LAYOUT_ONE_PRIMARY_TWO_SUB; }
            else if (lay == ATSTRINGHASH("1wide2sub", 0x30AC065C)) { tempLayout = CLandingOnlineLayout::LAYOUT_ONE_WIDE_TWO_SUB; }
            else if (lay == ATSTRINGHASH("1wide1sub2mini", 0xFECADAC6)) { tempLayout = CLandingOnlineLayout::LAYOUT_ONE_WIDE_ONE_SUB_TWO_MINI; } // Doesn't exist on ScAdmin but here for consistency.
            else if (lay == ATSTRINGHASH("1wide2mini1sub", 0x71804F0B)) { tempLayout = CLandingOnlineLayout::LAYOUT_ONE_WIDE_TWO_MINI_ONE_SUB; }
            else if (lay == ATSTRINGHASH("4sub", 0x66455EC6)) { tempLayout = CLandingOnlineLayout::LAYOUT_FOUR_SUB; }
            else if (lay == ATSTRINGHASH("1full", 0xF59044EC)) { tempLayout = CLandingOnlineLayout::LAYOUT_FULLSCREEN; } // Doesn't exist on ScAdmin but here for consistency.

            // If the priority is the same the first one in the list must win
            if (gnetVerifyf(tempLayout != CLandingOnlineLayout::LAYOUT_COUNT, "Unsupported layout %s", param->m_Value.c_str()) &&
                gnetVerify(VerifyLandingPageLayout(tempLayout, lipd, ld)) && lpriority < bestPriority)
            {
                layout = tempLayout;
                layoutPublishId = lipd;
                bestPriority = lpriority;
                found = true;
            }
        }
    }
    rcatchall
    {
    }

    return found;
}

bool VerifyLandingPageLayout(CLandingOnlineLayout::eLayoutType layout, const unsigned layoutPublishId, const CSocialClubFeedTilesRow& row)
{
    rtry
    {
#if !__NO_OUTPUT
        const ScFeedTile* tile = row.GetTiles().GetCount() > 0 ? row.GetTiles()[0] : nullptr;
        const ScFeedParam* param = tile ? tile->m_Feed.GetParam(ScFeedParam::LAYOUT_TYPE_ID): nullptr;
        const char* layoutname = param ? param->m_Value.c_str() : "undef";
#endif
        const LayoutMapping* mapping = GetLayoutMapping(layout);
        rcheck(mapping != nullptr, catchall, gnetError("Layout mapping for %s not found", layoutname));

        for (int i = 0; i < mapping->m_numCards; ++i)
        {
            const LayoutPos& pos = mapping->m_pos[i];
            rcheck(row.GetTile(pos.m_x, pos.m_y, layoutPublishId) != nullptr, catchall, gnetError("%s - (%d,%d) missing", layoutname, pos.m_x, pos.m_y));
        }

        return true;
    }
    rcatchall
    {
    }

    return false;
}

const LayoutMapping* GetLayoutMapping(CLandingOnlineLayout::eLayoutType layout)
{
    const int layoutIdx = static_cast<int>(layout);

    if (!gnetVerifyf(layoutIdx >= 0 && layoutIdx < (int)CLandingOnlineLayout::eLayoutType::LAYOUT_COUNT, "Invalid layout index"))
    {
        return nullptr;
    }

    return &s_LayoutMapping[layoutIdx];
}

bool FillLandingPageCard(CCloudCardItem& dest, const int column, const int row, const unsigned layoutPublishId)
{
    rtry
    {
        CSocialClubFeedLanding& ld = CSocialClubFeedTilesMgr::Get().Landing();
        rcheckall(ld.GetMetaDownloadState() == TilesMetaDownloadState::MetaReady);

        const ScFeedTile* tile = ld.GetTile(column, row, layoutPublishId);
        rcheckall(tile != nullptr);
        const ScFeedPost& post = tile->m_Feed;

        FillLandingPageCard(dest, post);
        return true;
    }
    rcatchall
    {
    }

    return false;
}

void FillLandingPageCard(CCloudCardItem& dest, const ScFeedPost& post)
{
    if (!CanBeShown(post))
    {
        FillEmptyCard(dest);
        return;
    }

    dest.SetGuid(post.GetId());
    FillText(dest.Title(), ScFeedLoc::TITLE_ID, post);
    FillText(dest.Description(), ScFeedLoc::BODY_ID, post);
    FillText(dest.Tooltip(), ScFeedLoc::FOOTER_ID, post);

    FillImage(dest.Texture(), dest.Txd(), ScFeedLoc::IMAGE_ID, post);
    FillSticker(dest.StickerType(), dest.StickerText(), post);

    uiPageDeckMessageBase* link = CreateLink(post);
    dest.SetOnSelectMessage(link);
}

void FillEmptyCard(CCloudCardItem& dest)
{
    dest.SetGuid(RosFeedGuid::GenerateGuid());
    dest.Title() = "";
    dest.TitleBig() = "";
    dest.Description() = "";
    dest.StickerText() = "";
    dest.Tooltip() = "";
    dest.Texture() = "";
    dest.FeaturedTexture() = "";
    dest.DestroyOnSelectMessage();
}

void FillCarouselCard(CCloudCardItem& dest, const ScFeedPost& post)
{
    if (!CanBeShown(post))
    {
        gnetWarning("FillCarouselCard called even though the card can't be shown");
        FillEmptyCard(dest);
        return;
    }

    const atString* bigtitle = post.GetLoc(ScFeedLoc::BIGTITLE_ID);

    dest.SetGuid(post.GetId());
    FillText(dest.Title(), ScFeedLoc::TITLE_ID, post);
    FillText(dest.TitleBig(), (bigtitle != nullptr && bigtitle->length() > 0) ? ScFeedLoc::BIGTITLE_ID : ScFeedLoc::TITLE_ID, post);
    FillText(dest.Description(), ScFeedLoc::BODY_ID, post);
    FillText(dest.Tooltip(), ScFeedLoc::FOOTER_ID, post);

    FillImage(dest.Texture(), dest.Txd(), ScFeedLoc::IMAGE_ID, post);
    FillImage(dest.FeaturedTexture(), dest.FeaturedTxd(), ScFeedLoc::BIGIMAGE_ID, post);
    FillSticker(dest.StickerType(), dest.StickerText(), post);

    uiPageDeckMessageBase* link = CreateLink(post);
    dest.SetOnSelectMessage(link);
}

CSocialClubFeedTilesRow* GetSource(const atHashString id)
{
    return CSocialClubFeedTilesMgr::Get().Row(id);
}

uiPageDeckMessageBase* CreateLink(const ScFeedPost& post, atHashString* linkMetricType)
{
    const ScFeedParam* param = post.GetParam(ScFeedParam::DEEP_LINK_ID);
    uiPageDeckMessageBase* msg = param ? CreateLink(param->m_Value.c_str(), linkMetricType) : nullptr;

    if (msg)
    {
        msg->SetTrackingId(atHashString(ScFeedPost::GetDlinkTrackingId(post)));
    }

    return msg;
}

uiPageDeckMessageBase* CreateLink(const char* text, atHashString* linkMetricType)
{
    uiPageDeckMessageBase* result = nullptr;

    sysServiceArgs arg;
    char buffer[128] = { 0 };
    char argstr[128] = { 0 };

    rtry
    {
        rcheckall(text != nullptr && text[0]!= 0);
        arg.Set(text);

        rcheck(arg.GetParamValue(ATSTRINGHASH("class", 0xE45C07DD), buffer), catchall, gnetError("Dlink 'class' not found"));
        atHashString classHash(buffer);

        if (classHash == ATSTRINGHASH("uiGoToOnlineMode", 0xD688B89D)) 
        {
            rcheck(arg.GetParamValue(ScriptRouterLink::SRL_MODE, buffer), catchall, gnetError("Dlink 'mode' not found"));
            eScriptRouterContextMode mode = ScriptRouterContext::GetSRCModeFromString(atString(buffer));
            rcheck(mode != eScriptRouterContextMode::SRCM_INVALID, catchall, gnetError("mode %s does not exist", buffer));

            rcheck(arg.GetParamValue(ScriptRouterLink::SRL_ARGTYPE, buffer), catchall, gnetError("Dlink 'argtype' not found"));
            eScriptRouterContextArgument argtype = ScriptRouterContext::GetSRCArgFromString(atString(buffer));
            rcheck(argtype != eScriptRouterContextArgument::SRCA_INVALID, catchall, gnetError("argtype %s does not exist", buffer));

            arg.GetParamValue(ScriptRouterLink::SRL_ARG, argstr); // This one is optional

			result = rage_new uiLandingPage_GoToOnlineMode(mode, argtype, argstr);
        }
		else if (classHash == ATSTRINGHASH("uiGoToEditorMode", 0xF6DA2279))
		{
			result = rage_new uiLandingPage_GoToEditorMode();
		}
		else if (classHash == ATSTRINGHASH("uiGoToStoryMode", 0xB8733ED7))
        {
            result = rage_new uiLandingPage_GoToStoryMode();
        }
        else if (classHash == ATSTRINGHASH("uiGoToPageMessage", 0x6A1D7748))
        {
            rcheck(arg.GetParamValue(ATSTRINGHASH("target", 0xA2220B09), buffer), catchall, gnetError("Dlink ui 'target' not found"));
            uiPageId pageId(atStringHash(buffer));
            result = rage_new uiGoToPageMessage(uiPageLink(pageId));
        }

        if (linkMetricType != nullptr && result != nullptr)
        {
            *linkMetricType = GetTrackingType(*result);
        }

        return result;
    }
    rcatchall
    {
    }

    if (result != nullptr)
    {
        delete result;
    }

    return nullptr;
}

atHashString GetTrackingType(const uiPageDeckMessageBase& msg)
{
    const atHashString classId = msg.GetClassId();

    if (classId == uiGoToPageMessage::GetStaticClassId())
    {
        return static_cast<const uiGoToPageMessage&>(msg).GetLinkInfo().GetTarget().GetId().GetAsHashString();
    }
    else if (classId == uiLandingPage_GoToOnlineMode::GetStaticClassId())
    {
        const uiLandingPage_GoToOnlineMode& onlineMsg = static_cast<const uiLandingPage_GoToOnlineMode&>(msg);

        if (onlineMsg.GetScriptRouterOnlineModeType() == SRCM_FREE)
        {
            return atHashString(ScriptRouterContext::GetStringFromSRCArgument(onlineMsg.GetScriptRouterArgType()).c_str());
        }

        return atHashString(ScriptRouterContext::GetStringFromSRCMode(onlineMsg.GetScriptRouterOnlineModeType()).c_str());
    }

    return classId;
}

void FillText(ConstString& dest, const atHashString id, const ScFeedPost& post, const atHashString fallback)
{
    const atString* text = post.GetLoc(id);

    // ID's are only allowed in rockstar_* posts.
    if (text && ScFeedLoc::IsLocalTextId(text->c_str()) && ScFeedPost::IsRockstarFeedType(post.GetFeedType()))
    {
        const char* res = TheText.Get(text->c_str() + strlen(ScFeedLoc::LOCAL_TEXT_PROTOCOL));
        dest = (res);
        return;
    }

    // If we have a text or there's no fallback set the text or blank
    if ((text != nullptr && text->length() > 0) || fallback.IsNull())
    {
        dest = (text ? *text : "");
    }
    else
    {
        dest = (TheText.Get(fallback));
    }
}

void FillImage(ConstString& tx, ConstString& txd, const atHashString id, const ScFeedPost& post, const char* defaultTxd)
{
    rtry
    {
        ScFeedContentDownloadInfo info;
        rcheckall(ScFeedContent::GetDownloadInfo(post, id, nullptr, info));

        char txdStr[128] = {0};
        const char* name = info.GetCacheName();

        if (ScFeedLoc::IsLocalImage(name))
        {
            rcheckall(defaultTxd != nullptr);
            const char* txStr = ScFeedContentDownloadInfo::ExtractLocalImage(name, defaultTxd, txdStr, sizeof(txdStr));
            rcheckall(txStr);
            name = txStr;
        }

        tx = (name);
        txd = (name);
    }
    rcatchall
    {
    }
}

void FillSticker(IItemStickerInterface::StickerType& stickerType, ConstString& stickerText, const ScFeedPost& post)
{
    const ScFeedParam* stickerPar = post.GetParam(ScFeedParam::STICKER_ID);
    const atString* stickerLoc = post.GetLoc(ScFeedLoc::STICKERTEXT_ID);

    if (stickerPar == nullptr || stickerPar->m_Value.empty())
    {
        stickerText = "";
        stickerType = IItemStickerInterface::StickerType::None;
        return;
    }

    const atHashString stickerId = stickerPar->ValueAsHash();
    const char* stickerLocText = stickerLoc ? stickerLoc->c_str() : "";

    const int num = COUNTOF(s_CloudStickers);

    for (int i = 0; i < num; ++i)
    {
        StickerBuild& build = s_CloudStickers[i];

        if (build.m_StickerId == stickerId)
        {
            stickerText = (build.m_StickerText == atHashString()) ? stickerLocText : TheText.Get(build.m_StickerText);
            stickerType = build.m_StickerType;
            return;
        }
    }

    gnetWarning("Sticker not found: %s", stickerPar->m_Value.c_str());
    stickerText = "";
    stickerType = IItemStickerInterface::StickerType::None;
}

bool IsTextureLoaded(const char* tx, const char* txd)
{
    if (tx == nullptr || tx[0] == '\0' || txd == nullptr || txd[0] == '\0')
    {
        return true;
    }

    // Downloaded textures have the same txd and tx and there's only one texture per txd
    strLocalIndex index = g_TxdStore.FindSlot(txd);
    fwTxd const* fwtxd = (index == -1) ? nullptr : g_TxdStore.Get(index);
    // Assume if the TXD is present and has any entries, all contained textures are available
    return fwtxd && fwtxd->GetEntry(0);
}

bool CanBeShown(const ScFeedPost& post)
{
    if (ScFeedPost::IsAboutGambling(post) && !CanShowGamblingCards())
    {
        return false;
    }

    return true;
}

bool CanShowGamblingCards()
{
    bool hideEvc = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FEED_HIDE_EVC", 0x736C0AC9), true);
    bool hidePvc = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("FEED_HIDE_PVC", 0x2ABC606F), true);

    return (NETWORK_LEGAL_RESTRICTIONS.IsAnyEvcAllowed() || !hideEvc) && (NETWORK_LEGAL_RESTRICTIONS.IsAnyPvcAllowed() || !hidePvc);
}

bool CanEnableNewsCard()
{
    bool const c_result = CNetworkNewsStoryMgr::Get().GetNumStories(NEWS_TYPE_TRANSITION_HASH) >= 1;
    return c_result;
}

} // namespace uiCloudSupport

#endif // GEN9_LANDING_PAGE_ENABLED
