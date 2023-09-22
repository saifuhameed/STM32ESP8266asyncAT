/*
 * esp01util.h
 *
 *  Created on: Sep 20, 2023
 *      Author: Saif
 */

#ifndef SRC_ESP01_ESP8266UTIL_H_
#define SRC_ESP01_ESP8266UTIL_H_




char * responseCodeToString(const int code) {
   char * r;

    switch (code)
    {
    case 100:
    	r = "Continue";
        break;
    case 101:
        r = "Switching Protocols";
        break;
    case 200:
        r = "OK";
        break;
    case 201:
        r = "Created";
        break;
    case 202:
        r = "Accepted";
        break;
    case 203:
        r = "Non-Authoritative Information";
        break;
    case 204:
        r = "No Content";
        break;
    case 205:
        r = "Reset Content";
        break;
    case 206:
        r = "Partial Content";
        break;
    case 300:
        r = "Multiple Choices";
        break;
    case 301:
        r = "Moved Permanently";
        break;
    case 302:
        r = "Found";
        break;
    case 303:
        r = "See Other";
        break;
    case 304:
        r = "Not Modified";
        break;
    case 305:
        r = "Use Proxy";
        break;
    case 307:
        r = "Temporary Redirect";
        break;
    case 400:
        r = "Bad Request";
        break;
    case 401:
        r = "Unauthorized";
        break;
    case 402:
        r = "Payment Required";
        break;
    case 403:
        r = "Forbidden";
        break;
    case 404:
        r = "Not Found";
        break;
    case 405:
        r = "Method Not Allowed";
        break;
    case 406:
        r = "Not Acceptable";
        break;
    case 407:
        r = "Proxy Authentication Required";
        break;
    case 408:
        r = "Request Timeout";
        break;
    case 409:
        r = "Conflict";
        break;
    case 410:
        r = "Gone";
        break;
    case 411:
        r = "Length Required";
        break;
    case 412:
        r = "Precondition Failed";
        break;
    case 413:
        r = "Request Entity Too Large";
        break;
    case 414:
        r = "URI Too Long";
        break;
    case 415:
        r = "Unsupported Media Type";
        break;
    case 416:
        r = "Range not satisfiable";
        break;
    case 417:
        r = "Expectation Failed";
        break;
    case 500:
        r = "Internal Server Error";
        break;
    case 501:
        r = "Not Implemented";
        break;
    case 502:
        r = "Bad Gateway";
        break;
    case 503:
        r = "Service Unavailable";
        break;
    case 504:
        r = "Gateway Timeout";
        break;
    case 505:
        r = "HTTP Version not supported";
        break;
    default:
        r = "";
        break;
    }
    return r;
}


#endif /* SRC_ESP01_ESP8266UTIL_H_ */
