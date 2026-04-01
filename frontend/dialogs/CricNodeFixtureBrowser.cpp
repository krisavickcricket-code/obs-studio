#include "CricNodeFixtureBrowser.hpp"

#include <ui-config.h>
#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
extern QCef *cef;
#endif

#include <widgets/OBSBasic.hpp>
#include <util/base.h>

#include <QDate>
#include <QDesktopServices>
#include <QHeaderView>
#include <QMessageBox>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

CricNodeFixtureBrowser::CricNodeFixtureBrowser(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Browse Fixtures");
	setMinimumSize(750, 550);

	auto *layout = new QVBoxLayout(this);

	/* Provider selection */
	auto *providerRow = new QHBoxLayout();
	providerRow->addWidget(new QLabel("Provider:"));
	providerCombo = new QComboBox();
	providerCombo->addItem("CricClubs", "cricclubs");
	providerCombo->addItem("DCL (Dallas Cricket League)", "dcl");
	providerCombo->addItem("Play-Cricket", "playcricket");
	providerCombo->addItem("PlayHQ", "playhq");
	providerRow->addWidget(providerCombo);
	layout->addLayout(providerRow);

	connect(providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&CricNodeFixtureBrowser::OnProviderChanged);

	/* ID input (club ID, site ID, grade ID, etc.) */
	auto *idRow = new QHBoxLayout();
	idLabel = new QLabel("Club ID:");
	idRow->addWidget(idLabel);
	idInput = new QLineEdit();
	idInput->setPlaceholderText("Enter ID...");
	idRow->addWidget(idInput);
	layout->addLayout(idRow);

	/* Token input (for Play-Cricket and PlayHQ) */
	auto *tokenRow = new QHBoxLayout();
	tokenLabel = new QLabel("API Token:");
	tokenRow->addWidget(tokenLabel);
	tokenInput = new QLineEdit();
	tokenInput->setPlaceholderText("Enter API token...");
	tokenRow->addWidget(tokenInput);
	layout->addLayout(tokenRow);
	tokenLabel->hide();
	tokenInput->hide();

	/* Date filter row */
	auto *dateRow = new QHBoxLayout();
	dateFilterCheck = new QCheckBox("Filter by date:");
	dateFilter = new QDateEdit(QDate::currentDate());
	dateFilter->setCalendarPopup(true);
	dateFilter->setDisplayFormat("MMM d, yyyy");
	dateFilter->setEnabled(false);
	connect(dateFilterCheck, &QCheckBox::toggled, this, [this](bool checked) {
		dateFilter->setEnabled(checked);
		if (!allFixtures.empty()) {
			fixtures.clear();
			if (checked) {
				QDate d = dateFilter->date();
				for (auto &f : allFixtures) {
					QString dateStr = QString::fromStdString(f.date).toLower();
					QString monthName = d.toString("MMM").toLower();
					bool dayMatch = dateStr.contains(QString::number(d.day()));
					bool monthMatch = dateStr.contains(monthName);
					bool yearMatch = dateStr.contains(QString::number(d.year()));
					if (dayMatch && monthMatch && yearMatch)
						fixtures.push_back(f);
				}
			} else {
				fixtures = allFixtures;
			}
			PopulateTable();
		}
	});
	connect(dateFilter, &QDateEdit::dateChanged, this, [this](const QDate &d) {
		if (dateFilterCheck->isChecked() && !allFixtures.empty()) {
			fixtures.clear();
			for (auto &f : allFixtures) {
				QString dateStr = QString::fromStdString(f.date).toLower();
				QString monthName = d.toString("MMM").toLower();
				bool dayMatch = dateStr.contains(QString::number(d.day()));
				bool monthMatch = dateStr.contains(monthName);
				bool yearMatch = dateStr.contains(QString::number(d.year()));
				if (dayMatch && monthMatch && yearMatch)
					fixtures.push_back(f);
			}
			PopulateTable();
		}
	});
	dateRow->addWidget(dateFilterCheck);
	dateRow->addWidget(dateFilter);
	dateRow->addStretch();
	layout->addLayout(dateRow);

	/* Buttons row: Fetch + Browse on Web */
	auto *fetchRow = new QHBoxLayout();
	fetchButton = new QPushButton("Fetch Fixtures");
	connect(fetchButton, &QPushButton::clicked, this, &CricNodeFixtureBrowser::OnFetchClicked);
	fetchRow->addWidget(fetchButton);

	browseWebButton = new QPushButton("Open in Browser");
	browseWebButton->setToolTip("Open the fixtures page in your web browser to find match IDs");
	connect(browseWebButton, &QPushButton::clicked, this, &CricNodeFixtureBrowser::OnBrowseWebClicked);
	fetchRow->addWidget(browseWebButton);
	browseWebButton->hide();

	fetchRow->addStretch();
	layout->addLayout(fetchRow);

	/* Progress */
	progressBar = new QProgressBar();
	progressBar->setVisible(false);
	layout->addWidget(progressBar);

	/* Status */
	statusLabel = new QLabel("");
	statusLabel->setWordWrap(true);
	layout->addWidget(statusLabel);

	/* Fixture table */
	fixtureTable = new QTableWidget();
	fixtureTable->setColumnCount(6);
	fixtureTable->setHorizontalHeaderLabels({"Team 1", "Team 2", "Date", "Time", "Venue", "Status"});
	fixtureTable->horizontalHeader()->setStretchLastSection(true);
	fixtureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	fixtureTable->setSelectionMode(QAbstractItemView::SingleSelection);
	fixtureTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	layout->addWidget(fixtureTable);

	/* Bottom buttons */
	auto *btnRow = new QHBoxLayout();
	selectButton = new QPushButton("Select Match");
	selectButton->setEnabled(false);
	connect(selectButton, &QPushButton::clicked, this, &CricNodeFixtureBrowser::OnSelectClicked);
	cancelButton = new QPushButton("Cancel");
	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	btnRow->addStretch();
	btnRow->addWidget(selectButton);
	btnRow->addWidget(cancelButton);
	layout->addLayout(btnRow);

	connect(fixtureTable, &QTableWidget::itemSelectionChanged, this, [this]() {
		selectButton->setEnabled(fixtureTable->currentRow() >= 0);
	});

	networkManager = new QNetworkAccessManager(this);
	connect(networkManager, &QNetworkAccessManager::finished, this,
		&CricNodeFixtureBrowser::OnNetworkReply);

	OnProviderChanged(0);
}

CricNodeFixtureBrowser::~CricNodeFixtureBrowser()
{
	CleanupCefBrowser();
}

void CricNodeFixtureBrowser::OnProviderChanged(int index)
{
	QString provider = providerCombo->itemData(index).toString();
	currentProvider = provider.toStdString();

	browseWebButton->hide();

	if (provider == "cricclubs") {
		idLabel->setText("Club ID:");
		idInput->setPlaceholderText("e.g. 423");
		tokenLabel->hide();
		tokenInput->hide();
		browseWebButton->show();
	} else if (provider == "dcl") {
		idLabel->setText("(No ID needed)");
		idInput->setPlaceholderText("Leave empty — fetches all DCL matches");
		tokenLabel->hide();
		tokenInput->hide();
	} else if (provider == "playcricket") {
		idLabel->setText("Site ID:");
		idInput->setPlaceholderText("e.g. 12345");
		tokenLabel->show();
		tokenInput->show();
		tokenLabel->setText("API Token:");
		tokenInput->setPlaceholderText("Play-Cricket API token");
	} else if (provider == "playhq") {
		idLabel->setText("Grade ID (UUID):");
		idInput->setPlaceholderText("e.g. abc123-...");
		tokenLabel->show();
		tokenInput->show();
		tokenLabel->setText("API Key:");
		tokenInput->setPlaceholderText("PlayHQ API key");
	}
}

void CricNodeFixtureBrowser::OnFetchClicked()
{
	allFixtures.clear();
	fixtures.clear();
	fixtureTable->setRowCount(0);
	selectButton->setEnabled(false);

	if (currentProvider == "dcl") {
		FetchDclFixtures();
	} else if (currentProvider == "playcricket") {
		FetchPlayCricketFixtures();
	} else if (currentProvider == "playhq") {
		FetchPlayHQFixtures();
	} else if (currentProvider == "cricclubs") {
		FetchCricClubsFixtures();
	}
}

void CricNodeFixtureBrowser::OnBrowseWebClicked()
{
	QString clubId = idInput->text().trimmed();
	if (clubId.isEmpty()) {
		QMessageBox::warning(this, "Missing Info", "Please enter a Club ID first.");
		return;
	}

	int year = QDate::currentDate().year();
	QString url;

	if (currentProvider == "cricclubs") {
		url = QString("https://cricclubs.com/club/fixtures.do?clubId=%1&league=All&year=%2&allseries=true")
			      .arg(clubId)
			      .arg(year);
	}

	if (!url.isEmpty())
		QDesktopServices::openUrl(QUrl(url));
}

/* ===================== DCL ===================== */

void CricNodeFixtureBrowser::FetchDclFixtures()
{
	SetStatus("Fetching DCL fixtures...");
	progressBar->setVisible(true);
	progressBar->setRange(0, 0);
	dclPagesLoaded = 0;
	dclTotalPages = 10;

	QUrl url("https://dallascricket.org:3000/api/getmatchlists");
	QUrlQuery query;
	query.addQueryItem("offset", "0");
	query.addQueryItem("query", "");
	query.addQueryItem("filter", "1");
	query.addQueryItem("isSearchById", "false");
	url.setQuery(query);

	QNetworkRequest request(url);
	request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
	request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
	request.setTransferTimeout(30000);
	networkManager->get(request);
}

/* ===================== Play-Cricket ===================== */

void CricNodeFixtureBrowser::FetchPlayCricketFixtures()
{
	QString siteId = idInput->text().trimmed();
	QString token = tokenInput->text().trimmed();
	if (siteId.isEmpty() || token.isEmpty()) {
		QMessageBox::warning(this, "Missing Info", "Please enter Site ID and API Token.");
		return;
	}

	SetStatus("Fetching Play-Cricket fixtures...");
	progressBar->setVisible(true);
	progressBar->setRange(0, 0);

	QUrl url("https://play-cricket.com/api/v2/result_summary.json");
	QUrlQuery query;
	query.addQueryItem("site_id", siteId);
	query.addQueryItem("season", QString::number(QDate::currentDate().year()));
	query.addQueryItem("api_token", token);
	url.setQuery(query);

	QNetworkRequest request(url);
	request.setTransferTimeout(30000);
	networkManager->get(request);
}

/* ===================== PlayHQ ===================== */

void CricNodeFixtureBrowser::FetchPlayHQFixtures()
{
	QString gradeId = idInput->text().trimmed();
	QString apiKey = tokenInput->text().trimmed();
	if (gradeId.isEmpty() || apiKey.isEmpty()) {
		QMessageBox::warning(this, "Missing Info", "Please enter Grade ID and API Key.");
		return;
	}

	SetStatus("Fetching PlayHQ fixtures...");
	progressBar->setVisible(true);
	progressBar->setRange(0, 0);

	QUrl url(QString("https://api.playhq.com/v2/grades/%1/games").arg(gradeId));
	QNetworkRequest request(url);
	request.setRawHeader("x-api-key", apiKey.toUtf8());
	request.setRawHeader("x-phq-tenant", "ca");
	request.setTransferTimeout(30000);
	networkManager->get(request);
}

/* ===================== CricClubs ===================== */

void CricNodeFixtureBrowser::FetchCricClubsFixtures()
{
	QString clubId = idInput->text().trimmed();
	if (clubId.isEmpty()) {
		QMessageBox::warning(this, "Missing Info", "Please enter a CricClubs Club ID.");
		return;
	}

	cricClubsClubId = clubId.toStdString();
	cefRetryCount = 0;

	SetStatus("Fetching CricClubs fixtures...");
	progressBar->setVisible(true);
	progressBar->setRange(0, 0);

#ifdef BROWSER_AVAILABLE
	if (cef && cef->initialized()) {
		FetchCricClubsViaCef();
		return;
	}
	blog(LOG_WARNING, "[CricNode] CEF not available, CricClubs fixture fetch won't work");
#endif
	SetStatus("Browser engine not available. Click \"Open in Browser\" to view fixtures manually.");
	progressBar->setVisible(false);
}

void CricNodeFixtureBrowser::FetchCricClubsViaCef()
{
#ifdef BROWSER_AVAILABLE
	OBSBasic::InitBrowserPanelSafeBlock();

	QString clubId = QString::fromStdString(cricClubsClubId);
	int currentYear = QDate::currentDate().year();
	std::string url =
		QString("https://cricclubs.com/club/fixtures.do?clubId=%1&league=All&year=%2&allseries=true")
			.arg(clubId)
			.arg(currentYear)
			.toStdString();

	/* Clean up any previous browser */
	CleanupCefBrowser();

	cefBrowser = cef->create_widget(this, url, nullptr);
	if (!cefBrowser) {
		blog(LOG_WARNING, "[CricNode] Failed to create CEF widget");
		SetStatus("Failed to create browser. Click \"Open in Browser\" instead.");
		progressBar->setVisible(false);
		return;
	}

	/* Hide the browser — we only need it for JS execution */
	cefBrowser->setFixedSize(1, 1);
	cefBrowser->hide();

	SetStatus("Loading CricClubs page in browser...");

	/* Listen for title changes — we set document.title to return JS results */
	connect(cefBrowser, &QCefWidget::titleChanged, this,
		&CricNodeFixtureBrowser::OnCefTitleChanged);

	/* After the page has had time to load and render, inject extraction JS.
	 * CricClubs pages are JS-heavy; 3 seconds is a safe delay
	 * (same approach as the Android app which uses a 1s postDelayed). */
	QTimer::singleShot(3500, this, &CricNodeFixtureBrowser::InjectCricClubsExtractionJs);

	blog(LOG_INFO, "[CricNode] Loading CricClubs fixtures via CEF: %s", url.c_str());
#endif
}

void CricNodeFixtureBrowser::InjectCricClubsExtractionJs()
{
#ifdef BROWSER_AVAILABLE
	if (!cefBrowser)
		return;

	SetStatus("Extracting fixtures from page...");
	std::string js = BuildCricClubsExtractionJs();
	cefBrowser->executeJavaScript(js);

	/* If no result arrives within 5 seconds, retry up to 2 times */
	QTimer::singleShot(5000, this, [this]() {
		if (fixtures.empty() && allFixtures.empty() && cefBrowser) {
			cefRetryCount++;
			if (cefRetryCount <= 2) {
				blog(LOG_INFO, "[CricNode] CEF extraction retry #%d", cefRetryCount);
				SetStatus(QString("Retrying extraction (attempt %1)...").arg(cefRetryCount + 1));
				std::string js = BuildCricClubsExtractionJs();
				cefBrowser->executeJavaScript(js);
			} else {
				SetStatus("Could not extract fixtures. Try \"Open in Browser\" instead.");
				progressBar->setVisible(false);
				CleanupCefBrowser();
			}
		}
	});
#endif
}

std::string CricNodeFixtureBrowser::BuildCricClubsExtractionJs()
{
	/* This JS extracts fixtures from the CricClubs DOM and writes the
	 * JSON result to document.title so we receive it via titleChanged.
	 * Ported from Android app's CricClubsParser.fixturesExtractionJs(). */
	int clubId = 0;
	try {
		clubId = std::stoi(cricClubsClubId);
	} catch (...) {
	}

	QString js = QString(R"JS(
document.title = (function() {
    try {
        var matches = [];
        var clubIdFallback = %1;
        var months = {jan:1,feb:2,mar:3,apr:4,may:5,jun:6,jul:7,aug:8,sep:9,oct:10,nov:11,dec:12};
        function clean(el) { return el ? (el.innerText || el.textContent || '').trim() : ''; }

        function parseSchTime(schTime) {
            if (!schTime) return null;
            var h2 = schTime.querySelector('h2');
            var h5s = schTime.querySelectorAll('h5');
            var dayNum = h2 ? parseInt(clean(h2)) : 0;
            var monthYear = h5s.length > 0 ? clean(h5s[0]) : '';
            var time = h5s.length > 1 ? clean(h5s[1]) : '';
            return {day: dayNum, time: time, dateStr: monthYear + ' ' + dayNum};
        }

        var blocks = document.querySelectorAll('div.schedule-all');
        for (var i = 0; i < blocks.length; i++) {
            var block = blocks[i];
            var schTime = block.querySelector('.sch-time');
            var dateInfo = parseSchTime(schTime);

            var teamLinks = block.querySelectorAll('.schedule-text h3 a[href*="viewTeam"]');
            var team1 = teamLinks.length > 0 ? clean(teamLinks[0]) : '';
            var team2 = teamLinks.length > 1 ? clean(teamLinks[1]) : '';

            var venueLink = block.querySelector('a[href*="viewGround"]');
            var venue = venueLink ? clean(venueLink) : '';

            var scorecardLink = block.querySelector('a[href*="viewScorecard"]');
            var hasScorecard = !!scorecardLink;

            var matchIdEl = scorecardLink || block.querySelector('a[href*="matchId"]');
            var matchId = 0, mClubId = clubIdFallback;
            if (matchIdEl) {
                var href = matchIdEl.getAttribute('href') || '';
                var mm = href.match(/matchId=(\d+)/);
                if (mm) matchId = parseInt(mm[1]);
                var cm = href.match(/clubId=(\d+)/);
                if (cm) mClubId = parseInt(cm[1]);
            }
            if (matchId === 0) {
                var rowDiv = block.querySelector('[id^="deleteRow"]');
                if (rowDiv) { var rm = rowDiv.id.match(/deleteRow(\d+)/); if (rm) matchId = parseInt(rm[1]); }
            }
            if (matchId === 0) {
                var rosterLink = block.querySelector('a[href*="fixtureId"]');
                if (rosterLink) { var fm = (rosterLink.getAttribute('href')||'').match(/fixtureId=(\d+)/); if (fm) matchId = parseInt(fm[1]); }
            }

            var status = 'SCHEDULED';
            if (hasScorecard) {
                var bt = clean(block).toLowerCase();
                if (bt.indexOf('won by')!==-1||bt.indexOf('tied')!==-1||bt.indexOf('draw')!==-1||bt.indexOf('no result')!==-1)
                    status = 'COMPLETED';
                else status = 'LIVE';
            }

            if (matchId > 0) {
                matches.push({matchId:matchId, clubId:mClubId, team1:team1, team2:team2,
                    time:dateInfo?dateInfo.time:'', date:dateInfo?dateInfo.dateStr:'',
                    venue:venue, status:status, hasScorecard:hasScorecard});
            }
        }
        return 'CRICNODE_FIXTURES:' + JSON.stringify({success:true, matches:matches, blocks:blocks.length});
    } catch(e) {
        return 'CRICNODE_FIXTURES:' + JSON.stringify({success:false, error:e.message, matches:[]});
    }
})();
)JS")
				   .arg(clubId);

	return js.toStdString();
}

void CricNodeFixtureBrowser::OnCefTitleChanged(const QString &title)
{
	if (!title.startsWith("CRICNODE_FIXTURES:"))
		return;

	QString json = title.mid(QString("CRICNODE_FIXTURES:").length());
	blog(LOG_INFO, "[CricNode] CEF extraction result: %s",
	     json.left(300).toUtf8().constData());

	ParseCricClubsCefResult(json);
	CleanupCefBrowser();
}

void CricNodeFixtureBrowser::ParseCricClubsCefResult(const QString &json)
{
	QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
	QJsonObject root = doc.object();

	if (!root["success"].toBool()) {
		QString error = root["error"].toString();
		SetStatus("JS extraction failed: " + error);
		progressBar->setVisible(false);
		return;
	}

	QJsonArray matchArr = root["matches"].toArray();
	int blockCount = root["blocks"].toInt();

	for (auto val : matchArr) {
		QJsonObject m = val.toObject();
		CricNodeFixture f;
		f.provider = "cricclubs";
		f.matchId = QString::number(m["matchId"].toInt()).toStdString();
		f.clubId = QString::number(m["clubId"].toInt()).toStdString();
		f.team1 = m["team1"].toString().toStdString();
		f.team2 = m["team2"].toString().toStdString();
		f.date = m["date"].toString().toStdString();
		f.time = m["time"].toString().toStdString();
		f.venue = m["venue"].toString().toStdString();
		f.status = m["status"].toString().toStdString();
		f.hasScorecard = m["hasScorecard"].toBool();

		if (!f.matchId.empty() && f.matchId != "0")
			fixtures.push_back(f);
	}

	allFixtures = fixtures;
	PopulateTable();
	progressBar->setVisible(false);

	blog(LOG_INFO, "[CricNode] CEF extracted %d fixtures from %d blocks",
	     (int)fixtures.size(), blockCount);
}

void CricNodeFixtureBrowser::CleanupCefBrowser()
{
#ifdef BROWSER_AVAILABLE
	if (cefBrowser) {
		cefBrowser->closeBrowser();
		cefBrowser->deleteLater();
		cefBrowser = nullptr;
	}
#endif
}

/* ===================== Network reply router ===================== */

void CricNodeFixtureBrowser::OnNetworkReply(QNetworkReply *reply)
{
	progressBar->setVisible(false);

	if (reply->error() != QNetworkReply::NoError) {
		SetStatus("Error: " + reply->errorString());
		reply->deleteLater();
		return;
	}

	QByteArray data = reply->readAll();
	QUrl requestUrl = reply->request().url();
	reply->deleteLater();

	if (requestUrl.host().contains("dallascricket")) {
		ParseDclResponse(data);
		dclPagesLoaded++;
		if (dclPagesLoaded < dclTotalPages) {
			QUrl url("https://dallascricket.org:3000/api/getmatchlists");
			QUrlQuery query;
			query.addQueryItem("offset", QString::number(dclPagesLoaded * 10));
			query.addQueryItem("query", "");
			query.addQueryItem("filter", "1");
			query.addQueryItem("isSearchById", "false");
			url.setQuery(query);

			QNetworkRequest request(url);
			request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
			request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
			request.setTransferTimeout(30000);
			networkManager->get(request);

			SetStatus(QString("Fetching DCL fixtures... (page %1/%2)")
					  .arg(dclPagesLoaded + 1)
					  .arg(dclTotalPages));
			progressBar->setVisible(true);
		} else {
			allFixtures = fixtures;
			PopulateTable();
		}
	} else if (requestUrl.host().contains("play-cricket")) {
		ParsePlayCricketResponse(data);
		allFixtures = fixtures;
		PopulateTable();
	} else if (requestUrl.host().contains("playhq")) {
		ParsePlayHQResponse(data);
		allFixtures = fixtures;
		PopulateTable();
	} else if (requestUrl.host().contains("cricclubs")) {
		ParseCricClubsResponse(data);
		allFixtures = fixtures;
		PopulateTable();
	}
}

/* ===================== Response parsers ===================== */

void CricNodeFixtureBrowser::ParseDclResponse(const QByteArray &data)
{
	QJsonDocument doc = QJsonDocument::fromJson(data);
	QJsonObject root = doc.object();
	if (!root["success"].toBool())
		return;

	QJsonArray matchList = root["matchLists"].toArray();
	for (auto val : matchList) {
		QJsonObject m = val.toObject();
		CricNodeFixture f;
		f.provider = "dcl";
		f.matchId = QString::number(m["id"].toInt()).toStdString();
		f.team1 = m["team1Name"].toString().toStdString();
		f.team2 = m["team2Name"].toString().toStdString();
		f.date = m["date"].toString().toStdString();
		f.time = m["start_time"].toString().toStdString();
		f.venue = m["ground_name"].toString().toStdString();

		if (m["is_live"].toInt() == 1)
			f.status = "LIVE";
		else if (m["is_match_ended"].toBool())
			f.status = "COMPLETED";
		else
			f.status = "SCHEDULED";

		f.hasScorecard = (f.status != "SCHEDULED");
		fixtures.push_back(f);
	}
}

void CricNodeFixtureBrowser::ParsePlayCricketResponse(const QByteArray &data)
{
	QJsonDocument doc = QJsonDocument::fromJson(data);
	QJsonObject root = doc.object();
	QJsonArray results = root["result_summary"].toArray();

	for (auto val : results) {
		QJsonObject m = val.toObject();
		CricNodeFixture f;
		f.provider = "playcricket";
		f.matchId = QString::number(m["id"].toInt()).toStdString();
		f.team1 = m["home_team_name"].toString().toStdString();
		f.team2 = m["away_team_name"].toString().toStdString();
		f.date = m["match_date"].toString().toStdString();
		f.time = m["match_time"].toString().toStdString();
		f.venue = m["ground_name"].toString().toStdString();

		QString status = m["status"].toString();
		if (status == "InProgress")
			f.status = "LIVE";
		else if (status == "Completed")
			f.status = "COMPLETED";
		else if (status == "Cancelled")
			f.status = "CANCELLED";
		else
			f.status = "SCHEDULED";

		f.hasScorecard = (f.status == "LIVE" || f.status == "COMPLETED");
		fixtures.push_back(f);
	}
}

void CricNodeFixtureBrowser::ParsePlayHQResponse(const QByteArray &data)
{
	QJsonDocument doc = QJsonDocument::fromJson(data);
	QJsonObject root = doc.object();
	QJsonArray rounds = root["rounds"].toArray();

	QJsonArray teamsArr = root["teams"].toArray();
	QMap<QString, QString> teamNames;
	for (auto t : teamsArr) {
		QJsonObject team = t.toObject();
		teamNames[team["id"].toString()] = team["name"].toString();
	}

	for (auto rv : rounds) {
		QJsonObject round = rv.toObject();
		QJsonArray games = round["games"].toArray();

		for (auto gv : games) {
			QJsonObject game = gv.toObject();
			CricNodeFixture f;
			f.provider = "playhq";
			f.matchId = game["id"].toString().toStdString();

			QJsonArray gameTeams = game["teams"].toArray();
			if (gameTeams.size() >= 2) {
				f.team1 = gameTeams[0].toObject()["name"].toString().toStdString();
				f.team2 = gameTeams[1].toObject()["name"].toString().toStdString();
			}

			QJsonArray schedule = game["schedule"].toArray();
			if (!schedule.isEmpty()) {
				QString dt = schedule[0].toObject()["dateTime"].toString();
				if (dt.contains("T")) {
					f.date = dt.left(10).toStdString();
					f.time = dt.mid(11, 5).toStdString();
				}
			}

			QString status = game["status"].toString();
			if (status == "LIVE")
				f.status = "LIVE";
			else if (status == "COMPLETED")
				f.status = "COMPLETED";
			else
				f.status = "SCHEDULED";

			f.hasScorecard = (f.status != "SCHEDULED");
			fixtures.push_back(f);
		}
	}
}

void CricNodeFixtureBrowser::ParseCricClubsResponse(const QByteArray &data)
{
	/* HTTP fallback parser — regex-based HTML scraping.
	 * Usually blocked by CricClubs (403). CEF path is preferred. */
	QString html = QString::fromUtf8(data);

	if (html.contains("403") || html.contains("Access Denied") || html.length() < 500) {
		SetStatus("CricClubs blocked the request.\nClick \"Open in Browser\" to view fixtures manually.");
		return;
	}

	QRegularExpression schedStart("schedule-all", QRegularExpression::CaseInsensitiveOption);
	QList<int> blockStarts;
	auto it = schedStart.globalMatch(html);
	while (it.hasNext()) {
		auto m = it.next();
		blockStarts.append(m.capturedStart());
	}

	if (blockStarts.isEmpty()) {
		SetStatus("No fixtures found. Click \"Open in Browser\" to view manually.");
		return;
	}

	QRegularExpression teamRe("viewTeam[^\"]*\"[^>]*>([^<]+)<", QRegularExpression::CaseInsensitiveOption);
	QRegularExpression venueRe("viewGround[^\"]*\"[^>]*>([^<]+)<", QRegularExpression::CaseInsensitiveOption);
	QRegularExpression scorecardRe("viewScorecard[^\"]*matchId=(\\d+)", QRegularExpression::CaseInsensitiveOption);
	QRegularExpression matchIdRe("matchId=(\\d+)", QRegularExpression::CaseInsensitiveOption);
	QRegularExpression clubIdRe("clubId=(\\d+)", QRegularExpression::CaseInsensitiveOption);
	QRegularExpression deleteRowRe("deleteRow(\\d+)", QRegularExpression::CaseInsensitiveOption);
	QRegularExpression fixtureIdRe("fixtureId=(\\d+)", QRegularExpression::CaseInsensitiveOption);
	QRegularExpression h2Re("<h2[^>]*>(\\d+)</h2>", QRegularExpression::CaseInsensitiveOption);
	QRegularExpression h5Re("<h5[^>]*>([^<]+)</h5>", QRegularExpression::CaseInsensitiveOption);

	for (int i = 0; i < blockStarts.size(); i++) {
		int start = blockStarts[i];
		int end = (i + 1 < blockStarts.size()) ? blockStarts[i + 1] : html.size();
		QString block = html.mid(start, end - start);

		CricNodeFixture f;
		f.provider = "cricclubs";
		f.clubId = cricClubsClubId;

		auto teamIt = teamRe.globalMatch(block);
		if (teamIt.hasNext())
			f.team1 = teamIt.next().captured(1).trimmed().toStdString();
		if (teamIt.hasNext())
			f.team2 = teamIt.next().captured(1).trimmed().toStdString();
		if (f.team1.empty() && f.team2.empty())
			continue;

		auto h2Match = h2Re.match(block);
		QList<QString> h5Values;
		auto h5It = h5Re.globalMatch(block);
		while (h5It.hasNext())
			h5Values.append(h5It.next().captured(1).trimmed());

		QString monthYear = h5Values.size() > 0 ? h5Values[0] : "";
		QString timeStr = h5Values.size() > 1 ? h5Values[1] : "";
		QString dayStr = h2Match.hasMatch() ? h2Match.captured(1) : "";
		f.date = (monthYear + " " + dayStr).trimmed().toStdString();
		f.time = timeStr.toStdString();

		auto venueMatch = venueRe.match(block);
		if (venueMatch.hasMatch())
			f.venue = venueMatch.captured(1).trimmed().toStdString();

		auto scMatch = scorecardRe.match(block);
		f.hasScorecard = scMatch.hasMatch();

		if (scMatch.hasMatch()) {
			f.matchId = scMatch.captured(1).toStdString();
		} else {
			auto midMatch = matchIdRe.match(block);
			if (midMatch.hasMatch())
				f.matchId = midMatch.captured(1).toStdString();
		}
		if (f.matchId.empty()) {
			auto drMatch = deleteRowRe.match(block);
			if (drMatch.hasMatch())
				f.matchId = drMatch.captured(1).toStdString();
		}
		if (f.matchId.empty()) {
			auto fiMatch = fixtureIdRe.match(block);
			if (fiMatch.hasMatch())
				f.matchId = fiMatch.captured(1).toStdString();
		}

		auto cidMatch = clubIdRe.match(block);
		if (cidMatch.hasMatch())
			f.clubId = cidMatch.captured(1).toStdString();

		QString blockLower = block.toLower();
		if (f.hasScorecard) {
			if (blockLower.contains("won by") || blockLower.contains("tied") ||
			    blockLower.contains("draw") || blockLower.contains("no result")) {
				f.status = "COMPLETED";
			} else {
				f.status = "LIVE";
			}
		} else {
			f.status = "SCHEDULED";
		}

		if (!f.matchId.empty())
			fixtures.push_back(f);
	}

	if (fixtures.empty())
		SetStatus("Could not parse fixtures. Click \"Open in Browser\" to find match IDs manually.");
}

/* ===================== Table / UI ===================== */

void CricNodeFixtureBrowser::PopulateTable()
{
	fixtureTable->setRowCount((int)fixtures.size());
	for (int i = 0; i < (int)fixtures.size(); i++) {
		auto &f = fixtures[i];
		fixtureTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(f.team1)));
		fixtureTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(f.team2)));
		fixtureTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(f.date)));
		fixtureTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(f.time)));
		fixtureTable->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(f.venue)));

		auto *statusItem = new QTableWidgetItem(QString::fromStdString(f.status));
		if (f.status == "LIVE")
			statusItem->setForeground(Qt::green);
		else if (f.status == "COMPLETED")
			statusItem->setForeground(Qt::gray);
		fixtureTable->setItem(i, 5, statusItem);
	}

	fixtureTable->resizeColumnsToContents();
	SetStatus(QString("Found %1 fixtures").arg(fixtures.size()));
}

void CricNodeFixtureBrowser::OnSelectClicked()
{
	int row = fixtureTable->currentRow();
	if (row >= 0 && row < (int)fixtures.size()) {
		selectedFixture = fixtures[row];
		accept();
	}
}

void CricNodeFixtureBrowser::SetStatus(const QString &text)
{
	statusLabel->setText(text);
}
