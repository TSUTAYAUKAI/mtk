#include<stdio.h>
#include "mtk_c.h"




/* addq: キューの最後尾にタスクを追加*/
void addq(TASK_ID_TYPE *head, TASK_ID_TYPE id) {
    TASK_ID_TYPE curr;

    /* キューが空の場合 */
    if (*head == NULLTASKID) {
        *head = id;
    } 
    else {
        /* 末尾を探す */
        curr = *head;
        while (task_tab[curr].next != NULLTASKID) {
            curr = task_tab[curr].next;
        }
        /* 末尾に繋ぐ */
        task_tab[curr].next = id;
    }
    
    /* 新しい末尾のnextは必ずNULL */
    task_tab[id].next = NULLTASKID;
}

/* removeq: キューの先頭からタスクを取り出す*/
TASK_ID_TYPE removeq(TASK_ID_TYPE *head) {
    TASK_ID_TYPE id;

    if (*head == NULLTASKID) {
        return NULLTASKID;
    }

    /* 先頭を取り出す */
    id = *head;
    /* 先頭位置を次にずらす */
    *head = task_tab[id].next;
    
    /* 取り出したタスクのnextはクリアしておく */
    task_tab[id].next = NULLTASKID;

    return id;
}

/* sched: 次に実行するタスクを決定する */
void sched(void) {
    /* readyキューから1つ取り出す */
    next_task = removeq(&ready);

    /* 実行できるタスクがなければ無限ループ*/
    if (next_task == NULLTASKID) {
        while(1);
    }
}

void sleep(int ch){
	addq(&semaphore[ch].nst, curr_task);
	sched();				        
	swtch();		        
}

void wakeup(int ch){
	TASK_ID_TYPE  wake_up_id = removeq(&semaphore[ch].nst);
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
