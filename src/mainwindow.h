#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "protocolparser.h"
#include "transmissionrepository.h"
#include "udpsendcontroller.h"

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QProgressBar;
class QTableWidget;
class QTextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);

private slots:
    void browseProtocol();
    void startSending();
    void stopSending();
    void searchLogs();
    void runBenchmark();
    void updateProgress(int sentCount, int totalCount);
    void appendPayload(const QString &payload);
    void finishRun(const QString &message);

private:
    void buildUi();
    bool loadProtocol(const QString &path);
    void renderLogs(const QList<TransmissionLogEntry> &entries);

    ProtocolParser m_parser;
    TransmissionRepository m_repository;
    UdpSendController m_controller;
    QLabel *m_statusLabel;
    QLineEdit *m_protocolPathEdit;
    QLineEdit *m_frequencyEdit;
    QLineEdit *m_countEdit;
    QLineEdit *m_ipEdit;
    QLineEdit *m_portEdit;
    QProgressBar *m_progressBar;
    QTextEdit *m_previewText;
    QLineEdit *m_timeFilterEdit;
    QLineEdit *m_protocolFilterEdit;
    QLineEdit *m_ipFilterEdit;
    QTableWidget *m_logTable;
};

#endif
