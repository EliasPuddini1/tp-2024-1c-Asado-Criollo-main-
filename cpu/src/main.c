#include "../utils_C/cpu.h"
cpuConfig cpu_config;

sem_t sem_busqueda_proceso;
sem_t sem_ciclo;
sem_t sem_interrupt;
sem_t sem_interrupt_fin;

pthread_mutex_t mutex_logger = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_flag_interrpupcion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_interrupcionRecibida = PTHREAD_MUTEX_INITIALIZER;

server interrupt;
server dispatch;


bool interrupcionRecibida;
int socket_conexionMemoria;
int procesoEnEjecucion;

int main(int argc, char *argv[]){
    iniciarLogger();
    // Config inicial
    t_config *config = config_create("cpu.config");
    obtenerConfiguracion(config);
    
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "CPU INIT");
    pthread_mutex_unlock(&mutex_logger);
    
    PCB *p = CrearPCB(0,0);
    p->Registros->AX = 3;
    p->Registros->BX = 4;
    p->Registros->CX = 5;
    p->Registros->DX = 6;
    uint32_t pepito = 168;
    p->Registros->AX = pepito;

    //Iniciar Server de CPU(Kernel)
    gestionarKernel();
    
    //operatoria
    //iniciarMMU    
    conectarAMemoria();

    while(1){
        sleep(1);
    }

    return 0;
}

void gestionarKernel(){
    interrupt.f = ServerCPU;
    interrupt.socket = getSocketServidor(cpu_config.puerto_escucha_interrupt);    
    if(interrupt.socket < 0)
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "Ha habido un problema al iniciar el servidor %i", interrupt.socket);
        pthread_mutex_unlock(&mutex_logger);
        exit(1);
    } else {
        pthread_mutex_lock(&mutex_logger);
        pthread_mutex_unlock(&mutex_logger);
    }
    
    dispatch.socket = getSocketServidor(cpu_config.puerto_escucha_dispatch);  
    dispatch.f = ServerCPU;
    if(dispatch.socket < 0)
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "Ha habido un problema al iniciar el servidor %i", interrupt.socket);
        pthread_mutex_unlock(&mutex_logger);
        exit(1);
    }
    else
    {
        pthread_mutex_lock(&mutex_logger);
        pthread_mutex_unlock(&mutex_logger);
    }

    pthread_t *thread_dispatch = malloc(sizeof(pthread_t));
    pthread_t *thread_interrupt = malloc(sizeof(pthread_t));
    pthread_create(thread_interrupt, NULL, (void*)iniciar_servidor, &interrupt);
    pthread_detach(*thread_interrupt);
    pthread_create(thread_dispatch, NULL, (void*)iniciar_servidor, &dispatch);
    pthread_detach(*thread_dispatch);
}

void ServerCPU(int socketCliente){
    int opcode = recibir_operacion(socketCliente);
    interrupcionRecibida = false;
    while (opcode >= 0) {
        switch (opcode) {
            case HANDSHAKE:
                handshake_Server(socketCliente, CPU);
                break;
            case CONTEXTO: {
                t_buffer *bufferRecibir = RecibirBuffer(socketCliente);
                PCB *p = DeserializarPCB(bufferRecibir);
                procesoEnEjecucion = p->PID;

                cicloParams *params = malloc(sizeof(cicloParams));
                params->p = p;
                params->registros = p->Registros;
                params->socketMemoria = socket_conexionMemoria;
                params->socketCliente = socketCliente;
                pthread_mutex_lock(&mutex_interrupcionRecibida);
                interrupcionRecibida = false;
                pthread_mutex_unlock(&mutex_interrupcionRecibida);
                params->interrupcionRecibida = &interrupcionRecibida;

                int codigo_desalojo;
                t_buffer* valorRetorno = EjecutarProceso(params, &codigo_desalojo);

                t_buffer* buffer = SerializarPCB(p);
                t_buffer *bufferDatosDesalojo = buffer_create(sizeof(int));
                buffer_add_uint32(bufferDatosDesalojo, codigo_desalojo);
                buffer_add_buffer(buffer, bufferDatosDesalojo);
                if (codigo_desalojo == OP_IO || codigo_desalojo == OP_RECURSOS)
                {
                    buffer_add_buffer(buffer, valorRetorno);
                }
                EnviarBuffer(socketCliente, buffer);
                //LiberarParams(params);
                buffer_destroy(buffer);
                buffer_destroy(bufferDatosDesalojo);
                buffer_destroy(valorRetorno);
                buffer_destroy(bufferRecibir);
                }
                break;
            case INTERRUPCION: {
                t_buffer *buffer = RecibirBuffer(socketCliente);
                buffer_read_uint32(buffer);
                pthread_mutex_lock(&mutex_interrupcionRecibida);
                interrupcionRecibida = true;
                pthread_mutex_unlock(&mutex_interrupcionRecibida);
                buffer_destroy(buffer);
                }
                break;
            default:
                break;
        }
        opcode = recibir_operacion(socketCliente);
    }
}

void iniciarLogger(){
    t_log_level nivel = LOG_LEVEL_INFO;
    logger = log_create("cpu.log", "CPU", true, nivel);

    if(logger == NULL){
        write(0, "ERROR -> NO SE PUDE CREAR EL LOG \n", 30);
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "----Log modulo CPU ----");
    pthread_mutex_unlock(&mutex_logger);
}

void obtenerConfiguracion(t_config *config)
{
    cpu_config.ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    cpu_config.puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    cpu_config.puerto_escucha_interrupt = config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT");
    cpu_config.puerto_escucha_dispatch = config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH");
    cpu_config.entradas_tlb = config_get_int_value(config, "CANTIDAD_ENTRADAS_TLB");
    cpu_config.algoritmo_tlb = config_get_string_value(config, "ALGORITMO_TLB");
}

void obtenerDatosMemoria(int *socket){
    uint32_t desplazamiento = 1;
    t_buffer* buffer = buffer_create(sizeof(uint32_t));
    buffer_add_uint32(buffer, desplazamiento);
    
    EnviarOperacion(socket_conexionMemoria, 6, buffer);

    buffer = RecibirBuffer(socket_conexionMemoria);
    
    int tamanio = buffer_read_uint32(buffer);

    cpu_config.tamanio_pagina = tamanio;

    pthread_mutex_lock(&mutex_logger);
    if(cpu_config.tamanio_pagina > 0){
        log_info(logger, "tamaño de pagina obtenida...");
    } else {
        log_error(logger, "error al obtener el tamaño de pagina");
    }
    pthread_mutex_unlock(&mutex_logger);
    
    free(buffer);
}

void conectarAMemoria(){
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "conectando a memoria...");
    pthread_mutex_unlock(&mutex_logger);
    
    iniciar_MMU();
    socket_conexionMemoria = conectar(cpu_config.ip_memoria, cpu_config.puerto_memoria, CPU);
    obtenerDatosMemoria(&socket_conexionMemoria);
}

void liberar_configuracion(){
    free(cpu_config.ip_memoria);
    free(cpu_config.puerto_memoria);
    free(cpu_config.algoritmo_tlb);
    free(cpu_config.puerto_escucha_dispatch);
    free(cpu_config.puerto_escucha_interrupt);
    log_destroy(logger);
}

void iniciarSemaforos(){
    sem_init(&sem_busqueda_proceso, 0, 1);
    sem_init(&sem_ciclo, 0, 0);
    sem_init(&sem_interrupt, 0, 0);
    sem_init(&sem_interrupt_fin, 0, 0);
    
    pthread_mutex_init(&mutex_flag_interrpupcion, NULL);
    pthread_mutex_init(&mutex_logger, NULL);
    pthread_mutex_init(&mutex_interrupcionRecibida, NULL);
}

void LiberarParams(cicloParams * p)
{
    LiberarPCB(p->p);
}

void liberarSemaforos(){
    sem_destroy(&sem_busqueda_proceso);
    sem_destroy(&sem_ciclo);
    sem_destroy(&sem_interrupt);
    sem_destroy(&sem_interrupt_fin);

    pthread_mutex_destroy(&mutex_flag_interrpupcion);
    pthread_mutex_destroy(&mutex_logger);
    pthread_mutex_destroy(&mutex_interrupcionRecibida);
}