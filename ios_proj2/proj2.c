/*
 * File:        proj1.c
 * Date:        April 2019
 * Author:      Kateřina Mušková, xmusko00@stud.fit.vutbr.cz
 * Project:     River Crossing Problem
 * Description: Proceses, semaphores, critical section, ..
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>

#include <sys/wait.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <semaphore.h>
#include <time.h>

#include <stdbool.h>


//******  errors, serf, hack
#define OK 0
#define ARG_ERROR 1
#define FORK_ERROR 2
#define FILE_ERROR 3
#define SHM_MEM_SEM_ERROR 4
#define INTERRUPTED_ERROR 5 

#define HACK 0
#define SERF 1


//* help
void help(){
    printf("River Crossing Problem\n");
    printf("Program expects 5 int arguments\n");
}

//******************************************************************************
//********************* ARGUMENTS and their processing *************************

// global structure, where parameters are stored
typedef struct{
    int P;
    int H; 
    int S;
    int R;
    int W;
    int C;
}param_st;

param_st params;

/*
 * function converts *char to int and check it
 */
unsigned int str_to_int(char *str) {
    unsigned int n;

    char *p_end = NULL;
    n = (unsigned int) strtol(str, &p_end, 10);

    //error check
    if (p_end == NULL || p_end == str){
        return UINT_MAX;
    }
    return n;
}


/*
 * processing argv, validating
 * after convert to int stored to params array
 */
void process_arguments(int argc, char** argv){
    if(argc == 2 && ( strcmp("--help", argv[1]) == 0 || strcmp("-help", argv[1]) == 0)) {
        help();
        exit(OK);
    }

    if(argc != 7){
        fprintf(stderr, "Wrong arguments. Program expects 6 int arguments\n");;
        exit(ARG_ERROR);
    }

    unsigned int int_argv[argc-1];

    for(int i=1; i<argc; i++){
        if((int_argv[i] = str_to_int(argv[i])) == UINT_MAX){
            fprintf(stderr, "Wrong arguments. Program expects 6 int arguments.\n");;
            exit(ARG_ERROR);
        }
    }

    params.P = int_argv[1];
    params.H = int_argv[2];
    params.S = int_argv[3];
    params.R = int_argv[4];
    params.W = int_argv[5];
    params.C = int_argv[6];

    if(params.P < 2 || (params.P % 2) != 0  ||  params.H < 0 || params.H > 2000 || params.S < 0 || params.S > 2000.){
        fprintf(stderr, "Error in first 3 arguments.");
        exit(ARG_ERROR);
    }

    if(params.R < 0 || params.R > 2000 || params.W < 20 ||  params.W > 2000 || params.C <5 ){
        fprintf(stderr, "Error in last 3 arguments.");
        exit(ARG_ERROR);
    }
}

//******************************************************************************
//********************* SHARED MEMORY and SEMAPHORES ***************************

//******  SHARED MEMORY

#define SHM_KEY_STRUCT "/xmusko00.ios.proj2.shm_struct"
#define FI_NAME "proj2.out"

typedef struct{
    int action_id;
    int number_hackers;
    int number_serfs;
    bool is_group_opened; 
    int boarded_serfs;
	int boarded_hackers;
}shm_struct;

shm_struct* shm_vars = NULL;  // global pointer to shared memory + file descriptor 
int shm_fd_struct;

FILE* file_log = NULL;

/*
 * inicialization of shared memory
 */
int init_shm_all(){
	  shm_fd_struct = shm_open(SHM_KEY_STRUCT, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	  if(shm_fd_struct == -1){
	  	perror("Shm_open failed");
	  	return -1;
	  }

	  ftruncate(shm_fd_struct, sizeof(shm_struct));

	  shm_vars = mmap(NULL, sizeof(shm_struct), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_struct, 0);
	  if (shm_vars == MAP_FAILED){
	  	perror("Mmap failed");
	  	shm_unlink(SHM_KEY_STRUCT);
		close(shm_fd_struct);

	  	return -1;
	  }

	  shm_vars->action_id = 1;
	  shm_vars->number_hackers = 0;
	  shm_vars->number_serfs = 0;
	  shm_vars->is_group_opened = false;
	  shm_vars->boarded_serfs = 0;
	  shm_vars->boarded_hackers = 0;

	return 0;
}

/*
 * closing and unlinking shm
 */
void unlink_shm_all(){
	munmap(shm_vars, sizeof(shm_struct));
	shm_unlink(SHM_KEY_STRUCT);
	close(shm_fd_struct);
}

//******  SEMAPHORES

// global names 
#define SEM_NAME_DATA "/xmusko00.ios.proj2.WRITE_F"
#define SEM_NAME_QUERY "/xmusko00.ios.proj2.QUERY"
#define SEM_NAME_GROUP_CHECK "/xmusko00.ios.proj2.G_CHECK"
#define SEM_NAME_JOIN_H "/xmusko00.ios.proj2.J_H"
#define SEM_NAME_JOIN_S "/xmusko00.ios.proj2.J_S"
#define SEM_NAME_BOARD "/xmusko00.ios.proj2.BOARD"
#define SEM_NAME_CAPITAN_WAKES "/xmusko00.ios.proj2.C_WAKES"
#define SEM_NAME_CAPITAN_LANDS "/xmusko00.ios.proj2.C_LANDS"
#define SEM_NAME_MEMBER_LANDS "/xmusko00.ios.proj2.M_LANDS"

sem_t* sem_data = NULL;
sem_t* sem_query = NULL;
sem_t* sem_group_check = NULL;
sem_t* sem_join_H = NULL;
sem_t* sem_join_S = NULL;
sem_t* sem_board = NULL;
sem_t* sem_capitan_wakes = NULL;
sem_t* sem_capitan_lands = NULL;
sem_t* sem_member_lands = NULL;

/*
 * inicialization of all semapohores
 */
int init_sem_all(){
    sem_data = sem_open(SEM_NAME_DATA, O_CREAT | O_EXCL, 0666, 1);
    sem_query = sem_open(SEM_NAME_QUERY, O_CREAT | O_EXCL, 0666, 1);

    sem_group_check = sem_open(SEM_NAME_GROUP_CHECK, O_CREAT | O_EXCL, 0666, 1);
    sem_join_H = sem_open(SEM_NAME_JOIN_H, O_CREAT | O_EXCL, 0666, 0);
    sem_join_S = sem_open(SEM_NAME_JOIN_S, O_CREAT | O_EXCL, 0666, 0);

    sem_board = sem_open(SEM_NAME_BOARD, O_CREAT | O_EXCL, 0666, 1);

    sem_capitan_wakes = sem_open(SEM_NAME_CAPITAN_WAKES, O_CREAT | O_EXCL, 0666, 0);
    sem_capitan_lands = sem_open(SEM_NAME_CAPITAN_LANDS, O_CREAT | O_EXCL, 0666, 0);
    sem_member_lands = sem_open(SEM_NAME_MEMBER_LANDS, O_CREAT | O_EXCL, 0666, 1);

    if(sem_data == SEM_FAILED || sem_query == SEM_FAILED ||  // control if everithing succeeded
    	sem_group_check == SEM_FAILED ||
    	sem_join_H == SEM_FAILED || sem_join_S == SEM_FAILED || sem_board == SEM_FAILED ||
    	sem_capitan_wakes == SEM_FAILED || sem_capitan_lands == SEM_FAILED || sem_member_lands == SEM_FAILED){

    fprintf(stderr, "Errno number: %d by opening semaphore.\n", errno);
    return -1;
	}
   
    return 0;
}

/*
 * closing and unlinking semaphores
 */
void unlink_sem_all(){
	sem_close(sem_data);
	sem_unlink(SEM_NAME_DATA);

	sem_close(sem_query);
	sem_unlink(SEM_NAME_QUERY);

	sem_close(sem_group_check);
	sem_unlink(SEM_NAME_GROUP_CHECK);

	sem_close(sem_join_H);
	sem_unlink(SEM_NAME_JOIN_H);

	sem_close(sem_join_S);
    sem_unlink(SEM_NAME_JOIN_S);

    sem_close(sem_board);
    sem_unlink(SEM_NAME_BOARD);

    sem_close(sem_capitan_wakes);
    sem_unlink(SEM_NAME_CAPITAN_WAKES);

    sem_close(sem_capitan_lands);
    sem_unlink(SEM_NAME_CAPITAN_LANDS);

    sem_close(sem_member_lands);
    sem_unlink(SEM_NAME_MEMBER_LANDS);
}

//******  CLEAN
void clean_up(){
	unlink_sem_all();
	unlink_shm_all();
	if(file_log != NULL) fclose(file_log);
}

//******************************************************************************
//********************* LIFE OF HACKERS and SERFS ******************************

//  log statuses
#define STARTS 0
#define LEAVES_QUERY 1
#define IS_BACK 2
#define WAITS 3
#define BOARDS 4
#define MEMBER_EXT 5
#define CAPITAN_EXT 6

char* log_opt[7]={ 
	"starts",
	"leaves queue",
	"is back",
	"waits",
	"boards",
	"member exits",
	"captain exits"
};

/*
 * prints log to proj2.out
 */
void print_log(char* kind_str, int id, int what_to_print){

	if(what_to_print == STARTS || what_to_print == IS_BACK){
		fprintf(file_log, "%d: %-5s\t%d: %s\n", shm_vars->action_id, kind_str, id+1, log_opt[what_to_print]);
	} else{
		fprintf(file_log, "%d: %-5s\t%d: %s\t:%d\t:%d\n", shm_vars->action_id, kind_str, id+1, log_opt[what_to_print],
		 shm_vars->number_hackers, shm_vars->number_serfs);
	}
	shm_vars->action_id++;	
}

/*
 * check if processes on query are able to make "boardable" group 
 */
bool check_query_group(){
	if (shm_vars->number_serfs >=2 && shm_vars->number_hackers >= 2) return true;
	if (shm_vars->number_serfs >= 4 || shm_vars->number_hackers >= 4) return true;

	return false;
}

/*
 * its already ensured the processes on query are boardable
 * allow 4 processes to board
 * if its possible to board calling process, board it as well (ret value tru)
 */
bool make_group_incl_this_process(int* num_my_kind, int* num_other_kind, sem_t* sem_my_kind, sem_t* sem_other_kind){

	if (*num_my_kind >=2 && *num_other_kind >= 2){
		
		sem_post(sem_my_kind);
		for(int i=0; i<2; i++) sem_post(sem_other_kind);
		return true;
	}
	
	if(*num_my_kind >= 4){
		for(int i=0; i<3; i++) sem_post(sem_my_kind);
		return true;

	} else{
		for(int i=0; i<4; i++) sem_post(sem_other_kind);
		return false;
	}
	
}

/*
 * entire live of hackers and serfs
 */
void passenger_life(int id, int kind){

	int* num_my_kind;
	int* num_other_kind;

	sem_t* sem_my_kind;
	sem_t* sem_other_kind;

	char* kind_str;

	int* boarded_my_kind;
	int* boarded_other_kind;

	int sleep_max = (params.W + 1)*1000;
    srand(time(NULL) * getpid());			//random sleep

	if(kind == HACK){
		num_my_kind = &shm_vars->number_hackers;
		sem_my_kind = sem_join_H;

		num_other_kind = &shm_vars->number_serfs;
		sem_other_kind = sem_join_S;

		kind_str = "HACK";

		boarded_my_kind = &shm_vars->boarded_hackers;
		boarded_other_kind = &shm_vars->boarded_serfs;

	} else{
		num_my_kind = &shm_vars->number_serfs;
		sem_my_kind = sem_join_S;

		num_other_kind = &shm_vars->number_hackers;
		sem_other_kind = sem_join_H;

		kind_str = "SERF";

		boarded_my_kind = &shm_vars->boarded_serfs;
		boarded_other_kind = &shm_vars->boarded_hackers;
	}

	//** PRINT START
	sem_wait(sem_data);
	print_log(kind_str, id, STARTS);
	sem_post(sem_data);
	
	//** GET TO QUERY
	while(1){
		sem_wait(sem_query);

		sem_wait(sem_data);
		int num = shm_vars->number_hackers + shm_vars->number_serfs;
		sem_post(sem_data);

		if (num < params.C){
			// it is possible to get to query
			sem_wait(sem_data);
			(*num_my_kind)++;			
			print_log(kind_str, id, WAITS);
			sem_post(sem_data);

			sem_post(sem_query);
			break;

		} else{
			// go away a return after sleep_max miliseconds
			sem_wait(sem_data);
			print_log(kind_str, id, LEAVES_QUERY);
			sem_post(sem_data);

			sem_post(sem_query);

			int time = rand() % (sleep_max);
			usleep(time);

			sem_wait(sem_data);
			print_log(kind_str, id, IS_BACK);
			sem_post(sem_data);

		}
	}

	//** PREPARE BOARD GROUP	
	sem_wait(sem_group_check);
	sem_wait(sem_query);

	// check if has already oppend the group, if not check if its possible to create group
	if(shm_vars->is_group_opened == false && check_query_group()){ 
		//ok, open goup and let others to join 

		if(make_group_incl_this_process(num_my_kind, num_other_kind, sem_my_kind, sem_other_kind) == true){ 
		// this process is includeded automaticly, it doesnt have to wait 
			shm_vars->is_group_opened = true;
			sem_post(sem_query);
			sem_post(sem_group_check);


		} else{ 	
		// it is not possible to add this process directly to group, it has to wait like others
			sem_post(sem_query);
			sem_post(sem_group_check);

			sem_wait(sem_my_kind);
		}

	} else{ 	
		//someone has already opened the group or will open												
		sem_post(sem_query);
		sem_post(sem_group_check);

		sem_wait(sem_my_kind);
	}

	//** BOARD
	sem_wait(sem_board);
	(*boarded_my_kind)++;
	int num_b = *boarded_my_kind + *boarded_other_kind;
	if(num_b < 4) sem_post(sem_board);

	//** SAIL
	if(num_b == 4){ //its capitan, the last one

		//prevent others to check_group, it could make problems by small molo capacity
		sem_wait(sem_group_check);

		//capitan prints log + others atomicly leave query 
		sem_wait(sem_data);
		*num_my_kind = *num_my_kind - *boarded_my_kind;
		*num_other_kind = *num_other_kind - *boarded_other_kind;

		print_log(kind_str, id, BOARDS);
		sem_post(sem_data);

		//allow others to check_group
		shm_vars->is_group_opened = false;
		sem_post(sem_group_check);

		//sail
		int time_c = (params.R +1) * 1000;
		usleep(rand() % time_c);

		for(int i=0; i<3; i++) sem_post(sem_capitan_wakes); // signal end of sail

		sem_wait(sem_capitan_lands); // capitan lands as last one
		shm_vars->boarded_hackers = 0; // clan 
		shm_vars->boarded_serfs = 0; 

		//** EXIT
		sem_wait(sem_data);
		print_log(kind_str, id, CAPITAN_EXT);
		sem_post(sem_data);

		//allow next goup to  board
		sem_post(sem_board);

	}else{ // member

		//sail
		sem_wait(sem_capitan_wakes);

		//land
		sem_wait(sem_member_lands);
		(*boarded_my_kind)--;

		//** EXIT
		sem_wait(sem_data);
		print_log(kind_str, id, MEMBER_EXT);
		sem_post(sem_data);

		if (*boarded_my_kind + *boarded_other_kind == 1) sem_post(sem_capitan_lands);
		sem_post(sem_member_lands);

	}
}

/*
 * hacker and serf kinds create theri own processes
 */
void lets_make_some_other_children(int kind, int count, int waiting_time){

    waiting_time = (waiting_time + 1) * 1000; // to microseconds

    pid_t pid_array[count]; // array of children
    for (int i=0; i<count; i++){
    	pid_t pid;

        if(waiting_time != 0)  usleep((unsigned int) (rand() % (waiting_time))); //wait some time

        if ((pid = fork()) < 0) {
            perror("Fork error.");
            kill(getpgid(0), SIGUSR1);  // let it know main process
            exit(FORK_ERROR);
        }

        if (pid == 0) { 
        	// child
        	passenger_life(i, kind);
            exit(OK);
        }
        //parrent
        pid_array[i] = pid;
    }

    // parrent waits for children
    for(int i=0; i<count; i++){
    	waitpid(pid_array[i], NULL, 0);
    }
}

/*
 * handles any errors
 */
void sig_handler(int sig_num){

	if(getpgid(0) == getpid()){  				// main process (group leader)
		if(sig_num == SIGUSR1 || sig_num == SIGTERM || sig_num == SIGINT){ 
												// bad new from child to main_process or terminating from outside
			signal(SIGCHLD, SIG_IGN);			// please no child zombie

			signal(SIGTERM, SIG_IGN);			// ignore others signals
			signal(SIGINT, SIG_IGN);
			signal(SIGUSR1, SIG_IGN);

			kill(0, SIGTERM); 					// tells everyone in the group to terminate self, SIG_INT the same

			clean_up();
			exit(INTERRUPTED_ERROR);
		}
	} else { 									//child or grandchild
		if(sig_num == SIGTERM || sig_num == SIGINT){
			signal(SIGCHLD, SIG_IGN);			// he could have children as well

			signal(SIGTERM, SIG_IGN);
			signal(SIGINT, SIG_IGN);
			signal(SIGUSR1, SIG_IGN);

			kill(getpgid(0), SIGUSR1);   		// what if someone else has terminated the child from outside, let it know the group leader

			exit(0);
		}
	}
}


int main(int argc, char** argv) {
	
	//set main_proces as group leader with gpid pid
	setpgid(0, 0);	

	//signals, nice exit if anithing happens
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);

    process_arguments(argc, argv);

    //init file
    file_log = fopen(FI_NAME, "w");
    if (file_log == NULL) {
    	perror("Cant open file.");
    	exit(FILE_ERROR);
    }
    setbuf(file_log,NULL);
    setbuf(stderr,NULL);

    //init shared memory
    unlink_shm_all();  // for sure
    unlink_sem_all();

    if(init_sem_all() == -1){
    	clean_up();
    	exit(SHM_MEM_SEM_ERROR);
    }

    if(init_shm_all() == -1){
    	clean_up();
    	exit(SHM_MEM_SEM_ERROR);
    }

    pid_t hackerPID;
    pid_t serfPID;
    pid_t pid;

    // make hacker kind
    if ((pid = fork()) < 0) {				
        perror("Fork error.");				// check success, (it will be successful anyway, but what if ...)
        signal(SIGTERM, SIG_IGN);
        kill(0, SIGTERM); 					// tells everyone in the group to terminate self, SIG_INT the same

		clean_up();
        exit(FORK_ERROR);
    }

    if (pid == 0) { 
    										// hacker kind - firstborn child
        lets_make_some_other_children(HACK, params.P, params.H);
        exit(OK);
    }

    // BIG BOSS process waiting for everyone to die - parent.
    hackerPID = pid;

    //*********

    // make serf kind
    if ((pid = fork()) < 0) {											
        perror("Fork error.");				// check success, (it will be successful anyway, but what if ...)
        signal(SIGTERM, SIG_IGN);
        kill(0, SIGTERM); 					// tells everyone in the group to terminate self, SIG_INT the same

		clean_up();
        exit(FORK_ERROR);
    }

    if (pid == 0) { 
    	// serf kind - secondborn child
        lets_make_some_other_children(SERF, params.P, params.S);
        exit(OK);
    }

    // BIG BOSS process - parent
	// And I will be waiting for you too, SERIF ....
	serfPID = pid;

    // ...right here. You see, Im waiting.
    waitpid(hackerPID, NULL, 0);
    waitpid(serfPID, NULL, 0);

    //unlik shared memory, semaphores, close windows, unplug appliances, lock the doors and say goodbye
    unlink_sem_all();
    unlink_shm_all();
    fclose(file_log);

    // die in peace
    return 0;
}
