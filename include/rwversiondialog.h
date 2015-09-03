#pragma once

#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

struct RwVersionTemplate {
	std::string name;

};



class RwVersionDialog : public QDialog
{
	//Q_OBJECT

public:
	RwVersionDialog() {
		setObjectName("txdOptionsBackground");
		setWindowTitle(tr("TXD Version Setup"));
		setWindowFlags(Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

		QVBoxLayout *verticalLayout = new QVBoxLayout(this);

		QVBoxLayout *topLayout = new QVBoxLayout;
		topLayout->setSpacing(6);
		topLayout->setMargin(10);

		QHBoxLayout *selectGameLayout = new QHBoxLayout;
		QLabel *gameLabel = new QLabel(tr("Game"));
		gameLabel->setObjectName("label25px");
		gameLabel->setFixedWidth(70);
		QComboBox *gameComboBox = new QComboBox;
		gameComboBox->addItem(tr("GTA San Andreas"));
		gameComboBox->addItem(tr("GTA Vice City"));
		gameComboBox->addItem(tr("GTA III"));
		gameComboBox->addItem(tr("Manhunt"));
		gameComboBox->addItem(tr("Custom"));

		selectGameLayout->addWidget(gameLabel);
		selectGameLayout->addWidget(gameComboBox);

		QHBoxLayout *versionLayout = new QHBoxLayout;
		QLabel *versionLabel = new QLabel(tr("Version"));
		versionLabel->setObjectName("label25px");

		QHBoxLayout *versionNumbersLayout = new QHBoxLayout;
		QLineEdit *versionLine1 = new QLineEdit;
		//versionLine1->setValidator(new QIntValidator(3, 6, this));
		versionLine1->setInputMask("0.00.00.00");
		//versionLine1->setFixedWidth(24);
		//versionLine1->setAlignment(Qt::AlignCenter);
		//QLineEdit *versionLine2 = new QLineEdit;
		//versionLine2->setInputMask("00");
		//versionLine2->setValidator(new QIntValidator(0, 15, this));
		//versionLine2->setFixedWidth(24);
		//versionLine2->setAlignment(Qt::AlignCenter);
		//QLineEdit *versionLine3 = new QLineEdit;
		//versionLine3->setInputMask("00");
		//versionLine3->setValidator(new QIntValidator(0, 15, this));
		//versionLine3->setFixedWidth(24);
		//versionLine3->setAlignment(Qt::AlignCenter);
		//QLineEdit *versionLine4 = new QLineEdit;
		//versionLine4->setInputMask("00");
		//versionLine4->setValidator(new QIntValidator(0, 63, this));
		//versionLine4->setFixedWidth(24);
		//versionLine4->setAlignment(Qt::AlignCenter);

		versionNumbersLayout->addWidget(versionLine1);
		//versionNumbersLayout->addWidget(versionLine2);
		//versionNumbersLayout->addWidget(versionLine3);
		//versionNumbersLayout->addWidget(versionLine4);
		//versionNumbersLayout->setSpacing(2);
		versionNumbersLayout->setMargin(0);

		QLabel *buildLabel = new QLabel(tr("Build"));
		buildLabel->setObjectName("label25px");
		QLineEdit *buildLine = new QLineEdit;
		buildLine->setInputMask("HHHH");
		buildLine->setFixedWidth(50);

		versionLayout->addWidget(versionLabel);
		versionLayout->addLayout(versionNumbersLayout);
		versionLayout->addWidget(buildLabel);
		versionLayout->addWidget(buildLine);

		topLayout->addLayout(selectGameLayout);
		topLayout->addSpacing(8);
		topLayout->addLayout(versionLayout);

		QWidget *line = new QWidget();
		line->setFixedHeight(1);
		line->setObjectName("hLineBackground");

		QHBoxLayout *buttonsLayout = new QHBoxLayout;
		QPushButton *buttonAccept = new QPushButton(tr("Accept"));
		QPushButton *buttonCancel = new QPushButton(tr("Cancel"));

		buttonsLayout->addWidget(buttonAccept);
		buttonsLayout->addWidget(buttonCancel);
		buttonsLayout->setAlignment(Qt::AlignRight);
		buttonsLayout->setMargin(10);
		buttonsLayout->setSpacing(6);
		
		verticalLayout->addLayout(topLayout);
		verticalLayout->addSpacing(12);
		verticalLayout->addWidget(line);
		verticalLayout->addLayout(buttonsLayout);
		verticalLayout->setSpacing(0);
		verticalLayout->setMargin(0);

		verticalLayout->setSizeConstraint(QLayout::SetFixedSize);
	}

	void Show() {
		exec();
	}
};