//
//  nexdome.cpp
//  NexDome X2 plugin
//
//  Created by Rodolphe Pineau on 6/11/2016.


#include "DigitalNet.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif
#ifdef SB_WIN_BUILD
#include <time.h>
#endif


CDigitalNet::CDigitalNet()
{

    m_pSerx = NULL;
    m_bIsConnected = false;

    m_nCurPos = 0;
    m_nTargetPos = 0;
    m_nPosLimit = 0;
    m_bAborted = false;

    memset(m_szFirmwareVersion, 0, SERIAL_BUFFER_SIZE);
	memset(m_szModel, 0, SERIAL_BUFFER_SIZE);
    memset(m_cDeviceData,0,42);
    memset(m_cControllerData,0,18);
    
#ifdef DigitalNet_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\DigitalNetLog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/DigitalNetLog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/DigitalNetLog.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif


#ifdef	DigitalNet_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CDigitalNet] Constructor Called.\n", timestamp);
	fflush(Logfile);
#endif

}

CDigitalNet::~CDigitalNet()
{
#ifdef	DigitalNet_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif
}

int CDigitalNet::Connect(const char *pszPort)
{
    int nErr = DigitalNet_OK;
    int nDummy;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef DigitalNet_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CDigitalNet::Connect] Called %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    m_bIsConnected = true;
    // 19200 8N2
    nErr = m_pSerx->open(pszPort, CONNECTION_SPEED, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1 -STOPBITS 2");
    if(nErr) {
            m_bIsConnected = false;
            return nErr;
    }

#ifdef DigitalNet_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CDigitalNet::Connect] connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    nErr = setManualMode();
    if(nErr) {
        m_bIsConnected = false;
        m_pSerx->close();
        return nErr;
    }


    // get firmware
    getFirmwareVersion(m_szFirmwareVersion, SERIAL_BUFFER_SIZE);
    // get position, ....
    getPosition(nDummy);

    // read some of the data array from the controller
    readControllerData();
    readDeviceData();

#ifdef DigitalNet_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CDigitalNet::Connect] firmware :  %s\n", timestamp, m_szFirmwareVersion);
    fprintf(Logfile, "[%s] [CDigitalNet::Connect] position :  %d\n", timestamp, m_nCurPos);
    fflush(Logfile);
#endif

    return nErr;
}

void CDigitalNet::Disconnect()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();

	m_bIsConnected = false;
}

#pragma mark - move commands
int CDigitalNet::haltFocuser()
{
    int nErr = DigitalNet_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    m_bAborted = true;

    nErr = DigitalNetCommand("FI00000", strlen("FI00000"), szResp, 1, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(szResp[0] == 'E') {
        nErr = ERR_CMDFAILED;
    }
    // if we were doing a goto let's reset target position to current position
    getPosition(m_nTargetPos);

    return nErr;
}


int CDigitalNet::moveRelativeToPosision(int nSteps)
{
    int nErr = DigitalNet_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    char szCmd[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    //if(m_nPosLimit < m_nCurPos + nSteps )
    //    return ERR_LIMITSEXCEEDED;

    m_nTargetPos = m_nCurPos + nSteps;
    m_bAborted = false;

#ifdef DigitalNet_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CDigitalNet::gotoPosition] goto relative position  : %d\n", timestamp, nSteps);
    fflush(Logfile);
#endif

    if(nSteps<0) {
        snprintf(szCmd, SERIAL_BUFFER_SIZE, "FI%05d", abs(nSteps));
    }
    else {
        snprintf(szCmd, SERIAL_BUFFER_SIZE, "FO%05d", abs(nSteps));
    }

    nErr = DigitalNetCommand(szCmd, (int)strlen(szCmd), szResp, 1, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(szResp[0] == 'E') {
        m_nTargetPos = m_nCurPos;
        nErr = ERR_CMDFAILED;
    }
    return nErr;
}

#pragma mark - command complete functions

int CDigitalNet::isGoToComplete(bool &bComplete)
{
    int nErr = DigitalNet_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(m_bAborted) {
        bComplete = true;
        m_nTargetPos = m_nCurPos;
        return nErr;
    }

    nErr = getPosition(m_nCurPos);
    if(nErr)
        return nErr;

    if(m_nCurPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;
    return nErr;
}


#pragma mark - getters and setters

int CDigitalNet::getFirmwareVersion(char *pszVersion, const int &nStrMaxLen)
{
    int nErr = DigitalNet_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(strlen(m_szFirmwareVersion)) {
        strncpy(pszVersion,m_szFirmwareVersion,nStrMaxLen);
        return nErr;
    }

    nErr = readDeviceData();
    if(nErr)
        return nErr;

	memset(pszVersion, 0, nStrMaxLen);
    memcpy(pszVersion, m_cDeviceData+1, 3);
    strncpy(m_szFirmwareVersion, pszVersion, SERIAL_BUFFER_SIZE);   // save firmware version so we don't need to re-read it
#ifdef DigitalNet_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CDigitalNet::getFirmwareVersion] m_szFirmwareVersion : %s\n", timestamp, m_szFirmwareVersion);
    fflush(Logfile);
#endif

    return nErr;
}

int CDigitalNet::getModel(char * pszModel,  const int &nStrMaxLen)
{
    int nErr = DigitalNet_OK;
    unsigned char nModel = 0;
	int nDeviceID = 0;
	memset(pszModel, 0, nStrMaxLen);
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

    if(strlen(m_szModel)) {
        strncpy(pszModel,m_szModel,nStrMaxLen);
        return nErr;
    }

    nErr = readControllerData();
    if(nErr)
        return nErr;
	
	nDeviceID = m_cDeviceData[14];
    nModel = m_cDeviceData[2+nDeviceID];
	nModel &= 0x0F;
	
#ifdef DigitalNet_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CDigitalNet::getModel] model = %d\n", timestamp, nModel);
    fflush(Logfile);
#endif

    switch(nModel) {
        case 1:
            strncpy(pszModel,"1 cm",nStrMaxLen);
            break;
        case 2:
            strncpy(pszModel,"2 cm",nStrMaxLen);
            break;
        case 3:
            strncpy(pszModel,"4 cm",nStrMaxLen);
            break;
        case 4:
            strncpy(pszModel,"6 cm",nStrMaxLen);
            break;
        default:
            strncpy(pszModel,"Unknown",nStrMaxLen);
            break;
    }
    strncpy(m_szModel, pszModel, SERIAL_BUFFER_SIZE);   // save nodel so we don't need to re-read it
    return nErr;
}


int CDigitalNet::getTemperature(double &dTemperature)
{
    int nErr = DigitalNet_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vFieldsData;
    std::vector<std::string> vCelcius;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = DigitalNetCommand("FTMPRO", strlen("FTMPRO"),  szResp, 7, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

	nErr = parseFields(szResp, vCelcius, '=');
	if(nErr)
		return nErr;

    // convert string value to double
    if(vCelcius.size()>=2)  // T=12.34
        dTemperature = atof(vCelcius[1].c_str());
    return nErr;
}

int CDigitalNet::getPosition(int &nPosition)
{
    int nErr = DigitalNet_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    nErr = DigitalNetCommand("FPOSRO", strlen("FPOSRO"), szResp, 7, SERIAL_BUFFER_SIZE);
    if(nErr) {
        nPosition = m_nCurPos;
        if (nErr == ERR_NORESPONSE)
            nErr = DigitalNet_OK; // we had a imeout and we ignore it
        return nErr;
    }
    // parse output to extract position value.
    nErr = parseFields(szResp, vFieldsData, '=');
    if(nErr) {
        return nErr;
    }
    if(vFieldsData.size()>=2) {
        // convert response
        nPosition = atoi(vFieldsData[1].c_str());
        m_nCurPos = nPosition;
    }
    return nErr;
}

int CDigitalNet::getPosLimit()
{
    return m_nPosLimit;
}

#pragma mark - mode function
int CDigitalNet::setManualMode()
{
    int nErr = DigitalNet_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    nErr = DigitalNetCommand("FMMODE", strlen("FMMODE"), szResp, 1, SERIAL_BUFFER_SIZE);
    if(szResp[0] == 'E') {
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}

int CDigitalNet::setFreeMode()
{
    int nErr = DigitalNet_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    nErr = DigitalNetCommand("FFMODE", strlen("FFMODE"), szResp, 1, SERIAL_BUFFER_SIZE);
    if(!strstr(szResp, "END")) {
        nErr = ERR_CMDFAILED;
    }

    return nErr;
}


int CDigitalNet::calibrateFocuser()
{
    int nErr = DigitalNet_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    nErr = DigitalNetCommand("FCENTR", strlen("FCENTR"), szResp, 5, SERIAL_BUFFER_SIZE);
    // ignore the response for now until I can debug this on actual hardware
    // as this might take a while to return a response.
    return nErr;
}

#pragma mark - read/write device internal data
int CDigitalNet::readDeviceData()
{
	int nErr = DigitalNet_OK;
	char szResp[SERIAL_BUFFER_SIZE];

	nErr = DigitalNetCommand("FDMODE", strlen("FDMODE"), szResp, 39, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

#ifdef DigitalNet_DEBUG
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump((unsigned char *)szResp, cHexMessage, 38, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [CDigitalNet::DigitalNetCommand] szResp = %s\n", timestamp, cHexMessage);
    fflush(Logfile);

#endif

    memcpy(m_cDeviceData, szResp, 38);
	return nErr;
}

int CDigitalNet::writeDeviceData()
{
	int nErr = DigitalNet_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	char szCmd[SERIAL_BUFFER_SIZE];

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "FNMODE");
	memcpy(szCmd+6, m_cDeviceData+19, 19);
	nErr = DigitalNetCommand(szCmd, 25, szResp, 1, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	if(szResp[0] != 'D') {
		nErr = ERR_CMDFAILED;
	}
    return nErr;
}

#pragma mark - read/write controller internal data
int CDigitalNet::readControllerData()
{
	int nErr = DigitalNet_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	
	nErr = DigitalNetCommand("FEMODE", strlen("FEMODE"), szResp, 19, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

#ifdef DigitalNet_DEBUG
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    hexdump((unsigned char *)szResp, cHexMessage, 18, LOG_BUFFER_SIZE);
    fprintf(Logfile, "[%s] [CDigitalNet::DigitalNetCommand] szResp = %s\n", timestamp, cHexMessage);
    fflush(Logfile);

#endif

    memcpy(m_cControllerData, szResp, 18);
	return nErr;
}

int CDigitalNet::writeControllerData()
{
	int nErr = DigitalNet_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	char szCmd[SERIAL_BUFFER_SIZE];

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "FGMODE");
	memcpy(szCmd+6, m_cControllerData+13, 5);
	nErr = DigitalNetCommand(szCmd, 11, szResp, 1, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	if(szResp[0] != 'D') {
		nErr = ERR_CMDFAILED;
	}

	return nErr;
}

#pragma mark - command and response functions

int CDigitalNet::DigitalNetCommand(const char *pszszCmd, const unsigned int &nCmdLen, char *pszResult, const unsigned int &nResultLenght, const unsigned int &nResultMaxLen)
{
    int nErr = DigitalNet_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(nResultLenght>nResultMaxLen)
        return ERR_MEMORY;

    m_pSerx->purgeTxRx();
#ifdef DigitalNet_DEBUG
	unsigned char cHexMessage[LOG_BUFFER_SIZE];
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CDigitalNet::DigitalNetCommand] Sending %s\n", timestamp, pszszCmd);
	hexdump((unsigned char *)pszszCmd, cHexMessage, nResultLenght, LOG_BUFFER_SIZE);
	fprintf(Logfile, "[%s] [CDigitalNet::DigitalNetCommand] Sending [hex] : %s\n", timestamp, cHexMessage);
	fflush(Logfile);
#endif
    nErr = m_pSerx->writeFile((void *)pszszCmd, nCmdLen, ulBytesWrite);
    m_pSerx->flushTx();

    if(nErr){
        return nErr;
    }

    if(pszResult) {
        // read response
        nErr = readResponse(szResp, nResultLenght, nResultMaxLen);
        if(nErr){
            return nErr;
        }
        memset(pszResult, 0, nResultMaxLen);
        memcpy(pszResult,szResp,nResultLenght);
#ifdef DigitalNet_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CDigitalNet::DigitalNetCommand] pszResult \"%s\"\n", timestamp, pszResult);
		hexdump((unsigned char *)pszResult, cHexMessage, nResultLenght, LOG_BUFFER_SIZE);
		fprintf(Logfile, "[%s] [CDigitalNet::DigitalNetCommand] pszResult [hex] : %s\n", timestamp, cHexMessage);
		fflush(Logfile);
#endif

	}
    return nErr;
}

int CDigitalNet::readResponse(char *pszRespBuffer, const unsigned int &nResultLenght, const unsigned int &nBufferLen)
{
    int nErr = DigitalNet_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef DigitalNet_DEBUG
    unsigned char cHexMessage[LOG_BUFFER_SIZE];
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CDigitalNet::readResponse] reading response, expeted response length : %d\n", timestamp, nResultLenght);
	fflush(Logfile);
#endif

	memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, MAX_TIMEOUT);
        if(nErr) {
#ifdef DigitalNet_DEBUG
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CDigitalNet::readResponse] ERRO READING response : %d\n", timestamp, nErr);
            fprintf(Logfile, "[%s] [CDigitalNet::readResponse] ulBytesRead : %lu\n", timestamp, ulBytesRead);
            fprintf(Logfile, "[%s] [CDigitalNet::readResponse] pszRespBuffer : %s\n", timestamp, pszRespBuffer);
            hexdump((unsigned char *)pszRespBuffer, cHexMessage, int(ulTotalBytesRead + ulBytesRead), LOG_BUFFER_SIZE);
            fprintf(Logfile, "[%s] [CDigitalNet::readResponse] pszRespBuffer [hex] : %s\n", timestamp, cHexMessage);
            fflush(Logfile);
#endif
            return nErr;
        }

		if (ulBytesRead !=1) {// timeout
#ifdef DigitalNet_DEBUG
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] CDigitalNet::readResponse timeout\n", timestamp);
            fprintf(Logfile, "[%s] CDigitalNet::readResponse : %s\n", timestamp, pszRespBuffer);
			fflush(Logfile);
#endif
            nErr = ERR_NORESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
        pszBufPtr++;
    } while (ulTotalBytesRead < nResultLenght );

#ifdef DigitalNet_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CDigitalNet::readResponse] ulTotalBytesRead : %lu\n", timestamp, ulTotalBytesRead);
	fflush(Logfile);
#endif

    return nErr;
}

int CDigitalNet::parseFields(const char *pszIn, std::vector<std::string> &svFields, const char &cSeparator)
{
    int nErr = DigitalNet_OK;
    std::string sSegment;
    std::stringstream ssTmp(pszIn);

    svFields.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, cSeparator))
    {
        svFields.push_back(sSegment);
    }

    if(svFields.size()==0) {
        nErr = ERR_BADFORMAT;
    }
    return nErr;
}

void CDigitalNet::hexdump(const unsigned char* pszInputBuffer, unsigned char *pszOutputBuffer, const int &nInputBufferSize, const int &nOutpuBufferSize)
{
    unsigned char *pszBuf = pszOutputBuffer;
    int nIdx=0;

    memset(pszOutputBuffer, 0, nOutpuBufferSize);
    for(nIdx=0; nIdx < nInputBufferSize && pszBuf < (pszOutputBuffer + nOutpuBufferSize -3); nIdx++){
        snprintf((char *)pszBuf,4,"%02X ", pszInputBuffer[nIdx]);
        pszBuf+=3;
    }
}
