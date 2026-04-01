#pragma once

#include <QDialog>
#include <QCheckBox>

class CricNodeOverlaySettings : public QDialog {
	Q_OBJECT

public:
	explicit CricNodeOverlaySettings(QWidget *parent = nullptr);

	static void LoadDefaults();

	static bool GetIntro();
	static bool GetPlayer();
	static bool GetWicket();
	static bool GetInnings();
	static bool GetSidepanel();
	static bool GetResult();

private slots:
	void OnAccepted();

private:
	QCheckBox *introCheck;
	QCheckBox *playerCheck;
	QCheckBox *wicketCheck;
	QCheckBox *inningsCheck;
	QCheckBox *sidepanelCheck;
	QCheckBox *resultCheck;
};
