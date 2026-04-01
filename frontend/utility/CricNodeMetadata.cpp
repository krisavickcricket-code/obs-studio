#include "CricNodeMetadata.hpp"

#include <obs.h>
#include <util/platform.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QBuffer>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QCoreApplication>

#include <cstring>
#include <chrono>
#include <regex>

/* ---- JSON serialization ---- */

static QJsonObject eventToJson(const CricNodeMatchEvent &e)
{
	QJsonObject obj;
	obj["type"] = QString::fromStdString(e.type);
	obj["videoTimestampMs"] = (qint64)e.videoTimestampMs;
	obj["wallClockMs"] = (qint64)e.wallClockMs;
	obj["innings"] = e.innings;
	obj["overs"] = QString::fromStdString(e.overs);
	obj["score"] = QString::fromStdString(e.score);
	if (!e.details.empty()) {
		QJsonObject det;
		for (auto &kv : e.details)
			det[QString::fromStdString(kv.first)] = QString::fromStdString(kv.second);
		obj["details"] = det;
	}
	return obj;
}

static std::string metadataToJsonString(const CricNodeMetadata &m, bool includeHmac)
{
	QJsonObject obj;
	obj["version"] = m.version;
	obj["appVersionCode"] = m.appVersionCode;
	obj["appVersionName"] = QString::fromStdString(m.appVersionName);
	obj["recordingStartUtc"] = (qint64)m.recordingStartUtc;
	obj["scorecardProvider"] = QString::fromStdString(m.scorecardProvider);
	obj["matchId"] = QString::fromStdString(m.matchId);
	if (!m.clubId.empty())
		obj["clubId"] = QString::fromStdString(m.clubId);

	QJsonArray evts;
	for (auto &e : m.events)
		evts.append(eventToJson(e));
	obj["events"] = evts;

	obj["hmac"] = includeHmac ? QString::fromStdString(m.hmac) : QString("");

	QJsonDocument doc(obj);
	return doc.toJson(QJsonDocument::Compact).toStdString();
}

std::string CricNodeMetadata::toJson() const
{
	return metadataToJsonString(*this, true);
}

std::string CricNodeMetadata::toJsonWithoutHmac() const
{
	return metadataToJsonString(*this, false);
}

/* ---- HMAC computation ---- */

static std::string computeHmac(const CricNodeMetadata &metadata)
{
	/* For desktop, use a fixed signing key derived from the app name.
	 * This matches the cloud verification for desktop builds. */
	std::string appId = "com.cricnode.pc";
	QByteArray keyData = QCryptographicHash::hash(appId.c_str(), QCryptographicHash::Sha256);
	keyData.append(appId.c_str());

	std::string payload = metadata.toJsonWithoutHmac();
	QMessageAuthenticationCode mac(QCryptographicHash::Sha256, keyData);
	mac.addData(payload.c_str(), (int)payload.size());
	QByteArray result = mac.result();

	/* Convert to hex string */
	return result.toHex().toStdString();
}

/* ---- Match Event Collector ---- */

void CricNodeEventCollector::startCollecting()
{
	std::lock_guard<std::mutex> lock(mutex);
	events.clear();
	recordingStartMs =
		std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();
	blog(LOG_INFO, "CricNode: Started collecting events (provider=%s, matchId=%s)",
	     scorecardProvider.c_str(), matchId.c_str());
}

std::vector<CricNodeMatchEvent> CricNodeEventCollector::stopCollecting()
{
	std::lock_guard<std::mutex> lock(mutex);
	auto result = std::move(events);
	events.clear();
	blog(LOG_INFO, "CricNode: Stopped collecting: %zu events", result.size());
	return result;
}

void CricNodeEventCollector::onEventFromJs(const std::string &jsonString)
{
	QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonString));
	if (!doc.isObject())
		return;

	QJsonObject json = doc.object();
	std::string type = json["type"].toString().toStdString();
	if (type.empty())
		return;

	int64_t jsTimestamp = (int64_t)json["timestamp"].toDouble(0);
	if (jsTimestamp == 0) {
		jsTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
				     std::chrono::system_clock::now().time_since_epoch())
				     .count();
	}

	int64_t videoRelativeMs = 0;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (recordingStartMs > 0)
			videoRelativeMs = std::max((int64_t)0, jsTimestamp - recordingStartMs);
	}

	CricNodeMatchEvent event;
	event.type = type;
	event.videoTimestampMs = videoRelativeMs;
	event.wallClockMs = jsTimestamp;
	event.innings = json["innings"].toInt(1);
	event.overs = json["overs"].toString().toStdString();
	event.score = json["score"].toString().toStdString();

	QJsonObject details = json["details"].toObject();
	for (auto it = details.begin(); it != details.end(); ++it)
		event.details[it.key().toStdString()] = it.value().toString().toStdString();

	{
		std::lock_guard<std::mutex> lock(mutex);
		events.push_back(event);
	}

	blog(LOG_DEBUG, "CricNode: Event: %s at %lldms (innings=%d, score=%s)", type.c_str(),
	     (long long)videoRelativeMs, event.innings, event.score.c_str());
}

void CricNodeEventCollector::parseOverlayUrl(const std::string &url)
{
	if (url.find("cricclubs_scorecard") != std::string::npos) {
		scorecardProvider = "cricclubs";
		matchId = extractParam(url, "matchId");
		clubId = extractParam(url, "clubId");
	} else if (url.find("dcl_scorecard") != std::string::npos) {
		scorecardProvider = "dcl";
		matchId = extractParam(url, "matchId");
	} else if (url.find("playcricket_scorecard") != std::string::npos) {
		scorecardProvider = "playcricket";
		matchId = extractParam(url, "matchId");
	} else if (url.find("playhq_scorecard") != std::string::npos) {
		scorecardProvider = "playhq";
		matchId = extractParam(url, "gameId");
	} else {
		scorecardProvider = "none";
		matchId = "";
		clubId = "";
	}
	blog(LOG_INFO, "CricNode: Parsed URL: provider=%s, matchId=%s", scorecardProvider.c_str(), matchId.c_str());
}

std::string CricNodeEventCollector::extractParam(const std::string &url, const std::string &param)
{
	std::regex re("[?&]" + param + "=([^&]+)");
	std::smatch match;
	if (std::regex_search(url, match, re) && match.size() > 1)
		return match[1].str();
	return "";
}

/* ---- MP4 Metadata Writer ---- */

bool CricNodeMp4Writer::inject(const std::string &filePath, const CricNodeMetadata &metadata)
{
	try {
		/* Compute HMAC and create signed metadata */
		CricNodeMetadata signed_meta = metadata;
		signed_meta.hmac = computeHmac(metadata);

		/* Serialize to JSON */
		std::string jsonStr = signed_meta.toJson();
		QByteArray jsonBytes = QByteArray::fromStdString(jsonStr);

		/* Gzip compress */
		QByteArray compressed = qCompress(jsonBytes, 9);
		/* qCompress adds a 4-byte header (uncompressed size) that gzip doesn't have.
		 * For compatibility with the Android reader which uses standard gzip,
		 * we store the qCompress output as-is since the cloud reader handles both formats. */

		/* Build UUID box: 4 (size) + 4 ("uuid") + 16 (UUID) + N (payload) */
		uint32_t boxSize = (uint32_t)(4 + 4 + 16 + compressed.size());

		/* Big-endian size */
		uint8_t sizeBytes[4];
		sizeBytes[0] = (boxSize >> 24) & 0xFF;
		sizeBytes[1] = (boxSize >> 16) & 0xFF;
		sizeBytes[2] = (boxSize >> 8) & 0xFF;
		sizeBytes[3] = boxSize & 0xFF;

		uint8_t boxType[4] = {'u', 'u', 'i', 'd'};

		/* Append to file */
		QFile file(QString::fromStdString(filePath));
		if (!file.open(QIODevice::Append))
			return false;

		file.write((const char *)sizeBytes, 4);
		file.write((const char *)boxType, 4);
		file.write((const char *)UUID_BYTES, 16);
		file.write(compressed);
		file.close();

		blog(LOG_INFO, "CricNode: Injected metadata: %d bytes compressed, %zu events, provider=%s",
		     compressed.size(), metadata.events.size(), metadata.scorecardProvider.c_str());
		return true;
	} catch (...) {
		blog(LOG_WARNING, "CricNode: Failed to inject metadata into %s", filePath.c_str());
		return false;
	}
}
