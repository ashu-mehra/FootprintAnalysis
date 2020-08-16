#ifndef _J9_SEGMENT_HPP__
#define _J9_SEGMENT_HPP__
#include <iostream>
#include <vector>
#include <string>
#include "AddrRange.hpp"



class J9Segment : public  AddrRange
   {
   public:
#define MEMORY_TYPE_JIT_SCRATCH_SPACE  0x1000000
#define MEMORY_TYPE_JIT_PERSISTENT      0x800000
#define MEMORY_TYPE_VIRTUAL  0x400

      enum SegmentType
         {
         UNKNOWN = 0,
         HEAP,
         INTERNAL,
         CLASS,
         CODECACHE,
         DATACACHE
         };
   private:
      unsigned long long _id;
      SegmentType        _type;
      unsigned           _flags;
      static const char *_segmentTypes[];
   public:
      J9Segment(const J9Segment &other) : AddrRange(other.getStart(), other.getEnd())
         {
       	    _id = other._id;
	    _type = other._type;
	    _flags = other._flags;
         }		 

      J9Segment(unsigned long long id, unsigned long long start, unsigned long long end, SegmentType segType, unsigned flags) :
         AddrRange(start, end),  _id(id), _type(segType), _flags(flags) {}
      const char *getTypeName() const { return _segmentTypes[_type]; }
      SegmentType getSegmentType() const { return _type; }
      unsigned getFlags() const { return _flags; }
      virtual void clear()
         {
         AddrRange::clear();
         _id = 0;
         _type = UNKNOWN;
         _flags = 0;
         }
      virtual int rangeType() const { return J9SEGMENT_RANGE; }
      bool isJITScratch() const { return _type == INTERNAL && (_flags & MEMORY_TYPE_JIT_SCRATCH_SPACE); }
      bool isJITPersistent() const { return _type == INTERNAL && (_flags & MEMORY_TYPE_JIT_PERSISTENT); }
   protected:
      virtual void print(std::ostream& os) const;
   }; // J9Segment


J9Segment::SegmentType determineSegmentType(const std::string& line);
void readJavacore(const char * javacoreFilename, std::vector<J9Segment>& segments);


#endif // _J9_SEGMENT_HPP__
