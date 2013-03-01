/*
 * ircbot-rquote.cpp
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

#include <skivvy/plugin-rquotes.h>

#include <mutex>
#include <future>
#include <thread>
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>

#include <pcrecpp.h>

#include <sookee/str.h>

#include <skivvy/types.h>
#include <skivvy/logrep.h>
#include <skivvy/utils.h>
#include <skivvy/store.h>

namespace skivvy { namespace rquotes {

IRC_BOT_PLUGIN(RQuotesIrcBotPlugin);
PLUGIN_INFO("rquotes", "Random Quotes", "0.1");

using namespace skivvy::types;
using namespace skivvy::utils;
using namespace skivvy::ircbot;
using namespace sookee::string;

using namespace pcrecpp;

const str RQUOTES_STORE = "rquotes.store.file";
const str RQUOTES_STORE_DEFAULT = "rquotes-store.txt";

const str QUOTE_MINDELAY = "rquotes.quote.mindelay";
const delay QUOTE_MINDELAY_DEFAULT = 1 * 60;
const str QUOTE_MAXDELAY = "rquotes.quote.maxdelay";
const delay QUOTE_MAXDELAY_DEFAULT = 60 * 60;

const str JOKE_MINDELAY = "rquotes.joke.mindelay";
const delay JOKE_MINDELAY_DEFAULT = 1 * 60;
const str JOKE_MAXDELAY = "rquotes.joke.maxdelay";
const delay JOKE_MAXDELAY_DEFAULT = 60 * 60;


class RandomQuoteIrcBotPluginRep
: public BasicIrcBotPlugin
{
	typedef std::set<std::string> channel_set;

	BackupStore store;

	channel_set channels;
	std::mutex channels_mtx;

	RandomTimer joke_timer;
	RandomTimer quote_timer;

	// jokes
	std::string joke();
	std::string clean_joke();
	void jokes_on(const message& msg);
	void jokes_off(const message& msg);

	// quotes
	std::string quote();
	void quotes_on(const message& msg);
	void quotes_off(const message& msg);



public:
	RandomQuoteIrcBotPluginRep(IrcBot& bot);
	virtual ~RandomQuoteIrcBotPluginRep();

	void set_delay(size_t maxtime);

	// INTERFACE: BasicIrcBotPlugin

	virtual bool initialize();

	// INTERFACE: IrcBotPlugin

	virtual std::string get_id() const;
	virtual std::string get_name() const;
	virtual std::string get_version() const;
	virtual void exit();

	void instant_quote(const message& msg);
	void instant_joke(const message& msg);
};

str log_report(const str& msg)
{
	if(!msg.empty())
		log(msg);
	return "";
}

// TODO: Fix when quote is too long for one IRC message. Split into multiples
std::string RandomQuoteIrcBotPluginRep::quote()
{

	const str POST_DATA = "number=4"
		"&collection%5B%5D=mgm"
		"&collection%5B%5D=motivate"
		"&collection%5B%5D=classic"
		"&collection%5B%5D=coles"
		"&collection%5B%5D=lindsly"
		"&collection%5B%5D=poorc"
		"&collection%5B%5D=altq"
		"&collection%5B%5D=20thcent"
		"&collection%5B%5D=bywomen"
		"&collection%5B%5D=devils"
		"&collection%5B%5D=contrib";

	net::socketstream ss;
	ss.open("www.quotationspage.com", 80);
	ss << "POST /random.php3 HTTP/1.1" << "\r\n";
	ss << "Host: www.quotationspage.com" << "\r\n";
	ss << "User-Agent: Skivvy rquotes: v" << VERSION << "\r\n";
	ss << "Accept: text/html,application/xhtml+xml,application/xml" << "\r\n";
	ss << "Referer: http://www.quotationspage.com/random.php3" << "\r\n";
	ss << "Connection: close" << "\r\n";
	ss << "Content-Type: application/x-www-form-urlencoded" << "\r\n";
	ss << "Content-Length: " << POST_DATA.size() << "\r\n";
	ss << "\r\n";
	ss << POST_DATA << std::flush;

	char c;
	soss oss;
	while(ss.get(c))
		oss.put(c);

	if(bot.has("rquotes.dump.file"))
		sofs(bot.getf("rquotes.dump.file")) << oss.str();

	RE re("");
	RE_Options ops;
	ops.set_dotall(true);
	ops.set_caseless(true);

	str q, a, q_a;

	q_a = oss.str();
	for(const str& pcre: bot.get_vec("rquotes.quote.pcre.q_a"))
		if(!(re = RE(pcre, ops)).PartialMatch(q_a, &q_a))
			return log_report(re.error());
	trim(q_a);

	q = q_a;
	for(const str& pcre: bot.get_vec("rquotes.quote.pcre.q"))
		if(!(re = RE(pcre, ops)).PartialMatch(q, &q))
			return log_report(re.error());
	trim(q);

	a = q_a;
	for(const str& pcre:  bot.get_vec("rquotes.quote.pcre.a"))
		if(!(re = RE(pcre, ops)).PartialMatch(a, &a))
			return log_report(re.error());
	trim(a);

	return q + " - " + a;
}

std::string RandomQuoteIrcBotPluginRep::joke()
{
	static std::string j;
	static std::mutex mtx;
	static time_t prev = 0;

	lock_guard lock(mtx);

	if(time(0) - prev > 2)
	{
		net::socketstream ss;
		ss.open("onelinerz.net", 80);
		ss << "GET " << "/random-one-liners/1/ HTTP/1.1\r\n";
		ss << "Host: www.onelinerz.net" << "\r\n";
		ss << "User-Agent: Skivvy rquotes: v" << VERSION << "\r\n";
		ss << "Accept: text/html" << "\r\n";
		ss << "\r\n" << std::flush;

		char c;
		soss oss;
		while(ss.get(c))
			oss.put(c);

		if(bot.has("rquotes.dump.file"))
			sofs(bot.getf("rquotes.dump.file")) << oss.str();

		RE re("");
		RE_Options ops;
		ops.set_dotall(true);
		ops.set_caseless(true);

		j = oss.str();
		for(const str& pcre: bot.get_vec("rquotes.joke.pcre"))
			if(!(re = RE(pcre, ops)).PartialMatch(j, &j))
				return log_report(re.error());
		trim(j);

		prev = time(0);
	}

	return j.empty() ? "Sorry but I'm not feeling vary funny right now." : j;
}

std::string get_clean_joke()
{
	std::string j;
	net::socketstream ss;
	ss.open("jokesclean.com", 80);
	ss << "GET " << "/OneLiner/Random/ HTTP/1.1\r\n";
	ss << "Host: www.jokesclean.com" << "\r\n";
	ss << "User-Agent: Skivvy rquotes: v" << VERSION << "\r\n";
	ss << "Accept: text/html" << "\r\n";
	ss << "\r\n" << std::flush;

	std::string line;
	while(std::getline(ss, line))
	{
		if(extract_delimited_text(line, R"(<font size="+2">)", R"(</font>)", j) != str::npos
		&& !j.empty())// && std::isalpha(j[0]))
			break;

		if(extract_delimited_text(line, R"(td>)", R"(</td>)", j) != str::npos
		&& !j.empty() && std::isalpha(j[0]))
			break;
	}
	return j.empty() ? "Feature under construction." : j;
}

std::string RandomQuoteIrcBotPluginRep::clean_joke()
{
	static std::string j;
	static std::mutex mtx;
	static time_t prev = 0;

	lock_guard lock(mtx);

	if(time(0) - prev > 60)
	{
		j = get_clean_joke();
		prev = time(0);
	}

	return j;
}

RandomQuoteIrcBotPluginRep::RandomQuoteIrcBotPluginRep(IrcBot& bot)
: BasicIrcBotPlugin(bot)
, store(bot.getf(RQUOTES_STORE, RQUOTES_STORE_DEFAULT))
, joke_timer([&](const void* u)
{
	irc->say(*reinterpret_cast<const std::string*>(u), "06joke: 01" + joke());
})
, quote_timer([&](const void* u)
{
	str q = quote();

	while(q.size() > 400)
	{
		irc->say(*reinterpret_cast<const std::string*>(u), "03quote: 01" + q.substr(0, 400));
		q = q.substr(400);
	}
	irc->say(*reinterpret_cast<const std::string*>(u), "03quote: 01" + q);
})
{
}

RandomQuoteIrcBotPluginRep::~RandomQuoteIrcBotPluginRep()
{
}

void RandomQuoteIrcBotPluginRep::instant_quote(const message& msg)
{
	str q = quote();

	while(q.size() > 400)
	{
		bot.fc_reply(msg, "03quote: 01" + q.substr(0, 400));
		q = q.substr(400);
	}
	bot.fc_reply(msg, "03quote: 01" + q);
}

void RandomQuoteIrcBotPluginRep::instant_joke(const message& msg)
{
	bug_func();
//	bot.fc_reply(msg, get_clean_joke());
	bot.fc_reply(msg, "06joke: 01" + joke());
}

void RandomQuoteIrcBotPluginRep::jokes_on(const message& msg)
{
	bug_func();
	joke_timer.set_mindelay(bot.get(JOKE_MINDELAY, JOKE_MINDELAY_DEFAULT));
	joke_timer.set_maxdelay(bot.get(JOKE_MAXDELAY, JOKE_MAXDELAY_DEFAULT));
	lock_guard lock(channels_mtx);
	channels.insert(msg.reply_to());
	if(joke_timer.on(&(*channels.find(msg.reply_to()))))
		bot.fc_reply(msg, "Jokes have been turned on for this channel.");
	else
		bot.fc_reply(msg, "Jokes are already rollin' in here!");
}

void RandomQuoteIrcBotPluginRep::jokes_off(const message& msg)
{
	bug_func();
	lock_guard lock(channels_mtx);
	channels.insert(msg.reply_to());
	if(joke_timer.off(&(*channels.find(msg.reply_to()))))
		bot.fc_reply(msg, "Okay, okay! I'll stop with the jokes already...");
	else
		bot.fc_reply(msg, "I wasn't even telling any jokes!");
}

void RandomQuoteIrcBotPluginRep::quotes_on(const message& msg)
{
	bug_func();
	quote_timer.set_mindelay(bot.get(QUOTE_MINDELAY, QUOTE_MINDELAY_DEFAULT));
	quote_timer.set_maxdelay(bot.get(QUOTE_MAXDELAY, QUOTE_MAXDELAY_DEFAULT));
	lock_guard lock(channels_mtx);
	channels.insert(msg.reply_to());
	if(quote_timer.on(&(*channels.find(msg.reply_to()))))
		bot.fc_reply(msg, "Quotes have been turned on for this channel.");
	else
		bot.fc_reply(msg, "Quotes are already rollin' in here!");
}

void RandomQuoteIrcBotPluginRep::quotes_off(const message& msg)
{
	bug_func();
	lock_guard lock(channels_mtx);
	channels.insert(msg.reply_to());
	if(quote_timer.off(&(*channels.find(msg.reply_to()))))
		bot.fc_reply(msg, "Okay, okay! I'll stop with the quotes already...");
	else
		bot.fc_reply(msg, "I wasn't even quoting anybody!");
}

// INTERFACE: BasicIrcBotPlugin

bool RandomQuoteIrcBotPluginRep::initialize()
{
	bug_func();
	add
	({
		"!quote"
		, "!quote An instant quote."
		, [&](const message& msg){ instant_quote(msg); }
	});
	add
	({
		"!quotes_on"
		, "!quotes_on Start quoting at random intervals."
		, [&](const message& msg){ quotes_on(msg); }
	});
	add
	({
		"!quotes_off"
		, "!quotes_off Stop quoting at random intervals."
		, [&](const message& msg){ quotes_off(msg); }
	});
	add
	({
		"!joke"
		, "!joke An instant joke."
		, [&](const message& msg){ instant_joke(msg); }
	});
	add
	({
		"!jokes_on"
		, "!jokes_on Start joking at random intervals."
		, [&](const message& msg){ jokes_on(msg); }
	});
	add
	({
		"!jokes_off"
		, "!jokes_off Stop joking at random intervals."
		, [&](const message& msg){ jokes_off(msg); }
	});
	return true;
}

// INTERFACE: IrcBotPlugin

str RandomQuoteIrcBotPluginRep::get_id() const { return ID; }
str RandomQuoteIrcBotPluginRep::get_name() const { return NAME; }
str RandomQuoteIrcBotPluginRep::get_version() const { return VERSION; }

void RandomQuoteIrcBotPluginRep::exit()
{
	joke_timer.turn_off();
	quote_timer.turn_off();
}

RQuotesIrcBotPlugin::RQuotesIrcBotPlugin(IrcBot& bot)
: rep(*(new RandomQuoteIrcBotPluginRep(bot)))
{
}

RQuotesIrcBotPlugin::~RQuotesIrcBotPlugin()
{
	delete &rep;
}

//void RQuotesIrcBotPlugin::set_delay(size_t maxtime) { rep.set_delay(maxtime); }

// INTERFACE: IrcBotPlugin
bool  RQuotesIrcBotPlugin::init() { return rep.init(); }
std::string RQuotesIrcBotPlugin::get_id() const { return rep.get_id(); }
std::string RQuotesIrcBotPlugin::get_name() const { return rep.get_name(); }
std::string RQuotesIrcBotPlugin::get_version() const { return rep.get_version(); }
IrcBotPlugin::command_list RQuotesIrcBotPlugin::list() const { return rep.list(); }
void RQuotesIrcBotPlugin::execute(const std::string& cmd, const message& msg) { rep.execute(cmd, msg); }
std::string RQuotesIrcBotPlugin::help(const std::string& cmd) const { return rep.help(cmd); }
void RQuotesIrcBotPlugin::exit() { rep.exit(); }

}} // skivvy::rquotes

