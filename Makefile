# NAME: Jesse Catalan,Ricardo Kuchimpos
# EMAIL: jessecatalan77@gmail.com,rkuchimpos@gmail.com
# ID: 204785152,704827423


lab3a: lab3a.c ext2_fs.h
	gcc -o lab3a -Wall -Wextra lab3a.c -lm

.PHONY: clean
clean:
	rm -rf lab3a
	rm -rf *.tar.gz

.PHONY: dist
dist:
	tar czf lab3a-204785152.tar.gz lab3a.c ext2_fs.h Makefile README
