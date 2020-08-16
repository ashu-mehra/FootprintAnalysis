#include <vector>
#include <string>
#include <regex>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

//#include <ctype> // isdigit
#include "smap.hpp"
#include "Util.hpp"

using namespace std;

/* The entry in a maps file is short version of the smaps entry
   Example
   000c0000-000c1000 ---p 000c0000 00:00 0
   000c1000-00400000 rw-p 000c1000 00:00 0
   00400000-00401000 r-xp 00000000 00:17 61571540                           /jtctest/sdk_installs/ESPRESSO/PKG/pxz3170_27/pxz3170_27-20131030_03/ibm-java-s390-71/jre/bin/java
   00401000-00402000 rw-p 00000000 00:17 61571540                           /jtctest/sdk_installs/ESPRESSO/PKG/pxz3170_27/pxz3170_27-20131030_03/ibm-java-s390-71/jre/bin/java
*/
bool readMapsEntry(ifstream& smap, SmapEntry &entry)
   {
   string line;

   while (smap.good())
      {
      getline(smap, line);
      // skip empty lines
      if (line.find_first_not_of(" \t\n") == string::npos)
         continue;

      // Line starts with an address range and ends with MMUPageSize: heading
      std::cmatch result; //start    -  end        protection     offset      device major:minor   inode     file
      std::regex pattern("([0-9a-f]+)-([0-9a-f]+) (\\S\\S\\S\\S) ([0-9a-f]+) ([0-9a-f]+:[0-9a-f]+) (\\d+)\\s*(\\S*)");
      if (std::regex_search(line.c_str(), result, pattern))
         {
         //cerr << "Match found: start=" << result[1] << " end=" << result[2] << endl;
         entry.setStart(hex2ull(result[1]));
         entry.setEnd(hex2ull(result[2]));
         entry._details.assign(result[7]);
         return true;
         }
      else
         {
         cerr << "No match for:" << line << endl;
         return false;
         }
      }
   return false;
   }

void readMapsFile(const char *smapsFilename, std::vector<SmapEntry>& smaps)
   {
#ifdef DEBUG
   cout << "Reading file: " << string(smapsFilename) << endl;
#endif
   // Open the file
   ifstream myfile(smapsFilename);
   // check if successfull
   if (!myfile.is_open())
      {
      cerr << "Cannot open " << smapsFilename << endl;
      exit(-1);
      }

   SmapEntry entry;
   bool result;
   do {
      entry.clear();
      result = readMapsEntry(myfile, entry);
      if (result)
         smaps.push_back(entry);
      } while (result);
   }


/* example of an entry in the smaps file
00400000-00404000 r-xp 00000000 08:06 9487366                            /usr/bin/xconsole
Size:                 16 kB
Rss:                  12 kB
Pss:                  12 kB
Shared_Clean:          0 kB
Shared_Dirty:          0 kB
Private_Clean:        12 kB
Private_Dirty:         0 kB
Referenced:           12 kB
Swap:                  0 kB
KernelPageSize:        4 kB
MMUPageSize:           4 kB
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

In Other cases I have seen
00081000-000c0000 rw-p 00081000 00:00 0
Size:               252 kB
Rss:                 12 kB
Shared_Clean:         0 kB
Shared_Dirty:         0 kB
Private_Clean:       12 kB
Private_Dirty:        0 kB
Swap:                 0 kB
Pss:                 12 kB

In Other cases (LinuxPPC LE) I have seen 
10000000-10010000 r-xp 00000000 fc:03 2891694                            /home/dsouzai/sdks/pxl6470_27sr1-20140411_01/jre/bin/java
Size:                 64 kB
Rss:                  64 kB
Pss:                  64 kB
Shared_Clean:          0 kB
Shared_Dirty:          0 kB
Private_Clean:        64 kB
Private_Dirty:         0 kB
Referenced:           64 kB
Anonymous:             0 kB
AnonHugePages:         0 kB
Swap:                  0 kB
KernelPageSize:       64 kB
MMUPageSize:          64 kB
Locked:                0 kB
VmFlags: rd ex mr mw me dw    ==> This is a new field that doesn't have the format of the other fields
*/


//---------------------------------------------------------------
int parseSmapsMainLine(string line, SmapEntry &entry)
   {
   try 
      {   
      // Line starts with an address range
      std::cmatch result; //start    -  end        protection     offset      device major:minor   inode     file
      std::regex pattern("([0-9a-f]+)-([0-9a-f]+) (\\S\\S\\S\\S) ([0-9a-f]+) ([0-9a-f]+:[0-9a-f]+) (\\d+)\\s*(\\S*)");

      if (std::regex_search(line.c_str(), result, pattern))
         {
         //cerr << "Match found: start=" << result[1] << " end=" << result[2] << endl;
         entry.setStart(hex2ull(result[1]));
         entry.setEnd(hex2ull(result[2]));
         entry._protection.assign(result[3]);
         entry._details.assign(result[7]);
         return 0;
         }
      else
         {
         return -1; // no match
         }
      } catch (std::regex_error& e) {
         if (e.code() == std::regex_constants::error_badrepeat)
            std::cerr << "Repeat was not preceded by a valid regular expression.\n";
         else 
            std::cerr << "Error using regex\n";
         return -1;
      } // end catch
   }

//------------------------------- parseSmapsDetailedEntry --------------------
// 'entry' is already partially formed. We just fiil in the rest of its fields
//----------------------------------------------------------------------------
int parseSmapsDetailedEntry(string line, SmapEntry &entry)
   {
   vector<string> tokens;
   // Deal with exception seen on Power Linux LE. Excluse the lines that start with  "VmFlags:"
   if (line.compare(0, 8, "VmFlags:") == 0)
      return 0;

   // Split the line into tokens. We expect something like "Size:                  4 kB"
   tokenize(line, tokens, ": \t");
   if (tokens.size() != 3)
      {
      return -1;
      }
   if (tokens[2] != "kB")
      {
      cerr << "smap line must use kB as units. Line: " << line << endl; exit(-1);
      }
   // Decode the entry type
   if (tokens[0] == "Size")
      {
      //entry._size = atoi(tokens[1].c_str());
      // Verify that size is the same as the size given by the address range
      size_t sz = atoi(tokens[1].c_str());
      if (entry.sizeKB() != sz)
         {
         cerr << "Warning: smap entry with size that does match the address range" << endl;
         cerr << line << endl;
         cerr << "   Size from address range=" << entry.sizeKB() << "KB. Size field says " << sz << "KB" << endl;;
         }
      }
   else if (tokens[0] == "Rss")
      {
      entry._rss = atoi(tokens[1].c_str());
      }
   else if (tokens[0] == "Pss")
      {
      entry._pss = atoi(tokens[1].c_str());
      }
   else if (tokens[0] == "KernelPageSize")
      {
      entry._kernelPageSize = atoi(tokens[1].c_str());
      }
   else if (tokens[0] == "MMUPageSize")
      {
      entry._mmuPageSize = atoi(tokens[1].c_str());
      }
   return 0;
   }

//-----------------------------------------------------------------
void readSmapsFile(const char *smapsFilename, const char *pagemapFilename, std::vector<SmapEntry>& smaps)
   {
#ifdef DEBUG
   cout << "Reading file: " << string(smapsFilename) << endl;
#endif
   // Open the file
   ifstream myfile(smapsFilename);

   // check if successfull
   if (!myfile.is_open())
      {
      cerr << "Cannot open " << smapsFilename << endl;
      exit(-1);
      }

   ifstream pagemapFile;
   if (pagemapFilename)
      {	   
      pagemapFile.open(pagemapFilename, ios::in | ios::binary);
      if (!pagemapFile.is_open())
         {
         cerr << "Cannot open " << pagemapFilename << endl;
         exit(-1);	 
         }		 
      }

   SmapEntry entry, tmpEntry;
   string line;
   bool mainLineInEffect = false;
   int lineNo = 0;
   while (myfile.good())
      {
      getline(myfile, line);
      lineNo++;
      // skip empty lines
      if (line.find_first_not_of(" \t\n") == string::npos)
         continue;
#ifdef DEBUG
      cout << "Processing line: " << line << endl;
#endif
      // Which type of line I have ?
      // Check if this is a new main line
      tmpEntry.clear();
      if (parseSmapsMainLine(line, tmpEntry) >= 0) // if success
         {
         // Main line
         // Save the entry that we have assembled so far
         if (mainLineInEffect)
            {
            if (pagemapFilename)		    
               getPageMapForSmapEntry(pagemapFile, entry);
            smaps.push_back(entry);
            mainLineInEffect = false;
            }
         // Copy tmpEntry into entry
         entry = tmpEntry;
         mainLineInEffect = true;
         continue;
         }
      else // This could be a line with details
         {
         if (mainLineInEffect)
            {
            if (parseSmapsDetailedEntry(line, entry) < 0)
               {
               cerr << "Error parsing line " << lineNo << ": " << line << endl;
               cerr << "We expected a line of the form: String Number String. Example: Private_Dirty:        0 kB";
               exit(-1);
               }
            continue; // read next line
            }
         else // This must some error; we cannot start processing a detailed entry without having a main entry first
            {
            cerr << "Error with line " << lineNo << ": " << line << endl;
            cerr << "Details line without any main line (should start with a digit). Exiting\n";
            exit(-1);
            }
         }
      } // while (smap.good())
   // We may have a fully formed entry at this point
   if (mainLineInEffect)
      smaps.push_back(entry);

   }

//------------------------------------------------------------------
void createPageMapFile(const char *pagemapFilename, std::vector<SmapEntry>& smaps)
   {
   int fd = open(pagemapFilename, O_RDONLY);
   if (fd == -1)
      {
         cout<<"Eror in opening pagemap\n";	      
         return;
      }

   ofstream relevantPageMap("mypagemap", ios::out | ios::binary);
   int pagesize = getpagesize();
   for (SmapEntry &entry: smaps)
      {
      uint64_t start = entry.getAddrRange().getStart();
      uint64_t end = entry.getAddrRange().getEnd();
      uint64_t counter = start;
      vector<bool> pageVector;
      int count = 0;
      cout << hex << start << " - " << end << dec << "\n";
      for (; counter < end; counter += pagesize)
         {
         unsigned char singleByte;
         uint64_t offset = (counter / pagesize) * sizeof(uint64_t);
	 //cout << hex << counter << "(" << offset << ")" << dec << endl;
	 if (-1 != lseek(fd, offset, SEEK_SET))
            {
            uint64_t value = 0;		    
            read(fd, &value, sizeof(value));
	    cout << "value: " << hex << value << dec << "\n";
	    if (value & 0x8000000000000000)
               {
               count += 1;
	       pageVector.push_back(true);
               }
	    else
	       pageVector.push_back(false);	    
	    }
	 else
            {
            cout << "lseek failed\n";
	    return;
            }		    
         }
      relevantPageMap.write((char *)&start, sizeof(start));
      relevantPageMap.write((char *)&end, sizeof(end));
      unsigned char oneByte = 0;
      int numBytes = 0;
      int numBits = 0;
      int lastBitPosUsed = -1;
      for (const bool &status: pageVector)
	 {
	 lastBitPosUsed += 1;
	 numBits += 1;
	 if (status)
	    {
            oneByte |= (1 << lastBitPosUsed);
	    }
	 if (lastBitPosUsed == 7)
            {
            // write the byte to the file
	    //cout << "byte(" << numBytes << "): " << hex << (unsigned int)oneByte << dec << "\n";
   	    relevantPageMap.write((char *)&oneByte, sizeof(oneByte));
            oneByte = 0;
	    numBytes += 1;
	    lastBitPosUsed = -1;
            }		 
         }
      if (lastBitPosUsed != -1)
        {
        // write the last partially full byte
	//cout << "byte(" << numBytes << "): " << hex << (unsigned int)oneByte << dec << "\n";
	relevantPageMap.write((char *)&oneByte, sizeof(oneByte));
        numBytes += 1;	
	}
      cout << "Number of bytes written " << numBytes << " bytes (bits=" << numBits << ")\n";
      int expectedNumBytes = (((end - start) / pagesize) + 0x7) >> 0x3;
      int expectedNumBits = (end - start) / pagesize;
      if (numBytes != expectedNumBytes)
        {
        cout << "Expected number of bytes to be " << expectedNumBytes << " (bits=" << expectedNumBits << ")\n";
        }		
      cout << "Total pages in memory: " << count << ", RSS: " << ((count * pagesize) >> 10) << " KB\n";
      }
   relevantPageMap.flush();
   relevantPageMap.close();
   return;
   }

//------------------------------------------------------------------
void getPageMapForSmapEntry(ifstream &pagemapFile, SmapEntry &entry)
   {
   uint64_t start = 0;
   uint64_t end = 0;
   int pagesize = getpagesize();

   pagemapFile.read((char *)&start, sizeof(start));
   pagemapFile.read((char *)&end, sizeof(end));

   int numBits = (end - start) / pagesize;
   int numBytesToRead = (numBits + 0x7) >> 0x3; // round up numBits
   int bitsRead = 0;
   
   //cout << "Reading pagemap for smap entry " << entry << "\n";
   for (int i = 0; i < numBytesToRead; i++)
      {
      unsigned char oneByte = 0;
      pagemapFile.read((char *)&oneByte, sizeof(oneByte));
      for (int j = 0; (j <= 7) && (bitsRead < numBits); j++, bitsRead += 1)
         {
         unsigned char bit = ((1 << j) & oneByte);
         if (bit)
	    {
            //cout << "page " << (i*8)+j << ": " << "true\n";
   	    entry._inMemoryPages.push_back(true);			 
	    }
         else
            {
            //cout << "page " << (i*8)+j << ": " << "false\n";
	    entry._inMemoryPages.push_back(false);		      
	    }
	 }
      //cout << "\n";      
      }	      
   }

//------------------------------------------------------------------
void readPageMapFile()
   {
   cout << "------------------------------ readPageMapFile ------------------------------\n";
   ifstream infile ("mypagemap", ios::in | ios::binary);
   int pagesize = getpagesize();
   while (infile.good())
      {
         uint64_t start = 0;
         uint64_t end = 0;
	 int count = 0;
	 vector<bool> pageVector;

         infile.read((char *)&start, sizeof(start));
         infile.read((char *)&end, sizeof(end));

	 if (start >= end)
	    break;

         cout << hex << start << " - " << end << dec << "\n";

	 int numBytesToRead = (((end - start) / pagesize) + 0x7) >> 0x3;
	 int numBits = (end - start) / pagesize;
         cout << "Number of bytes to be read " << numBytesToRead << " (bits=" << numBits << ")\n";
	 int bitsRead = 0;

	 for (int i = 0; i < numBytesToRead; i++)
           {
           unsigned char oneByte = 0;		   
           infile.read((char *)&oneByte, sizeof(oneByte));
	   //cout << "byte(" << i << "): " << hex << (unsigned int)oneByte << dec << "\n";
           for (int j = 0; (j <= 7) && (bitsRead < numBits); j++, bitsRead += 1)
              {
              unsigned char bit = ((1 << j) & oneByte);
	      if (bit)
	         {
                 pageVector.push_back(true);			 
		 count += 1;
                 }
	      else
                 pageVector.push_back(false);		      
              }   		      
           }		   
         cout << "Total pages in memory: " << count << ", RSS: " << ((count * pagesize) >> 10) << " KB\n";       	 
      }   
      infile.close();   
   }	  
//------------------------------------------------------------------
void printLargestUnallocatedBlocks(const vector<SmapEntry> &smaps)
   {
   unsigned long long virtSize = 0;
   unsigned long long rssSize = 0;
   unsigned long long totalGapSize = 0;
   TopTen<AddrRange, AddrRangeSizeLessThan> topTen;

   vector<SmapEntry>::const_iterator crtMap = smaps.begin();
   if (crtMap != smaps.end())
      {
      virtSize += crtMap->sizeKB();
      rssSize += crtMap->_rss;
      vector<SmapEntry>::const_iterator prevMap = crtMap;
      for (++crtMap; crtMap != smaps.end(); prevMap = crtMap, ++crtMap)
         {
         virtSize += crtMap->sizeKB();
         rssSize += crtMap->_rss;
         //cout << *it << endl;

         // Are there any gaps?
         unsigned long long gapSize = prevMap->gapKB(*crtMap);
         if (gapSize != 0)
            {
            totalGapSize += gapSize;
            topTen.processElement(AddrRange(prevMap->getEnd(), crtMap->getStart()));
            }
         }
      }
   cout << "Total virtual size: " << dec << virtSize << " kB. Total rss:" << rssSize << " kB." << endl;
   cout << "Total gap size: " << totalGapSize << " KB" << endl;
   topTen.print();
   }

//----------------------------------------------------------------
unsigned long long computeReservedSpaceKB(const vector<SmapEntry> &smaps)
   {
   unsigned long long reservedSpaceKB = 0;

   // Reserved space does not have read, write or execute access
   for (vector<SmapEntry>::const_iterator crtMap = smaps.begin(); crtMap != smaps.end(); ++crtMap)
      {
      const string& protection = crtMap->getProtectionString();
      if (protection.size() != 4)
         {
         cerr << "Protection string '" << protection << "' should have exactly 4 characters\n";
         exit(1);
         }
      // Reserved lines start with ---
      if (protection[0] == '-' && protection[1] == '-' && protection[2] == '-')
         {
         reservedSpaceKB += crtMap->sizeKB();
         //cout << "Found reserved block: " << *crtMap << " Protection=" << protection << endl;
         }
      }
   return reservedSpaceKB;
   }

//-------------------------------------------------------------
void printTopTenReservedSpaceKB(const vector<SmapEntry> &smaps)
   {
   TopTen<SmapEntry, MemoryEntrySizeLessThan> topTen;
   // Reserved space does not have read, write or execute access
   for (vector<SmapEntry>::const_iterator crtMap = smaps.begin(); crtMap != smaps.end(); ++crtMap)
      {
      const string& protection = crtMap->getProtectionString();
      // Reserved lines start with ---
      if (protection[0] == '-' && protection[1] == '-' && protection[2] == '-')
         topTen.processElement(*crtMap);
      }
   topTen.print();
   }

//-----------------------------------------------------------
bool SmapEntry::isMapForSharedLibrary() const
   {
   // A shared library has .so in its name
   // Note that the name does not necessarily end with .so  Example: /usr/lib/libXext.so.6.4.0
   const std::string details = getDetailsString();
   size_t soPosition = details.find(".so");
   if (soPosition != string::npos) // found
      {
      // We should add more checks here: the ".so" fragment must be followed by a space, a dot, a newline or simply end the string
      soPosition += 3; // jump over the .so characters
      if (details.size() <= soPosition ||  // line ends with ".so" but no endl
         details.at(soPosition) == '.' ||  // e.g. /usr/lib/libXext.so.6.4.0
         details.at(soPosition) == ' ' ||
         details.at(soPosition) == '\t' ||
         details.at(soPosition) == '\n' ||
         details.at(soPosition) == '\r')
         return true;
      }
   return false;
   }

//---------------------------------------------------------------
// The details field may contain something like [stack:28189]
bool SmapEntry::isMapForThreadStack() const
   {
   return (getDetailsString().find("[stack") == 0); // check if it starts with [stack
   }

//--------------------------------------------------------
void SmapEntry::print(std::ostream& os) const
   {
   os << std::hex << "Start=" << setfill('0') << setw(16) << getStart() <<
      " End=" << setfill('0') << setw(16) << getEnd() << std::dec <<
      " Size=" << setfill(' ') << setw(6) << sizeKB() << " rss=" << setfill(' ') << setw(6) << _rss << 
      " pss=" << setfill(' ') << setw(6) << _pss <<
      " Prot=" << getProtectionString();
   if (_details.length() > 0)
      os << " " << _details;
   }

