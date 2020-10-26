// Copyright (c) 2019-2020 akshaynexus
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "blacklist.h"
#include "util.h"
std::vector<std::string> blacklistedAddrs {

    "SNa1DdaghTbYmMpbPEAxp8cLLFbJvDCyFw", //burn address
    "SPjzXGRkKKFRXCe3H4tgEUheBBxVZrAusd", //dev1 address (getdzypher)
    "SeErAzKoniHdntGt4sbXDYhc9Syf28Cr4p", //dev2 address (getdzypher)
    "SPFpQN7cuA3HZ3uGDR5z1ZtWxdeny1DnNt"  //blacklist test address
};
bool ContainsBlacklistedAddr(std::string addr){
    //Iterate through blacklisted addresses
    for (Iter it = blacklistedAddrs.begin(); it!=blacklistedAddrs.end(); ++it) {
        if(*it == addr) {
            LogPrintf("ContainsBlacklistedAddr() Found Blacklisted addr %s\n", addr);
            return true;
        }
    }
    return false;
}