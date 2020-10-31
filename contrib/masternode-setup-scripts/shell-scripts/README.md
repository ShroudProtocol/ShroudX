# ShroudX Protocol Masternode Setup Scripts
 Various scripts for setting up shroudnode.


## SHELL SCRIPTS
=============================================================
### BEFORE USING THESE SCRIPTS (PRE-SETUP)
Open your notepad or any text editor application on your pc and write this down as your ``Cheat Sheet``
```
1. SHROUDNODE NAME = SN1
2. COLLATERAL = 50000
3. SHROUD ADDRESS = SZRnGyyPv1FVGgMGn7JXbuHCGbsgiBRprq
4. SHROUDNODE GENKEY = 84qRmqujiRqJ1vepSacScUz1EuBTYoaPM3cD5n1211THemaRWms
5. SHROUDNODE OUTPUTS = 4873d0c50c6ddc623bedcf0684dafc107809f9434b8426b728634f7c8c455615 1
6. UNIQUE IP OF THE VPS = 201.47.23.109:42998
```

### GETTING A VPS (STEP 1)
Set up your VPS, we recommend [VULTR](https://www.vultr.com/), and select ``DEPLOY INSTANCE`` then select the following
- Cloud compute
- Location -any
- Server type: Ubuntu 18.04
- Server size: 1GB $5/month
- Add your desired hostname and label
- Click DEPLOY
Note: The server will take a few minutes to deploy and will then shows as "running" in your "instances" section.

### QT WALLET CONFIGURATION (STEP 2)
1. Open your ``QT WALLET`` .
2. Open your ``debug console`` then type the following comamnd
	```
	shroudnode genkey
	```
	- Copy the generated key from your ``debug console`` and open your ``Cheat Sheet`` then paste the generated key on ``4. SHROUDNODE GENKEY`` (84qRmqujiRqJ1vepSacScUz1EuBTYoaPM3cD5n1211THemaRWms)

	- Copy your stealth address under the ``Receive`` tab (for example: SZRnGyyPv1FVGgMGn7JXbuHCGbsgiBRprq ) and paste it on your ``Cheat Sheet`` on ``3. SHROUD ADDRESS``

	- Copy the stealth address again from your ``Cheat Sheet`` under ``3. SHROUD ADDRESS`` and head over to your ``Send`` tab then paste the ``SHROUD ADDRESS`` on the ``Enter a Shroud address`` area on the ``Send`` tab and input the ``2. SHROUDNODE COLLATERAL`` which is ``50000`` SHROUD on the ``Amount`` area then click ``Send`` and wait for ``6`` confirmations.

	- Then click go to your Transactions tab and right click the transaction when you send the ``SHROUDNODE COLLATERAL`` then click ``Copy transaction ID`` when a windows pops-up and head over to your ``debug console`` then type ``shroudnode outputs`` then press ``enter`` copy the ``txhash`` and paste it on your ``Cheat Sheet`` under ``5. SHROUDNODE OUTPUTS`` ( 4873d0c50c6ddc623bedcf0684dafc107809f9434b8426b728634f7c8c455615 ) and also don't forget to copy the ``outputidx`` ( 1 ) and paste it next to the ``txhash``.

	- Then head over to your ``Cheat Sheet`` 
		- input your ``VPS`` ip address ( 201.47.23.109 ) under ``6. UNIQUE IP OF THE VPS``
		- input your ``ShroudXnode Name``  ( SN1 )under ``1. SHROUDNODE NAME``

3. Head over to your ``shroud`` directory
	- Windows: %APPDATA%/shroud
	- Linux: ~/.shroud
	- Mac: ~/Library/Application Support/shroud
4. Open and edit ``shroudnode.conf`` file with your preferred Text Editor
	- Inside ``shroudnode.conf`` file is this lines of text
	```
	# ShroudXnode config file
	# Format: alias IP:port shroudnode_privatekey collateral_output_txid collateral_output_index
	# Example: sn1 127.0.0.1:42998 7Cqyr4U7GU7qVo5TE1nrfA8XPVqh7GXBuEBPYzaWxEhiRRDLZ5c 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 1
	```
6. UNIQUE IP OF THE VPS = 201.47.23.109:42998
	- On line 4 add your ``1. SHROUDNODE NAME`` which is ``SN1``, next is add your ``6. UNIQUE IP OF THE VPS`` which is ``56.56.65.20`` and add the respective default port of ``shroud`` which is ``42998``,next is add your  ``4. SHROUDNODE GENKEY`` which is ``84qRmqujiRqJ1vepSacScUz1EuBTYoaPM3cD5n1211THemaRWms``, and lastly add your ``5. SHROUDNODE OUTPUTS``which is ``4873d0c50c6ddc623bedcf0684dafc107809f9434b8426b728634f7c8c455615`` then add your ``outputidx`` which is ``1`` next to your ``5. SHROUDNODE OUTPUTS``.

	- It will look like this on the ``shroudnode.conf`` file

	```
	# ShroudXnode config file
	# Format: alias IP:port shroudnode_privatekey collateral_output_txid collateral_output_index
	# Example: sn1 127.0.0.1:42998 7Cqyr4U7GU7qVo5TE1nrfA8XPVqh7GXBuEBPYzaWxEhiRRDLZ5c 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 1
	SN1 201.47.23.109:42998 84qRmqujiRqJ1vepSacScUz1EuBTYoaPM3cD5n1211THemaRWms 4873d0c50c6ddc623bedcf0684dafc107809f9434b8426b728634f7c8c455615 1
	```

### ACCESSING YOUR VPS (STEP 3)
1. Download a SSH Application Client (you can choose one below)
	- PuTTY (https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html)
	- Bitvise (https://www.bitvise.com/ssh-client-download) [RECOMMENDED]
2. Open Bitvise
	1. Enter your VPS ``ip address`` under ``Host`` on the Server area
	2. Enter the port number which is ``22`` under ``Port``
	3. Enter your VPS ``username`` under ``Username`` on the Authentication area
	4. Check the ``Store encrypted password in profile`` checkbox and enter your VPS ``password`` under ``Password``
	5. Then click ``Log in``

### DOWNLOADING THE SCRIPT ON YOUR VPS (STEP 4)
On your SSH Terminal type this lines below one at a time
```
git clone https://github.com/ShroudProtocol/ShroudX
chmod -R 0755 ShroudX
cd ShroudX/contrib/masternode-setup-scripts/shell-scripts
./Install.sh
```
Note: The script allows you to automatically install ``ShroudX`` from the ``ShroudX`` repository.

#### VPS WALLET CONFIGURATION (STEP 5)

```
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
```
1. Change these following lines on the bash file named ``Config.sh`` by following steps below via ``vim``

```
vi Config.sh
```
then 

- change the ``username`` value to your own
- change the ``password`` value to your own
- change the ``XXX.XXX.XXX.XXX:42998`` value to your own which is located on your ``Cheat Sheet`` (e.g. 201.47.23.109:42998)
- change the ``yourmasternodeprivkeyhere`` value to your own which is also located on your ``Cheat Sheet`` (e.g. 84qRmqujiRqJ1vepSacScUz1EuBTYoaPM3cD5n1211THemaRWms this is your ``SHROUDNODE GENKEY``)
	(To save and exit the editor press ``Ctrl + C`` then type ``:wq!`` then press Enter)

2. Then open ``Config.sh`` file by typing ``./Config.sh``. 

Note: It will automatically change your ``shroud.conf`` file located on the ``shroud`` directory inputting all the text above.

#### STARTING AND CHECKING YOUR SHROUDNODE (STEP 6)

1. OPEN YOUR QT WALLET ON YOUR LOCAL MACHINE

2. HEAD OVER TO ``Shroudnodes`` tab on your wallet

3. YOU CAN CLICK ``Start all`` to start your shroudnode

Note: ShroudXnodes that are enabled will appear on your ``Shroudnodes`` tab

### HOW TO UPDATE YOUR SHROUD DAEMON WITH A SCRIPT
Run first the ``Sourceupdate.sh`` shell file. On your SSH Terminal type this line below
```
cd
cd ShroudX/contrib/masternode-setup-scripts/shell-scripts
./Sourceupdate.sh
```

When finish updating the source using ``Sourceupdate.sh`` then you can run the ``Update.sh`` shell file. On your SSH Terminal type this line below
```
cd
cd ShroudX/contrib/masternode-setup-scripts/shell-scripts
./Update.sh
```
Note: It will automatically update your daemon



if you have question regarding to the scripts feel free to head over to ``ShroudX Discord Channel`` (https://discord.gg/fj8KDYd)


# GREAT JOB! YOU CONFIGURED YOUR SHROUDNODE.
