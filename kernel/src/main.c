#include "main.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <stdio.h>
#include <commons/config.h>
#include <commons/collections/queue.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
// Global variable definitions
int nextProcessID = 0;
t_queue *queue_new;
t_queue *queue_ready;
t_queue *cola_prioridad;
t_list *processList;
t_list *interfaces;
t_list *bloqueados;
t_list *recursos;
t_list *peticiones_recursos;
int CurrentPID = 0;
char *NombreInterfazCurr;
char *algoritmo_planificacion;
char* pathScripts;
char *curr_recurso;
int conexionCPUInterrupt, conexionCPUDispatch, conexionMemoria, multiprogramacion_max, quantum;
int multiprogramacion_curr = 0;
bool desalojado, pararPlanificacion, terminarProceso = false;
sem_t desaloja_proceso;
pthread_mutex_t mutex_new, mutex_ready, mutex_multiprogramacion, curr_interfaz, mutex_recursos, curr_pid, mutex_peticiones,mutex_logger = PTHREAD_MUTEX_INITIALIZER;
sem_t slots_multiprogramacion;


int main(int argc, char *argv[])
{
    ConfigInicial();
    char *leido;

    while (true)
    {
        leido = readline("> ");
        ejecutar_comando(leido);
    }
    return 0;
}
void IniciarProceso(char *filePath)
{
    int PID = nextProcessID;
    PCB *proceso = CrearPCB(PID, quantum);
    EnviarProceso(conexionMemoria, proceso, filePath);
    int code_error;
    recv(conexionMemoria, &code_error, sizeof(int), 0);
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "Se crea el proceso %d en NEW", proceso->PID);
    pthread_mutex_unlock(&mutex_logger);
    if (code_error == 0)
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "Se ha enviado el proceso a la memoria exitosamente");
        pthread_mutex_unlock(&mutex_logger);
    }

    list_add(processList, proceso);
    queue_push(queue_new, proceso);
    if (PID == 0) // Si es el primer proceso, iniciar planificacion y ejecucion
    {
        pthread_t *thread = malloc(sizeof(pthread_t));
        pthread_create(thread, NULL, PlanificadorLargoPlazo, NULL);
        pthread_detach(*thread);
        free(thread);
        pthread_t *thread1 = malloc(sizeof(pthread_t));
        pthread_create(thread1, NULL, PlanificadorCortoPlazo, NULL);
        pthread_detach(*thread1);
        free(thread1);
    }
    nextProcessID++;
}
// Funciones de interaccion con IO
int EnviarInstruccion(t_buffer *instruccion, char *nombreinterfaz)
{
    pthread_mutex_lock(&curr_interfaz);
    NombreInterfazCurr = nombreinterfaz;
    Interfaz *i = list_find(interfaces, IsInterfazPorNombre);
    pthread_mutex_unlock(&curr_interfaz);
    EnviarOperacion(i->socket, 1, instruccion);
    t_buffer *bufferRespuesta = RecibirBuffer(i->socket);
    int error_code = buffer_read_uint32(bufferRespuesta);
    if (error_code < 0)
    {
        log_info(logger, "Hubo un error en la operacion IO");
    }
    return error_code;
}
bool IsInterfazPorNombre(Interfaz *i)
{
    return strcmp(NombreInterfazCurr, i->nombre) == 0;
}
Interfaz *ValidarIO(char *nombre, operaciones_IO operacion)
{
    bool valida = false;
    pthread_mutex_lock(&curr_interfaz);
    NombreInterfazCurr = nombre;
    Interfaz *i = list_find(interfaces, IsInterfazPorNombre);
    pthread_mutex_unlock(&curr_interfaz);
    if (i == NULL)
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "La interfaz %s no está conectada", nombre);
        pthread_mutex_unlock(&mutex_logger);
        return i;
    }
    else
    {
        switch (i->tipo)
        {
        case GENERICA:
        {
            if (operacion == IO_GEN_SLEEP)
            {
                valida = true;
            }
            break;
        }
        case STDIN:
        {
            if (operacion == IO_STDIN_READ)
            {
                valida = true;
            }
            break;
        }
        case STDOUT:
        {
            if (operacion == IO_STDOUT_WRITE)
            {
                valida = true;
            }
            break;
        }
        case DIALFS:
            if(operacion == IO_FS_CREATE||operacion == IO_FS_DELETE||operacion == IO_FS_READ||operacion == IO_FS_TRUNCATE||operacion == IO_FS_WRITE)
            {
                valida = true;
            }
            break;
        }
    }
    if (valida)
    {
        return i;
    }
    else
    {
        return NULL;
    }
}
bool ValidarRecurso(char* nombre)
{
    pthread_mutex_lock(&curr_recurso);
    curr_recurso = nombre;
    recurso *r = list_find(recursos, isCurrRecurso);
    pthread_mutex_unlock(&curr_recurso);
    return r != NULL;
}
// Funciones de interaccion con Memoria
// Envia un proceso a la memoria (PCB + Filepath)
void EnviarProceso(int socket, PCB *p, char *filepath)
{

    t_buffer *bufferProceso = SerializarString(filepath);

    t_buffer *bufferPCB = SerializarPCB(p);

    buffer_add_buffer(bufferProceso, bufferPCB);

    int op_code = 1;
    EnviarOperacion(conexionMemoria, op_code, bufferProceso);
    buffer_destroy(bufferPCB);
    buffer_destroy(bufferProceso);
}
// Finaliza el proceso
void FinalizarProceso(int PID, char *motivo)
{
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "Finaliza el proceso %d - Motivo: %s", PID, motivo);
    pthread_mutex_unlock(&mutex_logger);
    PCB *p = list_get(processList, PID);
    if(p->Estado == EXEC && !strcmp(motivo,"INTERRUPTED_BY_USER"))
    {
        int quantum = 0;
        terminarProceso = true;
        InterrumpirCPU(&quantum);
        sem_wait(&desaloja_proceso);
    }
    pthread_mutex_lock(&mutex_ready);
    EliminarProcesoDeQueue(queue_ready, p);
    EliminarProcesoDeQueue(cola_prioridad, p);
    pthread_mutex_unlock(&mutex_ready);
    pthread_mutex_lock(&curr_pid);
    pthread_mutex_lock(&mutex_peticiones);
    CurrentPID = PID;
    t_list *listaFiltrada = list_filter(peticiones_recursos, isCurrPeticionByPID);
    pthread_mutex_unlock(&curr_pid);
    pthread_mutex_unlock(&mutex_peticiones);
    if (!list_is_empty(listaFiltrada))
    {
        for (int i = 0; i < list_size(listaFiltrada); i++)
        {
            peticion_recursos *p = list_get(listaFiltrada, i);
            SignalRecurso(p->nombreRecurso, PID);
        }
    }
    CambiarEstado(p, EXIT);
    pthread_mutex_lock(&mutex_multiprogramacion);
    multiprogramacion_curr--;
    pthread_mutex_unlock(&mutex_multiprogramacion);
    t_buffer *buffer = buffer_create(sizeof(int));
    buffer_add_uint32(buffer, p->PID);
    EnviarOperacion(conexionMemoria, 4, buffer);
    buffer = RecibirBuffer(conexionMemoria);
    int errorCode = buffer_read_uint32(buffer);
    if (errorCode < 0)
    {
        log_error(logger, "No se pudo borrar el proceso %d de la memoria", p->PID);
    }
}
// Planificacion
void InterrumpirCPU(int *quantumProceso)
{
    usleep(*quantumProceso * 1000);
    if (!desalojado)
    {
        t_buffer *buffer = buffer_create(sizeof(int));
        buffer_add_uint32(buffer, 0);
        EnviarOperacion(conexionCPUInterrupt, 2, buffer);
        buffer_destroy(buffer);
    }
}
void EnviarAReady(PCB *p)
{
    if (p->Estado != EXIT)
    {
        CambiarEstado(p, READY);
        pthread_mutex_lock(&mutex_ready);
        queue_push(queue_ready, p);
        char *prioridad = GetPrioridad(queue_ready);
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "Ingreso de proceso: %d a ready: Cola Ready / Ready Prioridad %s", p->PID, prioridad);
        free(prioridad);
        pthread_mutex_unlock(&mutex_logger);
        pthread_mutex_unlock(&mutex_ready);
    }
}
void EnviarAReadyPrimero(PCB *p)
{
    CambiarEstado(p, READY);
    t_queue *aux = queue_create();
    queue_push(aux, p);
    pthread_mutex_lock(&mutex_ready);
    int max_size = queue_size(queue_ready);
    for (int i = 0;i< max_size ; i++)
    {
        PCB *process = queue_pop(queue_ready);
        queue_push(aux, process);
    }
    max_size = queue_size(aux);
    for (int i = 0; i < max_size; i++)
    {
        PCB *process = queue_pop(aux);
        queue_push(queue_ready, process);
    }
    char *prioridad = GetPrioridad(queue_ready);
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "Ingreso de proceso: %d a ready: Cola Ready / Ready Prioridad %s", p->PID, prioridad);
    free(prioridad);
    pthread_mutex_unlock(&mutex_logger);
    pthread_mutex_unlock(&mutex_ready);
}
void EnviarAPrioridad(PCB *p)
{
    CambiarEstado(p, READY);
    pthread_mutex_lock(&mutex_ready);
    queue_push(cola_prioridad, p);
    pthread_mutex_unlock(&mutex_ready);
}
void *PlanificadorCortoPlazo(void *arg)
{
    while (1)
    {
        while (pararPlanificacion)
        {
        }
        if (!queue_is_empty(queue_ready) || !queue_is_empty(cola_prioridad))
        {
            PCB *p;
            
            pthread_mutex_lock(&mutex_ready);
            if (!queue_is_empty(cola_prioridad))
            {
                p = queue_pop(cola_prioridad);
            }
            else
            {
                p = queue_pop(queue_ready);
            }
            int quantumProceso = p->Quantum;
            CambiarEstado(p, EXEC);
            pthread_mutex_unlock(&mutex_ready);
            int op_code = 1;
            t_buffer *bufferEnviar = SerializarPCB(p);
            pthread_t *thread = malloc(sizeof(pthread_t));
            if (!strcmp(algoritmo_planificacion, "RR") || !strcmp(algoritmo_planificacion, "VRR"))
            {
                pthread_create(thread, NULL, InterrumpirCPU, &quantumProceso);
                pthread_detach(*thread);
            }
            struct timespec start, end;
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            desalojado = false;
            EnviarOperacion(conexionCPUDispatch, op_code, bufferEnviar);
            t_buffer* buffer = RecibirBuffer(conexionCPUDispatch);
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            uint64_t delta_ms = ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000) / 1000;
            desalojado = true;
            PCB* aux =   DeserializarPCB(buffer);
            memcpy(p,aux,sizeof(PCB));
            int motivo = buffer_read_uint32(buffer);
            if ((!strcmp(algoritmo_planificacion, "RR") || !strcmp(algoritmo_planificacion, "VRR")) && motivo != FIN_QUANTUM)
            {
                pthread_cancel(*thread);
            }
            free(thread);
            switch (motivo)
            {
            case FIN_PROCESO:
            {
                FinalizarProceso(p->PID, "SUCCESS");
                break;
            }
            case FIN_QUANTUM:
            {
                if(!terminarProceso)
                {
                    pthread_mutex_lock(&mutex_logger);
                    log_info(logger, "PID: %d - Desalojado por fin de Quantum", p->PID);
                    pthread_mutex_unlock(&mutex_logger);
                    p->Quantum = quantum;
                    EnviarAReady(p);
                }
                else
                {
                    terminarProceso = false;
                    sem_post(&desaloja_proceso);
                }
                
                break;
            }
            case BLOQUEO_IO:
            {
                int operacion = buffer_read_uint32(buffer);
                char *nombreIO = DeserializarString(buffer);
                Interfaz *i = ValidarIO(nombreIO, operacion);
                if (i == NULL)
                {
                    FinalizarProceso(p->PID, "INVALID_INTERFACE");
                }
                else
                {
                    solicitud_IO *solicitud = malloc(sizeof(solicitud_IO));
                    solicitud->proceso = p;
                    t_buffer *bufferAux = buffer_create(sizeof(int)*2);
                    buffer_add_uint32(bufferAux, operacion);
                    buffer_add_uint32(bufferAux,p->PID);
                    t_buffer* bufferSobrante = buffer_get_sobrante(buffer);
                    buffer_add_buffer(bufferAux,bufferSobrante);
                    solicitud->instruccion = bufferAux;
                    CambiarEstado(p, BLOCKED);
                    if(!strcmp(algoritmo_planificacion,"VRR"))
                    {
                        p->Quantum = quantumProceso - delta_ms;
                    }
                    else
                    {
                        p->Quantum = quantum;
                    }
                    pthread_mutex_lock(&mutex_logger);
                    log_info(logger, "PID: %d - Bloqueado por: %s", p->PID, nombreIO);
                    pthread_mutex_unlock(&mutex_logger);
                    queue_push(i->queue_procesar, solicitud);

                }
                break;
            }
            case CAMBIAR_RECURSO:
            {
                int operacion = buffer_read_uint32(buffer);
                char *nombre = DeserializarString(buffer);
                bool valido = ValidarRecurso(nombre);
                if(valido)
                {
                    if (operacion == OP_WAIT)
                {
                    bool bloqueado = WaitRecurso(nombre, p->PID);
                    if (bloqueado)
                    {
                        CambiarEstado(p, BLOCKED);
                        p->Quantum = quantum;
                        pthread_mutex_lock(&mutex_logger);
                        log_info(logger, "PID: %d - Bloqueado por: %s", p->PID, nombre);
                        pthread_mutex_unlock(&mutex_logger);
                        list_add(bloqueados, p);
                    }
                    else
                    {
                        int quantumRestante = quantumProceso - delta_ms;
                        if (quantumRestante > 0)
                        {
                            p->Quantum = quantumProceso - delta_ms;
                            EnviarAPrioridad(p);
                        }
                        else
                        {
                            pthread_mutex_lock(&mutex_logger);
                            log_info(logger, "PID: %d - Desalojado por fin de Quantum", p->PID);
                            pthread_mutex_unlock(&mutex_logger);
                            EnviarAReady(p);
                        }
                    }
                }

                else if (operacion == OP_SIGNAL)
                {
                    SignalRecurso(nombre, p->PID);
                    int quantumRestante = quantumProceso - delta_ms;
                    if (quantumRestante > 0)
                    {
                        p->Quantum = quantumProceso - delta_ms;
                        EnviarAPrioridad(p);
                    }
                    else
                    {
                        pthread_mutex_lock(&mutex_logger);
                        log_info(logger, "PID: %d - Desalojado por fin de Quantum", p->PID);
                        pthread_mutex_unlock(&mutex_logger);
                        EnviarAReady(p);
                    }
                }
                }
                else
                {
                    FinalizarProceso(p->PID,"INVALID_RESOURCE");
                }
                break;   
            }
            case OUT_OF_MEMORY:
            {
                FinalizarProceso(p->PID,"OUT_OF_MEMORY");
                break;
            }
            }
            //LiberarPCB(aux);
            buffer_destroy(buffer);
            buffer_destroy(bufferEnviar);
        }
    }
}
void *PlanificadorLargoPlazo(void *arg)
{
    while (1)
    {
        while (pararPlanificacion)
        {
        }
        pthread_mutex_lock(&mutex_new);
        pthread_mutex_lock(&mutex_multiprogramacion);
        if (!queue_is_empty(queue_new) && multiprogramacion_curr < multiprogramacion_max)
        {

            for (int i = 0; i < queue_size(queue_new); i++)
            {
                if (multiprogramacion_curr >= multiprogramacion_max)
                {
                    break;
                }
                PCB *p = queue_pop(queue_new);
                EnviarAReady(p);
                multiprogramacion_curr++;
                pthread_mutex_lock(&mutex_logger);
                log_info(logger, "Grado de multiprogramacion: %d", multiprogramacion_curr);
                pthread_mutex_unlock(&mutex_logger);
            }
        }
        pthread_mutex_unlock(&mutex_multiprogramacion);
        pthread_mutex_unlock(&mutex_new);
    }
}
void GestionarInterfaz(Interfaz *io)
{
    while (1)
    {
        while (pararPlanificacion)
        {
        }
        if (!queue_is_empty(io->queue_procesar))
        {
            solicitud_IO *solicitud = queue_pop(io->queue_procesar);
            if (solicitud->proceso->Estado == BLOCKED)
            {
                int error = EnviarInstruccion(solicitud->instruccion, io->nombre);
                if(error<0)
                {
                    FinalizarProceso(solicitud->proceso->PID,"IO_ERROR");
                }
                else
                {
                    if (!strcmp(algoritmo_planificacion, "VRR"))
                    {
                        EnviarAReadyPrimero(solicitud->proceso);
                    }
                    else
                    {
                        EnviarAReady(solicitud->proceso);
                    }
                }
                
            }
            //solicitud_destroy(solicitud);
        }
    }
}
bool WaitRecurso(char *nombre, int pid)
{
    pthread_mutex_lock(&mutex_recursos);
    pthread_mutex_lock(&mutex_peticiones);
    pthread_mutex_lock(&curr_recurso);
    curr_recurso = nombre;
    recurso *r = list_find(recursos, isCurrRecurso);
    pthread_mutex_unlock(&curr_recurso);
    peticion_recursos *peticion = malloc(sizeof(peticion_recursos));
    peticion->nombreRecurso = strdup(r->nombre);
    peticion->pid = pid;
    if (r->instanciasDisponibles > 0)
    {
        r->instanciasDisponibles--;
        peticion->ocupada = true;
    }
    else
    {
        peticion->ocupada = false;
    }
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "Peticion creada al recurso: %s por el proceso %d, instancias disponibles: %d", r->nombre, pid, r->instanciasDisponibles);
    pthread_mutex_unlock(&mutex_logger);
    list_add(peticiones_recursos, peticion);
    bool bloqueado = !peticion->ocupada;
    pthread_mutex_unlock(&mutex_peticiones);
    pthread_mutex_unlock(&mutex_recursos);
    return bloqueado;
}
void SignalRecurso(char *nombre, int pid)
{
    pthread_mutex_lock(&mutex_recursos);
    pthread_mutex_lock(&curr_recurso);
    pthread_mutex_lock(&curr_pid);
    pthread_mutex_lock(&mutex_peticiones);
    curr_recurso = nombre;
    CurrentPID = pid;
    recurso *r = list_find(recursos, isCurrRecurso);
    pthread_mutex_unlock(&curr_pid);
    r->instanciasDisponibles++;
    list_remove_by_condition(peticiones_recursos, isCurrPeticion);
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "Peticion liberada del recurso: %s por el proceso %d, instancias disponibles: %d", r->nombre, pid, r->instanciasDisponibles);
    pthread_mutex_unlock(&mutex_logger);
    if (r->instanciasDisponibles > 0)
    {
        OcuparRecurso(r);
    }
    pthread_mutex_unlock(&mutex_peticiones);
    pthread_mutex_unlock(&curr_recurso);
    pthread_mutex_unlock(&mutex_recursos);
}
void OcuparRecurso(recurso *r)
{
    for (int i = 0; i < list_size(peticiones_recursos); i++)
    {
        peticion_recursos *p = list_get(peticiones_recursos, i);
        if (!strcmp(p->nombreRecurso, r->nombre) && !p->ocupada)
        {
            r->instanciasDisponibles--;
            p->ocupada = true;
            pthread_mutex_lock(&mutex_logger);
            log_info(logger, "Recurso %s ocupado por el proceso: %d, instancias disponibles: %d", r->nombre, p->pid, r->instanciasDisponibles);
            pthread_mutex_unlock(&mutex_logger);
            pthread_mutex_lock(&curr_pid);
            CurrentPID = p->pid;
            PCB *proceso = list_find(bloqueados, isCurrentPID);
            pthread_mutex_unlock(&curr_pid);
            EnviarAReady(proceso);
            break;
        }
    }
}
void EliminarProcesoDeQueue(t_queue *queue, PCB *p)
{
    t_queue *queue_aux = queue_create();
    int max_size = queue_size(queue);
    for (int i = 0; i < max_size; i++)
    {
        PCB *aux = queue_pop(queue);
        if (p->PID != aux->PID)
        {
            queue_push(queue_aux, aux);
        }
    }
    max_size = queue_size(queue_aux);
    for (int i = 0; i < max_size; i++)
    {
        PCB *aux = queue_pop(queue_aux);
        if (p->PID != aux->PID)
        {
            queue_push(queue, aux);
        }
    }
    queue_destroy(queue_aux);
}
// Elimina un proceso
void EliminarProceso()
{
    int PID;
    printf("Elija el Process ID a borrar: ");
    scanf("%d", &PID);
    CurrentPID = PID;
    list_remove_by_condition(processList, isCurrentPID);
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "Se ha eliminado el proceso con PID: %d", PID);
    pthread_mutex_unlock(&mutex_logger);
}
// Ejecucion de Scripts
void EjecutarScript(char *filepath)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    // Allocate memory for the full path
    char *aux = strdup(pathScripts);
    if (!aux)
    {
        fprintf(stderr, "Failed to allocate memory for path\n");
        return;
    }

    char *path = malloc(strlen(aux) + strlen(filepath) + 1);
    if (!path)
    {
        fprintf(stderr, "Failed to allocate memory for full path\n");
        free(aux);
        return;
    }

    strcpy(path, aux);
    strcat(path, filepath);

    

    // Open the file
    fp = fopen(path, "r");
    

    if (fp == NULL)
    {
        perror("Failed to open file");
        return;
    }

    // Read and execute each line from the script file
    while ((read = getline(&line, &len, fp)) != -1)
    {
        line[strcspn(line, "\n")] = 0;  // Remove newline character
        ejecutar_comando(line);
    }

    // Clean up
    free(path);
    free(aux);
    free(line);
    fclose(fp);
}
void ejecutar_comando(const char *command)
{
    char cmd[256];
    char param[256];
    int param_int;
    // Check and parse each command
    if (sscanf(command, "EJECUTAR_SCRIPT %255[^\n]", param) == 1)
    {
        EjecutarScript(param);
    }
    else if (sscanf(command, "INICIAR_PROCESO %255[^\n]", param) == 1)
    {
        IniciarProceso(param);
    }
    else if (sscanf(command, "FINALIZAR_PROCESO %d", &param_int) == 1)
    {
        int pid = param_int;
        FinalizarProceso(pid, "INTERRUPTED_BY_USER");
    }
    else if (strcmp(command, "DETENER_PLANIFICACION") == 0)
    {
        pararPlanificacion = true;
    }
    else if (strcmp(command, "INICIAR_PLANIFICACION") == 0)
    {
        pararPlanificacion = false;
    }
    else if (sscanf(command, "MULTIPROGRAMACION %d", &param_int) == 1)
    {
        int valor = param_int;
        pthread_mutex_lock(&mutex_multiprogramacion);
        multiprogramacion_max = valor;
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "Nuevo valor maximo de multiprogramacion: %d", multiprogramacion_max);
        pthread_mutex_unlock(&mutex_logger);
        pthread_mutex_unlock(&mutex_multiprogramacion);
    }
    else if (strcmp(command, "PROCESO_ESTADO") == 0)
    {
        ListarProcesos();
    }
    else
    {
    }
}

// Funciones Auxiliares
char *GetPrioridad(t_queue *queue)
{
    int size = queue_size(queue);
    // Initial buffer size estimate (could be dynamically increased as needed)
    int buffer_size = 50;
    char *prioridad = (char *)malloc(buffer_size * sizeof(char));
    if (!prioridad) {
        // Handle memory allocation failure
        return NULL;
    }

    strcpy(prioridad, "[");

    for (int i = 0; i < size; i++)
    {
        PCB *p = queue_pop(queue);
        if (!p) {
            // Handle error if queue_pop fails
            free(prioridad);
            return NULL;
        }

        char str[10];
        sprintf(str, "%d", p->PID);

        // Ensure there is enough space for concatenation
        int needed_size = strlen(prioridad) + strlen(str) + (i < size - 1 ? 2 : 1); // +1 for comma or closing bracket, +1 for null terminator
        if (needed_size > buffer_size)
        {
            buffer_size *= 2; // Double the buffer size
            char *new_prioridad = (char *)realloc(prioridad, buffer_size);
            if (!new_prioridad) {
                // Handle memory reallocation failure
                free(prioridad);
                return NULL;
            }
            prioridad = new_prioridad;
        }

        strcat(prioridad, str);
        if (i < size - 1)
        {
            strcat(prioridad, ",");
        }
        queue_push(queue, p);
    }

    strcat(prioridad, "]");
    return prioridad;
}
bool isCurrentPID(PCB *p)
{
    return p->PID == CurrentPID;
}
void ListarProcesos()
{
    for (size_t i = 0; i < list_size(processList); i++)
    {
        PCB *p = list_get(processList, i);
        PrintPCB(*p);
    }
}
void CambiarEstado(PCB *p, int estado)
{
    char *estadoAnterior = GetNombreEstado(p->Estado);
    char *estadoActual = GetNombreEstado(estado);
    p->Estado = estado;
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "PID: %d - Estado Anterior: %s - Estado Actual: %s", p->PID, estadoAnterior, estadoActual);
    pthread_mutex_unlock(&mutex_logger);
    free(estadoAnterior);
    free(estadoActual);
}
char *GetNombreEstado(int estado)
{
    char *nombre;
    switch (estado)
    {
    case NEW:
        nombre = strdup("NEW");
        break;
    case READY:
        nombre = strdup("READY");
        break;
    case EXEC:
        nombre = strdup("EXEC");
        break;
    case BLOCKED:
        nombre = strdup("BLOCKED");
        break;
    case EXIT:
        nombre = strdup("EXIT");
        break;
    default:
        nombre = strdup("UNKNOWN");  // Handle default case
        break;
    }
    return nombre;
}
bool isCurrRecurso(recurso *r)
{
    return !strcmp(r->nombre, curr_recurso);
}
bool isCurrPeticion(peticion_recursos *p)
{
    return !strcmp(p->nombreRecurso, curr_recurso) && p->pid == CurrentPID;
}
bool isCurrPeticionByPID(peticion_recursos *p)
{
    return CurrentPID == p->pid;
}

// Server del Kernel
void ServerKernel(int socketCliente)
{
    bool identificada = false;
    int cod_op = recibir_operacion(socketCliente);
    while (cod_op >= 0)
    {
        switch (cod_op)
        {
        case HANDSHAKE:
            handshake_Server(socketCliente, KERNEL);
            break;
        case FINALIZAR_PROCESO:
            int PID;
            recv(socketCliente, &PID, sizeof(int), 0);
            FinalizarProceso(PID, "INTERRUPTED_BY_USER");
            break;
        case IDENTIFICAR_IO:
            Interfaz *i = malloc(sizeof(Interfaz));
            t_buffer *buffer = RecibirBuffer(socketCliente);
            i->tipo = buffer_read_uint32(buffer);
            i->socket = socketCliente;
            int lenght = buffer_read_uint32(buffer);
            i->nombre = buffer_read_string(buffer, lenght);
            i->queue_procesar = queue_create();
            list_add(interfaces, i);
            log_info(logger, "Se conectó la interfaz: %s", i->nombre);
            pthread_t *thread = malloc(sizeof(pthread_t));
            pthread_create(thread, NULL, GestionarInterfaz, i);
            pthread_detach(thread);
            identificada = true;
            break;
        // TO DO Implementar las demas operaciones que recibira de los clientes
        default:
            break;
        }
        if (identificada)
        {
            break;
        }
        cod_op = recibir_operacion(socketCliente);
    }
}

// Inicializacion
void ConfigInicial()
{
    sem_init(&desaloja_proceso,0,0);
    queue_new = queue_create();
    queue_ready = queue_create();
    cola_prioridad = queue_create();
    interfaces = list_create();
    processList = list_create();
    bloqueados = list_create();
    logger = log_create("kernel.log", "Kernel", true, LOG_LEVEL_INFO);
    t_config *config = config_create("kernel.config");
    char *puertoEscucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    multiprogramacion_max = config_get_int_value(config, "GRADO_MULTIPROGRAMACION");
    quantum = config_get_int_value(config, "QUANTUM");
    algoritmo_planificacion = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    pathScripts = config_get_string_value(config, "PATH_SCRIPTS");
    InicializarRecursos(config);
    sem_init(&slots_multiprogramacion, 0, multiprogramacion_max);
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "Kernel INIT");
    pthread_mutex_unlock(&mutex_logger);
    IniciarConexiones(config, puertoEscucha);
}

void InicializarRecursos(t_config *config)
{
    recursos = list_create();
    peticiones_recursos = list_create();
    char *s = config_get_string_value(config, "RECURSOS");
    int comas;
    // Cuenta las comas
    for (comas = 0; s[comas]; s[comas] == ',' ? comas++ : *s++)
        ;

    char **recursos_arr = config_get_array_value(config, "RECURSOS");
    char **instancias_recursos = config_get_array_value(config, "INSTANCIAS_RECURSOS");
    for (int i = 0; i < comas + 1; i++)
    {
        int instancias = atoi(instancias_recursos[i]);
        recurso *r = malloc(sizeof(recurso));
        r->instanciasDisponibles = instancias;
        r->maxinstancias = instancias;
        r->nombre = recursos_arr[i];
        pthread_mutex_lock(&mutex_recursos);
        list_add(recursos, r);
        pthread_mutex_unlock(&mutex_recursos);
    }
}

void IniciarConexiones(t_config *config, char *puertoEscucha)
{
    // Iniciar Server de Kernel
    server *serv = malloc(sizeof(serv));
    serv->socket = getSocketServidor(puertoEscucha);
    serv->f = ServerKernel;
    pthread_t *thread = malloc(sizeof(pthread_t));
    int error = pthread_create(thread, NULL, (void *)iniciar_servidor, serv);
    pthread_detach(*thread);
    // Conectar al server de la Memoria y el CPU
    char *ip = config_get_string_value(config, "IP_MEMORIA");
    char *puerto = config_get_string_value(config, "PUERTO_MEMORIA");
    conexionMemoria = conectar(ip, puerto, KERNEL);
    ip = config_get_string_value(config, "IP_CPU");
    puerto = config_get_string_value(config, "PUERTO_CPU_DISPATCH");
    conexionCPUDispatch = conectar(ip, puerto, KERNEL);
    puerto = config_get_string_value(config, "PUERTO_CPU_INTERRUPT");
    conexionCPUInterrupt = conectar(ip, puerto, KERNEL);
    if (error != 0)
    {
        pthread_mutex_lock(&mutex_logger);
        log_error(logger, "Error creando el hilo para empezar el servidor. Terminando la ejecución...");
        pthread_mutex_unlock(&mutex_logger);
        exit(1);
    }
}

void solicitud_destroy(solicitud_IO* solicitud)
{
    buffer_destroy(solicitud->instruccion);
    LiberarPCB(solicitud->proceso);
    free(solicitud);
}