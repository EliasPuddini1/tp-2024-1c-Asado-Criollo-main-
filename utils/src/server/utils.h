#ifndef UTILSSERVER_H_
#define UTILSSERVER_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<string.h>
#include<assert.h>
#include"commons.h"
typedef   void(*Funcion)(int a);
typedef struct
{
	int socket;
	Funcion f;
}server;

extern t_log* logger;
int getSocketServidor(char*);
void iniciar_servidor(server*);
int esperar_cliente(int);
int recibir_operacion(int);
int handshake_Server(int,int);
#endif /* UTILS_H_ */
