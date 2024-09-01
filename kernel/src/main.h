#ifndef KERNEL_H
#define KERNEL_H

#include <commons/config.h>
#include <commons/collections/queue.h>
#include <pthread.h>
#include <semaphore.h>
#include <server/utils.h>
#include <client/utils.h>
// Struct definitions
typedef struct {
    int tipo;
    int socket;
    char* nombre;
    t_queue* queue_procesar;
} Interfaz;

typedef struct {
    PCB* proceso;
    t_buffer* instruccion;
} solicitud_IO;

typedef struct {
    char* nombre;
    int maxinstancias;
    int instanciasDisponibles;
} recurso;

typedef struct {
    int pid;
    char* nombreRecurso;
    bool ocupada;
} peticion_recursos;

// Enum definitions
typedef enum {
    HANDSHAKE,
    FINALIZAR_PROCESO,
    IDENTIFICAR_IO
} op_code;

typedef enum {
    FIN_PROCESO,
    FIN_QUANTUM,
    BLOQUEO_IO,
    CAMBIAR_RECURSO,
    OUT_OF_MEMORY
} desalojo_code;

// Function declarations
int conectarAMemoria(t_config *conexion);
void ServerKernel(int socketCliente);
void IniciarProceso(char* filePath);
void EliminarProceso();
bool isCurrentPID(PCB * p);
void ListarProcesos();
void EnviarProceso(int socket, PCB* p, char* filepath);
bool IsInterfazPorNombre(Interfaz* i);
void* PlanificadorLargoPlazo(void* arg);
void* PlanificadorCortoPlazo(void* arg);
void ConfigInicial();
char* GetNombreEstado(int estado);
char* GetPrioridad(t_queue * queue);
void InicializarRecursos(t_config* config);
int EnviarInstruccion(t_buffer* instruccion, char* nombreinterfaz);
Interfaz* ValidarIO(char* nombre, operaciones_IO operacion);
void InterrumpirCPU(int * quantumProceso);
void EnviarAReady(PCB*p);
void GestionarInterfaz(Interfaz * io);
bool GestionarRecurso(recurso * r, PCB* p, int operacion);
void EjecutarScript(char * path);
void ejecutar_comando(const char* command);
void FinalizarProceso(int PID, char* motivo);
void CambiarEstado(PCB* p, int estado);
bool isCurrRecurso(recurso*r);
void IniciarConexiones(t_config *config, char* puertoEscucha);
bool isCurrPeticion(peticion_recursos*p);
bool isCurrPeticionByPID(peticion_recursos*p);
bool WaitRecurso(char* nombre,int pid);
void SignalRecurso(char* nombre,int pid);
void EliminarProcesoDeQueue(t_queue* queue,PCB* p);
extern int nextProcessID;
extern t_queue* queue_new;
extern t_queue* queue_ready;
extern t_queue* queue_salida_bloqueados;
extern t_list* processList;
extern t_list* interfaces;
extern t_list* bloqueados;
extern t_list* recursos;
extern t_list* peticiones_recursos;
extern int CurrentPID;
extern char* NombreInterfazCurr;
extern char* algoritmo_planificacion;
extern char* curr_recurso;
extern int conexionCPUInterrupt, conexionCPUDispatch, conexionMemoria, multiprogramacion_max, quantum;
extern int multiprogramacion_curr;
extern pthread_mutex_t mutex_new, mutex_ready, mutex_multiprogramacion, curr_interfaz, mutex_recursos;
extern sem_t slots_multiprogramacion;

#endif // KERNEL_H