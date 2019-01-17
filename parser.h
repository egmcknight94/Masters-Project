#include<stdlib.h>
#include<stdio.h>
#include<math.h>
typedef struct hufftree {
	int data;
	struct hufftree * parent;
	struct hufftree * bit0;
	struct hufftree * bit1;
} hufftree;
int thumbnailPassed = 0;
hufftree ** ACtrees;
hufftree ** DCtrees;
int bufferPos;
int bitBuffer;
FILE * inFile;
FILE * outFile;
int mcuHeight;
int mcuWidth;
int restartInterval = 0;
int * sampling;
void eachMCU();
hufftree * createNode(hufftree *);
void destroyTree(hufftree *);
int getByte();
void readHuffTables();
void resetTrees();
int getBit();
void manipData(int *, hufftree *, hufftree *);
void resetBitBuffer();
void readScan();
int parseDCCode(int table);
int parseACCode(int table);
int parseCode(hufftree *);
int parseValue(int);
void tearDown();
void parseSOF();
void readFile();
hufftree * createNode(hufftree * parent) {
	hufftree * ret = malloc(sizeof(hufftree));
	ret->data = -1;
	ret->parent = parent;
	ret->bit0 = NULL;
	ret->bit1 = NULL;
	return ret;
}
void destroyTree(hufftree * tree) {
	if(!tree->bit0) {
		destroyTree(tree->bit0);
		free(tree->bit0);
	}
	if(!tree->bit1) {
		destroyTree(tree->bit1);
		free(tree->bit1);
	}
	free(tree);
}
void readHuffTables() { //parse the huffman tables
	int bytesRead = 2; //need to count bytes to find end of segment
	int length = getByte()*256+getByte(); //length is two bytes
	while(bytesRead < length) { //read to end of segment
		int tableInfo = getByte(); //first byte of each table contains table information
		bytesRead++;
		hufftree * treeRoot; //pointer to root of tree in progress
		if(tableInfo/16 == 0) { //DC table
			if(DCtrees[tableInfo%16] != NULL) { //tree is left over from thumbnail
				destroyTree(DCtrees[tableInfo%16]); //get rid of it first
			}
			DCtrees[tableInfo%16] = createNode(NULL); //make a new root where the table should go
			treeRoot = DCtrees[tableInfo%16]; //set pointer to the root for the tree;
		}
		if(tableInfo/16 == 1) { //AC table
			if(ACtrees[tableInfo%16] != NULL) { //tree is left over from thumbnail
				destroyTree(ACtrees[tableInfo%16]); //get rid of it first
			}
			ACtrees[tableInfo%16] = createNode(NULL); //make a new root where the table should go
			treeRoot = ACtrees[tableInfo%16]; //set pointer to the root for the tree;
		}
		int *count = malloc(sizeof(int) * 16); //next sixteen bytes are counts of codes at each
		for(int slotNum = 0; slotNum < 16; slotNum++) {
			count[slotNum] = getByte();
			bytesRead++;
		}
		hufftree * iter = treeRoot; //variable to iterate through the tree
		int depth = 0; //keep track of how deep in the tree iter is
		for(int slotNum = 0; slotNum < 16; slotNum++) { //for every number of bits
			for(int codeNum = 0; codeNum < count[slotNum]; codeNum++) { //read that many codes
				while(depth <= slotNum) { //slotNum is one less than the code length in bits, so iterate until find a space deep enough
					if(!iter->bit0) { //if there isn't a node to the left already
						iter->bit0 = createNode(iter); //make one and move there
						iter = iter->bit0;
						depth++;
					}
					else if(!iter->bit1) { //otherwise check right
						iter->bit1 = createNode(iter);
						iter = iter->bit1;
						depth++;
					}
					else if(iter->parent) { //if both nodes exist, go to parent
						iter = iter->parent;
						depth--;
					}
					else {
						printf("Error parsing Huffman tables\n");
						return;
					}
				}
				iter->data = getByte(); //code is next byte
				bytesRead++;
				iter = iter->parent; //this node can't have children
				depth--;
			}
		}
		free(count);
	}
}
void resetTrees() {
	if(!DCtrees[0]) {
		destroyTree(DCtrees[0]);
		DCtrees[0] = NULL;
	}
	if(!DCtrees[1]) {
		destroyTree(DCtrees[1]);
		DCtrees[1] = NULL;
	}
	if(!ACtrees[0]) {
		destroyTree(ACtrees[0]);
		ACtrees[0] = NULL;
	}
	if(!ACtrees[1]) {
		destroyTree(ACtrees[1]);
		ACtrees[1] = NULL;
	}
}
int getBit() {
	if(bufferPos == 8) {
		bufferPos = 0;
		bitBuffer = fgetc(inFile);
		if(bitBuffer == 255) {
			int marker = fgetc(inFile);
			if(marker != 0) {
				ungetc(marker,inFile);
				ungetc(255,inFile);
				return -1;
			}
		}
	}
	int ret = (bitBuffer & 1 << (7 - bufferPos)) >> (7 - bufferPos);
	bufferPos++;
	return ret;
}
void resetBitBuffer() {
	bufferPos = 8;
}
void readScan() {
	int length = getByte()*256+getByte();
	int numComps = getByte();
	int * componentTableList = malloc(sizeof(int) * numComps);
	for(int iter = 0; iter < numComps; iter++) {
		getByte(); //don't care about channel id
		componentTableList[iter] = getByte(); //the tables used are in this byte
	}
	if(getByte() != 0 || getByte() != 63 || getByte() != 0) {
		printf("SOS marker unexpected\n");
		return;
	}
	resetBitBuffer();
	int curRestart = 0;
	int countBlocks = 0;
	printf("mcuWidth: %d\n",mcuWidth);
	printf("mcuHeight: %d\n",mcuHeight);
	for(int countHori = 0; countHori < mcuWidth; countHori++) {
		for(int countVert = 0; countVert < mcuHeight; countVert++) {
			if(restartInterval != 0) {
				if(curRestart == restartInterval) {
					curRestart = 0;
					if(bufferPos != 8) {
						bufferPos = 8;
					}
					eachMCU();
					getByte();
					getByte();
				}
				curRestart++;
			}
			countBlocks++;
			for(int comp = 0; comp < numComps; comp++) {
				for(int sample = 0; sample < sampling[comp]; sample++) {
					int usedAC = componentTableList[comp]/16;
					int usedDC = componentTableList[comp]%16;
					int * values = malloc(sizeof(int) * 64);
					int len = parseDCCode(usedDC);
					if(len > 0) {
						values[0] = parseValue(len);
						if(values[0] == 0) {
							return;
						}
					}
					else if (len == 0) {
						values[0] = 0;
					}
					else {
						return;
					}
					int valuesIter = 1;
					while(valuesIter < 64) {
						int code = parseACCode(usedAC);
						if(code == -1) {
							return;
						}
						else if(code == 0) {
							while(valuesIter < 64) {
								values[valuesIter] = 0;
								valuesIter++;
							}
						}
						else {
							for(int zeroCount = 0; zeroCount < code/16; zeroCount++) {
								values[valuesIter] = 0;
								valuesIter++;
							}
							if(code%16 != 0) {
								values[valuesIter] = parseValue(code%16);
								if(values[valuesIter] == 0) {
									return;
								}
							}
							else {
								values[valuesIter] = 0;
							}
							valuesIter++;
						}
					}
					manipData(values, DCtrees[usedDC],ACtrees[usedAC]);
					free(values);
				}
			}
		}
	}
	eachMCU();
}
int parseDCCode(int table) {
	return parseCode(DCtrees[table]);
}
int parseACCode(int table) {
	return parseCode(ACtrees[table]);
}
int parseCode(hufftree * iter) {
	while(iter->bit0) {
		int tmp = getBit();
		if(tmp == 0) {
			iter = iter->bit0;
		}
		else if(tmp == 1) {
			if(iter->bit1) {
				iter = iter->bit1;
			}
			else {
				return -1;
			}
		}
		else {
			return -1;
		}
	}
	return iter->data;
}
int parseValue(int valueLen) {
	int ret = 0;
	for(int bit = 0; bit < valueLen; bit++) {
		ret *= 2;
		int tmp = getBit();
		if(tmp == -1) {
			return 0;
		}
		ret += tmp;
	}
	if(ret < 1 << valueLen - 1) {
		ret -= (1 << valueLen) - 1;
	}
	return ret;
}
void tearDown() {
	fclose(outFile);
	fclose(inFile);
	if(!DCtrees[0]) {
		destroyTree(DCtrees[0]);
	}
	if(!DCtrees[1]) {
		destroyTree(DCtrees[1]);
	}
	free(DCtrees);
	if(!ACtrees[0]) {
		destroyTree(ACtrees[0]);
	}
	if(!ACtrees[1]) {
		destroyTree(ACtrees[1]);
	}
	free(ACtrees);
}
void parseSOF() {
	int length = getByte()*256+getByte();
	getByte();
	mcuWidth=getByte()*256+getByte();
	mcuHeight=getByte()*256+getByte();
	int numComps = getByte();
	if(sampling) { free(sampling); }
	sampling = malloc(sizeof(int) * numComps);
	int maxX = 0;
	int minX = 256;
	int maxY = 0;
	int minY = 256;
	for(int comp = 0; comp < numComps; comp++) {
		getByte();
		int sample = getByte();
		sampling[comp] = (sample%16)*(sample/16);
		if(sample%16 < minX) {
			minX = sample%16;
		}
		if(sample%16 > maxX) {
			maxX = sample%16;
		}
		if(sample%16 < minY) {
			minY = sample%16;
		}
		if(sample%16 > maxY) {
			maxY = sample%16;
		}
		getByte();
	}
	mcuHeight = ceil((double)mcuHeight/(maxY*8/minY));
	mcuWidth = ceil((double)mcuWidth/(maxX*8/minX));
}
void readFile() {
	int buff = 0;
	int layer = 0;
	while(buff != EOF) {
		if(buff == 255) {
			buff = getByte();
			switch(buff) {
				case 192:
				case 193:
					parseSOF();
					break;
				case 196: //huffman table
					readHuffTables();
					break;
				case 216: //Start of image. Using a counter so that EOI doesn't stop scan after thumbnail
					layer++;
					break;
				case 217: //End of image
					resetTrees();
					layer--; //reduce the counter
					if(layer == 0) { //if the counter is zero, all images are closed
						exit(0); //done
					}
					break;
				case 218: //Start of Scan
					printf("Started new scan\n");
					readScan();
					thumbnailPassed = 1;
					break;
				case 221: //Define restart interval
					restartInterval = getByte()*256+getByte();
					break;
			}
		}
		buff = getByte();
	}
}