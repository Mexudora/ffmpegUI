#pragma once

#include "conversioncommand.h"
#include "conversionrunner.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class ConversionQueue : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int count READ count NOTIFY queueChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY queueChanged)
    Q_PROPERTY(bool running READ running NOTIFY queueChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY queueChanged)
    Q_PROPERTY(bool stopRequested READ stopRequested NOTIFY queueChanged)
    Q_PROPERTY(QString error READ error NOTIFY queueChanged)
    Q_PROPERTY(QString selectedLog READ selectedLog NOTIFY detailsChanged)
    Q_PROPERTY(ConversionRunner *runner READ runner CONSTANT)

public:
    enum JobState { Waiting, Starting, Running, Cancelling, Succeeded, Failed, Cancelled };
    Q_ENUM(JobState)
    enum Role {
        IdRole = Qt::UserRole + 1, InputRole, OutputRole, StateRole,
        StatusRole, ProgressRole, ErrorRole
    };

    explicit ConversionQueue(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    int pendingCount() const;
    bool running() const;
    bool busy() const;
    bool stopRequested() const;
    QString error() const;
    QString selectedLog() const;
    ConversionRunner *runner();

    Q_INVOKABLE bool enqueue(ConversionCommand *command, double durationSeconds);
    Q_INVOKABLE void start();
    Q_INVOKABLE void cancelCurrent();
    Q_INVOKABLE void requestStopAfterCurrent();
    Q_INVOKABLE void removeWaiting(const QString &id);
    Q_INVOKABLE void clearFinished();
    Q_INVOKABLE void selectJob(const QString &id);

signals:
    void queueChanged();
    void detailsChanged();

private:
    struct Entry {
        ConversionJob job;
        JobState state = Waiting;
        QString status;
        double progress = -1;
        QString error;
        QString log;
    };

    int findRow(const QString &id) const;
    static bool terminal(JobState state);
    void dispatchNext();
    void onCompleted(ConversionRunner::State state);
    void updateActiveState();

    QList<Entry> m_entries;
    bool m_running = false;
    bool m_stopRequested = false;
    QString m_activeId;
    QString m_selectedId;
    QString m_error;
    ConversionRunner m_runner;
};
