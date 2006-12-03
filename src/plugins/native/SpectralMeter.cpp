/*Copyright (C) 2006 Remon Sijrier

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

$Id: SpectralMeter.cpp,v 1.1 2006/11/27 21:53:42 r_sijrier Exp $

*/


#include "SpectralMeter.h"
#include <AudioBus.h>

#include <Debugger.h>

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"
		


SpectralMeter::SpectralMeter()
	: Plugin()
{
}


SpectralMeter::~SpectralMeter()
{
}


QDomNode SpectralMeter::get_state( QDomDocument doc )
{
	QDomElement node = doc.createElement("Plugin");
	node.setAttribute("type", "SpectralMeterPlugin");
	node.setAttribute("bypassed", is_bypassed());
	return node;
}


int SpectralMeter::set_state(const QDomNode & node )
{
	QDomElement e = node.toElement();
	m_bypass = e.attribute( "bypassed", "0").toInt();
	
	return 1;
}


int SpectralMeter::init()
{
	return 1;
}


void SpectralMeter::process(AudioBus* bus, unsigned long nframes)
{
	if ( is_bypassed() ) {
		return;
	}
}


QString SpectralMeter::get_name( )
{
	return QString(tr("SpectralMeter Meter"));
}

//eof
 
 