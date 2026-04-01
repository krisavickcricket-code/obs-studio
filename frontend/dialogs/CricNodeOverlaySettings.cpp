#include "CricNodeOverlaySettings.hpp"

#include <OBSApp.hpp>
#include <utility/CricNodeStyle.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QGroupBox>

#include <obs-frontend-api.h>
#include <util/config-file.h>

#define CRICNODE_SECTION "CricNode"

CricNodeOverlaySettings::CricNodeOverlaySettings(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle("CricNode - Overlay Features");
	setMinimumWidth(320);
	setStyleSheet(CricNodeStyle::DialogSheet());

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setSpacing(12);
	mainLayout->setContentsMargins(16, 16, 16, 16);

	/* Logo header */
	QHBoxLayout *headerRow = new QHBoxLayout();
	QLabel *logoLabel = new QLabel(this);
	QPixmap logoPix(":/res/images/cricnode_globe.png");
	logoLabel->setPixmap(logoPix.scaledToHeight(28, Qt::SmoothTransformation));
	headerRow->addWidget(logoLabel);
	QLabel *titleLabel = new QLabel("Overlay Features", this);
	titleLabel->setStyleSheet(
		QString("font-size: 15px; font-weight: bold; color: %1;")
			.arg(CricNodeStyle::BrandPrimary));
	headerRow->addWidget(titleLabel);
	headerRow->addStretch();
	mainLayout->addLayout(headerRow);

	QLabel *header = new QLabel("Toggle which overlay features appear during the match:", this);
	header->setWordWrap(true);
	mainLayout->addWidget(header);

	QGroupBox *group = new QGroupBox("Features", this);
	QVBoxLayout *groupLayout = new QVBoxLayout(group);

	config_t *config = App()->GetUserConfig();

	introCheck = new QCheckBox("Match Intro", group);
	introCheck->setChecked(config_get_bool(config, CRICNODE_SECTION, "OverlayIntro"));
	groupLayout->addWidget(introCheck);

	playerCheck = new QCheckBox("Player Intros", group);
	playerCheck->setChecked(config_get_bool(config, CRICNODE_SECTION, "OverlayPlayer"));
	groupLayout->addWidget(playerCheck);

	wicketCheck = new QCheckBox("Wicket Events", group);
	wicketCheck->setChecked(config_get_bool(config, CRICNODE_SECTION, "OverlayWicket"));
	groupLayout->addWidget(wicketCheck);

	inningsCheck = new QCheckBox("Innings Break", group);
	inningsCheck->setChecked(config_get_bool(config, CRICNODE_SECTION, "OverlayInnings"));
	groupLayout->addWidget(inningsCheck);

	sidepanelCheck = new QCheckBox("Side Panel", group);
	sidepanelCheck->setChecked(config_get_bool(config, CRICNODE_SECTION, "OverlaySidepanel"));
	groupLayout->addWidget(sidepanelCheck);

	resultCheck = new QCheckBox("Match Result", group);
	resultCheck->setChecked(config_get_bool(config, CRICNODE_SECTION, "OverlayResult"));
	groupLayout->addWidget(resultCheck);

	mainLayout->addWidget(group);

	/* OK / Cancel */
	QHBoxLayout *btnLayout = new QHBoxLayout();
	QPushButton *okBtn = new QPushButton("  Save  ", this);
	QPushButton *cancelBtn = new QPushButton("  Cancel  ", this);
	cancelBtn->setStyleSheet(
		QString("QPushButton { background-color: %1; } QPushButton:hover { background-color: %2; }")
			.arg(CricNodeStyle::SurfaceLight)
			.arg(CricNodeStyle::SurfaceBorder));
	btnLayout->addStretch();
	btnLayout->addWidget(okBtn);
	btnLayout->addWidget(cancelBtn);
	mainLayout->addLayout(btnLayout);

	connect(okBtn, &QPushButton::clicked, this, &CricNodeOverlaySettings::OnAccepted);
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void CricNodeOverlaySettings::OnAccepted()
{
	config_t *config = App()->GetUserConfig();

	config_set_bool(config, CRICNODE_SECTION, "OverlayIntro", introCheck->isChecked());
	config_set_bool(config, CRICNODE_SECTION, "OverlayPlayer", playerCheck->isChecked());
	config_set_bool(config, CRICNODE_SECTION, "OverlayWicket", wicketCheck->isChecked());
	config_set_bool(config, CRICNODE_SECTION, "OverlayInnings", inningsCheck->isChecked());
	config_set_bool(config, CRICNODE_SECTION, "OverlaySidepanel", sidepanelCheck->isChecked());
	config_set_bool(config, CRICNODE_SECTION, "OverlayResult", resultCheck->isChecked());

	config_save_safe(config, "tmp", nullptr);

	accept();
}

void CricNodeOverlaySettings::LoadDefaults()
{
	config_t *config = App()->GetUserConfig();

	config_set_default_bool(config, CRICNODE_SECTION, "OverlayIntro", true);
	config_set_default_bool(config, CRICNODE_SECTION, "OverlayPlayer", true);
	config_set_default_bool(config, CRICNODE_SECTION, "OverlayWicket", true);
	config_set_default_bool(config, CRICNODE_SECTION, "OverlayInnings", true);
	config_set_default_bool(config, CRICNODE_SECTION, "OverlaySidepanel", true);
	config_set_default_bool(config, CRICNODE_SECTION, "OverlayResult", true);
}

bool CricNodeOverlaySettings::GetIntro()
{
	return config_get_bool(App()->GetUserConfig(), CRICNODE_SECTION, "OverlayIntro");
}

bool CricNodeOverlaySettings::GetPlayer()
{
	return config_get_bool(App()->GetUserConfig(), CRICNODE_SECTION, "OverlayPlayer");
}

bool CricNodeOverlaySettings::GetWicket()
{
	return config_get_bool(App()->GetUserConfig(), CRICNODE_SECTION, "OverlayWicket");
}

bool CricNodeOverlaySettings::GetInnings()
{
	return config_get_bool(App()->GetUserConfig(), CRICNODE_SECTION, "OverlayInnings");
}

bool CricNodeOverlaySettings::GetSidepanel()
{
	return config_get_bool(App()->GetUserConfig(), CRICNODE_SECTION, "OverlaySidepanel");
}

bool CricNodeOverlaySettings::GetResult()
{
	return config_get_bool(App()->GetUserConfig(), CRICNODE_SECTION, "OverlayResult");
}
