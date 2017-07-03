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




#include <zsummerX/zsummerX.h>


#include <iostream>
#include <queue>
#include <chrono>
#include <iomanip>
#include <string.h>
#include <signal.h>
#include <log4z/log4z.h>
using namespace std;
using namespace zsummer::log4z;
using namespace zsummer::network;


std::string g_remoteIP = "0.0.0.0";
unsigned short g_remotePort = 8081;
unsigned short g_startType = 0;  //0 listen, 1 connect
bool g_runing = true;

EventLoopPtr summer; //ioserver
TcpAcceptPtr accepter; //accept
TcpSocketPtr ts;//socket need create before accept.

TcpSocketPtr usedSocket;// established socket from accept or connect.


char recvBuffer[1024]; //recv buffer
int recvBufferLen = 1024;
char sendBuffer[1024]; //send buffer
int sendBufferLen = 1024;

unsigned long long sendCount = 0;


void signalFun(int sig)
{
    g_runing = false;
}

void OnSocketSend(NetErrorCode ec, int recvLength);
void OnSocketRecv(NetErrorCode ec, int recvLength);
void onConnect(NetErrorCode ec);
void OnAcceptSocket(NetErrorCode ec, TcpSocketPtr s);


int main(int argc, char* argv[])
{
    signal(SIGINT, signalFun);//注册键盘ctrl + c的中断执行函数
	//signal(SIGINT, SIG_DFL);//默认信号处理程序
	//signal(SIGINT, SIG_IGN);//忽略信号处理程序
    if (argc == 2 && 
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "/?") == 0))
    {
        cout << "Example:" << endl;
        cout << "tcpTest remoteIP remotePort startType" << endl;
        cout << "./tcpTest 0.0.0.0 8081 0" << endl;
        cout << "startType: 0 server, 1 client" << endl;
        return 0;
    }

    if (argc > 1) g_remoteIP = argv[1];
    if (argc > 2) g_remotePort = atoi(argv[2]);
    if (argc > 3) g_startType = atoi(argv[3]);
    
    if (g_startType == 0) 
		//ILog4zManager::getPtr()->config("E:\\github\\zsummerX\\example\\bin\\server.cfg");
		ILog4zManager::getPtr()->config(".\\..\\bin\\server.cfg");
    else 
		ILog4zManager::getPtr()->config(".\\..\\bin\\client.cfg");
    ILog4zManager::getPtr()->start();
    LOGI("g_remoteIP=" << g_remoteIP << ", g_remotePort=" << g_remotePort << ", g_startType=" << g_startType );
	//  using EventLoopPtr = std::shared_ptr<EventLoop>;
	//  EventLoopPtr summer;//std::shared_ptr<EventLoop> summer  //指向EventLoop空指针
    summer = std::shared_ptr<EventLoop>(new EventLoop);
    summer->initialize();// _io = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);

    if (g_startType == 0)//server端
    {
        accepter = std::shared_ptr<TcpAccept>(new TcpAccept());//初始化接收连接重叠结构 accept
        accepter->initialize(summer);// _summer = summer;    set eventloop线程summer 
        ts = std::shared_ptr<TcpSocket>(new TcpSocket);//初始化接收连接重叠结构 send recv connect

        if (!accepter->openAccept(g_remoteIP.c_str(), g_remotePort)) //监听套接字绑定完成端口
			return 0;
        accepter->doAccept(ts, std::bind(OnAcceptSocket, std::placeholders::_1, std::placeholders::_2));//投递接收连接请求
    }
    else//client
    {
        usedSocket = std::shared_ptr<TcpSocket>(new TcpSocket);//初始化接收 发送 连接重叠结构
        usedSocket->initialize(summer);
        usedSocket->doConnect(g_remoteIP.c_str(), g_remotePort, std::bind(onConnect, std::placeholders::_1));
    }
	//函数包装器 std::function<返回值（参数类型）> funcName = [捕获外部变量](参数类型 参数)
    std::function<void()> moniter = [&moniter]()
    {
        cout << "echo=" << sendCount / 5 << endl;//每秒发送几次
        sendCount = 0;
        summer->createTimer(5000, std::bind(moniter));//定时器5s
    };
    summer->createTimer(5000, std::bind(moniter));//开始定时

    while (g_runing) 
		summer->runOnce();

    ts.reset();
    accepter.reset();
    usedSocket.reset();
    return 0;
}

void OnSocketSend(NetErrorCode ec, int recvLength)
{
    if (ec) return;
    if (recvLength != sendBufferLen) 
		LOGW("safe warning, need translate remaining data.");
};

void OnSocketRecv(NetErrorCode ec, int recvLength)
{
    if (ec != NEC_SUCCESS) return;
    memcpy(sendBuffer, recvBuffer, recvLength);
    sendBufferLen = recvLength;
	//投递WSASend
    bool ret = usedSocket->doSend(sendBuffer, sendBufferLen, std::bind(OnSocketSend, std::placeholders::_1, std::placeholders::_2));// safe-warning: can't call this method again when last doSend request not return. 
    if (!ret)  return;
	//投递WSARecv
    ret = usedSocket->doRecv(recvBuffer, recvBufferLen, std::bind(OnSocketRecv, std::placeholders::_1, std::placeholders::_2));
    if (!ret)  return;
    sendCount++;
};

void onConnect(NetErrorCode ec)
{
    if (ec != NEC_SUCCESS)
    {
        LOGE("connect error");
        g_runing = false;
        return;
    }
    usedSocket->doRecv(recvBuffer, recvBufferLen, std::bind(OnSocketRecv, std::placeholders::_1, std::placeholders::_2));
    sprintf(sendBuffer, "%s", "hellow----------------------------------------------------------");
    sendBufferLen = (int)strlen(sendBuffer) + 1;
    usedSocket->doSend(sendBuffer, sendBufferLen, std::bind(OnSocketSend, std::placeholders::_1, std::placeholders::_2));// safe-warning: one socket can't concurrent call this method without wait callback. 

};


void OnAcceptSocket(NetErrorCode ec, TcpSocketPtr s)
{
    if (ec != NEC_SUCCESS)
    {
        g_runing = false;
        return;
    }
    usedSocket = s;
    usedSocket->initialize(summer);//连接套接字绑定完成端口
	//投递接收连接的请求
    usedSocket->doRecv(recvBuffer, recvBufferLen, std::bind(OnSocketRecv, std::placeholders::_1, std::placeholders::_2));
};


