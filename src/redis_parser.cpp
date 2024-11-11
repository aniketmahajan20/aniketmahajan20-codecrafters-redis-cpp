#include <sstream>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <unistd.h>

#include "redis_parser.hpp"


std::string RedisParser::parseRESPCommand(const std::string& input, std::mutex& database_mutex) {
    size_t pos = 0;

    // Parse array indicator, e.g., "*2\r\n"
    if (input[pos] != '*') {
        return "-ERR protocol error: expected array";
    }
    pos++;

    // Read number of elements
    size_t elements_end = input.find("\r\n", pos);
    int num_elements = std::stoi(input.substr(pos, elements_end - pos));
    pos = elements_end + 2;

    // Parse command bulk string, e.g., "$4\r\nECHO\r\n"
    std::string command = parseBulkString(input, pos);

    // Convert command to uppercase to make it case-insensitive
    for (auto& c : command) c = std::toupper(c);

    if (command == "ECHO"){
        return parseECHOCommand(input, pos);
    }
    else if (command == "PING"){
        return parsePINGCommand(input, pos);
    }
    else if (command == "SET"){
        return parseSETCommand(input, pos, num_elements, database_mutex);
    }
    else if (command == "GET"){
        return parseGETCommand(input, pos);
    }
    else {
        return "-ERR unknown command '" + command + "'";
    }

    
}
// Parses a GET Command
std::string RedisParser::parseGETCommand(const std::string& input, size_t& pos){
    std::string argument = parseBulkString(input, pos);
    if (database.find(argument) != database.end()){
        DBValue response_struct = database[argument];
        if (response_struct.expiry_time == 0 || get_current_time_milli() < response_struct.expiry_time){
            std::string response = response_struct.value;
            return "$" + std::to_string(response.size()) + "\r\n" + response + "\r\n";
        }
        else{
            database.erase(argument);
            return "$-1\r\n";
        }
    }
    else{
        return "$-1\r\n";
    }
}

//Parses a SET Command
std::string RedisParser::parseSETCommand(const std::string& input, size_t& pos, 
                                        int num_elements, std::mutex& database_mutex){
    std::string argument1 = parseBulkString(input, pos);
    std::string argument2 = parseBulkString(input, pos);
    long expiry_time = 0;
    // TODO: Implement error handling for cases that px was incorrect 
    // or the time value is not provided
    // Currently Assumed that PX and expiry time are valid commands provided
    if (num_elements > 3){
        std::string expiry_command = parseBulkString(input, pos);
        std::string expiry_value = parseBulkString(input, pos);
        auto current_time = get_current_time_milli();
        expiry_time = current_time + std::stoi(expiry_value);
    }
    database_mutex.lock();
    database[argument1] = DBValue(argument2, expiry_time);
    database_mutex.unlock();
    return "+OK\r\n";
}

// Parses an ECHO Command
std::string RedisParser::parseECHOCommand(const std::string& input, size_t& pos){
    // Parse argument bulk string, e.g., "$9\r\nraspberry\r\n"
    std::string argument = parseBulkString(input, pos);

    // Formulate the RESP response for the ECHO command (bulk string format)
    return "$" + std::to_string(argument.size()) + "\r\n" + argument + "\r\n";
}


// Parses a PING Command
std::string RedisParser::parsePINGCommand(const std::string& input, size_t& pos){
    return "+PONG\r\n";
}


// Parses a bulk string in RESP format, e.g., "$9\r\nraspberry\r\n"
std::string RedisParser::parseBulkString(const std::string& input, size_t& pos) {
    if (input[pos] != '$') {
        throw std::runtime_error("protocol error: expected bulk string");
    }
    pos++;

    // Find length of the bulk string
    size_t length_end = input.find("\r\n", pos);
    int length = std::stoi(input.substr(pos, length_end - pos));
    pos = length_end + 2;

    // Extract the bulk string value
    std::string bulk_string = input.substr(pos, length);
    pos += length + 2; // Move position past the bulk string and trailing "\r\n"

    return bulk_string;
}

inline long long RedisParser::get_current_time_milli(){
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return milliseconds;
}
