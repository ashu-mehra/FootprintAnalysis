#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "Util.hpp"
#include "Javacore.hpp"

using namespace std;

void J9Segment::print(std::ostream& os) const
   {
   os << std::hex << getTypeName() << " ID=" << setfill('0') << setw(16) << _id <<
      " Start=" << setfill('0') << setw(16) << getStart() << " End=" << setfill('0') << setw(16) << getEnd() <<
      " size=" << std::dec << setw(5) << sizeKB() << " flags=" << std::hex << setfill('0') << setw(8) << getFlags();
   }

const char * J9Segment::_segmentTypes[] = { "UNKNOWN", "HEAP", "INTERNAL", "CLASS", "CODECACHE", "DATACACHE" };

J9Segment::SegmentType determineSegmentType(const string& line)
   {
   vector<string> tokens;
   tokenize(line, tokens);

   if (tokens.size() >= 3)
      {
      if (tokens[1] == "JIT")
         {
         if (tokens[2] == "Code")
            return J9Segment::CODECACHE;
         if (tokens[2] == "Data")
            return J9Segment::DATACACHE;
         }
      else if (tokens[1] == "Internal" && tokens[2] == "Memory")
         {
         return J9Segment::INTERNAL;
         }
      else if (tokens[1] == "Class" && tokens[2] == "Memory")
         {
         return J9Segment::CLASS;
         }
      }
   return J9Segment::UNKNOWN;
   }
   

void readJavacore(const char * javacoreFilename, vector<J9Segment>& segments)
   {
   cout << "Reading javacore file: " << string(javacoreFilename) << endl;
   // Open the file
   ifstream myfile(javacoreFilename);
   // check if successfull
   if (!myfile.is_open())
      {
      cerr << "Cannot open " << javacoreFilename << endl;
      exit(-1);
      }
   // read each line from file
   string line;
   int lineNo = 0;
   bool memInfoFound = false;
   J9Segment::SegmentType segmentType = J9Segment::UNKNOWN;
   while (myfile.good())
      {
      getline(myfile, line);
      lineNo++;

      if (!memInfoFound)
         {
         // search for:0SECTION       MEMINFO subcomponent dump routine
         if (line.find("0SECTION       MEMINFO subcomponent dump routine") != std::string::npos)
            {
            memInfoFound = true;
#ifdef DEBUG
            cerr << "Found: MEMINFO subcomponent dump routine\n";
#endif
            }
         }
      else // process segments
         {
         if (line.find("1STHEAPTYPE", 0) != std::string::npos)
            {
            segmentType = J9Segment::HEAP;
            }
         else if (line.find("1STHEAPREGION", 0) != std::string::npos)
            {
            // 1STHEAPREGION  0x0000000000561550 0x0000000000580000 0x0000000030570000 0x000000002FFF0000 Generational/Tenured Region
            vector<string> tokens;
            tokenize(line, tokens);
            if (tokens.size() < 6)
               {
               cerr << "Have found " << tokens.size() << " instead of 6-7 at line " << lineNo << endl; exit(-1);
               }
            unsigned long long id = hex2ull(tokens[1]);
            unsigned long long startAddr = hex2ull(tokens[2]);
            unsigned long long endAddr = hex2ull(tokens[3]);
            segments.push_back(J9Segment(id, startAddr, endAddr, segmentType, 0));
            }
         else if (line.find("1STSEGTYPE", 0) != std::string::npos)
            {
            segmentType = determineSegmentType(line);
            if (segmentType == J9Segment::UNKNOWN)
               {
               cerr << "Unknown segment type at line " << lineNo << endl;  exit(-1);
               }
            }
         else if (line.find("1STSEGMENT", 0) != std::string::npos)
            {
            // Process one segment
            //1STSEGMENT     0x00007FBE13B616C0 0x00007FBE07CFB030 0x00007FBE07EF7EA0 0x00007FBE07EFB030 0x00000048 0x0000000000200000
            vector<string> tokens;
            tokenize(line, tokens);
            if (tokens.size() != 7)
               {
               cerr << "Have found " << tokens.size() << " instead of 7 at line " << lineNo << endl; exit(-1);
               }
            unsigned long long id = hex2ull(tokens[1]);
            unsigned long long startAddr = hex2ull(tokens[2]);
            unsigned long long endAddr = hex2ull(tokens[4]);
            unsigned flags = strtoul(tokens[5].c_str(), NULL, 16);
            segments.push_back(J9Segment(id, startAddr, endAddr, segmentType, flags));
#ifdef DEBUG
            //cerr << "Created one segment\n";
#endif
            }
         else if (line.find("1STGCHTYPE", 0) != std::string::npos)
            {
            break;
            }
         }
      } // end while
   myfile.close();
   cout << "Reading of segments from javacore file finished\n";
   }
