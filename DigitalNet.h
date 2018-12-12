//
//  DigitalNet.h
//  NexDome
//
//  Created by Rodolphe Pineau on 2017/05/30.
//  NexDome X2 plugin

#ifndef __DigitalNet__
#define __DigitalNet__
#include <math.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include <exception>
#include <typeinfo>
#include <stdexcept>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"

#define DigitalNet_DEBUG 2

#define SERIAL_BUFFER_SIZE 256
#define MAX_TIMEOUT 1000
#define LOG_BUFFER_SIZE 256

#define COMMAND_DELAY   500

enum DigitalNet_Errors    {DigitalNet_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, DigitalNet_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum MotorDir       {NORMAL = 0 , REVERSE};
enum MotorStatus    {IDLE = 0, MOVING};

class CDigitalNet
{
public:
    CDigitalNet();
    ~CDigitalNet();

    int         Connect(const char *pszPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; };
    void        setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper; };

    // move commands
    int         haltFocuser();
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);

    // getter and setter
    int         getFirmwareVersion(char *pszVersion, const int &nStrMaxLen);
    int         getModel(char * pszModel,  const int &nStrMaxLen);
    int         getTemperature(double &dTemperature);
    int         getPosition(int &nPosition);
    int         syncMotorPosition(int nPos);
    int         getPosLimit(void);
    void        setPosLimit(int nLimit);
    bool        isPosLimitEnabled(void);
    void        enablePosLimit(bool bEnable);
    int         centerFocuser(void);
    
protected:

    int             DigitalNetCommand(const char *pszCmd, const unsigned int &nCmdLen, char *pszResult, const unsigned int &nResultLenght, const unsigned int &nResultMaxLen);
    int             readResponse(char *pszRespBuffer, const unsigned int &nResultLenght, const unsigned int &nBufferLen);
    int             parseFields(const char *pszIn, std::vector<std::string> &svFields, const char &cSeparator);

    int             setManualMode();
    int             setFreeMode();

	int				readDeviceData();
	int				writeDeviceData();

	int				readControllerData();
	int				writeControllerData();

	char			m_cDeviceData[42];
	char			m_cControllerData[18];
	
    SerXInterface   *m_pSerx;
    SleeperInterface    *m_pSleeper;

    bool            m_bIsConnected;
    char            m_szFirmwareVersion[SERIAL_BUFFER_SIZE];
    char            m_szModel[SERIAL_BUFFER_SIZE];
    char            m_szLogBuffer[LOG_BUFFER_SIZE];

    int             m_nCurPos;
    int             m_nTargetPos;
    int             m_nPosLimit;
    bool            m_bPosLimitEnabled;
    bool            m_bMoving;

#ifdef DigitalNet_DEBUG
    std::string m_sLogfilePath;
	// timestamp for logs
	char *timestamp;
	time_t ltime;
	FILE *Logfile;	  // LogFile
#endif
};

#endif //__DigitalNet__
