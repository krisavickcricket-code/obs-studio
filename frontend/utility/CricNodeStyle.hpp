#pragma once

#include <QString>

/* CricNode brand colors — from https://cricnode.com/ */
namespace CricNodeStyle {

/* Primary brand (cyan/teal) */
constexpr const char *BrandPrimary = "#06b6d4";
constexpr const char *BrandLight = "#22d3ee";
constexpr const char *BrandDark = "#0891b2";

/* Secondary (green) */
constexpr const char *SecondaryGreen = "#10b981";

/* Accent (amber) */
constexpr const char *AccentAmber = "#fbbf24";

/* Surfaces */
constexpr const char *SurfaceDark = "#0b1120";
constexpr const char *SurfaceMid = "#1e293b";
constexpr const char *SurfaceLight = "#334155";
constexpr const char *SurfaceBorder = "#475569";

/* Text */
constexpr const char *TextPrimary = "#f1f5f9";
constexpr const char *TextSecondary = "#94a3b8";
constexpr const char *TextMuted = "#64748b";

/* Status */
constexpr const char *StatusLive = "#10b981";
constexpr const char *StatusScheduled = "#06b6d4";
constexpr const char *StatusCompleted = "#64748b";
constexpr const char *StatusError = "#fb2c36";

/* Stylesheet for CricNode dialogs & widgets.
 * Applied via widget->setStyleSheet(CricNodeStyle::DialogSheet()); */
inline QString DialogSheet()
{
	return QStringLiteral(
		/* Dialog background */
		"QDialog { background-color: %1; color: %2; }"

		/* Group boxes */
		"QGroupBox { border: 1px solid %3; border-radius: 6px; margin-top: 12px; "
		"  padding: 12px 8px 8px 8px; font-weight: bold; color: %4; }"
		"QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; "
		"  color: %4; }"

		/* Buttons */
		"QPushButton { background-color: %5; color: white; border: none; "
		"  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
		"QPushButton:hover { background-color: %6; }"
		"QPushButton:pressed { background-color: %7; }"
		"QPushButton:disabled { background-color: %8; color: %9; }"

		/* Inputs */
		"QLineEdit, QComboBox, QDateEdit, QSpinBox { background-color: %10; "
		"  color: %2; border: 1px solid %3; border-radius: 4px; padding: 4px 8px; }"
		"QLineEdit:focus, QComboBox:focus { border-color: %4; }"

		/* Checkboxes */
		"QCheckBox { color: %2; spacing: 6px; }"
		"QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px; "
		"  border: 2px solid %3; background-color: %10; }"
		"QCheckBox::indicator:checked { background-color: %4; border-color: %4; }"

		/* Labels */
		"QLabel { color: %2; }"

		/* Progress bar */
		"QProgressBar { border: 1px solid %3; border-radius: 4px; text-align: center; "
		"  background-color: %10; color: %2; height: 16px; }"
		"QProgressBar::chunk { background-color: %4; border-radius: 3px; }"

		/* Table widget */
		"QTableWidget { background-color: %10; alternate-background-color: %1; "
		"  color: %2; gridline-color: %3; border: 1px solid %3; border-radius: 4px; "
		"  selection-background-color: rgba(6, 182, 212, 0.3); }"
		"QHeaderView::section { background-color: %11; color: %4; "
		"  padding: 6px 8px; border: none; border-bottom: 2px solid %4; "
		"  font-weight: bold; }"
		"QTableWidget::item { padding: 4px 8px; }"
		"QTableWidget::item:selected { background-color: rgba(6, 182, 212, 0.25); }"

		/* List widget */
		"QListWidget { background-color: %10; color: %2; border: 1px solid %3; "
		"  border-radius: 4px; }"
		"QListWidget::item { padding: 6px 8px; border-bottom: 1px solid %3; }"
		"QListWidget::item:selected { background-color: rgba(6, 182, 212, 0.25); }"
		"QListWidget::item:hover { background-color: rgba(6, 182, 212, 0.1); }"

		/* Scroll bar */
		"QScrollBar:vertical { background: %1; width: 8px; border-radius: 4px; }"
		"QScrollBar::handle:vertical { background: %3; border-radius: 4px; min-height: 20px; }"
		"QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
	)
		.arg(SurfaceDark)     /* %1 */
		.arg(TextPrimary)     /* %2 */
		.arg(SurfaceLight)    /* %3 */
		.arg(BrandPrimary)    /* %4 */
		.arg(BrandPrimary)    /* %5  button bg */
		.arg(BrandLight)      /* %6  button hover */
		.arg(BrandDark)       /* %7  button pressed */
		.arg(SurfaceLight)    /* %8  button disabled bg */
		.arg(TextMuted)       /* %9  button disabled text */
		.arg(SurfaceMid)      /* %10 input bg */
		.arg(SurfaceDark);    /* %11 header bg */
}

/* Stylesheet for CricNode widgets (embedded in main window, uses QWidget not QDialog) */
inline QString WidgetSheet()
{
	return QStringLiteral(
		"QWidget#cricnodeWidget { background-color: %1; }"
		"QLabel { color: %2; }"

		"QPushButton { background-color: %3; color: white; border: none; "
		"  border-radius: 4px; padding: 5px 12px; font-weight: bold; font-size: 11px; }"
		"QPushButton:hover { background-color: %4; }"
		"QPushButton:pressed { background-color: %5; }"

		"QListWidget { background-color: %6; color: %2; border: 1px solid %7; "
		"  border-radius: 4px; }"
		"QListWidget::item { padding: 5px 8px; border-bottom: 1px solid %7; }"
		"QListWidget::item:selected { background-color: rgba(6, 182, 212, 0.25); }"
		"QListWidget::item:hover { background-color: rgba(6, 182, 212, 0.1); }"
	)
		.arg(SurfaceDark)     /* %1 widget bg */
		.arg(TextPrimary)     /* %2 text */
		.arg(BrandPrimary)    /* %3 button bg */
		.arg(BrandLight)      /* %4 button hover */
		.arg(BrandDark)       /* %5 button pressed */
		.arg(SurfaceMid)      /* %6 list bg */
		.arg(SurfaceLight);   /* %7 borders */
}

/* Styled header label for CricNode panels */
inline QString HeaderSheet()
{
	return QStringLiteral(
		"font-weight: bold; font-size: 14px; color: %1; "
		"padding: 4px 0;"
	).arg(BrandPrimary);
}

/* Status badge colors */
inline QString StatusColor(const std::string &status)
{
	if (status == "LIVE")
		return StatusLive;
	if (status == "SCHEDULED")
		return StatusScheduled;
	if (status == "COMPLETED")
		return StatusCompleted;
	return TextMuted;
}

} // namespace CricNodeStyle
