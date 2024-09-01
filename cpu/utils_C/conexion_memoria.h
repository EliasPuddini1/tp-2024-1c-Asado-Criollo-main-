#ifndef UTILSCONEXIONMEMORIA_H_
#define UTILSCONEXIONMEMORIA_H_

#include "./main.h"

typedef enum
{
    ENVIAR_DATO,
    SOLICITAR_DATO,
    SOLICITAR_INSTRUCCION
}solicitud_memoria;

void *gestionar_solicitud(void *request, solicitud_memoria code);
#endif // UTILSCONEXIONMEMORIA_H_