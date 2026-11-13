#pragma once

#include "conversionjob.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class ConversionCommand : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString executablePath READ executablePath WRITE setExecutablePath
               NOTIFY optionsChanged)
    Q_PROPERTY(QUrl inputFile READ inputFile WRITE setInputFile NOTIFY optionsChanged)
    Q_PROPERTY(QUrl outputFile READ outputFile WRITE setOutputFile NOTIFY optionsChanged)
    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY optionsChanged)
    Q_PROPERTY(QString container READ container WRITE setContainer NOTIFY optionsChanged)
    Q_PROPERTY(QString videoCodec READ videoCodec WRITE setVideoCodec NOTIFY optionsChanged)
    Q_PROPERTY(QString audioCodec READ audioCodec WRITE setAudioCodec NOTIFY optionsChanged)
    Q_PROPERTY(int audioBitrateKbps READ audioBitrateKbps WRITE setAudioBitrateKbps NOTIFY optionsChanged)

    Q_PROPERTY(bool valid READ valid NOTIFY commandChanged)
    Q_PROPERTY(QString error READ error NOTIFY commandChanged)
    Q_PROPERTY(QString program READ program NOTIFY commandChanged)
    Q_PROPERTY(QStringList arguments READ arguments NOTIFY commandChanged)
    Q_PROPERTY(QString commandPreview READ commandPreview NOTIFY commandChanged)

public:
    explicit ConversionCommand(QObject *parent = nullptr);

    QString executablePath() const;
    void setExecutablePath(const QString &path);
    QUrl inputFile() const;
    void setInputFile(const QUrl &url);
    QUrl outputFile() const;
    void setOutputFile(const QUrl &url);
    QString outputPath() const;
    void setOutputPath(const QString &path);
    QString container() const;
    void setContainer(const QString &container);
    QString videoCodec() const;
    void setVideoCodec(const QString &codec);
    QString audioCodec() const;
    void setAudioCodec(const QString &codec);
    int audioBitrateKbps() const;
    void setAudioBitrateKbps(int bitrate);
    ConversionJob createJob(double durationSeconds) const;

    bool valid() const;
    QString error() const;
    QString program() const;
    QStringList arguments() const;
    QString commandPreview() const;
    Q_INVOKABLE void copyPreview() const;
    Q_INVOKABLE bool refresh();

signals:
    void optionsChanged();
    void commandChanged();

private:
    void rebuild();

    QString m_executablePath = QStringLiteral("C:/ffmpeg/bin/ffmpeg.exe");
    QUrl m_inputFile;
    QUrl m_outputFile;
    QString m_container = QStringLiteral("mp4");
    QString m_videoCodec = QStringLiteral("libx264");
    QString m_audioCodec = QStringLiteral("aac");
    int m_audioBitrateKbps = 192;
    QString m_error;
    QString m_program;
    QStringList m_arguments;
    QString m_commandPreview;
};
