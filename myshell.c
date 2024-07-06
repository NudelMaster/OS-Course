#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>



int perform_input_redirection(char** arglist, int redirect_index) {
	pid_t pid = fork();
	if (pid < 0) {
		perror("Fork");
		return 0;
	}
	if(pid == 0) {
		// child process
		if(signal(SIGINT, SIG_DFL) == SIG_ERR) {
			// child should terminate upon SIGINT
			perror("SIGNAL");
			exit(1);
		}
		int fd = open(arglist[redirect_index+1], O_RDONLY);
		if(fd == -1) {
			perror("Open");
			exit(1);
		}
		if(dup2(fd, 0) == -1) {
			// redirect stdin to the input file
			perror("Dup error");
			exit(1);

		}
		// after redirecting file discriptor no longer needed
		close(fd);
		// remove redirection symbol for execution
		arglist[redirect_index] = NULL;
		if(signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
			// default child SIGINT handling to be restored to ensure correct handling after execvp
			perror("Error on changing child signal");
			exit(1);
		}
		if(execvp(arglist[0], arglist) == -1) {
			perror("Execution failed");
			exit(1);
		}
	}
	// parent process
	if (waitpid(pid, NULL, 0)== -1) {
		if(errno != ECHILD && errno != EINTR) {
			perror("waitpid");
			exit(1);
		}
		// otherwise child process finished successfuly
	}
	return 1;
}
int perform_output_riderection(char** arglist, int redirect_index) {
	pid_t pid = fork();
	if (pid < 0) {
		perror("Fork");
		return 0;
	}
	if (pid == 0) {
		// child process
		if(signal(SIGINT, SIG_DFL) == SIG_ERR) {
			// child should terminate upon SIGINT
			perror("SIGNAL");
			exit(1);
		}
		int fd = open(arglist[redirect_index+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
		if(fd == -1) {
			perror("Open");
			exit(1);
		}
		if(dup2(fd, 1) == -1) {
			// redirect stdin to the output file
			perror("Dup error");
			exit(1);
		}
		// no need to use the fd anymore
		close(fd);
		// Remove redirection symbol for execution
		arglist[redirect_index] = NULL;
		if(signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
			// default child SIGINT handling to be restored to ensure correct handling after execvp
			perror("Error on changing child signal");
			exit(1);
		}
		if(execvp(arglist[0], arglist) == -1) {
			perror("Execution failed");
			exit(1);
		}
	}
	// parent process
	if (waitpid(pid, NULL, 0)== -1) {
		if(errno != ECHILD && errno != EINTR) {
			perror("waitpid");
			exit(1);
		}
		// otherwise child process finished successfuly
	}
	return 1;

}
int perform_pipe(char** arglist, int pipe_index) {
	int pipefd[2];
	pid_t pid1, pid2;
	if (pipe(pipefd) == -1) {
		perror("pipe");
		return 0;
	}

	// creating first child for 1 end of the pipe
	pid1 = fork();
	if (pid1 < 0) {
		perror("fork");
		return 0;
	}
	// first child process
	if (pid1 == 0) {
		if(signal(SIGINT, SIG_DFL) == SIG_ERR) {
			// child should terminate upon SIGINT
			perror("SIGNAL");
			exit(1);
		}
		// closing reading end for this child
		close(pipefd[0]);
		if(dup2(pipefd[1], 1) == -1) {
			perror("dup error");
			exit(1);
		}
		// close write end after duplication
		close(pipefd[1]);

		if(signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
			// default child SIGINT handling to be restored to ensure correct handling after execvp
			perror("Error on changing child signal");
			exit(1);
		}
		// preparing for the command execution
		arglist[pipe_index] = NULL;
		if(execvp(arglist[0], arglist) == -1) {
			perror("Execution failed");
			exit(1);
		}

	}
	// second child for 2nd pipe end
	pid2 = fork();
	if(pid2 < 0) {
		perror("fork");
		exit(1);
	}
	// child process
	if(pid2 == 0) {
		if(signal(SIGINT, SIG_DFL) == SIG_ERR) {
			// child should terminate upon SIGINT
			perror("SIGNAL");
			exit(1);
		}
		// no writing to the pipe
		close(pipefd[1]);
		if(dup2(pipefd[0], 0) == -1) {
			perror("dup error");
			exit(1);
		}
		// close the second end of the pipe
		close(pipefd[0]);
		if(signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
			// default child SIGINT handling to be restored to ensure correct handling after execvp
			perror("Error on changing child signal");
			exit(1);
		}
		if(execvp(arglist[pipe_index+1], arglist + pipe_index + 1) == -1) {
			perror("Execution failed");
			exit(1);
		}
	}
	// parent process close both ends of the pipe
	close(pipefd[0]);
	close(pipefd[1]);


	if (waitpid(pid1, NULL, 0)== -1) {
		if(errno != ECHILD && errno != EINTR) {
			perror("waitpid");
			exit(1);
		}
		// otherwise child process finished successfuly
	}
	if (waitpid(pid2, NULL, 0)== -1) {
		if(errno != ECHILD && errno != EINTR) {
			perror("waitpid");
			exit(1);
		}
		// otherwise child process finished successfuly
	}
	return 1;
}
int background(char** arglist, int count) {
	pid_t pid;
	// create child for background process
	pid = fork();
	// ignore the & character for reading the command
	arglist[count-1] = NULL;
	if (pid == -1){
		// fork failed
		perror("fork");
		return 0;
	}
	// child process
	if(pid == 0) {
		// ignore SIGINT during background process
		if(signal(SIGINT, SIG_IGN) == SIG_ERR) {
			perror("signal");
			exit(1);
		}
		if(signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
			// default child SIGINT handling to be restored to ensure correct handling after execvp
			perror("Error on changing child signal");
			exit(1);
		}
		if(execvp(arglist[0], arglist) == -1) {
			perror("Execution failed");
			exit(1);
		}
	}
	// parent process returns 1
	return 1;

}
int perform_non_background(char** arglist) {
	pid_t pid = fork();
	if (pid == -1){
		// fork failed
		perror("fork");
		return 0;
	}
	if (pid == 0) {
		if(signal(SIGINT, SIG_DFL) == SIG_ERR) {
			// child should terminate upon SIGINT
			perror("SIGNAL");
			exit(1);
		}
		if(signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
			// default child SIGINT handling to be restored to ensure correct handling after execvp
			perror("Error on changing child signal");
			exit(1);
		}
		if(execvp(arglist[0], arglist) == -1) {
			perror("Execution failed");
			exit(1);
		}
	}
	// parent process
	if (waitpid(pid, NULL, 0)== -1) {
		if(errno != ECHILD && errno != EINTR) {
			perror("waitpid");
			exit(1);
		}
		// otherwise child process finished successfuly
	}
	return 1;

}
// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char** arglist)
{
	int pipe = 0, input_riderect = 0, output_redirect = 0, pipe_index = 0, redirect_index = 0;

	if (strcmp(arglist[count-1], "&") == 0) {
		// initialize background process
		return background(arglist, count);
	}

	int i = 0;
	while (arglist[i] != NULL) {
		if(strcmp(arglist[i], "|") == 0) {
			pipe_index = i;
			pipe = 1;
		}
		if(strcmp(arglist[i], "<") == 0) {
			input_riderect = 1;
			redirect_index = i;
		}
		if(strcmp(arglist[i], ">>") == 0) {
			output_redirect = 1;
			redirect_index = i;
		}
		i++;
	}
	if (pipe) {
		return perform_pipe(arglist, pipe_index);
	}
	if (input_riderect) {
		return perform_input_redirection(arglist, redirect_index);
	}
	if(output_redirect) {
		return perform_output_riderection(arglist, redirect_index);
	}
	// if code reached this point that non background process
	return perform_non_background(arglist);

}


void sigchld_handler(int sig) {
	int saved_err = errno;
	pid_t wait_pid;
	while((wait_pid = waitpid(-1, 0, WNOHANG)) > 0) {
		// zombies reaped
	}
	if(wait_pid == -1) {
		if(errno != ECHILD && errno != EINTR) {
			fprintf(stderr, "waiting failed for zombie handler Error: %s\n", strerror(errno));
		}
	}
	errno = saved_err;
}
// prepare and finalize calls for initialization and destruction of anything required
int prepare(void){
	// to complete in case of initializations
	// set up handler for child to get rid of zombies
	struct sigaction sa;
	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	if(sigaction(SIGCHLD, &sa, NULL) < 0) {
		fprintf(stderr, "sigaction register fail Error %s\n", strerror(errno));
		return 1;
	}
	if(signal(SIGINT, SIG_IGN) == SIG_ERR) {
		// parent process ignores SIGINT
		fprintf(stderr, "parent signal register failed, Error %s\n", strerror(errno));
		return 1;
	}
	return 0;
}
int finalize(void){
	// to complete if needed before exit
	return 0;
}

