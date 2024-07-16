// C code for writing content to a fil with error checking
// Author: Alec Ippolito

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>



int main(int argc, char *argv[]){
	if (argc != 3){
		syslog(LOG_ERR, "Error: 2 arrguments expected as [filesdir] [searchstr] but were not provided.");
		return 1;
	}
	
	const char *writefile = argv[1];
	const char *writestr = argv[2];
	
	FILE *fp;
	fp = fopen(writefile, "w");
	if (fp == NULL){
		syslog(LOG_ERR,"Error: Failed to create file %s.", writefile);
		return 1;
	}
	fprintf(fp, "%s\n", writestr);
	syslog(LOG_DEBUG, "Writing '%s' to %s", writestr, writefile);
	fclose(fp);

	return 0;
}
