#include "mainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QScrollBar>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QPoint>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
{
    setupUI();
    connectWebSocket();

    m_blinkTimer = new QTimer(this);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
        static bool visible = true;
        for (const QString &contact : m_blinkingContacts) {
            for (int i = 0; i < m_contactList->count(); ++i) {
                if (m_contactList->item(i)->data(Qt::UserRole).toString() == contact) {
                    m_contactList->item(i)->setHidden(!visible);
                    break;
                }
            }
        }
        visible = !visible;
    });
    m_blinkTimer->start(500);
}

MainWindow::~MainWindow()
{
    m_webSocket->close();
}

void MainWindow::setupUI()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout *mainLayout = new QHBoxLayout(central);

    // 左侧公屏
    QVBoxLayout *leftLayout = new QVBoxLayout();
    m_publicDisplay = new QTextEdit(this);
    m_publicDisplay->setReadOnly(true);
    // 启用右键菜单
    m_publicDisplay->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_publicDisplay, &QTextEdit::customContextMenuRequested,
            this, &MainWindow::onPublicDisplayCustomContextMenu);

    leftLayout->addWidget(m_publicDisplay);

    QHBoxLayout *pubInputLayout = new QHBoxLayout();
    m_publicInput = new QLineEdit(this);
    m_publicSendButton = new QPushButton("发送", this);
    pubInputLayout->addWidget(m_publicInput);
    pubInputLayout->addWidget(m_publicSendButton);
    leftLayout->addLayout(pubInputLayout);

    connect(m_publicInput, &QLineEdit::returnPressed, this, &MainWindow::onSendPublicClicked);
    connect(m_publicSendButton, &QPushButton::clicked, this, &MainWindow::onSendPublicClicked);

    mainLayout->addLayout(leftLayout, 3);

    // 右侧私聊联系人
    QVBoxLayout *rightLayout = new QVBoxLayout();
    m_contactLabel = new QLabel("私聊联系人", this);
    rightLayout->addWidget(m_contactLabel);
    m_contactList = new QListWidget(this);
    connect(m_contactList, &QListWidget::itemClicked, this, &MainWindow::onContactClicked);
    rightLayout->addWidget(m_contactList);
    mainLayout->addLayout(rightLayout, 1);

    setWindowTitle("聊天室");
    resize(700, 500);
}

void MainWindow::connectWebSocket()
{
    connect(m_webSocket, &QWebSocket::connected, this, &MainWindow::onConnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &MainWindow::onTextMessageReceived);
    m_webSocket->open(QUrl("ws://localhost:3000"));
}

void MainWindow::onConnected()
{
    qDebug() << "已连接";
}

void MainWindow::onTextMessageReceived(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString event = obj["event"].toString();
    QJsonObject data = obj["data"].toObject();

    if (event == "username") {
        m_username = data["username"].toString();
        setWindowTitle(QString("聊天室 - %1").arg(m_username));
        m_publicDisplay->append(QString("系统：你的用户名是 %1").arg(m_username));
    }
    else if (event == "public_message") {
        QString sender = data["sender"].toString();
        QString content = data["content"].toString();
        QString time = data["time"].toString();
        QDateTime dt = QDateTime::fromString(time, Qt::ISODate);
        QString timeStr = dt.toLocalTime().toString("hh:mm:ss");
        QString display = QString("[%1] %2: %3").arg(timeStr, sender, content);
        m_publicDisplay->append(display);
    }
    else if (event == "private_invite") {
        QString from = data["from"].toString();
        addOrUpdateContact(from, true);  // 闪烁
    }
    else if (event == "private_message") {
        QString from = data["from"].toString();
        QString to = data["to"].toString();
        QString content = data["content"].toString();
        QString time = data["time"].toString();

        if (from == m_username) {
            return;  // 自己发的消息已在本地显示，不重复
        }

        QString partner = from; // 对方就是 from
        addOrUpdateContact(partner, false);

        PrivateChatDialog *dlg = m_privateDialogs.value(partner);
        if (!dlg) {
            openOrFocusPrivateDialog(partner);
            dlg = m_privateDialogs.value(partner);
        }
        if (dlg) {
            dlg->appendMessage(from, content, time);
        }
    }
    else if (event == "private_close") {
        QString from = data["from"].toString();
        QString to = data["to"].toString();
        QString partner = (from == m_username) ? to : from;
        PrivateChatDialog *dlg = m_privateDialogs.take(partner);
        if (dlg) {
            dlg->disconnect();
            dlg->close();
            dlg->deleteLater();
        }
        removeContact(partner);
    }
    else if (event == "error") {
        QString msg = data["message"].toString();
        m_publicDisplay->append(QString("系统错误：%1").arg(msg));
    }
}

void MainWindow::onSendPublicClicked()
{
    QString content = m_publicInput->text().trimmed();
    if (content.isEmpty()) return;

    // 处理 /invite 命令（保留）
    if (content.startsWith("/invite ")) {
        QString targetId = content.mid(8).trimmed();
        if (!targetId.isEmpty()) {
            QJsonObject data;
            data["to"] = targetId;
            QJsonObject msg;
            msg["event"] = "private_invite";
            msg["data"] = data;
            m_webSocket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));

            addOrUpdateContact(targetId, false);
            openOrFocusPrivateDialog(targetId);
        }
        m_publicInput->clear();
        return;
    }

    // 普通公屏消息
    QJsonObject data;
    data["content"] = content;
    QJsonObject msg;
    msg["event"] = "public_message";
    msg["data"] = data;
    m_webSocket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    m_publicInput->clear();
}

void MainWindow::onPublicDisplayCustomContextMenu(const QPoint &pos)
{
    QString selectedText = m_publicDisplay->textCursor().selectedText().trimmed();
    if (selectedText.isEmpty()) return;

    // 只对形如 User_xxx 的文本显示回复菜单
    if (!selectedText.startsWith("User_")) return;

    QMenu *menu = new QMenu(this);
    QAction *replyAction = menu->addAction(QString("回复 %1").arg(selectedText));
    connect(replyAction, &QAction::triggered, this, [this, selectedText]() {
        QJsonObject data;
        data["to"] = selectedText;
        QJsonObject msg;
        msg["event"] = "private_invite";
        msg["data"] = data;
        m_webSocket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));

        addOrUpdateContact(selectedText, false);
        openOrFocusPrivateDialog(selectedText);
    });
    menu->exec(m_publicDisplay->viewport()->mapToGlobal(pos));
    delete menu;
}

void MainWindow::onContactClicked(QListWidgetItem *item)
{
    QString partnerId = item->data(Qt::UserRole).toString();
    openOrFocusPrivateDialog(partnerId);
    m_blinkingContacts.remove(partnerId);
    for (int i = 0; i < m_contactList->count(); ++i) {
        if (m_contactList->item(i)->data(Qt::UserRole).toString() == partnerId) {
            m_contactList->item(i)->setHidden(false);
            break;
        }
    }
}

void MainWindow::onPrivateClose(const QString &partnerId)
{
    // 只做本地清理，网络消息已在对话框 closeEvent 中发送
    m_privateDialogs.remove(partnerId);
    removeContact(partnerId);
}

// 辅助函数
void MainWindow::addOrUpdateContact(const QString &partnerId, bool hasNew)
{
    if (!m_contacts.contains(partnerId)) {
        QListWidgetItem *item = new QListWidgetItem(partnerId);
        item->setData(Qt::UserRole, partnerId);
        m_contactList->addItem(item);
        m_contacts.insert(partnerId);
    }
    if (hasNew) {
        m_blinkingContacts.insert(partnerId);
    }
}

void MainWindow::removeContact(const QString &partnerId)
{
    m_contacts.remove(partnerId);
    m_blinkingContacts.remove(partnerId);
    for (int i = 0; i < m_contactList->count(); ++i) {
        if (m_contactList->item(i)->data(Qt::UserRole).toString() == partnerId) {
            delete m_contactList->takeItem(i);
            break;
        }
    }
}

void MainWindow::openOrFocusPrivateDialog(const QString &partnerId)
{
    PrivateChatDialog *dlg = m_privateDialogs.value(partnerId);
    if (!dlg) {
        dlg = new PrivateChatDialog(partnerId, m_username, m_webSocket, this);
        connect(dlg, &PrivateChatDialog::closedByUser, this, &MainWindow::onPrivateClose);
        m_privateDialogs.insert(partnerId, dlg);
        dlg->show();
    } else {
        dlg->raise();
        dlg->activateWindow();
    }
}
