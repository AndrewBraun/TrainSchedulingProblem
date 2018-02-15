#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

//Global enumeration that says what direction a train is going or which turn it is.
enum direction { EAST, WEST };

//Enumeration to hold train priority.
enum priority { HIGH, LOW };

//Enum to determine what kind of message to output.
enum messageType { READY, ON, OFF }; 

//In case two trains are going in opposite directions with the same priority,
//this determines which train goes.
//By default, it is east.
enum direction Turn = EAST;

//Struct to represent stations.
typedef struct {
	size_t *queue;
	size_t lastTrainPtr;
	size_t readyTrainPtr;
} station;

station eastboundHigh;
station eastboundLow;
station westboundHigh;
station westboundLow;

//Struct to pass arguments to train threads, as well as a pointer to the thread itself.
typedef struct {
	int loadTime;
	int crossTime;
	enum direction dir;
	enum priority pri;
	pthread_t *thread;
} threadData;

//Array to hold threads and their data.
threadData *threadArray;

//Struct for keeping track of the time that every thread started loading.
struct timespec begin;

//Mutex and condition to coordinate start time for threads.
pthread_mutex_t startMutex;
pthread_cond_t startCond;

void readFromInput(char *fileName);
void readLineFromInput(FILE *filePtr, char *station, int *loadTime, int *crossTime);
void putTrainIntoStation(char station, int loadTime, int crossTime, size_t trainID);
size_t getNumOfTrains(FILE *filePtr);
void *trainThread(void *threadArg);
void printMessage(size_t trainID, enum messageType msg);
void stationInit(size_t numOfTrains);

//Reads the input file and loads the trains into the program.
//Exits the program if the file does not exist or if the input is invalid.
void readFromInput(char *fileName){

	FILE *filePtr;
	if((filePtr = fopen(fileName, "r")) == NULL){
		printf("ERROR: %s does not exist.\n",fileName);
		exit(EXIT_FAILURE);
	}
	//The number of trains in the input file.
	size_t numOfTrains = getNumOfTrains(filePtr);
	printf("Number of trains: %zu\n", numOfTrains);

	//Initializes the stations and thread arguments holder.	
	stationInit(numOfTrains);
	threadArray = (void *)malloc(sizeof(threadData) * (numOfTrains+1));

	char station; //The char representing which station a train will go to.
	int loadTime; //The loading time of a train.
	int crossTime; //The crossing time of a train.
	size_t trainID = 0; //The ID of the train. Low number means higher priority.

	readLineFromInput(filePtr, &station, &loadTime, &crossTime);
	while(!feof(filePtr)){
		putTrainIntoStation(station, loadTime, crossTime, trainID);
		trainID++;
		readLineFromInput(filePtr, &station, &loadTime, &crossTime);
	}
	putTrainIntoStation(station, loadTime, crossTime, trainID);

	fclose(filePtr);
}

//Gets the number of trains in the input file.
//Used for allocating enough space for the stations.
//Source: https://stackoverflow.com/questions/12733105/c-function-that-counts-lines-in-file
size_t getNumOfTrains(FILE *filePtr){
	size_t numOfTrains = 0;
	char c;
	while(!feof(filePtr)){
		if(c = fgetc(filePtr) == '\n') numOfTrains++;
	}
	rewind(filePtr);
	return numOfTrains;
}

//Reads a single line from the input file.
//Exits the program if the file is not properly formatted.
void readLineFromInput(FILE *filePtr, char *station, int *loadTime, int *crossTime){
	if(fscanf(filePtr, "%c %d %d\n", station, loadTime, crossTime) != 3){
		puts("ERROR: input file is not properly formatted.");
		exit(EXIT_FAILURE);
	}	
}

//Initializes the queues.
void stationInit(size_t numOfTrains){
        eastboundHigh.queue = (void *)malloc(sizeof(size_t) * (numOfTrains+1));
        eastboundLow.queue = (void *)malloc(sizeof(size_t) * (numOfTrains+1));
        westboundHigh.queue = (void *)malloc(sizeof(size_t) * (numOfTrains+1));
        westboundLow.queue = (void *)malloc(sizeof(size_t) * (numOfTrains+1));
        eastboundHigh.lastTrainPtr = 0;
        eastboundLow.lastTrainPtr = 0;
        westboundHigh.lastTrainPtr = 0;
        westboundLow.lastTrainPtr = 0;
	eastboundHigh.readyTrainPtr = 0;
	eastboundLow.readyTrainPtr = 0;
	westboundHigh.readyTrainPtr = 0;
	westboundLow.readyTrainPtr = 0;
}

void putTrainIntoStation(char station, int loadTime, int crossTime, size_t trainID){
	printf("Train %zu from station %c has load time %d and cross time %d.\n",trainID,station,loadTime,crossTime);
	threadArray[trainID].loadTime = loadTime;
	threadArray[trainID].crossTime = crossTime;
	switch (station){
		case 'E':
			threadArray[trainID].dir = EAST;
			threadArray[trainID].pri = HIGH;
			eastboundHigh.queue[eastboundHigh.lastTrainPtr++] = trainID;
			break;
		case 'e':
			threadArray[trainID].dir = EAST;
			threadArray[trainID].pri = LOW;
			eastboundLow.queue[eastboundLow.lastTrainPtr++] = trainID;
			break;
		case 'W':
			threadArray[trainID].dir = WEST;
			threadArray[trainID].pri = HIGH;
			westboundHigh.queue[westboundHigh.lastTrainPtr++] = trainID;
			break;
		case 'w':
			threadArray[trainID].dir = WEST;
			threadArray[trainID].pri = LOW;
			westboundLow.queue[westboundLow.lastTrainPtr++] = trainID;
			break;
		default:
			puts("ERROR: something went wrong in putTrainIntoStation.");
			break;
	}
	pthread_t newThread;
	threadArray->thread = &newThread;
	pthread_create(&newThread, NULL, trainThread, (void *) trainID);
}

//Outputs a message to stdout.
void printMessage(size_t trainID, enum messageType msg){
	struct timespec end;
	clock_gettime(CLOCK_MONOTONIC,&end);
	//Calculate total time.
	//Source: https://users.pja.edu.pl/~jms/qnx/help/watcom/clibref/qnx/clock_gettime.html
	double seconds = (end.tv_sec - begin.tv_sec) + (double)(end.tv_nsec - begin.tv_nsec)/1000000000;
	int hours = 0, minutes = 0;
	while(seconds >= 60){
		minutes++;
		seconds -= 60;
	}
	while(minutes >= 60){
		hours++;
		minutes -= 60;
	}

	char *dirString = threadArray[trainID].dir == EAST ? "East" : "West";
	switch (msg){
		case READY:
			printf("%02d:%02d:%04.1lf Train %2zu is ready to go %4s\n",hours,minutes,seconds,trainID,dirString);
			break;
		case ON:
			printf("%02d:%02d:%04.1lf Train %2zu is ON the main track going %4s\n",hours,minutes,seconds,trainID,dirString);
			break;
		case OFF:
			printf("%02d:%02d:%04.1lf Train %2zu is OFF the main track after going %4s\n",hours,minutes,seconds,trainID,dirString);
			break;
		default:
			puts("ERROR: Something went wrong in printMessage.");
			break;
	}
}

//Thread used by trains to load and cross.
void *trainThread(void *threadArg){
	size_t ID = (size_t) threadArg;
	pthread_cond_wait(&startCond,&startMutex);
	pthread_mutex_unlock(&startMutex);
	usleep(threadArray[ID].loadTime*100000);
	printMessage(ID, READY);
	pthread_exit(NULL);
}

int main(int argc, char *argv[]){

	//Checks to see if the user passed the right amount of arguments.
	if(argc != 2){
		puts("ERROR: this program needs one input file.");
		exit(EXIT_FAILURE);
	}
	
	pthread_mutex_init(&startMutex, NULL);
	pthread_mutex_lock(&startMutex);
	pthread_cond_init(&startCond, NULL);

	readFromInput(argv[1]);

	usleep(1000000);
	pthread_cond_broadcast(&startCond);
	clock_gettime(CLOCK_MONOTONIC,&begin);

	usleep(50000000);
	free(eastboundHigh.queue);
	free(eastboundLow.queue);
	free(westboundHigh.queue);
	free(westboundLow.queue);
	free(threadArray);

	pthread_mutex_destroy(&startMutex);
	pthread_cond_destroy(&startCond);
}
