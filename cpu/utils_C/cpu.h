#ifndef CPU_H_
#define CPU_H_

#include "./main.h"
#include "commons.h"

/*
typedef struct {
    PCB *p;
    Registros *registros;
    int socketMemoria;
    int socketCliente;
    bool *interrupcionRecibida;
} cicloParams;
*/

typedef enum
{
    FIN_PROCESO,
    INTERRUPCION_EXTERNA,
    OP_IO,
    OP_RECURSOS,
    OP_OUT_OF_MEMORY
}interrupt_code;
typedef struct {
    uint8_t* reg8;
    uint32_t* reg32;
    bool is8bit;
} RegisterPointer;

t_buffer* EjecutarProceso(void *args,int* codigo);
//void InicializarRegistros(Registros *reg);
char* fetchInstruccion(int socketMemoria, PCB *p);
void inicializarInstruccion(t_instruccion *instruccion);
uint8_t * obtenerRegistro_uint8(Registros* registros, char* token);
uint32_t * obtenerRegistro_uint32(Registros* registros, char* token);
RegisterPointer obtenerRegistro(Registros* registros, char* token);

instruccion obtenerInstruccion(char *token);
t_instruccion parsearInstruccion(char* instruccion, Registros *registros);
t_instruccion decodeInstruccion(char* instruccion, Registros *registros);

void asignarMemoriaParaInt(uint32_t **ptr);
void asignarMemoriaParaString(char **ptr);
#endif