ShroudXnode Build Instructions and Notes
=============================
 - Version 1.0.2
 - Date: October 31, 2020
 - More detailed guide available soon on: https://shroudx.eu/

Prerequisites
-------------
 - Ubuntu 18.04+
 - Libraries to build from ShroudX source
 - Port **42998** is open

Step 1. Build
----------------------
**1.1.**  Check out from source:

    git clone https://github.com/ShroudProtocol/ShroudX

**1.2.**  See [README.md](README.md) for instructions on building.

Step 2. (Optional - only if firewall is running). Open port 42998
----------------------
**2.1.**  Run:

    sudo ufw allow 42998/tcp
    sudo ufw default allow outgoing
    sudo ufw enable

Step 3. First run on your Local Wallet
----------------------
**3.0.**  Go to the checked out folder

    cd ShroudX

**3.1.**  Start daemon in testnet mode:

    ./src/shroudd -daemon -server -testnet

**3.2.**  Generate shroudnodeprivkey:

    ./src/shroud-cli shroudnode genkey

(Store this key)

**3.3.**  Get wallet address:

    ./src/shroud-cli getaccountaddress 0

**3.4.**  Send to received address **exactly 50000 SHROUD** in **1 transaction**. Wait for 6 confirmations.

**3.5.**  Stop daemon:

    ./src/shroud-cli stop

Step 4. In your VPS where you are hosting your ShroudXnode. Update config files
----------------------
**4.1.**  Create file **shroud.conf** (in folder **~/.shroud**)

    rpcuser=username
    rpcpassword=password
    rpcallowip=127.0.0.1
    debug=1
    txindex=1
    daemon=1
    server=1
    listen=1
    maxconnections=24
    shroudnode=1
    shroudnodeprivkey=XXXXXXXXXXXXXXXXX  ## Replace with your shroudnode private key
    externalip=XXX.XXX.XXX.XXX:42998 ## Replace with your node external IP

**4.2.**  Create file **shroudnode.conf** (in 2 folders **~/.shroud** and **~/.shroud/testnet3**) contains the following info:
 - LABEL: A one word name you make up to call your node (ex. SN1)
 - IP:PORT: Your shroudnode VPS's IP, and the port is always 42998.
 - SHROUDNODEPRIVKEY: This is the result of your "shroudnode genkey" from earlier.
 - TRANSACTION HASH: The collateral tx. hash from the 50000 SHROUD deposit.
 - INDEX: The Index is always 0 or 1.

To get TRANSACTION HASH, run:

    ./src/shroud-cli shroudnode outputs

The output will look like:

    { "d6fd38868bb8f9958e34d5155437d009b72dfd33fc28874c87fd42e51c0f74fdb" : "0", }

Sample of shroudnode.conf:

    SN1 51.52.53.54:42998 XrxSr3fXpX3dZcU7CoiFuFWqeHYw83r28btCFfIHqf6zkMp1PZ4 d6fd38868bb8f9958e34d5155437d009b72dfd33fc28874c87fd42e51c0f74fdb 0

Step 5. Run a ShroudXnode
----------------------
**5.1.**  Start ShroudXnode:

    ./src/shroud-cli shroudnode start-alias <LABEL>

For example:

    ./src/shroud-cli shroudnode start-alias SN1

**5.2.**  To check node status:

    ./src/shroud-cli shroudnode debug

If not successfully started, just repeat start command
