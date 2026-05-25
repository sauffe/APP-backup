#include "mainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QScrollBar>
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QPoint>
#include <QNetworkRequest>
#include <QUrl>
#include<QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_countdownTimer(new QTimer(this))
    , m_remainingSeconds(0)
{
    setupUI();
    connectWebSocket();

    // 闪烁定时器（联系人列表）
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

    // 倒计时定时器
    connect(m_countdownTimer, &QTimer::timeout, this, &MainWindow::updateCountdown);
}

MainWindow::~MainWindow()
{
    m_webSocket->close();
}

void MainWindow::setupUI()
{
    // 全局 QSS（样式美化）
    setStyleSheet(R"(
        QMainWindow {
            background-color: #f0f2f5;
        }
        QTextEdit, QLineEdit, QListWidget {
            border: 1px solid #e8e8e8;
            border-radius: 4px;
            padding: 4px;
            background: white;
        }
        QPushButton {
            background-color: #1890ff;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #40a9ff;
        }
        QPushButton:pressed {
            background-color: #096dd9;
        }
    )");

    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0,0,0,0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(m_splitter);

    // ========== 左侧公屏 ==========
    QWidget *leftWidget = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0,0,0,0);

    m_publicDisplay = new QTextEdit(this);
    m_publicDisplay->setReadOnly(true);
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

    m_splitter->addWidget(leftWidget);

    // ========== 右侧侧边栏 ==========
    m_sidebarWidget = new QWidget(this);
    m_sidebarWidget->setStyleSheet("background-color: white; border-left: 1px solid #e8e8e8;");
    QVBoxLayout *sidebarLayout = new QVBoxLayout(m_sidebarWidget);
    sidebarLayout->setContentsMargins(0,0,0,0);

    m_toggleBtn = new QPushButton("📮 信箱", this);
    connect(m_toggleBtn, &QPushButton::clicked, this, &MainWindow::onToggleSidebar);
    sidebarLayout->addWidget(m_toggleBtn);

    m_stackedWidget = new QStackedWidget(this);
    sidebarLayout->addWidget(m_stackedWidget);

    // --- Page 0: 信箱界面（重写） ---
    QWidget *mailPage = new QWidget(this);
    QVBoxLayout *mailLayout = new QVBoxLayout(mailPage);
    mailLayout->setContentsMargins(8,8,8,8);

    // 投递区
    QLabel *sendTitle = new QLabel("📥 投递匿名信", this);
    sendTitle->setStyleSheet("font-weight: bold; font-size: 14px; margin-top: 10px;");
    mailLayout->addWidget(sendTitle);

    QLabel *kwSendLabel = new QLabel("关键词：", this);
    m_mailKeywordSend = new QLineEdit(this);
    mailLayout->addWidget(kwSendLabel);
    mailLayout->addWidget(m_mailKeywordSend);

    QLabel *contentLabel = new QLabel("内容：", this);
    m_mailContentEdit = new QTextEdit(this);
    m_mailContentEdit->setMaximumHeight(100);
    mailLayout->addWidget(contentLabel);
    mailLayout->addWidget(m_mailContentEdit);

    m_sendMailBtn = new QPushButton("投递", this);
    m_sendMailBtn->setStyleSheet(
        "QPushButton { background-color: #1890ff; color: white; border: none; border-radius: 4px; padding: 6px 16px; font-size: 14px; }"
        "QPushButton:hover { background-color: #40a9ff; }"
        "QPushButton:pressed { background-color: #096dd9; }"
        );
    connect(m_sendMailBtn, &QPushButton::clicked, this, &MainWindow::onSendMail);
    mailLayout->addWidget(m_sendMailBtn);

    // 分隔线
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #e8e8e8;");
    mailLayout->addWidget(line);

    // 领取区
    QLabel *retrieveTitle = new QLabel("📬 领取信件", this);

    retrieveTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    mailLayout->addWidget(retrieveTitle);

    QLabel *kwRetLabel = new QLabel("关键词：", this);
    m_mailKeywordRetrieve = new QLineEdit(this);
    mailLayout->addWidget(kwRetLabel);
    mailLayout->addWidget(m_mailKeywordRetrieve);

    m_retrieveMailBtn = new QPushButton("领取", this);
    m_retrieveMailBtn->setStyleSheet(
        "QPushButton { background-color: #1890ff; color: white; border: none; border-radius: 4px; padding: 6px 16px; font-size: 14px; }"
        "QPushButton:hover { background-color: #40a9ff; }"
        "QPushButton:pressed { background-color: #096dd9; }"
        );
    connect(m_retrieveMailBtn, &QPushButton::clicked, this, &MainWindow::onRetrieveMail);
    mailLayout->addWidget(m_retrieveMailBtn);

    // 结果显示区
    m_mailResultLabel = new QLabel("", this);
    m_mailResultLabel->setWordWrap(true);
    m_mailResultLabel->setStyleSheet("background: #f6ffed; border: 1px solid #b7eb8f; padding: 8px; border-radius: 4px;");
    m_mailResultLabel->hide();
    m_countdownLabel = new QLabel("", this);
    m_countdownLabel->setStyleSheet("font-size: 12px; color: #888;");
    mailLayout->addWidget(m_mailResultLabel);
    mailLayout->addWidget(m_countdownLabel);
    mailLayout->addStretch();

    m_stackedWidget->addWidget(mailPage);  // index 0

    // --- Page 1: 私聊会话列表（同之前） ---
    QWidget *chatPage = new QWidget(this);
    QVBoxLayout *chatPageLayout = new QVBoxLayout(chatPage);
    chatPageLayout->setContentsMargins(8,8,8,8);

    m_contactLabel = new QLabel("私聊会话", this);
    m_contactLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    chatPageLayout->addWidget(m_contactLabel);

    m_contactList = new QListWidget(this);
    connect(m_contactList, &QListWidget::itemClicked, this, &MainWindow::onContactClicked);
    chatPageLayout->addWidget(m_contactList);

    m_stackedWidget->addWidget(chatPage);  // index 1

    m_stackedWidget->setCurrentIndex(0);   // 默认显示信箱

    m_splitter->addWidget(m_sidebarWidget);
    m_splitter->setStretchFactor(0, 3);   // 左3/4
    m_splitter->setStretchFactor(1, 1);   // 右1/4

    setWindowTitle("聊天室");
    resize(900, 600);
}

// ========== 信箱功能实现 ==========
void MainWindow::onSendMail()
{
    QString keyword = m_mailKeywordSend->text().trimmed();
    QString content = m_mailContentEdit->toPlainText().trimmed();
    if (keyword.isEmpty() || content.isEmpty()) return;

    QJsonObject json;
    json["keyword"] = keyword;
    json["content"] = content;

    QNetworkRequest request(QUrl("http://localhost:3000/mail/send"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QByteArray data = QJsonDocument(json).toJson();

    QNetworkReply *reply = m_networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            m_mailKeywordSend->clear();
            m_mailContentEdit->clear();
            QMessageBox::information(this, "成功", "信件投递成功！");
        } else {
            QMessageBox::warning(this, "失败", "投递失败：" + reply->errorString());
        }
        reply->deleteLater();
    });
}

void MainWindow::onRetrieveMail()
{
    QString keyword = m_mailKeywordRetrieve->text().trimmed();
    if (keyword.isEmpty()) return;

    QJsonObject json;
    json["keyword"] = keyword;

    QNetworkRequest request(QUrl("http://localhost:3000/mail/retrieve"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QByteArray data = QJsonDocument(json).toJson();

    QNetworkReply *reply = m_networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            QJsonObject obj = doc.object();

            if (obj.contains("content")) {
                QString content = obj["content"].toString();
                QString createdAt = obj["createdAt"].toString();
                int remaining = obj["remaining"].toInt();

                // 显示内容
                QDateTime dt = QDateTime::fromString(createdAt, Qt::ISODate);
                QString timeStr = dt.toLocalTime().toString("hh:mm:ss");
                m_mailResultLabel->setText(QString("内容：%1\n投递时间：%2").arg(content, timeStr));
                m_mailResultLabel->show();

                // 倒计时
                if (remaining > 0) {
                    m_remainingSeconds = remaining;
                    m_countdownTimer->start(1000);  // 每秒更新
                    updateCountdown();              // 立即显示
                } else {
                    m_countdownLabel->clear();
                    m_countdownTimer->stop();
                }
            } else {
                // 未找到信件
                m_mailResultLabel->setText("没有找到相关信件，或信件已失效");
                m_mailResultLabel->show();
                m_countdownLabel->clear();
                m_countdownTimer->stop();
            }
        } else {
            QMessageBox::warning(this, "失败", "领取失败：" + reply->errorString());
        }
        reply->deleteLater();
    });
}

void MainWindow::updateCountdown()
{
    if (m_remainingSeconds > 0) {
        m_countdownLabel->setText(QString("剩余有效时间：%1 秒").arg(m_remainingSeconds));
        m_remainingSeconds--;
    } else {
        m_countdownLabel->setText("信件已失效");
        m_countdownTimer->stop();
        // 可选：清空显示
        QTimer::singleShot(2000, this, [this]() {
            m_mailResultLabel->hide();
            m_countdownLabel->clear();
        });
    }
}

// ========== 侧边栏切换 ==========
void MainWindow::onToggleSidebar()
{
    int current = m_stackedWidget->currentIndex();
    if (current == 0) {
        m_stackedWidget->setCurrentIndex(1);
        m_toggleBtn->setText("💬 聊天");
    } else {
        m_stackedWidget->setCurrentIndex(0);
        m_toggleBtn->setText("📮 信箱");
    }
}

// ========== WebSocket 连接 ==========
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

// ========== 消息处理 ==========
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
        addOrUpdateContact(from, true); // 闪烁提示
    }
    else if (event == "private_message") {
        QString from = data["from"].toString();
        QString to = data["to"].toString();
        QString content = data["content"].toString();
        QString time = data["time"].toString();

        // 自己发的消息已经在本地显示过，不重复追加
        if (from == m_username) return;

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
            dlg->disconnect(); // 防止关闭信号再次触发
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

// ========== 公屏发送 ==========
void MainWindow::onSendPublicClicked()
{
    QString content = m_publicInput->text().trimmed();
    if (content.isEmpty()) return;

    // 支持 /invite 命令快速发起私聊
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

// ========== 右键回复 ==========
void MainWindow::onPublicDisplayCustomContextMenu(const QPoint &pos)
{
    QString selectedText = m_publicDisplay->textCursor().selectedText().trimmed();
    if (selectedText.isEmpty() || !selectedText.startsWith("User_")) return;

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

// ========== 联系人列表 ==========
void MainWindow::onContactClicked(QListWidgetItem *item)
{
    QString partnerId = item->data(Qt::UserRole).toString();
    openOrFocusPrivateDialog(partnerId);

    // 停止闪烁并显示
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
    // 网络消息已在 PrivateChatDialog::closeEvent 中发送，这里只做本地清理
    m_privateDialogs.remove(partnerId);
    removeContact(partnerId);
}

// ========== 辅助函数 ==========
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
// ========== 以下为公屏、私聊、右键回复等原有逻辑（需保留） ==========
// 请将上一版 mainwindow.cpp 中的 onConnected, onTextMessageReceived, onSendPublicClicked,
// onPublicDisplayCustomContextMenu, onContactClicked, onPrivateClose,
// addOrUpdateContact, removeContact, openOrFocusPrivateDialog 等函数完整复制到这里
// （限于篇幅，此处省略，但必须包含）
