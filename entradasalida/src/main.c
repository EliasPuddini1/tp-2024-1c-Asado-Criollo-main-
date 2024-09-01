#include "../include/main.h"
#include "DialFS.h"
int conexion_kernel;
int conexion_memoria;
int unidad_tiempo;
int retraso_compactacion;
typedef struct 
{
   long base;
   long size;
}archivo;

char * bitmap_path;
char* root_path;
char* nombres_path;
BlockFile* blockfile;

int main(int argc, char *argv[]) {
    // Config Inicial
    if(argc != 3)
    {
        printf("Argumentos invalidos");
        exit(-1);
    }
    char* nombreIO = argv[1];
    char* pathConfig = argv[2];
    InterfazInit *I = malloc(sizeof(InterfazInit));

    logger = log_create("EntradaSalida.log", nombreIO, true, LOG_LEVEL_INFO);
    Interfaz(nombreIO,pathConfig);
    return 0;
}

void* Interfaz(char* nombre, char* pathConfig) {

    // Verificar si el archivo de configuración es accesible
    FILE *file = fopen(pathConfig, "r");
    if (file == NULL) {
        log_info(logger, "El archivo de configuración %s no existe. Abortando...", pathConfig);
        exit(-1);
    } else {
        fclose(file);
    }

    t_config* config = config_create(pathConfig);
    if (config == NULL) 
    {
        log_error(logger, "Error al crear la configuración con el archivo %s", pathConfig);
        exit(-1);
    }
    char *tipoInterfaz = config_get_string_value(config, "TIPO_INTERFAZ");
    if (tipoInterfaz == NULL) 
    {
        log_error(logger, "No se encontró la clave TIPO_INTERFAZ en el archivo de configuración");
        exit(-1);
    }
    char *ip = config_get_string_value(config, "IP_KERNEL");
    char *puerto = config_get_string_value(config, "PUERTO_KERNEL");
    if (ip == NULL || puerto == NULL) 
    {
        log_error(logger, "No se encontraron IP o Puerto en el archivo de configuración");
        config_destroy(config);
        return NULL;
    }
    conexion_kernel = conectar(ip, puerto, IO);
    int tipo;

    if (strcmp(tipoInterfaz, "GENERICA") == 0) {
        tipo = GENERICA;
    } else if (strcmp(tipoInterfaz, "STDIN") == 0) {
        tipo = STDIN;
    } else if (strcmp(tipoInterfaz, "STDOUT") == 0) {
        tipo = STDOUT;
    } else if (strcmp(tipoInterfaz, "DIALFS") == 0) {
        tipo = DIALFS;
    }
    if(tipo == STDOUT || tipo == STDIN || tipo == DIALFS)
    {
        conexion_memoria = conexionMemoria(config);
    }
    if(tipo == DIALFS)
    {
        InicializarDialFS(config);
    }
    if(tipo == DIALFS || tipo == GENERICA)
    {
        unidad_tiempo = config_get_int_value(config, "TIEMPO_UNIDAD_TRABAJO");
    }
    t_buffer* buffer = buffer_create(sizeof(int));
    buffer_add_uint32(buffer, tipo);
    t_buffer* bufferNombre = SerializarString(nombre);
    buffer_add_buffer(buffer, bufferNombre);
    EnviarOperacion(conexion_kernel, CONECTAR_INTERFAZ, buffer);
    buffer_destroy(buffer);
    buffer_destroy(bufferNombre);
    int cod_op;
    cod_op = recibir_operacion(conexion_kernel);
    t_instruccion* instruccion = malloc(sizeof(t_instruccion));

    while (cod_op >= 0) {
        switch (cod_op) {
        case RECIBIR_INSTRUCCION:
            buffer = RecibirBuffer(conexion_kernel);
            instruccion->instruccion = buffer_read_uint32(buffer);
            int pid = buffer_read_uint32(buffer);
            switch(instruccion->instruccion)
            {
                case IO_GEN_SLEEP:
                    instruccion->valor = buffer_read_uint32(buffer);
                    break;
                case IO_STDIN_READ:
                    {
                        uint32_t* reg1 =*(uint32_t**)buffer_read(buffer,sizeof(int*));
                        uint32_t reg2 = buffer_read_uint32(buffer);
                        instruccion->registro1 = reg1;
                        instruccion->registro2 = &reg2;
                    }
                    break;
                case IO_STDOUT_WRITE:
                    {
                        uint32_t* reg1 = *(uint32_t**)buffer_read(buffer,sizeof(int*));
                        uint32_t reg2 = buffer_read_uint32(buffer);
                        instruccion->registro1 = reg1;
                        instruccion->registro2 = &reg2;
                    }
                    instruccion->valor = buffer_read_uint32(buffer);
                    break;
                case IO_FS_CREATE:
                case IO_FS_DELETE:
                    instruccion->archivo = DeserializarString(buffer);
                    break;
                case IO_FS_TRUNCATE:
                    instruccion->archivo = DeserializarString(buffer);
                    uint32_t reg1 = buffer_read_uint32(buffer);
                    instruccion->registro1 = &reg1;
                    break;
                case IO_FS_READ:
                {
                    instruccion->archivo = DeserializarString(buffer);
                    uint32_t* reg1 = *(uint32_t**)buffer_read(buffer,sizeof(int*));
                    instruccion->registro1 = reg1;
                    uint32_t reg2 = buffer_read_uint32(buffer);
                    instruccion->registro2 = &reg2;
                    uint32_t reg3 = buffer_read_uint32(buffer);
                    instruccion->registro3 = &reg3;
                    break;
                }
                    
                case IO_FS_WRITE:
                {
                    instruccion->archivo = DeserializarString(buffer);
                    uint32_t* reg1 = *(uint32_t**)buffer_read(buffer,sizeof(int*));
                    instruccion->registro1 = reg1;
                    uint32_t reg2 = buffer_read_uint32(buffer);
                    instruccion->registro2 = &reg2;
                    uint32_t reg3 = buffer_read_uint32(buffer);
                    instruccion->registro3 = &reg3;
                    break;
                } 
            }
            
            
            // TO DO
            int error = EjecutarInstruccion(instruccion,pid);
            t_buffer* bufferRespuesta = buffer_create(sizeof(int));
            buffer_add_uint32(bufferRespuesta,error);
            EnviarBuffer(conexion_kernel,bufferRespuesta);
            buffer_destroy(bufferRespuesta);
            break;
        // TO DO Implementar las demas operaciones que recibira de los clientes
        default:
            break;
        }
        cod_op = recibir_operacion(conexion_kernel);
    }
    free(instruccion);
    config_destroy(config);
    return NULL;
}

int conexionMemoria(t_config* config) {
    char* ip = config_get_string_value(config, "IP_MEMORIA");
    char* puerto = config_get_string_value(config, "PUERTO_MEMORIA");

    log_info(logger, "Conectando a Memoria en %s:%s...", ip, puerto);
    int conexion = conectar(ip, puerto, IO);

    return conexion;
}

int EjecutarInstruccion(t_instruccion* instruccion,int pid) {
    int error_code;
    char* nombre_instruccion = get_instruction_name(instruccion->instruccion);
    log_info(logger,"PID: %d - Operacion: %s",pid,nombre_instruccion);
    switch (instruccion->instruccion) {
        case IO_GEN_SLEEP:
            error_code = handle_IO_GEN_SLEEP(instruccion->valor*unidad_tiempo);
            break;
        case IO_STDIN_READ:
            error_code = handle_IO_STDIN_READ(instruccion->registro1, instruccion->registro2,conexion_memoria,pid);
            break;
        case IO_STDOUT_WRITE:
            error_code = handle_IO_STDOUT_WRITE(instruccion->registro1, instruccion->registro2,conexion_memoria,pid);
            break;
        case IO_FS_CREATE:
            log_info(logger,"PID: %d Crear Archivo: %s",pid,instruccion->archivo);
            usleep(unidad_tiempo*1000);
            error_code = handle_IO_FS_CREATE(instruccion->archivo);
            break;
        case IO_FS_DELETE:
            log_info(logger,"PID: %d Eliminar Archivo: %s",pid,instruccion->archivo);
            usleep(unidad_tiempo*1000);
            error_code = handle_IO_FS_DELETE(instruccion->archivo);
            break;
        case IO_FS_TRUNCATE:
            log_info(logger,"PID: %d Truncar Archivo: %s - Tamaño %d",pid,instruccion->archivo,*(instruccion->registro1));
            usleep(unidad_tiempo*1000);
            error_code = handle_IO_FS_TRUNCATE(instruccion->archivo, instruccion->registro1,pid);
            break;
        case IO_FS_WRITE:
            log_info("PID: %d Escribir Archivo: %s - Tamaño a Escribir %d - Puntero Archivo %d",pid,instruccion->archivo,*(instruccion->registro2),*(instruccion->registro3));
            usleep(unidad_tiempo*1000);
            error_code = handle_IO_FS_WRITE(instruccion->archivo, instruccion->registro1, instruccion->registro2, instruccion->registro3,pid,conexion_memoria);
            break;
        case IO_FS_READ:
            log_info(logger,"PID: %d Leer Archivo: %s - Tamaño a Leer %d - Puntero Archivo %d",pid,instruccion->archivo,*(instruccion->registro2),*(instruccion->registro3));
            usleep(unidad_tiempo*1000);
            error_code = handle_IO_FS_READ(instruccion->archivo, instruccion->registro1, instruccion->registro2, instruccion->registro3,pid,conexion_memoria);
            break;
        default:
            log_error(logger, "Instrucción no reconocida: %d", instruccion->instruccion);
            error_code = -1;
            break;
    }
    return error_code;
}

void InicializarDialFS(t_config * config)
{
    blockfile = malloc(sizeof(BlockFile));
    blockfile->block_size = config_get_int_value(config, "BLOCK_SIZE");
    blockfile->block_count = config_get_int_value(config, "BLOCK_COUNT");
    root_path = config_get_string_value(config, "PATH_BASE_DIALFS");
    char* nombres = config_get_string_value(config, "ARCHIVO_NOMBRES");
    retraso_compactacion = config_get_int_value(config, "RETRASO_COMPACTACION");
    
    char* bitmap_filename = "bitmap.dat";
    char* bloques_filename = "bloques.dat";

    nombres_path = malloc(strlen(root_path) + strlen(nombres) + 1);
    bitmap_path = malloc(strlen(root_path) + strlen(bitmap_filename) + 1);
    char* path_bloques = malloc(strlen(root_path) + strlen(bloques_filename) + 1);

    if (!nombres_path || !bitmap_path || !path_bloques) {
        fprintf(stderr, "Error allocating memory.\n");
        // Free allocated memory before exiting
        free(blockfile);
        free(nombres_path);
        free(bitmap_path);
        free(path_bloques);
        return;
    }

    strcpy(nombres_path, root_path);
    strcat(nombres_path, nombres);

    strcpy(bitmap_path, root_path);
    strcat(bitmap_path, bitmap_filename);

    strcpy(path_bloques, root_path);
    strcat(path_bloques, bloques_filename);
    FILE* bm = fopen(bitmap_path, "rb+");
    if(!bm)
    {
        Bitmap* bitmap = CreateBitmap(blockfile->block_count);
        GuardarBitmapToFile(bitmap, bitmap_path);
    }
    blockfile->path_bloques = path_bloques;

    FILE* f = fopen(path_bloques, "rb+");
    if (!f) {
        CrearBlockFile(path_bloques, blockfile->block_size * blockfile->block_count);
    } else {
        fclose(f);
    }
    f = fopen(nombres_path, "r");
    if (!f) {
        f = fopen(nombres_path, "w");
        fclose(f);
    }
    else{
        fclose(f);
    }
}