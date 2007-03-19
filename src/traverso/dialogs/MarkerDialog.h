/*
    Copyright (C) 2007 Remon Sijrier 
 
    This file is part of Traverso
 
    Traverso is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
 
*/

#ifndef MARKER_DIALOG_H
#define MARKER_DIALOG_H

#include "ui_MarkerDialog.h"
#include <QDialog>

class Project;

class MarkerDialog : public QDialog, protected Ui::MarkerDialog
{
	Q_OBJECT

public:
	MarkerDialog(QWidget* parent = 0);
	~MarkerDialog() {};
	
	
private:
	Project* m_project;

private slots:
	void set_project(Project* project);
	void update_marker_treeview();


};

#endif