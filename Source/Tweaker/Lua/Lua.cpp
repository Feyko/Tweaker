﻿#include "Lua.h"

#include <string>
#include "CoreMinimal.h"

#include "AssetRegistryModule.h"
#include "D:/SatisfactoryModding/SatisfactoryModLoader/Source/Tweaker/LuaLib/lua.hpp"
#include "IPlatformFilePak.h"

#include "UObjectIterator.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "LuaUClass.h"
#include "LuaUEnum.h"
#include "LuaUObject.h"
#include "TweakerSubsystem.h"
#include "util/Logging.h"
using namespace std;


namespace Tweaker
{
    namespace Lua
    {
        bool CheckLua(lua_State* L, int r)
        {
            if (r != LUA_OK)
            {
                string errormsg = lua_tostring(L, -1);
                LOGS(errormsg)
                return false;
            }
            else
            {
                return true;
            }
        }

        void stackDump (lua_State *L) {
    
            int i;
            int top = lua_gettop(L);
            for (i = 1; i <= top; i++) {  /* repeat for each level */
                int t = lua_type(L, i);
                string s = lua_typename(L, t);
                LOGS(s);
                LOG(""); /* put a separator */
            }
        }

    	// Mostly borrowed from FIN's source. Thanks Pana !
        void propertyToLua(lua_State* L, UProperty* p, void* data) {
        	LOG("Transforming from Property to Lua");
			auto c = p->GetClass()->ClassCastFlags;
			if (c & EClassCastFlags::CASTCLASS_UBoolProperty) {
				lua_pushboolean(L, *p->ContainerPtrToValuePtr<bool>(data));
			} else if (c & EClassCastFlags::CASTCLASS_UIntProperty) {
				lua_pushinteger(L, *p->ContainerPtrToValuePtr<std::int32_t>(data));
			} else if (c & EClassCastFlags::CASTCLASS_UInt64Property) {
				lua_pushinteger(L, *p->ContainerPtrToValuePtr<std::int64_t>(data));
			} else if (c & EClassCastFlags::CASTCLASS_UFloatProperty) {
				lua_pushnumber(L, *p->ContainerPtrToValuePtr<float>(data));
			} else if (c & EClassCastFlags::CASTCLASS_UStrProperty) {
				lua_pushstring(L, TCHAR_TO_UTF8(**p->ContainerPtrToValuePtr<FString>(data)));
			} else if (c & EClassCastFlags::CASTCLASS_UClassProperty) {
				LuaUClass::ConstructClass(L, *p->ContainerPtrToValuePtr<UClass*>(data));
			} else if (c & EClassCastFlags::CASTCLASS_UObjectProperty) {
				LuaUObject::ConstructObject(L, *p->ContainerPtrToValuePtr<UObject*>(data));
			} else if (c & EClassCastFlags::CASTCLASS_UArrayProperty) {
				UArrayProperty* prop = Cast<UArrayProperty>(p);
				const FScriptArray& arr = prop->GetPropertyValue_InContainer(data);
				lua_newtable(L);
				for (int i = 0; i < arr.Num(); ++i) {
					propertyToLua(L, prop->Inner, ((uint8*) arr.GetData()) + prop->Inner->ElementSize * i);
					lua_seti(L, -2, i+1);
				}
			} else {
				lua_pushnil(L);
			}
		}

    	// Mostly borrowed from FIN's source. Thanks Pana !
		void luaToProperty(lua_State* L, UProperty* p, void* data, int i) {
        	LOG("Transforming from Lua to Property");
			auto c = p->GetClass()->ClassCastFlags;
			if (c & EClassCastFlags::CASTCLASS_UBoolProperty) {
				*p->ContainerPtrToValuePtr<bool>(data) = static_cast<bool>(lua_toboolean(L, i));
			} else if (c & EClassCastFlags::CASTCLASS_UIntProperty) {
				*p->ContainerPtrToValuePtr<std::int32_t>(data) = static_cast<std::int32_t>(lua_tointeger(L, i));
			} else if (c & EClassCastFlags::CASTCLASS_UInt64Property) {
				*p->ContainerPtrToValuePtr<std::int64_t>(data) = static_cast<std::int64_t>(lua_tointeger(L, i));
			} else if (c & EClassCastFlags::CASTCLASS_UFloatProperty) {
				*p->ContainerPtrToValuePtr<float>(data) = static_cast<float>(lua_tonumber(L, i));
			} else if (c & EClassCastFlags::CASTCLASS_UStrProperty) {
				size_t len;
				const char* s = lua_tolstring(L, i, &len);
				if (!s) throw std::exception("Invalid String in string property parse");
				FString* o = p->ContainerPtrToValuePtr<FString>(data);
				*o = FString(UTF8_TO_TCHAR(s));
			} else if (c & EClassCastFlags::CASTCLASS_UClassProperty) {
				UClass* o = ((LuaUClass*)lua_touserdata(L, i))->Class;
				*p->ContainerPtrToValuePtr<UClass*>(data) = o;
			} else if (c & EClassCastFlags::CASTCLASS_UArrayProperty) {
				UArrayProperty* Property = Cast<UArrayProperty>(p);
				const void* ArrayValue = Property->ContainerPtrToValuePtr<void>(data);
				FScriptArrayHelper Helper(Property, ArrayValue);
				lua_pushnil(L);
				Helper.RemoveValues(0, Helper.Num());
				while (lua_next(L, i) != 0) {
					Helper.AddValue();
					luaToProperty(L, Property->Inner, (uint8*)Helper.GetRawPtr(Helper.Num()-1), -1);
					lua_pop(L, 1);
				}
			} else {
				lua_pushnil(L);
			}
		}
        
        int lua_GetClass(lua_State* L)
        {
            LOG("Getting the class");
            UClass* LoadedClass = nullptr;
            if(lua_isstring(L, 1))
            {
            	FString ClassName = lua_tostring(L, 1);
            	FString Package = lua_isstring(L, 2) ? lua_tostring(L, 2) : "FactoryGame";
            	LOGFS(("Trying to load /Script/" + Package+ "." + ClassName))
                LoadedClass = LoadObject<UClass>(NULL, *("/Script/" + Package+ "." + ClassName));
                if (!LoadedClass->IsValidLowLevel()) {
                	LOGFS(("Couldn't find the C++ class, trying to load with " + ClassName))
                	LoadedClass = LoadObject<UClass>(nullptr, *ClassName);
                	if(!LoadedClass->IsValidLowLevel()) {
                		LOG("Couldn't find the BP class by path")
                		for(TObjectIterator<UBlueprintGeneratedClass> it; it; ++it)
                		{
                			UClass* ItClass = *it;
                			if(ItClass->GetName() == ClassName)
                			{
                				LOG("Found the BP")
                                LoadedClass = ItClass;
                				break;
                			}
                		}
                		LOG("Couldn't find the class by iterating")
                	} else {LOG("Found the class by full path")}
                } else {LOG("Found the C++ class")}
				if(LoadedClass->IsValidLowLevel())
				{
					LuaUClass::ConstructClass(L, LoadedClass);
				}
            	else
            	{
            		LOG("Couldn't find the class at all")
            		lua_pushnil(L);
            	}
                
            }
            else
            {
                LOG("No Class Type was specified");
                lua_pushnil(L);
            }
            return 1;
        }


        int lua_Print(lua_State* L)
        {
            if( lua_isstring(L, -1))
            {
                string s = lua_tostring(L, -1);
            	LOGS(s)
            }
            return 0;
        }
    }
}

using namespace Tweaker::Lua;

void UFETLua::test()
{
    LOG("Starting the Lua test");
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    luaL_newmetatable(L, "LuaUClassMeTa");
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, LuaUClass::lua_index);
    lua_settable(L, -3);
    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, LuaUClass::lua_newindex);
    lua_settable(L, -3);
	lua_pushstring(L, "__call");
	lua_pushcfunction(L, LuaUClass::lua__call);
	lua_settable(L, -3);
	lua_pushstring(L, "__tostring");
	lua_pushcfunction(L, LuaUClass::lua__tostring);
	lua_settable(L, -3);

	luaL_newmetatable(L, "LuaUEnumMeTa");
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, LuaUEnum::lua_index);
	lua_settable(L, -3);
	lua_pushstring(L, "__newindex");
	lua_pushcfunction(L, LuaUEnum::lua_newindex);
	lua_settable(L, -3);

	luaL_newmetatable(L, "LuaUObjectMeTa");
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, LuaUObject::lua_index);
	lua_settable(L, -3);
	lua_pushstring(L, "__newindex");
	lua_pushcfunction(L, LuaUObject::lua_newindex);
	lua_settable(L, -3);
	
    lua_register(L, "GetClass", lua_GetClass);
    lua_register(L, "Log", lua_Print);

	LOGFS(SML::GetConfigDirectory())
    if(CheckLua(L, luaL_dofile(L, TCHAR_TO_UTF8(*(SML::GetConfigDirectory() + FString("/Tweaker/script.lua"))))))
    {

    }
}
