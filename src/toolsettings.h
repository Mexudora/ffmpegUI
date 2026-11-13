#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// One instance belongs to MediaWorkspace; QML cannot construct another one.
class ToolSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use MediaWorkspace.tools")
    Q_PROPERTY(QString directory READ directory NOTIFY directoryChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit ToolSettings(QObject *parent = nullptr);
    QString directory() const;
    QString error() const;
    QString ffmpegPath() const;
    QString ffprobePath() const;
    QString validationError() const;
    Q_INVOKABLE bool saveDirectory(const QString &directory);
    Q_INVOKABLE QString pathFromUrl(const QUrl &url) const;
    Q_INVOKABLE void clearError();

signals:
    void directoryChanged();
    void errorChanged();

private:
    static QString validateDirectory(const QString &directory);
    QString m_directory;
    QString m_error;
};
