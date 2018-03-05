My design has changed significantly from my design outlined in p2a.
This version has n+6 mutexes, where n is the number of trains.
There are also n+2 convars.
Each train has a mutex and an associated convar, which is used for crossing.
When 
