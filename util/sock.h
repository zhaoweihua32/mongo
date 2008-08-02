// sock.h

/**
*    Copyright (C) 2008 10gen Inc.
*  
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*  
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*  
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
 
#include <stdio.h>
#include <sstream>
#include "goodies.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
inline int getLastError() { return WSAGetLastError(); }
inline void disableNagle(int sock) { 
	int x = 1;
	if( setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &x, sizeof(x)) ) 
		cout << "ERROR: disableNagle failed" << endl;
}
inline void prebindOptions( int sock ){
}
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
inline void closesocket(int s) { close(s); }
const int INVALID_SOCKET = -1;
typedef int SOCKET;
//#define h_errno errno
inline int getLastError() { return errno; }
inline void disableNagle(int sock) { 
  int x = 1;

  #ifdef SOL_TCP
  int level = SOL_TCP;
  #else
  int level = SOL_SOCKET;
  #endif

  if( setsockopt(sock, level, TCP_NODELAY, (char *) &x, sizeof(x)) )
    log() << "ERROR: disableNagle failed" << endl; 

}
inline void prebindOptions( int sock ){
  DEV log() << "doing prebind option" << endl;
  int x = 1;
  if ( setsockopt( sock , SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x)) < 0 )
    cout << "Failed to set socket opt, SO_REUSEADDR" << endl;
}


#endif

// .empty() if err
inline string hostbyname_nonreentrant(const char *hostname) {
	struct hostent *h;
	h = gethostbyname(hostname);
	if( h == 0 ) return "";
	return inet_ntoa( *((struct in_addr *)(h->h_addr)) );
}

struct SockAddr {
	SockAddr() { addressSize = sizeof(sockaddr_in); memset(&sa, 0, sizeof(sa)); }
	SockAddr(int sourcePort); /* listener side */ 
	SockAddr(const char *ip, int port); /* EndPoint (remote) side, or if you want to specify which interface locally */

	struct sockaddr_in sa;
	socklen_t addressSize;

	bool isLocalHost() const { 
#if defined(_WIN32)
		return sa.sin_addr.S_un.S_addr == 0x100007f;
#else
		return sa.sin_addr.s_addr == 0x100007f; 
#endif
	}

	string toString() { 
		stringstream out;
		out << inet_ntoa(sa.sin_addr) << ':' 
			<< sa.sin_port;
		return out.str();
	}

	unsigned getPort() { return sa.sin_port; }

	bool operator==(const SockAddr& r) const { 
		return sa.sin_addr.s_addr == r.sa.sin_addr.s_addr &&
			sa.sin_port == r.sa.sin_port;
	}
	bool operator!=(const SockAddr& r) const { return !(*this == r); }
	bool operator<(const SockAddr& r) const { 
		if( sa.sin_port >= r.sa.sin_port )
			return false;
		return sa.sin_addr.s_addr < r.sa.sin_addr.s_addr;
	}
};

const int MaxMTU = 16384;

class UDPConnection {
public:
	UDPConnection() { sock = 0; }
	~UDPConnection() { if( sock ) { closesocket(sock); sock = 0; } }
	bool init(const SockAddr& myAddr);
	int recvfrom(char *buf, int len, SockAddr& sender);
	int sendto(char *buf, int len, const SockAddr& EndPoint);
	int mtu(const SockAddr& sa) { 
		return sa.isLocalHost() ? 16384 : 1480;
	}

	SOCKET sock;
};

inline int UDPConnection::recvfrom(char *buf, int len, SockAddr& sender) {
	return ::recvfrom(sock, buf, len, 0, (sockaddr *) &sender.sa, &sender.addressSize);
}

inline int UDPConnection::sendto(char *buf, int len, const SockAddr& EndPoint) {
	if( 0 && rand() < (RAND_MAX>>4) ) { 
		cout << " NOTSENT ";
		//		cout << curTimeMillis() << " .TEST: NOT SENDING PACKET" << endl;
		return 0;
	}
	return ::sendto(sock, buf, len, 0, (sockaddr *) &EndPoint.sa, EndPoint.addressSize);
}

inline bool UDPConnection::init(const SockAddr& myAddr) {
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if( sock == INVALID_SOCKET ) {
		cout << "invalid socket? " << errno << endl;
		return false;
	}
	//cout << sizeof(sockaddr_in) << ' ' << myAddr.addressSize << endl;
	if( bind(sock, (sockaddr *) &myAddr.sa, myAddr.addressSize) != 0 ) { 
		cout << "udp init failed" << endl;
		closesocket(sock);
		sock = 0;
		return false;
	}
	socklen_t optLen;
	int rcvbuf;
	if (getsockopt(sock,
		SOL_SOCKET, 
		SO_RCVBUF, 
		(char*)&rcvbuf, 
		&optLen) != -1)
		cout << "SO_RCVBUF:" << rcvbuf << endl;
	return true;
}

inline SockAddr::SockAddr(int sourcePort) {
	memset(sa.sin_zero, 0, sizeof(sa.sin_zero));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(sourcePort);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	addressSize = sizeof(sa);
}

inline SockAddr::SockAddr(const char *ip, int port) {
	memset(sa.sin_zero, 0, sizeof(sa.sin_zero));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(ip);
	addressSize = sizeof(sa);
}
