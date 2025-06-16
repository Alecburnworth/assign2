EXECUTING: To run this program, compile the code via make file. Simpy run the command "make" in your terminal where this repository has been stored. Once compiled, running .\test_assign2.exe will execute all of the test cases and return a log in the terminal denoting if they have passed or failed.

ABOUT THE SOLUTION: 
This program is the implementation to supply a user a buffer manager that provides them to read and write pages from a disk. The buffer is of fixed length frames, which hold individual pages from a disk. 

IMPLEMENTATION: To utilitze the member functions of this file, you must call the initialize function. Once initialized, the user has the ability to read and write data via this buffer manager to a file on disk. 

Main member functions that the user will interact with:
pinPage - allows the user to request a specific page from the page file to be loaded in the buffer to be modified. 
markDirty - the user calls this to inform the buffer pool that they have modified a page, the user must call this in order for their page to be written to disk. 
unpinPage - the user calls this after they are done with editing the page. 
shutdownBufferPool - this function is called to fully clear out the buffer pool. This will destroy all memory associated with the pool as well as write all pages to disk. 

CONTRIBUTORS: 
- Alec Burnworth: author, presenter, implementer
