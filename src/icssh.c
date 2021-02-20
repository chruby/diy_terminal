#include "icssh.h"
#include <readline/readline.h>
#include <errno.h>
#include <signal.h>
#include "helpers.h"
#include "linkedList.h"

// conditional flag denoting that a child has been terminated
// true = child has been terminated, false = child has not been terminated
volatile sig_atomic_t child_terminated = false;



void sigchld_handler(int signal)
{
	child_terminated = true; // setting conditional flag to be true
}


void sigusr2_handler(int signal)
{
	printf("Hi User! I am process %d\n", getpid());
}


int main(int argc, char* argv[]) {
	int exec_result;
	int exit_status;
	pid_t pid;
	pid_t wait_result;
	char* line;
	// file descriptors used in redirection
	int open_file_fd = -1;
	int writing_out_file_fd = -1;
	int writing_err_file_fd = -1;
	int writing_err_out_file_fd = -1;
	// copy of stdout, stdin, stderr
	int stdin_copy = dup(0);
	int stdout_copy = dup(1);
	int stderr_copy = dup(2);
	// previous directory to support "cd -" command
	// char prev_dir[63] = "/home/ics53/hw3";
	

	// List_t current_bg_procs[32]; // list of current background processes
#ifdef GS
    rl_outstream = fopen("/dev/null", "w");
#endif

	// Setup segmentation fault handler
	if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
		perror("Failed to set signal handler");
		exit(EXIT_FAILURE);
	}


	// install SIGCHLD handler
	if (signal(SIGCHLD, sigchld_handler) == SIG_ERR)
	{
		perror("Failed to set SIG_CHLD signal handler");
		exit(EXIT_FAILURE);
	}

	// initialize job list
	List_t *current_bg_procs = (List_t *)malloc(sizeof(List_t)); // list of current background processes
	current_bg_procs->head = NULL;
	current_bg_procs->length = 0;

	


	// install sigusr2 handler
	if (signal(SIGUSR2, sigusr2_handler) == SIG_ERR)
	{
		perror("failed to set SIGUSR2 signal handler");
		exit(EXIT_FAILURE);
	}


    // print the prompt & wait for the user to enter commands string
	while ((line = readline(SHELL_PROMPT)) != NULL) {

        // MAGIC HAPPENS! Command string is parsed into a job struct
        // Will print out error message if command string is invalid
		job_info* job = validate_input(line);
		
		// creating pipe
		// int num_pipe_args = my_pow(2, (job->nproc-1));
		// int my_pipe[num_pipe_args];
		// int j = 0;
		// for (j = 0; j < num_pipe_args; j++)
		// {
		// 	my_pipe[j] = 0;
		// }
		

		// reaping any/all background child processes that have terminated before executing any other commands
		if (child_terminated == true) //   || current_bg_procs->length > 0
		{
			// if global flag is true, reap all child procs before executing any commands
			// reap all terminated bg proc's one by one
			pid_t current_pid;
			while ((current_pid = waitpid(-1, &exit_status, WNOHANG)) > 0) // -1 means any child process; returns pid of child whose state has changed, otherwise -1
			{
				node_t *current = current_bg_procs->head;
				int index = 0;
				while (current != NULL)
				{
					if (current_pid == ((bgentry_t *)current->value)->pid)
					{
						printf(BG_TERM, current_pid, ((bgentry_t *)current->value)->job->line);
						// delete_job(current_pid, current_bg_procs); // 	 with this
						bgentry_t *deleting = removeByIndex(current_bg_procs, index);
						// printf("=====about to free job --%s-- on line 104\n", deleting->job->line);
						free_job(deleting->job);
						free(deleting);
						break;
					}
					index++;
					current = current->next;
				}
			}
			child_terminated = false;
		}

        if (job == NULL) { // Command was empty string or invalid
			free(line);
			continue;
		}

		// too many arguments
		if (job->procs->argc > 32)
		{
			printf("%s, too many arguments\n", EXEC_ERR);
			free_job(job);
			continue;
		}
		// each argument string can be 63 characters max
		int i = 0;
		for (i = 0; i < job->procs->argc; i++)
		{
			char *current_arg = job->procs->argv[i];
			if (strlen(current_arg) > 63)
			{
				printf("%s, argument is too long\n", EXEC_ERR);
				free_job(job);
				continue;
			}
		}
		

		// make sure none of the file names for i/o redirection match
		bool files_dont_match = validate_file_names(job->procs->in_file,
													job->procs->out_file,
													job->procs->err_file,
													job->procs->outerr_file);
		// if the files have the same name, print rd_err 
		if (files_dont_match == false)
		{
			fprintf(stderr, "%s", RD_ERR);
			free_job(job);
			continue;
		}
		
		// initializing pipes
		int my_pipe[job->nproc-1][2]; // 2 processes: pipe[1][2], 3 proc's: pipe[2][2]
		int k = 0, j = 0;
		for (k = 0; k < job->nproc-1; k++)
		{
			for (j = 0; j < 2; j++)
			{
				my_pipe[k][j] = 0;
			}
		}




		
        //Prints out the job linked list struture for debugging
        // #ifdef DEBUG   // If DEBUG flag removed in makefile, this will not longer print
        //     debug_print_job(job);
        // #endif
		
		// example built-in: exit
		if (strcmp(job->procs->cmd, "exit") == 0) {
			// Terminating the shell
			if (current_bg_procs->length > 0)
			{
				node_t *current = current_bg_procs->head;
				while (current != NULL)
				{
					printf(BG_TERM, ((bgentry_t *)current->value)->pid, ((bgentry_t *)current->value)->job->line);
					kill(((bgentry_t *)current->value)->pid, SIGKILL); // terminate the process thats still running
					current = current->next;
					bgentry_t *deleting = removeFront(current_bg_procs); // the process you want to delete from your list
					free_job(deleting->job); // free the memory of the job we deleted
					free(deleting);
				}
			}
			
			deleteList(current_bg_procs);
			free(current_bg_procs);
            free_job(job);
			free(line);
			
			validate_input(NULL);
            return 0;
		}


		// built-in: cd
		else if (strcmp(job->procs->cmd, "cd") == 0)
		{
			int return_val; // result of calling chdir

			char *desired_dir = job->procs->argv[1]; // where you want to change directories into
			if (desired_dir == NULL) // if you run just "cd"
			{
				desired_dir = "/home/ics53"; // change to home directory
			}

			return_val = chdir(desired_dir); // 0 on success, -1 on error
			if (return_val == 0) // chdir success
			{
				char buff[63];
				printf("%s\n", getcwd(buff, 63));
			}
			else // failed
			{
				fprintf(stderr, DIR_ERR);
			}
			
			continue;
		}


		// command: estatus
		else if (strcmp(job->procs->cmd, "estatus") == 0 && WIFEXITED(exit_status))
		{
			// printf("estaus\n");
			printf("%d\n", WEXITSTATUS(exit_status));
			continue;
		}


		// command: bglist
		else if (strcmp(job->procs->cmd, "bglist") == 0)
		{
			if (current_bg_procs->length > 0)
			{
				node_t *current = current_bg_procs->head;
				while (current != NULL)
				{
					print_bgentry((bgentry_t *)current->value);
					current = current->next;
				}
			}
			continue;
		}


		// extra credit - ascii53 command
		else if (strcmp(job->procs->cmd, "ascii53") == 0)
		{
			char *line0 = " _      _        _  _      _  _  _    _\n";
			char *line1 = "|_|    |_|      |_||_|    |_||_||_|  | |\n";
			char *line2 = "|_|    |_|    |_|            |_|     | |\n";
			char *line3 = "|_|    |_|    |_|            |_|     |_|\n";
			char *line4 = "|_|_  _|_|    |_|_  _      _ |_| _    _ \n";
			char *line5 = "  |_||_|        |_||_|    |_||_||_|  |_|\n";
			
			printf("%s%s%s%s%s%s", line0, line1, line2, line3, line4, line5);
			continue;
		}

		// create a pipe if you have a pipe command
		// if (job->nproc > 1)
		// {
		// 	int m = 0;
		// 	for (m = 0; m < num_pipe_args; m+=2)
		// 	{
		// 		pipe(my_pipe+m);
		// 	}
			 
		// }

		if (job->nproc > 1)
		{
			int m = 0;
			for (m = 0; m < job->nproc-1; m++)
			{
				pipe(my_pipe[m]);
			}
		}





		// example of good error handling!
		if ((pid = fork()) < 0) {
			perror("fork error");
			exit(EXIT_FAILURE);
		}
		if (pid == 0) {  // If zero, then it's the CHILD process
		    proc_info* proc = job->procs;
			// open the file where you're going to redirect to/from
			if (job->procs->in_file) // if there's a stdin file (< redirection)
			{
				// instead of getting input from stdin, we get it from in_file
				open_file_fd = open(job->procs->in_file, O_RDONLY);
				if (open_file_fd < 0)
				{
					fprintf(stderr, "%s", RD_ERR);
					continue;
				}
				dup2(open_file_fd, 0); // 0 = stdin
				// when to close? should dup2() be done in the parent?
			}
			if (job->procs->out_file) // if there's a stdout file (> redirection)
			{
				// want to print into the out_file instead of std_out
				writing_out_file_fd = open(job->procs->out_file, O_WRONLY | O_CREAT);
				if (writing_out_file_fd < 0)
				{
					fprintf(stderr, "%s", RD_ERR);
					continue;
				}
				dup2(writing_out_file_fd, 1); // 1 = stdout
			}
			if (job->procs->err_file) // if there's a stderr file (2> redirection)
			{
				// if you want to redirect from stderr output to err_file
				writing_err_file_fd = open(job->procs->err_file, O_WRONLY | O_CREAT);
				if (writing_err_file_fd < 0)
				{
					fprintf(stderr, "%s", RD_ERR);
					continue;
				}
				dup2(writing_err_file_fd, 2); // 2 = stderr
			}
			if (job->procs->outerr_file) // if there's a stderr&stdout file (&> redirection)
			{
				// if you want to redirect both stdout and stderr into outerr_file
				writing_err_out_file_fd = open(job->procs->outerr_file, O_WRONLY | O_CREAT);
				if (writing_err_out_file_fd < 0)
				{
					fprintf(stderr, "%s", RD_ERR);
					continue;
				}
				dup2(writing_err_out_file_fd, 1); // 1 = stdout
				dup2(writing_err_out_file_fd, 2); // 2 = stderr
			}
			
			if (job->nproc > 1) // if you're piping
			{
				// printf("	1st - cmd:%s, close:mypipe[0], write:mypipe[1], 1\n", proc->cmd);
				// close(my_pipe[0]); // close the read end of the pipe
				// dup2(my_pipe[1], 1); // connect stdout to write end of pipe
				// pipe(my_pipe[0]);
				// printf("	1st - cmd:%s, close:mypipe[0][0], write:mypipe[0][1], stdout: 1\n", proc->cmd);
				close(my_pipe[0][0]);
				dup2(my_pipe[0][1], 1);
			}

			exec_result = execvp(proc->cmd, proc->argv); // execute command
			if (exec_result < 0) {  //Error checking
				dup2(stdout_copy, 1); // restoring stdout back to normal
				printf(EXEC_ERR, proc->cmd);
				// Cleaning up to make Valgrind happy 
				// (not necessary because child will exit. Resources will be reaped by parent or init)
				free_job(job);  
				free(line);
    			validate_input(NULL);
				exit(EXIT_FAILURE);
			}
		}
		else {
            // As the PARENT, wait for the foreground job to finish
			// if you're piping, you do NOT want to wait for the process to finish
			if (job->bg == false && job->nproc <= 1) // if you want to wait for the process to finish
			{
				wait_result = waitpid(pid, &exit_status, 0);
				if (wait_result < 0) {
					printf(WAIT_ERR);
					exit(EXIT_FAILURE);
				}
				free_job(job);  // if a foreground job, we no longer need the data
			}
			else if (job->bg == true && job->nproc <=1)// if you don't want to wait for the process to finish, aka a background process
			{
				add_job(job, pid, current_bg_procs); // add process to list of bg processes
			}
			// else if (job->nproc > 10000) // change to > 1 when testing this
			// {
			// 	// after execvp runs for piping
			// 	// close(my_pipe[1]);    // close write end after exec
			// 	// second fork for piping
			// 	pid_t new_pid;
			// 	proc_info* new_proc = job->procs->next_proc;
			// 	// int read_ind = 0;
			// 	// int write_ind = 3;
			// 	// int close_ind = 1;
			// 	int pipe_ind = 0;
			// 	while (new_proc != NULL)
			// 	{
			// 		// if (pipe_ind > 0)
			// 		// {
			// 		// 	pipe(my_pipe[pipe_ind]);
			// 		// }
			// 		if ((new_pid = fork()) < 0) {
			// 			perror("fork error");
			// 			exit(EXIT_FAILURE);
			// 		}
			// 		if (new_pid == 0) {  // If zero, then it's the CHILD process
			// 			// printf("read:%d, write:%d\n", read, write);
			// 			// does first command even make it here? no
			// 			if (strcmp(new_proc->cmd, job->procs->argv[0]) != 0) // if you're not at the first command
			// 			{
			// 				// printf("	3rd - cmd:%s, closing:mypipe[%d], read:mypipe[%d], 0\n", new_proc->cmd, close_ind, read_ind);
			// 				// close(my_pipe[close_ind]);	// close write end of pipe
			// 				// dup2(my_pipe[read_ind], 0); // redirect read end of pipe to stdin
			// 				// close_ind++;
			// 				printf("	3rd - cmd:%s, closing:mypipe[%d][1], read:mypipe[%d][0], 0\n", new_proc->cmd, pipe_ind, pipe_ind);
			// 				close(my_pipe[pipe_ind][1]);
			// 				dup2(my_pipe[pipe_ind][0], 0);
			// 			}
			// 			if (new_proc->next_proc != NULL) // if you're not at the last process
			// 			{
			// 				// printf("	2nd - cmd:%s, closing:mypipe[%d], write:mypipe[%d], 1\n", new_proc->cmd, close_ind, write_ind);
			// 				// close(my_pipe[close_ind]); 	 // close read end of pipe
			// 				// dup2(my_pipe[write_ind], 1); // redirect stdout to write end of pipe
			// 				// close_ind++;
			// 				close(my_pipe[pipe_ind][0]);
			// 				pipe_ind++;
			// 				printf("	2nd - cmd:%s, closing:mypipe[%d][0], write:mypipe[%d][1], 1\n", new_proc->cmd, pipe_ind, pipe_ind);
			// 				dup2(my_pipe[pipe_ind][1], 1);
			// 			}
			// 			// closing everything in my_pipe
			// 			// int m = 0;
			// 			// for (m = 0; m < num_pipe_args; m++)
			// 			// {
			// 			// 	close(my_pipe[m]);
			// 			// }
			// 			// int a = 0;
			// 			// for (a = 0; a < 2; a++)
			// 			// {
			// 			// 	close(my_pipe[pipe_ind][a]);
			// 			// }
			// 			close(my_pipe[pipe_ind][0]);
			// 			if (new_proc->next_proc != NULL)
			// 			{
			// 				close(my_pipe[pipe_ind][1]);
			// 			}
			// 			else
			// 			{
			// 				dup2(stdout_copy, 1);
			// 				dup2(stderr_copy, 2);
			// 			}
			// 			exec_result = execvp(new_proc->cmd, new_proc->argv);
			// 			if (exec_result < 0) {  //Error checking
			// 				dup2(stdout_copy, 1); // restoring stdout back to normal
			// 				printf(EXEC_ERR, new_proc->cmd);
			// 				// Cleaning up to make Valgrind happy 
			// 				// (not necessary because child will exit. Resources will be reaped by parent or init)
			// 				free_job(job);  
			// 				free(line);
			// 				validate_input(NULL);
			// 				exit(EXIT_FAILURE);
			// 			}
			// 		}
			// 		else 
			// 		{
			// 			// if (strcmp(new_proc->cmd, job->procs->argv[0]) != 0)
			// 			// {
			// 			// 	read_ind+=2;
			// 			// 	close_ind++;
			// 			// }
			// 			// if (new_proc->next_proc != NULL)
			// 			// {
			// 			// 	write_ind+=2;
			// 			// 	close_ind++;
			// 			// }
			// 			if (new_proc->next_proc != NULL)
			// 			{
			// 				pipe_ind++;
			// 			}
			// 			// int m = 0;
			// 			// for (m = 0; m < num_pipe_args; m++)
			// 			// {
			// 			// 	close(my_pipe[m]); // close all pipes
			// 			// }
			// 			int a = 0;
			// 			for (a = 0; a < 2; a++)
			// 			{
			// 				close(my_pipe[pipe_ind][a]);
			// 			}
			// 			// As the PARENT, wait for the foreground job to finish
			// 			if (new_proc->next_proc == NULL) // if you're at the last command, wait
			// 			{
			// 				// printf("pid:%d\n", new_pid);
			// 				dup2(stdout_copy, 1); // restore stdout?
			// 				dup2(stderr_copy, 2); // restore stderr?
			// 				wait_result = waitpid(new_pid, &exit_status, 0);
			// 				if (wait_result < 0) {
			// 					printf(WAIT_ERR);
			// 					exit(EXIT_FAILURE);
			// 				}
			// 			}
			// 		}
			// 		new_proc = new_proc->next_proc;
			// 	}
			// 	free_job(job); // does this free all the proc's as well?
			// 	dup2(stdin_copy, 0); // restore stdin?
			// }
			
			else if (job->nproc > 1)
			{
				// after execvp runs for piping
				pid_t new_pid;
				proc_info* new_proc = job->procs->next_proc;
				int pipe_ind = 0;

				while (new_proc != NULL)
				{
					// second fork for piping
					if ((new_pid = fork()) < 0) {
						perror("fork error");
						exit(EXIT_FAILURE);
					}
					if (new_pid == 0) // If zero, then it's the CHILD process
					{  
						if (strcmp(new_proc->cmd, job->procs->argv[0]) != 0) // if not at the first process
						{
							// printf("cmd:%s, closing:mypipe[%d][1], read:my_pipe[%d][0], stdin: 0\n", new_proc->cmd, pipe_ind, pipe_ind);
							close(my_pipe[pipe_ind][1]);
							dup2(my_pipe[pipe_ind][0], 0);
						}
						// else
						// {
						// 	close(my_pipe[0][0]);
						// 	dup2(my_pipe[0][1], 1);
						// }
						if (new_proc->next_proc != NULL) // if not at the last process
						{
							// printf("cmd:%s, closing:mypipe[%d][0], write:my_pipe[%d][1], stdout: 1\n", new_proc->cmd, pipe_ind, pipe_ind);
							close(my_pipe[pipe_ind][0]);
							dup2(my_pipe[pipe_ind+1][1], 1);
						}
						// else
						// {
						// 	close(my_pipe[0][1]);
						// 	dup2(my_pipe[0][0], 0);
						// }
						int a = 0, b = 0;
						for (a = 0; a < job->nproc-1; a++) // close everything in my_pipes except for current pipe
						{
							for (b = 1; b >= 0; b--)
							{
								if (a != pipe_ind)
								{
									// printf("post dups - cmd:%s, closing my_pipe[%d][%d]\n", new_proc->cmd, a, b);
									close(my_pipe[a][b]);
								}
							}
						}

						// close(my_pipe[1]); 		// close the write end of the pipe
						// dup2(my_pipe[0], 0); 	// connect stdin to read end of pipe
						exec_result = execvp(new_proc->cmd, new_proc->argv);
						if (exec_result < 0) {  //Error checking
							dup2(stdout_copy, 1); // restoring stdout back to normal
							printf(EXEC_ERR, new_proc->cmd);
							// Cleaning up to make Valgrind happy 
							// (not necessary because child will exit. Resources will be reaped by parent or init)
							free_job(job);
							free(line);
							validate_input(NULL);
							exit(EXIT_FAILURE);
						}
					}
					else
					{
						// As the PARENT, wait for the foreground job to finish
						if (new_proc->next_proc == NULL) // if you're at the last cmd, wait
						{
							int a = 0, b = 0;
							for (a = 0; a < job->nproc-1; a++)
							{
								for (b = 0; b < 2; b++)
								{
									close(my_pipe[a][b]);
								}
							}
							
							wait_result = waitpid(new_pid, &exit_status, 0);
							if (wait_result < 0) {
								printf(WAIT_ERR);
								exit(EXIT_FAILURE);
							}
						}
					}
					pipe_ind += 1;
					new_proc = new_proc->next_proc;
				}
				free_job(job);  
			}
		}

		
		
		// closing all files that were opened
		if (open_file_fd != -1)
		{
			close(open_file_fd);
		}
		if (writing_out_file_fd != -1)
		{
			close(writing_out_file_fd);
		}
		if (writing_err_file_fd != -1)
		{
			close(writing_err_file_fd);
		}
		if (writing_err_out_file_fd != -1)
		{
			close(writing_err_out_file_fd);
		}
		
		free(line);
	}

    // calling validate_input with NULL will free the memory it has allocated
    validate_input(NULL);
	
	
	// if (open_file_fd != -1)
	// {
	// 	close(open_file_fd);
	// }
	// if (writing_out_file_fd != -1)
	// {
	// 	close(writing_out_file_fd);
	// }
	// if (writing_err_file_fd != -1)
	// {
	// 	close(writing_err_file_fd);
	// }
	// if (writing_err_out_file_fd != -1)
	// {
	// 	close(writing_err_out_file_fd);
	// }


#ifndef GS
	fclose(rl_outstream);
#endif
	return 0;
}
