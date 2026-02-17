// src/ui/StatusBar.h
//
// StatusBar – minimal stub (functionality moved to StatusLog).

#pragma once

#include <QWidget>

class StatusBar : public QWidget
{
    Q_OBJECT
public:
    explicit StatusBar(QWidget* parent = nullptr) : QWidget(parent) {}
};
