// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The ShroudX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SHROUDNODE_H
#define SHROUDNODE_H

#include "key.h"
#include "main.h"
#include "net.h"
#include "spork.h"
#include "timedata.h"
#include "utiltime.h"

class CShroudnode;
class CShroudnodeBroadcast;
class CShroudnodePing;

static const int SHROUDNODE_CHECK_SECONDS               =   5;
static const int SHROUDNODE_MIN_MNB_SECONDS             =   5 * 60; //BROADCAST_TIME
static const int SHROUDNODE_MIN_MNP_SECONDS             =  10 * 60; //PRE_ENABLE_TIME
static const int SHROUDNODE_EXPIRATION_SECONDS          =  65 * 60;
static const int SHROUDNODE_WATCHDOG_MAX_SECONDS        = 120 * 60;
static const int SHROUDNODE_NEW_START_REQUIRED_SECONDS  = 180 * 60;
static const int SHROUDNODE_COIN_REQUIRED  = 10000; //10k COLLATERAL

static const int SHROUDNODE_POSE_BAN_MAX_SCORE          = 5;
//
// The Shroudnode Ping Class : Contains a different serialize method for sending pings from shroudnodes throughout the network
//

class CShroudnodePing
{
public:
    CTxIn vin;
    uint256 blockHash;
    int64_t sigTime; //mnb message times
    std::vector<unsigned char> vchSig;
    //removed stop

    CShroudnodePing() :
        vin(),
        blockHash(),
        sigTime(0),
        vchSig()
        {}

    CShroudnodePing(CTxIn& vinNew);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vin);
        READWRITE(blockHash);
        READWRITE(sigTime);
        READWRITE(vchSig);
    }

    void swap(CShroudnodePing& first, CShroudnodePing& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.blockHash, second.blockHash);
        swap(first.sigTime, second.sigTime);
        swap(first.vchSig, second.vchSig);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << sigTime;
        return ss.GetHash();
    }

    bool IsExpired() { return GetTime() - sigTime > SHROUDNODE_NEW_START_REQUIRED_SECONDS; }

    bool Sign(CKey& keyShroudnode, CPubKey& pubKeyShroudnode);
    bool CheckSignature(CPubKey& pubKeyShroudnode, int &nDos);
    bool SimpleCheck(int& nDos);
    bool CheckAndUpdate(CShroudnode* pmn, bool fFromNewBroadcast, int& nDos);
    void Relay();

    CShroudnodePing& operator=(CShroudnodePing from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CShroudnodePing& a, const CShroudnodePing& b)
    {
        return a.vin == b.vin && a.blockHash == b.blockHash;
    }
    friend bool operator!=(const CShroudnodePing& a, const CShroudnodePing& b)
    {
        return !(a == b);
    }

};

struct shroudnode_info_t
{
    shroudnode_info_t()
        : vin(),
          addr(),
          pubKeyCollateralAddress(),
          pubKeyShroudnode(),
          sigTime(0),
          nLastDsq(0),
          nTimeLastChecked(0),
          nTimeLastPaid(0),
          nTimeLastWatchdogVote(0),
          nTimeLastPing(0),
          nActiveState(0),
          nProtocolVersion(0),
          fInfoValid(false),
          nRank(0)
        {}

    CTxIn vin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyShroudnode;
    int64_t sigTime; //mnb message time
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int64_t nTimeLastChecked;
    int64_t nTimeLastPaid;
    int64_t nTimeLastWatchdogVote;
    int64_t nTimeLastPing;
    int nActiveState;
    int nProtocolVersion;
    bool fInfoValid;
    int nRank;
};

//
// The Shroudnode Class. For managing the Darksend process. It contains the input of the 1000DRK, signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CShroudnode
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

public:
    enum state {
        SHROUDNODE_PRE_ENABLED,
        SHROUDNODE_ENABLED,
        SHROUDNODE_EXPIRED,
        SHROUDNODE_OUTPOINT_SPENT,
        SHROUDNODE_UPDATE_REQUIRED,
        SHROUDNODE_WATCHDOG_EXPIRED,
        SHROUDNODE_NEW_START_REQUIRED,
        SHROUDNODE_POSE_BAN
    };

    CTxIn vin;
    CService addr;
    CPubKey pubKeyCollateralAddress;
    CPubKey pubKeyShroudnode;
    CShroudnodePing lastPing;
    std::vector<unsigned char> vchSig;
    int64_t sigTime; //mnb message time
    int64_t nLastDsq; //the dsq count from the last dsq broadcast of this node
    int64_t nTimeLastChecked;
    int64_t nTimeLastPaid;
    int64_t nTimeLastWatchdogVote;
    int nActiveState;
    int nCacheCollateralBlock;
    int nBlockLastPaid;
    int nProtocolVersion;
    int nPoSeBanScore;
    int nPoSeBanHeight;
    int nRank;
    bool fAllowMixingTx;
    bool fUnitTest;

    // KEEP TRACK OF GOVERNANCE ITEMS EACH SHROUDNODE HAS VOTE UPON FOR RECALCULATION
    std::map<uint256, int> mapGovernanceObjectsVotedOn;

    CShroudnode();
    CShroudnode(const CShroudnode& other);
    CShroudnode(const CShroudnodeBroadcast& mnb);
    CShroudnode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyShroudnodeNew, int nProtocolVersionIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        LOCK(cs);
        READWRITE(vin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyShroudnode);
        READWRITE(lastPing);
        READWRITE(vchSig);
        READWRITE(sigTime);
        READWRITE(nLastDsq);
        READWRITE(nTimeLastChecked);
        READWRITE(nTimeLastPaid);
        READWRITE(nTimeLastWatchdogVote);
        READWRITE(nActiveState);
        READWRITE(nCacheCollateralBlock);
        READWRITE(nBlockLastPaid);
        READWRITE(nProtocolVersion);
        READWRITE(nPoSeBanScore);
        READWRITE(nPoSeBanHeight);
        READWRITE(fAllowMixingTx);
        READWRITE(fUnitTest);
        READWRITE(mapGovernanceObjectsVotedOn);
    }

    void swap(CShroudnode& first, CShroudnode& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.vin, second.vin);
        swap(first.addr, second.addr);
        swap(first.pubKeyCollateralAddress, second.pubKeyCollateralAddress);
        swap(first.pubKeyShroudnode, second.pubKeyShroudnode);
        swap(first.lastPing, second.lastPing);
        swap(first.vchSig, second.vchSig);
        swap(first.sigTime, second.sigTime);
        swap(first.nLastDsq, second.nLastDsq);
        swap(first.nTimeLastChecked, second.nTimeLastChecked);
        swap(first.nTimeLastPaid, second.nTimeLastPaid);
        swap(first.nTimeLastWatchdogVote, second.nTimeLastWatchdogVote);
        swap(first.nActiveState, second.nActiveState);
        swap(first.nRank, second.nRank);
        swap(first.nCacheCollateralBlock, second.nCacheCollateralBlock);
        swap(first.nBlockLastPaid, second.nBlockLastPaid);
        swap(first.nProtocolVersion, second.nProtocolVersion);
        swap(first.nPoSeBanScore, second.nPoSeBanScore);
        swap(first.nPoSeBanHeight, second.nPoSeBanHeight);
        swap(first.fAllowMixingTx, second.fAllowMixingTx);
        swap(first.fUnitTest, second.fUnitTest);
        swap(first.mapGovernanceObjectsVotedOn, second.mapGovernanceObjectsVotedOn);
    }

    // CALCULATE A RANK AGAINST OF GIVEN BLOCK
    arith_uint256 CalculateScore(const uint256& blockHash);

    bool UpdateFromNewBroadcast(CShroudnodeBroadcast& mnb);

    void Check(bool fForce = false);

    bool IsBroadcastedWithin(int nSeconds) { return GetAdjustedTime() - sigTime < nSeconds; }

    bool IsPingedWithin(int nSeconds, int64_t nTimeToCheckAt = -1)
    {
        if(lastPing == CShroudnodePing()) return false;

        if(nTimeToCheckAt == -1) {
            nTimeToCheckAt = GetAdjustedTime();
        }
        return nTimeToCheckAt - lastPing.sigTime < nSeconds;
    }

    bool IsEnabled() { return nActiveState == SHROUDNODE_ENABLED; }
    bool IsPreEnabled() { return nActiveState == SHROUDNODE_PRE_ENABLED; }
    bool IsPoSeBanned() { return nActiveState == SHROUDNODE_POSE_BAN; }
    // NOTE: this one relies on nPoSeBanScore, not on nActiveState as everything else here
    bool IsPoSeVerified() { return nPoSeBanScore <= -SHROUDNODE_POSE_BAN_MAX_SCORE; }
    bool IsExpired() { return nActiveState == SHROUDNODE_EXPIRED; }
    bool IsOutpointSpent() { return nActiveState == SHROUDNODE_OUTPOINT_SPENT; }
    bool IsUpdateRequired() { return nActiveState == SHROUDNODE_UPDATE_REQUIRED; }
    bool IsWatchdogExpired() { return nActiveState == SHROUDNODE_WATCHDOG_EXPIRED; }
    bool IsNewStartRequired() { return nActiveState == SHROUDNODE_NEW_START_REQUIRED; }

    static bool IsValidStateForAutoStart(int nActiveStateIn)
    {
        return  nActiveStateIn == SHROUDNODE_ENABLED ||
                nActiveStateIn == SHROUDNODE_PRE_ENABLED ||
                nActiveStateIn == SHROUDNODE_EXPIRED ||
                nActiveStateIn == SHROUDNODE_WATCHDOG_EXPIRED;
    }

    bool IsValidForPayment();

    bool IsMyShroudnode();

    bool IsValidNetAddr();
    static bool IsValidNetAddr(CService addrIn);

    void IncreasePoSeBanScore() { if(nPoSeBanScore < SHROUDNODE_POSE_BAN_MAX_SCORE) nPoSeBanScore++; }
    void DecreasePoSeBanScore() { if(nPoSeBanScore > -SHROUDNODE_POSE_BAN_MAX_SCORE) nPoSeBanScore--; }

    shroudnode_info_t GetInfo();

    static std::string StateToString(int nStateIn);
    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string ToString() const;
    UniValue ToJSON() const;

    void SetStatus(int newState);
    void SetLastPing(CShroudnodePing newShroudnodePing);
    void SetTimeLastPaid(int64_t newTimeLastPaid);
    void SetBlockLastPaid(int newBlockLastPaid);
    void SetRank(int newRank);

    int GetCollateralAge();

    int GetLastPaidTime() const { return nTimeLastPaid; }
    int GetLastPaidBlock() const { return nBlockLastPaid; }
    void UpdateLastPaid(const CBlockIndex *pindex, int nMaxBlocksToScanBack);

    // KEEP TRACK OF EACH GOVERNANCE ITEM INCASE THIS NODE GOES OFFLINE, SO WE CAN RECALC THEIR STATUS
    void AddGovernanceVote(uint256 nGovernanceObjectHash);
    // RECALCULATE CACHED STATUS FLAGS FOR ALL AFFECTED OBJECTS
    void FlagGovernanceItemsAsDirty();

    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void UpdateWatchdogVoteTime();

    CShroudnode& operator=(CShroudnode from)
    {
        swap(*this, from);
        return *this;
    }
    friend bool operator==(const CShroudnode& a, const CShroudnode& b)
    {
        return a.vin == b.vin;
    }
    friend bool operator!=(const CShroudnode& a, const CShroudnode& b)
    {
        return !(a.vin == b.vin);
    }

};


//
// The Shroudnode Broadcast Class : Contains a different serialize method for sending shroudnodes through the network
//

class CShroudnodeBroadcast : public CShroudnode
{
public:

    bool fRecovery;

    CShroudnodeBroadcast() : CShroudnode(), fRecovery(false) {}
    CShroudnodeBroadcast(const CShroudnode& mn) : CShroudnode(mn), fRecovery(false) {}
    CShroudnodeBroadcast(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyShroudnodeNew, int nProtocolVersionIn) :
        CShroudnode(addrNew, vinNew, pubKeyCollateralAddressNew, pubKeyShroudnodeNew, nProtocolVersionIn), fRecovery(false) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vin);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyShroudnode);
        READWRITE(vchSig);
        READWRITE(sigTime);
        READWRITE(nProtocolVersion);
        READWRITE(lastPing);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << pubKeyCollateralAddress;
        ss << sigTime;
        return ss.GetHash();
    }

    /// Create Shroudnode broadcast, needs to be relayed manually after that
    static bool Create(CTxIn vin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keyShroudnodeNew, CPubKey pubKeyShroudnodeNew, std::string &strErrorRet, CShroudnodeBroadcast &mnbRet);
    static bool Create(std::string strService, std::string strKey, std::string strTxHash, std::string strOutputIndex, std::string& strErrorRet, CShroudnodeBroadcast &mnbRet, bool fOffline = false);

    bool SimpleCheck(int& nDos);
    bool Update(CShroudnode* pmn, int& nDos);
    bool CheckOutpoint(int& nDos);

    bool Sign(CKey& keyCollateralAddress);
    bool CheckSignature(int& nDos);
    void RelayShroudNode();
};

class CShroudnodeVerification
{
public:
    CTxIn vin1;
    CTxIn vin2;
    CService addr;
    int nonce;
    int nBlockHeight;
    std::vector<unsigned char> vchSig1;
    std::vector<unsigned char> vchSig2;

    CShroudnodeVerification() :
        vin1(),
        vin2(),
        addr(),
        nonce(0),
        nBlockHeight(0),
        vchSig1(),
        vchSig2()
        {}

    CShroudnodeVerification(CService addr, int nonce, int nBlockHeight) :
        vin1(),
        vin2(),
        addr(addr),
        nonce(nonce),
        nBlockHeight(nBlockHeight),
        vchSig1(),
        vchSig2()
        {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vin1);
        READWRITE(vin2);
        READWRITE(addr);
        READWRITE(nonce);
        READWRITE(nBlockHeight);
        READWRITE(vchSig1);
        READWRITE(vchSig2);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin1;
        ss << vin2;
        ss << addr;
        ss << nonce;
        ss << nBlockHeight;
        return ss.GetHash();
    }

    void Relay() const
    {
        CInv inv(MSG_SHROUDNODE_VERIFY, GetHash());
        RelayInv(inv);
    }
};

#endif
