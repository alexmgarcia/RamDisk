/*
Nome: Alexandre Martins Garcia - nº 34625
Nome: Flávio Duarte Pacheco Fernandes - nº 35326

Notas em relação ao trabalho: Todas as funções estão implementadas

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "RamDisk.h"

#define LIST_END	-1	// Constant to end free lists of file descriptor entries
#define NO_FILE		-1	// Constant to invalidate file descriptors

#define NO_BLOCK	((BlockPt)NULL)	// Constant to mark the end of block lists

// The file system
FileSystem fs;

// The file descriptors table and the head of the free file descriptors list
FileDescriptor fdt[MAX_OPEN_FILES];	// File Descriptors Table
FileDesc fffd;						// first free file descriptor

// Allocate a new free block
BlockPt allocBlock() {
	BlockPt blockPt = fs.ffb;
	fs.ffb = blockPt->next;
	return blockPt;
}

// Free a block, making it available to be allocated with allocBlock
void freeBlock(BlockPt block) {
	block->next = fs.ffb;
	fs.ffb = block;
}

// Allocate a file entry
FileEntry* allocFileEntry(FileName fn) {
	// Going to point to the desired file entry
	FileEntry* fileTable = &fs.fileTable[fs.fffe]; 
	fileTable->direct = NULL;
	fileTable->indirect = NULL;
	fileTable->doubleIndirect = NULL;
	fs.fffe = fileTable->size; // Change first free file entry
	fileTable->size = 0;
	fs.nFiles++;
	strcpy(fileTable->name, fn);
	return fileTable;
}

// Free all direct, indirect and doubleIndirect pointers on a file entry
void freeBlockPointers(FileEntry *fe) {
	int i,j;
	int nPointers = fs.blkSize/sizeof(BlockPt);
	if (fe->direct != NULL) // Direct
		freeBlock(fe->direct);
	if (fe->indirect!=NULL){ // Indirect
		for(i=0;i<nPointers;i++){
			if(fe->indirect->blocks[i]!=NULL)	
				freeBlock(fe->indirect->blocks[i]);
		}
		freeBlock(fe->indirect);
	}	
	if(fe->doubleIndirect!=NULL){ // Double indirect
		for(i=0;i<nPointers;i++){
			for(j=0;j<nPointers;j++){
				if(fe->doubleIndirect->blocks[i]->blocks[j]!=NULL)
					freeBlock(fe->doubleIndirect->blocks[i]->blocks[j]);	
			}
			if(fe->doubleIndirect->blocks[i]!=NULL)
				freeBlock(fe->doubleIndirect->blocks[i]);
		}
	}
	fe->direct = NULL;
	fe->indirect = NULL;
	fe->doubleIndirect = NULL;
}

// Free a file entry, freeing its direct, indirect and doubleindirect pointers
void freeFileEntry(FileEntry* fe, int index) {
	strcpy(fe->name, "");	
	fe->size = fs.fffe;
	fs.fffe = index;
	fs.nFiles--;
	
	freeBlockPointers(fe);
}

// Allocate a free file descriptor
FileDesc allocFileDesc(File index) {
	File current, pos_tmp;
	fdt[fffd].f = index;
	current = fffd;
	pos_tmp = fdt[fffd].pos;
	fdt[fffd].pos = 0;
	fffd = pos_tmp;
	return current;
}

// Free a file descriptor
void freeFileDesc(FileDesc fd) {
	fdt[fd].f = NO_FILE;
	fdt[fd].pos = fffd;
	fffd = fd;
}

// Get a pointer to a block that is used to know on which block we will
// be writing on i.e., the pointer will point to a block in direct, indirect 
// or double indirect, depending on the desired position
BlockPt getBlock(File f, Size *pos) {
	BlockPt block = NO_BLOCK;
	int i=0;
        int nPointers = fs.blkSize/sizeof(BlockPt);
        if(*pos<fs.blkSize) { // Direct
		if (fs.fileTable[f].direct == NULL)
			// Allocate direct if it isnt already allocated
			fs.fileTable[f].direct = allocBlock();
		block= fs.fileTable[f].direct;
		return block;
	}
		// Change current position to go to a valid position on indirect
        *pos = *pos - fs.blkSize;
        if (*pos<nPointers*fs.blkSize) { // Indirect
		if (fs.fileTable[f].indirect == NULL){
			// Allocate indirect if it isnt already allocated
			fs.fileTable[f].indirect = allocBlock();	
			for (i=0;i<nPointers;i++){
				// All the indirect block pointers will be NULL	at the beginning
				fs.fileTable[f].indirect->blocks[i]= NULL;			
			}
		}		
		if (fs.fileTable[f].indirect->blocks[*pos/fs.blkSize]==NULL){	
			// Allocate the desired indirect block pointer if it isnt already allocated
			fs.fileTable[f].indirect->blocks[*pos/fs.blkSize] = allocBlock();
		}
		// Now the block pointer will point to a block on indirect blocks
		block = fs.fileTable[f].indirect->blocks[*pos/fs.blkSize];
		// Change current position to be a valid blocks' index
		*pos=(*pos%fs.blkSize);
		return block;
	}
		// Change current position to go to a valid position on double indirect
        *pos = *pos - nPointers*fs.blkSize;
        if (*pos<nPointers*nPointers*fs.blkSize) { // Double indirect
		if (fs.fileTable[f].doubleIndirect == NULL){
			fs.fileTable[f].doubleIndirect = allocBlock();
			for (i=0;i<nPointers;i++){
				// All the double indirect block pointers will be NULL
				// at the beginning
				fs.fileTable[f].doubleIndirect-> blocks[i]= NULL;				
			}

		}
		if (fs.fileTable[f].doubleIndirect -> 
			blocks[*pos/(nPointers*fs.blkSize)]==NULL){
			// Allocate the desired double indirect block pointer if it isnt 
			// already allocated
			fs.fileTable[f].doubleIndirect -> 
				blocks[*pos/(nPointers*fs.blkSize)] = allocBlock();
			for (i=0;i<nPointers;i++){
				// All the indirect blocks at double indirect block pointers
				// will be NULL at the beginning
				fs.fileTable[f].doubleIndirect -> 
					blocks[*pos/(nPointers*fs.blkSize)] -> blocks[i]= NULL;				
			}
		}
		if (fs.fileTable[f].doubleIndirect -> 
			blocks[*pos/(nPointers*fs.blkSize)] -> 
			blocks[*pos%(nPointers*fs.blkSize)/fs.blkSize]==NULL){
			// Allocate the desired indirect block at double indirect block pointer
			// if it isnt already allocated
			fs.fileTable[f].doubleIndirect -> 
				blocks[*pos/(nPointers*fs.blkSize)] -> 
				blocks[(*pos%(nPointers*fs.blkSize))/fs.blkSize]=allocBlock();
		}
		// Now the block pointer will point to a block on indirect
		// blocks at double indirect blocks
		block=(fs.fileTable[f].doubleIndirect -> 
			blocks[*pos/(nPointers*fs.blkSize)] ->
			blocks[(*pos%(nPointers*fs.blkSize))/fs.blkSize]);
		// Change current position to be a valid blocks' index
		*pos=((*pos%(nPointers*fs.blkSize))%fs.blkSize);
		return block;
	}
	return block;
}

// This should create the file system, as well as setup the File Descriptors table
int rdStart(int blockSize, int nBlocks, Size maxFiles) {
	int i = 0;
	BlockPt base;
	FileEntry* fileTable;
	Size blkSize = blockSize;

	// Initial validations
	if(blockSize < MIN_BLK_SIZE) return ERR_INVALID_BLK_SIZE;

	// Check if blockSize is a power of 2
	while (blkSize % 2 == 0 && blkSize > 1) {
		blkSize /= 2;
	}
	if (blkSize != 1) return ERR_INVALID_BLK_SIZE;
	if (maxFiles <= 0) return ERR_MAXFILES_NOT_POSITIVE;

	// Initialize file system control information
	fs.maxFiles = maxFiles;
	fs.nFiles = 0;
	fs.blkSize = blockSize;
	fs.fffe = 0;
	// Allocate file table and set it up
	if ((fs.fileTable = 
		(FileEntry*)malloc(sizeof(FileEntry) * maxFiles)) == NULL)
		return ERR_NOT_ENOUGH_MEMORY;
	fileTable = fs.fileTable;
	// Initialize each file entry
	for(i=0;i<maxFiles-1;i++) {
		// The file entry will point to the next free file entry
		fileTable->size = i+1; 
		fileTable->direct = NULL;
		fileTable->indirect = NULL;
		fileTable->doubleIndirect = NULL;
		strcpy(fileTable->name, "");
		fileTable++;
	}
	fileTable->size=LIST_END;
	fs.fffe = 0;

	// Setup the file descriptors table 
	fffd = 0;
	for (i=0;i<MAX_OPEN_FILES-1;i++) {
		fdt[i].pos = i+1;
		fdt[i].f = NO_FILE;
	}
	fdt[MAX_OPEN_FILES-1].pos = LIST_END;
	
	// Setup the RamDisk blocks
	if ((fs.base = 
		(BlockPt)malloc(nBlocks * fs.blkSize*(sizeof(BlockPt)))) == NULL) 
		return ERR_NOT_ENOUGH_MEMORY;
	fs.ffb = fs.base;
	base = fs.base;
	for (i=0;i<nBlocks-1;i++) {
		base->next = &fs.base[(i+1)*fs.blkSize];
		base=base->next;
	}
	base->next = NO_BLOCK;
	return NO_ERROR;
}

// Deallocates all memory used by the ram disk dynamically allocated structures
void rdStop(void) {
	// Free blocks
	free(fs.base);
	// Free the file table
	free(fs.fileTable);		
}

// Theoretical maximum size that a file can have in this file system
Size rdMaxFileSize(void) {
	Size blockSize = fs.blkSize;
	Size diskBlock = (blockSize / sizeof(BlockPt));
	Size indBlock =  diskBlock * blockSize;
	Size doubIndBlock = diskBlock * diskBlock * blockSize;
	return blockSize + indBlock + doubIndBlock;
}

// Get the index of a file on the fileTable
File getFile(FileName fn) {
	FileEntry* fileTable = fs.fileTable;
	int i = 0;
	bool found = false;
	while (i<fs.maxFiles && !found) {
		if (strcmp(fileTable->name, fn) == 0) {
			found = true;
		}
		else {
			fileTable++;
			i++;
		}
	}
	return (!found ? NO_FILE : i);
}

// Opens a file named fn for simultaneous read/write access
FileDesc rdOpen(FileName fn, int flags){	
	int index = getFile(fn);
	FileEntry* fe;
	int pos_tmp;
	int current;

	if (fffd == LIST_END) return ERR_TOO_MANY_OPEN_FILES;
	if (fs.nFiles == fs.maxFiles) return ERR_TOO_MANY_FS_FILES;
	switch(flags) {
		case OF_CREATE:
			if (index == NO_FILE) {
				index = fs.fffe;
				fe = allocFileEntry(fn);
			}
			// Insert into fdt
			fdt[fffd].f = index;
			current = fffd;
			pos_tmp = fdt[fffd].pos;
			fdt[fffd].pos = 0;
			fffd = pos_tmp;
			return current;
		break;
		case OF_TRUNCATE:
			if (index != NO_FILE) {
				// Free all the block pointers if the file is to be truncated
				// and already exists on the file table
				freeBlockPointers(&fs.fileTable[index]);
				fs.fileTable[index].size = 0;
			}
			else {
				index = fs.fffe;
				fe = allocFileEntry(fn);
			}
			// Insert into fdt
			return allocFileDesc(index);
			break;
		case OF_DEFAULT:
			if (index != NO_FILE)
				return allocFileDesc(index);
			else return ERR_FILE_NOT_FOUND;

		break;
	}
	return ERR_FILE_NOT_FOUND;	
}

// Closes the file and releases the file descriptor entry being used
void rdClose(FileDesc fd){
	if (fdt[fd].f != NO_FILE)
		freeFileDesc(fd);
}

// Closes all the opened file descriptors of a specific file
void closeDesc(File f) {
	int i = 0;
	for(;i<MAX_OPEN_FILES;i++)
		if (fdt[i].f == f) rdClose(i);
}

// Removes the file named fn from the filesystem
int rdDelete(FileName fn){
	int index = getFile(fn);
	if (index != NO_FILE) {
		// Close all the opened file descriptors of the file
		closeDesc(index);
		freeFileEntry(&fs.fileTable[index], index);
	}
	return NO_ERROR;
}

// Reads a single character from a file
int rdGet(FileDesc fd){
	File f = fdt[fd].f;
	Size pos = fdt[fd].pos;
	BlockPt tm;

	if (fdt[fd].f != NO_FILE) { // File is opened
		if (strcmp(fs.fileTable[f].name, "") == 0) {
			// If the file doesn't exists
			rdClose(fd);
			return ERR_FILE_NOT_FOUND;
		}
		else {
			// Get the block referred by current pos
			tm = getBlock(f, &pos);
			// The cast is used because END_OF_FILE is a negative value
			// and a Byte is an unsigned char
			if (tm->data[pos] == (unsigned char)END_OF_FILE)
				return END_OF_FILE;
			else return tm->data[pos];
		}
	}
	return NO_FILE;
}

// Writes character c at the current position
int rdPut(Byte c, FileDesc fd){
	File f = fdt[fd].f;
	Size pos = fdt[fd].pos;
	if (fs.ffb == NO_BLOCK) return ERR_NO_SPACE_LEFT_ON_DEVICE;
	if (fdt[fd].f != NO_FILE) { //File is opened
		if (strcmp(fs.fileTable[f].name, "") == 0) {
			// If the file doesn't exists
			rdClose(fd);
			return ERR_FILE_NOT_FOUND;
		}
		else {
			// Get the block referred by current pos
			BlockPt tm = getBlock(f, &pos);
			if (pos > fs.fileTable[f].size-1)
				// If the character is to be written on an empty position
				// then the size will be incremented
				fs.fileTable[f].size++;	
			rdSetPos(fd, (rdGetPos(fd))+1);
			return tm->data[pos]=c;
		}
	}
	
	return ERR_FILE_NOT_FOUND;
}

// Reads count bytes from the current position into the buffer pointed by buf
Size rdRead(FileDesc fd, void *buf, Size count){
	File f = fdt[fd].f;
	int i = 0;
	Byte* buf2;
	Byte c;
	buf2 = (Byte*)buf;
	if (f != NO_FILE) { // File os opened
		if (strcmp(fs.fileTable[f].name, "") == 0) {
			// If the file doesn't exists
			rdClose(fd);
			return ERR_FILE_NOT_FOUND;
		}
		else {
			i = fdt[fd].pos;
			// Read each character from current position to count
			// or until an END_OF_FILE appears
			while(i<count && (c = rdGet(fd)) != END_OF_FILE) {
				*buf2 = c;
				buf2++;
				rdSetPos(fd, ++i);
			}
		}
	}
	return i;
}

// Writes count bytes into the file, starting at the current position
Size rdWrite(FileDesc fd, const void *buf, Size count){
	File f = fdt[fd].f;
	int i=0;
	Size size = fs.fileTable[f].size;
	Byte* buf2 = (Byte*)buf;
	Size pos = rdGetPos(fd);
	if (fs.ffb == NO_BLOCK) return ERR_NO_SPACE_LEFT_ON_DEVICE;
	if (f != NO_FILE){
		if (strcmp(fs.fileTable[f].name, "") == 0) { // File is opened
			// If the file doesn't exists
			rdClose(fd);
			return ERR_FILE_NOT_FOUND;
		}
			else {
				if (size < pos) { 
					// If current file size if less than the position to write characters
					if (size == 0) { // If its an empty file
						rdSetPos(fd, 0);
						i = 0;
						// Fill it with zeros
						while(i<pos) {
							rdPut(0, fd);
							++i;
						}
					}
					else { // If it isnt an empty file
						while ((rdGet(fd)) != END_OF_FILE) 
							++i;
						// Fill it with zeros
						while (i<pos) {
							rdPut(0, fd);
							++i;
						}
					}
				}
				// If size is more than the position to write
				while (i<count) {
					rdPut(*buf2, fd);
					buf2++;
					++i;
				}
				// Write END_OF_FILE on last position
				rdPut(END_OF_FILE, fd);
				rdSetPos(fd,(rdGetPos(fd))-1);
				fs.fileTable[f].size--;

			}
	}
	return count;
}

// Sets the current position to from the start of the file
Offset rdSetPos(FileDesc fd, Offset offset){

	if (fdt[fd].f != NO_FILE) {
		fdt[fd].pos = offset;
		return offset;
	}
	return ERR_FILE_NOT_FOUND;
}

// Returns the current position
Offset rdGetPos(FileDesc fd) {
	if (fdt[fd].f != NO_FILE)
		return fdt[fd].pos;
	return ERR_FILE_NOT_FOUND;
}

// Loads a file named diskFile from the OS file system,
Size rdLoadFromDisk(char *rdFile, char *diskFile) {
	FILE *file;
	FileDesc f;
	char c;
	int i = 0;
	if (fs.nFiles == fs.maxFiles) return ERR_TOO_MANY_FS_FILES;
	if (fffd == LIST_END) return ERR_TOO_MANY_OPEN_FILES;
	if (fs.ffb == NO_BLOCK) return ERR_NO_SPACE_LEFT_ON_DEVICE;
	if ((file = fopen(diskFile, "r")) == NULL) return ERR_FILE_NOT_FOUND;
	f = rdOpen(rdFile, OF_TRUNCATE);
	while ((c = fgetc(file)) != EOF) {
		rdSetPos(f, i);
		rdPut(c, f);
		i++;
	}
	rdSetPos(f, i);
	// Write END_OF_FILE on last position
	rdPut(END_OF_FILE, f);
	fclose(file);
	return fs.fileTable[getFile(rdFile)].size;
}

// Stores the ramdisk file contents of a file named rdFile
Size rdStoreToDisk(char *diskFile, char *rdFile) {
	FileDesc f;
	int i = 0;
	Byte c;
	FILE* file = fopen(diskFile, "w");
	if (fffd == LIST_END) return ERR_TOO_MANY_OPEN_FILES;
	if (getFile(rdFile) == NO_FILE)  return ERR_FILE_NOT_FOUND;
	f = rdOpen(rdFile, OF_DEFAULT);
	rdSetPos(f, 0);

	// Get all characters until END_OF_FILE is found
	// The cast is used because END_OF_FILE is a negative value
	// and a Byte is an unsigned char
	while ((c = rdGet(f)) != (unsigned char)END_OF_FILE) {
		fputc(c, file);
		rdSetPos(f, ++i);
	}
	fclose(file);
	return i;
}
