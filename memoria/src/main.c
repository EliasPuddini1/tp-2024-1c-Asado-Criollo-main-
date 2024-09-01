#include <stdlib.h>
#include <stdio.h>
#include <commons/config.h>
#include <commons.h>
#include <server/utils.h>
#include <pthread.h>

typedef enum
{
    HANDSHAKE,
    RECIBIR_PROCESO,
    SOLICITAR_INSTRUCCION,
    RESIZE_PROCESO,
    ELIMINAR_PROCESO,
    SOLICITAR_DIRECCION_FISICA,
    SOLICITAR_TAMAÑO_PAGINA,
    ESCRIBIR,
    LEER
} op_code;
typedef struct
{
    t_list *instrucciones;
    PCB *pcb;
    int cantPaginas;
    t_list* Paginas;

} Proceso;

typedef struct 
{
    void * direccionFisica;
    bool ocupado;
} Frame;

void InicializarMemoria(int tamanioMemoria,int tamanioPagina);
void ServerMemoria(int socketCliente);
void iniciarServidorMemoria(char *puerto);
bool isCurrentPID(Proceso * p);
void * Leer(void * direccion, int tamanio, int pid);
void Escribir(void * direccion, void* value,int tamanio, int pid);
t_list* filtrar_pid(int pid);
int buscar_pagina(t_list* paginas, void* direccion);
int evaluar_sobrante(int sobrante,int tamanioPaginaRestante, int* cantidad);
pthread_mutex_t mutex_procesos,mutex_frames,mutex_PID,mutex_logger,mutex_memoria = PTHREAD_MUTEX_INITIALIZER;
t_list * procesos;
Frame * frames;
int retardoRespuesta;
void * memoria;
int tamanioPagina;
int tamanioMemoria;
int CurrentPID;
char* pathCarpeta;
int main(int argc, char *argv[])
{
    procesos = list_create();
    logger = log_create("memoria.log", "Memoria", true, LOG_LEVEL_INFO);
    t_config *config = config_create("memoria.config");
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "Memoria INIT");
    pthread_mutex_unlock(&mutex_logger);
    char *puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    tamanioMemoria = config_get_int_value(config, "TAM_MEMORIA");
    tamanioPagina = config_get_int_value(config, "TAM_PAGINA");
    pathCarpeta = config_get_string_value(config, "PATH_INSTRUCCIONES");

    InicializarMemoria(tamanioMemoria,tamanioPagina);

    retardoRespuesta = config_get_int_value(config,"RETARDO_RESPUESTA");
    // Config Inicial
    server serv;
    serv.socket = getSocketServidor(puerto);
    serv.f = ServerMemoria;
    iniciar_servidor(&serv);
    return 0;
}


void * Leer(void * direccion, int tamanio, int pid)
{
    t_list* paginas = filtrar_pid(pid);
    if(!paginas)
    {
        return NULL;
    }
    int numero = buscar_pagina(paginas,direccion);
    int* pagina = (int*) list_get(paginas,numero);
    pthread_mutex_lock(&mutex_frames);
    void* pie = frames[*pagina].direccionFisica;
    pthread_mutex_unlock(&mutex_frames);
    int restantepagina = tamanioPagina -(direccion-pie);
    void * value = malloc(tamanio);

    if(tamanio <= restantepagina){
        
        memcpy(value,direccion,tamanio);
        //list_destroy(paginas);
        return value;
    }else{
        t_list* paginas = filtrar_pid(pid);
        int numero = buscar_pagina(paginas,direccion);
        int tamanioLeer = restantepagina;
        memcpy(value,direccion,tamanioLeer);
        int desplazamiento = tamanioLeer;
        int tamanioRestante = tamanio -restantepagina;
        numero++;

        while(tamanioRestante > 0)
        {
            pagina = (int*) list_get(paginas,numero);
            pthread_mutex_lock(&mutex_frames);
            pie = frames[*pagina].direccionFisica;
            pthread_mutex_unlock(&mutex_frames);
            tamanioLeer = tamanioPagina<=tamanioRestante?tamanioPagina:tamanioRestante;
            memcpy(value+desplazamiento,pie,tamanioLeer);
            tamanioRestante -= tamanioLeer;
            desplazamiento+= tamanioLeer;
            numero++;
        }
        //list_destroy(paginas);
        return value;
    }
    
}

void Escribir(void * direccion, void* value,int tamanio,int pid)
{
    t_list* paginas = filtrar_pid(pid);
    if(!paginas)
    {
        return NULL;
    }
    int numero = buscar_pagina(paginas,direccion);
    int* pagina = (int*) list_get(paginas,numero);
    pthread_mutex_lock(&mutex_frames);
    void* pie = frames[*pagina].direccionFisica;
    pthread_mutex_unlock(&mutex_frames);
    int restantepagina = tamanioPagina -(direccion-pie);
    
    if(tamanio <= restantepagina){
        memcpy(direccion,value,tamanio);
    }else
    {
        t_list* paginas = filtrar_pid(pid);
        int numero = buscar_pagina(paginas,direccion);
        int tamanioEscribir = restantepagina;
        memcpy(direccion,value,restantepagina);
        int desplazamiento = tamanioEscribir;
        int tamanioRestante = tamanio -restantepagina;
        numero++;

        while(tamanioRestante > 0)
        {
            pagina = (int*) list_get(paginas,numero);
            pthread_mutex_lock(&mutex_frames);
            pie = frames[*pagina].direccionFisica;
            pthread_mutex_unlock(&mutex_frames);
            
            tamanioEscribir = tamanioPagina<=tamanioRestante?tamanioPagina:tamanioRestante;
            memcpy(pie,value+desplazamiento,tamanioEscribir);
            tamanioRestante-=tamanioEscribir;
            desplazamiento+= tamanioEscribir;
            numero++;
        }
    }
    //list_destroy(paginas);

}

int evaluar_sobrante(int sobrante,int tamanioPaginaRestante, int* cantidad){

    int desplazamiento = tamanioPagina - tamanioPaginaRestante;

    if(sobrante + desplazamiento == tamanioPagina){
        return 0;

    }else if(sobrante + desplazamiento > tamanioPagina){
        (*cantidad)++;
        return sobrante + desplazamiento -tamanioPagina;

    }else if(sobrante + desplazamiento < tamanioPagina){
        
        sobrante += desplazamiento;
        return sobrante;
        
    }

}

t_list* filtrar_pid(int pid){

    bool filtro_pid(void* arg){
        Proceso* proceso_a = (Proceso*) arg;

        return proceso_a->pcb->PID == pid;
    }
    pthread_mutex_lock(&mutex_procesos);
    t_list* procesos_B = (t_list *) list_filter(procesos,filtro_pid);
    Proceso* filtrado = (Proceso*) list_get(procesos_B,0);
    pthread_mutex_unlock(&mutex_procesos);
    return filtrado->Paginas;
}

int buscar_pagina(t_list* paginas, void* direccion){

    bool filtro_frame(int* frame){
        pthread_mutex_lock(&mutex_frames);
        void* inicio = frames[*frame].direccionFisica;
        pthread_mutex_unlock(&mutex_frames);
        void* fin = inicio + tamanioPagina;

        // Verificar si buscar está dentro de este frame
        return (direccion >= inicio && direccion < fin);
    }
    void* pagina = (void*) list_find(paginas,filtro_frame);

    // Si la página se encuentra, devolver su índice
    if (pagina != NULL) {
        for (int i = 0; i < list_size(paginas); i++) {
            if (list_get(paginas, i) == pagina) {
                return i; // Devolver el índice de la página encontrada
            }
        }
    }
    return -1;
}


void EliminarProceso(int pid)
{
    pthread_mutex_lock(&mutex_procesos);
    CurrentPID = pid;
    Proceso * p =list_find(procesos,isCurrentPID);
    log_info(logger,"PID: %d - Destruyendo tabla de páginas de tamaño: %d",pid,p->cantPaginas);
    for(int i = p->cantPaginas-1;i>=0;i--)
    {
        int * nroFrame = (int*)list_get(p->Paginas,i);
        LiberarFrame(*nroFrame);
        free(nroFrame);
        list_remove(p->Paginas,i);
    }
    list_remove_by_condition(procesos,isCurrentPID);
    LiberarProceso(p);
    pthread_mutex_unlock(&mutex_procesos);
    
}

void InicializarMemoria(int tamanioMemoria, int tamanioPagina)
{
    memoria = malloc(tamanioMemoria);
    int cantidadFrames = tamanioMemoria/tamanioPagina;
    pthread_mutex_lock(&mutex_frames);
    frames = malloc(sizeof(Frame)*cantidadFrames);
    for(int i = 0;i<cantidadFrames;i++)
    {
        void * direccionFrame = i*tamanioPagina + memoria;
        frames[i].direccionFisica = direccionFrame;
        frames[i].ocupado = 0;
    }
    pthread_mutex_unlock(&mutex_frames);
}

t_list *LeerPrograma(char *filepath)
{
    FILE *fp;
    t_list *instrucciones = list_create();
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char* aux = strdup(pathCarpeta);
    char *path = malloc(strlen(aux) + strlen(filepath) + 1);
    strcpy(path, aux);
    strcat(path,filepath);
    free(aux);
    fp = fopen(path, "r");
    free(path);
    if (fp == NULL)
        return NULL;

    while ((read = getline(&line, &len, fp)) != -1)
    {
        line[strcspn(line, "\n")] = 0;
        char* aux = strdup(line);
        list_add(instrucciones, aux);
    }
    free(line);
    fclose(fp);
    return instrucciones;
}

void LiberarFrame(int nroFrame)
{
    pthread_mutex_lock(&mutex_frames);
    frames[nroFrame].ocupado = 0;
    pthread_mutex_unlock(&mutex_frames);
}

int OcuparSiguienteFrame()
{
    pthread_mutex_lock(&mutex_frames);
    bool hayMemoria = false;
    int nroFrame;
    for(int i = 0;i<tamanioMemoria/tamanioPagina;i++)
    {
        if(!frames[i].ocupado)
        {
            nroFrame = i;
            hayMemoria = true;
            frames[i].ocupado = 1;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_frames);
    if(hayMemoria)
    {
        return nroFrame;
    }
    else
    {
        return -1;
    }
    
}

int ResizeProceso(Proceso* p, int bytes)
{
    int cantPaginasNuevo;
    if(bytes%tamanioPagina>0)
    {
        cantPaginasNuevo = bytes/tamanioPagina+1;
    }
    else
    {
        cantPaginasNuevo = bytes/tamanioPagina;
    }
    bool OutOfMemory = false;
    if(cantPaginasNuevo> p->cantPaginas)
    {
        log_info(logger,"PID: %d - Tamaño Actual: %d - Tamaño a Ampliar: %d",p->pcb->PID,p->cantPaginas,cantPaginasNuevo);
        for(int i = p->cantPaginas;i<cantPaginasNuevo;i++)
        {
            
            int nextFrame = OcuparSiguienteFrame();
            if(nextFrame == -1)
            {
                OutOfMemory = true;
                break;
            }

            int* framePtr = malloc(sizeof(int));
            if (framePtr == NULL)
            {
                OutOfMemory = true;
                break;
            }
            *framePtr = nextFrame;

            list_add(p->Paginas,framePtr);
            p->cantPaginas++;
            
        }
        
    }
    else
    {
        log_info(logger,"PID: %d - Tamaño Actual: %d - Tamaño a Reducir: %d",p->pcb->PID,p->cantPaginas,cantPaginasNuevo);
        for(int i = p->cantPaginas-1;i>=cantPaginasNuevo;i--)
        {
            int * nroFrame = (int*)list_get(p->Paginas,i);
            LiberarFrame(*nroFrame);
            p->cantPaginas--;
            free(nroFrame);
            list_remove(p->Paginas,i);
            
        }
        
    }
    if(OutOfMemory)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

Proceso *RecibirProceso(int socket)
{
    Proceso *p = malloc(sizeof(Proceso));
    
    t_buffer * buffer = RecibirBuffer(socket);

    char* filepath = DeserializarString(buffer);
    p->pcb = DeserializarPCB(buffer);
    
    p->instrucciones = LeerPrograma(filepath);
    p->Paginas = list_create();
    p->cantPaginas = 0;
    log_info("PID: %d - Creando tabla de páginas de tamaño: %d",p->pcb->PID,p->cantPaginas);
    buffer_destroy(buffer);
    free(filepath);
    return p;
}

bool isCurrentPID(Proceso * p)
{
    int i = p->pcb->PID == CurrentPID;
    return i;
}

void ServerMemoria(int socketCliente)
{
    
    int cod_op = recibir_operacion(socketCliente);
    log_info(logger, "Cod_OP: %i", cod_op);
    while (cod_op >= 0)
    {
        long milisegundos = retardoRespuesta*1000;
        usleep(milisegundos);
        switch (cod_op)
        {
        case HANDSHAKE:
            handshake_Server(socketCliente, MEMORIA);
            break;
        case RECIBIR_PROCESO:

            Proceso *p = RecibirProceso(socketCliente);
            pthread_mutex_lock(&mutex_procesos);
            list_add(procesos, p);
            pthread_mutex_unlock(&mutex_procesos);
            int code_error;
            if(p->instrucciones == NULL)
            {
                code_error =1;
            }
            else
            {
                code_error=0;
            }
            
            log_info(logger,"Se recibio el proceso con ID: %d",p->pcb->PID);
            send(socketCliente,&code_error,sizeof(int),0);

            break;
        case SOLICITAR_INSTRUCCION:
        {
            int pid,pc;
            t_buffer* bufferRecibir = RecibirBuffer(socketCliente);
            pid = buffer_read_uint32(bufferRecibir);
            pc = buffer_read_uint32(bufferRecibir);
            pthread_mutex_lock(&mutex_procesos);
            CurrentPID = pid;
            Proceso * process =list_find(procesos,isCurrentPID);
            char* instruccion = list_get(process->instrucciones,pc);
            pthread_mutex_unlock(&mutex_procesos);
            t_buffer* buffer = SerializarString(instruccion);
            EnviarBuffer(socketCliente,buffer);
            buffer_destroy(buffer);
            buffer_destroy(bufferRecibir);
            break;
        }
        case RESIZE_PROCESO: 
        {
            
            t_buffer *buffer = RecibirBuffer(socketCliente);
            int PID = buffer_read_uint32(buffer);
            int newSize = buffer_read_uint32(buffer);
            log_info(logger,"PID: %d- Accion: RESIZE - Tamaño %d",PID,newSize);
            pthread_mutex_lock(&mutex_PID);
            pthread_mutex_lock(&mutex_procesos);
            CurrentPID = PID;
            Proceso* p = list_find(procesos,isCurrentPID);
            pthread_mutex_unlock(&mutex_procesos);
            pthread_mutex_unlock(&mutex_PID);
            int errorCode = ResizeProceso(p,newSize);
            t_buffer *respuesta = buffer_create(sizeof(int));
            buffer_add_uint32(respuesta,errorCode);
            EnviarBuffer(socketCliente,respuesta);
            buffer_destroy(buffer);
            buffer_destroy(respuesta);
            break;
        }
        case ELIMINAR_PROCESO:
        {
            t_buffer *buffer = RecibirBuffer(socketCliente);
            int pid = buffer_read_uint32(buffer);
            EliminarProceso(pid);
            t_buffer * bufferRespuesta = buffer_create(sizeof(int));
            int errorCode = 0;
            buffer_add_uint32(bufferRespuesta,errorCode);
            EnviarBuffer(socketCliente,bufferRespuesta);
            buffer_destroy(buffer);
            buffer_destroy(bufferRespuesta);
            break;
        }
        case SOLICITAR_DIRECCION_FISICA:
        {

            t_buffer *buffer = RecibirBuffer(socketCliente);
            int pid = buffer_read_uint32(buffer);
            int nroPagina = buffer_read_uint32(buffer);
            pthread_mutex_lock(&mutex_procesos);
            CurrentPID = pid;
            Proceso * p =list_find(procesos,isCurrentPID);
            pthread_mutex_unlock(&mutex_procesos);
            int* frame = list_get(p->Paginas,nroPagina);
            pthread_mutex_lock(&mutex_frames);
            void * direccion = frames[*frame].direccionFisica;
            pthread_mutex_unlock(&mutex_frames);
            log_info(logger,"PID: %d - Pagina: %d - Marco: %p",pid,nroPagina,direccion);
            t_buffer *respuesta = buffer_create(sizeof(int*));
            buffer_write(respuesta,&direccion,sizeof(int**));
            respuesta->offset = 0;
            EnviarBuffer(socketCliente,respuesta);
            buffer_destroy(buffer);
            buffer_destroy(respuesta);
            break;

        }
        case SOLICITAR_TAMAÑO_PAGINA:
        {
            t_buffer* buffer = RecibirBuffer(socketCliente);
            t_buffer* bufferRespuesta = buffer_create(sizeof(int));
            buffer_add_uint32(bufferRespuesta,tamanioPagina);
            EnviarBuffer(socketCliente,bufferRespuesta);
            buffer_destroy(buffer);
            buffer_destroy(bufferRespuesta);
            break;
        }
        case ESCRIBIR:
        {
            t_buffer *buffer = RecibirBuffer(socketCliente);
            int pid = buffer_read_uint32(buffer);
            void** direccion = (void**)buffer_read(buffer,sizeof(void**));
            int tamanio = buffer->size - sizeof(int)-sizeof(void**);
            void* value = buffer_read(buffer,tamanio);
            log_info(logger,"PID: %d- Accion: ESCRIBIR - Direccion fisica: %p - Tamaño %d",pid,*direccion,tamanio);
            pthread_mutex_lock(&mutex_memoria);
            Escribir(*direccion,value,tamanio,pid);
            pthread_mutex_unlock(&mutex_memoria);
            t_buffer* bufferRespuesta = buffer_create(sizeof(int));
            buffer_add_uint32(bufferRespuesta,0);
            EnviarBuffer(socketCliente,bufferRespuesta);
            free(direccion);
            free(value);
            buffer_destroy(buffer);
            buffer_destroy(bufferRespuesta);
            break;
        }
        case LEER:
        {
            t_buffer *buffer = RecibirBuffer(socketCliente);
            int pid = buffer_read_uint32(buffer);
            void** direccion = (void**)buffer_read(buffer,sizeof(void**));
            int tamanio = buffer_read_uint32(buffer);
            log_info(logger,"PID: %d- Accion: LEER - Direccion fisica: %p - Tamaño %d",pid,*direccion,tamanio);
            pthread_mutex_lock(&mutex_memoria);
            void* value = Leer(*direccion,tamanio,pid);
            pthread_mutex_unlock(&mutex_memoria);
            t_buffer* bufferRespuesta = buffer_create(tamanio);
            buffer_write(bufferRespuesta,value,tamanio);
            EnviarBuffer(socketCliente,bufferRespuesta);
            buffer_destroy(buffer);
            buffer_destroy(bufferRespuesta);
            break;
        }
        
        default:
            break;
        }
        cod_op = recibir_operacion(socketCliente);
    }
}

void LiberarProceso(Proceso *proceso)
{
    if (proceso != NULL)
    {
        if (proceso->instrucciones != NULL)
        {
            list_destroy(proceso->instrucciones);
        }

        if (proceso->pcb != NULL)
        {
            LiberarPCB(proceso->pcb);
        }

        if (proceso->Paginas != NULL)
        {
            
            for (int i = 0; i < list_size(proceso->Paginas); i++)
            {
                int *frame = (int*)list_get(proceso->Paginas, i);
                if (frame != NULL)
                {
                    free(frame);
                }
            }
            list_destroy(proceso->Paginas);
        }

        free(proceso);
    }
}