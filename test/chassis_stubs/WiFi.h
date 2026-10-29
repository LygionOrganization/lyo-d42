#pragma once
#define WL_CONNECTED 3
struct Wifi{int status(){return 0;}int RSSI(){return -50;}int softAPgetStationNum(){return 1;}};inline Wifi WiFi;
