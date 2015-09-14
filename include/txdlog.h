#pragma once

#include <qlist.h>
#include <qwidget.h>
#include <qstring.h>
#include <qlistview.h>
#include <qdialog.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qscrollbar.h>
#include <qfiledialog.h>
#include <qobject.h>
#include <QHBoxLayout>
#include <qlistwidget.h>
#include <QlistWidgetItem>

#include "defs.h"

#include <Windows.h>

#include <ctime>

enum eLogMessageType
{
	LOGMSG_INFO,
	LOGMSG_WARNING,
	LOGMSG_ERROR
};

class LogItemWidget : public QWidget
{
	eLogMessageType msgType;

public:
	QLabel *textLabel;

	LogItemWidget(eLogMessageType MessageType, QPixmap &pixmap, QString message) : QWidget()
	{
		msgType = MessageType;
		QLabel *iconLabel = new QLabel();
		iconLabel->setPixmap(pixmap);
		iconLabel->setFixedSize(20, 20);
		textLabel = new QLabel(message);
		textLabel->setObjectName("logText");
		QHBoxLayout *layout = new QHBoxLayout();
		layout->setContentsMargins(2, 2, 2, 2);
		layout->addSpacing(8);
		layout->addWidget(iconLabel);
		layout->addWidget(textLabel);
		this->setLayout(layout);
	}

	eLogMessageType getMessageType()
	{
		return this->msgType;
	}

	QString getText()
	{
		return this->textLabel->text();
	}
};

class TxdLog : public QObject
{
	Q_OBJECT

public:
	TxdLog(QWidget *ParentWidget)
	{
		parent = ParentWidget;
		positioned = false;
		logWidget = new QWidget(ParentWidget, Qt::Window);
		logWidget->setWindowTitle("Log");
		logWidget->setMinimumSize(450, 150);
		logWidget->resize(500, 200);
		logWidget->setObjectName("background_1");
		/* --- Top panel --- */
		QPushButton *buttonSave = new QPushButton("Save");

		connect(buttonSave, SIGNAL(clicked()), this, SLOT(onLogSaveRequest()));

		buttonSave->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QPushButton *buttonCopy = new QPushButton("Copy");

		connect(buttonCopy, SIGNAL(clicked()), this, SLOT(onCopyLogLinesRequest()));

		buttonSave->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QPushButton *buttonCopyAll = new QPushButton("Copy All");

		connect(buttonCopyAll, SIGNAL(clicked()), this, SLOT(onCopyAllLogLinesRequest()));

		buttonCopyAll->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QPushButton *buttonClear = new QPushButton("Clear");

		connect(buttonClear, SIGNAL(clicked()), this, SLOT(onLogClearRequest()));

		buttonClear->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QPushButton *buttonClose = new QPushButton("Close");

		connect(buttonClose, SIGNAL(clicked()), this, SLOT(onWindowHideRequest()));

		buttonClose->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QHBoxLayout *buttonsLayout = new QHBoxLayout;
		buttonsLayout->addWidget(buttonSave);
		buttonsLayout->addWidget(buttonCopy);
		buttonsLayout->addWidget(buttonCopyAll);
		buttonsLayout->addWidget(buttonClear);
		buttonsLayout->addWidget(buttonClose);

		QWidget *buttonsBackground = new QWidget();
		buttonsBackground->setFixedHeight(60);
		buttonsBackground->setObjectName("background_1");
		buttonsBackground->setLayout(buttonsLayout);
		buttonsBackground->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QWidget *hLineBackground = new QWidget();
		hLineBackground->setFixedHeight(1);
		hLineBackground->setObjectName("hLineBackground");
		QVBoxLayout *mainLayout = new QVBoxLayout();
		mainLayout->addWidget(buttonsBackground);
		mainLayout->addWidget(hLineBackground);

		/* --- List --- */
		listWidget = new QListWidget;
		listWidget->setObjectName("logList");
		mainLayout->addWidget(listWidget);
		listWidget->setSelectionMode(QAbstractItemView::SelectionMode::MultiSelection);
		listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

		mainLayout->setContentsMargins(0, 0, 0, 0);
		mainLayout->setMargin(0);
		mainLayout->setSpacing(0);
		logWidget->setLayout(mainLayout);

		// icons
        // NOTE: the Qt libpng implementation complains about a "known invalid sRGB profile" here.
		picWarning.load("resources/warning.png");
		picError.load("resources/error.png");
		picInfo.load("resources/info.png");
	}

	void show()
	{
		if (!positioned)
		{
			QPoint mainWindowPosn = parent->pos();
			QSize mainWindowSize = parent->size();
			logWidget->move(mainWindowPosn.x() + 15, mainWindowPosn.y() + mainWindowSize.height() - 200 - 15);
			positioned = true;
		}

		if (!logWidget->isVisible()) // not fure
			logWidget->show();
	}

	void hide()
	{
		logWidget->hide();
	}

	void showError(QString msg)
	{
		addLogMessage(msg, LOGMSG_ERROR);
		if (!logWidget->isVisible()) // not fure
			logWidget->show();
	}

	void addLogMessage(QString msg, eLogMessageType msgType = LOGMSG_INFO)
	{
		QPixmap *pixmap;
		switch (msgType)
		{
		case LOGMSG_WARNING:
			pixmap = &picWarning;
			enableLogAfterTXDLoading = true; // Display log if there's a warning
			break;
		case LOGMSG_ERROR:
			pixmap = &picError;
			enableLogAfterTXDLoading = true; // Display log if there's an error
			break;
		default:
			pixmap = &picInfo;
		}

		QListWidgetItem *item = new QListWidgetItem;
		listWidget->insertItem(0, item);
		listWidget->setItemWidget(item, new LogItemWidget(msgType, *pixmap, msg));
		item->setSizeHint(QSize(listWidget->sizeHintForColumn(0), listWidget->sizeHintForRow(0)));
	}

	void clearLog(void)
	{
		this->listWidget->clear();
	}

	void str_replace(std::string &s, const std::string &search, const std::string &replace)
	{
	    for (size_t pos = 0;; pos += replace.length())
		{
			pos = s.find(search, pos);
			if (pos == std::string::npos) break;
			s.erase(pos, search.length());
			s.insert(pos, replace);
		}
	}

	std::string getLogItemLine(LogItemWidget *itemWidget)
	{
		std::string str = "*** [\"";
		std::string spacing = "\n          ";
		switch (itemWidget->getMessageType())
		{
		case LOGMSG_ERROR:
			spacing += "     ";
			str += "error\"]: ";
			break;
		case LOGMSG_WARNING:
			spacing += "       ";
			str += "warning\"]: ";
			break;
		default:
			spacing += "    ";
			str += "info\"]: ";
		}
		QString message = itemWidget->textLabel->text();
		std::string ansiMessage = message.toStdString();
		str_replace(ansiMessage, "\n", spacing);
		str += ansiMessage;
		str += "\n";
		return str;
	}

	// NOTE - 
	// Windows only
	void strToClipboard(const std::string &s)
	{
		if (OpenClipboard(NULL))
		{
			if (!EmptyClipboard())
			{
				CloseClipboard();
				return;
			}
			HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
			if (!hg)
			{
				CloseClipboard();
				return;
			}
			memcpy(GlobalLock(hg), s.c_str(), s.size() + 1);
			GlobalUnlock(hg);
			SetClipboardData(CF_TEXT, hg);
			CloseClipboard();
			GlobalFree(hg);
		}
	}

	void saveLog(QString fileName)
	{
		// Try to open a file handle.
		const std::wstring unicodeFileName = fileName.toStdWString();
		FILE *file = _wfopen(unicodeFileName.c_str(), L"wt");
		if (file)
		{
			time_t currentTime = time(NULL);
			char timeBuf[1024];
			std::strftime(timeBuf, sizeof(timeBuf), "%A %c", localtime(&currentTime));
			fprintf(file, "Magic.TXD generated log file on %s\ncompiled on %s version: %s\n", timeBuf, __DATE__, MTXD_VERSION_STRING);
			// Go through all log rows and print them.
			int numRows = this->listWidget->count();
			for (int n = numRows - 1; n >= 0; n--)
			{
				// Get message information.
				QListWidgetItem *logItem = (QListWidgetItem*)(this->listWidget->item(n));
				if (logItem)
				{
					LogItemWidget *logWidget = (LogItemWidget *)this->listWidget->itemWidget(logItem);
					std::string str = getLogItemLine(logWidget);
					fputs(str.c_str(), file);
				}
			}
			fclose(file);
		}
		else
		{
			// TODO: display reason why failed.
		}
	}

	void beforeTxdLoading()
	{
		enableLogAfterTXDLoading = false;
	}

	void afterTxdLoading()
	{
		if (enableLogAfterTXDLoading)
			show();
	}

public slots:
void onLogSaveRequest()
{
	QString saveFileName = QFileDialog::getSaveFileName(parent, parent->tr("Save Log as..."), QString(), "Log File (*.txt)");

	if (saveFileName.length() != 0)
	{
		saveLog(saveFileName);
	}
}

void onLogClearRequest()
{
	this->clearLog();
}

void onWindowHideRequest()
{
	this->hide();
}

void onCopyLogLinesRequest()
{
	QList<QListWidgetItem*> selection = listWidget->selectedItems();
	QList<QListWidgetItem *> reversed;
	reversed.reserve(selection.size());
	std::reverse_copy(selection.begin(), selection.end(), std::back_inserter(reversed));
	std::string str;
	foreach(QListWidgetItem *item, reversed)
	{
		LogItemWidget *logWidget = (LogItemWidget *)this->listWidget->itemWidget(item);
		str += getLogItemLine(logWidget);
	}
	strToClipboard(str);
}

void onCopyAllLogLinesRequest()
{
	std::string str;
	int numRows = this->listWidget->count();
	for (int n = numRows - 1; n >= 0; n--)
	{
		// Get message information.
		QListWidgetItem *logItem = (QListWidgetItem*)(this->listWidget->item(n));
		if (logItem)
		{
			LogItemWidget *logWidget = (LogItemWidget *)this->listWidget->itemWidget(logItem);
			str += getLogItemLine(logWidget);
		}
	}
	strToClipboard(str);
}

private:
	QWidget *parent;
	QWidget *logWidget;
	QListWidget *listWidget;

	bool enableLogAfterTXDLoading;
	bool positioned;

	QPixmap picWarning;
	QPixmap picError;
	QPixmap picInfo;
};