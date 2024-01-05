#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QScopedPointer>
#include <QSettings>
#include <QUuid>
#include <QDebug>
#include <QDir>

#include <QtConcurrent/QtConcurrent>
#include <QFutureInterface>
#include <QFuture>

#include <QJsonDocument>
#include <QJsonObject>

#include <QtHttpServer>
#include <QHostAddress>

#include <QChart>
#include <QChartView>
#include <QValueAxis>
#include <QBarSet>
#include <QBarSeries>
#include <QBarCategoryAxis>

#include <QWidget>
#include <QGridLayout>

#include <functional>

#include "CommonUtilities/CommonUtilities.h"

int main(int argc, char *argv[])
{
    QApplication app {argc, argv};

    QCoreApplication::setApplicationName("Barchart-Microservice");
    QCoreApplication::setApplicationVersion("1.0.0");

    QCommandLineParser commandlineParser;
    commandlineParser.addHelpOption();
    commandlineParser.addVersionOption();
    commandlineParser.setApplicationDescription("Microservice for BarChart-Plotting.");
    commandlineParser.process(app);

    if (!QFile::exists(QApplication::applicationDirPath() + QDir::separator() + "settings.ini"))
        commandlineParser.showHelp(-100);

    const QSettings settings {QApplication::applicationDirPath() + QDir::separator() + "settings.ini", QSettings::Format::IniFormat, &app};

    if (!settings.allKeys().contains(PORT_KEY))
        commandlineParser.showHelp(-101);

    const quint64 port {settings.value(PORT_KEY).toULongLong()};

    if (port > HIGHEST_PORT || port < LOWEST_PORT)
        commandlineParser.showHelp(-102);

    if (!settings.allKeys().contains(IMAGEPATH_KEY))
        commandlineParser.showHelp(-103);

    static const QString imagepath {settings.value(IMAGEPATH_KEY).toString()};

    if (imagepath.isEmpty())
        commandlineParser.showHelp(-104);

    if (!QFile::exists(imagepath))
        commandlineParser.showHelp(-105);

    if (QDir::isRelativePath(imagepath))
        commandlineParser.showHelp(-106);

    const QScopedPointer<QHttpServer> httpServer {new QHttpServer {&app}};

    httpServer->route("/bar/upwards", QHttpServerRequest::Method::Post,
    [](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
    {
        return QtConcurrent::run([&]()
        {
            const QJsonDocument jsonDocument {QJsonDocument::fromJson(request.body())};

            if (jsonDocument.isNull())
            {
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. Please send a valid JSON-Object."}
                    }
                };
            }

            const QJsonObject jsonObject {jsonDocument.object()};

            if (jsonObject.isEmpty())
            {
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. Please send a valid JSON-Object."}
                    }
                };
            }

            for (const QString &key : {"Points", "Categories"})
            {
                if (!jsonObject.contains(key))
                {
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", QString{"Invalid data sent. Missing JSON-Key '%0'. Please send a valid JSON-Object."}.arg(key)}
                        }
                    };
                }
            }

            if (!jsonObject.value("Points").isArray())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'Points' is not an array. Please send a valid JSON-Object."}
                    }
                };

            if (!jsonObject.value("Categories").isArray())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'Categories' is not an array. Please send a valid JSON-Object."}
                    }
                };

            const QJsonArray jsonArrayPoints {jsonObject.value("Points").toArray()};

            if (jsonArrayPoints.isEmpty())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'Points' is empty. Please send a valid JSON-Object."}
                    }
                };

             if (jsonArrayPoints.size() > 1)
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. JSON-Key 'Points' contains more than one array. Please send a valid JSON-Object."}
                    }
                };

            if (jsonArrayPoints.first().isNull())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. Array in JSON-Key 'Points' contains no JSON subobjects. Please send a valid JSON-Object."}
                    }
                };

            for (const QJsonValueConstRef arrayValue : jsonArrayPoints.first().toArray())
            {
                if (!arrayValue.isObject())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. A sub-object in array 'Points' is not a proper JSON-object. Please send a valid JSON-Object."}
                        }
                    };

                const QJsonObject arrayObject {arrayValue.toObject()};

                if (arrayObject.value("Caption").toString().isEmpty())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. A caption of one sub-object in array 'Points' is empty. Please send a valid JSON-Object."}
                        }
                    };

                if (!arrayObject.value("Values").isArray())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. JSON-Key 'Values' of one sub-object in array 'Points' is not an array. Please send a valid JSON-Object."}
                        }
                    };

                if (arrayObject.isEmpty())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. JSON-Key 'Values' of one sub-object in array 'Points' is empty. Please send a valid JSON-Object."}
                        }
                    };

                for (const QJsonValueConstRef subObject : arrayObject.value("Values").toArray())
                {
                    if (!subObject.isDouble())
                        return QHttpServerResponse
                        {
                            QJsonObject
                            {
                                {"Message", "Invalid data sent. A point in JSON-Key 'Values' in one sub-object of 'Points' is not a double value. Please send a valid JSON-Object."}
                            }
                        };
                }

                if (arrayObject.value("Values").toArray().size() != jsonObject.value("Categories").toArray().size())
                        return QHttpServerResponse
                        {
                            QJsonObject
                            {
                                {"Message", QString{"Invalid data sent. The 'Values'-array in JSON-Object with the JSON-Key 'Caption' : '%0' is not of the same size as the 'Categories'-Array. Please send a valid JSON-Object."}.arg(arrayObject.value("Caption").toString())}
                            }
                        };
            }

            for (const QJsonValueConstRef value : jsonObject.value("Categories").toArray())
            {
                if (!value.isString())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. A value in JSON-Key 'Values' in one sub-object of 'Categories' is not a string. Please send a valid JSON-Object."}
                        }
                    };

                if (value.toString().isEmpty())
                    return QHttpServerResponse
                    {
                        QJsonObject
                        {
                            {"Message", "Invalid data sent. A value in JSON-Key 'Values' in one sub-object of 'Categories' is a empty string. Please send a valid JSON-Object."}
                        }
                    };
            }

            const QVector<QJsonObject> pointsObjects {getJSONObjectsVectorFromJSONArray(jsonArrayPoints)};

            const QMap<QString, QVector<qreal> > captionToValues = [](const QVector<QJsonObject> &pointsObjects) -> QMap<QString, QVector<qreal> >
            {
                QMap<QString, QVector<qreal> > captionToValues;

                for (const QJsonObject &object : pointsObjects)
                {
                    const QString caption {object.value("Caption").toString()};
                    const QVector<qreal> values {convertFromArrayToRealsVector(object.value("Values").toArray())};

                    captionToValues.insert(caption, values);
                }

                return captionToValues;

            }(pointsObjects);

            const QStringList categories = [](const QJsonArray &jsonArrayCategories) -> QStringList
            {
                QStringList categories;

                for (const QJsonValueConstRef category : jsonArrayCategories)
                    categories << category.toString();

                return categories.toVector();

            }(jsonObject.value("Categories").toArray());

            const qreal yStart = [](const QMap<QString, QVector<qreal> > &captionToValues) -> qreal
            {
                QVector<qreal> allValues;

                for (const QString &caption : captionToValues.keys())
                    allValues << captionToValues.value(caption);

                const qreal minValue {*std::min_element(allValues.begin(), allValues.end())};

                if (allValues.isEmpty())
                    return 0;

                return minValue < 0 ? minValue : 0;

            }(captionToValues);

            const qreal yEnd = [](const QMap<QString, QVector<qreal> > &captionToValues) -> qreal
            {
                QVector<qreal> allValues;

                for (const QString &caption : captionToValues.keys())
                    allValues << captionToValues.value(caption);

                if (allValues.size() > 1)
                    return *std::max_element(allValues.begin(), allValues.end());

                return 0;

            }(captionToValues);

            const QScopedPointer<QWidget>     chartWidget {new QWidget};
            const QScopedPointer<QChartView>  chartView   {new QChartView};
            const QScopedPointer<QChart>      chart       {new QChart};
            const QScopedPointer<QGridLayout> gridLayout  {new QGridLayout};

            /* axisY und axisX Pointer dürfen nicht deleted
             * werden, da das chart-objekt die Ownership übernimmt */

            QValueAxis * const axisY {new QValueAxis};
            axisY->setTickType(QValueAxis::TickType::TicksFixed);
            axisY->setRange(yStart, yEnd);

            const qreal tickCount {static_cast<qreal>(yEnd + std::abs(yStart) + 1)};
            axisY->setTickCount(static_cast<int>(tickCount) > (tickCount / 10) ? static_cast<int>(std::ceil(static_cast<int>(tickCount / 10) + 1)) : static_cast<int>(tickCount) + 1);

            axisY->applyNiceNumbers();
            axisY->setTruncateLabels(false);
            chart->addAxis(axisY, Qt::AlignLeft);

            QBarCategoryAxis * const axisX {new QBarCategoryAxis};
            axisX->append(categories);
            chart->addAxis(axisX, Qt::AlignBottom);

            for (const QString &caption : captionToValues.keys())
            {
                /* series und barSet Pointer dürfen nicht deleted
                 * werden, da das chart-objekt die Ownership übernimmt */

                QBarSeries * const series {new QBarSeries};
                QBarSet    * const barSet {new QBarSet{caption}};

                for (const qreal &value : captionToValues.value(caption))
                     barSet->append(value);

                series->append(barSet);
                chart->addSeries(series);

                series->attachAxis(axisX);
                series->attachAxis(axisY);
            }

            chartView->setChart(chart.data());
            chartView->setRenderHint(QPainter::Antialiasing);

            gridLayout->addWidget(chartView.data(), 0, 0);
            chartWidget->setLayout(gridLayout.data());
            chartWidget->resize(QSize{1024, 768});

            const QString uuid {QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces)};
            const QString imageFilename {uuid + ".png"};
            chartWidget->grab().save(imagepath + QDir::separator() + imageFilename);

            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Link",    QString{"http://127.0.0.1:50001/bar/result/%0"}.arg(uuid)},
                    {"Message", "The provided url will expire in 24 hours."}
                }
            };
        });
    });

    httpServer->route("/bar", QHttpServerRequest::Method::Get         |
                              QHttpServerRequest::Method::Put     |
                              QHttpServerRequest::Method::Head    |
                              QHttpServerRequest::Method::Trace   |
                              QHttpServerRequest::Method::Patch   |
                              QHttpServerRequest::Method::Delete  |
                              QHttpServerRequest::Method::Options |
                              QHttpServerRequest::Method::Connect |
                              QHttpServerRequest::Method::Unknown,
    [](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
    {
        Q_UNUSED(request)

        return QtConcurrent::run([]()
        {
            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Message", "The used HTTP-Method is not implemented."}
                }
            };
        });
    });

    httpServer->route("/bar/result/<arg>", QHttpServerRequest::Method::Get         |
                                           QHttpServerRequest::Method::Put     |
                                           QHttpServerRequest::Method::Head    |
                                           QHttpServerRequest::Method::Trace   |
                                           QHttpServerRequest::Method::Patch   |
                                           QHttpServerRequest::Method::Delete  |
                                           QHttpServerRequest::Method::Options |
                                           QHttpServerRequest::Method::Connect |
                                           QHttpServerRequest::Method::Unknown,
    [](const QString &argument) -> QFuture<QHttpServerResponse>
    {
        static std::function<QHttpServerResponse(const QString &)> responseFunction = [](const QString &argument)
        {
            //see, if it is a correct uuid
            const QUuid uuid {QUuid::fromString(argument)};

            if (uuid.isNull())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "The submitted argument is not an UUID. Please send a valid UUID."}
                    }
                };

            if (!QFile::exists(imagepath + QDir::separator() + uuid.toString(QUuid::StringFormat::WithoutBraces) + ".png"))
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "The submitted UUID is either not linked to any chart or already expired. Please contact our support via our e-mail %0 ."}
                    }
                };

            QFile imageFile {imagepath + QDir::separator() + uuid.toString(QUuid::StringFormat::WithoutBraces) + ".png"};

            if (!imageFile.open(QFile::OpenModeFlag::ReadOnly))
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "An internal error (errorcode 100) has occured. Please contact our support via our e-mail %0 ."}
                    }
                };

            const QByteArray imageFileBytes {imageFile.readAll()};

            if (imageFileBytes.isEmpty())
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "An internal error (errorcode 101) has occured. Please contact our support via our e-mail %0 ."}
                    }
                };

            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Message", "The 'Data' entry of this JSON-object contains the base64-encoded png-file data of your chart-plot."},
                    {"Data",    QString{QString{imageFileBytes.toBase64()}.toUtf8()}}
                }
            };
        };

        return QtConcurrent::run(responseFunction, argument);
    });

    httpServer->route("/ping", QHttpServerRequest::Method::Get,
    [](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
    {
        Q_UNUSED(request)

        return QtConcurrent::run([]()
        {
            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Message", "Pong."}
                }
            };
        });
    });

    if (httpServer->listen(QHostAddress::LocalHost, static_cast<quint16>(port)) == 0)
        commandlineParser.showHelp(-99);

    qDebug() << QCoreApplication::applicationName() << " is running on port: " << port;
    return app.exec();
}
