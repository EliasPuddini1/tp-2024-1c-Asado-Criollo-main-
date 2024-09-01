#include "../include/operaciones.h"
#include "DialFS.h"
int handle_IO_GEN_SLEEP(uint32_t milisegundos) {
    int microsegundos = milisegundos*1000;
    usleep(microsegundos);
    return 0;
}

t_buffer* serializarInput(uint32_t* direccion, void* input,int pid, int tamanio) 
{
    t_buffer* buffer = buffer_create(sizeof(int**)+sizeof(int)+tamanio);
    buffer_add_uint32(buffer,pid);
    buffer_write(buffer,&direccion,sizeof(int**));
    buffer_write(buffer,input,tamanio);
    return buffer;
}

int handle_IO_STDIN_READ(uint32_t *reg1,uint32_t *reg2,int conexion,int pid) {
    int tamanio = *reg2;
    char* value = malloc(tamanio);
    char * input = readline("Ingrese el valor: ");
    for(int i = 0; i<tamanio && i< strlen(input);i++)
    {
        value[i] = input[i];
    }
    free(input);
    t_buffer* buffer = serializarInput(reg1, value,pid,tamanio);
    EnviarOperacion(conexion, 7, buffer);
    buffer = RecibirBuffer(conexion);
    return buffer_read_uint32(buffer);
}

t_buffer* serializarOutput(uint32_t *direccion, uint32_t tamaño,int pid) {
    t_buffer * buffer = buffer_create(sizeof(direccion)+sizeof(uint32_t)*2);
    buffer_add_uint32(buffer,pid);
    buffer_write(buffer,&direccion,sizeof(direccion));
    buffer_add_uint32(buffer,tamaño);
    return buffer;
}

int handle_IO_STDOUT_WRITE( uint32_t *reg1, uint32_t *reg2,int conexion,int pid) {
    uint32_t * direccion = reg1;
    uint32_t tamaño = *reg2;

    t_buffer* buffer = serializarOutput(direccion, tamaño,pid);

    EnviarOperacion(conexion, 8, buffer);
    buffer = RecibirBuffer(conexion);
    char* respuesta = (char*)buffer_read(buffer,tamaño);
    respuesta[tamaño] = '\0';
    printf("Valor leído de memoria: %s\n", respuesta);
}

int handle_IO_FS_CREATE(const char *filename) {
    return CrearFile(filename);;
}

int handle_IO_FS_DELETE(const char *filename) {
    return BorrarFile(filename);;
}

int handle_IO_FS_TRUNCATE(const char *filename, const uint32_t *reg,int pid) {
    
    return TruncateFile(filename,*reg,pid);;
}

int handle_IO_FS_WRITE(const char *filename, const uint32_t *reg1, const uint32_t *reg2, const uint32_t *reg3,int pid,int conexion) {
    void* posicionMemoria = reg1;
    int posicionArchivo = *reg3;
    int tamanio = *reg2;
    t_buffer* buffer = serializarOutput(posicionMemoria, tamanio,pid);
    EnviarOperacion(conexion, 8, buffer);
    buffer = RecibirBuffer(conexion);

    void* value = buffer_read(buffer,tamanio);
    if(!value)
    {
        return -1;
    }

    return FS_Write(filename,posicionArchivo,value,tamanio);;
}

int handle_IO_FS_READ(const char *filename, const uint32_t *reg1, const uint32_t *reg2, const uint32_t *reg3,int pid,int conexion) {
    void* posicionMemoria = reg1;
    int posicionArchivo = *reg3;
    int tamanio = *reg2;
    void* value = FS_Read(filename,posicionArchivo,tamanio);
    t_buffer* buffer = serializarInput(posicionMemoria,value,pid,tamanio);
    EnviarOperacion(conexion, 7, buffer);
    buffer = RecibirBuffer(conexion);
    return buffer_read_uint32(buffer);
}