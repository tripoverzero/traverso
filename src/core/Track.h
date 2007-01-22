/*
Copyright (C) 2005-2006 Remon Sijrier 

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

$Id: Track.h,v 1.20 2007/01/22 15:12:08 r_sijrier Exp $
*/


#ifndef TRACK_H
#define TRACK_H

#include <QString>
#include <QDomDocument>
#include <QList>
#include <QByteArray>

#include "ContextItem.h"
#include "AudioClipList.h"

#include "defines.h"

class AudioClip;
class Song;
class PluginChain;
class Plugin;


class Track : public ContextItem
{
	Q_OBJECT
	Q_CLASSINFO("mute", tr("Mute"))
	Q_CLASSINFO("toggle_arm", tr("Record: On/Off"))
	Q_CLASSINFO("solo", tr("Solo"))
	Q_CLASSINFO("gain", tr("Gain"))
	Q_CLASSINFO("pan", tr("Panorama"))
	Q_CLASSINFO("reset_gain", tr("Gain: Reset"))
	Q_CLASSINFO("import_audiosource", tr("Import audio"))
	Q_CLASSINFO("silence_others", tr("Silence other tracks"))
	Q_CLASSINFO("remove_myself", tr("Remove"))

public :
	Track(Song* pParentSong, int pID, const QString& pName, int pBaseY, int pHeight);
	Track(Song* song, const QDomNode node);
	~Track();

	static const int INITIAL_HEIGHT = 100;
	static const int MINIMUM_HEIGHT = 25;

	void activate();
	void deactivate();
	bool is_pointed(int y);

	Command* add_clip(AudioClip* clip, bool historable=true);
	Command* add_plugin(Plugin* plugin);

	Command* remove_clip(AudioClip* clip, bool historable=true);
	Command* remove_plugin(Plugin* plugin);
	
	Command* init_recording();
	
	int arm();
	int disarm();
	void toggle_active();

	AudioClip* get_clip_under(nframes_t framePos);
	AudioClip* get_clip_after(nframes_t framePos);
	AudioClip* get_clip_before(nframes_t framePos);
	AudioClip* get_clip_between(nframes_t framePosL, nframes_t framePosR);

	QList<AudioClip* > split_clip(nframes_t splitPoint);
	QList<AudioClip* > split_clip(AudioClip* c, nframes_t splitPoint);

	void get_render_range(nframes_t& startframe, nframes_t& endframe);


	// Get functions:
	int get_id() const
	{
		return ID;
	}
	QString get_bus_in() const
	{
		return busIn;
	}
	QString get_bus_out() const
	{
		return busOut;
	}
	int get_baseY() const
	{
		return baseY;
	}
	int get_height() const
	{
		return height;
	}
	int real_baseY() const;
	float get_pan() const
	{
		return m_pan;
	}
	Song* get_song() const
	{
		return m_song;
	}
	QString get_name() const
	{
		return m_name;
	}
	int get_total_clips();
	QDomNode get_state(QDomDocument doc);
	AudioClipList get_cliplist() const
	{
		return audioClipList;
	}
	
	PluginChain* get_plugin_chain() const
	{
		return pluginChain;
	}
	
	int get_sort_index() const;
	
	// End get functions

	// Set functions:
	void set_bus_out(QByteArray bus);
	void set_bus_in(QByteArray bus);
	void set_muted_by_solo(bool muted);
	void set_name(const QString& name);
	void set_solo(bool solo);
	void set_muted(bool muted);
	void set_pan(float pan);
	void set_baseY(int b);
	void set_sort_index(int index);
	void set_height(int h);
	int set_state( const QDomNode& node );
	void set_id(int id);
	// End set functions


	//Bool functions:
	bool is_muted();
	bool is_muted_by_solo();
	bool is_solo();
	bool is_active();
	bool armed();
	// End bool functions


	void vzoom_in(int newBaseY);
	void vzoom_out(int newBaseY);

	int process(nframes_t nframes);

private :
	Song*		m_song;
	AudioClipList 	audioClipList;
	PluginChain*	pluginChain;

	float m_gain;
	float m_pan;
	int ID;
	int numtakes;
	QByteArray busIn;
	QByteArray busOut;

	QString m_name;

	int baseY;
	int m_sortIndex;
	int height;

	bool isActive;
	bool isSolo;
	bool isMuted;
	bool isArmed;
	bool mutedBySolo;

	void set_armed(bool armed);
	void init();

signals:
	void audioClipAdded(AudioClip* clip);
	void audioClipRemoved(AudioClip* clip);
	void heightChanged();
	void muteChanged(bool isMuted);
	void soloChanged(bool isSolo);
	void armedChanged(bool isArmed);
	void lockChanged(bool isLocked);
	void isActiveChanged(bool isActive);
	void gainChanged();
	void panChanged();
	void stateChanged();
	void audibleStateChanged();
	void inBusChanged();
	void outBusChanged();

public slots:
	void set_gain(float gain);
	
	float get_gain() const;

	Command* mute();
	Command* toggle_arm();
	Command* solo();
	Command* gain();
	Command* pan();
	Command* reset_gain();
	Command* import_audiosource();
	Command* silence_others();
	Command* remove_myself();

private slots:
	void private_add_clip(AudioClip* clip);
	void private_remove_clip(AudioClip* clip);

};

#endif

