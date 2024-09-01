#include "DialFS.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commons.h"
extern char * bitmap_path;
extern BlockFile* blockfile;
extern t_log* logger;
extern char* root_path;
extern char* nombres_path;
extern int retraso_compactacion;
Bitmap* CreateBitmap(size_t size) {
    Bitmap *bitmap = (Bitmap *)malloc(sizeof(Bitmap));
    if (!bitmap) {
        return NULL;
    }
    bitmap->size = size;
    bitmap->free_space = size;
    bitmap->bits = (unsigned char *)calloc((size + 7) / 8, sizeof(unsigned char));
    if (!bitmap->bits) {
        free(bitmap);
        return NULL;
    }
    return bitmap;
}

void SetBit(Bitmap *bitmap, size_t index) {
    if (index < bitmap->size) {
        bitmap->bits[index / 8] |= (1 << (index % 8));
    }
}

void LimpiarBit(Bitmap *bitmap, size_t index) {
    if (index < bitmap->size) {
        bitmap->bits[index / 8] &= ~(1 << (index % 8));
    }
}

int IsSet(const Bitmap *bitmap, size_t index) {
    if (index < bitmap->size) {
        return bitmap->bits[index / 8] & (1 << (index % 8));
    }
    return 0;
}

void DestroyBitmap(Bitmap *bitmap) {
    if (bitmap) {
        free(bitmap->bits);
        free(bitmap);
    }
}

int GuardarBitmapToFile(const Bitmap *bitmap, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        return -1;
    }
    fwrite(&bitmap->size, sizeof(size_t), 1, file);
    fwrite(&bitmap->free_space, sizeof(size_t), 1, file);
    fwrite(bitmap->bits, sizeof(unsigned char), (bitmap->size + 7) / 8, file);
    fclose(file);
    return 0;
}

Bitmap* CargarBitmapDesdeFile(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return NULL;
    }
    Bitmap *bitmap = (Bitmap *)malloc(sizeof(Bitmap));
    if (!bitmap) {
        fclose(file);
        return NULL;
    }
    fread(&bitmap->size, sizeof(size_t), 1, file);
    fread(&bitmap->free_space, sizeof(size_t), 1, file);
    bitmap->bits = (unsigned char *)calloc((bitmap->size + 7) / 8, sizeof(unsigned char));
    if (!bitmap->bits) {
        free(bitmap);
        fclose(file);
        return NULL;
    }
    fread(bitmap->bits, sizeof(unsigned char), (bitmap->size + 7) / 8, file);
    fclose(file);
    return bitmap;
}

void CrearBlockFile(char* filename, int size) {
    FILE *file = fopen(filename, "wb");

    unsigned char *buffer = malloc(size);
    memset(buffer, 0, size);

    fwrite(buffer, sizeof(unsigned char), size, file);
    free(buffer);
    fclose(file);
}
/* void EscribirEnArchivoBloques(char* path,void* data, size_t dataSize, long position) {
    FILE *file = fopen(path, "rb+");
    fseek(file, position, SEEK_SET);
    fwrite(data, dataSize, 1, file);
    fclose(file);
} */

int CrearFile(char* filename)
{
    
    Bitmap* bitmap = CargarBitmapDesdeFile(bitmap_path);
    int base = GetNextFreeSpace(bitmap);
    char* path = getPath(filename);
    FILE* f  = fopen(path,"r");
    //Si ya existe el archivo devuelve error
    if(f)
    {
        return -1;
    }
    if(base >= 0)
    {
        CrearFileMetadata(filename,base);
        //EscribirEnArchivoBloques(path_bloques,0,1,base);
    }
    else
    {
        log_info(logger,"No hay espacio disponible para crear el archivo");
        return -1;
    }
    AsignarEspacioBitmap(bitmap,base,1);
    GuardarBitmapToFile(bitmap,bitmap_path);

    ActualizarArchivoNombres(filename,true);
    
}
int BorrarFile(char* filename)
{
    char* path = getPath(filename);
    t_config* config_file = config_create(path);
    int base = config_get_int_value(config_file,"BLOQUE_INICIAL");
    int size = config_get_int_value(config_file,"TAMANIO_ARCHIVO");
    int blocksize = size%blockfile->block_size==0?(size/blockfile->block_size):(size/blockfile->block_size)+1;
    int error = remove(path);
    if(error !=0)
    {
        return -1;
    }
    Bitmap* bitmap = CargarBitmapDesdeFile(bitmap_path);
    LiberarEspacioBitmap(bitmap,base,blocksize);
    GuardarBitmapToFile(bitmap,bitmap_path);
    free(bitmap);
    config_destroy(config_file);
    ActualizarArchivoNombres(filename,false);
    free(path);
    return 0;

}
void CrearFileMetadata(char* filename,int base)
{
    char* path = getPath(filename);
    t_config* config = config_create("metadata_base.config");
    char value[20];
    sprintf(value,"%d",base);
    config_set_value(config,"BLOQUE_INICIAL",value);
    char value2[20];
    sprintf(value2,"%d",blockfile->block_size);
    config_set_value(config,"TAMANIO_ARCHIVO",value2);
    config_save_in_file(config,path);
    free(path);
}

void AsignarEspacioBitmap(Bitmap* bitmap,int posicion,int size)
{
    for(int j =posicion; j<posicion+size;j++)
    {
        if(!IsSet(bitmap,j))
        {
            SetBit(bitmap,j);
            bitmap->free_space --;
        }
        
    }
    
}

void LiberarEspacioBitmap(Bitmap* bitmap,int posicion,int size)
{
    if(size == 0)
    {
        size = 1;
    }

    for(int j =posicion; j<posicion+size;j++)
    {
        if(IsSet(bitmap,j))
        {
            LimpiarBit(bitmap,j);
            bitmap->free_space++;
        }
    }
}

int TruncateFile(char* filename,int newsize,int pid)
{
    char* path = getPath(filename);
    t_config* config_file = config_create(path);
    int base = config_get_int_value(config_file,"BLOQUE_INICIAL");
    int size = config_get_int_value(config_file,"TAMANIO_ARCHIVO");
    int blocksize = size%blockfile->block_size==0?(size/blockfile->block_size):(size/blockfile->block_size)+1;
    int end = base+blocksize;
    int newblocksize = newsize%blockfile->block_size==0?(newsize/blockfile->block_size):(newsize/blockfile->block_size)+1;
    Bitmap* bitmap = CargarBitmapDesdeFile(bitmap_path);
    
    if(blocksize > newblocksize)
    {
        int dif = blocksize -newblocksize;
        LiberarEspacioBitmap(bitmap,blocksize-dif,dif);
        ActualizarMetadata(filename,base,newsize);
    }
    else if(blocksize < newblocksize)
    {
        int espacioAAsignar = newblocksize-blocksize;
        if(espacioAAsignar > bitmap->free_space)
        {
            return -1;
        }
        if(CheckIfEspacioDisponible(bitmap,end,espacioAAsignar))
        {
            AsignarEspacioBitmap(bitmap,end,espacioAAsignar);
            ActualizarMetadata(filename,base,newsize);
        }
        else
        {
            bitmap = Compactar(bitmap,blockfile->path_bloques,newsize,espacioAAsignar,filename,pid);
        }
    }
    else
    {
        ActualizarMetadata(filename,base,newsize);
    }
    GuardarBitmapToFile(bitmap,bitmap_path);
    config_destroy(config_file);
    free(path);
    //AsignarEspacioBitmap(bitmap,newsize);
    return 0;
}
bool CheckIfEspacioDisponible(Bitmap* bitmap,int base, int tamanio)
{
    bool hayEspacio = false;
    if(tamanio <= bitmap->free_space)
    {
        
        for(int i= base; i<(base+tamanio);i++)
        {
            if(!IsSet(bitmap,i))
            {
                hayEspacio = true;
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        return false;
    }
    return hayEspacio;
}
int GetNextFreeSpace(Bitmap* bitmap)
{
    int pos = -1;
    for(int i = 0; i< bitmap->size;i++)
    {
        if(!IsSet(bitmap,i))
        {
            pos = i;
            break;
        }
    }
    return pos;
}

void ActualizarArchivoNombres(char* nombre, bool crear)
{
    t_config* nombresArchivos = config_create(nombres_path);
    char* archivos = config_get_string_value(nombresArchivos, "ARCHIVOS");
    archivos = archivos?archivos:"";
    size_t archivos_len =  strlen(archivos);
    size_t nombre_len = strlen(nombre);
    size_t new_size = archivos_len + nombre_len + 3; 

    char* archivos_copy = malloc(new_size);

    strcpy(archivos_copy, archivos);

    if (crear) {
        if (archivos_len == 0 || strcmp(archivos_copy, "[]") == 0) {
            strcpy(archivos_copy, "[");
            strcat(archivos_copy, nombre);
            strcat(archivos_copy, "]");
        } else {
            insertAtEnd(archivos_copy, nombre);
        }
    } else {
        deleteValue(archivos_copy, nombre);
    }

    config_set_value(nombresArchivos, "ARCHIVOS", archivos_copy);
    config_save_in_file(nombresArchivos, nombres_path);

    free(archivos_copy);
}

char* getPath(char* filename)
{
    size_t root_path_len = strlen(root_path);
    size_t filename_len = strlen(filename);
    size_t total_len = root_path_len + filename_len + 1; // +1 for the null terminator

    // Allocate buffer for path
    char* path = malloc(total_len);
    if (path == NULL) {
        perror("Failed to allocate memory for path");
        return;
    }

    // Copy root_path into path and concatenate filename
    strcpy(path, root_path);
    strcat(path, filename);
    return path;
}

bool CompararBase(Archivo* a,Archivo* b)
{
    return a->base<b->base;
}

Bitmap* Compactar(Bitmap* bitmap,char* path_bloques,int newsize,int espacioAAsignar,char* filename,int pid)
{
    log_info(logger,"PID: %d - Inicio Compactación",pid);
    usleep(retraso_compactacion*1000);
    int tamanioAOcuparBitmap = (bitmap->size-bitmap->free_space)+ espacioAAsignar;
    LiberarEspacioBitmap(bitmap,0,bitmap->size);
    AsignarEspacioBitmap(bitmap,0,tamanioAOcuparBitmap);
    t_list* archivos = list_create();
    t_config* config_nombres = config_create(nombres_path);
    char** nombresArchivos = config_get_array_value(config_nombres,"ARCHIVOS");
    Archivo* archivoExpandir = malloc(sizeof(Archivo));
    //Cargar archivo en lista
    for(int i = 0; nombresArchivos[i];i++)
    {
        Archivo* archivo = malloc(sizeof(Archivo));
        archivo->nombre = nombresArchivos[i];
        char* pathConfig = getPath(archivo->nombre);
        t_config* metadata = config_create(pathConfig);
        archivo->base = config_get_int_value(metadata,"BLOQUE_INICIAL");
        archivo->size = config_get_int_value(metadata,"TAMANIO_ARCHIVO");
        if((archivo->size) ==0)
        {
            archivo->size = blockfile->block_size;
        }
        archivo->contenido = FS_Read(archivo->nombre,0,archivo->size);
        if(!strcmp(archivo->nombre,filename))
        {
            archivo->size = newsize;
            archivoExpandir = archivo;
        }
        else
        {
            list_add(archivos,archivo);
        }
    }
    //Ordenar por posicion
    list_sort(archivos,CompararBase);
    int pos = 0;
    FILE* bloques = fopen(blockfile->path_bloques,"r+");
    //Escribir archivo en el archivo de bloques
    for(int i = 0;i<list_size(archivos);i++)
    {
        Archivo* a = list_get(archivos,i);
        fseek(bloques,pos,SEEK_SET);
        int bloques_escribir = a->size%blockfile->block_size==0?a->size/blockfile->block_size:a->size/blockfile->block_size+1;
        int sizeEscribir = bloques_escribir*blockfile->block_size;
        fwrite(a->contenido,sizeEscribir,1,bloques);
        ActualizarMetadata(a->nombre,pos/blockfile->block_size,a->size);
        pos += sizeEscribir;
        
    }
    fseek(bloques,pos,SEEK_SET);
    fwrite(archivoExpandir->contenido,archivoExpandir->size,1,bloques);
    fclose(bloques);
    ActualizarMetadata(archivoExpandir->nombre,pos/blockfile->block_size,archivoExpandir->size);
    log_info(logger,"PID: %d - Fin Compactación",pid);
    return bitmap;
}

void ActualizarMetadata(char* filename,int base, int tamanio)
{
    char* path = getPath(filename);
    t_config* config_file = config_create(path);
    char value[20];
    sprintf(value,"%d",base);
    char value2[20];
    sprintf(value2,"%d",tamanio);
    config_set_value(config_file,"BLOQUE_INICIAL",value);
    config_set_value(config_file,"TAMANIO_ARCHIVO",value2);
    config_save_in_file(config_file,path);
}

int FS_Write(char* filename, int pos, void* value, int tamanio)
{
    char* pathConfig = getPath(filename);
    t_config* metadata = config_create(pathConfig);
    if(!metadata)
    {
        return -1;
    }
    int base = config_get_int_value(metadata,"BLOQUE_INICIAL");
    int tamanioMax = config_get_int_value(metadata,"TAMANIO_ARCHIVO");
    if(tamanioMax < tamanio+pos)
    {
        return -1;
    }
    else
    {
        FILE* f = fopen(blockfile->path_bloques,"r+");
        fseek(f,base*blockfile->block_size+pos,SEEK_SET);
        fwrite(value,tamanio,1,f);
        fclose(f); 
    }
    return 0;
}

void* FS_Read(char* filename, int pos, int tamanio)
{
    char* pathConfig = getPath(filename);
    t_config* metadata = config_create(pathConfig);
    void* value = malloc(tamanio);
    if(!metadata)
    {
        return NULL;
    }
    int base = config_get_int_value(metadata,"BLOQUE_INICIAL");
    int tamanioMax = config_get_int_value(metadata,"TAMANIO_ARCHIVO");
    if(tamanioMax == 0)
    {
        tamanioMax = blockfile->block_size;
    }
    if(tamanioMax < tamanio+pos)
    {
        return NULL;
    }
    else
    {
        FILE* f = fopen(blockfile->path_bloques,"r+");
        fseek(f,base*blockfile->block_size+pos,SEEK_SET);
        fread(value,tamanio,1,f);
        fclose(f);
    }
    return value;
}
