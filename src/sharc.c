/*
 * Copyright (c) 2013, Guillaume Voirin
 * All rights reserved.
 *
 * This software is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Alternatively, you can license this software under a commercial
 * license, as set out in licenses/commercial.txt.
 *
 * Centaurean SHARC
 * www.centaurean.com/sharc
 *
 * 01/06/13 13:08
 */

#include "sharc.h"

FORCE_INLINE void compress(const char* inFileName, const byte attemptMode, const uint32_t blockSize) {
    char outFileName[strlen(inFileName) + 6];
    
    outFileName[0] = '\0';
    strcat(outFileName, inFileName);
    strcat(outFileName, ".sharc");
    
    FILE* inFile = checkOpenFile(inFileName, "rb");    
    FILE* outFile = checkOpenFile(outFileName, "wb+");
    
    byte reachableMode;
    const byte nThread = 0;
    
    time_t chrono = clock();
    
    struct stat attributes;
    stat(inFileName, &attributes);
    
    FILE_HEADER fileHeader = createFileHeader(blockSize, attributes);
    fwrite(&fileHeader, sizeof(FILE_HEADER), 1, outFile);
    
    BYTE_BUFFER in = createByteBuffer(readBuffer[nThread], 0, blockSize);
    BYTE_BUFFER out = createByteBuffer(writeBuffer[nThread], 0, blockSize);
    
    while((in.size = (uint32_t)fread(readBuffer[nThread], sizeof(byte), blockSize, inFile)) > 0) {
        reachableMode = sharcEncode(&in, &out, attemptMode);
        
        BLOCK_HEADER blockHeader;
        if(reachableMode ^ MODE_COPY)
            blockHeader = createBlockHeader(reachableMode, out.position);
        else
            blockHeader = createBlockHeader(reachableMode, in.size);
        
        fwrite(&blockHeader, sizeof(BLOCK_HEADER), 1, outFile);
        
        if(reachableMode ^ MODE_COPY)
            fwrite(writeBuffer[nThread], sizeof(byte), out.position, outFile);
        else
            fwrite(readBuffer[nThread], sizeof(byte), in.size, outFile);
        
        rewindByteBuffer(&in);
        rewindByteBuffer(&out);
    }
    chrono = (1000 * (clock() - chrono)) / CLOCKS_PER_SEC;
    
    uint64_t totalRead = ftell(inFile);
    uint64_t totalWritten = ftell(outFile);
    
    fclose(inFile);
    fclose(outFile);

    double ratio = (1.0 * totalWritten) / totalRead;
    double speed = (1000.0 * totalRead) / (chrono * 1024.0 * 1024.0);
    printf("Compressed file %s, %lli bytes in, %lli bytes out, ", inFileName, totalRead, totalWritten);
    printf("Ratio out / in = %g, Time = %ld ms, Speed = %f MB/s\n", ratio, chrono, speed);
} 

FORCE_INLINE void decompress(const char* inFileName) {
    char* outFileName = "test.dec";
    
    FILE* inFile = checkOpenFile(inFileName, "rb");
    FILE* outFile = checkOpenFile(outFileName, "wb+");
    
    //uint32_t bytesRead;
    //byte mode;
    const byte nThread = 0;
    
    time_t chrono = clock();
    FILE_HEADER fileHeader = readFileHeader(inFile);
    BYTE_BUFFER in = createByteBuffer(readBuffer[nThread], 0, 0);
    BYTE_BUFFER out = createByteBuffer(writeBuffer[nThread], 0, fileHeader.bufferSize);
    
    BLOCK_HEADER blockHeader;
    while(readBlockHeaderFromFile(&blockHeader, inFile) > 0) {
        in.size = (uint32_t)fread(readBuffer[nThread], sizeof(byte), blockHeader.nextBlock, inFile);
        switch(blockHeader.mode) {
            case MODE_COPY:
                fwrite(in.pointer, sizeof(byte), in.size, outFile);
                break;
            default:
                if(sharcDecode(&in, &out, blockHeader.mode) ^ 0x1)
                    error("Unable to decompress !");
                fwrite(out.pointer, sizeof(byte), out.position, outFile);
                break;
        }
        
        rewindByteBuffer(&in);
        rewindByteBuffer(&out);
    }
    chrono = (1000 * (clock() - chrono)) / CLOCKS_PER_SEC;
    
    uint64_t totalRead = ftell(inFile);
    uint64_t totalWritten = ftell(outFile);
    
    fclose(inFile);
    fclose(outFile);
    
    double speed = (1000.0 * totalWritten) / (chrono * 1024.0 * 1024.0);
    printf("Decompressed file %s, %lli bytes in, %lli bytes out, ", inFileName, totalRead, totalWritten);
    printf("Time = %ld ms, Speed = %f MB/s\n", chrono, speed);
}

FORCE_INLINE void usage() {
    printf("Centaurean Sharc %i.%i.%i - Copyright (C) 2013 Guillaume Voirin\n", MAJOR_VERSION, MINOR_VERSION, REVISION);
    printf("Usage : sharc [OPTIONS]... [FILES]...\n")   ;
    printf("Superfast archiving of files.\n\n");
    printf("Available options :\n");
    printf("  -c[LEVEL], --compress[=LEVEL]     compress files using LEVEL if specified (default)\n");
	printf("                                    LEVEL can have the following values :\n");
	printf("                                    0 = Fastest compression algorithm (default)\n");
	printf("                                    1 = Better compression (dual pass), slightly slower\n");
	printf("  -d, --decompress                  decompress files");
    exit(0);
}

int main(int argc, char *argv[]) {
    if(argc <= 1)
		usage();

    byte action = ACTION_COMPRESS;
	byte mode = MODE_SINGLE_PASS;
    
    size_t argLength;
    for(int i = 1; i < argc; i ++) {
        switch(argv[i][0]) {
            case '-':
                argLength = strlen(argv[i]);
                if(argLength < 2)
                    usage();
                switch(argv[i][1]) {
                    case 'c':
                        if(argLength == 2) {
                            mode = MODE_SINGLE_PASS;
                            break;
                        }
                        if(argLength != 3)
                            usage();
                        mode = argv[i][2] - '0';
                        break;
                    case 'd':
                        action = ACTION_DECOMPRESS;
                        break;
					case '-':
						if(argLength < 3)
							usage();
						switch(argv[i][2]) {
							case 'c':
								if(argLength != 12)
									usage();
								mode = argv[i][11] - '0';
								break;
							case 'd':
								action = ACTION_DECOMPRESS;
								break;
							default:
								usage();
						}
						break;
                }
                break;
            default:
                switch(action) {
                    case ACTION_DECOMPRESS:
                        decompress(argv[i]);
                        break;
                    default:
                        compress(argv[i], mode, PREFERRED_BUFFER_SIZE);
                        break;
                }
                break;
        }
    }
}