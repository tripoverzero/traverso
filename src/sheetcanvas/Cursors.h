/*
    Copyright (C) 2005-2007 Remon Sijrier 
 
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

#ifndef CURSORS_H
#define CURSORS_H

#include "ViewItem.h"
#include <QTimer>
#include <QTimeLine>

class Sheet;
class SheetView;
class ClipsViewPort;
		
class PlayHead : public ViewItem
{
        Q_OBJECT

public:
        PlayHead(SheetView* sv, Sheet* sheet, ClipsViewPort* vp);
        ~PlayHead();

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void set_bounding_rect(QRectF rect);
	
	bool is_active();
	void set_active(bool active);
	
	enum PlayHeadMode {
		FLIP_PAGE,
		CENTERED,
		ANIMATED_FLIP_PAGE
	};
	
	void set_mode(PlayHeadMode mode);
	void toggle_follow();

private:
	Sheet*		m_sheet;
        QTimer		m_playTimer;
        QTimeLine	m_animation;
        ClipsViewPort*	m_vp;
        bool 		m_follow;
	bool		m_followDisabled;
        PlayHeadMode	m_mode;
        int 		m_animationScrollStartPos;
	int		m_animFrameRange;
	qreal		m_animScaleFactor;
	
private slots:
	void check_config();
        void play_start();
        void play_stop();
        void set_animation_value(int);
        void animation_finished();
        
public slots:
        void update_position();
        void enable_follow();  // enable/disable follow only do anything if following is
        void disable_follow(); // enabled in the config
};



class WorkCursor : public ViewItem
{
        Q_OBJECT

public:
        WorkCursor(SheetView* sv, Sheet* sheet);
        ~WorkCursor();

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void set_bounding_rect(QRectF rect);

private:
	Sheet*		m_sheet;
	SheetView*	m_sv;
	QPixmap		m_pix;
	
	void update_background();

public slots:
        void update_position();
};


#endif

//eof
