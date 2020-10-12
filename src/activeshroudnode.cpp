// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The ShroudX Project developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeshroudnode.h"
#include "consensus/consensus.h"
#include "shroudnode.h"
#include "shroudnode-sync.h"
#include "shroudnode-payments.h"
#include "shroudnodeman.h"
#include "protocol.h"
#include "validationinterface.h"

extern CWallet *pwalletMain;

// Keep track of the active Shroudnode
CActiveShroudnode activeShroudnode;

void CActiveShroudnode::ManageState() {
    LogPrint("shroudnode", "CActiveShroudnode::ManageState -- Start\n");
    if (!fShroudNode) {
        LogPrint("shroudnode", "CActiveShroudnode::ManageState -- Not a shroudnode, returning\n");
        return;
    }

    if (Params().NetworkIDString() != CBaseChainParams::REGTEST && !shroudnodeSync.GetBlockchainSynced()) {
        ChangeState(ACTIVE_SHROUDNODE_SYNC_IN_PROCESS);
        LogPrintf("CActiveShroudnode::ManageState -- %s: %s\n", GetStateString(), GetStatus());
        return;
    }

    if (nState == ACTIVE_SHROUDNODE_SYNC_IN_PROCESS) {
        ChangeState(ACTIVE_SHROUDNODE_INITIAL);
    }

    LogPrint("shroudnode", "CActiveShroudnode::ManageState -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);

    if (eType == SHROUDNODE_UNKNOWN) {
        ManageStateInitial();
    }

    if (eType == SHROUDNODE_REMOTE) {
        ManageStateRemote();
    } else if (eType == SHROUDNODE_LOCAL) {
        // Try Remote Start first so the started local shroudnode can be restarted without recreate shroudnode broadcast.
        ManageStateRemote();
        if (nState != ACTIVE_SHROUDNODE_STARTED)
            ManageStateLocal();
    }

    SendShroudnodePing();
}

std::string CActiveShroudnode::GetStateString() const {
    switch (nState) {
        case ACTIVE_SHROUDNODE_INITIAL:
            return "INITIAL";
        case ACTIVE_SHROUDNODE_SYNC_IN_PROCESS:
            return "SYNC_IN_PROCESS";
        case ACTIVE_SHROUDNODE_INPUT_TOO_NEW:
            return "INPUT_TOO_NEW";
        case ACTIVE_SHROUDNODE_NOT_CAPABLE:
            return "NOT_CAPABLE";
        case ACTIVE_SHROUDNODE_STARTED:
            return "STARTED";
        default:
            return "UNKNOWN";
    }
}

void CActiveShroudnode::ChangeState(int state) {
    if(nState!=state){
        nState = state;
    }
}

std::string CActiveShroudnode::GetStatus() const {
    switch (nState) {
        case ACTIVE_SHROUDNODE_INITIAL:
            return "Node just started, not yet activated";
        case ACTIVE_SHROUDNODE_SYNC_IN_PROCESS:
            return "Sync in progress. Must wait until sync is complete to start Shroudnode";
        case ACTIVE_SHROUDNODE_INPUT_TOO_NEW:
            return strprintf("Shroudnode input must have at least %d confirmations",
                             Params().GetConsensus().nShroudnodeMinimumConfirmations);
        case ACTIVE_SHROUDNODE_NOT_CAPABLE:
            return "Not capable shroudnode: " + strNotCapableReason;
        case ACTIVE_SHROUDNODE_STARTED:
            return "Shroudnode successfully started";
        default:
            return "Unknown";
    }
}

std::string CActiveShroudnode::GetTypeString() const {
    std::string strType;
    switch (eType) {
        case SHROUDNODE_UNKNOWN:
            strType = "UNKNOWN";
            break;
        case SHROUDNODE_REMOTE:
            strType = "REMOTE";
            break;
        case SHROUDNODE_LOCAL:
            strType = "LOCAL";
            break;
        default:
            strType = "UNKNOWN";
            break;
    }
    return strType;
}

bool CActiveShroudnode::SendShroudnodePing() {
    if (!fPingerEnabled) {
        LogPrint("shroudnode",
                 "CActiveShroudnode::SendShroudnodePing -- %s: shroudnode ping service is disabled, skipping...\n",
                 GetStateString());
        return false;
    }

    if (!mnodeman.Has(vin)) {
        strNotCapableReason = "Shroudnode not in shroudnode list";
        ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
        LogPrintf("CActiveShroudnode::SendShroudnodePing -- %s: %s\n", GetStateString(), strNotCapableReason);
        return false;
    }

    CShroudnodePing mnp(vin);
    if (!mnp.Sign(keyShroudnode, pubKeyShroudnode)) {
        LogPrintf("CActiveShroudnode::SendShroudnodePing -- ERROR: Couldn't sign Shroudnode Ping\n");
        return false;
    }

    // Update lastPing for our shroudnode in Shroudnode list
    if (mnodeman.IsShroudnodePingedWithin(vin, SHROUDNODE_MIN_MNP_SECONDS, mnp.sigTime)) {
        LogPrintf("CActiveShroudnode::SendShroudnodePing -- Too early to send Shroudnode Ping\n");
        return false;
    }

    mnodeman.SetShroudnodeLastPing(vin, mnp);

    LogPrintf("CActiveShroudnode::SendShroudnodePing -- Relaying ping, collateral=%s\n", vin.ToString());
    mnp.Relay();

    return true;
}

void CActiveShroudnode::ManageStateInitial() {
    LogPrint("shroudnode", "CActiveShroudnode::ManageStateInitial -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);

    // Check that our local network configuration is correct
    if (!fListen) {
        // listen option is probably overwritten by smth else, no good
        ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
        strNotCapableReason = "Shroudnode must accept connections from outside. Make sure listen configuration option is not overwritten by some another parameter.";
        LogPrintf("CActiveShroudnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    bool fFoundLocal = false;
    {
        LOCK(cs_vNodes);

        // First try to find whatever local address is specified by externalip option
        fFoundLocal = GetLocal(service) && CShroudnode::IsValidNetAddr(service);
        if (!fFoundLocal) {
            // nothing and no live connections, can't do anything for now
            if (vNodes.empty()) {
                ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
                strNotCapableReason = "Can't detect valid external address. Will retry when there are some connections available.";
                LogPrintf("CActiveShroudnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
                return;
            }
            // We have some peers, let's try to find our local address from one of them
            BOOST_FOREACH(CNode * pnode, vNodes)
            {
                if (pnode->fSuccessfullyConnected && pnode->addr.IsIPv4()) {
                    fFoundLocal = GetLocal(service, &pnode->addr) && CShroudnode::IsValidNetAddr(service);
                    if (fFoundLocal) break;
                }
            }
        }
    }

    if (!fFoundLocal) {
        ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
        strNotCapableReason = "Can't detect valid external address. Please consider using the externalip configuration option if problem persists. Make sure to use IPv4 address only.";
        LogPrintf("CActiveShroudnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort) {
            ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
            strNotCapableReason = strprintf("Invalid port: %u - only %d is supported on mainnet.", service.GetPort(),
                                            mainnetDefaultPort);
            LogPrintf("CActiveShroudnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
        strNotCapableReason = strprintf("Invalid port: %u - %d is only supported on mainnet.", service.GetPort(),
                                        mainnetDefaultPort);
        LogPrintf("CActiveShroudnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    LogPrintf("CActiveShroudnode::ManageStateInitial -- Checking inbound connection to '%s'\n", service.ToString());
    //TODO
    if (!ConnectNode(CAddress(service, NODE_NETWORK), NULL, false, true)) {
        ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
        strNotCapableReason = "Could not connect to " + service.ToString();
        LogPrintf("CActiveShroudnode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    // Default to REMOTE
    eType = SHROUDNODE_REMOTE;

    // Check if wallet funds are available
    if (!pwalletMain) {
        LogPrintf("CActiveShroudnode::ManageStateInitial -- %s: Wallet not available\n", GetStateString());
        return;
    }

    if (pwalletMain->IsLocked()) {
        LogPrintf("CActiveShroudnode::ManageStateInitial -- %s: Wallet is locked\n", GetStateString());
        return;
    }

    if (pwalletMain->GetBalance() < SHROUDNODE_COIN_REQUIRED * COIN) {
        LogPrintf("CActiveShroudnode::ManageStateInitial -- %s: Wallet balance is < 10000 SHROUD\n", GetStateString());
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    // If collateral is found switch to LOCAL mode
    if (pwalletMain->GetShroudnodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        eType = SHROUDNODE_LOCAL;
    }

    LogPrint("shroudnode", "CActiveShroudnode::ManageStateInitial -- End status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);
}

void CActiveShroudnode::ManageStateRemote() {
    LogPrint("shroudnode",
             "CActiveShroudnode::ManageStateRemote -- Start status = %s, type = %s, pinger enabled = %d, pubKeyShroudnode.GetID() = %s\n",
             GetStatus(), fPingerEnabled, GetTypeString(), pubKeyShroudnode.GetID().ToString());

    mnodeman.CheckShroudnode(pubKeyShroudnode);
    shroudnode_info_t infoMn = mnodeman.GetShroudnodeInfo(pubKeyShroudnode);

    if (infoMn.fInfoValid) {
        if (infoMn.nProtocolVersion < MIN_SHROUDNODE_PAYMENT_PROTO_VERSION_1 || infoMn.nProtocolVersion > MIN_SHROUDNODE_PAYMENT_PROTO_VERSION_2) {
            ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
            strNotCapableReason = "Invalid protocol version";
            LogPrintf("CActiveShroudnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (service != infoMn.addr) {
            ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
            // LogPrintf("service: %s\n", service.ToString());
            // LogPrintf("infoMn.addr: %s\n", infoMn.addr.ToString());
            strNotCapableReason = "Broadcasted IP doesn't match our external address. Make sure you issued a new broadcast if IP of this shroudnode changed recently.";
            LogPrintf("CActiveShroudnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (!CShroudnode::IsValidStateForAutoStart(infoMn.nActiveState)) {
            ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
            strNotCapableReason = strprintf("Shroudnode in %s state", CShroudnode::StateToString(infoMn.nActiveState));
            LogPrintf("CActiveShroudnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (nState != ACTIVE_SHROUDNODE_STARTED) {
            LogPrintf("CActiveShroudnode::ManageStateRemote -- STARTED!\n");
            vin = infoMn.vin;
            service = infoMn.addr;
            fPingerEnabled = true;
            ChangeState(ACTIVE_SHROUDNODE_STARTED);
        }
    } else {
        ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
        strNotCapableReason = "Shroudnode not in shroudnode list";
        LogPrintf("CActiveShroudnode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
    }
}

void CActiveShroudnode::ManageStateLocal() {
    LogPrint("shroudnode", "CActiveShroudnode::ManageStateLocal -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);
    if (nState == ACTIVE_SHROUDNODE_STARTED) {
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    if (pwalletMain->GetShroudnodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        int nInputAge = GetInputAge(vin);
        if (nInputAge < Params().GetConsensus().nShroudnodeMinimumConfirmations) {
            ChangeState(ACTIVE_SHROUDNODE_INPUT_TOO_NEW);
            strNotCapableReason = strprintf(_("%s - %d confirmations"), GetStatus(), nInputAge);
            LogPrintf("CActiveShroudnode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        {
            LOCK(pwalletMain->cs_wallet);
            pwalletMain->LockCoin(vin.prevout);
        }

        CShroudnodeBroadcast mnb;
        std::string strError;
        if (!CShroudnodeBroadcast::Create(vin, service, keyCollateral, pubKeyCollateral, keyShroudnode,
                                     pubKeyShroudnode, strError, mnb)) {
            ChangeState(ACTIVE_SHROUDNODE_NOT_CAPABLE);
            strNotCapableReason = "Error creating mastenode broadcast: " + strError;
            LogPrintf("CActiveShroudnode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        fPingerEnabled = true;
        ChangeState(ACTIVE_SHROUDNODE_STARTED);

        //update to shroudnode list
        LogPrintf("CActiveShroudnode::ManageStateLocal -- Update Shroudnode List\n");
        mnodeman.UpdateShroudnodeList(mnb);
        mnodeman.NotifyShroudnodeUpdates();

        //send to all peers
        LogPrintf("CActiveShroudnode::ManageStateLocal -- Relay broadcast, vin=%s\n", vin.ToString());
        mnb.RelayShroudNode();
    }
}
