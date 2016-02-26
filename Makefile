all: plex dep

plex: plex.c
	$(CC) -Wall plex.c -lutil -o plex

dep: per.c
	$(CC) -Wall per.c -o per
