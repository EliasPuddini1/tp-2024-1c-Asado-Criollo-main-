#ifndef MAIN_H
#define MAIN_H

#include <client/utils.h>
#include <server/utils.h>
#include "stdlib.h"
#include <readline/readline.h>
#include <stdio.h>
#include <pthread.h>
#include <commons/config.h>
#include "../include/operaciones.h"
#include "commons.h"

typedef enum
{
	HANDSHAKE,
	RECIBIR_INSTRUCCION,
    CONECTAR_INTERFAZ,
    LEER_MEMORIA,
    ESCRIBIR_MEMORIA,
    FIN_OP_IO
} op_code;

typedef struct {
    char* Nombre;
    char* PathConfig;
    char* Tipo;
} InterfazInit;

extern InterfazInit init;

void* Interfaz(char*nobmre,char*pathConfig);
int EjecutarInstruccion(t_instruccion* instruccion,int pid);

void crearConfig(const char* path, const char* type);

int conexionMemoria(t_config* config);
void EnviarOperacion(int socket, int operacion, t_buffer* buffer);
t_buffer* RecibirBuffer(int socket);

#endif
