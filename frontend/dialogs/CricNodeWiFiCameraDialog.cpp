#include "CricNodeWiFiCameraDialog.hpp"

#include <utility/CricNodeStyle.hpp>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>

#include <obs.hpp>

CricNodeWiFiCameraDialog::CricNodeWiFiCameraDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle("CricNode - WiFi Camera");
	setMinimumWidth(420);
	setStyleSheet(CricNodeStyle::DialogSheet());

	auto *layout = new QVBoxLayout(this);
	layout->setSpacing(12);
	layout->setContentsMargins(16, 16, 16, 16);

	/* Branded header */
	auto *headerLabel = new QLabel("Add WiFi / IP Camera", this);
	headerLabel->setStyleSheet(CricNodeStyle::HeaderSheet());
	layout->addWidget(headerLabel);

	auto *descLabel = new QLabel(
		"Enter the RTSP URL from your WiFi camera to add it as a video source.",
		this);
	descLabel->setWordWrap(true);
	layout->addWidget(descLabel);

	/* Form fields */
	auto *form = new QFormLayout();
	form->setSpacing(8);

	/* Camera name - auto-generate default */
	nameEdit = new QLineEdit(this);
	QString defaultName = "WiFi Camera 1";
	int counter = 1;
	for (;;) {
		OBSSourceAutoRelease existing = obs_get_source_by_name(
			defaultName.toUtf8().constData());
		if (!existing)
			break;
		counter++;
		defaultName = QString("WiFi Camera %1").arg(counter);
	}
	nameEdit->setText(defaultName);
	form->addRow("Camera Name:", nameEdit);

	urlEdit = new QLineEdit(this);
	urlEdit->setPlaceholderText("rtsp://192.168.1.100:554/stream");
	form->addRow("RTSP URL:", urlEdit);

	reconnectSpin = new QSpinBox(this);
	reconnectSpin->setRange(1, 120);
	reconnectSpin->setValue(10);
	reconnectSpin->setSuffix(" seconds");
	form->addRow("Reconnect Delay:", reconnectSpin);

	layout->addLayout(form);

	layout->addStretch();

	/* Buttons */
	auto *btnRow = new QHBoxLayout();
	btnRow->addStretch();

	auto *cancelBtn = new QPushButton("Cancel", this);
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
	btnRow->addWidget(cancelBtn);

	auto *okBtn = new QPushButton("Add Camera", this);
	okBtn->setDefault(true);
	connect(okBtn, &QPushButton::clicked, this,
		&CricNodeWiFiCameraDialog::OnAccepted);
	btnRow->addWidget(okBtn);

	layout->addLayout(btnRow);
}

void CricNodeWiFiCameraDialog::OnAccepted()
{
	if (nameEdit->text().trimmed().isEmpty()) {
		QMessageBox::warning(this, "CricNode",
				     "Please enter a camera name.");
		nameEdit->setFocus();
		return;
	}
	if (urlEdit->text().trimmed().isEmpty()) {
		QMessageBox::warning(this, "CricNode",
				     "Please enter the RTSP URL.");
		urlEdit->setFocus();
		return;
	}

	/* Check for duplicate source name */
	OBSSourceAutoRelease existing = obs_get_source_by_name(
		nameEdit->text().trimmed().toUtf8().constData());
	if (existing) {
		QMessageBox::warning(
			this, "CricNode",
			QString("A source named \"%1\" already exists. "
				"Please choose a different name.")
				.arg(nameEdit->text().trimmed()));
		nameEdit->setFocus();
		nameEdit->selectAll();
		return;
	}

	accept();
}

QString CricNodeWiFiCameraDialog::GetCameraName() const
{
	return nameEdit->text().trimmed();
}

QString CricNodeWiFiCameraDialog::GetRtspUrl() const
{
	return urlEdit->text().trimmed();
}

int CricNodeWiFiCameraDialog::GetReconnectDelay() const
{
	return reconnectSpin->value();
}
