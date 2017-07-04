/*
 * zsummerX License
 * -----------
 * 
 * zsummerX is licensed under the terms of the MIT license reproduced below.
 * This means that zsummerX is free software and can be used for both academic
 * and commercial purposes at absolutely no cost.
 * 
 * 
 * ===============================================================================
 * 
 * Copyright (C) 2010-2015 YaweiZhang <yawei.zhang@foxmail.com>.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * ===============================================================================
 * 
 * (end of COPYRIGHT)
 */


#include "performance.h"

static void hook_run_fn(lua_State *L, lua_Debug *ar);

static int newindex(lua_State * L)
{
	lua_pushvalue(L, 1);//t 把栈上给定索引处的元素作一个副本压栈
    lua_pushvalue(L, 2);//k	
    lua_pushvalue(L, 3);//v

	//int lo = lua_gettop(L); //此处应该是6
	/*
		6,v
		5,k
		4,t
		3,v
		2,k
		1,t
	*/
    lua_rawset(L, 4);//t        t[k] = v  
	//const char *ty = luaL_typename(L, 4); //类型是个table
	//int lo = lua_gettop(L); //此处应该是4
	/*
		4, table吧
		3, v
		2, k
		1, t
	*/
    lua_pop(L, 1);//从栈中弹出 1 个元素
    const char * key = luaL_checkstring(L, 2);//第 2 个参数是否是一个 字符串并返回这个字符串
	const char * v = luaL_checkstring(L, 3);//第 3 个参数是否是一个 字符串并返回这个字符串
    const char * vt = luaL_typename(L, 3);//返回给定索引处值的类型名
    std::stringstream ss;
    ss << "catch one operation that it's set a global value. key=" << key << ", type(v)=" << vt << ", is it correct ?";
	//将表summer压栈，返回该压入值的类型                返回summer["logw"]类型  是否为function
    if (lua_getglobal(L, "summer") == LUA_TTABLE && lua_getfield(L, -1, "logw") == LUA_TFUNCTION)
    {
		//ss.str().c_str();的值"catch one operation that it's set a global value. key=sdf, type(v)=string, is it correct ?"
        lua_pushstring(L, ss.str().c_str());
        lua_pcall(L, 1, 0, 0);
    }
    else if (lua_getglobal(L, "print") == LUA_TFUNCTION)
    {
        lua_pushstring(L, ss.str().c_str());
        lua_pcall(L, 1, 0, 0);
    }
    lua_pop(L, lua_gettop(L) - 3);
    
    return 0;
}


zsummer::Performence __perf;
void luaopen_performence(lua_State * L)
{
    //lua_Hook oldhook = lua_gethook(L);
    //int oldmask = lua_gethookmask(L);
    lua_sethook(L, &hook_run_fn, LUA_MASKCALL | LUA_MASKRET, 0);
    lua_getglobal(L, "_G");//将表_G压栈，返回该压入值的类型
    lua_newtable(L);//创建一个表格t放到栈顶
    lua_pushcclosure(L, newindex, 0);
    lua_setfield(L, -2, "__newindex");//t = { __newindex = newindex }
    lua_setmetatable(L, -2);//把一个table弹出堆栈，并将其设为给定索引处的值的metatable  _G.metatable = { __newindex = newindex }
	//设置了_G.metatable = { __newindex = newindex } 作用
	// lua脚本给全局变量table _G赋值时，会被__newindex捕捉到
}

void hook_run_fn(lua_State *L, lua_Debug *ar)
{
    // 获取Lua调用信息
    lua_getinfo(L, "Snl", ar);
    std::string key;
    if (ar->source)
    {
        key += ar->source;
    }
    key += "_";
    if (ar->what)
    {
        key += ar->what;
    }
    key += "_";
    if (ar->namewhat)
    {
        key += ar->namewhat;
    }
    key += "_";
    if (ar->name)
    {
        key += ar->name;
    }
    if (ar->event == LUA_HOOKCALL)
    {
        __perf._stack.push(key, lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0));
    }
    else if (ar->event == LUA_HOOKRET)
    {
        //lua_gc(L, LUA_GCCOLLECT, 0);
        auto t = __perf._stack.pop(key, lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0));
        if (std::get<0>(t))
        {
            __perf.call(key, std::get<1>(t), std::get<2>(t));
        }
        if (__perf.expire(50000.0))
        {
            __perf.dump(100);
        }
    }
}

