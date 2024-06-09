#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

volatile sig_atomic_t keep_going=1; //flag to stimulate the end of the sampling process

static void handler(int signum) //function to handle the alarm signal
{
    keep_going=0; //update flag
}

void rtesTimer(float t,float dt)
{
	int size=t/dt;
	long int i=0; //counting samples
	struct timeval *timestamps=(struct timeval*)malloc(sizeof(struct timeval)*size); //array to store the time that each sample was received
	signal(SIGALRM,handler); //assign handler to SIGALRM
	alarm(t); //signal when t is over
	struct timeval tv; //time struct to store gettimefoday outputs
	FILE *fptr; //pointer to results' text file
	double diff=dt*1000000; //variable to store time distance between samples
	double *diffs=(double*)malloc(sizeof(double)*(size-1)); //array to store the time difference between samples

	fptr=fopen("differences.txt","w"); //text file that stores the time differences between samples

	while(keep_going) //as long as alarm didn't signal yet
	{
		gettimeofday(&tv, NULL); //get current time struct
		timestamps[i]=tv; //save the new sample

		if(i>0) //if there are 2 or more samples stored
		{
			diff=(timestamps[i].tv_sec-timestamps[i-1].tv_sec)*1000000+(timestamps[i].tv_usec-timestamps[i-1].tv_usec);
			 //time difference between current sample and the previous one
			//printf("%f\n",diff);
			diffs[i-1]=diff; //store the time difference
		}

		i++; //one more sample has been received

		usleep(dt*1000000); //next sample to be received after dt*1000000 microseconds
	}

	for(long int j=0;j<i-1;j++){
		fprintf(fptr,"%lf\n",diffs[j]); //print results to differences.txt
	}
	printf("%ld\n",i); //print the total number of samples that were received during t
	free(timestamps); //free allocated memory
	free(diffs); //...
	fclose(fptr); //close text file
}

int main()
{
	int t=7200; //total time window of sampling
	float dt=0.1; //time distance between samples
	rtesTimer(t,dt); //function of sampling process
	return 0; //successfully end program
}

