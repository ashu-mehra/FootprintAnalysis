#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <cstdlib> // for exit
#include "Util.hpp"

using namespace std;

void error(const char * msg)
   {
   std::cerr << msg << std::endl;
   exit(-1);
   }

// Splits a string into tokens and puts the tokens into a container
void tokenize(const std::string& str, std::vector<std::string>& tokens, const char* delim)
   {
   std::string::size_type pos = 0;
   while (true)
      {
      // Search for first non white character
      size_t tokenPos = str.find_first_not_of(delim, pos);
      if (tokenPos == std::string::npos) // no valid character found
         return;
      // Search for the first white characted (end of token)
      size_t whitePos = str.find_first_of(delim, tokenPos);
      if (whitePos == std::string::npos)
         {
         // remaining of the string is a token
         tokens.push_back(str.substr(tokenPos));
         return;
         }
      else
         {
         // Token starts at tokenPos and ends at whitePos
         tokens.push_back(str.substr(tokenPos, whitePos - tokenPos));
         pos = whitePos;
         }
      } // end while
   }


unsigned long long hex2ull(const std::string& hexNumber)
   {
   unsigned long long res = 0;
   unsigned int start = 0;
   if (hexNumber.size() > 2 && hexNumber.at(0) == '0' && hexNumber.at(1) == 'x')
      start += 2; // jump over 0x
   for (unsigned int i = start; i < hexNumber.size(); i++)
      {
      unsigned char digit = hexNumber.at(i);
      if (digit >= '0' && digit <= '9')
         res = (res << 4) + digit - '0';
      else if (digit >= 'A' && digit <= 'F')
         res = (res << 4) + digit - 'A' + 10;
      else if (digit >= 'a' && digit <= 'f')
         res = (res << 4) + digit - 'a' + 10;
      else
         {
         std::cerr << "Conversion error for " << hexNumber << std::endl;
         return HEX_CONVERT_ERROR; // error
         }
      }
   return res;
   }

unsigned long long a2ull(const std::string& decimalNumber)
   {
   unsigned long long val = 0;
   for (unsigned int i = 0; i < decimalNumber.size(); i++)
      {
      unsigned char digit = decimalNumber.at(i);
      if (digit == ',')
         continue;
      if (digit >= '0' && digit <= '9')
         {
         val = val * 10 + (digit - '0');
         }
      else
         {
         std::cerr << "Conversion error for " << decimalNumber << std::endl;
         return INT_CONVERT_ERROR;
         }
      }
   return val;
   }


