#ifndef RTSPCONNECTOR_H
#define RTSPCONNECTOR_H
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <vector>

class RtspRtpReceiver;

class RtspConnector
{
public:
    static RtspConnector* createNew(std::string);
    bool executeProcess();
    RtspRtpReceiver* receiver();
private:
    RtspConnector(std::string);
    ~RtspConnector();
    bool getParamsFromUrl();
    bool connectToHost();
    bool rtspInfosRequest();
    bool callOptions();
    bool callDecribe();
    bool callSetup();
    bool callPlay();
    void callTeardown();
    void getMediaParams(const char*);
    std::string authenticateMd5(std::string, std::string, std::string, std::string, std::string, std::string);
    bool allocRtpReceiver();

    std::string m_rtspUrl;
    std::string m_username;
    std::string m_password;
    std::string m_ip;
    int m_port;
    int m_sockfd;
    int m_seq;
    std::string m_realm;
    std::string m_nonce;
    std::string m_uri;
    std::string m_baseUri;
    std::string m_session;
    bool m_authenticateFlag;
    std::vector<std::string> m_videoParams;
    std::vector<std::string> m_audioParams;
    RtspRtpReceiver* m_receiver;
};

#endif //RTSPCONNECTOR_H