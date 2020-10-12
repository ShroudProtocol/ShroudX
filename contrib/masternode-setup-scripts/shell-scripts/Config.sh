#!/bin/sh
# Copyright (c) 2020 The Shroud Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


clear
echo "Starting Shroudnode Auto Config script"
cd && cd /usr/local/bin
echo "Stopping shroudd..."
shroud-cli stop
cd ~/.shroud
rm -rf shroud.conf
echo "Editing shroud.conf..."
cat >> shroud.conf <<'EOF'
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
EOF
echo "Running shroudd..."
cd && cd /usr/local/bin
shroudd -daemon
echo "Shroudnode Configuration Completed!"