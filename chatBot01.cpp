#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <curl/curl.h>
#include <json.hpp>

using json = nlohmann::json;
using namespace std;

// cURL write callback to handle the response
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* out) {
    size_t totalSize = size * nmemb;
    out->append((char*)contents, totalSize);
    return totalSize;
}

// Get current time in Italy. Let's find out what time it is over there.
string getTimeInItaly() {
    string response;
    CURL* curl = curl_easy_init();
    if (curl) {
        string url = "https://worldtimeapi.org/api/timezone/Europe/Rome";  // Fetching the time from Italy
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_CAINFO, "C:/Users/NCG/CLionProjects/chatBot01/cacert.pem");

        CURLcode res = curl_easy_perform(curl);  // Requesting the time
        if (res != CURLE_OK) {
            cerr << "❌ Time fetch error: " << curl_easy_strerror(res) << endl;
            return "Could not fetch time.";
        }

        curl_easy_cleanup(curl);
    }

    try {
        auto jsonResponse = json::parse(response);  // Parsing the response to get the time
        string time = jsonResponse["datetime"];
        return "It's " + time + " in Italy.";
    } catch (const json::exception& e) {
        cerr << "Error parsing time data: " << e.what() << endl;
        return "Failed to parse time data.";
    }
}

// Recursive API call with retry functionality in case something goes wrong
string sendMessageRecursive(const string& msg, const string& apiKey, int retries = 3) {
    if (retries == 0) return "Error: Max retries reached.";

    CURL* curl = curl_easy_init();
    if (!curl) return "Curl failed.";

    string payload = R"({"model":"gpt-3.5-turbo","messages":[{"role":"user","content":")" + msg + R"("}]})";
    string response;
    struct curl_slist* headers = nullptr;

    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");  // Sending the message to the API
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "C:/Users/NCG/CLionProjects/chatBot01/cacert.pem");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res == CURLE_OK && !response.empty()) return response;

    cerr << "Retrying... (" << 4 - retries << "/3)\n";  // Trying again if it fails
    this_thread::sleep_for(chrono::seconds(1));  // Taking a small break before retrying
    return sendMessageRecursive(msg, apiKey, retries - 1);
}

int main() {
    const char* keyEnv = getenv("OPENAI_API_KEY");  // Retrieving the API key
    if (!keyEnv) {
        cerr << "OPENAI_API_KEY not set!\n";  // If the API key is missing, we can't proceed
        return 1;
    }
    string apiKey = keyEnv;

    string userInput;
    string botName = "SparkleBot";  // Assigning a name to the bot
    string userName = "User";  // Default user name
    int count = 0;
    bool girlyMode = true;  // Enable girly mode by default
    vector<string> history;
    vector<double> responseTimes;

    cout << "Welcome to SparkleBot's Chatroom!\n";
    cout << "Type 'exit' to leave, 'show history' to see the chat, or 'toggle girly' to switch mode.\n";

    while (true) {
        cout << "You: ";
        getline(cin, userInput);

        // Input validation
        if (userInput.empty()) {
            cout << "Please type something.\n";
            continue;
        }
        if (userInput.size() > 1000) {
            cout << "Message is too long, try something shorter.\n";
            continue;
        }
        if (userInput == "exit") break;

        // Commands
        if (userInput == "show history") {
            cout << "\nConversation So Far:\n";
            for (const auto& line : history) cout << line << "\n";  // Display chat history
            cout << "--------------------------\n";
            continue;
        }

        if (userInput == "toggle girly") {
            girlyMode = !girlyMode;  // Toggle girly mode
            cout << "Girly mode is now " << (girlyMode ? "ON" : "OFF") << "\n";  // Display the mode status
            continue;
        }

        if (userInput.find("Your name is now") != string::npos) {
            botName = userInput.substr(17);  // Change the bot's name if requested
            cout << "My name is now " << botName << "!\n";
            continue;
        }

        if (userInput.find("my name is") != string::npos) {
            userName = userInput.substr(10);  // Change the user's name if requested
            cout << botName << ": Hello, " << userName << "!\n";
            continue;
        }

        if (userInput.find("time in Italy") != string::npos) {
            cout << botName << ": " << getTimeInItaly() << "\n";  // Get the time in Italy
            continue;
        }

        // Process the message and get a response from the API
        count++;
        history.push_back(userName + ": " + userInput);
        auto start = chrono::high_resolution_clock::now();
        string raw = sendMessageRecursive(userInput, apiKey);
        auto end = chrono::high_resolution_clock::now();
        double timeTaken = chrono::duration<double>(end - start).count();
        responseTimes.push_back(timeTaken);

        try {
            auto j = json::parse(raw);
            if (j.contains("choices") && !j["choices"].empty()) {
                string reply = j["choices"][0]["message"]["content"];
                if (girlyMode) reply = " " + reply + " ";  // Add some extra flair if girly mode is enabled

                cout << botName << ": " << reply << "\n";
                history.push_back(botName + ": " + reply);

                cout << "Response time: " << timeTaken << " sec\n";  // Display how long it took to respond
                double avg = 0;
                for (auto t : responseTimes) avg += t;  // Calculate average response time
                avg /= responseTimes.size();
                cout << "Average time: " << avg << " sec\n";  // Show average response time
            } else {
                cout << botName << ": Hmm? Something went wrong.\n";  // If there's no valid response
            }
        } catch (const json::exception& e) {
            cerr << botName << ": Parsing failed - " << e.what() << "\n";  // Error parsing the response
        }

        cout << "------------------------\n";
    }

    cout << "Bye bye! You chatted " << count << " times.\n";  // End the chat
    return 0;
}
