#ifndef SIMPLENOTE_H_GUARD
#define SIMPLENOTE_H_GUARD

#include "note.hpp"

#include <curl/curl.h>

#include <string>
#include <set>

/**
 * The main Simplenote class
 */
class Simplenote{
    private:
        CURL *handle;
        
        char *err_buffer;

        const std::string login_url = "https://simple-note.appspot.com/api/login";
        
        std::string token, query_str = "?auth=", user_agent = "c++api_for_simplenote",
            data_url = "https://simple-note.appspot.com/api2/data",
            index_url = "https://simple-note.appspot.com/api2/index";
        
        std::string create_request_body(const std::string& email,
                                        const std::string& password);
        
        void init();
        void authenticate(const std::string& req_body);
    public:
        Simplenote(const std::string& email, const std::string& password);
        void debug();
        void set_user_agent(const std::string& ua);
        Note create_note(const Note& n);
        Note get_note(const std::string& key, unsigned int version=0);
        Note update(const Note& n); //TODO think wether to return a Note or to modify the referenced one and to void the function
        void trash(const Note& n); // same
        Note restore(const Note& n); //this is ok because by restoring you don;t have a note initially
        ~Simplenote();
};

#endif