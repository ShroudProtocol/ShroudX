#!/bin/sh
#!/bin/sh
# Copyright (c) 2020 The ShroudX Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


clear
echo "Starting Source Code Updater script"
echo "Deleting old source code..."
cd && sudo rm -rf Shroud
cd && sudo rm -rf ShroudX
echo "Downloading latest source code..."
git clone https://github.com/ShroudProtocol/ShroudX
echo "Setting permissions..."
sudo chmod -R 0755 ShroudX
echo "Shroud Source Code Updated Successfully!"