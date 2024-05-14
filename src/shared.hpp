#pragma once

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <ws2def.h>
	#include <ws2ipdef.h>
	#pragma comment (lib, "Ws2_32.lib")
	#pragma comment (lib, "Mswsock.lib")
	#pragma comment (lib, "AdvApi32.lib")
#else 
//todo: linux support
#endif // _WIN32

#ifdef _DEBUG
	#define debugf(a, ...) printf(a,  __VA_ARGS__)
	#define debugfl(a, ...) printf("Line %i : " a, __LINE__, __VA_ARGS__)
#else
	#define debugf(a, ...) __noop
#endif // _DEBUG

#include <iostream>

constexpr const char* serverPort_c = "16969";

enum MSG_TYPE : uint64_t {
	CONNECTION_KEEP, CONNECTION_END, SEND_TRY, SEND_FILE_DATA, SEND_RETRY, MSG_TYPE_MAX
};

struct header_t {
	uint64_t msgSize;
	MSG_TYPE msgType;
};

inline bool initWinSock(void) {
	int32_t result;
	WSADATA wsaData;
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		fprintf(stderr, "Couldn't initialize winsock!\n");
		return false;
	}
	return true;
}

inline void pushHeader(char* _buf, header_t& _header, MSG_TYPE _type) {
	ZeroMemory(_buf, _header.msgSize); // eventually sendbuflen_c 
	_header = {};
	_header.msgSize = 16;
	_header.msgType = CONNECTION_END;
	memcpy(_buf, &_header, sizeof(_header));
	return;
}