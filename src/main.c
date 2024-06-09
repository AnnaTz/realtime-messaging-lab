
//header files
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>
#include <pthread.h>

#define PORT 2288 //port of communication
#define nAEMs 12 //number of possible AEMs


uint32_t peerAEM[nAEMs] = {8929,8997,8880,8861,8920,8923,8989,8934,8945,8921,8962,8943}; //list of all possible AEMs
char** history; //double array of message history
char** aemIPs; //double array of the IP addresses that correspond to each possible AEM
int history_idx = 0; //index of message history
int end_flag; //flag for the code's termination
uint32_t info[6][nAEMs]; //matrix containing useful info about the peer devices
						  //row 0: peer devices' AEM
						  //row 1: index pointing to the last stored message that was sent to each device
						  //row 2: number of connections that were established with this device
						  //row 3: total duration of the communication with this device
						  //row 4: total number of messages sent to this device
						  //row 5: total number of messages received from this device
FILE* recFile; //file for received messages' info
FILE* sendFile; //file for sent messages' info
FILE* randomMesFile; //file for randomly generated messages' info
FILE* forMeFile; //file for the messages that were destined for my device
FILE* statsFile; //file for the statistical results




int IPtoAEM(char* ip) //function to convert an IP address to the corresponding AEM
{
	  char *s = (char*) malloc(sizeof(char)*5);
	  char *s1 = (char*) malloc(sizeof(char)*4);
	  char *s2 = (char*) malloc(sizeof(char)*2);
	  strncpy(s, ip+5, 6); //keep the [xx].[yy] part of 10.0.[xx].[yy]
	  strncpy(s1, s, 2);  //keep the [xx] part of [xx].[yy]
	  strncpy(s2, s+3, 3); //keep the [yy] part of [xx].[yy]
	  strcat(s1,s2); //concatenate to make the [xxyy] string
	  int aem = atoi(s1); //convert AEM from string to integer
	  return aem;
}

void makeIPs() //function to generate the IP addresses that the server will attempt to communicate with
{

	char* newIP = (char *) malloc(sizeof(char) * 12); //array for an IP address
	uint32_t connAEM; //variable of the connected device's AEM


	uint32_t x,y; //variables to store the first and last two digits of the AEM
	uint32_t len; //variable used to split the AEM in two parts
	for(int i=0; i<nAEMs; i++) //for each possible AEM
	{
		connAEM=peerAEM[i]; //get the AEM to be split
		len=floor(log10(abs(connAEM))) + 1;
		x = connAEM / pow(10, len / 2); //first half
		y = connAEM - x * pow(10, len / 2); //second half
								//10.0.[xx].[yy] ; [xx] -> first 2 AEM digits and [yy] -> last 2

		if(x < 10) //print IP address into array, in the correct form
			sprintf(newIP, "10.0.0%"PRIu32".%"PRIu32, x, y);
		if(y < 10)
			sprintf(newIP, "10.0.%"PRIu32".0%"PRIu32, x, y);
		else
			sprintf(newIP, "10.0.%"PRIu32".%"PRIu32, x, y);

		strcpy(aemIPs[i], newIP); //store the formated IP address

	}
	return;

}

void write_file(FILE* file,char* data) //function to save the code's results in files
{
	if (file){ //if the data file has been opened successfully

		fwrite(data, strlen(data), 1, file); //write data buffer to the file
		fflush(file);
	}
	return;
}

int connect_server(char* ip, char* message) //function to establish server's connection
{
	int sockid; //socket descriptor
	struct sockaddr_in srvAddPort; //socket address structure for server

	memset(&srvAddPort, 0, sizeof(srvAddPort)); //initialize socket address structure
	srvAddPort.sin_family= AF_INET; //assign internet family of IPv4
	srvAddPort.sin_port= htons(PORT); //assign port number
	srvAddPort.sin_addr.s_addr = inet_addr(ip); //convert IP address

	if ((sockid = socket (PF_INET, SOCK_STREAM, 0)) == -1) //create server's socket
		return 1; //catch error

	if (connect(sockid, (struct sockaddr*)&srvAddPort, sizeof(srvAddPort)) == -1) //connect socket to the server's address
	{
		close (sockid); //catch error
		return 1;
	}

	if (send(sockid, message, strlen(message), MSG_CONFIRM) == -1) //send the message
	{
		close(sockid); //catch error
		return 1;
	}


	close(sockid); //close socket
	return 0;
}

void server (char* ip) //server (sender) function
{

	sleep (60 + (rand ()%240)); //sleep for 1-5 minutes


	char sendBuff[2000]; //buffer of data to be written in the sendFile

	char randBuff[2000]; //buffer of the random message to be generated and sent by the server


	int prev_idx = 0; //index of the last stored message that has already been sent to the connected device


	uint32_t myAEM = 8918; //variable to store my AEM

	int nom = 0; //initialize the number of sent messages


	struct timeval tv; //time structure

	gettimeofday(&tv, NULL); //get current time
	uint64_t start = tv.tv_usec + tv.tv_sec*1000000; //save it in microseconds; it's the start of the server's communication

	uint64_t tcur; //variable of the updating current time

	uint64_t tdur; //variable of the duration of the communication


	for(int x=0; x<sizeof(peerAEM)/sizeof(uint32_t); x++)
	{
		if(info[0][x] == IPtoAEM(ip)) //find the AEM of the connected device in the info matrix
		{
			prev_idx = info[1][x]; //get the index of the last stored message that has been sent to this device
			break;
		}
	}


	if(prev_idx > history_idx) //if the last sent message index is greater that the current message history index,
							   //then the circular array has begun overwriting from the start
	{
		for(int j=prev_idx+1; j < 2000; j++) //from right after the last sent message until the end of the message history
		{

			char *firstAEM = (char*) malloc(sizeof(char)*5); //variable for the first AEM (source) of the stored message

			strncpy(firstAEM, history[j], 4); //keep the first AEM
			char buf[5]; //buffer for the AEM of the destination device
			sprintf(buf, "%d", IPtoAEM(ip)); //get the destination AEM
			if(!strcmp(firstAEM, buf)) //if the source AEM is equal to the destination
			{
				continue; //don't send the message, keep going with the next one
			}

			if(connect_server(ip, history[j]) == 1) //connect and send
				return; //catch error

			if(nom==0) //if it's the first of the current messages to be sent to this device
			{
				snprintf(sendBuff, sizeof(sendBuff), "destination IP: %s\n", ip); //print destination IP into the data buffer
				write_file(sendFile, sendBuff); //write buffer to the file
			}

			tcur = time(NULL); //get current time

			snprintf(sendBuff, sizeof(sendBuff), "time: %" PRIu64 "   ---   message: %s\n", tcur, history[j]); //print time and message into the data buffer
			write_file(sendFile, sendBuff); //write buffer to the file


			nom++; //increment number of sent messages
		}
		for(int j=0; j < history_idx; j++) //from the start of the message history until the last stored message
		{
			char *firstAEM = (char*) malloc(sizeof(char)*5); //variable for the first AEM (source) of the store message

			strncpy(firstAEM, history[j], 4); //keep the first AEM
			char buf[5]; //buffer for the AEM of the destination device
			sprintf(buf, "%d", IPtoAEM(ip)); //get the destination AEM
			if(!strcmp(firstAEM, buf)) //if the source AEM is equal to the destination
			{
				continue; //don't send the message, keep going with the next one
			}


			if(connect_server(ip, history[j]) == 1) //connect and send
				return; //catch error

			tcur = time(NULL); //get current time


			snprintf(sendBuff, sizeof(sendBuff), "time: %" PRIu64 "   ---   message: %s\n", tcur, history[j]); //print time and message into the data buffer

			write_file(sendFile, sendBuff); //write buffer to the file

			nom++; //increment number of sent messages
		}
	}
	else //else, the circular buffer has not begun to overwrite entries yet
	{
		for(int j=prev_idx+1; j < history_idx; j++) //from right after the last sent message until the last stored message
		{
			char *firstAEM = (char*) malloc(sizeof(char)*5); //variable for the first AEM (source) of the store message

			strncpy(firstAEM, history[j], 4); //keep the first AEM
			char buf[5]; //buffer for the AEM of the destination device
			sprintf(buf, "%d", IPtoAEM(ip)); //get the destination AEM
			if(!strcmp(firstAEM, buf)) //if the source AEM is equal to the destination
			{
				continue; //don't send the message, keep going with the next one
			}


			if(connect_server(ip, history[j]) == 1) //connect and send
				return; //catch error

			if(nom==0) //if it's the first of the current messages to be sent to this device
			{
				snprintf(sendBuff, sizeof(sendBuff), "destination IP: %s\n", ip); //print destination IP into the data buffer
				write_file(sendFile, sendBuff); //write buffer to the file
			}

			tcur = time(NULL); //get current time

			snprintf(sendBuff, sizeof(sendBuff), "time: %" PRIu64 "   ---   message: %s\n", tcur, history[j]); //print time and message into the data buffer

			write_file(sendFile, sendBuff); //write buffer to the file


			nom++; //increment number of sent messages
		}
	}

	for(int x=0; x<sizeof(peerAEM)/sizeof(uint32_t); x++)
	{
		if(info[0][x] == IPtoAEM(ip)) //find the AEM of the connected device in the info matrix
		{
			info[1][x] = history_idx; //save the current history index as the new last sent message index of the connected device
			break;
		}
	}



	int rand_idx=(rand() % nAEMs); //select random index value within the AEM list

	char text[256] = "Hi there my friend"; //array of the text to send

	tcur = time(NULL); //get current time

	snprintf(randBuff, sizeof(randBuff), "%" PRIu32 "_%" PRIu32 "_%" PRIu64 "_%s", myAEM, peerAEM[rand_idx], tcur, text); //print message into the buffer to be sent

	if(connect_server(ip, randBuff) == 1) //connect and send
		return; //catch error

	write_file(randomMesFile, randBuff); //write buffer to the file
	write_file(randomMesFile, "\n"); //change line to print properly


	snprintf(sendBuff, sizeof(sendBuff), "time: %" PRIu64 "   ---   message: %s\n", tcur, randBuff); //print time and message into the data buffer

	write_file(sendFile, sendBuff); //write buffer to the file

	nom++; //increment number of sent messages

	if(nom != 0) //if there has been a connection
	{

		gettimeofday(&tv, NULL); //get current time
		tcur = tv.tv_usec + tv.tv_sec*1000000; //save it in microseconds

		tdur = tcur -start; //compute the duration of the communication

		snprintf(sendBuff, sizeof(sendBuff), "number of  messages sent: %d\nduration: %" PRIu64 "\n\n\n", nom, tdur); //print number of sent messages and duration into the data buffer

		write_file(sendFile, sendBuff); //write buffer to the file

		for(int x=0; x<sizeof(peerAEM)/sizeof(uint32_t); x++)
		{
			if(info[0][x] == IPtoAEM(ip)) //find the AEM of the connected device in the info matrix
			{
				info[2][x] = info[2][x] + 1; //increment the number of communications established with this device
				info[3][x] = info[3][x] + tdur; //add the duration of the finished communication
				info[4][x] = info[4][x] + nom; //add the number of the newly sent messages
				break;
			}
		}
	}

	return ;
}

void listener() //listener (receiver) function
{
	int sockid; //socket descriptor
	struct sockaddr_in srvAddPort; //socket address structure for server
	struct sockaddr_in clntAddr; //socket address structure for listener
	char buffer[256]; //buffer for the message to be received from server

	uint64_t tcur; //variable of the updating current time

	char recBuff[2000]; //buffer of data to be written in the recfile


	memset(&srvAddPort, 0, sizeof(srvAddPort)); //initialize socket address structure
	srvAddPort.sin_family= AF_INET; //assign internet family of IPv4
	srvAddPort.sin_port= htons(PORT); //assign port number
	srvAddPort.sin_addr.s_addr = htonl(INADDR_ANY); //assign in_addr structure that contains an ipv4 address

	if ((sockid = socket (PF_INET, SOCK_STREAM, 0)) == -1) //create socket
		return; //catch error

	if(bind(sockid, (struct sockaddr *) &srvAddPort, sizeof(srvAddPort)) == -1) //assign server's details to socket
	{
		close (sockid); //catch error
		return;
	}
	if (listen (sockid, 2000) == -1) //mark socket as passive, with 2000 maximum number of incoming connections
	{
		close (sockid); //catch error
		return;
	}
	while (!end_flag) //while the 2 hours have not passed yet
	{
		size_t clntLen= sizeof(clntAddr); //length of client's address
		int clntSock; //accepted socket descriptor
		int rcvBytes; //number of received bytes

		if ((clntSock = accept (sockid, (struct sockaddr *)&clntAddr, (socklen_t*)&clntLen)) == -1) //accept incoming connection with client
		{
			close (sockid); //catch error
			continue;
		}

		if ((rcvBytes = recv(clntSock, buffer, sizeof(buffer), 0)) < 0) //receive incoming bytes
		{
			close (sockid); //catch error
			continue;
		}
		close(clntSock); //close socket
		buffer[rcvBytes] = '\0'; //terminate the received data buffer


		printf("%s\n",buffer); //print the received message


		int flag=0; //flag to indicate if the message should be saved


		char *secAEM = (char*) malloc(sizeof(char)*5); //variable for the second AEM (destination) of the received message
		strncpy(secAEM, buffer+5, 4); //get the second AEM
		if(!strcmp(secAEM, "8918")) //if the destination AEM is equal to mine
		{
			snprintf(recBuff, sizeof(recBuff), "%s\n", buffer); //attach "\n" to the message buffer so that it's printed properly
			write_file(forMeFile,recBuff); //write buffer to the file
			flag = 1; //the message is destined for my device, so it shouldn't get stored and sent to others
		}

		if(!flag) //if the message wasn't destined for me, go on and check for duplicate entries
		{
			for(int i = 0; i < history_idx; i++)
			{

				if(!strcmp(history[i],buffer)) //compare received message to every message already stored in history
				{
					flag=1; //duplicate entry found
					break;
				}
			}
		}


		if(flag==0) //if the message wasn't destined for me and there aren't any duplicate entries, then save it
		{
			strcpy(history[history_idx],buffer); //store the received message to the message history
			history_idx++; //increment message history index
			if(history_idx == 2000) //if maximum number of stored messages has been reached
				history_idx = 0; //reset message history index to the start, so that the storage is circular
		}


		tcur = time(NULL); //get current time


		char ipAddress[10]; //string variable for the IP address of the connected device

		inet_ntop(AF_INET, &(clntAddr.sin_addr), ipAddress, 12); //get the connected IP address



		snprintf(recBuff, sizeof(recBuff), "source IP: %s\ntime: %" PRIu64 "\nmessage: %s\n\n",ipAddress, tcur, buffer); //print source IP address, time and message into the data buffer

		write_file(recFile, recBuff); //write buffer to the file


		for(int x=0; x<sizeof(peerAEM)/sizeof(uint32_t); x++)
		{
			if(info[0][x] == IPtoAEM(ipAddress)) //find the AEM of the connected device in the info matrix
			{
				info[5][x] = info[5][x] + 1; //increment the number of the newly received messages
				break;
			}
		}

	}

	close(sockid); //close socket
	return;
}

void* thListener(void* ptr) //listener's thread
{
   listener(); //listener function
   return NULL;
}

int main(int argc, char const *argv[]) //main function
{
	printf("\nStarting\n");

	recFile = fopen("recFile.txt","w"); //open the files
	sendFile = fopen("sendFile.txt","w");
	randomMesFile = fopen("randomMesFile.txt","w");
	forMeFile = fopen("forMeFile.txt","w");
	statsFile = fopen("statsFile.txt","w");

	history = malloc(2000*sizeof(char*)); //allocate space for the message history array
	for(int i=0;i<2000;i++)  //2000 is the maximum number of messages that can be stored
		history[i]=malloc(275*sizeof(char));  //each message can be a maximum of 275 characters

	aemIPs = malloc(nAEMs*sizeof(char*)); //allocate space for the IP addresses' array
	for(int i=0;i<nAEMs;i++)
		aemIPs[i]=malloc(12*sizeof(char));


	for(int l=0; l<sizeof(peerAEM)/sizeof(uint32_t); l++) //initialize the info matrix
	{
		info[0][l]=peerAEM[l];
		info[1][l]=-1;
		info[2][l]=0;
		info[3][l]=0;
		info[4][l]=0;
		info[5][l]=0;
	}

	pthread_t tL;
	pthread_create( &tL, NULL, thListener, NULL); //create listener's thread

	makeIPs(); //generate and store possible IP addresses

	srand(time(NULL)); //set the seed of the random number generator


	end_flag = 0; //initialize the code's termination flag
	int64_t tStart = time(NULL); //save the code's starting time
	int64_t tDur = 0; //variable for the elapsed running time
	int64_t tNew = 0; //variable for updating the code's current time

	char* ipBuff = (char *) malloc(sizeof(char) * 12); //buffer to contain an IP address

	int ip_idx = 0; //index for the IP addresses' array

	while(!end_flag) //while the 2 hours have not passed
	{

		strcpy(ipBuff,aemIPs[ip_idx]); //get the IP address that the server will attempt to communicate with

		ip_idx++; //increment array index to the next position
		if(ip_idx==nAEMs) //if index has reached the end
			ip_idx = 0; //reset index to the start of the array

		server(ipBuff); //call server function for this IP address


		if(tDur <7200) //if the 2 hours have not passed yet
		{
			tNew = time(NULL); //update current time
			tDur = tNew - tStart; //update elapsed time
		}
		else
			end_flag = 1; //set the termination flag

	}

	int total_occur = 0; //variable of the total number of times that a connection was established with any device
	uint32_t total_dur = 0; //variable of the duration of communication with all devices totally
	int total_nom_sent = 0; //variable of the number of messages send to all devices totally
	int total_nom_rec = 0; //variable of the number of messages received from all devices totally

	char statsFileBuff[2000]; //buffer of data to be written in the statistic's file


	for(int x=0; x<sizeof(peerAEM)/sizeof(uint32_t); x++) //for every AEM
	{

		snprintf(statsFileBuff, sizeof(statsFileBuff), "AEM: %"PRIu32"\ntotal connection occurrences: %d\ntotal duration: %"PRIu32"\n", peerAEM[x], info[2][x], info[3][x]); //print total connection occurrences and total duration into the data buffer

		write_file(statsFile, statsFileBuff); //write buffer to the file


		snprintf(statsFileBuff, sizeof(statsFileBuff), "total number of messages sent to: %d\ntotal number of messages received from: %d\n\n", info[4][x], info[5][x]); //print total number of sent and received messages into the data buffer

		write_file(statsFile, statsFileBuff); //write buffer to the file


		total_occur += info[2][x]; //add to total number of established communications with all devices
		total_dur += info[3][x]; //add to total duration of communication with all devices
		total_nom_sent += info[4][x]; //add to total number of sent messages to all devices
		total_nom_rec += info[5][x]; //add to total number of received messages from all devices

	}

	float av1, av2; //variables to calculate average statistical values

	av1 = total_dur/total_occur; //average duration per session

	snprintf(statsFileBuff, sizeof(statsFileBuff), "\n\ntotal number of sessions: %d\naverage duration per session: %0.3f\n", total_occur, av1); //print total number of sessions and average duration into the data buffer

	write_file(statsFile, statsFileBuff); //write buffer to the file

	av1 = total_nom_sent/total_occur; //average number of messages sent per session
	av2 = total_nom_rec/total_occur; //average number of messages received per session

	snprintf(statsFileBuff, sizeof(statsFileBuff), "average number of messages sent per session: %0.3f\naverage number of messages received per session: %0.3f\n\n", av1, av2); //print average number of sent and received messages into the data buffer

	write_file(statsFile, statsFileBuff); //write buffer to the file



	printf("\nEnding\n");

	pthread_join( tL, NULL); //wait for the listener's thread to exit

	fclose(recFile); //close the files
	fclose(sendFile);
	fclose(randomMesFile);
	fclose(statsFile);
	fclose(forMeFile);

	free(history); //free dynamically allocated arrays
	free(aemIPs);

	return EXIT_SUCCESS; //exit code successfully
}
