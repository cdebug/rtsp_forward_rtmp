#ifndef RTSPRTPRECEIVER_H
#define RTSPRTPRECEIVER_H
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
class RtmpSender;

class RtspRtpReceiver
{
public:
    ~RtspRtpReceiver();
    static RtspRtpReceiver* createNew();
    bool allocSocket(int);
    int getLocalPort();

private:
    RtspRtpReceiver();
    void executeProcess();
    void resolveRtpData(uint8_t* data, int length);

    int m_sockfd;
    int m_localPort;
    uint8_t* m_frameBuff;
    int m_frameLen;
    RtmpSender* m_sender;
};

#endif //RTSPRTPRECEIVER_H