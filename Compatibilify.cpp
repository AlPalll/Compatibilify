#define NOMINMAX
#define CURL_STATICLIB
#include <stdio.h>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <fstream>
#include <queue>
#include <curl\curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Compatibilify {
public:
	void getProfiles();
	void getUserPlaylists();
	void getPlaylistTracks();
	void getArtistData();
	void calculateCompatibility();
private:
	void getUserIDs(std::vector<std::string>& profileURLs);
	void obtainAuthorization();
	void parseAuthorization();
	void parseUserPlaylists();
	void parsePlaylistTracks();
	void parseArtistData();
	std::pair<int, int> calculateLeastGenreOccurrences();
	int calculateSharedGenreCount(int identifier, int cap);

	int numberOfProfiles;
	std::pair<std::string, std::string> accessToken;
	std::vector<std::string> userIDs;
	std::vector<std::vector<std::string>> userPlaylistLinks;
	std::vector<std::unordered_map<std::string, int>> userArtists;
	std::vector<std::unordered_map<std::string, int>> userGenres;
};

int main() {
	curl_global_init(CURL_GLOBAL_ALL);

	Compatibilify driver;

	driver.getProfiles();
	driver.getUserPlaylists();
	driver.getPlaylistTracks();
	driver.getArtistData();
	driver.calculateCompatibility();

	curl_global_cleanup();

	return 0;
}

                                                                                |
//Determines the average occurrence of the genres (which occur in the playlist 
//with the least genre occurrences) in the other playlists.
int Compatibilify::calculateSharedGenreCount(int identifier, int cap) {
	int sharedGenreCount = 0;
	std::vector<int> indivGenreCount(numberOfProfiles, 0);

	for (auto it : userGenres[identifier]) {
		for (int i = 0; i < numberOfProfiles; ++i) {
			if (i != identifier) {
				indivGenreCount[i] += userGenres[i][it.first];
			}
		}
	}

	if (numberOfProfiles > 1) {
		for (int i = 0; i < numberOfProfiles; ++i) {
			if (indivGenreCount[i] > cap) {
				indivGenreCount[i] = cap;
			}
			sharedGenreCount += indivGenreCount[i];
		}

		sharedGenreCount = sharedGenreCount / (numberOfProfiles - 1);
	}

	return sharedGenreCount;
}

//Utilizes priority_queue to count all occurrences of unique genres in each 
//profile's public playlists and return the count and number of the lowest
//profile.
std::pair<int, int> Compatibilify::calculateLeastGenreOccurrences() {
	std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>,
	 std::greater<std::pair<int, int>>> genreCounts;

	for (int i = 0; i < numberOfProfiles; ++i) {
		std::pair<int, int> count;
		count.first = 0;
		count.second = i;
		for (auto it : userGenres[i]) {
			count.first += it.second;
		}
		genreCounts.push(count);
	}

	return genreCounts.top();
}

//Uses stored genres and occurrences to calculate compatibility
void Compatibilify::calculateCompatibility() {
	std::pair<int, int> leastGenreOccurrences = 
	calculateLeastGenreOccurrences();
	int sharedGenres = calculateSharedGenreCount(leastGenreOccurrences.second,
	 leastGenreOccurrences.first);
	double compatibility = std::min((double(sharedGenres) / 
	double(leastGenreOccurrences.first)) * 100, 100.0);
	std::string escape;

	std::cout << "RESULTS:\n\n";
	std::cout << "Profile " << leastGenreOccurrences.second + 1 <<
	 " has the least occurrences of explicitly defined genres associated with
	  their account\nat " 
		<< leastGenreOccurrences.first <<" genre occurrences.\n\n";
	std::cout << "There were, on average, " << sharedGenres << " occurrences 
	of these genres on each of the remaining profiles.\n\n";
	std::cout << "Calculated Compatibility: " << compatibility << "%\n\n\n";
	std::cout << "Thank you for using Compatibilify! Enter any key to exit.\n";
	std::cin >> escape;
}

//Parses artist data and stores genres
void Compatibilify::parseArtistData() {
	std::string baseJSON;
	std::string alteredJSON;

	for (int i = 0; i < numberOfProfiles; ++i) {
		std::unordered_map<std::string, int> genres;
		int remainder = 0;
		if (userArtists[i].size() % 50 > 0) {
			remainder = 1;
		}
		for (int j = 0; j < (int(userArtists[i].size()) / 50 + remainder); ++j){
			baseJSON = "userArtistData.JSON";
			alteredJSON = baseJSON.insert(4, std::to_string(i));
			alteredJSON = alteredJSON.insert(11, std::to_string(j));

			json artists;
			std::ifstream artistsJSON(alteredJSON);
			artistsJSON >> artists;

			for (int k = 0; k < artists["artists"].size(); ++k) {
				for (int l = 0; l < artists["artists"][k]["genres"].size();
				 ++l) {
					if (genres.count(artists["artists"][k]["genres"]
					[l].get<std::string>())) {
						genres[artists["artists"][k]["genres"]
						[l].get<std::string>()]++;
					}
					else {
						genres[artists["artists"][k]["genres"]
						[l].get<std::string>()] = 1;
					}
				}
			}


		}
		userGenres.push_back(genres);
	}
}

//Uses stored track data to retrieve artist data from the Spotify Web API.
void Compatibilify::getArtistData() {
	CURL* curl = curl_easy_init();
	CURLcode result;
	FILE* fp;
	struct curl_slist* list = NULL;
	std::string authorization = "Authorization: " + accessToken.second + " "
	 + accessToken.first;
	std::string baseJSON;
	std::string alteredJSON;
	std::string baseURL;
	std::string alteredURL;


	std::cout << "Downloading user artist data...\n";
	for (int i = 0; i < numberOfProfiles; ++i) {
		//Chunking GET requests into batches of 50 to satisfy Spotify Web
		//API's artist limit. 
		int remainder = 0;
		if (userArtists[i].size() % 50 != 0) {
			remainder = 1;
		}
		for (int j = 0; j < ((int(userArtists[i].size()) / 50) + remainder);
		 j ++) {
			//Creates query string for GET request.
			std::string queryArtists = "";
			auto it = std::next(userArtists[i].begin(), j * 50);
			auto orig = it;
			if (j * 50 + 50 < userArtists[i].size()) {
				for (it; it != std::next(orig, 50); ++it) {
					queryArtists.append(it->first);
					if (it != std::next(orig, 49)) {
						queryArtists.append(",");
					}		
				}
			}
			else {
				for (it; it != userArtists[i].end(); ++it) {
					queryArtists.append(it->first);
					if (it != std::next(userArtists[i].begin(),
					 userArtists[i].size() - 1)) {
						queryArtists.append(",");
					}
				}
			}

			baseJSON = "userArtistData.JSON";
			alteredJSON = baseJSON.insert(4, std::to_string(i));
			alteredJSON = alteredJSON.insert(11, std::to_string(j));
			baseURL = "https://api.spotify.com/v1/artists?ids=";
			alteredURL = baseURL.append(queryArtists);
			const char* url = alteredURL.c_str();
			const char* outFileName = alteredJSON.c_str();

			fp = fopen(outFileName, "wb");
			list = curl_slist_append(list, authorization.c_str());

			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

			result = curl_easy_perform(curl);
			if (result != CURLE_OK) {
				std::cout << curl_easy_strerror(result);
				exit(1);
			}

			fclose(fp);
		}
		std::cout << "Downloaded profile " << i + 1 << " artist data 
		successfully!\n";
	}
	
	std::cout << "Parsing user artist data...\n";
	std::cout << "\n";
	curl_easy_cleanup(curl);
	parseArtistData();
}

//Parses track list JSON and stores artists.
void Compatibilify::parsePlaylistTracks() {
	std::string baseJSON;
	std::string alteredJSON;

	for (int i = 0; i < numberOfProfiles; ++i) {
		std::unordered_map<std::string, int> artists;
		for (size_t j = 0; j < userPlaylistLinks[i].size(); ++j) {
			baseJSON = "userTracks.JSON";
			alteredJSON = baseJSON.insert(4, std::to_string(i));
			alteredJSON = alteredJSON.insert(11, std::to_string(j));
			
			json userTracks;
			std::ifstream userTracksStream(alteredJSON);
			userTracksStream >> userTracks;
			
			for (int k = 0; k < std::min(userTracks["total"],
			 userTracks["limit"]); ++k) {
				for (int l = 0;
				 l < userTracks["items"][k]["track"]["artists"].size(); ++l) {
					if (!userTracks["items"][k]["track"]["artists"][l]["id"]
					.is_null()) {
						if (artists.count(userTracks["items"][k]["track"]
						["artists"][l]["id"].get<std::string>())) {
							artists[userTracks["items"][k]["track"]
							["artists"][l]["id"].get<std::string>()]++;
						}
						else {
							artists[userTracks["items"][k]["track"]["artists"]
							[l]["id"].get<std::string>()] = 1;
						}
					}
				}	
			}
		}
		userArtists.push_back(artists);
	}
	userPlaylistLinks.clear();
}

//Interacts with Spotify Web API to retrieve tracks from public playlists
void Compatibilify::getPlaylistTracks() {
	CURL* curl = curl_easy_init();
	CURLcode result;
	FILE* fp;
	struct curl_slist* list = NULL;
	std::string authorization = "Authorization: " + accessToken.second + " "
	 + accessToken.first;
	std::string baseJSON;
	std::string alteredJSON;

	std::cout << "Downloading user playlist tracks...\n";
	for (int i = 0; i < numberOfProfiles; ++i) {

		for (size_t j = 0; j < userPlaylistLinks[i].size(); ++j) {
			baseJSON = "userTracks.JSON";
			alteredJSON = baseJSON.insert(4, std::to_string(i));
			alteredJSON = alteredJSON.insert(11, std::to_string(j));
			const char* url = userPlaylistLinks[i][j].c_str();
			const char* outFileName = alteredJSON.c_str();

			fp = fopen(outFileName, "wb");
			list = curl_slist_append(list, authorization.c_str());

			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

			result = curl_easy_perform(curl);
			if (result != CURLE_OK) {
				std::cout << curl_easy_strerror(result);
				exit(1);
			}

			fclose(fp);
		}
		std::cout << "Downloaded profile " << i + 1 << " playlist tracks 
		successfully!\n";
	}

	std::cout << "Parsing user playlist tracks...\n";
	std::cout << "\n";
	curl_easy_cleanup(curl);
	parsePlaylistTracks();
}

//Parses playlist JSON files and stores playlists.
void Compatibilify::parseUserPlaylists() {
	std::string baseUserPlaylistsFile;
	std::string userPlaylistsFile;

	for (int i = 0; i < numberOfProfiles; ++i) {
		std::vector<std::string> playlists;
		json userPlaylists;

		baseUserPlaylistsFile = "userPlaylists.JSON";
		userPlaylistsFile = baseUserPlaylistsFile.insert(4, 
		std::to_string(i));
		std::ifstream userPlayListsStream(userPlaylistsFile);
		userPlayListsStream >> userPlaylists;

		for (int i = 0; i < std::min(userPlaylists["limit"].get<int>(),
		 userPlaylists["total"].get<int>()); ++i) {
			playlists.push_back(userPlaylists["items"][i]["tracks"]["href"]);
		}
		
		userPlaylistLinks.push_back(playlists);
	}
}

//This function uses the userIDs to interact with the Spotify Web API and
// store the account playlists in unique JSON files.
void Compatibilify::getUserPlaylists() {
	 CURL *curl = curl_easy_init();
	 CURLcode result;
	 FILE* fp;
	 struct curl_slist* list = NULL;
	 std::string baseURL;
	 std::string alteredURL;
	 std::string baseJSON;
	 std::string alteredJSON;
	 std::string authorization = "Authorization: " + accessToken.second + 
	  " " + accessToken.first;

	 std::cout << "Downloading user playlists...\n";
	 for (int i = 0; i < numberOfProfiles; ++i) {
		 baseURL = "https://api.spotify.com/v1/users//playlists";
		 alteredURL = baseURL.insert(33, userIDs[i]);
		 baseJSON = "userPlaylists.JSON";
		 alteredJSON = baseJSON.insert(4, std::to_string(i));

		 const char* url = alteredURL.c_str();
		 const char* outFileName = alteredJSON.c_str();

		 fp = fopen(outFileName, "wb");
		 list = curl_slist_append(list, authorization.c_str());

		 curl_easy_setopt(curl, CURLOPT_URL, url);
		 curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
		 curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
		 curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
		 curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

		 result = curl_easy_perform(curl);
		 if (result == CURLE_OK) {
			 std::cout << "Downloaded profile " << i + 1 
			 << " playlists successfully!\n";
		 }
		 else {
			 std::cout << curl_easy_strerror(result);
			 exit(1);
		 }
		 
		 fclose(fp);
	 }
	 std::cout << "Parsing user playlists...\n";
	 std::cout << "\n";
	 userIDs.clear();
	 curl_easy_cleanup(curl);
	 parseUserPlaylists();
}

//Parses the access token JSON
void Compatibilify::parseAuthorization() {
	std::ifstream file("accessToken.JSON");
	json token;

	file >> token;
	accessToken.first = token["access_token"].get<std::string>();
	accessToken.second = token["token_type"].get<std::string>();  
}

//Obtains authorization to interact with the Spotify Web API.
void Compatibilify::obtainAuthorization() {
	CURL* curl = curl_easy_init();
	CURLcode result;
	FILE* fp;
	const char* url = "https://accounts.spotify.com/api/token";
	const char* postData = "grant_type=client_credentials";
	struct curl_slist* list = NULL;

	fp = fopen("accessToken.JSON", "wb");
	list = curl_slist_append(list,
	 "Authorization: Basic MDU3MDllMWJlMGZjNDk2Y2IwMmF
	 mNTUxYmY2MzQ3Nzc6MjZkZDk4NzQ3NjFkNDZmYjg0NmI3OTIzODE4YTMxY2Y=");

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	result = curl_easy_perform(curl);
	if (result == CURLE_OK) {
		std::cout <<
		 "Obtained authorization from Spotify Web API successfully!\n\n";
	}
	else {
		std::cout << curl_easy_strerror(result);
		exit(1);
	}

	fclose(fp);
	curl_easy_cleanup(curl);
	parseAuthorization();
}

//This function parses the account URLs for their user IDs and stores these IDs.
void Compatibilify::getUserIDs(std::vector<std::string>& profileURLs) {
	int IDLength;

	for (int i = 0; i < numberOfProfiles; ++i) {
		IDLength = 0;
		for (size_t j = 30; j < profileURLs[i].size(); ++j) {
			if (profileURLs[i][j] == '?') {
				break;
			}
			++IDLength;
		}
		userIDs.push_back(profileURLs[i].substr(30, IDLength));
	}

	obtainAuthorization();
}


//This function gets the number of profiles to analyze and the profile URLs
// from the user.
void Compatibilify::getProfiles() {
	std::vector<std::string> profileURLs;
	std::string URL;

	std::cout << "Welcome to Compatibilify!\n
	\nPlease enter the number of profiles to compare (1-10): ";
	std::cin >> numberOfProfiles;
	profileURLs.reserve(numberOfProfiles);
	userIDs.reserve(numberOfProfiles);

	for (int i = 1; i <= numberOfProfiles; ++i) {
		std::cout << "Please enter Spotify profile URL " << i << ": ";
		std::cin >> URL;
		profileURLs.push_back(URL);
	}

	std::cout << "\n";
	getUserIDs(profileURLs);
}