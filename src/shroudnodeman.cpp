// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The ShroudX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeshroudnode.h"
#include "addrman.h"
#include "darksend.h"
//#include "governance.h"
#include "shroudnode-payments.h"
#include "shroudnode-sync.h"
#include "shroudnode.h"
#include "shroudnodeconfig.h"
#include "shroudnodeman.h"
#include "netfulfilledman.h"
#include "util.h"
#include "validationinterface.h"

/** Shroudnode manager */
CShroudnodeMan mnodeman;

const std::string CShroudnodeMan::SERIALIZATION_VERSION_STRING = "CShroudnodeMan-Version-4";

struct CompareLastPaidBlock
{
    bool operator()(const std::pair<int, CShroudnode*>& t1,
                    const std::pair<int, CShroudnode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

struct CompareScoreMN
{
    bool operator()(const std::pair<int64_t, CShroudnode*>& t1,
                    const std::pair<int64_t, CShroudnode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

CShroudnodeIndex::CShroudnodeIndex()
    : nSize(0),
      mapIndex(),
      mapReverseIndex()
{}

bool CShroudnodeIndex::Get(int nIndex, CTxIn& vinShroudnode) const
{
    rindex_m_cit it = mapReverseIndex.find(nIndex);
    if(it == mapReverseIndex.end()) {
        return false;
    }
    vinShroudnode = it->second;
    return true;
}

int CShroudnodeIndex::GetShroudnodeIndex(const CTxIn& vinShroudnode) const
{
    index_m_cit it = mapIndex.find(vinShroudnode);
    if(it == mapIndex.end()) {
        return -1;
    }
    return it->second;
}

void CShroudnodeIndex::AddShroudnodeVIN(const CTxIn& vinShroudnode)
{
    index_m_it it = mapIndex.find(vinShroudnode);
    if(it != mapIndex.end()) {
        return;
    }
    int nNextIndex = nSize;
    mapIndex[vinShroudnode] = nNextIndex;
    mapReverseIndex[nNextIndex] = vinShroudnode;
    ++nSize;
}

void CShroudnodeIndex::Clear()
{
    mapIndex.clear();
    mapReverseIndex.clear();
    nSize = 0;
}
struct CompareByAddr

{
    bool operator()(const CShroudnode* t1,
                    const CShroudnode* t2) const
    {
        return t1->addr < t2->addr;
    }
};

void CShroudnodeIndex::RebuildIndex()
{
    nSize = mapIndex.size();
    for(index_m_it it = mapIndex.begin(); it != mapIndex.end(); ++it) {
        mapReverseIndex[it->second] = it->first;
    }
}

CShroudnodeMan::CShroudnodeMan() : cs(),
  vShroudnodes(),
  mAskedUsForShroudnodeList(),
  mWeAskedForShroudnodeList(),
  mWeAskedForShroudnodeListEntry(),
  mWeAskedForVerification(),
  mMnbRecoveryRequests(),
  mMnbRecoveryGoodReplies(),
  listScheduledMnbRequestConnections(),
  nLastIndexRebuildTime(0),
  indexShroudnodes(),
  indexShroudnodesOld(),
  fIndexRebuilt(false),
  fShroudnodesAdded(false),
  fShroudnodesRemoved(false),
//  vecDirtyGovernanceObjectHashes(),
  nLastWatchdogVoteTime(0),
  mapSeenShroudnodeBroadcast(),
  mapSeenShroudnodePing(),
  nDsqCount(0)
{}

bool CShroudnodeMan::Add(CShroudnode &mn)
{
    LOCK(cs);

    CShroudnode *pmn = Find(mn.vin);
    if (pmn == NULL) {
        LogPrint("shroudnode", "CShroudnodeMan::Add -- Adding new Shroudnode: addr=%s, %i now\n", mn.addr.ToString(), size() + 1);
        vShroudnodes.push_back(mn);
        indexShroudnodes.AddShroudnodeVIN(mn.vin);
        fShroudnodesAdded = true;
        return true;
    }

    return false;
}

void CShroudnodeMan::AskForMN(CNode* pnode, const CTxIn &vin)
{
    if(!pnode) return;

    LOCK(cs);

    std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it1 = mWeAskedForShroudnodeListEntry.find(vin.prevout);
    if (it1 != mWeAskedForShroudnodeListEntry.end()) {
        std::map<CNetAddr, int64_t>::iterator it2 = it1->second.find(pnode->addr);
        if (it2 != it1->second.end()) {
            if (GetTime() < it2->second) {
                // we've asked recently, should not repeat too often or we could get banned
                return;
            }
            // we asked this node for this outpoint but it's ok to ask again already
            LogPrintf("CShroudnodeMan::AskForMN -- Asking same peer %s for missing shroudnode entry again: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        } else {
            // we already asked for this outpoint but not this node
            LogPrintf("CShroudnodeMan::AskForMN -- Asking new peer %s for missing shroudnode entry: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        }
    } else {
        // we never asked any node for this outpoint
        LogPrintf("CShroudnodeMan::AskForMN -- Asking peer %s for missing shroudnode entry for the first time: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
    }
    mWeAskedForShroudnodeListEntry[vin.prevout][pnode->addr] = GetTime() + DSEG_UPDATE_SECONDS;

    pnode->PushMessage(NetMsgType::DSEG, vin);
}

void CShroudnodeMan::Check()
{
    LOCK(cs);

//    LogPrint("shroudnode", "CShroudnodeMan::Check -- nLastWatchdogVoteTime=%d, IsWatchdogActive()=%d\n", nLastWatchdogVoteTime, IsWatchdogActive());

    BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {
        mn.Check();
    }
}

void CShroudnodeMan::CheckAndRemove()
{
    if(!shroudnodeSync.IsShroudnodeListSynced()) return;

    LogPrintf("CShroudnodeMan::CheckAndRemove\n");

    {
        // Need LOCK2 here to ensure consistent locking order because code below locks cs_main
        // in CheckMnbAndUpdateShroudnodeList()
        LOCK2(cs_main, cs);

        Check();

        // Remove spent shroudnodes, prepare structures and make requests to reasure the state of inactive ones
        std::vector<CShroudnode>::iterator it = vShroudnodes.begin();
        std::vector<std::pair<int, CShroudnode> > vecShroudnodeRanks;
        // ask for up to MNB_RECOVERY_MAX_ASK_ENTRIES shroudnode entries at a time
        int nAskForMnbRecovery = MNB_RECOVERY_MAX_ASK_ENTRIES;
        while(it != vShroudnodes.end()) {
            CShroudnodeBroadcast mnb = CShroudnodeBroadcast(*it);
            uint256 hash = mnb.GetHash();
            // If collateral was spent ...
            if ((*it).IsOutpointSpent()) {
                LogPrint("shroudnode", "CShroudnodeMan::CheckAndRemove -- Removing Shroudnode: %s  addr=%s  %i now\n", (*it).GetStateString(), (*it).addr.ToString(), size() - 1);

                // erase all of the broadcasts we've seen from this txin, ...
                mapSeenShroudnodeBroadcast.erase(hash);
                mWeAskedForShroudnodeListEntry.erase((*it).vin.prevout);

                // and finally remove it from the list
//                it->FlagGovernanceItemsAsDirty();
                it = vShroudnodes.erase(it);
                fShroudnodesRemoved = true;
            } else {
                bool fAsk = pCurrentBlockIndex &&
                            (nAskForMnbRecovery > 0) &&
                            shroudnodeSync.IsSynced() &&
                            it->IsNewStartRequired() &&
                            !IsMnbRecoveryRequested(hash);
                if(fAsk) {
                    // this mn is in a non-recoverable state and we haven't asked other nodes yet
                    std::set<CNetAddr> setRequested;
                    // calulate only once and only when it's needed
                    if(vecShroudnodeRanks.empty()) {
                        int nRandomBlockHeight = GetRandInt(pCurrentBlockIndex->nHeight);
                        vecShroudnodeRanks = GetShroudnodeRanks(nRandomBlockHeight);
                    }
                    bool fAskedForMnbRecovery = false;
                    // ask first MNB_RECOVERY_QUORUM_TOTAL shroudnodes we can connect to and we haven't asked recently
                    for(int i = 0; setRequested.size() < MNB_RECOVERY_QUORUM_TOTAL && i < (int)vecShroudnodeRanks.size(); i++) {
                        // avoid banning
                        if(mWeAskedForShroudnodeListEntry.count(it->vin.prevout) && mWeAskedForShroudnodeListEntry[it->vin.prevout].count(vecShroudnodeRanks[i].second.addr)) continue;
                        // didn't ask recently, ok to ask now
                        CService addr = vecShroudnodeRanks[i].second.addr;
                        setRequested.insert(addr);
                        listScheduledMnbRequestConnections.push_back(std::make_pair(addr, hash));
                        fAskedForMnbRecovery = true;
                    }
                    if(fAskedForMnbRecovery) {
                        LogPrint("shroudnode", "CShroudnodeMan::CheckAndRemove -- Recovery initiated, shroudnode=%s\n", it->vin.prevout.ToStringShort());
                        nAskForMnbRecovery--;
                    }
                    // wait for mnb recovery replies for MNB_RECOVERY_WAIT_SECONDS seconds
                    mMnbRecoveryRequests[hash] = std::make_pair(GetTime() + MNB_RECOVERY_WAIT_SECONDS, setRequested);
                }
                ++it;
            }
        }

        // proces replies for SHROUDNODE_NEW_START_REQUIRED shroudnodes
        LogPrint("shroudnode", "CShroudnodeMan::CheckAndRemove -- mMnbRecoveryGoodReplies size=%d\n", (int)mMnbRecoveryGoodReplies.size());
        std::map<uint256, std::vector<CShroudnodeBroadcast> >::iterator itMnbReplies = mMnbRecoveryGoodReplies.begin();
        while(itMnbReplies != mMnbRecoveryGoodReplies.end()){
            if(mMnbRecoveryRequests[itMnbReplies->first].first < GetTime()) {
                // all nodes we asked should have replied now
                if(itMnbReplies->second.size() >= MNB_RECOVERY_QUORUM_REQUIRED) {
                    // majority of nodes we asked agrees that this mn doesn't require new mnb, reprocess one of new mnbs
                    LogPrint("shroudnode", "CShroudnodeMan::CheckAndRemove -- reprocessing mnb, shroudnode=%s\n", itMnbReplies->second[0].vin.prevout.ToStringShort());
                    // mapSeenShroudnodeBroadcast.erase(itMnbReplies->first);
                    int nDos;
                    itMnbReplies->second[0].fRecovery = true;
                    CheckMnbAndUpdateShroudnodeList(NULL, itMnbReplies->second[0], nDos);
                }
                LogPrint("shroudnode", "CShroudnodeMan::CheckAndRemove -- removing mnb recovery reply, shroudnode=%s, size=%d\n", itMnbReplies->second[0].vin.prevout.ToStringShort(), (int)itMnbReplies->second.size());
                mMnbRecoveryGoodReplies.erase(itMnbReplies++);
            } else {
                ++itMnbReplies;
            }
        }
    }
    {
        // no need for cm_main below
        LOCK(cs);

        std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > >::iterator itMnbRequest = mMnbRecoveryRequests.begin();
        while(itMnbRequest != mMnbRecoveryRequests.end()){
            // Allow this mnb to be re-verified again after MNB_RECOVERY_RETRY_SECONDS seconds
            // if mn is still in SHROUDNODE_NEW_START_REQUIRED state.
            if(GetTime() - itMnbRequest->second.first > MNB_RECOVERY_RETRY_SECONDS) {
                mMnbRecoveryRequests.erase(itMnbRequest++);
            } else {
                ++itMnbRequest;
            }
        }

        // check who's asked for the Shroudnode list
        std::map<CNetAddr, int64_t>::iterator it1 = mAskedUsForShroudnodeList.begin();
        while(it1 != mAskedUsForShroudnodeList.end()){
            if((*it1).second < GetTime()) {
                mAskedUsForShroudnodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check who we asked for the Shroudnode list
        it1 = mWeAskedForShroudnodeList.begin();
        while(it1 != mWeAskedForShroudnodeList.end()){
            if((*it1).second < GetTime()){
                mWeAskedForShroudnodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check which Shroudnodes we've asked for
        std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it2 = mWeAskedForShroudnodeListEntry.begin();
        while(it2 != mWeAskedForShroudnodeListEntry.end()){
            std::map<CNetAddr, int64_t>::iterator it3 = it2->second.begin();
            while(it3 != it2->second.end()){
                if(it3->second < GetTime()){
                    it2->second.erase(it3++);
                } else {
                    ++it3;
                }
            }
            if(it2->second.empty()) {
                mWeAskedForShroudnodeListEntry.erase(it2++);
            } else {
                ++it2;
            }
        }

        std::map<CNetAddr, CShroudnodeVerification>::iterator it3 = mWeAskedForVerification.begin();
        while(it3 != mWeAskedForVerification.end()){
            if(it3->second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS) {
                mWeAskedForVerification.erase(it3++);
            } else {
                ++it3;
            }
        }

        // NOTE: do not expire mapSeenShroudnodeBroadcast entries here, clean them on mnb updates!

        // remove expired mapSeenShroudnodePing
        std::map<uint256, CShroudnodePing>::iterator it4 = mapSeenShroudnodePing.begin();
        while(it4 != mapSeenShroudnodePing.end()){
            if((*it4).second.IsExpired()) {
                LogPrint("shroudnode", "CShroudnodeMan::CheckAndRemove -- Removing expired Shroudnode ping: hash=%s\n", (*it4).second.GetHash().ToString());
                mapSeenShroudnodePing.erase(it4++);
            } else {
                ++it4;
            }
        }

        // remove expired mapSeenShroudnodeVerification
        std::map<uint256, CShroudnodeVerification>::iterator itv2 = mapSeenShroudnodeVerification.begin();
        while(itv2 != mapSeenShroudnodeVerification.end()){
            if((*itv2).second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS){
                LogPrint("shroudnode", "CShroudnodeMan::CheckAndRemove -- Removing expired Shroudnode verification: hash=%s\n", (*itv2).first.ToString());
                mapSeenShroudnodeVerification.erase(itv2++);
            } else {
                ++itv2;
            }
        }

        LogPrintf("CShroudnodeMan::CheckAndRemove -- %s\n", ToString());

        if(fShroudnodesRemoved) {
            CheckAndRebuildShroudnodeIndex();
        }
    }

    if(fShroudnodesRemoved) {
        NotifyShroudnodeUpdates();
    }
}

void CShroudnodeMan::Clear()
{
    LOCK(cs);
    vShroudnodes.clear();
    mAskedUsForShroudnodeList.clear();
    mWeAskedForShroudnodeList.clear();
    mWeAskedForShroudnodeListEntry.clear();
    mapSeenShroudnodeBroadcast.clear();
    mapSeenShroudnodePing.clear();
    nDsqCount = 0;
    nLastWatchdogVoteTime = 0;
    indexShroudnodes.Clear();
    indexShroudnodesOld.Clear();
}

int CShroudnodeMan::CountShroudnodes(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinShroudnodePaymentsProto() : nProtocolVersion;

    BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {
        if(mn.nProtocolVersion < nProtocolVersion) continue;
        nCount++;
    }

    return nCount;
}

int CShroudnodeMan::CountEnabled(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinShroudnodePaymentsProto() : nProtocolVersion;

    BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {
        if(mn.nProtocolVersion < nProtocolVersion || !mn.IsEnabled()) continue;
        nCount++;
    }

    return nCount;
}

/* Only IPv4 shroudnodes are allowed in 12.1, saving this for later
int CShroudnodeMan::CountByIP(int nNetworkType)
{
    LOCK(cs);
    int nNodeCount = 0;

    BOOST_FOREACH(CShroudnode& mn, vShroudnodes)
        if ((nNetworkType == NET_IPV4 && mn.addr.IsIPv4()) ||
            (nNetworkType == NET_TOR  && mn.addr.IsTor())  ||
            (nNetworkType == NET_IPV6 && mn.addr.IsIPv6())) {
                nNodeCount++;
        }

    return nNodeCount;
}
*/

void CShroudnodeMan::DsegUpdate(CNode* pnode)
{
    LOCK(cs);

    if(Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if(!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal())) {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForShroudnodeList.find(pnode->addr);
            if(it != mWeAskedForShroudnodeList.end() && GetTime() < (*it).second) {
                LogPrintf("CShroudnodeMan::DsegUpdate -- we already asked %s for the list; skipping...\n", pnode->addr.ToString());
                return;
            }
        }
    }
    
    pnode->PushMessage(NetMsgType::DSEG, CTxIn());
    int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
    mWeAskedForShroudnodeList[pnode->addr] = askAgain;

    LogPrint("shroudnode", "CShroudnodeMan::DsegUpdate -- asked %s for the list\n", pnode->addr.ToString());
}

CShroudnode* CShroudnodeMan::Find(const std::string &txHash, const std::string outputIndex)
{
    LOCK(cs);

    BOOST_FOREACH(CShroudnode& mn, vShroudnodes)
    {
        COutPoint outpoint = mn.vin.prevout;

        if(txHash==outpoint.hash.ToString().substr(0,64) &&
           outputIndex==to_string(outpoint.n))
            return &mn;
    }
    return NULL;
}

CShroudnode* CShroudnodeMan::Find(const CScript &payee)
{
    LOCK(cs);

    BOOST_FOREACH(CShroudnode& mn, vShroudnodes)
    {
        if(GetScriptForDestination(mn.pubKeyCollateralAddress.GetID()) == payee)
            return &mn;
    }
    return NULL;
}

CShroudnode* CShroudnodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    BOOST_FOREACH(CShroudnode& mn, vShroudnodes)
    {
        if(mn.vin.prevout == vin.prevout)
            return &mn;
    }
    return NULL;
}

CShroudnode* CShroudnodeMan::Find(const CPubKey &pubKeyShroudnode)
{
    LOCK(cs);

    BOOST_FOREACH(CShroudnode& mn, vShroudnodes)
    {
        if(mn.pubKeyShroudnode == pubKeyShroudnode)
            return &mn;
    }
    return NULL;
}

bool CShroudnodeMan::Get(const CPubKey& pubKeyShroudnode, CShroudnode& shroudnode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CShroudnode* pMN = Find(pubKeyShroudnode);
    if(!pMN)  {
        return false;
    }
    shroudnode = *pMN;
    return true;
}

bool CShroudnodeMan::Get(const CTxIn& vin, CShroudnode& shroudnode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CShroudnode* pMN = Find(vin);
    if(!pMN)  {
        return false;
    }
    shroudnode = *pMN;
    return true;
}

shroudnode_info_t CShroudnodeMan::GetShroudnodeInfo(const CTxIn& vin)
{
    shroudnode_info_t info;
    LOCK(cs);
    CShroudnode* pMN = Find(vin);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

shroudnode_info_t CShroudnodeMan::GetShroudnodeInfo(const CPubKey& pubKeyShroudnode)
{
    shroudnode_info_t info;
    LOCK(cs);
    CShroudnode* pMN = Find(pubKeyShroudnode);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

bool CShroudnodeMan::Has(const CTxIn& vin)
{
    LOCK(cs);
    CShroudnode* pMN = Find(vin);
    return (pMN != NULL);
}

char* CShroudnodeMan::GetNotQualifyReason(CShroudnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount)
{
    if (!mn.IsValidForPayment()) {
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'not valid for payment'");
        return reasonStr;
    }
    // //check protocol version
    if (mn.nProtocolVersion < mnpayments.GetMinShroudnodePaymentsProto()) {
        // LogPrintf("Invalid nProtocolVersion!\n");
        // LogPrintf("mn.nProtocolVersion=%s!\n", mn.nProtocolVersion);
        // LogPrintf("mnpayments.GetMinShroudnodePaymentsProto=%s!\n", mnpayments.GetMinShroudnodePaymentsProto());
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'Invalid nProtocolVersion', nProtocolVersion=%d", mn.nProtocolVersion);
        return reasonStr;
    }
    //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
    if (mnpayments.IsScheduled(mn, nBlockHeight)) {
        // LogPrintf("mnpayments.IsScheduled!\n");
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'is scheduled'");
        return reasonStr;
    }
    //it's too new, wait for a cycle
    if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
        // LogPrintf("it's too new, wait for a cycle!\n");
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'too new', sigTime=%s, will be qualifed after=%s",
                DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime + (nMnCount * 2.6 * 60)).c_str());
        return reasonStr;
    }
    //make sure it has at least as many confirmations as there are shroudnodes
    if (mn.GetCollateralAge() < nMnCount) {
        // LogPrintf("mn.GetCollateralAge()=%s!\n", mn.GetCollateralAge());
        // LogPrintf("nMnCount=%s!\n", nMnCount);
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'collateralAge < znCount', collateralAge=%d, znCount=%d", mn.GetCollateralAge(), nMnCount);
        return reasonStr;
    }
    return NULL;
}

// Same method, different return type, to avoid Shroudnode operator issues.
// TODO: discuss standardizing the JSON type here, as it's done everywhere else in the code.
UniValue CShroudnodeMan::GetNotQualifyReasonToUniValue(CShroudnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount)
{
    UniValue ret(UniValue::VOBJ);
    UniValue data(UniValue::VOBJ);
    string description;

    if (!mn.IsValidForPayment()) {
        description = "not valid for payment";
    }

    //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
    else if (mnpayments.IsScheduled(mn, nBlockHeight)) {
        description = "Is scheduled";
    }

    // //check protocol version
    else if (mn.nProtocolVersion < mnpayments.GetMinShroudnodePaymentsProto()) {
        description = "Invalid nProtocolVersion";

        data.push_back(Pair("nProtocolVersion", mn.nProtocolVersion));
    }

    //it's too new, wait for a cycle
    else if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
        // LogPrintf("it's too new, wait for a cycle!\n");
        description = "Too new";

        //TODO unix timestamp
        data.push_back(Pair("sigTime", mn.sigTime));
        data.push_back(Pair("qualifiedAfter", mn.sigTime + (nMnCount * 2.6 * 60)));
    }
    //make sure it has at least as many confirmations as there are shroudnodes
    else if (mn.GetCollateralAge() < nMnCount) {
        description = "collateralAge < znCount";

        data.push_back(Pair("collateralAge", mn.GetCollateralAge()));
        data.push_back(Pair("znCount", nMnCount));
    }

    ret.push_back(Pair("result", description.empty()));
    if(!description.empty()){
        ret.push_back(Pair("description", description));
    }
    if(!data.empty()){
        ret.push_back(Pair("data", data));
    }

    return ret;
}

//
// Deterministically select the oldest/best shroudnode to pay on the network
//
CShroudnode* CShroudnodeMan::GetNextShroudnodeInQueueForPayment(bool fFilterSigTime, int& nCount)
{
    if(!pCurrentBlockIndex) {
        nCount = 0;
        return NULL;
    }
    return GetNextShroudnodeInQueueForPayment(pCurrentBlockIndex->nHeight, fFilterSigTime, nCount);
}

CShroudnode* CShroudnodeMan::GetNextShroudnodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount)
{
    // Need LOCK2 here to ensure consistent locking order because the GetBlockHash call below locks cs_main
    LOCK2(cs_main,cs);

    CShroudnode *pBestShroudnode = NULL;
    std::vector<std::pair<int, CShroudnode*> > vecShroudnodeLastPaid;

    /*
        Make a vector with all of the last paid times
    */
    int nMnCount = CountEnabled();
    int index = 0;
    BOOST_FOREACH(CShroudnode &mn, vShroudnodes)
    {
        index += 1;
        // LogPrintf("shroud=%s, mn=%s\n", index, mn.ToString());
        /*if (!mn.IsValidForPayment()) {
            LogPrint("shroudnodeman", "Shroudnode, %s, addr(%s), not-qualified: 'not valid for payment'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        // //check protocol version
        if (mn.nProtocolVersion < mnpayments.GetMinShroudnodePaymentsProto()) {
            // LogPrintf("Invalid nProtocolVersion!\n");
            // LogPrintf("mn.nProtocolVersion=%s!\n", mn.nProtocolVersion);
            // LogPrintf("mnpayments.GetMinShroudnodePaymentsProto=%s!\n", mnpayments.GetMinShroudnodePaymentsProto());
            LogPrint("shroudnodeman", "Shroudnode, %s, addr(%s), not-qualified: 'invalid nProtocolVersion'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
        if (mnpayments.IsScheduled(mn, nBlockHeight)) {
            // LogPrintf("mnpayments.IsScheduled!\n");
            LogPrint("shroudnodeman", "Shroudnode, %s, addr(%s), not-qualified: 'IsScheduled'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        //it's too new, wait for a cycle
        if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
            // LogPrintf("it's too new, wait for a cycle!\n");
            LogPrint("shroudnodeman", "Shroudnode, %s, addr(%s), not-qualified: 'it's too new, wait for a cycle!', sigTime=%s, will be qualifed after=%s\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime + (nMnCount * 2.6 * 60)).c_str());
            continue;
        }
        //make sure it has at least as many confirmations as there are shroudnodes
        if (mn.GetCollateralAge() < nMnCount) {
            // LogPrintf("mn.GetCollateralAge()=%s!\n", mn.GetCollateralAge());
            // LogPrintf("nMnCount=%s!\n", nMnCount);
            LogPrint("shroudnodeman", "Shroudnode, %s, addr(%s), not-qualified: 'mn.GetCollateralAge() < nMnCount', CollateralAge=%d, nMnCount=%d\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), mn.GetCollateralAge(), nMnCount);
            continue;
        }*/
        char* reasonStr = GetNotQualifyReason(mn, nBlockHeight, fFilterSigTime, nMnCount);
        if (reasonStr != NULL) {
            LogPrint("shroudnodeman", "Shroudnode, %s, addr(%s), qualify %s\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), reasonStr);
            delete [] reasonStr;
            continue;
        }
        vecShroudnodeLastPaid.push_back(std::make_pair(mn.GetLastPaidBlock(), &mn));
    }
    nCount = (int)vecShroudnodeLastPaid.size();

    //when the network is in the process of upgrading, don't penalize nodes that recently restarted
    if(fFilterSigTime && nCount < nMnCount / 3) {
        // LogPrintf("Need Return, nCount=%s, nMnCount/3=%s\n", nCount, nMnCount/3);
        return GetNextShroudnodeInQueueForPayment(nBlockHeight, false, nCount);
    }

    // Sort them low to high
    sort(vecShroudnodeLastPaid.begin(), vecShroudnodeLastPaid.end(), CompareLastPaidBlock());

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight - 101)) {
        LogPrintf("CShroudnode::GetNextShroudnodeInQueueForPayment -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight - 101);
        return NULL;
    }
    // Look at 1/10 of the oldest nodes (by last payment), calculate their scores and pay the best one
    //  -- This doesn't look at who is being paid in the +8-10 blocks, allowing for double payments very rarely
    //  -- 1/100 payments should be a double payment on mainnet - (1/(3000/10))*2
    //  -- (chance per block * chances before IsScheduled will fire)
    int nTenthNetwork = nMnCount/10;
    int nCountTenth = 0;
    arith_uint256 nHighest = 0;
    BOOST_FOREACH (PAIRTYPE(int, CShroudnode*)& s, vecShroudnodeLastPaid){
        arith_uint256 nScore = s.second->CalculateScore(blockHash);
        if(nScore > nHighest){
            nHighest = nScore;
            pBestShroudnode = s.second;
        }
        nCountTenth++;
        if(nCountTenth >= nTenthNetwork) break;
    }
    return pBestShroudnode;
}

CShroudnode* CShroudnodeMan::FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion)
{
    LOCK(cs);

    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinShroudnodePaymentsProto() : nProtocolVersion;

    int nCountEnabled = CountEnabled(nProtocolVersion);
    int nCountNotExcluded = nCountEnabled - vecToExclude.size();

    LogPrintf("CShroudnodeMan::FindRandomNotInVec -- %d enabled shroudnodes, %d shroudnodes to choose from\n", nCountEnabled, nCountNotExcluded);
    if(nCountNotExcluded < 1) return NULL;

    // fill a vector of pointers
    std::vector<CShroudnode*> vpShroudnodesShuffled;
    BOOST_FOREACH(CShroudnode &mn, vShroudnodes) {
        vpShroudnodesShuffled.push_back(&mn);
    }

    InsecureRand insecureRand;
    // shuffle pointers
    std::random_shuffle(vpShroudnodesShuffled.begin(), vpShroudnodesShuffled.end(), insecureRand);
    bool fExclude;

    // loop through
    BOOST_FOREACH(CShroudnode* pmn, vpShroudnodesShuffled) {
        if(pmn->nProtocolVersion < nProtocolVersion || !pmn->IsEnabled()) continue;
        fExclude = false;
        BOOST_FOREACH(const CTxIn &txinToExclude, vecToExclude) {
            if(pmn->vin.prevout == txinToExclude.prevout) {
                fExclude = true;
                break;
            }
        }
        if(fExclude) continue;
        // found the one not in vecToExclude
        LogPrint("shroudnode", "CShroudnodeMan::FindRandomNotInVec -- found, shroudnode=%s\n", pmn->vin.prevout.ToStringShort());
        return pmn;
    }

    LogPrint("shroudnode", "CShroudnodeMan::FindRandomNotInVec -- failed\n");
    return NULL;
}

int CShroudnodeMan::GetShroudnodeRank(const CTxIn& vin, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CShroudnode*> > vecShroudnodeScores;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return -1;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {
        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive) {
            if(!mn.IsEnabled()) continue;
        }
        else {
            if(!mn.IsValidForPayment()) continue;
        }
        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecShroudnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecShroudnodeScores.rbegin(), vecShroudnodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CShroudnode*)& scorePair, vecShroudnodeScores) {
        nRank++;
        if(scorePair.second->vin.prevout == vin.prevout) return nRank;
    }

    return -1;
}

std::vector<std::pair<int, CShroudnode> > CShroudnodeMan::GetShroudnodeRanks(int nBlockHeight, int nMinProtocol)
{
    std::vector<std::pair<int64_t, CShroudnode*> > vecShroudnodeScores;
    std::vector<std::pair<int, CShroudnode> > vecShroudnodeRanks;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return vecShroudnodeRanks;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {

        if(mn.nProtocolVersion < nMinProtocol || !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecShroudnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecShroudnodeScores.rbegin(), vecShroudnodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CShroudnode*)& s, vecShroudnodeScores) {
        nRank++;
        s.second->SetRank(nRank);
        vecShroudnodeRanks.push_back(std::make_pair(nRank, *s.second));
    }

    return vecShroudnodeRanks;
}

CShroudnode* CShroudnodeMan::GetShroudnodeByRank(int nRank, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CShroudnode*> > vecShroudnodeScores;

    LOCK(cs);

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight)) {
        LogPrintf("CShroudnode::GetShroudnodeByRank -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight);
        return NULL;
    }

    // Fill scores
    BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {

        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive && !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecShroudnodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecShroudnodeScores.rbegin(), vecShroudnodeScores.rend(), CompareScoreMN());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CShroudnode*)& s, vecShroudnodeScores){
        rank++;
        if(rank == nRank) {
            return s.second;
        }
    }

    return NULL;
}

void CShroudnodeMan::ProcessShroudnodeConnections()
{
    //we don't care about this for regtest
    if(Params().NetworkIDString() == CBaseChainParams::REGTEST) return;

    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes) {
        if(pnode->fShroudnode) {
            if(darkSendPool.pSubmittedToShroudnode != NULL && pnode->addr == darkSendPool.pSubmittedToShroudnode->addr) continue;
            // LogPrintf("Closing Shroudnode connection: peer=%d, addr=%s\n", pnode->id, pnode->addr.ToString());
            pnode->fDisconnect = true;
        }
    }
}

std::pair<CService, std::set<uint256> > CShroudnodeMan::PopScheduledMnbRequestConnection()
{
    LOCK(cs);
    if(listScheduledMnbRequestConnections.empty()) {
        return std::make_pair(CService(), std::set<uint256>());
    }

    std::set<uint256> setResult;

    listScheduledMnbRequestConnections.sort();
    std::pair<CService, uint256> pairFront = listScheduledMnbRequestConnections.front();

    // squash hashes from requests with the same CService as the first one into setResult
    std::list< std::pair<CService, uint256> >::iterator it = listScheduledMnbRequestConnections.begin();
    while(it != listScheduledMnbRequestConnections.end()) {
        if(pairFront.first == it->first) {
            setResult.insert(it->second);
            it = listScheduledMnbRequestConnections.erase(it);
        } else {
            // since list is sorted now, we can be sure that there is no more hashes left
            // to ask for from this addr
            break;
        }
    }
    return std::make_pair(pairFront.first, setResult);
}


void CShroudnodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{

//    LogPrint("shroudnode", "CShroudnodeMan::ProcessMessage, strCommand=%s\n", strCommand);
    if(fLiteMode) return; // disable all Index specific functionality
    if(!shroudnodeSync.IsBlockchainSynced()) return;

    if (strCommand == NetMsgType::MNANNOUNCE) { //Shroudnode Broadcast
        CShroudnodeBroadcast mnb;
        vRecv >> mnb;

        pfrom->setAskFor.erase(mnb.GetHash());

        LogPrintf("MNANNOUNCE -- Shroudnode announce, shroudnode=%s\n", mnb.vin.prevout.ToStringShort());

        int nDos = 0;

        if (CheckMnbAndUpdateShroudnodeList(pfrom, mnb, nDos)) {
            // use announced Shroudnode as a peer
            addrman.Add(CAddress(mnb.addr, NODE_NETWORK), pfrom->addr, 2*60*60);
        } else if(nDos > 0) {
            Misbehaving(pfrom->GetId(), nDos);
        }

        if(fShroudnodesAdded) {
            NotifyShroudnodeUpdates();
        }
    } else if (strCommand == NetMsgType::MNPING) { //Shroudnode Ping

        CShroudnodePing mnp;
        vRecv >> mnp;

        uint256 nHash = mnp.GetHash();

        pfrom->setAskFor.erase(nHash);

        LogPrint("shroudnode", "MNPING -- Shroudnode ping, shroudnode=%s\n", mnp.vin.prevout.ToStringShort());

        // Need LOCK2 here to ensure consistent locking order because the CheckAndUpdate call below locks cs_main
        LOCK2(cs_main, cs);

        if(mapSeenShroudnodePing.count(nHash)) return; //seen
        mapSeenShroudnodePing.insert(std::make_pair(nHash, mnp));

        LogPrint("shroudnode", "MNPING -- Shroudnode ping, shroudnode=%s new\n", mnp.vin.prevout.ToStringShort());

        // see if we have this Shroudnode
        CShroudnode* pmn = mnodeman.Find(mnp.vin);

        // too late, new MNANNOUNCE is required
        if(pmn && pmn->IsNewStartRequired()) return;

        int nDos = 0;
        if(mnp.CheckAndUpdate(pmn, false, nDos)) return;

        if(nDos > 0) {
            // if anything significant failed, mark that node
            Misbehaving(pfrom->GetId(), nDos);
        } else if(pmn != NULL) {
            // nothing significant failed, mn is a known one too
            return;
        }

        // something significant is broken or mn is unknown,
        // we might have to ask for a shroudnode entry once
        AskForMN(pfrom, mnp.vin);

    } else if (strCommand == NetMsgType::DSEG) { //Get Shroudnode list or specific entry
        // Ignore such requests until we are fully synced.
        // We could start processing this after shroudnode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!shroudnodeSync.IsSynced()) return;

        CTxIn vin;
        vRecv >> vin;

        LogPrint("shroudnode", "DSEG -- Shroudnode list, shroudnode=%s\n", vin.prevout.ToStringShort());

        LOCK(cs);

        if(vin == CTxIn()) { //only should ask for this once
            //local network
            bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());

            if(!isLocal && Params().NetworkIDString() == CBaseChainParams::MAIN) {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForShroudnodeList.find(pfrom->addr);
                if (i != mAskedUsForShroudnodeList.end()){
                    int64_t t = (*i).second;
                    if (GetTime() < t) {
                        Misbehaving(pfrom->GetId(), 34);
                        LogPrintf("DSEG -- peer already asked me for the list, peer=%d\n", pfrom->id);
                        return;
                    }
                }
                int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
                mAskedUsForShroudnodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok

        int nInvCount = 0;

        BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {
            if (vin != CTxIn() && vin != mn.vin) continue; // asked for specific vin but we are not there yet
            if (mn.addr.IsRFC1918() || mn.addr.IsLocal()) continue; // do not send local network shroudnode
            if (mn.IsUpdateRequired()) continue; // do not send outdated shroudnodes

            LogPrint("shroudnode", "DSEG -- Sending Shroudnode entry: shroudnode=%s  addr=%s\n", mn.vin.prevout.ToStringShort(), mn.addr.ToString());
            CShroudnodeBroadcast mnb = CShroudnodeBroadcast(mn);
            uint256 hash = mnb.GetHash();
            pfrom->PushInventory(CInv(MSG_SHROUDNODE_ANNOUNCE, hash));
            pfrom->PushInventory(CInv(MSG_SHROUDNODE_PING, mn.lastPing.GetHash()));
            nInvCount++;

            if (!mapSeenShroudnodeBroadcast.count(hash)) {
                mapSeenShroudnodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));
            }

            if (vin == mn.vin) {
                LogPrintf("DSEG -- Sent 1 Shroudnode inv to peer %d\n", pfrom->id);
                return;
            }
        }

        if(vin == CTxIn()) {
            pfrom->PushMessage(NetMsgType::SYNCSTATUSCOUNT, SHROUDNODE_SYNC_LIST, nInvCount);
            LogPrintf("DSEG -- Sent %d Shroudnode invs to peer %d\n", nInvCount, pfrom->id);
            return;
        }
        // smth weird happen - someone asked us for vin we have no idea about?
        LogPrint("shroudnode", "DSEG -- No invs sent to peer %d\n", pfrom->id);

    } else if (strCommand == NetMsgType::MNVERIFY) { // Shroudnode Verify

        // Need LOCK2 here to ensure consistent locking order because the all functions below call GetBlockHash which locks cs_main
        LOCK2(cs_main, cs);

        CShroudnodeVerification mnv;
        vRecv >> mnv;

        if(mnv.vchSig1.empty()) {
            // CASE 1: someone asked me to verify myself /IP we are using/
            SendVerifyReply(pfrom, mnv);
        } else if (mnv.vchSig2.empty()) {
            // CASE 2: we _probably_ got verification we requested from some shroudnode
            ProcessVerifyReply(pfrom, mnv);
        } else {
            // CASE 3: we _probably_ got verification broadcast signed by some shroudnode which verified another one
            ProcessVerifyBroadcast(pfrom, mnv);
        }
    }
}

// Verification of shroudnodes via unique direct requests.

void CShroudnodeMan::DoFullVerificationStep()
{
    if(activeShroudnode.vin == CTxIn()) return;
    if(!shroudnodeSync.IsSynced()) return;

    std::vector<std::pair<int, CShroudnode> > vecShroudnodeRanks = GetShroudnodeRanks(pCurrentBlockIndex->nHeight - 1, MIN_POSE_PROTO_VERSION);

    // Need LOCK2 here to ensure consistent locking order because the SendVerifyRequest call below locks cs_main
    // through GetHeight() signal in ConnectNode
    LOCK2(cs_main, cs);

    int nCount = 0;

    int nMyRank = -1;
    int nRanksTotal = (int)vecShroudnodeRanks.size();

    // send verify requests only if we are in top MAX_POSE_RANK
    std::vector<std::pair<int, CShroudnode> >::iterator it = vecShroudnodeRanks.begin();
    while(it != vecShroudnodeRanks.end()) {
        if(it->first > MAX_POSE_RANK) {
            LogPrint("shroudnode", "CShroudnodeMan::DoFullVerificationStep -- Must be in top %d to send verify request\n",
                        (int)MAX_POSE_RANK);
            return;
        }
        if(it->second.vin == activeShroudnode.vin) {
            nMyRank = it->first;
            LogPrint("shroudnode", "CShroudnodeMan::DoFullVerificationStep -- Found self at rank %d/%d, verifying up to %d shroudnodes\n",
                        nMyRank, nRanksTotal, (int)MAX_POSE_CONNECTIONS);
            break;
        }
        ++it;
    }

    // edge case: list is too short and this shroudnode is not enabled
    if(nMyRank == -1) return;

    // send verify requests to up to MAX_POSE_CONNECTIONS shroudnodes
    // starting from MAX_POSE_RANK + nMyRank and using MAX_POSE_CONNECTIONS as a step
    int nOffset = MAX_POSE_RANK + nMyRank - 1;
    if(nOffset >= (int)vecShroudnodeRanks.size()) return;

    std::vector<CShroudnode*> vSortedByAddr;
    BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {
        vSortedByAddr.push_back(&mn);
    }

    sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

    it = vecShroudnodeRanks.begin() + nOffset;
    while(it != vecShroudnodeRanks.end()) {
        if(it->second.IsPoSeVerified() || it->second.IsPoSeBanned()) {
            LogPrint("shroudnode", "CShroudnodeMan::DoFullVerificationStep -- Already %s%s%s shroudnode %s address %s, skipping...\n",
                        it->second.IsPoSeVerified() ? "verified" : "",
                        it->second.IsPoSeVerified() && it->second.IsPoSeBanned() ? " and " : "",
                        it->second.IsPoSeBanned() ? "banned" : "",
                        it->second.vin.prevout.ToStringShort(), it->second.addr.ToString());
            nOffset += MAX_POSE_CONNECTIONS;
            if(nOffset >= (int)vecShroudnodeRanks.size()) break;
            it += MAX_POSE_CONNECTIONS;
            continue;
        }
        LogPrint("shroudnode", "CShroudnodeMan::DoFullVerificationStep -- Verifying shroudnode %s rank %d/%d address %s\n",
                    it->second.vin.prevout.ToStringShort(), it->first, nRanksTotal, it->second.addr.ToString());
        if(SendVerifyRequest(CAddress(it->second.addr, NODE_NETWORK), vSortedByAddr)) {
            nCount++;
            if(nCount >= MAX_POSE_CONNECTIONS) break;
        }
        nOffset += MAX_POSE_CONNECTIONS;
        if(nOffset >= (int)vecShroudnodeRanks.size()) break;
        it += MAX_POSE_CONNECTIONS;
    }

    LogPrint("shroudnode", "CShroudnodeMan::DoFullVerificationStep -- Sent verification requests to %d shroudnodes\n", nCount);
}

// This function tries to find shroudnodes with the same addr,
// find a verified one and ban all the other. If there are many nodes
// with the same addr but none of them is verified yet, then none of them are banned.
// It could take many times to run this before most of the duplicate nodes are banned.

void CShroudnodeMan::CheckSameAddr()
{
    if(!shroudnodeSync.IsSynced() || vShroudnodes.empty()) return;

    std::vector<CShroudnode*> vBan;
    std::vector<CShroudnode*> vSortedByAddr;

    {
        LOCK(cs);

        CShroudnode* pprevShroudnode = NULL;
        CShroudnode* pverifiedShroudnode = NULL;

        BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {
            vSortedByAddr.push_back(&mn);
        }

        sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

        BOOST_FOREACH(CShroudnode* pmn, vSortedByAddr) {
            // check only (pre)enabled shroudnodes
            if(!pmn->IsEnabled() && !pmn->IsPreEnabled()) continue;
            // initial step
            if(!pprevShroudnode) {
                pprevShroudnode = pmn;
                pverifiedShroudnode = pmn->IsPoSeVerified() ? pmn : NULL;
                continue;
            }
            // second+ step
            if(pmn->addr == pprevShroudnode->addr) {
                if(pverifiedShroudnode) {
                    // another shroudnode with the same ip is verified, ban this one
                    vBan.push_back(pmn);
                } else if(pmn->IsPoSeVerified()) {
                    // this shroudnode with the same ip is verified, ban previous one
                    vBan.push_back(pprevShroudnode);
                    // and keep a reference to be able to ban following shroudnodes with the same ip
                    pverifiedShroudnode = pmn;
                }
            } else {
                pverifiedShroudnode = pmn->IsPoSeVerified() ? pmn : NULL;
            }
            pprevShroudnode = pmn;
        }
    }

    // ban duplicates
    BOOST_FOREACH(CShroudnode* pmn, vBan) {
        LogPrintf("CShroudnodeMan::CheckSameAddr -- increasing PoSe ban score for shroudnode %s\n", pmn->vin.prevout.ToStringShort());
        pmn->IncreasePoSeBanScore();
    }
}

bool CShroudnodeMan::SendVerifyRequest(const CAddress& addr, const std::vector<CShroudnode*>& vSortedByAddr)
{
    if(netfulfilledman.HasFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        // we already asked for verification, not a good idea to do this too often, skip it
        LogPrint("shroudnode", "CShroudnodeMan::SendVerifyRequest -- too many requests, skipping... addr=%s\n", addr.ToString());
        return false;
    }

    CNode* pnode = ConnectNode(addr, NULL, false, true);
    if(pnode == NULL) {
        LogPrintf("CShroudnodeMan::SendVerifyRequest -- can't connect to node to verify it, addr=%s\n", addr.ToString());
        return false;
    }

    netfulfilledman.AddFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request");
    // use random nonce, store it and require node to reply with correct one later
    CShroudnodeVerification mnv(addr, GetRandInt(999999), pCurrentBlockIndex->nHeight - 1);
    mWeAskedForVerification[addr] = mnv;
    LogPrintf("CShroudnodeMan::SendVerifyRequest -- verifying node using nonce %d addr=%s\n", mnv.nonce, addr.ToString());
    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);

    return true;
}

void CShroudnodeMan::SendVerifyReply(CNode* pnode, CShroudnodeVerification& mnv)
{
    // only shroudnodes can sign this, why would someone ask regular node?
    if(!fShroudNode) {
        // do not ban, malicious node might be using my IP
        // and trying to confuse the node which tries to verify it
        return;
    }

    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply")) {
//        // peer should not ask us that often
        LogPrintf("ShroudnodeMan::SendVerifyReply -- ERROR: peer already asked me recently, peer=%d\n", pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        LogPrintf("ShroudnodeMan::SendVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    std::string strMessage = strprintf("%s%d%s", activeShroudnode.service.ToString(), mnv.nonce, blockHash.ToString());

    if(!darkSendSigner.SignMessage(strMessage, mnv.vchSig1, activeShroudnode.keyShroudnode)) {
        LogPrintf("ShroudnodeMan::SendVerifyReply -- SignMessage() failed\n");
        return;
    }

    std::string strError;

    if(!darkSendSigner.VerifyMessage(activeShroudnode.pubKeyShroudnode, mnv.vchSig1, strMessage, strError)) {
        LogPrintf("ShroudnodeMan::SendVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
        return;
    }

    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);
    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply");
}

void CShroudnodeMan::ProcessVerifyReply(CNode* pnode, CShroudnodeVerification& mnv)
{
    std::string strError;

    // did we even ask for it? if that's the case we should have matching fulfilled request
    if(!netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        LogPrintf("CShroudnodeMan::ProcessVerifyReply -- ERROR: we didn't ask for verification of %s, peer=%d\n", pnode->addr.ToString(), pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    // Received nonce for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nonce != mnv.nonce) {
        LogPrintf("CShroudnodeMan::ProcessVerifyReply -- ERROR: wrong nounce: requested=%d, received=%d, peer=%d\n",
                    mWeAskedForVerification[pnode->addr].nonce, mnv.nonce, pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    // Received nBlockHeight for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nBlockHeight != mnv.nBlockHeight) {
        LogPrintf("CShroudnodeMan::ProcessVerifyReply -- ERROR: wrong nBlockHeight: requested=%d, received=%d, peer=%d\n",
                    mWeAskedForVerification[pnode->addr].nBlockHeight, mnv.nBlockHeight, pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        LogPrintf("ShroudnodeMan::ProcessVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

//    // we already verified this address, why node is spamming?
    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done")) {
        LogPrintf("CShroudnodeMan::ProcessVerifyReply -- ERROR: already verified %s recently\n", pnode->addr.ToString());
        Misbehaving(pnode->id, 20);
        return;
    }

    {
        LOCK(cs);

        CShroudnode* prealShroudnode = NULL;
        std::vector<CShroudnode*> vpShroudnodesToBan;
        std::vector<CShroudnode>::iterator it = vShroudnodes.begin();
        std::string strMessage1 = strprintf("%s%d%s", pnode->addr.ToString(), mnv.nonce, blockHash.ToString());
        while(it != vShroudnodes.end()) {
            if(CAddress(it->addr, NODE_NETWORK) == pnode->addr) {
                if(darkSendSigner.VerifyMessage(it->pubKeyShroudnode, mnv.vchSig1, strMessage1, strError)) {
                    // found it!
                    prealShroudnode = &(*it);
                    if(!it->IsPoSeVerified()) {
                        it->DecreasePoSeBanScore();
                    }
                    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done");

                    // we can only broadcast it if we are an activated shroudnode
                    if(activeShroudnode.vin == CTxIn()) continue;
                    // update ...
                    mnv.addr = it->addr;
                    mnv.vin1 = it->vin;
                    mnv.vin2 = activeShroudnode.vin;
                    std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                            mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());
                    // ... and sign it
                    if(!darkSendSigner.SignMessage(strMessage2, mnv.vchSig2, activeShroudnode.keyShroudnode)) {
                        LogPrintf("ShroudnodeMan::ProcessVerifyReply -- SignMessage() failed\n");
                        return;
                    }

                    std::string strError;

                    if(!darkSendSigner.VerifyMessage(activeShroudnode.pubKeyShroudnode, mnv.vchSig2, strMessage2, strError)) {
                        LogPrintf("ShroudnodeMan::ProcessVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
                        return;
                    }

                    mWeAskedForVerification[pnode->addr] = mnv;
                    mnv.Relay();

                } else {
                    vpShroudnodesToBan.push_back(&(*it));
                }
            }
            ++it;
        }
        // no real shroudnode found?...
        if(!prealShroudnode) {
            // this should never be the case normally,
            // only if someone is trying to game the system in some way or smth like that
            LogPrintf("CShroudnodeMan::ProcessVerifyReply -- ERROR: no real shroudnode found for addr %s\n", pnode->addr.ToString());
            Misbehaving(pnode->id, 20);
            return;
        }
        LogPrintf("CShroudnodeMan::ProcessVerifyReply -- verified real shroudnode %s for addr %s\n",
                    prealShroudnode->vin.prevout.ToStringShort(), pnode->addr.ToString());
        // increase ban score for everyone else
        BOOST_FOREACH(CShroudnode* pmn, vpShroudnodesToBan) {
            pmn->IncreasePoSeBanScore();
            LogPrint("shroudnode", "CShroudnodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        prealShroudnode->vin.prevout.ToStringShort(), pnode->addr.ToString(), pmn->nPoSeBanScore);
        }
        LogPrintf("CShroudnodeMan::ProcessVerifyBroadcast -- PoSe score increased for %d fake shroudnodes, addr %s\n",
                    (int)vpShroudnodesToBan.size(), pnode->addr.ToString());
    }
}

void CShroudnodeMan::ProcessVerifyBroadcast(CNode* pnode, const CShroudnodeVerification& mnv)
{
    std::string strError;

    if(mapSeenShroudnodeVerification.find(mnv.GetHash()) != mapSeenShroudnodeVerification.end()) {
        // we already have one
        return;
    }
    mapSeenShroudnodeVerification[mnv.GetHash()] = mnv;

    // we don't care about history
    if(mnv.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS) {
        LogPrint("shroudnode", "ShroudnodeMan::ProcessVerifyBroadcast -- Outdated: current block %d, verification block %d, peer=%d\n",
                    pCurrentBlockIndex->nHeight, mnv.nBlockHeight, pnode->id);
        return;
    }

    if(mnv.vin1.prevout == mnv.vin2.prevout) {
        LogPrint("shroudnode", "ShroudnodeMan::ProcessVerifyBroadcast -- ERROR: same vins %s, peer=%d\n",
                    mnv.vin1.prevout.ToStringShort(), pnode->id);
        // that was NOT a good idea to cheat and verify itself,
        // ban the node we received such message from
        Misbehaving(pnode->id, 100);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        LogPrintf("ShroudnodeMan::ProcessVerifyBroadcast -- Can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    int nRank = GetShroudnodeRank(mnv.vin2, mnv.nBlockHeight, MIN_POSE_PROTO_VERSION);

    if (nRank == -1) {
        LogPrint("shroudnode", "CShroudnodeMan::ProcessVerifyBroadcast -- Can't calculate rank for shroudnode %s\n",
                    mnv.vin2.prevout.ToStringShort());
        return;
    }

    if(nRank > MAX_POSE_RANK) {
        LogPrint("shroudnode", "CShroudnodeMan::ProcessVerifyBroadcast -- Mastrernode %s is not in top %d, current rank %d, peer=%d\n",
                    mnv.vin2.prevout.ToStringShort(), (int)MAX_POSE_RANK, nRank, pnode->id);
        return;
    }

    {
        LOCK(cs);

        std::string strMessage1 = strprintf("%s%d%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString());
        std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());

        CShroudnode* pmn1 = Find(mnv.vin1);
        if(!pmn1) {
            LogPrintf("CShroudnodeMan::ProcessVerifyBroadcast -- can't find shroudnode1 %s\n", mnv.vin1.prevout.ToStringShort());
            return;
        }

        CShroudnode* pmn2 = Find(mnv.vin2);
        if(!pmn2) {
            LogPrintf("CShroudnodeMan::ProcessVerifyBroadcast -- can't find shroudnode2 %s\n", mnv.vin2.prevout.ToStringShort());
            return;
        }

        if(pmn1->addr != mnv.addr) {
            LogPrintf("CShroudnodeMan::ProcessVerifyBroadcast -- addr %s do not match %s\n", mnv.addr.ToString(), pnode->addr.ToString());
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn1->pubKeyShroudnode, mnv.vchSig1, strMessage1, strError)) {
            LogPrintf("ShroudnodeMan::ProcessVerifyBroadcast -- VerifyMessage() for shroudnode1 failed, error: %s\n", strError);
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn2->pubKeyShroudnode, mnv.vchSig2, strMessage2, strError)) {
            LogPrintf("ShroudnodeMan::ProcessVerifyBroadcast -- VerifyMessage() for shroudnode2 failed, error: %s\n", strError);
            return;
        }

        if(!pmn1->IsPoSeVerified()) {
            pmn1->DecreasePoSeBanScore();
        }
        mnv.Relay();

        LogPrintf("CShroudnodeMan::ProcessVerifyBroadcast -- verified shroudnode %s for addr %s\n",
                    pmn1->vin.prevout.ToStringShort(), pnode->addr.ToString());

        // increase ban score for everyone else with the same addr
        int nCount = 0;
        BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {
            if(mn.addr != mnv.addr || mn.vin.prevout == mnv.vin1.prevout) continue;
            mn.IncreasePoSeBanScore();
            nCount++;
            LogPrint("shroudnode", "CShroudnodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        mn.vin.prevout.ToStringShort(), mn.addr.ToString(), mn.nPoSeBanScore);
        }
        LogPrintf("CShroudnodeMan::ProcessVerifyBroadcast -- PoSe score incresed for %d fake shroudnodes, addr %s\n",
                    nCount, pnode->addr.ToString());
    }
}

std::string CShroudnodeMan::ToString() const
{
    std::ostringstream info;

    info << "Shroudnodes: " << (int)vShroudnodes.size() <<
            ", peers who asked us for Shroudnode list: " << (int)mAskedUsForShroudnodeList.size() <<
            ", peers we asked for Shroudnode list: " << (int)mWeAskedForShroudnodeList.size() <<
            ", entries in Shroudnode list we asked for: " << (int)mWeAskedForShroudnodeListEntry.size() <<
            ", shroudnode index size: " << indexShroudnodes.GetSize() <<
            ", nDsqCount: " << (int)nDsqCount;

    return info.str();
}

void CShroudnodeMan::UpdateShroudnodeList(CShroudnodeBroadcast mnb)
{
    try {
        LogPrintf("CShroudnodeMan::UpdateShroudnodeList\n");
        LOCK2(cs_main, cs);
        mapSeenShroudnodePing.insert(std::make_pair(mnb.lastPing.GetHash(), mnb.lastPing));
        mapSeenShroudnodeBroadcast.insert(std::make_pair(mnb.GetHash(), std::make_pair(GetTime(), mnb)));

        LogPrintf("CShroudnodeMan::UpdateShroudnodeList -- shroudnode=%s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());

        CShroudnode *pmn = Find(mnb.vin);
        if (pmn == NULL) {
            CShroudnode mn(mnb);
            if (Add(mn)) {
                shroudnodeSync.AddedShroudnodeList();
                GetMainSignals().UpdatedShroudnode(mn);
            }
        } else {
            CShroudnodeBroadcast mnbOld = mapSeenShroudnodeBroadcast[CShroudnodeBroadcast(*pmn).GetHash()].second;
            if (pmn->UpdateFromNewBroadcast(mnb)) {
                shroudnodeSync.AddedShroudnodeList();
                GetMainSignals().UpdatedShroudnode(*pmn);
                mapSeenShroudnodeBroadcast.erase(mnbOld.GetHash());
            }
        }
    } catch (const std::exception &e) {
        PrintExceptionContinue(&e, "UpdateShroudnodeList");
    }
}

bool CShroudnodeMan::CheckMnbAndUpdateShroudnodeList(CNode* pfrom, CShroudnodeBroadcast mnb, int& nDos)
{
    // Need LOCK2 here to ensure consistent locking order because the SimpleCheck call below locks cs_main
    LOCK(cs_main);

    {
        LOCK(cs);
        nDos = 0;
        LogPrint("shroudnode", "CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- shroudnode=%s\n", mnb.vin.prevout.ToStringShort());

        uint256 hash = mnb.GetHash();
        if (mapSeenShroudnodeBroadcast.count(hash) && !mnb.fRecovery) { //seen
            LogPrint("shroudnode", "CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- shroudnode=%s seen\n", mnb.vin.prevout.ToStringShort());
            // less then 2 pings left before this MN goes into non-recoverable state, bump sync timeout
            if (GetTime() - mapSeenShroudnodeBroadcast[hash].first > SHROUDNODE_NEW_START_REQUIRED_SECONDS - SHROUDNODE_MIN_MNP_SECONDS * 2) {
                LogPrint("shroudnode", "CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- shroudnode=%s seen update\n", mnb.vin.prevout.ToStringShort());
                mapSeenShroudnodeBroadcast[hash].first = GetTime();
                shroudnodeSync.AddedShroudnodeList();
                GetMainSignals().UpdatedShroudnode(mnb);
            }
            // did we ask this node for it?
            if (pfrom && IsMnbRecoveryRequested(hash) && GetTime() < mMnbRecoveryRequests[hash].first) {
                LogPrint("shroudnode", "CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- mnb=%s seen request\n", hash.ToString());
                if (mMnbRecoveryRequests[hash].second.count(pfrom->addr)) {
                    LogPrint("shroudnode", "CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- mnb=%s seen request, addr=%s\n", hash.ToString(), pfrom->addr.ToString());
                    // do not allow node to send same mnb multiple times in recovery mode
                    mMnbRecoveryRequests[hash].second.erase(pfrom->addr);
                    // does it have newer lastPing?
                    if (mnb.lastPing.sigTime > mapSeenShroudnodeBroadcast[hash].second.lastPing.sigTime) {
                        // simulate Check
                        CShroudnode mnTemp = CShroudnode(mnb);
                        mnTemp.Check();
                        LogPrint("shroudnode", "CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- mnb=%s seen request, addr=%s, better lastPing: %d min ago, projected mn state: %s\n", hash.ToString(), pfrom->addr.ToString(), (GetTime() - mnb.lastPing.sigTime) / 60, mnTemp.GetStateString());
                        if (mnTemp.IsValidStateForAutoStart(mnTemp.nActiveState)) {
                            // this node thinks it's a good one
                            LogPrint("shroudnode", "CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- shroudnode=%s seen good\n", mnb.vin.prevout.ToStringShort());
                            mMnbRecoveryGoodReplies[hash].push_back(mnb);
                        }
                    }
                }
            }
            return true;
        }
        mapSeenShroudnodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));

        LogPrint("shroudnode", "CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- shroudnode=%s new\n", mnb.vin.prevout.ToStringShort());

        if (!mnb.SimpleCheck(nDos)) {
            LogPrint("shroudnode", "CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- SimpleCheck() failed, shroudnode=%s\n", mnb.vin.prevout.ToStringShort());
            return false;
        }

        // search Shroudnode list
        CShroudnode *pmn = Find(mnb.vin);
        if (pmn) {
            CShroudnodeBroadcast mnbOld = mapSeenShroudnodeBroadcast[CShroudnodeBroadcast(*pmn).GetHash()].second;
            if (!mnb.Update(pmn, nDos)) {
                LogPrint("shroudnode", "CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- Update() failed, shroudnode=%s\n", mnb.vin.prevout.ToStringShort());
                return false;
            }
            if (hash != mnbOld.GetHash()) {
                mapSeenShroudnodeBroadcast.erase(mnbOld.GetHash());
            }
        }
    } // end of LOCK(cs);

    if(mnb.CheckOutpoint(nDos)) {
        if(Add(mnb)){
            GetMainSignals().UpdatedShroudnode(mnb);  
        }
        shroudnodeSync.AddedShroudnodeList();
        // if it matches our Shroudnode privkey...
        if(fShroudNode && mnb.pubKeyShroudnode == activeShroudnode.pubKeyShroudnode) {
            mnb.nPoSeBanScore = -SHROUDNODE_POSE_BAN_MAX_SCORE;
            if(mnb.nProtocolVersion == PROTOCOL_VERSION) {
                // ... and PROTOCOL_VERSION, then we've been remotely activated ...
                LogPrintf("CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- Got NEW Shroudnode entry: shroudnode=%s  sigTime=%lld  addr=%s\n",
                            mnb.vin.prevout.ToStringShort(), mnb.sigTime, mnb.addr.ToString());
                activeShroudnode.ManageState();
            } else {
                // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
                // but also do not ban the node we get this message from
                LogPrintf("CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", mnb.nProtocolVersion, PROTOCOL_VERSION);
                return false;
            }
        }
        mnb.RelayShroudNode();
    } else {
        LogPrintf("CShroudnodeMan::CheckMnbAndUpdateShroudnodeList -- Rejected Shroudnode entry: %s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());
        return false;
    }

    return true;
}

void CShroudnodeMan::UpdateLastPaid()
{
    LOCK(cs);
    if(fLiteMode) return;
    if(!pCurrentBlockIndex) {
        // LogPrintf("CShroudnodeMan::UpdateLastPaid, pCurrentBlockIndex=NULL\n");
        return;
    }

    static bool IsFirstRun = true;
    // Do full scan on first run or if we are not a shroudnode
    // (MNs should update this info on every block, so limited scan should be enough for them)
    int nMaxBlocksToScanBack = (IsFirstRun || !fShroudNode) ? mnpayments.GetStorageLimit() : LAST_PAID_SCAN_BLOCKS;

    LogPrint("mnpayments", "CShroudnodeMan::UpdateLastPaid -- nHeight=%d, nMaxBlocksToScanBack=%d, IsFirstRun=%s\n",
                             pCurrentBlockIndex->nHeight, nMaxBlocksToScanBack, IsFirstRun ? "true" : "false");

    BOOST_FOREACH(CShroudnode& mn, vShroudnodes) {
        mn.UpdateLastPaid(pCurrentBlockIndex, nMaxBlocksToScanBack);
    }

    // every time is like the first time if winners list is not synced
    IsFirstRun = !shroudnodeSync.IsWinnersListSynced();
}

void CShroudnodeMan::CheckAndRebuildShroudnodeIndex()
{
    LOCK(cs);

    if(GetTime() - nLastIndexRebuildTime < MIN_INDEX_REBUILD_TIME) {
        return;
    }

    if(indexShroudnodes.GetSize() <= MAX_EXPECTED_INDEX_SIZE) {
        return;
    }

    if(indexShroudnodes.GetSize() <= int(vShroudnodes.size())) {
        return;
    }

    indexShroudnodesOld = indexShroudnodes;
    indexShroudnodes.Clear();
    for(size_t i = 0; i < vShroudnodes.size(); ++i) {
        indexShroudnodes.AddShroudnodeVIN(vShroudnodes[i].vin);
    }

    fIndexRebuilt = true;
    nLastIndexRebuildTime = GetTime();
}

void CShroudnodeMan::UpdateWatchdogVoteTime(const CTxIn& vin)
{
    LOCK(cs);
    CShroudnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->UpdateWatchdogVoteTime();
    nLastWatchdogVoteTime = GetTime();
}

bool CShroudnodeMan::IsWatchdogActive()
{
    LOCK(cs);
    // Check if any shroudnodes have voted recently, otherwise return false
    return (GetTime() - nLastWatchdogVoteTime) <= SHROUDNODE_WATCHDOG_MAX_SECONDS;
}

void CShroudnodeMan::CheckShroudnode(const CTxIn& vin, bool fForce)
{
    LOCK(cs);
    CShroudnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

void CShroudnodeMan::CheckShroudnode(const CPubKey& pubKeyShroudnode, bool fForce)
{
    LOCK(cs);
    CShroudnode* pMN = Find(pubKeyShroudnode);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

int CShroudnodeMan::GetShroudnodeState(const CTxIn& vin)
{
    LOCK(cs);
    CShroudnode* pMN = Find(vin);
    if(!pMN)  {
        return CShroudnode::SHROUDNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

int CShroudnodeMan::GetShroudnodeState(const CPubKey& pubKeyShroudnode)
{
    LOCK(cs);
    CShroudnode* pMN = Find(pubKeyShroudnode);
    if(!pMN)  {
        return CShroudnode::SHROUDNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

bool CShroudnodeMan::IsShroudnodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt)
{
    LOCK(cs);
    CShroudnode* pMN = Find(vin);
    if(!pMN) {
        return false;
    }
    return pMN->IsPingedWithin(nSeconds, nTimeToCheckAt);
}

void CShroudnodeMan::SetShroudnodeLastPing(const CTxIn& vin, const CShroudnodePing& mnp)
{
    LOCK(cs);
    CShroudnode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->SetLastPing(mnp);
    mapSeenShroudnodePing.insert(std::make_pair(mnp.GetHash(), mnp));

    CShroudnodeBroadcast mnb(*pMN);
    uint256 hash = mnb.GetHash();
    if(mapSeenShroudnodeBroadcast.count(hash)) {
        mapSeenShroudnodeBroadcast[hash].second.SetLastPing(mnp);
    }
}

void CShroudnodeMan::UpdatedBlockTip(const CBlockIndex *pindex)
{
    pCurrentBlockIndex = pindex;
    LogPrint("shroudnode", "CShroudnodeMan::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);

    CheckSameAddr();
    
    if(fShroudNode) {
        // normal wallet does not need to update this every block, doing update on rpc call should be enough
        UpdateLastPaid();
    }
}

void CShroudnodeMan::NotifyShroudnodeUpdates()
{
    // Avoid double locking
    bool fShroudnodesAddedLocal = false;
    bool fShroudnodesRemovedLocal = false;
    {
        LOCK(cs);
        fShroudnodesAddedLocal = fShroudnodesAdded;
        fShroudnodesRemovedLocal = fShroudnodesRemoved;
    }

    if(fShroudnodesAddedLocal) {
//        governance.CheckShroudnodeOrphanObjects();
//        governance.CheckShroudnodeOrphanVotes();
    }
    if(fShroudnodesRemovedLocal) {
//        governance.UpdateCachesAndClean();
    }

    LOCK(cs);
    fShroudnodesAdded = false;
    fShroudnodesRemoved = false;
}
