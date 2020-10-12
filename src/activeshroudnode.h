// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The ShroudX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACTIVESHROUDNODE_H
#define ACTIVESHROUDNODE_H

#include "net.h"
#include "key.h"
#include "wallet/wallet.h"

class CActiveShroudnode;

static const int ACTIVE_SHROUDNODE_INITIAL          = 0; // initial state
static const int ACTIVE_SHROUDNODE_SYNC_IN_PROCESS  = 1;
static const int ACTIVE_SHROUDNODE_INPUT_TOO_NEW    = 2;
static const int ACTIVE_SHROUDNODE_NOT_CAPABLE      = 3;
static const int ACTIVE_SHROUDNODE_STARTED          = 4;

extern CActiveShroudnode activeShroudnode;

// Responsible for activating the Shroudnode and pinging the network
class CActiveShroudnode
{
public:
    enum shroudnode_type_enum_t {
        SHROUDNODE_UNKNOWN = 0,
        SHROUDNODE_REMOTE  = 1,
        SHROUDNODE_LOCAL   = 2
    };

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    shroudnode_type_enum_t eType;

    bool fPingerEnabled;

    /// Ping Shroudnode
    bool SendShroudnodePing();

public:
    // Keys for the active Shroudnode
    CPubKey pubKeyShroudnode;
    CKey keyShroudnode;

    // Initialized while registering Shroudnode
    CTxIn vin;
    CService service;

    int nState; // should be one of ACTIVE_SHROUDNODE_XXXX
    std::string strNotCapableReason;

    CActiveShroudnode()
        : eType(SHROUDNODE_UNKNOWN),
          fPingerEnabled(false),
          pubKeyShroudnode(),
          keyShroudnode(),
          vin(),
          service(),
          nState(ACTIVE_SHROUDNODE_INITIAL)
    {}

    /// Manage state of active Shroudnode
    void ManageState();

    // Change state if different and publish update
    void ChangeState(int newState);

    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string GetTypeString() const;

private:
    void ManageStateInitial();
    void ManageStateRemote();
    void ManageStateLocal();
};

#endif
