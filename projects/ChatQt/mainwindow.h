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
    void onPublicDisplayCustomContextMenu(const QPoint &pos);  // 新增

private:
    void setupUI();
    void connectWebSocket();
    void addOrUpdateContact(const QString &partnerId, bool hasNew);
    void removeContact(const QString &partnerId);
    void openOrFocusPrivateDialog(const QString &partnerId);

    // UI 组件
    QTextEdit *m_publicDisplay;
    QLineEdit *m_publicInput;
    QPushButton *m_publicSendButton;

    QListWidget *m_contactList;
    QLabel *m_contactLabel;

    QWebSocket *m_webSocket;
    QString m_username;

    // 私聊管理
    QMap<QString, PrivateChatDialog*> m_privateDialogs;
    QTimer *m_blinkTimer;
    QSet<QString> m_blinkingContacts;
    QSet<QString> m_contacts;
};

#endif // MAINWINDOW_H
