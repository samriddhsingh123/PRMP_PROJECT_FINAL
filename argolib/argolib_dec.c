#include "argolib.h"
#define memory UINT32_MAX

ABT_pool *thread_pools;
ABT_xstream *kernel_level_threads;
ABT_sched *master_scheduler ;
ABT_cond *cond;
ABT_mutex *mut;
int* id_to_executor;
ABT_mutex mtask;
ABT_mutex msteal;
int task_count=0;
int get_count=0;
int steal_count=0;
int var;
unsigned seed;
int slept_till=0;
int energy=0;
int private=0;
ABT_cond *energy_cond;
ABT_mutex *energy_mutex;
int shutdown=0;

typedef struct {
    Task_handle* thread;
    uint32_t id;
    uint32_t thread_id;
    int wc;
    int we;
    int sc;
}task;

struct ranklist{
    int data;
    struct ranklist* next;
    struct ranklist* prev;
} ;

struct ranklist* rank_head=NULL;
struct ranklist* rank_tail=NULL;
struct ranklist* rank_temp=NULL;

struct tasklist{
    task curr;
    struct tasklist* next;
    struct tasklist* prev;
};

typedef struct {
    struct tasklist* tasks;
    struct tasklist* tasks_tail;
    struct tasklist* executed;
    struct tasklist* executed_tail;
    struct tasklist* steal_list;
    uint32_t ac;
    int sc;
} trace_worker;

typedef struct{
    ABT_pool private_tasks;
    Task_handle mail_box;
    uint32_t mailbox_count;
    uint32_t request_id;
    uint32_t id;
    uint32_t request_sent;
    uint32_t task_count;
} worker;

worker *workers;
ABT_mutex ranklock;
ABT_mutex* tracelock;
ABT_mutex* steallock;
trace_worker* trace_workers;
static int tracing_enabled=0;
static int replay_enabled=0;
int num_klts=0;

void print_task(task t){
    printf("id: %" PRIu32 ",thread_id: %" PRIu32 ", creator: %d, executor: %d |||| ",t.id,t.thread_id,t.wc,t.we);

}
void print(struct tasklist* t){
    if(t==NULL){
        return;
    }
    struct tasklist* temp=t;
    while(temp!=NULL){
        print_task(temp->curr);
        temp=temp->next;
    }
    printf("\n");
    return;

}

void pushsteallist(task t, int id){
    
    struct tasklist* node=(struct tasklist*)malloc(sizeof(struct tasklist));
    node->prev=NULL;
    node->curr=t;
    node->next=NULL;
    
    if(trace_workers[id].steal_list==NULL){
        trace_workers[id].steal_list=node->prev;
        return;
    }
    
    node->next=trace_workers[id].steal_list;
    trace_workers[id].steal_list=node;
    
    return;
}

task* popsteallist(int id){
    if(trace_workers[id].steal_list==NULL){
       
        return NULL;
    }
    task* ret=&trace_workers[id].steal_list->curr;
    
    trace_workers[id].steal_list=trace_workers[id].steal_list->next;
    
    return ret;
}


void addtoranklist(int data){
    ABT_mutex_lock(ranklock);
    struct ranklist* temp=(struct ranklist*)malloc(sizeof(struct ranklist));
    temp->next=NULL;
    temp->data=data;
    temp->prev=NULL;
    if(rank_head==NULL){
        rank_head=temp;
        rank_tail=temp;
        ABT_mutex_unlock(ranklock);
        return;
    }
    rank_tail->next=temp;
    temp->prev=rank_tail;
    rank_tail=temp;
    ABT_mutex_unlock(ranklock);
    return;
}

int get_next_rank(){
    int ret=-1;
    if(rank_temp!=NULL){
    ABT_mutex_lock(ranklock);
    ret=rank_temp->data;
    rank_temp=rank_temp->next;
    ABT_mutex_unlock(ranklock);
    }
    return ret;
}

void task_head_push(int id,task t){
   
   
    struct tasklist* node=(struct tasklist*)malloc(sizeof(struct tasklist));
    node->prev=NULL;
    node->curr=t;
    node->next=NULL;
    if(trace_workers[id].tasks==NULL){
        trace_workers[id].tasks=node;
        trace_workers[id].tasks_tail=node;
       
      
        return;
    }
    
    node->next=trace_workers[id].tasks;
    trace_workers[id].tasks->prev=node;
    trace_workers[id].tasks=node;
    
   
    return;
}

task* task_head_pop(int id){
    
    if(trace_workers[id].tasks==NULL){
        
        return NULL;
    }
    task* ret=&trace_workers[id].tasks->curr;
    
    trace_workers[id].tasks=trace_workers[id].tasks->next;
    if(trace_workers[id].tasks!=NULL){
    trace_workers[id].tasks->prev=NULL;
    }
    if(trace_workers[id].tasks==NULL){
        trace_workers[id].tasks_tail=NULL;
    }
   
    return ret;
}

task* task_tail_pop(int id){
    
    ABT_mutex_lock(tracelock[id]);
    if(trace_workers[id].tasks_tail==NULL){
        ABT_mutex_unlock(tracelock[id]);
        return NULL;
    }
    task* ret=&trace_workers[id].tasks_tail->curr;
    
    trace_workers[id].tasks_tail=trace_workers[id].tasks_tail->prev;
    if(trace_workers[id].tasks_tail!=NULL){
    trace_workers[id].tasks_tail->next=NULL;
    }
   
    if(trace_workers[id].tasks_tail==NULL){
        trace_workers[id].tasks=NULL;
    }
    ABT_mutex_unlock(tracelock[id]);
   
    return ret;
}

void task_tail_push(task t,int id){
    
    struct tasklist* node=(struct tasklist*)malloc(sizeof(struct tasklist));
    node->prev=NULL;
    node->curr=t;
    node->next=NULL;
    if(trace_workers[id].tasks_tail==NULL){
        trace_workers[id].tasks_tail=node;
        trace_workers[id].tasks=node;
       
        
        return;
    }
    
    trace_workers[id].tasks_tail->next=node;
    node->prev=trace_workers[id].tasks_tail;
    trace_workers[id].tasks_tail=node;
    
    
    
    return;
 }

 void executed_head_push(task t,int id){
   
    struct tasklist* node=(struct tasklist*)malloc(sizeof(struct tasklist));
    t.we=id;
    node->prev=NULL;
    node->curr=t;
    node->next=NULL;
    if(trace_workers[id].executed==NULL){
        trace_workers[id].executed=node;
        trace_workers[id].executed_tail=node;
        
       
        return;
    }
    
    node->next=trace_workers[id].executed;
    trace_workers[id].executed->prev=node;
    trace_workers[id].executed=node;
    
   
    
    return;
 }

 task* executed_head_pop(int id){
    
    if(trace_workers[id].executed==NULL){
       
        return NULL;
    }
    task* ret=&trace_workers[id].executed->curr;
    
    trace_workers[id].executed=trace_workers[id].executed->next;
    if(trace_workers[id].executed!=NULL){
    trace_workers[id].executed->prev=NULL;
    }
    if(trace_workers[id].executed==NULL){
        trace_workers[id].executed_tail=NULL;
    }
    
    return ret;
}



 void list_aggregation(){
    
    for(int i=0;i<num_klts;i++){
        
        trace_workers[i].executed=trace_workers[i].tasks;
        trace_workers[i].tasks=NULL;
        trace_workers[i].executed_tail=trace_workers[i].tasks_tail;
        trace_workers[i].tasks_tail=NULL;
       
        while(trace_workers[i].executed!=NULL){
            
            task* temp=executed_head_pop(i);
             
            if(temp!=NULL){
            int exec=id_to_executor[temp->thread_id]-1;
             if(i!=exec && exec!=-1){
                temp->we=exec;
                task_tail_push(*temp,i);
            }
            }
            
        }
    }
    return;
 }

struct tasklist* split(struct tasklist* head) 
{ 
    struct tasklist* fast = head;
    struct tasklist* slow = head; 
    while (fast->next!=NULL && fast->next->next!=NULL) 
    { 
        fast = fast->next->next; 
        slow = slow->next; 
    } 
    struct tasklist* temp = slow->next; 
    slow->next = NULL; 
    return temp; 
} 

 struct tasklist* merge(struct tasklist* a, struct tasklist* b) 
{ 
    if (a==NULL){ 
        return b; 
    }
  
    if (b==NULL){
        return a; 
    }

    if (a->curr.id >= b->curr.id) 
    { 
        b->next = merge(a,b->next); 
        b->next->prev = b; 
        b->prev = NULL; 
        return b;  
    } 
    else
    {   a->next = merge(a->next,b); 
        a->next->prev = a; 
        a->prev = NULL; 
        return a; 
    } 
} 

 struct tasklist* tasklist_merge_sort(struct tasklist* head){
   
    if (head==NULL || head->next==NULL){ 
        return head; 
    }
    struct tasklist *b = split(head); 
  
    
    head = tasklist_merge_sort(head); 
    b = tasklist_merge_sort(b); 
  
    
    return merge(head,b); 

 }

 void set_tasks_tail(int id){
    struct tasklist* temp=trace_workers[id].tasks;
    while(temp!=NULL && temp->next!=NULL){
        temp=temp->next;
    }
    trace_workers[id].tasks_tail=temp;
    return;
 }

 void list_sorting(){
        for(int i=0;i<num_klts;i++){
            trace_workers[i].tasks=tasklist_merge_sort(trace_workers[i].tasks);
            set_tasks_tail(i);
        }
    return;
 }

 void argolib_start_tracing(){
     var=0;
    if(replay_enabled==0){
    tracelock = (ABT_mutex*)malloc(sizeof(ABT_mutex)*num_klts);
    steallock = (ABT_mutex*)malloc(sizeof(ABT_mutex)*num_klts);
    
   
    tracing_enabled = 1;
    ABT_mutex_create(&ranklock);
    for(int i=0;i<num_klts;i++){
        ABT_mutex_create(&tracelock[i]);
        ABT_mutex_create(&steallock[i]);
        trace_workers[i].executed=NULL;
        trace_workers[i].tasks=NULL;
        trace_workers[i].ac=i*(UINT32_MAX/num_klts);
        trace_workers[i].sc=0;
       
    }
    }
    else{
        for(int i=0;i<num_klts;i++){
        trace_workers[i].ac=i*(UINT32_MAX/num_klts);
    }
    
        rank_temp=rank_head;
    }
    
    return;
}

void argolib_stop_tracing(){
    
    if(replay_enabled == 0) {
    list_aggregation(); 
    list_sorting(); 

    replay_enabled = 1;
    tracing_enabled = 0;
    
    }
    var=0;
    return;
}

typedef struct {
    uint32_t check_count;
} scheduler_info;




int sched_initialiser(ABT_sched sched, ABT_sched_config config)
{
    scheduler_info *p_data = (scheduler_info *)calloc(1, sizeof(scheduler_info));

    ABT_sched_config_read(config, 1, &p_data->check_count);
    void* t=p_data;
    ABT_sched_set_data(sched, t);

    return ABT_SUCCESS;
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
    int id;
    ABT_pool_get_id(ult_pools[0],&id);
    id=id-2;
    while (1) {
        ABT_pool_pop_thread(ult_pools[0], &thread);
        if (thread != ABT_THREAD_NULL) {
            
            if(tracing_enabled){
                ABT_mutex_lock(ranklock);
            ABT_unit_id tid;
            
            ABT_thread_get_id(thread,&tid);
            int get=(int)tid;
           if(id_to_executor[get]==0){
            id_to_executor[get]=id+1;
           }
        
              ABT_mutex_unlock(ranklock);
            }
            ABT_self_schedule(thread, ABT_POOL_NULL);
        } else if (num_pools > 1 && replay_enabled==0) {
            if(num_pools==2){
                steal_from=1;
            }
            else{
                steal_from=rand_r(&seed) % (num_pools - 1);
                steal_from++;
            }
           

            size_t size;
            ABT_pool_get_size(ult_pools[steal_from],&size);
            if((int)size>1){ ABT_pool_pop_thread(ult_pools[steal_from], &thread);}
            if (thread != ABT_THREAD_NULL) {
                if(get_count==1){
                ABT_mutex_lock(msteal);
                steal_count++;
                ABT_mutex_unlock(msteal);
                }
                if(tracing_enabled){
                    ABT_mutex_lock(ranklock);
                ABT_unit_id tid;
            ABT_thread_get_id(thread,&tid);
            int get=(int)tid;
           
            if(id_to_executor[get]==0 ){
                trace_workers[id].sc++;
                id_to_executor[get]=id+1;
            }
           
             ABT_mutex_unlock(ranklock);
                }
                ABT_self_schedule(thread, ult_pools[steal_from]);
            }
        }
        if(replay_enabled==1){
            
            task* steal=popsteallist(id);
            if(steal!=NULL){
                printf("In\n");
                ABT_self_schedule(*steal->thread,ult_pools[steal->wc]);
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
            if (stop == ABT_TRUE){
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



void argolib_worker_wake(int val){
    if(slept_till + val <= num_klts){
    slept_till+=val;
    for(int i=slept_till;i<slept_till+val;i++){
        ABT_cond_signal(energy_cond[i]);
    }
    }
    return;
}
void argolib_worker_sleep(int val){
    if(slept_till-val>0){
    slept_till-=val;
    }
    return;
}

// void energy_thread() { 
// const int fixed_interval=10;
// sleep(20); 
// double JPI_prev=0;
// while(!shutdown) {
// double JPI_curr = read_hardware_performance_counters();
// configure_DOP(JPI_prev, JPI_curr);
// JPI_prev = JPI_curr;
// sleep(fixed_interval);
// }
// }
// void configure_DOP(JPI_prev, JPI_curr) {
// static int lastAction = INC, wActive=wMax;
// const int wChange=2; // find experimentally on your system
// if(called for first time) {
// sleep_argolib_num_workers(wChange);
// lastAction=DEC;
// return;
// }
// if(energy reduced since last call) {
// if(lastAction==DEC) {
// sleep_argolib_num_workers(wChange);
// } else {
// awake_argolib_num_workers(wChange);
// }
// } else {
// if(lastAction == DEC) {
// awake_argolib_num_workers(wChange);
// lastAction = INC;
// } else {
// sleep_argolib_num_workers(wChange);
// lastAction = DEC;
// }
// }
// }

int sched_free(ABT_sched sched)
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
void create_scheds(int n)
{
    ABT_sched_config sched_configuration;
    ABT_pool *ult_pools;
    
    

    ABT_sched_config_var sched_check_count = { .idx = 0,
                                           .type = ABT_SCHED_CONFIG_INT };

    if(private==1){
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

void argolib_init(int argc, char **argv){
    if(argc>3){
        get_count=1;
    }
    ABT_mutex_create(&mtask);
    ABT_mutex_create(&msteal);
    num_klts = atoi(argv[1]);
    seed = time(NULL);
     
    id_to_executor=(int*)malloc(sizeof(int)*memory);
    cond=(ABT_cond*)malloc(sizeof(ABT_cond)*num_klts);
    mut=(ABT_mutex*)malloc(sizeof(ABT_mutex)*num_klts);
    kernel_level_threads = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_klts);
    thread_pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_klts);
    master_scheduler = (ABT_sched *)malloc(sizeof(ABT_sched) * num_klts);
    workers=(worker*)malloc(sizeof(worker)*num_klts);
    trace_workers=(trace_worker*)malloc(sizeof(trace_worker)*num_klts);
    ABT_init(argc, argv);
   
    for(int i=0;i<num_klts;i++){
        workers[i].request_id=-1;
        workers[i].mailbox_count=0;
        workers[i].mail_box=ABT_THREAD_NULL;
        workers[i].request_sent=0;
    }
    for (int i = 0; i < num_klts; i++) {
            ABT_pool_create_basic(ABT_POOL_RANDWS, ABT_POOL_ACCESS_MPMC,
                                  ABT_TRUE, &thread_pools[i]);
            ABT_mutex_create(&mut[i]);
            ABT_cond_create(&cond[i]);
    }
     
    for (int i = 0; i < num_klts; i++) {
        if(atoi(argv[2])==1){
            private=1;
        }
        else{
            energy=1;
            slept_till=num_klts;
            energy_cond=(ABT_cond*)malloc(sizeof(ABT_cond)*num_klts);
            energy_mutex=(ABT_mutex*)malloc(sizeof(ABT_mutex)*num_klts);
        }
        create_scheds(num_klts);
    }
    

    
    ABT_xstream_self(&kernel_level_threads[0]);
   
    ABT_xstream_set_main_sched(kernel_level_threads[0], master_scheduler[0]);
     
    
    for (int i = 1; i < num_klts; i++) {
        ABT_xstream_create(master_scheduler[i], &kernel_level_threads[i]);
    }
        
}

void argolib_finalize(){
    
    for (int i = 1; i < num_klts; i++) {
        ABT_xstream_join(kernel_level_threads[i]);
        ABT_xstream_free(&kernel_level_threads[i]);
    }
    for(int i=0;i<num_klts;i++){
        ABT_cond_broadcast(cond[i]);
        ABT_mutex_unlock(mut[i]);
    }
    ABT_finalize();
    
    free(kernel_level_threads);
    free(thread_pools);
    free(master_scheduler);
    free(id_to_executor);
    shutdown=1;
}

void argolib_kernel(fork_t fptr, void* args){
    task_count=0;
    steal_count=0;
    double t1 = ABT_get_wtime();
    fptr(args);
    double t2 = ABT_get_wtime();
    printf("Time: %f\n",t2-t1);
    if(get_count==1){
    printf("Task Count: %d\n",task_count);
    printf("Steal Count: %d\n",steal_count);
    }
   
}

void addtotraceworker(int id, task t){
    task_head_push(id,t);
    return;
}

Task_handle* argolib_fork(fork_t fptr, void* args){
    int rank;
    int rank_changed=0;
    if(var==0){
        rank=0;
        var=1;
    }
    ABT_xstream_self_rank(&rank);
    if(replay_enabled){
        task* tempz=task_head_pop(rank);
        trace_workers[rank].ac++;
        if(tempz!=NULL && tempz->id==trace_workers[rank].ac){
            task_tail_push(*tempz,rank);
            rank=tempz->we;
            rank_changed=1;
        }
        
        else{
            
            if(tempz!=NULL){
            task_head_push(rank,*tempz);
            }
        }
         
    }
    Task_handle *child1=(ABT_thread *)malloc(sizeof(ABT_thread));
   
    if(get_count==1){
    ABT_mutex_lock(mtask);
    task_count++;
    ABT_mutex_unlock(mtask);
    }
     ABT_pool target_pool = thread_pools[rank];
    if(!rank_changed){
    ABT_thread_create(target_pool,*fptr,args,
                          ABT_THREAD_ATTR_NULL,child1);
    }
    else{
       
        ABT_thread_create(target_pool,*fptr,args,
                          ABT_THREAD_ATTR_NULL,child1);
                          
        task *ret=(task*)malloc(sizeof(task));
        ret->thread=child1;
        ret->id=trace_workers[rank].ac;
        ret->wc=rank;
        ret->we=-1;
        ret->sc=0;
        
        pushsteallist(*ret,rank);
       
        
    }
    if(tracing_enabled){
    ABT_unit_id thread_id;
    ABT_thread_get_id(*child1,&thread_id);
    task ret={child1,++trace_workers[rank].ac,(uint32_t)thread_id,rank,-1,0};
    addtotraceworker(rank,ret);
    }
    return child1;
}
void argolib_join(Task_handle** list, int size){
    for(int i=0;i<size;i++){
        ABT_thread_free(list[i]);
    }
    return;
}