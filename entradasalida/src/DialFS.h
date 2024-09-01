#ifndef DIALFS_H
#define DIALFS_H

#include <stddef.h>
#include <commons/config.h>
#include <commons/log.h>
typedef struct {
    size_t size;
    int free_space;
    unsigned char *bits;

} Bitmap;

typedef struct 
{
    int block_count;
    int block_size;
    char* path_bloques;
}BlockFile;

typedef struct 
{
    char* nombre;
    int base;
    int size;
    void* contenido;
}Archivo;


extern BlockFile* blockfile;
extern t_log* logger;
extern char* root_path;
extern char* nombres_path;
extern int retraso_compactacion;

Bitmap* CreateBitmap(size_t size);
void SetBit(Bitmap *bitmap, size_t index);
void LimpiarBit(Bitmap *bitmap, size_t index);
int IsSet(const Bitmap *bitmap, size_t index);
void DestroyBitmap(Bitmap *bitmap);
int GuardarBitmapToFile(const Bitmap *bitmap, const char *filename);
Bitmap* CargarBitmapDesdeFile(const char *filename);
void CrearBlockFile(char* filename, int size);
int CrearFile(char* filename);
int BorrarFile(char* filename);
Bitmap* Compactar(Bitmap* bitmap,char* path_bloques,int newsize, int espacioAAsignar,char* filename,int pid);
void AsignarEspacioBitmap(Bitmap* bitmap,int posicion,int size);
void LiberarEspacioBitmap(Bitmap* bitmap,int posicion,int size);
void CrearFileMetadata(char* filename,int base);
int GetNextFreeSpace(Bitmap* bitmap);
bool CheckIfEspacioDisponible(Bitmap* bitmap,int base, int tamanio);
int TruncateFile(char* filename,int newsize,int pid);
void ActualizarArchivoNombres(char* nombre,bool crear);
char* getPath(char* filename);
int FS_Write(char* filename, int pos, void* value, int tamanio);
void* FS_Read(char* filename, int pos, int tamanio);
#endif // DIALFS_H