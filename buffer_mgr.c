#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "buffer_mgr_stat.h"
#include <stdlib.h>
 
//global vars

typedef struct Page_Frame{
	PageNumber page_num;  // page number of the frame
	int page_dirty;       // indicates that this page is modified
	int fix_count;        // how many users are currently reading this page
	char *contents;       // data-contents of the page
} Page_Frame;

int num_files_read = 0; 
int num_files_written = 0;
int buffer_size = -1; 
int max_buffer_size = 2;
int front = 0;
bool buffer_full = false; 


// NAME: initBufferPool
// PURPOSE: initialize all elements of the buffer pool
// PARAMS: 
// - bm: buffer pool 
// - pageFileName: name of the file we will be writing to
// - numPages: number of pages in our buffer pool
// - strategy - replacement strategy
// - stratData - page handle to store read data into memory
// RETURN VAL: RC_OK
RC initBufferPool(BM_BufferPool *const bm, 
					const char *const pageFileName, 
					const int numPages, 
					ReplacementStrategy strategy, 
					void *stratData){

	// allocate empty buffer of size 'Page_Size'
    Page_Frame *pf = malloc(sizeof(Page_Frame)*(numPages));

	// initialize all buffer data to null
	if(pf == NULL){
		return RC_FILE_HANDLE_NOT_INIT;
	}
	else{
		for(int i = 0; i < numPages; i++){
			pf[i].page_num = -1;
			pf[i].page_dirty = 0; 
			pf[i].fix_count = 0; 
			pf[i].contents = NULL; 
		}
		bm->pageFile = (char *)pageFileName;
		bm->numPages = numPages;
		bm->strategy = strategy;
		bm->mgmtData = pf;

		return RC_OK;
	}
}

// NAME: shutdownBufferPool
// PURPOSE: frees up all memory associated with the buffer pool 
// PARAMS: 
// - bm: buffer pool to shut down
// RETURN VAL: RC_OK, RC_PAGE_NOT_FOUND
RC shutdownBufferPool(BM_BufferPool *const bm){

	Page_Frame *pf = (Page_Frame *)bm->mgmtData;

	// write all dirty pages back to disk
	forceFlushPool(bm);

	for(int i = 0; i < bm->numPages; i++)
	{
		// If fixCount != 0, it means that the contents of the page was modified by some client and has not been written back to disk.
		if(pf[i].fix_count > 0)
		{
			// return error code
			return RC_PAGE_NOT_FOUND;
		}
	}

	// Releasing space occupied by the page
	free(pf);
	bm->mgmtData = NULL;
	return RC_OK; 

}

// NAME: forceFlushPool
// PURPOSE: Writes all dirty pages back to disk 
// PARAMS: 
// - bm: buffer pool to shut down
// RETURN VAL: RC_OK
RC forceFlushPool(BM_BufferPool *const bm){

	Page_Frame *pf = (Page_Frame *) bm->mgmtData;
	SM_FileHandle fh; 

	for (int i = 0; i < bm->numPages; i++){

		//find the page the user is requesting
		if (pf[i].page_dirty != 0){

			//decrement fix count
			pf[i].page_dirty = 0; 
			openPageFile(bm->pageFile, &fh);
			writeBlock(pf[i].page_num, &fh, pf[i].contents);
			fclose(fh.mgmtInfo);
			num_files_written++;
		}
	} 
	return RC_OK; 
}

// NAME: markDirty
// PURPOSE: marks specified page number as dirty
// PARAMS: 
// - bm: active buffer pool
// - page: page we will be marking dirty
// RETURN VAL: RC_OK
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page){
	
	Page_Frame *pf = (Page_Frame *) bm->mgmtData;

	for (int i = 0; i < bm->numPages; i++){

		if (pf[i].page_num == page->pageNum){

			pf[i].page_dirty = 1; 
			return RC_OK; 
		}
	}
	return RC_OK; 
}

// NAME: unpinPage
// PURPOSE: decrements the fix count from specifed page
// PARAMS: 
// - bm: active buffer pool
// - page: page we will be unpinning
// RETURN VAL: RC_OK
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page){

	Page_Frame *pf = (Page_Frame *) bm->mgmtData;

	for (int i = 0; i < bm->numPages; i++){

		//find the page the user is requesting
		if (pf[i].page_num == page->pageNum){
			
			//decrement fix count
			pf[i].fix_count--; 
			break;
			
		}
	} 
	return RC_OK;
}

// NAME: forcePage
// PURPOSE: forces a page to be written to the disk
// PARAMS: 
// - bm: active buffer pool
// - page: page we will be writing to disk
// RETURN VAL: RC_OK, RC_FILE_NOT_FOUND
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page){

	Page_Frame *pf = (Page_Frame *) bm->mgmtData;
	SM_FileHandle fh; 

	for (int i = 0; i < bm->numPages; i++){

		//find the page the user is requesting
		if (pf[i].page_num == page->pageNum){

			//open page file
			openPageFile(bm->pageFile, &fh);

			// write the contents of the page
			writeBlock(pf[i].page_num, &fh, pf[i].contents);

			//close instance of page file to prevent memory leak
			fclose(fh.mgmtInfo);

			// mark page as clean
			pf[i].page_dirty = 0;  

			// increment the number of files written
			num_files_written++; 

			return RC_OK;
		}
	} 
	return RC_FILE_NOT_FOUND;
}

// NAME: pinPage
// PURPOSE: Pins page and fills up the buffer
// PARAMS: 
// - bm: active buffer pool
// - page: page handle
// - pageNum: page that we will be pinning
// RETURN VAL: RC_OK
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, 
		const PageNumber pageNum){
				
	Page_Frame *pf = (Page_Frame *) bm->mgmtData;
	SM_FileHandle fh;

	// ensureCapacity is 0 indexed, need to add a page to account
	// for this
	int pages_needed = pageNum + 1;

	//initialize buffer flag
	buffer_full = true; 

	for (int i = 0; i < bm->numPages; i++){

		if(pf[i].page_num != -1 && pf[i].page_num == pageNum){

			//set the page number to the page number in page frame
			page->pageNum = pf[i].page_num;

			//set the contents to the page contents in page frame
			page->data = pf[i].contents;

			//increment fix count
			pf[i].fix_count++;

			//buffer is not full
			buffer_full = false;
			break;

		}
		// empty page in frame
		else if (pf[i].page_num == -1){

			//buffer is not false
			buffer_full = false; 

			//instantiate page contents
			pf[i].contents = (SM_PageHandle)malloc(PAGE_SIZE); 
			
			//open file to read data from
			openPageFile(bm->pageFile, &fh);

			//ensure file has enough capacity for the frame
			ensureCapacity(pages_needed, &fh);

			//read empty data into page frame
			readBlock(pageNum, &fh, pf[i].contents);

			//close page file to prevent memory leak
			fclose(fh.mgmtInfo);

			//set new page content
			pf[i].page_num = pageNum;
			pf[i].fix_count++;

			//set passed in page vals to new contents
			page->pageNum = pageNum;
			page->data = pf[i].contents;

			//increment numbers of files read
			num_files_read++; 

			//increment buffer size
			buffer_size++; 

			break;
		}

	}
	//buffer is full
	if(buffer_full){

		// FIFO implementation
		if(bm->strategy == RS_FIFO){

			// make new new page frame
			Page_Frame *next_page = (Page_Frame *)malloc(sizeof(Page_Frame));

			//open file 
			openPageFile(bm->pageFile, &fh);

			//ensure another page is made
			ensureCapacity(pages_needed, &fh);

			//initialize the data field for the new page
			next_page->contents = (SM_PageHandle) malloc(PAGE_SIZE);

			//read empty page nums contents
			readBlock(pageNum, &fh, next_page->contents);

			//close file to prevent memory leak
			fclose(fh.mgmtInfo);

			//initialize the contents of the new page
			next_page->page_num = pageNum;
			next_page->page_dirty = 0; 
			next_page->fix_count = 1; 

			//page now points to the new data
			page->pageNum = pageNum; 
			page->data = next_page->contents;

			//initialize the page frame
			Page_Frame *pf = (Page_Frame *)bm->mgmtData;

			//get the front index
			front = front % buffer_size; 
			
			//replace the front index
			for(int i = 0; i < buffer_size; i++){

				// if nobody is using this page
				if(pf[front].fix_count == 0){
					
					//if page is modified, write to disk
					if(pf[front].page_dirty == 1){

						//open file
						openPageFile(bm->pageFile, &fh);

						//write dirty data
						writeBlock(pf[front].page_num, &fh, pf[front].contents);

						//close the file handle
						fclose(fh.mgmtInfo);

						//increment the number of files written
						num_files_written++; 
					}

					// set the index to the new pages data
					pf[front].contents = next_page->contents; 
					pf[front].page_num = next_page->page_num; 
					pf[front].page_dirty = next_page->page_dirty; 
					pf[front].fix_count = next_page->fix_count; 

					break; 

				}
				//if somebody is modifying this file, move to new index to update
				else{
					front++; 
					front = (front % buffer_size == 0) ? 0 : front;
				}
			}
		}		
	}

	return RC_OK;

}

// NAME: getFrameContents
// PURPOSE: Returns the pages found in the frames of the buffer pool
// PARAMS: 
// - bm: active buffer pool
// RETURN VAL: page numbers
PageNumber *getFrameContents (BM_BufferPool *const bm){

	PageNumber *page_numbers = malloc(sizeof(int)*bm->numPages);
	Page_Frame *pf = (Page_Frame *) bm->mgmtData;

	for (int i = 0; i < bm->numPages; i++){

		//if page info is not null
		if(pf[i].page_num != -1){

			page_numbers[i] = pf[i].page_num; 

		}
		//otherwise return NO_PAGE
		else{

			page_numbers[i] = NO_PAGE;

		}
	}
	//return pagenumber array
	return page_numbers; 
}

// NAME: getDirtyFlags
// PURPOSE: Returns the pages that are marked dirty in the buffer pool
// PARAMS: 
// - bm: active buffer pool
// RETURN VAL: page numbers
bool *getDirtyFlags (BM_BufferPool *const bm){
	
	bool *dirty_flags = malloc(sizeof(bool)*bm->numPages);
	Page_Frame *pf = (Page_Frame *) bm->mgmtData;

	for (int i = 0; i < bm->numPages; i++){

		if(pf[i].page_dirty != 0){

			dirty_flags[i] = true; 

		}
		else{

			dirty_flags[i] = false;

		}
	}
	//return dirty flags array
	return dirty_flags; 
}

// NAME: getFixCounts
// PURPOSE: Returns the number of users who are currently editing the pages
// PARAMS: 
// - bm: active buffer pool
// RETURN VAL: the number of users who are currently editing the pages
int *getFixCounts (BM_BufferPool *const bm){

	int *fix_counts = malloc(sizeof(int)*bm->numPages);
	Page_Frame *pf = (Page_Frame *) bm->mgmtData;

	int i = 0;

	// Iterating through all the pages in the buffer pool and setting fixCounts' value to page's fixCount
	for (int i = 0; i < bm->numPages; i++){
	
		fix_counts[i] = (pf[i].fix_count != -1) ? pf[i].fix_count : 0;

	}
	return fix_counts; 
}

// NAME: getNumReadIO
// PURPOSE: Returns the number of pages that have been read from the disk
// PARAMS: 
// - bm: active buffer pool
// RETURN VAL: the number of total reads
int getNumReadIO (BM_BufferPool *const bm){
	return num_files_read; 
}

// NAME: getNumWriteIO
// PURPOSE: Returns the number of pages that have been written to the disk
// PARAMS: 
// - bm: active buffer pool
// RETURN VAL: the number of total writes
int getNumWriteIO (BM_BufferPool *const bm){
	return num_files_written;
}