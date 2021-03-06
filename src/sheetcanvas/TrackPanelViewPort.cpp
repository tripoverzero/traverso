/*
    Copyright (C) 2006 Remon Sijrier 
 
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
 
    $Id: TrackPanelViewPort.cpp,v 1.1 2008/01/21 16:17:30 r_sijrier Exp $
*/

#include "TrackPanelViewPort.h"
		
#include "SheetWidget.h"
#include "SheetView.h"
#include "TrackPanelView.h"
#include <ContextPointer.h>

#include <Debugger.h>


TrackPanelViewPort::TrackPanelViewPort(QGraphicsScene* scene, SheetWidget* sw)
	: ViewPort(scene, sw)
{
	setMinimumWidth(200);
	setMaximumWidth(200);

	m_sw = sw;
	
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void TrackPanelViewPort::get_pointed_context_items(QList<ContextItem* > &list)
{
	QList<QGraphicsItem *> itemlist = items(cpointer().on_first_input_event_x(), cpointer().on_first_input_event_y());
	foreach(QGraphicsItem* item, itemlist) {
		if (ViewItem::is_viewitem(item)) {
			list.append((ViewItem*)item);
		}
	}
	list.append(m_sw->get_sheetview());
}


//eof
