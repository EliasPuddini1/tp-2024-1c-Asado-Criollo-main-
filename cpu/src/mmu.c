#include "../utils_C/mmu.h"

void iniciar_MMU(){
    setear_algoritmo_tlb();
    iniciar_tlb();
}

int* obtener_direccion_fisica(uint32_t dir, int pid){
    

    int pagina = numero_pagina(dir);
    int desplazamiento = calcular_desplazamiento(dir,pagina);

    if(desplazamiento > cpu_config.tamanio_pagina){
        log_error(logger,"ERROR, el desplazamiento es mayor al limite. ");
    }

    void** direccion;
    void* frame = NULL;
    //int pagina = 3;
    if(cpu_config.entradas_tlb != 0)
    {
        frame = obtener_frame(pagina, pid); //frame en tlb
    }
    

    if(frame){

        log_info(logger,"PID: %d - OBTENER MARCO - Pagina:%d - Marco: %p",pid,pagina, frame);

        return frame+desplazamiento;

    }else{
        direccion = solicitar_direccion_fisica(pagina,pid);
        if( cpu_config.entradas_tlb!=0)
        {
            actualizar_tlb(*direccion,pagina,pid);
        }
        
        //int a = 2;
        //actualizar_tlb(&a,pagina,pid);
        
        return *direccion+desplazamiento;//void*
    }
    
    
    direccion = solicitar_direccion_fisica(pagina,pid);

    log_info(logger,"PID: %d - OBTENER MARCO - Pagina:%d - Marco: ",pid,pagina, *direccion);

    return *direccion + desplazamiento;
}


int numero_pagina(dir_logica dir){
    return floor(dir/cpu_config.tamanio_pagina);
}

int calcular_desplazamiento(dir_logica dir, int pagina){
    return dir - pagina * cpu_config.tamanio_pagina;
}