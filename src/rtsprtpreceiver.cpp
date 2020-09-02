#include "rtsprtpreceiver.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <thread>
#include "rtmpsender.h"

#define MAX_FRAME_BUFF 409600

RtspRtpReceiver::RtspRtpReceiver()
{
    m_frameBuff = new uint8_t[MAX_FRAME_BUFF];
    m_sender = RtmpSender::createNew("rtmp://192.168.31.13:1935/live/livestream");
    m_sender->init();
}

RtspRtpReceiver::~RtspRtpReceiver()
{

}

RtspRtpReceiver* RtspRtpReceiver::createNew()
{
    return new RtspRtpReceiver;
}

bool RtspRtpReceiver::allocSocket(int port)
{
    if((m_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        printf("socket alloc error\n");
        return false;
    }
    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(m_sockfd, (struct sockaddr*)&addr_in, sizeof(addr_in)) == -1)
    {
        printf("bind socket errot\n");
        return false;
    }
    std::thread(&RtspRtpReceiver::executeProcess, this).detach();
    m_localPort = port;
    return true;
}

void RtspRtpReceiver::executeProcess()
{
    char buff[4096], ipbuf[20];
    struct sockaddr_in addr_c;
    memset(&addr_c, 0, sizeof(addr_c));
    socklen_t len = sizeof(addr_c);
    while(1)
    {
        memset(buff, 0, sizeof(buff));
        int n = recvfrom(m_sockfd, buff, sizeof(buff), 0, (struct sockaddr*)&addr_c, &len);
        if(n > 0)
        {
            memset(ipbuf, 0, sizeof(ipbuf));
            inet_ntop(AF_INET,&addr_c.sin_addr.s_addr,ipbuf,sizeof(ipbuf));
            resolveRtpData((uint8_t*)buff, n);
        }
    }
}

int RtspRtpReceiver::getLocalPort()
{
    return m_localPort;
}

void RtspRtpReceiver::resolveRtpData(uint8_t* data, int length)
{
    //去掉padding
    if(data[0] & 0x20)
    {
        length -= data[length - 1];
    }
    if((data[12] & 0x1F) == 28)
    {
        uint8_t naluType = (data[12]&0xe0) | (data[13]&0x1f);
        if(data[13] & 0x80)//S
        {
            uint8_t s[] = {0x00, 0x00, 0x00, 0x01, naluType};
            memcpy(m_frameBuff, s, 5);
            m_frameLen = 5;
        }
        memcpy(m_frameBuff+m_frameLen, data+14, length - 14);
        m_frameLen += length - 14;
        if(data[13] & 0x40)//E
        {
            uint32_t timestamp;
            memcpy((uint8_t*)&timestamp, data + 4, 4);
            timestamp = ntohl(timestamp);
            m_sender->onSendFrame(m_frameBuff, m_frameLen, timestamp);
        }
    }
    else
    {
        uint8_t s[] = {0x00, 0x00, 0x00, 0x01};
        memcpy(m_frameBuff, s, 4);
        m_frameLen = 4;
        memcpy(m_frameBuff + m_frameLen, data+12, length-12);
        m_frameLen += length - 12;
        uint32_t timestamp;
        memcpy((uint8_t*)&timestamp, data + 4, 4);
        timestamp = ntohl(timestamp);
        m_sender->onSendFrame(m_frameBuff, m_frameLen, timestamp);
    }
}