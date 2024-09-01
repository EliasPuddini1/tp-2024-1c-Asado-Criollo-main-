#ifndef UTILSMAIN_H_
#define UTILSMAIN_H_

#include <server/utils.h>
#include <client/utils.h>

#include <readline/readline.h>
#include <commons/config.h>
#include <pthread.h>
#include "stdlib.h"
#include <commons/collections/queue.h>


#include "commons.h"
#include <time.h>
#include <math.h>

#include <semaphore.h>//para semaforos

#include <commons/collections/list.h> //para la lista tlb

typedef struct
{
    char* ip_memoria;
    char* puerto_memoria;
    char* puerto_escucha_dispatch;
    char* puerto_escucha_interrupt;
    int entradas_tlb;
    char* algoritmo_tlb;
    int tamanio_pagina;
} cpuConfig;

typedef struct {
    PCB *p;
    Registros *registros;
    int socketMemoria;
    int socketCliente;
    bool *interrupcionRecibida;
} cicloParams;
typedef enum
{
	HANDSHAKE,
	CONTEXTO,
	INTERRUPCION
} op_code;


extern bool interrupcionRecibida;
extern int procesoEnEjecucion;
extern int socket_conexionMemoria;
extern cpuConfig cpu_config;
extern t_log* logger;
extern PCB pcb;
//extern op_code opcode;

extern int enviarOperacion;
extern server dispatch;
extern server interrupt;
extern int operacionAEnviar;

extern sem_t sem_interrupt;
extern sem_t sem_interrupt_fin;
extern sem_t sem_ciclo;
extern sem_t sem_busqueda_proceso;

extern pthread_mutex_t mutex_logger;
extern pthread_mutex_t mutex_flag_interrpupcion;

void ServerCPU(int socketCliente);
void obtenerConfiguracion(t_config *);
void conectarAMemoria();
void obtenerDatosMemoria(int *);

t_instruccion fetchInstruction(int socketMemoria, PCB *p);
bool executeInstruccion(t_instruccion *instruccion, int socketCliente, int procesoEnEjecucion);
void checkInterrupt(PCB *p, bool interrupcionRecibida, int *programCounter);
void cicloInstruccion(int socketMemoria, PCB *p, t_log* logger, bool interrupcionRecibida);
void ServerCPU(int socketCliente);


t_instruccion conectarMemoria(int socketMemoria, PCB *p);
void iniciarLogger();
void gestionarKernel();
//void *cicloDeInstruccion(void *args);

//MMU
void iniciar_MMU();
int* obtener_direccion_fisica(uint32_t dir, int pid);
//CONEXION MEMORIA
int** solicitar_direccion_fisica(int pagina,int pid);
t_buffer* leer_en_memoria(int pid, int** direccion, int tamanio);
int escribir_en_memoria(int** direccion, int pid,void* value,int tamanio);
t_buffer* solicitar_instruccion(int pid, int pc);

//TLB
int iniciar_tlb();
void setear_algoritmo_tlb();
int *obtener_frame(int pagina, int pid);
void actualizar_tlb(void* frame, int pagina,int pid);

#define UNDEFINED -100
#define ERROR -1

typedef uint32_t dir_logica;

//a enfocarse en:
//movin y movout
//copystring
//iostdinread
//iostdoutwrite
#endif // UTILSMAIN_H_