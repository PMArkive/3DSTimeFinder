/*
 * This file is part of 3DSTimeFinder
 * Copyright (C) 2019-2020 by Admiral_Fish
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "Event7.hpp"
#include "ui_Event7.h"
#include <Core/Gen7/EventSearcher7.hpp>
#include <Core/Util/Utility.hpp>
#include <Forms/Gen7/ProfileManager7.hpp>
#include <Models/EventModel.hpp>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QtConcurrent>

Event7::Event7(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Event7)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);

    setupModel();
    updateProfiles();

    qRegisterMetaType<QVector<EventResult>>("QVector<EventResult>");
}

Event7::~Event7()
{
    QSettings settings;
    settings.setValue("event7/geometry", this->saveGeometry());

    delete ui;
}

void Event7::updateProfiles()
{
    profiles = Profile7::loadProfileList();
    profiles.insert(profiles.begin(), Profile7());

    QSettings setting;
    int val = setting.value("event7/profile", 0).toInt();

    ui->comboBoxProfiles->clear();

    for (const auto &profile : profiles)
    {
        ui->comboBoxProfiles->addItem(profile.getName());
    }

    if (val < ui->comboBoxProfiles->count())
    {
        ui->comboBoxProfiles->setCurrentIndex(val);
    }
}

void Event7::setupModel()
{
    model = new EventModel(ui->tableView);

    ui->tableView->setModel(model);

    auto natures = Utility::getNatures();
    auto hiddenPowers = Utility::getHiddenPowers();

    ui->comboBoxNature->setup(natures);
    ui->comboBoxNatureLocked->addItems(natures);
    ui->comboBoxHiddenPower->setup(hiddenPowers);

    ui->labelPID->setVisible(false);
    ui->textBoxPID->setVisible(false);
    ui->labelEC->setVisible(false);
    ui->textBoxEC->setVisible(false);

    ui->comboBoxPIDType->setItemData(0, PIDType::Random);
    ui->comboBoxPIDType->setItemData(1, PIDType::Nonshiny);
    ui->comboBoxPIDType->setItemData(2, PIDType::Shiny);
    ui->comboBoxPIDType->setItemData(3, PIDType::Specified);

    ui->comboBoxAbility->setup({ 255, 0, 1, 2 });
    ui->comboBoxGender->setup({ 255, 0, 2 });

    ui->textBoxStartFrame->setValues(InputType::Frame32Bit);
    ui->textBoxEndFrame->setValues(InputType::Frame32Bit);
    ui->textBoxTID->setValues(InputType::ID);
    ui->textBoxSID->setValues(InputType::ID);
    ui->textBoxPID->setValues(InputType::Seed32Bit);
    ui->textBoxEC->setValues(InputType::Seed32Bit);

    ui->dateTimeEditStartDate->setCalendarPopup(true);
    ui->dateTimeEditEndDate->setCalendarPopup(true);

    QDateTime dt(QDate(2000, 1, 1), QTime(0, 0, 0));
    ui->dateTimeEditStartDate->setMinimumDateTime(dt);
    ui->dateTimeEditEndDate->setMinimumDateTime(dt);

    connect(ui->pushButtonSearch, &QPushButton::clicked, this, &Event7::search);
    connect(ui->pushButtonProfileManager, &QPushButton::clicked, this, &Event7::profileManager);
    connect(
        ui->comboBoxProfiles, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Event7::profilesIndexChanged);
    connect(ui->checkBoxAbilityLock, &QCheckBox::clicked, this, &Event7::checkBoxAbilityLock);
    connect(ui->comboBoxPIDType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Event7::comboBoxPIDTypeIndexChanged);

    QSettings setting;
    if (setting.contains("event7/geometry"))
    {
        this->restoreGeometry(setting.value("event7/geometry").toByteArray());
    }
}

void Event7::search()
{
    QDateTime start = ui->dateTimeEditStartDate->dateTime();
    start.setTimeSpec(Qt::UTC);
    QDateTime end = ui->dateTimeEditEndDate->dateTime();
    start.setTimeSpec(Qt::UTC);
    u32 frameStart = ui->textBoxStartFrame->getUInt();
    u32 frameEnd = ui->textBoxEndFrame->getUInt();

    if (start > end)
    {
        QMessageBox error;
        error.setText(tr("Set end date to be after start date."));
        error.exec();
        return;
    }
    if (frameStart > frameEnd)
    {
        QMessageBox error;
        error.setText(tr("Set end frame to be after start frame"));
        error.exec();
        return;
    }

    model->clearModel();
    ui->pushButtonSearch->setEnabled(false);
    ui->pushButtonCancel->setEnabled(true);

    std::array<u8, 6> min = ui->ivFilter->getLower();
    std::array<u8, 6> max = ui->ivFilter->getUpper();

    u8 ivCount = static_cast<u8>(ui->spinBoxRandomIVs->value());
    auto type = static_cast<PIDType>(ui->comboBoxPIDType->getCurrentByte());

    EventFilter filter(min, max, ui->comboBoxNature->getChecked(), ui->comboBoxHiddenPower->getChecked(),
                       ui->comboBoxAbility->getCurrentByte(), ui->checkBoxShiny->isChecked(), ui->comboBoxGender->getCurrentByte());

    std::array<u8, 6> ivTemplate;
    ivTemplate[0] = ui->checkBoxHP->isChecked() ? static_cast<u8>(ui->spinBoxHP->value()) : 255;
    ivTemplate[1] = ui->checkBoxAtk->isChecked() ? static_cast<u8>(ui->spinBoxAtk->value()) : 255;
    ivTemplate[2] = ui->checkBoxDef->isChecked() ? static_cast<u8>(ui->spinBoxDef->value()) : 255;
    ivTemplate[3] = ui->checkBoxSpA->isChecked() ? static_cast<u8>(ui->spinBoxSpA->value()) : 255;
    ivTemplate[4] = ui->checkBoxSpD->isChecked() ? static_cast<u8>(ui->spinBoxSpD->value()) : 255;
    ivTemplate[5] = ui->checkBoxSpe->isChecked() ? static_cast<u8>(ui->spinBoxSpe->value()) : 255;

    auto *searcher
        = new EventSearcher7(start, end, frameStart, frameEnd, ivCount, type, profiles.at(ui->comboBoxProfiles->currentIndex()), filter);
    searcher->setLocks(ui->checkBoxAbilityLock->isChecked(), static_cast<u8>(ui->comboBoxAbilityLocked->currentIndex()),
                       ui->checkBoxNatureLocked->isChecked(), static_cast<u8>(ui->comboBoxNatureLocked->currentIndex()),
                       ui->checkBoxGenderLocked->isChecked(), static_cast<u8>(ui->comboBoxGenderLocked->currentIndex()));
    searcher->setIDs(ui->checkBoxOtherInfo->isChecked(), ui->textBoxTID->getUShort(), ui->textBoxSID->getUShort(),
                     ui->checkBoxYourID->isChecked());
    searcher->setHidden(ui->textBoxPID->getUInt(), ui->textBoxEC->getUInt());
    searcher->setIVTemplate(ivTemplate);
    connect(ui->pushButtonCancel, &QPushButton::clicked, this, [=] { searcher->cancelSearch(); });

    ui->progressBar->setRange(0, searcher->getMaxProgress());

    auto *timer = new QTimer();
    connect(timer, &QTimer::timeout, [=] {
        ui->progressBar->setValue(searcher->getProgress());
        model->addItems(searcher->getResults());
    });

    auto *watcher = new QFutureWatcher<void>();
    connect(watcher, &QFutureWatcher<void>::finished, watcher, &QFutureWatcher<void>::deleteLater);
    connect(watcher, &QFutureWatcher<void>::destroyed, this, [=] {
        ui->pushButtonSearch->setEnabled(true);
        ui->pushButtonCancel->setEnabled(false);

        timer->stop();
        delete timer;

        ui->progressBar->setValue(searcher->getProgress());
        model->addItems(searcher->getResults());

        delete searcher;
    });

    QSettings settings;
    int threads = settings.value("settings/threads", QThread::idealThreadCount()).toInt();

    auto future = QtConcurrent::run([=] { searcher->startSearch(threads); });

    watcher->setFuture(future);
    timer->start(1000);
}

void Event7::profileManager()
{
    auto *manager = new ProfileManager7();
    connect(manager, &ProfileManager7::updateProfiles, [=]() { emit alertProfiles(7); });
    manager->show();
    manager->raise();
}

void Event7::profilesIndexChanged(int index)
{
    if (index >= 0)
    {
        auto profile = profiles.at(index);
        ui->labelProfileOffsetValue->setText(QString::number(profile.getOffset()));
        ui->labelProfileTickValue->setText(QString::number(profile.getTick(), 16));
        ui->labelProfileTIDValue->setText(QString::number(profile.getTID()));
        ui->labelProfileSIDValue->setText(QString::number(profile.getSID()));
        ui->labelProfileGameValue->setText(profile.getVersionString());
        ui->labelProfileShinyCharmValue->setText(profile.getShinyCharm() ? "Yes" : "No");

        QSettings setting;
        setting.setValue("event7/profile", index);
    }
}

void Event7::checkBoxAbilityLock(bool checked)
{
    ui->comboBoxAbilityLocked->clear();
    if (checked)
    {
        ui->comboBoxAbilityLocked->addItem("-");
        ui->comboBoxAbilityLocked->addItem("1");
        ui->comboBoxAbilityLocked->addItem("2");
        ui->comboBoxAbilityLocked->addItem("H");
    }
    else
    {
        ui->comboBoxAbilityLocked->addItem("1/2");
        ui->comboBoxAbilityLocked->addItem("1/2/H");
    }
}

void Event7::comboBoxPIDTypeIndexChanged(int index)
{
    ui->labelPID->setVisible(index == 3);
    ui->textBoxPID->setVisible(index == 3);
}

void Event7::importEvent()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, "Select a wondercard file", QDir::currentPath(), "Wondercard (*.wc7 *.wc7full)");
    if (!fileName.isEmpty())
    {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly))
        {
            QByteArray data = file.readAll();
            file.close();
            if (data.size() != 784 && data.size() != 264)
            {
                QMessageBox error;
                error.setText("Invalid format for wondercard");
                error.exec();
                return;
            }

            if (data.size() == 784)
            {
                data = data.mid(520);
            }

            if (data.at(0x51) != 0)
            {
                QMessageBox error;
                error.setText("Invalid wondercard type");
                error.exec();
                return;
            }

            ui->checkBoxAbilityLock->setChecked(data.at(0xA2) < 3);
            checkBoxAbilityLock(ui->checkBoxAbilityLock->isChecked());
            ui->comboBoxAbilityLocked->setCurrentIndex(ui->checkBoxAbilityLock->isChecked() ? data.at(0xA2) + 1 : data.at(0xA2) - 3);

            ui->checkBoxNatureLocked->setChecked(static_cast<u8>(data.at(0xA0)) != 0xFF);
            ui->comboBoxNatureLocked->setCurrentIndex(ui->checkBoxNatureLocked->isChecked() ? data.at(0xA0) : 0);

            ui->checkBoxGenderLocked->setChecked(data.at(0xA1) != 3);
            ui->comboBoxGenderLocked->setCurrentIndex(ui->checkBoxGenderLocked->isChecked() ? (data.at(0xA1) + 1) % 3 : 0);

            QVector<u8> ivs;
            for (u8 order : { 0, 1, 2, 4, 5, 3 })
            {
                u8 iv = data.at(0xAF + order);
                ivs.append(iv);
            }

            u8 ivFlag = 0;
            for (u8 iv : ivs)
            {
                u8 value = iv - 0xFC;
                if (value < 3)
                {
                    ivFlag = iv;
                    break;
                }
            }

            ui->spinBoxRandomIVs->setValue(ivFlag == 0 ? 0 : ivFlag - 0xFB);

            ui->checkBoxHP->setChecked(ivFlag == 0 && ivs.at(0) <= 31);
            ui->spinBoxHP->setValue(ui->checkBoxHP->isChecked() ? ivs.at(0) : 0);

            ui->checkBoxAtk->setChecked(ivFlag == 0 && ivs.at(1) <= 31);
            ui->spinBoxAtk->setValue(ui->checkBoxAtk->isChecked() ? ivs.at(1) : 0);

            ui->checkBoxDef->setChecked(ivFlag == 0 && ivs.at(2) <= 31);
            ui->spinBoxDef->setValue(ui->checkBoxDef->isChecked() ? ivs.at(2) : 0);

            ui->checkBoxSpA->setChecked(ivFlag == 0 && ivs.at(3) <= 31);
            ui->spinBoxSpA->setValue(ui->checkBoxSpA->isChecked() ? ivs.at(3) : 0);

            ui->checkBoxSpD->setChecked(ivFlag == 0 && ivs.at(4) <= 31);
            ui->spinBoxSpD->setValue(ui->checkBoxSpD->isChecked() ? ivs.at(4) : 0);

            ui->checkBoxSpe->setChecked(ivFlag == 0 && ivs.at(5) <= 31);
            ui->spinBoxSpe->setValue(ui->checkBoxSpe->isChecked() ? ivs.at(5) : 0);

            ui->checkBoxOtherInfo->setChecked(true);
            u16 tid = (static_cast<u16>(data.at(0x69)) << 8) | static_cast<u8>(data.at(0x68));
            ui->textBoxTID->setText(QString::number(tid));
            u16 sid = (static_cast<u16>(data.at(0x6B)) << 8) | static_cast<u8>(data.at(0x6A));
            ui->textBoxSID->setText(QString::number(sid));

            QVector<u8> typeOrder = { 3, 0, 2, 1 };
            ui->comboBoxPIDType->setCurrentIndex(typeOrder.at(data.at(0xA3)));
            if (ui->comboBoxPIDType->currentIndex() == 3)
            {
                u32 pid = (data.at(0xD7) << 24) | (data.at(0xD6) << 16) | (data.at(0xD5) << 8) | data.at(0xD4);
                ui->textBoxPID->setText(QString::number(pid, 16));
            }

            u32 ec = (data.at(0x73) << 24) | (data.at(0x72) << 16) | (data.at(0x71) << 8) | data.at(0x70);
            ui->textBoxEC->setText(QString::number(ec, 16));
            ui->labelEC->setVisible(ec > 0);
            ui->textBoxEC->setVisible(ec > 0);

            ui->checkBoxYourID->setChecked(data.at(0xB5) == 3);
            ui->checkBoxEgg->setChecked(data.at(0xD1) == 1);
        }
        else
        {
            QMessageBox error;
            error.setText("There was a problem opening the wondercard");
            error.exec();
            return;
        }
    }
}
