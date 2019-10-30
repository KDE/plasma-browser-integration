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

#pragma once

#include <KJob>

#include <QByteArray>
#include <QJsonArray>
#include <QString>

class ItineraryExtractorJob : public KJob
{
    Q_OBJECT

public:
    ItineraryExtractorJob(const QByteArray &inputData, QObject *parent = nullptr);
    ItineraryExtractorJob(const QString &fileName, QObject *parent = nullptr);
    ~ItineraryExtractorJob() override;

    enum class InputType {
        Any,
        Email,
        Pdf,
        PkPass,
        ICal,
        Html
    };
    Q_ENUM(InputType)

    static bool isSupported();

    InputType inputType() const;
    void setInputType(InputType inputType);

    QJsonArray extractedData() const;

    void start() override;

private Q_SLOTS:
    void doStart();

private:
    static QString extractorPath();

    InputType m_inputType = InputType::Any;
    QByteArray m_inputData;
    QString m_fileName;

    QJsonArray m_extractedData;
};
