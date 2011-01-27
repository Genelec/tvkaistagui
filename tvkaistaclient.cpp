#include <QDebug>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QAuthenticator>
#include "cache.h"
#include "channelfeedparser.h"
#include "programmefeedparser.h"
#include "programmetableparser.h"
#include "tvkaistaclient.h"

TvkaistaClient::TvkaistaClient(QObject *parent) :
    QObject(parent), m_networkAccessManager(new QNetworkAccessManager(this)), m_reply(0),
    m_cache(0), m_programmeTableParser(new ProgrammeTableParser)
{
    m_requestedProgramme.id = -1;
    m_requestedStream.id = -1;
    connect(m_networkAccessManager, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)), SLOT(requestAuthenticationRequired(QNetworkReply*, QAuthenticator*)));
}

TvkaistaClient::~TvkaistaClient()
{
    delete m_programmeTableParser;
}

void TvkaistaClient::setCache(Cache *cache)
{
    m_cache = cache;
}

Cache* TvkaistaClient::cache() const
{
    return m_cache;
}

void TvkaistaClient::setUsername(const QString &username)
{
    m_username = username;
}

QString TvkaistaClient::username() const
{
    return m_username;
}

void TvkaistaClient::setPassword(const QString &password)
{
    m_password = password;
}

QString TvkaistaClient::password() const
{
    return m_password;
}

void TvkaistaClient::setCookies(const QByteArray &cookieString)
{
    /* Tyhjennetään evästeet. */
    m_networkAccessManager->setCookieJar(new QNetworkCookieJar());

    QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(cookieString);
    m_networkAccessManager->cookieJar()->setCookiesFromUrl(cookies, QUrl("http://www.tvkaista.fi/"));
}

QByteArray TvkaistaClient::cookies() const
{
    QList<QNetworkCookie> cookies = m_networkAccessManager->cookieJar()->cookiesForUrl(QUrl("http://www.tvkaista.fi/"));
    QByteArray cookieString;
    int count = cookies.size();

    for (int i = 0; i < count; i++) {
        if (!cookieString.isEmpty()) {
            cookieString.append('\n');
        }

        cookieString.append(cookies.at(i).toRawForm(QNetworkCookie::Full));
    }

    return cookieString;
}

void TvkaistaClient::setProxy(const QNetworkProxy &proxy)
{
    m_networkAccessManager->setProxy(proxy);
}

QNetworkProxy TvkaistaClient::proxy() const
{
    return m_networkAccessManager->proxy();
}

void TvkaistaClient::setFormat(int format)
{
    m_format = format;
}

int TvkaistaClient::format() const
{
    return m_format;
}

QString TvkaistaClient::lastError() const
{
    return m_error;
}

bool TvkaistaClient::isValidUsernameAndPassword() const
{
    return !m_username.isEmpty() && !m_password.isEmpty();
}

void TvkaistaClient::sendLoginRequest()
{
    abortRequest();
    QString urlString = "http://www.tvkaista.fi/";
    qDebug() << "GET" << urlString;
    m_reply = m_networkAccessManager->get(QNetworkRequest(QUrl(urlString)));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(requestNetworkError(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(finished()), SLOT(frontPageRequestFinished()));
}

void TvkaistaClient::sendChannelRequest()
{
    abortRequest();
    QString urlString = "http://www.tvkaista.fi/feedbeta/channels/";
    qDebug() << "GET" << urlString;
    QUrl url(urlString);
    QNetworkRequest request(url);
    m_reply = m_networkAccessManager->get(request);
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(requestNetworkError(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(finished()), SLOT(channelRequestFinished()));
}

void TvkaistaClient::sendProgrammeRequest(int channelId, const QDate &date)
{
    abortRequest();
    QString urlString = QString("http://www.tvkaista.fi/recordings/date/%1/%2/")
                        .arg(date.toString("dd/MM/yyyy")).arg(channelId);
    qDebug() << "GET" << urlString;
    m_reply = m_networkAccessManager->get(QNetworkRequest(QUrl(urlString)));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(requestNetworkError(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(readyRead()), SLOT(programmeRequestReadyRead()));
    connect(m_reply, SIGNAL(finished()), SLOT(programmeRequestFinished()));
    m_programmeTableParser->setRequestedDate(date);
    m_programmeTableParser->setRequestedChannelId(channelId);
}

void TvkaistaClient::sendPosterRequest(const Programme &programme)
{
    abortRequest();
    QString urlString = QString("http://www.tvkaista.fi/resources/recordings/screengrabs/%1.jpg").arg(programme.id);
    qDebug() << "GET" << urlString;
    m_reply = m_networkAccessManager->get(QNetworkRequest(QUrl(urlString)));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(requestNetworkError(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(finished()), SLOT(posterRequestFinished()));
    m_requestedProgramme = programme;
}

QNetworkReply* TvkaistaClient::sendDetailedFeedRequest(const Programme &programme)
{
    QString urlString = QString("http://www.tvkaista.fi/feedbeta/programs/%1/detailed.mediarss").arg(programme.id);
    qDebug() << "GET" << urlString;
    QUrl url(urlString);
    QNetworkRequest request(url);
    return m_networkAccessManager->get(request);
}

QNetworkReply* TvkaistaClient::sendRequest(const QUrl &url)
{
    qDebug() << "GET" << url.toString();
    QNetworkRequest request(url);
    return m_networkAccessManager->get(request);
}

QNetworkReply* TvkaistaClient::sendRequestWithAuthHeader(const QUrl &url)
{
    qDebug() << "GET" << url.toString();
    QNetworkRequest request(url);
    return m_networkAccessManager->get(request);
}

void TvkaistaClient::sendStreamRequest(const Programme &programme)
{
    abortRequest();
    QString urlString = QString("http://www.tvkaista.fi/recordings/download/%1/").arg(programme.id);

    switch (m_format) {
    case 0:
        urlString.append("3/300000/");
        break;

    case 1:
        urlString.append("4/1000000/");
        break;

    case 2:
        urlString.append("3/2000000/");
        break;

    default:
        urlString.append("0/8000000/");
    }

    qDebug() << "GET" << urlString;;
    m_reply = m_networkAccessManager->get(QNetworkRequest(QUrl(urlString)));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(requestNetworkError(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(finished()), SLOT(streamRequestFinished()));
    m_requestedStream = programme;
    m_requestedFormat = m_format;
}

void TvkaistaClient::sendSearchRequest(const QString &phrase)
{
    abortRequest();
    QString urlString = QString("http://www.tvkaista.fi/feedbeta/search/title/%1/flv.mediarss").arg(phrase);
    qDebug() << "GET" << urlString;
    QNetworkRequest request = QNetworkRequest(QUrl(urlString));
    m_reply = m_networkAccessManager->get(request);
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(requestNetworkError(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(finished()), SLOT(searchRequestFinished()));
}

void TvkaistaClient::frontPageRequestFinished()
{
    QList<QPair<QString, QString> > items;
    items << QPair<QString, QString>("username", m_username);
    items << QPair<QString, QString>("password", m_password);
    items << QPair<QString, QString>("rememberme", "unlessnot");
    items << QPair<QString, QString>("action", "login");
    QUrl url;
    url.setQueryItems(items);
    QByteArray data = url.toEncoded(QUrl::None);
    data.remove(0, 1);
    QString urlString = "http://www.tvkaista.fi/login/";
    qDebug() << "POST" << urlString;
    m_reply = m_networkAccessManager->post(QNetworkRequest(QUrl(urlString)), data);
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(requestNetworkError(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(finished()), SLOT(loginRequestFinished()));
}

void TvkaistaClient::loginRequestFinished()
{
    QByteArray data = m_reply->readAll();
    bool invalidPassword = data.contains("<form");
    m_lastLogin = QDateTime::currentDateTime();
    m_reply->deleteLater();
    m_reply = 0;

    if (invalidPassword) {
        emit loginError();
    }
    else if (m_programmeTableParser->requestedChannelId() >= 0) {
        sendProgrammeRequest(m_programmeTableParser->requestedChannelId(),
                             m_programmeTableParser->requestedDate());
    }
    else if (m_requestedStream.id >= 0) {
        sendStreamRequest(m_requestedStream);
    }
    else {
        emit loggedIn();
    }
}

void TvkaistaClient::channelRequestFinished()
{
    ChannelFeedParser parser;

    if (!parser.parse(m_reply)) {
        qDebug() << parser.lastError();
        m_reply->deleteLater();
        m_reply = 0;
    }
    else {
        QList<Channel> channels = parser.channels();
        m_cache->saveChannels(channels);
        m_reply->deleteLater();
        m_reply = 0;
        emit channelsFetched(channels);
    }
}

void TvkaistaClient::programmeRequestReadyRead()
{
    m_programmeTableParser->parse(m_reply);
}

void TvkaistaClient::programmeRequestFinished()
{
    if (!checkResponse()) {
        return;
    }

    if (m_programmeTableParser->isValidResults()) {
        QDateTime now = QDateTime::currentDateTime();
        QDate today = now.date();

        for (int i = 0; i < 7; i++) {
            QList<Programme> programmes = m_programmeTableParser->programmes(i);

            if (programmes.isEmpty()) {
                continue;
            }

            QDateTime expireDateTime;

            if (m_programmeTableParser->date(i) == today) {
                expireDateTime = now.addSecs(300);
            }
            else if (m_programmeTableParser->date(i) > today) {
                expireDateTime = QDateTime(m_programmeTableParser->date(i), QTime(0, 0));
            }

            m_cache->saveProgrammes(m_programmeTableParser->requestedChannelId(),
                                    m_programmeTableParser->date(i), now,
                                    expireDateTime, programmes);
        }
    }

    m_reply->deleteLater();
    m_reply = 0;
    emit programmesFetched(m_programmeTableParser->requestedChannelId(),
                           m_programmeTableParser->requestedDate(),
                           m_programmeTableParser->requestedProgrammes());
    m_programmeTableParser->clear();
}

void TvkaistaClient::posterRequestFinished()
{
    if (!checkResponse()) {
        return;
    }

    QByteArray data = m_reply->readAll();
    QImage poster = QImage::fromData(data, "JPEG");

    if (!poster.isNull()) {
        m_cache->savePoster(m_requestedProgramme, data);
        emit posterFetched(m_requestedProgramme, poster);
    }

    m_requestedProgramme.id = -1;
    m_reply->deleteLater();
    m_reply = 0;
}

void TvkaistaClient::streamRequestFinished()
{
    if (m_reply == 0) {
        return;
    }

    if (m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 302) {
        emit streamUrlFetched(m_requestedStream, m_requestedFormat,
                              m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
    }
}

void TvkaistaClient::searchRequestFinished()
{
    ProgrammeFeedParser parser;

    if (!parser.parse(m_reply)) {
        qWarning() << parser.lastError();
    }

    m_reply->deleteLater();
    m_reply = 0;
    emit searchResultsFetched(parser.programmes());
}

void TvkaistaClient::requestNetworkError(QNetworkReply::NetworkError error)
{
    if (error == QNetworkReply::OperationCanceledError || m_reply == 0) {
        return;
    }

    if (error == QNetworkReply::AuthenticationRequiredError) {
        m_networkError = 1;
    }
    else if (m_requestedStream.id >= 0 && error == QNetworkReply::ContentNotFoundError) {
        m_networkError = 2;
    }
    else if (error == QNetworkReply::ContentNotFoundError) {
        return;
    }
    else {
        m_networkError = 0;
        m_error = networkErrorString(error);
    }

    abortRequest();

    /* http://bugreports.qt.nokia.com/browse/QTBUG-16333 */
    QTimer::singleShot(0, this, SLOT(handleNetworkError()));
}

void TvkaistaClient::handleNetworkError()
{
    if (m_networkError == 1) {
        emit loginError();
    }
    else if (m_networkError == 2) {
        emit streamNotFound();
    }
    else {
        emit networkError();
    }
}

void TvkaistaClient::requestAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    Q_UNUSED(reply);
    qDebug() << "AUTH" << m_username;

    if (authenticator->user() != m_username || authenticator->password() != m_password) {
        authenticator->setUser(m_username);
        authenticator->setPassword(m_password);
    }
}

void TvkaistaClient::abortRequest()
{
    QNetworkReply *reply = m_reply;

    if (reply != 0) {
        m_reply = 0;
        reply->abort();
        reply->deleteLater();
    }

    m_requestedProgramme.id = -1;
    m_requestedStream.id = -1;
}

bool TvkaistaClient::checkResponse()
{
    if (m_reply == 0) {
        return false;
    }

    if (m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 302) {
        QDateTime now = QDateTime::currentDateTime();

        if (m_lastLogin.isNull() || m_lastLogin < now.addSecs(-5)) {
            sendLoginRequest();
            return false;
        }
    }

    return true;
}

QString TvkaistaClient::networkErrorString(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
        return "ConnectionRefused";
    case QNetworkReply::RemoteHostClosedError:
        return "RemoteHostClosed";
    case QNetworkReply::HostNotFoundError:
        return "HostNotFound";
    case QNetworkReply::TimeoutError:
        return "Timeout";
    case QNetworkReply::OperationCanceledError:
        return "OperationCanceled";
    case QNetworkReply::SslHandshakeFailedError:
        return "SslHandshakeFailed";
    case QNetworkReply::ContentAccessDenied:
        return "ContentAccessDenied";
    case QNetworkReply::ContentOperationNotPermittedError:
        return "ContentOperationNotPermitted";
    case QNetworkReply::ContentNotFoundError:
        return "ContentNotFound";
    case QNetworkReply::AuthenticationRequiredError:
        return "AuthenticationRequired";
    default:
        return "UnknownNetworkError";
    }
}
