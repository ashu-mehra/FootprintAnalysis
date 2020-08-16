#include <iostream>
#include <iomanip>
#include <string>
#include "MemoryEntry.hpp"
#include "Javacore.hpp"
#include "CallSites.hpp"

using namespace std;

void MemoryEntry::addCoveringRange(AddrRange& seg)
   {
   // insert based on start address
   std::list<const AddrRange*>::iterator s = _coveringRanges.begin();
   for (; s != _coveringRanges.end(); ++s)
      {
      uint64_t origEnd = seg.getEnd();
      AddrRange::MergeResult result = seg.merge(**s);

      if ((result == AddrRange::MATCH) || (result == AddrRange::SUBSET)) return;
      else if (result == AddrRange::DISJOINT)
         {
         if (seg < **s)
            break;		 
         }
      else if ((result == AddrRange::END_MATCH) || (result == AddrRange::OVERLAP_LEFT))
	 {
         break;
	 }
      else if ((result == AddrRange::START_MATCH) || (result == AddrRange::OVERLAP_RIGHT)) continue;
      else if (result == AddrRange::SUPERSET) 
         {
         AddrRange *newSeg = NULL;		 
         if (seg.rangeType() == AddrRange::J9SEGMENT_RANGE)
            {
            J9Segment *j9seg = static_cast<J9Segment *>(&seg);
            newSeg = new J9Segment(*j9seg);
            }
	 else
            {
            CallSite *cs = static_cast<CallSite *>(&seg);
            newSeg = new CallSite(*cs);
            }
	 newSeg->setStart((*s)->getEnd());
	 newSeg->setEnd(origEnd);
	 this->addCoveringRange(*newSeg);
	 break;
         }
#if 0
      if (seg == **s) // duplicate; segment and call site mapping to the same address
         return;
      if ((**s).includes(seg))
         return;
      if (seg < **s)
         break; // I found my insertion point
#endif
      }
   // We must insert before segment s
   _coveringRanges.insert(s, &seg);
   }

void MemoryEntry::createCoveringRange(AddrRange& seg)
   {
   AddrRange *newSeg = NULL;

   if (seg.rangeType() == AddrRange::J9SEGMENT_RANGE)
      {
      const J9Segment *j9seg = static_cast<const J9Segment *>(&seg);
      newSeg = new J9Segment(*j9seg);
      }
   if (seg.rangeType() == AddrRange::CALLSITE_RANGE)
      {
      const CallSite *callsite = static_cast<const CallSite *>(&seg);	      
      newSeg = new CallSite(*callsite);
      }	

   if (seg.includes(this->getAddrRange()))
      {		      
      newSeg->setStart(this->getStart());
      newSeg->setEnd(this->getEnd());
      }
   else if (seg.overlapsLeft(this->getAddrRange()))
      {
      newSeg->setStart(this->getStart());
      }
   else 
      {
      newSeg->setEnd(this->getEnd());
      }
   addCoveringRange(*newSeg);
   }

void MemoryEntry::print(std::ostream& os) const
   {
   os << std::hex << "Start=" << setfill('0') << setw(16) << getStart() <<
      " End=" << setfill('0') << setw(16) << getEnd() << std::dec <<
      " Size=" << setfill(' ') << setw(6) << sizeKB() << " rss=" << setfill(' ') << setw(6) << _rss << " Prot=" << getProtectionString();
   if (_details.length() > 0)
      os << " " << _details;
   }

// Print entry with annotations
void MemoryEntry::printEntryWithAnnotations() const
   {
   // Print the entry first
   cout << "MemEntry: " << *this << endl;
   // Check whether I need to print any covering segments/call-sites
   const list<const AddrRange*> coveringRanges = getCoveringRanges();
   if (coveringRanges.size() != 0)
      {
      cout << "\tCovering segments/call-sites:\n";
      // Go through the list of covering ranges
      for (list<const AddrRange*>::const_iterator range = coveringRanges.begin(); range != coveringRanges.end(); ++range)
         {
         cout << "\t\t" << **range << endl;
         }
      }
   const list<const AddrRange*> overlappingRanges = getOverlappingRanges();
   if (overlappingRanges.size() != 0)
      {
      cout << "\tOverlapping segments/call-sites:\n";
      for (list<const AddrRange*>::const_iterator range = overlappingRanges.begin(); range != overlappingRanges.end(); ++range)
         {
         cout << "\t\t" << **range << endl;
         }
      }
   }

