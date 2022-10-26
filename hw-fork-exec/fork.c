#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<wait.h>

int main(int argc, char *argv[]) {
	int pid;

	printf("Starting program; process has pid %d\n", getpid());

	FILE *stream = fopen("fork-output.txt","w");
	fprintf(stdout,"BEFORE FORK\n");
	fflush(stdout);
	int pipefd[2];
	pipe(pipefd);

	if ((pid = fork()) < 0) {
		fprintf(stderr, "Could not fork()");
		exit(1);
	}

	/* BEGIN SECTION A */

	fprintf(stdout,"SECTION A\n");
	fflush(stdout);
	//printf("Section A;  pid %d\n", getpid());
	//sleep(30);

	/* END SECTION A */
	if (pid == 0) {
		/* BEGIN SECTION B */

		fprintf(stdout,"SECTION B\n");
		fflush(stdout);
		//printf("Section B\n");

		//sleep(30);
		//sleep(30);
		//printf("Section B done sleeping\n");

		exit(0);

		/* END SECTION B */
	} else {
		/* BEGIN SECTION C */

		int status;
		fprintf(stdout,"SECTION C\n");
                fflush(stdout);
		//printf("Section C\n");

		//sleep(30);
		wait(&status);
		//printf("Section C done sleeping\n");
		char *newenviron[] = { NULL };

		printf("Program \"%s\" has pid %d.\n", argv[0], getpid());

		if(argc <= 1) {
			printf("No program to exec. Exiting...\n");
			exit(0);
		}

		printf("Running exec of \"%s\"\n", argv[1]);
		int fd = fileno(stream);
		int fd1 = fileno(stdout);
		dup2(fd, fd1);
		execve(argv[1], &argv[1], newenviron);
		printf("End of program \"%s\".\n", argv[0]);
		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */

	fprintf(stdout,"SECTION D\n");
	fflush(stdout);
	//printf("Section D\n");
	//sleep(30);

	/* END SECTION D */
}

