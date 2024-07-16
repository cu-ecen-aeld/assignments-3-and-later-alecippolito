#include "systemcalls.h"
#include  <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
	int ret = system(cmd);
	// system(cmd) call failed
	if (ret == -1){
		return false;
	}
	return WEXITSTATUS(ret) == 0; // return ret executing with 0 (success)
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
	va_list args;
	va_start(args, count);
	char * command[count+1];
	int i;
	for(i=0; i<count; i++)
	{
		command[i] = va_arg(args, char *);
	}
	command[count] = NULL;
	// this line is to avoid a compile warning before your implementation is complete
	// and may be removed
	command[count] = command[count];
	
	va_end(args);
	
	// make new process
	pid_t pid = fork();
	
	// fork call failed
	if (pid == -1){
		return false;
	} 
	
	// child process
	if (pid == 0){
		execv(command[0], command);
		exit(1); // error if execv returns
	} 
	// parent process
	else { 
		// wait for child process to change state
		int status;
		if (waitpid(pid, &status, 0) == -1){
			return false;
		}	
	return WEXITSTATUS(status) == 0; // return status executing with 0 (success)	
	}

}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
	va_list args;
	va_start(args, count);
	char * command[count+1];
	int i;
	for(i=0; i<count; i++)
	{
		command[i] = va_arg(args, char *);
	}
	command[count] = NULL;
	// this line is to avoid a compile warning before your implementation is complete
	// and may be removed
	command[count] = command[count];

	va_end(args);
	
	// open file with fd - parameters from https://stackoverflow.com/a/13784315/1446624 
	int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	// fd open failed
	if(fd == -1){
		exit(1);
	}
	// make new process
	pid_t pid = fork();
	// fork call failed
	if (pid == -1){
		return false;
	}	
	// child process
	if (pid == 0){
		// redireect td to output file
		if (dup2(fd,1) == -1){
			exit(1);
		}		
		close(fd);
		execv(command[0], command); 
		exit(1); // error if execv returns
	} 
	// parent process
	else{ 
		// wait for child process to change state
		int status;
		if (waitpid(pid, &status, 0) == -1){
			return false;
		}		
	return WEXITSTATUS(status) == 0; // return status executing with 0 (success)
	}

}
