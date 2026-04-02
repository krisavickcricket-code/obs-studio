#include "CricNodeFixtureMonitor.hpp"

#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QUrl>

#include <obs.h>
#include <util/base.h>

CricNodeFixtureMonitor *CricNodeFixtureMonitor::instance = nullptr;

CricNodeFixtureMonitor *CricNodeFixtureMonitor::Instance()
{
	if (!instance)
		instance = new CricNodeFixtureMonitor();
	return instance;
}

CricNodeFixtureMonitor::CricNodeFixtureMonitor() : QObject(nullptr)
{
	networkManager = new QNetworkAccessManager(this);

	pollTimer = new QTimer(this);
	pollTimer->setInterval(60000); /* 60 seconds */
	connect(pollTimer, &QTimer::timeout, this, &CricNodeFixtureMonitor::Poll);
}

void CricNodeFixtureMonitor::StartMonitoring(const MonitoredFixture &fixture)
{
	/* Don't add duplicates */
	for (auto &f : monitoredFixtures) {
		if (f.overlayId == fixture.overlayId) {
			f = fixture;
			return;
		}
	}

	monitoredFixtures.push_back(fixture);

	/* Start timer if not already running */
	if (!pollTimer->isActive() && !monitoredFixtures.empty())
		pollTimer->start();

	blog(LOG_INFO,
	     "[CricNode] Started monitoring fixture: %s vs %s (provider=%s, matchId=%s)",
	     fixture.team1.c_str(), fixture.team2.c_str(),
	     fixture.provider.c_str(), fixture.matchId.c_str());
}

void CricNodeFixtureMonitor::StopMonitoring(const std::string &overlayId)
{
	for (auto it = monitoredFixtures.begin(); it != monitoredFixtures.end(); ++it) {
		if (it->overlayId == overlayId) {
			blog(LOG_INFO,
			     "[CricNode] Stopped monitoring fixture for overlay: %s",
			     overlayId.c_str());
			monitoredFixtures.erase(it);
			break;
		}
	}

	if (monitoredFixtures.empty())
		pollTimer->stop();
}

void CricNodeFixtureMonitor::StopAll()
{
	monitoredFixtures.clear();
	pollTimer->stop();
	blog(LOG_INFO, "[CricNode] Stopped all fixture monitoring");
}

bool CricNodeFixtureMonitor::IsMonitoring(const std::string &overlayId) const
{
	for (auto &f : monitoredFixtures) {
		if (f.overlayId == overlayId)
			return true;
	}
	return false;
}

void CricNodeFixtureMonitor::Poll()
{
	QDate today = QDate::currentDate();

	for (auto &fixture : monitoredFixtures) {
		if (fixture.isLive)
			continue;

		/* Skip fixtures whose match date has passed (already played) */
		if (!fixture.matchDate.empty()) {
			QDate matchDate = QDate::fromString(
				QString::fromStdString(fixture.matchDate),
				Qt::ISODate);
			if (!matchDate.isValid()) {
				/* Try common formats: "Apr 1, 2026", "2026-04-01", "01/04/2026" */
				matchDate = QDate::fromString(
					QString::fromStdString(fixture.matchDate),
					"MMM d, yyyy");
			}
			if (!matchDate.isValid()) {
				matchDate = QDate::fromString(
					QString::fromStdString(fixture.matchDate),
					"MM/dd/yyyy");
			}
			if (matchDate.isValid() && matchDate < today) {
				blog(LOG_INFO,
				     "[CricNode] Skipping past fixture: %s vs %s (date=%s)",
				     fixture.team1.c_str(),
				     fixture.team2.c_str(),
				     fixture.matchDate.c_str());
				continue;
			}
		}

		if (fixture.provider == "dcl") {
			PollDcl(fixture);
		} else if (fixture.provider == "cricclubs") {
			PollCricClubs(fixture);
		}
		/* Play-Cricket and PlayHQ matches already have real match IDs,
		 * so they don't need fixture-to-live transition monitoring.
		 * Their scorecard URLs work with the same match ID. */
	}
}

void CricNodeFixtureMonitor::PollDcl(MonitoredFixture &fixture)
{
	QUrl url("https://dallascricket.org:3000/api/getmatchlists");
	QUrlQuery query;
	query.addQueryItem("offset", "0");
	query.addQueryItem("query", QString::fromStdString(fixture.matchId));
	query.addQueryItem("filter", "1");
	query.addQueryItem("isSearchById", "true");
	url.setQuery(query);

	QNetworkRequest request(url);
	request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
	request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
	request.setTransferTimeout(15000);

	/* Store the overlay ID in the request so we can match it in the reply */
	request.setAttribute(QNetworkRequest::User,
			     QString::fromStdString(fixture.overlayId));

	QNetworkReply *reply = networkManager->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		OnDclReply(reply);
	});
}

void CricNodeFixtureMonitor::PollCricClubs(MonitoredFixture &fixture)
{
	/* CricClubs doesn't have a clean REST API for match status.
	 * We check if the scorecard page returns valid data by hitting
	 * the match detail URL pattern. */
	QString url = QString("https://cricclubs.com/viewScorecard.do?matchId=%1&clubId=%2")
			      .arg(QString::fromStdString(fixture.matchId),
			           QString::fromStdString(fixture.clubId));

	QUrl reqUrl(url);
	QNetworkRequest request(reqUrl);
	request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
	request.setTransferTimeout(15000);
	request.setAttribute(QNetworkRequest::User,
			     QString::fromStdString(fixture.overlayId));

	QNetworkReply *reply = networkManager->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		if (reply->error() != QNetworkReply::NoError) {
			reply->deleteLater();
			return;
		}

		QByteArray data = reply->readAll();
		QString overlayId = reply->request()
					    .attribute(QNetworkRequest::User)
					    .toString();
		reply->deleteLater();

		/* Check if the page contains scorecard data (score table) */
		QString html = QString::fromUtf8(data);
		bool hasScorecard = html.contains("scorecardTable") ||
				    html.contains("scorecard-container") ||
				    html.contains("innings-table");

		if (hasScorecard) {
			for (auto &f : monitoredFixtures) {
				if (f.overlayId == overlayId.toStdString() && !f.isLive) {
					f.isLive = true;
					blog(LOG_INFO,
					     "[CricNode] CricClubs fixture went live: %s vs %s",
					     f.team1.c_str(), f.team2.c_str());
					emit FixtureWentLive(
						overlayId,
						QString::fromStdString(f.matchId),
						"cricclubs");
				}
			}
		}
	});
}

void CricNodeFixtureMonitor::OnDclReply(QNetworkReply *reply)
{
	if (reply->error() != QNetworkReply::NoError) {
		reply->deleteLater();
		return;
	}

	QByteArray data = reply->readAll();
	QString overlayId = reply->request()
				    .attribute(QNetworkRequest::User)
				    .toString();
	reply->deleteLater();

	/* Find the monitored fixture for this reply */
	for (auto &fixture : monitoredFixtures) {
		if (fixture.overlayId == overlayId.toStdString()) {
			CheckDclResult(data, fixture);
			break;
		}
	}
}

void CricNodeFixtureMonitor::CheckDclResult(const QByteArray &data,
					     MonitoredFixture &fixture)
{
	QJsonDocument doc = QJsonDocument::fromJson(data);
	QJsonObject root = doc.object();
	if (!root["success"].toBool())
		return;

	QJsonArray matchList = root["matchLists"].toArray();
	for (auto val : matchList) {
		QJsonObject m = val.toObject();
		QString id = QString::number(m["id"].toInt());

		if (id.toStdString() != fixture.matchId)
			continue;

		/* Skip completed matches */
		if (m["is_match_ended"].toBool()) {
			blog(LOG_INFO,
			     "[CricNode] DCL fixture already completed, skipping: %s vs %s",
			     fixture.team1.c_str(),
			     fixture.team2.c_str());
			break;
		}

		/* Verify team names match (case-insensitive contains) */
		QString t1 = m["team1Name"].toString().toLower();
		QString t2 = m["team2Name"].toString().toLower();
		QString ft1 = QString::fromStdString(fixture.team1).toLower();
		QString ft2 = QString::fromStdString(fixture.team2).toLower();

		bool teamsMatch = (t1.contains(ft1) || ft1.contains(t1)) &&
				  (t2.contains(ft2) || ft2.contains(t2));
		/* Also check reversed order */
		if (!teamsMatch)
			teamsMatch = (t1.contains(ft2) || ft2.contains(t1)) &&
				     (t2.contains(ft1) || ft1.contains(t2));

		if (!teamsMatch) {
			blog(LOG_WARNING,
			     "[CricNode] DCL team mismatch: expected '%s vs %s', got '%s vs %s'",
			     fixture.team1.c_str(), fixture.team2.c_str(),
			     t1.toUtf8().constData(), t2.toUtf8().constData());
			break;
		}

		/* Verify match date matches (if available) */
		if (!fixture.matchDate.empty()) {
			QString matchDateStr = m["date"].toString();
			if (!matchDateStr.isEmpty() &&
			    !matchDateStr.contains(
				    QString::fromStdString(fixture.matchDate))) {
				blog(LOG_WARNING,
				     "[CricNode] DCL date mismatch: expected '%s', got '%s'",
				     fixture.matchDate.c_str(),
				     matchDateStr.toUtf8().constData());
				break;
			}
		}

		bool isLive = (m["is_live"].toInt() == 1);
		if (isLive && !fixture.isLive) {
			fixture.isLive = true;
			blog(LOG_INFO,
			     "[CricNode] DCL fixture went live: %s vs %s (matchId=%s)",
			     fixture.team1.c_str(),
			     fixture.team2.c_str(),
			     fixture.matchId.c_str());

			emit FixtureWentLive(
				QString::fromStdString(fixture.overlayId),
				QString::fromStdString(fixture.matchId),
				"dcl");
		}
		break;
	}
}
