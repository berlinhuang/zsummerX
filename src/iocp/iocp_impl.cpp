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

#include <zsummerX/iocp/iocp_impl.h>
#include <zsummerX/iocp/tcpsocket_impl.h>
#include <zsummerX/iocp/tcpaccept_impl.h>
#include <zsummerX/iocp/udpsocket_impl.h>
using namespace zsummer::network;

bool EventLoop::initialize()
{
    if (_io != NULL)
    {
        LCF("iocp already craeted !  " << logSection());
        return false;
    }
	//创建一个IO完成端口
	//最后一个参数： 告诉IOCP，同一时刻最多有几个线程在运行，0默认为cpu的数量
    _io = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
    if (!_io)
    {
        LCF("CreateIoCompletionPort false!  " );
        return false;
    }
    return true;
}

void EventLoop::post(_OnPostHandler &&h)
{
    _OnPostHandler *ptr = new _OnPostHandler(std::move(h));
	//提供了一种方式来与线程池中的所有线程进行通信
	//_io:
	//0:
	//PCK_USER_DAT:
	//(LPOVERLAPPED)(ptr):
    PostQueuedCompletionStatus(_io, 0, PCK_USER_DATA, (LPOVERLAPPED)(ptr));
}

std::string EventLoop::logSection()
{
    std::stringstream os;
    os << "logSection[0x" << this << "]: _iocp=" << (void*)_io << ", current total timer=" << _timer.getTimersCount() <<", next expire time=" << _timer.getNextExpireTime();
    return os.str();
}



void EventLoop::runOnce(bool isImmediately)
{
    if (_io == NULL)
    {
        LCF("iocp handle not initialize. " <<logSection());
        return;
    }

    DWORD dwTranceBytes = 0;
    ULONG_PTR uComKey = NULL;
    LPOVERLAPPED pOverlapped = NULL;
	//_io: 线程希望对哪个IOCP进行监视
	//dwTranceBytes:一次完成后的I/O操作所传送数据的字节数
	//uComKey: 完成键 传递的数据被称为 单句柄数据 数据应该是与每个socket句柄对应
	//pOverlapped: 重叠结构体 传递的数据被称为 单IO数据 数据应该与每次的操作WSARecv、WSASend等相对应

	//GetQueuedCompletionStatus 将调用的线程切换到睡眠状态，知道IOCP的 IO完成队列（First in first out）出现一项，或者等待时间已经超出为止
	//调用GetQueuedCompletionStatus, 调用者线程的线程标识符会被添加到IOCP的 等待线程队列，是的IOCP内核对象知道 哪些线程正在等待对已完成的IO请求进行处理。
	//当IOCP的IO完成队列出现一项时，该IOCP就会唤醒 等待线程队列（Fist in last out）中的一个线程，这个线程会得到 已传输字节数，完成键，OVERLAPPED结构的地址
	//唤醒一个线程时会将线程标识符保存在 已释放线程列表中，使得IOCP能够知道哪些线程已被唤醒，并监控执行情况，当线程被其他函数切换到等待状态，IOCP会将它从已释放队列中移除，添加到已暂停线程列表中
	BOOL bRet = GetQueuedCompletionStatus(_io, &dwTranceBytes, &uComKey, &pOverlapped, isImmediately ? 0 : _timer.getNextExpireTime()/*INFINITE*/);

	//还有一个函数GetQueuedCompletionStatusEx 可以一次获取多个 IO完成队列 中的IO请求操作结果

	// LOGI(_timer.getNextExpireTime()); //will get 100ms
	
	//检查定时器超时状态
    _timer.checkTimer();
    
	if (!bRet && !pOverlapped)
    {
        //TIMEOUT
        return;
    }
    
    //! user post
    if (uComKey == PCK_USER_DATA)
    {
        _OnPostHandler * func = (_OnPostHandler*) pOverlapped;
        try
        {
            (*func)();
        }
        catch (const std::exception & e)
        {
            LCW("when call [post] callback throw one runtime_error. err=" << e.what());
        }
        catch (...)
        {
            LCW("when call [post] callback throw one unknown exception.");
        }
        delete func;
        return;
    }
    
    //! net data 收到的数据全部在这边
	/*
		_overlapped
		_type
		_tcpSocket
		_tcpAccept
		_udpSocket
		  _recvWSABuf
		  ....
	*/
	//! 处理来自网络的通知
    ExtendHandle & req = *(HandlerFromOverlaped(pOverlapped)); //计算出pOverLapped所在结构体ExtendHandle的首地址
    switch (req._type)//向IOCP投递请求前会设置ExtendHandle._type 
    {
	case ExtendHandle::HANDLE_ACCEPT://接收连接
        {
            if (req._tcpAccept)//投递完AcceptEx时设置了ExtendHandle._tcpAccept = shared_from_this();
            {
                req._tcpAccept->onIOCPMessage(bRet);
            }
        }
        break;
    case ExtendHandle::HANDLE_RECV://接收
    case ExtendHandle::HANDLE_SEND://发送
    case ExtendHandle::HANDLE_CONNECT://连接
        {
            if (req._tcpSocket)
            {
                req._tcpSocket->onIOCPMessage(bRet, dwTranceBytes, req._type);
            }
        }
        break;
    case ExtendHandle::HANDLE_SENDTO:
    case ExtendHandle::HANDLE_RECVFROM:
        {
            if (req._udpSocket)
            {
                req._udpSocket->onIOCPMessage(bRet, dwTranceBytes, req._type);
            }
        }
        break;
    default:
        LCE("GetQueuedCompletionStatus undefined type=" << req._type << logSection());
    }
    
}


