// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The ShroudX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeshroudnode.h"
#include "darksend.h"
#include "shroudnode-payments.h"
#include "shroudnode-sync.h"
#include "shroudnodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"

#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CShroudnodePayments mnpayments;

CCriticalSection cs_vecPayees;
CCriticalSection cs_mapShroudnodeBlocks;
CCriticalSection cs_mapShroudnodePaymentVotes;

/**
* IsBlockValueValid
*
*   Determine if coinbase outgoing created money is the correct value
*
*   Why is this needed?
*   - In Index some blocks are superblocks, which output much higher amounts of coins
*   - Otherblocks are 10% lower in outgoing value, so in total, no extra coins are created
*   - When non-superblocks are detected, the normal schedule should be maintained
*/

bool IsBlockValueValid(const CBlock &block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet) {
    strErrorRet = "";

    bool isBlockRewardValueMet = (block.vtx[0].GetValueOut() <= blockReward);
    if (fDebug) LogPrintf("block.vtx[0].GetValueOut() %lld <= blockReward %lld\n", block.vtx[0].GetValueOut(), blockReward);

    // we are still using budgets, but we have no data about them anymore,
    // all we know is predefined budget cycle and window

//    const Consensus::Params &consensusParams = Params().GetConsensus();
//
////    if (nBlockHeight < consensusParams.nSuperblockStartBlock) {
//        int nOffset = nBlockHeight % consensusParams.nBudgetPaymentsCycleBlocks;
//        if (nBlockHeight >= consensusParams.nBudgetPaymentsStartBlock &&
//            nOffset < consensusParams.nBudgetPaymentsWindowBlocks) {
//            // NOTE: make sure SPORK_13_OLD_SUPERBLOCK_FLAG is disabled when 12.1 starts to go live
//            if (shroudnodeSync.IsSynced() && !sporkManager.IsSporkActive(SPORK_13_OLD_SUPERBLOCK_FLAG)) {
//                // no budget blocks should be accepted here, if SPORK_13_OLD_SUPERBLOCK_FLAG is disabled
//                LogPrint("gobject", "IsBlockValueValid -- Client synced but budget spork is disabled, checking block value against block reward\n");
//                if (!isBlockRewardValueMet) {
//                    strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, budgets are disabled",
//                                            nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//                }
//                return isBlockRewardValueMet;
//            }
//            LogPrint("gobject", "IsBlockValueValid -- WARNING: Skipping budget block value checks, accepting block\n");
//            // TODO: reprocess blocks to make sure they are legit?
//            return true;
//        }
//        // LogPrint("gobject", "IsBlockValueValid -- Block is not in budget cycle window, checking block value against block reward\n");
//        if (!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, block is not in budget cycle window",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
//        return isBlockRewardValueMet;
//    }

    // superblocks started

//    CAmount nSuperblockMaxValue =  blockReward + CSuperblock::GetPaymentsLimit(nBlockHeight);
//    bool isSuperblockMaxValueMet = (block.vtx[0].GetValueOut() <= nSuperblockMaxValue);
//    bool isSuperblockMaxValueMet = false;

//    LogPrint("gobject", "block.vtx[0].GetValueOut() %lld <= nSuperblockMaxValue %lld\n", block.vtx[0].GetValueOut(), nSuperblockMaxValue);

    if (!shroudnodeSync.IsSynced()) {
        // not enough data but at least it must NOT exceed superblock max value
//        if(CSuperblock::IsValidBlockHeight(nBlockHeight)) {
//            if(fDebug) LogPrintf("IsBlockPayeeValid -- WARNING: Client not synced, checking superblock max bounds only\n");
//            if(!isSuperblockMaxValueMet) {
//                strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded superblock max value",
//                                        nBlockHeight, block.vtx[0].GetValueOut(), nSuperblockMaxValue);
//            }
//            return isSuperblockMaxValueMet;
//        }
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, only regular blocks are allowed at this height",
                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
        }
        // it MUST be a regular block otherwise
        return isBlockRewardValueMet;
    }

    // we are synced, let's try to check as much data as we can

    if (sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED)) {
////        if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
////            if(CSuperblockManager::IsValid(block.vtx[0], nBlockHeight, blockReward)) {
////                LogPrint("gobject", "IsBlockValueValid -- Valid superblock at height %d: %s", nBlockHeight, block.vtx[0].ToString());
////                // all checks are done in CSuperblock::IsValid, nothing to do here
////                return true;
////            }
////
////            // triggered but invalid? that's weird
////            LogPrintf("IsBlockValueValid -- ERROR: Invalid superblock detected at height %d: %s", nBlockHeight, block.vtx[0].ToString());
////            // should NOT allow invalid superblocks, when superblocks are enabled
////            strErrorRet = strprintf("invalid superblock detected at height %d", nBlockHeight);
////            return false;
////        }
//        LogPrint("gobject", "IsBlockValueValid -- No triggered superblock detected at height %d\n", nBlockHeight);
//        if(!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, no triggered superblock detected",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
    } else {
//        // should NOT allow superblocks at all, when superblocks are disabled
        LogPrint("gobject", "IsBlockValueValid -- Superblocks are disabled, no superblocks allowed\n");
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, superblocks are disabled",
                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
        }
    }

    // it MUST be a regular block
    return isBlockRewardValueMet;
}

bool IsBlockPayeeValid(const CTransaction &txNew, int nBlockHeight, CAmount blockReward) {
    // we can only check shroudnode payment /
    const Consensus::Params &consensusParams = Params().GetConsensus();

    if (nBlockHeight < consensusParams.nShroudnodePaymentsStartBlock) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        if (fDebug) LogPrintf("IsBlockPayeeValid -- shroudnode isn't start\n");
        return true;
    }
    if (!shroudnodeSync.IsSynced() && Params().NetworkIDString() != CBaseChainParams::REGTEST) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        if (fDebug) LogPrintf("IsBlockPayeeValid -- WARNING: Client not synced, skipping block payee checks\n");
        return true;
    }

    //check for shroudnode payee
    if (mnpayments.IsTransactionValid(txNew, nBlockHeight, false)) {
        LogPrint("mnpayments", "IsBlockPayeeValid -- Valid shroudnode payment at height %d: %s", nBlockHeight, txNew.ToString());
        return true;
    } else {
        if(sporkManager.IsSporkActive(SPORK_8_SHROUDNODE_PAYMENT_ENFORCEMENT)){
            return false;
        } else {
            LogPrintf("ShroudNode payment enforcement is disabled, accepting block\n");
            return true;
        }
    }
}

void FillBlockPayments(CMutableTransaction &txNew, int nBlockHeight, CAmount shroudnodePayment, CTxOut &txoutShroudnodeRet, std::vector <CTxOut> &voutSuperblockRet) {
    // only create superblocks if spork is enabled AND if superblock is actually triggered
    // (height should be validated inside)
//    if(sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED) &&
//        CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//            LogPrint("gobject", "FillBlockPayments -- triggered superblock creation at height %d\n", nBlockHeight);
//            CSuperblockManager::CreateSuperblock(txNew, nBlockHeight, voutSuperblockRet);
//            return;
//    }

    // FILL BLOCK PAYEE WITH SHROUDNODE PAYMENT OTHERWISE
    mnpayments.FillBlockPayee(txNew, nBlockHeight, shroudnodePayment, txoutShroudnodeRet);
    LogPrint("mnpayments", "FillBlockPayments -- nBlockHeight %d shroudnodePayment %lld txoutShroudnodeRet %s txNew %s",
             nBlockHeight, shroudnodePayment, txoutShroudnodeRet.ToString(), txNew.ToString());
}

std::string GetRequiredPaymentsString(int nBlockHeight) {
    // IF WE HAVE A ACTIVATED TRIGGER FOR THIS HEIGHT - IT IS A SUPERBLOCK, GET THE REQUIRED PAYEES
//    if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//        return CSuperblockManager::GetRequiredPaymentsString(nBlockHeight);
//    }

    // OTHERWISE, PAY SHROUDNODE
    return mnpayments.GetRequiredPaymentsString(nBlockHeight);
}

void CShroudnodePayments::Clear() {
    LOCK2(cs_mapShroudnodeBlocks, cs_mapShroudnodePaymentVotes);
    mapShroudnodeBlocks.clear();
    mapShroudnodePaymentVotes.clear();
}

bool CShroudnodePayments::CanVote(COutPoint outShroudnode, int nBlockHeight) {
    LOCK(cs_mapShroudnodePaymentVotes);

    if (mapShroudnodesLastVote.count(outShroudnode) && mapShroudnodesLastVote[outShroudnode] == nBlockHeight) {
        return false;
    }

    //record this shroudnode voted
    mapShroudnodesLastVote[outShroudnode] = nBlockHeight;
    return true;
}

std::string CShroudnodePayee::ToString() const {
    CTxDestination address1;
    ExtractDestination(scriptPubKey, address1);
    CBitcoinAddress address2(address1);
    std::string str;
    str += "(address: ";
    str += address2.ToString();
    str += ")\n";
    return str;
}

/**
*   FillBlockPayee
*
*   Fill Shroudnode ONLY payment block
*/

void CShroudnodePayments::FillBlockPayee(CMutableTransaction &txNew, int nBlockHeight, CAmount shroudnodePayment, CTxOut &txoutShroudnodeRet) {
    // make sure it's not filled yet
    txoutShroudnodeRet = CTxOut();

    CScript payee;
    bool foundMaxVotedPayee = true;

    if (!mnpayments.GetBlockPayee(nBlockHeight, payee)) {
        // no shroudnode detected...
        // LogPrintf("no shroudnode detected...\n");
        foundMaxVotedPayee = false;
        int nCount = 0;
        CShroudnode *winningNode = mnodeman.GetNextShroudnodeInQueueForPayment(nBlockHeight, true, nCount);
        if (!winningNode) {
            if(Params().NetworkIDString() != CBaseChainParams::REGTEST) {
                // ...and we can't calculate it on our own
                LogPrintf("CShroudnodePayments::FillBlockPayee -- Failed to detect shroudnode to pay\n");
                return;
            }
        }
        // fill payee with locally calculated winner and hope for the best
        if (winningNode) {
            payee = GetScriptForDestination(winningNode->pubKeyCollateralAddress.GetID());
            LogPrintf("payee=%s\n", winningNode->ToString());
        }
        else
            payee = txNew.vout[0].scriptPubKey;//This is only for unit tests scenario on REGTEST
    }
    txoutShroudnodeRet = CTxOut(shroudnodePayment, payee);
    txNew.vout.push_back(txoutShroudnodeRet);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);
    if (foundMaxVotedPayee) {
        LogPrintf("CShroudnodePayments::FillBlockPayee::foundMaxVotedPayee -- Shroudnode payment %lld to %s\n", shroudnodePayment, address2.ToString());
    } else {
        LogPrintf("CShroudnodePayments::FillBlockPayee -- Shroudnode payment %lld to %s\n", shroudnodePayment, address2.ToString());
    }

}

int CShroudnodePayments::GetMinShroudnodePaymentsProto() {
    return sporkManager.IsSporkActive(SPORK_10_SHROUDNODE_PAY_UPDATED_NODES)
           ? MIN_SHROUDNODE_PAYMENT_PROTO_VERSION_2
           : MIN_SHROUDNODE_PAYMENT_PROTO_VERSION_1;
}

void CShroudnodePayments::ProcessMessage(CNode *pfrom, std::string &strCommand, CDataStream &vRecv) {

//    LogPrintf("CShroudnodePayments::ProcessMessage strCommand=%s\n", strCommand);
    // Ignore any payments messages until shroudnode list is synced
    if (!shroudnodeSync.IsShroudnodeListSynced()) return;

    if (fLiteMode) return; // disable all Index specific functionality

    bool fTestNet = (Params().NetworkIDString() == CBaseChainParams::TESTNET);

    if (strCommand == NetMsgType::SHROUDNODEPAYMENTSYNC) { //Shroudnode Payments Request Sync

        // Ignore such requests until we are fully synced.
        // We could start processing this after shroudnode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!shroudnodeSync.IsSynced()) return;

        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (netfulfilledman.HasFulfilledRequest(pfrom->addr, NetMsgType::SHROUDNODEPAYMENTSYNC)) {
            // Asking for the payments list multiple times in a short period of time is no good
            LogPrintf("SHROUDNODEPAYMENTSYNC -- peer already asked me for the list, peer=%d\n", pfrom->id);
            if (!fTestNet) Misbehaving(pfrom->GetId(), 20);
            return;
        }
        netfulfilledman.AddFulfilledRequest(pfrom->addr, NetMsgType::SHROUDNODEPAYMENTSYNC);

        Sync(pfrom);
        LogPrint("mnpayments", "SHROUDNODEPAYMENTSYNC -- Sent Shroudnode payment votes to peer %d\n", pfrom->id);

    } else if (strCommand == NetMsgType::SHROUDNODEPAYMENTVOTE) { // Shroudnode Payments Vote for the Winner

        CShroudnodePaymentVote vote;
        vRecv >> vote;

        if (pfrom->nVersion < GetMinShroudnodePaymentsProto()) return;

        if (!pCurrentBlockIndex) return;

        uint256 nHash = vote.GetHash();

        pfrom->setAskFor.erase(nHash);

        {
            LOCK(cs_mapShroudnodePaymentVotes);
            if (mapShroudnodePaymentVotes.count(nHash)) {
                LogPrint("mnpayments", "SHROUDNODEPAYMENTVOTE -- hash=%s, nHeight=%d seen\n", nHash.ToString(), pCurrentBlockIndex->nHeight);
                return;
            }

            // Avoid processing same vote multiple times
            mapShroudnodePaymentVotes[nHash] = vote;
            // but first mark vote as non-verified,
            // AddPaymentVote() below should take care of it if vote is actually ok
            mapShroudnodePaymentVotes[nHash].MarkAsNotVerified();
        }

        int nFirstBlock = pCurrentBlockIndex->nHeight - GetStorageLimit();
        if (vote.nBlockHeight < nFirstBlock || vote.nBlockHeight > pCurrentBlockIndex->nHeight + 20) {
            LogPrint("mnpayments", "SHROUDNODEPAYMENTVOTE -- vote out of range: nFirstBlock=%d, nBlockHeight=%d, nHeight=%d\n", nFirstBlock, vote.nBlockHeight, pCurrentBlockIndex->nHeight);
            return;
        }

        std::string strError = "";
        if (!vote.IsValid(pfrom, pCurrentBlockIndex->nHeight, strError)) {
            LogPrint("mnpayments", "SHROUDNODEPAYMENTVOTE -- invalid message, error: %s\n", strError);
            return;
        }

        if (!CanVote(vote.vinShroudnode.prevout, vote.nBlockHeight)) {
            LogPrintf("SHROUDNODEPAYMENTVOTE -- shroudnode already voted, shroudnode=%s\n", vote.vinShroudnode.prevout.ToStringShort());
            return;
        }

        shroudnode_info_t mnInfo = mnodeman.GetShroudnodeInfo(vote.vinShroudnode);
        if (!mnInfo.fInfoValid) {
            // mn was not found, so we can't check vote, some info is probably missing
            LogPrintf("SHROUDNODEPAYMENTVOTE -- shroudnode is missing %s\n", vote.vinShroudnode.prevout.ToStringShort());
            mnodeman.AskForMN(pfrom, vote.vinShroudnode);
            return;
        }

        int nDos = 0;
        if (!vote.CheckSignature(mnInfo.pubKeyShroudnode, pCurrentBlockIndex->nHeight, nDos)) {
            if (nDos) {
                LogPrintf("SHROUDNODEPAYMENTVOTE -- ERROR: invalid signature\n");
                if (!fTestNet) Misbehaving(pfrom->GetId(), nDos);
            } else {
                // only warn about anything non-critical (i.e. nDos == 0) in debug mode
                LogPrint("mnpayments", "SHROUDNODEPAYMENTVOTE -- WARNING: invalid signature\n");
            }
            // Either our info or vote info could be outdated.
            // In case our info is outdated, ask for an update,
            mnodeman.AskForMN(pfrom, vote.vinShroudnode);
            // but there is nothing we can do if vote info itself is outdated
            // (i.e. it was signed by a mn which changed its key),
            // so just quit here.
            return;
        }

        CTxDestination address1;
        ExtractDestination(vote.payee, address1);
        CBitcoinAddress address2(address1);

        LogPrint("mnpayments", "SHROUDNODEPAYMENTVOTE -- vote: address=%s, nBlockHeight=%d, nHeight=%d, prevout=%s\n", address2.ToString(), vote.nBlockHeight, pCurrentBlockIndex->nHeight, vote.vinShroudnode.prevout.ToStringShort());

        if (AddPaymentVote(vote)) {
            vote.Relay();
            shroudnodeSync.AddedPaymentVote();
        }
    }
}

bool CShroudnodePaymentVote::Sign() {
    std::string strError;
    std::string strMessage = vinShroudnode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, activeShroudnode.keyShroudnode)) {
        LogPrintf("CShroudnodePaymentVote::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(activeShroudnode.pubKeyShroudnode, vchSig, strMessage, strError)) {
        LogPrintf("CShroudnodePaymentVote::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CShroudnodePayments::GetBlockPayee(int nBlockHeight, CScript &payee) {
    if (mapShroudnodeBlocks.count(nBlockHeight)) {
        return mapShroudnodeBlocks[nBlockHeight].GetBestPayee(payee);
    }

    return false;
}

// Is this shroudnode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 blocks of votes
bool CShroudnodePayments::IsScheduled(CShroudnode &mn, int nNotBlockHeight) {
    LOCK(cs_mapShroudnodeBlocks);

    if (!pCurrentBlockIndex) return false;

    CScript mnpayee;
    mnpayee = GetScriptForDestination(mn.pubKeyCollateralAddress.GetID());

    CScript payee;
    for (int64_t h = pCurrentBlockIndex->nHeight; h <= pCurrentBlockIndex->nHeight + 8; h++) {
        if (h == nNotBlockHeight) continue;
        if (mapShroudnodeBlocks.count(h) && mapShroudnodeBlocks[h].GetBestPayee(payee) && mnpayee == payee) {
            return true;
        }
    }

    return false;
}

bool CShroudnodePayments::AddPaymentVote(const CShroudnodePaymentVote &vote) {
    LogPrint("shroudnode-payments", "CShroudnodePayments::AddPaymentVote\n");
    uint256 blockHash = uint256();
    if (!GetBlockHash(blockHash, vote.nBlockHeight - 101)) return false;

    if (HasVerifiedPaymentVote(vote.GetHash())) return false;

    LOCK2(cs_mapShroudnodeBlocks, cs_mapShroudnodePaymentVotes);

    mapShroudnodePaymentVotes[vote.GetHash()] = vote;

    if (!mapShroudnodeBlocks.count(vote.nBlockHeight)) {
        CShroudnodeBlockPayees blockPayees(vote.nBlockHeight);
        mapShroudnodeBlocks[vote.nBlockHeight] = blockPayees;
    }

    mapShroudnodeBlocks[vote.nBlockHeight].AddPayee(vote);

    return true;
}

bool CShroudnodePayments::HasVerifiedPaymentVote(uint256 hashIn) {
    LOCK(cs_mapShroudnodePaymentVotes);
    std::map<uint256, CShroudnodePaymentVote>::iterator it = mapShroudnodePaymentVotes.find(hashIn);
    return it != mapShroudnodePaymentVotes.end() && it->second.IsVerified();
}

void CShroudnodeBlockPayees::AddPayee(const CShroudnodePaymentVote &vote) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CShroudnodePayee & payee, vecPayees)
    {
        if (payee.GetPayee() == vote.payee) {
            payee.AddVoteHash(vote.GetHash());
            return;
        }
    }
    CShroudnodePayee payeeNew(vote.payee, vote.GetHash());
    vecPayees.push_back(payeeNew);
}

bool CShroudnodeBlockPayees::GetBestPayee(CScript &payeeRet) {
    LOCK(cs_vecPayees);
    LogPrint("mnpayments", "CShroudnodeBlockPayees::GetBestPayee, vecPayees.size()=%s\n", vecPayees.size());
    if (!vecPayees.size()) {
        LogPrint("mnpayments", "CShroudnodeBlockPayees::GetBestPayee -- ERROR: couldn't find any payee\n");
        return false;
    }

    int nVotes = -1;
    BOOST_FOREACH(CShroudnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() > nVotes) {
            payeeRet = payee.GetPayee();
            nVotes = payee.GetVoteCount();
        }
    }

    return (nVotes > -1);
}

bool CShroudnodeBlockPayees::HasPayeeWithVotes(CScript payeeIn, int nVotesReq) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CShroudnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nVotesReq && payee.GetPayee() == payeeIn) {
            return true;
        }
    }

//    LogPrint("mnpayments", "CShroudnodeBlockPayees::HasPayeeWithVotes -- ERROR: couldn't find any payee with %d+ votes\n", nVotesReq);
    return false;
}

bool CShroudnodeBlockPayees::IsTransactionValid(const CTransaction &txNew, bool fMTP,int nHeight) {
    LOCK(cs_vecPayees);

    int nMaxSignatures = 0;
    std::string strPayeesPossible = "";


    CAmount nShroudnodePayment = GetShroudnodePayment(Params().GetConsensus(), fMTP,nHeight);

    //require at least MNPAYMENTS_SIGNATURES_REQUIRED signatures

    BOOST_FOREACH(CShroudnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nMaxSignatures) {
            nMaxSignatures = payee.GetVoteCount();
        }
    }
    LogPrintf("nmaxsig = %s \n",nMaxSignatures);
    // if we don't have at least MNPAYMENTS_SIGNATURES_REQUIRED signatures on a payee, approve whichever is the longest chain
    if (nMaxSignatures < MNPAYMENTS_SIGNATURES_REQUIRED) return true;

    bool hasValidPayee = false;

    BOOST_FOREACH(CShroudnodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
            hasValidPayee = true;

            BOOST_FOREACH(CTxOut txout, txNew.vout) {
                if (payee.GetPayee() == txout.scriptPubKey && nShroudnodePayment == txout.nValue) {
                    LogPrint("mnpayments", "CShroudnodeBlockPayees::IsTransactionValid -- Found required payment\n");
                    return true;
                }
            }

            CTxDestination address1;
            ExtractDestination(payee.GetPayee(), address1);
            CBitcoinAddress address2(address1);

            if (strPayeesPossible == "") {
                strPayeesPossible = address2.ToString();
            } else {
                strPayeesPossible += "," + address2.ToString();
            }
        }
    }

    LogPrintf("CShroudnodeBlockPayees::IsTransactionValid -- ERROR: Missing required payment, possible payees: '%s', amount: %f SHROUD\n", strPayeesPossible, (float) nShroudnodePayment / COIN);
    return false;
}

std::string CShroudnodeBlockPayees::GetRequiredPaymentsString() {
    LOCK(cs_vecPayees);

    std::string strRequiredPayments = "Unknown";

    BOOST_FOREACH(CShroudnodePayee & payee, vecPayees)
    {
        CTxDestination address1;
        ExtractDestination(payee.GetPayee(), address1);
        CBitcoinAddress address2(address1);

        if (strRequiredPayments != "Unknown") {
            strRequiredPayments += ", " + address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        } else {
            strRequiredPayments = address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        }
    }

    return strRequiredPayments;
}

std::string CShroudnodePayments::GetRequiredPaymentsString(int nBlockHeight) {
    LOCK(cs_mapShroudnodeBlocks);

    if (mapShroudnodeBlocks.count(nBlockHeight)) {
        return mapShroudnodeBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CShroudnodePayments::IsTransactionValid(const CTransaction &txNew, int nBlockHeight, bool fMTP) {
    LOCK(cs_mapShroudnodeBlocks);

    if (mapShroudnodeBlocks.count(nBlockHeight)) {
        return mapShroudnodeBlocks[nBlockHeight].IsTransactionValid(txNew, fMTP,nBlockHeight);
    }

    return true;
}

void CShroudnodePayments::CheckAndRemove() {
    if (!pCurrentBlockIndex) return;

    LOCK2(cs_mapShroudnodeBlocks, cs_mapShroudnodePaymentVotes);

    int nLimit = GetStorageLimit();

    std::map<uint256, CShroudnodePaymentVote>::iterator it = mapShroudnodePaymentVotes.begin();
    while (it != mapShroudnodePaymentVotes.end()) {
        CShroudnodePaymentVote vote = (*it).second;

        if (pCurrentBlockIndex->nHeight - vote.nBlockHeight > nLimit) {
            LogPrint("mnpayments", "CShroudnodePayments::CheckAndRemove -- Removing old Shroudnode payment: nBlockHeight=%d\n", vote.nBlockHeight);
            mapShroudnodePaymentVotes.erase(it++);
            mapShroudnodeBlocks.erase(vote.nBlockHeight);
        } else {
            ++it;
        }
    }
    LogPrintf("CShroudnodePayments::CheckAndRemove -- %s\n", ToString());
}

bool CShroudnodePaymentVote::IsValid(CNode *pnode, int nValidationHeight, std::string &strError) {
    CShroudnode *pmn = mnodeman.Find(vinShroudnode);

    if (!pmn) {
        strError = strprintf("Unknown Shroudnode: prevout=%s", vinShroudnode.prevout.ToStringShort());
        // Only ask if we are already synced and still have no idea about that Shroudnode
        if (shroudnodeSync.IsShroudnodeListSynced()) {
            mnodeman.AskForMN(pnode, vinShroudnode);
        }

        return false;
    }

    int nMinRequiredProtocol;
    if (nBlockHeight >= nValidationHeight) {
        // new votes must comply SPORK_10_SHROUDNODE_PAY_UPDATED_NODES rules
        nMinRequiredProtocol = mnpayments.GetMinShroudnodePaymentsProto();
    } else {
        // allow non-updated shroudnodes for old blocks
        nMinRequiredProtocol = MIN_SHROUDNODE_PAYMENT_PROTO_VERSION_1;
    }

    if (pmn->nProtocolVersion < nMinRequiredProtocol) {
        strError = strprintf("Shroudnode protocol is too old: nProtocolVersion=%d, nMinRequiredProtocol=%d", pmn->nProtocolVersion, nMinRequiredProtocol);
        return false;
    }

    // Only shroudnodes should try to check shroudnode rank for old votes - they need to pick the right winner for future blocks.
    // Regular clients (miners included) need to verify shroudnode rank for future block votes only.
    if (!fShroudNode && nBlockHeight < nValidationHeight) return true;

    int nRank = mnodeman.GetShroudnodeRank(vinShroudnode, nBlockHeight - 101, nMinRequiredProtocol, false);

    if (nRank == -1) {
        LogPrint("mnpayments", "CShroudnodePaymentVote::IsValid -- Can't calculate rank for shroudnode %s\n",
                 vinShroudnode.prevout.ToStringShort());
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        // It's common to have shroudnodes mistakenly think they are in the top 10
        // We don't want to print all of these messages in normal mode, debug mode should print though
        strError = strprintf("Shroudnode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        // Only ban for new mnw which is out of bounds, for old mnw MN list itself might be way too much off
        if (nRank > MNPAYMENTS_SIGNATURES_TOTAL * 2 && nBlockHeight > nValidationHeight) {
            strError = strprintf("Shroudnode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL * 2, nRank);
            LogPrintf("CShroudnodePaymentVote::IsValid -- Error: %s\n", strError);
            Misbehaving(pnode->GetId(), 20);
        }
        // Still invalid however
        return false;
    }

    return true;
}

bool CShroudnodePayments::ProcessBlock(int nBlockHeight) {

    // DETERMINE IF WE SHOULD BE VOTING FOR THE NEXT PAYEE

    if (fLiteMode || !fShroudNode) {
        return false;
    }

    // We have little chances to pick the right winner if winners list is out of sync
    // but we have no choice, so we'll try. However it doesn't make sense to even try to do so
    // if we have not enough data about shroudnodes.
    if (!shroudnodeSync.IsShroudnodeListSynced()) {
        return false;
    }

    int nRank = mnodeman.GetShroudnodeRank(activeShroudnode.vin, nBlockHeight - 101, GetMinShroudnodePaymentsProto(), false);

    if (nRank == -1) {
        LogPrint("mnpayments", "CShroudnodePayments::ProcessBlock -- Unknown Shroudnode\n");
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        LogPrint("mnpayments", "CShroudnodePayments::ProcessBlock -- Shroudnode not in the top %d (%d)\n", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        return false;
    }

    // LOCATE THE NEXT SHROUDNODE WHICH SHOULD BE PAID

    LogPrintf("CShroudnodePayments::ProcessBlock -- Start: nBlockHeight=%d, shroudnode=%s\n", nBlockHeight, activeShroudnode.vin.prevout.ToStringShort());

    // pay to the oldest MN that still had no payment but its input is old enough and it was active long enough
    int nCount = 0;
    CShroudnode *pmn = mnodeman.GetNextShroudnodeInQueueForPayment(nBlockHeight, true, nCount);

    if (pmn == NULL) {
        LogPrintf("CShroudnodePayments::ProcessBlock -- ERROR: Failed to find shroudnode to pay\n");
        return false;
    }

    LogPrintf("CShroudnodePayments::ProcessBlock -- Shroudnode found by GetNextShroudnodeInQueueForPayment(): %s\n", pmn->vin.prevout.ToStringShort());


    CScript payee = GetScriptForDestination(pmn->pubKeyCollateralAddress.GetID());

    CShroudnodePaymentVote voteNew(activeShroudnode.vin, nBlockHeight, payee);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);

    // SIGN MESSAGE TO NETWORK WITH OUR SHROUDNODE KEYS

    if (voteNew.Sign()) {
        if (AddPaymentVote(voteNew)) {
            voteNew.Relay();
            return true;
        }
    }

    return false;
}

void CShroudnodePaymentVote::Relay() {
    // do not relay until synced
    if (!shroudnodeSync.IsWinnersListSynced()) {
        LogPrint("shroudnode", "CShroudnodePaymentVote::Relay - shroudnodeSync.IsWinnersListSynced() not sync\n");
        return;
    }
    CInv inv(MSG_SHROUDNODE_PAYMENT_VOTE, GetHash());
    RelayInv(inv);
}

bool CShroudnodePaymentVote::CheckSignature(const CPubKey &pubKeyShroudnode, int nValidationHeight, int &nDos) {
    // do not ban by default
    nDos = 0;

    std::string strMessage = vinShroudnode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    std::string strError = "";
    if (!darkSendSigner.VerifyMessage(pubKeyShroudnode, vchSig, strMessage, strError)) {
        // Only ban for future block vote when we are already synced.
        // Otherwise it could be the case when MN which signed this vote is using another key now
        // and we have no idea about the old one.
        if (shroudnodeSync.IsShroudnodeListSynced() && nBlockHeight > nValidationHeight) {
            nDos = 20;
        }
        return error("CShroudnodePaymentVote::CheckSignature -- Got bad Shroudnode payment signature, shroudnode=%s, error: %s", vinShroudnode.prevout.ToStringShort().c_str(), strError);
    }

    return true;
}

std::string CShroudnodePaymentVote::ToString() const {
    std::ostringstream info;

    info << vinShroudnode.prevout.ToStringShort() <<
         ", " << nBlockHeight <<
         ", " << ScriptToAsmStr(payee) <<
         ", " << (int) vchSig.size();

    return info.str();
}

// Send only votes for future blocks, node should request every other missing payment block individually
void CShroudnodePayments::Sync(CNode *pnode) {
    LOCK(cs_mapShroudnodeBlocks);

    if (!pCurrentBlockIndex) return;

    int nInvCount = 0;

    for (int h = pCurrentBlockIndex->nHeight; h < pCurrentBlockIndex->nHeight + 20; h++) {
        if (mapShroudnodeBlocks.count(h)) {
            BOOST_FOREACH(CShroudnodePayee & payee, mapShroudnodeBlocks[h].vecPayees)
            {
                std::vector <uint256> vecVoteHashes = payee.GetVoteHashes();
                BOOST_FOREACH(uint256 & hash, vecVoteHashes)
                {
                    if (!HasVerifiedPaymentVote(hash)) continue;
                    pnode->PushInventory(CInv(MSG_SHROUDNODE_PAYMENT_VOTE, hash));
                    nInvCount++;
                }
            }
        }
    }

    LogPrintf("CShroudnodePayments::Sync -- Sent %d votes to peer %d\n", nInvCount, pnode->id);
    pnode->PushMessage(NetMsgType::SYNCSTATUSCOUNT, SHROUDNODE_SYNC_MNW, nInvCount);
}

// Request low data/unknown payment blocks in batches directly from some node instead of/after preliminary Sync.
void CShroudnodePayments::RequestLowDataPaymentBlocks(CNode *pnode) {
    if (!pCurrentBlockIndex) return;

    LOCK2(cs_main, cs_mapShroudnodeBlocks);

    std::vector <CInv> vToFetch;
    int nLimit = GetStorageLimit();

    const CBlockIndex *pindex = pCurrentBlockIndex;

    while (pCurrentBlockIndex->nHeight - pindex->nHeight < nLimit) {
        if (!mapShroudnodeBlocks.count(pindex->nHeight)) {
            // We have no idea about this block height, let's ask
            vToFetch.push_back(CInv(MSG_SHROUDNODE_PAYMENT_BLOCK, pindex->GetBlockHash()));
            // We should not violate GETDATA rules
            if (vToFetch.size() == MAX_INV_SZ) {
                LogPrintf("CShroudnodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d blocks\n", pnode->id, MAX_INV_SZ);
                pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
                // Start filling new batch
                vToFetch.clear();
            }
        }
        if (!pindex->pprev) break;
        pindex = pindex->pprev;
    }

    std::map<int, CShroudnodeBlockPayees>::iterator it = mapShroudnodeBlocks.begin();

    while (it != mapShroudnodeBlocks.end()) {
        int nTotalVotes = 0;
        bool fFound = false;
        BOOST_FOREACH(CShroudnodePayee & payee, it->second.vecPayees)
        {
            if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
                fFound = true;
                break;
            }
            nTotalVotes += payee.GetVoteCount();
        }
        // A clear winner (MNPAYMENTS_SIGNATURES_REQUIRED+ votes) was found
        // or no clear winner was found but there are at least avg number of votes
        if (fFound || nTotalVotes >= (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2) {
            // so just move to the next block
            ++it;
            continue;
        }
        // DEBUG
//        DBG (
//            // Let's see why this failed
//            BOOST_FOREACH(CShroudnodePayee& payee, it->second.vecPayees) {
//                CTxDestination address1;
//                ExtractDestination(payee.GetPayee(), address1);
//                CBitcoinAddress address2(address1);
//                printf("payee %s votes %d\n", address2.ToString().c_str(), payee.GetVoteCount());
//            }
//            printf("block %d votes total %d\n", it->first, nTotalVotes);
//        )
        // END DEBUG
        // Low data block found, let's try to sync it
        uint256 hash;
        if (GetBlockHash(hash, it->first)) {
            vToFetch.push_back(CInv(MSG_SHROUDNODE_PAYMENT_BLOCK, hash));
        }
        // We should not violate GETDATA rules
        if (vToFetch.size() == MAX_INV_SZ) {
            LogPrintf("CShroudnodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->id, MAX_INV_SZ);
            pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
            // Start filling new batch
            vToFetch.clear();
        }
        ++it;
    }
    // Ask for the rest of it
    if (!vToFetch.empty()) {
        LogPrintf("CShroudnodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->id, vToFetch.size());
        pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
    }
}

std::string CShroudnodePayments::ToString() const {
    std::ostringstream info;

    info << "Votes: " << (int) mapShroudnodePaymentVotes.size() <<
         ", Blocks: " << (int) mapShroudnodeBlocks.size();

    return info.str();
}

bool CShroudnodePayments::IsEnoughData() {
    float nAverageVotes = (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2;
    int nStorageLimit = GetStorageLimit();
    return GetBlockCount() > nStorageLimit && GetVoteCount() > nStorageLimit * nAverageVotes;
}

int CShroudnodePayments::GetStorageLimit() {
    return std::max(int(mnodeman.size() * nStorageCoeff), nMinBlocksToStore);
}

void CShroudnodePayments::UpdatedBlockTip(const CBlockIndex *pindex) {
    pCurrentBlockIndex = pindex;
    LogPrint("mnpayments", "CShroudnodePayments::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);
    
    ProcessBlock(pindex->nHeight + 5);
}
