#include "utils.h"
#include<commons/log.h>


int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints, *server_info;
	int err;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	err = getaddrinfo(ip, puerto, &hints, &server_info);

	int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
	err = connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);

	freeaddrinfo(server_info);

	if(err != 0)
	{
		return err;
	}

	

	return socket_cliente;
}
void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}
int handshake_Client(int socket_server,int modulo)
{
	int err;
	int cod_op = 0;
	send(socket_server, &cod_op, sizeof(int), 0);
	send(socket_server, &modulo, sizeof(int), 0);
    //enviar_mensaje(modulo, socket_server);
	recv(socket_server,&err, sizeof(int), MSG_WAITALL);
	return(err);
}
int conectar(char*ip, char* puerto, int modulo)
{
    int conexion;
 
    conexion = crear_conexion(ip, puerto);
    if(conexion > 0){
        int err = handshake_Client(conexion,modulo);
        if(err == 0)
        {
            log_info(logger, "El handshake ha sido exitoso");
        }
        else
        {
            log_info(logger, "El handshake ha fallado");
            conexion = -1;
        }
    }
    return conexion;
}