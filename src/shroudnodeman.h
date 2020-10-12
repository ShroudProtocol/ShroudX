// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The ShroudX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SHROUDNODEMAN_H
#define SHROUDNODEMAN_H

#include "shroudnode.h"
#include "sync.h"

using namespace std;

class CShroudnodeMan;

extern CShroudnodeMan mnodeman;

/**
 * Provides a forward and reverse index between MN vin's and integers.
 *
 * This mapping is normally add-only and is expected to be permanent
 * It is only rebuilt if the size of the index exceeds the expected maximum number
 * of MN's and the current number of known MN's.
 *
 * The external interface to this index is provided via delegation by CShroudnodeMan
 */
class CShroudnodeIndex
{
public: // Types
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

    typedef std::map<int,CTxIn> rindex_m_t;

    typedef rindex_m_t::iterator rindex_m_it;

    typedef rindex_m_t::const_iterator rindex_m_cit;

private:
    int                  nSize;

    index_m_t            mapIndex;

    rindex_m_t           mapReverseIndex;

public:
    CShroudnodeIndex();

    int GetSize() const {
        return nSize;
    }

    /// Retrieve shroudnode vin by index
    bool Get(int nIndex, CTxIn& vinShroudnode) const;

    /// Get index of a shroudnode vin
    int GetShroudnodeIndex(const CTxIn& vinShroudnode) const;

    void AddShroudnodeVIN(const CTxIn& vinShroudnode);

    void Clear();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(mapIndex);
        if(ser_action.ForRead()) {
            RebuildIndex();
        }
    }

private:
    void RebuildIndex();

};

class CShroudnodeMan
{
public:
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

private:
    static const int MAX_EXPECTED_INDEX_SIZE = 30000;

    /// Only allow 1 index rebuild per hour
    static const int64_t MIN_INDEX_REBUILD_TIME = 3600;

    static const std::string SERIALIZATION_VERSION_STRING;

    static const int DSEG_UPDATE_SECONDS        = 3 * 60 * 60;

    static const int LAST_PAID_SCAN_BLOCKS      = 100;

    static const int MIN_POSE_PROTO_VERSION     = 70203;
    static const int MAX_POSE_CONNECTIONS       = 10;
    static const int MAX_POSE_RANK              = 10;
    static const int MAX_POSE_BLOCKS            = 10;

    static const int MNB_RECOVERY_QUORUM_TOTAL      = 10;
    static const int MNB_RECOVERY_QUORUM_REQUIRED   = 6;
    static const int MNB_RECOVERY_MAX_ASK_ENTRIES   = 10;
    static const int MNB_RECOVERY_WAIT_SECONDS      = 60;
    static const int MNB_RECOVERY_RETRY_SECONDS     = 3 * 60 * 60;


    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    // map to hold all MNs
    std::vector<CShroudnode> vShroudnodes;
    // who's asked for the Shroudnode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForShroudnodeList;
    // who we asked for the Shroudnode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForShroudnodeList;
    // which Shroudnodes we've asked for
    std::map<COutPoint, std::map<CNetAddr, int64_t> > mWeAskedForShroudnodeListEntry;
    // who we asked for the shroudnode verification
    std::map<CNetAddr, CShroudnodeVerification> mWeAskedForVerification;

    // these maps are used for shroudnode recovery from SHROUDNODE_NEW_START_REQUIRED state
    std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > > mMnbRecoveryRequests;
    std::map<uint256, std::vector<CShroudnodeBroadcast> > mMnbRecoveryGoodReplies;
    std::list< std::pair<CService, uint256> > listScheduledMnbRequestConnections;

    int64_t nLastIndexRebuildTime;

    CShroudnodeIndex indexShroudnodes;

    CShroudnodeIndex indexShroudnodesOld;

    /// Set when index has been rebuilt, clear when read
    bool fIndexRebuilt;

    /// Set when shroudnodes are added, cleared when CGovernanceManager is notified
    bool fShroudnodesAdded;

    /// Set when shroudnodes are removed, cleared when CGovernanceManager is notified
    bool fShroudnodesRemoved;

    std::vector<uint256> vecDirtyGovernanceObjectHashes;

    int64_t nLastWatchdogVoteTime;

    friend class CShroudnodeSync;

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, std::pair<int64_t, CShroudnodeBroadcast> > mapSeenShroudnodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CShroudnodePing> mapSeenShroudnodePing;
    // Keep track of all verifications I've seen
    std::map<uint256, CShroudnodeVerification> mapSeenShroudnodeVerification;
    // keep track of dsq count to prevent shroudnodes from gaming darksend queue
    int64_t nDsqCount;


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        LOCK(cs);
        std::string strVersion;
        if(ser_action.ForRead()) {
            READWRITE(strVersion);
        }
        else {
            strVersion = SERIALIZATION_VERSION_STRING; 
            READWRITE(strVersion);
        }

        READWRITE(vShroudnodes);
        READWRITE(mAskedUsForShroudnodeList);
        READWRITE(mWeAskedForShroudnodeList);
        READWRITE(mWeAskedForShroudnodeListEntry);
        READWRITE(mMnbRecoveryRequests);
        READWRITE(mMnbRecoveryGoodReplies);
        READWRITE(nLastWatchdogVoteTime);
        READWRITE(nDsqCount);

        READWRITE(mapSeenShroudnodeBroadcast);
        READWRITE(mapSeenShroudnodePing);
        READWRITE(indexShroudnodes);
        if(ser_action.ForRead() && (strVersion != SERIALIZATION_VERSION_STRING)) {
            Clear();
        }
    }

    CShroudnodeMan();

    /// Add an entry
    bool Add(CShroudnode &mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode *pnode, const CTxIn &vin);
    void AskForMnb(CNode *pnode, const uint256 &hash);

    /// Check all Shroudnodes
    void Check();

    /// Check all Shroudnodes and remove inactive
    void CheckAndRemove();

    /// Clear Shroudnode vector
    void Clear();

    /// Count Shroudnodes filtered by nProtocolVersion.
    /// Shroudnode nProtocolVersion should match or be above the one specified in param here.
    int CountShroudnodes(int nProtocolVersion = -1);
    /// Count enabled Shroudnodes filtered by nProtocolVersion.
    /// Shroudnode nProtocolVersion should match or be above the one specified in param here.
    int CountEnabled(int nProtocolVersion = -1);

    /// Count Shroudnodes by network type - NET_IPV4, NET_IPV6, NET_TOR
    // int CountByIP(int nNetworkType);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    CShroudnode* Find(const std::string &txHash, const std::string outputIndex);
    CShroudnode* Find(const CScript &payee);
    CShroudnode* Find(const CTxIn& vin);
    CShroudnode* Find(const CPubKey& pubKeyShroudnode);

    /// Versions of Find that are safe to use from outside the class
    bool Get(const CPubKey& pubKeyShroudnode, CShroudnode& shroudnode);
    bool Get(const CTxIn& vin, CShroudnode& shroudnode);

    /// Retrieve shroudnode vin by index
    bool Get(int nIndex, CTxIn& vinShroudnode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexShroudnodes.Get(nIndex, vinShroudnode);
    }

    bool GetIndexRebuiltFlag() {
        LOCK(cs);
        return fIndexRebuilt;
    }

    /// Get index of a shroudnode vin
    int GetShroudnodeIndex(const CTxIn& vinShroudnode) {
        LOCK(cs);
        return indexShroudnodes.GetShroudnodeIndex(vinShroudnode);
    }

    /// Get old index of a shroudnode vin
    int GetShroudnodeIndexOld(const CTxIn& vinShroudnode) {
        LOCK(cs);
        return indexShroudnodesOld.GetShroudnodeIndex(vinShroudnode);
    }

    /// Get shroudnode VIN for an old index value
    bool GetShroudnodeVinForIndexOld(int nShroudnodeIndex, CTxIn& vinShroudnodeOut) {
        LOCK(cs);
        return indexShroudnodesOld.Get(nShroudnodeIndex, vinShroudnodeOut);
    }

    /// Get index of a shroudnode vin, returning rebuild flag
    int GetShroudnodeIndex(const CTxIn& vinShroudnode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexShroudnodes.GetShroudnodeIndex(vinShroudnode);
    }

    void ClearOldShroudnodeIndex() {
        LOCK(cs);
        indexShroudnodesOld.Clear();
        fIndexRebuilt = false;
    }

    bool Has(const CTxIn& vin);

    shroudnode_info_t GetShroudnodeInfo(const CTxIn& vin);

    shroudnode_info_t GetShroudnodeInfo(const CPubKey& pubKeyShroudnode);

    char* GetNotQualifyReason(CShroudnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount);

    UniValue GetNotQualifyReasonToUniValue(CShroudnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount);

    /// Find an entry in the shroudnode list that is next to be paid
    CShroudnode* GetNextShroudnodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount);
    /// Same as above but use current block height
    CShroudnode* GetNextShroudnodeInQueueForPayment(bool fFilterSigTime, int& nCount);

    /// Find a random entry
    CShroudnode* FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion = -1);

    std::vector<CShroudnode> GetFullShroudnodeVector() { LOCK(cs); return vShroudnodes; }

    std::vector<std::pair<int, CShroudnode> > GetShroudnodeRanks(int nBlockHeight = -1, int nMinProtocol=0);
    int GetShroudnodeRank(const CTxIn &vin, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);
    CShroudnode* GetShroudnodeByRank(int nRank, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);

    void ProcessShroudnodeConnections();
    std::pair<CService, std::set<uint256> > PopScheduledMnbRequestConnection();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void DoFullVerificationStep();
    void CheckSameAddr();
    bool SendVerifyRequest(const CAddress& addr, const std::vector<CShroudnode*>& vSortedByAddr);
    void SendVerifyReply(CNode* pnode, CShroudnodeVerification& mnv);
    void ProcessVerifyReply(CNode* pnode, CShroudnodeVerification& mnv);
    void ProcessVerifyBroadcast(CNode* pnode, const CShroudnodeVerification& mnv);

    /// Return the number of (unique) Shroudnodes
    int size() { return vShroudnodes.size(); }

    std::string ToString() const;

    /// Update shroudnode list and maps using provided CShroudnodeBroadcast
    void UpdateShroudnodeList(CShroudnodeBroadcast mnb);
    /// Perform complete check and only then update list and maps
    bool CheckMnbAndUpdateShroudnodeList(CNode* pfrom, CShroudnodeBroadcast mnb, int& nDos);
    bool IsMnbRecoveryRequested(const uint256& hash) { return mMnbRecoveryRequests.count(hash); }

    void UpdateLastPaid();

    void CheckAndRebuildShroudnodeIndex();

    void AddDirtyGovernanceObjectHash(const uint256& nHash)
    {
        LOCK(cs);
        vecDirtyGovernanceObjectHashes.push_back(nHash);
    }

    std::vector<uint256> GetAndClearDirtyGovernanceObjectHashes()
    {
        LOCK(cs);
        std::vector<uint256> vecTmp = vecDirtyGovernanceObjectHashes;
        vecDirtyGovernanceObjectHashes.clear();
        return vecTmp;;
    }

    bool IsWatchdogActive();
    void UpdateWatchdogVoteTime(const CTxIn& vin);
    bool AddGovernanceVote(const CTxIn& vin, uint256 nGovernanceObjectHash);
    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void CheckShroudnode(const CTxIn& vin, bool fForce = false);
    void CheckShroudnode(const CPubKey& pubKeyShroudnode, bool fForce = false);

    int GetShroudnodeState(const CTxIn& vin);
    int GetShroudnodeState(const CPubKey& pubKeyShroudnode);

    bool IsShroudnodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt = -1);
    void SetShroudnodeLastPing(const CTxIn& vin, const CShroudnodePing& mnp);

    void UpdatedBlockTip(const CBlockIndex *pindex);

    /**
     * Called to notify CGovernanceManager that the shroudnode index has been updated.
     * Must be called while not holding the CShroudnodeMan::cs mutex
     */
    void NotifyShroudnodeUpdates();

};

#endif
