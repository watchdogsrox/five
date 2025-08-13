#include "CriminalCareerDefs.h"

#if GEN9_STANDALONE_ENABLED

#include "CriminalCareerDefs_parser.h"
#include "CriminalCareerService.h"

// framework
#include "atl/vector.h"
#include "fwmaths/random.h"

// rage
#include "parsercore/utils.h"

void CriminalCareerDefs::ItemFeature::AssignIdentifier(const CriminalCareerDefs::ItemFeatureIdentifier& identifier)
{
	Assertf(m_identifier.IsNull(), "Attempting to reassign ItemFeature identifier " HASHFMT, HASHOUT(m_identifier));
	Assertf(identifier.IsNotNull(), "Attempting to assign an empty ItemFeature identifier");
	m_identifier = identifier;
}

const CriminalCareerDefs::LocalizedTextKey& CriminalCareerDefs::ItemCategoryDisplayData::GetTooltipAddKey() const
{
	const bool c_hasCESP = SCriminalCareerService::GetInstance().HasLocalPlayerPurchasedCESP();
	return c_hasCESP ? m_tooltipCESPAddKey : m_tooltipAddKey;
}

struct Token
{
public:
	void SetIsMethod(const atString& methodName, const atVector<Token>& params)
	{
		m_methodName = methodName;
		m_subTokens = params;
		m_isMethod = true;
	}

	void SetIsValue(int value)
	{
		m_value = value;
		m_isMethod = false;
	}

	const char* GetMethodName() const { return m_methodName; }
	int GetValue() const { return m_value; }
	const atVector<Token>& GetParameters() const { return m_subTokens; }
	bool IsMethod() const { return m_isMethod; }

private:
	atVector<Token> m_subTokens;
	ConstString m_methodName;
	int m_value;
	bool m_isMethod;
};

int ResolveToken(const Token& token)
{
	if (token.IsMethod())
	{
		const atString c_methodName(token.GetMethodName());
		const atVector<Token>& c_params = token.GetParameters();
		const int c_parameterCount = c_params.GetCount();

		if (c_methodName == "rand")
		{
			if (uiVerifyf(c_parameterCount == 1 || c_parameterCount == 2, "Unexpected parameter count %d for method \"%s\"", c_parameterCount, c_methodName.c_str()))
			{
				const int c_minRange = (c_parameterCount == 1) ? 0 : ResolveToken(c_params[0]);
				const int c_maxRange = ResolveToken(c_params[c_parameterCount - 1]);
				const int c_result = fwRandom::GetRandomNumberInRange(c_minRange, c_maxRange);
				return c_result;
			}
		}
		else
		{
			uiAssertf(false, "Unsupported method \"%s\"", c_methodName.c_str());
		}

		return 0;
	}
	else
	{
		return token.GetValue();
	}
}

atString Substr(atString str, int startIndex, int endIndex)
{
	atString substr(&str[startIndex]);
	substr.Truncate(endIndex - startIndex);
	return substr;
}

bool TryParseToken(atString str, int &ref_cursor, Token& out_token)
{
	if (!uiVerifyf(!str.IsNull(), "Attempting to parse empty token"))
	{
		return false;
	}

	const int c_formulaLength = str.GetLength();

	char tokenStart = str[ref_cursor];
	if (tokenStart == '$')
	{
		// Starting a new formula token. Expand to find calculation to perform
		u32 methodNameStart = ref_cursor + 1;
		u32 methodNameEnd = str.IndexOf('(', methodNameStart);
		if (!uiVerifyf(methodNameStart != methodNameEnd && methodNameEnd != atString::npos && methodNameEnd < c_formulaLength - 1, "Malformed formula at index %d of \"%s\"", methodNameStart, str.c_str()))
		{
			return false;
		}
		atString methodName = Substr(str, methodNameStart, methodNameEnd);

		// Check if we have parameters
		bool hasPendingParameter = str[methodNameEnd + 1] != ')';
		atVector<Token> parameters;
		while (hasPendingParameter)
		{
			// Build parameters into tokens
			ref_cursor = methodNameEnd + 1;
			Token param;
			if (!uiVerifyf(TryParseToken(str, ref_cursor, param), "Failed to parse token at index %d of \"%s\"", methodNameEnd + 1, str.c_str()))
			{
				return false;
			}
			parameters.PushAndGrow(param);
			hasPendingParameter = str[ref_cursor] == ',';
		}

		if (!uiVerifyf(str[ref_cursor] == ')', "Malformed formula at index %d of \"%s\"", methodNameStart, str.c_str()))
		{
			return false;
		}
		out_token.SetIsMethod(methodName, parameters);
		++ref_cursor;
	}
	else
	{
		// Retrieve raw value
		const u32 c_valueStart = ref_cursor;
		while (ref_cursor < c_formulaLength && str[ref_cursor] != ',' && str[ref_cursor] != ')')
		{
			++ref_cursor;
		}
		const u32 c_valueEnd = ref_cursor;
		if (!uiVerifyf(c_valueStart != c_valueEnd, "Malformed value at index %d of \"%s\"", c_valueStart, str.c_str()))
		{
			return false;
		}
		atString valueStr = Substr(str, c_valueStart, c_valueEnd);
		int valueInt = parUtils::StringToInt(valueStr.c_str());
		out_token.SetIsValue(valueInt);
	}

	return true;
}

bool TryParseRootToken(atString str, Token& out_token)
{
	int cursor = 0;
	str.Replace(" ", ""); // Strip out whitespace
	return TryParseToken(str, cursor, out_token);
}

void CriminalCareerDefs::ProfileStatData::PostLoad()
{
	if (!m_valueFormula.IsNull())
	{
		Token parsedFormula;

		if (uiVerifyf(TryParseRootToken(m_valueFormula, parsedFormula), "Failed to parse formula \"%s\"", m_valueFormula.c_str()))
		{
			m_value = ResolveToken(parsedFormula);
		}
	}
}

void CriminalCareerDefs::ShoppingCartItemLimits::AssignCategory(const CriminalCareerDefs::ItemCategory& category)
{
	Assertf(m_category.IsNull(), "Attempting to reassign ShoppingCartItemLimits category " HASHFMT, HASHOUT(m_category));
	Assertf(category.IsNotNull(), "Attempting to assign an invalid ShoppingCartItemLimits category");
	m_category = category;
}

StatId CriminalCareerDefs::BuildStatIdentifierForCharacterSlot(int characterSlot, const ProfileStatIdentifier& profileStatId)
{
	// Assign MP0_ or MP1_ prefix depending on character slot
	uiAssertf(characterSlot == 0 || characterSlot == 1, "Encountered unexpected character slot %d", characterSlot);
	// Profile stats can be a maximum of 32 characters long
	const int c_bufferLength = 32;
	char idBuffer[c_bufferLength];
	formatf(idBuffer, c_bufferLength, "MP%d_%s", characterSlot, profileStatId.c_str());
	const StatId c_statId(idBuffer);
	return c_statId;
}

int CCriminalCareerItem::GetCost() const
{
	const bool c_hasCESP = SCriminalCareerService::GetInstance().HasLocalPlayerPurchasedCESP();
	return c_hasCESP ? m_costCESP : m_cost;
}

bool CCriminalCareerItem::HasAnyDisplayStats() const
{
	return m_vehicleStatsData.HasAnyValues() || m_weaponStatsData.HasAnyValues();
}

void CCriminalCareerItem::AssignIdentifier(const CriminalCareerDefs::ItemIdentifier& identifier)
{
	Assertf(m_identifier.IsNull(), "Attempting to reassign CCriminalCareerItem identifier " HASHFMT, HASHOUT(m_identifier));
	Assertf(identifier.IsNotNull(), "Attempting to assign an empty CCriminalCareerItem identifier");
	m_identifier = identifier;
}

const CriminalCareerDefs::ShoppingCartItemLimits& CCriminalCareerShoppingCartValidationData::GetItemLimitsForCategory(const CriminalCareerDefs::ItemCategory& category) const
{
	const CriminalCareerDefs::ShoppingCartItemLimits* cp_found = m_itemCategoryLimits.Access(category);
	return (cp_found) ? *cp_found : m_noLimits;
}

void CCriminalCareerShoppingCartValidationData::PostLoad()
{
	// Traverse items and assign categories from mapped keys
	for (ItemCategoryValidationMap::Iterator it = m_itemCategoryLimits.CreateIterator(); !it.AtEnd(); ++it)
	{
		const CriminalCareerDefs::ItemCategory& category = it.GetKey();
		(*it).AssignCategory(category);
	}
}

#endif // GEN9_STANDALONE_ENABLED
