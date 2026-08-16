#include "ToolRegistry.h"

#include "KnowledgeStore.h"
#include "MathParser.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QStringList>

namespace {
QJsonObject functionObject(const QString &name,
                           const QString &description,
                           const QJsonObject &parameters)
{
    QJsonObject function;
    function.insert(QStringLiteral("name"), name);
    function.insert(QStringLiteral("description"), description);
    function.insert(QStringLiteral("parameters"), parameters);
    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("function"));
    object.insert(QStringLiteral("function"), function);
    return object;
}

QJsonObject stringParameters(const QJsonObject &properties, const QStringList &required)
{
    QJsonObject parameters;
    parameters.insert(QStringLiteral("type"), QStringLiteral("object"));
    parameters.insert(QStringLiteral("properties"), properties);
    QJsonArray requiredArray;
    for (const QString &item : required)
        requiredArray.append(item);
    parameters.insert(QStringLiteral("required"), requiredArray);
    return parameters;
}

QJsonObject property(const QString &type, const QString &description)
{
    QJsonObject object;
    object.insert(QStringLiteral("type"), type);
    object.insert(QStringLiteral("description"), description);
    return object;
}

QJsonObject httpGetJson(const QUrl &url, QString *error)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SmartAI/1.0"));
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&loop, reply]() {
        reply->abort();
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, [&loop]() {
        loop.quit();
    });
    timer.start(15000);
    loop.exec();
    timer.stop();

    const QByteArray data = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString errorString = reply->errorString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (networkError != QNetworkReply::NoError) {
        if (error)
            *error = networkError == QNetworkReply::OperationCanceledError ? QStringLiteral("Tool request timed out.") : errorString;
        return {};
    }
    if (status >= 400) {
        if (error)
            *error = QStringLiteral("Tool request failed with HTTP %1.").arg(status);
        return {};
    }
    return QJsonDocument::fromJson(data).object();
}

QByteArray httpGetBytes(const QUrl &url, QString *error)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 SmartAI/1.0"));
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&loop, reply]() {
        reply->abort();
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, [&loop]() {
        loop.quit();
    });
    timer.start(15000);
    loop.exec();
    timer.stop();

    const QByteArray data = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString errorString = reply->errorString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (networkError != QNetworkReply::NoError) {
        if (error)
            *error = networkError == QNetworkReply::OperationCanceledError ? QStringLiteral("Tool request timed out.") : errorString;
        return {};
    }
    if (status >= 400) {
        if (error)
            *error = QStringLiteral("Tool request failed with HTTP %1.").arg(status);
        return {};
    }
    return data;
}

QString plainTextFromHtml(const QString &html)
{
    QTextDocument document;
    document.setHtml(html);
    return document.toPlainText().simplified();
}

QString weatherCodeDescription(int code)
{
    switch (code) {
    case 0:
        return QStringLiteral("Clear sky");
    case 1:
    case 2:
        return QStringLiteral("Partly cloudy");
    case 3:
        return QStringLiteral("Overcast");
    case 45:
    case 48:
        return QStringLiteral("Fog");
    case 51:
    case 53:
    case 55:
        return QStringLiteral("Drizzle");
    case 61:
    case 63:
    case 65:
        return QStringLiteral("Rain");
    case 71:
    case 73:
    case 75:
        return QStringLiteral("Snow");
    case 80:
    case 81:
    case 82:
        return QStringLiteral("Rain showers");
    case 95:
    case 96:
    case 99:
        return QStringLiteral("Thunderstorm");
    default:
        return QStringLiteral("Unknown");
    }
}
}

QJsonArray ToolRegistry::toolDefinitions()
{
    QJsonArray tools;

    QJsonObject calculatorProperties;
    calculatorProperties.insert(QStringLiteral("expression"), property(QStringLiteral("string"), QStringLiteral("Mathematical expression, for example: (3+5)*2 or sqrt(16)")));
    tools.append(functionObject(QStringLiteral("calculator"),
                                QStringLiteral("Evaluate a safe mathematical expression. Supports + - * / ^ parentheses and sin/cos/tan/sqrt/log/abs."),
                                stringParameters(calculatorProperties, { QStringLiteral("expression") })));

    QJsonObject weatherProperties;
    weatherProperties.insert(QStringLiteral("location"), property(QStringLiteral("string"), QStringLiteral("City name, for example: Beijing or Shanghai")));
    tools.append(functionObject(QStringLiteral("weather"),
                                QStringLiteral("Get current weather for a city using Open-Meteo."),
                                stringParameters(weatherProperties, { QStringLiteral("location") })));

    QJsonObject searchProperties;
    searchProperties.insert(QStringLiteral("query"), property(QStringLiteral("string"), QStringLiteral("Web search query")));
    tools.append(functionObject(QStringLiteral("web_search"),
                                QStringLiteral("Search the web and return concise result summaries."),
                                stringParameters(searchProperties, { QStringLiteral("query") })));

    QJsonObject knowledgeProperties;
    knowledgeProperties.insert(QStringLiteral("query"), property(QStringLiteral("string"), QStringLiteral("Question to search in the local knowledge base")));
    tools.append(functionObject(QStringLiteral("knowledge_search"),
                                QStringLiteral("Retrieve relevant private documents from the local knowledge base."),
                                stringParameters(knowledgeProperties, { QStringLiteral("query") })));

    QJsonObject timeProperties;
    timeProperties.insert(QStringLiteral("timezone"), property(QStringLiteral("string"), QStringLiteral("Optional IANA timezone, for example Asia/Shanghai")));
    tools.append(functionObject(QStringLiteral("current_time"),
                                QStringLiteral("Get the current date and time."),
                                stringParameters(timeProperties, {})));

    return tools;
}

ToolResult ToolRegistry::execute(const QString &name,
                                 const QJsonObject &arguments,
                                 const ToolContext &context,
                                 QString *error)
{
    ToolResult result;
    if (name == QStringLiteral("calculator")) {
        const QString expression = arguments.value(QStringLiteral("expression")).toString();
        double value = 0.0;
        QString mathError;
        if (!MathParser::evaluate(expression, &value, &mathError)) {
            result.content = mathError;
            result.ok = false;
        } else {
            result.content = QString::number(value, 'g', 12);
            result.ok = true;
        }
        return result;
    }

    if (name == QStringLiteral("current_time")) {
        QString timezone = arguments.value(QStringLiteral("timezone")).toString();
        QDateTime now = timezone.isEmpty() ? QDateTime::currentDateTime() : QDateTime::currentDateTime();
        result.content = QStringLiteral("Current time: %1\nEpoch seconds: %2")
                             .arg(now.toString(Qt::ISODateWithMs))
                             .arg(now.toSecsSinceEpoch());
        if (!timezone.isEmpty())
            result.content += QStringLiteral("\nRequested timezone: %1").arg(timezone);
        result.ok = true;
        return result;
    }

    if (name == QStringLiteral("weather")) {
        const QString location = arguments.value(QStringLiteral("location")).toString();
        QUrl geocodeUrl(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
        QUrlQuery geocodeQuery;
        geocodeQuery.addQueryItem(QStringLiteral("name"), location);
        geocodeQuery.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
        geocodeQuery.addQueryItem(QStringLiteral("language"), QStringLiteral("en"));
        geocodeQuery.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
        geocodeUrl.setQuery(geocodeQuery);

        QString httpError;
        const QJsonObject geocode = httpGetJson(geocodeUrl, &httpError);
        if (geocode.isEmpty() || !geocode.contains(QStringLiteral("results")) || geocode.value(QStringLiteral("results")).toArray().isEmpty()) {
            if (error)
                *error = httpError.isEmpty() ? QStringLiteral("Location not found.") : httpError;
            return result;
        }
        const QJsonObject place = geocode.value(QStringLiteral("results")).toArray().first().toObject();
        const double latitude = place.value(QStringLiteral("latitude")).toDouble();
        const double longitude = place.value(QStringLiteral("longitude")).toDouble();
        const QString placeName = place.value(QStringLiteral("name")).toString();

        QUrl weatherUrl(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
        QUrlQuery weatherQuery;
        weatherQuery.addQueryItem(QStringLiteral("latitude"), QString::number(latitude, 'f', 6));
        weatherQuery.addQueryItem(QStringLiteral("longitude"), QString::number(longitude, 'f', 6));
        weatherQuery.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"));
        weatherQuery.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
        weatherUrl.setQuery(weatherQuery);

        const QJsonObject weather = httpGetJson(weatherUrl, &httpError);
        if (weather.isEmpty()) {
            if (error)
                *error = httpError;
            return result;
        }
        const QJsonObject current = weather.value(QStringLiteral("current")).toObject();
        const int code = current.value(QStringLiteral("weather_code")).toInt();
        result.content = QStringLiteral("%1 weather:\nTemperature: %2°C\nFeels like: %3°C\nHumidity: %4%%\nWind speed: %5 km/h\nCondition: %6")
                             .arg(placeName)
                             .arg(current.value(QStringLiteral("temperature_2m")).toDouble())
                             .arg(current.value(QStringLiteral("apparent_temperature")).toDouble())
                             .arg(current.value(QStringLiteral("relative_humidity_2m")).toInt())
                             .arg(current.value(QStringLiteral("wind_speed_10m")).toDouble())
                             .arg(weatherCodeDescription(code));
        result.ok = true;
        return result;
    }

    if (name == QStringLiteral("web_search")) {
        const QString query = arguments.value(QStringLiteral("query")).toString();
        QUrl url(QStringLiteral("https://html.duckduckgo.com/html/"));
        QUrlQuery urlQuery;
        urlQuery.addQueryItem(QStringLiteral("q"), query);
        url.setQuery(urlQuery);

        QString httpError;
        const QByteArray html = httpGetBytes(url, &httpError);
        if (html.isEmpty()) {
            if (error)
                *error = httpError;
            return result;
        }

        const QString htmlString = QString::fromUtf8(html);
        QRegularExpression resultRegex(QStringLiteral("<a[^>]+class=\"result__a\"[^>]+href=\"([^\"]+)\"[^>]*>(.*?)</a>"),
                                       QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QRegularExpression snippetRegex(QStringLiteral("<a[^>]+class=\"result__snippet\"[^>]*>(.*?)</a>"),
                                        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QStringList lines;
        auto resultIt = resultRegex.globalMatch(htmlString);
        auto snippetIt = snippetRegex.globalMatch(htmlString);
        int count = 0;
        while (resultIt.hasNext() && count < 5) {
            const QRegularExpressionMatch match = resultIt.next();
            QString title = plainTextFromHtml(match.captured(2));
            QString link = match.captured(1);
            QString snippet;
            if (snippetIt.hasNext()) {
                const QRegularExpressionMatch snippetMatch = snippetIt.next();
                snippet = plainTextFromHtml(snippetMatch.captured(1));
            }
            lines.append(QStringLiteral("%1. %2\n   %3\n   %4").arg(++count).arg(title, link, snippet));
        }
        if (lines.isEmpty()) {
            result.content = QStringLiteral("No useful search results were returned.");
            result.ok = true;
        } else {
            result.content = lines.join('\n');
            result.ok = true;
        }
        return result;
    }

    if (name == QStringLiteral("knowledge_search")) {
        if (!context.knowledgeStore) {
            result.content = QStringLiteral("The local knowledge base is unavailable.");
            result.ok = false;
            return result;
        }
        QVector<double> queryVector = context.knowledgeQueryVector;
        QString storeError;
        const QList<SearchHit> hits = context.knowledgeStore->search(queryVector, context.topK, &storeError);
        if (!storeError.isEmpty()) {
            if (error)
                *error = storeError;
            return result;
        }
        if (hits.isEmpty()) {
            result.content = QStringLiteral("No relevant knowledge base chunks were found.");
            result.ok = true;
            return result;
        }
        QStringList lines;
        for (const SearchHit &hit : hits) {
            lines.append(QStringLiteral("[%1 · chunk %2 · score %3]\n%4")
                             .arg(hit.documentTitle)
                             .arg(hit.chunkIndex + 1)
                             .arg(hit.score, 0, 'f', 3)
                             .arg(hit.text));
        }
        result.content = lines.join(QStringLiteral("\n\n"));
        result.ok = true;
        return result;
    }

    if (error)
        *error = QStringLiteral("Unknown tool: %1").arg(name);
    return result;
}
