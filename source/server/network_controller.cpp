#include "network_controller.hpp"
#include "../network/net_server.hpp"
#include "../exceptions/wrong_move.hpp"
#include <stdlib.h>
#include <time.h>
std::string gen_random(const int len);
network_controller::network_controller()
{
    this->server = new net_server(42069);
}

network_controller::~network_controller()
{
    delete server;
}

void network_controller::run()
{
    srand(time(NULL));
    // TODO: Initialize the server and start listening for connections (Implementing the listeners)

    this->initializePackageListeners();
    
    server->start();
    server->join_thread();
}

void network_controller::join_game(int player, std::string gameCode)
{
    auto game = game_controller_map.find(gameCode);
    if (game != game_controller_map.end()){
        game->second->join_game(player);
        player_game_controller_map[player] = game->second;
        if(game->second->can_start()){
            game->second->run();
        }
    }else{
        throw std::runtime_error("Game not found");
    }
}

/* generate a longer code than just a simple integer while still having a simple way of identifying the game 
Hash-Function from Thomas Mueller: https://stackoverflow.com/a/12996028/14379859 */
std::string network_controller::create_new_game_id(){
    std::string game_id;
    do {
        game_id = gen_random(4);
    } while(is_game_id_valid(game_id));
    return game_id;
}

bool network_controller::is_game_id_valid(std::string game_id){
    return game_controller_map.find(game_id) != game_controller_map.end();
}

/* https://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c */
std::string gen_random(const int len) {
    
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    
    return tmp_s;
}

std::string network_controller::create_new_game(int playerid)
{
    game_controller *game = new game_controller(this->server);
    std::string gameCode = create_new_game_id();
    game_controller_map[gameCode] = game;
    player_game_controller_map[playerid] = game;
    game->join_game(playerid);
    return gameCode;
}


void network_controller::initializePackageListeners()
{
    server->register_packet_listener<packet_socket_connect>([](int id, packet_socket_connect *packet) {
        std::cout << "Client connected: " << id << std::endl;
    });

    server->register_packet_listener<packet_socket_disconnect>([](int id, packet_socket_disconnect *packet) {
        std::cout << "Client disconnected: " << id << std::endl;
    });

    server->register_packet_listener<packet_login>([this](int id, packet_login *packet) {
        std::cout << "Login from " << id << ": " << packet->name << std::endl;
        names[id] = packet->name;
    });

    server->register_packet_listener<packet_game_request>([this](int id, packet_game_request *packet){
        auto game_code = create_new_game(id);
        std::cout << "Game request from " << id << ": " << game_code << std::endl;
        packet_game_code pgc;
        pgc.code = game_code;
        this->server->send_packet(&pgc, id);
    });
    
    server->register_packet_listener<packet_game_code>([this](int id, packet_game_code *packet){
        std::cout << "Game code from " << id << ": " << packet->code << std::endl;
        join_game(id, packet->code);
    });

    server->register_packet_listener<packet_game_place>([this](int id, packet_game_place *packet){
        std::cout << "Game place from " << id << ": " << packet->to <<  std::endl;
        auto gameController = player_game_controller_map.find(id);
        if(gameController == player_game_controller_map.end()){
            player_game_controller_map.at(id)->show_message("You are not in a game");
            return;
        }
        try{
            gameController->second->place_piece(id, packet->to);
        }
        catch(wrong_move &e){
            std::cout << "Player " << id << " tried an unvaild move" << std::endl;
            player_game_controller_map.at(id)->show_message(e.what());
        }
    });

    server->register_packet_listener<packet_socket_disconnect>([this](int id, packet_socket_disconnect *packet){
        std::cout << "Client disconnected: " << id << std::endl;
        auto gameController = player_game_controller_map.find(id);
        if(gameController != player_game_controller_map.end()){
            gameController->second->leave_game(id);	
        }
    });

}