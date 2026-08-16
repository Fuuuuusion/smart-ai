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
            *error = networkError == QNetworkReply::OperationCanceledError ? QStringLiteral("工具请求超时。") : errorString;
        return {};
    }
    if (status >= 400) {
        if (error)
            *error = QStringLiteral("工具请求失败，HTTP 状态码 %1。").arg(status);
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
            *error = networkError == QNetworkReply::OperationCanceledError ? QStringLiteral("工具请求超时。") : errorString;
        return {};
    }
    if (status >= 400) {
        if (error)
            *error = QStringLiteral("工具请求失败，HTTP 状态码 %1。").arg(status);
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
        return QStringLiteral("晴");
    case 1:
    case 2:
        return QStringLiteral("多云");
    case 3:
        return QStringLiteral("阴天");
    case 45:
    case 48:
        return QStringLiteral("雾");
    case 51:
    case 53:
    case 55:
        return QStringLiteral("毛毛雨");
    case 61:
    case 63:
    case 65:
        return QStringLiteral("雨");
    case 71:
    case 73:
    case 75:
        return QStringLiteral("雪");
    case 80:
    case 81:
    case 82:
        return QStringLiteral("阵雨");
    case 95:
    case 96:
    case 99:
        return QStringLiteral("雷暴");
    default:
        return QStringLiteral("未知");
    }
}
}

QJsonArray ToolRegistry::toolDefinitions()
{
    QJsonArray tools;

    QJsonObject calculatorProperties;
    calculatorProperties.insert(QStringLiteral("expression"), property(QStringLiteral("string"), QStringLiteral("数学表达式，例如：(3+5)*2 或 sqrt(16)")));
    tools.append(functionObject(QStringLiteral("calculator"),
                                QStringLiteral("计算安全数学表达式。支持 + - * / ^ 括号以及 sin/cos/tan/sqrt/log/abs。"),
                                stringParameters(calculatorProperties, { QStringLiteral("expression") })));

    QJsonObject weatherProperties;
    weatherProperties.insert(QStringLiteral("location"), property(QStringLiteral("string"), QStringLiteral("城市名称，例如：北京或上海")));
    tools.append(functionObject(QStringLiteral("weather"),
                                QStringLiteral("使用 Open-Meteo 查询指定城市的当前天气。"),
                                stringParameters(weatherProperties, { QStringLiteral("location") })));

    QJsonObject searchProperties;
    searchProperties.insert(QStringLiteral("query"), property(QStringLiteral("string"), QStringLiteral("网络搜索关键词")));
    tools.append(functionObject(QStringLiteral("web_search"),
                                QStringLiteral("搜索网络并返回简洁的结果摘要。"),
                                stringParameters(searchProperties, { QStringLiteral("query") })));

    QJsonObject knowledgeProperties;
    knowledgeProperties.insert(QStringLiteral("query"), property(QStringLiteral("string"), QStringLiteral("需要在本地知识库中检索的问题")));
    tools.append(functionObject(QStringLiteral("knowledge_search"),
                                QStringLiteral("从本地知识库中检索相关的私有文档片段。"),
                                stringParameters(knowledgeProperties, { QStringLiteral("query") })));

    QJsonObject timeProperties;
    timeProperties.insert(QStringLiteral("timezone"), property(QStringLiteral("string"), QStringLiteral("可选 IANA 时区，例如 Asia/Shanghai")));
    tools.append(functionObject(QStringLiteral("current_time"),
                                QStringLiteral("获取当前日期和时间。"),
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
        result.content = QStringLiteral("当前时间：%1\n时间戳（秒）：%2")
                             .arg(now.toString(Qt::ISODateWithMs))
                             .arg(now.toSecsSinceEpoch());
        if (!timezone.isEmpty())
            result.content += QStringLiteral("\n请求时区：%1").arg(timezone);
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
                *error = httpError.isEmpty() ? QStringLiteral("未找到该地点。") : httpError;
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
        result.content = QStringLiteral("%1 当前天气：\n温度：%2°C\n体感温度：%3°C\n湿度：%4%%\n风速：%5 km/h\n天气状况：%6")
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
            result.content = QStringLiteral("没有找到有用的搜索结果。");
            result.ok = true;
        } else {
            result.content = lines.join('\n');
            result.ok = true;
        }
        return result;
    }

    if (name == QStringLiteral("knowledge_search")) {
        if (!context.knowledgeStore) {
            result.content = QStringLiteral("本地知识库不可用。");
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
            result.content = QStringLiteral("未找到相关知识库片段。");
            result.ok = true;
            return result;
        }
        QStringList lines;
        for (const SearchHit &hit : hits) {
            lines.append(QStringLiteral("[%1 · 分块 %2 · 相似度 %3]\n%4")
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
        *error = QStringLiteral("未知工具：%1").arg(name);
    return result;
}
