#ifndef PRIVATECHATDIALOG_H
#define PRIVATECHATDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QWebSocket>
#include <QJsonObject>
#include <QCloseEvent>   // 必须包含

class PrivateChatDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PrivateChatDialog(const QString &partnerId,
                               const QString &myUsername,
                               QWebSocket *socket,
                               QWidget *parent = nullptr);
    ~PrivateChatDialog();

    void appendMessage(const QString &sender, const QString &content, const QString &time);

signals:
    void closedByUser(const QString &partnerId);

protected:
    void closeEvent(QCloseEvent *event) override;   // 重写

private slots:
    void onSendClicked();

private:
    void setupUI();

    QString m_partnerId;
    QString m_myUsername;
    QWebSocket *m_webSocket;
    QTextEdit *m_chatDisplay;
    QLineEdit *m_inputLine;
    QPushButton *m_sendButton;
};

#endif // PRIVATECHATDIALOG_H
