#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

//Enum that says what direction a train is going.
//Also used for which turn it is on the track.
enum direction { EAST, WEST };

//In case two trains are going in opposite directions with the same priority,
//this determines which train goes.
//At the start, it is east.
enum direction Turn = EAST;

//Enum to hold train priority.
enum priority { HIGH, LOW };

//Enum to determine what kind of message to output.
//Used by printMessage() to select the type of output.
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
	pthread_t *thread;
} threadData;

//Array to hold train info and thread pointers.
threadData *threadArray;

/*
This keeps track of the time when all threads started execution.
printMessage uses this to calculate the time it prints a message.
*/
struct timespec begin;

/*
Mutex and condition to coordinate start time for threads.
Every thread detects the condition signal at the same time,
thereby starting loading at the same time.
*/
pthread_mutex_t startMutex;
pthread_cond_t startCond;

/*
Mutexes for trains to adjust station pointers.
Whenever a train needs to modify readyTrainPtr, it locks the variable.
*/
pthread_mutex_t eastHighMutex;
pthread_mutex_t eastLowMutex;
pthread_mutex_t westHighMutex;
pthread_mutex_t westLowMutex;

/*
Mutex and condition for signaling to main that a train is loaded.
When a train is ready to go on the track, it sends this to main.
*/
pthread_mutex_t readyMutex;
pthread_cond_t readyCond;

//Used functions (besides main).
void readFromInput(char *fileName);
void readLineFromInput(FILE *filePtr, char *station, int *loadTime, int *crossTime);
void putTrainIntoStation(char station, int loadTime, int crossTime, size_t trainID);
size_t getNumOfTrains(FILE *filePtr);
void *trainThread(void *threadArg);
void printMessage(size_t trainID, enum messageType msg);
void printTime();
void stationInit(size_t numOfTrains);
void queueSort(station Station);
int isEmpty();
size_t bestTrain(size_t a, size_t b);

/*
Reads the input file and loads the trains into the program.
Exits the program if the file does not exist or if the input is invalid.
*/
void readFromInput(char *fileName){

	FILE *filePtr;
	if((filePtr = fopen(fileName, "r")) == NULL){
		printf("ERROR: %s does not exist.\n",fileName);
		exit(EXIT_FAILURE);
	}
	/*
	The number of trains in the input file.
	Used to make sure that the arrays are big enough.
	*/
	size_t numOfTrains = getNumOfTrains(filePtr);
	/*
	Initializes the stations and threadArray.
	Uses the number of trains to make sure that they're big enough.
	*/
	stationInit(numOfTrains);
	threadArray = (void *)malloc(sizeof(threadData) * (numOfTrains+1));

	char station; //The char representing which station a train will go to.
	int loadTime; //The loading time of a train.
	int crossTime; //The crossing time of a train.
	size_t trainID = 0; //The ID of the train.

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
Used for allocating enough space for the stations.
Source:
https://stackoverflow.com/questions/12733105/c-function-that-counts-lines-in-file
*/
size_t getNumOfTrains(FILE *filePtr){
	size_t numOfTrains = 0;
	char c;
	while(!feof(filePtr)){
		if(c = fgetc(filePtr) == '\n') numOfTrains++;
	}
	rewind(filePtr);
	return numOfTrains;
}

/*
Reads a single line from the input file.
Exits the program if the file is not properly formatted.
*/
void readLineFromInput(FILE *filePtr, char *station, int *loadTime, int *crossTime){
	if(fscanf(filePtr, "%c %d %d\n", station, loadTime, crossTime) != 3){
		puts("ERROR: input file is not properly formatted.");
		exit(EXIT_FAILURE);
	}
}

//Initializes the queues.
void stationInit(size_t numOfTrains){
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
	//Selects which station to put the train in.
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
	pthread_t newThread;
	threadArray->thread = &newThread;
	pthread_create(&newThread, NULL, trainThread, (void *) trainID);
}

/*
Sorts the trains in a station by loading times (lowest first).
Uses selection sort.
Useful for keeping track of which trains are loaded
in a station easily.

After a train loads,
it just needs to increment
readyTrainPtr
*/
void queueSort(station Station){
	size_t i,j;
	size_t *queue = Station.queue;
	size_t size = Station.lastTrainPtr;
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
/*
Outputs a message to stdout.
Calling thread selects which type of message it wants.
READY: Train <ID> is ready to go <direction>.
ON: Train <ID> is ON the main track going <direction>.
OFF: Train <ID> is OFF the main track after going <direction>.
*/
void printMessage(size_t trainID, enum messageType msg){

	//Prints the time.
	printTime();
	//Direction that the train is going.
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
This first gets the time that this function was called.
It then findws the difference between it and
the time all trains started loading.
This gives us the total time that has passed.
Source: https://users.pja.edu.pl/~jms/qnx/help/watcom/clibref/qnx/clock_gettime.html
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
It is passed the train's ID.
From the ID, it uses threadArray to get the train's information.
Note that crossing is actually handled by main.
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
	readyQueue.queue[readyQueue.lastTrainPtr++] = ID;
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
	pthread_exit(NULL);
}

/*
Returns boolean based on whether or not all of the queues are empty.
Useful for checking whether or not main should exit the program.
*/
int isEmpty(){
	if(eastHigh.lastTrainPtr != eastHigh.readyTrainPtr) return 0;
	if(eastLow.lastTrainPtr != eastLow.readyTrainPtr) return 0;
	if(westHigh.lastTrainPtr != westHigh.readyTrainPtr) return 0;
	if(westLow.lastTrainPtr != westLow.readyTrainPtr) return 0;
	if(readyQueue.lastTrainPtr != readyQueue.readyTrainPtr) return 0;
	return 1;
}

//Compares two trains and returns whichever train should go first on the track.
size_t bestTrain(size_t a, size_t b){
	threadData aData = threadArray[a];
	threadData bData = threadArray[b];
	//If one train has higher priority than the other, the former comes first.
	if(aData.pri == HIGH && bData.pri == LOW){
		return a;
	}
	else if(aData.pri == LOW && bData.pri == HIGH){
		return b;
	}
	//If the trains are going in the same direction, compare loading times and ID.
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

int main(int argc, char *argv[]){

	//Checks to see if the user passed the right amount of arguments.
	if(argc != 2){
		puts("ERROR: this program needs one input file.");
		exit(EXIT_FAILURE);
	}

	//Initialize all mutexes and condition variables.
	pthread_mutex_init(&startMutex, NULL);
	pthread_mutex_lock(&startMutex);
	pthread_cond_init(&startCond, NULL);
	pthread_mutex_init(&eastHighMutex, NULL);
	pthread_mutex_init(&eastLowMutex, NULL);
	pthread_mutex_init(&westHighMutex, NULL);
	pthread_mutex_init(&westLowMutex, NULL);
	pthread_mutex_init(&readyMutex, NULL);
	pthread_cond_init(&readyCond, NULL);

	//Initialize all trains.
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
	//Get the time that all trains started laoding.
	clock_gettime(CLOCK_MONOTONIC,&begin);

	pthread_mutex_lock(&readyMutex);
	//Continue until all trains are done.
	while(!isEmpty()){
		//Wait until a train is done loading.
		if(readyQueue.lastTrainPtr == readyQueue.readyTrainPtr){
			pthread_cond_wait(&readyCond,&readyMutex);
		}
		size_t ID;
		//If two or more trains are ready, pick the correct one.
		if(readyQueue.lastTrainPtr - readyQueue.readyTrainPtr > 1){
			size_t minID = readyQueue.readyTrainPtr;
			size_t i;

			for(i = readyQueue.readyTrainPtr+1; i < readyQueue.lastTrainPtr; i++){
				minID = bestTrain(minID,i);
			}
			if(minID != readyQueue.readyTrainPtr){
				size_t temp = readyQueue.queue[readyQueue.readyTrainPtr];
				readyQueue.queue[readyQueue.readyTrainPtr] = readyQueue.queue[minID];
				readyQueue.queue[minID] = temp;
			}
			readyQueue.readyTrainPtr++;
			ID = readyQueue.queue[minID];
		}
		//If there is only one train ready, pick that one.
		else{
			ID = readyQueue.queue[readyQueue.readyTrainPtr++];
		}
		printMessage(ID, ON);
		//Cross the train.
		usleep(threadArray[ID].crossTime*100000);

		if(threadArray[ID].dir == EAST){
			Turn = WEST;
		}
		else{
			Turn = EAST;
		}
		printMessage(ID, OFF);
	}

	//Exit the program.
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
