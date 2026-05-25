// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QWebSocket>
#include <QListWidget>
#include <QLabel>
#include <QMap>
#include <QTimer>
#include <QSet>
#include <QMenu>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "privatechatdialog.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnected();
    void onTextMessageReceived(const QString &message);
    void onSendPublicClicked();
    void onContactClicked(QListWidgetItem *item);
    void onPrivateClose(const QString &partnerId);
    void onPublicDisplayCustomContextMenu(const QPoint &pos);
    void onToggleSidebar();          // 切换侧边栏
    void onSendMail();               // 投递信件
    void onRetrieveMail();           // 领取信件
    void updateCountdown();          // 更新倒计时

private:
    void setupUI();
    void connectWebSocket();
    void addOrUpdateContact(const QString &partnerId, bool hasNew);
    void removeContact(const QString &partnerId);
    void openOrFocusPrivateDialog(const QString &partnerId);

    // UI 组件
    QSplitter *m_splitter;

    // 左侧公屏
    QTextEdit *m_publicDisplay;
    QLineEdit *m_publicInput;
    QPushButton *m_publicSendButton;

    // 右侧侧边栏
    QWidget *m_sidebarWidget;
    QPushButton *m_toggleBtn;
    QStackedWidget *m_stackedWidget;

    // 信箱页面控件
    QLineEdit *m_mailKeywordSend;    // 投递关键词
    QTextEdit *m_mailContentEdit;    // 投递内容
    QPushButton *m_sendMailBtn;
    QLineEdit *m_mailKeywordRetrieve; // 领取关键词
    QPushButton *m_retrieveMailBtn;
    QLabel *m_mailResultLabel;       // 显示领取结果
    QLabel *m_countdownLabel;        // 显示倒计时
    QTimer *m_countdownTimer;
    int m_remainingSeconds;          // 剩余秒数

    // 私聊会话列表（Page 1）
    QListWidget *m_contactList;
    QLabel *m_contactLabel;

    QWebSocket *m_webSocket;
    QString m_username;

    // 私聊管理
    QMap<QString, PrivateChatDialog*> m_privateDialogs;
    QTimer *m_blinkTimer;
    QSet<QString> m_blinkingContacts;
    QSet<QString> m_contacts;

    // 网络管理器
    QNetworkAccessManager *m_networkManager;
};

#endif // MAINWINDOW_H
