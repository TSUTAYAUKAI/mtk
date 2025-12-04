
#define NUMSEMAPHORE	4

typedef int TASK_ID_TYPE; 

typedef struct{
  int count;
  int nst;
  TASK_ID_TYPE task_list;
} SEMAPHORE_TYPE;



SEMAPHORE_TYPE 	semaphore[NUMSEMAPHORE]; 
