#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <curl/curl.h>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

void trim_newline(std::string& str) {
    size_t pos = str.find_last_not_of("\r\n");
    if (pos != std::string::npos) {
        str.erase(pos + 1);
    } else {
        str.clear();
    }
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

void worker_thread(std::queue<std::string>& url_queue, const std::string& url_base, std::atomic<int>& found_count, std::vector<std::string>& found_urls, std::mutex& queue_mutex, std::condition_variable& cv, std::mutex& output_mutex) {
    CURL* curl;
    CURLcode res;
    long response_code;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        while (true) {
            std::string url;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [&url_queue]() { return !url_queue.empty(); });

                url = url_queue.front();
                url_queue.pop();
            }

            std::string full_url = url_base + url;

            curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // Set timeout
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); // Set connection timeout

            res = curl_easy_perform(curl);

            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            {
                std::lock_guard<std::mutex> lock(output_mutex);
                if (res == CURLE_OK && response_code != 404 && response_code != 403 && response_code != 401 && response_code != 402) {
                    found_urls.push_back(full_url);
                    found_count.fetch_add(1, std::memory_order_relaxed);
                    std::cout << "\033[92m"; // Green color
                    std::cout << "Found: 「" << full_url << "」\n";
                } else {
                    std::cout << "\033[91m"; // Red color
                    std::cout << "Not found: " << full_url << "\n";
                }
                std::cout << "\033[0m"; // Reset color
            }
        }

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
}

int main() {
    std::cout << "\033[41m";
    std::cout << " _______________________________________________________________________\n";
    std::cout << "|                                                                       |\n";
    std::cout << "|                               Dirb v2.22                               |\n";
    std::cout << "|                              By The Dark Raver                         |\n";
    std::cout << "|_______________________________________________________________________|\n";
    std::cout << "\033[0m\n";
    std::cout << "\n";

    std::string url_base = " "; //Hedef Site  =   Target Site 
    std::cout << "URL_BASE: " << url_base << "\n";

    std::string filepath = " "; //Wordlist Dosya Yolu  =  Word list file path 
    std::cout << "WORDLIST_FILES: " << filepath << "\n";
    std::cout << "\n";

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error opening file: common.txt" << std::endl;
        return 1;
    }

    std::vector<std::string> lst;
    std::string line;

    while (std::getline(file, line)) {
        trim_newline(line);
        lst.push_back(line);
    }
    file.close();

    std::queue<std::string> url_queue;
    for (const auto& url : lst) {
        url_queue.push(url);
    }

    const int num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads(num_threads);
    std::atomic<int> found_count(0);
    std::vector<std::string> found_urls;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::mutex output_mutex;

    for (int i = 0; i < num_threads; ++i) {
        threads[i] = std::thread(worker_thread, std::ref(url_queue), std::ref(url_base), std::ref(found_count), std::ref(found_urls), std::ref(queue_mutex), std::ref(cv), std::ref(output_mutex));
    }

    cv.notify_all();

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "\nScan Finished. Found " << found_count.load(std::memory_order_relaxed) << " results.\n";

    std::cout << "Found URLs:\n";
    std::cout << "\033[92m"; // Green color for found URLs
    for (const auto& url : found_urls) {
        std::cout << "「" << url << "」\n";
    }
    std::cout << "\033[0m"; // Reset color

    return 0;
}
