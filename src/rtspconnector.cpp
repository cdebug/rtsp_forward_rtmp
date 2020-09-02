#include "rtspconnector.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <algorithm>
#include "rtsprtpreceiver.h"

std::string MD5(const std::string& src )
{
    MD5_CTX ctx;
 
    std::string md5_string;
    unsigned char md[16] = { 0 };
    char tmp[33] = { 0 };
 
    MD5_Init( &ctx );
    MD5_Update( &ctx, src.c_str(), src.size() );
    MD5_Final( md, &ctx );
 
    for( int i = 0; i < 16; ++i )
    {   
        memset( tmp, 0x00, sizeof( tmp ) );
        sprintf( tmp, "%02X", md[i] );
        md5_string += tmp;
    }   
    return md5_string;
}

std::string clipParam(std::string str, std::string startSym, char endSym)
{
    std::string ret;
    size_t pos = str.find(startSym);
    if(pos != std::string::npos)
    {
        pos += startSym.length();
        for(size_t i = pos+1; i < str.length(); ++i)
        {
            if(str.at(i) == endSym || str.at(i) == '\r' || str.at(i) == '\n')
            {
                ret = str.substr(pos, i - pos);
                break;
            }
        }
    }
    return ret;
}

const char* getLineFromBuff(const char* buff, char* line)
{
    memset(line, 0, sizeof(*line));
    if(buff[0] != 0)
    	while((*line++ = *buff++) != '\n');
    *line = '\0';
    return buff;
}


RtspConnector::RtspConnector(std::string url)
{
    m_rtspUrl = url;
    m_seq = 1;
    m_authenticateFlag = false;
    m_receiver = nullptr;
}

RtspConnector::~RtspConnector()
{

}

RtspConnector* RtspConnector::createNew(std::string url)
{
    return new RtspConnector(url);
}

bool RtspConnector::executeProcess()
{
    if(getParamsFromUrl() && connectToHost() && rtspInfosRequest())
        return true;
    else
        return false;
}

RtspRtpReceiver* RtspConnector::receiver()
{
    return m_receiver;
}

bool RtspConnector::getParamsFromUrl()
{
    char username[20], password[20], ip[16], port[6], str[100];
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    memset(ip, 0, sizeof(ip));
    memset(port, 0, sizeof(port));
    memset(str, 0, sizeof(str));
    if(sscanf(m_rtspUrl.c_str(), "rtsp://%[^:]:%[^@]@%[^:]:%[^/]/%s", username, password, ip, port, str) == 5)
    {  
        m_username = username;
        m_password = password;
        m_ip = ip;
        m_port = atoi(port);
    }
    else if(sscanf(m_rtspUrl.c_str(), "rtsp://%[^:]:%[^/]/%s", ip, port, str) == 3)
    {  
        m_username = "";
        m_password = "";
        m_ip = ip;
        m_port = atoi(port);
    }
    else
    {
        printf("Can not parse url\n");
        return false;
    }
    return true;
}

bool RtspConnector::connectToHost()
{
    if((m_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Cannot init socket\n");
        return false;
    }

   	struct sockaddr_in addr_client;  
    /* 填写服务器地址: IP + 端口 */
    addr_client.sin_family = AF_INET;
    addr_client.sin_port = htons(m_port);
    if(inet_pton(AF_INET, m_ip.c_str(), &addr_client.sin_addr) <= 0) /* 可通过返回值判断IP地址合法性 */
    {
        printf("invalid ip:%s\n", m_ip.c_str());
        return false;
    }
    /* client建链, TCP三次握手, 这里clientsockfd可以不显示绑定地址，内核会选client的本端IP并分配一个临时端口 */
    if (-1 == connect(m_sockfd, (struct sockaddr *)&addr_client, sizeof(addr_client))) 
    {
        perror("Connect failed!\n");
        close(m_sockfd);
        return false;
    }
    return true;
}

bool RtspConnector::rtspInfosRequest()
{
    if(callOptions() && callDecribe() && callSetup() && callPlay())
    {
        return true;
    }
    return false;
}

bool RtspConnector::callOptions()
{
    m_seq++;
    m_uri = m_rtspUrl;
    std::string tmp = m_username+":"+m_password+"@";
    size_t pos;
    if((pos = m_uri.find(tmp))!=std::string::npos)
        m_uri.replace(pos, tmp.length(), "");
    char buff[6096];
    memset(buff, 0, sizeof(buff));
    sprintf(buff, "OPTIONS %s RTSP/1.0\r\n"
"CSeq: %d\r\n"
"\r\n", m_uri.c_str(), m_seq);
    if(send(m_sockfd, buff, strlen(buff), 0) <= 0)
    {
        printf("options send error:%s\n", buff);
        return false;
    }
    if(recv(m_sockfd, buff, sizeof(buff), 0) <= 0)
    {
        printf("options recv error:%s\n", buff);
        return false;
    }
    char line[1024], version[20], stateCode[20], status[20];
    getLineFromBuff(buff, line);
    if(sscanf(line, "%s %s %s\r\n", version, stateCode, status) != 3)
    {
        printf("options cannot parser reply\n");
        return false;
    }
    if(strcmp(stateCode, "200") != 0)
    {
        printf("options stateCode:%s\n", stateCode);
        return false;
    }
    return true;
}

bool RtspConnector::callDecribe()
{
    m_seq ++;
    char buff[4096];
    memset(buff, 0, sizeof(buff));
    sprintf(buff, "DESCRIBE %s RTSP/1.0\r\n"
"CSeq: %d\r\n"
"Accept: application/sdp\r\n"
"\r\n", m_uri.c_str(), m_seq);
    if(send(m_sockfd, buff, strlen(buff), 0) <= 0)
    {
        printf("describe send error:%s\n", buff);
        return false;
    }
    if(recv(m_sockfd, buff, sizeof(buff), 0) <= 0)
    {
        printf("describe recv error:%s\n", buff);
        return false;
    }
    char line[1024], version[20], stateCode[20], status[20];
    const char* tmpPtr = getLineFromBuff(buff, line);
    if(sscanf(line, "%s %s %s\r\n", version, stateCode, status) != 3)
    {
        printf("describe cannot parser reply\n");
        return false;
    }
    if(strcmp(stateCode, "401") == 0)
    {
        m_authenticateFlag = true;
        m_seq ++;
        m_realm = clipParam(buff, "realm=\"", '\"');
        m_nonce = clipParam(buff, "nonce=\"", '\"');
        std::string response = authenticateMd5("DESCRIBE", m_username, m_password, m_realm, m_nonce, m_uri);
        memset(buff, 0, sizeof(buff));
        sprintf(buff, "DESCRIBE %s RTSP/1.0\r\n"
"CSeq: %d\r\n"
"Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"
"Accept: application/sdp\r\n"
"\r\n", m_uri.c_str(), m_seq, m_username.c_str(), m_realm.c_str()
, m_nonce.c_str(), m_uri.c_str(), response.c_str());
        if(send(m_sockfd, buff, strlen(buff), 0) <= 0)
        {
            printf("options send error:%s\n", buff);
            return false;
        }
        if(recv(m_sockfd, buff, sizeof(buff), 0) <= 0)
        {
            printf("options recv error:%s\n", buff);
            return false;
        }
        tmpPtr = getLineFromBuff(buff, line);
        if(sscanf(line, "%s %s %s\r\n", version, stateCode, status) != 3)
        {
            printf("cannot parser reply\n");
            return false;
        }
    }
    if(strcmp(stateCode, "200") != 0)
    {
        printf("describe stateCode:%s\n", stateCode);
        return false;
    }
    while((tmpPtr = getLineFromBuff(tmpPtr, line)))
    {
        if(strlen(line) <= 0)
            break;
        char baseUri[100];
        if(sscanf(line, "Content-Base: %s\r\n", baseUri) == 1)
        {
            m_baseUri = baseUri;
            break;
        }
    }
    getMediaParams(buff);
    return true;
}

bool RtspConnector::callSetup()
{
    m_seq ++;
    std::string videoUri, audioUri, prefix = "a=control:";
    for(size_t i = 0; i < m_videoParams.size(); ++i)
    {
        std::string param = m_videoParams.at(i);
        if(m_videoParams.at(i).compare(0, prefix.length(), prefix, 0, prefix.length()) == 0)
        {
            videoUri = m_videoParams.at(i).substr(prefix.length());
            break;
        }
    }
    for(size_t i = 0; i < m_audioParams.size(); ++i)
    {
        if(m_audioParams.at(i).compare(0, prefix.length(), prefix, 0, prefix.length()) == 0)
        {
            audioUri = m_audioParams.at(i).substr(prefix.length());
            break;
        }
    }
    for(size_t i = 0; i < videoUri.length();)
    {
        if(videoUri.at(i) == '\r' || videoUri.at(i) == '\n')
            videoUri.replace(i, 1, "");
        else
            i++;
    }
    for(size_t i = 0; i < audioUri.length();)
    {
        if(audioUri.at(i) == '\r' || audioUri.at(i) == '\n')
            audioUri.replace(i, 1, "");
        else
            i++;
    }
    if(!allocRtpReceiver())
    {
        printf("setup cannot alooc rtp receiver\n");
        return false;
    }
    int rtpPort = m_receiver->getLocalPort();
    std::string response = authenticateMd5("SETUP", m_username, m_password, m_realm, m_nonce, m_baseUri);
    char buff[6096];
    memset(buff, 0, sizeof(buff));
    if(m_authenticateFlag)
        sprintf(buff, "SETUP %s RTSP/1.0\r\n"
"CSeq: %d\r\n"
"Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"
"Transport: RTP/AVP;unicast;client_port=%d-%d\r\n"
"\r\n", videoUri.c_str(), m_seq, m_username.c_str(), m_realm.c_str(), m_nonce.c_str()
, m_baseUri.c_str(), response.c_str(), rtpPort, rtpPort+1);
    else
        sprintf(buff, "SETUP %s RTSP/1.0\r\n"
"CSeq: %d\r\n"
"Transport: RTP/AVP;unicast;client_port=%d-%d\r\n"
"\r\n", m_uri.c_str(), m_seq, rtpPort, rtpPort+1);
    if(send(m_sockfd, buff, strlen(buff), 0) <= 0)
    {
        printf("setup send error:%s\n", buff);
        return false;
    }
    memset(buff, 0, sizeof(buff));
    if(recv(m_sockfd, buff, sizeof(buff), 0) <= 0)
    {
        printf("setup recv error\n");
        return false;
    }
    char line[1024], version[20], stateCode[20], status[20];
    const char* tmpPtr = getLineFromBuff(buff, line);
    if(sscanf(line, "%s %s %s\r\n", version, stateCode, status) != 3)
    {
        printf("setup cannot parser reply\n");
        return false;
    }
    if(strcmp(stateCode, "200") != 0)
    {
        printf("setup failed\n");
        return false;
    }
    while((tmpPtr = getLineFromBuff(tmpPtr, line)))
    {
        if(strlen(line) <= 0)
            break;
        char str1[20], session[20], str2[100];
        if(sscanf(line, "Session%[^A-Za-z0-9]%[A-Za-z0-9]%s", str1, session, str2) == 3)
        {
            m_session = session;
            break;
        }
    }
    return true;
}

bool RtspConnector::callPlay()
{
    m_seq++;
    std::string response = authenticateMd5("PLAY", m_username, m_password, m_realm, m_nonce, m_baseUri);
    char buff[6096];
    memset(buff, 0, sizeof(buff));
    if(m_authenticateFlag)
        sprintf(buff, "PLAY %s RTSP/1.0\r\n"
"CSeq: %d\r\n"
"Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"\r\n"
"Session: %s\r\n"
"Range: npt=0.000-\r\n"
"\r\n", m_uri.c_str(), m_seq, m_username.c_str(), m_realm.c_str(), m_nonce.c_str()
, m_baseUri.c_str(), response.c_str(), m_session.c_str());
    else
        sprintf(buff, "PLAY %s RTSP/1.0\r\n"
"CSeq: %d\r\n"
"Session: %s\r\n"
"Range: npt=0.000-\r\n"
"\r\n", m_uri.c_str(), m_seq, m_session.c_str());
    if(send(m_sockfd, buff, strlen(buff), 0) <= 0)
    {
        printf("play send error:%s\n", buff);
        return false;
    }
    if(recv(m_sockfd, buff, sizeof(buff), 0) <= 0)
    {
        printf("play recv error:%s\n", buff);
        return false;
    }
    char line[1024], version[20], stateCode[20], status[20];
    getLineFromBuff(buff, line);
    if(sscanf(line, "%s %s %s\r\n", version, stateCode, status) != 3)
    {
        printf("play cannot parser reply\n");
        return false;
    }
    if(strcmp(stateCode, "200") != 0)
    {
        printf("play stateCode:%s\n", stateCode);
        return false;
    }
    printf("Play %s\n", m_rtspUrl.c_str());
    return true;
}

void RtspConnector::callTeardown()
{
    char buff[6096];
    memset(buff, 0, sizeof(buff));
    sprintf(buff, "DESCRIBE %s RTSP/1.0\r\n"
"CSeq: 3\r\n"
"Accept: application/sdp\r\n", m_rtspUrl.c_str());
    if(send(m_sockfd, buff, strlen(buff), 0) <= 0)
        printf("send error:%s\n", buff);
    if(recv(m_sockfd, buff, sizeof(buff), 0) <= 0)
        printf("recv error:%s\n", buff);
}

void RtspConnector::getMediaParams(const char* buff)
{
    char line[1024];
    std::vector<std::string>* tmp = nullptr;
    while((buff = getLineFromBuff(buff, line)))
    {
        if(strlen(line) == 0)
            break;
        if(strcmp(line, "\r\n") == 0)
            continue;
        if(strlen(line) >= 7 && strncmp(line, "m=video", 7) == 0)
        {
            tmp = &m_videoParams;
        }
        if(strlen(line) >= 7 && strncmp(line, "m=audio", 7) == 0)
        {
            tmp = &m_audioParams;
        }
        if(tmp)
            (*tmp).push_back(line);
    }
}

std::string RtspConnector::authenticateMd5(std::string method, std::string username, std::string password
    , std::string realm, std::string nonce, std::string uri)
{
    std::string ha1 = MD5(username + ":" + realm + ":" + password);
    std::string ha2 = MD5(method + ":" + uri);
    transform(ha1.begin(), ha1.end(), ha1.begin(), ::tolower);
    transform(ha2.begin(), ha2.end(), ha2.begin(), ::tolower);
    std::string response = MD5(ha1 + ":" + nonce + ":" + ha2);
    transform(response.begin(), response.end(), response.begin(), ::tolower);
    return response;
}

bool RtspConnector::allocRtpReceiver()
{
    RtspRtpReceiver* receiver = RtspRtpReceiver::createNew();
    for(int i = 20000; i < 30000; i++)
    {
        if(receiver->allocSocket(i))
        {
            m_receiver = receiver;
            return true;
        }
    }
    delete receiver;
    return false;
}