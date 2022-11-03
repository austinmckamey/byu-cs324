#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
Concurrency Homework
1.USER         PID    PPID NLWP     LWP S CMD
am895    1313214 1312785    1 1313214 S echoserveri
2.Only one thread and process is running, because the second and third client are waiting for the first client to disconnect from the server.
3.The second client that executed the nc command sends the bytes typed earlier to the server. The third client remains in a waiting phase.
4.USER         PID    PPID NLWP     LWP S CMD
am895    1315789 1312785    1 1315789 S echoserverp
am895    1315841 1315789    1 1315841 S echoserverp
am895    1315927 1315789    1 1315927 S echoserverp
am895    1315996 1315789    1 1315996 S echoserverp
5.There are 4 threads and 4 processes running, because the parent forks and makes a child to handle each client that connects to the server. This results in one parent and 3 children in this case.
6.USER         PID    PPID NLWP     LWP S CMD
am895    1317185 1312785    4 1317185 S echoservert
am895    1317185 1312785    4 1317216 S echoservert
am895    1317185 1312785    4 1317271 S echoservert
am895    1317185 1312785    4 1317288 S echoservert
7.There are 4 threads and 1 process running. In this case, the server is handling all of the clients at once. It opens and closes the clients, outputting the data it receives from it, as long as there are clients connected.
8.USER         PID    PPID NLWP     LWP S CMD
am895    1319076 1312785    9 1319076 S echoservert_pre
am895    1319076 1312785    9 1319077 S echoservert_pre
am895    1319076 1312785    9 1319078 S echoservert_pre
am895    1319076 1312785    9 1319079 S echoservert_pre
am895    1319076 1312785    9 1319080 S echoservert_pre
am895    1319076 1312785    9 1319081 S echoservert_pre
am895    1319076 1312785    9 1319082 S echoservert_pre
am895    1319076 1312785    9 1319083 S echoservert_pre
am895    1319076 1312785    9 1319085 S echoservert_pre
9.There are 9 threads running and 1 process running. This server creates 8 new threads before the clients request to connect. Then as they do, the threads are assigned to handle the clients as they are accepted.
10.There is 1 producer thread running.
11.There are 8 consumer threads running.
12.The producer thread is waiting to accept a client.
13.The consumer threads are waiting for an available item to remove.
14.Connecting a client to the server changes the state of the producer.
15.Connecting to a client changes the state of 1 of the consumer threads.
16.Only 1 consumer thread changes states, and it runs through all of the remove and insert functions.
17.Connecting to a client changes the state of 1 of the consumer threads.
18.The producer thread is now waiting for another client to try connecting to it or for a client to send data over the existing connections.
19.echoservert_pre, echoservert, echoserverp
20.echoservert_pre
21.echoservert
22.echoserverp
*/

int main(void) {
	char *buf;
	char arg1[2048];

	char* prefix = "The query string is: ";
	int length = strlen(prefix);
	/* Extract the argument */
	memcpy(arg1, prefix, length);
	if ((buf = getenv("QUERY_STRING")) != NULL) {
		memcpy(arg1 + length, buf, strlen(buf));
	}
	/* Generate the HTTP response */
	printf("Content-type: text/plain\r\n");
	printf("Content-length: %d\r\n\r\n", (int)strlen(arg1));
	printf("%s", arg1);
	fflush(stdout);
	exit(0);
}