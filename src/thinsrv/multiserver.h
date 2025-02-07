// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_MULTISERVER_H
#define DP_MULTISERVER_H
#include "libserver/jsonapi.h"
#include "libserver/sslserver.h"
#include <QObject>
#include <QHostAddress>
#include <QDateTime>

class QDir;
class QTcpServer;
#ifdef HAVE_WEBSOCKETS
class QWebSocketServer;
#endif

namespace server {

class Client;
class ExtBans;
class Session;
class SessionServer;
class ServerConfig;
class ThinServerClient;

/**
 * The drawpile server.
 */
class MultiServer final : public QObject {
Q_OBJECT
public:
	explicit MultiServer(ServerConfig *config, QObject *parent = nullptr);

	void setSslCertFile(
		const QString &certfile, const QString &keyfile,
		SslServer::Algorithm keyAlgorithm)
	{
		m_sslCertFile = certfile;
		m_sslKeyFile = keyfile;
		m_sslKeyAlgorithm = keyAlgorithm;
	}

	void setAutoStop(bool autostop);
	void setRecordingPath(const QString &path);
	void setTemplateDirectory(const QDir &dir);

	/**
	 * @brief Get the port the server is running from
	 * @return port number or zero if server is not running
	 */
	int port() const { return m_port; }

	Q_INVOKABLE bool isRunning() const { return m_state != STOPPED; }

	//! Start the server with the given descriptors, -1 to disable WebSocket
	bool startFd(int tcpFd, int webSocketFd);

	SessionServer *sessionServer() { return m_sessions; }

	ServerConfig *config() { return m_config; }

public slots:
	void setSessionDirectory(const QDir &dir);

	//! Start the server on the given port and listening address
	bool start(
		quint16 tcpPort, const QHostAddress &tcpAddress = QHostAddress::Any,
		quint16 webSocketPort = 0,
		const QHostAddress &webSocketAddress = QHostAddress::Any);

	 //! Stop the server. All clients are disconnected.
	void stop();

	/**
	 * @brief Call the server's JSON administration API
	 *
	 * This is used by the HTTP admin API.
	 *
	 * @param method query method
	 * @param path path components
	 * @param request request body content
	 * @return JSON API response content
	 */
	JsonApiResult callJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);

	/**
	 * @brief Like callJsonApi(), but the jsonApiResponse signal will be emitted instead of the return value
	 */
	void callJsonApiAsync(const QString &requestId, JsonApiMethod method, const QStringList &path, const QJsonObject &request);

private slots:
	void newTcpClient();
#ifdef HAVE_WEBSOCKETS
	void newWebSocketClient();
#endif
	void printStatusUpdate();
	void tryAutoStop();
	void assignRecording(Session *session);

signals:
	void serverStartError(const QString &message);
	void serverStarted();
	void serverStopped();
	void jsonApiResult(const QString &serverId, const JsonApiResult &result);
	void userCountChanged(int count);

private:
	bool createServer();
	bool createServer(bool enableWebSockets);
	bool abortStart();

	void newClient(ThinServerClient *client);

	JsonApiResult serverJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult statusJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult banlistJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult systembansJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult userbansJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult listserverWhitelistJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult accountsJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult logJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);
	JsonApiResult extbansJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request);

	enum State {RUNNING, STOPPING, STOPPED};

	ServerConfig *m_config;
	QTcpServer *m_tcpServer;
#ifdef HAVE_WEBSOCKETS
	QWebSocketServer *m_webSocketServer;
#endif
	SessionServer *m_sessions;
	ExtBans *m_extBans;

	State m_state;

	bool m_autoStop;
	int m_port;

	QString m_sslCertFile;
	QString m_sslKeyFile;
	SslServer::Algorithm m_sslKeyAlgorithm;
	QString m_recordingPath;

	QDateTime m_started;
};

}

#endif

