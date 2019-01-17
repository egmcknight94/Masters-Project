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
int outBuffer = 0;
int outBits = 0;
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
}
void decodePayload(int * buffer) {
	for(int pair = 0; pair < 32; pair++) {
		if(buffer[pair*2] != 0 || buffer[pair*2+1] != 0) {
			int value = (buffer[pair*2] - buffer[pair*2+1]) % 4;
			if(value < 0) {
				value += 4;
			}
			payloadBuffer = (payloadBuffer << 2) + value;
			payloadBits += 2;
			if(payloadSize == 0) {
				printf("Value %d\n",value);
				printf("payloadBuffer %d\n",payloadBuffer);
				if(payloadSizeBits) {
					if(payloadBits == payloadSizeBits) {
						payloadSize = payloadBuffer;
						printf("Payload size: %d\n",payloadSize);
						payloadBuffer = 0;
						payloadBits = 0;
					}
				}
				else if(payloadBits == 6) {
					payloadSizeBits = payloadBuffer;
					printf("Bits %d\n",payloadSizeBits);
					payloadBuffer = 0;
					payloadBits = 0;
				}
			}
			else if(payloadBits == 8) {
				fputc(payloadBuffer, outFile);
				payloadBits = 0;
				payloadBuffer = 0;
				if(ftell(outFile) == payloadSize) {
					return;
				}
			}
		}
	}
}
void eachMCU() {}
void manipData(int * values, hufftree * DCroot, hufftree * ACroot) {
	if(thumbnailPassed) {
		decodePayload(values);
		if(ftell(outFile) == payloadSize) {
			exit(0);
		}
	}
}