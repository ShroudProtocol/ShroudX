# ShroudX
ShroudX (SHROUD) is a new state of the art privacy coin that uses Sigma and Dandelion++ Protocol along with TOR.



### Key Features
    -Privacy (Anonymous and Untraceable)
    -Sigma Protocol 
    -Tor Protocol
    -Dandelion++ Protocol
    -Proof of Work (x16r-V2)
    -Proof of Stake (PoS 3.0)
    -Masternode

### Specifications
| Specification | Value |
|:-----------|:-----------|
| Name | `ShroudX` |
| Ticker | `SHROUD` |
| Consensus Algorithm | `PoW & PoS` |
| Mining Algorithm | `x16rv2` |
| Block Time | `60s (1 min)` |
| Type | `Hybrid PoW/PoS/MN` |
| Max Supply | `50,000,000 SHROUD` |
| Premine | `5,000,000 SHROUD` |
| Premine Burned | `4,724,447 SHROUD` |
| Coinbase Maturity | `100 Blocks` |
| Stake Maturity | `100 MIN` |
| Masternode Maturity | `100 MIN` |
| Masternode Confirmations | `15` |
| Masternode Collateral until block 168100| `10,000 SHROUD` |
| Masternode Collateral after block 168100 | `50,000 SHROUD` |
| Default Port | `42998` |
| RPC Port | `42999` |

#### Block Rewards
| Phase | Reward (SHROUD) | PoW | PoS | MN | Blocks |
|:-----------|:-----------|:-----------|:-----------|:-----------|:-----------|
| GENESIS | `0` | | | | `1` |
| PREMINE | `5,000,000` | | | | `2` |
| INITIAL | `0.0001` | `100 %` | `0%` | `0%` | `3 - 1500` |
| POSPREPARATION | `1` | `100 %` | `0%` | `0%` | `1501 - 10080` |
| YEAR1 | `5` | `15 %` | `15%` | `70%` | `10081 - 525600` |
| PREMINE BURNED | `4,724,447` | | | | `168100` |
| YEAR2 | `3` | `15 %` | `15%` | `70%` | `525601 - 1051200` |
| YEAR3 | `2` | `15 %` | `15%` | `70%` | `1051201 - 1576800` |
| YEAR4 | `1.75` | `15 %` | `15%` | `70%` | `1576801 - 2102400` |
| YEAR5 | `1.5` | `15 %` | `15%` | `70%` | `2102401 - 2628000` |
| YEAR6 | `1.25` | `15 %` | `15%` | `70%` | `2628001 - 3153600` |
| YEAR7 | `1` | `15 %` | `15%` | `70%` | `3153601 - 3679200` |
| YEAR8 | `0.75` | `15 %` | `15%` | `70%` | `3679201 - 4204800` |
| YEAR9 | `0.5` | `15 %` | `15%` | `70%` | `4204801 - 4730400` |
| YEAR10 | `0.25` | `15 %` | `15%` | `70%` | `4730401 - 5256000` |
| YEAR10++ | `0.1` | `15 %` | `15%` | `70%` | `5256001 - infinite` |

Windows Build Instructions and Notes
==================================
The Windows wallet is build with QTs QMAKE. See [build-windows.md](https://github.com/ShroudProtocol/ShroudX/blob/master/doc/build-windows.md) for instructions.

MACOS Build Instructions and Notes
==================================
The macOS wallet itself is build with QTs QMAKE. See [build-macos.md](https://github.com/ShroudProtocol/ShroudX/blob/master/doc/build-macos.md) for instructions.

Linux Build Instructions and Notes
==================================

Dependencies
----------------------
You can use the ``depscript.sh`` to automatically install Dependencies to build ShroudX or manually install them using the syntax below

1.  Update packages

        ``sudo apt-get update``

2.  Install required packages
        
        ``sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils libboost-all-dev libzmq3-dev libminizip-dev``

3.  Install Berkeley DB 4.8

        ``sudo add-apt-repository ppa:bitcoin/bitcoin && sudo apt-get update && sudo apt-get install libdb4.8-dev libdb4.8++-dev``
4.  Install QT 5

        ``
        sudo apt-get install libminiupnpc-dev && sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev
        ``
        

Build
----------------------
1.  Clone the source:

        git clone https://github.com/ShroudProtocol/ShroudX

2.  Build ShroudX core:

    Configure and build the headless ShroudX binaries as well as the GUI (if Qt is found).

    You can disable the GUI build by passing `--without-gui` to configure.

        ```
        ./autogen.sh
        ./configure
        make
        ```

3.  It is recommended to build and run the unit tests:

        ``make check``


Setting up a ShroudXnode
==================================

Read [contrib/masternode-setup-scripts/README.md](contrib/masternode-setup-scripts/README.md) for instructions.
