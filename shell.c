#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
#include "shell.h"
#include <errno.h>

//char *infile;
//char *outfile;
//char *appfile;
struct command cmds[MAXCMDS];
job_t jobs[MAXJOBS];
char cwd[1024];
//char bkgrnd;

int main(int argc,char *argv[])
{
    int i;
    char line[1024];      /*  allow large command lines  */
    int ncmds;
    char prompt[50];      /* shell prompt */

    /* PLACE SIGNAL CODE HERE */
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

    getcwd(cwd, sizeof(cwd));

    while(promptline(getcwd(cwd, sizeof(cwd)), line, sizeof(line)) > 0)
    {    /*
            l eof  */
        wait_bg();
        if(0 >= (ncmds = parseline(line)))  
        {
            continue;   /* read next line */
        }

		{
			int i;
			for (i = 0; i < ncmds; i++) {
				if (cmds[i].bkgrnd && (cmds[i].cmdflag&INPIP) && !(cmds[i].cmdflag&OUTPIP)) {
					int j = i-1;
					while (cmds[j].cmdflag&OUTPIP) {
						cmds[j].bkgrnd = cmds[i].bkgrnd;
						if (!cmds[j].cmdflag&INPIP) {
							break;
						}
						j--;
					}
				}
			}

		}
#ifdef DEBUG
        {
            int i,j;
            for (i = 0; i < ncmds; i++)
            {
                for (j = 0;NULL != cmds[i].cmdargs[j]; j++)
                {
                    fprintf(stderr,"cmd[%d].cmdargs[%d] = %s\n",i,j,cmds[i].cmdargs[j]);
                }
                if(cmds[i].infile!=NULL){
                    fprintf(stderr,"cmd[%d] infile = %s\n",i,cmds[i].infile);
                }
                if(cmds[i].outfile!=NULL){
                    fprintf(stderr,"cmd[%d] outfile = %s\n",i,cmds[i].outfile);
                }
                if(cmds[i].appfile!=NULL){
                    fprintf(stderr,"cmd[%d] appfile = %s\n",i,cmds[i].appfile);
                }
                fprintf(stderr,"cmds[%d].cmdflag = %o\n",i,cmds[i].cmdflag);
            }
        }
#endif
        {
			job_t current;
			int previous_pipe_output = -1;
            int pipeline_start = 0; //index of the first process in current pipeline in array pids
            pid_t pids[MAXCMDS];
            for (i=0;i < ncmds;i++){
                char *infile = cmds[i].infile;
                char *outfile = cmds[i].outfile;
                char *appfile = cmds[i].appfile;
                char bkgrnd = cmds[i].bkgrnd;
                int pipefd[2] = {-1, -1};//array for pipe fd

				if (!(cmds[i].cmdflag & INPIP)) {
					pipeline_start = i;
				}
				
				if (strcmp(cmds[pipeline_start].cmdargs[0], "exit") == 0) {
					return EXIT_SUCCESS;
				}

                if (strcmp(cmds[pipeline_start].cmdargs[0], "cd") == 0) {
                    cd(cmds[pipeline_start].cmdargs);
                    continue;
                }

                if (strcmp(cmds[pipeline_start].cmdargs[0], "jobs") == 0 || strcmp(cmds[pipeline_start].cmdargs[0], "joblist") == 0){
                    print_jobs(cmds[pipeline_start]);
                    continue;
                }

				if (strcmp(cmds[pipeline_start].cmdargs[0], "bg") == 0) {
					jobs_bg(cmds[pipeline_start].cmdargs);
					continue;
				}

				if (strcmp(cmds[pipeline_start].cmdargs[0], "fg") == 0) {
					int res;
					if (cmds[i].cmdargs[1] != NULL) {
                        char flag = 1;
                        int k = 0;
                        char* args = cmds[pipeline_start].cmdargs[1];
                        while (args[k] != '\0') {
                            if (args[k]<'0' || args[k]>'9')
                                flag = 0;
                            k++;
                        }
                        if(flag){
						    res = get_job(&current, atoi(cmds[i].cmdargs[1]));
                        }else{
                            printf("Syntax error\n");
                            continue;
                        }
                    } else {
						res = pop_job(&current);
					}
					if (res == 0) {
						if (tcsetpgrp(STDERR_FILENO, current.job_pgid) == -1) {
							perror("Couldn't set terminal foreground process group");
							return EXIT_FAILURE;
						}
						
                        kill(-current.job_pgid, SIGCONT);
                        
						wait_job(&current);
						if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
							perror("Couldn't set terminal foreground process group");
							return EXIT_FAILURE;
						}
					} else {
                        printf("No such job\n");
                    }
					continue;
				}

                if(cmds[i].cmdflag & OUTPIP) {
					//if process has pipeline output, create pipe
                    if(-1 == pipe(pipefd)) {
                        perror("Couldn't create pipe");
                        return EXIT_FAILURE;
                    }
                }
				
                /*  FORK AND EXECUTE  */
                pids[i] = fork();
				/*---------------\
				| HERE IS CHILD  |
				\---------------*/
                if(0 == pids[i]) {
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
					signal(SIGTTOU, SIG_DFL);
					//Collect all process of pipeline into one process group. 
					//If there are no pipelines, create unique group for every process
					if(-1 == setpgid(0, (cmds[i].cmdflag & INPIP)? pids[pipeline_start]:0)) {
                        perror("Couldn't set process group ID");
                        return EXIT_FAILURE;
                    }
					//If process is not in bg, move it to foreground
                    if(!bkgrnd && !(cmds[i].cmdflag & INPIP)) {
                        signal(SIGTTOU, SIG_IGN);
                        if(tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
                            perror("Couldn't set terminal foreground process group");
                            return EXIT_FAILURE;
                        }
                        signal(SIGTTOU, SIG_DFL);
                    }
					// I\O redirections
                    if(!(cmds[i].cmdflag&INPIP) && (infile != NULL)) {
                        int input = open(infile, O_RDONLY);
                        if(input != -1) {
                            if(dup2(input, STDIN_FILENO) == -1) {
                                perror("Couldn't redirect input");
                                close(input);
                                return EXIT_FAILURE;
                            }
                            close(input);
                        } else {
                            perror("Couldn't open input file");
                            return EXIT_FAILURE;
                        }
                    }
                    if(!(cmds[i].cmdflag&OUTPIP) && ((outfile != NULL) || (appfile != NULL))) {
                        int output = -1;
                        if(outfile != NULL) {
                            output = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, (mode_t)0644);
                        } else {
                            output = open(appfile, O_WRONLY | O_APPEND | O_CREAT, (mode_t)0644);
                        }
                        if(output != -1) {
                            if(dup2(output, STDOUT_FILENO) == -1) {
                                perror("Couldn't redirect output");
                                close(output);
                                return EXIT_FAILURE;
                            }
                            close(output);
                        } else {
                            perror("Couldn't open output file");
                            return EXIT_FAILURE;
                        }
                    }
					//If process is following another in pipeline, 
					// dup previous pipe out to stdin
                    if(cmds[i].cmdflag & INPIP) {
#ifdef DEBUG
						printf("I am number %d in line\n", i);
#endif
                        if(dup2(previous_pipe_output, STDIN_FILENO) == -1) {
                            perror("Couldn't redirect input");
                            close(previous_pipe_output);
                            return EXIT_FAILURE;
                        }
                        close(previous_pipe_output);
                    }
					//If process is followed by another in pipeline,
					// dup pipe in to stdout
                    if(cmds[i].cmdflag & OUTPIP) {
                        if(dup2(pipefd[1], STDOUT_FILENO) == -1) {
                            perror("Couldn't redirect output");
                            close(pipefd[1]);
                            return EXIT_FAILURE;
                        }
                        close(pipefd[1]);
                    }
					//execute
					if ((cmds[i].cmdflag & OUTPIP) && !(cmds[i].cmdflag & INPIP)) {
						kill(pids[i],SIGSTOP);
					}
                    execvp(cmds[i].cmdargs[0], cmds[i].cmdargs);
                    perror("Couldn't execute command");
                    return EXIT_FAILURE;
                } 
                /*---------------\	Your command for test: cat 29 | tail -f | sed '/S/s/g' | wc -l				
                | HERE IS PARENT |  Remember to remove quotes
                \---------------*/ 
                else if(pids[i] != -1) {
					if (!(cmds[i].cmdflag & INPIP)) {
						init_title(current.job_title, cmds[i].cmdargs, cmds[i].cmdflag);
						current.size = 1;
						current.job_pids[0] = pids[i];
						current.job_pgid = pids[i];
						current.index = (size_t) -1;
						current.status = RUNNING;
					}
                    if(cmds[i].cmdflag & INPIP) {
                        setpgid(pids[i], pids[pipeline_start]);
                        close(previous_pipe_output);
						add_to_title(current.job_title, cmds[i].cmdargs, cmds[i].cmdflag);
						current.size++;
						current.job_pids[i - pipeline_start] = pids[i];
						//printf("\t%d\n", pids[i]);
                    }
					//Close pipeline input
					if (cmds[i].cmdflag & OUTPIP) {
						close(pipefd[1]);
					}
					//Save pipe out for the next process in pipeline
                    previous_pipe_output = pipefd[0];
					//Handling multiple pipelines.					
					//Example: ps -o pid,ppid,pgid,sid,comm | cat | cat; ps -o pid,ppid,pgid,sid,comm | cat | cat
					if (!(cmds[i].cmdflag & OUTPIP)) {
						kill(pids[pipeline_start], SIGCONT);
						if (!bkgrnd) {
							//printf("Job %s is now fg\n", current.job_title);
							wait_job(&current);
							if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
								perror("Couldn't set terminal foreground process group");
								return EXIT_FAILURE;
							}
                            //printf("Job %s is now bg\n", current.job_title);
						}
						else {

							push_job(&current);
							if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
								perror("Couldn't set terminal foreground process group");
								return EXIT_FAILURE;
							}

						}
					}
					//Handling zombies
					//while (waitpid((pid_t)-1, NULL, WNOHANG) > 0);
                } else {
                    perror("Couldn't create process");
                }
                
            }
        }
    }  /* close while */
    return EXIT_SUCCESS;
}

/* PLACE SIGNAL CODE HERE */
