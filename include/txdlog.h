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

class MainWindow;

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
public:
	TxdLog(MainWindow *mainWnd, QString AppPath, QWidget *ParentWidget);

	void show();
	void hide();

    QByteArray saveGeometry( void );
    bool restoreGeometry( const QByteArray& buf );

	void showError(QString msg);

	void addLogMessage(QString msg, eLogMessageType msgType = LOGMSG_INFO);
	void clearLog(void);

private:
	static void str_replace(std::string &s, const std::string &search, const std::string &replace)
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
	static void strToClipboard(const std::string &s)
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

public:
	void saveLog(QString fileName);

	void beforeTxdLoading();
	void afterTxdLoading();

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
    MainWindow *mainWnd;

	QWidget *parent;
	QWidget *logWidget;
	QListWidget *listWidget;

	bool enableLogAfterTXDLoading;
	bool positioned;

	QPixmap picWarning;
	QPixmap picError;
	QPixmap picInfo;
};