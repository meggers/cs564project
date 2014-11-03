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
	// TODO: Implement this method by looking at the description in the writeup.
}


const Status BufMgr::allocBuf(int & frame) {
	// TODO: Implement this method by looking at the description in the writeup.
	return OK;
}


const Status BufMgr::readPage(File* file, const int PageNo, Page*& page) {
	
	int frameNumber;
	BufDesc* existingFrame;
	Page* existingPage;
	Status pageStatus;
	
	pageStatus = hashTable->lookup(file, PageNo, frameNumber);
	
	switch(pageStatus)
	{
		case OK:
		{
			existingFrame = &(bufTable[frameNumber]);
			existingPage = &(bufPool[frameNumber]);
			existingFrame->refbit = true;
			existingFrame->pinCnt++;
			page = existingPage;
			break;		
		}
		case HASHNOTFOUND:
		{
			Status attempt = allocBuf(frameNumber); // Allocate space
			
			if(attempt != OK)
				return attempt; // Return the error that was thrown in allocBuf()
				
			file->readPage(PageNo, page); // Read in new page; Not sure about parameters here
			hashTable->insert(file, PageNo, frameNumber); // Insert page into the hashTable
			bufTable[frameNumber].Set(file, PageNo);
			page = &(bufPool[frameNumber]); // Not sure if this is the right address...
			break;
		}
		default:
		{
			return NOTUSED1;
		}
	}	
	
	return OK;
}


const Status BufMgr::unPinPage(File* file, const int PageNo, 
			       const bool dirty) {
	// TODO: Implement this method by looking at the description in the writeup.
	return OK;
}


const Status BufMgr::allocPage(File* file, int& pageNo, Page*& page)  {
	
	int frameNumber;
	Page* pagePtr;
	
	file->allocatePage(pageNo); // Get the pageNo that was allocated on disk
	Status try1 = allocBuf(frameNumber); // Get the frame number that was allocated
	
	if (try1 != OK)
		return try1; // Return the error that was thrown in allocBuf()
		
	Status try2 = hashTable->insert(file, pageNo, frameNumber); // Keep track of things
	
	if (try2 != OK)
		return try2; // Return the error that was thrown in insert()
	
	bufTable[frameNumber].Set(file, pageNo); // Set the frame
	pagePtr = &(bufPool[frameNumber]); // Return the pointer to the frame	
	page = pagePtr;
	
	return OK;
}


const Status BufMgr::disposePage(File* file, const int pageNo) {
	// TODO: Implement this method by looking at the description in the writeup.
	return OK;
}


const Status BufMgr::flushFile(const File* file) {
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


