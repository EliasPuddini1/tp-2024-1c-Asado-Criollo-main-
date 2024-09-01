#include "../utils_C/cpu.h"
bool interrupcionIO;
bool interrupcionRecursos;
bool finProceso;
bool interrupcionOutOfMemory;
t_buffer *bufferRetorno;
t_buffer *EjecutarProceso(void *args, int *codigo)
{
    cicloParams *params = (cicloParams *)args;
    PCB *p = params->p;
    Registros *registros = p->Registros;
    bufferRetorno = buffer_create(0);
    int socketMemoria = params->socketMemoria;
    int socketCliente = params->socketCliente;
    bool *interrupcionRecibida = params->interrupcionRecibida;
    interrupcionIO = false;
    interrupcionRecursos = false;
    finProceso = false;
    interrupcionOutOfMemory = false;
    // pthread_mutex_lock(&mutex_logger);
    log_info(logger, "Iniciando ciclo de instruccion");
    // pthread_mutex_unlock(&mutex_logger);

    int pcant = p->Registros->PC;
    while (!(*interrupcionRecibida || interrupcionIO || interrupcionRecursos || finProceso || interrupcionOutOfMemory))
    {
        char *inst = fetchInstruccion(socketMemoria, p);
        log_info(logger, "PID: %d - Ejecutando: %s",procesoEnEjecucion, inst);
        t_instruccion instruccionDecodificada = decodeInstruccion(inst, registros);
        executeInstruccion(&instruccionDecodificada, socketCliente, procesoEnEjecucion);
        if (pcant == p->Registros->PC)
        {
            p->Registros->PC++;
            pcant = p->Registros->PC;
        }
        else
        {
            pcant = p->Registros->PC;
        }
        free(inst);
    }
    if (interrupcionIO)
    {
        *codigo = OP_IO;
    }
    else if (interrupcionRecursos)
    {
        *codigo = OP_RECURSOS;
    }
    else if (finProceso)
    {
        *codigo = FIN_PROCESO;
    }
    else if (interrupcionOutOfMemory)
    {
        *codigo = OP_OUT_OF_MEMORY;
    }
    else if (*interrupcionRecibida)
    {
        *codigo = INTERRUPCION_EXTERNA;
    }

    return bufferRetorno;
}
/*
void InicializarRegistros(Registros *reg) {
    reg->PC = 0;
    reg->AX = 0;
    reg->BX = 0;
    reg->CX = 0;
    reg->DX = 0;
    reg->EAX = 0;
    reg->EBX = 0;
    reg->ECX = 0;
    reg->EDX = 0;
}
*/

char *fetchInstruccion(int socketMemoria, PCB *p)
{
    pthread_mutex_lock(&mutex_logger);
    log_info(logger, "PID: %d - FETCH - Program Counter: %d", p->PID, p->Registros->PC);
    pthread_mutex_unlock(&mutex_logger);

    t_buffer *bufferInstruccion = solicitar_instruccion(p->PID, p->Registros->PC);
    if (bufferInstruccion->stream == NULL)
    {
        pthread_mutex_lock(&mutex_logger);
        log_error(logger, "Error al solicitar instruccion");
        pthread_mutex_unlock(&mutex_logger);
        buffer_destroy(bufferInstruccion);
        return NULL;
    }
    else
    {
        char *instruccionRecibida = DeserializarString(bufferInstruccion);
        buffer_destroy(bufferInstruccion);
        return instruccionRecibida;
    }
}

void inicializarInstruccion(t_instruccion *instruccion)
{
    memset(instruccion, 0, sizeof(t_instruccion));
    instruccion->registro2 = NULL;
    instruccion->registro3 = NULL;
    instruccion->valor = 0;
    instruccion->recurso = 0;
    instruccion->interfaz = NULL;
    instruccion->archivo = NULL;
}

void liberarInstruccion(t_instruccion *instruccion)
{
    if (instruccion == NULL)
    {
        return;
    }

    if (instruccion->interfaz != NULL)
    {
        free(instruccion->interfaz);
        instruccion->interfaz = NULL;
    }

    if (instruccion->archivo != NULL)
    {
        free(instruccion->archivo);
        instruccion->archivo = NULL;
    }

    if (instruccion->recurso != NULL)
    {
        free(instruccion->recurso);
        instruccion->recurso = NULL;
    }

    // Optionally, set other fields to NULL or zero for safety
    instruccion->registro1 = NULL;
    instruccion->registro2 = NULL;
    instruccion->registro3 = NULL;
    instruccion->registro18bit = false;
    instruccion->registro28bit = false;
    instruccion->registro38bit = false;
    instruccion->valor = 0;
}

/* void* obtenerRegistro_uint8(Registros* registros, char* token){
    if (strcmp(token, "AX") == 0) return &registros->AX;
    if (strcmp(token, "BX") == 0) return &registros->BX;
    if (strcmp(token, "CX") == 0) return &registros->CX;
    if (strcmp(token, "DX") == 0) return &registros->DX;
    if (strcmp(token, "PC") == 0) return &registros->PC;
    if (strcmp(token, "EAX") == 0) return &registros->EAX;
    if (strcmp(token, "EBX") == 0) return &registros->EBX;
    if (strcmp(token, "ECX") == 0) return &registros->ECX;
    if (strcmp(token, "EDX") == 0) return &registros->EDX;
    return NULL;
} */

/* uint32_t * obtenerRegistro(Registros* registros, char* token)
{
    uint32_t *registro;
    uint8_t *reg1 = obtenerRegistro_uint8(registros,token);
    if (reg1 != NULL) {
        registro = (uint32_t*)reg1;
    } else {
        registro = obtenerRegistro_uint32(registros, token);
    }
    return registro;
} */

RegisterPointer obtenerRegistro(Registros *registros, char *token)
{
    RegisterPointer reg_ptr; // Initialize to NULL
    reg_ptr.reg32 = NULL;
    reg_ptr.reg8 = NULL;
    if (strcmp(token, "AX") == 0)
    {
        reg_ptr.reg8 = &registros->AX;
        reg_ptr.is8bit = true;
    }
    else if (strcmp(token, "BX") == 0)
    {
        reg_ptr.reg8 = &registros->BX;
        reg_ptr.is8bit = true;
    }
    else if (strcmp(token, "CX") == 0)
    {
        reg_ptr.reg8 = &registros->CX;
        reg_ptr.is8bit = true;
    }
    else if (strcmp(token, "DX") == 0)
    {
        reg_ptr.reg8 = &registros->DX;
        reg_ptr.is8bit = true;
    }
    else if (strcmp(token, "PC") == 0)
    {
        reg_ptr.reg32 = &registros->PC;
        reg_ptr.is8bit = false;
    }
    else if (strcmp(token, "EAX") == 0)
    {
        reg_ptr.reg32 = &registros->EAX;
        reg_ptr.is8bit = false;
    }
    else if (strcmp(token, "EBX") == 0)
    {
        reg_ptr.reg32 = &registros->EBX;
        reg_ptr.is8bit = false;
    }
    else if (strcmp(token, "ECX") == 0)
    {
        reg_ptr.reg32 = &registros->ECX;
        reg_ptr.is8bit = false;
    }
    else if (strcmp(token, "EDX") == 0)
    {
        reg_ptr.reg32 = &registros->EDX;
        reg_ptr.is8bit = false;
    }
    else if (strcmp(token, "SI") == 0)
    {
        reg_ptr.reg32 = &registros->SI;
        reg_ptr.is8bit = false;
    }
    else if (strcmp(token, "DI") == 0)
    {
        reg_ptr.reg32 = &registros->DI;
        reg_ptr.is8bit = false;
    }

    return reg_ptr;
}

instruccion obtenerInstruccion(char *token)
{
    static instruccion SET_INST = SET;
    static instruccion MOV_IN_INST = MOV_IN;
    static instruccion MOV_OUT_INST = MOV_OUT;
    static instruccion SUM_INST = SUM;
    static instruccion SUB_INST = SUB;
    static instruccion JNZ_INST = JNZ;
    static instruccion RESIZE_INST = RESIZE;
    static instruccion COPY_STRING_INST = COPY_STRING;
    static instruccion WAIT_INST = WAIT;
    static instruccion SIGNAL_INST = SIGNAL;
    static instruccion IO_GEN_SLEEP_INST = IO_GEN_SLEEP;
    static instruccion IO_STDIN_READ_INST = IO_STDIN_READ;
    static instruccion IO_STDOUT_WRITE_INST = IO_STDOUT_WRITE;
    static instruccion IO_FS_CREATE_INST = IO_FS_CREATE;
    static instruccion IO_FS_DELETE_INST = IO_FS_DELETE;
    static instruccion IO_FS_TRUNCATE_INST = IO_FS_TRUNCATE;
    static instruccion IO_FS_WRITE_INST = IO_FS_WRITE;
    static instruccion IO_FS_READ_INST = IO_FS_READ;
    static instruccion INST_EXIT_INST = INST_EXIT;
    static instruccion INVALID_INSTRUCTION_INST = INVALID_INSTRUCTION;

    if (strcmp(token, "SET") == 0)
        return SET_INST;
    if (strcmp(token, "MOV_IN") == 0)
        return MOV_IN_INST;
    if (strcmp(token, "MOV_OUT") == 0)
        return MOV_OUT_INST;
    if (strcmp(token, "SUM") == 0)
        return SUM_INST;
    if (strcmp(token, "SUB") == 0)
        return SUB_INST;
    if (strcmp(token, "JNZ") == 0)
        return JNZ_INST;
    if (strcmp(token, "RESIZE") == 0)
        return RESIZE_INST;
    if (strcmp(token, "COPY_STRING") == 0)
        return COPY_STRING_INST;
    if (strcmp(token, "WAIT") == 0)
        return WAIT_INST;
    if (strcmp(token, "SIGNAL") == 0)
        return SIGNAL_INST;
    if (strcmp(token, "IO_GEN_SLEEP") == 0)
        return IO_GEN_SLEEP_INST;
    if (strcmp(token, "IO_STDIN_READ") == 0)
        return IO_STDIN_READ_INST;
    if (strcmp(token, "IO_STDOUT_WRITE") == 0)
        return IO_STDOUT_WRITE_INST;
    if (strcmp(token, "IO_FS_CREATE") == 0)
        return IO_FS_CREATE_INST;
    if (strcmp(token, "IO_FS_DELETE") == 0)
        return IO_FS_DELETE_INST;
    if (strcmp(token, "IO_FS_TRUNCATE") == 0)
        return IO_FS_TRUNCATE_INST;
    if (strcmp(token, "IO_FS_WRITE") == 0)
        return IO_FS_WRITE_INST;
    if (strcmp(token, "IO_FS_READ") == 0)
        return IO_FS_READ_INST;
    if (strcmp(token, "EXIT") == 0)
        return INST_EXIT_INST;
    return INVALID_INSTRUCTION_INST;
}

t_instruccion parsearInstruccion(char *instruccion, Registros *registros)
{
    t_instruccion instruccionParseada;
    inicializarInstruccion(&instruccionParseada);

    char *token = strtok(instruccion, " ");
    if (token != NULL)
    {
        instruccionParseada.instruccion = obtenerInstruccion(token);
        int operandoCount = 0;

        switch (instruccionParseada.instruccion)
        {
        case SET:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                switch (operandoCount)
                {
                case 1:
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro1 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro18bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro1 = reg_ptr.reg32;
                        instruccionParseada.registro18bit = false;
                    }
                    break;
                }
                case 2:
                    instruccionParseada.valor = (uint32_t)atoi(token);
                    break;
                default:
                    break;
                }
            }
            break;
        case JNZ:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                switch (operandoCount)
                {
                case 1:
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro1 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro18bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro1 = reg_ptr.reg32;
                        instruccionParseada.registro18bit = false;
                    }
                    break;
                }
                case 2:
                    instruccionParseada.valor = (uint32_t)atoi(token);
                    break;
                default:
                    break;
                }
            }
            instruccionParseada.registro2 = (uint32_t *)obtenerRegistro(registros, "PC").reg32;
            instruccionParseada.registro28bit = false;
            break;
        case MOV_IN:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                switch (operandoCount)
                {
                case 1:
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro1 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro18bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro1 = reg_ptr.reg32;
                        instruccionParseada.registro18bit = false;
                    }
                    break;
                }
                case 2:
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro2 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro28bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro2 = reg_ptr.reg32;
                        instruccionParseada.registro28bit = false;
                    }
                    break;
                }
                }
            }
            break;
        case MOV_OUT:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                switch (operandoCount)
                {
                case 1:
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro1 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro18bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro1 = reg_ptr.reg32;
                        instruccionParseada.registro18bit = false;
                    }
                    break;
                }
                case 2:
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro2 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro28bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro2 = reg_ptr.reg32;
                        instruccionParseada.registro28bit = false;
                    }
                    break;
                }
                }
            }
            break;
        case SUM:
        case SUB:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                switch (operandoCount)
                {
                case 1:
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro1 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro18bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro1 = reg_ptr.reg32;
                        instruccionParseada.registro18bit = false;
                    }
                    break;
                }
                case 2:
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro2 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro28bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro2 = reg_ptr.reg32;
                        instruccionParseada.registro28bit = false;
                    }
                    break;
                }
                default:
                    break;
                }
            }
            break;
        case COPY_STRING:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                if (operandoCount == 1)
                {
                    instruccionParseada.valor = atoi(token);
                }
            }
            instruccionParseada.registro1 = &(registros->SI);
            instruccionParseada.registro2 = &(registros->DI);
            break;
        case RESIZE:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                if (operandoCount == 1)
                {
                    instruccionParseada.valor = atoi(token);
                }
            }
            break;
        case WAIT:
        case SIGNAL:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                if (operandoCount == 1)
                {
                    instruccionParseada.recurso = strdup(token);
                }
            }
            break;
        case IO_GEN_SLEEP:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                if (operandoCount == 1)
                {
                    instruccionParseada.interfaz = strdup(token);
                }
                else if (operandoCount == 2)
                {
                    instruccionParseada.valor = (uint32_t)atoi(token);
                }
            }
            break;
        case IO_STDIN_READ:
        case IO_STDOUT_WRITE:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                if (operandoCount == 1)
                {
                    instruccionParseada.interfaz = strdup(token);
                }
                else if (operandoCount == 2)
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro1 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro18bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro1 = reg_ptr.reg32;
                        instruccionParseada.registro18bit = false;
                    }
                }
                else if (operandoCount == 3)
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro2 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro28bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro2 = reg_ptr.reg32;
                        instruccionParseada.registro28bit = false;
                    }
                }
            }
            break;
        case IO_FS_CREATE:
        case IO_FS_DELETE:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                if (operandoCount == 1)
                {
                    instruccionParseada.interfaz = strdup(token);
                }
                else if (operandoCount == 2)
                {
                    instruccionParseada.archivo = strdup(token);
                }
            }
            break;
        case IO_FS_TRUNCATE:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                if (operandoCount == 1)
                {
                    instruccionParseada.interfaz = strdup(token);
                }
                else if (operandoCount == 2)
                {
                    instruccionParseada.archivo = strdup(token);
                }
                else if (operandoCount == 3)
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro1 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro18bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro1 = reg_ptr.reg32;
                        instruccionParseada.registro18bit = false;
                    }
                }
            }
            break;
        case IO_FS_WRITE:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                if (operandoCount == 1)
                {
                    instruccionParseada.interfaz = strdup(token);
                }
                else if (operandoCount == 2)
                {
                    instruccionParseada.archivo = strdup(token);
                }
                else if (operandoCount == 3)
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro1 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro18bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro1 = reg_ptr.reg32;
                        instruccionParseada.registro18bit = false;
                    }
                }
                else if (operandoCount == 4)
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro2 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro28bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro2 = reg_ptr.reg32;
                        instruccionParseada.registro28bit = false;
                    }
                }
                else if (operandoCount == 5)
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro3 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro38bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro3 = reg_ptr.reg32;
                        instruccionParseada.registro38bit = false;
                    }
                }
            }
            break;
        case IO_FS_READ:
            while ((token = strtok(NULL, " ")) != NULL)
            {
                operandoCount++;
                if (operandoCount == 1)
                {
                    instruccionParseada.interfaz = strdup(token);
                }
                else if (operandoCount == 2)
                {
                    instruccionParseada.archivo = strdup(token);
                }
                else if (operandoCount == 3)
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro1 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro18bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro1 = reg_ptr.reg32;
                        instruccionParseada.registro18bit = false;
                    }
                }
                else if (operandoCount == 4)
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro2 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro28bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro2 = reg_ptr.reg32;
                        instruccionParseada.registro28bit = false;
                    }
                }
                else if (operandoCount == 5)
                {
                    RegisterPointer reg_ptr = obtenerRegistro(registros, token);
                    if (reg_ptr.is8bit)
                    {
                        instruccionParseada.registro3 = (uint32_t *)reg_ptr.reg8;
                        instruccionParseada.registro38bit = true;
                    }
                    else
                    {
                        instruccionParseada.registro3 = reg_ptr.reg32;
                        instruccionParseada.registro38bit = false;
                    }
                }
            }
            break;
        default:
            break;
        }
    }
    return instruccionParseada;
}
t_instruccion decodeInstruccion(char *instruccion, Registros *registros)
{
    return parsearInstruccion(instruccion, registros);
    // void MMU(instruccionDecodificada);
}

void asignarMemoriaParaInt(uint32_t **ptr)
{
    *ptr = (uint32_t *)malloc(sizeof(uint32_t));
    if (*ptr == NULL)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
}

void asignarMemoriaParaString(char **ptr)
{
    *ptr = (char *)malloc(sizeof(char) * 256);
    if (*ptr == NULL)
    {
        perror("Failed to allocate memory for string");
        exit(EXIT_FAILURE);
    }
}

bool executeInstruccion(t_instruccion *instruccion, int socketCliente, int procesoEnEjecucion)
{
    bool interrupcionInterna;
    switch (instruccion->instruccion)
    {
    case SET:
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: SET - %p %u", procesoEnEjecucion, instruccion->registro1, instruccion->valor);
        pthread_mutex_unlock(&mutex_logger);
        if (instruccion->registro18bit)
        {
            uint8_t value = (uint8_t)instruccion->valor;
            memcpy(instruccion->registro1, &value, sizeof(uint8_t));
        }
        else
        {
            *(instruccion->registro1) = instruccion->valor;
        }
        break;
    case SUM:
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: SUM - %p %p", procesoEnEjecucion, instruccion->registro1, instruccion->registro2);
        pthread_mutex_unlock(&mutex_logger);

        if (instruccion->registro18bit && instruccion->registro28bit)
        {
            *(uint8_t *)(instruccion->registro1) += *(uint8_t *)(instruccion->registro2);
        }
        else if (instruccion->registro18bit && !instruccion->registro28bit)
        {
            *(uint8_t *)(instruccion->registro1) += (uint8_t) * (instruccion->registro2);
        }
        else if (!instruccion->registro18bit && !instruccion->registro28bit)
        {
            *(instruccion->registro1) += *(instruccion->registro2);
        }
        else if (!instruccion->registro18bit && instruccion->registro28bit)
        {
            *(instruccion->registro1) += (uint32_t) * (uint8_t *)(instruccion->registro2);
        }

        break;
    case SUB:
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: SUB - %p %p", procesoEnEjecucion, instruccion->registro1, instruccion->registro2);
        pthread_mutex_unlock(&mutex_logger);

        if (instruccion->registro1 != NULL && instruccion->registro2 != NULL)
        {
            *(instruccion->registro1) -= *(instruccion->registro2);
        }
        break;
    case JNZ:
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: JNZ - %u", procesoEnEjecucion, instruccion->valor);
        pthread_mutex_unlock(&mutex_logger);

        if (*(instruccion->registro1) != 0)
        {
            *(instruccion->registro2) = instruccion->valor;
        }
        break;
    case IO_GEN_SLEEP:
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: IO_GEN_SLEEP - %s %u", procesoEnEjecucion, instruccion->interfaz, instruccion->valor);
        pthread_mutex_unlock(&mutex_logger);
        t_buffer *buffer = buffer_create(strlen(instruccion->interfaz) + sizeof(int) * 3);
        int operacion = IO_GEN_SLEEP;
        buffer_add_uint32(buffer, operacion);
        buffer_add_string(buffer, instruccion->interfaz);
        buffer_add_uint32(buffer, instruccion->valor);
        buffer_add_buffer(bufferRetorno, buffer);
        interrupcionIO = true;
        break;
    case RESIZE:
    {
        log_info(logger, "PID: %d - Ejecutando: RESIZE", procesoEnEjecucion);
        t_buffer *buffer = buffer_create(sizeof(int) * 2);
        buffer_add_uint32(buffer, procesoEnEjecucion);
        buffer_add_uint32(buffer, instruccion->valor);
        EnviarOperacion(socket_conexionMemoria, 3, buffer);
        buffer = RecibirBuffer(socket_conexionMemoria);
        int errorCode = buffer_read_uint32(buffer);
        if (errorCode < 0)
        {
            interrupcionOutOfMemory = true;
        }

        LimpiarTLB(procesoEnEjecucion, instruccion->valor);
        break;
    }
    case MOV_OUT:
    {
        // registro1 direccion
        int *direccion;
        if (instruccion->registro18bit)
        {
            direccion = obtener_direccion_fisica((uint32_t) * (uint8_t *)(instruccion->registro1), procesoEnEjecucion);
        }
        else
        {
            direccion = obtener_direccion_fisica(*(uint32_t *)(instruccion->registro1), procesoEnEjecucion);
        }
        int errorCode;
        if (instruccion->registro28bit)
        {
            uint8_t value8 = *(uint8_t *)(instruccion->registro2);
            log_info(logger, "PID: %d - accion: ESCRIBIR - direccion Fisica: %p - valor: %d", procesoEnEjecucion, (void *)direccion, value8);
            errorCode = escribir_en_memoria(&direccion, procesoEnEjecucion, &value8, sizeof(uint8_t));
        }
        else
        {
            uint32_t value32 = *(uint32_t *)(instruccion->registro2);
            log_info(logger, "PID: %d - accion: ESCRIBIR - direccion Fisica: %p - valor: %d", procesoEnEjecucion, (void *)direccion, value32);
            errorCode = escribir_en_memoria(&direccion, procesoEnEjecucion, &value32, sizeof(uint32_t));
        }

        if (errorCode < 0)
        {
            interrupcionOutOfMemory = true;
        }
        break;
    }
    case MOV_IN:
    {
        // registro2 direccion
        int *direccion;
        if (instruccion->registro28bit)
        {
            direccion = obtener_direccion_fisica(*(uint8_t *)(instruccion->registro2), procesoEnEjecucion);
        }
        else
        {
            direccion = obtener_direccion_fisica(*(uint32_t *)(instruccion->registro2), procesoEnEjecucion);
        }
        // direccion = obtener_direccion_fisica(*(instruccion->registro2),procesoEnEjecucion);
        int tamanio = instruccion->registro18bit ? 1 : 4;
        t_buffer *buffer = leer_en_memoria(procesoEnEjecucion, &direccion, tamanio);
        uint32_t value32;
        uint8_t value8;
        if (instruccion->registro18bit)
        {
            value8 = *(uint8_t *)buffer_read(buffer, sizeof(uint8_t));
            log_info(logger, "PID: %d - accion: LEER - direccion Fisica: %p - valor: %d", procesoEnEjecucion, (void *)direccion, value8);
            memcpy(instruccion->registro1, &value8, sizeof(value8));
        }
        else
        {
            value32 = *(uint32_t *)buffer_read(buffer, sizeof(uint32_t));
            log_info(logger, "PID: %d - accion: LEER - direccion Fisica: %p - valor: %d", procesoEnEjecucion, (void *)direccion, value32);
            memcpy(instruccion->registro1, &value32, sizeof(value32));
        }

        break;
    }
    case IO_STDIN_READ:
    {
        pthread_mutex_lock(&mutex_logger);

        pthread_mutex_unlock(&mutex_logger);
        t_buffer *buffer = buffer_create(sizeof(int *) + strlen(instruccion->interfaz) + sizeof(uint32_t) * 3);
        buffer_add_uint32(buffer, IO_STDIN_READ);
        buffer_add_string(buffer, instruccion->interfaz); // interfaz
        int *direccion;
        int tamanio;
        if (instruccion->registro18bit)
        {
            direccion = obtener_direccion_fisica(*(uint8_t *)(instruccion->registro1), procesoEnEjecucion);
        }
        else
        {
            direccion = obtener_direccion_fisica(*(uint32_t *)(instruccion->registro1), procesoEnEjecucion);
        }
        if (instruccion->registro28bit)
        {
            tamanio = (uint32_t) * (uint8_t *)(instruccion->registro2);
        }
        else
        {
            tamanio = *(instruccion->registro2);
        }
        log_info(logger, "PID: %d - Ejecutando: IO_STDIN_READ - %s %p %d", procesoEnEjecucion, instruccion->interfaz, direccion, *(instruccion->registro2));
        buffer_write(buffer, &direccion, sizeof(int *));
        buffer_add_uint32(buffer, tamanio); // tamaño
        buffer_add_buffer(bufferRetorno, buffer);
        interrupcionIO = true;
        break;
    }
    case IO_STDOUT_WRITE:
    {
        pthread_mutex_lock(&mutex_logger);

        pthread_mutex_unlock(&mutex_logger);

        t_buffer *buffer = buffer_create(sizeof(int *) + strlen(instruccion->interfaz) + sizeof(uint32_t) * 3);
        buffer_add_uint32(buffer, IO_STDOUT_WRITE);
        buffer_add_string(buffer, instruccion->interfaz); // interfaz
        int *direccion;
        if (instruccion->registro18bit)
        {
            direccion = obtener_direccion_fisica(*(uint8_t *)(instruccion->registro1), procesoEnEjecucion);
        }
        else
        {
            direccion = obtener_direccion_fisica(*(uint32_t *)(instruccion->registro1), procesoEnEjecucion);
        } // direccion
        int tamanio;
        if (instruccion->registro28bit)
        {
            tamanio = (uint32_t) * (uint8_t *)(instruccion->registro2);
        }
        else
        {
            tamanio = *(instruccion->registro2);
        }
        log_info(logger, "PID: %d - Ejecutando: IO_STDOUT_WRITE - %s %u %u", procesoEnEjecucion, instruccion->interfaz, direccion, *(instruccion->registro2));
        buffer_write(buffer, &direccion, sizeof(int *));
        buffer_add_uint32(buffer, tamanio); // tamaño
        buffer_add_buffer(bufferRetorno, buffer);
        interrupcionIO = true;
        break;
    }
    case IO_FS_CREATE:
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: IO_FS_CREATE - %s %s", procesoEnEjecucion, instruccion->interfaz, instruccion->archivo);
        pthread_mutex_unlock(&mutex_logger);

        t_buffer *buffer = buffer_create(sizeof(int) * 3 + strlen(instruccion->interfaz) + strlen(instruccion->archivo));
        buffer_add_int(buffer, IO_FS_CREATE);
        buffer_add_string(buffer, instruccion->interfaz);
        buffer_add_string(buffer, instruccion->archivo);
        buffer_add_buffer(bufferRetorno, buffer);
        interrupcionIO = true;
        break;
    }
    case IO_FS_DELETE:
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: IO_FS_DELETE - %s %s", procesoEnEjecucion, instruccion->interfaz, instruccion->archivo);
        pthread_mutex_unlock(&mutex_logger);

        t_buffer *buffer = buffer_create(sizeof(int) * 3 + strlen(instruccion->interfaz) + strlen(instruccion->archivo));
        buffer_add_int(buffer, IO_FS_DELETE);
        buffer_add_string(buffer, instruccion->interfaz);
        buffer_add_string(buffer, instruccion->archivo);
        buffer_add_buffer(bufferRetorno, buffer);
        interrupcionIO = true;
        break;
    }
    case IO_FS_TRUNCATE:
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: IO_FS_TRUNCATE - %s %s %d", procesoEnEjecucion, instruccion->interfaz, instruccion->archivo, *(instruccion->registro1));
        pthread_mutex_unlock(&mutex_logger);

        t_buffer *buffer = buffer_create(sizeof(int) + strlen(instruccion->interfaz) + strlen(instruccion->archivo) + sizeof(uint32_t) * 3);
        buffer_add_int(buffer, IO_FS_TRUNCATE);
        buffer_add_string(buffer, instruccion->interfaz);
        buffer_add_string(buffer, instruccion->archivo);
        if (instruccion->registro18bit)
        {
            buffer_add_uint32(buffer, (uint32_t) * (uint8_t *)(instruccion->registro1));
        }
        {
            buffer_add_uint32(buffer, *(instruccion->registro1));
        }

        buffer_add_buffer(bufferRetorno, buffer);
        interrupcionIO = true;
        break;
    }
    case IO_FS_WRITE:
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: IO_FS_WRITE - %s %u %u", procesoEnEjecucion, instruccion->interfaz, instruccion->archivo, instruccion->registro1, instruccion->registro2, instruccion->registro3);
        pthread_mutex_unlock(&mutex_logger);

        t_buffer *buffer = buffer_create(sizeof(void *) * 2 + strlen(instruccion->interfaz) + strlen(instruccion->archivo) + sizeof(uint32_t) * 3);
        buffer_add_int(buffer, IO_FS_WRITE);
        buffer_add_string(buffer, instruccion->interfaz);
        buffer_add_string(buffer, instruccion->archivo);
        void *direccion;
        if (instruccion->registro18bit)
        {
            direccion = obtener_direccion_fisica((uint32_t) * (uint8_t *)(instruccion->registro1), procesoEnEjecucion);
        }
        else
        {
            direccion = obtener_direccion_fisica(*(instruccion->registro1), procesoEnEjecucion);
        }
        buffer_write(buffer, &direccion, sizeof(void *));
        int tamanio;
        if (instruccion->registro28bit)
        {
            tamanio = (uint32_t) * (uint8_t *)(instruccion->registro2);
        }
        else
        {
            tamanio = *(instruccion->registro2);
        }
        buffer_add_int(buffer, tamanio); // tamaño
        int punteroArchivo;
        if (instruccion->registro38bit)
        {
            punteroArchivo = (uint32_t) * (uint8_t *)(instruccion->registro3);
        }
        else
        {
            punteroArchivo = *(instruccion->registro3);
        }
        buffer_add_int(buffer, punteroArchivo);

        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: IO_FS_WRITE - %s %s %p %d %d", procesoEnEjecucion, instruccion->interfaz, instruccion->archivo, direccion, tamanio, punteroArchivo);
        pthread_mutex_unlock(&mutex_logger);
        buffer_add_buffer(bufferRetorno, buffer);
        interrupcionIO = true;
        break;
    }
    case IO_FS_READ:
    {
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: IO_FS_READ - %s %u %u", procesoEnEjecucion, instruccion->interfaz, instruccion->archivo, instruccion->registro1, instruccion->registro2, instruccion->registro3);
        pthread_mutex_unlock(&mutex_logger);

        t_buffer *buffer = buffer_create(sizeof(void *) * 2 + strlen(instruccion->interfaz) + strlen(instruccion->archivo) + sizeof(uint32_t) * 3);
        buffer_add_int(buffer, IO_FS_READ);
        buffer_add_string(buffer, instruccion->interfaz);
        buffer_add_string(buffer, instruccion->archivo);
        void *direccion;
        if (instruccion->registro18bit)
        {
            direccion = obtener_direccion_fisica((uint32_t) * (uint8_t *)(instruccion->registro1), procesoEnEjecucion);
        }
        else
        {
            direccion = obtener_direccion_fisica(*(instruccion->registro1), procesoEnEjecucion);
        }
        buffer_write(buffer, &direccion, sizeof(void *));
        int tamanio;
        if (instruccion->registro28bit)
        {
            tamanio = (uint32_t) * (uint8_t *)(instruccion->registro2);
        }
        else
        {
            tamanio = *(instruccion->registro2);
        }
        buffer_add_int(buffer, tamanio); // tamaño
        int punteroArchivo;
        if (instruccion->registro38bit)
        {
            punteroArchivo = (uint32_t) * (uint8_t *)(instruccion->registro3);
        }
        else
        {
            punteroArchivo = *(instruccion->registro3);
        }
        buffer_add_int(buffer, punteroArchivo);

        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: IO_FS_READ - %s %s %p %d %d", procesoEnEjecucion, instruccion->interfaz, instruccion->archivo, direccion, tamanio, punteroArchivo);
        pthread_mutex_unlock(&mutex_logger);
        buffer_add_buffer(bufferRetorno, buffer);
        interrupcionIO = true;
        break;
    }
    // Implementar el resto de las operaciones
    case INST_EXIT:
        pthread_mutex_lock(&mutex_logger);
        log_info(logger, "PID: %d - Ejecutando: %u", procesoEnEjecucion, instruccion->instruccion);
        pthread_mutex_unlock(&mutex_logger);
        finProceso = true;
        break;
    case SIGNAL:
    {
        t_buffer *buffer = buffer_create(strlen(instruccion->recurso) + sizeof(int) * 2);
        operaciones_recursos operacion = OP_SIGNAL;
        buffer_add_uint32(buffer, operacion);
        buffer_add_string(buffer, instruccion->recurso);
        buffer_add_buffer(bufferRetorno, buffer);
        interrupcionRecursos = true;
        break;
    }

    case WAIT:
    {
        t_buffer *buffer = buffer_create(strlen(instruccion->recurso) + sizeof(int) * 2);
        operaciones_recursos operacion = OP_WAIT;
        buffer_add_uint32(buffer, operacion);
        buffer_add_string(buffer, instruccion->recurso);
        buffer_add_buffer(bufferRetorno, buffer);
        interrupcionRecursos = true;
        break;
    }
    case COPY_STRING:
    {
        int tamanio = instruccion->valor;
        int *direccionSource = obtener_direccion_fisica(*(instruccion->registro1), procesoEnEjecucion);
        int *direccionDestino = obtener_direccion_fisica(*(instruccion->registro2), procesoEnEjecucion);
        t_buffer *buffer = leer_en_memoria(procesoEnEjecucion, &direccionSource, tamanio);
        void *value = buffer_read(buffer, tamanio);
        escribir_en_memoria(&direccionDestino, procesoEnEjecucion, value, tamanio);
        break;
    }

    default:
        pthread_mutex_lock(&mutex_logger);
        log_warning(logger, "PID: %d - Instrucción desconocida: %u", procesoEnEjecucion, instruccion->instruccion);
        pthread_mutex_unlock(&mutex_logger);
        break;
    }
    return interrupcionInterna;
}