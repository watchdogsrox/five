// Rage headers
#include "script/wrapper.h"

// Framework headers
#include "fwscript/scriptguid.h"
#include "fwsys/gameskeleton.h"
#include "fwdecorator/decoratorExtension.h"
#include "fwdecorator/decoratorInterface.h"

// Game Headers
#include "script/script.h"
#include "commands_decorator.h"

DECORATOR_OPTIMISATIONS();

namespace decorator_commands
{
	bool g_DecorRegistrationsLocked = false;

	void Shutdown(u32 shutdownMode)
	{
		if (shutdownMode == SHUTDOWN_SESSION)
		{
			g_DecorRegistrationsLocked = false; //turn the lock off so decorators can be registered again.
		}
	}

	template<typename T>
    bool SetValue(int guid, const char* label, T val)
    {
        fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
        if (pBase)
        {
            atHashWithStringBank decKey(label);
            fwDecoratorExtension::Set((*pBase), decKey, val);
            return true;
        }
        else
        {
            decAssertf(0, "GUID %d is not a valid object to apply decorators too (label %s)", guid, label);
        }
        return false;
    }

    template<>
    bool SetValue<const char *>(int guid, const char* label, const char *val)
    {
        fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
        if (pBase)
        {
            atHashWithStringBank decKey(label);
            atHashWithStringBank decVal(val);
            fwDecoratorExtension::Set((*pBase), decKey, decVal);
            return true;
        }
        else
        {
            decAssertf(0, "GUID %d is not a valid object to apply decorators too (label %s)", guid, label);
        }
        return false;
    }

	bool SetTime(int guid, const char* label, int val)
	{
		fwDecorator::Time temp = (fwDecorator::Time)val;
		return SetValue<fwDecorator::Time>(guid, label, temp);
	}

	int GetTime(int guid, const char* label)
	{
		fwDecorator::Time out = fwDecorator::UNDEF_TIME;
		fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
		if (pBase)
		{
			atHashWithStringBank decKey(label);
			decVerifyf(fwDecoratorExtension::Get((*pBase),decKey,out), "Time decorator {%s} not found on GUID [%d]. Please verify the object has this decorator.", label, guid);
		}
		else
		{
			decAssertf(0, "GUID %d is not a valid object to apply decorators too (label %s)", guid, label);
		}
		return (int)out;
	}

    bool GetBool(int guid, const char* label)
    {
        bool out=false;
        fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
        if (pBase)
        {
            atHashWithStringBank decKey(label);
            decVerifyf(fwDecoratorExtension::Get((*pBase),decKey,out), "Bool decorator {%s} not found on GUID [%d]. Please verify the object has this decorator.", label, guid);
        }
        else
        {
            decAssertf(0, "GUID %d is not a valid object to apply decorators too (label %s)", guid, label);
        }
        return out;
    }

    float GetFloat(int guid, const char* label)
    {
        float out=0.f;
        fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
        if (pBase)
        {
            atHashWithStringBank decKey(label);
            decVerifyf(fwDecoratorExtension::Get((*pBase),decKey,out), "Float decorator {%s} not found on GUID [%d]. Please verify the object has this decorator.", label, guid);
        }
        else
        {
            decAssertf(0, "GUID %d is not a valid object to apply decorators too (label %s)", guid, label);
        }
        return out;
    }

    int GetInt(int guid, const char* label)
    {
        int out=0;
        fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
        if (pBase)
        {
            atHashWithStringBank decKey(label);
            decVerifyf(fwDecoratorExtension::Get((*pBase),decKey,out), "Int decorator {%s} not found on GUID [%d]. Please verify the object has this decorator.", label, guid);
        }
        else
        {
            decAssertf(0, "GUID %d is not a valid object to apply decorators too (label %s)", guid, label);
        }
        return out;
    }

    int GetStringHash(int guid, const char* label)
    {
        int out = 0;
        fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
        if (pBase)
        {
            atHashWithStringBank decKey(label);
            atHashWithStringBank temp;
            decVerifyf(fwDecoratorExtension::Get((*pBase),decKey,temp), "String decorator {%s} not found on GUID [%d]. Please verify the object has this decorator.", label, guid);
            out = temp.GetHash();
        }
        else
        {
            decAssertf(0, "GUID %d is not a valid object to apply decorators too (label %s)", guid, label);
        }
        return out;
    }

    bool StringMatches(int guid, const char* label, const char* value)
    {
        bool retval = false;
        fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
        if (pBase)
        {
            atHashWithStringBank decKey(label);
            atHashWithStringBank temp;
            fwDecoratorExtension::Get((*pBase), decKey, temp);

            atHashWithStringBank test(value);
            if (temp.GetHash() == test.GetHash())
            {
                retval = true;
            }
        }
        else
        {
            decAssertf(0, "GUID %d is not a valid object to apply decorators too (label %s, value%s)", guid, label, value);
        }
        return retval;
    }

    bool ExistsOn(int guid, const char* label)
    {
        fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
        if (pBase)
        {
            atHashWithStringBank decKey(label);
            return fwDecoratorExtension::ExistsOn((*pBase), decKey);
        }
        else
        {
            decAssertf(0, "GUID %d is not a valid object to apply decorators too (label %s)", guid, label);
        }
        return false;
    }

    bool RemoveFrom(int guid, const char* label)
    {
        fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
        if (pBase)
        {
            atHashWithStringBank decKey(label);
            return fwDecoratorExtension::RemoveFrom((*pBase), decKey);
        }
        else
        {
            decAssertf(0, "GUID %d is not a valid object to apply decorators too (label %s)", guid, label);
        }
        return false;
    }

    bool RemoveAllFrom(int guid)
    {
        fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(guid);
        if (pBase)
        {
            return fwDecoratorExtension::RemoveAllFrom((*pBase));
        }
        else
        {
            decAssertf(0, "GUID %d is not a valid object to apply decorators too", guid);
        }
        return false;
    }

    void RegisterName_W_Type(const char* label, int decoratorType)
    {
		if( g_DecorRegistrationsLocked )
		{
			decAssertf(0, "%s decorator cannot be registered after DECOR_REGISTER_LOCK has been called.", label);
			return;
		}

        const char* who = "Script";
#if DECORATOR_DEBUG
        who = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptName();
#endif
        gp_DecoratorInterface->Register(label, (fwDecorator::Type)decoratorType, who);
    }

	void RegisterLock()
    {
		g_DecorRegistrationsLocked = true;
    }

	void RegisterUnlock()
    {
		g_DecorRegistrationsLocked = false;
    }

    bool IsRegisteredAsType(const char* label, int decoratorType)
    {
        atHashWithStringBank decKey(label);
        return gp_DecoratorInterface->IsRegisteredAsType(decKey, (fwDecorator::Type)decoratorType);
    }

    void SetupScriptCommands()
    {
		SCR_REGISTER_SECURE_UNHASHED(DECOR_SET_TIME,0x69499f92536c52d9, SetTime);
        SCR_REGISTER_SECURE_UNHASHED(DECOR_SET_BOOL,0x2c8c3211219c37bd, SetValue<bool>);
        SCR_REGISTER_SECURE_UNHASHED(DECOR_SET_FLOAT,0x1403fd7f2dede0e8, SetValue<float>);
        SCR_REGISTER_SECURE_UNHASHED(DECOR_SET_INT,0x05b874e9abff72b4, SetValue<int>);
        SCR_REGISTER_UNUSED(DECOR_SET_STRING,0xbfb078a469d01d51, SetValue<const char *>);

		SCR_REGISTER_UNUSED(DECOR_GET_TIME,0xcf17343024f6ffb8, GetTime);
        SCR_REGISTER_SECURE(DECOR_GET_BOOL,0x9a1e55620a6c250d, GetBool);
        SCR_REGISTER_SECURE(DECOR_GET_FLOAT,0xbff2ff18a8a89f88, GetFloat);
        SCR_REGISTER_SECURE(DECOR_GET_INT,0xc3b76795ecbdef41, GetInt);
        SCR_REGISTER_UNUSED(DECOR_GET_STRING_HASH,0xd74dc079a89d890f, GetStringHash);
        
        SCR_REGISTER_UNUSED(DECOR_STRING_MATCHES,0xa1015a8ff2d5d37b, StringMatches);
        SCR_REGISTER_SECURE(DECOR_EXIST_ON,0xf2549ff74d64b720, ExistsOn);
        SCR_REGISTER_SECURE(DECOR_REMOVE,0x8b3711a36570b638, RemoveFrom);
        SCR_REGISTER_UNUSED(DECOR_REMOVE_ALL,0x6577ce993f5613d6, RemoveAllFrom);
        
        SCR_REGISTER_SECURE(DECOR_REGISTER,0x11a81a2059db324b, RegisterName_W_Type);
        SCR_REGISTER_SECURE(DECOR_IS_REGISTERED_AS_TYPE,0x6bd06f4780eac5dd, IsRegisteredAsType);

		SCR_REGISTER_SECURE(DECOR_REGISTER_LOCK,0x8649fc67a631cb32, RegisterLock);
		SCR_REGISTER_UNUSED(DECOR_REGISTER_UNLOCK,0xc3d247bca013ba18, RegisterUnlock);
    }
} // namespace decorator_commands