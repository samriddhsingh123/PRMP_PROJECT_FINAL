#include "argolib.h"
#define size 100000
typedef struct {
    double* ans;
    double* arr;
    int high;
    int low;
} hl;
int threshold=1;
void print(double arr[]){
    for(int i=0;i<size+2;i++){
        printf("%lf ",arr[i]);
    }
    printf("\n");
}
void recurse(void* args){
    hl* curr=args;
    int high=curr->high;
    int low=curr->low;
    double *ans=curr->ans;
    double *arr=curr->arr;
    
    if((high-low)>threshold){
        int mid=(high+low)/2;
        hl args1={ans,arr,mid,low};
        hl args2={ans,arr,high,mid};
        Task_handle* task1 = argolib_fork(&recurse,&args1);
        Task_handle** list = (Task_handle**)malloc(sizeof(Task_handle*));
        list[0]=task1;
        recurse(&args2);
        argolib_join(list,1);
    }
    else{
        
        for(int i=low;i<high;i++){
            ans[i]=(arr[i-1]+arr[i+1])/2.0;
        }
    }
}
int swap(void* args){
    int check=1;
    hl* curr=args;
    double *ans=curr->ans;
    double *arr=curr->arr;
    for(int i=0;i<size+1;i++){
        if(arr[i]!=ans[i]){
            check=0;
        }
        double a=arr[i];
        arr[i]=ans[i];
        ans[i]=a;
    }
    return check;
}
void compute(int itrs,int argc, char **argv){
    double* arr=(double*)malloc(sizeof(double)*(size+2));
    double* ans=(double*)malloc(sizeof(double)*(size+2));
    for(int i=0;i<size+1;i++){
        arr[i]=0;
        ans[i]=0;
    }
    ans[size+1]=1;
    arr[size+1]=1;
    hl c={ans,arr,size+1,1};
   
    argolib_init(argc, argv);
    
    double t1 = ABT_get_wtime();
    for(int i=0;i<itrs;i++){
        argolib_start_tracing();
        argolib_kernel(&recurse,&c);
        argolib_stop_tracing();
        // print(arr);
        int check=swap(&c);
        if(check){
            
            printf("Equillibrium Reached at iteration: %d\n",i+1);
            break;
        }
        
    }
    argolib_finalize();
    double t2 = ABT_get_wtime();
    printf("Total time Taken: %lf\n",t2-t1);
}
int main(int argc, char **argv){
    int itrs=1;
   
    compute(itrs,argc,argv);
    return 0;
}