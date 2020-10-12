#!/bin/sh
#!/bin/sh
# Copyright (c) 2020 The Shroud Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


clear
echo "Starting Shroudnode update script"
cd && cd /usr/local/bin
echo "Stopping shroudd..."
shroud-cli stop
cd && cd Shroud/contrib/masternode-setup-scripts/shell-scripts
echo "Downloading Shroud latest release update..."
wget -N https://github.com/ShroudXProject/Shroud/releases/download/v1.2.1/shroud-1.2.1-x86_64-linux-gnu.tar.gz
sudo tar -c /usr/local/bin -zxvf shroud-1.2.1-x86_64-linux-gnu.tar.gz
echo "Setting permissions..."
cd && sudo chmod +x /usr/local/bin/shroud*
sudo chmod +x /usr/local/bin/tor*
echo "Launching shroudd..."
cd && cd /usr/local/bin
shroudd -daemon
echo "Cleaning up..."
cd && cd Shroud/contrib/masternode-setup-scripts/shell-scripts
rm -rf shroud-1.2.1-x86_64-linux-gnu.tar.gz
echo "Shroudnode Updated Successfully!"