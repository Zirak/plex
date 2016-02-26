all: plex dep

plex: plex.c
	clang -Wall plex.c -lutil -o plex

dep: per.c
	clang -Wall per.c -o per
