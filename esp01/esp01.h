/*
 * esp01.h
 *
 *  Created on: Sep 15, 2023
 *      Author: Saif
 */

#ifndef SRC_ESP01_ESP01_H_
#define SRC_ESP01_ESP01_H_

typedef struct {
	char *header;
	const char *mime;
	uint16_t code;
	uint8_t method; //GET or POST
	const char *page;
}PageStructure;

typedef  int8_t(*myFunc)();
typedef PageStructure *(*serverFunc)();
#define true 1
#define false 0
#define HTTP_GET 	0
#define HTTP_POST 	1

typedef uint8_t bool;

typedef	enum
{
	WifiMode_Error                          =     0,
	WifiMode_Station                        =     1,
	WifiMode_SoftAp                         =     2,
	WifiMode_StationAndSoftAp               =     3,
}WifiMode_t;

typedef enum {
	STATUS_NOT_STARTED	=0,
	STATUS_STARTED		=1,
	STATUS_EXECUTING	=2,
	STATUS_COMPLETED	=3
} Step_Status;

typedef enum {
	RESULT_ERROR	= -1,
	RESULT_NONE		= 0,
	RESULT_OK	= 1
} StepResult;

typedef struct
{
	myFunc mfunc;
	uint8_t param1;
	uint8_t step;
	bool completed;
	uint16_t timeout;
	uint32_t started_time;
}FuncStatus;

typedef enum
{
  WifiEncryptionType_Open                 =     0,
  WifiEncryptionType_WPA_PSK              =     2,
  WifiEncryptionType_WPA2_PSK             =     3,
  WifiEncryptionType_WPA_WPA2_PSK         =     4,
}WifiEncryptionType_t;

typedef enum {
	WIFI_CONNECTION_STATUS_NO_STARTED		= 0,
	WIFI_CONNECTION_STATUS_CONNECTED_NOIP	= 1,
	WIFI_CONNECTION_STATUS_CONNECTED_GOTIP	= 2,
	WIFI_CONNECTION_STATUS_CONNECTING		= 3,
	WIFI_CONNECTION_STATUS_DISCONNECTED		= 4
}WifiConnectionStatus_e;

typedef struct{
	uint8_t WifiConnStatus;
	bool InProgress;
	char ip[17];
	char gateway[17];

}WifiConnectionStatus_t;

typedef struct{
	uint16_t port;
	uint16_t timeout;
	bool serverON;
}ServerStatus_t;


typedef struct
{
	serverFunc mfunc;
	uint8_t linkid;
	char * url;
	char *header;
	const char *mime;
	uint16_t code;
	uint8_t method; //GET or POST
	const char *page;
	uint16_t lastPageSendIndex;
	uint16_t lastHeaderSendIndex;
}ServerFuncs;

//**********************************************************************
void HAL_UARTEx_RxEventCallback_ESP01( UART_HandleTypeDef *huart,uint16_t Size);
void HAL_UART_TxCpltCallback_ESP01(UART_HandleTypeDef *huart);
//*********************************************************************

//************Library public functions*************************
void Debug(const char *info);
void Initialize_ESP01(char * _ssid, char * _pwd);
void Process_ESP01();
void StartServer_ESP01(uint16_t port, uint16_t tm);
void StopServer_ESP01();
void onServer_ESP01(char *url, uint8_t method,serverFunc func);
PageStructure serverSendESP01(int code, char * mime, const char *page);
void onServerPageNotFound_ESP01();
WifiConnectionStatus_t getWIfiStatus();
PageStructure  serverRedirectESP01( const char *link);
//****************************************************

//********* private Functions**********************
void QueueFunction(myFunc f);
void QueueFunction1(myFunc f, uint8_t param1);
char *_CheckAck(uint8_t* ack);
int8_t _CheckAck1(uint8_t* ack,bool timeout_fired,uint8_t timeout_step,uint8_t returnIfSuccess,uint8_t returnIfTimeOut);
void handleClient();
int parseString(const char * token,const char * value, char* target);
void _SendCommandNonblocking(uint8_t* command,bool incrementStep, uint16_t timeout);
void _SendDataNonblocking(const char * data,uint16_t startIndex, uint16_t bytesToSend,bool incrementStep, uint16_t timeout);
void prepareHeader(uint8_t index);
//************************************************************

//*********ASYNCHRONOUS private Functions**********************
int8_t ConnectWifi();
int8_t CheckWifiConnectionStatus();
int8_t GetConnectionIP();
int8_t Init();
int8_t StartServer();
int8_t CheckNetConnectionStatus();
int8_t SendPage();
//*****************************************************

WifiConnectionStatus_t getWIfiStatus();

#endif /* SRC_ESP01_ESP01_H_ */
