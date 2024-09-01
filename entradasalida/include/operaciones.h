#ifndef OPERACIONES_H
#define OPERACIONES_H

#include "../include/main.h"

int handle_IO_GEN_SLEEP(uint32_t tiempo);
int handle_IO_STDIN_READ(uint32_t *reg1,uint32_t *reg2,int conexion,int pid);
int handle_IO_STDOUT_WRITE(uint32_t *reg1,uint32_t *reg2,int conexion,int pid);
int handle_IO_FS_CREATE(const char *filename);
int handle_IO_FS_DELETE(const char *filename);
int handle_IO_FS_TRUNCATE(const char *filename, const uint32_t *reg,int pid);
int handle_IO_FS_WRITE(const char *filename, const uint32_t *reg1, const uint32_t *reg2, const uint32_t *reg3,int pid,int conexion_memoria);
int handle_IO_FS_READ(const char *filename, const uint32_t *reg1, const uint32_t *reg2, const uint32_t *reg3,int pid,int conexion_memoria);

t_buffer* serializarInput(uint32_t* direccion, void* input,int pid,int tamanio);
t_buffer* serializarOutput(uint32_t *direccion, uint32_t tamaño,int pid);

#endif