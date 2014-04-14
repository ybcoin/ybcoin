#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include <QUrl>

#include "json/json_spirit.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

namespace Ui {
    class OverviewPage;
}
class WalletModel;
class TxViewDelegate;
class TransactionFilterProxy;
class QTimer;
class QNetworkAccessManager;
class QNetworkReply;
/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = 0);
    ~OverviewPage();

    void setModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);
    void setNumTransactions(int count);

signals:
    void transactionClicked(const QModelIndex &index);

private:
    Ui::OverviewPage *ui;
    WalletModel *model;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;

    TxViewDelegate *txdelegate;
    TransactionFilterProxy *filter;
    QTimer *advsTimer;
    QNetworkAccessManager* nam;
    //QNetworkReply* advsReply;
    const QUrl advsUrl;
    std::deque<json_spirit::mValue> advsQue;
    QString moreUrl;
private:
    void clearAdvs();
private slots:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
    void handleLoadAdvsFinished(QNetworkReply* reply);
    void handleAdvsTimerUpdate();
};

#endif // OVERVIEWPAGE_H
