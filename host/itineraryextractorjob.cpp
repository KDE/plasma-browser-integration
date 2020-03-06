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
#include <QTimer>

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

ItineraryExtractorJob::OutputType ItineraryExtractorJob::outputType() const
{
    return m_outputType;
}

void ItineraryExtractorJob::setOutputType(ItineraryExtractorJob::OutputType outputType)
{
    m_outputType = outputType;
}

QByteArray ItineraryExtractorJob::extractedData() const
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

    m_process = new QProcess(this);
    m_process->setProgram(extractorPath());

    QStringList args;

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

    if (m_outputType != OutputType::Default) {
        const QMetaEnum me = QMetaEnum::fromType<OutputType>();

        const QString key = QString::fromUtf8(me.key(static_cast<int>(m_outputType)));
        if (key.isEmpty()) {
            setError(KIO::ERR_UNKNOWN); // TODO proper error
            emitResult();
            return;
        }
    }

    if (!m_fileName.isEmpty()) {
        args << m_fileName;
    }

    m_process->setArguments(args);

    connect(m_process, &QProcess::started, this, [this] {
        m_process->write(m_inputData);
        m_process->closeWriteChannel();

        m_killTimer = new QTimer(this);
        m_killTimer->setSingleShot(true);
        m_killTimer->setInterval(10000);
        connect(m_killTimer, &QTimer::timeout, this, [this] {
            qWarning() << "Itinerary took too long, killing it";
            m_process->kill();
        });
        m_killTimer->start();
    });

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (m_killTimer) {
            m_killTimer->stop();
        }

        if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            qWarning() << "Failed to extract itinerary information" << exitCode << exitStatus << m_process->errorString();
            setError(KIO::ERR_UNKNOWN); // TODO proper error code
            setErrorText(m_process->errorString());
            emitResult();
            return;
        }

        m_extractedData = m_process->readAllStandardOutput();
        emitResult();
    });

    m_process->start();
}
