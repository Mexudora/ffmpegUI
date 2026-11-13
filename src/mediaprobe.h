#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class MediaProbe : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString executablePath READ executablePath WRITE setExecutablePath
               NOTIFY executablePathChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY sourcePathChanged)
    Q_PROPERTY(QVariantMap metadata READ metadata NOTIFY metadataChanged)

public:
    explicit MediaProbe(QObject *parent = nullptr);
    ~MediaProbe() override;

    QString executablePath() const;
    void setExecutablePath(const QString &path);
    bool busy() const;
    QString error() const;
    QString sourcePath() const;
    QVariantMap metadata() const;

    Q_INVOKABLE void probe(const QUrl &fileUrl);

signals:
    void executablePathChanged();
    void busyChanged();
    void errorChanged();
    void sourcePathChanged();
    void metadataChanged();
    void completed(bool success);

private:
    void setBusy(bool busy);
    void setError(const QString &error);
    void finishFailure(const QString &message);
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);

    QProcess m_process;
    QString m_executablePath;
    bool m_busy = false;
    QString m_error;
    QString m_sourcePath;
    QVariantMap m_metadata;
};
