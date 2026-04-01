#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>

/* ---- Match Event ---- */

struct CricNodeMatchEvent {
	std::string type;          /* WICKET, BOUNDARY_4/6, SCORE_UPDATE, INNINGS_BREAK, MATCH_RESULT, BALL_UPDATE, STATE_SNAPSHOT */
	int64_t videoTimestampMs;  /* ms from recording start */
	int64_t wallClockMs;       /* UTC epoch ms */
	int innings;               /* 1 or 2 */
	std::string overs;         /* "5.3" */
	std::string score;         /* "145/3" */
	std::map<std::string, std::string> details; /* playerName, dismissalType, etc. */
};

/* ---- CricNode Metadata ---- */

struct CricNodeMetadata {
	int version = 1;
	int appVersionCode = 0;
	std::string appVersionName;
	int64_t recordingStartUtc = 0;
	std::string scorecardProvider; /* cricclubs, dcl, playcricket, playhq, none */
	std::string matchId;
	std::string clubId;
	std::vector<CricNodeMatchEvent> events;
	std::string hmac;

	std::string toJson() const;
	std::string toJsonWithoutHmac() const;
};

/* ---- Match Event Collector ---- */

class CricNodeEventCollector {
public:
	void startCollecting();
	std::vector<CricNodeMatchEvent> stopCollecting();
	void onEventFromJs(const std::string &jsonString);
	void parseOverlayUrl(const std::string &url);

	std::string scorecardProvider = "none";
	std::string matchId;
	std::string clubId;

private:
	std::mutex mutex;
	std::vector<CricNodeMatchEvent> events;
	int64_t recordingStartMs = 0;

	std::string extractParam(const std::string &url, const std::string &param);
};

/* ---- MP4 Metadata Writer ---- */

namespace CricNodeMp4Writer {

/* 16-byte UUID: "CricN0deMetaV01\0" */
static const uint8_t UUID_BYTES[16] = {'C', 'r', 'i', 'c', 'N', '0', 'd', 'e',
					'M', 'e', 't', 'a', 'V', '0', '1', 0};

bool inject(const std::string &filePath, const CricNodeMetadata &metadata);

} // namespace CricNodeMp4Writer
