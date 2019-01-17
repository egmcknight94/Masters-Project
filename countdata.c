#include "parser.h"
typedef struct {
	int data;
	int length;
} writeData;
void setUp(char *, char *);
FILE * inFile;
FILE * outFile;
int bitBuffer;
int bufferPos;
int * sampling;
int * counts;
int outBuffer = 0;
int outBits = 0;
void printToFile(writeData *);
int mcuHeight;
int mcuWidth;
int payloadSize;
int payloadSizeBits;
int payloadBuffer;
int payloadBits;
void decodePayload(int *);
int main(int argc, char ** args) {
	setUp(args[1], args[2]);
	readFile();
	if(payloadBuffer != EOF) {
		printf("Did not reach end of file\n");
	}
	tearDown();
}
int getByte() {
	return fgetc(inFile);
}
void setUp(char * inFileName, char * outFileName) {
	ACtrees = malloc(sizeof(hufftree *) * 2);
	ACtrees[0] = NULL;
	ACtrees[1] = NULL;
	DCtrees = malloc(sizeof(hufftree *) * 2);
	DCtrees[0] = NULL;
	DCtrees[1] = NULL;
	inFile = fopen(inFileName,"rb");
	outFile = fopen(outFileName,"wb");
	payloadBits = 0;
	payloadBuffer = 0;
	counts = malloc(sizeof(int)*21);
	for(int x = 0; x < 21; x++) {
		counts[x] = 0;
	}
}
void eachMCU() {
	for(int x = 0; x < 21; x++) {
		printf("%d: %d instances\n",x-10,counts[x]);
	}
}
void manipData(int * values, hufftree * DCroot, hufftree * ACroot) {
	for(int val = 0; val < 63; val++) {
		if(values[val] > -11 && values[val] < 11) {
			counts[values[val]+10]++;
		}
	}
}