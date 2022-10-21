/**************************************************************
* Class:  CSC-415-03 Fall 2022
* Name: Abbas Mahdavi
* Student ID: 918345420
* GitHub UserID: AbbasMahdavi021
* Project: Assignment 5 – Buffered I/O
*
* File: b_io.c
*
* Description: This project handles a buffered IO.
  This c file code handles the buffering.
  The b_open returns a integer file descriptor.
  The b_read takes a file descriptor, 
  a buffer and the number of bytes desired.

  The main program (buffer-main.o, provided) uses the 
  command line arguments to specify data file and 
  the desired target file(s). The main program uses 
  b_open, reads some variable number of characters at a time 
  from the file using b_read, prints those characters 
  to the screen (ending in a newline character), 
  and loops until it has read the entire file, 
  then b_close the file and exit. 
  The program may open multiple files at one time and 
  will request on chunk and print that one for each file, 
  so the output will contain a line from file 1 
  than a line from file 2, etc until the end of file.
* 
*
**************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "b_io.h"
#include "fsLowSmall.h"
#define MAXFCBS 20	//The maximum number of files open at one time


// This structure is all the information needed to maintain an open file
// It contains a pointer to a fileInfo strucutre and any other information
// that you need to maintain your open file.
typedef struct b_fcb {
	fileInfo * fi;	//holds the low level systems file info

	char* fileBuffer;
	// Initialize new variables keep track of the current open file 
	int fileDescriptor;
	//index of how much of the block is used
	long int bufferIndex;
	//increases based on number of blocks read
	long int filePos;
	int currentBlock;

	} b_fcb;
	
//static array of file control blocks
b_fcb fcbArray[MAXFCBS];

// Indicates that the file control block array has not been initialized
int startup = 0;	

// Method to initialize our file system / file control blocks
// Anything else that needs one time initialization can go in this routine
void b_init ()
	{
	if (startup)
		return;			//already initialized

	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].fi = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free File Control Block FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].fi == NULL)
			{
			fcbArray[i].fi = (fileInfo *)-2; // used but not assigned
			return i;		//Not thread safe but okay for this project
			}
		}

	return (-1);  //all in use
	}

// b_open is called by the "user application" to open a file.  This routine is 
// similar to the Linux open function.  	
// You will create your own file descriptor which is just an integer index into an
// array of file control blocks (fcbArray) that you maintain for each open file.  

b_io_fd b_open (char * filename, int flags)
	{
	if (startup == 0) b_init();  //Initialize our system

	//call b_getFCB so that we can keep track of it in a int
	int fileDes = b_getFCB();
	//call GetFileInfo and init it to the fi var
	fileInfo* fi = GetFileInfo(filename);

	/*initilize the remaining variables
	to the corresponding position in the array*/
	//Making sure every file has its own buffer
	fcbArray[fileDes].fileBuffer = malloc(B_CHUNK_SIZE);
	fcbArray[fileDes].fi = fi;
	fcbArray[fileDes].fileDescriptor = fileDes;
	//the buffer index/pos/block starts at 0
	fcbArray[fileDes].bufferIndex = 0;
	fcbArray[fileDes].filePos = 0;
	fcbArray[fileDes].currentBlock = 0;

	//if file buffer is NULL, we know malloc failed
	if(fcbArray[fileDes].fileBuffer == NULL){
		perror("Allocation ERROR!");
		//exit from fatal error
		return -1;
	}

	//Want to get the file descriptor at the end
	return fileDes;

	}

// b_read functions just like its Linux counterpart read.  The user passes in
// the file descriptor (index into fcbArray), a buffer where thay want you to 
// place the data, and a count of how many bytes they want from the file.
// The return value is the number of bytes you have copied into their buffer.
// The return value can never be greater then the requested count, but it can
// be less only when you have run out of bytes to read.  i.e. End of File	
int b_read (b_io_fd fd, char * buffer, int count){
		
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}

	// and check that the specified FCB is actually in use	
	if (fcbArray[fd].fi == NULL)		//File not open for this descriptor
		{
		return -1;
		}

	//var to keep track of the number of bytes read
	//will increament and return in this function
	int numBytesRead = 0;
	//keep track of where we are in the input buffer
	int bufferPosition = 0;

	//count will always be > 0, based on how many bytes we're reading
	while(count > 0){

		/*setting the count variable to be the amount of character
		keeping in mind the file size, as well as where we are
		in the file*/
		if(fcbArray[fd].fi->fileSize <= count + fcbArray[fd].filePos){
			count = fcbArray[fd].fi->fileSize - fcbArray[fd].filePos;
		}

		//populating the file buffer
		if(fcbArray[fd].bufferIndex == 0){
			LBAread(fcbArray[fd].fileBuffer, 1, //get 1 block at a time
			/*enter the current file (fd), using the file info (fi),
			get the location on disc 
			and off set by how many blocks we're read thus far. */
			fcbArray[fd].fi->location + fcbArray[fd].currentBlock);
		}

		//when the count can "fit" in the file buffer:
		if(count <= B_CHUNK_SIZE-fcbArray[fd].bufferIndex){
			//copy from the file buffer location to the input buffer
			//offset by the position of buffer, so it does not over-write
			memcpy(buffer + bufferPosition,fcbArray[fd].fileBuffer 
			//offset based on where we are in the file buffer
			//so do not copy from the start of the file buffer
			+ fcbArray[fd].bufferIndex,
			//copy all number of counts as there is enough space
			count);

			//Increase the position of the file buffer,
			//based on how many counts we just read and copied
			fcbArray[fd].filePos += count;
			//as well as index (might not be necessary)
			fcbArray[fd].bufferIndex += count;
			//as well as the total number of counts we've read
			numBytesRead += count;
			//returning that total for output.
			return numBytesRead;
		}

		//if we reach here, count does not fit in file buffer
		else{
			/*keep track of space we DO have avaliable in file buffer
			Which is 512 - the buffer index
			as buffer index represents how many bytes we've read*/
			int spaceLeft = B_CHUNK_SIZE-fcbArray[fd].bufferIndex;

			//copy the remaining amount of input buffer to file buffer...
			memcpy(buffer + bufferPosition,
			fcbArray[fd].fileBuffer + fcbArray[fd].bufferIndex,
			//...based on how many space is avaliable
			spaceLeft);

			/*update the position in the file,
			by the amount we read, which is equal to 
			the amount of space we provided*/
			fcbArray[fd].filePos += spaceLeft;
			//increase the total bytes read
			numBytesRead += spaceLeft;
			//decrease the original count, by the amount we read
			count -= spaceLeft;
			//update the index in the buffer, so that next block
			//knows where to start, and not included what we already read
			bufferPosition += spaceLeft;
			//since all space in file buffer is read,
			//index gets reset to 0
			fcbArray[fd].bufferIndex = 0;
	
		}
		//continue the while loop.
		}
		return numBytesRead;
	}
	
// b_close frees and allocated memory and places the file control block back 
// into the unused pool of file control blocks.
int b_close (b_io_fd fd)
	{
		//free rsources
		b_fcb file = fcbArray[fd];
		file.fi = NULL;
		free(fcbArray[fd].fileBuffer);
	}