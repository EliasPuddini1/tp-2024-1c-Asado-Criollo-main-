#ifndef UTILSMMU_H_
#define UTILSMMU_H_

#include "./main.h"




uint32_t obtener_dato_memoria(dir_logica dir);
uint32_t escribir_dato_memoria(dir_logica dir, uint32_t dato);
uint32_t procesar_solicitud(dir_logica dir, accion accion, uint32_t dato,int pid);
int numero_pagina(dir_logica dir);
int calcular_desplazamiento(dir_logica dir, int pagina);

#endif // UTILSMMU_H_