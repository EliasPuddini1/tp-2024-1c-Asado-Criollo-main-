#ifndef UTILSTLB_H_
#define UTILSTLB_H_

#include "./main.h"

typedef struct{

    int entrada;
    bool is_free;
    int pagina;
    void* nro_marco;
    int pid;
    long tiempoUsado;
}t_fila_tlb;

typedef enum{
    LRU,
    FIFO
}algoritmo;


t_list* TLB;
algoritmo tipo_algoritmo_tlb;


bool buscar_nro_pagina(void * arg, int paginaB, int pid);
t_fila_tlb* buscar_registro_tlb(int pagina, int pid);
void validar_marco(int* frame);
bool buscar_por_marco_(void * arg,int* frame);
void realizar_algoritmo_reemplazo(int* frame, int pagina,int pid);
bool ordenar_por_tiempo(void* arg1, void* arg2);
void loggear_resultados(t_fila_tlb* registro, int* frame, int pagina);
void LimpiarTLB(int pid, int newsize);
#endif // UTILSTLB_H_