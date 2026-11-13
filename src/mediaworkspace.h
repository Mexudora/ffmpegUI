#pragma once

#include "conversionqueue.h"
#include "mediaprobe.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class MediaWorkspace : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList files READ files NOTIFY filesChanged)
    Q_PROPERTY(int count READ count NOTIFY filesChanged)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectedChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectedChanged)
    Q_PROPERTY(QVariantMap selectedFile READ selectedFile NOTIFY selectedChanged)
    Q_PROPERTY(bool selectedInSet READ selectedInSet NOTIFY selectedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(QString error READ error NOTIFY statusChanged)
    Q_PROPERTY(QString message READ message NOTIFY statusChanged)
    Q_PROPERTY(QString container READ container WRITE setContainer NOTIFY settingsChanged)
    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY settingsChanged)
    Q_PROPERTY(bool outputIsDirectory READ outputIsDirectory WRITE setOutputIsDirectory NOTIFY settingsChanged)

public:
    explicit MediaWorkspace(QObject *parent = nullptr);
    QVariantList files() const;
    int count() const;
    QVariantList rows() const;
    QString selectedId() const;
    int selectedIndex() const;
    QVariantMap selectedFile() const;
    bool selectedInSet() const;
    bool busy() const;
    QString error() const;
    QString message() const;
    QString container() const;
    void setContainer(const QString &container);
    QString outputPath() const;
    void setOutputPath(const QString &path);
    bool outputIsDirectory() const;
    void setOutputIsDirectory(bool directory);

    Q_INVOKABLE void openFile(const QUrl &url);
    Q_INVOKABLE void addFiles(const QList<QUrl> &urls);
    Q_INVOKABLE void addFolder(const QUrl &url);
    Q_INVOKABLE void addCurrent();
    Q_INVOKABLE void removeFile(const QString &id);
    Q_INVOKABLE void clearSet();
    Q_INVOKABLE void selectFile(const QString &id);
    Q_INVOKABLE void selectIndex(int index);
    Q_INVOKABLE void setIncluded(const QString &rowId, const QString &fileId, bool checked);
    Q_INVOKABLE void setRowIncluded(const QString &rowId, bool checked);
    Q_INVOKABLE void setRowCodec(const QString &rowId, const QString &codec);
    Q_INVOKABLE void setRowBitrate(const QString &rowId, int bitrate);
    Q_INVOKABLE void chooseOutputDirectory(const QUrl &url);
    Q_INVOKABLE void chooseOutputFile(const QUrl &url);
    Q_INVOKABLE bool enqueue(ConversionQueue *queue);
    Q_INVOKABLE QString commandPreview(const QString &fileId) const;

signals:
    void filesChanged();
    void rowsChanged();
    void selectedChanged();
    void statusChanged();
    void settingsChanged();

private:
    enum LoadState { Loading, Ready, Error };
    struct MediaFile {
        QString id, path;
        LoadState state = Loading;
        QVariantMap metadata;
        QString error;
    };
    struct Cell { QVariantMap stream; bool checked = true; };
    struct Row {
        QString id, type, title, language, codec, seedCodec;
        int occurrence = 1;
        int bitrate = 192;
        bool attached = false;
        bool defaultIncluded = true;
        QHash<QString, Cell> cells;
    };
    struct Request { QString id, path; };

    MediaFile *findFile(const QString &id);
    const MediaFile *findFile(const QString &id) const;
    int findStagedPath(const QString &path) const;
    QVariantMap fileMap(const MediaFile &file) const;
    QString defaultCodec(const Row &row) const;
    QVariantList codecOptions(const Row &row) const;
    void rebuildRows();
    void dispatchProbe();
    void probeCompleted(bool success);
    void normalizeOutputMode();
    ConversionJob jobFor(const MediaFile &file, const QString &output) const;
    QString destinationFor(const MediaFile &file) const;

    QList<MediaFile> m_files;
    MediaFile m_preview;
    bool m_hasPreview = false;
    QList<Row> m_rows;
    QString m_selectedId;
    QString m_container = QStringLiteral("mp4");
    QString m_outputPath;
    bool m_outputDirectory = true;
    QString m_error, m_message;
    QQueue<Request> m_requests;
    QString m_activeId;
    bool m_processing = false;
    MediaProbe m_probe;
};
