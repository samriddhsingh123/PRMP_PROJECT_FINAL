#include "argolib.h"

typedef struct {
    int n;
    int ret;
} fib_struct;

void fib(void* args){
    fib_struct* curr=args;
    int n = curr->n;
    int *p_ret = &(curr)->ret;
    if(n<2){
        *p_ret=n;
        return;
    }
    fib_struct args1 = { n - 1, 0 };
    fib_struct args2 = { n - 2, 0 };
    Task_handle* task1 = argolib_fork(&fib,&args1);
    Task_handle* task2 = argolib_fork(&fib,&args2);
    Task_handle** list = (Task_handle**)malloc(sizeof(Task_handle*)*2);
    list[0]=task1;
    list[1]=task2;
    argolib_join(list,2);
    *p_ret=args1.ret + args2.ret;
    return;
}
int main(int argc, char **argv){
    
    argolib_init(argc, argv);
    int result=0;
    int n;
    printf("Enter the number: ");
    scanf("%d",&n);
    fib_struct args = { n , 0 };
    argolib_kernel(fib,&args);
    result=args.ret;
    printf("Result %d\n",result);
    argolib_finalize();
    return 0;
   
}