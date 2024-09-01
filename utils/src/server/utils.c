#include"utils.h"
#include <pthread.h>

t_log* logger;
int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

int getSocketServidor(char* puerto)
{
	int socket_servidor;
	int err;
	struct addrinfo hints, *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &server_info);
	// Creamos el socket de escucha del servidor
	socket_servidor = socket(server_info->ai_family,
                        server_info->ai_socktype,
                        server_info->ai_protocol);
	// Asociamos el socket a un puerto
	err = bind(socket_servidor, server_info->ai_addr, server_info->ai_addrlen);
	if(err != 0)
	{
		return(err);
	}
	err = listen(socket_servidor, SOMAXCONN);
	
	freeaddrinfo(server_info);
	log_trace(logger, "Listo para escuchar a mi cliente");
	return(socket_servidor);
}

void iniciar_servidor(server *server)
{
	while(true)
    {
		pthread_t *thread = malloc(sizeof(pthread_t));
        log_info(logger,"Esperando cliente...");
        int socketCliente = esperar_cliente(server->socket);
		if(socketCliente<0)
		{
			log_info(logger,"Socket: %i",socketCliente);
			free(thread);
			exit(1);
		}
		
        pthread_create(thread, NULL, (void*)server->f, socketCliente);
        pthread_detach(*thread);
		free(thread);
    }
}

int esperar_cliente(int socket_servidor)
{
	// Quitar esta línea cuando hayamos terminado de implementar la funcion
	// Aceptamos un nuevo cliente
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	if(socket_cliente<0)
	{
		log_error(logger,"Ha habido un error en la conexion");
	}
	log_info(logger, "Se conecto un cliente!");

	return socket_cliente;
}

int handshake_Server(int socket_cliente,int modulo)
{
	log_info(logger,"Comenzando handshake...");
	int moduloCliente;
	recv(socket_cliente, &moduloCliente, sizeof(int), MSG_WAITALL);
	int err;
	switch (modulo)
	{
	case KERNEL:
		if(moduloCliente == IO)
		{
			err = 0;
		}
		else
		{
			err = -1;
		}
		break;
	case MEMORIA:
		if(moduloCliente == IO || moduloCliente == KERNEL || moduloCliente == CPU)
		{
			err = 0;
		}
		else
		{
			err = -1;
		}
		break;
	case CPU:
		if(moduloCliente == KERNEL)
		{
			err = 0;
		}
		else
		{
			err = -1;
		}
		break;
	default:
		err = -1;
		break;
	}
	if(err == 0)
	{
		log_info(logger,"Cliente identificado. Modulo %s",getNombreModulo(moduloCliente));
		
	}
	else
	{
		log_info(logger,"El cliente no tiene acceso a este módulo");
	}
	send(socket_cliente,&err,sizeof(int),0);
	return err;
}

t_paquete* recibir_paquete(int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->buffer = malloc(sizeof(t_buffer));

	recv(socket_cliente, &(paquete->op_code), sizeof(uint8_t), 0);

	recv(socket_cliente, &(paquete->buffer->size), sizeof(uint32_t), 0);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	recv(socket_cliente, paquete->buffer->stream, paquete->buffer->size, 0);

	return paquete;
}