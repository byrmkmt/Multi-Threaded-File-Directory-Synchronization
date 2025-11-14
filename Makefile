all: project

project: BibakBOXServer.o BibakBOXClient.o
	gcc -o BibakBOXServer BibakBOXServer.o -lpthread
	gcc -o BibakBOXClient BibakBOXClient.o -lpthread

BibakBOXServer.o: BibakBOXServer.c 	
	gcc -c BibakBOXServer.c

BibakBOXClient.o: BibakBOXClient.c 	
	gcc -c BibakBOXClient.c

clean:
	rm *.o $(objects) BibakBOXServer BibakBOXClient
				
