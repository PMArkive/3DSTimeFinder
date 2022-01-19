/*
 * This file is part of 3DSTimeFinder
 * Copyright (C) 2019-2022 by Admiral_Fish
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

#ifndef PROFILEMANAGER7_HPP
#define PROFILEMANAGER7_HPP

#include <Core/Util/Utility.hpp>
#include <QWidget>

class ProfileModel7;

namespace Ui
{
    class ProfileManager7;
}

class ProfileManager7 : public QWidget
{
    Q_OBJECT
signals:
    void updateProfiles();

public:
    explicit ProfileManager7(QWidget *parent = nullptr);
    ~ProfileManager7() override;

private:
    Ui::ProfileManager7 *ui;
    ProfileModel7 *model;

    void setupModels();

private slots:
    void create();
    void edit();
    void remove();
};

#endif // PROFILEMANAGER7_HPP
