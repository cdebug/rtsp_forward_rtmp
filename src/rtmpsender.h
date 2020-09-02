#ifndef RTMPSENDER_H
#define RTMPSENDER_H
#include <iostream>
#include <string.h>
#include <stdio.h>

#define MAX_BUFF_SIZE 409600

class RtmpSender
{
public:
    ~RtmpSender();
    static RtmpSender* createNew(std::string);
    bool init();
    void onSendFrame(uint8_t*, unsigned int, uint32_t);
private:
    RtmpSender(std::string);
    int sendH264Frame(uint8_t*, unsigned int, bool, uint32_t);
    int SendPacket(unsigned int,uint8_t*,unsigned int, uint32_t);
    int SendVideoSpsPps(uint8_t *pps,int pps_len,uint8_t * sps,int sps_len);

    std::string m_rtmpUrl;
    struct RTMP* m_pRtmp;
    uint8_t* m_spsFrame;
    uint8_t* m_ppsFrame;
    int m_spsLen;
    int m_ppsLen;
    int m_timeTick;
    int m_packetNum;
};

#endif //RTMPSENDER_H