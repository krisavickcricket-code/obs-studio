#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

class CricNodeWiFiCameraDialog : public QDialog {
	Q_OBJECT

public:
	explicit CricNodeWiFiCameraDialog(QWidget *parent = nullptr);

	QString GetCameraName() const;
	QString GetRtspUrl() const;
	int GetReconnectDelay() const;

private slots:
	void OnAccepted();

private:
	QLineEdit *nameEdit;
	QLineEdit *urlEdit;
	QSpinBox *reconnectSpin;
};
