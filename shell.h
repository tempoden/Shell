#include <sys/types.h>


/*Max amount of arguments per command*/
#define MAXARGS 256
/*Max amount of commands in line*/
#define MAXCMDS 50
/*Max amount of jobs (Stopped or running in background)*/
#define MAXJOBS 20
/*Max length of job's "title"
example of output format:
[./sh] jobs
#N      JOB STATUS      JOB TITLE
[1]     Stopped         find ../../ -name *.c | xargs cat | grep 1 | less
[2]     Stopped         sleep 20
[3]     Running         sleep 40 | sleep 40 | sleep 40
*/
#define MAXTITLE 1024

/*
cmdflag - for pipe handling (==OUTPIPE for the first process in pipeline /#\ ==INIPIPE for the seconds process in pipelene /#\
                             ==OUTPIPE|INPIPE for all others members of pipeline)
bkgrnd  - background flag   (==0 for foreground command /#\ ==1 for background command)
infile  - input filename            <
outfile - output&replace filename   >
appfile - output&append filename    >>
*/
typedef struct command
{
    char *cmdargs[MAXARGS];
    char cmdflag;
    char bkgrnd;
    char *infile;
    char *outfile;
    char *appfile;
} cmd_t;


/*JOB STATUS defines*/
#define STOPPED 10
#define RUNNING 11
#define DONE 0

/*
job_title   -   containes shell command which led to job execution
job_pids    -   PIDs of all processes in pipeline in the same order as they were in command
job_pgid    -   process group ID of all processes in job
size        -   amount of PIDs in job_pids (usually equal to amount of procceses in job and group)
index       -   job index in job control system (to handle multiple fg\bg calls to the same job)
status      -   shows wheter job is stopped, running, or done
*/
typedef struct _job
{
	char job_title[MAXTITLE];
    pid_t job_pids[MAXCMDS];
    pid_t job_pgid;
    size_t size;
	size_t index;
	int status;
} job_t;


/*  cmdflag's  */
#define OUTPIP  01
#define INPIP   02

extern struct command cmds[];
/*extern char *infile, *outfile, *appfile;*/
extern char bkgrnd;
extern job_t jobs[];
extern size_t job_count;
int parseline(char *);
int promptline(char *, char *, int);


/*job control unit*/

void wait_job(job_t*);/*Waits job in foreground. In case of SIGTSTP moves shell back to foreground and pushes job to jobs[]*/

void wait_bg();/*Waits Running background jobs*/

void add_to_title(char *, char **, char);/*Adds string to title*/

void init_title(char *, char **, char);/*Initiates title with given string*/

int pop_job(job_t*);/*remove job from jobs[]*/

int push_job(job_t*);/*push job to jobs[]*/

int get_job(job_t*, int);/*get job from jobs[] at specific index (fg command handling)*/

void jobs_bg(char **);/*Handles bg command*/

void print_jobs(cmd_t);/*Print list of jobs*/

void pop_done();/*Remove terminated and done jobs from jobs[]*/

int check_for_termination(job_t*);/*Check job for abnormal termination (WIFSIGNALED)*/