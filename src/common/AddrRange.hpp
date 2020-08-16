#ifndef _ADDRRANGE_HPP__
#define _ADDRRANGE_HPP__
#include <iostream>
#include <iomanip>
#include <functional> // for binary predicates. Want to sort entries by size
#include <algorithm>

class AddrRange
   {
   unsigned long long _startAddr;
   unsigned long long _endAddr;
   public:

   enum MergeResult
      {
      MATCH = 0,
      DISJOINT,
      SUBSET,
      SUPERSET,
      START_MATCH,
      END_MATCH,
      OVERLAP_LEFT,
      OVERLAP_RIGHT,
      };

      AddrRange() : _startAddr(0), _endAddr(0) {}
      AddrRange(unsigned long long start, unsigned long long end) : _startAddr(start), _endAddr(end)
         {
         if (end <= start && !(start == 0 && end == 0))
            {
            std::cerr << std::hex << "Range error: start=" << start << " end=" << end << std::endl;
            }
         }
      unsigned long long getStart() const { return _startAddr; }
      unsigned long long getEnd() const { return _endAddr; }
      void setStart(unsigned long long a){ _startAddr = a; }
      void setEnd(unsigned long long a) { _endAddr = a; }
      virtual void clear() { _startAddr = _endAddr = 0; }
      bool includes(const AddrRange& other) const { return other._startAddr >= _startAddr && other._startAddr < _endAddr && other._endAddr <= _endAddr; }
      bool superset(const AddrRange& other) const
         {
         if (*this == other) return false;
	 else
	    {
            return other._startAddr >= _startAddr && other._startAddr < _endAddr && other._endAddr <= _endAddr;
	    }
	 }
      bool disjoint(const AddrRange& other) const { return _endAddr <= other._startAddr || other._endAddr <= _startAddr; }
      bool overlapsLeft(const AddrRange& other) const { if ((other._startAddr >= _startAddr) && (other._startAddr < _endAddr) && (other._endAddr > _endAddr)) return true; else return false; }
      bool overlapsRight(const AddrRange& other) const { if ((other._startAddr < _startAddr) && (other._endAddr > _startAddr) && (other._endAddr <= _endAddr)) return true; else return false; }
      bool overlaps(const AddrRange& other) const { if (overlapsLeft(other) || overlapsRight(other)) return true; else return false; }
      unsigned long long size() const { return _endAddr - _startAddr; }
      // Measure the size (KB) between the end of this segment and the beginning of the next (toOther)
      // The two segments must be disjoint
      unsigned long long gapKB(const AddrRange& toOther) const { return (toOther._startAddr - _endAddr) >> 10; }
      unsigned long long sizeKB() const { return (_endAddr - _startAddr) >> 10; }
      bool operator <(const AddrRange& other) const { return this->getStart() < other.getStart(); }
      bool operator >(const AddrRange& other) const { return this->getStart() > other.getStart(); }
      bool in(const AddrRange& other) const { return (this->getStart() >= other.getStart()) && (this->getEnd() <= other.getEnd()); }
      virtual bool operator == (const AddrRange& other) const { return this->getStart() == other.getStart() && this->getEnd() == other.getEnd(); }
      friend std::ostream& operator<<(std::ostream& os, const AddrRange& ar);
      enum { SIMPLE_RANGE = 0, CALLSITE_RANGE, J9SEGMENT_RANGE };
      virtual int rangeType() const { return SIMPLE_RANGE; }
      MergeResult merge(const AddrRange& range)
         {
         MergeResult result;
         if (*this == range) { result = MATCH; }
         else if (this->disjoint(range)) { result = DISJOINT; }
         else if (range.superset(*this)) { result = SUBSET; }
         else if (this->superset(range))
            {
            if (this->getEnd() == range.getEnd()) { this->setEnd(range.getStart()); result = END_MATCH; }
            else if (this->getStart() == range.getStart()) { this->setStart(range.getEnd()); result = START_MATCH; }
            else
               {
               this->setEnd(range.getStart());
               result = SUPERSET;
               }
            }
         else
            {
            if (overlapsLeft(range))
               {		    
               this->setEnd(range.getStart());
	       result = OVERLAP_LEFT;
	       }
            else
	       {
               this->setStart(range.getEnd());
	       result = OVERLAP_RIGHT;
	       }
            }
         return result;
         }

   protected:
      virtual void print(std::ostream& os) const
         {
         os << std::hex << "Start=" << std::setfill('0') << std::setw(16) << getStart() <<
            " End=" << std::setfill('0') << std::setw(16) << getEnd() << std::dec <<
            " Size=" << std::setfill(' ') << std::setw(6) << sizeKB();
         }
   }; //  AddrRange

inline std::ostream& operator<< (std::ostream& os, const AddrRange& ar)
   {
   ar.print(os);
   return os;
   }

// Define our binary function object class that will be used to order AddrRange by size
struct AddrRangeSizeLessThan : public std::binary_function<AddrRange, AddrRange, bool>
   {
   bool operator() (const AddrRange& m1, const AddrRange& m2) const
      {
      return (m1.size() < m2.size());
      }
   };


#endif // _ADDRRANGE_HPP__
