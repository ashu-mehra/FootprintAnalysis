#ifndef _SMAP_HPP__
#define _SMAP_HPP__
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>
#include "MemoryEntry.hpp"
/*
7fffbbdff000-7fffbbe00000 r-xp 00000000 00:00 0                          [vdso]
Size:                  4 kB
Rss:                   4 kB
Pss:                   0 kB
Shared_Clean:          4 kB
Shared_Dirty:          0 kB
Private_Clean:         0 kB
Private_Dirty:         0 kB
Referenced:            4 kB
Swap:                  0 kB
KernelPageSize:        4 kB
MMUPageSize:           4 kB
*/
class SmapEntry : public MemoryEntry
   {
   public:
      int _pss;
      int _sharedClean;
      int _sharedDirty;
      int _privateClean;
      int _privateDirty;
      int _swap;
      int _kernelPageSize;
      int _mmuPageSize;
      std::vector<bool> _inMemoryPages;
   public:
      SmapEntry() { clear(); }
      virtual void clear()
         {
         MemoryEntry::clear();
         _pss = 0; _sharedClean = 0; _sharedDirty = 0;
         _privateClean = 0; _privateDirty = 0; _swap = 0; _kernelPageSize = 0; _mmuPageSize = 0;
         }
      bool isMapForSharedLibrary() const;
      bool isMapForThreadStack() const;
      unsigned long long getPssSize() const { return _pss; } // result in KB
      bool isPageInMemory(uint64_t addr) const
	 {
         int pagesize = getpagesize();		 
         uint64_t pageAlignedAddr = addr & ~(pagesize-1);
         uint64_t start = _addrRange.getStart();
	 int index = (pageAlignedAddr - start) / pagesize;
	 return _inMemoryPages[index]; 
	 }
      uint64_t getInMemoryBytes(const AddrRange &range) const
         {
         return getInMemoryBytes(range.getStart(), range.getEnd());		 
         }		 
      uint64_t getInMemoryBytes(uint64_t start, uint64_t end) const
	 {
         uint64_t pageAlignedStart = start;
	 uint64_t pageAlignedEnd = end;
         int pagesize = getpagesize();		 
 	 uint64_t count = 0;

    	 if (start & (pagesize-1)) pageAlignedStart = (start + (pagesize-1)) & ~(pagesize-1); // round up start to next page boundary
	 if (end & (pagesize-1)) pageAlignedEnd = end & ~(pagesize-1); // round down end to page boundary

	 if (pageAlignedStart > pageAlignedEnd)
            {
            // this can happen if start and end are in same page
	    return end - start;
            }		    
	 uint64_t addr = pageAlignedStart;
	 while (addr < pageAlignedEnd)
            {
	    if (isPageInMemory(addr)) count += pagesize;
	    addr += pagesize;
            }
	 if (start != pageAlignedStart)
            {
            uint64_t page = start & ~(pagesize-1);
            if (isPageInMemory(page))
               {		    
	       count += (page + pagesize) - start;
	       }
            }
         if (end != pageAlignedEnd)
	    {
            uint64_t page = end & ~(pagesize-1);
            if (isPageInMemory(page))
               {		    
	       count += (end - page);
	       }
	    }
         return count;	 
	 }

      bool isPageCovered(uint64_t page) const
         {
         int pagesize = getpagesize();		 
         const std::list<const AddrRange*> coveringRanges = getCoveringRanges();
         for (auto seg = coveringRanges.cbegin(); seg != coveringRanges.cend(); ++seg)
            {
            uint64_t segStartPage = ((*seg)->getStart()) & ~(pagesize-1);
	    uint64_t segEndPage = ((*seg)->getEnd() + pagesize-1) & ~(pagesize-1);
	    if ((page >= segStartPage) && (page < segEndPage))
               return true;		       
	    }
	 return false;
         }

      uint64_t findMemoryNotCovered() const
         {
         int pagesize = getpagesize();		 
 	 uint64_t count = 0;
         uint64_t start = getAddrRange().getStart();
	 uint64_t end = getAddrRange().getEnd();
         uint64_t page = start;
	 // page should already be pagesizes aligned
         while (page < end)
	    {
            if (isPageInMemory(page) && !isPageCovered(page))
	       {
               count += pagesize;		        
	       }
            page += pagesize;	    
	    }
         return count;	 
         }		 
   protected:
      virtual void print(std::ostream& os) const;
   private:
      inline static bool endsWith(const std::string & str, const std::string & suffix)
         {
         return suffix.size() <= str.size() && std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
         }
   };


void readSmapsFile(const char *smapsFilename, const char *pagemapFilename, std::vector<SmapEntry>& smaps);
void readMapsFile(const char *smapsFilename, std::vector<SmapEntry>& smaps);
void createPageMapFile(const char *pagemapFilename, std::vector<SmapEntry>& smaps);
void getPageMapForSmapEntry(std::ifstream &pagemapFile, SmapEntry &entry);
void readPageMapFile();
void printLargestUnallocatedBlocks(const std::vector<SmapEntry> &smaps);
unsigned long long printSpaceKBTakenBySharedLibraries(const std::vector<SmapEntry> &smaps);
unsigned long long computeReservedSpaceKB(const std::vector<SmapEntry> &smaps);
void printTopTenReservedSpaceKB(const std::vector<SmapEntry> &smaps);
#endif // _SMAP_HPP__
