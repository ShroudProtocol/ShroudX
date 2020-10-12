// Copyright (c) 2018 Tadhg Riordan Zcoin Developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activeshroudnode.h"
#include "validationinterface.h"
#include "shroudnodeman.h"
#include "univalue.h"
#include "shroudnode-sync.h"
#include "shroudnodeconfig.h"
#include "client-api/server.h"
#include "client-api/protocol.h"
#include <client-api/wallet.h>
#include <unordered_map>

using namespace std;

bool GetShroudnodePayeeAddress(const std::string& txHash, const std::string& n, CBitcoinAddress& address){

    const CWalletTx* wtx = pwalletMain->GetWalletTx(uint256S(txHash));
    if(wtx==NULL)
        return false;

    CTxDestination destination;
    const CTxOut &txout = wtx->vout[stoi(n)];
    if (!ExtractDestination(txout.scriptPubKey, destination))
        return false;

    address.Set(destination);

    return true;
}

UniValue shroudnodekey(Type type, const UniValue& data, const UniValue& auth, bool fHelp){

    switch(type){
        case Update: {
            UniValue key(UniValue::VOBJ);
            CKey secret;
            secret.MakeNewKey(false);

            key.push_back(Pair("key", CBitcoinSecret(secret).ToString()));

            return key;
            break;
        }
        default: {
            throw JSONAPIError(API_TYPE_NOT_IMPLEMENTED, "Error: type does not exist for method called, or no type passed where method requires it.");
        }
    }
    return true;
}

UniValue shroudnodecontrol(Type type, const UniValue& data, const UniValue& auth, bool fHelp){

    switch(type){
        case Update: {
            string method;
            try {
                method = find_value(data, "method").get_str();
            }catch (const std::exception& e){
                throw JSONAPIError(API_INVALID_PARAMETER, "Invalid, missing or duplicate parameter");
            }
            
            UniValue overall(UniValue::VOBJ);
            UniValue detail(UniValue::VOBJ);
            UniValue ret(UniValue::VOBJ);
            
            int nSuccessful = 0;
            int nFailed = 0;

            if (method == "start-alias") {

                string alias;
                try {
                    alias = find_value(data, "alias").get_str();
                }catch (const std::exception& e){
                    throw JSONAPIError(API_INVALID_PARAMETER, "Invalid, missing or duplicate parameter");
                }

                bool fFound = false;

                UniValue status(UniValue::VOBJ);
                status.push_back(Pair("alias", alias));

                BOOST_FOREACH(CShroudnodeConfig::CShroudnodeEntry mne, shroudnodeConfig.getEntries()) {
                    if (mne.getAlias() == alias) {
                        fFound = true;
                        std::string strError;
                        CShroudnodeBroadcast mnb;

                        bool fResult = CShroudnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(),
                                                                    mne.getOutputIndex(), strError, mnb);
                        status.push_back(Pair("success", fResult));
                        if (fResult) {
                            nSuccessful++;
                            mnodeman.UpdateShroudnodeList(mnb);
                            mnb.RelayShroudNode();
                        } else {
                            nFailed++;
                            status.push_back(Pair("info", strError));
                        }
                        mnodeman.NotifyShroudnodeUpdates();
                        break;
                    }
                }

                if (!fFound) {
                    nFailed++;
                    status.push_back(Pair("success", false));
                    status.push_back(Pair("info", "Could not find alias in config. Verify with list-conf."));
                }

                detail.push_back(Pair("status", status));
            }

            else if (method == "start-all" || method == "start-missing") {
                {
                    LOCK(pwalletMain->cs_wallet);
                    EnsureWalletIsUnlocked();
                }

                if ((method == "start-missing") && !shroudnodeSync.IsShroudnodeListSynced()) {
                    throw JSONAPIError(API_CLIENT_IN_INITIAL_DOWNLOAD,
                                       "You can't use this command until shroudnode list is synced");
                }

                BOOST_FOREACH(CShroudnodeConfig::CShroudnodeEntry mne, shroudnodeConfig.getEntries()) {
                    std::string strError;

                    CTxIn vin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
                    CShroudnode *pmn = mnodeman.Find(vin);
                    CShroudnodeBroadcast mnb;

                    if (method == "start-missing" && pmn) continue;

                    bool fResult = CShroudnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(),
                                                                mne.getOutputIndex(), strError, mnb);

                    UniValue status(UniValue::VOBJ);
                    status.push_back(Pair("alias", mne.getAlias()));
                    status.push_back(Pair("success", fResult));

                    if (fResult) {
                        nSuccessful++;
                        mnodeman.UpdateShroudnodeList(mnb);
                        mnb.RelayShroudNode();
                    } else {
                        nFailed++;
                        status.push_back(Pair("info", strError));
                    }

                    detail.push_back(Pair("status", status));
                }
                mnodeman.NotifyShroudnodeUpdates();

            }

            else if(method=="update-status"){
                
            }
            else {
                throw runtime_error("Method not found.");
            }

            overall.push_back(Pair("successful", nSuccessful));
            overall.push_back(Pair("failed", nFailed));
            overall.push_back(Pair("total", nSuccessful + nFailed));

            ret.push_back(Pair("overall", overall));
            ret.push_back(Pair("detail", detail));

            return ret;
            break;
        }
        default: {
            throw JSONAPIError(API_TYPE_NOT_IMPLEMENTED, "Error: type does not exist for method called, or no type passed where method requires it.");
        }
    }
    return true;
}

UniValue shroudnodelist(Type type, const UniValue& data, const UniValue& auth, bool fHelp){

    switch(type){
        case Initial: {
            UniValue data(UniValue::VOBJ);
            UniValue nodes(UniValue::VOBJ);

            int fIndex = 0;
            BOOST_FOREACH(CShroudnodeConfig::CShroudnodeEntry mne, shroudnodeConfig.getEntries()) {
                const std::string& txHash = mne.getTxHash();
                const std::string& outputIndex = mne.getOutputIndex();
                CBitcoinAddress address;
                std::string key = txHash + outputIndex;
                CShroudnode* mn = mnodeman.Find(txHash, outputIndex);

                UniValue node(UniValue::VOBJ);
                if(mn==NULL){
                    node = mne.ToJSON();
                    node.push_back(Pair("position", fIndex++));
                    if(GetShroudnodePayeeAddress(txHash, outputIndex, address))
                        node.push_back(Pair("payeeAddress", address.ToString()));
                }else{
                    node = mn->ToJSON();
                }
                nodes.replace(key, node);
            }

            /*
             * If the Shroudnode list is not yet synced, return the wallet Shroudnodes, as described in shroudnode.conf
             * if it is, process all Shroudnodes, and return along with wallet Shroudnodes.
             * (if the wallet Shroudnode has started, it will be replaced in the synced list).
             */
            if(!shroudnodeSync.IsSynced()){
                data.push_back(Pair("nodes", nodes));
                data.push_back(Pair("total", mnodeman.CountShroudnodes()));
                return data;
            }

            std::vector <CShroudnode> vShroudnodes = mnodeman.GetFullShroudnodeVector();
            BOOST_FOREACH(CShroudnode & mn, vShroudnodes) {
                std::string txHash = mn.vin.prevout.hash.ToString().substr(0,64);
                std::string outputIndex = to_string(mn.vin.prevout.n);
                std::string key = txHash + outputIndex;

                // only process wallet Shroudnodes - they are already in "nodes", so if we find it, replace with update
                if(!find_value(nodes, key).isNull())
                    nodes.replace(key, mn.ToJSON());
            }

            data.push_back(Pair("nodes", nodes));
            data.push_back(Pair("total", mnodeman.CountShroudnodes()));
            return data;
            break;
        }
        default: {
            throw JSONAPIError(API_TYPE_NOT_IMPLEMENTED, "Error: type does not exist for method called, or no type passed where method requires it.");
        }
    }

    return true;
}

UniValue shroudnodeupdate(Type type, const UniValue& data, const UniValue& auth, bool fHelp){
    UniValue ret(UniValue::VOBJ);
    UniValue outpoint(UniValue::VOBJ);
    string key;
    // We already have the return data in the "data" object, here we simply form the key.
    try {
        outpoint = find_value(data, "outpoint").get_obj();
        key = find_value(outpoint, "txid").get_str() +  find_value(outpoint, "index").get_str();
    }catch (const std::exception& e){
        throw JSONAPIError(API_INVALID_PARAMETER, "Invalid, missing or duplicate parameter");
    }
    ret.push_back(Pair(key, data));
    return ret;
}

static const CAPICommand commands[] =
{ //  category              collection         actor (function)          authPort   authPassphrase   warmupOk
  //  --------------------- ------------       ----------------          -------- --------------   --------
    { "shroudnode",              "shroudnodeControl",    &shroudnodecontrol,            true,      true,            false  },
    { "shroudnode",              "shroudnodeKey",        &shroudnodekey,                true,      false,           false  },
    { "shroudnode",              "shroudnodeList",       &shroudnodelist,               true,      false,           false  },
    { "shroudnode",              "shroudnodeUpdate",     &shroudnodeupdate,             true,      false,           false  }
};
void RegisterShroudnodeAPICommands(CAPITable &tableAPI)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableAPI.appendCommand(commands[vcidx].collection, &commands[vcidx]);
}
