#include "commons.h"
#include <string.h>
#include <stdlib.h>
#include <commons/string.h>
#include <stdlib.h>
#include <string.h>

char *getNombreModulo(int modulo)
{
	char *nombre;
	switch (modulo)
	{
	case 0:
		nombre = "Kernel";
		break;
	case 1:
		nombre = "Memoria";
		break;
	case 2:
		nombre = "IO";
		break;
	case 3:
		nombre = "CPU";
		break;

	default:
		nombre = "No existe ese modulo";
		break;
	}
	return nombre;
}
void InicializarRegistros(Registros *r)
{
	r->PC = 0;
	r->AX = 0;
	r->BX = 0;
	r->CX = 0;
	r->DI = 0;
	r->DX = 0;
	r->EAX = 0;
	r->EBX = 0;
	r->ECX = 0;
	r->EDX = 0;
	r->SI = 0;
}

PCB * CrearPCB(int PID, int quantum)
{
	PCB * p = malloc(sizeof(PCB));
	p->PC = 0;
	p->PID = PID;
	p->Estado = NEW;
	p->Quantum = quantum;
	p->Registros = malloc(sizeof(Registros));
	InicializarRegistros(p->Registros);
	return p;
}

void LiberarPCB(PCB *p)
{
    if (p != NULL)
    {
        if (p->Registros != NULL)
        {
            free(p->Registros);
        }
        free(p);
    }
}

void PrintPCB(PCB p)
{
	char *estado;
	switch (p.Estado)
	{
	case 0:
		estado = "NEW";
		break;
	case 1:
		estado = "READY";
		break;
	case 2:
		estado = "EXEC";
		break;
	case 3:
		estado = "BLOCKED";
		break;
	case 4:
		estado = "EXIT";
		break;
	default:
		break;
	}
	printf("PID: %d. Estado: %s\n", p.PID, estado);
}
//Agrega el segundo buffer al primero, sin mover el offset
void buffer_add_buffer(t_buffer* buffer1, t_buffer* buffer2)
{
	int offset = buffer1->size;
	buffer1->size = buffer1->size + buffer2->size;
	void *temp = realloc(buffer1->stream, buffer1->size);
	if (temp == NULL && buffer1->size != 0) {
		// Handle allocation error, e.g., log an error message or exit
		fprintf(stderr, "Memory allocation failed\n");
		// Decide on the error handling strategy (exit, return error code, etc.)
	} else {
	buffer1->stream = temp;
	}
	memcpy(buffer1->stream + offset,buffer2->stream,buffer2->size);
}

void buffer_add_int(t_buffer *buffer, int data)
{
	memcpy(buffer->stream + buffer->offset, &data, sizeof(int));
	buffer->offset += sizeof(int);
}
// Agrega un uint32_t al buffer en la posición actual y avanza el offset
void buffer_add_uint32(t_buffer *buffer, uint32_t data)
{
	memcpy(buffer->stream + buffer->offset, &data, sizeof(uint32_t));
	buffer->offset += sizeof(uint32_t);
}

// Agrega un uint8_t al buffer
void buffer_add_uint8(t_buffer *buffer, uint8_t data)
{
	memcpy(buffer->stream + buffer->offset, &data, sizeof(uint8_t));
	buffer->offset += sizeof(uint8_t);
}

// Agrega string al buffer con un uint32_t adelante
void buffer_add_string(t_buffer *buffer, char *string)
{
	int length = strlen(string);
	memcpy(buffer->stream + buffer->offset, &length, sizeof(uint32_t));
	buffer->offset += sizeof(uint32_t);
	memcpy(buffer->stream + buffer->offset, string, length);
	buffer->offset += length;
}
//Lee un string del buffer
char* buffer_read_string(t_buffer* buffer, int length)
{
	char* string = malloc(length+1);
	for(int i = 0; i<length;i++)
	{
		string[i] = *(char*)(buffer->stream + buffer->offset + i);
	}
	string[length] = '\0';
	buffer->offset += length;
	return string;
}

int buffer_read_int(t_buffer *buffer)
{
	int i = *(int *)(buffer->stream + buffer->offset);
	buffer->offset += sizeof(int);
	return i;
}

// Lee un uint32_t del buffer en la dirección data y avanza el offset
uint32_t buffer_read_uint32(t_buffer *buffer)
{
	uint32_t i = *(uint32_t *)(buffer->stream + buffer->offset);
	buffer->offset += sizeof(uint32_t);
	return i;
}
// Lee un uint8_t del buffer
uint8_t buffer_read_uint8(t_buffer *buffer)
{
	uint8_t i = *(uint8_t *)(buffer->stream + buffer->offset);
	buffer->offset += sizeof(uint8_t);
	return i;
}

// Crea el buffer para posterior uso
t_buffer *buffer_create(int size)
{
	t_buffer *buffer = malloc(sizeof(t_buffer));
	buffer->offset = 0;
	buffer->size = size;
	buffer->stream = malloc(size);
	return buffer;
}
// Libera la memoria asociada al buffer
void buffer_destroy(t_buffer *buffer)
{
	free(buffer->stream);

	free(buffer);
}

void* buffer_read(t_buffer * buffer,int size)
{
	void* value = malloc(size);
	memcpy(value,buffer->stream+buffer->offset,size);
	buffer->offset +=size;
	return value;
}

t_buffer * buffer_get_sobrante(t_buffer * buffer)
{
	int sobrante = buffer->size-buffer->offset;
	t_buffer * bufferNuevo = buffer_create(sobrante);
	memcpy(bufferNuevo->stream,buffer->stream+ buffer->offset,sobrante);
	return bufferNuevo;
}

void buffer_write(t_buffer* buffer, void* value, int size)
{
	memcpy(buffer->stream+buffer->offset,value,size);
	buffer->offset +=size;
}
// Funciones de Serializacion


void SerializarRegistros(t_buffer *buffer, Registros *R)
{
	buffer_add_uint32(buffer, R->PC);
	buffer_add_uint8(buffer, R->AX);
	buffer_add_uint8(buffer, R->BX);
	buffer_add_uint8(buffer, R->CX);
	buffer_add_uint8(buffer, R->DX);
	buffer_add_uint32(buffer, R->EAX);
	buffer_add_uint32(buffer, R->EBX);
	buffer_add_uint32(buffer, R->ECX);
	buffer_add_uint32(buffer, R->EDX);
	buffer_add_uint32(buffer, R->SI);
	buffer_add_uint32(buffer, R->DI);
}

t_buffer *SerializarPCB(PCB *p)
{
	t_buffer *buffer = buffer_create(
		sizeof(uint32_t) * 5 + // PID, Program Counter, Quantum, Estado y Registro PC
		sizeof(uint8_t) * 4 +  // Registros AX, BX, CX y DX
		sizeof(uint32_t) * 6   // Registros EAX, EBX, ECX, EDX, SI y DI
	);

	buffer_add_uint32(buffer, p->PID);
	buffer_add_uint32(buffer, p->PC);
	buffer_add_uint32(buffer, p->Quantum);
	buffer_add_uint32(buffer, p->Estado);
	SerializarRegistros(buffer, p->Registros);

	return buffer;
}

t_buffer* SerializarString(char *mensaje)
{
	t_buffer * buffer = buffer_create(strlen(mensaje)+sizeof(int));
	buffer_add_string(buffer,mensaje);
	return buffer;
}

//Funciones de Deserializacion
Registros *DeserializarRegistros(t_buffer *buffer)
{
	Registros *R = malloc(sizeof(Registros));

	R->PC = buffer_read_uint32(buffer);
	R->AX = buffer_read_uint8(buffer);
	R->BX = buffer_read_uint8(buffer);
	R->CX = buffer_read_uint8(buffer);
	R->DX = buffer_read_uint8(buffer);
	R->EAX = buffer_read_uint32(buffer);
	R->EBX = buffer_read_uint32(buffer);
	R->ECX = buffer_read_uint32(buffer);
	R->EDX = buffer_read_uint32(buffer);
	R->SI = buffer_read_uint32(buffer);
	R->DI = buffer_read_uint32(buffer);
	return R;
}

PCB *DeserializarPCB(t_buffer *buffer)
{
	PCB *p = malloc(sizeof(PCB));

	p->PID = buffer_read_uint32(buffer);
	p->PC = buffer_read_uint32(buffer);
	p->Quantum = buffer_read_uint32(buffer);
	p->Estado = buffer_read_uint32(buffer);
	p->Registros = DeserializarRegistros(buffer);
	return p;
}

char* DeserializarString(t_buffer* buffer)
{
	int size = buffer_read_uint32(buffer);
	char* string = buffer_read_string(buffer,size);
	return string;
}
// Envia la operacion, que siempre es op_code + buffer TODO: Manejo de errores
void EnviarOperacion(int socket, int op_code, t_buffer *buffer)
{
	send(socket, &op_code, sizeof(int), 0);
	EnviarBuffer(socket,buffer);
}

void EnviarBuffer(int socket,t_buffer * buffer)
{
	send(socket, &(buffer->size),sizeof(int),0);
	send(socket, buffer->stream, buffer->size, 0);
}

t_buffer * RecibirBuffer(int socket)
{
	int size;
    recv(socket, &size, sizeof(int), 0);
	t_buffer * buffer = buffer_create(size);
	recv(socket,buffer->stream,buffer->size,0);
	return buffer;
}

void asignarMemoriaPara_uint32(uint32_t **integer){
    if (*integer == NULL) {
        *integer = malloc(sizeof(uint32_t));
    }
}

void asignarMemoriaPara_char(char **string){
    if (*string == NULL) {
        *string = malloc(256 * sizeof(char)); // Asignar 256 caracteres por defecto
    (*string)[0] = '\0'; // Inicializar la cadena vacía
    }
}

char* get_instruction_name(int value)
{
    switch (value)
    {
        case SET: return "SET";
        case MOV_IN: return "MOV_IN";
        case MOV_OUT: return "MOV_OUT";
        case SUM: return "SUM";
        case SUB: return "SUB";
        case JNZ: return "JNZ";
        case RESIZE: return "RESIZE";
        case COPY_STRING: return "COPY_STRING";
        case WAIT: return "WAIT";
        case SIGNAL: return "SIGNAL";
        case IO_GEN_SLEEP: return "IO_GEN_SLEEP";
        case IO_STDIN_READ: return "IO_STDIN_READ";
        case IO_STDOUT_WRITE: return "IO_STDOUT_WRITE";
        case IO_FS_CREATE: return "IO_FS_CREATE";
        case IO_FS_DELETE: return "IO_FS_DELETE";
        case IO_FS_TRUNCATE: return "IO_FS_TRUNCATE";
        case IO_FS_WRITE: return "IO_FS_WRITE";
        case IO_FS_READ: return "IO_FS_READ";
        case INST_EXIT: return "INST_EXIT";
        case INVALID_INSTRUCTION: return "INVALID_INSTRUCTION";
        default: return "UNKNOWN_INSTRUCTION";
    }
}

void insertAtEnd(char *str, const char *value) {
    size_t len = strlen(str);
    if (len > 1) {
        // Replace the closing bracket with a comma and then append the value and closing bracket
        str[len - 1] = ',';
        strcat(str, value);
        strcat(str, "]");
    } else {
        // If the string is empty, just add the value inside the brackets
        strcat(str, value);
        strcat(str, "]");
    }
}

void deleteValue(char *str, const char *value) {
    char *pos = strstr(str, value);
    if (pos) {
        size_t len = strlen(value);
        if (pos[len] == ',') {
            // If the value is followed by a comma, remove the value and the comma
            memmove(pos, pos + len + 1, strlen(pos + len + 1) + 1);
        } else if (pos > str + 1 && *(pos - 1) == ',') {
            // If the value is preceded by a comma, remove the value and the preceding comma
            memmove(pos - 1, pos + len, strlen(pos + len) + 1);
        } else {
            // If the value is the only one, remove it directly
            memmove(pos, pos + len, strlen(pos + len) + 1);
        }
    }
}