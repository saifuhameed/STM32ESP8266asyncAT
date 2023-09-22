/*
 * esp01.c
 *
 *  Created on: Sep 15, 2023
 *      Author: Saif
 */

#include "main.h"
#include "esp01.h"
#include <stdio.h>
#include "string.h"
#include <stdlib.h>
#include "esp8266util.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart1_rx;

UART_HandleTypeDef *_esp01_uart;
UART_HandleTypeDef *_debug_uart;


#define MAX_FUNC 10
#define MAX_URLS 10
FuncStatus funcStatus[MAX_FUNC]={};

ServerFuncs serveFuncs[MAX_URLS]={};
int8_t url_index_tail=0;
int8_t current_func_tail=-1;
int8_t current_func_head=-1;

char * ssid;
char * password;
#define ESP01_BUFFER_SIZE 512
uint8_t esp01_uart_rx_buffer[ESP01_BUFFER_SIZE] = {'\0'};

#define ESP01_MainBuf_SIZE 2048
uint8_t ESP01_MainBuf[ESP01_MainBuf_SIZE];
uint16_t oldPos = 0;
uint16_t newPos = 0;
uint8_t rx_event_fired=0; //1 means rx_event fired
uint8_t tx_complete_fired=0; //1 means rx_event fired

uint32_t esp01_prev_ticks_250=0;
uint32_t esp01_prev_ticks_1000=0;
uint32_t esp01_prev_ticks_5000=0;
uint32_t esp01_prev_ticks_10000=0;

WifiConnectionStatus_t wifiConnStatus;
ServerStatus_t serverStatus;
char header[500]={};
WifiConnectionStatus_t getWIfiStatus(){
	return wifiConnStatus;
}

// Changes are made in place //eltongo/unescape.c @ https://gist.github.com/eltongo/177a2d9473e2fc212c0685238e895328
int hex_to_int(char hexChar) {
    if (hexChar >= '0' && hexChar <= '9') {
        return hexChar - '0';
    } else if (hexChar >= 'a' && hexChar <= 'f') {
        return hexChar - 'a' + 10;
    }

    return -1;
}

// Unescapes url encoding: e.g. %20 becomes a space
// Changes are made in place //eltongo/unescape.c @ https://gist.github.com/eltongo/177a2d9473e2fc212c0685238e895328
void unescape_input(char *input) {
    size_t length = strlen(input);

    for (size_t i = 0; i < length; ++i) {
        if (input[i] == '%' && i < length - 2) {
            int high = hex_to_int(input[i + 1]) * 16;
            int low = hex_to_int(input[i + 2]);

            // if either high or low is -1, it means our conversion failed
            // (i.e. one of the digits was not hexadecimal
            if (high >= 0 && low >= 0) {
                input[i] = (char) (high + low);

                // pull characters to fill the 'empty' space
                for (size_t j = i + 3; j < length; ++j) {
                    input[j - 2] = input[j];
                }

                // two characters were removed from the string, so we shorten the length
                input[length - 2] = '\0';
                input[length - 1] = '\0';
                length -= 2;
            }
        }
    }
}
void Initialize_ESP01(char * _ssid, char * _pwd){
	ssid=_ssid;
	password=_pwd;
	_esp01_uart=&huart1;
	_debug_uart=&huart2;
	QueueFunction(Init);
	QueueFunction(ConnectWifi);
	QueueFunction(GetConnectionIP);
	QueueFunction(CheckWifiConnectionStatus);

}


void StartServer_ESP01(uint16_t port,uint16_t timeout){
	serverStatus.port=port;
	serverStatus.timeout=timeout;
	serverStatus.serverON=true;
	QueueFunction(StartServer);
	//QueueFunction(CheckNetConnectionStatus);
}

void StopServer_ESP01(){
	serverStatus.serverON=false;
}
PageStructure  serverSendESP01(int code, char * mime, const char *page){
	PageStructure sf;
	sf.page=page;
	sf.code=code;
	sf.mime=mime;
	sf.header='\0';
	return sf;
}

PageStructure  serverRedirectESP01( const char *link){
	char header1[]="HTTP/1.1 303 See Other\r\nContent-Type: text/html\r\nLocation: %s\r\n";
	sprintf(header,header1,link);
	PageStructure sf;
	sf.code=0;
	sf.mime='\0';
	sf.header=header;
	sf.page='\0';
	return sf;
}

void onServerPageNotFound_ESP01 (serverFunc func){
	serveFuncs[0].mfunc=func; //0 index exclusively for "Page not found"

}

void onServer_ESP01(char *url, uint8_t method, serverFunc func){
	url_index_tail++;
	if(url_index_tail>=MAX_URLS){
		url_index_tail=MAX_URLS-1;
	}
	serveFuncs[url_index_tail].url=url;
	serveFuncs[url_index_tail].mfunc=func;
	serveFuncs[url_index_tail].method=method;
}

void prepareHeader(uint8_t index){
	//sprintf(header, "%s %d %s \r\n Content-Type:%s\r\n","HTTP/1.0",serveFuncs[index].code, responseCodeToString(serveFuncs[index].code),serveFuncs[index].mime );
	if(serveFuncs[index].code>0){
		if(!serveFuncs[index].header)
		sprintf(header, "%s %d %s\r\nContent-Type: %s\r\n\r\n","HTTP/1.1", serveFuncs[index].code, responseCodeToString(serveFuncs[index].code),serveFuncs[index].mime );
		//sprintf(header, "%s %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n","HTTP/1.1", serveFuncs[index].code, responseCodeToString(serveFuncs[index].code),serveFuncs[index].mime,225 );

		serveFuncs[index].header=(char*)header;
	}

}


void handleClient(){
	char * pch1=_CheckAck((uint8_t*) "+IPD,");
	if(pch1){
		Debug(ESP01_MainBuf);
		uint8_t connid;
		char  requesturl[200];
		uint8_t ret=sscanf(pch1, "+IPD,%d,%*d:GET%s",&connid, requesturl );
		if(ret<2){
			ret=sscanf(pch1, "+IPD,%d,%*d:POST%s",&connid, requesturl );
		}
		unescape_input(&requesturl);
		uint8_t found=0;
		for (uint8_t i=1;i<MAX_URLS;i++){
			if(strcmp(serveFuncs[i].url,requesturl)==0){
				serveFuncs[i].linkid=connid;
				PageStructure * p=serveFuncs[i].mfunc();
				serveFuncs[i].page=p->page;
				serveFuncs[i].code=p->code;
				serveFuncs[i].header=p->header;
				serveFuncs[i].mime=p->mime;
				serveFuncs[i].lastPageSendIndex=0;
				serveFuncs[i].lastHeaderSendIndex=0;
				QueueFunction1(SendPage,i);
				found=1;
				break;
			}
		}
		if(found==0){
			serveFuncs[0].linkid=connid; //0 index exclusively for "Page not found"
			PageStructure *p=serveFuncs[0].mfunc();
			serveFuncs[0].linkid=connid;
			serveFuncs[0].lastPageSendIndex=0;
			serveFuncs[0].lastHeaderSendIndex=0;
			serveFuncs[0].page=p->page;
			serveFuncs[0].header='\0';
			serveFuncs[0].code=p->code;
			serveFuncs[0].mime=p->mime;
			QueueFunction1(SendPage,0);
			found=0;
		}
		memset( esp01_uart_rx_buffer , '\0' , ESP01_BUFFER_SIZE );
		memset( ESP01_MainBuf , '\0' , ESP01_MainBuf_SIZE );
		newPos=0;

	}

}

/*

+IPD,0,427:GET / HTTP/1.1\r\nHost: 192.168.1.69\r\nConnection: keep-alive\r\nUpgrade-Insecure-Requests: 1\r\nUser-Agent: Mozilla/5.0 (Windows

0,CONNECT

+IPD,0,455:GET /hi HTTP/1.1
Host: 192.168.1.69
Connection: keep-alive
Cache-Control: max-age=0
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/116.0.0.0 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*'/*;q=0.8,application/signed-exchange;v=b3;q=0.7
Accept-Encoding: gzip, deflate
Accept-Language: en-US,en;q=0.9

 */

void incrementHead(){
	current_func_head++;
	if(current_func_head>=MAX_FUNC){
		current_func_head=0;
	}
}

void Process_ESP01(){
	uint32_t current_tick=HAL_GetTick();
	if(serverStatus.serverON && wifiConnStatus.WifiConnStatus==WIFI_CONNECTION_STATUS_CONNECTED_GOTIP){
		handleClient();
	}
	if((current_tick-esp01_prev_ticks_250)>250){
		esp01_prev_ticks_250=current_tick;
	}
	if((current_tick-esp01_prev_ticks_1000)>1000){
		esp01_prev_ticks_1000=current_tick;
		if(wifiConnStatus.WifiConnStatus!=WIFI_CONNECTION_STATUS_CONNECTED_GOTIP && wifiConnStatus.InProgress!=true ){
			QueueFunction(ConnectWifi);
			QueueFunction(GetConnectionIP);
		}
	}
	if((current_tick-esp01_prev_ticks_5000)>5000){
		esp01_prev_ticks_5000=current_tick;
		//QueueFunction(CheckConnectionStatus);
		//QueueFunction(GetConnectionIP);
	}
	if((current_tick-esp01_prev_ticks_10000)>10000){
		esp01_prev_ticks_10000=current_tick;
	}

	if(funcStatus[current_func_head].mfunc!=NULL){
		if(funcStatus[current_func_head].completed==false){
			int8_t ret=funcStatus[current_func_head].mfunc();
			if(ret==RESULT_ERROR){
				incrementHead();
			}
		}else{

			funcStatus[current_func_head].mfunc=NULL;
			incrementHead();
		}
	}else{
		incrementHead();
	}
}

void QueueFunction(myFunc f){
	QueueFunction1(f,0);
}
void QueueFunction1(myFunc f, uint8_t param1){

	for(uint8_t i=0;i<MAX_FUNC;i++){
		if(funcStatus[i].mfunc==f && funcStatus[i].completed==false) return; //If already same function exist and is not completed, then don't add this function again
	}
	if(current_func_tail==-1){
		current_func_tail=0;
		current_func_head=0;
	}
	funcStatus[current_func_tail].step=1;
	funcStatus[current_func_tail].mfunc=f;
	funcStatus[current_func_tail].param1=param1;
	funcStatus[current_func_tail].completed=false;
	funcStatus[current_func_tail].started_time=0;
	funcStatus[current_func_tail].timeout=1000;
	current_func_tail++;
	if(current_func_tail>=MAX_FUNC)current_func_tail=0;
}


void Debug(const char *info) {
  char CRLF[3] = "\r\n";
  HAL_UART_Transmit(_debug_uart, (uint8_t *)CRLF, strlen(CRLF), 100);
  HAL_UART_Transmit(_debug_uart, (uint8_t *)info, strlen(info), 100);
  HAL_UART_Transmit(_debug_uart, (uint8_t *)CRLF, strlen(CRLF), 100);
}


void _makeReadyForNextStep(bool incrementStep, uint16_t timeout){
	memset( esp01_uart_rx_buffer , '\0' , ESP01_BUFFER_SIZE );
	memset( ESP01_MainBuf , '\0' , ESP01_MainBuf_SIZE );
	newPos=0;
	if(incrementStep==true)funcStatus[current_func_head].step++;
	funcStatus[current_func_head].started_time=HAL_GetTick();
	funcStatus[current_func_head].timeout=timeout;
	tx_complete_fired=0;
}

char commandbuff[100] ;
void _SendCommandNonblocking(uint8_t* command,bool incrementStep, uint16_t timeout){
	_makeReadyForNextStep(incrementStep,timeout);
	char buffer[120];
	sprintf((char *)buffer, "Sending command \"%s\"",command );
	Debug(buffer);
	sprintf(commandbuff,"%s\r\n",command);
	uint8_t sz=strlen((const char*)commandbuff);
	HAL_UART_Transmit_IT(_esp01_uart,(const uint8_t*)commandbuff    , sz);
	HAL_UARTEx_ReceiveToIdle_DMA(_esp01_uart, esp01_uart_rx_buffer, ESP01_BUFFER_SIZE);
}

void _SendDataNonblocking(const char * data,uint16_t startIndex, uint16_t bytesToSend,bool incrementStep, uint16_t timeout){
	_makeReadyForNextStep(incrementStep,timeout);
	uint8_t buffer[500];
	memcpy(buffer,data+startIndex,bytesToSend);
	//uint8_t sz=strlen((const char*)data);
	HAL_UART_Transmit_IT(_esp01_uart,(const uint8_t*)buffer, bytesToSend);
	HAL_UARTEx_ReceiveToIdle_DMA(_esp01_uart, esp01_uart_rx_buffer, ESP01_BUFFER_SIZE);
	Debug("Sending data");
	Debug((char*)buffer);
}

void HAL_UART_TxCpltCallback_ESP01(UART_HandleTypeDef *huart)
{
	if (huart->Instance == _esp01_uart->Instance) {
		tx_complete_fired =1;
		HAL_UARTEx_ReceiveToIdle_DMA(_esp01_uart, esp01_uart_rx_buffer, ESP01_BUFFER_SIZE);
	}
}


void HAL_UARTEx_RxEventCallback_ESP01( UART_HandleTypeDef *huart, uint16_t Size){
	if (huart->Instance == _esp01_uart->Instance) {

		oldPos = newPos;  // Update the last position before copying new data
		if (oldPos+Size > ESP01_MainBuf_SIZE)  // If the current position + new data size is greater than the main buffer
		{
			uint16_t datatocopy = ESP01_MainBuf_SIZE-oldPos;  // find out how much space is left in the main buffer
			memcpy ((uint8_t *)ESP01_MainBuf+oldPos, esp01_uart_rx_buffer, datatocopy);  // copy data in that remaining space
			oldPos = 0;  // point to the start of the buffer
			memcpy ((uint8_t *)ESP01_MainBuf, (uint8_t *)esp01_uart_rx_buffer+datatocopy, (Size-datatocopy));  // copy the remaining data
			newPos = (Size-datatocopy);  // update the position
		}/* if the current position + new data size is less than the main buffer we will simply copy the data into the buffer and update the position */
		else
		{
			memcpy ((uint8_t *)ESP01_MainBuf+oldPos, esp01_uart_rx_buffer, Size);
			newPos = Size+oldPos;
		}
		rx_event_fired=1;
		HAL_UARTEx_ReceiveToIdle_DMA(_esp01_uart, esp01_uart_rx_buffer, ESP01_BUFFER_SIZE);
		__HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
	}

}

uint8_t esp01command[300];

char *_CheckAck(uint8_t* ack){
	char *pch1 = strstr((const char*)&ESP01_MainBuf[0],(const char*) ack);
	return pch1;
}



int8_t _CheckAck1(uint8_t* ack,bool timeout_fired,uint8_t timeout_step,uint8_t returnIfSuccess,uint8_t returnIfTimeOut){
	char *pch1 = strstr((const char*)&ESP01_MainBuf[0],(const char*) ack);
	if(pch1){
		funcStatus[current_func_head].step++;
		return returnIfSuccess;
	}else{
		if(timeout_fired){
			Debug("AT timout");
			funcStatus[current_func_head].step=timeout_step;
			return returnIfTimeOut;
		}
	}
	return RESULT_NONE;
}

#define MAX_BYTES_TO_SEND 200
int16_t getPageBytesToSend(){
	int16_t len = strlen ( serveFuncs[funcStatus[current_func_head].param1].page);
	int16_t lsi=serveFuncs[funcStatus[current_func_head].param1].lastPageSendIndex;
	int16_t bts=len-lsi;
	if(bts>MAX_BYTES_TO_SEND)bts=MAX_BYTES_TO_SEND;
	return bts;
}
int16_t getHeaderBytesToSend(){
	int16_t len = strlen ( serveFuncs[funcStatus[current_func_head].param1].header);
	int16_t lsi=serveFuncs[funcStatus[current_func_head].param1].lastHeaderSendIndex;
	int16_t bts=len-lsi;
	if(bts>MAX_BYTES_TO_SEND)bts=MAX_BYTES_TO_SEND;
	return bts;
}

int8_t SendPage(){
	uint8_t step=funcStatus[current_func_head].step;
	uint32_t tick=HAL_GetTick();
	uint8_t timeout_fired=(tick-funcStatus[current_func_head].started_time)>funcStatus[current_func_head].timeout;

	if(step==1 ){
		prepareHeader(funcStatus[current_func_head].param1);
		int16_t hbts=getHeaderBytesToSend();
		int16_t pbts=getPageBytesToSend();
		if((hbts+pbts)>0){ //there is still some bytes to send from either header or page
			char data[80];
			sprintf (data, "AT+CIPSEND=%d,%d", serveFuncs[funcStatus[current_func_head].param1].linkid, hbts>0?hbts:pbts);
			_SendCommandNonblocking((uint8_t *)data,true,2000);
		}else{
			funcStatus[current_func_head].step=5;
		}

		return RESULT_NONE;
	}

	if(step==2 && (timeout_fired || rx_event_fired) ){
		rx_event_fired=0;
		uint8_t ret= _CheckAck1((uint8_t*) "\r\nOK\r\n\r\n>",timeout_fired,1,RESULT_NONE,RESULT_ERROR);
		if(ret==RESULT_ERROR  )funcStatus[current_func_head].completed=true;
		return ret;

	}
	if(step==3   ){
		int16_t hbts=getHeaderBytesToSend();
		int16_t pbts=getPageBytesToSend();
		if((hbts+pbts)>0){ //there is still some bytes to send from either header or page
			if(hbts>0){
				_SendDataNonblocking((const char*)serveFuncs[funcStatus[current_func_head].param1].header,serveFuncs[funcStatus[current_func_head].param1].lastHeaderSendIndex,hbts,  true,1000);
			}else{
				_SendDataNonblocking((const char*)serveFuncs[funcStatus[current_func_head].param1].page,serveFuncs[funcStatus[current_func_head].param1].lastPageSendIndex,pbts,  true,1000);
			}

		}

		return RESULT_NONE;
	}
	if(step==4 && (timeout_fired || rx_event_fired) ){
		rx_event_fired=0;
		uint8_t ret= _CheckAck1((uint8_t*) "\r\nSEND OK\r\n",timeout_fired,1,RESULT_OK,RESULT_ERROR);
		if(ret==RESULT_OK  ){
			int16_t hbts=getHeaderBytesToSend();
			int16_t pbts=getPageBytesToSend();
			if((hbts+pbts)>0){ //there is still some bytes to send from either header or page
				if(hbts>0){
					serveFuncs[funcStatus[current_func_head].param1].lastHeaderSendIndex+=hbts;
				}else{
					serveFuncs[funcStatus[current_func_head].param1].lastPageSendIndex+=pbts;
				}
			}

			funcStatus[current_func_head].step=1;
		}
		return ret;

	}
	if(step==5   ){
		char data[80];
		sprintf (data, "AT+CIPCLOSE=%d", serveFuncs[funcStatus[current_func_head].param1].linkid);
		_SendCommandNonblocking((uint8_t*)data,true,1000);
		funcStatus[current_func_head].completed=true;
		return RESULT_NONE;
	}
	return RESULT_NONE;
}

int8_t StartServer( ){
	uint8_t step=funcStatus[current_func_head].step;
	uint32_t tick=HAL_GetTick();
	uint8_t timeout_fired=(tick-funcStatus[current_func_head].started_time)>funcStatus[current_func_head].timeout;

	if(step==1 ){
		_SendCommandNonblocking((uint8_t *)"AT+CIPMUX=1",true,1000);
		return RESULT_NONE;
	}

	if(step==2 && (timeout_fired || rx_event_fired) ){
		rx_event_fired=0;
		uint8_t ret= _CheckAck1((uint8_t*) "OK\r\n",timeout_fired,1,RESULT_NONE,RESULT_ERROR);
		if(ret==RESULT_ERROR)funcStatus[current_func_head].completed=true;
		return ret;

	}
	if(step==3   ){
		sprintf((char *)esp01command, "AT+CIPSERVER=1,%d", serverStatus.port);
		_SendCommandNonblocking((uint8_t *)esp01command,true,1000);
		return RESULT_NONE;
	}

	if(step==4 && (timeout_fired || rx_event_fired) ){
		rx_event_fired=0;
		uint8_t ret= _CheckAck1((uint8_t*) "OK\r\n",timeout_fired,1,RESULT_NONE,RESULT_ERROR);
		if(ret==RESULT_ERROR)funcStatus[current_func_head].completed=true;
		return ret;
	}
	if(step==5   ){
		sprintf((char *)esp01command, "AT+CIPSTO=%d", serverStatus.timeout);
		_SendCommandNonblocking((uint8_t *)esp01command,true,1000);
		return RESULT_OK;
	}
	if(step==6 && (timeout_fired || rx_event_fired) ){
		rx_event_fired=0;
		uint8_t ret= _CheckAck1((uint8_t*) "OK\r\n",timeout_fired,5,RESULT_OK,RESULT_ERROR);
		if(ret==RESULT_OK || ret==RESULT_ERROR)funcStatus[current_func_head].completed=true;

		return ret;
	}
	return RESULT_NONE;
}

int8_t Init(){
	uint8_t step=funcStatus[current_func_head].step;
	uint32_t tick=HAL_GetTick();
	uint8_t timeout_fired=(tick-funcStatus[current_func_head].started_time)>funcStatus[current_func_head].timeout;
	if(step==1){
		_SendCommandNonblocking((uint8_t *)"AT+CWQAP",true,500); //Disconnect from AP
		return RESULT_NONE;
	}

	if(step==2 && (timeout_fired || rx_event_fired) ){
		rx_event_fired=0;
		uint8_t ret= _CheckAck1((uint8_t*) "\r\nOK\r\n",timeout_fired,1,RESULT_OK,RESULT_ERROR); //WIFI DISCONNECT\r\n
		if(ret==RESULT_OK || ret==RESULT_ERROR)funcStatus[current_func_head].completed=true;
		return ret;
	}

	return RESULT_NONE;
}

int parseString(const char * token,const char * value, char* target){
	char * pch1=_CheckAck((uint8_t*) token);
	if(pch1){
		return sscanf(pch1, value, target );
	}
	return 0;
}

int8_t GetConnectionIP(){
	uint8_t step=funcStatus[current_func_head].step;
	uint32_t tick=HAL_GetTick();
	uint8_t timeout_fired=(tick-funcStatus[current_func_head].started_time)>funcStatus[current_func_head].timeout;
	if(step==1 ){
		_SendCommandNonblocking((uint8_t *)"AT+CIPSTA?",true,1000);
		return RESULT_NONE;
	}
	if(step==2 && (timeout_fired || rx_event_fired) ){
		rx_event_fired=0;
		char result[16];

		if(  parseString("+CIPSTA:ip:","%*[^\"]\"%31[^\"]\"",result)>0){
			strcpy(wifiConnStatus.ip,result);
			if( parseString("+CIPSTA:gateway:","%*[^\"]\"%31[^\"]\"",result) >0){
				strcpy(wifiConnStatus.gateway,result);
				funcStatus[current_func_head].completed=true;
				return RESULT_OK;
			}
		}
		if(timeout_fired){
			funcStatus[current_func_head].completed=true;
			return RESULT_ERROR;
		}
		return RESULT_NONE;

	}
	return RESULT_NONE;
}

int8_t CheckWifiConnectionStatus(){
	uint8_t step=funcStatus[current_func_head].step;
	uint32_t tick=HAL_GetTick();
	uint8_t timeout_fired=(tick-funcStatus[current_func_head].started_time)>funcStatus[current_func_head].timeout;
	if(step==1){
		_SendCommandNonblocking((uint8_t *)"AT+CWSTATE?",true,1000);
		return RESULT_NONE;
	}

	if(step==2 && (timeout_fired || rx_event_fired) ){
		rx_event_fired=0;

		char * pch1=_CheckAck((uint8_t*) "+CWSTATE:");
		if(pch1){
			int status=0;
			sscanf(pch1, "+CWSTATE:%d", &status );
			wifiConnStatus.WifiConnStatus=(uint8_t)status;
			funcStatus[current_func_head].completed=true;
			return RESULT_OK;
		}
		if(timeout_fired){
			wifiConnStatus.WifiConnStatus=WIFI_CONNECTION_STATUS_DISCONNECTED;
			memset(wifiConnStatus.ip,'\0',sizeof(wifiConnStatus.ip));
			memset(wifiConnStatus.gateway,'\0',sizeof(wifiConnStatus.gateway));
			funcStatus[current_func_head].completed=true;
			return RESULT_ERROR;
		}

	}
	return RESULT_NONE;
}

int8_t CheckNetConnectionStatus(){
	uint8_t step=funcStatus[current_func_head].step;
	uint32_t tick=HAL_GetTick();
	uint8_t timeout_fired=(tick-funcStatus[current_func_head].started_time)>funcStatus[current_func_head].timeout;
	if(step==1){
		_SendCommandNonblocking((uint8_t *)"AT+CIPSTATE?",true,1000);
		return RESULT_NONE;
	}

	if(step==2 && (timeout_fired || rx_event_fired) ){
		rx_event_fired=0;

		char * pch1=_CheckAck((uint8_t*) "+CIPSTATE:");
		if(pch1){
			int status=0;
			sscanf(pch1, "+CIPSTATE:%d", &status );
			funcStatus[current_func_head].completed=true;
			return RESULT_OK;
		}
		if(timeout_fired){

			funcStatus[current_func_head].completed=true;
			return RESULT_ERROR;
		}

	}
	return RESULT_NONE;
}



int8_t ConnectWifi(){
	uint32_t tick=HAL_GetTick();
	wifiConnStatus.InProgress=true;
	uint8_t step=funcStatus[current_func_head].step;
	bool timeout_fired=(tick-funcStatus[current_func_head].started_time)>funcStatus[current_func_head].timeout;
	if(step==1 ){
		_SendCommandNonblocking((uint8_t *)"AT+CWMODE=1",true,1000);//Change mode to Station mode ‣ 2: SoftAP mode 3: SoftAP+Station mode
		return RESULT_NONE;
	}
	if(step==2 || (timeout_fired || rx_event_fired)){
		rx_event_fired=0;
		uint8_t ret= _CheckAck1((uint8_t*) "OK\r\n",timeout_fired,1,RESULT_NONE,RESULT_ERROR);
		if(ret==RESULT_ERROR){
			funcStatus[current_func_head].completed=true;
			wifiConnStatus.InProgress=false;
		}
		return ret;

	}
	if(step==3 ){
		sprintf((char *)esp01command, "AT+CWJAP=\"%s\",\"%s\"", ssid,password);
		_SendCommandNonblocking((uint8_t *)esp01command,true,1000);
		return RESULT_NONE;
	}

	if(step==4 || (timeout_fired || rx_event_fired)){
		rx_event_fired=0;
		funcStatus[current_func_head].timeout=5000;
		uint8_t ret= _CheckAck1((uint8_t*) "WIFI CONNECTED",timeout_fired,5,RESULT_NONE,RESULT_ERROR);
		if(ret==RESULT_ERROR){
			funcStatus[current_func_head].completed=true;
			wifiConnStatus.InProgress=false;
		}
		return ret;
	}
	if(step==5 || (timeout_fired || rx_event_fired)){
		rx_event_fired=0;
		uint8_t ret=_CheckAck1((uint8_t*) "WIFI GOT IP",timeout_fired,5,RESULT_NONE,RESULT_ERROR);
		if(ret==RESULT_ERROR){
			funcStatus[current_func_head].completed=true;
			wifiConnStatus.InProgress=false;
		}
		return ret;
	}
	if(step==6 || (timeout_fired || rx_event_fired)){
		rx_event_fired=0;
		uint8_t ret= _CheckAck1((uint8_t*) "OK\r\n",timeout_fired,3,RESULT_OK,RESULT_ERROR);
		if(ret==RESULT_OK || ret==RESULT_ERROR){
			funcStatus[current_func_head].completed=true;
			wifiConnStatus.InProgress=false;
		}
		return ret;
	}

	return RESULT_NONE;
}
