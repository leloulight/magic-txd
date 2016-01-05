#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>

void CalculateWindowSize(QWidget *widget, unsigned int baseWidth, unsigned int minBaseWidth, unsigned int maxBaseWidth, unsigned int minHeight, unsigned int baseHeight);

QHBoxLayout *CreateButtonsLayout(QWidget *Parent = 0);

QVBoxLayout *CreateRootLayout(QWidget *Parent = 0);

QHBoxLayout *CreateTopLevelHBoxLayout(QWidget *Parent = 0);

QVBoxLayout *CreateTopLevelVBoxLayout(QWidget *Parent = 0);

QPushButton *CreateButton(QString Text, QWidget *Parent = 0);

QWidget *CreateLine(QWidget *Parent = 0);

template <typename TopLayoutType> struct MagicLayout {
    QVBoxLayout *root;
    TopLayoutType *top;
    QHBoxLayout *bottom;

    MagicLayout(QWidget *Parent = 0) {
        root = CreateRootLayout(Parent);
        top = new TopLayoutType();
        top->setContentsMargins(QMargins(12, 12, 12, 12));
        top->setSpacing(10);
        root->addLayout(top);
        bottom = CreateButtonsLayout();
        QWidget *bottomWidget = new QWidget();
        bottomWidget->setObjectName("background_0");
        bottomWidget->setLayout(bottom);
        root->addWidget(CreateLine());
        root->addWidget(bottomWidget);
    }
};