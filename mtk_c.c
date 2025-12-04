#include<stdio.h>
#include "mtk_c.h"

void sleep(int ch){
	addq(&semaphore[ch].task_list, curr_task);
	sched();				        
	swtch();		        
}

void wakeup(int ch){
	TASK_ID_TYPE wake_up_id = removeq(&semaphore[ch].task_list);
	addq(&ready, wake_up_id);			        
}

void p_body(TASK_ID_TYPE id){
	semaphore[id].count--; 		        
	if(semaphore[id].count < 0){
		sleep(id);      
	}
}

void v_body(TASK_ID_TYPE id){
	semaphore[id].count++; 		        
	if(semaphore[id].count <= 0){
		wakeup(id); 
	}
}
