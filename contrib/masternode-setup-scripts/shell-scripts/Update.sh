#!/bin/sh
#!/bin/sh
# Copyright (c) 2020 The ShroudX Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


clear
echo "Starting ShroudXnode update script"
cd && cd /usr/local/bin
echo "Stopping shroudd..."
shroud-cli stop
cd && cd ShroudX/contrib/masternode-setup-scripts/shell-scripts
echo "Downloading ShroudX latest release update..."
wget -N https://github.com/ShroudProtocol/ShroudX/releases/download/v1.2.4/shroudx-1.2.4-x86_64-ubuntu-18.04.tar.gz
sudo tar -c /usr/local/bin -zxvf shroudx-1.2.4-x86_64-ubuntu-18.04.tar.gz
echo "Setting permissions..."
cd && sudo chmod +x /usr/local/bin/shroud*
sudo chmod +x /usr/local/bin/tor*
echo "Launching shroudd..."
cd && cd /usr/local/bin
shroudd -daemon
echo "Cleaning up..."
cd && cd ShroudX/contrib/masternode-setup-scripts/shell-scripts
rm -rf shroudx-1.2.4-x86_64-ubuntu-18.04.tar.gz
echo "Shroudnode Updated Successfully!"