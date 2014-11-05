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

/*
Allocates a free frame using the clock algorithm; if necessary, writing a dirty page back to disk. Returns BUFFEREXCEEDED if all buffer frames are pinned, UNIXERR if the call to the I/O layer returned an error when a dirty page was being written to disk and OK otherwise. This private method will get called by the readPage() and allocPage() methods described below.

Make sure that if the buffer frame allocated has a valid page in it, that you remove the appropriate entry from the hash table.
*/
const Status BufMgr::allocBuf(int & frame) {

	int state = ADVANCE_CLOCK;
	while (state != DONE) {
		switch (state) {
			case ADVANCE_CLOCK:
				clockHand = (clockHand + 1) % numBufs;
				state = bufTable[clockHand].valid ? REF_CHECK : WRITE_BACK;
				break;

			case REF_CHECK:
				if (bufTable[clockHand].refbit) {
					bufTable[clockHand].refbit = false;
					state = ADVANCE_CLOCK;
				} else {
					state = PINNED_CHECK;
				}

				break;

			case PINNED_CHECK:
				state = (bufTable[clockHand].pinCnt > 0) ? ADVANCE_CLOCK : DIRTY_CHECK;
				break;

			case DIRTY_CHECK:
				state = bufTable[clockHand].dirty ? FLUSH_PAGE : DONE;
				break;

			case FLUSH_PAGE:
				//wtf
				Status err = bufTable[clockHand].file->writePage( , );
				break;

			case DONE:
			case default:
				state = DONE;
				break;
		}
	}

	// ALLOCATE FREE FRAME!!!!!!!!!!!!!!!!!!!

	return OK;
}

	
const Status BufMgr::readPage(File* file, const int PageNo, Page*& page) {
	// TODO: Implement this method by looking at the description in the writeup.
	return OK;
}

/*
Decrements the pinCnt of the frame containing (file, PageNo) and, if dirty == true, sets the dirty bit. Returns OK if no errors occurred, HASHNOTFOUND if the page is not in the buffer pool hash table, PAGENOTPINNED if the pin count is already 0.
*/
const Status BufMgr::unPinPage(File* file, const int PageNo, const bool dirty) {
	int frameNo;

	Status status = hashTable->lookup(file, PageNo, frameNo);
	if (status != OK)
		return status;

	if (bufTable[frameNo].pinCnt == 0) {
		return PAGENOTPINNED;
	} else {
		if (dirty)
			bufTable[frameNo].dirty = true;

		bufTable[frameNo].pinCnt --;
	}

	return OK;
}


const Status BufMgr::allocPage(File* file, int& pageNo, Page*& page)  {
	// TODO: Implement this method by looking at the description in the writeup.
	return OK;
}

/*
Check to see if the page actually exists in the buffer pool by looking it up in the hash table. If it does exists, clear the page, remove the corresponding entry from the hash table and dispose the page in the file as well. Return the status of the call to dispose the page in the file.
*/
const Status BufMgr::disposePage(File* file, const int pageNo) {
	int frameNo;

	// Check to see if page exists in buffer pool
	Status found = hashTable->lookup(file, pageNo, frameNo);
	if (found != OK)
		return found;

	// Clear the page
	bufTable[frameNo]->Clear();

	return OK;
}


const Status BufMgr::flushFile(const File* file) {
	// TODO: Implement this method by looking at the description in the writeup.
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


