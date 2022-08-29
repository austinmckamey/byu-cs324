#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

/* ManHunt
	1. 1,2,3
	2. 1,2
	3. 2,3
	4. 2
	5. #include <sys/types.h>, #include <sys/stat.h>, #include <fcntl.h>
	6. 2,7
	7. socket(7)
	8. 3
	9. Null-terminated
	10. Greater than 0

I completed the TMUX exercise from part 2 */

int main(int argc, char *argv[]) {

int pid_t = getpid();
fprintf(stderr,"%d\n\n",pid_t);

char* env_var = getenv("CATMATCH_PATTERN");
FILE* fp = fopen(argv[1],"r");
char str[200];
while(1) {
	if(fgets(str,200,fp)!=NULL) {
		if(env_var) {
			char* match = strstr(str,env_var);
			if(match != NULL) {
				fprintf(stdout,"%d %s",1,str);
			}
			else {
				fprintf(stdout,"%d %s",0,str);
			}
		}
		else {
			fprintf(stdout,"%d %s",0,str);
		}
	}
	if(feof(fp)) {
		break;
	}
}
return 0;
}
