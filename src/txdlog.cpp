#include "mainwindow.h"

TxdLog::TxdLog(MainWindow *mainWnd, QString AppPath, QWidget *ParentWidget)
{
    this->mainWnd = mainWnd;

	parent = ParentWidget;
	positioned = false;
	logWidget = new QWidget(ParentWidget, Qt::Window);
	logWidget->setWindowTitle("Log");
	logWidget->setMinimumSize(450, 150);
	logWidget->resize(500, 200);
	logWidget->setObjectName("background_1");
	/* --- Top panel --- */
	QPushButton *buttonSave = new QPushButton("Save");

	connect(buttonSave, &QPushButton::clicked, this, &TxdLog::onLogSaveRequest);

	buttonSave->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	QPushButton *buttonCopy = new QPushButton("Copy");

	connect(buttonCopy, &QPushButton::clicked, this, &TxdLog::onCopyLogLinesRequest);

	buttonSave->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	QPushButton *buttonCopyAll = new QPushButton("Copy All");

	connect(buttonCopyAll, &QPushButton::clicked, this, &TxdLog::onCopyAllLogLinesRequest);

	buttonCopyAll->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	QPushButton *buttonClear = new QPushButton("Clear");

	connect(buttonClear, &QPushButton::clicked, this, &TxdLog::onLogClearRequest);

	buttonClear->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	QPushButton *buttonClose = new QPushButton("Close");

	connect(buttonClose, &QPushButton::clicked, this, &TxdLog::onWindowHideRequest);

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

    // Still not fure, but looks like weird bug with scrollbars was fixed in Qt5.x
	//listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setMargin(0);
	mainLayout->setSpacing(0);
	logWidget->setLayout(mainLayout);

	// icons
    // NOTE: the Qt libpng implementation complains about a "known invalid sRGB profile" here.
	picWarning.load(AppPath + "\\resources\\warning.png");
	picError.load(AppPath + "\\resources\\error.png");
	picInfo.load(AppPath + "\\resources\\info.png");
}

void TxdLog::show( void )
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

void TxdLog::hide( void )
{
	logWidget->hide();
}

QByteArray TxdLog::saveGeometry( void )
{
    return this->logWidget->saveGeometry();
}

bool TxdLog::restoreGeometry( const QByteArray& buf )
{
    bool restored = this->logWidget->restoreGeometry( buf );

    if ( restored )
    {
        this->positioned = true;
    }

    return restored;
}

void TxdLog::showError( QString msg )
{
	addLogMessage(msg, LOGMSG_ERROR);
	if (!logWidget->isVisible()) // not fure
		logWidget->show();
}

void TxdLog::addLogMessage( QString msg, eLogMessageType msgType )
{
	QPixmap *pixmap;
	switch (msgType)
	{
	case LOGMSG_WARNING:
		pixmap = &picWarning;
        if ( mainWnd->showLogOnWarning )
        {
		    enableLogAfterTXDLoading = true; // Display log if there's a warning
        }
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

void TxdLog::clearLog( void )
{
	this->listWidget->clear();
}

void TxdLog::saveLog( QString fileName )
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

void TxdLog::beforeTxdLoading( void )
{
	enableLogAfterTXDLoading = false;
}

void TxdLog::afterTxdLoading( void )
{
	if (enableLogAfterTXDLoading)
		show();
}