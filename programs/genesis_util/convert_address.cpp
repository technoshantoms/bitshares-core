/*
 * Acloudbank
 */

/**
 * Convert BTC / PTS addresses to a Graphene address.
 */

#include <graphene/protocol/pts_address.hpp>
#include <graphene/protocol/address.hpp>

#include <iostream>
#include <string>

using namespace graphene::protocol;

int main(int argc, char** argv)
{
   // grab 0 or more whitespace-delimited PTS addresses from stdin
   std::string s;
   while( std::cin >> s )
   {
      std::cout << std::string( address( pts_address( s ) ) ) << std::endl;
   }
   return 0;
}
