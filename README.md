# RobotBuddy

> #### BETA 
> RobotBuddy is currently in Beta! There are a few features missing, and the web dashboard is not yet available, do stay tuned though!

RobotBuddy is a Twitch bot made to enhance your viewers' League of Legends experience; it was built in modern C++ to prioritse performance and stability

### Usage

RobotBuddy joins your Twitch stream as `TinyRobotBuddy`, it adds commands as:
| Keyword             | Result                                                 |
|---------------------|--------------------------------------------------------|
| !today              | Score for the current session (W/L)                    |
| !elo                | Current elo (e.g. Diamond IV 67 LP)                    |
| !kda                | Accumulated KDA over the current session               |
| !opgg               | Replies a link to your OP.GG profile link              |
| !active             | Information about the game you're currently in         |

It also automatically sends a message when you finish a game with a summary of your game, including but not limited to the champion you played, the outcome of the game (Win/Lose), the LP you gained (or lost...)   

Some example of the formatted outputs can be seen in this screenshot of our future dashboard:  
<figure align="center">
  <img src="https://blanche.dev/img/RobotBuddy/RobotBuddyOutput.png" width="1000" />
</figure>

### Tech Stack
For those who may be interested in how we handle and store data;
- [Boost](https://www.boost.org/) for networking and co-routines
- [Nlohmann's JSON library](https://github.com/nlohmann/json) to parse the League of Legends:tm: API responses
- [PostgreSQL](https://www.postgresql.org/) for both the bot and the backend databases
- [Dear ImGui (specifically the Docking branch)](https://github.com/ocornut/imgui) for the debug UI

### Building RobotBuddy
RobotBuddy doesn't have specific build instructions, it was made in Visual Studio 2026 specifically for the Win32 platform, therefore, depending on your use-case, you might need to re-implement some features  
You will need a .env file with the following fields:  

| Field                 | Content                                                |
|-----------------------|--------------------------------------------------------|
| TWITCH_CLIENT_ID      | Your Twitch Application Client ID                      |
| TWITCH_CLIENT_SECRET  | Your Twitch Application Client Secret                  |
| TWITCH_ACCESS_TOKEN   | Your Twitch Application Token                          |
| TWITCH_REFRESH_TOKEN  | Your Twitch Application Refresh Token                  |
| TWITCH_BOT_NICK       | Your Twitch Application User Nickname                  |
| RIOT_API_KEY          | Your Riot Games API Key                                |
| POSTGRES_DB_PASSWORD  | Your Postgres database password                        |

### Credits
- [Amelia](https://github.com/0x416D656C6961) for the dashboard backend, and help with this project!
