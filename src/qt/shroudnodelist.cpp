#include "shroudnodelist.h"
#include "ui_shroudnodelist.h"

#include "activeshroudnode.h"
#include "clientmodel.h"
#include "init.h"
#include "guiutil.h"
#include "shroudnode-sync.h"
#include "shroudnodeconfig.h"
#include "shroudnodeman.h"
#include "sync.h"
#include "wallet/wallet.h"
#include "walletmodel.h"
#include "hybridui/styleSheet.h"
#include <QTimer>
#include <QMessageBox>

int GetOffsetFromUtc()
{
#if QT_VERSION < 0x050200
    const QDateTime dateTime1 = QDateTime::currentDateTime();
    const QDateTime dateTime2 = QDateTime(dateTime1.date(), dateTime1.time(), Qt::UTC);
    return dateTime1.secsTo(dateTime2);
#else
    return QDateTime::currentDateTime().offsetFromUtc();
#endif
}

ShroudnodeList::ShroudnodeList(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ShroudnodeList),
    clientModel(0),
    walletModel(0)
{
    ui->setupUi(this);

    ui->startButton->setEnabled(false);

    int columnAliasWidth = 100;
    int columnAddressWidth = 200;
    int columnProtocolWidth = 60;
    int columnStatusWidth = 80;
    int columnActiveWidth = 130;
    int columnLastSeenWidth = 130;

    ui->tableWidgetMyShroudnodes->setColumnWidth(0, columnAliasWidth);
    ui->tableWidgetMyShroudnodes->setColumnWidth(1, columnAddressWidth);
    ui->tableWidgetMyShroudnodes->setColumnWidth(2, columnProtocolWidth);
    ui->tableWidgetMyShroudnodes->setColumnWidth(3, columnStatusWidth);
    ui->tableWidgetMyShroudnodes->setColumnWidth(4, columnActiveWidth);
    ui->tableWidgetMyShroudnodes->setColumnWidth(5, columnLastSeenWidth);

    ui->tableWidgetShroudnodes->setColumnWidth(0, columnAddressWidth);
    ui->tableWidgetShroudnodes->setColumnWidth(1, columnProtocolWidth);
    ui->tableWidgetShroudnodes->setColumnWidth(2, columnStatusWidth);
    ui->tableWidgetShroudnodes->setColumnWidth(3, columnActiveWidth);
    ui->tableWidgetShroudnodes->setColumnWidth(4, columnLastSeenWidth);

    ui->tableWidgetMyShroudnodes->setContextMenuPolicy(Qt::CustomContextMenu);
    SetObjectStyleSheet(ui->tableWidgetMyShroudnodes, StyleSheetNames::TableViewLight);

    QAction *startAliasAction = new QAction(tr("Start alias"), this);
    contextMenu = new QMenu();
    contextMenu->addAction(startAliasAction);
    connect(ui->tableWidgetMyShroudnodes, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
    connect(startAliasAction, SIGNAL(triggered()), this, SLOT(on_startButton_clicked()));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
    connect(timer, SIGNAL(timeout()), this, SLOT(updateMyNodeList()));
    timer->start(1000);

    fFilterUpdated = false;
    nTimeFilterUpdated = GetTime();
    updateNodeList();
}

ShroudnodeList::~ShroudnodeList()
{
    delete ui;
}

void ShroudnodeList::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model) {
        // try to update list when shroudnode count changes
        // connect(clientModel, SIGNAL(strShroudnodesChanged(QString)), this, SLOT(updateNodeList()));
    }
}

void ShroudnodeList::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
}

void ShroudnodeList::showContextMenu(const QPoint &point)
{
    QTableWidgetItem *item = ui->tableWidgetMyShroudnodes->itemAt(point);
    if(item) contextMenu->exec(QCursor::pos());
}

void ShroudnodeList::StartAlias(std::string strAlias)
{
    std::string strStatusHtml;
    strStatusHtml += "<center>Alias: " + strAlias;

    BOOST_FOREACH(CShroudnodeConfig::CShroudnodeEntry mne, shroudnodeConfig.getEntries()) {
        if(mne.getAlias() == strAlias) {
            std::string strError;
            CShroudnodeBroadcast mnb;

            bool fSuccess = CShroudnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

            if(fSuccess) {
                strStatusHtml += "<br>Successfully started shroudnode.";
                mnodeman.UpdateShroudnodeList(mnb);
                mnb.RelayShroudNode();
                mnodeman.NotifyShroudnodeUpdates();
            } else {
                strStatusHtml += "<br>Failed to start shroudnode.<br>Error: " + strError;
            }
            break;
        }
    }
    strStatusHtml += "</center>";

    QMessageBox msg;
    msg.setText(QString::fromStdString(strStatusHtml));
    msg.exec();

    updateMyNodeList(true);
}

void ShroudnodeList::StartAll(std::string strCommand)
{
    int nCountSuccessful = 0;
    int nCountFailed = 0;
    std::string strFailedHtml;

    BOOST_FOREACH(CShroudnodeConfig::CShroudnodeEntry mne, shroudnodeConfig.getEntries()) {
        std::string strError;
        CShroudnodeBroadcast mnb;

        int32_t nOutputIndex = 0;
        if(!ParseInt32(mne.getOutputIndex(), &nOutputIndex)) {
            continue;
        }

        COutPoint outpoint = COutPoint(uint256S(mne.getTxHash()), nOutputIndex);

        if(strCommand == "start-missing" && mnodeman.Has(CTxIn(outpoint))) continue;

        bool fSuccess = CShroudnodeBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), strError, mnb);

        if(fSuccess) {
            nCountSuccessful++;
            mnodeman.UpdateShroudnodeList(mnb);
            mnb.RelayShroudNode();
            mnodeman.NotifyShroudnodeUpdates();
        } else {
            nCountFailed++;
            strFailedHtml += "\nFailed to start " + mne.getAlias() + ". Error: " + strError;
        }
    }
    pwalletMain->Lock();

    std::string returnObj;
    returnObj = strprintf("Successfully started %d shroudnodes, failed to start %d, total %d", nCountSuccessful, nCountFailed, nCountFailed + nCountSuccessful);
    if (nCountFailed > 0) {
        returnObj += strFailedHtml;
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(returnObj));
    msg.exec();

    updateMyNodeList(true);
}

void ShroudnodeList::updateMyShroudnodeInfo(QString strAlias, QString strAddr, const COutPoint& outpoint)
{
    bool fOldRowFound = false;
    int nNewRow = 0;

    for(int i = 0; i < ui->tableWidgetMyShroudnodes->rowCount(); i++) {
        if(ui->tableWidgetMyShroudnodes->item(i, 0)->text() == strAlias) {
            fOldRowFound = true;
            nNewRow = i;
            break;
        }
    }

    if(nNewRow == 0 && !fOldRowFound) {
        nNewRow = ui->tableWidgetMyShroudnodes->rowCount();
        ui->tableWidgetMyShroudnodes->insertRow(nNewRow);
    }

    shroudnode_info_t infoMn = mnodeman.GetShroudnodeInfo(CTxIn(outpoint));
    bool fFound = infoMn.fInfoValid;

    QTableWidgetItem *aliasItem = new QTableWidgetItem(strAlias);
    QTableWidgetItem *addrItem = new QTableWidgetItem(fFound ? QString::fromStdString(infoMn.addr.ToString()) : strAddr);
    QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(fFound ? infoMn.nProtocolVersion : -1));
    QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(fFound ? CShroudnode::StateToString(infoMn.nActiveState) : "MISSING"));
    QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(fFound ? (infoMn.nTimeLastPing - infoMn.sigTime) : 0)));
    QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M",
                                                                                                   fFound ? infoMn.nTimeLastPing + GetOffsetFromUtc() : 0)));
    QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(fFound ? CBitcoinAddress(infoMn.pubKeyCollateralAddress.GetID()).ToString() : ""));

    ui->tableWidgetMyShroudnodes->setItem(nNewRow, 0, aliasItem);
    ui->tableWidgetMyShroudnodes->setItem(nNewRow, 1, addrItem);
    ui->tableWidgetMyShroudnodes->setItem(nNewRow, 2, protocolItem);
    ui->tableWidgetMyShroudnodes->setItem(nNewRow, 3, statusItem);
    ui->tableWidgetMyShroudnodes->setItem(nNewRow, 4, activeSecondsItem);
    ui->tableWidgetMyShroudnodes->setItem(nNewRow, 5, lastSeenItem);
    ui->tableWidgetMyShroudnodes->setItem(nNewRow, 6, pubkeyItem);
}

void ShroudnodeList::updateMyNodeList(bool fForce)
{
    TRY_LOCK(cs_mymnlist, fLockAcquired);
    if(!fLockAcquired) {
        return;
    }
    static int64_t nTimeMyListUpdated = 0;

    // automatically update my shroudnode list only once in MY_MASTERNODELIST_UPDATE_SECONDS seconds,
    // this update still can be triggered manually at any time via button click
    int64_t nSecondsTillUpdate = nTimeMyListUpdated + MY_MASTERNODELIST_UPDATE_SECONDS - GetTime();
    ui->secondsLabel->setText(QString::number(nSecondsTillUpdate));

    if(nSecondsTillUpdate > 0 && !fForce) return;
    nTimeMyListUpdated = GetTime();

    ui->tableWidgetShroudnodes->setSortingEnabled(false);
    BOOST_FOREACH(CShroudnodeConfig::CShroudnodeEntry mne, shroudnodeConfig.getEntries()) {
        int32_t nOutputIndex = 0;
        if(!ParseInt32(mne.getOutputIndex(), &nOutputIndex)) {
            continue;
        }

        updateMyShroudnodeInfo(QString::fromStdString(mne.getAlias()), QString::fromStdString(mne.getIp()), COutPoint(uint256S(mne.getTxHash()), nOutputIndex));
    }
    ui->tableWidgetShroudnodes->setSortingEnabled(true);

    // reset "timer"
    ui->secondsLabel->setText("0");
}

void ShroudnodeList::updateNodeList()
{
    TRY_LOCK(cs_mnlist, fLockAcquired);
    if(!fLockAcquired) {
        return;
    }

    static int64_t nTimeListUpdated = GetTime();

    // to prevent high cpu usage update only once in MASTERNODELIST_UPDATE_SECONDS seconds
    // or MASTERNODELIST_FILTER_COOLDOWN_SECONDS seconds after filter was last changed
    int64_t nSecondsToWait = fFilterUpdated
                            ? nTimeFilterUpdated - GetTime() + MASTERNODELIST_FILTER_COOLDOWN_SECONDS
                            : nTimeListUpdated - GetTime() + MASTERNODELIST_UPDATE_SECONDS;

    if(fFilterUpdated) ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", nSecondsToWait)));
    if(nSecondsToWait > 0) return;

    nTimeListUpdated = GetTime();
    fFilterUpdated = false;

    QString strToFilter;
    ui->countLabel->setText("Updating...");
    ui->tableWidgetShroudnodes->setSortingEnabled(false);
    ui->tableWidgetShroudnodes->clearContents();
    ui->tableWidgetShroudnodes->setRowCount(0);
//    std::map<COutPoint, CShroudnode> mapShroudnodes = mnodeman.GetFullShroudnodeMap();
    std::vector<CShroudnode> vShroudnodes = mnodeman.GetFullShroudnodeVector();
    int offsetFromUtc = GetOffsetFromUtc();

    BOOST_FOREACH(CShroudnode & mn, vShroudnodes)
    {
//        CShroudnode mn = mnpair.second;
        // populate list
        // Address, Protocol, Status, Active Seconds, Last Seen, Pub Key
        QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(mn.addr.ToString()));
        QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(mn.nProtocolVersion));
        QTableWidgetItem *statusItem = new QTableWidgetItem(QString::fromStdString(mn.GetStatus()));
        QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(QString::fromStdString(DurationToDHMS(mn.lastPing.sigTime - mn.sigTime)));
        QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M", mn.lastPing.sigTime + offsetFromUtc)));
        QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString()));

        if (strCurrentFilter != "")
        {
            strToFilter =   addressItem->text() + " " +
                            protocolItem->text() + " " +
                            statusItem->text() + " " +
                            activeSecondsItem->text() + " " +
                            lastSeenItem->text() + " " +
                            pubkeyItem->text();
            if (!strToFilter.contains(strCurrentFilter)) continue;
        }

        ui->tableWidgetShroudnodes->insertRow(0);
        ui->tableWidgetShroudnodes->setItem(0, 0, addressItem);
        ui->tableWidgetShroudnodes->setItem(0, 1, protocolItem);
        ui->tableWidgetShroudnodes->setItem(0, 2, statusItem);
        ui->tableWidgetShroudnodes->setItem(0, 3, activeSecondsItem);
        ui->tableWidgetShroudnodes->setItem(0, 4, lastSeenItem);
        ui->tableWidgetShroudnodes->setItem(0, 5, pubkeyItem);
    }

    ui->countLabel->setText(QString::number(ui->tableWidgetShroudnodes->rowCount()));
    ui->tableWidgetShroudnodes->setSortingEnabled(true);
}

void ShroudnodeList::on_filterLineEdit_textChanged(const QString &strFilterIn)
{
    strCurrentFilter = strFilterIn;
    nTimeFilterUpdated = GetTime();
    fFilterUpdated = true;
    ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", MASTERNODELIST_FILTER_COOLDOWN_SECONDS)));
}

void ShroudnodeList::on_startButton_clicked()
{
    std::string strAlias;
    {
        LOCK(cs_mymnlist);
        // Find selected node alias
        QItemSelectionModel* selectionModel = ui->tableWidgetMyShroudnodes->selectionModel();
        QModelIndexList selected = selectionModel->selectedRows();

        if(selected.count() == 0) return;

        QModelIndex index = selected.at(0);
        int nSelectedRow = index.row();
        strAlias = ui->tableWidgetMyShroudnodes->item(nSelectedRow, 0)->text().toStdString();
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm shroudnode start"),
        tr("Are you sure you want to start shroudnode %1?").arg(QString::fromStdString(strAlias)),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAlias(strAlias);
        return;
    }

    StartAlias(strAlias);
}

void ShroudnodeList::on_startAllButton_clicked()
{
    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm all shroudnodes start"),
        tr("Are you sure you want to start ALL shroudnodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll();
        return;
    }

    StartAll();
}

void ShroudnodeList::on_startMissingButton_clicked()
{

    if(!shroudnodeSync.IsShroudnodeListSynced()) {
        QMessageBox::critical(this, tr("Command is not available right now"),
            tr("You can't use this command until shroudnode list is synced"));
        return;
    }

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this,
        tr("Confirm missing shroudnodes start"),
        tr("Are you sure you want to start MISSING shroudnodes?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if(retval != QMessageBox::Yes) return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    if(encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForMixingOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());

        if(!ctx.isValid()) return; // Unlock wallet was cancelled

        StartAll("start-missing");
        return;
    }

    StartAll("start-missing");
}

void ShroudnodeList::on_tableWidgetMyShroudnodes_itemSelectionChanged()
{
    if(ui->tableWidgetMyShroudnodes->selectedItems().count() > 0) {
        ui->startButton->setEnabled(true);
    }
}

void ShroudnodeList::on_UpdateButton_clicked()
{
    updateMyNodeList(true);
}
