#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "storage_mgr.h"
#include "dberror.h"
#include "test_helper.h"

// test name
char *testName;

/* test output files */
#define TESTPF "test_pagefile.bin"

/* prototypes for test functions */
static void testCreateOpenClose(void);
static void testSinglePageContent(void);

/* main function running all tests */
int
main (void)
{
  testName = "";
  
  initStorageManager();

  testCreateOpenClose();
  testSinglePageContent();

  return 0;
}


/* check a return code. If it is not RC_OK then output a message, error description, and exit */
/* Try to create, open, and close a page file */
void
testCreateOpenClose(void)
{
  SM_FileHandle fh;

  testName = "test create open and close methods";

  TEST_CHECK(createPageFile (TESTPF));
  
  TEST_CHECK(openPageFile (TESTPF, &fh));
  ASSERT_TRUE(strcmp(fh.fileName, TESTPF) == 0, "filename correct");
  ASSERT_TRUE((fh.totalNumPages == 1), "expect 1 page in new file");
  ASSERT_TRUE((fh.curPagePos == 0), "freshly opened file's page position should be 0");

  TEST_CHECK(closePageFile (&fh));
  TEST_CHECK(destroyPageFile (TESTPF));

   // after destruction trying to open the file should cause an error
  ASSERT_TRUE((openPageFile(TESTPF, &fh) != RC_OK), "opening non-existing file should return an error.");

  TEST_DONE();
}

/* Try to create, open, and close a page file */
void
testSinglePageContent(void)
{
  SM_FileHandle fh;
  SM_PageHandle ph;
  int i;

  testName = "test single page content";

  ph = (SM_PageHandle) malloc(PAGE_SIZE);

  // create a new page file
  TEST_CHECK(createPageFile (TESTPF));
  TEST_CHECK(openPageFile (TESTPF, &fh));
  printf("created and opened file\n");
  
  // read first page into handle
  TEST_CHECK(readFirstBlock (&fh, ph));
  // the page should be empty (zero bytes)
  for (i=0; i < PAGE_SIZE; i++){
    ASSERT_TRUE((ph[i] == 0), "expected zero byte in first page of freshly initialized page");
  }
  printf("first block was empty\n");
    
  // change ph to be a string and write that one to disk
  for (i=0; i < PAGE_SIZE; i++)
    ph[i] = (i % 10) + '0';
  TEST_CHECK(writeBlock (0, &fh, ph));
  printf("writing first block\n");

  // read back the page containing the string and check that it is correct
  TEST_CHECK(readFirstBlock (&fh, ph));
  for (i=0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == (i % 10) + '0'), "character in page read from disk is the one we expected.");
  printf("reading first block\n");

  // add empty block
  TEST_CHECK(appendEmptyBlock (&fh));
  printf("Created new block\n");

  // page count should have increased to 2
  ASSERT_TRUE((fh.totalNumPages == 2), "Capacity has been increased to 2 pages");

  // read next page block into handle
  TEST_CHECK(readNextBlock (&fh, ph));
  // the page should be empty (zero bytes)
  for (i=0; i < PAGE_SIZE; i++){
    ASSERT_TRUE((ph[i] == 0), "expected zero byte in first page of freshly initialized page");
  }
  printf("new block we just added is empty\n");

  // get current position in file
  ASSERT_TRUE((getBlockPos(&fh) == 1), "Check that we are on page 2");

  // read back the page containing the string and check that it is correct
  TEST_CHECK(readPreviousBlock (&fh, ph));
  for (i=0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((ph[i] == (i % 10) + '0'), "character in page read from disk is the one we expected.");
  printf("reading previous block\n");

  // get current position in file
  ASSERT_TRUE((getBlockPos(&fh) == 0), "Check that we are back on page 1");

  // read back the page containing the string and check that it is correct
  TEST_CHECK(ensureCapacity (10, &fh));
  ASSERT_TRUE((fh.totalNumPages == 10), "Capacity has been increased to desired page length");

  // change ph to be a string and write that one to disk
  for (i=0; i < 10; i++)
    ph[i] = (i) + '0';
  TEST_CHECK(writeBlock (9, &fh, ph));
  printf("writing data on 10th page\n");

  // read back the page containing the string and check that it is correct
  TEST_CHECK(readBlock (9, &fh, ph));
  for (i=0; i < 10; i++){
    ASSERT_TRUE((ph[i] == (i) + '0'), "character in page 10 read from disk is the one we expected.");
  }
  printf("reading 10th page\n");

  // get current position in file
  ASSERT_TRUE((getBlockPos(&fh) == 9), "Check that we are still on page 10");

  // write different characters onto current block(page 10)
  for (i=0; i < 20; i++)
    ph[i] = (i % 10) + '0';
  TEST_CHECK(writeCurrentBlock (&fh, ph));
  printf("writing current block\n");

  // read back page 10's data and ensure it was updated with new characters 
  TEST_CHECK(readCurrentBlock (&fh, ph));
  for (i=0; i < 20; i++){
    ASSERT_TRUE((ph[i] == (i % 10) + '0'), "character in page 10 read from disk is the one we expected.");
  }
  printf("reading current page\n");

  // close file
  TEST_CHECK(closePageFile (&fh));

  // open up file again
  TEST_CHECK(openPageFile (TESTPF, &fh));
  printf("opened file\n");

  // check that file data has not been wiped despite closing
  ASSERT_TRUE((fh.totalNumPages == 10), "Page data has been stored despite closing file");

  // read back page 10's data and ensure it hasn't been wiped 
  TEST_CHECK(readCurrentBlock (&fh, ph));
  for (i=0; i < 20; i++){
    ASSERT_TRUE((ph[i] == (i % 10) + '0'), "character in page 10 read from disk is the one we expected.");
  }
  printf("reading current page\n");

  // destroy new page file
  TEST_CHECK(destroyPageFile (TESTPF));  
  
  TEST_DONE();
}
