all: lab-normal master

lab-debug: lab.c
	gcc -DDEBUG lab.c -g -o lab

master: master.c
	gcc master.c -g -o master

lab-normal:
	gcc -g lab.c -o lab

test: lab
	./lab ./inputFile

test-output: lab
	./lab ./inputFile ./outputFile

test-dna: lab
	./lab ./inputFile ./outputFile ./testDNA

debug: lab
	gdb --args ./lab ./inputFile


clean: clean-lab clean-master clean-other

clean-lab:
	rm -f lab
clean-master:
	rm -f master
clean-other:
	rm -f outputFile
