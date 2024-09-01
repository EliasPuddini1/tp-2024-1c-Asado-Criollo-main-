#ifndef COMMONS_H_
#define COMMONS_H_

#include <commons/log.h>
#include <sys/socket.h>
#include <stdint.h>

typedef enum
{
	KERNEL,
	MEMORIA,
	IO,
	CPU
} modulos;

typedef enum
{
	NEW,
	READY,
	EXEC,
	BLOCKED,
	EXIT
} estados_proceso;

typedef enum
{
    GENERICA,
    STDIN,
    STDOUT,
    DIALFS
} tipo_io;

typedef enum
{
    OP_IO_GEN_SLEEP,
    OP_IO_STDIN_READ,
    OP_IO_STDOUT_WRITE,
    OP_IO_FS_CREATE,
    OP_IO_FS_DELETE,
    OP_IO_FS_TRUNCATE,
    OP_IO_FS_WRITE,
    OP_IO_FS_READ
}operaciones_IO;
typedef enum
{
    OP_WAIT,
    OP_SIGNAL
}operaciones_recursos;
typedef struct{
    uint32_t PC;
    uint8_t AX, BX, CX, DX;
    uint32_t EAX, EBX, ECX, EDX;
    uint32_t SI, DI;
} Registros;
typedef struct 
{
	uint32_t PID;
	uint32_t PC;
	uint32_t Quantum;
	uint32_t Estado;
	Registros *Registros;
} PCB;

typedef struct
{
    uint32_t size; // Tamaño del payload
    uint32_t offset; // Desplazamiento dentro del payload
    void* stream; // Payload
} t_buffer;

typedef enum
{
    SET,
    MOV_IN,
    MOV_OUT,
    SUM,
    SUB,
    JNZ,
    RESIZE,
    COPY_STRING,
    WAIT,
	SIGNAL,
    IO_GEN_SLEEP,
    IO_STDIN_READ,
    IO_STDOUT_WRITE,
    IO_FS_CREATE,
    IO_FS_DELETE,
    IO_FS_TRUNCATE,
    IO_FS_WRITE,
    IO_FS_READ,
    INST_EXIT,
	INVALID_INSTRUCTION
} instruccion;
typedef struct
{
	instruccion instruccion;
	uint32_t *registro1, *registro2, *registro3;
    bool registro18bit,registro28bit,registro38bit;
	uint32_t valor; // Dato, tamaño, unidades de trabajo
	char *interfaz;
	char *archivo;
    char* recurso;
} t_instruccion;
typedef struct
{
    uint8_t op_code;
    t_buffer* buffer;
} t_paquete;

typedef enum{
    WRITE,
    READ,
    DIRECCION_LOGICA
}accion;

typedef enum{
    WRITE_OK = 6,
    WRITE_FAULT = -6,
    READ_OK = 7,
    READ_FAULT = -7
}estado_memoria;

typedef struct{
    unsigned int frame;
    unsigned int desplazamiento;
    int pagina;
    accion accion_solicitada;
    uint32_t dato;
    estado_memoria estado;
} __attribute__((packed)) t_solicitud;


PCB * CrearPCB(int PID,int quantum);
void PrintPCB(PCB p);
void LiberarPCB(PCB*p);
char* DeserializarString(t_buffer* buffer);
t_buffer * SerializarString(char* mensaje);
char* getNombreModulo(int modulo);
t_buffer *bufferPCB(PCB *p);
t_buffer *buffer_create(int size);
void buffer_destroy(t_buffer *buffer);
void buffer_add_buffer(t_buffer* buffer1, t_buffer* buffer2);
void buffer_add_string(t_buffer *buffer, char *string);
void buffer_add_int(t_buffer *buffer, int data);
void buffer_add_uint32(t_buffer *buffer, uint32_t data);
void buffer_add_uint8(t_buffer *buffer, uint8_t data);
int buffer_read_int(t_buffer *buffer);
uint32_t buffer_read_uint32(t_buffer *buffer);
uint8_t buffer_read_uint8(t_buffer *buffer);
char* buffer_read_string(t_buffer *buffer, int length);
void buffer_write(t_buffer* buffer, void* value, int size);
void* buffer_read(t_buffer * buffer,int size);
t_buffer* SerializarPCB(PCB *p);
PCB* DeserializarPCB(t_buffer* buffer);
void EnviarOperacion(int socket, int op_code, t_buffer * buffer);
int recibirOperacion(int socket);
t_buffer * RecibirBuffer(int socket);
void EnviarBuffer(int socket,t_buffer *buffer);
char* get_instruction_name(int value);
void SerializarPIDyPC(t_buffer *buffer, uint32_t pid, uint32_t pc);
int buffer_get_int(t_buffer* buffer);
void insertAtEnd(char *str, const char *value);
void deleteValue(char *str, const char *value);

//CPU + MEMORIA
void EnviarBuffer(int socket,t_buffer * buffer);
void InicializarRegistros(Registros *r);
t_buffer * buffer_get_sobrante(t_buffer * buffer);
#endif