#include "shell.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <wait.h>

size_t job_count = 0;

int push_job(job_t* new_job){
	if(new_job->index == ((size_t) -1)){
		new_job->index = job_count++;
		memcpy(&jobs[job_count - 1], new_job, sizeof(job_t));
	}else{
		memcpy(&jobs[new_job->index], new_job, sizeof(job_t));
		if(new_job->index >= job_count){
			job_count++;
		}
	}
	if (new_job->status == RUNNING) {
		printf("[%ld]\t%d\n", new_job->index+1, new_job->job_pids[new_job->size-1]);
	} else {
		printf("\n[%ld]+\tStopped\t\t%s\n", new_job->index+1, jobs[new_job->index].job_title);
	}
	return 0;
}

int pop_job(job_t* dest){
	if(job_count == 0 || check_for_termination( &jobs[job_count-1])){
		return -1;
	}
	job_count--;
	printf("%s\n", jobs[job_count].job_title);
	memcpy(dest, &jobs[job_count], sizeof(job_t));
	memset(&jobs[job_count], 0, sizeof(job_t));
	dest->status = RUNNING;
	
	return 0;
}

int check_for_termination(job_t* job){
	int stat;
	pid_t pid = waitpid(job->job_pids[job->size-1], &stat, WNOHANG);
	if(pid>0 && ((WIFEXITED(stat)) || (WIFSIGNALED(stat)))){
		printf("\n[%ld]+\tTerminated\t\t%s\n", job->index+1, job->job_title);
		kill(-job->job_pids[0], SIGKILL);
		size_t i;
		for (i = 0; i < job->size-1; i++) {
			waitpid(job->job_pids[i], 0, 0);
		}
		memset(job, 0, sizeof(job_t));
		return 1;
	}else{
		return 0;
	}
}

int get_job(job_t* dest, int index) {
	if (index > job_count || index <= 0) {
		return -1;
	}
	if(check_for_termination(&jobs[index-1])){
		return -1;
	}
	if (index == job_count) {
		return pop_job(dest);
	}
	else {
		printf("%s\n", jobs[index - 1].job_title);
		memcpy(dest, &jobs[index - 1], sizeof(job_t));
		memset(&jobs[index - 1], 0, sizeof(job_t));
		dest->status = RUNNING;
	}
	return 0;
}

void add_to_title(char* title, char **cmdargs, char flag) {
	int k = 0;
	while (cmdargs[k] != NULL) {
		strcat(title, cmdargs[k++]);
		strcat(title, " ");
	}
	if(flag&OUTPIP)
		strcat(title, "| ");
}

void init_title(char* title, char **cmdargs, char flag) {
	int i;
	for (i = 0; i < MAXTITLE; i++)
		title[i] = 0;
	add_to_title(title, cmdargs, flag);
}

void wait_job(job_t* job) {	
	int stat = 0;
CHECK:
	if (waitpid(job->job_pids[job->size-1], &stat, WUNTRACED) != -1) {
		//If Ctrl+Z (default) is pressed, move fg processes to bg
		//  && (SIGTSTP == WSTOPSIG(stat) || SIGSTOP == WSTOPSIG(stat))
		if (WIFSTOPPED(stat)) {
			job->status = STOPPED;
			push_job(job);
			return;
		} else if (!((WIFEXITED(stat)) || (WIFSIGNALED(stat)))) {
			goto CHECK; //If fg proccess is still "alive", we should coninue waiting it
		}
	}
	else {
		perror("Couldn't wait for child process termination");
	}
	kill(-job->job_pids[0], SIGKILL);
	size_t i;
	for (i = 0; i < job->size-1; i++) {
		waitpid(job->job_pids[i], 0, 0);
	}
}

void jobs_bg(char ** args) {
	int i = 1;
	while (args[i] != NULL) {
		char flag = 1;
		int k = 0;
		while (args[i][k] != '\0') {
			if (args[i][k]<'0' || args[i][k]>'9')
				flag = 0;
			k++;
		}
		//printf("flag%d\n", (int)flag);
		if (flag) {
			int index = atoi(args[i]) - 1;
			if (index > job_count || index < 0) {
				printf("-shell:bg: no such job [%d]\n", index+1);
			}
			else if(jobs[index].status == STOPPED){
				jobs[index].status = RUNNING;
				printf("[%d]+\t%s &\n",index+1, jobs[index].job_title);
				kill(-jobs[index].job_pids[0], SIGCONT);

			}
		}
		i++;
	}
	if (i == 1) {
		size_t k = (job_count > 0) ? job_count - 1 : 0;
		while (jobs[k].status == RUNNING && k > 0) {
			k--;
		}
		if (jobs[k].status == STOPPED) {
			jobs[k].status = RUNNING;
			printf("[%ld]+\t%s &\n", k+1, jobs[k].job_title);
			size_t j;
			for (j = 0; j < jobs[k].size; j++)
				kill(jobs[k].job_pids[j], SIGCONT);
		} else {
			printf("-shell:bg: no stopped jobs\n");
		}
	}
	return;
}

void wait_bg() {
	pop_done();
	size_t i;
	int stat;
	for (i = 0; i < job_count; i++) {
		pid_t pid = -1;
		if(jobs[i].status == RUNNING){
			pid = waitpid(jobs[i].job_pids[jobs[i].size-1] ,&stat, WNOHANG);
		}else{
			check_for_termination(&jobs[i]);
			continue;
		}
		if (pid > 0){
			size_t j;
			for (j = 0; j < jobs[i].size - 1; j++) {
				waitpid(jobs[i].job_pids[j], 0, 0);
			}
			if(WIFSIGNALED(stat)){
				printf("\n[%ld]+\tTerminated\t\t%s\n", i+1, jobs[i].job_title);
			}else{
				printf("\n[%ld]+\tDone\t\t%s\n", i+1, jobs[i].job_title);
			}
			memset(&jobs[i], 0, sizeof(job_t));
			jobs[i].status = DONE;
		}
		if (jobs[job_count - 1].size == 0)
			job_count--;
	}
}

void pop_done(){
	size_t i;
	for (i = 0; i < job_count; i++){
		if (jobs[job_count - 1].size == 0)
			job_count--;
	}
}

void print_jobs(cmd_t cmd){
	pop_done();
	FILE* of = stdout;
	char was_opened = 0;
	if(cmd.outfile != NULL || cmd.appfile!=NULL){
		was_opened = 1;
		if(cmd.outfile != NULL){
			of = fopen(cmd.outfile, "w");
		}else{
			of = fopen(cmd.appfile, "a");
		}
		if(of == NULL){
			was_opened = 0;
			printf("Can't open file\n");
			of = stdout;
		}
	}
	if(job_count == 0){
		fprintf(of, "No active jobs\n");
	}else{
		size_t i;
		for(i = 0; i < job_count; i++){
			char stat[20] = "\0";
			if(jobs[i].status == RUNNING){
				strcat(stat, "Running");
			}else if(jobs[i].status == STOPPED){
				strcat(stat, "Stopped");
			}else{
				strcat(stat, "Done");
			}
			fprintf(of, "[%ld]\t%s\t\t%s\n", i+1, stat, jobs[i].job_title);
		}
	}
	if(was_opened)
		fclose(of);
}

/*
ALL IS DONE: (THUG LIFE)

rewrite parser (parse "" and '', split commands between ;)
make new for-logic: solid and exlusive for each process group

 find ../../ -name *.c | xargs cat | grep 1
 find ../../ -name *.c | xargs cat | grep 1 | less
*/