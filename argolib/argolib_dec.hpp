#include <unistd.h>
#include <functional>
#include <abt.h>
#include <iostream>
#include "pcm.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
using namespace std;

typedef void (*fork_t)(void* args);
ABT_pool *thread_pools;
ABT_xstream *kernel_level_threads;
ABT_sched *master_scheduler ;
ABT_cond *cond;
ABT_mutex *mut;
unsigned seed = time(NULL);
int num_klts=0;
int task_count=0;
int get_count=0;
int steal_count=0;
ABT_mutex mtask;
ABT_mutex msteal;
int slept_till=0;
int energy=0;
int private_deque=0;
ABT_cond *energy_cond;
ABT_mutex *energy_mutex;
int shutdown=0;
pthread_t ptid;
namespace argolib
{
    
    typedef struct {
        uint32_t check_count;
    } scheduler_info;
    typedef struct{
        std::function<void()>lambda;
    } handlingLambda;
    typedef struct{
        ABT_pool private_tasks;
        ABT_thread mail_box;
        uint32_t mailbox_count;
        uint32_t request_id;
        uint32_t id;
        uint32_t request_sent;
        uint32_t task_count;
    } worker;

    worker *workers;

    void argolib_worker_wake(int val){
    
    if(slept_till + val <= num_klts){
    
    for(int i=slept_till;i<slept_till+val;i++){
        ABT_cond_broadcast(energy_cond[i]);
    }
    slept_till+=val;
    }
   
    return;
}
void argolib_worker_sleep(int val){
    
    if(slept_till-val>0){
    slept_till-=val;
    }
    
    return;
}



void configure_DOP(double JPI_prev,double JPI_curr) {
        static int lastAction = 1, wActive=slept_till;
        static int first=0;
        const int wChange=3; 
        if(first=0) {
                argolib_worker_sleep(wChange);
                lastAction=-1;
                first=1;
                return;
        }
        if(JPI_curr<JPI_prev) {
            if(lastAction==-1) {
                argolib_worker_sleep(wChange);
            } 
            else {          

                argolib_worker_wake(wChange);
            }
            } else {
                if(lastAction == -1) {
                            

                    argolib_worker_wake(wChange);
                    lastAction = 1;
                } else {
                            

                    argolib_worker_sleep(wChange);
                    lastAction = -1;
            }
        }
        
}

void* energy_thread(void* args) { 
   
    const int fixed_interval=0.1;
    sleep(0.5); 
    
    double JPI_prev=0;
    logger:: end();
    while(!shutdown) {
   
    double JPI_curr = logger:: end();
   
    configure_DOP(JPI_prev, JPI_curr);
    
   
    JPI_prev = JPI_curr;
    sleep(fixed_interval);
    
}

for(int i=0;i<num_klts;i++){
    ABT_cond_broadcast(energy_cond[i]);
}
\
return NULL;
}




    int sched_initialiser(ABT_sched sched, ABT_sched_config config)
    {
        scheduler_info *p_data = (scheduler_info *)calloc(1, sizeof(scheduler_info));

        ABT_sched_config_read(config, 1, &p_data->check_count);
        void* t=p_data;
        ABT_sched_set_data(sched, t);

        return ABT_SUCCESS;
    }

    void energy_sched_run(ABT_sched sched){
    uint32_t work_count = 0;
    scheduler_info *sched_info_instance;
    int num_pools;
    ABT_pool *ult_pools;
    int steal_from;
    ABT_bool stop;
    void** point=(void**)&sched_info_instance;
    ABT_sched_get_data(sched, point);
    ABT_sched_get_num_pools(sched, &num_pools);
    ult_pools = (ABT_pool *)malloc(num_pools * sizeof(ABT_pool));
    ABT_sched_get_pools(sched, num_pools, 0, ult_pools);
    ABT_thread thread;
    int id;
    ABT_pool_get_id(ult_pools[0],&id);
    id=id-2;
    while (1) {
        if(id>=slept_till){
           
            ABT_cond_wait(energy_cond[id],energy_mutex[id]);
            
        }
        ABT_pool_pop_thread(ult_pools[0], &thread);
        if (thread != ABT_THREAD_NULL) {
            
            ABT_self_schedule(thread, ABT_POOL_NULL);
        } else if (num_pools > 1) {
            if(num_pools==2){
                steal_from=1;
            }
            else{
                steal_from=rand_r(&seed) % (num_pools - 1);
                steal_from++;
            }
            ABT_pool_pop_thread(ult_pools[steal_from], &thread);
            if (thread != ABT_THREAD_NULL) {
                if(get_count==1){
                ABT_mutex_lock(msteal);
                steal_count++;
                ABT_mutex_unlock(msteal);
                }

                ABT_self_schedule(thread, ABT_POOL_NULL);
            }
        }
        work_count++;
        int c=sched_info_instance->check_count;
        if (work_count >= c) {
            work_count = 0;
            ABT_sched_has_to_stop(sched, &stop);
            if (stop == ABT_TRUE ){

                break;
            }
            else{
            ABT_xstream_check_events(sched);
            }
        }
    }
}
    void private_sched_run(ABT_sched sched)
    {
        uint32_t work_count = 0;
        scheduler_info *sched_info_instance;
        int num_pools;
        ABT_pool *ult_pools;
        int steal_from;
        ABT_bool stop;

        void** point=(void **)&sched_info_instance;
        ABT_sched_get_data(sched, point);
        ABT_sched_get_num_pools(sched, &num_pools);
        
        ult_pools = (ABT_pool *)malloc(num_pools * sizeof(ABT_pool));
        ABT_sched_get_pools(sched, num_pools, 0, ult_pools);
        int f_id=-1;
        for(int i=0;i<num_pools;i++){
            ABT_pool temp=ult_pools[i];
            int id;
            ABT_pool_get_id(ult_pools[i],&id);
            if(f_id==-1){
                f_id=id;
                f_id=f_id-2;
            }
            id=id-2;
            workers[id].private_tasks=temp;
        }
        

        while (1) {
            
            int check=0;
            if(workers[f_id].request_id!=-1){
                size_t t;
                ABT_pool_get_size(workers[f_id].private_tasks,&t);
                    if((int)t>1){
                    ABT_thread temp;
                    ABT_pool_pop_thread(workers[f_id].private_tasks, &temp);
                    if(temp!= ABT_THREAD_NULL){
                    if(get_count==1){
                        ABT_mutex_lock(msteal);
                        steal_count++;
                        ABT_mutex_unlock(msteal);
                    }
                    workers[workers[f_id].request_id].mail_box=temp;
                    }
                    }
                    ABT_cond_broadcast(cond[f_id]);
                    
                    workers[f_id].request_id=-1;
                    
            }
            ABT_thread thread;
            ABT_pool_pop_thread(workers[f_id].private_tasks, &thread);
            if (thread != ABT_THREAD_NULL) {
                
                task_count++;
                ABT_self_schedule(thread, ABT_POOL_NULL);
                
                check=1;
            } 
            if(check==0 && workers[f_id].mail_box!=ABT_THREAD_NULL){
                
                ABT_self_schedule(workers[f_id].mail_box,ABT_POOL_NULL);
                workers[f_id].mail_box=ABT_THREAD_NULL;
                check=1;
            }
            
            
            if (check == 0 && num_pools > 1 ) {
                
                steal_from=(rand_r(&seed) % (num_pools));
                while(steal_from==f_id){
                    
                    steal_from=(rand_r(&seed) % (num_pools));
                }
                
                
                size_t tasks;
                ABT_pool_get_size(workers[f_id].private_tasks,&tasks);
                if((int)(tasks)<=1){
                    workers[f_id].request_id=-1;
                    ABT_cond_broadcast(cond[f_id]);
                }
                
                size_t size;
                ABT_pool_get_size(workers[steal_from].private_tasks,&size);
                int s=(int)size;
                
                if(s>1){
                ABT_mutex_lock(mut[steal_from]);
                
                workers[f_id].request_sent=1;
                workers[steal_from].request_id=f_id;

                ABT_cond_wait(cond[steal_from],mut[steal_from]);
                ABT_mutex_unlock(mut[steal_from]);
                }
                check=1;
            }
            int done=sched_info_instance->check_count;
            if (++work_count >= done) {
                
                work_count = 0;
                
                ABT_sched_has_to_stop(sched, &stop);
                if (stop == ABT_TRUE){
                    
                    break;
                }
                else{
                    ABT_xstream_check_events(sched);
                }
                
            }
            
            
        }
        
        
    
    }

    void sched_algo(ABT_sched sched)
    {   
        uint32_t work_count = 0;
        scheduler_info *sched_info_instance;
        int num_pools;
        ABT_pool *ult_pools;
        int steal_from;
        ABT_bool stop;
        void** point=(void**)&sched_info_instance;
        ABT_sched_get_data(sched, point);
        ABT_sched_get_num_pools(sched, &num_pools);
        ult_pools = (ABT_pool *)malloc(num_pools * sizeof(ABT_pool));
        ABT_sched_get_pools(sched, num_pools, 0, ult_pools);
        ABT_thread thread;
        while (1) {
            
            ABT_pool_pop_thread(ult_pools[0], &thread);
            if (thread != ABT_THREAD_NULL) {
                
                ABT_self_schedule(thread, ABT_POOL_NULL);
            } else if (num_pools > 1) {
                if(num_pools==2){
                    steal_from=1;
                }
                else{
                    steal_from=rand_r(&seed) % (num_pools - 1);
                    steal_from++;
                }
                ABT_pool_pop_thread(ult_pools[steal_from], &thread);
                if (thread != ABT_THREAD_NULL) {
                    if(get_count==1){
                        ABT_mutex_lock(msteal);
                        steal_count++;
                        ABT_mutex_unlock(msteal);
                    }
                    ABT_self_schedule(thread, ABT_POOL_NULL);
                }
            }
            work_count++;
            int c=sched_info_instance->check_count;
            if (work_count >= c) {
                work_count = 0;
                ABT_sched_has_to_stop(sched, &stop);
                if (stop == ABT_TRUE){
                    break;
                }
                else{
                ABT_xstream_check_events(sched);
                }
            }
        }
    }

    static int sched_free(ABT_sched sched)
    {
        scheduler_info *sched_info_instance;
        void** point=(void **)&sched_info_instance;
        ABT_sched_get_data(sched, point);
        free(sched_info_instance);

        return ABT_SUCCESS;
    }
    void assign_pools_to_scheduler(ABT_pool* pools,ABT_sched_config sched_configuration, int n, ABT_sched_def scheduler_define){
        for (int i = 0; i < n; i++) {
            for (int k = 0; k < n; k++) {
                int rt=(i + k);
                rt=rt%n;
                pools[k] = thread_pools[rt];
            }

            ABT_sched_create(&scheduler_define, n, pools, sched_configuration, &master_scheduler[i]);
        }
    }
    static void create_scheds(int n)
    {
        ABT_sched_config sched_configuration;
    ABT_pool *ult_pools;
    
    

    ABT_sched_config_var sched_check_count = { .idx = 0,
                                           .type = ABT_SCHED_CONFIG_INT };

    if(private_deque==1){
    ABT_sched_def scheduler_define = { .type = ABT_SCHED_TYPE_ULT,
                                .init = sched_initialiser,
                                .run = private_sched_run,
                                .free = sched_free,
                                .get_migr_pool = NULL };

    ABT_sched_config_create(&sched_configuration, sched_check_count, 10,
                            ABT_sched_config_var_end);

    ult_pools = (ABT_pool *)malloc(n * sizeof(ABT_pool));
    assign_pools_to_scheduler(ult_pools,sched_configuration,n,scheduler_define);
    free(ult_pools);

    ABT_sched_config_free(&sched_configuration);
    }
    else if(energy==1){
    ABT_sched_def scheduler_define = { .type = ABT_SCHED_TYPE_ULT,
                                .init = sched_initialiser,
                                .run = energy_sched_run,
                                .free = sched_free,
                                .get_migr_pool = NULL };

    ABT_sched_config_create(&sched_configuration, sched_check_count, 10,
                            ABT_sched_config_var_end);

    ult_pools = (ABT_pool *)malloc(n * sizeof(ABT_pool));
    assign_pools_to_scheduler(ult_pools,sched_configuration,n,scheduler_define);
    free(ult_pools);

    ABT_sched_config_free(&sched_configuration);
    }
    else{
        ABT_sched_def scheduler_define = { .type = ABT_SCHED_TYPE_ULT,
                                .init = sched_initialiser,
                                .run = sched_algo,
                                .free = sched_free,
                                .get_migr_pool = NULL };

        ABT_sched_config_create(&sched_configuration, sched_check_count, 10,
                                ABT_sched_config_var_end);

        ult_pools = (ABT_pool *)malloc(n * sizeof(ABT_pool));
        assign_pools_to_scheduler(ult_pools,sched_configuration,n,scheduler_define);
        free(ult_pools);

        ABT_sched_config_free(&sched_configuration);
    }
    }
    



    void init(int argc, char **argv){
        if(argc>3){
            get_count=1;
        }
        ABT_mutex_create(&mtask);
        ABT_mutex_create(&msteal);
        num_klts = atoi(argv[1]);
        cond=(ABT_cond*)malloc(sizeof(ABT_cond)*num_klts);
        mut=(ABT_mutex*)malloc(sizeof(ABT_mutex)*num_klts);
        kernel_level_threads = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_klts);
        thread_pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_klts);
        master_scheduler = (ABT_sched *)malloc(sizeof(ABT_sched) * num_klts);
        workers=(worker*)malloc(sizeof(worker)*num_klts);
        ABT_init(argc, argv);
        
        for(int i=0;i<num_klts;i++){
            workers[i].request_id=-1;
            workers[i].mailbox_count=0;
            workers[i].mail_box=ABT_THREAD_NULL;
            workers[i].request_sent=0;
            workers[i].task_count=0;
        }
        for (int i = 0; i < num_klts; i++) {
                ABT_pool_create_basic(ABT_POOL_RANDWS, ABT_POOL_ACCESS_MPMC,
                                    ABT_TRUE, &thread_pools[i]);
                ABT_mutex_create(&mut[i]);
                ABT_cond_create(&cond[i]);
        }
        for (int i = 0; i < num_klts; i++) {
            
        if(atoi(argv[2])==1){
            private_deque=1;
        }
        else if(atoi(argv[2])==0){

        }
        else{
            energy=1;
            slept_till=num_klts;
        }
        create_scheds(num_klts);
    }
    if(energy==1){
        energy_cond=(ABT_cond*)malloc(sizeof(ABT_cond)*num_klts);
        energy_mutex=(ABT_mutex*)malloc(sizeof(ABT_mutex)*num_klts);
       
        logger:: start();
         pthread_create(&ptid, NULL, energy_thread, NULL);
        for(int i=0;i<num_klts;i++){
            ABT_mutex_create(&energy_mutex[i]);
            ABT_cond_create(&energy_cond[i]);
        }
    }
    else{
        logger:: start();
    }
        

        
        ABT_xstream_self(&kernel_level_threads[0]);
        ABT_xstream_set_main_sched(kernel_level_threads[0], master_scheduler[0]);

        
        for (int i = 1; i < num_klts; i++) {
            ABT_xstream_create(master_scheduler[i], &kernel_level_threads[i]);
        }

    }
    void finalize(){
       
        for (int i = 1; i < num_klts; i++) {
            ABT_xstream_join(kernel_level_threads[i]);
            ABT_xstream_free(&kernel_level_threads[i]);
        }
         
        ABT_finalize();
        
        free(kernel_level_threads);
        free(thread_pools);
        free(master_scheduler);
        
        shutdown=1;
        if(energy==1){
        }
        else{
            logger:: end();
        }
       
    }
    void resolveLambda(void*args){
        ((handlingLambda*)args)->lambda();
    }
    template<class T>
    void kernel(T && lambda){
        double t1 = ABT_get_wtime();
        (lambda());
        double t2 = ABT_get_wtime();
        shutdown=1;
        if(energy==1){
        
        logger::end();
        pthread_join(ptid, NULL);
        }
        printf("Time: %f\n",t2-t1);
        if(get_count==1){
        printf("Task count: %d\n",task_count);
        printf("Steal count: %d\n",steal_count);
        }
    }
        typedef struct{
            ABT_thread thread;
            handlingLambda args;
        
        } Task_handle;
    template<typename T>
    Task_handle fork(T && lambda ){

        int rank;
        ABT_xstream_self_rank(&rank);
        Task_handle thread_handle;
        ABT_pool target_pool = thread_pools[rank];

        thread_handle.args.lambda=lambda;
        if(get_count==1){
            ABT_mutex_lock(mtask);
            task_count++;
            ABT_mutex_unlock(mtask);
        }
        ABT_thread_create(target_pool,(void(*)(void *))&resolveLambda,(void*)&thread_handle.args,
                            ABT_THREAD_ATTR_NULL,&thread_handle.thread);
        return thread_handle;
    }
    template<typename... Task_handle>
    void join(Task_handle ... handles){
            ((ABT_thread_free(&handles.thread)), ...);
        return;
}
}
