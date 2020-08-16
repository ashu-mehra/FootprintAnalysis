// FootprintAnalysis.cpp : Defines the entry point for the console application.
//
#include <vector>
#include <iostream>
#include <unordered_map>
#include <set>
#include <utility> // for std::pair
#include <algorithm> // for sort
#include <string.h>
#include "smap.hpp"
#include "CallSites.hpp"
#include "Util.hpp"
#include "vmmap.hpp"
//#define WINDOWS_FOOTPRINT
using namespace std;

template <typename MAPENTRY, typename T>
void annotateMapWithSegments(std::vector<MAPENTRY>&maps, std::vector<T>& segments)
   {
   // Annonate maps with j9segments
   cout << "Annotate maps with segments ...";
   for (auto map = maps.begin(); map != maps.end(); ++map)
      {
      for (auto seg = segments.begin(); seg != segments.end(); ++seg)
         {
         if (seg->disjoint(map->getAddrRange()))
            continue;
         if (map->getAddrRange().includes(*seg))
            map->addCoveringRange(*seg);
	 else if ((*seg).includes(map->getAddrRange()) || (*seg).overlaps(map->getAddrRange()))
	    map->createCoveringRange(*seg);
         else
            map->addOverlappingRange(*seg);
         }
      }
   cout << "Done\n";
   }

template <typename MAPENTRY>
unsigned long long printSpaceKBTakenBySharedLibraries(const vector<MAPENTRY> &smaps)
   {
   unsigned long long spaceTakenBySharedLibraries = 0;
   TopTen<MAPENTRY, MemoryEntrySizeLessThan> topTen;
   for (auto crtMap = smaps.cbegin(); crtMap != smaps.cend(); ++crtMap)
      {
      if (crtMap->isMapForSharedLibrary())
         {
         spaceTakenBySharedLibraries += crtMap->sizeKB();
         topTen.processElement(*crtMap);
         }
      }
   cout << "Total space taken by shared libraries: " << dec << spaceTakenBySharedLibraries << " KB\n";
   topTen.print();
   return spaceTakenBySharedLibraries;
   }

enum RangeCategories {
   GCHEAP = 0,
   CODECACHE,
   DATACACHE,
   DLL,
   STACK,
   SCC,
   SCRATCH,
   PERSIST,
   OTHER_INTERNAL,
   CLASS,
   CALLSITE_OPENJ9,
   CALLSITE_OTHERS,
   OVERHEAD,
   UNKNOWN,
   NOTCOVERED,
   NUM_CATEGORIES, // Must be the last one
   };

RangeCategories getJ9SegmentCategory(const J9Segment* j9seg)
   {
   switch (j9seg->getSegmentType())
      {
      case J9Segment::HEAP:
         return GCHEAP;
         break;
      case J9Segment::CODECACHE:
         return CODECACHE;
         break;
      case J9Segment::DATACACHE:
         return DATACACHE;
         break;
      case J9Segment::INTERNAL:
         if (j9seg->isJITScratch())
            return SCRATCH;
         else if (j9seg->isJITPersistent())
            return PERSIST;
         else
            return OTHER_INTERNAL;
         break;
      case J9Segment::CLASS:
         return CLASS;
         break;
      default:
         return UNKNOWN;
      }; // end switch
   }

RangeCategories getCallSiteCategory(const CallSite *callsite)
   {        
   RangeCategories category;	   
   switch (callsite->getSource())
      {
      case CallSite::CALLSITE_OPENJ9:
         category = CALLSITE_OPENJ9;
	 break;
      case CallSite::CALLSITE_OTHERS:
         category = CALLSITE_OTHERS;
	 break;
      default:
         return UNKNOWN;
      }
   return category;   
   }	   

template <typename MAPENTRY>
void printSpaceKBTakenByVmComponents(const vector<MAPENTRY> &smaps)
   {
   cout << "\nprintSpaceKBTakenByVmComponents...\n";

   const char* RangeCategoryNames[NUM_CATEGORIES] = { "GC heap", "CodeCache", "DataCache", "DLL", "Stack", "SCC", "JITScratch", "JITPersist", "Internal", "Classes", "CallSites_OpenJ9", "CallSites_Others", "Overhead", "Unknown", "Not covered" };
   // categories of covering ranges
   unsigned long long virtualSize[NUM_CATEGORIES]; // one entry for each category
   unsigned long long rssSize[NUM_CATEGORIES]; // one entry for each category
   unsigned long long pssSize[NUM_CATEGORIES]; // one entry for each category
   unsigned long long sz[NUM_CATEGORIES]; // one entry for each category
   unsigned long long rssSizePerSmapEntry[NUM_CATEGORIES]; // one entry for each category
   for (int i = 0; i < NUM_CATEGORIES; i++)
      virtualSize[i] = rssSize[i] = pssSize[i] = 0;
   //unsigned long long rss[NUM_CATEGORIES];


   unsigned long long totalVirtSize = 0;
   unsigned long long totalRssSize = 0;
   unsigned long long totalPssSize = 0;



   TopTen<MAPENTRY, MemoryEntryRssLessThan> topTenDlls;

   TopTen<MAPENTRY, MemoryEntryRssLessThan> topTenNotCovered;

   TopTen<MAPENTRY, MemoryEntryRssLessThan> topTenPartiallyCovered;

   unordered_map<string, unsigned long long> dllCollection;

   unordered_map<string, uint64_t> otherCallsitesMap;

   // Iterate through all smaps/vmmaps
   for (auto crtMap = smaps.cbegin(); crtMap != smaps.cend(); ++crtMap)
      {
      totalVirtSize += crtMap->size();
      totalRssSize += crtMap->getResidentSize() << 10;
      totalPssSize += crtMap->getPssSize() << 10;

      // Check if shared library
      if (crtMap->isMapForSharedLibrary())
         {
         virtualSize[DLL] += crtMap->size();
         rssSize[DLL] += crtMap->getResidentSize() << 10;
         pssSize[DLL] += crtMap->getPssSize() << 10;
         topTenDlls.processElement(*crtMap);

         // Note that in Linux a DLL may have 3 smaps. e.g.
         // Size = 11968 rss = 11136 Prot = r-xp / home / jbench / mpirvu / JITDll_gcc / libj9jit28.so
         // Size = 960   rss = 256   Prot = r--p / home / jbench / mpirvu / JITDll_gcc / libj9jit28.so
         // Size = 448   rss = 448   Prot = rw-p / home / jbench / mpirvu / JITDll_gcc / libj9jit28.so
         // We want to sum-up all contributions for the same DLL. Thus let's create a hashtable
         // that accumulates the sums (key is the name of the DLL, value is the total RSS)
         // Then we need to sort by the total RSS
         //
         auto& dllTotalRSSSize = dllCollection[crtMap->getDetailsString()]; // If key does not exist, it will be inserted
         dllTotalRSSSize += crtMap->getResidentSize() << 10;
         continue; // DLL maps are not shared with other categories
         }

      // Check if shared class cache. Find "javasharedresources" in the details string
      if (crtMap->getDetailsString().find("javasharedresources") != string::npos || // found
         crtMap->getDetailsString().find("classCache") != string::npos)
         {
         virtualSize[SCC] += crtMap->size();
         rssSize[SCC] += crtMap->getResidentSize() << 10;
         pssSize[SCC] += crtMap->getPssSize() << 10;
         continue; // SCC maps are not shared with other categories (even though classes can reside here)
         }

      // Thread stacks
      if (crtMap->isMapForThreadStack()) 
         {
         virtualSize[STACK] += crtMap->size();
         rssSize[STACK] += crtMap->getResidentSize() << 10;
         pssSize[STACK] += crtMap->getPssSize() << 10;
         continue; // Stack maps are not shared with other categories
         }

      for (int i = 0; i < NUM_CATEGORIES; i++)
         {	      
         sz[i] = 0;
	 rssSizePerSmapEntry[i] = 0;
	 }

      cout<<"Processing smap entry :" << *crtMap << "\n";
      // look for any covering segments
      const std::list<const AddrRange*> coveringRanges = crtMap->getCoveringRanges();
      const std::list<const AddrRange*> overlapRanges = crtMap->getOverlappingRanges();
      if (coveringRanges.size() != 0 && overlapRanges.size() != 0)
         {
         cerr << "Warning: smap starting at addr " << crtMap->getAddrRange().getStart() << " has both covering and overlapping ranges\n";
         }
      bool useProportionalAllocation = false;
      if (crtMap->_inMemoryPages.size() == 0) useProportionalAllocation = true;
      unsigned long long totalCoveredSize = 0;
      unsigned long long totalRssCoveredSize = 0;
      for (auto seg = coveringRanges.cbegin(); seg != coveringRanges.cend(); ++seg)
         {
         if ((*seg)->rangeType() == AddrRange::J9SEGMENT_RANGE) // exclude callsites
            {
            // Convert the range to a segment
            const J9Segment* j9seg = static_cast<const J9Segment*>(*seg);
            // Determine the type of the segment
            RangeCategories category = getJ9SegmentCategory(j9seg);
            if (category != UNKNOWN)
               {
               virtualSize[category] += j9seg->size();
               sz[category] += j9seg->size();
               if (!useProportionalAllocation)
                  {
	          uint64_t rssUsed = crtMap->getInMemoryBytes(**seg);
                  rssSizePerSmapEntry[category] += rssUsed;
		  cout << "RSS memory for addr range (" << RangeCategoryNames[category] << "): " << **seg << " : is " << dec << rssUsed << " bytes\n";
	          }
               }
            }
         else // This is a callsite
            {
            // Convert the range to a callsite
            const CallSite* callsite = static_cast<const CallSite*>(*seg);
	    RangeCategories category = getCallSiteCategory(callsite);
            virtualSize[category] += callsite->size();
            sz[category] += callsite->size();
            if (!useProportionalAllocation)
               {
	       uint64_t rssUsed = crtMap->getInMemoryBytes(**seg);
               rssSizePerSmapEntry[category] += rssUsed;
	       cout << "RSS memory for addr range (callsite): " << **seg << " : is " << dec << rssUsed << " bytes\n";
	       if (category == CALLSITE_OTHERS)
                  {
                     auto& callsiteSize = otherCallsitesMap[callsite->getFilename()]; // If key does not exist, it will be inserted
                     callsiteSize += rssUsed;
                  }		     
	       }
            }
         } // end for

      // Now look whether a map is covered by ranges of different types and assign RSS in proportional values
      int numDifferentCategories = 0;
      int lastNonNullCategory = 0;
      for (int i = 0; i < NUM_CATEGORIES; i++)
         {
         if (sz[i] > 0)
            {
            numDifferentCategories++;
            lastNonNullCategory = i;
            }
	 if (!useProportionalAllocation)
            {		 
   	    rssSize[i] += rssSizePerSmapEntry[i];
	    if (rssSizePerSmapEntry[i] > 0)
               {		 
               cout << "RSS memory for category " << RangeCategoryNames[i] << " is " << dec << rssSizePerSmapEntry[i] << " bytes\n";
	       }
	    totalRssCoveredSize += rssSizePerSmapEntry[i];
	    }
         totalCoveredSize += sz[i];
         }

      if (totalCoveredSize > 0)
         {
         if (!useProportionalAllocation)
            {
	    rssSizePerSmapEntry[UNKNOWN] = crtMap->findMemoryNotCovered();
	    rssSize[UNKNOWN] += rssSizePerSmapEntry[UNKNOWN];
	    //cout << "RSS memory not covered by any segment pages: " << dec << unknown << " bytes\n";
            if ((crtMap->getResidentSize() << 10) > totalRssCoveredSize)
               {		    
               rssSizePerSmapEntry[OVERHEAD] += (crtMap->getResidentSize() << 10) - totalRssCoveredSize - rssSizePerSmapEntry[UNKNOWN];
	       rssSize[OVERHEAD] += rssSizePerSmapEntry[OVERHEAD];
	       }
   	    cout << "RSS memory for category Overhead is " << dec << rssSizePerSmapEntry[OVERHEAD] << " bytes\n";
   	    cout << "RSS memory for category Unknown is " << dec << rssSizePerSmapEntry[UNKNOWN] << " bytes\n";
   	    virtualSize[UNKNOWN] += crtMap->size() - totalCoveredSize;

	    for (int i = 0; i < NUM_CATEGORIES; i++)
               {
               if ((rssSizePerSmapEntry[i] > 0) && (crtMap->getResidentSize() > 0))
                  {
                  // this is assuming that the rss of categories is not shareable across processes.
	          // if it is shareable then we need to get page level sharing information from /proc/kpagecount to get accurate pss.
	          uint64_t pss = rssSizePerSmapEntry[i] * (crtMap->getPssSize() << 10) / (crtMap->getResidentSize() << 10); // make PSS proportial to category's RSS size
		  cout << "PSS memory for category " << RangeCategoryNames[i] << ": " << pss << " bytes\n";
		  pssSize[i] += pss;
                  }
	       }
	    }
	 else
            {
            // If the map is covered by segments of the same type
            // then we can charge the entire RSS to that type
            if (numDifferentCategories == 1)
               {
               rssSize[lastNonNullCategory] += (crtMap->getResidentSize() << 10);
	       cout << "Adding rss size of " << crtMap->getResidentSize() << " KB to " << "category " << RangeCategoryNames[lastNonNullCategory] << "\n";
               pssSize[lastNonNullCategory] += (crtMap->getPssSize() << 10);
               }
            else // Proportional allocation
               {
   	       cout<<"Performing Proportional allocation for smap entry: " << *crtMap;
	       cout<<"\n";
               for (int i = 0; i < NUM_CATEGORIES; i++)
                  {
                  // TODO: must do some rounding
                  unsigned long long l = (crtMap->getResidentSize() << 10) * sz[i] / crtMap->size();
	          cout << "Adding rss size of " << (l >> 10) << " KB to " << "category " << RangeCategoryNames[i] << "\n";
                  rssSize[i] += l;
   	          pssSize[i] += ((crtMap->getPssSize() << 10) * sz[i] / crtMap->size());
                  }
	       cout << "Total covered size: " << totalCoveredSize << "\n";
               rssSize[UNKNOWN] += (crtMap->getResidentSize() << 10) * (crtMap->size() - totalCoveredSize) / crtMap->size();
   	       cout << "Adding rss size of " << (crtMap->getResidentSize() *  (crtMap->size() - totalCoveredSize) / crtMap->size()) << " KB to " << "category UNKNOWN" << "\n";
               virtualSize[UNKNOWN] += crtMap->size() - totalCoveredSize;
	       pssSize[UNKNOWN] += ((crtMap->getPssSize() << 10) * (crtMap->size() - totalCoveredSize) / crtMap->size());
               topTenPartiallyCovered.processElement(*crtMap);
               }
            }
	 }
      else
         {
         // Does it have overlapping ranges?
         // An overlapping range could happen if smaps are gathered first and then by the time
         // we collect the javacore the GC expands which makes the GC segment in the javacore
         // be larger than the smap.
         if (overlapRanges.size() != 0)
            {
            // If the j9segment totally overlaps the smap, then we know the exact type
            // of j9memory for that smap and the entire RSS can be attributed to that type.
            // We could relax this heuristic: if the overlapping segments for this smap are of the same kind
            // then we can guess the kind of memory for the smap.
            RangeCategories smapCategory = UNKNOWN;
            for (auto seg = overlapRanges.cbegin(); seg != overlapRanges.cend(); ++seg)
               {
               if ((*seg)->rangeType() == AddrRange::J9SEGMENT_RANGE) // exclude callsites
                  {
                  // Convert the range to a segment
                  const J9Segment* j9seg = static_cast<const J9Segment*>(*seg);

                  // Determine the type of the segment
                  RangeCategories segCategory = getJ9SegmentCategory(j9seg);
                  if (segCategory == UNKNOWN)
                     {
                     // Type of segment is not known, so type of smap is unknown
                     smapCategory = UNKNOWN;
                     break;
                     }
                  else
                     {
                     if (smapCategory == UNKNOWN) // Not yet set
                        {
                        smapCategory = segCategory;
                        }
                     else // Different types of segments. Cannot determine the type of smap
                        {
                        smapCategory = UNKNOWN;
                        break;
                        }
                     }
                  }
               else // callsites
                  {
                  // Theoretically we could handle these cases as well, but it's unlikely they occur
                  smapCategory = UNKNOWN; 
                  break;
                  }
               }
	    cout << "Adding rss size of " << crtMap->getResidentSize() << " KB to " << "category " << RangeCategoryNames[smapCategory] << "\n";
            rssSize[smapCategory] += (crtMap->getResidentSize() << 10);
            pssSize[smapCategory] += (crtMap->getPssSize() << 10);
            virtualSize[smapCategory] += crtMap->size();
            if (smapCategory == UNKNOWN)
               {
               cout << "smap with different/unknown segments that are not totaly included in this smap\n";
               topTenPartiallyCovered.processElement(*crtMap);
               }
            }
         else
            {
            cout << "Overlapping list is empty. Adding the rss to NOTCOVERED category.\n";
            rssSize[NOTCOVERED] += (crtMap->getResidentSize() << 10);
	    pssSize[NOTCOVERED] += (crtMap->getPssSize() << 10);
            virtualSize[NOTCOVERED] += crtMap->size();
            topTenNotCovered.processElement(*crtMap);
            }
         }
      } // end for (iterate through maps)
   cout << dec << endl;
   cout << "Totals:                Virtual= " << setw(8) << (totalVirtSize >> 10) << " KB; RSS= " << setw(8) << (totalRssSize >> 10) << " KB; PSS= " << setw(8) << (totalPssSize >> 10) << " KB\n";
   for (int i = 0; i < NUM_CATEGORIES; i++)
      {
      cout << setw(20) << RangeCategoryNames[i] << ":  Virtual= " << setw(8) << (virtualSize[i] >> 10) << " KB; RSS= " << setw(8) << (rssSize[i] >> 10) << " KB; PSS= " << setw(8) << (pssSize[i] >> 10) << " KB\n";
      }

   // Print explanation
   cout << endl;
   cout << "Unknown portion comes from maps that are partially covered by segments and callsites" << endl;
   cout << "'Not covered' are maps that are really not covered by any segment or callsite" << endl;

   // Print the callsite information for non-openj9 libraries
   typedef std::function<bool(std::pair<std::string, uint64_t>, std::pair<std::string, uint64_t>)> Comparator;
   Comparator comparator = [](std::pair<std::string, uint64_t> element1, std::pair<std::string, uint64_t> element2)
     {
     return element1.second > element2.second;	     
     };

   set<std::pair<std::string, uint64_t>, Comparator> otherCallsitesSet(otherCallsitesMap.begin(), otherCallsitesMap.end(), comparator);
   
   cout << "\n Allocations by non-openj9 dlls\n";
   for (auto &pair : otherCallsitesSet)
     {
     cout << dec << setw(8) << (pair.second >> 10) << " KB   " << pair.first << endl;
     }

   //
   // Process the hashtable with DLLs
   //
   using PairStringULL = pair < string, unsigned long long >;
   // Ideally, above I would use something like  decltype(dllCollection)::value_type
   // which is of type <const string, unsigned long long>
   // However, the sort algorithm used below uses operator = and the const in front of the
   // string creates problems when moving elements

   // Take all elements from the hashtable and put them into a vector
   vector<PairStringULL> vectorWithDlls(dllCollection.cbegin(), dllCollection.cend());

   
   // Sort the vector using a custom comparator (lambda) that knows how to
   // compare pairs (want to sort based on value of the map entry)
   //
   sort(vectorWithDlls.begin(), vectorWithDlls.end(),
        [](decltype(vectorWithDlls)::const_reference p1, decltype(vectorWithDlls)::const_reference p2) 
           { return p1.second > p2.second; } // compare using the second element of the pair  
       );

   // print the most expensive entries
   cout << "\n RSS of dlls\n";
   for_each(vectorWithDlls.cbegin(), vectorWithDlls.cend(), 
            [](decltype(vectorWithDlls)::const_reference element)
                {cout << dec << setw(8) << (element.second >> 10) << " KB   " << element.first << endl; }
           );

   cout << "\nTop 10 DDLs based on RSS:\n";
   topTenDlls.print();

   cout << "\nTop 10 maps not covered by anything\n";
   topTenNotCovered.print();

   cout << "\nTop 10 maps partially covered\n";
   topTenPartiallyCovered.print();
   }



#ifdef WINDOWS_FOOTPRINT
typedef VmmapEntry MapEntry; // For Windows
#else
typedef SmapEntry MapEntry;  // For Linux
#endif

static char * 
getOption(int argc, char *argv[], const char *option)
{
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], option)) {
			return argv[i+1];
		}
	}
	return NULL;
}

int main(int argc, char* argv[])
   {
   // Verify number of arguments
   if (argc < 2)
      error("Need at least one argument: smaps file, javacore and callsites are optional");

   char *smapsFile = getOption(argc, argv, "-s"); 
   char *pageMapFile = getOption(argc, argv, "-p");
   char *javacoreFile = getOption(argc, argv, "-j");
   char *callsitesFile = getOption(argc, argv, "-c");
   char *allocTraceFile = getOption(argc, argv, "-a");
   char *mallocTraceFile = getOption(argc, argv, "-m");

   // Read the smaps file
   vector<MapEntry> sMaps;
#ifdef WINDOWS_FOOTPRINT
   readVmmapFile(argv[1], sMaps);
#else
   readSmapsFile(smapsFile, pageMapFile, sMaps);
#endif
   
   //===================== Javacore processing ============================
   vector<J9Segment> segments; // must not become out-of-scope until I am done with the maps
   if (javacoreFile)
      {
      // Read the javacore file
      readJavacore(javacoreFile, segments);
   
      // let's print all segments
      //cout << "Print segments:\n";
      //for (vector<J9Segment>::iterator it = segments.begin(); it != segments.end(); ++it)
      //   cout << *it << endl;
       
      
      // Annonate maps with j9segments
      annotateMapWithSegments(sMaps, segments);
   
      }
   
   //======================== Callsites processing =============================
   vector<CallSite> callSites; // must not become out-of-scope until I am done with the maps
   if (callsitesFile)
      {
      // Read the callsites file
      readCallSitesFile(callsitesFile, callSites);
      }

   // Annotate the smaps file with callsites
   if (!callSites.empty())
      {	   
      annotateMapWithSegments(sMaps, callSites);
      }

   vector<CallSite> allocSites;
   if (mallocTraceFile)
      {	      
      //readMallocTraceFile(mallocTraceFile, allocSites, segments, callSites);
      readMallocTraceFile(mallocTraceFile, allocSites);
      }

   if (allocTraceFile)
      {	      
      //readAllocTraceFile(allocTraceFile, allocSites, segments, callSites);
      readAllocTraceFile(allocTraceFile, allocSites);
      }

   // Annotate the smaps file with callsites
   if (!allocSites.empty())
      {	   
      annotateMapWithSegments(sMaps, allocSites);
      }

#if 0
   // Annotate the smaps file with allocation sites 
   if (!allocSites.empty())
      {	   
      cout << "Allocation sites are (" << allocSites.size() << "): \n";
      for (auto seg = allocSites.cbegin(); seg != allocSites.cend(); ++seg)
	 {
         cout << *seg << endl;
	 }	 
      annotateMapWithSegments(sMaps, allocSites);
      }
#endif
   // print results one by one
   for (vector<MapEntry>::const_iterator map = sMaps.begin(); map != sMaps.end(); ++map)
      {
      map->printEntryWithAnnotations();
      }

   printSpaceKBTakenByVmComponents(sMaps);
   return 0;
   }


