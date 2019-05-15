#include <sys/types.h>



#define MAXARGS 256
#define MAXCMDS 50
#define MAXJOBS 20
#define MAXTITLE 1024

typedef struct command
{
    char *cmdargs[MAXARGS];
    char cmdflag;
    char bkgrnd;
    char *infile;
    char *outfile;
    char *appfile;
} cmd_t;

#define STOPPED 10
#define RUNNING 11
#define DONE 0

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
//extern char *infile, *outfile, *appfile;
extern char bkgrnd;
extern job_t jobs[];
extern size_t job_count;
int parseline(char *);
int promptline(char *, char *, int);


//job control unit
void wait_job(job_t*);
void wait_bg();
void add_to_title(char *, char **, char);
void init_title(char *, char **, char);
int pop_job(job_t*);
int push_job(job_t*);
int get_job(job_t*, int);
void jobs_bg(char **);
void print_jobs(cmd_t);
void pop_done();
int check_for_termination(job_t*);