#include "../utils_C/conexion_memoria.h"

int** solicitar_direccion_fisica(int pagina,int pid){

    t_buffer* buffer = buffer_create(sizeof(int)*2);
    buffer_add_uint32(buffer,pid);
    buffer_add_uint32(buffer,pagina);
    EnviarOperacion(socket_conexionMemoria,5,buffer);
    buffer = RecibirBuffer(socket_conexionMemoria);
    int** direccion_fisica = (int**)buffer_read(buffer,sizeof(int*));//direccion
    return direccion_fisica;
}

t_buffer* leer_en_memoria(int pid, int** direccion, int tamanio){
    t_buffer* buffer = buffer_create(sizeof(int)*2 + sizeof(int*));
    buffer_add_uint32(buffer,pid);//pid
    buffer_write(buffer,direccion,sizeof(int**));//direccion
    buffer_add_uint32(buffer,tamanio);//tamaño
    EnviarOperacion(socket_conexionMemoria,8,buffer);
    
    t_buffer* bufferRespuesta = RecibirBuffer(socket_conexionMemoria);
    buffer_destroy(buffer);
    return bufferRespuesta;
}

int escribir_en_memoria(int** direccion, int pid, void* value,int tamanio ){
    t_buffer* buffer = buffer_create(sizeof(int)+tamanio+sizeof(int*));
    buffer_add_uint32(buffer,pid);//pid
    buffer_write(buffer,direccion,sizeof(int**));//direccion
    buffer_write(buffer,value,tamanio);//valor
    EnviarOperacion(socket_conexionMemoria,7,buffer);
    buffer = RecibirBuffer(socket_conexionMemoria);
    int errorCode = buffer_read_uint32(buffer);
    //free(buffer);
    return errorCode;
}

t_buffer* solicitar_instruccion(int pid, int pc){
    t_buffer* buffer =buffer_create(sizeof(int)*2);
    buffer_add_uint32(buffer,pid);
    buffer_add_uint32(buffer,pc);
    EnviarOperacion(socket_conexionMemoria,2,buffer);
    return RecibirBuffer(socket_conexionMemoria);
}

