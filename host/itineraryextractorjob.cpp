/*
    Copyright (C) 2019 by Kai Uwe Broulik <kde@privat.broulik.de>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include "itineraryextractorjob.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QMetaObject>
#include <QProcess>

#include <KIO/Global>

#include <config-host.h>

ItineraryExtractorJob::ItineraryExtractorJob(const QByteArray &inputData, QObject *parent)
    : KJob(parent)
    , m_inputData(inputData)
{

}

ItineraryExtractorJob::ItineraryExtractorJob(const QString &fileName, QObject *parent)
    : KJob(parent)
    , m_fileName(fileName)
{

}

ItineraryExtractorJob::~ItineraryExtractorJob() = default;

bool ItineraryExtractorJob::isSupported()
{
    QFileInfo fi(extractorPath());
    return fi.exists() && fi.isExecutable();
}

QString ItineraryExtractorJob::extractorPath()
{
    return QFile::decodeName(CMAKE_INSTALL_FULL_LIBEXECDIR_KF5 "/kitinerary-extractor");
}

ItineraryExtractorJob::InputType ItineraryExtractorJob::inputType() const
{
    return m_inputType;
}

void ItineraryExtractorJob::setInputType(ItineraryExtractorJob::InputType inputType)
{
    m_inputType = inputType;
}

QJsonArray ItineraryExtractorJob::extractedData() const
{
    return m_extractedData;
}

void ItineraryExtractorJob::start()
{
    QMetaObject::invokeMethod(this, &ItineraryExtractorJob::doStart, Qt::QueuedConnection);
}

void ItineraryExtractorJob::doStart()
{
    if (m_inputData.isEmpty() && m_fileName.isEmpty()) {
        setError(KIO::ERR_UNKNOWN); // TODO proper error code
        emitResult();
        return;
    }

    QProcess *process = new QProcess(this);

    connect(process, &QProcess::started, this, [this, process] {
        if (m_inputData.isEmpty()) {
            return;
        }

        process->write(m_inputData);
        process->closeWriteChannel();
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        process->deleteLater();

        if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            qWarning() << "Failed to extract itinerary information"; // TODO print error
            setError(KIO::ERR_UNKNOWN); // TODO proper error code
            emitResult();
            return;
        }

        const QByteArray output = process->readAllStandardOutput();
        const QJsonArray itineraryInfo = QJsonDocument::fromJson(output).array();

        m_extractedData = QJsonDocument::fromJson(output).array();
        emitResult();
    });

    QStringList args;
    if (!m_fileName.isEmpty()) {
        args << m_fileName;
    }

    if (m_inputType != InputType::Any) {
        const QMetaEnum me = QMetaEnum::fromType<InputType>();

        const QString key = QString::fromUtf8(me.key(static_cast<int>(m_inputType)));
        if (key.isEmpty()) {
            setError(KIO::ERR_UNKNOWN); // TODO proper error
            emitResult();
            return;
        }

        args << QLatin1String("-t") << key;
    }

    process->start(extractorPath(), args);
}
