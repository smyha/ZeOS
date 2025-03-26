#ifndef MM_ADDRESS_H
#define MM_ADDRESS_H

#define ENTRY_DIR_PAGES       0

#define TOTAL_PAGES 1024
#define NUM_PAG_KERNEL 256  // Kernel size => 256 pages of 4KB = 1MB
#define PAG_LOG_INIT_CODE (PAG_LOG_INIT_DATA+NUM_PAG_DATA)  // 256 + 20 = 276
#define FRAME_INIT_CODE (PH_USER_START>>12) // 0x100000 >> 12 = 0x100 = 256
#define NUM_PAG_CODE 8
#define PAG_LOG_INIT_DATA (L_USER_START>>12)    // 0x100000 >> 12 = 0x100 = 256
#define NUM_PAG_DATA 20
#define PAGE_SIZE 0x1000    // 4096 bytes = 4KB = 2^12

/* Memory distribution */
/***********************/
#define KERNEL_START     0x10000        // Kernel code start 
#define L_USER_START        0x100000
#define USER_ESP	L_USER_START+(NUM_PAG_DATA)*0x1000-16   
/*USER_ESP = L_USER_START + NUM_PAG_DATA*0x1000 - 16
           = 0x100000 + 20*0x1000 - 16
           = 0x100000 + 0x50000 - 16
           = 0x150000 - 16
           = 0x150000 - 0x10
           = 0x14FFEC 
*/
#define USER_FIRST_PAGE	(L_USER_START>>12)      // 0x100000 >> 12 = 0x100 = 256

#define PH_PAGE(x) (x>>12)

#endif

