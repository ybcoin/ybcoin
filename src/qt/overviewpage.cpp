#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "version.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(qVariantCanConvert<QColor>(value))
        {
            foreground = qvariant_cast<QColor>(value);
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

using namespace json_spirit;
OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0),    
    //advsReply(NULL),
    advsUrl("http://ybcoin.org/apps/list.json")
{
    ui->setupUi(this);
    advsTimer = new QTimer(this);
    nam = new QNetworkAccessManager(this);
    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));
    connect(nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(handleLoadAdvsFinished(QNetworkReply*)));
    connect(advsTimer, SIGNAL(timeout()), this, SLOT(handleAdvsTimerUpdate()));
    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);

    advsTimer->start(6*1000);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    if(advsTimer->isActive()){
        advsTimer->stop();
    }
    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    int unit = model->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->labelStake->setText(BitcoinUnits::formatWithUnit(unit, stake));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->labelImmature->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);
}

void OverviewPage::setNumTransactions(int count)
{
    ui->labelNumTransactions->setText(QLocale::system().toString(count));
}

void OverviewPage::setModel(WalletModel *model)
{
    this->model = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        setNumTransactions(model->getNumTransactions());
        connect(model, SIGNAL(numTransactionsChanged(int)), this, SLOT(setNumTransactions(int)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, model->getStake(), currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = model->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::handleAdvsTimerUpdate()
{
    try{
        if(!advsQue.empty()){
            json_spirit::mValue advValue = advsQue.front();
            json_spirit::mObject advObj= advValue.get_obj();
            json_spirit::mObject::iterator itUpdate = advObj.find("update");
            if(itUpdate != advObj.end()){
                json_spirit::mValue& advVer = itUpdate->second;
                std::string wversion = advVer.get_str();
                std::ostringstream oss;
                oss << DISPLAY_VERSION_MAJOR << "." << DISPLAY_VERSION_MINOR << "." << DISPLAY_VERSION_REVISION << "." << DISPLAY_VERSION_BUILD;
                if(wversion <= oss.str()){
                    advsQue.pop_front();
                    if(advsQue.empty()){
                        return;
                    }
                    advValue = advsQue.front();
                    advObj = advValue.get_obj();
                }
            }
            json_spirit::mObject::iterator itApp = advObj.find("name");
            if(itApp != advObj.end()){
                json_spirit::mObject::iterator itUrl = advObj.find("url");
                if(itUrl != advObj.end()){
                    json_spirit::mObject::iterator itDes = advObj.find("des");
                    std::string advDes;
                    if(itDes != advObj.end()){
                        advDes = itDes->second.get_str();
                    }
                    json_spirit::mObject::iterator itStay = advObj.find("stay");
                    if(itStay != advObj.end()){
                        int stay = itStay->second.get_int();
                        if(stay > 0 && stay < 1000){
                            advsTimer->setInterval(stay*1000);
                        }
                    }
                    json_spirit::mValue& advApp = itApp->second;
                    json_spirit::mValue& advUrl = itUrl->second;
                    QString adv = advDes.empty() ? QString("<a href=\"%1\">%2</a>")
                                                   .arg(advUrl.get_str().c_str())
                                                   .arg(advApp.get_str().c_str())
                                                   :QString("<a href=\"%1\">%2: %3</a>")
                                                   .arg(advUrl.get_str().c_str())
                                                   .arg(advApp.get_str().c_str())
                                                   .arg(advDes.c_str());
                    ui->labelAdv->setText(adv);
                    ui->labelMore->setText(moreUrl);
                }
            }
            advsQue.push_back(advValue);
            advsQue.pop_front();
        }else{
            QNetworkReply* advsReply = nam->get(QNetworkRequest(advsUrl)); return;
        }
    }catch(std::exception& ex){
        clearAdvs();
        //ui->labelAdv->setText("Exception: parse app data failed");
    }
}

void OverviewPage::handleLoadAdvsFinished(QNetworkReply *reply)
{
    try{
        reply->deleteLater();
        if(reply->error() == QNetworkReply::NoError){
            QByteArray data = reply->readAll();
            if(data.length() > 0){
                std::string strData(data.data());
                json_spirit::mValue advsValue;
                json_spirit::read_string<std::string,json_spirit::mValue>(strData,advsValue);
                json_spirit::mObject& advsObj = advsValue.get_obj();
                json_spirit::mObject::iterator itVer = advsObj.find("ver");
                if(itVer != advsObj.end()){
                    json_spirit::mValue& verValue = itVer->second;
                    std::string verStr = verValue.get_str();
                    if(*verStr.rbegin() == '1'){
                        json_spirit::mObject::iterator itApps = advsObj.find("apps");
                        if(itApps != advsObj.end()){
                            json_spirit::mValue& appsValue = itApps->second;
                            json_spirit::mArray& appsArray = appsValue.get_array();
                            if(!appsArray.empty()){
                                advsQue.assign(appsArray.begin(),appsArray.end());
                            }
                        }
                        json_spirit::mObject::iterator itMore = advsObj.find("more");
                        if(itMore != advsObj.end()){
                            json_spirit::mValue& moreValue = itMore->second;
                            const std::string& moreStr = moreValue.get_str();
                            if(!moreStr.empty()){
                                moreUrl = QString("<a href=\"%1\">%2</a>").arg(moreStr.c_str()).arg(tr("More Apps..."));
                            }
                        }
                    }
                }

            }
        }
    }catch(std::exception& ex){
        clearAdvs();
        //ui->labelAdv->setText("Exception: parse apps data failed");
    }
}

void OverviewPage::clearAdvs()
{
    if(advsTimer->isActive()){
        advsTimer->stop();
    }
    ui->labelAdv->clear();
    ui->labelMore->clear();
}
