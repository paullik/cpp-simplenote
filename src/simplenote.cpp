/**
 * @file
 *
 * @author Barbu Paul - Gheorghe
 */
#include "includes/simplenote.hpp"
#include "includes/error.hpp"
#include "includes/note.hpp"
#include "includes/helpers.hpp"
#include "includes/base64.h"

#include <curl/curl.h>

#include <string>
#include <set>
#include <sstream>

using std::string;
using std::set;
using std::stringstream;

/**
 * TODO:
 * add tests
 *
 * add docs
 * 
 * debug and throw errs with network down
 * try-catch the simplenote ctor, SIGSERV in cURL...
 *
 * set cURL via a map or set, or something
 *
 * abstract away the curl_easy_perform parts
 *
 * what if the token becomes invalid (if the app using this lib runs for long hours?)
 */

//TODO: remove these
#include <iostream>
using std::cout;

/**
 * Explicit constructor for Simplenote that authenticates the user upon object
 * creation
 *
 * In this case the use of lazy loading is not a good choice
 * because the variables that would have been on the stack
 * (user's email and password) can be inspected.
 * By authenticating the user right away enables this vulnerability for a short
 * period of time, until the token is received.
 *
 * @throw InitError if the setup of cURL fails
 * @throw FetchError if no data can be fetched from Simplenote
 * @throw AuthenticationError if after connecting to Simplenote, no token is
 * received
 *
 * @param char* email the user's Simplenote email
 * @param char* password the user's password for Simplenote
 */
Simplenote::Simplenote(const string& email, const string& password){
    // this function call may throw InitError, so don't catch it
    // because the user should know if the object initialization failed
    init();

    string request_body = create_request_body(email, password);

    // this function call may throw InitError or FetchError, so don't catch here
    // because the user should know if the object initialization failed
    authenticate(request_body);


    //because the token is set at this point we can modify the urls to contain
    //the token and the email
    query_str += token + "&email=" + email;
}

/**
 * Creates a request body to be used for authentication
 *
 * The request body must compile with the Simplenote requirements, namely to
 * have a form similar to a URL query and to be base64 encoded
 *
 * @param char* email the user's Simplenote email
 * @param char* password the user's password for Simplenote
 *
 * @return string the base64 encoded request body
 */
string Simplenote::create_request_body(const string& email, const string& password){
    const string body = "email=" + email + "&password=" + password;

    return base64_encode(reinterpret_cast<const unsigned char*>(body.c_str()),
                        body.length());
}

/**
 * This function sets the cURL options for the object being created
 * @throw InitError if the setup fails
 */
void Simplenote::init(){
    handle = curl_easy_init();

    if(!handle){
        throw InitError("Could not initialize the cURL handler!");
    }

    bool setup = curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, err_buffer) ||
        #ifdef DEBUG
        curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L) ||
        #endif
        curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L) ||
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, -1) ||
        curl_easy_setopt(handle, CURLOPT_USERAGENT, user_agent.c_str()) ||
        curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 0L) ||
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 60L) ||
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 120L) ||
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L) ||
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);

    /**
     * setup will have a value of 0 if cURL was set up successfully,
     * else it will have a different value, see: man libcurl-errors
     * for a list of error codes and why 0 represents success.
     */
    if(setup){
        throw InitError(err_buffer);
    }
}


/**
 * Tries to get the API token in order to use it for the actual note handling
 * This is in fact the authentication process as seen by the Simplenote API
 *
 * @throw InitError if a cURL setup error occurs
 * @throw FetchError if no data can be fetched from Simplenote
 * @throw AuthenticationError if after connecting to Simplenote, no token is
 * received
 *
 * @param string req_body the request body which consists of the email and
 * password base64 encode3d as returned by Simplenote::create_request_body
 */
void Simplenote::authenticate(const string& req_body){
    /**
     * setup will have a value of 0 if cURL was set up successfully,
     * else it will have a different value, see: man libcurl-errors
     * for a list of error codes and why 0 represents success.
     */

    bool setup = curl_easy_setopt(handle, CURLOPT_URL, login_url.c_str()) ||
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, req_body.c_str()) ||
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_curl_string_data) ||
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &token);

    if(setup){
        throw InitError(err_buffer);
    }

    CURLcode retval = curl_easy_perform(handle);

    if(CURLE_OK != retval){
        throw FetchError(err_buffer);
    }

    if(token.empty()){
        throw AuthenticationError("Could not authenticate, check your \
            credentials and try again later!");
    }
}

/**
 * Setter method for the User-Agent header used by cURL
 *
 * @param string ua the new user agent
 */
void Simplenote::set_user_agent(const string& ua){
    user_agent = ua;

    CURLcode setup = curl_easy_setopt(handle, CURLOPT_USERAGENT, user_agent.c_str());

    if(!setup){
        throw InitError(err_buffer);
    }
}

/**
 * Create a note
 *
 * The actual note creation takes place at Simplenote
 *
 * @throw InitError if a cURL setup error occurs
 * @throw FetchError if no or errorneous data can be fetched from Simplenote
 * @throw CreateError if the note was not created
 * @throw ParseError if the retrieved data from the new note cannot be parsed,
 * see Note::Note(string json_str)
 * 
 * @param Note the note object that will be sent as JSON for creation
 *
 * @return Note the newly created note
 */
Note Simplenote::create_note(const Note& n){
    string json_response;

    string url = data_url + query_str;
    
    bool setup = curl_easy_setopt(handle, CURLOPT_URL, url.c_str()) ||
        curl_easy_setopt(handle, CURLOPT_HTTPPOST, 1) ||
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, n.get_json().c_str()) ||
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_curl_string_data) ||
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &json_response);

    if(setup){
        throw InitError(err_buffer);
    }
    
    CURLcode retval = curl_easy_perform(handle);

    if(CURLE_OK != retval){
        throw FetchError(err_buffer);
    }

    if (json_response.empty()){
        throw CreateError("Check your parameters, no note created!");
    }

    Note new_note(json_response);
    
    return new_note;
}

/**
 * Gets a Note object
 *
 * The default version of the note is the current one, which includes all the
 * details of the note, however if you specifiy a version argument,
 * then you'll get a stripped down Note object (that's how Simplenote works)
 *
 * @param string key the note's key you want to get
 * @param int version the note's version (defaults to the most recent version)
 *
 * @return a Note object, take care that if you specify other version number
 * than 0 then you'll get a stripped down version of a note (which consists only
 * of the contents, the version retrieved and whether the note was deleted
 */
Note Simplenote::get_note(const string& key, unsigned int version){
    // TODO ask about the versiondate key
    stringstream t;
    t<<version;

    string json_response, url = data_url + "/" + key + "/" + t.str() + query_str;

    bool setup = curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L) ||
        curl_easy_setopt(handle, CURLOPT_URL, url.c_str()) ||
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_curl_string_data) ||
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &json_response);

    if(setup){
        throw InitError(err_buffer);
    }

    CURLcode retval = curl_easy_perform(handle);

    if(CURLE_OK != retval){
        throw FetchError(err_buffer);
    }

    if (json_response.empty()){
        throw FetchError("No note retrieved, maybe it doesn't exist or"
            " maybe the selected version is wrong!");
    }

    Note note(json_response);

    return note;
}

/**
 * Updates a note
 *
 * The Note matching the version number will be updated
 *
 * @param Note n the note having its members modified that should update the
 * online note stored at Simplenote
 *
 * @return Note object representing the updated note
 */
Note Simplenote::update(const Note& n){
    string json_response, url = data_url + "/" + n.get_key() + query_str;
    
    bool setup = curl_easy_setopt(handle, CURLOPT_URL, url.c_str()) ||
        curl_easy_setopt(handle, CURLOPT_HTTPPOST, 1) ||
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, n.get_json(true).c_str()) ||
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_curl_string_data) ||
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &json_response);

    if(setup){
        throw InitError(err_buffer);
    }

    CURLcode retval = curl_easy_perform(handle);

    if(CURLE_OK != retval){
        throw FetchError(err_buffer);
    }
    
    if (json_response.empty()){
        throw UpdateError();
    }

    Note note(json_response);

    if(note.content.empty()){
        note.content = n.content;
    }

    return note;
}

void Simplenote::debug(){
}

Simplenote::~Simplenote(){
    curl_easy_cleanup(handle);
}