/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2015 Live Networks, Inc.  All rights reserved.
// A generic media server class, used to implement a RTSP server, and any other server that uses
//  "ServerMediaSession" objects to describe media to be served.
// Implementation

#include "GenericMediaServer.hh"
#include <GroupsockHelper.hh>
#if defined(__WIN32__) || defined(_WIN32) || defined(_QNX4)
#define snprintf _snprintf
#endif

////////// GenericMediaServer implementation //////////

void GenericMediaServer::addServerMediaSession(ServerMediaSession* serverMediaSession) {
  if (serverMediaSession == NULL) return;
  
  char const* sessionName = serverMediaSession->streamName();
  if (sessionName == NULL) sessionName = "";
  removeServerMediaSession(sessionName); // in case an existing "ServerMediaSession" with this name already exists
  
  fServerMediaSessions->Add(sessionName, (void*)serverMediaSession);
}

ServerMediaSession* GenericMediaServer
::lookupServerMediaSession(char const* streamName, Boolean /*isFirstLookupInSession*/) {
  // Default implementation:
  return (ServerMediaSession*)(fServerMediaSessions->Lookup(streamName));
}

void GenericMediaServer::removeServerMediaSession(ServerMediaSession* serverMediaSession) {
  if (serverMediaSession == NULL) return;
  
  fServerMediaSessions->Remove(serverMediaSession->streamName());
  if (serverMediaSession->referenceCount() == 0) {
    Medium::close(serverMediaSession);
  } else {
    serverMediaSession->deleteWhenUnreferenced() = True;
  }
}

void GenericMediaServer::removeServerMediaSession(char const* streamName) {
  removeServerMediaSession((ServerMediaSession*)(fServerMediaSessions->Lookup(streamName)));
}

void GenericMediaServer::closeAllClientSessionsForServerMediaSession(ServerMediaSession* serverMediaSession) {
  if (serverMediaSession == NULL) return;
  
  HashTable::Iterator* iter = HashTable::Iterator::create(*fClientSessions);
  GenericMediaServer::ClientSession* clientSession;
  char const* key; // dummy
  while ((clientSession = (GenericMediaServer::ClientSession*)(iter->next(key))) != NULL) {
    if (clientSession->fOurServerMediaSession == serverMediaSession) {
      delete clientSession;
    }
  }
  delete iter;
}

void GenericMediaServer::closeAllClientSessionsForServerMediaSession(char const* streamName) {
  closeAllClientSessionsForServerMediaSession((ServerMediaSession*)(fServerMediaSessions->Lookup(streamName)));
}

void GenericMediaServer::deleteServerMediaSession(ServerMediaSession* serverMediaSession) {
  if (serverMediaSession == NULL) return;
  
  closeAllClientSessionsForServerMediaSession(serverMediaSession);
  removeServerMediaSession(serverMediaSession);
}

void GenericMediaServer::deleteServerMediaSession(char const* streamName) {
  deleteServerMediaSession((ServerMediaSession*)(fServerMediaSessions->Lookup(streamName)));
}

GenericMediaServer
::GenericMediaServer(UsageEnvironment& env, int ourSocket, Port ourPort,
		     unsigned reclamationSeconds)
  : Medium(env),
    fServerSocket(ourSocket), fServerPort(ourPort), fReclamationSeconds(reclamationSeconds),
    fServerMediaSessions(HashTable::create(STRING_HASH_KEYS)),
    fClientConnections(HashTable::create(ONE_WORD_HASH_KEYS)),
    fClientSessions(HashTable::create(STRING_HASH_KEYS)) {
  ignoreSigPipeOnSocket(fServerSocket); // so that clients on the same host that are killed don't also kill us
  
  // Arrange to handle connections from others:
  env.taskScheduler().turnOnBackgroundReadHandling(fServerSocket, incomingConnectionHandler, this);     //设置 incomingConnectionHandler 回调函数
}

GenericMediaServer::~GenericMediaServer() {
  // Turn off background read handling:
  envir().taskScheduler().turnOffBackgroundReadHandling(fServerSocket);
  ::closeSocket(fServerSocket);
}

void GenericMediaServer::cleanup() {
  // This member function must be called in the destructor of any subclass of
  //"GenericMediaServer".  (We don't call this in the destructor of "GenericMediaServer" itself,
  // because by that time, the subclass destructor will already have been called, and this may
  // affect (break) the destruction of the "ClientSession" and "ClientConnection" objects, which
  // themselves will have been subclassed.)

  // Close all client session objects:
  GenericMediaServer::ClientSession* clientSession;
  while ((clientSession = (GenericMediaServer::ClientSession*)fClientSessions->getFirst()) != NULL) {
    delete clientSession;
  }
  delete fClientSessions;
  
  // Close all client connection objects:
  GenericMediaServer::ClientConnection* connection;
  while ((connection = (GenericMediaServer::ClientConnection*)fClientConnections->getFirst()) != NULL) {
    delete connection;
  }
  delete fClientConnections;
  
  // Delete all server media sessions
  ServerMediaSession* serverMediaSession;
  while ((serverMediaSession = (ServerMediaSession*)fServerMediaSessions->getFirst()) != NULL) {
    removeServerMediaSession(serverMediaSession); // will delete it, because it no longer has any 'client session' objects using it
  }
  delete fServerMediaSessions;
}

#define LISTEN_BACKLOG_SIZE 20

int GenericMediaServer::setUpOurSocket(UsageEnvironment& env, Port& ourPort) {
  int ourSocket = -1;
  
  do {
    // The following statement is enabled by default.
    // Don't disable it (by defining ALLOW_SERVER_PORT_REUSE) unless you know what you're doing.
#if !defined(ALLOW_SERVER_PORT_REUSE) && !defined(ALLOW_RTSP_SERVER_PORT_REUSE)
    // ALLOW_RTSP_SERVER_PORT_REUSE is for backwards-compatibility #####
    NoReuse dummy(env); // Don't use this socket if there's already a local server using it
#endif
    
    ourSocket = setupStreamSocket(env, ourPort);
    if (ourSocket < 0) break;
    
    // Make sure we have a big send buffer:
    if (!increaseSendBufferTo(env, ourSocket, 50*1024)) break;
    
    // Allow multiple simultaneous connections:
    if (listen(ourSocket, LISTEN_BACKLOG_SIZE) < 0) {
      env.setResultErrMsg("listen() failed: ");
      break;
    }
    
    if (ourPort.num() == 0) {
      // bind() will have chosen a port for us; return it also:
      if (!getSourcePort(env, ourSocket, ourPort)) break;
    }
    
    return ourSocket;
  } while (0);
  
  if (ourSocket != -1) ::closeSocket(ourSocket);
  return -1;
}

void GenericMediaServer::incomingConnectionHandler(void* instance, int /*mask*/) {  //供 env.taskScheduler() 回调的
  GenericMediaServer* server = (GenericMediaServer*)instance;
  server->incomingConnectionHandler();  //调用下面的 incomingConnectionHandler() 重写方法
}
void GenericMediaServer::incomingConnectionHandler() {      
  incomingConnectionHandlerOnSocket(fServerSocket);     
}

/**
 *  当收到客户的连接时需记录下代表客户端的新socket，以后用这个socket与这个客户通讯。
 *  每个客户将来会对应一个rtp会话，而且各客户的RTSP请求只控制自己的rtp会话；
 *  
 *  具体作用是：
 *      accept接受客户端的socket连接；
 *      然后设置clientSocket的属性,这里需要注意,我们在建立服务端socket时已经对服务端socket设置了非阻塞属性,这个地方又要设置accept后的clientSecket的属性;
 **/
void GenericMediaServer::incomingConnectionHandlerOnSocket(int serverSocket) {
  struct sockaddr_in clientAddr;
  SOCKLEN_T clientAddrLen = sizeof clientAddr;
  int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, (socklen_t*)&clientAddrLen);
  if (clientSocket < 0) {
    int err = envir().getErrno();
    if (err != EWOULDBLOCK) {
      envir().setResultErrMsg("accept() failed: ");
    }
    return;
  }
  ignoreSigPipeOnSocket(clientSocket); // so that clients on the same host that are killed don't also kill us
  makeSocketNonBlocking(clientSocket);  //设置为 非阻塞I/O ，当不可操作时，直接返回错误信息，而不是一直阻塞等待
  increaseSendBufferTo(envir(), clientSocket, 50*1024);
  
#ifdef DEBUG
  envir() << "accept()ed connection from " << AddressString(clientAddr).val() << "\n";
#endif
  
  // Create a new object for handling this connection:
  (void)createNewClientConnection(clientSocket, clientAddr);
}


////////// GenericMediaServer::ClientConnection implementation //////////

GenericMediaServer::ClientConnection
::ClientConnection(GenericMediaServer& ourServer, int clientSocket, struct sockaddr_in clientAddr)
  : fOurServer(ourServer), fOurSocket(clientSocket), fClientAddr(clientAddr) {	////首先对RTSPServer的成员变量进行赋值
  // Add ourself to our 'client connections' table:
  fOurServer.fClientConnections->Add((char const*)this, this);
  
  // Arrange to handle incoming requests:
  resetRequestBuffer();
  envir().taskScheduler()
    .setBackgroundHandling(fOurSocket, SOCKET_READABLE|SOCKET_EXCEPTION, incomingRequestHandler, this);  //setBackgroundHandling 函数用来处理fClientInputSocket socket 上收到数据，或异常时，调用incomingRequestHandler 回调函数;
}

GenericMediaServer::ClientConnection::~ClientConnection() {
  // Remove ourself from the server's 'client connections' hash table before we go:
  fOurServer.fClientConnections->Remove((char const*)this);
  
  closeSockets();
}

void GenericMediaServer::ClientConnection::closeSockets() {
  // Turn off background handling on our socket:
  envir().taskScheduler().disableBackgroundHandling(fOurSocket);
  ::closeSocket(fOurSocket);

  fOurSocket = -1;
}

void GenericMediaServer::ClientConnection::incomingRequestHandler(void* instance, int /*mask*/) { 	//envir().taskScheduler() 的回调，用于处理client请求
  ClientConnection* connection = (ClientConnection*)instance;
  connection->incomingRequestHandler();		//负责对 rtsp 客户端的请求处理
}

/**
 *  该函数调用 readSocket 从 fOurSocket 上读取数据；
 *  读到的数据保存在 fRequestBuffer 中，并返回实际读到的数据的长度；
 *  从socket中读到数据后必须对数据进行解析
 **/
void GenericMediaServer::ClientConnection::incomingRequestHandler() {
  struct sockaddr_in dummy; // 'from' address, meaningless in this case
  
  int bytesRead = readSocket(envir(), fOurSocket, &fRequestBuffer[fRequestBytesAlreadySeen], fRequestBufferBytesLeft, dummy);   //调用 readSocket 从 fOurSocket 上读取数据；
  handleRequestBytes(bytesRead);        //从socket中读到数据后必须对数据进行解析
}

void GenericMediaServer::ClientConnection::resetRequestBuffer() {
  fRequestBytesAlreadySeen = 0;
  fRequestBufferBytesLeft = sizeof fRequestBuffer;
}


////////// GenericMediaServer::ClientSession implementation //////////

GenericMediaServer::ClientSession
::ClientSession(GenericMediaServer& ourServer, u_int32_t sessionId)
  : fOurServer(ourServer), fOurSessionId(sessionId), fOurServerMediaSession(NULL),
    fLivenessCheckTask(NULL) {
  noteLiveness();
}

GenericMediaServer::ClientSession::~ClientSession() {
  // Turn off any liveness checking:
  envir().taskScheduler().unscheduleDelayedTask(fLivenessCheckTask);

  // Remove ourself from the server's 'client sessions' hash table before we go:
  char sessionIdStr[8+1];
  sprintf(sessionIdStr, "%08X", fOurSessionId);
  fOurServer.fClientSessions->Remove(sessionIdStr);
  
  if (fOurServerMediaSession != NULL) {
    fOurServerMediaSession->decrementReferenceCount();
    if (fOurServerMediaSession->referenceCount() == 0
	&& fOurServerMediaSession->deleteWhenUnreferenced()) {
      fOurServer.removeServerMediaSession(fOurServerMediaSession);
      fOurServerMediaSession = NULL;
    }
  }
}

void GenericMediaServer::ClientSession::noteLiveness() {
#ifdef DEBUG
  char const* streamName
    = (fOurServerMediaSession == NULL) ? "???" : fOurServerMediaSession->streamName();
  fprintf(stderr, "Client session (id \"%08X\", stream name \"%s\"): Liveness indication\n",
	  fOurSessionId, streamName);
#endif
  if (fOurServer.fReclamationSeconds > 0) {
    envir().taskScheduler().rescheduleDelayedTask(fLivenessCheckTask,
						  fOurServer.fReclamationSeconds*1000000,
						  (TaskFunc*)livenessTimeoutTask, this);
  }
}

void GenericMediaServer::ClientSession::noteClientLiveness(ClientSession* clientSession) {
  clientSession->noteLiveness();
}

void GenericMediaServer::ClientSession::livenessTimeoutTask(ClientSession* clientSession) {
  // If this gets called, the client session is assumed to have timed out, so delete it:
#ifdef DEBUG
  char const* streamName
    = (clientSession->fOurServerMediaSession == NULL) ? "???" : clientSession->fOurServerMediaSession->streamName();
  fprintf(stderr, "Client session (id \"%08X\", stream name \"%s\") has timed out (due to inactivity)\n",
	  clientSession->fOurSessionId, streamName);
#endif
  delete clientSession;
}

GenericMediaServer::ClientSession* GenericMediaServer::createNewClientSessionWithId() {
  u_int32_t sessionId;
  char sessionIdStr[8+1];

  // Choose a random (unused) 32-bit integer for the session id
  // (it will be encoded as a 8-digit hex number).  (We avoid choosing session id 0,
  // because that has a special use by some servers.)
  do {
    sessionId = (u_int32_t)our_random32();  //使用 random 方式创建一个 sessionId
    snprintf(sessionIdStr, sizeof sessionIdStr, "%08X", sessionId);
  } while (sessionId == 0 || lookupClientSession(sessionIdStr) != NULL);

  ClientSession* clientSession = createNewClientSession(sessionId);
  fClientSessions->Add(sessionIdStr, clientSession);

  return clientSession;
}

GenericMediaServer::ClientSession*
GenericMediaServer::lookupClientSession(u_int32_t sessionId) {
  char sessionIdStr[8+1];
  snprintf(sessionIdStr, sizeof sessionIdStr, "%08X", sessionId);
  return lookupClientSession(sessionIdStr);
}

GenericMediaServer::ClientSession*
GenericMediaServer::lookupClientSession(char const* sessionIdStr) {
  return (GenericMediaServer::ClientSession*)fClientSessions->Lookup(sessionIdStr);
}


////////// ServerMediaSessionIterator implementation //////////

GenericMediaServer::ServerMediaSessionIterator
::ServerMediaSessionIterator(GenericMediaServer& server)
  : fOurIterator((server.fServerMediaSessions == NULL)
		 ? NULL : HashTable::Iterator::create(*server.fServerMediaSessions)) {
}

GenericMediaServer::ServerMediaSessionIterator::~ServerMediaSessionIterator() {
  delete fOurIterator;
}

ServerMediaSession* GenericMediaServer::ServerMediaSessionIterator::next() {
  if (fOurIterator == NULL) return NULL;

  char const* key; // dummy
  return (ServerMediaSession*)(fOurIterator->next(key));
}


////////// UserAuthenticationDatabase implementation //////////

UserAuthenticationDatabase::UserAuthenticationDatabase(char const* realm,
						       Boolean passwordsAreMD5)
  : fTable(HashTable::create(STRING_HASH_KEYS)),
    fRealm(strDup(realm == NULL ? "LIVE555 Streaming Media" : realm)),
    fPasswordsAreMD5(passwordsAreMD5) {
}

UserAuthenticationDatabase::~UserAuthenticationDatabase() {
  delete[] fRealm;
  
  // Delete the allocated 'password' strings that we stored in the table, and then the table itself:
  char* password;
  while ((password = (char*)fTable->RemoveNext()) != NULL) {
    delete[] password;
  }
  delete fTable;
}

void UserAuthenticationDatabase::addUserRecord(char const* username,
					       char const* password) {
  fTable->Add(username, (void*)(strDup(password)));
}

void UserAuthenticationDatabase::removeUserRecord(char const* username) {
  char* password = (char*)(fTable->Lookup(username));
  fTable->Remove(username);
  delete[] password;
}

char const* UserAuthenticationDatabase::lookupPassword(char const* username) {
  return (char const*)(fTable->Lookup(username));
}
