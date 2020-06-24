/*
 * File:        init_error_exit.c
 * Date:        April 2019
 * Author:      Kateřina Mušková, xmusko00@stud.fit.vutbr.cz
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


// structure for program parameters
typedef struct{
    int P;
    int H;
    int S;
    int R;
    int W;
    int C;
}param_st;

param_st params;

#define OK 0
#define ARG_ERROR 1
#define FORK_ERROR 2
#define FILE_ERROR 3
#define SHM_MEM_SEM_ERROR 4
#define INTERRUPTED_ERROR 5 

#define HACK 0
#define SERF 1


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


//shared variables
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

shm_struct* shm_vars = NULL;
int shm_fd_struct;

FILE* file_log = NULL;


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

void unlink_shm_all(){
	munmap(shm_vars, sizeof(shm_struct));
	shm_unlink(SHM_KEY_STRUCT);
	close(shm_fd_struct);
}


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

    if(sem_data == SEM_FAILED || sem_query == SEM_FAILED || 
    	sem_group_check == SEM_FAILED ||
    	sem_join_H == SEM_FAILED || sem_join_S == SEM_FAILED || sem_board == SEM_FAILED ||
    	sem_capitan_wakes == SEM_FAILED || sem_capitan_lands == SEM_FAILED || sem_member_lands == SEM_FAILED){

    fprintf(stderr, "Errno number: %d by opening semaphore.\n", errno);
    return -1;
	}
   
    return 0;
}

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

void clean_up(){
	unlink_sem_all();
	unlink_shm_all();
	if(file_log != NULL) fclose(file_log);
}
