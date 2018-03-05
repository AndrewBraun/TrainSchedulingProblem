My design has changed significantly from my design outlined in p2a.
This version has n+6 mutexes, where n is the number of trains.
There are also n+2 convars.

To start, the main thread reads all the trains from the input file,
allocates enough space, then places each train in the station.
Then, the trains in the station are sorted by loading time.
All train threads start by waiting for a signal, with the
convar being startCond and the mutex being startMutex.

Once all trains are loaded, the main thread signals all train threads to start.
Then, each train thread unlocks startMutex once they get it and start usleep.
Main thread then waits for a signal (readyCond and readyMutex).

Once a train is done loading it does the following:
  1: Places itself into the ready station.
  2: Increments the ready train ponter for its station to indicate it is done.
  3: Signals main  that it is ready.
  4: Waits on the signal from its corresponding convar/mutex.

When main detects that a train is ready for loading,
it first checks to see if there are any trains with the same loading time.
If yes, it loads those trains too.
If not, then it scans the ready station and selects the best train.
Once main has selected a train, it signals the thread's convar.
Then the train threads usleeps for the crossing time.
Once a train is done, it destroys its mutex and convar, then exits.
This process repeats until all the trains are done.
