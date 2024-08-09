#include "DataOptions.hpp"
#include "ui_DataOptions.h"

#include <QMessageBox>

DataOptions::DataOptions(QWidget *parent)
    : QGroupBox(parent)
    , ui(new Ui::DataOptions)
{
    ui->setupUi(this);

    if (!QObject::connect(ui->cbox_show, SIGNAL(stateChanged(int)), this, SLOT(OnCBoxCheckStateChanged(int)))) {
        QMessageBox::critical(this, "Internal Error", "Failed to connect signal checkStateChanged in DataOptions!");
        throw;
    }
    if (!QObject::connect(ui->sbox_window, SIGNAL(valueChanged(int)), this, SLOT(OnSBoxValueChanged(int)))) {
        QMessageBox::critical(this, "Internal Error", "Failed to connect signal valueChanged in DataOptions!");
        throw;
    }
    if (!QObject::connect(ui->dsbox_min, SIGNAL(valueChanged(double)), this, SLOT(OnDBoxMinValueChanged(double)))) {
        QMessageBox::critical(this, "Internal Error", "Failed to connect signal valueChanged (min) in DataOptions!");
        throw;
    }
    if (!QObject::connect(ui->dsbox_max, SIGNAL(valueChanged(double)), this, SLOT(OnDBoxMaxValueChanged(double)))) {
        QMessageBox::critical(this, "Internal Error", "Failed to connect signal valueChanged (max) in DataOptions!");
        throw;
    }

    updateData();
}

DataOptions::~DataOptions()
{
    delete ui;
}

void DataOptions::updateData() {
    m_data.show = ui->cbox_show->isChecked();
    m_data.windowSize = ui->sbox_window->value();
    m_data.cutoffMin = ui->dsbox_min->value();
    m_data.cutoffMax = ui->dsbox_max->value();
}

void DataOptions::OnCBoxCheckStateChanged(int state) {
    m_data.show = (static_cast<Qt::CheckState>(state) == Qt::CheckState::Checked);
    emit optionsChanged(this);
}

void DataOptions::OnSBoxValueChanged(int value) {
    m_data.windowSize = value;
    emit optionsChanged(this);
}

void DataOptions::OnDBoxMinValueChanged(double value) {
    m_data.cutoffMin = value;
    emit optionsChanged(this);
}

void DataOptions::OnDBoxMaxValueChanged(double value) {
    m_data.cutoffMax = value;
    emit optionsChanged(this);
}
