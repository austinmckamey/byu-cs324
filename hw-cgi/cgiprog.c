#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
	char *buf;
	char arg1[2048];

	/* Extract the argument */
	if ((buf = getenv("QUERY_STRING")) != NULL) {
		strcpy(arg1, buf);
	}

	fprintf(stderr, "%d\n", (int)strlen(arg1));
	fprintf(stderr, "%s\n", arg1);
	/* Generate the HTTP response */
	printf("Content-type: text/plain\r\n");
	printf("Content-length: %d\r\n\r\n", (int)strlen(arg1));
	printf("The query string is: %s", arg1);
	fflush(stdout);
	exit(0);
}