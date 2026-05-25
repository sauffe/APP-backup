#include "privatechatdialog.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>

PrivateChatDialog::PrivateChatDialog(const QString &partnerId,
                                     const QString &myUsername,
                                     QWebSocket *socket,
                                     QWidget *parent)
    : QDialog(parent)
    , m_partnerId(partnerId)
    , m_myUsername(myUsername)
    , m_webSocket(socket)
{
    setupUI();
    setAttribute(Qt::WA_DeleteOnClose);
}

PrivateChatDialog::~PrivateChatDialog()
{
    // 不需要额外操作
}

void PrivateChatDialog::setupUI()
{
    setWindowTitle(QString("与 %1 私聊").arg(m_partnerId));
    resize(400, 300);

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_chatDisplay = new QTextEdit(this);
    m_chatDisplay->setReadOnly(true);
    layout->addWidget(m_chatDisplay);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    m_inputLine = new QLineEdit(this);
    m_sendButton = new QPushButton("发送", this);
    inputLayout->addWidget(m_inputLine);
    inputLayout->addWidget(m_sendButton);
    layout->addLayout(inputLayout);

    connect(m_inputLine, &QLineEdit::returnPressed, this, &PrivateChatDialog::onSendClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &PrivateChatDialog::onSendClicked);
}

void PrivateChatDialog::onSendClicked()
{
    QString content = m_inputLine->text().trimmed();
    if (content.isEmpty()) return;

    QJsonObject data;
    data["to"] = m_partnerId;
    data["content"] = content;
    QJsonObject msg;
    msg["event"] = "private_message";
    msg["data"] = data;

    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        m_webSocket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    }

    appendMessage(m_myUsername, content, QDateTime::currentDateTime().toString(Qt::ISODate));
    m_inputLine->clear();
}

void PrivateChatDialog::appendMessage(const QString &sender,
                                      const QString &content,
                                      const QString &time)
{
    QDateTime dt = QDateTime::fromString(time, Qt::ISODate);
    QString timeStr = dt.toLocalTime().toString("hh:mm:ss");
    QString display = QString("[%1] %2: %3").arg(timeStr, sender, content);
    m_chatDisplay->append(display);

    QScrollBar *sb = m_chatDisplay->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void PrivateChatDialog::closeEvent(QCloseEvent *event)
{
    // 发送 private_close 给后端
    QJsonObject data;
    data["to"] = m_partnerId;
    QJsonObject msg;
    msg["event"] = "private_close";
    msg["data"] = data;

    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        m_webSocket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    }

    // 通知 MainWindow 清理本地映射（仅清理，不再发网络消息）
    emit closedByUser(m_partnerId);

    event->accept();
}
