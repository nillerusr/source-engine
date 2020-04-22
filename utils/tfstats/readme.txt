TFStats v2.0 readme file

v2.0 New Features
* Full search custom rules.  Custom rules can now search every event in the log
	and match based on regular expression matching patterns. 
* General Rule File.  TFStats now reads TFC.RUL in addition to any map specific
	rule files so you can put any server-specific rules in tfc.rul. 
* Persistent Player Statistics. Player's stats are now saved (if you so specify) 
	on the hard-disk and every time you generate a report more are saved and/or 
	merged (if a player's stats had already been saved).  What this does is allow
	player stats to accumulate over time, across many matches.  This also has support
	for omitting players who have been absent for a long time from the report.
* Stats Resume for Lan Games.  Stats Resume now works on Lan games by matching IP
	addresses. It will match by name if it cannot match by IP for some reason.
* Windows Front End. the Win32 version of TFStats now features an easy-to-use
	Windows front end that automates generating several logs at once and provides
	easy to use controls to control all of the new switches that TFStats supports and
	the directories it reads from and writes to.
* Shared Report Resources. TFStats can now generate several reports that share
	the same set of reports to preserve hard-drive space.  

v2.0 Bug Fixes
*Players with < and > in their names now work properly.
*Multiline Broadcasts are now handled correctly.

v1.5 New Features
*Team Differentiation:  If a player plays on two different teams, tfstats 
	gathers stats for each team seperately, then when viewing that players 
	stats, there's a link to that player's merged stats. 
*Pseudonyms for players: This is so it's not confusing if players change their
	names. it uses the name they used for the most time, and also lists other
	names they used.
*DisplayMM2 switch: The user (the person who runs tfstats) can now choose if
	they want to display team messages or not.  a lot of clans e-mailed me asking
	me to take out mm2 messages from the dialogue readout.
*Stats Resume: Disconnected players resume their stats where they left off
	when they reconnect.  This doesn't work in lan games because there is no
	WONID to work with.
*Garbage handling: TFStats is more robust when it comes to garbage input now.

v1.5 Bug Fixes
*The team kill award now works
*medics don't get double kills anymore
*the dates are now correct
*RandomPC is now handled correctly

As always you can e-mail tfstats@valvesoftware.com with questions, comments
and feature suggestions.  Read the TFStats manual for full documentation

Thanks for using TFStats!
