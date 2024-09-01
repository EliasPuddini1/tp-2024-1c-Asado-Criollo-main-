#include "../utils_C/tlb.h"

int nextRegistroLibre;

int iniciar_tlb(){

    TLB = list_create();

    for(int i=0; i < cpu_config.entradas_tlb; i++){
        
        t_fila_tlb *registro = malloc(sizeof(t_fila_tlb));
        registro->is_free = true;
        registro->pagina = UNDEFINED;
        registro->nro_marco = (int*)malloc(sizeof(int));
        if (!registro->nro_marco) {
            perror("Error al asignar memoria para nro_marco");
            free(registro);
            return ERROR;
        }
        registro->nro_marco = NULL;
        registro->pid = UNDEFINED;
        registro->entrada = i;
        nextRegistroLibre = 0;
        list_add(TLB,registro);
    }
    return 0;
}

void setear_algoritmo_tlb(){
    if(!strcmp("FIFO",cpu_config.algoritmo_tlb)){
        tipo_algoritmo_tlb = FIFO;
    }
    else if(!strcmp("LRU",cpu_config.algoritmo_tlb)){
        tipo_algoritmo_tlb = LRU;
    }
    else{
        log_error(logger,"No se pudo setear el algorimo de TLB. Error en el archivo config.");
    }
}

void actualiar_tiempo_usado(t_fila_tlb * registro){
    if(tipo_algoritmo_tlb == LRU){
        long tiempo;
        time_t tiempo_actual;
        tiempo = (long) time(&tiempo_actual);
        registro->tiempoUsado = tiempo;
    }
}

bool buscar_nro_pagina(void * arg, int paginaB,int pid){
    t_fila_tlb * registro = (t_fila_tlb *) arg;
    return registro->pagina == paginaB && registro->pid == pid;
}



t_fila_tlb* buscar_registro_tlb(int pagina,int pid){
    bool buscar_pagina(void *arg){
        return buscar_nro_pagina(arg,pagina,pid);
    }
    t_fila_tlb *registro = NULL;
    registro = (t_fila_tlb *) list_find(TLB, buscar_pagina);

    return registro;
}



int * obtener_frame(int pagina, int pid){

    t_fila_tlb* registro = (t_fila_tlb*)buscar_registro_tlb(pagina,pid);



    if(!registro){
        log_info(logger,"PID: %d - TLB MISS - Pagina:%d",pid,pagina);
        return NULL;
    }
    
    actualiar_tiempo_usado(registro);


    log_info(logger,"PID: %d - TLB HIT - Pagina:%d",pid,pagina);
    return registro->nro_marco;;
}

void actualizar_tlb(void* frame, int pagina,int pid){
 
    if(nextRegistroLibre < cpu_config.entradas_tlb)
    {
        t_fila_tlb * registro = (t_fila_tlb *)list_get(TLB,nextRegistroLibre);
        loggear_resultados(registro, frame, pagina);
        registro->is_free = false;
        registro->nro_marco = frame;
        registro->pid= pid;
        registro->pagina = pagina;
        long tiempo;
        time_t tiempo_actual;
        tiempo = (long) time(&tiempo_actual);
        registro->tiempoUsado = tiempo;
        nextRegistroLibre++;
        if(tipo_algoritmo_tlb == FIFO)
        {
            nextRegistroLibre = nextRegistroLibre%cpu_config.entradas_tlb;
        }
    }
    else
    {

        realizar_algoritmo_reemplazo(frame,pagina,pid);
    }
}


void realizar_algoritmo_reemplazo(int* frame, int pagina,int pid){
    t_list* registros;
    registros = list_sorted(TLB,ordenar_por_tiempo);
    t_fila_tlb* registro = list_get(registros,0);
    loggear_resultados(registro,frame,pagina);
    registro->nro_marco = frame;
    registro->pagina = pagina;
    registro->pid = pid;
    long tiempo;
    time_t tiempo_actual;
    tiempo = (long) time(&tiempo_actual);
    registro->tiempoUsado = tiempo;
    list_destroy(registros);
}

bool ordenar_por_tiempo(void* arg1, void* arg2){
    t_fila_tlb* registro1 = (t_fila_tlb *)arg1;
    t_fila_tlb* registro2 = (t_fila_tlb *)arg2;

    return registro1->tiempoUsado < registro2->tiempoUsado;
    
}


void loggear_resultados(t_fila_tlb* registro, int* frame, int pagina){
    
    if (registro->nro_marco == NULL) {

        log_info(logger,"Entrada victima: %d Numero de Pagina: %d Marco: NULL", registro->entrada,registro->pagina);
    }else{
        log_info(logger,"Entrada victima: %d Numero de Pagina: %d Marco: %p", registro->entrada,registro->pagina, registro->nro_marco);
    }
    
    log_info(logger,"Nueva entrada -> Numero de Pagina: %d Marco: %p", pagina, frame);
}

void LimpiarTLB(int pid, int newsize)
{
    int newpagesize = newsize%cpu_config.tamanio_pagina==0?newsize/cpu_config.tamanio_pagina:newsize/cpu_config.tamanio_pagina+1;

    for(int i = 0; i<list_size(TLB);i++)
    {
        t_fila_tlb * registro = list_get(TLB,i);
        if(registro->pid == pid && registro->pagina>=newpagesize)
        {
            registro->is_free = true;
            registro->pagina = UNDEFINED;
            registro->nro_marco = NULL;
            registro->pid = UNDEFINED;
            registro->tiempoUsado = -1;
        }
    }
}

void eliminar_tlb(){
    for(int i = 0; i < list_size(TLB); i++) {
        t_fila_tlb *registro = (t_fila_tlb *)list_get(TLB, i);
        if (registro) {
            if (registro->nro_marco) {
                free(registro->nro_marco);
            }
            free(registro);
        }
    }
    list_destroy(TLB);
}