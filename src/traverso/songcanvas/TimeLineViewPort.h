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
 
    $Id: TimeLineViewPort.h,v 1.2 2007/01/16 13:51:23 r_sijrier Exp $
*/

#ifndef TIME_LINE_VIEW_PORT_H
#define TIME_LINE_VIEW_PORT_H

#include "ViewPort.h"

class SongWidget;
class SongView;
class TimeLineView;
class ClipsViewPort;
		
class TimeLineViewPort : public ViewPort
{
public:
	TimeLineViewPort(QGraphicsScene* scene, SongWidget* sw, ClipsViewPort* clipView);
	~TimeLineViewPort() {};
	
	void set_songview(SongView* view);
	
	void get_pointed_context_items(QList<ContextItem* > &list);

protected:
	void wheelEvent ( QWheelEvent * e );

private:
	ClipsViewPort*	clipView;
	TimeLineView* 	m_timeLineView;
};

#endif

//eof
