#pragma once
#ifndef _SKIVVY_IRCBOT_RQUOTE_H_
#define _SKIVVY_IRCBOT_RQUOTE_H_
/*
 * ircbot-rquote.h
 *
 *  Created on: 29 Jul 2011
 *      Author: oaskivvy@gmail.com
 */

/*-----------------------------------------------------------------.
| Copyright (C) 2011 SooKee oaskivvy@gmail.com               |
'------------------------------------------------------------------'

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.

http://www.gnu.org/licenses/gpl-2.0.html

'-----------------------------------------------------------------*/

#include <skivvy/ircbot.h>
#include <skivvy/types.h>

namespace skivvy { namespace rquotes {

using namespace skivvy::types;
using namespace skivvy::ircbot;

class RQuotesIrcBotPlugin
: public IrcBotPlugin
{
private:
	class RandomQuoteIrcBotPluginRep& rep;

public:
	RQuotesIrcBotPlugin(IrcBot& bot);
	virtual ~RQuotesIrcBotPlugin();

	// INTERFACE: IrcBotPlugin

	virtual bool init();

	virtual std::string get_id() const;
	virtual std::string get_name() const;
	virtual std::string get_version() const;

	virtual command_list list() const;
	virtual void execute(const std::string& cmd, const message& msg);
	virtual std::string help(const std::string& cmd) const;
	virtual void exit();
};

}} // skivvy::rquotes

#endif // _SKIVVY_IRCBOT_RQUOTE_H_
