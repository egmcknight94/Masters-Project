#include "parser.h"
typedef struct {
	int data;
	int length;
} writeData;
void setUp(char *, char *, char *);
void readScan();
FILE * inFile;
FILE * outFile;
FILE * payload;
int outBuffer = 0;
int outBits = 0;
void printToFile(writeData *);
writeData * findCode(int, hufftree *);
void huffEncode(int, int, hufftree *);	
int payloadSize;
int payloadBuffer;
int payloadBits;
void encodePayload(int *);
int main(int argc, char ** args) {
	setUp(args[1], args[2], args[3]);
	readFile();
	tearDown();
}
void setUp(char * inFileName, char * outFileName, char * payloadName) {
	ACtrees = malloc(sizeof(hufftree *) * 2);
	ACtrees[0] = NULL;
	ACtrees[1] = NULL;
	DCtrees = malloc(sizeof(hufftree *) * 2);
	DCtrees[0] = NULL;
	DCtrees[1] = NULL;
	inFile = fopen(inFileName,"rb");
	outFile = fopen(outFileName,"wb");
	payload = fopen(payloadName,"rb");
	fseek(payload,0L,SEEK_END);
	payloadSize = ftell(payload);
	int size = 0;
	while(1 << size <= payloadSize) {size++;}
	if(size % 2 == 1) {
		size++;
	}
	payloadBits = 6 + size;
	payloadBuffer = (size << size) + payloadSize;
	printf("Bits %d\nSize %d\nBuffer %d\n",size,payloadSize,payloadBuffer);
	rewind(payload);
}
void encodePayload(int * buffer) {
	for(int pair = 0; pair < 32; pair++) {
		if(buffer[pair*2] != 0 || buffer[pair*2+1] != 0) {
			int value = (buffer[pair*2] - buffer[pair*2+1]) % 4;
			if(value < 0) {
				value += 4;
			}
			if(payloadBits == 0) {
				payloadBits = 8;
				payloadBuffer = fgetc(payload);
				if(payloadBuffer == EOF) {
				printf("payload done\n");
					return;
				}
			}
			int bitsToEncode = payloadBuffer >> payloadBits - 2;
			payloadBuffer = payloadBuffer % (1 << payloadBits - 2);
			payloadBits -= 2;
			if(value != bitsToEncode) {
				buffer[pair*2] += 1;
				buffer[pair*2+1] -= 1;
				if(abs(buffer[pair*2]) > 2047 || abs(buffer[pair*2+1]) > 2047) {
					printf("Overflow\n");
				}
				value = (value + 2)%4;
				if(value < 0) {
					value += 4;
				}
			}
			if(value != bitsToEncode) {
				buffer[pair*2] += 1;
				buffer[pair*2+2] -= 1;
				if(abs(buffer[pair*2]) > 2047 || abs(buffer[pair*2+2]) > 2047) {
					printf("Overflow\n");
				}
				value = (value + 1)%4;
				if(value < 0) {
					value += 4;
				}
			}
			if(value != bitsToEncode) {
				buffer[pair*2] -= 1;
				buffer[pair*2+1] += 1;
			}
			if(buffer[pair*2] == 0 && buffer[pair*2+1] == 0) {
				if(rand() < 0.5) {
					buffer[pair*2] += 2;
					buffer[pair*2+1] -= 2;
				}
				else {
					buffer[pair*2] -= 2;
					buffer[pair*2+1] += 2;
				}
			}
		}
	}
}
int getByte() { //get the next byte in the input file, push it to the output file, and return it
	int ret = fgetc(inFile);
	if(ret != EOF) {
		fputc(ret,outFile);
	}
	return ret;
}
void printToFile(writeData * out) {
	outBuffer = (outBuffer << out->length) + out->data;
	outBits += out->length;
	while(outBits >= 8) {
		fputc(outBuffer >> (outBits-8),outFile);
		if(outBuffer >> (outBits - 8) == 255) {
			fputc(0,outFile);
		}
		outBuffer = outBuffer % (1 << (outBits - 8));
		outBits -= 8;
	}
	free(out);
}
void eachMCU() {
	if(outBits) {
	    writeData * clear = malloc(sizeof(writeData));
		clear->data = (1 << (8 - outBits)) - 1;
		clear->length = 8 - outBits;
		printToFile(clear); //last byte in a block ends in trailing 1s, as per standard
	}
}
writeData * findCode(int data, hufftree * root) {
	writeData *ret;
	if(root->data == data) {
		ret = malloc(sizeof(writeData));
		ret->data = 0;
		ret->length = 0;
	}
	else if(root->data != -1) {
		ret = NULL;
	}
	else {
		if(root->bit0) {
			ret = findCode(data,root->bit0);
		}
		if(ret) {
			ret->length += 1;
		}
		else {
			if(root->bit1) {
				ret = findCode(data,root->bit1);
			}
			if(ret) {
				ret->data += 1 << ret->length;
				ret->length += 1;
			}
		}
	}
	return ret;
}
void huffEncode(int zeroes, int data, hufftree * root) {
	if(data == 0) {
		writeData * wri = findCode(0,root);
		if(wri) {
			printToFile(findCode(0,root));
		}
		else {
		}
		return;
	}
	while(zeroes > 15) {
		zeroes -= 16;
		writeData * wri = findCode(240,root);
		if(wri) {
			printToFile(findCode(240,root));
		}
		else {
		}
	}
	int bits = 0;
	int cmp = abs(data);
	while((1 << bits) <= cmp) {bits++;}
	writeData * wri = findCode(16*zeroes+bits,root);
	if(wri) {
		printToFile(findCode(16*zeroes+bits,root));
	}
	else {
	}
	if(data < 0) {
		data += (1 << bits) - 1;
	}
	writeData * tmp = malloc(sizeof(writeData));
	tmp->data = data;
	tmp->length = bits;
	printToFile(tmp);
}
void manipData(int * values, hufftree * DCroot, hufftree * ACroot) {
	if(payloadBuffer != EOF && thumbnailPassed) {
		encodePayload(values);
	}
	huffEncode(0,values[0],DCroot);
	int zeroes = 0;
	for(int valuesIter = 1; valuesIter < 63; valuesIter++) {
		if(values[valuesIter] == 0) {
			zeroes++;
		}
		else {
			huffEncode(zeroes,values[valuesIter],ACroot);
			zeroes = 0;
		}
	}
	huffEncode(zeroes,values[63],ACroot);
}