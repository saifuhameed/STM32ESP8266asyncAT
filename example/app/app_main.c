/*
 * app_main.c
 *
 *  Created on: Sep 14, 2023
 *      Author: Saif
 */


#include "main.h"
#include "esp01.h"
#include "ssd1306.h"
#include <time.h>
#include <stdio.h>


extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

extern RTC_HandleTypeDef hrtc;

uint16_t uptime_days;
uint8_t uptime_hrs;
uint8_t uptime_mins;
uint8_t uptime_secs;
uint32_t uptime=0;

RTC_TimeTypeDef _time = {0};
RTC_DateTypeDef _date = {0};
time_t timestamp;
struct tm currTime;
WifiConnectionStatus_t ws;

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	HAL_UART_TxCpltCallback_ESP01(huart);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	HAL_UARTEx_RxEventCallback_ESP01(huart,Size);
}


void updateUptime(){
	uint32_t m_total,h_total;
	m_total=uptime/60;
	uptime_secs=uptime%60;

	h_total=m_total/60;
	uptime_mins=m_total%60;

	uptime_days=h_total/24;
	uptime_hrs=h_total%24;


}


uint8_t validatedatetime(uint8_t hrs,uint8_t minutes,uint8_t seconds,uint8_t year,uint8_t month,uint8_t date){

	if(hrs>59){
		return 0;
	}
	if(minutes>59){
		return 0;
	}
	if(seconds>59){
		return 0;
	}
	if(year>99){
		return 0;
	}
	if(month<1 || month >12){
		return 0;
	}
	if(date<1 || date >31){
		return 0;
	}
	return 1;
}

void display_status(){


	char buffer[24];
	uint8_t current_y=0;
	sprintf(buffer, "%02d:%02d:%02d %02d/%02d/%02d", _time.Hours,_time.Minutes,_time.Seconds, _date.Date,_date.Month,_date.Year  );
	SSD1306_GotoXY(1,current_y);
	SSD1306_Puts(buffer, &Font_7x10, 1);
	current_y=current_y+11;
	SSD1306_GotoXY(0,current_y);
	sprintf(buffer, "%03dd,%02dh:%02dm:%02ds", uptime_days,uptime_hrs,uptime_mins,uptime_secs);
	SSD1306_Puts(buffer, &Font_7x10, 1);
	current_y=current_y+11;
	SSD1306_GotoXY(0,current_y);
	sprintf(buffer, "Wi-fi %s", (ws.WifiConnStatus==WIFI_CONNECTION_STATUS_CONNECTED_GOTIP)?"connected   ":"disconnected");
	SSD1306_Puts(buffer, &Font_7x10, 1);

	current_y=current_y+11;
	SSD1306_GotoXY(0,current_y);
	SSD1306_Puts("I:                   ", &Font_7x10, 1);
	sprintf(buffer, "I:%s",ws.ip );
	SSD1306_GotoXY(0,current_y);
	SSD1306_Puts(buffer, &Font_7x10, 1);

	current_y=current_y+11;
	SSD1306_GotoXY(0,current_y);
	SSD1306_Puts("G:                   ", &Font_7x10, 1);
	snprintf(buffer,19, "G:%s",ws.gateway );
	SSD1306_GotoXY(0,current_y);
	SSD1306_Puts(buffer, &Font_7x10, 1);
	uint8_t ret=SSD1306_UpdateScreen(); //display
	if(ret!=0)SSD1306_Init();
}

uint8_t setDateTime(uint8_t hrs,uint8_t minutes,uint8_t seconds,uint8_t year,uint8_t month,uint8_t date){

	if(validatedatetime( hrs, minutes, seconds, year, month, date)){
		RTC_TimeTypeDef sTime = {0};
		sTime.Hours = hrs;
		sTime.Minutes = minutes;
		sTime.Seconds = seconds;
		if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
		{
			Error_Handler();
			return 0;
		}

		RTC_DateTypeDef sDate = {0};
		sDate.Date = date;
		sDate.Month = month;
		sDate.Year = year;
		if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
		{
			return 0;
			Error_Handler();
		}
		return 1;
	}else{
		return 0;
	}
}
const char page[]="<!DOCTYPE html><html><head><link rel=\"icon\" href=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12P4//8/AAX+Av7czFnnAAAAAElFTkSuQmCC\"></head><form action=\"/LED\" method=\"POST\"><input type=\"submit\" value=\"Toggle LED\"></form><svg height=\"100\" width=\"100\">"
			"<circle cx=\"50\" cy=\"50\" r=\"20\" stroke=\"black\" stroke-width=\"3\" fill=\"%s\" /></svg></html>";

char page2[500];
const char favicon[]="<link rel=\"icon\" href=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVQI12P4//8/AAX+Av7czFnnAAAAAElFTkSuQmCC\">";

void handleRoot(){
	uint8_t v=HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13);

	sprintf((char *)page2 , page,v?"#006605":"#1dff0c" );

	serverSendESP01(200, "text/html",page2 );
}
void handleStatus(){
	serverSendESP01(200, "text/html","<!DOCTYPE html><html><h1>This is the status page!</h1></html>");
}
void handleFavicon(){
	serverSendESP01(200, "text/html",favicon);
}
void handlePageNotFound(){
	serverSendESP01(404, "text/html","<h1>Page Not Found 404 Error!</h1>");
}
void handleLED() {                          // If a POST request is made to URI /LED
	HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);      // Change the state of the LED
	serverRedirectESP01("/");
	//serverSendESP01(200, "text/html","HTTP/1.1 303 See Other\r\nContent-Type: text/html\r\nLocation: /\r\n");

	//server.sendHeader("Location","/");        // Add a header to respond with a new location for the browser to go to the home page again
	//server.send(303);                         // Send it back to the browser with an HTTP status 303 (See Other) to redirect
}

void app_setup(){
	hrtc.Instance = RTC;
	  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
	  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
	  if (HAL_RTC_Init(&hrtc) != HAL_OK)
	  {
		Error_Handler();
	  }
	SSD1306_Init();
	SSD1306_Fill(SSD1306_COLOR_BLACK);
	Initialize_ESP01("Vodafone-2.4G-4xMb","saifu313");
	onServer_ESP01("/", HTTP_GET, handleRoot);     // Call the 'handleRoot' function when a client requests URI "/"
	onServer_ESP01("/LED", HTTP_POST, handleLED);  // Call the 'handleLED' function when a POST request is made to URI "/LED"
	onServer_ESP01( "/status",HTTP_GET, handleStatus);
	onServer_ESP01( "/favicon.ico",HTTP_GET, handleFavicon);
	onServerPageNotFound_ESP01(handlePageNotFound);
	StartServer_ESP01(80,120); //port, timeout

}

uint32_t prev_ticks_250=0;
uint32_t prev_ticks_1000=0;
uint32_t prev_ticks_5000=0;
uint32_t prev_ticks_10000=0;
uint32_t prev_ticks_30000=0;
uint32_t prev_ticks_60000=0;

void app_main() {
	uint32_t current_tick=HAL_GetTick();

	Process_ESP01();

	if((current_tick-prev_ticks_250)>250){
		prev_ticks_250=current_tick;
		//HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
		display_status();
	}

	if((current_tick-prev_ticks_1000)>1000){
		prev_ticks_1000=current_tick;

		ws=getWIfiStatus();

		uptime++;
		updateUptime();
		//char buffer[100];
		//sprintf(buffer, "%d:%d:%d\r\n", _time.Hours, _time.Minutes,_time.Seconds );
		//HAL_UART_Transmit(&huart2,(uint8_t *)buffer, strlen(buffer),100);
	}
	if((current_tick-prev_ticks_5000)>5000){
		prev_ticks_5000=current_tick;

	}

	if (HAL_RTC_GetTime(&hrtc, &_time, RTC_FORMAT_BIN) != HAL_OK)
	{
	  Error_Handler();
	}else{

	}

	if (HAL_RTC_GetDate(&hrtc, &_date, RTC_FORMAT_BIN) != HAL_OK)
	{
	  Error_Handler();
	}else{

	}

	currTime.tm_year = _date.Year + 100;  // In fact: 2000 + 18 - 1900
	currTime.tm_mday = _date.Date;
	currTime.tm_mon  = _date.Month - 1;

	currTime.tm_hour = _time.Hours;
	currTime.tm_min  = _time.Minutes;
	currTime.tm_sec  = _time.Seconds;

}
