#ifndef UTILSCLIENT_H_
#define UTILSCLIENT_H_

#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<commons.h>

extern t_log* logger;
int conectar(char*, char*, int);
int crear_conexion(char* ip, char* puerto);
int handshake_Client(int socket_server,int modulo);
#endif /* UTILS_H_ */
