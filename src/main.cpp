#include "rtspconnector.h"
#include <iostream>
#include <string.h>
#include <thread>
#include "rtmpsender.h"
#include "rtsprtpreceiver.h"

int main()
{
    std::string url = "rtsp://admin:5LH2DQ34@192.168.31.29:554/0/0/101?transportmode=unicast&profile=Profile_1";
    // std::string url = "rtsp://192.168.31.248:8554/h264ESVideoTest";
    RtspConnector::createNew(url)->executeProcess();
    while(1);
    return 0;
}