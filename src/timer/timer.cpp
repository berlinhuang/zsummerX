﻿/*
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


#include  <zsummerX/timer/timer.h>
using namespace zsummer::network;
#include <algorithm>

Timer::Timer()
{

}
Timer::~Timer()
{
}
//get current time tick. unit is millisecond.
unsigned long long Timer::getSteadyTick()
{
    return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::steady_clock::now().time_since_epoch()).count();
}

//get current time tick. unit is millisecond.
unsigned long long Timer::getSystemTick()
{
	//chrono是一个time library，源于boost，现在已经是C++标准
	//std::chrono::duration_cast<目标时间周期>(时间周期)
	//std::chrono::milliseconds 毫秒

	//Clocks当前时钟
	//std::chrono::system_clock //系统获取的时间
	//std::chrono::steady_clock //不能被修改的时钟
	//std::chrono::high_resolution_clock //高精度时钟，实际上是system_clock或者steady_clock的别名
	//::now()来获取时间点

    return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::system_clock::now().time_since_epoch()).count();
}

TimerID Timer::makeTimeID(bool useSystemTick, unsigned int delayTick)
{
    unsigned long long tick = 0;
    if (useSystemTick)
    {
        tick = getSystemTick() - _startSystemTime;
    }
    else
    {
        tick = getSteadyTick() - _startSteadyTime;
    }
    tick += delayTick;
    tick <<= SequenceBit;//15
    tick |= (++_queSeq & SequenceMask);
    tick &= TimeSeqMask;
    if (useSystemTick)
    {
        tick |= UsedSysMask;
    }
    return tick;
}
std::pair<bool, unsigned long long> Timer::resolveTimeID(TimerID timerID)
{
	//bool, unsigned long long
    return std::make_pair((timerID & UsedSysMask) != 0, (timerID & TimeSeqMask) >> SequenceBit);
}
using std::min;
//get next expire time  be used to set timeout when calling select / epoll_wait / GetQueuedCompletionStatus.
unsigned int Timer::getNextExpireTime()
{
    unsigned long long steady = -1;
    unsigned long long sys = -1;
    if (!_sysQue.empty())
    {
        sys = getSystemTick() - _startSystemTime - resolveTimeID(_sysQue.begin()->first).second;
    }
    if (!_steadyQue.empty())
    {
        steady = getSteadyTick() - _startSteadyTime - resolveTimeID(_steadyQue.begin()->first).second;
    }
    return (unsigned int)min(min(sys, steady), 100ULL);
}
TimerID Timer::createTimer(unsigned int delayTick, const _OnTimerHandler &handle, bool useSysTime)
{
    return createTimer(delayTick, std::bind(handle), useSysTime);
}
TimerID Timer::createTimer(unsigned int delayTick, _OnTimerHandler &&handle, bool useSysTime)
{
    _OnTimerHandler *pfunc = new _OnTimerHandler(std::move(handle));//std::move将左值转化为右值引用
    TimerID timerID = makeTimeID(useSysTime, delayTick);
    if (useSysTime)
    {
		//std::map<TimerID, _OnTimerHandler* > _sysQue;
		//pair< std::map<TimerID, _OnTimerHandler* >::iterator,bool> > p = _sysQue.insert(std::make_pair(timerID, pfunc))
		//p->first 就是返回的map<string,int>::iterator迭代器
		//p->first.first 就是TimerID类型数据
        while (!_sysQue.insert(std::make_pair(timerID, pfunc)).second)//判断插入是否成功
        {
            timerID = makeTimeID(useSysTime, delayTick);
        }
    }
    else
    {
        while (!_steadyQue.insert(std::make_pair(timerID, pfunc)).second)
        {
            timerID = makeTimeID(useSysTime, delayTick);
        }
    }
    return timerID;
}

bool Timer::cancelTimer(TimerID timerID)
{
    auto ret = resolveTimeID(timerID);
    if (ret.first)
    {
        auto founder = _sysQue.find(timerID);
        if (founder != _sysQue.end())
        {
            delete founder->second;
            _sysQue.erase(founder);
            return true;
        }
    }
    else
    {
        auto founder = _steadyQue.find(timerID);
        if (founder != _steadyQue.end())
        {
            delete founder->second;
            _steadyQue.erase(founder);
            return true;
        }
    }
    return false;
}
//执行定时器
void Timer::checkTimer()
{
    if (!_steadyQue.empty())
    {
        unsigned long long now = getSteadyTick() - _startSteadyTime;
		//std::map<TimerID, _OnTimerHandler* > _sysQue;
		//_steadyQue.begin()->first 是 TimerID
		//_steadyQue.begin()->second 是 _OnTimerHandler*
		//resolveTimeID(_steadyQue.begin()->first).second
        while (!_steadyQue.empty() && now > resolveTimeID(_steadyQue.begin()->first).second)
        {
            TimerID timerID = _steadyQue.begin()->first;
            _OnTimerHandler * handler = _steadyQue.begin()->second;
            //erase the pointer from timer queue before call handler.
            _steadyQue.erase(_steadyQue.begin());
            try
            {
                //LCD("call timer(). now=" << (now >> ReserveBit)  << ", expire=" << (timerID >> ReserveBit));
                (*handler)();//执行定时器
            }
            catch (const std::exception & e)
            {
                LCW("OnTimerHandler have runtime_error exception. timerID=" << timerID << ", err=" << e.what());
            }
            catch (...)
            {
                LCW("OnTimerHandler have unknown exception. timerID=" << timerID);
            }
            delete handler;
        }
    }
    if (!_sysQue.empty())
    {
        unsigned long long now = getSystemTick() - _startSystemTime;
        while (!_sysQue.empty() && now > resolveTimeID(_sysQue.begin()->first).second)
        {
            TimerID timerID = _sysQue.begin()->first;
            _OnTimerHandler * handler = _sysQue.begin()->second;
            //erase the pointer from timer queue before call handler.
            _sysQue.erase(_sysQue.begin());
            try
            {
                //LCD("call timer(). now=" << (now >> ReserveBit)  << ", expire=" << (timerID >> ReserveBit));
                (*handler)();
            }
            catch (const std::exception & e)
            {
                LCW("OnTimerHandler have runtime_error exception. timerID=" << timerID << ", err=" << e.what());
            }
            catch (...)
            {
                LCW("OnTimerHandler have unknown exception. timerID=" << timerID);
            }
            delete handler;
        }
    }
}

