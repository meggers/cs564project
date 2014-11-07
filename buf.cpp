#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include "page.h"
#include "buf.h"

#define ASSERT(c)  { if (!(c)) { \
		       cerr << "At line " << __LINE__ << ":" << endl << "  "; \
                       cerr << "This condition should hold: " #c << endl; \
                       exit(1); \
		     } \
                   }

//----------------------------------------
// Constructor of the class BufMgr
//----------------------------------------

BufMgr::BufMgr(const int bufs)
{
    numBufs = bufs;

    bufTable = new BufDesc[bufs];
    memset(bufTable, 0, bufs * sizeof(BufDesc));
    for (int i = 0; i < bufs; i++) 
    {
        bufTable[i].frameNo = i;
        bufTable[i].valid = false;
    }

    bufPool = new Page[bufs];
    memset(bufPool, 0, bufs * sizeof(Page));

    int htsize = ((((int) (bufs * 1.2))*2)/2)+1;
    hashTable = new BufHashTbl (htsize);  // allocate the buffer hash table

    clockHand = bufs - 1;
}


BufMgr::~BufMgr() {
	
	BufDesc* currentFrame;
	Page* currentPage;
	int currentPageNo;	

	for(int i=0; i<numBufs; i++) // Loop through, unpinning everything
	{
		currentFrame = &bufTable[i]; // Retrieve the frame metadata
		currentFrame->pinCnt = 0; // 'Unpin' all the frames so that flushing wont fail
	}
	
	for(int i=0; i<numBufs; i++) // Loop through, flushing dirty pages
	{
		currentFrame = &bufTable[i]; // Retrieve the frame metadata
		currentPageNo = currentFrame->pageNo;
		currentPage = &bufPool[i]; // Retrieve the actual page of records
		
		if(currentFrame->dirty) // Write back to disk if dirty
		{
			(currentFrame->file)->writePage(currentPageNo, currentPage);
		}
		hashTable->remove(currentFrame->file, currentPageNo); // Remove from hashtable
	}	
	
	// Deallocate the space in memory
	memset(bufTable, 0, numBufs * sizeof(BufDesc));
	memset(bufPool, 0, numBufs * sizeof(Page));
}


const Status BufMgr::allocBuf(int & frame) {
	
	for(int i=0; i<numBufs*2; i++) // Loop over every frame twice (see post CID=145)
	{
		advanceClock();
		BufDesc* frameData = &bufTable[clockHand];
	
		if(frameData->valid) // Valid Set? YES
		{
			if(frameData->refbit) // refBit Set? YES
			{
				frameData->refbit = false; // Clear refbit
				continue; // Skip to the next frame
			}
			else // refBit Set? NO
			{
				if(frameData->pinCnt > 0) // Page Pinned? YES
				{
					continue; // Skip to the next frame
				}
				else // Page Pinned? NO
				{
					if(frameData->dirty) // Dirty Bit Set? YES
					{
						Status pageWrite = frameData->file->writePage(frameData->pageNo, dirtyPage);
						
						if(pageWrite != OK)
							return pageWrite;
						
						hashTable->remove(frameData->file, frameData->pageNo); // Remove the entry FIRST!	
						frameData->Clear(); // See post CID=162						
						frame = clockHand;
						return OK;
					}
					else // Dirty Bit Set? NO
					{
						frameData->Clear(); // See post CID=162
						frame = clockHand;
						return OK;
					}
				}
			}
		}
		else // Valid Set? NO
		{
			frameData->Clear(); // See post CID=162
			frame = clockHand;
			return OK;
		}
	}	

	return BUFFEREXCEEDED;
}


const Status BufMgr::readPage(File* file, const int PageNo, Page*& page) {
	
	int frameNumber;
	Status pageStatus;
	
	pageStatus = hashTable->lookup(file, PageNo, frameNumber);
	
	switch(pageStatus)
	{
		case OK:
		{
			bufTable[frameNumber].refbit = true;
			bufTable[frameNumber].pinCnt ++;
			page = &(bufPool[frameNumber]);
			break;		
		}
		case HASHNOTFOUND:
		{
			Status attempt = allocBuf(frameNumber); // Allocate space
			
			if(attempt != OK)
				return attempt; // Return the error that was thrown in allocBuf()
				
			attempt = file->readPage(PageNo, &bufPool[frameNumber]); // Read in new page; Not sure about parameters here

			if (attempt != OK)
				return attempt;

			attempt = hashTable->insert(file, PageNo, frameNumber); // Insert page into the hashTable

			if (attempt != OK)
				return attempt;

			bufTable[frameNumber].Set(file, PageNo);
			page = &bufPool[frameNumber]; // Not sure if this is the right address...
			break;
		}
		default:
		{
			return NOTUSED1;
		}
	}	
	
	return OK;
}


const Status BufMgr::unPinPage(File* file, const int PageNo, const bool dirty) {
	
	int frameNumber, oldCount;
	Status try1;
	BufDesc* frameData;
	
	try1 = hashTable->lookup(file, PageNo, frameNumber); // Retrieve the frameNumber from the hashtable
	
	if(try1 != OK)
		return try1; // Return the error thrown by the hashtable (HASHNOTFOUND)
	
	frameData = &bufTable[frameNumber]; // Get the frame metadata
	
	if(dirty)
		frameData->dirty = true;
	
	if(frameData->pinCnt <= 0) { // Check pin status
		return PAGENOTPINNED;
	}
	else
	{
		oldCount = frameData->pinCnt;
		frameData->pinCnt = oldCount - 1; // Decrement the pin count
		
	}

	return OK;
}


const Status BufMgr::allocPage(File* file, int& pageNo, Page*& page)  {
	
	int frameNumber;
	Page* pagePtr;
	
	Status try3 = file->allocatePage(pageNo); // Get the pageNo that was allocated in the file
	if(try3 != OK) return try3; // Return the error thrown by the file class (probably UNIXERR)
	
	Status try1 = allocBuf(frameNumber); // Get the frame number that was allocated in the buffer
	if (try1 != OK) return try1; // Return the error that was thrown in allocBuf()
		
	Status try2 = hashTable->insert(file, pageNo, frameNumber); // Keep track of things
	if (try2 != OK) return try2; // Return the error that was thrown in insert()
	
	bufTable[frameNumber].Set(file, pageNo); // Set the frame
	pagePtr = &(bufPool[frameNumber]); // Return the pointer to the actual data frame	
	page = pagePtr;
	
	return OK;
}


const Status BufMgr::disposePage(File* file, const int pageNo) {
	
	int frameNumber, framePage;
	Status try1, try2, try3; // Used for methods that return status'
	BufDesc* frameData;
	File* frameFile;	
	
	try1 = hashTable->lookup(file, pageNo, frameNumber);
	if (try1 != OK) return try1;

	frameData = &bufTable[frameNumber];
	frameFile = frameData->file;
	framePage = frameData->pageNo;
	
	try3 = hashTable->remove(frameFile, framePage); // Remove entry from the hashTable
	if (try3 != OK) return try3;

	try2 = frameFile->disposePage(framePage); // Dispose of the page on disk
	frameData->Clear(); // Wipe the frame clean
		
	return try2;
}


const Status BufMgr::flushFile(const File* file) {

	BufDesc* currentFrame;
	Page* currentPage;
	int currentPageNo;
	Status attempt;	

	for(int i=0; i<numBufs; i++)
	{
		currentFrame = &bufTable[i]; // Retrieve the frame metadata
		currentPageNo = currentFrame->pageNo;
		currentPage = &bufPool[i]; // Retrieve the actual page of records
				
		if(currentFrame->file == file) // Check if this frame belongs to this file
		{	
			if(currentFrame->pinCnt > 0)
			{
				return PAGEPINNED; // We can stop as soon as we realize there are still pages pinned
			}
			
			if(currentFrame->dirty) // Write back to disk if dirty
			{
				attempt = (currentFrame->file)->writePage(currentPageNo, currentPage);
				if (attempt != OK) return attempt;				

				currentFrame->dirty = false;
			}

			attempt = hashTable->remove(currentFrame->file, currentPageNo); // Remove from hashtable
			if (attempt != OK) return attempt;

			currentFrame->Clear(); // Clear the frame for use by another page
		}
	}

	return OK;
}


void BufMgr::printSelf(void) 
{
    BufDesc* tmpbuf;
  
    cout << endl << "Print buffer...\n";
    for (int i=0; i<numBufs; i++) {
        tmpbuf = &(bufTable[i]);
        cout << i << "\t" << (char*)(&bufPool[i]) 
             << "\tpinCnt: " << tmpbuf->pinCnt;
    
        if (tmpbuf->valid == true)
            cout << "\tvalid\n";
        cout << endl;
    };
}


