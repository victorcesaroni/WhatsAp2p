#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

int main() {
	printf(KRED"===========================================================================\n"); fflush(stdout);
	printf(KGRN"STARTING ENCRYPTION OF YOUR FILES...\n"); fflush(stdout);
	usleep(2000000);
	printf(KGRN"70%%\n"); fflush(stdout);
	usleep(5000000);
	printf(KGRN"95%%\n"); fflush(stdout);
	usleep(1000000);
	printf(KGRN"100%%\n"); fflush(stdout);
	printf(KCYN"!!! SEND 0.07 BTC TO MEMECRY IN 10H OR YOUR KEY WILL BE DELETED FOREVER !!!\n"); fflush(stdout);
	printf(KCYN"!!! INFORM THIS CODE TO MEMECRY@GMAIL.COM: 0a94000-4f3e42-19b034-2c4de4 !!!\n"); fflush(stdout);
	printf(KRED"===========================================================================\n"); fflush(stdout);
	printf(KWHT"> \n"); fflush(stdout);
	return 0;
}
