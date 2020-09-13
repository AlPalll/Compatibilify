#Compatibilify

Compatibilify is a personal project I wrote to determine the musical compatibility of different Spotify users.

##Usage

Compatibilify is a command line application that is capable of determining the compatibility of up to ten different Spotify users. Run the program in the command line, and enter the desired number of users to compare, along with the URL of each profile when prompted.

##Functionality

Compatibilify works by sending HTTP requests to the Spotify Web API to receive public profile data for each of the desired Spotify profiles. It downloads and parses this data, storing it in a variety of data structures. This data is then analyzed via a unique algorithm in order to determine the shared compatibility of the different profiles.

##Dependencies

Compatibilify requires libcurl to send and recieve HTTP requests. It also requires nlohmann/json to open and parse JSON files recieved from the Spotify Web API.