#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

enum direction { EAST, WEST };
enum priority { HIGH, LOW };

//In case two trains are going in opposite directions with the same priority,
//this determines which train goes.
//At the start, it is east.
enum direction Turn = EAST;

//Enum to determine what kind of message to output.
enum messageType { READY, ON, OFF };

/*
Struct to represent stations. Also used for readyQueue.
The queue holds the IDs of the trains.
The trains' other info (cross time, etc.) is in threadArray (see below).
lastTrainPtr points to the array space after the last train in the array.
readyTrainPtr points to the array space after the latest loaded train.
When a train is loaded, it increments readyTrainPtr by 1.
For readyQueue, lastTrainPtr points to after the last train that is ready,
and readyTrainPtr points to after the last train that has gone off the track.
*/
typedef struct {
	size_t *queue;
	size_t lastTrainPtr;
	size_t readyTrainPtr;
} station;

/*
Four stations represent the stations used to hold trains.
The fifth station, readyQueue, holds all trains ready for the track.
*/
station eastHigh;
station eastLow;
station westHigh;
station westLow;
station readyQueue;

/*
This struct is used to hold train data (loadTime, crossTime, etc.)
It also holds a pointer to the corresponding train's thread.
It can be accessed by threads and main by using a train's ID.
A train's ID is based on its position in the input file.
*/
typedef struct {
	int loadTime;
	int crossTime;
	enum direction dir;
	enum priority pri;
	pthread_mutex_t trainMutex;
	pthread_cond_t trainCond;
	pthread_t thread;
} threadData;
//Array to hold train info and thread pointers.
threadData *threadArray;
//Starting time.
struct timespec begin;
//Mutex and condition to coordinate start time for threads.
pthread_mutex_t startMutex;
pthread_cond_t startCond;
//Mutexes for trains to adjust station pointers.
pthread_mutex_t eastHighMutex;
pthread_mutex_t eastLowMutex;
pthread_mutex_t westHighMutex;
pthread_mutex_t westLowMutex;
//Mutex and condition for signaling to main that a train is loaded.
pthread_mutex_t readyMutex;
pthread_cond_t readyCond;

void initializeMutexes();
void readFromInput(char *fileName);
void readLineFromInput(FILE *filePtr, char *station, int *loadTime, int *crossTime);
void putTrainIntoStation(char station, int loadTime, int crossTime, size_t trainID);
int getNumOfTrains(FILE *filePtr);
void *trainThread(void *threadArg);
void printMessage(size_t trainID, enum messageType msg);
void printTime();
void stationInit(int numOfTrains);
void queueSort(station Station);
int isEmpty();
int isReady();
size_t compareTrains(size_t a, size_t b);
size_t getBestTrain();
void finalize();

//Initializes the main mutexes used.
void initializeMutexes(){
	pthread_mutex_init(&startMutex, NULL);
	pthread_mutex_lock(&startMutex);
	pthread_cond_init(&startCond, NULL);
	pthread_mutex_init(&eastHighMutex, NULL);
	pthread_mutex_init(&eastLowMutex, NULL);
	pthread_mutex_init(&westHighMutex, NULL);
	pthread_mutex_init(&westLowMutex, NULL);
	pthread_mutex_init(&readyMutex, NULL);
	pthread_cond_init(&readyCond, NULL);
}

//Reads the input file and loads the trains into the program.
void readFromInput(char *fileName){
	FILE *filePtr;
	if((filePtr = fopen(fileName, "r")) == NULL){
		printf("ERROR: %s does not exist.\n",fileName);
		exit(EXIT_FAILURE);
	}

	size_t numOfTrains = getNumOfTrains(filePtr);
	stationInit(numOfTrains);
	threadArray = (void *)malloc(sizeof(threadData) * (numOfTrains+1));

	char station;
	int loadTime;
	int crossTime;
	size_t trainID = 0;
	readLineFromInput(filePtr, &station, &loadTime, &crossTime);
	while(!feof(filePtr)){
		putTrainIntoStation(station, loadTime, crossTime, trainID);
		trainID++; //Assigns a unique ID to each train.
		readLineFromInput(filePtr, &station, &loadTime, &crossTime);
	}
	putTrainIntoStation(station, loadTime, crossTime, trainID);
	fclose(filePtr);
}

/*
Gets the number of trains in the input file.
Source:
https://stackoverflow.com/questions/12733105/c-function-that-counts-lines-in-file
*/
int getNumOfTrains(FILE *filePtr){
	size_t numOfTrains = 0;
	char c;
	while(!feof(filePtr)){
		if(c = fgetc(filePtr) == '\n') numOfTrains++;
	}
	rewind(filePtr);
	return numOfTrains;
}

//Reads a single line from the input file.
void readLineFromInput(FILE *filePtr, char *station, int *loadTime, int *crossTime){
	if(fscanf(filePtr, "%c %d %d\n", station, loadTime, crossTime) != 3){
		puts("ERROR: input file is not properly formatted.");
		exit(EXIT_FAILURE);
	}
}

//Initializes the queues.
void stationInit(int numOfTrains){
        eastHigh.queue = (void *)malloc(sizeof(size_t) * (numOfTrains+1));
        eastLow.queue = (void *)malloc(sizeof(size_t) * (numOfTrains+1));
        westHigh.queue = (void *)malloc(sizeof(size_t) * (numOfTrains+1));
        westLow.queue = (void *)malloc(sizeof(size_t) * (numOfTrains+1));
				readyQueue.queue = (void *)malloc(sizeof(size_t) * (numOfTrains+1));
        eastHigh.lastTrainPtr = 0;
        eastLow.lastTrainPtr = 0;
        westHigh.lastTrainPtr = 0;
				westLow.lastTrainPtr = 0;
				readyQueue.lastTrainPtr = 0;
				eastHigh.readyTrainPtr = 0;
				eastLow.readyTrainPtr = 0;
				westHigh.readyTrainPtr = 0;
				westLow.readyTrainPtr = 0;
				readyQueue.readyTrainPtr = 0;
}

//Initializes a train, its thread, and puts it into a station.
void putTrainIntoStation(char station, int loadTime, int crossTime, size_t trainID){
	threadArray[trainID].loadTime = loadTime;
	threadArray[trainID].crossTime = crossTime;

	switch (station){
		case 'E':
			threadArray[trainID].dir = EAST;
			threadArray[trainID].pri = HIGH;
			eastHigh.queue[eastHigh.lastTrainPtr++] = trainID;
			break;
		case 'e':
			threadArray[trainID].dir = EAST;
			threadArray[trainID].pri = LOW;
			eastLow.queue[eastLow.lastTrainPtr++] = trainID;
			break;
		case 'W':
			threadArray[trainID].dir = WEST;
			threadArray[trainID].pri = HIGH;
			westHigh.queue[westHigh.lastTrainPtr++] = trainID;
			break;
		case 'w':
			threadArray[trainID].dir = WEST;
			threadArray[trainID].pri = LOW;
			westLow.queue[westLow.lastTrainPtr++] = trainID;
			break;
		default:
			puts("ERROR: something went wrong in putTrainIntoStation.");
			break;
	}
	//Threads are kept track of with a pointer in threadData.
	pthread_mutex_init(&(threadArray[trainID].trainMutex), NULL);
	pthread_cond_init(&(threadArray[trainID].trainCond), NULL);
	pthread_t newThread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&newThread, &attr, trainThread, (void *) trainID);
	threadArray[trainID].thread = newThread;
	pthread_attr_destroy(&attr);
}

/*
Sorts the trains in a station by loading times (lowest first).
Uses selection sort.
After a train loads, it increments readyTrainPtr.
*/
void queueSort(station Station){
	size_t i,j;
	size_t *queue = Station.queue;
	int size = Station.lastTrainPtr;
	if(!size) return;
	for(j = 0; j < size-1; j++){
		size_t min = j;
		for(i = j+1; i < size; i++){
			if(threadArray[queue[i]].loadTime < threadArray[queue[min]].loadTime){
				min = i;
			}
		}
		if(min != j){
			size_t temp = queue[j];
			queue[j] = queue[min];
			queue[min] = temp;
		}
	}
}

//Outputs a message to stdout.
void printMessage(size_t trainID, enum messageType msg){
	printTime();

	char *dirString = threadArray[trainID].dir == EAST ? "East" : "West";
	switch (msg){
		case READY:
			printf("Train %2zu is ready to go %4s\n",trainID,dirString);
			break;
		case ON:
			printf("Train %2zu is ON the main track going %4s\n",trainID,dirString);
			break;
		case OFF:
			printf("Train %2zu is OFF the main track after going %4s\n",trainID,dirString);
			break;
		default:
			puts("ERROR: Something went wrong in printMessage.");
			break;
	}
}

/*
Prints the time that an action happens.
Source:
https://users.pja.edu.pl/~jms/qnx/help/watcom/clibref/qnx/clock_gettime.html
*/
void printTime(){
	struct timespec end;
	clock_gettime(CLOCK_MONOTONIC,&end);
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
	printf("%02d:%02d:%04.1lf ", hours, minutes, seconds);
}

/*
Thread used by trains to load.
From the train ID passed, it uses threadArray to get the train's information.
*/
void *trainThread(void *threadArg){
	size_t ID = (size_t) threadArg;
	//Waits for main to give the signal to start loading the train.
	pthread_cond_wait(&startCond,&startMutex);
	pthread_mutex_unlock(&startMutex);
	usleep(threadArray[ID].loadTime*100000);
	printMessage(ID, READY);
	//Adds itself to the queue of trains ready for departure.
	pthread_mutex_lock(&readyMutex);
	readyQueue.queue[readyQueue.lastTrainPtr] = ID;
	readyQueue.lastTrainPtr++;
	pthread_mutex_unlock(&readyMutex);

	/*
	Rather than remiving itself from the station it came from,
	it instead indicates that it is loaded by incrementing readyTrainPtr.
	*/
	if(threadArray[ID].dir == EAST && threadArray[ID].pri == HIGH){
		pthread_mutex_lock(&eastHighMutex);
		eastHigh.readyTrainPtr++;
		pthread_mutex_unlock(&eastHighMutex);
	}
	else if(threadArray[ID].dir == EAST && threadArray[ID].pri == LOW){
		pthread_mutex_lock(&eastLowMutex);
		eastLow.readyTrainPtr++;
		pthread_mutex_unlock(&eastLowMutex);
	}
	else if(threadArray[ID].dir == WEST && threadArray[ID].pri == HIGH){
		pthread_mutex_lock(&westHighMutex);
		westHigh.readyTrainPtr++;
		pthread_mutex_unlock(&westHighMutex);
	}
	if(threadArray[ID].dir == WEST && threadArray[ID].pri == LOW){
		pthread_mutex_lock(&westLowMutex);
		westLow.readyTrainPtr++;
		pthread_mutex_unlock(&westLowMutex);
	}
	//Signals to main that it is ready to cross.
	pthread_cond_signal(&readyCond);
	pthread_cond_wait(&(threadArray[ID].trainCond), &(threadArray[ID].trainMutex));

	usleep(threadArray[ID].crossTime*100000);

	pthread_mutex_destroy(&(threadArray[ID].trainMutex));
	pthread_cond_destroy(&(threadArray[ID].trainCond));
	pthread_exit(NULL);
}

//Returns boolean based on whether or not all of the queues are empty.
int isEmpty(){
	if(eastHigh.lastTrainPtr != eastHigh.readyTrainPtr) return 0;
	if(eastLow.lastTrainPtr != eastLow.readyTrainPtr) return 0;
	if(westHigh.lastTrainPtr != westHigh.readyTrainPtr) return 0;
	if(westLow.lastTrainPtr != westLow.readyTrainPtr) return 0;
	if(readyQueue.lastTrainPtr != readyQueue.readyTrainPtr) return 0;
	return 1;
}

//Returns boolean based on whether or not main can select a train for crossing.
int isReady(){
	if(readyQueue.lastTrainPtr == readyQueue.readyTrainPtr) return 0;
	int loadTime=threadArray[readyQueue.queue[readyQueue.lastTrainPtr-1]].loadTime;
	if(eastHigh.lastTrainPtr != eastHigh.readyTrainPtr && loadTime >= threadArray[eastHigh.queue[eastHigh.readyTrainPtr]].loadTime) return 0;
	if(eastLow.lastTrainPtr != eastLow.readyTrainPtr && loadTime >= threadArray[eastLow.queue[eastLow.readyTrainPtr]].loadTime) return 0;
	if(westHigh.lastTrainPtr != westHigh.readyTrainPtr && loadTime >= threadArray[westHigh.queue[westHigh.readyTrainPtr]].loadTime) return 0;
	if(westLow.lastTrainPtr != westLow.readyTrainPtr && loadTime >= threadArray[westLow.queue[westLow.readyTrainPtr]].loadTime) return 0;
	return 1;
}

//Compares two trains and returns whichever train should go first on the track.
size_t compareTrains(size_t a, size_t b){
	threadData aData = threadArray[a];
	threadData bData = threadArray[b];
	//Comapares station priority
	if(aData.pri == HIGH && bData.pri == LOW){
		return a;
	}
	else if(aData.pri == LOW && bData.pri == HIGH){
		return b;
	}
	//Compares direction and load time.
	if(aData.dir == bData.dir){
		if(aData.loadTime < bData.loadTime){
			return a;
		}
		else if(aData.loadTime > bData.loadTime){
			return b;
		}
		else{
			return a < b ? a : b;
		}
	}
	//If the trains are going in opposite directions, use the turn.
	else{
		return Turn == aData.dir ? a : b;
	}
}

//Releases the structures and utexes from memory.
void finalize(){
	free(eastHigh.queue);
	free(eastLow.queue);
	free(westHigh.queue);
	free(westLow.queue);
	free(readyQueue.queue);
	free(threadArray);
	pthread_mutex_destroy(&startMutex);
	pthread_cond_destroy(&startCond);
	pthread_mutex_destroy(&eastHighMutex);
	pthread_mutex_destroy(&eastLowMutex);
	pthread_mutex_destroy(&westHighMutex);
	pthread_mutex_destroy(&westLowMutex);
	pthread_mutex_destroy(&readyMutex);
	pthread_cond_destroy(&readyCond);
}

// Gets the best train from the trains ready to cross.
size_t getBestTrain(){
	size_t minID = readyQueue.readyTrainPtr;
	size_t i;

	for(i = readyQueue.readyTrainPtr+1; i < readyQueue.lastTrainPtr; i++){
		size_t a = readyQueue.queue[minID];
		size_t b = readyQueue.queue[i];
		size_t c = compareTrains(a,b);
		if(c == b) minID = i;
	}
	if(minID != readyQueue.readyTrainPtr){
		size_t temp = readyQueue.queue[readyQueue.readyTrainPtr];
		readyQueue.queue[readyQueue.readyTrainPtr] = readyQueue.queue[minID];
		readyQueue.queue[minID] = temp;
	}
	readyQueue.readyTrainPtr++;
	return readyQueue.queue[readyQueue.readyTrainPtr-1];
}

int main(int argc, char *argv[]){
	if(argc != 2){
		puts("ERROR: this program needs one input file.");
		exit(EXIT_FAILURE);
	}
	initializeMutexes();
	readFromInput(argv[1]);
	//Sort trains in the queue by load time.
	queueSort(eastHigh);
	queueSort(eastLow);
  queueSort(westHigh);
  queueSort(westLow);

	//Wait a second to make sure all trains have time to prepare for loading.
	usleep(1000000);
	//Signal to all trains that they can start loading.
	pthread_cond_broadcast(&startCond);
	//Get the time that all trains started loading.
	clock_gettime(CLOCK_MONOTONIC,&begin);

	pthread_mutex_lock(&readyMutex);
	//Continue until all trains are done.
	while(!isEmpty()){
		//Wait until all trains are done loading.
		while(!isReady()){
			pthread_cond_wait(&readyCond,&readyMutex);
		}
		size_t ID = getBestTrain();

		pthread_mutex_unlock(&readyMutex);
		printMessage(ID, ON);
		//Cross the train.
		pthread_cond_signal(&(threadArray[ID].trainCond));
		pthread_join(threadArray[ID].thread, NULL);
		if(threadArray[ID].dir == EAST){
			Turn = WEST;
		}
		else{
			Turn = EAST;
		}
		printMessage(ID, OFF);
	}

	finalize();
}
