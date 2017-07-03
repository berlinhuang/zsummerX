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

//! zsummer的测试客户端模块
//! main文件

#include <signal.h>
#include <zsummerX/zsummerX.h>
#include <log4z/log4z.h>
#include <proto4z/proto4z.h>
using namespace std;
using namespace zsummer::network;
using namespace zsummer::proto4z;
//! 消息包缓冲区大小
#define _MSG_BUF_LEN    (1200)
std::string g_fillString;
std::string g_c2s = "client";
std::string g_s2c = "server";
#define NOW_TIME (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())
unsigned short g_type;
//! 消息包 
struct Picnic
{
    char           recvData[_MSG_BUF_LEN];
    UdpSocketPtr sock;//        using UdpSocketPtr = std::shared_ptr<UdpSocket>;
};
using PicnicPtr = std::shared_ptr<Picnic>;

unsigned long long g_totalEcho;
unsigned long long g_totalEchoTime;
unsigned long long g_totalSend;
unsigned long long g_totalRecv;


void onRecv(NetErrorCode ec, const char *remoteIP, unsigned short remotePort, int translate, PicnicPtr pic)
{
    if (ec)
    {
        LOGE ("onRecv Error, EC=" << ec );
        return;
    }
	//sendto 目标ip 目标port
    //pic->sock->doSendTo(pic->recvData, translate, remoteIP, remotePort);
	if (remotePort == 88)//server
	{
		pic->sock->doSendTo((char *)g_s2c.c_str(), (unsigned int)g_s2c.length(), remoteIP, remotePort);//server
	}
	else
	{
		pic->sock->doSendTo((char *)g_c2s.c_str(), (unsigned int)g_c2s.length(), remoteIP, remotePort);//client
	}

	//投递接收
    pic->sock->doRecvFrom(pic->recvData, _MSG_BUF_LEN, std::bind(onRecv,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3,
        std::placeholders::_4,
        pic));
    g_totalEcho++;
    g_totalRecv++;
    g_totalSend++;
}

int main(int argc, char* argv[])
{
    //! linux下需要屏蔽的一些信号
#ifndef _WIN32
    signal( SIGHUP, SIG_IGN );
    signal( SIGALRM, SIG_IGN ); 
    signal( SIGPIPE, SIG_IGN );
    signal( SIGXCPU, SIG_IGN );
    signal( SIGXFSZ, SIG_IGN );
    signal( SIGPROF, SIG_IGN ); 
    signal( SIGVTALRM, SIG_IGN );
    signal( SIGQUIT, SIG_IGN );
    signal( SIGCHLD, SIG_IGN);
#endif
    if (argc < 5)
    {
        cout <<"please input like example:" << endl;
        cout <<"./udpTest ip port type maxClient" << endl;
        cout <<"type: 0 server, 1 client" << endl;
        return 0;
    }

    std::string ip = argv[1];
    unsigned short port = atoi(argv[2]);
    g_type = atoi(argv[3]);
    unsigned int maxClient = atoi(argv[4]);
    if (g_type ==1)
    {
        zsummer::log4z::ILog4zManager::getPtr()->config(".\\..\\bin\\client.cfg");
        zsummer::log4z::ILog4zManager::getPtr()->start();
    }
    else
    {
        zsummer::log4z::ILog4zManager::getPtr()->config(".\\..\\bin\\server.cfg");
        zsummer::log4z::ILog4zManager::getPtr()->start();
    }
    LOGI("ip=" << ip << ", port=" << port << ", type=" << g_type << ", maxClients=" << maxClient);
    
    zsummer::network::EventLoopPtr summer(new zsummer::network::EventLoop());
    summer->initialize();//创建完成端口_io

    g_fillString.resize(200, 'z');//初始化200个z

    g_totalEcho = 0;
    g_totalEchoTime = 0;
    g_totalSend = 0;
    g_totalRecv = 0;

    
    if (g_type == 0)//server
    {
        PicnicPtr pic(new Picnic());
		memset(&pic->recvData, 0, sizeof(pic->recvData));
        pic->sock = std::make_shared<UdpSocket>();
		//创建socket
		//绑定地址
		//绑定完成端口
        pic->sock->initialize(summer, ip.c_str(), port);
		//
		/*
		struct ExtendHandle 
		{
			OVERLAPPED     _overlapped;
			unsigned char _type;
			std::shared_ptr<TcpSocket> _tcpSocket;
			std::shared_ptr<TcpAccept> _tcpAccept;
			std::shared_ptr<UdpSocket> _udpSocket;
			enum HANDLE_TYPE
			{
				HANDLE_ACCEPT,
				HANDLE_RECV,
				HANDLE_SEND,
				HANDLE_CONNECT,
				HANDLE_RECVFROM,
				HANDLE_SENDTO,
			};
		};
		*/
		// ExtendHandle _recvHandle;
		//_recvHandle._overlapped  //WSARecvFrom() 传入
		//_recvHandle._udpSocket = shared_from_this();
		//_recvHandle._type = ExtendHandle::HANDLE_RECVFROM;
		//投递WSARecvFrom
        pic->sock->doRecvFrom(pic->recvData, _MSG_BUF_LEN, std::bind(onRecv,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            pic));
    }
    else
    {

        for (unsigned int i = 0; i < maxClient; i++)
        {
            PicnicPtr pic(new Picnic);
			memset(&pic->recvData, 0, sizeof(pic->recvData));
            pic->sock = std::make_shared<UdpSocket>();
			//client
            if (!pic->sock->initialize(summer, "0.0.0.0", 0))
            {
                LOGI("init udp socket error.");
                continue;
            }
			//
            pic->sock->doRecvFrom(pic->recvData, _MSG_BUF_LEN, std::bind(onRecv,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3,
                std::placeholders::_4,
                pic));
			//客户端先发送
            //pic->sock->doSendTo((char *)g_fillString.c_str(), (unsigned int)g_fillString.length(), ip.c_str(), port);
			pic->sock->doSendTo((char *)g_c2s.c_str(), (unsigned int)g_c2s.length(), ip.c_str(), port);

        }

    }
    
    
    //定时检测
    unsigned long long nLast[3] = {0};
	//测试每秒的效率
    std::function<void()> doTimer = [&nLast, &doTimer, &summer]()
    {
        LOGI("EchoSpeed[" << (g_totalEcho - nLast[0])/5.0
            << "]  SendSpeed[" << (g_totalSend - nLast[1])/5.0
            << "]  RecvSpeed[" << (g_totalRecv - nLast[2])/ 5.0
            << "].");
        nLast[0] = g_totalEcho;
        nLast[1] = g_totalSend;
        nLast[2] = g_totalRecv;

        summer->createTimer(5 * 1000, std::bind(doTimer));
    };
    summer->createTimer(5*1000, std::bind(doTimer));
    do
    {
        summer->runOnce();
    } while (true);
    return 0;
}

