#ifndef _CALLSITE_HPP__
#define _CALLSITE_HPP__
#include <string>
#include "AddrRange.hpp"
#include "Javacore.hpp"

using namespace std;

class CallSite : public AddrRange
   {
   public:
      enum AllocType
         {
         ALLOC_TYPE_UNKNOWN = 0,
         ALLOC_TYPE_MALLOC = 1,
         ALLOC_TYPE_MMAP,
	 };
      enum CallSiteSource
         {
         CALLSITE_OPENJ9 = 0,
	 CALLSITE_OTHERS,
         CALLSITE_SOURCE_END,
         };	   
      CallSite(const CallSite &other) : AddrRange(other.getStart(), other.getEnd())
         {
         _filename.assign(other._filename);
         _lineNo = other._lineNo;
         _source = other._source;	 
	 _allocType = other._allocType;
         }		 
      CallSite(unsigned long long startAddr, unsigned long long endAddr, const std::string& filename, int lineNo, CallSiteSource source = CALLSITE_OPENJ9, AllocType allocType = ALLOC_TYPE_UNKNOWN) :
         AddrRange(startAddr, endAddr), _filename(filename), _lineNo(lineNo), _source(source), _allocType(allocType)
         {}
      virtual void clear()
         {
         AddrRange::clear();
         _filename.clear();
         _lineNo = 0;
         }
      virtual int rangeType() const { return CALLSITE_RANGE; }
      CallSiteSource getSource() const { return _source; }
      std::string getFilename() const { return _filename; }
      AllocType getAllocType() const { return _allocType; }
      static CallSiteSource getSource(string filename)
         {
         size_t isOpenJ9Lib = filename.find("libj9");
         size_t isOMRLib = filename.find("libomr");
         size_t isJVMLib = filename.find("jvm");

         if ((isOpenJ9Lib != string::npos) || (isOMRLib != string::npos) || (isJVMLib != string::npos))
            return CALLSITE_OPENJ9;
         else
            return CALLSITE_OTHERS;
         }
      static AllocType getAllocType(string function)
         {
         if (function.find("mmap") != string::npos) return ALLOC_TYPE_MMAP;
         if (function.find("malloc") != string::npos) return ALLOC_TYPE_MALLOC;
         if (function.find("calloc") != string::npos) return ALLOC_TYPE_MALLOC;
         return ALLOC_TYPE_UNKNOWN;	 
         }		 

   protected:
      virtual void print(std::ostream& os) const;
   private:
      std::string        _filename;
      unsigned           _lineNo;
      CallSiteSource _source;
      AllocType _allocType;
   }; //  AddrRange

void readCallSitesFile(const char *filename, vector<CallSite>& callSites);
void readAllocTraceFile(const char *filename, vector<CallSite>& allocSites);
void readMallocTraceFile(const char *filename, vector<CallSite>& allocSites);
#endif // _CALLSITE_HPP__
