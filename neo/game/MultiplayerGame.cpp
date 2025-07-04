/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "Game_local.h"

// could be a problem if players manage to go down sudden deaths till this .. oh well
#define LASTMAN_NOLIVES -20

idCVar g_spectatorChat( "g_spectatorChat", "0", CVAR_GAME | CVAR_ARCHIVE | CVAR_BOOL, "let spectators talk to everyone during game" );

// global sounds transmitted by index - 0 .. SND_COUNT
// sounds in this list get precached on MP start
const char* idMultiplayerGame::GlobalSoundStrings[] =
{
	"sound/feedback/voc_youwin.wav",
	"sound/feedback/voc_youlose.wav",
	"sound/feedback/fight.wav",
	"sound/feedback/vote_now.wav",
	"sound/feedback/vote_passed.wav",
	"sound/feedback/vote_failed.wav",
	"sound/feedback/three.wav",
	"sound/feedback/two.wav",
	"sound/feedback/one.wav",
	"sound/feedback/sudden_death.wav",
};

// handy verbose
const char* idMultiplayerGame::GameStateStrings[] =
{
	"INACTIVE",
	"WARMUP",
	"COUNTDOWN",
	"GAMEON",
	"SUDDENDEATH",
	"GAMEREVIEW",
	"NEXTGAME"
};

const char* idMultiplayerGame::MPGuis[] =
{
	"guis/mphud.gui",
	"guis/mpmain.gui",
	"guis/mpmsgmode.gui",
	"guis/netmenu.gui",
	NULL
};

const char* idMultiplayerGame::ThrottleVars[] =
{
	"ui_spectate",
	"ui_ready",
	"ui_team",
	NULL
};

const char* idMultiplayerGame::ThrottleVarsInEnglish[] =
{
	"#str_06738",
	"#str_06737",
	"#str_01991",
	NULL
};

const int idMultiplayerGame::ThrottleDelay[] =
{
	8,
	5,
	5
};

/*
================
idMultiplayerGame::idMultiplayerGame
================
*/
idMultiplayerGame::idMultiplayerGame()
{
	scoreBoard = NULL;
	spectateGui = NULL;
	guiChat = NULL;
	mainGui = NULL;
	mapList = NULL;
	msgmodeGui = NULL;
	lastGameType = GAME_SP;
	Clear();
}

/*
================
idMultiplayerGame::Shutdown
================
*/
void idMultiplayerGame::Shutdown()
{
	Clear();
}

/*
================
idMultiplayerGame::SetMenuSkin
================
*/
void idMultiplayerGame::SetMenuSkin()
{
	// skins
	idStr str = cvarSystem->GetCVarString( "mod_validSkins" );
	idStr uiSkin = cvarSystem->GetCVarString( "ui_skin" );
	idStr skin;
	int skinId = 1;
	int count = 1;
	while( str.Length() )
	{
		int n = str.Find( ";" );
		if( n >= 0 )
		{
			skin = str.Left( n );
			str = str.Right( str.Length() - n - 1 );
		}
		else
		{
			skin = str;
			str = "";
		}
		if( skin.Icmp( uiSkin ) == 0 )
		{
			skinId = count;
		}
		count++;
	}

	for( int i = 0; i < count; i++ )
	{
		mainGui->SetStateInt( va( "skin%i", i + 1 ), 0 );
	}
	mainGui->SetStateInt( va( "skin%i", skinId ), 1 );
}

/*
================
idMultiplayerGame::Reset
================
*/
void idMultiplayerGame::Reset()
{
	Clear();
	assert( !scoreBoard && !spectateGui && !guiChat && !mainGui && !mapList );
	scoreBoard = uiManager->FindGui( "guis/scoreboard.gui", true, false, true );
	spectateGui = uiManager->FindGui( "guis/spectate.gui", true, false, true );
	guiChat = uiManager->FindGui( "guis/chat.gui", true, false, true );
	mainGui = uiManager->FindGui( "guis/mpmain.gui", true, false, true );
	mapList = uiManager->AllocListGUI();
	mapList->Config( mainGui, "mapList" );
	// set this GUI so that our Draw function is still called when it becomes the active/fullscreen GUI
	mainGui->SetStateBool( "gameDraw", true );
	mainGui->SetKeyBindingNames();
	mainGui->SetStateInt( "com_machineSpec", cvarSystem->GetCVarInteger( "com_machineSpec" ) );
	SetMenuSkin();
	msgmodeGui = uiManager->FindGui( "guis/mpmsgmode.gui", true, false, true );
	msgmodeGui->SetStateBool( "gameDraw", true );
	ClearGuis();
	ClearChatData();
	warmupEndTime = 0;
}

/*
================
idMultiplayerGame::ServerClientConnect
================
*/
void idMultiplayerGame::ServerClientConnect( int clientNum )
{
	memset( &playerState[ clientNum ], 0, sizeof( playerState[ clientNum ] ) );
}

/*
================
idMultiplayerGame::SpawnPlayer
================
*/
void idMultiplayerGame::SpawnPlayer( int clientNum )
{

	bool ingame = playerState[ clientNum ].ingame;

	memset( &playerState[ clientNum ], 0, sizeof( playerState[ clientNum ] ) );
	if( !gameLocal.isClient )
	{
		idPlayer* p = static_cast< idPlayer* >( gameLocal.entities[ clientNum ] );
		p->spawnedTime = gameLocal.time;
		if( gameLocal.gameType == GAME_TDM )
		{
			SwitchToTeam( clientNum, -1, p->team );
		}
		p->tourneyRank = 0;
		if( gameLocal.gameType == GAME_TOURNEY && gameState == GAMEON )
		{
			p->tourneyRank++;
		}
		playerState[ clientNum ].ingame = ingame;
	}
}

/*
================
idMultiplayerGame::Clear
================
*/
void idMultiplayerGame::Clear()
{
	int i;

	gameState = INACTIVE;
	nextState = INACTIVE;
	pingUpdateTime = 0;
	vote = VOTE_NONE;
	voteTimeOut = 0;
	voteExecTime = 0;
	nextStateSwitch = 0;
	matchStartedTime = 0;
	currentTourneyPlayer[ 0 ] = -1;
	currentTourneyPlayer[ 1 ] = -1;
	one = two = three = false;
	memset( &playerState, 0 , sizeof( playerState ) );
	lastWinner = -1;
	currentMenu = 0;
	bCurrentMenuMsg = false;
	nextMenu = 0;
	pureReady = false;
	scoreBoard = NULL;
	spectateGui = NULL;
	guiChat = NULL;
	mainGui = NULL;
	msgmodeGui = NULL;
	if( mapList )
	{
		uiManager->FreeListGUI( mapList );
		mapList = NULL;
	}
	fragLimitTimeout = 0;
	memset( &switchThrottle, 0, sizeof( switchThrottle ) );
	voiceChatThrottle = 0;
	for( i = 0; i < NUM_CHAT_NOTIFY; i++ )
	{
		chatHistory[ i ].line.Clear();
	}
	warmupText.Clear();
	voteValue.Clear();
	voteString.Clear();
	startFragLimit = -1;
}

/*
================
idMultiplayerGame::ClearGuis
================
*/
void idMultiplayerGame::ClearGuis()
{
	int i;

	for( i = 0; i < MAX_CLIENTS; i++ )
	{
		scoreBoard->SetStateString( va( "player%i", i + 1 ), "" );
		scoreBoard->SetStateString( va( "player%i_score", i + 1 ), "" );
		scoreBoard->SetStateString( va( "player%i_tdm_tscore", i + 1 ), "" );
		scoreBoard->SetStateString( va( "player%i_tdm_score", i + 1 ), "" );
		scoreBoard->SetStateString( va( "player%i_wins", i + 1 ), "" );
		scoreBoard->SetStateString( va( "player%i_status", i + 1 ), "" );
		scoreBoard->SetStateInt( va( "rank%i", i + 1 ), 0 );
		scoreBoard->SetStateInt( "rank_self", 0 );

		idPlayer* player = static_cast<idPlayer*>( gameLocal.entities[ i ] );
		if( !player || !player->hud )
		{
			continue;
		}
		player->hud->SetStateString( va( "player%i", i + 1 ), "" );
		player->hud->SetStateString( va( "player%i_score", i + 1 ), "" );
		player->hud->SetStateString( va( "player%i_ready", i + 1 ), "" );
		scoreBoard->SetStateInt( va( "rank%i", i + 1 ), 0 );
		player->hud->SetStateInt( "rank_self", 0 );
	}
}

/*
================
idMultiplayerGame::UpdatePlayerRanks
================
*/
void idMultiplayerGame::UpdatePlayerRanks()
{
	int i, j, k;
	idPlayer* players[MAX_CLIENTS];
	idEntity* ent;
	idPlayer* player;

	memset( players, 0, sizeof( players ) );
	numRankedPlayers = 0;

	for( i = 0; i < gameLocal.numClients; i++ )
	{
		ent = gameLocal.entities[ i ];
		if( !ent || !ent->IsType( idPlayer::Type ) )
		{
			continue;
		}
		player = static_cast< idPlayer* >( ent );
		if( !CanPlay( player ) )
		{
			continue;
		}
		if( gameLocal.gameType == GAME_TOURNEY )
		{
			if( i != currentTourneyPlayer[ 0 ] && i != currentTourneyPlayer[ 1 ] )
			{
				continue;
			}
		}
		if( gameLocal.gameType == GAME_LASTMAN && playerState[ i ].fragCount == LASTMAN_NOLIVES )
		{
			continue;
		}
		for( j = 0; j < numRankedPlayers; j++ )
		{
			bool insert = false;
			if( gameLocal.gameType == GAME_TDM )
			{
				if( player->team != players[ j ]->team )
				{
					if( playerState[ i ].teamFragCount > playerState[ players[ j ]->entityNumber ].teamFragCount )
					{
						// team scores
						insert = true;
					}
					else if( playerState[ i ].teamFragCount == playerState[ players[ j ]->entityNumber ].teamFragCount && player->team < players[ j ]->team )
					{
						// at equal scores, sort by team number
						insert = true;
					}
				}
				else if( playerState[ i ].fragCount > playerState[ players[ j ]->entityNumber ].fragCount )
				{
					// in the same team, sort by frag count
					insert = true;
				}
			}
			else
			{
				insert = ( playerState[ i ].fragCount > playerState[ players[ j ]->entityNumber ].fragCount );
			}
			if( insert )
			{
				for( k = numRankedPlayers; k > j; k-- )
				{
					players[ k ] = players[ k - 1 ];
				}
				players[ j ] = player;
				break;
			}
		}
		if( j == numRankedPlayers )
		{
			players[ numRankedPlayers ] = player;
		}
		numRankedPlayers++;
	}

	memcpy( rankedPlayers, players, sizeof( players ) );
}


/*
================
idMultiplayerGame::UpdateRankColor
================
*/
void idMultiplayerGame::UpdateRankColor( idUserInterface* gui, const char* mask, int i, const idVec3& vec )
{
	for( int j = 1; j < 4; j++ )
	{
		gui->SetStateFloat( va( mask, i, j ), vec[ j - 1 ] );
	}
}

/*
================
idMultiplayerGame::UpdateScoreboard
================
*/
void idMultiplayerGame::UpdateScoreboard( idUserInterface* scoreBoard, idPlayer* player )
{
	int i, j, iline, k;
	idStr gameinfo;
	idStr livesinfo;
	idStr timeinfo;
	idEntity* ent;
	idPlayer* p;
	int value;

	scoreBoard->SetStateString( "scoretext", gameLocal.gameType == GAME_LASTMAN ? common->GetLanguageDict()->GetString( "#str_04242" ) : common->GetLanguageDict()->GetString( "#str_04243" ) );

	iline = 0; // the display lines
	if( gameState != WARMUP )
	{
		for( i = 0; i < numRankedPlayers; i++ )
		{
			// ranked player
			iline++;
			scoreBoard->SetStateString( va( "player%i", iline ), rankedPlayers[ i ]->GetUserInfo()->GetString( "ui_name" ) );
			if( gameLocal.gameType == GAME_TDM )
			{
				value = idMath::ClampInt( MP_PLAYER_MINFRAGS, MP_PLAYER_MAXFRAGS, playerState[ rankedPlayers[ i ]->entityNumber ].fragCount );
				scoreBoard->SetStateInt( va( "player%i_tdm_score", iline ), value );
				value = idMath::ClampInt( MP_PLAYER_MINFRAGS, MP_PLAYER_MAXFRAGS, playerState[ rankedPlayers[ i ]->entityNumber ].teamFragCount );
				scoreBoard->SetStateString( va( "player%i_tdm_tscore", iline ), va( "/ %i", value ) );
				scoreBoard->SetStateString( va( "player%i_score", iline ), "" );
			}
			else
			{
				value = idMath::ClampInt( MP_PLAYER_MINFRAGS, MP_PLAYER_MAXFRAGS, playerState[ rankedPlayers[ i ]->entityNumber ].fragCount );
				scoreBoard->SetStateInt( va( "player%i_score", iline ), value );
				scoreBoard->SetStateString( va( "player%i_tdm_tscore", iline ), "" );
				scoreBoard->SetStateString( va( "player%i_tdm_score", iline ), "" );
			}
			value = idMath::ClampInt( 0, MP_PLAYER_MAXWINS, playerState[ rankedPlayers[ i ]->entityNumber ].wins );
			scoreBoard->SetStateInt( va( "player%i_wins", iline ), value );
			scoreBoard->SetStateInt( va( "player%i_ping", iline ), playerState[ rankedPlayers[ i ]->entityNumber ].ping );
			// set the color band
			scoreBoard->SetStateInt( va( "rank%i", iline ), 1 );
			UpdateRankColor( scoreBoard, "rank%i_color%i", iline, rankedPlayers[ i ]->colorBar );
			if( rankedPlayers[ i ] == player )
			{
				// highlight who we are
				scoreBoard->SetStateInt( "rank_self", iline );
			}
		}
	}

	// if warmup, this draws everyone, otherwise it goes over spectators only
	// when doing warmup we loop twice to draw ready/not ready first *then* spectators
	// NOTE: in tourney, shows spectators according to their playing rank order?
	for( k = 0; k < ( gameState == WARMUP ? 2 : 1 ); k++ )
	{
		for( i = 0; i < MAX_CLIENTS; i++ )
		{
			ent = gameLocal.entities[ i ];
			if( !ent || !ent->IsType( idPlayer::Type ) )
			{
				continue;
			}
			if( gameState != WARMUP )
			{
				// check he's not covered by ranks already
				for( j = 0; j < numRankedPlayers; j++ )
				{
					if( ent == rankedPlayers[ j ] )
					{
						break;
					}
				}
				if( j != numRankedPlayers )
				{
					continue;
				}
			}
			p = static_cast< idPlayer* >( ent );
			if( gameState == WARMUP )
			{
				if( k == 0 && p->spectating )
				{
					continue;
				}
				if( k == 1 && !p->spectating )
				{
					continue;
				}
			}

			iline++;
			if( !playerState[ i ].ingame )
			{
				scoreBoard->SetStateString( va( "player%i", iline ), common->GetLanguageDict()->GetString( "#str_04244" ) );
				scoreBoard->SetStateString( va( "player%i_score", iline ), common->GetLanguageDict()->GetString( "#str_04245" ) );
				// no color band
				scoreBoard->SetStateInt( va( "rank%i", iline ), 0 );
			}
			else
			{
				scoreBoard->SetStateString( va( "player%i", iline ), gameLocal.userInfo[ i ].GetString( "ui_name" ) );
				if( gameState == WARMUP )
				{
					if( p->spectating )
					{
						scoreBoard->SetStateString( va( "player%i_score", iline ), common->GetLanguageDict()->GetString( "#str_04246" ) );
						// no color band
						scoreBoard->SetStateInt( va( "rank%i", iline ), 0 );
					}
					else
					{
						scoreBoard->SetStateString( va( "player%i_score", iline ), p->IsReady() ? common->GetLanguageDict()->GetString( "#str_04247" ) : common->GetLanguageDict()->GetString( "#str_04248" ) );
						// set the color band
						scoreBoard->SetStateInt( va( "rank%i", iline ), 1 );
						UpdateRankColor( scoreBoard, "rank%i_color%i", iline, p->colorBar );
					}
				}
				else
				{
					if( gameLocal.gameType == GAME_LASTMAN && playerState[ i ].fragCount == LASTMAN_NOLIVES )
					{
						scoreBoard->SetStateString( va( "player%i_score", iline ), common->GetLanguageDict()->GetString( "#str_06736" ) );
						// set the color band
						scoreBoard->SetStateInt( va( "rank%i", iline ), 1 );
						UpdateRankColor( scoreBoard, "rank%i_color%i", iline, p->colorBar );
					}
					else
					{
						scoreBoard->SetStateString( va( "player%i_score", iline ), common->GetLanguageDict()->GetString( "#str_04246" ) );
						// no color band
						scoreBoard->SetStateInt( va( "rank%i", iline ), 0 );
					}
				}
			}
			scoreBoard->SetStateString( va( "player%i_tdm_tscore", iline ), "" );
			scoreBoard->SetStateString( va( "player%i_tdm_score", iline ), "" );
			scoreBoard->SetStateString( va( "player%i_wins", iline ), "" );
			scoreBoard->SetStateInt( va( "player%i_ping", iline ), playerState[ i ].ping );
			if( i == player->entityNumber )
			{
				// highlight who we are
				scoreBoard->SetStateInt( "rank_self", iline );
			}
		}
	}

	// clear remaining lines (empty slots)
	iline++;
	while( iline < 5 )
	{
		scoreBoard->SetStateString( va( "player%i", iline ), "" );
		scoreBoard->SetStateString( va( "player%i_score", iline ), "" );
		scoreBoard->SetStateString( va( "player%i_tdm_tscore", iline ), "" );
		scoreBoard->SetStateString( va( "player%i_tdm_score", iline ), "" );
		scoreBoard->SetStateString( va( "player%i_wins", iline ), "" );
		scoreBoard->SetStateString( va( "player%i_ping", iline ), "" );
		scoreBoard->SetStateInt( va( "rank%i", iline ), 0 );
		iline++;
	}

	gameinfo = va( "%s: %s", common->GetLanguageDict()->GetString( "#str_02376" ), gameLocal.serverInfo.GetString( "si_gameType" ) );
	if( gameLocal.gameType == GAME_LASTMAN )
	{
		if( gameState == GAMEON || gameState == SUDDENDEATH )
		{
			livesinfo = va( "%s: %i", common->GetLanguageDict()->GetString( "#str_04264" ), startFragLimit );
		}
		else
		{
			livesinfo = va( "%s: %i", common->GetLanguageDict()->GetString( "#str_04264" ), gameLocal.serverInfo.GetInt( "si_fragLimit" ) );
		}

	}
	else
	{
		livesinfo = va( "%s: %i", common->GetLanguageDict()->GetString( "#str_01982" ), gameLocal.serverInfo.GetInt( "si_fragLimit" ) );
	}
	if( gameLocal.serverInfo.GetInt( "si_timeLimit" ) > 0 )
	{
		timeinfo = va( "%s: %i", common->GetLanguageDict()->GetString( "#str_01983" ), gameLocal.serverInfo.GetInt( "si_timeLimit" ) );
	}
	else
	{
		timeinfo = va( "%s", common->GetLanguageDict()->GetString( "#str_07209" ) );
	}
	scoreBoard->SetStateString( "gameinfo", gameinfo );
	scoreBoard->SetStateString( "livesinfo", livesinfo );
	scoreBoard->SetStateString( "timeinfo", timeinfo );

	scoreBoard->Redraw( gameLocal.time );
}

/*
================
idMultiplayerGame::GameTime
================
*/
const char* idMultiplayerGame::GameTime()
{
	static char buff[16];
	int m, s, t, ms;

	if( gameState == COUNTDOWN )
	{
		ms = warmupEndTime - gameLocal.realClientTime;
		s = ms / 1000 + 1;
		if( ms <= 0 )
		{
			strcpy( buff, "WMP --" );
		}
		else
		{
			sprintf( buff, "WMP %i", s );
		}
	}
	else
	{
		int timeLimit = gameLocal.serverInfo.GetInt( "si_timeLimit" );
		if( timeLimit )
		{
			ms = ( timeLimit * 60000 ) - ( gameLocal.time - matchStartedTime );
		}
		else
		{
			ms = gameLocal.time - matchStartedTime;
		}
		if( ms < 0 )
		{
			ms = 0;
		}

		s = ms / 1000;
		m = s / 60;
		s -= m * 60;
		t = s / 10;
		s -= t * 10;

		sprintf( buff, "%i:%i%i", m, t, s );
	}
	return &buff[0];
}

/*
================
idMultiplayerGame::NumActualClients
================
*/
int idMultiplayerGame::NumActualClients( bool countSpectators, int* teamcounts )
{
	idPlayer* p;
	int c = 0;

	if( teamcounts )
	{
		teamcounts[ 0 ] = teamcounts[ 1 ] = 0;
	}
	for( int i = 0 ; i < gameLocal.numClients ; i++ )
	{
		idEntity* ent = gameLocal.entities[ i ];
		if( !ent || !ent->IsType( idPlayer::Type ) )
		{
			continue;
		}
		p = static_cast< idPlayer* >( ent );
		if( countSpectators || CanPlay( p ) )
		{
			c++;
		}
		if( teamcounts && CanPlay( p ) )
		{
			teamcounts[ p->team ]++;
		}
	}
	return c;
}

/*
================
idMultiplayerGame::EnoughClientsToPlay
================
*/
bool idMultiplayerGame::EnoughClientsToPlay()
{
	int team[ 2 ];
	int clients = NumActualClients( false, &team[ 0 ] );
	if( gameLocal.gameType == GAME_TDM )
	{
		return clients >= 2 && team[ 0 ] && team[ 1 ];
	}
	else
	{
		return clients >= 2;
	}
}

/*
================
idMultiplayerGame::AllPlayersReady
================
*/
bool idMultiplayerGame::AllPlayersReady()
{
	int			i;
	idEntity*	ent;
	idPlayer*	p;
	int			team[ 2 ];

	if( NumActualClients( false, &team[ 0 ] ) <= 1 )
	{
		return false;
	}

	if( gameLocal.gameType == GAME_TDM )
	{
		if( !team[ 0 ] || !team[ 1 ] )
		{
			return false;
		}
	}

	if( !gameLocal.serverInfo.GetBool( "si_warmup" ) )
	{
		return true;
	}

	for( i = 0; i < gameLocal.numClients; i++ )
	{
		if( gameLocal.gameType == GAME_TOURNEY && i != currentTourneyPlayer[ 0 ] && i != currentTourneyPlayer[ 1 ] )
		{
			continue;
		}
		ent = gameLocal.entities[ i ];
		if( !ent || !ent->IsType( idPlayer::Type ) )
		{
			continue;
		}
		p = static_cast< idPlayer* >( ent );
		if( CanPlay( p ) && !p->IsReady() )
		{
			return false;
		}
		team[ p->team ]++;
	}

	return true;
}

/*
================
idMultiplayerGame::FragLimitHit
return the winning player (team player)
if there is no FragLeader(), the game is tied and we return NULL
================
*/
idPlayer* idMultiplayerGame::FragLimitHit()
{
	int i;
	int fragLimit = gameLocal.serverInfo.GetInt( "si_fragLimit" );
	idPlayer* leader;

	leader = FragLeader();
	if( !leader )
	{
		return NULL;
	}

	if( fragLimit <= 0 )
	{
		fragLimit = MP_PLAYER_MAXFRAGS;
	}

	if( gameLocal.gameType == GAME_LASTMAN )
	{
		// we have a leader, check if any other players have frags left
		assert( !static_cast< idPlayer* >( leader )->lastManOver );
		for( i = 0 ; i < gameLocal.numClients ; i++ )
		{
			idEntity* ent = gameLocal.entities[ i ];
			if( !ent || !ent->IsType( idPlayer::Type ) )
			{
				continue;
			}
			if( !CanPlay( static_cast< idPlayer* >( ent ) ) )
			{
				continue;
			}
			if( ent == leader )
			{
				continue;
			}
			if( playerState[ ent->entityNumber ].fragCount > 0 )
			{
				return NULL;
			}
		}
		// there is a leader, his score may even be negative, but no one else has frags left or is !lastManOver
		return leader;
	}
	else if( gameLocal.gameType == GAME_TDM )
	{
		if( playerState[ leader->entityNumber ].teamFragCount >= fragLimit )
		{
			return leader;
		}
	}
	else
	{
		if( playerState[ leader->entityNumber ].fragCount >= fragLimit )
		{
			return leader;
		}
	}

	return NULL;
}

/*
================
idMultiplayerGame::TimeLimitHit
================
*/
bool idMultiplayerGame::TimeLimitHit()
{
	int timeLimit = gameLocal.serverInfo.GetInt( "si_timeLimit" );
	if( timeLimit )
	{
		if( gameLocal.time >= matchStartedTime + timeLimit * 60000 )
		{
			return true;
		}
	}
	return false;
}

/*
================
idMultiplayerGame::FragLeader
return the current winner ( or a player from the winning team )
NULL if even
================
*/
idPlayer* idMultiplayerGame::FragLeader()
{
	int i;
	int frags[ MAX_CLIENTS ];
	idPlayer* leader = NULL;
	idEntity* ent;
	idPlayer* p;
	int high = -9999;
	int count = 0;
	bool teamLead[ 2 ] = { false, false };

	for( i = 0 ; i < gameLocal.numClients ; i++ )
	{
		ent = gameLocal.entities[ i ];
		if( !ent || !ent->IsType( idPlayer::Type ) )
		{
			continue;
		}
		if( !CanPlay( static_cast< idPlayer* >( ent ) ) )
		{
			continue;
		}
		if( gameLocal.gameType == GAME_TOURNEY && ent->entityNumber != currentTourneyPlayer[ 0 ] && ent->entityNumber != currentTourneyPlayer[ 1 ] )
		{
			continue;
		}
		if( static_cast< idPlayer* >( ent )->lastManOver )
		{
			continue;
		}

		int fragc = ( gameLocal.gameType == GAME_TDM ) ? playerState[i].teamFragCount : playerState[i].fragCount;
		if( fragc > high )
		{
			high = fragc;
		}

		frags[ i ] = fragc;
	}

	for( i = 0; i < gameLocal.numClients; i++ )
	{
		ent = gameLocal.entities[ i ];
		if( !ent || !ent->IsType( idPlayer::Type ) )
		{
			continue;
		}
		p = static_cast< idPlayer* >( ent );
		p->SetLeader( false );

		if( !CanPlay( p ) )
		{
			continue;
		}
		if( gameLocal.gameType == GAME_TOURNEY && ent->entityNumber != currentTourneyPlayer[ 0 ] && ent->entityNumber != currentTourneyPlayer[ 1 ] )
		{
			continue;
		}
		if( p->lastManOver )
		{
			continue;
		}
		if( p->spectating )
		{
			continue;
		}

		if( frags[ i ] >= high )
		{
			leader = p;
			count++;
			p->SetLeader( true );
			if( gameLocal.gameType == GAME_TDM )
			{
				teamLead[ p->team ] = true;
			}
		}
	}

	if( gameLocal.gameType != GAME_TDM )
	{
		// more than one player at the highest frags
		if( count > 1 )
		{
			return NULL;
		}
		else
		{
			return leader;
		}
	}
	else
	{
		if( teamLead[ 0 ] && teamLead[ 1 ] )
		{
			// even game in team play
			return NULL;
		}
		return leader;
	}
}

/*
================
idGameLocal::UpdateWinsLosses
================
*/
void idMultiplayerGame::UpdateWinsLosses( idPlayer* winner )
{
	if( winner )
	{
		// run back through and update win/loss count
		for( int i = 0; i < gameLocal.numClients; i++ )
		{
			idEntity* ent = gameLocal.entities[ i ];
			if( !ent || !ent->IsType( idPlayer::Type ) )
			{
				continue;
			}
			idPlayer* player = static_cast<idPlayer*>( ent );
			if( gameLocal.gameType == GAME_TDM )
			{
				if( player == winner || ( player != winner && player->team == winner->team ) )
				{
					playerState[ i ].wins++;
					PlayGlobalSound( player->entityNumber, SND_YOUWIN );
				}
				else
				{
					PlayGlobalSound( player->entityNumber, SND_YOULOSE );
				}
			}
			else if( gameLocal.gameType == GAME_LASTMAN )
			{
				if( player == winner )
				{
					playerState[ i ].wins++;
					PlayGlobalSound( player->entityNumber, SND_YOUWIN );
				}
				else if( !player->wantSpectate )
				{
					PlayGlobalSound( player->entityNumber, SND_YOULOSE );
				}
			}
			else if( gameLocal.gameType == GAME_TOURNEY )
			{
				if( player == winner )
				{
					playerState[ i ].wins++;
					PlayGlobalSound( player->entityNumber, SND_YOUWIN );
				}
				else if( i == currentTourneyPlayer[ 0 ] || i == currentTourneyPlayer[ 1 ] )
				{
					PlayGlobalSound( player->entityNumber, SND_YOULOSE );
				}
			}
			else
			{
				if( player == winner )
				{
					playerState[i].wins++;
					PlayGlobalSound( player->entityNumber, SND_YOUWIN );
				}
				else if( !player->wantSpectate )
				{
					PlayGlobalSound( player->entityNumber, SND_YOULOSE );
				}
			}
		}
	}
	if( winner )
	{
		lastWinner = winner->entityNumber;
	}
	else
	{
		lastWinner = -1;
	}
}

/*
================
idMultiplayerGame::TeamScore
================
*/
void idMultiplayerGame::TeamScore( int entityNumber, int team, int delta )
{
	playerState[ entityNumber ].fragCount += delta;
	for( int i = 0 ; i < gameLocal.numClients ; i++ )
	{
		idEntity* ent = gameLocal.entities[ i ];
		if( !ent || !ent->IsType( idPlayer::Type ) )
		{
			continue;
		}
		idPlayer* player = static_cast<idPlayer*>( ent );
		if( player->team == team )
		{
			playerState[ player->entityNumber ].teamFragCount += delta;
		}
	}
}

/*
================
idMultiplayerGame::PlayerDeath
================
*/
void idMultiplayerGame::PlayerDeath( idPlayer* dead, idPlayer* killer, bool telefrag )
{

	// don't do PrintMessageEvent and shit
	assert( !gameLocal.isClient );

	if( killer )
	{
		if( gameLocal.gameType == GAME_LASTMAN )
		{
			playerState[ dead->entityNumber ].fragCount--;
		}
		else if( gameLocal.gameType == GAME_TDM )
		{
			if( killer == dead || killer->team == dead->team )
			{
				// suicide or teamkill
				TeamScore( killer->entityNumber, killer->team, -1 );
			}
			else
			{
				TeamScore( killer->entityNumber, killer->team, +1 );
			}
		}
		else
		{
			playerState[ killer->entityNumber ].fragCount += ( killer == dead ) ? -1 : 1;
		}
	}

	if( killer && killer == dead )
	{
		PrintMessageEvent( -1, MSG_SUICIDE, dead->entityNumber );
	}
	else if( killer )
	{
		if( telefrag )
		{
			PrintMessageEvent( -1, MSG_TELEFRAGGED, dead->entityNumber, killer->entityNumber );
		}
		else if( gameLocal.gameType == GAME_TDM && dead->team == killer->team )
		{
			PrintMessageEvent( -1, MSG_KILLEDTEAM, dead->entityNumber, killer->entityNumber );
		}
		else
		{
			PrintMessageEvent( -1, MSG_KILLED, dead->entityNumber, killer->entityNumber );
		}
	}
	else
	{
		PrintMessageEvent( -1, MSG_DIED, dead->entityNumber );
		playerState[ dead->entityNumber ].fragCount--;
	}
}

/*
================
idMultiplayerGame::PlayerStats
================
*/
void idMultiplayerGame::PlayerStats( int clientNum, char* data, const int len )
{

	idEntity* ent;
	int team;

	*data = 0;

	// make sure we don't exceed the client list
	if( clientNum < 0 || clientNum > gameLocal.numClients )
	{
		return;
	}

	// find which team this player is on
	ent = gameLocal.entities[ clientNum ];
	if( ent && ent->IsType( idPlayer::Type ) )
	{
		team = static_cast< idPlayer* >( ent )->team;
	}
	else
	{
		return;
	}

	idStr::snPrintf( data, len, "team=%d score=%ld tks=%ld", team, playerState[ clientNum ].fragCount, playerState[ clientNum ].teamFragCount );

	return;

}

/*
================
idMultiplayerGame::PlayerVote
================
*/
void idMultiplayerGame::PlayerVote( int clientNum, playerVote_t vote )
{
	playerState[ clientNum ].vote = vote;
}

/*
================
idMultiplayerGame::DumpTourneyLine
================
*/
void idMultiplayerGame::DumpTourneyLine()
{
	int i;
	for( i = 0; i < gameLocal.numClients; i++ )
	{
		if( gameLocal.entities[ i ] && gameLocal.entities[ i ]->IsType( idPlayer::Type ) )
		{
			common->Printf( "client %d: rank %d\n", i, static_cast< idPlayer* >( gameLocal.entities[ i ] )->tourneyRank );
		}
	}
}

/*
================
idMultiplayerGame::NewState
================
*/
void idMultiplayerGame::NewState( gameState_t news, idPlayer* player )
{
	idBitMsg	outMsg;
	byte		msgBuf[MAX_GAME_MESSAGE_SIZE];
	int			i;

	assert( news != gameState );
	assert( !gameLocal.isClient );
	gameLocal.DPrintf( "%s -> %s\n", GameStateStrings[ gameState ], GameStateStrings[ news ] );
	switch( news )
	{
		case GAMEON:
		{
			gameLocal.LocalMapRestart();
			outMsg.Init( msgBuf, sizeof( msgBuf ) );
			outMsg.WriteByte( GAME_RELIABLE_MESSAGE_RESTART );
			outMsg.WriteBits( 0, 1 );
			networkSystem->ServerSendReliableMessage( -1, outMsg );

			PlayGlobalSound( -1, SND_FIGHT );
			matchStartedTime = gameLocal.time;
			fragLimitTimeout = 0;
			for( i = 0; i < gameLocal.numClients; i++ )
			{
				idEntity* ent = gameLocal.entities[ i ];
				if( !ent || !ent->IsType( idPlayer::Type ) )
				{
					continue;
				}
				idPlayer* p = static_cast<idPlayer*>( ent );
				p->SetLeader( false ); // don't carry the flag from previous games
				if( gameLocal.gameType == GAME_TOURNEY && currentTourneyPlayer[ 0 ] != i && currentTourneyPlayer[ 1 ] != i )
				{
					p->ServerSpectate( true );
					p->tourneyRank++;
				}
				else
				{
					int fragLimit = gameLocal.serverInfo.GetInt( "si_fragLimit" );
					int startingCount = ( gameLocal.gameType == GAME_LASTMAN ) ? fragLimit : 0;
					playerState[ i ].fragCount = startingCount;
					playerState[ i ].teamFragCount = startingCount;
					if( !static_cast<idPlayer*>( ent )->wantSpectate )
					{
						static_cast<idPlayer*>( ent )->ServerSpectate( false );
						if( gameLocal.gameType == GAME_TOURNEY )
						{
							p->tourneyRank = 0;
						}
					}
				}
				if( CanPlay( p ) )
				{
					p->lastManPresent = true;
				}
				else
				{
					p->lastManPresent = false;
				}
			}
			cvarSystem->SetCVarString( "ui_ready", "Not Ready" );
			switchThrottle[ 1 ] = 0;	// passby the throttle
			startFragLimit = gameLocal.serverInfo.GetInt( "si_fragLimit" );
			break;
		}
		case GAMEREVIEW:
		{
			nextState = INACTIVE;	// used to abort a game. cancel out any upcoming state change
			// set all players not ready and spectating
			for( i = 0; i < gameLocal.numClients; i++ )
			{
				idEntity* ent = gameLocal.entities[ i ];
				if( !ent || !ent->IsType( idPlayer::Type ) )
				{
					continue;
				}
				static_cast< idPlayer*>( ent )->forcedReady = false;
				static_cast<idPlayer*>( ent )->ServerSpectate( true );
			}
			UpdateWinsLosses( player );
			break;
		}
		case SUDDENDEATH:
		{
			PrintMessageEvent( -1, MSG_SUDDENDEATH );
			PlayGlobalSound( -1, SND_SUDDENDEATH );
			break;
		}
		case COUNTDOWN:
		{
			idBitMsg	outMsg;
			byte		msgBuf[ 128 ];

			warmupEndTime = gameLocal.time + 1000 * cvarSystem->GetCVarInteger( "g_countDown" );

			outMsg.Init( msgBuf, sizeof( msgBuf ) );
			outMsg.WriteByte( GAME_RELIABLE_MESSAGE_WARMUPTIME );
			outMsg.WriteLong( warmupEndTime );
			networkSystem->ServerSendReliableMessage( -1, outMsg );

			break;
		}
		default:
			break;
	}

	gameState = news;
}

/*
================
idMultiplayerGame::FillTourneySlots
NOTE: called each frame during warmup to keep the tourney slots filled
================
*/
void idMultiplayerGame::FillTourneySlots()
{
	int i, j, rankmax, rankmaxindex;
	idEntity* ent;
	idPlayer* p;

	// fill up the slots based on tourney ranks
	for( i = 0; i < 2; i++ )
	{
		if( currentTourneyPlayer[ i ] != -1 )
		{
			continue;
		}
		rankmax = -1;
		rankmaxindex = -1;
		for( j = 0; j < gameLocal.numClients; j++ )
		{
			ent = gameLocal.entities[ j ];
			if( !ent || !ent->IsType( idPlayer::Type ) )
			{
				continue;
			}
			if( currentTourneyPlayer[ 0 ] == j || currentTourneyPlayer[ 1 ] == j )
			{
				continue;
			}
			p = static_cast< idPlayer* >( ent );
			if( p->wantSpectate )
			{
				continue;
			}
			if( p->tourneyRank >= rankmax )
			{
				// when ranks are equal, use time in game
				if( p->tourneyRank == rankmax )
				{
					assert( rankmaxindex >= 0 );
					if( p->spawnedTime > static_cast< idPlayer* >( gameLocal.entities[ rankmaxindex ] )->spawnedTime )
					{
						continue;
					}
				}
				rankmax = static_cast< idPlayer* >( ent )->tourneyRank;
				rankmaxindex = j;
			}
		}
		currentTourneyPlayer[ i ] = rankmaxindex; // may be -1 if we found nothing
	}
}

/*
================
idMultiplayerGame::UpdateTourneyLine
we manipulate tourneyRank on player entities for internal ranking. it's easier to deal with.
but we need a real wait list to be synced down to clients for GUI
ignore current players, ignore wantSpectate
================
*/
void idMultiplayerGame::UpdateTourneyLine()
{
	int i, j, imax, max, globalmax = -1;
	idPlayer* p;

	assert( !gameLocal.isClient );
	if( gameLocal.gameType != GAME_TOURNEY )
	{
		return;
	}

	for( j = 1; j <= gameLocal.numClients; j++ )
	{
		max = -1;
		imax = -1;
		for( i = 0; i < gameLocal.numClients; i++ )
		{
			if( currentTourneyPlayer[ 0 ] == i || currentTourneyPlayer[ 1 ] == i )
			{
				continue;
			}
			p = static_cast< idPlayer* >( gameLocal.entities[ i ] );
			if( !p || p->wantSpectate )
			{
				continue;
			}
			if( p->tourneyRank > max && ( globalmax == -1 || p->tourneyRank < globalmax ) )
			{
				imax = i;
				max = p->tourneyRank;
			}
		}
		if( imax == -1 )
		{
			break;
		}

		idBitMsg outMsg;
		byte msgBuf[1024];
		outMsg.Init( msgBuf, sizeof( msgBuf ) );
		outMsg.WriteByte( GAME_RELIABLE_MESSAGE_TOURNEYLINE );
		outMsg.WriteByte( j );
		networkSystem->ServerSendReliableMessage( imax, outMsg );

		globalmax = max;
	}
}

/*
================
idMultiplayerGame::CycleTourneyPlayers
================
*/
void idMultiplayerGame::CycleTourneyPlayers()
{
	int i;
	idEntity* ent;
	idPlayer* player;

	currentTourneyPlayer[ 0 ] = -1;
	currentTourneyPlayer[ 1 ] = -1;
	// if any, winner from last round will play again
	if( lastWinner != -1 )
	{
		idEntity* ent = gameLocal.entities[ lastWinner ];
		if( ent && ent->IsType( idPlayer::Type ) )
		{
			currentTourneyPlayer[ 0 ] = lastWinner;
		}
	}
	FillTourneySlots();
	// force selected players in/out of the game and update the ranks
	for( i = 0 ; i < gameLocal.numClients ; i++ )
	{
		if( currentTourneyPlayer[ 0 ] == i || currentTourneyPlayer[ 1 ] == i )
		{
			player = static_cast<idPlayer*>( gameLocal.entities[ i ] );
			player->ServerSpectate( false );
		}
		else
		{
			ent = gameLocal.entities[ i ];
			if( ent && ent->IsType( idPlayer::Type ) )
			{
				player = static_cast<idPlayer*>( gameLocal.entities[ i ] );
				player->ServerSpectate( true );
			}
		}
	}
	UpdateTourneyLine();
}

/*
================
idMultiplayerGame::ExecuteVote
the votes are checked for validity/relevance before they are started
we assume that they are still legit when reaching here
================
*/
void idMultiplayerGame::ExecuteVote()
{
	bool needRestart;
	switch( vote )
	{
		case VOTE_RESTART:
			gameLocal.MapRestart();
			break;
		case VOTE_TIMELIMIT:
			si_timeLimit.SetInteger( atoi( voteValue ) );
			needRestart = gameLocal.NeedRestart();
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "rescanSI" );
			if( needRestart )
			{
				cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "nextMap" );
			}
			break;
		case VOTE_FRAGLIMIT:
			si_fragLimit.SetInteger( atoi( voteValue ) );
			needRestart = gameLocal.NeedRestart();
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "rescanSI" );
			if( needRestart )
			{
				cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "nextMap" );
			}
			break;
		case VOTE_GAMETYPE:
			si_gameType.SetString( voteValue );
			gameLocal.MapRestart();
			break;
		case VOTE_KICK:
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "kick %s", voteValue.c_str() ) );
			break;
		case VOTE_MAP:
			si_map.SetString( voteValue );
			gameLocal.MapRestart();
			break;
		case VOTE_SPECTATORS:
			si_spectators.SetBool( !si_spectators.GetBool() );
			needRestart = gameLocal.NeedRestart();
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "rescanSI" );
			if( needRestart )
			{
				cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "nextMap" );
			}
			break;
		case VOTE_NEXTMAP:
			cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "serverNextMap\n" );
			break;
	}
}

/*
================
idMultiplayerGame::CheckVote
================
*/
void idMultiplayerGame::CheckVote()
{
	int numVoters, i;

	if( vote == VOTE_NONE )
	{
		return;
	}

	if( voteExecTime )
	{
		if( gameLocal.time > voteExecTime )
		{
			voteExecTime = 0;
			ClientUpdateVote( VOTE_RESET, 0, 0 );
			ExecuteVote();
			vote = VOTE_NONE;
		}
		return;
	}

	// count voting players
	numVoters = 0;
	for( i = 0; i < gameLocal.numClients; i++ )
	{
		idEntity* ent = gameLocal.entities[ i ];
		if( !ent || !ent->IsType( idPlayer::Type ) )
		{
			continue;
		}
		if( playerState[ i ].vote != PLAYER_VOTE_NONE )
		{
			numVoters++;
		}
	}
	if( !numVoters )
	{
		// abort
		vote = VOTE_NONE;
		ClientUpdateVote( VOTE_ABORTED, yesVotes, noVotes );
		return;
	}
	if( yesVotes / numVoters > 0.5f )
	{
		ClientUpdateVote( VOTE_PASSED, yesVotes, noVotes );
		voteExecTime = gameLocal.time + 2000;
		return;
	}
	if( gameLocal.time > voteTimeOut || noVotes / numVoters >= 0.5f )
	{
		ClientUpdateVote( VOTE_FAILED, yesVotes, noVotes );
		vote = VOTE_NONE;
		return;
	}
}

/*
================
idMultiplayerGame::Warmup
================
*/
bool idMultiplayerGame::Warmup()
{
	return ( gameState == WARMUP );
}

/*
================
idMultiplayerGame::Run
================
*/
void idMultiplayerGame::Run()
{
	int i, timeLeft;
	idPlayer* player;
	int gameReviewPause;

	assert( gameLocal.isMultiplayer );
	assert( !gameLocal.isClient );

	pureReady = true;

	if( gameState == INACTIVE )
	{
		lastGameType = gameLocal.gameType;
		NewState( WARMUP );
	}

	CheckVote();

	CheckRespawns();

	if( nextState != INACTIVE && gameLocal.time > nextStateSwitch )
	{
		NewState( nextState );
		nextState = INACTIVE;
	}

	// don't update the ping every frame to save bandwidth
	if( gameLocal.time > pingUpdateTime )
	{
		for( i = 0; i < gameLocal.numClients; i++ )
		{
			playerState[i].ping = networkSystem->ServerGetClientPing( i );
		}
		pingUpdateTime = gameLocal.time + 1000;
	}

	warmupText = "";

	switch( gameState )
	{
		case GAMEREVIEW:
		{
			if( nextState == INACTIVE )
			{
				gameReviewPause = cvarSystem->GetCVarInteger( "g_gameReviewPause" );
				nextState = NEXTGAME;
				nextStateSwitch = gameLocal.time + 1000 * gameReviewPause;
			}
			break;
		}
		case NEXTGAME:
		{
			if( nextState == INACTIVE )
			{
				// game rotation, new map, gametype etc.
				if( gameLocal.NextMap() )
				{
					cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "serverMapRestart\n" );
					return;
				}
				NewState( WARMUP );
				if( gameLocal.gameType == GAME_TOURNEY )
				{
					CycleTourneyPlayers();
				}
				// put everyone back in from endgame spectate
				for( i = 0; i < gameLocal.numClients; i++ )
				{
					idEntity* ent = gameLocal.entities[ i ];
					if( ent && ent->IsType( idPlayer::Type ) )
					{
						if( !static_cast< idPlayer* >( ent )->wantSpectate )
						{
							CheckRespawns( static_cast<idPlayer*>( ent ) );
						}
					}
				}
			}
			break;
		}
		case WARMUP:
		{
			if( AllPlayersReady() )
			{
				NewState( COUNTDOWN );
				nextState = GAMEON;
				nextStateSwitch = gameLocal.time + 1000 * cvarSystem->GetCVarInteger( "g_countDown" );
			}
			warmupText = "Warming up.. waiting for players to get ready";
			one = two = three = false;
			break;
		}
		case COUNTDOWN:
		{
			timeLeft = ( nextStateSwitch - gameLocal.time ) / 1000 + 1;
			if( timeLeft == 3 && !three )
			{
				PlayGlobalSound( -1, SND_THREE );
				three = true;
			}
			else if( timeLeft == 2 && !two )
			{
				PlayGlobalSound( -1, SND_TWO );
				two = true;
			}
			else if( timeLeft == 1 && !one )
			{
				PlayGlobalSound( -1, SND_ONE );
				one = true;
			}
			warmupText = va( "Match starts in %i", timeLeft );
			break;
		}
		case GAMEON:
		{
			player = FragLimitHit();
			if( player )
			{
				// delay between detecting frag limit and ending game. let the death anims play
				if( !fragLimitTimeout )
				{
					common->DPrintf( "enter FragLimit timeout, player %d is leader\n", player->entityNumber );
					fragLimitTimeout = gameLocal.time + FRAGLIMIT_DELAY;
				}
				if( gameLocal.time > fragLimitTimeout )
				{
					NewState( GAMEREVIEW, player );
					PrintMessageEvent( -1, MSG_FRAGLIMIT, player->entityNumber );
				}
			}
			else
			{
				if( fragLimitTimeout )
				{
					// frag limit was hit and cancelled. means the two teams got even during FRAGLIMIT_DELAY
					// enter sudden death, the next frag leader will win
					SuddenRespawn();
					PrintMessageEvent( -1, MSG_HOLYSHIT );
					fragLimitTimeout = 0;
					NewState( SUDDENDEATH );
				}
				else if( TimeLimitHit() )
				{
					player = FragLeader();
					if( !player )
					{
						NewState( SUDDENDEATH );
					}
					else
					{
						NewState( GAMEREVIEW, player );
						PrintMessageEvent( -1, MSG_TIMELIMIT );
					}
				}
			}
			break;
		}
		case SUDDENDEATH:
		{
			player = FragLeader();
			if( player )
			{
				if( !fragLimitTimeout )
				{
					common->DPrintf( "enter sudden death FragLeader timeout, player %d is leader\n", player->entityNumber );
					fragLimitTimeout = gameLocal.time + FRAGLIMIT_DELAY;
				}
				if( gameLocal.time > fragLimitTimeout )
				{
					NewState( GAMEREVIEW, player );
					PrintMessageEvent( -1, MSG_FRAGLIMIT, player->entityNumber );
				}
			}
			else if( fragLimitTimeout )
			{
				SuddenRespawn();
				PrintMessageEvent( -1, MSG_HOLYSHIT );
				fragLimitTimeout = 0;
			}
			break;
		}
	}
}

/*
================
idMultiplayerGame::UpdateMainGui
================
*/
void idMultiplayerGame::UpdateMainGui()
{
	int i;
	mainGui->SetStateInt( "readyon", gameState == WARMUP ? 1 : 0 );
	mainGui->SetStateInt( "readyoff", gameState != WARMUP ? 1 : 0 );
	idStr strReady = cvarSystem->GetCVarString( "ui_ready" );
	if( strReady.Icmp( "ready" ) == 0 )
	{
		strReady = common->GetLanguageDict()->GetString( "#str_04248" );
	}
	else
	{
		strReady = common->GetLanguageDict()->GetString( "#str_04247" );
	}
	mainGui->SetStateString( "ui_ready", strReady );
	mainGui->SetStateInt( "teamon", gameLocal.gameType == GAME_TDM ? 1 : 0 );
	mainGui->SetStateInt( "teamoff", gameLocal.gameType != GAME_TDM ? 1 : 0 );
	if( gameLocal.gameType == GAME_TDM )
	{
		idPlayer* p = gameLocal.GetClientByNum( gameLocal.localClientNum );
		mainGui->SetStateInt( "team", p->team );
	}
	// setup vote
	mainGui->SetStateInt( "voteon", ( vote != VOTE_NONE && !voted ) ? 1 : 0 );
	mainGui->SetStateInt( "voteoff", ( vote != VOTE_NONE && !voted ) ? 0 : 1 );
	// last man hack
	mainGui->SetStateInt( "isLastMan", gameLocal.gameType == GAME_LASTMAN ? 1 : 0 );
	// send the current serverinfo values
	for( i = 0; i < gameLocal.serverInfo.GetNumKeyVals(); i++ )
	{
		const idKeyValue* keyval = gameLocal.serverInfo.GetKeyVal( i );
		mainGui->SetStateString( keyval->GetKey(), keyval->GetValue() );
	}
	mainGui->StateChanged( gameLocal.time );
#if defined( __linux__ )
	// replacing the oh-so-useful s_reverse with sound backend prompt
	mainGui->SetStateString( "driver_prompt", "1" );
#else
	mainGui->SetStateString( "driver_prompt", "0" );
#endif
}

/*
================
idMultiplayerGame::StartMenu
================
*/
idUserInterface* idMultiplayerGame::StartMenu()
{

	if( mainGui == NULL )
	{
		return NULL;
	}

	int i, j;
	if( currentMenu )
	{
		currentMenu = 0;
		cvarSystem->SetCVarBool( "ui_chat", false );
	}
	else
	{
		if( nextMenu >= 2 )
		{
			currentMenu = nextMenu;
		}
		else
		{
			// for default and explicit
			currentMenu = 1;
		}
		cvarSystem->SetCVarBool( "ui_chat", true );
	}
	nextMenu = 0;
	gameLocal.sessionCommand = "";	// in case we used "game_startMenu" to trigger the menu
	if( currentMenu == 1 )
	{
		UpdateMainGui();

		// UpdateMainGui sets most things, but it doesn't set these because
		// it'd be pointless and/or harmful to set them every frame (for various reasons)
		// Currenty the gui doesn't update properly if they change anyway, so we'll leave it like this.

		// setup callvote
		if( vote == VOTE_NONE )
		{
			bool callvote_ok = false;
			for( i = 0; i < VOTE_COUNT; i++ )
			{
				// flag on means vote is denied, so default value 0 means all votes and -1 disables
				mainGui->SetStateInt( va( "vote%d", i ), g_voteFlags.GetInteger() & ( 1 << i ) ? 0 : 1 );
				if( !( g_voteFlags.GetInteger() & ( 1 << i ) ) )
				{
					callvote_ok = true;
				}
			}
			mainGui->SetStateInt( "callvote", callvote_ok );
		}
		else
		{
			mainGui->SetStateInt( "callvote", 2 );
		}

		// player kick data
		idStr kickList;
		j = 0;
		for( i = 0; i < gameLocal.numClients; i++ )
		{
			if( gameLocal.entities[ i ] && gameLocal.entities[ i ]->IsType( idPlayer::Type ) )
			{
				if( kickList.Length() )
				{
					kickList += ";";
				}
				kickList += va( "\"%d - %s\"", i, gameLocal.userInfo[ i ].GetString( "ui_name" ) );
				kickVoteMap[ j ] = i;
				j++;
			}
		}
		mainGui->SetStateString( "kickChoices", kickList );

		mainGui->SetStateString( "chattext", "" );
		mainGui->Activate( true, gameLocal.time );
		return mainGui;
	}
	else if( currentMenu == 2 )
	{
		// the setup is done in MessageMode
		msgmodeGui->Activate( true, gameLocal.time );
		cvarSystem->SetCVarBool( "ui_chat", true );
		return msgmodeGui;
	}
	return NULL;
}

/*
================
idMultiplayerGame::DisableMenu
================
*/
void idMultiplayerGame::DisableMenu()
{
	gameLocal.sessionCommand = "";	// in case we used "game_startMenu" to trigger the menu
	if( currentMenu == 1 )
	{
		mainGui->Activate( false, gameLocal.time );
	}
	else if( currentMenu == 2 )
	{
		msgmodeGui->Activate( false, gameLocal.time );
	}
	currentMenu = 0;
	nextMenu = 0;
	cvarSystem->SetCVarBool( "ui_chat", false );
}

/*
================
idMultiplayerGame::SetMapShot
================
*/
void idMultiplayerGame::SetMapShot()
{
	char screenshot[ MAX_STRING_CHARS ];
	int mapNum = mapList->GetSelection( NULL, 0 );
	const idDict* dict = NULL;
	if( mapNum >= 0 )
	{
		dict = fileSystem->GetMapDecl( mapNum );
	}
	fileSystem->FindMapScreenshot( dict ? dict->GetString( "path" ) : "", screenshot, MAX_STRING_CHARS );
	mainGui->SetStateString( "current_levelshot", screenshot );
}

/*
================
idMultiplayerGame::HandleGuiCommands
================
*/
const char* idMultiplayerGame::HandleGuiCommands( const char* _menuCommand )
{
	idUserInterface*	currentGui;
	const char*		voteValue;
	int				vote_clientNum;
	int				icmd;
	idCmdArgs		args;

	if( !_menuCommand[ 0 ] )
	{
		common->Printf( "idMultiplayerGame::HandleGuiCommands: empty command\n" );
		return "continue";
	}
	assert( currentMenu );
	if( currentMenu == 1 )
	{
		currentGui = mainGui;
	}
	else
	{
		currentGui = msgmodeGui;
	}

	args.TokenizeString( _menuCommand, false );

	for( icmd = 0; icmd < args.Argc(); )
	{
		const char* cmd = args.Argv( icmd++ );

		if( !idStr::Icmp( cmd,	";"	) )
		{
			continue;
		}
		else if(	!idStr::Icmp( cmd, "video" ) )
		{
			idStr vcmd;
			if( args.Argc() - icmd	>= 1 )
			{
				vcmd = args.Argv( icmd++ );
			}

			int	oldSpec	= cvarSystem->GetCVarInteger( "com_machineSpec"	);

			if( idStr::Icmp( vcmd,	"low" )	== 0 )
			{
				cvarSystem->SetCVarInteger(	"com_machineSpec", 0 );
			}
			else if(	idStr::Icmp( vcmd, "medium"	) == 0 )
			{
				cvarSystem->SetCVarInteger(	"com_machineSpec", 1 );
			}
			else	if( idStr::Icmp( vcmd,	"high" ) ==	0 )
			{
				cvarSystem->SetCVarInteger(	"com_machineSpec", 2 );
			}
			else	if( idStr::Icmp( vcmd,	"ultra"	) == 0 )
			{
				cvarSystem->SetCVarInteger(	"com_machineSpec", 3 );
			}
			else if(	idStr::Icmp( vcmd, "recommended" ) == 0	)
			{
				cmdSystem->BufferCommandText( CMD_EXEC_NOW,	"setMachineSpec\n" );
			}

			if( oldSpec !=	cvarSystem->GetCVarInteger(	"com_machineSpec" )	)
			{
				currentGui->SetStateInt( "com_machineSpec",	cvarSystem->GetCVarInteger(	"com_machineSpec" )	);
				currentGui->StateChanged( gameLocal.realClientTime );
				cmdSystem->BufferCommandText( CMD_EXEC_NOW,	"execMachineSpec\n"	);
			}

			if( idStr::Icmp( vcmd,	"restart" )	 ==	0 )
			{
				cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "vid_restart\n" );
			}

			continue;
		}
		else if(	!idStr::Icmp( cmd, "play" )	)
		{
			if( args.Argc() - icmd	>= 1 )
			{
				idStr snd =	args.Argv( icmd++ );
				int	channel	= 1;
				if( snd.Length() == 1 )
				{
					channel	= atoi(	snd	);
					snd	= args.Argv( icmd++	);
				}
				gameSoundWorld->PlayShaderDirectly(	snd, channel );
			}
			continue;
		}
		else if(	!idStr::Icmp( cmd, "mpSkin"	) )
		{
			idStr skin;
			if( args.Argc() - icmd	>= 1 )
			{
				skin = args.Argv( icmd++ );
				cvarSystem->SetCVarString( "ui_skin", skin );
			}
			SetMenuSkin();
			continue;
		}
		else if(	!idStr::Icmp( cmd, "quit" )	)
		{
			cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n"	);
			return NULL;
		}
		else if(	!idStr::Icmp( cmd, "disconnect"	) )
		{
			cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "disconnect\n" );
			return NULL;
		}
		else if(	!idStr::Icmp( cmd, "close" ) )
		{
			DisableMenu();
			return NULL;
		}
		else if(	!idStr::Icmp( cmd, "spectate" )	)
		{
			ToggleSpectate();
			DisableMenu();
			return NULL;
		}
		else if(	!idStr::Icmp( cmd, "chatmessage" ) )
		{
			int	mode = currentGui->State().GetInt( "messagemode" );
			if( mode )
			{
				cmdSystem->BufferCommandText( CMD_EXEC_NOW,	va(	"sayTeam \"%s\"", currentGui->State().GetString( "chattext"	) )	);
			}
			else
			{
				cmdSystem->BufferCommandText( CMD_EXEC_NOW,	va(	"say \"%s\"", currentGui->State().GetString( "chattext"	) )	);
			}
			currentGui->SetStateString(	"chattext",	"" );
			if( currentMenu ==	1 )
			{
				return "continue";
			}
			else
			{
				DisableMenu();
				return NULL;
			}
		}
		else if(	!idStr::Icmp( cmd, "readytoggle" ) )
		{
			ToggleReady();
			DisableMenu();
			return NULL;
		}
		else if(	!idStr::Icmp( cmd, "teamtoggle"	) )
		{
			ToggleTeam(	);
			DisableMenu();
			return NULL;
		}
		else if(	!idStr::Icmp( cmd, "callVote" )	)
		{
			vote_flags_t voteIndex = ( vote_flags_t )mainGui->State().GetInt(	"voteIndex"	);
			if( voteIndex == VOTE_MAP )
			{
				int mapNum = mapList->GetSelection( NULL, 0 );
				if( mapNum >= 0 )
				{
					const idDict* dict = fileSystem->GetMapDecl( mapNum );
					if( dict )
					{
						ClientCallVote( VOTE_MAP, dict->GetString( "path" ) );
					}
				}
			}
			else
			{
				voteValue =	mainGui->State().GetString(	"str_voteValue"	);
				if( voteIndex == VOTE_KICK	)
				{
					vote_clientNum = kickVoteMap[ atoi(	voteValue )	];
					ClientCallVote(	voteIndex, va( "%d", vote_clientNum	) );
				}
				else
				{
					ClientCallVote(	voteIndex, voteValue );
				}
			}
			DisableMenu();
			return NULL;
		}
		else if(	!idStr::Icmp( cmd, "voteyes" ) )
		{
			CastVote( gameLocal.localClientNum,	true );
			DisableMenu();
			return NULL;
		}
		else if(	!idStr::Icmp( cmd, "voteno"	) )
		{
			CastVote( gameLocal.localClientNum,	false );
			DisableMenu();
			return NULL;
		}
		else if( !idStr::Icmp( cmd, "bind" ) )
		{
			if( args.Argc() - icmd >= 2 )
			{
				idStr key = args.Argv( icmd++ );
				idStr bind = args.Argv( icmd++ );
				cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "bindunbindtwo \"%s\" \"%s\"", key.c_str(), bind.c_str() ) );
				mainGui->SetKeyBindingNames();
			}
			continue;
		}
		else if( !idStr::Icmp( cmd, "clearbind" ) )
		{
			if( args.Argc() - icmd >= 1 )
			{
				idStr bind = args.Argv( icmd++ );
				cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "unbind \"%s\"", bind.c_str() ) );
				mainGui->SetKeyBindingNames();
			}
			continue;
		}
		else if(	!idStr::Icmp( cmd, "MAPScan" ) )
		{
			const char* gametype = gameLocal.serverInfo.GetString( "si_gameType" );
			if( gametype == NULL || *gametype == 0 || idStr::Icmp( gametype, "singleplayer" ) == 0 )
			{
				gametype = "Deathmatch";
			}

			int i, num;
			idStr si_map = gameLocal.serverInfo.GetString( "si_map" );
			const idDict* dict;

			mapList->Clear();
			mapList->SetSelection( -1 );
			num = fileSystem->GetNumMaps();
			for( i = 0; i < num; i++ )
			{
				dict = fileSystem->GetMapDecl( i );
				if( dict )
				{
					// any MP gametype supported
					bool isMP = false;
					int igt = GAME_SP + 1;
					while( si_gameTypeArgs[ igt ] )
					{
						if( dict->GetBool( si_gameTypeArgs[ igt ] ) )
						{
							isMP = true;
							break;
						}
						igt++;
					}
					if( isMP )
					{
						const char* mapName = dict->GetString( "name" );
						if( mapName[0] == '\0' )
						{
							mapName = dict->GetString( "path" );
						}
						mapName = common->GetLanguageDict()->GetString( mapName );
						mapList->Add( i, mapName );
						if( !si_map.Icmp( dict->GetString( "path" ) ) )
						{
							mapList->SetSelection( mapList->Num() - 1 );
						}
					}
				}
			}
			// set the current level shot
			SetMapShot(	);
			return "continue";
		}
		else if(	!idStr::Icmp( cmd, "click_maplist" ) )
		{
			SetMapShot(	);
			return "continue";
		}
		else if( strstr( cmd, "sound" ) == cmd )
		{
			// pass that back to the core, will know what to do with it
			return _menuCommand;
		}
		common->Printf(	"idMultiplayerGame::HandleGuiCommands: '%s'	unknown\n",	cmd	);

	}
	return "continue";
}

/*
================
idMultiplayerGame::Draw
================
*/
bool idMultiplayerGame::Draw( int clientNum )
{
	idPlayer* player, *viewPlayer;

	// clear the render entities for any players that don't need
	// icons and which might not be thinking because they weren't in
	// the last snapshot.
	for( int i = 0; i < gameLocal.numClients; i++ )
	{
		player = static_cast<idPlayer*>( gameLocal.entities[ i ] );
		if( player && !player->NeedsIcon() )
		{
			player->HidePlayerIcons();
		}
	}

	player = viewPlayer = static_cast<idPlayer*>( gameLocal.entities[ clientNum ] );

	if( player == NULL )
	{
		return false;
	}

	if( player->spectating )
	{
		viewPlayer = static_cast<idPlayer*>( gameLocal.entities[ player->spectator ] );
		if( viewPlayer == NULL )
		{
			return false;
		}
	}

	UpdatePlayerRanks();
	UpdateHud( viewPlayer, player->hud );
	// use the hud of the local player
	viewPlayer->playerView.RenderPlayerView( player->hud );

	if( currentMenu )
	{
#if 0
		// uncomment this if you want to track when players are in a menu
		if( !bCurrentMenuMsg )
		{
			idBitMsg	outMsg;
			byte		msgBuf[ 128 ];

			outMsg.Init( msgBuf, sizeof( msgBuf ) );
			outMsg.WriteByte( GAME_RELIABLE_MESSAGE_MENU );
			outMsg.WriteBits( 1, 1 );
			networkSystem->ClientSendReliableMessage( outMsg );

			bCurrentMenuMsg = true;
		}
#endif
		if( player->wantSpectate )
		{
			mainGui->SetStateString( "spectext", common->GetLanguageDict()->GetString( "#str_04249" ) );
		}
		else
		{
			mainGui->SetStateString( "spectext", common->GetLanguageDict()->GetString( "#str_04250" ) );
		}
		DrawChat();
		if( currentMenu == 1 )
		{
			UpdateMainGui();
			mainGui->Redraw( gameLocal.time );
		}
		else
		{
			msgmodeGui->Redraw( gameLocal.time );
		}
	}
	else
	{
#if 0
		// uncomment this if you want to track when players are in a menu
		if( bCurrentMenuMsg )
		{
			idBitMsg	outMsg;
			byte		msgBuf[ 128 ];

			outMsg.Init( msgBuf, sizeof( msgBuf ) );
			outMsg.WriteByte( GAME_RELIABLE_MESSAGE_MENU );
			outMsg.WriteBits( 0, 1 );
			networkSystem->ClientSendReliableMessage( outMsg );

			bCurrentMenuMsg = false;
		}
#endif
		if( player->spectating )
		{
			idStr spectatetext[ 2 ];
			int ispecline = 0;
			if( gameLocal.gameType == GAME_TOURNEY )
			{
				if( !player->wantSpectate )
				{
					spectatetext[ 0 ] = common->GetLanguageDict()->GetString( "#str_04246" );
					switch( player->tourneyLine )
					{
						case 0:
							spectatetext[ 0 ] += common->GetLanguageDict()->GetString( "#str_07003" );
							break;
						case 1:
							spectatetext[ 0 ] += common->GetLanguageDict()->GetString( "#str_07004" );
							break;
						case 2:
							spectatetext[ 0 ] += common->GetLanguageDict()->GetString( "#str_07005" );
							break;
						default:
							spectatetext[ 0 ] += va( common->GetLanguageDict()->GetString( "#str_07006" ), player->tourneyLine );
							break;
					}
					ispecline++;
				}
			}
			else if( gameLocal.gameType == GAME_LASTMAN )
			{
				if( !player->wantSpectate )
				{
					spectatetext[ 0 ] = common->GetLanguageDict()->GetString( "#str_07007" );
					ispecline++;
				}
			}
			if( player->spectator != player->entityNumber )
			{
				spectatetext[ ispecline ] = va( common->GetLanguageDict()->GetString( "#str_07008" ), viewPlayer->GetUserInfo()->GetString( "ui_name" ) );
			}
			else if( !ispecline )
			{
				spectatetext[ 0 ] = common->GetLanguageDict()->GetString( "#str_04246" );
			}
			spectateGui->SetStateString( "spectatetext0", spectatetext[0].c_str() );
			spectateGui->SetStateString( "spectatetext1", spectatetext[1].c_str() );
			if( vote != VOTE_NONE )
			{
				spectateGui->SetStateString( "vote", va( "%s (y: %d n: %d)", voteString.c_str(), ( int )yesVotes, ( int )noVotes ) );
			}
			else
			{
				spectateGui->SetStateString( "vote", "" );
			}
			spectateGui->Redraw( gameLocal.time );
		}
		DrawChat();
		DrawScoreBoard( player );
	}

	return true;
}

/*
================
idMultiplayerGame::UpdateHud
================
*/
void idMultiplayerGame::UpdateHud( idPlayer* player, idUserInterface* hud )
{
	int i;

	if( !hud )
	{
		return;
	}

	hud->SetStateBool( "warmup", Warmup() );

	if( gameState == WARMUP )
	{
		if( player->IsReady() )
		{
			hud->SetStateString( "warmuptext", common->GetLanguageDict()->GetString( "#str_04251" ) );
		}
		else
		{
			hud->SetStateString( "warmuptext", common->GetLanguageDict()->GetString( "#str_07002" ) );
		}
	}

	hud->SetStateString( "timer", ( Warmup() ) ? common->GetLanguageDict()->GetString( "#str_04251" ) : ( gameState == SUDDENDEATH ) ? common->GetLanguageDict()->GetString( "#str_04252" ) : GameTime() );
	if( vote != VOTE_NONE )
	{
		hud->SetStateString( "vote", va( "%s (y: %d n: %d)", voteString.c_str(), ( int )yesVotes, ( int )noVotes ) );
	}
	else
	{
		hud->SetStateString( "vote", "" );
	}

	hud->SetStateInt( "rank_self", 0 );
	if( gameState == GAMEON )
	{
		for( i = 0; i < numRankedPlayers; i++ )
		{
			if( gameLocal.gameType == GAME_TDM )
			{
				hud->SetStateInt( va( "player%i_score", i + 1 ), playerState[ rankedPlayers[ i ]->entityNumber ].teamFragCount );
			}
			else
			{
				hud->SetStateInt( va( "player%i_score", i + 1 ), playerState[ rankedPlayers[ i ]->entityNumber ].fragCount );
			}
			hud->SetStateInt( va( "rank%i", i + 1 ), 1 );
			UpdateRankColor( hud, "rank%i_color%i", i + 1, rankedPlayers[ i ]->colorBar );
			if( rankedPlayers[ i ] == player )
			{
				hud->SetStateInt( "rank_self", i + 1 );
			}
		}
	}
	for( i = ( gameState == GAMEON ? numRankedPlayers : 0 ) ; i < 5; i++ )
	{
		hud->SetStateString( va( "player%i", i + 1 ), "" );
		hud->SetStateString( va( "player%i_score", i + 1 ), "" );
		hud->SetStateInt( va( "rank%i", i + 1 ), 0 );
	}
}

/*
================
idMultiplayerGame::DrawScoreBoard
================
*/
void idMultiplayerGame::DrawScoreBoard( idPlayer* player )
{
	if( player->scoreBoardOpen || gameState == GAMEREVIEW )
	{
		if( !playerState[ player->entityNumber ].scoreBoardUp )
		{
			scoreBoard->Activate( true, gameLocal.time );
			playerState[ player->entityNumber ].scoreBoardUp = true;
		}
		UpdateScoreboard( scoreBoard, player );
	}
	else
	{
		if( playerState[ player->entityNumber ].scoreBoardUp )
		{
			scoreBoard->Activate( false, gameLocal.time );
			playerState[ player->entityNumber ].scoreBoardUp = false;
		}
	}
}

/*
===============
idMultiplayerGame::ClearChatData
===============
*/
void idMultiplayerGame::ClearChatData()
{
	chatHistoryIndex	= 0;
	chatHistorySize		= 0;
	chatDataUpdated		= true;
}

/*
===============
idMultiplayerGame::AddChatLine
===============
*/
void idMultiplayerGame::AddChatLine( const char* fmt, ... )
{
	idStr temp;
	va_list argptr;

	va_start( argptr, fmt );
	vsprintf( temp, fmt, argptr );
	va_end( argptr );

	gameLocal.Printf( "%s\n", temp.c_str() );

	chatHistory[ chatHistoryIndex % NUM_CHAT_NOTIFY ].line = temp;
	chatHistory[ chatHistoryIndex % NUM_CHAT_NOTIFY ].fade = 6;

	chatHistoryIndex++;
	if( chatHistorySize < NUM_CHAT_NOTIFY )
	{
		chatHistorySize++;
	}
	chatDataUpdated = true;
	lastChatLineTime = gameLocal.time;
}

/*
===============
idMultiplayerGame::DrawChat
===============
*/
void idMultiplayerGame::DrawChat()
{
	int i, j;
	if( guiChat )
	{
		if( gameLocal.time - lastChatLineTime > CHAT_FADE_TIME )
		{
			if( chatHistorySize > 0 )
			{
				for( i = chatHistoryIndex - chatHistorySize; i < chatHistoryIndex; i++ )
				{
					chatHistory[ i % NUM_CHAT_NOTIFY ].fade--;
					if( chatHistory[ i % NUM_CHAT_NOTIFY ].fade < 0 )
					{
						chatHistorySize--; // this assumes the removals are always at the beginning
					}
				}
				chatDataUpdated = true;
			}
			lastChatLineTime = gameLocal.time;
		}
		if( chatDataUpdated )
		{
			j = 0;
			i = chatHistoryIndex - chatHistorySize;
			while( i < chatHistoryIndex )
			{
				guiChat->SetStateString( va( "chat%i", j ), chatHistory[ i % NUM_CHAT_NOTIFY ].line );
				// don't set alpha above 4, the gui only knows that
				guiChat->SetStateInt( va( "alpha%i", j ), Min( 4, ( int )chatHistory[ i % NUM_CHAT_NOTIFY ].fade ) );
				j++;
				i++;
			}
			while( j < NUM_CHAT_NOTIFY )
			{
				guiChat->SetStateString( va( "chat%i", j ), "" );
				j++;
			}
			guiChat->Activate( true, gameLocal.time );
			chatDataUpdated = false;
		}
		guiChat->Redraw( gameLocal.time );
	}
}

const int ASYNC_PLAYER_FRAG_BITS = -idMath::BitsForInteger( MP_PLAYER_MAXFRAGS - MP_PLAYER_MINFRAGS );	// player can have negative frags
const int ASYNC_PLAYER_WINS_BITS = idMath::BitsForInteger( MP_PLAYER_MAXWINS );
const int ASYNC_PLAYER_PING_BITS = idMath::BitsForInteger( MP_PLAYER_MAXPING );

/*
================
idMultiplayerGame::WriteToSnapshot
================
*/
void idMultiplayerGame::WriteToSnapshot( idBitMsgDelta& msg ) const
{
	int i;
	int value;

	msg.WriteByte( gameState );
	msg.WriteShort( currentTourneyPlayer[ 0 ] );
	msg.WriteShort( currentTourneyPlayer[ 1 ] );
	for( i = 0; i < MAX_CLIENTS; i++ )
	{
		// clamp all values to min/max possible value that we can send over
		value = idMath::ClampInt( MP_PLAYER_MINFRAGS, MP_PLAYER_MAXFRAGS, playerState[i].fragCount );
		msg.WriteBits( value, ASYNC_PLAYER_FRAG_BITS );
		value = idMath::ClampInt( MP_PLAYER_MINFRAGS, MP_PLAYER_MAXFRAGS, playerState[i].teamFragCount );
		msg.WriteBits( value, ASYNC_PLAYER_FRAG_BITS );
		value = idMath::ClampInt( 0, MP_PLAYER_MAXWINS, playerState[i].wins );
		msg.WriteBits( value, ASYNC_PLAYER_WINS_BITS );
		value = idMath::ClampInt( 0, MP_PLAYER_MAXPING, playerState[i].ping );
		msg.WriteBits( value, ASYNC_PLAYER_PING_BITS );
		msg.WriteBits( playerState[i].ingame, 1 );
	}
}

/*
================
idMultiplayerGame::ReadFromSnapshot
================
*/
void idMultiplayerGame::ReadFromSnapshot( const idBitMsgDelta& msg )
{
	int i;
	gameState_t newState;

	newState = ( idMultiplayerGame::gameState_t )msg.ReadByte();
	if( newState != gameState )
	{
		gameLocal.DPrintf( "%s -> %s\n", GameStateStrings[ gameState ], GameStateStrings[ newState ] );
		gameState = newState;
		// these could be gathered in a BGNewState() kind of thing, as we have to do them in NewState as well
		if( gameState == GAMEON )
		{
			matchStartedTime = gameLocal.time;
			cvarSystem->SetCVarString( "ui_ready", "Not Ready" );
			switchThrottle[ 1 ] = 0;	// passby the throttle
			startFragLimit = gameLocal.serverInfo.GetInt( "si_fragLimit" );
		}
	}
	currentTourneyPlayer[ 0 ] = msg.ReadShort();
	currentTourneyPlayer[ 1 ] = msg.ReadShort();
	for( i = 0; i < MAX_CLIENTS; i++ )
	{
		playerState[i].fragCount = msg.ReadBits( ASYNC_PLAYER_FRAG_BITS );
		playerState[i].teamFragCount = msg.ReadBits( ASYNC_PLAYER_FRAG_BITS );
		playerState[i].wins = msg.ReadBits( ASYNC_PLAYER_WINS_BITS );
		playerState[i].ping = msg.ReadBits( ASYNC_PLAYER_PING_BITS );
		playerState[i].ingame = msg.ReadBits( 1 ) != 0;
	}
}

/*
================
idMultiplayerGame::PlayGlobalSound
================
*/
void idMultiplayerGame::PlayGlobalSound( int to, snd_evt_t evt, const char* shader )
{
	const idSoundShader* shaderDecl;

	if( to == -1 || to == gameLocal.localClientNum )
	{
		if( shader )
		{
			gameSoundWorld->PlayShaderDirectly( shader );
		}
		else
		{
			gameSoundWorld->PlayShaderDirectly( GlobalSoundStrings[ evt ] );
		}
	}

	if( !gameLocal.isClient )
	{
		idBitMsg outMsg;
		byte msgBuf[1024];
		outMsg.Init( msgBuf, sizeof( msgBuf ) );

		if( shader )
		{
			shaderDecl = declManager->FindSound( shader );
			if( !shaderDecl )
			{
				return;
			}
			outMsg.WriteByte( GAME_RELIABLE_MESSAGE_SOUND_INDEX );
			outMsg.WriteLong( gameLocal.ServerRemapDecl( to, DECL_SOUND, shaderDecl->Index() ) );
		}
		else
		{
			outMsg.WriteByte( GAME_RELIABLE_MESSAGE_SOUND_EVENT );
			outMsg.WriteByte( evt );
		}

		networkSystem->ServerSendReliableMessage( to, outMsg );
	}
}

/*
================
idMultiplayerGame::PrintMessageEvent
================
*/
void idMultiplayerGame::PrintMessageEvent( int to, msg_evt_t evt, int parm1, int parm2 )
{
	switch( evt )
	{
		case MSG_SUICIDE:
			assert( parm1 >= 0 );
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04293" ), gameLocal.userInfo[ parm1 ].GetString( "ui_name" ) );
			break;
		case MSG_KILLED:
			assert( parm1 >= 0 && parm2 >= 0 );
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04292" ), gameLocal.userInfo[ parm1 ].GetString( "ui_name" ), gameLocal.userInfo[ parm2 ].GetString( "ui_name" ) );
			break;
		case MSG_KILLEDTEAM:
			assert( parm1 >= 0 && parm2 >= 0 );
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04291" ), gameLocal.userInfo[ parm1 ].GetString( "ui_name" ), gameLocal.userInfo[ parm2 ].GetString( "ui_name" ) );
			break;
		case MSG_TELEFRAGGED:
			assert( parm1 >= 0 && parm2 >= 0 );
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04290" ), gameLocal.userInfo[ parm1 ].GetString( "ui_name" ), gameLocal.userInfo[ parm2 ].GetString( "ui_name" ) );
			break;
		case MSG_DIED:
			assert( parm1 >= 0 );
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04289" ), gameLocal.userInfo[ parm1 ].GetString( "ui_name" ) );
			break;
		case MSG_VOTE:
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04288" ) );
			break;
		case MSG_SUDDENDEATH:
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04287" ) );
			break;
		case MSG_FORCEREADY:
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04286" ), gameLocal.userInfo[ parm1 ].GetString( "ui_name" ) );
			if( gameLocal.entities[ parm1 ] && gameLocal.entities[ parm1 ]->IsType( idPlayer::Type ) )
			{
				static_cast< idPlayer* >( gameLocal.entities[ parm1 ] )->forcedReady = true;
			}
			break;
		case MSG_JOINEDSPEC:
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04285" ), gameLocal.userInfo[ parm1 ].GetString( "ui_name" ) );
			break;
		case MSG_TIMELIMIT:
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04284" ) );
			break;
		case MSG_FRAGLIMIT:
			if( gameLocal.gameType == GAME_LASTMAN )
			{
				AddChatLine( common->GetLanguageDict()->GetString( "#str_04283" ), gameLocal.userInfo[ parm1 ].GetString( "ui_name" ) );
			}
			else if( gameLocal.gameType == GAME_TDM )
			{
				AddChatLine( common->GetLanguageDict()->GetString( "#str_04282" ), gameLocal.userInfo[ parm1 ].GetString( "ui_team" ) );
			}
			else
			{
				AddChatLine( common->GetLanguageDict()->GetString( "#str_04281" ), gameLocal.userInfo[ parm1 ].GetString( "ui_name" ) );
			}
			break;
		case MSG_JOINTEAM:
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04280" ), gameLocal.userInfo[ parm1 ].GetString( "ui_name" ), parm2 ? common->GetLanguageDict()->GetString( "#str_02500" ) : common->GetLanguageDict()->GetString( "#str_02499" ) );
			break;
		case MSG_HOLYSHIT:
			AddChatLine( common->GetLanguageDict()->GetString( "#str_06732" ) );
			break;
		default:
			gameLocal.DPrintf( "PrintMessageEvent: unknown message type %d\n", evt );
			return;
	}
	if( !gameLocal.isClient )
	{
		idBitMsg outMsg;
		byte msgBuf[1024];
		outMsg.Init( msgBuf, sizeof( msgBuf ) );
		outMsg.WriteByte( GAME_RELIABLE_MESSAGE_DB );
		outMsg.WriteByte( evt );
		outMsg.WriteByte( parm1 );
		outMsg.WriteByte( parm2 );
		networkSystem->ServerSendReliableMessage( to, outMsg );
	}
}

/*
================
idMultiplayerGame::SuddenRespawns
solely for LMN if an end game ( fragLimitTimeout ) was entered and aborted before expiration
LMN players which still have lives left need to be respawned without being marked lastManOver
================
*/
void idMultiplayerGame::SuddenRespawn()
{
	int i;

	if( gameLocal.gameType != GAME_LASTMAN )
	{
		return;
	}

	for( i = 0; i < gameLocal.numClients; i++ )
	{
		if( !gameLocal.entities[ i ] || !gameLocal.entities[ i ]->IsType( idPlayer::Type ) )
		{
			continue;
		}
		if( !CanPlay( static_cast< idPlayer* >( gameLocal.entities[ i ] ) ) )
		{
			continue;
		}
		if( static_cast< idPlayer* >( gameLocal.entities[ i ] )->lastManOver )
		{
			continue;
		}
		static_cast< idPlayer* >( gameLocal.entities[ i ] )->lastManPlayAgain = true;
	}
}

/*
================
idMultiplayerGame::CheckSpawns
================
*/
void idMultiplayerGame::CheckRespawns( idPlayer* spectator )
{
	for( int i = 0 ; i < gameLocal.numClients ; i++ )
	{
		idEntity* ent = gameLocal.entities[ i ];
		if( !ent || !ent->IsType( idPlayer::Type ) )
		{
			continue;
		}
		idPlayer* p = static_cast<idPlayer*>( ent );
		// once we hit sudden death, nobody respawns till game has ended
		if( WantRespawn( p ) || p == spectator )
		{
			if( gameState == SUDDENDEATH && gameLocal.gameType != GAME_LASTMAN )
			{
				// respawn rules while sudden death are different
				// sudden death may trigger while a player is dead, so there are still cases where we need to respawn
				// don't do any respawns while we are in end game delay though
				if( !fragLimitTimeout )
				{
					if( gameLocal.gameType == GAME_TDM || p->IsLeader() )
					{
#ifdef _DEBUG
						if( gameLocal.gameType == GAME_TOURNEY )
						{
							assert( p->entityNumber == currentTourneyPlayer[ 0 ] || p->entityNumber == currentTourneyPlayer[ 1 ] );
						}
#endif
						p->ServerSpectate( false );
					}
					else if( !p->IsLeader() )
					{
						// sudden death is rolling, this player is not a leader, have him spectate
						p->ServerSpectate( true );
						CheckAbortGame();
					}
				}
			}
			else
			{
				if( gameLocal.gameType == GAME_DM ||
						gameLocal.gameType == GAME_TDM )
				{
					if( gameState == WARMUP || gameState == COUNTDOWN || gameState == GAMEON )
					{
						p->ServerSpectate( false );
					}
				}
				else if( gameLocal.gameType == GAME_TOURNEY )
				{
					if( i == currentTourneyPlayer[ 0 ] || i == currentTourneyPlayer[ 1 ] )
					{
						if( gameState == WARMUP || gameState == COUNTDOWN || gameState == GAMEON )
						{
							p->ServerSpectate( false );
						}
					}
					else if( gameState == WARMUP )
					{
						// make sure empty tourney slots get filled first
						FillTourneySlots();
						if( i == currentTourneyPlayer[ 0 ] || i == currentTourneyPlayer[ 1 ] )
						{
							p->ServerSpectate( false );
						}
					}
				}
				else if( gameLocal.gameType == GAME_LASTMAN )
				{
					if( gameState == WARMUP || gameState == COUNTDOWN )
					{
						p->ServerSpectate( false );
					}
					else if( gameState == GAMEON || gameState == SUDDENDEATH )
					{
						if( gameState == GAMEON && playerState[ i ].fragCount > 0 && p->lastManPresent )
						{
							assert( !p->lastManOver );
							p->ServerSpectate( false );
						}
						else if( p->lastManPlayAgain && p->lastManPresent )
						{
							assert( gameState == SUDDENDEATH );
							p->ServerSpectate( false );
						}
						else
						{
							// if a fragLimitTimeout was engaged, do NOT mark lastManOver as that could mean
							// everyone ends up spectator and game is stalled with no end
							// if the frag limit delay is engaged and cancels out before expiring, LMN players are
							// respawned to play the tie again ( through SuddenRespawn and lastManPlayAgain )
							if( !fragLimitTimeout && !p->lastManOver )
							{
								common->DPrintf( "client %d has lost all last man lives\n", i );
								// end of the game for this guy, send him to spectators
								p->lastManOver = true;
								// clients don't have access to lastManOver
								// so set the fragCount to something silly ( used in scoreboard and player ranking )
								playerState[ i ].fragCount = LASTMAN_NOLIVES;
								p->ServerSpectate( true );

								//Check for a situation where the last two player dies at the same time and don't
								//try to respawn manually...This was causing all players to go into spectate mode
								//and the server got stuck
								{
									int j;
									for( j = 0; j < gameLocal.numClients; j++ )
									{
										if( !gameLocal.entities[ j ] )
										{
											continue;
										}
										if( !CanPlay( static_cast< idPlayer* >( gameLocal.entities[ j ] ) ) )
										{
											continue;
										}
										if( !static_cast< idPlayer* >( gameLocal.entities[ j ] )->lastManOver )
										{
											break;
										}
									}
									if( j == gameLocal.numClients )
									{
										//Everyone is dead so don't allow this player to spectate
										//so the match will end
										p->ServerSpectate( false );
									}
								}
							}
						}
					}
				}
			}
		}
		else if( p->wantSpectate && !p->spectating )
		{
			playerState[ i ].fragCount = 0; // whenever you willingly go spectate during game, your score resets
			p->ServerSpectate( true );
			UpdateTourneyLine();
			CheckAbortGame();
		}
	}
}

/*
================
idMultiplayerGame::ForceReady
================
*/
void idMultiplayerGame::ForceReady()
{

	for( int i = 0 ; i < gameLocal.numClients ; i++ )
	{
		idEntity* ent = gameLocal.entities[ i ];
		if( !ent || !ent->IsType( idPlayer::Type ) )
		{
			continue;
		}
		idPlayer* p = static_cast<idPlayer*>( ent );
		if( !p->IsReady() )
		{
			PrintMessageEvent( -1, MSG_FORCEREADY, i );
			p->forcedReady = true;
		}
	}
}

/*
================
idMultiplayerGame::ForceReady_f
================
*/
void idMultiplayerGame::ForceReady_f( const idCmdArgs& args )
{
	if( !gameLocal.isMultiplayer || gameLocal.isClient )
	{
		common->Printf( "forceReady: multiplayer server only\n" );
		return;
	}
	gameLocal.mpGame.ForceReady();
}

/*
================
idMultiplayerGame::DropWeapon
================
*/
void idMultiplayerGame::DropWeapon( int clientNum )
{
	assert( !gameLocal.isClient );
	idEntity* ent = gameLocal.entities[ clientNum ];
	if( !ent || !ent->IsType( idPlayer::Type ) )
	{
		return;
	}
	static_cast< idPlayer* >( ent )->DropWeapon( false );
}

/*
================
idMultiplayerGame::DropWeapon_f
================
*/
void idMultiplayerGame::DropWeapon_f( const idCmdArgs& args )
{
	if( !gameLocal.isMultiplayer )
	{
		common->Printf( "clientDropWeapon: only valid in multiplayer\n" );
		return;
	}
	idBitMsg	outMsg;
	byte		msgBuf[128];
	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_DROPWEAPON );
	networkSystem->ClientSendReliableMessage( outMsg );
}

/*
================
idMultiplayerGame::MessageMode_f
================
*/
void idMultiplayerGame::MessageMode_f( const idCmdArgs& args )
{
	gameLocal.mpGame.MessageMode( args );
}

/*
================
idMultiplayerGame::MessageMode
================
*/
void idMultiplayerGame::MessageMode( const idCmdArgs& args )
{
	const char* mode;
	int imode;

	if( !gameLocal.isMultiplayer )
	{
		common->Printf( "clientMessageMode: only valid in multiplayer\n" );
		return;
	}
	if( !mainGui )
	{
		common->Printf( "no local client\n" );
		return;
	}
	mode = args.Argv( 1 );
	if( !mode[ 0 ] )
	{
		imode = 0;
	}
	else
	{
		imode = atoi( mode );
	}
	msgmodeGui->SetStateString( "messagemode", imode ? "1" : "0" );
	msgmodeGui->SetStateString( "chattext", "" );
	nextMenu = 2;
	// let the session know that we want our ingame main menu opened
	gameLocal.sessionCommand = "game_startmenu";
}

/*
================
idMultiplayerGame::Vote_f
FIXME: voting from console
================
*/
void idMultiplayerGame::Vote_f( const idCmdArgs& args ) { }

/*
================
idMultiplayerGame::CallVote_f
FIXME: voting from console
================
*/
void idMultiplayerGame::CallVote_f( const idCmdArgs& args ) { }

/*
================
idMultiplayerGame::ServerStartVote
================
*/
void idMultiplayerGame::ServerStartVote( int clientNum, vote_flags_t voteIndex, const char* value )
{
	int i;

	assert( vote == VOTE_NONE );

	// setup
	yesVotes = 1;
	noVotes = 0;
	vote = voteIndex;
	voteValue = value;
	voteTimeOut = gameLocal.time + 20000;
	// mark players allowed to vote - only current ingame players, players joining during vote will be ignored
	for( i = 0; i < gameLocal.numClients; i++ )
	{
		if( gameLocal.entities[ i ] && gameLocal.entities[ i ]->IsType( idPlayer::Type ) )
		{
			playerState[ i ].vote = ( i == clientNum ) ? PLAYER_VOTE_YES : PLAYER_VOTE_WAIT;
		}
		else
		{
			playerState[i].vote = PLAYER_VOTE_NONE;
		}
	}
}

/*
================
idMultiplayerGame::ClientStartVote
================
*/
void idMultiplayerGame::ClientStartVote( int clientNum, const char* _voteString )
{
	idBitMsg	outMsg;
	byte		msgBuf[ MAX_GAME_MESSAGE_SIZE ];

	if( !gameLocal.isClient )
	{
		outMsg.Init( msgBuf, sizeof( msgBuf ) );
		outMsg.WriteByte( GAME_RELIABLE_MESSAGE_STARTVOTE );
		outMsg.WriteByte( clientNum );
		outMsg.WriteString( _voteString );
		networkSystem->ServerSendReliableMessage( -1, outMsg );
	}

	voteString = _voteString;
	AddChatLine( va( common->GetLanguageDict()->GetString( "#str_04279" ), gameLocal.userInfo[ clientNum ].GetString( "ui_name" ) ) );
	gameSoundWorld->PlayShaderDirectly( GlobalSoundStrings[ SND_VOTE ] );
	if( clientNum == gameLocal.localClientNum )
	{
		voted = true;
	}
	else
	{
		voted = false;
	}
	if( gameLocal.isClient )
	{
		// the the vote value to something so the vote line is displayed
		vote = VOTE_RESTART;
		yesVotes = 1;
		noVotes = 0;
	}
}

/*
================
idMultiplayerGame::ClientUpdateVote
================
*/
void idMultiplayerGame::ClientUpdateVote( vote_result_t status, int yesCount, int noCount )
{
	idBitMsg	outMsg;
	byte		msgBuf[ MAX_GAME_MESSAGE_SIZE ];

	if( !gameLocal.isClient )
	{
		outMsg.Init( msgBuf, sizeof( msgBuf ) );
		outMsg.WriteByte( GAME_RELIABLE_MESSAGE_UPDATEVOTE );
		outMsg.WriteByte( status );
		outMsg.WriteByte( yesCount );
		outMsg.WriteByte( noCount );
		networkSystem->ServerSendReliableMessage( -1, outMsg );
	}

	if( vote == VOTE_NONE )
	{
		// clients coming in late don't get the vote start and are not allowed to vote
		return;
	}

	switch( status )
	{
		case VOTE_FAILED:
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04278" ) );
			gameSoundWorld->PlayShaderDirectly( GlobalSoundStrings[ SND_VOTE_FAILED ] );
			if( gameLocal.isClient )
			{
				vote = VOTE_NONE;
			}
			break;
		case VOTE_PASSED:
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04277" ) );
			gameSoundWorld->PlayShaderDirectly( GlobalSoundStrings[ SND_VOTE_PASSED ] );
			break;
		case VOTE_RESET:
			if( gameLocal.isClient )
			{
				vote = VOTE_NONE;
			}
			break;
		case VOTE_ABORTED:
			AddChatLine( common->GetLanguageDict()->GetString( "#str_04276" ) );
			if( gameLocal.isClient )
			{
				vote = VOTE_NONE;
			}
			break;
		default:
			break;
	}
	if( gameLocal.isClient )
	{
		yesVotes = yesCount;
		noVotes = noCount;
	}
}

/*
================
idMultiplayerGame::ClientCallVote
================
*/
void idMultiplayerGame::ClientCallVote( vote_flags_t voteIndex, const char* voteValue )
{
	idBitMsg	outMsg;
	byte		msgBuf[ MAX_GAME_MESSAGE_SIZE ];

	// send
	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_CALLVOTE );
	outMsg.WriteByte( voteIndex );
	outMsg.WriteString( voteValue );
	networkSystem->ClientSendReliableMessage( outMsg );
}

/*
================
idMultiplayerGame::CastVote
================
*/
void idMultiplayerGame::CastVote( int clientNum, bool castVote )
{
	idBitMsg	outMsg;
	byte		msgBuf[ 128 ];

	if( clientNum == gameLocal.localClientNum )
	{
		voted = true;
	}

	if( gameLocal.isClient )
	{
		outMsg.Init( msgBuf, sizeof( msgBuf ) );
		outMsg.WriteByte( GAME_RELIABLE_MESSAGE_CASTVOTE );
		outMsg.WriteByte( castVote );
		networkSystem->ClientSendReliableMessage( outMsg );
		return;
	}

	// sanity
	if( vote == VOTE_NONE )
	{
		gameLocal.ServerSendChatMessage( clientNum, "server", common->GetLanguageDict()->GetString( "#str_04275" ) );
		common->DPrintf( "client %d: cast vote while no vote in progress\n", clientNum );
		return;
	}
	if( playerState[ clientNum ].vote != PLAYER_VOTE_WAIT )
	{
		gameLocal.ServerSendChatMessage( clientNum, "server", common->GetLanguageDict()->GetString( "#str_04274" ) );
		common->DPrintf( "client %d: cast vote - vote %d != PLAYER_VOTE_WAIT\n", clientNum, playerState[ clientNum ].vote );
		return;
	}

	if( castVote )
	{
		playerState[ clientNum ].vote = PLAYER_VOTE_YES;
		yesVotes++;
	}
	else
	{
		playerState[ clientNum ].vote = PLAYER_VOTE_NO;
		noVotes++;
	}

	ClientUpdateVote( VOTE_UPDATE, yesVotes, noVotes );
}

/*
================
idMultiplayerGame::ServerCallVote
================
*/
void idMultiplayerGame::ServerCallVote( int clientNum, const idBitMsg& msg )
{
	vote_flags_t	voteIndex;
	int				vote_timeLimit, vote_fragLimit, vote_clientNum, vote_gameTypeIndex; //, vote_kickIndex;
	char			value[ MAX_STRING_CHARS ];

	assert( clientNum != -1 );
	assert( !gameLocal.isClient );

	voteIndex = ( vote_flags_t )msg.ReadByte();
	msg.ReadString( value, sizeof( value ) );

	// sanity checks - setup the vote
	if( vote != VOTE_NONE )
	{
		gameLocal.ServerSendChatMessage( clientNum, "server", common->GetLanguageDict()->GetString( "#str_04273" ) );
		common->DPrintf( "client %d: called vote while voting already in progress - ignored\n", clientNum );
		return;
	}
	switch( voteIndex )
	{
		case VOTE_RESTART:
			ServerStartVote( clientNum, voteIndex, "" );
			ClientStartVote( clientNum, common->GetLanguageDict()->GetString( "#str_04271" ) );
			break;
		case VOTE_NEXTMAP:
			ServerStartVote( clientNum, voteIndex, "" );
			ClientStartVote( clientNum, common->GetLanguageDict()->GetString( "#str_04272" ) );
			break;
		case VOTE_TIMELIMIT:
			vote_timeLimit = strtol( value, NULL, 10 );
			if( vote_timeLimit == gameLocal.serverInfo.GetInt( "si_timeLimit" ) )
			{
				gameLocal.ServerSendChatMessage( clientNum, "server", common->GetLanguageDict()->GetString( "#str_04270" ) );
				common->DPrintf( "client %d: already at the voted Time Limit\n", clientNum );
				return;
			}
			if( vote_timeLimit < si_timeLimit.GetMinValue() || vote_timeLimit > si_timeLimit.GetMaxValue() )
			{
				gameLocal.ServerSendChatMessage( clientNum, "server", common->GetLanguageDict()->GetString( "#str_04269" ) );
				common->DPrintf( "client %d: timelimit value out of range for vote: %s\n", clientNum, value );
				return;
			}
			ServerStartVote( clientNum, voteIndex, value );
			ClientStartVote( clientNum, va( common->GetLanguageDict()->GetString( "#str_04268" ), vote_timeLimit ) );
			break;
		case VOTE_FRAGLIMIT:
			vote_fragLimit = strtol( value, NULL, 10 );
			if( vote_fragLimit == gameLocal.serverInfo.GetInt( "si_fragLimit" ) )
			{
				gameLocal.ServerSendChatMessage( clientNum, "server", common->GetLanguageDict()->GetString( "#str_04267" ) );
				common->DPrintf( "client %d: already at the voted Frag Limit\n", clientNum );
				return;
			}
			if( vote_fragLimit < si_fragLimit.GetMinValue() || vote_fragLimit > si_fragLimit.GetMaxValue() )
			{
				gameLocal.ServerSendChatMessage( clientNum, "server", common->GetLanguageDict()->GetString( "#str_04266" ) );
				common->DPrintf( "client %d: fraglimit value out of range for vote: %s\n", clientNum, value );
				return;
			}
			ServerStartVote( clientNum, voteIndex, value );
			ClientStartVote( clientNum, va( common->GetLanguageDict()->GetString( "#str_04303" ), gameLocal.gameType == GAME_LASTMAN ? common->GetLanguageDict()->GetString( "#str_04264" ) : common->GetLanguageDict()->GetString( "#str_04265" ), vote_fragLimit ) );
			break;
		case VOTE_GAMETYPE:
			vote_gameTypeIndex = strtol( value, NULL, 10 );
			assert( vote_gameTypeIndex >= 0 && vote_gameTypeIndex <= 3 );
			switch( vote_gameTypeIndex )
			{
				case 0:
					strcpy( value, "Deathmatch" );
					break;
				case 1:
					strcpy( value, "Tourney" );
					break;
				case 2:
					strcpy( value, "Team DM" );
					break;
				case 3:
					strcpy( value, "Last Man" );
					break;
			}
			if( !idStr::Icmp( value, gameLocal.serverInfo.GetString( "si_gameType" ) ) )
			{
				gameLocal.ServerSendChatMessage( clientNum, "server", common->GetLanguageDict()->GetString( "#str_04259" ) );
				common->DPrintf( "client %d: already at the voted Game Type\n", clientNum );
				return;
			}
			ServerStartVote( clientNum, voteIndex, value );
			ClientStartVote( clientNum, va( common->GetLanguageDict()->GetString( "#str_04258" ), value ) );
			break;
		case VOTE_KICK:
			vote_clientNum = strtol( value, NULL, 10 );
			if( vote_clientNum == gameLocal.localClientNum )
			{
				gameLocal.ServerSendChatMessage( clientNum, "server", common->GetLanguageDict()->GetString( "#str_04257" ) );
				common->DPrintf( "client %d: called kick for the server host\n", clientNum );
				return;
			}
			ServerStartVote( clientNum, voteIndex, va( "%d", vote_clientNum ) );
			ClientStartVote( clientNum, va( common->GetLanguageDict()->GetString( "#str_04302" ), vote_clientNum, gameLocal.userInfo[ vote_clientNum ].GetString( "ui_name" ) ) );
			break;
		case VOTE_MAP:
		{
			if( idStr::FindText( gameLocal.serverInfo.GetString( "si_map" ), value ) != -1 )
			{
				gameLocal.ServerSendChatMessage( clientNum, "server", va( common->GetLanguageDict()->GetString( "#str_04295" ), value ) );
				common->DPrintf( "client %d: already running the voted map: %s\n", clientNum, value );
				return;
			}
			int				num = fileSystem->GetNumMaps();
			int				i;
			const idDict*	dict;
			bool			haveMap = false;
			for( i = 0; i < num; i++ )
			{
				dict = fileSystem->GetMapDecl( i );
				if( dict && !idStr::Icmp( dict->GetString( "path" ), value ) )
				{
					haveMap = true;
					break;
				}
			}
			if( !haveMap )
			{
				gameLocal.ServerSendChatMessage( clientNum, "server", va( common->GetLanguageDict()->GetString( "#str_04296" ), value ) );
				common->Printf( "client %d: map not found: %s\n", clientNum, value );
				return;
			}
			ServerStartVote( clientNum, voteIndex, value );
			ClientStartVote( clientNum, va( common->GetLanguageDict()->GetString( "#str_04256" ), common->GetLanguageDict()->GetString( dict ? dict->GetString( "name" ) : value ) ) );
			break;
		}
		case VOTE_SPECTATORS:
			if( gameLocal.serverInfo.GetBool( "si_spectators" ) )
			{
				ServerStartVote( clientNum, voteIndex, "" );
				ClientStartVote( clientNum, common->GetLanguageDict()->GetString( "#str_04255" ) );
			}
			else
			{
				ServerStartVote( clientNum, voteIndex, "" );
				ClientStartVote( clientNum, common->GetLanguageDict()->GetString( "#str_04254" ) );
			}
			break;
		default:
			gameLocal.ServerSendChatMessage( clientNum, "server", va( common->GetLanguageDict()->GetString( "#str_04297" ), ( int )voteIndex ) );
			common->DPrintf( "client %d: unknown vote index %d\n", clientNum, voteIndex );
	}
}

/*
================
idMultiplayerGame::DisconnectClient
================
*/
void idMultiplayerGame::DisconnectClient( int clientNum )
{
	if( lastWinner == clientNum )
	{
		lastWinner = -1;
	}
	UpdatePlayerRanks();
	CheckAbortGame();
}

/*
================
idMultiplayerGame::CheckAbortGame
================
*/
void idMultiplayerGame::CheckAbortGame()
{
	int i;
	if( gameLocal.gameType == GAME_TOURNEY && gameState == WARMUP )
	{
		// if a tourney player joined spectators, let someone else have his spot
		for( i = 0; i < 2; i++ )
		{
			if( !gameLocal.entities[ currentTourneyPlayer[ i ] ] || static_cast< idPlayer* >( gameLocal.entities[ currentTourneyPlayer[ i ] ] )->spectating )
			{
				currentTourneyPlayer[ i ] = -1;
			}
		}
	}
	// only checks for aborts -> game review below
	if( gameState != COUNTDOWN && gameState != GAMEON && gameState != SUDDENDEATH )
	{
		return;
	}
	switch( gameLocal.gameType )
	{
		case GAME_TOURNEY:
			for( i = 0; i < 2; i++ )
			{
				if( !gameLocal.entities[ currentTourneyPlayer[ i ] ] || static_cast< idPlayer* >( gameLocal.entities[ currentTourneyPlayer[ i ] ] )->spectating )
				{
					NewState( GAMEREVIEW );
					return;
				}
			}
			break;
		default:
			if( !EnoughClientsToPlay() )
			{
				NewState( GAMEREVIEW );
			}
			break;
	}
}

/*
================
idMultiplayerGame::WantKilled
================
*/
void idMultiplayerGame::WantKilled( int clientNum )
{
	idEntity* ent = gameLocal.entities[ clientNum ];
	if( ent && ent->IsType( idPlayer::Type ) )
	{
		static_cast<idPlayer*>( ent )->Kill( false, false );
	}
}

/*
================
idMultiplayerGame::MapRestart
================
*/
void idMultiplayerGame::MapRestart()
{
	int clientNum;

	assert( !gameLocal.isClient );
	if( gameState != WARMUP )
	{
		NewState( WARMUP );
		nextState = INACTIVE;
		nextStateSwitch = 0;
	}
	if( g_balanceTDM.GetBool() && lastGameType != GAME_TDM && gameLocal.gameType == GAME_TDM )
	{
		for( clientNum = 0; clientNum < gameLocal.numClients; clientNum++ )
		{
			if( gameLocal.entities[ clientNum ] && gameLocal.entities[ clientNum ]->IsType( idPlayer::Type ) )
			{
				if( static_cast< idPlayer* >( gameLocal.entities[ clientNum ] )->BalanceTDM() )
				{
					// core is in charge of syncing down userinfo changes
					// it will also call back game through SetUserInfo with the current info for update
					cmdSystem->BufferCommandText( CMD_EXEC_NOW, va( "updateUI %d\n", clientNum ) );
				}
			}
		}
	}
	lastGameType = gameLocal.gameType;
}

/*
================
idMultiplayerGame::SwitchToTeam
================
*/
void idMultiplayerGame::SwitchToTeam( int clientNum, int oldteam, int newteam )
{
	idEntity* ent;
	int i;

	assert( gameLocal.gameType == GAME_TDM );
	assert( oldteam != newteam );
	assert( !gameLocal.isClient );

	if( !gameLocal.isClient && newteam >= 0 && IsInGame( clientNum ) )
	{
		PrintMessageEvent( -1, MSG_JOINTEAM, clientNum, newteam );
	}
	// assign the right teamFragCount
	for( i = 0; i < gameLocal.numClients; i++ )
	{
		if( i == clientNum )
		{
			continue;
		}
		ent = gameLocal.entities[ i ];
		if( ent && ent->IsType( idPlayer::Type ) && static_cast< idPlayer* >( ent )->team == newteam )
		{
			playerState[ clientNum ].teamFragCount = playerState[ i ].teamFragCount;
			break;
		}
	}
	if( i == gameLocal.numClients )
	{
		// alone on this team
		playerState[ clientNum ].teamFragCount = 0;
	}
	if( gameState == GAMEON && oldteam != -1 )
	{
		// when changing teams during game, kill and respawn
		idPlayer* p = static_cast<idPlayer*>( gameLocal.entities[ clientNum ] );
		if( p->IsInTeleport() )
		{
			p->ServerSendEvent( idPlayer::EVENT_ABORT_TELEPORTER, NULL, false, -1 );
			p->SetPrivateCameraView( NULL );
		}
		p->Kill( true, true );
		CheckAbortGame();
	}
}

/*
================
idMultiplayerGame::ProcessChatMessage
================
*/
void idMultiplayerGame::ProcessChatMessage( int clientNum, bool team, const char* name, const char* text, const char* sound )
{
	idBitMsg	outMsg;
	byte		msgBuf[ 256 ];
	const char* prefix = NULL;
	int			send_to; // 0 - all, 1 - specs, 2 - team
	int			i;
	idEntity*	 ent;
	idPlayer*	p;
	idStr		prefixed_name;

	assert( !gameLocal.isClient );

	if( clientNum >= 0 )
	{
		p = static_cast< idPlayer* >( gameLocal.entities[ clientNum ] );
		if( !( p && p->IsType( idPlayer::Type ) ) )
		{
			return;
		}

		if( p->spectating )
		{
			prefix = "spectating";
			if( team || ( !g_spectatorChat.GetBool() && ( gameState == GAMEON || gameState == SUDDENDEATH ) ) )
			{
				// to specs
				send_to = 1;
			}
			else
			{
				// to all
				send_to = 0;
			}
		}
		else if( team )
		{
			prefix = "team";
			// to team
			send_to = 2;
		}
		else
		{
			// to all
			send_to = 0;
		}
	}
	else
	{
		p = NULL;
		send_to = 0;
	}
	// put the message together
	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_CHAT );
	if( prefix )
	{
		prefixed_name = va( "(%s) %s", prefix, name );
	}
	else
	{
		prefixed_name = name;
	}
	outMsg.WriteString( prefixed_name );
	outMsg.WriteString( text, -1, false );
	if( !send_to )
	{
		AddChatLine( "%s^0: %s\n", prefixed_name.c_str(), text );
		networkSystem->ServerSendReliableMessage( -1, outMsg );
		if( sound )
		{
			PlayGlobalSound( -1, SND_COUNT, sound );
		}
	}
	else
	{
		for( i = 0; i < gameLocal.numClients; i++ )
		{
			ent = gameLocal.entities[ i ];
			if( !ent || !ent->IsType( idPlayer::Type ) )
			{
				continue;
			}
			if( send_to == 1 && static_cast< idPlayer* >( ent )->spectating )
			{
				if( sound )
				{
					PlayGlobalSound( i, SND_COUNT, sound );
				}
				if( i == gameLocal.localClientNum )
				{
					AddChatLine( "%s^0: %s\n", prefixed_name.c_str(), text );
				}
				else
				{
					networkSystem->ServerSendReliableMessage( i, outMsg );
				}
			}
			else if( send_to == 2 && static_cast< idPlayer* >( ent )->team == p->team )
			{
				if( sound )
				{
					PlayGlobalSound( i, SND_COUNT, sound );
				}
				if( i == gameLocal.localClientNum )
				{
					AddChatLine( "%s^0: %s\n", prefixed_name.c_str(), text );
				}
				else
				{
					networkSystem->ServerSendReliableMessage( i, outMsg );
				}
			}
		}
	}
}

/*
================
idMultiplayerGame::Precache
================
*/
void idMultiplayerGame::Precache()
{
	int			i;
	idFile*		f;

	if( !gameLocal.isMultiplayer )
	{
		return;
	}
	gameLocal.FindEntityDefDict( "player_doommarine", false );;

	// skins
	idStr str = cvarSystem->GetCVarString( "mod_validSkins" );
	idStr skin;
	while( str.Length() )
	{
		int n = str.Find( ";" );
		if( n >= 0 )
		{
			skin = str.Left( n );
			str = str.Right( str.Length() - n - 1 );
		}
		else
		{
			skin = str;
			str = "";
		}
		declManager->FindSkin( skin, false );
	}

	for( i = 0; ui_skinArgs[ i ]; i++ )
	{
		declManager->FindSkin( ui_skinArgs[ i ], false );
	}
	// MP game sounds
	for( i = 0; i < SND_COUNT; i++ )
	{
		f = fileSystem->OpenFileRead( GlobalSoundStrings[ i ] );
		fileSystem->CloseFile( f );
	}
	// MP guis. just make sure we hit all of them
	i = 0;
	while( MPGuis[ i ] )
	{
		uiManager->FindGui( MPGuis[ i ], true );
		i++;
	}
}

/*
================
idMultiplayerGame::ToggleSpectate
================
*/
void idMultiplayerGame::ToggleSpectate()
{
	bool spectating;
	assert( gameLocal.isClient || gameLocal.localClientNum == 0 );

	spectating = ( idStr::Icmp( cvarSystem->GetCVarString( "ui_spectate" ), "Spectate" ) == 0 );
	if( spectating )
	{
		// always allow toggling to play
		cvarSystem->SetCVarString( "ui_spectate", "Play" );
	}
	else
	{
		// only allow toggling to spectate if spectators are enabled.
		if( gameLocal.serverInfo.GetBool( "si_spectators" ) )
		{
			cvarSystem->SetCVarString( "ui_spectate", "Spectate" );
		}
		else
		{
			gameLocal.mpGame.AddChatLine( common->GetLanguageDict()->GetString( "#str_06747" ) );
		}
	}
}

/*
================
idMultiplayerGame::ToggleReady
================
*/
void idMultiplayerGame::ToggleReady()
{
	bool ready;
	assert( gameLocal.isClient || gameLocal.localClientNum == 0 );

	ready = ( idStr::Icmp( cvarSystem->GetCVarString( "ui_ready" ), "Ready" ) == 0 );
	if( ready )
	{
		cvarSystem->SetCVarString( "ui_ready", "Not Ready" );
	}
	else
	{
		cvarSystem->SetCVarString( "ui_ready", "Ready" );
	}
}

/*
================
idMultiplayerGame::ToggleTeam
================
*/
void idMultiplayerGame::ToggleTeam()
{
	bool team;
	assert( gameLocal.isClient || gameLocal.localClientNum == 0 );

	team = ( idStr::Icmp( cvarSystem->GetCVarString( "ui_team" ), "Red" ) == 0 );
	if( team )
	{
		cvarSystem->SetCVarString( "ui_team", "Blue" );
	}
	else
	{
		cvarSystem->SetCVarString( "ui_team", "Red" );
	}
}

/*
================
idMultiplayerGame::ToggleUserInfo
================
*/
void idMultiplayerGame::ThrottleUserInfo()
{
	int i;

	assert( gameLocal.localClientNum >= 0 );

	i = 0;
	while( ThrottleVars[ i ] )
	{
		if( idStr::Icmp( gameLocal.userInfo[ gameLocal.localClientNum ].GetString( ThrottleVars[ i ] ),
						 cvarSystem->GetCVarString( ThrottleVars[ i ] ) ) )
		{
			if( gameLocal.realClientTime < switchThrottle[ i ] )
			{
				AddChatLine( common->GetLanguageDict()->GetString( "#str_04299" ), common->GetLanguageDict()->GetString( ThrottleVarsInEnglish[ i ] ), ( switchThrottle[ i ] - gameLocal.time ) / 1000 + 1 );
				cvarSystem->SetCVarString( ThrottleVars[ i ], gameLocal.userInfo[ gameLocal.localClientNum ].GetString( ThrottleVars[ i ] ) );
			}
			else
			{
				switchThrottle[ i ] = gameLocal.time + ThrottleDelay[ i ] * 1000;
			}
		}
		i++;
	}
}

/*
================
idMultiplayerGame::CanPlay
================
*/
bool idMultiplayerGame::CanPlay( idPlayer* p )
{
	return !p->wantSpectate && playerState[ p->entityNumber ].ingame;
}

/*
================
idMultiplayerGame::EnterGame
================
*/
void idMultiplayerGame::EnterGame( int clientNum )
{
	assert( !gameLocal.isClient );

	if( !playerState[ clientNum ].ingame )
	{
		playerState[ clientNum ].ingame = true;
		if( gameLocal.isMultiplayer )
		{
			// can't use PrintMessageEvent as clients don't know the nickname yet
			gameLocal.ServerSendChatMessage( -1, common->GetLanguageDict()->GetString( "#str_02047" ), va( common->GetLanguageDict()->GetString( "#str_07177" ), gameLocal.userInfo[ clientNum ].GetString( "ui_name" ) ) );
		}
	}
}

/*
================
idMultiplayerGame::WantRespawn
================
*/
bool idMultiplayerGame::WantRespawn( idPlayer* p )
{
	return p->forceRespawn && !p->wantSpectate && playerState[ p->entityNumber ].ingame;
}

/*
================
idMultiplayerGame::VoiceChat
================
*/
void idMultiplayerGame::VoiceChat_f( const idCmdArgs& args )
{
	gameLocal.mpGame.VoiceChat( args, false );
}

/*
================
idMultiplayerGame::VoiceChatTeam
================
*/
void idMultiplayerGame::VoiceChatTeam_f( const idCmdArgs& args )
{
	gameLocal.mpGame.VoiceChat( args, true );
}

/*
================
idMultiplayerGame::VoiceChat
================
*/
void idMultiplayerGame::VoiceChat( const idCmdArgs& args, bool team )
{
	idBitMsg			outMsg;
	byte				msgBuf[128];
	const char*			voc;
	const idDict*		spawnArgs;
	const idKeyValue*	keyval;
	int					index;

	if( !gameLocal.isMultiplayer )
	{
		common->Printf( "clientVoiceChat: only valid in multiplayer\n" );
		return;
	}
	if( args.Argc() != 2 )
	{
		common->Printf( "clientVoiceChat: bad args\n" );
		return;
	}
	// throttle
	if( gameLocal.realClientTime < voiceChatThrottle )
	{
		return;
	}

	voc = args.Argv( 1 );
	spawnArgs = gameLocal.FindEntityDefDict( "player_doommarine", false );
	keyval = spawnArgs->MatchPrefix( "snd_voc_", NULL );
	index = 0;
	while( keyval )
	{
		if( !keyval->GetValue().Icmp( voc ) )
		{
			break;
		}
		keyval = spawnArgs->MatchPrefix( "snd_voc_", keyval );
		index++;
	}
	if( !keyval )
	{
		common->Printf( "Voice command not found: %s\n", voc );
		return;
	}
	voiceChatThrottle = gameLocal.realClientTime + 1000;

	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_VCHAT );
	outMsg.WriteLong( index );
	outMsg.WriteBits( team ? 1 : 0, 1 );
	networkSystem->ClientSendReliableMessage( outMsg );
}

/*
================
idMultiplayerGame::ProcessVoiceChat
================
*/
void idMultiplayerGame::ProcessVoiceChat( int clientNum, bool team, int index )
{
	const idDict*		spawnArgs;
	const idKeyValue*	keyval;
	idStr				name;
	idStr				snd_key;
	idStr				text_key;
	idPlayer*			p;

	p = static_cast< idPlayer* >( gameLocal.entities[ clientNum ] );
	if( !( p && p->IsType( idPlayer::Type ) ) )
	{
		return;
	}

	if( p->spectating )
	{
		return;
	}

	// lookup the sound def
	spawnArgs = gameLocal.FindEntityDefDict( "player_doommarine", false );
	keyval = spawnArgs->MatchPrefix( "snd_voc_", NULL );
	while( index > 0 && keyval )
	{
		keyval = spawnArgs->MatchPrefix( "snd_voc_", keyval );
		index--;
	}
	if( !keyval )
	{
		common->DPrintf( "ProcessVoiceChat: unknown chat index %d\n", index );
		return;
	}
	snd_key = keyval->GetKey();
	name = gameLocal.userInfo[ clientNum ].GetString( "ui_name" );
	sprintf( text_key, "txt_%s", snd_key.Right( snd_key.Length() - 4 ).c_str() );
	if( team || gameState == COUNTDOWN || gameState == GAMEREVIEW )
	{
		ProcessChatMessage( clientNum, team, name, spawnArgs->GetString( text_key ), spawnArgs->GetString( snd_key ) );
	}
	else
	{
		p->StartSound( snd_key, SND_CHANNEL_ANY, 0, true, NULL );
		ProcessChatMessage( clientNum, team, name, spawnArgs->GetString( text_key ), NULL );
	}
}

/*
================
idMultiplayerGame::ServerWriteInitialReliableMessages
================
*/
void idMultiplayerGame::ServerWriteInitialReliableMessages( int clientNum )
{
	idBitMsg	outMsg;
	byte		msgBuf[ MAX_GAME_MESSAGE_SIZE ];
	int			i;
	idEntity*	ent;

	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.BeginWriting();
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_STARTSTATE );
	// send the game state and start time
	outMsg.WriteByte( gameState );
	outMsg.WriteLong( matchStartedTime );
	outMsg.WriteShort( startFragLimit );
	// send the powerup states and the spectate states
	for( i = 0; i < gameLocal.numClients; i++ )
	{
		ent = gameLocal.entities[ i ];
		if( i != clientNum && ent && ent->IsType( idPlayer::Type ) )
		{
			outMsg.WriteShort( i );
			outMsg.WriteShort( static_cast< idPlayer* >( ent )->inventory.powerups );
			outMsg.WriteBits( static_cast< idPlayer* >( ent )->spectating, 1 );
		}
	}
	outMsg.WriteShort( MAX_CLIENTS );
	networkSystem->ServerSendReliableMessage( clientNum, outMsg );

	// we send SI in connectResponse messages, but it may have been modified already
	outMsg.BeginWriting();
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_SERVERINFO );
	outMsg.WriteDeltaDict( gameLocal.serverInfo, NULL );
	networkSystem->ServerSendReliableMessage( clientNum, outMsg );

	// warmup time
	if( gameState == COUNTDOWN )
	{
		outMsg.BeginWriting();
		outMsg.WriteByte( GAME_RELIABLE_MESSAGE_WARMUPTIME );
		outMsg.WriteLong( warmupEndTime );
		networkSystem->ServerSendReliableMessage( clientNum, outMsg );
	}
}

/*
================
idMultiplayerGame::ClientReadStartState
================
*/
void idMultiplayerGame::ClientReadStartState( const idBitMsg& msg )
{
	int i, client, powerup;

	// read the state in preparation for reading snapshot updates
	gameState = ( idMultiplayerGame::gameState_t )msg.ReadByte();
	matchStartedTime = msg.ReadLong();
	startFragLimit = msg.ReadShort();
	while( ( client = msg.ReadShort() ) != MAX_CLIENTS )
	{
		assert( gameLocal.entities[ client ] && gameLocal.entities[ client ]->IsType( idPlayer::Type ) );
		powerup = msg.ReadShort();
		for( i = 0; i < MAX_POWERUPS; i++ )
		{
			if( powerup & ( 1 << i ) )
			{
				static_cast< idPlayer* >( gameLocal.entities[ client ] )->GivePowerUp( i, 0 );
			}
		}
		bool spectate = ( msg.ReadBits( 1 ) != 0 );
		static_cast< idPlayer* >( gameLocal.entities[ client ] )->Spectate( spectate );
	}
}

/*
================
idMultiplayerGame::ClientReadWarmupTime
================
*/
void idMultiplayerGame::ClientReadWarmupTime( const idBitMsg& msg )
{
	warmupEndTime = msg.ReadLong();
}

